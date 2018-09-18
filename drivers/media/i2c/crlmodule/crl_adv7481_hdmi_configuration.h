/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2016 - 2018 Intel Corporation */

#ifndef __CRLMODULE_ADV7481_HDMI_CONFIGURATION_H_
#define __CRLMODULE_ADV7481_HDMI_CONFIGURATION_H_

#include "crlmodule-sensor-ds.h"
irqreturn_t crl_adv7481_threaded_irq_fn(int irq, void *sensor_struct);

struct crl_ctrl_data_pair hdmi_ctrl_data_lanes[] = {
	{
		.ctrl_id = V4L2_CID_MIPI_LANES,
		.data = 4,
	},
	{
		.ctrl_id = V4L2_CID_MIPI_LANES,
		.data = 2,
	},
	{
		.ctrl_id = V4L2_CID_MIPI_LANES,
		.data = 1,
	},
};


static struct crl_register_write_rep adv7481_hdmi_onetime_init_regset[] = {
	{0xFF, CRL_REG_LEN_08BIT, 0xFF, 0xE0},
	{0x00, CRL_REG_LEN_DELAY, 0x05, 0x00},
	{0x01, CRL_REG_LEN_08BIT, 0x76, 0xE0}, /* ADI Required Write */
	{0x05, CRL_REG_LEN_08BIT, 0x96, 0xE0}, /* Setting Vid_Std to
					1600x1200(UXGA)@60 */
	{0xF2, CRL_REG_LEN_08BIT, 0x01, 0xE0}, /* Enable I2C Read
					Auto-Increment */
	{0xF3, CRL_REG_LEN_08BIT, 0x4C, 0xE0}, /* DPLL Map Address
					Set to 0x4C */
	{0xF4, CRL_REG_LEN_08BIT, 0x44, 0xE0}, /* CP Map Address
					Set to 0x44 */
	{0xF5, CRL_REG_LEN_08BIT, 0x68, 0xE0}, /* HDMI RX Map Address
					Set to 0x68 */
	{0xF6, CRL_REG_LEN_08BIT, 0x6C, 0xE0}, /* EDID Map Address
					Set to 0x6C */
	{0xF7, CRL_REG_LEN_08BIT, 0x64, 0xE0}, /* HDMI RX Repeater Map Address
					Set to 0x64 */
	{0xF8, CRL_REG_LEN_08BIT, 0x62, 0xE0}, /* HDMI RX Infoframe Map Address
					Set to 0x62 */
	{0xF9, CRL_REG_LEN_08BIT, 0xF0, 0xE0}, /* CBUS Map Address
					Set to 0xF0 */
	{0xFA, CRL_REG_LEN_08BIT, 0x82, 0xE0}, /* CEC Map Address
					Set to 0x82 */
	{0xFB, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* SDP Main Map Address
					Set to 0xF2 */
	{0xFC, CRL_REG_LEN_08BIT, 0x90, 0xE0}, /* CSI-TXB Map Address
					Set to 0x90 */
	{0xFD, CRL_REG_LEN_08BIT, 0x94, 0xE0}, /* CSI-TXA Map Address
					Set to 0x94 */
	{0x00, CRL_REG_LEN_08BIT, 0x40, 0xE0}, /* Disable chip powerdown &
					Enable HDMI Rx block */

	{0x40, CRL_REG_LEN_08BIT, 0xC3, 0x64}, /* Enable HDCP 1.1 Repeater */
	{0x69, CRL_REG_LEN_08BIT, 0x00, 0x64}, /* KSV List not ready port A */
	{0x77, CRL_REG_LEN_08BIT, 0x08, 0x64}, /* Clear KSV List */
	{0x78, CRL_REG_LEN_08BIT, 0x01, 0x64}, /* KSV_LIST_READY_CLR_A:
					Clears the BCAPS ready bit */
	{0x68, CRL_REG_LEN_08BIT, 0x00, 0x64}, /* Disable dual ksv list
					for port A */
	{0x41, CRL_REG_LEN_08BIT, 0x00, 0x64}, /* Reset b-status (1) */
	{0x42, CRL_REG_LEN_08BIT, 0x00, 0x64}, /* Reset b-status (2) */
	{0x91, CRL_REG_LEN_08BIT, 0x08, 0xE0}, /* AKSV Update Clear */

	{0x00, CRL_REG_LEN_08BIT, 0x08, 0x68}, /* Foreground Channel = A */
	{0x98, CRL_REG_LEN_08BIT, 0xFF, 0x68}, /* ADI Required Write */
	{0x99, CRL_REG_LEN_08BIT, 0xA3, 0x68}, /* ADI Required Write */
	{0x9A, CRL_REG_LEN_08BIT, 0x00, 0x68}, /* ADI Required Write */
	{0x9B, CRL_REG_LEN_08BIT, 0x0A, 0x68}, /* ADI Required Write */
	{0x9D, CRL_REG_LEN_08BIT, 0x40, 0x68}, /* ADI Required Write */
	{0xCB, CRL_REG_LEN_08BIT, 0x09, 0x68}, /* ADI Required Write */
	{0x3D, CRL_REG_LEN_08BIT, 0x10, 0x68}, /* ADI Required Write */
	{0x3E, CRL_REG_LEN_08BIT, 0x7B, 0x68}, /* ADI Required Write */
	{0x3F, CRL_REG_LEN_08BIT, 0x5E, 0x68}, /* ADI Required Write */
	{0x4E, CRL_REG_LEN_08BIT, 0xFE, 0x68}, /* ADI Required Write */
	{0x4F, CRL_REG_LEN_08BIT, 0x18, 0x68}, /* ADI Required Write */
	{0x57, CRL_REG_LEN_08BIT, 0xA3, 0x68}, /* ADI Required Write */
	{0x58, CRL_REG_LEN_08BIT, 0x04, 0x68}, /* ADI Required Write */
	{0x85, CRL_REG_LEN_08BIT, 0x10, 0x68}, /* ADI Required Write */
	{0x83, CRL_REG_LEN_08BIT, 0x00, 0x68}, /* Enable All Terminatio ns */
	{0xA3, CRL_REG_LEN_08BIT, 0x01, 0x68}, /* ADI Required Write */
	{0xBE, CRL_REG_LEN_08BIT, 0x00, 0x68}, /* ADI Required Write */
	{0x6C, CRL_REG_LEN_08BIT, 0x01, 0x68}, /* HPA Manual Enable */
	{0xF8, CRL_REG_LEN_08BIT, 0x01, 0x68}, /* HPA Asserted */
	{0x0F, CRL_REG_LEN_08BIT, 0x00, 0x68}, /* Audio Mute Speed
					Set to Fastest (Smallest Step Size) */
	{0x0E, CRL_REG_LEN_08BIT, 0xFF, 0xE0}, /* LLC/PIX/AUD/SPI PINS
					TRISTATED */

	{0x74, CRL_REG_LEN_08BIT, 0x43, 0xE0}, /* Enable interrupts */
	{0x75, CRL_REG_LEN_08BIT, 0x43, 0xE0},

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
};

static struct crl_register_write_rep adv7481_hdmi_mode_rgb565[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x02, 0xE0}, /* RGB Out of CP */
	{0x12, CRL_REG_LEN_08BIT, 0xF0, 0xE0}, /* CSC Depends on ip Packets - SDR 444 */
	{0x17, CRL_REG_LEN_08BIT, 0xB8, 0xE0}, /* Luma & Chroma Values Can Reach 254d */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI Required Write */
	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xDD, 0xE0},
	/* LLC/PIX/SPI PINS TRISTATED AUD Outputs Enabled */
	{0xDB, CRL_REG_LEN_08BIT, 0x10, 0x94}, /* ADI Required Write */
	 /* Enable 4-lane CSI TXB & Pixel Port */
	{0x7E, CRL_REG_LEN_08BIT, 0x98, 0x94}, /* ADI Required Write */
};

