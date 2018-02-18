/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful, but without
 * any warranty; without even the implied warranty of merchantability or
 * fitness for a particular purpose.  see the gnu general public license for
 * more details.
 *
 * you should have received a copy of the gnu general public license along
 * with this program; if not, write to the free software foundation, inc.,
 * 51 franklin street, fifth floor, boston, ma  02110-1301, usa.
 */

#include <linux/tegra-soc.h>
#include <linux/tegra-pmc.h>

#ifndef __TEGRA18x_FUSE_OFFSETS_H
#define __TEGRA18x_FUSE_OFFSETS_H

/* private_key4 */
#define DEVKEY_START_OFFSET		0x2A
#define DEVKEY_START_BIT		12

/* arm_debug_dis */
#define JTAG_START_OFFSET		0x0
#define JTAG_START_BIT			12

/* security_mode */
#define ODM_PROD_START_OFFSET		0x0
#define ODM_PROD_START_BIT		11

/* boot_device_info */
#define SB_DEVCFG_START_OFFSET		0x4f
#define SB_DEVCFG_START_BIT		23
#define SB_BOOT_DEV_CFG_SIZE_BITS	24

/* reserved_sw[2:0] */
#define SB_DEVSEL_START_OFFSET		0x50
#define SB_DEVSEL_START_BIT		15

/* private_key0 -> private_key3 (SBK) */
#define SBK_START_OFFSET		0x4b
#define SBK_START_BIT			23

/* reserved_sw[7:4] */
#define SW_RESERVED_START_OFFSET	0x50
#define SW_RESERVED_START_BIT		15
#define SW_RESERVED_SIZE_BITS		12

/* reserved_sw[3] */
#define IGNORE_DEVSEL_START_OFFSET	0x50
#define IGNORE_DEVSEL_START_BIT		18

/* public key */
#define PUBLIC_KEY_START_OFFSET		0x43
#define PUBLIC_KEY_START_BIT		23

/* pkc_disable */
#define PKC_DISABLE_START_OFFSET	0x52
#define PKC_DISABLE_START_BIT		7

/* video vp8 enable */
#define VP8_ENABLE_START_OFFSET		0x2E
#define VP8_ENABLE_START_BIT		4

/* odm lock */
#define ODM_LOCK_START_OFFSET		0x0
#define ODM_LOCK_START_BIT		6

/* reserved_odm0 -> reserved_odm7 */
#define ODM_RESERVED_DEVSEL_START_OFFSET	0x2
#define ODM_RESERVED_START_BIT			2

#define BOOT_SECURITY_INFO_START_OFFSET		0x0
#define BOOT_SECURITY_INFO_START_BIT		16

/* KEK0 key encryption key0 */
#define KEY_ENCRYPTION_KEY0_START_OFFSET	0x59
#define KEY_ENCRYPTION_KEY0_START_BIT		22

/* KEK1 key encryption key1 */
#define KEY_ENCRYPTION_KEY1_START_OFFSET	0x5d
#define KEY_ENCRYPTION_KEY1_START_BIT		22

/* KEK2 key encryption key2 */
#define KEY_ENCRYPTION_KEY2_START_OFFSET	0x61
#define KEY_ENCRYPTION_KEY2_START_BIT		22

/* odm_info */
#define FUSE_ODM_INFO_START_OFFSET		0x50
#define FUSE_ODM_INFO_START_BIT			31

/* H2 ODM hamming code */
#define FUSE_ODM_H2_START_OFFSET		0x67
#define FUSE_ODM_H2_START_BIT			31

#define FUSE_VENDOR_CODE		0x200
#define FUSE_VENDOR_CODE_MASK		0xf
#define FUSE_FAB_CODE			0x204
#define FUSE_FAB_CODE_MASK		0x3f
#define FUSE_LOT_CODE_0		0x208
#define FUSE_LOT_CODE_1		0x20c
#define FUSE_WAFER_ID			0x210
#define FUSE_WAFER_ID_MASK		0x3f
#define FUSE_X_COORDINATE		0x214
#define FUSE_X_COORDINATE_MASK		0x1ff
#define FUSE_Y_COORDINATE		0x218
#define FUSE_Y_COORDINATE_MASK		0x1ff
#define FUSE_GPU_INFO			0x390
#define FUSE_GPU_INFO_MASK		(1<<2)
#define FUSE_SPARE_BIT			0x380

