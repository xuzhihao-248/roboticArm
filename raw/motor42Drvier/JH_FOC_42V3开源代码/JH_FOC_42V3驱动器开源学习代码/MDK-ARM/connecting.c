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


#include "connecting.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "ctype.h"
#include "b_motor_foc.h"
#include "PID.h"
#include "mt6816.h"
#include "math.h"
#include "mos_set.h"
#include "usbd_cdc_if.h"
#include "can.h"
#include "myflash.h"
#include "filter.h"
#include "iwdg.h"
#include "position_foc.h"

//////////////////定义控制过程的参数
connect_crt_TypeDef connect_crt; 
uint8_t llimit_flag=0;//触发反转限位标记
uint8_t rlimit_flag=0;
uint8_t reset_flag=0;//是否在复位标记
extern float  vol,temperature ;//定义ADC物理量
uint16_t my_can_id=0x200;
float speed_send=0;
uint8_t pos_finish_flag=0;//电机位置环运动到位标记
extern float  PID_factor;

//1分别存储PID因子，2CANID，3角度环的零点编码器值,4是否启用上电自动运行速度环；5限位后是否反转，6，上电启动速度,7上电启动加速度，8上电启动最大电流,9延时反转,
//10,一阶段弱磁转变速度，11，二阶段弱磁转变速度
//12,上电位置环运行是否启用，13，APOS位置1，14，APOS位置2，15，最大速度，16，加减加速度，17，最大电流，18，命令后延时,19.上电是否复位,20,CANBRate
uint32_t save_data[20]={0,0,0,0,0,0,0,0,0,400,900,0,0,180,100,1000,1,1000,0,6};
uint16_t angle_zero=0;//角度环零点编码器计数,支持位置环单圈0点
uint8_t return_flag=0;//定义速度/位置/角度/编码器回传标记位
uint8_t return_flag_can=0;//CAN总线的回传标记
uint16_t return_delay=40; //回传延迟
extern  uint16_t time_num;
//uint8_t pos_erro_flag=0;//
uint8_t if_read_pos_start=1;//上电默认读取,是否读取位置环单圈0点，位置环复位后置0，不能再读取，只有设置位置环0点后才能继续读取

uint8_t syn_flag=0;//同步标记位：A0:电流环，B0:速度环，C0:位置环，D0:角度环,其他则没有同步
float syn_value[3]={0};//指令执行的值
uint8_t motion_set=0;//位置环主动设置角度标记位，主动设置时重置为0

extern PID_Controller_t weak_mag_id;
extern PID_Controller_t weak_mag_iq;//iq弱磁pid
extern float w_Uq;//Uq轴弱磁输出量

extern uint8_t connect_type;//定义通信类型0：USB，1:CAN
extern PCD_HandleTypeDef hpcd_USB_FS;

uint8_t config_st_en=0;//是否启用上电自动运行速度
uint8_t config_st_dr=0;//限位触发是否反转
///
int32_t config_st_speed=0;//上电启动最大速度
uint32_t config_st_delay=10;//运动延时

uint8_t config_st_en_p=0;//上电是否启用位置环自动运行
float config_st_apos1=0;
float config_st_apos2=0;
uint8_t config_st_reset=0;//上电是否先复位再区间运动

uint32_t dr_delay_count=0;//用于限位触发停止后计时延时反转;
uint8_t st_apos_flag=3;//上电位置运动标记

void stop_motor(connect_crt_TypeDef* connect)
{
		 weak_mag_id .integral =0;weak_mag_iq.integral  =0;
		 w_Uq =0;
		 m1_foc .tar_Id=0;m1_foc .tar_Iq=0;m1_foc .iq_inter=m1_foc .id_inter=0;
	   speed_pi .integral =0;
  if(connect ->motor_mode ==1)
	{
	//	connect ->motor_mode =0;
	//	HN1_set(900);HN2_set(900);HN3_set(900);HN4_set(900);
		m1_foc.tar_Iq=0;
	}
	else if(connect ->motor_mode ==2)
	{
		connect_crt .speed=0;
	  speed_pi .target =0;
	}
	else if(connect ->motor_mode ==3)
	{
		        // 位置环强制停止
        PositionControl_Stop(&position_ctrl);
        connect_crt.speed = 0;
        speed_pi.target = 0;
							///需要添加位置环强制停止代码
	}
	else if(connect ->motor_mode ==4)
	{
		
	}

}


void init_connect_crt(connect_crt_TypeDef* connect)//初始化控制参数
{
 connect ->motor_set=0;
 connect ->motor_mode=0;//默认空闲模式，无输出
 connect ->drive_current=0;
	connect ->max_current =2;
 connect ->speed=0;
 connect ->distance =0;
	
	 connect ->accel=400;
	 connect ->decel =400;
	 connect ->s_acc =1000;
	 connect ->max_speed=120;	
	if(mt6816_flag ==1)
	{
		config_st_en =(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x40C));
	  config_st_dr =(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x410));
	  config_st_speed =(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x414));
		
		if(config_st_en==0x01)
		{
			init_encoder_update();
		  if(config_st_speed>3000)config_st_speed=3000;else if(config_st_speed<-3000)config_st_speed =-3000;
			uint16_t  bb=(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x41C));
			connect ->max_current =bb*0.01;//设置最大电流
			bb =(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x418));//设置加速度
			connect ->s_acc =bb;
			if(connect ->s_acc>100000)connect ->s_acc=100000;
			config_st_delay =2*(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x420));
			connect ->speed =config_st_speed ;//设置速度
			connect ->motor_mode=2; //设置速度模式
		}
		else
		{
			config_st_en_p =(*(__IO uint8_t *)(FLASH_USER_START_ADDR +0x42C));
			if(config_st_en_p==1)
			{
				int pp=(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x430));
				config_st_apos1=0.01*pp;
				pp=(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x434));
				config_st_apos2=0.01*pp;
				//读取最大速度
				pp= (*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x438));
				if(pp>3000)pp=3000;
				connect ->max_speed=pp;
				//读取加速度
				pp= (*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x43C));
				if(pp>100000)pp=100000;
				connect ->accel=connect ->decel =pp;
				//读取电流
				pp= (*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x440));
				connect ->max_current=0.01*pp;
			  //读取延时
				config_st_delay=2*(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x444));
				//读取上电是否先复位
				config_st_reset =(*(__IO uint8_t *)(FLASH_USER_START_ADDR +0x448));
				init_encoder_update();
				if(if_read_pos_start ==1)
				{
						angle_zero =	save_data[2]=(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x408));//读取单圈角度零点
						if(angle_zero >16834)angle_zero =0;
				}					
				
							///需要添加位置环初始化代码
 // 1. 先初始化位置环（不设置目标位置）
        PositionControl_Init(&position_ctrl, 
                             connect->max_speed, 
                             connect->accel, 
                             connect->decel,
                             0.3f, 0.5f);
        
        // 2. 设置PID参数
        position_pi.kp = 0.6 * PID_factor;
        position_pi.ki = 0.001 * PID_factor;
        position_pi.kd = 0.0;
        
        // 3. 读取零点
        if(if_read_pos_start == 1)
        {
            angle_zero = save_data[2] = (*(__IO uint32_t *)(FLASH_USER_START_ADDR + 0x408));
            if(angle_zero > 16834) angle_zero = 0;
        }
        
        // 4. 第一次调用GetCurrentPosition会设置target_position和current_position
        // 不需要显式设置，让它在首次读取时自动初始化
        float init_pos = PositionControl_GetCurrentPosition();
        position_ctrl.target_position = init_pos;  // 明确设置初始目标
        position_ctrl.current_position = init_pos;
        
        PID_Controller_Init(&speed_pi, 0.015, 0.00015, 0.000, 0,
                           -connect->max_current, connect->max_current);
        connect_crt.motor_mode = 3;
        
        if(config_st_reset != 1)
        {
            PositionControl_StartAbsolute(&position_ctrl, config_st_apos1);
            st_apos_flag = 1;
        }
        else
        {
            PositionControl_Reset(&position_ctrl);
            reset_flag = 1;
        }
			}
	  }
		
	}
	
	 PID_Controller_Init(&speed_pi,0.015,0.00015,0.000,0,-connect ->max_current,connect ->max_current);
   PID_Controller_Init(&AngleControl.pid_angle,0.05,0.0001,0.8,0,-connect_crt .max_current ,connect_crt .max_current);		
}



