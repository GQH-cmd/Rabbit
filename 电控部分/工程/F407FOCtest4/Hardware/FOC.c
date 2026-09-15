/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  FOC.c
 * Description      :  FOC algorithm base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com
 * DATE:         May 24rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FOC.h"

/* Define -------------------------------------------------------------------*/
#define _2PI       		2 * PI
#define _3PI_2     		4.71238898f
#define _1_SQRT3 		0.57735027f
#define _2_SQRT3   		1.15470054f

#define M0_PolePairs	7
#define M1_PolePairs	7
#define Udc				12.0f

/* Private Variables --------------------------------------------------------*/
PIDController M0_iq_pid;
PIDController M0_id_pid;
PIDController M1_iq_pid;
PIDController M1_id_pid;
PIDController M0_vel_pid;
PIDController M1_vel_pid;
PIDController M0_pos_pid;
PIDController M1_pos_pid;

struct MovingAverageFilter iq0_avg_filter;
struct MovingAverageFilter iq1_avg_filter;
struct MovingAverageFilter vel0_avg_filter;
struct MovingAverageFilter vel1_avg_filter;
struct MovingAverageFilter pos0_avg_filter;
struct MovingAverageFilter pos1_avg_filter;

float32_t zero = 0.0f;
static uint8_t is_in_vel0_loop = 0;
static uint8_t is_in_vel1_loop = 0;

/* 外部变量声明 */
extern uint8_t M0_EncoderType; // 从main.c导入编码器类型变量
extern uint8_t M1_EncoderType;

/* Code -------------------------------------------------------------------*/
/**
 * @brief  FOC控制系统初始化
 * @param  None
 * @retval None
 */
