#!/usr/bin/env python3
"""
STM32H750 Python依赖自动安装脚本
自动检测并安装项目所需的Python包
"""

import sys
import subprocess
import importlib
from pathlib import Path

class DependencyInstaller:
    def __init__(self):
        self.required_packages = {
            'serial': 'pyserial>=3.5',
            'watchdog': 'watchdog>=2.1.0'
        }
        
    def check_package(self, package_name):
        """检查包是否已安装"""
        try:
            importlib.import_module(package_name)
            return True
        except ImportError:
            return False
            
    def install_package(self, package_spec):
        """安装指定的包"""
        print(f"正在安装 {package_spec}...")
        
        # 尝试不同的pip命令
        pip_commands = ['pip', 'pip3', 'python -m pip', 'python3 -m pip']
        
        for pip_cmd in pip_commands:
            try:
                cmd = f"{pip_cmd} install {package_spec}"
                result = subprocess.run(cmd.split(), 
                                      capture_output=True, 
                                      text=True, 
                                      timeout=300)  # 5分钟超时
                
                if result.returncode == 0:
                    print(f"✅ {package_spec} 安装成功")
                    return True
                    
            except (subprocess.TimeoutExpired, FileNotFoundError):
                continue
                
        print(f"❌ {package_spec} 安装失败")
        return False
        
    def install_from_requirements(self):
        """从requirements.txt安装所有依赖"""
        requirements_file = Path(__file__).parent / "requirements.txt"
        
        if not requirements_file.exists():
            print("❌ 找不到requirements.txt文件")
            return False
            
        print("📦 从requirements.txt安装依赖...")
        
        pip_commands = ['pip', 'pip3', 'python -m pip', 'python3 -m pip']
        
        for pip_cmd in pip_commands:
            try:
                cmd = f"{pip_cmd} install -r {requirements_file}"
                result = subprocess.run(cmd.split(), 
                                      capture_output=True, 
                                      text=True, 
                                      timeout=300)
                
                if result.returncode == 0:
                    print("✅ 所有依赖安装成功")
                    return True
                else:
                    print(f"安装输出: {result.stdout}")
                    print(f"错误信息: {result.stderr}")
                    
            except (subprocess.TimeoutExpired, FileNotFoundError):
                continue
                
        print("❌ 依赖安装失败")
        return False
        
    def check_and_install_all(self):
        """检查并安装所有必需的包"""
        print("🔍 检查Python依赖...")
        
        missing_packages = []
        
        for package_name, package_spec in self.required_packages.items():
            if self.check_package(package_name):
                print(f"✅ {package_name} 已安装")
            else:
                print(f"❌ {package_name} 未安装")
                missing_packages.append(package_spec)
                
        if not missing_packages:
            print("🎉 所有依赖都已安装！")
            return True
            
        print(f"\n📦 需要安装 {len(missing_packages)} 个包...")
        
        # 尝试批量安装
        success = self.install_from_requirements()
        
        if not success:
            # 逐个安装
            print("尝试逐个安装...")
            for package_spec in missing_packages:
                self.install_package(package_spec)
                
        # 再次检查
        print("\n🔍 验证安装结果...")
        all_installed = True
        for package_name in self.required_packages.keys():
            if self.check_package(package_name):
                print(f"✅ {package_name} 验证成功")
            else:
                print(f"❌ {package_name} 仍然缺失")
                all_installed = False
                
        return all_installed
        
    def show_manual_install_instructions(self):
        """显示手动安装指令"""
        print("\n📋 手动安装指令:")
        print("如果自动安装失败，请手动执行以下命令:")
        print()
        
        print("方式1 - 使用requirements.txt:")
        print("pip install -r requirements.txt")
        print("或")
        print("pip3 install -r requirements.txt")
        print()
        
        print("方式2 - 逐个安装:")
        for package_spec in self.required_packages.values():
            print(f"pip install {package_spec}")
        print()
        
        print("方式3 - 使用conda (如果使用Anaconda):")
        print("conda install pyserial watchdog")
        print()
        
        print("💡 小贴士:")
        print("- 如果遇到权限问题，尝试添加 --user 参数")
        print("- 如果在公司网络，可能需要配置代理")
        print("- Windows用户可以尝试在管理员权限下运行")

def main():
    print("🚀 STM32H750 Python依赖检查和安装工具")
    print("=" * 50)
    
    installer = DependencyInstaller()
    
    # 显示Python环境信息
    print(f"Python版本: {sys.version}")
    print(f"Python路径: {sys.executable}")
    print()
    
    # 检查并安装依赖
    success = installer.check_and_install_all()
    
    if success:
        print("\n🎉 所有依赖都已准备就绪！")
        print("现在可以正常使用项目脚本了。")
        
        # 测试主要功能
        print("\n🧪 测试脚本功能...")
        try:
            # 测试串口功能
            import serial
            print("✅ 串口功能可用")
            
            # 测试文件监控功能
            import watchdog
            print("✅ 文件监控功能可用")
            
            print("\n✨ 所有功能测试通过！")
            
        except ImportError as e:
            print(f"⚠️  导入测试失败: {e}")
            
    else:
        print("\n❌ 依赖安装不完整")
        installer.show_manual_install_instructions()
        return 1
        
    return 0

if __name__ == "__main__":
    sys.exit(main()) 