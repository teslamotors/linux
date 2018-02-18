/*
 * drivers/extcon/extcon-adc-jack.c
 *
 * Analog Jack extcon driver with ADC-based detection capability.
 *
 * Copyright (C) 2012 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * Modified for calling to IIO to get adc by <anish.singh@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/iio/consumer.h>
#include <linux/extcon/extcon-adc-jack.h>
#include <linux/extcon.h>
#include <linux/of.h>
#include <linux/of_irq.h>

/**
 * struct adc_jack_data - internal data for adc_jack device driver
 * @edev:		extcon device.
 * @cable_names:	list of supported cables.
 * @num_cables:		size of cable_names.
 * @adc_conditions:	list of adc value conditions.
 * @num_conditions:	size of adc_conditions.
 * @irq:		irq number of attach/detach event (0 if not exist).
 * @handling_delay:	interrupt handler will schedule extcon event
 *			handling at handling_delay jiffies.
 * @handler:		extcon event handler called by interrupt handler.
 * @chan:		iio channel being queried.
 */
struct adc_jack_data {
	struct device *dev;
	struct extcon_dev *edev;

	const char **cable_names;
	int num_cables;
	struct adc_jack_cond *adc_conditions;
	int num_conditions;

	int irq;
	unsigned long handling_delay; /* in jiffies */
	int debounce_jiffies;
	struct delayed_work handler;
	struct timer_list timer;

	struct iio_channel *chan;
};

static void adc_jack_handler(struct work_struct *work)
{
	struct adc_jack_data *data = container_of(to_delayed_work(work),
			struct adc_jack_data,
			handler);
	struct adc_jack_cond *def = NULL;
	u32 state = 0;
	int ret, adc_val;
	int i;
	int cindex = 0;

	ret = iio_read_channel_raw(data->chan, &adc_val);
	if (ret < 0) {
		dev_err(data->dev, "read channel() error: %d\n", ret);
		return;
	}

	/* Get state from adc value with adc_conditions */
	for (i = 0; i < data->num_conditions; i++) {
		def = &data->adc_conditions[i];
		if (!def->state)
			break;
		if (def->min_adc <= adc_val && def->max_adc >= adc_val) {
			state = def->state;
			cindex = ffs(state) - 1;
			break;
		}
	}

	if (state)
		dev_info(data->dev,
			"ADC Lower:Value:Upper = %d:%d:%d, Cable:%d\n",
			def->min_adc, adc_val, def->max_adc, cindex);

	/* if no def has met, it means state = 0 (no cables attached) */
	extcon_set_state(data->edev, state);
	dev_info(data->dev, "ADC read %d Cable State 0x%02X: %s cable\n",
			 adc_val, state,
			(state) ? data->cable_names[cindex]: "NO");
}

static void ecx_extcon_notifier_timer(unsigned long _data)
{
	struct adc_jack_data *data = (struct adc_jack_data *)_data;

	schedule_delayed_work(&data->handler, data->handling_delay);
}

static irqreturn_t adc_jack_irq_thread(int irq, void *_data)
{
	struct adc_jack_data *data = _data;

	if (data->debounce_jiffies)
		mod_timer(&data->timer, jiffies + data->debounce_jiffies);
	else
		queue_delayed_work(system_power_efficient_wq,
				   &data->handler, data->handling_delay);

	return IRQ_HANDLED;
}

static struct adc_jack_pdata *of_get_platform_data(
		struct platform_device *pdev)
{
	struct adc_jack_pdata *pdata;
	struct device_node *np = pdev->dev.of_node;
	struct property *prop;
	const char *supply;
	u32 pval;
	int ret;
	u32 state, min_adc, max_adc;
	int nstates, ncables;
	int i;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "extcon-adc-jack,name", &pdata->name);
	if (!pdata->name)
		pdata->name = np->name;

	ret = of_property_read_string_index(np, "io-channel-names", 0,
				&pdata->consumer_channel);
	if (ret < 0)
		return ERR_PTR(ret);

	ret = of_property_read_u32(np, "extcon-adc-jack,irq-flags", &pval);
	if (!ret)
		pdata->irq_flags = pval;

	ret = of_property_read_u32(np, "extcon-adc-jack,handling-delay", &pval);
	if (!ret)
		pdata->handling_delay_ms = pval;

	ret = of_property_read_u32(np, "extcon-adc-jack,debounce-ms", &pval);
	if (!ret)
		pdata->debounce_ms = pval;

	nstates = of_property_count_u32(np, "extcon-adc-jack,states");
	if (nstates < 0)
		return ERR_PTR(nstates);
	if (!nstates || (nstates % 3))
		return ERR_PTR(-EINVAL);

	nstates = nstates/3;

	pdata->adc_conditions = devm_kzalloc(&pdev->dev, (nstates + 1) *
				sizeof(*pdata->adc_conditions), GFP_KERNEL);
	if (!pdata->adc_conditions)
		return ERR_PTR(-ENOMEM);
	for (i = 0; i < nstates; ++i) {
		min_adc = 0;
		max_adc = 0;
		state = 0;

		of_property_read_u32_index(np, "extcon-adc-jack,states",
					i * 3 + 0, &state);
		of_property_read_u32_index(np, "extcon-adc-jack,states",
					i * 3 + 1, &min_adc);
		of_property_read_u32_index(np, "extcon-adc-jack,states",
					i * 3 + 2, &max_adc);

		pdata->adc_conditions[i].state = state;
		pdata->adc_conditions[i].min_adc = (int) min_adc;
		pdata->adc_conditions[i].max_adc = (int) max_adc;
	};

	ncables = of_property_count_strings(np, "extcon-adc-jack,cable-names");
	if (ncables < 0)
		return ERR_PTR(ncables);

	pdata->cable_names = devm_kzalloc(&pdev->dev, (ncables + 1) *
				sizeof(*pdata->cable_names), GFP_KERNEL);
	if (!pdata->cable_names)
		return ERR_PTR(-ENOMEM);

	i = 0;
	of_property_for_each_string(np, "extcon-adc-jack,cable-names", prop, supply)
		pdata->cable_names[i++] = supply;
	pdata->cable_names[i++] = NULL;

	return pdata;
}

