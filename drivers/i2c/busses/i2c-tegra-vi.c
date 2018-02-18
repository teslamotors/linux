/*
 * drivers/i2c/busses/vii2c-tegra.c
 *
 * Copyright (C) 2014-2016 NVIDIA Corporation.  All rights reserved.
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
#include <linux/i2c-tegra.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/clk/tegra.h>
#include <linux/spinlock.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-pm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/tegra-powergate.h>
#include <linux/pm_domain.h>
#include <linux/tegra_pm_domains.h>

#include <asm/unaligned.h>

#define TEGRA_I2C_TIMEOUT			(msecs_to_jiffies(1000))
#define TEGRA_I2C_RETRIES			3
#define BYTES_PER_FIFO_WORD			4

/* vi i2c specific registers and bit masks */
#define I2C_CNFG				(0x300 << 2)
#define I2C_CNFG_DEBOUNCE_CNT_SHIFT		12
#define I2C_CNFG_PACKET_MODE_EN			(1<<10)
#define I2C_CNFG_NEW_MASTER_FSM			(1<<11)
#define I2C_CNFG_NOACK				(1<<8)
#define I2C_STATUS				(0x31C << 2)
#define I2C_STATUS_BUSY				(1<<8)
#define I2C_TLOW_SEXT				(0x334 << 2)
#define I2C_TX_FIFO				(0x350 << 2)
#define I2C_RX_FIFO				(0x354 << 2)
#define I2C_PACKET_TRANSFER_STATUS		(0x358 << 2)
#define I2C_FIFO_CONTROL			(0x35C << 2)
#define I2C_FIFO_CONTROL_TX_FLUSH		(1<<1)
#define I2C_FIFO_CONTROL_RX_FLUSH		(1<<0)
#define I2C_FIFO_CONTROL_TX_TRIG_SHIFT		5
#define I2C_FIFO_CONTROL_RX_TRIG_SHIFT		2
#define I2C_FIFO_STATUS				(0x360 << 2)
#define I2C_FIFO_STATUS_TX_MASK			0xF0
#define I2C_FIFO_STATUS_TX_SHIFT		4
#define I2C_FIFO_STATUS_RX_MASK			0x0F
#define I2C_FIFO_STATUS_RX_SHIFT		0
#define I2C_INT_MASK				(0x364 << 2)
#define I2C_INT_STATUS				(0x368 << 2)
#define I2C_INT_BUS_CLEAR_DONE			(1<<11)
#define I2C_INT_PACKET_XFER_COMPLETE		(1<<7)
#define I2C_INT_ALL_PACKETS_XFER_COMPLETE	(1<<6)
#define I2C_INT_TX_FIFO_OVERFLOW		(1<<5)
#define I2C_INT_RX_FIFO_UNDERFLOW		(1<<4)
#define I2C_INT_NO_ACK				(1<<3)
#define I2C_INT_ARBITRATION_LOST		(1<<2)
#define I2C_INT_TX_FIFO_DATA_REQ		(1<<1)
#define I2C_INT_RX_FIFO_DATA_REQ		(1<<0)
#define I2C_INT_MASKED_STATUS			(0x370 << 2)

#define I2C_CLK_DIVISOR				(0x36C << 2)
#define I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT	16
#define I2C_CLK_MULTIPLIER_STD_FAST_MODE	8

#define I2C_ERR_NONE				0x00
#define I2C_ERR_NO_ACK				0x01
#define I2C_ERR_ARBITRATION_LOST		0x02
#define I2C_ERR_UNKNOWN_INTERRUPT		0x04
#define I2C_ERR_UNEXPECTED_STATUS		0x08

#define PACKET_HEADER0_HEADER_SIZE_SHIFT	28
#define PACKET_HEADER0_PACKET_ID_SHIFT		16
#define PACKET_HEADER0_CONT_ID_SHIFT		12
#define PACKET_HEADER0_PROTOCOL_I2C		(1<<4)

#define I2C_HEADER_HIGHSPEED_MODE		(1<<22)
#define I2C_HEADER_CONT_ON_NAK			(1<<21)
#define I2C_HEADER_SEND_START_BYTE		(1<<20)
#define I2C_HEADER_READ				(1<<19)
#define I2C_HEADER_10BIT_ADDR			(1<<18)
#define I2C_HEADER_IE_ENABLE			(1<<17)
#define I2C_HEADER_REPEAT_START			(1<<16)
#define I2C_HEADER_CONTINUE_XFER		(1<<15)
#define I2C_HEADER_MASTER_ADDR_SHIFT		12
#define I2C_HEADER_SLAVE_ADDR_SHIFT		1

#define I2C_BUS_CLEAR_CNFG			(0x384 << 2)
#define I2C_BC_SCLK_THRESHOLD			(9<<16)
#define I2C_BC_STOP_COND			(1<<2)
#define I2C_BC_TERMINATE			(1<<1)
#define I2C_BC_ENABLE				(1<<0)

#define I2C_BUS_CLEAR_STATUS			(0x388 << 2)
#define I2C_BC_STATUS				(1<<0)

#define I2C_CONFIG_LOAD				(0x38C << 2)
#define I2C_MSTR_CONFIG_LOAD			(1 << 0)
#define I2C_SLV_CONFIG_LOAD			(1 << 1)
#define I2C_TIMEOUT_CONFIG_LOAD			(1 << 2)

#define I2C_INTERFACE_TIMING_0			(0x394 << 2)
#define I2C_TLOW				4
#define I2C_TLOW_SHIFT				0
#define I2C_THIGH				2
#define I2C_THIGH_SHIFT				8
#define I2C_INTERFACE_TIMING_1			(0x398 << 2)
#define I2C_HS_INTERFACE_TIMING_0		(0x39C << 2)
#define I2C_HS_INTERFACE_TIMING_1		(0x3A0 << 2)

#define MAX_BUSCLEAR_CLOCK			(9 * 8 + 1)

/*
 * msg_end_type: The bus control which need to be send at end of transfer.
 * @MSG_END_STOP: Send stop pulse at end of transfer.
 * @MSG_END_REPEAT_START: Send repeat start at end of transfer.
 * @MSG_END_CONTINUE: The following on message is coming and so do not send
 *		stop or repeat start.
 */

static struct of_device_id tegra_ve_pd[] = {
	{ .compatible = "nvidia,tegra186-ve-pd", },
	{ .compatible = "nvidia,tegra210-ve-pd", },
	{ .compatible = "nvidia,tegra132-ve-pd", },
	{},
};

enum msg_end_type {
	MSG_END_STOP,
	MSG_END_REPEAT_START,
	MSG_END_CONTINUE,
};

struct tegra_vi_i2c_chipdata {
	bool timeout_irq_occurs_before_bus_inactive;
	bool has_xfer_complete_interrupt;
	bool has_hw_arb_support;
	bool has_fast_clock;
	bool has_clk_divisor_std_fast_mode;
	bool has_continue_xfer_support;
	u16 clk_divisor_std_fast_mode;
	u16 clk_divisor_fast_plus_mode;
	u16 clk_divisor_hs_mode;
	int clk_multiplier_hs_mode;
	bool has_config_load_reg;
};

#define IX_MAX_TXBUF_DATA_NUM		0x400
#define IX_NACK_RETRY_LIMIT		2

