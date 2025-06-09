#!/usr/bin/env python3
"""
STM32H750 开发调试脚本
功能：
1. 快速编译和烧录
2. 开发模式调试
3. 串口监控
"""

import os
import sys
import subprocess
import time
import threading
import serial
from pathlib import Path

class DevDebugger:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.bootloader_dir = self.project_root / "bootloader"
        self.application_dir = self.project_root / "application"
        self.cpu_count = self.get_cpu_count()
        
    def get_cpu_count(self):
        """获取CPU核心数用于并行编译"""
        try:
            import multiprocessing
            return multiprocessing.cpu_count()
        except:
            return 4  # 默认使用4线程
        
    def quick_build_all(self):
        """快速编译所有项目"""
        print("🔨 快速编译所有项目...")
        print(f"🔥 使用 {self.cpu_count} 线程并行编译")
        
        # 编译bootloader
        print("编译Bootloader...")
        result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.bootloader_dir, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"❌ Bootloader编译失败:\n{result.stderr}")
            return False
        print("✅ Bootloader编译成功")
        
        # 编译application
        print("编译Application...")
        result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.application_dir, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"❌ Application编译失败:\n{result.stderr}")
            return False
        print("✅ Application编译成功")
        
        return True
        
    def quick_flash_bootloader(self):
        """快速烧录bootloader（开发模式）"""
        print("🔥 快速烧录Bootloader...")
        
        if not self.quick_build_all():
            return False
            
        hex_file = self.bootloader_dir / "build" / "bootloader.hex"
        if not hex_file.exists():
            print(f"❌ 找不到hex文件: {hex_file}")
            return False
            
        # 使用OpenOCD快速烧录，转换路径为字符串并使用正斜杠
        hex_file_str = str(hex_file).replace('\\', '/')
        cmd = [
            "openocd",
            "-f", "interface/stlink.cfg",
            "-f", "target/stm32h7x.cfg",
            "-c", f"program \"{hex_file_str}\" verify reset exit"
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"❌ 烧录失败:\n{result.stderr}")
            return False
            
        print("✅ Bootloader烧录成功")
        return True
        
    def quick_flash_application(self):
        """快速烧录application到槽位A"""
        print("🔥 快速烧录Application...")
        
        bin_file = self.application_dir / "build" / "application.bin"
        if not bin_file.exists():
            print("先编译Application...")
            result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.application_dir)
            if result.returncode != 0:
                print("❌ Application编译失败")
                return False
                
        # 使用OpenOCD烧录到QSPI Flash，转换路径为字符串并使用正斜杠
        bin_file_str = str(bin_file).replace('\\', '/')
        cmd = [
            "openocd",
            "-f", "interface/stlink.cfg", 
            "-f", "target/stm32h7x.cfg",
            "-c", "init",
            "-c", "reset halt",
            "-c", f"flash write_image \"{bin_file_str}\" 0x90010000",
            "-c", "reset run",
            "-c", "exit"
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"❌ 烧录失败:\n{result.stderr}")
            return False
            
        print("✅ Application烧录成功")
        return True
        
    def monitor_serial(self, port="COM3", baudrate=115200):
        """串口监控"""
        print(f"📡 开始串口监控 {port}@{baudrate}...")
        print("按Ctrl+C退出监控")
        
        try:
            ser = serial.Serial(port, baudrate, timeout=1)
            while True:
                if ser.in_waiting:
                    data = ser.readline().decode('utf-8', errors='ignore').strip()
                    if data:
                        timestamp = time.strftime("%H:%M:%S")
                        print(f"[{timestamp}] {data}")
                time.sleep(0.01)
        except KeyboardInterrupt:
            print("\n串口监控已停止")
        except Exception as e:
            print(f"❌ 串口错误: {e}")
            
    def create_ram_debug_version(self):
        """创建RAM调试版本（用于GDB调试）"""
        print("🔧 创建RAM调试版本...")
        
        # 修改链接脚本，将Application放到RAM
        linker_script = self.application_dir / "STM32H750VBTx_FLASH.ld"
        
        # 备份原始链接脚本
        backup_script = self.application_dir / "STM32H750VBTx_FLASH_backup.ld"
        if not backup_script.exists():
            subprocess.run(["cp", str(linker_script), str(backup_script)])
            
        # 创建RAM版本的链接脚本
        ram_script = self.application_dir / "STM32H750VBTx_RAM.ld"
        
        ram_content = '''/* RAM调试版本链接脚本 */
MEMORY
{
  RAM (xrw)      : ORIGIN = 0x24000000, LENGTH = 512K
  ITCMRAM (xrw)  : ORIGIN = 0x00000000, LENGTH = 64K
  DTCMRAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 128K
}

/* 入口点 */
ENTRY(Reset_Handler)

/* 最高地址的用户模式堆栈 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 生成一个链接错误，如果没有为堆栈定义足够的RAM */
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

/* 定义段 */
SECTIONS
{
  /* 向量表和启动代码 */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >RAM

  /* 程序代码和其他只读数据 */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >RAM

  /* 常量数据 */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >RAM

  /* 已初始化数据段 */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM

  /* 未初始化数据段 */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM

  /* 用户堆栈 */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM
}
'''
        
        with open(ram_script, 'w') as f:
            f.write(ram_content)
            
        print(f"✅ RAM调试链接脚本已创建: {ram_script}")
        print("使用方法:")
        print(f"1. 修改Makefile，使用{ram_script}")
        print("2. 编译后可直接使用GDB调试")
        print("3. 不需要烧录到Flash，直接加载到RAM")
        
    def gdb_debug_session(self):
        """启动GDB调试会话"""
        print("🐛 启动GDB调试会话...")
        
        elf_file = self.application_dir / "build" / "application.elf"
        if not elf_file.exists():
            print("❌ 找不到ELF文件，请先编译")
            return False
            
        # 创建GDB脚本
        gdb_script = self.application_dir / "debug.gdb"
        gdb_content = f'''
# GDB调试脚本
target extended-remote localhost:3333
file {elf_file}
load
monitor reset halt
break main
continue
'''
        
        with open(gdb_script, 'w') as f:
            f.write(gdb_content)
            
        print("GDB调试脚本已创建")
        print("1. 先启动OpenOCD:")
        print("   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg")
        print("2. 然后启动GDB:")
        print(f"   arm-none-eabi-gdb -x {gdb_script}")
        
    def watch_and_rebuild(self):
        """监控文件变化并自动重新编译"""
        print("👀 开始监控文件变化...")
        print("修改源文件后会自动重新编译")
        print("按Ctrl+C停止监控")
        
        import watchdog.observers
        import watchdog.events
        
        class ChangeHandler(watchdog.events.FileSystemEventHandler):
            def __init__(self, debugger):
                self.debugger = debugger
                self.last_build_time = 0
                
            def on_modified(self, event):
                if event.is_directory:
                    return
                    
                # 只监控.c和.h文件
                if not (event.src_path.endswith('.c') or event.src_path.endswith('.h')):
                    return
                    
                # 防止频繁编译
                current_time = time.time()
                if current_time - self.last_build_time < 2:
                    return
                    
                print(f"检测到文件变化: {event.src_path}")
                if self.debugger.quick_build_all():
                    print("✅ 自动编译成功")
                else:
                    print("❌ 自动编译失败")
                    
                self.last_build_time = current_time
                
        observer = watchdog.observers.Observer()
        handler = ChangeHandler(self)
        
        # 监控源码目录
        observer.schedule(handler, str(self.application_dir / "Core"), recursive=True)
        observer.schedule(handler, str(self.bootloader_dir / "Core"), recursive=True)
        
        observer.start()
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            observer.stop()
            print("\n文件监控已停止")
        observer.join()

def main():
    if len(sys.argv) < 2:
        print("用法: python dev_debug.py <command> [args...]")
        print("命令:")
        print("  build         - 快速编译所有项目")
        print("  flash-boot    - 快速烧录bootloader")
        print("  flash-app     - 快速烧录application")
        print("  monitor [port] - 串口监控 (默认COM3)")
        print("  ram-debug     - 创建RAM调试版本")
        print("  gdb           - 启动GDB调试")
        print("  watch         - 监控文件变化自动编译")
        return
        
    project_root = Path(__file__).parent.parent
    debugger = DevDebugger(project_root)
    
    command = sys.argv[1]
    
    if command == "build":
        debugger.quick_build_all()
    elif command == "flash-boot":
        debugger.quick_flash_bootloader()
    elif command == "flash-app":
        debugger.quick_flash_application()
    elif command == "monitor":
        port = sys.argv[2] if len(sys.argv) > 2 else "COM3"
        debugger.monitor_serial(port)
    elif command == "ram-debug":
        debugger.create_ram_debug_version()
    elif command == "gdb":
        debugger.gdb_debug_session()
    elif command == "watch":
        debugger.watch_and_rebuild()
    else:
        print(f"未知命令: {command}")

if __name__ == "__main__":
    main() 