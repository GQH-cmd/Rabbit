#include "esp32_comm.h"

#include "spi.h"

#include <stdbool.h>
#include <string.h>

static SPI_HandleTypeDef *s_hspi;
static uint8_t s_tx_buffer[ESP32_COMM_FRAME_SIZE];
static uint8_t s_rx_buffer[ESP32_COMM_FRAME_SIZE];
static volatile Esp32CommState s_state = ESP32_COMM_STATE_IDLE;
static volatile uint32_t s_last_error;
static volatile uint32_t s_start_tick;
static volatile bool s_rx_frame_available;
static volatile bool s_nss_active;

#define ESP32_COMM_TRANSACTION_TIMEOUT_MS 100U

static Esp32CommResult Esp32Comm_ArmDma(void)
{
  const HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_DMA(
      s_hspi, s_tx_buffer, s_rx_buffer, ESP32_COMM_FRAME_SIZE);

  if (status == HAL_OK)
  {
    s_start_tick = 0U;
    s_nss_active = false;
    s_state = ESP32_COMM_STATE_BUSY;
    return ESP32_COMM_OK;
  }

  s_last_error = HAL_SPI_GetError(s_hspi);
  s_state = ESP32_COMM_STATE_ERROR;
  return (status == HAL_BUSY) ? ESP32_COMM_BUSY : ESP32_COMM_ERROR;
}

void Esp32Comm_Init(void)
{
  s_hspi = &hspi2;
  s_state = ESP32_COMM_STATE_IDLE;
  s_last_error = HAL_SPI_ERROR_NONE;
  s_start_tick = 0U;
  s_rx_frame_available = false;
  s_nss_active = false;
  memset(s_tx_buffer, 0xFF, sizeof(s_tx_buffer));
  memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
  (void)Esp32Comm_ArmDma();
}

Esp32CommResult Esp32Comm_SetTxFrame(const uint8_t *frame, uint16_t length)
{
  if ((frame == NULL) || (length != ESP32_COMM_FRAME_SIZE))
  {
    return ESP32_COMM_INVALID_ARGUMENT;
  }
  if (s_state == ESP32_COMM_STATE_BUSY)
  {
    return ESP32_COMM_BUSY;
  }
  memcpy(s_tx_buffer, frame, ESP32_COMM_FRAME_SIZE);
  __DMB();
  return ESP32_COMM_OK;
}

bool Esp32Comm_RxFrameAvailable(void)
{
  return s_rx_frame_available;
}

const uint8_t *Esp32Comm_GetRxFrame(void)
{
  __DMB();
  return s_rx_buffer;
}

void Esp32Comm_ConsumeRxFrame(void)
{
  s_rx_frame_available = false;
}

void Esp32Comm_Process(void)
{
  if (s_state == ESP32_COMM_STATE_BUSY)
  {
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET)
    {
      if (!s_nss_active)
      {
        s_nss_active = true;
        s_start_tick = HAL_GetTick();
      }
      else if ((HAL_GetTick() - s_start_tick) >= ESP32_COMM_TRANSACTION_TIMEOUT_MS)
      {
        (void)HAL_SPI_Abort(s_hspi);
        s_last_error = HAL_SPI_GetError(s_hspi);
        s_state = ESP32_COMM_STATE_TIMEOUT;
      }
    }
    else
    {
      s_nss_active = false;
    }
  }

  if ((s_state != ESP32_COMM_STATE_BUSY) && !s_rx_frame_available)
  {
    (void)Esp32Comm_ArmDma();
  }
}

void Esp32Comm_ClearResult(void)
{
  if (s_state != ESP32_COMM_STATE_BUSY)
  {
    s_state = ESP32_COMM_STATE_IDLE;
  }
}

Esp32CommState Esp32Comm_GetState(void)
{
  return s_state;
}

uint32_t Esp32Comm_GetLastError(void)
{
  return s_last_error;
}

const uint8_t *Esp32Comm_GetTxBuffer(void)
{
  return s_tx_buffer;
}

void Esp32Comm_OnTxRxComplete(SPI_HandleTypeDef *hspi)
{
  if ((hspi == s_hspi) && (s_state == ESP32_COMM_STATE_BUSY))
  {
    s_last_error = HAL_SPI_GetError(hspi);
    __DMB();
    s_rx_frame_available = true;
    s_nss_active = false;
    s_state = ESP32_COMM_STATE_COMPLETE;
  }
}

void Esp32Comm_OnError(SPI_HandleTypeDef *hspi)
{
  if ((hspi == s_hspi) && (s_state == ESP32_COMM_STATE_BUSY))
  {
    s_last_error = HAL_SPI_GetError(hspi);
    s_nss_active = false;
    s_state = ESP32_COMM_STATE_ERROR;
  }
}
