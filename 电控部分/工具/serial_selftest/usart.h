#ifndef SERIAL_SELFTEST_USART_H
#define SERIAL_SELFTEST_USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern UART_HandleTypeDef huart1;

void MX_USART1_UART_Init(void);
void USART1_Write(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
