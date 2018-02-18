/*
 * drivers/gpio/gpio-tmpm32xi2c.h
 *
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _LINUX_TMPM32XI2C_H
#define _LINUX_TMPM32XI2C_H

#define COMMAND_VER     0x00
#define CMD_VER_STR     "00"

#define CMD_PIN_RD      0x20
#define CMD_PIN_WR      0x30
#define CMD_PORT_RD     0x21
#define CMD_PORT_WR     0x31
#define CMD_PIN_IN      0x22
#define CMD_PIN_OUT     0x32
#define CMD_GET_DIR     0x23
#define CMD_POL_INV     0x24

#define CMD_RD_VOLT     0x25
#define CMD_RD_ADC      0x26

#define CMD_RESET       0x27
#define CMD_POWER       0x28

#define CMD_INT_REG     0x10
#define CMD_INT_MASK    0x29

#define CMD_FAN_PWM     0x33
#define CMD_FAN_TACH    0x2A

#define CMD_FORCE_RCV   0x2B

#define CMD_VERSION     0x2F

#define CMD_I2C_PROB    0xA0
#define CMD_CAN_TEST    0xA1

#define R_SYS_9V        0
#define R_VBAT          1
#define R_VDD_DDR_1V8   2

#define R_VDD_GPU       0
#define R_VDD_BCPU      1
#define R_VDD_MCPU      2
#define R_VDD_SOC       3
#define R_VDD_SRAM      4
#define R_VDD_DDR       5

#define V_FW            0
#define V_CMD           1

#define DIR_OUT         0
#define DIR_IN          1

#endif /* _LINUX_TMPM32XI2C_H */
