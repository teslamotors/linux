/*
 * drivers/i2c/busses/i2c-tegra.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
 *
 * Copyright (C) 2010-2017 NVIDIA Corporation.  All rights reserved.
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
#include <linux/reset.h>
#include <linux/spinlock.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-pm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-tegra.h>
#include <linux/tegra_prod.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/debugfs.h>

#include <asm/unaligned.h>

#define TEGRA_I2C_TIMEOUT			(msecs_to_jiffies(10000))
#define TEGRA_I2C_RETRIES			3
#define BYTES_PER_FIFO_WORD			4

#define I2C_CNFG				0x000
#define I2C_CNFG_DEBOUNCE_CNT_SHIFT		12
#define I2C_CNFG_PACKET_MODE_EN			(1<<10)
#define I2C_CNFG_NEW_MASTER_FSM			(1<<11)
#define I2C_CNFG_MULTI_MASTER_MODE		(1<<17)
#define I2C_STATUS				0x01C
#define I2C_STATUS_BUSY				(1<<8)
#define I2C_SL_CNFG				0x020
#define I2C_SL_CNFG_NACK			(1<<1)
#define I2C_SL_CNFG_NEWSL			(1<<2)
#define I2C_SL_ADDR1				0x02c
#define I2C_SL_ADDR2				0x030
#define I2C_TX_FIFO				0x050
#define I2C_RX_FIFO				0x054
#define I2C_PACKET_TRANSFER_STATUS		0x058
#define I2C_FIFO_CONTROL			0x05c
#define I2C_FIFO_CONTROL_TX_FLUSH		(1<<1)
#define I2C_FIFO_CONTROL_RX_FLUSH		(1<<0)

#define I2C_FIFO_CONTROL_TX_TRIG_SHIFT		5
#define I2C_FIFO_CONTROL_RX_TRIG_SHIFT		2
#define I2C_FIFO_CONTROL_RX_TRIG_1		(0<<2)
#define I2C_FIFO_CONTROL_RX_TRIG_4		(3<<2)
#define I2C_FIFO_CONTROL_RX_TRIG_8		(7<<2)
#define I2C_FIFO_CONTROL_TX_TRIG_1		(0<<5)
#define I2C_FIFO_CONTROL_TX_TRIG_4		(3<<5)
#define I2C_FIFO_CONTROL_TX_TRIG_8		(7<<5)

#define I2C_FIFO_STATUS				0x060
#define I2C_FIFO_STATUS_TX_MASK			0xF0
#define I2C_FIFO_STATUS_TX_SHIFT		4
#define I2C_FIFO_STATUS_RX_MASK			0x0F
#define I2C_FIFO_STATUS_RX_SHIFT		0
#define I2C_INT_MASK				0x064
#define I2C_INT_STATUS				0x068
#define I2C_INT_SOURCE				0x070
#define I2C_INT_BUS_CLEAR_DONE		(1<<11)
#define I2C_INT_PACKET_XFER_COMPLETE		(1<<7)
#define I2C_INT_ALL_PACKETS_XFER_COMPLETE	(1<<6)
#define I2C_INT_TX_FIFO_OVERFLOW		(1<<5)
#define I2C_INT_RX_FIFO_UNDERFLOW		(1<<4)
#define I2C_INT_NO_ACK				(1<<3)
#define I2C_INT_ARBITRATION_LOST		(1<<2)
#define I2C_INT_TX_FIFO_DATA_REQ		(1<<1)
#define I2C_INT_RX_FIFO_DATA_REQ		(1<<0)

#define I2C_CLK_DIVISOR				0x06c
#define I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT	16
#define I2C_CLK_MULTIPLIER_STD_FAST_MODE	8
#define I2C_CLK_DIVISOR_HS_MODE_MASK		0xFFFF

#define DVC_CTRL_REG1				0x000
#define DVC_CTRL_REG1_INTR_EN			(1<<10)
#define DVC_CTRL_REG2				0x004
#define DVC_CTRL_REG3				0x008
#define DVC_CTRL_REG3_SW_PROG			(1<<26)
#define DVC_CTRL_REG3_I2C_DONE_INTR_EN		(1<<30)
#define DVC_STATUS				0x00c
#define DVC_STATUS_I2C_DONE_INTR		(1<<30)

#define I2C_ERR_NONE				0x00
#define I2C_ERR_NO_ACK				0x01
#define I2C_ERR_ARBITRATION_LOST		0x02
#define I2C_ERR_UNKNOWN_INTERRUPT		0x04
#define I2C_ERR_UNEXPECTED_STATUS		0x08

#define PACKET_HEADER0_HEADER_SIZE_SHIFT	28
#define PACKET_HEADER0_PACKET_ID_SHIFT		16
#define PACKET_HEADER0_CONT_ID_SHIFT		12
#define PACKET_HEADER0_CONT_ID_MASK		0xF
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

#define I2C_BUS_CLEAR_CNFG				0x084
#define I2C_BC_SCLK_THRESHOLD				(9<<16)
#define I2C_BC_STOP_COND				(1<<2)
#define I2C_BC_TERMINATE				(1<<1)
#define I2C_BC_ENABLE					(1<<0)

#define I2C_BUS_CLEAR_STATUS				0x088
#define I2C_BC_STATUS					(1<<0)

#define I2C_CONFIG_LOAD				0x08C
#define I2C_MSTR_CONFIG_LOAD			(1 << 0)
#define I2C_SLV_CONFIG_LOAD			(1 << 1)
#define I2C_TIMEOUT_CONFIG_LOAD			(1 << 2)

#define I2C_CLKEN_OVERRIDE			0x090
#define I2C_MST_CORE_CLKEN_OVR			(1 << 0)

#define I2C_MASTER_RESET_CONTROL		0x0A8
#define I2C_CMD_ADDR0				0x004
#define I2C_CMD_ADDR1				0x008
#define I2C_CMD_DATA1				0x00C
#define I2C_CMD_DATA2				0x010
#define I2C_DEBUG_CONTROL			0x0A4
#define I2C_TLOW_SEXT				0x034
#define I2C_CLKEN_OVERRIDE			0x090
#define I2C_INTERRUPT_SET_REGISTER		0x074
#define I2C_INTERFACE_TIMING_0			0x94
#define I2C_TLOW_MASK				0x3F
#define I2C_THIGH_SHIFT				8
#define I2C_THIGH_MASK				(0x3F << I2C_THIGH_SHIFT)
#define I2C_TLOW_DEFAULT_COUNT			0x4
#define I2C_THIGH_DEFAULT_COUNT			0x2

#define SL_ADDR1(addr) (addr & 0xff)
#define SL_ADDR2(addr) ((addr >> 8) & 0xff)

#define MAX_BUSCLEAR_CLOCK			(9 * 8 + 1)
#define I2C_MAX_TRANSFER_LEN			4096
/* Max DMA buffer size = max packet transfer length + size of pkt header */
#define I2C_DMA_MAX_BUF_LEN			(I2C_MAX_TRANSFER_LEN + 12)
#define DATA_DMA_DIR_TX				(1 << 0)
#define DATA_DMA_DIR_RX				(1 << 1)
/* Upto I2C_PIO_MODE_MAX_LEN bytes, controller will use PIO mode,
 * above this, controller will use DMA to fill FIFO.
 * Here MAX PIO len is 20 bytes excluding packet header
 */
#define I2C_PIO_MODE_MAX_LEN			(20)
/* Packet header size in words */
#define I2C_PACKET_HEADER_SIZE			(3)

/* Define speed modes */
#define I2C_STANDARD_MODE			100000
#define I2C_FAST_MODE				400000
#define I2C_FAST_MODE_PLUS			1000000
#define I2C_HS_MODE				3500000

#define I2C_PROFILE_ALL_ADDR			0xFFFF

/*
 * msg_end_type: The bus control which need to be send at end of transfer.
 * @MSG_END_STOP: Send stop pulse at end of transfer.
 * @MSG_END_REPEAT_START: Send repeat start at end of transfer.
 * @MSG_END_CONTINUE: The following on message is coming and so do not send
 *		stop or repeat start.
 */
enum msg_end_type {
	MSG_END_STOP,
	MSG_END_REPEAT_START,
	MSG_END_CONTINUE,
};

struct profile_data {
	struct timespec hwt1, hwt2;
	u64 total_time;
};

enum profile_events {
	PROFL_TOTAL_CONT_TIME,
	PROFL_TOTAL_XFER_TIME,
	PROFL_SLAVE_XFER_TIME,
	PROFL_SLAVE_READ_TIME,
	PROFL_SLAVE_WRITE_TIME,
	PROFL_SIZE,
};

struct tegra_i2c_chipdata {
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
	bool has_multi_master_en_bit;
	bool has_sw_reset_reg;
	bool has_clk_override_reg;
	bool has_reg_write_buffering;
};

struct tegra_i2c_stats {
	unsigned int xfer_requests;
	unsigned int total_bytes;
	unsigned int reads, read_bytes;
	unsigned int writes, write_bytes;
	unsigned int int_no_ack, int_arb_lost, int_tx_fifo_overflow;
	unsigned int int_tx_fifo, int_rx_fifo, int_xfer_complete;
};

/**
 * struct tegra_i2c_dev	- per device i2c context
 * @dev: device reference for power management
 * @adapter: core i2c layer adapter information
 * @div_clk: clock reference for div clock of i2c controller.
 * @fast_clk: clock reference for fast clock of i2c controller.
 * @base: ioremapped registers cookie
 * @cont_id: i2c controller id, used for for packet header
 * @irq: irq number of transfer complete interrupt
 * @is_dvc: identifies the DVC i2c controller, has a different register layout
 * @msg_complete: transfer completion notifier
 * @msg_err: error code for completed message
 * @msg_buf: pointer to current message data
 * @msg_buf_remaining: size of unsent data in the message buffer
 * @msg_read: identifies read transfers
 * @bus_clk_rate: current i2c bus clock rate
 * @is_suspended: prevents i2c controller accesses after suspend is called
 */
struct tegra_i2c_dev {
	struct device *dev;
	struct i2c_adapter adapter;
	struct clk *div_clk;
	struct clk *fast_clk;
	struct reset_control *rstc;
	bool needs_cl_dvfs_clock;
	struct clk *dvfs_ref_clk;
	struct clk *dvfs_soc_clk;
	raw_spinlock_t fifo_lock;
	void __iomem *base;
	phys_addr_t phys;
	int cont_id;
	int irq;
	bool irq_disabled;
	int is_dvc;
	struct completion msg_complete;
	int msg_err;
	int next_msg_err;
	u8 *msg_buf;
	u8 *next_msg_buf;
	u32 packet_header;
	u32 next_packet_header;
	u32 payload_size;
	u32 next_payload_size;
	u32 io_header;
	u32 next_io_header;
	size_t msg_buf_remaining;
	size_t next_msg_buf_remaining;
	int msg_read;
	int next_msg_read;
	struct i2c_msg *msgs;
	int msg_add;
	int next_msg_add;
	int msgs_num;
	unsigned long bus_clk_rate;
	bool is_suspended;
	u16 slave_addr;
	bool is_clkon_always;
	bool is_high_speed_enable;
	u16 hs_master_code;
	u16 clk_divisor_non_hs_mode;
	u16 clk_divisor_hs_mode;
	bool use_single_xfer_complete;
	const struct tegra_i2c_chipdata *chipdata;
	int scl_gpio;
	int sda_gpio;
	struct i2c_algo_bit_data bit_data;
	const struct i2c_algorithm *bit_algo;
	bool bit_banging_xfer_after_shutdown;
	bool is_shutdown;
	struct notifier_block pm_nb;
	bool is_periph_reset_done;
	struct tegra_prod_list *prod_list;
	bool is_multimaster_mode;
	struct dma_chan *rx_dma_chan;
	u32 *rx_dma_buf;
	dma_addr_t rx_dma_phys;
	struct dma_async_tx_descriptor *rx_dma_desc;
	struct dma_chan *tx_dma_chan;
	u32 *tx_dma_buf;
	dma_addr_t tx_dma_phys;
	struct dma_async_tx_descriptor *tx_dma_desc;
	unsigned dma_buf_size;
	bool is_curr_dma_xfer;
	struct completion rx_dma_complete;
	struct completion tx_dma_complete;
	int curr_direction;
	int rx_dma_len;
	dma_cookie_t rx_cookie;
	bool disable_dma_mode;
	u32 low_clock_count;
	u32 high_clock_count;
	struct dentry *debugfs;
	u32 enable_profiling;
	u16 profile_addr;
	struct tegra_i2c_stats stat;
	struct tegra_i2c_stats slave_stat;
	struct profile_data pfd[PROFL_SIZE];
	bool transfer_in_progress;
};

static long long timespec_diff(struct timespec a, struct timespec b)
{
	long long ret = NSEC_PER_SEC * b.tv_sec + b.tv_nsec;
	ret -= NSEC_PER_SEC * a.tv_sec + a.tv_nsec;
	return ret;
}

static void profile_start(struct tegra_i2c_dev *i2c_dev, int slot)
{
	struct profile_data *pfd = &i2c_dev->pfd[slot];

	getnstimeofday(&pfd->hwt1);
}

static void profile_stop(struct tegra_i2c_dev *i2c_dev, int slot)
{
	struct profile_data *pfd = &i2c_dev->pfd[slot];
	unsigned long cputime;

	getnstimeofday(&pfd->hwt2);
	cputime = timespec_diff(pfd->hwt1, pfd->hwt2);

	pfd->total_time += cputime;
}

static int is_profiling_enabled(struct tegra_i2c_dev *i2c_dev, u16 profile_addr)
{
	if (!i2c_dev->enable_profiling)
		return 0;

	if ((profile_addr != I2C_PROFILE_ALL_ADDR) &&
		(i2c_dev->profile_addr != profile_addr))
		return 0;

	return 1;
}

static void dvc_writel(struct tegra_i2c_dev *i2c_dev, u32 val, unsigned long reg)
{
	writel(val, i2c_dev->base + reg);
}

static u32 dvc_readl(struct tegra_i2c_dev *i2c_dev, unsigned long reg)
{
	return readl(i2c_dev->base + reg);
}

