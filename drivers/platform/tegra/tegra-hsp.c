/*
 * Copyright (c) 2014-2016 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#include <linux/tegra-hsp.h>

#define HSP_DIMENSIONING		0x380

#define HSP_DB_REG_TRIGGER		0x0
#define HSP_DB_REG_ENABLE		0x4
#define HSP_DB_REG_RAW			0x8
#define HSP_DB_REG_PENDING		0xc

enum tegra_hsp_init_status {
	HSP_INIT_PENDING,
	HSP_INIT_FAILED,
	HSP_INIT_OKAY,
};

struct hsp_top {
	int status;
	void __iomem *base;
};

static DEFINE_MUTEX(hsp_top_lock);

struct db_handler_info {
	db_handler_t	handler;
	void			*data;
};

static struct hsp_top hsp_top = { .status = HSP_INIT_PENDING };
static void __iomem *db_bases[HSP_NR_DBS];

static DEFINE_MUTEX(db_handlers_lock);
static int db_irq;
static struct db_handler_info db_handlers[HSP_LAST_MASTER + 1];

static const char * const master_names[] = {
	[HSP_MASTER_SECURE_CCPLEX] = "SECURE_CCPLEX",
	[HSP_MASTER_SECURE_DPMU] = "SECURE_DPMU",
	[HSP_MASTER_SECURE_BPMP] = "SECURE_BPMP",
	[HSP_MASTER_SECURE_SPE] = "SECURE_SPE",
	[HSP_MASTER_SECURE_SCE] = "SECURE_SCE",
	[HSP_MASTER_CCPLEX] = "CCPLEX",
	[HSP_MASTER_DPMU] = "DPMU",
	[HSP_MASTER_BPMP] = "BPMP",
	[HSP_MASTER_SPE] = "SPE",
	[HSP_MASTER_SCE] = "SCE",
	[HSP_MASTER_APE] = "APE",
};

static const char * const db_names[] = {
	[HSP_DB_DPMU] = "DPMU",
	[HSP_DB_CCPLEX] = "CCPLEX",
	[HSP_DB_CCPLEX_TZ] = "CCPLEX_TZ",
	[HSP_DB_BPMP] = "BPMP",
	[HSP_DB_SPE] = "SPE",
	[HSP_DB_SCE] = "SCE",
	[HSP_DB_APE] = "APE",
};

static inline int is_master_valid(int master)
{
	return master_names[master] != NULL;
}

static inline int next_valid_master(int m)
{
	for (m++; m <= HSP_LAST_MASTER && !is_master_valid(m); m++)
		;
	return m;
}

#define for_each_valid_master(m) \
	for (m = HSP_FIRST_MASTER; \
		 m <= HSP_LAST_MASTER; \
		 m = next_valid_master(m))

#define hsp_ready() (hsp_top.status == HSP_INIT_OKAY)

static inline u32 hsp_readl(void __iomem *base, int reg)
{
	return readl(base + reg);
}

static inline void hsp_writel(void __iomem *base, int reg, u32 val)
{
	writel(val, base + reg);
	(void)readl(base + reg);
}

static irqreturn_t dbell_irq(int irq, void *data)
{
	ulong reg;
	int master;
	struct db_handler_info *info;

	reg = (ulong)hsp_readl(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_PENDING);
	hsp_writel(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_PENDING, reg);

	for_each_set_bit(master, &reg, HSP_LAST_MASTER + 1) {
		info = &db_handlers[master];
		if (unlikely(!is_master_valid(master))) {
			pr_warn("invalid master from HW.\n");
			continue;
		}
		if (info->handler)
			info->handler(info->data);
	}

	return IRQ_HANDLED;
}

static int tegra_hsp_db_set_master(enum tegra_hsp_master master, bool enabled)
{
	u32 reg;

	if (!hsp_ready() || !is_master_valid(master))
		return -EINVAL;

	mutex_lock(&hsp_top_lock);
	reg = hsp_readl(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_ENABLE);
	if (enabled)
		reg |= BIT(master);
	else
		reg &= ~BIT(master);
	hsp_writel(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_ENABLE, reg);
	mutex_unlock(&hsp_top_lock);
	return 0;
}

/**
 * tegra_hsp_db_enable_master: allow <master> to ring CCPLEX
 * @master:	 HSP master
 *
 * Returns 0 if successful.
 */
