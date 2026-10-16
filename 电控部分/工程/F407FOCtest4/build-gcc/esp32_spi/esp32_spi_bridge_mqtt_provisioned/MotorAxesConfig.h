#ifndef MOTOR_AXES_CONFIG_H
#define MOTOR_AXES_CONFIG_H
/* 本项目交付配置：只使用竖直M1。0仅用于旧双轴主机回归/显式兼容构建。 */
#ifndef FOC_M1_ONLY
#define FOC_M1_ONLY 1
#endif
#if FOC_M1_ONLY != 0 && FOC_M1_ONLY != 1
#error "FOC_M1_ONLY must be 0 or 1"
#endif
static inline int motor_axes_fault(int m0_fault, int m1_fault)
{
    return m1_fault || (!FOC_M1_ONLY && m0_fault);
}
#endif
