/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>

#define HIER_GROUP_CPU_ENABLE                                 0x00000000
#define HIER_GROUP_CPU_STATUS                                 0x00000004
#define HIER_GROUP1_MSELECT_ERROR                                     15

#define MSELECT_CONFIG_0                                             0x0
#define MSELECT_CONFIG_0_READ_TIMEOUT_EN_SLAVE0_SHIFT                 16
#define MSELECT_CONFIG_0_WRITE_TIMEOUT_EN_SLAVE0_SHIFT                17
#define MSELECT_CONFIG_0_READ_TIMEOUT_EN_SLAVE1_SHIFT                 18
#define MSELECT_CONFIG_0_WRITE_TIMEOUT_EN_SLAVE1_SHIFT                19
#define MSELECT_CONFIG_0_READ_TIMEOUT_EN_SLAVE2_SHIFT                 20
#define MSELECT_CONFIG_0_WRITE_TIMEOUT_EN_SLAVE2_SHIFT                21
#define MSELECT_CONFIG_0_ERR_RESP_EN_SLAVE1_SHIFT                     24
#define MSELECT_CONFIG_0_ERR_RESP_EN_SLAVE2_SHIFT                     25

#define MSELECT_TIMEOUT_TIMER_0                                     0x5c
#define MSELECT_ERROR_STATUS_0                                      0x60
#define MSELECT_DEFAULT_TIMEOUT                                 0xFFFFFF

struct tegra_mselect_control {
	u32 irq;
	u32 mselect_timeout_cycles;
	void __iomem *hier_ictlr_base;
	void __iomem *mselect_base;
};

static irqreturn_t tegra_mselect_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct tegra_mselect_control *msel = dev_get_drvdata(dev);
	unsigned long status;

	status = readl(msel->mselect_base + MSELECT_ERROR_STATUS_0);
	if (status != 0) {
		pr_err("MSELECT error detected! status=0x%x\n",
			(unsigned int)status);
		BUG();
	}

	return IRQ_HANDLED;
}

static int tegra_mselect_map_memory(struct device *dp,
				       struct tegra_mselect_control *msel)
{
	msel->mselect_base = of_iomap(dp->of_node, 0);
	if (!msel->mselect_base) {
		dev_err(dp, "Unable to map memory (mselect).\n");
		return -EBUSY;
	}

	if (of_device_is_compatible(dp->of_node, "nvidia,tegra-mselect-hier")) {
		msel->hier_ictlr_base = of_iomap(dp->of_node, 1);
		if (msel->hier_ictlr_base)
			return -ENODEV;
	}

	return 0;
}

static int tegra_mselect_irq_init(struct platform_device *pdev,
				     struct tegra_mselect_control *msel)
{
	unsigned long reg;
	int ret;

	msel->irq = platform_get_irq(pdev, 0);
	if (msel->irq <= 0)
		return -EBUSY;

	ret = devm_request_irq(&pdev->dev, msel->irq,
		tegra_mselect_irq_handler, IRQF_TRIGGER_HIGH,
		"mselect_irq", &pdev->dev);

	if (ret) {
		dev_err(&pdev->dev,
			"Unable to request interrupt for device (err=%d).\n",
			ret);
		return ret;
	}

	if (msel->hier_ictlr_base) {
		reg = readl(msel->hier_ictlr_base + HIER_GROUP_CPU_ENABLE);
		writel(reg | (1 << HIER_GROUP1_MSELECT_ERROR),
			msel->hier_ictlr_base + HIER_GROUP_CPU_ENABLE);
	}

	return 0;
}

static void tegra_mselect_set_timeout(struct tegra_mselect_control *msel,
					  u32 timeout_cycles)
{
	msel->mselect_timeout_cycles = timeout_cycles;

	writel(msel->mselect_timeout_cycles, msel->mselect_base +
		MSELECT_TIMEOUT_TIMER_0);
}

