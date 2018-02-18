/*
 * SPI driver for NVIDIA's Tegra124 SPI Controller.
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-tegra.h>
#include <linux/clk/tegra.h>
#include <linux/pinctrl/pinconf-tegra.h>

#define SPI_COMMAND1				0x000
#define SPI_BIT_LENGTH(x)			(((x) & 0x1f) << 0)
#define SPI_PACKED				(1 << 5)
#define SPI_TX_EN				(1 << 11)
#define SPI_RX_EN				(1 << 12)
#define SPI_BOTH_EN_BYTE			(1 << 13)
#define SPI_BOTH_EN_BIT				(1 << 14)
#define SPI_LSBYTE_FE				(1 << 15)
#define SPI_LSBIT_FE				(1 << 16)
#define SPI_BIDIROE				(1 << 17)
#define SPI_IDLE_SDA_DRIVE_LOW			(0 << 18)
#define SPI_IDLE_SDA_DRIVE_HIGH			(1 << 18)
#define SPI_IDLE_SDA_PULL_LOW			(2 << 18)
#define SPI_IDLE_SDA_PULL_HIGH			(3 << 18)
#define SPI_IDLE_SDA_MASK			(3 << 18)
#define SPI_CS_SS_VAL				(1 << 20)
#define SPI_CS_SW_HW				(1 << 21)
/* SPI_CS_POL_INACTIVE bits are default high */
#define SPI_CS_POL_INACTIVE			22
#define SPI_CS_POL_INACTIVE_0			(1 << 22)
#define SPI_CS_POL_INACTIVE_1			(1 << 23)
#define SPI_CS_POL_INACTIVE_2			(1 << 24)
#define SPI_CS_POL_INACTIVE_3			(1 << 25)
#define SPI_CS_POL_INACTIVE_MASK		(0xF << 22)

#define SPI_CS_SEL_0				(0 << 26)
#define SPI_CS_SEL_1				(1 << 26)
#define SPI_CS_SEL_2				(2 << 26)
#define SPI_CS_SEL_3				(3 << 26)
#define SPI_CS_SEL_MASK				(3 << 26)
#define SPI_CS_SEL(x)				(((x) & 0x3) << 26)
#define SPI_CS(x)				(((x) >> 26) & 0x3)
#define SPI_CONTROL_MODE_0			(0 << 28)
#define SPI_CONTROL_MODE_1			(1 << 28)
#define SPI_CONTROL_MODE_2			(2 << 28)
#define SPI_CONTROL_MODE_3			(3 << 28)
#define SPI_CONTROL_MODE_MASK			(3 << 28)
#define SPI_MODE_SEL(x)				(((x) & 0x3) << 28)
#define SPI_MODE(x)				(((x) >> 28) & 0x3)
#define SPI_M_S					(1 << 30)
#define SPI_PIO					(1 << 31)

#define SPI_CS_TIMING2				0x00C

#define SPI_TRANS_STATUS			0x010
#define SPI_BLK_CNT(val)			(((val) >> 0) & 0xFFFF)
#define SPI_SLV_IDLE_COUNT(val)			(((val) >> 16) & 0xFF)
#define SPI_RDY					(1 << 30)

#define SPI_FIFO_STATUS				0x014
#define SPI_RX_FIFO_EMPTY			(1 << 0)
#define SPI_RX_FIFO_FULL			(1 << 1)
#define SPI_TX_FIFO_EMPTY			(1 << 2)
#define SPI_TX_FIFO_FULL			(1 << 3)
#define SPI_RX_FIFO_UNF				(1 << 4)
#define SPI_RX_FIFO_OVF				(1 << 5)
#define SPI_TX_FIFO_UNF				(1 << 6)
#define SPI_TX_FIFO_OVF				(1 << 7)
#define SPI_ERR					(1 << 8)
#define SPI_TX_FIFO_FLUSH			(1 << 14)
#define SPI_RX_FIFO_FLUSH			(1 << 15)
#define SPI_TX_FIFO_EMPTY_COUNT(val)		(((val) >> 16) & 0x7F)
#define SPI_RX_FIFO_FULL_COUNT(val)		(((val) >> 23) & 0x7F)
#define SPI_FRAME_END				(1 << 30)
#define SPI_CS_INACTIVE				(1 << 31)
#define IS_SPI_CS_INACTIVE(val)			(val >> 31)
#define SPI_FIFO_ERROR				(SPI_RX_FIFO_UNF | \
			SPI_RX_FIFO_OVF | SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF)
#define SPI_FIFO_EMPTY			(SPI_RX_FIFO_EMPTY | SPI_TX_FIFO_EMPTY)

#define SPI_TX_DATA				0x018
#define SPI_RX_DATA				0x01C

#define SPI_DMA_CTL				0x020
#define SPI_TX_TRIG_1				(0 << 15)
#define SPI_TX_TRIG_4				(1 << 15)
#define SPI_TX_TRIG_8				(2 << 15)
#define SPI_TX_TRIG_16				(3 << 15)
#define SPI_TX_TRIG_MASK			(3 << 15)
#define SPI_TX_TRIG(val)			(((val) >> 15) & 0x3)
#define SPI_RX_TRIG_1				(0 << 19)
#define SPI_RX_TRIG_4				(1 << 19)
#define SPI_RX_TRIG_8				(2 << 19)
#define SPI_RX_TRIG_16				(3 << 19)
#define SPI_RX_TRIG_MASK			(3 << 19)
#define SPI_RX_TRIG(val)			(((val) >> 19) & 0x3)
#define SPI_IE_TX				(1 << 28)
#define SPI_IE_RX				(1 << 29)
#define SPI_CONT				(1 << 30)
#define SPI_DMA					(1 << 31)
#define SPI_DMA_EN				SPI_DMA

#define SPI_DMA_BLK				0x024
#define SPI_DMA_BLK_SET(x)			(((x) & 0xFFFF) << 0)

#define SPI_TX_FIFO				0x108
#define SPI_RX_FIFO				0x188

#define SPI_INTR_MASK		0x18c
#define SPI_INTR_RX_FIFO_UNF_MASK  (1 << 25)
#define SPI_INTR_RX_FIFO_OVF_MASK  (1 << 26)
#define SPI_INTR_TX_FIFO_UNF_MASK  (1 << 27)
#define SPI_INTR_TX_FIFO_OVF_MASK  (1 << 28)
#define SPI_INTR_RDY_MASK          (1 << 29)
#define SPI_INTR_FRAME_END_MASK    (1 << 30)
#define SPI_INTR_CS_MASK           (1 << 31)

#define SPI_MISC_REG				0x194
#define SPI_MISC_EXT_CLK_EN			(1 << 30)
#define SPI_MISC_CLKEN_OVERRIDE			(1 << 31)

#define MAX_CHIP_SELECT				4
#define SPI_FIFO_DEPTH				64
#define DATA_DIR_TX				(1 << 0)
#define DATA_DIR_RX				(1 << 1)

#define SPI_DMA_TIMEOUT				(msecs_to_jiffies(1000))
#define DEFAULT_SPI_DMA_BUF_LEN			(256*1024)
#define TX_FIFO_EMPTY_COUNT_MAX			SPI_TX_FIFO_EMPTY_COUNT(0x40)
#define RX_FIFO_FULL_COUNT_ZERO			SPI_RX_FIFO_FULL_COUNT(0)
#define MAX_HOLD_CYCLES				16
#define SPI_DEFAULT_SPEED			25000000

#define SPI_FIFO_DEPTH				64
#define SPI_FIFO_FLUSH_MAX_DELAY		2000
#define MAX_PACKETS				(1 << 16)
/* multiplication facotr for slave timeout, See code below */
#define DELAY_MUL_FACTOR			1000

#define PROFILE_SPI_SLAVE	/* profile spi sw overhead */
#define VERBOSE_DUMP_REGS	/* register dumps */
#define TEGRA_SPI_SLAVE_DEBUG	/* to enable debug interfaces, sysfs ... */

#define dump_xfer(_t, _x)						\
	dev_dbg(((_t))->dev, "%s%s len:%d bpw:%d %dus %dHz",		\
		(_x)->rx_buf ? "Rx" : "", (_x)->tx_buf ? "Tx" : "",	\
		(_x)->len, (_x)->bits_per_word & 0xff,			\
		(_x)->delay_usecs & 0xffff, (_x)->speed_hz);

#define dump_sw_state(_t, _f, ...)					\
do {									\
	dev_dbg((_t)->dev, _f, __VA_ARGS__);				\
	dev_dbg((_t)->dev, "[SW] cntrlr: %dHz bypw:%d dma?:%d dir?:%d "	\
		"dmasz:%d rx-trig:%d vlen?:%d pack?:%d force_unp?:%d\n",\
		(_t)->cur_speed, (_t)->bytes_per_word,			\
		(_t)->is_curr_dma_xfer,	(_t)->cur_direction,		\
		(_t)->curr_dma_words, (_t)->rx_trig_words,		\
		(_t)->variable_length_transfer, (_t)->is_packed,	\
		(_t)->force_unpacked_mode);				\
} while (0)


