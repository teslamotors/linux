/*
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#include <linux/uaccess.h>
#include <soc/tegra/tegra_bpmp.h>
#include <linux/tegra-firmwares.h>
#include "../../../arch/arm/mach-tegra/iomap.h"
#include "bpmp.h"

#define BPMP_MODULE_MAGIC		0x646f6d
#define SHARED_SIZE			512

static struct device *device;
static void *shared_virt;
static uint32_t shared_phys;

static char firmware_tag[32];
static DEFINE_SPINLOCK(shared_lock);
#ifdef CONFIG_DEBUG_FS
static struct dentry *bpmp_root;
static struct dentry *module_root;
static LIST_HEAD(modules);
DEFINE_MUTEX(bpmp_lock);

struct bpmp_module {
	struct list_head entry;
	char name[MODULE_NAME_LEN];
	struct dentry *root;
	u32 handle;
	u32 size;
};

struct module_hdr {
	u32 magic;
	u32 size;
	u32 reloc_size;
	u32 bss_size;
	u32 init_offset;
	u32 cleanup_offset;
	u8 reserved[72];
	u8 parent_tag[32];
};

int bpmp_create_attrs(const struct fops_entry *fent,
		struct dentry *parent, void *data)
{
	struct dentry *d;

	while (fent->name) {
		d = debugfs_create_file(fent->name, fent->mode, parent, data,
				fent->fops);
		if (IS_ERR_OR_NULL(d))
			return -EFAULT;
		fent++;
	}

	return 0;
}

static struct bpmp_module *bpmp_find_module(const char *name)
{
	struct bpmp_module *m;

	list_for_each_entry(m, &modules, entry) {
		if (!strncmp(m->name, name, MODULE_NAME_LEN))
			return m;
	}

	return NULL;
}

static int bpmp_module_load(struct device *dev, const void *base, u32 size,
		u32 *handle)
{
	void *virt;
	dma_addr_t phys;
	struct { u32 phys; u32 size; } __packed msg;
	int r;

	virt = tegra_bpmp_alloc_coherent(size, &phys, GFP_KERNEL);
	if (virt == NULL)
		return -ENOMEM;

	memcpy(virt, base, size);

	msg.phys = phys;
	msg.size = size;

	r = tegra_bpmp_send_receive(MRQ_MODULE_LOAD, &msg, sizeof(msg),
			handle, sizeof(*handle));

	tegra_bpmp_free_coherent(size, virt, phys);
	return r;
}

static int bpmp_module_unload(struct device *dev, u32 handle)
{
	return tegra_bpmp_send_receive(MRQ_MODULE_UNLOAD,
			&handle, sizeof(handle), NULL, 0);
}

static ssize_t bpmp_module_unload_store(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bpmp_module *m;
	char buf[MODULE_NAME_LEN];
	char *name;
	int r;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (strncpy_from_user(buf, user_buf, count) <= 0)
		return -EFAULT;

	buf[count] = 0;
	name = strim(buf);

	mutex_lock(&bpmp_lock);

	m = bpmp_find_module(name);
	if (!m) {
		r = -ENODEV;
		goto clean;
	}

	r = bpmp_module_unload(device, m->handle);
	if (r) {
		dev_err(device, "%s: failed to unload module (%d)\n", name, r);
		goto clean;
	}

	debugfs_remove_recursive(m->root);
	list_del(&m->entry);
	kfree(m);

clean:
	mutex_unlock(&bpmp_lock);
	return r ?: count;
}

static const struct file_operations bpmp_module_unload_fops = {
	.write = bpmp_module_unload_store
};

static int bpmp_module_ready(const char *name, const struct firmware *fw,
		struct bpmp_module *m)
{
	struct module_hdr *hdr;
	const int sz = sizeof(firmware_tag);
	char fmt[sz + 1];
	int err;

	hdr = (struct module_hdr *)fw->data;

	if (fw->size < sizeof(struct module_hdr) ||
			hdr->magic != BPMP_MODULE_MAGIC ||
			hdr->size + hdr->reloc_size != fw->size) {
		dev_err(device, "%s: invalid module format\n", name);
		return -EINVAL;
	}

	if (memcmp(hdr->parent_tag, firmware_tag, sz)) {
		dev_err(device, "%s: bad module - tag mismatch\n", name);
		memcpy(fmt, firmware_tag, sz);
		fmt[sz] = 0;
		dev_err(device, "firmware: %s\n", fmt);
		memcpy(fmt, hdr->parent_tag, sz);
		fmt[sz] = 0;
		dev_err(device, "%s : %s\n", name, fmt);
		return -EINVAL;
	}

	m->size = hdr->size + hdr->bss_size;

	err = bpmp_module_load(device, fw->data, fw->size, &m->handle);
	if (err) {
		dev_err(device, "failed to load module, code=%d\n", err);
		return err;
	}

	if (!debugfs_create_x32("handle", S_IRUGO, m->root, &m->handle))
		return -ENOMEM;

	if (!debugfs_create_x32("size", S_IRUGO, m->root, &m->size))
		return -ENOMEM;

	list_add_tail(&m->entry, &modules);

	return 0;
}

static ssize_t bpmp_module_load_store(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	const struct firmware *fw;
	struct bpmp_module *m;
	char buf[MODULE_NAME_LEN];
	int r;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (strncpy_from_user(buf, user_buf, count) <= 0)
		return -EFAULT;

	buf[count] = 0;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (m == NULL)
		return -ENOMEM;

	mutex_lock(&bpmp_lock);

	strcpy(m->name, strim(buf));
	if (bpmp_find_module(m->name)) {
		dev_err(device, "module %s exist\n", m->name);
		r = -EEXIST;
		goto clean;
	}

	m->root = debugfs_create_dir(m->name, module_root);
	if (!m->root) {
		r = -ENOMEM;
		goto clean;
	}

	r = request_firmware(&fw, m->name, device);
	if (r) {
		dev_err(device, "request_firmware() failed: %d\n", r);
		goto clean;
	}

	if (!fw) {
		r = -EFAULT;
		WARN_ON(0);
		goto clean;
	}

	dev_info(device, "%s: module ready %zu@%p\n",
			m->name, fw->size, fw->data);
	r = bpmp_module_ready(m->name, fw, m);
	release_firmware(fw);

clean:
	mutex_unlock(&bpmp_lock);

	if (r) {
		debugfs_remove_recursive(m->root);
		kfree(m);
		return r;
	}

	return count;
}

static const struct file_operations bpmp_module_load_fops = {
	.write = bpmp_module_load_store
};

void bpmp_cleanup_modules(void)
{
	debugfs_remove_recursive(module_root);

	while (!list_empty(&modules)) {
		struct bpmp_module *m = list_first_entry(&modules,
				struct bpmp_module,
				entry);
		list_del(&m->entry);
		kfree(m);
	}
}

int bpmp_init_modules(struct platform_device *pdev)
{
	struct dentry *d;

	module_root = debugfs_create_dir("module", bpmp_root);
	if (IS_ERR_OR_NULL(module_root))
		goto clean;

	d = debugfs_create_file("load", S_IWUSR, module_root, pdev,
			&bpmp_module_load_fops);
	if (IS_ERR_OR_NULL(d))
		goto clean;

	d = debugfs_create_file("unload", S_IWUSR, module_root, pdev,
			&bpmp_module_unload_fops);
	if (IS_ERR_OR_NULL(d))
		goto clean;

	return 0;

clean:
	WARN_ON(1);
	debugfs_remove_recursive(module_root);
	module_root = NULL;
	return -EFAULT;
}

int __bpmp_do_ping(void)
{
	int ret;
	int challenge = 1;
	int reply;

	ret = tegra_bpmp_send_receive(MRQ_PING,
			&challenge, sizeof(challenge), &reply, sizeof(reply));
	if (ret)
		return ret;

	if (reply != challenge * 2)
		return -EINVAL;

	return 0;
}

static int bpmp_ping_show(void *data, u64 *val)
{
	ktime_t tm;
	int ret;

	spin_lock(&shared_lock);
	tm = ktime_get();
	ret = __bpmp_do_ping();
	tm = ktime_sub(ktime_get(), tm);
	spin_unlock(&shared_lock);

	*val = ret ?: ktime_to_us(tm);
	return 0;
}

static int bpmp_modify_trace_mask(uint32_t clr, uint32_t set)
{
	uint32_t mb[] = { clr, set };
	uint32_t new;
	return tegra_bpmp_send_receive(MRQ_TRACE_MODIFY, mb, sizeof(mb),
			&new, sizeof(new)) ?: new;
}

static int bpmp_trace_enable_show(void *data, u64 *val)
{
	*val = bpmp_modify_trace_mask(0, 0);
	return 0;
}

static int bpmp_trace_enable_store(void *data, u64 val)
{
	return bpmp_modify_trace_mask(0, val);
}

static int bpmp_trace_disable_store(void *data, u64 val)
{
	return bpmp_modify_trace_mask(val, 0);
}

static int bpmp_mount_show(void *data, u64 *val)
{
	*val = bpmp_fwdebug_init(bpmp_root);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(bpmp_ping_fops, bpmp_ping_show, NULL, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(trace_enable_fops, bpmp_trace_enable_show,
		bpmp_trace_enable_store, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(trace_disable_fops, NULL,
		bpmp_trace_disable_store, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(bpmp_mount_fops, bpmp_mount_show, NULL, "%lld\n");

static __init int bpmp_init_mount(void)
{
	return bpmp_fwdebug_init(bpmp_root);
}
late_initcall(bpmp_init_mount);

struct bpmp_trace_iter {
	dma_addr_t phys;
	void *virt;
	int eof;
};

static int bpmp_trace_show(struct seq_file *file, void *v)
{
	struct bpmp_trace_iter *i = file->private;
	uint32_t mb[] = { i->phys, PAGE_SIZE };
	int ret;

	i->eof = 0;
	ret = tegra_bpmp_send_receive(MRQ_WRITE_TRACE, mb, sizeof(mb),
			&i->eof, sizeof(i->eof));
	pr_debug("%s: ret %d eof %d\n", __func__, ret, i->eof);
	if (ret < 0)
		return ret;

	ret = seq_write(file, i->virt, ret);
	if (ret < 0)
		return ret;

	return 0;
}

static void *bpmp_trace_start(struct seq_file *file, loff_t *pos)
{
	struct bpmp_trace_iter *i = file->private;
	uint32_t cmd;
	int first;
	int ret;

	first = (*pos == 0);
	if (first) {
		cmd = 0;
		ret = tegra_bpmp_send_receive(MRQ_TRACE_ITER,
				&cmd, sizeof(cmd), NULL, 0);
		if (WARN_ON(ret && ret != -EINVAL))
			return NULL;
	}

	pr_debug("%s: first %d eof %d pos %llu\n",
			__func__, first, i->eof, *pos);
	if (!first && i->eof == 1)
		return NULL;

	i->virt = tegra_bpmp_alloc_coherent(PAGE_SIZE, &i->phys, GFP_KERNEL);
	if (!i->virt)
		return NULL;

	return i;
}

static void *bpmp_trace_next(struct seq_file *file, void *v, loff_t *pos)
{
	struct bpmp_trace_iter *i = file->private;

	pr_debug("%s: eof %d pos %llu\n", __func__, i->eof, *pos);
	return NULL;
}

static void bpmp_trace_stop(struct seq_file *file, void *v)
{
	struct bpmp_trace_iter *i = file->private;

	pr_debug("%s: eof %d\n", __func__, i->eof);
	if (i->virt) {
		tegra_bpmp_free_coherent(PAGE_SIZE, i->virt, i->phys);
		i->virt = NULL;
	}
}

static const struct seq_operations bpmp_trace_seq_ops = {
	.start = bpmp_trace_start,
	.show = bpmp_trace_show,
	.next = bpmp_trace_next,
	.stop = bpmp_trace_stop
};

static ssize_t bpmp_trace_store(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	uint32_t cmd = 1;
	int ret;

	ret = tegra_bpmp_send_receive(MRQ_TRACE_ITER,
			&cmd, sizeof(cmd), NULL, 0);
	return ret ?: count;
}

static int bpmp_trace_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file, &bpmp_trace_seq_ops,
				sizeof(struct bpmp_trace_iter));
}

static const struct file_operations trace_fops = {
	.open = bpmp_trace_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = bpmp_trace_store,
	.release = seq_release_private,
};

static int bpmp_tag_show(struct seq_file *file, void *data)
{
	seq_write(file, firmware_tag, sizeof(firmware_tag));
	seq_putc(file, '\n');
	return 0;
}

static int bpmp_tag_open(struct inode *inode, struct file *file)
{
	return single_open(file, bpmp_tag_show, inode->i_private);
}

static const struct file_operations bpmp_tag_fops = {
	.open = bpmp_tag_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

#define MSG_NR_FIELDS	((MSG_DATA_SZ + 3) / 4)
#define MSG_DATA_COUNT	(MSG_NR_FIELDS + 1)

static uint32_t inbox_data[MSG_DATA_COUNT];

static ssize_t bpmp_mrq_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	/* size in dec, space, new line, terminator */
	char buf[MSG_DATA_COUNT * 11 + 1 + 1];
	uint32_t outbox_data[MSG_DATA_COUNT];
	char *line;
	char *p;
	int i;
	int ret;

	memset(outbox_data, 0, sizeof(outbox_data));
	memset(inbox_data, 0, sizeof(inbox_data));

	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto complete;
	}

	buf[count] = 0;
	line = strim(buf);


	for (i = 0; i < MSG_DATA_COUNT && line; i++) {
		p = strsep(&line, " ");
		ret = kstrtouint(p, 0, outbox_data + i);
		if (ret)
			break;
	}

	if (!i) {
		ret = -EINVAL;
		goto complete;
	}

	ret = tegra_bpmp_send_receive(outbox_data[0], outbox_data + 1,
			MSG_DATA_SZ, inbox_data + 1, MSG_DATA_SZ);

