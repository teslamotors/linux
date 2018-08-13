/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015 - 2018 Intel Corporation
 *
 * Author: Jianxu Zheng <jian.xu.zheng@intel.com>
 *
 */

#ifndef __CRLMODULE_ADV7481_CONFIGURATION_H_
#define __CRLMODULE_ADV7481_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"

static struct crl_register_write_rep adv7481_powerup_regset[] = {
	{0xFF, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* SW reset */
	{0x00, CRL_REG_LEN_DELAY, 0x05, 0x00}, /* Delay 5ms */
	{0x01, CRL_REG_LEN_08BIT, 0x76, 0xE0}, /* ADI recommended setting */
	{0xF2, CRL_REG_LEN_08BIT, 0x01, 0xE0}, /* I2C Rd Auto-Increment=1 */
	{0xF3, CRL_REG_LEN_08BIT, 0x4C, 0xE0}, /* DPLL Map Address */
	{0xF4, CRL_REG_LEN_08BIT, 0x44, 0xE0}, /* CP Map Address */
	{0xF5, CRL_REG_LEN_08BIT, 0x68, 0xE0}, /* HDMI RX Map Address */
	{0xF6, CRL_REG_LEN_08BIT, 0x6C, 0xE0}, /* EDID Map Address */
	{0xF7, CRL_REG_LEN_08BIT, 0x64, 0xE0}, /* HDMI RX Repeater Map Addr */
	{0xF8, CRL_REG_LEN_08BIT, 0x62, 0xE0}, /* HDMI RX Infoframe Map Addr */
	{0xF9, CRL_REG_LEN_08BIT, 0xF0, 0xE0}, /* CBUS Map Address Set */
	{0xFA, CRL_REG_LEN_08BIT, 0x82, 0xE0}, /* CEC Map Address Set */
	{0xFB, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* SDP Main Map Address */
	{0xFC, CRL_REG_LEN_08BIT, 0x90, 0xE0}, /* CSI-TXB Map Address */
	{0xFD, CRL_REG_LEN_08BIT, 0x94, 0xE0}, /* CSI-TXA Map Address */
	{0x00, CRL_REG_LEN_08BIT, 0x50, 0xE0}, /* Disable Chip Powerdown &
						  HDMI Rx Block */
	{0x40, CRL_REG_LEN_08BIT, 0x83, 0x64}, /* Enable HDCP 1.1 */
	{0x00, CRL_REG_LEN_08BIT, 0x08, 0x68}, /* ADI recommended setting */
	{0x3D, CRL_REG_LEN_08BIT, 0x10, 0x68}, /* ADI recommended setting */
	{0x3E, CRL_REG_LEN_08BIT, 0x69, 0x68}, /* ADI recommended setting */
	{0x3F, CRL_REG_LEN_08BIT, 0x46, 0x68}, /* ADI recommended setting */
	{0x4E, CRL_REG_LEN_08BIT, 0xFE, 0x68}, /* ADI recommended setting */
	{0x4F, CRL_REG_LEN_08BIT, 0x08, 0x68}, /* ADI recommended setting */
	{0x57, CRL_REG_LEN_08BIT, 0xA3, 0x68}, /* ADI recommended setting */
	{0x58, CRL_REG_LEN_08BIT, 0x04, 0x68}, /* ADI recommended setting */
	{0x85, CRL_REG_LEN_08BIT, 0x10, 0x68}, /* ADI recommended setting */
	{0x83, CRL_REG_LEN_08BIT, 0x00, 0x68}, /* Enable All Terminations */
	{0xBE, CRL_REG_LEN_08BIT, 0x00, 0x68}, /* ADI recommended setting */
	{0x6C, CRL_REG_LEN_08BIT, 0x01, 0x68}, /* HPA Manual Enable */
	{0xF8, CRL_REG_LEN_08BIT, 0x01, 0x68}, /* HPA Asserted */
	{0x0F, CRL_REG_LEN_08BIT, 0x00, 0x68}, /* Audio Mute Speed =
						  Fastest Smallest Step Size */
	{0x70, CRL_REG_LEN_08BIT, 0xA0, 0x64}, /* Write primary edid size */
	{0x74, CRL_REG_LEN_08BIT, 0x01, 0x64}, /* Enable manual edid */
	{0x7A, CRL_REG_LEN_08BIT, 0x00, 0x64}, /* Write edid sram select */
	{0xF6, CRL_REG_LEN_08BIT, 0x6C, 0xE0}, /* Write edid map bus address */

	{0x00*4, CRL_REG_LEN_32BIT, 0x00FFFFFF, 0x6C}, /* EDID programming */
	{0x01*4, CRL_REG_LEN_32BIT, 0xFFFFFF00, 0x6C}, /* EDID programming */
	{0x02*4, CRL_REG_LEN_32BIT, 0x4DD90100, 0x6C}, /* EDID programming */
	{0x03*4, CRL_REG_LEN_32BIT, 0x00000000, 0x6C}, /* EDID programming */
	{0x04*4, CRL_REG_LEN_32BIT, 0x00110103, 0x6C}, /* EDID programming */
	{0x05*4, CRL_REG_LEN_32BIT, 0x80000078, 0x6C}, /* EDID programming */
	{0x06*4, CRL_REG_LEN_32BIT, 0x0A0DC9A0, 0x6C}, /* EDID programming */
	{0x07*4, CRL_REG_LEN_32BIT, 0x57479827, 0x6C}, /* EDID programming */
	{0x08*4, CRL_REG_LEN_32BIT, 0x12484C00, 0x6C}, /* EDID programming */
	{0x09*4, CRL_REG_LEN_32BIT, 0x00000101, 0x6C}, /* EDID programming */
	{0x0A*4, CRL_REG_LEN_32BIT, 0x01010101, 0x6C}, /* EDID programming */
	{0x0B*4, CRL_REG_LEN_32BIT, 0x01010101, 0x6C}, /* EDID programming */
	{0x0C*4, CRL_REG_LEN_32BIT, 0x01010101, 0x6C}, /* EDID programming */
	{0x0D*4, CRL_REG_LEN_32BIT, 0x0101011D, 0x6C}, /* EDID programming */
	{0x0E*4, CRL_REG_LEN_32BIT, 0x80D0721C, 0x6C}, /* EDID programming */
	{0x0F*4, CRL_REG_LEN_32BIT, 0x1620102C, 0x6C}, /* EDID programming */
	{0x10*4, CRL_REG_LEN_32BIT, 0x2580C48E, 0x6C}, /* EDID programming */
	{0x11*4, CRL_REG_LEN_32BIT, 0x2100009E, 0x6C}, /* EDID programming */
	{0x12*4, CRL_REG_LEN_32BIT, 0x011D8018, 0x6C}, /* EDID programming */
	{0x13*4, CRL_REG_LEN_32BIT, 0x711C1620, 0x6C}, /* EDID programming */
	{0x14*4, CRL_REG_LEN_32BIT, 0x582C2500, 0x6C}, /* EDID programming */
	{0x15*4, CRL_REG_LEN_32BIT, 0xC48E2100, 0x6C}, /* EDID programming */
	{0x16*4, CRL_REG_LEN_32BIT, 0x009E0000, 0x6C}, /* EDID programming */
	{0x17*4, CRL_REG_LEN_32BIT, 0x00FC0048, 0x6C}, /* EDID programming */
	{0x18*4, CRL_REG_LEN_32BIT, 0x444D4920, 0x6C}, /* EDID programming */
	{0x19*4, CRL_REG_LEN_32BIT, 0x4C4C430A, 0x6C}, /* EDID programming */
	{0x1A*4, CRL_REG_LEN_32BIT, 0x20202020, 0x6C}, /* EDID programming */
	{0x1B*4, CRL_REG_LEN_32BIT, 0x000000FD, 0x6C}, /* EDID programming */
	{0x1C*4, CRL_REG_LEN_32BIT, 0x003B3D0F, 0x6C}, /* EDID programming */
	{0x1D*4, CRL_REG_LEN_32BIT, 0x2D08000A, 0x6C}, /* EDID programming */
	{0x1E*4, CRL_REG_LEN_32BIT, 0x20202020, 0x6C}, /* EDID programming */
	{0x1F*4, CRL_REG_LEN_32BIT, 0x202001C1, 0x6C}, /* EDID programming */
	{0x20*4, CRL_REG_LEN_32BIT, 0x02031E77, 0x6C}, /* EDID programming */
	{0x21*4, CRL_REG_LEN_32BIT, 0x4F941305, 0x6C}, /* EDID programming */
	{0x22*4, CRL_REG_LEN_32BIT, 0x03040201, 0x6C}, /* EDID programming */
	{0x23*4, CRL_REG_LEN_32BIT, 0x16150706, 0x6C}, /* EDID programming */
	{0x24*4, CRL_REG_LEN_32BIT, 0x1110121F, 0x6C}, /* EDID programming */
	{0x25*4, CRL_REG_LEN_32BIT, 0x23090701, 0x6C}, /* EDID programming */
	{0x26*4, CRL_REG_LEN_32BIT, 0x65030C00, 0x6C}, /* EDID programming */
	{0x27*4, CRL_REG_LEN_32BIT, 0x10008C0A, 0x6C}, /* EDID programming */
	{0x28*4, CRL_REG_LEN_32BIT, 0xD0902040, 0x6C}, /* EDID programming */
	{0x29*4, CRL_REG_LEN_32BIT, 0x31200C40, 0x6C}, /* EDID programming */
	{0x2A*4, CRL_REG_LEN_32BIT, 0x5500138E, 0x6C}, /* EDID programming */
	{0x2B*4, CRL_REG_LEN_32BIT, 0x21000018, 0x6C}, /* EDID programming */
	{0x2C*4, CRL_REG_LEN_32BIT, 0x011D00BC, 0x6C}, /* EDID programming */
	{0x2D*4, CRL_REG_LEN_32BIT, 0x52D01E20, 0x6C}, /* EDID programming */
	{0x2E*4, CRL_REG_LEN_32BIT, 0xB8285540, 0x6C}, /* EDID programming */
	{0x2F*4, CRL_REG_LEN_32BIT, 0xC48E2100, 0x6C}, /* EDID programming */
	{0x30*4, CRL_REG_LEN_32BIT, 0x001E8C0A, 0x6C}, /* EDID programming */
	{0x31*4, CRL_REG_LEN_32BIT, 0xD08A20E0, 0x6C}, /* EDID programming */
	{0x32*4, CRL_REG_LEN_32BIT, 0x2D10103E, 0x6C}, /* EDID programming */
	{0x33*4, CRL_REG_LEN_32BIT, 0x9600C48E, 0x6C}, /* EDID programming */
	{0x34*4, CRL_REG_LEN_32BIT, 0x21000018, 0x6C}, /* EDID programming */
	{0x35*4, CRL_REG_LEN_32BIT, 0x011D0072, 0x6C}, /* EDID programming */
	{0x36*4, CRL_REG_LEN_32BIT, 0x51D01E20, 0x6C}, /* EDID programming */
	{0x37*4, CRL_REG_LEN_32BIT, 0x6E285500, 0x6C}, /* EDID programming */
	{0x38*4, CRL_REG_LEN_32BIT, 0xC48E2100, 0x6C}, /* EDID programming */
	{0x39*4, CRL_REG_LEN_32BIT, 0x001E8C0A, 0x6C}, /* EDID programming */
	{0x3A*4, CRL_REG_LEN_32BIT, 0xD08A20E0, 0x6C}, /* EDID programming */
	{0x3B*4, CRL_REG_LEN_32BIT, 0x2D10103E, 0x6C}, /* EDID programming */
	{0x3C*4, CRL_REG_LEN_32BIT, 0x9600138E, 0x6C}, /* EDID programming */
	{0x3D*4, CRL_REG_LEN_32BIT, 0x21000018, 0x6C}, /* EDID programming */
	{0x3E*4, CRL_REG_LEN_32BIT, 0x00000000, 0x6C}, /* EDID programming */
	{0x3F*4, CRL_REG_LEN_32BIT, 0x000000CB, 0x6C}, /* EDID programming */

	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94}, /* No MIPI frame start */
	{0x26, CRL_REG_LEN_08BIT, 0x55, 0x94}, /* Disable sleep mode */
	{0x27, CRL_REG_LEN_08BIT, 0x55, 0x94}, /* Disable escape mode */
	{0x7E, CRL_REG_LEN_08BIT, 0xA0, 0x94}, /* ADI recommended setting */
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x90}, /* ADI recommended setting */
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x90}, /* ADI recommended setting */
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94}, /* ADI recommended setting */
	{0x34, CRL_REG_LEN_08BIT, 0x55, 0x94}, /* ADI recommended setting */
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94}, /* ADI recommended setting */
	{0xCA, CRL_REG_LEN_08BIT, 0x02, 0x94}, /* ADI recommended setting */
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94}, /* ADI recommended setting */
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94}, /* ADI recommended setting */
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94}, /* Power up DPHY */
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94}, /* ADI recommended setting */
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94}, /* ADI recommended setting */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_mode_1080p[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* YCrCb output */
	{0x05, CRL_REG_LEN_08BIT, 0x5E, 0xE0}, /* Select Resolution 1080P */
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* ADI recommended setting */
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0}, /* CP-Insert_AV_Code */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI recommended setting */

	{0x8B, CRL_REG_LEN_08BIT, 0x43, 0x44}, /* 1080P shift left 44 pixel */
	{0x8C, CRL_REG_LEN_08BIT, 0xD4, 0x44}, /* 1080P shift left 44 pixel */
	{0x8B, CRL_REG_LEN_08BIT, 0x4F, 0x44}, /* 1080P shift left 44 pixel */
	{0x8D, CRL_REG_LEN_08BIT, 0xD4, 0x44}, /* 1080P shift left 44 pixel */

	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* LLC/PIX/AUD/SPI PINS TRISTATED */
	{0x10, CRL_REG_LEN_08BIT, 0xA0, 0xE0}, /* Enable 4-lane CSI Tx & Pixel Port */
	{0x1C, CRL_REG_LEN_08BIT, 0x3A, 0xE0}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_mode_720p[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* YCrCb output */
	{0x05, CRL_REG_LEN_08BIT, 0x53, 0xE0}, /* Select Resolution 720P */
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* ADI recommended setting */
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0}, /* CP-Insert_AV_Code */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI recommended setting */

	{0x8B, CRL_REG_LEN_08BIT, 0x43, 0x44}, /* 720P shift left 40 pixel */
	{0x8C, CRL_REG_LEN_08BIT, 0xD8, 0x44}, /* 720P shift left 40 pixel */
	{0x8B, CRL_REG_LEN_08BIT, 0x4F, 0x44}, /* 720P shift left 40 pixel */
	{0x8D, CRL_REG_LEN_08BIT, 0xD8, 0x44}, /* 720P shift left 40 pixel */

	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* LLC/PIX/AUD/SPI PINS TRISTATED */
	{0x10, CRL_REG_LEN_08BIT, 0xA0, 0xE0}, /* Enable 4-lane CSI Tx & Pixel Port */
	{0x1C, CRL_REG_LEN_08BIT, 0x3A, 0xE0}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_mode_VGA[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* YCrCb output */
	{0x05, CRL_REG_LEN_08BIT, 0x88, 0xE0}, /* Select Resolution VGA */
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* ADI recommended setting */
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0}, /* CP-Insert_AV_Code */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI recommended setting */

	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* LLC/PIX/AUD/SPI PINS TRISTATED */
	{0x10, CRL_REG_LEN_08BIT, 0xA0, 0xE0}, /* Enable 4-lane CSI Tx & Pixel Port */
	{0x1C, CRL_REG_LEN_08BIT, 0x3A, 0xE0}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_mode_1080i[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* YCrCb output */
	{0x05, CRL_REG_LEN_08BIT, 0x54, 0xE0}, /* Select Resolution 1080i*/
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* ADI recommended setting */
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0}, /* CP-Insert_AV_Code */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI recommended setting */

	{0x8B, CRL_REG_LEN_08BIT, 0x43, 0x44}, /* 1080i shift left 44 pixel */
	{0x8C, CRL_REG_LEN_08BIT, 0xD4, 0x44}, /* 1080i shift left 44 pixel */
	{0x8B, CRL_REG_LEN_08BIT, 0x4F, 0x44}, /* 1080i shift left 44 pixel */
	{0x8D, CRL_REG_LEN_08BIT, 0xD4, 0x44}, /* 1080i shift left 44 pixel */

	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* LLC/PIX/AUD/SPI PINS TRISTATED */
	{0x10, CRL_REG_LEN_08BIT, 0xA0, 0xE0}, /* Enable 4-lane CSI Tx & Pixel Port */
	{0x1C, CRL_REG_LEN_08BIT, 0x3A, 0xE0}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_mode_480i[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* YCrCb output */
	{0x05, CRL_REG_LEN_08BIT, 0x40, 0xE0}, /* Select Resolution 480i */
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* ADI recommended setting */
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0}, /* CP-Insert_AV_Code */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI recommended setting */

	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* LLC/PIX/AUD/SPI PINS TRISTATED */
	{0x10, CRL_REG_LEN_08BIT, 0xA0, 0xE0}, /* Enable 4-lane CSI Tx & Pixel Port */
	{0x1C, CRL_REG_LEN_08BIT, 0x3A, 0xE0}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_mode_576p[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* YCrCb output */
	{0x05, CRL_REG_LEN_08BIT, 0x4B, 0xE0}, /* Select Resolution 576p*/
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* ADI recommended setting */
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0}, /* CP-Insert_AV_Code */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI recommended setting */

	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* LLC/PIX/AUD/SPI PINS TRISTATED */
	{0x10, CRL_REG_LEN_08BIT, 0xA0, 0xE0}, /* Enable 4-lane CSI Tx & Pixel Port */
	{0x1C, CRL_REG_LEN_08BIT, 0x3A, 0xE0}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_mode_576i[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* YCrCb output */
	{0x05, CRL_REG_LEN_08BIT, 0x41, 0xE0}, /* Select Resolution 576i*/
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* ADI recommended setting */
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0}, /* CP-Insert_AV_Code */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI recommended setting */

	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* LLC/PIX/AUD/SPI PINS TRISTATED */
	{0x10, CRL_REG_LEN_08BIT, 0xA0, 0xE0}, /* Enable 4-lane CSI Tx & Pixel Port */
	{0x1C, CRL_REG_LEN_08BIT, 0x3A, 0xE0}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_streamon_regs[] = {
	{0x00, CRL_REG_LEN_DELAY, 0x02, 0x00},
	{0x00, CRL_REG_LEN_08BIT, 0x24, 0x94}, /* Power-up CSI-TX */
	{0x00, CRL_REG_LEN_DELAY, 0x01, 0x00},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94}, /* ADI recommended setting */
	{0x00, CRL_REG_LEN_DELAY, 0x01, 0x00},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_streamoff_regs[] = {
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94}, /* ADI Recommended Write */
	{0x1E, CRL_REG_LEN_08BIT, 0x00, 0x94}, /* Reset the clock Lane */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x00, 0x94}, /* i2c_mipi_pll_en - 1'b0 Disable MIPI PLL */
	{0xC1, CRL_REG_LEN_08BIT, 0x3B, 0x94},
};