/**
 * struct tegra_vi_i2c_dev	- per device i2c context
 * @dev: device reference for power management
 * @adapter: core i2c layer adapter information
 * @div_clk: clock reference for div clock of i2c controller.
 * @fast_clk: clock reference for fast clock of i2c controller.
 * @base: ioremapped registers cookie
 * @cont_id: i2c controller id, used for for packet header
 * @irq: irq number of transfer complete interrupt
 * @msg_complete: transfer completion notifier
 * @bus_clk_rate: current i2c bus clock rate
 * @is_suspended: prevents i2c controller accesses after suspend is called
 */
struct tegra_vi_i2c_dev {
	struct device *dev;
	struct i2c_adapter adapter;
	struct regulator *reg;
	struct clk *div_clk;
	struct clk *fast_clk;
	struct clk *slow_clk;
	struct clk *host1x_clk;
	spinlock_t fifo_lock;
	void __iomem *base;
	int cont_id;
	int irq;
	struct completion msg_complete;
	unsigned long bus_clk_rate;
	bool is_suspended;
	u16 slave_addr;
	bool is_clkon_always;
	bool is_high_speed_enable;
	u16 hs_master_code;
	u16 clk_divisor_non_hs_mode;
	bool use_single_xfer_complete;
	const struct tegra_vi_i2c_chipdata *chipdata;
	int scl_gpio;
	int sda_gpio;
	struct i2c_algo_bit_data bit_data;
	const struct i2c_algorithm *bit_algo;
	bool bit_banging_xfer_after_shutdown;
	bool is_shutdown;
	struct notifier_block pm_nb;
	struct regulator *pull_up_supply;

	/* i2c transfer */
	u32 ix_iopacket_w0, ix_iopacket_w2;
	u32 ix_i2c_cnfg_reg;
	u32 ix_int_mask_reg;
	u32 ix_pkt_id;
	u32 ix_txbuf_data[IX_MAX_TXBUF_DATA_NUM];
	u32 ix_txbuf_count;
	u32 ix_txbuf_cur;
	u8 *ix_rxbuf_data;
	u32 ix_rxbuf_remain;
	u32 ix_err_status;
	u32 ix_nack_retry;

	/* statistics */
	struct {
		unsigned int xfer_requests;
		unsigned int total_bytes;
		unsigned int reads, read_bytes;
		unsigned int writes, write_bytes;
		unsigned int int_no_ack, int_arb_lost, int_tx_fifo_overflow;
		unsigned int int_tx_fifo, int_rx_fifo, int_xfer_complete;
	} stat;
};

static void i2c_writel(struct tegra_vi_i2c_dev *i2c_dev, u32 val,
	unsigned long reg)
{
	writel(val, i2c_dev->base + reg);

	/* Read back register to make sure that register writes completed */
	if (reg != I2C_TX_FIFO)
		readl(i2c_dev->base + reg);
	else
		readl(i2c_dev->base + I2C_PACKET_TRANSFER_STATUS);
}

static u32 i2c_readl(struct tegra_vi_i2c_dev *i2c_dev, unsigned long reg)
{
	return readl(i2c_dev->base + reg);
}

static inline void tegra_vi_i2c_gpio_setscl(void *data, int state)
{
	struct tegra_vi_i2c_dev *i2c_dev = data;

	gpio_set_value(i2c_dev->scl_gpio, state);
}

static inline int tegra_vi_i2c_gpio_getscl(void *data)
{
	struct tegra_vi_i2c_dev *i2c_dev = data;

	return gpio_get_value(i2c_dev->scl_gpio);
}

static inline void tegra_vi_i2c_gpio_setsda(void *data, int state)
{
	struct tegra_vi_i2c_dev *i2c_dev = data;

	gpio_set_value(i2c_dev->sda_gpio, state);
}

static inline int tegra_vi_i2c_gpio_getsda(void *data)
{
	struct tegra_vi_i2c_dev *i2c_dev = data;

	return gpio_get_value(i2c_dev->sda_gpio);
}

static int tegra_vi_i2c_gpio_request(struct tegra_vi_i2c_dev *i2c_dev)
{
	int ret;

	ret = gpio_request_one(i2c_dev->scl_gpio,
				GPIOF_OUT_INIT_HIGH | GPIOF_OPEN_DRAIN,
				"i2c-gpio-scl");
	if (ret < 0) {
		dev_err(i2c_dev->dev, "GPIO request for gpio %d failed %d\n",
				i2c_dev->scl_gpio, ret);
		return ret;
	}

	ret = gpio_request_one(i2c_dev->sda_gpio,
				GPIOF_OUT_INIT_HIGH | GPIOF_OPEN_DRAIN,
				"i2c-gpio-sda");
	if (ret < 0) {
		dev_err(i2c_dev->dev, "GPIO request for gpio %d failed %d\n",
				i2c_dev->sda_gpio, ret);
		gpio_free(i2c_dev->scl_gpio);
		return ret;
	}
	return ret;
}

static void tegra_vi_i2c_gpio_free(struct tegra_vi_i2c_dev *i2c_dev)
{
	gpio_free(i2c_dev->scl_gpio);
	gpio_free(i2c_dev->sda_gpio);
}

static int tegra_vi_i2c_gpio_xfer(struct i2c_adapter *adap,
	struct i2c_msg msgs[], int num)
{
	struct tegra_vi_i2c_dev *i2c_dev = i2c_get_adapdata(adap);
	int ret;

	ret = tegra_vi_i2c_gpio_request(i2c_dev);
	if (ret < 0)
		return ret;

	ret = i2c_dev->bit_algo->master_xfer(adap, msgs, num);
	if (ret < 0)
		dev_err(i2c_dev->dev, "i2c-bit-algo xfer failed %d\n", ret);

	tegra_vi_i2c_gpio_free(i2c_dev);
	return ret;
}

static int tegra_vi_i2c_gpio_init(struct tegra_vi_i2c_dev *i2c_dev)
{
	struct i2c_algo_bit_data *bit_data = &i2c_dev->bit_data;

	bit_data->setsda = tegra_vi_i2c_gpio_setsda;
	bit_data->getsda = tegra_vi_i2c_gpio_getsda;
	bit_data->setscl = tegra_vi_i2c_gpio_setscl;
	bit_data->getscl = tegra_vi_i2c_gpio_getscl;
	bit_data->data = i2c_dev;
	bit_data->udelay = 20; /* 50KHz */
	bit_data->timeout = HZ; /* 10 ms*/
	i2c_dev->bit_algo = &i2c_bit_algo;
	i2c_dev->adapter.algo_data = bit_data;
	return 0;
}

static int tegra_vi_i2c_flush_fifos(struct tegra_vi_i2c_dev *i2c_dev)
{
	unsigned long timeout = jiffies + HZ;
	u32 val = i2c_readl(i2c_dev, I2C_FIFO_CONTROL);
	val |= I2C_FIFO_CONTROL_TX_FLUSH | I2C_FIFO_CONTROL_RX_FLUSH;
	i2c_writel(i2c_dev, val, I2C_FIFO_CONTROL);

	while (i2c_readl(i2c_dev, I2C_FIFO_CONTROL) &
		(I2C_FIFO_CONTROL_TX_FLUSH | I2C_FIFO_CONTROL_RX_FLUSH)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(i2c_dev->dev, "timeout waiting for fifo flush\n");
			return -ETIMEDOUT;
		}
		usleep_range(1000, 1100);
	}
	return 0;
}

