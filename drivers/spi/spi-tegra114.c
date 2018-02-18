/*
 * SPI driver for NVIDIA's Tegra114 SPI Controller.
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
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-tegra.h>
#include <linux/clk/tegra.h>
#include <linux/reset.h>
#include <linux/tegra_prod.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

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
#define SPI_CONTROL_MODE_0			(0 << 28)
#define SPI_CONTROL_MODE_1			(1 << 28)
#define SPI_CONTROL_MODE_2			(2 << 28)
#define SPI_CONTROL_MODE_3			(3 << 28)
#define SPI_CONTROL_MODE_MASK			(3 << 28)
#define SPI_MODE_SEL(x)				(((x) & 0x3) << 28)
#define SPI_MODE_VAL(x)				(((x) >> 28) & 0x3)
#define SPI_M_S					(1 << 30)
#define SPI_PIO					(1 << 31)

#define SPI_COMMAND2				0x004
#define SPI_TX_TAP_DELAY(x)			(((x) & 0x3F) << 6)
#define SPI_RX_TAP_DELAY(x)			(((x) & 0x3F) << 0)

#define SPI_CS_TIMING1				0x008
#define SPI_SETUP_HOLD(setup, hold)		(((setup) << 4) | (hold))
#define SPI_CS_SETUP_HOLD(reg, cs, val)			\
		((((val) & 0xFFu) << ((cs) * 8)) |	\
		((reg) & ~(0xFFu << ((cs) * 8))))

#define SPI_CS_TIMING2				0x00C
#define CYCLES_BETWEEN_PACKETS_0(x)		(((x) & 0x1F) << 0)
#define CS_ACTIVE_BETWEEN_PACKETS_0		(1 << 5)
#define CYCLES_BETWEEN_PACKETS_1(x)		(((x) & 0x1F) << 8)
#define CS_ACTIVE_BETWEEN_PACKETS_1		(1 << 13)
#define CYCLES_BETWEEN_PACKETS_2(x)		(((x) & 0x1F) << 16)
#define CS_ACTIVE_BETWEEN_PACKETS_2		(1 << 21)
#define CYCLES_BETWEEN_PACKETS_3(x)		(((x) & 0x1F) << 24)
#define CS_ACTIVE_BETWEEN_PACKETS_3		(1 << 29)
#define SPI_SET_CS_ACTIVE_BETWEEN_PACKETS(reg, cs, val)		\
		(reg = (((val) & 0x1) << ((cs) * 8 + 5)) |	\
			((reg) & ~(1 << ((cs) * 8 + 5))))
#define SPI_SET_CYCLES_BETWEEN_PACKETS(reg, cs, val)		\
		(reg = (((val) & 0x1F) << ((cs) * 8)) |		\
			((reg) & ~(0x1F << ((cs) * 8))))

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
#define SPI_RX_TRIG_1				(0 << 19)
#define SPI_RX_TRIG_4				(1 << 19)
#define SPI_RX_TRIG_8				(2 << 19)
#define SPI_RX_TRIG_16				(3 << 19)
#define SPI_RX_TRIG_MASK			(3 << 19)
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
#define SPI_INTR_RX_FIFO_UNF_MASK		(1 << 25)
#define SPI_INTR_RX_FIFO_OVF_MASK		(1 << 26)
#define SPI_INTR_TX_FIFO_UNF_MASK		(1 << 27)
#define SPI_INTR_TX_FIFO_OVF_MASK		(1 << 28)
#define SPI_INTR_RDY_MASK			(1 << 29)
#define SPI_INTR_ALL_MASK			(0x1fUL << 25)

#define SPI_MISC				0x194
#define SPI_MISC_CLKEN_OVERRIDE			(1 << 31)

#define MAX_CHIP_SELECT				4
#define SPI_FIFO_DEPTH				64
#define DATA_DIR_TX				(1 << 0)
#define DATA_DIR_RX				(1 << 1)

#define SPI_DMA_TIMEOUT				(msecs_to_jiffies(10000))
#define DEFAULT_SPI_DMA_BUF_LEN			(16*1024)
#define TX_FIFO_EMPTY_COUNT_MAX			(0x40)
#define RX_FIFO_FULL_COUNT_ZERO			(0)
#define MAX_HOLD_CYCLES				16
#define SPI_DEFAULT_SPEED			25000000

#define MAX_CHIP_SELECT				4
#define SPI_FIFO_DEPTH				64
#define SPI_FIFO_FLUSH_MAX_DELAY 		2000

#define SPI_SPEED_TAP_DELAY_MARGIN 35000000
#define SPI_DEFAULT_RX_TAP_DELAY 10
#define SPI_POLL_TIMEOUT 10000
#define SPI_AUTOSUSPEND_DELAY 100	/* 100ms */

struct tegra_spi_chip_data {
	bool intr_mask_reg;
	bool set_rx_tap_delay;
	bool slcg_support;
};

struct tegra_spi_data {
	struct device				*dev;
	struct spi_master			*master;
	spinlock_t				lock;
	struct dentry				*debugfs;

	struct clk				*clk;
	struct reset_control			*rstc;
	void __iomem				*base;
	phys_addr_t				phys;
	unsigned				irq;
	bool					clock_always_on;
	bool					polling_mode;
	bool					boost_reg_access;
	bool					runtime_pm;
	u32					spi_max_frequency;
	u32					cur_speed;
	unsigned				min_div;

	struct spi_device			*cur_spi;
	unsigned				cur_pos;
	unsigned				cur_len;
	unsigned				words_per_32bit;
	unsigned				bytes_per_word;
	unsigned				curr_dma_words;
	unsigned				cur_direction;

	unsigned				cur_rx_pos;
	unsigned				cur_tx_pos;

	unsigned				dma_buf_size;
	unsigned				max_buf_size;
	bool					is_curr_dma_xfer;
	bool					is_hw_based_cs;
	bool					transfer_in_progress;

	struct completion			rx_dma_complete;
	struct completion			tx_dma_complete;

	u32					tx_status;
	u32					rx_status;
	u32					status_reg;
	bool					is_packed;
	unsigned long				packed_size;

	u32					command1_reg;
	u32					dma_control_reg;
	u32					def_command1_reg;
	u32					def_command2_reg;
	u32					spi_cs_timing;
	u8					def_chip_select;
	u8					last_used_cs;

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
	bool					cs_gpio_reqstd[MAX_CHIP_SELECT];
	struct tegra_prod_list *prod_list;
	struct pinctrl *pinctrl;
	struct pinctrl_state *enable_interface;
};


static int tegra_spi_runtime_suspend(struct device *dev);
static int tegra_spi_runtime_resume(struct device *dev);
static int tegra_spi_status_poll(struct tegra_spi_data *tspi);
static int tegra_spi_set_clock_rate(struct tegra_spi_data *tspi, u32 speed);


static inline unsigned long tegra_spi_readl(struct tegra_spi_data *tspi,
		unsigned long reg)
{
	return readl(tspi->base + reg);
}

static inline void tegra_spi_writel(struct tegra_spi_data *tspi,
		unsigned long val, unsigned long reg)
{
	/* Read back register to make sure that register writes completed */
	if ((reg == SPI_COMMAND1) && (val & SPI_PIO))
		readl(tspi->base + SPI_COMMAND1);

	writel(val, tspi->base + reg);
}

static inline int tegra_spi_runtime_get(struct tegra_spi_data *tspi)
{
	if (!tspi->runtime_pm)
		return 0;

	return pm_runtime_get_sync(tspi->dev);
}

