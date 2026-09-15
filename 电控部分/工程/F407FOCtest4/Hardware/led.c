/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  led.c
 * Description      :  led light base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 15rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "led.h"

/* Define -------------------------------------------------------------------*/
#define LED_STATE_TABLE_SIZE (sizeof(ledStateTable) / sizeof(LEDState))

/* Private Variables --------------------------------------------------------*/
static const LEDState ledStateTable[] = 
{
	/* 顺序：1→2→3→4→5→7→6→8(跑马灯) */
	// case1: A=0, B=0, C=0 → next=2
	{GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET, 2},
	// case2: A=1, B=0, C=0 → next=3
	{GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_RESET, 3},
	// case3: A=0, B=1, C=0 → next=4
	{GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_RESET, 4},
	// case4: A=1, B=1, C=0 → next=5
	{GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_RESET, 5},
	// case5: A=0, B=0, C=1 → next=6
	{GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET,   6},
	// case6: A=0, B=1, C=1 → next=7
	{GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_SET,   7},
	// case7: A=1, B=0, C=1 → next=8
	{GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_SET,   8},
	// case8: A=1, B=1, C=1 → next=1
	{GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_SET,   1},
};
/* Code ---------------------------------------------------------------------*/
/********************************************************************************
	更新LED显示
*********************************************************************************/
void Update_LED_State(void)
{
	static uint8_t currentState = 1; // 初始状态为case1（索引0对应case1）
	
	/* 获取当前状态 */
	const LEDState *state = &ledStateTable[currentState - 1];
	
	/* 设置LED引脚状态 */
	HAL_GPIO_WritePin(GPIOD, LEDA_Pin, state->ledA);
	HAL_GPIO_WritePin(GPIOD, LEDB_Pin, state->ledB);
	HAL_GPIO_WritePin(GPIOD, LEDC_Pin, state->ledC);
	
	/* 更新下一状态 */
	currentState = state->nextState;
}
/* End of this file */
