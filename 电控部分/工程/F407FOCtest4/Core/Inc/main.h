/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "arm_math.h"
#include "arm_const_structs.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern TIM_HandleTypeDef htim2;
extern float32_t Temp_Real_value;
extern uint8_t rx_buffer[50];
extern uint8_t rx_len;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
uint8_t Parse_Command(uint8_t* data, uint8_t len);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
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

/* USER CODE BEGIN Private defines */
void DWT_Init(void);
void Start(void);
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