#ifdef VERBOSE_DUMP_REGS
#define dump_regs(_log, _t, _f, ...)				\
do {								\
	uint32_t fifo, cmd, dma_ctl, dma_blk, trans;		\
	fifo = tegra_spi_readl((_t), SPI_FIFO_STATUS);		\
	cmd = tegra_spi_readl((_t), SPI_COMMAND1);		\
	dma_ctl = tegra_spi_readl((_t), SPI_DMA_CTL);		\
	dma_blk = tegra_spi_readl((_t), SPI_DMA_BLK);		\
	trans = tegra_spi_readl((_t), SPI_TRANS_STATUS);	\
	dev_##_log(&((_t)->master->dev), _f, ##__VA_ARGS__);	\
	dev_##_log(&((_t)->master->dev),			\
		"  CMD[%08x]: %s %s M%d CS%d [%c%c%c%c] %s "	\
			"%s %s %s %s %db "			\
		"TRANS[%08x]:%s I:%d B:%d\n"			\
		" FIFO[%08x]:RxF:%d TxE:%d Err[%s%s%s] "	\
			"RxSTA[%s%s%s%s] TxSTA[%s%s%s%s]"	\
		"DMA[%08x]:%s %s %s %s "			\
		"RxTr:%d TxTr:%d B:%d\n",			\
		/* command */					\
		cmd,						\
		cmd & SPI_PIO ? "Go" : "",			\
		cmd & SPI_M_S ? "Ma" : "Sl",			\
		SPI_MODE(cmd), SPI_CS(cmd),			\
		(cmd & SPI_CS_POL_INACTIVE_0) ? 'H' : 'L',	\
		(cmd & SPI_CS_POL_INACTIVE_1) ? 'H' : 'L',	\
		(cmd & SPI_CS_POL_INACTIVE_2) ? 'H' : 'L',	\
		(cmd & SPI_CS_POL_INACTIVE_3) ? 'H' : 'L',	\
		(cmd & SPI_LSBYTE_FE) ? "LSB" : "MSB",		\
		(cmd & SPI_LSBIT_FE) ? "LSb" : "MSb",		\
		(cmd & SPI_RX_EN) ? "Rx" : "",			\
		(cmd & SPI_TX_EN) ? "Tx" : "",			\
		(cmd & SPI_PACKED) ? "Pa" : "Un",		\
		(SPI_BIT_LENGTH(cmd) + 1),			\
		/* transfer status */				\
		trans,						\
		(trans & SPI_RDY) ? "RDY" : "BSY",		\
		SPI_SLV_IDLE_COUNT(trans),			\
		SPI_BLK_CNT(trans),				\
		/* fifo */					\
		fifo,						\
		SPI_RX_FIFO_FULL_COUNT(fifo),			\
		SPI_TX_FIFO_EMPTY_COUNT(fifo),			\
		(fifo & SPI_CS_INACTIVE) ? "Cs" : "",		\
		(fifo & SPI_FRAME_END) ? "Fe" : "" ,		\
		(fifo & SPI_FIFO_ERROR) ? "Er" : "" ,		\
		(fifo & SPI_RX_FIFO_OVF) ? "O" : "" ,		\
		(fifo & SPI_RX_FIFO_UNF) ? "U" : "" ,		\
		(fifo & SPI_RX_FIFO_FULL) ? "F" : "" ,		\
		(fifo & SPI_RX_FIFO_EMPTY) ? "E" : "" ,		\
		(fifo & SPI_TX_FIFO_OVF) ? "O" : "" ,		\
		(fifo & SPI_TX_FIFO_UNF) ? "U" : "" ,		\
		(fifo & SPI_TX_FIFO_FULL) ? "F" : "" ,		\
		(fifo & SPI_TX_FIFO_EMPTY) ? "E" : "" ,		\
		/* dma ctl */					\
		dma_ctl,					\
		(dma_ctl & SPI_DMA_EN) ? "De" : "",		\
		(dma_ctl & SPI_CONT) ? "Co" : "",		\
		(dma_ctl & SPI_IE_RX) ? "RxIE" : "",		\
		(dma_ctl & SPI_IE_TX) ? "TxIE" : "",		\
		SPI_RX_TRIG(dma_ctl),				\
		SPI_TX_TRIG(dma_ctl),				\
		/* dma blk */					\
		SPI_DMA_BLK_SET(dma_blk));			\
} while (0)
#else
#define dump_regs(_log, _t, _f, ...)					\
	dev_##_log(&((_t)->master->dev), _f, ##__VA_ARGS__);
#endif

#define print_fifo_word(_m, _t, _c, _x)					\
	dev_dbg((_t)->dev,						\
			"%s[%d] %02lx %02lx %02lx %02lx\n",		\
			(_m), (_c),					\
			(_x) & 0xff, ((_x) >> 8) & 0xff,		\
			((_x) >> 16) & 0xff, ((_x) >> 24) & 0xff);

/* convert size in fifo bytes to number of packets */
/* for unpacked, each packet occupies 4 bytes, i.e., a fifo word
 * for packed the each fifo word can be occupied by multiple packets
 *  as number of bytes in each packet.
 */
#define fifo_bytes_to_packets(_t, _sz)				\
	((_t)->is_packed ? (_sz)/(_t)->bytes_per_word : (_sz)/4);

struct tegra_spi_chip_data {
	bool intr_mask_reg;
	bool mask_cs_inactive_intr;
	bool new_features;
};

struct tegra_spi_data {
	struct device				*dev;
	struct spi_master			*master;
	spinlock_t				lock;

	struct clk				*clk;
	struct reset_control			*rstc;
	void __iomem				*base;
	phys_addr_t				phys;
	unsigned				irq;
	int					dma_req_sel;
	bool					clock_always_on;
	u32					spi_max_frequency;
	u32					cur_speed;
	unsigned				min_div;

	struct spi_device			*cur_spi;
	unsigned				words_per_32bit;
	/* bypw, rounding to nearest byte, irrespective of packed/unpacked */
	unsigned				bytes_per_word;
	unsigned				curr_dma_words;
	unsigned				cur_direction;

	unsigned				dma_buf_size;
	unsigned				max_buf_size;
	bool					is_curr_dma_xfer;
	bool					variable_length_transfer;

	/* Slave Ready Polarity (true: Active High, false: Active Low) */
	int					gpio_slave_ready;
	bool					slave_ready_active_high;

	struct completion			rx_dma_complete;
	struct completion			tx_dma_complete;

	u32					tx_status;
	u32					rx_status;
	bool					reset_ctrl_status;
	u32					status_reg;
	bool					is_packed;

	u32					command1_reg;
	u32					dma_control_reg;
	u32					def_command1_reg;

	struct completion			xfer_completion;
	struct spi_transfer			*curr_xfer;
	struct dma_chan				*rx_dma_chan;
	u32					*rx_dma_buf;
	dma_addr_t				rx_dma_phys;
	struct dma_async_tx_descriptor		*rx_dma_desc;

	struct dma_chan				*tx_dma_chan;
	u32					*tx_dma_buf;
	dma_addr_t				tx_dma_phys;
	struct dma_async_tx_descriptor		*tx_dma_desc;
	const struct tegra_spi_chip_data  *chip_data;
	struct tegra_spi_device_controller_data cdata[MAX_CHIP_SELECT];

	spi_callback			spi_slave_ready_callback;
	spi_callback			spi_slave_isr_callback;
	void				*client_data;
	atomic_t			isr_expected;
	u32				words_to_transfer;
	int				curr_rx_pos;
	/* common variable for tx and rx to update the actual length */
	int				curr_pos;
	int				rx_dma_len;
	int				rem_len;
	int				rx_trig_words;
	int				force_unpacked_mode;
	char				*clk_pin;
	int				clk_pin_group;
	struct pinctrl_dev		*pctl_dev;
	bool				clk_pin_state_enabled;
#ifdef PROFILE_SPI_SLAVE
	ktime_t				start_time;
	ktime_t				end_time;
	int				profile_size;
#endif
};



static int tegra_spi_runtime_suspend(struct device *dev);
static int tegra_spi_runtime_resume(struct device *dev);
static int tegra_spi_validate_request(struct spi_device *spi,
			struct tegra_spi_data *tspi, struct spi_transfer *t);
static int tegra_clk_pin_control(bool enable, struct tegra_spi_data *tspi);
static int tegra_spi_ext_clk_enable(bool enable, struct tegra_spi_data *tspi);

#ifdef TEGRA_SPI_SLAVE_DEBUG
static ssize_t force_unpacked_mode_set(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct tegra_spi_data *tspi;
	struct spi_master *master = dev_get_drvdata(dev);
	if (master) {
		tspi = spi_master_get_devdata(master);
		if (tspi && count) {
			tspi->force_unpacked_mode = ((buf[0] - '0')  > 0);
			return count;
		}
	}
	return -ENODEV;
}

static ssize_t force_unpacked_mode_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct tegra_spi_data *tspi;
	struct spi_master *master = dev_get_drvdata(dev);
	if (master) {
		tspi = spi_master_get_devdata(master);
		return sprintf(buf, "%d", tspi->force_unpacked_mode);
	}
	return -ENODEV;
}

static DEVICE_ATTR(force_unpacked_mode, 0644, force_unpacked_mode_show,
						force_unpacked_mode_set);
#endif

static inline unsigned long tegra_spi_readl(struct tegra_spi_data *tspi,
		unsigned long reg)
{
	return readl(tspi->base + reg);
}

static inline void tegra_spi_writel(struct tegra_spi_data *tspi,
		unsigned long val, unsigned long reg)
{
	writel(val, tspi->base + reg);

	/* TODO: remove the forced fence read below */
	/* Read back register to make sure that register writes completed */
	if (reg != SPI_TX_FIFO)
		readl(tspi->base + SPI_COMMAND1);
}

static inline void tegra_spi_fence(struct tegra_spi_data *tspi)
{
	/* Read back register to make sure that register writes completed */
	readl(tspi->base + SPI_COMMAND1);
}

/* FIXME: the name of the function */
int tegra124_spi_slave_register_callback(struct spi_device *spi,
	spi_callback func_ready, spi_callback func_isr, void *client_data)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);

	/* Expect atleast one callback function. */
	if ((!tspi) || (!func_ready && !func_isr))
		return -EINVAL;

	tspi->spi_slave_ready_callback = func_ready;
	tspi->spi_slave_isr_callback = func_isr;

	tspi->client_data = client_data;
	return 0;
}
EXPORT_SYMBOL_GPL(tegra124_spi_slave_register_callback);

static void tegra_spi_clear_status(struct tegra_spi_data *tspi)
{
	unsigned long val;

	/* Write 1 to clear status register */
	val = tegra_spi_readl(tspi, SPI_TRANS_STATUS);
	tegra_spi_writel(tspi, val, SPI_TRANS_STATUS);

	if (tspi->chip_data->intr_mask_reg) {
		val = tegra_spi_readl(tspi, SPI_INTR_MASK);
		if (!(val & SPI_INTR_RDY_MASK)) {
			val |= (SPI_INTR_CS_MASK |
				SPI_INTR_FRAME_END_MASK |
				SPI_INTR_RDY_MASK |
				SPI_INTR_RX_FIFO_UNF_MASK |
				SPI_INTR_TX_FIFO_UNF_MASK |
				SPI_INTR_RX_FIFO_OVF_MASK |
				SPI_INTR_TX_FIFO_OVF_MASK);
			tegra_spi_writel(tspi, val, SPI_INTR_MASK);
		}
	}

	/* Clear fifo status error if any */
	val = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	tegra_spi_writel(tspi, val &
				(SPI_FRAME_END | SPI_CS_INACTIVE | SPI_ERR |
				SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF |
				SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF),
			SPI_FIFO_STATUS);
}

