/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include "bpmp.h"

#define DEBUGFS_S_ISDIR		(1 << 9)
#define DEBUGFS_S_IRUSR		(1 << 8)
#define DEBUGFS_S_IWUSR		(1 << 7)

enum mrq_debugfs_data_in_req {
	MRQ_DEBUGFS_DATA_IN_REQ_READ = 1,
	MRQ_DEBUGFS_DATA_IN_REQ_WRITE = 2,
	MRQ_DEBUGFS_DATA_IN_REQ_DUMPDIR = 3,
};

struct mrq_debugfs_data_in_read {
	u32 fnameaddr;
	u32 fnamelen;
	u32 dataaddr;
	u32 datalen;
} __packed;

struct mrq_debugfs_data_in_write {
	u32 fnameaddr;
	u32 fnamelen;
	u32 dataaddr;
	u32 datalen;
} __packed;

struct mrq_debugfs_data_in_dumpdir {
	u32 addr;
	u32 len;
} __packed;

struct mrq_debugfs_data_out_read {
	s32 err;
	u32 bytes_read;
} __packed;

struct mrq_debugfs_data_out_write {
	s32 err;
	u32 bytes_written;
} __packed;

struct mrq_debugfs_data_out_dumpdir {
	s32 err;
	u32 len;
} __packed;

struct mrq_debugfs_data_in {
	u32 req;
	union {
		struct mrq_debugfs_data_in_read read;
		struct mrq_debugfs_data_in_write write;
		struct mrq_debugfs_data_in_dumpdir dumpdir;
	} data;
} __packed;

struct mrq_debugfs_data_out {
	u32 req;
	union {
		struct mrq_debugfs_data_out_read read;
		struct mrq_debugfs_data_out_write write;
		struct mrq_debugfs_data_out_dumpdir dumpdir;
	} data;
} __packed;

static int bpmp_debug_mrq(struct mrq_debugfs_data_in *in,
		struct mrq_debugfs_data_out *out)
{
	return tegra_bpmp_send_receive(MRQ_DEBUGFS,
			in, sizeof(*in), out, sizeof(*out));
}

static int __bpmp_debugfs_write(uint32_t physname, int sizename,
		uint32_t physdata, int sizedata)
{
	struct mrq_debugfs_data_in in;
	struct mrq_debugfs_data_out out;
	int r;

	if (sizename < 0 || sizedata < 0)
		return -EINVAL;

	in.req = cpu_to_le32(MRQ_DEBUGFS_DATA_IN_REQ_WRITE);
	in.data.write.fnameaddr = cpu_to_le32(physname);
	in.data.write.fnamelen = cpu_to_le32(sizename);
	in.data.write.dataaddr = cpu_to_le32(physdata);
	in.data.write.datalen = cpu_to_le32(sizedata);

	r = bpmp_debug_mrq(&in, &out);
	r = r ?: le32_to_cpu(out.data.write.err);
	r = r ?: le32_to_cpu(out.data.write.bytes_written);

	return r;
}

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static int bpmp_debugfs_dumpdir(u32 dma_addr, u32 size)
{
	struct mrq_debugfs_data_in in;
	struct mrq_debugfs_data_out out;
	int r;

	in.req = cpu_to_le32(MRQ_DEBUGFS_DATA_IN_REQ_DUMPDIR);
	in.data.dumpdir.addr = cpu_to_le32(dma_addr);
	in.data.dumpdir.len = cpu_to_le32(size);

	r = bpmp_debug_mrq(&in, &out);
	r = r ?: le32_to_cpu(out.data.write.err);
	r = r ?: le32_to_cpu(out.data.dumpdir.len);

	return r;
}
#endif

struct seqbuf {
	char *buf;
	size_t pos;
	size_t size;
};

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static void seqbuf_init(struct seqbuf *seqbuf, void *buf, size_t size)
{
	seqbuf->buf = buf;
	seqbuf->size = size;
	seqbuf->pos = 0;
}

static size_t seqbuf_avail(struct seqbuf *seqbuf)
{
	return seqbuf->pos < seqbuf->size ? seqbuf->size - seqbuf->pos : 0;
}

static size_t seqbuf_status(struct seqbuf *seqbuf)
{
	return seqbuf->pos <= seqbuf->size ? 0 : -EOVERFLOW;
}

static int seqbuf_eof(struct seqbuf *seqbuf)
{
	return seqbuf->pos >= seqbuf->size;
}

static int seqbuf_read(struct seqbuf *seqbuf, void *buf, size_t nbyte)
{
	nbyte = min(nbyte, seqbuf_avail(seqbuf));
	memcpy(buf, seqbuf->buf + seqbuf->pos,
			min(nbyte, seqbuf_avail(seqbuf)));
	seqbuf->pos += nbyte;
	return seqbuf_status(seqbuf);
}

static int seqbuf_read_u32(struct seqbuf *seqbuf, u32 *v)
{
	int err;
	err = seqbuf_read(seqbuf, v, 4);
	*v = le32_to_cpu(*v);
	return err;
}

