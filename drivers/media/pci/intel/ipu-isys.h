/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2018 Intel Corporation */

#ifndef IPU_ISYS_H
#define IPU_ISYS_H

#include <linux/pm_qos.h>
#include <linux/spinlock.h>

#include <media/v4l2-device.h>
#include <media/media-device.h>

#include <uapi/linux/ipu-isys.h>

#include "ipu.h"
#include "ipu-isys-csi2.h"
#include "ipu-isys-csi2-be.h"
#include "ipu-isys-tpg.h"
#include "ipu-isys-video.h"
#include "ipu-pdata.h"
#include "ipu-fw-isys.h"
#include "ipu-platform-isys.h"

#define IPU_ISYS_2600_MEM_LINE_ALIGN	64

/* for TPG */
#define IPU_ISYS_FREQ		533000000UL

/*
 * Current message queue configuration. These must be big enough
 * so that they never gets full. Queues are located in system memory
 */
#define IPU_ISYS_SIZE_RECV_QUEUE 40
#define IPU_ISYS_SIZE_SEND_QUEUE 40
#define IPU_ISYS_SIZE_PROXY_RECV_QUEUE 5
#define IPU_ISYS_SIZE_PROXY_SEND_QUEUE 5
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
#define IPU_LIB_CALL_TIMEOUT_JIFFIES \
	msecs_to_jiffies(IPU_LIB_CALL_TIMEOUT_MS)

#define IPU_ISYS_CSI2_LONG_PACKET_HEADER_SIZE	32
#define IPU_ISYS_CSI2_LONG_PACKET_FOOTER_SIZE	32

#define IPU_ISYS_MIN_WIDTH		1U
#define IPU_ISYS_MIN_HEIGHT		1U
#define IPU_ISYS_MAX_WIDTH		16384U
#define IPU_ISYS_MAX_HEIGHT		16384U

struct task_struct;

/*
 * struct ipu_isys
 *
 * @media_dev: Media device
 * @v4l2_dev: V4L2 device
 * @adev: ISYS bus device
 * @power: Is ISYS powered on or not?
 * @isr_bits: Which bits does the ISR handle?
 * @power_lock: Serialise access to power (power state in general)
 * @csi2_rx_ctrl_cached: cached shared value between all CSI2 receivers
 * @lock: serialise access to pipes
 * @pipes: pipelines per stream ID
 * @fwcom: fw communication layer private pointer
 *         or optional external library private pointer
 * @line_align: line alignment in memory
 * @reset_needed: Isys requires d0i0->i3 transition
 * @video_opened: total number of opened file handles on video nodes
 * @mutex: serialise access isys video open/release related operations
 * @stream_mutex: serialise stream start and stop, queueing requests
 * @lib_mutex: optional external library mutex
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
 * @short_packet_source: select short packet capture mode
 */
struct ipu_isys {
	struct media_device media_dev;
	struct v4l2_device v4l2_dev;
	struct ipu_bus_device *adev;

	int power;
	spinlock_t power_lock;	/* Serialise access to power */
	u32 isr_csi2_bits;
	u32 csi2_rx_ctrl_cached;
	spinlock_t lock;	/* Serialise access to pipes */
	struct ipu_isys_pipeline *pipes[IPU_ISYS_MAX_STREAMS];
	void *fwcom;
	unsigned int line_align;
	bool reset_needed;
	bool icache_prefetch;
	bool csi2_cse_ipc_not_supported;
	unsigned int video_opened;
	unsigned int stream_opened;
	struct dentry *debugfsdir;
	struct mutex mutex;	/* Serialise isys video open/release related */
	struct mutex stream_mutex;	/* Stream start, stop, queueing reqs */
	struct mutex lib_mutex;	/* Serialise optional external library mutex */

	struct ipu_isys_pdata *pdata;

	struct ipu_isys_csi2 *csi2;
	struct ipu_isys_tpg *tpg;
	struct ipu_isys_isa isa;
	struct ipu_isys_csi2_be csi2_be;
	struct ipu_isys_csi2_be_soc csi2_be_soc;

	const struct firmware *fw;
	struct sg_table fw_sgt;

	u64 *pkg_dir;
	dma_addr_t pkg_dir_dma_addr;
	unsigned int pkg_dir_size;

	struct list_head requests;
	struct pm_qos_request pm_qos;
	unsigned int short_packet_source;
	struct ipu_isys_csi2_monitor_message *short_packet_trace_buffer;
	dma_addr_t short_packet_trace_buffer_dma_addr;
	unsigned int short_packet_tracing_count;
	struct mutex short_packet_tracing_mutex;	/* For tracing count */
	u64 tsc_timer_base;
	u64 tunit_timer_base;
	spinlock_t listlock;	/* Protect framebuflist */
	struct list_head framebuflist;
	struct list_head framebuflist_fw;
};

struct isys_fw_msgs {
	union {
		u64 dummy;
		struct ipu_fw_isys_frame_buff_set_abi frame;
		struct ipu_fw_isys_stream_cfg_data_abi stream;
	} fw_msg;
	struct list_head head;
	dma_addr_t dma_addr;
};

#define to_frame_msg_buf(a) (&(a)->fw_msg.frame)
#define to_stream_cfg_msg_buf(a) (&(a)->fw_msg.stream)
#define to_dma_addr(a) ((a)->dma_addr)

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
int ipu_pipeline_pm_use(struct media_entity *entity, int use);
#endif
struct isys_fw_msgs *ipu_get_fw_msg_buf(struct ipu_isys_pipeline *ip);
void ipu_put_fw_mgs_buffer(struct ipu_isys *isys, u64 data);
void ipu_cleanup_fw_msg_bufs(struct ipu_isys *isys);

extern const struct v4l2_ioctl_ops ipu_isys_ioctl_ops;

void isys_setup_hw(struct ipu_isys *isys);
int isys_isr_one(struct ipu_bus_device *adev);
int ipu_isys_isr_run(void *ptr);
irqreturn_t isys_isr(struct ipu_bus_device *adev);

#endif /* IPU_ISYS_H */