static inline int tegra_spi_runtime_put(struct tegra_spi_data *tspi)
{
	if (!tspi->runtime_pm)
		return 0;

	pm_runtime_mark_last_busy(tspi->dev);
	return pm_runtime_put_autosuspend(tspi->dev);
}

static void tegra_spi_clear_status(struct tegra_spi_data *tspi)
{
	unsigned long val;

	/* Write 1 to clear status register */
	val = tegra_spi_readl(tspi, SPI_TRANS_STATUS);
	if (val & SPI_RDY)
		tegra_spi_writel(tspi, val, SPI_TRANS_STATUS);

	/* Clear fifo status error if any */
	tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	if (tspi->status_reg & SPI_ERR)
		tegra_spi_writel(tspi, SPI_ERR | SPI_FIFO_ERROR,
				SPI_FIFO_STATUS);
}

static unsigned tegra_spi_calculate_curr_xfer_param(
	struct spi_device *spi, struct tegra_spi_data *tspi,
	struct spi_transfer *t)
{
	unsigned remain_len = t->len - tspi->cur_pos;
	unsigned max_word;
	unsigned bits_per_word ;
	unsigned max_len;
	unsigned total_fifo_words;

	bits_per_word = t->bits_per_word ? t->bits_per_word :
						spi->bits_per_word;
	tspi->bytes_per_word = (bits_per_word - 1) / 8 + 1;
	/*
	 * SPI transfer length should be multiple of SPI word size
	 * where SPI word size should be power-of-two multiple
	 */
	if (tspi->bytes_per_word == 3)
		tspi->bytes_per_word = 4;