static void reset_controller(struct tegra_spi_data *tspi)
{
	if (!tspi->reset_ctrl_status)
		return;

	/* TODO: Debug the reset sequence */
	if (tspi->is_curr_dma_xfer &&
			(tspi->cur_direction & DATA_DIR_TX))
		dmaengine_terminate_all(tspi->tx_dma_chan);
	if (tspi->is_curr_dma_xfer &&
			(tspi->cur_direction & DATA_DIR_RX))
		dmaengine_terminate_all(tspi->rx_dma_chan);
	wmb(); /* barrier for dma terminate to happen */
	reset_control_reset(tspi->rstc);
	/* restore default value */
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	/* set CS inactive b/w packets to mask CS_INACTIVE interrupts */
	if (tspi->chip_data->mask_cs_inactive_intr)
		tegra_spi_writel(tspi, 0, SPI_CS_TIMING2);
	tegra_spi_clear_status(tspi);
	tegra_spi_writel(tspi, tspi->dma_control_reg, SPI_DMA_CTL);
	dump_regs(dbg, tspi, "after controller reset");
	tspi->reset_ctrl_status = false;
}

static unsigned tegra_spi_calculate_curr_xfer_param(
	struct spi_device *spi, struct tegra_spi_data *tspi,
	struct spi_transfer *t)
{
	unsigned remain_len = t->len;
	unsigned max_word;
	unsigned bits_per_word;
	unsigned max_len;
	unsigned total_fifo_words;

	bits_per_word = t->bits_per_word ? t->bits_per_word :
						spi->bits_per_word;
	tspi->bytes_per_word = (bits_per_word - 1) / 8 + 1;

	if (!tspi->force_unpacked_mode &&
			(bits_per_word == 8 || bits_per_word == 16)) {
		tspi->is_packed = 1;
		tspi->words_per_32bit = 32/bits_per_word;
	} else {
		tspi->is_packed = 0;
		tspi->words_per_32bit = 1;
	}

	if (tspi->is_packed) {
		max_len = min(remain_len, tspi->max_buf_size);
		tspi->curr_dma_words = max_len/tspi->bytes_per_word;
		total_fifo_words = (max_len + 3)/4;
	} else {
		max_word = (remain_len - 1) / tspi->bytes_per_word + 1;
		max_word = min(max_word, tspi->max_buf_size/4);
		tspi->curr_dma_words = max_word;
		total_fifo_words = max_word;
	}
	return total_fifo_words;
}

static unsigned tegra_spi_fill_tx_fifo_from_client_txbuf(
	struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned nbytes;
	unsigned tx_empty_count;
	unsigned max_n_32bit;
	unsigned i, count;
	unsigned long x;
	unsigned int written_words;
	unsigned fifo_words_left;
	u8 *tx_buf = (u8 *)t->tx_buf;

	tx_empty_count = SPI_TX_FIFO_EMPTY_COUNT(
			tegra_spi_readl(tspi, SPI_FIFO_STATUS));

	if (tspi->is_packed) {
		fifo_words_left = tx_empty_count * tspi->words_per_32bit;
		written_words = min(fifo_words_left, tspi->curr_dma_words);
		nbytes = written_words * tspi->bytes_per_word;
		max_n_32bit = DIV_ROUND_UP(nbytes, 4);
		for (count = 0; count < max_n_32bit; count++) {
			x = 0;
			for (i = 0; (i < 4) && nbytes; i++, nbytes--)
				x |= (*tx_buf++) << (i*8);

			print_fifo_word("TxFIFO", tspi, count, x);
			tegra_spi_writel(tspi, x, SPI_TX_FIFO);
		}
	} else {
		max_n_32bit = min(tspi->curr_dma_words,  tx_empty_count);
		written_words = max_n_32bit;
		nbytes = written_words * tspi->bytes_per_word;
		for (count = 0; count < max_n_32bit; count++) {
			x = 0;
			for (i = 0; nbytes && (i < tspi->bytes_per_word);
							i++, nbytes--)
				x |= ((*tx_buf++) << i*8);
			print_fifo_word("TxFIFO", tspi, count, x);
			tegra_spi_writel(tspi, x, SPI_TX_FIFO);
		}
	}
	return written_words;
}

