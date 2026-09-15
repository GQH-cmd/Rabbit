/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  lcd.c
 * Description      :  lcd driver base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         FEB 03rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __LCD_H
#define __LCD_H 

#ifdef _cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "main.h"
//#include "font.h"
#include "spi.h"
#include "gpio.h"
#include "Hzlib.h"
#include "ui.h"
#include "Motor.h"
#include "Filter.h"

/* Define -------------------------------------------------------------------*/
//Width and height definitions of LCD
#define LCD_Width 			240
#define LCD_Height 			135
#define Width_Offset		40
#define Height_Offset		53
//Brush color
#define WHITE         	 	0xFFFF
#define BLACK         		0x0000	  
#define BLUE         	 	0x001F  
#define BRED             	0XF81F
#define GRED 			 	0XFFE0
#define GBLUE			 	0X07FF
#define RED           	 	0xF800
#define MAGENTA       	 	0xF81F
#define GREEN         	 	0x07E0
#define CYAN          	 	0x7FFF
#define YELLOW        	 	0xFFE0 //黄色
#define BROWN 			 	0XBC40 //棕色
#define BRRED 			 	0XFC07 //棕红色
#define GRAY  			 	0X8430 //灰色
#define DARKBLUE      	 	0X01CF //深蓝色
#define LIGHTBLUE      	 	0X7D7C //浅蓝色  
#define GRAYBLUE       	 	0X5458 //灰蓝色
#define LIGHTGREEN     	 	0X841F //浅绿色
#define LGRAY 			 	0XC618 //浅灰色(PANNEL)，窗体背景色
#define LGRAYBLUE        	0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           	0X2B12 //浅棕蓝色(选择条目的反色)

/*
	LCD_RST:	PD3
	LCD_DC:		PB4	
*/

#define	LCD_RST(n)		(n?HAL_GPIO_WritePin(GPIOD,GPIO_PIN_3,GPIO_PIN_SET):HAL_GPIO_WritePin(GPIOD,GPIO_PIN_3,GPIO_PIN_RESET))
#define	LCD_DC(n)		(n?HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4,GPIO_PIN_SET):HAL_GPIO_WritePin(GPIOB,GPIO_PIN_4,GPIO_PIN_RESET))

/* Private Variables --------------------------------------------------------*/
extern uint16_t	POINT_COLOR;		//Default brush color
extern uint16_t	BACK_COLOR;			//Default background color
extern volatile uint8_t spi_dma_busy;

/* Private function prototypes -----------------------------------------------*/ 
void LCD_Init(void);																					// Init

void LCD_Write_HalfWord(const uint16_t da);																// Write half a byte of data to LCD
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);								// Setting up the data display area

void LCD_Clear(uint16_t color);																			// Clean screen
void LCD_Fill(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);		// Filled monochrome

void LCD_Draw_Point(uint16_t x, uint16_t y);															// Draw points
void LCD_Draw_ColorPoint(uint16_t x, uint16_t y,uint16_t color);										// Painting with color dots
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);									// Draw line
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);								// Draw rectangle
void LCD_Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r);												// Circle drawing
void LCD_Show_Image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *p);			// display picture

void LCD_Show_O1(void);		// 开机loge
void LCD_Show_O2(void);
void LCD_Show_O3(void);
void LCD_Show_O4(void);
void LCD_Show_O5(void);
void LCD_Show_O6(void);
void LCD_Show_O7(void);

void LCD_Show_BG(void);
void LCD_Show_Poewr_Num(float32_t Voltage_Real_value);
void LCD_Show_Vol_Num(float32_t Voltage_Real_value);
void LCD_Show_Temperature_Num(float32_t Temp_Real_value1, float32_t Temp_Real_value2);
void LCD_Show_Motor_Mode(uint8_t M0_Mode, uint8_t M1_Mode);
void LCD_Show_Motor_Vel(float32_t M0_Vel, float32_t M1_Vel);
//void LCD_Show_Motor_Cur(float32_t M0_Cur, float32_t M1_Cur);
void LCD_Show_Servo_Angle(uint8_t S0_Angle, uint8_t S1_Angle);

#ifdef _cplusplus
}
#endif

#endif 