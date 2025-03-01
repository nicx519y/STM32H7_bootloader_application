/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

#include "main.h"
#include "usb_otg_hs.h"
#include "usart.h"
#include "qspi-w25q64.h"
#include "board_cfg.h"

HCD_HandleTypeDef hhcd_USB_OTG_HS;

void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
bool readMetadata(void);
void copyApplicationToRAM(void);
void JumpToApplication(void);

// metadata.bin 的 Flash 地址
#define METADATA_ADDR 0x00000000
#define MAX_METADATA_SIZE 8192  // 最大元数据大小，根据实际情况调整
#define SECTION_NAME_LENGTH 32  // 段名固定长度，与 Python 脚本保持一致

// 结构体定义
typedef struct {
    uint32_t num_sections;  // 段数量
} MetadataHeader;

typedef struct {
    char name[SECTION_NAME_LENGTH]; // 固定长度段名
    uint32_t size;                  // 段大小
    uint32_t vma;                   // 虚拟内存地址
    uint32_t lma;                   // 加载内存地址
} SectionInfo;

// 全局变量，存储元数据信息
static uint8_t g_metadata_buffer[MAX_METADATA_SIZE];
static uint32_t g_num_sections = 0;
static bool g_metadata_loaded = false;


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  MPU_Config();

  HAL_Init();

  USART1_Init();
  QSPI_W25Qxx_Init();
  BOOT_DBG("QSPI_W25Qxx_Init success\r\n");

  copyApplicationToRAM();
  JumpToApplication();
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
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

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* 配置 QSPI Flash 区域 */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x90000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_8MB;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;  // 必须显式允许执行
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

