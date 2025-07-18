#include "leds/leds_manager.hpp"
#include <algorithm>
#include "board_cfg.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef LEDS_ANIMATION_CYCLE
#define LEDS_ANIMATION_CYCLE 10000  // 10秒周期
#endif

LEDsManager::LEDsManager()
{
    opts = &STORAGE_MANAGER.getDefaultGamepadProfile()->ledsConfigs;

    const bool* enabledKeys = STORAGE_MANAGER.getDefaultGamepadProfile()->keysConfig.keysEnableTag;
    // 初始化启用按键掩码
    enabledKeysMask = 0;
    for(uint8_t i = 0; i < 32; i++) {
        if(i < NUM_ADC_BUTTONS) {
            enabledKeysMask |= (enabledKeys[i] ? (1 << i) : 0);
        } else {
            enabledKeysMask |= (1 << i);
        }
    }

    usingTemporaryConfig = false;
    animationStartTime = 0;
    lastButtonState = 0;
    rippleCount = 0;
    for (int i = 0; i < 5; i++) {
        ripples[i].centerIndex = 0;
        ripples[i].startTime = 0;
    }
    
#if HAS_LED_AROUND

    aroundLedAnimationStartTime = 0;
    aroundLedRippleCount = 0;
    for (int i = 0; i < 5; i++) {
        aroundLedRipples[i].centerIndex = 0;
        aroundLedRipples[i].startTime = 0;
    }
    
    // 初始化震荡动画状态
    lastQuakeTriggerTime = 0;
    lastButtonPressTime = 0;
#endif
};

void LEDsManager::setup()
{
    WS2812B_Init();

    WS2812B_SetAllLEDBrightness(0);
    WS2812B_SetAllLEDColor(0, 0, 0);

    if(!opts->ledEnabled) {
        WS2812B_Stop();
    } else {
        WS2812B_Start();
        // 设置初始亮度
        setLedsBrightness(opts->ledBrightness);
        // 更新颜色配置
        updateColorsFromConfig();

        // 初始化动画
        animationStartTime = HAL_GetTick();

        // 对于静态效果，直接设置颜色
        if (opts->ledEffect == LEDEffect::STATIC) {
            WS2812B_SetAllLEDColor(backgroundColor1.r, backgroundColor1.g, backgroundColor1.b);
            WS2812B_SetAllLEDBrightness(opts->ledBrightness);
        }

    #if HAS_LED_AROUND
        // 初始化环绕灯
        aroundLedAnimationStartTime = HAL_GetTick();
        
        // 根据配置设置环绕灯
        if (!opts->aroundLedEnabled) {
            // 环绕灯关闭，设置为黑色
            for (uint8_t i = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i < NUM_LED; i++) {
                WS2812B_SetLEDColor(0, 0, 0, i);
            }
            setAmbientLightBrightness(0);
        } else {
            // 环绕灯开启，设置初始状态
            if (opts->aroundLedEffect == AroundLEDEffect::AROUND_STATIC) {
                RGBColor aroundColor = hexToRGB(opts->aroundLedColor1);
                for (uint8_t i = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i < NUM_LED; i++) {
                    WS2812B_SetLEDColor(aroundColor.r, aroundColor.g, aroundColor.b, i);
                }
                setAmbientLightBrightness(opts->aroundLedBrightness);
            }
        }
    #endif

    }

}

/**
 * @brief 更新LED效果
 * @param virtualPinMask 按钮虚拟引脚掩码 表示按钮状态
 */
