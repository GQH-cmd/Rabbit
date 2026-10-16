/* 生产函数行为回归。能抓住：旧角度被重复微分、时间戳错配、速度相关滤波偏置、
 * 冷启动/微秒回绕错误，以及把真实运动或I2C故障错误显示成静止。 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef float float32_t;
#define PI 3.14159265358979323846f
#define _2PI 2 * PI
#define AS5600_OK 0
#define AS5600_ERROR 1
#define ENCODER_DISABLED 2
#define RAW_ANGLE_H 0x0c
#define GPIOE 1
#define MOTOR_EN1_Pin 0
#define MOTOR_EN2_Pin 1
#define M0_BK_OUT_Pin 2
#define M1_BK_OUT_Pin 3
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
typedef struct { unsigned bus; } I2C_HandleTypeDef;
static I2C_HandleTypeDef hi2c1={0},hi2c2={1};
static uint32_t clock_us;
static uint16_t raw_input[2];
static bool read_error[2];
static int gpio[4];
uint8_t M0_EncoderType,M1_EncoderType;
static uint32_t get_micros(void){return clock_us;}
/* 旧版函数使用它；新版不应再用调用时刻替代角度采样时刻。 */
uint32_t delta_time_us(uint32_t old){return clock_us-old;}
static float32_t my_fabsf(float32_t x){return fabsf(x);}
static uint8_t AS5600_RdReg(I2C_HandleTypeDef *bus,uint16_t reg,uint8_t *out,uint16_t size){
    if(reg!=RAW_ANGLE_H||size!=2){fprintf(stderr,"Unexpected I2C request\n");return AS5600_ERROR;}
    if(read_error[bus->bus])return AS5600_ERROR;
    out[0]=(uint8_t)(raw_input[bus->bus]>>8);out[1]=(uint8_t)raw_input[bus->bus];
    return AS5600_OK;
}
static void HAL_GPIO_WritePin(int port,int pin,int value){
    if(port==GPIOE&&pin>=0&&pin<4)gpio[pin]=value;
}
/* Sensor0/1与真实函数都来自同一份生产结构，硬件边界以上不做mock。 */
#include "as5600_velocity_extracted.h"
AS5600 Sensor0,Sensor1;