static inline int tegra_vi_i2c_power_enable(struct tegra_vi_i2c_dev *i2c_dev)
{
	int ret;
	int partition_id;

	if (i2c_dev->pull_up_supply) {
		ret = regulator_enable(i2c_dev->pull_up_supply);
		if (ret < 0) {
			dev_err(i2c_dev->dev, "Pull up regulator supply failed: %d\n",
				ret);
			return ret;
		}
	}

	ret = regulator_enable(i2c_dev->reg);
	if (ret)
		return ret;

	partition_id = tegra_pd_get_powergate_id(tegra_ve_pd);
	if (partition_id < 0)
		return -EINVAL;
	tegra_unpowergate_partition(partition_id);

	return 0;
}

static inline int tegra_vi_i2c_power_disable(struct tegra_vi_i2c_dev *i2c_dev)
{
	int partition_id;
	int ret = 0;

	partition_id = tegra_pd_get_powergate_id(tegra_ve_pd);
	if (partition_id < 0)
		return -EINVAL;

	if (tegra_powergate_is_powered(partition_id))
		tegra_powergate_partition(partition_id);

	ret = regulator_disable(i2c_dev->reg);
	if (ret)
		dev_err(i2c_dev->dev, "%s: regulator err\n", __func__);

	if (i2c_dev->pull_up_supply) {
		ret = regulator_disable(i2c_dev->pull_up_supply);
		if (ret)
			dev_err(i2c_dev->dev, "%s: pull_up_supply err\n",
					__func__);
	}

	return ret;
}

static int tegra_vi_i2c_clock_enable(struct tegra_vi_i2c_dev *i2c_dev)
{
	int ret;

	if (i2c_dev->chipdata->has_fast_clock) {
		ret = clk_prepare_enable(i2c_dev->fast_clk);
		if (ret < 0) {
			dev_err(i2c_dev->dev,
				"Enabling fast clk failed, err %d\n", ret);
			goto fast_clk_err;
		}
	}
	ret = clk_prepare_enable(i2c_dev->div_clk);
	if (ret < 0) {
		dev_err(i2c_dev->dev,
			"Enabling div clk failed, err %d\n", ret);
		goto div_clk_err;
	}
	ret = clk_prepare_enable(i2c_dev->slow_clk);
	if (ret < 0) {
		dev_err(i2c_dev->dev,
			"Enabling slow clk failed, err %d\n", ret);
		goto slow_clk_err;
	}

	ret = clk_set_rate(i2c_dev->host1x_clk, 0);
	if (ret < 0) {
		dev_err(i2c_dev->dev,
			"Set host1x clk failed, err %d\n", ret);
		goto host1x_clk_err;
	}
	ret = clk_prepare_enable(i2c_dev->host1x_clk);
	if (ret < 0) {
		dev_err(i2c_dev->dev,
			"Enabling host1x clk failed, err %d\n", ret);
		goto host1x_clk_err;
	}

	return 0;

host1x_clk_err:
	clk_disable_unprepare(i2c_dev->slow_clk);
slow_clk_err:
	clk_disable_unprepare(i2c_dev->div_clk);
div_clk_err:
	if (i2c_dev->chipdata->has_fast_clock)
		clk_disable_unprepare(i2c_dev->fast_clk);
fast_clk_err:
	return ret;
}

static void tegra_vi_i2c_clock_disable(struct tegra_vi_i2c_dev *i2c_dev)
{
	clk_disable_unprepare(i2c_dev->host1x_clk);
	clk_disable_unprepare(i2c_dev->slow_clk);
	clk_disable_unprepare(i2c_dev->div_clk);
	if (i2c_dev->chipdata->has_fast_clock)
		clk_disable_unprepare(i2c_dev->fast_clk);
}

static void tegra_vi_i2c_set_clk_rate(struct tegra_vi_i2c_dev *i2c_dev)
{
	u32 clk_multiplier;
	if (i2c_dev->is_high_speed_enable)
		clk_multiplier = i2c_dev->chipdata->clk_multiplier_hs_mode
			* (i2c_dev->chipdata->clk_divisor_hs_mode + 1);
	else
		clk_multiplier = (I2C_TLOW + I2C_THIGH + 3)
			* (i2c_dev->clk_divisor_non_hs_mode + 1);

	clk_set_rate(i2c_dev->div_clk, i2c_dev->bus_clk_rate * clk_multiplier);
	/* slow i2c clock is always 1M */
	clk_set_rate(i2c_dev->slow_clk, 1000000);
}

/*
 * I2C transfer
 */

#define I2C_INTERFACE_TIMING_1_DEFAULT		0x04070404
#define I2C_HS_INTERFACE_TIMING_0_DEFAULT	0x308
#define I2C_HS_INTERFACE_TIMING_1_DEFAULT	0x0B0B0B
#define I2C_BUS_CLEAR_CNFG_DEFAULT		0x90004
#define I2C_TLOW_SEXT_DEFAULT			0x0

static int tegra_ix_init(struct tegra_vi_i2c_dev *i2c_dev)
{
	u32 val;
	u32 int_mask;
	int err = 0;
	u32 clk_divisor = 0;
	unsigned long timeout = jiffies + HZ;

	if (!i2c_dev->reg || !regulator_is_enabled(i2c_dev->reg))
		return -ENODEV;

	tegra_periph_reset_assert(i2c_dev->div_clk);
	udelay(2);
	tegra_periph_reset_deassert(i2c_dev->div_clk);

	val = I2C_CNFG_NEW_MASTER_FSM | I2C_CNFG_PACKET_MODE_EN;
	if (!i2c_dev->is_high_speed_enable)
		val |= (0x2 << I2C_CNFG_DEBOUNCE_CNT_SHIFT);
	i2c_dev->ix_i2c_cnfg_reg = val;
	i2c_writel(i2c_dev, val, I2C_CNFG);

	i2c_writel(i2c_dev, 0, I2C_INT_MASK);
	i2c_writel(i2c_dev, (I2C_TLOW << I2C_TLOW_SHIFT) |
		(I2C_THIGH << I2C_THIGH_SHIFT), I2C_INTERFACE_TIMING_0);
	i2c_writel(i2c_dev, I2C_INTERFACE_TIMING_1_DEFAULT,
		I2C_INTERFACE_TIMING_1);
	i2c_writel(i2c_dev, I2C_HS_INTERFACE_TIMING_0_DEFAULT,
		I2C_HS_INTERFACE_TIMING_0);
	i2c_writel(i2c_dev, I2C_HS_INTERFACE_TIMING_1_DEFAULT,
		I2C_HS_INTERFACE_TIMING_1);
	i2c_writel(i2c_dev, I2C_BUS_CLEAR_CNFG_DEFAULT,
		I2C_BUS_CLEAR_CNFG);
	i2c_writel(i2c_dev, I2C_TLOW_SEXT_DEFAULT,
		I2C_TLOW_SEXT);

	tegra_vi_i2c_set_clk_rate(i2c_dev);

	clk_divisor |= i2c_dev->chipdata->clk_divisor_hs_mode;
	if (i2c_dev->chipdata->has_clk_divisor_std_fast_mode)
		clk_divisor |= i2c_dev->clk_divisor_non_hs_mode
			<< I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT;
	i2c_writel(i2c_dev, clk_divisor, I2C_CLK_DIVISOR);

	val = (3 << I2C_FIFO_CONTROL_TX_TRIG_SHIFT) |
		(0 << I2C_FIFO_CONTROL_RX_TRIG_SHIFT);
	i2c_writel(i2c_dev, val, I2C_FIFO_CONTROL);

	if (tegra_vi_i2c_flush_fifos(i2c_dev))
		err = -ETIMEDOUT;

	if (i2c_dev->chipdata->has_config_load_reg) {
		i2c_writel(i2c_dev, I2C_MSTR_CONFIG_LOAD, I2C_CONFIG_LOAD);
		while (i2c_readl(i2c_dev, I2C_CONFIG_LOAD) != 0) {
			if (time_after(jiffies, timeout)) {
				dev_warn(i2c_dev->dev, "timeout waiting for config load\n");
				return -ETIMEDOUT;
			}
			usleep_range(1000, 1100);
		}
	}

	int_mask = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST
		| I2C_INT_TX_FIFO_OVERFLOW | I2C_INT_RX_FIFO_DATA_REQ;
	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		int_mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;
	i2c_dev->ix_int_mask_reg = int_mask;
	i2c_writel(i2c_dev, int_mask, I2C_INT_MASK);
	i2c_writel(i2c_dev, int_mask, I2C_INT_STATUS);

	i2c_dev->ix_iopacket_w0 = (0 << PACKET_HEADER0_HEADER_SIZE_SHIFT) |
		PACKET_HEADER0_PROTOCOL_I2C |
		((i2c_dev->cont_id & 0x0f) << PACKET_HEADER0_CONT_ID_SHIFT);

	if (i2c_dev->is_high_speed_enable) {
		i2c_dev->ix_iopacket_w2 |= I2C_HEADER_HIGHSPEED_MODE;
		i2c_dev->ix_iopacket_w2 |= ((i2c_dev->hs_master_code & 0x7)
			<< I2C_HEADER_MASTER_ADDR_SHIFT);
	}

	i2c_dev->ix_txbuf_cur = 0;

	return err;
}

