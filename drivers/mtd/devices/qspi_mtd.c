/*
 * MTD SPI driver for qspi flash chips
 *
 * Author: Mike Lavender, mike@steroidmicros.com
 * Copyright (c) 2005, Intec Automation Inc.
 * Copyright (c) 2013-2016, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mod_devicetable.h>

#include <linux/mtd/cfi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/of_platform.h>

#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mtd/qspi_mtd.h>

/*
 * NOTE: Below Macro is used to optimize the QPI/QUAD mode switch logic...
 * - QPI/QUAD mode is used for flash write. QUAD mode is used for flash read.
 * - When QPI is enabled, QUAD is don't care.
 * - If below macro is disabled...
 *  o QPI/QUAD mode is enabled/disabled at the start/end of each flash write
 *    function call.
 *  o QUAD mode is enabled/disabled at the start/end of each flash read
 *    function call.
 * - If below macro is enabled...
 *  o QPI/QUAD mode is enabled at the start of flash write. QPI/QUAD mode is
 *    disabled whenever erase is invoked. QPI mode is disabled on read.
 *  o QUAD mode is enabled at the start of flash read. QUAD mode is disabled
 *    whenever erase is invoked. QUAD is don't care in QPI mode.
 */
#define QMODE_SWITCH_OPTIMIZED

#define COMMAND_WIDTH				1
#define ADDRESS_WIDTH				4
#define WE_RETRY_COUNT				200
#define WIP_RETRY_COUNT				2000000
#define QUAD_ENABLE_WAIT_TIME			1000
#define WRITE_ENABLE_WAIT_TIME			10
#define WRITE_ENABLE_SLEEP_TIME			10
#define WIP_ENABLE_WAIT_TIME			10
#define WIP_ENABLE_SLEEP_TIME			50
#define BITS8_PER_WORD				8
#define BITS16_PER_WORD				16
#define BITS32_PER_WORD				32
#define RWAR_SR1NV				0x0
#define RWAR_CR1NV				0x2
#define RWAR_SR1V				0x00800000
#define RWAR_CR1V				0x00800002
#define RWAR_CR2V				0x00800003
#define RWAR_CR3V				0x00800004
#define WRAR					0x71
#define SR1NV_WRITE_DIS				(1<<7)
#define SR1NV_BLOCK_PROT			(0x7<<2)
#define CR3V_512PAGE_SIZE			(1<<4)

#define JEDEC_ID_S25FX512S	0x010220

static int qspi_write_en(struct qspi *flash,
		uint8_t is_enable, uint8_t is_sleep);
static int wait_till_ready(struct qspi *flash, uint8_t is_sleep);
static int qspi_read_any_reg(struct qspi *flash,
			uint32_t regaddr, uint8_t *pdata);

static inline struct qspi *mtd_to_qspi(struct mtd_info *mtd)
{
	return container_of(mtd, struct qspi, mtd);
}

/*
 * Set Mode for transfer request
 * Function sets Bus width, DDR/SDR and opcode
 */

static void set_mode(struct spi_transfer *tfr, uint8_t is_ddr,
		uint8_t bus_width, uint8_t op_code)
{
	tfr->delay_usecs = set_op_mode(op_code) | set_bus_width(bus_width);
	if (is_ddr)
		tfr->delay_usecs |= set_sdr_ddr;
}

/*
 * Copy Paramters from default command table
 * Command table contains command, address and data
 * related information associated with opcode
 */

static void copy_cmd_default(struct qcmdset *qcmd, struct qcmdset *cmd_table)
{
	qcmd->qcmd.op_code = cmd_table->qcmd.op_code;
	qcmd->qcmd.is_ddr = cmd_table->qcmd.is_ddr;
	qcmd->qcmd.bus_width = cmd_table->qcmd.bus_width;
	qcmd->qcmd.post_txn = cmd_table->qcmd.post_txn;
	qcmd->qaddr.address = cmd_table->qaddr.address;
	qcmd->qaddr.is_ddr = cmd_table->qaddr.is_ddr;
	qcmd->qaddr.len = cmd_table->qaddr.len;
	qcmd->qaddr.bus_width = cmd_table->qaddr.bus_width;
	qcmd->qaddr.dummy_cycles = cmd_table->qaddr.dummy_cycles;
	qcmd->qdata.is_ddr = cmd_table->qdata.is_ddr;
	qcmd->qdata.bus_width = cmd_table->qdata.bus_width;
}

/*
 * Copy Paramters from default command table
 * Command table contains command, address and data
 * related information associated with opcode
 */

static int read_sr1_reg(struct qspi *flash, uint8_t *regval)
{
	int status = PASS;

	status = qspi_read_any_reg(flash, RWAR_SR1V, regval);
	if (status) {
		pr_err("error: %s SR1V read failed: Status: x%x\n",
			__func__, status);
		return status;
	}

	return status;
}

/*
 * Function for WRAR command. Shall be called with
 * 1. flash->lock taken.
 * 2. WIP bit cleared
 * NOTE: Caller needs to poll for WIP
 */
