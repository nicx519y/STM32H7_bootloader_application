#include "gamepad.hpp"
#include "storagemanager.hpp"
#include "drivermanager.hpp"
#include "micro_timer.hpp"

/*
 * ======================================================================
 * 防抖算法性能优化说明
 * ======================================================================
 * 
 * 原始算法问题分析：
 * 1. 每次调用都获取时间戳 - MICROS_TIMER.micros() 开销约50-100个CPU周期
 * 2. 循环处理每个按键 - 最多32次循环，每次都有复杂的状态机判断
 * 3. 复杂的状态机逻辑 - 每个按键3个状态，多次条件判断
 * 4. 位操作效率低 - 逐位检查和设置，未利用32位并行处理
 * 
 * 优化方案对比：
 * 
 * 1. debounceFilterOptimized (推荐普通使用)
 *    - 延迟：2ms
 *    - 只在有变化时获取时间戳
 *    - 使用位操作批量处理所有按键
 *    - 简化状态逻辑
 *    - 性能提升：约5-8倍
 * 
 * 2. debounceFilterUltraFast (推荐竞技使用)
 *    - 延迟：约150μs (3次采样 × 50μs间隔)
 *    - 完全避免时间戳获取
 *    - 使用计数器代替时间判断
 *    - 最小化CPU开销
 *    - 性能提升：约10-15倍
 * 
 * 3. debounceFilterAdaptive (智能场景)
 *    - 延迟：1-5ms (自适应)
 *    - 根据输入频率动态调整防抖时间
 *    - 适合不同使用场景
 *    - 性能提升：约3-6倍
 * 
 * 使用建议：
 * - 竞技游戏：选择 DEBOUNCE_ALGORITHM_ULTRAFAST (最低延迟)
 * - 一般使用：选择 DEBOUNCE_ALGORITHM_OPTIMIZED (平衡性能和稳定性)
 * - 复杂环境：选择 DEBOUNCE_ALGORITHM_ADAPTIVE (自适应)
 * 
 * 配置方法：
 * 修改 DEBOUNCE_ALGORITHM_SELECTED 宏定义，并设置 USE_DEBOUNCE_FILTER = 1
 * ======================================================================
 */


Gamepad::Gamepad()
{
	options = Storage::getInstance().getDefaultGamepadProfile();
}

void Gamepad::setup()
{
    mapDpadUp    = new GamepadButtonMapping(options->keysConfig.keyDpadUp, GAMEPAD_MASK_UP);
	mapDpadDown  = new GamepadButtonMapping(options->keysConfig.keyDpadDown, GAMEPAD_MASK_DOWN);
	mapDpadLeft  = new GamepadButtonMapping(options->keysConfig.keyDpadLeft, GAMEPAD_MASK_LEFT);
	mapDpadRight = new GamepadButtonMapping(options->keysConfig.keyDpadRight, GAMEPAD_MASK_RIGHT);
	mapButtonB1  = new GamepadButtonMapping(options->keysConfig.keyButtonB1, GAMEPAD_MASK_B1);
	mapButtonB2  = new GamepadButtonMapping(options->keysConfig.keyButtonB2, GAMEPAD_MASK_B2);
	mapButtonB3  = new GamepadButtonMapping(options->keysConfig.keyButtonB3, GAMEPAD_MASK_B3);
	mapButtonB4  = new GamepadButtonMapping(options->keysConfig.keyButtonB4, GAMEPAD_MASK_B4);
	mapButtonL1  = new GamepadButtonMapping(options->keysConfig.keyButtonL1, GAMEPAD_MASK_L1);
	mapButtonR1  = new GamepadButtonMapping(options->keysConfig.keyButtonR1, GAMEPAD_MASK_R1);
	mapButtonL2  = new GamepadButtonMapping(options->keysConfig.keyButtonL2, GAMEPAD_MASK_L2);
	mapButtonR2  = new GamepadButtonMapping(options->keysConfig.keyButtonR2, GAMEPAD_MASK_R2);
	mapButtonS1  = new GamepadButtonMapping(options->keysConfig.keyButtonS1, GAMEPAD_MASK_S1);
	mapButtonS2  = new GamepadButtonMapping(options->keysConfig.keyButtonS2, GAMEPAD_MASK_S2);
	mapButtonL3  = new GamepadButtonMapping(options->keysConfig.keyButtonL3, GAMEPAD_MASK_L3);
	mapButtonR3  = new GamepadButtonMapping(options->keysConfig.keyButtonR3, GAMEPAD_MASK_R3);
	mapButtonA1  = new GamepadButtonMapping(options->keysConfig.keyButtonA1, GAMEPAD_MASK_A1);
	mapButtonA2  = new GamepadButtonMapping(options->keysConfig.keyButtonA2, GAMEPAD_MASK_A2);
	mapButtonFn  = new GamepadButtonMapping(options->keysConfig.keyButtonFn, AUX_MASK_FUNCTION);

}