/* FIFO mode rx data copy from SPI-FIFO to client buffer */
static unsigned int tegra_spi_read_rx_fifo_to_client_rxbuf(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned rx_full_count;
	unsigned i, count;
	unsigned long x;
	unsigned recv_len, npackets;
	u8 *rx_buf = (u8 *)t->rx_buf + tspi->curr_rx_pos;

	rx_full_count = SPI_RX_FIFO_FULL_COUNT(
			tegra_spi_readl(tspi, SPI_FIFO_STATUS));

	/* actual bytes to be copied */
	npackets = fifo_bytes_to_packets(tspi, rx_full_count*4);
	npackets = min(npackets, tspi->words_to_transfer);
	recv_len = npackets * tspi->bytes_per_word;

	if (tspi->is_packed) {
		int j = recv_len;
		if (rx_full_count != DIV_ROUND_UP(tspi->words_to_transfer *
						tspi->bytes_per_word, 4))
			dump_regs(dbg, tspi,
				"fifo-cnt[%d] != trans-cnt[%d]",
				rx_full_count, tspi->words_to_transfer);

		for (count = 0; j && count < rx_full_count; count++) {
			x = tegra_spi_readl(tspi, SPI_RX_FIFO);
			print_fifo_word("RxFIFO", tspi, count, x);
			for (i = 0; j && (i < 4); i++, j--)
				*rx_buf++ = (x >> i*8) & 0xFF;
		}
	} else {
		unsigned int bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
						tspi->cur_spi->bits_per_word;
		if (rx_full_count != tspi->words_to_transfer)
			dump_regs(dbg, tspi,
				"fifo-cnt[%d] != trans-cnt[%d]",
				rx_full_count, tspi->words_to_transfer);

		rx_full_count = min(rx_full_count, tspi->words_to_transfer);
		for (count = 0; count < rx_full_count; count++) {
			x = tegra_spi_readl(tspi, SPI_RX_FIFO);
			print_fifo_word("RxFIFO", tspi, count, x);
		for (i = 0; (i < tspi->bytes_per_word); i++)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
	}
	tspi->words_to_transfer -= npackets;
	tspi->rem_len -= recv_len;
	tspi->curr_rx_pos += recv_len;
	return recv_len;
}

static void tegra_spi_copy_client_txbuf_to_spi_txbuf(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned len;

	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(tspi->dev, tspi->tx_dma_phys,
				tspi->dma_buf_size, DMA_TO_DEVICE);

	if (tspi->is_packed) {
		len = tspi->curr_dma_words * tspi->bytes_per_word;
		memcpy(tspi->tx_dma_buf, t->tx_buf, len);
	} else {
		unsigned int i;
		unsigned int count;
		u8 *tx_buf = (u8 *)t->tx_buf;
		unsigned consume = tspi->curr_dma_words * tspi->bytes_per_word;
		unsigned int x;

		for (count = 0; count < tspi->curr_dma_words; count++) {
			x = 0;
			for (i = 0; consume && (i < tspi->bytes_per_word);
							i++, consume--)
				x |= ((*tx_buf++) << i * 8);
			tspi->tx_dma_buf[count] = x;
		}
	}

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(tspi->dev, tspi->tx_dma_phys,
				tspi->dma_buf_size, DMA_TO_DEVICE);
}

static unsigned int tegra_spi_copy_spi_rxbuf_to_client_rxbuf(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned dma_bytes, npackets;

	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(tspi->dev, tspi->rx_dma_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);

	dma_bytes = tspi->words_to_transfer * tspi->bytes_per_word;
	npackets = tspi->words_to_transfer;
	if (tspi->is_packed) {
		memcpy(t->rx_buf, tspi->rx_dma_buf, dma_bytes);
	} else {
		unsigned int i;
		unsigned int count;
		unsigned char *rx_buf = t->rx_buf;
		unsigned int x;
		unsigned int bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
						tspi->cur_spi->bits_per_word;
		/* copy received dma words to rx client buffer */
		for (count = 0; count <  tspi->words_to_transfer; count++) {
			x = tspi->rx_dma_buf[count];
			for (i = 0; (i < tspi->bytes_per_word); i++)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
	}

	tspi->words_to_transfer -= npackets;
	tspi->rem_len -= npackets*tspi->bytes_per_word;
	tspi->curr_rx_pos += npackets*tspi->bytes_per_word;

	if (tspi->words_to_transfer)
		dma_bytes += tegra_spi_read_rx_fifo_to_client_rxbuf(tspi, t);

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(tspi->dev, tspi->rx_dma_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);
	return dma_bytes;
}

static void tegra_spi_rx_dma_complete(void *args)
{
	struct tegra_spi_data *tspi = args;
	dev_dbg(tspi->dev, "rx-dma-complete\n");
	complete(&tspi->rx_dma_complete);
	/* TODO: how to get transfer size from dma */
}

static void tegra_spi_tx_dma_complete(void *args)
{
	struct tegra_spi_data *tspi = args;
	dev_dbg(tspi->dev, "tx-dma-complete\n");
	complete(&tspi->tx_dma_complete);
}

static int tegra_spi_start_tx_dma(struct tegra_spi_data *tspi, int len)
{
	reinit_completion(&tspi->tx_dma_complete);
	dev_dbg(tspi->dev, "Starting tx dma for len:%d\n", len);
	tspi->tx_dma_desc = dmaengine_prep_slave_single(tspi->tx_dma_chan,
				tspi->tx_dma_phys, len, DMA_MEM_TO_DEV,
				DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!tspi->tx_dma_desc) {
		dev_err(tspi->dev, "Not able to get desc for Tx\n");
		return -EIO;
	}

	tspi->tx_dma_desc->callback = tegra_spi_tx_dma_complete;
	tspi->tx_dma_desc->callback_param = tspi;

	dmaengine_submit(tspi->tx_dma_desc);
	dma_async_issue_pending(tspi->tx_dma_chan);
	return 0;
}

static int tegra_spi_start_rx_dma(struct tegra_spi_data *tspi, int len)
{
	reinit_completion(&tspi->rx_dma_complete);
	tspi->rx_dma_len = len;

	dev_dbg(tspi->dev, "Starting rx dma for len:%d\n", len);
	tspi->rx_dma_desc = dmaengine_prep_slave_single(tspi->rx_dma_chan,
				tspi->rx_dma_phys, len, DMA_DEV_TO_MEM,
				DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!tspi->rx_dma_desc) {
		dev_err(tspi->dev, "Not able to get desc for Rx\n");
		return -EIO;
	}

	tspi->rx_dma_desc->callback = tegra_spi_rx_dma_complete;
	tspi->rx_dma_desc->callback_param = tspi;

	dmaengine_submit(tspi->rx_dma_desc);
	dma_async_issue_pending(tspi->rx_dma_chan);
	return 0;
}

static int check_and_clear_fifo(struct tegra_spi_data *tspi)
{
	unsigned long status;
	int cnt = SPI_FIFO_FLUSH_MAX_DELAY;

	/* Make sure that Rx and Tx fifo are empty */
	status = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	if ((status & SPI_FIFO_EMPTY) != SPI_FIFO_EMPTY) {
		/* flush the fifo */
		status |= (SPI_RX_FIFO_FLUSH | SPI_TX_FIFO_FLUSH);
		tegra_spi_writel(tspi, status, SPI_FIFO_STATUS);
		do {
			status = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
			if ((status & SPI_FIFO_EMPTY) == SPI_FIFO_EMPTY)
				return 0;
			udelay(1);
		} while (cnt--);
		dev_err(tspi->dev,
			"Rx/Tx fifo are not empty status 0x%08lx\n", status);
		return -EIO;
	}
	return 0;
}

static inline void tegra_spi_slave_busy(struct tegra_spi_data *tspi)
{
	int deassert_val;

	if (tspi->slave_ready_active_high)
		deassert_val = 0;
	else
		deassert_val = 1;

	/* Deassert ready line to indicate Busy */
	if (gpio_is_valid(tspi->gpio_slave_ready))
		gpio_set_value(tspi->gpio_slave_ready, deassert_val);
}

static inline void tegra_spi_slave_ready(struct tegra_spi_data *tspi)
{
	int assert_val;

	if (tspi->slave_ready_active_high)
		assert_val = 1;
	else
		assert_val = 0;

	/* Assert ready line to indicate Ready */
	if (gpio_is_valid(tspi->gpio_slave_ready))
		gpio_set_value(tspi->gpio_slave_ready, assert_val);
}

static inline int tegra_spi_ext_clk_enable(bool enable,
				struct tegra_spi_data *tspi)
{
	unsigned long misc_reg = 0;
	int ret = 0;

	if (tspi->chip_data->new_features) {
		/* Enable external clock bit in SPI_MISC_REG */
		if (enable)
			misc_reg |= SPI_MISC_EXT_CLK_EN;
		else
			misc_reg &= (~SPI_MISC_EXT_CLK_EN);

		tegra_spi_writel(tspi, misc_reg, SPI_MISC_REG);
	} else {
		/* Software based clock control for metastability */
		ret = tegra_clk_pin_control(enable, tspi);
	}

	return ret;

}
static int tegra_spi_start_dma_based_transfer(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned long val, flags;
	unsigned long intr_mask;
	unsigned int len;
	int ret = 0;
	int maxburst = 0;
	struct dma_slave_config dma_sconfig;

	/* Make sure that Rx and Tx fifo are empty */
	ret = check_and_clear_fifo(tspi);
	if (ret != 0)
		return ret;

	val = SPI_DMA_BLK_SET(tspi->curr_dma_words - 1);
	tegra_spi_writel(tspi, val, SPI_DMA_BLK);

	val = 0; /* Reset the variable for reuse */

	if (tspi->is_packed)
		len = DIV_ROUND_UP(tspi->curr_dma_words * tspi->bytes_per_word,
					4) * 4;
	else
		len = tspi->curr_dma_words * 4;

	if (!tspi->rx_trig_words) {
		/* Set attention level based on length of transfer */
		if (len & 0xF) {
			val |= SPI_TX_TRIG_1 | SPI_RX_TRIG_1;
			maxburst = 1;
		} else if (((len) >> 4) & 0x1) {
			val |= SPI_TX_TRIG_4 | SPI_RX_TRIG_4;
			maxburst = 4;
		} else {
			val |= SPI_TX_TRIG_8 | SPI_RX_TRIG_8;
			maxburst = 8;
		}
	} else { /* dt can override the trigger */
		if (tspi->rx_trig_words == 4) {
			val |= SPI_TX_TRIG_4 | SPI_RX_TRIG_4;
			maxburst = 4;
		} else if (tspi->rx_trig_words == 8) {
			val |= SPI_TX_TRIG_8 | SPI_RX_TRIG_8;
			maxburst = 8;
		}
	}

	if (tspi->variable_length_transfer &&
		tspi->chip_data->new_features &&
		(tspi->cur_direction & DATA_DIR_RX)) {
		val &= ~SPI_RX_TRIG_MASK;
		val |= SPI_RX_TRIG_16;
	}

	if (tspi->chip_data->intr_mask_reg) {
		if ((tspi->cur_direction & DATA_DIR_TX) ||
				(tspi->cur_direction & DATA_DIR_RX)) {
			intr_mask = tegra_spi_readl(tspi, SPI_INTR_MASK);
			intr_mask &= ~(SPI_INTR_CS_MASK |
					SPI_INTR_RDY_MASK |
					SPI_INTR_RX_FIFO_UNF_MASK |
					SPI_INTR_TX_FIFO_UNF_MASK |
					SPI_INTR_RX_FIFO_OVF_MASK |
					SPI_INTR_TX_FIFO_OVF_MASK);
			tegra_spi_writel(tspi, intr_mask, SPI_INTR_MASK);
		}
	} else if (!tspi->chip_data->new_features) {
		if (tspi->cur_direction & DATA_DIR_TX)
			val |= SPI_IE_TX;
		if (tspi->cur_direction & DATA_DIR_RX)
			val |= SPI_IE_RX;
	}

	tegra_spi_writel(tspi, val, SPI_DMA_CTL);
	tspi->dma_control_reg = val;

	if (tspi->cur_direction & DATA_DIR_TX) {
		dma_sconfig.dst_addr = tspi->phys + SPI_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = maxburst;
		dmaengine_slave_config(tspi->tx_dma_chan, &dma_sconfig);

		tegra_spi_copy_client_txbuf_to_spi_txbuf(tspi, t);
		ret = tegra_spi_start_tx_dma(tspi, len);
		if (ret < 0) {
			dev_err(tspi->dev,
				"Starting tx dma failed, err %d\n", ret);
			return ret;
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		/* Make the dma buffer to read by dma */
		dma_sync_single_for_device(tspi->dev, tspi->rx_dma_phys,
				tspi->dma_buf_size, DMA_FROM_DEVICE);

		dma_sconfig.src_addr = tspi->phys + SPI_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		if (tspi->variable_length_transfer &&
			tspi->chip_data->new_features)
			dma_sconfig.src_maxburst = 16;
		else
			dma_sconfig.src_maxburst = maxburst;
		dmaengine_slave_config(tspi->rx_dma_chan, &dma_sconfig);

		/* align Rx Dma to receive size */
		ret = tegra_spi_start_rx_dma(tspi,
			(len >> tspi->rx_trig_words) << tspi->rx_trig_words);
		if (ret < 0) {
			dev_err(tspi->dev,
				"Starting rx dma failed, err %d\n", ret);
			if (tspi->cur_direction & DATA_DIR_TX)
				dmaengine_terminate_all(tspi->tx_dma_chan);
			return ret;
		}
	}
	tspi->is_curr_dma_xfer = true;
	tspi->dma_control_reg = val;

	val |= SPI_DMA_EN;
	dump_regs(dbg, tspi, "Before DMA EN");

	/* atomically set both sw/hw isr enable */
	spin_lock_irqsave(&tspi->lock, flags);
	atomic_set(&tspi->isr_expected, 1);
	wmb(); /* to complete the register writes and the atomic var update */
	tegra_spi_writel(tspi, val, SPI_DMA_CTL);
	tegra_spi_fence(tspi);
	spin_unlock_irqrestore(&tspi->lock, flags);
	ret = tegra_spi_ext_clk_enable(true, tspi);

	return ret;
}

/* Enable/Disable clk input using pinctrl config. It helps to avoid
 * meta-stability issue by preventing interface clock while writing controller
 * register or resetting controller. This function should not be called from
 * non-sleepable context.
 */
static int tegra_clk_pin_control(bool enable, struct tegra_spi_data *tspi)
{
	unsigned long val;
	int ret = 0;

	if (tspi->clk_pin) {
		if (tspi->clk_pin_state_enabled == enable)
			return ret;

		tspi->clk_pin_state_enabled = enable;
		if (enable) {
			val = TEGRA_PINCONF_PACK(TEGRA_PINCONF_PARAM_ENABLE_INPUT,
					TEGRA_PIN_ENABLE);
		} else {
			val = TEGRA_PINCONF_PACK(TEGRA_PINCONF_PARAM_ENABLE_INPUT,
					TEGRA_PIN_DISABLE);
		}
		ret = pinctrl_set_config_for_group_sel(tspi->pctl_dev,
				tspi->clk_pin_group, val);
		if (ret < 0) {
			dev_err(tspi->dev,
				"spi clk pin input state change err %d\n", ret);
		}
	}
	return ret;
}

static int tegra_spi_start_cpu_based_transfer(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned long val, flags;
	unsigned long intr_mask;
	unsigned cur_words;
	int ret;

	ret = check_and_clear_fifo(tspi);
	if (ret != 0)
		return ret;

	if (tspi->cur_direction & DATA_DIR_TX)
		cur_words = tegra_spi_fill_tx_fifo_from_client_txbuf(tspi, t);
	else
		cur_words = tspi->curr_dma_words;

	val = SPI_DMA_BLK_SET(cur_words - 1);
	tegra_spi_writel(tspi, val, SPI_DMA_BLK);

	val = 0;

	if (tspi->chip_data->intr_mask_reg) {
		if ((tspi->cur_direction & DATA_DIR_TX) ||
				(tspi->cur_direction & DATA_DIR_RX)) {
			intr_mask = tegra_spi_readl(tspi, SPI_INTR_MASK);
			intr_mask &= ~(SPI_INTR_CS_MASK |
					SPI_INTR_RDY_MASK |
					SPI_INTR_RX_FIFO_UNF_MASK |
					SPI_INTR_TX_FIFO_UNF_MASK |
					SPI_INTR_RX_FIFO_OVF_MASK |
					SPI_INTR_TX_FIFO_OVF_MASK);
			tegra_spi_writel(tspi, intr_mask, SPI_INTR_MASK);
		}
	} else {
		if (tspi->cur_direction & DATA_DIR_TX)
			val |= SPI_IE_TX;
		if (tspi->cur_direction & DATA_DIR_RX)
			val |= SPI_IE_RX;
	}

	tegra_spi_writel(tspi, val, SPI_DMA_CTL);
	tspi->dma_control_reg = val;

	tspi->is_curr_dma_xfer = false;
	val = tspi->command1_reg;
	val |= SPI_PIO;
	dump_regs(dbg, tspi, "Before PIO EN");

	/* atomically set both sw/hw isr enable */
	spin_lock_irqsave(&tspi->lock, flags);
	atomic_set(&tspi->isr_expected, 1);
	wmb(); /* to complete the register writes and the atomic var update */
	tegra_spi_writel(tspi, val, SPI_COMMAND1);
	tegra_spi_fence(tspi);
	spin_unlock_irqrestore(&tspi->lock, flags);
	ret = tegra_spi_ext_clk_enable(true, tspi);

	return ret;
}

static int tegra_spi_init_dma_param(struct tegra_spi_data *tspi,
			bool dma_to_memory)
{
	struct dma_chan *dma_chan;
	u32 *dma_buf;
	dma_addr_t dma_phys;
	int ret;
	struct dma_slave_config dma_sconfig;

	dma_chan = dma_request_slave_channel_reason(tspi->dev,
					dma_to_memory ? "rx" : "tx");
	if (IS_ERR(dma_chan)) {
		ret = PTR_ERR(dma_chan);
		if (ret != -EPROBE_DEFER)
			dev_err(tspi->dev,
				"Dma channel is not available: %d\n", ret);
		return ret;
	}

	dma_buf = dma_alloc_coherent(tspi->dev, tspi->dma_buf_size,
				&dma_phys, GFP_KERNEL);
	if (!dma_buf) {
		dev_err(tspi->dev, "Not able to allocate the dma buffer\n");
		dma_release_channel(dma_chan);
		return -ENOMEM;
	}

	dma_sconfig.slave_id = tspi->dma_req_sel;
	if (dma_to_memory) {
		dma_sconfig.src_addr = tspi->phys + SPI_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.src_maxburst = tspi->rx_trig_words;
	} else {
		dma_sconfig.dst_addr = tspi->phys + SPI_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = tspi->rx_trig_words;
	}

	ret = dmaengine_slave_config(dma_chan, &dma_sconfig);
	if (ret)
		goto scrub;
	if (dma_to_memory) {
		tspi->rx_dma_chan = dma_chan;
		tspi->rx_dma_buf = dma_buf;
		tspi->rx_dma_phys = dma_phys;
	} else {
		tspi->tx_dma_chan = dma_chan;
		tspi->tx_dma_buf = dma_buf;
		tspi->tx_dma_phys = dma_phys;
	}
	return 0;

scrub:
	dma_free_coherent(tspi->dev, tspi->dma_buf_size, dma_buf, dma_phys);
	dma_release_channel(dma_chan);
	return ret;
}

static void tegra_spi_deinit_dma_param(struct tegra_spi_data *tspi,
	bool dma_to_memory)
{
	u32 *dma_buf;
	dma_addr_t dma_phys;
	struct dma_chan *dma_chan;

	if (dma_to_memory) {
		dma_buf = tspi->rx_dma_buf;
		dma_chan = tspi->rx_dma_chan;
		dma_phys = tspi->rx_dma_phys;
		tspi->rx_dma_chan = NULL;
		tspi->rx_dma_buf = NULL;
	} else {
		dma_buf = tspi->tx_dma_buf;
		dma_chan = tspi->tx_dma_chan;
		dma_phys = tspi->tx_dma_phys;
		tspi->tx_dma_buf = NULL;
		tspi->tx_dma_chan = NULL;
	}
	if (!dma_chan)
		return;

	dma_free_coherent(tspi->dev, tspi->dma_buf_size, dma_buf, dma_phys);
	dma_release_channel(dma_chan);
}

static int tegra_spi_validate_request(struct spi_device *spi,
			struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	int req_mode;

	req_mode = spi->mode & 0x3;
	if ((req_mode == SPI_MODE_0) || (req_mode == SPI_MODE_2)) {
		if (t->tx_buf) {
			/* Tx is not supported in mode 0 and mode 2 */
			dev_err(tspi->dev, "Tx is not supported in mode %d\n",
					req_mode);
			return -EINVAL;
		}
	}

	/* Check that the all words are available */
	if (t->len % tspi->bytes_per_word != 0) {
		dev_dbg(tspi->dev, "length is not a multiple of bypw\n");
		return -EINVAL;
	}

	/* the transfer needs to be completed in one go */
	if (t->len > tspi->dma_buf_size) {
		dev_dbg(tspi->dev, "message size is greater than dma size\n");
		return -EMSGSIZE;
	}

	/* In unpacked mode, a fifo-word (4 bytes) is used to store a word
	 * irrespective of bytes-per-word setting. This means that the
	 * DMA transfers a FIFO word to dma buffer occuping 4 bytes and
	 * hence the total number of packets possible is 1/4th of dma buff size
	 */
	if (!tspi->is_packed &&
		((t->len / tspi->bytes_per_word) > tspi->dma_buf_size / 4)) {
		dev_dbg(tspi->dev, "Num words is greater than dma size\n");
		return -EMSGSIZE;
	}

	/* the total number of packets should be less than max dma packets */
	if ((t->len / tspi->bytes_per_word) > MAX_PACKETS) {
		dev_dbg(tspi->dev, "Num packets is greater than 64K\n");
		return -EMSGSIZE;
	}
	return 0;
}

static void set_best_clk_source(struct spi_device *spi,
		unsigned long rate)
{
	long new_rate;
	unsigned long err_rate, crate, prate;
	unsigned int cdiv, fin_err = rate;
	int ret;
	struct clk *pclk, *fpclk = NULL;
	const char *pclk_name, *fpclk_name;
	struct device_node *node;
	struct property *prop;
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);

	node = spi->master->dev.of_node;
	if (!of_property_count_strings(node, "nvidia,clk-parents"))
		return;

	/* when parent of a clk changes divider is not changed
	 * set a min div with which clk will not cross max rate
	 */
	if (!tspi->min_div) {
		of_property_for_each_string(node, "nvidia,clk-parents",
				prop, pclk_name) {
			pclk = clk_get(tspi->dev, pclk_name);
			if (IS_ERR(pclk))
				continue;
			prate = clk_get_rate(pclk);
			crate = tspi->spi_max_frequency;
			cdiv = DIV_ROUND_UP(prate, crate);
			if (cdiv > tspi->min_div)
				tspi->min_div = cdiv;
		}
	}

	pclk = clk_get_parent(tspi->clk);
	crate = clk_get_rate(tspi->clk);
	prate = clk_get_rate(pclk);
	cdiv = DIV_ROUND_UP(prate, crate);
	if (cdiv < tspi->min_div) {
		crate = DIV_ROUND_UP(prate, tspi->min_div);
		clk_set_rate(tspi->clk, crate);
	}

	of_property_for_each_string(node, "nvidia,clk-parents",
				prop, pclk_name) {
		pclk = clk_get(tspi->dev, pclk_name);
		if (IS_ERR(pclk))
			continue;

		ret = clk_set_parent(tspi->clk, pclk);
		if (ret < 0) {
			dev_warn(tspi->dev,
				"Error in setting parent clk src %s: %d\n",
				pclk_name, ret);
			continue;
		}

		new_rate = clk_round_rate(tspi->clk, rate);
		if (new_rate < 0)
			continue;

		err_rate = abs(new_rate - rate);
		if (err_rate < fin_err) {
			fpclk = pclk;
			fin_err = err_rate;
			fpclk_name = pclk_name;
		}
	}

	if (fpclk) {
		dev_dbg(tspi->dev, "Setting clk_src %s\n",
				fpclk_name);
		clk_set_parent(tspi->clk, fpclk);
	}
}

static int tegra_spi_start_transfer_one(struct spi_device *spi,
		struct spi_transfer *t, bool is_first_of_msg,
		bool is_single_xfer)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	u32 speed;
	u32 core_speed;
	u8 bits_per_word;
	unsigned total_fifo_words;
	int ret;
	unsigned long command1;
	int req_mode;

	bits_per_word = t->bits_per_word;
	speed = t->speed_hz ? t->speed_hz : spi->max_speed_hz;
	/* Set slave controller clk 1.5 times the bus frequency */
	if (!speed)
		speed = tspi->spi_max_frequency;

	if (tspi->chip_data->new_features) {
		/* In case of new feature, all DMA interfaces are async.
		 * so only 1.5x freq ration required */
		core_speed = (speed * 3) >> 1;
	} else {
		/* To maintain min 1.5x and max 4x ratio between
		 * slave core clk and interface clk */
		core_speed = speed * 4;
	}
	if (core_speed > tspi->spi_max_frequency)
		core_speed = tspi->spi_max_frequency;

	if (core_speed < ((speed * 3) >> 1)) {
		dev_err(tspi->dev, "Cannot set requested clk freq %d\n", speed);
		return -EINVAL;
	}

	if (core_speed != tspi->cur_speed) {
		set_best_clk_source(spi, core_speed);
		ret = clk_set_rate(tspi->clk, core_speed);
		if (ret) {
			dev_err(tspi->dev, "Failed to set clk freq %d\n", ret);
			return -EINVAL;
		}
		tspi->cur_speed = core_speed;
	}

	tspi->cur_spi = spi;
	tspi->curr_xfer = t;
	tspi->curr_rx_pos = 0;
	tspi->curr_pos = 0;
	tspi->tx_status = 0;
	tspi->rx_status = 0;
	total_fifo_words = tegra_spi_calculate_curr_xfer_param(spi, tspi, t);

	ret = tegra_spi_validate_request(spi, tspi, t);
	if (ret < 0)
		return ret;

	if (is_first_of_msg) {
		tegra_spi_clear_status(tspi);

		command1 = tspi->def_command1_reg;
		command1 |= SPI_BIT_LENGTH(bits_per_word - 1);

		command1 &= ~SPI_CONTROL_MODE_MASK;
		req_mode = spi->mode & 0x3;
		if (req_mode == SPI_MODE_0)
			command1 |= SPI_CONTROL_MODE_0;
		else if (req_mode == SPI_MODE_1)
			command1 |= SPI_CONTROL_MODE_1;
		else if (req_mode == SPI_MODE_2)
			command1 |= SPI_CONTROL_MODE_2;
		else if (req_mode == SPI_MODE_3)
			command1 |= SPI_CONTROL_MODE_3;

		tegra_spi_writel(tspi, command1, SPI_COMMAND1);
		tspi->variable_length_transfer = false;
		if (cdata && cdata->variable_length_transfer)
			tspi->variable_length_transfer = true;
	} else {
		command1 = tspi->command1_reg;
		command1 &= ~SPI_BIT_LENGTH(~0);
		command1 |= SPI_BIT_LENGTH(bits_per_word - 1);
	}

	if (spi->mode & SPI_LSB_FIRST)
		command1 |= SPI_LSBIT_FE;
	else
		command1 &= ~SPI_LSBIT_FE;

	if (tspi->is_packed)
		command1 |= SPI_PACKED;

	command1 &= ~(SPI_CS_SEL_MASK | SPI_TX_EN | SPI_RX_EN);
	tspi->cur_direction = 0;
	if (t->rx_buf) {
		command1 |= SPI_RX_EN;
		tspi->cur_direction |= DATA_DIR_RX;
	}
	if (t->tx_buf) {
		command1 |= SPI_TX_EN;
		tspi->cur_direction |= DATA_DIR_TX;
	}
	command1 |= SPI_CS_SEL(spi->chip_select);
	tegra_spi_writel(tspi, command1, SPI_COMMAND1);
	tspi->command1_reg = command1;

	dev_dbg(tspi->dev, "The def 0x%x and written 0x%lx\n",
				tspi->def_command1_reg, command1);

	if (total_fifo_words > SPI_FIFO_DEPTH)
		ret = tegra_spi_start_dma_based_transfer(tspi, t);
	else
		ret = tegra_spi_start_cpu_based_transfer(tspi, t);

	tegra_spi_slave_ready(tspi);

	/* Inform client that we are ready now. */
	if (tspi->spi_slave_ready_callback)
		tspi->spi_slave_ready_callback(tspi->client_data);

	return ret;
}

static struct tegra_spi_device_controller_data
	*tegra_spi_get_cdata_dt(struct spi_device *spi,
			struct tegra_spi_data *tspi)
{
	struct tegra_spi_device_controller_data *cdata;
	struct device_node *slave_np, *data_np;
	int ret;

	slave_np = spi->dev.of_node;
	if (!slave_np) {
		dev_dbg(&spi->dev, "device node not found\n");
		return NULL;
	}

	data_np = of_get_child_by_name(slave_np, "controller-data");
	if (!data_np) {
		dev_dbg(&spi->dev, "child node 'controller-data' not found\n");
		return NULL;
	}

	cdata = &tspi->cdata[spi->chip_select];
	memset(cdata, 0, sizeof(*cdata));

	ret = of_property_read_bool(data_np, "nvidia,variable-length-transfer");
	if (ret)
		cdata->variable_length_transfer = 1;

	of_node_put(data_np);
	return cdata;
}

static int tegra_spi_setup(struct spi_device *spi)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	unsigned long val;
	unsigned long flags;
	int ret;
	unsigned int cs_pol_bit[MAX_CHIP_SELECT] = {
			SPI_CS_POL_INACTIVE_0,
			SPI_CS_POL_INACTIVE_1,
			SPI_CS_POL_INACTIVE_2,
			SPI_CS_POL_INACTIVE_3,
	};

	dev_dbg(&spi->dev, "setup %d bpw, %scpol, %scpha, %dHz\n",
		spi->bits_per_word,
		spi->mode & SPI_CPOL ? "" : "~",
		spi->mode & SPI_CPHA ? "" : "~",
		spi->max_speed_hz);

	if (!cdata) {
		cdata = tegra_spi_get_cdata_dt(spi, tspi);
		spi->controller_data = cdata;
	}

	/* Set speed to the spi max fequency if spi device has not set */
	spi->max_speed_hz = spi->max_speed_hz ? : tspi->spi_max_frequency;

	ret = pm_runtime_get_sync(tspi->dev);
	if (ret < 0) {
		dev_err(tspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}

	spin_lock_irqsave(&tspi->lock, flags);
	val = tspi->def_command1_reg;
	if (spi->mode & SPI_CS_HIGH)
		val &= ~cs_pol_bit[spi->chip_select];
	else
		val |= cs_pol_bit[spi->chip_select];

	tspi->def_command1_reg = val;
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	spin_unlock_irqrestore(&tspi->lock, flags);

	pm_runtime_put(tspi->dev);
	return 0;
}

static void handle_cpu_based_err_xfer(struct tegra_spi_data *tspi)
{
	if (tspi->tx_status ||  tspi->rx_status) {
		dump_regs(err_ratelimited, tspi,
			"cpu-xfer-err [status:%08x]", tspi->status_reg);
		tspi->reset_ctrl_status = true;
	}

}

static void handle_dma_based_err_xfer(struct tegra_spi_data *tspi)
{
	int err = 0;

	/* Abort dmas if any error */
	if (tspi->cur_direction & DATA_DIR_TX) {
		if (tspi->tx_status) {
			dmaengine_terminate_all(tspi->tx_dma_chan);
			dump_regs(err_ratelimited, tspi,
				"tx-dma-err [status:%08x]", tspi->status_reg);
			err += 1;
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		if (tspi->rx_status) {
			dmaengine_terminate_all(tspi->rx_dma_chan);
			dump_regs(err_ratelimited, tspi,
				"rx-dma-err [status:%08x]", tspi->status_reg);
			err += 2;
		}
	}

	if (err)
		tspi->reset_ctrl_status = true;
}


static int tegra_spi_wait_on_message_xfer(struct tegra_spi_data *tspi)
{
	long ret;
	unsigned long timeout;

	/* delay_usecs is used as timeout for slave, value of 0 is inf wait.
	 * since delay_usecs will be too small for practical use (65ms max)
	 * a multiplier of 1000 is used to make as msecs.
	 */
	timeout = tspi->curr_xfer->delay_usecs ?
			usecs_to_jiffies(tspi->curr_xfer->delay_usecs *
				DELAY_MUL_FACTOR) : MAX_SCHEDULE_TIMEOUT;
	/* wait for spi isr */
	ret = wait_for_completion_interruptible_timeout(
				&tspi->xfer_completion, timeout);
	if (ret <= 0) {
		/* interrupted-wait/late interrupt is considered spurious */
		atomic_set(&tspi->isr_expected, 0);
		if (ret == -ERESTARTSYS) {
			flush_signals(current);
			dev_warn(tspi->dev, "waiting for master was interrupted\n");
		}
		if (ret == 0)
			dev_info(tspi->dev, "Timeout waiting for master\n");
		dump_regs(dbg, tspi, "spi-int-timeout [ret:%ld]", ret);
		if (tspi->is_curr_dma_xfer &&
				(tspi->cur_direction & DATA_DIR_TX))
			dmaengine_terminate_all(tspi->tx_dma_chan);
		if (tspi->is_curr_dma_xfer &&
				(tspi->cur_direction & DATA_DIR_RX))
			dmaengine_terminate_all(tspi->rx_dma_chan);

		/* resetting controller to unarm spi */
		tspi->reset_ctrl_status = true;
		ret = -EIO;
		return ret;
	}
	/* interrupt came and status in tspi->status_reg */
	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->tx_status = tspi->status_reg &
					(SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF);

	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->rx_status = tspi->status_reg &
					(SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF);

	/* workaround when number of packets is 64K. The transfer status
	 * field is 16bit and zero based count (0...2^16-1) and hence for 64K
	 * the counter resets to 0. Hence for a transfer, if count is
	 * reported as zero, expected is 64K and there is no error, then
	 * we correct it to 64K. Packets greater than 64K is not allowed.
	 */
	tspi->words_to_transfer =
		(tspi->words_to_transfer == 0 &&
			tspi->curr_xfer->len == MAX_PACKETS) ?
			MAX_PACKETS : tspi->words_to_transfer;

	/* check if required bytes where transferred */
	if (!tspi->variable_length_transfer &&
		tspi->words_to_transfer !=
			tspi->curr_xfer->len/tspi->bytes_per_word) {
		dump_regs(err_ratelimited, tspi,
				"transferred[%d] != requested[%d]",
				tspi->words_to_transfer,
				tspi->curr_xfer->len/tspi->bytes_per_word);
		tspi->tx_status = SPI_TX_FIFO_UNF;
		tspi->rx_status = SPI_RX_FIFO_OVF;
	}

	/* cs inactive intr while it is masked, mark as error */
	if (tspi->chip_data->mask_cs_inactive_intr &&
			(tspi->status_reg & SPI_CS_INACTIVE)) {
		dev_dbg(tspi->dev, "cs inactive intr, status_reg = 0x%x\n",
				tspi->status_reg);
		tspi->tx_status = SPI_TX_FIFO_UNF;
		tspi->rx_status = SPI_RX_FIFO_OVF;
	}

	if (!tspi->is_curr_dma_xfer)
		handle_cpu_based_err_xfer(tspi);
	else
		handle_dma_based_err_xfer(tspi);

	/* truncate in case we received more than requested */
	tspi->words_to_transfer = min(tspi->words_to_transfer,
				tspi->curr_xfer->len/tspi->bytes_per_word);

	return (tspi->tx_status || tspi->rx_status) ? -EIO : 0;
}

static int tegra_spi_handle_message(struct tegra_spi_data *tspi,
		struct spi_transfer *xfer)
{
	int ret = 0;
	long wait_status;
	unsigned pending_rx_word_count;
	unsigned long fifo_status;
	unsigned long word_tfr_status;
	unsigned long actual_words_xferrd;

	/* we are here, so no io-error and received the expected num bytes
	 * expect in variable length case where received <= expected
	 */
	if (!tspi->is_curr_dma_xfer) {
		if (tspi->cur_direction & DATA_DIR_RX)
			tegra_spi_read_rx_fifo_to_client_rxbuf(tspi, xfer);

	} else {
		if (tspi->cur_direction & DATA_DIR_TX) {
			if ((IS_SPI_CS_INACTIVE(tspi->status_reg)) &&
				tspi->variable_length_transfer) {
				/* Check for TX DMA completion. If the TX DMA
				 * is already completed, No need to terminate
				 * the DMA xfer.
				 */
				wait_status = try_wait_for_completion(
							&tspi->tx_dma_complete);
				if (!wait_status) {
					dev_dbg(tspi->dev,
						"terminating tx dma\n");
					dmaengine_terminate_all(
							tspi->tx_dma_chan);
					async_tx_ack(tspi->tx_dma_desc);
				} else
					dev_dbg(tspi->dev,
						"received tx dma completion\n");

				tspi->reset_ctrl_status = true;
			} else {
				wait_status =
				      wait_for_completion_interruptible_timeout(
						&tspi->tx_dma_complete,
						SPI_DMA_TIMEOUT);
				if (wait_status <= 0) {
					dmaengine_terminate_all(
							tspi->tx_dma_chan);
					dump_regs(dbg, tspi,
						"tx-dma-timeout [wait:%ld]",
						wait_status);
					tspi->reset_ctrl_status = true;
					ret = -EIO;
					return ret;
				}
			}
		}
		if (tspi->cur_direction & DATA_DIR_RX) {
			if (tspi->variable_length_transfer) {
				dmaengine_terminate_all(tspi->rx_dma_chan);
				async_tx_ack(tspi->rx_dma_desc);

				fifo_status =
					tegra_spi_readl(tspi, SPI_FIFO_STATUS);

				pending_rx_word_count =
					SPI_RX_FIFO_FULL_COUNT(fifo_status);

				word_tfr_status =
					DIV_ROUND_UP(tspi->words_to_transfer ,
						tspi->words_per_32bit);

				actual_words_xferrd =
					word_tfr_status - pending_rx_word_count;

				while (pending_rx_word_count > 0) {

					tspi->rx_dma_buf[actual_words_xferrd] =
						tegra_spi_readl(tspi,
							SPI_RX_FIFO);

					actual_words_xferrd++;
					pending_rx_word_count--;
				}
			} else {
				wait_status =
				      wait_for_completion_interruptible_timeout(
						&tspi->rx_dma_complete,
						SPI_DMA_TIMEOUT);
				if (wait_status <= 0) {
					dmaengine_terminate_all(
							tspi->rx_dma_chan);
					dump_regs(dbg, tspi,
						"rx-dma-timeout [wait:%ld]",
						wait_status);
					tspi->reset_ctrl_status = true;
					ret = -EIO;
					return ret;
				}
			}
			tegra_spi_copy_spi_rxbuf_to_client_rxbuf(tspi, xfer);
		}
	}

	/* For TX direction, It might be possible that CS deassert happens
	 * before the tranfer completion(variable length case). In this scenario
	 * actual transferred data is less than the requested data. So to update
	 * the curr_pos(and finally xfer->len) read the data transfer count from
	 * SPI_TRANS_STATUS and update the xfer->len accordingly.
	 *
	 * In case of RX direction, tspi->curr_rx_pos contains actual received
	 * data. We can directly use it to update the xfer->len.
	 */
	if (tspi->cur_direction & DATA_DIR_TX) {
		tspi->curr_pos += tspi->words_to_transfer * tspi->bytes_per_word;
		tspi->rem_len -= tspi->curr_pos;
	}
	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->curr_pos = tspi->curr_rx_pos;

	/* If external master de-asserts the CS randomly
	and after that is SW wants to do a fresh transfer,
	SW needs to apply controller reset to flush the fifos
	and internal pipeline as controller might have
	fetched more data from memory by then. */
	if (tspi->curr_pos == xfer->len ||
				(IS_SPI_CS_INACTIVE(tspi->status_reg) &&
				tspi->variable_length_transfer)) {
		xfer->len = tspi->curr_pos;
		if ((IS_SPI_CS_INACTIVE(tspi->status_reg))
			&& tspi->variable_length_transfer
			&& tspi->chip_data->new_features
			&& (tspi->cur_direction & DATA_DIR_TX)) {
				tspi->reset_ctrl_status = true;
		}
	}

	return ret;
}

static int tegra_spi_transfer_one_message(struct spi_master *master,
			struct spi_message *msg)
{
	bool is_first_msg = true;
	int single_xfer;
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);
	struct spi_transfer *xfer;
	struct spi_device *spi = msg->spi;
	int ret;

	msg->status = 0;
	msg->actual_length = 0;

	ret = pm_runtime_get_sync(tspi->dev);
	if (ret < 0) {
		dev_err(tspi->dev, "runtime PM get failed: %d\n", ret);
		msg->status = ret;
		spi_finalize_current_message(master);
		return ret;
	}
	/* allow SIGINT for interrupting the sleep */
	allow_signal(SIGINT);

	/* Fix me - Implement proper sequnce for CDC */
	tegra_spi_ext_clk_enable(false, tspi);
	reset_controller(tspi);
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);

	single_xfer = list_is_singular(&msg->transfers);
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		reinit_completion(&tspi->xfer_completion);
		tspi->rem_len = xfer->len;
		tspi->words_to_transfer = 0;
		dump_xfer(tspi, xfer);
		/* program hardware */
		ret = tegra_spi_start_transfer_one(spi, xfer,
					is_first_msg, single_xfer);
		if (ret < 0) {
			dev_err(tspi->dev,
				"spi can not start transfer, err %d\n", ret);
			goto exit;
		}
		is_first_msg = false;
		/* wait for spi-interrupt and handle transfer error */
		ret = tegra_spi_wait_on_message_xfer(tspi);
		if (ret) {
			tegra_spi_slave_busy(tspi);
			goto exit;
		}

		/* unpack and copy data to client */
		ret = tegra_spi_handle_message(tspi, xfer);
		if (ret < 0)
			goto exit;

		if (tspi->variable_length_transfer)
			msg->actual_length += xfer->len;
		else
			msg->actual_length += (xfer->len - tspi->rem_len);

#ifdef PROFILE_SPI_SLAVE
		{
			ktime_t time;
			tspi->end_time = ktime_get();
			tspi->profile_size = xfer->len - tspi->rem_len;
			time = ktime_sub(tspi->end_time, tspi->start_time);
			dev_dbg(tspi->dev,
				"Profile: size=%dB time-from-spi-int=%lldns\n",
					tspi->profile_size, time.tv64);
		}
#endif
		if (tspi->rem_len)
			dump_sw_state(tspi,
				"Error transfer was truncated [rem:%d]",
				tspi->rem_len);
		/* checks for any error after s/w handling the transfer */
		ret = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
		if (ret &
			(SPI_FRAME_END | SPI_CS_INACTIVE | SPI_ERR |
				SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF |
				SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF))
			dump_regs(dbg, tspi, "sw-handle-error");

		if (!(ret & SPI_FIFO_EMPTY))
			dump_regs(dbg, tspi, "rx/tx fifo are not empty");
	}
	ret = 0;
