#ifndef SERIAL_SELFTEST_MAIN_H
#define SERIAL_SELFTEST_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void Error_Handler(void);
void SystemClock_Config(void);

#define TEST_LED_Pin GPIO_PIN_2
#define TEST_LED_GPIO_Port GPIOE
#define M0_BK_OUT_Pin GPIO_PIN_3
#define M0_BK_OUT_GPIO_Port GPIOE
#define M1_BK_OUT_Pin GPIO_PIN_4
#define M1_BK_OUT_GPIO_Port GPIOE
#define MOTOR_EN1_Pin GPIO_PIN_5
#define MOTOR_EN1_GPIO_Port GPIOE
#define MOTOR_EN2_Pin GPIO_PIN_6
#define MOTOR_EN2_GPIO_Port GPIOE
#define SPI2_CS_Pin GPIO_PIN_10
#define SPI2_CS_GPIO_Port GPIOD
#define LEDA_Pin GPIO_PIN_13
#define LEDA_GPIO_Port GPIOD
#define LEDB_Pin GPIO_PIN_14
#define LEDB_GPIO_Port GPIOD
#define LEDC_Pin GPIO_PIN_15
#define LEDC_GPIO_Port GPIOD
#define SPI3_CS_Pin GPIO_PIN_15
#define SPI3_CS_GPIO_Port GPIOA
#define TFT_RES_Pin GPIO_PIN_3
#define TFT_RES_GPIO_Port GPIOD
#define SPI1_CS_Pin GPIO_PIN_7
#define SPI1_CS_GPIO_Port GPIOD
#define SPI1_DC_Pin GPIO_PIN_4
#define SPI1_DC_GPIO_Port GPIOB

#ifdef __cplusplus
}
#endif

#endif
