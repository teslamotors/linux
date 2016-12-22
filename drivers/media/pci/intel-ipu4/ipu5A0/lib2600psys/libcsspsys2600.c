/*
 * Copyright (c) 2015--2016 Intel Corporation. All Rights Reserved.
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
#include "libcsspsys2600.h"
#include <vied_nci_psys_resource_model.h>
#include <ia_css_psys_device.h>
#include <ipu_device_cell_properties_func.h>

#include <uapi/linux/intel-ipu4-psys.h>

#include "../../intel-ipu4.h"
#include "../../intel-ipu4-psys-abi.h"
#include "../../intel-ipu4-psys.h"
#include "../../intel-ipu4-resources.h"
#include "../../intel-ipu4-wrapper.h"
#include "../../intel-ipu4-mmu.h"

#include <ia_css_psys_process_group_cmd_impl.h>
#include <ia_css_psys_init.h>
#include <ia_css_psys_transport.h>
#include <ia_css_terminal_base_types.h>
#include <ia_css_terminal_types.h>
#include <ia_css_program_group_param_types.h>
#include <ia_css_program_group_data.h>

#define BASIC_ABI_CHECK

#ifndef BASIC_ABI_CHECK
#define ABI_CHECK(a, b, field)						\
	{								\
	if (offsetof(typeof(*a), field) != offsetof(typeof(*b), field))	\
		pr_err("intel_ipu4 psys ABI mismatch %s\n",		\
		       __stringify(field));				\
	}
#else
#define ABI_CHECK(a, b, field) { }
#endif

#define SIZE_OF_CHECK(a, b)						\
	{								\
		if (sizeof(*a) != sizeof(*b))				\
		pr_err("intel_ipu4 psys ABI size of mismatch %s\n",	\
		       __stringify(a));					\
	}

#define SIZE_CHECK(a, b)						\
	{								\
		if ((a) != (b))				\
		pr_err("intel_ipu4 psys ABI size mismatch %s\n",	\
		       __stringify(a));					\
	}

static void abi_sanity_checker(void)
{
	struct ipu_fw_psys_process_group *ipu_fw_psys_pg;
	struct ia_css_process_group_s *ia_css_pg;

	struct ipu_fw_psys_srv_init *ipu_fw_psys_init;
	struct ia_css_psys_server_init *ia_css_psys_init;

	struct ipu_fw_psys_cmd *ipu_fw_psys_cmd;
	struct ia_css_psys_cmd_s *ia_css_psys_cmd;

	struct ipu_fw_psys_event *ipu_fw_psys_event;
	struct ia_css_psys_event_s *ia_css_psys_event;

	struct ipu_fw_psys_terminal *ipu_fw_psys_terminal;
	struct ia_css_terminal_s *ia_css_terminal;

	struct ipu_fw_psys_param_terminal *ipu_fw_psys_param_terminal;
	struct ia_css_param_terminal_s *ia_css_param_terminal;

	struct ipu_fw_psys_param_payload *ipu_fw_psys_param_payload;
	struct ia_css_param_payload_s *ia_css_param_payload;

	struct ipu_fw_psys_frame *ipu_fw_psys_frame;
	struct ia_css_frame_s *ia_css_frame;

	struct ipu_fw_psys_frame_descriptor *ipu_fw_psys_frame_descriptor;
	struct ia_css_frame_descriptor_s *ia_css_frame_descriptor;

	struct ipu_fw_psys_stream *ipu_fw_psys_stream;
	struct ia_css_stream_s *ia_css_stream;

	SIZE_OF_CHECK(ipu_fw_psys_pg, ia_css_pg);
	ABI_CHECK(ipu_fw_psys_pg, ia_css_pg, ID);
	ABI_CHECK(ipu_fw_psys_pg, ia_css_pg, process_count);
	ABI_CHECK(ipu_fw_psys_pg, ia_css_pg, processes_offset);

	SIZE_OF_CHECK(ipu_fw_psys_init, ia_css_psys_init);
	ABI_CHECK(ipu_fw_psys_init, ia_css_psys_init, icache_prefetch_sp);
	ABI_CHECK(ipu_fw_psys_init, ia_css_psys_init, icache_prefetch_isp);

	SIZE_OF_CHECK(ipu_fw_psys_cmd, ia_css_psys_cmd);
	ABI_CHECK(ipu_fw_psys_cmd, ia_css_psys_cmd, command);
	ABI_CHECK(ipu_fw_psys_cmd, ia_css_psys_cmd, msg);
	ABI_CHECK(ipu_fw_psys_cmd, ia_css_psys_cmd, process_group);

	SIZE_OF_CHECK(ipu_fw_psys_event, ia_css_psys_event);
	ABI_CHECK(ipu_fw_psys_event, ia_css_psys_event, status);
	ABI_CHECK(ipu_fw_psys_event, ia_css_psys_event, token);

	SIZE_OF_CHECK(ipu_fw_psys_terminal, ia_css_terminal);
	ABI_CHECK(ipu_fw_psys_terminal, ia_css_terminal, terminal_type);

	SIZE_OF_CHECK(ipu_fw_psys_param_terminal, ia_css_param_terminal);
	ABI_CHECK(ipu_fw_psys_param_terminal, ia_css_param_terminal,
		  param_payload);

	SIZE_OF_CHECK(ipu_fw_psys_param_payload, ia_css_param_payload);
	ABI_CHECK(ipu_fw_psys_param_payload, ia_css_param_payload, buffer);

	SIZE_OF_CHECK(ipu_fw_psys_frame, ia_css_frame);
	ABI_CHECK(ipu_fw_psys_frame, ia_css_frame, data_bytes);
	ABI_CHECK(ipu_fw_psys_frame, ia_css_frame, data);
	ABI_CHECK(ipu_fw_psys_frame, ia_css_frame, buffer_state);

	SIZE_OF_CHECK(ipu_fw_psys_frame_descriptor, ia_css_frame_descriptor);
	SIZE_OF_CHECK(ipu_fw_psys_stream, ia_css_stream);
	SIZE_CHECK(IPU_FW_PSYS_N_CELL_ID, VIED_NCI_N_CELL_ID);
}

/*Implement SMIF/Vied subsystem access here */

