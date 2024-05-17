// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
//
// AMD SPI controller driver
//
// Copyright (c) 2020, Advanced Micro Devices, Inc.
// spi-mem conversion by Tesla, Inc
//
// Author: Sanjay R Mehta <sanju.mehta@amd.com>

#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-mem.h>
#include <linux/dmi.h>
#include <linux/refcount.h>

#define AMD_SPI_CTRL0_REG	0x00
#define AMD_SPI_EXEC_CMD	BIT(16)
#define AMD_SPI_FIFO_CLEAR	BIT(20)
#define AMD_SPI_BUSY		BIT(31)

#define AMD_SPI_OPCODE_MASK	0xFF

#define AMD_SPI_ALT_CS_REG	0x1D
#define AMD_SPI_ALT_CS_MASK	0x3

#define AMD_SPI_FIFO_BASE	0x80
#define AMD_SPI_TX_COUNT_REG	0x48
#define AMD_SPI_RX_COUNT_REG	0x4B
#define AMD_SPI_STATUS_REG	0x4C

#define AMD_SPI_MEM_SIZE	200

#define AMD_SPI_MAX_FIFO_DEPTH	71

#define NOR_REFCOUNT_IDLE	1

struct amd_spi {
	void __iomem *io_remap_addr;
	unsigned long io_base_addr;
	u32 rom_addr;
	u8 chip_select;
	refcount_t spi_nor_refcount;	/* Any spi-nor devices in use */
};

/*
 * Optionally repurpose an unused controller register as a hardware semaphore for
 * arbitrating SPI bus ownership between the OS and bootloader/SMM.
 */
static bool amd_spi_sem_en = false;
#define AMD_SPI_SEM_REG		0xfc
#define AMD_SPI_SEM_RESETVAL	0x00
#define AMD_SPI_SEM_SMI_DENY	0x20
#define AMD_SPI_SEM_LOCKED	0x10
#define AMD_SPI_SEM_UNLOCKED	0x08

static inline u8 amd_spi_readreg8(struct spi_master *master, int idx)
{
	struct amd_spi *amd_spi = spi_master_get_devdata(master);

	return ioread8((u8 __iomem *)amd_spi->io_remap_addr + idx);
}

static inline void amd_spi_writereg8(struct spi_master *master, int idx,
				     u8 val)
{
	struct amd_spi *amd_spi = spi_master_get_devdata(master);

	iowrite8(val, ((u8 __iomem *)amd_spi->io_remap_addr + idx));
}

static inline void amd_spi_setclear_reg8(struct spi_master *master, int idx,
					 u8 set, u8 clear)
{
	struct amd_spi *amd_spi = spi_master_get_devdata(master);
	volatile void __iomem *p = (u8 __iomem *)amd_spi->io_remap_addr + idx;
	u8 tmp = ioread8((void __iomem *) p);

	tmp = (tmp & ~clear) | set;
	iowrite8(tmp, (void __iomem *) p);
}

static inline u32 amd_spi_readreg32(struct spi_master *master, int idx)
{
	struct amd_spi *amd_spi = spi_master_get_devdata(master);

	return ioread32((u8 __iomem *)amd_spi->io_remap_addr + idx);
}

static inline void amd_spi_writereg32(struct spi_master *master, int idx,
				      u32 val)
{
	struct amd_spi *amd_spi = spi_master_get_devdata(master);

	iowrite32(val, ((u8 __iomem *)amd_spi->io_remap_addr + idx));
}

static inline void amd_spi_setclear_reg32(struct spi_master *master, int idx,
					  u32 set, u32 clear)
{
	struct amd_spi *amd_spi = spi_master_get_devdata(master);
	volatile void __iomem *p = (u8 __iomem *)amd_spi->io_remap_addr + idx;
	u32 tmp = ioread32((void __iomem *) p);

	tmp = (tmp & ~clear) | set;
	iowrite32(tmp, (void __iomem *) p);
}

static void amd_spi_select_chip(struct spi_master *master)
{
	struct amd_spi *amd_spi = spi_master_get_devdata(master);
	u8 chip_select = amd_spi->chip_select;

	amd_spi_setclear_reg8(master, AMD_SPI_ALT_CS_REG, chip_select,
			      AMD_SPI_ALT_CS_MASK);
}

static void amd_spi_clear_fifo_ptr(struct spi_master *master)
{
	amd_spi_setclear_reg32(master, AMD_SPI_CTRL0_REG, AMD_SPI_FIFO_CLEAR,
			       AMD_SPI_FIFO_CLEAR);
}