static void dvc_i2c_mask_irq(struct tegra_i2c_dev *i2c_dev, u32 mask)
{
	u32 int_mask = dvc_readl(i2c_dev, DVC_CTRL_REG3);
	int_mask &= ~mask;
	dvc_writel(i2c_dev, int_mask, DVC_CTRL_REG3);
}

static void dvc_i2c_unmask_irq(struct tegra_i2c_dev *i2c_dev, u32 mask)
{
	u32 int_mask = dvc_readl(i2c_dev, DVC_CTRL_REG3);
	int_mask |= mask;
	dvc_writel(i2c_dev, int_mask, DVC_CTRL_REG3);
}

/*
 * i2c_writel and i2c_readl will offset the register if necessary to talk
 * to the I2C block inside the DVC block
 */
static unsigned long tegra_i2c_reg_addr(struct tegra_i2c_dev *i2c_dev,
	unsigned long reg)
{
	if (i2c_dev->is_dvc)
		reg += (reg >= I2C_TX_FIFO) ? 0x10 : 0x40;
	return reg;
}

static void i2c_writel(struct tegra_i2c_dev *i2c_dev, u32 val,
	unsigned long reg)
{
	writel(val, i2c_dev->base + tegra_i2c_reg_addr(i2c_dev, reg));

	/* Read back register to make sure that register writes completed */
	if (i2c_dev->chipdata->has_reg_write_buffering) {
		if (reg != I2C_TX_FIFO)
			readl(i2c_dev->base + tegra_i2c_reg_addr(i2c_dev, reg));
	}
}

static u32 i2c_readl(struct tegra_i2c_dev *i2c_dev, unsigned long reg)
{
	return readl(i2c_dev->base + tegra_i2c_reg_addr(i2c_dev, reg));
}

static void i2c_writesl(struct tegra_i2c_dev *i2c_dev, void *data,
	unsigned long reg, int len)
{
	u32 *buf = data;
	while (len--)
		writel(*buf++, i2c_dev->base +
					tegra_i2c_reg_addr(i2c_dev, reg));
}

static void i2c_readsl(struct tegra_i2c_dev *i2c_dev, void *data,
	unsigned long reg, int len)
{
	u32 *buf = data;

	while (len--)
		*buf++ = readl(i2c_dev->base +
					tegra_i2c_reg_addr(i2c_dev, reg));
}

static inline void tegra_i2c_gpio_setscl(void *data, int state)
{
	struct tegra_i2c_dev *i2c_dev = data;

	gpio_set_value(i2c_dev->scl_gpio, state);
}

static inline int tegra_i2c_gpio_getscl(void *data)
{
	struct tegra_i2c_dev *i2c_dev = data;

	return gpio_get_value(i2c_dev->scl_gpio);
}

static inline void tegra_i2c_gpio_setsda(void *data, int state)
{
	struct tegra_i2c_dev *i2c_dev = data;

	gpio_set_value(i2c_dev->sda_gpio, state);
}

static inline int tegra_i2c_gpio_getsda(void *data)
{
	struct tegra_i2c_dev *i2c_dev = data;

	return gpio_get_value(i2c_dev->sda_gpio);
}

static int tegra_i2c_gpio_request(struct tegra_i2c_dev *i2c_dev)
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

static void tegra_i2c_gpio_free(struct tegra_i2c_dev *i2c_dev)
{
	gpio_free(i2c_dev->scl_gpio);
	gpio_free(i2c_dev->sda_gpio);
}

static int tegra_i2c_gpio_xfer(struct i2c_adapter *adap,
	struct i2c_msg msgs[], int num)
{
	struct tegra_i2c_dev *i2c_dev = i2c_get_adapdata(adap);
	int ret;

	ret = tegra_i2c_gpio_request(i2c_dev);
	if (ret < 0)
		return ret;

	ret = i2c_dev->bit_algo->master_xfer(adap, msgs, num);
	if (ret < 0)
		dev_err(i2c_dev->dev, "i2c-bit-algo xfer failed %d\n", ret);

	tegra_i2c_gpio_free(i2c_dev);
	return ret;
}

static int tegra_i2c_gpio_init(struct tegra_i2c_dev *i2c_dev)
{
	struct i2c_algo_bit_data *bit_data = &i2c_dev->bit_data;

	bit_data->setsda = tegra_i2c_gpio_setsda;
	bit_data->getsda = tegra_i2c_gpio_getsda;
	bit_data->setscl = tegra_i2c_gpio_setscl;
	bit_data->getscl = tegra_i2c_gpio_getscl;
	bit_data->data = i2c_dev;
	bit_data->udelay = 20; /* 50KHz */
	bit_data->timeout = HZ; /* 10 ms*/
	i2c_dev->bit_algo = &i2c_bit_algo;
	i2c_dev->adapter.algo_data = bit_data;
	return 0;
}

static void tegra_i2c_rx_dma_complete(void *args)
{
	struct tegra_i2c_dev *i2c_dev = args;
	struct dma_tx_state state;

	dma_sync_single_for_cpu(i2c_dev->dev, i2c_dev->rx_dma_phys,
			i2c_dev->dma_buf_size, DMA_FROM_DEVICE);
	dmaengine_tx_status(i2c_dev->rx_dma_chan, i2c_dev->rx_cookie, &state);
	memcpy(i2c_dev->msg_buf, i2c_dev->rx_dma_buf, i2c_dev->rx_dma_len);
	dma_sync_single_for_device(i2c_dev->dev, i2c_dev->rx_dma_phys,
			i2c_dev->dma_buf_size, DMA_FROM_DEVICE);
	complete(&i2c_dev->rx_dma_complete);
}

static void tegra_i2c_tx_dma_complete(void *args)
{
	struct tegra_i2c_dev *i2c_dev = args;

	complete(&i2c_dev->tx_dma_complete);
}

static int tegra_i2c_start_tx_dma(struct tegra_i2c_dev *i2c_dev, int len)
{
	reinit_completion(&i2c_dev->tx_dma_complete);
	dev_dbg(i2c_dev->dev, "Starting tx dma for len:%d\n", len);
	i2c_dev->tx_dma_desc = dmaengine_prep_slave_single(i2c_dev->tx_dma_chan,
				i2c_dev->tx_dma_phys, len, DMA_MEM_TO_DEV,
				DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!i2c_dev->tx_dma_desc) {
		dev_err(i2c_dev->dev, "Not able to get desc for Tx\n");
		return -EIO;
	}

	i2c_dev->tx_dma_desc->callback = tegra_i2c_tx_dma_complete;
	i2c_dev->tx_dma_desc->callback_param = i2c_dev;

	dmaengine_submit(i2c_dev->tx_dma_desc);
	dma_async_issue_pending(i2c_dev->tx_dma_chan);
	return 0;
}

static int tegra_i2c_start_rx_dma(struct tegra_i2c_dev *i2c_dev, int len)
{
	reinit_completion(&i2c_dev->rx_dma_complete);
	i2c_dev->rx_dma_len = len;

	dev_dbg(i2c_dev->dev, "Starting rx dma for len:%d\n", len);
	i2c_dev->rx_dma_desc = dmaengine_prep_slave_single(i2c_dev->rx_dma_chan,
				i2c_dev->rx_dma_phys, len, DMA_DEV_TO_MEM,
				DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!i2c_dev->rx_dma_desc) {
		dev_err(i2c_dev->dev, "Not able to get desc for Rx\n");
		return -EIO;
	}

	i2c_dev->rx_dma_desc->callback = tegra_i2c_rx_dma_complete;
	i2c_dev->rx_dma_desc->callback_param = i2c_dev;

	dmaengine_submit(i2c_dev->rx_dma_desc);
	dma_async_issue_pending(i2c_dev->rx_dma_chan);
	return 0;
}


static int tegra_i2c_init_dma_param(struct tegra_i2c_dev *i2c_dev,
							bool dma_to_memory)
{
	struct dma_chan *dma_chan;
	u32 *dma_buf;
	dma_addr_t dma_phys;
	int ret;
	struct dma_slave_config dma_sconfig;

	dma_chan = dma_request_slave_channel_reason(i2c_dev->dev,
						dma_to_memory ? "rx" : "tx");
	if (IS_ERR(dma_chan)) {
		ret = PTR_ERR(dma_chan);
		if (ret != -EPROBE_DEFER)
			dev_err(i2c_dev->dev,
				"Dma channel is not available: %d\n", ret);
		return ret;
	}

	dma_buf = dma_alloc_coherent(i2c_dev->dev, i2c_dev->dma_buf_size,
							&dma_phys, GFP_KERNEL);
	if (!dma_buf) {
		dev_err(i2c_dev->dev, "Not able to allocate the dma buffer\n");
		dma_release_channel(dma_chan);
		return -ENOMEM;
	}

	if (dma_to_memory) {
		dma_sconfig.src_addr = i2c_dev->phys + I2C_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.src_maxburst = 0;
	} else {
		dma_sconfig.dst_addr = i2c_dev->phys + I2C_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = 0;
	}

	ret = dmaengine_slave_config(dma_chan, &dma_sconfig);
	if (ret)
		goto scrub;
	if (dma_to_memory) {
		i2c_dev->rx_dma_chan = dma_chan;
		i2c_dev->rx_dma_buf = dma_buf;
		i2c_dev->rx_dma_phys = dma_phys;
	} else {
		i2c_dev->tx_dma_chan = dma_chan;
		i2c_dev->tx_dma_buf = dma_buf;
		i2c_dev->tx_dma_phys = dma_phys;
	}
	return 0;

scrub:
	dma_free_coherent(i2c_dev->dev, i2c_dev->dma_buf_size,
							dma_buf, dma_phys);
	dma_release_channel(dma_chan);
	return ret;
}

static void tegra_i2c_deinit_dma_param(struct tegra_i2c_dev *i2c_dev,
	bool dma_to_memory)
{
	u32 *dma_buf;
	dma_addr_t dma_phys;
	struct dma_chan *dma_chan;

	if (dma_to_memory) {
		dma_buf = i2c_dev->rx_dma_buf;
		dma_chan = i2c_dev->rx_dma_chan;
		dma_phys = i2c_dev->rx_dma_phys;
		i2c_dev->rx_dma_chan = NULL;
		i2c_dev->rx_dma_buf = NULL;
	} else {
		dma_buf = i2c_dev->tx_dma_buf;
		dma_chan = i2c_dev->tx_dma_chan;
		dma_phys = i2c_dev->tx_dma_phys;
		i2c_dev->tx_dma_buf = NULL;
		i2c_dev->tx_dma_chan = NULL;
	}
	if (!dma_chan)
		return;

	dma_free_coherent(i2c_dev->dev, i2c_dev->dma_buf_size,
							dma_buf, dma_phys);
	dma_release_channel(dma_chan);
}

static void tegra_i2c_mask_irq(struct tegra_i2c_dev *i2c_dev, u32 mask)
{
	u32 int_mask = i2c_readl(i2c_dev, I2C_INT_MASK);
	int_mask &= ~mask;
	i2c_writel(i2c_dev, int_mask, I2C_INT_MASK);
}

static void tegra_i2c_unmask_irq(struct tegra_i2c_dev *i2c_dev, u32 mask)
{
	u32 int_mask = i2c_readl(i2c_dev, I2C_INT_MASK);
	int_mask |= mask;
	i2c_writel(i2c_dev, int_mask, I2C_INT_MASK);
}

static int tegra_i2c_flush_fifos(struct tegra_i2c_dev *i2c_dev)
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
		msleep(1);
	}
	return 0;
}

static int tegra_i2c_empty_rx_fifo(struct tegra_i2c_dev *i2c_dev)
{
	u32 val;
	int rx_fifo_avail;
	u8 *buf = i2c_dev->msg_buf;
	size_t buf_remaining = i2c_dev->msg_buf_remaining;
	int words_to_transfer;

	val = i2c_readl(i2c_dev, I2C_FIFO_STATUS);
	rx_fifo_avail = (val & I2C_FIFO_STATUS_RX_MASK) >>
		I2C_FIFO_STATUS_RX_SHIFT;

	/* Rounds down to not include partial word at the end of buf */
	words_to_transfer = buf_remaining / BYTES_PER_FIFO_WORD;
	if (words_to_transfer > rx_fifo_avail)
		words_to_transfer = rx_fifo_avail;

	i2c_readsl(i2c_dev, buf, I2C_RX_FIFO, words_to_transfer);

	buf += words_to_transfer * BYTES_PER_FIFO_WORD;
	buf_remaining -= words_to_transfer * BYTES_PER_FIFO_WORD;
	rx_fifo_avail -= words_to_transfer;

	/*
	 * If there is a partial word at the end of buf, handle it manually to
	 * prevent overwriting past the end of buf
	 */
	if (rx_fifo_avail > 0 && buf_remaining > 0) {
		WARN_ON(buf_remaining > 3);
		val = i2c_readl(i2c_dev, I2C_RX_FIFO);
		memcpy(buf, &val, buf_remaining);
		buf_remaining = 0;
		rx_fifo_avail--;
	}

	WARN_ON(rx_fifo_avail > 0 && buf_remaining > 0);
	i2c_dev->msg_buf_remaining = buf_remaining;
	i2c_dev->msg_buf = buf;

	return 0;
}

