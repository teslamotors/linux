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
#include "skl.h"
#include "skl-sst-dsp.h"
#include "skl-sst-ipc.h"
#include "skl-tplg-interface.h"
#include "skl-topology.h"
#include "../common/sst-dsp.h"
#include "../common/sst-dsp-priv.h"
#include "skl-nhlt.h"

#define MOD_BUF		PAGE_SIZE
#define FW_REG_BUF	PAGE_SIZE
#define FW_REG_SIZE	0x60
#define MAX_SSP 	4

struct nhlt_blob {
	size_t size;
	struct nhlt_specific_cfg *cfg;
};

struct skl_debug {
	struct skl *skl;
	struct device *dev;

	struct dentry *fs;
	struct dentry *modules;
	struct dentry *nhlt;
	u8 fw_read_buff[FW_REG_BUF];
	struct nhlt_blob ssp_blob[MAX_SSP];
	struct nhlt_blob dmic_blob;
};

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
			"Resources:\n\tMCPS %#x\n\tIBS %#x\n\tOBS %#x\t\n",
			mconfig->mcps, mconfig->ibs, mconfig->obs);

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
		kfree(d->ssp_blob[i].cfg);
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
		snprintf(name, (sizeof(name)-1), "ssp%d", i);
		if (!debugfs_create_file(name,
					0644, d->nhlt,
					&d->ssp_blob[i], &nhlt_fops))
			dev_err(d->dev, "%s: debugfs init failed\n", name);
	}

	if (!debugfs_create_file("dmic", 0644,
				d->nhlt, &d->dmic_blob,
				&nhlt_fops))
		dev_err(d->dev, "%s: debugfs init failed\n", name);

	return 0;
}

struct skl_debug *skl_debugfs_init(struct skl *skl)
{
	struct skl_debug *d;

	d = devm_kzalloc(&skl->pci->dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return NULL;

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

	/* now create the NHLT dir */
	d->nhlt =  debugfs_create_dir("nhlt", d->fs);
	if (IS_ERR(d->nhlt) || !d->nhlt) {
		dev_err(&skl->pci->dev, "nhlt debugfs create failed\n");
		return NULL;
	}

	skl_init_nhlt(d);

	return d;

err:
	debugfs_remove_recursive(d->fs);
	return NULL;
}
