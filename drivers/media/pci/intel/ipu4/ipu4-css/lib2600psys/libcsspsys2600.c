/*
 * Copyright (c) 2015--2018 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/module.h>

#include <uapi/linux/ipu-psys.h>

#include "ipu.h"
#include "ipu-mmu.h"
#include "ipu-psys.h"
#include "ipu-fw-psys.h"
#include "ipu-wrapper.h"
#include "libcsspsys2600.h"

#include <vied_nci_psys_resource_model.h>
#include <ia_css_psys_device.h>
#include <ipu_device_cell_properties_func.h>
#include <ia_css_psys_program_group_private.h>
#include <ia_css_psys_process_private_types.h>

int ipu_fw_psys_pg_start(struct ipu_psys_kcmd *kcmd)
{
	return -ia_css_process_group_start((ia_css_process_group_t *)
					   kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_start);

int ipu_fw_psys_pg_disown(struct ipu_psys_kcmd *kcmd)
{
	return -ia_css_process_group_disown((ia_css_process_group_t *)
					    kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_disown);

int ipu_fw_psys_pg_abort(struct ipu_psys_kcmd *kcmd)
{
	int rval;

	rval = ia_css_process_group_stop((ia_css_process_group_t *)
					 kcmd->kpg->pg);
	if (rval) {
		dev_err(&kcmd->fh->psys->adev->dev,
			"failed to abort kcmd!\n");
		kcmd->pg_user = NULL;
		rval = -EIO;
		/* TODO: need to reset PSYS by power cycling it */
	}
	return rval;
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_abort);