static int tegra_i2c_fill_tx_fifo(struct tegra_i2c_dev *i2c_dev)
{
	u32 val;
	int tx_fifo_avail;
	u8 *buf;
	size_t buf_remaining;
	int words_to_transfer;

	if (!i2c_dev->msg_buf_remaining)
		return 0;
	buf = i2c_dev->msg_buf;
	buf_remaining = i2c_dev->msg_buf_remaining;

	val = i2c_readl(i2c_dev, I2C_FIFO_STATUS);
	tx_fifo_avail = (val & I2C_FIFO_STATUS_TX_MASK) >>
		I2C_FIFO_STATUS_TX_SHIFT;

	/* Rounds down to not include partial word at the end of buf */
	words_to_transfer = buf_remaining / BYTES_PER_FIFO_WORD;

	/* It's very common to have < 4 bytes, so optimize that case. */
	if (words_to_transfer) {
		if (words_to_transfer > tx_fifo_avail)
			words_to_transfer = tx_fifo_avail;

		/*
		 * Update state before writing to FIFO.  If this casues us
		 * to finish writing all bytes (AKA buf_remaining goes to 0) we
		 * have a potential for an interrupt (PACKET_XFER_COMPLETE is
		 * not maskable).  We need to make sure that the isr sees
		 * buf_remaining as 0 and doesn't call us back re-entrantly.
		 */
		buf_remaining -= words_to_transfer * BYTES_PER_FIFO_WORD;
		tx_fifo_avail -= words_to_transfer;
		i2c_dev->msg_buf_remaining = buf_remaining;
		i2c_dev->msg_buf = buf +
			words_to_transfer * BYTES_PER_FIFO_WORD;
		barrier();

		i2c_writesl(i2c_dev, buf, I2C_TX_FIFO, words_to_transfer);

		buf += words_to_transfer * BYTES_PER_FIFO_WORD;
	}

	/*
	 * If there is a partial word at the end of buf, handle it manually to
	 * prevent reading past the end of buf, which could cross a page
	 * boundary and fault.
	 */
	if (tx_fifo_avail > 0 && buf_remaining > 0) {
		if (buf_remaining > 3) {
			dev_err(i2c_dev->dev,
				"Remaining buffer more than 3 %zd\n",
				buf_remaining);
			WARN_ON(1);
		}
		memcpy(&val, buf, buf_remaining);

		/* Again update before writing to FIFO to make sure isr sees. */
		i2c_dev->msg_buf_remaining = 0;
		i2c_dev->msg_buf = NULL;
		barrier();

		i2c_writel(i2c_dev, val, I2C_TX_FIFO);
	}

	return 0;
}

/*
 * One of the Tegra I2C blocks is inside the DVC (Digital Voltage Controller)
 * block.  This block is identical to the rest of the I2C blocks, except that
 * it only supports master mode, it has registers moved around, and it needs
 * some extra init to get it into I2C mode.  The register moves are handled
 * by i2c_readl and i2c_writel
 */
static void tegra_dvc_init(struct tegra_i2c_dev *i2c_dev)
{
	u32 val = 0;
	val = dvc_readl(i2c_dev, DVC_CTRL_REG3);
	val |= DVC_CTRL_REG3_SW_PROG;
	dvc_writel(i2c_dev, val, DVC_CTRL_REG3);

	val = dvc_readl(i2c_dev, DVC_CTRL_REG1);
	val |= DVC_CTRL_REG1_INTR_EN;
	dvc_writel(i2c_dev, val, DVC_CTRL_REG1);
}

static void tegra_i2c_slave_init(struct tegra_i2c_dev *i2c_dev)
{
	u32 val = I2C_SL_CNFG_NEWSL | I2C_SL_CNFG_NACK;

	i2c_writel(i2c_dev, val, I2C_SL_CNFG);

	if (i2c_dev->slave_addr) {
		u16 addr = i2c_dev->slave_addr;

		i2c_writel(i2c_dev, SL_ADDR1(addr), I2C_SL_ADDR1);
		i2c_writel(i2c_dev, SL_ADDR2(addr), I2C_SL_ADDR2);
	}
}

static inline int tegra_i2c_clock_enable(struct tegra_i2c_dev *i2c_dev)
{
	int ret;

	if (i2c_dev->needs_cl_dvfs_clock) {
		ret = clk_prepare_enable(i2c_dev->dvfs_soc_clk);
		if (ret < 0) {
			dev_err(i2c_dev->dev,
				"Error in enabling dvfs soc clock %d\n", ret);
			return ret;
		}
		ret = clk_prepare_enable(i2c_dev->dvfs_ref_clk);
		if (ret < 0) {
			dev_err(i2c_dev->dev,
				"Error in enabling dvfs ref clock %d\n", ret);
			goto ref_clk_err;
		}
	}
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
	return 0;

div_clk_err:
	if (i2c_dev->chipdata->has_fast_clock)
		clk_disable_unprepare(i2c_dev->fast_clk);
fast_clk_err:
	if (i2c_dev->needs_cl_dvfs_clock)
		clk_disable_unprepare(i2c_dev->dvfs_ref_clk);
ref_clk_err:
	if (i2c_dev->needs_cl_dvfs_clock)
		clk_disable_unprepare(i2c_dev->dvfs_soc_clk);
	return ret;
}

static inline void tegra_i2c_clock_disable(struct tegra_i2c_dev *i2c_dev)
{
	clk_disable_unprepare(i2c_dev->div_clk);
	if (i2c_dev->chipdata->has_fast_clock)
		clk_disable_unprepare(i2c_dev->fast_clk);
	if (i2c_dev->needs_cl_dvfs_clock) {
		clk_disable_unprepare(i2c_dev->dvfs_soc_clk);
		clk_disable_unprepare(i2c_dev->dvfs_ref_clk);
	}
}

static int tegra_i2c_set_clk_rate(struct tegra_i2c_dev *i2c_dev)
{
	u32 clk_multiplier;
	int ret;


	switch (i2c_dev->bus_clk_rate) {
	case I2C_HS_MODE:
		clk_multiplier = i2c_dev->chipdata->clk_multiplier_hs_mode;
		clk_multiplier *= (i2c_dev->clk_divisor_hs_mode + 1);
		break;
	case I2C_FAST_MODE_PLUS:
		clk_multiplier = (i2c_dev->low_clock_count +
				  i2c_dev->high_clock_count + 2);
		clk_multiplier *= (i2c_dev->clk_divisor_non_hs_mode + 1);
		break;
	case I2C_STANDARD_MODE:
	case I2C_FAST_MODE:
	default:
		clk_multiplier = (i2c_dev->low_clock_count +
				  i2c_dev->high_clock_count + 2);
		clk_multiplier *= (i2c_dev->clk_divisor_non_hs_mode + 1);
		break;
	}

	ret = clk_set_rate(i2c_dev->div_clk, i2c_dev->bus_clk_rate
							* clk_multiplier);
	if (ret < 0) {
		dev_err(i2c_dev->dev, "set clock rate %lu failed: %d\n",
			i2c_dev->bus_clk_rate * clk_multiplier, ret);
		return ret;
	}
	return ret;
}

static void tegra_i2c_config_prod_settings(struct tegra_i2c_dev *i2c_dev)
{
	char *prod_name;
	int ret;

	switch (i2c_dev->bus_clk_rate) {
	case I2C_FAST_MODE:
		prod_name = "prod_c_fm";
		break;
	case I2C_FAST_MODE_PLUS:
		prod_name = "prod_c_fmplus";
		break;
	case I2C_HS_MODE:
		prod_name = "prod_c_hs";
		break;
	case I2C_STANDARD_MODE:
	default:
		prod_name = "prod_c_sm";
		break;
	}

	ret = tegra_prod_set_by_name(&i2c_dev->base, prod_name,
				     i2c_dev->prod_list);
	if (ret == 0)
		dev_info(i2c_dev->dev, "setting prod: %s\n", prod_name);
}

static void tegra_i2c_get_clk_parameters(struct tegra_i2c_dev *i2c_dev)
{
	u32 val;

	val = i2c_readl(i2c_dev, I2C_INTERFACE_TIMING_0);
	i2c_dev->low_clock_count = val & I2C_TLOW_MASK;
	i2c_dev->high_clock_count = (val & I2C_THIGH_MASK) >> I2C_THIGH_SHIFT;

	val = i2c_readl(i2c_dev, I2C_CLK_DIVISOR);
	i2c_dev->clk_divisor_hs_mode = val & I2C_CLK_DIVISOR_HS_MODE_MASK;
	i2c_dev->clk_divisor_non_hs_mode = (val >>
			I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT);
}

static int tegra_i2c_init(struct tegra_i2c_dev *i2c_dev)
{
	u32 val;
	int err = 0;
	u32 clk_divisor = 0;
	unsigned long timeout;

	err = tegra_i2c_clock_enable(i2c_dev);
	if (err < 0) {
		dev_err(i2c_dev->dev, "Clock enable failed %d\n", err);
		return err;
	}

	if (i2c_dev->chipdata->has_sw_reset_reg) {
		if (i2c_dev->is_periph_reset_done) {
			/* If already controller reset is done through
			   clock reset control register, then use SW reset */
			i2c_writel(i2c_dev, 1, I2C_MASTER_RESET_CONTROL);
			udelay(2);
			i2c_writel(i2c_dev, 0, I2C_MASTER_RESET_CONTROL);
			goto skip_periph_reset;
		}
	}
	reset_control_reset(i2c_dev->rstc);
	i2c_dev->is_periph_reset_done = true;

skip_periph_reset:
	if (i2c_dev->is_dvc)
		tegra_dvc_init(i2c_dev);

	val = I2C_CNFG_NEW_MASTER_FSM;
	if (!i2c_dev->is_high_speed_enable)
		val |= (0x2 << I2C_CNFG_DEBOUNCE_CNT_SHIFT);

	i2c_writel(i2c_dev, val, I2C_CNFG);
	i2c_writel(i2c_dev, 0, I2C_INT_MASK);

	clk_divisor |= i2c_dev->clk_divisor_hs_mode;
	if (i2c_dev->chipdata->has_clk_divisor_std_fast_mode)
		clk_divisor |= i2c_dev->clk_divisor_non_hs_mode
				<< I2C_CLK_DIVISOR_STD_FAST_MODE_SHIFT;
	i2c_writel(i2c_dev, clk_divisor, I2C_CLK_DIVISOR);

	if (i2c_dev->prod_list)
		tegra_i2c_config_prod_settings(i2c_dev);

	tegra_i2c_get_clk_parameters(i2c_dev);

	err = tegra_i2c_set_clk_rate(i2c_dev);
	if (err < 0)
		return err;

	if (!i2c_dev->is_dvc) {
		u32 sl_cfg = i2c_readl(i2c_dev, I2C_SL_CNFG);
		sl_cfg |= I2C_SL_CNFG_NACK | I2C_SL_CNFG_NEWSL;
		i2c_writel(i2c_dev, sl_cfg, I2C_SL_CNFG);
		i2c_writel(i2c_dev, 0xfc, I2C_SL_ADDR1);
		i2c_writel(i2c_dev, 0x00, I2C_SL_ADDR2);

	}

	val = 7 << I2C_FIFO_CONTROL_TX_TRIG_SHIFT |
		0 << I2C_FIFO_CONTROL_RX_TRIG_SHIFT;
	i2c_writel(i2c_dev, val, I2C_FIFO_CONTROL);

	if (!i2c_dev->is_dvc)
		tegra_i2c_slave_init(i2c_dev);

	if (tegra_i2c_flush_fifos(i2c_dev))
		err = -ETIMEDOUT;

	if (i2c_dev->is_multimaster_mode && i2c_dev->chipdata->has_clk_override_reg)
		i2c_writel(i2c_dev, I2C_MST_CORE_CLKEN_OVR, I2C_CLKEN_OVERRIDE);

	if (i2c_dev->chipdata->has_config_load_reg) {
		i2c_writel(i2c_dev, I2C_MSTR_CONFIG_LOAD, I2C_CONFIG_LOAD);
		timeout = jiffies + msecs_to_jiffies(1000);
		while (i2c_readl(i2c_dev, I2C_CONFIG_LOAD) != 0) {
			if (time_after(jiffies, timeout)) {
				dev_warn(i2c_dev->dev, "timeout waiting for config load\n");
				return -ETIMEDOUT;
			}
			msleep(1);
		}
	}

	tegra_i2c_clock_disable(i2c_dev);

	if (i2c_dev->irq_disabled) {
		i2c_dev->irq_disabled = 0;
		enable_irq(i2c_dev->irq);
	}

	return err;
}
static int tegra_i2c_copy_next_to_current(struct tegra_i2c_dev *i2c_dev)
{
	i2c_dev->msg_buf = i2c_dev->next_msg_buf;
	i2c_dev->msg_buf_remaining = i2c_dev->next_msg_buf_remaining;
	i2c_dev->msg_err = i2c_dev->next_msg_err;
	i2c_dev->msg_read = i2c_dev->next_msg_read;
	i2c_dev->msg_add = i2c_dev->next_msg_add;
	i2c_dev->packet_header = i2c_dev->next_packet_header;
	i2c_dev->io_header = i2c_dev->next_io_header;
	i2c_dev->payload_size = i2c_dev->next_payload_size;

	return 0;
}

static int tegra_i2c_disable_packet_mode(struct tegra_i2c_dev *i2c_dev)
{
	u32 cnfg;
	unsigned long timeout;

	cnfg = i2c_readl(i2c_dev, I2C_CNFG);
	if (cnfg & I2C_CNFG_PACKET_MODE_EN)
		i2c_writel(i2c_dev, cnfg & (~I2C_CNFG_PACKET_MODE_EN),
				I2C_CNFG);

	if (i2c_dev->chipdata->has_config_load_reg) {
		i2c_writel(i2c_dev, I2C_MSTR_CONFIG_LOAD,
				I2C_CONFIG_LOAD);
		timeout = jiffies + msecs_to_jiffies(1000);
		while (i2c_readl(i2c_dev, I2C_CONFIG_LOAD) != 0) {
			if (time_after(jiffies, timeout)) {
				dev_err(i2c_dev->dev,
						"timeout config_load");
				return -ETIMEDOUT;
			}
			udelay(2);
		}
	}
	return 0;
}