void LEDsManager::loop(uint32_t virtualPinMask)
{
    if(!opts->ledEnabled) {
        return;
    }

    // 处理按钮按下事件（用于涟漪效果）
    processButtonPress(virtualPinMask);
    
    // 更新涟漪状态
    updateRipples();
    
    // 获取动画进度
    float progress = getAnimationProgress();
    
    // 获取当前动画算法
    LedAnimationAlgorithm algorithm = getLedAnimation(opts->ledEffect);
    
    // 准备全局动画参数
    LedAnimationParams params;
    params.colorEnabled = true;
    params.frontColor = frontColor;
    params.backColor1 = backgroundColor1;
    params.backColor2 = backgroundColor2;
    params.defaultBackColor = defaultBackColor;
    params.effectStyle = opts->ledEffect;
    params.animationSpeed = opts->ledAnimationSpeed;
    params.progress = progress;
    
    // 设置涟漪参数
    params.global.rippleCount = rippleCount;
    for (uint8_t i = 0; i < rippleCount && i < 5; i++) {
        params.global.rippleCenters[i] = ripples[i].centerIndex;
        uint32_t now = HAL_GetTick();
        uint32_t elapsed = now - ripples[i].startTime;
        // 涟漪持续时间根据动画速度调整（与 TypeScript 版本保持一致）
        const uint32_t rippleDuration = 3000 / opts->ledAnimationSpeed; // 毫秒
        params.global.rippleProgress[i] = (float)elapsed / rippleDuration;
        if (params.global.rippleProgress[i] > 1.0f) {
            params.global.rippleProgress[i] = 1.0f;
        }
    }
    
#if HAS_LED_AROUND
    // 设置环绕灯同步模式参数
    params.global.aroundLedSyncMode = opts->aroundLedEnabled && opts->aroundLedSyncToMainLed;
    
    // 环绕灯处理
    if (!opts->aroundLedEnabled) {
        // 模式1：环绕灯关闭 - 设置为黑色，亮度为0
        for (uint8_t i = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i < NUM_LED; i++) {
            WS2812B_SetLEDColor(0, 0, 0, i);
        }
        setAmbientLightBrightness(0);
    } else if (opts->aroundLedSyncToMainLed) {
        // 模式2：环绕灯同步到主LED - 使用主LED配置和动画
        
        // 在同步模式下，动画算法需要处理所有LED（主LED + 环绕LED）
        // 为每个环绕LED计算颜色并设置（索引从按钮LED数量开始）
        for (uint8_t i = 0; i < NUM_LED_AROUND; i++) {
            params.index = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS) + i; // 环绕LED在全局数组中的索引
            params.pressed = false; // 环绕LED没有按钮状态
            
            RGBColor color = algorithm(params);
            WS2812B_SetLEDColor(color.r, color.g, color.b, (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS) + i);
        }
        setAmbientLightBrightness(opts->aroundLedBrightness);
    } else {
        // 模式3：环绕灯独立模式 - 使用环绕灯独立配置
        processAroundLedAnimation();
    }
#endif
    
    // 为每个主LED（按钮LED）计算颜色并设置
    for (uint8_t i = 0; i < (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i++) {
        params.index = i;
        params.pressed = (virtualPinMask & (1 << i)) != 0;
        
        RGBColor color = algorithm(params);
        WS2812B_SetLEDColor(color.r, color.g, color.b, i);
    }
}