static struct crl_register_write_rep adv7481_hdmi_mode_rgb888[] = {
	{0x04, CRL_REG_LEN_08BIT, 0x02, 0xE0}, /* RGB Out of CP */
	{0x12, CRL_REG_LEN_08BIT, 0xF0, 0xE0}, /* CSC Depends on ip Packets - SDR 444 */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* Luma & Chroma Values Can Reach 254d */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI Required Write */
	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xDD, 0xE0},
	/* LLC/PIX/SPI PINS TRISTATED AUD Outputs Enabled */
	{0xDB, CRL_REG_LEN_08BIT, 0x10, 0x94}, /* ADI Required Write */
	{0x7E, CRL_REG_LEN_08BIT, 0x1B, 0x94}, /* ADI Required Write */
};


static struct crl_register_write_rep adv7481_hdmi_mode_uyvy[] = {
	{0x1C, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* ADI Require Write*/
	{0x04, CRL_REG_LEN_08BIT, 0x00, 0xE0}, /* YCrCb output */
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* Luma & Chroma Values Can Reach 254d */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI Required Write */
	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xDD, 0xE0}, /* LLC/PIX/SPI PINS TRISTATED AUD Outputs Enabled */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	 /* Enable 4-lane CSI TXB & Pixel Port */
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* Set Auto DPHY Timing */
	{0xDB, CRL_REG_LEN_08BIT, 0x10, 0x94}, /* ADI Required Write */
	{0x7E, CRL_REG_LEN_08BIT, 0x00, 0x94}, /* ADI Required Write */
};

