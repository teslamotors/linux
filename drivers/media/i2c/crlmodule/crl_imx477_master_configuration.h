/* SPDX-License_Identifier: GPL-2.0 */
/* Copyright (C) 2017 - 2018 Intel Corporation
 *
 * Author: Alexei Zavjalov <alexei.zavjalov@intel.com>
 *
 */

#ifndef __CRLMODULE_IMX477_MASTER_CONFIGURATION_H_
#define __CRLMODULE_IMX477_MASTER_CONFIGURATION_H_

#include "crl_imx477_common_regs.h"

static struct crl_register_write_rep imx477_onetime_init_regset_master[] = {
	{0x0103, CRL_REG_LEN_08BIT, 0x01}, /* Software reset        */

	{0x3010, CRL_REG_LEN_08BIT, 0x01}, /* SLAVE_ADD_EN_2ND      */
	{0x3011, CRL_REG_LEN_08BIT, 0x01}, /* SLAVE_ADD_ACKEN_2ND   */

	{0x3F0B, CRL_REG_LEN_08BIT, 0x01}, /* Multi camera mode: on */

	{0x3041, CRL_REG_LEN_08BIT, 0x01}, /* Mode: Master */
	{0x3040, CRL_REG_LEN_08BIT, 0x01}, /* XVS pin: out */
	{0x4B81, CRL_REG_LEN_08BIT, 0x01}, /* Mode: Master */

	{0x3042, CRL_REG_LEN_08BIT, 0x00}, /* VSYNC Delay in lines [15:8]  */
	{0x3043, CRL_REG_LEN_08BIT, 0x00}, /* VSYNC Delay in lines [7:0]   */
	{0x3044, CRL_REG_LEN_08BIT, 0x00}, /* VSYNC Delay in clocks [15:8] */
	{0x3045, CRL_REG_LEN_08BIT, 0x00}, /* VSYNC Delay in clocks [7:0]  */
	{0x3045, CRL_REG_LEN_08BIT, 0x00}, /* VSYNC thin down setting      */

	/* External Clock Setting */
	{0x0136, CRL_REG_LEN_08BIT, 0x13}, /* External clock freq (dec) [15:8] */
	{0x0137, CRL_REG_LEN_08BIT, 0x33}, /* External clock freq (dec) [7:0]  */

	/* Global Setting */
	{0x0808, CRL_REG_LEN_08BIT, 0x02}, /* MIPI Global Timing: Register Control */
	{0xE07A, CRL_REG_LEN_08BIT, 0x01},
	{0xE000, CRL_REG_LEN_08BIT, 0x00}, /* RUN/STOP of CSI2 during Frame Blanking: HS */
	{0x4AE9, CRL_REG_LEN_08BIT, 0x18},
	{0x4AEA, CRL_REG_LEN_08BIT, 0x08},
	{0xF61C, CRL_REG_LEN_08BIT, 0x04},
	{0xF61E, CRL_REG_LEN_08BIT, 0x04},
	{0x4AE9, CRL_REG_LEN_08BIT, 0x21},
	{0x4AEA, CRL_REG_LEN_08BIT, 0x80},
	{0x38A8, CRL_REG_LEN_08BIT, 0x1F},
	{0x38A9, CRL_REG_LEN_08BIT, 0xFF},
	{0x38AA, CRL_REG_LEN_08BIT, 0x1F},
	{0x38AB, CRL_REG_LEN_08BIT, 0xFF},
	{0x420B, CRL_REG_LEN_08BIT, 0x01},
	{0x55D4, CRL_REG_LEN_08BIT, 0x00},
	{0x55D5, CRL_REG_LEN_08BIT, 0x00},
	{0x55D6, CRL_REG_LEN_08BIT, 0x07},
	{0x55D7, CRL_REG_LEN_08BIT, 0xFF},
	{0x55E8, CRL_REG_LEN_08BIT, 0x07},
	{0x55E9, CRL_REG_LEN_08BIT, 0xFF},
	{0x55EA, CRL_REG_LEN_08BIT, 0x00},
	{0x55EB, CRL_REG_LEN_08BIT, 0x00},
	{0x574C, CRL_REG_LEN_08BIT, 0x07},
	{0x574D, CRL_REG_LEN_08BIT, 0xFF},
	{0x574E, CRL_REG_LEN_08BIT, 0x00},
	{0x574F, CRL_REG_LEN_08BIT, 0x00},
	{0x5754, CRL_REG_LEN_08BIT, 0x00},
	{0x5755, CRL_REG_LEN_08BIT, 0x00},
	{0x5756, CRL_REG_LEN_08BIT, 0x07},
	{0x5757, CRL_REG_LEN_08BIT, 0xFF},
	{0x5973, CRL_REG_LEN_08BIT, 0x04},
	{0x5974, CRL_REG_LEN_08BIT, 0x01},
	{0x5D13, CRL_REG_LEN_08BIT, 0xC3},
	{0x5D14, CRL_REG_LEN_08BIT, 0x58},
	{0x5D15, CRL_REG_LEN_08BIT, 0xA3},
	{0x5D16, CRL_REG_LEN_08BIT, 0x1D},
	{0x5D17, CRL_REG_LEN_08BIT, 0x65},
	{0x5D18, CRL_REG_LEN_08BIT, 0x8C},
	{0x5D1A, CRL_REG_LEN_08BIT, 0x06},
	{0x5D1B, CRL_REG_LEN_08BIT, 0xA9},
	{0x5D1C, CRL_REG_LEN_08BIT, 0x45},
	{0x5D1D, CRL_REG_LEN_08BIT, 0x3A},
	{0x5D1E, CRL_REG_LEN_08BIT, 0xAB},
	{0x5D1F, CRL_REG_LEN_08BIT, 0x15},
	{0x5D21, CRL_REG_LEN_08BIT, 0x0E},
	{0x5D22, CRL_REG_LEN_08BIT, 0x52},
	{0x5D23, CRL_REG_LEN_08BIT, 0xAA},
	{0x5D24, CRL_REG_LEN_08BIT, 0x7D},
	{0x5D25, CRL_REG_LEN_08BIT, 0x57},
	{0x5D26, CRL_REG_LEN_08BIT, 0xA8},
	{0x5D37, CRL_REG_LEN_08BIT, 0x5A},
	{0x5D38, CRL_REG_LEN_08BIT, 0x5A},
	{0x5D77, CRL_REG_LEN_08BIT, 0x7F},
	{0x7B7C, CRL_REG_LEN_08BIT, 0x00},
	{0x7B7D, CRL_REG_LEN_08BIT, 0x00},
	{0x8D1F, CRL_REG_LEN_08BIT, 0x00},
	{0x8D27, CRL_REG_LEN_08BIT, 0x00},
	{0x9004, CRL_REG_LEN_08BIT, 0x03},
	{0x9200, CRL_REG_LEN_08BIT, 0x50},
	{0x9201, CRL_REG_LEN_08BIT, 0x6C},
	{0x9202, CRL_REG_LEN_08BIT, 0x71},
	{0x9203, CRL_REG_LEN_08BIT, 0x00},
	{0x9204, CRL_REG_LEN_08BIT, 0x71},
	{0x9205, CRL_REG_LEN_08BIT, 0x01},
	{0x9371, CRL_REG_LEN_08BIT, 0x6A},
	{0x9373, CRL_REG_LEN_08BIT, 0x6A},
	{0x9375, CRL_REG_LEN_08BIT, 0x64},
	{0x990C, CRL_REG_LEN_08BIT, 0x00},
	{0x990D, CRL_REG_LEN_08BIT, 0x08},
	{0x9956, CRL_REG_LEN_08BIT, 0x8C},
	{0x9957, CRL_REG_LEN_08BIT, 0x64},
	{0x9958, CRL_REG_LEN_08BIT, 0x50},
	{0x9A48, CRL_REG_LEN_08BIT, 0x06},
	{0x9A49, CRL_REG_LEN_08BIT, 0x06},
	{0x9A4A, CRL_REG_LEN_08BIT, 0x06},
	{0x9A4B, CRL_REG_LEN_08BIT, 0x06},
	{0x9A4C, CRL_REG_LEN_08BIT, 0x06},
	{0x9A4D, CRL_REG_LEN_08BIT, 0x06},
	{0xA001, CRL_REG_LEN_08BIT, 0x0A},
	{0xA003, CRL_REG_LEN_08BIT, 0x0A},
	{0xA005, CRL_REG_LEN_08BIT, 0x0A},
	{0xA006, CRL_REG_LEN_08BIT, 0x01},
	{0xA007, CRL_REG_LEN_08BIT, 0xC0},
	{0xA009, CRL_REG_LEN_08BIT, 0xC0},