void LEDsManager::processButtonPress(uint32_t virtualPinMask)
{
    // 检测新按下的按钮（用于涟漪效果）
    uint32_t newPressed = virtualPinMask & ~lastButtonState;
    
    if (newPressed != 0 && opts->ledEffect == LEDEffect::RIPPLE) {
        // 查找按下的按钮并添加涟漪
        for (uint8_t i = 0; i < (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i++) {
            if (newPressed & (1 << i)) {
                // 添加新的涟漪
                if (rippleCount < 5) {
                    ripples[rippleCount].centerIndex = i;
                    ripples[rippleCount].startTime = HAL_GetTick();
                    rippleCount++;
                } else {
                    // 如果已满，替换最老的涟漪
                    ripples[0].centerIndex = i;
                    ripples[0].startTime = HAL_GetTick();
                }
                break;
            }
        }
    }
    
#if HAS_LED_AROUND
    // 检测按键按下用于环绕灯动画触发
    if (newPressed != 0 && opts->aroundLedEnabled && !opts->aroundLedSyncToMainLed && 
        opts->aroundLedTriggerByButton) {
        // 任何按键按下时重置环绕灯动画
        uint32_t currentTime = HAL_GetTick();
        aroundLedAnimationStartTime = currentTime; // 重置动画时间
        
        // 为所有环绕灯效果设置触发时间（统一处理）
        lastQuakeTriggerTime = currentTime;  // 用于震荡动画
        lastButtonPressTime = currentTime;   // 通用按键触发时间
        
        APP_DBG("AROUND_LED: Button pressed, restarting around LED animation (effect: %d) at time %lu", 
                opts->aroundLedEffect, currentTime);
    }
#endif
    
    lastButtonState = virtualPinMask;
}

void LEDsManager::updateRipples()
{
    if (opts->ledEffect != LEDEffect::RIPPLE) {
        rippleCount = 0;
        return;
    }
    
    uint32_t now = HAL_GetTick();
    // 涟漪持续时间根据动画速度调整（与 TypeScript 版本保持一致）
    const uint32_t rippleDuration = 3000 / opts->ledAnimationSpeed; // 毫秒
    
    // 移除过期的涟漪
    uint8_t newCount = 0;
    for (uint8_t i = 0; i < rippleCount; i++) {
        if ((now - ripples[i].startTime) < rippleDuration) {
            if (newCount != i) {
                ripples[newCount] = ripples[i];
            }
            newCount++;
        }
    }
    rippleCount = newCount;
}

float LEDsManager::getAnimationProgress()
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - animationStartTime;
    
    // 应用动画速度倍数
    float speedMultiplier = (float)opts->ledAnimationSpeed;
    float progress = (float)(elapsed % LEDS_ANIMATION_CYCLE) / LEDS_ANIMATION_CYCLE * speedMultiplier;
    
    // 确保进度值在 0.0-1.0 范围内循环
    progress = fmodf(progress, 1.0f);
    
    return progress;
}

void LEDsManager::deinit()
{
    HAL_Delay(50); // 等待最后一帧发送完成
    WS2812B_Stop();
}

void LEDsManager::effectStyleNext() {
    opts->ledEffect = static_cast<LEDEffect>((opts->ledEffect + 1) % LEDEffect::NUM_EFFECTS);
    
    // 只有在使用默认配置时才保存到存储
    if (!usingTemporaryConfig) {
        STORAGE_MANAGER.saveConfig();
    }
    
    deinit();
    setup();
}

void LEDsManager::effectStylePrev() {
    opts->ledEffect = static_cast<LEDEffect>((opts->ledEffect - 1 + LEDEffect::NUM_EFFECTS) % LEDEffect::NUM_EFFECTS);
    
    // 只有在使用默认配置时才保存到存储
    if (!usingTemporaryConfig) {
        STORAGE_MANAGER.saveConfig();
    }
    
    deinit();
    setup();
}   

void LEDsManager::brightnessUp() {
    if(opts->ledBrightness == 100) {
        return;
    } else {
        opts->ledBrightness = std::min((int)opts->ledBrightness + 20, 100); // 增加10%
        setLedsBrightness(opts->ledBrightness);
        // 只有在使用默认配置时才保存到存储
        if (!usingTemporaryConfig) {
            STORAGE_MANAGER.saveConfig();
            HAL_Delay(20); // 等待保存完成
        }
        
        
    }
}

void LEDsManager::brightnessDown() {
    if(opts->ledBrightness == 0) {
        return;
    } else {
        opts->ledBrightness = std::max((int)opts->ledBrightness - 20, 0); // 减少10%
        setLedsBrightness(opts->ledBrightness);
        // 只有在使用默认配置时才保存到存储
        if (!usingTemporaryConfig) {
            STORAGE_MANAGER.saveConfig();
            HAL_Delay(20); // 等待保存完成
        }
        
        
    }
}

void LEDsManager::enableSwitch() {
    opts->ledEnabled = !opts->ledEnabled;
    
    // 只有在使用默认配置时才保存到存储
    if (!usingTemporaryConfig) {
        STORAGE_MANAGER.saveConfig();
    }
    
    deinit();
    setup();
}

void LEDsManager::setLedsBrightness(uint8_t brightness) {
    const uint8_t b = (uint8_t)((float_t)(brightness) * LEDS_BRIGHTNESS_RATIO * 255.0 / 100.0);
    WS2812B_SetLEDBrightnessByMask(b, 0, enabledKeysMask); // 根据启用按键掩码设置亮度，未启用的按键亮度为0
}

