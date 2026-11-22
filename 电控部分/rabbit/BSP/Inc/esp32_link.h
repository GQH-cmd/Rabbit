#ifndef ESP32_LINK_H
#define ESP32_LINK_H

#include "esp32_link_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

void Esp32Link_Init(void);
typedef void (*Esp32LinkRequestHandler)(const Esp32LinkRequest *request,
                                        Esp32LinkStatus *status);
void Esp32Link_SetRequestHandler(Esp32LinkRequestHandler handler);
bool Esp32Link_ProcessFrame(const uint8_t *rx_frame, uint8_t *tx_frame);
uint32_t Esp32Link_GetSample(void);
uint32_t Esp32Link_GetBadFrameCount(void);

#ifdef __cplusplus
}
#endif

#endif