static int qspi_write_any_reg(struct qspi *flash,
			uint32_t regaddr, uint8_t data)
{
	uint8_t cmd_addr_buf[4];
	struct spi_transfer t[3];
	struct spi_message m;
	struct qcmdset *cmd_table;
	int err;

	cmd_table = &cmd_info_table[WRITE_ANY_REG];

	err = qspi_write_en(flash, TRUE, FALSE);
	if (err) {
		pr_err("error: %s: WE failed: reg:x%x data:x%x, Status: x%x ",
			__func__, regaddr, data, err);
		return err;
	}

	spi_message_init(&m);
	memset(t, 0, sizeof(t));

	cmd_addr_buf[0] = cmd_table->qcmd.op_code;
	cmd_addr_buf[1] = (regaddr >> 16) & 0xFF;
	cmd_addr_buf[2] = (regaddr >> 8) & 0xFF;
	cmd_addr_buf[3] = regaddr & 0xFF;

	t[0].tx_buf = cmd_addr_buf;
	t[0].len = cmd_table->qaddr.len + 1;
	t[0].bits_per_word = BITS8_PER_WORD;
	set_mode(&t[0], cmd_table->qaddr.is_ddr,
		flash->curr_cmd_mode, cmd_table->qcmd.op_code);
	spi_message_add_tail(&t[0], &m);

	t[1].tx_buf = &data;
	t[1].len = 1;
	set_mode(&t[1], cmd_table->qdata.is_ddr,
		flash->curr_cmd_mode, cmd_table->qcmd.op_code);
	t[1].cs_change = TRUE;
	spi_message_add_tail(&t[1], &m);

	err = spi_sync(flash->spi, &m);
	if (err < 0) {
		pr_err("error: %s spi_sync Failed: reg:x%x dat:x%x sts:x%x\n",
			__func__, regaddr, data, err);
		return err;
	}

	return err;
}

/*
 * Function for RDAR command. Shall be called with
 * 1. flash->lock taken.
 * 2. WIP bit cleared
 */
static int qspi_read_any_reg(struct qspi *flash,
			uint32_t regaddr, uint8_t *pdata)
{
	uint8_t cmd_addr_buf[5];
	struct spi_transfer t[3];
	struct spi_message m;
	struct qcmdset *cmd_table;
	int err;

	cmd_table = &cmd_info_table[READ_ANY_REG];

	spi_message_init(&m);
	memset(t, 0, sizeof(t));

	cmd_addr_buf[0] = cmd_table->qcmd.op_code;
	cmd_addr_buf[1] = (regaddr >> 16) & 0xFF;
	cmd_addr_buf[2] = (regaddr >> 8) & 0xFF;
	cmd_addr_buf[3] = regaddr & 0xFF;

	t[0].len = 1;
	t[0].tx_buf = &cmd_addr_buf[0];
	set_mode(&t[0], cmd_table->qcmd.is_ddr,
		flash->curr_cmd_mode, cmd_table->qcmd.op_code);
	spi_message_add_tail(&t[0], &m);

	if (flash->curr_cmd_mode == X4)
		t[1].len = cmd_table->qaddr.len +
				4 * cmd_table->qaddr.dummy_cycles;
	else
		t[1].len = cmd_table->qaddr.len + cmd_table->qaddr.dummy_cycles;

	t[1].tx_buf = &cmd_addr_buf[1];
	set_mode(&t[1], cmd_table->qaddr.is_ddr,
		flash->curr_cmd_mode, cmd_table->qcmd.op_code);
	spi_message_add_tail(&t[1], &m);

	t[2].len = 1;
	t[2].rx_buf = pdata;
	set_mode(&t[2], cmd_table->qdata.is_ddr,
		flash->curr_cmd_mode, cmd_table->qcmd.op_code);
	/* in-activate the cs at the end */
	t[2].cs_change = TRUE;
	spi_message_add_tail(&t[2], &m);

	err = spi_sync(flash->spi, &m);
	if (err < 0) {
		pr_err("error: %s spi_sync call Failed: reg:x%x, Status: x%x ",
			__func__, regaddr, err);
		return err;
	}

	return 0;
}

/*
 * Enable/Disable QUAD flasg when QPI mode is disabled
 * Shall be called with...
 * 1. flash->lock taken.
 * 2. WIP bit cleared
 */
static int qspi_quad_flag_set(struct qspi *flash, uint8_t is_set)
{
	uint8_t regval;
	int status = PASS;

	pr_debug("%s: %s %d\n", dev_name(&flash->spi->dev), __func__, is_set);

	if ((flash->is_quad_set && is_set) ||
		(!flash->is_quad_set && !is_set)) {
		return status;
	}

	status = qspi_read_any_reg(flash, RWAR_CR1V, &regval);
	if (status) {
		pr_err("error: %s CR1V read failed: bset: %d, Status: x%x\n",
			__func__, is_set, status);
		return status;
	}

	if (is_set)
		regval |= 0x02;
	else
		regval &= ~0x02;

	status = qspi_write_any_reg(flash, RWAR_CR1V, regval);
	if (status) {
		pr_err("error: %s CR1V write failed: bset: %d, Status: x%x\n",
			__func__, is_set, status);
		return status;
	}

	status = wait_till_ready(flash, FALSE);
	if (status) {
		pr_err("error: %s: WIP failed: bset:%d, Status: x%x\n",
			__func__, is_set, status);
	}

	flash->is_quad_set = is_set;
	return status;
}

