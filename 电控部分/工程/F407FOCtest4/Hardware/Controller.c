/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  Controller.c
 * Description      :  Controller algorithm base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         FEB 22rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Controller.h"

/* Define ------------------------------------------------------------*/
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

/* Private Variables ------------------------------------------------------------*/

/* Code ---------------------------------------------------------*/
/********************************************************************************
	PID控制器初始化
*********************************************************************************/
void PID_init(PIDController* pid, float32_t Kp, float32_t Ki, float32_t Kd, float32_t outputMin, float32_t outputMax, float32_t imin, float32_t imax)
{
	pid->Kp = Kp;
	pid->Ki = Ki;
	pid->Kd = Kd;
	pid->errorSum = 0;
	pid->lastError = 0;
	pid->outputMin = outputMin;
	pid->outputMax = outputMax;
	pid->integralMin = imin;
	pid->integralMax = imax;	
	pid->dt = 0.01f;
	pid->inv_dt = 1.0f / pid->dt;
}

/********************************************************************************
	PID计算
*********************************************************************************/
float32_t PID(PIDController* pid, float32_t setpoint, float32_t input) 
{
	// 计算误差
	const float32_t error = setpoint - input;
    // 积分限幅
    pid->errorSum += error * pid->dt;
	pid->errorSum = CLAMP(pid->errorSum, pid->integralMin, pid->integralMax);
	
	// 计算微分项
	float32_t error_delta = (error - pid->lastError) * pid->inv_dt; // 用乘法代替除法

	// PID计算
	float32_t output = pid->Kp * error  + pid->Ki * pid->errorSum  + pid->Kd * error_delta;

	// 输出限幅
//	output = output < pid->outputMin ? pid->outputMin : (output > pid->outputMax ? pid->outputMax : output);
	output = CLAMP(output, pid->outputMin, pid->outputMax);
	// 保存误差历史
	pid->lastError = error;

	return output;
}

/* End of this file */
