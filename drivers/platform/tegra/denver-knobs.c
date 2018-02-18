/*
 * Copyright (c) 2013-2015, NVIDIA Corporation. All rights reserved.
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

#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <asm/traps.h>
#include <asm/cputype.h>
#include <asm/cpu.h>

#include <mach/hardware.h>

#include "denver-knobs.h"

#define BG_CLR_SHIFT	16
#define BG_STATUS_SHIFT	32

#define NVG_CHANNEL_PMIC	0

/* 4 instructions to skip upon undef exception */
#define CREGS_NR_JUMP_OFFSET 16

#ifdef CONFIG_DEBUG_FS
struct denver_creg {
	unsigned long offset;
	const char *name;
};

static struct denver_creg denver_cregs[] = {
	{0x00000000, "ifu.mcstatinfo"},
	{0x00000001, "jsr.ret_mcstatinfo"},
	{0x00000002, "jsr.mts_mcstatinfo"},
	{0x00000003, "l2i.mcstatinfo"},
	{0x00000004, "dcc.mcstatinfo1"},
	{0x00000005, "dcc.mcstatinfo2"},
	{0x00000006, "lvb.mcstatinfo3"},
	{0x00000010, "jsr.pending_traps_set"},
	{0x00000011, "jsr.int_sw"},
	{0x0000001e, "dec.pcr_type"},
	{0x0000001f, "dec.pcr_valid"},
	{0x00000020, "dec.pcr_match_0"},
	{0x00000021, "dec.pcr_match_1"},
	{0x00000022, "dec.pcr_match_2"},
	{0x00000023, "dec.pcr_match_3"},
	{0x00000024, "dec.pcr_match_4"},
	{0x00000025, "dec.pcr_match_5"},
	{0x00000026, "dec.pcr_match_6"},
	{0x00000027, "dec.pcr_match_7"},
	{0x00000028, "dec.pcr_mask_0"},
	{0x00000029, "dec.pcr_mask_1"},
	{0x0000002a, "dec.pcr_mask_2"},
	{0x0000002b, "dec.pcr_mask_3"},
	{0x0000002c, "dec.pcr_mask_4"},
	{0x0000002d, "dec.pcr_mask_5"},
	{0x0000002e, "dec.pcr_mask_6"},
	{0x0000002f, "dec.pcr_mask_7"},
	{0x00000030, "mm.tom1"},
	{0x00000031, "mm.tom2"},
	{0x00000032, "mm.tom3"},
	{0x00000033, "mm.tom4"},
	{0x00000034, "mm.bom1"},
	{0x00000035, "mm.bom2"},
	{0x00000036, "mm.bom3"},
	{0x00000037, "mm.bom4"},
	{0x00000038, "mm.cheese"},
	{0x00000040, "mm.dmtrr_base0"},
	{0x00000041, "mm.dmtrr_base1"},
	{0x00000042, "mm.dmtrr_base2"},
	{0x00000043, "mm.dmtrr_base3"},
	{0x00000044, "mm.dmtrr_base4"},
	{0x00000045, "mm.dmtrr_base5"},
	{0x00000046, "mm.dmtrr_base6"},
	{0x00000047, "mm.dmtrr_base7"},
	{0x00000048, "mm.dmtrr_mask0"},
	{0x00000049, "mm.dmtrr_mask1"},
	{0x0000004a, "mm.dmtrr_mask2"},
	{0x0000004b, "mm.dmtrr_mask3"},
	{0x0000004c, "mm.dmtrr_mask4"},
	{0x0000004d, "mm.dmtrr_mask5"},
	{0x0000004e, "mm.dmtrr_mask6"},
	{0x0000004f, "mm.dmtrr_mask7"}
};

#define NR_CREGS (sizeof(denver_cregs) / sizeof(struct denver_creg))

enum creg_command {
	CREG_INDEX = 1,
	CREG_READ,
	CREG_WRITE
};
#endif

static const char * const pmic_names[] = {
	[UNDEFINED] = "none",
	[AMS_372x] = "AMS 3722/3720",
	[TI_TPS_65913_22] = "TI TPS65913 2.2",
	[OPEN_VR] = "Open VR",
	[TI_TPS_65913_23] = "TI TPS65913 2.3",
};

