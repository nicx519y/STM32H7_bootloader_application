/**
  ******************************************************************************
  * @file    firmware_upgrade.h
  * @brief   Bootloader固件升级管理头文件
  ******************************************************************************
  */

#ifndef __FIRMWARE_UPGRADE_H
#define __FIRMWARE_UPGRADE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "board_cfg.h"

/* 升级状态枚举 */
typedef enum {
    UPGRADE_STATUS_IDLE = 0x00,           // 空闲状态
    UPGRADE_STATUS_DOWNLOADING = 0x01,    // 下载中
    UPGRADE_STATUS_DOWNLOADED = 0x02,     // 下载完成
    UPGRADE_STATUS_VERIFYING = 0x03,      // 校验中
    UPGRADE_STATUS_VERIFIED = 0x04,       // 校验完成
    UPGRADE_STATUS_INSTALLING = 0x05,     // 安装中
    UPGRADE_STATUS_INSTALLED = 0x06,      // 安装完成
    UPGRADE_STATUS_REBOOTING = 0x07,      // 重启中
    UPGRADE_STATUS_SUCCESS = 0x08,        // 升级成功
    UPGRADE_STATUS_FAILED = 0x09,         // 升级失败
    UPGRADE_STATUS_ROLLBACK = 0x0A,       // 回滚
    UPGRADE_STATUS_PENDING = 0x0B         // 等待重启升级
} upgrade_status_t;

/* 升级类型枚举 */
typedef enum {
    UPGRADE_TYPE_FIRMWARE = 0x01,         // 固件包升级 (Application + Web Resources)
    UPGRADE_TYPE_RESOURCES = 0x02,        // 资源包升级 (Config + ADC Mapping)
    UPGRADE_TYPE_APPLICATION_ONLY = 0x03, // 仅Application升级
    UPGRADE_TYPE_WEB_ONLY = 0x04,         // 仅Web Resources升级
    UPGRADE_TYPE_CONFIG_ONLY = 0x05,      // 仅Config升级
    UPGRADE_TYPE_ADC_ONLY = 0x06          // 仅ADC Mapping升级
} upgrade_type_t;

/* 槽位枚举 */
typedef enum {
    UPGRADE_SLOT_A = 0x00,                // 槽位A
    UPGRADE_SLOT_B = 0x01                 // 槽位B
} upgrade_slot_t;

/* 校验类型枚举 */
typedef enum {
    CHECKSUM_TYPE_CRC32 = 0x01,           // CRC32校验
    CHECKSUM_TYPE_MD5 = 0x02,             // MD5校验
    CHECKSUM_TYPE_SHA256 = 0x03           // SHA256校验
} checksum_type_t;

/* 组件升级信息结构体 */
typedef struct {
    uint32_t size;                        // 组件大小
    uint32_t version;                     // 组件版本
    checksum_type_t checksum_type;        // 校验类型
    uint32_t checksum;                    // 校验值 (对于CRC32)
    uint8_t hash[32];                     // 哈希值 (对于MD5/SHA256)
    upgrade_status_t status;              // 升级状态
} component_upgrade_info_t;

/* 升级元数据结构体 */
typedef struct {
    uint32_t magic;                       // 魔数 0x55AA55AA
    uint32_t version;                     // 元数据版本
    upgrade_slot_t active_slot;           // 当前活动槽位
    upgrade_slot_t upgrade_slot;          // 升级目标槽位
    upgrade_type_t upgrade_type;          // 升级类型
    upgrade_status_t upgrade_status;      // 总体升级状态
    uint32_t upgrade_attempts;            // 升级尝试次数
    uint32_t max_attempts;                // 最大尝试次数
    uint32_t timestamp;                   // 升级时间戳
    
    /* 各组件升级信息 */
    component_upgrade_info_t application; // Application组件
    component_upgrade_info_t web_resources; // Web Resources组件
    component_upgrade_info_t config;      // Config组件
    component_upgrade_info_t adc_mapping; // ADC Mapping组件
    
    uint32_t reserved[16];                // 保留字段
    uint32_t crc32;                       // 元数据CRC32校验
} upgrade_metadata_t;

/* 启动验证结构体 */
typedef struct {
    bool is_valid;                        // 启动是否成功
    uint32_t boot_count;                  // 启动次数
    uint32_t last_boot_time;              // 最后启动时间
    upgrade_status_t last_status;         // 最后状态
} boot_validation_t;

/* 升级相关宏定义 */
#define UPGRADE_METADATA_MAGIC          0x55AA55AA    // 升级元数据魔数
#define UPGRADE_METADATA_VERSION        0x00000001    // 升级元数据版本
#define UPGRADE_MAX_ATTEMPTS            3             // 最大升级尝试次数

