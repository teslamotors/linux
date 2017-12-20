/* compat26/tegra/procfs.c
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * linux proc-fs compatibility from older kernels
*/

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "compat26.h"

struct compat_file_operations {
	read_proc_t		*read;
	void			*data;
};

struct compat_file_session {
	struct compat_file_operations *ops;
	loff_t			size;
};

static ssize_t pfs_read(struct file *f, char __user *buff,
			size_t sz, loff_t *off)
{
	struct compat_file_session *ses = f->private_data;
	struct compat_file_operations *cfo = ses->ops;
	loff_t loff = *off;
	ssize_t ret;
	char *bp;
	int eof = 0;

	if (sz > PAGE_SIZE)
		sz = PAGE_SIZE;

	if (sz == 0 || loff >= ses->size)
		return 0;

	bp = kmalloc(sz, GFP_KERNEL);
	if (!bp)
		return -ENOMEM;

	ret = cfo->read(bp, NULL, loff, sz, &eof, cfo->data);
	if (ret >= 0) {
		int cp = copy_to_user(buff, bp, ret);
		if (cp > 0)
			ret = -EFAULT;
		else
			*off = loff + ret;

		if (eof)
			ses->size = loff + ret;
	}

	kfree(bp);
	return ret;
}

static int pfs_open(struct inode *inode, struct file *file)
{
	struct compat_file_session *sess;

	sess = kmalloc(sizeof(*sess), GFP_KERNEL);
	if (!sess)
		return -ENOMEM;

	sess->size = UINT_MAX;
	sess->ops = PDE_DATA(inode);
	file->private_data = sess;
	return 0;
}

static int pfs_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	file->private_data = NULL;
	return 0;
}

static const struct file_operations pfs_fops = {
	.owner	= THIS_MODULE,
	.read	= pfs_read,
	.open	= pfs_open,
	.release = pfs_release,
	.llseek = generic_file_llseek,
};

struct proc_dir_entry *create_proc_read_entry(const char *name,
					      umode_t mode,
					      struct proc_dir_entry *base, 
					      read_proc_t *read_proc,
					      void * data)
{
	struct proc_dir_entry *res;
	struct compat_file_operations *fops;

	fops = kzalloc(sizeof(struct compat_file_operations), GFP_KERNEL);
	if (WARN_ON(!fops))
		return NULL;

	fops->read = read_proc;
	fops->data = data;
	res = proc_create_data(name, mode, base, &pfs_fops, fops);
	return res;
}
