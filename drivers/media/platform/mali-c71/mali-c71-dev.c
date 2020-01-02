#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/pm.h>

#include "mali-c71.h"
#include "mali-c71-reg.h"
#include "mali-c71-v4l.h"


/* MCFE FIFO low watermark recommended by ARM: units(64b) per line + 16 */
#define MCFE_FIFO_WATERMARK_LOW \
	(ISP_SRC_BYTES_PER_LINE_PACKED(ISP_MAX_WIDTH) / 8 + 16)

/* MCFE FIFO high watermark */
#define MCFE_FIFO_WATERMARK_HIGH 2032

/* MCBE FIFO watermark */
#define MCBE_FIFO_WATERMARK 240

/* Maximum amount of time to wait for an ISP request to finish, in millisecs */
#define REQUEST_TIMEOUT_MSECS 500

/*
 * Each slot takes up 4 output slots 1 for each plane (Y U V) and one reserved
 * for possible future use.
 */
#define OUT_BUFS_PER_SLOT (4)

enum out_plane {
	Y_PLANE = 0,
	U_PLANE = 1,
	V_PLANE = 2,
};

static inline u8 slot_rawbuf(u8 slot) {
	return slot;
}

static inline u8 slot_outbuf(u8 slot, enum out_plane plane) {
	return slot * OUT_BUFS_PER_SLOT + plane;
}

static inline void isp_rawbuf_writel(u32 val, struct ispdev *isp, int idx,
					u32 ofs)
{
	writel(val, isp->isp_regs + C71_REG_RAWBUF_BASE +
			idx * C71_REG_RAWBUF_OFS + ofs);
}

static inline void isp_outbuf_writel(u32 val, struct ispdev *isp, int idx,
					u32 ofs)
{

	writel(val, isp->isp_regs + C71_REG_OUTBUF_BASE +
			idx * C71_REG_OUTBUF_OFS + ofs);
}

static inline void isp_slot_writeb(u8 newval, struct ispdev *isp,
					uint32_t base, int slot)
{
	u32 ofs, val;

	ofs = base + (slot & ~3);
	val = readl(isp->isp_regs + ofs);
	val &= ~(0xff << ((slot & 3) * 8));
	val |= newval << ((slot & 3) * 8);
	writel(val, isp->isp_regs + ofs);
}

static inline u8 isp_slot_readb(struct ispdev *isp, uint32_t base,
					int slot)
{
	u32 inval = readl(isp->isp_regs + base + (slot & ~3));

	return (u8)((inval >> ((slot & 3) * 8)) & 0xff);
}

/* Slot setup determined later by user configuration (width/height) */
int isp_slot_setup(struct ispdev *isp, int slot,
			  uint32_t width, uint32_t height)
{
	u8 buf;

	isp->isp_slot[slot].width = width;
	isp->isp_slot[slot].height = height;

	buf = slot_rawbuf(slot);
	isp_rawbuf_writel(ISP_SRC_BYTES_PER_LINE_PACKED(width),
		isp, buf, C71_REG_OFS_BUF_LINE_OFFSET);
	isp_rawbuf_writel(width,
		isp, buf, C71_REG_OFS_BUF_ACTIVE_WIDTH);
	isp_rawbuf_writel(height,
		isp, buf, C71_REG_OFS_BUF_ACTIVE_HEIGHT);


	/* Y output */
	buf = slot_outbuf(slot, Y_PLANE);
	isp_outbuf_writel(width,
		isp, buf, C71_REG_OFS_BUF_LINE_OFFSET);
	isp_outbuf_writel(width,
		isp, buf, C71_REG_OFS_BUF_ACTIVE_WIDTH);
	isp_outbuf_writel(height,
		isp, buf, C71_REG_OFS_BUF_ACTIVE_HEIGHT);

	/* quarter-size U output */
	buf = slot_outbuf(slot, U_PLANE);
	isp_outbuf_writel(width/2,
		isp, buf, C71_REG_OFS_BUF_LINE_OFFSET);
	isp_outbuf_writel(width/2,
		isp, buf, C71_REG_OFS_BUF_ACTIVE_WIDTH);
	isp_outbuf_writel(height/2,
		isp, buf, C71_REG_OFS_BUF_ACTIVE_HEIGHT);

	/* quarter-size V output */
	buf = slot_outbuf(slot, V_PLANE);
	isp_outbuf_writel(width/2,
		isp, buf, C71_REG_OFS_BUF_LINE_OFFSET);
	isp_outbuf_writel(width/2,
		isp, buf, C71_REG_OFS_BUF_ACTIVE_WIDTH);
	isp_outbuf_writel(height/2,
		isp, buf, C71_REG_OFS_BUF_ACTIVE_HEIGHT);

	return 0;
}

