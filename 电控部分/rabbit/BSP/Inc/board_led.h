#ifndef BOARD_LED_H
#define BOARD_LED_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The user LED is connected from 3V3 to PA1 through a series resistor. */
#define BOARD_LED_ACTIVE_HIGH 0

void BoardLed_Init(void);
void BoardLed_On(void);
void BoardLed_Off(void);
void BoardLed_Set(bool enabled);
void BoardLed_Toggle(void);
bool BoardLed_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_LED_H */
