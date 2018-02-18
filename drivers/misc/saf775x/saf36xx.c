/*
 * saf36xx.c -- SAF36XX Soc Audio driver
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/ioctl.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-tegra.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <asm/mach-types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>

#define SAF36XX_WATCHDOG_TIMEOUT msecs_to_jiffies(10000)

static struct spi_device *codec_priv;

struct saf36xx_priv {
	struct mutex mutex;
	void *control_data;
	int saturn_rst_gpio;
	int sat_spi_int_gpio;
	wait_queue_head_t wait;
	int boot_state;
	u8 *read_buf;
	size_t read_buf_idx;
	size_t read_buf_sz;
};

enum {
	SAF36XX_RESET_IOCTL = _IO(0xF5, 0x01),
	SAF36XX_GET_INTERRUPT_STATE = _IO(0xF5, 0x02),
	SAF36XX_ENTER_BOOT_STATE = _IO(0xF5, 0x03),
	SAF36XX_ENTER_COMM_STATE = _IO(0xF5, 0x04),
};

static int saturn_chip_reset(void)
{
	int err = 0;
	struct saf36xx_priv *saf36xx =
			(struct saf36xx_priv *)spi_get_drvdata(codec_priv);

	if (gpio_is_valid(saf36xx->saturn_rst_gpio)) {
		gpio_set_value_cansleep(saf36xx->saturn_rst_gpio, 0);
		usleep_range(7000, 8000);
		gpio_set_value_cansleep(saf36xx->saturn_rst_gpio, 1);
		usleep_range(10000, 11000);
	} else {
		pr_info("reset saturn GPIO invalid, gpio= %d",
				saf36xx->saturn_rst_gpio);
		err = -EBADF;
	}
	return err;
}

static void saturn_chip_get_gpios(void)
{
	struct device_node *np;
	int err;
	struct saf36xx_priv *saf36xx =
			(struct saf36xx_priv *)spi_get_drvdata(codec_priv);

	np = of_find_compatible_node(NULL, NULL, "nvidia,tegra-saf36xx");
	if (NULL != np) {
		saf36xx->saturn_rst_gpio =
				of_get_named_gpio(np, "sat-rst-gpio", 0);
		pr_info("saf36xx: rst-gpio:%d\n", saf36xx->saturn_rst_gpio);
		if (!gpio_is_valid(saf36xx->saturn_rst_gpio)) {
			pr_info("reset saturn GPIO get name failed, gpio= %d\n",
					saf36xx->saturn_rst_gpio);
		} else {
			err = gpio_request(saf36xx->saturn_rst_gpio,
					"sat_rst_gpio");
			if (err)
				pr_err("reset saturn GPIO failed ,err=%d\n",
					err);
			else
				gpio_direction_output(
					saf36xx->saturn_rst_gpio, 1);
		}

		saf36xx->sat_spi_int_gpio =
				of_get_named_gpio(np, "sat-spi-int-gpio", 0);
		pr_info("saf36xx: int-gpio:%d\n", saf36xx->sat_spi_int_gpio);
		if (!gpio_is_valid(saf36xx->sat_spi_int_gpio)) {
			pr_info("saturn int GPIO get name failed, gpio = %d\n",
				saf36xx->sat_spi_int_gpio);
		} else {
			err = gpio_request(saf36xx->sat_spi_int_gpio,
					"sat_spi_int_gpio");
			if (err)
				pr_err("saturn GPIO request failed, err=%d\n",
				err);
			else
				gpio_direction_input(saf36xx->sat_spi_int_gpio);
		}
	} else {
		pr_err("reset saturn GPIO get node failed, np= NULL\n");
	}
}

static long saf36xx_hwdep_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct saf36xx_priv *saf36xx =
			(struct saf36xx_priv *)spi_get_drvdata(codec_priv);

	switch (cmd) {
	case SAF36XX_RESET_IOCTL:
		return saturn_chip_reset();
	/* Used for debugging the interrupt GPIO from userspace */
	case SAF36XX_GET_INTERRUPT_STATE:
		return gpio_get_value_cansleep(saf36xx->sat_spi_int_gpio);
	case SAF36XX_ENTER_BOOT_STATE:
		saf36xx->boot_state = 1;
		return 0;
	case SAF36XX_ENTER_COMM_STATE:
		saf36xx->boot_state = 0;
		return 0;
	default:
		return -EFAULT;
	}
}