/*
 * Init time setup sufficient to allow DMA operation, still requires
 * userspace to configure slot via IOCTL in order to get something
 * useful back from ISP.
 */
static int isp_dma_setup(struct platform_device *pdev)
{
	struct ispdev *isp = platform_get_drvdata(pdev);
	u32 out_mode;
	int i;

	dev_info(&pdev->dev, "Configuring for DMA.\n");
	/* Manual scheduling */
	 writel(C71_REG_MCFE_SCHEDULER_MODE_MANUAL,
			 isp->isp_regs + C71_REG_MCFE_SCHEDULER_MODE);
	/*
	 * Unfortunately input mode is shared by all slots and outside of
	 * CDMA area, so if we need to vary between slots we'll need special
	 * handling.
	 */
	writel(C71_REG_MCFE_INPUT_MODE_VAL(C71_REG_MCFE_INPUT_MODE_TDMF,
				C71_REG_MCFE_INPUT_MODE_TDMF,
				C71_REG_MCFE_INPUT_MODE_TDMF,
				C71_REG_MCFE_INPUT_MODE_OFF),
			isp->isp_regs + C71_REG_MCFE_INPUT_MODE);

	/* Slots in RAM source/one-shot mode */
	writel(C71_REG_MCFE_SLOT_MODE_ALL_ONESHOT,
			isp->isp_regs + C71_REG_MCFE_SLOT_MODE_0_3);
	writel(C71_REG_MCFE_SLOT_MODE_ALL_ONESHOT,
			isp->isp_regs + C71_REG_MCFE_SLOT_MODE_4_7);
	writel(C71_REG_MCFE_SLOT_MODE_ALL_ONESHOT,
			isp->isp_regs + C71_REG_MCFE_SLOT_MODE_8_11);
	writel(C71_REG_MCFE_SLOT_MODE_ALL_ONESHOT,
			isp->isp_regs + C71_REG_MCFE_SLOT_MODE_12_15);

	for (i = 0; i < ISP_N_SLOTS; i++) {
		writel(isp->isp_slot[i].cdma_buf_iova,
			isp->isp_regs + C71_REG_MCFE_SLOT_CFG_BASE(i));
		writel(C71_CDMA_BUF_LEN,
			isp->isp_regs + C71_REG_MCFE_SLOT_CFG_SIZE(i));
		writel(C71_REG_MCFE_SLOT_CFG_FULL_SPACE |
				C71_REG_MCFE_SLOT_CFG_IRIDIX,
			isp->isp_regs + C71_REG_MCFE_SLOT_CFG_READ_UNITS(i));
		/* Disable dma from DRAM until CDMA buf has valid data */
		writel(C71_REG_MCFE_SLOT_CFG_NONE,
			isp->isp_regs + C71_REG_MCFE_SLOT_CFG_WRITE_UNITS(i));
	}

	/* Disable slot cfg handshaking */
	writel((C71_REG_MCFE_SLOT_CFG_AVAIL_NONE <<
		C71_REG_MCFE_SLOT_CFG_AVAIL_SHIFT) |
			(C71_REG_MCFE_SLOT_CFG_AVAIL_NOACK_ALL <<
			 C71_REG_MCFE_SLOT_CFG_AVAIL_NOACK_SHIFT),
		isp->isp_regs + C71_REG_MCFE_SLOT_CFG_AVAIL);

	writel(C71_REG_MCFE_SLOT_CFG_ALL_VALID,
		isp->isp_regs + C71_REG_MCFE_SLOT_CFG_VALID_0_3);
	writel(C71_REG_MCFE_SLOT_CFG_ALL_VALID,
		isp->isp_regs + C71_REG_MCFE_SLOT_CFG_VALID_4_7);
	writel(C71_REG_MCFE_SLOT_CFG_ALL_VALID,
		isp->isp_regs + C71_REG_MCFE_SLOT_CFG_VALID_8_11);
	writel(C71_REG_MCFE_SLOT_CFG_ALL_VALID,
		isp->isp_regs + C71_REG_MCFE_SLOT_CFG_VALID_12_15);

	writel((MCFE_FIFO_WATERMARK_HIGH <<
			C71_REG_MCFE_FIFO1_WATERMARK_HIGH_SHIFT) |
		(MCFE_FIFO_WATERMARK_LOW <<
			C71_REG_MCFE_FIFO1_WATERMARK_LOW_SHIFT),
		isp->isp_regs + C71_REG_MCFE_FIFO1_WATERMARK);

	/* MCBE Registers */
	writel(C71_REG_MCBE_FLOW_CONTROL_ENABLE,
			isp->isp_regs + C71_REG_MCBE_FLOW_CONTROL);
	writel((MCBE_FIFO_WATERMARK << C71_REG_MCBE_FIFO_1_WATERMARK_SHIFT) |
			(0 << C71_REG_MCBE_FIFO_2_WATERMARK_SHIFT),
		isp->isp_regs + C71_REG_MCBE_FIFO_1_2_WATERMARK);

	/* Top */
	writel(ISP_WIDTH,
		isp->isp_regs + C71_REG_TOP_ACTIVE_WIDTH);
	writel(ISP_HEIGHT,
		isp->isp_regs + C71_REG_TOP_ACTIVE_HEIGHT);
	writel((C71_REG_TOP_CFA_PATTERN_START_GR <<
		C71_REG_TOP_CFA_PATTERN_START_SHIFT) |
			(C71_REG_TOP_CFA_PATTERN_RGGB <<
			 C71_REG_TOP_CFA_PATTERN_SELECT_SHIFT),
		isp->isp_regs + C71_REG_TOP_CFA_PATTERN);

	/* Pipeline */
	writel(C71_REG_PIPELINE_BYPASS_TEST_GEN |
		C71_REG_PIPLEINE_BYPASS_FRAME_STITCH |
		C71_REG_PIPELINE_BYPASS_FS_CHANNEL_SW |
		C71_REG_PIPELINE_BYPASS_GAMMA_BE_SQ,
		isp->isp_regs + C71_REG_PIPELINE_BYPASS);

	/* Pipeline frontend */
	writel(C71_REG_PIPE_FE_HIST1_SWITCH_ISP_PIPE_TAP1,
		isp->isp_regs + C71_REG_PIPE_FE_HIST1_SWITCH);

	/* Output formatter */
	writel(C71_REG_OUTFMT_BYPASS_CLIP_RGB |
		C71_REG_OUTFMT_BYPASS_LUT_RGB |
		C71_REG_OUTFMT_BYPASS_RGB2RGB,
		isp->isp_regs + C71_REG_OUTFMT_BYPASS);

	/* Out format MUXes */
	out_mode = C71_REG_OUTMUX_MODE(C71_REG_OUTMUX_MODE_WIDTH_YUV,
				C71_REG_OUTMUX_MSB_ALIGN_MSB,
				C71_REG_OUTMUX_MODE_YUV_Y_12);
	writel(out_mode, isp->isp_regs + C71_REG_OUTMUX_AXI1_MODE);
	out_mode = C71_REG_OUTMUX_MODE(C71_REG_OUTMUX_MODE_WIDTH_YUV,
				C71_REG_OUTMUX_MSB_ALIGN_MSB,
				C71_REG_OUTMUX_MODE_YUV_U_12);
	writel(out_mode, isp->isp_regs + C71_REG_OUTMUX_AXI2_MODE);
	out_mode = C71_REG_OUTMUX_MODE(C71_REG_OUTMUX_MODE_WIDTH_YUV,
				C71_REG_OUTMUX_MSB_ALIGN_MSB,
				C71_REG_OUTMUX_MODE_YUV_V_12);
	writel(out_mode, isp->isp_regs + C71_REG_OUTMUX_AXI3_MODE);

	/* Rawbuf setup */
	for (i = 0; i < ISP_N_SLOTS; i++) {
		isp_rawbuf_writel(C71_REG_BUF_CONFIG(1, 12, 0), isp,
					slot_rawbuf(i), C71_REG_OFS_BUF_CONFIG);
	}

	/* Outbuf setup */
	for (i = 0; i < ISP_N_SLOTS; i++) {
		/* Y output */
		isp_outbuf_writel(C71_REG_BUF_CONFIG(1, 8, 1), isp,
					slot_outbuf(i, Y_PLANE),
					C71_REG_OFS_BUF_CONFIG);
		/* quarter-size U output */
		isp_outbuf_writel(C71_REG_BUF_CONFIG(1, 8, 1), isp,
					slot_outbuf(i, U_PLANE),
					C71_REG_OFS_BUF_CONFIG);
		/* quarter-size V output */
		isp_outbuf_writel(C71_REG_BUF_CONFIG(1, 8, 1), isp,
					slot_outbuf(i, V_PLANE),
					C71_REG_OFS_BUF_CONFIG);
	}

	for (i = 0; i < ISP_N_SLOTS; i++) {
		(void) isp_slot_setup(isp, i, ISP_WIDTH, ISP_HEIGHT);
		/* Slot i using raw buffer i */
		writel(C71_REG_MCFE_SLOT_BUFS(i,
					C71_REG_MCFE_SLOT_BUF_UNUSED,
					C71_REG_MCFE_SLOT_BUF_UNUSED,
					C71_REG_MCFE_SLOT_BUF_UNUSED),
				isp->isp_regs + C71_REG_MCFE_SLOT_INPUTS(i));
		/* Slot i using output buffer1 4i ... 4i + 2 */
		writel(C71_REG_MCFE_SLOT_BUFS(slot_outbuf(i, Y_PLANE),
						slot_outbuf(i, U_PLANE),
						slot_outbuf(i, V_PLANE),
						C71_REG_MCFE_SLOT_BUF_UNUSED),
				isp->isp_regs + C71_REG_MCFE_SLOT_OUT_BUF1(i));
		/* Second output buffer is unsued for all slots */
		writel(C71_REG_MCFE_SLOT_BUFS(C71_REG_MCFE_SLOT_BUF_UNUSED,
					C71_REG_MCFE_SLOT_BUF_UNUSED,
					C71_REG_MCFE_SLOT_BUF_UNUSED,
					C71_REG_MCFE_SLOT_BUF_UNUSED),
				isp->isp_regs + C71_REG_MCFE_SLOT_OUT_BUF2(i));
	}

	return 0;
}

