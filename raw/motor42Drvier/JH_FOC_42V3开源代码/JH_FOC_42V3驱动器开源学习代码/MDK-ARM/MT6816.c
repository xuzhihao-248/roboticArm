
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



#include "mt6816.h"
#include "stdio.h"
#include "myflash.h"
#include "b_motor_foc.h"
#include "float_char.h"
#include "iwdg.h"
#include "can.h"
#include "usbd_cdc_if.h"
#include "connecting.h"


int  DIR=-1;//编码器方向+-1
	MT6816_SPI_Data_Typedef mt6816_spi_data;
	uint16_t init_angle=0;
uint8_t mt6816_flag=0;//是否存在编码器
extern float vol;
uint8_t encoder_adjust_flag=1;//定义编码器校准的标记

extern uint8_t connect_type;//定义通信类型0：USB，1:CAN
extern PCD_HandleTypeDef hpcd_USB_FS;



//定义延时函数
void SysTick_Delay_us(uint32_t us)
{
  uint16_t i=6;
	for(uint16_t k=0;k<us;k++)
	{
		i=6;
  	while(i--);
	}
}

		uint16_t temp[201] = {0};
void adjust_b_motor(void)//正反转一圈保存数据
{
		uint16_t tetemp1[200]={0};
		uint16_t tetemp2[200]={0};
    set_uduq(&m1_foc ,0,0.5);//小占空比对齐
	  foc_open(&m1_foc ,0);//电角度0初始化
	  HAL_Delay (100);
		set_uduq(&m1_foc ,0,1);//小占空比对齐
	  foc_open(&m1_foc ,0);//电角度0初始化
	  HAL_Delay (100);
		float  bbb=33/vol;
		if(bbb>3)bbb=3;
		set_uduq(&m1_foc ,0,bbb);
		foc_open(&m1_foc ,0);
		HAL_Delay(900);
		REIN_MT6816_GetAngleData();
    HAL_IWDG_Refresh (&hiwdg );//看门狗清零
		/////////正转一圈
		for(uint16_t i=0;i<200;i++)
		{
			tetemp1 [i]=REIN_MT6816_GetAngleData();
			for(uint16_t j=0;j<256;j++)
			{
				foc_open (&m1_foc ,j+(i%4)*256);
			 //foc_open(&m1_foc ,aglc[(i+1)%4]);
				SysTick_Delay_us(100);
			}
			//HAL_Delay(10);
			HAL_IWDG_Refresh (&hiwdg );//看门狗清零
//			printf("%d\r\n",mt6816.angle_data);
		}
		foc_open(&m1_foc ,0);
		HAL_Delay (1000);
		/////////////反转一圈
		for(uint16_t i=0;i<200;i++)
		{
			tetemp2 [i]=REIN_MT6816_GetAngleData();
			for(uint16_t j=0;j<256;j++)
			{
				foc_open(&m1_foc ,1023-j-256*(i%4));
         SysTick_Delay_us (100);			
			}
			//foc_open(&m1_foc ,aglc1[i%4]);
			//HAL_Delay(10);
			HAL_IWDG_Refresh (&hiwdg );//看门狗清零
//			printf("%d\r\n",mt6816.angle_data);
		}
		for(uint8_t jj=0;jj<200;jj++)//将正转反转得到的值取平均
		{
			
			if(jj==0)
				{
					if(abs(tetemp1 [0]-tetemp2 [0])>400)
					{
						if(tetemp1 [0]>tetemp2 [0])
						{
						 if(16384-tetemp1 [0]>tetemp2 [0])//在大的数这一边
							temp [jj] =(uint16_t )(0.5*(tetemp1 [0]+tetemp2 [0]+16384));
						 else //在小的数那一边
							 temp [jj] =(uint16_t )(0.5*(tetemp1 [0]+tetemp2 [0]-16384));	 
						}
						else
						{
							if(16384-tetemp2 [0]>tetemp1 [0])//在大的数这一边
							temp [jj] =(uint16_t )(0.5*(tetemp1 [0]+tetemp2 [0]+16384));
						 else //在小的数那一边
							 temp [jj] =(uint16_t )(0.5*(tetemp1 [0]+tetemp2 [0]-16384));	 
						}					
					}
					else
					temp[jj] =(uint16_t )(0.5*(tetemp1 [0]+tetemp2  [0]));
				}
			else
		  {
					if(abs(tetemp1 [jj]-tetemp2  [200-jj])>400)
					{
					if(tetemp1 [jj]>tetemp2 [200-jj])
						{
						 if(16384-tetemp1 [jj]>tetemp2 [200-jj])//在大的数这一边
							temp [jj] =(uint16_t )(0.5*(tetemp1 [jj]+tetemp2 [200-jj]+16384));
						 else //在小的数那一边
							 temp [jj] =(uint16_t )(0.5*(tetemp1 [jj]+tetemp2 [200-jj]-16384));	 
						}
						else
						{
							if(16384-tetemp2 [200-jj]>tetemp1 [jj])//在大的数这一边
							temp [jj] =(uint16_t )(0.5*(tetemp1 [jj]+tetemp2 [200-jj]+16384));
						 else //在小的数那一边
							 temp [jj] =(uint16_t )(0.5*(tetemp1 [jj]+tetemp2 [200-jj]-16384));	 
						}		
					
					}
					else
					temp[jj] =(uint16_t )(0.5*(tetemp1 [jj]+tetemp2  [200-jj]));
			}

		}

}
void REIN_mt6816_spi_data_Signal_Init(void)//编码器校准，识别编码器方向，非线性校准以及无刷电机极对数等
	{
			mt6816_spi_data.sample_data = 0;
			mt6816_spi_data.angle = 0;
    encoder_adjust_flag=0;//编码器校准中
    uint8_t ad_s_flag=0;//校准是否成功
		
		for(uint8_t j=0;j<5;j++)//最多校准5次
		{
			adjust_b_motor ();

////////////////////判断校准数据准确性		
			uint8_t ad_count=0;
			for(uint8_t i=0;i<200;i++)
			{
				uint16_t cc=0;
				if(i!=199)
				cc=abs(temp [i+1]-temp [i]);
				else 
				cc=abs(temp [0]-temp [i]);
				if(cc>2000)cc=16384-cc;

			 if(cc>65&&cc<95)
				 ad_count ++;
//			 else
//			 {
//				 char s[8],dc[8],dd[18];
//			 	snprintf(s, sizeof(s), "%d", i  );
//				 snprintf (dc,8,"%d",cc);
//				 snprintf (dd,18,"i:%scc:%s\n",s,dc);
//				CDC_Transmit_FS(dd,18);
//				HAL_Delay (10);			HAL_IWDG_Refresh (&hiwdg );//看门狗清零
//			 }

			}
			if(ad_count ==200)
			{
			  ad_s_flag =1;
				break ;
			}
					set_uduq(&m1_foc ,0,0);
					foc_open (&m1_foc ,0);
    }
		if(ad_s_flag ==1)
		{
	    uint8_t dir_count=0;
				for(uint8_t ii=0;ii<10;ii++)
				{
					if((temp [ii+1]-temp [ii])>0)
						dir_count ++;
			  }
				
				if(dir_count >=8)
						DIR=1;
				else DIR=-1;
					
				
			 
			
				if(DIR==1) //////////////////保存编码器方向值
				temp[200] = 11;
				else 
				temp [200]=22;
				
				Flash_HAL_Write_N_Data(FLASH_USER_START_ADDR,temp,201);
				HAL_Delay (100);
				encoder_adjust_flag=1;//校准完成
				if(connect_type==0)
				CDC_Transmit_FS ("Adjust Success!\n",16);	
				else
				{
					set_cantx_buf (0xdd,0xc0,0xc0,0,0,0,0,0);//成功返回的代码
			    CAN_Transmit(&my_can_tx );
				}
				HAL_Delay (100);
				//复位前注意释放关键硬件资源
					 if(connect_type ==0)//失能CAN通信
			 HAL_CAN_MspDeInit (&hcan);
			 else 
			 {
				HAL_PCD_DeInit(&hpcd_USB_FS);//失能USB通信
				CAN_Init();//初始化总线
			 }
				HAL_Delay(20);
				__disable_irq();          // 关闭所有中断
				HAL_NVIC_SystemReset();   // 执行复位
  }
  else
		{
		
						if(connect_type==0)
				CDC_Transmit_FS ("Adjust False!\n",14);	
				else
				{
					set_cantx_buf (0xdd,0xc0,0xFF,0,0,0,0,0);//成功返回的代码
			    CAN_Transmit(&my_can_tx );
				}
		}
	
}

