// SPDX-License-Identifier: GPL-2.0-only
/*
 * MTD driver for Tesla InfoZ platform
 *
 * Copyright (C) 2021 Tesla, Inc.
 */

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/map.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <asm/io.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dmi.h>

#define INFOZ_FLASH_SPI_BUS			0
#define INFOZ_FLASH_WIN_ADDR			0xff000000
#define INFOZ_FLASH_WIN_SIZE			(16 << 20)
#define INFOZ_FLASH_ACTUAL_SIZE			(32 << 20)
#define INFOZ_FLASH_COUNT			2

struct infoz_flash_info {
	struct spi_controller	*spi;
	struct spi_device	*spi_dev[INFOZ_FLASH_COUNT];
	struct mtd_info		*mtd;
	struct platform_device	*pdev;
	struct map_info		map;
	bool			present;
};

static struct infoz_flash_info flash_info = { 0 };

#define KB (1024)
#define MB (1024*KB)
static struct mtd_partition infoz_flash_parts0[] = {
	{ .name = "VBLOCK_A",			.size = 64*KB,		.offset = 0, },
	{ .name = "COREBOOT",			.size = 2*MB,		.offset = MTDPART_OFS_NXTBLK, },
	/* Includes RO_FRID/RW_FWID in last 160 bytes */
	{ .name = "GBB",			.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "FW_MAIN_A",			.size = 5056*KB,	.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "RW_MRC_CACHE",		.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "PSP_NVRAM",			.size = 128*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "PSP_RPMC_NVRAM",		.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "PSP_RPMC_NVRAM_BACKUP",	.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "PSP_NVRAM_BACKUP",		.size = 128*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "PSP_NVRAM_DEBUG",		.size = 192*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "PSP_NVRAM_BACKUP_DEBUG",	.size = 192*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "RO_VPD",			.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK,		.mask_flags = MTD_WRITEABLE,},
	/* Includes FMAP in last 2 KB */
	{ .name = "RSVD0",			.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "KERNEL_0",			.size = 8*MB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "KERNEL_1",			.size = 16*MB,		.offset = MTDPART_OFS_NXTBLK, },
};

