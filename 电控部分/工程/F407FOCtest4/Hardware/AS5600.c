/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  AS5600.c
 * Description      :  AS5600 driver base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 24rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "AS5600.h"
#include "main.h"

/* Define -------------------------------------------------------------------*/
#define _2PI       		2 * PI
#define FILTER_ALPHA 	0.2f

#define SYSTEM_CLOCK		168000000.0f	// STM32F407主频168MHz
#define APB1_PRESCALER		4				// APB1分频系数
#define TIM2_PRESCALER		83				// TIM2预分频寄存器值
#define FILTER_ALPHA_BASE	0.1f			// 基础滤波系数

/* 预计算常量 */
#define RAW_TO_RAD          (_2PI / 4095.0f)    // 原始数据转弧度的系数
#define RAW_TO_DEG          (360.0f / 4096.0f)  // 原始数据转角度的系数
#define MIN_DELTA_TIME      50                  // 最小有效时间差值（计数值）
#define MAX_DELTA_TIME      (uint32_t)(1e6 / TIMER_PERIOD_US) // 最大有效时间差值（1秒）
#define VELOCITY_NORM       100.0f              // 速度归一化参数

/* Private Variables --------------------------------------------------------*/
AS5600 Sensor0 = {0};
AS5600 Sensor1 = {0};

// 计算TIM2实际时钟频率1MHz（单位：Hz）
const float32_t TIM2_CLOCK = (SYSTEM_CLOCK / APB1_PRESCALER) * 2 / (TIM2_PRESCALER + 1);
const float32_t TIMER_PERIOD_US = 1.0f / (TIM2_CLOCK / 1e6); // 每个计数对应的微秒数
/* Code ---------------------------------------------------------------------*/
/**
 * @brief  AS5600编码器初始化函数
 * @param  None
 * @retval None
 */
void AS5600_Init(void)
{
	Sensor0.motorNum = 0;			// motorNum 设置为 0
	Sensor0.sensorPrescaler = 2; 	// 10kHz
	Sensor1.motorNum = 1; 			// motorNum 设置为 1
	Sensor1.sensorPrescaler = 2; 	// 10kHz
}

/**
 * @brief  读取编码器角度值（通用函数）
 * @param  pStru: AS5600传感器结构体指针
 * @param  hi2c: I2C句柄
 * @retval None
 */
void AS5600_RdRawAngle(AS5600 *pStru, I2C_HandleTypeDef *hi2c)
{
    uint8_t rbuff[2] = {0};
    
    pStru->result = AS5600_RdReg(hi2c, RAW_ANGLE_H, rbuff, 2);
	if (pStru->result == AS5600_ERROR)
	{
		/* 配置GPIO为开漏输出 */
		GPIO_InitTypeDef GPIO_InitStruct;
		GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8 | GPIO_PIN_10, GPIO_PIN_SET);
		
		/* 关闭电机电源，开启刹车电路 */
		HAL_GPIO_WritePin(GPIOE, MOTOR_EN1_Pin, GPIO_PIN_RESET);	// 关闭电机电源1引脚
		HAL_GPIO_WritePin(GPIOE, MOTOR_EN2_Pin, GPIO_PIN_RESET);	// 关闭电机电源2引脚
		HAL_GPIO_WritePin(GPIOE, M0_BK_OUT_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, M1_BK_OUT_Pin, GPIO_PIN_SET);

		LCD_Show_Image(14, 75, 213, 22, MCUrestart);
		HAL_Delay(3000);
		NVIC_SystemReset();
	}
	else if (pStru->result == AS5600_OK)
	{
        pStru->rawAngle = (rbuff[0] & 0x0f) << 8| rbuff[1];			// 获得原始数据
		pStru->angle = pStru->rawAngle * RAW_TO_RAD;			    // 将原始角度转换为弧度值
	}
}

/**
 * @brief  使用I2C1读取AS5600传感器0的角度值
 * @param  None
 * @retval 角度值（弧度制）
 */
float32_t I2C1_AS5600_GetAngle(void)
{
    AS5600_RdRawAngle(&Sensor0, &hi2c1);
    return Sensor0.angle;
}

/**
 * @brief  使用I2C2读取AS5600传感器1的角度值
 * @param  None
 * @retval 角度值（弧度制）
 */
float32_t I2C2_AS5600_GetAngle(void)
{
    AS5600_RdRawAngle(&Sensor1, &hi2c2);
    return Sensor1.angle;
}

/**
 * @brief  计算传感器的角速度
 * @param  pStru: AS5600传感器结构体指针
 * @retval 角速度(rad/s)
 */
