/*
 * drivers/mtd/maps/tegra_nor.c
 *
 * MTD mapping driver for the internal SNOR controller in Tegra SoCs
 *
 * Copyright (c) 2009, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define NV_DEBUG 0

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach/flash.h>
#include <linux/clk.h>

#include <mach/nor.h>

#include "tegra_nor.h"

/* This macro defines whether 2 copy approach (from device to DMA buffers,
 * followed by a memcpy to MTD buffer) OR dma_map MTD buffers and directly
 * DMA into the same is to be used.
 */
#define SNOR_USE_DOUBLE_COPY 1

#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif

static int wait_for_dma_completion(struct tegra_nor_info *info)
{
	unsigned long dma_timeout;
	int ret;

	dma_timeout = msecs_to_jiffies(SNOR_DMA_TIMEOUT_MS);
	ret = wait_for_completion_timeout(&info->dma_complete, dma_timeout);
	return ret ? 0 : ret;
}

static void tegra_flash_dma(struct map_info *map,
			    void *to, unsigned long from, ssize_t len)
{
	uint32_t snor_config, dma_config = 0;
	int dma_transfer_count = 0, word32_count = 0;
	uint32_t nor_address, ahb_address, current_transfer = 0;
	struct tegra_nor_info *c =
	    container_of(map, struct tegra_nor_info, map);
	unsigned int bytes_remaining = len;
	struct tegra_nor_chip_parms *chip_parm = c->plat->chip_parms;

	snor_config = c->init_config;
	writel(c->timing0_read, TIMING0_REG);
	writel(c->timing1_read, TIMING1_REG);

	if (len > 32) {
#ifndef SNOR_USE_DOUBLE_COPY
		ahb_address = dma_map_single(c->dev, to, len, DMA_FROM_DEVICE);
		if (dma_mapping_error(c->dev, ahb_address)) {
			dev_err(c->dev,
				"Couldn't DMA map a %d byte buffer\n", len);
			goto out_copy;
		}
#endif
		word32_count = len >> 2;
		bytes_remaining = len & 0x00000003;
		// The parameters can be setup in any order since we write to
		// controller register only after all parameters are set.
		// SNOR CONFIGURATION SETUP
		snor_config |= CONFIG_DEVICE_MODE(1);
		/* config word page */
		snor_config |=
			CONFIG_PAGE_SZ(chip_parm->config.page_size);
		snor_config |= CONFIG_MST_ENB;
		// SNOR DMA CONFIGURATION SETUP
		dma_config &= ~DMA_CFG_DIR;	// NOR -> AHB
		dma_config |= DMA_CFG_BRST_SZ(4);	// One word burst

		for (nor_address = (unsigned int)(map->phys + from)
#ifdef SNOR_USE_DOUBLE_COPY
		     , ahb_address = (unsigned int)(to)
#endif
		     ;
		     word32_count > 0;
		     word32_count -= current_transfer,
		     dma_transfer_count += current_transfer,
		     nor_address += (current_transfer * 4),
		     ahb_address += (current_transfer * 4)) {

			current_transfer =
			    (word32_count >
			     SNOR_DMA_LIMIT_WORDS) ? (SNOR_DMA_LIMIT_WORDS) :
			    word32_count;
			// Start NOR operation
			snor_config |= CONFIG_GO;
			dma_config |= DMA_CFG_GO;
			/* Enable interrupt before every transaction since the
			 * interrupt handler disables it */
			dma_config |= DMA_CFG_INT_ENB;
			// Num of AHB (32-bit) words to transferred minus 1
			dma_config |= DMA_CFG_WRD_CNT(current_transfer - 1);
#ifdef SNOR_USE_DOUBLE_COPY
			writel(c->dma_phys_buffer, AHB_ADDR_PTR_REG);
#else
			writel(ahb_address, AHB_ADDR_PTR_REG);
#endif
			writel(nor_address, NOR_ADDR_PTR_REG);
			writel(snor_config, CONFIG_REG);
			writel(dma_config, DMA_CFG_REG);
			if (wait_for_dma_completion(c)) {
				dev_err(c->dev, "timout waiting for DMA\n");
				/* Transfer the remaining words by memcpy */
				bytes_remaining += (word32_count << 2);
				goto dma_failed;
			}
#ifdef SNOR_USE_DOUBLE_COPY
			memcpy((char *)(ahb_address),
			       (char *)(c->dma_virt_buffer),
			       (current_transfer << 2));
#endif
		}

dma_failed:
#ifndef SNOR_USE_DOUBLE_COPY
		dma_unmap_single(c->dev, ahb_address, len, DMA_FROM_DEVICE);
#endif
		;
	}
	// Put the controller back into slave mode.
	snor_config = readl(CONFIG_REG);
	snor_config &= ~CONFIG_MST_ENB;
	snor_config |= CONFIG_DEVICE_MODE(0);
	writel(snor_config, CONFIG_REG);
#ifndef SNOR_USE_DOUBLE_COPY
out_copy:
#endif
	memcpy_fromio(((char *)to + (dma_transfer_count << 2)),
		      ((char *)(map->virt + from) + (dma_transfer_count << 2)),
		      bytes_remaining);

	writel(c->timing0_default, TIMING0_REG);
	writel(c->timing1_default, TIMING1_REG);
}