static ssize_t saf36xx_hwdep_write(struct file *filp,
				   const char __user *_buf,
				   size_t size,
				   loff_t *off_)
{
	char *data;
	int ret = 0, mode = 0;
	struct saf36xx_priv *saf36xx =
			(struct saf36xx_priv *)spi_get_drvdata(codec_priv);

	if ((!saf36xx->boot_state) &&
		(gpio_get_value_cansleep(saf36xx->sat_spi_int_gpio) == 0))
		return -EAGAIN;

	data = devm_kzalloc(&codec_priv->dev, sizeof(*data) * size, GFP_KERNEL);

	if (copy_from_user(data, _buf, sizeof(*data) * size)) {
		dev_err(&codec_priv->dev, "Error copying user data");
		devm_kfree(&codec_priv->dev, data);
		ret = -EFAULT;
		goto err_copy;
	}
	mode = saf36xx->boot_state == 1 ? 0 : 1;
	/* If we're in boot state, wait for GPIO to be low, otherwise high */
	ret = wait_event_interruptible_timeout(saf36xx->wait,
		(gpio_get_value_cansleep(saf36xx->sat_spi_int_gpio) == mode),
		SAF36XX_WATCHDOG_TIMEOUT);
	if (ret == 0) {
		dev_err(&codec_priv->dev, "Saturn watchdog timeout (write)\n");
		devm_kfree(&codec_priv->dev, data);
		return -ETIMEDOUT;
	}
	mutex_lock(&saf36xx->mutex);

	ret = spi_write(saf36xx->control_data, data, size);

	if (ret)
		dev_err(&codec_priv->dev, "spi_write returned: %d\n", ret);

	mdelay(10);
	mutex_unlock(&saf36xx->mutex);

err_copy:
	devm_kfree(&codec_priv->dev, data);
	ret = saf36xx->boot_state;
end:
	return ret;
}



