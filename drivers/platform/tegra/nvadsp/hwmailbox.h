/*
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __HWMAILBOX_H
#define __HWMAILBOX_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/spinlock.h>

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#include "hwmailbox-t21x.h"
#else
#include "hwmailbox-t18x.h"
#endif /* CONFIG_ARCH_TEGRA_21x_SOC */

/*
 * The interpretation of hwmailbox content is:
 *             31   30 29    0
 *           [TAG|TYPE|MESSAGE]
 */
#define HWMBOX_TAG_SHIFT	31
#define HWMBOX_TAG_MASK		0x1
#define HWMBOX_TAG_INVALID	0
#define HWMBOX_TAG_VALID	1
/* Set Invalid TAG */
#define SET_HWMBOX_TAG_INVALID	(HWMBOX_TAG_INVALID << HWMBOX_TAG_SHIFT)
/* Set Valid TAG */
#define SET_HWMBOX_TAG_VALID	(HWMBOX_TAG_VALID << HWMBOX_TAG_SHIFT)
/* Get current TAG */
#define HWMBOX_TAG(val)		((val & HWMBOX_TAG_MASK) << HWMBOX_TAG_SHIFT)

/*
 * Mailbox can be used for sending short messages and long messages
 */
#define HWMBOX_MSG_TYPE_SHIFT	30
#define HWMBOX_MSG_TYPE_MASK	0x1
#define HWMBOX_MSG_SMSG		0
#define HWMBOX_MSG_LMSG		1
/* Set SMSG type */
#define SET_HWMBOX_MSG_SMSG	(HWMBOX_MSG_SMSG << HWMBOX_MSG_TYPE_SHIFT)
/* Set LMSG type */
#define SET_HWMBOX_MSG_LMSG	(HWMBOX_MSG_LMSG << HWMBOX_MSG_TYPE_SHIFT)
/* Get MSG type */
#define HWMBOX_MSG_TYPE(val)	\
		((val >> HWMBOX_MSG_TYPE_SHIFT) & HWMBOX_MSG_TYPE_MASK)
/* Check if SMSG */
#define IS_HWMBOX_MSG_SMSG(val)	\
		(!((val >> HWMBOX_MSG_TYPE_SHIFT) & HWMBOX_MSG_TYPE_MASK))
/* Check if LMSG */
#define IS_HWMBOX_MSG_LMSG(val)	\
		((val >> HWMBOX_MSG_TYPE_SHIFT) & HWMBOX_MSG_TYPE_MASK)

/*
 * The format for a short message is:
 *            31   30 29   20 19          0
 *          [TAG|TYPE|MBOX ID|SHORT MESSAGE]
 *            1b   1b  10bits     20bits
 */
#define HWMBOX_SMSG_SHIFT	0
#define HWMBOX_SMSG_MASK	0x3FFFFFFF
#define HWMBOX_SMSG(val)	((val >> HWMBOX_SMSG_SHIFT) & HWMBOX_SMSG_MASK)
#define HWMBOX_SMSG_MID_SHIFT	20
#define HWMBOX_SMSG_MID_MASK	0x3FF
#define HWMBOX_SMSG_MID(val)	\
			((val >> HWMBOX_SMSG_MID_SHIFT) & HWMBOX_SMSG_MID_MASK)
#define HWMBOX_SMSG_MSG_SHIFT	0
#define HWMBOX_SMSG_MSG_MASK	0xFFFFF
#define HWMBOX_SMSG_MSG(val)	\
			((val >> HWMBOX_SMSG_MSG_SHIFT) & HWMBOX_SMSG_MSG_MASK)
/* Set mailbox id for a short message */
#define SET_HWMBOX_SMSG_MID(val) \
			((val & HWMBOX_SMSG_MID_MASK) << HWMBOX_SMSG_MID_SHIFT)
/* Set msg value in a short message */
#define SET_HWMBOX_SMSG_MSG(val) \
			((val & HWMBOX_SMSG_MSG_MASK) << HWMBOX_SMSG_MSG_SHIFT)

/* Prepare a small message with mailbox id and data */
#define PREPARE_HWMBOX_SMSG(mid, data)	(SET_HWMBOX_TAG_VALID | \
					 SET_HWMBOX_MSG_SMSG | \
					 SET_HWMBOX_SMSG_MID(mid) | \
					 SET_HWMBOX_SMSG_MSG(data))
/* Prepare empty mailbox value */
#define PREPARE_HWMBOX_EMPTY_MSG()	(HWMBOX_TAG_INVALID | 0x0)

/*
 * Queue size must be power of 2 as '&' op
 * is being used to manage circular queues
 */
#define HWMBOX_QUEUE_SIZE	1024
#define HWMBOX_QUEUE_SIZE_MASK	(HWMBOX_QUEUE_SIZE - 1)
struct hwmbox_queue {
	uint32_t array[HWMBOX_QUEUE_SIZE];
	uint16_t head;
	uint16_t tail;
	uint16_t count;
	struct completion comp;
	spinlock_t lock;
};

int nvadsp_hwmbox_init(struct platform_device *);
status_t nvadsp_hwmbox_send_data(uint16_t, uint32_t, uint32_t);
void dump_mailbox_regs(void);

#endif /* __HWMAILBOX_H */
