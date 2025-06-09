/**
  ******************************************************************************
  * @file    upgrade_client.h
  * @brief   Application端升级客户端接口
  ******************************************************************************
  */

#ifndef __UPGRADE_CLIENT_H
#define __UPGRADE_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 引用bootloader中的升级定义 */
typedef enum {
    UPGRADE_STATUS_IDLE = 0x00,
    UPGRADE_STATUS_DOWNLOADING = 0x01,
    UPGRADE_STATUS_DOWNLOADED = 0x02,
    UPGRADE_STATUS_VERIFYING = 0x03,
    UPGRADE_STATUS_VERIFIED = 0x04,
    UPGRADE_STATUS_INSTALLING = 0x05,
    UPGRADE_STATUS_INSTALLED = 0x06,
    UPGRADE_STATUS_REBOOTING = 0x07,
    UPGRADE_STATUS_SUCCESS = 0x08,
    UPGRADE_STATUS_FAILED = 0x09,
    UPGRADE_STATUS_ROLLBACK = 0x0A,
    UPGRADE_STATUS_PENDING = 0x0B
} upgrade_status_t;

typedef enum {
    UPGRADE_TYPE_FIRMWARE = 0x01,
    UPGRADE_TYPE_RESOURCES = 0x02,
    UPGRADE_TYPE_APPLICATION_ONLY = 0x03,
    UPGRADE_TYPE_WEB_ONLY = 0x04,
    UPGRADE_TYPE_CONFIG_ONLY = 0x05,
    UPGRADE_TYPE_ADC_ONLY = 0x06
} upgrade_type_t;

typedef enum {
    CHECKSUM_TYPE_CRC32 = 0x01,
    CHECKSUM_TYPE_MD5 = 0x02,
    CHECKSUM_TYPE_SHA256 = 0x03
} checksum_type_t;

typedef struct {
    uint32_t size;
    uint32_t version;
    checksum_type_t checksum_type;
    uint32_t checksum;
    uint8_t hash[32];
    upgrade_status_t status;
} component_upgrade_info_t;

typedef struct {
    upgrade_type_t type;
    component_upgrade_info_t components[4]; // [app, web, config, adc]
} upgrade_request_t;

/* 升级包解析结构 */
typedef struct {
    uint32_t magic;           // "H750"
    uint32_t version;         // 包版本
    uint32_t upgrade_type;    // 升级类型
    uint64_t timestamp;       // 时间戳
    uint32_t component_count; // 组件数量
    uint8_t reserved[12];     // 保留字段
} upgrade_package_header_t;

typedef struct {
    char name[32];            // 组件名称
    uint32_t size;            // 组件大小
    uint32_t version;         // 组件版本
    uint32_t checksum;        // CRC32校验值
    uint32_t offset;          // 在包中的偏移量
} upgrade_component_header_t;

/* 升级客户端API */

/**
 * @brief 初始化升级客户端
 * @retval 0:成功 -1:失败
 */
int upgrade_client_init(void);

/**
 * @brief 解析升级包头部
 * @param data 升级包数据
 * @param size 数据大小
 * @param header 输出的包头信息
 * @retval 0:成功 -1:失败
 */
int upgrade_parse_package_header(const uint8_t *data, uint32_t size, upgrade_package_header_t *header);

/**
 * @brief 开始升级流程
 * @param package_data 升级包数据
 * @param package_size 升级包大小
 * @retval 0:成功 -1:失败
 */
int upgrade_start_from_package(const uint8_t *package_data, uint32_t package_size);

/**
 * @brief 写入组件数据到Flash
 * @param component_name 组件名称
 * @param data 数据指针
 * @param size 数据大小
 * @param offset 写入偏移量
 * @retval 写入的字节数 或 -1:失败
 */
int upgrade_write_component_data(const char *component_name, const uint8_t *data, uint32_t size, uint32_t offset);

/**
 * @brief 完成组件写入并验证
 * @param component_name 组件名称
 * @param expected_checksum 期望的校验值
 * @retval 0:成功 -1:失败
 */
int upgrade_finish_component(const char *component_name, uint32_t expected_checksum);

/**
 * @brief 提交升级请求
 * @param request 升级请求
 * @retval 0:成功 -1:失败
 */
int upgrade_commit_request(const upgrade_request_t *request);

/**
 * @brief 获取升级状态
 * @retval 升级状态
 */
upgrade_status_t upgrade_client_get_status(void);

/**
 * @brief 获取升级进度
 * @retval 进度百分比 (0-100) 或 -1:错误
 */
int upgrade_client_get_progress(void);

/**
 * @brief 确认启动成功
 * @retval 0:成功 -1:失败
 */
int upgrade_client_confirm_boot(void);

/**
 * @brief 请求系统重启进行升级
 * @retval 无返回（系统重启）
 */
void upgrade_client_reboot(void);

/**
 * @brief 取消正在进行的升级
 * @retval 0:成功 -1:失败
 */
int upgrade_client_cancel(void);

/* HTTP/网络升级相关API */

/**
 * @brief 从URL下载并升级
 * @param url 升级包URL
 * @param progress_callback 进度回调函数
 * @retval 0:成功 -1:失败
 */
int upgrade_from_url(const char *url, void (*progress_callback)(int progress));

/**
 * @brief 从内存中的升级包进行升级
 * @param package_data 升级包数据
 * @param package_size 升级包大小
 * @param progress_callback 进度回调函数
 * @retval 0:成功 -1:失败
 */
int upgrade_from_memory(const uint8_t *package_data, uint32_t package_size, 
                        void (*progress_callback)(int progress));

/* 工具函数 */

/**
 * @brief 计算CRC32
 * @param data 数据指针
 * @param size 数据大小
 * @retval CRC32值
 */
uint32_t upgrade_client_crc32(const uint8_t *data, uint32_t size);

/**
 * @brief 获取组件在槽位中的地址
 * @param component_name 组件名称
 * @param is_backup_slot 是否为备用槽位
 * @retval 地址 或 0:错误
 */
uint32_t upgrade_get_component_address(const char *component_name, bool is_backup_slot);

/**
 * @brief 擦除组件所在的Flash扇区
 * @param component_name 组件名称
 * @param is_backup_slot 是否为备用槽位
 * @retval 0:成功 -1:失败
 */
int upgrade_erase_component_sectors(const char *component_name, bool is_backup_slot);

#ifdef __cplusplus
}
#endif

#endif /* __UPGRADE_CLIENT_H */ 