#ifndef ESP32_COMM_H
#define ESP32_COMM_H

#include "main.h"
#include "esp32_link_protocol.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_COMM_FRAME_SIZE ESP32_LINK_FRAME_SIZE

typedef enum
{
  ESP32_COMM_STATE_IDLE = 0U,
  ESP32_COMM_STATE_BUSY,
  ESP32_COMM_STATE_COMPLETE,
  ESP32_COMM_STATE_TIMEOUT,
  ESP32_COMM_STATE_ERROR
} Esp32CommState;

typedef enum
{
  ESP32_COMM_OK = 0U,
  ESP32_COMM_BUSY,
  ESP32_COMM_TIMEOUT,
  ESP32_COMM_ERROR,
  ESP32_COMM_INVALID_ARGUMENT
} Esp32CommResult;

void Esp32Comm_Init(void);
/*
 * SPI2 is an STM32 slave. ESP32 owns NSS and the SPI clock, so the STM32
 * cannot initiate a transfer. Each transaction is exactly FRAME_SIZE bytes.
 */
Esp32CommResult Esp32Comm_SetTxFrame(const uint8_t *frame, uint16_t length);
bool Esp32Comm_RxFrameAvailable(void);
const uint8_t *Esp32Comm_GetRxFrame(void);
void Esp32Comm_ConsumeRxFrame(void);
void Esp32Comm_Process(void);
void Esp32Comm_ClearResult(void);
Esp32CommState Esp32Comm_GetState(void);
uint32_t Esp32Comm_GetLastError(void);
const uint8_t *Esp32Comm_GetTxBuffer(void);

/* Called only by the project's centralized HAL callback dispatcher. */
void Esp32Comm_OnTxRxComplete(SPI_HandleTypeDef *hspi);
void Esp32Comm_OnError(SPI_HandleTypeDef *hspi);

#ifdef __cplusplus
}
#endif

#endif /* ESP32_COMM_H */