complete:
	inbox_data[0] = ret;
	return ret ?: count;
}

static int bpmp_mrq_show(struct seq_file *file, void *data)
{
	int i;
	for (i = 0; i < MSG_DATA_COUNT; i++) {
		seq_printf(file, "0x%x%s", inbox_data[i],
				i == MSG_DATA_COUNT - 1 ? "\n" : " ");
	}

	return 0;
}

static int bpmp_mrq_open(struct inode *inode, struct file *file)
{
	return single_open(file, bpmp_mrq_show, inode->i_private);
}

static const struct file_operations bpmp_mrq_fops = {
	.open = bpmp_mrq_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.write = bpmp_mrq_write,
	.release = single_release
};

static const struct fops_entry root_attrs[] = {
	{ "ping", &bpmp_ping_fops, S_IRUGO },
	{ "trace_enable", &trace_enable_fops, S_IRUGO | S_IWUSR },
	{ "trace_disable", &trace_disable_fops, S_IWUSR },
	{ "trace", &trace_fops, S_IRUGO | S_IWUSR },
	{ "tag", &bpmp_tag_fops, S_IRUGO },
	{ "mrq", &bpmp_mrq_fops, S_IRUGO | S_IWUSR },
	{ "mount", &bpmp_mount_fops, S_IRUGO },
	{ NULL, NULL, 0 }
};