void LEDsManager::setAmbientLightBrightness(uint8_t brightness) {
    const uint8_t b = (uint8_t)((float_t)(brightness) * LEDS_BRIGHTNESS_RATIO * 255.0 / 100.0);
    WS2812B_SetLEDBrightness(b, NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS, NUM_LED - (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS));
}

// /**
//  * @brief 测试特定的LED动画效果
//  * @param effect 要测试的动画效果
//  * @param progress 动画进度 (0.0-1.0)
//  * @param buttonMask 模拟的按钮状态掩码
//  */
// void LEDsManager::testAnimation(LEDEffect effect, float progress, uint32_t buttonMask)
// {
//     if(!opts->ledEnabled) {
//         APP_DBG("LEDsManager::testAnimation - LED disabled");
//         return;
//     }

//     // 获取测试动画算法
//     LedAnimationAlgorithm algorithm = getLedAnimation(effect);
    
//     // 准备测试参数
//     LedAnimationParams params;
//     params.colorEnabled = true;
//     params.frontColor = frontColor;
//     params.backColor1 = backgroundColor1;
//     params.backColor2 = backgroundColor2;
//     params.defaultBackColor = defaultBackColor;
//     params.effectStyle = effect;
//     params.animationSpeed = opts->ledAnimationSpeed;
//     params.progress = progress;
    
//     // 如果是涟漪效果，设置一个测试涟漪
//     if (effect == LEDEffect::RIPPLE) {
//         params.global.rippleCount = 1;
//         params.global.rippleCenters[0] = 10; // 中心按钮索引10
//         params.global.rippleProgress[0] = progress;
//     } else {
//         params.global.rippleCount = 0;
//     }
    
//     // 为每个LED计算颜色并设置
//     for (uint8_t i = 0; i < (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i++) {
//         params.index = i;
//         params.pressed = (buttonMask & (1 << i)) != 0;
        
//         RGBColor color = algorithm(params);
        
//         WS2812B_SetLEDColor(color.r, color.g, color.b, i);
//     }
// }

// /**
//  * @brief 预览动画效果的完整周期
//  * @param effect 要预览的动画效果
//  * @param duration 预览持续时间(毫秒)
//  */
// void LEDsManager::previewAnimation(LEDEffect effect, uint32_t duration)
// {
//     if(!opts->ledEnabled) {
//         return;
//     }
    
//     uint32_t startTime = HAL_GetTick();
//     uint32_t buttonMask = 0; // 可以设置一些按钮状态用于测试
    
//     // 为涟漪效果设置一些测试按钮
//     if (effect == LEDEffect::RIPPLE) {
//         buttonMask = 0x04; // 按钮2按下，用于触发涟漪
//     }
    
//     while ((HAL_GetTick() - startTime) < duration) {
//         uint32_t elapsed = HAL_GetTick() - startTime;
//         float progress = (float)(elapsed % LEDS_ANIMATION_CYCLE) / LEDS_ANIMATION_CYCLE; // 使用相同的10秒周期
        
//         testAnimation(effect, progress, buttonMask);
//         HAL_Delay(16); // ~60FPS
        
//         // 为涟漪效果在中途改变按钮状态
//         if (effect == LEDEffect::RIPPLE && elapsed > duration / 3 && elapsed < duration / 3 + 100) {
//             buttonMask = 0x100; // 按钮8按下，创建新的涟漪
//         } else if (effect == LEDEffect::RIPPLE && elapsed > duration / 3 + 100) {
//             buttonMask = 0; // 释放按钮
//         }
//     }
// }

/**
 * @brief 设置临时配置进行预览
 * @param tempConfig 临时LED配置
 * @param enabledKeysMask 启用按键掩码
 */
void LEDsManager::setTemporaryConfig(const LEDProfile& tempConfig, uint32_t enabledKeysMask)
{
    temporaryConfig = tempConfig;
    opts = &temporaryConfig;
    usingTemporaryConfig = true;
    this->enabledKeysMask = enabledKeysMask;

    // 重新初始化以应用新配置
    deinit();
    setup();
}

/**
 * @brief 恢复使用默认存储配置
 */
