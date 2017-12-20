/* Tegra legacy proc driver
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/moduleparam.h>

#include <asm/system_info.h>

struct legacy_proc_ent {
	struct proc_dir_entry *proc;
	const char	*name;
	const char	*value;
};

static struct legacy_proc_ent legacy_bserial	= { .name = "board_serial", };
static struct legacy_proc_ent legacy_skuinfo	= { .name = "skuinfo", };
static struct legacy_proc_ent legacy_skuver	= { .name = "skuver", };
static struct legacy_proc_ent legacy_prodinfo	= { .name = "prodinfo", };
static struct legacy_proc_ent legacy_prodver	= { .name = "prodver", };
static struct legacy_proc_ent legacy_boardrev	= { .name = "boardrev", };

static struct legacy_proc_ent *legacy_ents[] = {
	&legacy_bserial,
	&legacy_skuinfo,
	&legacy_skuver,
	&legacy_prodinfo,
	&legacy_prodver,
	&legacy_boardrev,
	NULL
};

static char board_rev_buf[18];	/* for hex string and terminator */

static int legacy_param_set(const char *val, const struct kernel_param *kp)
{
	struct legacy_proc_ent *ent = kp->arg;

	ent->value = kstrdup(val, GFP_KERNEL);
	if (!ent->value)
		return -ENOMEM;

	pr_debug("legacy_param: set %s to %s\n", ent->name, ent->value);
	return 0;
}

static int legacy_param_get(char *buffer, const struct kernel_param *kp)
{
	struct legacy_proc_ent *ent = kp->arg;

	if (!ent->value)
		return -EINVAL;
	return snprintf(buffer, PAGE_SIZE, "%s", ent->value);
}

static struct kernel_param_ops legacy_param_ops = {
	.set = legacy_param_set,
	.get = legacy_param_get,
};

/* we are slighlty abusing the moduleparam code here, but it is the only
 * way to get callbacks to happen nicely when we are able to allocate
 * memory and without any module name prefix to the parameters.
 */
#define legacy_param(__param, __desc, __info)	\
	__module_param_call("", __param, &legacy_param_ops, __info, 0444, 4, 0)

legacy_param(nvsku, nvsku, &legacy_skuinfo);
legacy_param(SkuVer, skuver, &legacy_skuver);
legacy_param(ProdVer, prodver, &legacy_prodver);
legacy_param(ProdInfo, prodinfo, &legacy_prodinfo);
legacy_param(boardrev, boardrev, &legacy_boardrev);

static int legacy_proc_show(struct seq_file *m, void *v)
{
	struct legacy_proc_ent *ent = m->private;

	if (ent->value) {
		seq_printf(m, "%s\n", ent->value);
		return 0;
	}

	return -EINVAL;
}

static int legacy_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, legacy_proc_show, PDE_DATA(inode));
}

static const struct file_operations legacy_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= legacy_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init legacy_init(void)
{
	struct legacy_proc_ent **ents = legacy_ents;
	struct legacy_proc_ent *ent;
	u64 rev;

	rev = system_serial_low;
	rev |= ((u64)system_serial_high) << 32;

	legacy_bserial.value = board_rev_buf;
	snprintf(board_rev_buf, sizeof(board_rev_buf), "%013llu", rev);

	for (ent = *ents; (ent = *ents) != NULL; ents++) {
		pr_debug("%s: creating %s (%s)\n",
			__func__, ent->name, ent->value);
		ent->proc = proc_create_data(ent->name, 0444, NULL,
				       &legacy_proc_fops, ent);
		if (!ent->proc)
			pr_err("%s: error creating %s\n", __func__, ent->name);
	}

	return 0;
}

arch_initcall(legacy_init);
