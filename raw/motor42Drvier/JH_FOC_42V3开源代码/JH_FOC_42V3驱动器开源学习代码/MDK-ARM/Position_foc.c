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



#include "position_foc.h"
#include "mt6816.h"
#include "connecting.h"
#include "math.h"
#include "myflash.h"
#include "pid.h"

PositionControl_TypeDef position_ctrl;
// 位置环初始化
void PositionControl_Init(PositionControl_TypeDef* pos, 
                          float max_speed,
                          float accel, 
                          float decel,
                          float deadband,
                          float threshold)
{
    // 先初始化编码器累计器（使用当前 if_read_pos_start 状态）
    EncoderAccumulator_Init(&encoder_acc);
    
    // 基本参数
    pos->start_position = 0;
    pos->position_error = 0;
    pos->max_speed = max_speed;
    pos->accel = accel;
    pos->decel = decel;
    pos->deadband = deadband;
    pos->position_threshold = threshold;
    pos->total_distance = 0;
    pos->remaining_distance = 0;
    
    // 状态
    pos->motion_state = 0;
    pos->motion_phase = 0;
    pos->is_absolute = 0;
    pos->is_running = 0;
    pos->pos_finish_flag = 1;
    pos->is_auto_correct = 0;
    
    // 速度规划
    pos->planned_speed_deg = 0;
    pos->cruise_speed_deg = 0;
    pos->decel_start_pos = 0;
    
    // 累计角度状态
    pos->total_angle = 0;
    pos->last_raw_angle = 0;
    pos->first_read = 0;  // 不再使用，由 encoder_acc 管理
    pos->raw_angle_offset = 0;
    pos->angle_offset = 0;
    
    // 位置保持参数
    pos->hold_gain = 0.1f;
    pos->hold_integral = 0;
    
    // 设置当前位置和目标位置
    pos->current_position = encoder_acc.angle_accum;
    // 关键：使用 target_angle_acc 作为初始目标
    pos->target_position = encoder_acc.target_angle_acc;
    
    // 位置环PID
    float kp = 0.2f;
    float ki = 0.001f;
    float kd = 0.0f;
    float max_speed_deg = RPM_TO_DEG_PER_SEC(max_speed);
    PID_Controller_Init(&position_pi, kp, ki, kd, 0, 
                       -max_speed_deg, max_speed_deg);
}

// 位置保持函数 - 抗扰动
void PositionControl_HoldPosition(PositionControl_TypeDef* pos)
{
    if (connect_crt.motor_mode != 3) return;
    if (pos->is_running) return;
    
    pos->current_position = PositionControl_GetCurrentPosition();
    float error = pos->target_position - pos->current_position;
    float abs_error = fabs(error);
    
    // 自动修正：使用PID直接控制
    if (abs_error > pos->deadband * 2.0f) {
        pos->is_running = 1;
        pos->motion_state = 1;
        pos->motion_phase = 0;
        pos->pos_finish_flag = 0;
        pos->is_auto_correct = 1;
        
        // 使用适中的PID参数
        position_pi.kp = 0.1f;
        position_pi.ki = 0.005f;
        position_pi.integral = 0;
        position_pi.erro = error;
        speed_pi.integral = 0;
        
        // 直接使用PID计算初始速度
        float init_speed = position_pi.kp * error;
        float max_init = RPM_TO_DEG_PER_SEC(pos->max_speed * 0.5f);
        if (init_speed > max_init) init_speed = max_init;
        if (init_speed < -max_init) init_speed = -max_init;
        
        // 确保最小启动速度
        float min_speed = RPM_TO_DEG_PER_SEC(5.0f);
        if (abs_error > pos->deadband && fabs(init_speed) < min_speed) {
            init_speed = (error > 0) ? min_speed : -min_speed;
        }
        
        pos->planned_speed_deg = init_speed;
        return;
    }
    
    // 小误差保持
    if (abs_error > pos->deadband * 0.5f) {
        float hold_speed = error * pos->hold_gain;
        float max_hold = RPM_TO_DEG_PER_SEC(5.0f);
        if (hold_speed > max_hold) hold_speed = max_hold;
        if (hold_speed < -max_hold) hold_speed = -max_hold;
        
        connect_crt.speed = DEG_PER_SEC_TO_RPM(hold_speed);
        speed_pi.target = connect_crt.speed;
    } else {
        connect_crt.speed = 0;
        speed_pi.target = 0;
        speed_pi.integral = 0;
        position_pi.integral = 0;
    }
}

