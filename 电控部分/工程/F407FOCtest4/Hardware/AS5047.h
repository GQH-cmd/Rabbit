/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  AS5047.h
 * Description      :  AS5047 driver base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 24rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __AS5047_H
#define __AS5047_H 

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "usart.h"
#include "i2c.h"
#include "spi.h"
#include <stdlib.h>
#include <stdio.h>
#include "stdbool.h"
#include "Motor.h"
#include "FOC.h"
#include "lcd.h"
#include "gpio.h"
#include "main.h"

/* Define -------------------------------------------------------------------*/
#define SPI2_AS5047P_CS_L       HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_RESET)
#define SPI2_AS5047P_CS_H       HAL_GPIO_WritePin(SPI2_CS_GPIO_Port, SPI2_CS_Pin, GPIO_PIN_SET)

#define SPI3_AS5047P_CS_L       HAL_GPIO_WritePin(SPI3_CS_GPIO_Port, SPI3_CS_Pin, GPIO_PIN_RESET)
#define SPI3_AS5047P_CS_H       HAL_GPIO_WritePin(SPI3_CS_GPIO_Port, SPI3_CS_Pin, GPIO_PIN_SET)

/* Private Variables --------------------------------------------------------*/
typedef struct
{
	int			motorNum;		// 电机编号(0或1)
	uint8_t 	sensorNum;		// 传感器编号
	uint16_t	rawAngle; 		// 原始角度值（0-16383）
	float32_t	angle;    		// 角度值（弧度制 0-2π）
	float32_t	degAngle; 		// 角度值（角度制 0-360°）
	
	uint16_t	status;			// 传感器状态寄存器
	uint16_t	diag;			// 传感器诊断寄存器
	
	// 位置跟踪相关
	float32_t	lastAngle;		// 上一次角度
	int32_t		numOfCircles;	// 旋转圈数
	float32_t	totalAngle;		// 总角度（包含多圈）
	
	// 速度计算相关
	float32_t	velocity;		// 角速度
	uint32_t	lastTs;			// 上次时间戳
	float32_t	lastTotalAngle;	// 上次总角度（用于精确速度计算）
	float32_t	curDegAngle;	// 当前单圈角度（0 - 360）
	
	float32_t	absDegAngle;	// 绝对角度 = curDegAngle + numOfCircles*360
} AS5047;

extern AS5047 Sensor2;	// SPI2接口的传感器
extern AS5047 Sensor3;	// SPI3接口的传感器

/* Private function prototypes -----------------------------------------------*/ 
// 通用函数
uint16_t SPI_ReadWrite_OneByte(SPI_HandleTypeDef *spi_handle, GPIO_TypeDef* cs_port, uint16_t cs_pin, uint16_t _txdata);
uint16_t AS5047P_read(SPI_HandleTypeDef *spi_handle, GPIO_TypeDef* cs_port, uint16_t cs_pin, uint16_t add);
float32_t AS5047P_GetAngle(AS5047 *pStru, SPI_HandleTypeDef *spi_handle, GPIO_TypeDef* cs_port, uint16_t cs_pin);
void SPI_UpdatePosition(AS5047 *pStru, float32_t ZeroEAngle);
float32_t AS5047_GetVelocity(AS5047 *pStru);

// 兼容函数
uint16_t SPI2_ReadWrite_OneByte(uint16_t _txdata);
uint16_t SPI3_ReadWrite_OneByte(uint16_t _txdata);
uint16_t SPI2_AS5047P_read(uint16_t add);
uint16_t SPI3_AS5047P_read(uint16_t add);
float32_t SPI2_AS5047P_GetAngle(void);
float32_t SPI3_AS5047P_GetAngle(void);
void M0_SPI_UpdatePosition(void);
void M1_SPI_UpdatePosition(void);
float32_t AS5047_M0_GetVelocity(void);
float32_t AS5047_M1_GetVelocity(void);
void AS5047P_Init(void);

#ifdef _cplusplus
}
#endif

#endif 