static irqreturn_t tegra_nor_isr(int flag, void *dev_id)
{
	struct tegra_nor_info *info = (struct tegra_nor_info *)dev_id;
	uint32_t dma_config = readl(DMA_CFG_REG);
	if (dma_config & DMA_CFG_INT_STA) {
		/* Disable interrupts. WAR for BUG:821560 */
		dma_config &= ~DMA_CFG_INT_ENB;
		writel(dma_config, DMA_CFG_REG);
		complete(&info->dma_complete);
	} else {
		printk(KERN_ERR "%s: Spurious interrupt\n", __func__);
	}
	return IRQ_HANDLED;
}

static int tegra_snor_controller_init(struct tegra_nor_info *info)
{
	struct tegra_nor_chip_parms *chip_parm = info->plat->chip_parms;
	uint32_t width = info->plat->flash->width;
	uint32_t config = 0;
	unsigned long clk_rate_khz = clk_get_rate(info->clk)/1000;

	config |= CONFIG_DEVICE_MODE(0);
	config |= CONFIG_SNOR_CS(0);
	config &= ~CONFIG_DEVICE_TYPE;	// Select NOR
	config |= CONFIG_WP; /* Enable writes */
	switch (width) {
	case 2:
		config &= ~CONFIG_WORDWIDE;	//16 bit
		break;
	default:
	case 4:
		config |= CONFIG_WORDWIDE;	//32 bit
		break;
	}
	config |= CONFIG_BURST_LEN(0);
	config &= ~CONFIG_MUX_MODE;
	writel(config, CONFIG_REG);
	info->init_config = config;

#define TIME_TO_CNT(timing) (((((timing) * (clk_rate_khz)) + 1000000 - 1) / 1000000) - 1)
	info->timing0_default = (TIMING0_PG_RDY(TIME_TO_CNT(chip_parm->timing_default.pg_rdy)) |
		   TIMING0_PG_SEQ(TIME_TO_CNT(chip_parm->timing_default.pg_seq)) |
		   TIMING0_MUX(TIME_TO_CNT(chip_parm->timing_default.mux)) |
		   TIMING0_HOLD(TIME_TO_CNT(chip_parm->timing_default.hold)) |
		   TIMING0_ADV(TIME_TO_CNT(chip_parm->timing_default.adv)) |
		   TIMING0_CE(TIME_TO_CNT(chip_parm->timing_default.ce)));

	writel(info->timing0_default, TIMING0_REG);

	info->timing1_default = (TIMING1_WE(TIME_TO_CNT(chip_parm->timing_default.we)) |
		   TIMING1_OE(TIME_TO_CNT(chip_parm->timing_default.oe)) |
		   TIMING1_WAIT(TIME_TO_CNT(chip_parm->timing_default.wait)));

	writel(info->timing1_default, TIMING1_REG);

	info->timing0_read = (TIMING0_PG_RDY(TIME_TO_CNT(chip_parm->timing_read.pg_rdy)) |
		   TIMING0_PG_SEQ(TIME_TO_CNT(chip_parm->timing_read.pg_seq)) |
		   TIMING0_MUX(TIME_TO_CNT(chip_parm->timing_read.mux)) |
		   TIMING0_HOLD(TIME_TO_CNT(chip_parm->timing_read.hold)) |
		   TIMING0_ADV(TIME_TO_CNT(chip_parm->timing_read.adv)) |
		   TIMING0_CE(TIME_TO_CNT(chip_parm->timing_read.ce)));

	info->timing1_read = (TIMING1_WE(TIME_TO_CNT(chip_parm->timing_read.we)) |
		   TIMING1_OE(TIME_TO_CNT(chip_parm->timing_read.oe)) |
		   TIMING1_WAIT(TIME_TO_CNT(chip_parm->timing_read.wait)));
#undef TIME_TO_CNT
	return 0;
}