	if ((bits_per_word == 8 || bits_per_word == 16) && (t->len > 3)) {
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
	u8 *tx_buf = (u8 *)t->tx_buf + tspi->cur_tx_pos;

	tx_empty_count = TX_FIFO_EMPTY_COUNT_MAX;

	if (tspi->is_packed) {
		fifo_words_left = tx_empty_count * tspi->words_per_32bit;
		written_words = min(fifo_words_left, tspi->curr_dma_words);
		nbytes = written_words * tspi->bytes_per_word;
		max_n_32bit = DIV_ROUND_UP(nbytes, 4);
		for (count = 0; count < max_n_32bit; count++) {
			x = 0;
			for (i = 0; (i < 4) && nbytes; i++, nbytes--)
				x |= (*tx_buf++) << (i*8);
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
			tegra_spi_writel(tspi, x, SPI_TX_FIFO);
		}
	}
	tspi->cur_tx_pos += written_words * tspi->bytes_per_word;
	return written_words;
}

static unsigned int tegra_spi_read_rx_fifo_to_client_rxbuf(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned rx_full_count;
	unsigned long fifo_status;
	unsigned i, count;
	unsigned long x;
	unsigned int read_words = 0;
	unsigned len;
	u8 *rx_buf = (u8 *)t->rx_buf + tspi->cur_rx_pos;

	fifo_status = tspi->status_reg;
	rx_full_count = SPI_RX_FIFO_FULL_COUNT(fifo_status);
	if (tspi->is_packed) {
		len = tspi->curr_dma_words * tspi->bytes_per_word;
		for (count = 0; count < rx_full_count; count++) {
			x = tegra_spi_readl(tspi, SPI_RX_FIFO);
			for (i = 0; len && (i < 4); i++, len--)
				*rx_buf++ = (x >> i*8) & 0xFF;
		}
		tspi->cur_rx_pos += tspi->curr_dma_words * tspi->bytes_per_word;
		read_words += tspi->curr_dma_words;
	} else {
		unsigned int bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
						tspi->cur_spi->bits_per_word;
		for (count = 0; count < rx_full_count; count++) {
			x = tegra_spi_readl(tspi, SPI_RX_FIFO);
			for (i = 0; (i < tspi->bytes_per_word); i++)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
		tspi->cur_rx_pos += rx_full_count * tspi->bytes_per_word;
		read_words += rx_full_count;
	}
	return read_words;
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
		memcpy(tspi->tx_dma_buf, t->tx_buf + tspi->cur_pos, len);
	} else {
		unsigned int i;
		unsigned int count;
		u8 *tx_buf = (u8 *)t->tx_buf + tspi->cur_tx_pos;
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
	tspi->cur_tx_pos += tspi->curr_dma_words * tspi->bytes_per_word;

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(tspi->dev, tspi->tx_dma_phys,
				tspi->dma_buf_size, DMA_TO_DEVICE);
}

static void tegra_spi_copy_spi_rxbuf_to_client_rxbuf(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned len;

	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(tspi->dev, tspi->rx_dma_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);

	if (tspi->is_packed) {
		len = tspi->curr_dma_words * tspi->bytes_per_word;
		memcpy(t->rx_buf + tspi->cur_rx_pos, tspi->rx_dma_buf, len);
	} else {
		unsigned int i;
		unsigned int count;
		unsigned char *rx_buf = t->rx_buf + tspi->cur_rx_pos;
		unsigned int x;
		unsigned int rx_mask, bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
						tspi->cur_spi->bits_per_word;
		rx_mask = (1ULL << bits_per_word) - 1;
		for (count = 0; count < tspi->curr_dma_words; count++) {
			x = tspi->rx_dma_buf[count];
			x &= rx_mask;
			for (i = 0; (i < tspi->bytes_per_word); i++)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
	}
	tspi->cur_rx_pos += tspi->curr_dma_words * tspi->bytes_per_word;

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(tspi->dev, tspi->rx_dma_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);
}

static void tegra_spi_dma_complete(void *args)
{
	struct completion *dma_complete = args;

	complete(dma_complete);
}

static int tegra_spi_start_tx_dma(struct tegra_spi_data *tspi, int len)
{
	reinit_completion(&tspi->tx_dma_complete);
	tspi->tx_dma_desc = dmaengine_prep_slave_single(tspi->tx_dma_chan,
				tspi->tx_dma_phys, len, DMA_MEM_TO_DEV,
				DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!tspi->tx_dma_desc) {
		dev_err(tspi->dev, "Not able to get desc for Tx\n");
		return -EIO;
	}

	tspi->tx_dma_desc->callback = tegra_spi_dma_complete;
	tspi->tx_dma_desc->callback_param = &tspi->tx_dma_complete;

	dmaengine_submit(tspi->tx_dma_desc);
	dma_async_issue_pending(tspi->tx_dma_chan);
	return 0;
}

static int tegra_spi_start_rx_dma(struct tegra_spi_data *tspi, int len)
{
	reinit_completion(&tspi->rx_dma_complete);
	tspi->rx_dma_desc = dmaengine_prep_slave_single(tspi->rx_dma_chan,
				tspi->rx_dma_phys, len, DMA_DEV_TO_MEM,
				DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!tspi->rx_dma_desc) {
		dev_err(tspi->dev, "Not able to get desc for Rx\n");
		return -EIO;
	}

	tspi->rx_dma_desc->callback = tegra_spi_dma_complete;
	tspi->rx_dma_desc->callback_param = &tspi->rx_dma_complete;

	dmaengine_submit(tspi->rx_dma_desc);
	dma_async_issue_pending(tspi->rx_dma_chan);
	return 0;
}

static int check_and_clear_fifo(struct tegra_spi_data *tspi)
{
	unsigned long status = tspi->status_reg;
	int cnt = SPI_FIFO_FLUSH_MAX_DELAY;

	/* Make sure that Rx and Tx fifo are empty */
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

static int tegra_spi_start_dma_based_transfer(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned long val, cmd1;
	unsigned int len;
	unsigned long flags;
	int ret = 0, maxburst;
	struct dma_slave_config dma_sconfig;
	u32 speed;

	/* Make sure that Rx and Tx fifo are empty */
	ret = check_and_clear_fifo(tspi);
	if (ret != 0)
		return ret;

	val = SPI_DMA_BLK_SET(tspi->curr_dma_words - 1);
	tegra_spi_writel(tspi, val, SPI_DMA_BLK);

	if (tspi->is_packed)
		len = DIV_ROUND_UP(tspi->curr_dma_words * tspi->bytes_per_word,
					4) * 4;
	else
		len = tspi->curr_dma_words * 4;

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

	if (!tspi->chip_data->intr_mask_reg) {
		if (!tspi->polling_mode) {
			if (tspi->cur_direction & DATA_DIR_TX)
				val |= SPI_IE_TX;
			if (tspi->cur_direction & DATA_DIR_RX)
				val |= SPI_IE_RX;
		}
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
		dma_sconfig.src_maxburst = maxburst;
		dmaengine_slave_config(tspi->rx_dma_chan, &dma_sconfig);

		ret = tegra_spi_start_rx_dma(tspi, len);
		if (ret < 0) {
			dev_err(tspi->dev,
				"Starting rx dma failed, err %d\n", ret);
			if (tspi->cur_direction & DATA_DIR_TX)
				dmaengine_terminate_all(tspi->tx_dma_chan);
			return ret;
		}
	}

	if (tspi->boost_reg_access) {
		speed = t->speed_hz ? t->speed_hz :
				tspi->cur_spi->max_speed_hz;
		ret = tegra_spi_set_clock_rate(tspi, speed);
		if (ret < 0)
			return ret;
	}

	spin_lock_irqsave(&tspi->lock, flags);
	cmd1 = tspi->command1_reg;
	cmd1 &= ~(SPI_TX_EN | SPI_RX_EN | SPI_BOTH_EN_BIT);
	if ((t->rx_nbits == SPI_NBITS_DUAL) ||
			(t->tx_nbits == SPI_NBITS_DUAL))
		cmd1 |= SPI_BOTH_EN_BIT;
	if (tspi->cur_direction & DATA_DIR_TX)
		cmd1 |= SPI_TX_EN;
	if (tspi->cur_direction & DATA_DIR_RX)
		cmd1 |= SPI_RX_EN;
	tegra_spi_writel(tspi, cmd1, SPI_COMMAND1);

	tspi->is_curr_dma_xfer = true;
	tspi->dma_control_reg = val;

	tspi->transfer_in_progress = true;
	val |= SPI_DMA_EN;
	tegra_spi_writel(tspi, val, SPI_DMA_CTL);
	spin_unlock_irqrestore(&tspi->lock, flags);
	return ret;
}

static int tegra_spi_start_cpu_based_transfer(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned long val;
	unsigned long flags;
	unsigned cur_words;
	int ret;
	u32 speed;

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

	if (!tspi->chip_data->intr_mask_reg) {
		if (!tspi->polling_mode) {
			if (tspi->cur_direction & DATA_DIR_TX)
				val |= SPI_IE_TX;
			if (tspi->cur_direction & DATA_DIR_RX)
				val |= SPI_IE_RX;
		}
		tegra_spi_writel(tspi, val, SPI_DMA_CTL);
	}

	tspi->dma_control_reg = val;

	if (tspi->boost_reg_access) {
		speed = t->speed_hz ? t->speed_hz :
				tspi->cur_spi->max_speed_hz;
		ret = tegra_spi_set_clock_rate(tspi, speed);
		if (ret < 0)
			return ret;
	}

	spin_lock_irqsave(&tspi->lock, flags);
	tspi->is_curr_dma_xfer = false;
	val = tspi->command1_reg;
	val &= ~(SPI_TX_EN | SPI_RX_EN | SPI_BOTH_EN_BIT);
	if ((t->rx_nbits == SPI_NBITS_DUAL) ||
			(t->tx_nbits == SPI_NBITS_DUAL))
		val |= SPI_BOTH_EN_BIT;
	if (tspi->cur_direction & DATA_DIR_TX)
		val |= SPI_TX_EN;
	if (tspi->cur_direction & DATA_DIR_RX)
		val |= SPI_RX_EN;

	tspi->transfer_in_progress = true;
	val |= SPI_PIO;
	tegra_spi_writel(tspi, val, SPI_COMMAND1);
	spin_unlock_irqrestore(&tspi->lock, flags);
	return 0;
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

	if (dma_to_memory) {
		dma_sconfig.src_addr = tspi->phys + SPI_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.src_maxburst = 0;
	} else {
		dma_sconfig.dst_addr = tspi->phys + SPI_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = 0;
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

static void set_best_clk_source(struct tegra_spi_data *tspi,
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

	node = tspi->master->dev.of_node;
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
				"Error in setting parent clk src %s\n",
				pclk_name);
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

static int tegra_spi_set_clock_rate(struct tegra_spi_data *tspi, u32 speed)
{
	int ret;

	if (speed == tspi->cur_speed)
		return 0;
	set_best_clk_source(tspi, speed);
	ret = clk_set_rate(tspi->clk, speed);
	if (ret) {
		dev_err(tspi->dev, "Failed to set clk freq %d\n", ret);
		return -EINVAL;
	}
	tspi->cur_speed = speed;

	return 0;
}

static void tegra_spi_set_cmd2(struct tegra_spi_data *tspi, int chip_select,
				u32 speed)
{
	int ret;
	char prod_name[15];
	u32 command2_reg;

	/* Avoid write to register for transfers to last used device */
	if (tspi->last_used_cs == chip_select)
		return;

	if (tspi->prod_list) {
		sprintf(prod_name, "prod_c_cs%d", chip_select);
		ret = tegra_prod_set_by_name(&tspi->base, prod_name,
				tspi->prod_list);
		ret = tegra_prod_set_by_name(&tspi->base, "prod",
				tspi->prod_list);
	} else {
		command2_reg = tspi->def_command2_reg;
		if (tspi->chip_data->set_rx_tap_delay) {
			if (speed > SPI_SPEED_TAP_DELAY_MARGIN) {
				command2_reg = command2_reg &
					(~SPI_RX_TAP_DELAY(63));
				command2_reg = command2_reg |
					SPI_RX_TAP_DELAY(
					SPI_DEFAULT_RX_TAP_DELAY);
			}
		}
		tegra_spi_writel(tspi, command2_reg, SPI_COMMAND2);
	}
	tspi->last_used_cs = chip_select;
}

static int tegra_spi_start_transfer_one(struct spi_device *spi,
		struct spi_transfer *t, bool is_first_of_msg,
		bool is_single_xfer)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);
	u32 speed;
	u32 spi_cs_timing2 = 0;
	u8 bits_per_word;
	unsigned total_fifo_words;
	int ret;
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	unsigned long command1;
	int req_mode;

	bits_per_word = t->bits_per_word;
	speed = t->speed_hz ? t->speed_hz : spi->max_speed_hz;
		/* set max clock for faster register access */
	if (tspi->boost_reg_access)
		ret = tegra_spi_set_clock_rate(tspi, tspi->spi_max_frequency);
	else
		ret = tegra_spi_set_clock_rate(tspi, speed);
	if (ret < 0)
		return ret;

	tspi->cur_spi = spi;
	tspi->cur_pos = 0;
	tspi->cur_rx_pos = 0;
	tspi->cur_tx_pos = 0;
	tspi->curr_xfer = t;
	tspi->tx_status = 0;
	tspi->rx_status = 0;
	total_fifo_words = tegra_spi_calculate_curr_xfer_param(spi, tspi, t);

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

		/* Apply mode setting before switching chip select */
		if (SPI_MODE_VAL(command1) !=
		    SPI_MODE_VAL(tspi->def_command1_reg))
			tegra_spi_writel(tspi, command1, SPI_COMMAND1);

		/* possibly use the hw based chip select */
		tspi->is_hw_based_cs = false;
		if (cdata && cdata->is_hw_based_cs && is_single_xfer &&
			((tspi->curr_dma_words * tspi->bytes_per_word) ==
						(t->len - tspi->cur_pos))) {
			u32 set_count;
			u32 hold_count;
			u32 spi_cs_timing;
			u32 spi_cs_setup;

			set_count = min(cdata->cs_setup_clk_count, 16);
			if (set_count)
				set_count--;

			hold_count = min(cdata->cs_hold_clk_count, 16);
			if (hold_count)
				hold_count--;

			spi_cs_setup = SPI_SETUP_HOLD(set_count,
					hold_count);
			spi_cs_timing = tspi->spi_cs_timing;
			spi_cs_timing = SPI_CS_SETUP_HOLD(spi_cs_timing,
						spi->chip_select,
						spi_cs_setup);
			tspi->spi_cs_timing = spi_cs_timing;
			tegra_spi_writel(tspi, spi_cs_timing,
						SPI_CS_TIMING1);
			tspi->is_hw_based_cs = true;
		}

		if (cdata && cdata->cs_inactive_cycles) {
			u32 inactive_cycles;

			SPI_SET_CS_ACTIVE_BETWEEN_PACKETS(spi_cs_timing2,
						spi->chip_select,
						0);
			inactive_cycles = min(cdata->cs_inactive_cycles, 32);
			SPI_SET_CYCLES_BETWEEN_PACKETS(spi_cs_timing2,
						spi->chip_select,
						inactive_cycles);
			tegra_spi_writel(tspi, spi_cs_timing2, SPI_CS_TIMING2);
			tspi->is_hw_based_cs = true;
		} else {
			SPI_SET_CS_ACTIVE_BETWEEN_PACKETS(spi_cs_timing2,
						spi->chip_select, 1);
			SPI_SET_CYCLES_BETWEEN_PACKETS(spi_cs_timing2,
						spi->chip_select, 0);
			tegra_spi_writel(tspi, spi_cs_timing2, SPI_CS_TIMING2);
		}

		if (!tspi->is_hw_based_cs) {
			command1 |= SPI_CS_SW_HW;
			if (spi->mode & SPI_CS_HIGH)
				command1 |= SPI_CS_SS_VAL;
			else
				command1 &= ~SPI_CS_SS_VAL;
		} else {
			command1 &= ~SPI_CS_SW_HW;
			command1 &= ~SPI_CS_SS_VAL;
		}

		if (cdata && gpio_is_valid(cdata->cs_gpio)) {
			int gval = 0;
			if (spi->mode & SPI_CS_HIGH)
				gval = 1;
			gpio_set_value(cdata->cs_gpio, gval);
		}

		tegra_spi_set_cmd2(tspi, spi->chip_select, speed);

	} else {
		command1 = tspi->command1_reg;
		command1 &= ~SPI_BIT_LENGTH(~0);
		command1 |= SPI_BIT_LENGTH(bits_per_word - 1);
	}

	if (spi->mode & SPI_LSBYTE_FIRST)
		command1 |= SPI_LSBYTE_FE;
	else
		command1 &= ~SPI_LSBYTE_FE;

	if (spi->mode & SPI_LSB_FIRST)
		command1 |= SPI_LSBIT_FE;
	else
		command1 &= ~SPI_LSBIT_FE;

	if (spi->mode & SPI_3WIRE)
		command1 |= SPI_BIDIROE;
	else
		command1 &= ~SPI_BIDIROE;

	command1 &=  ~SPI_BOTH_EN_BIT;
	if ((t->rx_nbits == SPI_NBITS_DUAL) ||
			(t->tx_nbits == SPI_NBITS_DUAL))
		command1 |= SPI_BOTH_EN_BIT;

	if (tspi->is_packed)
		command1 |= SPI_PACKED;
	else
		command1 &= ~SPI_PACKED;

	command1 &= ~(SPI_CS_SEL_MASK | SPI_TX_EN | SPI_RX_EN);
	tspi->cur_direction = 0;
	if (t->rx_buf)
		tspi->cur_direction |= DATA_DIR_RX;
	if (t->tx_buf)
		tspi->cur_direction |= DATA_DIR_TX;

	command1 |= SPI_CS_SEL(spi->chip_select);
	tspi->command1_reg = command1;

	dev_dbg(tspi->dev, "The def 0x%x and written 0x%lx\n",
				tspi->def_command1_reg, command1);

	tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);

	if (total_fifo_words > SPI_FIFO_DEPTH)
		ret = tegra_spi_start_dma_based_transfer(tspi, t);
	else
		ret = tegra_spi_start_cpu_based_transfer(tspi, t);
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

	ret = of_property_read_bool(data_np, "nvidia,enable-hw-based-cs");
	if (ret)
		cdata->is_hw_based_cs = 1;

	of_property_read_u32(data_np, "nvidia,cs-setup-clk-count",
			&cdata->cs_setup_clk_count);
	of_property_read_u32(data_np, "nvidia,cs-hold-clk-count",
			&cdata->cs_hold_clk_count);
	of_property_read_u32(data_np, "nvidia,rx-clk-tap-delay",
			&cdata->rx_clk_tap_delay);
	of_property_read_u32(data_np, "nvidia,tx-clk-tap-delay",
			&cdata->tx_clk_tap_delay);
	of_property_read_u32(data_np, "nvidia,cs-inactive-cycles",
			&cdata->cs_inactive_cycles);
	of_property_read_u32(data_np, "nvidia,clk-delay-between-packets",
			&cdata->clk_delay_between_packets);

	if (cdata->cs_inactive_cycles && cdata->clk_delay_between_packets) {
		dev_err(&spi->dev,
			"CS inactive time and packet delay cannot coexist\n");
		return NULL;
	}

	if (cdata->clk_delay_between_packets)
		cdata->cs_inactive_cycles = cdata->clk_delay_between_packets;

	cdata->cs_gpio = -EINVAL;
	cdata->cs_gpio = of_get_named_gpio(data_np,
					"nvidia,chipselect-gpio", 0);
	if ((cdata->cs_gpio < 0) && (cdata->cs_gpio != -ENOENT)) {
		dev_err(&spi->dev,
			"CS GPIO is not found on node %s: %d\n",
			data_np->name, cdata->cs_gpio);
		return NULL;
	}
	if ((cdata->cs_gpio < 0) && (cdata->clk_delay_between_packets)) {
		dev_err(&spi->dev,
			"CS packet delay requires gpio chip select\n");
		return NULL;
	}

	of_node_put(data_np);
	return cdata;
}

static int tegra_spi_setup(struct spi_device *spi)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	unsigned long val;
	unsigned long flags;
	unsigned long intr_mask;
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
	if (cdata)
		if (cdata->clk_delay_between_packets)
			cdata->cs_inactive_cycles =
				cdata->clk_delay_between_packets;

	/* Set speed to the spi max fequency if spi device has not set */
	spi->max_speed_hz = spi->max_speed_hz ? : tspi->spi_max_frequency;

	ret = tegra_spi_runtime_get(tspi);
	if (ret < 0) {
		dev_err(tspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}

	if (cdata && gpio_is_valid(cdata->cs_gpio)) {
		if (!tspi->cs_gpio_reqstd[spi->chip_select]) {
			int gpio_flag = GPIOF_OUT_INIT_HIGH;
			if (spi->mode & SPI_CS_HIGH)
				gpio_flag = GPIOF_OUT_INIT_LOW;

			ret = devm_gpio_request_one(tspi->dev, cdata->cs_gpio,
					gpio_flag, "cs_gpio");
			if (ret < 0) {
				dev_err(&spi->dev,
					"GPIO request failed: %d\n", ret);
				return ret;
			}
			tspi->cs_gpio_reqstd[spi->chip_select] = true;
		} else {
			int val = (spi->mode & SPI_CS_HIGH) ? 0 : 1;
			gpio_set_value(cdata->cs_gpio, val);
		}
	}

	if (tspi->chip_data->intr_mask_reg) {
		if ((tspi->cur_direction & DATA_DIR_TX) ||
		    (tspi->cur_direction & DATA_DIR_RX)) {
			intr_mask = tegra_spi_readl(tspi, SPI_INTR_MASK);
			if (!tspi->polling_mode)
				intr_mask &= ~(SPI_INTR_ALL_MASK);
			else
				intr_mask |= SPI_INTR_ALL_MASK;
			tegra_spi_writel(tspi, intr_mask, SPI_INTR_MASK);
		}
	}
	spin_lock_irqsave(&tspi->lock, flags);
	val = tspi->def_command1_reg;
	if (spi->mode & SPI_CS_HIGH)
		val &= ~cs_pol_bit[spi->chip_select];
	else
		val |= cs_pol_bit[spi->chip_select];
	if (tspi->def_chip_select == spi->chip_select)
		val |= SPI_MODE_SEL(spi->mode & 0x3);

	tspi->def_command1_reg = val;
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	spin_unlock_irqrestore(&tspi->lock, flags);

	tegra_spi_set_cmd2(tspi, spi->chip_select, spi->max_speed_hz);

	tegra_spi_runtime_put(tspi);
	return 0;
}

static  int tegra_spi_cs_low(struct spi_device *spi,
		bool state)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	int ret;
	unsigned long val;
	unsigned long flags;
	unsigned int cs_pol_bit[MAX_CHIP_SELECT] = {
			SPI_CS_POL_INACTIVE_0,
			SPI_CS_POL_INACTIVE_1,
			SPI_CS_POL_INACTIVE_2,
			SPI_CS_POL_INACTIVE_3,
	};

	ret = tegra_spi_runtime_get(tspi);
	if (ret < 0) {
		dev_err(tspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}

	if (cdata && gpio_is_valid(cdata->cs_gpio))
		gpio_set_value(cdata->cs_gpio, 0);

	spin_lock_irqsave(&tspi->lock, flags);
	if (!(spi->mode & SPI_CS_HIGH)) {
		val = tegra_spi_readl(tspi, SPI_COMMAND1);
		if (state)
			val &= ~cs_pol_bit[spi->chip_select];
		else
			val |= cs_pol_bit[spi->chip_select];
		tegra_spi_writel(tspi, val, SPI_COMMAND1);
	}

	spin_unlock_irqrestore(&tspi->lock, flags);
	tegra_spi_runtime_put(tspi);

	return 0;
}

static void tegra_spi_dump_regs(struct tegra_spi_data *tspi)
{
	u32 command1_reg;
	u32 fifo_status_reg;
	u32 dma_ctrl_reg;
	u32 trans_status_reg;

	command1_reg = tegra_spi_readl(tspi, SPI_COMMAND1);
	fifo_status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	dma_ctrl_reg = tegra_spi_readl(tspi, SPI_DMA_CTL);
	trans_status_reg = tegra_spi_readl(tspi, SPI_TRANS_STATUS);

	dev_err(tspi->dev,
			"SPI_ERR: CMD_0: 0x%08x, FIFO_STS: 0x%08x\n",
			command1_reg, fifo_status_reg);
	dev_err(tspi->dev,
			"SPI_ERR: DMA_CTL: 0x%08x, TRANS_STS: 0x%08x\n",
			dma_ctrl_reg, trans_status_reg);
}

static int tegra_spi_wait_on_message_xfer(struct tegra_spi_data *tspi)
{
	int ret;

	if (tspi->polling_mode)
		ret = tegra_spi_status_poll(tspi);
	else
		ret = wait_for_completion_timeout(&tspi->xfer_completion,
				SPI_DMA_TIMEOUT);
	if (WARN_ON(ret == 0)) {
		dev_err(tspi->dev,
				"spi trasfer timeout, err %d\n", ret);
		tegra_spi_dump_regs(tspi);
		reset_control_reset(tspi->rstc);
		if (tspi->is_curr_dma_xfer &&
				(tspi->cur_direction & DATA_DIR_TX))
			dmaengine_terminate_all(tspi->tx_dma_chan);
		if (tspi->is_curr_dma_xfer &&
				(tspi->cur_direction & DATA_DIR_RX))
			dmaengine_terminate_all(tspi->rx_dma_chan);
		ret = -EIO;
		return ret;
	}
	if (tspi->tx_status ||  tspi->rx_status) {
		dev_err(tspi->dev, "Error in Transfer\n");
		tegra_spi_dump_regs(tspi);
		check_and_clear_fifo(tspi);
		ret = -EIO;
	}

	return 0;
}

static int tegra_spi_wait_remain_message(struct tegra_spi_data *tspi,
		struct spi_transfer *xfer)
{
	unsigned total_fifo_words;
	int ret = 0;

	reinit_completion(&tspi->xfer_completion);

	if (tspi->is_curr_dma_xfer) {
		total_fifo_words = tegra_spi_calculate_curr_xfer_param(
				tspi->cur_spi, tspi, xfer);
		if (total_fifo_words > SPI_FIFO_DEPTH)
			ret = tegra_spi_start_dma_based_transfer(tspi, xfer);
		else
			ret = tegra_spi_start_cpu_based_transfer(tspi, xfer);
	} else {
		tegra_spi_calculate_curr_xfer_param(tspi->cur_spi, tspi, xfer);
		tegra_spi_start_cpu_based_transfer(tspi, xfer);
	}

	ret = tegra_spi_wait_on_message_xfer(tspi);

	return ret;
}

static int tegra_spi_handle_message(struct tegra_spi_data *tspi,
		struct spi_transfer *xfer)
{
	int ret = 0;
	long wait_status;

	if (tspi->boost_reg_access) {
		/* set max clock for faster register access */
		ret = tegra_spi_set_clock_rate(tspi, tspi->spi_max_frequency);
		if (ret < 0)
			return ret;
	}

	if (!tspi->is_curr_dma_xfer) {
		if (tspi->cur_direction & DATA_DIR_RX)
			tegra_spi_read_rx_fifo_to_client_rxbuf(tspi, xfer);
		if (tspi->cur_direction & DATA_DIR_TX)
			tspi->cur_pos = tspi->cur_tx_pos;
		else if (tspi->cur_direction & DATA_DIR_RX)
			tspi->cur_pos = tspi->cur_rx_pos;
		else
			WARN_ON(1);
	} else {
		if (tspi->cur_direction & DATA_DIR_TX) {
			wait_status = wait_for_completion_interruptible_timeout(
					&tspi->tx_dma_complete,
					SPI_DMA_TIMEOUT);
			if (wait_status <= 0) {
				dmaengine_terminate_all(tspi->tx_dma_chan);
				dev_err(tspi->dev, "TxDma Xfer failed, wait_status - %ld\n",
						wait_status);
				tegra_spi_dump_regs(tspi);
				reset_control_reset(tspi->rstc);
				ret = -EIO;
				return ret;
			}
		}
		if (tspi->cur_direction & DATA_DIR_RX) {
			wait_status = wait_for_completion_interruptible_timeout(
					&tspi->rx_dma_complete,
					SPI_DMA_TIMEOUT);
			if (wait_status <= 0) {
				dmaengine_terminate_all(tspi->rx_dma_chan);
				dev_err(tspi->dev,
						"RxDma Xfer failed, wait_status -  %ld\n",
						wait_status);
				tegra_spi_dump_regs(tspi);
				reset_control_reset(tspi->rstc);
				ret = -EIO;
				return ret;
			}
		}
		if (tspi->cur_direction & DATA_DIR_RX)
			tegra_spi_copy_spi_rxbuf_to_client_rxbuf(tspi, xfer);

		if (tspi->cur_direction & DATA_DIR_TX)
			tspi->cur_pos = tspi->cur_tx_pos;
		else
			tspi->cur_pos = tspi->cur_rx_pos;

	}
	return 0;
}

static int tegra_spi_transfer_one_message(struct spi_master *master,
			struct spi_message *msg)
{
	bool is_first_msg = true;
	bool is_new_msg = true;
	int single_xfer;
	struct tegra_spi_data *tspi = spi_master_get_devdata(master);
	struct spi_transfer *xfer;
	struct spi_device *spi = msg->spi;
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	int ret;
	int gval = 1;
	unsigned long cmd1 = 0;

	msg->status = 0;
	msg->actual_length = 0;

	if (spi->mode & SPI_CS_HIGH)
		gval = 0;

	ret = tegra_spi_runtime_get(tspi);
	if (ret < 0) {
		dev_err(tspi->dev, "runtime PM get failed: %d\n", ret);
		msg->status = ret;
		spi_finalize_current_message(master);
		return ret;
	}

	single_xfer = list_is_singular(&msg->transfers);
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		while (1) {
			if (is_new_msg) {
				reinit_completion(&tspi->xfer_completion);
				ret = tegra_spi_start_transfer_one(spi, xfer,
						is_first_msg, single_xfer);
				if (ret < 0) {
					dev_err(tspi->dev,
							"spi cannot start transfer,err %d\n",
							ret);
					goto exit;
				}
				is_first_msg = false;
				is_new_msg = false;
				ret = tegra_spi_wait_on_message_xfer(tspi);
				if (ret)
					goto exit;
				ret = tegra_spi_handle_message(tspi, xfer);
				if (ret)
					goto exit;
				if (tspi->cur_pos == xfer->len) {
					is_new_msg = true;
					break;
				}
			} else {
				ret = tegra_spi_wait_remain_message(tspi, xfer);
				if (ret)
					goto exit;
				ret = tegra_spi_handle_message(tspi, xfer);
				if (ret)
					goto exit;
				if (tspi->cur_pos == xfer->len) {
					is_new_msg = true;
					break;
				}
			}
		} /* End of while */
		msg->actual_length += xfer->len;
		if (xfer->cs_change && xfer->delay_usecs) {
			tegra_spi_writel(tspi, tspi->def_command1_reg,
					SPI_COMMAND1);

			if (cdata && gpio_is_valid(cdata->cs_gpio))
				gpio_set_value(cdata->cs_gpio, gval);

			udelay(xfer->delay_usecs);

			if (cdata && gpio_is_valid(cdata->cs_gpio))
				gpio_set_value(cdata->cs_gpio, !gval);
		}
	}
	ret = 0;
exit:
	/* CS de-assert is required before clock goes to it's default state. */
	cmd1 = tegra_spi_readl(tspi, SPI_COMMAND1);
	if (!tspi->is_hw_based_cs) {
		if (spi->mode & SPI_CS_HIGH) {
			/* Active high. Reset the value to make it deactive */
			cmd1 &= ~SPI_CS_SS_VAL;
		} else {
			/* Active low. Set the value to make it deactive */
			cmd1 |= SPI_CS_SS_VAL;
		}
	}

	tegra_spi_writel(tspi, cmd1, SPI_COMMAND1);
	if (cdata && gpio_is_valid(cdata->cs_gpio))
		gpio_set_value(cdata->cs_gpio, gval);

	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);

