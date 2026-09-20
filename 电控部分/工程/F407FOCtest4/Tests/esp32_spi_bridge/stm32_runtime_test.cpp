#include <cstdio>
#include <cstring>
#include <cmath>
#include "FocLinkProtocol.h"
#define ESP32_SPI_LINK 1
#define PI 3.14159265358979323846f
#define ENCODER_DISABLED 0
#define ENCODER_AS5600 1
#define GPIOA 0
#define GPIO_PIN_15 15
#define GPIO_PIN_RESET 0
#define SPI3_IRQn 0
#define SPI3 3
#define HAL_SPI_STATE_READY 1
#define HAL_UNLOCKED 0
#define HAL_OK 0
#define SPI_FLAG_RXNE 1
#define SPI_FLAG_OVR 2
static uint32_t now=0,primask=0;
static int nss=1,arms=0,resets=0,extraByte=0,initResult=0;
uint32_t HAL_GetTick(){return now;}
uint32_t __get_PRIMASK(){return primask;}
void __disable_irq(){primask=1;}
void __enable_irq(){primask=0;}
void __set_PRIMASK(uint32_t n){primask=n;}
void HAL_NVIC_DisableIRQ(int){}
void HAL_NVIC_EnableIRQ(int){}
void HAL_NVIC_ClearPendingIRQ(int){}
#define __HAL_RCC_SPI3_FORCE_RESET() do {++resets;extraByte=0;}while(0)
#define __HAL_RCC_SPI3_RELEASE_RESET() do {}while(0)
#define __HAL_SPI_GET_FLAG(h,f) (extraByte)
int HAL_GPIO_ReadPin(int,int){return nss;}
struct SPI_HandleTypeDef {int Instance=SPI3,State=0,Lock=0;uint16_t RxXferCount=0;} hspi3;
int HAL_SPI_Init(SPI_HandleTypeDef*){return initResult;}
int HAL_SPI_TransmitReceive_IT(SPI_HandleTypeDef* h,uint8_t*,uint8_t*,uint16_t n){++arms;h->RxXferCount=n;return HAL_OK;}
enum MotorMode {MODE_OPEN,MODE_CURRENT,MODE_VELOCITY,MODE_POSITION};
struct MotorControl {MotorMode mode;union {float Ope,Cur,Vel,Pos;}param;} M0{},M1{};
struct Sensor {float velocity;}Sensor0{},Sensor1{},Sensor2{},Sensor3{};
struct Current {float iq;}M0_Curs{},M1_Curs{};
uint8_t M0_EncoderType=ENCODER_AS5600,M1_EncoderType=ENCODER_DISABLED,M0_ContactDetected=0,M1_ContactDetected=0;
uint8_t SPI3_LinkRxFrame[FLP_SIZE],SPI3_LinkTxFrame[FLP_SIZE];
volatile uint8_t SPI3_LinkFrameReady=0,SPI3_LinkError=0;
uint8_t SPI3_LinkArmed=0,SPI3_LinkOwned=0;
uint32_t SPI3_LinkLastAlive=0;
FlpStatus SPI3_LinkStatus{};
#define UART1_FRAME_SIZE 50
uint8_t UART1_FrameBuffer[50],UART1_FrameLength=0,UART1_FrameReady=0;
int huart1=1;char uartReply[16];
int HAL_UART_Transmit(int*,uint8_t* p,uint16_t n,int){memcpy(uartReply,p,n);uartReply[n]=0;return 0;}
#include "stm32_runtime_extracted.h"
static int count=0;
#define CHECK(x) do {++count;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int parse(const char* cmd){return Parse_Command((uint8_t*)cmd,(uint8_t)strlen(cmd));}
static void command(uint32_t id,const char* cmd){flp_request(SPI3_LinkRxFrame,FLP_COMMAND,id,cmd,0);hspi3.RxXferCount=0;HAL_SPI_TxRxCpltCallback(&hspi3);App_SPI3_CommandTask();}
int main(){
  CHECK(parse("v30,o0")&&M0.mode==MODE_VELOCITY&&M0.param.Vel==30);
  CHECK(!parse("v50,oNaN")&&M0.param.Vel==30&&M1.param.Ope==0);
  CHECK(!parse("q20,o0")&&M0.mode==MODE_VELOCITY);
  CHECK(!parse("v20,0")&&!parse("o2,o0,garbage"));
  primask=1;CHECK(parse("p-10,c0.5")&&primask==1);primask=0;
  nss=0;App_SPI3_CommandTask();CHECK(arms==0);nss=1;
  App_SPI3_CommandTask();CHECK(arms==1&&SPI3_LinkArmed);
  CHECK(M0.mode==MODE_POSITION&&M0.param.Pos==-10); // 初始化/探测不运动。
  command(1,"PING");CHECK(SPI3_LinkStatus.result==FLP_PONG&&M0.param.Pos==-10);
  FlpStatus s{};CHECK(flp_read_reply(SPI3_LinkTxFrame,&s)&&s.id==1&&s.result==FLP_PONG);
  command(2,"v30,o0");CHECK(SPI3_LinkOwned&&M0.param.Vel==30);
  int before=arms;HAL_SPI_TxRxCpltCallback(&hspi3);CHECK(arms==before); // ISR 不覆盖缓冲。
  App_SPI3_CommandTask();CHECK(M0.param.Vel==30); // 相同编号不会再应用。
  command(2,"v55,o0");CHECK(M0.param.Vel==30);
  command(3,"v55,oNaN");CHECK(SPI3_LinkStatus.result==FLP_BAD_COMMAND&&M0.param.Vel==30);
  now=1000;flp_request(SPI3_LinkRxFrame,FLP_POLL,0,"",1);SPI3_LinkFrameReady=1;
  App_SPI3_CommandTask();CHECK(SPI3_LinkLastAlive==1000&&M0.param.Vel==30);
  now=2999;App_SPI3_CommandTask();CHECK(SPI3_LinkOwned);
  now=3000;App_SPI3_CommandTask();CHECK(!SPI3_LinkOwned&&M0.mode==MODE_OPEN&&M0.param.Ope==0);
  CHECK(SPI3_LinkStatus.faults&FLP_FAULT_WATCHDOG);
  command(4,"v40,o0");flp_request(SPI3_LinkRxFrame,FLP_POLL,0,"",0);SPI3_LinkFrameReady=1;
  App_SPI3_CommandTask();CHECK(!SPI3_LinkOwned&&M0.param.Ope==0);
  command(5,"v40,o0"); // 串口有效命令接管，退出 SPI 看门狗；无效命令不能接管。
  memcpy(UART1_FrameBuffer,"v1,0",4);UART1_FrameLength=4;UART1_FrameReady=1;
  App_USART1_CommandTask();CHECK(!strcmp(uartReply,"NACK\r\n")&&SPI3_LinkOwned&&M0.param.Vel==40);
  memcpy(UART1_FrameBuffer,"v25,o0",6);UART1_FrameLength=6;UART1_FrameReady=1;
  App_USART1_CommandTask();CHECK(!strcmp(uartReply,"ACK\r\n")&&!SPI3_LinkOwned&&M0.param.Vel==25);
  now+=3000;App_SPI3_CommandTask();CHECK(M0.mode==MODE_VELOCITY&&M0.param.Vel==25);
  flp_request(SPI3_LinkRxFrame,FLP_COMMAND,6,"v90,o0",0);SPI3_LinkRxFrame[11]^=1;SPI3_LinkFrameReady=1;
  App_SPI3_CommandTask();CHECK(M0.param.Vel==25&&(SPI3_LinkStatus.faults&FLP_FAULT_FRAME));
  before=arms;hspi3.RxXferCount=20;nss=0;App_SPI3_CommandTask();CHECK(arms==before);
  nss=1;App_SPI3_CommandTask();CHECK(arms==before+1&&hspi3.RxXferCount==64);
  extraByte=1;command(7,"v90,o0");CHECK(M0.param.Vel==25); // 超长帧丢弃。
  HAL_SPI_ErrorCallback(&hspi3);before=arms;App_SPI3_CommandTask();CHECK(arms==before+1);
  Sensor0.velocity=-PI;M0_Curs.iq=-0.12f;M0_ContactDetected=1;command(8,"PING");
  CHECK(flp_read_reply(SPI3_LinkTxFrame,&s)&&s.rpm0>=-3001&&s.rpm0<=-2999&&s.iq0<=-119);
  CHECK((s.flags&FLP_CONTACT0)&&(s.flags&FLP_ENCODER0)&&!(s.flags&FLP_ENCODER1));
  now=0xffffff00;command(9,"v20,o0");now=0x10;App_SPI3_CommandTask();CHECK(SPI3_LinkOwned);
  now=0x900;App_SPI3_CommandTask();CHECK(!SPI3_LinkOwned&&M0.param.Ope==0);
  SPI3_LinkArmed=0;initResult=1;App_SPI3_CommandTask();CHECK(!SPI3_LinkArmed);
  initResult=0;App_SPI3_CommandTask();CHECK(SPI3_LinkArmed);
  printf("PASS %d STM32 runtime checks\n",count);
}