// 全局累计器实例（在 position_foc.c 中定义）
EncoderAccumulator_TypeDef encoder_acc;

// 初始化累计器
void EncoderAccumulator_Init(EncoderAccumulator_TypeDef* enc) {
    enc->angle_prev = mt6816_count * 0.02197265625f;
    enc->round_count = 0;
    enc->angle_accum = mt6816_count * DIR * 0.02197265625f;
    enc->initialized = 1;
    enc->start_angle = angle_zero * 0.02197265625f * DIR; // 记录位置环的开始角度
    
    // 计算角度误差（原始值，范围可能很大）
    float err = DIR * (mt6816_count - angle_zero) * 0.02197265625f;
    
    if (if_read_pos_start == 1) {
        // 判断是否需要处理跨圈情况
        if (err <= -180) {  
            enc->angle_accum = mt6816_count * DIR * 0.02197265625f + 360;
            enc->target_angle_acc = err + 360;
            enc->round_count = DIR; // 手动跨了一圈
        } 
        else if (err > 180) {  // 接近 180° 边界
           enc->angle_accum = mt6816_count * DIR * 0.02197265625f - 360;
            enc->target_angle_acc = err - 360;
            enc->round_count = -DIR; // 反向跨了一圈
        }
        else {
           enc->target_angle_acc = err;
        }
    } 
    else {
        enc->target_angle_acc = err;
    }
    
    enc->erro = 0;
}

// 更新累计角度（核心修改）
float EncoderAccumulator_Update(EncoderAccumulator_TypeDef* enc) {
    float angle_now = mt6816_count * 0.02197265625f;  // 0~360度
 
    // 如果未初始化，先初始化
    if (!enc->initialized) {
        enc->angle_prev = angle_now;
        enc->angle_accum = angle_now * DIR;
        enc->round_count = 0;
        enc->initialized = 1;
        return enc->angle_accum;
    }

    float delta = angle_now - enc->angle_prev;

    // 跨圈判断，基于delta原始符号
    if (delta > 180.0f) {
        enc->round_count -= 1;  // 编码器逆向跨圈（由360跳回0）
    } else if (delta < -180.0f) {
        enc->round_count += 1;  // 编码器正向跨圈（由0跳到接近360）
    }

    enc->angle_prev = angle_now;

    // 累计角度时乘以DIR，保证累计角度方向正确
    enc->angle_accum = (angle_now + enc->round_count * 360.0f) * DIR-enc->start_angle;

    return enc->angle_accum;
}

// 修改后的获取当前位置函数
float PositionControl_GetCurrentPosition(void)
{
    // 使用新的累计器
    return EncoderAccumulator_Update(&encoder_acc);
}



