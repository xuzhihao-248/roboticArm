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



#include "myflash.h"
 
 
 
//STM32G431RBT6的FLASH为128KB，因此FLASH地址起始地址:0x0800 0000,结束地址是:0x0802 0000
 
/**
  * @brief  HAL库版写一个uint32t类型的数据
  * @param  addr: 存储数据的地址
  * @param  data: 写入的数据
  * @retval 成功返回0， 失败返回-1
  */
int Flash_HAL_Write_Data(uint32_t addr, uint32_t *data)
{
    //1、FLASH解锁
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);
    //2、FLASH擦除
    FLASH_EraseInitTypeDef EraseInitStruct; 
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;    //页擦除
    EraseInitStruct.Banks = FLASH_BANK_1;
    EraseInitStruct.PageAddress = (FLASH_USER_START_ADDR+0X400);    //从第几个页开始擦除（0开始）
    EraseInitStruct.NbPages = 1;    //擦除多少个页
    uint32_t PageError = 0;            //记录擦除出错时的起始地址
    if(HAL_FLASHEx_Erase(&EraseInitStruct, &PageError)!=HAL_OK)
    {
        return -1;
    }
    //3、FLASH写入
    for(uint16_t i=0; i<20; i++)
    {
        if(HAL_FLASH_Program(TYPEPROGRAM_WORD, addr, data[i])!=HAL_OK)
        {
          //  printf("FLASH写入失败\r\n");
            return -1;
        }
        addr += sizeof(uint32_t);
    }
    //4、FLASH上锁
    HAL_FLASH_Lock();
    return 0;
}
 


/**
  * @brief  HAL库版写N个uint32_t类型的数据
  * @param  addr: 存储数据的地址
  * @param  data: 数据数组
  * @param  num: 数据的个数
  * @retval 成功返回0， 失败返回-1
  */

int Flash_HAL_Write_N_Data(uint32_t addr, uint16_t *data, uint16_t num)
{
    //1、FLASH解锁
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);
    
    //2、FLASH擦除
    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;    //页擦除
    EraseInitStruct.Banks = FLASH_BANK_1;
    EraseInitStruct.PageAddress = FLASH_USER_START_ADDR;    //从第几个页开始擦除（0开始）
    EraseInitStruct.NbPages = 1;    //擦除多少个页
    uint32_t PageError = 0;            //记录擦除出错时的起始地址
    if(HAL_FLASHEx_Erase(&EraseInitStruct, &PageError)!=HAL_OK)
    {
        //printf("FLASH擦除出错,开始出错地址:%#x\r\n", PageError);
        return -1;
    }
    //3、FLASH写入
    for(uint16_t i=0; i<num; i++)
    {
        if(HAL_FLASH_Program(TYPEPROGRAM_WORD, addr, data[i])!=HAL_OK)
        {
          //  printf("FLASH写入失败\r\n");
            return -1;
        }
        addr += sizeof(uint32_t);
    }
    //4、FLASH上锁
    HAL_FLASH_Lock();    
    return 0;
}
 
/**
  * @brief  HAL库版读取N个uint64_t类型的数据
  * @param  addr: 读取数据的地址（用户空间的地址）
  * @param  data: 数据数组
  * @param  num: 数据的个数
  * @retval NONE
  */
void Flash_HAL_Read_N_Data(uint32_t addr, uint16_t *data, uint32_t num)
{
 
    for(uint32_t i=0; i<num; i++)
    {
        data[i] = *(volatile uint16_t*)addr;
        addr += sizeof(uint32_t);//根据读取的数据类型进行内存地址递增
    }
}
 
/**
  * @brief  HAL库版读取N个uint8_t类型的数据
  * @param  addr: 读取数据的地址
  * @param  data: 数据数组
  * @param  num: 数据的个数
  * @retval NONE
  */
void Flash_HAL_Read_N_Byte(uint32_t addr, uint8_t *data, uint32_t num)
{
 
    for(uint32_t i=0; i<num; i++)
    {
        data[i] = *(volatile uint8_t*)addr;
        addr += sizeof(uint8_t);//根据读取的数据类型进行内存地址递增
    }
}


void  Flash_Read_Angle(uint16_t buf[200])
{
  Flash_HAL_Read_N_Data(FLASH_USER_START_ADDR,buf,200);
}


uint16_t flash_read_dir(int addr)
{

		return *(__IO uint32_t *)(FLASH_USER_START_ADDR + addr  * 4);
	
	
}

	
void Flash_HAL_Read_N_32Data(uint32_t addr, uint32_t *data, uint32_t num)
{
 
    for(uint32_t i=0; i<num; i++)
    {
        data[i] = *(volatile uint32_t*)addr;
        addr += sizeof(uint32_t);//根据读取的数据类型进行内存地址递增
    }
}

	
