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

void USB_FixInterrupts_TinyUSB(void)
{
  USB_HOST_DBG("Preparing USB OTG HS for TinyUSB...");
  
  // Disable and clear pending interrupt
  HAL_NVIC_DisableIRQ(OTG_HS_IRQn);
  HAL_NVIC_ClearPendingIRQ(OTG_HS_IRQn);
  
  // Set priority
  HAL_NVIC_SetPriority(OTG_HS_IRQn, 2, 0);  // Use same priority as in MspInit
  
  // Get USB OTG HS base address
  uint32_t usb_base = (uint32_t)USB_OTG_HS;
  USB_HOST_DBG("  USB OTG HS Base: 0x%08lX", usb_base);
  
  // Reset the core (try a soft reset to fix mode issues)
  USB_HOST_DBG("  Performing soft reset of USB controller");
  volatile uint32_t* grstctl_reg = (volatile uint32_t*)(usb_base + 0x010); // GRSTCTL
  *grstctl_reg = 0x00000001; // Set CSRST bit (Core Soft Reset)
  
  // Wait for reset to complete
  uint32_t count = 0;
  while ((*grstctl_reg & 0x00000001) && (count < 200000)) {
    count++;
  }
  USB_HOST_DBG("  Soft reset %s", (count < 200000) ? "completed" : "timed out");
  
  // Wait for AHB to be idle
  HAL_Delay(20);
  
  // Now set force host mode
  volatile uint32_t* gusbcfg_reg = (volatile uint32_t*)(usb_base + 0x00C);
  uint32_t gusbcfg_val = *gusbcfg_reg;
  USB_HOST_DBG("  Original GUSBCFG: 0x%08lX", gusbcfg_val);
  
  // Clear all mode bits first, then set host mode only
  gusbcfg_val &= ~(0x60000000);  // Clear both FDMOD and FHMOD
  *gusbcfg_reg = gusbcfg_val;
  HAL_Delay(50);  // Give time for changes to take effect
  
  gusbcfg_val = *gusbcfg_reg;
  gusbcfg_val |= 0x20000000;    // Set FHMOD (Force Host Mode)
  *gusbcfg_reg = gusbcfg_val;
  
  USB_HOST_DBG("  Updated GUSBCFG: 0x%08lX", *gusbcfg_reg);
  HAL_Delay(100);  // Longer delay to ensure mode switch
  
  // Clear all interrupt flags
  volatile uint32_t* gintsts_reg = (volatile uint32_t*)(usb_base + 0x014);
  *gintsts_reg = 0xFFFFFFFF;
  
  // Enable USB OTG HS interrupt
  HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
  
  USB_HOST_DBG("USB OTG HS prepared for TinyUSB");
  
  // TinyUSB will be initialized next
}

/**
  * @brief  Diagnose USB OTG HS interrupt configuration and output status information
  * @param  None
  * @retval None
  */
