/*
 * Copyright (C) 2017 Intel, Inc.
 * Copyright (C) 2016 Google, Inc.
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
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/smwall.h>
#include <linux/trusty/trusty.h>

struct trusty_timer {
	struct sec_timer_state *sts;
	struct hrtimer tm;
	struct work_struct work;
};

struct trusty_timer_dev_state {
	struct device *dev;
	struct device *smwall_dev;
	struct device *trusty_dev;
	struct notifier_block call_notifier;
	struct trusty_timer timer;
	struct workqueue_struct *workqueue;
};

static void timer_work_func(struct work_struct *work)
{
	int ret;
	struct trusty_timer_dev_state *s;

	s = container_of(work, struct trusty_timer_dev_state, timer.work);

	ret = trusty_std_call32(s->trusty_dev, SMC_SC_LK_TIMER, 0, 0, 0);
	if (ret != 0)
		dev_err(s->dev, "%s failed %d\n", __func__, ret);
}

static enum hrtimer_restart trusty_timer_cb(struct hrtimer *tm)
{
	struct trusty_timer_dev_state *s;

	s = container_of(tm, struct trusty_timer_dev_state, timer.tm);

	queue_work_on(0, s->workqueue, &s->timer.work);

	return HRTIMER_NORESTART;
}

static int trusty_timer_call_notify(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct trusty_timer *tt;
	struct sec_timer_state *sts;
	struct trusty_timer_dev_state *s;

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	s = container_of(nb, struct trusty_timer_dev_state, call_notifier);

	/* this notifier is executed in non-preemptible context */
	tt = &s->timer;
	sts = tt->sts;

	if (sts->tv_ns > sts->cv_ns) {
		hrtimer_cancel(&tt->tm);
	} else if (sts->cv_ns > sts->tv_ns) {
		/* need to set/reset timer */
		hrtimer_start(&tt->tm, ns_to_ktime(sts->cv_ns - sts->tv_ns),
				HRTIMER_MODE_REL_PINNED);
	}

	sts->cv_ns = 0ULL;
	sts->tv_ns = 0ULL;

	return NOTIFY_OK;
}

static int trusty_timer_probe(struct platform_device *pdev)
{
	int ret;
	struct trusty_timer_dev_state *s;
	struct trusty_timer *tt;

	ret = trusty_detect_vmm();
	if (ret < 0) {
		dev_err(&pdev->dev, "Cannot detect VMM which supports trusty!");
		return -EINVAL;
	}

	dev_dbg(&pdev->dev, "%s\n", __func__);

	if (!trusty_wall_base(pdev->dev.parent)) {
		dev_notice(&pdev->dev, "smwall: is not setup by parent\n");
		return -ENODEV;
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;

	s->dev = &pdev->dev;
	s->smwall_dev = s->dev->parent;
	s->trusty_dev = s->smwall_dev->parent;
	platform_set_drvdata(pdev, s);

	tt = &s->timer;

	hrtimer_init(&tt->tm, CLOCK_BOOTTIME, HRTIMER_MODE_REL_PINNED);
	tt->tm.function = trusty_timer_cb;
	tt->sts =
		trusty_wall_per_cpu_item_ptr(s->smwall_dev, 0,
				SM_WALL_PER_CPU_SEC_TIMER_ID,
				sizeof(*tt->sts));
	WARN_ON(!tt->sts);

	s->workqueue = alloc_workqueue("trusty-timer-wq", WQ_CPU_INTENSIVE, 0);
	if (!s->workqueue) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "Failed to allocate work queue\n");
		goto err_allocate_work_queue;
	}

	/* register notifier */
	s->call_notifier.notifier_call = trusty_timer_call_notify;
	ret = trusty_call_notifier_register(s->trusty_dev, &s->call_notifier);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register call notifier\n");
		kfree(s);
		return ret;
	}

	INIT_WORK(&s->timer.work, timer_work_func);

	dev_info(s->dev, "initialized\n");

	return 0;

	destroy_workqueue(s->workqueue);
err_allocate_work_queue:
	kfree(s);
	return ret;

}

static int trusty_timer_remove(struct platform_device *pdev)
{
	struct trusty_timer_dev_state *s = platform_get_drvdata(pdev);
	struct trusty_timer *tt;


	dev_dbg(&pdev->dev, "%s\n", __func__);

	/* unregister notifier */
	trusty_call_notifier_unregister(s->trusty_dev, &s->call_notifier);

	tt = &s->timer;
	hrtimer_cancel(&tt->tm);

	flush_work(&tt->work);
	destroy_workqueue(s->workqueue);
	/* free state */
	kfree(s);
	return 0;
}

static const struct of_device_id trusty_test_of_match[] = {
	{ .compatible = "android,trusty-timer-v1", },
	{},
};

static struct platform_driver trusty_timer_driver = {
	.probe = trusty_timer_probe,
	.remove = trusty_timer_remove,
	.driver = {
		.name = "trusty-timer",
		.owner = THIS_MODULE,
		.of_match_table = trusty_test_of_match,
	},
};

module_platform_driver(trusty_timer_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Trusty timer driver");
