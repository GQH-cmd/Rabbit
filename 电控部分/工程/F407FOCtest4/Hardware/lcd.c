/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name        :  lcd.c
 * Description      :  lcd driver base on stm32f407
 ******************************************************************************
 * @attention
 *
 * COPYRIGHT:    Copyright (c) 2025  2710614006@qq.com(based on LiBaifeng，13104312598)
 * DATE:         FEB 03rd, 2025
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "lcd.h"

/* Define -------------------------------------------------------------------*/
#define LCD_TOTAL_BUF_SIZE	(240*135*2)
#define LCD_Buf_Size 		648

/* Private Variables --------------------------------------------------------*/
static uint8_t lcd_buf[LCD_Buf_Size];

uint16_t	POINT_COLOR = WHITE;	//画笔颜色	默认为黑色
uint16_t	BACK_COLOR 	= BLACK;	//背景颜色	默认为白色

/* Code --------------------------------------------------------------------*/
/**
 * @brief	LCD控制接口初始化
 *
 * @param   void
 *
 * @return  void
 */

/**
 * @brief	LCD底层SPI发送数据函数
 *
 * @param   data	数据的起始地址
 * @param   size	发送数据大小
 *
 * @return  void
 */
uint8_t	SPI1_WriteByte(uint8_t* data, uint16_t size);
static void LCD_SPI_Send(uint8_t *data, uint16_t size)
{
	HAL_SPI_Transmit(&hspi1, data, size, 0xff);
}


/**
 * @brief	写命令到LCD
 *
 * @param   cmd		需要发送的命令
 *
 * @return  void
 */
static void LCD_Write_Cmd(uint8_t cmd)
{
    LCD_DC(0);

    LCD_SPI_Send(&cmd, 1);
}

/**
 * @brief	写数据到LCD
 *
 * @param   cmd		需要发送的数据
 *
 * @return  void
 */
static void LCD_Write_Data(uint8_t data)
{
    LCD_DC(1);

    LCD_SPI_Send(&data, 1);
}

/**
 * @brief	写半个字的数据到LCD
 *
 * @param   cmd		需要发送的数据
 *
 * @return  void
 */
void LCD_Write_HalfWord(const uint16_t da)
{
    uint8_t data[2] = {0};

    data[0] = da >> 8;
    data[1] = da;

    LCD_DC(1);
    LCD_SPI_Send(data, 2);
}

/**
 * 设置数据写入LCD缓存区域
 *
 * @param   x1,y1	起点坐标
 * @param   x2,y2	终点坐标
 *
 * @return  void
 */
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* Column Address */
	LCD_Write_Cmd(0x2A);
    LCD_Write_Data((Width_Offset + x1) >> 8);
    LCD_Write_Data((Width_Offset + x1));
    LCD_Write_Data((Width_Offset + x2) >> 8);
    LCD_Write_Data((Width_Offset + x2));


		/* Row Address */
    LCD_Write_Cmd(0x2B);
    LCD_Write_Data((Height_Offset + y1) >> 8);
    LCD_Write_Data((Height_Offset + y1));
    LCD_Write_Data((Height_Offset + y2) >> 8);
    LCD_Write_Data((Height_Offset + y2));

    LCD_Write_Cmd(0x2C);
}

/**
 * 以一种颜色清空LCD屏
 *
 * @param   color	清屏颜色
 *
 * @return  void
 */
void LCD_Clear(uint16_t color)
{
    uint16_t i, j;
    uint8_t data[2] = {0};

    data[0] = color >> 8;
    data[1] = color;

//    LCD_Address_Set(40, 53, 40 + LCD_Width - 1, 53 + LCD_Height - 1);
		LCD_Address_Set(0, 0, LCD_Width - 1, LCD_Height - 1);

//    LCD_Address_Set(0, 0, 240 - 1, 135 - 1);

    for(j = 0; j < LCD_Buf_Size / 2; j++)
    {
        lcd_buf[j * 2] =  data[0];
        lcd_buf[j * 2 + 1] =  data[1];
    }

    LCD_DC(1);

    for(i = 0; i < (LCD_TOTAL_BUF_SIZE / LCD_Buf_Size); i++)
    {
        LCD_SPI_Send(lcd_buf, LCD_Buf_Size);
    }
}

