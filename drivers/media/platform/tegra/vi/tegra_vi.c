/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/nvhost.h>
#include <linux/nvhost_vi_ioctl.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>

#include <mach/clk.h>
#include <linux/platform/tegra/latency_allowance.h>

#include "bus_client.h"
#include "nvhost_acm.h"
#include "chip_support.h"
#include "host1x/host1x.h"
#include "vi.h"
#include "vi_irq.h"

#define T12_VI_CFG_CG_CTRL	0xb8
#define T12_CG_2ND_LEVEL_EN	1
#define T12_VI_CSI_0_SW_RESET	0x100
#define T12_CSI_CSI_SW_SENSOR_A_RESET 0x858
#define T12_CSI_CSICIL_SW_SENSOR_A_RESET 0x94c
#define T12_VI_CSI_0_CSI_IMAGE_DT 0x120

#define T12_VI_CSI_1_SW_RESET	0x200
#define T12_CSI_CSI_SW_SENSOR_B_RESET 0x88c
#define T12_CSI_CSICIL_SW_SENSOR_B_RESET 0x980
#define T12_VI_CSI_1_CSI_IMAGE_DT 0x220

#define T21_CSI_CILA_PAD_CONFIG0 0x92c
#define T21_CSI1_CILA_PAD_CONFIG0 0x112c
#define T21_CSI2_CILA_PAD_CONFIG0 0x192c

#define VI_MAX_BPP 2

int nvhost_vi_finalize_poweron(struct platform_device *dev)
{
	struct vi *tegra_vi = nvhost_get_private_data(dev);
	int ret = 0;

	if (!tegra_vi)
		return -EINVAL;

	if (tegra_vi->reg) {
		ret = regulator_enable(tegra_vi->reg);
		if (ret) {
			dev_err(&dev->dev,
					"%s: enable csi regulator failed.\n",
					__func__);
			return ret;
		}
	}

#ifdef CONFIG_ARCH_TEGRA_12x_SOC
	/* Only do this for vi.0 not for slave device vi.1 */
	if (dev->id == 0)
		host1x_writel(dev, T12_VI_CFG_CG_CTRL, T12_CG_2ND_LEVEL_EN);
#endif

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
	{
		void __iomem *reset_reg[3];
		struct nvhost_device_data *pdata = dev->dev.platform_data;

		reset_reg[0] = pdata->aperture[0] +
			       T21_CSI_CILA_PAD_CONFIG0;
		reset_reg[1] = pdata->aperture[0] +
			       T21_CSI1_CILA_PAD_CONFIG0;
		reset_reg[2] = pdata->aperture[0] +
			       T21_CSI2_CILA_PAD_CONFIG0;

		writel(readl(reset_reg[0]) & 0xfffcffff, reset_reg[0]);
		writel(readl(reset_reg[1]) & 0xfffcffff, reset_reg[1]);
		writel(readl(reset_reg[2]) & 0xfffcffff, reset_reg[2]);
	}
#endif

	ret = vi_enable_irq(tegra_vi);
	if (ret)
		dev_err(&tegra_vi->ndev->dev, "%s: vi_enable_irq failed\n",
			__func__);

	return ret;
}

int nvhost_vi_prepare_poweroff(struct platform_device *dev)
{
	int ret = 0;
	struct vi *tegra_vi = nvhost_get_private_data(dev);

	if (!tegra_vi)
		return -EINVAL;

	ret = vi_disable_irq(tegra_vi);
	if (ret) {
		dev_err(&tegra_vi->ndev->dev, "%s: vi_disable_irq failed\n",
			__func__);
		return ret;
	}

	if (tegra_vi->reg) {
		ret = regulator_disable(tegra_vi->reg);
		if (ret)
			dev_err(&dev->dev,
				"%s: disable csi regulator failed.\n",
				__func__);
	}

	return ret;
}