static void amd_spi_set_opcode(struct spi_master *master, u8 cmd_opcode)
{
	amd_spi_setclear_reg32(master, AMD_SPI_CTRL0_REG, cmd_opcode,
			       AMD_SPI_OPCODE_MASK);
}

static inline void amd_spi_set_rx_count(struct spi_master *master,
					u8 rx_count)
{
	amd_spi_setclear_reg8(master, AMD_SPI_RX_COUNT_REG, rx_count, 0xff);
}

static inline void amd_spi_set_tx_count(struct spi_master *master,
					u8 tx_count)
{
	amd_spi_setclear_reg8(master, AMD_SPI_TX_COUNT_REG, tx_count, 0xff);
}

static inline int amd_spi_busy_wait(struct amd_spi *amd_spi)
{
	bool spi_busy;
	int timeout = 100000;

	/* poll for SPI bus to become idle */
	spi_busy = (ioread32((u8 __iomem *)amd_spi->io_remap_addr +
		    AMD_SPI_CTRL0_REG) & AMD_SPI_BUSY) == AMD_SPI_BUSY;
	while (spi_busy) {
		usleep_range(10, 20);
		if (timeout-- < 0)
			return -ETIMEDOUT;

		spi_busy = (ioread32((u8 __iomem *)amd_spi->io_remap_addr +
			    AMD_SPI_CTRL0_REG) & AMD_SPI_BUSY) == AMD_SPI_BUSY;
	}

	return 0;
}

static int amd_spi_execute_opcode(struct spi_master *master)
{
	struct amd_spi *amd_spi = spi_master_get_devdata(master);

	/* Set ExecuteOpCode bit in the CTRL0 register */
	amd_spi_setclear_reg32(master, AMD_SPI_CTRL0_REG, AMD_SPI_EXEC_CMD,
			       AMD_SPI_EXEC_CMD);

	return amd_spi_busy_wait(amd_spi);
}

static int amd_spi_master_setup(struct spi_device *spi)
{
	struct spi_master *master = spi->master;

	amd_spi_clear_fifo_ptr(master);

	return 0;
}

static int amd_spi_infoz_quirks(const struct dmi_system_id *id)
{
	printk(KERN_INFO "spi-amd: Enabling InfoZ quirks\n");
	amd_spi_sem_en = true;

	return 0;
}

static inline int amd_spi_smi_is_denied(struct spi_master *master)
{
	return !!(amd_spi_readreg8(master, AMD_SPI_SEM_REG) &
		AMD_SPI_SEM_SMI_DENY);
}

static inline void amd_spi_smi_deny(struct spi_master *master, int val)
{
	amd_spi_setclear_reg32(master, AMD_SPI_SEM_REG,
		val ? AMD_SPI_SEM_SMI_DENY : 0,
		val ? 0 : AMD_SPI_SEM_SMI_DENY);
}

static int amd_spi_mem_get(struct spi_mem *mem)
{
	struct spi_controller *master = mem->spi->controller;
	struct amd_spi *amd_spi = spi_master_get_devdata(master);

	/*
	 * Keep track of any spi-nor users and hold the OS ownership
	 * semaphore for as long as any spi-nor operations are happening
	 * on this controller.  This protects against SMM interference.
	 */
	refcount_inc(&(amd_spi->spi_nor_refcount));

	if (!amd_spi_sem_en)
		return 0;

	/*
	 * Grab repurposed HW semaphore
	 *
	 * Because the mtd/spi layers already have their own mutex locking
	 * at the SPI bus level within the OS, the only other contender
	 * for this lock is the SMI handler.  Since the SMI handler can
	 * be deferred and only reads it, we should always be able to
	 * grab this lock without any atomic operations.
	 */
	amd_spi_setclear_reg8(master, AMD_SPI_SEM_REG, AMD_SPI_SEM_LOCKED,
			      AMD_SPI_SEM_UNLOCKED);

	return 0;
}

static void amd_spi_mem_put(struct spi_mem *mem)
{
	struct spi_controller *master = mem->spi->controller;
	struct amd_spi *amd_spi = spi_master_get_devdata(master);

	refcount_dec(&(amd_spi->spi_nor_refcount));

	if (!amd_spi_sem_en)
		return;

	if (refcount_read(&(amd_spi->spi_nor_refcount)) == NOR_REFCOUNT_IDLE) {
		/* Nothing else using it, release semaphore */
		amd_spi_setclear_reg8(master, AMD_SPI_SEM_REG,
				      AMD_SPI_SEM_UNLOCKED, AMD_SPI_SEM_LOCKED);
	}
}

