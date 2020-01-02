/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_IRQ_H
#define __ASM_IRQ_H

#ifndef __ASSEMBLER__

#include <asm-generic/irq.h>
#include <linux/cpumask.h>

struct pt_regs;

extern void set_handle_irq(void (*handle_irq)(struct pt_regs *));

static inline int nr_legacy_irqs(void)
{
	return 0;
}

#ifdef CONFIG_SMP
extern void arch_trigger_cpumask_backtrace(const cpumask_t *mask,
					   bool exclude_self);
#define arch_trigger_cpumask_backtrace arch_trigger_cpumask_backtrace
#endif

#endif /* !__ASSEMBLER__ */
#endif