int tegra_hsp_db_enable_master(enum tegra_hsp_master master)
{
	return tegra_hsp_db_set_master(master, true);
}
EXPORT_SYMBOL(tegra_hsp_db_enable_master);

/**
 * tegra_hsp_db_disable_master: disallow <master> from ringing CCPLEX
 * @master:	HSP master
 *
 * Returns 0 if successful.
 */
int tegra_hsp_db_disable_master(enum tegra_hsp_master master)
{
	return tegra_hsp_db_set_master(master, false);
}
EXPORT_SYMBOL(tegra_hsp_db_disable_master);

/**
 * tegra_hsp_db_ring: ring the <dbell>
 * @dbell:	 HSP dbell to be rung
 *
 * Returns 0 if successful.
 */
int tegra_hsp_db_ring(enum tegra_hsp_doorbell dbell)
{
	if (!hsp_ready() || dbell >= HSP_NR_DBS)
		return -EINVAL;
	hsp_writel(db_bases[dbell], HSP_DB_REG_TRIGGER, 1);
	return 0;
}
EXPORT_SYMBOL(tegra_hsp_db_ring);

/**
 * tegra_hsp_db_can_ring: check if CCPLEX can ring the <dbell>
 * @dbell:	 HSP dbell to be checked
 *
 * Returns 1 if CCPLEX can ring <dbell> otherwise 0.
 */
int tegra_hsp_db_can_ring(enum tegra_hsp_doorbell dbell)
{
	int reg;
	if (!hsp_ready() || dbell >= HSP_NR_DBS)
		return 0;
	reg = hsp_readl(db_bases[dbell], HSP_DB_REG_ENABLE);
	return !!(reg & BIT(HSP_MASTER_CCPLEX));
}
EXPORT_SYMBOL(tegra_hsp_db_can_ring);

/**
 * tegra_hsp_db_add_handler: register an CCPLEX doorbell IRQ handler
 * @ master:	master id
 * @ handler:	IRQ handler
 * @ data:		custom data
 *
 * Returns 0 if successful.
 */
int tegra_hsp_db_add_handler(int master, db_handler_t handler, void *data)
{
	if (!handler || !is_master_valid(master))
		return -EINVAL;

	if (unlikely(db_irq <= 0))
		return -ENODEV;

	mutex_lock(&db_handlers_lock);
	if (likely(db_handlers[master].handler != NULL)) {
		mutex_unlock(&db_handlers_lock);
		return -EBUSY;
	}

	disable_irq(db_irq);
	db_handlers[master].handler = handler;
	db_handlers[master].data = data;
	enable_irq(db_irq);
	mutex_unlock(&db_handlers_lock);

	return 0;
}
EXPORT_SYMBOL(tegra_hsp_db_add_handler);

/**
 * tegra_hsp_db_del_handler: unregister an CCPLEX doorbell IRQ handler
 * @handler:	IRQ handler
 *
 * Returns 0 if successful.
 */
int tegra_hsp_db_del_handler(int master)
{
	if (!is_master_valid(master))
		return -EINVAL;

	if (unlikely(db_irq <= 0))
		return -ENODEV;

	mutex_lock(&db_handlers_lock);
	WARN_ON(db_handlers[master].handler == NULL);
	disable_irq(db_irq);
	db_handlers[master].handler = NULL;
	db_handlers[master].data = NULL;
	enable_irq(db_irq);
	mutex_unlock(&db_handlers_lock);

	return 0;
}
EXPORT_SYMBOL(tegra_hsp_db_del_handler);

#ifdef CONFIG_DEBUG_FS

static int hsp_dbg_enable_master_show(void *data, u64 *val)
{
	*val = hsp_readl(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_ENABLE);
	return 0;
}

static int hsp_dbg_enable_master_store(void *data, u64 val)
{
	if (!is_master_valid((u32)val))
		return -EINVAL;
	return tegra_hsp_db_enable_master((enum tegra_hsp_master)val);
}

