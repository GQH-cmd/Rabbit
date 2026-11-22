#include "self_test.h"

#include "spi.h"

#include <string.h>

#define SELF_TEST_MAGIC             0x54455354UL /* ASCII: TEST */
volatile SelfTestReport g_self_test_report;

static volatile uint8_t spi_dma_complete;
static volatile uint8_t spi_dma_error;

static void RecordResult(SelfTestState state, uint32_t bit)
{
  if (state == SELF_TEST_PASS)
  {
    g_self_test_report.passed_mask |= bit;
  }
  else
  {
    g_self_test_report.failed_mask |= bit;
  }
}

static SelfTestState TestClock(void)
{
  g_self_test_report.sysclk_hz = HAL_RCC_GetSysClockFreq();
  g_self_test_report.hclk_hz = HAL_RCC_GetHCLKFreq();
  g_self_test_report.pclk1_hz = HAL_RCC_GetPCLK1Freq();
  g_self_test_report.pclk2_hz = HAL_RCC_GetPCLK2Freq();

  if ((g_self_test_report.sysclk_hz == 168000000UL) &&
      (g_self_test_report.hclk_hz == 168000000UL) &&
      (g_self_test_report.pclk1_hz == 42000000UL) &&
      (g_self_test_report.pclk2_hz == 84000000UL))
  {
    return SELF_TEST_PASS;
  }

  return SELF_TEST_FAIL;
}

static SelfTestState TestPumpGpio(void)
{
  const uint32_t mode = (GPIOE->MODER >> (2U * 2U)) & 0x3U;
  const uint32_t output_type = (GPIOE->OTYPER >> 2U) & 0x1U;

  /* Safety: do not pulse the relay. Only verify PE2 is push-pull output. */
  if ((mode == 0x1U) && (output_type == 0U))
  {
    return SELF_TEST_PASS;
  }

  return SELF_TEST_FAIL;
}

static SelfTestState TestLimitSwitchGpio(void)
{
  for (uint32_t pin = 8U; pin <= 13U; ++pin)
  {
    const uint32_t mode = (GPIOE->MODER >> (pin * 2U)) & 0x3U;
    const uint32_t pull = (GPIOE->PUPDR >> (pin * 2U)) & 0x3U;
    if ((mode != 0U) || (pull != 1U))
    {
      return SELF_TEST_FAIL;
    }
  }
  return SELF_TEST_PASS;
}

static SelfTestState TestSpiGpio(void)
{
  const uint32_t pb12_mode = (GPIOB->MODER >> (12U * 2U)) & 0x3U;
  const uint32_t pb13_mode = (GPIOB->MODER >> (13U * 2U)) & 0x3U;
  const uint32_t pb14_mode = (GPIOB->MODER >> (14U * 2U)) & 0x3U;
  const uint32_t pb15_mode = (GPIOB->MODER >> (15U * 2U)) & 0x3U;
  const uint32_t pb13_af = (GPIOB->AFR[1] >> ((13U - 8U) * 4U)) & 0xFU;
  const uint32_t pb14_af = (GPIOB->AFR[1] >> ((14U - 8U) * 4U)) & 0xFU;
  const uint32_t pb15_af = (GPIOB->AFR[1] >> ((15U - 8U) * 4U)) & 0xFU;
  const uint32_t pb12_af = (GPIOB->AFR[1] >> ((12U - 8U) * 4U)) & 0xFU;

  if ((pb12_mode == 0x2U) && (pb12_af == GPIO_AF5_SPI2) &&
      (pb13_mode == 0x2U) && (pb14_mode == 0x2U) && (pb15_mode == 0x2U) &&
      (pb13_af == GPIO_AF5_SPI2) &&
      (pb14_af == GPIO_AF5_SPI2) &&
      (pb15_af == GPIO_AF5_SPI2))
  {
    return SELF_TEST_PASS;
  }

  return SELF_TEST_FAIL;
}

static SelfTestState TestSpiPollingLoopback(void)
{
  /* SPI slave transfers require an external master and are not self-testable. */
  return SELF_TEST_NOT_RUN;
}

static SelfTestState TestSpiDmaLoopback(void)
{
  /* SPI slave DMA is verified with an ESP32 master, not an STM32 loopback. */
  return SELF_TEST_NOT_RUN;
}

void SelfTest_Run(void)
{
  memset((void *)&g_self_test_report, 0, sizeof(g_self_test_report));
  g_self_test_report.magic = SELF_TEST_MAGIC;

  g_self_test_report.clock = TestClock();
  RecordResult(g_self_test_report.clock, SELF_TEST_CLOCK_BIT);

  g_self_test_report.pump_gpio = TestPumpGpio();
  RecordResult(g_self_test_report.pump_gpio, SELF_TEST_PUMP_GPIO_BIT);

  g_self_test_report.limit_switch_gpio = TestLimitSwitchGpio();
  RecordResult(g_self_test_report.limit_switch_gpio, SELF_TEST_LIMIT_GPIO_BIT);

  /* The motor UART and its protocol will be added separately. */
  g_self_test_report.motor_link = SELF_TEST_NOT_RUN;

  g_self_test_report.spi_gpio = TestSpiGpio();
  RecordResult(g_self_test_report.spi_gpio, SELF_TEST_SPI_GPIO_BIT);

  g_self_test_report.spi_polling_loopback = TestSpiPollingLoopback();
  g_self_test_report.spi_dma_loopback = TestSpiDmaLoopback();
  g_self_test_report.finished = 1U;
}

void SelfTest_OnSpiTxRxComplete(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI2)
  {
    spi_dma_complete = 1U;
  }
}

void SelfTest_OnSpiError(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI2)
  {
    spi_dma_error = 1U;
  }
}
