/*
 * QSPI driver for NVIDIA's Tegra210 QUAD SPI Controller.
 *
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/spi/qspi-tegra.h>
#include <linux/clk/tegra.h>

#define QSPI_COMMAND1				0x000
#define QSPI_BIT_LENGTH(x)			(((x) & 0x1f) << 0)
#define QSPI_PACKED				(1 << 5)
#define QSPI_INTERFACE_WIDTH(x)			(((x) & 0x03) << 7)
#define QSPI_INTERFACE_WIDTH_MASK		(0x03 << 7)
#define QSPI_SDR_DDR_SEL			(1 << 9)
#define QSPI_TX_EN				(1 << 11)
#define QSPI_RX_EN				(1 << 12)
#define QSPI_LSBYTE_FE				(1 << 15)
#define QSPI_LSBIT_FE				(1 << 16)
/* BIDIR can be enabled in x1 mode */
#define QSPI_BIDIR				(1 << 17)
#define QSPI_IDLE_SDA_DRIVE_LOW			(0 << 18)
#define QSPI_IDLE_SDA_DRIVE_HIGH		(1 << 18)
#define QSPI_IDLE_SDA_MASK			(3 << 18)
#define QSPI_CS_SW_VAL				(1 << 20)
#define QSPI_CS_SW_HW				(1 << 21)
/* QSPI_CS_POL_INACTIVE bits are default high */
#define QSPI_CS_SEL_0				(0 << 26)
#define QSPI_CS_SEL_MASK			(3 << 26)
#define QSPI_CS_SEL(x)				(((x) & 0x3) << 26)
#define QSPI_CONTROL_MODE_0			(0 << 28)
#define QSPI_CONTROL_MODE_3			(3 << 28)
#define QSPI_CONTROL_MODE_MASK			(3 << 28)
#define QSPI_MODE_SEL(x)			(((x) & 0x3) << 28)
#define QSPI_M_S				(1 << 30)
#define QSPI_PIO				(1 << 31)

#define QSPI_COMMAND2				0x004
#define QSPI_RX_TAP_DELAY(x)			(((x) & 0xFFF) << 0)
#define QSPI_TX_TAP_DELAY(x)			(((x) & 0xFFF) << 12)
#define QSPI_RX_EXT_TAP_DELAY(x)		(((x) & 0xFF) << 24)

#define QSPI_CS_TIMING1				0x008
#define QSPI_SETUP_HOLD(setup, hold)		\
		(((setup) << 4) | ((hold) & 0x0F))
#define QSPI_CS_SETUP_HOLD(reg, cs, val)			\
		((((val) & 0xFFu) << ((cs) * 8)) |	\
		((reg) & ~(0xFFu << ((cs) * 8))))

#define QSPI_CS_TIMING2				0x00C
#define CYCLES_BETWEEN_PACKETS_0(x)		(((x) & 0x1F) << 0)
#define CS_ACTIVE_BETWEEN_PACKETS_0             (1 << 5)
#define QSPI_SET_CYCLES_BETWEEN_PACKETS(reg, cs, val)		\
		(reg = (((val) & 0xF) << ((cs) * 8)) |		\
			((reg) & ~(0xF << ((cs) * 8))))
#define QSPI_NUM_DUMMY_CYCLE(x)			(((x) & 0xFF) << 23)
#define QSPI_NUM_DUMMY_CYCLE_MASK		(0xFF << 23)
#define QSPI_HALF_FULL_CYCLE_SAMPLE		(1 << 31)

#define QSPI_TRANS_STATUS			0x010
#define QSPI_BLK_CNT(val)			(((val) >> 0) & 0xFFFF)
#define QSPI_RDY				(1 << 30)

#define QSPI_FIFO_STATUS			0x014
#define QSPI_RX_FIFO_EMPTY			(1 << 0)
#define QSPI_RX_FIFO_FULL			(1 << 1)
#define QSPI_TX_FIFO_EMPTY			(1 << 2)
#define QSPI_TX_FIFO_FULL			(1 << 3)
#define QSPI_RX_FIFO_UNF			(1 << 4)
#define QSPI_RX_FIFO_OVF			(1 << 5)
#define QSPI_TX_FIFO_UNF			(1 << 6)
#define QSPI_TX_FIFO_OVF			(1 << 7)
#define QSPI_ERR				(1 << 8)
#define QSPI_TX_FIFO_FLUSH			(1 << 14)
#define QSPI_RX_FIFO_FLUSH			(1 << 15)
#define QSPI_TX_FIFO_EMPTY_COUNT(val)		(((val) >> 16) & 0x7F)
#define QSPI_RX_FIFO_FULL_COUNT(val)		(((val) >> 23) & 0x7F)

#define QSPI_FIFO_ERROR				(QSPI_RX_FIFO_UNF | \
			QSPI_RX_FIFO_OVF | QSPI_TX_FIFO_UNF | QSPI_TX_FIFO_OVF)
#define QSPI_FIFO_EMPTY			(QSPI_RX_FIFO_EMPTY | \
						QSPI_TX_FIFO_EMPTY)

#define QSPI_TX_DATA				0x018
#define QSPI_RX_DATA				0x01C

#define QSPI_DMA_CTL				0x020
#define QSPI_TX_TRIG_1				(0 << 15)
#define QSPI_TX_TRIG_4				(1 << 15)
#define QSPI_TX_TRIG_8				(2 << 15)
#define QSPI_TX_TRIG_16				(3 << 15)
#define QSPI_TX_TRIG_MASK			(3 << 15)
#define QSPI_RX_TRIG_1				(0 << 19)
#define QSPI_RX_TRIG_4				(1 << 19)
#define QSPI_RX_TRIG_8				(2 << 19)
#define QSPI_RX_TRIG_16				(3 << 19)
#define QSPI_RX_TRIG_MASK			(3 << 19)
#define QSPI_IE_TX				(1 << 28)
#define QSPI_IE_RX				(1 << 29)
#define QSPI_DMA				(1 << 31)
#define QSPI_DMA_EN				QSPI_DMA

#define QSPI_DMA_BLK				0x024
#define QSPI_DMA_BLK_SET(x)			(((x) & 0xFFFF) << 0)

#define QSPI_TX_FIFO				0x108
#define QSPI_RX_FIFO				0x188

#define QSPI_INTR_MASK				0x18c
#define QSPI_INTR_RX_FIFO_UNF_MASK		(1 << 25)
#define QSPI_INTR_RX_FIFO_OVF_MASK		(1 << 26)
#define QSPI_INTR_TX_FIFO_UNF_MASK		(1 << 27)
#define QSPI_INTR_TX_FIFO_OVF_MASK		(1 << 28)
#define QSPI_INTR_RDY_MASK			(1 << 29)

#define DATA_DIR_TX				(1 << 0)
#define DATA_DIR_RX				(1 << 1)

#define QSPI_DMA_TIMEOUT			(msecs_to_jiffies(10000))
#define DEFAULT_SPI_DMA_BUF_LEN			(64*1024)
#define TX_FIFO_EMPTY_COUNT_MAX			SPI_TX_FIFO_EMPTY_COUNT(0x40)
#define RX_FIFO_FULL_COUNT_ZERO			SPI_RX_FIFO_FULL_COUNT(0)

/*
 * NOTE: Actual chip has only one CS. Below is WAR to enable
 * spidev and mtd layer register same time.
 */
