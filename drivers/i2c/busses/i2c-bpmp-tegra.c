/*
 * drivers/i2c/busses/i2c-bpmp-tegra.c
 *
 * Copyright (C) 2015 NVIDIA Corporation.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-gpio.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/clk/tegra.h>
#include <linux/spinlock.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-pm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/dma-mapping.h>
#include <soc/tegra/tegra_bpmp.h>
#include <linux/i2c-bpmp-tegra.h>
#include <linux/bpmp_mrq_i2c.h>

#include <asm/unaligned.h>

#define TEGRA_I2C_TIMEOUT			(msecs_to_jiffies(1000))

#define SERIALI2C_TEN		0x0010
#define SERIALI2C_RD		0x0001
#define SERIALI2C_STOP		0x8000
#define SERIALI2C_NOSTART	0x4000
#define SERIALI2C_REV_DIR_ADDR	0x2000
#define SERIALI2C_IGNORE_NAK	0x1000
#define SERIALI2C_NO_RD_ACK	0x0800
#define SERIALI2C_RECV_LEN	0x0400

static int seriali2c_xlate_flags(u16 *xlated_flags, u16 linux_flags)
{
	u16 ret = 0;
	if (linux_flags & I2C_M_TEN) {
		linux_flags &= ~I2C_M_TEN;
		ret |= SERIALI2C_TEN;
	}
	if (linux_flags & I2C_M_RD) {
		linux_flags &= ~I2C_M_RD;
		ret |= SERIALI2C_RD;
	}
	if (linux_flags & I2C_M_STOP) {
		linux_flags &= ~I2C_M_STOP;
		ret |= SERIALI2C_STOP;
	}
	if (linux_flags & I2C_M_NOSTART) {
		linux_flags &= ~I2C_M_NOSTART;
		ret |= SERIALI2C_NOSTART;
	}
	if (linux_flags & I2C_M_REV_DIR_ADDR) {
		linux_flags &= ~I2C_M_REV_DIR_ADDR;
		ret |= SERIALI2C_REV_DIR_ADDR;
	}
	if (linux_flags & I2C_M_IGNORE_NAK) {
		linux_flags &= ~I2C_M_IGNORE_NAK;
		ret |= SERIALI2C_IGNORE_NAK;
	}
	if (linux_flags & I2C_M_NO_RD_ACK) {
		linux_flags &= ~I2C_M_NO_RD_ACK;
		ret |= SERIALI2C_NO_RD_ACK;
	}
	if (linux_flags & I2C_M_RECV_LEN) {
		linux_flags &= ~I2C_M_RECV_LEN;
		ret |= SERIALI2C_RECV_LEN;
	}
	*xlated_flags = ret;
	return (linux_flags != 0) ? -EINVAL : 0;
}

/*
   The serialized I2C format is simply the following:
   [addr little-endian][flags little-endian][len little-endian][data if write]
   [addr little-endian][flags little-endian][len little-endian][data if write]
   ...

   The flags are translated from Linux kernel representation to seriali2c
   representation. Any undefined flag being set causes an error.

   The data is there only for writes. Reads have the data transferred in the
   other direction, and thus data is not present.

   See deserialize_i2c documentation for the data format in the other direction.
 */
#define OUTPUT_BYTE(x)                                                         \
	do {                                                                   \
		if (pos >= bufsize)                                            \
			return -EOVERFLOW;                                     \
		buf[pos++] = (x);                                              \
	}                                                                      \
	while (0)

static int serialize_i2c(char *buf, size_t bufsize,
			 struct i2c_msg *msgs, int num)
{
	int i;
	int r;
	size_t pos = 0;
	for (i = 0; i < num; i++) {
		struct i2c_msg *msg = &msgs[i];
		u16 xlated_flags;
		r = seriali2c_xlate_flags(&xlated_flags, msg->flags);
		if (r)
			return -EINVAL;
		OUTPUT_BYTE(msg->addr & 0xff);
		OUTPUT_BYTE((msg->addr & 0xff00) >> 8);
		OUTPUT_BYTE(xlated_flags & 0xff);
		OUTPUT_BYTE((xlated_flags & 0xff00) >> 8);
		OUTPUT_BYTE(msg->len & 0xff);
		OUTPUT_BYTE((msg->len & 0xff00) >> 8);
		if ((xlated_flags & SERIALI2C_RD) == 0) {
			/* WR */
			int j;
			for (j = 0; j < msg->len; j++)
				OUTPUT_BYTE(msg->buf[j]);
		}
	}
	if (pos > INT_MAX)
		return -EOVERFLOW;
	return (int)pos;
}
#undef OUTPUT_BYTE

