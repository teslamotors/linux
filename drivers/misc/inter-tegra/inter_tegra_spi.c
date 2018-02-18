/*
 * drivers/misc/inter_tegra_spi.c
 *
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/jiffies.h>
#include <linux/thermal.h>

#include "inter_tegra_spi.h"

static int inter_tegra_spi_xfer(struct spi_device *spi,
			u8 *txbuf, u8 *rxbuf, int count, int timeout)
{
	struct spi_message	m;
	struct spi_transfer	t = {
			.tx_buf		= txbuf,
			.rx_buf		= rxbuf,
			.len		= count,
			.delay_usecs	= timeout,
		};
	ssize_t status = 0;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(spi, &m);
	if (status < 0)
		return (int)status;

	return 0;
}

static u8 make_checksum(void *data, int num)
{
	int i;
	unsigned int sum = 0;
	u8 *ptr = data;

	for (i = 0; i < num; i++)
		sum += *ptr++;

	return (u8)((~sum + 1) & 0xFF);
}

static int
inter_tegra_send(struct inter_tegra_data *inter_tegra, int type)
{
	struct inter_tegra_pkt txbuf;
	struct inter_tegra_pkt rxbuf;
	int err = 0;
	int pkt_length = 0;
	u8 rx_check_sum;

	memset(&txbuf, 0, sizeof(txbuf));
	memset(&rxbuf, 0, sizeof(rxbuf));

	pkt_length = sizeof(txbuf);

	txbuf.head = HEAD;
	switch (type) {
	case TYPE_TEMP:
		txbuf.cmd = CMD_TEMP;
		txbuf.pkt_data.value = inter_tegra->tx_temp;
		break;
	default:
		dev_err(&inter_tegra->spi->dev, "cmd err!\n");
		return -1;
	}
	txbuf.len = sizeof(txbuf.pkt_data);
	txbuf.check_sum = make_checksum(&txbuf.pkt_data,
						sizeof(txbuf.pkt_data));

	err = inter_tegra_spi_xfer(inter_tegra->spi,
					(u8 *)&txbuf, (u8 *)&rxbuf,
					pkt_length, 0);
	if (err) {
		dev_err(&inter_tegra->spi->dev, "tx spi err!\n");
		return -1;
	}

	if (rxbuf.head != HEAD || rxbuf.cmd != CMD_SUCCESS) {
		dev_err(&inter_tegra->spi->dev, "tx receive packet err!\n");
		return -1;
	}

	rx_check_sum = make_checksum(&rxbuf.pkt_data, sizeof(rxbuf.pkt_data));

	if (rxbuf.check_sum != rx_check_sum) {
		dev_err(&inter_tegra->spi->dev, "tx receive checksum err!\n");
		return -1;
	}
	return 0;
}

static int
inter_tegra_receive(struct inter_tegra_data *inter_tegra)
{
	struct inter_tegra_pkt txbuf;
	struct inter_tegra_pkt rxbuf;
	int err = 0;
	int pkt_length = 0;
	u8 rx_check_sum;

	memset(&txbuf, 0, sizeof(txbuf));
	memset(&rxbuf, 0, sizeof(rxbuf));

	pkt_length = sizeof(txbuf);

	txbuf.head = HEAD;
	txbuf.cmd = CMD_SUCCESS;
	txbuf.len = sizeof(txbuf.pkt_data);
	txbuf.pkt_data.value = 0;
	txbuf.check_sum = make_checksum(&txbuf.pkt_data,
						sizeof(txbuf.pkt_data));

	err = inter_tegra_spi_xfer(inter_tegra->spi,
					(u8 *)&txbuf, (u8 *)&rxbuf,
					pkt_length,
					inter_tegra->receive_timeout_ms);
	if (err) {
		dev_err(&inter_tegra->spi->dev, "rx spi err!\n");
		return -1;
	}

	if (rxbuf.head != HEAD) {
		dev_err(&inter_tegra->spi->dev, "rx receive packet err!\n");
		return -1;
	}

	rx_check_sum = make_checksum(&rxbuf.pkt_data, sizeof(rxbuf.pkt_data));

	if (rxbuf.check_sum != rx_check_sum) {
		dev_err(&inter_tegra->spi->dev, "rx receive checksum err!\n");
		return -1;
	}

	switch (rxbuf.cmd) {
	case CMD_TEMP:
		inter_tegra->rx_temp = rxbuf.pkt_data.value;
		break;
	default:
		dev_err(&inter_tegra->spi->dev, "cmd err!\n");
		return -1;
	}
	return 0;
}

static int inter_tegra_read_thread(void *data)
{
	struct inter_tegra_data *inter_tegra =
		(struct inter_tegra_data *)data;
	int err = 0;

	while (!kthread_should_stop()) {
		err = inter_tegra_receive(inter_tegra);
		if (!err)
			dev_dbg(&inter_tegra->spi->dev,
				"[inter_tegra_read_thread] rx_temp:%d\n",
				inter_tegra->rx_temp);
		else {
			/* If there is no packet from master, set to 0C */
			inter_tegra->rx_temp = 0;
		}
	}
	return 0;
}

