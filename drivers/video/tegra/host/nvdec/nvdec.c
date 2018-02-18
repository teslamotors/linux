/*
 * Tegra NVDEC Module Support
 *
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

#include <linux/slab.h>         /* for kzalloc */
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/nvhost.h>
#include <linux/pm_runtime.h>
#include <mach/clk.h>
#include <asm/byteorder.h>      /* for parsing ucode image wrt endianness */
#include <linux/delay.h>	/* for udelay */
#include <linux/scatterlist.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/tegra-soc.h>
#include <linux/version.h>

#include <linux/tegra_pm_domains.h>
#include <linux/nvhost_nvdec_ioctl.h>

#include <linux/platform/tegra/mc.h>

#include "dev.h"
#include "nvdec.h"
#include "nvhost_vm.h"
#include "hw_nvdec.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "nvhost_scale.h"
#include "chip_support.h"
#include "t124/t124.h"
#include "t210/t210.h"
#include "iomap.h"

#include <linux/ote_protocol.h>

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "t186/t186.h"
#endif

#define NVDEC_IDLE_TIMEOUT_DEFAULT	100000	/* 100 milliseconds */
#define NVDEC_IDLE_CHECK_PERIOD		10	/* 10 usec */
#define FW_NAME_SIZE			32

#define get_nvdec(ndev) ((struct nvdec **)(ndev)->dev.platform_data)
#define set_nvdec(ndev, f) ((ndev)->dev.platform_data = f)

static int nvhost_nvdec_init_sw(struct platform_device *dev);
static unsigned int tegra_nvdec_bootloader_enabled;

/* caller is responsible for freeing */
enum {
	host_nvdec_fw_bl = 0,
	host_nvdec_fw_ls
};

static char *nvdec_get_fw_name(struct platform_device *dev, int fw)
{
	char *fw_name;
	u8 maj, min;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	u32 debug_mode = host1x_readl(dev, nvdec_scp_ctl_stat_r()) &
					nvdec_scp_ctl_stat_debug_mode_m();

	/* note size here is a little over...*/
	fw_name = kzalloc(FW_NAME_SIZE, GFP_KERNEL);
	if (!fw_name)
		return NULL;

	decode_nvdec_ver(pdata->version, &maj, &min);
	if (tegra_nvdec_bootloader_enabled) {
		if (debug_mode) {
			if (fw == host_nvdec_fw_bl) {
				if (tegra_platform_is_qt() ||
					tegra_platform_is_linsim())
					snprintf(fw_name, FW_NAME_SIZE,
							"nvhost_nvdec_bl_no_wpr0%d%d.fw",
							maj, min);
				else
					snprintf(fw_name, FW_NAME_SIZE,
							"nvhost_nvdec_bl0%d%d.fw",
							maj, min);
			} else
				snprintf(fw_name, FW_NAME_SIZE,
						"nvhost_nvdec0%d%d.fw",
						maj, min);
		} else {
			if (fw == host_nvdec_fw_bl)
				if (tegra_platform_is_qt() ||
					tegra_platform_is_linsim()) {
					dev_info(&dev->dev,
						"Prod + No-WPR not allowed\n");
					kfree(fw_name);
					return NULL;
				} else
					snprintf(fw_name, FW_NAME_SIZE,
							"nvhost_nvdec_bl0%d%d_prod.fw",
							maj, min);
			else
				snprintf(fw_name, FW_NAME_SIZE,
						"nvhost_nvdec0%d%d_prod.fw",
						maj, min);
		}
	} else
		snprintf(fw_name, FW_NAME_SIZE, "nvhost_nvdec0%d%d_ns.fw", maj, min);

	dev_info(&dev->dev, "fw name:%s\n", fw_name);

	return fw_name;
}