static int __init tegra_nor_probe(struct platform_device *pdev)
{
	int err = 0;
	struct tegra_nor_platform *plat = pdev->dev.platform_data;
	struct tegra_nor_info *info = NULL;
	struct resource *res;
	struct resource *iomem;
	void *base;
	int irq;

	if (!plat) {
		pr_err("%s: no platform device info\n", __func__);
		err = -EINVAL;
		goto fail;
	} else if (!plat->chip_parms) {
		pr_err("%s: no platform nor parms\n", __func__);
		err = -EINVAL;
		goto fail;
	}

	/* Get NOR flash aperture & map the same */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource?\n");
		err = -ENODEV;
		goto fail;
	}

	iomem = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!iomem) {
		dev_err(&pdev->dev, "NOR region already claimed\n");
		err = -EBUSY;
		goto fail;
	}

	info = kzalloc(sizeof(struct tegra_nor_info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto out_release_region;
	}

	info->plat = plat;
	info->dev = &pdev->dev;
	info->map.bankwidth = plat->flash->width;
	info->map.name = dev_name(&pdev->dev);

	base = ioremap(iomem->start, resource_size(iomem));
	if (!base) {
		dev_err(&pdev->dev, "Can't ioremap NOR region\n");
		err = -ENOMEM;
		goto out_free_info;
	}

	info->map.phys = iomem->start;
	info->map.size = resource_size(iomem);
	info->map.virt = base;

	/* Get NOR interrupt */
	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "no irq resource?\n");
		err = -ENODEV;
		goto out_iounmap;
	}

	info->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(info->clk)) {
		err = PTR_ERR(info->clk);
		goto out_iounmap;
	}

	err = clk_enable(info->clk);
	if (err != 0)
		goto out_clk_put;

	info->dma_virt_buffer = dma_alloc_coherent(&pdev->dev, SNOR_DMA_LIMIT,
						   &info->dma_phys_buffer,
						   GFP_KERNEL);
	if (info->dma_virt_buffer == NULL) {
		dev_err(&pdev->dev, "Could not allocate buffer for DMA");
		goto out_clk_disable;
	}

	simple_map_init(&info->map);
	/* DMA using SNOR DMA controller */
	info->map.copy_from = tegra_flash_dma;
	/* Intialise the SNOR controller before probe */
	tegra_snor_controller_init(info);
	init_completion(&info->dma_complete);

	/* Register SNOR DMA completion interrupt */
	err = request_irq(irq, tegra_nor_isr, IRQF_DISABLED, pdev->name, info);
	if (err) {
		dev_err(&pdev->dev, "Failed to request irq %i\n", irq);
		goto out_dma_free_coherent;
	}

	info->mtd = do_map_probe(plat->flash->map_name, &info->map);
	if (!info->mtd) {
		err = -EIO;
		goto out_dma_free_coherent;
	}
	info->mtd->owner = THIS_MODULE;
	info->parts = NULL;

#ifdef CONFIG_MTD_PARTITIONS
	err = parse_mtd_partitions(info->mtd, part_probes, &info->parts, 0);
	if (err > 0)
		err = add_mtd_partitions(info->mtd, info->parts, err);
	else if (err <= 0 && plat->flash->parts)
		err =
		    add_mtd_partitions(info->mtd, plat->flash->parts,
				       plat->flash->nr_parts);
	else
#endif
		add_mtd_device(info->mtd);

	platform_set_drvdata(pdev, info);
	return 0;

out_dma_free_coherent:
	dma_free_coherent(&pdev->dev, SNOR_DMA_LIMIT, info->dma_virt_buffer,
			  info->dma_phys_buffer);
out_clk_disable:
	clk_disable(info->clk);
out_clk_put:
	clk_put(info->clk);
out_iounmap:
	iounmap(iomem);
out_free_info:
	kfree(info);
out_release_region:
	release_region(iomem->start, resource_size(iomem));
fail:
	pr_err("Tegra NOR probe failed\n");
	return err;
}

static int __exit tegra_nor_remove(struct platform_device *pdev)
{
	struct tegra_nor_info *info = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	if (info) {
		if (info->parts) {
			del_mtd_partitions(info->mtd);
			kfree(info->parts);
		} else
			del_mtd_device(info->mtd);
		map_destroy(info->mtd);
		release_mem_region(info->map.phys, info->map.size);
		iounmap((void __iomem *)info->map.virt);
		dma_free_coherent(&pdev->dev, SNOR_DMA_LIMIT,
				  info->dma_virt_buffer, info->dma_phys_buffer);
		clk_disable(info->clk);
		clk_put(info->clk);
		kfree(info);
	}

	return 0;
}

static struct platform_driver tegra_nor_driver = {
	.probe = tegra_nor_probe,
	.remove = __exit_p(tegraflash_remove),
	.driver = {
		   .name = "tegra_nor",
		   .owner = THIS_MODULE,
		   },
};

static int __init tegra_nor_init(void)
{
	return platform_driver_register(&tegra_nor_driver);
}

static void __exit tegra_nor_exit(void)
{
	platform_driver_unregister(&tegra_nor_driver);
}

module_init(tegra_nor_init);
module_exit(tegra_nor_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NOR Flash mapping driver for NVIDIA Tegra based boards");
MODULE_ALIAS("platform:tegraflash");
