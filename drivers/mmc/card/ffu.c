/*
 * *  ffu.c
 *
 *  Copyright 2007-2008 Pierre Ossman
 *
 *  Modified by SanDisk Corp., Copyright © 2013 SanDisk Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program includes bug.h, card.h, host.h, mmc.h, scatterlist.h,
 * slab.h, ffu.h & swap.h header files
 * The original, unmodified version of this program – the mmc_test.c
 * file – is obtained under the GPL v2.0 license that is available via
 * http://www.gnu.org/licenses/,
 * or http://www.opensource.org/licenses/gpl-2.0.php
*/

#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/mmc/ffu.h>
#include <linux/firmware.h>

/**
 * struct mmc_ffu_pages - pages allocated by 'alloc_pages()'.
 * @page: first page in the allocation
 * @order: order of the number of pages allocated
 */
struct mmc_ffu_pages {
	struct page *page;
	unsigned int order;
};

/**
 * struct mmc_ffu_mem - allocated memory.
 * @arr: array of allocations
 * @cnt: number of allocations
 */
struct mmc_ffu_mem {
	struct mmc_ffu_pages *arr;
	unsigned int cnt;
};

struct mmc_ffu_area {
	unsigned long max_sz;
	unsigned int max_tfr;
	unsigned int max_segs;
	unsigned int max_seg_sz;
	unsigned int blocks;
	unsigned int sg_len;
	struct mmc_ffu_mem *mem;
	struct scatterlist *sg;
};

static void mmc_ffu_prepare_mrq(struct mmc_card *card,
	struct mmc_request *mrq, struct scatterlist *sg, unsigned int sg_len,
	u32 arg, unsigned int blocks, unsigned int blksz)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data || !mrq->stop);

	if (blocks > 1)
		mrq->cmd->opcode = MMC_WRITE_MULTIPLE_BLOCK;
	else
		mrq->cmd->opcode = MMC_WRITE_BLOCK;

	mrq->cmd->arg = arg;
	if (!mmc_card_blockaddr(card))
		mrq->cmd->arg <<= 9;

	mrq->cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	if (blocks == 1) {
		mrq->stop = NULL;
	} else {
		mrq->stop->opcode = MMC_STOP_TRANSMISSION;
		mrq->stop->arg = 0;
		mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	}

	mrq->data->blksz = blksz;
	mrq->data->blocks = blocks;
	mrq->data->flags = MMC_DATA_WRITE;
	mrq->data->sg = sg;
	mrq->data->sg_len = sg_len;

	mmc_set_data_timeout(mrq->data, card);
}

/*
 * Checks that a normal transfer didn't have any errors
 */
static int mmc_ffu_check_result(struct mmc_request *mrq)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data);

	if (mrq->cmd->error != 0)
		return -EINVAL;

	if (mrq->data->error != 0)
		return -EINVAL;

	if (mrq->stop != NULL && mrq->stop->error != 0)
		return -1;

	if (mrq->data->bytes_xfered != (mrq->data->blocks * mrq->data->blksz))
		return -EINVAL;

	return 0;
}

static int mmc_ffu_busy(struct mmc_command *cmd)
{
	return !(cmd->resp[0] & R1_READY_FOR_DATA) ||
		(R1_CURRENT_STATE(cmd->resp[0]) == R1_STATE_PRG);
}

static int mmc_ffu_wait_busy(struct mmc_card *card)
{
	int ret, busy = 0;
	struct mmc_command cmd = {0};

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	do {
		ret = mmc_wait_for_cmd(card->host, &cmd, 0);
		if (ret)
			break;

		if (!busy && mmc_ffu_busy(&cmd)) {
			busy = 1;
			if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY) {
				pr_warn("%s: Warning: Host did not wait for busy state to end.\n",
					mmc_hostname(card->host));
			}
		}

	} while (mmc_ffu_busy(&cmd));

	return ret;
}

/*
 * transfer with certain parameters
 */
static int mmc_ffu_simple_transfer(struct mmc_card *card,
	struct scatterlist *sg, unsigned int sg_len, u32 arg,
	unsigned int blocks, unsigned int blksz)
{
	struct mmc_request mrq = {0};
	struct mmc_command cmd = {0};
	struct mmc_command stop = {0};
	struct mmc_data data = {0};

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;
	mmc_ffu_prepare_mrq(card, &mrq, sg, sg_len, arg, blocks, blksz);
	mmc_wait_for_req(card->host, &mrq);

	mmc_ffu_wait_busy(card);

	return mmc_ffu_check_result(&mrq);
}

