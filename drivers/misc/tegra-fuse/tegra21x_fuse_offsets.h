/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION. All rights reserved.
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
#include "fuse.h"

#ifndef __TEGRA21x_FUSE_OFFSETS_H
#define __TEGRA21x_FUSE_OFFSETS_H

/* private_key4 */
#define DEVKEY_START_OFFSET			0x2A
#define DEVKEY_START_BIT			12

/* arm_debug_dis */
#define JTAG_START_OFFSET		0x0
#define JTAG_START_BIT			3

/* security_mode */
#define ODM_PROD_START_OFFSET		0x0
#define ODM_PROD_START_BIT		11

/* boot_device_info */
#define SB_DEVCFG_START_OFFSET		0x2C
#define SB_DEVCFG_START_BIT		20

/* reserved_sw[2:0] */
#define SB_DEVSEL_START_OFFSET		0x2C
#define SB_DEVSEL_START_BIT		28

/* private_key0 -> private_key3 (SBK) */
#define SBK_START_OFFSET	0x22
#define SBK_START_BIT		20

/* reserved_sw[7:4] */
#define SW_RESERVED_START_OFFSET	0x2E
#define SW_RESERVED_START_BIT		4
#define SW_RESERVED_SIZE_BITS		12

/* reserved_sw[3] */
#define IGNORE_DEVSEL_START_OFFSET	0x2E
#define IGNORE_DEVSEL_START_BIT	7

/* public key */
#define PUBLIC_KEY_START_OFFSET	0xC
#define PUBLIC_KEY_START_BIT		6

/* pkc_disable */
#define PKC_DISABLE_START_OFFSET	0x52
#define PKC_DISABLE_START_BIT		7

/* odm lock */
#define ODM_LOCK_START_OFFSET		0x0
#define ODM_LOCK_START_BIT		6

/* AID */
#ifdef CONFIG_AID_FUSE
#define AID_START_OFFSET			0x67
#define AID_START_BIT				2
#endif

/* reserved_odm0 -> reserved_odm7 */
#define ODM_RESERVED_DEVSEL_START_OFFSET	0x2E
#define ODM_RESERVED_START_BIT			17

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
#define FUSE_OPT_SUBREVISION		0x248
#define FUSE_OPT_SUBREVISION_MASK	0xF
#define FUSE_RESERVED_CALIB		0x304
#define FUSE_GPU_INFO			0x390
#define FUSE_GPU_INFO_MASK		(1<<2)
#define FUSE_SPARE_BIT			0x380

#define TEGRA_FUSE_SUPPLY		"dummy"
#define PGM_TIME_US		12

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

static u32 fuse_record_buff[FUSE_MAX_PATCH_PAYLOAD];

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
static struct fuse_chip_option_override_regs fuse_override_regs[FUSE_TOT_WORDS];

static u32 calc_parity(u32 *data, u32 size)
{
	u32  parity_word, i;

	parity_word = data[0];
	for (i = 1; i < size; i++)
		parity_word ^= data[i];

	parity_word = (parity_word & 0x0000FFFF) ^ (parity_word >> 16);
	parity_word = (parity_word & 0x000000FF) ^ (parity_word >>  8);
	parity_word = (parity_word & 0x0000000F) ^ (parity_word >>  4);
	parity_word = (parity_word & 0x00000003) ^ (parity_word >>  2);
	parity_word = (parity_word & 0x00000001) ^ (parity_word >>  1);
	return parity_word;
}

static u32 hamming16_syndrome(u32 *data, u32 size)
{
	u32 i, j, syndrome;

	/* Calculate the syndrome */
	syndrome = 0;

	for (i = 0; i < size; i++) {
		if (data[i] != 0) {
			/* for speed, assuming that many words could be zero */
			for (j = 0; j < UINT_BITS; j++) {
				if ((data[i] >> j) & 0x1)
					syndrome ^= (H16_START_OFFSET +
							(i * UINT_BITS) + j);
			}
		}
	}
	return syndrome;
}

static u32 hamming16_decode(u32 *data, u32 size)
{
	u32 calculated_parity, stored_syndrome, stored_parity;
	u32 syndrome, offset, i, j, offset_bits_set;

	calculated_parity = calc_parity(data, size);
	stored_syndrome = data[0] & H16_H_MASK;
	stored_parity = data[0] & H16_PARITY_MASK;
	data[0] &= ~H16_ECC_MASK;
	syndrome = hamming16_syndrome(data, size) ^ stored_syndrome;
	data[0] ^= stored_parity;
	data[0] ^= stored_syndrome;
	if (syndrome != 0) {
		if (!calculated_parity)
			return H_UNCORRECTED_ERROR_EVEN;
		/* error is at bit offset syndrome - H16_START_OFFSET,
		 * can be corrected if in range [H16_BITS, N * UINT_BITS]
		 */
		offset = syndrome - H16_START_OFFSET;
		i = (offset >> LOG2_UINT_BITS);
		if ((offset < H16_BITS) || (offset >= UINT_BITS * size)) {
			/* special case of the Hamming bits themselves,
			 * detected by syndrome (or equivalently offset)
			 * being a power of two
			 */
			offset_bits_set = 0;
			for (j = 0; j < H16_BITS-1; j++) {
				if ((syndrome >> j) & 1)
					offset_bits_set++;
			}
			if (offset_bits_set == 1) {
				data[0] ^= syndrome;
				return H_CORRECTED_ERROR;
			} else {
				return H_UNCORRECTED_ERROR_ODD;
			}
		}
		data[i] ^= (1 << (offset & INSIDE_UINT_OFFSET_MASK));
		return H_CORRECTED_ERROR;
	}
	if (calculated_parity) {
		data[0] ^= H16_PARITY_MASK;
		return H_CORRECTED_ERROR;
	}
	return H_NO_ERROR;
}