/**
 * 用一个颜色填充整个区域
 *
 * @param   x_start,y_start     起点坐标
 * @param   x_end,y_end			终点坐标
 * @param   color       		填充颜色
 *
 * @return  void
 */
void LCD_Fill(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color)
{
    uint16_t i = 0;
    uint32_t size = 0, size_remain = 0;

    size = (x_end - x_start + 1) * (y_end - y_start + 1) * 2;

    if(size > LCD_Buf_Size)
    {
        size_remain = size - LCD_Buf_Size;
        size = LCD_Buf_Size;
    }

    LCD_Address_Set(x_start, y_start, x_end, y_end);

    while(1)
    {
        for(i = 0; i < size / 2; i++)
        {
            lcd_buf[2 * i] = color >> 8;
            lcd_buf[2 * i + 1] = color;
        }

        LCD_DC(1);
        LCD_SPI_Send(lcd_buf, size);

        if(size_remain == 0)
            break;

        if(size_remain > LCD_Buf_Size)
        {
            size_remain = size_remain - LCD_Buf_Size;
        }

        else
        {
            size = size_remain;
            size_remain = 0;
        }
    }
}

/**
 * 画点函数
 *
 * @param   x,y		画点坐标
 *
 * @return  void
 */
void LCD_Draw_Point(uint16_t x, uint16_t y)
{
    LCD_Address_Set(x, y, x, y);
    LCD_Write_HalfWord(POINT_COLOR);
}

/**
 * 画点带颜色函数
 *
 * @param   x,y		画点坐标
 *
 * @return  void
 */
void LCD_Draw_ColorPoint(uint16_t x, uint16_t y,uint16_t color)
{
    LCD_Address_Set(x, y, x, y);
    LCD_Write_HalfWord(color);
}



/**
 * @brief	画线函数(直线、斜线)
 *
 * @param   x1,y1	起点坐标
 * @param   x2,y2	终点坐标
 *
 * @return  void
 */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;
    uint32_t i = 0;

    if(y1 == y2)
    {
        /*快速画水平线*/
        LCD_Address_Set(x1, y1, x2, y2);

        for(i = 0; i < x2 - x1; i++)
        {
            lcd_buf[2 * i] = POINT_COLOR >> 8;
            lcd_buf[2 * i + 1] = POINT_COLOR;
        }

        LCD_DC(1);
        LCD_SPI_Send(lcd_buf, (x2 - x1) * 2);
        return;
    }

    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if(delta_x > 0)incx = 1;

    else if(delta_x == 0)incx = 0;

    else
    {
        incx = -1;
        delta_x = -delta_x;
    }

    if(delta_y > 0)incy = 1;

    else if(delta_y == 0)incy = 0;

    else
    {
        incy = -1;
        delta_y = -delta_y;
    }

    if(delta_x > delta_y)distance = delta_x;

    else distance = delta_y;

    for(t = 0; t <= distance + 1; t++)
    {
        LCD_Draw_Point(row, col);
        xerr += delta_x ;
        yerr += delta_y ;

        if(xerr > distance)
        {
            xerr -= distance;
            row += incx;
        }

        if(yerr > distance)
        {
            yerr -= distance;
            col += incy;
        }
    }
}

/**
 * @brief	画一个矩形
 *
 * @param   x1,y1	起点坐标
 * @param   x2,y2	终点坐标
 *
 * @return  void
 */
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    LCD_DrawLine(x1, y1, x2, y1);
    LCD_DrawLine(x1, y1, x1, y2);
    LCD_DrawLine(x1, y2, x2, y2);
    LCD_DrawLine(x2, y1, x2, y2);
}

/**
 * @brief  画一个圆
 *
 * @param  x0, y0 圆心坐标
 * @param  r     圆半径
 *
 * @return void
 */
void LCD_Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r)
{
    int a = 0, b = r;
    int di = 3 - (r << 1);

    while (a <= b)
    {
        // 绘制圆的八分之一
        LCD_Draw_Point(x0 + b, y0 + a);
        LCD_Draw_Point(x0 - b, y0 + a);
        LCD_Draw_Point(x0 + a, y0 + b);
        LCD_Draw_Point(x0 - a, y0 + b);
        LCD_Draw_Point(x0 + b, y0 - a);
        LCD_Draw_Point(x0 - b, y0 - a);
        LCD_Draw_Point(x0 + a, y0 - b);
        LCD_Draw_Point(x0 - a, y0 - b);

        a++;
				//Bresenham
        if (di < 0) di += 4 * a + 6;
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }
    }
}