static struct crl_register_write_rep adv7481_hdmi_mode_yuyv[] = {
	{0x1C, CRL_REG_LEN_08BIT, 0x3A, 0xE0}, /* Enable Interrupt*/
	{0x04, CRL_REG_LEN_08BIT, 0x40, 0xE0}, /* YCrCb output good=0xE0*/
	{0x12, CRL_REG_LEN_08BIT, 0xF2, 0xE0}, /* CSC Depends on ip Packets - SDR422 set */
	{0x17, CRL_REG_LEN_08BIT, 0x80, 0xE0}, /* Luma & Chroma Values Can Reach 254d */
	{0x7C, CRL_REG_LEN_08BIT, 0x00, 0x44}, /* ADI Required Write */
	{0x3E, CRL_REG_LEN_08BIT, 0x08, 0x44}, /* Invert order of Cb and Cr*/
	{0x0C, CRL_REG_LEN_08BIT, 0xE0, 0xE0}, /* Enable LLC_DLL & Double LLC Timing */
	{0x0E, CRL_REG_LEN_08BIT, 0xDD, 0xE0}, /* LLC/PIX/SPI PINS TRISTATED AUD Outputs Enabled */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	 /* Enable 4-lane CSI TXB & Pixel Port */
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* Set Auto DPHY Timing */
	{0xDB, CRL_REG_LEN_08BIT, 0x10, 0x94}, /* ADI Required Write */
	{0x7E, CRL_REG_LEN_08BIT, 0x00, 0x94}, /* ADI Required Write */
};

static struct crl_register_write_rep adv7481_hdmi_mode_1080p[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* Set Auto DPHY Timing */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94},
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94},
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94},
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94},
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94},
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94},
	{0x00, CRL_REG_LEN_08BIT, 0x24, 0x94},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xC9, CRL_REG_LEN_08BIT, 0x2D, 0x44},
	{0x05, CRL_REG_LEN_08BIT, 0x5E, 0xE0},
	{0x8B, CRL_REG_LEN_08BIT, 0x43, 0x44}, /* shift 44 pixel to right */
	{0x8C, CRL_REG_LEN_08BIT, 0xD4, 0x44},
	{0x8B, CRL_REG_LEN_08BIT, 0x4F, 0x44},
	{0x8D, CRL_REG_LEN_08BIT, 0xD4, 0x44},
};

static struct crl_register_write_rep adv7481_hdmi_mode_1080i[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* Set Auto DPHY Timing */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94},
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94},
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94},
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94},
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94},
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94},
	{0x00, CRL_REG_LEN_08BIT, 0x24, 0x94},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xC9, CRL_REG_LEN_08BIT, 0x2D, 0x44},
	{0x05, CRL_REG_LEN_08BIT, 0x54, 0xE0},
	{0x8B, CRL_REG_LEN_08BIT, 0x43, 0x44}, /* shift 44 pixel to right */
	{0x8C, CRL_REG_LEN_08BIT, 0xD4, 0x44},
	{0x8B, CRL_REG_LEN_08BIT, 0x4F, 0x44},
	{0x8D, CRL_REG_LEN_08BIT, 0xD4, 0x44},
};