exit:
	pm_runtime_put(tspi->dev);
	msg->status = ret;
	spi_finalize_current_message(master);
	return ret;
}

static irqreturn_t tegra_spi_isr(int irq, void *context_data)
{
	struct tegra_spi_data *tspi = context_data;
	unsigned long status_reg;

	status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	dump_regs(dbg, tspi, "@isr");

	tegra_spi_clear_status(tspi);

	/* if isr was not expected */
	if (!atomic_xchg(&tspi->isr_expected, 0)) {
		dev_err_ratelimited(tspi->dev,
				"spurious interrupt, status_reg = 0x%x\n",
				tspi->status_reg);
		return IRQ_NONE;
	}

#ifdef PROFILE_SPI_SLAVE
	tspi->start_time = ktime_get();
#endif
	tspi->status_reg = status_reg;
	tspi->words_to_transfer = SPI_BLK_CNT(
				tegra_spi_readl(tspi, SPI_TRANS_STATUS));

	tegra_spi_slave_busy(tspi);

	/* Inform client about controller interrupt. */
	if (tspi->spi_slave_isr_callback)
		tspi->spi_slave_isr_callback(tspi->client_data);

	complete(&tspi->xfer_completion);
	return IRQ_HANDLED;
}

static struct tegra_spi_platform_data *tegra_spi_parse_dt(
		struct platform_device *pdev)
{
	struct tegra_spi_platform_data *pdata;
	const unsigned int *prop;
	struct device_node *np = pdev->dev.of_node;
	enum of_gpio_flags gpio_flags;
	u32 of_dma[2];

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Memory alloc for pdata failed\n");
		return NULL;
	}

	if (of_property_read_u32_array(np, "nvidia,dma-request-selector",
				of_dma, 2) >= 0)
		pdata->dma_req_sel = of_dma[1];

	prop = of_get_property(np, "spi-max-frequency", NULL);
	if (prop)
		pdata->spi_max_frequency = be32_to_cpup(prop);

	prop = of_get_property(np, "nvidia,rx-trigger-words", NULL);
	if (prop)
		pdata->rx_trig_words = be32_to_cpup(prop);

	prop = of_get_property(np, "nvidia,least-significant-bit", NULL);
	if (prop)
		pdata->ls_bit = (be32_to_cpup(prop)) & 0x1;

	if (of_find_property(np, "nvidia,clock-always-on", NULL))
		pdata->is_clkon_always = true;

	of_property_read_string(np, "nvidia,clk-pin", &pdata->clk_pin);

	pdata->gpio_slave_ready =
		of_get_named_gpio_flags(np, "nvidia,slave-ready-gpio", 0,
				&gpio_flags);

	if (gpio_flags & OF_GPIO_ACTIVE_LOW)
		pdata->slave_ready_active_high = false;
	else
		pdata->slave_ready_active_high = true;

	return pdata;
}

