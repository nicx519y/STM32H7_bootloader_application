/**
  ******************************************************************************
  * @file    upgrade_client.c
  * @brief   Application端升级客户端实现
  ******************************************************************************
  */

#include "upgrade_client.h"
#include "qspi-w25q64.h"
#include <string.h>
#include <stdio.h>

/* 私有变量 */
static bool client_initialized = false;
static upgrade_request_t current_request;
static bool request_pending = false;

/* 私有函数声明 */
static uint32_t get_component_slot_address(const char *component_name, bool is_backup_slot);
static int write_upgrade_metadata(const upgrade_request_t *request);
static uint32_t calculate_crc32(const uint8_t *data, uint32_t size);

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

/* 地址定义 (与bootloader保持一致) */
#define QSPI_BASE_ADDR                  0x90000000
#define UPGRADE_METADATA_ADDR           (QSPI_BASE_ADDR + 0x000000)

/* 槽位A地址定义 */
#define APPLICATION_SLOT_A_ADDR         (QSPI_BASE_ADDR + 0x010000)
#define WEB_RESOURCES_SLOT_A_ADDR       (QSPI_BASE_ADDR + 0x110000)
#define CONFIG_SLOT_A_ADDR              (QSPI_BASE_ADDR + 0x290000)
#define ADC_MAPPING_SLOT_A_ADDR         (QSPI_BASE_ADDR + 0x1A0000)

/* 槽位B地址定义 */
#define APPLICATION_SLOT_B_ADDR         (QSPI_BASE_ADDR + 0x2B0000)
#define WEB_RESOURCES_SLOT_B_ADDR       (QSPI_BASE_ADDR + 0x3B0000)
#define CONFIG_SLOT_B_ADDR              (QSPI_BASE_ADDR + 0x4B0000)
#define ADC_MAPPING_SLOT_B_ADDR         (QSPI_BASE_ADDR + 0x340000)

/**
 * @brief 初始化升级客户端
 */
int upgrade_client_init(void)
{
    if (client_initialized) {
        return 0;
    }
    
    /* 初始化QSPI */
    if (QSPI_W25Qxx_Init() != QSPI_W25Qxx_OK) {
        printf("[UPGRADE] QSPI init failed\r\n");
        return -1;
    }
    
    client_initialized = true;
    request_pending = false;
    
    printf("[UPGRADE] Client initialized\r\n");
    return 0;
}

/**
 * @brief 解析升级包头部
 */
