#define FOC_M1_ONLY 1
#define main dual_suite_not_run
#include "stm32_runtime_test.cpp"
#undef main
int main(){
  M0_EncoderType=ENCODER_DISABLED;M1_EncoderType=ENCODER_AS5600;
  CHECK(parse("o0,v100")&&M1.mode==MODE_VELOCITY&&M1.param.Vel==100);
  CHECK(!parse("v100,v100")&&M0.mode==MODE_OPEN&&M0.param.Ope==0);
  CHECK(!parse("v0,v100"));CHECK(!parse("c1,v100"));CHECK(!parse("p0,v100"));
  CHECK(parse("o0,o0"));App_SPI3_Snapshot();
  CHECK(SPI3_LinkStatus.flags&0x80);CHECK(!(SPI3_LinkStatus.flags&FLP_ENCODER0));
  CHECK(SPI3_LinkStatus.rpm0==0&&SPI3_LinkStatus.iq0==0&&SPI3_LinkStatus.mode0==0);
  M1_EncoderType=ENCODER_DISABLED;CHECK(!parse("o0,v100"));CHECK(parse("o0,o0"));
  App_SPI3_Snapshot();CHECK(!(SPI3_LinkStatus.flags&FLP_ENCODER1));
  recoveryResult=0;UART1_FrameReady=1;CHECK(!parse("RECOVER")&&recoveryCalls==1&&UART1_FrameReady==1);
  recoveryResult=1;UART1_FrameReady=UART2_FrameReady=1;
  CHECK(parse("RECOVER")&&!UART1_FrameReady&&!UART2_FrameReady);
  App_SPI3_CommandTask(); // 主循环先冲掉跨通道旧帧，再从新DMA接收后续请求。
  command(998,"RECOVER");CHECK(SPI3_LinkStatus.result==FLP_ACCEPTED&&!SPI3_LinkOwned);
  CHECK(!parse("RECOVERx"));
  M1_EncoderType=ENCODER_AS5600;CHECK(parse("o0,o0"));
  flp_request(SPI3_LinkRxFrame,FLP_COMMAND,999,"o0,v100",0);
  SPI3_LinkFrameReady=1;hspi3.RxXferCount=0;CHECK(parse("RECOVER"));
  App_SPI3_CommandTask();CHECK(M1.mode==MODE_OPEN&&M1.param.Ope==0&&!SPI3_LinkOwned);
  command(1000,"o0,v100");CHECK(M1.mode==MODE_VELOCITY&&M1.param.Vel==100);
  CHECK(parse("o0,o0"));command(1001,"RECOVER");
  FlpStatus recovered{};CHECK(flp_read_reply(SPI3_LinkTxFrame,&recovered)&&recovered.id==1001&&recovered.result==FLP_ACCEPTED);
  hspi3.RxXferCount=20;nss=0;CHECK(parse("RECOVER"));
  App_SPI3_CommandTask();CHECK(SPI3_LinkDiscardPending&&M1.mode==MODE_OPEN);
  nss=1;App_SPI3_CommandTask();CHECK(!SPI3_LinkDiscardPending&&hspi3.RxXferCount==FLP_SIZE&&M1.mode==MODE_OPEN);
  std::printf("M1-only STM32 parser/snapshot: %d checks\n",count);return 0;
}