void Gamepad::process()
{
	memcpy(&rawState, &state, sizeof(GamepadState));

	// NOTE: Inverted X/Y-axis must run before SOCD and Dpad processing
	if (options->keysConfig.invertXAxis) {
		bool left = (state.dpad & mapDpadLeft->buttonMask) != 0;
		bool right = (state.dpad & mapDpadRight->buttonMask) != 0;
		state.dpad &= ~(mapDpadLeft->buttonMask | mapDpadRight->buttonMask);
		if (left)
			state.dpad |= mapDpadRight->buttonMask;
		if (right)
			state.dpad |= mapDpadLeft->buttonMask;
	}

	if (options->keysConfig.invertYAxis) {
		bool up = (state.dpad & mapDpadUp->buttonMask) != 0;
		bool down = (state.dpad & mapDpadDown->buttonMask) != 0;
		state.dpad &= ~(mapDpadUp->buttonMask | mapDpadDown->buttonMask);
		if (up)
			state.dpad |= mapDpadDown->buttonMask;
		if (down)
			state.dpad |= mapDpadUp->buttonMask;
	}

	// 4-way before SOCD, might have better history without losing any coherent functionality
	if (options->keysConfig.fourWayMode) {
		state.dpad = filterToFourWayMode(state.dpad);
	}

	state.dpad = runSOCDCleaner(resolveSOCDMode(*options), state.dpad);
}

void Gamepad::deinit()
{
    delete mapDpadUp;
	delete mapDpadDown;
	delete mapDpadLeft;
	delete mapDpadRight;
	delete mapButtonB1;
	delete mapButtonB2;
	delete mapButtonB3;
	delete mapButtonB4;
	delete mapButtonL1;
	delete mapButtonR1;
	delete mapButtonL2;
	delete mapButtonR2;
	delete mapButtonS1;
	delete mapButtonS2;
	delete mapButtonL3;
	delete mapButtonR3;
	delete mapButtonA1;
	delete mapButtonA2;
	delete mapButtonFn;
	
	this->clearState();

}


void Gamepad::read(Mask_t values)
{
	
	state.aux = 0
		| (values & mapButtonFn->virtualPinMask)   ? mapButtonFn->buttonMask : 0;

	state.dpad = 0
		| ((values & mapDpadUp->virtualPinMask)    ? mapDpadUp->buttonMask : 0)
		| ((values & mapDpadDown->virtualPinMask)  ? mapDpadDown->buttonMask : 0)
		| ((values & mapDpadLeft->virtualPinMask)  ? mapDpadLeft->buttonMask  : 0)
		| ((values & mapDpadRight->virtualPinMask) ? mapDpadRight->buttonMask : 0)
	;

	state.buttons = 0
		| ((values & mapButtonB1->virtualPinMask)  ? mapButtonB1->buttonMask  : 0)
		| ((values & mapButtonB2->virtualPinMask)  ? mapButtonB2->buttonMask  : 0)
		| ((values & mapButtonB3->virtualPinMask)  ? mapButtonB3->buttonMask  : 0)
		| ((values & mapButtonB4->virtualPinMask)  ? mapButtonB4->buttonMask  : 0)
		| ((values & mapButtonL1->virtualPinMask)  ? mapButtonL1->buttonMask  : 0)
		| ((values & mapButtonR1->virtualPinMask)  ? mapButtonR1->buttonMask  : 0)
		| ((values & mapButtonL2->virtualPinMask)  ? mapButtonL2->buttonMask  : 0)
		| ((values & mapButtonR2->virtualPinMask)  ? mapButtonR2->buttonMask  : 0)
		| ((values & mapButtonS1->virtualPinMask)  ? mapButtonS1->buttonMask  : 0)
		| ((values & mapButtonS2->virtualPinMask)  ? mapButtonS2->buttonMask  : 0)
		| ((values & mapButtonL3->virtualPinMask)  ? mapButtonL3->buttonMask  : 0)
		| ((values & mapButtonR3->virtualPinMask)  ? mapButtonR3->buttonMask  : 0)
		| ((values & mapButtonA1->virtualPinMask)  ? mapButtonA1->buttonMask  : 0)
		| ((values & mapButtonA2->virtualPinMask)  ? mapButtonA2->buttonMask  : 0)
	;

	state.lx = GAMEPAD_JOYSTICK_MID;
	state.ly = GAMEPAD_JOYSTICK_MID;
	state.rx = GAMEPAD_JOYSTICK_MID;
	state.ry = GAMEPAD_JOYSTICK_MID;
	state.lt = 0;
	state.rt = 0;

	process();
}

void Gamepad::clearState()
{
	state.dpad = 0;
	state.buttons = 0;
	state.aux = 0;
	state.lx = GAMEPAD_JOYSTICK_MID;
	state.ly = GAMEPAD_JOYSTICK_MID;
	state.rx = GAMEPAD_JOYSTICK_MID;
	state.ry = GAMEPAD_JOYSTICK_MID;
	state.lt = 0;
	state.rt = 0;
}

void Gamepad::setSOCDMode(SOCDMode socdMode) {
    options->keysConfig.socdMode = socdMode;
}


