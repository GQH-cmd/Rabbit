/* 实际ADC回调、零角度/方向校准、FOC_Init、PWM出口。仅I/O及电流环边界替换。 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef float float32_t;
#include "m1_control_types.h"
#define ENCODER_AS5600 0
#define ENCODER_AS5047 1
#define ENCODER_DISABLED 2
#define M0_PolePairs 7
#define M1_PolePairs 7
#define GPIOE 1
#define M0_BK_OUT_Pin 1
#define M1_BK_OUT_Pin 2
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define ADC1 1
#define ADC2 2
#define ADC_INJECTED_RANK_1 1
#define ADC_INJECTED_RANK_2 2
#define TIM_CHANNEL_1 0
#define TIM_CHANNEL_2 1
#define TIM_CHANNEL_3 2
#define TIM_CHANNEL_4 3
#define TIM_IT_UPDATE 1
typedef struct{int Instance;} ADC_HandleTypeDef;
typedef struct{int bus;} I2C_HandleTypeDef;
typedef struct{unsigned compare[3];} TIM_HandleTypeDef;
typedef struct{int sensorPrescaler;float angle;uint16_t rawAngle;} Sensor;
typedef struct{float Tcmp1,Tcmp2,Tcmp3;} SVPWM_Outputs;
typedef struct{int unused;} PIDController;
ADC_HandleTypeDef hadc1={ADC1},hadc2={ADC2};
I2C_HandleTypeDef hi2c1={0},hi2c2={1};
TIM_HandleTypeDef htim1,htim8;
static int pwm_starts[2],pwmn_starts[2],moe_enabled[2];
static void HAL_TIM_Base_Start(TIM_HandleTypeDef *h){}
static void HAL_TIM_PWM_Start(TIM_HandleTypeDef *h,int ch){++pwm_starts[h==&htim8];}
static void HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *h,int ch){++pwmn_starts[h==&htim8];}
#define __HAL_TIM_ENABLE_IT(h,it) ((void)0)
#define __HAL_TIM_MOE_ENABLE(h) (++moe_enabled[(h)==&htim8])
#define __HAL_TIM_MOE_DISABLE(h) (moe_enabled[(h)==&htim8]=0)
#define __HAL_TIM_SET_COMPARE(h,ch,v) ((h)->compare[ch]=(unsigned)(v))
Motor M0_Motor,M1_Motor;
MotorControl M0,M1;
CURRENT_s M0_Curs,M1_Curs;
VELOCITY_s M0_Vels,M1_Vels;
POSITION_s M0_Poses,M1_Poses;
Sensor Sensor0,Sensor1,Sensor2,Sensor3;
PIDController M0_iq_pid,M0_id_pid,M0_vel_pid,M0_pos_pid,M1_iq_pid,M1_id_pid,M1_vel_pid,M1_pos_pid;
uint8_t M0_EncoderType=ENCODER_DISABLED,M1_EncoderType=ENCODER_AS5600;
volatile uint8_t FOC_M1CalibrationValid=0;
static float V_REF=3.3f,ADC_MAX=4096,R_SHUNT=0.01f,GAIN=50;
static float adc_offset_1,adc_offset_2,adc_offset_3,adc_offset_4;
static int sensor0Prescaler,sensor1Prescaler,reads[2],controls[2],brake0,power_calls;
static int adc_starts[2],adc_reads[2];
static float last_q,last_d;
static int checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
void SetSVPWM(float32_t,float32_t,float32_t,float32_t);
static SVPWM_Outputs CalculateSVPWM(float q,float d,float a){last_q=q;last_d=d;return (SVPWM_Outputs){10,20,30};}
void HAL_GPIO_WritePin(int p,int pin,int value){if(pin&M0_BK_OUT_Pin)brake0=value;}
static void HAL_ADC_Start(ADC_HandleTypeDef *a){++adc_starts[a==&hadc2];}
static uint32_t HAL_ADCEx_InjectedGetValue(ADC_HandleTypeDef *a,int rank){++adc_reads[a==&hadc2];return 2000;}
static void AS5600_RdRawAngle(Sensor *s,I2C_HandleTypeDef *bus){++reads[bus->bus];s->rawAngle=1234;s->angle+=0.01f;}
static float I2C1_AS5600_GetAngle(void){AS5600_RdRawAngle(&Sensor0,&hi2c1);return Sensor0.angle;}
static float I2C2_AS5600_GetAngle(void){AS5600_RdRawAngle(&Sensor1,&hi2c2);return Sensor1.angle;}
static void SPI2_AS5047P_GetAngle(void){++reads[0];}
static void SPI3_AS5047P_GetAngle(void){++reads[1];}
static float Normalize_Angle(float a){return a;}
static void M0_OpeLoop(float a){++controls[0];}
static void M1_OpeLoop(float a){++controls[1];}
static void M0_CurLoop(void){++controls[0];}
static void M1_CurLoop(void){++controls[1];}
static void M0_VelLoop(void){++controls[0];}
static void M1_VelLoop(void){++controls[1];}
static void M0_PosLoop(void){++controls[0];}
static void M1_PosLoop(void){++controls[1];}
static void Motor_Init(Motor *m,int number,int pairs){m->motorNum=number;m->polePairs=pairs;m->dir=0;}
static void CurLoopInit(CURRENT_s *s){memset(s,0,sizeof(*s));}
static void VelLoopInit(VELOCITY_s *s){memset(s,0,sizeof(*s));}
static void PosLoopInit(POSITION_s *s){memset(s,0,sizeof(*s));}
static void PID_init(PIDController *p,float kp,float ki,float kd,float low,float high,float ilow,float ihigh){}
static void Motor_Power_Init(int mode){++power_calls;}
static void HAL_Delay(unsigned delay){}
#include "m1_control_extracted.h"
int main(void){
  Current_Init();CHECK(adc_starts[0]==0&&adc_reads[0]==0);
  CHECK(adc_starts[1]==1&&adc_reads[1]==200);
  CHECK(adc_offset_1==0&&adc_offset_2==0&&adc_offset_3==2000&&adc_offset_4==2000);
  Motor_TIM18_Init();CHECK(pwm_starts[0]==0&&pwmn_starts[0]==0&&moe_enabled[0]==0);
  CHECK(pwm_starts[1]==4&&pwmn_starts[1]==3&&moe_enabled[1]==1);
  Sensor0.sensorPrescaler=Sensor1.sensorPrescaler=2;
  M1.mode=MODE_VELOCITY;M1.param.Vel=100;
  for(int i=0;i<6;i++){HAL_ADCEx_InjectedConvCpltCallback(&hadc1);HAL_ADCEx_InjectedConvCpltCallback(&hadc2);}
  CHECK(reads[0]==0);CHECK(reads[1]==2);CHECK(controls[0]==0);CHECK(controls[1]==6);
  M0_EncoderType=ENCODER_AS5600; // 即使其他路径意外改了类型，单轴配置仍不允许M0采样/驱动。
  for(int i=0;i<3;i++)HAL_ADCEx_InjectedConvCpltCallback(&hadc1);
  CHECK(reads[0]==0);CHECK(controls[0]==0);
  SetSVPWM(0,3,3,2);CHECK(htim1.compare[0]==0&&htim1.compare[1]==0&&htim1.compare[2]==0);CHECK(brake0==GPIO_PIN_SET);
  SetSVPWM(1,3,0,2);CHECK(htim8.compare[0]==10&&htim8.compare[2]==30);
  M0_EncoderType=ENCODER_DISABLED;reads[0]=reads[1]=0;FOC_Init();
  CHECK(power_calls==1);CHECK(reads[0]==0);CHECK(reads[1]>=101);CHECK(M1_Motor.dir==1);
  CHECK(FOC_M1CalibrationValid==1);
  M1_EncoderType=ENCODER_DISABLED;int before=controls[1],before_reads=reads[1];
  HAL_ADCEx_InjectedConvCpltCallback(&hadc2);CHECK(controls[1]==before);CHECK(reads[1]==before_reads);
  CHECK(last_q==0&&last_d==0);
  printf("M1-only ADC/calibration/PWM: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
