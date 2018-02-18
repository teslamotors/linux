/*
 * drivers/watchdog/tegra_wdt.c
 *
 * watchdog driver for NVIDIA tegra internal watchdog
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation.
 *
 * based on drivers/watchdog/softdog.c and drivers/watchdog/omap_wdt.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/uaccess.h>
#include <linux/watchdog.h>
#include <linux/tegra-soc.h>

/* minimum and maximum watchdog trigger periods, in seconds */
#define MIN_WDT_PERIOD	5
#define MAX_WDT_PERIOD	1000

struct tegra_wdt {
	struct platform_device	*pdev;
	struct watchdog_device	wdt;
	unsigned long		users;
	void __iomem		*wdt_source;
	void __iomem		*wdt_timer;
	u32			config;
	int			irq;
	unsigned long		status;
	bool			extended_suspend;
/* Bit numbers for status flags */
#define WDT_ENABLED		0
#define WDT_ENABLED_ON_INIT	1
#define WDT_ENABLED_USERSPACE	2
};

/*
 * The total expiry count of Tegra WDTs is limited to HW design and depends
 * on skip configuration if supported. To be safe, we set the default expiry
 * count to 1. It should be updated later with value specified in device tree.
 */
static int expiry_count = 1;

/*
 * To detect lockup condition, the heartbeat should be expiry_count*lockup.
 * It may be taken over later by timeout value requested by application.
 * Must be greater than expiry_count*MIN_WDT_PERIOD and lower than
 * expiry_count*MAX_WDT_PERIOD.
 */
static int heartbeat = 120;

static struct syscore_ops tegra_wdt_syscore_ops;

static struct tegra_wdt *s_tegra_wdt;

static inline struct tegra_wdt *to_tegra_wdt(struct watchdog_device *wdt)
{
	return container_of(wdt, struct tegra_wdt, wdt);
}

#define TIMER_PTV			0
#define TIMER_EN			(1 << 31)
#define TIMER_PERIODIC			(1 << 30)
#define TIMER_PCR			0x4
#define TIMER_PCR_INTR			(1 << 30)
#define WDT_CFG				(0)
#define WDT_CFG_PERIOD			(1 << 4)
#define WDT_CFG_INT_EN			(1 << 12)
#define WDT_CFG_FIQ_EN			(1 << 13)
#define WDT_CFG_SYS_RST_EN		(1 << 14)
#define WDT_CFG_PMC2CAR_RST_EN		(1 << 15)
#define WDT_STATUS			(4)
#define WDT_INTR_STAT			(1 << 1)
#define WDT_CMD				(8)
#define WDT_CMD_START_COUNTER		(1 << 0)
#define WDT_CMD_DISABLE_COUNTER		(1 << 1)
#define WDT_UNLOCK			(0xC)
#define WDT_UNLOCK_PATTERN		(0xC45A << 0)
#define MAX_NR_CPU_WDT			0x4

static int __tegra_wdt_ping(struct tegra_wdt *tegra_wdt)
{
	u32 val;

	/*
	 * Disable timer, load the timeout value and restart.
	 */
	writel(WDT_UNLOCK_PATTERN, tegra_wdt->wdt_source + WDT_UNLOCK);
	writel(WDT_CMD_DISABLE_COUNTER, tegra_wdt->wdt_source + WDT_CMD);

	writel(TIMER_PCR_INTR, tegra_wdt->wdt_timer + TIMER_PCR);
	val = (tegra_wdt->wdt.timeout * USEC_PER_SEC) / expiry_count;
	val |= (TIMER_EN | TIMER_PERIODIC);
	writel(val, tegra_wdt->wdt_timer + TIMER_PTV);

	writel(WDT_CMD_START_COUNTER, tegra_wdt->wdt_source + WDT_CMD);

	dev_dbg(tegra_wdt->wdt.dev, "wdt cleared\n");
	return 0;
}

static irqreturn_t tegra_wdt_irq(int irq, void *data)
{
	struct tegra_wdt *tegra_wdt = data;

	__tegra_wdt_ping(tegra_wdt);

	return IRQ_HANDLED;
}

