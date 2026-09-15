/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f4xx_hal.h"
#include "stdio.h"
#include "stdbool.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "lcd.h"
#include "FOC.h"
#include "AS5600.h"
#include "AS5047.h"
#include "Filter.h"
#include "led.h"
#include "ContactDetect.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 定义编码器类型
#define ENCODER_AS5600   0
#define ENCODER_AS5047   1
#define CONTACT_DETECT_DELTA_CURRENT       0.30f
#define CONTACT_DETECT_SAMPLE_PERIOD_MS    500U
#define CONTACT_DETECT_HOLD_TIME_MS        50U
#define CONTACT_DETECT_STARTUP_IGNORE_MS   1000U
#define CONTACT_DETECT_TREND_SAMPLES       10U
#define CONTACT_DETECT_MIN_DROP_COUNT      7U
#define CONTACT_DETECT_FINAL_SPEED_RATIO   0.50f
#define CONTACT_DETECT_STOP_RPM            65
#define CONTACT_DETECT_MIN_OPEN_TARGET     0.10f
#define CONTACT_DETECT_MIN_CURRENT_TARGET  0.05f
#define CONTACT_DETECT_MIN_SPEED_TARGET    5.0f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
struct MovingAverageFilter vol_avg_filter;
struct MovingAverageFilter temp1_avg_filter;
struct MovingAverageFilter temp2_avg_filter;

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t Pre_d, Comp_d, Duty;
uint16_t TFT_ADC_value; 			// TFT屏幕的ADC转换值
float32_t TFT_Real_value; 			// TFT滑动变阻器实际电压值

uint16_t Voltage_ADC_value;			// 电源的ADC转换值
float32_t Voltage_Real_value; 		// 电源的实际电压值

uint16_t Temp1_ADC_value;			// 温度的ADC转换值
uint16_t Temp2_ADC_value;
float32_t Temp1_Vol_value;			// 温度的实际电压值
float32_t Temp2_Vol_value;
float32_t R1_ntc;					// NTC值
float32_t R2_ntc;
float32_t temperature1;				// 温度的实际值
float32_t temperature2;

uint8_t powerMode = 1;				// 电源供电模式
uint8_t S0_Angle = 0;				// 舵机角度
uint8_t S1_Angle = 0;

uint8_t rx_buffer[50];				// 串口缓存
uint8_t rx_len;						// 串口数据长度

// 编码器选择变量
uint8_t M0_EncoderType = ENCODER_AS5600;  // 默认使用AS5600
uint8_t M1_EncoderType = ENCODER_AS5600;  // 默认使用AS5047
ContactDetector M0_ContactDetector;
uint8_t M0_ContactDetected = 0U;
uint8_t M0_ContactWasEnabled = 0U;
uint32_t M0_ContactLastSampleMs = 0U;
uint32_t M0_ContactStartMs = 0U;
float32_t M0_SpeedSamples[CONTACT_DETECT_TREND_SAMPLES];
uint8_t M0_SpeedSampleCount = 0U;
uint8_t M0_SpeedSampleIndex = 0U;
int32_t M0_LastSpeedRpmAbs = 0;
uint8_t M0_HasLastSpeed = 0U;
ContactDetector M1_ContactDetector;
uint8_t M1_ContactDetected = 0U;
uint8_t M1_ContactWasEnabled = 0U;
uint32_t M1_ContactLastSampleMs = 0U;
uint32_t M1_ContactStartMs = 0U;
float32_t M1_SpeedSamples[CONTACT_DETECT_TREND_SAMPLES];
uint8_t M1_SpeedSampleCount = 0U;
uint8_t M1_SpeedSampleIndex = 0U;
int32_t M1_LastSpeedRpmAbs = 0;
uint8_t M1_HasLastSpeed = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static float32_t App_Abs(float32_t value)
{
	return (value < 0.0f) ? -value : value;
}

static int32_t App_Scale100(float32_t value)
{
	return (int32_t)(value * 100.0f);
}

static int32_t App_Rpm(float32_t velocity_rad_s)
{
	return (int32_t)(velocity_rad_s * (60.0f / (2.0f * PI)));
}

static float32_t M0_GetSpeedAbs(void)
{
	float32_t velocity = (M0_EncoderType == ENCODER_AS5600) ? Sensor0.velocity : Sensor2.velocity;
	return App_Abs(velocity);
}

