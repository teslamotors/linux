/*
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <mach/iomap.h>
#include <mach/clk.h>

#include <media/tegra_itu656.h>

#define TEGRA_ITU656_NAME "tegra_itu656"

#define TEGRA_VI_CLK_RST_CTRL_OFFSET 0x148
#define TEGRA_DC_DISP_PP_SELECT_OFFSET 0x42c

DEFINE_MUTEX(tegra_itu656_lock);

static struct clk *vi_clk;
static struct clk *csus_clk;

struct tegra_itu656_block {
	int (*enable) (void);
	int (*disable) (void);
	bool is_enabled;
};


static int tegra_itu656_enable_vi(void)
{
	int retval;

	retval = clk_enable(vi_clk);
	if (!retval) {
		return retval;
	}

	retval = clk_enable(csus_clk);

	return retval;
}

static int tegra_itu656_disable_vi(void)
{
	clk_disable(vi_clk);
	clk_disable(csus_clk);

	return 0;
}

struct tegra_itu656_block tegra_itu656_block[] = {
	[TEGRA_ITU656_MODULE_VI] = {tegra_itu656_enable_vi,
		tegra_itu656_disable_vi, true},
};

static int tegra_itu656_clk_set(void)
{
	u32 val;
	void __iomem *car = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
	void __iomem *apb_misc = IO_ADDRESS(TEGRA_APB_MISC_BASE);

	writel(0x2, car + TEGRA_VI_CLK_RST_CTRL_OFFSET);

	val = readl(apb_misc + TEGRA_DC_DISP_PP_SELECT_OFFSET);
	writel(val | 0x1, apb_misc + TEGRA_DC_DISP_PP_SELECT_OFFSET);

	return 0;
}

static int tegra_itu656_reset(void)
{
	tegra_periph_reset_assert(vi_clk);
	udelay(10);
	tegra_periph_reset_deassert(vi_clk);

	return 0;
}

static long tegra_itu656_ioctl(struct file *file,
			       unsigned int cmd, unsigned long arg)
{
	uint id;
	int retval;

	/* first element of arg must be u32 with id of module to talk to */
	if (copy_from_user(&id, (const void __user *)arg, sizeof(uint))) {
		pr_err("%s: Failed to copy arg from user", __func__);
		return -EFAULT;
	}

	if (id >= ARRAY_SIZE(tegra_itu656_block)) {
		pr_err("%s: Invalid id to tegra isp ioctl%d\n", __func__, id);
		return -EINVAL;
	}

	switch (cmd) {
	case TEGRA_ITU656_IOCTL_ENABLE:
	{
		if (!tegra_itu656_block[id].is_enabled) {
			retval = tegra_itu656_enable_vi ();
			if (retval) {
				return retval;
			}
			tegra_itu656_block[id].is_enabled = true;
		}
		return 0;
	}
	case TEGRA_ITU656_IOCTL_DISABLE:
	{
		if (tegra_itu656_block[id].is_enabled) {
			tegra_itu656_disable_vi();
			tegra_itu656_block[id].is_enabled = false;
		}
		return 0;
	}
	case TEGRA_ITU656_IOCTL_CLK_SET:
	{
		return tegra_itu656_clk_set();
	}
	case TEGRA_ITU656_IOCTL_RESET:
	{
		return tegra_itu656_reset();
	}
	default:
		pr_err("%s: Unknown tegra_itu656 ioctl.\n", TEGRA_ITU656_NAME);
		return -EINVAL;
	}
	return 0;
}

static int tegra_itu656_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations tegra_itu656_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tegra_itu656_ioctl,
	.release = tegra_itu656_release,
};

static struct miscdevice tegra_itu656_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TEGRA_ITU656_NAME,
	.fops = &tegra_itu656_fops,
};

static int tegra_itu656_clk_get(struct platform_device *pdev, const char *name,
				struct clk **clk)
{
	*clk = clk_get(&pdev->dev, name);
	if (IS_ERR_OR_NULL(*clk)) {
		pr_err("%s: unable to get clock for %s\n", __func__, name);
		*clk = NULL;
		return PTR_ERR(*clk);
	}
	return 0;
}

static int tegra_itu656_probe(struct platform_device *pdev)
{
	int err;

	pr_info("%s: probe\n", TEGRA_ITU656_NAME);

	err = misc_register(&tegra_itu656_device);
	if (err) {
		pr_err("%s: Unable to register misc device!\n",
		       TEGRA_ITU656_NAME);
		return err;
	}

	err = tegra_itu656_clk_get(pdev, "vi", &vi_clk);
	if (err) {
		return err;
	}

	err = tegra_itu656_clk_get(pdev, "csus", &csus_clk);
	if (err) {
		clk_put(vi_clk);
		return err;
	}

	return 0;
}

static int tegra_itu656_remove(struct platform_device *pdev)
{
	misc_deregister(&tegra_itu656_device);
	return 0;
}

static struct platform_driver tegra_itu656_driver = {
	.probe = tegra_itu656_probe,
	.remove = tegra_itu656_remove,
	.driver = { .name = TEGRA_ITU656_NAME }
};

static int __init tegra_itu656_init(void)
{
	return platform_driver_register(&tegra_itu656_driver);
}

static void __exit tegra_itu656_exit(void)
{
	platform_driver_unregister(&tegra_itu656_driver);
}

module_init(tegra_itu656_init);
module_exit(tegra_itu656_exit);