/* 存储器地址定义 (基于0x90000000 QSPI基地址) */
#define QSPI_BASE_ADDR                  0x90000000

/* 元数据存储地址 (第一个扇区) */
#define UPGRADE_METADATA_ADDR           (QSPI_BASE_ADDR + 0x000000)    // 0x90000000

/* 槽位A地址定义 */
#define APPLICATION_SLOT_A_ADDR         (QSPI_BASE_ADDR + 0x010000)    // 0x90010000 - 1MB
#define WEB_RESOURCES_SLOT_A_ADDR       (QSPI_BASE_ADDR + 0x110000)    // 0x90110000 - 512KB  
#define CONFIG_SLOT_A_ADDR              (QSPI_BASE_ADDR + 0x190000)    // 0x90190000 - 64KB
#define ADC_MAPPING_SLOT_A_ADDR         (QSPI_BASE_ADDR + 0x1A0000)    // 0x901A0000 - 64KB

/* 槽位B地址定义 */  
#define APPLICATION_SLOT_B_ADDR         (QSPI_BASE_ADDR + 0x1B0000)    // 0x901B0000 - 1MB
#define WEB_RESOURCES_SLOT_B_ADDR       (QSPI_BASE_ADDR + 0x2B0000)    // 0x902B0000 - 512KB
#define CONFIG_SLOT_B_ADDR              (QSPI_BASE_ADDR + 0x330000)    // 0x90330000 - 64KB
#define ADC_MAPPING_SLOT_B_ADDR         (QSPI_BASE_ADDR + 0x340000)    // 0x90340000 - 64KB

/* Bootloader专用函数 - 升级管理 */

/**
 * @brief 初始化升级管理器
 * @retval 0:成功 -1:失败
 */
int upgrade_manager_init(void);

/**
 * @brief 读取升级元数据
 * @param metadata 元数据指针
 * @retval 0:成功 -1:失败
 */
int upgrade_read_metadata(upgrade_metadata_t *metadata);

/**
 * @brief 写入升级元数据
 * @param metadata 元数据指针
 * @retval 0:成功 -1:失败
 */
int upgrade_write_metadata(const upgrade_metadata_t *metadata);

/**
 * @brief 获取当前活动槽位
 * @retval 活动槽位
 */
upgrade_slot_t upgrade_get_active_slot(void);

/**
 * @brief 检查是否有待处理的升级
 * @retval true:有升级待处理 false:无升级
 */
bool upgrade_check_pending(void);

/**
 * @brief 执行升级流程
 * @retval 0:成功 -1:失败
 */
int upgrade_process_pending(void);

/**
 * @brief 验证组件完整性
 * @param slot 槽位
 * @param type 组件类型
 * @param info 组件信息
 * @retval 0:验证成功 -1:验证失败
 */
int upgrade_verify_component(upgrade_slot_t slot, upgrade_type_t type, const component_upgrade_info_t *info);

/**
 * @brief 切换活动槽位
 * @param slot 目标槽位
 * @retval 0:成功 -1:失败
 */
int upgrade_switch_slot(upgrade_slot_t slot);

/**
 * @brief 回滚到上一个版本
 * @retval 0:成功 -1:失败
 */
int upgrade_rollback(void);

/**
 * @brief 获取槽位地址
 * @param slot 槽位
 * @param type 组件类型
 * @retval 地址
 */
uint32_t upgrade_get_slot_address(upgrade_slot_t slot, upgrade_type_t type);

/**
 * @brief 启动确认（由application调用）
 * @retval 0:成功 -1:失败
 */
int upgrade_confirm_boot(void);

/**
 * @brief 计算CRC32
 * @param data 数据指针
 * @param size 数据大小
 * @retval CRC32值
 */
uint32_t upgrade_calculate_crc32(const uint8_t *data, uint32_t size);

/* Application专用函数 - 升级请求接口 */

/**
 * @brief 升级请求结构体
 */
typedef struct {
    upgrade_type_t type;                  // 升级类型
    component_upgrade_info_t components[4]; // 组件信息数组 [app, web, config, adc]
} upgrade_request_t;

/**
 * @brief 请求升级（由application调用）
 * @param request 升级请求
 * @retval 0:成功 -1:失败
 */
int upgrade_request(const upgrade_request_t *request);

/**
 * @brief 获取升级状态（由application调用）
 * @retval 升级状态
 */
upgrade_status_t upgrade_get_status(void);

/**
 * @brief 获取升级进度（由application调用）
 * @param type 组件类型
 * @retval 进度百分比 (0-100) 或 -1:错误
 */
int upgrade_get_progress(upgrade_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* __FIRMWARE_UPGRADE_H */ 