/* fuse registers used in public fuse data read API */
#define FUSE_FT_REV			0x128
#define FUSE_CP_REV			0x190
/* fuse spare bits are used to get Tj-ADT values */
#define NUM_TSENSOR_SPARE_BITS		28
/* tsensor calibration register */
#define FUSE_TSENSOR_CALIB_0		0x198
/* sparse realignment register */
#define FUSE_SPARE_REALIGNMENT_REG_0	0x37c
/* tsensor8_calib */
#define FUSE_TSENSOR_CALIB_8		0x280

#define FUSE_BASE_CP_SHIFT		0
#define FUSE_BASE_CP_MASK		0x3ff
#define FUSE_BASE_FT_SHIFT		10
#define FUSE_BASE_FT_MASK		0x7ff
#define FUSE_SHIFT_CP_SHIFT		0
#define FUSE_SHIFT_CP_MASK		0x3f
#define FUSE_SHIFT_CP_BITS		6
#define FUSE_SHIFT_FT_SHIFT		21
#define FUSE_SHIFT_FT_MASK		0x1f
#define FUSE_SHIFT_FT_BITS		5

#define TEGRA_FUSE_SUPPLY		"dummy"
#define PGM_TIME_US			5

#define FUSE_SIZE_IN_BITS			(6 *  1024)
#define FUSE_PATCH_START_ADDRESS	((FUSE_SIZE_IN_BITS - 32)/32)
#define FUSE_MAX_PATCH_PAYLOAD		(2560 >> 5)
#define FUSE_FIRST_BOOTROM_PATCH_SIZE_REG	0x19c

#define FUSE_PATCH_C_MASK		0x000F0000
#define FUSE_PATCH_C_SHIFT		16
#define FUSE_NEXT_PATCH_SHIFT	25

#define FUSE_CAM_ADDR_MASK		0xFFFF0000
#define FUSE_CAM_ADDR_SHIFT		16
#define FUSE_CAM_DATA_MASK		0x0000FFFF

#define FUSE_CHIP_REG_START_OFFSET		0x100
#define FUSE_CHIP_REG_END_OFFSET		0x3FC
#define FUSE_OVERRIDE_REG_START_ADDR	0xFFFF
#define FUSE_OVERRIDE_REG_END_ADDR		0xFE80

#define FUSE_OVERRIDE_MSB16_MASK	0xFFFF0000
#define FUSE_OVERRIDE_LSB16_MASK	0x0000FFFF
#define FUSE_OVERRIDE_MSB16_SHIFT	16
#define FUSE_TOT_WORDS				192

#define FUSE_OPT_SUBREVISION		0x248
#define FUSE_OPT_SUBREVISION_MASK	0xF

#define H2_START_MACRO_BIT_INDEX	2167
#define H2_END_MACRO_BIT_INDEX		3326

extern void tegra_pmc_fuse_disable_mirroring(void);
extern void tegra_pmc_fuse_enable_mirroring(void);
extern u32 fuse_cmd_read(u32 addr);

static const u32 UINT_BITS = 32;
static const u32 LOG2_UINT_BITS = 5;
static const u32 INSIDE_UINT_OFFSET_MASK = 0x1F;

static const u32 H16_BITS = 16;
static const u32 H16_START_OFFSET = 1 << (16 - 2);
static const u32 H16_ECC_MASK      = 0xFFF0FFFFU;
static const u32 H16_PARITY_MASK   = 0x00008000U;
static const u32 H16_H_MASK        = 0x00007FFFU;

static const u32 H5_CODEWORD_SIZE = 12;
static const u32 H5_BIT_OFFSET = 20;
/* the H5 code word is mapped on MSB of a word */
static const u32 H5_CODEWORD_MASK = 0xFFF00000U;
static const u32 H5_PARITY_MASK   = 0x01000000U;

static const u32 H_NO_ERROR;
static const u32 H_CORRECTED_ERROR = 1;
static const u32 H_UNCORRECTED_ERROR_ODD = 2;
static const u32 H_UNCORRECTED_ERROR_EVEN = 3;

enum override_indicator {
	FUSE_OVERRIDE_NONE = 0,
	FUSE_OVERRIDE_LSB16,
	FUSE_OVERRIDE_MSB16,
	FUSE_OVERRIDE_WORD,
};

struct fuse_chip_option_override_regs {
	enum override_indicator override_flag;
	u32 override_data;
};

