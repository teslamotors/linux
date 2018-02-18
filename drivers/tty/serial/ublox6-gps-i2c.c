/* u-blox 6 I2C GPS driver
 *
 * Copyright (C) 2015 Felipe F. Tonello <eu@felipetonello.com>
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Driver that enables control of a U-Blox GPS receiver, connected
 * via I2C, through the TTY interface.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>

#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.0"
#define DRIVER_DESC "u-blox I2C GPS driver"

#define UBLOX_GPS_NUM 1 /* Only support 1 GPS at a time */

/* By default u-blox GPS fill its buffer every 1 second (1000 msecs) */
#define READ_TIME 1000
#define FAST_READ_TIME 100

struct ublox_device {
	struct tty_driver *tty_driver;
	struct tty_port tty_port;
	struct i2c_client *i2c_client;
	struct regmap *i2c_regmap;
	struct delayed_work dwork;
	bool is_active;
};

static void ublox_gps_read_worker(struct work_struct *private);

static const struct regmap_config ublox_gps_i2c_regmap_config = {
	.reg_bits = 8,
	.reg_stride = 0,
	.pad_bits = 0,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
	.reg_format_endian = REGMAP_ENDIAN_LITTLE,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,
};

static void ublox_gps_read_worker(struct work_struct *private)
{
	unsigned int gps_buf_size;
	u8 *buf;
	int bytes_pushed;
	int ret;
	struct ublox_device *ublox_dev = (struct ublox_device *)
			container_of(private, struct ublox_device, dwork.work);

	ret = regmap_bulk_read(ublox_dev->i2c_regmap, 0xfd, &gps_buf_size, 2);
	if (ret < 0) {
		dev_warn(&ublox_dev->i2c_client->dev,
			": couldn't read register(0xfd) from GPS.\n");
		/* try one more time */
		goto end;
	}

	/*
	 * The currently available number of bytes is read via the 2 bytes at
	 * 0xfd and 0xfe.
	 */
	gps_buf_size = ((gps_buf_size & 0xff) << 8) |
			((gps_buf_size & 0xff00) >> 8);
	if (gps_buf_size > 0) {
		buf = kcalloc(gps_buf_size, sizeof(*buf), GFP_KERNEL);
		if (!buf)
			/* try one more time */
			goto end;

		do {
			ret = regmap_raw_read(ublox_dev->i2c_regmap, 0xff,
						(char *)buf, gps_buf_size);
			if (ret < 0) {
				dev_warn(&ublox_dev->i2c_client->dev,
					": couldn't read register(0xfd) from GPS.\n");
				kfree(buf);
				/* try one more time */
				goto end;
			}

			dev_dbg(&ublox_dev->i2c_client->dev,
				"%d bytes read from i2c\n", gps_buf_size);

			bytes_pushed = tty_insert_flip_string(
				&ublox_dev->tty_port, buf, gps_buf_size);
			dev_dbg(&ublox_dev->i2c_client->dev,
				"%d bytes pushed to tty buffer\n",
				bytes_pushed);

			gps_buf_size -= bytes_pushed;

			/*
			 * There is a small chance that we need to split the
			 * data over several buffers. If this is the case we
			 * must loop.
			 */
		} while (unlikely(gps_buf_size > 0));

		tty_flip_buffer_push(&ublox_dev->tty_port);

		kfree(buf);
		if (ublox_dev->is_active)
			schedule_delayed_work(&ublox_dev->dwork,
					msecs_to_jiffies(FAST_READ_TIME));
		return;
	}
end:
	/* resubmit the workqueue again */
	if (ublox_dev->is_active)
		schedule_delayed_work(&ublox_dev->dwork,
					msecs_to_jiffies(READ_TIME));
}

static int ublox_gps_serial_open(struct tty_struct *tty, struct file *filp)
{
	struct ublox_device *ublox_dev = (struct ublox_device *)
			container_of(tty->port, struct ublox_device, tty_port);

	return tty_port_open(&ublox_dev->tty_port, tty, filp);
}

static void ublox_gps_serial_close(struct tty_struct *tty, struct file *filp)
{
	struct ublox_device *ublox_dev = (struct ublox_device *)
			container_of(tty->port, struct ublox_device, tty_port);

	tty_port_close(&ublox_dev->tty_port, tty, filp);
}

static int ublox_gps_serial_write(struct tty_struct *tty,
				const unsigned char *buf, int count)
{
	struct ublox_device *ublox_dev = (struct ublox_device *)
			container_of(tty->port, struct ublox_device, tty_port);
	int ret;

	/* Write data */
	ret = regmap_raw_write(ublox_dev->i2c_regmap, 0xff, (char *)buf, count);
	if (ret < 0)
		dev_err(&ublox_dev->i2c_client->dev, "i2c write failed: %d\n",
			ret);
	else
		dev_dbg(&ublox_dev->i2c_client->dev, "%d bytes written\n",
			count);
	if (ublox_dev->is_active)
		schedule_delayed_work(&ublox_dev->dwork,
					msecs_to_jiffies(FAST_READ_TIME));

	return (ret == 0 ? count : ret);
}