// 位置环更新 - 在1ms定时器中调用
// 位置环更新 - 保留梯形规划，修复反向问题
void PositionControl_Update(PositionControl_TypeDef* pos)
{
	    pos->current_position = PositionControl_GetCurrentPosition();
    if (!pos->is_running) {
        PositionControl_HoldPosition(pos);
        return;
    }
    

    
    float error = pos->target_position - pos->current_position;
    float abs_error = fabs(error);
    
    // === 到达检测 ===
    static uint16_t arrive_count = 0;
    
    if (abs_error < pos->deadband) {
        arrive_count++;
        if (arrive_count >= 10) {
            pos->motion_state = 2;
            pos->motion_phase = 3;
            pos->planned_speed_deg = 0;
            pos->is_running = 0;
            pos->pos_finish_flag = 1;
            
            connect_crt.speed = 0;
            speed_pi.target = 0;
            position_pi.integral = 0;
            speed_pi.integral = 0;
            
            arrive_count = 0;
            return;
        }
    } else {
        arrive_count = 0;
    }
    
    // ===== 梯形速度规划 =====
    float max_speed_deg = RPM_TO_DEG_PER_SEC(pos->max_speed);
    float accel_deg = RPM_TO_DEG_PER_SEC(pos->accel);
    float decel_deg = RPM_TO_DEG_PER_SEC(pos->decel);
    
    float current_speed_deg = pos->planned_speed_deg;  // 保留符号
    float current_speed_abs = fabs(current_speed_deg);
    
    // 计算需要的减速距离
    float decel_distance = (current_speed_abs * current_speed_abs) / (2.0f * decel_deg);
    
    // 判断运动阶段
    if (abs_error <= decel_distance + pos->deadband) {
        // === 减速阶段 ===
        pos->motion_phase = 2;
        
        float decel_step = decel_deg * 0.001f;
        
        // 根据误差方向减速
        if (current_speed_deg > 0) {
            current_speed_deg -= decel_step;
            // 关键修复：如果误差已经反向，立即反转速度方向
            if (error < -pos->deadband) {
                current_speed_deg = -RPM_TO_DEG_PER_SEC(5.0f);  // 反向启动
            } else if (current_speed_deg < 0) {
                current_speed_deg = 0;  // 减速到0，不反向
            }
        } else if (current_speed_deg < 0) {
            current_speed_deg += decel_step;
            // 关键修复：如果误差已经反向，立即反转速度方向
            if (error > pos->deadband) {
                current_speed_deg = RPM_TO_DEG_PER_SEC(5.0f);  // 反向启动
            } else if (current_speed_deg > 0) {
                current_speed_deg = 0;  // 减速到0，不反向
            }
        } else {
            // 速度为0，根据误差设置初始速度
            if (error > pos->deadband) {
                current_speed_deg = RPM_TO_DEG_PER_SEC(5.0f);
            } else if (error < -pos->deadband) {
                current_speed_deg = -RPM_TO_DEG_PER_SEC(5.0f);
            }
        }
        
        // === 低速时切换到PID主导，确保精确定位 ===
        if (abs_error < 5.0f) {  // 误差小于5度时
            // 直接使用PID控制，不用梯形规划
            position_pi.erro = error;
            float pid_speed = PID_Controller_Update(&position_pi, error);
            
            // 限幅到低速范围
            float max_slow = RPM_TO_DEG_PER_SEC(15.0f);
            if (pid_speed > max_slow) pid_speed = max_slow;
            if (pid_speed < -max_slow) pid_speed = -max_slow;
            
            // 确保方向正确（不能反向）
            if (error > 0 && pid_speed < 0) pid_speed = RPM_TO_DEG_PER_SEC(1.0f);
            if (error < 0 && pid_speed > 0) pid_speed = -RPM_TO_DEG_PER_SEC(1.0f);
            
            current_speed_deg = pid_speed;
        }
        
        pos->planned_speed_deg = current_speed_deg;
        
    } else if (current_speed_abs < max_speed_deg - 1.0f) {
        // === 加速阶段 ===
        pos->motion_phase = 0;
        
        float accel_step = accel_deg * 0.001f;
        
        if (error > 0) {
            pos->planned_speed_deg += accel_step;
            if (pos->planned_speed_deg > max_speed_deg) {
                pos->planned_speed_deg = max_speed_deg;
            }
        } else {
            pos->planned_speed_deg -= accel_step;
            if (pos->planned_speed_deg < -max_speed_deg) {
                pos->planned_speed_deg = -max_speed_deg;
            }
        }
        
        // 确保启动时方向正确
        if (error > pos->deadband && pos->planned_speed_deg <= 0) {
            pos->planned_speed_deg = RPM_TO_DEG_PER_SEC(5.0f);
        } else if (error < -pos->deadband && pos->planned_speed_deg >= 0) {
            pos->planned_speed_deg = -RPM_TO_DEG_PER_SEC(5.0f);
        }
        
    } else {
        // === 巡航阶段 ===
        pos->motion_phase = 1;
        pos->planned_speed_deg = (error > 0) ? max_speed_deg : -max_speed_deg;
    }
    
    // ===== PID微调（仅在大距离时使用） =====
    float final_speed_deg = pos->planned_speed_deg;
    
    if (abs_error > 5.0f) {  // 距离大于5度时才叠加PID微调
        float pid_adjustment = PID_Controller_Update(&position_pi, error);
        float max_pid_adj = max_speed_deg * 0.05f;
        if (pid_adjustment > max_pid_adj) pid_adjustment = max_pid_adj;
        if (pid_adjustment < -max_pid_adj) pid_adjustment = -max_pid_adj;
        
        final_speed_deg += pid_adjustment;
    }
    
    // === 最终安全检查：确保速度方向与误差方向一致 ===
    if (abs_error > pos->deadband) {
        if (error > 0 && final_speed_deg < 0) {
            final_speed_deg = RPM_TO_DEG_PER_SEC(3.0f);  // 强制正向
        } else if (error < 0 && final_speed_deg > 0) {
            final_speed_deg = -RPM_TO_DEG_PER_SEC(3.0f);  // 强制反向
        }
    } else {
        final_speed_deg = 0;  // 在死区内停止
    }
    
    // 限幅
    if (final_speed_deg > max_speed_deg) final_speed_deg = max_speed_deg;
    if (final_speed_deg < -max_speed_deg) final_speed_deg = -max_speed_deg;
    
    // 转换为rpm
    float final_speed_rpm = DEG_PER_SEC_TO_RPM(final_speed_deg);
    
    connect_crt.speed = final_speed_rpm;
    speed_pi.target = final_speed_rpm;
    
    pos->remaining_distance = abs_error;
    pos->position_error = error;
    pos->pos_finish_flag = 0;
}