static const u32 h5syndrome_table[] = {
	0x1, 0x2, 0x4, 0x8,
	0x0, 0x3, 0x5, 0x6,
	0x7, 0x9, 0xA, 0xB };

static u32 tegra_fuse_calculate_parity(u32 val)
{
	u32 i, p = 0;

	for (i = 0; i < 32; i++)
		p ^= ((val >> i) & 1);

	return p;
}

static ssize_t tegra_fuse_dump_h2(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	u32 startrowindex, startbitindex;
	u32 endrowindex, endbitindex;
	u32 bitindex;
	u32 rowindex;
	u32 rowdata;
	u32 hammingvalue = 0;
	u32 pattern = 0x7ff;
	u32 parity;
	u32 hammingcode;

	startrowindex = H2_START_MACRO_BIT_INDEX / 32;
	startbitindex = H2_START_MACRO_BIT_INDEX % 32;
	endrowindex = H2_END_MACRO_BIT_INDEX / 32;
	endbitindex = H2_END_MACRO_BIT_INDEX % 32;

	for (rowindex = startrowindex; rowindex <= endrowindex; rowindex++) {
		rowdata = fuse_cmd_read(rowindex);
		for (bitindex = 0; bitindex < 32; bitindex++) {
			pattern++;
			if ((rowindex == startrowindex) &&
			    (bitindex < startbitindex))
				continue;
			if ((rowindex == endrowindex) &&
			    (bitindex > endbitindex))
				continue;
			if ((rowdata >> bitindex) & 0x1)
				hammingvalue ^= pattern;
		}
	}
	parity = tegra_fuse_calculate_parity(hammingvalue);
	hammingcode = hammingvalue | (1 << 12) | ((parity ^ 1) << 13);
	sprintf(buf, "0x%08x\n", hammingcode);

	return strlen(buf);
}

static DEVICE_ATTR(public_key, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(pkc_disable, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(odm_lock, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(boot_sec_info, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(kek0, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(kek1, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(kek2, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(odm_info, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(odm_h2, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(dump_h2, 0440, tegra_fuse_dump_h2, NULL);

static int tegra_fuse_add_sysfs_variables(struct platform_device *pdev,
					bool odm_security_mode)
{
	dev_attr_odm_lock.attr.mode = 0640;
	if (odm_security_mode) {
		dev_attr_public_key.attr.mode =  0440;
		dev_attr_pkc_disable.attr.mode = 0440;
		dev_attr_boot_sec_info.attr.mode = 0440;
		dev_attr_kek0.attr.mode = 0440;
		dev_attr_kek1.attr.mode = 0440;
		dev_attr_kek2.attr.mode = 0440;
		dev_attr_odm_info.attr.mode = 0440;
		dev_attr_odm_h2.attr.mode = 0440;
		dev_attr_dump_h2.attr.mode = 0440;
	} else {
		dev_attr_public_key.attr.mode =  0640;
		dev_attr_pkc_disable.attr.mode = 0640;
		dev_attr_boot_sec_info.attr.mode = 0640;
		dev_attr_kek0.attr.mode = 0640;
		dev_attr_kek1.attr.mode = 0640;
		dev_attr_kek2.attr.mode = 0640;
		dev_attr_odm_info.attr.mode = 0640;
		dev_attr_odm_h2.attr.mode = 0640;
		dev_attr_dump_h2.attr.mode = 0640;
	}
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_public_key.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_pkc_disable.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_odm_lock.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_boot_sec_info.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_kek0.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_kek1.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_kek2.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_odm_info.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_odm_h2.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_dump_h2.attr));
	return 0;
}

static int tegra_fuse_rm_sysfs_variables(struct platform_device *pdev)
{
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_public_key.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_pkc_disable.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_odm_lock.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_boot_sec_info.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_kek0.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_kek1.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_kek2.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_odm_info.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_odm_h2.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_dump_h2.attr);

	return 0;
}

static int tegra_fuse_ch_sysfs_perm(struct device *dev, struct kobject *kobj)
{
	CHK_ERR(dev, sysfs_chmod_file(kobj,
				&dev_attr_public_key.attr, 0440));
	CHK_ERR(dev, sysfs_chmod_file(kobj,
				&dev_attr_pkc_disable.attr, 0440));

	return 0;
}
#endif /* __TEGRA18x_FUSE_OFFSETS_H */
