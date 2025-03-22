#include "input_state.hpp"
#include "adc_btns/adc_btns_worker.hpp"
#include "gpio_btns/gpio_btns_worker.hpp"
#include "gamepad.hpp"
#include "leds/leds_manager.hpp"
#include "hotkeys_manager.hpp"
#include "usb.h" // 包含USB相关函数
#include "usb_otg.h"
#include "tusb.h" // 包含TinyUSB头文件



// USB主机设备连接状态
bool usb_host_device_connected = false;

// USB主机连接事件回调函数
void tuh_mount_cb(uint8_t dev_addr)
{
    APP_DBG("USB Host device connected, address: %d", dev_addr);
    usb_host_device_connected = true;
    
    // 获取设备信息
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    APP_DBG("VID: %04x, PID: %04x", vid, pid);
    
    // 可以在这里添加设备枚举和进一步初始化的代码
}

// USB主机断开事件回调函数
void tuh_umount_cb(uint8_t dev_addr)
{
    APP_DBG("USB Host device disconnected, address: %d", dev_addr);
    usb_host_device_connected = false;
}

void InputState::setup() {
    APP_DBG("InputState::setup");
    
    InputMode inputMode = STORAGE_MANAGER.getInputMode();
    APP_DBG("InputState::setup inputMode: %d", inputMode);
    if(inputMode == InputMode::INPUT_MODE_CONFIG) {
        APP_ERR("InputState::setup error - inputMode: INPUT_MODE_CONFIG, not supported for input state");
        return;
    }
    
    DRIVER_MANAGER.setup(inputMode);
    inputDriver = DRIVER_MANAGER.getDriver();

    // ADC_BTNS_WORKER.setup();
    // GPIO_BTNS_WORKER.setup();
    // GAMEPAD.setup();

    // #if HAS_LED == 1
    // LEDS_MANAGER.setup();
    // #endif

    /*************** 初始化USB start ************ */
    // 初始化HAL USB主机控制器
    APP_DBG("MX_USB_OTG_HS_HCD_Init start");
    MX_USB_OTG_HS_HCD_Init();
    APP_DBG("MX_USB_OTG_HS_HCD_Init done");
    USB_FixInterrupts(); // 修复USB中断
    
    // 诊断USB控制器状态
    USB_DiagnoseInterruptConfig();

    // 初始化TinyUSB设备栈
    APP_DBG("tud_init start");  
    tud_init(TUD_OPT_RHPORT);
    APP_DBG("tud_init done");
    
    // 初始化TinyUSB主机栈
    APP_DBG("tuh_init start");
    tuh_init(TUH_OPT_RHPORT);
    APP_DBG("tuh_init done, USB Host mode activated on port: %d", TUH_OPT_RHPORT);
    
    // 启动USB主机端口供电
    APP_DBG("Starting USB Host port power...");
    if (tuh_inited()) {
        // 如果初始化成功，检查端口电源并开始轮询
        bool rhport_connected = false;
        tuh_task(); // 首次运行任务来检查连接状态
        
        APP_DBG("USB Host initialization successful");
        
        // 可选：强制开启VBUS (如果硬件支持)
        uint32_t hprt = *(__IO uint32_t *)((uint32_t)USB_OTG_HS + 0x440);
        if ((hprt & USB_OTG_HPRT_PPWR) == 0) { // 检查PPWR位
            // 端口电源未开启，尝试手动开启
            uint32_t new_hprt = hprt & ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCA);
            new_hprt |= USB_OTG_HPRT_PPWR; // 设置PPWR位
            *(__IO uint32_t *)((uint32_t)USB_OTG_HS + 0x440) = new_hprt;
            APP_DBG("USB Host port power enabled manually");
        }
    } else {
        APP_ERR("Failed to initialize USB Host");
    }
    
    /**************** 初始化USB end ******************* */

    // workTime = MICROS_TIMER.micros();
    // calibrationTime = MICROS_TIMER.micros();
    // ledAnimationTime = MICROS_TIMER.micros();

    isRunning = true;
}

void InputState::loop() { 

    // if(MICROS_TIMER.checkInterval(READ_BTNS_INTERVAL, workTime)) {

    //     virtualPinMask = GPIO_BTNS_WORKER.read() | ADC_BTNS_WORKER.read();

    //     if(((lastVirtualPinMask & virtualPinMask) == FN_BUTTON_VIRTUAL_PIN) && (lastVirtualPinMask != FN_BUTTON_VIRTUAL_PIN)) { // 如果按下了 FN 键并且不只是 FN 键，并且click了其他键，则执行快捷键
    //         HOTKEYS_MANAGER.runVirtualPinMask(lastVirtualPinMask);
    //     } else { // 否则，处理游戏手柄数据
    //         GAMEPAD.read(virtualPinMask);
    //         inputDriver->process(&GAMEPAD);  // xinput 处理游戏手柄数据，将按键数据映射到xinput协议 形成 report 数据，然后通过 usb 发送出去
    //     }

    //     lastVirtualPinMask = virtualPinMask;
    // }


    // 处理USB任务
    tud_task(); // 设备模式任务
    
    // 处理USB主机任务
    tuh_task(); // 主机模式任务
    
    // 周期性检查并输出USB主机状态
    static uint32_t usb_check_time = 0;
    static bool last_connection_state = false;
    
    if (HAL_GetTick() - usb_check_time > 1000) { // 每秒检查一次
        usb_check_time = HAL_GetTick();
        
        // 检查当前连接状态是否变化
        if (usb_host_device_connected != last_connection_state) {
            if (usb_host_device_connected) {
                APP_DBG("USB Host状态: 设备已连接");
            } else {
                APP_DBG("USB Host状态: 无设备连接");
            }
            last_connection_state = usb_host_device_connected;
        }
        
        // 检查USB主机控制器状态
        uint32_t hprt = *(__IO uint32_t *)((uint32_t)USB_OTG_HS + 0x440);
        bool port_connected = (hprt & USB_OTG_HPRT_PCSTS_Msk) != 0; // PCSTS位
        
        if (port_connected && !usb_host_device_connected) {
            APP_DBG("USB Host端口有连接但TinyUSB未识别到设备，HPRT=0x%08lX", hprt);
        }
    }

    // #if HAS_LED == 1
    // if(MICROS_TIMER.checkInterval(LEDS_ANIMATION_INTERVAL, ledAnimationTime)) {
    //     LEDS_MANAGER.loop(virtualPinMask);
    // }
    // #endif

    #if ENABLED_DYNAMIC_CALIBRATION == 1
    // if(MICROS_TIMER.checkInterval(DYNAMIC_CALIBRATION_INTERVAL, calibrationTime)) {
    //     ADC_BTNS_WORKER.dynamicCalibration();
    // }
    #endif
}

void InputState::reset() {

}
