/**
 * Copyright (c) 2015 - 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/platform/tegra/isomgr.h>
#include <linux/platform/tegra/bwmgr_mc.h>
#include <linux/debugfs.h>

#define IS_HANDLE_VALID(x) ((x >= bwmgr.bwmgr_client) && \
		(x < bwmgr.bwmgr_client + TEGRA_BWMGR_CLIENT_COUNT))

struct tegra_bwmgr_client {
	unsigned long bw;
	unsigned long iso_bw;
	unsigned long cap;
	unsigned long iso_cap;
	unsigned long floor;
	int refcount;
};

/* TODO: Manage client state in a dynamic list */
static struct {
	struct tegra_bwmgr_client bwmgr_client[TEGRA_BWMGR_CLIENT_COUNT];
	struct mutex lock;
	unsigned long emc_min_rate;
	unsigned long emc_max_rate;
	struct clk *emc_clk;
	struct task_struct *task;
	bool status;
} bwmgr;

static u32 clk_update_disabled;

static void bwmgr_debugfs_init(void);

static inline void bwmgr_lock(void)
{
	BUG_ON(bwmgr.task == current); /* disallow rentrance, avoid deadlock */
	mutex_lock(&bwmgr.lock);
	bwmgr.task = current;
}

static inline void bwmgr_unlock(void)
{
	BUG_ON(bwmgr.task != current); /* detect mismatched calls */
	bwmgr.task = NULL;
	mutex_unlock(&bwmgr.lock);
}

/* call with bwmgr lock held except during init*/
static void purge_client(struct tegra_bwmgr_client *handle)
{
	handle->bw = 0;
	handle->iso_bw = 0;
	handle->cap = bwmgr.emc_max_rate;
	handle->iso_cap = bwmgr.emc_max_rate;
	handle->floor = 0;
	handle->refcount = 0;
}

/* call with bwmgr lock held */
static int bwmgr_update_clk(void)
{
	int i;
	unsigned long bw = 0;
	unsigned long iso_bw = 0;
	unsigned long non_iso_cap = bwmgr.emc_max_rate;
	unsigned long iso_cap = bwmgr.emc_max_rate;
	unsigned long floor = 0;
	unsigned long iso_bw_min;
	u64 iso_client_flags = 0;

	/* sizeof(iso_client_flags) */
	BUILD_BUG_ON(TEGRA_BWMGR_CLIENT_COUNT > 64);
	BUG_ON(bwmgr.task != current); /* check that lock is held */

	for (i = 0; i < TEGRA_BWMGR_CLIENT_COUNT; i++) {
		bw += bwmgr.bwmgr_client[i].bw;
		bw = min(bw, bwmgr.emc_max_rate);

		if (bwmgr.bwmgr_client[i].iso_bw > 0) {
			iso_client_flags |= BIT(i);
			iso_bw += bwmgr.bwmgr_client[i].iso_bw;
			iso_bw = min(iso_bw, bwmgr.emc_max_rate);
		}

		non_iso_cap = min(non_iso_cap, bwmgr.bwmgr_client[i].cap);
		iso_cap = min(iso_cap, bwmgr.bwmgr_client[i].iso_cap);
		floor = max(floor, bwmgr.bwmgr_client[i].floor);
	}

	bw += iso_bw;
	bw = max(bw, bwmgr.emc_min_rate);
	bw = min(bw, bwmgr.emc_max_rate);

	bw = bwmgr_apply_efficiency(
			bw, iso_bw, bwmgr.emc_max_rate,
			iso_client_flags, &iso_bw_min);
	iso_bw_min = clk_round_rate(bwmgr.emc_clk, iso_bw_min);

	floor = min(floor, bwmgr.emc_max_rate);
	bw = max(bw, floor);

	bw = min(bw, min(iso_cap, max(non_iso_cap, iso_bw_min)));
	bw = clk_round_rate(bwmgr.emc_clk, bw);

	if (bw == tegra_bwmgr_get_emc_rate())
		return 0;

	return clk_set_rate(bwmgr.emc_clk, bw);
}

struct tegra_bwmgr_client *tegra_bwmgr_register(
		enum tegra_bwmgr_client_id client)
{
	if ((client >= TEGRA_BWMGR_CLIENT_COUNT) || (client < 0)) {
		pr_err("bwmgr: invalid client id %d tried to register",
				client);
		WARN_ON(true);
		return ERR_PTR(-EINVAL);
	}

	bwmgr_lock();
	(bwmgr.bwmgr_client + client)->refcount++;
	bwmgr_unlock();
	return (bwmgr.bwmgr_client + client);
}
EXPORT_SYMBOL_GPL(tegra_bwmgr_register);

