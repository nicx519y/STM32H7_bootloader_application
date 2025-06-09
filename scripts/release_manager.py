#!/usr/bin/env python3
"""
STM32H750 发版管理脚本
功能：
1. 列出所有构建的版本
2. 对比版本差异
3. 清理旧版本
4. 验证版本完整性
"""

import os
import sys
import json
from pathlib import Path
from datetime import datetime

class ReleaseManager:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.release_dir = self.project_root / "release"
        
    def list_versions(self):
        """列出所有版本"""
        if not self.release_dir.exists():
            print("❌ 发版目录不存在")
            return
            
        versions = []
        for version_dir in self.release_dir.iterdir():
            if version_dir.is_dir() and version_dir.name.startswith('v'):
                version_info_file = version_dir / "version_info.json"
                if version_info_file.exists():
                    try:
                        with open(version_info_file, 'r', encoding='utf-8') as f:
                            info = json.load(f)
                        versions.append((version_dir.name, info))
                    except:
                        versions.append((version_dir.name, {"error": "无法读取版本信息"}))
                        
        if not versions:
            print("📋 没有找到任何版本")
            return
            
        print("📋 已构建的版本:")
        print("-" * 80)
        print(f"{'版本号':<12} {'构建时间':<20} {'Bootloader':<12} {'Application':<12} {'QSPI镜像':<12}")
        print("-" * 80)
        
        for version_name, info in sorted(versions):
            if "error" in info:
                print(f"{version_name:<12} {info['error']}")
                continue
                
            build_time = info.get('build_time', 'N/A')[:19]  # 截取到秒
            bootloader_size = self.format_size(info.get('bootloader_size', 0))
            app_size = self.format_size(info.get('application_size', 0))
            qspi_size = self.format_size(info.get('qspi_image_size', 0))
            
            print(f"{version_name:<12} {build_time:<20} {bootloader_size:<12} {app_size:<12} {qspi_size:<12}")
            
    def format_size(self, size):
        """格式化文件大小"""
        if size >= 1024 * 1024:
            return f"{size / (1024 * 1024):.1f}MB"
        elif size >= 1024:
            return f"{size / 1024:.1f}KB"
        else:
            return f"{size}B"
            
    def show_version_details(self, version):
        """显示版本详细信息"""
        version_dir = self.release_dir / f"v{version}"
        if not version_dir.exists():
            print(f"❌ 版本 v{version} 不存在")
            return
            
        version_info_file = version_dir / "version_info.json"
        upgrade_pkg_file = version_dir / "upgrade_package.json"
        
        print(f"📋 版本 v{version} 详细信息:")
        print("=" * 50)
        
        # 版本基本信息
        if version_info_file.exists():
            with open(version_info_file, 'r', encoding='utf-8') as f:
                info = json.load(f)
            
            print(f"构建时间: {info.get('build_time', 'N/A')}")
            print(f"Bootloader大小: {self.format_size(info.get('bootloader_size', 0))}")
            print(f"Application大小: {self.format_size(info.get('application_size', 0))}")
            print(f"QSPI镜像大小: {self.format_size(info.get('qspi_image_size', 0))}")
            
        print()
        
        # 升级包信息
        if upgrade_pkg_file.exists():
            with open(upgrade_pkg_file, 'r', encoding='utf-8') as f:
                pkg = json.load(f)
                
            print("升级包信息:")
            print(f"  类型: {pkg.get('type', 'N/A')}")
            print(f"  描述: {pkg.get('description', 'N/A')}")
            
            if 'components' in pkg:
                for comp_name, comp_info in pkg['components'].items():
                    print(f"  {comp_name}:")
                    print(f"    文件: {comp_info.get('filename', 'N/A')}")
                    print(f"    大小: {self.format_size(comp_info.get('size', 0))}")
                    print(f"    CRC32: {comp_info.get('crc32', 'N/A')}")
                    print(f"    目标地址: {comp_info.get('target_address', 'N/A')}")
                    
        print()
        
        # 文件列表
        print("包含文件:")
        for file in version_dir.iterdir():
            if file.is_file():
                print(f"  - {file.name}: {self.format_size(file.stat().st_size)}")
                
    def compare_versions(self, version1, version2):
        """对比两个版本"""
        v1_dir = self.release_dir / f"v{version1}"
        v2_dir = self.release_dir / f"v{version2}"
        
        if not v1_dir.exists():
            print(f"❌ 版本 v{version1} 不存在")
            return
        if not v2_dir.exists():
            print(f"❌ 版本 v{version2} 不存在")
            return
            
        print(f"🔍 对比版本 v{version1} 和 v{version2}:")
        print("=" * 50)
        
        # 读取版本信息
        v1_info = {}
        v2_info = {}
        
        v1_info_file = v1_dir / "version_info.json"
        v2_info_file = v2_dir / "version_info.json"
        
        if v1_info_file.exists():
            with open(v1_info_file, 'r', encoding='utf-8') as f:
                v1_info = json.load(f)
                
        if v2_info_file.exists():
            with open(v2_info_file, 'r', encoding='utf-8') as f:
                v2_info = json.load(f)
                
        # 对比大小
        print("文件大小对比:")
        print(f"{'组件':<15} {'v' + version1:<15} {'v' + version2:<15} {'差异':<15}")
        print("-" * 60)
        
        components = ['bootloader_size', 'application_size', 'qspi_image_size']
        for comp in components:
            v1_size = v1_info.get(comp, 0)
            v2_size = v2_info.get(comp, 0)
            diff = v2_size - v1_size
            
            comp_name = comp.replace('_size', '').title()
            v1_str = self.format_size(v1_size)
            v2_str = self.format_size(v2_size)
            
            if diff == 0:
                diff_str = "无变化"
            elif diff > 0:
                diff_str = f"+{self.format_size(diff)}"
            else:
                diff_str = f"-{self.format_size(-diff)}"
                
            print(f"{comp_name:<15} {v1_str:<15} {v2_str:<15} {diff_str:<15}")
            
    def clean_old_versions(self, keep_count=5):
        """清理旧版本，保留最新的几个版本"""
        if not self.release_dir.exists():
            print("❌ 发版目录不存在")
            return
            
        versions = []
        for version_dir in self.release_dir.iterdir():
            if version_dir.is_dir() and version_dir.name.startswith('v'):
                version_info_file = version_dir / "version_info.json"
                if version_info_file.exists():
                    with open(version_info_file, 'r', encoding='utf-8') as f:
                        info = json.load(f)
                    build_time = info.get('build_time', '')
                    versions.append((build_time, version_dir))
                    
        if len(versions) <= keep_count:
            print(f"📋 当前版本数 ({len(versions)}) 不超过保留数量 ({keep_count})，无需清理")
            return
            
        # 按构建时间排序，保留最新的
        versions.sort(key=lambda x: x[0], reverse=True)
        to_delete = versions[keep_count:]
        
        print(f"🗑️  将删除 {len(to_delete)} 个旧版本:")
        for _, version_dir in to_delete:
            print(f"  - {version_dir.name}")
            
        confirm = input("确认删除? (y/N): ").strip().lower()
        if confirm == 'y':
            import shutil
            for _, version_dir in to_delete:
                shutil.rmtree(version_dir)
                print(f"✅ 已删除 {version_dir.name}")
        else:
            print("❌ 取消删除")
            
    def verify_version(self, version):
        """验证版本完整性"""
        version_dir = self.release_dir / f"v{version}"
        if not version_dir.exists():
            print(f"❌ 版本 v{version} 不存在")
            return False
            
        print(f"🔍 验证版本 v{version} 完整性:")
        
        required_files = [
            "bootloader.bin",
            "application.bin", 
            "qspi_full.bin",
            "upgrade_package.json",
            "version_info.json",
            "flash.sh"
        ]
        
        all_good = True
        
        for filename in required_files:
            file_path = version_dir / filename
            if file_path.exists():
                size = file_path.stat().st_size
                print(f"✅ {filename}: {self.format_size(size)}")
            else:
                print(f"❌ {filename}: 缺失")
                all_good = False
                
        if all_good:
            print("✅ 版本完整性验证通过")
        else:
            print("❌ 版本完整性验证失败")
            
        return all_good

