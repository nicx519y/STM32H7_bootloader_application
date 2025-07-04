#include "stm32h7xx_hal.h"
#include "board_cfg.h"
#include "bsp/board_api.h"
#include "qspi-w25q64.h"
#include "usart.h"
#include "usb.h"
#include "adc.h"
#include "dma.h"
#include "bdma.h"
#include "pwm-ws2812b.h"
#include "utils.h"

UART_HandleTypeDef UartHandle;

void SystemClock_Config(void);
void PeriphCommonClock_Config(void);

void board_init(void)
{
    // Implemented in board.h
    SystemClock_Config();
    PeriphCommonClock_Config();

    // Enable All GPIOs clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE(); // USB ULPI NXT
    __HAL_RCC_GPIOC_CLK_ENABLE(); // USB ULPI NXT
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE(); // USB ULPI NXT
#ifdef __HAL_RCC_GPIOI_CLK_ENABLE
    __HAL_RCC_GPIOI_CLK_ENABLE(); // USB ULPI NXT
#endif
    __HAL_RCC_GPIOJ_CLK_ENABLE();

#if CFG_TUSB_OS == OPT_OS_NONE
    // HAL 库会在 HAL_Init() 中自动配置 SysTick
#elif CFG_TUSB_OS == OPT_OS_FREERTOS
    // Explicitly disable systick to prevent its ISR runs before scheduler start
    SysTick->CTRL &= ~1U;

    // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
#ifdef USB_OTG_FS_PERIPH_BASE
    NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif
    NVIC_SetPriority(OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
#endif

    USART1_Init(); // USART for debug
    APP_DBG("board init: USART1_Init success.");

    // 验证时钟配置
    APP_DBG("board init: SYSCLK: %lu", HAL_RCC_GetSysClockFreq());
    APP_DBG("board init: HCLK: %lu", HAL_RCC_GetHCLKFreq());
    APP_DBG("board init: PCLK1: %lu", HAL_RCC_GetPCLK1Freq());
    APP_DBG("board init: PCLK2: %lu", HAL_RCC_GetPCLK2Freq());

    QSPI_W25Qxx_Init(); // 初始化QSPI Flash不执行 因为bootloader已经初始化
    APP_DBG("board init: QSPI_W25Qxx_Init success.");

    // QSPI_W25Qxx_Test(0x00500000);

    // 由于采用了DWT方案做微秒级定时器，所以不需要初始化TIM2
    // MX_TIM2_Init(); // 8000频率定时器 并开启中断模式
    // APP_DBG("board init: MX_TIM2_Init success.");

    USB_clock_init();

    USB_Device_Init();

    /* Initialize USB Host */
    USB_Host_Init();

    APP_DBG("board init: USB_init success.");

    MX_DMA_Init();

    APP_DBG("board init: MX_DMA_Init success.");

    MX_BDMA_Init();

    APP_DBG("board init: MX_BDMA_Init success.");

    MX_ADC1_Init();

    APP_DBG("board init: MX_ADC1_Init success.");

    MX_ADC2_Init();

    APP_DBG("board init: MX_ADC2_Init success.");

    MX_ADC3_Init();

    APP_DBG("board init: MX_ADC3_Init success.");

#ifdef HAS_LED
    WS2812B_Init();
    APP_DBG("board init: WS2812B_Init success.");
#endif // HAS_LED
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{

    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    // 更新 SystemCoreClock 变量
    SystemCoreClockUpdate();

    /** Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /** Configure the main internal regulator output voltage */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    /** Configure LSE Drive Capability */
    HAL_PWR_EnableBkUpAccess();

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 2;
    RCC_OscInitStruct.PLL.PLLN = 80;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }

    

    /** Configure peripheral clock */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_ADC;
    PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
    PeriphClkInitStruct.PLL3.PLL3M = 2;
    PeriphClkInitStruct.PLL3.PLL3N = 15;
    PeriphClkInitStruct.PLL3.PLL3P = 2;
    PeriphClkInitStruct.PLL3.PLL3Q = 4;
    PeriphClkInitStruct.PLL3.PLL3R = 5;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_3;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOMEDIUM;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
    PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL3;  // ADC时钟频率 HSE: 24MHz，ADC时钟频率 = HSE / PLL3.PLL3M * PLL3.PLL3N / PLL3.PLL3R = 24MHz / 2 * 15 / 4 = 45MHz
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Peripherals Common Clock Configuration
 * @retval None
 */
void PeriphCommonClock_Config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /** Initializes the peripherals clock
     */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInitStruct.PLL3.PLL3M = 2;
    PeriphClkInitStruct.PLL3.PLL3N = 15;
    PeriphClkInitStruct.PLL3.PLL3P = 2;
    PeriphClkInitStruct.PLL3.PLL3Q = 4;
    PeriphClkInitStruct.PLL3.PLL3R = 5;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_3;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOMEDIUM;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
    PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL3;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief 获取当前槽的基地址
 * @note 通过检查当前代码运行的地址来判断处于哪个槽
 * @retval 当前槽的基地址 (0x90000000 for 槽A, 0x902B0000 for 槽B)
 */
uint32_t get_current_slot_base_address(void)
{
    // 使用链接器定义的Flash起始地址符号
    extern uint32_t _flash_start;  // 链接器脚本中定义的Flash起始地址
    uint32_t flash_start_address = (uint32_t)&_flash_start;
    
    // 双槽地址范围定义
    #define SLOT_A_BASE         0x90000000   // 槽A起始地址
    #define SLOT_A_END          0x902AFFFF   // 槽A结束地址  
    #define SLOT_B_BASE         0x902B0000   // 槽B起始地址
    #define SLOT_B_END          0x9055FFFF   // 槽B结束地址
    
    // 判断Flash起始地址在哪个槽范围内
    if (flash_start_address >= SLOT_A_BASE && flash_start_address <= SLOT_A_END) {
        // 当前固件在槽A
        APP_DBG("Current slot: A (Flash start: 0x%08X)", flash_start_address);
        return SLOT_A_BASE;
    } else if (flash_start_address >= SLOT_B_BASE && flash_start_address <= SLOT_B_END) {
        // 当前固件在槽B
        APP_DBG("Current slot: B (Flash start: 0x%08X)", flash_start_address);
        return SLOT_B_BASE;
    } else {
        // 未知地址，根据地址值智能判断
        if (flash_start_address == 0x90000000) {
            APP_DBG("Detected Slot A by exact match (0x%08X)", flash_start_address);
            return SLOT_A_BASE;
        } else if (flash_start_address == 0x902B0000) {
            APP_DBG("Detected Slot B by exact match (0x%08X)", flash_start_address);
            return SLOT_B_BASE;
        } else {
            // 如果都不匹配，根据地址大小判断更可能的槽
            if (flash_start_address < 0x90200000) {
                APP_DBG("Flash address 0x%08X < 0x90200000, assuming Slot A", flash_start_address);
                return SLOT_A_BASE;
            } else {
                APP_DBG("Flash address 0x%08X >= 0x90200000, assuming Slot B", flash_start_address);
                return SLOT_B_BASE;
            }
        }
    }
}