/*
 * Enable/ Disable Write Enable Bit in Configuration Register
 * Set WEL bit to 1 before Erase and Write Operations
 */

static int qspi_write_en(struct qspi *flash,
		uint8_t is_enable, uint8_t is_sleep)
{
	struct spi_transfer t[1];
	uint8_t cmd_buf[5], regval;
	int status = 0, err, tried = 0, comp;
	struct spi_message m;

	do {
		if (tried++ == WE_RETRY_COUNT) {
			pr_err("tired max times not changing WE bit\n");
			return FAIL;
		}
		memset(t, 0, sizeof(t));
		spi_message_init(&m);

		if (is_enable) {
			cmd_buf[0] = OPCODE_WRITE_ENABLE;
			comp = WEL_ENABLE;
		} else {
			cmd_buf[0] = OPCODE_WRITE_DISABLE;
			comp = WEL_DISABLE;
		}

		t[0].len = COMMAND_WIDTH;
		t[0].tx_buf = cmd_buf;
		t[0].bits_per_word = BITS8_PER_WORD;

		set_mode(&t[0], FALSE, flash->curr_cmd_mode,
				STATUS_READ);

		spi_message_add_tail(&t[0], &m);

		err = spi_sync(flash->spi, &m);
		if (err < 0) {
			pr_err("error: %s spi_sync call failed x%x",
				__func__, status);
			return 1;
		}

		if (is_sleep)
			msleep(WRITE_ENABLE_SLEEP_TIME);
		else
			udelay(WRITE_ENABLE_WAIT_TIME);

		status = read_sr1_reg(flash, &regval);
		if (status) {
			pr_err("error: %s: RDSR1 failed: Status: x%x ",
				__func__, status);
			return status;
		}
	} while ((regval & WEL_ENABLE) != comp);

	return status;
}

/*
 * Wait till flash is ready for erase/write operations.
 * Returns negative if error occurred.
 */

static int wait_till_ready(struct qspi *flash, uint8_t is_sleep)
{
	uint8_t regval, status = PASS;
	int tried = 0;

	do {
		if (tried++ == WIP_RETRY_COUNT) {
			pr_err("tired max times not changing WIP bit\n");
			return FAIL;
		}
		if ((tried % 20) == 0)
			pr_debug("Waiting in WIP iter: %d\n", tried);

		if (is_sleep)
			msleep(WIP_ENABLE_SLEEP_TIME);
		else
			udelay(WIP_ENABLE_WAIT_TIME);

		status = read_sr1_reg(flash, &regval);
		if (status) {
			pr_err("error: %s: RDSR1 failed: Status: x%x ",
				__func__, status);
			return status;
		}
	} while ((regval & WIP_ENABLE) == WIP_ENABLE);

	return status;
}

/*
 * Erase the whole flash memory
 *
 * Returns 0 if successful, non-zero otherwise.
 */

static int erase_chip(struct qspi *flash)
{
	uint8_t cmd_opcode;
	struct spi_transfer t[1];
	struct spi_message m;
	int status;

	pr_debug("%s: %s %lldKiB\n",
		dev_name(&flash->spi->dev), __func__,
		(long long)(flash->mtd.size >> 10));

	/* Send write enable, then erase commands. */
	status = qspi_write_en(flash, TRUE, TRUE);
	if (status) {
		pr_err("error: %s: WE failed: Status: x%x ", __func__, status);
		return status;
	}
	copy_cmd_default(&flash->cmd_table,
				&cmd_info_table[ERASE_BULK]);

	/* Set up command buffer. */
	cmd_opcode = OPCODE_CHIP_ERASE;

	spi_message_init(&m);
	memset(t, 0, sizeof(t));
	t[0].len = COMMAND_WIDTH;
	t[0].tx_buf = &cmd_opcode;
	t[0].cs_change = TRUE;
	set_mode(&t[0], FALSE,
		X1, cmd_opcode);

	spi_message_add_tail(&t[0], &m);
	status = spi_sync(flash->spi, &m);
	if (status < 0) {
		pr_err("error: %s spi_sync call failed x%x", __func__, status);
		return status;
	}

	status = wait_till_ready(flash, TRUE);
	if (status) {
		pr_err("error: %s: WIP failed: Status: x%x ", __func__, status);
		return status;
	}

	return 0;
}

/*
 * Erase one sector of flash memory at offset ``offset'' which is any
 * address within the sector which should be erased.
 *
 * Returns 0 if successful, non-zero otherwise.
 */