#if defined(CONFIG_TEGRA_ISOMGR)
static int vi_isomgr_register(struct vi *tegra_vi)
{
	int iso_client_id = TEGRA_ISO_CLIENT_VI_0;
	struct clk *vi_clk;
	struct nvhost_device_data *pdata =
				platform_get_drvdata(tegra_vi->ndev);

	dev_dbg(&tegra_vi->ndev->dev, "%s++\n", __func__);

	if (WARN_ONCE(pdata == NULL, "pdata not found, %s failed\n", __func__))
		return -ENODEV;

	/* Get max VI BW */
	vi_clk = pdata->clk[0];
	tegra_vi->max_bw =
			(clk_round_rate(vi_clk, UINT_MAX) / 1000) * VI_MAX_BPP;

	/* Register with max possible BW in VI usecases.*/
	tegra_vi->isomgr_handle = tegra_isomgr_register(iso_client_id,
					tegra_vi->max_bw,
					NULL,	/* tegra_isomgr_renegotiate */
					NULL);	/* *priv */

	if (!tegra_vi->isomgr_handle) {
		dev_err(&tegra_vi->ndev->dev, "%s: unable to register isomgr\n",
					__func__);
		return -ENOMEM;
	}

	return 0;
}

static int vi_set_isomgr_request(struct vi *tegra_vi, uint vi_bw, uint lt)
{
	int ret = 0;

	dev_dbg(&tegra_vi->ndev->dev,
		"%s++ bw=%u, lt=%u\n", __func__, vi_bw, lt);

	/* return value of tegra_isomgr_reserve is dvfs latency in usec */
	ret = tegra_isomgr_reserve(tegra_vi->isomgr_handle,
				vi_bw,	/* KB/sec */
				lt);	/* usec */
	if (!ret) {
		dev_err(&tegra_vi->ndev->dev,
		"%s: failed to reserve %u KBps\n", __func__, vi_bw);
		return -ENOMEM;
	}

	/* return value of tegra_isomgr_realize is dvfs latency in usec */
	ret = tegra_isomgr_realize(tegra_vi->isomgr_handle);
	if (ret)
		dev_dbg(&tegra_vi->ndev->dev,
		"%s: tegra_vi isomgr latency is %d usec",
		__func__, ret);
	else {
		dev_err(&tegra_vi->ndev->dev,
		"%s: failed to realize %u KBps\n", __func__, vi_bw);
			return -ENOMEM;
	}
	return 0;
}

static int vi_isomgr_release(struct vi *tegra_vi)
{
	int ret = 0;

	dev_dbg(&tegra_vi->ndev->dev, "%s++\n", __func__);

	/* deallocate isomgr bw */
	ret = vi_set_isomgr_request(tegra_vi, 0, 0);
	if (ret) {
		dev_err(&tegra_vi->ndev->dev,
		"%s: failed to deallocate memory in isomgr\n",
		__func__);
		return -ENOMEM;
	}

	return 0;
}
#endif

static int vi_set_la(u32 vi_bw)
{
	int ret = 0;

#ifdef CONFIG_TEGRA_MC
	ret = tegra_set_camera_ptsa(TEGRA_LA_VI_W, vi_bw, 1);

	if (!ret) {
		ret = tegra_set_latency_allowance(TEGRA_LA_VI_W,
			vi_bw);

		if (ret)
			pr_err("%s: set latency failed: %d\n",
				__func__, ret);
	} else {
		pr_err("%s: set ptsa failed: %d\n", __func__, ret);
	}
#endif

	return ret;
}