static inline int safe_size_add(size_t *result, size_t a, size_t b)
{
	if (a > SIZE_MAX - b)
		return -EOVERFLOW;
	*result = a + b;
	return 0;
}

/*
   The data in the BPMP->CPU direction is composed of sequential blocks
   for those messages that have I2C_M_RD. So, for example, if you have:
   !I2C_M_RD, len == 5, data == a0 01 02 03 04
   !I2C_M_RD, len == 1, data == a0
   I2C_M_RD, len == 2, data == [uninitialized buffer 1]
   !I2C_M_RD, len == 1, data == a2
   I2C_M_RD, len == 2, data == [uninitialized buffer 2]
   ...then the data in the BPMP->CPU direction would be 4 bytes total, and
   would contain 2 bytes that will go to uninitialized buffer 1, and 2 bytes
   that will go to uninitialized buffer 2.
 */
static int deserialize_i2c(char *buf, size_t bufsize,
			   struct i2c_msg *msgs, int num)
{
	size_t totallen = 0;
	size_t pos = 0;
	int i;
	for (i = 0; i < num; i++) {
		if (msgs[i].flags & I2C_M_RD) {
			if (safe_size_add(&totallen, totallen, msgs[i].len))
				return -EINVAL;
		}
	}
	if (totallen != bufsize)
		return -EINVAL;
	for (i = 0; i < num; i++) {
		if (msgs[i].flags & I2C_M_RD) {
			BUG_ON(pos + msgs[i].len > bufsize);
			memcpy(msgs[i].buf, buf + pos, msgs[i].len);
			pos += msgs[i].len;
		}
	}
	return 0;
}

/**
 * struct tegra_i2c_dev	- per device i2c context
 * @adapter: core i2c layer adapter information
 */
struct tegra_bpmp_i2c_dev {
	struct i2c_adapter adapter;
	u32 bpmp_adapter_id;
};


static int tegra_bpmp_i2c(struct mrq_i2c_data_in *in,
			  struct mrq_i2c_data_out *out,
			  struct tegra_bpmp_i2c_dev *i2c_dev)
{
	unsigned long flags;
	int ret;

	if (irqs_disabled())
		return tegra_bpmp_send_receive_atomic(MRQ_I2C,
				in, sizeof(*in), out, sizeof(*out));

	if (i2c_dev->adapter.atomic_xfer_only) {
		local_irq_save(flags);
		ret = tegra_bpmp_send_receive_atomic(MRQ_I2C,
				in, sizeof(*in), out, sizeof(*out));
		local_irq_restore(flags);
		return ret;
	}

	return tegra_bpmp_send_receive(MRQ_I2C,
			      in, sizeof(*in), out, sizeof(*out));
}

static int tegra_bpmp_i2c_req(u32 adapter, struct mrq_i2c_data_in *in,
		int data_in_size, struct mrq_i2c_data_out *out,
		struct tegra_bpmp_i2c_dev *i2c_dev)
{
	int r;

	in->req = MRQ_I2C_DATA_IN_REQ_I2C_REQ;
	in->data.i2c_req.adapter = adapter;
	in->data.i2c_req.data_in_size = data_in_size;
	r = tegra_bpmp_i2c(in, out, i2c_dev);
	if (r) {
		WARN_ON(r > 0);
		return r;
	}

	return (int)out->data.i2c_req.data_size;
}

struct tegra_bpmp_i2c_chipdata {
};


static int tegra_bpmp_i2c_init(struct tegra_bpmp_i2c_dev *i2c_dev)
{
	int err = 0;

	return err;
}

static void dump_i2c_msgs(struct tegra_bpmp_i2c_dev *i2c_dev,
			  struct i2c_msg msgs[], int num)
{
	int i, j;

