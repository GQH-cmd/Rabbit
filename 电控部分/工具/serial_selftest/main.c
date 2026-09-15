#include <string.h>

#include "gpio.h"
#include "main.h"
#include "usart.h"

static void banner(void)
{
  static const char msg[] = "SELFTEST OK\r\n";
  USART1_Write((const uint8_t *)msg, (uint16_t)(sizeof(msg) - 1));
}

int main(void)
{
  uint32_t last_blink = 0;
  uint32_t last_banner = 0;
  uint8_t ch = 0;

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  banner();

  while (1)
  {
    uint32_t now = HAL_GetTick();

    if ((now - last_blink) >= 250U)
    {
      HAL_GPIO_TogglePin(TEST_LED_GPIO_Port, TEST_LED_Pin);
      last_blink = now;
    }

    if ((now - last_banner) >= 1000U)
    {
      banner();
      last_banner = now;
    }

    if (HAL_UART_Receive(&huart1, &ch, 1, 10) == HAL_OK)
    {
      USART1_Write(&ch, 1);
      if (ch == '\r')
      {
        static const uint8_t lf = '\n';
        USART1_Write(&lf, 1);
      }
    }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

void SysTick_Handler(void)
{
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(TEST_LED_GPIO_Port, TEST_LED_Pin);
    for (volatile uint32_t i = 0; i < 200000U; ++i)
    {
    }
  }
}