static void tegra_wdt_ref(struct watchdog_device *wdt)
{
	struct tegra_wdt *tegra_wdt = to_tegra_wdt(wdt);

	if (tegra_wdt->irq <= 0)
		return;

	/*
	 * Remove the interrupt handler if userspace is taking over WDT.
	 */
	if (!test_and_set_bit(WDT_ENABLED_USERSPACE, &tegra_wdt->status) &&
			test_bit(WDT_ENABLED_ON_INIT, &tegra_wdt->status))
		devm_free_irq(&tegra_wdt->pdev->dev, tegra_wdt->irq,
			tegra_wdt);
}

static int __tegra_wdt_enable(struct tegra_wdt *tegra_wdt)
{
	u32 val;

	writel(TIMER_PCR_INTR, tegra_wdt->wdt_timer + TIMER_PCR);
	/*
	 * The timeout needs to be divided by expiry_count here so as to
	 * keep the ultimate watchdog reset timeout the same as the program
	 * timeout requested by application. The program timeout should make
	 * sure WDT FIQ will never be asserted in a valid use case.
	 */
	val = (tegra_wdt->wdt.timeout * USEC_PER_SEC) / expiry_count;
	val |= (TIMER_EN | TIMER_PERIODIC);
	writel(val, tegra_wdt->wdt_timer + TIMER_PTV);

	writel(tegra_wdt->config, tegra_wdt->wdt_source + WDT_CFG);
	writel(WDT_CMD_START_COUNTER, tegra_wdt->wdt_source + WDT_CMD);

	set_bit(WDT_ENABLED, &tegra_wdt->status);

	return 0;
}

static int __tegra_wdt_disable(struct tegra_wdt *tegra_wdt)
{
	writel(WDT_UNLOCK_PATTERN, tegra_wdt->wdt_source + WDT_UNLOCK);
	writel(WDT_CMD_DISABLE_COUNTER, tegra_wdt->wdt_source + WDT_CMD);

	writel(0, tegra_wdt->wdt_timer + TIMER_PTV);

	clear_bit(WDT_ENABLED, &tegra_wdt->status);

	return 0;
}

static int tegra_wdt_enable(struct watchdog_device *wdt)
{
	struct tegra_wdt *tegra_wdt = to_tegra_wdt(wdt);
	return __tegra_wdt_enable(tegra_wdt);
}

static int tegra_wdt_disable(struct watchdog_device *wdt)
{
	struct tegra_wdt *tegra_wdt = to_tegra_wdt(wdt);
	return __tegra_wdt_disable(tegra_wdt);
}

static int tegra_wdt_ping(struct watchdog_device *wdt)
{
	struct tegra_wdt *tegra_wdt = to_tegra_wdt(wdt);
	return __tegra_wdt_ping(tegra_wdt);
}


static int tegra_wdt_set_timeout(struct watchdog_device *wdt,
	unsigned int timeout)
{
	tegra_wdt_disable(wdt);
	wdt->timeout = timeout;
	tegra_wdt_enable(wdt);

	dev_info(wdt->dev, "wdt timeout set to %u seconds\n", timeout);
	return 0;
}

static const struct watchdog_info tegra_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "Tegra WDT",
	.firmware_version = 1,
};

static const struct watchdog_ops tegra_wdt_ops = {
	.owner = THIS_MODULE,
	.start = tegra_wdt_enable,
	.stop  = tegra_wdt_disable,
	.ping  = tegra_wdt_ping,
	.set_timeout = tegra_wdt_set_timeout,
	.ref   = tegra_wdt_ref,
};

#ifdef CONFIG_DEBUG_FS

static int disable_wdt_reset_show(void *data, u64 *val)
{
	struct tegra_wdt *tegra_wdt = data;

	*val = tegra_wdt->config &
			(WDT_CFG_SYS_RST_EN | WDT_CFG_PMC2CAR_RST_EN)
			? 0 : 1;
	return 0;
}