	/* Image Tuning */
	{0x3D8A, CRL_REG_LEN_08BIT, 0x01},
	{0x7B3B, CRL_REG_LEN_08BIT, 0x01},
	{0x7B4C, CRL_REG_LEN_08BIT, 0x00},
	{0x9905, CRL_REG_LEN_08BIT, 0x00},
	{0x9907, CRL_REG_LEN_08BIT, 0x00},
	{0x9909, CRL_REG_LEN_08BIT, 0x00},
	{0x990B, CRL_REG_LEN_08BIT, 0x00},
	{0x9944, CRL_REG_LEN_08BIT, 0x3C},
	{0x9947, CRL_REG_LEN_08BIT, 0x3C},
	{0x994A, CRL_REG_LEN_08BIT, 0x8C},
	{0x994B, CRL_REG_LEN_08BIT, 0x50},
	{0x994C, CRL_REG_LEN_08BIT, 0x1B},
	{0x994D, CRL_REG_LEN_08BIT, 0x8C},
	{0x994E, CRL_REG_LEN_08BIT, 0x50},
	{0x994F, CRL_REG_LEN_08BIT, 0x1B},
	{0x9950, CRL_REG_LEN_08BIT, 0x8C},
	{0x9951, CRL_REG_LEN_08BIT, 0x1B},
	{0x9952, CRL_REG_LEN_08BIT, 0x0A},
	{0x9953, CRL_REG_LEN_08BIT, 0x8C},
	{0x9954, CRL_REG_LEN_08BIT, 0x1B},
	{0x9955, CRL_REG_LEN_08BIT, 0x0A},
	{0x9A13, CRL_REG_LEN_08BIT, 0x04},
	{0x9A14, CRL_REG_LEN_08BIT, 0x04},
	{0x9A19, CRL_REG_LEN_08BIT, 0x00},
	{0x9A1C, CRL_REG_LEN_08BIT, 0x04},
	{0x9A1D, CRL_REG_LEN_08BIT, 0x04},
	{0x9A26, CRL_REG_LEN_08BIT, 0x05},
	{0x9A27, CRL_REG_LEN_08BIT, 0x05},
	{0x9A2C, CRL_REG_LEN_08BIT, 0x01},
	{0x9A2D, CRL_REG_LEN_08BIT, 0x03},
	{0x9A2F, CRL_REG_LEN_08BIT, 0x05},
	{0x9A30, CRL_REG_LEN_08BIT, 0x05},
	{0x9A41, CRL_REG_LEN_08BIT, 0x00},
	{0x9A46, CRL_REG_LEN_08BIT, 0x00},
	{0x9A47, CRL_REG_LEN_08BIT, 0x00},
	{0x9C17, CRL_REG_LEN_08BIT, 0x35},
	{0x9C1D, CRL_REG_LEN_08BIT, 0x31},
	{0x9C29, CRL_REG_LEN_08BIT, 0x50},
	{0x9C3B, CRL_REG_LEN_08BIT, 0x2F},
	{0x9C41, CRL_REG_LEN_08BIT, 0x6B},
	{0x9C47, CRL_REG_LEN_08BIT, 0x2D},
	{0x9C4D, CRL_REG_LEN_08BIT, 0x40},
	{0x9C6B, CRL_REG_LEN_08BIT, 0x00},
	{0x9C71, CRL_REG_LEN_08BIT, 0xC8},
	{0x9C73, CRL_REG_LEN_08BIT, 0x32},
	{0x9C75, CRL_REG_LEN_08BIT, 0x04},
	{0x9C7D, CRL_REG_LEN_08BIT, 0x2D},
	{0x9C83, CRL_REG_LEN_08BIT, 0x40},
	{0x9C94, CRL_REG_LEN_08BIT, 0x3F},
	{0x9C95, CRL_REG_LEN_08BIT, 0x3F},
	{0x9C96, CRL_REG_LEN_08BIT, 0x3F},
	{0x9C97, CRL_REG_LEN_08BIT, 0x00},
	{0x9C98, CRL_REG_LEN_08BIT, 0x00},
	{0x9C99, CRL_REG_LEN_08BIT, 0x00},
	{0x9C9A, CRL_REG_LEN_08BIT, 0x3F},
	{0x9C9B, CRL_REG_LEN_08BIT, 0x3F},
	{0x9C9C, CRL_REG_LEN_08BIT, 0x3F},
	{0x9CA0, CRL_REG_LEN_08BIT, 0x0F},
	{0x9CA1, CRL_REG_LEN_08BIT, 0x0F},
	{0x9CA2, CRL_REG_LEN_08BIT, 0x0F},
	{0x9CA3, CRL_REG_LEN_08BIT, 0x00},
	{0x9CA4, CRL_REG_LEN_08BIT, 0x00},
	{0x9CA5, CRL_REG_LEN_08BIT, 0x00},
	{0x9CA6, CRL_REG_LEN_08BIT, 0x1E},
	{0x9CA7, CRL_REG_LEN_08BIT, 0x1E},
	{0x9CA8, CRL_REG_LEN_08BIT, 0x1E},
	{0x9CA9, CRL_REG_LEN_08BIT, 0x00},
	{0x9CAA, CRL_REG_LEN_08BIT, 0x00},
	{0x9CAB, CRL_REG_LEN_08BIT, 0x00},
	{0x9CAC, CRL_REG_LEN_08BIT, 0x09},
	{0x9CAD, CRL_REG_LEN_08BIT, 0x09},
	{0x9CAE, CRL_REG_LEN_08BIT, 0x09},
	{0x9CBD, CRL_REG_LEN_08BIT, 0x50},
	{0x9CBF, CRL_REG_LEN_08BIT, 0x50},
	{0x9CC1, CRL_REG_LEN_08BIT, 0x50},
	{0x9CC3, CRL_REG_LEN_08BIT, 0x40},
	{0x9CC5, CRL_REG_LEN_08BIT, 0x40},
	{0x9CC7, CRL_REG_LEN_08BIT, 0x40},
	{0x9CC9, CRL_REG_LEN_08BIT, 0x0A},
	{0x9CCB, CRL_REG_LEN_08BIT, 0x0A},
	{0x9CCD, CRL_REG_LEN_08BIT, 0x0A},
	{0x9D17, CRL_REG_LEN_08BIT, 0x35},
	{0x9D1D, CRL_REG_LEN_08BIT, 0x31},
	{0x9D29, CRL_REG_LEN_08BIT, 0x50},
	{0x9D3B, CRL_REG_LEN_08BIT, 0x2F},
	{0x9D41, CRL_REG_LEN_08BIT, 0x6B},
	{0x9D47, CRL_REG_LEN_08BIT, 0x42},
	{0x9D4D, CRL_REG_LEN_08BIT, 0x5A},
	{0x9D6B, CRL_REG_LEN_08BIT, 0x00},
	{0x9D71, CRL_REG_LEN_08BIT, 0xC8},
	{0x9D73, CRL_REG_LEN_08BIT, 0x32},
	{0x9D75, CRL_REG_LEN_08BIT, 0x04},
	{0x9D7D, CRL_REG_LEN_08BIT, 0x42},
	{0x9D83, CRL_REG_LEN_08BIT, 0x5A},
	{0x9D94, CRL_REG_LEN_08BIT, 0x3F},
	{0x9D95, CRL_REG_LEN_08BIT, 0x3F},
	{0x9D96, CRL_REG_LEN_08BIT, 0x3F},
	{0x9D97, CRL_REG_LEN_08BIT, 0x00},
	{0x9D98, CRL_REG_LEN_08BIT, 0x00},
	{0x9D99, CRL_REG_LEN_08BIT, 0x00},
	{0x9D9A, CRL_REG_LEN_08BIT, 0x3F},
	{0x9D9B, CRL_REG_LEN_08BIT, 0x3F},
	{0x9D9C, CRL_REG_LEN_08BIT, 0x3F},
	{0x9D9D, CRL_REG_LEN_08BIT, 0x1F},
	{0x9D9E, CRL_REG_LEN_08BIT, 0x1F},
	{0x9D9F, CRL_REG_LEN_08BIT, 0x1F},
	{0x9DA0, CRL_REG_LEN_08BIT, 0x0F},
	{0x9DA1, CRL_REG_LEN_08BIT, 0x0F},
	{0x9DA2, CRL_REG_LEN_08BIT, 0x0F},
	{0x9DA3, CRL_REG_LEN_08BIT, 0x00},
	{0x9DA4, CRL_REG_LEN_08BIT, 0x00},
	{0x9DA5, CRL_REG_LEN_08BIT, 0x00},
	{0x9DA6, CRL_REG_LEN_08BIT, 0x1E},
	{0x9DA7, CRL_REG_LEN_08BIT, 0x1E},
	{0x9DA8, CRL_REG_LEN_08BIT, 0x1E},
	{0x9DA9, CRL_REG_LEN_08BIT, 0x00},
	{0x9DAA, CRL_REG_LEN_08BIT, 0x00},
	{0x9DAB, CRL_REG_LEN_08BIT, 0x00},
	{0x9DAC, CRL_REG_LEN_08BIT, 0x09},
	{0x9DAD, CRL_REG_LEN_08BIT, 0x09},
	{0x9DAE, CRL_REG_LEN_08BIT, 0x09},
	{0x9DC9, CRL_REG_LEN_08BIT, 0x0A},
	{0x9DCB, CRL_REG_LEN_08BIT, 0x0A},
	{0x9DCD, CRL_REG_LEN_08BIT, 0x0A},
	{0x9E17, CRL_REG_LEN_08BIT, 0x35},
	{0x9E1D, CRL_REG_LEN_08BIT, 0x31},
	{0x9E29, CRL_REG_LEN_08BIT, 0x50},
	{0x9E3B, CRL_REG_LEN_08BIT, 0x2F},
	{0x9E41, CRL_REG_LEN_08BIT, 0x6B},
	{0x9E47, CRL_REG_LEN_08BIT, 0x2D},
	{0x9E4D, CRL_REG_LEN_08BIT, 0x40},
	{0x9E6B, CRL_REG_LEN_08BIT, 0x00},
	{0x9E71, CRL_REG_LEN_08BIT, 0xC8},
	{0x9E73, CRL_REG_LEN_08BIT, 0x32},
	{0x9E75, CRL_REG_LEN_08BIT, 0x04},
	{0x9E94, CRL_REG_LEN_08BIT, 0x0F},
	{0x9E95, CRL_REG_LEN_08BIT, 0x0F},
	{0x9E96, CRL_REG_LEN_08BIT, 0x0F},
	{0x9E97, CRL_REG_LEN_08BIT, 0x00},
	{0x9E98, CRL_REG_LEN_08BIT, 0x00},
	{0x9E99, CRL_REG_LEN_08BIT, 0x00},
	{0x9EA0, CRL_REG_LEN_08BIT, 0x0F},
	{0x9EA1, CRL_REG_LEN_08BIT, 0x0F},
	{0x9EA2, CRL_REG_LEN_08BIT, 0x0F},
	{0x9EA3, CRL_REG_LEN_08BIT, 0x00},
	{0x9EA4, CRL_REG_LEN_08BIT, 0x00},
	{0x9EA5, CRL_REG_LEN_08BIT, 0x00},
	{0x9EA6, CRL_REG_LEN_08BIT, 0x3F},
	{0x9EA7, CRL_REG_LEN_08BIT, 0x3F},
	{0x9EA8, CRL_REG_LEN_08BIT, 0x3F},
	{0x9EA9, CRL_REG_LEN_08BIT, 0x00},
	{0x9EAA, CRL_REG_LEN_08BIT, 0x00},
	{0x9EAB, CRL_REG_LEN_08BIT, 0x00},
	{0x9EAC, CRL_REG_LEN_08BIT, 0x09},
	{0x9EAD, CRL_REG_LEN_08BIT, 0x09},
	{0x9EAE, CRL_REG_LEN_08BIT, 0x09},
	{0x9EC9, CRL_REG_LEN_08BIT, 0x0A},
	{0x9ECB, CRL_REG_LEN_08BIT, 0x0A},
	{0x9ECD, CRL_REG_LEN_08BIT, 0x0A},
	{0x9F17, CRL_REG_LEN_08BIT, 0x35},
	{0x9F1D, CRL_REG_LEN_08BIT, 0x31},
	{0x9F29, CRL_REG_LEN_08BIT, 0x50},
	{0x9F3B, CRL_REG_LEN_08BIT, 0x2F},
	{0x9F41, CRL_REG_LEN_08BIT, 0x6B},
	{0x9F47, CRL_REG_LEN_08BIT, 0x42},
	{0x9F4D, CRL_REG_LEN_08BIT, 0x5A},
	{0x9F6B, CRL_REG_LEN_08BIT, 0x00},
	{0x9F71, CRL_REG_LEN_08BIT, 0xC8},
	{0x9F73, CRL_REG_LEN_08BIT, 0x32},
	{0x9F75, CRL_REG_LEN_08BIT, 0x04},
	{0x9F94, CRL_REG_LEN_08BIT, 0x0F},
	{0x9F95, CRL_REG_LEN_08BIT, 0x0F},
	{0x9F96, CRL_REG_LEN_08BIT, 0x0F},
	{0x9F97, CRL_REG_LEN_08BIT, 0x00},
	{0x9F98, CRL_REG_LEN_08BIT, 0x00},
	{0x9F99, CRL_REG_LEN_08BIT, 0x00},
	{0x9F9A, CRL_REG_LEN_08BIT, 0x2F},
	{0x9F9B, CRL_REG_LEN_08BIT, 0x2F},
	{0x9F9C, CRL_REG_LEN_08BIT, 0x2F},
	{0x9F9D, CRL_REG_LEN_08BIT, 0x00},
	{0x9F9E, CRL_REG_LEN_08BIT, 0x00},
	{0x9F9F, CRL_REG_LEN_08BIT, 0x00},
	{0x9FA0, CRL_REG_LEN_08BIT, 0x0F},
	{0x9FA1, CRL_REG_LEN_08BIT, 0x0F},
	{0x9FA2, CRL_REG_LEN_08BIT, 0x0F},
	{0x9FA3, CRL_REG_LEN_08BIT, 0x00},
	{0x9FA4, CRL_REG_LEN_08BIT, 0x00},
	{0x9FA5, CRL_REG_LEN_08BIT, 0x00},
	{0x9FA6, CRL_REG_LEN_08BIT, 0x1E},
	{0x9FA7, CRL_REG_LEN_08BIT, 0x1E},
	{0x9FA8, CRL_REG_LEN_08BIT, 0x1E},
	{0x9FA9, CRL_REG_LEN_08BIT, 0x00},
	{0x9FAA, CRL_REG_LEN_08BIT, 0x00},
	{0x9FAB, CRL_REG_LEN_08BIT, 0x00},
	{0x9FAC, CRL_REG_LEN_08BIT, 0x09},
	{0x9FAD, CRL_REG_LEN_08BIT, 0x09},
	{0x9FAE, CRL_REG_LEN_08BIT, 0x09},
	{0x9FC9, CRL_REG_LEN_08BIT, 0x0A},
	{0x9FCB, CRL_REG_LEN_08BIT, 0x0A},
	{0x9FCD, CRL_REG_LEN_08BIT, 0x0A},
	{0xA14B, CRL_REG_LEN_08BIT, 0xFF},
	{0xA151, CRL_REG_LEN_08BIT, 0x0C},
	{0xA153, CRL_REG_LEN_08BIT, 0x50},
	{0xA155, CRL_REG_LEN_08BIT, 0x02},
	{0xA157, CRL_REG_LEN_08BIT, 0x00},
	{0xA1AD, CRL_REG_LEN_08BIT, 0xFF},
	{0xA1B3, CRL_REG_LEN_08BIT, 0x0C},
	{0xA1B5, CRL_REG_LEN_08BIT, 0x50},
	{0xA1B9, CRL_REG_LEN_08BIT, 0x00},
	{0xA24B, CRL_REG_LEN_08BIT, 0xFF},
	{0xA257, CRL_REG_LEN_08BIT, 0x00},
	{0xA2AD, CRL_REG_LEN_08BIT, 0xFF},
	{0xA2B9, CRL_REG_LEN_08BIT, 0x00},
	{0xB21F, CRL_REG_LEN_08BIT, 0x04},
	{0xB35C, CRL_REG_LEN_08BIT, 0x00},
	{0xB35E, CRL_REG_LEN_08BIT, 0x08},
};