static int adc_jack_probe(struct platform_device *pdev)
{
	struct adc_jack_data *data;
	struct adc_jack_pdata *pdata = dev_get_platdata(&pdev->dev);
	int i, err = 0;

	if (!pdata && pdev->dev.of_node) {
		pdata = of_get_platform_data(pdev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	if (!pdata)
		return -EINVAL;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (!pdata->cable_names) {
		dev_err(&pdev->dev, "error: cable_names not defined.\n");
		return -EINVAL;
	}

	data->dev = &pdev->dev;
	data->edev = devm_extcon_dev_allocate(&pdev->dev, pdata->cable_names);
	if (IS_ERR(data->edev)) {
		dev_err(&pdev->dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}
	data->edev->name = pdata->name;
	data->cable_names = pdata->cable_names;

	/* Check the length of array and set num_cables */
	for (i = 0; data->edev->supported_cable[i]; i++)
		;
	if (i == 0 || i > SUPPORTED_CABLE_MAX) {
		dev_err(&pdev->dev, "error: pdata->cable_names size = %d\n",
				i - 1);
		return -EINVAL;
	}
	data->num_cables = i;

	if (!pdata->adc_conditions ||
			!pdata->adc_conditions[0].state) {
		dev_err(&pdev->dev, "error: adc_conditions not defined.\n");
		return -EINVAL;
	}
	data->adc_conditions = pdata->adc_conditions;

	/* Check the length of array and set num_conditions */
	for (i = 0; data->adc_conditions[i].state; i++)
		;
	data->num_conditions = i;

	data->chan = iio_channel_get(&pdev->dev, pdata->consumer_channel);
	if (IS_ERR(data->chan))
		return PTR_ERR(data->chan);

	data->handling_delay = msecs_to_jiffies(pdata->handling_delay_ms);
	data->debounce_jiffies = msecs_to_jiffies(pdata->debounce_ms);

	INIT_DEFERRABLE_WORK(&data->handler, adc_jack_handler);
	setup_timer(&data->timer,
		ecx_extcon_notifier_timer, (unsigned long)data);

	platform_set_drvdata(pdev, data);

	err = devm_extcon_dev_register(&pdev->dev, data->edev);
	if (err)
		return err;

	data->irq = platform_get_irq(pdev, 0);
	if (data->irq < 0)
		data->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (data->irq < 0) {
		dev_err(&pdev->dev, "platform_get_irq failed\n");
		return -ENODEV;
	}

	err = request_any_context_irq(data->irq, adc_jack_irq_thread,
			pdata->irq_flags, pdata->name, data);

	if (err < 0) {
		dev_err(&pdev->dev, "error: irq %d\n", data->irq);
		return err;
	}

	device_init_wakeup(data->dev, true);
	adc_jack_handler(&data->handler.work);

	return 0;
}

static int adc_jack_remove(struct platform_device *pdev)
{
	struct adc_jack_data *data = platform_get_drvdata(pdev);

	free_irq(data->irq, data);
	cancel_work_sync(&data->handler.work);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int adc_jack_suspend(struct device *dev)
{
	struct adc_jack_data *data = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&data->handler);
	if (device_may_wakeup(data->dev))
		enable_irq_wake(data->irq);

	return 0;
}

static int adc_jack_resume(struct device *dev)
{
	struct adc_jack_data *data = dev_get_drvdata(dev);

	if (device_may_wakeup(data->dev))
		disable_irq_wake(data->irq);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(adc_jack_pm_ops, adc_jack_suspend, adc_jack_resume);

static struct of_device_id of_adc_jack_tbl[] = {
	{ .compatible = "extcon-adc-jack", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_adc_jack_tbl);

static struct platform_driver adc_jack_driver = {
	.probe          = adc_jack_probe,
	.remove         = adc_jack_remove,
	.driver         = {
		.name   = "adc-jack",
		.owner  = THIS_MODULE,
		.of_match_table = of_adc_jack_tbl,
		.pm = &adc_jack_pm_ops,
	},
};

module_platform_driver(adc_jack_driver);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("ADC Jack extcon driver");
MODULE_LICENSE("GPL v2");