/**
 * @brief	显示图片
 *
 * @remark	Image2Lcd取模方式：	C语言数据/水平扫描/16位真彩色(RGB565)/高位在前		其他的不要选
 *
 * @param   x,y		起点坐标
 * @param   width	图片宽度
 * @param   height	图片高度
 * @param   p		图片缓存数据起始地址
 *
 * @return  void
 */
void LCD_Show_Image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *p)
{
    if(x + width > LCD_Width || y + height > LCD_Height)
    {
        return;
    }

    LCD_Address_Set(x, y, x + width - 1, y + height - 1);

    LCD_DC(1);

	if(width * height * 2 > 65535)
	{
		LCD_SPI_Send((uint8_t *)p, 65535);
		LCD_SPI_Send((uint8_t *)(p + 65535), width*height * 2 - 65535);
	}
	else
	{
		LCD_SPI_Send((uint8_t *)p, width * height * 2);
	}
}


/**
 * @brief	LCD初始化
 *
 * @param   void
 *
 * @return  void
 */
void LCD_Init(void)
{
    //LCD_Gpio_Init();	//硬件接口初始化

    LCD_RST(0);
    HAL_Delay(12);
    LCD_RST(1);
	
    HAL_Delay(12);
    /* Sleep Out */
    LCD_Write_Cmd(0x11);
    /* wait for power stability */
    HAL_Delay(12);

    /* Memory Data Access Control */
    LCD_Write_Cmd(0x36);
    LCD_Write_Data(0x60);

    /* RGB 5-6-5-bit  */
    LCD_Write_Cmd(0x3A);
    LCD_Write_Data(0x65);

    /* Porch Setting */
    LCD_Write_Cmd(0xB2);
    LCD_Write_Data(0x0C);
    LCD_Write_Data(0x0C);
    LCD_Write_Data(0x00);
    LCD_Write_Data(0x33);
    LCD_Write_Data(0x33);


    /*  Gate Control */
    LCD_Write_Cmd(0xB7);
    LCD_Write_Data(0x72);

    /* VCOM Setting */
    LCD_Write_Cmd(0xBB);
    LCD_Write_Data(0x3D);   //Vcom=1.625V

    /* LCM Control */
    LCD_Write_Cmd(0xC0);
    LCD_Write_Data(0x2C);

    /* VDV and VRH Command Enable */
    LCD_Write_Cmd(0xC2);
    LCD_Write_Data(0x01);

    /* VRH Set */
    LCD_Write_Cmd(0xC3);
    LCD_Write_Data(0x19);

    /* VDV Set */
    LCD_Write_Cmd(0xC4);
    LCD_Write_Data(0x20);

    /* Frame Rate Control in Normal Mode */
    LCD_Write_Cmd(0xC6);
    LCD_Write_Data(0x0F);	//60MHZ

    /* Power Control 1 */
    LCD_Write_Cmd(0xD0);
    LCD_Write_Data(0xA4);
    LCD_Write_Data(0xA1);

    /* Positive Voltage Gamma Control */
    LCD_Write_Cmd(0xE0);
    LCD_Write_Data(0xD0);
    LCD_Write_Data(0x04);
    LCD_Write_Data(0x0D);
    LCD_Write_Data(0x11);
    LCD_Write_Data(0x13);
    LCD_Write_Data(0x2B);
    LCD_Write_Data(0x3F);
    LCD_Write_Data(0x54);
    LCD_Write_Data(0x4C);
    LCD_Write_Data(0x18);
    LCD_Write_Data(0x0D);
    LCD_Write_Data(0x0B);
    LCD_Write_Data(0x1F);
    LCD_Write_Data(0x23);

    /* Negative Voltage Gamma Control */
    LCD_Write_Cmd(0xE1);
    LCD_Write_Data(0xD0);
    LCD_Write_Data(0x04);
    LCD_Write_Data(0x0C);
    LCD_Write_Data(0x11);
    LCD_Write_Data(0x13);
    LCD_Write_Data(0x2C);
    LCD_Write_Data(0x3F);
    LCD_Write_Data(0x44);
    LCD_Write_Data(0x51);
    LCD_Write_Data(0x2F);
    LCD_Write_Data(0x1F);
    LCD_Write_Data(0x1F);
    LCD_Write_Data(0x20);
    LCD_Write_Data(0x23);

    /* Display Inversion On */
    LCD_Write_Cmd(0x21);

    LCD_Write_Cmd(0x29);

    LCD_Address_Set(0, 0, LCD_Width - 1, LCD_Height - 1);

    LCD_Clear(BLACK);

    /* Display on */

}