unsigned long long _hrt_master_port_subsystem_base_address;

/* end of platform stubs */

static int libcsspsys2600_pg_start(struct intel_ipu4_psys_kcmd *kcmd)
{
	return -ia_css_process_group_start((ia_css_process_group_t *)
					   kcmd->kpg->pg);
}

static int libcsspsys2600_pg_disown(struct intel_ipu4_psys_kcmd *kcmd)
{
	return -ia_css_process_group_disown((ia_css_process_group_t *)
					    kcmd->kpg->pg);
}

static int libcsspsys2600_pg_abort(struct intel_ipu4_psys_kcmd *kcmd)
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

static int libcsspsys2600_pg_submit(struct intel_ipu4_psys_kcmd *kcmd)
{
	return -ia_css_process_group_submit((ia_css_process_group_t *)
					    kcmd->kpg->pg);
}

static void *syscom_buffer;
static struct ia_css_syscom_config *syscom_config;
static struct ia_css_psys_server_init *server_init;

static struct intel_ipu4_psys_kcmd *libcsspsys2600_pg_rcv(
	struct intel_ipu4_psys *psys,
	u32 *status)
{
	struct ia_css_psys_event_s event;
	struct intel_ipu4_psys_kcmd *kcmd;

	if (!ia_css_psys_event_queue_receive(psys_syscom,
					     IA_CSS_PSYS_EVENT_QUEUE_MAIN_ID,
					     &event))
		return NULL;

	dev_dbg(&psys->adev->dev, "psys received event status:%d\n",
		event.status);

	kcmd = (struct intel_ipu4_psys_kcmd *)event.token;
	*status = event.status;
	return kcmd ? kcmd : ERR_PTR(-EIO);
}