#define MAX_CHIP_SELECT				2
#define QSPI_FIFO_DEPTH				64
#define QSPI_FIFO_FLUSH_MAX_DELAY		2000

struct tegra_qspi_data {
	struct device				*dev;
	struct spi_master			*master;
	spinlock_t				lock;

	struct clk				*clk;
	void __iomem				*base;
	phys_addr_t				phys;
	unsigned				irq;
	int					dma_req_sel;
	bool					clock_always_on;
	u32					qspi_max_frequency;
	u32					cur_speed;

	struct spi_device			*cur_qspi;
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
	u32					qspi_cs_timing;

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
	struct tegra_qspi_device_controller_data cdata[MAX_CHIP_SELECT];
};


static int tegra_qspi_runtime_suspend(struct device *dev);
static int tegra_qspi_runtime_resume(struct device *dev);
static struct tegra_qspi_device_controller_data
	*tegra_qspi_get_cdata_dt(struct spi_device *spi,
			struct tegra_qspi_data *tqspi);

static inline unsigned long tegra_qspi_readl(struct tegra_qspi_data *tqspi,
		unsigned long reg)
{
	return readl(tqspi->base + reg);
}

static inline void tegra_qspi_writel(struct tegra_qspi_data *tqspi,
		unsigned long val, unsigned long reg)
{
	writel(val, tqspi->base + reg);

	/* Read back register to make sure that register writes completed */
	if (reg != QSPI_TX_FIFO)
		readl(tqspi->base + QSPI_COMMAND1);
}

static void tegra_qspi_clear_status(struct tegra_qspi_data *tqspi)
{
	unsigned long val;

	/* Write 1 to clear status register */
	val = tegra_qspi_readl(tqspi, QSPI_TRANS_STATUS);
	tegra_qspi_writel(tqspi, val, QSPI_TRANS_STATUS);

	val = tegra_qspi_readl(tqspi, QSPI_INTR_MASK);
	if (!(val & QSPI_INTR_RDY_MASK)) {
		val |= (QSPI_INTR_RDY_MASK |
				QSPI_INTR_RX_FIFO_UNF_MASK |
				QSPI_INTR_TX_FIFO_UNF_MASK |
				QSPI_INTR_RX_FIFO_OVF_MASK |
				QSPI_INTR_TX_FIFO_OVF_MASK);
		tegra_qspi_writel(tqspi, val, QSPI_INTR_MASK);
	}

	/* Clear fifo status error if any */
	val = tegra_qspi_readl(tqspi, QSPI_FIFO_STATUS);
	if (val & QSPI_ERR)
		tegra_qspi_writel(tqspi, QSPI_ERR | QSPI_FIFO_ERROR,
				QSPI_FIFO_STATUS);
}

static int check_and_clear_fifo(struct tegra_qspi_data *tqspi)
{
	unsigned long status;
	int cnt = QSPI_FIFO_FLUSH_MAX_DELAY;

	/* Make sure that Rx and Tx fifo are empty */
	status = tegra_qspi_readl(tqspi, QSPI_FIFO_STATUS);
	if ((status & QSPI_FIFO_EMPTY) != QSPI_FIFO_EMPTY) {
		/* flush the fifo */
		status |= (QSPI_RX_FIFO_FLUSH | QSPI_TX_FIFO_FLUSH);
		tegra_qspi_writel(tqspi, status, QSPI_FIFO_STATUS);
		do {
			status = tegra_qspi_readl(tqspi, QSPI_FIFO_STATUS);
			if ((status & QSPI_FIFO_EMPTY) == QSPI_FIFO_EMPTY)
				return 0;
			udelay(1);
		} while (cnt--);
		dev_err(tqspi->dev,
			"Rx/Tx fifo are not empty status 0x%08lx\n", status);
		return -EIO;
	}
	return 0;
}

static unsigned tegra_qspi_calculate_curr_xfer_param(
		struct spi_device *spi, struct tegra_qspi_data *tqspi,
		struct spi_transfer *t)
{
	unsigned remain_len = t->len - tqspi->cur_pos;
	unsigned max_word;
	unsigned bits_per_word;
	unsigned max_len;
	unsigned total_fifo_words;

	bits_per_word = t->bits_per_word ? t->bits_per_word :
		spi->bits_per_word;
	tqspi->bytes_per_word = (bits_per_word - 1) / 8 + 1;

	if (bits_per_word == 8 || bits_per_word == 16) {
		tqspi->is_packed = 1;
		tqspi->words_per_32bit = 32/bits_per_word;
	} else {
		tqspi->is_packed = 0;
		tqspi->words_per_32bit = 1;
	}

	if (tqspi->is_packed) {
		max_len = min(remain_len, tqspi->max_buf_size);
		tqspi->curr_dma_words = max_len/tqspi->bytes_per_word;
		total_fifo_words = (max_len + 3)/4;
	} else {
		max_word = (remain_len - 1) / tqspi->bytes_per_word + 1;
		max_word = min(max_word, tqspi->max_buf_size/4);
		tqspi->curr_dma_words = max_word;
		total_fifo_words = max_word;
	}
	return total_fifo_words;
}

static unsigned tegra_qspi_fill_tx_fifo_from_client_txbuf(
		struct tegra_qspi_data *tqspi, struct spi_transfer *t)
{
	unsigned nbytes;
	unsigned tx_empty_count;
	unsigned long fifo_status;
	unsigned max_n_32bit;
	unsigned i, count;
	unsigned long x;
	unsigned int written_words;
	unsigned fifo_words_left;
	u8 *tx_buf = (u8 *)t->tx_buf + tqspi->cur_tx_pos;

	fifo_status = tegra_qspi_readl(tqspi, QSPI_FIFO_STATUS);
	tx_empty_count = QSPI_TX_FIFO_EMPTY_COUNT(fifo_status);

	if (tqspi->is_packed) {
		fifo_words_left = tx_empty_count * tqspi->words_per_32bit;
		written_words = min(fifo_words_left, tqspi->curr_dma_words);
		nbytes = written_words * tqspi->bytes_per_word;
		max_n_32bit = DIV_ROUND_UP(nbytes, 4);
		for (count = 0; count < max_n_32bit; count++) {
			x = 0;
			for (i = 0; (i < 4) && nbytes; i++, nbytes--)
				x |= (*tx_buf++) << (i*8);
			tegra_qspi_writel(tqspi, x, QSPI_TX_FIFO);
		}
	} else {
		max_n_32bit = min(tqspi->curr_dma_words,  tx_empty_count);
		written_words = max_n_32bit;
		nbytes = written_words * tqspi->bytes_per_word;
		for (count = 0; count < max_n_32bit; count++) {
			x = 0;
			for (i = 0; nbytes && (i < tqspi->bytes_per_word);
					i++, nbytes--)
				x |= ((*tx_buf++) << i*8);
			tegra_qspi_writel(tqspi, x, QSPI_TX_FIFO);
		}
	}
	tqspi->cur_tx_pos += written_words * tqspi->bytes_per_word;
	return written_words;
}

static unsigned int tegra_qspi_read_rx_fifo_to_client_rxbuf(
		struct tegra_qspi_data *tqspi, struct spi_transfer *t)
{
	unsigned rx_full_count;
	unsigned long fifo_status;
	unsigned i, count;
	unsigned long x;
	unsigned int read_words = 0;
	unsigned len;
	u8 *rx_buf = (u8 *)t->rx_buf + tqspi->cur_rx_pos;

