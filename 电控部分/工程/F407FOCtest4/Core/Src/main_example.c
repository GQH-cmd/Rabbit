/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main_example.c
  * @brief          : AS5047P 传感器测试程序
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "can.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"
#include "AS5047.h"

/* Private variables ---------------------------------------------------------*/
uint32_t time_now = 0;
uint32_t time_last = 0;
uint16_t zero_angle_spi2 = 0;
uint16_t zero_angle_spi3 = 0;
float32_t dt = 0.0f;  // 时间间隔（秒）

/**
  * @brief  使用AS5047P读取角度的主程序示例
  * @retval int
  */
int main_example(void)
{
  /* 初始化HAL库 */
  HAL_Init();

  /* 配置系统时钟 */
  SystemClock_Config();

  /* 初始化所有外设 */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();  // 用于时间测量
  /* 其他初始化代码 */

  /* 初始化AS5047P传感器 */
  AS5047P_Init();
  
  /* 延时等待传感器稳定 */
  HAL_Delay(100);
  
  /* 读取初始位置，用作零点 */
  zero_angle_spi2 = Sensor2.rawAngle;
  zero_angle_spi3 = Sensor3.rawAngle;
  
  printf("AS5047P传感器测试程序开始运行\n");
  printf("--------------------------------\n");
  printf("SPI2 零点角度: %u\n", zero_angle_spi2);
  printf("SPI3 零点角度: %u\n", zero_angle_spi3);
  printf("--------------------------------\n");
  
  /* 获取初始时间 */
  time_last = HAL_GetTick();
  
  /* 主循环 */
  while (1)
  {
    /* 计算时间间隔 */
    time_now = HAL_GetTick();
    dt = (float32_t)(time_now - time_last) / 1000.0f;  // 转换为秒
    time_last = time_now;
    
    /* 更新位置（多圈跟踪） */
    SPI2_AS5047P_UpdatePosition(zero_angle_spi2);
    SPI3_AS5047P_UpdatePosition(zero_angle_spi3);
    
    /* 计算速度 */
    SPI2_AS5047P_GetVelocity(dt);
    SPI3_AS5047P_GetVelocity(dt);
    
    /* 打印角度和速度数据 */
    printf("SPI2 - 角度: %.2f°, 总角度: %.2f°, 速度: %.2f rad/s (%.2f RPM)\n",
           Sensor2.degAngle, Sensor2.totalAngle, Sensor2.velocity, Sensor2.rpmSpeed);
    
    printf("SPI3 - 角度: %.2f°, 总角度: %.2f°, 速度: %.2f rad/s (%.2f RPM)\n",
           Sensor3.degAngle, Sensor3.totalAngle, Sensor3.velocity, Sensor3.rpmSpeed);
    
    printf("--------------------------------\n");
    
    /* 延时100ms */
    HAL_Delay(100);
  }
} 