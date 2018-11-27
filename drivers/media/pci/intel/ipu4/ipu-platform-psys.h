/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2018 Intel Corporation */

#ifndef IPU_PLATFORM_PSYS_H
#define IPU_PLATFORM_PSYS_H

#include <uapi/linux/ipu-psys.h>

struct ipu_psys_fh;
struct ipu_psys_kcmd;

struct ipu_psys_scheduler {
	struct list_head kcmds[IPU_PSYS_CMD_PRIORITY_NUM];
	struct ipu_psys_kcmd
	*new_kcmd_tail[IPU_PSYS_CMD_PRIORITY_NUM];
};

enum ipu_psys_cmd_state {
	KCMD_STATE_NEW,
	KCMD_STATE_START_PREPARED,
	KCMD_STATE_STARTED,
	KCMD_STATE_RUN_PREPARED,
	KCMD_STATE_RUNNING,
	KCMD_STATE_COMPLETE
};

int ipu_psys_fh_init(struct ipu_psys_fh *fh);
int ipu_psys_fh_deinit(struct ipu_psys_fh *fh);

#endif /* IPU_PLATFORM_PSYS_H */