static int tegra_mselect_timeout_init(struct tegra_mselect_control *msel)
{
	unsigned long reg;

	tegra_mselect_set_timeout(msel, MSELECT_DEFAULT_TIMEOUT);

	reg = readl(msel->mselect_base + MSELECT_CONFIG_0);
	writel(reg |
		((1 << MSELECT_CONFIG_0_READ_TIMEOUT_EN_SLAVE0_SHIFT)  |
		 (1 << MSELECT_CONFIG_0_WRITE_TIMEOUT_EN_SLAVE0_SHIFT) |
		 (1 << MSELECT_CONFIG_0_READ_TIMEOUT_EN_SLAVE1_SHIFT)  |
		 (1 << MSELECT_CONFIG_0_WRITE_TIMEOUT_EN_SLAVE1_SHIFT) |
		 (1 << MSELECT_CONFIG_0_READ_TIMEOUT_EN_SLAVE2_SHIFT)  |
		 (1 << MSELECT_CONFIG_0_WRITE_TIMEOUT_EN_SLAVE2_SHIFT) |
		 (1 << MSELECT_CONFIG_0_ERR_RESP_EN_SLAVE1_SHIFT)      |
		 (1 << MSELECT_CONFIG_0_ERR_RESP_EN_SLAVE2_SHIFT)),
		msel->mselect_base + MSELECT_CONFIG_0);

	return 0;
}

static ssize_t mselect_timeout_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct tegra_mselect_control *msel = dev_get_drvdata(device);

	return sprintf(buf, "%d\n", msel->mselect_timeout_cycles);
}

static ssize_t mselect_timeout_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tegra_mselect_control *msel = dev_get_drvdata(device);
	unsigned long val = 0;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	tegra_mselect_set_timeout(msel, val);

	return count;
}
static DEVICE_ATTR(mselect_timeout, S_IWUSR | S_IRUGO, mselect_timeout_show,
		mselect_timeout_store);

static void tegra_mselect_create_sysfs(struct platform_device *pdev)
{
	int error;

	error = device_create_file(&pdev->dev, &dev_attr_mselect_timeout);

	if (error)
		dev_err(&pdev->dev, "Failed to create sysfs attributes!\n");
}

static void tegra_mselect_remove_sysfs(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_mselect_timeout);
}

static int tegra_mselect_probe(struct platform_device *pdev)
{
	struct tegra_mselect_control *msel;
	int ret;

	msel = devm_kzalloc(&pdev->dev, sizeof(struct tegra_mselect_control),
		GFP_KERNEL);
	if (!msel)
		return -ENOMEM;

	ret = tegra_mselect_map_memory(&pdev->dev, msel);
	if (ret)
		return ret;

	ret = tegra_mselect_timeout_init(msel);
	if (ret)
		return ret;

	dev_set_drvdata(&pdev->dev, msel);

	ret = tegra_mselect_irq_init(pdev, msel);
	if (ret)
		return ret;

	tegra_mselect_create_sysfs(pdev);

	dev_notice(&pdev->dev, "probed\n");

	return 0;
}

static int __exit tegra_mselect_remove(struct platform_device *pdev)
{
	tegra_mselect_remove_sysfs(pdev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id tegra_mselect_of_match[] = {
	{ .compatible = "nvidia,tegra-mselect", },
	{ .compatible = "nvidia,tegra-mselect-hier", },
	{ },
};

MODULE_DEVICE_TABLE(of, tegra_mselect_of_match);
#endif

static struct platform_driver tegra_mselect_driver = {
	.driver = {
		   .name = "tegra-mselect",
		   .of_match_table = of_match_ptr(tegra_mselect_of_match),
		   .owner = THIS_MODULE,
		   },
	.probe = tegra_mselect_probe,
	.remove = __exit_p(tegra_mselect_remove),
};

static int __init tegra_mselect_init(void)
{
	return platform_driver_register(&tegra_mselect_driver);
}

static void __exit tegra_mselect_exit(void)
{
	platform_driver_unregister(&tegra_mselect_driver);
}

module_init(tegra_mselect_init);
module_exit(tegra_mselect_exit);

MODULE_DESCRIPTION("Tegra Hierarchical Interrupt Controller Driver");
MODULE_LICENSE("GPL v2");
