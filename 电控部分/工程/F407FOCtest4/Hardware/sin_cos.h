/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  sin_cos.h
 * Description      :  Trigonometric function table
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 15rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __SIN_COS_H__
#define __SIN_COS_H__

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "arm_math.h"

/* Define -------------------------------------------------------------------*/
#define FAST_MATH_TABLE_SIZE  512

/* Private Variables --------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/ 
float32_t arm_sin_f32_tab(float32_t x);
float32_t arm_cos_f32_tab(float32_t x);


#ifdef _cplusplus
}
#endif

#endif 