static int disable_wdt_reset_store(void *data, u64 val)
{
	struct tegra_wdt *tegra_wdt = data;

	tegra_wdt->config = readl(tegra_wdt->wdt_source + WDT_CFG);
	if (val)
		tegra_wdt->config &= ~(WDT_CFG_SYS_RST_EN | WDT_CFG_PMC2CAR_RST_EN);
	else
		tegra_wdt->config |= WDT_CFG_PMC2CAR_RST_EN;

	/* Apply config only if WDT is enabled */
	if (test_bit(WDT_ENABLED, &tegra_wdt->status)) {
		__tegra_wdt_disable(tegra_wdt);
		__tegra_wdt_enable(tegra_wdt);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(disable_wdt_reset_fops, disable_wdt_reset_show,
	disable_wdt_reset_store, "%lld\n");

static void tegra_wdt_debugfs_init(struct tegra_wdt *tegra_wdt)
{
	struct dentry *root;
	struct dentry *retval;

	root = debugfs_create_dir("tegra_wdt", NULL);
	if (IS_ERR_OR_NULL(root))
		goto clean;

	retval = debugfs_create_file("disable_wdt_reset", S_IRUGO | S_IWUSR,
				root, (void *)tegra_wdt, &disable_wdt_reset_fops);
	if (IS_ERR_OR_NULL(retval))
		goto clean;

	return;
clean:
	pr_warn("tegra_wdt: Failed to create debugfs!\n");
	if (root)
		debugfs_remove_recursive(root);
}

#else /* !CONFIG_DEBUG_FS */
static inline void tegra_wdt_debugfs_init(struct tegra_wdt* tegra_wdt) { };
#endif /* CONFIG_DEBUG_FS */

static int tegra_wdt_probe(struct platform_device *pdev)
{
	struct resource *res_src, *res_wdt;
	struct tegra_wdt *tegra_wdt;
	struct device_node *np = pdev->dev.of_node;
	u32 pval = 0;
	int ret = 0;
	bool enable_on_init;
	bool extended_suspend = false;

	if (!np) {
		dev_err(&pdev->dev, "Supprot registration from DT only");
		return -EPERM;
	}

	enable_on_init = of_property_read_bool(np, "nvidia,enable-on-init");
	ret = of_property_read_u32(np, "nvidia,heartbeat-init", &pval);
	if (!ret)
		heartbeat = pval;

	ret = of_property_read_u32(np, "nvidia,expiry-count", &pval);
	if (!ret)
		expiry_count = pval;

	extended_suspend = of_property_read_bool(np,
					"nvidia,extend-watchdog-suspend");
	tegra_wdt = devm_kzalloc(&pdev->dev, sizeof(*tegra_wdt), GFP_KERNEL);
	if (!tegra_wdt) {
		dev_err(&pdev->dev, "out of memory\n");
		return -ENOMEM;
	}

	tegra_wdt->pdev = pdev;
	tegra_wdt->extended_suspend = extended_suspend;
	tegra_wdt->wdt.info = &tegra_wdt_info;
	tegra_wdt->wdt.ops = &tegra_wdt_ops;
	tegra_wdt->wdt.min_timeout = MIN_WDT_PERIOD * expiry_count;
	tegra_wdt->wdt.max_timeout = MAX_WDT_PERIOD * expiry_count;
	watchdog_init_timeout(&tegra_wdt->wdt, heartbeat, &pdev->dev);

	res_src = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	res_wdt = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (!res_src || !res_wdt) {
		dev_err(&pdev->dev, "incorrect resources\n");
		return -ENOENT;
	}

	tegra_wdt->wdt_source = devm_ioremap_resource(&pdev->dev, res_src);
	if (IS_ERR(tegra_wdt->wdt_source)) {
		dev_err(&pdev->dev, "Cannot request memregion/iomap res_src\n");
		return PTR_ERR(tegra_wdt->wdt_source);
	}

	tegra_wdt->wdt_timer = devm_ioremap_resource(&pdev->dev, res_wdt);
	if (IS_ERR(tegra_wdt->wdt_timer)) {
		dev_err(&pdev->dev, "Cannot request memregion/iomap res_wdt\n");
		return PTR_ERR(tegra_wdt->wdt_timer);
	}

	/* Configure timer source */
	if ((res_wdt->start & 0xff) < 0x50)
		tegra_wdt->config = 1 + (res_wdt->start & 0xf) / 8;
	else
		tegra_wdt->config = ((int) (3 + ((res_wdt->start & 0xff) -
							0x50) / 8)) % 10;
	/* Enable interrupts and reset events by default */
	tegra_wdt->config |= WDT_CFG_PERIOD | WDT_CFG_INT_EN |
				WDT_CFG_FIQ_EN | WDT_CFG_PMC2CAR_RST_EN;

	tegra_wdt_disable(&tegra_wdt->wdt);
	writel(TIMER_PCR_INTR, tegra_wdt->wdt_timer + TIMER_PCR);

	/* Init and enable watchdog on WDT0 during probe */
	if (enable_on_init) {
		if (of_machine_is_compatible("nvidia,tegra210")) {
			tegra_wdt->irq = platform_get_irq(pdev, 0);
			if (tegra_wdt->irq <= 0) {
				dev_err(&pdev->dev, "failed to get WDT IRQ\n");
				return -ENXIO;
			}

			ret = devm_request_threaded_irq(&pdev->dev,
					tegra_wdt->irq,	NULL, tegra_wdt_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					dev_name(&pdev->dev), tegra_wdt);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"failed to register irq %d err %d\n",
					tegra_wdt->irq, ret);
				return ret;
			}
		}

		tegra_wdt_enable(&tegra_wdt->wdt);
		set_bit(WDOG_ACTIVE, &tegra_wdt->wdt.status);
		set_bit(WDT_ENABLED_ON_INIT, &tegra_wdt->status);
		pr_info("Tegra WDT enabled on probe. Timeout = %u seconds.\n",
						tegra_wdt->wdt.timeout);
	}

	ret = watchdog_register_device(&tegra_wdt->wdt);
	if (ret) {
		dev_err(&pdev->dev, "failed to register watchdog device\n");
		if (enable_on_init)
			tegra_wdt_disable(&tegra_wdt->wdt);
		return ret;
	}

	platform_set_drvdata(pdev, tegra_wdt);

	tegra_wdt_debugfs_init(tegra_wdt);

	if (tegra_wdt->extended_suspend)
		register_syscore_ops(&tegra_wdt_syscore_ops);

	s_tegra_wdt = tegra_wdt;
	dev_info(&pdev->dev, "%s done\n", __func__);
	return 0;
}

