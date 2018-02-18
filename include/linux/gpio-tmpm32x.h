/*
 * arch/arm/mach-tegra/include/mach/gpio-names.h
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __GPIO_TMPM32X_H
#define __GPIO_TMPM32X_H

/*
https://p4viewer.nvidia.com/get///syseng/
Projects/P1889/MCU/bringup_firmware/application/ucc_mcu_io_def.h
*/

#if 0
/* Port A */
#define VCM_RESET_MCU      0x00  /* OUT */
#define VCM_12V_EN_MCU     0x01  /* OUT */
#define PMU_POWER_ON_MCU   0x02  /* OUT - PMU enable */
#define VCM_WAKE_MCU       0x03  /* OUT */
#define FORCE_RECOVERY_MCU 0x04  /* OUT - enable force recovery to Tegra */
#define FORCE_PMU_OFF_MCU  0x05  /* OUT - force PMU off */
#define IDT_OE             0x06  /* OUT */
#define MCU_PROD_PROT      0x07  /* IN */

/* Port B */
#define PORTB0_RESERVED    0x08
#define PORTB1_RESERVED	   0x09
#define PORTB2_RESERVED    0x0A
#define PORTB3_RESERVED    0x0B
#define PORTB4_RESERVED    0x0C
#define PORTB5_RESERVED    0x0D
#define PORTB6_RESERVED    0x0E
#define PORTB7_RESERVED    0x0F

/* Port C */
#define MCU_TACH_IN        0x10  /* IN  - cooling fan tach input */
#define MCU_FAN_SEL        0x11  /* OUT - cooling fan enable */
#define MCU_CAN_STB1       0x12  /* OUT */
#define MCU_CAN_ERR1       0x13  /* IN */
#define MCU_CAN_EN1        0x14  /* OUT */
#define MCU_CAN_STB0       0x15  /* OUT */
#define MCU_CAN_EN0        0x16  /* OUT */
#define MCU_CAN_ERR0       0x17  /* IN */

/* Port D */
#define WL_HOST_WAKE       0x18  /* IN */
#define WL_REG_ON          0x19  /* OUT - WiFi enable */
#define BT_REG_ON          0x1A  /* OUT - Bluetooth enable */
#define MODEM_WAKE         0x1B  /* IN */
#define CVB2CS_PDN         0x1C  /* OUT */
#define EN_VDDIO_SDCARD3   0x1D  /* OUT */
#define EN_VDDIO_SDCARD2   0x1E  /* OUT */
#define FPDLINK2_PWRDWN    0x1F  /* OUT */

/* Port E */
/* OUT - select MobileEye(1) or SPI flash device(0) */
#define FV_SPI_MUX_SEL     0x20
#define AURIX_FV_GPIO1     0x21  /* IN/OUT - from MobileEye AIC */
#define AURIX_FV_GPIO2     0x22  /* IN/OUT - from MobileEye AIC */
#define AURIX_FV_GPIO3     0x23  /* IN/OUT - from MobileEye AIC */
#define TEGRA_FV_GPIO0     0x24  /* IN/OUT - from MobileEye AIC */
#define TEGRA_FV_GPIO1     0x25  /* IN/OUT - from MobileEye AIC */
#define FV_FPGA_GPIO0      0x26  /* IN/OUT - from MobileEye AIC */
#define FV_FPGA_GPIO1      0x27  /* IN/OUT - from MobileEye AIC */

/* Port F */
#define MCU_TST1           0x28  /* OUT - debug output 1 */
#define MCU_TST2           0x29  /* OUT - debug output 2 */
#define LCD_BL_PWM         0x2A  /* OUT */
#define LCD_BL_EN          0x2B  /* OUT */
#define PMU_POWER_GOOD     0x2C  /* IN - power good from PMU */
/* IN - ignition key "Accessories" from VIC harness */
#define MCU_VIC_ACC        0x2D
/* IN - ignition key "Run" from VIC harness */
#define MCU_VIC_RUN        0x2E
#define PORTF7_RESERVED    0x2F

/* Port G */
#define PORTG0_RESERVED    0x30
#define PORTG1_RESERVED    0x31
#define PORTG2_RESERVED    0x32
#define PORTG3_RESERVED    0x33
#define PORTG4_RESERVED    0x34
#define PORTG5_RESERVED    0x45
#define PORTG6_RESERVED    0x36
#define EN_VDDIO_SATA      0x37  /* OUT */

/* Port H */
#define MCU_CAN_TXD0       0x38  /* CAN 0 function */
#define MCU_CAN_RXD0       0x39  /* " */
#define MCU_CAN_TXD1       0x3A  /* CAN 1 function */
#define MCU_CAN_RXD1       0x3B  /* " */
#define MCU_SPI0_MISO      0x3C  /* SPI 0 MASTER function */
#define MCU_SPI0_MOSI      0x3D  /* " */
#define MCU_SPI0_CS        0x3E  /* " */
#define MCU_SPI0_SCK       0x3F  /* " */

