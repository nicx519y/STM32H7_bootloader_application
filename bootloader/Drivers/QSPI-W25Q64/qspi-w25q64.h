#ifndef __QSPI_W25Q64_H
#define __QSPI_W25Q64_H

#include <stdbool.h>
#include "stm32h7xx_hal.h"
#include "usart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QSPI_W25Qxx_DEBUG 1

#if QSPI_W25Qxx_DEBUG
    #define QSPI_W25Qxx_DBG(fmt, ...) printf("[QSPI_W25Qxx] " fmt "\r\n", ##__VA_ARGS__)
    #define QSPI_W25Qxx_ERR(fmt, ...) printf("[QSPI_W25Qxx_ERR] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define QSPI_W25Qxx_DBG(fmt, ...) ((void)0)
    #define QSPI_W25Qxx_ERR(fmt, ...) ((void)0)
#endif



/*----------------------------------------------- 命名参数宏 -------------------------------------------*/

#define QSPI_W25Qxx_OK           		0		// W25Qxx通信正常
#define W25Qxx_ERROR_INIT         		-1		// 初始化错误
#define W25Qxx_ERROR_WriteEnable       -2		// 写使能错误
#define W25Qxx_ERROR_AUTOPOLLING       -3		// 轮询等待错误，无响应
#define W25Qxx_ERROR_Erase         		-4		// 擦除错误
#define W25Qxx_ERROR_TRANSMIT         	-5		// 传输错误
#define W25Qxx_ERROR_MemoryMapped		-6    // 内存映射模式错误

#define W25Qxx_CMD_EnableReset  		0x66		// 使能复位
#define W25Qxx_CMD_ResetDevice   	0x99		// 复位器件
#define W25Qxx_CMD_JedecID 			0x9F		// JEDEC ID  
#define W25Qxx_CMD_WriteEnable		0X06		// 写使能

#define W25Qxx_CMD_SectorErase 		0x20		// 扇区擦除，4K字节， 参考擦除时间 45ms
#define W25Qxx_CMD_BlockErase_32K 	0x52		// 块擦除，  32K字节，参考擦除时间 120ms
#define W25Qxx_CMD_BlockErase_64K 	0xD8		// 块擦除，  64K字节，参考擦除时间 150ms
#define W25Qxx_CMD_ChipErase 			0xC7		// 整片擦除，参考擦除时间 20S

#define W25Qxx_CMD_QuadInputPageProgram  	0x32  		// 1-1-4模式下(1线指令1线地址4线数据)，页编程指令，参考写入时间 0.4ms 
#define W25Qxx_CMD_FastReadQuad_IO       	0xEB  		// 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令

#define W25Qxx_CMD_ReadStatus_REG1			0X05			// 读状态寄存器1
#define W25Qxx_Status_REG1_BUSY  			0x01			// 读状态寄存器1的第0位（只读），Busy标志位，当正在擦除/写入数据/写命令时会被置1
#define W25Qxx_Status_REG1_WEL  				0x02			// 读状态寄存器1的第1位（只读），WEL写使能标志位，该标志位为1时，代表可以进行写操作

#define W25Qxx_PageSize       				256			// 页大小，256字节
#define W25Qxx_FlashSize       				0x800000		// W25Q64大小，8M字节
#define W25Qxx_FLASH_ID           			0Xef4017    // W25Q64 JEDEC ID
#define W25Qxx_ChipErase_TIMEOUT_MAX		100000U		// 超时等待时间，W25Q64整片擦除所需最大时间是100S
#define W25Qxx_Mem_Addr							0x90000000 	// 内存映射模式的地址
#define W25Qxx_SECTOR_SIZE     0x1000      // 4KB

#define W25Qxx_CMD_PageProgram           0x02  // 标准页编程指令
#define W25Qxx_CMD_QuadPageProgram       0x32  // 四线页编程指令

// W25Q64 指令集
#define W25Qxx_CMD_FastRead              0x0B    /* Fast Read Data */

// 添加状态寄存器2命令定义
#define W25Qxx_CMD_ReadStatus_REG2    0x35    // 读状态寄存器2
#define W25Qxx_CMD_WriteStatus_REG2   0x31    // 写状态寄存器2
#define W25Qxx_Status_REG2_QE         0x02    // 状态寄存器2的QE位（bit 1）

/*----------------------------------------------- 引脚配置宏 ------------------------------------------*/

#define  QUADSPI_CLK_PIN							GPIO_PIN_10								// QUADSPI_CLK 引脚
#define	QUADSPI_CLK_PORT							GPIOF										// QUADSPI_CLK 引脚端口
#define	QUADSPI_CLK_AF								GPIO_AF9_QUADSPI						// QUADSPI_CLK IO口复用
#define 	GPIO_QUADSPI_CLK_ENABLE      			__HAL_RCC_GPIOF_CLK_ENABLE()	 	// QUADSPI_CLK 引脚时钟

#define  QUADSPI_BK1_NCS_PIN						GPIO_PIN_6								// QUADSPI_BK1_NCS 引脚
#define	QUADSPI_BK1_NCS_PORT						GPIOG										// QUADSPI_BK1_NCS 引脚端口
#define	QUADSPI_BK1_NCS_AF						GPIO_AF10_QUADSPI						// QUADSPI_BK1_NCS IO口复用
#define 	GPIO_QUADSPI_BK1_NCS_ENABLE        	__HAL_RCC_GPIOG_CLK_ENABLE()	 	// QUADSPI_BK1_NCS 引脚时钟

