#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* STM32/ESP32 task link v3: fixed 64-byte, little-endian, CRC16-CCITT-FALSE. */
#define FLP_SIZE 64U
#define FLP_VERSION 3U
#define FLP_COMMAND 1U
#define FLP_POLL 2U

enum FlpTask : uint8_t {
  FLP_TASK_NONE = 0U,
  FLP_TASK_FEED = 1U,
  FLP_TASK_CLEAN = 2U,
  FLP_TASK_STOP = 3U,
  FLP_TASK_RESET_FAULT = 4U,
  FLP_TASK_PING = 5U,
  FLP_TASK_ARM = 6U,
  /* Accepted only by STM32 Simulation firmware, never through MQTT. */
  FLP_TASK_SIM_FAIL_NEXT = 7U,
  FLP_TASK_SIM_TIMEOUT_NEXT = 8U,
  FLP_TASK_SIM_CLEAR_FAULT = 9U
};

enum FlpResult : uint8_t {
  FLP_RESULT_STATUS = 0U,
  FLP_RESULT_ACCEPTED = 1U,
  FLP_RESULT_PONG = 2U,
  FLP_RESULT_REJECTED = 3U,
  FLP_RESULT_BAD_FRAME = 4U,
  FLP_RESULT_UNSUPPORTED = 5U,
  FLP_RESULT_FAULTED = 6U
};

enum FlpState : uint8_t {
  FLP_STATE_BOOT_SELF_TEST = 0U,
  FLP_STATE_IDLE = 1U,
  FLP_STATE_FEED = 2U,
  FLP_STATE_CLEAN = 3U,
  FLP_STATE_STOPPING = 4U,
  FLP_STATE_FAULT = 5U
};

struct FlpStatus {
  uint32_t id, sample, uptime, faultCode, selfTestPassed, selfTestFailed;
  uint32_t transitions, badFrames, canDropped, transportError;
  uint8_t state, activeTask, lastTask, result, linkFlags;
};

static inline uint16_t flp_crc16(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xffffU;
  while (length-- != 0U) {
    crc ^= (uint16_t)*data++ << 8U;
    for (uint8_t bit = 0U; bit < 8U; ++bit)
      crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
  }
  return crc;
}

static inline void flp_put32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value; data[1] = (uint8_t)(value >> 8U);
  data[2] = (uint8_t)(value >> 16U); data[3] = (uint8_t)(value >> 24U);
}

static inline uint32_t flp_get32(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static inline void flp_finish(uint8_t *frame)
{
  uint16_t crc = flp_crc16(frame, 62U);
  frame[62] = (uint8_t)crc; frame[63] = (uint8_t)(crc >> 8U);
}

static inline bool flp_valid(const uint8_t *frame, uint8_t magic)
{
  return frame[0] == magic && frame[1] == FLP_VERSION &&
         flp_crc16(frame, 62U) == ((uint16_t)frame[62] | ((uint16_t)frame[63] << 8U));
}

static inline bool flp_request(uint8_t *frame, uint8_t kind, uint32_t id,
                               FlpTask task, uint32_t argument0, uint32_t argument1,
                               bool keepalive)
{
  memset(frame, 0, FLP_SIZE);
  if (kind == FLP_COMMAND) {
    if (id == 0U || task > FLP_TASK_SIM_CLEAR_FAULT) return false;
  } else if (kind == FLP_POLL) {
    if (id != 0U || task != FLP_TASK_NONE || argument0 != 0U || argument1 != 0U) return false;
  } else return false;

  frame[0] = 0xa6U; frame[1] = FLP_VERSION; frame[2] = kind;
  frame[3] = (kind == FLP_COMMAND) ? (uint8_t)task : (keepalive ? 1U : 0U);
  flp_put32(frame + 4U, id);
  if (kind == FLP_COMMAND) {
    flp_put32(frame + 8U, argument0);
    flp_put32(frame + 12U, argument1);
  }
  flp_finish(frame);
  return true;
}

static inline bool flp_read_reply(const uint8_t *frame, FlpStatus *status)
{
  if (!flp_valid(frame, 0x5aU) || frame[2] > FLP_STATE_FAULT ||
      frame[3] > FLP_RESULT_FAULTED || frame[8] > FLP_TASK_SIM_CLEAR_FAULT ||
      frame[9] > FLP_TASK_SIM_CLEAR_FAULT)
    return false;
  for (uint8_t i = 49U; i < 62U; ++i) if (frame[i]) return false;
  status->state = frame[2]; status->result = frame[3]; status->id = flp_get32(frame + 4U);
  status->activeTask = frame[8]; status->lastTask = frame[9];
  status->faultCode = flp_get32(frame + 12U); status->selfTestPassed = flp_get32(frame + 16U);
  status->selfTestFailed = flp_get32(frame + 20U); status->uptime = flp_get32(frame + 24U);
  status->transitions = flp_get32(frame + 28U); status->sample = flp_get32(frame + 32U);
  status->badFrames = flp_get32(frame + 36U); status->canDropped = flp_get32(frame + 40U);
  status->transportError = flp_get32(frame + 44U);
  status->linkFlags = frame[48];
  return true;
}
