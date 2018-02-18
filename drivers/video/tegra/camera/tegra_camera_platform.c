/*
 * drivers/video/tegra/camera/tegra_camera_platform.c
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/platform/tegra/mc.h>
#include <linux/of.h>

#include "vi.h"
#include "tegra_camera_dev_mfi.h"
#include "tegra_camera_platform.h"

#define CAMDEV_NAME "tegra_camera_ctrl"

static const struct of_device_id tegra_camera_of_ids[] = {
	{ .compatible = "nvidia, tegra-camera-platform" },
	{ },
};

static struct miscdevice tegra_camera_misc;

static int tegra_camera_isomgr_register(struct tegra_camera_info *info)
{
#if defined(CONFIG_TEGRA_ISOMGR)
	int ret = 0;
	u32 num_csi_lanes = 0;
	u32 max_lane_speed = 0;
	u32 bits_per_pixel = 0;
	u32 vi_bpp = 0;
	u64 vi_iso_bw = 0;
	u32 vi_margin_pct = 0;
	u32 max_pixel_rate = 0;
	u32 isp_bpp = 0;
	u64 isp_iso_bw = 0;
	u32 isp_margin_pct = 0;
	struct device_node *np = info->dev->of_node;

	dev_dbg(info->dev, "%s++\n", __func__);

	ret |= of_property_read_u32(np, "num_csi_lanes", &num_csi_lanes);
	ret |= of_property_read_u32(np, "max_lane_speed", &max_lane_speed);
	ret |= of_property_read_u32(np, "min_bits_per_pixel", &bits_per_pixel);
	ret |= of_property_read_u32(np, "vi_peak_byte_per_pixel", &vi_bpp);
	ret |= of_property_read_u32(np, "vi_bw_margin_pct", &vi_margin_pct);
	ret |= of_property_read_u32(np, "max_pixel_rate", &max_pixel_rate);
	ret |= of_property_read_u32(np, "isp_peak_byte_per_pixel", &isp_bpp);
	ret |= of_property_read_u32(np, "isp_bw_margin_pct", &isp_margin_pct);

	if (ret)
		dev_info(info->dev, "%s: some fields not in DT.\n", __func__);

	/*
	 * Use per-camera specifics to calculate ISO BW needed,
	 * which is smaller than the per-asic max.
	 *
	 * The formula for VI ISO BW is based on total number
	 * of active csi lanes when all cameras on the camera
	 * board are active.
	 *
	 * The formula for ISP ISO BW is based on max number
	 * of ISP's used in ISO mode given number of camera(s)
	 * on the camera board and the number of ISP's on the ASIC.
	 *
	 * The final ISO BW is based on the max of the two.
	 */
	vi_iso_bw = ((num_csi_lanes * max_lane_speed) / bits_per_pixel)
				* vi_bpp * (100 + vi_margin_pct) / 100;
	isp_iso_bw = max_pixel_rate * isp_bpp * (100 + isp_margin_pct) / 100;
	if (vi_iso_bw > isp_iso_bw)
		info->max_bw = vi_iso_bw;
	else
		info->max_bw = isp_iso_bw;

	if (!info->max_bw) {
		dev_err(info->dev, "%s: BW must be non-zero\n", __func__);
		return -EINVAL;
	}

	dev_info(info->dev, "%s isp_iso_bw=%llu, vi_iso_bw=%llu, max_bw=%llu\n",
				__func__, isp_iso_bw, vi_iso_bw, info->max_bw);

	/* Register with max possible BW for CAMERA usecases.*/
	info->isomgr_handle = tegra_isomgr_register(
					TEGRA_ISO_CLIENT_TEGRA_CAMERA,
					info->max_bw,
					NULL,	/* tegra_isomgr_renegotiate */
					NULL);	/* *priv */

	if (IS_ERR(info->isomgr_handle)) {
		dev_err(info->dev,
			"%s: unable to register to isomgr\n",
				__func__);
		return -ENOMEM;
	}
#endif

	return 0;
}

static int tegra_camera_isomgr_unregister(struct tegra_camera_info *info)
{
#if defined(CONFIG_TEGRA_ISOMGR)
	tegra_isomgr_unregister(info->isomgr_handle);
	info->isomgr_handle = NULL;
#endif

	return 0;
}

static int tegra_camera_isomgr_request(
		struct tegra_camera_info *info, uint iso_bw, uint lt)
{
#if defined(CONFIG_TEGRA_ISOMGR)
	int ret = 0;

	dev_dbg(info->dev,
		"%s++ bw=%u, lt=%u\n", __func__, iso_bw, lt);

	if (!info->isomgr_handle) {
		dev_err(info->dev,
		"%s: isomgr_handle is NULL\n",
		__func__);
		return -EINVAL;
	}