/* Port I */
#define PORTI0_RESERVED    0x40
#define PORTI1_RESERVED    0x41
#define PORTI2_RESERVED    0x42
#define PORTI3_RESERVED    0x43
#define AIC1_PDN           0x44  /* OUT */
#define AIC2_PDN           0x45  /* OUT */
#define AIC3_PDN           0x46  /* OUT */
#define AIC4_PDN           0x47  /* OUT */

/* Port J */
#define MCU_TRACED0        0x48  /* JTAG debugger connection */
#define MCU_TRACED1        0x49  /* " */
#define MCU_TRACED2        0x4A  /* " */
#define MCU_TRACED3        0x4B  /* " */
#define MCU_TRACECLK       0x4C  /* " */
#define PORTJ5_RESERVED    0x4D
#define PORTJ6_RESERVED    0x4E
#define PORTJ7_RESERVED    0x4F

/* Port K */
#define DBG_FRC_RCVRY      0x50  /* IN - force recovery from PM342 */
#define DBG_ONKEY          0x51  /* IN - from PM342 */
#define DBG_FORCE_OFF      0x52  /* IN - force PMU off from PM342 */
#define DBG_SYS_RESET      0x53  /* IN - system reset from PM342 */
#define VIC_RECOVERY       0x54  /* IN - force recovery from VIC harness */
#define DBG_JTAG_SRST      0x55  /* IN - from PM342 */
#define PORTK6_RESERVED    0x56
#define UCC_RESET          0x57  /* IN - from Tegra RESET_OUT */

/* Port L */
#define PORTL0_RESERVED    0x58
#define PORTL1_RESERVED    0x59
#define PORTL2_RESERVED    0x5A
#define PORTL3_RESERVED    0x5B
#define MCU_UART0_TXD      0x5C  /* UART 0 Ffunction */
#define MCU_UART0_RXD      0x5D  /* " */
#define MCU_UART0_CTS      0x5E  /* " */
#define MCU_UART0_RTS      0x5F  /* " */

/* Port M */
#define MCU_UART1_TXD      0x60  /* UART 1 Ffunction */
#define MCU_UART1_RXD      0x61  /* " */
#define MCU_UART1_CTS      0x62  /* " */
#define MCU_UART1_RTS      0x63  /* " */
#define MCU_SPI1_MISO      0x64  /* SPI 1 SLAVE function */
#define MCU_SPI1_MOSI      0x65  /* " */
#define MCU_SPI1_CS        0x66  /* " */
#define MCU_SPI1_SCK       0x67  /* " */

/* Port N */
#define PORTN0_RESERVED    0x68
#define PORTN1_RESERVED    0x69
#define PORTN2_RESERVED    0x6A
#define PORTN3_RESERVED    0x6B
#define PORTN4_RESERVED    0x6C
#define PORTN5_RESERVED    0x6D
#define PORTN6_RESERVED    0x6E
#define MCU_PDON           0x6F  /* IN - power on override */
#endif

