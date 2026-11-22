#ifndef MOTOR_CAN_H
#define MOTOR_CAN_H

#include "main.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTOR_CAN_MAX_DATA_LENGTH 8U

typedef enum
{
  MOTOR_CAN_ID_STANDARD = 0U,
  MOTOR_CAN_ID_EXTENDED = 1U
} MotorCanIdType;

typedef enum
{
  MOTOR_CAN_FRAME_DATA = 0U,
  MOTOR_CAN_FRAME_REMOTE = 1U
} MotorCanFrameType;

typedef enum
{
  MOTOR_CAN_OK = 0U,
  MOTOR_CAN_ERROR,
  MOTOR_CAN_BUSY,
  MOTOR_CAN_INVALID_ARGUMENT,
  MOTOR_CAN_NOT_INITIALIZED,
  MOTOR_CAN_RX_EMPTY
} MotorCanStatus;

typedef struct
{
  uint32_t id;
  MotorCanIdType id_type;
  MotorCanFrameType frame_type;
  uint8_t dlc;
  uint8_t data[MOTOR_CAN_MAX_DATA_LENGTH];
  uint32_t timestamp_ms;
} MotorCanFrame;

/* One endpoint can describe one motor; applications may define two or more. */
typedef struct
{
  uint32_t tx_id;
  uint32_t rx_id;
  MotorCanIdType id_type;
} MotorCanEndpoint;

MotorCanStatus MotorCan_Init(void);
MotorCanStatus MotorCan_DeInit(void);
MotorCanStatus MotorCan_Send(const MotorCanFrame *frame, uint32_t *mailbox);
MotorCanStatus MotorCan_SendTo(const MotorCanEndpoint *endpoint,
                               const uint8_t *data,
                               uint8_t length,
                               uint32_t *mailbox);
MotorCanStatus MotorCan_RxPop(MotorCanFrame *frame);
uint16_t MotorCan_RxPending(void);
uint32_t MotorCan_GetDroppedCount(void);
uint32_t MotorCan_GetLastError(void);
bool MotorCan_IsStarted(void);

/* Called only by the project's centralized HAL callback dispatcher. */
void MotorCan_OnRxFifo0Pending(CAN_HandleTypeDef *hcan);
void MotorCan_OnError(CAN_HandleTypeDef *hcan);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CAN_H */
