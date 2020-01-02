/*
 *  skl.h - HD Audio skylake defintions.
 *
 *  Copyright (C) 2015 Intel Corp
 *  Author: Jeeja KP <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#ifndef __SOUND_SOC_SKL_H
#define __SOUND_SOC_SKL_H

#include <sound/hda_register.h>
#include <sound/hdaudio_ext.h>
#include "skl-nhlt.h"
#include "skl-tplg-interface.h"
#include "skl-sst-dsp.h"

#define SKL_SUSPEND_DELAY 2000

/* Vendor Specific Registers */
#define AZX_REG_VS_EM1			0x1000
#define AZX_REG_VS_INRC			0x1004
#define AZX_REG_VS_OUTRC		0x1008
#define AZX_REG_VS_FIFOTRK		0x100C
#define AZX_REG_VS_FIFOTRK2		0x1010
#define AZX_REG_VS_EM2			0x1030
#define AZX_REG_VS_EM3L			0x1038
#define AZX_REG_VS_EM3U			0x103C
#define AZX_REG_VS_EM4L			0x1040
#define AZX_REG_VS_EM4U			0x1044
#define AZX_REG_VS_LTRC			0x1048
#define AZX_REG_VS_D0I3C		0x104A
#define AZX_REG_VS_PCE			0x104B
#define AZX_REG_VS_L2MAGC		0x1050
#define AZX_REG_VS_L2LAHPT		0x1054
#define AZX_REG_VS_SDXDPIB_XBASE	0x1084
#define AZX_REG_VS_SDXDPIB_XINTERVAL	0x20
#define AZX_REG_VS_SDXEFIFOS_XBASE	0x1094
#define AZX_REG_VS_SDXEFIFOS_XINTERVAL	0x20

/* D0I3C Register fields */
#define AZX_REG_VS_D0I3C_CIP      0x1 /* Command in progress */
#define AZX_REG_VS_D0I3C_I3       0x4 /* D0i3 enable */
#define AZX_PCIREG_PGCTL		0x44
#define AZX_PGCTL_LSRMD_MASK		(1 << 4)
#define AZX_PGCTL_ADSPPGD_MASK		(1 << 2)
#define AZX_PCIREG_CGCTL		0x48
#define AZX_CGCTL_MISCBDCGE_MASK	(1 << 6)
#define AZX_CGCTL_ADSPDCGE_MASK		(1 << 1)
#define AZX_EM2_DUM_MASK		(1 << 23)

struct skl_dsp_resource {
	u32 max_mcps;
	u32 max_mem;
	u32 mcps;
	u32 mem;
};

struct skl_debug;
struct snd_soc_dapm_widget;

struct dsp_init {
	void __iomem *mmio_base;
	int irq;
	struct skl_dsp_loader_ops dsp_ops;
	u8 num_ssp;
};

struct skl {
	struct hdac_ext_bus ebus;
	struct pci_dev *pci;

	unsigned int init_failed:1; /* delayed init failed */
	struct platform_device *dmic_dev;
	struct platform_device *i2s_dev;

	struct nhlt_acpi_table *nhlt; /* nhlt ptr */
	struct skl_sst *skl_sst; /* sst skl ctx */

	struct skl_dsp_resource resource;
	struct list_head ppl_list;
	const struct firmware *tplg;

	struct skl_debug *debugfs;
	bool nhlt_override;
	int supend_active;

	struct mutex thread_irq_lock;

};

struct platform_info {
	u32 sram0_base;
	u32 sram1_base;
	u32 w0_stat_sz;
	u32 w0_up_sz;
	size_t in_size;
	size_t out_size;
	void __iomem *lpe;
	void __iomem *in_base;
	void __iomem *out_base;
};

#define skl_to_ebus(s)	(&(s)->ebus)
#define ebus_to_skl(sbus) \
	container_of(sbus, struct skl, sbus)

/* to pass dai dma data */
struct skl_dma_params {
	u32 format;
	u8 stream_tag;
};

int skl_platform_unregister(struct device *dev);
int skl_platform_register(struct device *dev);

struct nhlt_acpi_table *skl_nhlt_init(struct device *dev);
void skl_nhlt_free(struct nhlt_acpi_table *addr);
struct nhlt_specific_cfg *skl_get_ep_blob(struct skl *skl, u32 instance,
			u8 link_type, u8 s_fmt, u8 no_ch, u32 s_rate, u8 dirn);

int skl_init_dsp_hw(struct skl *skl, u32 num_ssp);
int skl_init_dsp_fw(struct skl *skl);
int skl_free_dsp(struct skl *skl);
int skl_suspend_dsp(struct skl *skl);
int skl_resume_dsp(struct skl *skl);
int skl_free(struct hdac_ext_bus *ebus);
void skl_update_d0i3c(struct device *dev, bool enable);
void skl_enable_miscbdcge(struct device *dev, bool enable);

struct skl_module_cfg;

#ifdef CONFIG_DEBUG_FS

struct skl_debug *skl_debugfs_init(struct skl *skl);
void skl_debugfs_exit(struct skl_debug *d);
struct nhlt_specific_cfg
*skl_nhlt_get_debugfs_blob(struct skl_debug *d, u8 link_type, u32 instance,
			u8 stream);
void skl_debug_init_module(struct skl_debug *d,
	struct snd_soc_dapm_widget *w, struct skl_module_cfg *mconfig);

void skl_update_dsp_debug_info(struct skl_debug *d,
		struct platform_info *dbg_info);

#else

struct skl_debug {
};

static inline struct skl_debug *skl_debugfs_init(struct skl *skl)
{
	return NULL;
}
static inline void skl_debugfs_exit(struct skl_debug *d)
{
}

static inline struct nhlt_specific_cfg
*skl_nhlt_get_debugfs_blob(struct skl_debug *d, u8 link_type, u32 instance, u8 stream)
{
	return NULL;
}

static inline void skl_debug_init_module(struct skl_debug *d,
			struct snd_soc_dapm_widget *w,
			struct skl_module_cfg *mconfig)
{
}

static inline void skl_update_dsp_debug_info(struct skl_debug *d,
		struct platform_info *dbg_info)
{
}

#endif

void skl_cleanup_resources(struct skl *skl);
#endif /* __SOUND_SOC_SKL_H */
