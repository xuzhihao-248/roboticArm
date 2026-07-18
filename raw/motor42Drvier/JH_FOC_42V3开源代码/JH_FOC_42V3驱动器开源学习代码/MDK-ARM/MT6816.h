#include <stdbool.h>	
#include <string.h>		
#include <stdlib.h>		
#include <stdio.h>		
#include <spi.h>		
#include <main.h>		

#define MT6816_SPI_CS_H()	     HAL_GPIO_WritePin(GPIOA,GPIO_PIN_4,GPIO_PIN_SET) 
#define MT6816_SPI_CS_L()		 HAL_GPIO_WritePin(GPIOA,GPIO_PIN_4,GPIO_PIN_RESET) 
#define MT6816_SPI_Get_HSPI		    (hspi1)

extern uint16_t init_angle;
extern int  DIR;
extern uint8_t mt6816_flag;

typedef struct {
		uint16_t sample_data;
    uint16_t angle;
    bool parity_ok;
    bool no_magnet_flag;

} MT6816_SPI_Data_Typedef;


// 封装函数：带重试读取角度，最多尝试10次
float REIN_MT6816_GetAngleData(void);
	void REIN_mt6816_spi_data_Signal_Init(void);
bool check_mt6816(void);//检测编码器是否存在
void SysTick_Delay_us(uint32_t us);