static int hsp_dbg_ring_store(void *data, u64 val)
{
	if (val >= HSP_NR_DBS)
		return -EINVAL;
	return tegra_hsp_db_ring((enum tegra_hsp_doorbell)val);
}

static int hsp_dbg_can_ring_show(void *data, u64 *val)
{
	enum tegra_hsp_doorbell db;
	*val = 0ull;
	for (db = HSP_FIRST_DB; db <= HSP_LAST_DB; db++)
		if (tegra_hsp_db_can_ring(db))
			*val |= BIT(db);
	return 0;
}

/* By convention, CPU shouldn't touch other processors' DBs.
 * So this interface is created for debugging purpose.
 */
static int hsp_dbg_can_ring_store(void *data, u64 val)
{
	int reg;
	enum tegra_hsp_doorbell dbell = (int)val;

	if (!hsp_ready() || dbell >= HSP_NR_DBS)
		return -EINVAL;

	mutex_lock(&hsp_top_lock);
	reg = hsp_readl(db_bases[dbell], HSP_DB_REG_ENABLE);
	reg |= BIT(HSP_MASTER_CCPLEX);
	hsp_writel(db_bases[dbell], HSP_DB_REG_ENABLE, reg);
	mutex_unlock(&hsp_top_lock);
	return 0;
}

static int hsp_dbg_pending_show(void *data, u64 *val)
{
	*val = hsp_readl(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_PENDING);
	return 0;
}

static int hsp_dbg_pending_store(void *data, u64 val)
{
	hsp_writel(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_PENDING, (u32)val);
	return 0;
}

static int hsp_dbg_raw_show(void *data, u64 *val)
{
	*val = hsp_readl(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_RAW);
	return 0;
}

static int hsp_dbg_raw_store(void *data, u64 val)
{
	hsp_writel(db_bases[HSP_DB_CCPLEX], HSP_DB_REG_RAW, (u32)val);
	return 0;
}

static u32 ccplex_intr_count;

static void hsp_dbg_db_handler(void *data)
{
	ccplex_intr_count++;
}

static int hsp_dbg_intr_count_show(void *data, u64 *val)
{
	*val = ccplex_intr_count;
	return 0;
}

static int hsp_dbg_intr_count_store(void *data, u64 val)
{
	ccplex_intr_count = val;
	return 0;
}

static int hsp_dbg_doorbells_show(struct seq_file *s, void *data)
{
	int db;
	seq_printf(s, "%-20s%-10s%-10s\n", "name", "id", "offset");
	seq_printf(s, "--------------------------------------------------\n");
	for (db = HSP_FIRST_DB; db <= HSP_LAST_DB; db++)
		seq_printf(s, "%-20s%-10d%-10lx\n", db_names[db], db,
			(uintptr_t)(db_bases[db] - hsp_top.base));
	return 0;
}

static int hsp_dbg_masters_show(struct seq_file *s, void *data)
{
	int m;
	seq_printf(s, "%-20s%-10s\n", "name", "id");
	seq_printf(s, "----------------------------------------\n");
	for_each_valid_master(m)
		seq_printf(s, "%-20s%-10d\n", master_names[m], m);
	return 0;
}