void JumpToApplication(void)
{
    // 确保元数据已加载
    if (!g_metadata_loaded) {
        if (!readMetadata()) {
            BOOT_ERR("Failed to read metadata, cannot jump to application");
            return;
        }
    }
    
    // 在元数据中查找中断向量表段的VMA地址
    uint32_t vector_table_addr = 0;
    uint32_t offset = sizeof(MetadataHeader);
    
    for (uint32_t i = 0; i < g_num_sections; i++) {
        SectionInfo* section = (SectionInfo*)(g_metadata_buffer + offset);
        
        char name[SECTION_NAME_LENGTH + 1];
        memcpy(name, section->name, SECTION_NAME_LENGTH);
        name[SECTION_NAME_LENGTH] = '\0';
        
        if (strcmp(name, ".isr_vector") == 0) {
            vector_table_addr = section->vma;
            BOOT_DBG("Found .isr_vector section at VMA: 0x%08lX", vector_table_addr);
            break;
        }
        
        offset += sizeof(SectionInfo);
    }
    
    if (vector_table_addr == 0) {
        BOOT_ERR("Could not find .isr_vector section in metadata");
        return;
    }

    // 打印中断向量表 前16个地址的值
    BOOT_DBG("ISR Vector Table:");
    for(int i = 0; i < 16; i++) {
        BOOT_DBG("  Address 0x%08X: 0x%08X", vector_table_addr + i * 4, *(__IO uint32_t*)(vector_table_addr + i * 4));
    }
    
    // 从中断向量表获取应用程序堆栈指针和入口点
    uint32_t app_stack = *(__IO uint32_t*)vector_table_addr;
    uint32_t jump_address = *(__IO uint32_t*)(vector_table_addr + 4);
    
    BOOT_DBG("App Stack address: 0x%08X, App Stack value: 0x%08X", vector_table_addr, app_stack);
    BOOT_DBG("Jump Address: 0x%08X, Jump Address value: 0x%08X", vector_table_addr + 4, jump_address);

    // 验证栈指针
    if ((app_stack & 0xFF000000) != 0x20000000) {
        BOOT_ERR("Invalid stack pointer: 0x%08X", app_stack);
        return;
    } else {
        BOOT_DBG("Valid stack pointer: 0x%08X", app_stack);
    }

    // 验证入口点地址
    if ((jump_address & 0xFF000000) != 0x24000000) {
        BOOT_ERR("Invalid jump address: 0x%08X", jump_address);
        return;
    } else {
        BOOT_DBG("Valid jump address: 0x%08X", jump_address);
    }

    // 验证目标地址的指令内容
    uint16_t* code_ptr = (uint16_t*)(jump_address & ~1UL);
    BOOT_DBG("First instructions at target:");
    for(int i = 0; i < 4; i++) {
        BOOT_DBG("  Instruction %d: 0x%04X", i, code_ptr[i]);
    }

    /****************************  跳转前准备  ************************* */
    SysTick->CTRL = 0;                          // 关闭SysTick
    SysTick->LOAD = 0;                          // 清零重载
    SysTick->VAL = 0;                           // 清零计数

    __set_CONTROL(0); // priviage mode 
    __disable_irq();  // disable interrupt
    __set_PRIMASK(1);

    // 关闭所有中断
    __disable_irq();
    BOOT_DBG("Interrupts disabled");

    // 清除所有中断
    for(int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    BOOT_DBG("NVIC cleared");

    HAL_MPU_Disable();

    // 设置向量表为应用程序的向量表地址
    SCB->VTOR = vector_table_addr;
    BOOT_DBG("VTOR set to: 0x%08X", SCB->VTOR);
    
    // 验证向量表设置是否生效
    BOOT_DBG("SCB->VTOR after set: 0x%08X", SCB->VTOR);
    BOOT_DBG("Stack Pointer from vector: 0x%08X", *(__IO uint32_t*)SCB->VTOR);
    BOOT_DBG("Reset Handler from vector: 0x%08X", *(__IO uint32_t*)(SCB->VTOR + 4));

    // 设置主堆栈指针
    __set_MSP(app_stack);
    BOOT_DBG("MSP set to: 0x%08X", __get_MSP());

    // 内存屏障
    __DSB();
    __ISB();
    BOOT_DBG("Memory barriers executed");

    // 确保跳转地址是 Thumb 模式
    jump_address |= 0x1;
    BOOT_DBG("Final jump address (with Thumb bit): 0x%08X", jump_address);

    // 使用函数指针跳转
    typedef void (*pFunction)(void);
    pFunction app_reset_handler = (pFunction)jump_address;

    BOOT_DBG("About to jump...");
    BOOT_DBG("Last debug message before jump!");
    
    // 最后一次内存屏障
    __DSB();
    __ISB();
    
    // 跳转
    app_reset_handler();

    // 不应该到达这里
    BOOT_ERR("Jump failed!");
    while(1);
}

// 读取元数据并存储在全局变量中
bool readMetadata(void)
{
    uint32_t offset = 0;
    
    BOOT_DBG("Reading metadata from address 0x%08X", METADATA_ADDR);
    
    // 读取头部，获取段数量
    QSPI_W25Qxx_ReadBuffer(g_metadata_buffer, METADATA_ADDR, sizeof(MetadataHeader));
    MetadataHeader* header = (MetadataHeader*)g_metadata_buffer;
    
    BOOT_DBG("Header bytes as uint32: 0x%08lX", header->num_sections);
    BOOT_DBG("Found %lu sections in metadata", header->num_sections);
    g_num_sections = header->num_sections;
    
    // 估计元数据大小
    uint32_t estimated_size = sizeof(MetadataHeader) + 
                              g_num_sections * (SECTION_NAME_LENGTH + 3 * sizeof(uint32_t));
    if (estimated_size > MAX_METADATA_SIZE) {
        BOOT_ERR("Estimated metadata size too large: %lu bytes", estimated_size);
        return false;
    }
    
    // 读取整个元数据块
    QSPI_W25Qxx_ReadBuffer(g_metadata_buffer, METADATA_ADDR, estimated_size);
    g_metadata_loaded = true;
    
    // 解析并打印元数据
    offset = sizeof(MetadataHeader);  // 跳过头部
    
    for (uint32_t i = 0; i < g_num_sections; i++) {
        // 从缓冲区获取段信息
        SectionInfo* section = (SectionInfo*)(g_metadata_buffer + offset);
        
        // 确保字符串结束符
        char name[SECTION_NAME_LENGTH + 1];
        memcpy(name, section->name, SECTION_NAME_LENGTH);
        name[SECTION_NAME_LENGTH] = '\0';
        
        // 输出段信息
        BOOT_DBG("Section %lu: %s", i, name);
        BOOT_DBG("  Size: 0x%08lX (%lu bytes)", section->size, section->size);
        BOOT_DBG("  VMA: 0x%08lX", section->vma);
        BOOT_DBG("  LMA: 0x%08lX", section->lma);
        
        // 移动到下一个段
        offset += sizeof(SectionInfo);
    }
    
    BOOT_DBG("Metadata parsing complete");
    return true;
}

// 使用已加载的元数据复制应用到RAM
void copyApplicationToRAM(void)
{
    if (!g_metadata_loaded) {
        if (!readMetadata()) {
            BOOT_ERR("Failed to read metadata");
            return;
        }
    }
    
    BOOT_DBG("Starting application copy to RAM...");
    uint32_t offset = sizeof(MetadataHeader);  // 跳过头部
    
    // 首先找到并处理.bss段（先清零）
    for (uint32_t i = 0; i < g_num_sections; i++) {
        SectionInfo* section = (SectionInfo*)(g_metadata_buffer + offset);
        
        // 确保字符串结束符并比较段名
        char name[SECTION_NAME_LENGTH + 1];
        memcpy(name, section->name, SECTION_NAME_LENGTH);
        name[SECTION_NAME_LENGTH] = '\0';
        
        // 找到.bss段并清零
        if (strcmp(name, ".bss") == 0) {
            BOOT_DBG("Zeroing .bss section at 0x%08lX, size: %lu bytes", section->vma, section->size);
            memset((void*)section->vma, 0, section->size);
        }
        
        offset += sizeof(SectionInfo);
    }
    
    // 重置offset指针，再次遍历段
    offset = sizeof(MetadataHeader);
    
    // 然后复制其他需要的段
    for (uint32_t i = 0; i < g_num_sections; i++) {
        SectionInfo* section = (SectionInfo*)(g_metadata_buffer + offset);
        
        // 确保字符串结束符并比较段名
        char name[SECTION_NAME_LENGTH + 1];
        memcpy(name, section->name, SECTION_NAME_LENGTH);
        name[SECTION_NAME_LENGTH] = '\0';
        
        // 检查是否是需要复制的段
        if (strcmp(name, ".isr_vector") == 0 || 
            strcmp(name, ".text") == 0 || 
            strcmp(name, ".rodata") == 0 || 
            strcmp(name, ".init_array") == 0 || 
            strcmp(name, ".fini_array") == 0 || 
            strcmp(name, ".data") == 0) {
            
            BOOT_DBG("Copying section %s from LMA:0x%08lX to VMA:0x%08lX, size: %lu bytes", 
                    name, section->lma, section->vma, section->size);
            
            // 分配临时缓冲区来存储Flash数据
            uint8_t* buffer = NULL;
            uint32_t buffer_size = 0;
            
            // 确定合适的缓冲区大小，避免分配过大内存
            if (section->size <= 4096) {
                buffer_size = section->size;
            } else {
                buffer_size = 4096; // 使用4KB缓冲区分块复制
            }
            
            buffer = (uint8_t*)malloc(buffer_size);
            if (buffer == NULL) {
                BOOT_ERR("Failed to allocate memory for section copy");
                return;
            }
            
            // 分块复制大段
            uint32_t remaining = section->size;
            uint32_t flash_offset = section->lma & 0x0FFFFFFF; // 去掉Flash基地址高位
            uint32_t ram_offset = section->vma;
            
            while (remaining > 0) {
                uint32_t chunk_size = (remaining > buffer_size) ? buffer_size : remaining;
                
                // 从Flash读取到缓冲区
                QSPI_W25Qxx_ReadBuffer(buffer, flash_offset, chunk_size);
                
                // 复制到RAM
                memcpy((void*)ram_offset, buffer, chunk_size);
                
                // 更新指针和剩余大小
                flash_offset += chunk_size;
                ram_offset += chunk_size;
                remaining -= chunk_size;
            }
            
            free(buffer);
        }
        
        offset += sizeof(SectionInfo);
    }
    
    BOOT_DBG("Application successfully copied to RAM");
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}



#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
