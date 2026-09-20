/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  Motor.c
 * Description      :  Motor algorithm base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 24rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Motor.h"

/* Define ------------------------------------------------------------*/
#define _12Div4800	0.0025f		// 12V电压对arr4800

/* Private Variables ------------------------------------------------------------*/
Motor M0_Motor;
Motor M1_Motor;

CURRENT_s M0_Curs;
CURRENT_s M1_Curs;

VELOCITY_s M0_Vels;
VELOCITY_s M1_Vels;

POSITION_s M0_Poses;
POSITION_s M1_Poses;

MotorControl M0 = {
    .mode = MODE_OPEN,
    .param = { .Ope = 0.0f }	// M0电机的初始化
};

MotorControl M1 = {
    .mode = MODE_OPEN,
    .param = { .Ope = 0.0f }	// M1电机的初始化
};

int sensor0Prescaler = 0;
int sensor1Prescaler = 0;

/* 外部变量声明 */
extern uint8_t M0_EncoderType; // 从main.c导入编码器类型变量
extern uint8_t M1_EncoderType;

/* Code ---------------------------------------------------------*/
/**
 * @brief  电机电源初始化
 * @param  motorPowerMode: 电源模式 1-板上DC-DC供电 2-外部电源供电
 * @retval None
 */
void Motor_Power_Init(uint8_t motorPowerMode)
{
	switch (motorPowerMode)
	{
		case 1:
			HAL_GPIO_WritePin(GPIOE, MOTOR_EN1_Pin, GPIO_PIN_SET);
			HAL_Delay(100);
			break;
		case 2:
			HAL_GPIO_WritePin(GPIOE, MOTOR_EN2_Pin, GPIO_PIN_SET);
			HAL_Delay(100);
			break;
			
	}
}

/**
 * @brief  电机结构体初始化
 * @param  pStru: 电机结构体指针
 * @param  motorNum: 电机编号(0或1)
 * @param  polePairs: 电机极对数
 * @retval None
 */
void Motor_Init(Motor *pStru, int motorNum, int polePairs)
{
	pStru -> motorNum = motorNum;
	pStru -> dir = 0;
	pStru -> polePairs = polePairs;
	pStru -> zeroAngle = 0;
	pStru -> sector = 0;
}

/**
 * @brief  电流环结构体初始化
 * @param  pStru: 电流环结构体指针
 * @retval None
 */
void CurLoopInit(CURRENT_s *pStru)
{
	pStru -> id = 	  0.0f;
	pStru -> idr = 	  0.0f;
	pStru -> iq = 	  0.0f;
	pStru -> iqr = 	  0.0f;
	pStru -> Ud = 	  0.0f;
	pStru -> Uq = 	  0.0f;
	pStru -> iu = 	  0.0f;
	pStru -> iv = 	  0.0f;
	pStru -> iw = 	  0.0f;
	pStru -> ialpha = 0.0f;
	pStru -> ibeta =  0.0f;
}

/**
 * @brief  速度环结构体初始化
 * @param  pStru: 速度环结构体指针
 * @retval None
 */
void VelLoopInit(VELOCITY_s *pStru)
{
	pStru -> velocity = 0.0f;
	pStru -> velocityTar = 0.0f;
	pStru -> velLoopPrescaler = 0;
	pStru -> cnt = 0;
}

/**
 * @brief  位置环结构体初始化
 * @param  pStru: 位置环结构体指针
 * @retval None
 */
void PosLoopInit(POSITION_s *pStru)
{
	pStru -> position = 0.0f;
	pStru -> positionTar = 0.0f;
	pStru -> posLoopPrescaler = 19;
	pStru -> cnt = 19;
}

/**
 * @brief  ADC电流采样校准（在电机停止时调用）
 * @param  None
 * @retval None
 */
void Current_Init(void)
{
	// 采集获取平均值作为零点偏移
	const uint8_t samples = 100;
	adc_offset_1 = 0;
	adc_offset_2 = 0;
	adc_offset_3 = 0;
	adc_offset_4 = 0;
//	adc_offset_5 = 0;
//	adc_offset_6 = 0;
	HAL_ADC_Start(&hadc1);
	HAL_ADC_Start(&hadc2);
	for (int i = 0; i < samples; i++)
	{
		adc_offset_1 += HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
		adc_offset_2 += HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_2);
		adc_offset_3 += HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1);
		adc_offset_4 += HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_2);
//		adc_offset_5 += HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_3);
//		adc_offset_6 += HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_3);
		HAL_Delay(1); // 适当延时确保采样完成
	}
	adc_offset_1 /= samples;
	adc_offset_2 /= samples;
	adc_offset_3 /= samples;
	adc_offset_4 /= samples;
//	adc_offset_5 /= samples;
//	adc_offset_6 /= samples;
}

/**
 * @brief  ADC注入通道转换完成回调函数
 * @param  hadc: ADC句柄
 * @retval None
 */
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
		/*** 读取ADC原始值 ***/	
		uint32_t adc_value_1 = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
		uint32_t adc_value_2 = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_2);