void tegra_bwmgr_unregister(struct tegra_bwmgr_client *handle)
{
	if (!IS_HANDLE_VALID(handle)) {
		WARN_ON(true);
		return;
	}

	bwmgr_lock();
	handle->refcount--;

	if (handle->refcount <= 0) {
		if (handle->refcount < 0) {
			pr_err("bwmgr: Mismatched unregister call, client %ld\n",
				handle - bwmgr.bwmgr_client);
			WARN_ON(true);
		}
		purge_client(handle);
	}

	bwmgr_unlock();
}
EXPORT_SYMBOL_GPL(tegra_bwmgr_unregister);

unsigned long tegra_bwmgr_get_emc_rate(void)
{
	if (bwmgr.emc_clk)
		return clk_get_rate(bwmgr.emc_clk);

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_bwmgr_get_emc_rate);

unsigned long tegra_bwmgr_get_max_emc_rate(void)
{
	return bwmgr.emc_max_rate;
}
EXPORT_SYMBOL_GPL(tegra_bwmgr_get_max_emc_rate);

unsigned long tegra_bwmgr_round_rate(unsigned long bw)
{
	if (bwmgr.emc_clk)
		return clk_round_rate(bwmgr.emc_clk, bw);

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_bwmgr_round_rate);

int tegra_bwmgr_set_emc(struct tegra_bwmgr_client *handle, unsigned long val,
		enum tegra_bwmgr_request_type req)
{
	int ret = 0;
	bool update_clk = false;

	if (!bwmgr.emc_clk)
		return 0;

	if (!bwmgr.status)
		return 0;

	if (!IS_HANDLE_VALID(handle)) {
		pr_err("bwmgr: client sent bad handle %p\n",
				handle);
		WARN_ON(true);
		return -EINVAL;
	}

	if (req >= TEGRA_BWMGR_SET_EMC_REQ_COUNT) {
		pr_err("bwmgr: client %ld sent bad request type %d\n",
				handle - bwmgr.bwmgr_client, req);
		WARN_ON(true);
		return -EINVAL;
	}

	bwmgr_lock();

	switch (req) {
	case TEGRA_BWMGR_SET_EMC_FLOOR:
		if (handle->floor != val) {
			handle->floor = val;
			update_clk = true;
		}
		break;

	case TEGRA_BWMGR_SET_EMC_CAP:
		if (val == 0)
			val = bwmgr.emc_max_rate;

		if (handle->cap != val) {
			handle->cap = val;
			update_clk = true;
		}
		break;

	case TEGRA_BWMGR_SET_EMC_ISO_CAP:
		if (val == 0)
			val = bwmgr.emc_max_rate;

		if (handle->iso_cap != val) {
			handle->iso_cap = val;
			update_clk = true;
		}
		break;

	case TEGRA_BWMGR_SET_EMC_SHARED_BW:
		if (handle->bw != val) {
			handle->bw = val;
			update_clk = true;
		}
		break;

	case TEGRA_BWMGR_SET_EMC_SHARED_BW_ISO:
		if (handle->iso_bw != val) {
			handle->iso_bw = val;
			update_clk = true;
		}
		break;

	default:
		WARN_ON(true);
		break;
	}

	if (update_clk && !clk_update_disabled)
		ret = bwmgr_update_clk();

	bwmgr_unlock();

	if (ret)
		pr_err("bwmgr: couldn't set emc clock rate.\n");

	return ret;
}
EXPORT_SYMBOL_GPL(tegra_bwmgr_set_emc);