static struct crl_sensor_detect_config adv7481_sensor_detect_regset[] = {
	{
		.reg = { 0x0019, CRL_REG_LEN_08BIT, 0x000000ff },
		.width = 5,
	},
	{
		.reg = { 0x0016, CRL_REG_LEN_16BIT, 0x0000ffff },
		.width = 7,
	},
};

static struct crl_pll_configuration adv7481_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 400000000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 800000000,
		.pixel_rate_pa = 800000000,
		.comp_items = 0,
		.ctrl_data = 0,
		.pll_regs_items = 0,
		.pll_regs = NULL,
	 },

};

static struct crl_subdev_rect_rep adv7481_1080p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
};

static struct crl_subdev_rect_rep adv7481_720p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1280,
		.out_rect.height = 720,
	},
};

static struct crl_subdev_rect_rep adv7481_VGA_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 640,
		.out_rect.height = 480,
	},
};

static struct crl_subdev_rect_rep adv7481_1080i_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 540,
	},
};

static struct crl_subdev_rect_rep adv7481_480i_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 720,
		.out_rect.height = 240,
	},
};

static struct crl_subdev_rect_rep adv7481_576p_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 720,
		.out_rect.height = 576,
	},
};

static struct crl_subdev_rect_rep adv7481_576i_rects[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 1920,
		.out_rect.height = 1080,
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.in_rect.left = 0,
		.in_rect.top = 0,
		.in_rect.width = 1920,
		.in_rect.height = 1080,
		.out_rect.left = 0,
		.out_rect.top = 0,
		.out_rect.width = 720,
		.out_rect.height = 288,
	},
};
static struct crl_mode_rep adv7481_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_1080p_rects),
		.sd_rects = adv7481_1080p_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1920,
		.height = 1080,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(adv7481_mode_1080p),
		.mode_regs = adv7481_mode_1080p,
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_720p_rects),
		.sd_rects = adv7481_720p_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1280,
		.height = 720,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(adv7481_mode_720p),
		.mode_regs = adv7481_mode_720p,
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_VGA_rects),
		.sd_rects = adv7481_VGA_rects,
		.binn_hor = 3,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 640,
		.height = 480,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(adv7481_mode_VGA),
		.mode_regs = adv7481_mode_VGA,
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_1080i_rects),
		.sd_rects = adv7481_1080i_rects,
		.binn_hor = 1,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 1920,
		.height = 540,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(adv7481_mode_1080i),
		.mode_regs = adv7481_mode_1080i,
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_480i_rects),
		.sd_rects = adv7481_480i_rects,
		.binn_hor = 2,
		.binn_vert = 4,
		.scale_m = 1,
		.width = 720,
		.height = 240,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(adv7481_mode_480i),
		.mode_regs = adv7481_mode_480i,
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_576p_rects),
		.sd_rects = adv7481_576p_rects,
		.binn_hor = 2,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 720,
		.height = 576,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(adv7481_mode_576p),
		.mode_regs = adv7481_mode_576p,
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_576i_rects),
		.sd_rects = adv7481_576i_rects,
		.binn_hor = 2,
		.binn_vert = 3,
		.scale_m = 1,
		.width = 720,
		.height = 288,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = 0,
		.mode_regs = adv7481_mode_576i,
	},
};

