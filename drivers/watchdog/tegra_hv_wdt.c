/*
 * drivers/watchdog/tegra_hv_wdt.c
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/init.h>
#include <linux/of.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-soc.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/interrupt.h>

/* The hypervisor monitor service requires the watchdog to be refreshed at
 * least once every 60 seconds, so set our refresh time to less than half of
 * that to account for client reboots that may not be communicated to the
 * monitor service. Note that should all client reboots be communicated to
 * the monitor service, then the refresh interval can be safely doubled. */
#define REFRESH_INTERVAL_SECS (28)

#define IVC_MESSAGE_DATA "x"
#define IVC_SUCCESS_CODE sizeof(IVC_MESSAGE_DATA)

struct tegra_hv_wdt {
	struct watchdog_device wdt;
	struct tegra_hv_ivc_cookie *ivc;
	struct platform_device *pdev;
	struct task_struct *thread;
	wait_queue_head_t notify;
	struct mutex lock;
	bool interrupt;
	bool user;
	int saved;
};

static int tegra_hv_wdt_start(struct watchdog_device *wdt)
{
	struct tegra_hv_wdt *hv = container_of(wdt, struct tegra_hv_wdt, wdt);

	dev_info(&hv->pdev->dev, "device opened\n");
	hv->user = true;

	return 0;
}

/*
 * This function is only used when the user does a "magic close"
 * (by writing 'V' to the /dev/watchdog device.)
 */
static int tegra_hv_wdt_stop(struct watchdog_device *wdt)
{
	struct tegra_hv_wdt *hv = container_of(wdt, struct tegra_hv_wdt, wdt);

	dev_info(&hv->pdev->dev, "device closed\n");
	hv->user = false;

	return 0;
}

static int tegra_hv_wdt_ping(struct watchdog_device *wdt)
{
	struct tegra_hv_wdt *hv = container_of(wdt, struct tegra_hv_wdt, wdt);
	int ret;

	mutex_lock(&hv->lock);

	ret = tegra_hv_ivc_write(hv->ivc, IVC_MESSAGE_DATA,
		sizeof(IVC_MESSAGE_DATA));

	/*
	 * If a write error occurs, we log the problem only on state changes
	 * to error states. This gives us indications when a problem may be
	 * starting but won't flood system logs with duplicate information of
	 * little value if the error condition persists.
	 */

	if ((ret != IVC_SUCCESS_CODE) && (ret != hv->saved))
		dev_err(&hv->pdev->dev, "ivc write error %d\n", ret);
	hv->saved = ret;

	mutex_unlock(&hv->lock);

	return 0;
}

static int tegra_hv_wdt_notified(struct tegra_hv_wdt *hv)
{
	int ret;

	mutex_lock(&hv->lock);
	ret = tegra_hv_ivc_channel_notified(hv->ivc);
	mutex_unlock(&hv->lock);

	return ret;
}

static int tegra_hv_wdt_loop(void *arg)
{
	struct tegra_hv_wdt *hv = (struct tegra_hv_wdt *)arg;

	mutex_lock(&hv->lock);
	tegra_hv_ivc_channel_reset(hv->ivc);
	mutex_unlock(&hv->lock);

	wait_event(hv->notify, tegra_hv_wdt_notified(hv) == 0);
	dev_info(&hv->pdev->dev, "ivc channel ready\n");

	/*
	 * At this point, the channel has completed reset and we can
	 * now communicate with the monitor service. We no longer have
	 * a need for interrupts (though they shouldn't happen.)
	 */

	devm_free_irq(&hv->pdev->dev, hv->ivc->irq, &hv->wdt);
	hv->interrupt = false;

	while (!kthread_should_stop()) {
		if (!hv->user)
			tegra_hv_wdt_ping(&hv->wdt);
		ssleep(REFRESH_INTERVAL_SECS);
	}

	return 0;
}

static irqreturn_t tegra_hv_wdt_interrupt(int irq, void *data)
{
	struct tegra_hv_wdt *hv = (struct tegra_hv_wdt *)data;
	wake_up(&hv->notify);
	return IRQ_HANDLED;
}

static int tegra_hv_wdt_parse(struct platform_device *pdev,
	struct device_node **qn, u32 *id)
{
	struct device_node *dn;

	dn = pdev->dev.of_node;
	if (dn == NULL) {
		dev_err(&pdev->dev, "failed to find device node\n");
		return -EINVAL;
	}

	if (of_property_read_u32_index(dn, "ivc", 1, id) != 0) {
		dev_err(&pdev->dev, "failed to find ivc property\n");
		return -EINVAL;
	}

	*qn = of_parse_phandle(dn, "ivc", 0);
	if (*qn == NULL) {
		dev_err(&pdev->dev, "failed to find queue node\n");
		return -EINVAL;
	}

	return 0;
}

static const struct watchdog_info tegra_hv_wdt_info = {
	.options = WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "Tegra HV WDT",
	.firmware_version = 1,
};

static const struct watchdog_ops tegra_hv_wdt_ops = {
	.owner = THIS_MODULE,
	.start = tegra_hv_wdt_start,
	.stop = tegra_hv_wdt_stop,
	.ping = tegra_hv_wdt_ping,
};

static int tegra_hv_wdt_setup_no_cleanup(struct tegra_hv_wdt *hv,
	struct platform_device *pdev, struct device_node *qn, unsigned int id)
{
	int errcode;

