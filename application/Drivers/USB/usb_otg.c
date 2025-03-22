/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    usb_otg.c
 * @brief   This file provides code for the configuration
 *          of the USB_OTG instances.
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
#include "usb_otg.h"
#include "usbh.h"
#include "board_cfg.h"
#include "stm32h7xx_it.h"
#include <stdbool.h>

/* USER CODE BEGIN 0 */

void USB_OTG_Host_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_PWREx_EnableUSBVoltageDetector();

  __HAL_RCC_GPIOB_CLK_ENABLE();
  /**USB_OTG_HS GPIO Configuration
  PB15     ------> USB_OTG_HS_DP
  PB14     ------> USB_OTG_HS_DM
  */
  GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_OTG2_FS;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USB_OTG_HS clock enable */
  __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

  /* USB_OTG_HS interrupt Init */
  HAL_NVIC_SetPriority(OTG_HS_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
  /* USER CODE BEGIN USB_OTG_HS_MspInit 1 */
  USB_HOST_DBG("USB_OTG_HS_MspInit");
}

/* USER CODE END 0 */

HCD_HandleTypeDef hhcd_USB_OTG_HS;

/**
 * @brief Enhanced OTG_HS_IRQHandler that provides more debug information
 * @note This function should be called from the original IRQHandler
 */
void USB_HS_EnhancedIRQHandler(void)
{
  // 静态变量跟踪上一次的中断状态，避免重复日志
  static uint32_t last_gintsts = 0;
  static bool connection_handled = false;
  static uint32_t connection_count = 0;
  
  // 保存中断状态
  uint32_t gintsts = USB_OTG_HS->GINTSTS;
  uint32_t gintmsk = USB_OTG_HS->GINTMSK;
  uint32_t pending = gintsts & gintmsk;
  volatile uint32_t* hprt_reg = (volatile uint32_t*)((uint32_t)USB_OTG_HS + 0x440);
  uint32_t hprt = *hprt_reg;
  
  // 在每次中断开始时输出基本信息
  USB_HOST_DBG("USB IRQ: GINTSTS=0x%08lX, pending=0x%08lX, HPRT=0x%08lX", 
          gintsts, pending, hprt);
  
  // 检查端口连接状态 (PCSTS位)
  bool port_connected = (hprt & USB_OTG_HPRT_PCSTS) != 0;
  
  // 如果发现端口连接且之前未处理过
  if (port_connected && !connection_handled) {
    USB_HOST_DBG("USB device connection detected! Starting enumeration... (detection #%lu)", ++connection_count);
    
    // 记录端口状态变化前的数据
    USB_HOST_DBG("  Connection status before: HPRT=0x%08lX", hprt);
    
    // 清除任何连接状态位
    uint32_t new_hprt = hprt;
    // 清除W1C位(写1清除)，避免意外清除
    new_hprt &= ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET | 
                 USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);
    // 设置PCDET位来确认已检测到连接
    new_hprt |= USB_OTG_HPRT_PCDET;
    
    // 写入HPRT寄存器
    *hprt_reg = new_hprt;
    USB_HOST_DBG("  After status bits cleared: HPRT=0x%08lX", *hprt_reg);
    
    // 标记连接已处理
    connection_handled = true;
    
    // 记录更多诊断信息，帮助理解问题
    USB_HOST_DBG("  Port speed: %s", 
           ((hprt & 0x00000060) == 0x00000000) ? "High Speed" : 
           ((hprt & 0x00000060) == 0x00000020) ? "Full Speed" : 
           ((hprt & 0x00000060) == 0x00000040) ? "Low Speed" : "Unknown");
  }
  
  // 检查连接断开
  if (!port_connected && connection_handled) {
    USB_HOST_DBG("USB device disconnected");
    connection_handled = false; // 重置连接状态标志
  }
  
  // 处理特定的中断
  if (pending != 0) {
    // 只在中断状态变化时详细打印，以减少日志
    if (pending != last_gintsts) {
      USB_HOST_DBG("  Interrupt status changed:");
      last_gintsts = pending;
      
      // 检查端口中断
      if (pending & USB_OTG_GINTSTS_HPRTINT) { // 位9：端口中断
        USB_HOST_DBG("  * Port interrupt: HPRT=0x%08lX", hprt);
        
        // 检查特定端口状态
        if (hprt & USB_OTG_HPRT_PCDET) {
          USB_HOST_DBG("    - Port connection detected (PCDET)");
        }
        
        if (hprt & USB_OTG_HPRT_PENCHNG) {
          USB_HOST_DBG("    - Port enable changed (PENCHNG): %s", 
                 (hprt & USB_OTG_HPRT_PENA) ? "Enabled" : "Disabled");
        }
        
        if (hprt & USB_OTG_HPRT_POCCHNG) {
          USB_HOST_DBG("    - Port overcurrent changed (POCCHNG)");
        }
      }
      
      if (pending & USB_OTG_GINTSTS_DISCINT) {
        USB_HOST_DBG("  * Device disconnect interrupt (DISCINT)");
      }
      
      if (pending & USB_OTG_GINTSTS_SRQINT) {
        USB_HOST_DBG("  * Session request interrupt (SRQINT)");
      }
      
      if (pending & USB_OTG_GINTSTS_RXFLVL) {
        USB_HOST_DBG("  * RxFIFO non-empty interrupt (RXFLVL)");
      }
      
      if (pending & USB_OTG_GINTSTS_HCINT) {
        USB_HOST_DBG("  * Host channel interrupt (HCINT)");
        // 输出每个主机通道的状态
        // HAINT寄存器在STM32H7的USB_OTG_HS中需要通过偏移量访问
        volatile uint32_t* haint_reg = (volatile uint32_t*)((uint32_t)USB_OTG_HS + 0x414); // HAINT偏移量为0x414
        uint32_t haint = *haint_reg;
        USB_HOST_DBG("    HAINT=0x%08lX", haint);
      }
    }
  }
  
  // 调用TinyUSB的中断处理函数
  // 对于STM32H7 OTG_HS，TinyUSB使用端口号1
  extern void tuh_int_handler(uint8_t rhport, bool in_isr);
  USB_HOST_DBG("  Calling TinyUSB handler...");
  tuh_int_handler(1, true);
  
  // 中断后再次检查关键状态
  uint32_t post_hprt = *hprt_reg;
  if (post_hprt != hprt) {
    USB_HOST_DBG("  HPRT changed after interrupt: 0x%08lX -> 0x%08lX", hprt, post_hprt);
  }
  
  USB_HOST_DBG("  Interrupt processing complete");
}