static ssize_t spi_amd_smi_deny_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct spi_master *master = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", amd_spi_smi_is_denied(master));
}

static ssize_t spi_amd_smi_deny_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct spi_master *master = dev_get_drvdata(dev);
	int error;
	unsigned int val;

	error = kstrtouint(buf, 10, &val);
	if (error)
		return error;

	amd_spi_smi_deny(master, val);

	return count;
}

/*
 * A "message" contains multiple "transfers".
 * Our FIFO is 71 bytes and needs to hold all TX bytes (except the opcode),
 * plus all RX bytes requested.
 *
 * For accounting purposes here, "messages" count all overhead bytes
 * (including opcode, address, dummy bytes), and "transfers" only count the
 * data bytes.
 *
 * Practically this means we should limit ourselves to 64 bytes of actual
 * data at a time.
 */
static size_t amd_spi_max_msg_size(struct spi_device *spi)
{
	return AMD_SPI_MAX_FIFO_DEPTH + 1;	/* opcode excluded */
}

static size_t amd_spi_max_xfer_size(struct spi_device *spi)
{
	const int overhead = 7;		/* 1 opcode, 4 addr, 2 dummy */
	return AMD_SPI_MAX_FIFO_DEPTH - overhead;
}

static int amd_spi_adjust_op_size(struct spi_mem *mem, struct spi_mem_op *op)
{
	int fifo_len;
	fifo_len = op->addr.nbytes + op->dummy.nbytes + op->data.nbytes;

	/* Limit transfer sizes by our FIFO depth */
	if (fifo_len > AMD_SPI_MAX_FIFO_DEPTH)
		op->data.nbytes -= (fifo_len - AMD_SPI_MAX_FIFO_DEPTH);

	return 0;
}

static bool amd_spi_mem_supports_op(struct spi_mem *mem,
				    const struct spi_mem_op *op)
{
	/* Only single IO mode seems to work with Alternate programming method */
	if (op->data.buswidth > 1 || op->addr.buswidth > 1 ||
	    op->dummy.buswidth > 1 || op->cmd.buswidth > 1)
		return false;

	return true;
}

static int amd_spi_mem_exec_op(struct spi_mem *mem,
			       const struct spi_mem_op *op)
{
	struct spi_master *master = mem->spi->master;
	struct amd_spi *amd_spi = spi_controller_get_devdata(master);
	int ret;
	u8 fifo_tx, fifo_rx;
	int i;
	u8 fifo_idx = 0;
	u8 *data_buf;

	amd_spi_mem_get(mem);

	ret = amd_spi_busy_wait(amd_spi);
	if (ret != 0)
		goto out;

	amd_spi->chip_select = mem->spi->chip_select;
	amd_spi_select_chip(master);

	amd_spi_set_opcode(master, op->cmd.opcode);

	/* Set up transfer size */
	switch (op->data.dir) {
	case SPI_MEM_NO_DATA:
		fifo_tx = op->addr.nbytes + op->dummy.nbytes;
		fifo_rx = 0;
		break;
	case SPI_MEM_DATA_OUT:
		fifo_tx = op->addr.nbytes + op->dummy.nbytes + (u8) op->data.nbytes;
		fifo_rx = 0;
		data_buf = (u8 *) op->data.buf.out;
		break;
	case SPI_MEM_DATA_IN:
		fifo_tx = op->addr.nbytes + op->dummy.nbytes;
		fifo_rx = (u8) op->data.nbytes;
		data_buf = (u8 *) op->data.buf.in;
		break;
	}
	amd_spi_set_tx_count(master, fifo_tx);
	amd_spi_set_rx_count(master, fifo_rx);

	/* Write address to FIFO, MSB first */
	for (i = 0; i < op->addr.nbytes; i++) {
		iowrite8((op->addr.val >> (op->addr.nbytes-(i+1))*8) & 0xff,
			 ((u8 __iomem *) amd_spi->io_remap_addr +
			 AMD_SPI_FIFO_BASE + fifo_idx));
		fifo_idx++;
	}

	/* Write dummy bytes to FIFO */
	for (i = 0; i < op->dummy.nbytes; i++) {
		iowrite8(0x00, ((u8 __iomem *) amd_spi->io_remap_addr +
			 AMD_SPI_FIFO_BASE + fifo_idx));
		fifo_idx++;
	}

	/* Write TX data bytes to FIFO */
	if (op->data.dir == SPI_MEM_DATA_OUT) {
		for (i = 0; i < op->data.nbytes; i++) {
			iowrite8(data_buf[i],
				 ((u8 __iomem *) amd_spi->io_remap_addr +
				 AMD_SPI_FIFO_BASE + fifo_idx));
			fifo_idx++;
		}
	}
	BUG_ON(fifo_idx != fifo_tx);

	ret = amd_spi_execute_opcode(master);
	if (ret != 0)
		goto out;

	/* Read data from FIFO to receive buffer  */
	memcpy_fromio((void *) data_buf,
		      (u8 __iomem *)amd_spi->io_remap_addr + AMD_SPI_FIFO_BASE + fifo_tx,
		      fifo_rx);

out:
	amd_spi_mem_put(mem);
	return ret;
}