static void isp_slot_start(struct ispdev *isp, int slot,
				dma_addr_t src_addr, dma_addr_t dst_addr)
{
	unsigned long flags;
	u8 buf;

	spin_lock_irqsave(&isp->irq_lock, flags);

	buf = slot_rawbuf(slot);
	isp_rawbuf_writel(src_addr, isp, buf, C71_REG_OFS_BUF_BASE_ADDR);
	isp_rawbuf_writel(C71_REG_BUF_STATUS_FILLED, isp, buf,
				C71_REG_OFS_BUF_STATUS);

	/* Y plane */
	buf = slot_outbuf(slot, Y_PLANE);
	isp_outbuf_writel(dst_addr +
				ISP_YUV12_Y_OFFSET(isp->isp_slot[slot].width,
						isp->isp_slot[slot].height),
				isp, buf, C71_REG_OFS_BUF_BASE_ADDR);
	isp_outbuf_writel(C71_REG_BUF_STATUS_EMPTY, isp, buf,
				C71_REG_OFS_BUF_STATUS);

	/* followed by U plane */
	buf = slot_outbuf(slot, U_PLANE);
	isp_outbuf_writel(dst_addr +
				ISP_YUV12_U_OFFSET(isp->isp_slot[slot].width,
						isp->isp_slot[slot].height),
				isp, buf, C71_REG_OFS_BUF_BASE_ADDR);
	isp_outbuf_writel(C71_REG_BUF_STATUS_EMPTY, isp, buf,
				C71_REG_OFS_BUF_STATUS);

	/* followed by V plane */
	buf = slot_outbuf(slot, V_PLANE);
	isp_outbuf_writel(dst_addr +
				ISP_YUV12_V_OFFSET(isp->isp_slot[slot].width,
						isp->isp_slot[slot].height),
				isp, buf, C71_REG_OFS_BUF_BASE_ADDR);
	isp_outbuf_writel(C71_REG_BUF_STATUS_EMPTY, isp, buf,
				C71_REG_OFS_BUF_STATUS);

	/* Idle -> SlotN transition triggers the slot */
	writel(C71_REG_MCFE_START_SLOT_IDLE,
			isp->isp_regs + C71_REG_MCFE_START_SLOT);
	writel(slot, isp->isp_regs + C71_REG_MCFE_START_SLOT);

	spin_unlock_irqrestore(&isp->irq_lock, flags);
	mod_timer(&isp->isp_slot[slot].req_timeout, jiffies +
			msecs_to_jiffies(REQUEST_TIMEOUT_MSECS));
}