static void tegra_i2c_reg_dump(struct tegra_i2c_dev *i2c_dev,
		struct i2c_msg *msg, struct i2c_msg *next_msg)
{
	dev_err(i2c_dev->dev, "--- register dump for debugging ----\n");
	dev_err(i2c_dev->dev, "I2C_CNFG - 0x%x\n",
			i2c_readl(i2c_dev, I2C_CNFG));
	dev_err(i2c_dev->dev, "I2C_PACKET_TRANSFER_STATUS - 0x%x\n",
			i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));
	dev_err(i2c_dev->dev, "I2C_FIFO_CONTROL - 0x%x\n",
			i2c_readl(i2c_dev, I2C_FIFO_CONTROL));
	dev_err(i2c_dev->dev, "I2C_FIFO_STATUS - 0x%x\n",
			i2c_readl(i2c_dev, I2C_FIFO_STATUS));
	dev_err(i2c_dev->dev, "I2C_INT_MASK - 0x%x\n",
			i2c_readl(i2c_dev, I2C_INT_MASK));
	dev_err(i2c_dev->dev, "I2C_INT_STATUS - 0x%x\n",
			i2c_readl(i2c_dev, I2C_INT_STATUS));
	dev_err(i2c_dev->dev, "irq_disabled - %d\n",
			i2c_dev->irq_disabled);
	dev_err(i2c_dev->dev, "buf_remaining - %zd\n",
			i2c_dev->msg_buf_remaining);
	if (msg != NULL) {
		dev_err(i2c_dev->dev, "msg->len - %d\n", msg->len);
		dev_err(i2c_dev->dev, "is_msg_write - %d\n",
				!(msg->flags & I2C_M_RD));
	}
	if (next_msg != NULL) {
		dev_err(i2c_dev->dev, "next_msg->len - %d\n",
				next_msg->len);
		dev_err(i2c_dev->dev, "is_next_msg_write - %d\n",
				!(next_msg->flags & I2C_M_RD));
	}
	if (i2c_dev->msgs) {
		struct i2c_msg *msgs = i2c_dev->msgs;
		int i;

		for (i = 0; i < i2c_dev->msgs_num; i++)
			dev_err(i2c_dev->dev,
				"msgs[%d] %c, addr=0x%04x, len=%d\n",
				i, (msgs[i].flags & I2C_M_RD) ? 'R' : 'W',
				msgs[i].addr, msgs[i].len);
	}
}

static irqreturn_t tegra_i2c_isr(int irq, void *dev_id)
{
	u32 status;
	unsigned long flags = 0;

	const u32 status_err = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST
					| I2C_INT_TX_FIFO_OVERFLOW;
	struct tegra_i2c_dev *i2c_dev = dev_id;
	u32 mask;

	raw_spin_lock_irqsave(&i2c_dev->fifo_lock, flags);
	if (!i2c_dev->transfer_in_progress) {
		dev_err(i2c_dev->dev, "ISR called even though no transfer\n");
		goto done;
	}

	status = i2c_readl(i2c_dev, I2C_INT_STATUS);

	if (status == 0) {
		dev_err(i2c_dev->dev, "unknown interrupt Add 0x%02x\n",
						i2c_dev->msg_add);
		i2c_dev->msg_err |= I2C_ERR_UNKNOWN_INTERRUPT;

		if (!i2c_dev->irq_disabled) {
			disable_irq_nosync(i2c_dev->irq);
			i2c_dev->irq_disabled = 1;
		}
		/* Clear all interrupts */
		status = 0xFFFFFFFFU;
		goto err;
	}

	if (unlikely(status & status_err)) {
		tegra_i2c_disable_packet_mode(i2c_dev);
		dev_dbg(i2c_dev->dev, "I2c error status 0x%08x\n", status);
		if (status & I2C_INT_NO_ACK) {
			if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR))
				++i2c_dev->stat.int_no_ack;
			if (is_profiling_enabled(i2c_dev, i2c_dev->msg_add))
				++i2c_dev->slave_stat.int_no_ack;
			i2c_dev->msg_err |= I2C_ERR_NO_ACK;
			dev_dbg(i2c_dev->dev, "no acknowledge from address"
					" 0x%x\n", i2c_dev->msg_add);
			dev_dbg(i2c_dev->dev, "Packet status 0x%08x\n",
				i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));
		}

		if (status & I2C_INT_ARBITRATION_LOST) {
			if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR))
				++i2c_dev->stat.int_arb_lost;
			if (is_profiling_enabled(i2c_dev, i2c_dev->msg_add))
				++i2c_dev->slave_stat.int_arb_lost;
			i2c_dev->msg_err |= I2C_ERR_ARBITRATION_LOST;
			dev_dbg(i2c_dev->dev, "arbitration lost during "
				" communicate to add 0x%x\n", i2c_dev->msg_add);
			dev_dbg(i2c_dev->dev, "Packet status 0x%08x\n",
				i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));
		}

		if (status & I2C_INT_TX_FIFO_OVERFLOW) {
			if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR))
				++i2c_dev->stat.int_tx_fifo_overflow;
			if (is_profiling_enabled(i2c_dev, i2c_dev->msg_add))
				++i2c_dev->slave_stat.int_tx_fifo_overflow;
			i2c_dev->msg_err |= I2C_INT_TX_FIFO_OVERFLOW;
			dev_dbg(i2c_dev->dev, "Tx fifo overflow during "
				" communicate to add 0x%x\n", i2c_dev->msg_add);
			dev_dbg(i2c_dev->dev, "Packet status 0x%08x\n",
				i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));
		}
		goto err;
	}

	if (i2c_dev->chipdata->has_hw_arb_support &&
			(status & I2C_INT_BUS_CLEAR_DONE))
		goto err;

	if (unlikely((i2c_readl(i2c_dev, I2C_STATUS) & I2C_STATUS_BUSY)
				&& (status == I2C_INT_TX_FIFO_DATA_REQ)
				&& i2c_dev->msg_read
				&& i2c_dev->msg_buf_remaining)) {
		dev_err(i2c_dev->dev, "unexpected status\n");
		i2c_dev->msg_err |= I2C_ERR_UNEXPECTED_STATUS;

		if (!i2c_dev->irq_disabled) {
			disable_irq_nosync(i2c_dev->irq);
			i2c_dev->irq_disabled = 1;
		}

		goto err;
	}

	if (i2c_dev->msg_read && (status & I2C_INT_RX_FIFO_DATA_REQ)) {
		if (i2c_dev->msg_buf_remaining)
			tegra_i2c_empty_rx_fifo(i2c_dev);
		else {
			dev_err(i2c_dev->dev, "RX FIFO data req irq, remain = 0\n");
			tegra_i2c_reg_dump(i2c_dev, NULL, NULL);
			WARN_ON(1);
		}
		if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR))
			++i2c_dev->stat.int_rx_fifo;
		if (is_profiling_enabled(i2c_dev, i2c_dev->msg_add))
			++i2c_dev->slave_stat.int_rx_fifo;
	}

	if (!i2c_dev->msg_read && (status & I2C_INT_TX_FIFO_DATA_REQ)) {
		if (i2c_dev->msg_buf_remaining)
			tegra_i2c_fill_tx_fifo(i2c_dev);
		else
			tegra_i2c_mask_irq(i2c_dev, I2C_INT_TX_FIFO_DATA_REQ);

		if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR))
			++i2c_dev->stat.int_tx_fifo;
		if (is_profiling_enabled(i2c_dev, i2c_dev->msg_add))
			++i2c_dev->slave_stat.int_tx_fifo;
	}

	i2c_writel(i2c_dev, status, I2C_INT_STATUS);

	if (i2c_dev->is_dvc)
		dvc_writel(i2c_dev, DVC_STATUS_I2C_DONE_INTR, DVC_STATUS);

	if (status & I2C_INT_ALL_PACKETS_XFER_COMPLETE) {
		if (i2c_dev->msg_buf_remaining) {
			dev_err(i2c_dev->dev, "all_pkt_com_irq,remain != 0\n");
			dev_err(i2c_dev->dev, "Irq status - 0x%x\n", status);
			tegra_i2c_reg_dump(i2c_dev, NULL, NULL);
			WARN_ON(1);
		}
		complete(&i2c_dev->msg_complete);
		if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR))
			++i2c_dev->stat.int_xfer_complete;
		if (is_profiling_enabled(i2c_dev, i2c_dev->msg_add))
			++i2c_dev->slave_stat.int_xfer_complete;
	} else if ((status & I2C_INT_PACKET_XFER_COMPLETE)
				&& i2c_dev->use_single_xfer_complete) {
		if (i2c_dev->msg_buf_remaining) {
			dev_err(i2c_dev->dev, "pkt_com_irq,remain != 0\n");
			dev_err(i2c_dev->dev, "Irq status - 0x%x\n", status);
			tegra_i2c_reg_dump(i2c_dev, NULL, NULL);
			WARN_ON(1);
		}
		complete(&i2c_dev->msg_complete);
		if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR))
			++i2c_dev->stat.int_xfer_complete;
		if (is_profiling_enabled(i2c_dev, i2c_dev->msg_add))
			++i2c_dev->slave_stat.int_xfer_complete;
	}

	goto done;
err:
	dev_dbg(i2c_dev->dev, "reg: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		 i2c_readl(i2c_dev, I2C_CNFG), i2c_readl(i2c_dev, I2C_STATUS),
		 i2c_readl(i2c_dev, I2C_INT_STATUS),
		 i2c_readl(i2c_dev, I2C_PACKET_TRANSFER_STATUS));

	dev_dbg(i2c_dev->dev, "packet: 0x%08x %u 0x%08x\n",
		 i2c_dev->packet_header, i2c_dev->payload_size,
		 i2c_dev->io_header);

	if (i2c_dev->msgs) {
		struct i2c_msg *msgs = i2c_dev->msgs;
		int i;

		for (i = 0; i < i2c_dev->msgs_num; i++)
			dev_dbg(i2c_dev->dev,
				 "msgs[%d] %c, addr=0x%04x, len=%d\n",
				 i, (msgs[i].flags & I2C_M_RD) ? 'R' : 'W',
				 msgs[i].addr, msgs[i].len);
	}

	mask = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST |
		I2C_INT_PACKET_XFER_COMPLETE | I2C_INT_TX_FIFO_DATA_REQ |
		I2C_INT_RX_FIFO_DATA_REQ | I2C_INT_TX_FIFO_OVERFLOW;

	i2c_writel(i2c_dev, status, I2C_INT_STATUS);

	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	if (!(i2c_dev->use_single_xfer_complete &&
			i2c_dev->chipdata->has_xfer_complete_interrupt))
		mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	if (i2c_dev->chipdata->has_hw_arb_support)
		mask |= I2C_INT_BUS_CLEAR_DONE;

	/* An error occurred, mask all interrupts */
	tegra_i2c_mask_irq(i2c_dev, mask);

	/* An error occured, mask dvc interrupt */
	if (i2c_dev->is_dvc) {
		dvc_i2c_mask_irq(i2c_dev, DVC_CTRL_REG3_I2C_DONE_INTR_EN);
		dvc_writel(i2c_dev, DVC_STATUS_I2C_DONE_INTR, DVC_STATUS);
	}

	if (i2c_dev->is_curr_dma_xfer) {
		if (i2c_dev->curr_direction == DATA_DMA_DIR_TX) {
			dmaengine_terminate_all(i2c_dev->tx_dma_chan);
			complete(&i2c_dev->tx_dma_complete);
		} else {
			dmaengine_terminate_all(i2c_dev->rx_dma_chan);
			complete(&i2c_dev->rx_dma_complete);
		}
	}

	complete(&i2c_dev->msg_complete);

done:
	raw_spin_unlock_irqrestore(&i2c_dev->fifo_lock, flags);

	return IRQ_HANDLED;
}

static int tegra_i2c_send_next_read_msg_pkt_header(struct tegra_i2c_dev *i2c_dev, struct i2c_msg *next_msg, enum msg_end_type end_state)
{
	i2c_dev->next_msg_buf = next_msg->buf;
	i2c_dev->next_msg_buf_remaining = next_msg->len;
	i2c_dev->next_msg_err = I2C_ERR_NONE;
	i2c_dev->next_msg_read = 1;
	i2c_dev->next_msg_add = next_msg->addr;
	i2c_dev->next_packet_header = (0 << PACKET_HEADER0_HEADER_SIZE_SHIFT) |
			PACKET_HEADER0_PROTOCOL_I2C |
			(i2c_dev->cont_id << PACKET_HEADER0_CONT_ID_SHIFT) |
			(1 << PACKET_HEADER0_PACKET_ID_SHIFT);

	i2c_writel(i2c_dev, i2c_dev->next_packet_header, I2C_TX_FIFO);

	i2c_dev->next_payload_size = next_msg->len - 1;

	i2c_writel(i2c_dev, i2c_dev->next_payload_size, I2C_TX_FIFO);

	i2c_dev->next_io_header = I2C_HEADER_IE_ENABLE;

	if (end_state == MSG_END_CONTINUE)
		i2c_dev->next_io_header |= I2C_HEADER_CONTINUE_XFER;
	else if (end_state == MSG_END_REPEAT_START)
		i2c_dev->next_io_header |= I2C_HEADER_REPEAT_START;

	if (next_msg->flags & I2C_M_TEN) {
		i2c_dev->next_io_header |= next_msg->addr;
		i2c_dev->next_io_header |= I2C_HEADER_10BIT_ADDR;
	} else {
		i2c_dev->next_io_header |=
				(next_msg->addr << I2C_HEADER_SLAVE_ADDR_SHIFT);
	}
	if (next_msg->flags & I2C_M_IGNORE_NAK)
		i2c_dev->next_io_header |= I2C_HEADER_CONT_ON_NAK;

	i2c_dev->next_io_header |= I2C_HEADER_READ;

	if (i2c_dev->is_high_speed_enable) {
		i2c_dev->next_io_header |= I2C_HEADER_HIGHSPEED_MODE;
		i2c_dev->next_io_header |= ((i2c_dev->hs_master_code & 0x7)
					<<  I2C_HEADER_MASTER_ADDR_SHIFT);
	}
	i2c_writel(i2c_dev, i2c_dev->next_io_header, I2C_TX_FIFO);

	return 0;
}

