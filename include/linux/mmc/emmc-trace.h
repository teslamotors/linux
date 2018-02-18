#ifndef LINUX_MMC_EMMC_TRACE_H
#define LINUX_MMC_EMMC_TRACE_H

#include <linux/tracepoint.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include "../../../drivers/mmc/card/queue.h"

#ifdef DEBUG
#define INFO_MSG(fmt, ...) printk("\nEMMC_TRACE: "fmt, ##__VA_ARGS__)
#else
#define INFO_MSG(fmt, ...) {}
#endif

#define MMC_TRACER_MAGIC 0x12345
#define MMC_TRACER_VERSION 0x12345

#define	MMC_TRACE_JOURNAL	(0x1)
#define	MMC_TRACE_NONJOURNAL1	(0x2)
#define	MMC_TRACE_NONJOURNAL2	(0x3)

enum mmc_trace_type {
	MMC_PRE_DONE = 0,
	MMC_ISSUE,
	MMC_ISSUE_DONE,
	MMC_TRAN_DONE,
	MMC_COMPLETE,
	MMC_HASH,
	MMC_POST_START = 10,
	MMC_POST_DONE = 11,
	MMC_END_REQ = 12,
	MMC_PRE_START = 13,
};

void emmc_trace(unsigned int type, struct mmc_queue_req *mqrq, struct mmc_host *host);

#define MMC_MAX_PATH		(4096)

struct emmc_trace_data {
	u32			type;	/* trace type */

	/* request information */
	u32			cmd_flags;		/* request cmd_flags */
	u32			addr;			/* data address [sector] */
	u32			size;			/* data size [sector] */
	unsigned int		que_sync;		/* sync queue depth */
	unsigned int		que_async;		/* async queue depth */

	/* command information */
	u32			packed_depth;		/* packed depth */

	/* file information */
	int			data_type;		/* 0: journal, 1: user */
	umode_t			i_mode;		/* inode mode */
	unsigned int		i_ino;	/* inode number */

	/* cpu information */
	unsigned long long	cpu_usage;		/* cpu usage */
	unsigned long long	cpu_idle;		/* cpu idle */

	/* task information */
	u32			pid;			/* process id */
	unsigned char		pname[TASK_COMM_LEN];	/* process name */

	unsigned char		fname[MMC_MAX_PATH];	/* file name */
};

#endif
