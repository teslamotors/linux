/*
 * arch/arm64/mach-tegra/nvdumper_regdump.c
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/kdebug.h>
#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <nvdumper.h>

#include <asm/cacheflush.h>

struct mode_regs_t {
	unsigned long sp_svc;
	unsigned long lr_svc;
	unsigned long spsr_svc;
	/* 0x17 */
	unsigned long sp_abt;
	unsigned long lr_abt;
	unsigned long spsr_abt;
	/* 0x1b */
	unsigned long sp_und;
	unsigned long lr_und;
	unsigned long spsr_und;
	/* 0x12 */
	unsigned long sp_irq;
	unsigned long lr_irq;
	unsigned long spsr_irq;
	/* 0x11 */
	unsigned long r8_fiq;
	unsigned long r9_fiq;
	unsigned long r10_fiq;
	unsigned long r11_fiq;
	unsigned long r12_fiq;
	unsigned long sp_fiq;
	unsigned long lr_fiq;
	unsigned long spsr_fiq;
	/* 0x1f: sys/usr */
	unsigned long r8_usr;
	unsigned long r9_usr;
	unsigned long r10_usr;
	unsigned long r11_usr;
	unsigned long r12_usr;
	unsigned long sp_usr;
	unsigned long lr_usr;
};

struct cp15_regs_t {
	unsigned int SCTLR;
	unsigned int TTBR0;
	unsigned int TTBR1;
	unsigned int TTBCR;
	unsigned int DACR;
	unsigned int DFSR;
	unsigned int IFSR;
	unsigned int ADFSR;
	unsigned int AIFSR;
	unsigned int DFAR;
	unsigned int IFAR;
	unsigned int PAR;
	unsigned int TLPLDR;
	unsigned int PRRR;
	unsigned int NRRR;
	unsigned int CONTEXTIDR;
	unsigned int TPIDRURW;
	unsigned int TPIDRURO;
	unsigned int TPIDRPRW;
	unsigned int CFGBASEADDREG;
};

struct nvdumper_cpu_data_t {
	bool is_online;
	struct pt_regs pt_regs;
	struct mode_regs_t mode_regs;
	struct cp15_regs_t cp15_regs;
	struct task_struct *current_task;
};

static struct nvdumper_cpu_data_t *nvdumper_cpu_data;
static int max_cpus;
static dma_addr_t nvdumper_p;

void __naked save_mode_regs(struct mode_regs_t *regs)
{
	asm volatile (
		"mrs    r1, cpsr\n"
		"msr    cpsr_c, #0xd3 @(SVC_MODE | PSR_I_BIT | PSR_F_BIT)\n"
		"stmia  r0!, {r13 - r14}\n"
		"mrs    r2, spsr\n"
		"msr    cpsr_c, #0xd7 @(ABT_MODE | PSR_I_BIT | PSR_F_BIT)\n"
		"stmia  r0!, {r2, r13 - r14}\n"
		"mrs    r2, spsr\n"
		"msr    cpsr_c, #0xdb @(UND_MODE | PSR_I_BIT | PSR_F_BIT)\n"
		"stmia  r0!, {r2, r13 - r14}\n"
		"mrs    r2, spsr\n"
		"msr    cpsr_c, #0xd2 @(IRQ_MODE | PSR_I_BIT | PSR_F_BIT)\n"
		"stmia  r0!, {r2, r13 - r14}\n"
		"mrs    r2, spsr\n"
		"msr    cpsr_c, #0xd1 @(FIQ_MODE | PSR_I_BIT | PSR_F_BIT)\n"
		"stmia  r0!, {r2, r8 - r14}\n"
		"mrs    r2, spsr\n"
		"msr    cpsr_c, #0xdf @(SYS_MODE | PSR_I_BIT | PSR_F_BIT)\n"
		"stmia  r0!, {r2, r8 - r14}\n"
		"msr    cpsr_c, r1\n"
		"bx        lr\n");
}