static void inter_tegra_send_work(struct work_struct *work)
{
	struct inter_tegra_data *inter_tegra =
		container_of(work, struct inter_tegra_data, work.work);
	int err = 0;

	mutex_lock(&inter_tegra->tx_mutex);

	err = inter_tegra_send(inter_tegra, TYPE_TEMP);
	if (!err)
		dev_dbg(&inter_tegra->spi->dev,
			"[inter_tegra_send_work] tx_temp:%d\n",
			inter_tegra->tx_temp);

	mutex_unlock(&inter_tegra->tx_mutex);

	schedule_delayed_work(&inter_tegra->work,
			msecs_to_jiffies(inter_tegra->send_period_ms));
}

static int inter_tegra_match(struct thermal_zone_device *thz, void *data)
{
	return (strcmp((char *)data, thz->type) == 0);
}

static int inter_tegra_get_tx_temp(void *data, long *temp)
{
	struct inter_tegra_data *inter_tegra = data;
	struct thermal_zone_device *thz;
	long read_temp = 0;

	thz = thermal_zone_device_find((void *)inter_tegra->send_tz_type,
					inter_tegra_match);

	if (!thz || thz->ops->get_temp == NULL ||
		thz->ops->get_temp(thz, &read_temp))
		read_temp = 0;

	*temp = read_temp;
	inter_tegra->tx_temp = (int)read_temp;
	return 0;
}

static struct thermal_of_sensor_ops inter_tegra_tx_ops = {
	.get_temp = inter_tegra_get_tx_temp,
	.get_trend = NULL,
};

static int inter_tegra_get_rx_temp(void *data, long *temp)
{
	struct inter_tegra_data *inter_tegra = data;

	*temp = (long)inter_tegra->rx_temp;
	return 0;
}

static struct thermal_of_sensor_ops inter_tegra_rx_ops = {
	.get_temp = inter_tegra_get_rx_temp,
	.get_trend = NULL,
};

static ssize_t send_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inter_tegra_data *inter_tegra = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", inter_tegra->send_enable);
}

static ssize_t send_enable_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct inter_tegra_data *inter_tegra = dev_get_drvdata(dev);
	bool is_enable = false;
	int ret;

	ret = strtobool(buf, &is_enable);
	if (ret)
		return ret;

	if (is_enable) {
		mutex_lock(&inter_tegra->mutex);
		if (inter_tegra->send_enable) {
			dev_info(dev, "tx work already enabled\n");
			mutex_unlock(&inter_tegra->mutex);
			return count;
		}
		schedule_delayed_work(&inter_tegra->work,
			msecs_to_jiffies(inter_tegra->send_period_ms));
		inter_tegra->send_enable = 1;
		mutex_unlock(&inter_tegra->mutex);
	} else {
		mutex_lock(&inter_tegra->mutex);
		cancel_delayed_work_sync(&inter_tegra->work);
		inter_tegra->send_enable = 0;
		mutex_unlock(&inter_tegra->mutex);
	}

	return count;
}

static DEVICE_ATTR(send_enable, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		send_enable_show, send_enable_store);

static struct attribute *inter_tegra_tx_attributes[] = {
	&dev_attr_send_enable.attr,
	NULL
};

static const struct attribute_group inter_tegra_tx_attr_group = {
	.attrs = inter_tegra_tx_attributes,
};

static ssize_t receive_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct inter_tegra_data *inter_tegra = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", inter_tegra->receive_enable);
}

