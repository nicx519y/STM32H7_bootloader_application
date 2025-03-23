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
  
  // 检查端口连接状态和状态位，但不修改任何状态
  bool port_connected = (hprt & USB_OTG_HPRT_PCSTS) != 0;
  bool connection_detected = (hprt & USB_OTG_HPRT_PCDET) != 0;
  bool enable_changed = (hprt & USB_OTG_HPRT_PENCHNG) != 0;
  bool port_enabled = (hprt & USB_OTG_HPRT_PENA) != 0;
  
  // 仅用于日志记录 - 不修改任何寄存器
  if (port_connected && connection_detected && !connection_handled) {
    USB_HOST_DBG("USB device connection detected! Starting enumeration... (detection #%lu)", ++connection_count);
    USB_HOST_DBG("  Connection status before: HPRT=0x%08lX", hprt);
    
    // 解析速度位
    uint32_t speed_bits = (hprt & USB_OTG_HPRT_PSPD) >> USB_OTG_HPRT_PSPD_Pos;
    USB_HOST_DBG("  Port speed: %s", 
               speed_bits == 0 ? "High Speed" : 
               speed_bits == 1 ? "Full Speed" : 
               speed_bits == 2 ? "Low Speed" : "Unknown");
    
    // 标记连接已处理 (仅用于日志)
    connection_handled = true;
  }
  
  // 检查连接断开 (仅用于日志)
  if (!port_connected && connection_handled) {
    USB_HOST_DBG("USB device disconnected");
    connection_handled = false; // 重置连接状态标志
  }
  
  // 输出更多的寄存器状态
  if (enable_changed) {
    USB_HOST_DBG("  Port enable changed: %s", port_enabled ? "ENABLED" : "DISABLED");
  }
  
  // 调用TinyUSB的中断处理函数
  extern void tuh_int_handler(uint8_t rhport, bool in_isr);
  USB_HOST_DBG("  Calling TinyUSB handler...");
  tuh_int_handler(1, false);
  
  // 检查HPRT的变化
  uint32_t hprt_after = *hprt_reg;
  if (hprt != hprt_after) {
    USB_HOST_DBG("  HPRT changed after interrupt: 0x%08lX -> 0x%08lX", hprt, hprt_after);
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

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report_desc, uint16_t desc_len)
{
  USB_HOST_DBG("设备挂载成功");
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx)
{ 
  USB_HOST_DBG("设备卸载成功");
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len)
{
  USB_HOST_DBG("设备报告接收成功");
}
