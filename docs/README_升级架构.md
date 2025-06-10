# STM32H750 双槽升级架构说明

## 📋 架构概述

本项目采用**双槽升级架构**，实现STM32H750固件的安全升级。由于STM32H750内部Flash只有128KB，Application必须运行在RAM中。

### 核心设计
- **Bootloader**: 运行在内部Flash (128KB)，负责升级管理和Application加载
- **Application**: 存储在QSPI Flash，加载到RAM (512KB) 中执行
- **双槽机制**: Slot A/B，支持安全回滚
- **组件化升级**: 支持Application、Web Resources、Config、ADC Mapping独立升级

## 🗂️ 存储器布局

### 内部Flash (128KB)
```
0x08000000 - 0x0801FFFF: Bootloader (128KB)
```

### QSPI Flash (8MB) - W25Q64
```
地址范围                   大小    说明
0x90000000-0x90010000     64KB    升级元数据区

【槽位A】  
0x90010000-0x90110000     1MB     Application Slot A
0x90110000-0x90290000     1.5MB   Web Resources Slot A  
0x90290000-0x902A0000     64KB    Config Slot A
0x902A0000-0x902B0000     64KB    ADC Mapping Slot A

【槽位B】
0x902B0000-0x903B0000     1MB     Application Slot B
0x903B0000-0x904B0000     1.5MB   Web Resources Slot B
0x904B0000-0x904C0000     64KB    Config Slot B  
0x904C0000-0x904D0000     64KB    ADC Mapping Slot B

0x904D0000-0x90800000     3.2MB   未使用区域
```

### RAM (512KB)
```
0x24000000 - 0x2407FFFF: Application代码和数据 (512KB)
0x00000000 - 0x0000FFFF: ITCMRAM - 向量表 (64KB)
```

## 🔄 启动流程

### 1. Bootloader启动
1. **初始化系统**: 时钟、QSPI、GPIO等
2. **读取升级元数据**: 检查是否有升级任务
3. **处理升级**: 如有待处理升级，执行验证和安装
4. **选择槽位**: 根据元数据选择活动槽位
5. **加载Application**: 从QSPI Flash加载到RAM
6. **跳转执行**: 跳转到RAM中的Application

### 2. Application启动
1. **槽位检测**: 根据升级元数据确定当前槽位
2. **内存重定位**: 从QSPI Flash拷贝各段到RAM
3. **系统初始化**: MPU、时钟、外设等
4. **启动确认**: 向Bootloader确认成功启动
5. **正常运行**: 执行主要功能

## 🛠️ 开发调试流程

### 快速调试
```bash
# 1. 使用调试脚本（推荐）
python debug_scripts/debug_setup.py full-flash

# 2. 或者手动分步
cd bootloader && make flash    # 烧录bootloader
cd application && make flash   # 烧录application
```

### 调试工具
- **Windows**: 运行 `debug.bat`
- **Linux**: 运行 `./debug.sh`

### 调试选项
1. **编译并烧录Bootloader** - 快速测试bootloader
2. **编译并烧录Application** - 快速测试application  
3. **完整烧录** - 烧录完整系统
4. **仅编译** - 验证代码无误

## 🚀 正常发版流程

### 1. 构建发版
```bash
python release_scripts/build_release.py 1.0.0
```

### 2. 发版输出
```
release/v1.0.0/
├── bootloader.bin          # Bootloader固件
├── application.bin         # Application固件
├── qspi_full.bin          # 完整QSPI镜像
├── upgrade_package.json   # 升级包
├── flash.sh              # 烧录脚本
└── version_info.json     # 版本信息
```

### 3. 生产烧录
```bash
cd release/v1.0.0
./flash.sh
```

## 🔧 升级机制

### 升级类型
- **FIRMWARE**: Application + Web Resources
- **RESOURCES**: Config + ADC Mapping  
- **APPLICATION_ONLY**: 仅Application
- **WEB_ONLY**: 仅Web Resources
- **CONFIG_ONLY**: 仅Config
- **ADC_ONLY**: 仅ADC Mapping

### 升级流程
1. **下载升级包**: Application通过网络下载
2. **写入非活动槽位**: 写入到Slot B (如当前是Slot A)
3. **请求升级**: 设置升级元数据，重启
4. **Bootloader验证**: 验证新固件完整性
5. **切换槽位**: 验证成功后切换到新槽位
6. **启动确认**: Application确认启动成功
7. **完成升级**: 标记升级状态为成功

### 安全机制
- **CRC32校验**: 确保数据完整性
- **最大重试次数**: 防止无限重启
- **自动回滚**: 新版本启动失败时自动回滚
- **双槽备份**: 始终保持一个可用版本

## 📁 项目结构

```
HBox_Git/
├── bootloader/             # Bootloader项目
│   ├── Core/
│   │   ├── Src/
│   │   │   └── firmware_upgrade.c  # 升级管理
│   │   └── Inc/
│   │       └── firmware_upgrade.h  # 升级接口
│   ├── Openocd_Script/
│   │   └── ST-LINK-FLASH.cfg      # 内部Flash烧录
│   └── Makefile
├── application/            # Application项目
│   ├── Core/
│   │   ├── Src/
│   │   │   └── upgrade_client.c    # 升级客户端
│   │   └── Inc/
│   │       ├── upgrade_client.h    # 升级客户端接口
│   │       └── board_cfg.h         # 板级配置
│   ├── Openocd_Script/
│   │   └── ST-LINK-QSPIFLASH.cfg  # QSPI Flash烧录
│   ├── startup_stm32h750xx.s       # 启动代码
│   ├── STM32H750XBHX_FLASH.ld     # 链接脚本
│   └── Makefile
├── release_scripts/        # 发版脚本
│   └── build_release.py
├── debug_scripts/          # 调试脚本
│   └── debug_setup.py
├── debug.bat              # Windows调试工具
├── debug.sh               # Linux调试工具
└── README_升级架构.md      # 本文档
```

## 🔍 常见问题

### Q: 为什么Application不能放在内部Flash？
A: STM32H750内部Flash只有128KB，而Application编译后通常超过200KB，必须使用外部QSPI Flash。

### Q: 直接烧录Application能运行吗？
A: 不能。Application依赖Bootloader创建的升级元数据来选择槽位。必须先烧录Bootloader，再创建包含元数据的QSPI镜像。

### Q: 如何调试RAM中的代码？
A: 推荐使用串口调试、LED指示等方法。如需JTAG调试，可以临时创建内部Flash调试版本。

### Q: 升级失败如何恢复？
A: 系统有自动回滚机制。如果新版本启动失败，会自动回滚到上一个工作版本。

### Q: 如何验证升级是否成功？
A: Application启动后会调用 `upgrade_confirm_boot()` 确认启动成功。如果超时未确认，系统会认为升级失败并回滚。

## 📚 API参考

### Bootloader API (firmware_upgrade.h)
- `upgrade_manager_init()` - 初始化升级管理器
- `upgrade_process_pending()` - 处理待升级任务
- `upgrade_get_active_slot()` - 获取当前活动槽位
- `upgrade_verify_component()` - 验证组件完整性

### Application API (upgrade_client.h)  
- `upgrade_client_init()` - 初始化升级客户端
- `upgrade_request()` - 请求升级
- `upgrade_write_component()` - 写入组件数据
- `upgrade_confirm_boot()` - 确认启动成功

---

> 💡 **提示**: 开发阶段建议使用调试脚本快速迭代，生产发版使用自动化构建脚本确保一致性。 