#!/usr/bin/env python3
"""
STM32H750 固件发版构建脚本
功能：
1. 编译bootloader和application
2. 生成升级包
3. 创建完整的固件镜像
"""

import os
import sys
import subprocess
import struct
import zlib
import json
import shutil
import argparse
from pathlib import Path
from datetime import datetime

# 配置参数
QSPI_BASE = 0x90000000
METADATA_ADDR = 0x90000000

# W25Q64 槽位地址定义 
APPLICATION_SLOT_A = 0x90010000
APPLICATION_SLOT_B = 0x902B0000

WEB_RESOURCES_SLOT_A = 0x90110000
WEB_RESOURCES_SLOT_B = 0x903B0000

CONFIG_SLOT_A = 0x90290000  
CONFIG_SLOT_B = 0x904B0000

ADC_MAPPING_SLOT_A = 0x902A0000
ADC_MAPPING_SLOT_B = 0x904C0000

class FirmwareBuilder:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.bootloader_dir = self.project_root / "bootloader"
        self.application_dir = self.project_root / "application"
        self.output_dir = self.project_root / "release"
        self.cpu_count = self.get_cpu_count()
        
    def get_cpu_count(self):
        """获取CPU核心数用于并行编译"""
        try:
            import multiprocessing
            return multiprocessing.cpu_count()
        except:
            return 4  # 默认使用4线程
        
    def clean_projects(self):
        """清理项目"""
        print("🧹 清理项目...")
        subprocess.run(["make", "clean"], cwd=self.bootloader_dir, check=True)
        subprocess.run(["make", "clean"], cwd=self.application_dir, check=True)
        
    def build_bootloader(self):
        """编译bootloader"""
        print("🔨 编译Bootloader...")
        result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.bootloader_dir, 
                               capture_output=True, text=True, errors='ignore')
        if result.returncode != 0:
            print(f"❌ Bootloader编译失败:\n{result.stderr}")
            sys.exit(1)
        print("✅ Bootloader编译成功")
        return self.bootloader_dir / "build" / "bootloader.bin"
        
    def build_application(self):
        """编译application"""
        print("🔨 编译Application...")
        result = subprocess.run(["make", f"-j{self.cpu_count}"], cwd=self.application_dir, 
                               capture_output=True, text=True, errors='ignore')
        if result.returncode != 0:
            print(f"❌ Application编译失败:\n{result.stderr}")
            sys.exit(1)
        print("✅ Application编译成功")
        return self.application_dir / "build" / "application.bin"
        
    def calculate_crc32(self, data):
        """计算CRC32"""
        return zlib.crc32(data) & 0xffffffff
        
    def create_upgrade_metadata(self, version="1.0.0"):
        """创建升级元数据"""
        print("📦 创建升级元数据...")
        
        # 读取application文件信息
        app_bin = self.application_dir / "build" / "application.bin"
        app_data = app_bin.read_bytes()
        app_crc = self.calculate_crc32(app_data)
        
        # 升级元数据结构
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
        metadata += struct.pack('<I', int(version.replace('.', '')))  # version
        metadata += struct.pack('<B', 1)  # checksum_type (CRC32)
        metadata += struct.pack('<I', app_crc)  # checksum
        metadata += b'\x00' * 32  # hash (unused for CRC32)
        metadata += struct.pack('<B', 8)  # status (SUCCESS)
        metadata += b'\x00' * 3  # padding
        
        # 其他组件信息 (暂时为空)
        for _ in range(3):  # web, config, adc
            metadata += b'\x00' * 48  # empty component info
            
        # 保留字段
        metadata += b'\x00' * 64  # reserved[16]
        
        # 计算元数据CRC32
        metadata_crc = self.calculate_crc32(metadata)
        metadata += struct.pack('<I', metadata_crc)
        
        return metadata
        
    def create_qspi_image(self, version="1.0.0"):
        """创建完整的QSPI镜像"""
        print("💾 创建QSPI镜像...")
        
        # 创建8MB的空镜像
        qspi_image = bytearray(8 * 1024 * 1024)
        
        # 写入升级元数据
        metadata = self.create_upgrade_metadata(version)
        qspi_image[0:len(metadata)] = metadata
        
        # 写入Application到Slot A
        app_bin = self.application_dir / "build" / "application.bin"
        app_data = app_bin.read_bytes()
        slot_a_offset = APPLICATION_SLOT_A - QSPI_BASE
        qspi_image[slot_a_offset:slot_a_offset + len(app_data)] = app_data
        
        return bytes(qspi_image)
        
    def create_upgrade_package(self, version="1.0.0"):
        """创建升级包"""
        print("📦 创建升级包...")
        
        app_bin = self.application_dir / "build" / "application.bin"
        app_data = app_bin.read_bytes()
        
        # 只包含元数据，不包含实际的二进制数据
        upgrade_pkg = {
            "version": version,
            "type": "application_only",
            "build_time": datetime.now().isoformat(),
            "description": f"STM32H750 Application v{version}",
            "components": {
                "application": {
                    "filename": "application.bin",
                    "size": len(app_data),
                    "crc32": hex(self.calculate_crc32(app_data)),
                    "target_address": "0x90010000",
                    "description": "Main application firmware"
                }
            },
            "flash_instructions": {
                "bootloader": {
                    "filename": "bootloader.bin",
                    "target": "internal_flash",
                    "address": "0x08000000"
                },
                "qspi_image": {
                    "filename": "qspi_full.bin", 
                    "target": "qspi_flash",
                    "address": "0x90000000"
                }
            },
            "usage": "Use flash.sh script to program the device"
        }
        
        return json.dumps(upgrade_pkg, indent=2)
        
    def build_release(self, version="1.0.0"):
        """构建发版"""
        print(f"🚀 开始构建发版 v{version}")
        
        # 创建输出目录
        self.output_dir.mkdir(exist_ok=True)
        
        # 清理和编译
        self.clean_projects()
        bootloader_bin = self.build_bootloader()
        application_bin = self.build_application()
        
        # 创建发版文件
        version_dir = self.output_dir / f"v{version}"
        version_dir.mkdir(exist_ok=True)
        
        # 1. 复制单独的固件文件
        shutil.copy2(bootloader_bin, version_dir / "bootloader.bin")
        shutil.copy2(application_bin, version_dir / "application.bin")
        
        # 2. 创建完整的QSPI镜像
        qspi_image = self.create_qspi_image(version)
        (version_dir / "qspi_full.bin").write_bytes(qspi_image)
        
        # 3. 创建升级包
        upgrade_pkg = self.create_upgrade_package(version)
        with open(version_dir / "upgrade_package.json", 'w', encoding='utf-8') as f:
            f.write(upgrade_pkg)
        
        # 4. 创建烧录脚本
        flash_script = f"""#!/bin/bash
# STM32H750 固件烧录脚本 v{version}

echo "开始烧录STM32H750固件..."

# 烧录Bootloader到内部Flash
echo "1. 烧录Bootloader到内部Flash..."
cd ../bootloader
openocd -f Openocd_Script/ST-LINK-FLASH.cfg -c "program build/bootloader.elf verify reset exit"

# 烧录QSPI完整镜像
echo "2. 烧录QSPI完整镜像..."
cd ../application  
openocd -f Openocd_Script/ST-LINK-QSPIFLASH.cfg -c "program ../release/v{version}/qspi_full.bin 0x90000000 verify exit"

echo "固件烧录完成！"
echo "请重启设备。"
"""
        with open(version_dir / "flash.sh", 'w', encoding='utf-8') as f:
            f.write(flash_script)
        (version_dir / "flash.sh").chmod(0o755)
        
        # 5. 创建版本信息
        version_info = {
            "version": version,
            "build_time": datetime.now().isoformat(),
            "bootloader_size": bootloader_bin.stat().st_size,
            "application_size": application_bin.stat().st_size,
            "qspi_image_size": len(qspi_image),
            "files": [
                "bootloader.bin",
                "application.bin", 
                "qspi_full.bin",
                "upgrade_package.json",
                "flash.sh"
            ]
        }
        with open(version_dir / "version_info.json", 'w', encoding='utf-8') as f:
            f.write(json.dumps(version_info, indent=2))
        
        print(f"✅ 发版构建完成！")
        print(f"📁 输出目录: {version_dir}")
        print(f"📊 文件列表:")
        for file in version_dir.iterdir():
            print(f"   - {file.name}: {file.stat().st_size} bytes")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: python build_release.py <version>")
        print("例如: python build_release.py 1.0.0")
        sys.exit(1)
        
    version = sys.argv[1]
    project_root = Path(__file__).parent.parent
    
    builder = FirmwareBuilder(project_root)
    builder.build_release(version) 