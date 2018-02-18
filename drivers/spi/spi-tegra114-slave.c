/*
 * SPI slave driver for NVIDIA's Tegra114 SPI Controller.
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
#include <linux/reset.h>
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
#include <linux/spi/spi-tegra.h>
#include <linux/clk/tegra.h>

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

#define DATA_DIR_TX				(1 << 0)
#define DATA_DIR_RX				(1 << 1)

#define SPI_DMA_TIMEOUT				(msecs_to_jiffies(1000))
#define DEFAULT_SPI_DMA_BUF_LEN			(256*1024)
#define TX_FIFO_EMPTY_COUNT_MAX			SPI_TX_FIFO_EMPTY_COUNT(0x40)
#define RX_FIFO_FULL_COUNT_ZERO			SPI_RX_FIFO_FULL_COUNT(0)
#define SPI_DEFAULT_SPEED			25000000

#define MAX_CHIP_SELECT				4
#define SPI_FIFO_DEPTH				64
#define SPI_FIFO_FLUSH_MAX_DELAY		2000

struct tegra_spi_chip_data {
	bool intr_mask_reg;
	bool mask_cs_inactive_intr;
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
	bool					variable_length_transfer;
	bool					new_features;

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
};


static int tegra_spi_runtime_suspend(struct device *dev);
static int tegra_spi_runtime_resume(struct device *dev);
static int tegra_spi_validate_request(struct spi_device *spi,
			struct tegra_spi_data *tspi, struct spi_transfer *t);
static void tegra_spi_slave_dump_regs(struct tegra_spi_data *tspi);
static inline unsigned long tegra_spi_readl(struct tegra_spi_data *tspi,
		unsigned long reg)
{
	return readl(tspi->base + reg);
}

static inline void tegra_spi_writel(struct tegra_spi_data *tspi,
		unsigned long val, unsigned long reg)
{
	writel(val, tspi->base + reg);

	/* Read back register to make sure that register writes completed */
	if (reg != SPI_TX_FIFO)
		readl(tspi->base + SPI_COMMAND1);
}

int tegra_spi_slave_register_callback(struct spi_device *spi,
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
EXPORT_SYMBOL_GPL(tegra_spi_slave_register_callback);

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
	if (val & SPI_ERR)
		tegra_spi_writel(tspi, SPI_ERR | SPI_FIFO_ERROR,
				SPI_FIFO_STATUS);

	if (val & SPI_CS_INACTIVE)
		tegra_spi_writel(tspi, SPI_CS_INACTIVE,
				 SPI_FIFO_STATUS);

	if (val & SPI_FRAME_END)
		tegra_spi_writel(tspi, SPI_FRAME_END,
				 SPI_FIFO_STATUS);
}

static void tegra_spi_slave_dump_regs(struct tegra_spi_data *tspi)
{
	u32 command1_reg;
	u32 fifo_status_reg;
	u32 dma_ctrl_reg;
	u32 trans_status_reg;
	u32 dma_blk_size_reg;
	u32 intr_mask_reg;

	command1_reg = tegra_spi_readl(tspi, SPI_COMMAND1);
	fifo_status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	dma_ctrl_reg = tegra_spi_readl(tspi, SPI_DMA_CTL);
	trans_status_reg = tegra_spi_readl(tspi, SPI_TRANS_STATUS);
	dma_blk_size_reg = tegra_spi_readl(tspi, SPI_DMA_BLK);
	intr_mask_reg = tegra_spi_readl(tspi, SPI_INTR_MASK);

	dev_err(tspi->dev,
			"SPI-Slave_ERR: CMD_0: 0x%08x, FIFO_STS: 0x%08x\n",
			command1_reg, fifo_status_reg);
	dev_err(tspi->dev,
			"SPI-Slave_ERR: DMA_CTL: 0x%08x, TRANS_STS: 0x%08x\n",
			dma_ctrl_reg, trans_status_reg);
	dev_err(tspi->dev,
			"SPI-Slave_ERR: DMA_BLK_SIZE: 0x%08x, INTR_MASK: 0x%08x\n",
			dma_blk_size_reg, intr_mask_reg);

}
static unsigned tegra_spi_calculate_curr_xfer_param(
	struct spi_device *spi, struct tegra_spi_data *tspi,
	struct spi_transfer *t)
{
	unsigned remain_len = t->len - tspi->cur_pos;
	unsigned max_word;
	unsigned bits_per_word;
	unsigned max_len;
	unsigned total_fifo_words;

