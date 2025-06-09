#!/usr/bin/env python3
"""
STM32H750 快速开始脚本
帮助用户快速配置和使用整个双槽升级系统
"""

import os
import sys
import subprocess
from pathlib import Path

def print_header():
    print("=" * 60)
    print("🚀 STM32H750 双槽升级系统 - 快速开始")
    print("=" * 60)
    print()

def print_step(step_num, title, description=""):
    print(f"📋 Step {step_num}: {title}")
    if description:
        print(f"   {description}")
    print()

def get_cpu_count():
    """获取CPU核心数用于并行编译"""
    try:
        import multiprocessing
        return multiprocessing.cpu_count()
    except:
        return 4  # 默认使用4线程

def check_python_packages():
    """检查Python包依赖"""
    print("🐍 检查Python包依赖...")
    
    required_packages = {
        'serial': 'pyserial',
        'watchdog': 'watchdog'
    }
    
    missing_packages = []
    
    for package_name, package_display in required_packages.items():
        try:
            __import__(package_name)
            print(f"✅ {package_display}")
        except ImportError:
            print(f"❌ {package_display} (缺失)")
            missing_packages.append(package_name)
    
    if missing_packages:
        print(f"\n⚠️  缺少Python包: {', '.join(missing_packages)}")
        print("正在自动安装依赖...")
        
        # 调用依赖安装脚本
        try:
            result = subprocess.run([sys.executable, "install_deps.py"], 
                                  capture_output=True, text=True)
            if result.returncode == 0:
                print("✅ Python依赖安装成功")
                
                # 再次检查
                still_missing = []
                for package_name in missing_packages:
                    try:
                        __import__(package_name)
                    except ImportError:
                        still_missing.append(package_name)
                
                if still_missing:
                    print(f"❌ 仍有包缺失: {', '.join(still_missing)}")
                    print("请手动安装:")
                    print("pip install -r requirements.txt")
                    return False
                    
            else:
                print(f"❌ 自动安装失败:\n{result.stderr}")
                print("请手动安装:")
                print("pip install -r requirements.txt")
                return False
                
        except FileNotFoundError:
            print("❌ 找不到install_deps.py脚本")
            print("请手动安装:")
            print("pip install -r requirements.txt")
            return False
    
    return True

def check_dependencies():
    """检查依赖工具"""
    print_step(1, "检查依赖工具")
    
    # 检查Python包依赖
    if not check_python_packages():
        return False
    
    required_tools = [
        ("make", "构建工具"),
        ("arm-none-eabi-gcc", "ARM交叉编译器"),
        ("openocd", "调试和烧录工具"),
        ("python", "Python解释器")
    ]
    
    missing_tools = []
    
    for tool, desc in required_tools:
        try:
            result = subprocess.run([tool, "--version"], 
                                  capture_output=True, 
                                  text=True, 
                                  timeout=5)
            if result.returncode == 0:
                print(f"✅ {tool} - {desc}")
            else:
                print(f"❌ {tool} - {desc} (找不到)")
                missing_tools.append(tool)
        except (subprocess.TimeoutExpired, FileNotFoundError):
            print(f"❌ {tool} - {desc} (找不到)")
            missing_tools.append(tool)
    
    if missing_tools:
        print(f"\n⚠️  缺少工具: {', '.join(missing_tools)}")
        print("请安装缺少的工具后再继续")
        return False
    
    print("✅ 所有依赖工具已安装")
    return True

def check_project_structure():
    """检查项目结构"""
    print_step(2, "检查项目结构")
    
    project_root = Path(__file__).parent
    required_dirs = [
        "bootloader",
        "application", 
        "scripts"
    ]
    
    required_files = [
        "bootloader/Makefile",
        "application/Makefile",
        "scripts/init_system.py",
        "scripts/dev_debug.py"
    ]
    
    all_good = True
    
    for dir_name in required_dirs:
        dir_path = project_root / dir_name
        if dir_path.exists():
            print(f"✅ {dir_name}/ 目录")
        else:
            print(f"❌ {dir_name}/ 目录 (缺失)")
            all_good = False
    
    for file_name in required_files:
        file_path = project_root / file_name
        if file_path.exists():
            print(f"✅ {file_name}")
        else:
            print(f"❌ {file_name} (缺失)")
            all_good = False
    
    if not all_good:
        print("\n⚠️  项目结构不完整，请检查缺失的目录和文件")
        return False
    
    print("✅ 项目结构完整")
    return True