static ssize_t receive_enable_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct inter_tegra_data *inter_tegra = dev_get_drvdata(dev);
	bool is_enable = false;
	int ret;

	ret = strtobool(buf, &is_enable);
	if (ret)
		return ret;

	if (is_enable) {
		mutex_lock(&inter_tegra->mutex);
		if (inter_tegra->receive_enable) {
			dev_info(dev, "rx thread already enabled\n");
			mutex_unlock(&inter_tegra->mutex);
			return count;
		}
		inter_tegra->thread = kthread_run(inter_tegra_read_thread,
			(void *)inter_tegra, "inter_tegra_read_thread");
		if (IS_ERR_OR_NULL(inter_tegra->thread)) {
			inter_tegra->thread = NULL;
			mutex_unlock(&inter_tegra->mutex);
			return count;
		}
		inter_tegra->receive_enable = 1;
		mutex_unlock(&inter_tegra->mutex);
	} else {
		mutex_lock(&inter_tegra->mutex);
		if (!inter_tegra->receive_enable) {
			dev_info(dev, "rx thread does not created.\n");
			mutex_unlock(&inter_tegra->mutex);
			return count;
		}
		if (inter_tegra->thread) {
			kthread_stop(inter_tegra->thread);
			inter_tegra->thread = NULL;
		}
		inter_tegra->receive_enable = 0;
		mutex_unlock(&inter_tegra->mutex);
	}

	return count;
}

static DEVICE_ATTR(receive_enable, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		receive_enable_show, receive_enable_store);

static struct attribute *inter_tegra_rx_attributes[] = {
	&dev_attr_receive_enable.attr,
	NULL
};

static const struct attribute_group inter_tegra_rx_attr_group = {
	.attrs = inter_tegra_rx_attributes,
};

static int inter_tegra_spi_probe(struct spi_device *spi)
{
	struct device_node *np = spi->dev.of_node;
	struct inter_tegra_data *inter_tegra;
	int err;
	u32 value;
	struct thermal_zone_device *thz = NULL;

	if (!np) {
		dev_err(&spi->dev, "dev of_node NULL\n");
		return -EINVAL;
	}

	/* Allocate driver data */
	inter_tegra = devm_kzalloc(&spi->dev,
				sizeof(*inter_tegra), GFP_KERNEL);
	if (IS_ERR_OR_NULL(inter_tegra))
		return -ENOMEM;

	/* Initialize the driver inter_tegra */
	inter_tegra->spi = spi;

	err = spi_setup(spi);
	if (err < 0) {
		dev_err(&spi->dev, "spi_setup failed!\n");
		goto error;
	}

	spi_set_drvdata(spi, inter_tegra);

	if (of_property_read_bool(np, "is-master"))
		inter_tegra->is_master = 1;

	mutex_init(&inter_tegra->mutex);

	if (inter_tegra->is_master) {
		mutex_init(&inter_tegra->tx_mutex);

		thz = thermal_zone_of_sensor_register2(&spi->dev, 0,
							inter_tegra,
							&inter_tegra_tx_ops);
		if (IS_ERR(thz)) {
			dev_err(&spi->dev,
				"Failed to register thermal zone device\n");
			err = -EINVAL;
			goto error;
		}
		inter_tegra->tx_thz = thz;

		/* SPI master send period time. */
		err = of_property_read_u32(np, "send_period_ms", &value);
		if (err < 0) {
			dev_info(&spi->dev,
				"no send_period_ms: %d\n", err);
			goto error;
		}

		inter_tegra->send_period_ms = (int)value;
		dev_info(&spi->dev, "send_period_ms: %d\n",
			inter_tegra->send_period_ms);

		INIT_DELAYED_WORK(&(inter_tegra->work),
				inter_tegra_send_work);

		err = of_property_read_string(np, "send_tz_type",
						&inter_tegra->send_tz_type);
		if (err < 0) {
			dev_info(&spi->dev,
				"no send_tz_type: %d\n", err);
			goto error;
		}
		dev_info(&spi->dev, "send_tz_type: %s\n",
			inter_tegra->send_tz_type);

		err = sysfs_create_group(&spi->dev.kobj,
					&inter_tegra_tx_attr_group);
		if (err < 0) {
			dev_info(&spi->dev,
				"sysfs create err=%d\n", err);
			goto error;
		}
		dev_info(&spi->dev,
			"registered inter_tegra_spi driver as master\n");
	} else {
		thz = thermal_zone_of_sensor_register2(&spi->dev, 0,
							inter_tegra,
							&inter_tegra_rx_ops);
		if (IS_ERR(thz)) {
			dev_err(&spi->dev,
				"Failed to register thermal zone device\n");
			err = -EINVAL;
			goto error;
		}
		inter_tegra->rx_thz = thz;

		/* SPI slave receive timeout value. This value need to set
		   larger than send_period_ms of master, 0 is infinite wait.*/
		err = of_property_read_u32(np, "receive_timeout_ms", &value);
		if (err < 0) {
			dev_info(&spi->dev,
				"no receive_timeout_ms: %d\n", err);
			goto error;
		}

		inter_tegra->receive_timeout_ms = (int)value;
		dev_info(&spi->dev, "receive_timeout_ms: %d\n",
			inter_tegra->receive_timeout_ms);

		err = sysfs_create_group(&spi->dev.kobj,
					&inter_tegra_rx_attr_group);
		if (err < 0) {
			dev_info(&spi->dev,
				"sysfs create err=%d\n", err);
			goto error;
		}
		dev_info(&spi->dev,
			"registered inter_tegra_spi driver as slave\n");
	}
	return 0;
error:
	devm_kfree(&spi->dev, (void *)inter_tegra);
	return err;
}