static long vi_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct vi *tegra_vi = file->private_data;

	if (_IOC_TYPE(cmd) != NVHOST_VI_IOCTL_MAGIC)
		return -EFAULT;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVHOST_VI_IOCTL_ENABLE_TPG): {
		uint enable;
		int ret;
		struct clk *clk;

		if (copy_from_user(&enable,
			(const void __user *)arg, sizeof(uint))) {
			dev_err(&tegra_vi->ndev->dev,
				"%s: Failed to copy arg from user\n", __func__);
			return -EFAULT;
		}

		clk = clk_get(&tegra_vi->ndev->dev, "pll_d");
		if (IS_ERR(clk))
			return -EINVAL;

		if (enable)
			ret = tegra_clk_cfg_ex(clk,
				TEGRA_CLK_PLLD_CSI_OUT_ENB, 1);
		else
			ret = tegra_clk_cfg_ex(clk,
				TEGRA_CLK_MIPI_CSI_OUT_ENB, 1);
		clk_put(clk);

		return ret;
	}
	case _IOC_NR(NVHOST_VI_IOCTL_GET_VI_CLK): {
		int ret;
		u64 vi_clk_rate = 0;

		ret = nvhost_module_get_rate(tegra_vi->ndev,
			(unsigned long *)&vi_clk_rate, 0);
		if (ret) {
			dev_err(&tegra_vi->ndev->dev,
			"%s: failed to get vi clk\n",
			__func__);
			return ret;
		}

		if (copy_to_user((void __user *)arg,
			&vi_clk_rate, sizeof(vi_clk_rate))) {
			dev_err(&tegra_vi->ndev->dev,
			"%s:Failed to copy vi clk rate to user\n",
			__func__);
			return -EFAULT;
		}

		return 0;
	}
	case _IOC_NR(NVHOST_VI_IOCTL_SET_VI_LA_BW): {
		u32 ret = 0;
		u32 vi_la_bw;

		if (copy_from_user(&vi_la_bw,
			(const void __user *)arg,
				sizeof(vi_la_bw))) {
			dev_err(&tegra_vi->ndev->dev,
				"%s: Failed to copy arg from user\n", __func__);
			return -EFAULT;
			}

		/* Set latency allowance for VI, BW is in MBps */
		ret = vi_set_la(vi_la_bw);
		if (ret) {
			dev_err(&tegra_vi->ndev->dev,
			"%s: failed to set la vi_bw %u MBps\n",
			__func__, vi_la_bw);
			return -ENOMEM;
		}

		return 0;
	}
	case _IOC_NR(NVHOST_VI_IOCTL_SET_EMC_INFO): {
		uint vi_bw;
		int ret;

		if (copy_from_user(&vi_bw,
			(const void __user *)arg, sizeof(uint))) {
			dev_err(&tegra_vi->ndev->dev,
				"%s: Failed to copy arg from user\n", __func__);
			return -EFAULT;
		}

		ret = vi_set_la(vi_bw/1000);
		if (ret) {
			dev_err(&tegra_vi->ndev->dev,
			"%s: failed to set la for vi_bw %u MBps\n",
			__func__, vi_bw/1000);
			return -ENOMEM;
		}

#if defined(CONFIG_TEGRA_ISOMGR)
		/*
		 * Register VI as isomgr client.
		 */
		if (!tegra_vi->isomgr_handle) {
			ret = vi_isomgr_register(tegra_vi);
			if (ret) {
				dev_err(&tegra_vi->ndev->dev,
				"%s: failed to register VI as isomgr client\n",
				__func__);
				return -ENOMEM;
			}
		}

		if (vi_bw > tegra_vi->max_bw) {
			dev_err(&tegra_vi->ndev->dev,
			"%s: Requested ISO BW %u is more than "
			"VI's max BW %u possible\n",
			__func__, vi_bw, tegra_vi->max_bw);
			return -EINVAL;
		}

		/*
		 * Set VI ISO BW requirements.
		 * There is no way to figure out what latency
		 * can be tolerated in VI without reading VI
		 * registers for now. 3 usec is minimum time
		 * to switch PLL source. Let's put 4 usec as
		 * latency for now.
		 */
		ret = vi_set_isomgr_request(tegra_vi, vi_bw, 4);
		if (ret) {
			dev_err(&tegra_vi->ndev->dev,
			"%s: failed to reserve %u KBps\n",
			__func__, vi_bw);
			return -ENOMEM;
		}
#endif
		return ret;
	}
	case _IOC_NR(NVHOST_VI_IOCTL_SET_VI_CLK): {
		long vi_clk_rate = 0;

		if (copy_from_user(&vi_clk_rate,
			(const void __user *)arg, sizeof(long))) {
			dev_err(&tegra_vi->ndev->dev,
				"%s: Failed to copy arg from user\n", __func__);
			return -EFAULT;
		}

		return nvhost_module_set_rate(tegra_vi->ndev,
				tegra_vi, vi_clk_rate, 0, NVHOST_CLOCK);
	}
	default:
		dev_err(&tegra_vi->ndev->dev,
			"%s: Unknown vi ioctl.\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vi_open(struct inode *inode, struct file *file)
{
	struct nvhost_device_data *pdata;
	struct vi *vi;
	int err = 0;

	pdata = container_of(inode->i_cdev,
		struct nvhost_device_data, ctrl_cdev);
	if (WARN_ONCE(pdata == NULL, "pdata not found, %s failed\n", __func__))
		return -ENODEV;

	vi = (struct vi *)pdata->private_data;
	if (WARN_ONCE(vi == NULL, "vi not found, %s failed\n", __func__))
		return -ENODEV;

	file->private_data = vi;

	/* add vi client to acm */
	if (nvhost_module_add_client(vi->ndev, vi)) {
		dev_err(&vi->ndev->dev,
			"%s: failed add vi client\n",
			__func__);
		return -ENOMEM;
	}

	return err;
}

static int vi_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct vi *tegra_vi = file->private_data;

#if defined(CONFIG_TEGRA_ISOMGR)
	/* nullify isomgr request */
	if (tegra_vi->isomgr_handle) {
		ret = vi_isomgr_release(tegra_vi);
		if (ret) {
			dev_err(&tegra_vi->ndev->dev,
			"%s: failed to deallocate memory in isomgr\n",
			__func__);
			return -ENOMEM;
		}
	}
#endif

	/* remove vi client from acm */
	nvhost_module_remove_client(tegra_vi->ndev, tegra_vi);

	return ret;
}

