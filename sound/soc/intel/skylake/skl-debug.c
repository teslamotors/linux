/*
 *  skl-debug.c - Debugfs for skl driver
 *
 *  Copyright (C) 2016-17 Intel Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#include <linux/pci.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include "skl.h"
#include "skl-sst-dsp.h"
#include "skl-sst-ipc.h"
#include "skl-tplg-interface.h"
#include "skl-topology.h"
#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "skl-nhlt.h"

#define MOD_BUF (2 * PAGE_SIZE)
#define FW_REG_BUF	PAGE_SIZE
#define FW_REG_SIZE	0x60
#define MAX_SSP 	6
#define MAX_SZ 1025
#define IPC_MOD_LARGE_CONFIG_GET 3
#define IPC_MOD_LARGE_CONFIG_SET 4
#define MOD_BUF1 (3 * PAGE_SIZE)
#define MAX_TLV_PAYLOAD_SIZE	4088
#define EXTENDED_PARAMS_SZ	2

#define DEFAULT_SZ 100
#define DEFAULT_ID 0XFF
#define ADSP_PROPERTIES_SZ	0x64
#define ADSP_RESOURCE_STATE_SZ	0x18
#define FIRMWARE_CONFIG_SZ	0x14c
#define HARDWARE_CONFIG_SZ	0x84
#define MODULES_INFO_SZ		0xa70
#define PIPELINE_LIST_INFO_SZ	0xc
#define PIPELINE_PROPS_SZ	0x60
#define SCHEDULERS_INFO_SZ	0x34
#define GATEWAYS_INFO_SZ	0x4e4
#define MEMORY_STATE_INFO_SZ	0x1000
#define POWER_STATE_INFO_SZ	0x1000

struct nhlt_blob {
	size_t size;
	struct nhlt_specific_cfg *cfg;
};

struct skl_pipe_event_data {
	long event_time;
	int event_type;
};

struct skl_debug {
	struct skl *skl;
	struct device *dev;

	struct dentry *fs;
	struct dentry *modules;
	struct dentry *nhlt;
	u8 fw_read_buff[FW_REG_BUF];
	struct nhlt_blob ssp_blob[2*MAX_SSP];
	struct nhlt_blob dmic_blob;
	u32 ipc_data[MAX_SZ];
	struct fw_ipc_data fw_ipc_data;
	struct skl_pipe_event_data data;
};

struct nhlt_specific_cfg
*skl_nhlt_get_debugfs_blob(struct skl_debug *d, u8 link_type, u32 instance,
		u8 stream)
{
	switch (link_type) {
	case NHLT_LINK_DMIC:
		return d->dmic_blob.cfg;

	case NHLT_LINK_SSP:
		if (instance >= MAX_SSP)
			return NULL;

		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			return d->ssp_blob[instance].cfg;
		else
			return d->ssp_blob[MAX_SSP + instance].cfg;

	default:
		break;
	}

	dev_err(d->dev, "NHLT debugfs query failed\n");
	return NULL;
}

static ssize_t nhlt_read(struct file *file, char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct nhlt_blob *blob = file->private_data;

	if (!blob->cfg)
		return -EIO;

	return simple_read_from_buffer(user_buf, count, ppos,
			blob->cfg, blob->size);
}

static ssize_t nhlt_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct nhlt_blob *blob = file->private_data;
	struct nhlt_specific_cfg *new_cfg;
	ssize_t written;
	size_t size = blob->size;

	if (!blob->cfg) {
		/* allocate mem for blob */
		blob->cfg = kzalloc(count, GFP_KERNEL);
		if (!blob->cfg)
			return -ENOMEM;
		size = count;
	} else if (blob->size < count) {
		/* size if different, so relloc */
		new_cfg = krealloc(blob->cfg, count, GFP_KERNEL);
		if (!new_cfg)
			return -ENOMEM;
		size = count;
		blob->cfg = new_cfg;
	}

	written = simple_write_to_buffer(blob->cfg, size, ppos,
						user_buf, count);
	blob->size = written;

	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_NOW_UNRELIABLE);

	print_hex_dump(KERN_DEBUG, "Debugfs Blob:", DUMP_PREFIX_OFFSET, 8, 4,
			blob->cfg, blob->size, false);

	return written;
}