static int failures,checks;
#define CHECK(expr) do{++checks;if(!(expr)){++failures;printf("FAIL %s:%d: %s\n",__func__,__LINE__,#expr);}}while(0)
static float rpm(float velocity){return velocity*60.0f/(2.0f*PI);}
static void reset(void){
    memset(&Sensor0,0,sizeof Sensor0);memset(&Sensor1,0,sizeof Sensor1);
    memset(raw_input,0,sizeof raw_input);memset(read_error,0,sizeof read_error);
    M0_EncoderType=M1_EncoderType=0;clock_us=1000;
    gpio[0]=gpio[1]=1;gpio[2]=gpio[3]=0;AS5600_Init();
}
static float sample(unsigned bus,uint16_t raw,uint32_t sample_at,unsigned call_delay){
    AS5600 *s=bus?&Sensor1:&Sensor0;
    raw_input[bus]=raw;clock_us=sample_at;
    AS5600_RdRawAngle(s,bus?&hi2c2:&hi2c1);
    clock_us=sample_at+call_delay;
    return rpm(I2C_Get_Velocity(s));
}
static uint16_t motion_angle(uint16_t initial,double speed_rpm,uint32_t elapsed){
    /* 独立物理输入：12位编码器每圈4096计数，不使用生产换算/滤波生成期望值。 */
    long long count=llround((double)initial+speed_rpm*4096.0*(double)elapsed/60000000.0);
    count%=4096;if(count<0)count+=4096;return (uint16_t)count;
}
static void constant_angle_no_startup_spike(void){
    reset();float peak=0;
    for(unsigned i=0;i<300;++i){float v=fabsf(sample(0,3000,1000+i*333,0));if(v>peak)peak=v;}
    CHECK(peak<0.001f);
}
static void small_stationary_jitter_does_not_trip_65(void){
    const unsigned short_dt[]={50,50,100},long_dt[]={50,400,400};
    const unsigned span[]={2,1,2};
    for(unsigned profile=0;profile<3;++profile){
        reset();uint32_t at=1000;float peak=0;sample(0,1024,at,0);
        for(unsigned n=0;n<1000;++n){
            at+=(n&1)?short_dt[profile]:long_dt[profile];
            float v=fabsf(sample(0,(uint16_t)(1024+((n&1)?0:span[profile])),at,0));
            if(v>peak)peak=v;
        }
        printf("stationary jitter profile=%u peak_rpm=%.3f\n",profile,peak);
        CHECK(peak<10.0f); /* 不只是刚好压到65以下。 */
    }
}
static void repeated_control_call_does_not_invent_zero_samples(void){
    reset();float before=0;
    for(unsigned n=0;n<=100;++n)before=sample(0,motion_angle(1000,300,n*1000),1000+n*1000,0);
    CHECK(before>290.0f&&before<310.0f);
    for(unsigned n=0;n<100;++n){clock_us+=100;(void)I2C_Get_Velocity(&Sensor0);}
    CHECK(fabsf(rpm(Sensor0.velocity)-before)<0.001f);
}
static void real_motion_not_hidden_and_wrap_direction_correct(void){
    const double targets[]={70,-70,100,-100,300,-300,1000,-1000};
    for(unsigned target=0;target<8;++target){
        reset();float end=0;uint32_t elapsed=0;
        sample(0,4090,1000,0);unsigned crossed_at=0;
        for(unsigned n=1;n<=1500;++n){
            elapsed+=(n&1)?50:400;
            end=sample(0,motion_angle(4090,targets[target],elapsed),1000+elapsed,0);
            if(!crossed_at&&fabsf(end)>65.0f)crossed_at=elapsed;
        }
        printf("moving target_rpm=%.0f actual_rpm=%.3f first_over65_us=%u\n",targets[target],end,crossed_at);
        CHECK(fabsf(end-(float)targets[target])<10.0f);
        CHECK(crossed_at>0&&crossed_at<30000);
    }
}
static void call_latency_does_not_replace_acquisition_time(void){
    reset();float end=0;
    for(unsigned n=0;n<300;++n){
        unsigned delay=(n&1)?900:0;
        end=sample(0,motion_angle(1234,100,n*1000),1000+n*1000,delay);
    }
    CHECK(fabsf(end-100.0f)<5.0f);
}
static void tick_zero_and_microsecond_wrap_are_valid(void){
    reset();sample(0,1000,0,0);
    float first=sample(0,1020,1000,0);
    CHECK(first>10.0f); /* 时间戳0不是“未初始化”的哨兵。 */
    reset();float end=0;uint32_t start=UINT32_MAX-2000;
    for(unsigned n=0;n<=200;++n)end=sample(0,motion_angle(1000,-100,n*1000),start+n*1000,0);
    CHECK(fabsf(end+100.0f)<5.0f);
}
static void two_motors_have_independent_history(void){
    reset();float a=0,b=0;
    for(unsigned n=0;n<=300;++n){
        a=sample(0,motion_angle(1000,100,n*1000),1000+n*1000,0);
        b=sample(1,motion_angle(3000,-300,n*1000),1100+n*1000,0);
    }
    CHECK(fabsf(a-100.0f)<5.0f);CHECK(fabsf(b+300.0f)<5.0f);
}
static void actual_stop_settles_without_old_filter_tail(void){
    reset();float end=0;
    for(unsigned n=0;n<=150;++n)sample(0,motion_angle(1000,300,n*1000),1000+n*1000,0);
    uint16_t last=raw_input[0];
    for(unsigned n=1;n<=40;++n)end=sample(0,last,151000+n*1000,0);
    CHECK(fabsf(end)<1.0f);
}
static void long_gap_rebaselines_without_claiming_zero_speed(void){
    reset();float before=0;
    for(unsigned n=0;n<=100;++n)before=sample(0,motion_angle(1000,100,n*1000),1000+n*1000,0);
    uint16_t last=raw_input[0];uint32_t resumed=clock_us+1000001;
    float held=sample(0,last,resumed,0);CHECK(fabsf(held-before)<0.001f);
    for(unsigned n=1;n<=40;++n)sample(0,last,resumed+n*1000,0);
    CHECK(fabsf(rpm(Sensor0.velocity))<1.0f);
}
static void i2c_error_keeps_existing_both_motor_shutdown(void){
    for(unsigned bus=0;bus<2;++bus){
        reset();sample(bus,1000,1000,0);sample(bus,1050,2000,0);
        read_error[bus]=true;(void)sample(bus,1050,3000,0);
        AS5600 *s=bus?&Sensor1:&Sensor0;
        CHECK((bus?M1_EncoderType:M0_EncoderType)==ENCODER_DISABLED);
        CHECK(s->result==AS5600_ERROR&&s->velocity==0.0f);
        CHECK(gpio[0]==0&&gpio[1]==0&&gpio[2]==1&&gpio[3]==1);
    }
}
int main(void){
    constant_angle_no_startup_spike();
    small_stationary_jitter_does_not_trip_65();
    repeated_control_call_does_not_invent_zero_samples();
    real_motion_not_hidden_and_wrap_direction_correct();
    call_latency_does_not_replace_acquisition_time();
    tick_zero_and_microsecond_wrap_are_valid();
    two_motors_have_independent_history();
    actual_stop_settles_without_old_filter_tail();
    long_gap_rebaselines_without_claiming_zero_speed();
    i2c_error_keeps_existing_both_motor_shutdown();
    printf("AS5600 velocity regression: %d checks, %d failures; simulated hardware only\n",checks,failures);
    return failures?1:0;
}