static struct crl_register_write_rep adv7481_hdmi_mode_480p[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* Set Auto DPHY Timing */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94},
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94},
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94},
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94},
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94},
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94},
	{0x00, CRL_REG_LEN_08BIT, 0x24, 0x94},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xC9, CRL_REG_LEN_08BIT, 0x2D, 0x44},
	{0x05, CRL_REG_LEN_08BIT, 0x4A, 0xE0},
};

static struct crl_register_write_rep adv7481_hdmi_mode_720p[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* Set Auto DPHY Timing */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94},
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94},
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94},
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94},
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94},
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94},
	{0x00, CRL_REG_LEN_08BIT, 0x24, 0x94},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xC9, CRL_REG_LEN_08BIT, 0x2D, 0x44},
	{0x05, CRL_REG_LEN_08BIT, 0x53, 0xE0},
	{0x8B, CRL_REG_LEN_08BIT, 0x43, 0x44}, /* shift 40 pixel to right */
	{0x8C, CRL_REG_LEN_08BIT, 0xD8, 0x44},
	{0x8B, CRL_REG_LEN_08BIT, 0x4F, 0x44},
	{0x8D, CRL_REG_LEN_08BIT, 0xD8, 0x44},
};

static struct crl_register_write_rep adv7481_hdmi_mode_576p[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* Set Auto DPHY Timing */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94},
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94},
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94},
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94},
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94},
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94},
	{0x00, CRL_REG_LEN_08BIT, 0x24, 0x94},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xC9, CRL_REG_LEN_08BIT, 0x2D, 0x44},
	{0x05, CRL_REG_LEN_08BIT, 0x4B, 0xE0},
};

static struct crl_register_write_rep adv7481_hdmi_mode_576i[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x81, 0x94}, /* Enable 1-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA1, 0x94}, /* Set Auto DPHY Timing */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94},
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94},
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94},
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94},
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94},
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94},
	{0x00, CRL_REG_LEN_08BIT, 0x21, 0x94},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xC9, CRL_REG_LEN_08BIT, 0x2D, 0x44},
	{0x05, CRL_REG_LEN_08BIT, 0x41, 0xE0},
};

static struct crl_register_write_rep adv7481_hdmi_mode_480i[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x81, 0x94}, /* Enable 1-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA1, 0x94}, /* Set Auto DPHY Timing */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94},
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94},
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94},
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94},
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94},
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94},
	{0x00, CRL_REG_LEN_08BIT, 0x21, 0x94},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xC9, CRL_REG_LEN_08BIT, 0x2D, 0x44},
	{0x05, CRL_REG_LEN_08BIT, 0x40, 0xE0},
};

static struct crl_register_write_rep adv7481_hdmi_mode_vga[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /*  Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /*  Set Auto DPHY Timing */
	{0x10, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0xA0, 0xE0, 0xA0},
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94},
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94},
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94},
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94},
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94},
	{0x1E, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94},
	{0x00, CRL_REG_LEN_08BIT, 0x24, 0x94},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94},
	{0xC9, CRL_REG_LEN_08BIT, 0x2D, 0x44},
	{0x05, CRL_REG_LEN_08BIT, 0x88, 0xE0},
};

static struct crl_register_write_rep adv7481_hdmi_powerup_regset[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x84, 0x94}, /* Enable 4-lane MIPI */
	{0x00, CRL_REG_LEN_08BIT, 0xA4, 0x94}, /* Set Auto DPHY Timing */
	{0xDB, CRL_REG_LEN_08BIT, 0x10, 0x94}, /* ADI Required Write */
	{0xD6, CRL_REG_LEN_08BIT, 0x07, 0x94}, /* ADI Required Write */
	{0xC4, CRL_REG_LEN_08BIT, 0x0A, 0x94}, /* ADI Required Write */
	{0x71, CRL_REG_LEN_08BIT, 0x33, 0x94}, /* ADI Required Write */
	{0x72, CRL_REG_LEN_08BIT, 0x11, 0x94}, /* ADI Required Write */
	{0xF0, CRL_REG_LEN_08BIT, 0x00, 0x94}, /* i2c_dphy_pwdn - 1'b0 */
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94}, /* ADI Required Write */
	{0x1E, CRL_REG_LEN_08BIT, 0xC0, 0x94},
	/* ADI Required Write, transmit only Frame Start/End packets */
	{0xDA, CRL_REG_LEN_08BIT, 0x01, 0x94}, /* i2c_mipi_pll_en - 1'b1 */
};