static const struct file_operations nhlt_fops = {
	.open = simple_open,
	.read = nhlt_read,
	.write = nhlt_write,
	.llseek = default_llseek,
};

static ssize_t mod_control_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct skl_debug *d = file->private_data;
	const u32 param_data_size = d->ipc_data[0];
	const u32 *param_data = &d->ipc_data[1];

	return simple_read_from_buffer(user_buf, count, ppos,
					param_data, param_data_size);

}

static ssize_t mod_control_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct skl_debug *d = file->private_data;
	struct mod_set_get *mod_set_get;
	char *buf;
	int retval, type;
	ssize_t written;
	u32 size, mbsz;

	struct skl_sst *ctx = d->skl->skl_sst;
	struct skl_ipc_large_config_msg msg;
	struct skl_ipc_header header = {0};
	u64 *ipc_header = (u64 *)(&header);

	d->ipc_data[0] = 0;

	buf = kzalloc(MOD_BUF, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	written = simple_write_to_buffer(buf, MOD_BUF, ppos,
						user_buf, count);
	size = written;
	print_hex_dump(KERN_DEBUG, "buf :", DUMP_PREFIX_OFFSET, 8, 4,
			buf, size, false);

	mod_set_get = (struct mod_set_get *)buf;
	header.primary = mod_set_get->primary;
	header.extension = mod_set_get->extension;

	mbsz = mod_set_get->size - (sizeof(u32)*2);
	print_hex_dump(KERN_DEBUG, "header mailbox:", DUMP_PREFIX_OFFSET, 8, 4,
			mod_set_get->mailbx, size-12, false);
	type =  ((0x1f000000) & (mod_set_get->primary))>>24;

	switch (type) {

	case IPC_MOD_LARGE_CONFIG_GET:
		msg.module_id = (header.primary) & 0x0000ffff;
		msg.instance_id = ((header.primary) & 0x00ff0000)>>16;
		msg.large_param_id = ((header.extension) & 0x0ff00000)>>20;
		msg.param_data_size = (header.extension) & 0x000fffff;

		if (mbsz)
			retval = skl_ipc_get_large_config(&ctx->ipc, &msg,
							&d->ipc_data[1],
							&mod_set_get->mailbx[0],
							mbsz, NULL);
		else
			retval = skl_ipc_get_large_config(&ctx->ipc,
							&msg,
							&d->ipc_data[1],
							NULL, 0, NULL);
		if (retval == 0)
			d->ipc_data[0] = msg.param_data_size;
		break;

	case IPC_MOD_LARGE_CONFIG_SET:
		msg.module_id = (header.primary) & 0x0000ffff;
		msg.instance_id = ((header.primary) & 0x00ff0000)>>16;
		msg.large_param_id = ((header.extension) & 0x0ff00000)>>20;
		msg.param_data_size = (header.extension) & 0x000fffff;

		retval = skl_ipc_set_large_config(&ctx->ipc, &msg,
						(u32 *)(&mod_set_get->mailbx));
		break;

	default:
		if (mbsz)
			retval = sst_ipc_tx_message_wait(&ctx->ipc, *ipc_header,
				mod_set_get->mailbx, mbsz, NULL, NULL);

		else
			retval = sst_ipc_tx_message_wait(&ctx->ipc, *ipc_header,
				NULL, 0, NULL, NULL);

		break;

	}

	if (retval)
		return -EIO;

	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_NOW_UNRELIABLE);
	kfree(buf);
	return written;
}

static const struct file_operations set_get_ctrl_fops = {
	.open = simple_open,
	.read = mod_control_read,
	.write = mod_control_write,
	.llseek = default_llseek,
};

static int skl_init_mod_set_get(struct skl_debug *d)
{
	if (!debugfs_create_file("set_get_ctrl", 0644, d->modules, d,
				 &set_get_ctrl_fops)) {
		dev_err(d->dev, "module set get ctrl debugfs init failed\n");
		return -EIO;
	}
	return 0;
}

