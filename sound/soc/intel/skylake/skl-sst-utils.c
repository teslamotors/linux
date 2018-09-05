/*
 *  skl-sst-utils.c - SKL sst utils functions
 *
 *  Copyright (C) 2016 Intel Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uuid.h>
#include <linux/devcoredump.h>
#include <linux/pci.h>
#include "skl-sst-dsp.h"
#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "skl-sst-ipc.h"


#define UUID_STR_SIZE 37
#define TYPE0_EXCEPTION 0
#define TYPE1_EXCEPTION 1
#define TYPE2_EXCEPTION 2
#define MAX_CRASH_DATA_TYPES 3
#define CRASH_DUMP_VERSION 0x1
/* FW Extended Manifest Header id = $AE1 */
#define SKL_EXT_MANIFEST_HEADER_MAGIC   0x31454124

#define UUID_ATTR_RO(_name) \
	struct uuid_attribute uuid_attr_##_name = __ATTR_RO(_name)

struct skl_sysfs_tree {
	struct kobject *dsp_kobj;
	struct kobject *modules_kobj;
	struct skl_sysfs_module **mod_obj;
};

struct skl_sysfs_module {
	struct kobject kobj;
	struct uuid_module *uuid_mod;
	struct list_head *module_list;
	int fw_ops_load_mod;
};

struct uuid_attribute {
	struct attribute	attr;
	ssize_t (*show)(struct skl_sysfs_module *modinfo_obj,
			struct uuid_attribute *attr, char *buf);
	ssize_t (*store)(struct skl_sysfs_module *modinfo_obj,
			struct uuid_attribute *attr, const char *buf,
			size_t count);
};

struct UUID {
	u8 id[16];
};

union seg_flags {
	u32 ul;
	struct {
		u32 contents : 1;
		u32 alloc    : 1;
		u32 load     : 1;
		u32 read_only : 1;
		u32 code     : 1;
		u32 data     : 1;
		u32 _rsvd0   : 2;
		u32 type     : 4;
		u32 _rsvd1   : 4;
		u32 length   : 16;
	} r;
} __packed;

struct segment_desc {
	union seg_flags flags;
	u32 v_base_addr;
	u32 file_offset;
};

struct module_type {
	u32 load_type  : 4;
	u32 auto_start : 1;
	u32 domain_ll  : 1;
	u32 domain_dp  : 1;
	u32 rsvd       : 25;
} __packed;

struct adsp_module_entry {
	u32 struct_id;
	u8  name[8];
	struct UUID uuid;
	struct module_type type;
	u8  hash1[DEFAULT_HASH_SHA256_LEN];
	u32 entry_point;
	u16 cfg_offset;
	u16 cfg_count;
	u32 affinity_mask;
	u16 instance_max_count;
	u16 instance_bss_size;
	struct segment_desc segments[3];
} __packed;

struct adsp_fw_hdr {
	u32 id;
	u32 len;
	u8  name[8];
	u32 preload_page_count;
	u32 fw_image_flags;
	u32 feature_mask;
	u16 major;
	u16 minor;
	u16 hotfix;
	u16 build;
	u32 num_modules;
	u32 hw_buf_base;
	u32 hw_buf_length;
	u32 load_offset;
} __packed;

struct skl_ext_manifest_hdr {
	u32 id;
	u32 len;
	u16 version_major;
	u16 version_minor;
	u32 entries;
};

struct adsp_crash_hdr {
	u16 type;
	u16 length;
	char data[0];
} __packed;

struct adsp_type0_crash_data {
	u32 crash_dump_ver;
	u16 bus_dev_id;
	u16 cavs_hw_version;
	struct fw_version fw_ver;
	struct sw_version sw_ver;
} __packed;

struct adsp_type1_crash_data {
	u32 mod_uuid[4];
	u32 hash[2];
	u16 mod_id;
	u16 rsvd;
} __packed;

struct adsp_type2_crash_data {
	u32 fwreg[FW_REG_SZ];
} __packed;

static int skl_get_pvtid_map(struct uuid_module *module, int instance_id)
{
	int pvt_id;

	for (pvt_id = 0; pvt_id < module->max_instance; pvt_id++) {
		if (module->instance_id[pvt_id] == instance_id)
			return pvt_id;
	}
	return -EINVAL;
}