static struct crl_register_write_rep adv7481_hdmi_streamon_regs[] = {
	{0x00, CRL_REG_LEN_DELAY, 0x02, 0x00},
	{0x00, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0x21, 0x94, 0xF8},
	/* Power-up CSI-TX */
	{0x00, CRL_REG_LEN_DELAY, 0x01, 0x00},
	{0xC1, CRL_REG_LEN_08BIT, 0x2B, 0x94}, /* ADI recommended setting */
	{0x00, CRL_REG_LEN_DELAY, 0x01, 0x00},
	{0x31, CRL_REG_LEN_08BIT, 0x80, 0x94}, /* ADI recommended setting */
};

static struct crl_register_write_rep adv7481_hdmi_streamoff_regs[] = {
	{0x31, CRL_REG_LEN_08BIT, 0x82, 0x94}, /* ADI Recommended Write */
	{0x1E, CRL_REG_LEN_08BIT, 0x00, 0x94}, /* Reset the clock Lane */
	{0x00, CRL_REG_LEN_08BIT, 0xA1, 0x94},
	{0xDA, CRL_REG_LEN_08BIT, 0x00, 0x94},
	/* i2c_mipi_pll_en -1'b0 Disable MIPI PLL */
	{0xC1, CRL_REG_LEN_08BIT, 0x3B, 0x94},
};

static struct crl_pll_configuration adv7481_hdmi_pll_configurations[] = {
	{
		.input_clk = 24000000,
		.op_sys_clk = 297000000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 594000000,
		.pixel_rate_pa = 594000000,
	},
	{
		.input_clk = 24000000,
		.op_sys_clk = 445500000,
		.bitsperpixel = 24,
		.pixel_rate_csi = 891000000,
		.pixel_rate_pa = 891000000,
	},
	/* 28.636 input clock */
	{
		.input_clk = 286363636,
		.op_sys_clk = 297000000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 148500000,
		.pixel_rate_pa = 297000000,
	},
	{
		.input_clk = 286363636,
		.op_sys_clk = 297000000,
		.bitsperpixel = 24,
		.pixel_rate_csi = 148500000,
		.pixel_rate_pa = 297000000,
	},
	{
		.input_clk = 286363636,
		.op_sys_clk = 148500000,
		.bitsperpixel = 16,
		.pixel_rate_csi = 74250000,
		.pixel_rate_pa = 148500000,
		.csi_lanes = 4,
	},
};

