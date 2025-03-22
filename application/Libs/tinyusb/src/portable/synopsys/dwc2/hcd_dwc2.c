/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if CFG_TUH_ENABLED && (CFG_TUSB_MCU == OPT_MCU_STM32H7)

#include "host/hcd.h"
#include "stm32h7xx.h"
#include "usb_otg.h"

// 外部声明
extern HCD_HandleTypeDef hhcd_USB_OTG_HS;

// DWC2 控制器类型
typedef struct {
  USB_OTG_GlobalTypeDef* global_base;
  uint32_t hprt_base;
  HCD_HandleTypeDef* hhcd;
  bool connected;
  bool port_enabled;
  tusb_speed_t speed;
} dwc2_controller_t;

// 根据rhport获取控制器
static dwc2_controller_t* get_controller(uint8_t rhport) {
  static dwc2_controller_t controllers[2] = {0};
  
  // STM32H750有两个USB端口，rhport=1对应OTG_HS
  if (rhport == 1) {
    if (controllers[1].global_base == NULL) {
      controllers[1].global_base = USB_OTG_HS;
      controllers[1].hprt_base = (uint32_t)USB_OTG_HS + 0x440;
      controllers[1].hhcd = &hhcd_USB_OTG_HS;
      controllers[1].connected = false;
      controllers[1].port_enabled = false;
      controllers[1].speed = TUSB_SPEED_FULL;
    }
    return &controllers[1];
  }
  
  return NULL;
}

// 添加这些定义
#define EP_TYPE_CTRL 0
#define EP_TYPE_ISOC 1
#define EP_TYPE_BULK 2
#define EP_TYPE_INTR 3

#define TOGGLE_0 0
#define TOGGLE_1 1

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// 可选的HCD配置，由tuh_configure()调用
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void* cfg_param) {
  (void) rhport;
  (void) cfg_id;
  (void) cfg_param;

  return true;
}

// 初始化控制器为主机模式
bool hcd_init(uint8_t rhport) {
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return false;
  
  // STM32 HAL库已经完成了大部分初始化，我们只需补充一些关键设置
  
  // 确保全局中断使能
  controller->global_base->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
  
  // 强制主机模式
  controller->global_base->GUSBCFG |= USB_OTG_GUSBCFG_FHMOD;
  controller->global_base->GUSBCFG &= ~USB_OTG_GUSBCFG_FDMOD;
  
  // 设置中断掩码：至少需要端口中断和断开连接中断
  controller->global_base->GINTMSK |= USB_OTG_GINTMSK_PRTIM | USB_OTG_GINTMSK_DISCINT;
  
  // 启用端口电源
  uint32_t hprt = *(__IO uint32_t*)controller->hprt_base;
  uint32_t new_hprt = hprt & ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET | USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);
  new_hprt |= USB_OTG_HPRT_PPWR;
  *(__IO uint32_t*)controller->hprt_base = new_hprt;
  
  // 初始设置为未连接状态
  controller->connected = false;
  controller->port_enabled = false;
  
  return true;
}

// 中断处理程序
void hcd_int_handler(uint8_t rhport, bool in_isr) {
  (void) in_isr;
  
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return;
  
  USB_OTG_GlobalTypeDef* USBx = controller->global_base;
  uint32_t gintsts = USBx->GINTSTS & USBx->GINTMSK;
  
  // 处理端口中断
  if (gintsts & USB_OTG_GINTSTS_HPRTINT) {
    uint32_t hprt = *(__IO uint32_t*)controller->hprt_base;
    
    // 检查端口连接状态变化
    if (hprt & USB_OTG_HPRT_PCDET) {
      bool connected = (hprt & USB_OTG_HPRT_PCSTS) ? true : false;
      
      // 清除连接检测标志位
      uint32_t new_hprt = hprt & ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET | USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);
      new_hprt |= USB_OTG_HPRT_PCDET;
      *(__IO uint32_t*)controller->hprt_base = new_hprt;
      
      if (connected != controller->connected) {
        controller->connected = connected;
        
        if (connected) {
          // 检测设备速度
          uint32_t speed_bits = (hprt & USB_OTG_HPRT_PSPD) >> USB_OTG_HPRT_PSPD_Pos;
          switch (speed_bits) {
            case 0: controller->speed = TUSB_SPEED_HIGH; break;
            case 1: controller->speed = TUSB_SPEED_FULL; break;
            case 2: controller->speed = TUSB_SPEED_LOW; break;
            default: controller->speed = TUSB_SPEED_FULL; break;
          }
          
          // 启动端口复位
          hcd_port_reset(rhport);
        } else {
          // 设备断开连接
          if (controller->port_enabled) {
            controller->port_enabled = false;
            hcd_event_device_remove(rhport, true);
          }
        }
      }
    }
    
    // 检查端口使能状态变化
    if (hprt & USB_OTG_HPRT_PENCHNG) {
      bool enabled = (hprt & USB_OTG_HPRT_PENA) ? true : false;
      
      // 清除端口使能变化标志位
      uint32_t new_hprt = hprt & ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET | USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);
      new_hprt |= USB_OTG_HPRT_PENCHNG;
      *(__IO uint32_t*)controller->hprt_base = new_hprt;
      
      if (enabled && controller->connected && !controller->port_enabled) {
        controller->port_enabled = true;
        
        // 通知上层设备已连接
        hcd_event_device_attach(rhport, true);
      } else if (!enabled && controller->port_enabled) {
        controller->port_enabled = false;
      }
    }
  }
  
  // 处理设备断开连接中断
  if (gintsts & USB_OTG_GINTSTS_DISCINT) {
    // 清除断开连接中断标志
    USBx->GINTSTS = USB_OTG_GINTSTS_DISCINT;
    
    if (controller->connected) {
      controller->connected = false;
      controller->port_enabled = false;
      
      // 通知上层设备已移除
      hcd_event_device_remove(rhport, true);
    }
  }
}

