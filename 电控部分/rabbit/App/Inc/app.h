#ifndef APP_H
#define APP_H

#include "esp32_comm.h"
#include "limit_switches.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  APP_STATE_BOOT_SELF_TEST = 0U,
  APP_STATE_IDLE,
  APP_STATE_FEED,
  APP_STATE_CLEAN,
  APP_STATE_STOPPING,
  APP_STATE_FAULT
} AppState;

typedef enum
{
  APP_TASK_STAGE_IDLE = 0U,
  APP_TASK_STAGE_MOVE_HORIZONTAL,
  APP_TASK_STAGE_MOVE_DOWN,
  APP_TASK_STAGE_MILK_PUMP,
  APP_TASK_STAGE_CLEANER_PUMP,
  APP_TASK_STAGE_WATER_PUMP,
  APP_TASK_STAGE_MOVE_UP
} AppTaskStage;

typedef enum
{
  APP_MOTOR_STOPPED = 0U,
  APP_MOTOR_MOVE_LEFT,
  APP_MOTOR_MOVE_RIGHT,
  APP_MOTOR_MOVE_UP,
  APP_MOTOR_MOVE_DOWN
} AppMotorRequest;

typedef enum
{
  APP_PUMP_NONE = 0U,
  APP_PUMP_MILK,
  APP_PUMP_CLEANER,
  APP_PUMP_WATER
} AppPumpRequest;

typedef enum
{
  APP_FAULT_NONE = 0U,
  APP_FAULT_SELF_TEST = (1UL << 0),
  APP_FAULT_ACTUATOR_INIT = (1UL << 1),
  APP_FAULT_TASK_FAILURE = (1UL << 2),
  APP_FAULT_SPI_RUNTIME = (1UL << 3),
  APP_FAULT_TASK_TIMEOUT = (1UL << 4)
} AppFault;

typedef struct
{
  bool initialized;
  uint32_t loop_count;
  uint32_t esp32_rx_count;
  uint32_t pump_toggle_count;
  bool pump_state;
  uint8_t limit_switch_raw_mask;
  uint8_t limit_switch_mask;
  AppTaskStage task_stage;
  AppMotorRequest motor1_request;
  AppMotorRequest motor2_request;
  AppPumpRequest pump_request;
  uint8_t last_esp32_frame[ESP32_COMM_FRAME_SIZE];
  Esp32CommState esp32_state;
  AppState state;
  Esp32LinkTask active_task;
  Esp32LinkTask last_task;
  uint32_t fault_code;
  uint32_t state_entered_ms;
  uint32_t state_transition_count;
  uint32_t self_test_passed_mask;
  uint32_t self_test_failed_mask;
  bool simulation_enabled;
  uint32_t stage_started_ms;
  uint32_t simulation_failures_injected;
} AppRuntime;

void App_Init(void);
void App_Run(void);
const AppRuntime *App_GetRuntime(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