static void isp_kick_request_queue(struct ispdev *isp, struct isp_slot *slot)
{

	if (!list_empty(&slot->pending_reqs)) {
		struct isp_req *req = list_first_entry(&slot->pending_reqs,
							struct isp_req, list);
		if (req->state == ISP_REQ_NEW) {
			pr_debug("Starting request for slot %d\n", slot->idx);
			req->state = ISP_REQ_PENDING;
			isp_slot_start(isp, slot->idx, req->src_addr,
					req->dst_addr);
		}
	}
}

static void isp_slot_handle_irq(struct ispdev *isp, int slot_num)
{
	struct isp_slot *slot = &isp->isp_slot[slot_num];
	struct isp_req *req;
	unsigned long flags;

	/* mark first request as completed */
	spin_lock_irqsave(&slot->slot_lock, flags);
	if (!list_empty(&slot->pending_reqs)) {
		u8 sts = isp_slot_readb(isp,
				C71_REG_MCFE_SLOT_STATUS_0_3,
				slot_num) & C71_REG_MCFE_SLOT_STATUS_MASK;

		req = list_first_entry(&slot->pending_reqs, struct isp_req,
					list);

		if (sts != 0) {
			printk(KERN_ERR "slot %d status 0x%02x\n",
					slot_num, sts);

			if ((sts & C71_REG_MCFE_SLOT_STATUS_ERROR_BIT) != 0) {
				isp_slot_writeb(C71_REG_MCFE_SLOT_MODE_CANCEL,
					isp, C71_REG_MCFE_SLOT_MODE_0_3,
					slot_num);
				isp_slot_writeb(C71_REG_MCFE_SLOT_MODE_ONESHOT,
					isp, C71_REG_MCFE_SLOT_MODE_0_3,
					slot_num);
				req->state = ISP_REQ_ERROR;
			} else {
				/* XXX - irq came too early or ISP stuck */
			}
		} else {
			/* CDMA valid after successful completion */
			if (slot->cdma_valid == 0) {
				writel(C71_REG_MCFE_SLOT_CFG_FULL_SPACE |
						C71_REG_MCFE_SLOT_CFG_IRIDIX,
					isp->isp_regs +
					C71_REG_MCFE_SLOT_CFG_WRITE_UNITS(slot_num));
				slot->cdma_valid = 1;
			}

			req->state = ISP_REQ_DONE;
		}
		del_timer(&slot->req_timeout);
		list_del(&req->list);
		spin_unlock_irqrestore(&slot->slot_lock, flags);

		req->callback(sts, req->priv);
		kfree(req);
	} else {
		/* Flag spurious completion? */
		spin_unlock_irqrestore(&slot->slot_lock, flags);
	}


	/*
	 * Notify suspend handler of completion if the driver is suspending,
	 * otherwise submit next request, if any
	 */
	spin_lock_irqsave(&slot->slot_lock, flags);
	if (slot->flags & ISP_SLOT_FLAG_SUSPENDING)
		complete(&slot->suspended);
	else
		isp_kick_request_queue(isp, slot);
	spin_unlock_irqrestore(&slot->slot_lock, flags);
}

