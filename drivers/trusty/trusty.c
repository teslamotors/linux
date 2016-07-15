/*
 * Copyright (C) 2013 Google, Inc.
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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/sm_err.h>
#include <linux/trusty/trusty.h>

#define TRUSTY_VMCALL_SMC 0x74727500
#define TRUSTY_LKTIMER_INTERVAL 10   /* 10 ms */
#define TRUSTY_LKTIMER_VECTOR   0x31 /* INT_PIT */

enum lktimer_mode {
	ONESHOT_TIMER,
	PERIODICAL_TIMER,
};

struct trusty_state {
	struct device *dev;
	struct mutex smc_lock;
	struct atomic_notifier_head notifier;
	struct completion cpu_idle_completion;
	struct timer_list timer;
	struct work_struct timer_work;
	enum lktimer_mode timer_mode;
	unsigned long timer_interval;
	char *version_str;
	u32 api_version;
};

struct trusty_smc_interface {
	struct device *dev;
	ulong args[5];
};

static void trusty_lktimer_work_func(struct work_struct *work)
{
	int ret;
	unsigned int vector;
	struct trusty_state *s =
			container_of(work, struct trusty_state, timer_work);

	dev_dbg(s->dev, "%s\n", __func__);

	/* need vector number only for the first time */
	vector = TRUSTY_LKTIMER_VECTOR;

	do {
		ret = trusty_std_call32(s->dev, SMC_SC_NOP, vector, 0, 0);
		vector = 0;
	} while (ret == SM_ERR_NOP_INTERRUPTED);

	if (ret != SM_ERR_NOP_DONE)
		dev_err(s->dev, "%s: SMC_SC_NOP failed %d", __func__, ret);

	dev_notice_once(s->dev, "LK OS proxy timer works\n");
}

static void trusty_lktimer_func(unsigned long data)
{
	struct trusty_state *s = (struct trusty_state *)data;

	/* binding it physical CPU0 only because trusty OS runs on it */
	schedule_work_on(0, &s->timer_work);

	/* reactivate the timer again in periodic mode */
	if (s->timer_mode == PERIODICAL_TIMER)
		mod_timer(&s->timer,
			jiffies + msecs_to_jiffies(s->timer_interval));
}

static void trusty_init_lktimer(struct trusty_state *s)
{
	INIT_WORK(&s->timer_work, trusty_lktimer_work_func);
	setup_timer(&s->timer, trusty_lktimer_func, (unsigned long)s);
}

/* note that this function is not thread-safe */
static void trusty_configure_lktimer(struct trusty_state *s,
			enum lktimer_mode mode, unsigned long interval)
{
	if (mode != ONESHOT_TIMER && mode != PERIODICAL_TIMER) {
		pr_err("%s: invalid timer mode: %d\n", __func__, mode);
		return;
	}

	s->timer_mode = mode;
	s->timer_interval = interval;
	mod_timer(&s->timer, jiffies + msecs_to_jiffies(s->timer_interval));
}

/*
 * this should be called when removing trusty dev and
 * when LK/Trusty crashes, to disable proxy timer.
 */
static void trusty_del_lktimer(struct trusty_state *s)
{
	del_timer_sync(&s->timer);
	flush_work(&s->timer_work);
}

static inline ulong smc(ulong r0, ulong r1, ulong r2, ulong r3)
{
	__asm__ __volatile__(
	"vmcall; \n"
	:"=D"(r0)
	:"a"(TRUSTY_VMCALL_SMC), "D"(r0), "S"(r1), "d"(r2), "b"(r3)
	);
	return r0;
}

static void trusty_fast_call32_remote(void *args)
{
	struct trusty_smc_interface *p_args = args;
	struct device *dev = p_args->dev;
	ulong smcnr = p_args->args[0];
	ulong a0 = p_args->args[1];
	ulong a1 = p_args->args[2];
	ulong a2 = p_args->args[3];
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	BUG_ON(!s);
	BUG_ON(!SMC_IS_FASTCALL(smcnr));
	BUG_ON(SMC_IS_SMC64(smcnr));

	p_args->args[4] = smc(smcnr, a0, a1, a2);
}

s32 trusty_fast_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2)
{
	int cpu = 0;
	int ret = 0;
	struct trusty_smc_interface s;
	s.dev = dev;
	s.args[0] = smcnr;
	s.args[1] = a0;
	s.args[2] = a1;
	s.args[3] = a2;
	s.args[4] = 0;

	ret = smp_call_function_single(cpu, trusty_fast_call32_remote, (void *)&s, 1);

	if (ret) {
		pr_err("%s: smp_call_function_single failed: %d\n", __func__, ret);
	}

	return s.args[4];
}
EXPORT_SYMBOL(trusty_fast_call32);

