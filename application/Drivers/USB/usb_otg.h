/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usb_otg.h
  * @brief   This file contains all the function prototypes for
  *          the usb_otg.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_OTG_H__
#define __USB_OTG_H__



#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#define USB_HOST_Debug  1
#if USB_HOST_Debug
  #define USB_HOST_DBG(fmt, ...) printf("[USB_HOST] " fmt "\r\n", ##__VA_ARGS__)
  #define USB_HOST_ERR(fmt, ...) printf("[USB_HOST] Err: " fmt "\r\n", ##__VA_ARGS__)
#else
  #define USB_HOST_DBG(fmt, ...) ((void)0)
  #define USB_HOST_ERR(fmt, ...) ((void)0)
#endif
/* USER CODE END Includes */

extern HCD_HandleTypeDef hhcd_USB_OTG_HS;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */


void USB_OTG_Host_Init(void);
void USB_HS_EnhancedIRQHandler(void);
void USB_HS_MonitorTinyUSB(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USB_OTG_H__ */