static struct crl_sensor_subdev_config adv7481_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "adv7481 binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "adv7481 pixel array",
	},
};

static struct crl_sensor_limits adv7481_sensor_limits = {
	.x_addr_min = 0,
	.y_addr_min = 0,
	.x_addr_max = 1920,
	.y_addr_max = 1080,
	.min_frame_length_lines = 160,
	.max_frame_length_lines = 65535,
	.min_line_length_pixels = 6024,
	.max_line_length_pixels = 32752,
	.scaler_m_min = 1,
	.scaler_m_max = 1,
	.scaler_n_min = 1,
	.scaler_n_max = 1,
	.min_even_inc = 1,
	.max_even_inc = 1,
	.min_odd_inc = 1,
	.max_odd_inc = 1,
};

static struct crl_csi_data_fmt adv7481_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_UYVY8_1X16,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 16,
		.regs_items = ARRAY_SIZE(adv7481_mode_1080p),
		.regs = adv7481_mode_1080p, /* default yuv422 format */
	},
};

static struct crl_v4l2_ctrl adv7481_v4l2_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = CRL_V4L2_CTRL_TYPE_MENU_INT,
		.data.v4l2_int_menu.def = 0,
		.data.v4l2_int_menu.max = 0,
		.data.v4l2_int_menu.menu = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_PA",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 0,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_PIXEL_RATE,
		.name = "V4L2_CID_PIXEL_RATE_CSI",
		.type = CRL_V4L2_CTRL_TYPE_INTEGER,
		.data.std_data.min = 0,
		.data.std_data.max = 0,
		.data.std_data.step = 1,
		.data.std_data.def = 0,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
	},
};

