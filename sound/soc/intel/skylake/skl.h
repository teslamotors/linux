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
#include <sound/soc.h>
#include "skl-nhlt.h"

#define SKL_SUSPEND_DELAY 2000

#define SKL_MAX_ASTATE_CFG		3

#define AZX_PCIREG_PGCTL		0x44
#define AZX_PGCTL_LSRMD_MASK		(1 << 4)
#define AZX_PCIREG_CGCTL		0x48
#define AZX_CGCTL_MISCBDCGE_MASK	(1 << 6)
/* D0I3C Register fields */
#define AZX_REG_VS_D0I3C_CIP      0x1 /* Command in progress */
#define AZX_REG_VS_D0I3C_I3       0x4 /* D0i3 enable */
#define AZX_EM2_DUM_MASK		(1 << 23)

#define SKL_MAX_DMACTRL_CFG	7
#define SKL_MAX_DMA_CFG		24
#define BXT_INSTANCE_ID		0
#define BXT_BASE_FW_MODULE_ID	0


struct skl_dsp_resource {
	u32 max_mcps;
	u32 max_mem;
	u32 mcps;
	u32 mem;
};

struct skl_debug;

struct skl_dma_config {
	u32 min_size;
	u32 max_size;
} __packed;

struct skl_dma_buff_cfg {
	u32 type;
	u32 size;
	struct skl_dma_config dma_cfg[SKL_MAX_DMA_CFG];
} __packed;

struct skl_dmctrl_hdr {
	u32 vbus_id;
	u32 freq;
	u32 tdm_slot;
	u32 fmt;
	u32 direction;
	u32 ch;
	u32 data_size;
	u32 *data;
} __packed;

struct skl_dmactrl_config {
	u32 type;
	u32 size;
	u32 idx;
	struct skl_dmctrl_hdr hdr[SKL_MAX_DMACTRL_CFG];
} __packed;

struct skl_astate_config {
	u32 kcps;
	u32 clk_src;
};

struct skl_astate_cfg {
	u32 count;
	struct skl_astate_config astate_table[0];
};

struct skl_fw_cfg_info {
	struct skl_dma_buff_cfg dmacfg;
	struct skl_dmactrl_config dmactrl_cfg;
	struct skl_astate_cfg *astate_cfg;
} __packed;

struct ep_group_cnt {
	int cnt;
	int *vbus_id;
};

struct skl {
	struct hdac_ext_bus ebus;
	struct pci_dev *pci;

	unsigned int init_done:1; /* delayed init status */
	struct platform_device *dmic_dev;
	struct platform_device *i2s_dev;
	struct snd_soc_platform *platform;

	unsigned int nhlt_version;
	struct nhlt_acpi_table *nhlt; /* nhlt ptr */
	struct skl_sst *skl_sst; /* sst skl ctx */

	struct skl_dsp_resource resource;
	struct list_head ppl_list;
	struct list_head bind_list;

	const char *fw_name;
	char tplg_name[64];
	unsigned short pci_id;
	const struct firmware *tplg;

	int supend_active;

	struct work_struct probe_work;

	struct skl_debug *debugfs;
	u8 nr_modules;
	struct skl_module **modules;
	bool nhlt_override;
	bool mod_set_get_status;
	struct ep_group_cnt grp_cnt;
	struct skl_fw_cfg_info cfg;
	struct snd_soc_acpi_mach *mach;
	bool use_tplg_pcm;
};

#define skl_to_ebus(s)	(&(s)->ebus)
#define ebus_to_skl(sbus) \
	container_of(sbus, struct skl, sbus)

/* to pass dai dma data */
struct skl_dma_params {
	u32 format;
	u8 stream_tag;
};

/* to pass dmic data */
struct skl_machine_pdata {
	u32 dmic_num;
	bool use_tplg_pcm; /* use total dais and dai links from topology*/
};

int skl_platform_unregister(struct device *dev);
int skl_platform_register(struct device *dev);

int skl_get_nhlt_version(struct device *dev);
struct nhlt_acpi_table *skl_nhlt_init(struct device *dev);
void skl_nhlt_free(struct nhlt_acpi_table *addr);
struct nhlt_specific_cfg *skl_get_ep_blob(struct skl *skl, u32 instance,
					u8 link_type, u8 s_fmt, u8 no_ch,
					u32 s_rate, u8 dirn, u8 dev_type);
struct nhlt_specific_cfg *
skl_get_nhlt_specific_cfg(struct skl *skl, u32 instance, u8 link_type,
		u8 s_fmt, u8 num_ch, u32 s_rate, u8 dir, u8 dev_type);

int skl_get_dmic_geo(struct skl *skl);
int skl_nhlt_update_topology_bin(struct skl *skl);
int skl_init_dsp(struct skl *skl);
int skl_free_dsp(struct skl *skl);
int skl_suspend_late_dsp(struct skl *skl);
int skl_suspend_dsp(struct skl *skl);
int skl_resume_dsp(struct skl *skl);
void skl_cleanup_resources(struct skl *skl);
void skl_update_d0i3c(struct device *dev, bool enable);
int skl_nhlt_create_sysfs(struct skl *skl);
void skl_nhlt_remove_sysfs(struct skl *skl);

struct skl_module_cfg;

#ifdef CONFIG_DEBUG_FS
struct skl_debug *skl_debugfs_init(struct skl *skl);
void skl_debug_init_module(struct skl_debug *d,
			struct snd_soc_dapm_widget *w,
			struct skl_module_cfg *mconfig);
struct nhlt_specific_cfg
*skl_nhlt_get_debugfs_blob(struct skl_debug *d, u8 link_type, u32 instance,
			u8 stream);
void skl_dbg_event(struct skl_sst *ctx, int type);
#else
static inline struct skl_debug *skl_debugfs_init(struct skl *skl)
{
	return NULL;
}
static inline void skl_debug_init_module(struct skl_debug *d,
					 struct snd_soc_dapm_widget *w,
					 struct skl_module_cfg *mconfig)
{}
static inline struct nhlt_specific_cfg
*skl_nhlt_get_debugfs_blob(struct skl_debug *d, u8 link_type, u32 instance,
			u8 stream)
{
	return NULL;
}
static inline void skl_dbg_event(struct skl_sst *ctx, int type)
{
}
#endif

#endif /* __SOUND_SOC_SKL_H */