void LEDsManager::restoreDefaultConfig()
{
    if (usingTemporaryConfig) {
        usingTemporaryConfig = false;
        opts = &STORAGE_MANAGER.getDefaultGamepadProfile()->ledsConfigs;
        
        // 重新初始化以应用默认配置
        deinit();
        setup();
    }
}

/**
 * @brief 检查是否正在使用临时配置
 * @return true 如果正在使用临时配置
 */
bool LEDsManager::isUsingTemporaryConfig() const
{
    return usingTemporaryConfig;
}

/**
 * @brief 从当前配置更新颜色值
 */
void LEDsManager::updateColorsFromConfig()
{
    frontColor = hexToRGB(opts->ledColor1);
    backgroundColor1 = hexToRGB(opts->ledColor2);
    backgroundColor2 = hexToRGB(opts->ledColor3);
    defaultBackColor = {0, 0, 0}; // 黑色作为默认背景色
}

#if HAS_LED_AROUND
/**
 * @brief 处理环绕灯独立动画
 */
void LEDsManager::processAroundLedAnimation()
{
    float progress = getAroundLedAnimationProgress();
    
    switch (opts->aroundLedEffect) {
        case AroundLEDEffect::AROUND_STATIC:
            {
                // 静态效果：显示固定颜色（不受触发模式影响）
                RGBColor color = hexToRGB(opts->aroundLedColor1);
                
                for (uint8_t i = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i < NUM_LED; i++) {
                    WS2812B_SetLEDColor(color.r, color.g, color.b, i);
                }
                setAmbientLightBrightness(opts->aroundLedBrightness);
            }
            break;
        
        case AroundLEDEffect::AROUND_BREATHING:
            {
                for (uint8_t i = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i < NUM_LED; i++) {
                    // 计算相对于环绕灯起始位置的索引
                    uint8_t aroundIndex = i - (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS);
                    
                    RGBColor color = aroundLedBreathingAnimation(progress, aroundIndex, 
                                                              opts->aroundLedColor1, 
                                                              opts->aroundLedColor2, 
                                                              opts->aroundLedAnimationSpeed,
                                                              0); // triggerTime参数已废弃，传入0
                    
                    WS2812B_SetLEDColor(color.r, color.g, color.b, i);
                }
                setAmbientLightBrightness(opts->aroundLedBrightness);
            }
            break;
            
        case AroundLEDEffect::AROUND_QUAKE:
            {
                for (uint8_t i = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i < NUM_LED; i++) {
                    // 计算相对于环绕灯起始位置的索引
                    uint8_t aroundIndex = i - (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS);
                    
                    RGBColor color = aroundLedQuakeAnimation(progress, aroundIndex, 
                                                           opts->aroundLedColor1, 
                                                           opts->aroundLedColor2, 
                                                           opts->aroundLedAnimationSpeed,
                                                           0); // triggerTime参数已废弃，传入0
                    
                    WS2812B_SetLEDColor(color.r, color.g, color.b, i);
                }
                setAmbientLightBrightness(opts->aroundLedBrightness);
            }
            break;
            
        case AroundLEDEffect::AROUND_METEOR:
            {
                for (uint8_t i = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i < NUM_LED; i++) {
                    // 计算相对于环绕灯起始位置的索引
                    uint8_t aroundIndex = i - (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS);
                    
                    RGBColor color = aroundLedMeteorAnimation(progress, aroundIndex, 
                                                            opts->aroundLedColor1, 
                                                            opts->aroundLedColor2, 
                                                            opts->aroundLedAnimationSpeed,
                                                            0); // triggerTime参数已废弃，传入0
                    
                    WS2812B_SetLEDColor(color.r, color.g, color.b, i);
                }
                setAmbientLightBrightness(opts->aroundLedBrightness);   
            }
            break;
            
        default:
            // 默认情况：关闭环绕灯
            for (uint8_t i = (NUM_ADC_BUTTONS + NUM_GPIO_BUTTONS); i < NUM_LED; i++) {
                WS2812B_SetLEDColor(0, 0, 0, i);
            }
            setAmbientLightBrightness(0);
            break;
    }
}

