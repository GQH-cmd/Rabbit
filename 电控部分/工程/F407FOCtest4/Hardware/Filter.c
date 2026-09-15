/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  Filter.c
 * Description      :  Filter algorithm base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         FEB 22rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Filter.h"

/* Define ------------------------------------------------------------*/

/* Private Variables ------------------------------------------------------------*/

/* Code ---------------------------------------------------------*/
/********************************************************************************
	滑动平均滤波器初始化
*********************************************************************************/
void MovingAvg_Init(struct MovingAverageFilter* filter, uint8_t window_size)
{
    filter->window_size = window_size;
    filter->buffer = (float32_t*)malloc(window_size * sizeof(float32_t));
    memset(filter->buffer, 0, window_size * sizeof(float32_t));
    filter->index = 0;
    filter->sum = 0.0f;
}

/********************************************************************************
	计算滑动平均滤波器
*********************************************************************************/
float32_t MovingAvg_Update(struct MovingAverageFilter* filter, float32_t input)
{
    filter->sum -= filter->buffer[filter->index];
    filter->buffer[filter->index] = input;
    filter->sum += input;
    filter->index = (filter->index + 1) % filter->window_size;
    return filter->sum / filter->window_size;
}

/********************************************************************************
	释放空间
*********************************************************************************/
void MovingAvg_Free(struct MovingAverageFilter* filter)
{
    free(filter->buffer);
}

/* End of this file */