/********************************************************************************
	显示ui背景
*********************************************************************************/
void LCD_Show_BG(void)
{
	LCD_Show_Image(0, 0, 240, 135, BG);	
}

/********************************************************************************
	显示开机logo
*********************************************************************************/
void LCD_Show_O1(void)
{
	LCD_Show_Image(30, 37, 100, 63, O1);	
}

void LCD_Show_O2(void)
{
	LCD_Show_Image(30, 37, 123, 63, O2);	
}

void LCD_Show_O3(void)
{
	LCD_Show_Image(30, 37, 145, 63, O3);	
}

void LCD_Show_O4(void)
{
	LCD_Show_Image(30, 37, 168, 63, O4);	
}

void LCD_Show_O5(void)
{
	LCD_Show_Image(30, 37, 190, 63, O5);	
}

void LCD_Show_O6(void)
{
	LCD_Show_Image(58, 59, 13, 13, O6);	
}

void LCD_Show_O7(void)
{
	LCD_Show_Image(110, 55, 109, 23, O7);	
}

/********************************************************************************
	显示电源供电模式 1/2
*********************************************************************************/
void LCD_Show_Poewr_Num(float32_t volRealValue) 
{
    static const uint8_t* lastNum = NULL;
    static uint8_t last_state = 0;
    const float32_t hysteresis = 0.5f;  // 滞回区间
    
    // 滞回比较逻辑
    const uint8_t curState = (volRealValue > (16.0f + (last_state ? hysteresis : -hysteresis))) ? 1 : 0;
    
    const uint8_t* curNum = (curState) ? Num_cx2 : Num_cx1;

    if(curNum != lastNum) {
        LCD_Show_Image(49, 37, 11, 11, curNum);
        lastNum = curNum;
        last_state = curState;  // 更新状态缓存
    }
}

/********************************************************************************
	显示电源电压
*********************************************************************************/
void LCD_Show_Vol_Num(float32_t volRealValue)
{
    static const uint8_t* last_ten, *last_unit, *last_d1, *last_d2;
	static const uint8_t* const Num_cd[] =
	{
        Num_cd0, Num_cd1, Num_cd2, Num_cd3, Num_cd4,
        Num_cd5, Num_cd6, Num_cd7, Num_cd8, Num_cd9
    };
	
    // 数值处理
    uint32_t vol = (uint32_t)(fabsf(volRealValue) * 100.0f + 0.5f);
    
    // 数字分解（先提取数字索引）
    const uint8_t digits[] =
	{
        (vol / 1000) % 10,  // 十位索引
        (vol / 100) % 10,   // 个位索引
        (vol / 10) % 10,    // 小数1索引
        vol % 10            // 小数2索引
    };

    // 通过索引获取显示内容
    const uint8_t* voltage_ten = Num_cd[digits[0]];
    const uint8_t* voltage_unit = Num_cd[digits[1]];
    const uint8_t* voltage_d1 = Num_cd[digits[2]];
    const uint8_t* voltage_d2 = Num_cd[digits[3]];

    // 带缓存的显示逻辑
    if(voltage_ten != last_ten)
	{
        LCD_Show_Image(87, 20, 27, 27, voltage_ten);
        last_ten = voltage_ten;
    }
    if(voltage_unit != last_unit)
	{
        LCD_Show_Image(114, 20, 27, 27, voltage_unit);
        last_unit = voltage_unit;
    }
    if(voltage_d1 != last_d1)
	{
        LCD_Show_Image(157, 20, 27, 27, voltage_d1);
        last_d1 = voltage_d1;
    }
    if(voltage_d2 != last_d2)
	{
        LCD_Show_Image(184, 20, 27, 27, voltage_d2);
        last_d2 = voltage_d2;
    }
}