void FOC_Init(void)
{
	Motor_Init(&M0_Motor, 0, M0_PolePairs); 		// motorNum 设置为 0
	Motor_Init(&M1_Motor, 1, M1_PolePairs);			// motorNum 设置为 1
	
	CurLoopInit(&M0_Curs);
	CurLoopInit(&M1_Curs);
	
	VelLoopInit(&M0_Vels);
	VelLoopInit(&M1_Vels);
	
	PosLoopInit(&M0_Poses);
	PosLoopInit(&M1_Poses);
		
	if (M0_EncoderType == 0) { // AS5600
		PID_init(&M0_iq_pid, 1.0f, 0.2f, 0.0f, -7.0f, 7.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M0_id_pid, 1.0f, 4.0f, 0.0f, -7.0f, 7.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M0_vel_pid, 3.5f, 1.0f, 0.0f, -5.0f, 5.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M0_pos_pid, 0.3f, 0.2f, 0.01f, -300.0f, 300.0f, -3.0f, 3.0f);	/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
	} else { // AS5047
		PID_init(&M0_iq_pid, 2.0f, 0.3f, 0.0f, -7.0f, 7.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M0_id_pid, 1.0f, 4.0f, 0.0f, -7.0f, 7.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M0_vel_pid, 3.0f, 1.0f, 0.0f, -5.0f, 5.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M0_pos_pid, 1.5f, 0.8f, 0.01f, -300.0f, 300.0f, -3.0f, 3.0f);	/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */

	}

	if (M1_EncoderType == 0) { // AS5600
		PID_init(&M1_iq_pid, 1.0f, 0.2f, 0.0f, -7.0f, 7.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M1_id_pid, 1.0f, 4.0f, 0.0f, -7.0f, 7.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M1_vel_pid, 3.5f, 1.0f, 0.0f, -5.0f, 5.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		//PID_init(&M1_vel_pid, 1.0f, 0.2f, 0.0f, -5.0f, 5.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M1_pos_pid, 0.4f, 0.2f, 0.01f, -300.0f, 300.0f, -3.0f, 3.0f);	/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
	} else { // AS5047
		PID_init(&M1_iq_pid, 2.0f, 0.3f, 0.0f, -7.0f, 7.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M1_id_pid, 1.0f, 4.0f, 0.0f, -7.0f, 7.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M1_vel_pid, 3.0f, 1.0f, 0.0f, -5.0f, 5.0f, -3.0f, 3.0f);		/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */
		PID_init(&M1_pos_pid, 1.5f, 0.8f, 0.01f, -300.0f, 300.0f, -3.0f, 3.0f);	/* Kp  Ki  Kd  outputMin  outputMax  integralMin  integralMax */

	}
	
	/* 开启电机电源 */
	Motor_Power_Init(1); // 默认电源模式1，通过板子上的dcdc供电。改为2后直接由外接电源供电
	
	/* adc校准 */
	Current_Init();
	
	/* 零电角度初始化 */
	Check_Zero_Angle();

	/* 判断电机方向 */
	Check_Dir();
	
	/* 滤波函数初始化 */
//	MovingAvg_Init(&iq0_avg_filter, 3);
//	MovingAvg_Init(&iq1_avg_filter, 3);
//	MovingAvg_Init(&vel0_avg_filter, 5);
//	MovingAvg_Init(&vel1_avg_filter, 5);
//	MovingAvg_Init(&pos0_avg_filter, 30);
}

/**
 * @brief  电机零电角度初始化
 * @param  None
 * @retval None
 */
void Check_Zero_Angle(void)
{
	/* 给d轴一个值 */
	SetSVPWM(M0_Motor.motorNum, 0.0f, 3.0f, 0.0f);
	SetSVPWM(M1_Motor.motorNum, 0.0f, 3.0f, 0.0f);

	/* 等待电机稳定 */
	HAL_Delay(1000);
	
	/* 根据编码器类型读取角度值 */
	if (M0_EncoderType == 0)
	{ // AS5600
		I2C1_AS5600_GetAngle();
		M0_Motor.zeroAngle = Sensor0.rawAngle;
	} else
	{ // AS5047
		SPI2_AS5047P_GetAngle();
		M0_Motor.zeroAngle = Sensor2.rawAngle;
	}
	
	if (M1_EncoderType == 0)
	{ // AS5600
		I2C2_AS5600_GetAngle();
		M1_Motor.zeroAngle = Sensor1.rawAngle;
	} else
	{ // AS5047
		SPI3_AS5047P_GetAngle();
		M1_Motor.zeroAngle = Sensor3.rawAngle;
	}
}

/**
 * @brief  判断电机旋转方向
 * @param  None
 * @retval None
 */
void Check_Dir(void)
{
	static float32_t angle = 0.0f;
	static float32_t m_angleTemp[2] = {0};
	short m_count[2] = {0};	 // 记录机械角度的变化次数
	int8_t* dirs[] = {&M0_Motor.dir, &M1_Motor.dir};
	
	// 旋转电机进行方向检测
	for (int i = 0; i < 100; i++) 
	{
		// 根据编码器类型读取角度
		if (M0_EncoderType == 0)
		{ // AS5600
			I2C1_AS5600_GetAngle();
		} else
		{ // AS5047
			SPI2_AS5047P_GetAngle();
		}
		
		if (M1_EncoderType == 0)
		{ // AS5600
			I2C2_AS5600_GetAngle();
		} else 
		{ // AS5047
			SPI3_AS5047P_GetAngle();
		}

		angle += 0.1f;
		
		// 设置双电机SVPWM
		for (int m = 0; m < 2; m++)
		{
			SetSVPWM(m, 3.0f, 0.0f, angle);
		}
	
		// 处理双电机数据
		float32_t angles[2];
		
		// 根据编码器类型获取角度
		if (M0_EncoderType == 0)
		{ // AS5600
			angles[0] = Normalize_Angle(Sensor0.angle);
		} else { // AS5047
			angles[0] = Normalize_Angle(Sensor2.angle);
		}
		
		if (M1_EncoderType == 0)
		{ // AS5600
			angles[1] = Normalize_Angle(Sensor1.angle);
		} else { // AS5047
			angles[1] = Normalize_Angle(Sensor3.angle);
		}
	
		for (int m = 0; m < 2; m++)
		{
			if (angles[m] > m_angleTemp[m])
			{
				m_count[m]++;
			} else if (angles[m] < m_angleTemp[m])
			{
				m_count[m]--;
			}
			m_angleTemp[m] = angles[m];
		}
		
		HAL_Delay(1);
	}
	HAL_Delay(20);
	
	// 反方向旋转电机进行复位
	for (int i = 0; i < 100; i++)
	{
		angle -= 0.1f;
		for (int m = 0; m < 2; m++)
		{
			SetSVPWM(m, 2.0f, 0.0f, angle);
		}
		HAL_Delay(1);
	}
	
	// 方向判定
	for (int m = 0; m < 2; m++)
	{
		if (m_count[m] > 50)
		{
			*dirs[m] = 1;
		} else if (m_count[m] < -50)
		{
			*dirs[m] = -1;
		}
	}
	SetSVPWM(0, 0.0f, 0.0f, 0.0f);
	SetSVPWM(1, 0.0f, 0.0f, 0.0f);
}

/**
 * @brief  归一化角度值到0-2PI范围
 * @param  angle: 输入角度值(弧度)
 * @retval 归一化后的角度值(弧度，范围0-2PI)
 */
float32_t Normalize_Angle(float32_t angle)
{
	angle = fmodf(angle, 2 * PI);  // 取模运算
	return angle < 0 ? angle + 2 * PI : angle; // 处理负角度
}

/**
 * @brief  归一化角度值到0-360度范围
 * @param  angle: 输入角度值(度)
 * @retval 归一化后的角度值(度，范围0-360)
 */
float32_t Normalize_DegAngle(float32_t angle)
{
	angle = fmodf(angle, 360);  // 取模运算
	return angle < 0 ? angle + 360 : angle; // 处理负角度
}

/**
 * @brief  获取电机0的电角度
 * @param  rawAngle: 原始机械角度
 * @retval 电角度值(弧度)
 */
float32_t M0_GetElectric_Angle(float32_t rawAngle)
{
	float32_t eAngle;
	rawAngle = rawAngle - M0_Motor.zeroAngle;
	if (M0_EncoderType == 0)
	{ // AS5600
		eAngle = Normalize_Angle(rawAngle * _2PI / 4096 * M0_PolePairs * M0_Motor.dir);
	} else 
	{ // AS5047
		eAngle = Normalize_Angle(rawAngle * _2PI / 16384 * M0_PolePairs * M0_Motor.dir);
	}

	return eAngle;
}

/**
 * @brief  获取电机1的电角度
 * @param  rawAngle: 原始机械角度
 * @retval 电角度值(弧度)
 */
float32_t M1_GetElectric_Angle(float32_t rawAngle)
{
	float32_t eAngle;
	rawAngle = rawAngle + M1_Motor.zeroAngle;
	if (M1_EncoderType == 0)
	{ // AS5600
		eAngle = Normalize_Angle(rawAngle * _2PI / 4096 * M1_PolePairs * M1_Motor.dir);
	} else 
	{ // AS5047
		eAngle = Normalize_Angle(rawAngle * _2PI / 16384 * M1_PolePairs * M1_Motor.dir);
	}

	return eAngle;
}

/**
 * @brief  Clark变换(三相电流到alpha-beta坐标系)
 * @param  iu: U相电流
 * @param  iv: V相电流
 * @param  iw: W相电流
 * @param  i_alpha: alpha轴电流指针
 * @param  i_beta: beta轴电流指针
 * @retval None
 */
void Clarke_Transform(float32_t iu, float32_t iv, float32_t iw, float32_t* i_alpha, float32_t* i_beta)
{
    /* 采用等幅值变换
     * 变换矩阵：
     * [ α ] = [     1             0     ] [ u ]
     * [ β ] = [ -1/sqrt(3)   -2/sqrt(3) ] [ V ]
     */
    *i_alpha = iu;
	*i_beta  = -(iu + 2 * iw) * _1_SQRT3;
}

/**
 * @brief  Park变换(alpha-beta坐标系到d-q坐标系)
 * @param  ialpha: alpha轴电流
 * @param  ibeta: beta轴电流
 * @param  eAngle: 电角度(弧度)
 * @param  id: d轴电流指针
 * @param  iq: q轴电流指针
 * @retval None
 */
void Park_Transform(float32_t ialpha, float32_t ibeta, float32_t eAngle, float32_t* id, float32_t* iq)
{
	float32_t sinTheta = arm_sin_f32_tab(eAngle);
	float32_t cosTheta = arm_cos_f32_tab(eAngle);
    /* 变换矩阵：
     * [ Id ] = [ cosθ    sinθ ] [ α ]
     * [ Iq ] = [ sinθ   -cosθ ] [ β ]
     */
	*id = ialpha * cosTheta + ibeta * sinTheta;
    *iq = ialpha * (-sinTheta) + ibeta * cosTheta;
}

/**
 * @brief  电机0开环控制
 * @param  Target: 目标转速
 * @retval None
 */
void M0_OpeLoop(float32_t Target)
{
	float32_t eAngle;
	
	// 根据编码器类型获取电角度
	if (M0_EncoderType == 0)
	{ // AS5600
		eAngle = M0_GetElectric_Angle(Sensor0.rawAngle);
	} else 
	{ // AS5047
		eAngle = M0_GetElectric_Angle(Sensor2.rawAngle);
	}
	
	// 更新位置和速度信息
	FOC_M0_Velocity_Update();
	
	Clarke_Transform(M0_Curs.iu, M0_Curs.iv, M0_Curs.iw, &M0_Curs.ialpha, &M0_Curs.ibeta);
 	Park_Transform(M0_Curs.ialpha, M0_Curs.ibeta, eAngle, &M0_Curs.id, &M0_Curs.iq);
	SetSVPWM(M0_Motor.motorNum, Target, 0.0f, eAngle);
}

/**
 * @brief  电机1开环控制
 * @param  Target: 目标转速
 * @retval None
 */
void M1_OpeLoop(float32_t Target)
{
	float32_t eAngle;
	
	// 根据编码器类型获取电角度
	if (M1_EncoderType == 0) 
	{		// AS5600
		eAngle = M1_GetElectric_Angle(Sensor1.rawAngle);
	} else 
	{ // AS5047
		eAngle = M1_GetElectric_Angle(Sensor3.rawAngle);
	}
	
	// 更新位置和速度信息
	FOC_M1_Velocity_Update();
	
	Clarke_Transform(M1_Curs.iu, M1_Curs.iv, M1_Curs.iw, &M1_Curs.ialpha, &M1_Curs.ibeta);
 	Park_Transform(M1_Curs.ialpha, M1_Curs.ibeta, eAngle, &M1_Curs.id, &M1_Curs.iq);
	SetSVPWM(M1_Motor.motorNum, Target, 0.0f, eAngle);
}

/**
 * @brief  电机0电流环控制
 * @param  None
 * @retval None
 */
void M0_CurLoop(void)
{
	if (!is_in_vel0_loop)
	{
		// 更新位置和速度信息
		FOC_M0_Velocity_Update();
    }
	
	float32_t eAngle;
	
	// 根据编码器类型获取电角度
	if (M0_EncoderType == 0)
	{ // AS5600
		eAngle = M0_GetElectric_Angle(Sensor0.rawAngle);
	} else 
	{ // AS5047
		eAngle = M0_GetElectric_Angle(Sensor2.rawAngle);
	}
    
	Clarke_Transform(M0_Curs.iu, M0_Curs.iv, M0_Curs.iw, &M0_Curs.ialpha, &M0_Curs.ibeta);
 	Park_Transform(M0_Curs.ialpha, M0_Curs.ibeta, eAngle, &M0_Curs.id, &M0_Curs.iq);

	M0_Curs.Uq = PID(&M0_iq_pid, M0_Curs.iqr, M0_Curs.iq);
	M0_Curs.Ud = PID(&M0_id_pid, M0_Curs.idr, M0_Curs.id);
	SetSVPWM(M0_Motor.motorNum, M0_Curs.Uq, M0_Curs.Ud, eAngle);
}

/**
 * @brief  电机1电流环控制
 * @param  None
 * @retval None
 */
void M1_CurLoop(void)
{
	if (!is_in_vel1_loop)
	{
		// 更新位置和速度信息
		FOC_M1_Velocity_Update();
    }

	float32_t eAngle;
	
	// 根据编码器类型获取电角度
	if (M1_EncoderType == 0)
	{ // AS5600
		eAngle = M1_GetElectric_Angle(Sensor1.rawAngle);
	} else 
	{ // AS5047
		eAngle = M1_GetElectric_Angle(Sensor3.rawAngle);
	}
    
	Clarke_Transform(M1_Curs.iu, M1_Curs.iv, M1_Curs.iw, &M1_Curs.ialpha, &M1_Curs.ibeta);
 	Park_Transform(M1_Curs.ialpha, M1_Curs.ibeta, eAngle, &M1_Curs.id, &M1_Curs.iq);
	
	M1_Curs.Uq = PID(&M1_iq_pid, M1_Curs.iqr, M1_Curs.iq);
	M1_Curs.Ud = PID(&M1_id_pid, M1_Curs.idr, M1_Curs.id);
	SetSVPWM(M1_Motor.motorNum, M1_Curs.Uq, M1_Curs.Ud, eAngle);
}

/**
 * @brief  电机0速度环控制(1/10电流环频率)
 * @param  None
 * @retval None
 */
void M0_VelLoop(void)
{
	 is_in_vel0_loop = 1;
	
    // 根据目标速度动态调整预分频器
    int newPrescaler = 9;
    if (M0_Vels.velLoopPrescaler != newPrescaler)
	{
        M0_Vels.velLoopPrescaler = newPrescaler;
        M0_Vels.cnt = 0;
    }

    if (M0_Vels.cnt == M0_Vels.velLoopPrescaler)
	{
        M0_Vels.cnt = 0;
		
		// 更新位置和速度信息
		FOC_M0_Velocity_Update();
		
        // 计算速度环输出
		float32_t velocity_feedback;
		if (M0_EncoderType == 0)
		{ // AS5600
			velocity_feedback = Sensor0.velocity / (2 * PI); // rad/s → RPS
		} else 
		{ // AS5047
			velocity_feedback = Sensor2.velocity / (2 * PI); // rad/s → RPS
		}
		
        M0_Vels.velocity = PID(&M0_vel_pid, M0_Vels.velocityTar / 60.0f, velocity_feedback) * M0_Motor.dir;
		
		// 速度环 PID 输出作为电流目标
        M0_Curs.iqr = M0_Vels.velocity;
    }
    M0_Vels.cnt++;
    M0_CurLoop(); // 电流环控制
	
	is_in_vel0_loop = 0;
}

/**
 * @brief  电机1速度环控制(1/10电流环频率)
 * @param  None
 * @retval None
 */
void M1_VelLoop(void)
{
	is_in_vel1_loop = 1;
	
    // 根据目标速度动态调整预分频器
    int newPrescaler = 9;
    if (M1_Vels.velLoopPrescaler != newPrescaler)
	{
        M1_Vels.velLoopPrescaler = newPrescaler;
        M1_Vels.cnt = 0;
    }

    if (M1_Vels.cnt == M1_Vels.velLoopPrescaler)
	{
        M1_Vels.cnt = 0;
		
		// 更新位置和速度信息
		FOC_M1_Velocity_Update();
		
        // 计算速度环输出
		float32_t velocity_feedback;
		if (M1_EncoderType == 0)
		{ // AS5600
			velocity_feedback = Sensor1.velocity / (2 * PI); // rad/s → RPS
		} else 
		{ // AS5047
			velocity_feedback = Sensor3.velocity / (2 * PI); // rad/s → RPS
		}
		
        M1_Vels.velocity = PID(&M1_vel_pid, M1_Vels.velocityTar / 60.0f, velocity_feedback) * -M1_Motor.dir;
		
		// 速度环 PID 输出作为电流目标
        M1_Curs.iqr = M1_Vels.velocity;
    }
    M1_Vels.cnt++;
    M1_CurLoop(); // 电流环控制
	
	is_in_vel1_loop = 0;
}

/**
 * @brief  电机0位置环控制(1/20电流环频率)
 * @param  None
 * @retval None
 */
void M0_PosLoop(void)
{
    if (M0_Poses.cnt == M0_Poses.posLoopPrescaler)
	{
        M0_Poses.cnt = 0;
		
		// 更新位置和速度信息
		FOC_M0_Position_Update();

		/* 计算位置误差 */
		float32_t error;
		if (M0_EncoderType == 0)
		{ // AS5600
			error = M0_Poses.positionTar - Sensor0.absDegAngle;
		} else
		{ // AS5047
			error = M0_Poses.positionTar * 4.0f - Sensor2.absDegAngle;
		}
		
        /* 位置环 PID 输出作为速度目标 */
        float32_t velocity_target = PID(&M0_pos_pid, error, 0.0f);
        /* 传递速度目标到速度环 */
        M0_Vels.velocityTar = velocity_target;
    }
    M0_Poses.cnt++;
	M0_VelLoop(); // 速度环控制
}

/**
 * @brief  电机1位置环控制(1/20电流环频率)
 * @param  None
 * @retval None
 */
void M1_PosLoop(void)
{
    if (M1_Poses.cnt == M1_Poses.posLoopPrescaler)
	{
        M1_Poses.cnt = 0;
		
		// 更新位置和速度信息
		FOC_M1_Position_Update();

		/* 计算位置误差 */
		float32_t error;
		if (M1_EncoderType == 0)
		{ // AS5600
			error = M1_Poses.positionTar - Sensor1.absDegAngle;
		} else
		{ // AS5047
			error = M1_Poses.positionTar * 4.0f - Sensor3.absDegAngle;
		}
		
        /* 位置环 PID 输出作为速度目标 */
        float32_t velocity_target = PID(&M1_pos_pid, error, 0.0f);
        /* 传递速度目标到速度环 */
        M1_Vels.velocityTar = velocity_target;
    }
    M1_Poses.cnt++;
	M1_VelLoop(); // 速度环控制
}

/**
 * @brief  计算SVPWM调制信号
 * @param  Uq: q轴电压
 * @param  Ud: d轴电压
 * @param  eAngle: 电角度(弧度)
 * @retval SVPWM输出结构体
 */
SVPWM_Outputs CalculateSVPWM(float32_t Uq, float32_t Ud, float32_t eAngle)
{
	uint8_t sector;
	// 预计算常量，避免重复计算
	static const float32_t Ts = 1.0f;
	static const float32_t SQRT3_OVER_2 = 0.866025388f;
	static const float32_t SQRT3 = 1.73205078f;
	static const float32_t TsDivUdc = 1.0f / Udc; // 1.0 / 12.0 = 0.0833333
	
	float32_t sinAngle, cosAngle;
	float32_t Ualpha, Ubeta;
	float32_t Tx, Ty, f_temp;
	float32_t Ta, Tb, Tc;
	float32_t Tcmp1, Tcmp2, Tcmp3;
	SVPWM_Outputs outputs;
	
	// 使用查表法计算sin和cos，提高速度
	sinAngle = arm_sin_f32_tab(eAngle);
	cosAngle = arm_cos_f32_tab(eAngle);

	// Park逆变换，计算α-β坐标系下的电压
	Ualpha = Ud * cosAngle - Uq * sinAngle;
	Ubeta = Ud * sinAngle + Uq * cosAngle;
	
	// 扇区判断
	sector = 0;
	if (Ubeta > 0.0f) 
		sector = 1;
	if ((SQRT3 * Ualpha - Ubeta) > 0.0f) 
		sector += 2;
	if ((-SQRT3 * Ualpha - Ubeta) > 0.0f) 
		sector += 4; 

	// 基于扇区计算Tx和Ty，使用预计算的常量
	const float32_t U_TsDivUdc = TsDivUdc * Ts; // 因为Ts是1.0，这里实际上就是TsDivUdc
	
	switch (sector) 
	{
		case 1: // 0b001
			Tx = (-1.5f * Ualpha + SQRT3_OVER_2 * Ubeta) * U_TsDivUdc;
			Ty = (1.5f * Ualpha + SQRT3_OVER_2 * Ubeta) * U_TsDivUdc;
			break;

		case 2: // 0b010
			Tx = (1.5f * Ualpha + SQRT3_OVER_2 * Ubeta) * U_TsDivUdc;
			Ty = -SQRT3 * Ubeta * U_TsDivUdc;
			break;

		case 3: // 0b011
			Tx = -(-1.5f * Ualpha + SQRT3_OVER_2 * Ubeta) * U_TsDivUdc;
			Ty = SQRT3 * Ubeta * U_TsDivUdc;
			break;

		case 4: // 0b100
			Tx = -SQRT3 * Ubeta * U_TsDivUdc;
			Ty = (-1.5f * Ualpha + SQRT3_OVER_2 * Ubeta) * U_TsDivUdc;
			break;

		case 5: // 0b101
			Tx = SQRT3 * Ubeta * U_TsDivUdc;
			Ty = -(1.5f * Ualpha + SQRT3_OVER_2 * Ubeta) * U_TsDivUdc;
			break;

		default: // case 6 (0b110) 和 case 0
			Tx = -(1.5f * Ualpha + SQRT3_OVER_2 * Ubeta) * U_TsDivUdc;
			Ty = -(-1.5f * Ualpha + SQRT3_OVER_2 * Ubeta) * U_TsDivUdc;
			break;
	}
	
	/* 归一化 */
	f_temp = Tx + Ty;
	if (f_temp > Ts)
	{
		const float32_t inv_f_temp = 1.0f / f_temp;
		Tx *= inv_f_temp;
		Ty *= inv_f_temp;
	}

	Ta = (Ts - Tx - Ty) * 0.25f;
	Tb = Ta + Tx * 0.5f;
	Tc = Tb + Ty * 0.5f;
	
	// 使用查表优化扇区到PWM输出的映射
	static const uint8_t pwm_pattern[6][3] =
	{
		{1, 0, 2}, // 扇区1: Tb, Ta, Tc
		{0, 2, 1}, // 扇区2: Ta, Tc, Tb
		{0, 1, 2}, // 扇区3: Ta, Tb, Tc
		{2, 1, 0}, // 扇区4: Tc, Tb, Ta
		{2, 0, 1}, // 扇区5: Tc, Ta, Tb
		{1, 2, 0}  // 扇区6: Tb, Tc, Ta
	};
	
	// 使用sector作为索引，避免多个分支判断
	if (sector > 0 && sector <= 6)
	{
		const float32_t pwm_values[3] = {Ta, Tb, Tc};
		Tcmp1 = pwm_values[pwm_pattern[sector-1][0]];
		Tcmp2 = pwm_values[pwm_pattern[sector-1][1]];
		Tcmp3 = pwm_values[pwm_pattern[sector-1][2]];
	} else
	{
		// 处理错误情况
		Tcmp1 = Tcmp2 = Tcmp3 = 0.5f;
	}
	
	// 乘以4800作为输出 (中央对齐模式，20kHZ)
	static const float32_t PWM_SCALE = 4800.0f;
	outputs.Tcmp1 = Tcmp1 * PWM_SCALE;
	outputs.Tcmp2 = Tcmp2 * PWM_SCALE;
	outputs.Tcmp3 = Tcmp3 * PWM_SCALE;
	
	return outputs;
}

/**
 * @brief  设置电机SVPWM信号
 * @param  motorNum: 电机编号(0或1)
 * @param  Uq: q轴电压
 * @param  Ud: d轴电压
 * @param  eAngle: 电角度(弧度)
 * @retval None
 */
void SetSVPWM(float32_t motorNum, float32_t Uq, float32_t Ud, float32_t eAngle)
{
	SVPWM_Outputs outputs = CalculateSVPWM(Uq, Ud, eAngle);
	// 电机0使用TIM1
	if (motorNum == 0)
	{
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, outputs.Tcmp1);
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, outputs.Tcmp2);
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, outputs.Tcmp3);
	}
	// 电机1使用TIM8
	else if (motorNum == 1)
	{
		__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, outputs.Tcmp1);
		__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, outputs.Tcmp2);
		__HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, outputs.Tcmp3);
	}
}

