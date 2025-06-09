#!/usr/bin/env python3
"""
STM32H750 开发调试脚本
功能：
1. 快速编译和烧录
2. 创建调试环境
3. 支持不同的调试模式
"""

import os
import sys
import subprocess
import struct
from pathlib import Path

class DebugHelper:
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
        
    def quick_flash_bootloader(self):
        """快速烧录bootloader"""
        print("🔥 快速烧录Bootloader...")
        print(f"🔥 使用 {self.cpu_count} 线程并行编译")
        
        # 编译bootloader
        result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.bootloader_dir, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"❌ Bootloader编译失败:\n{result.stderr}")
            return False
            
        # 烧录到内部Flash
        result = subprocess.run([
            "openocd", 
            "-f", "Openocd_Script/ST-LINK-FLASH.cfg",
            "-c", "program build/bootloader.elf verify reset exit"
        ], cwd=self.bootloader_dir)
        
        if result.returncode == 0:
            print("✅ Bootloader烧录成功")
            return True
        else:
            print("❌ Bootloader烧录失败")
            return False
            
    def create_minimal_qspi_image(self):
        """创建最小QSPI镜像（仅包含application）"""
        print("💾 创建最小QSPI镜像...")
        print(f"🔥 使用 {self.cpu_count} 线程并行编译")
        
        # 编译application
        result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.application_dir, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"❌ Application编译失败:\n{result.stderr}")
            return None
            
        # 读取application数据
        app_bin = self.application_dir / "build" / "application.bin"
        app_data = app_bin.read_bytes()
        
        # 创建最小镜像（只需要包含metadata和application）
        image_size = 2 * 1024 * 1024  # 2MB足够调试使用
        qspi_image = bytearray(image_size)
        
        # 创建简单的升级元数据
        import zlib
        app_crc = zlib.crc32(app_data) & 0xffffffff
        
        metadata = struct.pack('<I', 0x55AA55AA)  # magic
        metadata += struct.pack('<I', 0x00000001)  # version
        metadata += struct.pack('<B', 0)  # active_slot (Slot A)
        metadata += struct.pack('<B', 0)  # upgrade_slot (Slot A)
        metadata += struct.pack('<B', 3)  # upgrade_type (APPLICATION_ONLY)
        metadata += struct.pack('<B', 8)  # upgrade_status (SUCCESS)
        metadata += struct.pack('<I', 0)  # upgrade_attempts
        metadata += struct.pack('<I', 3)  # max_attempts
        metadata += struct.pack('<I', 0)  # timestamp
        
        # Application组件信息
        metadata += struct.pack('<I', len(app_data))  # size
        metadata += struct.pack('<I', 100)  # version (debug)
        metadata += struct.pack('<B', 1)  # checksum_type (CRC32)
        metadata += struct.pack('<I', app_crc)  # checksum
        metadata += b'\x00' * 32  # hash
        metadata += struct.pack('<B', 8)  # status (SUCCESS)
        metadata += b'\x00' * 3  # padding
        
        # 其他组件信息为空
        for _ in range(3):
            metadata += b'\x00' * 48
        metadata += b'\x00' * 64  # reserved
        
        # 计算元数据CRC32
        metadata_crc = zlib.crc32(metadata) & 0xffffffff
        metadata += struct.pack('<I', metadata_crc)
        
        # 写入元数据
        qspi_image[0:len(metadata)] = metadata
        
        # 写入Application到Slot A (偏移0x10000)
        slot_a_offset = 0x10000
        qspi_image[slot_a_offset:slot_a_offset + len(app_data)] = app_data
        
        # 保存镜像
        debug_image = self.application_dir / "debug_qspi.bin"
        debug_image.write_bytes(bytes(qspi_image))
        print(f"✅ 调试镜像已创建: {debug_image}")
        
        return debug_image
        
    def quick_flash_application(self):
        """快速烧录application"""
        print("🔥 快速烧录Application...")
        
        debug_image = self.create_minimal_qspi_image()
        if not debug_image:
            return False
            
        # 烧录到QSPI Flash
        result = subprocess.run([
            "openocd",
            "-f", "Openocd_Script/ST-LINK-QSPIFLASH.cfg", 
            "-c", f"program {debug_image} 0x90000000 verify exit"
        ], cwd=self.application_dir)
        
        if result.returncode == 0:
            print("✅ Application烧录成功")
            return True
        else:
            print("❌ Application烧录失败")
            return False
            
    def full_debug_flash(self):
        """完整调试烧录（bootloader + application）"""
        print("🚀 完整调试烧录...")
        
        success = True
        success &= self.quick_flash_bootloader()
        success &= self.quick_flash_application()
        
        if success:
            print("✅ 完整调试烧录成功！请重启设备。")
        else:
            print("❌ 调试烧录失败！")
            
        return success
        
    def create_debug_commands(self):
        """创建调试命令脚本"""
        
        # Windows批处理文件
        debug_bat = self.project_root / "debug.bat"
        debug_bat.write_text(f"""@echo off
echo STM32H750 调试工具

:menu
echo.
echo 请选择操作:
echo 1. 编译并烧录Bootloader
echo 2. 编译并烧录Application  
echo 3. 完整烧录 (Bootloader + Application)
echo 4. 仅编译Application
echo 5. 仅编译Bootloader
echo 0. 退出

set /p choice=请输入选择 (0-5): 

if "%choice%"=="1" goto flash_bootloader
if "%choice%"=="2" goto flash_application
if "%choice%"=="3" goto full_flash
if "%choice%"=="4" goto build_app
if "%choice%"=="5" goto build_boot
if "%choice%"=="0" goto exit
goto menu

:flash_bootloader
python debug_scripts/debug_setup.py flash-bootloader
pause
goto menu

:flash_application  
python debug_scripts/debug_setup.py flash-application
pause
goto menu

:full_flash
python debug_scripts/debug_setup.py full-flash
pause
goto menu

:build_app
cd application
make -j{self.cpu_count}
cd ..
pause
goto menu

:build_boot
cd bootloader  
make -j{self.cpu_count}
cd ..
pause
goto menu

:exit
""")
        
        # Linux shell脚本
        debug_sh = self.project_root / "debug.sh"
        debug_sh.write_text(f"""#!/bin/bash
# STM32H750 调试工具

while true; do
    echo
    echo "STM32H750 调试工具"
    echo "=================="
    echo "1. 编译并烧录Bootloader"
    echo "2. 编译并烧录Application"  
    echo "3. 完整烧录 (Bootloader + Application)"
    echo "4. 仅编译Application"
    echo "5. 仅编译Bootloader"
    echo "0. 退出"
    echo
    
    read -p "请输入选择 (0-5): " choice
    
    case $choice in
        1)
            python3 debug_scripts/debug_setup.py flash-bootloader
            read -p "按回车键继续..."
            ;;
        2)
            python3 debug_scripts/debug_setup.py flash-application
            read -p "按回车键继续..."
            ;;
        3)
            python3 debug_scripts/debug_setup.py full-flash
            read -p "按回车键继续..."
            ;;
        4)
            cd application && make -j{self.cpu_count} && cd ..
            read -p "按回车键继续..."
            ;;
        5)
            cd bootloader && make -j{self.cpu_count} && cd ..
            read -p "按回车键继续..."
            ;;
        0)
            echo "退出"
            break
            ;;
        *)
            echo "无效选择，请重新输入"
            ;;
    esac
done
""")
        debug_sh.chmod(0o755)
        
        print("✅ 调试脚本已创建:")
        print(f"   - Windows: {debug_bat}")
        print(f"   - Linux: {debug_sh}")

if __name__ == "__main__":
    project_root = Path(__file__).parent.parent
    helper = DebugHelper(project_root)
    
    if len(sys.argv) < 2:
        print("STM32H750 调试工具")
        print("==================")
        print("可用命令:")
        print("  flash-bootloader    - 快速烧录bootloader")
        print("  flash-application   - 快速烧录application")
        print("  full-flash         - 完整烧录")
        print("  create-scripts     - 创建调试脚本")
        print()
        print("示例: python debug_setup.py full-flash")
        
        # 自动创建调试脚本
        helper.create_debug_commands()
        sys.exit(1)
        
    command = sys.argv[1]
    
    if command == "flash-bootloader":
        helper.quick_flash_bootloader()
    elif command == "flash-application":
        helper.quick_flash_application()
    elif command == "full-flash":
        helper.full_debug_flash()
    elif command == "create-scripts":
        helper.create_debug_commands()
    else:
        print(f"未知命令: {command}") 