static struct mtd_partition infoz_flash_parts1[] = {
	{ .name = "ALT_VBLOCK_A",		.size = 64*KB,		.offset = 0, },
	{ .name = "ALT_COREBOOT",		.size = 2*MB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_GBB",			.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_FW_MAIN_A",		.size = 5056*KB,	.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_RW_MRC_CACHE",		.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_PSP_NVRAM",		.size = 128*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_PSP_RPMC_NVRAM",		.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_PSP_RPMC_NVRAM_BACKUP",	.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_PSP_NVRAM_BACKUP",	.size = 128*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_PSP_NVRAM_DEBUG",	.size = 192*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_PSP_NVRAM_BACKUP_DEBUG",	.size = 192*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_RO_VPD",			.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK,		.mask_flags = MTD_WRITEABLE,},
	/* Includes FMAP in last 2 KB */
	{ .name = "ALT_RSVD0",			.size = 64*KB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_KERNEL_0",		.size = 8*MB,		.offset = MTDPART_OFS_NXTBLK, },
	{ .name = "ALT_KERNEL_1",		.size = 16*MB,		.offset = MTDPART_OFS_NXTBLK, },
	/* Overlapping update regions */
	{ .name = "UPDATE_0",			.size = 7296*KB,	.offset = 0, },
	/* Do not include ALT_RO_VPD in UPDATE_HOLE */
	{ .name = "UPDATE_HOLE",		.size = 768*KB,		.offset = 0x720000, },
	{ .name = "UPDATE_1",			.size = 24640*KB,	.offset = 0x7f0000, },
};

/* Current boot flash */
static struct flash_platform_data infoz_flash_spi0_pdata = {
	.name =		"spiflash0",
	.nr_parts =	ARRAY_SIZE(infoz_flash_parts0),
	.parts =	infoz_flash_parts0,
};

/* Alternate/offline flash */
static struct flash_platform_data infoz_flash_spi1_pdata = {
	.name =		"spiflash1",
	.nr_parts =	ARRAY_SIZE(infoz_flash_parts1),
	.parts =	infoz_flash_parts1,
};

static struct spi_board_info infoz_flash_board_info[] = {
	{
		.modalias =		"spi-nor",
		.bus_num =		0,
		.chip_select =		0,
		.mode =			SPI_MODE_0,
		.platform_data =	&infoz_flash_spi0_pdata,
	},
	{
		.modalias =		"spi-nor",
		.bus_num =		0,
		.chip_select =		1,
		.mode =			SPI_MODE_0,
		.platform_data =	&infoz_flash_spi1_pdata,
	},
};

static int infoz_flash_probe(struct platform_device *pdev)
{
	int i;
	struct spi_device *spi_dev;

	if (flash_info.present)
		return -ENODEV;

	if (!dmi_match(DMI_PRODUCT_FAMILY, "Tesla_InfoZ"))
		return -ENODEV;

	flash_info.spi = spi_busnum_to_master(INFOZ_FLASH_SPI_BUS);
	if (IS_ERR_OR_NULL(flash_info.spi)) {
		/* Maybe the spi-amd driver hasn't loaded yet */
		return -EPROBE_DEFER;
	}

	platform_set_drvdata(pdev, &flash_info);

	/* Register SPI NOR flash devices */
	BUILD_BUG_ON(ARRAY_SIZE(infoz_flash_board_info) != INFOZ_FLASH_COUNT);
	for (i = 0; i < ARRAY_SIZE(infoz_flash_board_info); i++) {
		spi_dev = spi_new_device(flash_info.spi, &(infoz_flash_board_info[i]));
		if (spi_dev == NULL) {
			dev_err(&pdev->dev, "%s: Failed to add SPI device %d\n", __func__, i);
			return -ENODEV;
		}
		flash_info.spi_dev[i] = spi_dev;
	}

	flash_info.present = true;
	dev_info(&pdev->dev, "infoz-flash: Added %d chips\n", INFOZ_FLASH_COUNT);

	return 0;
}

static int infoz_flash_remove(struct platform_device *pdev)
{
	int i;
	struct infoz_flash_info *info;

	info = platform_get_drvdata(pdev);
	if (!info)
		return 0;

	for (i = 0; i < INFOZ_FLASH_COUNT; i++) {
		if (info->spi_dev[i] == NULL)
			break;
		spi_unregister_device(info->spi_dev[i]);
	}

	return 0;
}

static struct platform_driver infoz_flash_driver = {
	.driver		= {
		.name	= "infoz-flash",
	},
	.probe		= infoz_flash_probe,
	.remove		= infoz_flash_remove,
};

static struct resource infoz_flash_resource = {
	.start		= INFOZ_FLASH_WIN_ADDR,
	.end		= INFOZ_FLASH_WIN_ADDR + INFOZ_FLASH_WIN_SIZE - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device infoz_flash_dev = {
	.name		= "infoz-flash",
	.id		= 0,
	.num_resources	= 1,
	.resource	= &infoz_flash_resource,
};

static int __init infoz_flash_init(void)
{
	int err;

	err = platform_driver_register(&infoz_flash_driver);
	if (err == 0) {
		err = platform_device_register(&infoz_flash_dev);
		if (err)
			platform_driver_unregister(&infoz_flash_driver);
	}

	return err;
}

static void __exit infoz_flash_exit(void)
{
	platform_device_unregister(&infoz_flash_dev);
	platform_driver_unregister(&infoz_flash_driver);
}

module_init(infoz_flash_init);
module_exit(infoz_flash_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tesla OpenSource <opensource@tesla.com>");
MODULE_DESCRIPTION("MTD map driver for Tesla InfoZ platform");
