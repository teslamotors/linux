/* SPDX-LIcense_Identifier: GPL-2.0 */
/* Copyright (C) 2018 Intel Corporation */

#ifndef MAX9286_REG_H
#define MAX9286_REG_H

#include <media/max9286.h>

#define DS_ADDR_MAX9286           0x48
#define S_ADDR_MAX96705           0x40
#define S_ADDR_MAX96705_BROADCAST (S_ADDR_MAX96705 + NR_OF_MAX_STREAMS + 1)

#define ADDR_AR0231AT_SENSOR	  0x10

/* Deserializer: MAX9286 registers */
#define DS_LINK_ENABLE            0x00
#define DS_FSYNCMODE              0x01
#define DS_FSYNC_PERIOD_LOW       0x06
#define DS_FSYNC_PERIOD_MIDDLE    0x07
#define DS_FSYNC_PERIOD_HIGH      0x08
#define DS_FWDCCEN_REVCCEN        0x0A
#define DS_LINK_OUTORD            0x0B
#define DS_HS_VS                  0x0C
#define DS_CSI_DBL_DT             0x12
#define DS_CSI_VC_CTL             0x15
#define DS_ENEQ                   0x1B
#define DS_HIGHIMM                0x1C
#define DS_MAX9286_DEVID          0x1E
#define DS_FSYNC_LOCKED           0x31
#define DS_I2CLOCACK              0x34
#define DS_FPL_RT                 0x3B
#define DS_ENCRC_FPL              0x3F
#define DS_CONFIGL_VIDEOL_DET     0x49
#define DS_OVERLAP_WIN_LOW        0x63
#define DS_OVERLAP_WIN_HIGH       0x64
#define DS_ATUO_MASK_LINK         0x69

/* Serializer: MAX96705 registers */
#define S_SERADDR                 0x00
#define S_MAIN_CTL                0x04
#define S_CMLLVL_PREEMP           0x06
#define S_CONFIG                  0x07
#define S_RSVD_8                  0x08
#define S_I2C_SOURCE_IS           0x09
#define S_I2C_DST_IS              0x0A
#define S_I2C_SOURCE_SER          0x0B
#define S_I2C_DST_SER             0x0C
#define S_INPUT_STATUS            0x15
#define S_SYNC_GEN_CONFIG         0x43
#define S_VS_DLY_2                0x44
#define S_VS_DLY_1                0x45
#define S_VS_H_2                  0x47
#define S_VS_H_1                  0x48
#define S_VS_H_0                  0x49
#define S_DBL_ALIGN_TO            0x67
#define S_RSVD_97                 0x97

struct max9286_register_write {
	u8 reg;
	u8 val;
};

static const struct max9286_register_write max9286_byte_order_settings_12bit[] = {
	{0x20, 0x0B},
	{0x21, 0x0A},
	{0x22, 0x09},
	{0x23, 0x08},
	{0x24, 0x07},
	{0x25, 0x06},
	{0x26, 0x05},
	{0x27, 0x04},
	{0x28, 0x03},
	{0x29, 0x02},
	{0x2A, 0x01},
	{0x2B, 0x00},
	{0x30, 0x1B},
	{0x31, 0x1A},
	{0x32, 0x19},
	{0x33, 0x18},
	{0x34, 0x17},
	{0x35, 0x16},
	{0x36, 0x15},
	{0x37, 0x14},
	{0x38, 0x13},
	{0x39, 0x12},
	{0x3A, 0x11},
	{0x3B, 0x10},
};

static const struct max9286_register_write max9286_byte_order_settings_10bit[] = {
	{0x20, 0x09},
	{0x21, 0x08},
	{0x22, 0x07},
	{0x23, 0x06},
	{0x24, 0x05},
	{0x25, 0x04},
	{0x26, 0x03},
	{0x27, 0x02},
	{0x28, 0x01},
	{0x29, 0x00},
	{0x30, 0x19},
	{0x31, 0x18},
	{0x32, 0x17},
	{0x33, 0x16},
	{0x34, 0x15},
	{0x35, 0x14},
	{0x36, 0x13},
	{0x37, 0x12},
	{0x38, 0x11},
	{0x39, 0x10},
};

#endif