/********************************************************************************
	显示电机温度 0.0-60.0
*********************************************************************************/
// 温度显示函数（十位、个位、小数点后1位）
void LCD_Show_Temperature_Num(float32_t Temp_Real_value1, float32_t Temp_Real_value2)
{
    static const uint8_t *last1_ten, *last1_unit, *last1_decimal, *last2_ten, *last2_unit, *last2_decimal;
	static const uint8_t* const Num_cx[] =
	{
        Num_cx0, Num_cx1, Num_cx2, Num_cx3, Num_cx4,
        Num_cx5, Num_cx6, Num_cx7, Num_cx8, Num_cx9
    };    
	
    // 数值处理
    uint32_t temp1 = fabsf(Temp_Real_value1) * 10;
    uint32_t temp2 = fabsf(Temp_Real_value2) * 10;
    
    // 数字分解（先提取数字索引）
    const uint8_t digits1[] =
	{
        (temp1 / 100) % 10,  // 十位索引
        (temp1 / 10) % 10,   // 个位索引
         temp1 % 10          // 小数位索引
    };
    const uint8_t digits2[] =
	{
        (temp2 / 100) % 10,  // 十位索引
        (temp2 / 10) % 10,   // 个位索引
         temp2 % 10          // 小数位索引
    };

    // 通过索引获取显示内容
    const uint8_t* temp1_ten = Num_cx[digits1[0]];
    const uint8_t* temp1_unit = Num_cx[digits1[1]];
    const uint8_t* temp1_decimal = Num_cx[digits1[2]];
    const uint8_t* temp2_ten = Num_cx[digits2[0]];
    const uint8_t* temp2_unit = Num_cx[digits2[1]];
    const uint8_t* temp2_decimal = Num_cx[digits2[2]];

    // 带缓存的显示逻辑
    if(temp1_ten != last1_ten)
	{
        LCD_Show_Image(103, 6, 11, 11, temp1_ten);
        last1_ten = temp1_ten;
    }
    if(temp1_unit != last1_unit)
	{
        LCD_Show_Image(114, 6, 11, 11, temp1_unit);
        last1_unit = temp1_unit;
    }
    if(temp1_decimal != last1_decimal)
	{
        LCD_Show_Image(132, 6, 11, 11, temp1_decimal);
        last1_decimal = temp1_decimal;
    }
	
	if(temp2_ten != last2_ten)
	{
        LCD_Show_Image(175, 6, 11, 11, temp2_ten);
        last2_ten = temp2_ten;
    }
    if(temp2_unit != last2_unit)
	{
        LCD_Show_Image(186, 6, 11, 11, temp2_unit);
        last2_unit = temp2_unit;
    }
    if(temp2_decimal != last2_decimal)
	{
        LCD_Show_Image(204, 6, 11, 11, temp2_decimal);
        last2_decimal = temp2_decimal;
    }
}

/********************************************************************************
	显示电机控制模式
*********************************************************************************/
static void show_motor_mode(int x, int y, uint8_t mode)
{
	static const uint8_t* motor_mode_images[4] =
	{
		Ope, Cur, Vel, Pos  // 索引 0~3 对应不同模式
	};
	
    if (mode >= sizeof(motor_mode_images)/sizeof(motor_mode_images[0]))
	{
        mode = 0; // 超出范围时默认显示 Ope
    }
    LCD_Show_Image(x, y, 25, 14, motor_mode_images[mode]);
}

void LCD_Show_Motor_Mode(uint8_t M0_Mode, uint8_t M1_Mode)
{
    // 静态变量保存上一次的模式，初始值设为无效值确保首次更新
    static uint8_t last_M0_mode = 0xFF;
    static uint8_t last_M1_mode = 0xFF;

    // 检查M0模式是否变化
    if (M0_Mode != last_M0_mode)
	{
        show_motor_mode(84, 78, M0_Mode);
        last_M0_mode = M0_Mode; // 更新记录值
    }

    // 检查M1模式是否变化
    if (M1_Mode != last_M1_mode)
	{
        show_motor_mode(198, 78, M1_Mode);
        last_M1_mode = M1_Mode; // 更新记录值
    }
}

