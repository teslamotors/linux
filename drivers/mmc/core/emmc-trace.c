/* emmc-trace.c
 *
 * Extended to support emmc block level trace capture.
 *
 * (C) Copyright 2007 Mathieu Desnoyers <mathieu.desnoyers@polymtl.ca>
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This file is released under the GPLv2.
 * See the file COPYING for more details.
 */

#include <linux/blkdev.h>
#include <linux/types.h>
#include <linux/blktrace_api.h>

#include <linux/buffer_head.h>
#include <linux/jbd2.h>
#include <linux/mm_types.h>

#include <linux/mmc/emmc-trace.h>
#include <linux/scatterlist.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>

#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/irqnr.h>
#include <asm/cputime.h>

#include <crypto/hash.h>

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif
#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif

#ifdef CONFIG_EMMC_BLKTRACE

static unsigned int cnt[64];

static struct request *_get_request(struct mmc_queue_req *mqrq)
{
	if (mqrq == NULL)
		return NULL;
	return mqrq->req;
}

static struct request_queue *_get_request_queue(struct request *req)
{
	if (req == NULL)
		return NULL;
	return req->q;
}

static struct page *_get_page(struct request *req)
{
	struct bio *bio;
	struct page *page;

	if (req == NULL)
		return NULL;

	if (req->cmd_flags & (REQ_FLUSH | REQ_DISCARD))
		return NULL;

	bio = req->bio;
	if (bio == NULL)
		return NULL;

	page = bio->bi_io_vec[0].bv_page;

	return page;
}

static struct bio *_get_bio(struct request *req)
{
	struct bio *bio;

	if (req == NULL)
		return NULL;

	if (req->cmd_flags & (REQ_FLUSH | REQ_DISCARD))
		return NULL;

	bio = req->bio;
	if (bio == NULL)
		return NULL;

	return bio;
}

static struct buffer_head *_get_buffer_head(struct page *page)
{
	if (page == NULL)
		return NULL;

	if (!page_has_buffers(page))
		return NULL;

	return page_buffers(page);
}

/* return: 0 (journal), 1 (user) */
static int _get_data_type(struct buffer_head *bh)
{
	struct buffer_head *last;
	struct commit_header *ch;

	if (bh == NULL)
		return 1;
	last = bh;
	while (last->b_this_page != bh)
		last = last->b_this_page;
	ch = (struct commit_header *) last->b_data;

	if (buffer_jwrite(last))
		return 0;

	if ((ch) && (ch->h_magic == cpu_to_be32(JBD2_MAGIC_NUMBER)))
		return 0;

	return 1;
}

static struct inode *_get_inode(struct page *page)
{
	struct address_space *mapping;
	struct inode *inode;

	if (page == NULL)
		return NULL;

	mapping = page->mapping;
	if (mapping == NULL)
		return NULL;
	if ((unsigned int) mapping & 1)
		return NULL;

	inode = mapping->host;

	return inode;
}

static umode_t _get_imode(struct inode *inode)
{
	if (inode == NULL)
		return 0;
	return inode->i_mode;
}

static unsigned int _get_swapinfo(struct page *page)
{
	if (page == NULL)
		return 0;

	if (PageSwapCache(page))
		return 3;

	return 0;
}

static void _get_path(const struct dentry *dentry, char *fname, int *idx)
{
	if (!IS_ROOT(dentry))
		_get_path(dentry->d_parent, fname, idx);
	if (*idx + dentry->d_name.len >= MMC_MAX_PATH)
		return;
	strncpy(fname + *idx, dentry->d_name.name, dentry->d_name.len);
	*idx += dentry->d_name.len;
	if (!IS_ROOT(dentry) && (*idx + 1 < MMC_MAX_PATH)) {
		fname[*idx] = '/';
		(*idx)++;
	}
	return;
}

static void _get_cpu_usage(unsigned long long *cpu_usage, unsigned long long *cpu_idle)
{
	int i;
	cputime64_t user, nice, system, idle, iowait, irq, softirq, steal;
	cputime64_t guest, guest_nice;

	user = nice = system = idle = iowait =
		irq = softirq = steal = 0UL;
	guest = guest_nice = 0UL;

	for_each_possible_cpu(i) {
		user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle += kcpustat_cpu(i).cpustat[CPUTIME_IDLE];
		iowait += kcpustat_cpu(i).cpustat[CPUTIME_IOWAIT];
		irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest += kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];

	}

	*cpu_usage = (unsigned long long)cputime64_to_clock_t(user) +
		(unsigned long long)cputime64_to_clock_t(nice) +
		(unsigned long long)cputime64_to_clock_t(system) +
		(unsigned long long)cputime64_to_clock_t(irq) +
		(unsigned long long)cputime64_to_clock_t(softirq) +
		(unsigned long long)cputime64_to_clock_t(steal) +
		(unsigned long long)cputime64_to_clock_t(guest) +
		(unsigned long long)cputime64_to_clock_t(guest_nice);
	*cpu_idle = (unsigned long long)cputime64_to_clock_t(idle) +
		(unsigned long long)cputime64_to_clock_t(iowait);

	return;
}