/* Work around for HW bug 1317495  */
static struct tegra_spi_chip_data tegra124_spi_chip_data = {
	.intr_mask_reg = false,
	.mask_cs_inactive_intr = true,
	.new_features = false,
};

static struct tegra_spi_chip_data tegra210_spi_chip_data = {
	.intr_mask_reg = true,
	.mask_cs_inactive_intr = false,
	.new_features = false,
};

static struct tegra_spi_chip_data tegra186_spi_chip_data = {
	.intr_mask_reg = true,
	.mask_cs_inactive_intr = false,
	.new_features = true,
};

static struct of_device_id tegra_spi_of_match[] = {
	{
		.compatible = "nvidia,tegra124-spi-slave",
		.data       = &tegra124_spi_chip_data,
	}, {
		.compatible = "nvidia,tegra210-spi-slave",
		.data       = &tegra210_spi_chip_data,
	}, {
		.compatible = "nvidia,tegra186-spi-slave",
		.data       = &tegra186_spi_chip_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_spi_of_match);

static int tegra_spi_probe(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct tegra_spi_data	*tspi;
	struct resource		*r;
	struct tegra_spi_platform_data *pdata = pdev->dev.platform_data;
	struct device_node *np;
	const struct of_device_id *match;
	const struct tegra_spi_chip_data *chip_data = &tegra124_spi_chip_data;
	int ret, spi_irq;
	int bus_num;
	int deassert_val;

	if (pdev->dev.of_node) {
		bus_num = of_alias_get_id(pdev->dev.of_node, "spi");
		if (bus_num < 0) {
			dev_warn(&pdev->dev,
				"Dynamic bus number will be registerd\n");
			bus_num = -1;
		}
	} else {
		bus_num = pdev->id;
	}

	if (!pdata && pdev->dev.of_node)
		pdata = tegra_spi_parse_dt(pdev);

	if (!pdata) {
		dev_err(&pdev->dev, "No platform data, exiting\n");
		return -ENODEV;
	}

	if (!pdata->spi_max_frequency)
		pdata->spi_max_frequency = 25000000; /* 25MHz */

	if (pdata->rx_trig_words != 0 &&
		 pdata->rx_trig_words != 4 && pdata->rx_trig_words != 8)
		pdata->rx_trig_words = 0;

	master = spi_alloc_master(&pdev->dev, sizeof(*tspi));
	if (!master) {
		dev_err(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST;
	/* supported bpw 8-32 */
	master->bits_per_word_mask = (u32) ~(BIT(0)|BIT(1)|
					BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6));
	master->setup = tegra_spi_setup;
	master->transfer_one_message = tegra_spi_transfer_one_message;
	master->num_chipselect = MAX_CHIP_SELECT;
	master->bus_num = bus_num;

	dev_set_drvdata(&pdev->dev, master);
	tspi = spi_master_get_devdata(master);
	tspi->master = master;
	tspi->dma_req_sel = pdata->dma_req_sel;
	tspi->clock_always_on = pdata->is_clkon_always;
	tspi->rx_trig_words = pdata->rx_trig_words;
	tspi->dev = &pdev->dev;
	dev_dbg(&pdev->dev, "rx-trigger:%d clock-always-on:%d dma-req:%d\n",
				tspi->rx_trig_words, tspi->clock_always_on,
				tspi->dma_req_sel);

	if (pdev->dev.of_node) {
		match = of_match_device(tegra_spi_of_match,
				&pdev->dev);
		if (match)
			chip_data = match->data;
	}
	tspi->chip_data = chip_data;
	tspi->clk_pin = (char *)pdata->clk_pin;
	if (!tspi->chip_data->new_features) {
		if (tspi->clk_pin) {
			tspi->clk_pin_state_enabled = true;
			np = of_find_node_by_name(NULL, "pinmux");
			if (!np) {
				ret = -EINVAL;
				dev_err(&pdev->dev, "no pinmux found\n");
				goto exit_free_master;
			}
			tspi->pctl_dev = pinctrl_get_dev_from_of_node(np);
			if (!tspi->pctl_dev) {
				ret = -EINVAL;
				dev_err(&pdev->dev, "invalid pinctrl\n");
				goto exit_free_master;
			}
			tspi->clk_pin_group = pinctrl_get_group_from_group_name
				(tspi->pctl_dev, tspi->clk_pin);

			if (tspi->clk_pin_group < 0) {
				ret = tspi->clk_pin_group;
				dev_err(&pdev->dev, "invalid clk_pin group\n");
				goto exit_free_master;
			}
		} else {
			ret = -EINVAL;
			dev_err(&pdev->dev,
			     "Pin group name for clock line is not defined.\n");
			goto exit_free_master;
		}
	}

	tspi->gpio_slave_ready = pdata->gpio_slave_ready;

	if (gpio_is_valid(tspi->gpio_slave_ready))
		if (gpio_cansleep(tspi->gpio_slave_ready)) {
			dev_err(&pdev->dev, "Slave Ready GPIO %d is unusable as it can sleep\n",
					tspi->gpio_slave_ready);
			ret = -EINVAL;
			goto exit_free_master;
		}

	tspi->slave_ready_active_high = pdata->slave_ready_active_high;

	if (gpio_is_valid(tspi->gpio_slave_ready)) {
		ret = gpio_request(tspi->gpio_slave_ready,
				"gpio-spi-slave-ready");
		if (!ret) {
			if (tspi->slave_ready_active_high)
				deassert_val = 0;
			else
				deassert_val = 1;

			gpio_direction_output(tspi->gpio_slave_ready,
					deassert_val);
		} else {
			dev_err(&pdev->dev, "Slave Ready GPIO %d is busy\n",
					tspi->gpio_slave_ready);
			goto exit_free_master;
		}
	}


	spin_lock_init(&tspi->lock);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "No IO memory resource\n");
		ret = -ENODEV;
		goto exit_free_master;
	}
	tspi->phys = r->start;
	tspi->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(tspi->base)) {
		dev_err(&pdev->dev,
			"Cannot request memregion/iomap dma address\n");
		ret = PTR_ERR(tspi->base);
		goto exit_free_master;
	}

	tspi->rstc = devm_reset_control_get(&pdev->dev, "spi");
	if (IS_ERR(tspi->rstc)) {
		dev_err(&pdev->dev, "Reset control is not found\n");
		return PTR_ERR(tspi->rstc);
	}

	atomic_set(&tspi->isr_expected, 0);
	spi_irq = platform_get_irq(pdev, 0);
	tspi->irq = spi_irq;
	ret = request_irq(tspi->irq, tegra_spi_isr, 0,
			dev_name(&pdev->dev), tspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
					tspi->irq);
		goto exit_free_master;
	}

	tspi->clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR(tspi->clk)) {
		dev_err(&pdev->dev, "can not get clock\n");
		ret = PTR_ERR(tspi->clk);
		goto exit_free_irq;
	}

	tspi->max_buf_size = SPI_FIFO_DEPTH << 2;
	tspi->dma_buf_size = DEFAULT_SPI_DMA_BUF_LEN;
	tspi->spi_max_frequency = pdata->spi_max_frequency;

	if (pdata->dma_req_sel) {
		ret = tegra_spi_init_dma_param(tspi, true);
		if (ret < 0) {
			if (ret == -EPROBE_DEFER)
				/* GPCDMA is not available at this point.
				 * probe will be deferred */
				dev_info(&pdev->dev,
					"Probe Deferred, ret %d\n", ret);
			else
				dev_err(&pdev->dev,
					"RxDma Init failed, err %d\n", ret);

			goto exit_free_irq;
		}

		ret = tegra_spi_init_dma_param(tspi, false);
		if (ret < 0) {
			dev_err(&pdev->dev, "TxDma Init failed, err %d\n", ret);
			goto exit_rx_dma_free;
		}
		tspi->max_buf_size = tspi->dma_buf_size;
		init_completion(&tspi->tx_dma_complete);
		init_completion(&tspi->rx_dma_complete);
	}

	init_completion(&tspi->xfer_completion);

	if (tspi->clock_always_on) {
		ret = clk_prepare_enable(tspi->clk);
		if (ret < 0) {
			dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
			goto exit_deinit_dma;
		}
	}
	tspi->reset_ctrl_status = false;
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra_spi_runtime_resume(&pdev->dev);
		if (ret)
			goto exit_pm_disable;
	}

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "pm runtime get failed, e = %d\n", ret);
		goto exit_pm_disable;
	}
	reset_control_reset(tspi->rstc);
	tegra_spi_ext_clk_enable(false, tspi);
	tspi->def_command1_reg  = SPI_CS_SW_HW | SPI_CS_POL_INACTIVE_0 |
		SPI_CS_POL_INACTIVE_1 | SPI_CS_POL_INACTIVE_2 |
		SPI_CS_POL_INACTIVE_3 | SPI_CS_SS_VAL | SPI_CONTROL_MODE_1;

	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	/* set CS inactive b/w packets to mask CS_INACTIVE interrupts */
	if (tspi->chip_data->mask_cs_inactive_intr)
		tegra_spi_writel(tspi, 0, SPI_CS_TIMING2);
	pm_runtime_put(&pdev->dev);

	master->dev.of_node = pdev->dev.of_node;
	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not register to master err %d\n", ret);
		goto exit_pm_disable;
	}

