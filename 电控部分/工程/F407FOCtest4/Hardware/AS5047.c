/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  AS5047.c
 * Description      :  AS5047 driver base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 24rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "AS5047.h"
#include "gpio.h"
#include "main.h"

/* Define -------------------------------------------------------------------*/
#define _2PI					2 * PI
#define FILTER_ALPHA            0.2f

// AS5047P 寄存器地址
/* 空操作寄存器，用于启动SPI读取流程或保持SPI通信连续性，读取时无实际意义 */
#define NOP 0x0000
/* 错误标志寄存器，包含奇偶校验错误、无效命令错误等状态位。读取后自动清除错误标志 */
#define ERRFL 0x0001
#define PROG 0x0003
#define DIAAGC 0x3FFC
#define MAG 0x3FFD
/* 未补偿角度寄存器，输出未经动态误差补偿的原始角度值 */
#define ANGLEUNC 0x3FFE
/* 输出动态角度误差补偿后的角度值 */
#define ANGLECOM 0x3FFF

#define ZPOSM 0x0016
#define ZPOSL 0x0017
#define SETTINGS1 0x0018
#define SETTINGS2 0x0019

#define RAW_TO_DEG          (360.0f / 4096.0f)  // 原始数据转角度的系数

/* Private Variables --------------------------------------------------------*/
// 存储传感器数据结构
AS5047 Sensor2 = {0};
AS5047 Sensor3 = {0};

/* Private function prototypes -----------------------------------------------*/ 
static uint16_t Parity_bit_Calculate(uint16_t data_2_cal);
static float32_t as5047_fabsf(float32_t x);

/* Code ---------------------------------------------------------------------*/
/**
 * @brief  计算奇偶校验位
 * @param  data_2_cal: 要计算的数据
 * @retval 校验位值
 */
static uint16_t Parity_bit_Calculate(uint16_t data_2_cal)
{
	uint16_t parity_bit_value = 0;
	while(data_2_cal != 0)
	{
		parity_bit_value ^= data_2_cal; 
		data_2_cal >>= 1;
	}
	return (parity_bit_value & 0x1); 
}

/**
 * @brief  返回绝对值
 * @param  x: 输入值
 * @retval 绝对值
 */
static float32_t as5047_fabsf(float32_t x)
{
    return x < 0 ? -x : x;
}

/**
 * @brief  SPI接口读写一个字节
 * @param  spi_handle: SPI句柄
 * @param  cs_port: CS引脚端口
 * @param  cs_pin: CS引脚
 * @param  _txdata: 要发送的数据
 * @retval 接收到的数据
 */
uint16_t SPI_ReadWrite_OneByte(SPI_HandleTypeDef *spi_handle, GPIO_TypeDef* cs_port, uint16_t cs_pin, uint16_t _txdata)
{
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);  // CS拉低
	uint16_t rxdata;
	if(HAL_SPI_TransmitReceive(spi_handle, (uint8_t *)&_txdata, (uint8_t *)&rxdata, 1, 1000) != HAL_OK)
		rxdata = 0;
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);  	// CS拉高
	return rxdata;
}

/**
 * @brief  使用指定SPI读取AS5047P寄存器
 * @param  spi_handle: SPI句柄
 * @param  cs_port: CS引脚端口
 * @param  cs_pin: CS引脚
 * @param  add: 寄存器地址
 * @retval 读取的数据
 */
uint16_t AS5047P_read(SPI_HandleTypeDef *spi_handle, GPIO_TypeDef* cs_port, uint16_t cs_pin, uint16_t add)
{
	uint16_t data;
	add |= 0x4000;	//读指令 bit14 置1
	if(Parity_bit_Calculate(add) == 1) add = add | 0x8000;						//如果前15位 1的个数为奇数则Bit15 置1
	SPI_ReadWrite_OneByte(spi_handle, cs_port, cs_pin, add);					//发送一个指令能读到的数据
	data = SPI_ReadWrite_OneByte(spi_handle, cs_port, cs_pin, NOP | 0x4000);	//发送一个空指令获取上一个指令的数据
	data &= 0x3fff;
	return data;
}

