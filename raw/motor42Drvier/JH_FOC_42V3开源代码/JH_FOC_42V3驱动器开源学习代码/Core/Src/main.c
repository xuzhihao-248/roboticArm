/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
	
	
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
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "can.h"
#include "iwdg.h"
#include "spi.h"
#include "tim.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "float_char.h"
#include "adc_task.h"
#include "B_motor_foc.h"
#include "mos_set.h"
#include "mt6816.h"
#include "PID.h"
#include "connecting.h"
#include "filter.h"
#include "myflash.h"
#include "usbd_cdc_if.h"
#include "math.h"
#include "position_foc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
uint32_t add_angle;//角度累加，初始化用
float current[2],vol=0,temperature=0 ;//定义ADC物理量
uint8_t Iadjust=1;//电流校准标记
uint8_t cchar[8][4]={0};//定义浮点数转换后的数据缓存
uint8_t tail[4]={0x00, 0x00, 0x80, 0x7f};
extern int16_t  Scope[200];
extern uint16_t angledata[200];
extern uint16_t my_can_id;//CAN总线ID
extern float speed_send;
uint8_t yy=0;
uint8_t erro_flag=0;//发生错误标记
uint8_t limit_vol=0;//积分限幅电压
extern uint32_t save_data[20];

//////////////////////////////
uint8_t connect_type=0;//定义通信类型0：USB，1:CAN
float last_vol=10;//电压滤波


//////////////弱磁部分
float w_Uq=0;//Uq轴弱磁输出量
PID_Controller_t weak_mag_id; //id弱磁pid
PID_Controller_t weak_mag_iq;//iq弱磁pid
PID_Controller_t weak_mag_uq;//uq弱磁pid

uint16_t weak1_speed_save=400;  //保存的电机弱磁1临界速度
uint16_t weak2_speed_save=1000;  //保存的电机弱磁2临界速度
uint16_t  weak_speed_1 =400 ;         /////最终第一步弱磁临界速度
uint16_t weak_speed_2 =1000;         /////最终第二步弱磁临界速度
//读取错误时弱磁默认值
#define default_weak1 400
#define default_weak2 920

#define weak_Iq_min 0.2         //二阶段弱磁最小Iq电流，这个值和高速电流采样的精度相关，理论上越小越好，可以增大极限转速！如果该值太小，采样电流不准确则大概率会超速失调！
float spid_factor=0,s_kp0=0,s_ki0=0,s_kp=0,s_ki=0,s_kp1=0,s_ki1=0;//速度环PID调节因子，根据驱动电压适当调节
uint8_t t_speed=30;//低速PID参数转变速度
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
uint16_t time_num=1;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
////////定义保护函数
float check_iq=0,check_id=0;//过流检测变量，滤波存储用
void motor_protect(void)
{
  if(temperature >80||fabs(check_iq) >4||fabs(check_id)>4||(vol>40||vol<4))
	{
		//存在保护，关闭电机运行
		 connect_crt .motor_mode =0;	 
		 set_uduq (&m1_foc ,0,0);
		 foc_open  (&m1_foc ,0);
		 weak_mag_id .integral =0;weak_mag_iq.integral  =0;weak_mag_uq .integral =0;
		 w_Uq =0;speed_pi.actual_value=0;
		 m1_foc .tar_Id=0;m1_foc .tar_Iq=0;m1_foc .iq_inter=m1_foc .id_inter=0;
	//	stop_motor (&connect_crt );
		
//								uint8_t data[64] = {0};  // 适当增大缓冲区
//						char s[10] = {0};        // 增大缓冲区确保足够空间
//						char p[18] = {0};
//						char a[10] = {0};
//						char e[10] = {0};
//						char q[10] = {0};//返回Iq
//						int length = 0;

//						// 格式化字符串，保留三位小数
//						snprintf(s, sizeof(s), "%.3f", vol);
//						snprintf(p, sizeof(p), "%.3f", m1_foc .Iq);
//						snprintf(a, sizeof(a), "%.3f", m1_foc .Id);
//						snprintf(e, sizeof(e), "%.2f", temperature );
//						
//						// 构建完整字符串
//						length = snprintf((char*)data, sizeof(data), 
//							"v:%sid:%siq:%se:%st:%s\n", 
//														 s, p, a, e,q);
//														CDC_Transmit_FS(data, length);  // 已经包含换行符

		if(erro_flag ==0)
		{
			if(connect_type ==0)
  	  CDC_Transmit_FS ("Protecting!\n",13);
			else
			{
			set_cantx_buf (0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff);//返回模式不匹配错误代码
			CAN_Transmit(&my_can_tx );
			}
		}
		erro_flag =1;
	}
	else if(mt6816_flag ==1)
		erro_flag =0;
}

