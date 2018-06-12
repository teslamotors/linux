/*
 * Intel HDA Cannonlake FPGA driver
 *
 * Copyright (C) 2014, Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define HDA_INTEL_CNL_FPGA_ROM_SIZE (16 * 1024)
#define HDA_INTEL_CNL_FPGA_ROM_BAR_IDX 2
#define HDA_INTEL_CNL_FPGA_ROM_KEY_OFFSET 0x805c
#define HDA_INTEL_CNL_FPGA_ROM_MEM_OFFSET 0xc000

struct hda_intel_cnl_fpga_dev {
	void __iomem *rom_mem_base;
	struct pci_dev *pdev;
};

static u32 hda_intel_cnl_fpga_readl(
	struct hda_intel_cnl_fpga_dev *fpgadev,
	u32 offset)
{
	u32 val;

	val = readl((fpgadev)->rom_mem_base + offset);
	printk(KERN_INFO "%s: TG@%x == %x\n", __func__, offset, val);
	return val;
}

static void hda_intel_cnl_fpga_writel(
	struct hda_intel_cnl_fpga_dev *fpgadev,
	u32 offset,
	u32 val)
{
	printk("%s: TG@%x <= %x\n", __func__, offset, val);
	writel(val, (fpgadev)->rom_mem_base + offset);
}

static int hda_intel_cnl_fpga_rom_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t hda_intel_cnl_fpga_rom_read(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	struct hda_intel_cnl_fpga_dev *fpgadev = file->private_data;
	int i, size;
	u32 *buf;

	printk(KERN_INFO "hda_intel_cnl_fpga_rom_read_in\n");

	dev_dbg(&fpgadev->pdev->dev, "%s: pbuf: %p, *ppos: 0x%llx",
		__func__, buffer, *ppos);

	size = HDA_INTEL_CNL_FPGA_ROM_SIZE;

	if (*ppos >= size)
		return 0;
	if (*ppos + count > size)
		count = size - *ppos;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_dbg(&fpgadev->pdev->dev, " %s: kzalloc failed, aborting\n",
			__func__);
		return -ENOMEM;
	}
	size = size / sizeof(*buf);

	printk(KERN_INFO "%s: bar: 0x%p\n", __func__, fpgadev->rom_mem_base);

	for (i = 0; i < size; i++) {
		hda_intel_cnl_fpga_writel(
			fpgadev, HDA_INTEL_CNL_FPGA_ROM_KEY_OFFSET, 0);

		buf[i] = hda_intel_cnl_fpga_readl(fpgadev,
			HDA_INTEL_CNL_FPGA_ROM_MEM_OFFSET + i * sizeof(*buf));
	}

	if (copy_to_user(buffer, buf, count)) {
		dev_err(&fpgadev->pdev->dev, " %s: copy_to_user failed, aborting\n",
			__func__);
		return -EFAULT;
	}
	kfree(buf);

	*ppos += count;

	dev_dbg(&fpgadev->pdev->dev, "%s: *ppos: 0x%llx, count: %zu",
		__func__, *ppos, count);

	printk(KERN_INFO "hda_intel_cnl_fpga_rom_read_out\n");

	return count;
}

static ssize_t hda_intel_cnl_fpga_rom_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	struct hda_intel_cnl_fpga_dev *fpgadev = file->private_data;
	int i, size;
	u32 *buf;

	printk(KERN_INFO "hda_intel_cnl_fpga_rom_write_in\n");

	dev_dbg(&fpgadev->pdev->dev, "%s: pbuf: %p, *ppos: 0x%llx",
			__func__, buffer, *ppos);

	size = HDA_INTEL_CNL_FPGA_ROM_SIZE;

	if (*ppos >= size)
		return 0;
	if (*ppos + count > size)
		count = size - *ppos;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_err(&fpgadev->pdev->dev, " %s: kzalloc failed, aborting\n",
			__func__);
		return -ENOMEM;
	}

	if (copy_from_user(buf, buffer, count)) {
		dev_err(&fpgadev->pdev->dev, " %s: copy_from_user failed, aborting\n",
			__func__);
		return -EFAULT;
	}

	size = size / sizeof(*buf);

	for (i = 0; i < size; i++) {
		hda_intel_cnl_fpga_writel(
			fpgadev, HDA_INTEL_CNL_FPGA_ROM_KEY_OFFSET, 0);

		hda_intel_cnl_fpga_writel(fpgadev,
			HDA_INTEL_CNL_FPGA_ROM_MEM_OFFSET + i * sizeof(*buf),
			buf[i]);
	}

	kfree(buf);
	*ppos += count;

	dev_dbg(&fpgadev->pdev->dev, "%s: *ppos: 0x%llx, count: %zu",
		__func__, *ppos, count);

	printk(KERN_INFO "hda_intel_cnl_fpga_rom_write_out\n");

	return count;
}

static const struct file_operations hda_intel_cnl_fpga_rom_fops = {
	.owner = THIS_MODULE,
	.open = hda_intel_cnl_fpga_rom_open,
	.read = hda_intel_cnl_fpga_rom_read,
	.write = hda_intel_cnl_fpga_rom_write,
};

static int hda_intel_cnl_fpga_probe(struct pci_dev *pdev,
		const struct pci_device_id *pci_id)
{
	struct hda_intel_cnl_fpga_dev *fpgadev;
	int ret = 0;

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: pci_enable_device failed: %d\n",
			__func__, ret);
		return ret;
	}

	fpgadev = kzalloc(sizeof(struct hda_intel_cnl_fpga_dev), GFP_KERNEL);
	if (!fpgadev) {
		dev_err(&pdev->dev, "%s: kzalloc failed, aborting.\n",
			__func__);
		ret = -ENOMEM;
		goto err_alloc;
	}

	printk(KERN_INFO "%s: bar: 0, start 0x%p\n", __func__,
	       (void *)pci_resource_start(pdev, 0));
	printk(KERN_INFO "%s: bar: 0, flags 0x%lx\n", __func__,
	       pci_resource_flags(pdev, 0));
	printk(KERN_INFO "%s: bar: 1, start 0x%p\n", __func__,
	       (void *)pci_resource_start(pdev, 1));
	printk(KERN_INFO "%s: bar: 1, flags 0x%lx\n", __func__,
	       pci_resource_flags(pdev, 1));
	printk(KERN_INFO "%s: bar: 2, start 0x%p\n", __func__,
	       (void *)pci_resource_start(pdev, 2));
	printk(KERN_INFO "%s: bar: 2, flags 0x%lx\n", __func__,
	       pci_resource_flags(pdev, 2));
	printk(KERN_INFO "%s: bar: 3, start 0x%p\n", __func__,
	       (void *)pci_resource_start(pdev, 3));
	printk(KERN_INFO "%s: bar: 3, flags 0x%lx\n", __func__,
	       pci_resource_flags(pdev, 3));

	fpgadev->rom_mem_base
		= pci_ioremap_bar(pdev, HDA_INTEL_CNL_FPGA_ROM_BAR_IDX);
	printk("%s: bar: 0x%p\n", __func__, fpgadev->rom_mem_base);
	if (!fpgadev->rom_mem_base) {
		dev_err(&pdev->dev, "%s: ioremap failed, aborting\n",
			__func__);
		ret = -ENXIO;
		goto err_map;
	}

	fpgadev->pdev = pdev;

	pci_set_drvdata(pdev, fpgadev);

	if (!debugfs_create_file("cnl_oed_rom", 0644, NULL,
		fpgadev, &hda_intel_cnl_fpga_rom_fops)) {
		dev_err(&pdev->dev, "%s: cannot create debugfs entry.\n",
			__func__);
		ret = -ENODEV;
		goto err_map;
	}

	return ret;

err_map:
	if (fpgadev->rom_mem_base) {
		iounmap(fpgadev->rom_mem_base);
		fpgadev->rom_mem_base = NULL;
	}
	kfree(fpgadev);
err_alloc:
	pci_disable_device(pdev);
	return ret;
}

static void hda_intel_cnl_fpga_remove(struct pci_dev *pdev)
{
	struct hda_intel_cnl_fpga_dev *fpgadev = pci_get_drvdata(pdev);

	printk(KERN_INFO "hda_intel_cnl_fpga_remove_in");
	/* unmap PCI memory space, mapped during device init. */
	if (fpgadev->rom_mem_base) {
		iounmap(fpgadev->rom_mem_base);
		fpgadev->rom_mem_base = NULL;
	}
	pci_disable_device(pdev);
	kfree(fpgadev);

	printk(KERN_INFO "hda_intel_cnl_fpga_remove_out");
}

/* PCI IDs */
static const struct pci_device_id hda_intel_cnl_fpga_ids[] = {
	{ PCI_DEVICE(0x8086, 0xF501) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, hda_intel_cnl_fpga_ids);

/* pci_driver definition */
static struct pci_driver hda_intel_cnl_fpga_driver = {
	.name = KBUILD_MODNAME,
	.id_table = hda_intel_cnl_fpga_ids,
	.probe = hda_intel_cnl_fpga_probe,
	.remove = hda_intel_cnl_fpga_remove,
};

module_pci_driver(hda_intel_cnl_fpga_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel HDA CNL FPGA driver");