static int tegra_ix_read_rx_fifo(struct tegra_vi_i2c_dev *i2c_dev)
{
	u32 rxfifos;
	u8 *buf8d;
	unsigned int rx_remain;

	rxfifos = i2c_readl(i2c_dev, I2C_FIFO_STATUS);
	buf8d = i2c_dev->ix_rxbuf_data;
	rx_remain = i2c_dev->ix_rxbuf_remain;

	rxfifos &= I2C_FIFO_STATUS_RX_MASK;
	rxfifos >>= I2C_FIFO_STATUS_RX_SHIFT;

	if (unlikely(rxfifos == 0))
		return -1;

	if (unlikely(i2c_dev->ix_rxbuf_data == NULL)) {
		while (rxfifos-- > 0)
			i2c_readl(i2c_dev, I2C_RX_FIFO);
		return -1;
	}

	while (rxfifos-- > 0) {
		u32 data = i2c_readl(i2c_dev, I2C_RX_FIFO);
		unsigned int rx_bytes = rx_remain;

		if (rx_bytes > BYTES_PER_FIFO_WORD)
			rx_bytes = BYTES_PER_FIFO_WORD;

		switch (rx_bytes) {
		case 4:
			*(u32 *) buf8d = data;
			break;
		case 3:
			*buf8d++ = data & 0xff;
			data >>= 8;
			/* fall through */
		case 2:
			*buf8d++ = data & 0xff;
			data >>= 8;
			/* fall through */
		case 1:
			*buf8d++ = data & 0xff;
			break;
		}

		rx_remain -= rx_bytes;
	}

	i2c_dev->ix_rxbuf_data = buf8d;
	i2c_dev->ix_rxbuf_remain = rx_remain;
	if (rx_remain == 0)
		i2c_dev->ix_rxbuf_data = NULL;

	return rx_remain;
}

static void tegra_ix_fill_tx_fifo(struct tegra_vi_i2c_dev *i2c_dev)
{
	u32 txfifos;
	u32 *buf;
	unsigned int txbuf_cur;
	unsigned int txbuf_count;
	unsigned int txbuf_remain;

	txfifos = i2c_readl(i2c_dev, I2C_FIFO_STATUS);
	txbuf_cur = i2c_dev->ix_txbuf_cur;
	txbuf_count = i2c_dev->ix_txbuf_count;

	if (txbuf_cur == txbuf_count)
		return;

	buf = i2c_dev->ix_txbuf_data + txbuf_cur;
	txfifos &= I2C_FIFO_STATUS_TX_MASK;
	txfifos >>= I2C_FIFO_STATUS_TX_SHIFT;

	if (txfifos > 0)
		--txfifos;

	txbuf_remain = txbuf_count - txbuf_cur;
	if (txfifos >= txbuf_remain) {
		txfifos = txbuf_remain;
		i2c_dev->ix_int_mask_reg &= ~I2C_INT_TX_FIFO_DATA_REQ;
	} else
		i2c_dev->ix_int_mask_reg |= I2C_INT_TX_FIFO_DATA_REQ;
	i2c_dev->ix_txbuf_cur = txbuf_cur + txfifos;

	while (txfifos-- > 0)
		i2c_writel(i2c_dev, *buf++, I2C_TX_FIFO);

	i2c_writel(i2c_dev, i2c_dev->ix_int_mask_reg, I2C_INT_MASK);
}

static irqreturn_t tegra_ix_isr(int irq, void *dev_id)
{
	const u32 status_err = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST
		| I2C_INT_TX_FIFO_OVERFLOW;

	u32 status, status_raw;
	struct tegra_vi_i2c_dev *i2c_dev = dev_id;

	status_raw = i2c_readl(i2c_dev, I2C_INT_STATUS);
	status = status_raw & i2c_dev->ix_int_mask_reg;
	i2c_dev->ix_err_status = status & status_err;

	if (unlikely(i2c_dev->ix_err_status)) {
		if (status & I2C_INT_NO_ACK)
			++i2c_dev->stat.int_no_ack;

		if (status & I2C_INT_ARBITRATION_LOST)
			++i2c_dev->stat.int_arb_lost;

		if (status & I2C_INT_TX_FIFO_OVERFLOW)
			++i2c_dev->stat.int_tx_fifo_overflow;

		i2c_writel(i2c_dev, status_err, I2C_INT_STATUS);
		complete(&i2c_dev->msg_complete);

		return IRQ_HANDLED;
	}

	if (status & I2C_INT_RX_FIFO_DATA_REQ) {
		tegra_ix_read_rx_fifo(i2c_dev);
		++i2c_dev->stat.int_rx_fifo;
	}

	if (status & I2C_INT_TX_FIFO_DATA_REQ) {
		tegra_ix_fill_tx_fifo(i2c_dev);
		++i2c_dev->stat.int_tx_fifo;
	}

	i2c_writel(i2c_dev, status_raw, I2C_INT_STATUS);

	if (status & I2C_INT_ALL_PACKETS_XFER_COMPLETE) {
		complete(&i2c_dev->msg_complete);
		++i2c_dev->stat.int_xfer_complete;
	}

	if (status & I2C_INT_BUS_CLEAR_DONE)
		complete(&i2c_dev->msg_complete);

	return IRQ_HANDLED;
}

