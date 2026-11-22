#include "esp32_link.h"

#include "main.h"

static uint32_t s_sample;
static uint32_t s_bad_frames;
static Esp32LinkRequestHandler s_handler;

void Esp32Link_Init(void)
{
  s_sample = 0U;
  s_bad_frames = 0U;
  s_handler = NULL;
}

void Esp32Link_SetRequestHandler(Esp32LinkRequestHandler handler)
{
  s_handler = handler;
}

bool Esp32Link_ProcessFrame(const uint8_t *rx_frame, uint8_t *tx_frame)
{
  Esp32LinkRequest request;
  Esp32LinkStatus reply = {0};
  reply.result = ESP32_LINK_RESULT_BAD_FRAME;
  reply.sample = ++s_sample;
  reply.uptime_ms = HAL_GetTick();
  reply.bad_frames = s_bad_frames;

  if (!Esp32Link_ReadRequest(rx_frame, &request)) {
    ++s_bad_frames;
    reply.bad_frames = s_bad_frames;
    Esp32Link_MakeReply(tx_frame, &reply);
    return false;
  }

  reply.id = request.is_poll ? 0U : request.id;
  reply.result = ESP32_LINK_RESULT_REJECTED;
  if (s_handler != NULL) s_handler(&request, &reply);
  else reply.result = ESP32_LINK_RESULT_UNSUPPORTED;
  reply.bad_frames = s_bad_frames;
  reply.sample = s_sample;
  reply.uptime_ms = HAL_GetTick();
  Esp32Link_MakeReply(tx_frame, &reply);
  return true;
}

uint32_t Esp32Link_GetSample(void) { return s_sample; }
uint32_t Esp32Link_GetBadFrameCount(void) { return s_bad_frames; }