static int erase_sector(struct qspi *flash, u32 offset,
				uint8_t erase_opcode, u32 size)
{
	uint8_t cmd_addr_buf[5];
	struct spi_transfer t[1];
	struct spi_message m;
	int err = 0;

	pr_debug("%s: %s %dKiB at 0x%08x\n",
		dev_name(&flash->spi->dev),
		__func__, size / 1024, offset);

	/* Send write enable, then erase commands. */
	err = qspi_write_en(flash, TRUE, TRUE);
	if (err) {
		pr_err("error: %s: WE failed: Status: x%x ", __func__, err);
		return err;
	}
	copy_cmd_default(&flash->cmd_table,
				&cmd_info_table[ERASE_SECT]);

	/* Set up command buffer. */
	cmd_addr_buf[0] = erase_opcode;
	cmd_addr_buf[1] = (offset >> 24) & 0xFF;
	cmd_addr_buf[2] = (offset >> 16) & 0xFF;
	cmd_addr_buf[3] = (offset >> 8) & 0xFF;
	cmd_addr_buf[4] = offset & 0xFF;

	spi_message_init(&m);
	memset(t, 0, sizeof(t));

	t[0].len = (COMMAND_WIDTH + ADDRESS_WIDTH);
	t[0].tx_buf = cmd_addr_buf;

	t[0].cs_change = TRUE;
	set_mode(&t[0], FALSE,
		X1, flash->erase_opcode);

	spi_message_add_tail(&t[0], &m);

	err = spi_sync(flash->spi, &m);
	if (err < 0) {
		pr_err("error: %s spi_sync call failed x%x", __func__, err);
		return err;
	}

	err = wait_till_ready(flash, TRUE);
	if (err) {
		pr_err("error: %s: WIP failed: Status: x%x ", __func__, err);
		return err;
	}

	return 0;
}

/****************************************************************************/

/*
 * MTD Erase, Read and Write implementation
 */

/****************************************************************************/

/*
 * Erase an address range on the flash chip.  The address range may extend
 * one or more erase sectors.  Return an error is there is a problem erasing.
 */
static int qspi_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct qspi *flash = mtd_to_qspi(mtd);
	u32 addr, len;
	uint32_t rem;

	pr_debug("%s: %s at 0x%llx, len %lld\n",
		dev_name(&flash->spi->dev),
		__func__, (long long)instr->addr,
		(long long)instr->len);

	div_u64_rem(instr->len, mtd->erasesize, &rem);

	if (rem)
		return -EINVAL;

	addr = instr->addr;
	len = instr->len;

	mutex_lock(&flash->lock);
	/* whole-chip erase? */
	if (len == flash->mtd.size) {
		if (erase_chip(flash)) {
			instr->state = MTD_ERASE_FAILED;
			mutex_unlock(&flash->lock);
			return -EIO;
		}

		/* "sector"-at-a-time erase */
	} else {
		while (len) {
			if (erase_sector(flash, addr,
				flash->erase_opcode, mtd->erasesize)) {
				instr->state = MTD_ERASE_FAILED;
				mutex_unlock(&flash->lock);
				return -EIO;
			}
			/* Take care of subsectors erase if required */
			if (flash->flash_info->n_subsectors) {
				struct  flash_info *flinfo = flash->flash_info;
				u32 ssaddr = addr;
				u32 eaddr = min((addr + mtd->erasesize),
							flinfo->ss_endoffset);

				while ((ssaddr >= flinfo->ss_soffset) &&
					((ssaddr + flinfo->ss_size) <= eaddr)) {

					pr_debug("%s: Erasing subblock @ x%x, len x%x\n",
						dev_name(&flash->spi->dev),
						ssaddr, flinfo->ss_size);

					if (erase_sector(flash, ssaddr,
						flinfo->ss_erase_opcode,
							flinfo->ss_size)) {
						instr->state = MTD_ERASE_FAILED;
						mutex_unlock(&flash->lock);
						return -EIO;
					}
					ssaddr += flinfo->ss_size;
				}
			}
			addr += mtd->erasesize;
			len -= mtd->erasesize;
		}
	}

	mutex_unlock(&flash->lock);

	instr->state = MTD_ERASE_DONE;
	mtd_erase_callback(instr);

	return 0;
}

/*
 * Read an address range from the flash chip.  The address range
 * may be any size provided it is within the physical boundaries.
 */