int upgrade_parse_package_header(const uint8_t *data, uint32_t size, upgrade_package_header_t *header)
{
    if (!data || !header || size < sizeof(upgrade_package_header_t)) {
        return -1;
    }
    
    memcpy(header, data, sizeof(upgrade_package_header_t));
    
    /* 验证魔数 */
    if (header->magic != 0x30353748) { // "H750"
        printf("[UPGRADE] Invalid package magic: 0x%08lX\r\n", header->magic);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 开始升级流程
 */
int upgrade_start_from_package(const uint8_t *package_data, uint32_t package_size)
{
    if (!client_initialized) {
        return -1;
    }
    
    upgrade_package_header_t header;
    if (upgrade_parse_package_header(package_data, package_size, &header) != 0) {
        return -1;
    }
    
    printf("[UPGRADE] Starting upgrade from package, type: %ld\r\n", header.upgrade_type);
    
    /* 准备升级请求 */
    memset(&current_request, 0, sizeof(current_request));
    current_request.type = (upgrade_type_t)header.upgrade_type;
    
    return 0;
}

/**
 * @brief 写入组件数据到Flash
 */
int upgrade_write_component_data(const char *component_name, const uint8_t *data, uint32_t size, uint32_t offset)
{
    if (!client_initialized || !component_name || !data || size == 0) {
        return -1;
    }
    
    uint32_t addr = get_component_slot_address(component_name, true); // 写入备用槽位
    if (addr == 0) {
        printf("[UPGRADE] Unknown component: %s\r\n", component_name);
        return -1;
    }
    
    addr += offset;
    
    if (QSPI_W25Qxx_WriteBuffer((uint8_t*)data, addr, size) != QSPI_W25Qxx_OK) {
        printf("[UPGRADE] Write failed for %s at 0x%08lX\r\n", component_name, addr);
        return -1;
    }
    
    printf("[UPGRADE] Written %ld bytes to %s at offset %ld\r\n", size, component_name, offset);
    return size;
}

/**
 * @brief 完成组件写入并验证
 */
int upgrade_finish_component(const char *component_name, uint32_t expected_checksum)
{
    if (!client_initialized || !component_name) {
        return -1;
    }
    
    printf("[UPGRADE] Finishing component %s, expected CRC: 0x%08lX\r\n", 
           component_name, expected_checksum);
    
    /* 这里可以添加验证逻辑 */
    return 0;
}

/**
 * @brief 提交升级请求
 */
int upgrade_commit_request(const upgrade_request_t *request)
{
    if (!client_initialized || !request) {
        return -1;
    }
    
    printf("[UPGRADE] Committing upgrade request, type: %d\r\n", request->type);
    
    /* 将升级请求写入元数据区域给bootloader */
    if (write_upgrade_metadata(request) != 0) {
        printf("[UPGRADE] Failed to write metadata\r\n");
        return -1;
    }
    
    current_request = *request;
    request_pending = true;
    
    printf("[UPGRADE] Upgrade request committed\r\n");
    return 0;
}

/**
 * @brief 获取升级状态
 */
upgrade_status_t upgrade_client_get_status(void)
{
    /* 这里应该从共享内存或元数据区域读取状态 */
    if (request_pending) {
        return UPGRADE_STATUS_PENDING;
    }
    return UPGRADE_STATUS_IDLE;
}

/**
 * @brief 获取升级进度
 */
int upgrade_client_get_progress(void)
{
    /* 这里应该从共享内存或元数据区域读取进度 */
    if (request_pending) {
        return 0; // 等待重启
    }
    return -1;
}

/**
 * @brief 确认启动成功
 */
int upgrade_client_confirm_boot(void)
{
    if (!client_initialized) {
        return -1;
    }
    
    printf("[UPGRADE] Boot confirmed\r\n");
    
    /* 这里应该调用bootloader的确认接口 */
    return 0;
}

/**
 * @brief 请求系统重启进行升级
 */
void upgrade_client_reboot(void)
{
    printf("[UPGRADE] Rebooting for upgrade...\r\n");
    
    /* 延迟一下确保消息输出 */
    for (volatile int i = 0; i < 1000000; i++);
    
    /* 软件复位 */
    NVIC_SystemReset();
}

/**
 * @brief 取消正在进行的升级
 */
int upgrade_client_cancel(void)
{
    if (!client_initialized) {
        return -1;
    }
    
    request_pending = false;
    memset(&current_request, 0, sizeof(current_request));
    
    printf("[UPGRADE] Upgrade cancelled\r\n");
    return 0;
}

/**
 * @brief 从URL下载并升级
 */
int upgrade_from_url(const char *url, void (*progress_callback)(int progress))
{
    if (!client_initialized || !url) {
        return -1;
    }
    
    printf("[UPGRADE] Download from URL not implemented: %s\r\n", url);
    /* TODO: 实现HTTP下载功能 */
    return -1;
}

/**
 * @brief 从内存中的升级包进行升级
 */
int upgrade_from_memory(const uint8_t *package_data, uint32_t package_size, 
                        void (*progress_callback)(int progress))
{
    if (!client_initialized || !package_data || package_size == 0) {
        return -1;
    }
    
    if (progress_callback) {
        progress_callback(0);
    }
    
    /* 解析升级包 */
    if (upgrade_start_from_package(package_data, package_size) != 0) {
        return -1;
    }
    
    if (progress_callback) {
        progress_callback(10);
    }
    
    /* TODO: 实现完整的升级包处理逻辑 */
    printf("[UPGRADE] Memory upgrade not fully implemented\r\n");
    
    if (progress_callback) {
        progress_callback(100);
    }
    
    return 0;
}

/**
 * @brief 计算CRC32
 */
uint32_t upgrade_client_crc32(const uint8_t *data, uint32_t size)
{
    return calculate_crc32(data, size);
}

/**
 * @brief 获取组件在槽位中的地址
 */
uint32_t upgrade_get_component_address(const char *component_name, bool is_backup_slot)
{
    return get_component_slot_address(component_name, is_backup_slot);
}

/**
 * @brief 擦除组件所在的Flash扇区
 */
int upgrade_erase_component_sectors(const char *component_name, bool is_backup_slot)
{
    if (!client_initialized || !component_name) {
        return -1;
    }
    
    uint32_t addr = get_component_slot_address(component_name, is_backup_slot);
    if (addr == 0) {
        return -1;
    }
    
    /* 擦除对应的扇区 */
    if (QSPI_W25Qxx_SectorErase(addr) != QSPI_W25Qxx_OK) {
        printf("[UPGRADE] Erase failed for %s at 0x%08lX\r\n", component_name, addr);
        return -1;
    }
    
    printf("[UPGRADE] Erased sectors for %s\r\n", component_name);
    return 0;
}

/* 私有函数实现 */

/**
 * @brief 获取组件槽位地址
 */
static uint32_t get_component_slot_address(const char *component_name, bool is_backup_slot)
{
    if (strcmp(component_name, "application") == 0) {
        return is_backup_slot ? APPLICATION_SLOT_B_ADDR : APPLICATION_SLOT_A_ADDR;
    } else if (strcmp(component_name, "web") == 0) {
        return is_backup_slot ? WEB_RESOURCES_SLOT_B_ADDR : WEB_RESOURCES_SLOT_A_ADDR;
    } else if (strcmp(component_name, "config") == 0) {
        return is_backup_slot ? CONFIG_SLOT_B_ADDR : CONFIG_SLOT_A_ADDR;
    } else if (strcmp(component_name, "adc") == 0) {
        return is_backup_slot ? ADC_MAPPING_SLOT_B_ADDR : ADC_MAPPING_SLOT_A_ADDR;
    }
    
    return 0;
}

/**
 * @brief 写入升级元数据
 */
static int write_upgrade_metadata(const upgrade_request_t *request)
{
    /* 简化的元数据结构，实际应该与bootloader中的格式一致 */
    typedef struct {
        uint32_t magic;                   // 0x55AA55AA
        uint32_t version;                 // 版本
        uint32_t upgrade_type;            // 升级类型
        uint32_t status;                  // 状态
        component_upgrade_info_t components[4]; // 组件信息
        uint32_t crc32;                   // CRC32
    } simple_metadata_t;
    
    simple_metadata_t metadata;
    memset(&metadata, 0, sizeof(metadata));
    
    metadata.magic = 0x55AA55AA;
    metadata.version = 1;
    metadata.upgrade_type = request->type;
    metadata.status = UPGRADE_STATUS_PENDING;
    
    memcpy(metadata.components, request->components, sizeof(metadata.components));
    
    /* 计算CRC32 */
    metadata.crc32 = calculate_crc32((uint8_t*)&metadata, sizeof(metadata) - sizeof(uint32_t));
    
    /* 擦除元数据扇区 */
    if (QSPI_W25Qxx_SectorErase(UPGRADE_METADATA_ADDR) != QSPI_W25Qxx_OK) {
        return -1;
    }
    
    /* 写入元数据 */
    if (QSPI_W25Qxx_WriteBuffer((uint8_t*)&metadata, UPGRADE_METADATA_ADDR, sizeof(metadata)) != QSPI_W25Qxx_OK) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief 计算CRC32（内部实现）
 */
static uint32_t calculate_crc32(const uint8_t *data, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
} 