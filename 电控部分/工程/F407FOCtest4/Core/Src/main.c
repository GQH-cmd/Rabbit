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
#include <string.h>
#include "FocLinkProtocol.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 定义编码器类型
#define CONTACT_DETECT_DELTA_CURRENT       0.30f
#define CONTACT_DETECT_SAMPLE_PERIOD_MS    100U
#define CONTACT_DETECT_HOLD_TIME_MS        50U
#define CONTACT_DETECT_STARTUP_IGNORE_MS   1000U
#define CONTACT_DETECT_TREND_SAMPLES       10U
#define CONTACT_DETECT_MIN_DROP_COUNT      7U
#define CONTACT_DETECT_FINAL_SPEED_RATIO   0.50f
#define CONTACT_DETECT_STOP_RPM            65
#define CONTACT_DETECT_MIN_OPEN_TARGET     0.10f
#define CONTACT_DETECT_MIN_CURRENT_TARGET  0.05f
#define CONTACT_DETECT_MIN_SPEED_TARGET    5.0f
#ifdef ESP32_SPI_LINK
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
struct MovingAverageFilter vol_avg_filter;
struct MovingAverageFilter temp1_avg_filter;
struct MovingAverageFilter temp2_avg_filter;

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static void App_BootMarker(const char *text)
{
	HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100U);
}

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

/* USB1/USART1 command receiver.  Use one-byte interrupt reception so XCOM
 * commands are handled independently of the SPI3 link and DMA idle state. */
extern uint8_t USART1_RxByte;
#define UART1_FRAME_SIZE 50U
static uint8_t UART1_BuildBuffer[UART1_FRAME_SIZE];
static uint8_t UART1_FrameBuffer[UART1_FRAME_SIZE];
static volatile uint8_t UART1_BuildLength = 0U;
static volatile uint8_t UART1_FrameLength = 0U;
static volatile uint8_t UART1_FrameReady = 0U;

// USART2 无线模块接收缓存：一行命令以 '\n' 结束，例如 o0.2,o0\n
#define UART2_FRAME_SIZE 50U
static uint8_t UART2_RxByte;
static uint8_t UART2_BuildBuffer[UART2_FRAME_SIZE];
static uint8_t UART2_FrameBuffer[UART2_FRAME_SIZE];
static volatile uint8_t UART2_BuildLength = 0U;
static volatile uint8_t UART2_FrameLength = 0U;
static volatile uint8_t UART2_FrameReady = 0U;

#ifdef ESP32_SPI_LINK
/* 双向缓冲只在主循环重装；中断只标记完成，不覆盖待处理命令。 */
static uint8_t SPI3_LinkRxFrame[FLP_SIZE];
static uint8_t SPI3_LinkTxFrame[FLP_SIZE];
static volatile uint8_t SPI3_LinkFrameReady = 0U;
static volatile uint8_t SPI3_LinkError = 0U;
static uint8_t SPI3_LinkArmed = 0U, SPI3_LinkOwned = 0U;
static uint32_t SPI3_LinkLastAlive = 0U;
static FlpStatus SPI3_LinkStatus;
#endif

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

static void App_USART1_CommandTask(void)
{
	uint8_t local_frame[UART1_FRAME_SIZE];
	uint8_t local_length;
	static const uint8_t ack[] = "ACK\r\n";

	if (UART1_FrameReady == 0U)
	{
		return;
	}

	__disable_irq();
	local_length = UART1_FrameLength;
	memcpy(local_frame, UART1_FrameBuffer, local_length);
	UART1_FrameReady = 0U;
	__enable_irq();

	if (local_length > 0U)
	{
		uint8_t accepted = Parse_Command(local_frame, local_length);
#ifdef ESP32_SPI_LINK
        if (accepted) SPI3_LinkOwned = 0U;
#endif
        if (accepted) HAL_UART_Transmit(&huart1, (uint8_t *)ack, sizeof(ack) - 1U, 20U);
        else HAL_UART_Transmit(&huart1, (uint8_t *)"NACK\r\n", 6U, 20U);
	}
}