//定义上电通信类型检测函数
void get_connect_type(void)
{
  uint8_t num=0;
	for(uint8_t i=0;i<200;i++)
	if(HAL_GPIO_ReadPin (GPIOA ,GPIO_PIN_1 )==1)
	{
		num++;
		HAL_Delay (1);
	}
	if(num>190)connect_type =0;
	else connect_type =1;

}
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern PCD_HandleTypeDef hpcd_USB_FS;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
	//弱磁PID初始化
	 PID_Controller_Init(&weak_mag_id ,0.01,0.000005,0,0,-1.2,1.2);
	 PID_Controller_Init(&weak_mag_iq ,0.01,0.00001,0,0,-0,0.0);
	 PID_Controller_Init(&weak_mag_uq ,0.004,0.00002,0,0,-4,4);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_IWDG_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
 // MX_USB_DEVICE_Init();
  MX_ADC2_Init();
  MX_TIM3_Init();
  //MX_CAN_Init();
  /* USER CODE BEGIN 2 */
	  Flash_HAL_Read_N_32Data(0x0800FC00,(uint32_t *)save_data,20);//先读取保存的数据
		  my_can_id =*(volatile uint16_t*)0x0800FC04;//获取CANID
	  if(my_can_id >0x7ff)my_can_id =0x200;
		  //获取CAN时钟分频系数
			can_Prescaler =(*(__IO uint8_t *)(FLASH_USER_START_ADDR +0x44C));;
			if(can_Prescaler !=60&&can_Prescaler !=24&&can_Prescaler !=12&&can_Prescaler !=6&&can_Prescaler !=3)//范围不对，默认分频6，500kbps
				can_Prescaler =6;
			
			HAL_GPIO_WritePin (GPIOB ,GPIO_PIN_12,GPIO_PIN_SET );
			
			get_connect_type();
		//注释掉上述自动生产的初始化
	 if(connect_type ==0)//失能CAN通信
	 {
		 		 MX_CAN_Init();
	   HAL_CAN_MspDeInit (&hcan);
		 MX_USB_DEVICE_Init();
	 }
	 else 
	 {
		CAN_Init();//初始化总线
	 }
    HAL_Delay (1000);//较大延时等待电脑识别USB
   HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
	 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
	 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
	 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	 HN1_set (900);HN2_set (900);HN3_set (900); HN4_set (900);
	 HAL_TIM_Base_Start_IT(&htim2);             //启动时钟
	 
	 	HAL_ADCEx_Calibration_Start(&hadc1);    //ADC校准
	  HAL_ADCEx_Calibration_Start(&hadc2);    //ADC校准
		HAL_ADCEx_InjectedStart_IT(&hadc1);                       //启动ADC转换
	  HAL_Delay (10);
   float offettotal[2]={0};
	 		for(uint8_t k=0;k<100;k++)
		 {
			 HAL_IWDG_Refresh (&hiwdg );
			   offettotal [0]+=current [0];
				 offettotal [1]+=current [1];
			  vol+=get_vol ();
				 HAL_Delay (5);
			}
		 vol=vol*0.01;
			//若有驱动器最大电流问题，限制一下输出电压,按需开启