def initial_build():
    """初始编译"""
    print_step(3, "初始编译", "编译bootloader和application以验证环境")
    
    project_root = Path(__file__).parent
    cpu_count = get_cpu_count()
    print(f"🔥 使用 {cpu_count} 线程并行编译")
    
    # 编译bootloader
    print("🔨 编译Bootloader...")
    result = subprocess.run(["make", "clean"], 
                          cwd=project_root / "bootloader",
                          capture_output=True, text=True)
    result = subprocess.run(["make", f"-j{cpu_count}"], 
                          cwd=project_root / "bootloader",
                          capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"❌ Bootloader编译失败:\n{result.stderr}")
        return False
    print("✅ Bootloader编译成功")
    
    # 编译application
    print("🔨 编译Application...")
    result = subprocess.run(["make", "clean"], 
                          cwd=project_root / "application",
                          capture_output=True, text=True)
    result = subprocess.run(["make", f"-j{cpu_count}"], 
                          cwd=project_root / "application",
                          capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"❌ Application编译失败:\n{result.stderr}")
        return False
    print("✅ Application编译成功")
    
    return True

def show_next_steps():
    """显示下一步操作"""
    print_step(4, "下一步操作")
    
    print("🎯 根据你的需求选择：")
    print()
    print("📱 首次部署新设备:")
    print("   python scripts/init_system.py init")
    print()
    print("🔧 开发调试:")
    print("   python scripts/dev_debug.py build      # 快速编译")
    print("   python scripts/dev_debug.py flash-app  # 烧录application")
    print("   python scripts/dev_debug.py monitor    # 串口监控")
    print("   python scripts/dev_debug.py watch      # 自动编译")
    print()
    print("🚀 发布新版本:")
    print("   python release_scripts/build_release.py --version 1.0.2.1")
    print()
    print("📖 详细文档:")
    print("   查看 工作流程.md 了解完整工作流程")
    print("   查看 README_升级架构.md 了解系统架构")
    print()

def interactive_setup():
    """交互式设置"""
    print("🤖 交互式设置向导")
    print()
    
    # 询问用户需求
    print("请选择你的使用场景:")
    print("1. 首次部署新设备")
    print("2. 开发调试现有代码")
    print("3. 发布新版本")
    print("4. 仅查看系统状态")
    
    choice = input("\n请输入选择 (1-4): ").strip()
    
    if choice == "1":
        print("\n🚀 开始首次部署...")
        print("即将执行: python scripts/init_system.py init")
        confirm = input("继续? (y/N): ").strip().lower()
        if confirm == 'y':
            subprocess.run([sys.executable, "scripts/init_system.py", "init"])
        
    elif choice == "2":
        print("\n🔧 开发调试模式")
        print("可用命令:")
        print("- python scripts/dev_debug.py build")
        print("- python scripts/dev_debug.py flash-app")
        print("- python scripts/dev_debug.py monitor")
        
        action = input("\n选择操作 (build/flash-app/monitor): ").strip()
        if action in ["build", "flash-app", "monitor"]:
            subprocess.run([sys.executable, "scripts/dev_debug.py", action])
            
    elif choice == "3":
        print("\n🚀 发布新版本")
        version = input("请输入版本号 (例如: 1.0.2.1): ").strip()
        if version:
            subprocess.run([sys.executable, "release_scripts/build_release.py", 
                          "--version", version])
            
    elif choice == "4":
        print("\n📊 系统状态")
        project_root = Path(__file__).parent
        
        # 检查构建文件
        bootloader_elf = project_root / "bootloader/build/bootloader.elf"
        application_elf = project_root / "application/build/application.elf"
        
        print(f"Bootloader: {'✅ 已构建' if bootloader_elf.exists() else '❌ 未构建'}")
        print(f"Application: {'✅ 已构建' if application_elf.exists() else '❌ 未构建'}")
        
    else:
        print("无效选择")

def main():
    print_header()
    
    # 检查依赖
    if not check_dependencies():
        return 1
    
    # 检查项目结构
    if not check_project_structure():
        return 1
    
    # 初始编译
    if not initial_build():
        return 1
    
    # 显示下一步
    show_next_steps()
    
    # 询问是否进入交互模式
    if len(sys.argv) == 1:  # 没有命令行参数
        interactive = input("是否进入交互式设置向导? (y/N): ").strip().lower()
        if interactive == 'y':
            interactive_setup()
    
    print("\n🎉 快速开始完成!")
    print("如有问题，请查看工作流程.md或联系技术支持")
    
    return 0

if __name__ == "__main__":
    sys.exit(main()) 