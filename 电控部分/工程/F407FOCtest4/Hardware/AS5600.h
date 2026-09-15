/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  AS5600.h
 * Description      :  AS5600 driver base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 24rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __AS5600_H
#define __AS5600_H 

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "usart.h"
#include "i2c.h"
#include <stdlib.h>
#include <stdio.h>
#include "stdbool.h"
#include "Motor.h"
#include "FOC.h"
#include "lcd.h"

/* Define -------------------------------------------------------------------*/
#define _2PI							2 * PI
 
#define	AS5600_SLAVE_ADDRESS 			0x36 << 1
#define	RAW_ANGLE_H						0x0C
#define	RAW_ANGLE_L						0x0E

/* Private Variables --------------------------------------------------------*/
enum A5600_STATUS
{
	AS5600_OK = 0 ,
	AS5600_ERROR
};
 
typedef struct __AS5600STRUCT
{
    // 基本信息
    int         motorNum;       		// 电机编号(0或1)
    int         sensorPrescaler;		// 编码器读取分频系数
    uint16_t    rawAngle;				// 原始数据：0 - 4095
    float32_t   angle;					// 角度：0 - 2pi
    float32_t   degAngle;				// 角度：0 - 360
    float32_t   eAngle;					// 电角度：0 - 2pi
    float32_t   compensatedAngle;		// 偏移角度
    bool        result;					// false:不使用  true:使用
    
    // 位置跟踪相关
    float32_t   lastAngle;				// 上一个角度
    int16_t     numOfCircle; 			// 圈数 (旧接口，保留兼容)
    int32_t     numOfCircles;			// 累计圈数（正数为顺时针，负数为逆时针）
    float32_t   curDegAngle; 			// 当前单圈角度（0 - 360）
    float32_t   absDegAngle; 			// 绝对角度 = curDegAngle + numOfCircles*360
    float32_t   position;				// 位置
    
    // 速度计算相关
    float32_t   velocity;				// 速度，单位：弧度每秒（rad/s）
    float32_t   velThreshold;			// 噪声容限阈值
    uint32_t    lastTs;					// 上一个时间戳
    float32_t   lastTotalAngle;			// 带方向连续性的总角度缓存
}AS5600;

extern AS5600 Sensor0;
extern AS5600 Sensor1;

/* Private function prototypes -----------------------------------------------*/ 
// 通用函数
void AS5600_Init(void);
void AS5600_RdRawAngle(AS5600 *pStru, I2C_HandleTypeDef *hi2c);
void I2C_Update_Position(AS5600 *pStru, float32_t ZeroEAngle);
float32_t I2C_Get_Velocity(AS5600 *pStru);
uint8_t AS5600_WeReg(I2C_HandleTypeDef *hi2c, uint16_t regAdd, uint8_t *pData, uint16_t Size);
void I2C_Bus_Recovery(I2C_HandleTypeDef *hi2c);
void DWT_Delay_us(uint32_t us);
void Init_Motor_Num(AS5600 *pStru, int motorNum);
uint8_t AS5600_RdReg(I2C_HandleTypeDef *hi2c, uint16_t regAdd, uint8_t *pData, uint16_t Size);
float32_t my_fabsf(float32_t x);

// 兼容函数
float32_t I2C1_AS5600_GetAngle(void);
float32_t I2C2_AS5600_GetAngle(void);
void M0_I2C_UpdatePosition(AS5600 *pStru, float32_t ZeroEAngle);
void M1_I2C_UpdatePosition(AS5600 *pStru, float32_t ZeroEAngle);
float32_t AS5600_M0_GetVelocity(void);
float32_t AS5600_M1_GetVelocity(void);

#ifdef _cplusplus
}
#endif

#endif 