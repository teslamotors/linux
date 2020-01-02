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
#include "skl-topology.h"


#define UUID_STR_SIZE 37
#define DEFAULT_HASH_SHA256_LEN 32
#define TYPE1_HEADER_ROW 7

/* FW Extended Manifest Header id = $AE1 */
#define SKL_EXT_MANIFEST_HEADER_MAGIC   0x31454124
#define MAX_DSP_EXCEPTION_STACK_SIZE (64*1024)

/* FW adds headers and trailing patters to extended crash data */
#define EXTRA_BYTES	256

struct skl_dfw_module_mod {
	char name[100];
	struct skl_dfw_module skl_dfw_mod;
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

struct uuid_module {
	uuid_le uuid;
	int id;
	int is_loadable;

	struct skl_module *mod_data;
	struct list_head list;
	u8 hash1[DEFAULT_HASH_SHA256_LEN];
};

struct skl_ext_manifest_hdr {
	u32 id;
	u32 len;
	u16 version_major;
	u16 version_minor;
	u32 entries;
};

void skl_set_pin_module_id(struct skl_sst *ctx, struct skl_module_cfg *mconfig)
{
	int i;
	struct skl_module_inst_id *pin_id;
	struct uuid_module *module;

	list_for_each_entry(module, &ctx->uuid_list, list) {
		for (i = 0; i < SKL_MAX_IN_QUEUE; i++) {
			pin_id = &mconfig->m_in_pin[i].id;
			if (uuid_le_cmp(pin_id->pin_uuid, module->uuid) == 0)
				pin_id->module_id = module->id;
		}
	}

	list_for_each_entry(module, &ctx->uuid_list, list) {
		for (i = 0; i < SKL_MAX_OUT_QUEUE; i++) {
			pin_id = &mconfig->m_out_pin[i].id;
			if (uuid_le_cmp(pin_id->pin_uuid, module->uuid) == 0)
				pin_id->module_id = module->id;
		}
	}
}

int snd_skl_get_module_info(struct skl_sst *ctx,
			struct skl_module_cfg *mconfig)
{
	struct uuid_module *module;
	uuid_le *uuid_mod;

	uuid_mod = (uuid_le *)mconfig->guid;

	if (list_empty(&ctx->uuid_list)) {
		dev_err(ctx->dev, "Module list is empty\n");
		return -EINVAL;
	}