//			if(vol >16)
//			limit_vol =120/vol+0.5;
//			else limit_vol =8;
			//读取保存的弱磁临界速度
			weak1_speed_save =*(volatile uint16_t*)(FLASH_USER_START_ADDR +0x424);
			weak2_speed_save =*(volatile uint16_t*)(FLASH_USER_START_ADDR +0x428);
			if(weak1_speed_save >4000)weak1_speed_save =default_weak1;if(weak2_speed_save >8000)weak2_speed_save =default_weak2;
			
			spid_factor =12/vol;if(spid_factor >1)spid_factor =1;if(spid_factor <0.4)spid_factor =0.4;//根据输入电压调整一下速度环pid参数
			s_kp=spid_factor *0.008;s_ki=spid_factor *0.00004;
			s_kp0 =spid_factor*0.02;s_ki0=spid_factor*0.001; 
			s_kp1 =spid_factor*0.04;s_ki1=spid_factor*0.0008; 
			weak_speed_1 =weak1_speed_save +(vol-12)*10;weak_speed_2 =weak2_speed_save +(vol-12)*15;//根据电压简单改变弱磁转变速度
			//if(weak_speed_1>500)weak_speed_1 =500;//限制，防止跑飞 
			t_speed =vol/12*30;
			
    offet_I [0]=0.01*offettotal [0];offet_I [1]=0.01*offettotal [1] ;
	  Iadjust =0;//完成校准
			
			
	 mt6816_flag =check_mt6816();//检查是否存在编码器
	 if(mt6816_flag ==0||(vol<5||vol>25))
		 erro_flag =1;
	 
	 b_foc_init ();		
	 init_connect_crt(&connect_crt );
	  HAL_TIM_Base_Start_IT(&htim3);  
	 
	 start_reset_waiting();//巡查是否需要复位，等待执行
	 
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		
		HAL_IWDG_Refresh (&hiwdg );
		if(time_num %100==0)
		{
			vol=low_pass_filter(get_vol (),last_vol ,0.1);
			check_iq =low_pass_filter(m1_foc .Iq ,check_iq ,0.5);
			check_id =low_pass_filter(m1_foc .Id ,check_id ,0.5);
			last_vol =vol;
			temperature =get_temp ();
			motor_protect ();//检测各种参数保护
			weak_mag_id .output_max =speed_pi .output_max;weak_mag_id .output_min =speed_pi .output_min ;//循环限制弱磁Id的范围,防止大于iq，第一阶段弱磁反转
		}
		if(connect_type ==0)//采用USB通信模式		
		usb_task ();	
		else 
		can_task ();

    speed_send =low_pass_filter (speed_pi .actual_value ,speed_send ,0.1);
		//yy++;if(yy>=200)yy=0;
			//	HAL_Delay (1);
//		    float_to_char(m1_foc .Id         ,cchar [0]);
//			  float_to_char(m1_foc .Iq      ,cchar [1]);
//				float_to_char(m1_foc .tar_Id        ,cchar [2]);
//				float_to_char(m1_foc .tar_Iq       ,cchar [3]);
//				float_to_char(m1_foc .Ud    ,cchar [4]);
//				float_to_char(m1_foc .Uq           ,cchar [5]);
//				float_to_char(m1_foc .Ibeta        ,cchar [6]);
//		    float_to_char(m1_foc .Ialpha         ,cchar [7]);
//				copy_to_vofa (cchar [0],0);
//		    copy_to_vofa (cchar [1],4);
//		    copy_to_vofa (cchar [2],8);
//		    copy_to_vofa (cchar [3],12);
//				    copy_to_vofa (cchar [4],16);
//				    copy_to_vofa (cchar [5],20);
//		    copy_to_vofa (cchar [6] ,24);
//			    copy_to_vofa (cchar [7] ,28);
//				    copy_to_vofa (tail ,32);
//		    CDC_Transmit_FS(vofa_char ,36);
		time_num ++;
    if(time_num >10000){time_num =1; }
//		if(time_num %5000==0)
//		{ if(m1_foc .tar_Id !=1)
//			m1_foc .tar_Id =1;
//			else 			m1_foc .tar_Id =-1;
//		}
		//if(time_num %5000==0)HAL_GPIO_TogglePin (GPIOB ,GPIO_PIN_12);
     if(erro_flag ==0)
			 HAL_GPIO_WritePin (GPIOB ,GPIO_PIN_12,GPIO_PIN_RESET );
		 else 
			 HAL_GPIO_WritePin (GPIOB ,GPIO_PIN_12,GPIO_PIN_SET );
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