	init_waitqueue_head(&hv->notify);
	mutex_init(&hv->lock);

	hv->user = false;
	hv->saved = IVC_SUCCESS_CODE;
	hv->pdev = pdev;

	hv->ivc = tegra_hv_ivc_reserve(qn, id, NULL);
	if (IS_ERR_OR_NULL(hv->ivc)) {
		dev_err(&pdev->dev, "failed to reserve ivc %u\n", id);
		return -EINVAL;
	}

	errcode = devm_request_irq(&pdev->dev, hv->ivc->irq,
		tegra_hv_wdt_interrupt, 0, dev_name(&pdev->dev), (void *)hv);
	if (errcode < 0) {
		dev_err(&pdev->dev, "failed to get irq %d\n", hv->ivc->irq);
		return -EINVAL;
	}

	hv->interrupt = true;

	hv->thread = kthread_create(tegra_hv_wdt_loop, (void *)hv,
		"tegra-hv-wdt");
	if (IS_ERR_OR_NULL(hv->thread)) {
		dev_err(&pdev->dev, "failed to create kthread\n");
		return -EINVAL;
	}

	hv->wdt.info = &tegra_hv_wdt_info;
	hv->wdt.ops = &tegra_hv_wdt_ops;
	hv->wdt.timeout = REFRESH_INTERVAL_SECS;

	errcode = watchdog_register_device(&hv->wdt);
	if (errcode < 0) {
		memset(&hv->wdt, 0, sizeof(hv->wdt));
		dev_err(&pdev->dev, "failed to register device\n");
		return errcode;
	}

	return 0;
}

static void tegra_hv_wdt_cleanup(struct tegra_hv_wdt *hv)
{
	int errcode;

	if (!IS_ERR_OR_NULL(hv->thread)) {
		errcode = kthread_stop(hv->thread);
		if ((errcode != 0) && (errcode != -EINTR))
			dev_err(&hv->pdev->dev, "failed to stop thread\n");
	}

	if (hv->wdt.info)
		watchdog_unregister_device(&hv->wdt);

	if (hv->interrupt)
		devm_free_irq(&hv->pdev->dev, hv->ivc->irq, &hv->wdt);

	if (!IS_ERR_OR_NULL(hv->ivc)) {
		if (tegra_hv_ivc_unreserve(hv->ivc) != 0)
			dev_err(&hv->pdev->dev, "failed to unreserve ivc\n");
	}
}

static int tegra_hv_wdt_probe(struct platform_device *pdev)
{
	struct tegra_hv_wdt *hv;
	struct device_node *qn;
	int errcode;
	u32 id;

	if (!is_tegra_hypervisor_mode()) {
		dev_info(&pdev->dev, "hypervisor is not present\n");
		return -ENODEV;
	}

	errcode = tegra_hv_wdt_parse(pdev, &qn, &id);
	if (errcode < 0) {
		dev_err(&pdev->dev, "failed to parse device tree\n");
		return -ENODEV;
	}

	hv = devm_kzalloc(&pdev->dev, sizeof(*hv), GFP_KERNEL);
	if (!hv) {
		of_node_put(qn);
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	/*
	 * Note that if the setup function fails, there may be
	 * dangling resources. So, we must clean up after failure.
	 */

	errcode = tegra_hv_wdt_setup_no_cleanup(hv, pdev, qn, id);
	of_node_put(qn);

	if (errcode < 0) {
		tegra_hv_wdt_cleanup(hv);
		devm_kfree(&pdev->dev, hv);
		dev_err(&pdev->dev, "failed to setup device\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, hv);

	errcode = wake_up_process(hv->thread);
	if (errcode != 1)
		dev_err(&pdev->dev, "failed to wake up thread\n");

	dev_info(&pdev->dev, "id=%u irq=%d peer=%d num=%d size=%d\n",
		id, hv->ivc->irq, hv->ivc->peer_vmid, hv->ivc->nframes,
		hv->ivc->frame_size);

	return 0;
}

static int tegra_hv_wdt_remove(struct platform_device *pdev)
{
	struct tegra_hv_wdt *hv = platform_get_drvdata(pdev);

	/*
	 * The below cleanup function will block waiting for the
	 * refresh kthread to exit (if it has already started running.)
	 */

	tegra_hv_wdt_cleanup(hv);
	devm_kfree(&pdev->dev, hv);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id tegra_hv_wdt_match[] = {
	{ .compatible = "nvidia,tegra-hv-wdt", },
	{}
};

static struct platform_driver tegra_hv_wdt_driver = {
	.probe		= tegra_hv_wdt_probe,
	.remove		= tegra_hv_wdt_remove,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "tegra_hv_wdt",
		.of_match_table = of_match_ptr(tegra_hv_wdt_match),
	},
};

static int __init tegra_hv_wdt_init(void)
{
	return platform_driver_register(&tegra_hv_wdt_driver);
}

static void __exit tegra_hv_wdt_exit(void)
{
	platform_driver_unregister(&tegra_hv_wdt_driver);
}

subsys_initcall(tegra_hv_wdt_init);
module_exit(tegra_hv_wdt_exit);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("Tegra Hypervisor Watchdog Driver");
MODULE_LICENSE("GPL");
