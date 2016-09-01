/*
 * Copyright (c) 2013--2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef INTEL_IPU4_ISYS_H
#define INTEL_IPU4_ISYS_H

#include <linux/pm_qos.h>
#include <linux/spinlock.h>

#include <media/v4l2-device.h>
#include <media/media-device.h>

#include "intel-ipu4.h"
#include "intel-ipu4-isys-compat-defs.h"
#include "intel-ipu4-isys-csi2.h"
#include "intel-ipu4-isys-csi2-be.h"
#include "intel-ipu4-isys-isa.h"
#include "intel-ipu4-isys-tpg.h"
#include "intel-ipu4-isys-video.h"
#include "intel-ipu4-pdata.h"

#define INTEL_IPU4_ISYS_ENTITY_PREFIX		"Intel IPU4"

#define INTEL_IPU4_ISYS_2600_MEM_LINE_ALIGN	64

/* for TPG */
#define INTEL_IPU4_ISYS_FREQ_BXT_FPGA		25000000UL
#define INTEL_IPU4_ISYS_FREQ_BXT		533000000UL

/*
 * Device close takes some time from last ack message to actual stopping
 * of the SP processor. As long as the SP processor runs we can't proceed with
 * clean up of resources.
 */
#define INTEL_IPU4_ISYS_OPEN_TIMEOUT_US		1000
#define INTEL_IPU4_ISYS_OPEN_RETRY		1000
#define INTEL_IPU4_ISYS_TURNOFF_DELAY_US		1000
#define INTEL_IPU4_ISYS_TURNOFF_TIMEOUT		1000
#define INTEL_IPU4_LIB_CALL_TIMEOUT_MS		2000
#define INTEL_IPU4_LIB_CALL_TIMEOUT_JIFFIES \
	msecs_to_jiffies(INTEL_IPU4_LIB_CALL_TIMEOUT_MS)

#define INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE	32
#define INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE	32

/*
 * For B0/1: FW support max 6 streams
 */
#define INTEL_IPU4_ISYS_MAX_STREAMS		6


#define INTEL_IPU4_ISYS_MIN_WIDTH		1U
#define INTEL_IPU4_ISYS_MIN_HEIGHT		1U
#define INTEL_IPU4_ISYS_MAX_WIDTH		16384U
#define INTEL_IPU4_ISYS_MAX_HEIGHT		16384U

struct task_struct;

/*
 * struct intel_ipu4_isys
 *
 * @media_dev: Media device
 * @v4l2_dev: V4L2 device
 * @adev: ISYS ipu4 bus device
 * @power: Is ISYS powered on or not?
 * @isr_bits: Which bits does the ISR handle?
 * @power_lock: Serialise access to power (power state in general)
 * @csi2_rx_ctrl_cached: cached shared value between all CSI2 receivers
 * @lock: serialise access to pipes
 * @pipes: pipelines per stream ID
 * @ssi: ssi library private pointer
 * @line_align: line alignment in memory
 * @legacy_port_cfg: lane mappings for legacy CSI-2 ports
 * @combo_port_cfg: lane mappings for D/C-PHY ports
 * @isr_thread: for polling for events if interrupt delivery isn't available
 * @reset_needed: Isys requires d0i0->i3 transition
 * @video_opened: total number of opened file handles on video nodes
 * @mutex: serialise access isys video open/release related operations
 * @stream_mutex: serialise stream start and stop, queueing requests
 * @pdata: platform data pointer
 * @csi2: CSI-2 receivers
 * @tpg: test pattern generators
 * @csi2_be: CSI-2 back-ends
 * @isa: Input system accelerator
 * @fw: ISYS firmware binary (unsecure firmware)
 * @fw_sgt: fw scatterlist
 * @pkg_dir: host pointer to pkg_dir
 * @pkg_dir_dma_addr: I/O virtual address for pkg_dir
 * @pkg_dir_size: size of pkg_dir in bytes
 */
struct intel_ipu4_isys {
	struct media_device media_dev;
	struct v4l2_device v4l2_dev;
	struct intel_ipu4_bus_device *adev;

	int power;
	spinlock_t power_lock;
	u32 isr_csi2_bits;
	u32 csi2_rx_ctrl_cached;
	spinlock_t lock;
	struct intel_ipu4_isys_pipeline *pipes[INTEL_IPU4_ISYS_MAX_STREAMS];
	void *ssi;
	unsigned int line_align;
	u32 legacy_port_cfg;
	u32 combo_port_cfg;
	struct task_struct *isr_thread;
	bool reset_needed;
	bool icache_prefetch;
	bool csi2_cse_ipc_not_supported;
	unsigned int video_opened;
	unsigned int stream_opened;
	struct dentry *debugfsdir;
	struct mutex mutex;
	struct mutex stream_mutex;
	struct mutex lib_mutex;

	struct intel_ipu4_isys_pdata *pdata;

	struct intel_ipu4_isys_csi2 *csi2;
	struct intel_ipu4_isys_tpg *tpg;
	struct intel_ipu4_isys_isa isa;
	struct intel_ipu4_isys_csi2_be csi2_be;
	struct intel_ipu4_isys_csi2_be_soc csi2_be_soc;

	const struct firmware *fw;
	struct sg_table fw_sgt;

	u64 *pkg_dir;
	dma_addr_t pkg_dir_dma_addr;
	unsigned pkg_dir_size;

	struct list_head requests;
	struct pm_qos_request pm_qos;
};

extern const struct v4l2_ioctl_ops intel_ipu4_isys_ioctl_ops;

#define intel_ipu4_lib_call_notrace_unlocked(func, isys, ...)		\
	({								\
		 int rval;						\
									\
		 rval = -ia_css_isys_##func((isys)->ssi, ##__VA_ARGS__);\
									\
		 rval;							\
	})

#define intel_ipu4_lib_call_notrace(func, isys, ...)			\
	({								\
		 int rval;						\
									\
		 mutex_lock(&(isys)->lib_mutex);			\
									\
		 rval = intel_ipu4_lib_call_notrace_unlocked(		\
			 func, isys, ##__VA_ARGS__);			\
									\
		 mutex_unlock(&(isys)->lib_mutex);			\
									\
		 rval;							\
	})

#define intel_ipu4_lib_call(func, isys, ...)				\
	({								\
		 int rval;						\
		 dev_dbg(&(isys)->adev->dev, "hostlib: libcall %s\n", #func);\
		 rval = intel_ipu4_lib_call_notrace(func, isys, ##__VA_ARGS__);\
									\
		 rval;							\
	})

#endif /* INTEL_IPU4_ISYS_H */
