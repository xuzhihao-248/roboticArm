#ifndef __FLASH_H
#define __FLASH_H
 
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DIR_AD 200
#define stepm_type 900
#define motortype 901


//定义第62页地址,62页储存校准数据，63页储存其他数据
#define FLASH_USER_START_ADDR 0x0800F800 
 
//#define FLASH_USER_START_ADDR   ADDR_FLASH_PAGE_15   /* Start @ of user Flash area */
//#define FLASH_USER_END_ADDR     ADDR_FLASH_PAGE_18   /* End @ of user Flash area */
 
 
int Flash_HAL_Write_Data(uint32_t addr, uint32_t *data);
int Flash_HAL_Write_N_Data(uint32_t addr, uint16_t *data, uint16_t num);
void Flash_HAL_Read_N_Data(uint32_t addr, uint16_t *data, uint32_t num);
void Flash_HAL_Read_N_Byte(uint32_t addr, uint8_t *data, uint32_t num);
void  Flash_Read_Angle(uint16_t buf[200]);
uint16_t flash_read_dir(int addr);
 void Flash_HAL_Read_N_32Data(uint32_t addr, uint32_t *data, uint32_t num);
#endif