	/* return value of tegra_isomgr_reserve is dvfs latency in usec */
	ret = tegra_isomgr_reserve(info->isomgr_handle,
				iso_bw,	/* KB/sec */
				lt);	/* usec */
	if (!ret) {
		dev_err(info->dev,
		"%s: failed to reserve %u KBps\n", __func__, iso_bw);
		return -ENOMEM;
	}

	/* return value of tegra_isomgr_realize is dvfs latency in usec */
	ret = tegra_isomgr_realize(info->isomgr_handle);
	if (ret)
		dev_dbg(info->dev,
		"%s: tegra_camera isomgr latency is %d usec",
		__func__, ret);
	else {
		dev_err(info->dev,
		"%s: failed to realize %u KBps\n", __func__, iso_bw);
			return -ENOMEM;
	}
#endif

	return 0;
}

static int tegra_camera_isomgr_release(struct tegra_camera_info *info)
{
#if defined(CONFIG_TEGRA_ISOMGR)
	int ret = 0;
	dev_dbg(info->dev, "%s++\n", __func__);

	/* deallocate isomgr bw */
	ret = tegra_camera_isomgr_request(info, 0, 4);
	if (ret) {
		dev_err(info->dev,
		"%s: failed to deallocate memory in isomgr\n",
		__func__);
		return -ENOMEM;
	}
#endif

	return 0;
}

static int tegra_camera_open(struct inode *inode, struct file *file)
{
	struct tegra_camera_info *info;
	struct miscdevice *mdev;
#if !defined(CONFIG_TEGRA_BWMGR)
	int i, ret, index_put, index_disable;
#endif

	mdev = file->private_data;
	info = dev_get_drvdata(mdev->parent);
	file->private_data = info;
#if defined(CONFIG_TEGRA_BWMGR)
	/* get bandwidth manager handle if needed */
	info->bwmgr_handle =
		tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_CAMERA);

	/* set the initial rate */
	if (IS_ERR_OR_NULL(info->bwmgr_handle)) {
		info->bwmgr_handle = NULL;
		return -ENODEV;
	}
	tegra_bwmgr_set_emc(info->bwmgr_handle, 0,
			TEGRA_BWMGR_SET_EMC_SHARED_BW);
	return 0;
#else
	for (i = 0; i < NUM_CLKS; i++) {
		info->clks[i] = clk_get_sys(info->devname, clk_names[i]);
		if (IS_ERR(info->clks[i])) {
			dev_err(info->dev, "clk_get_sys failed for %s:%s\n",
				info->devname, clk_names[i]);
			index_put = i-1;
			goto err_get_clk;
		}

		ret = clk_set_rate(info->clks[i], 0);
		if (ret) {
			dev_err(info->dev, "Cannot init %s\n", clk_names[i]);
			index_put = i;
			goto err_set_clk;
		}

		ret = clk_prepare_enable(info->clks[i]);
		if (ret) {
			dev_err(info->dev, "Cannot enable %s\n", clk_names[i]);
			index_put = i;
			index_disable = i-1;
			goto err_prep_clk;
		}
	}
	return 0;

err_prep_clk:
	for (i = index_disable; i >= 0; i--) {
		if (!IS_ERR_OR_NULL(info->clks[i]))
			clk_disable_unprepare(info->clks[i]);
	}

err_set_clk:
err_get_clk:
	for (i = index_put; i >= 0; i--) {
		if (!IS_ERR_OR_NULL(info->clks[i]))
			clk_put(info->clks[i]);
		info->clks[i] = NULL;
	}
	return -ENOENT;
#endif
}

static int tegra_camera_release(struct inode *inode, struct file *file)
{

	struct tegra_camera_info *info;
#if !defined(CONFIG_TEGRA_BWMGR)
	int i;
#endif
	int ret;

	info = file->private_data;
#if defined(CONFIG_TEGRA_BWMGR)
	tegra_bwmgr_unregister(info->bwmgr_handle);
#else
	for (i = 0; i < NUM_CLKS; i++) {
		if (!IS_ERR_OR_NULL(info->clks[i])) {
			clk_disable_unprepare(info->clks[i]);
			clk_put(info->clks[i]);
		}
		info->clks[i] = NULL;
	}
#endif

	/* nullify isomgr request */
	ret = tegra_camera_isomgr_release(info);
	if (ret) {
		dev_err(info->dev,
		"%s: failed to deallocate memory in isomgr\n",
		__func__);
		return -ENOMEM;
	}

	return 0;
}