static struct crl_subdev_rect_rep adv7481_hdmi_1080p_rects[] = {
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

static struct crl_subdev_rect_rep adv7481_hdmi_720p_rects[] = {
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

static struct crl_subdev_rect_rep adv7481_hdmi_VGA_rects[] = {
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

static struct crl_subdev_rect_rep adv7481_hdmi_1080i_rects[] = {
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

static struct crl_subdev_rect_rep adv7481_hdmi_480p_rects[] = {
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
		.out_rect.height = 480,
	},
};

static struct crl_subdev_rect_rep adv7481_hdmi_480i_rects[] = {
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

static struct crl_subdev_rect_rep adv7481_hdmi_576p_rects[] = {
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

static struct crl_subdev_rect_rep adv7481_hdmi_576i_rects[] = {
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
static struct crl_mode_rep adv7481_hdmi_modes[] = {
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_hdmi_1080p_rects),
		.sd_rects = adv7481_hdmi_1080p_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1920,
		.height = 1080,
		.mode_regs_items = ARRAY_SIZE(adv7481_hdmi_mode_1080p),
		.mode_regs = adv7481_hdmi_mode_1080p,
		.comp_items = 1,
		.ctrl_data = &hdmi_ctrl_data_lanes[0],
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_hdmi_720p_rects),
		.sd_rects = adv7481_hdmi_720p_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1280,
		.height = 720,
		.mode_regs_items = ARRAY_SIZE(adv7481_hdmi_mode_720p),
		.mode_regs = adv7481_hdmi_mode_720p,
		.comp_items = 1,
		.ctrl_data = &hdmi_ctrl_data_lanes[0],
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_hdmi_VGA_rects),
		.sd_rects = adv7481_hdmi_VGA_rects,
		.binn_hor = 3,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 640,
		.height = 480,
		.mode_regs_items = ARRAY_SIZE(adv7481_hdmi_mode_vga),
		.mode_regs = adv7481_hdmi_mode_vga,
		.comp_items = 1,
		.ctrl_data = &hdmi_ctrl_data_lanes[0],
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_hdmi_1080i_rects),
		.sd_rects = adv7481_hdmi_1080i_rects,
		.binn_hor = 1,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 1920,
		.height = 540,
		.mode_regs_items = ARRAY_SIZE(adv7481_hdmi_mode_1080i),
		.mode_regs = adv7481_hdmi_mode_1080i,
		.comp_items = 1,
		.ctrl_data = &hdmi_ctrl_data_lanes[0],
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_hdmi_480p_rects),
		.sd_rects = adv7481_hdmi_480p_rects,
		.binn_hor = 2,
		.binn_vert = 2,
		.scale_m = 1,
		.width = 720,
		.height = 480,
		.mode_regs_items = ARRAY_SIZE(adv7481_hdmi_mode_480p),
		.mode_regs = adv7481_hdmi_mode_480p,
		.comp_items = 1,
		.ctrl_data = &hdmi_ctrl_data_lanes[0],
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_hdmi_480i_rects),
		.sd_rects = adv7481_hdmi_480i_rects,
		.binn_hor = 2,
		.binn_vert = 4,
		.scale_m = 1,
		.width = 720,
		.height = 240,
		.mode_regs_items = ARRAY_SIZE(adv7481_hdmi_mode_480i),
		.mode_regs = adv7481_hdmi_mode_480i,
		.comp_items = 1,
		.ctrl_data = &hdmi_ctrl_data_lanes[2],
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_hdmi_576p_rects),
		.sd_rects = adv7481_hdmi_576p_rects,
		.binn_hor = 2,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 720,
		.height = 576,
		.mode_regs_items = ARRAY_SIZE(adv7481_hdmi_mode_576p),
		.mode_regs = adv7481_hdmi_mode_576p,
		.comp_items = 1,
		.ctrl_data = &hdmi_ctrl_data_lanes[0],
	},
	{
		.sd_rects_items = ARRAY_SIZE(adv7481_hdmi_576i_rects),
		.sd_rects = adv7481_hdmi_576i_rects,
		.binn_hor = 2,
		.binn_vert = 3,
		.scale_m = 1,
		.width = 720,
		.height = 288,
		.mode_regs_items = ARRAY_SIZE(adv7481_hdmi_mode_576i),
		.mode_regs = adv7481_hdmi_mode_576i,
		.comp_items = 1,
		.ctrl_data = &hdmi_ctrl_data_lanes[2],
	},
};

static struct crl_sensor_subdev_config adv7481_hdmi_sensor_subdevs[] = {
	{
		.subdev_type = CRL_SUBDEV_TYPE_BINNER,
		.name = "adv7481-hdmi binner",
	},
	{
		.subdev_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.name = "adv7481-hdmi pixel array",
	},
};