static void isp_req_timeout(unsigned long data)
{
	struct isp_slot *slot = (struct isp_slot *) data;

	printk(KERN_ERR "isp_req_timeout: slot %d\n", slot->idx);
	/* XXX - reset ISP and cancel v4l layer reqs */
}

static irqreturn_t isp_isr(int irq, void *dev_instance)
{
	struct ispdev *isp = dev_instance;
	u32 start_pending, end_pending, stats_pending, mcfe_pending;
	unsigned long slot_irqs;
	int slot_num;

	/* XXX - should move these to separate irqs */
	start_pending = readl(isp->isp_regs + C71_REG_FE_IRQ_FRAME_START_STS);
	end_pending = readl(isp->isp_regs + C71_REG_FE_IRQ_FRAME_END_STS);
	stats_pending = readl(isp->isp_regs + C71_REG_FE_IRQ_STATS_STS);
	mcfe_pending = readl(isp->isp_regs + C71_REG_FE_IRQ_MCFE_STS);

	if (start_pending)
		writel(start_pending, isp->isp_regs +
			C71_REG_FE_IRQ_FRAME_START_CLR);
	if (end_pending)
		writel(end_pending, isp->isp_regs +
			C71_REG_FE_IRQ_FRAME_END_CLR);
	if (stats_pending)
		writel(stats_pending, isp->isp_regs +
			C71_REG_FE_IRQ_STATS_CLR);
	if (mcfe_pending)
		writel(mcfe_pending, isp->isp_regs +
			C71_REG_FE_IRQ_MCFE_CLR);

	slot_irqs = (mcfe_pending & C71_REG_FE_IRQ_MCFE_SLOT_MASK) >>
			C71_REG_FE_IRQ_MCFE_SLOT_SHIFT;
	for_each_set_bit(slot_num, &slot_irqs, ISP_N_SLOTS)
		isp_slot_handle_irq(isp, slot_num);


	return IRQ_RETVAL((start_pending | end_pending | stats_pending |
				mcfe_pending) != 0);
}

