// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2016 - 2018 Intel Corporation

#include <linux/delay.h>

#include <uapi/linux/ipu-psys.h>

#include "ipu-fw-com.h"
#include "ipu-fw-psys.h"
#include "ipu-psys.h"

int ipu_fw_psys_pg_start(struct ipu_psys_kcmd *kcmd)
{
	kcmd->kpg->pg->state = IPU_FW_PSYS_PROCESS_GROUP_STARTED;
	return 0;
}

int ipu_fw_psys_pg_load_cycles(struct ipu_psys_kcmd *kcmd)
{
	return 0;
}

int ipu_fw_psys_pg_init_cycles(struct ipu_psys_kcmd *kcmd)
{
	return 0;
}

int ipu_fw_psys_pg_processing_cycles(struct ipu_psys_kcmd *kcmd)
{
	return 0;
}

int ipu_fw_psys_pg_disown(struct ipu_psys_kcmd *kcmd)
{
	struct ipu_fw_psys_cmd *psys_cmd;
	int ret = 0;

	psys_cmd = ipu_send_get_token(kcmd->fh->psys->fwcom, 0);
	if (!psys_cmd) {
		dev_err(&kcmd->fh->psys->adev->dev, "failed to get token!\n");
		kcmd->pg_user = NULL;
		ret = -ENODATA;
		goto out;
	}
	psys_cmd->command = IPU_FW_PSYS_PROCESS_GROUP_CMD_START;
	psys_cmd->msg = 0;
	psys_cmd->context_handle = kcmd->kpg->pg->ipu_virtual_address;
	ipu_send_put_token(kcmd->fh->psys->fwcom, 0);

out:
	return ret;
}


int ipu_fw_psys_pg_abort(struct ipu_psys_kcmd *kcmd)
{
	struct ipu_fw_psys_cmd *psys_cmd;
	int ret = 0;

	psys_cmd = ipu_send_get_token(kcmd->fh->psys->fwcom, 0);
	if (!psys_cmd) {
		dev_err(&kcmd->fh->psys->adev->dev, "failed to get token!\n");
		kcmd->pg_user = NULL;
		ret = -ENODATA;
		goto out;
	}
	psys_cmd->command = IPU_FW_PSYS_PROCESS_GROUP_CMD_STOP;
	psys_cmd->msg = 0;
	psys_cmd->context_handle = kcmd->kpg->pg->ipu_virtual_address;
	ipu_send_put_token(kcmd->fh->psys->fwcom, 0);

out:
	return ret;
}

int ipu_fw_psys_pg_submit(struct ipu_psys_kcmd *kcmd)
{
	kcmd->kpg->pg->state = IPU_FW_PSYS_PROCESS_GROUP_BLOCKED;
	return 0;
}

int ipu_fw_psys_rcv_event(struct ipu_psys *psys,
			  struct ipu_fw_psys_event *event)
{
	void *rcv;

	rcv = ipu_recv_get_token(psys->fwcom, 0);
	if (!rcv)
		return 0;

	memcpy(event, rcv, sizeof(*event));
	ipu_recv_put_token(psys->fwcom, 0);
	return 1;
}

int ipu_fw_psys_terminal_set(struct ipu_fw_psys_terminal *terminal,
			     int terminal_idx,
			     struct ipu_psys_kcmd *kcmd,
			     u32 buffer, unsigned int size)
{
	u32 type;
	u32 buffer_state;

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
		if (!ipu_fw_psys_pg_get_protocol(kcmd))
			dterminal->frame.data = buffer;
		else
			dterminal->frame.data_index = terminal_idx;
		dterminal->frame.buffer_state = buffer_state;
	} else {
		struct ipu_fw_psys_param_terminal *pterminal =
		    (struct ipu_fw_psys_param_terminal *)terminal;
		if (!ipu_fw_psys_pg_get_protocol(kcmd))
			pterminal->param_payload.buffer = buffer;
		else
			pterminal->param_payload.terminal_index = terminal_idx;
	}
	return 0;
}

static int process_get_cell(struct ipu_fw_psys_process *process, int index)
{
	int cell;

#if defined(CONFIG_VIDEO_INTEL_IPU4) || defined(CONFIG_VIDEO_INTEL_IPU4P)
	cell = process->cell_id;
#else
	cell = process->cells[index];
#endif
	return cell;
}

static u32 process_get_cells_bitmap(struct ipu_fw_psys_process *process)
{
	unsigned int i;
	int cell_id;
	u32 bitmap = 0;

	for (i = 0; i < IPU_FW_PSYS_PROCESS_MAX_CELLS; i++) {
		cell_id = process_get_cell(process, i);
		if (cell_id != IPU_FW_PSYS_N_CELL_ID)
			bitmap |= (1 << cell_id);
	}
	return bitmap;
}