static void App_USART2_CommandTask(void)
{
	uint8_t local_frame[UART2_FRAME_SIZE];
	uint8_t local_length;
	static const uint8_t ack[] = "ACK\r\n";

	if (UART2_FrameReady == 0U)
	{
		return;
	}

	/* 暂停极短时间，避免主循环复制时被接收中断改写 */
	__disable_irq();
	local_length = UART2_FrameLength;
	memcpy(local_frame, UART2_FrameBuffer, local_length);
	UART2_FrameReady = 0U;
	__enable_irq();

	if (local_length > 0U)
	{
		/* 复用现有的 o/c/v/p 双电机命令解析器 */
		uint8_t accepted = Parse_Command(local_frame, local_length);
#ifdef ESP32_SPI_LINK
        if (accepted) SPI3_LinkOwned = 0U;
#endif
        if (accepted) HAL_UART_Transmit(&huart2, (uint8_t *)ack, sizeof(ack) - 1U, 20U);
        else HAL_UART_Transmit(&huart2, (uint8_t *)"NACK\r\n", 6U, 20U);
	}
}

#ifdef ESP32_SPI_LINK
/* 有符号遥测：转速单位 0.01 RPM，iq 单位 0.001 A；异常数值不强转溢出。 */
static int32_t App_LinkScale(float value, float scale)
{
    float v = value * scale;
    if (!isfinite(v) || v > 2147483000.0f || v < -2147483000.0f)
    {
        SPI3_LinkStatus.faults |= FLP_FAULT_VALUE;
        return 0;
    }
    return (int32_t)v;
}

static void App_SPI3_Stop(void)
{
    uint8_t stop[] = "o0,o0";
    (void)Parse_Command(stop, 5U);
    SPI3_LinkOwned = 0U;
}

static void App_SPI3_Snapshot(void)
{
    FlpStatus *s = &SPI3_LinkStatus;
    s->flags = FLP_READY | (s->id ? FLP_ACK : 0U);
    if (M0_EncoderType != ENCODER_DISABLED) s->flags |= FLP_ENCODER0;
    if (M1_EncoderType != ENCODER_DISABLED) s->flags |= FLP_ENCODER1;
    if (M0_ContactDetected) s->flags |= FLP_CONTACT0;
    if (M1_ContactDetected) s->flags |= FLP_CONTACT1;
    if (SPI3_LinkOwned) s->flags |= FLP_SPI_OWNER;
    /* 这里只表示编码器配置已启用，不冒充编码器硬件健康证明。 */
    float v0 = (M0_EncoderType == ENCODER_AS5600) ? Sensor0.velocity : Sensor2.velocity;
    float v1 = (M1_EncoderType == ENCODER_AS5600) ? Sensor1.velocity : Sensor3.velocity;
    s->rpm0 = (s->flags & FLP_ENCODER0) ? App_LinkScale(v0, 6000.0f / (2.0f * PI)) : 0;
    s->rpm1 = (s->flags & FLP_ENCODER1) ? App_LinkScale(v1, 6000.0f / (2.0f * PI)) : 0;
    s->iq0 = App_LinkScale(M0_Curs.iq, 1000.0f);
    s->iq1 = App_LinkScale(M1_Curs.iq, 1000.0f);
    s->mode0 = (uint8_t)M0.mode;s->mode1 = (uint8_t)M1.mode;
    s->uptime = HAL_GetTick();++s->sample;
    flp_reply(SPI3_LinkTxFrame, s);
}

static void App_SPI3_Arm(void)
{
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_RESET) return;
    /* 仅片选为高时复位 SPI3 外设，清除短帧/溢出/上一帧移位器残留。
     * 不改 GPIO 复用、不操作 SPI1/SPI2，不在 ISR 内阻塞等候。 */
    HAL_NVIC_DisableIRQ(SPI3_IRQn);
    __HAL_RCC_SPI3_FORCE_RESET();
    __HAL_RCC_SPI3_RELEASE_RESET();
    hspi3.State = HAL_SPI_STATE_READY;
    hspi3.Lock = HAL_UNLOCKED;
    SPI3_LinkFrameReady = 0U;SPI3_LinkError = 0U;
    if (HAL_SPI_Init(&hspi3) != HAL_OK) { SPI3_LinkArmed = 0U; }
    else {
        App_SPI3_Snapshot();
        SPI3_LinkArmed = (HAL_SPI_TransmitReceive_IT(&hspi3, SPI3_LinkTxFrame,
                               SPI3_LinkRxFrame, FLP_SIZE) == HAL_OK);
    }
    HAL_NVIC_ClearPendingIRQ(SPI3_IRQn);
    HAL_NVIC_EnableIRQ(SPI3_IRQn);
}

