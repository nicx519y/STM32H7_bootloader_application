/*
******************************************************************************
**

**  File        : LinkerScript.ld
**
**  Author		: STM32CubeMX
**
**  Abstract    : Linker script for STM32H750XBHx series
**                128Kbytes FLASH and 1056Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed "as is," without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** <h2><center>&copy; COPYRIGHT(c) 2019 STMicroelectronics</center></h2>
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**   1. Redistributions of source code must retain the above copyright notice,
**      this list of conditions and the following disclaimer.
**   2. Redistributions in binary form must reproduce the above copyright notice,
**      this list of conditions and the following disclaimer in the documentation
**      and/or other materials provided with the distribution.
**   3. Neither the name of STMicroelectronics nor the names of its contributors
**      may be used to endorse or promote products derived from this software
**      without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);    /* end of DTCMRAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x400;      /* 1KB 堆 */
_Min_Stack_Size = 0x200;     /* 512B 栈 */

/* 双槽升级方案地址定义 */
APPLICATION_SLOT_A_ADDR = 0x90010000;           /* Application Slot A (1MB) - 偏移64KB给升级元数据 */
APPLICATION_SLOT_B_ADDR = 0x90110000;           /* Application Slot B (1MB) */

/* Specify the memory areas */
MEMORY
{
ITCMRAM (xrw)                 : ORIGIN = 0x00000000, LENGTH = 64K
DTCMRAM (xrw)                 : ORIGIN = 0x20000000, LENGTH = 128K
RAM (xrw)                     : ORIGIN = 0x24000000, LENGTH = 512K
RAM_D2 (xrw)                  : ORIGIN = 0x30000000, LENGTH = 288K
RAM_D3 (xrw)                  : ORIGIN = 0x38000000, LENGTH = 64K
/* Application使用连续的1MB空间 */
FLASH (rx)                    : ORIGIN = APPLICATION_SLOT_A_ADDR, LENGTH = 1024K
}

/* Define output sections */
SECTIONS
{
  /* 向量表 - 放在FLASH的开头，运行时复制到ITCMRAM */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* 代码段 - 存储在FLASH，运行时复制到RAM */
  .text :
  {
    . = ALIGN(4);
    *(.text)           
    *(.text.*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
  } >FLASH

  /* 常量数据 - 存储在FLASH，运行时复制到RAM */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH
  
  /* ARM exception tables */
  .ARM.extab :
  {
    __extab_start = .;
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    __extab_end = .;
  } >FLASH

  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* 初始化数据，运行时从FLASH复制到RAM */
  _sidata = LOADADDR(.data);
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >FLASH

  /* 定义运行时复制所需的符号 */
  _sivector = ORIGIN(FLASH);                                  /* 向量表在FLASH中的位置 */
  _evector = _sivector + SIZEOF(.isr_vector);                 /* 向量表结束位置 */
  _sitext = _evector;                                         /* 代码段在FLASH中的位置 */  
  _etext = _sitext + SIZEOF(.text);                           /* 代码段结束位置 */
  _sirodata = _etext;                                         /* 只读数据在FLASH中的位置 */
  _erodata = _sirodata + SIZEOF(.rodata);                     /* 只读数据结束位置 */
  
  /* 运行时的目标地址（VMA） */
  _vector_start = ORIGIN(ITCMRAM);                           /* 向量表运行地址 */
  _text_start = ORIGIN(RAM);                                 /* 代码段运行地址 */
  _rodata_start = _text_start + SIZEOF(.text);               /* 只读数据运行地址 */
  
  /* 为启动代码提供运行时地址符号 */
  _stext = _text_start;                                      /* 代码段运行时起始地址 */
  _srodata = _rodata_start;                                  /* 只读数据运行时起始地址 */
  
  /* 未初始化数据段 - 在RAM中，无需从FLASH加载 */
  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* DMA相关段 */
  ._RAM_D1_Area (NOLOAD) :
  {
    . = ALIGN(32);
    *(.DMA_Section)
    *(.DMA_Section*)
    . = ALIGN(32);
  } >RAM

  /* User_heap_stack section */
  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );       
    PROVIDE ( _end = . );       
    . = . + _Min_Heap_Size;     
    . = . + _Min_Stack_Size;    
    . = ALIGN(8);
  } >DTCMRAM

  ._RAM_D2_Area (NOLOAD) :
  {
      . = ALIGN(32);
      *(.DMA_Section)         /* RAM_D2 area section */
      *(.DMA_Section*)        /* RAM_D2 area section */
      . = ALIGN(32);
  } >RAM_D2

  ._RAM_D3_Area (NOLOAD) :
  {
      . = ALIGN(32);
      *(.BDMA_Section)         /* RAM_D3 area section */
      *(.BDMA_Section*)        /* RAM_D3 area section */
      . = ALIGN(32);
  } >RAM_D3

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }

  /* 提供双槽地址给应用程序使用 */
  PROVIDE(APPLICATION_SLOT_A_START = APPLICATION_SLOT_A_ADDR);
  PROVIDE(APPLICATION_SLOT_B_START = APPLICATION_SLOT_B_ADDR);
}