struct isp_req *isp_buf_submit(struct ispdev *isp, int slot,
				dma_addr_t src_addr, dma_addr_t dst_addr,
				void (*callback)(u32, void *),
				void *priv)
{
	struct isp_req *req;
	unsigned long flags;
	int was_empty;

	if (slot < 0 || slot >= ISP_N_SLOTS)
		return ERR_PTR(-EINVAL);

	/* XXX - move submit to threaded bh so we don't need atomic alloc */
	req = kzalloc(sizeof(*req), GFP_ATOMIC);
	if (!req)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&req->list);
	req->src_addr = src_addr;
	req->dst_addr = dst_addr;
	req->callback = callback;
	req->priv = priv;

	spin_lock_irqsave(&isp->isp_slot[slot].slot_lock, flags);
	was_empty = list_empty(&isp->isp_slot[slot].pending_reqs);
	list_add_tail(&req->list, &isp->isp_slot[slot].pending_reqs);
	if (was_empty &&
		!(isp->isp_slot[slot].flags & ISP_SLOT_FLAG_SUSPENDING)) {
		req->state = ISP_REQ_PENDING;
		isp_slot_start(isp, slot, src_addr, dst_addr);
	}
	spin_unlock_irqrestore(&isp->isp_slot[slot].slot_lock, flags);

	return req;
}

static void isp_cdma_read_pio(struct ispdev *isp, uint32_t *cdma_buf)
{
	u32 ofs;

	/* Read entire reg space 32 bits at a time */
	for (ofs = 0; ofs < C71_CDMA_BUF_LEN; ofs += 4)
		*cdma_buf++ = readl(isp->isp_regs + ofs);
}


uint8_t *isp_get_cdma_buf(struct ispdev *isp, int slot_num,
				enum isp_cdma_buf_op dir)
{
	return (uint8_t *) isp->isp_slot[slot_num].cdma_buf;
}

void isp_put_cdma_buf(struct ispdev *isp, int slot_num,
				enum isp_cdma_buf_op dir)
{
	if (dir == ISP_CDMA_OP_SET) {
		struct isp_slot *slot = &isp->isp_slot[slot_num];

		/* mark CDMA valid after set operation */
		if (slot->cdma_valid == 0) {
			writel(C71_REG_MCFE_SLOT_CFG_FULL_SPACE,
				isp->isp_regs +
				C71_REG_MCFE_SLOT_CFG_WRITE_UNITS(slot_num));
			// slot->cdma_valid = 1;
		}
	}
}

static void isp_init_slots(struct platform_device *pdev, struct ispdev *isp)
{
	int i;
	dma_addr_t junk;

	/*
	 * ISP seems to hate CDMA buffer at 0xfffe0000, returns slot status
	 * of 8 (error).  So burn a page in the iommu to guarantee we never
	 * get this offset (less wasteful of iova than reducing mask.)
	 */
	(void) dmam_alloc_coherent(&pdev->dev, PAGE_SIZE, &junk,
					GFP_KERNEL);

	for (i = 0; i < ISP_N_SLOTS; i++) {
		struct isp_slot *slot = &isp->isp_slot[i];

		slot->isp = isp;
		slot->idx = i;
		spin_lock_init(&slot->slot_lock);
		INIT_LIST_HEAD(&slot->pending_reqs);
		init_completion(&slot->suspended);
		init_timer(&slot->req_timeout);
		slot->req_timeout.function = isp_req_timeout;
		slot->req_timeout.data = (unsigned long) slot;
		slot->cdma_buf = dmam_alloc_coherent(&pdev->dev,
			C71_CDMA_BUF_LEN, &slot->cdma_buf_iova,
			GFP_KERNEL);
		memset(slot->cdma_buf, 0, C71_CDMA_BUF_LEN);
	}
}