/*
 * Map memory into a scatterlist.
 */
static unsigned int mmc_ffu_map_sg(struct mmc_ffu_mem *mem, int size,
	struct scatterlist *sglist)
{
	struct scatterlist *sg = sglist;
	unsigned int i;
	unsigned long sz = size;
	unsigned int sctr_len = 0;
	unsigned long len;

	sg_init_table(sglist, mem->cnt);

	for (i = 0; i < mem->cnt && sz; i++, sz -= len) {
		len = PAGE_SIZE * (1 << mem->arr[i].order);

		if (len > sz) {
			len = sz;
			sz = 0;
		}

		sg_set_page(sg, mem->arr[i].page, len, 0);
		sg = sg_next(sg);
		sctr_len += 1;
	}

	return sctr_len;
}

static void mmc_ffu_free_mem(struct mmc_ffu_mem *mem)
{
	if (!mem)
		return;

	while (mem->cnt--)
		__free_pages(mem->arr[mem->cnt].page, mem->arr[mem->cnt].order);

	kfree(mem->arr);
	kfree(mem);
}

/*
 * Cleanup struct mmc_ffu_area.
 */
static void mmc_ffu_area_cleanup(struct mmc_ffu_area *area)
{
	kfree(area->sg);
	mmc_ffu_free_mem(area->mem);
}

/*
 * Allocate a lot of memory, preferably max_sz but at least min_sz. In case
 * there isn't much memory do not exceed 1/16th total low mem pages. Also do
 * not exceed a maximum number of segments and try not to make segments much
 * bigger than maximum segment size.
 */
static struct mmc_ffu_mem *mmc_ffu_alloc_mem(unsigned long min_sz,
	unsigned long max_sz, unsigned int max_segs, unsigned int max_seg_sz)
{
	unsigned long max_page_cnt = DIV_ROUND_UP(max_sz, PAGE_SIZE);
	unsigned long min_page_cnt = DIV_ROUND_UP(min_sz, PAGE_SIZE);
	unsigned long max_seg_page_cnt = DIV_ROUND_UP(max_seg_sz, PAGE_SIZE);
	unsigned long page_cnt = 0;
	/* we divide by 16 to ensure we will not allocate a big amount
	 * of unnecessary pages */
	unsigned long limit = nr_free_buffer_pages() >> 4;
	struct mmc_ffu_mem *mem;
	gfp_t flags = GFP_KERNEL | GFP_DMA | __GFP_NOWARN | __GFP_NORETRY;

	if (max_page_cnt > limit)
		max_page_cnt = limit;

	if (min_page_cnt > max_page_cnt)
		min_page_cnt = max_page_cnt;

	if (max_segs * max_seg_page_cnt > max_page_cnt)
		max_segs = DIV_ROUND_UP(max_page_cnt, max_seg_page_cnt);

	mem = kzalloc(sizeof(struct mmc_ffu_mem), GFP_KERNEL);
	if (!mem)
		return NULL;

	mem->arr = kzalloc(sizeof(struct mmc_ffu_pages) * max_segs,
		GFP_KERNEL);
	if (!mem->arr)
		goto out_free;

	while (max_page_cnt) {
		struct page *page;
		unsigned int order;

		order = get_order(max_seg_page_cnt << PAGE_SHIFT);

		do {
			page = alloc_pages(flags, order);
		} while (!page && order--);

		if (!page)
			goto out_free;

		mem->arr[mem->cnt].page = page;
		mem->arr[mem->cnt].order = order;
		mem->cnt += 1;
		page_cnt += 1UL << order;
		if (max_page_cnt <= (1UL << order))
			break;
		max_page_cnt -= 1UL << order;
	}

	if (page_cnt < min_page_cnt)
		goto out_free;

	return mem;

out_free:
	mmc_ffu_free_mem(mem);
	return NULL;
}

/*
 * Initialize an area for data transfers.
 * Copy the data to the allocated pages.
 */