/**
 * @brief  从AS5047P获取角度值（弧度值）
 * @param  pStru: 传感器结构体指针
 * @param  spi_handle: SPI句柄
 * @param  cs_port: CS引脚端口
 * @param  cs_pin: CS引脚
 * @retval 角度值（弧度制）
 */
float32_t AS5047P_GetAngle(AS5047 *pStru, SPI_HandleTypeDef *spi_handle, GPIO_TypeDef* cs_port, uint16_t cs_pin)
{
	uint16_t angle_data = AS5047P_read(spi_handle, cs_port, cs_pin, ANGLECOM);
	pStru->rawAngle = angle_data;
	pStru->angle = ((float32_t)angle_data) * _2PI / 16384.0f;
	return pStru->angle;
}

/**
 * @brief  使用SPI2从AS5047P获取角度值（弧度值）
 * @param  None
 * @retval 角度值（弧度制）
 */
float32_t SPI2_AS5047P_GetAngle(void)
{
	return AS5047P_GetAngle(&Sensor2, &hspi2, SPI2_CS_GPIO_Port, SPI2_CS_Pin);
}

/**
 * @brief  使用SPI3从AS5047P获取角度值（弧度值）
 * @param  None
 * @retval 角度值（弧度制）
 */
float32_t SPI3_AS5047P_GetAngle(void)
{
	return AS5047P_GetAngle(&Sensor3, &hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin);
}

/**
 * @brief  更新传感器位置
 * @param  pStru: 传感器结构体指针
 * @param  ZeroEAngle: 零点角度
 * @retval None
 */
void SPI_UpdatePosition(AS5047 *pStru, float32_t ZeroEAngle)
{
	static float32_t prevDeg[2] = {0.0f, 0.0f}; // 为两个电机分别保存前一角度
	int motorIdx = pStru->motorNum; // 获取电机索引(0或1)

	// 使用预计算常量，减少浮点运算
	pStru->curDegAngle = Normalize_DegAngle((pStru->rawAngle - ZeroEAngle) * RAW_TO_DEG);
	
	float32_t delta = pStru->curDegAngle - prevDeg[motorIdx];
	// 使用预定义的180.0f常量判断跨圈
	if (as5047_fabsf(delta) > 180.0f)
	{	// 角度跳变超过180°时判定为跨圈
		if (delta > 0)
		{
			pStru->numOfCircles--;  // 顺时针跨圈（如360 - 0）
		} else
		{
			pStru->numOfCircles++;  // 逆时针跨圈（如0 - 360）
		}
	}
	prevDeg[motorIdx] = pStru->curDegAngle;

	pStru->absDegAngle = pStru->curDegAngle + pStru->numOfCircles * 360.0f;
}

void M0_SPI_UpdatePosition(void)
{
	SPI_UpdatePosition(&Sensor2, M0_Motor.zeroAngle);
}

void M1_SPI_UpdatePosition(void)
{
	SPI_UpdatePosition(&Sensor3, M1_Motor.zeroAngle);
}

/**
 * @brief  计算传感器的角速度
 * @param  pStru: 传感器结构体指针
 * @retval 角速度(rad/s)
 */
float32_t AS5047_GetVelocity(AS5047 *pStru)
{
	uint32_t curTick = get_micros();
	
	// 精确时间差计算（考虑32位计数器溢出）
	uint32_t deltaT = delta_time_us(pStru->lastTs);
	
	// 初始化处理
	if (pStru->lastTs == 0)
	{
		pStru->lastTs = curTick;
		pStru->lastTotalAngle = pStru->numOfCircles * _2PI + pStru->angle;
		return 0.0f;
	}
	
	// 时间有效性校验（最小和最大时间间隔）
	if (deltaT < 50 || deltaT > 1000000) // 50微秒~1秒的有效范围
	{
		return pStru->velocity;
	}
	
	/* 精确角度差计算 */
	float32_t currentTotalAngle = pStru->numOfCircles * _2PI + pStru->angle;
	float32_t deltaAngle = currentTotalAngle - pStru->lastTotalAngle;
	
	if (as5047_fabsf(deltaAngle) > PI)
	{
		deltaAngle -= (deltaAngle > 0) ? _2PI : -_2PI;
	}
	
	/* 精确时间计算 */
	float32_t delta_t_s = deltaT * 1e-6f; // 转换为秒
	
	/* 速度计算（带符号保护） */
	float32_t velocity = (delta_t_s > 1e-6f) ? (deltaAngle / delta_t_s) : 0.0f;
	
	/* 动态滤波 - 优化版本 */
	float32_t abs_vel = as5047_fabsf(velocity);
	float32_t alpha;
	
	if (abs_vel < 10.0f)
	{
		alpha = 0.01f; // 低速时使用最小滤波系数
	} 
	else if (abs_vel > 100.0f)
	{
		alpha = 0.2f; // 高速时使用最大滤波系数
	} 
	else
	{
		alpha = 0.01f + 0.19f * (abs_vel / 100.0f); // 线性比例计算滤波系数
	}
	
	pStru->velocity = alpha * velocity + (1.0f - alpha) * pStru->velocity;
	
	/* 更新基准值 */
	pStru->lastTotalAngle = currentTotalAngle;
	pStru->lastTs = curTick;
	
	return pStru->velocity;
}