int tegra_bwmgr_notifier_register(struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;

	if (bwmgr.emc_clk)
		return clk_notifier_register(bwmgr.emc_clk, nb);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(tegra_bwmgr_notifier_register);

int tegra_bwmgr_notifier_unregister(struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;

	if (bwmgr.emc_clk)
		return clk_notifier_unregister(bwmgr.emc_clk, nb);
	else
		return -ENODEV;
}
EXPORT_SYMBOL_GPL(tegra_bwmgr_notifier_unregister);

int __init bwmgr_init(void)
{
	int i;
	struct device_node *dn;
	long round_rate;

	mutex_init(&bwmgr.lock);
	bwmgr_debugfs_init();
	bwmgr_eff_init();

	dn = of_find_compatible_node(NULL, NULL, "nvidia,bwmgr");
	if (dn == NULL) {
		pr_err("bwmgr: dt node not found.\n");
		return -ENODEV;
	}

	bwmgr.emc_clk = of_clk_get(dn, 0);
	if (IS_ERR_OR_NULL(bwmgr.emc_clk)) {
		pr_err("bwmgr: couldn't find emc clock.\n");
		bwmgr.emc_clk = NULL;
		WARN_ON(true);
		return -ENODEV;
	}

	round_rate = clk_round_rate(bwmgr.emc_clk, 0);
	if (round_rate < 0) {
		bwmgr.emc_min_rate = 0;
		pr_err("bwmgr: couldn't get emc clock min rate.\n");
	} else
		bwmgr.emc_min_rate = (unsigned long)round_rate;

	/* Use LONG_MAX as downstream functions treats rate arg as signed */
	round_rate = clk_round_rate(bwmgr.emc_clk, LONG_MAX);
	if (round_rate < 0) {
		bwmgr.emc_max_rate = 0;
		pr_err("bwmgr: couldn't get emc clock max rate.\n");
	} else
		bwmgr.emc_max_rate = (unsigned long)round_rate;

	for (i = 0; i < TEGRA_BWMGR_CLIENT_COUNT; i++)
		purge_client(bwmgr.bwmgr_client + i);

	/* Check status property is okay or not. */
	if (of_device_is_available(dn))
		bwmgr.status = true;
	else
		bwmgr.status = false;

	return 0;
}
subsys_initcall(bwmgr_init);

void __exit bwmgr_exit(void)
{
	int i;

	for (i = 0; i < TEGRA_BWMGR_CLIENT_COUNT; i++)
		purge_client(bwmgr.bwmgr_client + i);

	bwmgr.emc_clk = NULL;
	mutex_destroy(&bwmgr.lock);
}
module_exit(bwmgr_exit);

#if CONFIG_DEBUG_FS
static struct tegra_bwmgr_client *bwmgr_debugfs_client_handle;
static struct dentry *debugfs_dir;
static struct dentry *debugfs_node_floor;
static struct dentry *debugfs_node_cap;
static struct dentry *debugfs_node_iso_cap;
static struct dentry *debugfs_node_bw;
static struct dentry *debugfs_node_iso_bw;
static struct dentry *debugfs_node_emc_rate;
static struct dentry *debugfs_node_emc_min;
static struct dentry *debugfs_node_emc_max;
static struct dentry *debugfs_node_clients_info;

/* keep in sync with tegra_bwmgr_client_id */
static const char * const tegra_bwmgr_client_names[] = {
	"cpu_0",
	"cpu_1",
	"disp_0",
	"disp_1",
	"disp_2",
	"usbd",
	"xhci",
	"sdmmc1",
	"sdmmc2",
	"sdmmc3",
	"sdmmc4",
	"mon",
	"gpu",
	"msenc",
	"nvjpg",
	"nvdec",
	"tsec",
	"tsecb",
	"vi",
	"ispa",
	"ispb",
	"camera",
	"isomgr",
	"thermal",
	"vic",
	"adsp",
	"adma",
	"pcie",
	"bbc_0",
	"eqos",
	"se0",
	"se1",
	"se2",
	"se3",
	"se4",
	"debug",
	"null",
};

static int bwmgr_debugfs_emc_rate_set(void *data, u64 val)
{
	return 0;
}

static int bwmgr_debugfs_emc_rate_get(void *data, u64 *val)
{
	*val = tegra_bwmgr_get_emc_rate();
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_debugfs_emc_rate, bwmgr_debugfs_emc_rate_get,
		bwmgr_debugfs_emc_rate_set, "%llu\n");

static int bwmgr_debugfs_floor_set(void *data, u64 val)
{
	tegra_bwmgr_set_emc(bwmgr_debugfs_client_handle, val,
			TEGRA_BWMGR_SET_EMC_FLOOR);
	return 0;
}

static int bwmgr_debugfs_floor_get(void *data, u64 *val)
{
	*val = bwmgr_debugfs_client_handle->floor;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_debugfs_floor, bwmgr_debugfs_floor_get,
		bwmgr_debugfs_floor_set, "%llu\n");

static int bwmgr_debugfs_cap_set(void *data, u64 val)
{
	tegra_bwmgr_set_emc(bwmgr_debugfs_client_handle, val,
			TEGRA_BWMGR_SET_EMC_CAP);
	return 0;
}

static int bwmgr_debugfs_cap_get(void *data, u64 *val)
{
	*val = bwmgr_debugfs_client_handle->cap;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_debugfs_cap, bwmgr_debugfs_cap_get,
		bwmgr_debugfs_cap_set, "%llu\n");

static int bwmgr_debugfs_iso_cap_set(void *data, u64 val)
{
	tegra_bwmgr_set_emc(bwmgr_debugfs_client_handle, val,
			TEGRA_BWMGR_SET_EMC_ISO_CAP);
	return 0;
}

static int bwmgr_debugfs_iso_cap_get(void *data, u64 *val)
{
	*val = bwmgr_debugfs_client_handle->iso_cap;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_debugfs_iso_cap, bwmgr_debugfs_iso_cap_get,
		bwmgr_debugfs_iso_cap_set, "%llu\n");

static int bwmgr_debugfs_bw_set(void *data, u64 val)
{
	tegra_bwmgr_set_emc(bwmgr_debugfs_client_handle, val,
			TEGRA_BWMGR_SET_EMC_SHARED_BW);
	return 0;
}

static int bwmgr_debugfs_bw_get(void *data, u64 *val)
{
	*val = bwmgr_debugfs_client_handle->bw;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_debugfs_bw, bwmgr_debugfs_bw_get,
		bwmgr_debugfs_bw_set, "%llu\n");

static int bwmgr_debugfs_iso_bw_set(void *data, u64 val)
{
	tegra_bwmgr_set_emc(bwmgr_debugfs_client_handle, val,
			TEGRA_BWMGR_SET_EMC_SHARED_BW_ISO);
	return 0;
}

static int bwmgr_debugfs_iso_bw_get(void *data, u64 *val)
{
	*val = bwmgr_debugfs_client_handle->iso_bw;
	return 0;
}

static int bwmgr_clients_info_show(struct seq_file *s, void *data)
{
	int i;

	seq_printf(s, "%15s%15s%15s%15s%15s%15s (Khz)\n", "Client",
			"Floor", "SharedBw", "SharedIsoBw", "Cap",
			"IsoCap");

	for (i = 0; i < TEGRA_BWMGR_CLIENT_COUNT; i++) {
		seq_printf(s, "%15s%15lu%15lu%15lu%15lu%15lu\n",
				tegra_bwmgr_client_names[i],
				bwmgr.bwmgr_client[i].floor / 1000,
				bwmgr.bwmgr_client[i].bw / 1000,
				bwmgr.bwmgr_client[i].iso_bw / 1000,
				bwmgr.bwmgr_client[i].cap / 1000,
				bwmgr.bwmgr_client[i].iso_cap / 1000);
	}

	return 0;

}
DEFINE_SIMPLE_ATTRIBUTE(fops_debugfs_iso_bw, bwmgr_debugfs_iso_bw_get,
		bwmgr_debugfs_iso_bw_set, "%llu\n");

static int bwmgr_clients_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, bwmgr_clients_info_show, inode->i_private);
}

