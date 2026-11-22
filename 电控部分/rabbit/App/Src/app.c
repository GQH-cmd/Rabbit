#include "app.h"

#include "esp32_link.h"
#include "board_led.h"
#include "limit_switches.h"
#include "pump.h"
#include "self_test.h"

#include <string.h>

#define APP_PUMP_TEST_TOGGLE_MS 1000U

#define APP_SIM_STOP_SETTLE_MS        300U
#define APP_SIM_TASK_TIMEOUT_MS      4500U
#define APP_LIMIT_WAIT_TIMEOUT_MS   30000U
#define APP_FEED_PUMP_MS              2000U
#define APP_CLEANER_PUMP_MS           3000U
#define APP_WATER_PUMP_MS             5000U

static AppRuntime s_runtime;

#if APP_ENABLE_SIMULATION
static bool s_sim_fail_next_task;
static bool s_sim_timeout_next_task;
#endif

#if APP_ENABLE_PUMP_LED_TEST
static uint32_t s_pump_test_tick;

static void App_ProcessPumpLedTest(void)
{
  const uint32_t now = HAL_GetTick();
  if ((now - s_pump_test_tick) >= APP_PUMP_TEST_TOGGLE_MS)
  {
    s_pump_test_tick = now;
    Pump_Set(!Pump_GetState());
    s_runtime.pump_toggle_count++;
  }
}
#endif

static void App_EnterState(AppState state)
{
  if (s_runtime.state != state)
  {
    s_runtime.state = state;
    s_runtime.state_entered_ms = HAL_GetTick();
    s_runtime.state_transition_count++;
  }
}

#if APP_ENABLE_SIMULATION
static void App_SetSimulationPumpIndicator(bool enabled)
{
  /* Simulation must never touch PE2. PA1 is the only virtual pump indicator. */
  BoardLed_Set(enabled);
}
#endif

static void App_SetPumpRequest(AppPumpRequest request)
{
  s_runtime.pump_request = request;
#if APP_ENABLE_SIMULATION
  App_SetSimulationPumpIndicator(request != APP_PUMP_NONE);
#else
  (void)request;
#endif
}

static void App_StopMotionRequests(void)
{
  s_runtime.motor1_request = APP_MOTOR_STOPPED;
  s_runtime.motor2_request = APP_MOTOR_STOPPED;
}

static bool App_SelfTestBlocksOperation(void)
{
  return s_runtime.self_test_failed_mask != 0U;
}

static void App_RequestStop(bool faulted, uint32_t fault_code)
{
  Pump_Off();
  App_StopMotionRequests();
  App_SetPumpRequest(APP_PUMP_NONE);
  s_runtime.task_stage = APP_TASK_STAGE_IDLE;
  s_runtime.stage_started_ms = HAL_GetTick();
  s_runtime.active_task = ESP32_LINK_TASK_STOP;
  if (faulted)
  {
    s_runtime.fault_code |= fault_code;
  }
  App_EnterState(APP_STATE_STOPPING);
}

static bool App_ActuatorsStopped(void)
{
  /* Future motor drivers must report their confirmed stopped condition here. */
#if APP_ENABLE_SIMULATION
  if ((HAL_GetTick() - s_runtime.stage_started_ms) < APP_SIM_STOP_SETTLE_MS)
  {
    return false;
  }
#endif
  return !Pump_GetState();
}

#if APP_ENABLE_SIMULATION
static void App_EnterTaskStage(AppTaskStage stage)
{
  s_runtime.task_stage = stage;
  s_runtime.stage_started_ms = HAL_GetTick();
}

static bool App_TaskStageTimedOut(uint32_t timeout_ms)
{
  return (HAL_GetTick() - s_runtime.stage_started_ms) >= timeout_ms;
}

static void App_StartSimulationTask(Esp32LinkTask task)
{
  s_runtime.active_task = task;
  App_StopMotionRequests();
  App_SetPumpRequest(APP_PUMP_NONE);
  s_runtime.motor1_request = (task == ESP32_LINK_TASK_FEED)
                             ? APP_MOTOR_MOVE_LEFT : APP_MOTOR_MOVE_RIGHT;
  App_EnterTaskStage(APP_TASK_STAGE_MOVE_HORIZONTAL);
  App_EnterState((task == ESP32_LINK_TASK_FEED) ? APP_STATE_FEED : APP_STATE_CLEAN);
}