void USB_DiagnoseInterruptConfig_TinyUSB(void)
{
  USB_HOST_DBG("\r\n===== USB OTG HS Interrupt Configuration Diagnosis =====");
  
  // Check NVIC interrupt configuration
  uint32_t nvic_iser = NVIC->ISER[OTG_HS_IRQn >> 5];
  uint32_t nvic_bit = 1 << (OTG_HS_IRQn & 0x1F);
  uint8_t is_nvic_enabled = (nvic_iser & nvic_bit) ? 1 : 0;
  
  uint32_t nvic_priority = NVIC_GetPriority(OTG_HS_IRQn);
  
  USB_HOST_DBG("NVIC Configuration:");
  USB_HOST_DBG("  OTG_HS_IRQn = %d", OTG_HS_IRQn);
  USB_HOST_DBG("  Interrupt Enable Status: %s", is_nvic_enabled ? "Enabled" : "Disabled");
  USB_HOST_DBG("  Priority: %lu", nvic_priority);
  
  // Get USB OTG HS base address
  uint32_t usb_base = (uint32_t)USB_OTG_HS;
  USB_HOST_DBG("USB OTG HS Base Address: 0x%08lX", usb_base);
  
  // Check USB clock configuration
  uint32_t rcc_ahb1enr = RCC->AHB1ENR;
  uint8_t is_usb_hs_clk_enabled = (rcc_ahb1enr & RCC_AHB1ENR_USB2OTGHSEN) ? 1 : 0;
  uint8_t is_usb_hs_ulpi_clk_enabled = (rcc_ahb1enr & RCC_AHB1ENR_USB2OTGHSULPIEN) ? 1 : 0;
  
  USB_HOST_DBG("USB Clock Configuration:");
  USB_HOST_DBG("  USB OTG HS Clock: %s", is_usb_hs_clk_enabled ? "Enabled" : "Disabled");
  USB_HOST_DBG("  USB OTG HS ULPI Clock: %s", is_usb_hs_ulpi_clk_enabled ? "Enabled" : "Disabled");
  
  // Get values of key registers
  volatile uint32_t gahbcfg = *(volatile uint32_t*)(usb_base + 0x008); // GAHBCFG
  volatile uint32_t gintsts = *(volatile uint32_t*)(usb_base + 0x014); // GINTSTS
  volatile uint32_t gintmsk = *(volatile uint32_t*)(usb_base + 0x018); // GINTMSK
  volatile uint32_t hprt = *(volatile uint32_t*)(usb_base + 0x440);    // HPRT
  volatile uint32_t gusbcfg = *(volatile uint32_t*)(usb_base + 0x00C); // GUSBCFG
  volatile uint32_t gccfg = *(volatile uint32_t*)(usb_base + 0x038);   // GCCFG
  
  USB_HOST_DBG("USB Register Status:");
  USB_HOST_DBG("  GAHBCFG = 0x%08lX", gahbcfg);
  USB_HOST_DBG("    Global Interrupt Enable (GINT): %s", (gahbcfg & 0x00000001) ? "Enabled" : "Disabled");
  
  USB_HOST_DBG("  GUSBCFG = 0x%08lX", gusbcfg);
  USB_HOST_DBG("    Force Host Mode (FHMOD): %s", (gusbcfg & 0x20000000) ? "Enabled" : "Disabled");
  USB_HOST_DBG("    Force Device Mode (FDMOD): %s", (gusbcfg & 0x40000000) ? "Enabled" : "Disabled");
  
  USB_HOST_DBG("  GCCFG = 0x%08lX", gccfg);
  USB_HOST_DBG("    Power Down (PWRDWN): %s", (gccfg & 0x00010000) ? "Enabled" : "Disabled");
  
  USB_HOST_DBG("  GINTSTS = 0x%08lX", gintsts);
  USB_HOST_DBG("    Current Mode (CMOD): %s", (gintsts & 0x00000001) ? "Host" : "Device");
  USB_HOST_DBG("    OTG Interrupt (OTGINT): %s", (gintsts & 0x00000004) ? "Active" : "Inactive");
  USB_HOST_DBG("    SOF Interrupt (SOF): %s", (gintsts & 0x00000008) ? "Active" : "Inactive");
  USB_HOST_DBG("    RxFIFO Non-Empty (RXFLVL): %s", (gintsts & 0x00000010) ? "Active" : "Inactive");
  
  USB_HOST_DBG("  GINTMSK = 0x%08lX", gintmsk);
  USB_HOST_DBG("    OTG Interrupt Mask (OTGM): %s", (gintmsk & 0x00000004) ? "Enabled" : "Disabled");
  USB_HOST_DBG("    SOF Interrupt Mask (SOFM): %s", (gintmsk & 0x00000008) ? "Enabled" : "Disabled");
  USB_HOST_DBG("    RxFIFO Non-Empty Mask (RXFLVLM): %s", (gintmsk & 0x00000010) ? "Enabled" : "Disabled");
  USB_HOST_DBG("    Host Channel Interrupt Mask (HCIM): %s", (gintmsk & 0x01000000) ? "Enabled" : "Disabled");
  USB_HOST_DBG("    Port Interrupt Mask (PRTIM): %s", (gintmsk & 0x00000200) ? "Enabled" : "Disabled");
  
  USB_HOST_DBG("  HPRT = 0x%08lX", hprt);
  USB_HOST_DBG("    Port Connect Status (PCSTS): %s", (hprt & 0x00000001) ? "Connected" : "Disconnected");
  USB_HOST_DBG("    Port Enable (PENA): %s", (hprt & 0x00000004) ? "Enabled" : "Disabled");
  USB_HOST_DBG("    Port Speed (PSPD): %s", 
         ((hprt & 0x00000060) == 0x00000000) ? "High Speed" : 
         ((hprt & 0x00000060) == 0x00000020) ? "Full Speed" : 
         ((hprt & 0x00000060) == 0x00000040) ? "Low Speed" : "Unknown");
  
  // Check for pending interrupts
  uint32_t pending_irq = (gintsts & gintmsk);
  if (pending_irq) {
    USB_HOST_DBG("  Pending Interrupts: 0x%08lX", pending_irq);
    if (pending_irq & 0x00000004) USB_HOST_DBG("    OTG Interrupt Pending");
    if (pending_irq & 0x00000008) USB_HOST_DBG("    SOF Interrupt Pending");
    if (pending_irq & 0x00000010) USB_HOST_DBG("    RxFIFO Non-Empty Interrupt Pending");
    if (pending_irq & 0x00000200) USB_HOST_DBG("    Port Interrupt Pending");
    if (pending_irq & 0x01000000) USB_HOST_DBG("    Host Channel Interrupt Pending");
  } else {
    USB_HOST_DBG("  No Pending Interrupts");
  }
  
  // Check if interrupt handler is correctly installed in vector table
  uint32_t *vector_table = (uint32_t*)SCB->VTOR;
  uint32_t installed_handler = vector_table[OTG_HS_IRQn + 16]; // +16 for core vectors
  uint32_t expected_handler = (uint32_t)&OTG_HS_IRQHandler;
  
  USB_HOST_DBG("  Vector Table Base: 0x%08lX", (uint32_t)vector_table);
  USB_HOST_DBG("  Installed Handler: 0x%08lX", installed_handler);
  USB_HOST_DBG("  Expected Handler: 0x%08lX", expected_handler);
  USB_HOST_DBG("  Handler Match: %s", (installed_handler == expected_handler) ? "Yes" : "No");
  
  USB_HOST_DBG("============ Diagnosis Complete ============\r\n");
}

