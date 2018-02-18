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
 */

#include <asm/traps.h>
#include <linux/debugfs.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/cpu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/tegra-mce.h>
#include <linux/platform/tegra/ari_mca.h>
#include <linux/platform/tegra/tegra18_a57_mca.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-cpu.h>

static u64 read_cpumerrsr(void)
{
	u64 reg;

	asm volatile("mrs %0, s3_1_c15_c2_2" : "=r" (reg));
	return reg;
}

static void write_cpumerrsr(u64 value)
{
	asm volatile("msr s3_1_c15_c2_2, %0" : "=r" (value));
}

static u64 read_l2merrsr(void)
{
	u64 reg;

	asm volatile("mrs %0, s3_1_c15_c2_3" : "=r" (reg));
	return reg;
}

static void write_l2merrsr(u64 value)
{
	asm volatile ("msr s3_1_c15_c2_3, %0" : "=r" (value));
}

static u64 read_l2ctlr(void)
{
	u64 reg;

	asm volatile("mrs %0, s3_1_c11_c0_2" : "=r" (reg));
	return reg;
}

static int report_serr(int cpu, int uncorrected, int corrected)
{
	u32 error;
	int e;
	u64 data = 0;
	mca_cmd_t mca_cmd = {.cmd = MCA_ARI_CMD_REPORT_SERR,
			     .idx = 0x1,
			     .subidx = cpu};

	mca_cmd.inst = uncorrected ? 0x1 : 0;
	mca_cmd.inst |= corrected ? 0x2 : 0;

	e = tegra_mce_read_uncore_mca(mca_cmd, &data, &error);
	if (e != 0) {
		pr_err("%s: ARI call failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void print_a57_serr(struct seq_file *file, const char *fmt, ...)
{
	va_list args;
	struct va_format vaf;

	va_start(args, fmt);

	if (file) {
		seq_vprintf(file, fmt, args);
	} else {
		vaf.fmt = fmt;
		vaf.va = &args;
		pr_crit("%pV", &vaf);
	}

	va_end(args);
}

static struct a57_ramid ramids[] = {
	{.name = "Instruction L1 Tag RAM", .id = 0x00},
	{.name = "Instruction L1 Data RAM", .id = 0x01},
	{.name = "Data L1 Tag RAM", .id = 0x08},
	{.name = "Data L1 Data RAM", .id = 0x09},
	{.name = "L2 TLB RAM", .id = 0x18},
	{}
};

static struct a57_ramid l2_ramids[] = {
	{.name = "L2 Tag RAM", .id = 0x10},
	{.name = "L2 Data RAM", .id = 0x11},
	{.name = "L2 Snoop Tag RAM", .id = 0x12},
	{.name = "L2 Dirty RAM", .id = 0x14},
	{.name = "L2 Inclusion PF RAM", .id = 0x18},
	{}
};

static void print_ramid(struct seq_file *file, u64 ramid)
{
	u64 i;
	int found = 0;

	for (i = 0; ramids[i].name; i++) {
		if (ramids[i].id == ramid) {
			print_a57_serr(file, "\t%s: 0x%llx\n",
				       ramids[i].name, ramid);
			found = 1;
			break;
		}
	}

	if (!found)
		print_a57_serr(file, "\tUnknown RAMID: 0x%llx\n", ramid);
}

static void print_l2ramid(struct seq_file *file, u64 ramid)
{
	u64 i;
	int found = 0;

	for (i = 0; l2_ramids[i].name; i++) {
		if (l2_ramids[i].id == ramid) {
			print_a57_serr(file, "\t%s: 0x%llx\n",
				       l2_ramids[i].name, ramid);
			found = 1;
			break;
		}
	}

	if (!found)
		print_a57_serr(file, "\tUnknown RAMID: 0x%llx\n", ramid);
}

static void print_a57_cpumerrsr(struct seq_file *file, u64 syndrome)
{
	print_a57_serr(file, "\tCPUMERRSR: 0x%llx\n", syndrome);
	if (syndrome & A57_CPUMERRSR_VALID) {
		if (syndrome & A57_CPUMERRSR_FATAL)
			print_a57_serr(file, "\tFATAL Error\n");
		print_ramid(file, get_a57_cpumerrsr_ramid(syndrome));
		print_a57_serr(file, "\tBank/Way of Error:  0x%llx\n",
			       get_a57_cpumerrsr_bank(syndrome));
		print_a57_serr(file, "\tIndex of Error:     0x%llx\n",
			       get_a57_cpumerrsr_index(syndrome));
		print_a57_serr(file, "\tRepeat Error Count: %llu\n",
			       get_a57_cpumerrsr_repeat(syndrome));
		print_a57_serr(file, "\tOther Error Count:  %llu\n",
			       get_a57_cpumerrsr_other(syndrome));
	}
}

static void print_a57_l2merrsr(struct seq_file *file, u64 syndrome)
{
	print_a57_serr(file, "\tL2MERRSR: 0x%llx\n", syndrome);
	if (syndrome & A57_L2MERRSR_VALID) {
		if (syndrome & A57_L2MERRSR_FATAL)
			print_a57_serr(file, "\tFATAL Error\n");
		print_l2ramid(file, get_a57_l2merrsr_ramid(syndrome));
		print_a57_serr(file, "\tCPU ID of Error:    %llu\n",
			       get_a57_l2merrsr_cpuid(syndrome));
		print_a57_serr(file, "\tWAY of Error:       %llu\n",
			       get_a57_l2merrsr_way(syndrome));
		print_a57_serr(file, "\tIndex of Error:     %llu\n",
			       get_a57_l2merrsr_index(syndrome));
		print_a57_serr(file, "\tRepeat Error Count: %llu\n",
			       get_a57_l2merrsr_repeat(syndrome));
		print_a57_serr(file, "\tOther Error Count:  %llu\n",
			       get_a57_l2merrsr_other(syndrome));
	}
}

static void print_a57_merrsr(struct seq_file *file,
			     u64 cpu_syndrome, u64 l2_syndrome)
{
	if (cpu_syndrome & A57_CPUMERRSR_VALID)
		print_a57_cpumerrsr(file, cpu_syndrome);
	if (l2_syndrome & A57_L2MERRSR_VALID)
		print_a57_l2merrsr(file, l2_syndrome);
}

static DEFINE_RAW_SPINLOCK(a57_mca_lock);

static int a57_serr_hook(struct pt_regs *regs, int reason,
			 unsigned int esr, void *priv)
{
	u64 cpu_syndrome;
	u64 l2_syndrome;
	int cpu;
	int cpuid;
	u64 mpidr;
	int corrected;
	unsigned long flags;
	struct cpuinfo_arm64 *cpuinfo;

	raw_spin_lock_irqsave(&a57_mca_lock, flags);
	cpu = smp_processor_id();
	cpuinfo = &per_cpu(cpu_data, cpu);

	if (MIDR_PARTNUM(cpuinfo->reg_midr) == ARM_CPU_PART_CORTEX_A57) {
		cpu_syndrome = read_cpumerrsr();
		l2_syndrome = read_l2merrsr();
		mpidr = read_cpuid_mpidr();
		cpuid = (int)MCA_ARI_EXTRACT(mpidr, 1, 0);
		if ((cpu_syndrome & A57_CPUMERRSR_VALID)
		    || (l2_syndrome & A57_L2MERRSR_VALID)) {
			pr_crit("**************************************\n");
			pr_crit("A57 CPU Memory Error on CPU %d (A57 Core %d)\n",
				cpu, cpuid);
			pr_crit("\tESR:               0x%x\n", esr);

			print_a57_merrsr(NULL, cpu_syndrome, l2_syndrome);
			pr_crit("**************************************\n");
		}

		if (cpu_syndrome & A57_CPUMERRSR_VALID) {
			if (cpu_syndrome & A57_CPUMERRSR_FATAL) {
				corrected = (get_a57_cpumerrsr_other(cpu_syndrome) != 0) ||
					    (get_a57_cpumerrsr_repeat(cpu_syndrome) != 0);
				report_serr(cpuid,
					    1,
					    corrected);
			}

			write_cpumerrsr(0x0ULL);
		}
		if (l2_syndrome & A57_L2MERRSR_VALID)
			write_l2merrsr(0x0ULL);
	}

	raw_spin_unlock_irqrestore(&a57_mca_lock, flags);
	return 0;
}

static DEFINE_MUTEX(a57_serr_mutex);
static struct dentry *a57_serr_root;

static int a57_merrsr_show(struct seq_file *file, void *data)
{
	u64 cpu_syndrome;
	u64 l2_syndrome;
	int cpu;
	struct cpuinfo_arm64 *cpuinfo;

	mutex_lock(&a57_serr_mutex);

	cpu = smp_processor_id();
	cpuinfo = &per_cpu(cpu_data, cpu);

	if (MIDR_PARTNUM(cpuinfo->reg_midr) == ARM_CPU_PART_CORTEX_A57) {
		cpu_syndrome = read_cpumerrsr();
		l2_syndrome = read_l2merrsr();
		if ((cpu_syndrome & A57_CPUMERRSR_VALID)
		    || (l2_syndrome & A57_L2MERRSR_VALID)) {
			print_a57_merrsr(file, cpu_syndrome, l2_syndrome);
			if (cpu_syndrome & A57_CPUMERRSR_VALID)
				write_cpumerrsr(0x0ULL);
			if (l2_syndrome & A57_L2MERRSR_VALID)
				write_l2merrsr(0x0ULL);
		} else {
			print_a57_serr(file,
				       "CPUMERRSR and L2MERRSR not valid\n");
		}
	} else {
		print_a57_serr(file, "Not running on an A57 CPU\n");
	}

	mutex_unlock(&a57_serr_mutex);
	return 0;
}

static u32 cpu_ecc_errs;
static DEFINE_RAW_SPINLOCK(cpu_ecc_lock);

static void a57_cpu_ecc_err(void *info)
{
	int cpu;
	struct cpuinfo_arm64 *cpuinfo;
	u64 cpu_syndrome;
	u64 l2_syndrome;
	unsigned long flags;

	raw_spin_lock_irqsave(&cpu_ecc_lock, flags);

	cpu = smp_processor_id();
	cpuinfo = &per_cpu(cpu_data, cpu);

	cpu_syndrome = read_cpumerrsr();
	l2_syndrome = read_l2merrsr();
	if (cpu_syndrome & A57_CPUMERRSR_VALID) {
		if ((cpu_syndrome & A57_CPUMERRSR_FATAL)
		    || (get_a57_cpumerrsr_repeat(cpu_syndrome) != 0ULL)
		    || (get_a57_cpumerrsr_other(cpu_syndrome) != 0ULL))
			cpu_ecc_errs |= 1U << cpu;

		write_cpumerrsr(0x0ULL);
	}

	if (l2_syndrome & A57_L2MERRSR_VALID) {
		if ((l2_syndrome & A57_L2MERRSR_FATAL)
		    || (get_a57_l2merrsr_repeat(l2_syndrome) != 0ULL)
		    || (get_a57_l2merrsr_other(l2_syndrome) != 0ULL))
			cpu_ecc_errs |= 1U << cpu;

		write_l2merrsr(0x0ULL);
	}

	raw_spin_unlock_irqrestore(&cpu_ecc_lock, flags);
}

static int a57_ecc_show(struct seq_file *file, void *data)
{
	int cpu;
	struct cpuinfo_arm64 *cpuinfo;

	cpu_ecc_errs = 0;

	for_each_online_cpu(cpu) {
		cpuinfo = &per_cpu(cpu_data, cpu);
		if (MIDR_PARTNUM(cpuinfo->reg_midr) == ARM_CPU_PART_CORTEX_A57) {
			smp_call_function_single(cpu, a57_cpu_ecc_err,
						 NULL, true);
		}
	}

	print_a57_serr(file, "0x%08x\n", cpu_ecc_errs);

	return 0;
}

static int a57_merrsr_open(struct inode *inode, struct file *file)
{
	return single_open(file, a57_merrsr_show, inode->i_private);
}

static const struct file_operations tegra18_a57_merrsr_fops = {
	.open = a57_merrsr_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int a57_ecc_open(struct inode *inode, struct file *file)
{
	return single_open(file, a57_ecc_show, inode->i_private);
}

static const struct file_operations tegra18_a57_ecc_fops = {
	.open = a57_ecc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int a57_serr_dbgfs_init(void)
{
	struct dentry *d;

	d = debugfs_create_dir("a57-serr", NULL);
	if (IS_ERR_OR_NULL(d)) {
		pr_err("%s: could not create 'a57-serr' node\n", __func__);
		goto clean;
	}

	a57_serr_root = d;

	d = debugfs_create_file("a57-merrsr",
				S_IRUGO, a57_serr_root,
				NULL,
				&tegra18_a57_merrsr_fops);
	if (IS_ERR_OR_NULL(d)) {
		pr_err("%s: could not create '%s' node\n",
		       __func__, "a57-merrsr");
		goto clean;
	}

	d = debugfs_create_file("a57-ecc",
				S_IRUGO, a57_serr_root,
				NULL,
				&tegra18_a57_ecc_fops);
	if (IS_ERR_OR_NULL(d)) {
		pr_err("%s: could not create '%s' node\n",
		       __func__, "a57-ecc");
		goto clean;
	}

	return 0;

  clean:
	debugfs_remove_recursive(a57_serr_root);
	return PTR_ERR(d);
}

static struct serr_hook hook = {
	.fn = a57_serr_hook
};

static void a57_ecc_print_info(void *data)
{
	u32 ecc_settings;
	int cpu = smp_processor_id();
	struct cpuinfo_arm64 *cpuinfo = &per_cpu(cpu_data, cpu);
	char *core_type = "Denver";

	if (MIDR_PARTNUM(cpuinfo->reg_midr) == ARM_CPU_PART_CORTEX_A57) {
		ecc_settings = read_l2ctlr();
		pr_info("**** A57 ECC: %s\n",
			(ecc_settings & A57_L2CTLR_ECC_EN) ? "Enabled" :
			"Disabled");
		core_type = "A57";
	}
	pr_info("%s: on CPU %d a %s Core\n", __func__, cpu, core_type);
}

static int __init tegra18_a57_serr_init(void)
{
	/*
	 * No point in registering an ECC error handler on the
	 * simulator.
	 */
	if (tegra_cpu_is_asim())
		return 0;

	on_each_cpu(a57_ecc_print_info, NULL, 1);

	register_serr_hook(&hook);

	return a57_serr_dbgfs_init();
}
module_init(tegra18_a57_serr_init);

static void __exit tegra18_a57_serr_exit(void)
{
	unregister_serr_hook(&hook);
}
module_exit(tegra18_a57_serr_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tegra A57 SError handler");