/**
 * @brief Periodically check TinyUSB status and display USB device information
 * @note This function should be called periodically in the main loop
 */
void USB_HS_MonitorTinyUSB(void)
{
  static uint32_t last_check_time = 0;
  static bool mount_helper_run = false;
  static uint8_t check_count = 0;
  
  // Check every 2 seconds
  uint32_t current_time = HAL_GetTick();
  if (current_time - last_check_time > 2000) {
    last_check_time = current_time;
    check_count++;
    
    // Get port status
    volatile uint32_t* hprt_reg = (volatile uint32_t*)((uint32_t)USB_OTG_HS + 0x440);
    uint32_t hprt = *hprt_reg;
    bool port_connected = (hprt & USB_OTG_HPRT_PCSTS) != 0;
    bool port_enabled = (hprt & USB_OTG_HPRT_PENA) != 0;
    
    USB_HOST_DBG("===== USB Monitor(%u) =====", check_count);
    USB_HOST_DBG("Port Status: %s, %s", 
           port_connected ? "Connected" : "Disconnected",
           port_enabled ? "Enabled" : "Disabled");
    
    // Check TinyUSB device status
    extern bool tuh_mounted(uint8_t dev_addr);
    extern uint8_t tuh_rhport_status_get(uint8_t rhport);
    
    uint8_t rhport_status = tuh_rhport_status_get(1);
    USB_HOST_DBG("Root Hub Port Status: 0x%02X", rhport_status);
    
    // Check if devices at addresses 1-7 are mounted
    USB_HOST_DBG("Device Mount Status:");
    bool any_mounted = false;
    for (uint8_t i = 1; i <= 7; i++) {
      bool mounted = tuh_mounted(i);
      if (mounted) {
        USB_HOST_DBG("  Address %u: Mounted", i);
        any_mounted = true;
      }
    }
    
    if (!any_mounted) {
      USB_HOST_DBG("  No devices mounted");
      
      // If port is connected but not enabled, and we haven't tried auxiliary mounting yet
      if (port_connected && !port_enabled && !mount_helper_run && check_count > 2) {
        USB_HOST_DBG("Attempting auxiliary device mounting...");
        
        // Try to force port reset
        uint32_t new_hprt = hprt;
        new_hprt &= ~(USB_OTG_HPRT_PCDET | USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);
        new_hprt |= USB_OTG_HPRT_PRST; // Set port reset bit
        *hprt_reg = new_hprt;
        
        HAL_Delay(50); // Wait for reset to complete
        
        // Clear reset bit
        new_hprt = *hprt_reg;
        new_hprt &= ~(USB_OTG_HPRT_PCDET | USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG | USB_OTG_HPRT_PRST);
        *hprt_reg = new_hprt;
        
        USB_HOST_DBG("Port reset complete, HPRT=0x%08lX", *hprt_reg);
        mount_helper_run = true;
      }
      
      // If port is connected but TinyUSB hasn't recognized it
      if (port_connected) {
        USB_HOST_DBG("USB Host port is connected but TinyUSB hasn't recognized device, HPRT=0x%08lX", hprt);
      }
    }
    
    // Check latest status of key registers
    USB_HOST_DBG("USB Register Status:");
    USB_HOST_DBG("  GAHBCFG: 0x%08lX", USB_OTG_HS->GAHBCFG);
    USB_HOST_DBG("  GINTMSK: 0x%08lX", USB_OTG_HS->GINTMSK);
    USB_HOST_DBG("  GINTSTS: 0x%08lX", USB_OTG_HS->GINTSTS);
    USB_HOST_DBG("  HPRT: 0x%08lX", hprt);
    
    USB_HOST_DBG("=====================");
  }
}