static ssize_t skl_print_pins(struct skl_module_pin *m_pin, char *buf,
				int max_pin, ssize_t size, bool direction)
{
	int i;
	ssize_t ret = 0;

	for (i = 0; i < max_pin; i++)
		ret += snprintf(buf + size, MOD_BUF - size,
				"%s %d\n\tModule %d\n\tInstance %d\n\t"
				"In-used %s\n\tType %s\n"
				"\tState %d\n\tIndex %d\n",
				direction ? "Input Pin:" : "Output Pin:",
				i, m_pin[i].id.module_id,
				m_pin[i].id.instance_id,
				m_pin[i].in_use ? "Used" : "Unused",
				m_pin[i].is_dynamic ? "Dynamic" : "Static",
				m_pin[i].pin_state, i);
	return ret;
}

static ssize_t skl_print_fmt(struct skl_module_fmt *fmt, char *buf,
					ssize_t size, bool direction)
{
	return snprintf(buf + size, MOD_BUF - size,
			"%s\n\tCh %d\n\tFreq %d\n\tBit depth %d\n\t"
			"Valid bit depth %d\n\tCh config %#x\n\tInterleaving %d\n\t"
			"Sample Type %d\n\tCh Map %#x\n",
			direction ? "Input Format:" : "Output Format:",
			fmt->channels, fmt->s_freq, fmt->bit_depth,
			fmt->valid_bit_depth, fmt->ch_cfg,
			fmt->interleaving_style, fmt->sample_type,
			fmt->ch_map);
}

static ssize_t module_read(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	struct skl_module_cfg *mconfig = file->private_data;
	struct skl_module *module = mconfig->module;
	struct skl_module_res *res = &module->resources[mconfig->res_idx];
	char *buf;
	ssize_t ret;

	buf = kzalloc(MOD_BUF, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = snprintf(buf, MOD_BUF, "Module:\n\tUUID %pUL\n\tModule id %d\n"
			"\tInstance id %d\n\tPvt_id %d\n", mconfig->guid,
			mconfig->id.module_id, mconfig->id.instance_id,
			mconfig->id.pvt_id);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"Resources:\n\tCPC %#x\n\tIBS %#x\n\tOBS %#x\t\n",
			res->cpc, res->ibs, res->obs);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"Module data:\n\tCore %d\n\tIn queue %d\n\t"
			"Out queue %d\n\tType %s\n",
			mconfig->core_id, mconfig->max_in_queue,
			mconfig->max_out_queue,
			mconfig->is_loadable ? "loadable" : "inbuilt");

	ret += skl_print_fmt(mconfig->in_fmt, buf, ret, true);
	ret += skl_print_fmt(mconfig->out_fmt, buf, ret, false);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"Fixup:\n\tParams %#x\n\tConverter %#x\n",
			mconfig->params_fixup, mconfig->converter);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"Module Gateway:\n\tType %#x\n\tVbus %#x\n\tHW conn %#x\n\tSlot %#x\n",
			mconfig->dev_type, mconfig->vbus_id,
			mconfig->hw_conn_type, mconfig->time_slot);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"Pipeline:\n\tID %d\n\tPriority %d\n\tConn Type %d\n\t"
			"Pages %#x\n", mconfig->pipe->ppl_id,
			mconfig->pipe->pipe_priority, mconfig->pipe->conn_type,
			mconfig->pipe->memory_pages);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"\tParams:\n\t\tHost DMA %d\n\t\tLink DMA %d\n",
			mconfig->pipe->p_params->host_dma_id,
			mconfig->pipe->p_params->link_dma_id);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"\tPCM params:\n\t\tCh %d\n\t\tFreq %d\n\t\tFormat %d\n",
			mconfig->pipe->p_params->ch,
			mconfig->pipe->p_params->s_freq,
			mconfig->pipe->p_params->s_fmt);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"\tLink %#x\n\tStream %#x\n",
			mconfig->pipe->p_params->linktype,
			mconfig->pipe->p_params->stream);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"\tState %d\n\tPassthru %s\n",
			mconfig->pipe->state,
			mconfig->pipe->passthru ? "true" : "false");

	ret += skl_print_pins(mconfig->m_in_pin, buf,
			mconfig->max_in_queue, ret, true);
	ret += skl_print_pins(mconfig->m_out_pin, buf,
			mconfig->max_out_queue, ret, false);

	ret += snprintf(buf + ret, MOD_BUF - ret,
			"Other:\n\tDomain %d\n\tHomogenous Input %s\n\t"
			"Homogenous Output %s\n\tIn Queue Mask %d\n\t"
			"Out Queue Mask %d\n\tDMA ID %d\n\tMem Pages %d\n\t"
			"Module Type %d\n\tModule State %d\n",
			mconfig->domain,
			mconfig->homogenous_inputs ? "true" : "false",
			mconfig->homogenous_outputs ? "true" : "false",
			mconfig->in_queue_mask, mconfig->out_queue_mask,
			mconfig->dma_id, mconfig->mem_pages, mconfig->m_state,
			mconfig->m_type);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);

	kfree(buf);
	return ret;
}