	list_for_each_entry(module, &ctx->uuid_list, list) {
		if (uuid_le_cmp(*uuid_mod, module->uuid) == 0) {
			mconfig->id.module_id = module->id;
			mconfig->module = module->mod_data;
			mconfig->module->loadable = module->is_loadable;
			skl_set_pin_module_id(ctx, mconfig);

			return 0;
		}
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_skl_get_module_info);

/* This function checks tha available data on the core id
 * passed as an argument and returns the bytes available
 */
int skl_check_ext_excep_data_avail(struct skl_sst *ctx, int idx)
{
	u32 size = ctx->dsp->trace_wind.size/ctx->dsp->trace_wind.nr_dsp;
	u8 *base = (u8 *)ctx->dsp->trace_wind.addr;
	u32 read, write;
	u32 *ptr;

	/* move to the source dsp tracing window */
        base += (idx * size);
        ptr = (u32 *) base;
        read = ptr[0];
        write = ptr[1];

	if (write == read)
		return 0;
        else if (write > read)
		return (write - read);
	else
		return (size - 8 - read + write);
}

/* Function to read the extended DSP crash information from the
 * log buffer memory window, on per core basis.
 * Data is read into the buffer passed as *ext_core_dump.
 * number of bytes read is updated in the sz_ext_dump
 */
void skl_read_ext_exception_data(struct skl_sst *ctx, int idx,
			void *ext_core_dump, int *sz_ext_dump)
{
	u32 size = ctx->dsp->trace_wind.size/ctx->dsp->trace_wind.nr_dsp;
	u8 *base = (u8 *)ctx->dsp->trace_wind.addr;
	u32 read, write;
	int offset = *sz_ext_dump;
	u32 *ptr;

	/* move to the current core's tracing window */
	base += (idx * size);
	ptr = (u32 *) base;
	read = ptr[0];
	write = ptr[1];

	/* in case of read = write, just return */
	if (read == write)
		return;
	if (write > read) {
		memcpy_fromio((ext_core_dump + offset),
				(base + 8 + read),
				(write - read));
		*sz_ext_dump = offset + write - read;
		/* advance read pointer */
		ptr[0] += write - read;
	} else {
		/* wrap around condition - copy till the end */
		memcpy_fromio((ext_core_dump + offset),
				(base + 8 + read),
				(size - 8 - read));
		*sz_ext_dump = offset + size - 8 - read;
		offset = *sz_ext_dump;

		/* copy from the beginnning */
		memcpy_fromio((ext_core_dump + offset), (base + 8), write);
		*sz_ext_dump = offset + write;
		/* update the read pointer */
		ptr[0] = write;
	}
}

void fw_exception_dump_read(struct skl_sst *ctx, int stack_size)
{
	int num_mod = 0, size, size_core_dump, sz_ext_dump = 0, idx = 0;
	struct uuid_module *module, *module1;
	void *coredump, *ext_core_dump;
	void *fw_reg_addr, *offset;
	struct pci_dev *pci = to_pci_dev(ctx->dsp->dev);
	u16 length0, bus_dev_id, length1, length2;
	u32 crash_dump_ver;
	struct type0_crash_data *type0_data;
	struct type1_crash_data *type1_data;
	struct type2_crash_data *type2_data;
	struct adsp_type1_crash_data *type1_mod_info;
	struct sst_dsp *sst = ctx->dsp;

	if (list_empty(&ctx->uuid_list)) {
		dev_err(ctx->dev, "Module list is empty\n");
		return;
	}

	list_for_each_entry(module1, &ctx->uuid_list, list) {
		num_mod++;
	}

	if(stack_size)
		ext_core_dump = vzalloc(stack_size + EXTRA_BYTES);
	else
		ext_core_dump = vzalloc(MAX_DSP_EXCEPTION_STACK_SIZE + EXTRA_BYTES);
        if (!ext_core_dump) {
                dev_err(ctx->dsp->dev, "failed to allocate memory for FW Stack\n");
                return;
        }

	for (idx = 0; idx < sst->trace_wind.nr_dsp; idx++) {
		if (skl_check_ext_excep_data_avail(ctx, idx) != 0) {
			while (sz_ext_dump < stack_size) {
				skl_read_ext_exception_data(ctx, idx,
						ext_core_dump, &sz_ext_dump);
			}
		}
	}

	length0 = (sizeof(struct fw_version)
		+ sizeof(struct sw_version) + sizeof(u32)*2)/sizeof(u32);
	length1 = num_mod * TYPE1_HEADER_ROW;
	length2 = MAX_FW_REG_SZ/sizeof(u32);

	size = (num_mod * sizeof(u32) * TYPE1_HEADER_ROW);
	size_core_dump = sizeof(struct type0_crash_data) +
				sizeof(struct type1_crash_data) +
				sizeof(struct type2_crash_data) +
				size + sz_ext_dump;

	coredump = vzalloc(size_core_dump + sz_ext_dump);
	if (!coredump) {
		dev_err(ctx->dsp->dev, "failed to allocate memory\n");
		vfree(ext_core_dump);
		return;
	}

	offset = coredump;
	bus_dev_id = pci->device;
	crash_dump_ver = 0x1;

	type0_data = (struct type0_crash_data *) offset;
	type0_data->type = TYPE0_EXCEPTION;
	type0_data->length = length0;
	type0_data->crash_dump_ver = crash_dump_ver;
	type0_data->bus_dev_id = bus_dev_id;
	offset += sizeof(struct type0_crash_data);

	type1_data = (struct type1_crash_data *) offset;
	type1_data->type = TYPE1_EXCEPTION;
	type1_data->length = length1;
	type1_mod_info = offset + sizeof(struct type1_crash_data);

	list_for_each_entry(module, &ctx->uuid_list, list) {
		memcpy(type1_mod_info->mod_uuid, &(module->uuid),
					(sizeof(type1_mod_info->mod_uuid)));
		memcpy(type1_mod_info->hash1, &(module->hash1),
					(sizeof(type1_mod_info->hash1)));
		memcpy(&type1_mod_info->mod_id, &(module->id),
					(sizeof(type1_mod_info->mod_id)));
		type1_mod_info ++;
	}

	offset += sizeof(struct type1_crash_data) + size;

	type2_data = (struct type2_crash_data *) offset;
	type2_data->type = TYPE2_EXCEPTION;
	type2_data->length = length2;
	fw_reg_addr = ctx->dsp->mailbox.in_base - ctx->dsp->addr.w0_stat_sz;
	memcpy_fromio(type2_data->fwreg, fw_reg_addr, MAX_FW_REG_SZ);

	if (sz_ext_dump) {
		offset = coredump + size_core_dump;
		memcpy(offset, ext_core_dump, sz_ext_dump);
	}

	vfree(ext_core_dump);

	dev_coredumpv(ctx->dsp->dev, coredump,
			(size_core_dump + sz_ext_dump), GFP_KERNEL);

}
EXPORT_SYMBOL_GPL(fw_exception_dump_read);

/*
 * Parse the firmware binary to get the UUID, module id
 * and loadable flags
 */
int snd_skl_parse_uuids(struct sst_dsp *ctx, const struct firmware *fw,
			unsigned int offset, int index)
{
	struct adsp_fw_hdr *adsp_hdr;
	struct adsp_module_entry *mod_entry;
	int i, j, num_entry, num_modules;
	uuid_le *uuid_bin_fw, *uuid_bin_tplg;
	const char *buf;
	struct skl_sst *skl = ctx->thread_context;
	struct uuid_module *module;
	struct firmware stripped_fw;
	unsigned int safe_file;
	struct skl_module *mod_data;

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

	num_modules = skl->manifest.nr_modules;
	/*
	 * Read the UUID(GUID) from FW Manifest.
	 *
	 * The 16 byte UUID format is: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXX
	 * Populate the UUID table to store module_id and loadable flags
	 * for the module.
	 */

	for (i = 0; i < num_entry; i++, mod_entry++) {
		module = kzalloc(sizeof(*module), GFP_KERNEL);
		if (!module)
			return -ENOMEM;

		uuid_bin_fw = (uuid_le *)mod_entry->uuid.id;
		memcpy(&module->uuid, uuid_bin_fw, sizeof(module->uuid));

		module->id = (i | (index << 12));
		module->is_loadable = mod_entry->type.load_type;
		memcpy(&module->hash1, mod_entry->hash1,
					sizeof(module->hash1));

		/* copy the module data in the parsed module uuid list */
		for (j = 0; j < num_modules; j++) {
			mod_data = &skl->manifest.modules[j];
			uuid_bin_tplg = &mod_data->uuid;
			if (uuid_le_cmp(*uuid_bin_fw, *uuid_bin_tplg) == 0) {
				module->mod_data = mod_data;
				break;
			}
		}

		list_add_tail(&module->list, &skl->uuid_list);

		dev_dbg(ctx->dev,
			"Adding uuid :%pUL   mod id: %d  Loadable: %d\n",
			&module->uuid, module->id, module->is_loadable);
	}

	return 0;
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

int skl_get_firmware_configuration(struct sst_dsp *ctx)
{
	struct skl_ipc_large_config_msg msg;
	struct skl_sst *skl = ctx->thread_context;
	u8 *ipc_data;
	int ret = 0;

	ipc_data = kzalloc(DSP_BUF, GFP_KERNEL);
	if (!ipc_data)
		return -ENOMEM;

	msg.module_id = 0;
	msg.instance_id = 0;
	msg.large_param_id = FIRMWARE_CONFIG;
	msg.param_data_size = DSP_BUF;

	ret = skl_ipc_get_large_config(&skl->ipc, &msg,
			(u32 *)ipc_data, NULL, 0);
	if (ret < 0) {
		dev_err(ctx->dev, "failed to get fw configuration !!!\n");
		goto err;
	}
	
	/* Parse the first 16 bytes, to get the FW Version info */
	ret = skl_parse_fw_config_info(ctx, ipc_data, 16);
	if (ret < 0)
		dev_err(ctx->dev, "failed to parse configuration !!!\n");
err:
	kfree(ipc_data);
	return ret;
}