static const u32 h5syndrome_table[] = {
	0x1, 0x2, 0x4, 0x8,
	0x0, 0x3, 0x5, 0x6,
	0x7, 0x9, 0xA, 0xB };

static u32 hamming5_syndrome(u32 *data)
{
	u32 i, syndrome;

	/* data is assumed to be passed as present in Fuse,
	 * i.e. the ECC word starts at bit H5_BIT_OFFSET
	 */
	syndrome = 0;
	for (i = 0; i < H5_CODEWORD_SIZE; i++) {
		if (data[0] & (1 << (H5_BIT_OFFSET + i)))
			syndrome ^= h5syndrome_table[i];
	}

	/* Return the syndrome aligned to the right offset */
	return syndrome << H5_BIT_OFFSET;
}

static u32 hamming5_decode(u32 *data)
{
	u32 calculated_parity, syndrome, i, storednonh5;

	/* Required for correct parity */
	storednonh5 = data[0] & ~H5_CODEWORD_MASK;
	data[0] &= H5_CODEWORD_MASK;

	/* parity should be even (0) */
	calculated_parity = calc_parity(data, 1);
	syndrome = hamming5_syndrome(data) >> H5_BIT_OFFSET;
	data[0] |= storednonh5;

	if (syndrome != 0) {
		if (!calculated_parity)
			return H_UNCORRECTED_ERROR_EVEN;

		/* decode using the syndrome table, if we fall
		 * through the position associated with the
		 * syndrome does not exist
		 */
		for (i = 0; i < H5_CODEWORD_SIZE; i++) {
			if (syndrome == h5syndrome_table[i]) {
				data[0] ^= (1 << (i + H5_BIT_OFFSET));
				return H_CORRECTED_ERROR;
			}
		}
		return H_UNCORRECTED_ERROR_ODD;
	}
	if (calculated_parity) {
		data[0] ^= H5_PARITY_MASK;
		return H_CORRECTED_ERROR;
	}
	return H_NO_ERROR;
}

static void fuse_override_msb16(u32 reg_index, u32 cam_data)
{
	u32 override_data, temp_data;

	override_data = (cam_data << FUSE_OVERRIDE_MSB16_SHIFT);
	if (fuse_override_regs[reg_index].override_flag ==
			FUSE_OVERRIDE_LSB16) {
		temp_data = fuse_override_regs[reg_index].override_data;
		temp_data = (temp_data & ~FUSE_OVERRIDE_MSB16_MASK);
		override_data |= temp_data;
	}
	fuse_override_regs[reg_index].override_data = override_data;
	fuse_override_regs[reg_index].override_flag |= FUSE_OVERRIDE_MSB16;
}

static void fuse_override_lsb16(u32 reg_index, u32 cam_data)
{
	u32 override_data, temp_data;

	override_data = cam_data;
	if (fuse_override_regs[reg_index].override_flag ==
			FUSE_OVERRIDE_MSB16) {
		temp_data = fuse_override_regs[reg_index].override_data;
		temp_data = temp_data & ~FUSE_OVERRIDE_LSB16_MASK;
		override_data |= temp_data;
	}
	fuse_override_regs[reg_index].override_data = override_data;
	fuse_override_regs[reg_index].override_flag |= FUSE_OVERRIDE_LSB16;
}

static int fuse_override_option_regs(u32 *patch_buff, u32 num_entries)
{
	u32 cam_addr, cam_data;
	int count = 0;
	u32 override_index, reg_index, msb_indicator;

	for (count = 0; count < num_entries; count++) {
		cam_addr = ((patch_buff[count] &
				FUSE_CAM_ADDR_MASK) >> FUSE_CAM_ADDR_SHIFT);
		if ((cam_addr >= FUSE_OVERRIDE_REG_END_ADDR) &&
			(cam_addr <= FUSE_OVERRIDE_REG_START_ADDR)) {
			cam_data = patch_buff[count] & FUSE_CAM_DATA_MASK;
			override_index = (FUSE_OVERRIDE_REG_START_ADDR -
					cam_addr);
			reg_index = override_index >> 1;
			msb_indicator = override_index & 1;
			if (msb_indicator)
				fuse_override_msb16(reg_index, cam_data);
			else
				fuse_override_lsb16(reg_index, cam_data);
		}
	}

	return 0;
}

