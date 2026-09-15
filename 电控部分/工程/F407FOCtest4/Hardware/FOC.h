/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  FOC.h
 * Description      :  FOC algorithm base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 24rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __FOC_H
#define __FOC_H

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "usart.h"
#include "stdio.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "AS5600.h"
#include "AS5047.h"
#include "ADC.h"
#include "Controller.h"
#include "Filter.h"
#include "Motor.h"
#include "sin_cos.h"

/* Private Variables ---------------------------------------------------------*/
typedef struct __SVPWMOUTPUT
{
	float32_t 			Tcmp1;
	float32_t 			Tcmp2;
	float32_t 			Tcmp3;
}SVPWM_Outputs;

extern float32_t zero;
extern float32_t M0_velocity;
extern float32_t M1_velocity;

/* Private function prototypes -----------------------------------------------*/
void FOC_Init(void);

void Check_Zero_Angle(void);
void Check_Phase_Sequence(void);
void Check_Dir(void);

float32_t Normalize_Angle(float32_t angle);
float32_t Normalize_DegAngle(float32_t angle);

float32_t M0_GetElectric_Angle(float32_t rawAngle);
float32_t M1_GetElectric_Angle(float32_t angle);
float32_t M1_GetElectric_Angle(float32_t angle);
float32_t M1_GetElectric_Angle(float32_t angle);

void Clarke_Transform(float32_t iu, float32_t iv, float32_t iw, float32_t* i_alpha, float32_t* i_beta);
void Park_Transform(float32_t ialpha, float32_t ibeta, float32_t eAngle, float32_t* id, float32_t* iq);

void FOC_M0_Position_Velocity_Update(void);
void FOC_M1_Position_Velocity_Update(void);

void M0_OpeLoop(float32_t Target);
void M1_OpeLoop(float32_t Target);
void M0_CurLoop(void);
void M1_CurLoop(void);
void M0_VelLoop(void);
void M1_VelLoop(void);
void M0_PosLoop(void);
void M1_PosLoop(void);

SVPWM_Outputs CalculateSVPWM(float32_t Uq, float32_t Ud, float32_t eAngle);
void SetSVPWM(float32_t motorNum, float32_t Uq, float32_t Ud, float32_t eAngle);

void FOC_M0_Velocity_Update(void);
void FOC_M1_Velocity_Update(void);
void FOC_M0_Position_Update(void);
void FOC_M1_Position_Update(void);


#ifdef _cplusplus
}
#endif

#endif 