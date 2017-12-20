/* compat26/core/core.c
 *
 * Copyright 2016 Codethink Ltd.
*/

#include <linux/kernel.h>
#include <linux/workqueue.h>

#include "../include/compat26.h"

struct workqueue_struct *system_nrt_wq;



static int compat26_init(void)
{
	printk(KERN_INFO "creating 2.6 compatibility driver layer\n");

	system_nrt_wq = create_singlethread_workqueue("compat26-nrt-wq");
	BUG_ON(!system_nrt_wq);

	return 0;
}

subsys_initcall(compat26_init);

#undef set_pte_ext
#define is_pte_inwb(pte) ((pte & __pgprot(L_PTE_MT_MASK)) == L_PTE_MT_INNER_WB)
#define to_pte_lval(ptr) ((pte & __pgprot(L_PTE_MT_MASK)) >> 2)

void debug_cpu_set__pte_ext(pte_t *ptep, pte_t pte, unsigned int ext)
{
	bool debug = is_pte_inwb(pte);

	if (debug)
		pr_info("ptep %p, pte %08x (%d), ext %u\n",
			ptep, pte, to_pte_lval(pte), ext);

	cpu_set_pte_ext(ptep, pte, ext);

	if (debug) {
		unsigned long ptev = ptep[+2048/4];
		pr_info("ptep %p now %08x (linux %08lx, CB=%lu)\n",
			ptep, *ptep, ptev, (ptev >> 2) & 3);
	}
}