/* Power items, they are enabled in the order they are listed here */
static struct crl_power_seq_entity adv7481_power_items[] = {
	{
		.type = CRL_POWER_ETY_CLK_FRAMEWORK,
		.val = 24000000,
	},
	{
		.type = CRL_POWER_ETY_GPIO_FROM_PDATA,
		.val = 1,
	},
};

static struct crl_sensor_configuration adv7481_crl_configuration = {

	.power_items = ARRAY_SIZE(adv7481_power_items),
	.power_entities = adv7481_power_items,

	.powerup_regs_items = ARRAY_SIZE(adv7481_powerup_regset),
	.powerup_regs = adv7481_powerup_regset,

	.poweroff_regs_items = ARRAY_SIZE(adv7481_streamoff_regs),
	.poweroff_regs = adv7481_streamoff_regs,

	.id_reg_items = ARRAY_SIZE(adv7481_sensor_detect_regset),
	.id_regs = adv7481_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(adv7481_sensor_subdevs),
	.subdevs = adv7481_sensor_subdevs,

	.sensor_limits = &adv7481_sensor_limits,

	.pll_config_items = ARRAY_SIZE(adv7481_pll_configurations),
	.pll_configs = adv7481_pll_configurations,

	.modes_items = ARRAY_SIZE(adv7481_modes),
	.modes = adv7481_modes,

	.streamon_regs_items = ARRAY_SIZE(adv7481_streamon_regs),
	.streamon_regs = adv7481_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(adv7481_streamoff_regs),
	.streamoff_regs = adv7481_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(adv7481_v4l2_ctrls),
	.v4l2_ctrl_bank = adv7481_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(adv7481_crl_csi_data_fmt),
	.csi_fmts = adv7481_crl_csi_data_fmt,
};

#endif  /* __CRLMODULE_ADV7481_CONFIGURATION_H_ */