static void tegra_ix_decode_packet(struct tegra_vi_i2c_dev *i2c_dev,
	u32 *cmd_buf, unsigned int count)
{
	enum { IOPACKET_W0, IOPACKET_W1, IOPACKET_W2, IOPACKET_PAYLOAD };

	unsigned int i, j;
	unsigned int state = IOPACKET_W0, len = 0;
	u32 data;
	u8 *data8;

	dev_dbg(i2c_dev->dev, "TXFIFO: %u words\n", count);

	for (i = 0; i < count; ++i) {
		data = cmd_buf[i];

		switch (state) {
		case IOPACKET_W0:
			dev_dbg(i2c_dev->dev,
				"%08x: TYPE=%u PROTOCOL=%u CTRL=%u PKTID=%02x HDRSIZE=%u",
				data,
				(data >> 0) & 0x07,
				(data >> 4) & 0x0f,
				(data >> 12) & 0x0f,
				(data >> 16) & 0xff,
				(data >> 28) & 0x03);
			state = IOPACKET_W1;
			break;
		case IOPACKET_W1:
			dev_dbg(i2c_dev->dev,
				"%08x: LEN=%u\n", data, data + 1);
			state = IOPACKET_W2;
			len = data + 1;
			break;
		case IOPACKET_W2:
			dev_dbg(i2c_dev->dev, "%08x: SLAVE=%02x, %s\n",
				data,
				(data >> 1) & 0x3ff,
				(data & (1 << 19)) ? "READ" : "WRITE");
			if (data & (1 << 15))
				dev_dbg(i2c_dev->dev, "  CONTINUE_XFER\n");
			if (data & (1 << 16))
				dev_dbg(i2c_dev->dev, "  REPEAT_START\n");
			if (data & (1 << 17))
				dev_dbg(i2c_dev->dev, "  IE\n");
			if (data & (1 << 18))
				dev_dbg(i2c_dev->dev, "  10BIT\n");
			if (data & (1 << 20))
				dev_dbg(i2c_dev->dev, "  SEND_START\n");
			if (data & (1 << 21))
				dev_dbg(i2c_dev->dev, "  CONTINUE_ON_NAK\n");
			if (data & (1 << 22)) {
				dev_dbg(i2c_dev->dev,
					"  HS MODE, HSMASTER=%u\n",
					(data >> 12) & 0x07);
			}
			if ((data & (1 << 19)) != 0)
				state = IOPACKET_W0;
			else
				state = IOPACKET_PAYLOAD;
			break;
		case IOPACKET_PAYLOAD:
			dev_dbg(i2c_dev->dev, "%08x:\n", data);
			data8 = (u8 *) &data;
			for (j = 0; j < 4 && len > 0; ++j, --len)
				dev_dbg(i2c_dev->dev, "  %02x\n", *data8++);
			if (len == 0)
				state = IOPACKET_W0;
			break;
		}
	}
}