void ipu_fw_psys_pg_dump(struct ipu_psys *psys,
			 struct ipu_psys_kcmd *kcmd, const char *note)
{
	struct ipu_fw_psys_process_group *pg = kcmd->kpg->pg;
	u32 pgid = pg->ID;
	u8 processes = pg->process_count;
	u16 *process_offset_table = (u16 *)((char *)pg + pg->processes_offset);
	unsigned int p, chn, mem, mem_id;
	int cell;

	dev_dbg(&psys->adev->dev, "%s %s pgid %i has %i processes:\n",
		__func__, note, pgid, processes);

	for (p = 0; p < processes; p++) {
		struct ipu_fw_psys_process *process =
		    (struct ipu_fw_psys_process *)
		    ((char *)pg + process_offset_table[p]);
		cell = process_get_cell(process, 0);
		dev_dbg(&psys->adev->dev, "\t process %i size=%u",
			p, process->size);
		dev_dbg(&psys->adev->dev,
			"\t cell %i cell_bitmap=0x%x kernel_bitmap 0x%llx",
			cell, process_get_cells_bitmap(process),
			(u64) process->kernel_bitmap[1] << 32 |
			(u64) process->kernel_bitmap[0]);
		for (mem = 0; mem < IPU_FW_PSYS_N_DATA_MEM_TYPE_ID; mem++) {
			mem_id = process->ext_mem_id[mem];
			if (mem_id != IPU_FW_PSYS_N_MEM_ID)
				dev_dbg(&psys->adev->dev,
					"\t mem type %u id %d offset=0x%x",
					mem, mem_id,
					process->ext_mem_offset[mem]);
		}
		for (chn = 0; chn < IPU_FW_PSYS_N_DEV_CHN_ID; chn++) {
			if (process->dev_chn_offset[chn] != (u16)(-1))
				dev_dbg(&psys->adev->dev,
					"\t dev_chn[%u]=0x%x\n",
					chn, process->dev_chn_offset[chn]);
		}
	}
}

int ipu_fw_psys_pg_get_id(struct ipu_psys_kcmd *kcmd)
{
	return kcmd->kpg->pg->ID;
}

int ipu_fw_psys_pg_get_terminal_count(struct ipu_psys_kcmd *kcmd)
{
	return kcmd->kpg->pg->terminal_count;
}

int ipu_fw_psys_pg_get_size(struct ipu_psys_kcmd *kcmd)
{
	return kcmd->kpg->pg->size;
}

int ipu_fw_psys_pg_set_ipu_vaddress(struct ipu_psys_kcmd *kcmd,
				    dma_addr_t vaddress)
{
	kcmd->kpg->pg->ipu_virtual_address = vaddress;
	return 0;
}

struct ipu_fw_psys_terminal *ipu_fw_psys_pg_get_terminal(struct ipu_psys_kcmd
							 *kcmd, int index)
{
	struct ipu_fw_psys_terminal *terminal;
	u16 *terminal_offset_table;

	terminal_offset_table =
	    (uint16_t *)((char *)kcmd->kpg->pg +
			  kcmd->kpg->pg->terminals_offset);
	terminal = (struct ipu_fw_psys_terminal *)
	    ((char *)kcmd->kpg->pg + terminal_offset_table[index]);
	return terminal;
}

void ipu_fw_psys_pg_set_token(struct ipu_psys_kcmd *kcmd, u64 token)
{
	kcmd->kpg->pg->token = (u64)token;
}

u64 ipu_fw_psys_pg_get_token(struct ipu_psys_kcmd *kcmd)
{
	return kcmd->kpg->pg->token;
}

int ipu_fw_psys_pg_get_protocol(struct ipu_psys_kcmd *kcmd)
{
	return kcmd->kpg->pg->protocol_version;
}


int ipu_fw_psys_open(struct ipu_psys *psys)
{
	int retry = IPU_PSYS_OPEN_RETRY, retval;

	retval = ipu_fw_com_open(psys->fwcom);
	if (retval) {
		dev_err(&psys->adev->dev, "fw com open failed.\n");
		return retval;
	}

	do {
		usleep_range(IPU_PSYS_OPEN_TIMEOUT_US,
			     IPU_PSYS_OPEN_TIMEOUT_US + 10);
		retval = ipu_fw_com_ready(psys->fwcom);
		if (!retval) {
			dev_dbg(&psys->adev->dev, "psys port open ready!\n");
			break;
		}
	} while (retry-- > 0);

	if (!retry && retval) {
		dev_err(&psys->adev->dev, "psys port open ready failed %d\n",
			retval);
		ipu_fw_com_close(psys->fwcom);
		return retval;
	}
	return 0;
}

int ipu_fw_psys_close(struct ipu_psys *psys)
{
	int retval;

	retval = ipu_fw_com_close(psys->fwcom);
	if (retval) {
		dev_err(&psys->adev->dev, "fw com close failed.\n");
		return retval;
	}
	return retval;
}
