#ifndef ESP32_LINK_PROTOCOL_H
#define ESP32_LINK_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ESP32_LINK_FRAME_SIZE 64U
#define ESP32_LINK_VERSION 3U
#define ESP32_LINK_COMMAND 1U
#define ESP32_LINK_POLL 2U
#define ESP32_LINK_POLL_KEEPALIVE 1U
#define ESP32_LINK_STATUS_SIMULATION (1U << 0)

typedef enum
{
  ESP32_LINK_TASK_NONE = 0U,
  ESP32_LINK_TASK_FEED = 1U,
  ESP32_LINK_TASK_CLEAN = 2U,
  ESP32_LINK_TASK_STOP = 3U,
  ESP32_LINK_TASK_RESET_FAULT = 4U,
  ESP32_LINK_TASK_PING = 5U,
  ESP32_LINK_TASK_ARM = 6U,
  /* Local simulation commands. Production firmware always rejects these. */
  ESP32_LINK_TASK_SIM_FAIL_NEXT = 7U,
  ESP32_LINK_TASK_SIM_TIMEOUT_NEXT = 8U,
  ESP32_LINK_TASK_SIM_CLEAR_FAULT = 9U
} Esp32LinkTask;

typedef enum
{
  ESP32_LINK_RESULT_STATUS = 0U,
  ESP32_LINK_RESULT_ACCEPTED = 1U,
  ESP32_LINK_RESULT_PONG = 2U,
  ESP32_LINK_RESULT_REJECTED = 3U,
  ESP32_LINK_RESULT_BAD_FRAME = 4U,
  ESP32_LINK_RESULT_UNSUPPORTED = 5U,
  ESP32_LINK_RESULT_FAULTED = 6U
} Esp32LinkResult;

typedef struct
{
  uint32_t id;
  uint32_t argument0;
  uint32_t argument1;
  Esp32LinkTask task;
  bool keepalive;
  bool is_poll;
} Esp32LinkRequest;

typedef struct
{
  uint32_t id;
  uint32_t sample;
  uint32_t uptime_ms;
  uint32_t fault_code;
  uint32_t self_test_passed_mask;
  uint32_t self_test_failed_mask;
  uint32_t transition_count;
  uint32_t bad_frames;
  uint32_t can_dropped_frames;
  uint32_t transport_error;
  uint8_t link_flags;
  uint8_t state;
  Esp32LinkTask active_task;
  Esp32LinkTask last_task;
  Esp32LinkResult result;
} Esp32LinkStatus;

static inline uint16_t Esp32Link_Crc16(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xffffU;
  while (length-- != 0U) {
    crc ^= (uint16_t)*data++ << 8U;
    for (uint8_t bit = 0U; bit < 8U; ++bit)
      crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
  }
  return crc;
}

static inline uint32_t Esp32Link_Get32(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static inline void Esp32Link_Put32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value; data[1] = (uint8_t)(value >> 8U);
  data[2] = (uint8_t)(value >> 16U); data[3] = (uint8_t)(value >> 24U);
}

static inline bool Esp32Link_Valid(const uint8_t *frame, uint8_t magic)
{
  uint16_t expected = (uint16_t)frame[62] | ((uint16_t)frame[63] << 8U);
  return frame[0] == magic && frame[1] == ESP32_LINK_VERSION &&
         Esp32Link_Crc16(frame, 62U) == expected;
}

static inline bool Esp32Link_ReadRequest(const uint8_t *frame, Esp32LinkRequest *request)
{
  if (!Esp32Link_Valid(frame, 0xa6U)) return false;
  memset(request, 0, sizeof(*request));
  request->id = Esp32Link_Get32(frame + 4U);
  if (frame[2] == ESP32_LINK_COMMAND) {
    if (!request->id || frame[3] > ESP32_LINK_TASK_SIM_CLEAR_FAULT) return false;
    request->task = (Esp32LinkTask)frame[3];
    request->argument0 = Esp32Link_Get32(frame + 8U);
    request->argument1 = Esp32Link_Get32(frame + 12U);
  } else if (frame[2] == ESP32_LINK_POLL && !request->id && frame[3] <= ESP32_LINK_POLL_KEEPALIVE) {
    request->is_poll = true;
    request->keepalive = frame[3] == ESP32_LINK_POLL_KEEPALIVE;
  } else return false;
  for (uint8_t i = 16U; i < 62U; ++i) if (frame[i]) return false;
  return true;
}

static inline void Esp32Link_MakeReply(uint8_t *frame, const Esp32LinkStatus *status)
{
  memset(frame, 0, ESP32_LINK_FRAME_SIZE); frame[0] = 0x5aU; frame[1] = ESP32_LINK_VERSION;
  frame[2] = status->state; frame[3] = (uint8_t)status->result;
  Esp32Link_Put32(frame + 4U, status->id);
  frame[8] = (uint8_t)status->active_task; frame[9] = (uint8_t)status->last_task;
  Esp32Link_Put32(frame + 12U, status->fault_code);
  Esp32Link_Put32(frame + 16U, status->self_test_passed_mask);
  Esp32Link_Put32(frame + 20U, status->self_test_failed_mask);
  Esp32Link_Put32(frame + 24U, status->uptime_ms);
  Esp32Link_Put32(frame + 28U, status->transition_count);
  Esp32Link_Put32(frame + 32U, status->sample);
  Esp32Link_Put32(frame + 36U, status->bad_frames);
  Esp32Link_Put32(frame + 40U, status->can_dropped_frames);
  Esp32Link_Put32(frame + 44U, status->transport_error);
  frame[48] = status->link_flags;
  uint16_t crc = Esp32Link_Crc16(frame, 62U); frame[62] = (uint8_t)crc; frame[63] = (uint8_t)(crc >> 8U);
}

#endif