static struct crl_sensor_limits adv7481_hdmi_sensor_limits = {
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

static struct crl_csi_data_fmt adv7481_hdmi_crl_csi_data_fmt[] = {
	{
		.code = MEDIA_BUS_FMT_RGB565_1X16,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 16,
		.regs_items = ARRAY_SIZE(adv7481_hdmi_mode_rgb565),
		.regs = adv7481_hdmi_mode_rgb565,
	},
	{
		.code = MEDIA_BUS_FMT_UYVY8_1X16,
		.pixel_order = CRL_PIXEL_ORDER_IGNORE,
		.bits_per_pixel = 16,
		.regs_items = ARRAY_SIZE(adv7481_hdmi_mode_uyvy),
		.regs = adv7481_hdmi_mode_uyvy,
	},
	{
		.code = MEDIA_BUS_FMT_RGB888_1X24,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
		.bits_per_pixel = 24,
		.regs_items = ARRAY_SIZE(adv7481_hdmi_mode_rgb888),
		.regs = adv7481_hdmi_mode_rgb888,
	},
	{
		.code = MEDIA_BUS_FMT_YUYV8_1X16,
		.pixel_order = CRL_PIXEL_ORDER_IGNORE,
		.bits_per_pixel = 16,
		.regs_items = ARRAY_SIZE(adv7481_hdmi_mode_yuyv),
		.regs = adv7481_hdmi_mode_yuyv,
	},
};

static const char * const adv7481_hdmi_test_pattern_menu[] = {
	"default",
	"30fps",
	"50fps",
	"60fps",
	"real",
};

static struct crl_register_write_rep adv7481_hdmi_test_pattern_default_mode[] = {
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0},
	{0x37, CRL_REG_LEN_08BIT, 0x81, 0x44},
	{0x00, CRL_REG_LEN_08BIT, 0x00, 0xE0},
};

static struct crl_register_write_rep adv7481_hdmi_test_pattern_30fps_mode[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x00, 0xE0},
	{0x03, CRL_REG_LEN_08BIT, 0xA6, 0xE0},
	{0x04, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0x80, 0xE0, 0xFD},
	{0x37, CRL_REG_LEN_08BIT, 0x85, 0x44},
};

static struct crl_register_write_rep adv7481_hdmi_test_pattern_50fps_mode[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x00, 0xE0},
	{0x03, CRL_REG_LEN_08BIT, 0x96, 0xE0},
	{0x04, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0x80, 0xE0, 0xFD},
	{0x37, CRL_REG_LEN_08BIT, 0x85, 0x44},
};

static struct crl_register_write_rep adv7481_hdmi_test_pattern_60fps_mode[] = {
	{0x00, CRL_REG_LEN_08BIT, 0x00, 0xE0},
	{0x03, CRL_REG_LEN_08BIT, 0x86, 0xE0},
	{0x04, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0x80, 0xE0, 0xFD},
	{0x37, CRL_REG_LEN_08BIT, 0x85, 0x44},
};

static struct crl_register_write_rep adv7481_hdmi_real_mode[] = {
	{0x00, CRL_REG_LEN_DELAY, 0x05, 0x00},
	{0x03, CRL_REG_LEN_08BIT, 0x00, 0xE0},
	{0x04, CRL_REG_LEN_08BIT | CRL_REG_READ_AND_UPDATE, 0x00, 0xE0, 0xFD},
	{0x37, CRL_REG_LEN_08BIT, 0x00, 0x44},
};

static struct crl_dep_reg_list adv7481_hdmi_test_pattern_fps_types_regs[] = {
	{ CRL_DEP_CTRL_CONDITION_EQUAL,
		{ CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST, 0 },
		ARRAY_SIZE(adv7481_hdmi_test_pattern_default_mode),
		adv7481_hdmi_test_pattern_default_mode, 0, 0 },
	{ CRL_DEP_CTRL_CONDITION_EQUAL,
		{ CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST, 1 },
		ARRAY_SIZE(adv7481_hdmi_test_pattern_30fps_mode),
		adv7481_hdmi_test_pattern_30fps_mode, 0, 0 },
	{ CRL_DEP_CTRL_CONDITION_EQUAL,
		{ CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST, 2 },
		ARRAY_SIZE(adv7481_hdmi_test_pattern_50fps_mode),
		adv7481_hdmi_test_pattern_50fps_mode, 0, 0 },
	{ CRL_DEP_CTRL_CONDITION_EQUAL,
		{ CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST, 3 },
		ARRAY_SIZE(adv7481_hdmi_test_pattern_60fps_mode),
		adv7481_hdmi_test_pattern_60fps_mode, 0, 0 },
	{ CRL_DEP_CTRL_CONDITION_EQUAL,
		{ CRL_DYNAMIC_VAL_OPERAND_TYPE_CONST, 4 },
		ARRAY_SIZE(adv7481_hdmi_real_mode),
		adv7481_hdmi_real_mode, 0, 0 },
};