static long tegra_camera_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct tegra_camera_info *info;

	info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(TEGRA_CAMERA_IOCTL_SET_BW):
	{
		struct bw_info kcopy;
		unsigned long mc_khz = 0;

		memset(&kcopy, 0, sizeof(kcopy));

		if (copy_from_user(&kcopy, (const void __user *)arg,
			sizeof(struct bw_info))) {
			dev_err(info->dev, "%s:Failed to get data from user\n",
				__func__);
			return -EFAULT;
		}

		/* Use Khz to prevent overflow */
		mc_khz = tegra_emc_bw_to_freq_req(kcopy.bw);
		mc_khz = min(ULONG_MAX / 1000, mc_khz);

		if (kcopy.is_iso) {
			dev_dbg(info->dev, "%s:Set iso bw %llu at %lu KHz\n",
				__func__, kcopy.bw, mc_khz);
			/*
			 * Request to ISOMGR.
			 * 3 usec is minimum time to switch PLL source.
			 * Let's put 4 usec as latency for now.
			 */
			ret = tegra_camera_isomgr_request(info, kcopy.bw, 4);
			if (ret) {
				dev_err(info->dev,
				"%s: failed to reserve %llu KBps with isomgr\n",
				__func__, kcopy.bw);
				return -ENOMEM;
			}
#if !defined(CONFIG_TEGRA_BWMGR)
			ret = clk_set_rate(info->clks[ISO_EMC], mc_khz * 1000);
			if (ret)
				dev_err(info->dev, "%s:Failed to set iso bw\n",
					__func__);
#endif
		} else {
			dev_dbg(info->dev, "%s:Set bw %llu at %lu KHz\n",
				__func__, kcopy.bw, mc_khz);
#if defined(CONFIG_TEGRA_BWMGR)
			ret = tegra_bwmgr_set_emc(info->bwmgr_handle,
				mc_khz*1000, TEGRA_BWMGR_SET_EMC_SHARED_BW);
#else
			ret = clk_set_rate(info->clks[EMC], mc_khz * 1000);
#endif
			if (ret)
				dev_err(info->dev, "%s:Failed to set bw\n",
					__func__);
		}
		break;
	}
	default:
		break;
	}

	return 0;
}

static const struct file_operations tegra_camera_ops = {
	.owner = THIS_MODULE,
	.open = tegra_camera_open,
	.unlocked_ioctl = tegra_camera_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tegra_camera_ioctl,
#endif
	.release = tegra_camera_release,
};

static int tegra_camera_probe(struct platform_device *pdev)
{
	int ret;
	struct tegra_camera_info *info;

	dev_info(&pdev->dev, "%s:camera_platform_driver probe\n", __func__);

	tegra_camera_misc.minor = MISC_DYNAMIC_MINOR;
	tegra_camera_misc.name = CAMDEV_NAME;
	tegra_camera_misc.fops = &tegra_camera_ops;
	tegra_camera_misc.parent = &pdev->dev;

	ret = misc_register(&tegra_camera_misc);
	if (ret) {
		dev_err(tegra_camera_misc.this_device,
			"register failed for %s\n",
			tegra_camera_misc.name);
		return ret;
	}
	info = devm_kzalloc(tegra_camera_misc.this_device,
		sizeof(struct tegra_camera_info), GFP_KERNEL);
	if (!info) {
		dev_err(tegra_camera_misc.this_device,
			"Can't allocate memory for %s\n",
			tegra_camera_misc.name);
		return -ENOMEM;
	}

	memset(info, 0, sizeof(*info));

	strcpy(info->devname, tegra_camera_misc.name);
	info->dev = &pdev->dev;

	/* Register Camera as isomgr client. */
	ret = tegra_camera_isomgr_register(info);
	if (ret) {
		dev_err(info->dev,
		"%s: failed to register CAMERA as isomgr client\n",
		__func__);
		return -ENOMEM;
	}

	tegra_camera_dev_mfi_init();
	tegra_vi_register_mfi_cb(tegra_camera_dev_mfi_cb, NULL);

	platform_set_drvdata(pdev, info);

	return 0;

}
static int tegra_camera_remove(struct platform_device *pdev)
{
	struct tegra_camera_info *info = platform_get_drvdata(pdev);
	dev_info(&pdev->dev, "%s:camera_platform_driver remove\n", __func__);

	tegra_vi_unregister_mfi_cb();

    tegra_camera_isomgr_unregister(info);

	return misc_deregister(&tegra_camera_misc);
}

static struct platform_driver tegra_camera_driver = {
	.probe = tegra_camera_probe,
	.remove = tegra_camera_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "tegra_camera_platform",
		.of_match_table = tegra_camera_of_ids
	}
};
static int __init tegra_camera_init(void)
{
	return platform_driver_register(&tegra_camera_driver);
}
static void __exit tegra_camera_exit(void)
{
	platform_driver_unregister(&tegra_camera_driver);
}

module_init(tegra_camera_init);
module_exit(tegra_camera_exit);

