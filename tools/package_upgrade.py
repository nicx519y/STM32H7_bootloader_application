#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
固件升级包打包工具
支持生成H750项目的标准升级包格式
"""

import os
import sys
import json
import struct
import argparse
import binascii
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional

# 升级包版本
PACKAGE_VERSION = "1.0.0"

# 升级类型定义（与固件中的枚举对应）
UPGRADE_TYPES = {
    'FIRMWARE': 0x01,      # Application + Web Resources
    'RESOURCES': 0x02,     # Config + ADC Mapping  
    'APPLICATION': 0x03,   # 仅Application
    'WEB': 0x04,          # 仅Web Resources
    'CONFIG': 0x05,       # 仅Config
    'ADC': 0x06           # 仅ADC Mapping
}

# 校验类型定义
CHECKSUM_TYPES = {
    'CRC32': 0x01,
    'MD5': 0x02,
    'SHA256': 0x03
}

class ComponentInfo:
    """组件信息类"""
    def __init__(self, name: str, file_path: str, version: str = "1.0.0"):
        self.name = name
        self.file_path = file_path
        self.version = version
        self.size = 0
        self.crc32 = 0
        self.data = b''
        
    def load_data(self):
        """加载组件数据"""
        if not os.path.exists(self.file_path):
            raise FileNotFoundError(f"组件文件不存在: {self.file_path}")
            
        with open(self.file_path, 'rb') as f:
            self.data = f.read()
            self.size = len(self.data)
            self.crc32 = self._calculate_crc32(self.data)
    
    def _calculate_crc32(self, data: bytes) -> int:
        """计算CRC32"""
        return binascii.crc32(data) & 0xFFFFFFFF
    
    def to_dict(self) -> Dict:
        """转换为字典格式"""
        return {
            'name': self.name,
            'size': self.size,
            'version': self.version,
            'checksum_type': 'CRC32',
            'checksum': self.crc32,
            'file_path': self.file_path
        }

class UpgradePackage:
    """升级包类"""
    def __init__(self, package_type: str, version: str = "1.0.0"):
        if package_type.upper() not in UPGRADE_TYPES:
            raise ValueError(f"不支持的升级类型: {package_type}")
            
        self.package_type = package_type.upper()
        self.version = version
        self.timestamp = int(datetime.now().timestamp())
        self.components: List[ComponentInfo] = []
        self.metadata = {}
        
    def add_component(self, component: ComponentInfo):
        """添加组件"""
        component.load_data()
        self.components.append(component)
        
    def validate_package(self):
        """验证升级包内容"""
        type_val = UPGRADE_TYPES[self.package_type]
        
        # 根据升级类型验证必需的组件
        required_components = []
        if type_val == UPGRADE_TYPES['FIRMWARE']:
            required_components = ['application', 'web_resources']
        elif type_val == UPGRADE_TYPES['RESOURCES']:
            required_components = ['config', 'adc_mapping']
        elif type_val == UPGRADE_TYPES['APPLICATION']:
            required_components = ['application']
        elif type_val == UPGRADE_TYPES['WEB']:
            required_components = ['web_resources']
        elif type_val == UPGRADE_TYPES['CONFIG']:
            required_components = ['config']
        elif type_val == UPGRADE_TYPES['ADC']:
            required_components = ['adc_mapping']
            
        component_names = [comp.name for comp in self.components]
        missing = set(required_components) - set(component_names)
        if missing:
            raise ValueError(f"缺少必需的组件: {missing}")
            
        # 验证组件大小限制
        size_limits = {
            'application': 1024 * 1024,        # 1MB
            'web_resources': 1536 * 1024,      # 1.5MB
            'config': 64 * 1024,               # 64KB
            'adc_mapping': 64 * 1024            # 64KB
        }
        
        for comp in self.components:
            if comp.name in size_limits:
                if comp.size > size_limits[comp.name]:
                    raise ValueError(f"组件 {comp.name} 大小 {comp.size} 超过限制 {size_limits[comp.name]}")
    
    def generate_metadata(self) -> Dict:
        """生成元数据"""
        metadata = {
            'package_info': {
                'version': PACKAGE_VERSION,
                'type': self.package_type,
                'upgrade_type_code': UPGRADE_TYPES[self.package_type],
                'firmware_version': self.version,
                'timestamp': self.timestamp,
                'build_time': datetime.fromtimestamp(self.timestamp).isoformat()
            },
            'components': [comp.to_dict() for comp in self.components],
            'checksum_info': {
                'algorithm': 'CRC32',
                'package_crc32': 0  # 将在打包时计算
            }
        }
        return metadata
    
    def pack(self, output_path: str):
        """打包升级包"""
        self.validate_package()
        
        # 生成元数据
        metadata = self.generate_metadata()
        
        # 创建升级包文件
        with open(output_path, 'wb') as f:
            # 1. 写入包头
            header = self._create_header(metadata)
            f.write(header)
            
            # 2. 写入元数据
            metadata_json = json.dumps(metadata, indent=2, ensure_ascii=False)
            metadata_bytes = metadata_json.encode('utf-8')
            metadata_size = len(metadata_bytes)
            
            # 写入元数据大小和内容
            f.write(struct.pack('<I', metadata_size))
            f.write(metadata_bytes)
            
            # 3. 写入组件数据
            for comp in self.components:
                # 写入组件大小和数据
                f.write(struct.pack('<I', comp.size))
                f.write(comp.data)
                
                # 对齐到4字节边界
                padding = (4 - (comp.size % 4)) % 4
                if padding > 0:
                    f.write(b'\x00' * padding)
        
        print(f"升级包已生成: {output_path}")
        print(f"包类型: {self.package_type}")
        print(f"组件数量: {len(self.components)}")
        for comp in self.components:
            print(f"  - {comp.name}: {comp.size} 字节, CRC32: 0x{comp.crc32:08X}")
    
    def _create_header(self, metadata: Dict) -> bytes:
        """创建包头"""
        # 包头格式:
        # Magic (4字节): "H750"
        # Version (4字节): 包版本
        # Type (4字节): 升级类型
        # Timestamp (8字节): 时间戳
        # Component Count (4字节): 组件数量
        # Reserved (12字节): 保留字段
        
        magic = b'H750'
        version = struct.pack('<I', int(self.version.replace('.', '')))
        upgrade_type = struct.pack('<I', UPGRADE_TYPES[self.package_type])
        timestamp = struct.pack('<Q', self.timestamp)
        component_count = struct.pack('<I', len(self.components))
        reserved = b'\x00' * 12
        
        return magic + version + upgrade_type + timestamp + component_count + reserved

def create_firmware_package(args):
    """创建固件包 (Application + Web Resources)"""
    package = UpgradePackage('FIRMWARE', args.version)
    
    # 添加Application组件
    if args.application:
        app_comp = ComponentInfo('application', args.application, args.version)
        package.add_component(app_comp)
    
    # 添加Web Resources组件
    if args.web_resources:
        web_comp = ComponentInfo('web_resources', args.web_resources, args.version)
        package.add_component(web_comp)
    
    output_name = f"firmware_v{args.version}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.upg"
    output_path = os.path.join(args.output, output_name)
    package.pack(output_path)

def create_resources_package(args):
    """创建资源包 (Config + ADC Mapping)"""
    package = UpgradePackage('RESOURCES', args.version)
    
    # 添加Config组件
    if args.config:
        config_comp = ComponentInfo('config', args.config, args.version)
        package.add_component(config_comp)
    
    # 添加ADC Mapping组件
    if args.adc_mapping:
        adc_comp = ComponentInfo('adc_mapping', args.adc_mapping, args.version)
        package.add_component(adc_comp)
    
    output_name = f"resources_v{args.version}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.upg"
    output_path = os.path.join(args.output, output_name)
    package.pack(output_path)

def create_single_component_package(args, component_type: str, file_path: str):
    """创建单组件包"""
    package = UpgradePackage(component_type, args.version)
    comp = ComponentInfo(component_type.lower(), file_path, args.version)
    package.add_component(comp)
    
    output_name = f"{component_type.lower()}_v{args.version}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.upg"
    output_path = os.path.join(args.output, output_name)
    package.pack(output_path)

def main():
    parser = argparse.ArgumentParser(description='H750固件升级包打包工具')
    parser.add_argument('--version', '-v', default='1.0.0', help='固件版本号')
    parser.add_argument('--output', '-o', default='./output', help='输出目录')
    
    subparsers = parser.add_subparsers(dest='command', help='打包命令')
    
    # 固件包命令
    firmware_parser = subparsers.add_parser('firmware', help='创建固件包')
    firmware_parser.add_argument('--application', '-a', help='Application二进制文件路径')
    firmware_parser.add_argument('--web-resources', '-w', help='Web Resources文件路径')
    
    # 资源包命令
    resources_parser = subparsers.add_parser('resources', help='创建资源包')
    resources_parser.add_argument('--config', '-c', help='Config文件路径')
    resources_parser.add_argument('--adc-mapping', '-m', help='ADC Mapping文件路径')
    
    # 单组件包命令
    app_parser = subparsers.add_parser('application', help='创建Application包')
    app_parser.add_argument('file', help='Application文件路径')
    
    web_parser = subparsers.add_parser('web', help='创建Web Resources包')
    web_parser.add_argument('file', help='Web Resources文件路径')
    
    config_parser = subparsers.add_parser('config', help='创建Config包')
    config_parser.add_argument('file', help='Config文件路径')
    
    adc_parser = subparsers.add_parser('adc', help='创建ADC Mapping包')
    adc_parser.add_argument('file', help='ADC Mapping文件路径')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return
    
    # 创建输出目录
    os.makedirs(args.output, exist_ok=True)
    
    try:
        if args.command == 'firmware':
            create_firmware_package(args)
        elif args.command == 'resources':
            create_resources_package(args)
        elif args.command == 'application':
            create_single_component_package(args, 'APPLICATION', args.file)
        elif args.command == 'web':
            create_single_component_package(args, 'WEB', args.file)
        elif args.command == 'config':
            create_single_component_package(args, 'CONFIG', args.file)
        elif args.command == 'adc':
            create_single_component_package(args, 'ADC', args.file)
            
    except Exception as e:
        print(f"错误: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main() 