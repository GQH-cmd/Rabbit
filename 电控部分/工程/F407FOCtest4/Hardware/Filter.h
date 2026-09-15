/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  Filter.h
 * Description      :  Filter driver base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         FEB 03rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */
#ifndef __FILTER_H
#define __FILTER_H 

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include <stdlib.h>

/* Define -------------------------------------------------------------------*/

/* Private Variables --------------------------------------------------------*/
struct MovingAverageFilter {
    float32_t* buffer;		// 动态数组指针
    uint8_t window_size; 	// 存储窗口大小
    uint8_t index;
    float32_t sum;
};

/* Private function prototypes -----------------------------------------------*/ 
void MovingAvg_Init(struct MovingAverageFilter* filter, uint8_t window_size);
float32_t MovingAvg_Update(struct MovingAverageFilter* filter, float32_t input);
void MovingAvg_Free(struct MovingAverageFilter* filter);

#endif 