static int tegra_i2c_issue_bus_clear(struct tegra_i2c_dev *i2c_dev)
{
	unsigned long timeout;

	if (i2c_dev->chipdata->has_hw_arb_support) {
		int ret;
		reinit_completion(&i2c_dev->msg_complete);
		i2c_writel(i2c_dev, I2C_BC_ENABLE
				| I2C_BC_SCLK_THRESHOLD
				| I2C_BC_STOP_COND
				| I2C_BC_TERMINATE
				, I2C_BUS_CLEAR_CNFG);
		if (i2c_dev->chipdata->has_config_load_reg) {
			i2c_writel(i2c_dev, I2C_MSTR_CONFIG_LOAD,
					I2C_CONFIG_LOAD);
			timeout = jiffies + msecs_to_jiffies(1000);
			while (i2c_readl(i2c_dev, I2C_CONFIG_LOAD) != 0) {
				if (time_after(jiffies, timeout)) {
					dev_warn(i2c_dev->dev,
						"timeout config_load");
					return -ETIMEDOUT;
				}
				msleep(1);
			}
		}
		tegra_i2c_unmask_irq(i2c_dev, I2C_INT_BUS_CLEAR_DONE);

		ret = wait_for_completion_timeout(&i2c_dev->msg_complete,
				TEGRA_I2C_TIMEOUT);
		if (ret == 0)
			dev_err(i2c_dev->dev, "timed out for bus clear\n");
		if (!(i2c_readl(i2c_dev, I2C_BUS_CLEAR_STATUS) & I2C_BC_STATUS))
			dev_warn(i2c_dev->dev, "Un-recovered Arbitration lost\n");
	} else {
		i2c_algo_busclear_gpio(i2c_dev->dev,
				i2c_dev->scl_gpio, GPIOF_OPEN_DRAIN,
				i2c_dev->sda_gpio, GPIOF_OPEN_DRAIN,
				MAX_BUSCLEAR_CLOCK, 100000);
	}

	return 0;
}

static void tegra_i2c_fill_packet_header(struct tegra_i2c_dev *i2c_dev,
	struct i2c_msg *msg, enum msg_end_type end_state,
	struct i2c_msg *next_msg, enum msg_end_type next_msg_end_state,
	u32 *packet_header)
{
	/* Generic header packet */
	i2c_dev->packet_header = (0 << PACKET_HEADER0_HEADER_SIZE_SHIFT)
		| PACKET_HEADER0_PROTOCOL_I2C |
		(i2c_dev->cont_id << PACKET_HEADER0_CONT_ID_SHIFT) |
		(1 << PACKET_HEADER0_PACKET_ID_SHIFT);
	packet_header[0] = i2c_dev->packet_header;

	/* Payload size */
	i2c_dev->payload_size = msg->len - 1;
	packet_header[1] = i2c_dev->payload_size;

	/* IO header */
	i2c_dev->use_single_xfer_complete = true;
	i2c_dev->io_header = 0;
	if (next_msg == NULL)
		i2c_dev->io_header = I2C_HEADER_IE_ENABLE;

	if (end_state == MSG_END_CONTINUE)
		i2c_dev->io_header |= I2C_HEADER_CONTINUE_XFER;
	else if (end_state == MSG_END_REPEAT_START)
		i2c_dev->io_header |= I2C_HEADER_REPEAT_START;
	if (msg->flags & I2C_M_TEN) {
		i2c_dev->io_header |= msg->addr;
		i2c_dev->io_header |= I2C_HEADER_10BIT_ADDR;
	} else {
		i2c_dev->io_header |=
			msg->addr << I2C_HEADER_SLAVE_ADDR_SHIFT;
	}
	if (msg->flags & I2C_M_IGNORE_NAK)
		i2c_dev->io_header |= I2C_HEADER_CONT_ON_NAK;
	if (msg->flags & I2C_M_RD)
		i2c_dev->io_header |= I2C_HEADER_READ;
	if (i2c_dev->is_high_speed_enable) {
		i2c_dev->io_header |= I2C_HEADER_HIGHSPEED_MODE;
		i2c_dev->io_header |= ((i2c_dev->hs_master_code & 0x7)
				<<  I2C_HEADER_MASTER_ADDR_SHIFT);
	}
	packet_header[2] = i2c_dev->io_header;
}

static void tegra_i2c_config_fifo_trig(struct tegra_i2c_dev *i2c_dev,
		int len)
{
	u32 val;
	u8 maxburst = 0;
	struct dma_slave_config dma_sconfig;

	val = i2c_readl(i2c_dev, I2C_FIFO_CONTROL);
	if (len & 0xF) {
		val |= I2C_FIFO_CONTROL_TX_TRIG_1
			| I2C_FIFO_CONTROL_RX_TRIG_1;
		maxburst = 1;
	} else if (((len) >> 4) & 0x1) {
		val |= I2C_FIFO_CONTROL_TX_TRIG_4
			| I2C_FIFO_CONTROL_RX_TRIG_4;
		maxburst = 4;
	} else {
		val |= I2C_FIFO_CONTROL_TX_TRIG_8
			| I2C_FIFO_CONTROL_RX_TRIG_8;
		maxburst = 8;
	}
	i2c_writel(i2c_dev, val, I2C_FIFO_CONTROL);

	if (i2c_dev->curr_direction == DATA_DMA_DIR_TX) {
		dma_sconfig.dst_addr = i2c_dev->phys + I2C_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = maxburst;
		dmaengine_slave_config(i2c_dev->tx_dma_chan, &dma_sconfig);
	} else {
		dma_sconfig.src_addr = i2c_dev->phys + I2C_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.src_maxburst = maxburst;
		dmaengine_slave_config(i2c_dev->rx_dma_chan, &dma_sconfig);
	}
}

static int tegra_i2c_start_dma_based_xfer(struct tegra_i2c_dev *i2c_dev,
	struct i2c_msg *msg, enum msg_end_type end_state,
	struct i2c_msg *next_msg, enum msg_end_type next_msg_end_state)
{
	int ret = 0;
	u32 packet_header[3];
	u32 dma_xfer_len;
	u32 int_mask;
	unsigned long flags = 0;

	i2c_dev->is_curr_dma_xfer = true;

	/* setting msg_buf_remaining to zero as complete
	   xfer is done thru DMA*/
	i2c_dev->msg_buf_remaining = 0;

	/* Enable error interrupts */
	int_mask = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST
					| I2C_INT_TX_FIFO_OVERFLOW;
	tegra_i2c_unmask_irq(i2c_dev, int_mask);

	if (!(msg->flags & I2C_M_RD)) {
		i2c_dev->curr_direction = DATA_DMA_DIR_TX;
		dma_sync_single_for_cpu(i2c_dev->dev, i2c_dev->tx_dma_phys,
				i2c_dev->dma_buf_size, DMA_TO_DEVICE);
		/* Fill packet header */
		tegra_i2c_fill_packet_header(i2c_dev, msg, end_state, next_msg,
				next_msg_end_state, packet_header);

		dma_xfer_len = I2C_PACKET_HEADER_SIZE;
		memcpy(i2c_dev->tx_dma_buf, packet_header, (dma_xfer_len * 4));

		/* Copy data payload to dma buffer*/
		memcpy(i2c_dev->tx_dma_buf + dma_xfer_len, i2c_dev->msg_buf,
				msg->len);
		/* make the dma buffer to read by dma */
		dma_sync_single_for_device(i2c_dev->dev, i2c_dev->tx_dma_phys,
				i2c_dev->dma_buf_size, DMA_TO_DEVICE);
		dma_xfer_len += DIV_ROUND_UP(msg->len, 4);

		/* Round up final length for DMA xfer in bytes*/
		dma_xfer_len = DIV_ROUND_UP(dma_xfer_len * 4, 4) * 4;

		tegra_i2c_config_fifo_trig(i2c_dev, dma_xfer_len);

		/* Acquire the lock before posting the data to FIFO */
		raw_spin_lock_irqsave(&i2c_dev->fifo_lock, flags);

		ret = tegra_i2c_start_tx_dma(i2c_dev, dma_xfer_len);
		if (ret < 0) {
			dev_err(i2c_dev->dev,
				"Starting tx dma failed, err %d\n", ret);
			goto exit;
		}
	} else {
		i2c_dev->curr_direction = DATA_DMA_DIR_RX;
		/* Round up final length for DMA xfer */
		dma_xfer_len = DIV_ROUND_UP(msg->len, 4) * 4;
		tegra_i2c_config_fifo_trig(i2c_dev, dma_xfer_len);

		ret = tegra_i2c_start_rx_dma(i2c_dev, dma_xfer_len);
		if (ret < 0) {
			dev_err(i2c_dev->dev,
				"Starting rx dma failed, err %d\n", ret);
			return ret;
		}
		/* Fill packet header */
		tegra_i2c_fill_packet_header(i2c_dev, msg, end_state, next_msg,
				next_msg_end_state, packet_header);

		/* Acquire the lock before posting the data to FIFO */
		raw_spin_lock_irqsave(&i2c_dev->fifo_lock, flags);

		/* Transfer packet header through PIO */
		i2c_writel(i2c_dev, packet_header[0], I2C_TX_FIFO);
		i2c_writel(i2c_dev, packet_header[1], I2C_TX_FIFO);
		i2c_writel(i2c_dev, packet_header[2], I2C_TX_FIFO);
	}

	if (i2c_dev->is_dvc)
		dvc_i2c_unmask_irq(i2c_dev, DVC_CTRL_REG3_I2C_DONE_INTR_EN);

	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		int_mask |= I2C_INT_PACKET_XFER_COMPLETE;

	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		int_mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	if (!(i2c_dev->use_single_xfer_complete &&
			i2c_dev->chipdata->has_xfer_complete_interrupt))
		int_mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	tegra_i2c_unmask_irq(i2c_dev, int_mask);

exit:
	raw_spin_unlock_irqrestore(&i2c_dev->fifo_lock, flags);
	return ret;
}

static int tegra_i2c_start_pio_based_xfer(struct tegra_i2c_dev *i2c_dev,
	struct i2c_msg *msg, enum msg_end_type end_state,
	struct i2c_msg *next_msg, enum msg_end_type next_msg_end_state)
{

	u32 val;
	u32 int_mask;
	u32 packet_header[3];
	unsigned long flags = 0;

	i2c_dev->is_curr_dma_xfer = false;
	val = 7 << I2C_FIFO_CONTROL_TX_TRIG_SHIFT |
		0 << I2C_FIFO_CONTROL_RX_TRIG_SHIFT;
	i2c_writel(i2c_dev, val, I2C_FIFO_CONTROL);

	i2c_dev->msg_add = msg->addr;

	/* Enable error interrupts */
	int_mask = I2C_INT_NO_ACK | I2C_INT_ARBITRATION_LOST
					| I2C_INT_TX_FIFO_OVERFLOW;
	tegra_i2c_unmask_irq(i2c_dev, int_mask);

	/* Fill packet header */
	tegra_i2c_fill_packet_header(i2c_dev, msg, end_state, next_msg,
			next_msg_end_state, packet_header);

	/* Acquire the lock before posting the data to FIFO */
	raw_spin_lock_irqsave(&i2c_dev->fifo_lock, flags);

	i2c_writel(i2c_dev, packet_header[0], I2C_TX_FIFO);
	i2c_writel(i2c_dev, packet_header[1], I2C_TX_FIFO);
	i2c_writel(i2c_dev, packet_header[2], I2C_TX_FIFO);

	if (!(msg->flags & I2C_M_RD))
		tegra_i2c_fill_tx_fifo(i2c_dev);

	if (i2c_dev->is_dvc)
		dvc_i2c_unmask_irq(i2c_dev, DVC_CTRL_REG3_I2C_DONE_INTR_EN);

	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		int_mask |= I2C_INT_PACKET_XFER_COMPLETE;

	if (i2c_dev->chipdata->has_xfer_complete_interrupt)
		int_mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;


	if (msg->flags & I2C_M_RD)
		int_mask |= I2C_INT_RX_FIFO_DATA_REQ;
	else if (i2c_dev->msg_buf_remaining)
		int_mask |= I2C_INT_TX_FIFO_DATA_REQ;

	if (next_msg != NULL) {
		tegra_i2c_send_next_read_msg_pkt_header(i2c_dev, next_msg,
							next_msg_end_state);
		tegra_i2c_copy_next_to_current(i2c_dev);
		int_mask |= I2C_INT_RX_FIFO_DATA_REQ;
		i2c_dev->use_single_xfer_complete = false;
	}

	if (!(i2c_dev->use_single_xfer_complete &&
			i2c_dev->chipdata->has_xfer_complete_interrupt))
		int_mask |= I2C_INT_ALL_PACKETS_XFER_COMPLETE;

	tegra_i2c_unmask_irq(i2c_dev, int_mask);
	raw_spin_unlock_irqrestore(&i2c_dev->fifo_lock, flags);

	return 0;
}

static int tegra_i2c_pre_xfer_config(struct tegra_i2c_dev *i2c_dev,
		struct i2c_msg *msg)
{
	unsigned long timeout;
	u32 cnfg;

	tegra_i2c_flush_fifos(i2c_dev);

	i2c_dev->is_curr_dma_xfer = false;
	i2c_dev->msg_buf = msg->buf;
	i2c_dev->msg_buf_remaining = msg->len;
	i2c_dev->msg_err = I2C_ERR_NONE;
	i2c_dev->msg_read = (msg->flags & I2C_M_RD);
	reinit_completion(&i2c_dev->msg_complete);

	/* configuring below register to default as per TRM*/
	i2c_writel(i2c_dev, 0, I2C_MASTER_RESET_CONTROL);
	i2c_writel(i2c_dev, 0, I2C_TLOW_SEXT);
	i2c_writel(i2c_dev, 0, I2C_CMD_ADDR0);
	i2c_writel(i2c_dev, 0, I2C_CMD_ADDR1);
	i2c_writel(i2c_dev, 0, I2C_CMD_DATA1);
	i2c_writel(i2c_dev, 0, I2C_CMD_DATA2);
	i2c_writel(i2c_dev, 0, I2C_DEBUG_CONTROL);
	i2c_writel(i2c_dev, 0, I2C_INTERRUPT_SET_REGISTER);
	i2c_writel(i2c_dev, 0, I2C_FIFO_CONTROL);

	cnfg = I2C_CNFG_NEW_MASTER_FSM | I2C_CNFG_PACKET_MODE_EN;
	if (i2c_dev->chipdata->has_multi_master_en_bit)
		cnfg |= I2C_CNFG_MULTI_MASTER_MODE;

	if (!i2c_dev->is_high_speed_enable)
		cnfg |= (0x2 << I2C_CNFG_DEBOUNCE_CNT_SHIFT);

	i2c_writel(i2c_dev, cnfg, I2C_CNFG);