int skl_get_module_id(struct skl_sst *ctx, uuid_le *uuid_mod)
{
	struct uuid_module *module;

	list_for_each_entry(module, &ctx->uuid_list, list) {
		if (uuid_le_cmp(*uuid_mod, module->uuid) == 0)
			return module->id;
	}
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(skl_get_module_id);

int skl_get_pvt_instance_id_map(struct skl_sst *ctx,
				int module_id, int instance_id)
{
	struct uuid_module *module;

	list_for_each_entry(module, &ctx->uuid_list, list) {
		if (module->id == module_id)
			return skl_get_pvtid_map(module, instance_id);
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(skl_get_pvt_instance_id_map);

static inline int skl_getid_32(struct uuid_module *module, u64 *val,
				int word1_mask, int word2_mask)
{
	int index, max_inst, pvt_id;
	u32 mask_val;

	max_inst =  module->max_instance;
	mask_val = (u32)(*val >> word1_mask);

	if (mask_val != 0xffffffff) {
		index = ffz(mask_val);
		pvt_id = index + word1_mask + word2_mask;
		if (pvt_id <= (max_inst - 1)) {
			*val |= 1ULL << (index + word1_mask);
			return pvt_id;
		}
	}

	return -EINVAL;
}

static inline int skl_pvtid_128(struct uuid_module *module)
{
	int j, i, word1_mask, word2_mask = 0, pvt_id;

	for (j = 0; j < MAX_INSTANCE_BUFF; j++) {
		word1_mask = 0;

		for (i = 0; i < 2; i++) {
			pvt_id = skl_getid_32(module, &module->pvt_id[j],
						word1_mask, word2_mask);
			if (pvt_id >= 0)
				return pvt_id;

			word1_mask += 32;
			if ((word1_mask + word2_mask) >= module->max_instance)
				return -EINVAL;
		}

		word2_mask += 64;
		if (word2_mask >= module->max_instance)
			return -EINVAL;
	}

	return -EINVAL;
}

/**
 * skl_get_pvt_id: generate a private id for use as module id
 *
 * @ctx: driver context
 * @mconfig: module configuration data
 *
 * This generates a 128 bit private unique id for a module TYPE so that
 * module instance is unique
 */
int skl_get_pvt_id(struct skl_sst *ctx, uuid_le *uuid_mod, int instance_id)
{
	struct uuid_module *module;
	int pvt_id;

	list_for_each_entry(module, &ctx->uuid_list, list) {
		if (uuid_le_cmp(*uuid_mod, module->uuid) == 0) {

			pvt_id = skl_pvtid_128(module);
			if (pvt_id >= 0) {
				module->instance_id[pvt_id] = instance_id;

				return pvt_id;
			}
		}
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(skl_get_pvt_id);

/**
 * skl_put_pvt_id: free up the private id allocated
 *
 * @ctx: driver context
 * @mconfig: module configuration data
 *
 * This frees a 128 bit private unique id previously generated
 */
int skl_put_pvt_id(struct skl_sst *ctx, uuid_le *uuid_mod, int *pvt_id)
{
	int i;
	struct uuid_module *module;

	list_for_each_entry(module, &ctx->uuid_list, list) {
		if (uuid_le_cmp(*uuid_mod, module->uuid) == 0) {

			if (*pvt_id != 0)
				i = (*pvt_id) / 64;
			else
				i = 0;

			module->pvt_id[i] &= ~(1 << (*pvt_id));
			*pvt_id = -1;
			return 0;
		}
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(skl_put_pvt_id);

int skl_dsp_crash_dump_read(struct skl_sst *ctx)
{
	int num_mod = 0, size_core_dump;
	struct uuid_module *module, *module1;
	void *coredump;
	void *fw_reg_addr, *offset;
	struct pci_dev *pci = to_pci_dev(ctx->dsp->dev);
	u16 length0, length1, length2;
	struct adsp_crash_hdr *crash_data_hdr;
	struct adsp_type0_crash_data *type0_data;
	struct adsp_type1_crash_data *type1_data;
	struct adsp_type2_crash_data *type2_data;

	if (list_empty(&ctx->uuid_list))
		dev_info(ctx->dev, "Module list is empty\n");

	list_for_each_entry(module1, &ctx->uuid_list, list) {
		num_mod++;
	}

	/* Length representing in DWORD */
	length0 = sizeof(*type0_data) / sizeof(u32);
	length1 = (num_mod * sizeof(*type1_data)) / sizeof(u32);
	length2 = sizeof(*type2_data) / sizeof(u32);

	/* type1 data size is calculated based on number of modules */
	size_core_dump = (MAX_CRASH_DATA_TYPES * sizeof(*crash_data_hdr)) +
			sizeof(*type0_data) + (num_mod * sizeof(*type1_data)) +
			sizeof(*type2_data);

	coredump = vzalloc(size_core_dump);
	if (!coredump)
		return -ENOMEM;

	offset = coredump;

	/* Fill type0 header and data */
	crash_data_hdr = (struct adsp_crash_hdr *) offset;
	crash_data_hdr->type = TYPE0_EXCEPTION;
	crash_data_hdr->length = length0;
	offset += sizeof(*crash_data_hdr);
	type0_data = (struct adsp_type0_crash_data *) offset;
	type0_data->crash_dump_ver = CRASH_DUMP_VERSION;
	type0_data->bus_dev_id = pci->device;
	offset += sizeof(*type0_data);

	/* Fill type1 header and data */
	crash_data_hdr = (struct adsp_crash_hdr *) offset;
	crash_data_hdr->type = TYPE1_EXCEPTION;
	crash_data_hdr->length = length1;
	offset += sizeof(*crash_data_hdr);
	type1_data = (struct adsp_type1_crash_data *) offset;
	list_for_each_entry(module, &ctx->uuid_list, list) {
		memcpy(type1_data->mod_uuid, &(module->uuid),
					(sizeof(type1_data->mod_uuid)));
		memcpy(type1_data->hash, &(module->hash),
					(sizeof(type1_data->hash)));
		memcpy(&type1_data->mod_id, &(module->id),
					(sizeof(type1_data->mod_id)));
		type1_data++;
	}
	offset += (num_mod * sizeof(*type1_data));

	/* Fill type2 header and data */
	crash_data_hdr = (struct adsp_crash_hdr *) offset;
	crash_data_hdr->type = TYPE2_EXCEPTION;
	crash_data_hdr->length = length2;
	offset += sizeof(*crash_data_hdr);
	type2_data = (struct adsp_type2_crash_data *) offset;
	fw_reg_addr = ctx->dsp->mailbox.in_base - ctx->dsp->addr.w0_stat_sz;
	memcpy_fromio(type2_data->fwreg, fw_reg_addr, sizeof(*type2_data));

	dev_coredumpv(ctx->dsp->dev, coredump,
			size_core_dump, GFP_KERNEL);
	return 0;
}

/*
 * Parse the firmware binary to get the UUID, module id
 * and loadable flags
 */
int snd_skl_parse_uuids(struct sst_dsp *ctx, const struct firmware *fw,
			unsigned int offset, int index)
{
	struct adsp_fw_hdr *adsp_hdr;
	struct adsp_module_entry *mod_entry;
	int i, num_entry, size;
	uuid_le *uuid_bin;
	const char *buf;
	struct skl_sst *skl = ctx->thread_context;
	struct uuid_module *module;
	struct firmware stripped_fw;
	unsigned int safe_file;
	int ret = 0;

	/* Get the FW pointer to derive ADSP header */
	stripped_fw.data = fw->data;
	stripped_fw.size = fw->size;

	skl_dsp_strip_extended_manifest(&stripped_fw);

	buf = stripped_fw.data;

	/* check if we have enough space in file to move to header */
	safe_file = sizeof(*adsp_hdr) + offset;
	if (stripped_fw.size <= safe_file) {
		dev_err(ctx->dev, "Small fw file size, No space for hdr\n");
		return -EINVAL;
	}

	adsp_hdr = (struct adsp_fw_hdr *)(buf + offset);

	/* check 1st module entry is in file */
	safe_file += adsp_hdr->len + sizeof(*mod_entry);
	if (stripped_fw.size <= safe_file) {
		dev_err(ctx->dev, "Small fw file size, No module entry\n");
		return -EINVAL;
	}

	mod_entry = (struct adsp_module_entry *)
		(buf + offset + adsp_hdr->len);

	num_entry = adsp_hdr->num_modules;

	/* check all entries are in file */
	safe_file += num_entry * sizeof(*mod_entry);
	if (stripped_fw.size <= safe_file) {
		dev_err(ctx->dev, "Small fw file size, No modules\n");
		return -EINVAL;
	}


	/*
	 * Read the UUID(GUID) from FW Manifest.
	 *
	 * The 16 byte UUID format is: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXX
	 * Populate the UUID table to store module_id and loadable flags
	 * for the module.
	 */

	for (i = 0; i < num_entry; i++, mod_entry++) {
		module = kzalloc(sizeof(*module), GFP_KERNEL);
		if (!module) {
			ret = -ENOMEM;
			goto free_uuid_list;
		}

		uuid_bin = (uuid_le *)mod_entry->uuid.id;
		memcpy(&module->uuid, uuid_bin, sizeof(module->uuid));

		module->id = (i | (index << 12));
		module->is_loadable = mod_entry->type.load_type;
		module->max_instance = mod_entry->instance_max_count;
		size = sizeof(int) * mod_entry->instance_max_count;
		module->instance_id = devm_kzalloc(ctx->dev, size, GFP_KERNEL);
		if (!module->instance_id) {
			ret = -ENOMEM;
			goto free_uuid_list;
		}
		memcpy(&module->hash, mod_entry->hash1, sizeof(module->hash));

		list_add_tail(&module->list, &skl->uuid_list);

		dev_dbg(ctx->dev,
			"Adding uuid :%pUL   mod id: %d  Loadable: %d\n",
			&module->uuid, module->id, module->is_loadable);
	}

	return 0;

free_uuid_list:
	skl_freeup_uuid_list(skl);
	return ret;
}

static int skl_parse_hw_config_info(struct sst_dsp *ctx, u8 *src, int limit)
{
	struct  skl_tlv_message *message;
	int offset = 0, shift;
	u32 *value;
	struct skl_sst *skl = ctx->thread_context;
	struct skl_hw_property_info *hw_property = &skl->hw_property;
	enum skl_hw_info_type type;

	while (offset < limit) {

		message = (struct skl_tlv_message *)src;
		if (message == NULL)
		break;

		/* Skip TLV header to read value */
		src += sizeof(struct skl_tlv_message);

		value = (u32 *)src;
		type = message->type;

		switch (type) {
		case SKL_CAVS_VERSION:
			hw_property->cavs_version = *value;
			break;

		case SKL_DSP_CORES:
			hw_property->dsp_cores = *value;
			break;

		case SKL_MEM_PAGE_TYPES:
			hw_property->mem_page_bytes = *value;
			break;

		case SKL_TOTAL_PHYS_MEM_PAGES:
			hw_property->total_phys_mem_pages = *value;
			break;

		case SKL_I2S_CAPS:
			memcpy(&hw_property->i2s_caps, value,
				sizeof(hw_property->i2s_caps));
			break;

		case SKL_GPDMA_CAPS:
			memcpy(&hw_property->gpdma_caps, value,
				sizeof(hw_property->gpdma_caps));
			break;

		case SKL_GATEWAY_COUNT:
			hw_property->gateway_count = *value;
			break;

		case SKL_HB_EBB_COUNT:
			hw_property->hb_ebb_count = *value;
			break;

		case SKL_LP_EBB_COUNT:
			hw_property->lp_ebb_count = *value;
			break;

		case SKL_EBB_SIZE_BYTES:
			hw_property->ebb_size_bytes = *value;
			break;

		default:
			dev_err(ctx->dev, "Invalid hw info type:%d \n", type);
			break;
		}

		shift = message->length + sizeof(*message);
		offset += shift;
		/* skip over to next tlv data */
		src += message->length;
	}

	return 0;
}

int skl_get_hardware_configuration(struct sst_dsp *ctx)
{
	struct skl_ipc_large_config_msg msg;
	struct skl_sst *skl = ctx->thread_context;
	u8 *ipc_data;
	int ret = 0;
	size_t rx_bytes;

	ipc_data = kzalloc(DSP_BUF, GFP_KERNEL);
	if (!ipc_data)
		return -ENOMEM;

	msg.module_id = 0;
	msg.instance_id = 0;
	msg.large_param_id = HARDWARE_CONFIG;
	msg.param_data_size = DSP_BUF;

	ret = skl_ipc_get_large_config(&skl->ipc, &msg,
		(u32 *)ipc_data, NULL, 0, &rx_bytes);
	if (ret < 0) {
		dev_err(ctx->dev, "failed to get hw configuration !!!\n");
		goto err;
	}

	ret = skl_parse_hw_config_info(ctx, ipc_data, rx_bytes);
	if (ret < 0)
		dev_err(ctx->dev, "failed to parse configuration !!!\n");

err:
	kfree(ipc_data);
	return ret;
}

void skl_freeup_uuid_list(struct skl_sst *ctx)
{
	struct uuid_module *uuid, *_uuid;

	list_for_each_entry_safe(uuid, _uuid, &ctx->uuid_list, list) {
		list_del(&uuid->list);
		kfree(uuid);
	}
}

/*
 * some firmware binary contains some extended manifest. This needs
 * to be stripped in that case before we load and use that image.
 *
 * Get the module id for the module by checking
 * the table for the UUID for the module
 */
int skl_dsp_strip_extended_manifest(struct firmware *fw)
{
	struct skl_ext_manifest_hdr *hdr;

	/* check if fw file is greater than header we are looking */
	if (fw->size < sizeof(hdr)) {
		pr_err("%s: Firmware file small, no hdr\n", __func__);
		return -EINVAL;
	}

	hdr = (struct skl_ext_manifest_hdr *)fw->data;

	if (hdr->id == SKL_EXT_MANIFEST_HEADER_MAGIC) {
		fw->size -= hdr->len;
		fw->data += hdr->len;
	}

	return 0;
}

int skl_sst_ctx_init(struct device *dev, int irq, const char *fw_name,
	struct skl_dsp_loader_ops dsp_ops, struct skl_sst **dsp,
	struct sst_dsp_device *skl_dev)
{
	struct skl_sst *skl;
	struct sst_dsp *sst;

	skl = devm_kzalloc(dev, sizeof(*skl), GFP_KERNEL);
	if (skl == NULL)
		return -ENOMEM;

	skl->dev = dev;
	skl_dev->thread_context = skl;
	INIT_LIST_HEAD(&skl->uuid_list);
	skl->dsp = skl_dsp_ctx_init(dev, skl_dev, irq);
	if (!skl->dsp) {
		dev_err(skl->dev, "%s: no device\n", __func__);
		return -ENODEV;
	}

	sst = skl->dsp;
	sst->fw_name = fw_name;
	sst->dsp_ops = dsp_ops;
	init_waitqueue_head(&skl->mod_load_wait);
	INIT_LIST_HEAD(&sst->module_list);

	skl->is_first_boot = true;
	if (dsp)
		*dsp = skl;

	return 0;
}

int skl_prepare_lib_load(struct skl_sst *skl, struct skl_lib_info *linfo,
		struct firmware *stripped_fw,
		unsigned int hdr_offset, int index)
{
	int ret;
	struct sst_dsp *dsp = skl->dsp;

	if (linfo->fw == NULL) {
		ret = request_firmware(&linfo->fw, linfo->name,
					skl->dev);
		if (ret < 0) {
			dev_err(skl->dev, "Request lib %s failed:%d\n",
				linfo->name, ret);
			return ret;
		}
	}

	if (skl->is_first_boot) {
		ret = snd_skl_parse_uuids(dsp, linfo->fw, hdr_offset, index);
		if (ret < 0)
			return ret;
	}

	stripped_fw->data = linfo->fw->data;
	stripped_fw->size = linfo->fw->size;
	skl_dsp_strip_extended_manifest(stripped_fw);

	return 0;
}

void skl_release_library(struct skl_lib_info *linfo, int lib_count)
{
	int i;

	/* library indices start from 1 to N. 0 represents base FW */
	for (i = 1; i < lib_count; i++) {
		if (linfo[i].fw) {
			release_firmware(linfo[i].fw);
			linfo[i].fw = NULL;
		}
	}
}
static int skl_fill_sch_cfg(
		struct skl_fw_property_info *fw_property,
		u32 *src)
{
	struct skl_scheduler_config *sch_config =
		&(fw_property->scheduler_config);

	sch_config->sys_tick_multiplier = *src;
	sch_config->sys_tick_divider = *(src + 1);
	sch_config->sys_tick_source = *(src + 2);
	sch_config->sys_tick_cfg_length = *(src + 3);

	if (sch_config->sys_tick_cfg_length > 0) {
		sch_config->sys_tick_cfg =
			kcalloc(sch_config->sys_tick_cfg_length,
					sizeof(*sch_config->sys_tick_cfg),
					GFP_KERNEL);

		if (!sch_config->sys_tick_cfg)
			return -ENOMEM;

		memcpy(sch_config->sys_tick_cfg,
				src + 4,
				sch_config->sys_tick_cfg_length *
				sizeof(*sch_config->sys_tick_cfg));
	}
	return 0;
}

static int skl_fill_dma_cfg(struct skl_tlv_message *message,
		struct skl_fw_property_info *fw_property, u32 *src)
{
	struct skl_dma_buff_config dma_buff_cfg;

	fw_property->num_dma_cfg = message->length /
		sizeof(dma_buff_cfg);

	if (fw_property->num_dma_cfg > 0) {
		fw_property->dma_config =
			kcalloc(fw_property->num_dma_cfg,
					sizeof(dma_buff_cfg),
					GFP_KERNEL);

		if (!fw_property->dma_config)
			return -ENOMEM;

		memcpy(fw_property->dma_config, src,
				message->length);
	}
	return 0;
}

static int skl_parse_fw_config_info(struct sst_dsp *ctx,
		u8 *src, int limit)
{
	struct skl_tlv_message *message;
	int offset = 0, shift, ret = 0;
	u32 *value;
	struct skl_sst *skl = ctx->thread_context;
	struct skl_fw_property_info *fw_property = &skl->fw_property;
	enum skl_fw_info_type type;
	struct skl_scheduler_config *sch_config =
		&fw_property->scheduler_config;

	while (offset < limit) {

		message = (struct skl_tlv_message *)src;
		if (message == NULL)
			break;

		/* Skip TLV header to read value */
		src += sizeof(*message);

		value = (u32 *)src;
		type = message->type;

		switch (type) {
		case SKL_FW_VERSION:
			memcpy(&fw_property->version, value,
					sizeof(fw_property->version));
			break;

		case SKL_MEMORY_RECLAIMED:
			fw_property->memory_reclaimed = *value;
			break;

		case SKL_SLOW_CLOCK_FREQ_HZ:
			fw_property->slow_clock_freq_hz = *value;
			break;

		case SKL_FAST_CLOCK_FREQ_HZ:
			fw_property->fast_clock_freq_hz = *value;
			break;

		case SKL_DMA_BUFFER_CONFIG:
			ret = skl_fill_dma_cfg(message, fw_property, value);
			if (ret < 0)
				goto err;
			break;

		case SKL_ALH_SUPPORT_LEVEL:
			fw_property->alh_support = *value;
			break;

		case SKL_IPC_DL_MAILBOX_BYTES:
			fw_property->ipc_dl_mailbox_bytes = *value;
			break;

		case SKL_IPC_UL_MAILBOX_BYTES:
			fw_property->ipc_ul_mailbox_bytes = *value;
			break;

		case SKL_TRACE_LOG_BYTES:
			fw_property->trace_log_bytes = *value;
			break;

		case SKL_MAX_PPL_COUNT:
			fw_property->max_ppl_count = *value;
			break;

		case SKL_MAX_ASTATE_COUNT:
			fw_property->max_astate_count = *value;
			break;

		case SKL_MAX_MODULE_PIN_COUNT:
			fw_property->max_module_pin_count = *value;
			break;

		case SKL_MODULES_COUNT:
			fw_property->modules_count = *value;
			break;

		case SKL_MAX_MOD_INST_COUNT:
			fw_property->max_mod_inst_count = *value;
			break;

		case SKL_MAX_LL_TASKS_PER_PRI_COUNT:
			fw_property->max_ll_tasks_per_pri_count = *value;
			break;

		case SKL_LL_PRI_COUNT:
			fw_property->ll_pri_count = *value;
			break;

		case SKL_MAX_DP_TASKS_COUNT:
			fw_property->max_dp_tasks_count = *value;
			break;

		case SKL_MAX_LIBS_COUNT:
			fw_property->max_libs_count = *value;
			break;

		case SKL_SCHEDULER_CONFIG:
			ret = skl_fill_sch_cfg(fw_property, value);
			if (ret < 0)
				goto err;
			break;

		case SKL_XTAL_FREQ_HZ:
			fw_property->xtal_freq_hz = *value;
			break;

		case SKL_CLOCKS_CONFIG:
			memcpy(&(fw_property->clk_config), value,
					message->length);
			break;

		default:
			dev_err(ctx->dev, "Invalid fw info type:%d !!\n",
					type);
			break;
		}

		shift = message->length + sizeof(*message);
		offset += shift;
		/* skip over to next tlv data */
		src += message->length;
	}
err:
	if (ret < 0) {
		kfree(fw_property->dma_config);
		kfree(sch_config->sys_tick_cfg);
	}

	return ret;
}

int skl_notify_tplg_change(struct skl_sst *ctx, int type)
{
	struct skl_notify_data *notify_data;

	notify_data = kzalloc(sizeof(*notify_data), GFP_KERNEL);
	if (!notify_data)
		return -ENOMEM;

	notify_data->type = 0xFF;
	notify_data->length = sizeof(struct skl_tcn_events);
	notify_data->tcn_data.type = type;
	do_gettimeofday(&(notify_data->tcn_data.tv));
	ctx->notify_ops.notify_cb(ctx, SKL_TPLG_CHG_NOTIFY, notify_data);
	kfree(notify_data);

	return 0;
}
EXPORT_SYMBOL_GPL(skl_notify_tplg_change);

int skl_get_firmware_configuration(struct sst_dsp *ctx)
{
	struct skl_ipc_large_config_msg msg;
	struct skl_sst *skl = ctx->thread_context;
	u8 *ipc_data;
	int ret = 0;
	size_t rx_bytes;

	ipc_data = kzalloc(DSP_BUF, GFP_KERNEL);
	if (!ipc_data)
		return -ENOMEM;

	msg.module_id = 0;
	msg.instance_id = 0;
	msg.large_param_id = FIRMWARE_CONFIG;
	msg.param_data_size = DSP_BUF;

	ret = skl_ipc_get_large_config(&skl->ipc, &msg,
			(u32 *)ipc_data, NULL, 0, &rx_bytes);
	if (ret < 0) {
		dev_err(ctx->dev, "failed to get fw configuration !!!\n");
		goto err;
	}

	ret = skl_parse_fw_config_info(ctx, ipc_data, rx_bytes);
	if (ret < 0)
		dev_err(ctx->dev, "failed to parse configuration !!!\n");

err:
	kfree(ipc_data);
	return ret;
}

static ssize_t uuid_attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct uuid_attribute *uuid_attr =
		container_of(attr, struct uuid_attribute, attr);
	struct skl_sysfs_module *modinfo_obj =
		container_of(kobj, struct skl_sysfs_module, kobj);

	if (uuid_attr->show)
		return uuid_attr->show(modinfo_obj, uuid_attr, buf);

	return 0;
}

static const struct sysfs_ops uuid_sysfs_ops = {
	.show	= uuid_attr_show,
};

static void uuid_release(struct kobject *kobj)
{
	struct skl_sysfs_module *modinfo_obj =
		container_of(kobj, struct skl_sysfs_module, kobj);

	kfree(modinfo_obj);
}

static struct kobj_type uuid_ktype = {
	.release        = uuid_release,
	.sysfs_ops	= &uuid_sysfs_ops,
};

static ssize_t loaded_show(struct skl_sysfs_module *modinfo_obj,
				struct uuid_attribute *attr, char *buf)
{
	struct skl_module_table *module_list;

	if ((!modinfo_obj->fw_ops_load_mod) ||
		(modinfo_obj->fw_ops_load_mod &&
		!modinfo_obj->uuid_mod->is_loadable))
		return sprintf(buf, "%d\n", true);

	if (list_empty(modinfo_obj->module_list))
		return sprintf(buf, "%d\n", false);

	list_for_each_entry(module_list, modinfo_obj->module_list, list) {
		if (module_list->mod_info->mod_id
					== modinfo_obj->uuid_mod->id)
			return sprintf(buf, "%d\n", module_list->usage_cnt);
	}

	return sprintf(buf, "%d\n", false);
}

static ssize_t hash_show(struct skl_sysfs_module *modinfo_obj,
				struct uuid_attribute *attr, char *buf)
{
	int ret = 0;
	int i;

	for (i = 0; i < DEFAULT_HASH_SHA256_LEN; i++)
		ret += sprintf(buf + ret, "%d ",
					modinfo_obj->uuid_mod->hash[i]);
	ret += sprintf(buf + ret, "\n");

	return ret;
}


static ssize_t id_show(struct skl_sysfs_module *modinfo_obj,
				struct uuid_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", modinfo_obj->uuid_mod->id);
}

static UUID_ATTR_RO(loaded);
static UUID_ATTR_RO(hash);
static UUID_ATTR_RO(id);

static struct attribute *modules_attrs[] = {
	&uuid_attr_loaded.attr,
	&uuid_attr_hash.attr,
	&uuid_attr_id.attr,
	NULL,
};

static const struct attribute_group uuid_group = {
	.attrs = modules_attrs,
};

static void free_uuid_node(struct kobject *kobj,
			     const struct attribute_group *group)
{
	if (kobj) {
		sysfs_remove_group(kobj, group);
		kobject_put(kobj);
	}
}

void skl_module_sysfs_exit(struct skl_sst *ctx)
{
	struct skl_sysfs_tree *tree = ctx->sysfs_tree;
	struct skl_sysfs_module **m;

	if (!tree)
		return;

	if (tree->mod_obj) {
		for (m = tree->mod_obj; *m; m++)
			free_uuid_node(&(*m)->kobj, &uuid_group);
		kfree(tree->mod_obj);
	}

	if (tree->modules_kobj)
		kobject_put(tree->modules_kobj);

	if (tree->dsp_kobj)
		kobject_put(tree->dsp_kobj);

	kfree(tree);
	ctx->sysfs_tree = NULL;
}
EXPORT_SYMBOL_GPL(skl_module_sysfs_exit);

int skl_module_sysfs_init(struct skl_sst *ctx, struct kobject *kobj)
{
	struct uuid_module *module;
	struct skl_sysfs_module *modinfo_obj;
	char *uuid_name;
	int count = 0;
	int max_mod = 0;
	int ret = 0;

	if (list_empty(&ctx->uuid_list))
		return 0;

	ctx->sysfs_tree = kzalloc(sizeof(*ctx->sysfs_tree), GFP_KERNEL);
	if (!ctx->sysfs_tree) {
		ret = -ENOMEM;
		goto err_sysfs_exit;
	}

	ctx->sysfs_tree->dsp_kobj = kobject_create_and_add("dsp", kobj);
	if (!ctx->sysfs_tree->dsp_kobj)
		goto err_sysfs_exit;

	ctx->sysfs_tree->modules_kobj = kobject_create_and_add("modules",
						ctx->sysfs_tree->dsp_kobj);
	if (!ctx->sysfs_tree->modules_kobj)
		goto err_sysfs_exit;

	list_for_each_entry(module, &ctx->uuid_list, list)
		max_mod++;

	ctx->sysfs_tree->mod_obj = kcalloc(max_mod + 1,
			sizeof(*ctx->sysfs_tree->mod_obj), GFP_KERNEL);
	if (!ctx->sysfs_tree->mod_obj) {
		ret = -ENOMEM;
		goto err_sysfs_exit;
	}

	list_for_each_entry(module, &ctx->uuid_list, list) {
		modinfo_obj = kzalloc(sizeof(*modinfo_obj), GFP_KERNEL);
		if (!modinfo_obj) {
			ret = -ENOMEM;
			goto err_sysfs_exit;
		}

		uuid_name = kasprintf(GFP_KERNEL, "%pUL", &module->uuid);
		ret = kobject_init_and_add(&modinfo_obj->kobj, &uuid_ktype,
				ctx->sysfs_tree->modules_kobj, uuid_name);
		if (ret < 0)
			goto err_sysfs_exit;

		ret = sysfs_create_group(&modinfo_obj->kobj, &uuid_group);
		if (ret < 0)
			goto err_sysfs_exit;

		modinfo_obj->uuid_mod = module;
		modinfo_obj->module_list = &ctx->dsp->module_list;
		modinfo_obj->fw_ops_load_mod =
				(ctx->dsp->fw_ops.load_mod == NULL) ? 0 : 1;

		ctx->sysfs_tree->mod_obj[count] = modinfo_obj;
		count++;
	}

	return 0;

err_sysfs_exit:
	 skl_module_sysfs_exit(ctx);

	return ret;
}
EXPORT_SYMBOL_GPL(skl_module_sysfs_init);

int skl_validate_fw_version(struct skl_sst *skl)
{
	struct skl_fw_version *fw_version = &skl->fw_property.version;
	const struct skl_dsp_ops *ops = skl->dsp_ops;

	dev_info(skl->dev, "ADSP FW Version: %d.%d.%d.%d\n",
		 fw_version->major, fw_version->minor,
		 fw_version->hotfix, fw_version->build);


	if (ops->min_fw_ver.major == fw_version->major &&
	    ops->min_fw_ver.minor == fw_version->minor &&
	    ops->min_fw_ver.hotfix == fw_version->hotfix &&
	    ops->min_fw_ver.build <= fw_version->build)
		return 0;

	dev_err(skl->dev, "Incorrect ADSP FW version = %d.%d.%d.%d, minimum supported FW version = %d.%d.%d.%d\n",
		fw_version->major, fw_version->minor,
		fw_version->hotfix, fw_version->build,
		ops->min_fw_ver.major, ops->min_fw_ver.minor,
		ops->min_fw_ver.hotfix, ops->min_fw_ver.build);

	return -EINVAL;
}
