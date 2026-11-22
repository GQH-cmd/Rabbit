#include "motor_can.h"

#include "can.h"

#include <string.h>

#define MOTOR_CAN_RX_QUEUE_LENGTH 32U
#define MOTOR_CAN_FILTER_BANK      0U
#define MOTOR_CAN_NOTIFICATION_MASK (CAN_IT_RX_FIFO0_MSG_PENDING | \
                                     CAN_IT_ERROR_WARNING | \
                                     CAN_IT_ERROR_PASSIVE | \
                                     CAN_IT_BUSOFF | \
                                     CAN_IT_LAST_ERROR_CODE | \
                                     CAN_IT_ERROR)

static CAN_HandleTypeDef *s_hcan;
static MotorCanFrame s_rx_queue[MOTOR_CAN_RX_QUEUE_LENGTH];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static volatile uint32_t s_rx_dropped;
static volatile uint32_t s_last_error;
static volatile bool s_started;

static uint16_t MotorCan_NextIndex(uint16_t index)
{
  index++;
  return (index >= MOTOR_CAN_RX_QUEUE_LENGTH) ? 0U : index;
}

static MotorCanStatus MotorCan_MapHalStatus(HAL_StatusTypeDef status)
{
  if (status == HAL_OK)
  {
    return MOTOR_CAN_OK;
  }
  if (status == HAL_BUSY)
  {
    return MOTOR_CAN_BUSY;
  }
  return MOTOR_CAN_ERROR;
}

MotorCanStatus MotorCan_Init(void)
{
  CAN_FilterTypeDef filter = {0};
  HAL_StatusTypeDef status;

  s_hcan = &hcan1;
  s_rx_head = 0U;
  s_rx_tail = 0U;
  s_rx_dropped = 0U;
  s_last_error = HAL_CAN_ERROR_NONE;
  s_started = false;

  filter.FilterBank = MOTOR_CAN_FILTER_BANK;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh = 0U;
  filter.FilterIdLow = 0U;
  filter.FilterMaskIdHigh = 0U;
  filter.FilterMaskIdLow = 0U;
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = 14U;

  status = HAL_CAN_ConfigFilter(s_hcan, &filter);
  if (status != HAL_OK)
  {
    s_last_error = HAL_CAN_GetError(s_hcan);
    return MotorCan_MapHalStatus(status);
  }

  status = HAL_CAN_Start(s_hcan);
  if (status != HAL_OK)
  {
    s_last_error = HAL_CAN_GetError(s_hcan);
    return MotorCan_MapHalStatus(status);
  }

  s_started = true;
  status = HAL_CAN_ActivateNotification(s_hcan, MOTOR_CAN_NOTIFICATION_MASK);
  if (status != HAL_OK)
  {
    s_last_error = HAL_CAN_GetError(s_hcan);
    s_started = false;
    (void)HAL_CAN_Stop(s_hcan);
    return MotorCan_MapHalStatus(status);
  }

  return MOTOR_CAN_OK;
}

MotorCanStatus MotorCan_DeInit(void)
{
  HAL_StatusTypeDef status;

  if (s_hcan == NULL)
  {
    return MOTOR_CAN_NOT_INITIALIZED;
  }

  (void)HAL_CAN_DeactivateNotification(s_hcan, MOTOR_CAN_NOTIFICATION_MASK);
  status = HAL_CAN_Stop(s_hcan);
  s_started = false;
  return MotorCan_MapHalStatus(status);
}

MotorCanStatus MotorCan_Send(const MotorCanFrame *frame, uint32_t *mailbox)
{
  CAN_TxHeaderTypeDef header = {0};
  uint8_t empty_data[MOTOR_CAN_MAX_DATA_LENGTH] = {0};
  uint32_t local_mailbox;
  HAL_StatusTypeDef status;

  if (frame == NULL)
  {
    return MOTOR_CAN_INVALID_ARGUMENT;
  }
  if (!s_started || (s_hcan == NULL))
  {
    return MOTOR_CAN_NOT_INITIALIZED;
  }
  if ((frame->dlc > MOTOR_CAN_MAX_DATA_LENGTH) ||
      ((frame->id_type != MOTOR_CAN_ID_STANDARD) &&
       (frame->id_type != MOTOR_CAN_ID_EXTENDED)) ||
      ((frame->frame_type != MOTOR_CAN_FRAME_DATA) &&
       (frame->frame_type != MOTOR_CAN_FRAME_REMOTE)) ||
      ((frame->id_type == MOTOR_CAN_ID_STANDARD) && (frame->id > 0x7FFU)) ||
      ((frame->id_type == MOTOR_CAN_ID_EXTENDED) && (frame->id > 0x1FFFFFFFU)))
  {
    return MOTOR_CAN_INVALID_ARGUMENT;
  }

  header.StdId = (frame->id_type == MOTOR_CAN_ID_STANDARD) ? frame->id : 0U;
  header.ExtId = (frame->id_type == MOTOR_CAN_ID_EXTENDED) ? frame->id : 0U;
  header.IDE = (frame->id_type == MOTOR_CAN_ID_EXTENDED) ? CAN_ID_EXT : CAN_ID_STD;
  header.RTR = (frame->frame_type == MOTOR_CAN_FRAME_REMOTE) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  header.DLC = frame->dlc;
  header.TransmitGlobalTime = DISABLE;

  status = HAL_CAN_AddTxMessage(s_hcan, &header,
                                (uint8_t *)((frame->frame_type == MOTOR_CAN_FRAME_DATA) ?
                                            frame->data : empty_data),
                                (mailbox != NULL) ? mailbox : &local_mailbox);
  if (status != HAL_OK)
  {
    s_last_error = HAL_CAN_GetError(s_hcan);
  }
  return MotorCan_MapHalStatus(status);
}

