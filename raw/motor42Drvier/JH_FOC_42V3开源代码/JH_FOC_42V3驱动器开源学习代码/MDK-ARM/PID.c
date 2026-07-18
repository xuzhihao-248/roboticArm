	/*！！！！！！作者声明！！！！！！！！ */
/**
  ******************************************************************************
  * JH_FOC_42V3步进电机FOC驱动器开源代码
	* 该代码已实现步进电机开环，电流闭环，速度-电流闭环，角度-电流闭环，电机非线性校准等
  * 注意该代码中的位置环代码绝大部分由AI生成，并非原厂位置环代码，控制效果不如原厂，需要自己优化！！
	* 注意该代码中的位置环代码绝大部分由AI生成，并非原厂位置环代码，控制效果不如原厂，需要自己优化！！
	* 注意该代码中的位置环代码绝大部分由AI生成，并非原厂位置环代码，控制效果不如原厂，需要自己优化！！
  ******************************************************************************
	* 代码绝大部分都是作者书写，或有不足，敬请指正！
	* 作者能力有限，代码可能阅读性不是特别友好，望包涵！
	  ******************************************************************************
	
	* 最后重点声明：该代码仅用于学习交流，禁止任何商业用途！！！！！
									该代码仅用于学习交流，禁止任何商业用途！！！！！
									该代码仅用于学习交流，禁止任何商业用途！！！！！
									该代码仅用于学习交流，禁止任何商业用途！！！！！
									该代码仅用于学习交流，禁止任何商业用途！！！！！
  ******************************************************************************
	                                                     
																					版权所有		
																							淘宝店铺：JHive智控
																				      店铺链接：https://shop355919857.taobao.com/?spm=a21xtw.29978518.0.0
	                                                                 2026年7月8日
																																	 
	******************************************************************************
  */



#include "pid.h"
#include "mt6816.h"
#include "math.h"
#include "usbd_cdc_if.h"
#include "can.h"
#include "connecting.h"
#include "b_motor_foc.h"

PID_Controller_t speed_pi; //速度环pid
PID_Controller_t position_pi;
// 编码器变量
uint16_t  mt6816_count=0;//编码器计数值
uint16_t encoder_last_count = 0;
int32_t encoder_count_accum = 0;
extern uint8_t pos_finish_flag;
extern uint8_t connect_type;
// 编码器PPR
#define MT6816_PPR 16384
// 定时器频率及周期需要你根据硬件具体调整
extern uint16_t angle_zero;

///PI控制
float PID_Controller_Update(PID_Controller_t *pi, float error)
{
    pi->integral += error * pi->ki;
    // 反积分限幅
    if(pi->integral > pi->output_max) pi->integral = pi->output_max;
    else if(pi->integral < pi->output_min) pi->integral = pi->output_min;

    float output = pi->kp * error +  pi->integral+pi->kd*(error-pi->last_err );

    // 输出限幅
    if(output > pi->output_max) output = pi->output_max;
    else if(output < pi->output_min) output = pi->output_min;
    pi->last_err =error ;  
    return output;
}

 ///PI参数初始化
void PID_Controller_Init(PID_Controller_t *pi, float kp, float ki,float kd,float target ,float out_min, float out_max)
{
    pi->kp = kp;
    pi->ki = ki;
	  pi->kd = kd;
	  pi->target =target ;
    pi->integral = 0.0f;
	  if(out_max >4 )out_max =4;
	  if(out_min <-4)out_min =-4;//限幅
    pi->output_min = out_min;
    pi->output_max = out_max;
}


// 计算编码器差值，考虑编码器计数范围为0~MT6816_PPR-1环绕
int16_t Encoder_GetDiff(uint16_t current_count, uint16_t last_count)
{
    int16_t diff = ((int16_t)current_count - (int16_t)last_count);

    // 如果差值大于半个计数周期，说明发生了环绕，需校正
    if(diff > (MT6816_PPR / 2))
        diff -= MT6816_PPR;
    else if(diff < -(MT6816_PPR / 2))
        diff += MT6816_PPR;

    return (int16_t)diff*DIR;
}

// 读取编码器计数，计算累计计数（处理溢出）
void Encoder_Update(void)
{
    int16_t diff = Encoder_GetDiff(mt6816_count , encoder_last_count);
    encoder_count_accum += diff;
    encoder_last_count = mt6816_count;
}

void init_encoder_update(void)//初始化计数，需要速度环进行前使用
{
    encoder_last_count   = mt6816_count; // 以当前值为新的“上一点”
    encoder_count_accum  = 0;
}

// 计算速度（单位rpm），周期为 speed_loop_period 秒
float Calculate_Speed(float speed_loop_period)
{
    float revolutions = (float)encoder_count_accum / MT6816_PPR;
    encoder_count_accum = 0;  // 清零累计计数

    float speed_rpm = (revolutions / speed_loop_period) * 60.0f;
    return speed_rpm;
}




//角度环（0-360°）绝对位置

AngleControl_TypeDef AngleControl;

// 角度归一化到[0, 360)
float Angle_Normalize(float angle) {
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return angle;
}


// 计算角度误差，范围在[-pi, pi]
float Angle_Error(float target, float current) {
    float diff = target - current;
    if (diff > 180) diff -= 360;
    else if (diff < -180) diff += 360;
    return diff;
}


// 设置目标绝对角度，单位度，自动转换为弧度并归一化到[0,360)
void AngleControl_SetTargetAngle(AngleControl_TypeDef* ctrl, float target_deg) 
{
    ctrl->pid_angle.target =Angle_Normalize(target_deg) ;
}

// 设置零点（记录当前编码器角度作为零点）
void AngleControl_SetZero(AngleControl_TypeDef* ctrl) 
	{
    ctrl->zero_position = DIR*mt6816_count *0.02197265625f;
		ctrl ->pid_angle.target =0;
}


void  AngleControl_Update(AngleControl_TypeDef* ctrl) 
{
   
	float current_angle = DIR*mt6816_count*0.02197265625f;
    // 当前相对零点角度
    float relative_angle = Angle_Normalize((current_angle - ctrl->zero_position));

    // 计算误差
    float error =Angle_Error(ctrl->pid_angle.target, relative_angle);
    if(error >60)error =60;else if(error <-60)error=-60;
    // PID控制计算Iq参考电流
    ctrl->Iq_ref = PID_Controller_Update(&ctrl->pid_angle, error);

}


