static const struct file_operations mcfg_fops = {
	.open = simple_open,
	.read = module_read,
	.llseek = default_llseek,
};

void skl_debug_init_module(struct skl_debug *d,
			struct snd_soc_dapm_widget *w,
			struct skl_module_cfg *mconfig)
{
	if (!debugfs_create_file(w->name, 0444,
				d->modules, mconfig,
				&mcfg_fops))
		dev_err(d->dev, "%s: module debugfs init failed\n", w->name);
}

static ssize_t fw_softreg_read(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	struct skl_debug *d = file->private_data;
	struct sst_dsp *sst = d->skl->skl_sst->dsp;
	size_t w0_stat_sz = sst->addr.w0_stat_sz;
	void __iomem *in_base = sst->mailbox.in_base;
	void __iomem *fw_reg_addr;
	unsigned int offset;
	char *tmp;
	ssize_t ret = 0;

	tmp = kzalloc(FW_REG_BUF, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	fw_reg_addr = in_base - w0_stat_sz;
	memset(d->fw_read_buff, 0, FW_REG_BUF);

	if (w0_stat_sz > 0)
		__iowrite32_copy(d->fw_read_buff, fw_reg_addr, w0_stat_sz >> 2);

	for (offset = 0; offset < FW_REG_SIZE; offset += 16) {
		ret += snprintf(tmp + ret, FW_REG_BUF - ret, "%#.4x: ", offset);
		hex_dump_to_buffer(d->fw_read_buff + offset, 16, 16, 4,
				   tmp + ret, FW_REG_BUF - ret, 0);
		ret += strlen(tmp + ret);

		/* print newline for each offset */
		if (FW_REG_BUF - ret > 0)
			tmp[ret++] = '\n';
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, tmp, ret);
	kfree(tmp);

	return ret;
}

static const struct file_operations soft_regs_ctrl_fops = {
	.open = simple_open,
	.read = fw_softreg_read,
	.llseek = default_llseek,
};

static void skl_exit_nhlt(struct skl_debug *d)
{
	int i;

	/* free blob memory, if allocated */
	for (i = 0; i < MAX_SSP; i++)
		kfree(d->ssp_blob[MAX_SSP + i].cfg);
}

static ssize_t nhlt_control_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct skl_debug *d = file->private_data;
	char *state;

	state = d->skl->nhlt_override ? "enable\n" : "disable\n";
	return simple_read_from_buffer(user_buf, count, ppos,
			state, strlen(state));
}

static ssize_t nhlt_control_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct skl_debug *d = file->private_data;
	char buf[16];
	int len = min(count, (sizeof(buf) - 1));


	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = 0;

	if (!strncmp(buf, "enable\n", len)) {
		d->skl->nhlt_override = true;
	} else if (!strncmp(buf, "disable\n", len)) {
		d->skl->nhlt_override = false;
		skl_exit_nhlt(d);
	} else {
		return -EINVAL;
	}

	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_NOW_UNRELIABLE);

	return len;
}

static const struct file_operations ssp_cntrl_nhlt_fops = {
	.open = simple_open,
	.read = nhlt_control_read,
	.write = nhlt_control_write,
	.llseek = default_llseek,
};

static int skl_init_nhlt(struct skl_debug *d)
{
	int i;
	char name[12];

	if (!debugfs_create_file("control",
				0644, d->nhlt,
				d, &ssp_cntrl_nhlt_fops)) {
		dev_err(d->dev, "nhlt control debugfs init failed\n");
		return -EIO;
	}

	for (i = 0; i < MAX_SSP; i++) {
		snprintf(name, (sizeof(name)-1), "ssp%dp", i);
		if (!debugfs_create_file(name,
					0644, d->nhlt,
					&d->ssp_blob[i], &nhlt_fops))
			dev_err(d->dev, "%s: debugfs init failed\n", name);
		snprintf(name, (sizeof(name)-1), "ssp%dc", i);
		if (!debugfs_create_file(name,
					0644, d->nhlt,
					&d->ssp_blob[MAX_SSP + i], &nhlt_fops))
			dev_err(d->dev, "%s: debugfs init failed\n", name);
	}

	if (!debugfs_create_file("dmic", 0644,
				d->nhlt, &d->dmic_blob,
				&nhlt_fops))
		dev_err(d->dev, "%s: debugfs init failed\n", name);

	return 0;
}