static int bpmp_init_debug(struct platform_device *pdev)
{
	struct dentry *root;

	root = debugfs_create_dir("bpmp", NULL);
	if (IS_ERR_OR_NULL(root))
		goto clean;

	if (bpmp_create_attrs(root_attrs, root, pdev))
		goto clean;

	if (bpmp_init_cpuidle_debug(root))
		goto clean;

	if (bpmp_platdbg_init(root, pdev))
		goto clean;

	bpmp_root = root;
	return 0;

clean:
	WARN_ON(1);
	debugfs_remove_recursive(root);
	return -EFAULT;
}
#else
static inline int bpmp_init_debug(struct platform_device *pdev) { return 0; }
int bpmp_init_modules(struct platform_device *pdev) { return 0; }
int __bpmp_do_ping(void) { return 0; }
#endif

int bpmp_get_fwtag(void)
{
	int r;
	size_t sz = sizeof(firmware_tag);
	char fmt[sz + 1];

	spin_lock(&shared_lock);
	r = tegra_bpmp_send_receive(MRQ_QUERY_TAG,
			&shared_phys, sizeof(shared_phys), NULL, 0);
	if (!r) {
		memcpy(firmware_tag, shared_virt, sz);
		memcpy(fmt, firmware_tag, sz);
		fmt[sz] = 0;
		dev_info(device, "firmware tag is %s\n", fmt);
	}
	spin_unlock(&shared_lock);

	return r;
}