MotorCanStatus MotorCan_SendTo(const MotorCanEndpoint *endpoint,
                               const uint8_t *data,
                               uint8_t length,
                               uint32_t *mailbox)
{
  MotorCanFrame frame = {0};

  if ((endpoint == NULL) || (length > MOTOR_CAN_MAX_DATA_LENGTH) ||
      ((data == NULL) && (length != 0U)))
  {
    return MOTOR_CAN_INVALID_ARGUMENT;
  }

  frame.id = endpoint->tx_id;
  frame.id_type = endpoint->id_type;
  frame.frame_type = MOTOR_CAN_FRAME_DATA;
  frame.dlc = length;
  if (length != 0U)
  {
    memcpy(frame.data, data, length);
  }

  return MotorCan_Send(&frame, mailbox);
}

MotorCanStatus MotorCan_RxPop(MotorCanFrame *frame)
{
  uint16_t tail;

  if (frame == NULL)
  {
    return MOTOR_CAN_INVALID_ARGUMENT;
  }

  tail = s_rx_tail;
  if (tail == s_rx_head)
  {
    return MOTOR_CAN_RX_EMPTY;
  }

  *frame = s_rx_queue[tail];
  __DMB();
  s_rx_tail = MotorCan_NextIndex(tail);
  return MOTOR_CAN_OK;
}

uint16_t MotorCan_RxPending(void)
{
  const uint16_t head = s_rx_head;
  const uint16_t tail = s_rx_tail;

  return (head >= tail) ? (head - tail) : (MOTOR_CAN_RX_QUEUE_LENGTH - tail + head);
}

uint32_t MotorCan_GetDroppedCount(void)
{
  return s_rx_dropped;
}

uint32_t MotorCan_GetLastError(void)
{
  return s_last_error;
}

bool MotorCan_IsStarted(void)
{
  return s_started;
}

void MotorCan_OnRxFifo0Pending(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef header;
  MotorCanFrame frame;
  uint16_t head;
  uint16_t next;

  if ((hcan != s_hcan) || !s_started)
  {
    return;
  }

  while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) != 0U)
  {
    memset(&frame, 0, sizeof(frame));
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, frame.data) != HAL_OK)
    {
      s_last_error = HAL_CAN_GetError(hcan);
      return;
    }

    frame.id_type = (header.IDE == CAN_ID_EXT) ? MOTOR_CAN_ID_EXTENDED : MOTOR_CAN_ID_STANDARD;
    frame.id = (header.IDE == CAN_ID_EXT) ? header.ExtId : header.StdId;
    frame.frame_type = (header.RTR == CAN_RTR_REMOTE) ? MOTOR_CAN_FRAME_REMOTE : MOTOR_CAN_FRAME_DATA;
    frame.dlc = (header.DLC <= MOTOR_CAN_MAX_DATA_LENGTH) ?
                (uint8_t)header.DLC : MOTOR_CAN_MAX_DATA_LENGTH;
    frame.timestamp_ms = HAL_GetTick();

    head = s_rx_head;
    next = MotorCan_NextIndex(head);
    if (next == s_rx_tail)
    {
      s_rx_dropped++;
      continue;
    }

    s_rx_queue[head] = frame;
    __DMB();
    s_rx_head = next;
  }
}

void MotorCan_OnError(CAN_HandleTypeDef *hcan)
{
  if (hcan == s_hcan)
  {
    s_last_error = HAL_CAN_GetError(hcan);
  }
}