static int tegra_ix_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
	int num)
{
	static bool s_need_init = true;

	struct tegra_vi_i2c_dev *i2c_dev = i2c_get_adapdata(adap);
	int i, ret;
	struct i2c_msg *msg, *msg_next;
	u32 *buf;
	int timed_out;

	if (unlikely(num == 0))
		return 0;

	if (unlikely(i2c_dev->is_suspended))
		return -EBUSY;

	if (unlikely(i2c_dev->is_shutdown || adap->atomic_xfer_only)) {
		if (i2c_dev->bit_banging_xfer_after_shutdown) {
			ret = tegra_vi_i2c_gpio_xfer(adap, msgs, num);
			return ret ? ret : num;
		}
	}

	if (unlikely(adap->atomic_xfer_only))
		return -EBUSY;

	ret = pm_runtime_get_sync(i2c_dev->dev);
	if (unlikely(ret < 0))
		return ret;

	if (unlikely(s_need_init)) {
		tegra_ix_init(i2c_dev);
		s_need_init = false;
	}

	/* Process I2C messages */
	i = 0;
	msg = msgs;
	msg_next = (num == 1) ? NULL : msg + 1;
	buf = i2c_dev->ix_txbuf_data;
	++i2c_dev->stat.xfer_requests;
	i2c_dev->ix_nack_retry = IX_NACK_RETRY_LIMIT;

	while (true) {
		u32 val;

		/* packet header */
		i2c_dev->ix_pkt_id += 1 << PACKET_HEADER0_PACKET_ID_SHIFT;
		i2c_dev->ix_pkt_id &= 0xff << PACKET_HEADER0_PACKET_ID_SHIFT;
		*buf++ = i2c_dev->ix_iopacket_w0 | i2c_dev->ix_pkt_id;

		/* payload size */
		*buf++ = msg->len - 1;

		/* IO header */
		if ((msg->flags & I2C_M_TEN) == 0)
			val = msg->addr << I2C_HEADER_SLAVE_ADDR_SHIFT;
		else
			val = msg->addr | I2C_HEADER_10BIT_ADDR;
		if (unlikely(msg->flags & I2C_M_IGNORE_NAK))
			val |= I2C_HEADER_CONT_ON_NAK;
		if (unlikely(msg->flags & I2C_M_RD))
			val |= I2C_HEADER_READ;
		val |= i2c_dev->ix_iopacket_w2;

		if (msg_next == NULL) {
			val |= I2C_HEADER_IE_ENABLE;
			i2c_dev->stat.total_bytes += 1 + msg->len;
		} else {
			if (msg_next->flags & I2C_M_NOSTART) {
				val |= I2C_HEADER_CONTINUE_XFER;
				i2c_dev->stat.total_bytes += msg->len;
			} else {
				val |= I2C_HEADER_REPEAT_START;
				i2c_dev->stat.total_bytes += 1 + msg->len;
			}
		}
		*buf++ = val;

		/* payload */
		if (likely((msg->flags & I2C_M_RD) == 0)) {
			if (likely(msg->len <= 4)) {
				/* write 4 bytes or less: most of the cases */
				u8 *buf8d = (u8 *) buf;
				u8 *buf8s = msg->buf;

				switch (msg->len) {
				case 4:
					*buf8d++ = *buf8s++;
					/* fall through */
				case 3:
					*buf8d++ = *buf8s++;
					/* fall through */
				case 2:
					*buf8d++ = *buf8s++;
					/* fall through */
				case 1:
					*buf8d = *buf8s;
					break;
				}
				++buf;
			} else {
				int count_word, count_byte;
				u32 *buf32s = (u32 *) msg->buf;

				count_word = msg->len / BYTES_PER_FIFO_WORD;
				count_byte = msg->len % BYTES_PER_FIFO_WORD;

				while (count_word-- > 0)
					*buf++ = *buf32s++;

				if (count_byte) {
					u8 *buf8d = (u8 *) buf;
					u8 *buf8s = (u8 *) buf32s;

					switch (count_byte) {
					case 3:
						*buf8d++ = *buf8s++;
					case 2:
						*buf8d++ = *buf8s++;
					case 1:
						*buf8d = *buf8s;
					}
					++buf;
				}
			}

			++i2c_dev->stat.writes;
			i2c_dev->stat.write_bytes += msg->len;
		} else {
			i2c_dev->ix_rxbuf_data = msg->buf;
			i2c_dev->ix_rxbuf_remain = msg->len;

			++i2c_dev->stat.reads;
			i2c_dev->stat.read_bytes += msg->len;

			++i;

			if (msg_next)
				i2c_dev->ix_txbuf_data[2] |=
					I2C_HEADER_IE_ENABLE;
			break;
		}

		++i;
		if (msg_next == NULL)
			break;
		msg = msg_next;
		msg_next = (i < num - 1) ? msg_next + 1 : NULL;
	}

	i2c_dev->ix_txbuf_count = buf - i2c_dev->ix_txbuf_data;
replay:
	i2c_dev->ix_txbuf_cur = 0;
	init_completion(&i2c_dev->msg_complete);

	/* Program TX FIFO */
	{
		unsigned long flags;

		spin_lock_irqsave(&i2c_dev->fifo_lock, flags);
		tegra_ix_fill_tx_fifo(i2c_dev);
		spin_unlock_irqrestore(&i2c_dev->fifo_lock, flags);
	}

	/* Wait for completion of last I2C transfer */
	timed_out = false;
	if (wait_for_completion_timeout(&i2c_dev->msg_complete,
	    TEGRA_I2C_TIMEOUT) == 0)
		timed_out = true;
	else if (i2c_dev->ix_err_status == 0)
		goto done;

	/* Error handling */
	if (i2c_dev->ix_err_status & I2C_INT_NO_ACK) {
		int retry;

		if (i2c_dev->chipdata->timeout_irq_occurs_before_bus_inactive) {
			/*
			 * In NACK error condition, resetting of I2C controller
			 * happens before STOP condition is properly completed
			 * by I2C controller, so wait for 2 clock cycle to
			 * complete STOP condition.
			 */
			udelay(DIV_ROUND_UP(2 * 1000000,
			    i2c_dev->bus_clk_rate));
		}

		/*
		 * NACK interrupt is generated before the I2C controller
		 * generates the STOP condition on the bus. So wait for 2 clk
		 * periods before resetting the controller so that STOP
		 * condition has been delivered properly.
		 */
		udelay(DIV_ROUND_UP(2 * 1000000, i2c_dev->bus_clk_rate));

		/* Replay last command */
		if (--i2c_dev->ix_nack_retry > 0) {
			const u32 flags = I2C_FIFO_CONTROL_TX_FLUSH |
			    I2C_FIFO_CONTROL_RX_FLUSH;

			i2c_writel(i2c_dev, flags, I2C_FIFO_CONTROL);

			for (retry = 3; retry > 0; --retry) {
				u32 val = i2c_readl(i2c_dev, I2C_FIFO_CONTROL);

				if ((val & flags) == 0)
					goto replay;
				usleep_range(10, 20);
			}
		}
	}

	dev_notice(i2c_dev->dev, "I2C failure to address 0x%02x\n",
		msgs[0].addr);
	if (timed_out)
		dev_notice(i2c_dev->dev, "  Timeout\n");
	if (i2c_dev->ix_err_status & I2C_INT_NO_ACK)
		dev_notice(i2c_dev->dev, "  No ACK\n");
	if (i2c_dev->ix_err_status & I2C_INT_ARBITRATION_LOST)
		dev_notice(i2c_dev->dev, "  Arbitration Lost\n");
	if (i2c_dev->ix_err_status & I2C_INT_TX_FIFO_OVERFLOW)
		dev_notice(i2c_dev->dev, "  TX FIFO Overflow\n");
	dev_notice(i2c_dev->dev,
		"reg: cnfg=0x%08x status=0x%08x int_status=0x%08x int_masked_status=0x%08x pkt_status=0x%08x\n",
		i2c_readl(i2c_dev, I2C_CNFG),
		i2c_readl(i2c_dev, I2C_STATUS),
		i2c_readl(i2c_dev, I2C_INT_STATUS),
		i2c_readl(i2c_dev, I2C_INT_MASKED_STATUS),
		i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));

	tegra_ix_decode_packet(i2c_dev, i2c_dev->ix_txbuf_data,
		i2c_dev->ix_txbuf_count);

	tegra_ix_init(i2c_dev);

	if (i2c_dev->ix_err_status & I2C_INT_ARBITRATION_LOST) {
		if (i2c_dev->chipdata->has_hw_arb_support) {
			i2c_writel(i2c_dev, I2C_BC_ENABLE
					| I2C_BC_SCLK_THRESHOLD
					| I2C_BC_STOP_COND
					| I2C_BC_TERMINATE
					, I2C_BUS_CLEAR_CNFG);

			init_completion(&i2c_dev->msg_complete);
			i2c_dev->ix_int_mask_reg |= I2C_INT_BUS_CLEAR_DONE;
			i2c_writel(i2c_dev, i2c_dev->ix_int_mask_reg,
			    I2C_INT_MASK);

			wait_for_completion_timeout(&i2c_dev->msg_complete,
			    TEGRA_I2C_TIMEOUT);

			i2c_dev->ix_int_mask_reg &= ~I2C_INT_BUS_CLEAR_DONE;
			i2c_writel(i2c_dev, i2c_dev->ix_int_mask_reg,
			    I2C_INT_MASK);

			if (!(i2c_readl(i2c_dev, I2C_BUS_CLEAR_STATUS)
				& I2C_BC_STATUS))
				dev_warn(i2c_dev->dev,
					"Un-recovered Arbitration lost\n");
		} else {
			i2c_algo_busclear_gpio(i2c_dev->dev,
				i2c_dev->scl_gpio, GPIOF_OPEN_DRAIN,
				i2c_dev->sda_gpio, GPIOF_OPEN_DRAIN,
				MAX_BUSCLEAR_CLOCK, 100000);
		}
	}

	i2c_dev->ix_err_status = 0;
	i = -EIO;

done:
	pm_runtime_mark_last_busy(i2c_dev->dev);
	pm_runtime_put_autosuspend(i2c_dev->dev);

	return i;
}

static ssize_t i2c_attr_stats_read(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	char *cp = buf;
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_vi_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	cp += sprintf(cp, "Xfer requests: %u\n", i2c_dev->stat.xfer_requests);
	cp += sprintf(cp, "Total bytes: %u\n", i2c_dev->stat.total_bytes);
	cp += sprintf(cp, "Read requests: %u\n", i2c_dev->stat.reads);
	cp += sprintf(cp, "Read bytes: %u\n", i2c_dev->stat.read_bytes);
	cp += sprintf(cp, "Write requests: %u\n", i2c_dev->stat.writes);
	cp += sprintf(cp, "Write bytes: %u\n", i2c_dev->stat.write_bytes);
	cp += sprintf(cp, "Interrupts - No ACK: %u\n",
		i2c_dev->stat.int_no_ack);
	cp += sprintf(cp, "Interrupts - Arb Lost: %u\n",
		i2c_dev->stat.int_arb_lost);
	cp += sprintf(cp, "Interrupts - TX FIFO Overflow: %u\n",
		i2c_dev->stat.int_tx_fifo_overflow);
	cp += sprintf(cp, "Interrupts - TX FIFO Request: %u\n",
		i2c_dev->stat.int_tx_fifo);
	cp += sprintf(cp, "Interrupts - RX FIFO Available: %u\n",
		i2c_dev->stat.int_rx_fifo);
	cp += sprintf(cp, "Interrupts - Transfer complete: %u\n",
		i2c_dev->stat.int_xfer_complete);

	return cp - buf;
}

static ssize_t i2c_attr_stats_write(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t len)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_vi_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	memset(&i2c_dev->stat, 0, sizeof(i2c_dev->stat));

	return len;
}

static DEVICE_ATTR(stats, S_IRUGO | S_IWUSR,
	i2c_attr_stats_read,
	i2c_attr_stats_write);

static u32 tegra_vi_i2c_func(struct i2c_adapter *adap)
{
	struct tegra_vi_i2c_dev *i2c_dev = i2c_get_adapdata(adap);
	u32 ret = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR |
				I2C_FUNC_PROTOCOL_MANGLING;

	if (i2c_dev->chipdata->has_continue_xfer_support)
		ret |= I2C_FUNC_NOSTART;
	return ret;
}