static ssize_t bpmp_version(struct device *dev, char *data, size_t size)
{
	return snprintf(data, size, "firmware tag %*.*s",
		 (int)sizeof(firmware_tag), (int)sizeof(firmware_tag),
		 firmware_tag);
}


void *__weak bpmp_get_virt_for_alloc(void *virt, dma_addr_t phys)
{
	return virt;
}

void *tegra_bpmp_alloc_coherent(size_t size, dma_addr_t *phys,
		gfp_t flags)
{
	void *virt;

	if (!device)
		return NULL;

	virt = dma_alloc_coherent(device, size, phys,
			flags);

	virt = bpmp_get_virt_for_alloc(virt, *phys);

	return virt;
}

void *__weak bpmp_get_virt_for_free(void *virt, dma_addr_t phys)
{
	return virt;
}

void tegra_bpmp_free_coherent(size_t size, void *vaddr,
		dma_addr_t phys)
{
	if (!device) {
		pr_err("device not found\n");
		return;
	}

	vaddr = bpmp_get_virt_for_free(vaddr, phys);

	dma_free_coherent(device, size, vaddr, phys);
}

static int bpmp_mem_init(void)
{
	dma_addr_t phys;

	shared_virt = tegra_bpmp_alloc_coherent(SHARED_SIZE, &phys,
			GFP_KERNEL);
	if (!shared_virt)
		return -ENOMEM;

	shared_phys = phys;
	return 0;
}

