#!/usr/bin/env python3
"""
编译性能测试工具
比较不同线程数下的编译速度
"""

import subprocess
import time
import multiprocessing
from pathlib import Path
import sys

class BuildBenchmark:
    def __init__(self, project_root):
        self.project_root = Path(project_root)
        self.bootloader_dir = self.project_root / "bootloader"
        self.application_dir = self.project_root / "application"
        self.max_cores = multiprocessing.cpu_count()
        
    def clean_project(self, project_dir):
        """清理项目"""
        subprocess.run(["make", "clean"], cwd=project_dir, 
                      capture_output=True, text=True)
    
    def build_project(self, project_dir, jobs=1):
        """编译项目并计时"""
        start_time = time.time()
        
        if jobs == 1:
            result = subprocess.run(["make"], cwd=project_dir, 
                                  capture_output=True, text=True)
        else:
            result = subprocess.run(["make", f"-j{jobs}"], cwd=project_dir,
                                  capture_output=True, text=True)
        
        end_time = time.time()
        duration = end_time - start_time
        
        return result.returncode == 0, duration
    
    def benchmark_project(self, project_name, project_dir):
        """测试单个项目的编译性能"""
        print(f"\n🔨 测试 {project_name} 编译性能...")
        print("=" * 50)
        
        # 测试不同的线程数
        test_jobs = [1, 2, 4, self.max_cores]
        if self.max_cores > 4 and self.max_cores not in test_jobs:
            test_jobs.append(self.max_cores)
        
        results = []
        
        for jobs in test_jobs:
            print(f"测试 {jobs} 线程编译...")
            
            # 清理项目
            self.clean_project(project_dir)
            
            # 编译并计时
            success, duration = self.build_project(project_dir, jobs)
            
            if success:
                print(f"✅ {jobs} 线程: {duration:.2f}秒")
                results.append((jobs, duration))
            else:
                print(f"❌ {jobs} 线程: 编译失败")
                
        return results
    
    def run_full_benchmark(self):
        """运行完整的性能测试"""
        print("🚀 STM32H750 编译性能测试")
        print(f"系统CPU核心数: {self.max_cores}")
        print("=" * 60)
        
        # 测试Bootloader
        bootloader_results = self.benchmark_project("Bootloader", self.bootloader_dir)
        
        # 测试Application
        application_results = self.benchmark_project("Application", self.application_dir)
        
        # 显示汇总结果
        self.show_summary(bootloader_results, application_results)
        
        # 给出推荐配置
        self.show_recommendations(bootloader_results, application_results)
    
    def show_summary(self, bootloader_results, application_results):
        """显示汇总结果"""
        print("\n📊 性能测试汇总")
        print("=" * 60)
        
        print("\n🔹 Bootloader 编译时间:")
        for jobs, duration in bootloader_results:
            speedup = bootloader_results[0][1] / duration if bootloader_results else 1
            print(f"  {jobs:2d} 线程: {duration:6.2f}秒 (加速比: {speedup:.2f}x)")
            
        print("\n🔹 Application 编译时间:")
        for jobs, duration in application_results:
            speedup = application_results[0][1] / duration if application_results else 1
            print(f"  {jobs:2d} 线程: {duration:6.2f}秒 (加速比: {speedup:.2f}x)")
            
        # 计算总编译时间
        print("\n🔹 总编译时间 (Bootloader + Application):")
        if bootloader_results and application_results:
            for i, ((jobs1, dur1), (jobs2, dur2)) in enumerate(zip(bootloader_results, application_results)):
                total_duration = dur1 + dur2
                if i == 0:
                    baseline_total = total_duration
                speedup = baseline_total / total_duration
                print(f"  {jobs1:2d} 线程: {total_duration:6.2f}秒 (加速比: {speedup:.2f}x)")
    
    def show_recommendations(self, bootloader_results, application_results):
        """显示推荐配置"""
        print("\n💡 推荐配置")
        print("=" * 60)
        
        if not bootloader_results or not application_results:
            print("⚠️  测试数据不足，无法给出推荐")
            return
            
        # 找到最佳性能的线程数
        best_boot_jobs = min(bootloader_results, key=lambda x: x[1])[0]
        best_app_jobs = min(application_results, key=lambda x: x[1])[0]
        
        # 计算性能提升
        boot_improvement = (bootloader_results[0][1] / min(bootloader_results, key=lambda x: x[1])[1] - 1) * 100
        app_improvement = (application_results[0][1] / min(application_results, key=lambda x: x[1])[1] - 1) * 100
        
        print(f"🚀 推荐使用 {max(best_boot_jobs, best_app_jobs)} 线程并行编译")
        print(f"   - Bootloader 最佳: {best_boot_jobs} 线程 (性能提升: {boot_improvement:.1f}%)")
        print(f"   - Application 最佳: {best_app_jobs} 线程 (性能提升: {app_improvement:.1f}%)")
        
        print(f"\n🔧 所有脚本已优化为使用 {self.max_cores} 线程编译")
        print("   - quick_start.py")
        print("   - scripts/dev_debug.py")
        print("   - scripts/init_system.py")
        print("   - release_scripts/build_release.py")
        print("   - debug_scripts/debug_setup.py")
        
        # 显示使用建议
        print("\n📖 使用建议:")
        if boot_improvement > 50 or app_improvement > 50:
            print("✅ 并行编译效果显著，强烈推荐使用")
        elif boot_improvement > 20 or app_improvement > 20:
            print("✅ 并行编译有一定效果，建议使用")
        else:
            print("⚠️  并行编译效果有限，可能项目规模较小")
            
        if self.max_cores >= 8:
            print("💻 您的系统CPU核心较多，并行编译优势明显")
        elif self.max_cores >= 4:
            print("💻 您的系统有足够的CPU核心支持并行编译")
        else:
            print("💻 您的系统CPU核心较少，并行编译效果可能有限")

def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--quick":
        # 快速测试模式，只测试当前推荐配置
        project_root = Path(__file__).parent.parent
        benchmark = BuildBenchmark(project_root)
        
        print("⚡ 快速编译测试")
        print("=" * 40)
        
        # 测试推荐配置
        jobs = benchmark.max_cores
        
        print(f"测试 {jobs} 线程并行编译...")
        
        # Bootloader
        benchmark.clean_project(benchmark.bootloader_dir)
        success, duration = benchmark.build_project(benchmark.bootloader_dir, jobs)
        print(f"Bootloader: {duration:.2f}秒 {'✅' if success else '❌'}")
        
        # Application
        benchmark.clean_project(benchmark.application_dir)
        success, duration = benchmark.build_project(benchmark.application_dir, jobs)
        print(f"Application: {duration:.2f}秒 {'✅' if success else '❌'}")
        
    else:
        # 完整性能测试
        project_root = Path(__file__).parent.parent
        benchmark = BuildBenchmark(project_root)
        benchmark.run_full_benchmark()

if __name__ == "__main__":
    main() 