static const struct i2c_algorithm tegra_vi_i2c_algo = {
	.master_xfer	= tegra_ix_xfer,
	.functionality	= tegra_vi_i2c_func,
};

static int __tegra_vi_i2c_suspend_noirq(struct tegra_vi_i2c_dev *i2c_dev);
static int __tegra_vi_i2c_resume_noirq(struct tegra_vi_i2c_dev *i2c_dev);

static int tegra_vi_i2c_pm_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tegra_vi_i2c_dev *i2c_dev =
		container_of(nb, struct tegra_vi_i2c_dev, pm_nb);

	if (event == TEGRA_PM_SUSPEND)
		__tegra_vi_i2c_suspend_noirq(i2c_dev);
	else if (event == TEGRA_PM_RESUME)
		__tegra_vi_i2c_resume_noirq(i2c_dev);

	return NOTIFY_OK;
}

static struct tegra_i2c_platform_data *parse_i2c_tegra_dt(
	struct platform_device *pdev)
{
	struct tegra_i2c_platform_data *pdata;
	struct device_node *np = pdev->dev.of_node;
	u32 prop;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	if (!of_property_read_u32(np, "clock-frequency", &prop))
		pdata->bus_clk_rate = prop;

	pdata->is_clkon_always = of_property_read_bool(np,
					"nvidia,clock-always-on");

	if (!of_property_read_u32(np, "nvidia,hs-master-code", &prop)) {
		pdata->hs_master_code = prop;
		pdata->is_high_speed_enable = true;
	}

	pdata->bit_banging_xfer_after_shutdown = of_property_read_bool(np,
				"nvidia,bit-banging-xfer-after-shutdown");

	pdata->scl_gpio = of_get_named_gpio(np, "scl-gpio", 0);
	pdata->sda_gpio = of_get_named_gpio(np, "sda-gpio", 0);

	/* Default configuration for device tree initiated driver */
	pdata->slave_addr = 0xFC;
	return pdata;
}

static struct tegra_vi_i2c_chipdata tegra210_vii2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = false,
	.has_xfer_complete_interrupt = true,
	.has_continue_xfer_support = true,
	.has_hw_arb_support = true,
	.has_fast_clock = false,
	.has_clk_divisor_std_fast_mode = true,
	.clk_divisor_std_fast_mode = 0x1f,
	.clk_divisor_fast_plus_mode = 0x10,
	.clk_divisor_hs_mode = 2,
	.clk_multiplier_hs_mode = 13,
	.has_config_load_reg = true,
};

/* Match table for of_platform binding */
static const struct of_device_id tegra_vii2c_of_match[] = {
	{
		.compatible = "nvidia,tegra210-vii2c",
		.data = &tegra210_vii2c_chipdata, },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_vii2c_of_match);

static struct platform_device_id tegra_vi_i2c_devtype[] = {
	{
		.name = "tegra21-vii2c",
		.driver_data = (unsigned long)&tegra210_vii2c_chipdata,
	},
};

static int tegra_vi_i2c_probe(struct platform_device *pdev)
{
	struct tegra_vi_i2c_dev *i2c_dev;
	struct tegra_i2c_platform_data *pdata = pdev->dev.platform_data;
	struct resource *res;
	struct clk *div_clk;
	struct clk *slow_clk;
	struct clk *host1x_clk;
	struct clk *fast_clk = NULL;
	void __iomem *base;
	int irq;
	int ret = 0;
	const struct tegra_vi_i2c_chipdata *chip_data = NULL;
	const struct of_device_id *match;
	int bus_num = -1;
	struct pinctrl *pin;
	struct pinctrl_state *s;

	if (pdev->dev.of_node) {
		match = of_match_device(
			of_match_ptr(tegra_vii2c_of_match), &pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "Device Not matching\n");
			return -ENODEV;
		}
		chip_data = match->data;
		if (!pdata)
			pdata = parse_i2c_tegra_dt(pdev);
	} else {
		chip_data = (struct tegra_vi_i2c_chipdata *)
			pdev->id_entry->driver_data;
		bus_num = pdev->id;
	}

	if (IS_ERR(pdata) || !pdata || !chip_data) {
		dev_err(&pdev->dev, "no platform/chip data?\n");
		return IS_ERR(pdata) ? PTR_ERR(pdata) : -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource\n");
		return -EINVAL;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "no irq resource\n");
		return -EINVAL;
	}
	irq = res->start;

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(*i2c_dev), GFP_KERNEL);
	if (!i2c_dev) {
		dev_err(&pdev->dev, "Could not allocate struct tegra_vi_i2c_dev");
		return -ENOMEM;
	}

	i2c_dev->chipdata = chip_data;
	i2c_dev->dev = &pdev->dev;

	div_clk = devm_clk_get(&pdev->dev, "vii2c");
	if (IS_ERR(div_clk)) {
		dev_err(&pdev->dev, "missing controller clock");
		return PTR_ERR(div_clk);
	}

	slow_clk = devm_clk_get(&pdev->dev, "i2cslow");
	if (IS_ERR(slow_clk)) {
		dev_err(&pdev->dev, "missing slow clock");
		return PTR_ERR(slow_clk);
	}

	host1x_clk = devm_clk_get(&pdev->dev, "host1x");
	if (IS_ERR(host1x_clk)) {
		dev_err(&pdev->dev, "missing host1x clock");
		return PTR_ERR(host1x_clk);
	}

	if (i2c_dev->chipdata->has_fast_clock) {
		fast_clk = devm_clk_get(&pdev->dev, "fast-clk");
		if (IS_ERR(fast_clk)) {
			dev_err(&pdev->dev, "missing bus clock");
			return PTR_ERR(fast_clk);
		}
	}

	if (pdata->is_high_speed_enable) {
		pin = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(pin)) {
			dev_warn(&pdev->dev, "Missing pinctrl device\n");
			goto skip_pinctrl;
		}

		s = pinctrl_lookup_state(pin, "hs_mode");
		if (IS_ERR(s)) {
			dev_warn(&pdev->dev, "Missing hs_mode state\n");
			goto skip_pinctrl;
		}

		ret = pinctrl_select_state(pin, s);
		if (ret < 0)
			dev_err(&pdev->dev, "setting state failed\n");
	}