	if (i2c_dev->chipdata->has_config_load_reg) {
		i2c_writel(i2c_dev, I2C_MSTR_CONFIG_LOAD,
					I2C_CONFIG_LOAD);
		timeout = jiffies + msecs_to_jiffies(1000);
		while (i2c_readl(i2c_dev, I2C_CONFIG_LOAD) != 0) {
			if (time_after(jiffies, timeout)) {
				dev_warn(i2c_dev->dev,
					"timeout config_load");
				return -ETIMEDOUT;
			}
			udelay(2);
		}
	}
	i2c_writel(i2c_dev, 0, I2C_INT_MASK);

	return 0;
}

static int tegra_i2c_handle_xfer_error(struct tegra_i2c_dev *i2c_dev)
{
	int ret;

	/* Prints errors */
	if (i2c_dev->msg_err & I2C_ERR_UNKNOWN_INTERRUPT)
		dev_warn(i2c_dev->dev, "unknown interrupt Add 0x%02x\n",
				i2c_dev->msg_add);
	if (i2c_dev->msg_err & I2C_ERR_NO_ACK)
		dev_warn(i2c_dev->dev, "no acknowledge from address 0x%x\n",
				i2c_dev->msg_add);
	if (i2c_dev->msg_err & I2C_ERR_ARBITRATION_LOST)
		dev_warn(i2c_dev->dev, "arb lost in communicate to add 0x%x\n",
				i2c_dev->msg_add);
	if (i2c_dev->msg_err & I2C_INT_TX_FIFO_OVERFLOW)
		dev_warn(i2c_dev->dev, "Tx fifo overflow to add 0x%x\n",
				i2c_dev->msg_add);
	if (i2c_dev->msg_err & I2C_ERR_UNEXPECTED_STATUS)
		dev_warn(i2c_dev->dev, "unexpected status to add 0x%x\n",
				i2c_dev->msg_add);

	if ((i2c_dev->chipdata->timeout_irq_occurs_before_bus_inactive) &&
		(i2c_dev->msg_err == I2C_ERR_NO_ACK)) {
		/*
		* In NACK error condition resetting of I2C controller happens
		* before STOP condition is properly completed by I2C controller,
		* so wait for 2 clock cycle to complete STOP condition.
		*/
		udelay(DIV_ROUND_UP(2 * 1000000, i2c_dev->bus_clk_rate));
	}

	/*
	 * NACK interrupt is generated before the I2C controller generates the
	 * STOP condition on the bus. So wait for 2 clock periods before resetting
	 * the controller so that STOP condition has been delivered properly.
	 */
	if (i2c_dev->msg_err == I2C_ERR_NO_ACK)
		udelay(DIV_ROUND_UP(2 * 1000000, i2c_dev->bus_clk_rate));

	ret = tegra_i2c_init(i2c_dev);
	if (ret) {
		WARN_ON(1);
		return ret;
	}

	/* Arbitration Lost occurs, Start recovery */
	if (i2c_dev->msg_err == I2C_ERR_ARBITRATION_LOST) {
		if (!i2c_dev->is_multimaster_mode) {
			ret = tegra_i2c_issue_bus_clear(i2c_dev);
			if (ret)
				return ret;
		}
		return -EAGAIN;
	}

	if (i2c_dev->msg_err == I2C_ERR_NO_ACK)
		return -EREMOTEIO;

	if (i2c_dev->msg_err & I2C_ERR_UNEXPECTED_STATUS)
		return -EAGAIN;

	return -EIO;
}


static int tegra_i2c_xfer_msg(struct tegra_i2c_dev *i2c_dev,
	struct i2c_msg *msg, enum msg_end_type end_state,
	struct i2c_msg *next_msg, enum msg_end_type next_msg_end_state)
{
	u32 int_mask = 0;
	int ret;

	if (msg->len == 0)
		return -EINVAL;

	if (!i2c_dev->disable_dma_mode && (msg->len > I2C_PIO_MODE_MAX_LEN)
			&& !(i2c_dev->tx_dma_chan && i2c_dev->rx_dma_chan)) {
		ret = tegra_i2c_init_dma_param(i2c_dev, true);
		if (ret && (ret != -EPROBE_DEFER) && (ret != -ENODEV))
			return ret;
		ret = tegra_i2c_init_dma_param(i2c_dev, false);
		if (ret && (ret != -EPROBE_DEFER) && (ret != -ENODEV))
			return ret;
	}

	ret = tegra_i2c_pre_xfer_config(i2c_dev, msg);
	if (ret)
		return ret;

	if ((msg->len > I2C_PIO_MODE_MAX_LEN) && i2c_dev->tx_dma_chan
				&& i2c_dev->rx_dma_chan)
		ret = tegra_i2c_start_dma_based_xfer(i2c_dev, msg, end_state,
				next_msg, next_msg_end_state);
	else
		ret = tegra_i2c_start_pio_based_xfer(i2c_dev, msg, end_state,
				next_msg, next_msg_end_state);

	if (ret)
		return ret;

	dev_dbg(i2c_dev->dev, "unmasked irq: %02x\n",
		i2c_readl(i2c_dev, I2C_INT_MASK));

	if (i2c_dev->is_curr_dma_xfer) {
		if (i2c_dev->curr_direction == DATA_DMA_DIR_TX) {
			ret = wait_for_completion_timeout(
					&i2c_dev->tx_dma_complete,
					TEGRA_I2C_TIMEOUT);
			if (ret == 0) {
				dev_err(i2c_dev->dev, "tx dma timeout\n");
				dmaengine_terminate_all(i2c_dev->tx_dma_chan);
				goto end_xfer;
			}
		} else if (i2c_dev->curr_direction == DATA_DMA_DIR_RX) {
			ret = wait_for_completion_timeout(
					&i2c_dev->rx_dma_complete,
					TEGRA_I2C_TIMEOUT);
			if (ret == 0) {
				dev_err(i2c_dev->dev, "rx dma timeout\n");
				dmaengine_terminate_all(i2c_dev->rx_dma_chan);
				goto end_xfer;
			}
		}
	}

	ret = wait_for_completion_timeout(&i2c_dev->msg_complete,
					TEGRA_I2C_TIMEOUT);
	if (ret == 0) {
		tegra_i2c_reg_dump(i2c_dev, msg, next_msg);
		if (i2c_dev->is_curr_dma_xfer) {
			if (i2c_dev->curr_direction == DATA_DMA_DIR_TX)
				dmaengine_terminate_all(i2c_dev->tx_dma_chan);
			else if (i2c_dev->curr_direction == DATA_DMA_DIR_RX)
				dmaengine_terminate_all(i2c_dev->rx_dma_chan);
		}
	}

end_xfer:
	int_mask = i2c_readl(i2c_dev, I2C_INT_MASK);
	tegra_i2c_mask_irq(i2c_dev, int_mask);

	if (i2c_dev->is_dvc)
		dvc_i2c_mask_irq(i2c_dev, DVC_CTRL_REG3_I2C_DONE_INTR_EN);

	if (likely(i2c_dev->msg_err == I2C_ERR_NONE) &&
			end_state != MSG_END_CONTINUE)
		tegra_i2c_disable_packet_mode(i2c_dev);

	if (ret == 0) {
		dev_err(i2c_dev->dev,
			"i2c transfer timed out, addr 0x%04x, data 0x%02x\n",
			msg->addr, msg->buf[0]);

		ret = tegra_i2c_init(i2c_dev);
		if (!ret)
			ret = -ETIMEDOUT;
		else
			WARN_ON(1);
		return ret;
	}

	dev_dbg(i2c_dev->dev, "transfer complete: %d %d %d\n",
		ret, completion_done(&i2c_dev->msg_complete), i2c_dev->msg_err);

	if (likely(i2c_dev->msg_err == I2C_ERR_NONE))
		return 0;

	return tegra_i2c_handle_xfer_error(i2c_dev);
}

static int tegra_i2c_split_i2c_msg_xfer(struct tegra_i2c_dev *i2c_dev,
		struct i2c_msg *msg, enum msg_end_type end_type,
		enum msg_end_type next_msg_end_type)
{
	int size, len, ret;
	struct i2c_msg temp_msg;
	u8 *buf = msg->buf;
	enum msg_end_type temp_end_type;

	size = msg->len;
	temp_msg.flags = msg->flags;
	temp_msg.addr = msg->addr;
	temp_end_type = end_type;
	do {
		temp_msg.buf = buf;
		len = min(size, I2C_MAX_TRANSFER_LEN);
		temp_msg.len = len;
		size -= len;
		if ((len == I2C_MAX_TRANSFER_LEN) && size)
			end_type = MSG_END_CONTINUE;
		else
			end_type = temp_end_type;
		ret = tegra_i2c_xfer_msg(i2c_dev, &temp_msg, end_type,
				NULL, next_msg_end_type);
		if (ret)
			return ret;
		buf += len;
	} while (size != 0);

	return ret;
}

static int tegra_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
	int num)
{
	struct tegra_i2c_dev *i2c_dev = i2c_get_adapdata(adap);
	int i;
	int ret = 0;
	u16 slave_addr = I2C_PROFILE_ALL_ADDR;

	BUG_ON(!rt_mutex_is_locked(&(adap->bus_lock)));
	if (i2c_dev->is_suspended)
		return -EBUSY;

	if ((i2c_dev->is_shutdown || adap->atomic_xfer_only)
		&& i2c_dev->bit_banging_xfer_after_shutdown)
		return tegra_i2c_gpio_xfer(adap, msgs, num);

	if (adap->atomic_xfer_only)
		return -EBUSY;

	if (adap->bus_clk_rate != i2c_dev->bus_clk_rate) {
		i2c_dev->bus_clk_rate = adap->bus_clk_rate;
		ret = tegra_i2c_set_clk_rate(i2c_dev);
		if (ret < 0)
			return ret;
	}

	i2c_dev->msgs = msgs;
	i2c_dev->msgs_num = num;

	if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR)) {
		slave_addr = msgs[0].addr;
		++i2c_dev->stat.xfer_requests;
		profile_start(i2c_dev, PROFL_TOTAL_XFER_TIME);
	}

	if (is_profiling_enabled(i2c_dev, msgs[0].addr)) {
		slave_addr = msgs[0].addr;
		++i2c_dev->slave_stat.xfer_requests;
		profile_start(i2c_dev, PROFL_SLAVE_XFER_TIME);
	}

	pm_runtime_get_sync(&adap->dev);
	ret = tegra_i2c_clock_enable(i2c_dev);
	if (ret < 0) {
		dev_err(i2c_dev->dev, "Clock enable failed %d\n", ret);
		pm_runtime_put(&adap->dev);
		return ret;
	}
	i2c_dev->transfer_in_progress = true;

	for (i = 0; i < num; i++) {
		enum msg_end_type end_type = MSG_END_STOP;
		enum msg_end_type next_msg_end_type = MSG_END_STOP;
		bool rd_wr = 0;

		if (i < (num - 1)) {
			if (msgs[i + 1].flags & I2C_M_NOSTART)
				end_type = MSG_END_CONTINUE;
			else
				end_type = MSG_END_REPEAT_START;
			if (i < num - 2) {
				if (msgs[i + 2].flags & I2C_M_NOSTART)
					next_msg_end_type = MSG_END_CONTINUE;
				else
					next_msg_end_type = MSG_END_REPEAT_START;
			}
			if ((!(msgs[i].flags & I2C_M_RD)) && (msgs[i].len <= 8)
				&& (msgs[i+1].flags & I2C_M_RD)
				&& (msgs[i+1].len <= I2C_MAX_TRANSFER_LEN)
				&& (next_msg_end_type != MSG_END_CONTINUE)
				&& (end_type == MSG_END_REPEAT_START)) {

				if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR)) {
					++i2c_dev->stat.reads;
					++i2c_dev->stat.writes;
					i2c_dev->stat.write_bytes += msgs[i].len;
					i2c_dev->stat.read_bytes += msgs[i+1].len;
					i2c_dev->stat.total_bytes += msgs[i].len;
					i2c_dev->stat.total_bytes += msgs[i+1].len;
				}
				if (is_profiling_enabled(i2c_dev, slave_addr)) {
					++i2c_dev->slave_stat.reads;
					++i2c_dev->slave_stat.writes;
					i2c_dev->slave_stat.read_bytes += msgs[i+1].len;
					i2c_dev->slave_stat.write_bytes += msgs[i].len;
					i2c_dev->slave_stat.total_bytes += msgs[i].len;
					i2c_dev->slave_stat.total_bytes += msgs[i+1].len;
					profile_start(i2c_dev, PROFL_SLAVE_READ_TIME);
					rd_wr = 1;
				}

				ret = tegra_i2c_xfer_msg(i2c_dev, &msgs[i], end_type, &msgs[i+1], next_msg_end_type);
				if (ret)
					break;
				i++;
			} else {
				if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR)) {
					i2c_dev->stat.total_bytes += msgs[i].len;
					if (msgs[i].flags & I2C_M_RD) {
						i2c_dev->stat.read_bytes += msgs[i].len;
						++i2c_dev->stat.reads;
					} else {
						++i2c_dev->stat.writes;
						i2c_dev->stat.write_bytes += msgs[i].len;
					}
				}
				if (is_profiling_enabled(i2c_dev, slave_addr)) {
					i2c_dev->slave_stat.total_bytes += msgs[i].len;
					if (msgs[i].flags & I2C_M_RD) {
						++i2c_dev->slave_stat.reads;
						i2c_dev->slave_stat.read_bytes += msgs[i+1].len;
						profile_start(i2c_dev, PROFL_SLAVE_READ_TIME);
						rd_wr = 1;
					} else {
						++i2c_dev->slave_stat.writes;
						i2c_dev->slave_stat.write_bytes += msgs[i].len;
						profile_start(i2c_dev, PROFL_SLAVE_WRITE_TIME);
						rd_wr = 0;
					}
				}

				if (msgs[i].len > I2C_MAX_TRANSFER_LEN) {
					ret = tegra_i2c_split_i2c_msg_xfer(i2c_dev, &msgs[i],
							end_type, next_msg_end_type);
					if (ret)
						break;
				} else {
					ret = tegra_i2c_xfer_msg(i2c_dev, &msgs[i], end_type,
							NULL, next_msg_end_type);
					if (ret)
						break;
				}
			}
		} else {
			if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR)) {
				i2c_dev->stat.total_bytes += msgs[i].len;
				if (msgs[i].flags & I2C_M_RD) {
					i2c_dev->stat.read_bytes += msgs[i].len;
					++i2c_dev->stat.reads;
				} else {
					i2c_dev->stat.write_bytes += msgs[i].len;
					++i2c_dev->stat.writes;
				}
			}
			if (is_profiling_enabled(i2c_dev, slave_addr)) {
				i2c_dev->slave_stat.total_bytes += msgs[i].len;
				if (msgs[i].flags & I2C_M_RD) {
					i2c_dev->slave_stat.read_bytes += msgs[i].len;
					++i2c_dev->slave_stat.reads;
					profile_start(i2c_dev, PROFL_SLAVE_READ_TIME);
					rd_wr = 1;
				} else {
					i2c_dev->slave_stat.write_bytes += msgs[i].len;
					++i2c_dev->slave_stat.writes;
					profile_start(i2c_dev, PROFL_SLAVE_WRITE_TIME);
					rd_wr = 0;
				}
			}

			if (msgs[i].len > I2C_MAX_TRANSFER_LEN) {
				ret = tegra_i2c_split_i2c_msg_xfer(i2c_dev,
						&msgs[i], end_type,
						next_msg_end_type);
				if (ret)
					break;
			} else {
				ret = tegra_i2c_xfer_msg(i2c_dev, &msgs[i], end_type, NULL, next_msg_end_type);
				if (ret)
					break;
			}
		}

		if (is_profiling_enabled(i2c_dev, slave_addr)) {
			if (rd_wr)
				profile_stop(i2c_dev, PROFL_SLAVE_READ_TIME);
			else
				profile_stop(i2c_dev, PROFL_SLAVE_WRITE_TIME);
		}
	}

	i2c_dev->transfer_in_progress = false;
	tegra_i2c_clock_disable(i2c_dev);
	pm_runtime_put(&adap->dev);

	i2c_dev->msgs = NULL;
	i2c_dev->msgs_num = 0;
	if (is_profiling_enabled(i2c_dev, I2C_PROFILE_ALL_ADDR))
		profile_stop(i2c_dev, PROFL_TOTAL_XFER_TIME);
	if (is_profiling_enabled(i2c_dev, slave_addr))
		profile_stop(i2c_dev, PROFL_SLAVE_XFER_TIME);

	return ret ?: i;
}