	fifo_status = tegra_qspi_readl(tqspi, QSPI_FIFO_STATUS);
	rx_full_count = QSPI_RX_FIFO_FULL_COUNT(fifo_status);
	if (tqspi->is_packed) {
		len = tqspi->curr_dma_words * tqspi->bytes_per_word;
		for (count = 0; count < rx_full_count; count++) {
			x = tegra_qspi_readl(tqspi, QSPI_RX_FIFO);
			for (i = 0; len && (i < 4); i++, len--)
				*rx_buf++ = (x >> i*8) & 0xFF;
		}
		tqspi->cur_rx_pos += tqspi->curr_dma_words *
					tqspi->bytes_per_word;
		read_words += tqspi->curr_dma_words;
	} else {
		unsigned int bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
			tqspi->cur_qspi->bits_per_word;
		for (count = 0; count < rx_full_count; count++) {
			x = tegra_qspi_readl(tqspi, QSPI_RX_FIFO);
			for (i = 0; (i < tqspi->bytes_per_word); i++)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
		tqspi->cur_rx_pos += rx_full_count * tqspi->bytes_per_word;
		read_words += rx_full_count;
	}
	return read_words;
}

static void tegra_qspi_copy_client_txbuf_to_qspi_txbuf(
		struct tegra_qspi_data *tqspi, struct spi_transfer *t)
{
	unsigned len;

	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(tqspi->dev, tqspi->tx_dma_phys,
			tqspi->dma_buf_size, DMA_TO_DEVICE);

	if (tqspi->is_packed) {
		len = tqspi->curr_dma_words * tqspi->bytes_per_word;
		memcpy(tqspi->tx_dma_buf, t->tx_buf + tqspi->cur_pos, len);
	} else {
		unsigned int i;
		unsigned int count;
		u8 *tx_buf = (u8 *)t->tx_buf + tqspi->cur_tx_pos;
		unsigned consume = tqspi->curr_dma_words * tqspi->bytes_per_word;
		unsigned int x;

		for (count = 0; count < tqspi->curr_dma_words; count++) {
			x = 0;
			for (i = 0; consume && (i < tqspi->bytes_per_word);
					i++, consume--)
				x |= ((*tx_buf++) << i * 8);
			tqspi->tx_dma_buf[count] = x;
		}
	}
	tqspi->cur_tx_pos += tqspi->curr_dma_words * tqspi->bytes_per_word;

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(tqspi->dev, tqspi->tx_dma_phys,
			tqspi->dma_buf_size, DMA_TO_DEVICE);
}

static void tegra_qspi_copy_qspi_rxbuf_to_client_rxbuf(
		struct tegra_qspi_data *tqspi, struct spi_transfer *t)
{
	unsigned len;

	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(tqspi->dev, tqspi->rx_dma_phys,
			tqspi->dma_buf_size, DMA_FROM_DEVICE);

	if (tqspi->is_packed) {
		len = tqspi->curr_dma_words * tqspi->bytes_per_word;
		memcpy(t->rx_buf + tqspi->cur_rx_pos, tqspi->rx_dma_buf, len);
	} else {
		unsigned int i;
		unsigned int count;
		unsigned char *rx_buf = t->rx_buf + tqspi->cur_rx_pos;
		unsigned int x;
		unsigned int rx_mask, bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
			tqspi->cur_qspi->bits_per_word;
		rx_mask = (1ULL << bits_per_word) - 1;
		for (count = 0; count < tqspi->curr_dma_words; count++) {
			x = tqspi->rx_dma_buf[count];
			x &= rx_mask;
			for (i = 0; (i < tqspi->bytes_per_word); i++)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
	}
	tqspi->cur_rx_pos += tqspi->curr_dma_words * tqspi->bytes_per_word;

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(tqspi->dev, tqspi->rx_dma_phys,
			tqspi->dma_buf_size, DMA_FROM_DEVICE);
}

static void tegra_qspi_dma_complete(void *args)
{
	struct completion *dma_complete = args;

	complete(dma_complete);
}

static int tegra_qspi_start_tx_dma(struct tegra_qspi_data *tqspi, int len)
{
	reinit_completion(&tqspi->tx_dma_complete);
	tqspi->tx_dma_desc = dmaengine_prep_slave_single(tqspi->tx_dma_chan,
			tqspi->tx_dma_phys, len, DMA_MEM_TO_DEV,
			DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!tqspi->tx_dma_desc) {
		dev_err(tqspi->dev, "Not able to get desc for Tx\n");
		return -EIO;
	}

	tqspi->tx_dma_desc->callback = tegra_qspi_dma_complete;
	tqspi->tx_dma_desc->callback_param = &tqspi->tx_dma_complete;

	dmaengine_submit(tqspi->tx_dma_desc);
	dma_async_issue_pending(tqspi->tx_dma_chan);
	return 0;
}

static int tegra_qspi_start_rx_dma(struct tegra_qspi_data *tqspi, int len)
{
	reinit_completion(&tqspi->rx_dma_complete);
	tqspi->rx_dma_desc = dmaengine_prep_slave_single(tqspi->rx_dma_chan,
			tqspi->rx_dma_phys, len, DMA_DEV_TO_MEM,
			DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!tqspi->rx_dma_desc) {
		dev_err(tqspi->dev, "Not able to get desc for Rx\n");
		return -EIO;
	}

	tqspi->rx_dma_desc->callback = tegra_qspi_dma_complete;
	tqspi->rx_dma_desc->callback_param = &tqspi->rx_dma_complete;

	dmaengine_submit(tqspi->rx_dma_desc);
	dma_async_issue_pending(tqspi->rx_dma_chan);
	return 0;
}

static int tegra_qspi_start_dma_based_transfer(
		struct tegra_qspi_data *tqspi, struct spi_transfer *t)
{
	unsigned long val, command1;
	unsigned int len, intr_mask;
	int ret = 0;

	/* Make sure that Rx and Tx fifo are empty */
	ret = check_and_clear_fifo(tqspi);
	if (ret != 0)
		return ret;
	/* TX_EN/RX_EN should not be set here */
	command1 = tqspi->command1_reg;
	tegra_qspi_writel(tqspi, command1, QSPI_COMMAND1);

	val = QSPI_DMA_BLK_SET(tqspi->curr_dma_words - 1);
	tegra_qspi_writel(tqspi, val, QSPI_DMA_BLK);

	if (tqspi->is_packed)
		len = DIV_ROUND_UP(tqspi->curr_dma_words * tqspi->bytes_per_word,
				4) * 4;
	else
		len = tqspi->curr_dma_words * 4;

	/* Set attention level based on length of transfer */
	if (len & 0xF)
		val |= QSPI_TX_TRIG_1 | QSPI_RX_TRIG_1;
	else if (((len) >> 4) & 0x1)
		val |= QSPI_TX_TRIG_4 | QSPI_RX_TRIG_4;
	else
		val |= QSPI_TX_TRIG_8 | QSPI_RX_TRIG_8;

	if ((tqspi->cur_direction & DATA_DIR_TX) ||
			(tqspi->cur_direction & DATA_DIR_RX)) {
		intr_mask = tegra_qspi_readl(tqspi, QSPI_INTR_MASK);
		intr_mask &= ~(QSPI_INTR_RDY_MASK |
				QSPI_INTR_RX_FIFO_UNF_MASK |
				QSPI_INTR_TX_FIFO_UNF_MASK |
				QSPI_INTR_RX_FIFO_OVF_MASK |
				QSPI_INTR_TX_FIFO_OVF_MASK);
		tegra_qspi_writel(tqspi, intr_mask, QSPI_INTR_MASK);
	}

