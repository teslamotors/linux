/* compat26/include/compat26.g
 *
 * 2.6 compatbility layer includes
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *	Copyright 2016 Codethink Ltd.
 *
 * Include file for helping build the 2.6 forward-ported items under the 4.x
 * kernel. These are mostly defines and inline functions, as well as a few
 * missing includes.
*/

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) && defined(CONFIG_ARCH_TEGRA_3x_SOC)
#error build cannot currently support building for both tegra2 and tegra3
#endif

/* a few of the files need this, so ensure that we have it included */
#include <linux/module.h>
#include <linux/mm.h>
#include "../include/linux/dma.h"

/* f_dentry has been moved, see Documentation/filesystems/porting */
#define f_dentry f_path.dentry

/* this no longer exists, all wq are non-re-entrant */
#define WQ_NON_REENTRANT	(0)

/* structures needed by change_memory_common */

struct page_change_data {
        pgprot_t set_mask;
        pgprot_t clear_mask;
};

/* things that we no longer have */
#define __devexit
#define __devinit

/* think strtoul these are compatible enough */
#define strict_strtoul kstrtoul
#define strict_strtol kstrtol

/* pr_warn issue */
#define pr_warning_ratelimited pr_warn_ratelimited

#define IRQF_DISABLED	(0)	/* need to fix this */

/* From linux/workqueue.h */

/*
 * system_nrt_wq is non-reentrant and guarantees that any given work
 * item is never executed in parallel by multiple CPUs.  Queue
 * flushing might take relatively long.
 */
extern struct workqueue_struct *system_nrt_wq;

/* From include/linux/fb.h */

/*
 * Stereo modes
 */
#define FB_VMODE_STEREO_NONE        0x00000000  /* not stereo */
#define FB_VMODE_STEREO_FRAME_PACK  0x01000000  /* frame packing */
#define FB_VMODE_STEREO_TOP_BOTTOM  0x02000000  /* top-bottom */
#define FB_VMODE_STEREO_LEFT_RIGHT  0x04000000  /* left-right */
#define FB_VMODE_STEREO_MASK        0xFF000000


/* from arch/arm/include/asm/pgtable.h */

/* this will have to do for the moment... */
//#define pgprot_inner_writeback(prot) pgprot_writecombine(prot)
//#define pgprot_inner_writeback(prot) pgprot_noncached(prot)
#define pgprot_inner_writeback(prog) __pgprot_modify(prot, L_PTE_MT_MASK, L_PTE_MT_INNER_WB)

/* other issues */
#define compat_alloc_vm_area(__sz) alloc_vm_area(__sz, NULL)
#define compat26_alloc_vm_area(__sz) alloc_vm_area(__sz, NULL)

/* set page attributes */
/* see arch/arm/mm/pageattr.c */

extern pte_t *lookup_address(unsigned long address, unsigned int *level);

enum {
        PG_LEVEL_NONE,
        PG_LEVEL_4K,
        PG_LEVEL_2M,
        PG_LEVEL_NUM
};

extern int set_pages_array_wc(struct page **pages, int addrinarray);
extern int set_pages_array_wb(struct page **pages, int addrinarray);
extern int set_pages_array_uc(struct page **pages, int addrinarray);
extern int set_pages_array_iwb(struct page **pages, int addrinarray);

/* think these BDI flags are directly mappable into 4.4 */

#define BDI_RECLAIMABLE WB_RECLAIMABLE
#define BDI_WRITEBACK WB_WRITEBACK

/* not sure how to deal with these two */
#define BDI_CAP_READ_MAP (0)
#define BDI_CAP_WRITE_MAP (0)

/* think we can define this to be the same */
#define oom_adj oom_score_adj

/* VM_RESERVED died in 3.7 */
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)

extern unsigned long tegra_carveout_start;
extern unsigned long tegra_carveout_size;

/* only currently for pdk19 */
/* TODO */
struct page;
enum dma_data_direction;
extern void  __dma_page_cpu_to_dev(struct page *, unsigned long,
				   size_t, enum dma_data_direction);

typedef	int (read_proc_t)(char *page, char **start, off_t off,
			  int count, int *eof, void *data);

extern struct proc_dir_entry *create_proc_read_entry(const char *name,
						     mode_t mode,
						     struct proc_dir_entry *base, 
						     read_proc_t *read_proc,
						     void * data);