bool RINE_mt6816_spi_data_Get_AngleData(void)
{
			uint16_t data_t[2];
			uint16_t data_r[2];
			uint8_t h_count;
			data_t[0] = (0x80 | 0x03) << 8;
			data_t[1] = (0x80 | 0x04) << 8;
			for(uint8_t i=0; i<3; i++){
					//读取SPI数据
					MT6816_SPI_CS_L();
					HAL_SPI_TransmitReceive(&MT6816_SPI_Get_HSPI, (uint8_t*)&data_t[0], (uint8_t*)&data_r[0], 1, HAL_MAX_DELAY);
					MT6816_SPI_CS_H();
					MT6816_SPI_CS_L();
					HAL_SPI_TransmitReceive(&MT6816_SPI_Get_HSPI, (uint8_t*)&data_t[1], (uint8_t*)&data_r[1], 1, HAL_MAX_DELAY);
					MT6816_SPI_CS_H();
					mt6816_spi_data.sample_data = ((data_r[0] & 0x00FF) << 8) | (data_r[1] & 0x00FF);
					//奇偶校验
					h_count = 0;
					for(uint8_t j=0; j<16; j++){
					if(mt6816_spi_data.sample_data & (0x0001 << j))
						h_count++;
					}
					if(h_count & 0x01){
					mt6816_spi_data.parity_ok = false;
					}
					else{
					mt6816_spi_data.parity_ok = true;
					break;
					}
			}
			if(mt6816_spi_data.parity_ok)
			{
					mt6816_spi_data.angle = mt6816_spi_data.sample_data >> 2;
					mt6816_spi_data.no_magnet_flag = (bool)(mt6816_spi_data.sample_data & (0x0001 << 1));
						if(mt6816_spi_data.no_magnet_flag)//磁通不足
							return false ;
						else return true ;
			}
			else 
				return false ;//奇偶校验不通过
}

	uint16_t last_data=0;
// 封装函数：带重试读取角度，最多尝试10次
float REIN_MT6816_GetAngleData(void)
{
    uint8_t retry = 0;

    for(retry = 0; retry < 10; retry++){
        if(RINE_mt6816_spi_data_Get_AngleData()){
            // 根据手册，角度分辨率16384(14bit)，对应360度
//            angle_degree = (float)mt6816_spi_data.angle* 360.0f / 16384.0f;
//            return angle_degree;
					last_data =mt6816_spi_data.angle;
					return mt6816_spi_data.angle;
        }
    }
    // 读取失败，返回上一次正确的数据
    return last_data;
}


bool check_mt6816(void)//检测编码器是否存在
{
   
   uint8_t n=0;
	for(uint8_t i=0;i<10;i++)
	{
	  if(RINE_mt6816_spi_data_Get_AngleData()&&mt6816_spi_data.angle<16385)
			n++;
		HAL_Delay (1);
	}
	if(n>=9)//9次检测到数据为真
		return true;
	else
		return false;
}






