# H750固件升级包格式说明

## 概述

本文档描述了H750项目固件升级包的格式规范，包括包结构、组件类型、打包工具使用方法等。

## 升级包格式

### 包文件结构

```
升级包文件 (.upg)
├── 包头 (32字节)
├── 元数据大小 (4字节)
├── 元数据 (JSON格式)
├── 组件1大小 (4字节)
├── 组件1数据
├── 组件1填充 (4字节对齐)
├── 组件2大小 (4字节)
├── 组件2数据
├── 组件2填充 (4字节对齐)
└── ...
```

### 包头格式

```c
typedef struct {
    uint32_t magic;           // 魔数: "H750" (0x48373530)
    uint32_t version;         // 包版本号
    uint32_t upgrade_type;    // 升级类型
    uint64_t timestamp;       // 构建时间戳
    uint32_t component_count; // 组件数量
    uint8_t reserved[12];     // 保留字段
} upgrade_package_header_t;
```

### 升级类型

| 类型代码 | 名称 | 包含组件 | 说明 |
|---------|------|----------|------|
| 0x01 | FIRMWARE | Application + Web Resources | 固件包，包含核心程序和网页资源 |
| 0x02 | RESOURCES | Config + ADC Mapping | 资源包，包含配置文件 |
| 0x03 | APPLICATION | Application | 仅核心程序 |
| 0x04 | WEB | Web Resources | 仅网页资源 |
| 0x05 | CONFIG | Config | 仅设备配置 |
| 0x06 | ADC | ADC Mapping | 仅ADC映射配置 |

### 元数据格式

元数据使用JSON格式，包含以下信息：

```json
{
  "package_info": {
    "version": "1.0.0",
    "type": "FIRMWARE",
    "upgrade_type_code": 1,
    "firmware_version": "2.1.0",
    "timestamp": 1703123456,
    "build_time": "2023-12-21T10:30:56"
  },
  "components": [
    {
      "name": "application",
      "size": 786432,
      "version": "2.1.0",
      "checksum_type": "CRC32",
      "checksum": 2864434397,
      "file_path": "/path/to/application.bin"
    },
    {
      "name": "web_resources",
      "size": 624128,
      "version": "2.1.0",
      "checksum_type": "CRC32",
      "checksum": 1572864935,
      "file_path": "/path/to/web_resources.bin"
    }
  ],
  "checksum_info": {
    "algorithm": "CRC32",
    "package_crc32": 0
  }
}
```

## 组件规格

### 大小限制

| 组件 | 最大大小 | 槽位大小 | 说明 |
|------|----------|----------|------|
| Application | 1MB | 1MB | 主程序代码 |
| Web Resources | 1.5MB | 1.5MB | 网页文件压缩包 |
| Config | 32KB | 32KB | 设备配置数据 |
| ADC Mapping | 64KB | 64KB | ADC通道映射配置 |

## QSPI Flash地址分配

### W25Q64 (8MB) 地址映射
```
地址范围                   大小     说明
0x90000000-0x90010000     64KB     升级元数据区

【槽位A】
0x90010000-0x90110000     1MB      Application Slot A  
0x90110000-0x90290000     1.5MB    Web Resources Slot A
0x90290000-0x902A0000     64KB     Config Slot A
0x902A0000-0x902B0000     64KB     ADC Mapping Slot A

【槽位B】  
0x902B0000-0x903B0000     1MB      Application Slot B
0x903B0000-0x904B0000     1.5MB    Web Resources Slot B  
0x904B0000-0x904C0000     64KB     Config Slot B
0x904C0000-0x904D0000     64KB     ADC Mapping Slot B

0x904D0000-0x90800000     3.2MB    未使用区域
```

## 打包工具使用

### 安装依赖

```bash
pip install binascii
```

### 基本用法

#### 创建固件包
```bash
python3 tools/package_upgrade.py firmware \
    --application build/application.bin \
    --web-resources build/web_resources.bin \
    --version 2.1.0 \
    --output packages/
```

#### 创建资源包
```bash
python3 tools/package_upgrade.py resources \
    --config config/device_config.bin \
    --adc-mapping config/adc_mapping.bin \
    --version 2.1.0 \
    --output packages/
```