static int qspi_read(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	struct qspi *flash = mtd_to_qspi(mtd);
	struct spi_transfer t[3];
	struct spi_message m;
	uint8_t merge_cmd_addr = FALSE;
	uint8_t cmd_addr_buf[5];
	int err;
	struct tegra_qspi_device_controller_data *cdata
				= flash->spi->controller_data;

	pr_debug("%s: %s from 0x%08x, len %zd\n",
		dev_name(&flash->spi->dev),
		__func__, (u32)from, len);

	spi_message_init(&m);
	memset(t, 0, sizeof(t));

	/* take lock here to protect race condition
	 * in case of concurrent read and write with
	 * different cmd_mode selection.
	 */
	mutex_lock(&flash->lock);

	/* Set Controller data Parameters
	 * Set DDR/SDR, X1/X4 and Dummy Cycles from DT
	 */

	if (cdata) {
		if (len > cdata->x1_len_limit) {
			if (cdata->x4_is_ddr) {
				copy_cmd_default(&flash->cmd_table,
					&cmd_info_table[DDR_QUAD_IO_READ]);
			} else {
				copy_cmd_default(&flash->cmd_table,
					&cmd_info_table[QUAD_IO_READ]);
			}
		} else {
			copy_cmd_default(&flash->cmd_table,
					&cmd_info_table[FAST_READ]);
		}
	} else {
		copy_cmd_default(&flash->cmd_table,
					&cmd_info_table[QUAD_IO_READ]);
	}

	/* check if possible to merge cmd and address */
	if ((flash->cmd_table.qcmd.is_ddr ==
		flash->cmd_table.qaddr.is_ddr) &&
		(flash->cmd_table.qcmd.bus_width ==
		flash->cmd_table.qaddr.bus_width) &&
		flash->cmd_table.qcmd.post_txn > 0) {

		merge_cmd_addr = TRUE;
		flash->cmd_table.qcmd.post_txn =
			flash->cmd_table.qcmd.post_txn - 1;
	}
	cmd_addr_buf[0] = flash->cmd_table.qcmd.op_code;
	cmd_addr_buf[1] = (from >> 24) & 0xFF;
	cmd_addr_buf[2] = (from >> 16) & 0xFF;
	cmd_addr_buf[3] = (from >> 8) & 0xFF;
	cmd_addr_buf[4] = from & 0xFF;

	if (merge_cmd_addr) {

		t[0].len = (flash->cmd_table.qaddr.len + 1
			+ (flash->cmd_table.qaddr.dummy_cycles/8));
		t[0].tx_buf = cmd_addr_buf;

		set_mode(&t[0],
			flash->cmd_table.qcmd.is_ddr,
			flash->cmd_table.qcmd.bus_width,
			flash->cmd_table.qcmd.op_code);
		spi_message_add_tail(&t[0], &m);

		t[1].len = len;
		t[1].rx_buf = buf;
		set_mode(&t[1],
			flash->cmd_table.qdata.is_ddr,
			flash->cmd_table.qdata.bus_width,
			flash->cmd_table.qcmd.op_code);

		/* in-activate the cs at the end */
		t[1].cs_change = TRUE;
		spi_message_add_tail(&t[1], &m);

	} else {

		t[0].len = 1;
		t[0].tx_buf = &cmd_addr_buf[0];

		set_mode(&t[0],
			flash->cmd_table.qcmd.is_ddr,
			flash->cmd_table.qcmd.bus_width,
			flash->cmd_table.qcmd.op_code);

		spi_message_add_tail(&t[0], &m);

		t[1].len = ((flash->cmd_table.qaddr.len +
			(flash->cmd_table.qaddr.dummy_cycles/8)));

		t[1].tx_buf = &cmd_addr_buf[1];

		set_mode(&t[1],
			flash->cmd_table.qaddr.is_ddr,
			flash->cmd_table.qaddr.bus_width,
			flash->cmd_table.qcmd.op_code);

		spi_message_add_tail(&t[1], &m);

		t[2].len = len;
		t[2].rx_buf = buf;
		set_mode(&t[2],
			flash->cmd_table.qdata.is_ddr,
			flash->cmd_table.qdata.bus_width,
			flash->cmd_table.qcmd.op_code);

		/* in-activate the cs at the end */
		t[2].cs_change = TRUE;
		spi_message_add_tail(&t[2], &m);
	}

	/* Enable QUAD bit before doing QUAD i/o operation */
	if (flash->cmd_table.qdata.bus_width == X4) {
		err = qspi_quad_flag_set(flash, TRUE);
		if (err) {
			mutex_unlock(&flash->lock);
			return err;
		}
	}

	spi_sync(flash->spi, &m);
	*retlen = m.actual_length -
		(flash->cmd_table.qaddr.len + 1 +
		(flash->cmd_table.qaddr.dummy_cycles/8));

	mutex_unlock(&flash->lock);
	return 0;
}