// 使能USB中断
void hcd_int_enable(uint8_t rhport) {
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return;
  
  // STM32 HAL库已配置中断，只需确保全局中断已使能
  controller->global_base->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
}

// 禁用USB中断
void hcd_int_disable(uint8_t rhport) {
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return;
  
  controller->global_base->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;
}

// 获取帧号 (1ms)
uint32_t hcd_frame_number(uint8_t rhport) {
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return 0;
  
  // STM32H7中HFNUM是直接访问寄存器，不是结构体成员
  return (*(volatile uint32_t*)((uint32_t)controller->global_base + 0x408) & 0xFFFF);
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// 获取根集线器端口的当前连接状态
bool hcd_port_connect_status(uint8_t rhport) {
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return false;
  
  uint32_t hprt = *(__IO uint32_t*)controller->hprt_base;
  return (hprt & USB_OTG_HPRT_PCSTS) ? true : false;
}

// 在端口上重置USB总线
void hcd_port_reset(uint8_t rhport) {
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return;
  
  uint32_t hprt = *(__IO uint32_t*)controller->hprt_base;
  uint32_t new_hprt = hprt & ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET | USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);
  new_hprt |= USB_OTG_HPRT_PRST;
  *(__IO uint32_t*)controller->hprt_base = new_hprt;
  
  // 重置至少需要10ms
  HAL_Delay(20);
  
  // 结束复位
  hcd_port_reset_end(rhport);
}

// 完成总线复位序列
void hcd_port_reset_end(uint8_t rhport) {
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return;
  
  uint32_t hprt = *(__IO uint32_t*)controller->hprt_base;
  uint32_t new_hprt = hprt & ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET | USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG | USB_OTG_HPRT_PRST);
  *(__IO uint32_t*)controller->hprt_base = new_hprt;
}

// 获取端口链接速度
tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
  dwc2_controller_t* controller = get_controller(rhport);
  if (!controller) return TUSB_SPEED_FULL;
  
  return controller->speed;
}

// HCD关闭此设备所有已打开的端点
void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
  (void) rhport;
  (void) dev_addr;
  
  // STM32 HAL库已经处理这部分逻辑
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

// 打开端点
bool hcd_edpt_open(uint8_t rhport, uint8_t dev_addr, tusb_desc_endpoint_t const * ep_desc) {
  (void) rhport;
  
  uint8_t const epnum = ep_desc->bEndpointAddress & 0x7F;
  uint8_t const dir = (ep_desc->bEndpointAddress & 0x80) ? 1 : 0;
  uint8_t const xfer_type = ep_desc->bmAttributes.xfer;
  uint16_t const max_packet_size = ep_desc->wMaxPacketSize;
  
  // 调用HAL库函数打开端点 - 调整参数匹配HAL库定义
  if (HAL_HCD_HC_Init(&hhcd_USB_OTG_HS, epnum, dev_addr, 
                      dir,
                      EP_TYPE_CTRL,
                      max_packet_size,
                      0) != HAL_OK) {
    return false;
  }
  
  return true;
}

// 提交一个传输，完成时必须调用hcd_event_xfer_complete()
bool hcd_edpt_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
  (void) rhport;
  
  uint8_t const epnum = ep_addr & 0x7F;
  uint8_t const dir = (ep_addr & 0x80) ? 1 : 0;
  
  // 调整参数匹配HAL库定义
  if (HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_HS, epnum, dir, 
                              EP_TYPE_CTRL, 
                              TOGGLE_0, 
                              buffer, 
                              buflen,
                              dev_addr) != HAL_OK) {
    return false;
  }
  
  return true;
}

// 终止排队传输
bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  
  uint8_t ch_num = ep_addr & 0x7F;
  
  if (HAL_HCD_HC_Halt(&hhcd_USB_OTG_HS, ch_num) != HAL_OK) {
    return false;
  }
  
  return true;
}

// 提交一个特殊传输来发送8字节设置包
bool hcd_setup_send(uint8_t rhport, uint8_t dev_addr, uint8_t const setup_packet[8]) {
  (void) rhport;
  
  // 调整参数匹配HAL库定义
  if (HAL_HCD_HC_SubmitRequest(&hhcd_USB_OTG_HS, 0, 0, 
                              EP_TYPE_CTRL, 
                              TOGGLE_0, 
                              (uint8_t*)setup_packet, 
                              8,
                              dev_addr) != HAL_OK) {
    return false;
  }
  
  return true;
}

// 清除停止，数据切换也重置为DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
  (void) rhport;
  (void) dev_addr;
  
  uint8_t const epnum = ep_addr & 0x7F;
  
  if (HAL_HCD_HC_Halt(&hhcd_USB_OTG_HS, epnum) != HAL_OK) {
    return false;
  }
  
  return true;
}

#endif