struct emmc_trace_data etrace;
void emmc_trace(unsigned int type, struct mmc_queue_req *mqrq, struct mmc_host *host)
{
	struct request *req;
	struct bio *bio;
	struct request_queue *q;
	struct blk_trace *bt;
	struct request_list *rl;
	struct page *page;
	struct buffer_head *bh;
	struct inode *inode;
	int size;

	req = _get_request(mqrq);
	if (req == NULL)
		return;

	q = _get_request_queue(req);
	if (q == NULL)
		return;

	bt = q->blk_trace;
	if (likely(!bt))
		return;
	if (unlikely(bt->trace_state != Blktrace_running))
		return;

	/* trace type */
	etrace.type = type;

	/* request information */
	etrace.cmd_flags = req->cmd_flags;
	if (req->cmd_flags & REQ_FLUSH) {
		etrace.addr = 0;
		etrace.size = 0;
	} else {
		etrace.addr = blk_rq_pos(req);
		etrace.size = blk_rq_sectors(req);
	}
	rl = &q->root_rl;
	etrace.que_sync = rl->count[BLK_RW_SYNC];
	etrace.que_async = rl->count[BLK_RW_ASYNC];

	/* command information */
	etrace.packed_depth = 0;
	if (etrace.packed_depth > 1) {
		int i = 0;
		etrace.size = 0;
		do {
			etrace.size += cnt[i];
		} while (cnt[++i] != 0);
	}

	/* file information */
	page = _get_page(req);
	bio = _get_bio(req);
	bh = _get_buffer_head(page);
	etrace.data_type = _get_data_type(bh);
	inode = _get_inode(page);
	etrace.i_mode = _get_imode(inode);
	etrace.i_ino = _get_swapinfo(page);
	size = 0;

	_get_cpu_usage(&etrace.cpu_usage, &etrace.cpu_idle);

	etrace.pid = 0;

	blk_add_driver_data(q, req, &etrace,
		sizeof(struct emmc_trace_data) - MMC_MAX_PATH + size);

#ifdef MMC_DUMP_MD5_HASH
#define MMC_DUMP_CHUNK_SIZE		(4096)
#define MMC_DUMP_CHUNK_SEC		(4096 / 512)
	if ((type == MMC_COMPLETE) &&
		(req->cmd_flags & REQ_WRITE) && (blk_rq_sectors(req))) {
		int i = 0;

		do {
			char *buf;

			addr[i] = addr[i] - (addr[i] % MMC_DUMP_CHUNK_SEC);
			cnt[i] =
			((cnt[i] +
			(MMC_DUMP_CHUNK_SEC - 1)) / MMC_DUMP_CHUNK_SEC) *
			MMC_DUMP_CHUNK_SEC;

			buf = kmalloc(MMC_DUMP_CHUNK_SIZE, GFP_KERNEL);
			if (!buf) {
				printk("ERR: cannot allocate memory\n");
				break;
			}

			do {
				struct mmc_request mrq = {0};
				struct mmc_command sbc = {0};
				struct mmc_command cmd = {0};
				struct mmc_data data = {0};
				struct scatterlist sg;
				struct crypto_hash *tfm;
				struct hash_desc desc;

				mrq.sbc = &sbc;
				mrq.cmd = &cmd;
				mrq.data = &data;

				sbc.opcode = MMC_SET_BLOCK_COUNT;
				sbc.arg = MMC_DUMP_CHUNK_SEC;
				sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;

				cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
				cmd.arg = addr[i];
				cmd.flags = MMC_RSP_SPI_R1 |
				MMC_RSP_R1 | MMC_CMD_ADTC;

				data.blksz = 512;
				data.blocks = MMC_DUMP_CHUNK_SEC;
				data.flags = MMC_DATA_READ;
				data.sg = &sg;
				data.sg_len = 1;

				sg_init_one(data.sg, buf, MMC_DUMP_CHUNK_SIZE);

				mmc_wait_for_req(host, &mrq);

				if (sbc.error || cmd.error || data.error) {
					printk("BLKTRACE ERR: command error\n");
					break;
				}

				/* md5 */
				memset(etrace.fname, 0, 16);
				tfm = crypto_alloc_hash("md5", 0,
						CRYPTO_ALG_ASYNC);
				if (!tfm) {
					printk("BLKTRACE ERR: cannot get crypto\n");
					break;
				}
				desc.tfm = tfm;
				if (crypto_hash_init(&desc) != 0) {
					printk("BLKTRACE ERR: cannot initialize crypto\n");
					crypto_free_hash(tfm);
					break;
				}
				if (crypto_hash_digest(&desc,
				&sg, MMC_DUMP_CHUNK_SIZE, etrace.fname) != 0) {
					printk("BLKTRACE ERR: cannot calculate md5\n");
					crypto_free_hash(tfm);
					break;
				}
				crypto_free_hash(tfm);

				etrace.type = MMC_HASH;
				etrace.addr = addr[i];
				etrace.size = MMC_DUMP_CHUNK_SEC;
				etrace.packed_depth = i;

				blk_add_driver_data(q, req,
					&etrace,
					sizeof(struct emmc_trace_data) -
							MMC_MAX_PATH + 16);

				addr[i] += MMC_DUMP_CHUNK_SEC;
				cnt[i] -= MMC_DUMP_CHUNK_SEC;
			} while (cnt[i] > 0);

			kfree(buf);
		} while (cnt[++i] != 0);
	}
#endif

	return;
}
EXPORT_SYMBOL(emmc_trace);


#else /* !CONFIG_EMMC_BLKTRACE */


void emmc_trace(unsigned int type, struct mmc_queue_req *mqrq, struct mmc_host *host)
{
}
EXPORT_SYMBOL(emmc_trace);


#endif /* CONFIG_EMMC_BLKTRACE */