static ssize_t saf36xx_hwdep_read(struct file *filp,
				  char __user *_buf,
				  size_t size,
				  loff_t *off)
{
	char *data;
	int ret = 0;
	struct saf36xx_priv *saf36xx =
			(struct saf36xx_priv *)spi_get_drvdata(codec_priv);

	/* When in boot state, wait until the interrupt pin goes low until
	 * reading */
	if (saf36xx->boot_state == 1) {
		data = devm_kzalloc(&codec_priv->dev,
				sizeof(*data) * size,
				GFP_KERNEL);
	    if (!data) {
		dev_err(&codec_priv->dev,
			"Failed to allocate memory for write buffer");
		ret = -ENOMEM;
		goto end;
	    }
	    ret = wait_event_interruptible_timeout(saf36xx->wait,
		    (gpio_get_value_cansleep(saf36xx->sat_spi_int_gpio) == 0),
		    SAF36XX_WATCHDOG_TIMEOUT);
	    if (ret == 0) {
		dev_err(&codec_priv->dev,
			"Saturn watchdog timeout (read)\n");
		devm_kfree(&codec_priv->dev, data);
		return -ETIMEDOUT;
	    }
	    mutex_lock(&saf36xx->mutex);
	    ret = spi_read(saf36xx->control_data, data, size);
	    if (ret)
		dev_err(&codec_priv->dev, "spi_read returned: %d\n", ret);
	    mdelay(10);
	    mutex_unlock(&saf36xx->mutex);
	    if (ret)
		goto err_read;
	    if (copy_to_user(_buf, data, sizeof(*data) * size))
		ret = -EFAULT;
err_read:
	    devm_kfree(&codec_priv->dev, data);
	} else {
	    /* When in API mode, read an entire message into saf36xx->read_buf,
	     * and read from this buffer when processing read() requests from
	     * user space. A low interrupt pin means there is more data. If
	     * there is no more data, return an error to the user to prevent
	     * blocking */
	    mutex_lock(&saf36xx->mutex);
	    /* Check if entire buffer has been read */
	    if (saf36xx->read_buf_idx >= saf36xx->read_buf_sz &&
		saf36xx->read_buf_sz > 0) {
		devm_kfree(&codec_priv->dev, saf36xx->read_buf);
		saf36xx->read_buf_sz = 0;
		saf36xx->read_buf_idx = 0;
	    }

	    /* Need to fill a new buffer with data from SPI */
	    if (saf36xx->read_buf_sz == 0) {
		if (gpio_get_value_cansleep(saf36xx->sat_spi_int_gpio) == 0) {
			static const int header_sz = 4;
			static const int crc_sz = 2;
			u8 header[4] = { 0 };

			ret = spi_read(saf36xx->control_data,
			   header, header_sz);
			if (ret < 0) {
				dev_err(&codec_priv->dev,
				    "Failed to read message header\n");
				ret = -EFAULT; /* TODO: device error here.. */
			}

			/* All messages must begin with 0xAD */
			if (ret == 0 && header[0] != 0xAD) {
				pr_err("Found junk data .Discarding\n");
				ret = -EAGAIN;
			} else {
				u16 msg_sz = ((header[2] << 8) | header[3])
				+ crc_sz;
				size_t buf_sz = sizeof(u8)
					* (header_sz + msg_sz);
				dev_dbg(&codec_priv->dev,
				    "Allocating new buffer of size %zu\n",
				    buf_sz);
				saf36xx->read_buf = devm_kzalloc(
					&codec_priv->dev, buf_sz,
					GFP_KERNEL);
				if (saf36xx->read_buf) {
					saf36xx->read_buf_sz = buf_sz;
					saf36xx->read_buf_idx = 0;
					memcpy(saf36xx->read_buf, header,
					    header_sz);
					ret = spi_read(saf36xx->control_data,
					    saf36xx->read_buf + header_sz,
					    msg_sz);
				} else {
					pr_err("Error allocating buffer\n");
					ret = -ENOMEM;
				}
			}
		} else {
			/* We have no data, and none available for reading */
			ret = -EAGAIN;
		}
	    }

	    /* The buffer is not empty. Return buffered data */
	    if (ret == 0 && saf36xx->read_buf_sz > 0) {
		size_t remaining;
		size_t copy_sz;

		remaining = saf36xx->read_buf_sz - saf36xx->read_buf_idx;
		copy_sz = min(remaining, size);

		if (copy_to_user(_buf,
		    saf36xx->read_buf + saf36xx->read_buf_idx,
		    sizeof(u8) * copy_sz)) {
			ret = -EFAULT;
		} else {
		    saf36xx->read_buf_idx += copy_sz;
		}
	    }

	    mutex_unlock(&saf36xx->mutex);
	}

end:
	return ret;
}

static irqreturn_t saf36xx_sat_spi_interrupt(int irq, void *data)
{
	struct saf36xx_priv *saf36xx = (struct saf36xx_priv *)data;

	if (gpio_get_value_cansleep(saf36xx->sat_spi_int_gpio) == 0)
		wake_up(&saf36xx->wait);

	return IRQ_HANDLED;
}

static int saf36xx_ioctl_open(struct inode *inp, struct file *filp)
{
	return 0;
}

static int saf36xx_ioctl_release(struct inode *inp, struct file *filp)
{
	return 0;
}

static int saf36xx_ioctl_major;
static struct cdev saf36xx_ioctl_cdev;
static struct class *saf36xx_ioctl_class;

static const struct file_operations saf36xx_ioctl_fops = {
	.owner = THIS_MODULE,
	.open = saf36xx_ioctl_open,
	.release = saf36xx_ioctl_release,
	.unlocked_ioctl = saf36xx_hwdep_ioctl,
	.write = saf36xx_hwdep_write,
	.read = saf36xx_hwdep_read,
};