uint8_t pos_update_count=0;//浣嶇疆鏇存柊璁℃暟
uint16_t scount=0;//到达目标后计数
uint8_t acc_flag=0;//速度无法达到目标标记
uint16_t b_step=0;//
uint8_t count_t=0;
uint32_t timer_value=0;
extern float uq_max,uq_min;
float uref=0;
uint32_t t_count=0;//定时器中断计数

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim ==&htim2 )//用来触发ADC电流采集，多此一举，因为TIM2设置事件更新时无法触发ADC自动注入采集，不知道为什么
	{ 
		 timer_value = TIM2->CNT;
      
		 if(timer_value<100 )//在中断高电平启动转换，这里由于不是PWM中断触发采样，实际实测触发频率约10KHZ左右
		 {
	     HAL_ADCEx_InjectedStart_IT(&hadc1);  		//启动ADC转换		
		 }
	}
	else if(htim ==&htim3 )//2KHZ
	{

		       check_limit();//限位检查
		       get_zero_by_current();
			////		/////速度环
					 if(connect_crt .motor_mode ==3||connect_crt .motor_mode ==2){//速度环位置环都需要执行
									// 判断是否达到要设定速度
						      if(connect_crt .motor_mode ==2&&speed_pi .target!=connect_crt .speed){
										  if(speed_pi .target >connect_crt .speed )
											 {
											  speed_pi .target -=connect_crt .s_acc  *0.0005;
												if(speed_pi .target <connect_crt .speed )speed_pi .target =connect_crt .speed ;
											 }
											else if(speed_pi .target <connect_crt .speed )
											{
												speed_pi .target +=connect_crt .s_acc  *0.0005;
												if(speed_pi .target >connect_crt .speed )speed_pi .target =connect_crt .speed ;
											}				
											acc_flag =0;scount =0;
									}
									else if(scount <20000)//延迟10s
									{
									  scount ++;
									}
									else if(acc_flag ==0&&((connect_crt .speed>0&&speed_pi .target >speed_pi .actual_value+200)||(connect_crt .speed<0&&speed_pi .target <speed_pi .actual_value -200)))
									{
											speed_pi .target =speed_pi .actual_value+ ((speed_pi .actual_value>=0) ? 100:-100);
											acc_flag =1;
											connect_crt .speed =speed_pi .target;
				 					}
									else acc_flag =1;
									
									float speed_error = speed_pi.target  - speed_pi.actual_value;
									if(speed_error >200)speed_error =200;else if(speed_error <-200)speed_error =-200;//限制误差，防止突变失控

									////////////两步弱磁模型，分段调控，不同电机需要调参才能更丝滑，调试时如果失控请关闭电源或者直接切换到空闲模式。
									////////////该模型存在缺点，因为是分段控制，加减速或者负载较大时可能速度有不连续性或者震荡，作者能力有限，欢迎各位大佬修正或者分享更好的方案
									float omg=fabs (speed_pi .target);if(omg<t_speed){if(connect_crt .motor_mode!=3){speed_pi.kp=s_kp0 ;speed_pi .ki =s_ki0 ;}else{speed_pi.kp=s_kp1 ;speed_pi .ki =s_ki1;}}else{speed_pi.kp=s_kp ;speed_pi .ki =s_ki;}

										if(omg<weak_speed_1||((speed_pi .actual_value <-1000&&speed_pi .target >=0)||(speed_pi .actual_value >1000&&speed_pi .target <=0)) )//不启用弱磁
										{
											w_Uq*=0.1;
											if(fabs (speed_pi .actual_value )<weak_speed_1+100)
											{
												m1_foc .tar_Id*=0.999;
										    m1_foc .tar_Iq = PID_Controller_Update(&speed_pi, speed_error);
											}
											else 
											{
												if(m1_foc .tar_Id<1.8)
												m1_foc .tar_Id+=0.01;
												else
												m1_foc .tar_Id=1.8;//增大Id恢复Ud值
												//兜底，速度从第二阶段直接切换到反方向目标值时增加一个突变，加速度非常大时存在这种情况防止跑飞
												if(speed_pi .actual_value <-600&&speed_pi .target >=0){ m1_foc .Iq=0.8;w_Uq =-10;}
												else if(speed_pi .actual_value >600&&speed_pi .target <=0){m1_foc .Iq=-0.8;w_Uq =10;}
											}
										}
										else if(omg<=weak_speed_2)//第一步弱磁,最大Iq,调节Id间接减小Ud
										{
											w_Uq*=0.1;weak_mag_uq.integral=0;
											
											 if(speed_pi .target >=0)
											 { 
												 if(m1_foc .tar_Iq<connect_crt .max_current)
											   m1_foc .tar_Iq+=0.01;
												 else m1_foc .tar_Iq=connect_crt .max_current;
											 }
											 else
											 {
												 if(m1_foc .tar_Iq>-connect_crt .max_current)
											   m1_foc .tar_Iq-=0.01;
												 else m1_foc .tar_Iq=-connect_crt .max_current;
											 }
												if(speed_pi .target >0)weak_mag_id .erro =-speed_error ;//识别反馈方向
												else weak_mag_id .erro =speed_error ;
												m1_foc .tar_Id   =PID_Controller_Update(&weak_mag_id ,weak_mag_id .erro );
										}
										else //第二步弱磁，减小Iq，调节Uq
										{	
                      if(m1_foc .tar_Id >-2)		//直接设置强弱磁电流									
											m1_foc .tar_Id -=0.01;
											else
											m1_foc .tar_Id=-2;
											weak_mag_iq .erro =-speed_error ;//反馈方向
											if(speed_pi .target >0){weak_mag_iq .output_max =2;weak_mag_iq .output_min=weak_Iq_min;if(w_Uq >-4)w_Uq -=0.5;else w_Uq =-4;}
											else if(speed_pi .target <0){weak_mag_iq .output_max =-weak_Iq_min;weak_mag_iq .output_min=-2;if(w_Uq <4)w_Uq +=0.5;else w_Uq =4;}
											m1_foc .tar_Iq =PID_Controller_Update(&weak_mag_iq ,weak_mag_iq.erro );	
											//w_Uq =PID_Controller_Update(&weak_mag_uq ,weak_mag_iq.erro );	//增加直接调节Uq的PID弱磁
											
										}

															


					////////////位置环1ms/1000Hz
            if(connect_crt .motor_mode ==3){
						pos_update_count++;
						if(pos_update_count >=2)
						{
							pos_update_count=0;
							PositionControl_Update(&position_ctrl);
						}
					}
								 
				}
					 
				else if(connect_crt .motor_mode ==4){
					
					//角度换
			   	AngleControl_Update(&AngleControl);//角度换控制
					m1_foc .tar_Iq =AngleControl.Iq_ref-0.001*speed_pi .actual_value ;//添加速度阻尼
				}
									
	}
}
uint8_t jj=0;
extern uint8_t encoder_adjust_flag;
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc)//ADC中断回调，运行电流环
{
	if(hadc == &hadc1 ){
		if(encoder_adjust_flag )
		mt6816_count =REIN_MT6816_GetAngleData();
	 if(connect_crt .motor_mode !=0&&Iadjust==0){

		// HAL_GPIO_WritePin (GPIOB,GPIO_PIN_12,GPIO_PIN_SET );	 
		 jj++;
		 if(jj>=5)
		 {
		  Encoder_Update();//编码器累计计数
		 //得到实际速度
      speed_pi .actual_value = low_pass_filter (Calculate_Speed(0.000515),speed_pi .actual_value ,0.1);//计算速度时间=10/实际频率*0.0005
			//speed_pi .actual_value = Calculate_Speed(0.002);//计算速度
			 jj=0;
		 }
//		
		 ////////////////////步进电机电流环部分
		 		  get_AB_current(&m1_foc );
					Sector_tracker();
		 	//	 HAL_GPIO_WritePin (GPIOB,GPIO_PIN_12,GPIO_PIN_RESET );//测试得出该计算量占据约0.02ms时间
					b_step =(m1_foc .angle + 256 * (m1_foc.angle_sector % 4) + m1_foc.lead_angle+(int8_t )(0.006*speed_pi .actual_value)) % 1024;//电气角度计算+运算延时电角度补偿
					current_ctr  (&m1_foc ,b_step);

//		
//    set_uduq (&m1_foc,0,4);		 
//	  foc_open(&m1_foc ,b_step);
//		b_step +=4;if(b_step >1023)b_step =0;
		 
       //   HAL_GPIO_TogglePin(GPIOB,GPIO_PIN_12);
	 }
	 else if(Iadjust ==1)
	 {
	     adjust_I ();
	 }
	 else
	 {

//    set_uduq (&m1_foc,0,4);		 
//	  foc_open(&m1_foc ,b_step);
//		b_step +=8;if(b_step >1023)b_step =0;
//		 	   get_AB_current(&m1_foc );
	//	HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_9);
	 }

  }
	   
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
