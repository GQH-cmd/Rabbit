/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  Motor.h
 * Description      :  Motor algorithm base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 24rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __MOTOR_H
#define __MOTOR_H

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "usart.h"
#include "stm32f4xx_it.h"
#include "stdio.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "AS5600.h"
#include "ADC.h"
#include "Controller.h"
#include "Filter.h"
#include "FOC.h"

/* Private Variables ------------------------------------------------------------*/
static float32_t ADC_MAX	  = 4096.0f;		// 12位ADC最大值
static float32_t V_REF 		  =	3.3f;  			// ADC参考电压3.3V
static float32_t R_SHUNT	  = 0.01f; 			// 10mΩ采样电阻
static float32_t GAIN   	  = 50.0f; 			// TP181A1增益50
		
static float32_t adc_offset_1 = 0.0f;  			// U相零点偏移
static float32_t adc_offset_2 = 0.0f;  			// W相零点偏移
static float32_t adc_offset_3 = 0.0f;  			// U相零点偏移
static float32_t adc_offset_4 = 0.0f;  			// W相零点偏移
static float32_t adc_offset_5 = 0.0f;  			// M0零点偏移
static float32_t adc_offset_6 = 0.0f;  			// M1零点偏移

// 电机模式
typedef enum {
    MODE_OPEN =     0,  // 开环模式
    MODE_CURRENT =  1,  // 电流模式
    MODE_VELOCITY = 2,  // 速度模式
    MODE_POSITION = 3   // 位置模式
} MotorMode;

// 电机控制参数
typedef struct
{
    MotorMode mode;
    union
	{
        float32_t Ope;  // 操作模式参数
        float32_t Cur;  // 电流模式参数
        float32_t Vel;  // 速度模式参数
        float32_t Pos;  // 位置模式参数
    }param;
}MotorControl;

// 电机参数
typedef struct __MOTORSTRUCT
{
	int8_t 				motorNum;		// 目标电机
	int8_t 				dir;			// 电机方向
	int8_t				polePairs;		// 电机极对数
	float32_t			zeroAngle;		// 电机零电角度
	uint8_t				sector;		// 电机零电角度
}Motor;

// 电流环
typedef struct __CURRENTSTRUCT
{
	float32_t id;		//d轴电流反馈
	float32_t idr;		//d轴电流给定
	float32_t iq;		//q轴电流反馈
	float32_t iqr;		//q轴电流给定
	float32_t Ud;		//d轴电压输出
	float32_t Uq;		//q轴电压输出

	float32_t iu;
	float32_t iv;
	float32_t iw;
	
	float32_t ialpha;
	float32_t ibeta;
}CURRENT_s;

// 速度环
typedef struct __VELOCITYSTRUCT
{
	float32_t velocity;				//速度值
	float32_t velocityTar;			//速度目标值
	uint8_t	  velLoopPrescaler;		//速度环分频值
	uint8_t   cnt;					//速度环计数（用于速度环分频，1/10电流环频率）

}VELOCITY_s;

// 位置环
typedef struct __POSITIONSTRUCT
{
	float32_t position;				//位置值（角度制）
	float32_t positionTar;			//位置目标值
	float32_t posLoopPrescaler;		//位置环分频值
	uint8_t   cnt;					//位置环计数（用于位置环分频，1/20电流环频率）

}POSITION_s;

extern CURRENT_s  M0_Curs;
extern CURRENT_s  M1_Curs;
extern VELOCITY_s M0_Vels;
extern VELOCITY_s M1_Vels;
extern POSITION_s M0_Poses;
extern POSITION_s M1_Poses;

extern Motor M0_Motor;
extern Motor M1_Motor;

extern MotorControl M0;
extern MotorControl M1;

void Motor_Power_Init(uint8_t motorPowerMode);
void Motor_Init(Motor *pStru, int motorNum, int polePairs);
void CurLoopInit(CURRENT_s *current);
void VelLoopInit(VELOCITY_s *pStru);
void PosLoopInit(POSITION_s *pStru);
void Current_Init(void);

uint32_t get_micros(void);
uint32_t delta_time_us(uint32_t old_stamp);


#ifdef _cplusplus
}
#endif

#endif 