#ifdef TEGRA_SPI_SLAVE_DEBUG
	ret = device_create_file(&pdev->dev, &dev_attr_force_unpacked_mode);
	if (ret != 0)
		goto exit_unregister_master;

	return ret;

exit_unregister_master:
	spi_unregister_master(master);

#endif
	return ret;

exit_pm_disable:
	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_spi_runtime_suspend(&pdev->dev);
	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);
exit_deinit_dma:
	tegra_spi_deinit_dma_param(tspi, false);
exit_rx_dma_free:
	tegra_spi_deinit_dma_param(tspi, true);
exit_free_irq:
	free_irq(spi_irq, tspi);
exit_free_master:
	spi_master_put(master);
	return ret;
}

static int tegra_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = dev_get_drvdata(&pdev->dev);
	struct tegra_spi_data	*tspi = spi_master_get_devdata(master);

#ifdef TEGRA_SPI_SLAVE_DEBUG
	device_remove_file(&pdev->dev, &dev_attr_force_unpacked_mode);
#endif
	free_irq(tspi->irq, tspi);
	spi_unregister_master(master);

	if (tspi->tx_dma_chan)
		tegra_spi_deinit_dma_param(tspi, false);

	if (tspi->rx_dma_chan)
		tegra_spi_deinit_dma_param(tspi, true);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_spi_runtime_suspend(&pdev->dev);

	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_spi_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);
	int ret;

	ret = spi_master_suspend(master);

	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);

	return ret;
}