static int qspi_write(struct mtd_info *mtd, loff_t to, size_t len,
		size_t *retlen, const u_char *buf)
{
	struct qspi *flash = mtd_to_qspi(mtd);
	u32 page_offset, page_size;
	struct spi_transfer t[2];
	struct spi_message m;
	uint8_t cmd_addr_buf[5];
	uint8_t opcode;
	int err = 0;
	u32 offset = (unsigned long)to;
	struct tegra_qspi_device_controller_data *cdata =
					flash->spi->controller_data;

	pr_debug("%s: %s to 0x%08x, len %zd\n", dev_name(&flash->spi->dev),
			__func__, (u32)to, len);

	mutex_lock(&flash->lock);

	/* Set Controller data Parameters
	 * Set DDR/SDR, X1/X4 and Dummy Cycles from DT
	 */

	if (cdata) {
		if (len > cdata->x1_len_limit) {
			copy_cmd_default(&flash->cmd_table,
				&cmd_info_table[PAGE_PROGRAM]);
		} else {
			copy_cmd_default(&flash->cmd_table,
					&cmd_info_table[PAGE_PROGRAM]);
		}
	} else {
		copy_cmd_default(&flash->cmd_table,
				&cmd_info_table[PAGE_PROGRAM]);
	}

	cmd_addr_buf[0] = opcode = flash->cmd_table.qcmd.op_code;
	cmd_addr_buf[1] = (offset >> 24) & 0xFF;
	cmd_addr_buf[2] = (offset >> 16) & 0xFF;
	cmd_addr_buf[3] = (offset >> 8) & 0xFF;
	cmd_addr_buf[4] = offset & 0xFF;

	page_offset = offset & (flash->page_size - 1);

	/* do all the bytes fit onto one page? */
	if (page_offset + len <= flash->page_size) {

		spi_message_init(&m);
		memset(t, 0, sizeof(t));
		t[0].tx_buf = cmd_addr_buf;
		t[0].len = flash->cmd_table.qaddr.len + 1;
		t[0].bits_per_word = BITS8_PER_WORD;
		set_mode(&t[0], flash->cmd_table.qcmd.is_ddr,
			flash->cmd_table.qcmd.bus_width, opcode);

		spi_message_add_tail(&t[0], &m);

		t[1].tx_buf = buf;

		t[1].len = len;
		set_mode(&t[1], flash->cmd_table.qcmd.is_ddr,
			flash->cmd_table.qdata.bus_width, opcode);

		t[1].cs_change = TRUE;
		spi_message_add_tail(&t[1], &m);
		/* Wait until finished previous write command. */

		if (err) {
			pr_err("error: %s: QPI/QUAD set failed: Status: x%x ",
				__func__, err);
			mutex_unlock(&flash->lock);
			return err;
		}

		err = qspi_write_en(flash, TRUE, FALSE);
		if (err) {
			pr_err("error: %s: WE failed: Status: x%x ",
				__func__, err);
			goto clear_qmode;
		}

		spi_sync(flash->spi, &m);

		err = wait_till_ready(flash, FALSE);
		if (err) {
			pr_err("error: %s: WIP failed: Status: x%x ",
				__func__, err);
			goto clear_qmode;
		}

		*retlen = m.actual_length - (flash->cmd_table.qaddr.len + 1);
clear_qmode:

		mutex_unlock(&flash->lock);
		return err;
	} else {
		u32 i;
		spi_message_init(&m);
		memset(t, 0, sizeof(t));

		/* the size of data remaining on the first page */
		page_size = flash->page_size - page_offset;
		t[0].tx_buf = cmd_addr_buf;
		t[0].len = flash->cmd_table.qaddr.len + 1;

		t[0].bits_per_word = BITS8_PER_WORD;
		set_mode(&t[0], flash->cmd_table.qcmd.is_ddr,
			flash->cmd_table.qcmd.bus_width, opcode);

		spi_message_add_tail(&t[0], &m);

		t[1].tx_buf = buf;

		t[1].len = page_size;
		set_mode(&t[1], flash->cmd_table.qcmd.is_ddr,
			flash->cmd_table.qdata.bus_width, opcode);

		t[1].cs_change = TRUE;
		spi_message_add_tail(&t[1], &m);

		if (err) {
			pr_err("error: %s: QPI/QUAD set failed: Status: x%x ",
				__func__, err);
			mutex_unlock(&flash->lock);
			return 1;
		}

		err = qspi_write_en(flash, TRUE, FALSE);
		if (err) {
			pr_err("error: %s: WE failed: Status: x%x ",
				__func__, err);
			goto clear_qmode1;
		}

		spi_sync(flash->spi, &m);

		err = wait_till_ready(flash, FALSE);
		if (err) {
			pr_err("error: %s: WIP failed: Status: x%x ",
				__func__, err);
			goto clear_qmode1;
		}

		*retlen = m.actual_length - (flash->cmd_table.qaddr.len + 1);

		/* write everything in flash->page_size chunks */
		for (i = page_size; i < len; i += page_size) {
			page_size = len - i;
			if (page_size > flash->page_size)
				page_size = flash->page_size;
			/* Need to check for auto address increment */
			/* Check line no 584 to 597 code is required */
			spi_message_init(&m);
			memset(t, 0, sizeof(t));

			offset = to + i;

			/* write the next page to flash */
			cmd_addr_buf[0] = opcode =
				flash->cmd_table.qcmd.op_code;
			cmd_addr_buf[1] = (offset >> 24) & 0xFF;
			cmd_addr_buf[2] = (offset >> 16) & 0xFF;
			cmd_addr_buf[3] = (offset >> 8) & 0xFF;
			cmd_addr_buf[4] = offset & 0xFF;

			t[0].tx_buf = cmd_addr_buf;
			t[0].len = flash->cmd_table.qaddr.len + 1;

			t[0].bits_per_word = BITS8_PER_WORD;
			set_mode(&t[0],
				flash->cmd_table.qcmd.is_ddr,
				flash->cmd_table.qcmd.bus_width,
				opcode);

			spi_message_add_tail(&t[0], &m);

			t[1].tx_buf = (buf + i);
			t[1].len = page_size;
			set_mode(&t[1],
				flash->cmd_table.qcmd.is_ddr,
				flash->cmd_table.qdata.bus_width,
				opcode);

			t[1].cs_change = TRUE;
			spi_message_add_tail(&t[1], &m);

			err = qspi_write_en(flash, TRUE, FALSE);
			if (err) {
				pr_err("error: %s: WE failed: Status: x%x ",
					__func__, err);
				goto clear_qmode1;
			}

			spi_sync(flash->spi, &m);

			err = wait_till_ready(flash, FALSE);
			if (err) {
				pr_err("error: %s: WIP failed: Status: x%x ",
					__func__, err);
				goto clear_qmode1;
			}
			*retlen +=
				m.actual_length -
				(flash->cmd_table.qaddr.len + 1);
		}
clear_qmode1:
		mutex_unlock(&flash->lock);

		return err;
	}
}