static int tegra_wdt_remove(struct platform_device *pdev)
{
	struct tegra_wdt *tegra_wdt = platform_get_drvdata(pdev);

	tegra_wdt_disable(&tegra_wdt->wdt);

	watchdog_unregister_device(&tegra_wdt->wdt);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_wdt_suspend(struct device *dev)
{
	struct tegra_wdt *tegra_wdt = dev_get_drvdata(dev);

	if (!tegra_wdt->extended_suspend)
		__tegra_wdt_disable(tegra_wdt);
	else
		__tegra_wdt_ping(tegra_wdt);
	return 0;
}

static int tegra_wdt_resume(struct device *dev)
{
	struct tegra_wdt *tegra_wdt = dev_get_drvdata(dev);

	if (watchdog_active(&tegra_wdt->wdt)) {
		if (tegra_wdt->extended_suspend)
			__tegra_wdt_ping(tegra_wdt);
		else
			__tegra_wdt_enable(tegra_wdt);
	} else {
		if (tegra_wdt->extended_suspend)
			__tegra_wdt_disable(tegra_wdt);
	}

	return 0;
}

static int tegra_wdt_syscore_suspend(void)
{
	if (!s_tegra_wdt->extended_suspend)
		return 0;

	__tegra_wdt_disable(s_tegra_wdt);
	return 0;
}

static void tegra_wdt_syscore_resume(void)
{
	if (!s_tegra_wdt->extended_suspend)
		return;

	__tegra_wdt_enable(s_tegra_wdt);
}
#else
static int tegra_wdt_syscore_suspend(void)
{
	return 0;
}
static void tegra_wdt_syscore_resume(void) { }
#endif

static struct syscore_ops tegra_wdt_syscore_ops = {
	.suspend =	tegra_wdt_syscore_suspend,
	.resume =	tegra_wdt_syscore_resume,
};

static const struct dev_pm_ops tegra_wdt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tegra_wdt_suspend, tegra_wdt_resume)
};

static const struct of_device_id tegra_wdt_match[] = {
	{ .compatible = "nvidia,tegra-wdt", },
	{}
};
MODULE_DEVICE_TABLE(of, tegra_wdt_match);

static struct platform_driver tegra_wdt_driver = {
	.probe		= tegra_wdt_probe,
	.remove		= tegra_wdt_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "tegra_wdt",
		.pm	= &tegra_wdt_pm_ops,
		.of_match_table = of_match_ptr(tegra_wdt_match),
	},
};

static int __init tegra_wdt_init(void)
{
	return platform_driver_register(&tegra_wdt_driver);
}

static void __exit tegra_wdt_exit(void)
{
	platform_driver_unregister(&tegra_wdt_driver);
}

subsys_initcall(tegra_wdt_init);
module_exit(tegra_wdt_exit);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("Tegra Watchdog Driver");

module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog heartbeat period in seconds");

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tegra_wdt");