static void App_ProcessSimulationTask(void)
{
  const bool feeding = s_runtime.active_task == ESP32_LINK_TASK_FEED;
  const LimitSwitchId horizontal_target = feeding ? LIMIT_SWITCH_LEFT : LIMIT_SWITCH_RIGHT;
  const LimitSwitchId down_target = feeding ? LIMIT_SWITCH_LEFT_DOWN : LIMIT_SWITCH_RIGHT_DOWN;
  const LimitSwitchId up_target = feeding ? LIMIT_SWITCH_LEFT_UP : LIMIT_SWITCH_RIGHT_UP;

  if (s_sim_fail_next_task)
  {
    s_sim_fail_next_task = false;
    s_runtime.simulation_failures_injected++;
    App_RequestStop(true, APP_FAULT_TASK_FAILURE);
    return;
  }

  if (s_sim_timeout_next_task)
  {
    if (App_TaskStageTimedOut(APP_SIM_TASK_TIMEOUT_MS))
    {
      s_sim_timeout_next_task = false;
      s_runtime.simulation_failures_injected++;
      App_RequestStop(true, APP_FAULT_TASK_TIMEOUT);
    }
    return;
  }

  switch (s_runtime.task_stage)
  {
    case APP_TASK_STAGE_MOVE_HORIZONTAL:
      if (LimitSwitches_IsActive(horizontal_target))
      {
        s_runtime.motor1_request = APP_MOTOR_STOPPED;
        s_runtime.motor2_request = APP_MOTOR_MOVE_DOWN;
        App_EnterTaskStage(APP_TASK_STAGE_MOVE_DOWN);
      }
      else if (App_TaskStageTimedOut(APP_LIMIT_WAIT_TIMEOUT_MS))
      {
        App_RequestStop(true, APP_FAULT_TASK_TIMEOUT);
      }
      break;

    case APP_TASK_STAGE_MOVE_DOWN:
      if (LimitSwitches_IsActive(down_target))
      {
        s_runtime.motor2_request = APP_MOTOR_STOPPED;
        if (feeding)
        {
          App_SetPumpRequest(APP_PUMP_MILK);
          App_EnterTaskStage(APP_TASK_STAGE_MILK_PUMP);
        }
        else
        {
          App_SetPumpRequest(APP_PUMP_CLEANER);
          App_EnterTaskStage(APP_TASK_STAGE_CLEANER_PUMP);
        }
      }
      else if (App_TaskStageTimedOut(APP_LIMIT_WAIT_TIMEOUT_MS))
      {
        App_RequestStop(true, APP_FAULT_TASK_TIMEOUT);
      }
      break;

    case APP_TASK_STAGE_MILK_PUMP:
      if (App_TaskStageTimedOut(APP_FEED_PUMP_MS))
      {
        App_SetPumpRequest(APP_PUMP_NONE);
        s_runtime.motor2_request = APP_MOTOR_MOVE_UP;
        App_EnterTaskStage(APP_TASK_STAGE_MOVE_UP);
      }
      break;

    case APP_TASK_STAGE_CLEANER_PUMP:
      if (App_TaskStageTimedOut(APP_CLEANER_PUMP_MS))
      {
        App_SetPumpRequest(APP_PUMP_WATER);
        App_EnterTaskStage(APP_TASK_STAGE_WATER_PUMP);
      }
      break;

    case APP_TASK_STAGE_WATER_PUMP:
      if (App_TaskStageTimedOut(APP_WATER_PUMP_MS))
      {
        App_SetPumpRequest(APP_PUMP_NONE);
        s_runtime.motor2_request = APP_MOTOR_MOVE_UP;
        App_EnterTaskStage(APP_TASK_STAGE_MOVE_UP);
      }
      break;

    case APP_TASK_STAGE_MOVE_UP:
      if (LimitSwitches_IsActive(up_target))
      {
        App_RequestStop(false, APP_FAULT_NONE);
      }
      else if (App_TaskStageTimedOut(APP_LIMIT_WAIT_TIMEOUT_MS))
      {
        App_RequestStop(true, APP_FAULT_TASK_TIMEOUT);
      }
      break;

    default:
      App_RequestStop(true, APP_FAULT_TASK_TIMEOUT);
      break;
  }
}
#endif

