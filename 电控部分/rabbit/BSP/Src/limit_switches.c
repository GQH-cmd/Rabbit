#include "limit_switches.h"

#include "main.h"

#define LIMIT_SWITCH_DEBOUNCE_MS 20U
#define LIMIT_SWITCH_FIRST_PIN   8U

static uint8_t s_raw_mask;
static uint8_t s_candidate_mask;
static uint8_t s_stable_mask;
static uint32_t s_candidate_since_ms;

static uint8_t LimitSwitches_ReadHardware(void)
{
  /* PE8..PE13 are contiguous. Inputs use pull-ups and the normally-open
   * contacts close to GND, so invert the GPIO levels to obtain active-high
   * switch bits. */
  const uint32_t levels = (GPIOE->IDR >> LIMIT_SWITCH_FIRST_PIN) & LIMIT_SWITCH_MASK_ALL;
  return (uint8_t)((~levels) & LIMIT_SWITCH_MASK_ALL);
}

void LimitSwitches_Init(void)
{
  s_raw_mask = LimitSwitches_ReadHardware();
  s_candidate_mask = s_raw_mask;
  s_stable_mask = s_raw_mask;
  s_candidate_since_ms = HAL_GetTick();
}

void LimitSwitches_Process(uint32_t now_ms)
{
  s_raw_mask = LimitSwitches_ReadHardware();

  if (s_raw_mask != s_candidate_mask)
  {
    s_candidate_mask = s_raw_mask;
    s_candidate_since_ms = now_ms;
  }
  else if ((s_stable_mask != s_candidate_mask) &&
           ((now_ms - s_candidate_since_ms) >= LIMIT_SWITCH_DEBOUNCE_MS))
  {
    s_stable_mask = s_candidate_mask;
  }
}

uint8_t LimitSwitches_GetRawMask(void)
{
  return s_raw_mask;
}

uint8_t LimitSwitches_GetStableMask(void)
{
  return s_stable_mask;
}

bool LimitSwitches_IsActive(LimitSwitchId id)
{
  return ((uint32_t)id < LIMIT_SWITCH_COUNT) &&
         ((s_stable_mask & (1U << (uint32_t)id)) != 0U);
}