static void isp_init_cdma(struct platform_device *pdev, struct ispdev *isp)
{
	int i;

	/* Read once and then populate all the rest from that */
	isp_cdma_read_pio(isp, isp->isp_slot[0].cdma_buf);
	for (i = 0; i < ISP_N_SLOTS; i++)
		memcpy(isp->isp_slot[i].cdma_buf, isp->isp_slot[0].cdma_buf,
			C71_CDMA_BUF_LEN);

}

static void isp_interrupt_setup(struct platform_device *pdev)
{
	struct ispdev *isp = platform_get_drvdata(pdev);

	writel(0xffff, isp->isp_regs + C71_REG_FE_IRQ_FRAME_START_CLR);
	writel(0xffff, isp->isp_regs + C71_REG_FE_IRQ_FRAME_END_CLR);
	writel(0x1ffff, isp->isp_regs + C71_REG_FE_IRQ_STATS_CLR);
	writel(0xfffff, isp->isp_regs + C71_REG_FE_IRQ_MCFE_CLR);
	writel(0xffff << C71_REG_FE_IRQ_MCFE_MASK_SLOTS_SHIFT,
			isp->isp_regs + C71_REG_FE_IRQ_MCFE_MASK);
}


static int isp_probe(struct platform_device *pdev)
{
	struct ispdev *isp;
	struct resource *res;
	int ret, irq;
	u32 cs_rev, prod_id, prod_ver, hw_rev;

	if (!pdev->dev.of_node)
		return -EINVAL;

	isp = devm_kzalloc(&pdev->dev, sizeof(*isp), GFP_KERNEL);
	if (!isp)
		return -ENOMEM;
	spin_lock_init(&isp->irq_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	isp->isp_regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(isp->isp_regs))
		return PTR_ERR(isp->isp_regs);
	dev_info(&pdev->dev, "regs start: 0x%lx, len 0x%lx\n",
		(unsigned long) res->start,
		(unsigned long) (res->end - res->start + 1));

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		dev_err(&pdev->dev, "failed to set dma mask\n");

	if (pdev->dev.dma_parms == NULL)
		pdev->dev.dma_parms = devm_kzalloc(&pdev->dev,
			sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	if (pdev->dev.dma_parms == NULL)
		return -ENOMEM;
	dma_set_max_seg_size(&pdev->dev, DMA_BIT_MASK(32));

	isp_init_slots(pdev, isp);

	platform_set_drvdata(pdev, isp);

	cs_rev = readl(isp->isp_regs + C71_REG_ID_API);
	dev_info(&pdev->dev, "Config space revision: 0x%x\n", cs_rev);
	prod_id = readl(isp->isp_regs + C71_REG_ID_PRODUCT);
	dev_info(&pdev->dev, "Product identification number: 0x%x\n", prod_id);
	prod_ver = readl(isp->isp_regs + C71_REG_ID_VERSION);
	dev_info(&pdev->dev, "Product version identifier: 0x%x\n", prod_ver);
	hw_rev = readl(isp->isp_regs + C71_REG_ID_REVISION);
	dev_info(&pdev->dev, "ISP hardware revision number: 0x%x\n", hw_rev);

	isp_interrupt_setup(pdev);

	/* we only map irqs to first vector for now */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get irq 0\n");
		return -ENXIO;
	}
	ret = devm_request_irq(&pdev->dev, irq, isp_isr, 0x0, "isp", isp);
	if (ret) {
		dev_err(&pdev->dev, "failed to register irq 0\n");
		return ret;
	}

	isp_dma_setup(pdev);
	isp_init_cdma(pdev, isp);

	ret = mali_isp_register_if(pdev);
	if (ret)
		return ret;

	return 0;
}

static int isp_remove(struct platform_device *pdev)
{
	mali_isp_unregister_if(pdev);
	return 0;
}

static const struct of_device_id isp_dt_match[] = {
	{ .compatible = "arm,mali-c71" },
	{},
};