int ipu_fw_psys_pg_submit(struct ipu_psys_kcmd *kcmd)
{
	return -ia_css_process_group_submit((ia_css_process_group_t *)
					    kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_submit);

static void *syscom_buffer;
static struct ia_css_syscom_config *syscom_config;
static struct ia_css_psys_server_init *server_init;

int ipu_fw_psys_rcv_event(struct ipu_psys *psys,
				struct ipu_fw_psys_event *event)
{
	return ia_css_psys_event_queue_receive(psys_syscom,
		IA_CSS_PSYS_EVENT_QUEUE_MAIN_ID,
		(struct ia_css_psys_event_s *)event);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_rcv_event);

int ipu_fw_psys_terminal_set(struct ipu_fw_psys_terminal *terminal,
				       int terminal_idx,
				       struct ipu_psys_kcmd *kcmd,
				       u32 buffer,
				       unsigned	size)
{
	ia_css_terminal_type_t type;
	u32 buffer_state;

	type = ia_css_terminal_get_type((ia_css_terminal_t *)terminal);

	switch (type) {
	case IA_CSS_TERMINAL_TYPE_PARAM_CACHED_IN:
	case IA_CSS_TERMINAL_TYPE_PARAM_CACHED_OUT:
	case IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_IN:
	case IA_CSS_TERMINAL_TYPE_PARAM_SPATIAL_OUT:
	case IA_CSS_TERMINAL_TYPE_PARAM_SLICED_IN:
	case IA_CSS_TERMINAL_TYPE_PARAM_SLICED_OUT:
	case IA_CSS_TERMINAL_TYPE_PROGRAM:
		buffer_state = IA_CSS_BUFFER_UNDEFINED;
		break;
	case IA_CSS_TERMINAL_TYPE_PARAM_STREAM:
	case IA_CSS_TERMINAL_TYPE_DATA_IN:
	case IA_CSS_TERMINAL_TYPE_STATE_IN:
		buffer_state = IA_CSS_BUFFER_FULL;
		break;
	case IA_CSS_TERMINAL_TYPE_DATA_OUT:
	case IA_CSS_TERMINAL_TYPE_STATE_OUT:
		buffer_state = IA_CSS_BUFFER_EMPTY;
		break;
	default:
		dev_err(&kcmd->fh->psys->adev->dev,
			"unknown terminal type: 0x%x\n", type);
		return -EAGAIN;
	}

	if (type == IA_CSS_TERMINAL_TYPE_DATA_IN ||
	    type == IA_CSS_TERMINAL_TYPE_DATA_OUT) {
		ia_css_frame_t *frame;

		if (ia_css_data_terminal_set_connection_type(
			    (ia_css_data_terminal_t *)terminal,
			    IA_CSS_CONNECTION_MEMORY))
			return -EIO;
		frame = ia_css_data_terminal_get_frame(
			(ia_css_data_terminal_t *)terminal);
		if (!frame)
			return -EIO;

		if (ia_css_frame_set_data_bytes(frame, size))
			return -EIO;
	}

	return -ia_css_process_group_attach_buffer(
		(ia_css_process_group_t *)kcmd->kpg->pg, buffer,
		buffer_state, terminal_idx);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_terminal_set);

void ipu_fw_psys_pg_dump(struct ipu_psys *psys,
				   struct ipu_psys_kcmd *kcmd,
				   const char *note)
{
	ia_css_process_group_t *pg = (ia_css_process_group_t *)kcmd->kpg->pg;
	ia_css_program_group_ID_t pgid =
		ia_css_process_group_get_program_group_ID(pg);
	uint8_t processes = ia_css_process_group_get_process_count(
		(ia_css_process_group_t *)kcmd->kpg->pg);
	unsigned int p;

	dev_dbg(&psys->adev->dev, "%s %s pgid %i processes %i\n",
		__func__, note, pgid, processes);
	for (p = 0; p < processes; p++) {
		ia_css_process_t *process =
			ia_css_process_group_get_process(pg, p);

		dev_dbg(&psys->adev->dev,
			"%s pgid %i process %i cell %i dev_chn: ext0 %i ext1r %i ext1w %i int %i ipfd %i isa %i\n",
			__func__, pgid, p,
			ia_css_process_get_cell(process),
			ia_css_process_get_dev_chn(process,
					   VIED_NCI_DEV_CHN_DMA_EXT0_ID),
			ia_css_process_get_dev_chn(process,
					   VIED_NCI_DEV_CHN_DMA_EXT1_READ_ID),
			ia_css_process_get_dev_chn(process,
					   VIED_NCI_DEV_CHN_DMA_EXT1_WRITE_ID),
			ia_css_process_get_dev_chn(process,
					VIED_NCI_DEV_CHN_DMA_INTERNAL_ID),
			ia_css_process_get_dev_chn(process,
					VIED_NCI_DEV_CHN_DMA_IPFD_ID),
			ia_css_process_get_dev_chn(process,
					VIED_NCI_DEV_CHN_DMA_ISA_ID));
	}
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_dump);

int ipu_fw_psys_pg_get_id(struct ipu_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_program_group_ID(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_get_id);

int ipu_fw_psys_pg_get_terminal_count(struct ipu_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_terminal_count(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_get_terminal_count);

int ipu_fw_psys_pg_get_size(struct ipu_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_size((ia_css_process_group_t *)
					     kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_get_size);

int ipu_fw_psys_pg_set_ipu_vaddress(struct ipu_psys_kcmd *kcmd,
				       dma_addr_t vaddress)
{
	return ia_css_process_group_set_ipu_vaddress((ia_css_process_group_t *)
						     kcmd->kpg->pg, vaddress);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_set_ipu_vaddress);

int ipu_fw_psys_pg_load_cycles(struct ipu_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_pg_load_cycles(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_load_cycles);

int ipu_fw_psys_pg_init_cycles(struct ipu_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_pg_init_cycles(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_init_cycles);

int ipu_fw_psys_pg_processing_cycles(struct ipu_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_pg_processing_cycles(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_processing_cycles);

struct ipu_fw_psys_terminal *
ipu_fw_psys_pg_get_terminal(struct ipu_psys_kcmd *kcmd, int index)
{
	return (struct ipu_fw_psys_terminal *)ia_css_process_group_get_terminal(
			(ia_css_process_group_t *)kcmd->kpg->pg, index);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_get_terminal);

void ipu_fw_psys_pg_set_token(struct ipu_psys_kcmd *kcmd, u64 token)
{
	ia_css_process_group_set_token((ia_css_process_group_t *)kcmd->kpg->pg,
				       token);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_set_token);

int ipu_fw_psys_pg_get_protocol(
	struct ipu_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_protocol_version(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_get_protocol);

int ipu_fw_psys_open(struct ipu_psys *psys)
{
	bool opened;
	int retry = IPU_PSYS_OPEN_RETRY;

	ipu_wrapper_init(PSYS_MMID, &psys->adev->dev,
				psys->pdata->base);

	server_init->icache_prefetch_sp = psys->icache_prefetch_sp;
	server_init->icache_prefetch_isp = psys->icache_prefetch_isp;

	psys_syscom = ia_css_psys_open(syscom_buffer, syscom_config);
	if (!psys_syscom) {
		dev_err(&psys->adev->dev,
			"psys library open failed\n");
		return -ENODEV;
	}
	do {
		opened = ia_css_psys_open_is_ready(psys_syscom);
		if (opened)
			break;
		usleep_range(IPU_PSYS_OPEN_TIMEOUT_US,
			     IPU_PSYS_OPEN_TIMEOUT_US + 10);
		retry--;
	} while (retry > 0);

	if (!retry && !opened) {
		dev_err(&psys->adev->dev,
			"psys library open ready failed\n");
		ia_css_psys_close(psys_syscom);
		ia_css_psys_release(psys_syscom, 1);
		psys_syscom = NULL;
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_open);

int ipu_fw_psys_close(struct ipu_psys *psys)
{
	int rval;
	unsigned int retry = IPU_PSYS_CLOSE_TIMEOUT;

	if (!psys_syscom)
		return 0;

	if (ia_css_psys_close(psys_syscom)) {
		dev_err(&psys->adev->dev,
			"psys library close ready failed\n");
		return 0;
	}

	do {
		rval = ia_css_psys_release(psys_syscom, 0);
		if (rval && rval != -EBUSY) {
			dev_dbg(&psys->adev->dev, "psys library release failed\n");
			break;
		}
		usleep_range(IPU_PSYS_CLOSE_TIMEOUT_US,
			     IPU_PSYS_CLOSE_TIMEOUT_US + 10);
	} while (rval && --retry);

	psys_syscom = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_close);

u64 ipu_fw_psys_pg_get_token(struct ipu_psys_kcmd *kcmd)
{
	return 0;
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_pg_get_token);

static const struct ipu_fw_resource_definitions default_defs = {
	.cells = vied_nci_cell_type,
	.num_cells = VIED_NCI_N_CELL_ID,
	.num_cells_type = VIED_NCI_N_CELL_TYPE_ID,
	.dev_channels = vied_nci_dev_chn_size,
	.num_dev_channels = VIED_NCI_N_DEV_CHN_ID,

	.num_ext_mem_types = VIED_NCI_N_DATA_MEM_TYPE_ID,
	.num_ext_mem_ids = VIED_NCI_N_MEM_ID,
	.ext_mem_ids = vied_nci_mem_size,

	.cell_mem_row = VIED_NCI_N_MEM_TYPE_ID,
	.cell_mem = (enum ipu_mem_id *)vied_nci_cell_mem,
};

const struct ipu_fw_resource_definitions *res_defs = &default_defs;
EXPORT_SYMBOL_GPL(res_defs);

int ipu_fw_psys_set_process_cell_id(struct ipu_fw_psys_process *ptr, u8 index,
				    u8 value)
{
	return ia_css_process_set_cell((ia_css_process_t *)ptr,
				       (vied_nci_cell_ID_t)value);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_set_process_cell_id);

u8 ipu_fw_psys_get_process_cell_id(struct ipu_fw_psys_process *ptr, u8 index)
{
	return ia_css_process_get_cell((ia_css_process_t *)ptr);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_get_process_cell_id);

int ipu_fw_psys_clear_process_cell(struct ipu_fw_psys_process *ptr)
{
	return ia_css_process_clear_cell((ia_css_process_t *)ptr);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_clear_process_cell);

int ipu_fw_psys_set_process_dev_chn_offset(struct ipu_fw_psys_process *ptr,
					   u16 offset, u16 value)
{
	return ia_css_process_set_dev_chn((ia_css_process_t *)ptr,
					  (vied_nci_dev_chn_ID_t)offset,
					  (vied_nci_resource_size_t)value);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_set_process_dev_chn_offset);

int ipu_fw_psys_set_process_ext_mem(struct ipu_fw_psys_process *ptr,
				    u16 type_id, u16 mem_id, u16 offset)
{
	return ia_css_process_set_ext_mem((ia_css_process_t *)ptr, mem_id, offset);
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_set_process_ext_mem);

int ipu_fw_psys_get_program_manifest_by_process(
	struct ipu_fw_generic_program_manifest *gen_pm,
	const struct ipu_fw_psys_program_group_manifest *pg_manifest,
	struct ipu_fw_psys_process *process)
{
	ia_css_program_ID_t process_id =
		ia_css_process_get_program_ID(
			(const ia_css_process_t *)process);
	int programs =
		ia_css_program_group_manifest_get_program_count(
			(const ia_css_program_group_manifest_t *)pg_manifest);
	int i;

	for (i = 0; i < programs; i++) {
		ia_css_program_ID_t program_id;
		ia_css_program_manifest_t *pm =
			ia_css_program_group_manifest_get_prgrm_mnfst(
				(const ia_css_program_group_manifest_t *)
				pg_manifest, i);
		if (!pm)
			continue;
		program_id = ia_css_program_manifest_get_program_ID(pm);
		if (program_id == process_id) {
			gen_pm->dev_chn_size = (u16 *)pm->dev_chn_size;
			gen_pm->ext_mem_size = (u16 *)pm->ext_mem_size;
			gen_pm->cell_id = pm->cell_id;
			gen_pm->cell_type_id = pm->cell_type_id;
			return 0;
		}
	}
	return -ENOENT;
}
EXPORT_SYMBOL_GPL(ipu_fw_psys_get_program_manifest_by_process);

static int __init libcsspsys2600_init(void)
{
	int rval;

	syscom_buffer = kzalloc(ia_css_sizeof_psys(NULL), GFP_KERNEL);
	if (!syscom_buffer)
		return -ENOMEM;

	syscom_config = kzalloc(sizeof(struct ia_css_syscom_config),
				GFP_KERNEL);
	if (!syscom_config) {
		rval = -ENOMEM;
		goto out_syscom_buffer_free;
	}

	server_init = kzalloc(sizeof(struct ia_css_psys_server_init),
			      GFP_KERNEL);
	if (!server_init) {
		rval = -ENOMEM;
		goto out_syscom_config_free;
	}

	server_init->ddr_pkg_dir_address = 0;
	server_init->host_ddr_pkg_dir = 0;
	server_init->pkg_dir_size = 0;

	*syscom_config = *ia_css_psys_specify();
	syscom_config->specific_addr = server_init;
	syscom_config->specific_size = sizeof(struct ia_css_psys_server_init);
	syscom_config->ssid = PSYS_SSID;
	syscom_config->mmid = PSYS_MMID;
	syscom_config->regs_addr = ipu_device_cell_memory_address(SPC0,
					IPU_DEVICE_SP2600_CONTROL_REGS);
	syscom_config->dmem_addr = ipu_device_cell_memory_address(SPC0,
					IPU_DEVICE_SP2600_CONTROL_DMEM);

	return 0;

out_syscom_config_free:
	kfree(syscom_config);
out_syscom_buffer_free:
	kfree(syscom_buffer);

	return rval;
}

static void __exit libcsspsys2600_exit(void)
{
	kfree(syscom_buffer);
	kfree(syscom_config);
	kfree(server_init);
}

module_init(libcsspsys2600_init);
module_exit(libcsspsys2600_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu psys css library");