static void save_cp15_regs(struct cp15_regs_t *cp15_regs)
{
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"        /* SCTLR */
		"str r1, [%0]\n\t"
		"mrc    p15, 0, r1, c2, c0, 0\n\t"        /* TTBR0 */
		"str r1, [%0,#4]\n\t"
		"mrc    p15, 0, r1, c2, c0, 1\n\t"        /* TTBR1 */
		"str r1, [%0,#8]\n\t"
		"mrc    p15, 0, r1, c2, c0, 2\n\t"        /* TTBCR */
		"str r1, [%0,#12]\n\t"
		"mrc    p15, 0, r1, c3, c0, 0\n\t"        /* DACR */
		"str r1, [%0,#16]\n\t"
		"mrc    p15, 0, r1, c5, c0, 0\n\t"        /* DFSR */
		"str r1, [%0,#20]\n\t"
		"mrc    p15, 0, r1, c5, c0, 1\n\t"        /* IFSR */
		"str r1, [%0,#24]\n\t"
		"mrc    p15, 0, r1, c5, c1, 0\n\t"        /* ADFSR */
		"str r1, [%0,#28]\n\t"
		"mrc    p15, 0, r1, c5, c1, 1\n\t"        /* AIFSR */
		"str r1, [%0,#32]\n\t"
		"mrc    p15, 0, r1, c6, c0, 0\n\t"        /* DFAR */
		"str r1, [%0,#36]\n\t"
		"mrc    p15, 0, r1, c6, c0, 2\n\t"        /* IFAR */
		"str r1, [%0,#40]\n\t"
		"mrc    p15, 0, r1, c7, c4, 0\n\t"        /* PAR */
		"str r1, [%0,#44]\n\t"
		"mrc    p15, 0, r1, c10, c2, 0\n\t"        /* PRRR */
		"str r1, [%0,#52]\n\t"
		"mrc    p15, 0, r1, c10, c2, 1\n\t"        /* NRRR */
		"str r1, [%0,#56]\n\t"
		"mrc    p15, 0, r1, c13, c0, 1\n\t"        /* CONTEXTIDR */
		"str r1, [%0,#60]\n\t"
		"mrc    p15, 0, r1, c13, c0, 2\n\t"        /* TPIDRURW */
		"str r1, [%0,#64]\n\t"
		"mrc    p15, 0, r1, c13, c0, 3\n\t"        /* TPIDRURO */
		"str r1, [%0,#68]\n\t"
		"mrc    p15, 0, r1, c13, c0, 4\n\t"        /* TPIDRPRW */
		"str r1, [%0,#72]\n\t"
		"mrc    p15, 4, r1, c15, c0, 0\n\t"        /* CFGBASEADDREG */
		"str r1, [%0,#76]\n\t" :                /* output */
		: "r"(cp15_regs)                        /* input */
		: "%r1", "memory"                        /* clobbered register */
	);
}

static void nvdumper_save_regs(void *data)
{
	int id = smp_processor_id();

	if (!nvdumper_cpu_data) {
		pr_info("nvdumper_cpu_data is not initialized!\n");
		return;
	}

	if (current_thread_info())
		nvdumper_cpu_data[id].current_task =
			current_thread_info()->task;

	nvdumper_cpu_data[id].is_online = true;

	__asm__ __volatile__ (
		"stmia        %[regs_base], {r0-r12}\n\t"
		"mov        %[_ARM_sp], sp\n\t"
		"str        lr, %[_ARM_lr]\n\t"
		"adr        %[_ARM_pc], 1f\n\t"
		"mrs        %[_ARM_cpsr], cpsr\n\t"
		"1:"
		: [_ARM_pc] "=r" (nvdumper_cpu_data[id].pt_regs.ARM_pc),
		[_ARM_cpsr] "=r" (nvdumper_cpu_data[id].pt_regs.ARM_cpsr),
		[_ARM_sp] "=r" (nvdumper_cpu_data[id].pt_regs.ARM_sp),
		[_ARM_lr] "=o" (nvdumper_cpu_data[id].pt_regs.ARM_lr)
		: [regs_base] "r" (&nvdumper_cpu_data[id].pt_regs.ARM_r0)
		: "memory"
	);

	save_mode_regs(&nvdumper_cpu_data[id].mode_regs);

	save_cp15_regs(&nvdumper_cpu_data[id].cp15_regs);

	pr_info("nvdumper: all registers are saved.\n");
}