static ssize_t adsp_control_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{

	struct skl_debug *d = file->private_data;
	char *buf1;
	ssize_t ret;
	unsigned int data, ofs = 0;
	int replysz = 0;

	mutex_lock(&d->fw_ipc_data.mutex);
	replysz = d->fw_ipc_data.replysz;
	data = d->fw_ipc_data.adsp_id;

	buf1 = kzalloc(MOD_BUF1, GFP_ATOMIC);
	if (!buf1) {
		mutex_unlock(&d->fw_ipc_data.mutex);
		return -ENOMEM;
	}

	ret = snprintf(buf1, MOD_BUF1,
			"\nADSP_PROP ID %x\n", data);
	for (ofs = 0 ; ofs < replysz ; ofs += 16) {
		ret += snprintf(buf1 + ret, MOD_BUF1 - ret,
			"0x%.4x : ", ofs);
		hex_dump_to_buffer((u8 *)(&(d->fw_ipc_data.mailbx[0])) + ofs,
					16, 16, 4,
					buf1 + ret, MOD_BUF1 - ret, 0);
		ret += strlen(buf1 + ret);
		if (MOD_BUF1 - ret > 0)
			buf1[ret++] = '\n';
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buf1, ret);
	mutex_unlock(&d->fw_ipc_data.mutex);
	kfree(buf1);

	return ret;
}

static ssize_t adsp_control_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct skl_debug *d = file->private_data;
	char buf[8];
	int err, replysz;
	unsigned int dsp_property;
	u32 *ipc_data;
	struct skl_sst *ctx = d->skl->skl_sst;
	struct skl_ipc_large_config_msg msg;
	char id[8];
	u32 tx_data[EXTENDED_PARAMS_SZ];
	int j = 0, bufsize, tx_param = 0, tx_param_id;
	int len = min(count, (sizeof(buf)-1));

	mutex_lock(&d->fw_ipc_data.mutex);
	if (copy_from_user(buf, user_buf, len)) {
		mutex_unlock(&d->fw_ipc_data.mutex);
		return -EFAULT;
	}

	buf[len] = '\0';
	bufsize = strlen(buf);

	while (buf[j] != '\0') {
		if (buf[j] == ',') {
			if ((bufsize-j) > sizeof(id)) {
				dev_err(d->dev, "ID buffer overflow\n");
				return -EINVAL;
			}
			strncpy(id, &buf[j+1], (bufsize-j));
			buf[j] = '\0';
			tx_param = 1;
		} else
			j++;
	}

	err = kstrtouint(buf, 10, &dsp_property);
	if (err)
		return -EINVAL;

	if ((dsp_property == DMA_CONTROL) || (dsp_property == ENABLE_LOGS)) {
		dev_err(d->dev, "invalid input !! not readable\n");
		mutex_unlock(&d->fw_ipc_data.mutex);
		return -EINVAL;
	}

	if (tx_param == 1) {
		err = kstrtouint(id, 10, &tx_param_id);
		if (err)
			return -EINVAL;

		tx_data[0] = (tx_param_id << 8) | dsp_property;
		tx_data[1] = MAX_TLV_PAYLOAD_SIZE;
	}

	ipc_data = kzalloc(DSP_BUF, GFP_ATOMIC);
	if (!ipc_data) {
		mutex_unlock(&d->fw_ipc_data.mutex);
		return -ENOMEM;
	}

	switch (dsp_property) {

	case ADSP_PROPERTIES:
	replysz = ADSP_PROPERTIES_SZ;
	break;

	case ADSP_RESOURCE_STATE:
	replysz = ADSP_RESOURCE_STATE_SZ;
	break;

	case FIRMWARE_CONFIG:
	replysz = FIRMWARE_CONFIG_SZ;
	break;

	case HARDWARE_CONFIG:
	replysz = HARDWARE_CONFIG_SZ;
	break;

	case MODULES_INFO:
	replysz = MODULES_INFO_SZ;
	break;

	case PIPELINE_LIST_INFO:
	replysz = PIPELINE_LIST_INFO_SZ;
	break;

	case PIPELINE_PROPS:
	replysz = PIPELINE_PROPS_SZ;
	break;

	case SCHEDULERS_INFO:
	replysz = SCHEDULERS_INFO_SZ;
	break;

	case GATEWAYS_INFO:
	replysz = GATEWAYS_INFO_SZ;
	break;

	case MEMORY_STATE_INFO:
	replysz = MEMORY_STATE_INFO_SZ;
	break;

	case POWER_STATE_INFO:
	replysz = POWER_STATE_INFO_SZ;
	break;

	default:
	mutex_unlock(&d->fw_ipc_data.mutex);
	kfree(ipc_data);
	return -EINVAL;
	}

	msg.module_id = 0x0;
	msg.instance_id = 0x0;

	if (tx_param == 1)
		msg.large_param_id = 0xFF;
	else
		msg.large_param_id = dsp_property;

	msg.param_data_size = replysz;

	if (tx_param == 1)
		skl_ipc_get_large_config(&ctx->ipc, &msg,
				ipc_data, tx_data,
				EXTENDED_PARAMS_SZ*sizeof(u32), NULL);
	else
		skl_ipc_get_large_config(&ctx->ipc, &msg,
				ipc_data, NULL,
				0, NULL);

	memset(&d->fw_ipc_data.mailbx[0], 0, DSP_BUF);

	memcpy(&d->fw_ipc_data.mailbx[0], ipc_data, replysz);

	d->fw_ipc_data.adsp_id = dsp_property;

	d->fw_ipc_data.replysz = replysz;

	/* Userspace has been fiddling around behindthe kernel's back*/
	add_taint(TAINT_USER, LOCKDEP_NOW_UNRELIABLE);
	mutex_unlock(&d->fw_ipc_data.mutex);
	kfree(ipc_data);

	return len;
}