//		uint32_t adc_value_3 = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_3);

		/*** 转换为实际电压（减去偏置） ***/
		float32_t u_1 = V_REF * ((float32_t)(adc_value_1 - adc_offset_1) / ADC_MAX);
		float32_t u_2 = V_REF * ((float32_t)(adc_value_2 - adc_offset_2) / ADC_MAX);
//		float32_t u_3 = V_REF * ((float32_t)(adc_value_3 - adc_offset_5) / ADC_MAX);

		/*** 计算电流（I = V/(R*G)） ***/
		M0_Curs.iw = u_1 / (R_SHUNT * GAIN);
		M0_Curs.iu = u_2 / (R_SHUNT * GAIN);
		M0_Curs.iv =  -(M0_Curs.iu + M0_Curs.iw);

		if (M0_EncoderType == ENCODER_DISABLED)
		{
			M0_Curs.iqr = 0.0f;
			M0_Curs.Ud = 0.0f;
			M0_Curs.Uq = 0.0f;
			SetSVPWM(M0_Motor.motorNum, 0.0f, 0.0f, 0.0f);
			return;
		}
		
		/*** 电机编码器读取角度 ***/
		if (M0_EncoderType == 0)
		{ // AS5600
			if (++sensor0Prescaler > Sensor0.sensorPrescaler)
			{
				AS5600_RdRawAngle(&Sensor0, &hi2c1);
				sensor0Prescaler = 0;
			}
		} else
		{ // AS5047
			SPI2_AS5047P_GetAngle();
		}
	  
		/*** 指定运行模式 ***/
		if (M0.mode == 0)
		{
			M0_OpeLoop(M0.param.Ope);
		}
		else if (M0.mode == 1)
		{
			M0_Curs.iqr = M0.param.Cur;
			M0_CurLoop();
		}
		else if (M0.mode == 2)
		{
			M0_Vels.velocityTar = M0.param.Vel;
			M0_VelLoop(); // 2804无刷电机 MAX 1300rmp
		}
		else if (M0.mode == 3)
		{
			M0_Poses.positionTar = M0.param.Pos;
			M0_PosLoop();
		}
	}
  else if (hadc->Instance == ADC2)
  {
		/*** 读取ADC原始值 ***/
		uint32_t adc_value_1 = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1);
		uint32_t adc_value_2 = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_2);
//		uint32_t adc_value_3 = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_3);

		/*** 转换为实际电压（减去偏置） ***/
		float32_t u_1 = V_REF * ((float32_t)(adc_value_1 - adc_offset_3) / ADC_MAX);
		float32_t u_2 = V_REF * ((float32_t)(adc_value_2 - adc_offset_4) / ADC_MAX);
//		float32_t u_3 = V_REF * ((float32_t)(adc_value_3 - adc_offset_6) / ADC_MAX);

		/*** 计算电流（I = V/(R*G)） ***/
		M1_Curs.iw = u_1 / (R_SHUNT * GAIN);
		M1_Curs.iu = u_2 / (R_SHUNT * GAIN);
		M1_Curs.iv =  -(M1_Curs.iu + M1_Curs.iw);

		/* 未接第二个电机/编码器时，禁止进入M1控制和I2C2/SPI3读取，
		 * 否则会把缺失的AS5600当成通信故障反复处理。 */
		if (M1_EncoderType == ENCODER_DISABLED)
		{
			M1_Curs.iqr = 0.0f;
			M1_Curs.Ud = 0.0f;
			M1_Curs.Uq = 0.0f;
			SetSVPWM(M1_Motor.motorNum, 0.0f, 0.0f, 0.0f);
			return;
		}
	  
		/*** 电机编码器读取角度 ***/
		if (M1_EncoderType == 0)
		{ // AS5600
			if (++sensor1Prescaler > Sensor1.sensorPrescaler)
			{
				AS5600_RdRawAngle(&Sensor1, &hi2c2);
				sensor1Prescaler = 0;
			}
		} else
		{ // AS5047
			SPI3_AS5047P_GetAngle();
		}
	  
		/*** 指定运行模式 ***/
		if (M1.mode == 0)
		{
			M1_OpeLoop(M1.param.Ope);
		}
		else if (M1.mode == 1)
		{
			M1_Curs.iqr = M1.param.Cur;
			M1_CurLoop();
		}
		else if (M1.mode == 2)
		{
			M1_Vels.velocityTar = M1.param.Vel;
			M1_VelLoop();
		}
		else if (M1.mode == 3)
		{
			M1_Poses.positionTar = M1.param.Pos;
			M1_PosLoop();
		}
	}
}

/**
 * @brief  获取当前微秒时间戳(使用DWT计数器)
 * @param  None
 * @retval 当前时间戳(微秒)
 */
uint32_t get_micros(void)
{
    return __HAL_TIM_GET_COUNTER(&htim2); // 读取TIM2->CNT寄存器
}

/**
 * @brief  计算时间差(微秒)，处理计数器溢出情况
 * @param  old_stamp: 旧的时间戳
 * @retval 时间差(微秒)
 */
uint32_t delta_time_us(uint32_t old_stamp)
{
    uint32_t new_stamp = __HAL_TIM_GET_COUNTER(&htim2);
    return (new_stamp - old_stamp) & 0xFFFFFFFF; // 利用32位无符号数自动溢出特性
}