static float32_t M1_GetSpeedAbs(void)
{
	float32_t velocity = (M1_EncoderType == ENCODER_AS5600) ? Sensor1.velocity : Sensor3.velocity;
	return App_Abs(velocity);
}

static void M0_ResetSpeedTrend(void)
{
	M0_SpeedSampleCount = 0U;
	M0_SpeedSampleIndex = 0U;
	M0_LastSpeedRpmAbs = 0;
	M0_HasLastSpeed = 0U;
	for (uint8_t i = 0U; i < CONTACT_DETECT_TREND_SAMPLES; i++)
	{
		M0_SpeedSamples[i] = 0.0f;
	}
}

static void M1_ResetSpeedTrend(void)
{
	M1_SpeedSampleCount = 0U;
	M1_SpeedSampleIndex = 0U;
	M1_LastSpeedRpmAbs = 0;
	M1_HasLastSpeed = 0U;
	for (uint8_t i = 0U; i < CONTACT_DETECT_TREND_SAMPLES; i++)
	{
		M1_SpeedSamples[i] = 0.0f;
	}
}

static uint8_t M0_SpeedIsBelowStopRpm(void)
{
	int32_t current_rpm_abs = App_Rpm(M0_GetSpeedAbs());

	M0_LastSpeedRpmAbs = current_rpm_abs;
	M0_HasLastSpeed = 1U;
	return (current_rpm_abs <= CONTACT_DETECT_STOP_RPM) ? 1U : 0U;
}

static uint8_t M1_SpeedIsBelowStopRpm(void)
{
	int32_t current_rpm_abs = App_Rpm(M1_GetSpeedAbs());

	M1_LastSpeedRpmAbs = current_rpm_abs;
	M1_HasLastSpeed = 1U;
	return (current_rpm_abs <= CONTACT_DETECT_STOP_RPM) ? 1U : 0U;
}

static uint8_t M0_IsRunningCommand(void)
{
	if (M0.mode == MODE_OPEN)
	{
		return App_Abs(M0.param.Ope) > CONTACT_DETECT_MIN_OPEN_TARGET;
	}
	if (M0.mode == MODE_CURRENT)
	{
		return App_Abs(M0.param.Cur) > CONTACT_DETECT_MIN_CURRENT_TARGET;
	}
	if (M0.mode == MODE_VELOCITY)
	{
		return App_Abs(M0.param.Vel) > CONTACT_DETECT_MIN_SPEED_TARGET;
	}
	if (M0.mode == MODE_POSITION)
	{
		return 0U;
	}
	return 0U;
}

static uint8_t M1_IsRunningCommand(void)
{
	if (M1.mode == MODE_OPEN)
	{
		return App_Abs(M1.param.Ope) > CONTACT_DETECT_MIN_OPEN_TARGET;
	}
	if (M1.mode == MODE_CURRENT)
	{
		return App_Abs(M1.param.Cur) > CONTACT_DETECT_MIN_CURRENT_TARGET;
	}
	if (M1.mode == MODE_VELOCITY)
	{
		return App_Abs(M1.param.Vel) > CONTACT_DETECT_MIN_SPEED_TARGET;
	}
	if (M1.mode == MODE_POSITION)
	{
		return 0U;
	}
	return 0U;
}

static void App_StopBothMotorsForContact(void)
{
	M0.mode = MODE_OPEN;
	M0.param.Ope = 0.0f;
	M0_Curs.iqr = 0.0f;
	M0_Vels.velocityTar = 0.0f;
	M1.mode = MODE_OPEN;
	M1.param.Ope = 0.0f;
	M1_Curs.iqr = 0.0f;
	M1_Vels.velocityTar = 0.0f;
	HAL_GPIO_WritePin(TEST_LED_GPIO_Port, TEST_LED_Pin, GPIO_PIN_SET);
}

static void M0_StopForContact(void)
{
	App_StopBothMotorsForContact();
	printf("CONTACT M0 rpm_abs=%ld iq_x100=%ld base_x100=%ld\r\n",
		   (long)M0_LastSpeedRpmAbs,
		   (long)App_Scale100(M0_Curs.iq),
		   (long)App_Scale100(M0_ContactDetector.baseline_current));
}