static int nvdec_dma_wait_idle(struct platform_device *dev, u32 *timeout)
{
	nvhost_dbg_fn("");

	if (!*timeout)
		*timeout = NVDEC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, NVDEC_IDLE_CHECK_PERIOD, *timeout);
		u32 dmatrfcmd = host1x_readl(dev, nvdec_dmatrfcmd_r());
		u32 idle_v = nvdec_dmatrfcmd_idle_v(dmatrfcmd);

		if (nvdec_dmatrfcmd_idle_true_v() == idle_v) {
			nvhost_dbg_fn("done");
			return 0;
		}

		udelay(NVDEC_IDLE_CHECK_PERIOD);
		*timeout -= check;
	} while (*timeout || !tegra_platform_is_silicon());

	dev_err(&dev->dev, "dma idle timeout");

	return -1;
}

static int nvdec_dma_pa_to_internal_256b(struct platform_device *dev,
		u32 offset, u32 internal_offset, bool imem)
{
	struct nvhost_device_data *pdata = nvhost_get_devdata(dev);
	u32 cmd = nvdec_dmatrfcmd_size_256b_f();
	u32 pa_offset =  nvdec_dmatrffboffs_offs_f(offset);
	u32 i_offset = nvdec_dmatrfmoffs_offs_f(internal_offset);
	u32 timeout = 0; /* default*/

	if (imem)
		cmd |= nvdec_dmatrfcmd_imem_true_f();

	if (pdata->isolate_contexts)
		cmd |= nvdec_dmatrfcmd_dmactx_f(1);

	host1x_writel(dev, nvdec_dmatrfmoffs_r(), i_offset);
	host1x_writel(dev, nvdec_dmatrffboffs_r(), pa_offset);
	host1x_writel(dev, nvdec_dmatrfcmd_r(), cmd);

	return nvdec_dma_wait_idle(dev, &timeout);

}

static int nvdec_wait_idle(struct platform_device *dev, u32 *timeout)
{
	u32 w = 0;

	nvhost_dbg_fn("");

	if (!*timeout)
		*timeout = NVDEC_IDLE_TIMEOUT_DEFAULT;

	do {
		u32 check = min_t(u32, NVDEC_IDLE_CHECK_PERIOD, *timeout);
		w = host1x_readl(dev, nvdec_idlestate_r());

		if (!w) {
			nvhost_dbg_fn("done");
			return 0;
		}
		udelay(NVDEC_IDLE_CHECK_PERIOD);
		*timeout -= check;
	} while (*timeout || !tegra_platform_is_silicon());

	nvhost_err(&dev->dev, "nvdec_idlestate_r = %x\n", w);

	return -1;
}

static int nvdec_wait_mem_scrubbing(struct platform_device *dev)
{
	int retries = NVDEC_IDLE_TIMEOUT_DEFAULT / NVDEC_IDLE_CHECK_PERIOD;
	nvhost_dbg_fn("");

	do {
		u32 w = host1x_readl(dev, nvdec_dmactl_r()) &
			(nvdec_dmactl_dmem_scrubbing_m() |
			 nvdec_dmactl_imem_scrubbing_m());

		if (!w) {
			nvhost_dbg_fn("done");
			return 0;
		}
		udelay(NVDEC_IDLE_CHECK_PERIOD);
	} while (--retries || !tegra_platform_is_silicon());

	nvhost_err(&dev->dev, "Falcon mem scrubbing timeout");
	return -ETIMEDOUT;
}