/**
 * @brief  M0电机速度更新
 * @param  None
 * @retval None
 */
void FOC_M0_Velocity_Update(void)
{
	if (M0_EncoderType == 0)
	{ // AS5600
		AS5600_M0_GetVelocity();
	} else
	{ // AS5047
		AS5047_M0_GetVelocity();
	}
}

/**
 * @brief  M1电机速度更新
 * @param  None
 * @retval None
 */
void FOC_M1_Velocity_Update(void)
{
	if (M1_EncoderType == 0)
	{ // AS5600
		AS5600_M1_GetVelocity();
	} else
	{ // AS5047
		AS5047_M1_GetVelocity();
	}
}

/**
 * @brief  M0电机位置更新
 * @param  None
 * @retval None
 */
void FOC_M0_Position_Update(void)
{
	if (M0_EncoderType == 0)
	{ // AS5600
		M0_I2C_UpdatePosition(&Sensor0, M0_Motor.zeroAngle);
	} else
	{ // AS5047
		M0_SPI_UpdatePosition();
	}
}

/**
 * @brief  M1电机位置更新
 * @param  None
 * @retval None
 */
void FOC_M1_Position_Update(void)
{
	if (M1_EncoderType == 0)
	{ // AS5600
		M1_I2C_UpdatePosition(&Sensor1, M1_Motor.zeroAngle);
	} else { // AS5047
		M1_SPI_UpdatePosition();
	}
}