static const struct spi_device_id *jedec_probe(struct spi_device *spi)
{
	int			tmp;
	u8			code = OPCODE_RDID;
	u8			id[5];
	u32			jedec;
	u16			ext_jedec;
	struct flash_info	*info;

	/* JEDEC also defines an optional "extended device information"
	 * string for after vendor-specific data, after the three bytes
	 * we use here.  Supporting some chips might require using it.
	 */
	tmp = spi_write_then_read(spi, &code, 1, id, 5);
	if (tmp < 0) {
		pr_debug("%s: error %d reading JEDEC ID\n",
				dev_name(&spi->dev), tmp);
		return ERR_PTR(tmp);
	}

	jedec = id[0];
	jedec = jedec << 8;
	jedec |= id[1];
	jedec = jedec << 8;
	jedec |= id[2];

	ext_jedec = id[3] << 8 | id[4];
	for (tmp = 0; tmp < ARRAY_SIZE(qspi_ids) - 1; tmp++) {
		info = (void *)qspi_ids[tmp].driver_data;

		if (info->jedec_id == jedec) {
			if (info->ext_id != 0 && info->ext_id != ext_jedec)
				continue;
			return &qspi_ids[tmp];
		}
	}
	pr_err("unrecognized JEDEC id %06x\n", jedec);
	return ERR_PTR(-ENODEV);
}