#ifdef CONFIG_DEBUG_FS
static int tegra_i2c_stats_show(struct seq_file *s, void *unused)
{
	struct tegra_i2c_dev *i2c_dev = s->private;

	seq_puts(s, "\n======= I2C STATS ==============\n");
	seq_printf(s, "Xfer requests: %u\n", i2c_dev->stat.xfer_requests);
	seq_printf(s, "Total bytes: %u\n", i2c_dev->stat.total_bytes);
	seq_printf(s, "Read requests: %u\n", i2c_dev->stat.reads);
	seq_printf(s, "Read bytes: %u\n", i2c_dev->stat.read_bytes);
	seq_printf(s, "Write requests: %u\n", i2c_dev->stat.writes);
	seq_printf(s, "Write bytes: %u\n", i2c_dev->stat.write_bytes);
	seq_printf(s, "Total time of capture (in usec): %llu\n",
			i2c_dev->pfd[PROFL_TOTAL_CONT_TIME].total_time/1000);
	seq_printf(s, "Total Xfer time (in usec): %llu\n",
			i2c_dev->pfd[PROFL_TOTAL_XFER_TIME].total_time/1000);
	seq_printf(s, "Idle ime (in usec): %llu\n",
			i2c_dev->pfd[PROFL_TOTAL_CONT_TIME].total_time/1000
			- i2c_dev->pfd[PROFL_TOTAL_XFER_TIME].total_time/1000);

	seq_printf(s, "\n===== PROFILE ADDR :- 0x%x =====\n",
			i2c_dev->profile_addr);
	seq_printf(s, "Xfer requests: %u\n", i2c_dev->slave_stat.xfer_requests);
	seq_printf(s, "Total bytes: %u\n", i2c_dev->slave_stat.total_bytes);
	seq_printf(s, "Read requests: %u\n", i2c_dev->slave_stat.reads);
	seq_printf(s, "Read bytes: %u\n", i2c_dev->slave_stat.read_bytes);
	seq_printf(s, "Write requests: %u\n", i2c_dev->slave_stat.writes);
	seq_printf(s, "Write bytes: %u\n", i2c_dev->slave_stat.write_bytes);
	seq_printf(s, "Slave Xfer time (in usec): %llu\n",
			i2c_dev->pfd[PROFL_SLAVE_XFER_TIME].total_time/1000);
	seq_printf(s, "Read time (in use): %llu\n",
			i2c_dev->pfd[PROFL_SLAVE_READ_TIME].total_time/1000);
	seq_printf(s, "Write time (in usec): %llu\n",
			i2c_dev->pfd[PROFL_SLAVE_WRITE_TIME].total_time/1000);

	return 0;
}

static int tegra_i2c_stats_open(struct inode *inode, struct file *f)
{
	return single_open(f, tegra_i2c_stats_show, inode->i_private);
}

static const struct file_operations tegra_i2c_stats_fops = {
	.owner = THIS_MODULE,
	.open = tegra_i2c_stats_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static int tegra_i2c_profiling_enabled_get(void *data, u64 *val)
{
	struct tegra_i2c_dev *i2c_dev = data;

	*val = i2c_dev->enable_profiling;

	return 0;
}

static int tegra_i2c_profiling_enabled_set(void *data, u64 val)
{
	struct tegra_i2c_dev *i2c_dev = data;

	if (val) {
		memset(&i2c_dev->stat, 0, sizeof(i2c_dev->stat));
		memset(&i2c_dev->slave_stat, 0, sizeof(i2c_dev->slave_stat));
		memset(&i2c_dev->pfd, 0 , sizeof(i2c_dev->pfd));
		profile_start(i2c_dev, PROFL_TOTAL_CONT_TIME);
		i2c_dev->enable_profiling = val;
	} else {
		i2c_dev->enable_profiling = val;
		profile_stop(i2c_dev, PROFL_TOTAL_CONT_TIME);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(tegra_i2c_profiling_enabled_fops,
			tegra_i2c_profiling_enabled_get,
			tegra_i2c_profiling_enabled_set,
			"%llu\n");

static void tegra_i2c_debugfs_init(struct tegra_i2c_dev *i2c_dev)
{
	struct dentry *retval;

	i2c_dev->debugfs = debugfs_create_dir(dev_name(i2c_dev->dev), NULL);
	if (IS_ERR_OR_NULL(i2c_dev->debugfs))
		goto clean;

	retval = debugfs_create_file("stats_dump", S_IRUGO | S_IWUSR,
				     i2c_dev->debugfs, (void *)i2c_dev,
				     &tegra_i2c_stats_fops);
	if (IS_ERR_OR_NULL(retval))
		goto clean;

	retval = debugfs_create_file("enable_profiling", S_IRUGO | S_IWUSR,
				     i2c_dev->debugfs, (void *)i2c_dev,
				     &tegra_i2c_profiling_enabled_fops);
	if (IS_ERR_OR_NULL(retval))
		goto clean;

	debugfs_create_u16("profile_addr", 0644, i2c_dev->debugfs,
			&i2c_dev->profile_addr);
	return;
clean:
	dev_warn(i2c_dev->dev, "Failed to create debugfs!\n");
	debugfs_remove_recursive(i2c_dev->debugfs);
}

static void tegra_i2c_debugfs_deinit(struct tegra_i2c_dev *i2c_dev)
{
	debugfs_remove_recursive(i2c_dev->debugfs);
}
#else
static void tegra_i2c_debugfs_init(struct tegra_i2c_dev *i2c_dev) {}
static void tegra_i2c_debugfs_deinit(struct tegra_i2c_dev *i2c_dev) {}
#endif

static u32 tegra_i2c_func(struct i2c_adapter *adap)
{
	struct tegra_i2c_dev *i2c_dev = i2c_get_adapdata(adap);
	u32 ret = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR |
				I2C_FUNC_PROTOCOL_MANGLING;

	if (i2c_dev->chipdata->has_continue_xfer_support)
		ret |= I2C_FUNC_NOSTART;
	return ret;
}

static const struct i2c_algorithm tegra_i2c_algo = {
	.master_xfer	= tegra_i2c_xfer,
	.functionality	= tegra_i2c_func,
};

static int tegra_i2c_pm_notifier(struct notifier_block *nb,
	unsigned long event, void *data);

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

	pdata->needs_cl_dvfs_clock = of_property_read_bool(np,
					"nvidia,require-cldvfs-clock");

	pdata->bit_banging_xfer_after_shutdown = of_property_read_bool(np,
					"nvidia,bit-banging-xfer-after-shutdown");

	pdata->scl_gpio = of_get_named_gpio(np, "scl-gpio", 0);
	pdata->sda_gpio = of_get_named_gpio(np, "sda-gpio", 0);
	pdata->is_dvc = of_device_is_compatible(np, "nvidia,tegra20-i2c-dvc");

	pdata->is_multimaster_mode = of_property_read_bool(np,
			"nvidia,multimaster-mode");

	if (pdata->is_multimaster_mode)
		pdata->is_clkon_always = true;

	pdata->disable_dma_mode = of_property_read_bool(np,
			"nvidia,disable-dma-mode");

	/* Default configuration for device tree initiated driver */
	pdata->slave_addr = 0xFC;

	return pdata;
}

static struct tegra_i2c_chipdata tegra20_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = true,
	.has_xfer_complete_interrupt = false,
	.has_continue_xfer_support = false,
	.has_hw_arb_support = false,
	.has_fast_clock = true,
	.has_clk_divisor_std_fast_mode = false,
	.clk_divisor_std_fast_mode = 0,
	.clk_divisor_hs_mode = 3,
	.clk_multiplier_hs_mode = 12,
	.has_config_load_reg = false,
	.has_multi_master_en_bit = false,
	.has_sw_reset_reg = false,
	.has_clk_override_reg = false,
	.has_reg_write_buffering = true,
};

static struct tegra_i2c_chipdata tegra30_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = true,
	.has_xfer_complete_interrupt = false,
	.has_continue_xfer_support = true,
	.has_hw_arb_support = false,
	.has_fast_clock = true,
	.has_clk_divisor_std_fast_mode = false,
	.clk_divisor_std_fast_mode = 0,
	.clk_divisor_hs_mode = 3,
	.clk_multiplier_hs_mode = 12,
	.has_config_load_reg = false,
	.has_multi_master_en_bit = false,
	.has_sw_reset_reg = false,
	.has_clk_override_reg = false,
	.has_reg_write_buffering = true,
};

static struct tegra_i2c_chipdata tegra114_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = false,
	.has_xfer_complete_interrupt = true,
	.has_continue_xfer_support = true,
	.has_hw_arb_support = true,
	.has_fast_clock = false,
	.has_clk_divisor_std_fast_mode = true,
	.clk_divisor_std_fast_mode = 0x19,
	.clk_divisor_fast_plus_mode = 0x10,
	.clk_divisor_hs_mode = 1,
	.clk_multiplier_hs_mode = 3,
	.has_config_load_reg = false,
	.has_multi_master_en_bit = false,
	.has_sw_reset_reg = false,
	.has_clk_override_reg = false,
	.has_reg_write_buffering = true,
};

static struct tegra_i2c_chipdata tegra148_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = false,
	.has_xfer_complete_interrupt = true,
	.has_continue_xfer_support = true,
	.has_hw_arb_support = true,
	.has_fast_clock = false,
	.has_clk_divisor_std_fast_mode = true,
	.clk_divisor_std_fast_mode = 0x19,
	.clk_divisor_fast_plus_mode = 0x19,
	.clk_divisor_hs_mode = 2,
	.clk_multiplier_hs_mode = 13,
	.has_config_load_reg = true,
	.has_multi_master_en_bit = false,
	.has_sw_reset_reg = false,
	.has_clk_override_reg = true,
	.has_reg_write_buffering = true,
};

static struct tegra_i2c_chipdata tegra124_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = false,
	.has_xfer_complete_interrupt = true,
	.has_continue_xfer_support = true,
	.has_hw_arb_support = true,
	.has_fast_clock = false,
	.has_clk_divisor_std_fast_mode = true,
	.clk_divisor_std_fast_mode = 0x19,
	.clk_divisor_fast_plus_mode = 0x10,
	.clk_divisor_hs_mode = 2,
	.clk_multiplier_hs_mode = 13,
	.has_config_load_reg = true,
	.has_multi_master_en_bit = false,
	.has_sw_reset_reg = false,
	.has_clk_override_reg = true,
	.has_reg_write_buffering = true,
};

static struct tegra_i2c_chipdata tegra210_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = false,
	.has_xfer_complete_interrupt = true,
	.has_continue_xfer_support = true,
	.has_hw_arb_support = true,
	.has_fast_clock = false,
	.has_clk_divisor_std_fast_mode = true,
	.clk_divisor_std_fast_mode = 0x19,
	.clk_divisor_fast_plus_mode = 0x10,
	.clk_divisor_hs_mode = 2,
	.clk_multiplier_hs_mode = 13,
	.has_config_load_reg = true,
	.has_multi_master_en_bit = true,
	.has_sw_reset_reg = false,
	.has_clk_override_reg = true,
	.has_reg_write_buffering = true,
};

static struct tegra_i2c_chipdata tegra186_i2c_chipdata = {
	.timeout_irq_occurs_before_bus_inactive = false,
	.has_xfer_complete_interrupt = true,
	.has_continue_xfer_support = true,
	.has_hw_arb_support = true,
	.has_fast_clock = false,
	.has_clk_divisor_std_fast_mode = true,
	.clk_divisor_std_fast_mode = 0x19,
	.clk_divisor_fast_plus_mode = 0x10,
	.clk_divisor_hs_mode = 2,
	.clk_multiplier_hs_mode = 13,
	.has_config_load_reg = true,
	.has_multi_master_en_bit = true,
	.has_sw_reset_reg = true,
	.has_clk_override_reg = true,
	.has_reg_write_buffering = false,
};

