#ifndef PUMP_H
#define PUMP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set to 0 at compile time for an active-low relay input. */
#ifndef PUMP_ACTIVE_HIGH
#define PUMP_ACTIVE_HIGH 1
#endif

#ifndef PUMP_LED_MIRROR
#define PUMP_LED_MIRROR 1
#endif

void Pump_Init(void);
void Pump_On(void);
void Pump_Off(void);
void Pump_Set(bool enabled);
bool Pump_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* PUMP_H */