static int tegra_fuse_override_chip_option_regs(void)
{
	u32 patch_record_size, patch_start_addr;
	u32 patch_header, num_cam_entries;
	int count, ret;

	patch_record_size = tegra_fuse_readl(
			FUSE_FIRST_BOOTROM_PATCH_SIZE_REG);
	if (patch_record_size > FUSE_MAX_PATCH_PAYLOAD) {
		pr_err("%s - patch_record_size is more than allocated\n",
				__func__);
		return -1;
	}

	patch_start_addr = FUSE_PATCH_START_ADDRESS;
	while (patch_record_size) {
		/* Read the complete patch record */
		for (count = 0; count < patch_record_size; count++) {
			fuse_record_buff[count] =
				fuse_cmd_read(patch_start_addr);
			patch_start_addr--;
		}

		patch_header = fuse_record_buff[0];
		/* Patch  header - 32 bits
		 * N[7] [31-25]  --> 7 bit Next patch record size
		 * H5[5] [24-20] --> 5 bit hamming code with parity,
		 *		protecting N field
		 * C[4] [19-16]  --> 4 bit number of CAM entries
		 * H16[16] [15-0] --> 16 bit hamming code with parity,
		 *		protecting this record except fields N and H5
		 */

		ret = hamming16_decode(fuse_record_buff, patch_record_size);
		if (ret < H_UNCORRECTED_ERROR_ODD) {
			num_cam_entries = ((patch_header & FUSE_PATCH_C_MASK)
					>> FUSE_PATCH_C_SHIFT);
			if (num_cam_entries)
				fuse_override_option_regs(&fuse_record_buff[1],
						num_cam_entries);
			/* Check if we have any other records */
			if (patch_header >> FUSE_NEXT_PATCH_SHIFT) {
				ret = hamming5_decode(&patch_header);
				if (ret >= H_UNCORRECTED_ERROR_ODD) {
					pr_err("Unrecoverable patch record\n");
					return ret;
				} else
					patch_record_size =
						(patch_header >>
						 FUSE_NEXT_PATCH_SHIFT);
			} else {
				patch_record_size = 0;
			}
		} else {
			pr_err("Unrecoverable patch record\n");
			return ret;
		}
	}

	return 0;
}

static void fuse_update_overridden_reg_val(unsigned long offset,
		u32 *val)
{
	u32 index;

	if ((offset < FUSE_CHIP_REG_START_OFFSET) ||
			(offset > FUSE_CHIP_REG_END_OFFSET))
		return;

	index = (offset - FUSE_CHIP_REG_START_OFFSET) >> 2;

	switch (fuse_override_regs[index].override_flag) {
	case FUSE_OVERRIDE_LSB16:
		*val = *val & ~FUSE_OVERRIDE_LSB16_MASK;
		*val = *val | fuse_override_regs[index].override_data;
		break;
	case FUSE_OVERRIDE_MSB16:
		*val = *val & ~FUSE_OVERRIDE_MSB16_MASK;
		*val = *val | fuse_override_regs[index].override_data;
		break;
	case FUSE_OVERRIDE_WORD:
		*val = fuse_override_regs[index].override_data;
		break;
	default:
		break;
	}
}

static inline int fuse_get_gpcpll_adc_rev(u32 val)
{
	return (val >> 30) & 0x3;
}

static inline int fuse_get_gpcpll_adc_slope_uv(u32 val)
{
	/*      Integer part in mV  * 1000 + fractional part in uV */
	return ((val >> 24) & 0x3f) * 1000 + ((val >> 14) & 0x3ff);
}

static inline int fuse_get_gpcpll_adc_intercept_uv(u32 val)
{
	/*      Integer part in mV  * 1000 + fractional part in 100uV */
	return ((val >> 4) & 0x3ff) * 1000 + ((val >> 0) & 0xf) * 100;
}

static DEVICE_ATTR(public_key, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(pkc_disable, 0440, tegra_fuse_show, tegra_fuse_store);
static DEVICE_ATTR(odm_lock, 0440, tegra_fuse_show, tegra_fuse_store);

static int tegra_fuse_add_sysfs_variables(struct platform_device *pdev,
					bool odm_security_mode)
{
	dev_attr_odm_lock.attr.mode = 0640;
	if (odm_security_mode) {
		dev_attr_public_key.attr.mode =  0440;
		dev_attr_pkc_disable.attr.mode = 0440;
	} else {
		dev_attr_public_key.attr.mode =  0640;
		dev_attr_pkc_disable.attr.mode = 0640;
	}
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_public_key.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_pkc_disable.attr));
	CHK_ERR(&pdev->dev, sysfs_create_file(&pdev->dev.kobj,
				&dev_attr_odm_lock.attr));

	return 0;
}

static int tegra_fuse_rm_sysfs_variables(struct platform_device *pdev)
{
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_public_key.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_pkc_disable.attr);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_odm_lock.attr);

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

/* Add dummy functions */
static inline void tegra_pmc_fuse_disable_mirroring(void)
{}

static inline void tegra_pmc_fuse_enable_mirroring(void)
{}
#endif /* __TEGRA21x_FUSE_OFFSETS_H */