	tegra_spi_runtime_put(tspi);
	msg->status = ret;
	spi_finalize_current_message(master);
	return ret;
}

static void handle_cpu_based_err_xfer(struct tegra_spi_data *tspi)
{
	unsigned long flags;

	spin_lock_irqsave(&tspi->lock, flags);
	if (tspi->tx_status ||  tspi->rx_status) {
		dev_err(tspi->dev, "CpuXfer ERROR bit set 0x%x\n",
			tspi->status_reg);
		dev_err(tspi->dev, "CpuXfer 0x%08x:0x%08x\n",
			tspi->command1_reg, tspi->dma_control_reg);
		tegra_spi_dump_regs(tspi);
		reset_control_reset(tspi->rstc);
	}
	spin_unlock_irqrestore(&tspi->lock, flags);
}

static void handle_dma_based_err_xfer(struct tegra_spi_data *tspi)
{
	int err = 0;
	unsigned long flags;

	spin_lock_irqsave(&tspi->lock, flags);
	/* Abort dmas if any error */
	if (tspi->cur_direction & DATA_DIR_TX) {
		if (tspi->tx_status) {
			dmaengine_terminate_all(tspi->tx_dma_chan);
			err += 1;
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		if (tspi->rx_status) {
			dmaengine_terminate_all(tspi->rx_dma_chan);
			err += 2;
		}
	}

	if (err) {
		dev_err(tspi->dev, "DmaXfer: ERROR bit set 0x%x\n",
			tspi->status_reg);
		dev_err(tspi->dev, "DmaXfer 0x%08x:0x%08x\n",
			tspi->command1_reg, tspi->dma_control_reg);
		tegra_spi_dump_regs(tspi);
		reset_control_reset(tspi->rstc);
	}
	spin_unlock_irqrestore(&tspi->lock, flags);
}

static irqreturn_t tegra_spi_isr(int irq, void *context_data)
{
	struct tegra_spi_data *tspi = context_data;

	if (tspi->polling_mode)
		dev_warn(tspi->dev, "interrupt raised in polling mode\n");

	tegra_spi_clear_status(tspi);
	if (!tspi->transfer_in_progress) {
		dev_err(tspi->dev, "spurious interrupt, status_reg = 0x%x\n",
				tspi->status_reg);
		return IRQ_NONE;
	}
	tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	if (tspi->cur_direction & SPI_TX_EN)
		tspi->tx_status = tspi->status_reg &
					(SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF);

	if (tspi->cur_direction & SPI_RX_EN)
		tspi->rx_status = tspi->status_reg &
					(SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF);

	if (!tspi->is_curr_dma_xfer)
		handle_cpu_based_err_xfer(tspi);
	else
		handle_dma_based_err_xfer(tspi);

	tspi->transfer_in_progress = false;
	complete(&tspi->xfer_completion);
	return IRQ_HANDLED;
}

static int tegra_spi_status_poll(struct tegra_spi_data *tspi)
{
	unsigned int status;
	unsigned long timeout;

	timeout = SPI_POLL_TIMEOUT;
	/*
	 * Read register would take between 1~3us and 1us delay added in loop
	 * Calculate timeout taking this into consideration
	 */
	do {
		status = tegra_spi_readl(tspi, SPI_TRANS_STATUS);
		if (status & SPI_RDY)
			break;
		timeout--;
		udelay(1);
	} while (timeout);

	if (!timeout) {
		dev_err(tspi->dev, "transfer timeout (polling)\n");
		return 0;
	}

	tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->tx_status = tspi->status_reg &
					(SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF);

	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->rx_status = tspi->status_reg &
					(SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF);

	if (!(tspi->cur_direction & DATA_DIR_TX) &&
			!(tspi->cur_direction & DATA_DIR_RX))
		dev_err(tspi->dev, "spurious interrupt, status_reg = 0x%x\n",
				tspi->status_reg);

	tegra_spi_clear_status(tspi);

	if (!tspi->is_curr_dma_xfer)
		handle_cpu_based_err_xfer(tspi);
	else
		handle_dma_based_err_xfer(tspi);

	return timeout;
}

static void tegra_spi_set_slcg(struct tegra_spi_data *tspi)
{
	int reg;

	if (!tspi->chip_data->slcg_support)
		return;
	reg = tegra_spi_readl(tspi, SPI_MISC);
	reg &= ~SPI_MISC_CLKEN_OVERRIDE;
	tegra_spi_writel(tspi, reg, SPI_MISC);
}

static struct tegra_spi_platform_data *tegra_spi_parse_dt(
		struct platform_device *pdev)
{
	struct tegra_spi_platform_data *pdata;
	const unsigned int *prop;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *nc = NULL;
	struct device_node *found_nc = NULL;
	u32 pval;
	int len;
	int ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Memory alloc for pdata failed\n");
		return NULL;
	}

	prop = of_get_property(np, "spi-max-frequency", NULL);
	if (prop)
		pdata->spi_max_frequency = be32_to_cpup(prop);

	if (of_find_property(np, "nvidia,clock-always-on", NULL))
		pdata->is_clkon_always = true;

	if (of_find_property(np, "nvidia,polling-mode", NULL))
		pdata->is_polling_mode = true;
	if (of_find_property(np, "nvidia,boost-reg-access", NULL))
		pdata->boost_reg_access = true;

	if (of_find_property(np, "nvidia,disable-runtime-pm", NULL))
		pdata->runtime_pm = false;
	else
		pdata->runtime_pm = true;

	ret = of_property_read_u32(np, "nvidia,maximum-dma-buffer-size", &pval);
	if (!ret)
		pdata->max_dma_buffer_size = pval;

	/* when no client is defined, default chipselect is zero */
	pdata->def_chip_select = 0;

	/*
	 * Last child node or first node which has property as default-cs will
	 * become the default.
	 */
	for_each_available_child_of_node(np, nc) {
		if (!strcmp(nc->name, "prod-settings"))
			continue;
		found_nc = nc;
		ret = of_property_read_bool(nc, "nvidia,default-chipselect");
		if (ret)
			break;
	}
	if (found_nc) {
		prop = of_get_property(found_nc, "reg", &len);
		if (!prop || len < sizeof(*prop))
			dev_err(&pdev->dev, "%s has no reg property\n",
					found_nc->full_name);
		else
			pdata->def_chip_select = be32_to_cpup(prop);
	}

	return pdata;
}