	tegra_qspi_writel(tqspi, val, QSPI_DMA_CTL);
	tqspi->dma_control_reg = val;

	if (tqspi->cur_direction & DATA_DIR_TX) {
		command1 |= QSPI_TX_EN;
		tegra_qspi_copy_client_txbuf_to_qspi_txbuf(tqspi, t);
		ret = tegra_qspi_start_tx_dma(tqspi, len);
		if (ret < 0) {
			dev_err(tqspi->dev,
				"Starting tx dma failed, err %d\n", ret);
			return ret;
		}
	}

	if (tqspi->cur_direction & DATA_DIR_RX) {
		command1 |= QSPI_RX_EN;
		/* Make the dma buffer to read by dma */
		dma_sync_single_for_device(tqspi->dev, tqspi->rx_dma_phys,
				tqspi->dma_buf_size, DMA_FROM_DEVICE);

		ret = tegra_qspi_start_rx_dma(tqspi, len);
		if (ret < 0) {
			dev_err(tqspi->dev,
				"Starting rx dma failed, err %d\n", ret);
			if (tqspi->cur_direction & DATA_DIR_TX)
				dmaengine_terminate_all(tqspi->tx_dma_chan);
			return ret;
		}
	}
	tqspi->is_curr_dma_xfer = true;
	tqspi->dma_control_reg = val;
	val |= QSPI_DMA_EN;

	/* TX_EN/RX_EN need to set after DMA_BLK to avoid spurious interrupt */
	tegra_qspi_writel(tqspi, command1, QSPI_COMMAND1);
	tegra_qspi_writel(tqspi, val, QSPI_DMA_CTL);
	return ret;
}

static int tegra_qspi_start_cpu_based_transfer(
		struct tegra_qspi_data *tqspi, struct spi_transfer *t)
{
	unsigned long val;
	unsigned int cur_words, intr_mask;
	int ret = 0;

	/* Make sure Tx/Rx fifo is empty */
	ret = check_and_clear_fifo(tqspi);
	if (ret != 0)
		return ret;

	/* TX_EN/RX_EN should not be set here */
	tegra_qspi_writel(tqspi, tqspi->command1_reg, QSPI_COMMAND1);

	if (tqspi->cur_direction & DATA_DIR_TX)
		cur_words = tegra_qspi_fill_tx_fifo_from_client_txbuf(tqspi, t);
	else
		cur_words = tqspi->curr_dma_words;

	val = QSPI_DMA_BLK_SET(cur_words - 1);
	tegra_qspi_writel(tqspi, val, QSPI_DMA_BLK);

	val = 0;

	if ((tqspi->cur_direction & DATA_DIR_TX) ||
			(tqspi->cur_direction & DATA_DIR_RX)) {
		intr_mask = tegra_qspi_readl(tqspi, QSPI_INTR_MASK);
		intr_mask &= ~(QSPI_INTR_RDY_MASK |
				QSPI_INTR_RX_FIFO_UNF_MASK |
				QSPI_INTR_TX_FIFO_UNF_MASK |
				QSPI_INTR_RX_FIFO_OVF_MASK |
				QSPI_INTR_TX_FIFO_OVF_MASK);
		tegra_qspi_writel(tqspi, intr_mask, QSPI_INTR_MASK);
	}


	tqspi->is_curr_dma_xfer = false;
	val = tqspi->command1_reg;
	/* TX_EN/RX_EN need to set after DMA_BLK to avoid spurious interrupt */
	if (tqspi->cur_direction & DATA_DIR_RX)
		val |= QSPI_RX_EN;
	if (tqspi->cur_direction & DATA_DIR_TX)
		val |= QSPI_TX_EN;
	tegra_qspi_writel(tqspi, val, QSPI_COMMAND1);

