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

// 在 position_foc.h 中添加
#define RPM_TO_DEG_PER_SEC(rpm) ((rpm) * 6.0f)        // rpm -> 度/秒 (360/60=6)
#define DEG_PER_SEC_TO_RPM(deg) ((deg) / 6.0f)        // 度/秒 -> rpm
#define RPM_TO_RAD_PER_SEC(rpm) ((rpm) * 0.10472f)    // rpm -> 弧度/秒

typedef struct {
    // 目标与位置
    float target_position;      // 目标位置（度）
    float current_position;     // 当前位置（度）
    float start_position;       // 起始位置（度）
    float position_error;       // 位置误差（度）
    
    // 运动参数（速度单位：rpm，加速度单位：rpm/s）
    float max_speed;           // 最大速度（rpm）
    float accel;               // 加速度（rpm/s）
    float decel;               // 减速度（rpm/s）
    
    // 速度规划（内部使用度/秒）
    float planned_speed_deg;   // 规划速度（度/秒）
    float cruise_speed_deg;    // 巡航速度（度/秒）
    
    // 状态控制
    uint8_t motion_phase;      // 0:加速 1:巡航 2:减速 3:停止
    uint8_t motion_state;      // 0:空闲 1:运行中 2:完成
    uint8_t is_running;
    uint8_t is_absolute;
    uint8_t pos_finish_flag;
    
    // 梯形规划参数
    float decel_start_pos;     // 开始减速位置
    float total_distance;      // 总距离（度）
    float remaining_distance;  // 剩余距离（度）
    
    // 稳态保持
    float hold_gain;           // 位置保持增益
    float hold_integral;       // 位置保持积分
    
    // 死区和阈值
    float deadband;            // 位置死区（度）
    float position_threshold;  // 位置阈值（度）
    
    // 累计角度相关（移到结构体中，避免static问题）
    float total_angle;          // 累计角度
    float last_raw_angle;       // 上次原始角度
    uint8_t first_read;         // 首次读取标志
    float raw_angle_offset;     // 原始角度偏移
    
   uint8_t is_auto_correct;   // 新增：区分用户指令(0)和自动修正(1)
    float angle_offset;
    
} PositionControl_TypeDef;
// 位置环PID（位置环输出为速度目标值）


// 在 position_foc.h 中添加
typedef struct {
    float angle_prev;      // 上一次编码器角度 (0~360)
    float angle_accum;     // 累计角度
    float start_angle;     // 起始角度
    float target_angle_acc; // 目标累计角度
    int16_t round_count;   // 跨圈计数
    uint8_t initialized;   // 初始化标志
    float erro;            // 误差
} EncoderAccumulator_TypeDef;

// 在 PositionControl_TypeDef 中添加或外部声明
extern EncoderAccumulator_TypeDef encoder_acc;

extern PositionControl_TypeDef position_ctrl;

// 位置环相关外部变量声明
extern float speed_send;
extern uint32_t timer_value;
extern uint8_t pos_finish_flag;


// 位置环初始化
void PositionControl_Init(PositionControl_TypeDef* pos, 
                          float max_speed, 
                          float accel, 
                          float decel,
                          float deadband,
                          float threshold);
													// 获取当前位置（度）
float PositionControl_GetCurrentPosition(void);
// 位置环更新 - 在1ms定时器中调用
void PositionControl_Update(PositionControl_TypeDef* pos);
													// 启动位置环绝对运动
void PositionControl_StartAbsolute(PositionControl_TypeDef* pos, float target_angle);
													// 启动位置环相对运动
void PositionControl_StartRelative(PositionControl_TypeDef* pos, float delta_angle);
													// 停止位置环运动
void PositionControl_Stop(PositionControl_TypeDef* pos);
													// 位置环复位（找零点）
void PositionControl_Reset(PositionControl_TypeDef* pos);
													// 位置环零点设置
void PositionControl_SetZero(PositionControl_TypeDef* pos);
													// 限位判断函数 - 检查目标位置是否超出限位
uint8_t PositionControl_CheckLimit(float target_pos);

// 初始化累计器
void EncoderAccumulator_Init(EncoderAccumulator_TypeDef* enc) ;
float EncoderAccumulator_Update(EncoderAccumulator_TypeDef* enc);

