/**
  ******************************************************************************
  * @file    firmware_upgrade.c
  * @brief   Bootloader固件升级管理实现
  ******************************************************************************
  */

#include "firmware_upgrade.h"
#include "qspi-w25q64.h"
#include <string.h>

/* CRC32查找表 */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/* 静态变量 */
static upgrade_metadata_t g_metadata;
static bool g_metadata_valid = false;

/* 内部函数声明 */
static void upgrade_init_default_metadata(upgrade_metadata_t *metadata);
static uint32_t upgrade_calculate_metadata_crc32(const upgrade_metadata_t *metadata);
static bool upgrade_is_metadata_valid(const upgrade_metadata_t *metadata);

/**
 * @brief 计算CRC32
 */
uint32_t upgrade_calculate_crc32(const uint8_t *data, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

/**
 * @brief 计算元数据CRC32
 */
static uint32_t upgrade_calculate_metadata_crc32(const upgrade_metadata_t *metadata)
{
    return upgrade_calculate_crc32((const uint8_t*)metadata, 
                                   sizeof(upgrade_metadata_t) - sizeof(uint32_t));
}

/**
 * @brief 验证元数据有效性
 */
static bool upgrade_is_metadata_valid(const upgrade_metadata_t *metadata)
{
    if (metadata->magic != UPGRADE_METADATA_MAGIC) {
        return false;
    }
    
    if (metadata->version != UPGRADE_METADATA_VERSION) {
        return false;
    }
    
    uint32_t calc_crc = upgrade_calculate_metadata_crc32(metadata);
    if (calc_crc != metadata->crc32) {
        return false;
    }
    
    return true;
}

/**
 * @brief 初始化默认元数据
 */
static void upgrade_init_default_metadata(upgrade_metadata_t *metadata)
{
    memset(metadata, 0, sizeof(upgrade_metadata_t));
    
    metadata->magic = UPGRADE_METADATA_MAGIC;
    metadata->version = UPGRADE_METADATA_VERSION;
    metadata->active_slot = UPGRADE_SLOT_A;
    metadata->upgrade_slot = UPGRADE_SLOT_B;
    metadata->upgrade_type = UPGRADE_TYPE_FIRMWARE;
    metadata->upgrade_status = UPGRADE_STATUS_IDLE;
    metadata->upgrade_attempts = 0;
    metadata->max_attempts = UPGRADE_MAX_ATTEMPTS;
    metadata->timestamp = 0;
    
    /* 初始化组件信息 */
    metadata->application.checksum_type = CHECKSUM_TYPE_CRC32;
    metadata->web_resources.checksum_type = CHECKSUM_TYPE_CRC32;
    metadata->config.checksum_type = CHECKSUM_TYPE_CRC32;
    metadata->adc_mapping.checksum_type = CHECKSUM_TYPE_CRC32;
    
    metadata->crc32 = upgrade_calculate_metadata_crc32(metadata);
}

/**
 * @brief 初始化升级管理器
 */
int upgrade_manager_init(void)
{
    /* 读取元数据 */
    if (upgrade_read_metadata(&g_metadata) == 0) {
        g_metadata_valid = true;
        return 0;
    }
    
    /* 如果读取失败，初始化默认元数据 */
    upgrade_init_default_metadata(&g_metadata);
    
    /* 写入默认元数据 */
    if (upgrade_write_metadata(&g_metadata) == 0) {
        g_metadata_valid = true;
        return 0;
    }
    
    return -1;
}

/**
 * @brief 读取升级元数据
 */
int upgrade_read_metadata(upgrade_metadata_t *metadata)
{
    if (QSPI_W25Qxx_ReadBuffer((uint8_t*)metadata, UPGRADE_METADATA_ADDR, sizeof(upgrade_metadata_t)) != QSPI_W25Qxx_OK) {
        return -1;
    }
    
    if (!upgrade_is_metadata_valid(metadata)) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 写入升级元数据
 */
int upgrade_write_metadata(const upgrade_metadata_t *metadata)
{
    upgrade_metadata_t temp_metadata = *metadata;
    temp_metadata.crc32 = upgrade_calculate_metadata_crc32(&temp_metadata);
    
    /* 擦除扇区 */
    if (QSPI_W25Qxx_SectorErase(UPGRADE_METADATA_ADDR) != QSPI_W25Qxx_OK) {
        return -1;
    }
    
    /* 写入数据 */
    if (QSPI_W25Qxx_WriteBuffer((uint8_t*)&temp_metadata, UPGRADE_METADATA_ADDR, sizeof(upgrade_metadata_t)) != QSPI_W25Qxx_OK) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 获取当前活动槽位
 */
upgrade_slot_t upgrade_get_active_slot(void)
{
    if (!g_metadata_valid) {
        return UPGRADE_SLOT_A; // 默认返回槽位A
    }
    
    return g_metadata.active_slot;
}

/**
 * @brief 检查是否有待处理的升级
 */
bool upgrade_check_pending(void)
{
    if (!g_metadata_valid) {
        return false;
    }
    
    return (g_metadata.upgrade_status == UPGRADE_STATUS_PENDING ||
            g_metadata.upgrade_status == UPGRADE_STATUS_DOWNLOADED ||
            g_metadata.upgrade_status == UPGRADE_STATUS_VERIFIED);
}

/**
 * @brief 获取槽位地址
 */
uint32_t upgrade_get_slot_address(upgrade_slot_t slot, upgrade_type_t type)
{
    uint32_t base_addr;
    
    switch (type) {
        case UPGRADE_TYPE_APPLICATION_ONLY:
            base_addr = (slot == UPGRADE_SLOT_A) ? APPLICATION_SLOT_A_ADDR : APPLICATION_SLOT_B_ADDR;
            break;
        case UPGRADE_TYPE_WEB_ONLY:
            base_addr = (slot == UPGRADE_SLOT_A) ? WEB_RESOURCES_SLOT_A_ADDR : WEB_RESOURCES_SLOT_B_ADDR;
            break;
        case UPGRADE_TYPE_CONFIG_ONLY:
            base_addr = (slot == UPGRADE_SLOT_A) ? CONFIG_SLOT_A_ADDR : CONFIG_SLOT_B_ADDR;
            break;
        case UPGRADE_TYPE_ADC_ONLY:
            base_addr = (slot == UPGRADE_SLOT_A) ? ADC_MAPPING_SLOT_A_ADDR : ADC_MAPPING_SLOT_B_ADDR;
            break;
        default:
            base_addr = (slot == UPGRADE_SLOT_A) ? APPLICATION_SLOT_A_ADDR : APPLICATION_SLOT_B_ADDR;
            break;
    }
    
    return base_addr;
}

/**
 * @brief 验证组件完整性
 */
int upgrade_verify_component(upgrade_slot_t slot, upgrade_type_t type, const component_upgrade_info_t *info)
{
    uint32_t addr = upgrade_get_slot_address(slot, type);
    uint32_t calc_checksum;
    uint8_t buffer[1024];
    uint32_t remaining = info->size;
    uint32_t offset = 0;
    
    if (info->checksum_type != CHECKSUM_TYPE_CRC32) {
        return -1; // 目前只支持CRC32
    }
    
    calc_checksum = 0xFFFFFFFF;
    
    while (remaining > 0) {
        uint32_t read_size = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        
        if (QSPI_W25Qxx_ReadBuffer(buffer, addr + offset, read_size) != QSPI_W25Qxx_OK) {
            return -1;
        }
        
        for (uint32_t i = 0; i < read_size; i++) {
            calc_checksum = crc32_table[(calc_checksum ^ buffer[i]) & 0xFF] ^ (calc_checksum >> 8);
        }
        
        remaining -= read_size;
        offset += read_size;
    }
    
    calc_checksum ^= 0xFFFFFFFF;
    
    return (calc_checksum == info->checksum) ? 0 : -1;
}

/**
 * @brief 切换活动槽位
 */
int upgrade_switch_slot(upgrade_slot_t slot)
{
    if (!g_metadata_valid) {
        return -1;
    }
    
    g_metadata.active_slot = slot;
    g_metadata.upgrade_status = UPGRADE_STATUS_SUCCESS;
    
    return upgrade_write_metadata(&g_metadata);
}

/**
 * @brief 执行升级流程
 */
int upgrade_process_pending(void)
{
    if (!g_metadata_valid || !upgrade_check_pending()) {
        return -1;
    }
    
    /* 检查升级尝试次数 */
    if (g_metadata.upgrade_attempts >= g_metadata.max_attempts) {
        /* 超过最大尝试次数，回滚 */
        return upgrade_rollback();
    }
    
    /* 增加尝试次数 */
    g_metadata.upgrade_attempts++;
    g_metadata.upgrade_status = UPGRADE_STATUS_VERIFYING;
    upgrade_write_metadata(&g_metadata);
    
    /* 验证组件 */
    bool all_verified = true;
    
    if (g_metadata.upgrade_type == UPGRADE_TYPE_FIRMWARE ||
        g_metadata.upgrade_type == UPGRADE_TYPE_APPLICATION_ONLY) {
        if (upgrade_verify_component(g_metadata.upgrade_slot, UPGRADE_TYPE_APPLICATION_ONLY, 
                                    &g_metadata.application) != 0) {
            all_verified = false;
        }
    }
    
    if (g_metadata.upgrade_type == UPGRADE_TYPE_FIRMWARE ||
        g_metadata.upgrade_type == UPGRADE_TYPE_WEB_ONLY) {
        if (upgrade_verify_component(g_metadata.upgrade_slot, UPGRADE_TYPE_WEB_ONLY, 
                                    &g_metadata.web_resources) != 0) {
            all_verified = false;
        }
    }
    
    if (g_metadata.upgrade_type == UPGRADE_TYPE_RESOURCES ||
        g_metadata.upgrade_type == UPGRADE_TYPE_CONFIG_ONLY) {
        if (upgrade_verify_component(g_metadata.upgrade_slot, UPGRADE_TYPE_CONFIG_ONLY, 
                                    &g_metadata.config) != 0) {
            all_verified = false;
        }
    }
    
    if (g_metadata.upgrade_type == UPGRADE_TYPE_RESOURCES ||
        g_metadata.upgrade_type == UPGRADE_TYPE_ADC_ONLY) {
        if (upgrade_verify_component(g_metadata.upgrade_slot, UPGRADE_TYPE_ADC_ONLY, 
                                    &g_metadata.adc_mapping) != 0) {
            all_verified = false;
        }
    }
    
    if (!all_verified) {
        g_metadata.upgrade_status = UPGRADE_STATUS_FAILED;
        upgrade_write_metadata(&g_metadata);
        return -1;
    }
    
    /* 验证成功，切换槽位 */
    g_metadata.upgrade_status = UPGRADE_STATUS_INSTALLING;
    upgrade_write_metadata(&g_metadata);
    
    return upgrade_switch_slot(g_metadata.upgrade_slot);
}

/**
 * @brief 回滚到上一个版本
 */
int upgrade_rollback(void)
{
    if (!g_metadata_valid) {
        return -1;
    }
    
    /* 切换到另一个槽位 */
    upgrade_slot_t rollback_slot = (g_metadata.active_slot == UPGRADE_SLOT_A) ? 
                                   UPGRADE_SLOT_B : UPGRADE_SLOT_A;
    
    g_metadata.active_slot = rollback_slot;
    g_metadata.upgrade_status = UPGRADE_STATUS_ROLLBACK;
    g_metadata.upgrade_attempts = 0;
    
    return upgrade_write_metadata(&g_metadata);
}

/**
 * @brief 启动确认
 */
int upgrade_confirm_boot(void)
{
    if (!g_metadata_valid) {
        return -1;
    }
    
    if (g_metadata.upgrade_status == UPGRADE_STATUS_SUCCESS) {
        /* 启动确认成功，重置升级尝试计数 */
        g_metadata.upgrade_attempts = 0;
        g_metadata.upgrade_status = UPGRADE_STATUS_IDLE;
        return upgrade_write_metadata(&g_metadata);
    }
    
    return 0;
}

/**
 * @brief 请求升级
 */
int upgrade_request(const upgrade_request_t *request)
{
    if (!g_metadata_valid) {
        return -1;
    }
    
    if (g_metadata.upgrade_status != UPGRADE_STATUS_IDLE) {
        return -1; // 已有升级在进行中
    }
    
    /* 设置升级信息 */
    g_metadata.upgrade_type = request->type;
    g_metadata.upgrade_slot = (g_metadata.active_slot == UPGRADE_SLOT_A) ? 
                              UPGRADE_SLOT_B : UPGRADE_SLOT_A;
    g_metadata.upgrade_status = UPGRADE_STATUS_PENDING;
    g_metadata.upgrade_attempts = 0;
    
    /* 复制组件信息 */
    g_metadata.application = request->components[0];
    g_metadata.web_resources = request->components[1];
    g_metadata.config = request->components[2];
    g_metadata.adc_mapping = request->components[3];
    
    return upgrade_write_metadata(&g_metadata);
}

/**
 * @brief 获取升级状态
 */
upgrade_status_t upgrade_get_status(void)
{
    if (!g_metadata_valid) {
        return UPGRADE_STATUS_FAILED;
    }
    
    return g_metadata.upgrade_status;
}

/**
 * @brief 获取升级进度
 */
int upgrade_get_progress(upgrade_type_t type)
{
    if (!g_metadata_valid) {
        return -1;
    }
    
    /* 简单的进度计算，实际应用中可以更精确 */
    switch (g_metadata.upgrade_status) {
        case UPGRADE_STATUS_IDLE:
            return 0;
        case UPGRADE_STATUS_DOWNLOADING:
            return 25;
        case UPGRADE_STATUS_DOWNLOADED:
            return 50;
        case UPGRADE_STATUS_VERIFYING:
            return 75;
        case UPGRADE_STATUS_VERIFIED:
        case UPGRADE_STATUS_INSTALLING:
            return 90;
        case UPGRADE_STATUS_SUCCESS:
            return 100;
        case UPGRADE_STATUS_FAILED:
        case UPGRADE_STATUS_ROLLBACK:
            return -1;
        default:
            return 0;
    }
} 