static int inter_tegra_spi_remove(struct spi_device *spi)
{
	struct inter_tegra_data *inter_tegra = spi_get_drvdata(spi);

	if (inter_tegra->is_master) {
		cancel_delayed_work_sync(&inter_tegra->work);
		sysfs_remove_group(&spi->dev.kobj, &inter_tegra_tx_attr_group);
		mutex_destroy(&inter_tegra->tx_mutex);
	} else {
		if (inter_tegra->thread)
			kthread_stop(inter_tegra->thread);
		sysfs_remove_group(&spi->dev.kobj, &inter_tegra_rx_attr_group);
		inter_tegra->thread = NULL;
	}
	mutex_destroy(&inter_tegra->mutex);
	return 0;
}

static void inter_tegra_shutdown(struct spi_device *spi)
{
	struct inter_tegra_data *inter_tegra = spi_get_drvdata(spi);

	if (inter_tegra->is_master) {
		cancel_delayed_work_sync(&inter_tegra->work);
	} else {
		if (inter_tegra->thread)
			kthread_stop(inter_tegra->thread);
		inter_tegra->thread = NULL;
	}
}

#ifdef CONFIG_PM_SLEEP
static int inter_tegra_suspend(struct device *dev)
{
	struct inter_tegra_data *inter_tegra = dev_get_drvdata(dev);

	mutex_lock(&inter_tegra->mutex);
	if (inter_tegra->is_master) {
		cancel_delayed_work_sync(&inter_tegra->work);
	} else {
		if (inter_tegra->receive_enable) {
			if (inter_tegra->thread)
				kthread_stop(inter_tegra->thread);
			inter_tegra->thread = NULL;
		}
	}
	mutex_unlock(&inter_tegra->mutex);
	return 0;
}

static int inter_tegra_resume(struct device *dev)
{
	struct inter_tegra_data *inter_tegra = dev_get_drvdata(dev);

	mutex_lock(&inter_tegra->mutex);
	if (inter_tegra->is_master) {
		if (inter_tegra->send_enable)
			schedule_delayed_work(&inter_tegra->work,
				msecs_to_jiffies(inter_tegra->send_period_ms));
	} else {
		if (inter_tegra->receive_enable) {
			inter_tegra->thread =
				kthread_run(inter_tegra_read_thread,
				(void *)inter_tegra, "inter_tegra_read_thread");
			if (IS_ERR_OR_NULL(inter_tegra->thread)) {
				inter_tegra->thread = NULL;
				dev_err(&inter_tegra->spi->dev,
					"fail to create thread\n");
			}
		}
	}
	mutex_unlock(&inter_tegra->mutex);
	return 0;
}
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(inter_tegra_pm_ops,
			inter_tegra_suspend,
			inter_tegra_resume);

static const struct of_device_id inter_tegra_ids[] = {
	{ .compatible = "inter-tegra-spi", },
	{}
};
MODULE_DEVICE_TABLE(of, inter_tegra_ids);

static struct spi_driver inter_tegra_spi_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "inter_tegra_spi",
		.of_match_table = of_match_ptr(inter_tegra_ids),
		.pm = &inter_tegra_pm_ops,
	},
	.probe		= inter_tegra_spi_probe,
	.remove		= inter_tegra_spi_remove,
	.shutdown	= inter_tegra_shutdown,
};

static int __init inter_tegra_spi_init(void)
{
	return spi_register_driver(&inter_tegra_spi_driver);
}

static void __exit inter_tegra_spi_exit(void)
{
	spi_unregister_driver(&inter_tegra_spi_driver);
}

module_init(inter_tegra_spi_init);
module_exit(inter_tegra_spi_exit);

MODULE_AUTHOR("Hyong Bin Kim <hyongbink@nvidia.com>");
MODULE_DESCRIPTION("Interconnected Tegra SPI driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:inter_tegra_spi");
