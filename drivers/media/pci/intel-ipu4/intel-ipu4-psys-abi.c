/*
 * Copyright (c) 2016-2017 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>

#include <uapi/linux/intel-ipu4-psys.h>

#include "intel-ipu4-fw-com.h"
#include "intel-ipu4-psys-abi.h"
#include "intel-ipu4-psys.h"

static struct intel_ipu4_psys_abi *ext_abi;

void intel_ipu4_psys_abi_init_ext(struct intel_ipu4_psys_abi *abi)
{
	ext_abi = abi;
}
EXPORT_SYMBOL_GPL(intel_ipu4_psys_abi_init_ext);

void intel_ipu4_psys_abi_cleanup_ext(struct intel_ipu4_psys_abi *abi)
{
	if (ext_abi == abi)
		ext_abi = NULL;
}
EXPORT_SYMBOL_GPL(intel_ipu4_psys_abi_cleanup_ext);

int intel_ipu4_psys_abi_pg_start(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (ext_abi)
		return ext_abi->pg_start(kcmd);
	kcmd->kpg->pg->state = IPU_FW_PSYS_PROCESS_GROUP_STARTED;
	return 0;
}


int intel_ipu4_psys_abi_pg_load_cycles(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (ext_abi)
		return ext_abi->pg_get_load_cycles(kcmd);
	return 0;
}


int intel_ipu4_psys_abi_pg_init_cycles(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (ext_abi)
		return ext_abi->pg_get_init_cycles(kcmd);
	return 0;
}


int intel_ipu4_psys_abi_pg_processing_cycles(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (ext_abi)
		return ext_abi->pg_get_processing_cycles(kcmd);
	return 0;
}

int intel_ipu4_psys_abi_pg_disown(struct intel_ipu4_psys_kcmd *kcmd)
{
	struct ipu_fw_psys_cmd *psys_cmd;
	int ret = 0;

	if (ext_abi)
		return ext_abi->pg_disown(kcmd);

	psys_cmd = intel_ipu4_send_get_token(kcmd->fh->psys->fwcom, 0);
	if (psys_cmd == NULL) {
		dev_err(&kcmd->fh->psys->adev->dev, "failed to get token!\n");
		kcmd->pg_user = NULL;
		ret = -ENODATA;
		goto out;
	}
	psys_cmd->command = IPU_FW_PSYS_PROCESS_GROUP_CMD_START;
	psys_cmd->msg = 0;
	psys_cmd->process_group = kcmd->kpg->pg->ipu_virtual_address;
	intel_ipu4_send_put_token(kcmd->fh->psys->fwcom, 0);

out:
	return ret;
}

int intel_ipu4_psys_abi_pg_abort(struct intel_ipu4_psys_kcmd *kcmd)
{
	struct ipu_fw_psys_cmd *psys_cmd;
	int ret = 0;

	if (ext_abi)
		return ext_abi->pg_abort(kcmd);

	psys_cmd = intel_ipu4_send_get_token(kcmd->fh->psys->fwcom, 0);
	if (psys_cmd == NULL) {
		dev_err(&kcmd->fh->psys->adev->dev, "failed to get token!\n");
		kcmd->pg_user = NULL;
		ret = -ENODATA;
		goto out;
	}
	psys_cmd->command = IPU_FW_PSYS_PROCESS_GROUP_CMD_STOP;
	psys_cmd->msg = 0;
	psys_cmd->process_group = kcmd->kpg->pg->ipu_virtual_address;
	intel_ipu4_send_put_token(kcmd->fh->psys->fwcom, 0);

out:
	return ret;
}

int intel_ipu4_psys_abi_pg_submit(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (ext_abi)
		return ext_abi->pg_submit(kcmd);
	kcmd->kpg->pg->state = IPU_FW_PSYS_PROCESS_GROUP_BLOCKED;
	return 0;
}

struct intel_ipu4_psys_kcmd *intel_ipu4_psys_abi_rcv_kcmd(
	struct intel_ipu4_psys *psys,
	u32 *status)
{
	struct ipu_fw_psys_event *event;
	struct intel_ipu4_psys_kcmd *kcmd;

	if (ext_abi)
		return ext_abi->pg_rcv(psys, status);

	event = intel_ipu4_recv_get_token(psys->fwcom, 0);
	if (!event)
		return NULL;

	*status = event->status;
	kcmd = (struct intel_ipu4_psys_kcmd *)event->token;
	intel_ipu4_recv_put_token(psys->fwcom, 0);
	return kcmd ? kcmd : ERR_PTR(-EIO);
}

int intel_ipu4_psys_abi_terminal_set(struct ipu_fw_psys_terminal *terminal,
				     int terminal_idx,
				     struct intel_ipu4_psys_kcmd *kcmd,
				     u32 buffer,
				     unsigned size)
{
	u32 type;
	u32 buffer_state;

	if (ext_abi)
		return ext_abi->terminal_set(terminal, terminal_idx, kcmd,
					     buffer, size);

	type = terminal->terminal_type;

	switch (type) {
	case IPU_FW_PSYS_TERMINAL_TYPE_PARAM_CACHED_IN:
	case IPU_FW_PSYS_TERMINAL_TYPE_PARAM_CACHED_OUT:
	case IPU_FW_PSYS_TERMINAL_TYPE_PARAM_SPATIAL_IN:
	case IPU_FW_PSYS_TERMINAL_TYPE_PARAM_SPATIAL_OUT:
	case IPU_FW_PSYS_TERMINAL_TYPE_PARAM_SLICED_IN:
	case IPU_FW_PSYS_TERMINAL_TYPE_PARAM_SLICED_OUT:
	case IPU_FW_PSYS_TERMINAL_TYPE_PROGRAM:
		buffer_state = IPU_FW_PSYS_BUFFER_UNDEFINED;
		break;
	case IPU_FW_PSYS_TERMINAL_TYPE_PARAM_STREAM:
	case IPU_FW_PSYS_TERMINAL_TYPE_DATA_IN:
	case IPU_FW_PSYS_TERMINAL_TYPE_STATE_IN:
		buffer_state = IPU_FW_PSYS_BUFFER_FULL;
		break;
	case IPU_FW_PSYS_TERMINAL_TYPE_DATA_OUT:
	case IPU_FW_PSYS_TERMINAL_TYPE_STATE_OUT:
		buffer_state = IPU_FW_PSYS_BUFFER_EMPTY;
		break;
	default:
		dev_err(&kcmd->fh->psys->adev->dev,
			"unknown terminal type: 0x%x\n", type);
		return -EAGAIN;
	}

	if (type == IPU_FW_PSYS_TERMINAL_TYPE_DATA_IN ||
	    type == IPU_FW_PSYS_TERMINAL_TYPE_DATA_OUT) {
		struct ipu_fw_psys_data_terminal *dterminal =
			(struct ipu_fw_psys_data_terminal *)terminal;
		dterminal->connection_type = IPU_FW_PSYS_CONNECTION_MEMORY;
		dterminal->frame.data_bytes = size;
		dterminal->frame.data = buffer;
		dterminal->frame.buffer_state = buffer_state;
	} else {
		struct ipu_fw_psys_param_terminal *pterminal =
			(struct ipu_fw_psys_param_terminal *) terminal;
		pterminal->param_payload.buffer = buffer;
	}
	return 0;
}

void intel_ipu4_psys_abi_pg_dump(struct intel_ipu4_psys *psys,
				 struct intel_ipu4_psys_kcmd *kcmd,
				 const char *note)
{
	struct ipu_fw_psys_process_group *pg = kcmd->kpg->pg;
	u32 pgid = pg->ID;
	uint8_t processes = pg->process_count;
	u16 *process_offset_table = (u16 *)pg + pg->processes_offset;
	unsigned int p;

	if (ext_abi) {
		ext_abi->pg_dump(psys, kcmd, note);
		return;
	}

	dev_dbg(&psys->adev->dev, "%s %s pgid %i processes %i\n",
		__func__, note, pgid, processes);

	for (p = 0; p < processes; p++) {
		struct ipu_fw_psys_process *process =
			(struct ipu_fw_psys_process *)
			((char *)pg + process_offset_table[p]);

		dev_dbg(&psys->adev->dev,
			"%s pgid %i process %i cell %i dev_chn: ext0 %i ext1r %i ext1w %i int %i ipfd %i isa %i\n",
			__func__, pgid, p, process->cell_id,
			process->dev_chn_offset[IPU_FW_PSYS_DEV_CHN_DMA_EXT0_ID],
			process->dev_chn_offset[
				IPU_FW_PSYS_DEV_CHN_DMA_EXT1_READ_ID],
			process->dev_chn_offset[
				IPU_FW_PSYS_DEV_CHN_DMA_EXT1_WRITE_ID],
			process->dev_chn_offset[
				IPU_FW_PSYS_DEV_CHN_DMA_INTERNAL_ID],
			process->dev_chn_offset[
				IPU_FW_PSYS_DEV_CHN_DMA_IPFD_ID],
			process->dev_chn_offset[IPU_FW_PSYS_DEV_CHN_DMA_ISA_ID]);
	}
}

int intel_ipu4_psys_abi_pg_get_id(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (ext_abi)
		return ext_abi->pg_get_id(kcmd);
	return kcmd->kpg->pg->ID;
}

int intel_ipu4_psys_abi_pg_get_terminal_count(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (ext_abi)
		return ext_abi->pg_get_terminal_count(kcmd);
	return kcmd->kpg->pg->terminal_count;
}
int intel_ipu4_psys_abi_pg_get_size(struct intel_ipu4_psys_kcmd *kcmd)
{
	if (ext_abi)
		return ext_abi->pg_get_size(kcmd);
	return kcmd->kpg->pg->size;
}
int intel_ipu4_psys_abi_pg_set_ipu_vaddress(struct intel_ipu4_psys_kcmd *kcmd,
					    dma_addr_t vaddress)
{
	if (ext_abi)
		return ext_abi->pg_set_ipu_vaddress(kcmd, vaddress);
	kcmd->kpg->pg->ipu_virtual_address = vaddress;
	return 0;
}
struct ipu_fw_psys_terminal *intel_ipu4_psys_abi_pg_get_terminal(
	struct intel_ipu4_psys_kcmd *kcmd,
	int index)
{
	struct ipu_fw_psys_terminal *terminal;
	u16 *terminal_offset_table;

	if (ext_abi)
		return ext_abi->pg_get_terminal(kcmd, index);

	terminal_offset_table =
		(uint16_t *) ((char *)kcmd->kpg->pg +
			      kcmd->kpg->pg->terminals_offset);
	terminal = (struct ipu_fw_psys_terminal *)
		((char *)kcmd->kpg->pg + terminal_offset_table[index]);
	return terminal;
}
void intel_ipu4_psys_abi_pg_set_token(struct intel_ipu4_psys_kcmd *kcmd,
				      u64 token)
{
	if (ext_abi)
		return ext_abi->pg_set_token(kcmd, token);
	kcmd->kpg->pg->token = (u64) kcmd;
}

int intel_ipu4_psys_abi_open(struct intel_ipu4_psys *psys)
{
	int retry = INTEL_IPU4_PSYS_OPEN_RETRY, retval;

	if (ext_abi)
		return ext_abi->open(psys);

	retval = intel_ipu4_fw_com_open(psys->fwcom);
	if (retval) {
		dev_err(&psys->adev->dev, "fw com open failed.\n");
		return retval;
	}

	do {
		usleep_range(INTEL_IPU4_PSYS_OPEN_TIMEOUT_US,
			     INTEL_IPU4_PSYS_OPEN_TIMEOUT_US + 10);
		retval = intel_ipu4_fw_com_ready(psys->fwcom);
		if (!retval) {
			dev_dbg(&psys->adev->dev, "psys port open ready!\n");
			break;
		}
	} while (retry-- > 0);

	if (!retry && retval) {
		dev_err(&psys->adev->dev, "psys port open ready failed %d\n",
			retval);
		intel_ipu4_fw_com_close(psys->fwcom);
		return retval;
	}
	return 0;
}

int intel_ipu4_psys_abi_close(struct intel_ipu4_psys *psys)
{
	int retval;

	if (ext_abi)
		return ext_abi->close(psys);

	retval = intel_ipu4_fw_com_close(psys->fwcom);
	if (retval) {
		dev_err(&psys->adev->dev, "fw com close failed.\n");
		return retval;
	}
	return retval;
}