static const struct spi_controller_mem_ops amd_spi_mem_ops = {
	.adjust_op_size = amd_spi_adjust_op_size,
	.supports_op = amd_spi_mem_supports_op,
	.exec_op = amd_spi_mem_exec_op,
	/* Ensures higher-level spi-nor ops are guarded by HW semaphore */
	.prepare = amd_spi_mem_get,
	.unprepare = amd_spi_mem_put,
};

static const struct dmi_system_id amd_spi_quirk_dmi_table[] = {
	{
		.ident = "Tesla InfoZ",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_FAMILY, "Tesla_InfoZ"),
		},
		.callback = amd_spi_infoz_quirks,
	},
	{ }
};

static umode_t spi_amd_attr_is_visible(struct kobject *kobj,
				       struct attribute *attr, int n)
{
	if (!amd_spi_sem_en)
		return 0;

	return attr->mode;
}

static DEVICE_ATTR(smi_deny, 0664, spi_amd_smi_deny_show, spi_amd_smi_deny_store);

static struct attribute *spi_amd_attributes[] = {
	&dev_attr_smi_deny.attr,
	NULL
};

static const struct attribute_group spi_amd_attr_group = {
	.is_visible	= spi_amd_attr_is_visible,
	.attrs		= spi_amd_attributes,
};

static int amd_spi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct spi_master *master;
	struct amd_spi *amd_spi;
	struct resource *res;
	int err = 0;

	/* Allocate storage for spi_master and driver private data */
	master = spi_alloc_master(dev, sizeof(struct amd_spi));
	if (!master) {
		dev_err(dev, "Error allocating SPI master\n");
		return -ENOMEM;
	}

	amd_spi = spi_master_get_devdata(master);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	amd_spi->io_remap_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(amd_spi->io_remap_addr)) {
		err = PTR_ERR(amd_spi->io_remap_addr);
		dev_err(dev, "error %d ioremap of SPI registers failed\n", err);
		goto err_free_master;
	}
	dev_dbg(dev, "io_remap_address: %p\n", amd_spi->io_remap_addr);
	refcount_set(&(amd_spi->spi_nor_refcount), NOR_REFCOUNT_IDLE);

	/* Initialize the spi_master fields */
	master->bus_num = 0;
	master->num_chipselect = 4;
	master->mode_bits = 0;
	master->flags = SPI_MASTER_HALF_DUPLEX;
	master->setup = amd_spi_master_setup;
	master->mem_ops = &amd_spi_mem_ops;
	master->max_transfer_size = amd_spi_max_xfer_size;
	master->max_message_size = amd_spi_max_msg_size;

	dmi_check_system(amd_spi_quirk_dmi_table);

	/* Register the controller with SPI framework */
	err = devm_spi_register_master(dev, master);
	if (err) {
		dev_err(dev, "error %d registering SPI controller\n", err);
		goto err_free_master;
	}

	platform_set_drvdata(pdev, master);
	err = devm_device_add_group(dev, &spi_amd_attr_group);
	if (err) {
		goto err_free_master;
	}

	return 0;

err_free_master:
	spi_master_put(master);

	return err;
}

static const struct acpi_device_id spi_acpi_match[] = {
	{ "AMDI0061", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, spi_acpi_match);

static struct platform_driver amd_spi_driver = {
	.driver = {
		.name = "amd_spi",
		.acpi_match_table = ACPI_PTR(spi_acpi_match),
	},
	.probe = amd_spi_probe,
};

module_platform_driver(amd_spi_driver);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Sanjay Mehta <sanju.mehta@amd.com>");
MODULE_DESCRIPTION("AMD SPI Master Controller Driver");