#define  QUADSPI_BK1_IO0_PIN						GPIO_PIN_8								// QUADSPI_BK1_IO0 引脚
#define	QUADSPI_BK1_IO0_PORT						GPIOF										// QUADSPI_BK1_IO0 引脚端口
#define	QUADSPI_BK1_IO0_AF						GPIO_AF10_QUADSPI						// QUADSPI_BK1_IO0 IO口复用
#define 	GPIO_QUADSPI_BK1_IO0_ENABLE        	__HAL_RCC_GPIOF_CLK_ENABLE()	 	// QUADSPI_BK1_IO0 引脚时钟

#define  QUADSPI_BK1_IO1_PIN						GPIO_PIN_9								// QUADSPI_BK1_IO1 引脚
#define	QUADSPI_BK1_IO1_PORT						GPIOF										// QUADSPI_BK1_IO1 引脚端口
#define	QUADSPI_BK1_IO1_AF						GPIO_AF10_QUADSPI						// QUADSPI_BK1_IO1 IO口复用
#define 	GPIO_QUADSPI_BK1_IO1_ENABLE        	__HAL_RCC_GPIOF_CLK_ENABLE()	 	// QUADSPI_BK1_IO1 引脚时钟

#define  QUADSPI_BK1_IO2_PIN						GPIO_PIN_7								// QUADSPI_BK1_IO2 引脚
#define	QUADSPI_BK1_IO2_PORT						GPIOF										// QUADSPI_BK1_IO2 引脚端口
#define	QUADSPI_BK1_IO2_AF						GPIO_AF9_QUADSPI						// QUADSPI_BK1_IO2 IO口复用
#define 	GPIO_QUADSPI_BK1_IO2_ENABLE        	__HAL_RCC_GPIOF_CLK_ENABLE()	 	// QUADSPI_BK1_IO2 引脚时钟

#define  QUADSPI_BK1_IO3_PIN						GPIO_PIN_6								// QUADSPI_BK1_IO3 引脚
#define	QUADSPI_BK1_IO3_PORT						GPIOF										// QUADSPI_BK1_IO3 引脚端口
#define	QUADSPI_BK1_IO3_AF						GPIO_AF9_QUADSPI						// QUADSPI_BK1_IO3 IO口复用
#define 	GPIO_QUADSPI_BK1_IO3_ENABLE      	__HAL_RCC_GPIOF_CLK_ENABLE()	 	// QUADSPI_BK1_IO3 引脚时钟


/*----------------------------------------------- 函数声明 ---------------------------------------------------*/

extern QSPI_HandleTypeDef hqspi;
void MX_QUADSPI_Init(void);
int8_t QSPI_W25Qxx_Init(void);						// W25Qxx初始化
int8_t QSPI_W25Qxx_Reset(void);					// 复位器件
uint32_t QSPI_W25Qxx_ReadID(void);					// 读取器件ID
int8_t QSPI_W25Qxx_MemoryMappedMode(void);		// 进入内存映射模式
	
int8_t QSPI_W25Qxx_SectorErase(uint32_t SectorAddress);		// 扇区擦除，4K字节， 参考擦除时间 45ms
int8_t QSPI_W25Qxx_BlockErase_32K (uint32_t SectorAddress);	// 块擦除，  32K字节，参考擦除时间 120ms
int8_t QSPI_W25Qxx_BlockErase_64K (uint32_t SectorAddress);	// 块擦除，  64K字节，参考擦除时间 150ms，实际使用建议使用64K擦除，擦除的时间最快
int8_t QSPI_W25Qxx_ChipErase (void);                         // 整片擦除，参考擦除时间 20S

int8_t QSPI_W25Qxx_WritePage(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);	// 按页写入，最大256字节

int8_t QSPI_W25Qxx_WriteBuffer(uint8_t* pBuffer, uint32_t WriteAddr, uint32_t NumByteToWrite);				// 写入数据，最大不能超过flash芯片的大小
int8_t QSPI_W25Qxx_ReadBuffer(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead);	// 读取数据，最大不能超过flash芯片的大小

int8_t QSPI_W25Qxx_WriteBuffer_WithXIPOrNot(uint8_t* pData, uint32_t WriteAddr, uint32_t NumByteToWrite);
int8_t QSPI_W25Qxx_ReadBuffer_WithXIPOrNot(uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead);

int8_t QSPI_W25Qxx_WriteString(char* string, uint32_t ReadAddr);
int8_t QSPI_W25Qxx_ReadString(char* buffer, uint32_t ReadAddr);

int8_t QSPI_W25Qxx_Test(uint32_t test_addr);  // 修改测试函数声明，添加地址参数
int8_t QSPI_W25Qxx_BufferErase(uint32_t StartAddr, uint32_t Size);  // 添加缓冲区擦除函数声明

int8_t QSPI_W25Qxx_EnterMemoryMappedMode(void); // 进入内存映射模式
int8_t QSPI_W25Qxx_ExitMemoryMappedMode(void);  // 退出内存映射模式
bool QSPI_W25Qxx_IsMemoryMappedMode(void);       // 判断是否处于内存映射模式

int8_t QSPI_W25Qxx_QuadEnable(void);

#ifdef __cplusplus
}
#endif

#endif /* __QSPI_W25Q64_H */ 