static const struct file_operations ssp_cntrl_adsp_fops = {
	.open = simple_open,
	.read = adsp_control_read,
	.write = adsp_control_write,
	.llseek = default_llseek,
};

static int skl_init_adsp(struct skl_debug *d)
{
	if (!debugfs_create_file("adsp_prop_ctrl", 0644, d->fs, d,
				 &ssp_cntrl_adsp_fops)) {
		dev_err(d->dev, "adsp control debugfs init failed\n");
		return -EIO;
	}

	memset(&d->fw_ipc_data.mailbx[0], 0, DSP_BUF);
	d->fw_ipc_data.replysz = DEFAULT_SZ;
	d->fw_ipc_data.adsp_id = DEFAULT_ID;

	return 0;
}

static ssize_t core_power_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct skl_debug *d = file->private_data;
	struct skl_sst *skl_ctx = d->skl->skl_sst;
	struct sst_dsp *ctx = skl_ctx->dsp;
	char buf[16];
	int len = min(count, (sizeof(buf) - 1));
	unsigned int core_id;
	char *ptr;
	int wake;
	int err;


	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;
	buf[len] = 0;

	/*
	 * The buffer content should be "wake n" or "sleep n",
	 * where n is the core id
	 */
	ptr = strnstr(buf, "wake", len);
	if (ptr) {
		ptr = ptr + 5;
		wake = 1;
	} else {
		ptr = strnstr(buf, "sleep", len);
		if (ptr) {
			ptr = ptr + 6;
			wake = 0;
		} else
			return -EINVAL;
	}

	err = kstrtouint(ptr, 10, &core_id);
	if (err) {
		dev_err(d->dev, "%s: Debugfs kstrtouint returned error = %d\n",
				__func__, err);
		return err;
	}

	dev_info(d->dev, "Debugfs: %s %d\n", wake ? "wake" : "sleep", core_id);

	if (wake) {
		if (core_id == SKL_DSP_CORE0_ID)
			pm_runtime_get_sync(d->dev);
		else
			skl_dsp_get_core(ctx, core_id);
	} else {
		if (core_id == SKL_DSP_CORE0_ID)
			pm_runtime_put_sync(d->dev);
		else
			skl_dsp_put_core(ctx, core_id);
	}

	/* Userspace has been fiddling around behind the kernel's back */
	add_taint(TAINT_USER, LOCKDEP_NOW_UNRELIABLE);

	return len;
}
static const struct file_operations core_power_fops = {
	.open = simple_open,
	.write = core_power_write,
	.llseek = default_llseek,
};