static DEFINE_SPINLOCK(nvg_lock);

int smp_call_function_denver(smp_call_func_t func, void *info, int wait)
{
	int cpu;
	struct cpuinfo_arm64 *cpuinfo;

	for_each_online_cpu(cpu) {
		/* Skip non-Denver CPUs */
		cpuinfo = &per_cpu(cpu_data, cpu);
		if (MIDR_IMPLEMENTOR(cpuinfo->reg_midr) != ARM_CPU_IMP_NVIDIA)
			continue;
		return smp_call_function_single(cpu, func, info, wait);
	}
	/* return ENXIO when there isn't an online Denver CPU to behave the
	   same as smp_call_function_single() when the requested CPU is offline
	 */
	return -ENXIO;
}
EXPORT_SYMBOL_GPL(smp_call_function_denver);

static void _denver_get_bg_allowed(void *ret)
{
	unsigned long regval;

	asm volatile("mrs %0, s3_0_c15_c0_2" : "=r" (regval) : );
	regval = (regval >> BG_STATUS_SHIFT) & 0xffff;

	*((bool *) ret) = !!(regval & (1 << smp_processor_id()));
}

bool denver_get_bg_allowed(int cpu)
{
	bool enabled;

	smp_call_function_single(cpu, _denver_get_bg_allowed,
				 (void *) &enabled, 1);
	return enabled;
}

static void _denver_set_bg_allowed(void *_enable)
{
	unsigned long regval;
	bool enabled = 1;
	bool enable = (bool) _enable;

	regval = 1 << smp_processor_id();
	if (!enable)
		regval <<= BG_CLR_SHIFT;

	asm volatile("msr s3_0_c15_c0_2, %0" : : "r" (regval));

	/* Flush all background work for disable */
	if (!enable)
		while (enabled)
			_denver_get_bg_allowed((void *) &enabled);
}

void denver_set_bg_allowed(int cpu, bool enable)
{
	smp_call_function_single(cpu, _denver_set_bg_allowed,
				 (void *) enable, 1);
}

#ifdef CONFIG_DEBUG_FS

static struct dentry *denver_debugfs_root;

static int bgallowed_get(void *data, u64 *val)
{
	int cpu = (int)((s64)data);
	*val = denver_get_bg_allowed(cpu);
	return 0;
}