int nvhost_nvdec_finalize_poweron(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	u32 timeout;
	u32 offset;
	int err = 0;
	struct nvdec **m;

	dev_dbg(&dev->dev, "nvdec_boot: start\n");
	err = nvhost_nvdec_init_sw(dev);
	if (err)
		return err;

	m = get_nvdec(dev);

	err = nvdec_wait_mem_scrubbing(dev);
	if (err)
		return err;

	/* load transcfg configuration if defined */
	if (pdata->transcfg_addr)
		host1x_writel(dev, pdata->transcfg_addr, pdata->transcfg_val);

	if (tegra_nvdec_bootloader_enabled) {

		u32 fb_data_offset = 0;
		u32 initial_dmem_offset = 0;
		struct nvdec_bl_shared_data shared_data;
		u32 debug = host1x_readl(dev,
					nvdec_scp_ctl_stat_r()) &
					nvdec_scp_ctl_stat_debug_mode_m();
		bool skip_wpr_settings = debug &&
			(tegra_platform_is_qt() || tegra_platform_is_linsim());


		fb_data_offset = (m[0]->os.bin_data_offset +
					m[0]->os.data_offset)/(sizeof(u32));

		shared_data.ls_fw_start_addr = m[1]->phys >> 8;
		shared_data.ls_fw_size = m[1]->size;

		/* no wpr firmware does not need these */
		if (!skip_wpr_settings) {
			struct mc_carveout_info inf;
			int ret;

			ret = mc_get_carveout_info(&inf, NULL,
						   MC_SECURITY_CARVEOUT1);
			if (ret)
				return -ENOMEM;

			/* Put the 40-bit addr formed by wpr_addr_hi and
			   wpr_addr_lo divided by 256 into 32-bit wpr_addr */
			shared_data.wpr_addr = inf.base >> 8;
			shared_data.wpr_size = inf.size; /* Already in bytes. */
		}
		memcpy(&(m[0]->mapped[fb_data_offset + initial_dmem_offset]),
			&shared_data, sizeof(shared_data));
	}
	host1x_writel(dev, nvdec_dmactl_r(), 0);
	host1x_writel(dev, nvdec_dmatrfbase_r(),
		(m[0]->phys + m[0]->os.bin_data_offset) >> 8);

	/* Write nvdec ucode data to dmem */
	dev_dbg(&dev->dev, "nvdec_boot: load dmem\n");
	for (offset = 0; offset < m[0]->os.data_size; offset += 256) {
		nvdec_dma_pa_to_internal_256b(dev,
					   m[0]->os.data_offset + offset,
					   offset, false);
	}
	/* Write nvdec ucode to imem */
	dev_dbg(&dev->dev, "nvdec_boot: load imem\n");
	nvdec_dma_pa_to_internal_256b(dev, m[0]->os.code_offset, 0, true);

	/* setup nvdec interrupts and enable interface */
	host1x_writel(dev, nvdec_irqmset_r(),
			(nvdec_irqmset_ext_f(0xff) |
				nvdec_irqmset_swgen1_set_f() |
				nvdec_irqmset_swgen0_set_f() |
				nvdec_irqmset_exterr_set_f() |
				nvdec_irqmset_halt_set_f()   |
				nvdec_irqmset_wdtmr_set_f()));
	host1x_writel(dev, nvdec_irqdest_r(),
			(nvdec_irqdest_host_ext_f(0xff) |
				nvdec_irqdest_host_swgen1_host_f() |
				nvdec_irqdest_host_swgen0_host_f() |
				nvdec_irqdest_host_exterr_host_f() |
				nvdec_irqdest_host_halt_host_f()));
	host1x_writel(dev, nvdec_itfen_r(),
			(nvdec_itfen_mthden_enable_f() |
				nvdec_itfen_ctxen_enable_f()));

	/* boot nvdec */
	dev_dbg(&dev->dev, "nvdec_boot: start falcon\n");
	host1x_writel(dev, nvdec_bootvec_r(), nvdec_bootvec_vec_f(0));
	host1x_writel(dev, nvdec_cpuctl_r(),
			nvdec_cpuctl_startcpu_true_f());

	timeout = 0; /* default */

	err = nvdec_wait_idle(dev, &timeout);
	if (err != 0) {
		dev_err(&dev->dev, "boot failed due to timeout");
		return err;
	}
	dev_dbg(&dev->dev, "nvdec_boot: success\n");

#if defined(CONFIG_TRUSTED_LITTLE_KERNEL) || defined(CONFIG_OTE_TRUSTY)
	if (te_is_secos_dev_enabled())
		te_restore_keyslots();
#endif

	return 0;
}