#ifdef CONFIG_64BIT
s64 trusty_fast_call64(struct device *dev, u64 smcnr, u64 a0, u64 a1, u64 a2)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	BUG_ON(!s);
	BUG_ON(!SMC_IS_FASTCALL(smcnr));
	BUG_ON(!SMC_IS_SMC64(smcnr));

	return smc(smcnr, a0, a1, a2);
}
#endif

static ulong trusty_std_call_inner(struct device *dev, ulong smcnr,
				   ulong a0, ulong a1, ulong a2)
{
	ulong ret;
	int retry = 5;

	dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx)\n",
		__func__, smcnr, a0, a1, a2);
	while (true) {
		ret = smc(smcnr, a0, a1, a2);
		while ((s32)ret == SM_ERR_FIQ_INTERRUPTED)
			ret = smc(SMC_SC_RESTART_FIQ, 0, 0, 0);
		if ((int)ret != SM_ERR_BUSY || !retry)
			break;

		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy, retry\n",
			__func__, smcnr, a0, a1, a2);
		retry--;
	}

	return ret;
}

static void trusty_std_call_inner_wrapper_remote(void *args)
{
	struct trusty_smc_interface *p_args = args;
	struct device *dev = p_args->dev;
	ulong smcnr = p_args->args[0];
	ulong a0 = p_args->args[1];
	ulong a1 = p_args->args[2];
	ulong a2 = p_args->args[3];
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));
	ulong ret;
	unsigned long flags;

	local_irq_save(flags);
	atomic_notifier_call_chain(&s->notifier, TRUSTY_CALL_PREPARE,
					   NULL);
	ret = trusty_std_call_inner(dev, smcnr, a0, a1, a2);
	atomic_notifier_call_chain(&s->notifier, TRUSTY_CALL_RETURNED,
					   NULL);
	local_irq_restore(flags);

	p_args->args[4] = ret;
}

static ulong trusty_std_call_inner_wrapper(struct device *dev, ulong smcnr,
				   ulong a0, ulong a1, ulong a2)
{
	int cpu = 0;
	int ret = 0;
	struct trusty_smc_interface s;
	s.dev = dev;
	s.args[0] = smcnr;
	s.args[1] = a0;
	s.args[2] = a1;
	s.args[3] = a2;
	s.args[4] = 0;

	ret = smp_call_function_single(cpu, trusty_std_call_inner_wrapper_remote, (void *)&s, 1);

	if (ret) {
		pr_err("%s: smp_call_function_single failed: %d\n", __func__, ret);
	}

	return s.args[4];
}

static ulong trusty_std_call_helper(struct device *dev, ulong smcnr,
				    ulong a0, ulong a1, ulong a2)
{
	ulong ret;
	int sleep_time = 1;

	while (true) {
		ret = trusty_std_call_inner_wrapper(dev, smcnr, a0, a1, a2);

		if ((int)ret != SM_ERR_BUSY)
			break;

		if (sleep_time == 256)
			dev_warn(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy\n",
				 __func__, smcnr, a0, a1, a2);
		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy, wait %d ms\n",
			__func__, smcnr, a0, a1, a2, sleep_time);

		msleep(sleep_time);
		if (sleep_time < 1000)
			sleep_time <<= 1;

		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) retry\n",
			__func__, smcnr, a0, a1, a2);
	}

	if (sleep_time > 256)
		dev_warn(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) busy cleared\n",
			 __func__, smcnr, a0, a1, a2);

	return ret;
}

static void trusty_std_call_cpu_idle(struct trusty_state *s)
{
	int ret;

	ret = wait_for_completion_timeout(&s->cpu_idle_completion, HZ * 10);
	if (!ret) {
		pr_warn("%s: timed out waiting for cpu idle to clear, retry anyway\n",
			__func__);
	}
}