static int ublox_gps_write_room(struct tty_struct *tty)
{
	/* arbitrary value */
	return 1024;
}

static const struct tty_operations ublox_gps_serial_ops = {
	.open = ublox_gps_serial_open,
	.close = ublox_gps_serial_close,
	.write = ublox_gps_serial_write,
	.write_room = ublox_gps_write_room,
};

static void ublox_gps_shutdown(struct tty_port *port)
{
	struct ublox_device *ublox_dev = (struct ublox_device *)
			container_of(port, struct ublox_device, tty_port);

	ublox_dev->is_active = false;
	cancel_delayed_work(&ublox_dev->dwork);
	flush_work(&ublox_dev->dwork.work);
}

static int ublox_gps_activate(struct tty_port *port, struct tty_struct *tty)
{
	struct ublox_device *ublox_dev = (struct ublox_device *)
			container_of(port, struct ublox_device, tty_port);

	ublox_dev->is_active = true;
	schedule_delayed_work(&ublox_dev->dwork, 0);

	return 0;
}

static const struct tty_port_operations ublox_gps_port_ops = {
	.shutdown = ublox_gps_shutdown,
	.activate = ublox_gps_activate,
};

static int ublox_gps_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int result = 0;
	struct tty_driver *ublox_gps_tty_driver;
	struct ublox_device *ublox_dev;

	ublox_gps_tty_driver = tty_alloc_driver(UBLOX_GPS_NUM, 0);

	if (IS_ERR(ublox_gps_tty_driver))
		return PTR_ERR(ublox_gps_tty_driver);

	ublox_dev = devm_kzalloc(&client->dev, sizeof(struct ublox_device),
				GFP_KERNEL);
	if (!ublox_dev) {
		result = -ENOMEM;
		goto err;
	}
	ublox_dev->tty_driver = ublox_gps_tty_driver;

	tty_port_init(&ublox_dev->tty_port);

	ublox_gps_tty_driver->owner = THIS_MODULE;
	ublox_gps_tty_driver->driver_name = "ublox_gps";
	ublox_gps_tty_driver->name = "ttyUBLX";
	ublox_gps_tty_driver->major = 0; /* dynamically assign */
	ublox_gps_tty_driver->minor_start = 0;
	ublox_gps_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	ublox_gps_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	ublox_gps_tty_driver->flags = TTY_DRIVER_REAL_RAW;
	ublox_gps_tty_driver->init_termios = tty_std_termios;
	ublox_gps_tty_driver->init_termios.c_iflag = IGNCR | IXON;
	ublox_gps_tty_driver->init_termios.c_oflag = OPOST;
	ublox_gps_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD |
		HUPCL | CLOCAL;
	ublox_gps_tty_driver->init_termios.c_lflag &= ~ICANON;
	ublox_gps_tty_driver->init_termios.c_ispeed = 9600;
	ublox_gps_tty_driver->init_termios.c_ospeed = 9600;
	ublox_gps_tty_driver->num = UBLOX_GPS_NUM;
	tty_set_operations(ublox_gps_tty_driver, &ublox_gps_serial_ops);
	ublox_dev->tty_port.ops = &ublox_gps_port_ops;
	tty_port_link_device(&ublox_dev->tty_port, ublox_gps_tty_driver, 0);
	result = tty_register_driver(ublox_gps_tty_driver);
	if (result) {
		dev_err(&client->dev, ": %s - tty_register_driver failed\n",
			__func__);
		goto err;
	}

	ublox_dev->i2c_client = client;

	ublox_dev->i2c_regmap = devm_regmap_init_i2c(client,
						&ublox_gps_i2c_regmap_config);
	if (IS_ERR(ublox_dev->i2c_regmap)) {
		result = PTR_ERR(ublox_dev->i2c_regmap);
		goto err;
	}

	i2c_set_clientdata(client, ublox_dev);

	INIT_DELAYED_WORK(&ublox_dev->dwork, ublox_gps_read_worker);
	ublox_dev->is_active = false;

	dev_info(&client->dev, ": " DRIVER_VERSION ": " DRIVER_DESC "\n");

	return result;

err:
	dev_err(&client->dev, ": %s - returning with error %d\n",
		__func__, result);

	put_tty_driver(ublox_gps_tty_driver);

	return result;
}

static int ublox_gps_remove(struct i2c_client *client)
{
	struct ublox_device *ublox_dev = (struct ublox_device *)
						i2c_get_clientdata(client);

	tty_unregister_driver(ublox_dev->tty_driver);
	put_tty_driver(ublox_dev->tty_driver);

	return 0;
}

static const struct i2c_device_id ublox_gps_id[] = {
	{ "ublox_gps", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ublox_gps_id);

static struct i2c_driver ublox_gps_i2c_driver = {
	.driver = {
		.name  = "ublox_gps",
		.owner = THIS_MODULE,
	},
	.id_table  = ublox_gps_id,
	.probe     = ublox_gps_probe,
	.remove    = ublox_gps_remove,
};

module_i2c_driver(ublox_gps_i2c_driver);

MODULE_AUTHOR("Felipe F. Tonello <eu@felipetonello.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