static void saf36xx_ioctl_cleanup(void)
{
	cdev_del(&saf36xx_ioctl_cdev);
	device_destroy(saf36xx_ioctl_class, MKDEV(saf36xx_ioctl_major, 0));

	if (saf36xx_ioctl_class)
		class_destroy(saf36xx_ioctl_class);

	unregister_chrdev_region(MKDEV(saf36xx_ioctl_major, 0), 1);
}

static int saf36xx_hwdep_create(void)
{
	int result;
	int ret = -ENODEV;
	dev_t saf36xx_ioctl_dev;

	result = alloc_chrdev_region(&saf36xx_ioctl_dev, 0,
			1, "saf36xx_hwdep");
	if (result < 0)
		goto fail_err;

	saf36xx_ioctl_major = MAJOR(saf36xx_ioctl_dev);
	cdev_init(&saf36xx_ioctl_cdev, &saf36xx_ioctl_fops);

	saf36xx_ioctl_cdev.owner = THIS_MODULE;
	saf36xx_ioctl_cdev.ops = &saf36xx_ioctl_fops;
	result = cdev_add(&saf36xx_ioctl_cdev, saf36xx_ioctl_dev, 1);
	if (result < 0)
		goto fail_chrdev;

	saf36xx_ioctl_class = class_create(THIS_MODULE, "saf36xx_hwdep");

	if (IS_ERR(saf36xx_ioctl_class)) {
		pr_err("saf36xx_hwdep: device class file already in use.\n");
		saf36xx_ioctl_cleanup();
		return PTR_ERR(saf36xx_ioctl_class);
	}

	device_create(saf36xx_ioctl_class, NULL,
			MKDEV(saf36xx_ioctl_major, 0),
				NULL, "%s", "saf36xx_hwdep");
	return 0;

fail_chrdev:
	unregister_chrdev_region(saf36xx_ioctl_dev, 1);

fail_err:
	return ret;
}

static int saf36xx_hwdep_cleanup(void)
{
	saf36xx_ioctl_cleanup();
	return 0;
}

static int saf36xx_spi_probe(struct spi_device *spi)
{
	struct saf36xx_priv *saf36xx;
	int irq;
	int ret;

	codec_priv = spi;
	saf36xx = devm_kzalloc(&spi->dev, sizeof(struct saf36xx_priv),
			GFP_KERNEL);

	saf36xx->control_data = spi;
	spi_set_drvdata(spi, saf36xx);
	saturn_chip_get_gpios();

	mutex_init(&saf36xx->mutex);
	init_waitqueue_head(&saf36xx->wait);
	saf36xx->boot_state = 1;

	saf36xx_hwdep_create();

	irq = gpio_to_irq(saf36xx->sat_spi_int_gpio);
	ret = request_threaded_irq(irq, NULL, saf36xx_sat_spi_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					dev_name(&spi->dev), saf36xx);
	if (ret < 0) {
		dev_err(&spi->dev, "Cannot request irq %d for Fault (%d)\n",
				irq, ret);
		return ret;
	}

	return 0;
}

static int saf36xx_spi_remove(struct spi_device *spi)
{
	struct saf36xx_priv *saf36xx =
		(struct saf36xx_priv *)spi_get_drvdata(spi);

	devm_kfree(&spi->dev, saf36xx);
	gpio_free(saf36xx->saturn_rst_gpio);
	gpio_free(saf36xx->sat_spi_int_gpio);
	free_irq(gpio_to_irq(saf36xx->sat_spi_int_gpio), NULL);
	saf36xx_hwdep_cleanup();
	return 0;
}

static struct spi_driver saf36xx_spi_driver = {
	.driver = {
		.name = "saf36xx",
		.owner = THIS_MODULE,
	},
	.probe = saf36xx_spi_probe,
	.remove = saf36xx_spi_remove,
};

module_spi_driver(saf36xx_spi_driver);

MODULE_AUTHOR("Gaurav Tendolkar <gtendolkar@nvidia.com>");
MODULE_DESCRIPTION("SAF36XX Soc Audio driver");
MODULE_LICENSE("GPL");