static int libcsspsys2600_terminal_set(struct ipu_fw_psys_terminal *terminal,
				       int terminal_idx,
				       struct intel_ipu4_psys_kcmd *kcmd,
				       vied_vaddress_t buffer,
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

static void libcsspsys2600_pg_dump(struct intel_ipu4_psys *psys,
				   struct intel_ipu4_psys_kcmd *kcmd,
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

int libcsspsys2600_pg_get_id(struct intel_ipu4_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_program_group_ID(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}

int libcsspsys2600_pg_get_terminal_count(struct intel_ipu4_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_terminal_count(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}

int libcsspsys2600_pg_get_size(struct intel_ipu4_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_size((ia_css_process_group_t *)
					     kcmd->kpg->pg);
}

int libcsspsys2600_pg_set_ipu_vaddress(struct intel_ipu4_psys_kcmd *kcmd,
				       dma_addr_t vaddress)
{
	return ia_css_process_group_set_ipu_vaddress((ia_css_process_group_t *)
						     kcmd->kpg->pg, vaddress);
}

int libcsspsys2600_pg_get_load_cycles(struct intel_ipu4_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_pg_load_cycles(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}

int libcsspsys2600_pg_get_init_cycles(struct intel_ipu4_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_pg_init_cycles(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}

int libcsspsys2600_pg_get_processing_cycles(struct intel_ipu4_psys_kcmd *kcmd)
{
	return ia_css_process_group_get_pg_processing_cycles(
		(ia_css_process_group_t *)kcmd->kpg->pg);
}

void *libcsspsys2600_pg_get_terminal(struct intel_ipu4_psys_kcmd *kcmd,
							 int index)
{
	return ia_css_process_group_get_terminal((ia_css_process_group_t *)
						 kcmd->kpg->pg, index);
}

void libcsspsys2600_pg_set_token(struct intel_ipu4_psys_kcmd *kcmd, u64 token)
{
	ia_css_process_group_set_token((ia_css_process_group_t *)kcmd->kpg->pg,
				       token);
}

static int libcsspsys2600_open(struct intel_ipu4_psys *psys)
{
	bool opened;
	int retry = INTEL_IPU4_PSYS_OPEN_RETRY;

	intel_ipu4_wrapper_init(PSYS_MMID, &psys->adev->dev,
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
		usleep_range(INTEL_IPU4_PSYS_OPEN_TIMEOUT_US,
			     INTEL_IPU4_PSYS_OPEN_TIMEOUT_US + 10);
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

static int libcsspsys2600_close(struct intel_ipu4_psys *psys)
{
	int rval;
	unsigned int retry = INTEL_IPU4_PSYS_CLOSE_TIMEOUT;

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
		usleep_range(INTEL_IPU4_PSYS_CLOSE_TIMEOUT_US,
			     INTEL_IPU4_PSYS_CLOSE_TIMEOUT_US + 10);
	} while (rval && --retry);

	psys_syscom = NULL;

	return 0;
}

struct intel_ipu4_psys_abi abi = {
	.pg_start = libcsspsys2600_pg_start,
	.pg_disown = libcsspsys2600_pg_disown,
	.pg_submit = libcsspsys2600_pg_submit,
	.pg_abort = libcsspsys2600_pg_abort,
	.pg_rcv = libcsspsys2600_pg_rcv,
	.terminal_set = libcsspsys2600_terminal_set,
	.pg_dump = libcsspsys2600_pg_dump,
	.pg_get_id = libcsspsys2600_pg_get_id,
	.pg_get_terminal_count =
	libcsspsys2600_pg_get_terminal_count,
	.pg_get_size = libcsspsys2600_pg_get_size,
	.pg_set_ipu_vaddress = libcsspsys2600_pg_set_ipu_vaddress,
	.pg_get_terminal = libcsspsys2600_pg_get_terminal,
	.pg_set_token = libcsspsys2600_pg_set_token,
	.pg_get_load_cycles = libcsspsys2600_pg_get_load_cycles,
	.pg_get_init_cycles = libcsspsys2600_pg_get_init_cycles,
	.pg_get_processing_cycles = libcsspsys2600_pg_get_processing_cycles,
	.open = libcsspsys2600_open,
	.close = libcsspsys2600_close,
};

struct intel_ipu4_psys_abi *ext_abi = &abi;
EXPORT_SYMBOL(ext_abi);

static int __init libcsspsys2600_init(void)
{
	int rval;

	syscom_buffer = kzalloc(ia_css_sizeof_psys(NULL), GFP_KERNEL);
	if (!syscom_buffer)
		return -ENOMEM;

	syscom_config = kzalloc(
		sizeof(struct ia_css_syscom_config), GFP_KERNEL);
	if (!syscom_config) {
		rval = -ENOMEM;
		goto out_syscom_buffer_free;
	}

	server_init = kzalloc(
		sizeof(struct ia_css_psys_server_init), GFP_KERNEL);
	if (!server_init) {
		rval = -ENOMEM;
		goto out_syscom_config_free;
	}

	server_init->ddr_pkg_dir_address = 0;
	server_init->host_ddr_pkg_dir = 0;
	server_init->pkg_dir_size = 0;

	*syscom_config = *ia_css_psys_specify();
	syscom_config->specific_addr = server_init;
	syscom_config->specific_size =
		sizeof(struct ia_css_psys_server_init);
	syscom_config->ssid = PSYS_SSID;
	syscom_config->mmid = PSYS_MMID;
	syscom_config->regs_addr =
		ipu_device_cell_memory_address(
			SPC0, IPU_DEVICE_SP2600_CONTROL_REGS);
	syscom_config->dmem_addr =
		ipu_device_cell_memory_address(
			SPC0, IPU_DEVICE_SP2600_CONTROL_DMEM);
	intel_ipu4_psys_abi_init_ext(&abi);

	abi_sanity_checker();

	return 0;

out_syscom_config_free:
	kfree(syscom_config);
out_syscom_buffer_free:
	kfree(syscom_buffer);

	return rval;
}

static void __exit libcsspsys2600_exit(void)
{
	intel_ipu4_psys_abi_cleanup_ext(&abi);
	kfree(syscom_buffer);
	kfree(syscom_config);
	kfree(server_init);
}

module_init(libcsspsys2600_init);
module_exit(libcsspsys2600_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel intel_ipu4 psys library");