const struct file_operations tegra_vi_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = vi_open,
	.unlocked_ioctl = vi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vi_ioctl,
#endif
	.release = vi_release,
};

/* Reset sensor data if respective clk is ON */
void nvhost_vi_reset_all(struct platform_device *pdev)
{
	void __iomem *reset_reg[4];
	int err;
	bool enabled = false;
	struct nvhost_device_data *pdata = pdev->dev.platform_data;
	struct clk *clk;

	err = nvhost_clk_get(pdev, "cilab", &clk);
	if (!err && tegra_is_clk_enabled(clk)) {
		reset_reg[0] = pdata->aperture[0] +
			T12_VI_CSI_0_SW_RESET;
		reset_reg[1] = pdata->aperture[0] +
			T12_CSI_CSI_SW_SENSOR_A_RESET;
		reset_reg[2] = pdata->aperture[0] +
			T12_CSI_CSICIL_SW_SENSOR_A_RESET;
		reset_reg[3] = pdata->aperture[0] +
			T12_VI_CSI_0_CSI_IMAGE_DT;

		writel(0, reset_reg[3]);
		writel(0x1, reset_reg[2]);
		writel(0x1, reset_reg[1]);
		writel(0x1f, reset_reg[0]);

		udelay(10);

		writel(0, reset_reg[2]);
		writel(0, reset_reg[1]);
	}

	err = nvhost_clk_get(pdev, "cilcd", &clk);
	if (!err && tegra_is_clk_enabled(clk))
		enabled = true;

	err = nvhost_clk_get(pdev, "cile", &clk);
	if (!err && tegra_is_clk_enabled(clk))
		enabled = true;

	if (enabled) {
		reset_reg[0] = pdata->aperture[0] +
			T12_VI_CSI_1_SW_RESET;
		reset_reg[1] = pdata->aperture[0] +
			T12_CSI_CSI_SW_SENSOR_B_RESET;
		reset_reg[2] = pdata->aperture[0] +
			T12_CSI_CSICIL_SW_SENSOR_B_RESET;
		reset_reg[3] = pdata->aperture[0] +
			T12_VI_CSI_1_CSI_IMAGE_DT;

		writel(0, reset_reg[3]);
		writel(0x1, reset_reg[2]);
		writel(0x1, reset_reg[1]);
		writel(0x1f, reset_reg[0]);

		udelay(10);

		writel(0, reset_reg[2]);
		writel(0, reset_reg[1]);
	}
}
