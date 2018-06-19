/* SPDX-License_Identifier: GPL-2.0 */
/* Copyright (C) 2013 - 2018 Intel Corporation */

#ifndef IPU_PLATFORM_H
#define IPU_PLATFORM_H

#define IPU_NAME			"intel-ipu4"

#ifdef CONFIG_VIDEO_INTEL_IPU4
#define IPU_CPD_FIRMWARE_NAME	"ipu4_cpd_b0.bin"
#else
#define IPU_CPD_FIRMWARE_NAME	"ipu4p_cpd.bin"
#endif

/*
 * The following definitions are encoded to the media_device's model field so
 * that the software components which uses IPU driver can get the hw stepping
 * information.
 */
#ifdef CONFIG_VIDEO_INTEL_IPU4
#define IPU_MEDIA_DEV_MODEL_NAME	"ipu4/Broxton B"
#else
#define IPU_MEDIA_DEV_MODEL_NAME	"ipu4p"
#endif

#ifdef CONFIG_VIDEO_INTEL_IPU4

#define IPU_HW_BXT_P_B1_REV	0xa
#define IPU_HW_BXT_P_D0_REV	0xb
#define IPU_HW_BXT_P_E0_REV	0xc

#define IPU_ISYS_NUM_STREAMS            8       /* Max 8 */

/* BXTP E0 has icache bug fixed */
#define is_ipu_hw_bxtp_e0(isp)			\
	({ typeof(isp) __isp = (isp); \
	(__isp->pdev->device == IPU_PCI_ID &&		\
	 __isp->pdev->revision == IPU_HW_BXT_P_E0_REV); })
#endif

/* declearations, definitions in ipu4.c */
extern const struct ipu_isys_internal_pdata isys_ipdata;
extern const struct ipu_psys_internal_pdata psys_ipdata;
extern const struct ipu_buttress_ctrl isys_buttress_ctrl;
extern const struct ipu_buttress_ctrl psys_buttress_ctrl;

/* definitions in ipu4-isys.c */
extern struct ipu_trace_block isys_trace_blocks[];
/* definitions in ipu4-psys.c */
extern struct ipu_trace_block psys_trace_blocks[];

#endif