	/* Need to stabilize other reg bit before PIO bit set */
	udelay(2);
	val |= QSPI_PIO;
	tegra_qspi_writel(tqspi, val, QSPI_COMMAND1);
	return 0;
}

static int tegra_qspi_init_dma_param(struct tegra_qspi_data *tqspi,
		bool dma_to_memory)
{
	struct dma_chan *dma_chan;
	u32 *dma_buf;
	dma_addr_t dma_phys;
	int ret;
	struct dma_slave_config dma_sconfig;
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_chan = dma_request_channel(mask, NULL, NULL);
	if (!dma_chan) {
		dev_err(tqspi->dev,
			"Dma channel is not available, will try later\n");
		return -EPROBE_DEFER;
	}

	dma_buf = dma_alloc_coherent(tqspi->dev, tqspi->dma_buf_size,
			&dma_phys, GFP_KERNEL);
	if (!dma_buf) {
		dev_err(tqspi->dev, "Not able to allocate the dma buffer\n");
		dma_release_channel(dma_chan);
		return -ENOMEM;
	}

	dma_sconfig.slave_id = tqspi->dma_req_sel;
	if (dma_to_memory) {
		dma_sconfig.src_addr = tqspi->phys + QSPI_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.src_maxburst = 0;
	} else {
		dma_sconfig.dst_addr = tqspi->phys + QSPI_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = 0;
	}

	ret = dmaengine_slave_config(dma_chan, &dma_sconfig);
	if (ret)
		goto scrub;
	if (dma_to_memory) {
		tqspi->rx_dma_chan = dma_chan;
		tqspi->rx_dma_buf = dma_buf;
		tqspi->rx_dma_phys = dma_phys;
	} else {
		tqspi->tx_dma_chan = dma_chan;
		tqspi->tx_dma_buf = dma_buf;
		tqspi->tx_dma_phys = dma_phys;
	}
	return 0;

scrub:
	dma_free_coherent(tqspi->dev, tqspi->dma_buf_size, dma_buf, dma_phys);
	dma_release_channel(dma_chan);
	return ret;
}

static void tegra_qspi_deinit_dma_param(struct tegra_qspi_data *tqspi,
		bool dma_to_memory)
{
	u32 *dma_buf;
	dma_addr_t dma_phys;
	struct dma_chan *dma_chan;

	if (dma_to_memory) {
		dma_buf = tqspi->rx_dma_buf;
		dma_chan = tqspi->rx_dma_chan;
		dma_phys = tqspi->rx_dma_phys;
		tqspi->rx_dma_chan = NULL;
		tqspi->rx_dma_buf = NULL;
	} else {
		dma_buf = tqspi->tx_dma_buf;
		dma_chan = tqspi->tx_dma_chan;
		dma_phys = tqspi->tx_dma_phys;
		tqspi->tx_dma_buf = NULL;
		tqspi->tx_dma_chan = NULL;
	}
	if (!dma_chan)
		return;

	dma_free_coherent(tqspi->dev, tqspi->dma_buf_size, dma_buf, dma_phys);
	dma_release_channel(dma_chan);
}

static int tegra_qspi_validate_request(struct spi_device *spi,
		struct tegra_qspi_data *tqspi, struct spi_transfer *t,
		bool is_ddr)
{
	int req_mode;

	req_mode = spi->mode & 0x3;
	if ((req_mode == SPI_MODE_1) || (req_mode == SPI_MODE_2)) {
		dev_err(tqspi->dev, "Qspi does not support mode %d\n",
				req_mode);
		return -EINVAL;
	}

	if ((req_mode == SPI_MODE_3) && is_ddr) {
		dev_err(tqspi->dev, "DDR is not supported in mode 3\n");
		return -EINVAL;
	}

	if ((t->bits_per_word != 8) && (t->bits_per_word != 16)
			&& (t->bits_per_word != 32)) {
		dev_err(tqspi->dev, "qspi does not support bpw = %d\n",
				t->bits_per_word);
		return -EINVAL;
	}

	if (((t->bits_per_word == 16) && (t->len & 0x1)) ||
		((t->bits_per_word == 32) && (t->len & 0x3))) {
		dev_err(tqspi->dev, "unaligned len = %d, bits_per_word = %d not supported\n",
				t->len, t->bits_per_word);
		return -EINVAL;
	}

	return 0;
}

static int tegra_qspi_start_transfer_one(struct spi_device *spi,
		struct spi_transfer *t, bool is_first_of_msg,
		bool is_single_xfer)
{
	struct tegra_qspi_data *tqspi = spi_master_get_devdata(spi->master);
	u32 speed,  qspi_cs_timing2 = 0;
	u8 bits_per_word;
	unsigned total_fifo_words;
	int ret;
	unsigned long command1;
	int req_mode;
	u8 bus_width = X1, num_dummy_cycles = 0;
	bool is_ddr = false;
	struct tegra_qspi_device_controller_data *cdata = spi->controller_data;

	bits_per_word = t->bits_per_word;
	tqspi->cur_qspi = spi;
	tqspi->cur_pos = 0;
	tqspi->cur_rx_pos = 0;
	tqspi->cur_tx_pos = 0;
	tqspi->curr_xfer = t;
	tqspi->tx_status = 0;
	tqspi->rx_status = 0;
	total_fifo_words = tegra_qspi_calculate_curr_xfer_param(spi, tqspi, t);

	if (cdata) {
		if ((t->len - tqspi->cur_pos) >  cdata->x1_len_limit) {
			num_dummy_cycles =  cdata->x4_dymmy_cycle;
			speed = cdata->x4_bus_speed;
		} else {
			is_ddr = false;
			num_dummy_cycles =  cdata->x1_dymmy_cycle;
			speed = cdata->x1_bus_speed;
		}
	} else {
		dev_err(tqspi->dev, "Controller Data is NULL\n");
		return -EINVAL;
	}
	/*
	 * NOTE:
	 * 1.Bus width can be x4 even for command/addr for QPI commands.
	 *   So caller requested bus width should be considered.
	 * 2. is_ddr is not applicable for write. Write is always in SDR mode.
	 */
	is_ddr = get_sdr_ddr(t->delay_usecs);
	bus_width = get_bus_width(t->delay_usecs);

	if (is_ddr) {
		ret = tegra_clk_cfg_ex(tqspi->clk, TEGRA_CLK_QSPI_DIV2_ENB, 1);
		if (ret) {
			dev_err(tqspi->dev, "Failed to set clk DIV2 %d\n", ret);
			return -EINVAL;
		}
	} else if (!cdata->ifddr_div2_sdr) {
		ret = tegra_clk_cfg_ex(tqspi->clk, TEGRA_CLK_QSPI_DIV2_ENB, 0);
		if (ret) {
			dev_err(tqspi->dev, "Failed to reset clk DIV2\n");
			return -EINVAL;
		}
	}

	ret = tegra_qspi_validate_request(spi, tqspi, t, is_ddr);
	if (ret)
		return ret;

	if (!speed)
		speed = tqspi->qspi_max_frequency;
	if (speed != tqspi->cur_speed) {
		ret = clk_set_rate(tqspi->clk, speed);
		if (ret) {
			dev_err(tqspi->dev, "Failed to set clk freq %d\n", ret);
			return -EINVAL;
		}
		tqspi->cur_speed = speed;
	}

	if (is_first_of_msg) {
		tegra_qspi_clear_status(tqspi);

		command1 = tqspi->def_command1_reg;
		command1 |= QSPI_BIT_LENGTH(bits_per_word - 1);

		command1 &= ~QSPI_CONTROL_MODE_MASK;
		req_mode = spi->mode & 0x3;
		if (req_mode == SPI_MODE_0)
			command1 |= QSPI_CONTROL_MODE_0;
		else if (req_mode == SPI_MODE_3)
			command1 |= QSPI_CONTROL_MODE_3;
		else {
			dev_err(tqspi->dev, "invalid mode %d\n", req_mode);
			return -EINVAL;
		}
		/* Toggle CS to active state now */
		if (spi->mode & SPI_CS_HIGH)
			command1 |= QSPI_CS_SW_VAL;
		else
			command1 &= ~QSPI_CS_SW_VAL;
		tegra_qspi_writel(tqspi, command1, QSPI_COMMAND1);
	} else {
		command1 = tqspi->command1_reg;
		command1 &= ~QSPI_BIT_LENGTH(~0);
		command1 |= QSPI_BIT_LENGTH(bits_per_word - 1);
	}

	command1 &= ~QSPI_SDR_DDR_SEL;
	if (is_ddr)
		command1 |= QSPI_SDR_DDR_SEL;

	command1 &= ~QSPI_INTERFACE_WIDTH_MASK;
	command1 |= QSPI_INTERFACE_WIDTH(bus_width);

	command1 &= ~QSPI_PACKED;
	if (tqspi->is_packed)
		command1 |= QSPI_PACKED;

	command1 &= ~(QSPI_TX_EN | QSPI_RX_EN);
	tqspi->cur_direction = 0;
	if (t->rx_buf)
		tqspi->cur_direction |= DATA_DIR_RX;

	if (t->tx_buf)
		tqspi->cur_direction |= DATA_DIR_TX;

	tqspi->command1_reg = command1;

	if (!cdata) {
		u32 command2_reg;
		int rx_tap_delay;
		int tx_tap_delay;


		rx_tap_delay = cdata->rx_clk_tap_delay;
		tx_tap_delay = min(cdata->tx_clk_tap_delay, 63);

		command2_reg = QSPI_TX_TAP_DELAY(tx_tap_delay) |
			QSPI_RX_TAP_DELAY(rx_tap_delay);
		tegra_qspi_writel(tqspi, command2_reg, QSPI_COMMAND2);
	}

	qspi_cs_timing2 &= QSPI_NUM_DUMMY_CYCLE_MASK;
	qspi_cs_timing2 |= QSPI_NUM_DUMMY_CYCLE(num_dummy_cycles);
	qspi_cs_timing2 |= CS_ACTIVE_BETWEEN_PACKETS_0;
	tegra_qspi_writel(tqspi, qspi_cs_timing2, QSPI_CS_TIMING2);

	if (total_fifo_words > QSPI_FIFO_DEPTH)
		ret = tegra_qspi_start_dma_based_transfer(tqspi, t);
	else
		ret = tegra_qspi_start_cpu_based_transfer(tqspi, t);

	return ret;
}

static int tegra_qspi_setup(struct spi_device *spi)
{
	struct tegra_qspi_data *tqspi = spi_master_get_devdata(spi->master);
	struct tegra_qspi_device_controller_data *cdata = spi->controller_data;
	unsigned long val;
	unsigned long flags;
	int ret;

	dev_dbg(&spi->dev, "setup %d bpw, %scpol, %scpha, %dHz\n",
			spi->bits_per_word,
			spi->mode & SPI_CPOL ? "" : "~",
			spi->mode & SPI_CPHA ? "" : "~",
			spi->max_speed_hz);

	if (spi->chip_select >= MAX_CHIP_SELECT) {
		dev_err(tqspi->dev, "Wrong chip select = %d\n",
			spi->chip_select);
		return -EINVAL;
	}

	if (!cdata) {
		cdata = tegra_qspi_get_cdata_dt(spi, tqspi);
		spi->controller_data = cdata;
	}

	/* Set speed to the spi max fequency if qspi device has not set */
	spi->max_speed_hz = spi->max_speed_hz ? : tqspi->qspi_max_frequency;

	ret = pm_runtime_get_sync(tqspi->dev);
	if (ret < 0) {
		dev_err(tqspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}

	spin_lock_irqsave(&tqspi->lock, flags);
	/* keep default cs state to inactive */
	val = tqspi->def_command1_reg;
	if (spi->mode & SPI_CS_HIGH)
		val  &= ~QSPI_CS_SW_VAL;
	else
		val |= QSPI_CS_SW_VAL;

	tqspi->def_command1_reg = val;
	tegra_qspi_writel(tqspi, tqspi->def_command1_reg, QSPI_COMMAND1);
	spin_unlock_irqrestore(&tqspi->lock, flags);

	pm_runtime_put(tqspi->dev);
	return 0;
}

static  int tegra_qspi_cs_low(struct spi_device *spi,
		bool state)
{
	struct tegra_qspi_data *tqspi = spi_master_get_devdata(spi->master);
	int ret;
	unsigned long val;
	unsigned long flags;

	BUG_ON(spi->chip_select >= MAX_CHIP_SELECT);

	ret = pm_runtime_get_sync(tqspi->dev);
	if (ret < 0) {
		dev_err(tqspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}

	spin_lock_irqsave(&tqspi->lock, flags);

	val = tegra_qspi_readl(tqspi, QSPI_COMMAND1);
	if (state)
		val &= ~QSPI_CS_SW_VAL;
	else
		val |= QSPI_CS_SW_VAL;
	tegra_qspi_writel(tqspi, val, QSPI_COMMAND1);

	spin_unlock_irqrestore(&tqspi->lock, flags);
	pm_runtime_put(tqspi->dev);
	return 0;
}

static int tegra_qspi_transfer_one_message(struct spi_master *master,
		struct spi_message *msg)
{
	bool is_first_msg = true;
	int single_xfer;
	struct tegra_qspi_data *tqspi = spi_master_get_devdata(master);
	struct spi_transfer *xfer;
	struct spi_device *spi = msg->spi;
	int ret;

	msg->status = 0;
	msg->actual_length = 0;

	ret = pm_runtime_get_sync(tqspi->dev);
	if (ret < 0) {
		dev_err(tqspi->dev, "runtime PM get failed: %d\n", ret);
		msg->status = ret;
		spi_finalize_current_message(master);
		return ret;
	}

	single_xfer = list_is_singular(&msg->transfers);
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		reinit_completion(&tqspi->xfer_completion);
		ret = tegra_qspi_start_transfer_one(spi, xfer,
				is_first_msg, single_xfer);
		if (ret < 0) {
			dev_err(tqspi->dev,
				"qspi can not start transfer, err %d\n", ret);
			goto exit;
		}
		is_first_msg = false;
		ret = wait_for_completion_timeout(&tqspi->xfer_completion,
				QSPI_DMA_TIMEOUT);
		if (WARN_ON(ret == 0)) {
			dev_err(tqspi->dev,
				"spi transfer timeout, err %d\n", ret);
			if (tqspi->is_curr_dma_xfer &&
					(tqspi->cur_direction & DATA_DIR_TX))
				dmaengine_terminate_all(tqspi->tx_dma_chan);
			if (tqspi->is_curr_dma_xfer &&
					(tqspi->cur_direction & DATA_DIR_RX))
				dmaengine_terminate_all(tqspi->rx_dma_chan);
			ret = -EIO;
			goto exit;
		}

		if (tqspi->tx_status ||  tqspi->rx_status) {
			dev_err(tqspi->dev, "Error in Transfer\n");
			tqspi->tx_status = 0;
			tqspi->rx_status = 0;
			ret = -EIO;
			goto exit;
		}
		msg->actual_length += xfer->len;
	}
	ret = 0;
exit:
	tegra_qspi_writel(tqspi, tqspi->def_command1_reg, QSPI_COMMAND1);
	pm_runtime_put(tqspi->dev);
	msg->status = ret;
	spi_finalize_current_message(master);
	return ret;
}

static irqreturn_t handle_cpu_based_xfer(struct tegra_qspi_data *tqspi)
{
	struct spi_transfer *t = tqspi->curr_xfer;
	unsigned long flags;

	spin_lock_irqsave(&tqspi->lock, flags);
	if (tqspi->tx_status ||  tqspi->rx_status) {
		dev_err(tqspi->dev, "CpuXfer ERROR bit set 0x%x\n",
				tqspi->status_reg);
		dev_err(tqspi->dev, "CpuXfer 0x%08x:0x%08x\n",
				tqspi->command1_reg, tqspi->dma_control_reg);
		tegra_periph_reset_assert(tqspi->clk);
		udelay(2);
		tegra_periph_reset_deassert(tqspi->clk);
		complete(&tqspi->xfer_completion);
		tqspi->tx_status = 0;
		tqspi->rx_status = 0;
		goto exit;
	}

	if (tqspi->cur_direction & DATA_DIR_RX)
		tegra_qspi_read_rx_fifo_to_client_rxbuf(tqspi, t);

	if (tqspi->cur_direction & DATA_DIR_TX)
		tqspi->cur_pos = tqspi->cur_tx_pos;
	else
		tqspi->cur_pos = tqspi->cur_rx_pos;

	if (tqspi->cur_pos >= t->len) {
		complete(&tqspi->xfer_completion);
		goto exit;
	}

	tegra_qspi_calculate_curr_xfer_param(tqspi->cur_qspi, tqspi, t);
	tegra_qspi_start_cpu_based_transfer(tqspi, t);
exit:
	spin_unlock_irqrestore(&tqspi->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t handle_dma_based_xfer(struct tegra_qspi_data *tqspi)
{
	struct spi_transfer *t = tqspi->curr_xfer;
	long wait_status;
	int err = 0;
	unsigned total_fifo_words;
	unsigned long flags;

	/* Abort dmas if any error */
	if (tqspi->cur_direction & DATA_DIR_TX) {
		if (tqspi->tx_status) {
			dmaengine_terminate_all(tqspi->tx_dma_chan);
			err += 1;
		} else {
			wait_status = wait_for_completion_interruptible_timeout(
					&tqspi->tx_dma_complete, QSPI_DMA_TIMEOUT);
			if (wait_status <= 0) {
				dmaengine_terminate_all(tqspi->tx_dma_chan);
				dev_err(tqspi->dev, "TxDma Xfer failed\n");
				err += 1;
			}
		}
	}

	if (tqspi->cur_direction & DATA_DIR_RX) {
		if (tqspi->rx_status) {
			dmaengine_terminate_all(tqspi->rx_dma_chan);
			err += 2;
		} else {
			wait_status = wait_for_completion_interruptible_timeout(
				&tqspi->rx_dma_complete, QSPI_DMA_TIMEOUT);
			if (wait_status <= 0) {
				dmaengine_terminate_all(tqspi->rx_dma_chan);
				dev_err(tqspi->dev, "RxDma Xfer failed\n");
				err += 2;
			}
		}
	}

	spin_lock_irqsave(&tqspi->lock, flags);
	if (err) {
		dev_err(tqspi->dev, "DmaXfer: ERROR bit set 0x%x\n",
				tqspi->status_reg);
		dev_err(tqspi->dev, "DmaXfer 0x%08x:0x%08x\n",
				tqspi->command1_reg, tqspi->dma_control_reg);
		tegra_periph_reset_assert(tqspi->clk);
		udelay(2);
		tegra_periph_reset_deassert(tqspi->clk);
		complete(&tqspi->xfer_completion);
		tqspi->rx_status = 0;
		tqspi->tx_status = 0;
		spin_unlock_irqrestore(&tqspi->lock, flags);
		return IRQ_HANDLED;
	}

	if (tqspi->cur_direction & DATA_DIR_RX)
		tegra_qspi_copy_qspi_rxbuf_to_client_rxbuf(tqspi, t);

	if (tqspi->cur_direction & DATA_DIR_TX)
		tqspi->cur_pos = tqspi->cur_tx_pos;
	else
		tqspi->cur_pos = tqspi->cur_rx_pos;

	if (tqspi->cur_pos >= t->len) {
		complete(&tqspi->xfer_completion);
		goto exit;
	}

	/* Continue transfer in current message */
	total_fifo_words = tegra_qspi_calculate_curr_xfer_param(tqspi->cur_qspi,
			tqspi, t);
	if (total_fifo_words > QSPI_FIFO_DEPTH)
		err = tegra_qspi_start_dma_based_transfer(tqspi, t);
	else
		err = tegra_qspi_start_cpu_based_transfer(tqspi, t);

exit:
	spin_unlock_irqrestore(&tqspi->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t tegra_qspi_isr_thread(int irq, void *context_data)
{
	struct tegra_qspi_data *tqspi = context_data;

	if (!tqspi->is_curr_dma_xfer)
		return handle_cpu_based_xfer(tqspi);
	return handle_dma_based_xfer(tqspi);
}

static irqreturn_t tegra_qspi_isr(int irq, void *context_data)
{
	struct tegra_qspi_data *tqspi = context_data;

	tqspi->status_reg = tegra_qspi_readl(tqspi, QSPI_FIFO_STATUS);
	if (tqspi->cur_direction & DATA_DIR_TX)
		tqspi->tx_status = tqspi->status_reg &
			(QSPI_TX_FIFO_UNF | QSPI_TX_FIFO_OVF);

	if (tqspi->cur_direction & DATA_DIR_RX)
		tqspi->rx_status = tqspi->status_reg &
			(QSPI_RX_FIFO_OVF | QSPI_RX_FIFO_UNF);

	if (!(tqspi->cur_direction & DATA_DIR_TX) &&
			!(tqspi->cur_direction & DATA_DIR_RX))
		dev_err(tqspi->dev, "spurious interrupt, status_reg = 0x%x\n",
				tqspi->status_reg);

	tegra_qspi_clear_status(tqspi);

	return IRQ_WAKE_THREAD;
}

static struct tegra_qspi_device_controller_data
	*tegra_qspi_get_cdata_dt(struct spi_device *spi,
			struct tegra_qspi_data *tqspi)
{
	struct tegra_qspi_device_controller_data *cdata = NULL;
	const unsigned int *prop;
	struct device_node *np = spi->dev.of_node, *data_np = NULL;

	if (!np) {
		dev_dbg(&spi->dev, "Device node not found \n");
		return NULL;
	}

	data_np = of_get_child_by_name(np, "controller-data");
	if (!data_np) {
		dev_dbg(&spi->dev, "child node 'controller-data' not found\n");
		return NULL;
	}

	cdata = &tqspi->cdata[spi->chip_select];
	memset(cdata, 0, sizeof(*cdata));

	prop = of_get_property(data_np, "nvidia,rx-clk-tap-delay", NULL);
	if (prop)
		cdata->rx_clk_tap_delay = be32_to_cpup(prop);

	prop = of_get_property(data_np, "nvidia,tx-clk-tap-delay", NULL);
	if (prop)
		cdata->tx_clk_tap_delay = be32_to_cpup(prop);

	prop = of_get_property(data_np, "nvidia,x1-len-limit", NULL);
	if (prop)
		cdata->x1_len_limit = be32_to_cpup(prop);

	prop = of_get_property(data_np, "nvidia,x1-bus-speed", NULL);
	if (prop)
		cdata->x1_bus_speed = be32_to_cpup(prop);

	prop = of_get_property(data_np, "nvidia,x1-dymmy-cycle", NULL);
	if (prop)
		cdata->x1_dymmy_cycle = be32_to_cpup(prop);

	prop = of_get_property(data_np, "nvidia,x4-bus-speed", NULL);
	if (prop)
		cdata->x4_bus_speed = be32_to_cpup(prop);

	prop = of_get_property(data_np, "nvidia,x4-dymmy-cycle", NULL);
	if (prop)
		cdata->x4_dymmy_cycle = be32_to_cpup(prop);

	prop = of_get_property(data_np, "nvidia,x4-is-ddr", NULL);
	if (prop)
		cdata->x4_is_ddr = be32_to_cpup(prop);

	prop = of_get_property(data_np, "nvidia,ifddr-div2-sdr", NULL);
	if (prop)
		cdata->ifddr_div2_sdr = be32_to_cpup(prop);

	of_node_put(data_np);
	return cdata;
}

static struct tegra_qspi_platform_data *tegra_qspi_parse_dt(
		struct platform_device *pdev)
{
	struct tegra_qspi_platform_data *pdata;
	const unsigned int *prop;
	struct device_node *np = pdev->dev.of_node;
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
		pdata->qspi_max_frequency = be32_to_cpup(prop);

	if (of_find_property(np, "nvidia,clock-always-on", NULL))
		pdata->is_clkon_always = true;

	return pdata;
}

static struct of_device_id tegra_qspi_of_match[] = {
	{ .compatible = "nvidia,tegra210-qspi", },
	{}
};
MODULE_DEVICE_TABLE(of, tegra_qspi_of_match);

static int tegra_qspi_probe(struct platform_device *pdev)
{
	struct spi_master	*master;
	struct tegra_qspi_data	*tqspi;
	struct resource		*r;
	struct tegra_qspi_platform_data *pdata = pdev->dev.platform_data;
	int ret, qspi_irq;
	int bus_num;

	if (pdev->dev.of_node) {
		bus_num = of_alias_get_id(pdev->dev.of_node, "qspi");
		if (bus_num < 0) {
			dev_warn(&pdev->dev,
				"Dynamic bus number will be registerd\n");
			bus_num = -1;
		}
	} else {
		bus_num = pdev->id;
	}

	if (!pdata && pdev->dev.of_node)
		pdata = tegra_qspi_parse_dt(pdev);

	if (!pdata) {
		dev_err(&pdev->dev, "No platform data, exiting\n");
		return -ENODEV;
	}

	if (!pdata->qspi_max_frequency)
		pdata->qspi_max_frequency = 133000000; /* 133MHz */

	master = spi_alloc_master(&pdev->dev, sizeof(*tqspi));
	if (!master) {
		dev_err(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}

	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;
	master->setup = tegra_qspi_setup;
	master->transfer_one_message = tegra_qspi_transfer_one_message;
	master->num_chipselect = MAX_CHIP_SELECT;
	master->bus_num = bus_num;
	master->spi_cs_low  = tegra_qspi_cs_low;

	dev_set_drvdata(&pdev->dev, master);
	tqspi = spi_master_get_devdata(master);
	tqspi->master = master;
	tqspi->dma_req_sel = pdata->dma_req_sel;
	tqspi->clock_always_on = pdata->is_clkon_always;
	tqspi->dev = &pdev->dev;
	spin_lock_init(&tqspi->lock);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!r) {
		dev_err(&pdev->dev, "No IO memory resource\n");
		ret = -ENODEV;
		goto exit_free_master;
	}
	tqspi->phys = r->start;
	tqspi->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(tqspi->base)) {
		dev_err(&pdev->dev,
				"Cannot request memregion/iomap dma address\n");
		ret = PTR_ERR(tqspi->base);
		goto exit_free_master;
	}

	qspi_irq = platform_get_irq(pdev, 0);
	tqspi->irq = qspi_irq;
	ret = request_threaded_irq(tqspi->irq, tegra_qspi_isr,
			tegra_qspi_isr_thread, IRQF_ONESHOT,
			dev_name(&pdev->dev), tqspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
				tqspi->irq);
		goto exit_free_master;
	}

	tqspi->clk = devm_clk_get(&pdev->dev, "qspi");
	if (IS_ERR(tqspi->clk)) {
		dev_err(&pdev->dev, "can not get clock\n");
		ret = PTR_ERR(tqspi->clk);
		goto exit_free_irq;
	}

	tqspi->max_buf_size = QSPI_FIFO_DEPTH << 2;
	tqspi->dma_buf_size = DEFAULT_SPI_DMA_BUF_LEN;
	tqspi->qspi_max_frequency = pdata->qspi_max_frequency;

	if (pdata->dma_req_sel) {
		ret = tegra_qspi_init_dma_param(tqspi, true);
		if (ret < 0) {
			dev_err(&pdev->dev, "RxDma Init failed, err %d\n", ret);
			goto exit_free_irq;
		}

		ret = tegra_qspi_init_dma_param(tqspi, false);
		if (ret < 0) {
			dev_err(&pdev->dev, "TxDma Init failed, err %d\n", ret);
			goto exit_rx_dma_free;
		}
		tqspi->max_buf_size = tqspi->dma_buf_size;
		init_completion(&tqspi->tx_dma_complete);
		init_completion(&tqspi->rx_dma_complete);
	}

	init_completion(&tqspi->xfer_completion);

	if (tqspi->clock_always_on) {
		ret = clk_prepare_enable(tqspi->clk);
		if (ret < 0) {
			dev_err(tqspi->dev, "clk_prepare failed: %d\n", ret);
			goto exit_deinit_dma;
		}
	}
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra_qspi_runtime_resume(&pdev->dev);
		if (ret)
			goto exit_pm_disable;
	}

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "pm runtime get failed, e = %d\n", ret);
		goto exit_pm_disable;
	}

	tqspi->def_command1_reg  = QSPI_M_S | QSPI_CS_SW_HW |  QSPI_CS_SW_VAL;
	tegra_qspi_writel(tqspi, tqspi->def_command1_reg, QSPI_COMMAND1);
	tqspi->def_command2_reg = tegra_qspi_readl(tqspi, QSPI_COMMAND2);
	pm_runtime_put(&pdev->dev);

	master->dev.of_node = pdev->dev.of_node;
	ret = spi_register_master(master);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not register to master err %d\n", ret);
		goto exit_pm_disable;
	}
	return ret;

exit_pm_disable:
	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_qspi_runtime_suspend(&pdev->dev);
	if (tqspi->clock_always_on)
		clk_disable_unprepare(tqspi->clk);
exit_deinit_dma:
	tegra_qspi_deinit_dma_param(tqspi, false);
exit_rx_dma_free:
	tegra_qspi_deinit_dma_param(tqspi, true);
exit_free_irq:
	free_irq(qspi_irq, tqspi);
exit_free_master:
	spi_master_put(master);
	return ret;
}

static int tegra_qspi_remove(struct platform_device *pdev)
{
	struct spi_master *master = dev_get_drvdata(&pdev->dev);
	struct tegra_qspi_data	*tqspi = spi_master_get_devdata(master);

	free_irq(tqspi->irq, tqspi);
	spi_unregister_master(master);

	if (tqspi->tx_dma_chan)
		tegra_qspi_deinit_dma_param(tqspi, false);

	if (tqspi->rx_dma_chan)
		tegra_qspi_deinit_dma_param(tqspi, true);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_qspi_runtime_suspend(&pdev->dev);

	if (tqspi->clock_always_on)
		clk_disable_unprepare(tqspi->clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_qspi_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_qspi_data *tqspi = spi_master_get_devdata(master);
	int ret;

	ret = spi_master_suspend(master);

	if (tqspi->clock_always_on)
		clk_disable_unprepare(tqspi->clk);

	return ret;
}

static int tegra_qspi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_qspi_data *tqspi = spi_master_get_devdata(master);
	int ret;

	if (tqspi->clock_always_on) {
		ret = clk_prepare_enable(tqspi->clk);
		if (ret < 0) {
			dev_err(tqspi->dev, "clk_prepare failed: %d\n", ret);
			return ret;
		}
	}

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}
	tegra_qspi_writel(tqspi, tqspi->command1_reg, QSPI_COMMAND1);
	tegra_qspi_writel(tqspi, tqspi->def_command2_reg, QSPI_COMMAND2);
	pm_runtime_put(dev);

