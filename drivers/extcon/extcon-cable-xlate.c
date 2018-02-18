/*
 * extcon-cable-xlate: Cable translator based on different cable states.
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/extcon.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>

#define DEFAULT_CABLE_WAITTIME_MS		500
#define EXTCON_XLATE_WAKEUP_TIME		1000

struct ecx_io_cable_states {
	int in_state;
	int in_mask;
	int out_states;
};

struct ecx_io_cable_new_states {
	int last_cable_in_state;
	int current_in_cable_state;
	int in_mask;
	int new_cable_out_state;
	int reschedule_wq;
};

struct ecx_platform_data {
	const char *name;
	struct ecx_io_cable_states *io_states;
	struct ecx_io_cable_new_states *io_new_states;
	int n_io_states;
	int n_io_new_states;
	const char **in_cable_names;
	int n_in_cable;
	const char **out_cable_names;
	int n_out_cable;
	int cable_insert_delay;
	int cable_detect_suspend_delay;
};

struct extcon_cable_xlate;

struct ecx_in_cables {
	struct extcon_cable_xlate *ecx;
	struct extcon_cable *ec_cable;
	struct notifier_block nb;
	struct extcon_specific_cable_nb ec_cable_nb;
};

struct extcon_cable_xlate {
	struct device *dev;
	struct ecx_platform_data *pdata;
	struct extcon_dev *edev;
	struct ecx_in_cables *in_cables;
	struct delayed_work work;
	struct timer_list timer;
	int debounce_jiffies;
	int timer_to_work_jiffies;
	spinlock_t lock;
	struct mutex cable_lock;
	struct wake_lock wake_lock;
	bool extcon_init_done;
	int last_cable_in_state;
	int last_cable_out_state;
	int detect_suspend_jiffies;
};

static int ecx_extcon_notifier(struct notifier_block *self,
		unsigned long event, void *ptr);
static int ecx_init_input_cables(struct extcon_cable_xlate *ecx)
{
	struct ecx_in_cables *in_cables;
	struct extcon_cable *ex_cable;
	int i;
	int ret;

	for (i = 0; i < ecx->pdata->n_in_cable; i++) {
		in_cables = &ecx->in_cables[i];
		if (in_cables->ec_cable)
			continue;

		in_cables->ecx = ecx;
		in_cables->nb.notifier_call = ecx_extcon_notifier;
		ex_cable = extcon_get_extcon_cable(ecx->dev,
						ecx->pdata->in_cable_names[i]);
		if (IS_ERR(ex_cable)) {
			ret = PTR_ERR(ex_cable);
			if (ret != -EPROBE_DEFER)
				dev_err(ecx->dev,
					"extcon get failed for %s: %d\n",
					ecx->pdata->in_cable_names[i], ret);
			return ret;
		};
		in_cables->ec_cable = ex_cable;
	}

	for (i = 0; i < ecx->pdata->n_in_cable; i++) {
		in_cables = &ecx->in_cables[i];
		ret = extcon_register_cable_interest(&in_cables->ec_cable_nb,
				in_cables->ec_cable, &in_cables->nb);
		if (ret < 0) {
			dev_err(ecx->dev, "Cable %s registration failed: %d\n",
				ecx->pdata->in_cable_names[i], ret);
			return ret;
		}
	}
	return 0;
}

static int ecx_attach_cable(struct extcon_cable_xlate *ecx)
{
	struct ecx_in_cables *in_cables;
	int mask_state = 0;
	int all_states = 0;
	int new_state = -1;
	int i;
	int ret;
	int reschedule_wq = 0;

	mutex_lock(&ecx->cable_lock);
	for (i = 0; i < ecx->pdata->n_in_cable; i++) {
		in_cables = &ecx->in_cables[i];

		ret = extcon_get_cable_state_(in_cables->ec_cable->edev,
				in_cables->ec_cable->cable_index);
		if (ret >= 1)
			all_states |= BIT(i);
	}

	if (ecx->pdata->io_new_states) {
		for (i = 0; i < ecx->pdata->n_io_new_states; ++i) {
			mask_state = all_states & ecx->pdata->io_new_states[i].in_mask;
			if (mask_state ==
					ecx->pdata->io_new_states[i].current_in_cable_state) {
					if ((ecx->last_cable_in_state == mask_state) && mask_state) {
						mutex_unlock(&ecx->cable_lock);
						return 0;
					}

					if ((ecx->last_cable_in_state ==
							ecx->pdata->io_new_states[i].last_cable_in_state)
							|| (mask_state == 0)) {

						ecx->last_cable_in_state = mask_state;
						new_state = ecx->pdata->io_new_states[i].new_cable_out_state;
						reschedule_wq = ecx->pdata->io_new_states[i].reschedule_wq;
						break;
					}
			}
		}
	} else {
		for (i = 0; i < ecx->pdata->n_io_states; ++i) {
			mask_state = all_states & ecx->pdata->io_states[i].in_mask;
			if (mask_state == ecx->pdata->io_states[i].in_state) {
				new_state = ecx->pdata->io_states[i].out_states;
				break;
			}
		}
	}

	if (new_state < 0) {
		dev_err(ecx->dev, "Cable state 0x%04x is not defined\n",
			all_states);
		dev_err(ecx->dev,
			"Last cable in state: 0x%04x, mask state: 0x%04x\n",
			ecx->last_cable_in_state, mask_state);
		mutex_unlock(&ecx->cable_lock);
		return -EINVAL;
	}
	if (ecx->last_cable_out_state != new_state) {
		extcon_set_state(ecx->edev, new_state);
		dev_info(ecx->dev, "New cable state 0x%04x\n", new_state);
		if (new_state) {
			i = ffs(new_state) - 1;
			dev_info(ecx->dev, "Cable%d %s is attach\n",
				i, ecx->pdata->out_cable_names[i]);
		} else {
			ecx->last_cable_in_state = 0;
			dev_info(ecx->dev, "No cable attach\n");
		}
	}
	ecx->last_cable_out_state = new_state;
	if (reschedule_wq)
		schedule_delayed_work(&ecx->work, msecs_to_jiffies(1000));
	mutex_unlock(&ecx->cable_lock);
	return 0;
}

static void ecx_cable_state_update_work(struct work_struct *work)
{
	struct extcon_cable_xlate *ecx = container_of(to_delayed_work(work),
					struct extcon_cable_xlate, work);
	int ret;

	if (!ecx->extcon_init_done) {
		ret = ecx_init_input_cables(ecx);
		if (ret < 0) {
			if (ret == -EPROBE_DEFER)
				schedule_delayed_work(&ecx->work,
					msecs_to_jiffies(1000));
			return;
		}
		dev_info(ecx->dev, "Extcon Init success\n");
		ecx->extcon_init_done = true;
		schedule_delayed_work(&ecx->work, msecs_to_jiffies(1000));
		return;
	}
	ecx_attach_cable(ecx);
}

static void ecx_extcon_notifier_timer(unsigned long _data)
{
	struct extcon_cable_xlate *ecx = (struct extcon_cable_xlate *)_data;

	schedule_delayed_work(&ecx->work, ecx->timer_to_work_jiffies);
}

static int ecx_extcon_notifier(struct notifier_block *self,
		unsigned long event, void *ptr)
{
	struct ecx_in_cables *cable = container_of(self,
					struct ecx_in_cables, nb);
	struct extcon_cable_xlate *ecx = cable->ecx;
	unsigned long flags;

	/*Hold wakelock to complete cable detection */
	if (!wake_lock_active(&ecx->wake_lock))
		wake_lock_timeout(&ecx->wake_lock,
					ecx->detect_suspend_jiffies);

	spin_lock_irqsave(&ecx->lock, flags);
	mod_timer(&ecx->timer, jiffies + ecx->debounce_jiffies);
	spin_unlock_irqrestore(&ecx->lock, flags);
	return 0;
}

