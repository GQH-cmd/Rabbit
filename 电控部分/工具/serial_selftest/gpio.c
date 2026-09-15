#include "gpio.h"

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOE, TEST_LED_Pin | M0_BK_OUT_Pin | M1_BK_OUT_Pin | MOTOR_EN1_Pin | MOTOR_EN2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, SPI2_CS_Pin | LEDA_Pin | LEDB_Pin | LEDC_Pin | TFT_RES_Pin | SPI1_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SPI3_CS_GPIO_Port, SPI3_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SPI1_DC_GPIO_Port, SPI1_DC_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = TEST_LED_Pin | M0_BK_OUT_Pin | M1_BK_OUT_Pin | MOTOR_EN1_Pin | MOTOR_EN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SPI2_CS_Pin | LEDA_Pin | LEDB_Pin | LEDC_Pin | TFT_RES_Pin | SPI1_CS_Pin;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SPI3_CS_Pin;
  HAL_GPIO_Init(SPI3_CS_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SPI1_DC_Pin;
  HAL_GPIO_Init(SPI1_DC_GPIO_Port, &GPIO_InitStruct);
}