static void App_SPI3_Process(const uint8_t *frame)
{
    FlpRequest q;
    if (!flp_read_request(frame, &q)) {
        ++SPI3_LinkStatus.badFrames;SPI3_LinkStatus.faults |= FLP_FAULT_FRAME;return;
    }
    if (q.kind == FLP_POLL) {
        if (SPI3_LinkOwned) {
            if (q.keepalive) SPI3_LinkLastAlive = HAL_GetTick();
            else App_SPI3_Stop();
        }
        return;
    }
    /* 同一命令编号仅执行一次；POLL 不覆盖最近命令回执。 */
    if (SPI3_LinkStatus.id == q.id) return;
    SPI3_LinkStatus.id = q.id;
    if (!strcmp(q.text, "PING")) SPI3_LinkStatus.result = FLP_PONG;
    else if (Parse_Command((uint8_t *)q.text, (uint8_t)strlen(q.text))) {
        SPI3_LinkStatus.result = FLP_ACCEPTED;
        SPI3_LinkOwned = !(M0.mode == MODE_OPEN && M1.mode == MODE_OPEN &&
                          M0.param.Ope == 0.0f && M1.param.Ope == 0.0f);
        SPI3_LinkLastAlive = HAL_GetTick();
    } else {
        SPI3_LinkStatus.result = FLP_BAD_COMMAND;++SPI3_LinkStatus.badFrames;
    }
    printf("SPI3 ACK id=%lu result=%u\r\n", (unsigned long)q.id, SPI3_LinkStatus.result);
}

static void App_SPI3_CommandTask(void)
{
    /* 只监管 SPI 接管的运动；XCOM 有效命令接管后不受此看门狗影响。 */
    if (SPI3_LinkOwned && (HAL_GetTick() - SPI3_LinkLastAlive >= FLP_WATCHDOG_MS)) {
        App_SPI3_Stop();SPI3_LinkStatus.faults |= FLP_FAULT_WATCHDOG;
        printf("SPI3 WATCHDOG STOP\r\n");
    }
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_RESET) return;
    if (SPI3_LinkError) {
        ++SPI3_LinkStatus.badFrames;SPI3_LinkStatus.faults |= FLP_FAULT_TRANSPORT;
        App_SPI3_Arm();
    } else if (SPI3_LinkFrameReady) {
        /* 当前缓冲不再重装，主循环处理期间不会被下一帧覆盖。 */
        if (__HAL_SPI_GET_FLAG(&hspi3, SPI_FLAG_RXNE) || __HAL_SPI_GET_FLAG(&hspi3, SPI_FLAG_OVR)) {
            ++SPI3_LinkStatus.badFrames;SPI3_LinkStatus.faults |= FLP_FAULT_TRANSPORT;
        } else App_SPI3_Process(SPI3_LinkRxFrame);
        App_SPI3_Arm();
    } else if (!SPI3_LinkArmed || (hspi3.RxXferCount > 0U && hspi3.RxXferCount < FLP_SIZE)) {
        if (SPI3_LinkArmed) {++SPI3_LinkStatus.badFrames;SPI3_LinkStatus.faults |= FLP_FAULT_TRANSPORT;}
        App_SPI3_Arm();
    }
}
#endif

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
	if (M1_EncoderType == ENCODER_DISABLED)
	{
		return 0.0f;
	}
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
	if (M0_EncoderType == ENCODER_DISABLED)
	{
		return 0U;
	}
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
	if (M1_EncoderType == ENCODER_DISABLED)
	{
		return 0U;
	}
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

	/* Existing project behavior: a commanded motor whose measured speed stays
	 * below 65 RPM is treated as contact and both motors are stopped. */
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
	float32_t m1_velocity = M1_GetSpeedAbs();
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
  App_BootMarker("BOOT USART1\r\n");
  MX_SPI2_Init();
  App_BootMarker("BOOT SPI2\r\n");
  MX_USART2_UART_Init();
  App_BootMarker("BOOT USART2\r\n");
  MX_USB_OTG_FS_PCD_Init();
  App_BootMarker("BOOT USB\r\n");
  MX_SPI1_Init();
  App_BootMarker("BOOT SPI1\r\n");
  MX_SPI3_Init();
  App_BootMarker("BOOT SPI3\r\n");
  MX_TIM4_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
	MX_TIM7_Init();
  App_BootMarker("BOOT TIMERS\r\n");
  /* USER CODE BEGIN 2 */
