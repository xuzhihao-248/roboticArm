/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
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
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "can.h"

/* USER CODE BEGIN 0 */


extern uint16_t my_can_id;
uint8_t can_Prescaler=6;//CAN时钟分频系数，6对应500kps
//#define CAN_BASE_ID 0x200<<5                       ///< CAN标准ID，最大11位，也就是0x7FF

#define CAN_FILTER_MODE_MASK_ENABLE 0       ///< CAN过滤器模式选择：=0：列表模式  =1：屏蔽模式


CAN_TxPacketTypeDef my_can_tx;
CAN_RxPacketTypeDef my_can_rx;

void CAN_Filter_Config(void)
{
    CAN_FilterTypeDef sFilterConfig;


    sFilterConfig.FilterBank           = 0;                                             // 设置过滤器组编号
#if CAN_FILTER_MODE_MASK_ENABLE
    sFilterConfig.FilterMode           = CAN_FILTERMODE_IDMASK;                         // 屏蔽位模式
#else
    sFilterConfig.FilterMode           = CAN_FILTERMODE_IDLIST;                         // 列表模式
#endif
    sFilterConfig.FilterScale          = CAN_FILTERSCALE_16BIT;                         // 标准ID使用16位宽
    sFilterConfig.FilterIdHigh         = my_can_id<<5;                                     // 标识符寄存器一ID高十六位，放入扩展帧位
    sFilterConfig.FilterIdLow          = my_can_id<<5;                                     // 标识符寄存器一ID低十六位，放入扩展帧位
    sFilterConfig.FilterMaskIdHigh     = 0x7ff<<5;                                     // 标识符寄存器二ID高十六位，放入扩展帧位
    sFilterConfig.FilterMaskIdLow      = 0x7ff<<5;                                     // 标识符寄存器二ID低十六位，放入扩展帧位
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;                                  // 过滤器组关联到FIFO0
    sFilterConfig.FilterActivation     = ENABLE;                                        // 激活过滤器
    sFilterConfig.SlaveStartFilterBank = 14;                                            // 设置从CAN的起始过滤器编号，本单片机只有一个CAN，顾此参数无效
    if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

void CAN_Init(void)
{
	  //my_can_id =*(__IO uint32_t *)0x0800F804;
	  if(my_can_id >0x7ff)my_can_id =0x200;
	  my_can_rx .can_rx_flag =0;
	
	  my_can_tx .hdr .StdId=my_can_id ;//设置发送编号，与接收一致，可认为是本机地址
    my_can_tx .hdr .IDE=CAN_ID_STD;//标准ID类型
	  my_can_tx .hdr .DLC=8;//数据长度8
	  my_can_tx .hdr .RTR=CAN_RTR_DATA ;//数据帧
	  my_can_tx.hdr .TransmitGlobalTime =DISABLE ;
	
    MX_CAN_Init();
    CAN_Filter_Config();
    HAL_CAN_Start(&hcan);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);                   // 使能CAN接收中断
}

/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* CAN init function */
void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = can_Prescaler;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_7TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**CAN GPIO Configuration
    PB8     ------> CAN_RX
    PB9     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    __HAL_AFIO_REMAP_CAN1_2();

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN GPIO Configuration
    PB8     ------> CAN_RX
    PB9     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8|GPIO_PIN_9);

    /* CAN1 interrupt Deinit */
  /* USER CODE BEGIN CAN1:USB_LP_CAN1_RX0_IRQn disable */
    /**
    * Uncomment the line below to disable the "USB_LP_CAN1_RX0_IRQn" interrupt
    * Be aware, disabling shared interrupt may affect other IPs
    */
    /* HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn); */
  /* USER CODE END CAN1:USB_LP_CAN1_RX0_IRQn disable */

  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
uint8_t CAN_Transmit(CAN_TxPacketTypeDef* packet)
{
    if(HAL_CAN_AddTxMessage(&hcan, &packet->hdr, packet->payload, &packet->mailbox) != HAL_OK)
        return 1;
    return 0;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
 
    // CAN数据接收
     if(hcan->Instance == CAN1)

    {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &my_can_rx .hdr, my_can_rx.payload) == HAL_OK)       // 获得接收到的数据头和数据
        {
           my_can_rx.can_rx_flag=1;
           HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);                        // 再次使能FIFO0接收中断
        }
    }
}

/* USER CODE END 1 */