	dev_err(&i2c_dev->adapter.dev, "--- message dump for debugging ---\n");
	for (i = 0; i < num; i++) {
		struct i2c_msg *msg = &msgs[i];
		dev_err(&i2c_dev->adapter.dev,
			"addr 0x%x flags 0x%x len %d data:",
			(unsigned)msg->addr,
			(unsigned)msg->flags,
			(int)msg->len);
		for (j = 0; j < (int)msg->len; j++)
			dev_err(&i2c_dev->adapter.dev, " %.2x", msg->buf[j]);
		dev_err(&i2c_dev->adapter.dev, "\n");
	}
}

static int tegra_bpmp_i2c_preverify(struct i2c_msg *msgs, int num)
{
	int i, totallen = 0;

	for (i = 0, totallen = 0; i < num; i++) {
		if (!(msgs[i].flags & I2C_M_RD))
			totallen += 6 + msgs[i].len;
	}
	if (totallen > TEGRA_I2C_IPC_MAX_IN_BUF_SIZE)
		return totallen;

	for (i = 0, totallen = 0; i < num; i++) {
		if ((msgs[i].flags & I2C_M_RD))
			totallen += msgs[i].len;
	}
	if (totallen > TEGRA_I2C_IPC_MAX_OUT_BUF_SIZE)
		return totallen;

	return 0;
}

static int tegra_bpmp_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
	int num)
{
	struct tegra_bpmp_i2c_dev *i2c_dev = i2c_get_adapdata(adap);
	struct mrq_i2c_data_in in;
	struct mrq_i2c_data_out out;
	int ret = 0;
	int size;

	ret = tegra_bpmp_i2c_preverify(msgs, num);
	if (ret != 0) {
		dev_err(&i2c_dev->adapter.dev, "msg len : %d not supported\n",
			ret);
		return -EINVAL;
	}

	size = serialize_i2c(in.data.i2c_req.data_in_buf,
			TEGRA_I2C_IPC_MAX_IN_BUF_SIZE,
			msgs, num);
	if (size < 0) {
		dev_err(&i2c_dev->adapter.dev, "serialize_i2c ret %d\n", size);
		dump_i2c_msgs(i2c_dev, msgs, num);
		return size;
	}

	ret = tegra_bpmp_i2c_req(i2c_dev->bpmp_adapter_id, &in, size, &out,
			i2c_dev);
	if (ret < 0) {
		dev_err(&i2c_dev->adapter.dev, "tegra_bpmp_i2c_req ret %d\n",
			ret);
		dump_i2c_msgs(i2c_dev, msgs, num);
		return ret;
	}

	ret = deserialize_i2c(out.data.i2c_req.data_out_buf, ret, msgs, num);
	if (ret < 0) {
		dev_err(&i2c_dev->adapter.dev, "deserialize_i2c ret %d\n",
			ret);
		dump_i2c_msgs(i2c_dev, msgs, num);
		return ret;
	}
	WARN_ON(ret > 0);

	return num;
}

static u32 tegra_bpmp_i2c_func(struct i2c_adapter *adap)
{
	u32 ret = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR |
				I2C_FUNC_PROTOCOL_MANGLING | I2C_FUNC_NOSTART;
	return ret;
}

static const struct i2c_algorithm tegra_bpmp_i2c_algo = {
	.master_xfer	= tegra_bpmp_i2c_xfer,
	.functionality	= tegra_bpmp_i2c_func,
};

static struct tegra_bpmp_i2c_platform_data *parse_i2c_tegra_dt(
	struct platform_device *pdev)
{
	struct tegra_bpmp_i2c_platform_data *pdata;
	u32 adapter;
	struct device_node *np = pdev->dev.of_node;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	if (!of_property_read_u32(np, "adapter", &adapter))
		pdata->adapter = adapter;

	return pdata;
}

static struct tegra_bpmp_i2c_chipdata tegra186_bpmp_i2c_chipdata = {
};

/* Match table for of_platform binding */
static const struct of_device_id tegra_bpmp_i2c_of_match[] = {
	{ .compatible = "nvidia,tegra186-bpmp-i2c",
	  .data = &tegra186_bpmp_i2c_chipdata, },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_bpmp_i2c_of_match);

