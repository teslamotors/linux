/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

/* #define DEBUG */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/completion.h>
#include "i2c-t18x-slv-common.h"

struct i2cslv_client {
	unsigned long addr;

	bool is_7bit_addr;

	bool client_rx_in_prgrs;
	bool client_tx_in_prgrs;

	unsigned char *rx_buf;
	unsigned char *tx_buf;

	unsigned int rx_buf_len;
	unsigned int rx_buf_pos;

	unsigned int tx_buf_len;
	unsigned int tx_buf_pos;
	struct completion slave_rx_complete;

	struct i2cslv_client_ops *i2c_clnt_ops;
} *i2cslv_client;

extern int i2cslv_register_client(struct i2cslv_client_ops *,
				  const unsigned long,
				  bool);

extern void i2cslv_unregister_client(void);
/* i2cslv_client_read - Controller driver use this callback to send data to
 * 			client driver.
 * This callback executes in interrupt context. DON'T include any sleepable
 * functions.
 */

static void i2cslv_client_read(unsigned char wd)
{

	if (i2cslv_client->rx_buf_pos == MAX_BUFF_SIZE) {
		pr_err("%s %x\n", __func__, wd);
		pr_err("rx buffer full");
		return;
	}

	i2cslv_client->rx_buf[i2cslv_client->rx_buf_pos++] = wd;

	i2cslv_client->rx_buf_len++;

	pr_debug("%s %x\n", __func__, wd);
}

/* i2cslv_client_read_end - Controller driver use this callback to pass read
 *				xfer end event to client driver.
 * This callback executes in interrupt context. DON'T include any sleepable
 * functions.
 */

static void i2cslv_client_read_end(void)
{
	pr_debug("%s\n", __func__);
	i2cslv_client->rx_buf_pos = 0;
	complete(&i2cslv_client->slave_rx_complete);
}

/* i2cslv_client_write - Controller driver use this callback to get data to
 * 			client driver.
 *
 * If data is not available in tx buffer, This function returns 0xAA as dummy
 * data. This callback executes in interrupt context. DON'T include any
 * sleepable functions.
 */
static unsigned char i2cslv_client_write(void)
{
	unsigned char dummy_wr = 0xAA;
	unsigned char udata;

	pr_debug("%s\n", __func__);

	if (i2cslv_client->tx_buf_pos > i2cslv_client->tx_buf_len) {
		pr_err("tx buffer empty");
		return dummy_wr;
	}

	udata = i2cslv_client->tx_buf[i2cslv_client->tx_buf_pos++];
	return udata;
}

/* i2cslv_client_write_end - Controller driver use this callback to pass write
 *				xfer end event to client driver.
 * This callback executes in interrupt context. DON'T include any sleepable
 * functions.
 */
static void i2cslv_client_write_end(void)
{
	pr_debug("%s\n", __func__);

	i2cslv_client->tx_buf_pos = 0;
}

static struct i2cslv_client_ops i2c_clnt_ops = {
	.slv_sendbyte_to_client = i2cslv_client_read,
	.slv_sendbyte_end_to_client = i2cslv_client_read_end,
	.slv_getbyte_from_client = i2cslv_client_write,
	.slv_getbyte_end_from_client = i2cslv_client_write_end,
};

inline int tegra_i2cslv_register(const unsigned long i2cslv_addr,
				 bool is_seven_bit)
{
	return i2cslv_register_client(&i2c_clnt_ops, i2cslv_addr, is_seven_bit);
}
EXPORT_SYMBOL(tegra_i2cslv_register);

inline void tegra_i2cslv_unregister(void)
{
	i2cslv_unregister_client();
}
EXPORT_SYMBOL(tegra_i2cslv_unregister);

/* tegra_i2cslv_write - To fill the client tx buffer.
 *
 * This function is used by the application, to fill the client tx buffer.
 */
int tegra_i2cslv_write(const char *wr, const unsigned int len)
{
	/* FIXME: Add multiple write support from application */
	if (!wr || len > MAX_BUFF_SIZE)
		return -EINVAL;

	memcpy(i2cslv_client->tx_buf, wr, len);

	i2cslv_client->tx_buf_len = len;

	i2cslv_client->tx_buf_pos = 0;
	return 0;
}
EXPORT_SYMBOL(tegra_i2cslv_write);

/* tegra_i2cslv_read - To get the client rx buffer.
 *
 * This function is used by the application, to get the client rx buffer.
 * This function waits for the current slave rx xfer completion.
 */
int tegra_i2cslv_read(char *rd, unsigned int len)
{
	long ret;
	if (!rd || len > MAX_BUFF_SIZE)
		return -EINVAL;

	ret = wait_for_completion_interruptible_timeout(
				&i2cslv_client->slave_rx_complete,
				SLAVE_RX_TIMEOUT);

	if (ret <= 0) {
		pr_err("Timeout waiting for RX end of xfer\n");
		return -EIO;
	}

	memcpy(rd, i2cslv_client->rx_buf, len);
	reinit_completion(&i2cslv_client->slave_rx_complete);

	return len;
}
EXPORT_SYMBOL(tegra_i2cslv_read);

static int __init tegra_i2cslv_init_client(void)
{
	int ret = 0;

	i2cslv_client = kzalloc(sizeof(*i2cslv_client), GFP_KERNEL);
	if (!i2cslv_client)
		goto err_enmm;

	i2cslv_client->rx_buf = kzalloc(MAX_BUFF_SIZE, GFP_KERNEL);
	if (!(i2cslv_client->rx_buf))
		goto err_enmm;

	i2cslv_client->tx_buf = kzalloc(MAX_BUFF_SIZE, GFP_KERNEL);
	if (!(i2cslv_client->tx_buf))
		goto err_enmm;

	i2cslv_client->rx_buf_len = 0;

	i2cslv_client->rx_buf_pos = 0;

	i2cslv_client->tx_buf_len = 0;

	i2cslv_client->tx_buf_pos = 0;

	init_completion(&i2cslv_client->slave_rx_complete);

	return ret;
err_enmm:
	kfree(i2cslv_client->rx_buf);
	kfree(i2cslv_client);

	ret = -ENOMEM;
	pr_err("i2cslv_client alloc error\n");
	return ret;
}

static void __exit tegra_i2cslv_exit_client(void)
{
	pr_debug("%s\n", __func__);

	kfree(i2cslv_client->rx_buf);
	kfree(i2cslv_client->tx_buf);
	kfree(i2cslv_client);
}

MODULE_AUTHOR("Ankit Gupta <guptaa@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA i2c slave client module");
MODULE_LICENSE("GPL v2");
late_initcall(tegra_i2cslv_init_client);
module_exit(tegra_i2cslv_exit_client);