	bits_per_word = t->bits_per_word ? t->bits_per_word :
						spi->bits_per_word;
	tspi->bytes_per_word = (bits_per_word - 1) / 8 + 1;

	if (bits_per_word == 8 || bits_per_word == 16) {
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
	unsigned long fifo_status;
	unsigned max_n_32bit;
	unsigned i, count;
	unsigned long x;
	unsigned int written_words;
	unsigned fifo_words_left;
	u8 *tx_buf = (u8 *)t->tx_buf + tspi->cur_tx_pos;

	fifo_status = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	tx_empty_count = SPI_TX_FIFO_EMPTY_COUNT(fifo_status);

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
	unsigned int transfer_blk_count;
	unsigned i, count;
	unsigned long x;
	unsigned int read_words = 0;
	unsigned len;
	u8 *rx_buf = (u8 *)t->rx_buf + tspi->cur_rx_pos;

	fifo_status = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	rx_full_count = SPI_RX_FIFO_FULL_COUNT(fifo_status);

	transfer_blk_count = SPI_BLK_CNT(
			tegra_spi_readl(tspi, SPI_TRANS_STATUS));

	if (tspi->is_packed) {
		len = transfer_blk_count * tspi->bytes_per_word;
		WARN_ON((rx_full_count != (DIV_ROUND_UP(len, 4))));
		for (count = 0; count < rx_full_count; count++) {
			x = tegra_spi_readl(tspi, SPI_RX_FIFO);
			for (i = 0; len && (i < 4); i++, len--)
				*rx_buf++ = (x >> i*8) & 0xFF;
		}
		tspi->cur_rx_pos += transfer_blk_count * tspi->bytes_per_word;
		read_words += transfer_blk_count;
	} else {
		unsigned int bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
						tspi->cur_spi->bits_per_word;

		WARN_ON((rx_full_count != transfer_blk_count));
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
	unsigned long transfer_blk_count = SPI_BLK_CNT(
			tegra_spi_readl(tspi, SPI_TRANS_STATUS));
	if (!transfer_blk_count)
		transfer_blk_count = tspi->curr_dma_words;

	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(tspi->dev, tspi->rx_dma_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);

	if (tspi->is_packed) {
		len = transfer_blk_count * tspi->bytes_per_word;
		memcpy(t->rx_buf + tspi->cur_rx_pos, tspi->rx_dma_buf, len);
	} else {
		unsigned int i;
		unsigned int count;
		unsigned char *rx_buf = t->rx_buf + tspi->cur_rx_pos;
		unsigned int x;
		unsigned int rx_mask, bits_per_word;

		bits_per_word = t->bits_per_word ? t->bits_per_word :
						tspi->cur_spi->bits_per_word;
		rx_mask = (unsigned int)((1ULL << bits_per_word) - 1);
		for (count = 0; count < transfer_blk_count; count++) {
			x = tspi->rx_dma_buf[count];
			x &= rx_mask;
			for (i = 0; (i < tspi->bytes_per_word); i++)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
	}
	tspi->cur_rx_pos += transfer_blk_count * tspi->bytes_per_word;

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

static int tegra_spi_start_dma_based_transfer(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned long val;
	unsigned long intr_mask;
	unsigned int len;
	int ret = 0, maxburst;
	struct dma_slave_config dma_sconfig;
	unsigned int misc_reg = 0;

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
	val = 0;
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
	if (tspi->variable_length_transfer &&
		tspi->new_features &&
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
	} else {
		if (!tspi->new_features) {
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

		/* Programming guidelines provided by HW team
		If it is any intermediate chunk, then data is
		written to the memory once the DMA receives
		MC_BURST number of words, Set Maximum burst for
		variable length case */

		if (tspi->variable_length_transfer &&
			tspi->new_features)
			dma_sconfig.src_maxburst = 16;
		else
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
	tspi->is_curr_dma_xfer = true;
	tspi->dma_control_reg = val;

	val |= SPI_DMA_EN;
	tegra_spi_writel(tspi, val, SPI_DMA_CTL);

	/* After enabling DMA Bit Enable external clock bit */

	if (tspi->new_features) {
		misc_reg |= SPI_MISC_EXT_CLK_EN;
		tegra_spi_writel(tspi, misc_reg, SPI_MISC_REG);
	}
	/* Inform client that we are ready now. */
	if (tspi->spi_slave_ready_callback)
		tspi->spi_slave_ready_callback(tspi->client_data);

	return ret;
}

static int tegra_spi_start_cpu_based_transfer(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned long val;
	unsigned long intr_mask;
	unsigned cur_words;
	int ret = 0;
	unsigned int misc_reg = 0;

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

	/* All Tx data need to be in FIFO before enabling PIO */
	wmb();
	val |= SPI_PIO;
	tegra_spi_writel(tspi, val, SPI_COMMAND1);

	/* After enabling PIO Bit Enable external clock bit */

	if (tspi->new_features) {
		misc_reg |= SPI_MISC_EXT_CLK_EN;
		tegra_spi_writel(tspi, misc_reg, SPI_MISC_REG);
	}

	/* Inform client that we are ready now. */
	if (tspi->spi_slave_ready_callback)
		tspi->spi_slave_ready_callback(tspi->client_data);

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

	dma_sconfig.slave_id = tspi->dma_req_sel;
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

	if (tspi->is_packed) {
		if ((t->bits_per_word != 8) && (t->bits_per_word != 16)
				&& (t->bits_per_word != 32)) {
			dev_err(tspi->dev, "Packed mode does not support bpw = %d\n",
					t->bits_per_word);
			return -EINVAL;
		}
	} else {
		if (t->bits_per_word < 8) {
			dev_err(tspi->dev, "Unpacked mode does not support bpw = %d\n",
					t->bits_per_word);
			return -EINVAL;
		}
	}
	/* Check that the all words are available */
	if (t->len % tspi->bytes_per_word != 0)
		return -EINVAL;

	return 0;
}

static int tegra_spi_start_transfer_one(struct spi_device *spi,
		struct spi_transfer *t, bool is_first_of_msg,
		bool is_single_xfer)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	u32 speed;
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

	speed = speed * 3;
	speed = speed >> 1;

	if (speed != tspi->cur_speed) {
		ret = clk_set_rate(tspi->clk, speed);
		if (ret) {
			dev_err(tspi->dev, "Failed to set clk freq %d\n", ret);
			return -EINVAL;
		}
		tspi->cur_speed = speed;
	}

	tspi->cur_spi = spi;
	tspi->cur_pos = 0;
	tspi->cur_rx_pos = 0;
	tspi->cur_tx_pos = 0;
	tspi->curr_xfer = t;
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
		tspi->new_features = false;
		if (cdata && cdata->variable_length_transfer)
			tspi->variable_length_transfer = true;
		if (cdata && cdata->new_features)
			tspi->new_features = true;
	} else {
		command1 = tspi->command1_reg;
		command1 &= ~SPI_BIT_LENGTH(~0);
		command1 |= SPI_BIT_LENGTH(bits_per_word - 1);
	}

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
	/* Fix me Variable name new_features */
	ret = of_property_read_bool(data_np, "nvidia,new-features");
	if (ret)
		cdata->new_features = 1;

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

	BUG_ON(spi->chip_select >= MAX_CHIP_SELECT);

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

static  int tegra_spi_cs_low(struct spi_device *spi,
		bool state)
{
	struct tegra_spi_data *tspi = spi_master_get_devdata(spi->master);
	int ret;
	unsigned long val;
	unsigned long flags;
	unsigned int cs_pol_bit[MAX_CHIP_SELECT] = {
			SPI_CS_POL_INACTIVE_0,
			SPI_CS_POL_INACTIVE_1,
			SPI_CS_POL_INACTIVE_2,
			SPI_CS_POL_INACTIVE_3,
	};

	BUG_ON(spi->chip_select >= MAX_CHIP_SELECT);

	ret = pm_runtime_get_sync(tspi->dev);
	if (ret < 0) {
		dev_err(tspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}

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
	pm_runtime_put(tspi->dev);
	return 0;
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
	unsigned int misc_reg = 0;

	msg->status = 0;
	msg->actual_length = 0;

	ret = pm_runtime_get_sync(tspi->dev);
	if (ret < 0) {
		dev_err(tspi->dev, "runtime PM get failed: %d\n", ret);
		msg->status = ret;
		spi_finalize_current_message(master);
		return ret;
	}

	/* Fix me - Add sequence for CDC */
	reset_control_reset(tspi->rstc);
	if (tspi->new_features) {
		misc_reg &= SPI_MISC_EXT_CLK_EN;
		tegra_spi_writel(tspi, misc_reg, SPI_MISC_REG);
	}
	single_xfer = list_is_singular(&msg->transfers);
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		reinit_completion(&tspi->xfer_completion);
		ret = tegra_spi_start_transfer_one(spi, xfer,
					is_first_msg, single_xfer);
		if (ret < 0) {
			dev_err(tspi->dev,
				"spi can not start transfer, err %d\n", ret);
			goto exit;
		}
		is_first_msg = false;
		ret = wait_for_completion_killable(&tspi->xfer_completion);
		if (WARN_ON(ret != 0)) {
			dev_err(tspi->dev,
				"spi transfer timeout, err %d\n", ret);
			if (tspi->is_curr_dma_xfer &&
					(tspi->cur_direction & DATA_DIR_TX))
				dmaengine_terminate_all(tspi->tx_dma_chan);
			if (tspi->is_curr_dma_xfer &&
					(tspi->cur_direction & DATA_DIR_RX))
				dmaengine_terminate_all(tspi->rx_dma_chan);
			ret = -EIO;
			goto exit;
		}

		if (tspi->tx_status ||  tspi->rx_status) {
			dev_err(tspi->dev, "Error in Transfer\n");
			ret = -EIO;
			goto exit;
		}
		msg->actual_length += xfer->len;
		if (xfer->cs_change && xfer->delay_usecs) {
			tegra_spi_writel(tspi, tspi->def_command1_reg,
					SPI_COMMAND1);
			udelay(xfer->delay_usecs);
		}
	}
	ret = 0;
exit:
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	pm_runtime_put(tspi->dev);
	msg->status = ret;
	spi_finalize_current_message(master);
	return ret;
}

static void reset_controller(struct tegra_spi_data *tspi)
{

	reset_control_reset(tspi->rstc);
	/* restore default value */
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	/* set CS inactive b/w packets to mask CS_INACTIVE interrupts */
	if (tspi->chip_data->mask_cs_inactive_intr)
		tegra_spi_writel(tspi, 0, SPI_CS_TIMING2);
}

static irqreturn_t handle_cpu_based_xfer(struct tegra_spi_data *tspi)
{
	struct spi_transfer *t = tspi->curr_xfer;
	unsigned long flags;
	unsigned long transfer_blk_count =
			SPI_BLK_CNT(tegra_spi_readl(tspi, SPI_TRANS_STATUS));

	spin_lock_irqsave(&tspi->lock, flags);
	if (tspi->tx_status ||  tspi->rx_status) {
		dev_err(tspi->dev, "CpuXfer ERROR bit set 0x%x\n",
			tspi->status_reg);
		dev_err(tspi->dev, "CpuXfer 0x%08x:0x%08x\n",
			tspi->command1_reg, tspi->dma_control_reg);
		tegra_spi_slave_dump_regs(tspi);
		reset_controller(tspi);
		complete(&tspi->xfer_completion);
		goto exit;
	}

	if (tspi->cur_direction & DATA_DIR_RX)
		tegra_spi_read_rx_fifo_to_client_rxbuf(tspi, t);

	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->cur_pos += transfer_blk_count * tspi->bytes_per_word;
	else
		tspi->cur_pos = tspi->cur_rx_pos;

	/* If external master de-asserts the CS randomly
	and after that is SW wants to do a fresh transfer,
	SW needs to apply controller reset to flush the fifos
	and internal pipeline as controller might have
	fetched more data from memory by then. */

	if ((tspi->cur_pos == t->len) ||
		(IS_SPI_CS_INACTIVE(tspi->status_reg)
			&& tspi->variable_length_transfer)) {
		complete(&tspi->xfer_completion);
		t->len = tspi->cur_pos;
		if ((IS_SPI_CS_INACTIVE(tspi->status_reg))
			&& tspi->variable_length_transfer
			&& tspi->new_features
			&& (tspi->cur_direction & DATA_DIR_TX)) {
				reset_controller(tspi);
		}

		goto exit;
	}
	tegra_spi_calculate_curr_xfer_param(tspi->cur_spi, tspi, t);
	tegra_spi_start_cpu_based_transfer(tspi, t);
exit:
	spin_unlock_irqrestore(&tspi->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t handle_dma_based_xfer(struct tegra_spi_data *tspi)
{
	struct spi_transfer *t = tspi->curr_xfer;
	long wait_status;
	int err = 0;
	unsigned total_fifo_words;
	unsigned long flags;
	unsigned pending_rx_word_count;
	unsigned long fifo_status, word_tfr_status, actual_word_tfr;
	unsigned long transfer_blk_count =
		SPI_BLK_CNT(tegra_spi_readl(tspi, SPI_TRANS_STATUS));

	if (!transfer_blk_count)
		transfer_blk_count = tspi->curr_dma_words;

	/* Abort dmas if any error */
	if (tspi->cur_direction & DATA_DIR_TX) {
		if (tspi->tx_status) {
			dmaengine_terminate_all(tspi->tx_dma_chan);
			err += 1;
		} else {
			if ((IS_SPI_CS_INACTIVE(tspi->status_reg)) &&
				tspi->variable_length_transfer) {
				dmaengine_terminate_all(tspi->tx_dma_chan);
				async_tx_ack(tspi->tx_dma_desc);
			} else {
				wait_status =
				wait_for_completion_interruptible_timeout(
						&tspi->tx_dma_complete,
						SPI_DMA_TIMEOUT);

				if (wait_status <= 0) {
					dmaengine_terminate_all(
						tspi->tx_dma_chan);
					dev_err(tspi->dev, "TxDma Xfer failed\n");
					err += 1;
				}
			}
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		if (tspi->rx_status) {
			dmaengine_terminate_all(tspi->rx_dma_chan);
			err += 2;
		} else {
			if ((IS_SPI_CS_INACTIVE(tspi->status_reg)) &&
				tspi->variable_length_transfer) {
				dmaengine_terminate_all(tspi->rx_dma_chan);
				async_tx_ack(tspi->rx_dma_desc);

				fifo_status =
					tegra_spi_readl(tspi, SPI_FIFO_STATUS);

				pending_rx_word_count =
					SPI_RX_FIFO_FULL_COUNT(fifo_status);

				word_tfr_status =
					DIV_ROUND_UP(transfer_blk_count ,
						tspi->words_per_32bit);

				actual_word_tfr =
					word_tfr_status - pending_rx_word_count;

				while (pending_rx_word_count > 0) {

					tspi->rx_dma_buf[actual_word_tfr] =
						tegra_spi_readl(tspi,
							SPI_RX_FIFO);

					actual_word_tfr++;
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
					dev_err(tspi->dev, "RxDma Xfer failed\n");
					err += 2;
				}
			}
		}
	}

	spin_lock_irqsave(&tspi->lock, flags);
	if (err) {
		dev_err(tspi->dev, "DmaXfer: ERROR bit set 0x%x\n",
			tspi->status_reg);
		dev_err(tspi->dev, "DmaXfer 0x%08x:0x%08x\n",
			tspi->command1_reg, tspi->dma_control_reg);

		tegra_spi_slave_dump_regs(tspi);
		reset_controller(tspi);
		complete(&tspi->xfer_completion);
		spin_unlock_irqrestore(&tspi->lock, flags);
		return IRQ_HANDLED;
	}

	if (tspi->cur_direction & DATA_DIR_RX)
		tegra_spi_copy_spi_rxbuf_to_client_rxbuf(tspi, t);

	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->cur_pos += transfer_blk_count * tspi->bytes_per_word;
	else
		tspi->cur_pos = tspi->cur_rx_pos;

	/* If external master de-asserts the CS randomly
	and after that is SW wants to do a fresh transfer,
	SW needs to apply controller reset to flush the fifos
	and internal pipeline as controller might have
	fetched more data from memory by then. */

	if ((tspi->cur_pos == t->len) ||
		(IS_SPI_CS_INACTIVE(tspi->status_reg) &&
			tspi->variable_length_transfer)) {
		t->len = tspi->cur_pos;
		complete(&tspi->xfer_completion);
		/* WAR */
		if ((IS_SPI_CS_INACTIVE(tspi->status_reg))
			&& tspi->variable_length_transfer
			&& tspi->new_features
			&& (tspi->cur_direction & DATA_DIR_TX)) {
				reset_controller(tspi);
		}
		goto exit;
	}

	/* Continue transfer in current message */
	total_fifo_words = tegra_spi_calculate_curr_xfer_param(tspi->cur_spi,
							tspi, t);
	if (total_fifo_words > SPI_FIFO_DEPTH)
		err = tegra_spi_start_dma_based_transfer(tspi, t);
	else
		err = tegra_spi_start_cpu_based_transfer(tspi, t);

exit:
	spin_unlock_irqrestore(&tspi->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t tegra_spi_isr_thread(int irq, void *context_data)
{
	struct tegra_spi_data *tspi = context_data;

	if (!tspi->is_curr_dma_xfer)
		return handle_cpu_based_xfer(tspi);
	return handle_dma_based_xfer(tspi);
}

static irqreturn_t tegra_spi_isr(int irq, void *context_data)
{
	struct tegra_spi_data *tspi = context_data;

	/* Inform client about controller interrupt. */
	if (tspi->spi_slave_isr_callback)
		tspi->spi_slave_isr_callback(tspi->client_data);

	tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->tx_status = tspi->status_reg &
					(SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF);

	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->rx_status = tspi->status_reg &
					(SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF);
	/* cs inactive intr while it is masked, mark as error */
	if (tspi->chip_data->mask_cs_inactive_intr &&
			(tspi->status_reg & SPI_CS_INACTIVE)) {
		dev_err(tspi->dev, "cs inactive intr, status_reg = 0x%x\n",
				tspi->status_reg);
		tspi->tx_status = SPI_TX_FIFO_UNF;
		tspi->rx_status = SPI_RX_FIFO_OVF;
	}

	if (!(tspi->cur_direction & DATA_DIR_TX) &&
			!(tspi->cur_direction & DATA_DIR_RX))
		dev_err(tspi->dev, "spurious interrupt, status_reg = 0x%x\n",
				tspi->status_reg);

	tegra_spi_clear_status(tspi);

	return IRQ_WAKE_THREAD;
}

static struct tegra_spi_platform_data *tegra_spi_parse_dt(
		struct platform_device *pdev)
{
	struct tegra_spi_platform_data *pdata;
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
		pdata->spi_max_frequency = be32_to_cpup(prop);

	if (of_find_property(np, "nvidia,clock-always-on", NULL))
		pdata->is_clkon_always = true;

	return pdata;
}

static struct tegra_spi_chip_data tegra114_spi_chip_data = {
	.intr_mask_reg = false,
	.mask_cs_inactive_intr = true, /* mask cs intr for T114/T124 */
};

static struct tegra_spi_chip_data tegra210_spi_chip_data = {
	.intr_mask_reg = true,
	.mask_cs_inactive_intr = false,
};
static struct tegra_spi_chip_data tegra186_spi_chip_data = {
	.intr_mask_reg = true,
	.mask_cs_inactive_intr = false,
};
static struct of_device_id tegra_spi_of_match[] = {
	{
		.compatible = "nvidia,tegra114-spi-slave",
		.data       = &tegra114_spi_chip_data,
	}, {
		.compatible = "nvidia,tegra210-spi-slave-deprecated",
		.data       = &tegra210_spi_chip_data,
	}, {
		.compatible = "nvidia,tegra186-spi-slave-deprecated",
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
	const struct of_device_id *match;
	const struct tegra_spi_chip_data *chip_data = &tegra114_spi_chip_data;
	int ret = 0, spi_irq;
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
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSBYTE_FE;
	master->setup = tegra_spi_setup;
	master->transfer_one_message = tegra_spi_transfer_one_message;
	master->num_chipselect = MAX_CHIP_SELECT;
	master->bus_num = bus_num;
	master->spi_cs_low  = tegra_spi_cs_low;

	dev_set_drvdata(&pdev->dev, master);
	tspi = spi_master_get_devdata(master);
	tspi->master = master;
	tspi->dma_req_sel = pdata->dma_req_sel;
	tspi->clock_always_on = pdata->is_clkon_always;
	tspi->dev = &pdev->dev;

	if (pdev->dev.of_node) {
		match = of_match_device(tegra_spi_of_match,
				&pdev->dev);
		if (match)
			chip_data = match->data;
	}
	tspi->chip_data = chip_data;

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
	tspi->rstc = devm_reset_control_get(&pdev->dev, "spi-slave-reset");
	if (IS_ERR(tspi->rstc)) {
		dev_err(&pdev->dev, "Reset control is not found: %d\n", ret);
		return PTR_ERR(tspi->rstc);
	}

	spi_irq = platform_get_irq(pdev, 0);
	tspi->irq = spi_irq;
	ret = request_threaded_irq(tspi->irq, tegra_spi_isr,
			tegra_spi_isr_thread, IRQF_ONESHOT,
			dev_name(&pdev->dev), tspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
					tspi->irq);
		goto exit_free_master;
	}

	tspi->clk = devm_clk_get(&pdev->dev, "spi-slave");
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
			dev_err(&pdev->dev, "RxDma Init failed, err %d\n", ret);
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
		.name		= "spi-tegra114-slave",
		.owner		= THIS_MODULE,
		.pm		= &tegra_spi_pm_ops,
		.of_match_table	= of_match_ptr(tegra_spi_of_match),
	},
	.probe =	tegra_spi_probe,
	.remove =	tegra_spi_remove,
};
module_platform_driver(tegra_spi_driver);

MODULE_ALIAS("platform:spi-tegra114-slave");
MODULE_DESCRIPTION("NVIDIA Tegra114 SPI slave Controller Driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>, Amlan Kundu <akundu@nvidia.com>");
MODULE_LICENSE("GPL v2");