void check_limit(void)//限位检查
{
	///////////上电运行位置环代码,位置区间运动
   if(config_st_en_p==1)
   {
			 if(st_apos_flag<2)//复位完成后才计数
			 {
				dr_delay_count ++;if(dr_delay_count >99999999)dr_delay_count =99999999;
			 }
			if(dr_delay_count >config_st_delay)
			{
				if(st_apos_flag ==0)
				{															
							if(pos_finish_flag !=0)
							{
								stop_motor (&connect_crt);			
							}								  

							                // 运行到位置1
                if(!PositionControl_CheckLimit(config_st_apos1))
                {
                    PositionControl_StartAbsolute(&position_ctrl, config_st_apos1);
                }
						 st_apos_flag =1;
				}
				else if(st_apos_flag ==1)
			 { 
						if(pos_finish_flag !=0)
						{
							stop_motor (&connect_crt);	
						}								  		
							///需要添加位置环上电运动绝对代码
						                // 运行到位置2
                if(!PositionControl_CheckLimit(config_st_apos2))
                {
                    PositionControl_StartAbsolute(&position_ctrl, config_st_apos2);
                }
						st_apos_flag =0;
			 }
				dr_delay_count=0;
			}
	 }   
	 //上电速度自启动与限位触发
   if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_2)==1&&rlimit_flag ==0)
	 {
		 
		 if(config_st_dr ==1&&config_st_en ==1)
		 {
			 dr_delay_count ++;if(dr_delay_count >99999999)dr_delay_count =99999999;
			 stop_motor (&connect_crt);
			 if(dr_delay_count >=config_st_delay )
			 {
		    connect_crt .speed=-abs(config_st_speed);
				 dr_delay_count =0;
				 rlimit_flag =1;
			 }
		 }
		 else
		 {
				stop_motor (&connect_crt);
			  rlimit_flag =1;
			 	if(connect_type==0)
				CDC_Transmit_FS ("Limit now!\n",11);	
				else
				{
					set_cantx_buf (0xFF,0xF1,0x00,0,0,0,0,0);//成功返回存在限位的代码
			    CAN_Transmit(&my_can_tx );
				}
		 }
	 }
	 else if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_2)==0&&rlimit_flag ==1)
		 rlimit_flag =0;
	 
	  if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_3)==1&&llimit_flag ==0)
	 {
		 
		 if(config_st_dr ==1&&config_st_en ==1)
		 {
			 dr_delay_count ++;if(dr_delay_count >99999999)dr_delay_count =99999999;
			 stop_motor (&connect_crt);
			 if(dr_delay_count >=config_st_delay )
			 {
			   connect_crt .speed=abs(config_st_speed);
				 dr_delay_count =0;
				 llimit_flag =1;
			 }
		 }
		 else
		 {
			 llimit_flag =1;
			 stop_motor (&connect_crt);
			 if(reset_flag ==1)//如果是复位触发的
			 {
				 if_read_pos_start=0;//不再使用ROM保存的数据作为位置环单圈0点，需设置设置为0点才能置1;
					angle_zero =mt6816_count ;//复位后以当前位置为零点，但是不保存到rom中

				                 // 复位完成
                if_read_pos_start = 0;
                angle_zero = mt6816_count;
                position_ctrl.current_position = 0;
                position_ctrl.target_position = 0;
                position_ctrl.start_position = 0;
                position_ctrl.position_error = 0;
                position_ctrl.is_running = 0;
                pos_finish_flag = 1;
                reset_flag = 0;
				 EncoderAccumulator_Init(&encoder_acc);
				 ///需要添加位置环复位后的代码
				 return ;
			 }

			 	if(connect_type==0)
				CDC_Transmit_FS ("Limit now!\n",11);	
				else
				{
					set_cantx_buf (0xFF,0xF1,0x00,0,0,0,0,0);//成功返回存在限位的代码
			    CAN_Transmit(&my_can_tx );
				}
		 }
	 }
	 else if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_3)==0&&llimit_flag ==1)
		 llimit_flag =0;
}


void get_zero_by_current(void)//通过检测堵转寻找零点
{
   if(reset_flag ==1)
	 {
		 if(fabs(m1_foc.Iq) >fabs(connect_crt .max_current)||fabs(m1_foc.Iq) >2.5)
		 {
			   stop_motor (&connect_crt);
			 	 if_read_pos_start=0;//不再使用ROM保存的数据作为位置环单圈0点，需设置设置为0点才能置1;
			   angle_zero =mt6816_count ;//复位后以当前位置为零点，但是不保存到rom中
            // 更新位置环
            position_ctrl.current_position = 0;
            position_ctrl.target_position = 0;
            position_ctrl.start_position = 0;
            position_ctrl.position_error = 0;
            position_ctrl.is_running = 0;
            pos_finish_flag = 1;
            reset_flag = 0;
			 EncoderAccumulator_Init(&encoder_acc);
			 ///需要添加位置环复位后的代码
			 	if(connect_type==0)
				CDC_Transmit_FS ("Limit now!\n",11);	
				else
				{
					set_cantx_buf (0xFF,0xF1,0x00,0,0,0,0,0);//成功返回存在限位的代码
			    CAN_Transmit(&my_can_tx );
				}
				
		 }
	 }
}







void start_reset_waiting(void)//上电位置自启动需要复位时等待函数
{
	while(1)
	{
		if(connect_type ==0)//采用USB通信模式		
		usb_task ();	
		else 
		can_task ();
				HAL_IWDG_Refresh (&hiwdg );
		if(st_apos_flag==3&&config_st_en_p==1)
		{
			if(llimit_flag ==1&&pos_finish_flag !=2)//已经复位，运行到2位置
			{
			 motion_set=0;
			 ///需要添加位置环绝对运动代码;//运行2
				                if(!PositionControl_CheckLimit(config_st_apos2))
                {
                    PositionControl_StartAbsolute(&position_ctrl, config_st_apos2);
                }
			 pos_finish_flag =2;			
			}
			else if(pos_finish_flag==0)//运行到2位置了，运行到1位置
			{
				//	 AbsolutePositionMotion_Start (&my_motion ,&encoder_acc ,config_st_apos1);//运行1
				                if(!PositionControl_CheckLimit(config_st_apos1))
                {
                    PositionControl_StartAbsolute(&position_ctrl, config_st_apos1);
                }
					dr_delay_count=0.5*config_st_delay;
					 st_apos_flag=1;

					return ;
			}
			
		}
		else
			return ;
		
 }


}

	//////////////////////////////////////////////将指令按照某个字符串分段存储
uint8_t split_buf(uint8_t *s,uint8_t split_buf[10][10],char sp)
 {
	 uint16_t i = 0, j = 0, num = 0;
	for (i = 0; s[i] != 0; i++)
	{
		if (s[i] != sp)
		{
			split_buf[num][j] = s[i];
			j++;
			if(j>11) break ;
		}
		else 
		{
			num++; j = 0; 
		}
		
	}
	return num+1;
	}


//十六进制字符传转换成16进制数字
uint16_t hex4ToUint16( char hex[4]) {
    uint16_t result = 0;
    
    for(int i = 0; i < 4; i++) {
        char c = toupper(hex[i]);
        uint8_t value = (c >= 'A') ? (c - 'A' + 10) : (c - '0');
        result = (result << 4) | value;
    }
    
    return result;
}
////十六进制数转换成字符串
void uint16ToHex(uint16_t num, char hex[5]) {
    const char hexChars[] = "0123456789ABCDEF";
    hex[0] = hexChars[(num >> 12) & 0xF];
    hex[1] = hexChars[(num >> 8) & 0xF];
    hex[2] = hexChars[(num >> 4) & 0xF];
    hex[3] = hexChars[num & 0xF];
    hex[4] = '\0';
}
//限制范围函数
void limit_max_min(float *value, float max, float min)
{
  if (*value > max) *value = max;
  else if (*value < min) *value = min;
}

// 将任意角度归一化到 [-180, 180) 范围
float normalize_angle_to_180(float angle) {
    // 首先归一化到 [0, 360) 范围
    angle = fmodf(angle, 360.0f);
    if (angle < 0) {
        angle += 360.0f;
    }
    
    // 转换到 [-180, 180) 范围
    if (angle > 180.0f) {
        angle -= 360.0f;
    }
    
    return angle;
}