static int tegra_spi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);
	int ret;

	if (tspi->clock_always_on) {
		ret = clk_prepare_enable(tspi->clk);
		if (ret < 0) {
			dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
			return ret;
		}
	}

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}
	tegra_spi_writel(tspi, tspi->command1_reg, SPI_COMMAND1);
	pm_runtime_put(dev);

	return spi_master_resume(master);
}
#endif

static int tegra_spi_runtime_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);

	/* Flush all write which are in PPSB queue by reading back */
	tegra_spi_readl(tspi, SPI_COMMAND1);

	clk_disable_unprepare(tspi->clk);
	return 0;
}

static int tegra_spi_runtime_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);
	int ret;

	ret = clk_prepare_enable(tspi->clk);
	if (ret < 0) {
		dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static const struct dev_pm_ops tegra_spi_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_spi_runtime_suspend,
		tegra_spi_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra_spi_suspend, tegra_spi_resume)
};
static struct platform_driver tegra_spi_driver = {
	.driver = {
		.name		= "spi-tegra124-slave",
		.owner		= THIS_MODULE,
		.pm		= &tegra_spi_pm_ops,
		.of_match_table	= of_match_ptr(tegra_spi_of_match),
	},
	.probe =	tegra_spi_probe,
	.remove =	tegra_spi_remove,
};
module_platform_driver(tegra_spi_driver);

MODULE_ALIAS("platform:spi-tegra124");
MODULE_DESCRIPTION("NVIDIA Tegra124 SPI Controller Driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com> Yousuf A <yousufa@nvidia.com>");
MODULE_LICENSE("GPL v2");
