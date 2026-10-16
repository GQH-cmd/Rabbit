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
#define AS5600_VEL_MIN_US   1000U               // 至少累计1ms角度差，避免量化抖动被短间隔放大
#define AS5600_VEL_MAX_US   1000000U            // 超过1秒重新建立差分基准，不把未知速度填零
#define AS5600_VEL_FILTER_US 5000U              // 一阶低通时间常数5ms；系数随采样时间而非瞬时速度变化
#define VELOCITY_NORM       100.0f              // 速度归一化参数

/* Private Variables --------------------------------------------------------*/
AS5600 Sensor0 = {0};
AS5600 Sensor1 = {0};
volatile uint8_t AS5600_ReadFaults = 0U; /* bit0=I2C1，bit1=I2C2；复位前保留 */
static uint8_t AS5600_ReportedFaults = 0U;
extern uint8_t M0_EncoderType, M1_EncoderType;

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
    AS5600_ReadFaults = 0U;
    AS5600_ReportedFaults = 0U;
	Sensor0.motorNum = 0;			// motorNum 设置为 0
	Sensor0.sensorPrescaler = 2; 	// 10kHz
	Sensor1.motorNum = 1; 			// motorNum 设置为 1
	Sensor1.sensorPrescaler = 2; 	// 10kHz
    AS5600 *sensors[2] = {&Sensor0, &Sensor1};
    for (unsigned int i = 0; i < 2; ++i)
    {
        sensors[i]->velocity = 0.0f;
        sensors[i]->lastTs = 0U;
        sensors[i]->lastTotalAngle = 0.0f;
        sensors[i]->angleTs = 0U;
        sensors[i]->angleFresh = false;
        sensors[i]->velocityInitialized = false;
    }
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
		/* 出厂代码这里改写了 PB8~PB11，导致一路 I2C 失败后另一条总线也失效。
		 * 不改引脚复用、不自动复位；两电机项目任一路故障都停机。 */
		if (pStru == &Sensor0) M0_EncoderType = ENCODER_DISABLED;
		else if (pStru == &Sensor1) M1_EncoderType = ENCODER_DISABLED;
#if FOC_M1_ONLY
        if (pStru == &Sensor1) FOC_M1EncoderFaultStop();
#endif
		HAL_GPIO_WritePin(GPIOE, MOTOR_EN1_Pin, GPIO_PIN_RESET);	// 关闭电机电源1引脚
		HAL_GPIO_WritePin(GPIOE, MOTOR_EN2_Pin, GPIO_PIN_RESET);	// 关闭电机电源2引脚
		HAL_GPIO_WritePin(GPIOE, M0_BK_OUT_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOE, M1_BK_OUT_Pin, GPIO_PIN_SET);

		pStru->rawAngle = 0U;
		pStru->angle = 0.0f;
		pStru->velocity = 0.0f;
        pStru->angleFresh = false;
        pStru->velocityInitialized = false;
	}
	else if (pStru->result == AS5600_OK)
	{
        const uint32_t sampledAt = get_micros();
        pStru->rawAngle = (rbuff[0] & 0x0f) << 8| rbuff[1];			// 获得原始数据
		pStru->angle = pStru->rawAngle * RAW_TO_RAD;			    // 将原始角度转换为弧度值
        pStru->angleTs = sampledAt;
        pStru->angleFresh = true;
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
    /* I2C角度每几次控制回调才更新一次；旧角度不能当成新的零速度样本。
     * 使用读角度时的同一份时间戳，不能用控制函数调用时刻计算采样间隔。 */
    if (!pStru->angleFresh) return pStru->velocity;
    pStru->angleFresh = false;
    const uint32_t curTick = pStru->angleTs;
    const float32_t currentTotalAngle = pStru->numOfCircle * _2PI + pStru->angle;
    if (!pStru->velocityInitialized)
    {
        pStru->lastTs = curTick;
        pStru->lastTotalAngle = currentTotalAngle;
        pStru->velocityInitialized = true;
        return pStru->velocity;
    }

    /* TIM2是1MHz的32位计数器；无符号相减同时处理回绕。短窗口不更新基准，
     * 等下一份新角度累计足够的时间与位移，不能按“角度没变”强制填零。 */
    const uint32_t deltaT = curTick - pStru->lastTs;
    if (deltaT < AS5600_VEL_MIN_US) return pStru->velocity;
    if (deltaT > AS5600_VEL_MAX_US)
    {
        pStru->lastTs = curTick;
        pStru->lastTotalAngle = currentTotalAngle;
        return pStru->velocity;
    }
    float32_t deltaAngle = currentTotalAngle - pStru->lastTotalAngle;
    if (my_fabsf(deltaAngle) > PI)
    {
        deltaAngle -= (deltaAngle > 0.0f) ? _2PI : -_2PI;
    }
    const float32_t velocity = deltaAngle / ((float32_t)deltaT * 1e-6f);
    const float32_t alpha = (float32_t)deltaT / (AS5600_VEL_FILTER_US + deltaT);
    pStru->velocity += alpha * (velocity - pStru->velocity);
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
			AS5600_ReadFaults |= 1U;
		} 
		else if (hi2c->Instance == I2C2)
		{
			AS5600_ReadFaults |= 2U;
		}
		return AS5600_ERROR;
	}
}

/* 仅由主循环调用。中断读取失败必须先返回，让原有停机分支立即关电机，
 * 不能在停机前进行串口输出或整屏绘制。LCD由TIM6刷新，此处只打印，避免
 * 主循环与TIM6同时操作没有事务锁的LCD总线。锁存位不清除，不自动重启。 */
void AS5600_ReportErrors(void)
{
    const uint8_t pending = AS5600_ReadFaults & (uint8_t)~AS5600_ReportedFaults;
    if (pending == 0U) return;
    AS5600_ReportedFaults |= pending;
    printf("AS5600_READ_FAULT buses=0x%02X; motors disabled; STOP then RECOVER (M1-only); calibration/velocity faults require separate handling\r\n",
           (unsigned int)AS5600_ReadFaults);
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