static struct crl_v4l2_ctrl adv7481_hdmi_v4l2_ctrls[] = {
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_IDLE,
		.ctrl_id = V4L2_CID_LINK_FREQ,
		.name = "V4L2_CID_LINK_FREQ",
		.type = CRL_V4L2_CTRL_TYPE_MENU_INT,
		.impact = CRL_IMPACTS_NO_IMPACT,
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
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_BINNER,
		.op_type = CRL_V4L2_CTRL_GET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_MIPI_LANES,
		.name = "V4L2_CID_MIPI_LANES",
		.type = CRL_V4L2_CTRL_TYPE_CUSTOM,
		.data.std_data.min = 1,
		.data.std_data.max = 4,
		.data.std_data.step = 1,
		.data.std_data.def = 4,
		.flags = 0,
		.impact = CRL_IMPACTS_NO_IMPACT,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
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
	},
	{
		.sd_type = CRL_SUBDEV_TYPE_PIXEL_ARRAY,
		.op_type = CRL_V4L2_CTRL_SET_OP,
		.context = SENSOR_POWERED_ON,
		.ctrl_id = V4L2_CID_TEST_PATTERN,
		.name = "V4L2_CID_TEST_PATTERN",
		.type = CRL_V4L2_CTRL_TYPE_MENU_ITEMS,
		.data.v4l2_menu_items.menu = adv7481_hdmi_test_pattern_menu,
		.data.v4l2_menu_items.size = ARRAY_SIZE(adv7481_hdmi_test_pattern_menu),
		.impact = CRL_IMPACTS_NO_IMPACT,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.ctrl = 0,
		.regs_items = 0,
		.regs = 0,
		.dep_items = 0,
		.dep_ctrls = 0,
		.crl_ctrl_dep_reg_list = ARRAY_SIZE(adv7481_hdmi_test_pattern_fps_types_regs),
		.dep_regs = adv7481_hdmi_test_pattern_fps_types_regs,
	},
};

int adv7481_sensor_init(struct i2c_client *);
int adv7481_sensor_cleanup(struct i2c_client *);

static struct crl_sensor_configuration adv7481_hdmi_crl_configuration = {

	.sensor_init = adv7481_sensor_init,
	.sensor_cleanup = adv7481_sensor_cleanup,

	.onetime_init_regs_items =
		ARRAY_SIZE(adv7481_hdmi_onetime_init_regset),
	.onetime_init_regs = adv7481_hdmi_onetime_init_regset,

	.powerup_regs_items = ARRAY_SIZE(adv7481_hdmi_powerup_regset),
	.powerup_regs = adv7481_hdmi_powerup_regset,

	.poweroff_regs_items = ARRAY_SIZE(adv7481_hdmi_streamoff_regs),
	.poweroff_regs = adv7481_hdmi_streamoff_regs,

	.subdev_items = ARRAY_SIZE(adv7481_hdmi_sensor_subdevs),
	.subdevs = adv7481_hdmi_sensor_subdevs,

	.sensor_limits = &adv7481_hdmi_sensor_limits,

	.pll_config_items = ARRAY_SIZE(adv7481_hdmi_pll_configurations),
	.pll_configs = adv7481_hdmi_pll_configurations,

	.modes_items = ARRAY_SIZE(adv7481_hdmi_modes),
	.modes = adv7481_hdmi_modes,

	.streamon_regs_items = ARRAY_SIZE(adv7481_hdmi_streamon_regs),
	.streamon_regs = adv7481_hdmi_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(adv7481_hdmi_streamoff_regs),
	.streamoff_regs = adv7481_hdmi_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(adv7481_hdmi_v4l2_ctrls),
	.v4l2_ctrl_bank = adv7481_hdmi_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(adv7481_hdmi_crl_csi_data_fmt),
	.csi_fmts = adv7481_hdmi_crl_csi_data_fmt,

	.irq_in_use = false,
	.crl_irq_fn = NULL,
	.crl_threaded_irq_fn = crl_adv7481_threaded_irq_fn,

	.addr_len = CRL_ADDR_7BIT,
	.i2c_mutex_in_use = true,
};

#endif  /* __CRLMODULE_ADV7481_HDMI_CONFIGURATION_H_ */