#### 创建单组件包
```bash
# Application包
python3 tools/package_upgrade.py application build/application.bin \
    --version 2.1.0 --output packages/

# Web Resources包
python3 tools/package_upgrade.py web build/web_resources.bin \
    --version 2.1.0 --output packages/

# Config包
python3 tools/package_upgrade.py config config/device_config.bin \
    --version 2.1.0 --output packages/

# ADC Mapping包
python3 tools/package_upgrade.py adc config/adc_mapping.bin \
    --version 2.1.0 --output packages/
```

### 自动化构建

使用提供的构建脚本进行自动化构建和打包：

```bash
# 完整发布流程
./tools/build_and_package.sh -v 2.1.0 --release full-release

# 仅构建
./tools/build_and_package.sh build-all

# 仅打包
./tools/build_and_package.sh package-all

# 清理
./tools/build_and_package.sh clean
```

## 升级流程

### 1. Application端流程

```c
#include "upgrade_client.h"

// 初始化升级客户端
upgrade_client_init();

// 从网络或其他途径获取升级包
uint8_t *package_data = get_upgrade_package();
uint32_t package_size = get_package_size();

// 开始升级
if (upgrade_from_memory(package_data, package_size, progress_callback) == 0) {
    // 重启进行升级
    upgrade_client_reboot();
}
```

### 2. Bootloader端流程

```c
#include "firmware_upgrade.h"

// 初始化升级管理器
upgrade_manager_init();

// 检查是否有待处理的升级
if (upgrade_check_pending()) {
    // 执行升级流程
    if (upgrade_process_pending() == 0) {
        // 升级成功，继续启动
    } else {
        // 升级失败，可能需要回滚
        upgrade_rollback();
    }
}

// 获取当前活动槽位并启动
upgrade_slot_t active_slot = upgrade_get_active_slot();
// 跳转到相应槽位的application
```

### 3. 升级确认

Application启动后需要确认升级成功：

```c
void application_main(void) {
    // 其他初始化...
    
    // 确认启动成功
    upgrade_client_confirm_boot();
    
    // 继续正常业务逻辑...
}
```

## 安全机制

### 1. 完整性校验
- 每个组件使用CRC32校验
- 升级包整体校验
- 写入后重新校验

### 2. 回滚机制
- 升级失败自动回滚到之前版本
- 最大升级尝试次数限制
- 启动确认机制防止无限回滚

### 3. 双槽架构
- A/B槽位设计，确保有可用版本
- 零停机时间升级
- 原子性切换

## 错误处理

### 常见错误码

| 错误 | 说明 | 解决方法 |
|------|------|----------|
| -1 | 一般错误 | 检查日志详细信息 |
| 包头校验失败 | 升级包损坏 | 重新下载升级包 |
| 组件大小超限 | 组件过大 | 检查组件是否正确 |
| Flash写入失败 | 硬件问题 | 检查Flash连接 |
| CRC校验失败 | 数据损坏 | 重新传输数据 |

### 故障恢复

1. **升级中断恢复**：重启后bootloader会检查升级状态并继续
2. **数据损坏恢复**：自动回滚到上一个可用版本
3. **Flash故障恢复**：使用备用槽位的版本

## 最佳实践

### 1. 版本管理
- 使用语义化版本号 (如 2.1.0)
- 保持版本号一致性
- 记录版本变更历史

### 2. 测试流程
- 在开发环境充分测试
- 验证升级包完整性
- 测试回滚机制

### 3. 发布流程
- 使用自动化构建脚本
- 生成发布说明文档
- 保留多个版本的升级包

### 4. 监控和日志
- 记录升级过程日志
- 监控升级成功率
- 统计组件使用情况

## 工具链

### 构建工具
- `tools/package_upgrade.py`: 升级包打包工具
- `tools/build_and_package.sh`: 自动化构建脚本

### 调试工具
- CRC32计算器
- 升级包解析器
- Flash内容查看器

## FAQ

**Q: 升级包最大支持多大？**
A: 理论上无限制，但建议单个升级包不超过4MB。

**Q: 如何验证升级包的有效性？**
A: 使用`upgrade_parse_package_header()`函数验证包头和CRC32。

**Q: 升级失败如何恢复？**
A: 系统会自动回滚到之前的可用版本，无需手动干预。

**Q: 是否支持增量升级？**
A: 当前版本不支持，所有升级都是全量替换。

**Q: 如何自定义升级流程？**
A: 可以修改`upgrade_client.c`中的逻辑，或实现自定义的进度回调函数。 