float32_t I2C_Get_Velocity(AS5600 *pStru)
{
	uint32_t curTick = get_micros();
	
	// 精确时间差计算（考虑32位计数器溢出）
	uint32_t deltaT = delta_time_us(pStru->lastTs);
	
	// 初始化处理
	if(pStru->lastTs == 0)
	{
		pStru->lastTs = curTick;
		pStru->lastTotalAngle = pStru->numOfCircle * _2PI + pStru->angle;
		return 0.0f;
	}
	
	// 时间有效性校验（最小和最大时间间隔）
	if(deltaT < 50 || deltaT > 1000000) // 50微秒~1秒的有效范围
	{
		return pStru->velocity;
	}
	
	/* 精确角度差计算 */
	float32_t currentTotalAngle = pStru->numOfCircle * _2PI + pStru->angle;
	float32_t deltaAngle = currentTotalAngle - pStru->lastTotalAngle;
	
	/* 角度环跳变修正 */
	if(my_fabsf(deltaAngle) > PI)
	{
		deltaAngle -= (deltaAngle > 0) ? _2PI : -_2PI;
	}
	
	/* 精确时间计算 */
	float32_t delta_t_s = deltaT * 1e-6f; // 转换为秒
	
	/* 速度计算（带符号保护） */
	float32_t velocity = (delta_t_s > 1e-6f) ? (deltaAngle / delta_t_s) : 0.0f;
	
	/* 动态滤波 - 优化版本 */
	float32_t abs_vel = my_fabsf(velocity);
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
 * @brief  计算电机0传感器的角速度
 * @param  None
 * @retval 角速度(rad/s)
 */
float32_t AS5600_M0_GetVelocity(void)
{
	return I2C_Get_Velocity(&Sensor0);
}

/**
 * @brief  计算电机1传感器的角速度
 * @param  None
 * @retval 角速度(rad/s)
 */
float32_t AS5600_M1_GetVelocity(void)
{
	return I2C_Get_Velocity(&Sensor1);
}

/**
 * @brief  更新传感器位置
 * @param  pStru: AS5600传感器结构体指针
 * @param  ZeroEAngle: 零点角度
 * @retval None
 */
void I2C_Update_Position(AS5600 *pStru, float32_t ZeroEAngle)
{
    static float32_t prevDeg[2] = {0.0f, 0.0f}; // 为两个电机分别保存前一角度
    int motorIdx = pStru->motorNum; // 获取电机索引(0或1)
    
    // 使用预计算常量，减少浮点运算
    pStru->curDegAngle = Normalize_DegAngle((pStru->rawAngle - ZeroEAngle) * RAW_TO_DEG);
    
    float32_t delta = pStru->curDegAngle - prevDeg[motorIdx];
    // 使用预定义的180.0f常量判断跨圈
    if (my_fabsf(delta) > 180.0f)
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

/**
 * @brief  更新电机0传感器位置
 * @param  pStru: AS5600传感器结构体指针
 * @param  ZeroEAngle: 零点角度
 * @retval None
 */
void M0_I2C_UpdatePosition(AS5600 *pStru, float32_t ZeroEAngle)
{
    I2C_Update_Position(pStru, ZeroEAngle);
}

/**
 * @brief  更新电机1传感器位置
 * @param  pStru: AS5600传感器结构体指针
 * @param  ZeroEAngle: 零点角度
 * @retval None
 */
void M1_I2C_UpdatePosition(AS5600 *pStru, float32_t ZeroEAngle)
{
    I2C_Update_Position(pStru, ZeroEAngle);
}

/**
 * @brief  使用I2C读取AS5600寄存器
 * @param  hi2c: I2C句柄
 * @param  regAdd: 寄存器地址
 * @param  pData: 数据缓冲区指针
 * @param  Size: 数据大小
 * @retval 操作结果(AS5600_OK或AS5600_ERROR)
 */
uint8_t AS5600_RdReg(I2C_HandleTypeDef *hi2c, uint16_t regAdd, uint8_t *pData, uint16_t Size)
{
    HAL_StatusTypeDef status;
    uint32_t timeout = 10; // 单次超时时间

	status = HAL_I2C_Mem_Read(hi2c, AS5600_SLAVE_ADDRESS, regAdd, I2C_MEMADD_SIZE_8BIT, pData, Size, timeout);

	if (status == HAL_OK) 
	{
		// 读取成功：不熄灭TEST_LED，返回成功
		HAL_GPIO_WritePin(GPIOE, TEST_LED_Pin, GPIO_PIN_RESET);
		return AS5600_OK;
	}
	else 
	{
		// 熄灭TEST_LED
		HAL_GPIO_WritePin(GPIOE, TEST_LED_Pin, GPIO_PIN_SET);
		if (hi2c->Instance == I2C1)
		{
			printf("I2C Error: Instance=0x%08X\n", (unsigned int)hi2c->Instance);
			LCD_Clear(BLACK);
			LCD_Show_Image(7, 43, 100, 25, M0AS5600Error);
		} 
		else if (hi2c->Instance == I2C2)
		{
			LCD_Clear(BLACK);
			LCD_Show_Image(133, 43, 100, 25, M1AS5600Error);
		}
		return AS5600_ERROR;
	}
}

/**
 * @brief  使用I2C写入AS5600寄存器
 * @param  hi2c: I2C句柄
 * @param  regAdd: 寄存器地址
 * @param  pData: 数据缓冲区指针
 * @param  Size: 数据大小
 * @retval 操作结果(AS5600_OK或AS5600_ERROR)
 */
uint8_t AS5600_WeReg(I2C_HandleTypeDef *hi2c, uint16_t regAdd, uint8_t *pData, uint16_t Size)
{
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Write(hi2c, AS5600_SLAVE_ADDRESS, regAdd, I2C_MEMADD_SIZE_8BIT, pData, Size, 1000);
  
	if (status == HAL_OK)
	{
		return AS5600_OK;
	}
	else if (status == HAL_BUSY) 
    {
		HAL_GPIO_WritePin(GPIOE, TEST_LED_Pin, GPIO_PIN_SET);
    }
	else 
    {
		HAL_GPIO_WritePin(GPIOE, TEST_LED_Pin, GPIO_PIN_SET);
    }
    return AS5600_ERROR; 
}

/**
 * @brief  实现精确的微秒级延时
 * @param  us: 延时时间(微秒)
 * @retval None
 */
void DWT_Delay_us(uint32_t us)
{
    uint32_t start = CoreDebug->DEMCR; // 使用CoreDebug代替DWT
    
    // 确保DWT CYCCNT可用
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    
    // 重置计数器
    DWT->CYCCNT = 0;
    
    // 启用计数器
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    
    // 开始计时
    start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < cycles);
}

/**
 * @brief  返回绝对值
 * @param  x: 输入值
 * @retval 绝对值
 */
float32_t my_fabsf(float32_t x)
{
	return (x >= 0.0f) ? x : -x;
}