static void App_ProcessState(void)
{
  switch (s_runtime.state)
  {
    case APP_STATE_BOOT_SELF_TEST:
      /* App_Init executes the synchronous baseline test before entering this loop. */
      break;

    case APP_STATE_IDLE:
      break;

    case APP_STATE_FEED:
    case APP_STATE_CLEAN:
#if APP_ENABLE_SIMULATION
      App_ProcessSimulationTask();
#else
      /* Real motor execution is intentionally not enabled until its serial protocol is known. */
      App_RequestStop(true, APP_FAULT_TASK_TIMEOUT);
#endif
      break;

    case APP_STATE_STOPPING:
      if (App_ActuatorsStopped())
      {
        s_runtime.active_task = ESP32_LINK_TASK_NONE;
        s_runtime.motor1_request = APP_MOTOR_STOPPED;
        s_runtime.motor2_request = APP_MOTOR_STOPPED;
        s_runtime.pump_request = APP_PUMP_NONE;
        App_EnterState((s_runtime.fault_code == APP_FAULT_NONE) ? APP_STATE_IDLE : APP_STATE_FAULT);
      }
      break;

    case APP_STATE_FAULT:
    default:
      Pump_Off();
      break;
  }
}

static void App_MakeLinkStatus(Esp32LinkStatus *reply)
{
  reply->link_flags = s_runtime.simulation_enabled ? ESP32_LINK_STATUS_SIMULATION : 0U;
  reply->state = (uint8_t)s_runtime.state;
  reply->active_task = s_runtime.active_task;
  reply->last_task = s_runtime.last_task;
  reply->fault_code = s_runtime.fault_code;
  reply->self_test_passed_mask = s_runtime.self_test_passed_mask;
  reply->self_test_failed_mask = s_runtime.self_test_failed_mask;
  reply->transition_count = s_runtime.state_transition_count;
  /* Retained in protocol v3 for compatibility; the retired CAN link is absent. */
  reply->can_dropped_frames = 0U;
  reply->transport_error = Esp32Comm_GetLastError();
}

static void App_HandleLinkRequest(const Esp32LinkRequest *request,
                                  Esp32LinkStatus *reply)
{
  App_MakeLinkStatus(reply);

  if (request->is_poll)
  {
    reply->result = ESP32_LINK_RESULT_STATUS;
    return;
  }

  s_runtime.last_task = request->task;
  reply->id = request->id;

  switch (request->task)
  {
    case ESP32_LINK_TASK_PING:
      reply->result = ESP32_LINK_RESULT_PONG;
      break;

    case ESP32_LINK_TASK_STOP:
      App_RequestStop(false, APP_FAULT_NONE);
      reply->result = ESP32_LINK_RESULT_ACCEPTED;
      break;

    case ESP32_LINK_TASK_RESET_FAULT:
      if (s_runtime.state != APP_STATE_FAULT)
      {
        reply->result = ESP32_LINK_RESULT_REJECTED;
      }
      else if (s_runtime.self_test_failed_mask != 0U)
      {
        reply->result = ESP32_LINK_RESULT_FAULTED;
      }
      else
      {
        s_runtime.fault_code = APP_FAULT_NONE;
        s_runtime.active_task = ESP32_LINK_TASK_NONE;
        App_EnterState(APP_STATE_IDLE);
        reply->result = ESP32_LINK_RESULT_ACCEPTED;
      }
      break;

    case ESP32_LINK_TASK_ARM:
      reply->result = (s_runtime.state == APP_STATE_IDLE)
                      ? ESP32_LINK_RESULT_ACCEPTED
                      : ESP32_LINK_RESULT_REJECTED;
      break;

    case ESP32_LINK_TASK_FEED:
    case ESP32_LINK_TASK_CLEAN:
#if APP_ENABLE_SIMULATION
      if ((s_runtime.state != APP_STATE_IDLE) || App_SelfTestBlocksOperation())
      {
        reply->result = ESP32_LINK_RESULT_REJECTED;
      }
      else
      {
        App_StartSimulationTask(request->task);
        reply->result = ESP32_LINK_RESULT_ACCEPTED;
      }
#else
      /* Reserved state-machine entries; reject until the serial motor contract is implemented. */
      reply->result = ESP32_LINK_RESULT_UNSUPPORTED;
#endif
      break;

    case ESP32_LINK_TASK_SIM_FAIL_NEXT:
#if APP_ENABLE_SIMULATION
      s_sim_fail_next_task = true;
      reply->result = ESP32_LINK_RESULT_ACCEPTED;
#else
      reply->result = ESP32_LINK_RESULT_UNSUPPORTED;
#endif
      break;

    case ESP32_LINK_TASK_SIM_TIMEOUT_NEXT:
#if APP_ENABLE_SIMULATION
      s_sim_timeout_next_task = true;
      reply->result = ESP32_LINK_RESULT_ACCEPTED;
#else
      reply->result = ESP32_LINK_RESULT_UNSUPPORTED;
#endif
      break;

    case ESP32_LINK_TASK_SIM_CLEAR_FAULT:
#if APP_ENABLE_SIMULATION
      s_sim_fail_next_task = false;
      s_sim_timeout_next_task = false;
      reply->result = ESP32_LINK_RESULT_ACCEPTED;
#else
      reply->result = ESP32_LINK_RESULT_UNSUPPORTED;
#endif
      break;

    case ESP32_LINK_TASK_NONE:
    default:
      reply->result = ESP32_LINK_RESULT_REJECTED;
      break;
  }

  App_MakeLinkStatus(reply);
}