/**
 * @brief 使用HAL库和直接寄存器操作的混合方式初始化USB OTG HS
 * @note 该函数将先使用HAL库初始化基本结构，然后通过直接寄存器操作修改关键设置
 */
void USB_InitForTinyUSB_Complete(void)
{
  USB_HOST_DBG("\r\n========== USB OTG HS Mixed Initialization Method ==========");
  
  // Step 1: First use HAL library to initialize the basic structure
  USB_HOST_DBG("1. Using HAL library for basic initialization...");
  
  // Create a temporary HAL handle for initialization
  HCD_HandleTypeDef hhcd_temp = {0};
  hhcd_temp.Instance = USB_OTG_HS;
  hhcd_temp.Init.Host_channels = 16;
  hhcd_temp.Init.speed = HCD_SPEED_FULL;
  hhcd_temp.Init.dma_enable = DISABLE;
  hhcd_temp.Init.phy_itface = USB_OTG_EMBEDDED_PHY;
  hhcd_temp.Init.Sof_enable = DISABLE;
  hhcd_temp.Init.low_power_enable = DISABLE;
  hhcd_temp.Init.use_external_vbus = DISABLE;
  
  // Call HAL library initialization
  HAL_HCD_Init(&hhcd_temp);
  USB_HOST_DBG("  HAL library initialization completed");
  
  // Step 2: Disable HAL interrupt handler, we will use our own interrupt handler
  USB_HOST_DBG("2. Configuring interrupt handling...");
  HAL_NVIC_DisableIRQ(OTG_HS_IRQn);
  HAL_NVIC_ClearPendingIRQ(OTG_HS_IRQn);
  HAL_NVIC_SetPriority(OTG_HS_IRQn, 2, 0);
  USB_HOST_DBG("  ✓ Interrupt configuration completed");
  
  // Step 3: Read current register status
  USB_HOST_DBG("3. Current register status:");
  USB_HOST_DBG("  GUSBCFG: 0x%08lX", USB_OTG_HS->GUSBCFG);
  USB_HOST_DBG("  GINTMSK: 0x%08lX", USB_OTG_HS->GINTMSK);
  USB_HOST_DBG("  GAHBCFG: 0x%08lX", USB_OTG_HS->GAHBCFG);
  USB_HOST_DBG("  HPRT: 0x%08lX", *(__IO uint32_t *)((uint32_t)USB_OTG_HS + 0x440));
  
  // Step 4: Force host mode using HAL library
  USB_HOST_DBG("4. Forcing host mode...");
  USB_OTG_HS->GUSBCFG &= ~(USB_OTG_GUSBCFG_FDMOD);
  USB_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_FHMOD;
  
  // Wait for mode switch to complete
  HAL_Delay(100);
  USB_HOST_DBG("  Current GUSBCFG: 0x%08lX", USB_OTG_HS->GUSBCFG);
  
  // Step 5: Ensure port power is turned on
  USB_HOST_DBG("5. Configuring port power...");
  uint32_t hprt = *(__IO uint32_t *)((uint32_t)USB_OTG_HS + 0x440);
  uint32_t new_hprt = hprt & ~(0x0000002E); // Clear W1C bits
  new_hprt |= 0x00001000; // Set port power bit
  *(__IO uint32_t *)((uint32_t)USB_OTG_HS + 0x440) = new_hprt;
  USB_HOST_DBG("  Current HPRT: 0x%08lX", *(__IO uint32_t *)((uint32_t)USB_OTG_HS + 0x440));
  
  // Step 6: Configure interrupt mask - reuse HAL's configuration
  USB_HOST_DBG("6. Checking interrupt mask...");
  
  // Ensure port interrupt and disconnect interrupt are enabled
  if ((USB_OTG_HS->GINTMSK & USB_OTG_GINTMSK_PRTIM) == 0 ||
      (USB_OTG_HS->GINTMSK & USB_OTG_GINTMSK_DISCINT) == 0) {
    
    USB_HOST_DBG("  Updating interrupt mask");
    // Save current mask, add critical interrupts
    uint32_t current_mask = USB_OTG_HS->GINTMSK;
    current_mask |= USB_OTG_GINTMSK_PRTIM;   // Port interrupt
    current_mask |= USB_OTG_GINTMSK_DISCINT; // Disconnect interrupt
    current_mask |= (1 << 24);               // Host channel interrupt (HCIM)
    USB_OTG_HS->GINTMSK = current_mask;
  }
  
  USB_HOST_DBG("  Current GINTMSK: 0x%08lX", USB_OTG_HS->GINTMSK);
  
  // Step 7: Ensure global interrupt is enabled
  USB_HOST_DBG("7. Ensuring global interrupt enable...");
  USB_OTG_HS->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
  USB_HOST_DBG("  Current GAHBCFG: 0x%08lX", USB_OTG_HS->GAHBCFG);
  
  // Step 8: Clear any pending interrupts
  USB_OTG_HS->GINTSTS = 0xFFFFFFFF;
  
  // Enable NVIC interrupt
  HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
  
  // Step 9: Final check
  USB_HOST_DBG("8. Final register status:");
  USB_HOST_DBG("  GUSBCFG: 0x%08lX", USB_OTG_HS->GUSBCFG);
  USB_HOST_DBG("  GINTMSK: 0x%08lX", USB_OTG_HS->GINTMSK);
  USB_HOST_DBG("  GAHBCFG: 0x%08lX", USB_OTG_HS->GAHBCFG);
  USB_HOST_DBG("  HPRT: 0x%08lX", *(__IO uint32_t *)((uint32_t)USB_OTG_HS + 0x440));
  
  USB_HOST_DBG("USB OTG HS initialization completed, ready for TinyUSB initialization");
  USB_HOST_DBG("\r\nImportant Notes:");
  USB_HOST_DBG("1. Next call tusb_init()");
  USB_HOST_DBG("2. Then call tuh_task() in the main loop");
  USB_HOST_DBG("========== Initialization Complete ==========\r\n");
}

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