static const char *seqbuf_strget(struct seqbuf *seqbuf)
{
	const char *ptr = seqbuf->buf + seqbuf->pos;
	seqbuf->pos += strnlen(seqbuf->buf + seqbuf->pos, seqbuf_avail(seqbuf));
	seqbuf->pos++;

	if (seqbuf_status(seqbuf))
		return NULL;
	else
		return ptr;
}

static int seqbuf_seek(struct seqbuf *seqbuf, ssize_t offset)
{
	seqbuf->pos += offset;
	return seqbuf_status(seqbuf);
}
#endif

static const char *root_path;

static const char *get_filename(const struct file *file, char *buf, int size)
{
	const char *filename;
	size_t root_len = strlen(root_path);

	filename = dentry_path(file->f_dentry, buf, size);
	if (IS_ERR_OR_NULL(filename))
		return NULL;

	if (strlen(filename) < root_len ||
			strncmp(filename, root_path, root_len))
		return NULL;

	filename += root_len;
	return filename;
}

static int __bpmp_debugfs_read(uint32_t physname, int sizename,
		uint32_t physdata, int sizedata)
{
	struct mrq_debugfs_data_in in;
	struct mrq_debugfs_data_out out;
	int r;

	if (sizename < 0 || sizedata < 0)
		return -EINVAL;

	in.req = cpu_to_le32(MRQ_DEBUGFS_DATA_IN_REQ_READ);
	in.data.read.fnameaddr = cpu_to_le32(physname);
	in.data.read.fnamelen = cpu_to_le32(sizename);
	in.data.read.dataaddr = cpu_to_le32(physdata);
	in.data.read.datalen = cpu_to_le32(sizedata);

	r = bpmp_debug_mrq(&in, &out);
	r = r ?: le32_to_cpu(out.data.read.err);
	r = r ?: le32_to_cpu(out.data.read.bytes_read);

	return r;
}

static int bpmp_debugfs_read(const char *name, void *data, int sizedata)
{
	void *datavirt = NULL;
	void *namevirt = NULL;
	dma_addr_t dataphys;
	dma_addr_t namephys;
	int sizename = strlen(name) + 1;
	int ret;

	if (sizedata < 0)
		return -EINVAL;

	if (sizename > SZ_4K)
		return -ENAMETOOLONG;

	datavirt = tegra_bpmp_alloc_coherent(sizedata, &dataphys, GFP_KERNEL);
	if (datavirt == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	namevirt = tegra_bpmp_alloc_coherent(sizename, &namephys, GFP_KERNEL);
	if (namevirt == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	memcpy(namevirt, name, sizename);

	ret = __bpmp_debugfs_read(namephys, sizename, dataphys, sizedata);

	if (ret >= 0) {
		if (ret > sizedata) {
			WARN_ON(1);
			ret = sizedata;
		}
		memcpy(data, datavirt, ret);
	}

out:
	if (datavirt != NULL)
		tegra_bpmp_free_coherent(sizedata, datavirt, dataphys);

	if (namevirt != NULL)
		tegra_bpmp_free_coherent(sizename, namevirt, namephys);

	return ret;
}

static int debugfs_show(struct seq_file *m, void *p)
{
	struct file *file = m->private;
	const size_t datasize = SZ_32K;
	const size_t namesize = SZ_256;
	char *databuf = NULL;
	char *namebuf = NULL;
	const char *filename;
	int ret;

	namebuf = kmalloc(namesize, GFP_KERNEL);
	if (!namebuf)
		return -ENOMEM;

	filename = get_filename(file, namebuf, namesize);
	if (!filename) {
		ret = -ENOENT;
		goto out;
	}

	databuf = kmalloc(datasize, GFP_KERNEL);
	if (!databuf) {
		ret = -ENOMEM;
		goto out;
	}

	ret = bpmp_debugfs_read(filename, databuf, datasize);
	if (ret >= 0)
		ret = seq_write(m, databuf, ret);

out:
	kfree(namebuf);
	kfree(databuf);
	return ret;
}

static int debugfs_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, debugfs_show, file, SZ_32K);
}

static int bpmp_debugfs_write(const char *name, const void *data, int sizedata)
{
	void *datavirt = NULL;
	void *namevirt = NULL;
	dma_addr_t dataphys;
	dma_addr_t namephys;
	int sizename = strlen(name) + 1;
	int ret;

	if (sizedata < 0)
		return -EINVAL;

	if (sizename > SZ_4K)
		return -ENAMETOOLONG;

	datavirt = tegra_bpmp_alloc_coherent(sizedata, &dataphys, GFP_KERNEL);
	if (datavirt == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	namevirt = tegra_bpmp_alloc_coherent(sizename, &namephys, GFP_KERNEL);
	if (namevirt == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	memcpy(namevirt, name, sizename);
	memcpy(datavirt, data, sizedata);

	ret = __bpmp_debugfs_write(namephys, sizename, dataphys, sizedata);

	if (ret >= 0) {
		if (ret > sizedata) {
			WARN_ON(1);
			ret = sizedata;
		}
	}

out:
	if (datavirt != NULL)
		tegra_bpmp_free_coherent(sizedata, datavirt, dataphys);

	if (namevirt != NULL)
		tegra_bpmp_free_coherent(sizename, namevirt, namephys);

	return ret;
}

static ssize_t debugfs_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	const size_t datasize = SZ_4K;
	const size_t namesize = SZ_256;
	char *databuf = NULL;
	char *namebuf = NULL;
	const char *filename;
	ssize_t ret;

	if (count > datasize)
		return -ENOMEM;
	namebuf = kmalloc(namesize, GFP_KERNEL);
	databuf = kmalloc(datasize, GFP_KERNEL);
	if (!namebuf || !databuf) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(databuf, buf, count))
		return -EFAULT;

	filename = get_filename(file, namebuf, namesize);
	if (!filename)
		return -EFAULT;

	ret = bpmp_debugfs_write(filename, databuf, count);
out:
	kfree(namebuf);
	kfree(databuf);
	return ret;
}