static struct tegra_spi_chip_data tegra114_spi_chip_data = {
	.intr_mask_reg = false,
	.set_rx_tap_delay = false,
	.slcg_support = false,
};

static struct tegra_spi_chip_data tegra124_spi_chip_data = {
	.intr_mask_reg = false,
	.set_rx_tap_delay = true,
	.slcg_support = false,
};

static struct tegra_spi_chip_data tegra210_spi_chip_data = {
	.intr_mask_reg = true,
	.set_rx_tap_delay = false,
	.slcg_support = false,
};

static struct tegra_spi_chip_data tegra186_spi_chip_data = {
	.intr_mask_reg = true,
	.set_rx_tap_delay = false,
	.slcg_support = true,
};

static struct of_device_id tegra_spi_of_match[] = {
	{
		.compatible = "nvidia,tegra114-spi",
		.data       = &tegra114_spi_chip_data,
	}, {
		.compatible = "nvidia,tegra124-spi",
		.data       = &tegra124_spi_chip_data,
	}, {
		.compatible = "nvidia,tegra210-spi",
		.data       = &tegra210_spi_chip_data,
	}, {
		.compatible = "nvidia,tegra186-spi",
		.data       = &tegra186_spi_chip_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_spi_of_match);

#ifdef	CONFIG_DEBUG_FS
static int tegra_spi_slcg_show(struct seq_file *s, void *unused)
{
	struct tegra_spi_data *tspi = s->private;
	unsigned long reg;
	int ret;

	if (!tspi->chip_data->slcg_support) {
		seq_puts(s, "unsupported\n");
		return 0;
	}
	ret = tegra_spi_runtime_get(tspi);
	if (ret < 0) {
		dev_err(tspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}
	reg = tegra_spi_readl(tspi, SPI_MISC);
	if (reg & SPI_MISC_CLKEN_OVERRIDE)
		seq_puts(s, "disabled\n");
	else
		seq_puts(s, "enabled\n");

	tegra_spi_runtime_put(tspi);

	return 0;
}

static int tegra_spi_slcg_dfs_open(struct inode *inode, struct file *f)
{
	return single_open(f, tegra_spi_slcg_show, inode->i_private);
}

static const struct file_operations tegra_spi_slcg_dfs_fops = {
	.owner = THIS_MODULE,
	.open = tegra_spi_slcg_dfs_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static void tegra_spi_debugfs_init(struct tegra_spi_data *tspi)
{
	struct dentry *retval;

	tspi->debugfs = debugfs_create_dir(dev_name(tspi->dev), NULL);
	if (IS_ERR_OR_NULL(tspi->debugfs))
		goto clean;
	retval = debugfs_create_file("slcg", S_IRUGO | S_IWUSR,
				     tspi->debugfs, (void *)tspi,
				     &tegra_spi_slcg_dfs_fops);
	if (IS_ERR_OR_NULL(retval))
		goto clean;

	return;
clean:
	pr_warn("tegra spi: Failed to create debugfs!\n");
	debugfs_remove_recursive(tspi->debugfs);
}

static void tegra_spi_debugfs_deinit(struct tegra_spi_data *tspi)
{
	debugfs_remove_recursive(tspi->debugfs);
}
#else
static void tegra_spi_debugfs_init(struct tegra_spi_data *tspi) {}
static void tegra_spi_debugfs_deinit(struct tegra_spi_data *tspi) {}
#endif

static int tegra_spi_probe(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct tegra_spi_data	*tspi;
	struct resource		*r;
	struct tegra_spi_platform_data *pdata = pdev->dev.platform_data;
	const struct of_device_id *match;
	const struct tegra_spi_chip_data *chip_data = &tegra114_spi_chip_data;
	int ret, spi_irq;
	int bus_num;

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

	master = spi_alloc_master(&pdev->dev, sizeof(*tspi));
	if (!master) {
		dev_err(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST |
		SPI_TX_DUAL | SPI_RX_DUAL;
	/* supported bpw 4-32 */
	master->bits_per_word_mask = (u32) ~(BIT(0)|BIT(1)|BIT(2));
	master->setup = tegra_spi_setup;
	master->transfer_one_message = tegra_spi_transfer_one_message;
	master->num_chipselect = MAX_CHIP_SELECT;
	master->bus_num = bus_num;
	master->spi_cs_low  = tegra_spi_cs_low;

	dev_set_drvdata(&pdev->dev, master);
	tspi = spi_master_get_devdata(master);
	tspi->master = master;
	tspi->clock_always_on = pdata->is_clkon_always;
	tspi->polling_mode = pdata->is_polling_mode;
	tspi->boost_reg_access = pdata->boost_reg_access;
	tspi->runtime_pm = pdata->runtime_pm;
	if (!tspi->runtime_pm)
		tspi->clock_always_on = true;
	tspi->def_chip_select = pdata->def_chip_select;
	tspi->last_used_cs = MAX_CHIP_SELECT + 1;
	tspi->dev = &pdev->dev;

	if (pdev->dev.of_node) {
		match = of_match_device(tegra_spi_of_match,
				&pdev->dev);
		if (match)
			chip_data = match->data;
	}
	tspi->chip_data = chip_data;

	if (tspi->chip_data->slcg_support)
		tspi->clock_always_on = true;

	tspi->prod_list = tegra_prod_get(&pdev->dev, NULL);
	if (IS_ERR(tspi->prod_list)) {
		dev_dbg(&pdev->dev, "Prod settings list not initialized\n");
		tspi->prod_list = NULL;
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

	spi_irq = platform_get_irq(pdev, 0);
	tspi->irq = spi_irq;

	tspi->clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR(tspi->clk)) {
		dev_err(&pdev->dev, "can not get clock\n");
		ret = PTR_ERR(tspi->clk);
		goto exit_free_master;
	}

	tspi->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(tspi->pinctrl)) {
		dev_info(&pdev->dev, "Pincontrol not found\n");
		tspi->pinctrl = NULL;
	}

	if (tspi->pinctrl) {
		tspi->enable_interface = pinctrl_lookup_state(tspi->pinctrl,
						"interface-enable");
		if (IS_ERR(tspi->enable_interface)) {
			dev_info(&pdev->dev, "Static pin configuration used\n");
			tspi->enable_interface = NULL;
		}
	}

	tspi->rstc = devm_reset_control_get(&pdev->dev, "spi");
	if (IS_ERR(tspi->rstc)) {
		ret = PTR_ERR(tspi->rstc);
		dev_err(&pdev->dev, "can not get reset control %d\n", ret);
		goto exit_free_master;
	}

	tspi->max_buf_size = SPI_FIFO_DEPTH << 2;
	tspi->dma_buf_size = (pdata->max_dma_buffer_size) ?
				pdata->max_dma_buffer_size :
				DEFAULT_SPI_DMA_BUF_LEN;
	tspi->spi_max_frequency = pdata->spi_max_frequency;
	tspi->min_div = 0;

	ret = tegra_spi_init_dma_param(tspi, true);
	if (ret < 0)
		goto exit_free_master;
	ret = tegra_spi_init_dma_param(tspi, false);
	if (ret < 0)
		goto exit_rx_dma_free;
	tspi->max_buf_size = tspi->dma_buf_size;
	init_completion(&tspi->tx_dma_complete);
	init_completion(&tspi->rx_dma_complete);

	init_completion(&tspi->xfer_completion);

	if (tspi->clock_always_on) {
		ret = clk_prepare_enable(tspi->clk);
		if (ret < 0) {
			dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
			goto exit_deinit_dma;
		}
	}
	if (tspi->runtime_pm) {
		pm_runtime_enable(tspi->dev);
		if (!pm_runtime_enabled(tspi->dev)) {
			ret = tegra_spi_runtime_resume(tspi->dev);
			if (ret)
				goto exit_pm_disable;
		}

		/* set autosuspend delay for the adapter device */
		pm_runtime_set_autosuspend_delay(tspi->dev,
						 SPI_AUTOSUSPEND_DELAY);
		pm_runtime_use_autosuspend(tspi->dev);
	}

	ret = tegra_spi_runtime_get(tspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "pm runtime get failed, e = %d\n", ret);
		goto exit_pm_disable;
	}

	reset_control_reset(tspi->rstc);

	tspi->def_command1_reg  = SPI_M_S | SPI_LSBYTE_FE;
	tspi->def_command1_reg |= SPI_CS_SEL(tspi->def_chip_select);
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	tspi->def_command2_reg = tegra_spi_readl(tspi, SPI_COMMAND2);
	tegra_spi_set_slcg(tspi);
	tegra_spi_runtime_put(tspi);

	ret = devm_request_irq(&pdev->dev, tspi->irq, tegra_spi_isr, 0,
			dev_name(&pdev->dev), tspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
					tspi->irq);
		goto exit_free_master;
	}

	master->dev.of_node = pdev->dev.of_node;
	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not register to master err %d\n", ret);
		goto exit_pm_disable;
	}

	if (tspi->enable_interface) {
		ret = pinctrl_select_state(tspi->pinctrl,
				tspi->enable_interface);
		if (ret < 0) {
			dev_err(&pdev->dev, "Enable pin interface failed: %d\n",
				ret);
			goto exit_master_unreg;
		}
	}
	tegra_spi_debugfs_init(tspi);

	return ret;

exit_master_unreg:
	spi_unregister_master(master);

exit_pm_disable:
	pm_runtime_disable(tspi->dev);
	if (!pm_runtime_status_suspended(tspi->dev))
		tegra_spi_runtime_suspend(tspi->dev);
	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);
exit_deinit_dma:
	tegra_spi_deinit_dma_param(tspi, false);
exit_rx_dma_free:
	tegra_spi_deinit_dma_param(tspi, true);
exit_free_master:
	if (tspi->prod_list)
		tegra_prod_release(&tspi->prod_list);
	spi_master_put(master);
	return ret;
}