static struct platform_device_id tegra_bpmp_i2c_devtype[] = {
	{
		.name = "tegra12-bpmp-i2c",
		.driver_data = (unsigned long)&tegra186_bpmp_i2c_chipdata,
	},
	{ }
};

static int tegra_bpmp_i2c_probe(struct platform_device *pdev)
{
	struct tegra_bpmp_i2c_dev *i2c_dev;
	struct tegra_bpmp_i2c_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
	const struct tegra_bpmp_i2c_chipdata *chip_data = NULL;
	const struct of_device_id *match;
	int bus_num = -1;

	if (pdev->dev.of_node) {
		match = of_match_device(of_match_ptr(tegra_bpmp_i2c_of_match),
					&pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "Device Not matching\n");
			return -ENODEV;
		}
		chip_data = match->data;
		if (!pdata)
			pdata = parse_i2c_tegra_dt(pdev);
	} else {
		chip_data = (struct tegra_bpmp_i2c_chipdata *)
			    pdev->id_entry->driver_data;
		bus_num = pdev->id;
	}

	if (IS_ERR(pdata) || !pdata || !chip_data) {
		dev_err(&pdev->dev, "no platform/chip data?\n");
		return IS_ERR(pdata) ? PTR_ERR(pdata) : -ENODEV;
	}

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev) {
		dev_err(&pdev->dev, "Could not allocate struct tegra_i2c_dev");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, i2c_dev);

	ret = tegra_bpmp_i2c_init(i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize i2c controller");
		goto err;
	}

	pm_runtime_enable(&pdev->dev);

	i2c_set_adapdata(&i2c_dev->adapter, i2c_dev);
	i2c_dev->adapter.owner = THIS_MODULE;
	i2c_dev->adapter.class = I2C_CLASS_HWMON;
	strlcpy(i2c_dev->adapter.name, "Tegra BPMP I2C adapter",
		sizeof(i2c_dev->adapter.name));
	i2c_dev->adapter.algo = &tegra_bpmp_i2c_algo;
	i2c_dev->adapter.dev.parent = &pdev->dev;
	i2c_dev->adapter.nr = bus_num;
	i2c_dev->adapter.dev.of_node = pdev->dev.of_node;

	i2c_dev->bpmp_adapter_id = pdata->adapter;

	ret = i2c_add_numbered_adapter(&i2c_dev->adapter);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add I2C adapter\n");
		goto err;
	}

	pm_runtime_enable(&i2c_dev->adapter.dev);

	return 0;

err:
	return ret;
}

static int tegra_bpmp_i2c_remove(struct platform_device *pdev)
{
	struct tegra_bpmp_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	i2c_del_adapter(&i2c_dev->adapter);
	pm_runtime_disable(&i2c_dev->adapter.dev);

	return 0;
}

static void tegra_bpmp_i2c_shutdown(struct platform_device *pdev)
{
	struct tegra_bpmp_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	dev_info(&i2c_dev->adapter.dev, "Bus is shutdown down...\n");
	i2c_shutdown_adapter(&i2c_dev->adapter);
}

static struct platform_driver tegra_bpmp_i2c_driver = {
	.probe   = tegra_bpmp_i2c_probe,
	.remove  = tegra_bpmp_i2c_remove,
	.shutdown = tegra_bpmp_i2c_shutdown,
	.id_table = tegra_bpmp_i2c_devtype,
	.driver  = {
		.name  = "tegra-bpmp-i2c",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_bpmp_i2c_of_match),
		.pm    = NULL,
	},
};

static int __init tegra_bpmp_i2c_init_driver(void)
{
	return platform_driver_register(&tegra_bpmp_i2c_driver);
}

static void __exit tegra_bpmp_i2c_exit_driver(void)
{
	platform_driver_unregister(&tegra_bpmp_i2c_driver);
}

subsys_initcall(tegra_bpmp_i2c_init_driver);
module_exit(tegra_bpmp_i2c_exit_driver);

MODULE_DESCRIPTION("nVidia Tegra186 BPMP I2C Bus Controller driver");
MODULE_AUTHOR("Juha-Matti Tilli");
MODULE_AUTHOR("Shardar Shariff Md <smohammed@nvidia.com>");
MODULE_LICENSE("GPL v2");