/* must set CONFIG_DEBUG_ATOMIC_SLEEP=n
** otherwise mutex_lock() will fail and crash
*/
s32 trusty_std_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2)
{
	int ret;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	BUG_ON(SMC_IS_FASTCALL(smcnr));
	BUG_ON(SMC_IS_SMC64(smcnr));

	if (smcnr != SMC_SC_NOP) {
		mutex_lock(&s->smc_lock);
		reinit_completion(&s->cpu_idle_completion);
	}

	dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) started\n",
		__func__, smcnr, a0, a1, a2);

	ret = trusty_std_call_helper(dev, smcnr, a0, a1, a2);
	while (ret == SM_ERR_INTERRUPTED || ret == SM_ERR_CPU_IDLE) {
		dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) interrupted\n",
			__func__, smcnr, a0, a1, a2);
		if (ret == SM_ERR_CPU_IDLE)
			trusty_std_call_cpu_idle(s);
		ret = trusty_std_call_helper(dev, SMC_SC_RESTART_LAST, 0, 0, 0);
	}
	dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) returned 0x%x\n",
		__func__, smcnr, a0, a1, a2, ret);

	WARN_ONCE(ret == SM_ERR_PANIC, "trusty crashed");

	if (ret == SM_ERR_PANIC)
		trusty_del_lktimer(s);

	if (smcnr == SMC_SC_NOP)
		complete(&s->cpu_idle_completion);
	else
		mutex_unlock(&s->smc_lock);

	return ret;
}
EXPORT_SYMBOL(trusty_std_call32);

int trusty_call_notifier_register(struct device *dev, struct notifier_block *n)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return atomic_notifier_chain_register(&s->notifier, n);
}
EXPORT_SYMBOL(trusty_call_notifier_register);

int trusty_call_notifier_unregister(struct device *dev,
				    struct notifier_block *n)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return atomic_notifier_chain_unregister(&s->notifier, n);
}
EXPORT_SYMBOL(trusty_call_notifier_unregister);

static int trusty_remove_child(struct device *dev, void *data)
{
	dev_dbg(dev, "%s() is called()\n", __func__);
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

ssize_t trusty_version_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return scnprintf(buf, PAGE_SIZE, "%s\n", s->version_str);
}

DEVICE_ATTR(trusty_version, S_IRUSR, trusty_version_show, NULL);

const char *trusty_version_str_get(struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return s->version_str;
}
EXPORT_SYMBOL(trusty_version_str_get);

static void trusty_init_version(struct trusty_state *s, struct device *dev)
{
	int ret;
	int i;
	int version_str_len;

	ret = trusty_fast_call32(dev, SMC_FC_GET_VERSION_STR, -1, 0, 0);
	if (ret <= 0)
		goto err_get_size;

	version_str_len = ret;

	s->version_str = kmalloc(version_str_len + 1, GFP_KERNEL);
	if (!s->version_str)
		goto err_get_size;
	for (i = 0; i < version_str_len; i++) {
		ret = trusty_fast_call32(dev, SMC_FC_GET_VERSION_STR, i, 0, 0);
		if (ret < 0)
			goto err_get_char;
		s->version_str[i] = ret;
	}
	s->version_str[i] = '\0';

	dev_info(dev, "trusty version: %s\n", s->version_str);

	ret = device_create_file(dev, &dev_attr_trusty_version);
	if (ret)
		goto err_create_file;
	return;

err_create_file:
err_get_char:
	kfree(s->version_str);
	s->version_str = NULL;
err_get_size:
	dev_err(dev, "failed to get version: %d\n", ret);
}

u32 trusty_get_api_version(struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return s->api_version;
}
EXPORT_SYMBOL(trusty_get_api_version);

static int trusty_init_api_version(struct trusty_state *s, struct device *dev)
{
	u32 api_version;
	api_version = trusty_fast_call32(dev, SMC_FC_API_VERSION,
					 TRUSTY_API_VERSION_CURRENT, 0, 0);
	if (api_version == SM_ERR_UNDEFINED_SMC)
		api_version = 0;

	if (api_version > TRUSTY_API_VERSION_CURRENT) {
		dev_err(dev, "unsupported api version %u > %u\n",
			api_version, TRUSTY_API_VERSION_CURRENT);
		return -EINVAL;
	}

	dev_info(dev, "selected api version: %u (requested %u)\n",
		 api_version, TRUSTY_API_VERSION_CURRENT);
	s->api_version = api_version;

	return 0;
}