static int hsp_dbg_handlers_show(struct seq_file *s, void *data)
{
	int m;
	seq_printf(s, "%-20s%-30s\n", "master", "handler");
	seq_printf(s, "--------------------------------------------------\n");
	for_each_valid_master(m)
		seq_printf(s, "%-20s%-30pS\n", master_names[m],
			db_handlers[m].handler);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(enable_master_fops,
	hsp_dbg_enable_master_show, hsp_dbg_enable_master_store, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(ring_fops,
	NULL, hsp_dbg_ring_store, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(can_ring_fops,
	hsp_dbg_can_ring_show, hsp_dbg_can_ring_store, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(pending_fops,
	hsp_dbg_pending_show, hsp_dbg_pending_store, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(raw_fops,
	hsp_dbg_raw_show, hsp_dbg_raw_store, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(intr_count_fops,
	hsp_dbg_intr_count_show, hsp_dbg_intr_count_store, "%lld\n");

#define DEFINE_DBG_OPEN(name) \
static int hsp_dbg_##name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, hsp_dbg_##name##_show, inode->i_private); \
} \
static const struct file_operations name##_fops = { \
	.open = hsp_dbg_##name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
};

DEFINE_DBG_OPEN(doorbells);
DEFINE_DBG_OPEN(masters);
DEFINE_DBG_OPEN(handlers);

struct debugfs_entry {
	const char *name;
	const struct file_operations *fops;
	mode_t mode;
};

static struct debugfs_entry hsp_dbg_attrs[] = {
	{ "enable_master", &enable_master_fops, S_IRUGO | S_IWUSR },
	{ "ring", &ring_fops, S_IWUSR },
	{ "can_ring", &can_ring_fops, S_IRUGO | S_IWUSR },
	{ "pending", &pending_fops, S_IRUGO | S_IWUSR },
	{ "raw", &raw_fops, S_IRUGO | S_IWUSR },
	{ "doorbells", &doorbells_fops, S_IRUGO },
	{ "masters", &masters_fops, S_IRUGO },
	{ "handlers", &handlers_fops, S_IRUGO },
	{ "intr_count", &intr_count_fops, S_IRUGO },
	{ NULL, NULL, 0 }
};

static struct dentry *hsp_debugfs_root;

static int debugfs_init(void)
{
	struct dentry *dent;
	struct debugfs_entry *fent;

	if (!hsp_ready())
		return 0;

	hsp_debugfs_root = debugfs_create_dir("tegra_hsp", NULL);
	if (IS_ERR_OR_NULL(hsp_debugfs_root))
		return -EFAULT;

	fent = hsp_dbg_attrs;
	while (fent->name) {
		dent = debugfs_create_file(fent->name, fent->mode,
			hsp_debugfs_root, NULL, fent->fops);
		if (IS_ERR_OR_NULL(dent))
			goto abort;
		fent++;
	}

	tegra_hsp_db_add_handler(HSP_MASTER_CCPLEX, hsp_dbg_db_handler, NULL);

	return 0;

abort:
	debugfs_remove_recursive(hsp_debugfs_root);
	return -EFAULT;
}
late_initcall(debugfs_init);
#endif

#define NV(prop) "nvidia," prop

int tegra_hsp_init(void)
{
	int i;
	int irq;
	struct device_node *np = NULL;
	void __iomem *base;
	int ret = -EINVAL;
	u32 reg;

	if (hsp_ready())
		return 0;

	/* Look for TOP0 HSP (the one with the doorbells) */
	do {
		np = of_find_compatible_node(np, NULL, NV("tegra186-hsp"));
		if (np == NULL) {
			WARN_ON(1);
			pr_err("tegra-hsp: NV data required.\n");
			return -ENODEV;
		}

		irq = of_irq_get_byname(np, "doorbell");
	} while (irq <= 0);

	base = of_iomap(np, 0);
	if (base == NULL) {
		pr_err("tegra-hsp: failed to map HSP IO space.\n");
		goto out;
	}

	reg = readl(base + HSP_DIMENSIONING);
	hsp_top.base = base;

	base += 0x10000; /* skip common regs */
	base += (reg & 0x000f) << 15; /* skip shared mailboxes */
	base += ((reg & 0x00f0) >> 4) << 16; /* skip shared semaphores */
	base += ((reg & 0x0f00) >> 8) << 16; /* skip arbitrated semaphores */

	for (i = HSP_FIRST_DB; i <= HSP_LAST_DB; i++) {
		db_bases[i] = base;
		base += 0x100;
		pr_debug("tegra-hsp: db[%d]: %p\n", i, db_bases[i]);
	}

	ret = request_irq(irq, dbell_irq, IRQF_NO_SUSPEND, "hsp", NULL);
	if (ret) {
		pr_err("tegra-hsp: request_irq() failed (%d)\n", ret);
		goto out;
	}

	db_irq = irq;
	hsp_top.status = HSP_INIT_OKAY;
	ret = 0;
out:
	of_node_put(np);
	return ret;
}