/* Match table for of_platform binding */
static const struct of_device_id tegra_i2c_of_match[] = {
	{ .compatible = "nvidia,tegra186-i2c",
					.data = &tegra186_i2c_chipdata, },
	{ .compatible = "nvidia,tegra210-i2c",
					.data = &tegra210_i2c_chipdata, },
	{ .compatible = "nvidia,tegra124-i2c", .data = &tegra124_i2c_chipdata, },
	{ .compatible = "nvidia,tegra148-i2c", .data = &tegra148_i2c_chipdata, },
	{ .compatible = "nvidia,tegra114-i2c", .data = &tegra114_i2c_chipdata, },
	{ .compatible = "nvidia,tegra30-i2c", .data = &tegra30_i2c_chipdata, },
	{ .compatible = "nvidia,tegra20-i2c", .data = &tegra20_i2c_chipdata, },
	{ .compatible = "nvidia,tegra20-i2c-dvc", .data = &tegra20_i2c_chipdata, },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_i2c_of_match);

static struct platform_device_id tegra_i2c_devtype[] = {
	{
		.name = "tegra-i2c",
		.driver_data = (unsigned long)&tegra30_i2c_chipdata,
	},
	{
		.name = "tegra20-i2c",
		.driver_data = (unsigned long)&tegra20_i2c_chipdata,
	},
	{
		.name = "tegra30-i2c",
		.driver_data = (unsigned long)&tegra30_i2c_chipdata,
	},
	{
		.name = "tegra11-i2c",
		.driver_data = (unsigned long)&tegra114_i2c_chipdata,
	},
	{
		.name = "tegra14-i2c",
		.driver_data = (unsigned long)&tegra148_i2c_chipdata,
	},
	{
		.name = "tegra12-i2c",
		.driver_data = (unsigned long)&tegra124_i2c_chipdata,
	},
	{
		.name = "tegra21-i2c",
		.driver_data = (unsigned long)&tegra210_i2c_chipdata,
	},
	{
		.name = "tegra18-i2c",
		.driver_data = (unsigned long)&tegra186_i2c_chipdata,
	},
};

static int tegra_i2c_probe(struct platform_device *pdev)
{
	struct tegra_i2c_dev *i2c_dev;
	struct tegra_i2c_platform_data *pdata = pdev->dev.platform_data;
	struct resource *res;
	struct clk *parent_clk;
	struct clk *div_clk;
	struct clk *fast_clk = NULL;
	struct clk *dvfs_ref_clk = NULL;
	struct clk *dvfs_soc_clk = NULL;
	void __iomem *base;
	phys_addr_t phys;
	int irq;
	int ret = 0;
	const struct tegra_i2c_chipdata *chip_data = NULL;
	const struct of_device_id *match;
	int bus_num = -1;
	struct pinctrl *pin;
	struct pinctrl_state *s;
	char prod_name[15];

	if (pdev->dev.of_node) {
		match = of_match_device(of_match_ptr(tegra_i2c_of_match), &pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "Device Not matching\n");
			return -ENODEV;
		}
		chip_data = match->data;
		if (!pdata)
			pdata = parse_i2c_tegra_dt(pdev);
	} else {
		chip_data = (struct tegra_i2c_chipdata *)pdev->id_entry->driver_data;
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
	phys = res->start;
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
	if (!i2c_dev)
		return -ENOMEM;

	i2c_dev->chipdata = chip_data;
	i2c_dev->needs_cl_dvfs_clock = pdata->needs_cl_dvfs_clock;

	i2c_dev->rstc = devm_reset_control_get(&pdev->dev, "i2c");
	if (IS_ERR(i2c_dev->rstc)) {
		ret = PTR_ERR(i2c_dev->rstc);
		dev_err(&pdev->dev, "Reset control is not foundi: %d\n", ret);
		return ret;
	}

	div_clk = devm_clk_get(&pdev->dev, "div-clk");
	if (IS_ERR(div_clk)) {
		dev_err(&pdev->dev, "missing controller clock");
		return PTR_ERR(div_clk);
	}

	parent_clk = devm_clk_get(&pdev->dev, "parent");
	if (IS_ERR(parent_clk))
		dev_err(&pdev->dev, "Unable to get parent_clk err:%ld\n",
				PTR_ERR(parent_clk));
	else {
		ret = clk_set_parent(div_clk, parent_clk);
		if (ret < 0)
			dev_warn(&pdev->dev,
				"Couldn't set the parent clock : %d\n", ret);
	}

	if (i2c_dev->chipdata->has_fast_clock) {
		fast_clk = devm_clk_get(&pdev->dev, "fast-clk");
		if (IS_ERR(fast_clk)) {
			dev_err(&pdev->dev, "missing bus clock");
			return PTR_ERR(fast_clk);
		}
	}

	if (i2c_dev->needs_cl_dvfs_clock) {
		dvfs_ref_clk = devm_clk_get(&pdev->dev, "cl_dvfs_ref");
		if (IS_ERR(dvfs_ref_clk)) {
			dev_err(&pdev->dev, "missing cl_dvfs_ref clock");
			return PTR_ERR(dvfs_ref_clk);
		}
		i2c_dev->dvfs_ref_clk = dvfs_ref_clk;
		dvfs_soc_clk = devm_clk_get(&pdev->dev, "cl_dvfs_soc");
		if (IS_ERR(dvfs_soc_clk)) {
			dev_err(&pdev->dev, "missing cl_dvfs_soc clock");
			return PTR_ERR(dvfs_soc_clk);
		}
		i2c_dev->dvfs_soc_clk = dvfs_soc_clk;
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
	i2c_dev->phys = phys;
	i2c_dev->base = base;
	i2c_dev->div_clk = div_clk;
	if (i2c_dev->chipdata->has_fast_clock)
		i2c_dev->fast_clk = fast_clk;
	i2c_dev->irq = irq;
	i2c_dev->dev = &pdev->dev;
	i2c_dev->is_clkon_always = pdata->is_clkon_always;
	i2c_dev->is_multimaster_mode = pdata->is_multimaster_mode;
	i2c_dev->bus_clk_rate = pdata->bus_clk_rate ? pdata->bus_clk_rate: 100000;
	i2c_dev->is_high_speed_enable = pdata->is_high_speed_enable;

	i2c_dev->clk_divisor_non_hs_mode =
		i2c_dev->chipdata->clk_divisor_std_fast_mode;

	if (i2c_dev->bus_clk_rate == I2C_FAST_MODE_PLUS)
		i2c_dev->clk_divisor_non_hs_mode =
			i2c_dev->chipdata->clk_divisor_fast_plus_mode;

	i2c_dev->clk_divisor_hs_mode =
		i2c_dev->chipdata->clk_divisor_hs_mode;

	i2c_dev->msgs = NULL;
	i2c_dev->msgs_num = 0;
	i2c_dev->is_dvc = pdata->is_dvc;
	i2c_dev->slave_addr = pdata->slave_addr;
	i2c_dev->hs_master_code = pdata->hs_master_code;
	i2c_dev->bit_banging_xfer_after_shutdown =
			pdata->bit_banging_xfer_after_shutdown;
	i2c_dev->dma_buf_size = I2C_DMA_MAX_BUF_LEN;
	i2c_dev->disable_dma_mode = pdata->disable_dma_mode;
	init_completion(&i2c_dev->msg_complete);
	init_completion(&i2c_dev->tx_dma_complete);
	init_completion(&i2c_dev->rx_dma_complete);

	raw_spin_lock_init(&i2c_dev->fifo_lock);

	i2c_dev->prod_list = tegra_prod_get(&pdev->dev, NULL);
	if (IS_ERR_OR_NULL(i2c_dev->prod_list)) {
		dev_dbg(&pdev->dev, "Prod-setting not available\n");
		i2c_dev->prod_list = NULL;
	}

	platform_set_drvdata(pdev, i2c_dev);

	if (i2c_dev->is_clkon_always)
		tegra_i2c_clock_enable(i2c_dev);

	if (!i2c_dev->disable_dma_mode) {
		ret = tegra_i2c_init_dma_param(i2c_dev, true);
		if (ret && (ret != -EPROBE_DEFER) && (ret != -ENODEV))
			goto err;
		ret = tegra_i2c_init_dma_param(i2c_dev, false);
		if (ret && (ret != -EPROBE_DEFER) && (ret != -ENODEV))
			goto err;
	}

	ret = tegra_i2c_init(i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize i2c controller");
		goto err;
	}

	ret = devm_request_irq(&pdev->dev, i2c_dev->irq,
			tegra_i2c_isr, IRQF_NO_SUSPEND,
			dev_name(&pdev->dev), i2c_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq %i\n", i2c_dev->irq);
		goto err;
	}

	pm_runtime_enable(&pdev->dev);

	i2c_dev->scl_gpio = pdata->scl_gpio;
	i2c_dev->sda_gpio = pdata->sda_gpio;
	i2c_set_adapdata(&i2c_dev->adapter, i2c_dev);
	i2c_dev->adapter.owner = THIS_MODULE;
	i2c_dev->adapter.class = I2C_CLASS_DEPRECATED;
	strlcpy(i2c_dev->adapter.name, "Tegra I2C adapter",
		sizeof(i2c_dev->adapter.name));
	i2c_dev->adapter.bus_clk_rate = i2c_dev->bus_clk_rate;
	i2c_dev->adapter.algo = &tegra_i2c_algo;
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
		goto err;
	}
	i2c_dev->cont_id = i2c_dev->adapter.nr & PACKET_HEADER0_CONT_ID_MASK;
	if (pdata->is_high_speed_enable) {
		sprintf(prod_name, "i2c%d_hs_prod", i2c_dev->cont_id);
		ret = tegra_pinctrl_config_prod(&pdev->dev, prod_name);
		if (ret < 0)
			dev_warn(&pdev->dev, "Failed to set %s setting\n",
					prod_name);
	}

	i2c_dev->pm_nb.notifier_call = tegra_i2c_pm_notifier;

	tegra_register_pm_notifier(&i2c_dev->pm_nb);

	pm_runtime_enable(&i2c_dev->adapter.dev);
	tegra_i2c_gpio_init(i2c_dev);
	tegra_i2c_debugfs_init(i2c_dev);

	return 0;
err:
	tegra_prod_release(&i2c_dev->prod_list);
	return ret;
}

static int tegra_i2c_remove(struct platform_device *pdev)
{
	struct tegra_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	tegra_unregister_pm_notifier(&i2c_dev->pm_nb);
	i2c_del_adapter(&i2c_dev->adapter);
	if (i2c_dev->tx_dma_chan)
		tegra_i2c_deinit_dma_param(i2c_dev, false);

	if (i2c_dev->rx_dma_chan)
		tegra_i2c_deinit_dma_param(i2c_dev, true);


	pm_runtime_disable(&i2c_dev->adapter.dev);

	if (i2c_dev->is_clkon_always)
		tegra_i2c_clock_disable(i2c_dev);

	pm_runtime_disable(&pdev->dev);
	tegra_i2c_debugfs_deinit(i2c_dev);
	tegra_prod_release(&i2c_dev->prod_list);
	return 0;
}

static void tegra_i2c_shutdown(struct platform_device *pdev)
{
	struct tegra_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	dev_info(i2c_dev->dev, "Bus is shutdown down..\n");
	i2c_shutdown_adapter(&i2c_dev->adapter);
	i2c_dev->is_shutdown = true;
}

#ifdef CONFIG_PM_SLEEP
static int __tegra_i2c_suspend_noirq_late(struct tegra_i2c_dev *i2c_dev)
{
	i2c_dev->is_suspended = true;
	if (i2c_dev->is_clkon_always)
		tegra_i2c_clock_disable(i2c_dev);

	return 0;
}

static int tegra_i2c_suspend_noirq_late(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	i2c_lock_adapter(&i2c_dev->adapter);

	__tegra_i2c_suspend_noirq_late(i2c_dev);

	i2c_unlock_adapter(&i2c_dev->adapter);

	return 0;
}

static int __tegra_i2c_resume_noirq_early(struct tegra_i2c_dev *i2c_dev)
{
	int ret;

	if (i2c_dev->is_clkon_always)
		tegra_i2c_clock_enable(i2c_dev);

	ret = tegra_i2c_init(i2c_dev);
	if (ret)
		return ret;

	i2c_dev->is_suspended = false;

	return 0;
}

static int tegra_i2c_resume_noirq_early(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_i2c_dev *i2c_dev = platform_get_drvdata(pdev);

	i2c_lock_adapter(&i2c_dev->adapter);

	__tegra_i2c_resume_noirq_early(i2c_dev);

	i2c_unlock_adapter(&i2c_dev->adapter);

	return 0;
}

static int tegra_i2c_pm_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tegra_i2c_dev *i2c_dev = container_of(nb, struct tegra_i2c_dev, pm_nb);

	if (event == TEGRA_PM_SUSPEND)
		__tegra_i2c_suspend_noirq_late(i2c_dev);
	else if (event == TEGRA_PM_RESUME)
		__tegra_i2c_resume_noirq_early(i2c_dev);

	return NOTIFY_OK;
}

static const struct dev_pm_ops tegra_i2c_pm = {
	.suspend_noirq_late = tegra_i2c_suspend_noirq_late,
	.resume_noirq_early = tegra_i2c_resume_noirq_early,
};
#define TEGRA_I2C_PM	(&tegra_i2c_pm)
#else
#define TEGRA_I2C_PM	NULL
static int tegra_i2c_pm_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	return NOTIFY_OK;
}
#endif

static struct platform_driver tegra_i2c_driver = {
	.probe   = tegra_i2c_probe,
	.remove  = tegra_i2c_remove,
	.late_shutdown = tegra_i2c_shutdown,
	.id_table = tegra_i2c_devtype,
	.driver  = {
		.name  = "tegra-i2c",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_i2c_of_match),
		.pm    = TEGRA_I2C_PM,
	},
};

static int __init tegra_i2c_init_driver(void)
{
	return platform_driver_register(&tegra_i2c_driver);
}

static void __exit tegra_i2c_exit_driver(void)
{
	platform_driver_unregister(&tegra_i2c_driver);
}

subsys_initcall(tegra_i2c_init_driver);
module_exit(tegra_i2c_exit_driver);

MODULE_DESCRIPTION("nVidia Tegra2 I2C Bus Controller driver");
MODULE_AUTHOR("Colin Cross");
MODULE_LICENSE("GPL v2");