void skl_dbg_event(struct skl_sst *ctx, int type)
{
	int retval;
	struct timeval pipe_event_tv;
	struct skl *skl = get_skl_ctx(ctx->dev);
	struct kobject *kobj;

	kobj = &skl->platform->dev->kobj;

	if (type == SKL_PIPE_CREATED)
		/* pipe creation event */
		retval = kobject_uevent(kobj, KOBJ_ADD);
	else if (type == SKL_PIPE_INVALID)
		/* pipe deletion event */
		retval = kobject_uevent(kobj, KOBJ_REMOVE);
	else
		return;

	if (retval < 0) {
		dev_err(ctx->dev,
			"pipeline uevent failed, ret = %d\n", retval);
		return;
	}

	do_gettimeofday(&pipe_event_tv);

	skl->debugfs->data.event_time = pipe_event_tv.tv_usec;
	skl->debugfs->data.event_type = type;
}

static ssize_t skl_dbg_event_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct skl_debug *d = file->private_data;
	char buf[32];
	char pipe_state[24];
	int retval;

	if (d->data.event_type)
		strcpy(pipe_state, "SKL_PIPE_CREATED");
	else
		strcpy(pipe_state, "SKL_PIPE_INVALID");

	retval = snprintf(buf, sizeof(buf), "%s - %ld\n",
			pipe_state, d->data.event_time);

	return simple_read_from_buffer(user_buf, count, ppos, buf, retval);
}

static const struct file_operations skl_dbg_event_fops = {
	.open = simple_open,
	.read = skl_dbg_event_read,
	.llseek = default_llseek,
};

static int skl_init_dbg_event(struct skl_debug *d)
{
	if (!debugfs_create_file("dbg_event", 0644, d->fs, d,
				&skl_dbg_event_fops)) {
		dev_err(d->dev, "dbg_event debugfs file creation failed\n");
		return -EIO;
	}

	return 0;
}

struct skl_debug *skl_debugfs_init(struct skl *skl)
{
	struct skl_debug *d;
	int ret;

	d = devm_kzalloc(&skl->pci->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return NULL;

	mutex_init(&d->fw_ipc_data.mutex);
	/* create the debugfs dir with platform component's debugfs as parent */
	d->fs = debugfs_create_dir("dsp",
				   skl->platform->component.debugfs_root);
	if (IS_ERR(d->fs) || !d->fs) {
		dev_err(&skl->pci->dev, "debugfs root creation failed\n");
		return NULL;
	}

	d->skl = skl;
	d->dev = &skl->pci->dev;

	/* now create the module dir */
	d->modules = debugfs_create_dir("modules", d->fs);
	if (IS_ERR(d->modules) || !d->modules) {
		dev_err(&skl->pci->dev, "modules debugfs create failed\n");
		goto err;
	}

	if (!debugfs_create_file("fw_soft_regs_rd", 0444, d->fs, d,
				 &soft_regs_ctrl_fops)) {
		dev_err(d->dev, "fw soft regs control debugfs init failed\n");
		goto err;
	}

	if (!debugfs_create_file("core_power", 0644, d->fs, d,
			 &core_power_fops)) {
	dev_err(d->dev, "core power debugfs init failed\n");
	goto err;
	}

	/* now create the NHLT dir */
	d->nhlt =  debugfs_create_dir("nhlt", d->fs);
	if (IS_ERR(d->nhlt) || !d->nhlt) {
		dev_err(&skl->pci->dev, "nhlt debugfs create failed\n");
		return NULL;
	}

	skl_init_nhlt(d);
	skl_init_adsp(d);
	skl_init_mod_set_get(d);

	ret = skl_init_dbg_event(d);
	if (ret < 0) {
		dev_err(&skl->pci->dev,
			"dbg_event debugfs init failed, ret = %d\n", ret);
		goto err;
	}

	return d;

err:
	debugfs_remove_recursive(d->fs);
	return NULL;
}
