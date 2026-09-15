/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  Controller.h
 * Description      :  Controller algorithm base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         FEB 03rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "arm_math.h"
#include "arm_const_structs.h"


/* Private Variables ------------------------------------------------------------*/
extern struct PID_Controller Iq_PID;
extern struct PID_Controller Id_PID;

typedef struct __PIDSTRUCT{
    float32_t Kp;						// 比例系数
    float32_t Ki; 						// 积分系数
    float32_t Kd;						// 微分系数
    float32_t errorSum;     			// 误差积分
    float32_t lastError;    			// 上次误差
    float32_t outputMin;    			// 输出最小值
    float32_t outputMax;    			// 输出最大值
	float32_t integralMin, integralMax; // 积分限幅参数
	float32_t dt;						// 采样时间
	float32_t inv_dt;					// 采样时间的倒数
}PIDController;

/* Private function prototypes -----------------------------------------------*/
void PID_init(PIDController* pid, float32_t Kp, float32_t Ki, float32_t Kd, float32_t outputMin, float32_t outputMax, float32_t imin, float32_t imax);
float32_t PID(PIDController* pid, float32_t setpoint, float32_t input); 

#ifdef _cplusplus
}
#endif

#endif 