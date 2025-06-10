#!/usr/bin/env python3
"""
STM32H750 系统初始化脚本
功能：
1. 初始化升级元数据
2. 设置默认槽位信息
3. 烧录初始固件
"""

import os
import sys
import subprocess
import struct
from pathlib import Path
import argparse

# QSPI Flash 地址定义
QSPI_BASE = 0x90000000  
METADATA_ADDR = 0x90000000          # 升级元数据区 (64KB)

# 槽位A地址定义
APPLICATION_SLOT_A = 0x90010000     # Application Slot A (1MB)
WEB_RESOURCES_SLOT_A = 0x90110000   # Web Resources Slot A (1.5MB)
CONFIG_SLOT_A = 0x90290000          # Config Slot A (64KB)
ADC_MAPPING_SLOT_A = 0x902A0000     # ADC Mapping Slot A (64KB)

# 槽位B地址定义
APPLICATION_SLOT_B = 0x902B0000     # Application Slot B (1MB)
WEB_RESOURCES_SLOT_B = 0x903B0000   # Web Resources Slot B (1.5MB)
CONFIG_SLOT_B = 0x904B0000          # Config Slot B (64KB)
ADC_MAPPING_SLOT_B = 0x904C0000     # ADC Mapping Slot B (64KB)

# 升级元数据结构
UPGRADE_MAGIC = 0x55AA55AA
COMPONENT_APPLICATION = 1
COMPONENT_WEB_RESOURCES = 2
COMPONENT_CONFIG = 3
COMPONENT_ADC_MAPPING = 4

class SystemInitializer:
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
        
    def create_initial_metadata(self):
        """创建初始升级元数据"""
        print("📝 创建初始升级元数据...")
        
        metadata = bytearray(4096)  # 4KB元数据区
        
        # 升级控制块
        offset = 0
        struct.pack_into('<I', metadata, offset, UPGRADE_MAGIC)        # 魔数
        struct.pack_into('<I', metadata, offset + 4, 0)               # 升级状态：无升级
        struct.pack_into('<I', metadata, offset + 8, 0)               # 重试次数
        struct.pack_into('<I', metadata, offset + 12, 0)              # 升级组件
        
        # 槽位A信息（默认活跃槽位）
        offset = 256
        struct.pack_into('<I', metadata, offset, UPGRADE_MAGIC)        # 魔数
        struct.pack_into('<I', metadata, offset + 4, 1)               # 有效标志
        struct.pack_into('<I', metadata, offset + 8, 0x00010001)      # 版本号 1.0.0.1
        struct.pack_into('<I', metadata, offset + 12, 0)              # CRC32 (待计算)
        struct.pack_into('<I', metadata, offset + 16, 0x100000)       # 大小 1MB
        
        # 槽位B信息（备用槽位）
        offset = 512
        struct.pack_into('<I', metadata, offset, UPGRADE_MAGIC)        # 魔数
        struct.pack_into('<I', metadata, offset + 4, 0)               # 无效标志
        struct.pack_into('<I', metadata, offset + 8, 0)               # 版本号
        struct.pack_into('<I', metadata, offset + 12, 0)              # CRC32
        struct.pack_into('<I', metadata, offset + 16, 0)              # 大小
        
        # 保存到文件
        metadata_file = self.project_root / "metadata_init.bin"
        with open(metadata_file, 'wb') as f:
            f.write(metadata)
            
        print(f"✅ 元数据文件已创建: {metadata_file}")
        return metadata_file
        
    def flash_bootloader(self):
        """烧录Bootloader到内部Flash"""
        print("🔥 烧录Bootloader到内部Flash...")
        
        # 编译bootloader
        result = subprocess.run(["make", "clean"], cwd=self.bootloader_dir)
        result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.bootloader_dir)
        if result.returncode != 0:
            print("❌ Bootloader编译失败")
            return False
            
        # 烧录命令（使用OpenOCD）
        hex_file = self.bootloader_dir / "build" / "bootloader.hex"
        if not hex_file.exists():
            print(f"❌ 找不到hex文件: {hex_file}")
            return False
            
        print(f"烧录文件: {hex_file}")
        # 这里需要根据你的调试器配置OpenOCD命令
        print("请手动执行以下命令烧录Bootloader:")
        print(f"openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c \"program {hex_file} verify reset exit\"")
        return True
        
    def flash_metadata(self, metadata_file):
        """烧录元数据到QSPI Flash"""
        print("🔥 烧录元数据到QSPI Flash...")
        
        print("请手动执行以下命令烧录元数据:")
        print(f"openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \\")
        print(f"  -c \"init\" \\")
        print(f"  -c \"reset halt\" \\") 
        print(f"  -c \"flash write_image {metadata_file} 0x90000000\" \\")
        print(f"  -c \"reset run\" \\")
        print(f"  -c \"exit\"")
        
    def flash_application_to_slot_a(self):
        """烧录Application到槽位A"""
        print("🔥 烧录Application到槽位A...")
        
        # 编译application
        result = subprocess.run(["make", "clean"], cwd=self.application_dir)
        result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.application_dir)
        if result.returncode != 0:
            print("❌ Application编译失败")
            return False
            
        bin_file = self.application_dir / "build" / "application.bin"
        if not bin_file.exists():
            print(f"❌ 找不到bin文件: {bin_file}")
            return False
            
        print("请手动执行以下命令烧录Application:")
        print(f"openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \\")
        print(f"  -c \"init\" \\")
        print(f"  -c \"reset halt\" \\")
        print(f"  -c \"flash write_image {bin_file} 0x90010000\" \\")
        print(f"  -c \"reset run\" \\")
        print(f"  -c \"exit\"")
        
    def init_system(self):
        """完整的系统初始化"""
        print("🚀 开始STM32H750系统初始化...")
        print("=" * 50)
        
        # 1. 创建初始元数据
        metadata_file = self.create_initial_metadata()
        
        # 2. 烧录bootloader
        self.flash_bootloader()
        
        # 3. 烧录元数据
        self.flash_metadata(metadata_file)
        
        # 4. 烧录application
        self.flash_application_to_slot_a()
        
        print("=" * 50)
        print("✅ 系统初始化完成！")
        print("\n🎯 下一步:")
        print("1. 连接串口调试工具，波特率115200")
        print("2. 重启开发板，观察启动日志")
        print("3. 如果正常启动，可以开始开发调试")

def main():
    parser = argparse.ArgumentParser(description='STM32H750系统初始化工具')
    parser.add_argument('--project-root', '-p', 
                        default='.', 
                        help='项目根目录 (默认: 当前目录)')
    parser.add_argument('--skip-bootloader', '-s', 
                        action='store_true',
                        help='跳过bootloader烧录')
    parser.add_argument('--metadata-only', '-m',
                        action='store_true', 
                        help='只创建元数据文件')
    
    args = parser.parse_args()
    
    try:
        initializer = SystemInitializer(args.project_root)
        
        if args.metadata_only:
            initializer.create_initial_metadata()
        else:
            initializer.init_system()
            
    except Exception as e:
        print(f"❌ 初始化失败: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