static int mmc_ffu_area_init(struct mmc_ffu_area *area, struct mmc_card *card,
	const u8 *data, int size)
{
	int ret;
	int i;
	int length = 0;
	int min_size = 0;

	area->max_tfr = size;

	/* Try to allocate enough memory for a max. sized transfer. Less is OK
	 * because the same memory can be mapped into the scatterlist more than
	 * once. Also, take into account the limits imposed on scatterlist
	 * segments by the host driver.
	 */
	area->mem = mmc_ffu_alloc_mem(1, area->max_tfr, area->max_segs,
		area->max_seg_sz);
	if (!area->mem)
		return -ENOMEM;

	/* copy data to page */
	for (i = 0; i < area->mem->cnt; i++) {
		if (length > size) {
			ret = -EINVAL;
			goto out_free;
		}
		min_size = min(size - length, (int)area->max_seg_sz);
		memcpy(page_address(area->mem->arr[i].page), data + length,
			min_size);
		length += min_size;
	}

	area->sg = kzalloc(sizeof(struct scatterlist) * area->mem->cnt,
		GFP_KERNEL);
	if (!area->sg) {
		ret = -ENOMEM;
		goto out_free;
	}

	area->sg_len = mmc_ffu_map_sg(area->mem, size, area->sg);

	return 0;

out_free:
	mmc_ffu_area_cleanup(area);
	return ret;
}

static int mmc_ffu_write(struct mmc_card *card, const u8 *src, u32 arg,
	int size)
{
	int rc;
	struct mmc_ffu_area area = {0};
	int max_tfr;

	area.max_segs = card->host->max_segs;
	area.max_seg_sz = card->host->max_seg_size & ~(CARD_BLOCK_SIZE - 1);

	do {
		max_tfr = size;
		if (max_tfr >> 9 > card->host->max_blk_count)
			max_tfr = card->host->max_blk_count << 9;
		if (max_tfr > card->host->max_req_size)
			max_tfr = card->host->max_req_size;
		if (DIV_ROUND_UP(max_tfr, area.max_seg_sz) > area.max_segs)
			max_tfr = area.max_segs * area.max_seg_sz;
		if ((max_tfr > card->host->max_seg_size) &&
			((max_tfr % card->host->max_seg_size) != 0))
			max_tfr = area.max_seg_sz;

		rc = mmc_ffu_area_init(&area, card, src, max_tfr);
		if (rc != 0)
			goto exit;

		rc = mmc_ffu_simple_transfer(card, area.sg, area.sg_len, arg,
			max_tfr / CARD_BLOCK_SIZE, CARD_BLOCK_SIZE);
		if (rc != 0) {
			pr_err("%s mmc_ffu_simple_transfer %d\n", __func__, rc);
			goto exit;
		}
		src += max_tfr;
		size -= max_tfr;
		mmc_ffu_area_cleanup(&area);
	} while (size > 0);

	return 0;

exit:
	mmc_ffu_area_cleanup(&area);
	return rc;
}

/* Flush all scheduled work from the MMC work queue.
 * and initialize the MMC device */
static int mmc_ffu_restart(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = 0;

	mmc_flush_cache(host);
	err = mmc_power_save_host(host);
	if (err) {
		pr_warn("%s: going to sleep failed (%d)!!!\n",
			__func__ , err);
		goto exit;
	}

	err = mmc_power_restore_host(host);

exit:

	return err;
}

