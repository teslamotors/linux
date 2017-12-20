/* compat26/tegra/core.c
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * Handle the core of the tegra26 compatibility layer
*/

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/slab.h>

#include "compat26_internal.h"

struct compat26_debug_file {
	struct dentry	*dent;
	int		(*print)(struct seq_file *s);
};

struct dentry *tegra26_debug_root;

static int __init tegra26_debugfs_init(void)
{
	tegra26_debug_root = debugfs_create_dir("compat26", NULL);
	if (IS_ERR(tegra26_debug_root)) {
		pr_err("%s: failed to make directory\n", __func__);
		return PTR_ERR(tegra26_debug_root);
	}

	return 0;
}

postcore_initcall(tegra26_debugfs_init);

static ssize_t tegra26_debugfs_show(struct seq_file *s, void *data)
{
	struct compat26_debug_file *dfile = s->private;
	return dfile->print(s);
}

static int tegra26_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra26_debugfs_show, inode->i_private);
}

static struct file_operations tegra26_debugfs_fops_ro = {
	.open	= tegra26_debugfs_open,
	.release = single_release,
	.read	= seq_read,
	.llseek	= seq_lseek,
};

int tegra26_debugfs_add_ro(const char *name, int (*print)(struct seq_file *s))
{
	struct compat26_debug_file *dfile;
	struct dentry *dent;

	if (WARN_ON(!name) || WARN_ON(!print))
		return -EINVAL;

	dfile = kcalloc(1, sizeof(struct compat26_debug_file), GFP_KERNEL);
	if (!dfile) {
		pr_err("%s: cannot make file (no memory)\n", __func__);
		return -ENOMEM;
	}

	dfile->print = print;

	dent = debugfs_create_file(name, S_IRUGO, tegra26_debug_root, dfile,
				   &tegra26_debugfs_fops_ro);
	if (IS_ERR(dent)) {
		pr_err("%s: cannot create file '%s'\n", __func__, name);
		kfree(dfile);
		return PTR_ERR(dent);
	}

	dfile->dent = dent;
	return 0;
}
EXPORT_SYMBOL(tegra26_debugfs_add_ro);

