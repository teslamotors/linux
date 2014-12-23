/******************************************************************************
 * Intel Management Engine Interface (Intel MEI) Linux driver
 * Intel MEI Interface Header
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2015 - 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2015 - 2017 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#ifndef _MEI_SPD_H
#define _MEI_SPD_H

#include <linux/fs.h>
#include <linux/mei_cl_bus.h>

enum mei_spd_state {
	MEI_SPD_STATE_INIT,
	MEI_SPD_STATE_INIT_WAIT,
	MEI_SPD_STATE_INIT_DONE,
	MEI_SPD_STATE_RUNNING,
	MEI_SPD_STATE_STOPPING,
};

/**
 * struct mei_spd - spd device struct
 *
 * @cldev:     client bus device
 * @gpp:       GPP partition block device
 * @gpp_partition_id: GPP partition id (1-6)
 * @gpp_interface: gpp class interface for discovery
 * @dev_type:  storage device type
 * @dev_id_sz: device id size
 * @dev_id:    device id string
 * @lock:      mutex to sync request processing
 * @state:     driver state
 * @status_send_w: workitem for sending status to the FW
 * @buf_sz:    receive/transmit buffer allocated size
 * @buf:       receive/transmit buffer
 * @dbgfs_dir: debugfs directory entry
 */
struct mei_spd {
	struct mei_cl_device *cldev;
	struct block_device *gpp;
	u32    gpp_partition_id;
	struct class_interface gpp_interface;
	u32    dev_type;
	u32    dev_id_sz;
	u8     *dev_id;
	struct mutex lock;
	enum mei_spd_state state;
	struct work_struct status_send_w;
	size_t buf_sz;
	u8 *buf;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *dbgfs_dir;
#endif /* CONFIG_DEBUG_FS */
};

struct mei_spd *mei_spd_alloc(struct mei_cl_device *cldev);
void mei_spd_free(struct mei_spd *spd);

int mei_spd_cmd_init_req(struct mei_spd *spd);
int mei_spd_cmd_storage_status_req(struct mei_spd *spd);
ssize_t mei_spd_cmd(struct mei_spd *spd);

void mei_spd_gpp_prepare(struct mei_spd *spd);
bool mei_spd_gpp_is_open(struct mei_spd *spd);
int mei_spd_gpp_init(struct mei_spd *spd);
void mei_spd_gpp_exit(struct mei_spd *spd);
int mei_spd_gpp_read(struct mei_spd *spd, size_t off, u8 *data, size_t size);
int mei_spd_gpp_write(struct mei_spd *spd, size_t off, u8 *data, size_t size);

#if IS_ENABLED(CONFIG_DEBUG_FS)
int mei_spd_dbgfs_register(struct mei_spd *spd, const char *name);
void mei_spd_dbgfs_deregister(struct mei_spd *spd);
#else
static inline int mei_spd_dbgfs_register(struct mei_spd *spd, const char *name)
{
	return 0;
}

static inline void mei_spd_dbgfs_deregister(struct mei_spd *spd)
{
}

#endif /* CONFIG_DEBUG_FS */

const char *mei_spd_state_str(enum mei_spd_state state);

#define spd_err(spd, fmt, ...) \
	dev_err(&(spd)->cldev->dev, fmt, ##__VA_ARGS__)
#define spd_warn(spd, fmt, ...) \
	dev_warn(&(spd)->cldev->dev, fmt, ##__VA_ARGS__)
#define spd_dbg(spd, fmt, ...) \
	dev_dbg(&(spd)->cldev->dev, fmt, ##__VA_ARGS__)

#endif /* _MEI_SPD_H */