void usb_task(void)//分析usb串口接收到的数据进行指令操作
{
 if(my_usb_len >0)
 {
	 my_usb_len =0;
	 HAL_GPIO_WritePin (GPIOB ,GPIO_PIN_12 ,GPIO_PIN_SET );//收到消息灯灭一下
	 uint8_t message_rx[10][10]={0,0,0,0,0,0,0,0,0,0};
			split_buf (my_usb_buf ,message_rx ,'+');//将每一段指令再进行拆解
			
			if(strncmp(message_rx [0],"JH0",3)==0)////判断命令头是否正确
				{
					
		
					 //////////////////////////////////////////设置电机运行模式
					 if(strncmp(message_rx[1],"Mode",4)==0)
					{
					   if(strncmp(message_rx[2],"IDLE",4)==0)
						 {
							  w_Uq=0;m1_foc .tar_Id =0;m1_foc .id_inter =0;//弱磁部分清零
							  connect_crt .motor_mode =0;	 
								set_uduq (&m1_foc ,0,0);
						    foc_open  (&m1_foc ,0);
							  CDC_Transmit_FS ("SetMode Success!\n",17);//注意usb连续发送数据的话间隔延迟要大于1ms
						 }	
             else if(strncmp(message_rx[2],"Current",7)==0&&connect_crt .motor_mode !=1)	
						 {   
							  if(mt6816_flag )
								{
						     connect_crt .motor_mode =1;
									connect_crt .drive_current=0;
									 w_Uq=0;m1_foc .tar_Id =0;m1_foc .id_inter =0;//弱磁部分清零
								 set_foc_Iqcurrent(&m1_foc ,connect_crt .drive_current);//切换电流模式后无输出，需要设置
								 CDC_Transmit_FS ("SetMode Success!\n",17);//注意usb连续发送数据的话间隔延迟要大于1ms
								}
								else CDC_Transmit_FS ("NotFind encoder!\n",17);
						 }		
					   else if(strncmp(message_rx[2],"Speed",5)==0&&(connect_crt .motor_mode !=1||connect_crt .drive_current ==0)&&connect_crt .motor_mode !=2)	//电流环模式下不能直接切换
						 {   
							  if(mt6816_flag)
								{
									init_encoder_update();
									connect_crt .speed=0;
									PID_Controller_Init(&speed_pi,0.015,0.00015,0.0,0,-connect_crt .max_current ,connect_crt .max_current);//设置速度								
						      connect_crt .motor_mode =2;
									CDC_Transmit_FS ("SetMode Success!\n",17);
								}
								else CDC_Transmit_FS ("NotFind encoder!\n",17);
						 }	
					   else if(strncmp(message_rx[2],"Pos",3)==0&&(connect_crt .motor_mode !=1||connect_crt .drive_current ==0)&&connect_crt .motor_mode !=3)	
						 {
							  if(mt6816_flag)
								{
									init_encoder_update();

													
													// 设置位置PID参数
													position_pi.kp = 0.6 * PID_factor;
													position_pi.ki = 0.001 * PID_factor;
													position_pi.kd = 0.0;
													
													if(if_read_pos_start == 1)
													{
															angle_zero = save_data[2] = (*(__IO uint32_t *)(FLASH_USER_START_ADDR + 0x408));
															if(angle_zero > 16834) angle_zero = 0;
														
													}
																										// 位置环初始化
													PositionControl_Init(&position_ctrl, 
																							 connect_crt.max_speed, 
																							 connect_crt.accel, 
																							 connect_crt.decel,
																							 0.3f, 0.5f);
													PID_Controller_Init(&speed_pi, 0.015, 0.00015, 0.000, 0,
																						 -connect_crt.max_current, connect_crt.max_current);
													
													connect_crt.motor_mode = 3;
													CDC_Transmit_FS("SetMode Success!\n", 17);
								}
								else CDC_Transmit_FS ("NotFind encoder!\n",17);
						 }							
					   else if(strncmp(message_rx[2],"Angle",5)==0&&(connect_crt .motor_mode !=1||connect_crt .drive_current ==0))	
						 {
							   if(mt6816_flag){		

                   PID_Controller_Init(&AngleControl.pid_angle,0.03,0.0000,1.2,0,-connect_crt .max_current ,connect_crt .max_current);						
									 angle_zero =	save_data[2]=(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x408));//读取角度环零点
									 if(angle_zero >16834)angle_zero =0;
									 AngleControl .zero_position=DIR*angle_zero  *0.02197265625f;//将存储数据设置为零点
									 // AngleControl_SetZero(&AngleControl);   
                   AngleControl .pid_angle.target	=DIR*(mt6816_count-angle_zero) *0.02197265625f;	//就以当前角度作为目标角度							 
									 connect_crt .motor_mode =4;
									 CDC_Transmit_FS ("SetMode Success!\n",17);
								 }
								 else CDC_Transmit_FS ("NotFind encoder!\n",17);
						 }
						 
					}
				  ///////////////////////////////////////////////////////////
				 	else if(strncmp(message_rx[1],"SetCurr",7)==0)//设置电机恒流电流值
					   {
							  connect_crt .drive_current= atof(message_rx[2]);
                if(connect_crt .motor_mode ==1)
								{
								  if((llimit_flag ==1&&connect_crt .drive_current  <0)||(rlimit_flag ==1&&connect_crt .drive_current  >0))//限位
								 { 
									 connect_crt .drive_current =0;
									 CDC_Transmit_FS ("Limit now!\n",11);
								 }
								 set_foc_Iqcurrent(&m1_foc ,connect_crt .drive_current);
								}
								connect_crt .max_current =fabs (connect_crt .drive_current);
								if(connect_crt .max_current>System_MAX_I )connect_crt .max_current=System_MAX_I ;//限流
							  AngleControl .pid_angle.output_max= speed_pi .output_max=connect_crt .max_current;
							  AngleControl .pid_angle.output_min= speed_pi .output_min =-connect_crt .max_current;	
                								
  	            CDC_Transmit_FS ("SetCurrent Success!\n",20);
						 }	
					/////////////////////////////////设置速度环速度
          	else if(strncmp(message_rx[1],"Speed",7)==0)
						{				
							if(connect_crt .motor_mode ==2){
							   connect_crt .speed =atof(message_rx[2]);
                 if((llimit_flag ==1&&connect_crt .speed <0)||(rlimit_flag ==1&&connect_crt .speed >0))//限位
								 { 
									 connect_crt .speed=0;
									 CDC_Transmit_FS ("Limit now!\n",11);
								 }
								 else CDC_Transmit_FS ("SetSpeed Success!\n",18);
								 
								  if(connect_crt .speed >5000)connect_crt .speed =5000;
                  else if(connect_crt .speed <-5000)connect_crt .speed =-5000;						 								
							}
							else 	CDC_Transmit_FS ("Mode Error!\n",12);
						}	
						
						else if(strncmp(message_rx[1],"SAcc",4)==0)
						{
							if(connect_crt .motor_mode ==2){
							   connect_crt .s_acc =fabs (atof(message_rx[2]));	
									limit_max_min( &connect_crt .s_acc,10000,1)	;//限制加速度						
								CDC_Transmit_FS ("SetSAcc Success!\n",18);
							}
							else 	CDC_Transmit_FS ("Mode Error!\n",12);
						
						}
          //////////////////////////////////位置环设置
            else if(strncmp(message_rx[1],"Posinit",7)==0)//设置电机位置环速度参数
						{				
							if(connect_crt .motor_mode ==3){
							   connect_crt .max_speed  =fabs(atof(message_rx[2]));
								 connect_crt .accel =fabs (atof(message_rx[3]));
								 connect_crt .decel =fabs (atof(message_rx[4]));
								 limit_max_min(&connect_crt .accel,200000,1);
								 limit_max_min(&connect_crt .decel,10000,1);
								 limit_max_min(&connect_crt .max_speed,3000,1);
								        // 更新位置环参数
									position_ctrl.max_speed = connect_crt.max_speed;
									position_ctrl.accel = connect_crt.accel;
									position_ctrl.decel = connect_crt.decel;
									position_pi.output_max = connect_crt.max_speed;
									position_pi.output_min = -connect_crt.max_speed;
								 CDC_Transmit_FS ("InitPos Success!\n",17);
							}
							else 	CDC_Transmit_FS ("Mode Error!\n",12);
						}
            else if(strncmp(message_rx[1],"RPos",4)==0)	//位置环相对运动角度设置
						{
								if(connect_crt.motor_mode == 3)
								{
										float delta = atof(message_rx[2]);
										float current_pos = PositionControl_GetCurrentPosition();
										float target_pos = current_pos + delta;
										
										if(PositionControl_CheckLimit(target_pos))
										{
												connect_crt.distance = 0;
												CDC_Transmit_FS("Limit now!\n", 11);
										}
										else
										{
												PositionControl_StartRelative(&position_ctrl, delta);
												CDC_Transmit_FS("RPos Started!\n", 14);
										}
								}
								else
										CDC_Transmit_FS("Mode Error!\n", 12);
					  }
						else if(strncmp(message_rx[1],"APos",4)==0)//位置环绝对角度运动
						{
									if(connect_crt.motor_mode == 3)
									{
											float target_pos = atof(message_rx[2]);
											
											if(PositionControl_CheckLimit(target_pos))
											{
													connect_crt.distance = 0;
													CDC_Transmit_FS("Limit now!\n", 11);
											}
											else
											{
													PositionControl_StartAbsolute(&position_ctrl, target_pos);
													CDC_Transmit_FS("APos Started!\n", 14);
											}
									}
									else
											CDC_Transmit_FS("Mode Error!\n", 12);							
						}
						else if(strncmp(message_rx[1],"PZero",5)==0)//设置当前位置为位置环零点
						{
							
								if(connect_crt .motor_mode ==3){

									 save_data [2]=angle_zero=mt6816_count ;//保持单圈0点
	
									if_read_pos_start=1;//重新启用ROM数据作为0点
									
								 PositionControl_SetZero(&position_ctrl);

						      Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data );//写入存储

									CDC_Transmit_FS ("PZero Success!\n",15);
								}
								else 	CDC_Transmit_FS ("Mode Error!\n",12);
						
						}
					  else if(strncmp(message_rx[1],"Reset",5)==0)//复位找零点
						{
							
								if(connect_crt .motor_mode ==3){
									if(llimit_flag ==0)//没有在反转限位时才能复位
									{
										 PositionControl_Reset(&position_ctrl);
										reset_flag =1;
										CDC_Transmit_FS ("Reset Success!\n",15);
									}
									else
									{
														                 // 复位完成
                if_read_pos_start = 0;
                angle_zero = mt6816_count;
                position_ctrl.current_position = 0;
                position_ctrl.target_position = 0;
                position_ctrl.start_position = 0;
                position_ctrl.position_error = 0;
                position_ctrl.is_running = 0;
                pos_finish_flag = 1;
                reset_flag = 0;
                     ///需要添加位置环复位代码 
										 reset_flag =0; 
										CDC_Transmit_FS ("Limit now!\n",11);	
									}
								}
								else 	CDC_Transmit_FS ("Mode Error!\n",12);
						
						}
			
													
						///////////////////////////////////////角度环
						else if(strncmp(message_rx[1],"Angle",5)==0)//设置当前位置为位置环零点
						{
							
								if(connect_crt .motor_mode ==4){
									connect_crt .distance =atof(message_rx[2]);
									AngleControl_SetTargetAngle(&AngleControl,connect_crt .distance );
									CDC_Transmit_FS ("SetAngle Success!\n",18);
								}
								else 	CDC_Transmit_FS ("Mode Error!\n",12);
						
						}
						
						else if(strncmp(message_rx[1],"AZero",5)==0)
						{
								if(connect_crt .motor_mode ==4){
									AngleControl_SetZero(&AngleControl);

									 save_data [2]=angle_zero=mt6816_count ;

						      Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data );//写入存储
									CDC_Transmit_FS ("AZero Success!\n",16);
								}
									else 	CDC_Transmit_FS ("Mode Error!\n",12);
						}
						
					 /////////////////////////////编码器校准
				   else if(strncmp(message_rx[1],"Adjust",6)==0)
				  	{
								connect_crt .motor_mode =0;//开环模式下校准
								set_uduq (&m1_foc ,0,0);
						    foc_open  (&m1_foc ,0);
						 	HAL_Delay (50);
							if(check_mt6816 ())
							{
								CDC_Transmit_FS ("Adjusting!Device will be reseted later!\n",42);						
						  	HAL_Delay (10);
							  REIN_mt6816_spi_data_Signal_Init();

							}
							 else CDC_Transmit_FS ("NotFind encoder!\n",17);
						}
					 else if(strncmp(message_rx[1],"STOP",4)==0)
						 {
							 stop_motor (&connect_crt );
							 CDC_Transmit_FS ("STOP Success!\n",14);
							 HAL_Delay (10);
											 
						 }
					 else if(strncmp(message_rx[1],"RESETMCU",8)==0)//重启
					 {
													//复位前注意释放关键硬件资源
								 if(connect_type ==0)//失能CAN通信
						 HAL_CAN_MspDeInit (&hcan);
						 else 
						 {
							HAL_PCD_DeInit(&hpcd_USB_FS);//失能USB通信
							CAN_Init();//初始化总线
						 }
               HAL_Delay (20);//延时片刻
					    __disable_irq();          // 关闭所有中断
              HAL_NVIC_SystemReset();   // 执行复位
					 }
					 else if(strncmp(message_rx[1],"PIDfactor",9)==0)//设置PID因子
					 {
						  PID_factor =atof(message_rx[2]);
						  if(PID_factor >10||PID_factor <0.1)
							{
							PID_factor =1;CDC_Transmit_FS ("PIDfactor Error!\n",17);
							}
							else CDC_Transmit_FS ("PIDfactor Success!\n",19);
							can_foc_init();//重新初始化参数
              save_data [0]=PID_factor *100;

						  Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data );//调节因子写入存储
					 }
					 else if(strncmp(message_rx[1],"ConfigSTS",9)==0)//设置上电是否启动速度环参数
					 {
						 save_data [11]=0;//上电位置环禁用
						 
						  connect_crt .speed =0;
						  HAL_Delay (1000);
						  save_data [3] =atoi(message_rx[2]);
						  save_data [4]=atoi(message_rx[3]);
						  int32_t p=atoi(message_rx[4]);if(p>3000)p=3000;else if(p<-3000)p=-3000;
						  save_data [5]=p;
              save_data [6]=abs(atoi(message_rx[5]));
              save_data [7]=fabs(atof(message_rx[6]))*100	;					 
						  save_data [8]=abs(atoi(message_rx [7]));
						 

						 
						  Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data );//写入存储
						  CDC_Transmit_FS ("ConfigSTS Success! Reset Please!\n",33);
					 }
					else if(strncmp(message_rx[1],"ConfigSTP",9)==0)//设置上电是否启动位置环参数
					 {
						 //12,上电位置环运行是否启用，13，APOS位置1，14，APOS位置2，15，最大速度，16，加减加速度，17，最大电流，18，命令后延时,19，复位后再启用区间运动
						  int32_t apos=0;
						  save_data [3]=0;//上电速度环禁用

						  save_data [11] =atoi(message_rx[2]);
						  apos=atof(message_rx[3])*100;		
							save_data[12] =apos ;						 
						  apos=atof(message_rx[4])*100;
						  save_data [13]=apos;
              save_data [14]=abs(atoi(message_rx[5]));
              save_data [15]=abs(atoi(message_rx[6]))	;					 
						  save_data [16]=fabs(atof(message_rx [7]))*100;
						  save_data [17]=abs(atoi(message_rx [8]));
							save_data [18]=atoi(message_rx[9]);
						 
						  Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data );//写入存储
						  CDC_Transmit_FS ("ConfigSTP Success! Reset Please!\n",33);
					 }
					 else if(strncmp(message_rx[1],"ConfigWK",8)==0)//
					 {
						  save_data [9] =abs(atoi(message_rx[2]));
						  save_data [10]=abs(atoi(message_rx[3]));

						  Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data );//调节因子写入存储
						 CDC_Transmit_FS ("ConfigWK Success! Reset Please!\n",32);
					 }
					 else if(strncmp(message_rx[1],"Gmode",6)==0)//获取当前模式
					 {
						 uint8_t send_data[6];
						 memcpy (send_data ,"mode",4);
             send_data[4] = connect_crt.motor_mode + '0';  // 数字转字符
						 send_data[5] = '\n';  // 添加换行符
					   CDC_Transmit_FS(send_data,6);
					 }
						else if(strncmp(message_rx[1],"Gspeed",6)==0)//获取速度
						{
								char send_str[32];
								int len = snprintf(send_str, sizeof(send_str), "speed%.3f\n", speed_send);
								if (len > 0 && len < sizeof(send_str)) {
										CDC_Transmit_FS((uint8_t*)send_str, len);
								}
						}
						else if(strncmp(message_rx[1],"Gpos",4)==0)//获取位置
						{
								char send_str[32];
								float pos = PositionControl_GetCurrentPosition();
								int len = snprintf(send_str, sizeof(send_str), "pos%.3f\n", pos);
								if (len > 0 && len < sizeof(send_str)) {
										CDC_Transmit_FS((uint8_t*)send_str, len);
								}
						}
						else if(strncmp(message_rx[1],"Gtemp",5)==0)//获取温度
						{
								char send_str[32];
								int len = snprintf(send_str, sizeof(send_str), "temp%.3f\n", temperature);
								if (len > 0 && len < sizeof(send_str)) {
										CDC_Transmit_FS((uint8_t*)send_str, len);
								}
						}
						else if(strncmp(message_rx[1],"Gangle",6)==0)//获取角度
						{
								char send_str[32];
								int len = snprintf(send_str, sizeof(send_str), "angle%.3f\n", normalize_angle_to_180(DIR*(mt6816_count-angle_zero) *0.02197265625f));
								if (len > 0 && len < sizeof(send_str)) {
										CDC_Transmit_FS((uint8_t*)send_str, len);
								}
						}
						else if(strncmp(message_rx[1],"Gvol",4)==0)//获取电压
						{
								char send_str[32];
								int len = snprintf(send_str, sizeof(send_str), "vol%.3f\n", vol);
								if (len > 0 && len < sizeof(send_str)) {
										CDC_Transmit_FS((uint8_t*)send_str, len);
								}
						}
				    else if(strncmp(message_rx[1],"Gtorque",7)==0)//获取当前扭矩
						{
								char send_str[32];
								int len = snprintf(send_str, sizeof(send_str), "torque%.3f\n", m1_foc.Iq);
								if (len > 0 && len < sizeof(send_str)) {
										CDC_Transmit_FS((uint8_t*)send_str, len);
								}
						}
						else if(strncmp(message_rx[1],"StartRD",7)==0)//开启数据回传
						{
								return_flag = 1;//回传标记位开启
								uint16_t return_HZ;
								return_HZ = atoi(message_rx[2]);//获取回传频率
								if(return_HZ > 100) return_HZ = 100;
								if(return_HZ < 1) return_HZ = 1;
								return_delay = 5000 / return_HZ;//存在延迟，用500除以
						}
						else if(strncmp(message_rx[1],"StopRD",6)==0)
						{
								return_flag = 0;//关闭回传
							CDC_Transmit_FS ("StopRD Success!\n",16); 
						}
						else if(strncmp(message_rx[1],"Gencoder",8)==0)//获取编码器值
						{
								char send_str[32];
								int len = snprintf(send_str, sizeof(send_str), "encoder%d\n", mt6816_count);
								if (len > 0 && len < sizeof(send_str)) {
										CDC_Transmit_FS((uint8_t*)send_str, len);
								}
						}
					 else if(strncmp(message_rx[1],"Gcanid",6)==0)
					 {
					   char  ff[6]={0};
						 uint16ToHex (my_can_id ,ff);
						 ff[5] = '\n';  // 添加换行符
					   CDC_Transmit_FS((uint8_t *)ff,6);
					 }
					 else if(strncmp(message_rx[1],"GcanBR",6)==0)
					 {
             switch(can_Prescaler )
						 {
							 case 3:CDC_Transmit_FS ("Can Rate 1Mbps\n",16);	break ;		
							 case 6:CDC_Transmit_FS ("Can Rate 500kbps\n",17);	break ;		
							 case 12:CDC_Transmit_FS ("Can Rate 250kbps\n",17);	break ;		
							 case 24:CDC_Transmit_FS ("Can Rate 125kbps\n",17);	break ;	
							 case 60:CDC_Transmit_FS ("Can Rate 50kbps\n",16);	break ;	
               default :CDC_Transmit_FS ("Can Rate Erro\n",14);	break ;						 
						 }
					 }
					 else if(strncmp(message_rx[1],"Scanid",6)==0)//设置CANID
					 {
						 uint16_t id;
					  id  = hex4ToUint16 ((char*)message_rx [2]);
						 if(id<=0x7ff)
						 {
							 my_can_id =id;
							save_data [1]=my_can_id;

							Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data);//写入存储
							CDC_Transmit_FS ("SetID Success!\n",15);
						 }
						 else 
						 CDC_Transmit_FS ("SetID error!\n",13); 
					 }
					 else if(strncmp(message_rx[1],"ScanBR",6)==0)//设置CAN波特率
					 {
						 uint8_t err=0;
						 if(strncmp(message_rx[2],"1Mbps",5)==0)
						 {
						   can_Prescaler =3;
						 }
						 else if(strncmp(message_rx[2],"500kbps",7)==0)
						 {
						   can_Prescaler =6;
						 }
						 else if(strncmp(message_rx[2],"250kbps",7)==0)
						 {
						   can_Prescaler =12;
						 }
						 else if(strncmp(message_rx[2],"125kbps",7)==0)
						 {
						   can_Prescaler =24;
						 }
						 	else if(strncmp(message_rx[2],"50kbps",6)==0)
						 {
						   can_Prescaler =60;
						 }
					   else//错误
						 {
						   err =1;
						 }
						 if(err==0)
						 {
							save_data [19]=can_Prescaler ;
							Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data);//写入存储
							CDC_Transmit_FS ("SetcanBR Success!\n",19);	
						 }
             else 
						 {
						  CDC_Transmit_FS ("SetcanBR error!\n",16);	
						 }							 
					 }
            													
				   }
				return ;
	 
    }
 
			 if(time_num %return_delay ==0&&return_flag ==1)//回传数据
			 {
						uint8_t data[64] = {0};  // 适当增大缓冲区
						char s[10] = {0};        // 增大缓冲区确保足够空间
						char p[18] = {0};
						char a[10] = {0};
						char e[10] = {0};
						char q[10] = {0};//返回Iq
						int length = 0;

						// 格式化字符串，保留三位小数
						snprintf(s, sizeof(s), "%.3f", speed_send  );
						snprintf(p, sizeof(p), "%.3f", PositionControl_GetCurrentPosition());
						snprintf(a, sizeof(a), "%.3f", normalize_angle_to_180(DIR * (mt6816_count - angle_zero) * 0.02197265625f  ));
						snprintf(e, sizeof(e), "%d", mt6816_count);
            snprintf(q, sizeof(q), "%.3f",m1_foc .Iq   );
						
						// 构建完整字符串
						length = snprintf((char*)data, sizeof(data), 
							"s:%sp:%sa:%se:%sq:%s\n", 
														 s, p, a, e,q);

						// 发送数据，length+1确保包含换行符
						if (length > 0 && length < sizeof(data)) {
								CDC_Transmit_FS(data, length);  // 已经包含换行符
						} else {
								// 错误处理：数据过长
								// 可以截断或采取其他措施
								CDC_Transmit_FS(data, sizeof(data));
						}			 
			 }

}