static int bgallowed_set(void *data, u64 val)
{
	int cpu = (int)((s64)data);
	denver_set_bg_allowed(cpu, (bool)val);
	pr_debug("CPU%d: BGALLOWED is %s.\n",
			cpu, val ? "enabled" : "disabled");
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(bgallowed_fops, bgallowed_get,
			bgallowed_set, "%llu\n");

static int __init create_denver_bgallowed(void)
{
	int cpu;
	char name[30];

	for_each_present_cpu(cpu) {
		struct cpuinfo_arm64 *cpuinfo = &per_cpu(cpu_data, cpu);

		/* Skip non-denver CPUs */
		if (MIDR_IMPLEMENTOR(cpuinfo->reg_midr) != ARM_CPU_IMP_NVIDIA)
			continue;
		snprintf(name, 30, "bgallowed_cpu%d", cpu);
		if (!debugfs_create_file(
			name, S_IRUGO, denver_debugfs_root,
			(void *)((s64)cpu), &bgallowed_fops)) {
			pr_info("ERROR: failed to create bgallow_fs");
			return -ENOMEM;
		}
	}

	return 0;
}

static struct dentry *denver_creg_root;

struct creg_param {
	u64 offset;
	u64 val;
};

static void _denver_creg_get(struct creg_param *param)
{
	asm volatile (
	"	sys 0, c11, c0, 1, %1\n"
	"	sys 0, c11, c0, 0, %2\n"
	"	sys 0, c11, c0, 0, %3\n"
	"	sysl %0, 0, c11, c0, 0\n"
	: "=r" (param->val)
	: "r" (param->offset), "r" (CREG_INDEX), "r" (CREG_READ)
	);
}

static void _denver_creg_set(struct creg_param *param)
{
	asm volatile (
	"	sys 0, c11, c0, 1, %0\n"
	"	sys 0, c11, c0, 0, %1\n"
	"	sys 0, c11, c0, 1, %2\n"
	"	sys 0, c11, c0, 0, %3\n"
	:
	: "r" (param->offset), "r" (CREG_INDEX),
	  "r" (param->val), "r" (CREG_WRITE)
	);
}

static int denver_creg_get(void *data, u64 *val)
{
	struct creg_param param;
	int ret;
	struct denver_creg *reg = (struct denver_creg *)data;

	param.offset = reg->offset;
	ret = smp_call_function_denver((smp_call_func_t) _denver_creg_get,
				       &param, 1);
	*val = param.val;
	return ret;
}

static int denver_creg_set(void *data, u64 val)
{
	struct creg_param param;
	struct denver_creg *reg = (struct denver_creg *)data;

	pr_debug("CREG: write %s @ 0x%lx =  0x%llx\n",
			reg->name, reg->offset, val);

	param.offset = reg->offset;
	param.val = val;
	return smp_call_function_denver((smp_call_func_t) _denver_creg_set,
					&param, 1);
}

DEFINE_SIMPLE_ATTRIBUTE(denver_creg_fops, denver_creg_get,
			denver_creg_set, "%llu\n");

static int __init create_denver_cregs(void)
{
	int i;
	struct denver_creg *reg;
	int cpu;
	struct cpuinfo_arm64 *cpuinfo;
	int denver_present = false;

	/* Check if any of the present CPUs are Denver */
	for_each_present_cpu(cpu) {
		cpuinfo = &per_cpu(cpu_data, cpu);
		if (MIDR_IMPLEMENTOR(cpuinfo->reg_midr) == ARM_CPU_IMP_NVIDIA) {
			denver_present = true;
			break;
		}
	}
	if (!denver_present)
		return 0;

	denver_creg_root = debugfs_create_dir(
			"cregs", denver_debugfs_root);

	for (i = 0; i < NR_CREGS; ++i) {
		reg = &denver_cregs[i];
		if (!debugfs_create_file(
			reg->name, S_IRUGO, denver_creg_root,
			reg, &denver_creg_fops))
			return -ENOMEM;
	}

	return 0;
}

struct nvmstat {
	u64 stat0;
	u64 pg;
	u64 tot;
	u64 stat1;
	u64 bg;
	u64 fg;
};

static struct nvmstat nvmstat_agg;

static void _get_stats(void *_stat)
{
	u64 stat0;
	struct nvmstat *stat = (struct nvmstat *) _stat;

	/* read nvmstat0 */
	asm volatile("mrs %0, s3_0_c15_c0_0" : "=r" (stat->stat0) : );
	stat->pg = (u32)(stat->stat0);
	nvmstat_agg.pg += stat->pg;
	stat->tot = (u32)(stat->stat0 >> 32);
	nvmstat_agg.tot += stat->tot;

	/* read nvmstat1 */
	asm volatile("mrs %0, s3_0_c15_c0_1" : "=r" (stat->stat1) : );
	stat->bg = (u32)(stat->stat1);
	nvmstat_agg.bg += stat->bg;
	stat->fg = (u32)(stat->stat1 >> 32);
	nvmstat_agg.fg += stat->fg;

	/* reset nvmstat0 */
	stat0 = 0;
	asm volatile("msr s3_0_c15_c0_0, %0" : : "r" (stat0));
}

static int get_stats(struct nvmstat *stats)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct cpuinfo_arm64 *cpuinfo = &per_cpu(cpu_data, cpu);

		/* Skip non-denver CPUs */
		if (MIDR_IMPLEMENTOR(cpuinfo->reg_midr) == ARM_CPU_IMP_NVIDIA) {
			smp_call_function_single(cpu, _get_stats,
						 (void *) stats, 1);
			return 1;
		}
	}
	return 0;
}

static void print_stats(struct seq_file *s, struct nvmstat *stat)
{

	seq_printf(s, "nvmstat0 = %llu\n",  stat->stat0);
	seq_printf(s, "nvmstat0_pg = %llu\n",  stat->pg);
	seq_printf(s, "nvmstat0_tot = %llu\n", stat->tot);
	seq_printf(s, "nvmstat1 = %llu\n",  stat->stat1);
	seq_printf(s, "nvmstat1_bg = %llu\n",  stat->bg);
	seq_printf(s, "nvmstat1_fg = %llu\n",  stat->fg);
}