def main():
    if len(sys.argv) < 2:
        print("STM32H750 发版管理工具")
        print("用法: python release_manager.py <command> [args...]")
        print()
        print("命令:")
        print("  list                     - 列出所有版本")
        print("  show <version>           - 显示版本详细信息")
        print("  compare <v1> <v2>        - 对比两个版本")
        print("  verify <version>         - 验证版本完整性")
        print("  clean [keep_count]       - 清理旧版本 (默认保留5个)")
        print()
        print("示例:")
        print("  python release_manager.py list")
        print("  python release_manager.py show 1.0.1")
        print("  python release_manager.py compare 1.0.0 1.0.1")
        print("  python release_manager.py clean 3")
        return
        
    project_root = Path(__file__).parent.parent
    manager = ReleaseManager(project_root)
    
    command = sys.argv[1]
    
    if command == "list":
        manager.list_versions()
    elif command == "show":
        if len(sys.argv) < 3:
            print("❌ 请指定版本号")
            return
        manager.show_version_details(sys.argv[2])
    elif command == "compare":
        if len(sys.argv) < 4:
            print("❌ 请指定两个版本号")
            return
        manager.compare_versions(sys.argv[2], sys.argv[3])
    elif command == "verify":
        if len(sys.argv) < 3:
            print("❌ 请指定版本号")
            return
        manager.verify_version(sys.argv[2])
    elif command == "clean":
        keep_count = int(sys.argv[2]) if len(sys.argv) > 2 else 5
        manager.clean_old_versions(keep_count)
    else:
        print(f"❌ 未知命令: {command}")

if __name__ == "__main__":
    main() 