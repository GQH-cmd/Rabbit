#ifndef SELF_TEST_H
#define SELF_TEST_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  SELF_TEST_NOT_RUN = 0U,
  SELF_TEST_PASS = 1U,
  SELF_TEST_FAIL = 2U
} SelfTestState;

enum
{
  SELF_TEST_CLOCK_BIT       = (1UL << 0),
  SELF_TEST_PUMP_GPIO_BIT   = (1UL << 1),
  SELF_TEST_LIMIT_GPIO_BIT  = (1UL << 2),
  SELF_TEST_MOTOR_LINK_BIT  = (1UL << 3),
  SELF_TEST_SPI_GPIO_BIT    = (1UL << 4),
  SELF_TEST_SPI_POLLING_BIT = (1UL << 5),
  SELF_TEST_SPI_DMA_BIT     = (1UL << 6)
};

typedef struct
{
  uint32_t magic;
  uint32_t finished;
  uint32_t passed_mask;
  uint32_t failed_mask;

  SelfTestState clock;
  SelfTestState pump_gpio;
  SelfTestState limit_switch_gpio;
  SelfTestState motor_link;
  SelfTestState spi_gpio;
  SelfTestState spi_polling_loopback;
  SelfTestState spi_dma_loopback;

  uint32_t sysclk_hz;
  uint32_t hclk_hz;
  uint32_t pclk1_hz;
  uint32_t pclk2_hz;
  uint32_t motor_link_error;
  uint32_t spi_error;

  uint8_t motor_link_tx[8];
  uint8_t motor_link_rx[8];
  uint8_t spi_polling_tx[8];
  uint8_t spi_polling_rx[8];
  uint8_t spi_dma_tx[8];
  uint8_t spi_dma_rx[8];
} SelfTestReport;

/* Add g_self_test_report to the debugger Watch/Expressions window. */
extern volatile SelfTestReport g_self_test_report;

void SelfTest_Run(void);
void SelfTest_OnSpiTxRxComplete(SPI_HandleTypeDef *hspi);
void SelfTest_OnSpiError(SPI_HandleTypeDef *hspi);

#ifdef __cplusplus
}
#endif

#endif /* SELF_TEST_H */