static void App_ProcessEsp32Rx(void)
{
  if (Esp32Comm_RxFrameAvailable())
  {
    uint8_t response[ESP32_COMM_FRAME_SIZE];
    (void)Esp32Link_ProcessFrame(Esp32Comm_GetRxFrame(), response);
    (void)Esp32Comm_SetTxFrame(response, sizeof(response));
    memcpy(s_runtime.last_esp32_frame, Esp32Comm_GetRxFrame(), ESP32_COMM_FRAME_SIZE);
    Esp32Comm_ConsumeRxFrame();
    s_runtime.esp32_rx_count++;
  }
}

void App_Init(void)
{
  memset(&s_runtime, 0, sizeof(s_runtime));
  s_runtime.state = APP_STATE_BOOT_SELF_TEST;
  s_runtime.simulation_enabled = APP_ENABLE_SIMULATION != 0;

  Pump_Init();
  LimitSwitches_Init();
  Esp32Link_Init();
  Esp32Link_SetRequestHandler(App_HandleLinkRequest);

  SelfTest_Run();
  s_runtime.self_test_passed_mask = g_self_test_report.passed_mask;
  s_runtime.self_test_failed_mask = g_self_test_report.failed_mask;

#if APP_ENABLE_PUMP_LED_TEST
  s_pump_test_tick = HAL_GetTick();
#endif

  Esp32Comm_Init();
  if (App_SelfTestBlocksOperation())
  {
    s_runtime.fault_code |= APP_FAULT_SELF_TEST;
  }

  s_runtime.pump_state = Pump_GetState();
  s_runtime.limit_switch_raw_mask = LimitSwitches_GetRawMask();
  s_runtime.limit_switch_mask = LimitSwitches_GetStableMask();
  s_runtime.esp32_state = Esp32Comm_GetState();
  s_runtime.initialized = true;
  App_EnterState((s_runtime.fault_code == APP_FAULT_NONE) ? APP_STATE_IDLE : APP_STATE_FAULT);
}

void App_Run(void)
{
#if APP_ENABLE_PUMP_LED_TEST
  App_ProcessPumpLedTest();
#endif

  App_ProcessEsp32Rx();
  Esp32Comm_Process();
  LimitSwitches_Process(HAL_GetTick());
  App_ProcessState();

  s_runtime.pump_state = Pump_GetState();
  s_runtime.limit_switch_raw_mask = LimitSwitches_GetRawMask();
  s_runtime.limit_switch_mask = LimitSwitches_GetStableMask();
  s_runtime.esp32_state = Esp32Comm_GetState();
  s_runtime.loop_count++;
}

const AppRuntime *App_GetRuntime(void)
{
  return &s_runtime;
}