static int nvdec_setup_ucode_image(struct platform_device *dev,
		u32 *ucode_ptr,
		const struct firmware *ucode_fw,
		struct nvdec *m)
{
	/* image data is little endian. */
	struct nvdec_ucode_v1 ucode;
	int w;

	/* copy the whole thing taking into account endianness */
	for (w = 0; w < ucode_fw->size / sizeof(u32); w++)
		ucode_ptr[w] = le32_to_cpu(((u32 *)ucode_fw->data)[w]);

	ucode.bin_header = (struct nvdec_ucode_bin_header_v1 *)ucode_ptr;
	/* endian problems would show up right here */
	if (ucode.bin_header->bin_magic != 0x10de) {
		dev_err(&dev->dev,
			   "failed to get firmware magic");
		return -EINVAL;
	}
	if (ucode.bin_header->bin_ver != 1) {
		dev_err(&dev->dev,
			   "unsupported firmware version");
		return -ENOENT;
	}
	/* shouldn't be bigger than what firmware thinks */
	if (ucode.bin_header->bin_size > ucode_fw->size) {
		dev_err(&dev->dev,
			   "ucode image size inconsistency");
		return -EINVAL;
	}

	nvhost_dbg_info("ucode bin header: magic:0x%x ver:%d size:%d",
		ucode.bin_header->bin_magic,
		ucode.bin_header->bin_ver,
		ucode.bin_header->bin_size);
	nvhost_dbg_info("ucode bin header: os bin (header,data) offset size: 0x%x, 0x%x %d",
		ucode.bin_header->os_bin_header_offset,
		ucode.bin_header->os_bin_data_offset,
		ucode.bin_header->os_bin_size);
	ucode.os_header = (struct nvdec_ucode_os_header_v1 *)
		(((void *)ucode_ptr) + ucode.bin_header->os_bin_header_offset);

	nvhost_dbg_info("os ucode header: os code (offset,size): 0x%x, 0x%x",
		ucode.os_header->os_code_offset,
		ucode.os_header->os_code_size);
	nvhost_dbg_info("os ucode header: os data (offset,size): 0x%x, 0x%x",
		ucode.os_header->os_data_offset,
		ucode.os_header->os_data_size);
	nvhost_dbg_info("os ucode header: num apps: %d",
		ucode.os_header->num_apps);

	m->os.size = ucode.bin_header->os_bin_size;
	m->os.bin_data_offset = ucode.bin_header->os_bin_data_offset;
	m->os.code_offset = ucode.os_header->os_code_offset;
	m->os.data_offset = ucode.os_header->os_data_offset;
	m->os.data_size   = ucode.os_header->os_data_size;

	return 0;
}

static int nvdec_read_ucode(struct platform_device *dev, const char *fw_name,
			struct nvdec *m)
{
	const struct firmware *ucode_fw;
	int err;

	m->phys = 0;
	m->mapped = NULL;
	init_dma_attrs(&m->attrs);

	ucode_fw  = nvhost_client_request_firmware(dev, fw_name);
	if (!ucode_fw) {
		dev_err(&dev->dev, "failed to get nvdec firmware %s\n",
				fw_name);
		err = -ENOENT;
		return err;
	}

	m->size = ucode_fw->size;
	dma_set_attr(DMA_ATTR_READ_ONLY, &m->attrs);

	m->mapped = dma_alloc_attrs(&dev->dev,
			m->size, &m->phys,
			GFP_KERNEL, &m->attrs);
	if (!m->mapped) {
		dev_err(&dev->dev, "dma memory allocation failed");
		err = -ENOMEM;
		goto clean_up;
	}

	err = nvdec_setup_ucode_image(dev, m->mapped, ucode_fw, m);
	if (err) {
		dev_err(&dev->dev, "failed to parse firmware image %s\n",
				fw_name);
		goto clean_up;
	}