static void M1_StopForContact(void)
{
	App_StopBothMotorsForContact();
	printf("CONTACT M1 rpm_abs=%ld iq_x100=%ld base_x100=%ld\r\n",
		   (long)M1_LastSpeedRpmAbs,
		   (long)App_Scale100(M1_Curs.iq),
		   (long)App_Scale100(M1_ContactDetector.baseline_current));
}

static void M0_ContactDetect_Task(void)
{
	uint32_t now = HAL_GetTick();
	uint8_t enable = M0_IsRunningCommand();

	if ((now - M0_ContactLastSampleMs) < CONTACT_DETECT_SAMPLE_PERIOD_MS)
	{
		return;
	}
	M0_ContactLastSampleMs = now;

	if ((enable != 0U) && (M0_ContactWasEnabled == 0U))
	{
		ContactDetect_Reset(&M0_ContactDetector);
		M0_ResetSpeedTrend();
		M0_ContactDetected = 0U;
		M0_ContactStartMs = now;
		HAL_GPIO_WritePin(TEST_LED_GPIO_Port, TEST_LED_Pin, GPIO_PIN_RESET);
	}
	M0_ContactWasEnabled = enable;

	if (enable == 0U)
	{
		if (M0_ContactDetected == 0U)
		{
			ContactDetect_Reset(&M0_ContactDetector);
			M0_ResetSpeedTrend();
			HAL_GPIO_WritePin(TEST_LED_GPIO_Port, TEST_LED_Pin, GPIO_PIN_RESET);
		}
		return;
	}

	if ((now - M0_ContactStartMs) < CONTACT_DETECT_STARTUP_IGNORE_MS)
	{
		M0_ResetSpeedTrend();
		return;
	}

	if ((M0_ContactDetected == 0U) && (M0_SpeedIsBelowStopRpm() != 0U))
	{
		M0_ContactDetected = 1U;
		M0_StopForContact();
	}
}

static void M1_ContactDetect_Task(void)
{
	uint32_t now = HAL_GetTick();
	uint8_t enable = M1_IsRunningCommand();

	if ((now - M1_ContactLastSampleMs) < CONTACT_DETECT_SAMPLE_PERIOD_MS)
	{
		return;
	}
	M1_ContactLastSampleMs = now;

	if ((enable != 0U) && (M1_ContactWasEnabled == 0U))
	{
		ContactDetect_Reset(&M1_ContactDetector);
		M1_ResetSpeedTrend();
		M1_ContactDetected = 0U;
		M1_ContactStartMs = now;
		HAL_GPIO_WritePin(TEST_LED_GPIO_Port, TEST_LED_Pin, GPIO_PIN_RESET);
	}
	M1_ContactWasEnabled = enable;

	if (enable == 0U)
	{
		if (M1_ContactDetected == 0U)
		{
			ContactDetect_Reset(&M1_ContactDetector);
			M1_ResetSpeedTrend();
			HAL_GPIO_WritePin(TEST_LED_GPIO_Port, TEST_LED_Pin, GPIO_PIN_RESET);
		}
		return;
	}

	if ((now - M1_ContactStartMs) < CONTACT_DETECT_STARTUP_IGNORE_MS)
	{
		M1_ResetSpeedTrend();
		return;
	}

	if ((M1_ContactDetected == 0U) && (M1_SpeedIsBelowStopRpm() != 0U))
	{
		M1_ContactDetected = 1U;
		M1_StopForContact();
	}
}