//  DWT_Init();
	/* USART2 接收 ESP32 发来的、以换行结束的命令 */
	HAL_UART_Receive_IT(&huart2, &UART2_RxByte, 1U);
  App_BootMarker("BOOT UART2RX\r\n");

  App_BootMarker("BOOT PRESTART\r\n");
  Start();
  App_BootMarker("BOOT POSTSTART\r\n");
  ContactDetect_Init(&M0_ContactDetector,
					 CONTACT_DETECT_DELTA_CURRENT,
					 CONTACT_DETECT_HOLD_TIME_MS / CONTACT_DETECT_SAMPLE_PERIOD_MS);
  ContactDetect_Init(&M1_ContactDetector,
					 CONTACT_DETECT_DELTA_CURRENT,
					 CONTACT_DETECT_HOLD_TIME_MS / CONTACT_DETECT_SAMPLE_PERIOD_MS);
  printf("BOOT CONTACT TEST\r\n");
#ifdef ESP32_SPI_LINK
  /* 初始化完成后才允许回传 READY；首次装载在主循环等待片选空闲。 */
  App_BootMarker("BOOT SPI3 DUPLEX v2 frame=64\r\n");
#endif

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	App_USART1_CommandTask();
	App_USART2_CommandTask();
#ifdef ESP32_SPI_LINK
	App_SPI3_CommandTask();
#endif
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
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		uint8_t ch = USART1_RxByte;

		if (UART1_FrameReady == 0U)
		{
			if ((ch == '\n') || (ch == '\r'))
			{
				if (UART1_BuildLength > 0U)
				{
					memcpy(UART1_FrameBuffer, UART1_BuildBuffer, UART1_BuildLength);
					UART1_FrameLength = UART1_BuildLength;
					UART1_FrameReady = 1U;
				}
				UART1_BuildLength = 0U;
			}
			else
			{
				if (UART1_BuildLength < (UART1_FRAME_SIZE - 1U))
				{
					UART1_BuildBuffer[UART1_BuildLength++] = ch;
				}
				else
				{
					UART1_BuildLength = 0U;
				}
			}
		}

		HAL_UART_Receive_IT(&huart1, &USART1_RxByte, 1U);
	}
	else if (huart->Instance == USART2)
	{
		uint8_t ch = UART2_RxByte;

		if (UART2_FrameReady == 0U)
		{
			if (ch == '\n')
			{
				if (UART2_BuildLength > 0U)
				{
					memcpy(UART2_FrameBuffer, UART2_BuildBuffer, UART2_BuildLength);
					UART2_FrameLength = UART2_BuildLength;
					UART2_FrameReady = 1U;
				}
				UART2_BuildLength = 0U;
			}
			else if (ch != '\r')
			{
				if (UART2_BuildLength < (UART2_FRAME_SIZE - 1U))
				{
					UART2_BuildBuffer[UART2_BuildLength++] = ch;
				}
				else
				{
					/* 超长帧丢弃，等待下一行重新同步 */
					UART2_BuildLength = 0U;
				}
			}
		}

		/* 继续接收下一个字节 */
		HAL_UART_Receive_IT(&huart2, &UART2_RxByte, 1U);
	}
}

#ifdef ESP32_SPI_LINK
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI3) SPI3_LinkFrameReady = 1U;
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI3) SPI3_LinkError = 1U;
}
#endif

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
		float32_t M1_velocity = M1_GetSpeedAbs();
		
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
	
	/* 按电机分别探测编码器。单电机时，未接的那一路标记为DISABLED，
	 * 不再触发原工程的“报错→LCD→系统复位”死循环。 */
	if ((M0_EncoderType == ENCODER_AS5600) || (M1_EncoderType == ENCODER_AS5600))
	{
		AS5600_Init();
	}
	if (M0_EncoderType == ENCODER_AS5600)
	{
		I2C1_AS5600_GetAngle();
		if (Sensor0.result == AS5600_ERROR)
		{
			M0_EncoderType = ENCODER_DISABLED;
			printf("ENCODER M0 DISABLED\r\n");
		}
	}
	else if (M0_EncoderType == ENCODER_AS5047)
	{
		AS5047P_Init();
	}
	if (M1_EncoderType == ENCODER_AS5600)
	{
		I2C2_AS5600_GetAngle();
		if (Sensor1.result == AS5600_ERROR)
		{
			M1_EncoderType = ENCODER_DISABLED;
			printf("ENCODER M1 DISABLED\r\n");
		}
	}
	else if (M1_EncoderType == ENCODER_AS5047)
	{
		AS5047P_Init();
	}
	/* AS5600 探测失败路径会先关电源并拉刹车；探测结束后恢复正常启动状态，
	 * 只让上面判定为有效的电机参与 FOC。 */
	HAL_GPIO_WritePin(GPIOE, M0_BK_OUT_Pin | M1_BK_OUT_Pin, GPIO_PIN_RESET);
	
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
