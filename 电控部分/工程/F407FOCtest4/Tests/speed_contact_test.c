/* 捕获低速目标绕过保护、反转显示丢符号等回归；硬件时序不在主机测试范围内。 */
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "ContactDetect.h"

typedef float float32_t;
#define PI 3.14159265358979323846f
#define ENCODER_AS5600 0U
#define ENCODER_AS5047 1U
#define ENCODER_DISABLED 2U
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define TEST_LED_GPIO_Port 0
#define TEST_LED_Pin 0

/* 仅替换硬件时钟/GPIO/日志；接触任务和停止函数均提取自 main.c。 */
static uint32_t now_ms;
static volatile uint8_t ADC_AuxReadFaults; /* 辅助ADC边界；停止/接触业务仍使用真实源码。 */
static uint32_t HAL_GetTick(void) { return now_ms; }
static void HAL_GPIO_WritePin(int port, int pin, int value)
{ (void)port; (void)pin; (void)value; }
static float App_Abs(float value) { return value < 0 ? -value : value; }
static char log_text[8192];
static int log_printf(const char *format, ...)
{
    size_t used = strlen(log_text);
    va_list args;
    va_start(args, format);
    int count = vsnprintf(log_text + used, sizeof(log_text) - used, format, args);
    va_end(args);
    return count;
}
static struct { float velocity; uint16_t rawAngle; uint8_t result; } Sensor0, Sensor1, Sensor2, Sensor3;
static struct { float iqr, iq; } M0_Curs, M1_Curs;
static struct { float velocityTar; } M0_Vels, M1_Vels;
static struct { int8_t dir; float zeroAngle; } M0_Motor, M1_Motor;
#define printf log_printf
#include "contact_runtime_extracted.h"
#undef printf

static int checks, failures;
#define CHECK(cond, label) do { ++checks; if (!(cond)) { \
    ++failures; fprintf(stderr, "FAIL: %s\n", label); } } while (0)

static void reset_case(float target)
{
    M0_EncoderType = M1_EncoderType = ENCODER_AS5600;
    M0.mode = M1.mode = MODE_VELOCITY;
    M0.param.Vel = M1.param.Vel = target;
    M0_Curs.iqr = M1_Curs.iqr = 0.5f;
    M0_Vels.velocityTar = M1_Vels.velocityTar = target;
    M0_ContactDetected = M1_ContactDetected = 0;
    M0_ContactWasEnabled = M1_ContactWasEnabled = 0;
    M0_ContactLastSampleMs = M1_ContactLastSampleMs = 0;
    M0_ContactStartMs = M1_ContactStartMs = 0;
    Sensor0.velocity = Sensor1.velocity = 0;
    Sensor2.velocity = Sensor3.velocity = 0;
    M0_Motor.dir = 0;
    M1_Motor.dir = -1;
    M0_Motor.zeroAngle = 100;
    M1_Motor.zeroAngle = 200;
    now_ms = 100;
    log_text[0] = '\0';
}

static void run_contact(int motor)
{
    if (motor == 0) M0_ContactDetect_Task();
    else M1_ContactDetect_Task();
}

int main(void)
{
    const float targets[] = {1, -1, 5, -5, 10, -10};
    for (int motor = 0; motor < 2; ++motor) {
        for (unsigned i = 0; i < sizeof(targets)/sizeof(targets[0]); ++i) {
            reset_case(targets[i]);
            run_contact(motor);
            CHECK(M0.mode == MODE_VELOCITY && M1.mode == MODE_VELOCITY,
                  "startup grace must remain in force");
            now_ms = 1100;
            run_contact(motor);
            CHECK(M0.mode == MODE_OPEN && M1.mode == MODE_OPEN &&
                  M0.param.Ope == 0 && M1.param.Ope == 0,
                  "nonzero low-speed command with stopped shaft must stop BOTH motors");
            CHECK(M0_Curs.iqr == 0 && M1_Curs.iqr == 0 &&
                  M0_Vels.velocityTar == 0 && M1_Vels.velocityTar == 0,
                  "contact stop must clear both current and speed targets");
        }
        reset_case(0);
        CHECK(!M0_IsRunningCommand() && !M1_IsRunningCommand(),
              "zero command must not be treated as motion");
        M0_EncoderType = M1_EncoderType = ENCODER_DISABLED;
        M0.param.Vel = M1.param.Vel = 1;
        CHECK(!M0_IsRunningCommand() && !M1_IsRunningCommand(),
              "disabled encoder remains disabled");
        reset_case(100);
        Sensor0.velocity = Sensor1.velocity = -100 * (2 * PI) / 60;
        run_contact(motor);
        now_ms = 1100;
        run_contact(motor);
        CHECK(M0.mode == MODE_VELOCITY && M1.mode == MODE_VELOCITY,
              "reverse 100 RPM is above the contact threshold");
        Sensor0.velocity = Sensor1.velocity = -65 * (2 * PI) / 60;
        now_ms = 1200;
        run_contact(motor);
        CHECK(M0.mode == MODE_OPEN && M1.mode == MODE_OPEN,
              "reverse 65 RPM must stop both motors");
    }
    reset_case(1);
    Sensor1.velocity = -10.0f; /* -95 RPM，显示必须保留方向。 */
    now_ms = 2000;
    App_StatusPrint_Task();
    CHECK(strstr(log_text, "m1_rpm=-95") != NULL, "M1 telemetry must preserve signed speed");
    CHECK(strstr(log_text, "MOTORDBG") && strstr(log_text, "dir0=0 dir1=-1") &&
          strstr(log_text, "enc0=0 enc1=0") && strstr(log_text, "as0=0 as1=0") &&
          strstr(log_text, "target0_x100=100 target1_x100=100") &&
          strstr(log_text, "guard0=1 guard1=1"),
          "diagnostics must expose calibration, encoder, target and protection gate");
    ADC_AuxReadFaults = 1; now_ms = 2500; log_text[0] = '\0';
    App_StatusPrint_Task();
    CHECK(strstr(log_text, "ADC_AUX_FAULT mask=0x01") != NULL, "ADC fault must be visible from main");
    now_ms = 3000; log_text[0] = '\0'; App_StatusPrint_Task();
    CHECK(strstr(log_text, "ADC_AUX_FAULT") == NULL, "unchanged ADC fault must not flood logs");
    ADC_AuxReadFaults = 3; now_ms = 3500; log_text[0] = '\0'; App_StatusPrint_Task();
    CHECK(strstr(log_text, "ADC_AUX_FAULT mask=0x03") != NULL, "new ADC fault bit must be reported");
    printf("Speed/contact regression: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