#ifdef CONFIG_PM_SLEEP


static int isp_req_pending(struct isp_slot *slot)
{

	if (!list_empty(&slot->pending_reqs)) {
		struct isp_req *req = list_first_entry(&slot->pending_reqs,
							struct isp_req, list);
		if (req->state == ISP_REQ_PENDING)
			return 1;
	}

	return 0;
}

static int isp_wait_for_pending_requests(struct platform_device *pdev)
{
	int i;

	dev_dbg(&pdev->dev,
			"Waiting up to %d ms for ISP slots to go idle\n",
			REQUEST_TIMEOUT_MSECS);

	for (i = 0; i < ISP_N_SLOTS; i++) {
		struct ispdev *isp = platform_get_drvdata(pdev);
		struct isp_slot *slot = &isp->isp_slot[i];
		int pending;
		int ret;

		spin_lock_irq(&slot->slot_lock);
		pending = isp_req_pending(slot);
		spin_unlock_irq(&slot->slot_lock);

		if (!pending)
			continue;

		dev_dbg(&pdev->dev, "waiting for slot %d\n", slot->idx);
		ret = wait_for_completion_timeout(&slot->suspended,
				msecs_to_jiffies(REQUEST_TIMEOUT_MSECS));
		if (!ret) {
			dev_err(&pdev->dev, "timeout waiting for slot%d", i);
			return -EBUSY;
		}
	}

	dev_dbg(&pdev->dev, "All slots stopped\n");

	return 0;
}

static void isp_suspend_processing(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ISP_N_SLOTS; i++) {
		struct ispdev *isp = platform_get_drvdata(pdev);
		struct isp_slot *slot = &isp->isp_slot[i];

		spin_lock_irq(&slot->slot_lock);
		slot->flags |= ISP_SLOT_FLAG_SUSPENDING;
		spin_unlock_irq(&slot->slot_lock);
	}

}

static void isp_save_slots_config(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ISP_N_SLOTS; i++) {
		struct ispdev *isp = platform_get_drvdata(pdev);
		struct isp_slot *slot = &isp->isp_slot[i];

		slot->cdma_write_config = readl(isp->isp_regs +
				C71_REG_MCFE_SLOT_CFG_WRITE_UNITS(i));
	}

}

static void isp_restore_slots_config(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ISP_N_SLOTS; i++) {
		struct ispdev *isp = platform_get_drvdata(pdev);
		struct isp_slot *slot = &isp->isp_slot[i];

		writel(slot->cdma_write_config, isp->isp_regs +
				C71_REG_MCFE_SLOT_CFG_WRITE_UNITS(i));
	}
}

static void isp_resume_processing(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ISP_N_SLOTS; i++) {
		struct ispdev *isp = platform_get_drvdata(pdev);
		struct isp_slot *slot = &isp->isp_slot[i];

		spin_lock_irq(&slot->slot_lock);
		slot->flags &= ~ISP_SLOT_FLAG_SUSPENDING;
		reinit_completion(&slot->suspended);
		isp_kick_request_queue(isp, slot);
		spin_unlock_irq(&slot->slot_lock);
	}
}

static int isp_suspend(struct device *dev)
{

	struct platform_device *pdev = to_platform_device(dev);
	int ret;

	dev_dbg(dev, "Suspending...\n");

	isp_suspend_processing(pdev);

	ret = isp_wait_for_pending_requests(pdev);
	if (ret != 0) {
		isp_resume_processing(pdev);
		return ret;
	}

	isp_save_slots_config(pdev);

	return 0;
}

static int isp_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	dev_dbg(dev, "Resuming...\n");

	isp_interrupt_setup(pdev);
	isp_dma_setup(pdev);
	isp_restore_slots_config(pdev);
	isp_resume_processing(pdev);

	return 0;
}

#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(isp_pm_ops, isp_suspend, isp_resume);

static struct platform_driver isp_driver = {
	.driver		= {
		.name = "isp",
		.pm = &isp_pm_ops,
		.of_match_table = isp_dt_match,
	},
	.probe		= isp_probe,
	.remove		= isp_remove,
};

module_platform_driver(isp_driver);

MODULE_DESCRIPTION("ARM Mali C71 Image Signal Processor driver");
MODULE_AUTHOR("Tesla, Inc.");
MODULE_LICENSE("GPL v2");