	m->valid = true;

	release_firmware(ucode_fw);

	return 0;

clean_up:
	if (m->mapped) {
		dma_free_attrs(&dev->dev,
				m->size, m->mapped,
				m->phys, &m->attrs);
		m->mapped = NULL;
	}
	release_firmware(ucode_fw);
	return err;
}

static int nvhost_nvdec_init_sw(struct platform_device *dev)
{
	int err = 0;
	struct nvdec **m = get_nvdec(dev);
	char **fw_name;
	int i;

	nvhost_dbg_fn("in dev:%p", dev);

	/* check if firmware resources already allocated */
	if (m)
		return 0;

	m = kzalloc(2*sizeof(struct nvdec *), GFP_KERNEL);
	if (!m) {
		dev_err(&dev->dev, "couldn't allocate ucode ptr");
		return -ENOMEM;
	}
	set_nvdec(dev, m);
	nvhost_dbg_fn("primed dev:%p", dev);

	fw_name = kzalloc(2*sizeof(char *), GFP_KERNEL);
	if (!fw_name) {
		dev_err(&dev->dev, "couldn't allocate firmware ptr");
		err = -ENOMEM;
		goto error;
	}

	fw_name[0] = nvdec_get_fw_name(dev, host_nvdec_fw_bl);
	if (!fw_name[0]) {
		dev_err(&dev->dev, "couldn't determine BL fw name");
		err = -EINVAL;
		goto error_fw_name;
	}
	if (tegra_nvdec_bootloader_enabled) {
		fw_name[1] = nvdec_get_fw_name(dev, host_nvdec_fw_ls);
		if (!fw_name[1]) {
			dev_err(&dev->dev, "couldn't determine LS fw name");
			err = -EINVAL;
			goto err_fw;
		}
	}

	for (i = 0; i < (1 + tegra_nvdec_bootloader_enabled); i++) {
		m[i] = kzalloc(sizeof(struct nvdec), GFP_KERNEL);
		if (!m[i]) {
			dev_err(&dev->dev, "couldn't alloc ucode");
			err = -ENOMEM;
			goto err_ucode;
		}

		err = nvdec_read_ucode(dev, fw_name[i], m[i]);
		kfree(fw_name[i]);
		fw_name[i] = NULL;
		if (err || !m[i]->valid) {
			dev_err(&dev->dev, "ucode not valid");
			goto err_ucode;
		}
	}
	kfree(fw_name);
	fw_name = NULL;

	return 0;

err_ucode:
	kfree(m[0]);
	kfree(m[1]);
	kfree(fw_name[1]);
err_fw:
	kfree(fw_name[0]);
error_fw_name:
	kfree(fw_name);
error:
	kfree(m);
	set_nvdec(dev, NULL);
	dev_err(&dev->dev, "failed");
	return err;
}

static struct of_device_id tegra_nvdec_of_match[] = {
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra210-nvdec",
		.data = (struct nvhost_device_data *)&t21_nvdec_info },
#endif
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = "nvidia,tegra186-nvdec",
		.data = (struct nvhost_device_data *)&t18_nvdec_info },
#endif
	{ },
};

static int nvdec_open(struct inode *inode, struct file *file)
{
	struct nvhost_device_data *pdata;
	struct nvdec_private *priv;

	pdata = container_of(inode->i_cdev,
		struct nvhost_device_data, ctrl_cdev);

	if (WARN_ONCE(pdata == NULL,
			"pdata not found, %s failed\n", __func__))
		return -ENODEV;

	if (WARN_ONCE(pdata->pdev == NULL,
			"device not found, %s failed\n", __func__))
		return -ENODEV;

	priv = kzalloc(sizeof(struct nvdec_private), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdata->pdev->dev,
			"couldn't allocate nvdec private");
		return -ENOMEM;
	}
	priv->pdev = pdata->pdev;
	atomic_set(&priv->refcnt, 0);

	file->private_data = priv;

	return 0;
}