/********************************************************************************
	显示电机速度
*********************************************************************************/
void LCD_Show_Motor_Vel(float32_t M0_Vel, float32_t M1_Vel)
{
    static const uint8_t* last_m0[4], *last_m1[4];
    static const uint8_t* const Num_xx[] = 
    {
        Num_xx0, Num_xx1, Num_xx2, Num_xx3, Num_xx4,
        Num_xx5, Num_xx6, Num_xx7, Num_xx8, Num_xx9
    };

    // 处理M0速度
    uint32_t vel0 = (uint32_t)fabsf(M0_Vel);
    const uint8_t digits0[] = 
    {
        (vel0 / 1000) % 10,  // 千位
        (vel0 / 100) % 10,   // 百位
        (vel0 / 10) % 10,    // 十位
         vel0 % 10           // 个位
    };

    const uint8_t* disp0[4];
    disp0[0] = (vel0 >= 1000) ? Num_xx[digits0[0]] : Num_xx_blank; // 千位
    disp0[1] = (vel0 >= 100)  ? Num_xx[digits0[1]] : Num_xx_blank; // 百位
    disp0[2] = (vel0 >= 10)   ? Num_xx[digits0[2]] : Num_xx_blank; // 十位
    disp0[3] = Num_xx[digits0[3]];                                 // 个位

    // 更新显示
	if(disp0[0] != last_m0[0]) LCD_Show_Image(55, 95, 10, 13, disp0[0]), last_m0[0] = disp0[0]; // 千位
    if(disp0[1] != last_m0[1]) LCD_Show_Image(65, 95, 10, 13, disp0[1]), last_m0[1] = disp0[1]; // 百位
    if(disp0[2] != last_m0[2]) LCD_Show_Image(75, 95, 10, 13, disp0[2]), last_m0[2] = disp0[2]; // 十位
    if(disp0[3] != last_m0[3]) LCD_Show_Image(85, 95, 10, 13, disp0[3]), last_m0[3] = disp0[3]; // 个位
    
	// 处理M1速度（逻辑相同）
    uint32_t vel1 = (uint32_t)fabsf(M1_Vel);
    const uint8_t digits1[] = 
    {
        (vel1 / 1000) % 10,  // 千位
        (vel1 / 100) % 10,   // 百位
        (vel1 / 10) % 10,    // 十位
         vel1 % 10           // 个位
    };

    const uint8_t* disp1[4];
    disp1[0] = (vel1 >= 1000) ? Num_xx[digits1[0]] : Num_xx_blank; // 千位
    disp1[1] = (vel1 >= 100)  ? Num_xx[digits1[1]] : Num_xx_blank; // 百位
    disp1[2] = (vel1 >= 10)   ? Num_xx[digits1[2]] : Num_xx_blank; // 十位
    disp1[3] = Num_xx[digits1[3]];                                 // 个位

	if(disp1[0] != last_m1[0]) LCD_Show_Image(169, 95, 10, 13, disp1[0]), last_m1[0] = disp1[0]; // 千位
    if(disp1[1] != last_m1[1]) LCD_Show_Image(179, 95, 10, 13, disp1[1]), last_m1[1] = disp1[1]; // 百位
    if(disp1[2] != last_m1[2]) LCD_Show_Image(189, 95, 10, 13, disp1[2]), last_m1[2] = disp1[2]; // 十位
    if(disp1[3] != last_m1[3]) LCD_Show_Image(199, 95, 10, 13, disp1[3]), last_m1[3] = disp1[3]; // 个位
}