static struct syscore_ops bpmp_syscore_ops = {
	.resume = tegra_bpmp_resume,
};

void __weak bpmp_setup_allocator(struct device *dev)
{
}

static int bpmp_probe(struct platform_device *pdev)
{
	int r = 0;

	bpmp_setup_allocator(&pdev->dev);

	/*tegra_bpmp_alloc_coherent can be called as soon as "device" is set up
	 */
	device = &pdev->dev;

	r = bpmp_linear_map_init(device);
	r = r ?: bpmp_mem_init();
	r = r ?: bpmp_clk_init(pdev);
	r = r ?: bpmp_init_debug(pdev);
	r = r ?: bpmp_init_modules(pdev);
	r = r ?: bpmp_mail_init();
	r = r ?: bpmp_get_fwtag();
	r = r ?: of_platform_populate(device->of_node, NULL, NULL, device);

	register_syscore_ops(&bpmp_syscore_ops);

	if (r == 0)
		devm_tegrafw_register(device, "bpmp",
			TFW_NORMAL, bpmp_version, NULL);
	return r;
}

static const struct of_device_id bpmp_of_matches[] = {
	{ .compatible = "nvidia,tegra186-bpmp" },
	{ .compatible = "nvidia,tegra186-bpmp-hv" },
	{ .compatible = "nvidia,tegra210-bpmp" },
	{}
};

static struct platform_driver bpmp_driver = {
	.probe = bpmp_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "bpmp",
		.of_match_table = of_match_ptr(bpmp_of_matches)
	}
};

static __init int bpmp_init(void)
{
	return platform_driver_register(&bpmp_driver);
}
core_initcall(bpmp_init);
