/* 真实HAL/业务函数；仅模拟硬件寄存器、抢占条件与I/O边界。有限host预算避免旧版无限循环挂死测试。 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stddef.h>

typedef enum {HAL_OK=0,HAL_ERROR=1,HAL_TIMEOUT=3} HAL_StatusTypeDef;
typedef enum {RESET=0,SET=1} FlagStatus;
typedef struct {uint32_t PreviousState,State,Mode,ErrorCode,Lock,Instance;} I2C_HandleTypeDef;
typedef struct {uint32_t CR2,SQR1;} ADC_TypeDef;
typedef struct {uint32_t ContinuousConvMode;} ADC_InitTypeDef;
typedef struct {ADC_TypeDef *Instance;uint32_t State,Lock;ADC_InitTypeDef Init;} ADC_HandleTypeDef;
typedef struct {uint32_t Channel,Rank,SamplingTime;} ADC_ChannelConfTypeDef;
#define HAL_MAX_DELAY 0xffffffffU
#define HAL_ADC_STATE_ERROR_CONFIG 0x01U
#define HAL_ADC_STATE_TIMEOUT 0x02U
#define HAL_ADC_STATE_REG_EOC 0x04U
#define HAL_ADC_STATE_REG_BUSY 0x08U
#define HAL_ADC_STATE_INJ_BUSY 0x10U
#define HAL_ADC_STATE_READY 0x20U
#define HAL_I2C_STATE_READY 0x01U
#define HAL_I2C_MODE_NONE 0U
#define I2C_STATE_NONE 0U
#define HAL_I2C_ERROR_TIMEOUT 0x20U
#define ADC_CR2_EOCS 0x400U
#define ADC_CR2_DMA 0x100U
#define ADC_SQR1_L 0xf00000U
#define ADC_FLAG_EOC 2U
#define ADC_FLAG_STRT 16U
#define ADC_SAMPLETIME_3CYCLES 0U
#define DISABLE 0U
#define HAL_IS_BIT_SET(x,b) (((x)&(b))!=0)
#define HAL_IS_BIT_CLR(x,b) (((x)&(b))==0)
#define SET_BIT(x,b) ((x)|=(b))
#define CLEAR_BIT(x,b) ((x)&=~(b))
#define __HAL_UNLOCK(h) ((h)->Lock=0)
#define ADC_IS_SOFTWARE_START_REGULAR(h) 1
#define __HAL_ADC_CLEAR_FLAG(h,f) ((void)0)
#define AS5600_SLAVE_ADDRESS (0x36U<<1)
#define I2C_MEMADD_SIZE_8BIT 1
#define AS5600_OK 0
#define AS5600_ERROR 1
#define I2C1 1U
#define I2C2 2U
#define GPIOE 1
#define TEST_LED_Pin 0
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define BLACK 0
#define M0AS5600Error ((const void*)1)
#define M1AS5600Error ((const void*)2)
static ADC_TypeDef regs[3];
ADC_HandleTypeDef hadc1={&regs[0],0,0,{0}},hadc2={&regs[1],0,0,{0}},hadc3={&regs[2],0,0,{0}};
static I2C_HandleTypeDef i2c1={0,0,0,0,0,I2C1},i2c2={0,0,0,0,0,I2C2};
static jmp_buf loop_budget;
static unsigned reads,ticks,can_preempt,adc_ready_after,i2c_ready_after,inside_irq,irq_ui_calls,serial_calls,lcd_calls;
static unsigned start_calls,get_calls,adc_data,force_config_error,force_start_error,force_i2c_error;
static uint32_t config_channel,config_rank,config_sample;
static int checks,failures;
#define CHECK(expr) do{++checks;if(!(expr)){++failures;printf("FAIL %s:%d: %s\n",__func__,__LINE__,#expr);}}while(0)
static uint32_t HAL_GetTick(void){if(can_preempt)++ticks;return ticks;}
static unsigned bounded_flag_read(void){if(++reads>10000)longjmp(loop_budget,1);return reads;}
static FlagStatus i2c_flag(void){bounded_flag_read();return i2c_ready_after&&reads>=i2c_ready_after?RESET:SET;}
static int adc_flag(void){return adc_ready_after&&bounded_flag_read()>=adc_ready_after?1:(adc_ready_after?0:(bounded_flag_read(),0));}
#define __HAL_I2C_GET_FLAG(h,f) i2c_flag()
#define __HAL_ADC_GET_FLAG(h,f) adc_flag()
static HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *h,ADC_ChannelConfTypeDef *s){
    config_channel=s->Channel;config_rank=s->Rank;config_sample=s->SamplingTime;
    return force_config_error?HAL_ERROR:HAL_OK;
}
static HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h){++start_calls;return force_start_error?HAL_ERROR:HAL_OK;}
static uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h){++get_calls;return adc_data;}
static HAL_StatusTypeDef I2C_WaitOnFlagUntilTimeout(I2C_HandleTypeDef*,uint32_t,FlagStatus,uint32_t,uint32_t);
static HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef*,uint32_t);
static HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h,uint32_t dev,uint16_t reg,uint32_t asize,uint8_t *out,uint16_t size,uint32_t timeout){
    if(force_i2c_error)return HAL_ERROR;
    return I2C_WaitOnFlagUntilTimeout(h,1,SET,timeout,HAL_GetTick());
}
static void HAL_GPIO_WritePin(int p,int pin,int level){}
static int test_driver_printf(const char *fmt,...){++serial_calls;if(inside_irq)++irq_ui_calls;return 0;}
void LCD_Clear(int color){++lcd_calls;if(inside_irq)++irq_ui_calls;}
void LCD_Show_Image(int x,int y,int w,int h,const void *image){++lcd_calls;if(inside_irq)++irq_ui_calls;}
#define printf test_driver_printf
#include "hal_wait_extracted.h"
#undef printf

static void fresh(void){
    reads=ticks=0;can_preempt=SOURCE_TICK_PRIORITY<SOURCE_ADC_PRIORITY;
    adc_ready_after=3;i2c_ready_after=0;inside_irq=1;irq_ui_calls=serial_calls=lcd_calls=0;
    start_calls=get_calls=0;adc_data=1234;force_config_error=force_start_error=force_i2c_error=0;
    config_channel=config_rank=config_sample=0;
    ADC_AuxReadFaults=0;AS5600_ReadFaults=0;
}
static void adc_configuration_is_fully_initialized(void){
    fresh();uint16_t value=Read_ADC_Value(&hadc1,1);
    CHECK(value==1234);CHECK(config_channel==1);CHECK(config_rank==1);CHECK(config_sample==ADC_SAMPLETIME_3CYCLES);
    printf("ADC observed rank=0x%08X sampling=0x%08X\n",config_rank,config_sample);
}
static void adc_stuck_conversion_returns_without_hanging(void){
    fresh();(void)Read_ADC_Value(&hadc1,1);adc_ready_after=0;reads=ticks=0;
    if(setjmp(loop_budget)){CHECK(false);printf("ADC wait exceeded host budget; timeout is not bounded\n");return;}
    get_calls=0;uint16_t held=Read_ADC_Value(&hadc1,1);
    CHECK(held==1234);CHECK(ADC_AuxReadFaults&1);CHECK(ticks>0&&ticks<=5);
    CHECK(get_calls==0);
    adc_ready_after=3;reads=0;adc_data=2048;
    CHECK(Read_ADC_Value(&hadc1,1)==2048);CHECK(ADC_AuxReadFaults&1);
}
static void adc_failure_does_not_start_or_read_invalid_conversion(void){
    fresh();adc_data=2000;(void)Read_ADC_Value(&hadc2,0);
    force_config_error=1;start_calls=get_calls=0;adc_data=4095;
    CHECK(Read_ADC_Value(&hadc2,0)==2000);CHECK(start_calls==0);CHECK(ADC_AuxReadFaults&2);CHECK(get_calls==0);
    force_config_error=0;force_start_error=1;start_calls=0;
    CHECK(Read_ADC_Value(&hadc2,0)==2000);CHECK(start_calls==1);CHECK(get_calls==0);
    force_start_error=0;reads=0;adc_data=777;
    CHECK(Read_ADC_Value(&hadc2,0)==777);
}
static void adc_instances_keep_independent_valid_caches(void){
    fresh();adc_data=321;(void)Read_ADC_Value(&hadc1,1);
    adc_data=654;reads=0;(void)Read_ADC_Value(&hadc2,0);
    adc_data=987;reads=0;(void)Read_ADC_Value(&hadc3,0);
    force_config_error=1;
    CHECK(Read_ADC_Value(&hadc1,1)==321);CHECK(Read_ADC_Value(&hadc2,0)==654);
    CHECK(Read_ADC_Value(&hadc3,0)==987);CHECK(ADC_AuxReadFaults==7);
}
static void i2c_healthy_transfer_has_no_fault_or_ui(void){
    fresh();i2c_ready_after=3;uint8_t out[2];
    CHECK(AS5600_RdReg(&i2c1,0x0c,out,2)==AS5600_OK);
    CHECK(AS5600_ReadFaults==0);CHECK(irq_ui_calls==0);
}
static void i2c_stuck_flag_times_out_inside_adc_irq(void){
    fresh();uint8_t out[2];
    if(setjmp(loop_budget)){CHECK(false);printf("I2C timeout tick cannot preempt ADC IRQ\n");return;}
    int status=AS5600_RdReg(&i2c1,0x0c,out,2);
    CHECK(status==AS5600_ERROR);CHECK(ticks>0&&ticks<=20);CHECK(irq_ui_calls==0);CHECK(AS5600_ReadFaults==1);
}
static void i2c_failure_reports_only_from_main_and_only_once(void){
    fresh();force_i2c_error=1;uint8_t out[2];
    CHECK(AS5600_RdReg(&i2c1,0x0c,out,2)==AS5600_ERROR);
    CHECK(AS5600_RdReg(&i2c2,0x0c,out,2)==AS5600_ERROR);
    CHECK(AS5600_ReadFaults==3);
    CHECK(irq_ui_calls==0);CHECK(serial_calls==0&&lcd_calls==0);
    unsigned before_main_serial=serial_calls;
    inside_irq=0;AS5600_ReportErrors();CHECK(serial_calls>before_main_serial);
    CHECK(lcd_calls==0); /* TIM6拥有LCD刷新；主循环故障消费不能新增跨上下文LCD事务。 */
    unsigned before_serial=serial_calls,before_lcd=lcd_calls;
    AS5600_ReportErrors();CHECK(serial_calls==before_serial&&lcd_calls==before_lcd);
    CHECK(AS5600_ReadFaults==3);
}
int main(void){
    adc_configuration_is_fully_initialized();adc_stuck_conversion_returns_without_hanging();
    adc_failure_does_not_start_or_read_invalid_conversion();adc_instances_keep_independent_valid_caches();
    i2c_healthy_transfer_has_no_fault_or_ui();i2c_stuck_flag_times_out_inside_adc_irq();
    i2c_failure_reports_only_from_main_and_only_once();
    printf("HAL wait-chain regression: %d checks, %d failures; host preemption simulation only\n",checks,failures);
    return failures?1:0;
}