void PositionControl_StartAbsolute(PositionControl_TypeDef* pos, float target_angle)
{
    // === 用户指令：直接设置目标位置 ===
    pos->target_position = target_angle;
    
    // 标记为用户主动运动
    pos->is_auto_correct = 0;
    
    // 获取当前位置
    pos->current_position = PositionControl_GetCurrentPosition();
    pos->start_position = pos->current_position;
    
    // 计算误差
    float error = pos->target_position - pos->current_position;
    pos->total_distance = fabs(error);
    pos->remaining_distance = pos->total_distance;
    
    // 重置状态
    pos->is_absolute = 1;
    pos->is_running = 1;
    pos->motion_state = 1;
    pos->motion_phase = 0;
    pos->planned_speed_deg = 0;
    pos->pos_finish_flag = 0;
    
    // 重置PID积分
    position_pi.integral = 0;
    position_pi.erro = error;
    speed_pi.integral = 0;
    
    // 根据运动距离调整PID参数
    if (pos->total_distance > 200.0f) {
        position_pi.kp = 0.4f;
        position_pi.ki = 0.002f;
    } else if (pos->total_distance < 20.0f) {
        position_pi.kp = 0.15f;
        position_pi.ki = 0.0005f;
    } else {
        position_pi.kp = 0.3f;
        position_pi.ki = 0.001f;
    }
    
    // 配置速度环
    speed_pi.output_max = connect_crt.max_current;
    speed_pi.output_min = -connect_crt.max_current;
    speed_pi.kp = 0.015f;
    speed_pi.ki = 0.00015f;
    speed_pi.integral = 0;
    
    if (pos->accel > 2000.0f) {
        speed_pi.kp = 0.025f;
        speed_pi.ki = 0.00025f;
    } else if (pos->accel < 500.0f) {
        speed_pi.kp = 0.008f;
        speed_pi.ki = 0.00008f;
    }
}

void PositionControl_StartRelative(PositionControl_TypeDef* pos, float delta_angle)
{
    // === 用户指令：基于目标位置累加 ===
    // 注意：target_position 是用户设定的目标，不应受外部扰动影响
    pos->target_position = pos->target_position + delta_angle;
    
    // 标记为用户主动运动（非自动修正）
    pos->is_auto_correct = 0;
    
    // 获取当前位置（仅用于计算误差，不修改目标）
    pos->current_position = PositionControl_GetCurrentPosition();
    pos->start_position = pos->current_position;
    
    // 计算误差（目标 - 当前位置）
    float error = pos->target_position - pos->current_position;
    pos->total_distance = fabs(error);
    pos->remaining_distance = pos->total_distance;
    
    // 重置状态
    pos->is_absolute = 0;
    pos->is_running = 1;
    pos->motion_state = 1;
    pos->motion_phase = 0;
    pos->planned_speed_deg = 0;
    pos->pos_finish_flag = 0;
    
    // 重置PID积分
    position_pi.integral = 0;
    position_pi.erro = error;
    speed_pi.integral = 0;
    
    // 根据运动距离调整PID参数
    if (pos->total_distance > 200.0f) {
        position_pi.kp = 0.4f;
        position_pi.ki = 0.002f;
    } else if (pos->total_distance < 20.0f) {
        position_pi.kp = 0.15f;
        position_pi.ki = 0.0005f;
    } else {
        position_pi.kp = 0.3f;
        position_pi.ki = 0.001f;
    }
    
    // 配置速度环
    speed_pi.output_max = connect_crt.max_current;
    speed_pi.output_min = -connect_crt.max_current;
    speed_pi.kp = 0.015f;
    speed_pi.ki = 0.00015f;
    speed_pi.integral = 0;
    
    if (pos->accel > 2000.0f) {
        speed_pi.kp = 0.025f;
        speed_pi.ki = 0.00025f;
    } else if (pos->accel < 500.0f) {
        speed_pi.kp = 0.008f;
        speed_pi.ki = 0.00008f;
    }
}






