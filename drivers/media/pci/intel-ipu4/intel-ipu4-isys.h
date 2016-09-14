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
#include "intel-ipu-isys-csi2-common.h"
#include "intel-ipu4-isys-csi2-be.h"
#include "intel-ipu4-isys-isa.h"
#include "intel-ipu4-isys-tpg.h"
#include "intel-ipu4-isys-video.h"
#include "intel-ipu4-pdata.h"
#include "intel-ipu4-isys-fw-msgs.h"

#define INTEL_IPU4_ISYS_ENTITY_PREFIX		"Intel IPU4"

#define INTEL_IPU4_ISYS_2600_MEM_LINE_ALIGN	64

/* for TPG */
#define INTEL_IPU4_ISYS_FREQ_BXT_FPGA		25000000UL
#define INTEL_IPU4_ISYS_FREQ_BXT		533000000UL

/*
 * Current message queue configuration. These must be big enough
 * so that they never gets full. Queues are located in system memory
 */
#define INTEL_IPU4_ISYS_SIZE_RECV_QUEUE 40
#define INTEL_IPU4_ISYS_SIZE_SEND_QUEUE 40
#define INTEL_IPU4_ISYS_SIZE_PROXY_RECV_QUEUE 5
#define INTEL_IPU4_ISYS_SIZE_PROXY_SEND_QUEUE 5
#define INTEL_IPU4_ISYS_NUM_RECV_QUEUE 1

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
 * For B0/1: FW support max 8 streams
 */
#define INTEL_IPU4_ISYS_MAX_STREAMS		8


#define INTEL_IPU4_ISYS_MIN_WIDTH		1U
#define INTEL_IPU4_ISYS_MIN_HEIGHT		1U
#define INTEL_IPU4_ISYS_MAX_WIDTH		16384U
#define INTEL_IPU4_ISYS_MAX_HEIGHT		16384U

struct task_struct;

struct intel_ipu4_isys_fw_ctrl {
	int (*fw_init)(struct intel_ipu4_isys *isys,
		       unsigned int num_streams);
	int (*fw_close)(struct intel_ipu4_isys *isys);
	void (*fw_force_clean)(struct intel_ipu4_isys *isys);
	int (*simple_cmd)(struct intel_ipu4_isys *isys,
			  const unsigned int stream_handle,
			  enum ipu_fw_isys_send_type send_type);
	int (*complex_cmd)(struct intel_ipu4_isys *isys,
			   const unsigned int stream_handle,
			   void *cpu_mapped_buf,
			   dma_addr_t dma_mapped_buf,
			   size_t size,
			   enum ipu_fw_isys_send_type send_type);
	int (*fw_proxy_cmd)(struct intel_ipu4_isys *isys,
			    unsigned int req_id,
			    unsigned int index,
			    unsigned int offset,
			    u32 value);
	struct ipu_fw_isys_resp_info_abi *(*get_response)(
		void *context, unsigned int queue,
		struct ipu_fw_isys_resp_info_abi *response);
	void (*put_response)(
		void *context, unsigned int queue);
};

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
 * @fwcom: fw communication layer private pointer
 *         or optional external library private pointer
 * @line_align: line alignment in memory
 * @legacy_port_cfg: lane mappings for legacy CSI-2 ports
 * @combo_port_cfg: lane mappings for D/C-PHY ports
 * @isr_thread: for polling for events if interrupt delivery isn't available
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
struct intel_ipu4_isys {
	struct media_device media_dev;
	struct v4l2_device v4l2_dev;
	struct intel_ipu4_bus_device *adev;
	const struct  intel_ipu4_isys_fw_ctrl *fwctrl;

	int power;
	spinlock_t power_lock;
	u32 isr_csi2_bits;
	u32 csi2_rx_ctrl_cached;
	spinlock_t lock;
	struct intel_ipu4_isys_pipeline *pipes[INTEL_IPU4_ISYS_MAX_STREAMS];
	void *fwcom;
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
	unsigned int short_packet_source;
	struct intel_ipu4_isys_csi2_monitor_message *short_packet_trace_buffer;
	dma_addr_t short_packet_trace_buffer_dma_addr;
	unsigned int short_packet_tracing_count;
	struct mutex short_packet_tracing_mutex;
	u64 tsc_timer_base;
	u64 tunit_timer_base;
	spinlock_t listlock;
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

struct isys_fw_msgs *intel_ipu4_get_fw_msg_buf(
	struct intel_ipu4_isys_pipeline *ip);
void intel_ipu4_put_fw_mgs_buffer(struct intel_ipu4_isys *isys,
		       u64 data);
void intel_ipu4_cleanup_fw_msg_bufs(struct intel_ipu4_isys *isys);
void intel_ipu4_isys_register_ext_library(
	const struct intel_ipu4_isys_fw_ctrl *ops);
void intel_ipu4_isys_unregister_ext_library(void);
const struct intel_ipu4_isys_fw_ctrl *intel_ipu4_isys_get_api_ops(void);

extern const struct v4l2_ioctl_ops intel_ipu4_isys_ioctl_ops;

#endif /* INTEL_IPU4_ISYS_H */
