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

typedef struct {
	
  uint8_t motor_set;  //设置电机0：步进电机，1：无刷电机，2：直流电机
	uint8_t motor_mode; //设置电机的工作模式0：开环，1：电流闭环，2：速度环，3：位置环，4：角度环
  float drive_current; //设置电流环电流，FOC闭环时是Iq的值
	float max_current;  //最大输出电流，只有正值
	
	float speed;  //控制电机速度环运动的速度
	float s_acc;//电机速度环的加速度
	
	float distance; //电机要转动的角度
	
	float max_speed;         // 最大速度，单位度/秒
  float accel;             // 加速度，单位度/秒^2
  float decel;             // 减速度，单位度/秒^2

}connect_crt_TypeDef;

extern uint8_t reset_flag;
extern uint16_t angle_zero;
extern uint32_t save_data[20];
extern  uint8_t motion_set;
extern uint8_t if_read_pos_start;
extern connect_crt_TypeDef connect_crt; 
void init_connect_crt(connect_crt_TypeDef* connect);//初始化控制参数
void usb_task(void);
void check_limit(void);//限位检查
void get_zero_by_current(void);//通过检测堵转寻找零点
void stop_motor(connect_crt_TypeDef* connect);
//////////////////Can总线通信
void can_task(void);
void set_cantx_buf(uint8_t b0,uint8_t b1,uint8_t b2,uint8_t b3,uint8_t b4,uint8_t b5,uint8_t b6,uint8_t b7);

void start_reset_waiting(void);//上电位置自启动需要复位时等待函数