/**
 * @brief  计算SPI2传感器的角速度
 * @param  None
 * @retval 角速度(rad/s)
 */
float32_t AS5047_M0_GetVelocity(void)
{
	return AS5047_GetVelocity(&Sensor2);
}

/**
 * @brief  计算SPI3传感器的角速度
 * @param  None
 * @retval 角速度(rad/s)
 */
float32_t AS5047_M1_GetVelocity(void)
{
	return AS5047_GetVelocity(&Sensor3);
}

/**
 * @brief  AS5047P初始化函数
 * @param  None
 * @retval None
 */
void AS5047P_Init(void)
{
	// 初始化SPI CS引脚为高电平（未选中状态）
	SPI2_AS5047P_CS_H;
	SPI3_AS5047P_CS_H;
	
	// 初始化传感器数据结构
	Sensor2.motorNum = 0;  // 传感器2对应电机0
	Sensor2.sensorNum = 3;  // 传感器编号3（SPI2接口）
	
	Sensor3.motorNum = 1;  // 传感器3对应电机1
	Sensor3.sensorNum = 4;  // 传感器编号4（SPI3接口）
	
	// 读取传感器状态，确认通信正常
	uint16_t status_spi2 = AS5047P_read(&hspi2, SPI2_CS_GPIO_Port, SPI2_CS_Pin, ERRFL);
	uint16_t status_spi3 = AS5047P_read(&hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin, ERRFL);
	
	// 读取诊断信息
	uint16_t diag_spi2 = AS5047P_read(&hspi2, SPI2_CS_GPIO_Port, SPI2_CS_Pin, DIAAGC);
	uint16_t diag_spi3 = AS5047P_read(&hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin, DIAAGC);
	
	// 保存初始化状态
	Sensor2.status = status_spi2;
	Sensor2.diag = diag_spi2;
	Sensor3.status = status_spi3;
	Sensor3.diag = diag_spi3;
}

/**
 * @brief  SPI2接口读写一个字节
 * @param  _txdata: 要发送的数据
 * @retval 接收到的数据
 */
uint16_t SPI2_ReadWrite_OneByte(uint16_t _txdata)
{
	return SPI_ReadWrite_OneByte(&hspi2, SPI2_CS_GPIO_Port, SPI2_CS_Pin, _txdata);
}

/**
 * @brief  SPI3接口读写一个字节
 * @param  _txdata: 要发送的数据
 * @retval 接收到的数据
 */
uint16_t SPI3_ReadWrite_OneByte(uint16_t _txdata)
{
	return SPI_ReadWrite_OneByte(&hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin, _txdata);
}

/**
 * @brief  使用SPI2读取AS5047P寄存器
 * @param  add: 寄存器地址
 * @retval 读取的数据
 */
uint16_t SPI2_AS5047P_read(uint16_t add)
{
	return AS5047P_read(&hspi2, SPI2_CS_GPIO_Port, SPI2_CS_Pin, add);
}

/**
 * @brief  使用SPI3读取AS5047P寄存器
 * @param  add: 寄存器地址
 * @retval 读取的数据
 */
uint16_t SPI3_AS5047P_read(uint16_t add)
{
	return AS5047P_read(&hspi3, SPI3_CS_GPIO_Port, SPI3_CS_Pin, add);
}
