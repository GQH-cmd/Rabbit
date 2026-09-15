/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  led.h
 * Description      :  led light base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 15rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __LED_H
#define __LED_H 

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "main.h"

/* Define -------------------------------------------------------------------*/

/* Private Variables --------------------------------------------------------*/
typedef struct
{
	GPIO_PinState ledA;
	GPIO_PinState ledB;
	GPIO_PinState ledC;
	uint8_t nextState; // 下一个状态索引
} LEDState;

/* Private function prototypes -----------------------------------------------*/ 
void Update_LED_State(void);

#ifdef _cplusplus
}
#endif

#endif 