static struct crl_register_write_rep imx477_4056_3040_19MHZ_master[] = {
	/* Frame Horizontal Clock Count */
	{0x0342, CRL_REG_LEN_08BIT, 0x39}, /* Line length [15:8]  */
	{0x0343, CRL_REG_LEN_08BIT, 0x14}, /* Line length [7:0]   */

	/* Frame Vertical Clock Count */
	{0x0340, CRL_REG_LEN_08BIT, 0x20}, /* Frame length [15:8] */
	{0x0341, CRL_REG_LEN_08BIT, 0x11}, /* Frame length [7:0]  */

	/* Visible Size */
	{0x0344, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [12:8] */
	{0x0345, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [7:0]  */
	{0x0346, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [12:8] */
	{0x0347, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [7:0]  */
	{0x0348, CRL_REG_LEN_08BIT, 0x0F}, /* Analog cropping end X [12:8]   */
	{0x0349, CRL_REG_LEN_08BIT, 0xD7}, /* Analog cropping end X [7:0]    */
	{0x034A, CRL_REG_LEN_08BIT, 0x0B}, /* Analog cropping end Y [12:8]   */
	{0x034B, CRL_REG_LEN_08BIT, 0xDF}, /* Analog cropping end Y [7:0]    */

	/* Mode Setting */
	{0x00E3, CRL_REG_LEN_08BIT, 0x00}, /* DOL-HDR Disable */
	{0x00E4, CRL_REG_LEN_08BIT, 0x00}, /* DOL Mode: DOL-HDR Disable */
	{0x0220, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x0221, CRL_REG_LEN_08BIT, 0x11}, /* Undocumented */
	{0x0381, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, even -> odd */
	{0x0383, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, odd -> even */
	{0x0385, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, even -> odd  */
	{0x0387, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, odd -> even  */
	{0x0900, CRL_REG_LEN_08BIT, 0x00}, /* Binning mode: Disable */
	{0x0901, CRL_REG_LEN_08BIT, 0x11}, /* Binning Type for Horizontal */
	{0x0902, CRL_REG_LEN_08BIT, 0x02}, /* Binning Type for Vertical   */
	{0x3140, CRL_REG_LEN_08BIT, 0x02}, /* Undocumented */
	{0x3C00, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x3C01, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x3C02, CRL_REG_LEN_08BIT, 0xDC}, /* Undocumented */
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00}, /* AD converter: 10 bit */
	{0x5748, CRL_REG_LEN_08BIT, 0x07}, /* Undocumented */
	{0x5749, CRL_REG_LEN_08BIT, 0xFF}, /* Undocumented */
	{0x574A, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x574B, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x7B75, CRL_REG_LEN_08BIT, 0x0E}, /* Undocumented */
	{0x7B76, CRL_REG_LEN_08BIT, 0x09}, /* Undocumented */
	{0x7B77, CRL_REG_LEN_08BIT, 0x0C}, /* Undocumented */
	{0x7B78, CRL_REG_LEN_08BIT, 0x06}, /* Undocumented */
	{0x7B79, CRL_REG_LEN_08BIT, 0x3B}, /* Undocumented */
	{0x7B53, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x9369, CRL_REG_LEN_08BIT, 0x5A}, /* Undocumented */
	{0x936B, CRL_REG_LEN_08BIT, 0x55}, /* Undocumented */
	{0x936D, CRL_REG_LEN_08BIT, 0x28}, /* Undocumented */
	{0x9304, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x9305, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9A, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9B, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9C, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9D, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9F, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xA2A9, CRL_REG_LEN_08BIT, 0x60}, /* Undocumented */
	{0xA2B7, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */

	/* Digital Crop & Scaling */
	{0x0401, CRL_REG_LEN_08BIT, 0x00}, /* Scaling mode: No Scaling     */
	{0x0404, CRL_REG_LEN_08BIT, 0x00}, /* Down Scaling Factor M [8]    */
	{0x0405, CRL_REG_LEN_08BIT, 0x10}, /* Down Scaling Factor M [7:0]  */
	{0x0408, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [12:8]    */
	{0x0409, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [7:0]     */
	{0x040A, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [12:8]    */
	{0x040B, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [7:0]     */
	{0x040C, CRL_REG_LEN_08BIT, 0x0F}, /* Width after cropping [12:8]  */
	{0x040D, CRL_REG_LEN_08BIT, 0xD8}, /* Width after cropping [7:0]   */
	{0x040E, CRL_REG_LEN_08BIT, 0x0B}, /* Height after cropping [12:8] */
	{0x040F, CRL_REG_LEN_08BIT, 0xE0}, /* Height after cropping [7:0]  */

	/* Output Crop */
	{0x034C, CRL_REG_LEN_08BIT, 0x0F}, /* X output size [12:8] */
	{0x034D, CRL_REG_LEN_08BIT, 0xD8}, /* X output size [7:0]  */
	{0x034E, CRL_REG_LEN_08BIT, 0x0B}, /* Y output size [12:8] */
	{0x034F, CRL_REG_LEN_08BIT, 0xE0}, /* Y output size [7:0]  */
};

static struct crl_register_write_rep imx477_4056_3040_19MHZ_DOL_2f_master[] = {
	/* Frame Horizontal Clock Count */
	{0x0342, CRL_REG_LEN_08BIT, 0x39}, /* Line length [15:8]  */
	{0x0343, CRL_REG_LEN_08BIT, 0x14}, /* Line length [7:0]   */
	/* Frame Vertical Clock Count */
	{0x0340, CRL_REG_LEN_08BIT, 0x20}, /* Frame length [15:8] */
	{0x0341, CRL_REG_LEN_08BIT, 0x11}, /* Frame length [7:0]  */
	/* Visible Size */
	{0x0344, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [12:8] */
	{0x0345, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [7:0]  */
	{0x0346, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [12:8] */
	{0x0347, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [7:0]  */
	{0x0348, CRL_REG_LEN_08BIT, 0x0F}, /* Analog cropping end X [12:8]   */
	{0x0349, CRL_REG_LEN_08BIT, 0xD7}, /* Analog cropping end X [7:0]    */
	{0x034A, CRL_REG_LEN_08BIT, 0x0B}, /* Analog cropping end Y [12:8]   */
	{0x034B, CRL_REG_LEN_08BIT, 0xDF}, /* Analog cropping end Y [7:0]    */
	/* Mode Setting */
	{0x00E3, CRL_REG_LEN_08BIT, 0x01}, /* DOL-HDR Enable */
	{0x00E4, CRL_REG_LEN_08BIT, 0x01}, /* DOL Mode: 2 frames in DOL-HDR */
	/* virtual channel ID of visible line and embedded line of DOL 2nd frame */
	{0x3E10, CRL_REG_LEN_08BIT, 0x01},
	{0x0220, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x0221, CRL_REG_LEN_08BIT, 0x11}, /* Undocumented */
	{0x0381, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, even -> odd */
	{0x0383, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, odd -> even */
	{0x0385, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, even -> odd  */
	{0x0387, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, odd -> even  */
	{0x0900, CRL_REG_LEN_08BIT, 0x00}, /* Binning mode: Disable */
	{0x0901, CRL_REG_LEN_08BIT, 0x11}, /* Binning Type for Horizontal */
	{0x0902, CRL_REG_LEN_08BIT, 0x02}, /* Binning Type for Vertical   */
	{0x3140, CRL_REG_LEN_08BIT, 0x02}, /* Undocumented */
	{0x3C00, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x3C01, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x3C02, CRL_REG_LEN_08BIT, 0xDC}, /* Undocumented */
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00}, /* AD converter: 10 bit */
	{0x5748, CRL_REG_LEN_08BIT, 0x07}, /* Undocumented */
	{0x5749, CRL_REG_LEN_08BIT, 0xFF}, /* Undocumented */
	{0x574A, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x574B, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x7B75, CRL_REG_LEN_08BIT, 0x0E}, /* Undocumented */
	{0x7B76, CRL_REG_LEN_08BIT, 0x09}, /* Undocumented */
	{0x7B77, CRL_REG_LEN_08BIT, 0x0C}, /* Undocumented */
	{0x7B78, CRL_REG_LEN_08BIT, 0x06}, /* Undocumented */
	{0x7B79, CRL_REG_LEN_08BIT, 0x3B}, /* Undocumented */
	{0x7B53, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x9369, CRL_REG_LEN_08BIT, 0x5A}, /* Undocumented */
	{0x936B, CRL_REG_LEN_08BIT, 0x55}, /* Undocumented */
	{0x936D, CRL_REG_LEN_08BIT, 0x28}, /* Undocumented */
	{0x9304, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x9305, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9A, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9B, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9C, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9D, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9F, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xA2A9, CRL_REG_LEN_08BIT, 0x60}, /* Undocumented */
	{0xA2B7, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	/* Digital Crop & Scaling */
	{0x0401, CRL_REG_LEN_08BIT, 0x00}, /* Scaling mode: No Scaling     */
	{0x0404, CRL_REG_LEN_08BIT, 0x00}, /* Down Scaling Factor M [8]    */
	{0x0405, CRL_REG_LEN_08BIT, 0x10}, /* Down Scaling Factor M [7:0]  */
	{0x0408, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [12:8]    */
	{0x0409, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [7:0]     */
	{0x040A, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [12:8]    */
	{0x040B, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [7:0]     */
	{0x040C, CRL_REG_LEN_08BIT, 0x0F}, /* Width after cropping [12:8]  */
	{0x040D, CRL_REG_LEN_08BIT, 0xD8}, /* Width after cropping [7:0]   */
	{0x040E, CRL_REG_LEN_08BIT, 0x0B}, /* Height after cropping [12:8] */
	{0x040F, CRL_REG_LEN_08BIT, 0xE0}, /* Height after cropping [7:0]  */
	/* Output Crop */
	{0x034C, CRL_REG_LEN_08BIT, 0x0F}, /* X output size [12:8] */
	{0x034D, CRL_REG_LEN_08BIT, 0xD8}, /* X output size [7:0]  */
	{0x034E, CRL_REG_LEN_08BIT, 0x0B}, /* Y output size [12:8] */
	{0x034F, CRL_REG_LEN_08BIT, 0xE0}, /* Y output size [7:0]  */
};

static struct crl_register_write_rep imx477_4056_3040_19MHZ_DOL_3f_master[] = {
	/* Frame Horizontal Clock Count */
	{0x0342, CRL_REG_LEN_08BIT, 0x39}, /* Line length [15:8]  */
	{0x0343, CRL_REG_LEN_08BIT, 0x14}, /* Line length [7:0]   */
	/* Frame Vertical Clock Count */
	{0x0340, CRL_REG_LEN_08BIT, 0x20}, /* Frame length [15:8] */
	{0x0341, CRL_REG_LEN_08BIT, 0x11}, /* Frame length [7:0]  */
	/* Visible Size */
	{0x0344, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [12:8] */
	{0x0345, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [7:0]  */
	{0x0346, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [12:8] */
	{0x0347, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [7:0]  */
	{0x0348, CRL_REG_LEN_08BIT, 0x0F}, /* Analog cropping end X [12:8]   */
	{0x0349, CRL_REG_LEN_08BIT, 0xD7}, /* Analog cropping end X [7:0]    */
	{0x034A, CRL_REG_LEN_08BIT, 0x0B}, /* Analog cropping end Y [12:8]   */
	{0x034B, CRL_REG_LEN_08BIT, 0xDF}, /* Analog cropping end Y [7:0]    */
	/* Mode Setting */
	{0x00E3, CRL_REG_LEN_08BIT, 0x01}, /* DOL-HDR Enable */
	{0x00E4, CRL_REG_LEN_08BIT, 0x02}, /* DOL Mode: 2 frames in DOL-HDR */
	/* virtual channel ID of visible line and embedded line of DOL 2nd frame */
	{0x3E10, CRL_REG_LEN_08BIT, 0x01},
	/* virtual channel ID of visible line and embedded line of DOL 3rd frame */
	{0x3E11, CRL_REG_LEN_08BIT, 0x02},
	{0x0220, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x0221, CRL_REG_LEN_08BIT, 0x11}, /* Undocumented */
	{0x0381, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, even -> odd */
	{0x0383, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, odd -> even */
	{0x0385, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, even -> odd  */
	{0x0387, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, odd -> even  */
	{0x0900, CRL_REG_LEN_08BIT, 0x00}, /* Binning mode: Disable */
	{0x0901, CRL_REG_LEN_08BIT, 0x11}, /* Binning Type for Horizontal */
	{0x0902, CRL_REG_LEN_08BIT, 0x02}, /* Binning Type for Vertical   */
	{0x3140, CRL_REG_LEN_08BIT, 0x02}, /* Undocumented */
	{0x3C00, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x3C01, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x3C02, CRL_REG_LEN_08BIT, 0xDC}, /* Undocumented */
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00}, /* AD converter: 10 bit */
	{0x5748, CRL_REG_LEN_08BIT, 0x07}, /* Undocumented */
	{0x5749, CRL_REG_LEN_08BIT, 0xFF}, /* Undocumented */
	{0x574A, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x574B, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x7B75, CRL_REG_LEN_08BIT, 0x0E}, /* Undocumented */
	{0x7B76, CRL_REG_LEN_08BIT, 0x09}, /* Undocumented */
	{0x7B77, CRL_REG_LEN_08BIT, 0x0C}, /* Undocumented */
	{0x7B78, CRL_REG_LEN_08BIT, 0x06}, /* Undocumented */
	{0x7B79, CRL_REG_LEN_08BIT, 0x3B}, /* Undocumented */
	{0x7B53, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x9369, CRL_REG_LEN_08BIT, 0x5A}, /* Undocumented */
	{0x936B, CRL_REG_LEN_08BIT, 0x55}, /* Undocumented */
	{0x936D, CRL_REG_LEN_08BIT, 0x28}, /* Undocumented */
	{0x9304, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x9305, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9A, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9B, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9C, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9D, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9F, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xA2A9, CRL_REG_LEN_08BIT, 0x60}, /* Undocumented */
	{0xA2B7, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	/* Digital Crop & Scaling */
	{0x0401, CRL_REG_LEN_08BIT, 0x00}, /* Scaling mode: No Scaling     */
	{0x0404, CRL_REG_LEN_08BIT, 0x00}, /* Down Scaling Factor M [8]    */
	{0x0405, CRL_REG_LEN_08BIT, 0x10}, /* Down Scaling Factor M [7:0]  */
	{0x0408, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [12:8]    */
	{0x0409, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [7:0]     */
	{0x040A, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [12:8]    */
	{0x040B, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [7:0]     */
	{0x040C, CRL_REG_LEN_08BIT, 0x0F}, /* Width after cropping [12:8]  */
	{0x040D, CRL_REG_LEN_08BIT, 0xD8}, /* Width after cropping [7:0]   */
	{0x040E, CRL_REG_LEN_08BIT, 0x0B}, /* Height after cropping [12:8] */
	{0x040F, CRL_REG_LEN_08BIT, 0xE0}, /* Height after cropping [7:0]  */
	/* Output Crop */
	{0x034C, CRL_REG_LEN_08BIT, 0x0F}, /* X output size [12:8] */
	{0x034D, CRL_REG_LEN_08BIT, 0xD8}, /* X output size [7:0]  */
	{0x034E, CRL_REG_LEN_08BIT, 0x0B}, /* Y output size [12:8] */
	{0x034F, CRL_REG_LEN_08BIT, 0xE0}, /* Y output size [7:0]  */
};

static struct crl_register_write_rep imx477_4056_2288_19MHZ_master[] = {
	/* Frame Horizontal Clock Count */
	{0x0342, CRL_REG_LEN_08BIT, 0x39}, /* Line length [15:8]  */
	{0x0343, CRL_REG_LEN_08BIT, 0x14}, /* Line length [7:0]   */

	/* Frame Vertical Clock Count */
	{0x0340, CRL_REG_LEN_08BIT, 0x20}, /* Frame length [15:8] */
	{0x0341, CRL_REG_LEN_08BIT, 0x11}, /* Frame length [7:0]  */

	/* Visible Size */
	/* (0,376) to (4055, 2664) */
	{0x0344, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [12:8] */
	{0x0345, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [7:0]  */
	{0x0346, CRL_REG_LEN_08BIT, 0x01}, /* Analog cropping start Y [12:8] */
	{0x0347, CRL_REG_LEN_08BIT, 0x78}, /* Analog cropping start Y [7:0]  */
	{0x0348, CRL_REG_LEN_08BIT, 0x0F}, /* Analog cropping end X [12:8]   */
	{0x0349, CRL_REG_LEN_08BIT, 0xD7}, /* Analog cropping end X [7:0]    */
	{0x034A, CRL_REG_LEN_08BIT, 0x0A}, /* Analog cropping end Y [12:8]   */
	{0x034B, CRL_REG_LEN_08BIT, 0x68}, /* Analog cropping end Y [7:0]    */

	/* Mode Setting */
	{0x00E3, CRL_REG_LEN_08BIT, 0x00}, /* DOL-HDR Disable */
	{0x00E4, CRL_REG_LEN_08BIT, 0x00}, /* DOL Mode: DOL-HDR Disable */
	{0x0220, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x0221, CRL_REG_LEN_08BIT, 0x11}, /* Undocumented */
	{0x0381, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, even -> odd */
	{0x0383, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, odd -> even */
	{0x0385, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, even -> odd  */
	{0x0387, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, odd -> even  */
	{0x0900, CRL_REG_LEN_08BIT, 0x00}, /* Binning mode: Disable */
	{0x0901, CRL_REG_LEN_08BIT, 0x11}, /* Binning Type for Horizontal */
	{0x0902, CRL_REG_LEN_08BIT, 0x02}, /* Binning Type for Vertical   */
	{0x3140, CRL_REG_LEN_08BIT, 0x02}, /* Undocumented */
	{0x3C00, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x3C01, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x3C02, CRL_REG_LEN_08BIT, 0xDC}, /* Undocumented */
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00}, /* AD converter: 10 bit */
	{0x5748, CRL_REG_LEN_08BIT, 0x07}, /* Undocumented */
	{0x5749, CRL_REG_LEN_08BIT, 0xFF}, /* Undocumented */
	{0x574A, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x574B, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x7B75, CRL_REG_LEN_08BIT, 0x0E}, /* Undocumented */
	{0x7B76, CRL_REG_LEN_08BIT, 0x09}, /* Undocumented */
	{0x7B77, CRL_REG_LEN_08BIT, 0x0C}, /* Undocumented */
	{0x7B78, CRL_REG_LEN_08BIT, 0x06}, /* Undocumented */
	{0x7B79, CRL_REG_LEN_08BIT, 0x3B}, /* Undocumented */
	{0x7B53, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x9369, CRL_REG_LEN_08BIT, 0x5A}, /* Undocumented */
	{0x936B, CRL_REG_LEN_08BIT, 0x55}, /* Undocumented */
	{0x936D, CRL_REG_LEN_08BIT, 0x28}, /* Undocumented */
	{0x9304, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x9305, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9A, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9B, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9C, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9D, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9F, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xA2A9, CRL_REG_LEN_08BIT, 0x60}, /* Undocumented */
	{0xA2B7, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */

	/* Digital Crop & Scaling */
	{0x0401, CRL_REG_LEN_08BIT, 0x00}, /* Scaling mode: No Scaling     */
	{0x0404, CRL_REG_LEN_08BIT, 0x00}, /* Down Scaling Factor M [8]    */
	{0x0405, CRL_REG_LEN_08BIT, 0x10}, /* Down Scaling Factor M [7:0]  */
	{0x0408, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [12:8]    */
	{0x0409, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [7:0]     */
	{0x040A, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [12:8]    */
	{0x040B, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [7:0]     */
	{0x040C, CRL_REG_LEN_08BIT, 0x0F}, /* Width after cropping [12:8]  */
	{0x040D, CRL_REG_LEN_08BIT, 0xD8}, /* Width after cropping [7:0]   */
	{0x040E, CRL_REG_LEN_08BIT, 0x08}, /* Height after cropping [12:8] */
	{0x040F, CRL_REG_LEN_08BIT, 0xF0}, /* Height after cropping [7:0]  */

	/* Output Crop */
	{0x034C, CRL_REG_LEN_08BIT, 0x0F}, /* X output size [12:8] */
	{0x034D, CRL_REG_LEN_08BIT, 0xD8}, /* X output size [7:0]  */
	{0x034E, CRL_REG_LEN_08BIT, 0x08}, /* Y output size [12:8] */
	{0x034F, CRL_REG_LEN_08BIT, 0xF0}, /* Y output size [7:0]  */
};


static struct crl_register_write_rep imx477_2832_1632_19MHZ_master[] = {
	/* Frame Horizontal Clock Count */
	{0x0342, CRL_REG_LEN_08BIT, 0x39}, /* Line length [15:8]  */
	{0x0343, CRL_REG_LEN_08BIT, 0x14}, /* Line length [7:0]   */

	/* Frame Vertical Clock Count */
	{0x0340, CRL_REG_LEN_08BIT, 0x20}, /* Frame length [15:8] */
	{0x0341, CRL_REG_LEN_08BIT, 0x11}, /* Frame length [7:0]  */

	/* Visible Size */
	{0x0344, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [12:8] */
	{0x0345, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [7:0]  */
	{0x0346, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [12:8] */
	{0x0347, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [7:0]  */
	{0x0348, CRL_REG_LEN_08BIT, 0x0F}, /* Analog cropping end X [12:8]   */
	{0x0349, CRL_REG_LEN_08BIT, 0xD7}, /* Analog cropping end X [7:0]    */
	{0x034A, CRL_REG_LEN_08BIT, 0x0B}, /* Analog cropping end Y [12:8]   */
	{0x034B, CRL_REG_LEN_08BIT, 0xDF}, /* Analog cropping end Y [7:0]    */

	/* Mode Setting */
	{0x00E3, CRL_REG_LEN_08BIT, 0x00}, /* DOL-HDR Disable */
	{0x00E4, CRL_REG_LEN_08BIT, 0x00}, /* DOL Mode: DOL-HDR Disable */
	{0x0220, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x0221, CRL_REG_LEN_08BIT, 0x11}, /* Undocumented */
	{0x0381, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, even -> odd */
	{0x0383, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, odd -> even */
	{0x0385, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, even -> odd  */
	{0x0387, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, odd -> even  */
	{0x0900, CRL_REG_LEN_08BIT, 0x00}, /* Binning mode: Disable */
	{0x0901, CRL_REG_LEN_08BIT, 0x11}, /* Binning Type for Horizontal */
	{0x0902, CRL_REG_LEN_08BIT, 0x02}, /* Binning Type for Vertical   */
	{0x3140, CRL_REG_LEN_08BIT, 0x02}, /* Undocumented */
	{0x3C00, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x3C01, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x3C02, CRL_REG_LEN_08BIT, 0xDC}, /* Undocumented */
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00}, /* AD converter: 10 bit */
	{0x5748, CRL_REG_LEN_08BIT, 0x07}, /* Undocumented */
	{0x5749, CRL_REG_LEN_08BIT, 0xFF}, /* Undocumented */
	{0x574A, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x574B, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x7B75, CRL_REG_LEN_08BIT, 0x0E}, /* Undocumented */
	{0x7B76, CRL_REG_LEN_08BIT, 0x09}, /* Undocumented */
	{0x7B77, CRL_REG_LEN_08BIT, 0x0C}, /* Undocumented */
	{0x7B78, CRL_REG_LEN_08BIT, 0x06}, /* Undocumented */
	{0x7B79, CRL_REG_LEN_08BIT, 0x3B}, /* Undocumented */
	{0x7B53, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x9369, CRL_REG_LEN_08BIT, 0x5A}, /* Undocumented */
	{0x936B, CRL_REG_LEN_08BIT, 0x55}, /* Undocumented */
	{0x936D, CRL_REG_LEN_08BIT, 0x28}, /* Undocumented */
	{0x9304, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x9305, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9A, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9B, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9C, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9D, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9F, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xA2A9, CRL_REG_LEN_08BIT, 0x60}, /* Undocumented */
	{0xA2B7, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */

	/* Digital Crop & Scaling */
	/* scale factor 16/22, 3894x2244 to 2832x1632 */
	{0x0401, CRL_REG_LEN_08BIT, 0x02}, /* Scaling mode: Scaling     */
	{0x0404, CRL_REG_LEN_08BIT, 0x00}, /* Down Scaling Factor M [8]    */
	{0x0405, CRL_REG_LEN_08BIT, 0x16}, /* Down Scaling Factor M [7:0]  */
	{0x0408, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [12:8]    */
	{0x0409, CRL_REG_LEN_08BIT, 0x52}, /* Crop Offset from X [7:0]     */
	{0x040A, CRL_REG_LEN_08BIT, 0x01}, /* Crop Offset from Y [12:8]    */
	{0x040B, CRL_REG_LEN_08BIT, 0x8E}, /* Crop Offset from Y [7:0]     */
	{0x040C, CRL_REG_LEN_08BIT, 0x0F}, /* Width after cropping [12:8]  */
	{0x040D, CRL_REG_LEN_08BIT, 0x36}, /* Width after cropping [7:0]   */
	{0x040E, CRL_REG_LEN_08BIT, 0x08}, /* Height after cropping [12:8] */
	{0x040F, CRL_REG_LEN_08BIT, 0xC4}, /* Height after cropping [7:0]  */

	/* Output Crop */
	{0x034C, CRL_REG_LEN_08BIT, 0x0B}, /* X output size [12:8] */
	{0x034D, CRL_REG_LEN_08BIT, 0x10}, /* X output size [7:0]  */
	{0x034E, CRL_REG_LEN_08BIT, 0x06}, /* Y output size [12:8] */
	{0x034F, CRL_REG_LEN_08BIT, 0x60}, /* Y output size [7:0]  */
};


static struct crl_register_write_rep imx477_2028_1128_19MHZ_master[] = {
	/* Frame Horizontal Clock Count */
	{0x0342, CRL_REG_LEN_08BIT, 0x39}, /* Line length [15:8]  */
	{0x0343, CRL_REG_LEN_08BIT, 0x14}, /* Line length [7:0]   */

	/* Frame Vertical Clock Count */
	{0x0340, CRL_REG_LEN_08BIT, 0x20}, /* Frame length [15:8] */
	{0x0341, CRL_REG_LEN_08BIT, 0x11}, /* Frame length [7:0]  */

	/* Visible Size */
	{0x0344, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [12:8] */
	{0x0345, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [7:0]  */
	{0x0346, CRL_REG_LEN_08BIT, 0x01}, /* Analog cropping start Y [12:8] */
	{0x0347, CRL_REG_LEN_08BIT, 0x88}, /* Analog cropping start Y [7:0]  */
	{0x0348, CRL_REG_LEN_08BIT, 0x0F}, /* Analog cropping end X [12:8]   */
	{0x0349, CRL_REG_LEN_08BIT, 0xD7}, /* Analog cropping end X [7:0]    */
	{0x034A, CRL_REG_LEN_08BIT, 0x0A}, /* Analog cropping end Y [12:8]   */
	{0x034B, CRL_REG_LEN_08BIT, 0x58}, /* Analog cropping end Y [7:0]    */

	/* Mode Setting */
	{0x00E3, CRL_REG_LEN_08BIT, 0x00}, /* DOL-HDR Disable */
	{0x00E4, CRL_REG_LEN_08BIT, 0x00}, /* DOL Mode: DOL-HDR Disable */
	{0x0220, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x0221, CRL_REG_LEN_08BIT, 0x11}, /* Undocumented */
	{0x0381, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, even -> odd */
	{0x0383, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, odd -> even */
	{0x0385, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, even -> odd  */
	{0x0387, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, odd -> even  */
	{0x0900, CRL_REG_LEN_08BIT, 0x01}, /* Binning mode: Disable */
	{0x0901, CRL_REG_LEN_08BIT, 0x22}, /* Binning Type for Horizontal */
	{0x0902, CRL_REG_LEN_08BIT, 0x02}, /* Binning Type for Vertical   */
	{0x3140, CRL_REG_LEN_08BIT, 0x02}, /* Undocumented */
	{0x3C00, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x3C01, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x3C02, CRL_REG_LEN_08BIT, 0xDC}, /* Undocumented */
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00}, /* AD converter: 10 bit */
	{0x5748, CRL_REG_LEN_08BIT, 0x07}, /* Undocumented */
	{0x5749, CRL_REG_LEN_08BIT, 0xFF}, /* Undocumented */
	{0x574A, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x574B, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x7B75, CRL_REG_LEN_08BIT, 0x0E}, /* Undocumented */
	{0x7B76, CRL_REG_LEN_08BIT, 0x09}, /* Undocumented */
	{0x7B77, CRL_REG_LEN_08BIT, 0x0C}, /* Undocumented */
	{0x7B78, CRL_REG_LEN_08BIT, 0x06}, /* Undocumented */
	{0x7B79, CRL_REG_LEN_08BIT, 0x3B}, /* Undocumented */
	{0x7B53, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x9369, CRL_REG_LEN_08BIT, 0x5A}, /* Undocumented */
	{0x936B, CRL_REG_LEN_08BIT, 0x55}, /* Undocumented */
	{0x936D, CRL_REG_LEN_08BIT, 0x28}, /* Undocumented */
	{0x9304, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x9305, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9A, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9B, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9C, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9D, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9F, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xA2A9, CRL_REG_LEN_08BIT, 0x60}, /* Undocumented */
	{0xA2B7, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */

	/* Digital Crop & Scaling */
	{0x0401, CRL_REG_LEN_08BIT, 0x00}, /* Scaling mode: No Scaling     */
	{0x0404, CRL_REG_LEN_08BIT, 0x00}, /* Down Scaling Factor M [8]    */
	{0x0405, CRL_REG_LEN_08BIT, 0x10}, /* Down Scaling Factor M [7:0]  */
	{0x0408, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [12:8]    */
	{0x0409, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [7:0]     */
	{0x040A, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [12:8]    */
	{0x040B, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [7:0]     */
	{0x040C, CRL_REG_LEN_08BIT, 0x07}, /* Width after cropping [12:8]  */
	{0x040D, CRL_REG_LEN_08BIT, 0xEC}, /* Width after cropping [7:0]   */
	{0x040E, CRL_REG_LEN_08BIT, 0x04}, /* Height after cropping [12:8] */
	{0x040F, CRL_REG_LEN_08BIT, 0x68}, /* Height after cropping [7:0]  */

	/* Output Crop */
	{0x034C, CRL_REG_LEN_08BIT, 0x07}, /* X output size [12:8] */
	{0x034D, CRL_REG_LEN_08BIT, 0xEC}, /* X output size [7:0]  */
	{0x034E, CRL_REG_LEN_08BIT, 0x04}, /* Y output size [12:8] */
	{0x034F, CRL_REG_LEN_08BIT, 0x68}, /* Y output size [7:0]  */
};

static struct crl_register_write_rep imx477_1296_768_19MHZ_master[] = {
	/* Frame Horizontal Clock Count */
	{0x0342, CRL_REG_LEN_08BIT, 0x39}, /* Line length [15:8]  */
	{0x0343, CRL_REG_LEN_08BIT, 0x14}, /* Line length [7:0]   */

	/* Frame Vertical Clock Count */
	{0x0340, CRL_REG_LEN_08BIT, 0x20}, /* Frame length [15:8] */
	{0x0341, CRL_REG_LEN_08BIT, 0x11}, /* Frame length [7:0]  */

	/* Visible Size */
	{0x0344, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [12:8] */
	{0x0345, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [7:0]  */
	{0x0346, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [12:8] */
	{0x0347, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [7:0]  */
	{0x0348, CRL_REG_LEN_08BIT, 0x0F}, /* Analog cropping end X [12:8]   */
	{0x0349, CRL_REG_LEN_08BIT, 0xD7}, /* Analog cropping end X [7:0]    */
	{0x034A, CRL_REG_LEN_08BIT, 0x0B}, /* Analog cropping end Y [12:8]   */
	{0x034B, CRL_REG_LEN_08BIT, 0xDF}, /* Analog cropping end Y [7:0]    */

	/* Mode Setting */
	{0x00E3, CRL_REG_LEN_08BIT, 0x00}, /* DOL-HDR Disable */
	{0x00E4, CRL_REG_LEN_08BIT, 0x00}, /* DOL Mode: DOL-HDR Disable */
	{0x0220, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x0221, CRL_REG_LEN_08BIT, 0x11}, /* Undocumented */
	{0x0381, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, even -> odd */
	{0x0383, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, odd -> even */
	{0x0385, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, even -> odd  */
	{0x0387, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, odd -> even  */
	{0x0900, CRL_REG_LEN_08BIT, 0x00}, /* Binning mode: Disable */
	{0x0901, CRL_REG_LEN_08BIT, 0x11}, /* Binning Type for Horizontal */
	{0x0902, CRL_REG_LEN_08BIT, 0x02}, /* Binning Type for Vertical   */
	{0x3140, CRL_REG_LEN_08BIT, 0x02}, /* Undocumented */
	{0x3C00, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x3C01, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x3C02, CRL_REG_LEN_08BIT, 0xDC}, /* Undocumented */
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00}, /* AD converter: 10 bit */
	{0x5748, CRL_REG_LEN_08BIT, 0x07}, /* Undocumented */
	{0x5749, CRL_REG_LEN_08BIT, 0xFF}, /* Undocumented */
	{0x574A, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x574B, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x7B75, CRL_REG_LEN_08BIT, 0x0E}, /* Undocumented */
	{0x7B76, CRL_REG_LEN_08BIT, 0x09}, /* Undocumented */
	{0x7B77, CRL_REG_LEN_08BIT, 0x0C}, /* Undocumented */
	{0x7B78, CRL_REG_LEN_08BIT, 0x06}, /* Undocumented */
	{0x7B79, CRL_REG_LEN_08BIT, 0x3B}, /* Undocumented */
	{0x7B53, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x9369, CRL_REG_LEN_08BIT, 0x5A}, /* Undocumented */
	{0x936B, CRL_REG_LEN_08BIT, 0x55}, /* Undocumented */
	{0x936D, CRL_REG_LEN_08BIT, 0x28}, /* Undocumented */
	{0x9304, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x9305, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9A, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9B, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9C, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9D, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9F, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xA2A9, CRL_REG_LEN_08BIT, 0x60}, /* Undocumented */
	{0xA2B7, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */

	/* Digital Crop & Scaling */
	/* scale factor 16/50, 4050x2400 to 1296x768 */
	{0x0401, CRL_REG_LEN_08BIT, 0x02}, /* Scaling mode: Scaling     */
	{0x0404, CRL_REG_LEN_08BIT, 0x00}, /* Down Scaling Factor M [8]    */
	{0x0405, CRL_REG_LEN_08BIT, 0x32}, /* Down Scaling Factor M [7:0]  */
	{0x0408, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [12:8]    */
	{0x0409, CRL_REG_LEN_08BIT, 0x04}, /* Crop Offset from X [7:0]     */
	{0x040A, CRL_REG_LEN_08BIT, 0x01}, /* Crop Offset from Y [12:8]    */
	{0x040B, CRL_REG_LEN_08BIT, 0x40}, /* Crop Offset from Y [7:0]     */
	{0x040C, CRL_REG_LEN_08BIT, 0x0F}, /* Width after cropping [12:8]  */
	{0x040D, CRL_REG_LEN_08BIT, 0xD2}, /* Width after cropping [7:0]   */
	{0x040E, CRL_REG_LEN_08BIT, 0x09}, /* Height after cropping [12:8] */
	{0x040F, CRL_REG_LEN_08BIT, 0x60}, /* Height after cropping [7:0]  */

	/* Output Crop */
	{0x034C, CRL_REG_LEN_08BIT, 0x05}, /* X output size [12:8] */
	{0x034D, CRL_REG_LEN_08BIT, 0x10}, /* X output size [7:0]  */
	{0x034E, CRL_REG_LEN_08BIT, 0x03}, /* Y output size [12:8] */
	{0x034F, CRL_REG_LEN_08BIT, 0x00}, /* Y output size [7:0]  */
};

static struct crl_register_write_rep imx477_656_512_19MHZ_master[] = {
	/* Frame Horizontal Clock Count */
	{0x0342, CRL_REG_LEN_08BIT, 0x39}, /* Line length [15:8]  */
	{0x0343, CRL_REG_LEN_08BIT, 0x14}, /* Line length [7:0]   */

	/* Frame Vertical Clock Count */
	{0x0340, CRL_REG_LEN_08BIT, 0x20}, /* Frame length [15:8] */
	{0x0341, CRL_REG_LEN_08BIT, 0x11}, /* Frame length [7:0]  */

	/* Visible Size */
	{0x0344, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [12:8] */
	{0x0345, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start X [7:0]  */
	{0x0346, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [12:8] */
	{0x0347, CRL_REG_LEN_08BIT, 0x00}, /* Analog cropping start Y [7:0]  */
	{0x0348, CRL_REG_LEN_08BIT, 0x0F}, /* Analog cropping end X [12:8]   */
	{0x0349, CRL_REG_LEN_08BIT, 0xD7}, /* Analog cropping end X [7:0]    */
	{0x034A, CRL_REG_LEN_08BIT, 0x0B}, /* Analog cropping end Y [12:8]   */
	{0x034B, CRL_REG_LEN_08BIT, 0xDF}, /* Analog cropping end Y [7:0]    */

	/* Mode Setting */
	{0x00E3, CRL_REG_LEN_08BIT, 0x00}, /* DOL-HDR Disable */
	{0x00E4, CRL_REG_LEN_08BIT, 0x00}, /* DOL Mode: DOL-HDR Disable */
	{0x0220, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x0221, CRL_REG_LEN_08BIT, 0x11}, /* Undocumented */
	{0x0381, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, even -> odd */
	{0x0383, CRL_REG_LEN_08BIT, 0x01}, /* Num of pixels skipped, odd -> even */
	{0x0385, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, even -> odd  */
	{0x0387, CRL_REG_LEN_08BIT, 0x01}, /* Num of lines skipped, odd -> even  */
	{0x0900, CRL_REG_LEN_08BIT, 0x00}, /* Binning mode: Disable */
	{0x0901, CRL_REG_LEN_08BIT, 0x11}, /* Binning Type for Horizontal */
	{0x0902, CRL_REG_LEN_08BIT, 0x02}, /* Binning Type for Vertical   */
	{0x3140, CRL_REG_LEN_08BIT, 0x02}, /* Undocumented */
	{0x3C00, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x3C01, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x3C02, CRL_REG_LEN_08BIT, 0xDC}, /* Undocumented */
	{0x3F0D, CRL_REG_LEN_08BIT, 0x00}, /* AD converter: 10 bit */
	{0x5748, CRL_REG_LEN_08BIT, 0x07}, /* Undocumented */
	{0x5749, CRL_REG_LEN_08BIT, 0xFF}, /* Undocumented */
	{0x574A, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x574B, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x7B75, CRL_REG_LEN_08BIT, 0x0E}, /* Undocumented */
	{0x7B76, CRL_REG_LEN_08BIT, 0x09}, /* Undocumented */
	{0x7B77, CRL_REG_LEN_08BIT, 0x0C}, /* Undocumented */
	{0x7B78, CRL_REG_LEN_08BIT, 0x06}, /* Undocumented */
	{0x7B79, CRL_REG_LEN_08BIT, 0x3B}, /* Undocumented */
	{0x7B53, CRL_REG_LEN_08BIT, 0x01}, /* Undocumented */
	{0x9369, CRL_REG_LEN_08BIT, 0x5A}, /* Undocumented */
	{0x936B, CRL_REG_LEN_08BIT, 0x55}, /* Undocumented */
	{0x936D, CRL_REG_LEN_08BIT, 0x28}, /* Undocumented */
	{0x9304, CRL_REG_LEN_08BIT, 0x03}, /* Undocumented */
	{0x9305, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9A, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9B, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9C, CRL_REG_LEN_08BIT, 0x2F}, /* Undocumented */
	{0x9E9D, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9E, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0x9E9F, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */
	{0xA2A9, CRL_REG_LEN_08BIT, 0x60}, /* Undocumented */
	{0xA2B7, CRL_REG_LEN_08BIT, 0x00}, /* Undocumented */

	/* Digital Crop & Scaling */
	/* scale factor 16/95, 3895x3040 to 656x512 */
	{0x0401, CRL_REG_LEN_08BIT, 0x02}, /* Scaling mode: Scaling     */
	{0x0404, CRL_REG_LEN_08BIT, 0x00}, /* Down Scaling Factor M [8]    */
	{0x0405, CRL_REG_LEN_08BIT, 0x5F}, /* Down Scaling Factor M [7:0]  */
	{0x0408, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from X [12:8]    */
	{0x0409, CRL_REG_LEN_08BIT, 0x50}, /* Crop Offset from X [7:0]     */
	{0x040A, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [12:8]    */
	{0x040B, CRL_REG_LEN_08BIT, 0x00}, /* Crop Offset from Y [7:0]     */
	{0x040C, CRL_REG_LEN_08BIT, 0x0F}, /* Width after cropping [12:8]  */
	{0x040D, CRL_REG_LEN_08BIT, 0x37}, /* Width after cropping [7:0]   */
	{0x040E, CRL_REG_LEN_08BIT, 0x0B}, /* Height after cropping [12:8] */
	{0x040F, CRL_REG_LEN_08BIT, 0xE0}, /* Height after cropping [7:0]  */

	/* Output Crop */
	{0x034C, CRL_REG_LEN_08BIT, 0x02}, /* X output size [12:8] */
	{0x034D, CRL_REG_LEN_08BIT, 0x90}, /* X output size [7:0]  */
	{0x034E, CRL_REG_LEN_08BIT, 0x02}, /* Y output size [12:8] */
	{0x034F, CRL_REG_LEN_08BIT, 0x00}, /* Y output size [7:0]  */
};

static struct crl_mode_rep imx477_modes_master[] = {
	{
		.sd_rects_items = ARRAY_SIZE(imx477_4056_3040_rects),
		.sd_rects = imx477_4056_3040_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 4056,
		.height = 3040,
		.min_llp = 14612,
		.min_fll = 8209,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx477_4056_3040_19MHZ_master),
		.mode_regs = imx477_4056_3040_19MHZ_master,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx477_4056_3040_rects),
		.sd_rects = imx477_4056_3040_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 4056,
		.height = 3040,
		.min_llp = 14612,
		.min_fll = 8209,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx477_4056_3040_19MHZ_DOL_2f_master),
		.mode_regs = imx477_4056_3040_19MHZ_DOL_2f_master,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx477_4056_3040_rects),
		.sd_rects = imx477_4056_3040_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 4056,
		.height = 3040,
		.min_llp = 14612,
		.min_fll = 8209,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx477_4056_3040_19MHZ_DOL_3f_master),
		.mode_regs = imx477_4056_3040_19MHZ_DOL_3f_master,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx477_4056_2288_rects),
		.sd_rects = imx477_4056_2288_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 4056,
		.height = 2288,
		.min_llp = 14612,
		.min_fll = 8209,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx477_4056_2288_19MHZ_master),
		.mode_regs = imx477_4056_2288_19MHZ_master,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx477_2832_1632_rects),
		.sd_rects = imx477_2832_1632_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 2832,
		.height = 1632,
		.min_llp = 14612,
		.min_fll = 8209,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx477_2832_1632_19MHZ_master),
		.mode_regs = imx477_2832_1632_19MHZ_master,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx477_2028_1128_rects),
		.sd_rects = imx477_2028_1128_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 2028,
		.height = 1128,
		.min_llp = 14612,
		.min_fll = 8209,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx477_2028_1128_19MHZ_master),
		.mode_regs = imx477_2028_1128_19MHZ_master,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx477_1296_768_rects),
		.sd_rects = imx477_1296_768_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 1296,
		.height = 768,
		.min_llp = 14612,
		.min_fll = 8209,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx477_1296_768_19MHZ_master),
		.mode_regs = imx477_1296_768_19MHZ_master,
	},
	{
		.sd_rects_items = ARRAY_SIZE(imx477_656_512_rects),
		.sd_rects = imx477_656_512_rects,
		.binn_hor = 1,
		.binn_vert = 1,
		.scale_m = 1,
		.width = 656,
		.height = 512,
		.min_llp = 14612,
		.min_fll = 8209,
		.comp_items = 0,
		.ctrl_data = 0,
		.mode_regs_items = ARRAY_SIZE(imx477_656_512_19MHZ_master),
		.mode_regs = imx477_656_512_19MHZ_master,
	},
};

static struct crl_flip_data imx477_flip_configurations_master[] = {
	{
		.flip = CRL_FLIP_DEFAULT_NONE,
		.pixel_order = CRL_PIXEL_ORDER_RGGB,
	},
	{
		.flip = CRL_FLIP_HFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GRBG,
	},
	{
		.flip = CRL_FLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_GBRG,
	},
	{
		.flip = CRL_FLIP_HFLIP_VFLIP,
		.pixel_order = CRL_PIXEL_ORDER_BGGR,
	}
};

struct crl_sensor_configuration imx477_master_crl_configuration = {

	.power_items = ARRAY_SIZE(imx477_power_items),
	.power_entities = imx477_power_items,

	.onetime_init_regs_items = ARRAY_SIZE(imx477_onetime_init_regset_master),
	.onetime_init_regs = imx477_onetime_init_regset_master,

	.powerup_regs_items = ARRAY_SIZE(imx477_powerup_standby),
	.powerup_regs = imx477_powerup_standby,

	.poweroff_regs_items = 0,
	.poweroff_regs = 0,

	.id_reg_items = ARRAY_SIZE(imx477_sensor_detect_regset),
	.id_regs = imx477_sensor_detect_regset,

	.subdev_items = ARRAY_SIZE(imx477_sensor_subdevs),
	.subdevs = imx477_sensor_subdevs,

	.sensor_limits = &imx477_sensor_limits,

	.pll_config_items = ARRAY_SIZE(imx477_pll_configurations),
	.pll_configs = imx477_pll_configurations,

	.modes_items = ARRAY_SIZE(imx477_modes_master),
	.modes = imx477_modes_master,

	.streamon_regs_items = ARRAY_SIZE(imx477_streamon_regs),
	.streamon_regs = imx477_streamon_regs,

	.streamoff_regs_items = ARRAY_SIZE(imx477_streamoff_regs),
	.streamoff_regs = imx477_streamoff_regs,

	.v4l2_ctrls_items = ARRAY_SIZE(imx477_v4l2_ctrls),
	.v4l2_ctrl_bank = imx477_v4l2_ctrls,

	.csi_fmts_items = ARRAY_SIZE(imx477_crl_csi_data_fmt),
	.csi_fmts = imx477_crl_csi_data_fmt,

	.flip_items = ARRAY_SIZE(imx477_flip_configurations_master),
	.flip_data = imx477_flip_configurations_master,

	.frame_desc_entries = ARRAY_SIZE(imx477_frame_desc),
	.frame_desc_type = CRL_V4L2_MBUS_FRAME_DESC_TYPE_CSI2,
	.frame_desc = imx477_frame_desc,
};

#endif  /* __CRLMODULE_IMX477_MASTER_CONFIGURATION_H_ */
