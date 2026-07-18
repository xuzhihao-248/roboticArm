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


#include "adc_task.h"
#include "adc.h"
#include "math.h"



float offet_I[2]={0.0 , 0.0 };

uint16_t  get_adc_xchannel(uint8_t CHANNEL)//获取某一路ADC值
{ 
	uint16_t adcvalue=0;
	 ADC_ChannelConfTypeDef sConfig = {0};
	switch (CHANNEL )
	{
		case 0:   sConfig.Channel = ADC_CHANNEL_0;break ;
		//case 1:   sConfig.Channel = ADC_CHANNEL_1;break ;

		default :break ;
	}
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
	 HAL_ADC_Start(&hadc2);
   HAL_ADC_PollForConversion(&hadc2,0xffff);//等待ADC转换完成
   adcvalue=  HAL_ADC_GetValue(&hadc2);
	 HAL_ADC_Stop(&hadc2);
	return adcvalue ;
}


float get_temp(void)
{
	 	  //使用发03内部传感器
 	HAL_ADC_Start(&hadc1);
   HAL_ADC_PollForConversion(&hadc1,100);//等待ADC转换完成
   uint16_t adcvalue=  HAL_ADC_GetValue(&hadc1);
	 HAL_ADC_Stop(&hadc1);

//   	uint16_t adcvalue=get_adc_xchannel(11);
//	  uint16_t Rnc=(10000*adcvalue )/(4095-adcvalue );
//	  float temp=0.000001f;
//		//temp =81.005*exp(-1.2/(4096-adcvalue)*adcvalue);//5-50摄氏度拟合结果比较好
//	  temp =((log(Rnc*0.0001)*0.0003) + 0.00335 );
//	  temp =1.0 / temp -273.15-5;
	  float temp=(1.43-(adcvalue *3.3/4096))/0.0043+25;
		return temp ;
}


float  get_vol(void)
{
		  uint16_t adcvalue=get_adc_xchannel(0);
	    float vol=0.1f;
      vol=0.0089*adcvalue;//输入电压
			return vol;
}