// 停止位置环运动
void PositionControl_Stop(PositionControl_TypeDef* pos)
{
    // 停止运动，但切换到保持模式
    pos->is_running = 0;
    pos->motion_state = 0;
    pos->motion_phase = 3;
    pos->planned_speed_deg = 0;
    pos->pos_finish_flag = 1;
    
		pos->target_position=PositionControl_GetCurrentPosition();
    // 重置PID积分
    position_pi.integral = 0;
    speed_pi.integral = 0;
    
    // 停止电机输出
    connect_crt.speed = 0;
    speed_pi.target = 0;
    set_uduq(&m1_foc, 0, 0);
    foc_open(&m1_foc, 0);
}

// 复位位置环状态（在每次运动前调用）
void PositionControl_ResetState(PositionControl_TypeDef* pos)
{
    pos->first_read = 1;
    pos->total_angle = 0;
    pos->last_raw_angle = 0;
    pos->planned_speed_deg = 0;
    pos->position_error = 0;
    position_pi.integral = 0;
    speed_pi.integral = 0;
}

// 位置环复位（找零点）
void PositionControl_Reset(PositionControl_TypeDef* pos)
{
    // 先向负方向运动找限位
    connect_crt.speed = -120;
    speed_pi.target = connect_crt.speed;
    pos->target_position-=36000000;
    // 复位标志设置，在限位检查中处理
    reset_flag = 1;
    pos->is_running = 1;
    pos->motion_state = 0;  // 复位状态
    
    // 注意：复位后 if_read_pos_start 会被清零
    // 在限位触发时会调用 PositionControl_SetZero 重新初始化
}

// 位置环零点设置
void PositionControl_SetZero(PositionControl_TypeDef* pos)
{
    // 设置硬件零点
    angle_zero = mt6816_count;
    
    // 标记为已读取ROM零点（下次上电可以使用）
    if_read_pos_start = 1;
    
    // 重新初始化累计器（以当前位置为基准）
    EncoderAccumulator_Init(&encoder_acc);
    
    // 更新位置环
    pos->current_position = encoder_acc.angle_accum;
    pos->target_position = encoder_acc.target_angle_acc;
    pos->start_position = 0;
    pos->position_error = 0;
    pos->pos_finish_flag = 1;
    pos->is_running = 0;
    
    position_pi.integral = 0;
    speed_pi.integral = 0;
    connect_crt.speed = 0;
    speed_pi.target = 0;
}

// 限位判断函数 - 检查目标位置是否超出限位
uint8_t PositionControl_CheckLimit(float target_pos)
{
    // 根据限位开关判断
    // GPIOA_PIN_2 - 右限位（正方向）
    // GPIOA_PIN_3 - 左限位（负方向）
    // 假设当前位置在限位之间，判断目标是否会导致超出限位
    // 这里简化处理，实际需要根据机械限位位置设置软限位
    
    float current_pos = PositionControl_GetCurrentPosition();
    float delta = target_pos - current_pos;
    
    if (delta > 0 && HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == 1) {
        // 正方向运动但右限位触发
        return 1;
    }
    if (delta < 0 && HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == 1) {
        // 负方向运动但左限位触发
        return 1;
    }
    return 0;
}












