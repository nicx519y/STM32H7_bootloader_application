#include "hid_host.h"

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void)dev_addr;
    (void)instance;
    (void)report;
    (void)len;

    // 打印接收到的报告数据
    printf("HID报告从设备 %d, 实例 %d 接收, 长度 = %d\r\n", dev_addr, instance, len);
}