#include "board_led.h"

#include "main.h"

#define BOARD_LED_GPIO_PORT GPIOA
#define BOARD_LED_PIN       GPIO_PIN_1

static GPIO_PinState BoardLed_ActiveLevel(void)
{
#if BOARD_LED_ACTIVE_HIGH
  return GPIO_PIN_SET;
#else
  return GPIO_PIN_RESET;
#endif
}

static GPIO_PinState BoardLed_InactiveLevel(void)
{
  return (BoardLed_ActiveLevel() == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void BoardLed_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  HAL_GPIO_WritePin(BOARD_LED_GPIO_PORT, BOARD_LED_PIN, BoardLed_InactiveLevel());

  gpio.Pin = BOARD_LED_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BOARD_LED_GPIO_PORT, &gpio);
}

void BoardLed_On(void)
{
  HAL_GPIO_WritePin(BOARD_LED_GPIO_PORT, BOARD_LED_PIN, BoardLed_ActiveLevel());
}

void BoardLed_Off(void)
{
  HAL_GPIO_WritePin(BOARD_LED_GPIO_PORT, BOARD_LED_PIN, BoardLed_InactiveLevel());
}

void BoardLed_Set(bool enabled)
{
  if (enabled)
  {
    BoardLed_On();
  }
  else
  {
    BoardLed_Off();
  }
}

void BoardLed_Toggle(void)
{
  BoardLed_Set(!BoardLed_GetState());
}

bool BoardLed_GetState(void)
{
  return HAL_GPIO_ReadPin(BOARD_LED_GPIO_PORT, BOARD_LED_PIN) == BoardLed_ActiveLevel();
}