#define TMPM32X_GPIO_BASE	500
#define TMPM32X_GPIO_INVALID	-1
#define TMPM32X_GPIO_PA0	(TMPM32X_GPIO_BASE + 0)
#define TMPM32X_GPIO_PA1	(TMPM32X_GPIO_BASE + 1)
#define TMPM32X_GPIO_PA2	(TMPM32X_GPIO_BASE + 2)
#define TMPM32X_GPIO_PA3	(TMPM32X_GPIO_BASE + 3)
#define TMPM32X_GPIO_PA4	(TMPM32X_GPIO_BASE + 4)
#define TMPM32X_GPIO_PA5	(TMPM32X_GPIO_BASE + 5)
#define TMPM32X_GPIO_PA6	(TMPM32X_GPIO_BASE + 6)
#define TMPM32X_GPIO_PA7	(TMPM32X_GPIO_BASE + 7)
#define TMPM32X_GPIO_PB0	(TMPM32X_GPIO_BASE + 8)
#define TMPM32X_GPIO_PB1	(TMPM32X_GPIO_BASE + 9)
#define TMPM32X_GPIO_PB2	(TMPM32X_GPIO_BASE + 10)
#define TMPM32X_GPIO_PB3	(TMPM32X_GPIO_BASE + 11)
#define TMPM32X_GPIO_PB4	(TMPM32X_GPIO_BASE + 12)
#define TMPM32X_GPIO_PB5	(TMPM32X_GPIO_BASE + 13)
#define TMPM32X_GPIO_PB6	(TMPM32X_GPIO_BASE + 14)
#define TMPM32X_GPIO_PB7	(TMPM32X_GPIO_BASE + 15)
#define TMPM32X_GPIO_PC0	(TMPM32X_GPIO_BASE + 16)
#define TMPM32X_GPIO_PC1	(TMPM32X_GPIO_BASE + 17)
#define TMPM32X_GPIO_PC2	(TMPM32X_GPIO_BASE + 18)
#define TMPM32X_GPIO_PC3	(TMPM32X_GPIO_BASE + 19)
#define TMPM32X_GPIO_PC4	(TMPM32X_GPIO_BASE + 20)
#define TMPM32X_GPIO_PC5	(TMPM32X_GPIO_BASE + 21)
#define TMPM32X_GPIO_PC6	(TMPM32X_GPIO_BASE + 22)
#define TMPM32X_GPIO_PC7	(TMPM32X_GPIO_BASE + 23)
#define TMPM32X_GPIO_PD0	(TMPM32X_GPIO_BASE + 24)
#define TMPM32X_GPIO_PD1	(TMPM32X_GPIO_BASE + 25)
#define TMPM32X_GPIO_PD2	(TMPM32X_GPIO_BASE + 26)
#define TMPM32X_GPIO_PD3	(TMPM32X_GPIO_BASE + 27)
#define TMPM32X_GPIO_PD4	(TMPM32X_GPIO_BASE + 28)
#define TMPM32X_GPIO_PD5	(TMPM32X_GPIO_BASE + 29)
#define TMPM32X_GPIO_PD6	(TMPM32X_GPIO_BASE + 30)
#define TMPM32X_GPIO_PD7	(TMPM32X_GPIO_BASE + 31)
#define TMPM32X_GPIO_PE0	(TMPM32X_GPIO_BASE + 32)
#define TMPM32X_GPIO_PE1	(TMPM32X_GPIO_BASE + 33)
#define TMPM32X_GPIO_PE2	(TMPM32X_GPIO_BASE + 34)
#define TMPM32X_GPIO_PE3	(TMPM32X_GPIO_BASE + 35)
#define TMPM32X_GPIO_PE4	(TMPM32X_GPIO_BASE + 36)
#define TMPM32X_GPIO_PE5	(TMPM32X_GPIO_BASE + 37)
#define TMPM32X_GPIO_PE6	(TMPM32X_GPIO_BASE + 38)
#define TMPM32X_GPIO_PE7	(TMPM32X_GPIO_BASE + 39)
#define TMPM32X_GPIO_PF0	(TMPM32X_GPIO_BASE + 40)
#define TMPM32X_GPIO_PF1	(TMPM32X_GPIO_BASE + 41)
#define TMPM32X_GPIO_PF2	(TMPM32X_GPIO_BASE + 42)
#define TMPM32X_GPIO_PF3	(TMPM32X_GPIO_BASE + 43)
#define TMPM32X_GPIO_PF4	(TMPM32X_GPIO_BASE + 44)
#define TMPM32X_GPIO_PF5	(TMPM32X_GPIO_BASE + 45)
#define TMPM32X_GPIO_PF6	(TMPM32X_GPIO_BASE + 46)
#define TMPM32X_GPIO_PF7	(TMPM32X_GPIO_BASE + 47)
#define TMPM32X_GPIO_PG0	(TMPM32X_GPIO_BASE + 48)
#define TMPM32X_GPIO_PG1	(TMPM32X_GPIO_BASE + 49)
#define TMPM32X_GPIO_PG2	(TMPM32X_GPIO_BASE + 50)
#define TMPM32X_GPIO_PG3	(TMPM32X_GPIO_BASE + 51)
#define TMPM32X_GPIO_PG4	(TMPM32X_GPIO_BASE + 52)
#define TMPM32X_GPIO_PG5	(TMPM32X_GPIO_BASE + 53)
#define TMPM32X_GPIO_PG6	(TMPM32X_GPIO_BASE + 54)
#define TMPM32X_GPIO_PG7	(TMPM32X_GPIO_BASE + 55)
#define TMPM32X_GPIO_PH0	(TMPM32X_GPIO_BASE + 56)
#define TMPM32X_GPIO_PH1	(TMPM32X_GPIO_BASE + 57)
#define TMPM32X_GPIO_PH2	(TMPM32X_GPIO_BASE + 58)
#define TMPM32X_GPIO_PH3	(TMPM32X_GPIO_BASE + 59)
#define TMPM32X_GPIO_PH4	(TMPM32X_GPIO_BASE + 60)
#define TMPM32X_GPIO_PH5	(TMPM32X_GPIO_BASE + 61)
#define TMPM32X_GPIO_PH6	(TMPM32X_GPIO_BASE + 62)
#define TMPM32X_GPIO_PH7	(TMPM32X_GPIO_BASE + 63)
#define TMPM32X_GPIO_PI0	(TMPM32X_GPIO_BASE + 64)
#define TMPM32X_GPIO_PI1	(TMPM32X_GPIO_BASE + 65)
#define TMPM32X_GPIO_PI2	(TMPM32X_GPIO_BASE + 66)
#define TMPM32X_GPIO_PI3	(TMPM32X_GPIO_BASE + 67)
#define TMPM32X_GPIO_PI4	(TMPM32X_GPIO_BASE + 68)
#define TMPM32X_GPIO_PI5	(TMPM32X_GPIO_BASE + 69)
#define TMPM32X_GPIO_PI6	(TMPM32X_GPIO_BASE + 70)
#define TMPM32X_GPIO_PI7	(TMPM32X_GPIO_BASE + 71)
#define TMPM32X_GPIO_PJ0	(TMPM32X_GPIO_BASE + 72)
#define TMPM32X_GPIO_PJ1	(TMPM32X_GPIO_BASE + 73)
#define TMPM32X_GPIO_PJ2	(TMPM32X_GPIO_BASE + 74)
#define TMPM32X_GPIO_PJ3	(TMPM32X_GPIO_BASE + 75)
#define TMPM32X_GPIO_PJ4	(TMPM32X_GPIO_BASE + 76)
#define TMPM32X_GPIO_PJ5	(TMPM32X_GPIO_BASE + 77)
#define TMPM32X_GPIO_PJ6	(TMPM32X_GPIO_BASE + 78)
#define TMPM32X_GPIO_PJ7	(TMPM32X_GPIO_BASE + 79)
#define TMPM32X_GPIO_PK0	(TMPM32X_GPIO_BASE + 80)
#define TMPM32X_GPIO_PK1	(TMPM32X_GPIO_BASE + 81)
#define TMPM32X_GPIO_PK2	(TMPM32X_GPIO_BASE + 82)
#define TMPM32X_GPIO_PK3	(TMPM32X_GPIO_BASE + 83)
#define TMPM32X_GPIO_PK4	(TMPM32X_GPIO_BASE + 84)
#define TMPM32X_GPIO_PK5	(TMPM32X_GPIO_BASE + 85)
#define TMPM32X_GPIO_PK6	(TMPM32X_GPIO_BASE + 86)
#define TMPM32X_GPIO_PK7	(TMPM32X_GPIO_BASE + 87)
#define TMPM32X_GPIO_PL0	(TMPM32X_GPIO_BASE + 88)
#define TMPM32X_GPIO_PL1	(TMPM32X_GPIO_BASE + 89)
#define TMPM32X_GPIO_PL2	(TMPM32X_GPIO_BASE + 90)
#define TMPM32X_GPIO_PL3	(TMPM32X_GPIO_BASE + 91)
#define TMPM32X_GPIO_PL4	(TMPM32X_GPIO_BASE + 92)
#define TMPM32X_GPIO_PL5	(TMPM32X_GPIO_BASE + 93)
#define TMPM32X_GPIO_PL6	(TMPM32X_GPIO_BASE + 94)
#define TMPM32X_GPIO_PL7	(TMPM32X_GPIO_BASE + 95)
#define TMPM32X_GPIO_PM0	(TMPM32X_GPIO_BASE + 96)
#define TMPM32X_GPIO_PM1	(TMPM32X_GPIO_BASE + 97)
#define TMPM32X_GPIO_PM2	(TMPM32X_GPIO_BASE + 98)
#define TMPM32X_GPIO_PM3	(TMPM32X_GPIO_BASE + 99)
#define TMPM32X_GPIO_PM4	(TMPM32X_GPIO_BASE + 100)
#define TMPM32X_GPIO_PM5	(TMPM32X_GPIO_BASE + 101)
#define TMPM32X_GPIO_PM6	(TMPM32X_GPIO_BASE + 102)
#define TMPM32X_GPIO_PM7	(TMPM32X_GPIO_BASE + 103)
#define TMPM32X_GPIO_PN0	(TMPM32X_GPIO_BASE + 104)
#define TMPM32X_GPIO_PN1	(TMPM32X_GPIO_BASE + 105)
#define TMPM32X_GPIO_PN2	(TMPM32X_GPIO_BASE + 106)
#define TMPM32X_GPIO_PN3	(TMPM32X_GPIO_BASE + 107)
#define TMPM32X_GPIO_PN4	(TMPM32X_GPIO_BASE + 108)
#define TMPM32X_GPIO_PN5	(TMPM32X_GPIO_BASE + 109)
#define TMPM32X_GPIO_PN6	(TMPM32X_GPIO_BASE + 110)
#define TMPM32X_GPIO_PN7	(TMPM32X_GPIO_BASE + 111)
#define TMPM32X_MAX_GPIO	(112)
#endif