//////////填入CAN发送缓存
void set_cantx_buf(uint8_t b0,uint8_t b1,uint8_t b2,uint8_t b3,uint8_t b4,uint8_t b5,uint8_t b6,uint8_t b7)
{
	my_can_tx .payload [0]=b0;my_can_tx .payload [1]=b1;my_can_tx .payload [2]=b2;my_can_tx .payload [3]=b3;
	my_can_tx .payload [4]=b4;my_can_tx .payload [5]=b5;my_can_tx .payload [6]=b6;my_can_tx .payload [7]=b7;
}



void intTo7Bytes(int num, uint8_t bytes[8]) {   
	  bytes[4] = (num >> 24) & 0xFF;  // 最高字节
    bytes[5] = (num >> 16) & 0xFF;
    bytes[6] = (num >> 8)  & 0xFF;
    bytes[7] = num & 0xFF;          // 最低字节
}

void setbyte4(uint8_t byte[4],uint8_t a1,uint8_t a2,uint8_t a3,uint8_t a4)//给四个字节数组赋值
{
   byte [0]=a1;byte [1]=a2;byte [2]=a3;byte [3]=a4;
}

int32_t bytesToInt32(uint8_t bytes[4]) //4字节数组转换成int32_t
{
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

int16_t bytesToInt16(uint8_t bytes0,uint8_t byte1) //4字节数组转换成int32_t
{
     return (bytes0 << 8) | byte1;

}


void syn_task(void)//执行同步命令
{
	
							 if(syn_flag ==0xA0)//执行电流环的同步
						 {
								 if(connect_crt .motor_mode ==1)
								{
										if((llimit_flag ==1&&connect_crt .drive_current  <0)||(rlimit_flag ==1&&connect_crt .drive_current  >0))//限位
									 { 
											connect_crt .drive_current =0;
											set_cantx_buf (0xff,0xf1,0,0,0,0,0,0);//返回限位错误
											CAN_Transmit(&my_can_tx );return ;
									 }
									 connect_crt .drive_current =syn_value [0];
									 set_foc_Iqcurrent(&m1_foc ,connect_crt .drive_current);
									 								syn_flag=0;
								}
						 }
						 else if(syn_flag ==0xB0)//运行速度环的同步
						 {					 
						 			if(connect_crt .motor_mode ==2)
							{

							   connect_crt .speed =syn_value [1];//设置速度																 
								 connect_crt .max_current=syn_value [0];
								 if(connect_crt .max_current>System_MAX_I )connect_crt .max_current=System_MAX_I ;//限流
								 speed_pi .output_min=-connect_crt .max_current;
							   speed_pi .output_max =connect_crt .max_current;								
								//设置加速度
								 connect_crt .s_acc=syn_value [2];
								
                 if((llimit_flag ==1&&connect_crt .speed <0)||(rlimit_flag ==1&&connect_crt .speed >0))//限位
								 { 
									 connect_crt .speed=0;
									 set_cantx_buf (0xff,0xf1,0,0,0,0,0,0);//返回限位错误
			             CAN_Transmit(&my_can_tx );
								 }
								 else 	
								{			
									 set_cantx_buf (0xdd,0xc7,0,0,0,0,0,0);//成功返回的代码
			             CAN_Transmit(&my_can_tx );
								 }
								 
								  if(connect_crt .speed >3000)connect_crt .speed =3000;
                  else if(connect_crt .speed <-3000)connect_crt .speed =-3000;	
									syn_flag =0;
							}
							else {					
								set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
			          CAN_Transmit(&my_can_tx );
							}
						 }
						// 同步任务中的位置环处理
						else if(syn_flag == 0xc0 || syn_flag == 0xc2)
						{
								if(connect_crt.motor_mode == 3)
								{
										float delta = syn_value[1];
										float current_pos = PositionControl_GetCurrentPosition();
										float target_pos = current_pos + delta;
										
										if(syn_flag == 0xc0)
										{
												connect_crt.max_current = syn_value[0];
												if(connect_crt.max_current > System_MAX_I) connect_crt.max_current = System_MAX_I;
												speed_pi.output_min = -connect_crt.max_current;
												speed_pi.output_max = connect_crt.max_current;
										}
										else
										{
												connect_crt.max_speed = syn_value[0];
												limit_max_min(&connect_crt.max_speed, 3000, -3000);
												position_ctrl.max_speed = connect_crt.max_speed;
												position_pi.output_max = connect_crt.max_speed;
												position_pi.output_min = -connect_crt.max_speed;
										}
										
										if(PositionControl_CheckLimit(target_pos))
										{
												set_cantx_buf(0xff, 0xf1, 0, 0, 0, 0, 0, 0);
												CAN_Transmit(&my_can_tx);
										}
										else
										{
												PositionControl_StartRelative(&position_ctrl, delta);
												set_cantx_buf(0xdd, 0xc8, 0, 0, 0, 0, 0, 0);
												CAN_Transmit(&my_can_tx);
										}
										syn_flag = 0;
								}
								else
								{
										set_cantx_buf(0xff, 0xf2, 0, 0, 0, 0, 0, 0);
										CAN_Transmit(&my_can_tx);
								}
						}
						else if(syn_flag == 0xc1 || syn_flag == 0xc3)
						{
								if(connect_crt.motor_mode == 3)
								{
										float target_pos = syn_value[1];
										
										if(syn_flag == 0xc1)
										{
												connect_crt.max_current = syn_value[0];
												if(connect_crt.max_current > System_MAX_I) connect_crt.max_current = System_MAX_I;
												speed_pi.output_min = -connect_crt.max_current;
												speed_pi.output_max = connect_crt.max_current;
										}
										else
										{
												connect_crt.max_speed = syn_value[0];
												limit_max_min(&connect_crt.max_speed, 3000, -3000);
												position_ctrl.max_speed = connect_crt.max_speed;
												position_pi.output_max = connect_crt.max_speed;
												position_pi.output_min = -connect_crt.max_speed;
										}
										
										if(PositionControl_CheckLimit(target_pos))
										{
												set_cantx_buf(0xff, 0xf1, 0, 0, 0, 0, 0, 0);
												CAN_Transmit(&my_can_tx);
										}
										else
										{
												PositionControl_StartAbsolute(&position_ctrl, target_pos);
												set_cantx_buf(0xdd, 0xc9, 0, 0, 0, 0, 0, 0);
												CAN_Transmit(&my_can_tx);
										}
										syn_flag = 0;
								}
								else
								{
										set_cantx_buf(0xff, 0xf2, 0, 0, 0, 0, 0, 0);
										CAN_Transmit(&my_can_tx);
								}
						}			

          else if(syn_flag ==0xd0)
					{
													if(connect_crt .motor_mode ==4){
									//设置角度
									connect_crt .distance =syn_value [1];
								 //设置电流
                 connect_crt .max_current=syn_value [0];
								 if(connect_crt .max_current>System_MAX_I )connect_crt .max_current=System_MAX_I ;//限流								
								 AngleControl .pid_angle.output_max=connect_crt .max_current;	
								 AngleControl .pid_angle.output_min=-connect_crt .max_current;	
									
									AngleControl_SetTargetAngle(&AngleControl,connect_crt .distance );
									set_cantx_buf (0xdd,0xcd,0,0,0,0,0,0);//成功返回的代码
			            CAN_Transmit(&my_can_tx );
														syn_flag =0;
								}
							  else 	
							  {
								set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
								CAN_Transmit(&my_can_tx );
							  }
					
					}						

}


//////////////////Can总线通信
void can_task(void)
{
  if(my_can_rx .can_rx_flag ==1)//收到消息
	{
	   my_can_rx .can_rx_flag =0;
     uint8_t can_buf[8]={0};
     memcpy (can_buf ,my_can_rx .payload ,8);		 
		 if(can_buf [0]==0x11&&can_buf [1]==0x22)//电机校准
		 {
			 	connect_crt .motor_mode =0;//开环模式下校准
				set_uduq (&m1_foc ,0,0);
				foc_open  (&m1_foc ,0);
				HAL_Delay (50);
			 	if(check_mt6816 ())
				{
						 set_cantx_buf (0xdd,0xc0,0,0,0,0,0,0);//成功返回的代码
			       CAN_Transmit(&my_can_tx );

				    	HAL_Delay (10);
						 REIN_mt6816_spi_data_Signal_Init();
						// FOC_Init ();

				}
			 else 
			 {
		    set_cantx_buf (0xff,0xf0,0,0,0,0,0,0);//返回编码器不存在错误
			   CAN_Transmit(&my_can_tx );
			 }
		 }
		 else if(can_buf [0]==0x23)//设置模式
		 {
			 if(can_buf [1]==0x11&&can_buf [2]==0)//空闲模式
			 {
				  w_Uq=0;m1_foc .tar_Id =0;m1_foc .id_inter =0;//弱磁部分清零
				 				connect_crt .motor_mode =0;	 
								set_uduq (&m1_foc ,0,0);
						    foc_open  (&m1_foc ,0);
				 		    set_cantx_buf (0xdd,0xc1,0,0,0,0,0,0);//成功返回的代码
			          CAN_Transmit(&my_can_tx );
			 }
			 else if(can_buf [1]==0x22&&can_buf [2]==0)//电流模式
			 {
			 							  if(mt6816_flag )
								{
						     connect_crt .motor_mode =1;
									 w_Uq=0;m1_foc .tar_Id =0;m1_foc .id_inter =0;//弱磁部分清零
									connect_crt .drive_current=0;
								 set_foc_Iqcurrent(&m1_foc ,connect_crt .drive_current);//切换电流模式后无输出，需要设置
								 set_cantx_buf (0xdd,0xc2,0,0,0,0,0,0);//成功返回的代码
			           CAN_Transmit(&my_can_tx );
								}
								else {
									 set_cantx_buf (0xff,0xf0,0,0,0,0,0,0);//返回编码器不存在错误
			             CAN_Transmit(&my_can_tx );
								}
			 }
			 else if(can_buf [1]==0x33&&can_buf [2]==0&&connect_crt .motor_mode !=2)//速度模式
			 {
				 							  if(mt6816_flag)
								{
									connect_crt .speed=0;
										init_encoder_update();
									PID_Controller_Init(&speed_pi,0.015,0.00015,0.00,0,-connect_crt .max_current ,connect_crt .max_current);//设置速度									
						      connect_crt .motor_mode =2;
								set_cantx_buf (0xdd,0xc3,0,0,0,0,0,0);//成功返回的代码
			           CAN_Transmit(&my_can_tx );
								}
								else
								{											   
									set_cantx_buf (0xff,0xf0,0,0,0,0,0,0);//返回编码器不存在错误
			            CAN_Transmit(&my_can_tx );
								}
			 }
			 else if(can_buf [1]==0x44&&can_buf [2]==0&&connect_crt .motor_mode !=3)//位置模式
			 {
				 							  if(mt6816_flag)
								{
										init_encoder_update();

									
									position_pi.kp = 0.6 * PID_factor;
									position_pi.ki = 0.001 * PID_factor;
									position_pi.kd = 0.0;
									if(if_read_pos_start ==1)
									{
											angle_zero =	save_data[2]=(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x408));//读取单圈角度零点
											if(angle_zero >16834)angle_zero =0;
									}	
																		 // 位置环初始化
									PositionControl_Init(&position_ctrl, 
																			 connect_crt.max_speed, 
																			 connect_crt.accel, 
																			 connect_crt.decel,
																			 0.3f, 0.5f);
								///需要添加位置环初始化代码
									
									PID_Controller_Init(&speed_pi,0.015,0.00015,0.00,0,-connect_crt .max_current ,connect_crt .max_current);//设置速度				
									
								  connect_crt .motor_mode =3;
									set_cantx_buf (0xdd,0xc4,0,0,0,0,0,0);//成功返回的代码
			            CAN_Transmit(&my_can_tx );
								}
								else 
								{		    
									set_cantx_buf (0xff,0xf0,0,0,0,0,0,0);//返回编码器不存在错误
			             CAN_Transmit(&my_can_tx );
								}
			 }
			 else if(can_buf [1]==0x55&&can_buf [2]==0)//角度模式
			 {
				 					 if(mt6816_flag){		

										  PID_Controller_Init(&AngleControl.pid_angle,0.03,0.0000,1.2,0,-connect_crt .max_current ,connect_crt .max_current);
                   //PID_Controller_Init(&AngleControl.pid_angle,0.05,0.0002,0.8,0,-connect_crt .max_current ,connect_crt .max_current);		
									 angle_zero =	save_data[2]=(*(__IO uint32_t *)(FLASH_USER_START_ADDR +0x408));//读取角度环零点
									 if(angle_zero >16834)angle_zero =0;
									 AngleControl .zero_position=DIR*angle_zero  *0.02197265625f;//将存储数据设置为零点
									 // AngleControl_SetZero(&AngleControl);   
                   AngleControl .pid_angle.target	=DIR*(mt6816_count-angle_zero) *0.02197265625f;	//就以当前角度作为目标角度	
									 // AngleControl_SetZero(&AngleControl);                 
									 connect_crt .motor_mode =4;
									 set_cantx_buf (0xdd,0xc5,0,0,0,0,0,0);//成功返回的代码
			             CAN_Transmit(&my_can_tx );
								 }
								 else 		   
								 {									 
									 set_cantx_buf (0xff,0xf0,0,0,0,0,0,0);//返回编码器不存在错误
			             CAN_Transmit(&my_can_tx );
								 }
						 
			 }
		 }
		 else if(can_buf [0]==0x44)//设置电流
		 {	  
			 	 connect_crt .drive_current= 0.01* bytesToInt16 (can_buf [6],can_buf [7]);
         if(connect_crt .motor_mode ==1)
				{
					  if((llimit_flag ==1&&connect_crt .drive_current  <0)||(rlimit_flag ==1&&connect_crt .drive_current  >0))//限位
					 { 
							connect_crt .drive_current =0;
						 	set_cantx_buf (0xff,0xf1,0,0,0,0,0,0);//返回限位错误
			        CAN_Transmit(&my_can_tx );
					 }
		       ////////////处理同步指令
					if(can_buf [1]==0xAA)
					{
						syn_flag =0XA0;//标记为设置电流需同步
					  syn_value[0]=connect_crt .drive_current;//电流值
						syn_value [1]=0;//无其他值
						syn_value [2]=0;//无其他值
						set_cantx_buf (0xdd,0xd3,0,0,0,0,0,0);//设置同步成功返回的代码
			      CAN_Transmit(&my_can_tx );
						return ;//直接退出
					}
					 set_foc_Iqcurrent(&m1_foc ,connect_crt .drive_current);
				}
				
				connect_crt .max_current =fabs(connect_crt .drive_current);
					if(fabs (connect_crt .max_current)>System_MAX_I )connect_crt .max_current=System_MAX_I ;//限流
					AngleControl .pid_angle.output_max= speed_pi .output_max=connect_crt .max_current;
				  AngleControl .pid_angle.output_min= speed_pi .output_min =-connect_crt .max_current;	
					set_cantx_buf (0xdd,0xc6,0,0,0,0,0,0);//成功返回的代码
			    CAN_Transmit(&my_can_tx );
		 }
		 
		 	/////////////////////////////////设置速度环速度
     else if(can_buf [0]==0x33)
		 {				
							if(connect_crt .motor_mode ==2)
							{
								  ////////////处理同步指令
									if(can_buf [1]==0xAA)
									{
										syn_flag =0XB0;//标记为设置速度需同步
										syn_value[1]=bytesToInt16(can_buf [6],can_buf [7]);//储存速度
										limit_max_min( &syn_value[1],3000,-3000)	;//限制加速度		
										syn_value [0]=fabs(bytesToInt16(can_buf [4],can_buf [5])*0.01);//储存电流
										syn_value [2]=abs(bytesToInt16(can_buf [2],can_buf [3]));//储存加速度
										set_cantx_buf (0xdd,0xd3,0,0,0,0,0,0);//设置同步成功返回的代码
			              CAN_Transmit(&my_can_tx );
										return ;//直接退出
									}
							   connect_crt .speed =bytesToInt16(can_buf [6],can_buf [7]);//设置速度																 
								 connect_crt .max_current=fabs(bytesToInt16(can_buf [4],can_buf [5])*0.01);
								 if(connect_crt .max_current>System_MAX_I )connect_crt .max_current=System_MAX_I ;//限流
								 speed_pi .output_min=-connect_crt .max_current;
							   speed_pi .output_max =connect_crt .max_current;								
								//设置加速度
								 connect_crt .s_acc=abs(bytesToInt16(can_buf [2],can_buf [3]));
									limit_max_min( &connect_crt .s_acc,10000,1)	;//限制加速度		
									
                 if((llimit_flag ==1&&connect_crt .speed <0)||(rlimit_flag ==1&&connect_crt .speed >0))//限位
								 { 
									 connect_crt .speed=0;
									 set_cantx_buf (0xff,0xf1,0,0,0,0,0,0);//返回限位错误
			             CAN_Transmit(&my_can_tx );
								 }
								 else 	
								{			
									 set_cantx_buf (0xdd,0xc7,0,0,0,0,0,0);//成功返回的代码
			             CAN_Transmit(&my_can_tx );
								 }
								 
								  if(connect_crt .speed >3000)connect_crt .speed =3000;
                  else if(connect_crt .speed <-3000)connect_crt .speed =-3000;						 								
							}
							else {					
								set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
			          CAN_Transmit(&my_can_tx );
							}
	   }						
			
			//////////////////////////////////位置环设置
      else if(can_buf [0]==0x5a&&can_buf [1]==0)//设置电机位置环速度参数
						{				
							if(connect_crt .motor_mode ==3){
								 
								 //设置最大速度							
							   connect_crt .max_speed  =abs(bytesToInt16(can_buf [2],can_buf [3]));
								 //设置加速加速度
								 connect_crt .accel =abs(bytesToInt16(can_buf [4],can_buf [5]));
								//设置减速加速度大小
								 connect_crt .decel =abs(bytesToInt16(can_buf [6],can_buf [7]));
								limit_max_min(&connect_crt .accel,200000,1);
								limit_max_min(&connect_crt .decel,10000,1);
								limit_max_min(&connect_crt .max_speed,3000,1);
								
									///需要添加位置环参数修改代码
								        // 更新位置环参数
									position_ctrl.max_speed = connect_crt.max_speed;
									position_ctrl.accel = connect_crt.accel;
									position_ctrl.decel = connect_crt.decel;
									position_pi.output_max = connect_crt.max_speed;
									position_pi.output_min = -connect_crt.max_speed;
							  	set_cantx_buf (0xdd,0xcA,0,0,0,0,0,0);//成功返回的代码
			           CAN_Transmit(&my_can_tx );
							}
							else 	
							{
								set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
								CAN_Transmit(&my_can_tx );
							}					
						}
      else if(can_buf [0]==0x55||can_buf [0]==0x56)	//位置环相对运动角度设置，位置-电流
						{
							if(connect_crt .motor_mode ==3){
								 uint8_t byte[4];
								 //设置相对位置
			           setbyte4 (byte ,can_buf [4],can_buf [5],can_buf [6],can_buf [7]);							
									float delta = 0.01 * bytesToInt32(byte);
									float current_pos = PositionControl_GetCurrentPosition();
									float target_pos = current_pos + delta;
								 if(can_buf [0]==0x55)
								 {
								 //设置电流
								 connect_crt .max_current=fabs(bytesToInt16(can_buf [2],can_buf [3])*0.01);
								 if(connect_crt .max_current>System_MAX_I )connect_crt .max_current=System_MAX_I ;//限流
								 speed_pi .output_min=-connect_crt .max_current;
								 speed_pi .output_max =connect_crt .max_current;
								 }
								 else
								 {
								 	//设置最大速度
								 connect_crt .max_speed=abs(bytesToInt16(can_buf [2],can_buf [3]));
								 limit_max_min (&connect_crt .max_speed,3000,-3000);
									  position_ctrl.max_speed = connect_crt.max_speed;
										position_pi.output_max = connect_crt.max_speed;
										position_pi.output_min = -connect_crt.max_speed;
									///需要添加位置环最大速度修改代码
								 
								 }
								  ////////////处理同步指令
									if(can_buf [1]==0xAA)
									{
										
                    if(can_buf [0]==0x55)	
										{											
										 syn_value [0]= connect_crt .max_current;//储存电流
											syn_flag =0XC0;//标记为设置相对位置位置需同步	
										}
										else
										{
										syn_value [0]= connect_crt .max_speed ;//储存速度
											syn_flag =0XC2;//标记为设置相对位置位置需同步	
										}
										syn_value[1] = delta;;//储存相对位置
										syn_value [2]=0;//无储存
										set_cantx_buf (0xdd,0xd3,0,0,0,0,0,0);//设置同步成功返回的代码
			              CAN_Transmit(&my_can_tx );
										return ;//直接退出
									}
										if(PositionControl_CheckLimit(target_pos))
										{
												set_cantx_buf(0xff, 0xf1, 0, 0, 0, 0, 0, 0);
												CAN_Transmit(&my_can_tx);
										}
								else
									{

											             PositionControl_StartRelative(&position_ctrl, delta);
            set_cantx_buf(0xdd, 0xc8, 0, 0, 0, 0, 0, 0);
            CAN_Transmit(&my_can_tx);
												///需要添加位置环相对运动代码
									}
						  }
							else 	
							{
								set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
								CAN_Transmit(&my_can_tx );
							}			
					  }

				// CAN位置环绝对运动
				else if(can_buf[0] == 0x57 || can_buf[0] == 0x58)
				{
						if(connect_crt.motor_mode == 3)
						{
								uint8_t byte[4];
								setbyte4(byte, can_buf[4], can_buf[5], can_buf[6], can_buf[7]);
								float target_pos = 0.01 * bytesToInt32(byte);
								
								if(can_buf[0] == 0x57)
								{
										connect_crt.max_current = fabs(bytesToInt16(can_buf[2], can_buf[3]) * 0.01);
										if(connect_crt.max_current > System_MAX_I) connect_crt.max_current = System_MAX_I;
										speed_pi.output_min = -connect_crt.max_current;
										speed_pi.output_max = connect_crt.max_current;
								}
								else
								{
										connect_crt.max_speed = abs(bytesToInt16(can_buf[2], can_buf[3]));
										limit_max_min(&connect_crt.max_speed, 3000, -3000);
										position_ctrl.max_speed = connect_crt.max_speed;
										position_pi.output_max = connect_crt.max_speed;
										position_pi.output_min = -connect_crt.max_speed;
								}
								
								if(can_buf[1] == 0xAA)
								{
										if(can_buf[0] == 0x57)
										{
												syn_value[0] = connect_crt.max_current;
												syn_flag = 0XC1;
										}
										else
										{
												syn_value[0] = connect_crt.max_speed;
												syn_flag = 0XC3;
										}
										syn_value[1] = target_pos;
										set_cantx_buf(0xdd, 0xd3, 0, 0, 0, 0, 0, 0);
										CAN_Transmit(&my_can_tx);
										return;
								}
								
								if(PositionControl_CheckLimit(target_pos))
								{
										set_cantx_buf(0xff, 0xf1, 0, 0, 0, 0, 0, 0);
										CAN_Transmit(&my_can_tx);
								}
								else
								{
										PositionControl_StartAbsolute(&position_ctrl, target_pos);
										set_cantx_buf(0xdd, 0xc9, 0, 0, 0, 0, 0, 0);
										CAN_Transmit(&my_can_tx);
								}
						}
						else
						{
								set_cantx_buf(0xff, 0xf2, 0, 0, 0, 0, 0, 0);
								CAN_Transmit(&my_can_tx);
						}
				}
				else if(can_buf [0]==0x5c&&can_buf [1]==0x11)//设置当前位置为位置环零点
						{
							
								if(connect_crt .motor_mode ==3){
									
									if_read_pos_start=1;//重新启用ROM数据作为0点			
									save_data [2]=angle_zero=mt6816_count ;//保持单圈0点

									///需要添加位置环设置零点代码
									PositionControl_SetZero(&position_ctrl);
						      Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data );//写入存储		
								  set_cantx_buf (0xdd,0xcB,0,0,0,0,0,0);//成功返回的代码
			            CAN_Transmit(&my_can_tx );
								}
								else 	
							  {
								set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
								CAN_Transmit(&my_can_tx );
							  }
						}
			 else if(can_buf [0]==0x5f&&can_buf [1]==0x11)//复位找零点
						{
							
								if(connect_crt .motor_mode ==3){
									if(llimit_flag ==0)//没有在反转限位时才能复位
									{
										///需要添加位置环复位代码
											reset_flag =1;
											PositionControl_Reset(&position_ctrl);
											set_cantx_buf (0xdd,0xcc,0,0,0,0,0,0);//成功返回的代码
											CAN_Transmit(&my_can_tx );
									}
									else
									{
														                 // 复位完成
                if_read_pos_start = 0;
                angle_zero = mt6816_count;
                position_ctrl.current_position = 0;
                position_ctrl.target_position = 0;
                position_ctrl.start_position = 0;
                position_ctrl.position_error = 0;
                position_ctrl.is_running = 0;
                pos_finish_flag = 1;
                reset_flag = 0;
											///需要添加位置环已经复位代码
											 reset_flag =0; 
											set_cantx_buf (0xFF,0xF1,0x00,0,0,0,0,0);//成功返回存在限位的代码
											CAN_Transmit(&my_can_tx );		
									}
								}
							 else 	
							  {
											set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
											CAN_Transmit(&my_can_tx );
							  }
						
						}
						
						///////////////////////////////////////角度环
						else if(can_buf [0]==0x66)//设置角度
						{
							
								if(connect_crt .motor_mode ==4){
									//设置角度
									connect_crt .distance =0.01*bytesToInt16 (can_buf [6],can_buf [7] );
								 //设置电流
                 connect_crt .max_current=fabs(bytesToInt16(can_buf [4],can_buf [5])*0.01);
								 if(connect_crt .max_current>System_MAX_I )connect_crt .max_current=System_MAX_I ;//限流								
								 AngleControl .pid_angle.output_max=connect_crt .max_current;	
								 AngleControl .pid_angle.output_min=-connect_crt .max_current;	
									
									if(can_buf [1]==0xAA)
									{
										syn_flag =0XD0;//标记为设置角度需同步										
										syn_value [0]= connect_crt .max_current;//储存电流
										syn_value[1]= connect_crt .distance;//储存角度
										syn_value [2]=0;//无储存
										set_cantx_buf (0xdd,0xd3,0,0,0,0,0,0);//设置同步成功返回的代码
			              CAN_Transmit(&my_can_tx );
										return ;//直接退出
									}
									
									AngleControl_SetTargetAngle(&AngleControl,connect_crt .distance );
									set_cantx_buf (0xdd,0xcd,0,0,0,0,0,0);//成功返回的代码
			            CAN_Transmit(&my_can_tx );
								}
							  else 	
							  {
								set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
								CAN_Transmit(&my_can_tx );
							  }
						
						}
						
						else if(can_buf [0]==0x69&&can_buf [1]==0x11)//设置角度环零点
						{
								if(connect_crt .motor_mode ==4){
									AngleControl_SetZero(&AngleControl);

									save_data [2]=angle_zero=mt6816_count ;

						      Flash_HAL_Write_Data(FLASH_USER_START_ADDR+0X400,save_data );//调节因子写入存储
									set_cantx_buf (0xdd,0xcf,0,0,0,0,0,0);//成功返回的代码
			            CAN_Transmit(&my_can_tx );
								}
								else 	
							  {
								set_cantx_buf (0xff,0xf2,0,0,0,0,0,0);//返回模式不匹配错误代码
								CAN_Transmit(&my_can_tx );
							  }
						}
						
						
					 else if(can_buf [0]==0x77&&can_buf [1]==0x78)
						 {
							 stop_motor (&connect_crt );
							 set_cantx_buf (0xdd,0xd0,0,0,0,0,0,0);//成功返回的代码
			         CAN_Transmit(&my_can_tx );
							 HAL_Delay (10);
	
											 
						 }
					 else if(can_buf [0]==0x88&&can_buf [1]==0x89&&can_buf [2]==0x8a)//重启
					 {
						 	 set_cantx_buf (0xdd,0xd1,0,0,0,0,0,0);//成功返回的代码
			         CAN_Transmit(&my_can_tx );
						   HAL_Delay (2);
													//复位前注意释放关键硬件资源
								 if(connect_type ==0)//失能CAN通信
						 HAL_CAN_MspDeInit (&hcan);
						 else 
						 {
							HAL_PCD_DeInit(&hpcd_USB_FS);//失能USB通信
							CAN_Init();//初始化总线
						 }
               HAL_Delay (20);//延时片刻
					    __disable_irq();          // 关闭所有中断
              HAL_NVIC_SystemReset();   // 执行复位
					 }
					 	else if(can_buf [0]==0x99&&can_buf [1]==0x9A)//进行同步指令
					 {
							syn_task();
					 }
					 
					 else if(can_buf [0]==0xc0&&can_buf [1]==0xa0)//获取速度
					 {
						 memset(my_can_tx .payload,0,8);
              int32_t ss=100*speed_send;
						 intTo7Bytes( ss ,my_can_tx .payload );
						 my_can_tx .payload[0]=0xa0;
             CAN_Transmit(&my_can_tx );
					 }
					 else if(can_buf [0]==0xc3&&can_buf [1]==0xa3)//获取位置
					 {
						 memset(my_can_tx .payload,0,8);
						 my_can_tx .payload[0]=0xa3;
						 int32_t ss = 100 * PositionControl_GetCurrentPosition();
						 intTo7Bytes( ss,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
					 }
					 else if(can_buf [0]==0xc4&&can_buf [1]==0xa4)//获取角度
					 {
						 memset(my_can_tx .payload,0,8);
						 my_can_tx .payload[0]=0xa4;
						 int32_t ss=100*(normalize_angle_to_180(DIR * (mt6816_count - angle_zero) * 0.02197265625f));
						 intTo7Bytes( ss,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
					 }
					 else if(can_buf [0]==0xc5&&can_buf [1]==0xa5)//获取温度
					 {
						 memset(my_can_tx .payload,0,8);
						 	my_can_tx .payload[0]=0xa5;
						 int32_t ss=100*temperature;
						 intTo7Bytes( ss ,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
					 }
					 else if(can_buf [0]==0xc7&&can_buf [1]==0xa7)//获取电压
					 {
						 memset(my_can_tx .payload,0,8);
						 int32_t ss=100*vol;
						 my_can_tx .payload[0]=0xa7;
						 intTo7Bytes(ss  ,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
					 }
					 else if(can_buf [0]==0xc8&&can_buf [1]==0xa8)//获取编码器计数值
					 {
						 memset(my_can_tx .payload,0,8);
						 int32_t ss=mt6816_count ;
						 my_can_tx .payload[0]=0xa8;
						 intTo7Bytes(ss  ,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
					 }
					 
					 else if(can_buf [0]==0xc9&&can_buf [1]==0xa9)//获取当前工作模式
					 {
						 memset(my_can_tx .payload,0,8);
						 my_can_tx .payload [0]=0xa9;
						 switch(connect_crt .motor_mode )
						 {
							 case 0:my_can_tx .payload [7]=0;break ;
							 case 1:my_can_tx .payload [7]=0x11;break ;
							 case 2:my_can_tx .payload [7]=0x22;break ;
							 case 3:my_can_tx .payload [7]=0x33;break ;
							 case 4:my_can_tx .payload [7]=0x44;break ;
							 default :my_can_tx .payload [7]=0xFF;break ;
						 }
             CAN_Transmit(&my_can_tx );
					 }
					  else if(can_buf [0]==0xcb&&can_buf [1]==0xab)//获取扭矩
					 {
						 memset(my_can_tx .payload,0,8);
						 int32_t ss=m1_foc.Iq*100 ;
						 my_can_tx .payload[0]=0xab;
						 intTo7Bytes(ss  ,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
					 }
					else if(can_buf [0]==0xca&&can_buf [1]==0xaa)//获取速度位置角度编码器值
					 {
						 return_flag_can=1;
						 uint8_t return_HZ;
						 return_HZ =can_buf [7];//获取回传频率
						 if(return_HZ >200)return_HZ =200;
						 if(return_HZ<1)return_HZ =1;
						 return_delay=5000/return_HZ ;
						 

					 }
					 else if(can_buf [0]==0xca&&can_buf [1]==0xaf)
					 {
					 return_flag_can =0;
						 			set_cantx_buf (0xdd,0xd2,0,0,0,0,0,0);//成功返回的代码
			            CAN_Transmit(&my_can_tx );
					 
					 }
					 return ;
		}
	
    if(time_num %return_delay ==0&&return_flag_can ==1)
		{
								 memset(my_can_tx .payload,0,8);
              int32_t ss=100*speed_send;
						 intTo7Bytes( ss ,my_can_tx .payload );
						 my_can_tx .payload[0]=0xa0;
             CAN_Transmit(&my_can_tx );
						SysTick_Delay_us (100);
						 memset(my_can_tx .payload,0,8);
						 my_can_tx .payload[0]=0xa3;
						  ss=100*PositionControl_GetCurrentPosition();///需要添加位置环读取代码
						 intTo7Bytes( ss,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
							SysTick_Delay_us (100);;
						 memset(my_can_tx .payload,0,8);
						 my_can_tx .payload[0]=0xa4;
						  ss=100*(normalize_angle_to_180(DIR * (mt6816_count - angle_zero) * 0.02197265625f));
						 intTo7Bytes( ss,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
						 memset(my_can_tx .payload,0,8);
						  ss=mt6816_count ;
						 my_can_tx .payload[0]=0xa8;
						 intTo7Bytes(ss  ,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );	
						 SysTick_Delay_us (100);;
						 memset(my_can_tx .payload,0,8);
						 my_can_tx .payload[0]=0xab;
						  ss=100*(m1_foc .Iq);
						 intTo7Bytes( ss,my_can_tx .payload );
             CAN_Transmit(&my_can_tx );
						 
		}


}