static struct ecx_platform_data *ecx_get_pdata_from_dt(
	struct platform_device *pdev)
{
	struct ecx_platform_data *pdata;
	struct device_node *np = pdev->dev.of_node;
	u32 pval;
	int ret;
	const char *names;
	struct property *prop;
	int count;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "extcon-name", &pdata->name);
	if (!pdata->name)
		pdata->name = np->name;

	ret = of_property_read_u32(np, "cable-insert-delay", &pval);
	if (!ret)
		pdata->cable_insert_delay = pval;
	else
		pdata->cable_insert_delay = DEFAULT_CABLE_WAITTIME_MS;

	ret = of_property_read_u32(np, "cable-detect-suspend-delay", &pval);
	if (!ret)
		pdata->cable_detect_suspend_delay = pval;
	else
		pdata->cable_detect_suspend_delay = EXTCON_XLATE_WAKEUP_TIME;


	pdata->n_out_cable = of_property_count_strings(np,
					"output-cable-names");
	if (pdata->n_out_cable <= 0) {
		dev_err(&pdev->dev, "Not found output cable names\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->out_cable_names = devm_kzalloc(&pdev->dev,
				(pdata->n_out_cable + 1) *
				sizeof(*pdata->out_cable_names), GFP_KERNEL);
	if (!pdata->out_cable_names)
		return ERR_PTR(-ENOMEM);
	count = 0;
	of_property_for_each_string(np, "output-cable-names", prop, names)
		pdata->out_cable_names[count++] = names;
	pdata->out_cable_names[count] = NULL;


	pdata->n_in_cable = of_property_count_strings(np, "extcon-cable-names");
	if (pdata->n_in_cable <= 0) {
		dev_err(&pdev->dev, "Not found input cable names\n");
		return ERR_PTR(-EINVAL);
	}
	pdata->in_cable_names = devm_kzalloc(&pdev->dev,
				(pdata->n_in_cable + 1) *
				sizeof(*pdata->in_cable_names), GFP_KERNEL);
	if (!pdata->in_cable_names)
		return ERR_PTR(-ENOMEM);
	count = 0;
	of_property_for_each_string(np, "extcon-cable-names", prop, names)
		pdata->in_cable_names[count++] = names;
	pdata->in_cable_names[count] = NULL;


	pdata->n_io_states = of_property_count_u32(np, "cable-states");
	if ((pdata->n_io_states < 3) || (pdata->n_io_states % 3 != 0)) {
		dev_err(&pdev->dev, "not found proper cable state\n");
		goto cable_new_states;
	}
	pdata->n_io_states /= 3;
	pdata->io_states = devm_kzalloc(&pdev->dev, (pdata->n_io_states) *
				sizeof(*pdata->io_states), GFP_KERNEL);
	if (!pdata->io_states)
		return ERR_PTR(-ENOMEM);
	for (count = 0;  count < pdata->n_io_states; ++count) {
		ret = of_property_read_u32_index(np, "cable-states",
				count * 3, &pval);
		if (!ret)
			pdata->io_states[count].in_state = pval;

		ret = of_property_read_u32_index(np, "cable-states",
				count * 3 + 1, &pval);
		if (!ret)
			pdata->io_states[count].in_mask = pval;

		ret = of_property_read_u32_index(np, "cable-states",
				count * 3 + 2, &pval);
		if (!ret)
			pdata->io_states[count].out_states = pval;
	}

cable_new_states:
	pdata->n_io_new_states = of_property_count_u32(np, "cable-new-states");
	if ((pdata->n_io_new_states < 5) || (pdata->n_io_new_states % 5 != 0)) {
		dev_dbg(&pdev->dev, "not found proper cable-new-states\n");
		goto exit;
	}
	pdata->n_io_new_states /= 5;
	pdata->io_new_states = devm_kzalloc(&pdev->dev, (pdata->n_io_new_states) *
				sizeof(*pdata->io_new_states), GFP_KERNEL);
	if (!pdata->io_new_states)
		return ERR_PTR(-ENOMEM);
	for (count = 0;  count < pdata->n_io_new_states; ++count) {
		ret = of_property_read_u32_index(np, "cable-new-states",
				count * 5, &pval);
		if (!ret)
			pdata->io_new_states[count].last_cable_in_state = pval;

		ret = of_property_read_u32_index(np, "cable-new-states",
				count * 5 + 1, &pval);
		if (!ret)
			pdata->io_new_states[count].current_in_cable_state = pval;

		ret = of_property_read_u32_index(np, "cable-new-states",
				count * 5 + 2, &pval);
		if (!ret)
			pdata->io_new_states[count].in_mask = pval;

		ret = of_property_read_u32_index(np, "cable-new-states",
				count * 5 + 3, &pval);
		if (!ret)
			pdata->io_new_states[count].new_cable_out_state = pval;

		ret = of_property_read_u32_index(np, "cable-new-states",
				count * 5 + 4, &pval);
		if (!ret)
			pdata->io_new_states[count].reschedule_wq = pval;
	}
exit:
	if ((pdata->n_io_states < 3) && (pdata->n_io_new_states < 5))
		return ERR_PTR(-EINVAL);
	return pdata;
}
static int ecx_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct extcon_cable_xlate *ecx;
	struct ecx_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (!pdata && pdev->dev.of_node) {
		pdata = ecx_get_pdata_from_dt(pdev);
		if (IS_ERR(pdata)) {
			ret = PTR_ERR(pdata);
			pdata = NULL;
		}
	}

	if (!pdata) {
		dev_err(&pdev->dev, "No platform data, exiting..\n");
		return -ENODEV;
	}

	ecx = devm_kzalloc(&pdev->dev, sizeof(*ecx), GFP_KERNEL);
	if (!ecx)
		return -ENOMEM;

	ecx->in_cables = devm_kzalloc(&pdev->dev, pdata->n_in_cable *
				sizeof(*ecx->in_cables), GFP_KERNEL);
	if (!ecx->in_cables)
		return -ENOMEM;

	ecx->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, ecx);
	spin_lock_init(&ecx->lock);
	mutex_init(&ecx->cable_lock);

	ecx->dev = &pdev->dev;
	ecx->edev = devm_extcon_dev_allocate(&pdev->dev, pdata->out_cable_names);
	if (IS_ERR(ecx->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}
	ecx->edev->name = pdata->name;
	ecx->debounce_jiffies = msecs_to_jiffies(pdata->cable_insert_delay);
	ecx->timer_to_work_jiffies = msecs_to_jiffies(500);
	ecx->detect_suspend_jiffies =
			msecs_to_jiffies(pdata->cable_detect_suspend_delay);
	ecx->pdata = pdata;

	wake_lock_init(&ecx->wake_lock, WAKE_LOCK_SUSPEND,
						"extcon-suspend-lock");

	ret = devm_extcon_dev_register(&pdev->dev, ecx->edev);
	if (ret < 0) {
		dev_err(ecx->dev, "Extcon registration failed: %d\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&ecx->work, ecx_cable_state_update_work);
	setup_timer(&ecx->timer, ecx_extcon_notifier_timer, (unsigned long)ecx);

	ret = ecx_init_input_cables(ecx);
	if (ret < 0) {
		if (ret == -EPROBE_DEFER)
			goto defer_now;
		return ret;
	}

	ecx->extcon_init_done = true;

defer_now:
	if (ecx->extcon_init_done) {
		ecx_cable_state_update_work(&ecx->work.work);
	} else {
		extcon_set_state(ecx->edev, 0);
		schedule_delayed_work(&ecx->work, msecs_to_jiffies(1000));
	}
	dev_info(&pdev->dev, "%s() get success\n", __func__);
	return 0;
}

static int ecx_remove(struct platform_device *pdev)
{
	struct extcon_cable_xlate *ecx = platform_get_drvdata(pdev);

	del_timer_sync(&ecx->timer);
	cancel_delayed_work_sync(&ecx->work);
	return 0;
}

static struct of_device_id ecx_of_match[] = {
	{ .compatible = "extcon-cable-xlate", },
	{},
};
MODULE_DEVICE_TABLE(of, ecx_of_match);

static struct platform_driver ecx_driver = {
	.driver = {
		.name = "extcon-cable-xlate",
		.owner = THIS_MODULE,
		.of_match_table = ecx_of_match,
	},
	.probe = ecx_probe,
	.remove = ecx_remove,
};

static int __init ecx_init(void)
{
	return platform_driver_register(&ecx_driver);
}

static void __exit ecx_exit(void)
{
	platform_driver_unregister(&ecx_driver);
}

subsys_initcall_sync(ecx_init);
module_exit(ecx_exit);

MODULE_DESCRIPTION("Power supply detection through extcon driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