skip_pinctrl:

	i2c_dev->pull_up_supply = devm_regulator_get(&pdev->dev, "bus-pullup");
	if (IS_ERR(i2c_dev->pull_up_supply)) {
		ret = PTR_ERR(i2c_dev->pull_up_supply);
		if (ret  == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		dev_err(&pdev->dev, "bus-pullup regulator not found: %d\n",
			ret);
		i2c_dev->pull_up_supply = NULL;
	}

	/* get regulator */
	i2c_dev->reg = devm_regulator_get(&pdev->dev, "avdd_dsi_csi");
	if (IS_ERR(i2c_dev->reg)) {
		dev_err(&pdev->dev, "could not get regulator: %ld",
			PTR_ERR(i2c_dev->reg));
		return PTR_ERR(i2c_dev->reg);
	}

	i2c_dev->base = base;
	i2c_dev->div_clk = div_clk;
	i2c_dev->slow_clk = slow_clk;
	i2c_dev->host1x_clk = host1x_clk;
	if (i2c_dev->chipdata->has_fast_clock)
		i2c_dev->fast_clk = fast_clk;
	i2c_dev->irq = irq;
	i2c_dev->cont_id = pdev->id & 0xff;
	i2c_dev->dev = &pdev->dev;
	i2c_dev->is_clkon_always = pdata->is_clkon_always;
	i2c_dev->bus_clk_rate =
		pdata->bus_clk_rate ? pdata->bus_clk_rate : 100000;
	i2c_dev->is_high_speed_enable = pdata->is_high_speed_enable;
	i2c_dev->clk_divisor_non_hs_mode =
			i2c_dev->chipdata->clk_divisor_std_fast_mode;
	if (i2c_dev->bus_clk_rate == 1000000)
		i2c_dev->clk_divisor_non_hs_mode =
			i2c_dev->chipdata->clk_divisor_fast_plus_mode;
	i2c_dev->slave_addr = pdata->slave_addr;
	i2c_dev->hs_master_code = pdata->hs_master_code;
	i2c_dev->bit_banging_xfer_after_shutdown =
			pdata->bit_banging_xfer_after_shutdown;
	init_completion(&i2c_dev->msg_complete);

	spin_lock_init(&i2c_dev->fifo_lock);

	platform_set_drvdata(pdev, i2c_dev);

	if (i2c_dev->is_clkon_always)
		tegra_vi_i2c_clock_enable(i2c_dev);

	ret = devm_request_irq(&pdev->dev, i2c_dev->irq,
		tegra_ix_isr,
		IRQF_NO_SUSPEND, dev_name(&pdev->dev), i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq %i\n", i2c_dev->irq);
		return ret;
	}

	pm_runtime_enable(&pdev->dev);

	i2c_dev->scl_gpio = pdata->scl_gpio;
	i2c_dev->sda_gpio = pdata->sda_gpio;
	i2c_set_adapdata(&i2c_dev->adapter, i2c_dev);
	i2c_dev->adapter.owner = THIS_MODULE;
	i2c_dev->adapter.class = I2C_CLASS_HWMON;
	strlcpy(i2c_dev->adapter.name, "Tegra I2C adapter",
		sizeof(i2c_dev->adapter.name));
	i2c_dev->adapter.algo = &tegra_vi_i2c_algo;
	i2c_dev->adapter.dev.parent = &pdev->dev;
	i2c_dev->adapter.nr = bus_num;
	i2c_dev->adapter.dev.of_node = pdev->dev.of_node;

	if (pdata->retries)
		i2c_dev->adapter.retries = pdata->retries;
	else
		i2c_dev->adapter.retries = TEGRA_I2C_RETRIES;

	if (pdata->timeout)
		i2c_dev->adapter.timeout = pdata->timeout;

	ret = i2c_add_numbered_adapter(&i2c_dev->adapter);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add I2C adapter\n");
		return ret;
	}

	i2c_dev->pm_nb.notifier_call = tegra_vi_i2c_pm_notifier;

	tegra_register_pm_notifier(&i2c_dev->pm_nb);

	tegra_vi_i2c_gpio_init(i2c_dev);

	/* set 100ms autosuspend delay for the adapter device */
	pm_runtime_set_autosuspend_delay(i2c_dev->dev, 100);
	pm_runtime_use_autosuspend(i2c_dev->dev);

	device_create_file(&pdev->dev, &dev_attr_stats);

	return 0;
}

static int tegra_vi_i2c_remove(struct platform_device *pdev)
{
	struct tegra_vi_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	tegra_unregister_pm_notifier(&i2c_dev->pm_nb);
	i2c_del_adapter(&i2c_dev->adapter);

	if (i2c_dev->is_clkon_always)
		tegra_vi_i2c_clock_disable(i2c_dev);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static void tegra_vi_i2c_shutdown(struct platform_device *pdev)
{
	struct tegra_vi_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	dev_info(i2c_dev->dev, "Bus is shut down\n");
	i2c_shutdown_adapter(&i2c_dev->adapter);
	i2c_dev->is_shutdown = true;
}

#ifdef CONFIG_PM_SLEEP
static int __tegra_vi_i2c_suspend_noirq(struct tegra_vi_i2c_dev *i2c_dev)
{
	i2c_dev->is_suspended = true;
	if (i2c_dev->is_clkon_always)
		tegra_vi_i2c_clock_disable(i2c_dev);

	return 0;
}

static int tegra_vi_i2c_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_vi_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	/*
	 * device_prepare takes one ref, so expect usage count to
	 * be 1 at this point.
	 */
#ifdef CONFIG_PM
	if (atomic_read(&dev->power.usage_count) > 1)
		return -EBUSY;
#endif

	i2c_lock_adapter(&i2c_dev->adapter);

	__tegra_vi_i2c_suspend_noirq(i2c_dev);

	i2c_unlock_adapter(&i2c_dev->adapter);

	return 0;
}

static int __tegra_vi_i2c_resume_noirq(struct tegra_vi_i2c_dev *i2c_dev)
{
	if (i2c_dev->is_clkon_always)
		tegra_vi_i2c_clock_enable(i2c_dev);

	i2c_dev->is_suspended = false;

	tegra_ix_init(i2c_dev);

	return 0;
}

static int tegra_vi_i2c_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_vi_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	i2c_lock_adapter(&i2c_dev->adapter);

	__tegra_vi_i2c_resume_noirq(i2c_dev);

	i2c_unlock_adapter(&i2c_dev->adapter);

	return 0;
}
#endif

#ifdef CONFIG_PM

static int tegra_vi_i2c_runtime_resume(struct device *dev)
{
	struct tegra_vi_i2c_dev *i2c_dev = dev_get_drvdata(dev);
	int ret;

	ret = tegra_vi_i2c_power_enable(i2c_dev);
	if (ret)
		goto err_power_enable;

	ret = tegra_vi_i2c_clock_enable(i2c_dev);
	if (ret)
		goto err_clock_enable;

	tegra_ix_init(i2c_dev);

	return 0;

err_clock_enable:
	tegra_vi_i2c_power_disable(i2c_dev);
err_power_enable:
	return ret;
}

static int tegra_vi_i2c_runtime_suspend(struct device *dev)
{
	struct tegra_vi_i2c_dev *i2c_dev = dev_get_drvdata(dev);
	int ret;

	tegra_vi_i2c_clock_disable(i2c_dev);

	ret = tegra_vi_i2c_power_disable(i2c_dev);
	WARN_ON(ret);

	return 0;
}
#endif

static const struct dev_pm_ops tegra_vii2c_pm = {
#ifdef CONFIG_PM_SLEEP
	.suspend_noirq = tegra_vi_i2c_suspend_noirq,
	.resume_noirq = tegra_vi_i2c_resume_noirq,
#endif
#ifdef CONFIG_PM
	.runtime_suspend = tegra_vi_i2c_runtime_suspend,
	.runtime_resume = tegra_vi_i2c_runtime_resume,
#endif
};

static struct platform_driver tegra_vii2c_driver = {
	.probe   = tegra_vi_i2c_probe,
	.remove  = tegra_vi_i2c_remove,
	.shutdown = tegra_vi_i2c_shutdown,
	.id_table = tegra_vi_i2c_devtype,
	.driver  = {
		.name  = "tegra-vii2c",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_vii2c_of_match),
		.pm    = &tegra_vii2c_pm,
	},
};

module_platform_driver(tegra_vii2c_driver);

MODULE_DESCRIPTION("nVidia Tegra VI-I2C Bus Controller driver");
MODULE_LICENSE("GPL v2");