static const struct file_operations fops_bwmgr_clients_info = {
	.open = bwmgr_clients_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void bwmgr_debugfs_init(void)
{
	bwmgr_debugfs_client_handle =
		tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_DEBUG);

	if (IS_ERR_OR_NULL(bwmgr_debugfs_client_handle)) {
		bwmgr_debugfs_client_handle = NULL;
		pr_err("bwmgr: could not register bwmgr debugfs client\n");
		return;
	}

	debugfs_dir = debugfs_create_dir("tegra_bwmgr", NULL);
	if (debugfs_dir) {
		debugfs_create_bool(
			"clk_update_disabled", S_IRWXU, debugfs_dir,
			&clk_update_disabled);
		debugfs_node_emc_min = debugfs_create_u64(
			"emc_min_rate", S_IRUSR, debugfs_dir,
			(u64 *) &bwmgr.emc_min_rate);
		debugfs_node_emc_max = debugfs_create_u64(
			"emc_max_rate", S_IRUSR, debugfs_dir,
			(u64 *) &bwmgr.emc_max_rate);
		debugfs_node_emc_rate = debugfs_create_file
			("emc_rate", S_IRUSR, debugfs_dir, NULL,
			 &fops_debugfs_emc_rate);
		debugfs_node_floor = debugfs_create_file
			("debug_client_floor", S_IRWXU, debugfs_dir, NULL,
			 &fops_debugfs_floor);
		debugfs_node_cap = debugfs_create_file
			("debug_client_cap", S_IRWXU, debugfs_dir, NULL,
			 &fops_debugfs_cap);
		debugfs_node_iso_cap = debugfs_create_file
			("debug_client_iso_cap", S_IRWXU, debugfs_dir, NULL,
			 &fops_debugfs_iso_cap);
		debugfs_node_bw = debugfs_create_file
			("debug_client_bw", S_IRWXU, debugfs_dir, NULL,
			 &fops_debugfs_bw);
		debugfs_node_iso_bw = debugfs_create_file
			("debug_client_iso_bw", S_IRWXU, debugfs_dir, NULL,
			 &fops_debugfs_iso_bw);
		debugfs_node_clients_info = debugfs_create_file
			("bwmgr_clients_info", S_IRUGO, debugfs_dir, NULL,
			 &fops_bwmgr_clients_info);
	} else
		pr_err("bwmgr: error creating bwmgr debugfs dir.\n");
}

#else
static void bwmgr_debugfs_init(void) {};
#endif /* CONFIG_DEBUG_FS */