void nvdumper_crash_setup_regs(void)
{
	pr_info("entering nvdumper_crash_setup_regs\n");
	on_each_cpu(nvdumper_save_regs, NULL, 1);
}

void print_cpu_data(int id)
{
	struct pt_regs *pt_regs = &nvdumper_cpu_data[id].pt_regs;
	struct mode_regs_t *mode_regs = &nvdumper_cpu_data[id].mode_regs;
	struct cp15_regs_t *cp15_regs = &nvdumper_cpu_data[id].cp15_regs;

	pr_info("------------------------------------------------\n");

	pr_info("CPU%d Status: %s\n", id,

	nvdumper_cpu_data[id].is_online ? "online" : "offline");

	pr_info("current task: %p\n", nvdumper_cpu_data[id].current_task);

	if (nvdumper_cpu_data[id].is_online) {
		pr_info("ARM_r0   = 0x%08lx\n", pt_regs->ARM_r0);
		pr_info("ARM_r1   = 0x%08lx\n", pt_regs->ARM_r1);
		pr_info("ARM_r2   = 0x%08lx\n", pt_regs->ARM_r2);
		pr_info("ARM_r3   = 0x%08lx\n", pt_regs->ARM_r3);
		pr_info("ARM_r4   = 0x%08lx\n", pt_regs->ARM_r4);
		pr_info("ARM_r5   = 0x%08lx\n", pt_regs->ARM_r5);
		pr_info("ARM_r6   = 0x%08lx\n", pt_regs->ARM_r6);
		pr_info("ARM_r7   = 0x%08lx\n", pt_regs->ARM_r7);
		pr_info("ARM_r8   = 0x%08lx\n", pt_regs->ARM_r8);
		pr_info("ARM_r9   = 0x%08lx\n", pt_regs->ARM_r9);
		pr_info("ARM_r10  = 0x%08lx\n", pt_regs->ARM_r10);
		pr_info("ARM_fp   = 0x%08lx\n", pt_regs->ARM_fp);
		pr_info("ARM_ip   = 0x%08lx\n", pt_regs->ARM_ip);
		pr_info("ARM_sp   = 0x%08lx\n", pt_regs->ARM_sp);
		pr_info("ARM_lr   = 0x%08lx\n", pt_regs->ARM_lr);
		pr_info("ARM_pc   = 0x%08lx\n", pt_regs->ARM_pc);
		pr_info("ARM_cpsr = 0x%08lx\n", pt_regs->ARM_cpsr);

		pr_info("sp_svc   = 0x%08lx\n", mode_regs->sp_svc);
		pr_info("lr_svc   = 0x%08lx\n", mode_regs->lr_svc);
		pr_info("spsr_svc = 0x%08lx\n", mode_regs->spsr_svc);
		pr_info("sp_abt   = 0x%08lx\n", mode_regs->sp_abt);
		pr_info("lr_abt   = 0x%08lx\n", mode_regs->lr_abt);
		pr_info("spsr_abt = 0x%08lx\n", mode_regs->spsr_abt);
		pr_info("sp_und   = 0x%08lx\n", mode_regs->sp_und);
		pr_info("lr_und   = 0x%08lx\n", mode_regs->lr_und);
		pr_info("spsr_und = 0x%08lx\n", mode_regs->spsr_und);
		pr_info("sp_irq   = 0x%08lx\n", mode_regs->sp_irq);
		pr_info("lr_irq   = 0x%08lx\n", mode_regs->lr_irq);
		pr_info("spsr_irq = 0x%08lx\n", mode_regs->spsr_irq);
		pr_info("r8_fiq   = 0x%08lx\n", mode_regs->r8_fiq);
		pr_info("r9_fiq   = 0x%08lx\n", mode_regs->r9_fiq);
		pr_info("r10_fiq  = 0x%08lx\n", mode_regs->r10_fiq);
		pr_info("r11_fiq  = 0x%08lx\n", mode_regs->r11_fiq);
		pr_info("r12_fiq  = 0x%08lx\n", mode_regs->r12_fiq);
		pr_info("sp_fiq   = 0x%08lx\n", mode_regs->sp_fiq);
		pr_info("lr_fiq   = 0x%08lx\n", mode_regs->lr_fiq);
		pr_info("spsr_fiq = 0x%08lx\n", mode_regs->spsr_fiq);
		pr_info("r8_usr   = 0x%08lx\n", mode_regs->r8_usr);
		pr_info("r9_usr   = 0x%08lx\n", mode_regs->r9_usr);
		pr_info("r10_usr  = 0x%08lx\n", mode_regs->r10_usr);
		pr_info("r11_usr  = 0x%08lx\n", mode_regs->r11_usr);
		pr_info("r12_usr  = 0x%08lx\n", mode_regs->r12_usr);
		pr_info("sp_usr   = 0x%08lx\n", mode_regs->sp_usr);
		pr_info("lr_usr   = 0x%08lx\n", mode_regs->lr_usr);

		pr_info("SCTLR    = 0x%08x\n", cp15_regs->SCTLR);
		pr_info("TTBR0    = 0x%08x\n", cp15_regs->TTBR0);
		pr_info("TTBR1    = 0x%08x\n", cp15_regs->TTBR1);
		pr_info("TTBCR    = 0x%08x\n", cp15_regs->TTBCR);
		pr_info("DACR     = 0x%08x\n", cp15_regs->DACR);
		pr_info("DFSR     = 0x%08x\n", cp15_regs->DFSR);
		pr_info("IFSR     = 0x%08x\n", cp15_regs->IFSR);
		pr_info("ADFSR    = 0x%08x\n", cp15_regs->ADFSR);
		pr_info("AIFSR    = 0x%08x\n", cp15_regs->AIFSR);
		pr_info("DFAR     = 0x%08x\n", cp15_regs->DFAR);
		pr_info("IFAR     = 0x%08x\n", cp15_regs->IFAR);
		pr_info("PAR      = 0x%08x\n", cp15_regs->PAR);
		pr_info("TLPLDR   = 0x%08x\n", cp15_regs->TLPLDR);
		pr_info("PRRR     = 0x%08x\n", cp15_regs->PRRR);
		pr_info("NRRR     = 0x%08x\n", cp15_regs->NRRR);
		pr_info("CONTEXTIDR = 0x%08x\n", cp15_regs->CONTEXTIDR);
		pr_info("TPIDRURW = 0x%08x\n", cp15_regs->TPIDRURW);
		pr_info("TPIDRURO = 0x%08x\n", cp15_regs->TPIDRURO);
		pr_info("TPIDRPRW = 0x%08x\n", cp15_regs->TPIDRPRW);
		pr_info("CFGBASEADDREG = 0x%08x\n", cp15_regs->CFGBASEADDREG);
	}
}