int mmc_ffu_download(struct mmc_card *card, const char *name)
{
	u8 ext_csd[CARD_BLOCK_SIZE];
	int err;
	int ret;
	u32 arg;
	u32 fw_prog_bytes;
	const struct firmware *fw;

	/* Check if FFU is supported */
	if (!card->ext_csd.ffu_capable) {
		pr_err("FFU: %s: error FFU is not supported %d rev %d\n",
			mmc_hostname(card->host), card->ext_csd.ffu_capable,
			card->ext_csd.rev);
		return -EOPNOTSUPP;
	}

	if (strlen(name) > 512) {
		pr_err("FFU: %s: %.20s is not a valid argument\n",
			mmc_hostname(card->host), name);
		return -EINVAL;
	}

	/* setup FW data buffer */
	err = request_firmware(&fw, name, &card->dev);
	if (err) {
		pr_err("FFU: %s: Firmware request failed %d\n",
			mmc_hostname(card->host), err);
		return err;
	}
	if ((fw->size % CARD_BLOCK_SIZE)) {
		pr_warn("FFU: %s: Warning %zd firmware data size is not aligned!!!\n",
			mmc_hostname(card->host), fw->size);
	}

	mmc_claim_host(card->host);

	/* set device to FFU mode */
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG,
		MMC_FFU_MODE_SET, card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_err("FFU: %s: error %d FFU is not supported\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* Read the EXT_CSD */
	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("FFU: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* set CMD ARG */
	arg = ext_csd[EXT_CSD_FFU_ARG] |
		ext_csd[EXT_CSD_FFU_ARG + 1] << 8 |
		ext_csd[EXT_CSD_FFU_ARG + 2] << 16 |
		ext_csd[EXT_CSD_FFU_ARG + 3] << 24;

	err = mmc_ffu_write(card, fw->data, arg, (int)fw->size);
	if (err) {
		pr_err("FFU: %s: error %d write switching to normal\n",
			mmc_hostname(card->host), err);
	}

	/* host switch back to work in normal MMC Read/Write commands */
	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
		EXT_CSD_MODE_CONFIG, MMC_FFU_MODE_NORMAL,
		card->ext_csd.generic_cmd6_time);
	if (ret) {
		if (!err)
			err = ret;
		goto exit;
	}

	/* Read the EXT_CSD */
	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("FFU: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		goto exit;
	}

	/* check that the eMMC has received the payload */
	fw_prog_bytes = ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG] |
		ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 1] << 8 |
		ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 2] << 16 |
		ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 3] << 24;

	/* convert sectors to bytes: multiply by 512B or 4KB as
	   required by the card */
	 fw_prog_bytes *=
		CARD_BLOCK_SIZE << (ext_csd[EXT_CSD_DATA_SECTOR_SIZE] * 3);
	if (fw_prog_bytes != fw->size) {
		err = -EINVAL;
		pr_err("FFU: %s: error %d number of programmed fw sector incorrect %d %zd\n",
				__func__, err, fw_prog_bytes, fw->size);
	}

exit:
	release_firmware(fw);
	mmc_release_host(card->host);
	return err;
}
EXPORT_SYMBOL(mmc_ffu_download);

int mmc_ffu_install(struct mmc_card *card)
{
	u8 ext_csd[CARD_BLOCK_SIZE];
	int err;
	u32 ffu_data_len;
	u32 timeout;

	/* Check if FFU is supported */
	if (!card->ext_csd.ffu_capable) {
		pr_err("FFU: %s: error FFU is not supported\n",
			mmc_hostname(card->host));
		return -EOPNOTSUPP;
	}

	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("FFU: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		return err;
	}

	/* check mode operation */
	if (!FFU_FEATURES(ext_csd[EXT_CSD_FFU_FEATURES])) {
		/* restart the eMMC */
		err = mmc_ffu_restart(card);
		if (err) {
			pr_err("FFU: %s: install error %d:\n",
				mmc_hostname(card->host), err);
			return err;
		}
	} else {

		ffu_data_len = ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG]|
			ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 1] << 8 |
			ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 2] << 16 |
			ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 3] << 24;

		if (!ffu_data_len) {
			err = -EPERM;
			return err;
		}
		/* set device to FFU mode */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_MODE_CONFIG, 0x1,
			card->ext_csd.generic_cmd6_time);

		if (err) {
			pr_err("FFU: %s: error %d FFU is not supported\n",
				mmc_hostname(card->host), err);
			return err;
		}

		timeout = ext_csd[EXT_CSD_OPERATION_CODE_TIMEOUT];
		if (timeout == 0 || timeout > 0x17) {
			timeout = 0x17;
			pr_warn("FFU: %s: operation code timeout is out of range. Using maximum timeout.\n",
				mmc_hostname(card->host));
		}

		/* timeout is at millisecond resolution */
		timeout = (100 * (1 << timeout) / 1000) + 1;

		/* set ext_csd to install mode */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_MODE_OPERATION_CODES,
			MMC_FFU_INSTALL_SET, timeout);

		if (err) {
			pr_err("FFU: %s: error %d setting install mode\n",
				mmc_hostname(card->host), err);
			return err;
		}
	}

	/* read ext_csd */
	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("FFU: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		return err;
	}

	/* return status */
	err = ext_csd[EXT_CSD_FFU_STATUS];
	if (err) {
		pr_err("FFU: %s: error %d FFU install:\n",
			mmc_hostname(card->host), err);
		return  -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mmc_ffu_install);
