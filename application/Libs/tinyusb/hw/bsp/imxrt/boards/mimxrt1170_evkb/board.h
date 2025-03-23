/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

/* metadata:
   name: i.MX RT1070 Evaluation Kit
   url: https://www.nxp.com/design/design-center/development-boards-and-designs/i-mx-evaluation-and-development-boards/i-mx-rt1170-evaluation-kit:MIMXRT1170-EVKB
*/

#ifndef BOARD_MIMXRT1170_EVKB_H_
#define BOARD_MIMXRT1170_EVKB_H_

// required since iMXRT MCUX-SDK include this file for board size
#define BOARD_FLASH_SIZE (0x1000000U)

// LED: IOMUXC_GPIO_AD_04_GPIO9_IO03
#define LED_PORT              BOARD_INITPINS_USER_LED_PERIPHERAL
#define LED_PIN               BOARD_INITPINS_USER_LED_CHANNEL
#define LED_STATE_ON          0

// SW8 button: IOMUXC_WAKEUP_DIG_GPIO13_IO00
#define BUTTON_PORT           BOARD_INITPINS_USER_BUTTON_PERIPHERAL
#define BUTTON_PIN            BOARD_INITPINS_USER_BUTTON_CHANNEL
#define BUTTON_STATE_ACTIVE   0

// UART: IOMUXC_GPIO_AD_B0_13_LPUART1_RX, IOMUXC_GPIO_AD_B0_12_LPUART1_TX
#define UART_PORT             LPUART1
#define UART_CLK_ROOT         BOARD_BOOTCLOCKRUN_LPUART10_CLK_ROOT

#endif