int nvdumper_regdump_init(void)
{
	int ret;

	max_cpus = num_possible_cpus();

	nvdumper_cpu_data = dma_alloc_coherent(NULL,
		sizeof(struct nvdumper_cpu_data_t) * max_cpus,
		&nvdumper_p, 0);
	if (!nvdumper_cpu_data) {
		pr_err("%s: can not allocate bounce buffer\n", __func__);

		return -ENOMEM;
	}

	ret = register_die_notifier(&nvdumper_die_notifier);
	if (ret != 0) {
		pr_err("%s: registering die notifier failed with err=%d\n",
				__func__, ret);
		goto err_out1;
	}

	ret = atomic_notifier_chain_register(&panic_notifier_list,
		&nvdumper_panic_notifier);
	if (ret != 0) {
		pr_err("%s: unable to register a panic notifier (err=%d)\n",
				__func__, ret);
		goto err_out2;
	}

	return ret;

err_out2:
	unregister_die_notifier(&nvdumper_die_notifier);

err_out1:
	if (nvdumper_cpu_data)
		dma_free_coherent(NULL,
				sizeof(struct nvdumper_cpu_data_t) * max_cpus,
				nvdumper_cpu_data, nvdumper_p);

	return ret;
}

void nvdumper_regdump_exit(void)
{
	dma_free_coherent(NULL, sizeof(struct nvdumper_cpu_data_t) * max_cpus,
			nvdumper_cpu_data, nvdumper_p);
	unregister_die_notifier(&nvdumper_die_notifier);
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&nvdumper_panic_notifier);
}
