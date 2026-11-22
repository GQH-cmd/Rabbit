#include "pump.h"

#if PUMP_LED_MIRROR
#include "board_led.h"
#endif

#include "main.h"

#if ((PUMP_ACTIVE_HIGH != 0) && (PUMP_ACTIVE_HIGH != 1))
#error "PUMP_ACTIVE_HIGH must be 0 or 1"
#endif

static GPIO_PinState Pump_ActiveLevel(void)
{
#if PUMP_ACTIVE_HIGH
  return GPIO_PIN_SET;
#else
  return GPIO_PIN_RESET;
#endif
}

static GPIO_PinState Pump_InactiveLevel(void)
{
  return (Pump_ActiveLevel() == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

static void Pump_Apply(bool enabled)
{
  HAL_GPIO_WritePin(PUMP_RELAY_GPIO_Port, PUMP_RELAY_Pin,
                    enabled ? Pump_ActiveLevel() : Pump_InactiveLevel());

#if PUMP_LED_MIRROR
  /* Mirror the level actually observed on PE2, not only the requested state. */
  BoardLed_Set(Pump_GetState());
#endif
}

void Pump_Init(void)
{
#if PUMP_LED_MIRROR
  BoardLed_Init();
#endif
  Pump_Off();
}

void Pump_On(void)
{
  Pump_Apply(true);
}

void Pump_Off(void)
{
  Pump_Apply(false);
}

void Pump_Set(bool enabled)
{
  Pump_Apply(enabled);
}

bool Pump_GetState(void)
{
  return HAL_GPIO_ReadPin(PUMP_RELAY_GPIO_Port, PUMP_RELAY_Pin) == Pump_ActiveLevel();
}
