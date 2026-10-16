#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef float float32_t;
#define PI 3.14159265358979323846f
#define _2PI (2*PI)
#include "FocLinkProtocol.h"
#include "recovery_types.h"
#define ENCODER_AS5600 0
#define ENCODER_DISABLED 2
#define AS5600_OK 0
#define AS5600_ERROR 1
#define RAW_ANGLE_H 0x0c
#define AS5600_SLAVE_ADDRESS (0x36<<1)
#define I2C_MEMADD_SIZE_8BIT 1
#define GPIOE 0
#define MOTOR_EN1_Pin 1
#define MOTOR_EN2_Pin 2
#define M0_BK_OUT_Pin 4
#define M1_BK_OUT_Pin 8
#define TEST_LED_Pin 16
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define I2C1 1
#define I2C2 2
#define HAL_OK 0
#define HAL_ERROR 1
typedef int HAL_StatusTypeDef;
typedef struct {int Instance;} I2C_HandleTypeDef;
typedef struct {int unused;} TIM_HandleTypeDef;
I2C_HandleTypeDef hi2c1={I2C1},hi2c2={I2C2};
TIM_HandleTypeDef htim8;
AS5600 Sensor0,Sensor1;
MotorControl M0,M1;
Motor M1_Motor;
CURRENT_s M0_Curs,M1_Curs;
VELOCITY_s M0_Vels,M1_Vels;
PIDController M1_iq_pid,M1_id_pid,M1_vel_pid,M1_pos_pid;
uint8_t M0_EncoderType=2,M1_EncoderType;
volatile uint8_t AS5600_ReadFaults,FOC_VelocityFault,FOC_M1CalibrationValid,FOC_M1RecoveryHold;
volatile uint8_t FOC_HardwareBreakFault;
static uint8_t AS5600_ReportedFaults,is_in_vel1_loop;
static uint32_t now,primask,ipsr;
static int outputs,moe,power_calls,reads,discarded,unprotected_power_sets;
static int fail_reads,always_fail,spinning,break_flag,trip_on_power;
static uint16_t angle_input;
volatile uint8_t UART1_FrameReady,UART2_FrameReady,UART1_BuildLength,UART2_BuildLength;
volatile uint8_t UART1_DropUntilEol,UART2_DropUntilEol;
#define __HAL_TIM_MOE_DISABLE_UNCONDITIONALLY(h) (moe=0)
#define __HAL_TIM_MOE_ENABLE(h) (moe=1)
#define TIM_FLAG_BREAK 1
#define RESET 0
#define __HAL_TIM_GET_FLAG(h,f) break_flag
#define __HAL_TIM_CLEAR_FLAG(h,f) (break_flag=0)
static void HAL_TIM_IRQHandler(TIM_HandleTypeDef *h){}
static void HAL_GPIO_WritePin(int port,int pins,int value){
  if(pins==MOTOR_EN1_Pin&&value==GPIO_PIN_SET){++power_calls;if(!primask)++unprotected_power_sets;if(trip_on_power)FOC_HardwareBreakFault=2;}
  if(value)outputs|=pins;else outputs&=~pins;
}
static uint32_t HAL_GetTick(void){return now;}
static void HAL_Delay(uint32_t ms){now+=ms;}
static uint32_t get_micros(void){return now*1000;}
static uint32_t __get_PRIMASK(void){return primask;}
static uint32_t __get_IPSR(void){return ipsr;}
static void __disable_irq(void){primask=1;}
static void __set_PRIMASK(uint32_t p){primask=p;}
static void Motor_Power_Init(uint8_t mode){HAL_GPIO_WritePin(GPIOE,MOTOR_EN1_Pin,GPIO_PIN_SET);HAL_Delay(100);}
void App_DiscardPendingMotorCommandsActual(void);
static void App_DiscardPendingMotorCommands(void){++discarded;App_DiscardPendingMotorCommandsActual();}
static HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus,int addr,int reg,int mode,uint8_t *data,int size,uint32_t timeout){
  ++reads;++now;
  if(always_fail||reads<=fail_reads)return HAL_ERROR;
  if(spinning)angle_input=(angle_input+400)&4095;
  data[0]=angle_input>>8;data[1]=angle_input&255;return HAL_OK;
}
void FOC_M1EncoderFaultStop(void);
uint8_t FOC_M1RecoverEncoder(void);
uint8_t FOC_M1ReleaseRecoveryHold(void);
uint8_t AS5600_RdReg(I2C_HandleTypeDef*,uint16_t,uint8_t*,uint16_t);
#include "recovery_extracted.h"
static int checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
static int parse(const char *s){return Parse_Command((uint8_t*)s,(uint8_t)strlen(s));}
static void reset(void){
  (void)Motor_Power_Init;(void)HAL_GetTick;(void)is_in_vel1_loop;(void)App_DiscardPendingMotorCommands;(void)__get_IPSR;
  memset(&Sensor1,0,sizeof Sensor1);memset(&M0,0,sizeof M0);memset(&M1,0,sizeof M1);
  M0_EncoderType=2;M1_EncoderType=0;FOC_VelocityFault=0;FOC_M1CalibrationValid=1;FOC_M1RecoveryHold=0;
  FOC_HardwareBreakFault=0;
  break_flag=trip_on_power=0;
  unprotected_power_sets=0;
  ipsr=0;
  AS5600_ReadFaults=0;reads=0;now=1000;fail_reads=always_fail=spinning=discarded=power_calls=0;outputs=3;moe=1;angle_input=1024;primask=0;
}
static void fault(void){always_fail=1;AS5600_RdRawAngle(&Sensor1,&hi2c2);always_fail=0;reads=0;}
int main(void){
  reset();M1.mode=MODE_VELOCITY;M1.param.Vel=100;M1_Curs.iqr=2;M1_Vels.velocityTar=100;fault();
  CHECK(M1_EncoderType==2);CHECK(AS5600_ReadFaults==2);CHECK(FOC_M1RecoveryHold==1);
  CHECK(M1.mode==MODE_OPEN&&M1.param.Ope==0);CHECK(M1_Curs.iqr==0&&M1_Vels.velocityTar==0);
  CHECK(!moe&&(outputs&M1_BK_OUT_Pin)&&!(outputs&3));CHECK(!parse("o0,v100"));
  always_fail=1;uint32_t start=now;CHECK(!parse("RECOVER"));CHECK(reads<=5&&now-start<=250);
  CHECK(M1_EncoderType==2&&FOC_M1RecoveryHold&&!moe&&!(outputs&3));
  always_fail=0;reads=0;M1_iq_pid.errorSum=3;M1_vel_pid.lastError=2;Sensor1.velocity=200;
  UART1_FrameReady=UART2_FrameReady=1;UART1_BuildLength=3;UART2_BuildLength=5;
  UART1_DropUntilEol=UART2_DropUntilEol=0;
  primask=1;CHECK(!parse("RECOVER")&&reads==0&&power_calls==1);CHECK(primask==1);primask=0;
  CHECK(parse("RECOVER"));
  CHECK(!UART1_FrameReady&&!UART2_FrameReady&&!UART1_BuildLength&&!UART2_BuildLength);
  CHECK(UART1_DropUntilEol&&UART2_DropUntilEol);
  CHECK(App_ShouldDropRecoveryByte(&UART1_DropUntilEol,'v')&&UART1_DropUntilEol);
  CHECK(App_ShouldDropRecoveryByte(&UART1_DropUntilEol,'\n')&&!UART1_DropUntilEol);
  CHECK(!App_ShouldDropRecoveryByte(&UART1_DropUntilEol,'o'));
  CHECK(reads==3&&discarded==1);CHECK(M1_EncoderType==0&&Sensor1.result==AS5600_OK);
  CHECK(unprotected_power_sets==0);
  CHECK(M1.mode==MODE_OPEN&&M1.param.Ope==0&&FOC_M1RecoveryHold==1&&!moe);
  CHECK(outputs&M1_BK_OUT_Pin);CHECK(Sensor1.velocity==0&&!Sensor1.velocityInitialized);
  CHECK(M1_iq_pid.errorSum==0&&M1_vel_pid.lastError==0);CHECK(AS5600_ReadFaults==2);
  CHECK(parse("o0,o0")&&FOC_M1RecoveryHold&&!moe);
  CHECK(parse("o0,v100")&&M1.mode==MODE_VELOCITY&&M1.param.Vel==100);
  CHECK(!FOC_M1RecoveryHold&&moe&&!(outputs&M1_BK_OUT_Pin));CHECK(!parse("RECOVER"));
  reset();fault();fail_reads=2;CHECK(parse("RECOVER"));CHECK(reads==5);
  reset();fault();spinning=1;CHECK(!parse("RECOVER"));CHECK(reads==5&&M1_EncoderType==2&&!moe);
  reset();fault();FOC_M1CalibrationValid=0;CHECK(!parse("RECOVER"));CHECK(M1_EncoderType==2&&!moe);
  reset();fault();FOC_VelocityFault=2;CHECK(!parse("RECOVER"));CHECK(reads==0&&FOC_VelocityFault==2);
  reset();fault();CHECK(!parse("RECOVERx")&&!parse("recover"));CHECK(!parse("v100,v100"));
  reset();break_flag=1;TIM8_BRK_TIM12_IRQHandler();CHECK(FOC_HardwareBreakFault==2&&!moe&&M1_EncoderType==2);
  CHECK(!parse("RECOVER")&&reads==0&&power_calls==0);
  M1_EncoderType=0;FOC_M1RecoveryHold=0;CHECK(!parse("o0,v100"));
  reset();fault();trip_on_power=1;CHECK(!parse("RECOVER")&&reads==0&&!(outputs&3));
  reset();fault();break_flag=1;CHECK(!parse("RECOVER")&&power_calls==0);
  reset();fault();ipsr=15;CHECK(!parse("RECOVER")&&power_calls==0&&reads==0);
  printf("Encoder recovery: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