static int qspi_probe(struct spi_device *spi)
{
	const struct spi_device_id	*id;
	struct flash_platform_data	*data;
	struct qspi			*flash;
	struct flash_info		*info;
	unsigned			i;
	struct mtd_part_parser_data	ppdata;
	struct device_node __maybe_unused *np;
	struct tegra_qspi_device_controller_data *cdata = spi->controller_data;
	uint8_t regval;
	int status = PASS;

	id = spi_get_device_id(spi);
	np = spi->dev.of_node;

#ifdef CONFIG_MTD_OF_PARTS
	if (!of_device_is_available(np))
		return -ENODEV;
#endif

	data = spi->dev.platform_data;
	if (data && data->type) {
		const struct spi_device_id *plat_id;

		for (i = 0; i < ARRAY_SIZE(qspi_ids) - 1; i++) {
			plat_id = &qspi_ids[i];
			if (strcmp(data->type, plat_id->name))
				continue;
			break;
		}

		if (i < ARRAY_SIZE(qspi_ids) - 1)
			id = plat_id;
		else
			dev_warn(&spi->dev, "unrecognized id %s\n", data->type);
	}

	info = (void *)id->driver_data;

	if (info->jedec_id) {
		const struct spi_device_id *jid;

		jid = jedec_probe(spi);
		if (IS_ERR(jid)) {
			return PTR_ERR(jid);
		} else if (jid != id) {
			dev_warn(&spi->dev, "found %s, expected %s\n",
					jid->name, id->name);
			id = jid;
			info = (void *)jid->driver_data;
		}
	}

	flash = kzalloc(sizeof(*flash), GFP_KERNEL);
	if (!flash)
		return -ENOMEM;

	flash->spi = spi;
	flash->flash_info = info;
	mutex_init(&flash->lock);

	dev_set_drvdata(&spi->dev, flash);

	/*
	 * Atmel, SST and Intel/Numonyx serial flash tend to power
	 * up with the software protection bits set
	 */

	if (data && data->name)
		flash->mtd.name = data->name;
	else
		flash->mtd.name = dev_name(&spi->dev);

	flash->mtd.type = MTD_NORFLASH;
	flash->mtd.writesize = 1;
	flash->mtd.flags = MTD_CAP_NORFLASH;
	flash->mtd.size = info->sector_size * info->n_sectors;
	flash->mtd._erase = qspi_erase;
	flash->mtd._read = qspi_read;
	flash->mtd._write = qspi_write;
	flash->erase_opcode = OPCODE_SE;
	flash->mtd.erasesize = info->sector_size;

	ppdata.of_node = spi->dev.of_node;
	flash->mtd.dev.parent = &spi->dev;
	flash->page_size = info->page_size;
	flash->mtd.writebufsize = flash->page_size;

	flash->addr_width = ADDRESS_WIDTH;
	cdata = flash->spi->controller_data;

	if (info->n_subsectors) {
		info->ss_endoffset = info->ss_soffset +
					info->ss_size * info->n_subsectors;
		if (info->ss_endoffset > flash->mtd.size) {
			dev_err(&spi->dev, "%s SSErr %x %x %x %llx\n", id->name,
				info->n_subsectors, info->ss_soffset,
					info->ss_size, flash->mtd.size);
			return -EINVAL;
		}
		dev_info(&spi->dev, "%s SSG %x %x %x %llx\n", id->name,
				info->n_subsectors, info->ss_soffset,
					info->ss_size, flash->mtd.size);
	}

	dev_info(&spi->dev, "%s (%lld Kbytes)\n", id->name,
			(long long)flash->mtd.size >> 10);

	dev_info(&spi->dev,
	"mtd .name = %s, .size = 0x%llx (%lldMiB) .erasesize = 0x%.8x (%uKiB) .numeraseregions = %d\n",
		flash->mtd.name,
		(long long)flash->mtd.size, (long long)(flash->mtd.size >> 20),
		flash->mtd.erasesize, flash->mtd.erasesize / 1024,
		flash->mtd.numeraseregions);

	if (flash->mtd.numeraseregions)
		for (i = 0; i < flash->mtd.numeraseregions; i++)
			dev_info(&spi->dev,
			"mtd.eraseregions[%d] ={.offset = 0x%llx,.erasesize = 0x%.8x (%uKiB),.numblocks = %d}\n",
			i, (long long)flash->mtd.eraseregions[i].offset,
			flash->mtd.eraseregions[i].erasesize,
			flash->mtd.eraseregions[i].erasesize / 1024,
			flash->mtd.eraseregions[i].numblocks);

	/*
	 * FIXME: Unlock the flash if locked. It is WAR to unlock the flash
	 *	  as locked bit is setting unexpectedly
	 */
	status = qspi_read_any_reg(flash, RWAR_SR1NV, &regval);
	if (status) {
		pr_err("error: %s RWAR_CR2V read failed: Status: x%x ",
			__func__, status);
		return status;
	}

	if (regval & (SR1NV_WRITE_DIS | SR1NV_BLOCK_PROT)) {
		regval = regval & ~(SR1NV_WRITE_DIS | SR1NV_BLOCK_PROT);
		qspi_write_any_reg(flash, RWAR_SR1NV, regval);
		wait_till_ready(flash, FALSE);
	}

	/* Set 512 page size when s25fx512s */
	if ((info->jedec_id == JEDEC_ID_S25FX512S) && (info->page_size == 512)) {
		status = qspi_read_any_reg(flash, RWAR_CR3V, &regval);
		if (status) {
			pr_err("error: %s RWAR_CR3V read failed: Status: x%x ",
				__func__, status);
			return status;
		}

		if ((regval & CR3V_512PAGE_SIZE) == 0) {
			regval = regval | CR3V_512PAGE_SIZE;
			qspi_write_any_reg(flash, RWAR_CR3V, regval);
			wait_till_ready(flash, FALSE);
		}
	}

	/* partitions should match sector boundaries; and it may be good to
	 * use readonly partitions for writeprotected sectors (BP2..BP0).
	 */
	return mtd_device_parse_register(&flash->mtd, NULL, &ppdata,
			data ? data->parts : NULL,
			data ? data->nr_parts : 0);
}

static int qspi_remove(struct spi_device *spi)
{
	struct qspi	*flash = dev_get_drvdata(&spi->dev);
	int		status;

	/* Clean up MTD stuff. */
	status = mtd_device_unregister(&flash->mtd);
	if (status == 0)
		kfree(flash);
	return 0;
}

static struct spi_driver qspi_mtd_driver = {
	.driver = {
		.name	= "qspi_mtd",
		.owner	= THIS_MODULE,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.id_table	= qspi_ids,
	.probe	= qspi_probe,
	.remove	= qspi_remove,

};
module_spi_driver(qspi_mtd_driver);

MODULE_AUTHOR("Amlan Kundu <akundu@nvidia.com>, Ashutosh Patel <ashutoshp@nvidia.com>");
MODULE_DESCRIPTION("MTD SPI driver for Spansion/micron QSPI flash chips");
MODULE_LICENSE("GPL v2");
