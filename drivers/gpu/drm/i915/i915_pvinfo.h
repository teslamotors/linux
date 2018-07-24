/*
 * Copyright(c) 2011-2016 Intel Corporation. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _I915_PVINFO_H_
#define _I915_PVINFO_H_

/* The MMIO offset of the shared info between guest and host emulator */
#define VGT_PVINFO_PAGE	0x78000
#define VGT_PVINFO_SIZE	0x1000

/* Scratch reg used for redirecting command access to registers, any
 * command access to PVINFO page would be discarded, so it has no HW
 * impact.
 */
#define VGT_SCRATCH_REG VGT_PVINFO_PAGE

/*
 * The following structure pages are defined in GEN MMIO space
 * for virtualization. (One page for now)
 */
#define VGT_MAGIC         0x4776544776544776ULL	/* 'vGTvGTvG' */
#define VGT_VERSION_MAJOR 1
#define VGT_VERSION_MINOR 0

/*
 * notifications from guest to vgpu device model
 */
enum vgt_g2v_type {
	VGT_G2V_PPGTT_L3_PAGE_TABLE_CREATE = 2,
	VGT_G2V_PPGTT_L3_PAGE_TABLE_DESTROY,
	VGT_G2V_PPGTT_L4_PAGE_TABLE_CREATE,
	VGT_G2V_PPGTT_L4_PAGE_TABLE_DESTROY,
	VGT_G2V_EXECLIST_CONTEXT_CREATE,
	VGT_G2V_EXECLIST_CONTEXT_DESTROY,
	VGT_G2V_MAX,
};

#define PLANE_COLOR_CTL_BIT	(1 << 0)
#define PLANE_KEY_BIT		(1 << 1)
#define PLANE_SCALER_BIT	(1 << 2)

struct pv_plane_update {
	u32 flags;
	u32 plane_color_ctl;
	u32 plane_key_val;
	u32 plane_key_max;
	u32 plane_key_msk;
	u32 plane_offset;
	u32 plane_stride;
	u32 plane_size;
	u32 plane_aux_dist;
	u32 plane_aux_offset;
	u32 ps_ctrl;
	u32 ps_pwr_gate;
	u32 ps_win_ps;
	u32 ps_win_sz;
	u32 plane_pos;
	u32 plane_ctl;
};

/* shared page(4KB) between gvt and VM, located at the first page next
 * to MMIO region(2MB size normally).
 */
struct gvt_shared_page {
	u32 elsp_data[4];
	u32 reg_addr;
	struct pv_plane_update pv_plane;
	u32 rsvd2[0x400 - 21];
};

#define VGPU_PVMMIO(vgpu) vgpu_vreg(vgpu, vgtif_reg(enable_pvmmio))

/*
 * VGT capabilities type
 */
#define VGT_CAPS_FULL_48BIT_PPGTT	BIT(2)

/*
 * define different levels of PVMMIO optimization
 */
enum pvmmio_levels {
	PVMMIO_ELSP_SUBMIT = 0x1,
	PVMMIO_PLANE_UPDATE = 0x2,
};

struct vgt_if {
	u64 magic;		/* VGT_MAGIC */
	u16 version_major;
	u16 version_minor;
	u32 vgt_id;		/* ID of vGT instance */
	u32 vgt_caps;		/* VGT capabilities */
	u32 rsv1[11];		/* pad to offset 0x40 */
	/*
	 *  Data structure to describe the balooning info of resources.
	 *  Each VM can only have one portion of continuous area for now.
	 *  (May support scattered resource in future)
	 *  (starting from offset 0x40)
	 */
	struct {
		/* Aperture register balooning */
		struct {
			u32 base;
			u32 size;
		} mappable_gmadr;	/* aperture */
		/* GMADR register balooning */
		struct {
			u32 base;
			u32 size;
		} nonmappable_gmadr;	/* non aperture */
		/* allowed fence registers */
		u32 fence_num;
		u32 rsv2[3];
	} avail_rs;		/* available/assigned resource */
	u32 rsv3[0x200 - 24];	/* pad to half page */
	/*
	 * The bottom half page is for response from Gfx driver to hypervisor.
	 */
	u32 rsv4;
	u32 display_ready;	/* ready for display owner switch */

	u32 rsv5[4];

	u32 g2v_notify;
	u32 rsv6[7];

	struct {
		u32 lo;
		u32 hi;
	} pdp[4];

	u32 execlist_context_descriptor_lo;
	u32 execlist_context_descriptor_hi;
	u32 enable_pvmmio;
	u32 pv_mmio; /* vgpu trapped mmio read will be redirected here */
	u32 scaler_owned;

	u32  rsv7[0x200 - 27];    /* pad to one page */
} __packed;

#define vgtif_reg(x) \
	_MMIO((VGT_PVINFO_PAGE + offsetof(struct vgt_if, x)))

/* vGPU display status to be used by the host side */
#define VGT_DRV_DISPLAY_NOT_READY 0
#define VGT_DRV_DISPLAY_READY     1  /* ready for display switch */

#endif /* _I915_PVINFO_H_ */