static void App_StatusPrint_Task(void)
{
	static uint32_t last_print_ms = 0U;
	uint32_t now = HAL_GetTick();

	if ((now - last_print_ms) < 500U)
	{
		return;
	}
	last_print_ms = now;

	printf("CUR m0_iq_x100=%ld m1_iq_x100=%ld m0_mode=%u m1_mode=%u contact0=%u contact1=%u\r\n",
		   (long)App_Scale100(M0_Curs.iq),
		   (long)App_Scale100(M1_Curs.iq),
		   (unsigned int)M0.mode,
		   (unsigned int)M1.mode,
		   (unsigned int)M0_ContactDetected,
		   (unsigned int)M1_ContactDetected);

	float32_t m0_velocity = (M0_EncoderType == ENCODER_AS5600) ? Sensor0.velocity : Sensor2.velocity;
	float32_t m1_velocity = (M1_EncoderType == ENCODER_AS5600) ? Sensor1.velocity : Sensor3.velocity;
	printf("VEL m0_rpm=%ld m1_rpm=%ld\r\n",
		   (long)App_Rpm(m0_velocity),
		   (long)App_Rpm(m1_velocity));
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_TIM1_Init();
  MX_TIM8_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_CAN1_Init();
  MX_USART1_UART_Init();
  MX_SPI2_Init();
  MX_USART2_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_SPI1_Init();
  MX_SPI3_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */
//  DWT_Init();
  Start();
  ContactDetect_Init(&M0_ContactDetector,
					 CONTACT_DETECT_DELTA_CURRENT,
					 CONTACT_DETECT_HOLD_TIME_MS / CONTACT_DETECT_SAMPLE_PERIOD_MS);
  ContactDetect_Init(&M1_ContactDetector,
					 CONTACT_DETECT_DELTA_CURRENT,
					 CONTACT_DETECT_HOLD_TIME_MS / CONTACT_DETECT_SAMPLE_PERIOD_MS);
  printf("BOOT CONTACT TEST\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	M0_ContactDetect_Task();
	M1_ContactDetect_Task();
	App_StatusPrint_Task();
// HAL_Delay(100);
//	printf("%.3f, %.3f, %.3f, %.3f, %.3f\n", M1_Curs.ialpha, M1_Curs.ibeta, M1_Curs.id, M1_Curs.iq, zero);
//	printf("%.3f, %.3f, %.3f, %.3f\n", M0_Curs.iqr, M0_Curs.iq, M0_Curs.idr, M0_Curs.id);
//	printf("%.3f, %.3f, %.3f, %.3f\n", M0_Vels.velocityTar, Sensor0.velocity * (60.0f / (2 * PI)), M0_Curs.iqr, M0_Curs.iq);
//	printf("%.3f, %.3f, %.3f, %.3f\n", M1_Curs.iu, M1_Curs.iv, M1_Curs.iw, zero);
//	printf("%.3f, %.3f, %.3f, %.3f\n", TFT_Real_value, Voltage_Real_value, temperature1, temperature2);

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
uint8_t count1 = 0;
uint8_t count2 = 1;
uint8_t count3 = 0;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM4)
	{
		/* 计算TFT分压电阻电压值 */
		TFT_ADC_value = Read_ADC_Value(&hadc2, ADC_CHANNEL_0);  // 调用函数触发ADC
		TFT_Real_value = TFT_ADC_value * (3.3f / 4096);  		// 计算实际电压
		Duty = TFT_Real_value * (100 / 3.3f);					// 计算占空比
		Comp_d = Pre_d * Duty / 100;
		
		/* 设置比较值 */
		if (__HAL_TIM_GET_COMPARE(&htim4, TIM_CHANNEL_1) != Comp_d)
		{
			__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, Comp_d);
		}
		static uint8_t i = 0;
		if (i == 9)
		{
			ADC_dispose();
			i = 0;
			/* 计算电源电压值 */
			Voltage_ADC_value = Read_ADC_Value(&hadc1, ADC_CHANNEL_1);			// 调用函数触发ADC
			Voltage_ADC_value = MovingAvg_Update(&vol_avg_filter, Voltage_ADC_value);
			Voltage_Real_value = Voltage_ADC_value  * (3.3f / 4096) * 11.3f;	// 计算实际电压
			
			/* 计算热敏电阻值 */		
			Temp1_ADC_value = adcValue.value1;
			Temp2_ADC_value = adcValue.value2;
			Temp1_Vol_value = Temp1_ADC_value  * (3.3f / 4096);
			Temp2_Vol_value = Temp2_ADC_value  * (3.3f / 4096);
			R1_ntc = (3.3f * 3.0f / Temp1_Vol_value) - 1;
			R2_ntc = (3.3f * 3.0f / Temp2_Vol_value) - 1;
			temperature1 = R_ntc_to_temperature(R1_ntc, 0.0f, 60.0f, 1.0f);
			temperature2 = R_ntc_to_temperature(R2_ntc, 0.0f, 60.0f, 1.0f);
			temperature1 = MovingAvg_Update(&temp1_avg_filter, temperature1);
			temperature2 = MovingAvg_Update(&temp2_avg_filter, temperature2);
		}
		i++;
	}
	if (htim->Instance == TIM6)
	{
		count1++;
		if (count1 == 6)
		{
			count1 = 0;
			LCD_Show_Vol_Num(Voltage_Real_value);
			LCD_Show_Servo_Angle(S0_Angle, S1_Angle);
			LCD_Show_Poewr_Num(powerMode);
			LCD_Show_Temperature_Num(temperature1, temperature2);
		}

		LCD_Show_Motor_Mode(M0.mode, M1.mode);
		
		// 根据编码器类型显示对应的速度值
		float32_t M0_velocity = (M0_EncoderType == ENCODER_AS5600) ? Sensor0.velocity : Sensor2.velocity;
		float32_t M1_velocity = (M1_EncoderType == ENCODER_AS5600) ? Sensor1.velocity : Sensor3.velocity;
		
		LCD_Show_Motor_Vel(M0_velocity * (60.0f / (2 * PI)), M1_velocity * (60.0f / (2 * PI)));
		LCD_Show_Servo_Angle(S0_Angle, S1_Angle);	
	}
	if (htim->Instance == TIM7)
	{
		Update_LED_State();
	}
}