/********************************************************************************
	显示电机电流
*********************************************************************************/
//void LCD_Show_Motor_Cur(float32_t M0_Cur, float32_t M1_Cur)
//{
//    static const uint8_t* last_m0[4], *last_m1[4];
//	static const uint8_t* const Num_xx[] =
//	{
//		Num_xx0, Num_xx1, Num_xx2, Num_xx3, Num_xx4,
//		Num_xx5, Num_xx6, Num_xx7, Num_xx8, Num_xx9
//    };
//    
//    // 处理M0电流
//	if (M0_Cur < 0) {M0_Cur = -M0_Cur;}
//	if (M1_Cur < 0) {M1_Cur = -M1_Cur;}
////	M0_Cur = MovingAvg_Update(&M0ibus_avg_filter, M0_Cur * 0.2f);
////	M1_Cur = MovingAvg_Update(&M1ibus_avg_filter, M1_Cur * 0.2f);
////	printf("%.3f, %.3f\n", M0_Cur, M1_Cur);
//    uint32_t cur0 = fabsf(M0_Cur) * 1000;
//    const uint8_t digits0[] =
//	{
//        (cur0 / 1000) % 10,
//        (cur0 / 100) % 10,
//        (cur0 / 10) % 10,
//        cur0 % 10
//    };
//    
//    const uint8_t* disp0[] =
//	{
//        Num_xx[digits0[0]],
//        Num_xx[digits0[1]],
//        Num_xx[digits0[2]],
//        Num_xx[digits0[3]]
//    };
//    
//    if(disp0[0] != last_m0[0]) LCD_Show_Image(61, 95, 10, 13, disp0[0]), last_m0[0] = disp0[0];
//    if(disp0[1] != last_m0[1]) LCD_Show_Image(74, 95, 10, 13, disp0[1]), last_m0[1] = disp0[1];
//    if(disp0[2] != last_m0[2]) LCD_Show_Image(84, 95, 10, 13, disp0[2]), last_m0[2] = disp0[2];
//    if(disp0[3] != last_m0[3]) LCD_Show_Image(94, 95, 10, 13, disp0[3]), last_m0[3] = disp0[3];
//    
//    // 处理M1电流
//    uint32_t cur1 = fabsf(M1_Cur) * 1000;
//    const uint8_t digits1[] = {
//        (cur1 / 1000) % 10,
//        (cur1 / 100) % 10,
//        (cur1 / 10) % 10,
//        cur1 % 10
//    };
//    
//    const uint8_t* disp1[] = {
//        Num_xx[digits1[0]],
//        Num_xx[digits1[1]],
//        Num_xx[digits1[2]],
//        Num_xx[digits1[3]]
//    };
//    
//    if(disp1[0] != last_m1[0]) LCD_Show_Image(175, 95, 10, 13, disp1[0]), last_m1[0] = disp1[0];
//    if(disp1[1] != last_m1[1]) LCD_Show_Image(188, 95, 10, 13, disp1[1]), last_m1[1] = disp1[1];
//    if(disp1[2] != last_m1[2]) LCD_Show_Image(198, 95, 10, 13, disp1[2]), last_m1[2] = disp1[2];
//    if(disp1[3] != last_m1[3]) LCD_Show_Image(208, 95, 10, 13, disp1[3]), last_m1[3] = disp1[3];
//}

/********************************************************************************
	显示舵机角度
*********************************************************************************/
void LCD_Show_Servo_Angle(uint8_t S0_Angle, uint8_t S1_Angle)
{
    static const uint8_t* last_s0[2], *last_s1[2];
    static const uint8_t* const Num_xx[] = 
    {
        Num_xx0, Num_xx1, Num_xx2, Num_xx3, Num_xx4,
        Num_xx5, Num_xx6, Num_xx7, Num_xx8, Num_xx9
    };
    
    // 角度限幅
    S0_Angle = S0_Angle > 90 ? 90 : S0_Angle;
    S1_Angle = S1_Angle > 90 ? 90 : S1_Angle;

    /****** 处理S0角度 ​******/
    const uint8_t s0_tens = S0_Angle / 10;
    const uint8_t s0_units = S0_Angle % 10;
    
    // 动态选择显示内容
    const uint8_t* disp_s0[] = 
    {
        (S0_Angle >= 10) ? Num_xx[s0_tens] : Num_xx_blank, // 小于10时隐藏十位
        Num_xx[s0_units]
    };

    // 十位显示逻辑
    if(disp_s0[0] != last_s0[0])
	{
        LCD_Show_Image(87, 113, 10, 13, disp_s0[0]);
        last_s0[0] = disp_s0[0];
    }
    
    // 个位显示逻辑
    if(disp_s0[1] != last_s0[1]) 
	{
        LCD_Show_Image(97, 113, 10, 13, disp_s0[1]);
        last_s0[1] = disp_s0[1];
    }

    /****** 处理S1角度 ​******/
    const uint8_t s1_tens = S1_Angle / 10;
    const uint8_t s1_units = S1_Angle % 10;
    
    const uint8_t* disp_s1[] = 
    {
        (S1_Angle >= 10) ? Num_xx[s1_tens] : Num_xx_blank,
        Num_xx[s1_units]
    };

    if(disp_s1[0] != last_s1[0])
	{
        LCD_Show_Image(202, 113, 10, 13, disp_s1[0]);
        last_s1[0] = disp_s1[0];
    }
    
    if(disp_s1[1] != last_s1[1])
	{
        LCD_Show_Image(212, 113, 10, 13, disp_s1[1]);
        last_s1[1] = disp_s1[1];
    }
}
/* End of this file */
