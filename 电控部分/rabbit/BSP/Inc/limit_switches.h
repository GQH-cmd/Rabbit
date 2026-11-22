#ifndef LIMIT_SWITCHES_H
#define LIMIT_SWITCHES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bit order for the six-bit mechanism position word. A set bit means the
 * corresponding normally-open switch is pressed/closed. */
typedef enum
{
  LIMIT_SWITCH_LEFT = 0U,
  LIMIT_SWITCH_RIGHT,
  LIMIT_SWITCH_LEFT_UP,
  LIMIT_SWITCH_LEFT_DOWN,
  LIMIT_SWITCH_RIGHT_UP,
  LIMIT_SWITCH_RIGHT_DOWN,
  LIMIT_SWITCH_COUNT
} LimitSwitchId;

#define LIMIT_SWITCH_MASK_ALL ((1U << LIMIT_SWITCH_COUNT) - 1U)

void LimitSwitches_Init(void);
void LimitSwitches_Process(uint32_t now_ms);
uint8_t LimitSwitches_GetRawMask(void);
uint8_t LimitSwitches_GetStableMask(void);
bool LimitSwitches_IsActive(LimitSwitchId id);

#ifdef __cplusplus
}
#endif

#endif /* LIMIT_SWITCHES_H */
