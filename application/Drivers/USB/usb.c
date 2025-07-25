#include "usb.h"
#include "tusb.h"
#include "stm32h7xx_hal.h"
#include "board_cfg.h"

void USB_clock_init() {
    /**
     * USB Clock
     * 
     */
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    // 确保HSI48时钟开启 (USB需要48MHz时钟)
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }
    
    // 配置USB时钟源为HSI48
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

}

void USB_Device_Init() {

    GPIO_InitTypeDef GPIO_InitStruct;

    #if BOARD_TUD_RHPORT == 0
    // Despite being call USB2_OTG
    // OTG_FS is marked as RHPort0 by TinyUSB to be consistent across stm32 port
    // PA9 VUSB, PA10 ID, PA11 DM, PA12 DP

    // Configure DM DP Pins
    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // This for ID line debug
    // GPIO_InitStruct.Pin = GPIO_PIN_10;
    // GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    // GPIO_InitStruct.Pull = GPIO_PULLUP;
    // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    // GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
    // HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // https://community.st.com/s/question/0D50X00009XkYZLSA3/stm32h7-nucleo-usb-fs-cdc
    // TODO: Board init actually works fine without this line.
    HAL_PWREx_EnableUSBVoltageDetector();
    __HAL_RCC_USB2_OTG_FS_CLK_ENABLE();

    #if OTG_FS_VBUS_SENSE
    // Configure VBUS Pin
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Enable VBUS sense (B device) via pin PA9
    USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBDEN;
    #else
    // Disable VBUS sense (B device) via pin PA9
    USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

    // B-peripheral session valid override enable
    USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USB_OTG_FS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
    #endif // vbus sense

    #elif BOARD_TUD_RHPORT == 1
    // Despite being call USB2_OTG
    // OTG_HS is marked as RHPort1 by TinyUSB to be consistent across stm32 port

    struct {
        GPIO_TypeDef* port;
        uint32_t pin;
    } const ulpi_pins[] =
    {
        ULPI_PINS
    };

    for (uint8_t i=0; i < sizeof(ulpi_pins)/sizeof(ulpi_pins[0]); i++)
    {
        GPIO_InitStruct.Pin       = ulpi_pins[i].pin;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG2_HS;
        HAL_GPIO_Init(ulpi_pins[i].port, &GPIO_InitStruct);
    }

    // Enable USB HS & ULPI Clocks
    __HAL_RCC_USB1_OTG_HS_ULPI_CLK_ENABLE();
    __HAL_RCC_USB1_OTG_HS_CLK_ENABLE();

    #if OTG_HS_VBUS_SENSE
    #error OTG HS VBUS Sense enabled is not implemented
    #else
    // No VBUS sense
    USB_OTG_HS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

    // B-peripheral session valid override enable
    USB_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USB_OTG_HS->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
    #endif

    // Force device mode
    USB_OTG_HS->GUSBCFG &= ~USB_OTG_GUSBCFG_FHMOD;
    USB_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

    HAL_PWREx_EnableUSBVoltageDetector();

    // For waveshare openh743 ULPI PHY reset walkaround
    board_stm32h7_post_init();
    #endif // rhport = 1

}

void USB_Host_Init() {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_PWREx_EnableUSBVoltageDetector();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USB_OTG_HS GPIO Configuration
     PB15     ------> USB_OTG_HS_DP
    PB14     ------> USB_OTG_HS_DM
    */
    GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_OTG2_FS;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USB_OTG_HS clock enable */
    __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

    /* USB_OTG_HS interrupt Init */
    HAL_NVIC_SetPriority(OTG_HS_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
    /* USER CODE BEGIN USB_OTG_HS_MspInit 1 */
}



