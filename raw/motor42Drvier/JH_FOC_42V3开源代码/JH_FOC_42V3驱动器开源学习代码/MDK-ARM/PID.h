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


#include "main.h"
#include "b_motor_foc.h"

typedef struct {
    float kp;
    float ki;
	  float kd;
    float integral;
	  float actual_value;//测量值
	  float target; //目标值
    float output_max;
    float output_min;
	  float erro;
	  float last_err;
} PID_Controller_t;


// 绝对角度闭环控制结构体
typedef struct AngleControl_TypeDef{
    float zero_position;      // 零点编码器角度，单位度
    PID_Controller_t pid_angle;
    float Iq_ref;             // 输出Iq参考电流
} AngleControl_TypeDef;










extern AngleControl_TypeDef AngleControl;//角度控制结构体
extern PID_Controller_t speed_pi; //速度环pid
extern PID_Controller_t position_pi;;

float PID_Controller_Update(PID_Controller_t *pi, float error);
void PID_Controller_Init(PID_Controller_t *pi, float kp, float ki,float kd,float target ,float out_min, float out_max);


///////////////////////速度环
// 速度环计数
extern uint16_t  mt6816_count;
extern uint16_t encoder_last_count;
// 读取编码器计数，计算累计计数（处理溢出）
void Encoder_Update(void);
// 计算速度（单位rpm），周期为 speed_loop_period 秒
float Calculate_Speed(float speed_loop_period);
void init_encoder_update(void);//初始化计数，需要速度环进行前使用
float Angle_Error(float target, float current);
/////////////////////////////////////////////
//角度环

// 设置目标绝对角度，单位度，自动转换为弧度并归一化到[0,360)
void AngleControl_SetTargetAngle(AngleControl_TypeDef* ctrl, float target_deg) ;
// 设置零点（记录当前编码器角度作为零点）
void AngleControl_SetZero(AngleControl_TypeDef* ctrl) ;
//角度控制环
void  AngleControl_Update(AngleControl_TypeDef* ctrl) ;


////////////////////////////////位置环

/////////////////////////////////////////