/**
 * @brief 获取环绕灯动画进度
 * @return 动画进度 (0.0-1.0)
 */
float LEDsManager::getAroundLedAnimationProgress()
{
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - aroundLedAnimationStartTime;
    uint32_t animationDuration = static_cast<uint32_t>(600.0f * (7 - opts->aroundLedAnimationSpeed));
    
    if(opts->aroundLedEffect == AroundLEDEffect::AROUND_QUAKE) {
        animationDuration /= 2;
    }

    if (opts->aroundLedTriggerByButton) {
        // 按钮触发模式：动画持续一个周期后停止
        if (elapsed >= animationDuration) {
            // 动画周期结束，返回1.0（完成状态，显示静止状态）
            return 1.0f;
        } else {
            // 动画进行中，返回当前进度
            return (float)elapsed / (float)animationDuration;
        }
    } else {
        // 循环模式：连续循环动画
        uint32_t cycleTime = elapsed % animationDuration;
        return (float)cycleTime / (float)animationDuration;
    }
}

/**
 * @brief 更新环绕灯颜色（用于外部调用）
 */
void LEDsManager::updateAroundLedColors()
{
    if (!opts->aroundLedEnabled) {
        return;
    }
    
    if (opts->aroundLedSyncToMainLed) {
        // 同步模式下在主循环中处理，这里不需要做任何事
        return;
    }
    
    // 独立模式下更新环绕灯
    processAroundLedAnimation();
}

#if HAS_LED_AROUND

/**
 * @brief 切换到下一个环绕灯效果
 */
void LEDsManager::ambientLightEffectStyleNext() {
    opts->aroundLedEffect = static_cast<AroundLEDEffect>((opts->aroundLedEffect + 1) % AroundLEDEffect::NUM_AROUND_LED_EFFECTS);
    
    // 只有在使用默认配置时才保存到存储
    if (!usingTemporaryConfig) {
        STORAGE_MANAGER.saveConfig();
    }
    
    deinit();
    setup();
}

/**
 * @brief 切换到上一个环绕灯效果
 */
void LEDsManager::ambientLightEffectStylePrev() {
    opts->aroundLedEffect = static_cast<AroundLEDEffect>((opts->aroundLedEffect - 1 + AroundLEDEffect::NUM_AROUND_LED_EFFECTS) % AroundLEDEffect::NUM_AROUND_LED_EFFECTS);
    
    // 只有在使用默认配置时才保存到存储
    if (!usingTemporaryConfig) {
        STORAGE_MANAGER.saveConfig();
    }
    
    deinit();
    setup();
}

/**
 * @brief 增加环绕灯亮度
 */
void LEDsManager::ambientLightBrightnessUp() {
    if(opts->aroundLedBrightness == 100) {
        return;
    } else {
        opts->aroundLedBrightness = std::min((int)opts->aroundLedBrightness + 20, 100); // 增加10%
        setAmbientLightBrightness(opts->aroundLedBrightness);

        // 只有在使用默认配置时才保存到存储
        if (!usingTemporaryConfig) {
            STORAGE_MANAGER.saveConfig();
            HAL_Delay(20); // 等待保存完成
        }
    }
}

/**
 * @brief 减少环绕灯亮度
 */
void LEDsManager::ambientLightBrightnessDown() {
    if(opts->aroundLedBrightness == 0) {
        return;
    } else {
        opts->aroundLedBrightness = std::max((int)opts->aroundLedBrightness - 20, 0); // 减少10%
        setAmbientLightBrightness(opts->aroundLedBrightness);
        // 只有在使用默认配置时才保存到存储
        if (!usingTemporaryConfig) {
            STORAGE_MANAGER.saveConfig();
            HAL_Delay(20); // 等待保存完成
        }
        
    }
}

/**
 * @brief 切换环绕灯开关状态
 */
void LEDsManager::ambientLightEnableSwitch() {
    opts->aroundLedEnabled = !opts->aroundLedEnabled;
    
    // 只有在使用默认配置时才保存到存储
    if (!usingTemporaryConfig) {
        STORAGE_MANAGER.saveConfig();
    }
    
    deinit();
    setup();
}

#endif

#endif