static const struct file_operations debugfs_fops = {
	.open		= debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= debugfs_write,
	.release	= single_release,
};

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static int bpmp_populate_dir(struct seqbuf *seqbuf, struct dentry *parent,
		u32 depth)
{
	int err;

	while (!seqbuf_eof(seqbuf)) {
		u32 d, t;
		const char *name;
		struct dentry *dentry;

		seqbuf_read_u32(seqbuf, &d);
		if (d < depth) {
			seqbuf_seek(seqbuf, -4);
			/* go up a level */
			return 0;
		}
		seqbuf_read_u32(seqbuf, &t);
		name = seqbuf_strget(seqbuf);

		if (seqbuf_status(seqbuf))
			return seqbuf_status(seqbuf);

		if (d != depth) {
			/* malformed data received from BPMP */
			return -EIO;
		}

		if (t & DEBUGFS_S_ISDIR) {
			dentry = debugfs_create_dir(name, parent);
			if (IS_ERR_OR_NULL(dentry))
				return dentry ? PTR_ERR(dentry) : -ENOMEM;
			err = bpmp_populate_dir(seqbuf, dentry, depth+1);
			if (err)
				return err;
		} else {
			umode_t mode;
			mode = t & DEBUGFS_S_IRUSR ? S_IRUSR : 0;
			mode |= t & DEBUGFS_S_IWUSR ? S_IWUSR : 0;
			dentry = debugfs_create_file(name, mode,
					parent, NULL,
					&debugfs_fops);
			if (IS_ERR_OR_NULL(dentry))
				return -ENOMEM;
		}
	}

	return 0;
}
#endif

static DEFINE_MUTEX(lock);

#ifndef CONFIG_ARCH_TEGRA_21x_SOC
static struct dentry *bpmp_debugfs_root;
static char root_path_buf[256];

static int bpmp_fwdebug_recreate(void *buf, size_t bufsize, struct dentry *root)
{
	struct seqbuf seqbuf;
	int err;

	mutex_lock(&lock);

	debugfs_remove_recursive(bpmp_debugfs_root);

	bpmp_debugfs_root = debugfs_create_dir("debug", root);
	if (IS_ERR_OR_NULL(bpmp_debugfs_root)) {
		pr_err("failed to create bpmp debugfs directory\n");
		bpmp_debugfs_root = NULL;
		mutex_unlock(&lock);
		return -ENOMEM;
	}

	root_path = dentry_path_raw(bpmp_debugfs_root, root_path_buf,
			sizeof(root_path_buf));
	if (IS_ERR_OR_NULL(root_path)) {
		pr_err("failed to figure out bpmp root path\n");
		err = root_path ? PTR_ERR(root_path) : -ENOENT;
		goto clean;
	}

	seqbuf_init(&seqbuf, buf, bufsize);
	err = bpmp_populate_dir(&seqbuf, bpmp_debugfs_root, 0);
	if (err)
		goto clean;

	mutex_unlock(&lock);
	return 0;

clean:
	debugfs_remove_recursive(bpmp_debugfs_root);
	bpmp_debugfs_root = NULL;
	mutex_unlock(&lock);
	return err;
}
#endif

#ifdef CONFIG_ARCH_TEGRA_21x_SOC
int bpmp_fwdebug_init(struct dentry *root)
{
	return 0;
}
#else
int bpmp_fwdebug_init(struct dentry *root)
{
	dma_addr_t phys;
	void *virt;
	const int sz = SZ_128K;
	int ret;

	if (WARN_ON(!root))
		return -EINVAL;

	virt = tegra_bpmp_alloc_coherent(sz, &phys, GFP_KERNEL);
	if (!virt) {
		pr_err("%s: dma_alloc_coherent() failed\n", __func__);
		return -ENOMEM;
	}

	ret = bpmp_debugfs_dumpdir(phys, sz);
	if (ret < 0) {
		pr_err("bpmp_debugfs_dumpdir() failed (%d)\n", ret);
		goto out;
	}

	ret = bpmp_fwdebug_recreate(virt, ret, root);
	if (ret) {
		pr_err("create_bpmp_debugfs() failed (%d)\n", ret);
		goto out;
	}

	pr_info("bpmp: mounted debugfs mirror\n");

out:
	tegra_bpmp_free_coherent(sz, virt, phys);
	return ret;
}
#endif
