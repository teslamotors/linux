/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <mach/clk.h>
#include <soc/tegra/tegra_bpmp.h>
#include <soc/tegra/tegra_pasr.h>

#include "../../../arch/arm/mach-tegra/iomap.h"
#include "bpmp.h"

#define BPMP_FIRMWARE_NAME		"bpmp.bin"

#define FLOW_CTRL_HALT_COP_EVENTS	IO_ADDRESS(TEGRA_FLOW_CTRL_BASE + 0x4)
#define FLOW_MODE_STOP			(0x2 << 29)
#define FLOW_MODE_NONE			0x0
#define TEGRA_NVAVP_RESET_VECTOR_ADDR	IO_ADDRESS(TEGRA_EXCEPTION_VECTORS_BASE + 0x200)

static struct clk *cop_clk;
static struct clk *sclk;
static struct clk *emc_clk;

#ifdef CONFIG_DEBUG_FS
static phys_addr_t loadfw_phys;
static void *loadfw_virt;

static struct bpmp_cpuidle_state plat_cpuidle_state[] = {
	{ TEGRA_PM_CC4, "CC4" },
	{ TEGRA_PM_CC6, "CC6" },
	{ TEGRA_PM_CC7, "CC7" },
	{ TEGRA_PM_SC2, "SC2" },
	{ TEGRA_PM_SC3, "SC3" },
	{ TEGRA_PM_SC4, "SC4" },
	{ TEGRA_PM_SC7, "SC7" },
	{ 0, NULL }
};

struct fw_header {
	uint32_t magic;
	uint32_t version;
	uint32_t chip_id;
	uint32_t mem_size;
	uint32_t reset_offset;
	uint8_t reserved2[124];
	uint8_t md5sum[32];
	uint8_t common_head[40];
	uint8_t platform_head[40];
} __PACKED;

struct platform_config {
	u32 magic;
	u32 version;
	u32 chip_id;
	u32 chip_version;
	u32 sku;
	u32 pmic_id;
	u32 pmic_sku;
};

static void bpmp_stop(void)
{
	writel(FLOW_MODE_STOP, FLOW_CTRL_HALT_COP_EVENTS);
}

static void bpmp_reset(u32 addr)
{
	bpmp_stop();
	writel(addr, TEGRA_NVAVP_RESET_VECTOR_ADDR);

	tegra_periph_reset_assert(cop_clk);
	udelay(2);
	tegra_periph_reset_deassert(cop_clk);

	writel(FLOW_MODE_NONE, FLOW_CTRL_HALT_COP_EVENTS);
}

static void bpmp_loadfw(struct device *device, const void *data, int size)
{
	struct fw_header *h;
	struct platform_config bpmp_config;
	unsigned int cfgsz = sizeof(bpmp_config);
	u32 reset_addr;
	int r;
	const int sz = sizeof(h->md5sum);
	char fwtag[sz + 1];

	if (!data || !size) {
		dev_err(device, "no data to load\n");
		return;
	}

	dev_info(device, "firmware ready: %d bytes\n", size);

	h = (struct fw_header *)data;
	reset_addr = loadfw_phys + h->reset_offset;

	memcpy(fwtag, h->md5sum, sz);
	fwtag[sz] = 0;

	dev_info(device, "magic     : %x\n", h->magic);
	dev_info(device, "version   : %x\n", h->version);
	dev_info(device, "chip      : %x\n", h->chip_id);
	dev_info(device, "memsize   : %u bytes\n", h->mem_size);
	dev_info(device, "reset off : %x\n", h->reset_offset);
	dev_info(device, "reset addr: %x\n", reset_addr);
	dev_info(device, "fwtag     : %s\n", fwtag);

	if (size > h->mem_size || h->mem_size + cfgsz > SZ_256K) {
		dev_err(device, "firmware too big\n");
		return;
	}

	bpmp_stop();
	bpmp_detach();

	/* TODO */
	memset(&bpmp_config, 0, cfgsz);

	memcpy(loadfw_virt, data, size);
	memset(loadfw_virt + size, 0, SZ_256K - size);
	memcpy(loadfw_virt + h->mem_size, &bpmp_config, cfgsz);

	bpmp_reset(reset_addr);
	r = bpmp_attach();
	if (r) {
		dev_err(device, "attach failed with error %d\n", r);
		return;
	}

	bpmp_get_fwtag();
	dev_info(device, "firmware load done\n");
}

static void bpmp_fwready(const struct firmware *fw, void *context)
{
	struct platform_device *pdev = context;
	struct device *device = &pdev->dev;

	if (!fw) {
		dev_err(device, "firmware not ready\n");
		return;
	}

	if (!loadfw_virt) {
		loadfw_virt = dma_alloc_coherent(device, SZ_256K,
				&loadfw_phys, GFP_KERNEL);
		dev_info(device, "loadfw_phys: 0x%llx\n", loadfw_phys);
		dev_info(device, "loadfw_virt: 0x%p\n", loadfw_virt);
		if (!loadfw_virt || !loadfw_phys) {
			dev_err(device, "out of memory\n");
			return;
		}
	}

	mutex_lock(&bpmp_lock);
	bpmp_cleanup_modules();
	bpmp_init_modules(pdev);
	bpmp_loadfw(device, fw->data, fw->size);
	mutex_unlock(&bpmp_lock);

	release_firmware(fw);
}

static int bpmp_reset_store(void *data, u64 val)
{
	struct platform_device *pdev = data;

	return request_firmware_nowait(THIS_MODULE, false, BPMP_FIRMWARE_NAME,
			&pdev->dev, GFP_KERNEL, pdev, bpmp_fwready);
}

DEFINE_SIMPLE_ATTRIBUTE(bpmp_reset_fops, NULL, bpmp_reset_store, "%lld\n");