void Start(void)
{
	Motor_TIM18_Init();
	LCD_TIM4_Init();
	LCD_Init();
	Pre_d = __HAL_TIM_GET_AUTORELOAD(&htim4);
	LCD_Clear(BLACK);
	HAL_Delay(100);
	LCD_Show_O1();
	HAL_Delay(100);
	
	// 根据选择初始化对应编码器
	if (M0_EncoderType == ENCODER_AS5600 && M1_EncoderType == ENCODER_AS5600)
	{
		// 初始化AS5600编码器
		AS5600_Init();
		I2C1_AS5600_GetAngle();
		I2C2_AS5600_GetAngle();
	} else if (M0_EncoderType == ENCODER_AS5047 && M1_EncoderType == ENCODER_AS5047)
	{
		// 初始化AS5047编码器
		AS5047P_Init();
	} else {
		// 混合使用编码器的情况
		if (M0_EncoderType == ENCODER_AS5600)
		{
			AS5600_Init();
			I2C1_AS5600_GetAngle();
		} else {
			AS5047P_Init();
		}
		
		if (M1_EncoderType == ENCODER_AS5600)
		{
			if (M0_EncoderType != ENCODER_AS5600)
			{
				AS5600_Init(); // 如果M0没有初始化AS5600，则需要初始化
			}
			I2C2_AS5600_GetAngle();
		}
	}
	
	LCD_Show_O2();
	HAL_Delay(100);
	MovingAvg_Init(&vol_avg_filter, 5);
	MovingAvg_Init(&temp1_avg_filter, 20);
	MovingAvg_Init(&temp2_avg_filter, 20);
	LCD_Show_O3();
	HAL_Delay(100);
	HAL_ADC_Start_IT(&hadc1);
	LCD_Show_O4();
	HAL_Delay(100);

	HAL_ADC_Start_IT(&hadc2);
	HAL_Delay(100);
	LCD_Show_O5();

	FOC_Init();
	LCD_Show_O6();
	HAL_Delay(10);
	LCD_Show_O7();
	HAL_Delay(800);
	LCD_Show_BG();
	
	LCD_Show_Poewr_Num(Voltage_Real_value);
	LCD_Show_Temperature_Num(temperature1, temperature2);
	LCD_Show_Vol_Num(Voltage_Real_value);
	LCD_Show_Motor_Mode(M0.mode, M1.mode);
	
	LCD_Show_Motor_Vel(0.0f, 0.0f);
	LCD_Show_Servo_Angle(S0_Angle, S1_Angle);
	Other_TIM26_Init();
	TIM7_Init();

	HAL_ADCEx_InjectedStart(&hadc1);
	HAL_ADC_Start_IT(&hadc1);
	HAL_ADCEx_InjectedStart(&hadc2);
	HAL_ADC_Start_IT(&hadc2);
	HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adcData, 2);
	
	__HAL_ADC_ENABLE_IT(&hadc1, ADC_IT_JEOC);
	__HAL_TIM_ENABLE_IT(&htim1, TIM_IT_BREAK);
	__HAL_ADC_ENABLE_IT(&hadc2, ADC_IT_JEOC);
	__HAL_TIM_ENABLE_IT(&htim8, TIM_IT_BREAK);
}

// 基于DWT的微秒级延时
void DWT_Init(void)
{
	if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk))
	{
		CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
		DWT->CYCCNT = 0;
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	}
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
