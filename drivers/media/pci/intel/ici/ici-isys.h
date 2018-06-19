/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef ICI_ISYS_H
#define ICI_ISYS_H

#define ICI_ENABLED

#ifdef ICI_ENABLED
#define IPU4_DEBUG

#include <linux/pm_qos.h>
#include <linux/spinlock.h>

#include "ipu.h"
#include "ipu-pdata.h"
#include "ipu-fw-isys.h"
#include "ici-isys-stream.h"
#include "ici-isys-csi2.h"
#include "ici-isys-csi2-be.h"
#include "ici-isys-pipeline-device.h"
#include "ici-isys-tpg.h"
#include "ipu-platform.h"
#include "ipu4/ipu-platform-isys.h"
#include "ipu4/ipu-platform-regs.h"

#define IPU_ISYS_ENTITY_PREFIX		"Intel IPU4"

#define IPU_ISYS_2600_MEM_LINE_ALIGN	64

#define IPU_ISYS_MAX_CSI2_PORTS IPU_ISYS_MAX_CSI2_LEGACY_PORTS+IPU_ISYS_MAX_CSI2_COMBO_PORTS
/* for TPG */
#define INTEL_IPU4_ISYS_FREQ_BXT_FPGA		25000000UL
#define INTEL_IPU4_ISYS_FREQ_BXT		533000000UL

#define IPU_ISYS_SIZE_RECV_QUEUE 40
#define IPU_ISYS_SIZE_SEND_QUEUE 40
#define IPU_ISYS_NUM_RECV_QUEUE 1

/*
 * Device close takes some time from last ack message to actual stopping
 * of the SP processor. As long as the SP processor runs we can't proceed with
 * clean up of resources.
 */
#define IPU_ISYS_OPEN_TIMEOUT_US		1000
#define IPU_ISYS_OPEN_RETRY		1000
#define IPU_ISYS_TURNOFF_DELAY_US		1000
#define IPU_ISYS_TURNOFF_TIMEOUT		1000
#define IPU_LIB_CALL_TIMEOUT_MS		2000
#define IPU_LIB_CALL_TIMEOUT_JIFFIES \
	msecs_to_jiffies(IPU_LIB_CALL_TIMEOUT_MS)

#define INTEL_IPU4_ISYS_CSI2_LONG_PACKET_HEADER_SIZE	32
#define INTEL_IPU4_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE	32

/*
 * For B0/1: FW support max 6 streams
 */
#define INTEL_IPU4_ISYS_MAX_STREAMS		6


#define IPU_ISYS_MIN_WIDTH		1U
#define IPU_ISYS_MIN_HEIGHT		1U
#define IPU_ISYS_MAX_WIDTH		16384U
#define IPU_ISYS_MAX_HEIGHT		16384U

struct task_struct;

/*
 * struct ici_isys
 *
 * @media_dev: Media device
 * @v4l2_dev: V4L2 device
 * @adev: ISYS ipu4 bus device
 * @power: Is ISYS powered on or not?
 * @isr_bits: Which bits does the ISR handle?
 * @power_lock: Serialise access to power (power state in general)
 * @lock: serialise access to pipes
 * @pipes: pipelines per stream ID
 * @fwcom: fwcom library private pointer
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
struct ici_isys {
	struct ipu_bus_device *adev;

	int power;
	spinlock_t power_lock;
	u32 isr_csi2_bits;
	spinlock_t lock;
	struct ipu_isys_pipeline *pipes[IPU_ISYS_MAX_STREAMS];
	void *fwcom;
	unsigned int line_align;
	u32 legacy_port_cfg;
	u32 combo_port_cfg;
	struct task_struct *isr_thread;
	bool reset_needed;
	bool icache_prefetch;
	unsigned int video_opened;
	unsigned int stream_opened;
	struct dentry *debugfsdir;
	struct mutex mutex;
	struct mutex stream_mutex;
	struct mutex lib_mutex;

	struct ipu_isys_pdata *pdata;

	struct ici_isys_pipeline_device pipeline_dev;

	struct ici_isys_pipeline *ici_pipes[IPU_ISYS_MAX_STREAMS];
	struct ici_isys_csi2 ici_csi2[IPU_ISYS_MAX_CSI2_PORTS];
	struct ici_isys_tpg ici_tpg[2]; // TODO map to a macro
	struct ici_isys_csi2_be ici_csi2_be[NR_OF_CSI2_BE_SOC_STREAMS];
	unsigned int ici_stream_opened;

	const struct firmware *fw;
	struct sg_table fw_sgt;

	u64 *pkg_dir;
	dma_addr_t pkg_dir_dma_addr;
	unsigned pkg_dir_size;

	struct list_head requests;
	struct pm_qos_request pm_qos;
	struct ici_isys_csi2_monitor_message *short_packet_trace_buffer;
	dma_addr_t short_packet_trace_buffer_dma_addr;
	u64 tsc_timer_base;
	u64 tunit_timer_base;
        spinlock_t listlock;    /* Protect framebuflist */
        struct list_head framebuflist;
        struct list_head framebuflist_fw;
};

int intel_ipu4_isys_isr_run_ici(void *ptr);

struct isys_fw_msgs {
        union {
                u64 dummy;
                struct ipu_fw_isys_frame_buff_set_abi frame;
                struct ipu_fw_isys_stream_cfg_data_abi stream;
        } fw_msg;
        struct list_head head;
        dma_addr_t dma_addr;
};

#define ipu_lib_call_notrace_unlocked(func, isys, ...)		\
	({								\
		 int rval;						\
									\
		 rval = -ia_css_isys_##func((isys)->fwcom, ##__VA_ARGS__);\
									\
		 rval;							\
	})

#define ipu_lib_call_notrace(func, isys, ...)			\
	({								\
		 int rval;						\
									\
		 mutex_lock(&(isys)->lib_mutex);			\
									\
		 rval = ipu_lib_call_notrace_unlocked(		\
			 func, isys, ##__VA_ARGS__);			\
									\
		 mutex_unlock(&(isys)->lib_mutex);			\
									\
		 rval;							\
	})

#define ipu_lib_call(func, isys, ...)				\
	({								\
		 int rval;						\
		 dev_dbg(&(isys)->adev->dev, "hostlib: libcall %s\n", #func);\
		 rval = ipu_lib_call_notrace(func, isys, ##__VA_ARGS__);\
									\
		 rval;							\
	})

#undef DEBUGK
#ifdef IPU4_DEBUG /* Macro for printing debug infos */
#	ifdef __KERNEL__	/* for kernel space */
#		define DEBUGK(fmt, args...) printk(KERN_DEBUG "IPU4: " fmt, ## args)
#	else				/* for user space */
#		define DEBUGK(fmt, args...) fprintf(stderr, fmt, ## args)
#	endif
#else /* no debug prints */
#	define DEBUGK(fmt, args...)
#endif

#else /* ICI_ENABLED */
#pragma message "IPU ICI version is DISABLED."
#endif /* ICI_ENABLED */

#endif /* ICI_ISYS_H */