	return spi_master_resume(master);
}
#endif

static int tegra_qspi_runtime_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_qspi_data *tqspi = spi_master_get_devdata(master);

	/* Flush all write which are in PPSB queue by reading back */
	tegra_qspi_readl(tqspi, QSPI_COMMAND1);

	clk_disable_unprepare(tqspi->clk);
	return 0;
}

static int tegra_qspi_runtime_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tegra_qspi_data *tqspi = spi_master_get_devdata(master);
	int ret;

	ret = clk_prepare_enable(tqspi->clk);
	if (ret < 0) {
		dev_err(tqspi->dev, "clk_prepare failed: %d\n", ret);
		return ret;
	}
	return 0;
}

	static const struct dev_pm_ops tegra_qspi_pm_ops = {
		SET_RUNTIME_PM_OPS(tegra_qspi_runtime_suspend,
				tegra_qspi_runtime_resume, NULL)
			SET_SYSTEM_SLEEP_PM_OPS(tegra_qspi_suspend, tegra_qspi_resume)
	};
static struct platform_driver tegra_qspi_driver = {
	.driver = {
		.name		= "tegra210-qspi",
		.owner		= THIS_MODULE,
		.pm		= &tegra_qspi_pm_ops,
		.of_match_table	= of_match_ptr(tegra_qspi_of_match),
	},
	.probe =	tegra_qspi_probe,
	.remove =	tegra_qspi_remove,
};
module_platform_driver(tegra_qspi_driver);

MODULE_ALIAS("platform:tegra210-qspi");
MODULE_DESCRIPTION("NVIDIA Tegra210 QSPI Controller Driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>, Amlan Kundu <akundu@nvidia.com>");
MODULE_LICENSE("GPL v2");