static int trusty_probe(struct platform_device *pdev)
{
	int ret;
	struct trusty_state *s;
	struct device_node *node = pdev->dev.of_node;

	if (!node) {
		dev_err(&pdev->dev, "of_node required\n");
		return -EINVAL;
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		ret = -ENOMEM;
		goto err_allocate_state;
	}
	mutex_init(&s->smc_lock);
	ATOMIC_INIT_NOTIFIER_HEAD(&s->notifier);
	init_completion(&s->cpu_idle_completion);
	platform_set_drvdata(pdev, s);
	s->dev = &pdev->dev;

	trusty_init_version(s, &pdev->dev);

	ret = trusty_init_api_version(s, &pdev->dev);
	if (ret < 0)
		goto err_api_version;

	trusty_init_lktimer(s);
	trusty_configure_lktimer(s,
		PERIODICAL_TIMER, TRUSTY_LKTIMER_INTERVAL);

	return 0;

err_api_version:
	if (s->version_str) {
		device_remove_file(&pdev->dev, &dev_attr_trusty_version);
		kfree(s->version_str);
	}
	device_for_each_child(&pdev->dev, NULL, trusty_remove_child);
	mutex_destroy(&s->smc_lock);
	kfree(s);
err_allocate_state:
	return ret;
}

static int trusty_remove(struct platform_device *pdev)
{
	struct trusty_state *s = platform_get_drvdata(pdev);

	dev_dbg(&(pdev->dev), "%s() is called\n", __func__);

	device_for_each_child(&pdev->dev, NULL, trusty_remove_child);
	mutex_destroy(&s->smc_lock);
	if (s->version_str) {
		device_remove_file(&pdev->dev, &dev_attr_trusty_version);
		kfree(s->version_str);
	}
	trusty_del_lktimer(s);
	kfree(s);
	return 0;
}

static const struct of_device_id trusty_of_match[] = {
	{ .compatible = "android,trusty-smc-v1", },
	{},
};

static struct platform_driver trusty_driver = {
	.probe = trusty_probe,
	.remove = trusty_remove,
	.driver	= {
		.name = "trusty",
		.owner = THIS_MODULE,
		.of_match_table = trusty_of_match,
	},
};

void	trusty_dev_release(struct device *dev)
{
	dev_dbg(dev, "%s() is called()\n", __func__);
	return;
}

static struct device_node trusty_irq_node = {
	.name = "trusty-irq",
	.sibling = NULL,
};

static struct device_node trusty_virtio_node = {
	.name = "trusty-virtio",
	.sibling = &trusty_irq_node,
};

static struct device_node trusty_log_node = {
	.name = "trusty-log",
	.sibling = &trusty_virtio_node,
};


static struct device_node trusty_node = {
	.name = "trusty",
	.child = &trusty_log_node,
};

static struct platform_device trusty_platform_dev = {
	.name = "trusty",
	.id   = -1,
	.num_resources = 0,
	.dev = {
		.release = trusty_dev_release,
		.of_node = &trusty_node,
	},
};
static struct platform_device trusty_platform_dev_log = {
	.name = "trusty-log",
	.id   = -1,
	.num_resources = 0,
	.dev = {
		.release = trusty_dev_release,
		.parent = &trusty_platform_dev.dev,
		.of_node = &trusty_log_node,
	},
};

static struct platform_device trusty_platform_dev_virtio = {
	.name = "trusty-virtio",
	.id   = -1,
	.num_resources = 0,
	.dev = {
		.release = trusty_dev_release,
		.parent = &trusty_platform_dev.dev,
		.of_node = &trusty_virtio_node,
	},
};

static struct platform_device trusty_platform_dev_irq = {
	.name = "trusty-irq",
	.id   = -1,
	.num_resources = 0,
	.dev = {
		.release = trusty_dev_release,
		.parent = &trusty_platform_dev.dev,
		.of_node = &trusty_irq_node,
	},
};

static struct platform_device *trusty_devices[] __initdata = {
	&trusty_platform_dev,
	&trusty_platform_dev_log,
	&trusty_platform_dev_virtio,
	&trusty_platform_dev_irq
};
static int __init trusty_driver_init(void)
{
	int ret = 0;

	ret = platform_add_devices(trusty_devices, ARRAY_SIZE(trusty_devices));
	if (ret) {
		printk(KERN_ERR "%s(): platform_add_devices() failed, ret %d\n", __func__, ret);
		return ret;
	}
	return platform_driver_register(&trusty_driver);
}

static void __exit trusty_driver_exit(void)
{
	platform_driver_unregister(&trusty_driver);
	platform_device_unregister(&trusty_platform_dev);
}

subsys_initcall(trusty_driver_init);
module_exit(trusty_driver_exit);

MODULE_LICENSE("GPL");