static int inst_stats_show(struct seq_file *s, void *data)
{
	struct nvmstat stat;

	if (!get_stats(&stat)) {
		seq_puts(s, "No Denver cores online\n");
		return 0;
	}
	print_stats(s, &stat);
	return 0;
}

static int inst_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, inst_stats_show, inode->i_private);
}

static const struct file_operations inst_stats_fops = {
	.open		= inst_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int agg_stats_show(struct seq_file *s, void *data)
{
	struct nvmstat stat;

	if (!get_stats(&stat)) {
		seq_puts(s, "No Denver cores online\n");
		return 0;
	}
	print_stats(s, &nvmstat_agg);
	return 0;
}

static int agg_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, agg_stats_show, inode->i_private);
}

static const struct file_operations agg_stats_fops = {
	.open		= agg_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init create_denver_nvmstats(void)
{
	struct dentry *nvmstats_dir;

	nvmstats_dir = debugfs_create_dir("nvmstats", denver_debugfs_root);
	if (!nvmstats_dir) {
		pr_err("%s: Couldn't create the \"nvmstats\" debugfs node.\n",
			__func__);
		return -1;
	}

	if (!debugfs_create_file("instantaneous_stats",
				S_IRUGO,
				nvmstats_dir,
				NULL,
				&inst_stats_fops)) {
		pr_err("%s: Couldn't create the \"instantaneous_stats\" debugfs node.\n",
			__func__);
		return -1;
	}

	if (!debugfs_create_file("aggregated_stats",
				S_IRUGO,
				nvmstats_dir,
				NULL,
				&agg_stats_fops)) {
		pr_err("%s: Couldn't create the \"aggregated_stats\" debugfs node.\n",
			__func__);
		return -1;
	}

	return 0;
}

static void denver_set_mts_nvgindex(u32 index)
{
	asm volatile("msr s3_0_c15_c1_2, %0" : : "r" (index));
}

static void denver_set_mts_nvgdata(u64 data)
{
	asm volatile("msr s3_0_c15_c1_3, %0" : : "r" (data));
}

static void denver_get_mts_nvgdata(u64 *data)
{
	asm volatile("mrs %0, s3_0_c15_c1_3" : "=r" (*data));
}

int denver_set_pmic_config(enum denver_pmic_type type,
		u16 ret_vol, bool lock)
{
	u64 reg;
	static bool locked;

	if (type >= NR_PMIC_TYPES)
		return -EINVAL;

	if (locked) {
		pr_warn("Denver: PMIC config is locked.\n");
		return -EINVAL;
	}

	spin_lock(&nvg_lock);

	denver_set_mts_nvgindex(NVG_CHANNEL_PMIC);

	/* retention voltage is sanitized by MTS */
	reg = type | (ret_vol << 16) | ((u64)lock << 63);

	denver_set_mts_nvgdata(reg);

	spin_unlock(&nvg_lock);

	locked = lock;

	return 0;
}

int denver_get_pmic_config(enum denver_pmic_type *type,
		u16 *ret_vol, bool *lock)
{
	u64 reg = 0;

	spin_lock(&nvg_lock);

	denver_set_mts_nvgindex(NVG_CHANNEL_PMIC);

	denver_get_mts_nvgdata(&reg);

	spin_unlock(&nvg_lock);

	*type = reg & 0xffff;
	*ret_vol = (reg >> 16) & 0xffff;
	*lock = (reg >> 63);

	return 0;
}

static int __init denver_pmic_init(void)
{
	u32 voltage;
	u32 type;
	u32 lock;
	int err;
	struct device_node *np;

	np = of_find_node_by_path("/denver_cpuidle_pmic");
	if (!np) {
		pr_debug("Denver: using default PMIC setting.\n");
		return 0;
	}

	err = of_property_read_u32(np, "type", &type);
	if (err) {
		pr_err("%s: failed to read PMIC type\n", __func__);
		goto done;
	}
	if (type >= NR_PMIC_TYPES) {
		pr_err("%s: invalid PMIC type: %d\n", __func__, type);
		goto done;
	}

	err = of_property_read_u32(np, "retention-voltage", &voltage);
	if (err) {
		pr_err("%s: failed to read voltage\n", __func__);
		goto done;
	}

	err = of_property_read_u32(np, "lock", &lock);
	if (err) {
		pr_err("%s: failed to read lock\n", __func__);
		goto done;
	}
	if (lock != 0 && lock != 1) {
		pr_err("%s: invalid lock setting [0|1]: read %d\n", __func__, lock);
		goto done;
	}

	err = denver_set_pmic_config(type, (u16)voltage, lock);

	pr_info("Denver: PMIC: type = %s, voltage = %d, locked = %d\n",
			pmic_names[type], voltage, lock);

done:
	return err;
}
arch_initcall(denver_pmic_init);

static bool backdoor_enabled;

bool denver_backdoor_enabled(void)
{
	return backdoor_enabled;
}

/*
 * Handle the invalid instruction exception caused by Denver
 * backdoor accesses on a system where backdoor is disabled.
 */
static int undef_handler(struct pt_regs *regs, unsigned int instr)
{
	backdoor_enabled = false;

	/*
	 * Skip all 4 instructions denver_creg_get(). We are here
	 * because the 1st SYS caused the undef instr exception.
	 */
	regs->pc += CREGS_NR_JUMP_OFFSET;

	return 0;
}

/*
 * We need to handle only the below instruction which is used
 * by a pilot command during init such that no further backdoor
 * will be executed if the pilot trigged an exception.
 *
 *  Instr: sys 0, c11, c0, 1, <x>
 */
static struct undef_hook undef_backdoor_hook = {
	.instr_mask	= 0xffffffe0,
	.instr_val	= 0xd508b020,
	.pstate_mask	= 0x0,	/* ignore */
	.pstate_val	= 0x0, /* ignore */
	.fn		= undef_handler
};

static void check_backdoor(void)
{
	u64 val;

	/* Assume true unless proved otherwise */
	backdoor_enabled = true;

	/* Install hook */
	register_undef_hook(&undef_backdoor_hook);

	/* Pilot command. If this returns an error, it is because it didn't
	   find an online Denver CPU. There cannot be a Denver backdoor without
	   a Denver CPU */
	if (denver_creg_get(&denver_cregs[0], &val))
		 backdoor_enabled = false;

	pr_info("Denver: backdoor interface is %savailable.\n",
	backdoor_enabled ? "" : "NOT ");
}

static u32 mts_version;

static int mts_version_cpu_notify(struct notifier_block *nb,
					 unsigned long action, void *pcpu)
{
	/* Record MTS version if the current CPU is Denver */
	if (!mts_version && ((read_cpuid_id() >> 24) == 'N'))
		asm volatile ("mrs %0, AIDR_EL1" : "=r" (mts_version));
	return NOTIFY_OK;
}

static struct notifier_block mts_version_cpu_nb = {
	.notifier_call = mts_version_cpu_notify,
};

static int __init denver_knobs_init_early(void)
{
	return register_cpu_notifier(&mts_version_cpu_nb);
}
early_initcall(denver_knobs_init_early);

static int mts_version_show(struct seq_file *m, void *v)
{
	return seq_printf(m, "%u\n", mts_version);
}

static int mts_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, mts_version_show, NULL);
}

static const struct file_operations mts_version_fops = {
	.open		= mts_version_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init denver_knobs_init(void)
{
	int error;

	if (tegra_cpu_is_asim())
		return 0;

	denver_debugfs_root = debugfs_create_dir("tegra_denver", NULL);

	check_backdoor();

	/* BGALLOWED/NVMSTATS don't go through the SYS backdoor
	 *  interface such that we can always enable them.
	 */
	error = create_denver_bgallowed();
	if (error)
		return error;

	error = create_denver_nvmstats();
	if (error)
		return error;

	if (backdoor_enabled) {
		error = create_denver_cregs();
		if (error)
			return error;
	}

	/* Cancel the notifier as mts_version should be set now. */
	unregister_cpu_notifier(&mts_version_cpu_nb);

	if (mts_version && !proc_create("mts_version", 0, NULL, &mts_version_fops)) {
		pr_err("Failed to create /proc/mts_version!\n");
		return -1;
	}

	return 0;
}
device_initcall(denver_knobs_init);

#endif
