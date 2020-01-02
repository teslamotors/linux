/*
 * Copyright (C) 2017 Tesla Motors, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MALI_C71_H__
#define __MALI_C71_H__

/*
 * Upper bound used to configure FIFO watermarks efficiently for
 * image sizes in testing, not necessarily a hardware limitation.
 * Minimum bound chosen arbitrarily, not a hardware limitation.
 */
#define ISP_MAX_WIDTH	3840
#define ISP_MIN_WIDTH	320

/*
 * Upper bound is documented hardware limit, but lower bound is arbitrary
 * (and probably not fully tested.)
 */
#define ISP_MAX_HEIGHT	4096
#define ISP_MIN_HEIGHT	32

#define ISP_WIDTH	1280
/* we strip off top 2/bottom 2 lines */
#define ISP_HEIGHT	960
#define ISP_SRC_BYTES_PER_LINE_PACKED(width)	((width) * 3 / 2)
#define ISP_DST_BYTES_PER_LINE_YUV(width)	((width) * 3 / 2)

#define ISP_YUV12_Y_OFFSET(width, height)	0
#define ISP_YUV12_U_OFFSET(width, height)	(width * height)
#define ISP_YUV12_V_OFFSET(width, height)	(width * height * 5 / 4)

#define ISP_N_SLOTS	16

/* CDMA buf sized to cover entire register space */
#define C71_CDMA_BUF_LEN		0x20000

/* The device is suspending and request processing should be paused */
#define ISP_SLOT_FLAG_SUSPENDING (1u << 0)

struct ispdev_if;

enum isp_req_state {
	ISP_REQ_NEW = 0,
	ISP_REQ_PENDING = 1,
	ISP_REQ_DONE = 2,
	ISP_REQ_ERROR = 3,
	ISP_REQ_TIMEOUT = 4,
};

struct isp_req {
	struct list_head list;

	dma_addr_t src_addr;
	dma_addr_t dst_addr;

	enum isp_req_state state;
	u32 status;
	void (*callback)(u32 status, void *priv);
	void *priv;
};

struct isp_slot {
	struct ispdev *isp;
	int idx;
	unsigned long flags;

	spinlock_t slot_lock;
	struct list_head pending_reqs;
	struct completion suspended;
	u64 completions;
	struct timer_list req_timeout;

	uint32_t *cdma_buf;
	dma_addr_t cdma_buf_iova;
	int cdma_valid;
	u32 cdma_write_config;

	u32	width;
	u32	height;
};

struct ispdev {
	struct ispdev_if *isp_if;
	void __iomem *isp_regs;
	spinlock_t irq_lock;

	struct isp_slot isp_slot[ISP_N_SLOTS];
};

struct isp_req *isp_buf_submit(struct ispdev *isp, int slot,
				dma_addr_t src_addr, dma_addr_t dst_addr,
				void (*callback)(u32, void *),
				void *priv);

int isp_slot_setup(struct ispdev *isp, int slot, uint32_t width,
			uint32_t height);

enum isp_cdma_buf_op {
	ISP_CDMA_OP_GET = 0,
	ISP_CDMA_OP_SET = 1,
};

uint8_t *isp_get_cdma_buf(struct ispdev *isp, int slot_num,
				enum isp_cdma_buf_op dir);
void isp_put_cdma_buf(struct ispdev *isp, int slot_num,
				enum isp_cdma_buf_op dir);

#endif /* __MALI_C71_H__ */