static const struct fops_entry platdbg_attrs[] = {
	{ "reset", &bpmp_reset_fops, S_IWUSR },
	{ NULL, NULL, 0 }
};

int bpmp_platdbg_init(struct dentry *root, struct platform_device *pdev)
{
	return bpmp_create_attrs(platdbg_attrs, root, pdev);
}

static int bpmp_cpuidle_name_show(struct seq_file *file, void *data)
{
	struct bpmp_cpuidle_state *state = file->private;

	seq_printf(file, "%s\n", state->name);
	return 0;
}

static int bpmp_cpuidle_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, bpmp_cpuidle_name_show, inode->i_private);
}

static const struct file_operations cpuidle_name_fops = {
	.open = bpmp_cpuidle_name_open,
	.read = seq_read,
	.release = single_release
};

static int __bpmp_cpuidle_show(struct seq_file *file, bool show_time)
{
	struct bpmp_cpuidle_state *state = file->private;
	struct { uint64_t usage; uint64_t time; } __packed mb[3];
	uint32_t id = state->id;
	int ret;

	ret = tegra_bpmp_send_receive(MRQ_CPUIDLE_USAGE, &id, sizeof(id),
			&mb, sizeof(mb));
	if (ret) {
		seq_printf(file, "%d\n", ret);
		return ret;
	}

	if (show_time) {
		seq_printf(file, "%llu (%llu, %llu)\n",
				mb[0].time, mb[1].time, mb[2].time);
	} else {
		seq_printf(file, "%llu (%llu, %llu)\n",
				mb[0].usage, mb[1].usage, mb[2].usage);
	}

	return 0;
}

static int bpmp_cpuidle_usage_show(struct seq_file *file, void *data)
{
	return __bpmp_cpuidle_show(file, false);
}

static int bpmp_cpuidle_usage_open(struct inode *inode, struct file *file)
{
	return single_open(file, bpmp_cpuidle_usage_show, inode->i_private);
}

static const struct file_operations cpuidle_usage_fops = {
	.open = bpmp_cpuidle_usage_open,
	.read = seq_read,
	.release = single_release
};

static int bpmp_cpuidle_time_show(struct seq_file *file, void *data)
{
	return __bpmp_cpuidle_show(file, true);
}

static int bpmp_cpuidle_time_open(struct inode *inode, struct file *file)
{
	return single_open(file, bpmp_cpuidle_time_show, inode->i_private);
}

static const struct file_operations cpuidle_time_fops = {
	.open = bpmp_cpuidle_time_open,
	.read = seq_read,
	.release = single_release
};

static const struct fops_entry cpuidle_attrs[] = {
	{ "name", &cpuidle_name_fops, S_IRUGO },
	{ "usage", &cpuidle_usage_fops, S_IRUGO },
	{ "time", &cpuidle_time_fops, S_IRUGO },
	{ NULL, NULL, 0 }
};

static int bpmp_create_cpuidle_debug(int index, struct dentry *parent,
		struct bpmp_cpuidle_state *state)
{
	struct dentry *top;
	char name[16];

	sprintf(name, "state%d", index);
	top = debugfs_create_dir(name, parent);
	if (IS_ERR_OR_NULL(top))
		return -EFAULT;

	return bpmp_create_attrs(cpuidle_attrs, top, state);
}

int bpmp_init_cpuidle_debug(struct dentry *root)
{
	struct bpmp_cpuidle_state *state;
	struct dentry *d;
	unsigned int i;

	d = debugfs_create_dir("cpuidle", root);
	if (IS_ERR_OR_NULL(d))
		return -EFAULT;

	for (i = 0, state = plat_cpuidle_state; state->name; i++, state++) {
		if (bpmp_create_cpuidle_debug(i, d, state))
			return -EFAULT;
	}

	return 0;
}

#else

int bpmp_platdbg_init(struct dentry *root, struct platform_device *pdev)
{
	return -ENODEV;
}

int bpmp_init_cpuidle_debug(struct dentry *root)
{
	return -ENODEV;
}

#endif

int bpmp_linear_map_init(struct device *device)
{
	struct device_node *node;
	DEFINE_DMA_ATTRS(attrs);
	uint32_t of_start;
	uint32_t of_size;
	int ret;

	node = of_find_node_by_path("/bpmp");
	WARN_ON(!node);
	if (!node)
		return -ENODEV;

	ret = of_property_read_u32(node, "carveout-start", &of_start);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "carveout-size", &of_size);
	if (ret)
		return ret;

	dma_set_attr(DMA_ATTR_SKIP_IOVA_GAP, &attrs);
	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	ret = dma_map_linear_attrs(device, of_start, of_size, 0, &attrs);
	if (ret == DMA_ERROR_CODE)
		return -ENOMEM;

	return 0;
}

int bpmp_clk_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	cop_clk = clk_get_sys(NULL, "cop");
	if (IS_ERR(cop_clk)) {
		dev_err(dev, "cannot get cop clock\n");
		return -ENODEV;
	}

	sclk = clk_get_sys(NULL, "sclk");
	if (IS_ERR(sclk)) {
		dev_err(dev, "cannot get avp sclk\n");
		return -ENODEV;
	}

	emc_clk = clk_get_sys("tegra_emc", "emc");
	if (IS_ERR(emc_clk)) {
		dev_err(dev, "cannot get avp emc clk\n");
		return -ENODEV;
	}

	clk_prepare_enable(cop_clk);
	clk_prepare_enable(sclk);
	clk_prepare_enable(emc_clk);

	if (tegra21_pasr_init(&pdev->dev))
		dev_err(&pdev->dev, "PASR init failed\n");

	return 0;
}