static int tegra_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = dev_get_drvdata(&pdev->dev);
	struct tegra_spi_data	*tspi = spi_master_get_devdata(master);

	tegra_spi_debugfs_deinit(tspi);
	spi_unregister_master(master);

	if (tspi->tx_dma_chan)
		tegra_spi_deinit_dma_param(tspi, false);

	if (tspi->rx_dma_chan)
		tegra_spi_deinit_dma_param(tspi, true);

	pm_runtime_disable(tspi->dev);
	if (!pm_runtime_status_suspended(tspi->dev))
		tegra_spi_runtime_suspend(tspi->dev);

	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);

	if (tspi->prod_list)
		tegra_prod_release(&tspi->prod_list);

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
	unsigned long intr_mask;
	int ret;

	if (tspi->clock_always_on) {
		ret = clk_prepare_enable(tspi->clk);
		if (ret < 0) {
			dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
			return ret;
		}
	}

	ret = tegra_spi_runtime_get(tspi);
	if (ret < 0) {
		dev_err(dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}
	tegra_spi_writel(tspi, tspi->command1_reg, SPI_COMMAND1);
	tegra_spi_writel(tspi, tspi->def_command2_reg, SPI_COMMAND2);
	tspi->last_used_cs = MAX_CHIP_SELECT + 1;
	tegra_spi_set_slcg(tspi);
	if (tspi->chip_data->intr_mask_reg) {
		if ((tspi->cur_direction & DATA_DIR_TX) ||
		    (tspi->cur_direction & DATA_DIR_RX)) {
			intr_mask = tegra_spi_readl(tspi, SPI_INTR_MASK);
			if (!tspi->polling_mode)
				intr_mask &= ~(SPI_INTR_ALL_MASK);
			else
				intr_mask |= SPI_INTR_ALL_MASK;
			tegra_spi_writel(tspi, intr_mask, SPI_INTR_MASK);
		}
	}
	tegra_spi_runtime_put(tspi);

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
		.name		= "spi-tegra114",
		.owner		= THIS_MODULE,
		.pm		= &tegra_spi_pm_ops,
		.of_match_table	= of_match_ptr(tegra_spi_of_match),
	},
	.probe =	tegra_spi_probe,
	.remove =	tegra_spi_remove,
};
module_platform_driver(tegra_spi_driver);

MODULE_ALIAS("platform:spi-tegra114");
MODULE_DESCRIPTION("NVIDIA Tegra114/124 SPI Controller Driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