static long nvdec_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct nvdec_private *priv = file->private_data;
	struct platform_device *pdev = priv->pdev;
	int err;

	if (WARN_ONCE(pdev == NULL, "pdata not found, %s failed\n", __func__))
		return -ENODEV;

	if (_IOC_TYPE(cmd) != NVHOST_NVDEC_IOCTL_MAGIC)
		return -EFAULT;

	switch (cmd) {
	case NVHOST_NVDEC_IOCTL_POWERON:
		err = nvhost_module_busy(pdev);
		if (err)
			return err;

		atomic_inc(&priv->refcnt);
	break;
	case NVHOST_NVDEC_IOCTL_POWEROFF:
		if (atomic_dec_if_positive(&priv->refcnt) >= 0)
			nvhost_module_idle(pdev);
	break;
	default:
		dev_err(&pdev->dev,
			"%s: Unknown nvdec ioctl.\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int nvdec_release(struct inode *inode, struct file *file)
{
	struct nvdec_private *priv = file->private_data;

	nvhost_module_idle_mult(priv->pdev, atomic_read(&priv->refcnt));
	kfree(priv);

	return 0;
}
static int nvdec_probe(struct platform_device *dev)
{
	int err = 0;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_nvdec_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	err = nvhost_check_bondout(pdata->bond_out_id);
	if (err) {
		dev_err(&dev->dev, "No NVDEC unit present. err:%d", err);
		return err;
	}

	pdata->pdev = dev;

	mutex_init(&pdata->lock);

	platform_set_drvdata(dev, pdata);

	err = nvhost_client_device_get_resources(dev);
	if (err)
		return err;

	dev->dev.platform_data = NULL;

	/* get the module clocks to sane state */
	nvhost_module_init(dev);

#ifdef CONFIG_PM_GENERIC_DOMAINS
	/* add module power domain and also add its domain
	 * as sub-domain of host1x domain */
	err = nvhost_module_add_domain(&pdata->pd, dev);
#endif

	err = nvhost_client_device_init(dev);

	return 0;
}

static int __exit nvdec_remove(struct platform_device *dev)
{
	nvhost_client_device_release(dev);
	return 0;
}

static struct platform_driver nvdec_driver = {
	.probe = nvdec_probe,
	.remove = __exit_p(nvdec_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "nvdec",
#ifdef CONFIG_OF
		.of_match_table = tegra_nvdec_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &nvhost_module_pm_ops,
#endif
	}
};

const struct file_operations tegra_nvdec_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = nvdec_open,
	.unlocked_ioctl = nvdec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = nvdec_ioctl,
#endif
	.release = nvdec_release,
};

static struct of_device_id tegra_nvdec_domain_match[] = {
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra210-nvdec-pd",
	.data = (struct nvhost_device_data *)&t21_nvdec_info},
#endif
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = "nvidia,tegra186-nvdec-pd",
	.data = (struct nvhost_device_data *)&t18_nvdec_info},
#endif
	{},
};

static int __init tegra_nvdec_bootloader_enabled_arg(char *options)
{
	char *p = options;

	tegra_nvdec_bootloader_enabled = memparse(p, &p);

	return 0;
}
early_param("nvdec_enabled", tegra_nvdec_bootloader_enabled_arg);

static int __init nvdec_init(void)
{
	int ret;

	/* Below kernel config check is for T210 */
	if (IS_ENABLED(CONFIG_NVDEC_BOOTLOADER))
		tegra_nvdec_bootloader_enabled = 1;

	ret = nvhost_domain_init(tegra_nvdec_domain_match);
	if (ret)
		return ret;

	return platform_driver_register(&nvdec_driver);
}

static void __exit nvdec_exit(void)
{
	platform_driver_unregister(&nvdec_driver);
}

module_init(nvdec_init);
module_exit(nvdec_exit);
