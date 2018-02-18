/*
 * Based on arch/arm/mm/mmu.c
 *
 * Copyright (C) 1995-2005 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/mman.h>
#include <linux/nodemask.h>
#include <linux/memblock.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/stop_machine.h>

#include <asm/cputype.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/sizes.h>
#include <asm/tlb.h>
#include <asm/memblock.h>
#include <asm/mmu_context.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "mm.h"

/*
 * Empty_zero_page is a special page that is used for zero-initialized data
 * and COW.
 */
unsigned long empty_zero_page;
EXPORT_SYMBOL(empty_zero_page);
unsigned long zero_page_mask;
static int zero_page_order = 3;

struct cachepolicy {
	const char	policy[16];
	u64		mair;
	u64		tcr;
};

static struct cachepolicy cache_policies[] __initdata = {
	{
		.policy		= "uncached",
		.mair		= 0x44,			/* inner, outer non-cacheable */
		.tcr		= TCR_IRGN_NC | TCR_ORGN_NC,
	}, {
		.policy		= "writethrough",
		.mair		= 0xaa,			/* inner, outer write-through, read-allocate */
		.tcr		= TCR_IRGN_WT | TCR_ORGN_WT,
	}, {
		.policy		= "writeback",
		.mair		= 0xee,			/* inner, outer write-back, read-allocate */
		.tcr		= TCR_IRGN_WBnWA | TCR_ORGN_WBnWA,
	}
};

/*
 * These are useful for identifying cache coherency problems by allowing the
 * cache or the cache and writebuffer to be turned off. It changes the Normal
 * memory caching attributes in the MAIR_EL1 register.
 */
static int __init early_cachepolicy(char *p)
{
	int i;
	u64 tmp;

	for (i = 0; i < ARRAY_SIZE(cache_policies); i++) {
		int len = strlen(cache_policies[i].policy);

		if (memcmp(p, cache_policies[i].policy, len) == 0)
			break;
	}
	if (i == ARRAY_SIZE(cache_policies)) {
		pr_err("ERROR: unknown or unsupported cache policy: %s\n", p);
		return 0;
	}

	flush_cache_all();

	/*
	 * Modify MT_NORMAL attributes in MAIR_EL1.
	 */
	asm volatile(
	"	mrs	%0, mair_el1\n"
	"	bfi	%0, %1, %2, #8\n"
	"	msr	mair_el1, %0\n"
	"	isb\n"
	: "=&r" (tmp)
	: "r" (cache_policies[i].mair), "i" (MT_NORMAL * 8));

	/*
	 * Modify TCR PTW cacheability attributes.
	 */
	asm volatile(
	"	mrs	%0, tcr_el1\n"
	"	bic	%0, %0, %2\n"
	"	orr	%0, %0, %1\n"
	"	msr	tcr_el1, %0\n"
	"	isb\n"
	: "=&r" (tmp)
	: "r" (cache_policies[i].tcr), "r" (TCR_IRGN_MASK | TCR_ORGN_MASK));

	flush_cache_all();

	return 0;
}
early_param("cachepolicy", early_cachepolicy);

pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
			      unsigned long size, pgprot_t vma_prot)
{
	if (!pfn_valid(pfn))
		return pgprot_noncached(vma_prot);
	else if (file->f_flags & O_SYNC)
		return pgprot_writecombine(vma_prot);
	return vma_prot;
}
EXPORT_SYMBOL(phys_mem_access_prot);

void __init *early_alloc(unsigned long sz)
{
	void *ptr = __va(memblock_alloc(sz, sz));
	BUG_ON(!ptr);
	memset(ptr, 0, sz);
	return ptr;
}

/*
 * remap a PMD into pages
 */
static void split_pmd(pmd_t *pmd, pte_t *pte)
{
	unsigned long pfn = pmd_pfn(*pmd);
	int i = 0;

	do {
		/*
		 * Need to have the least restrictive permissions available
		 * permissions will be fixed up later
		 */
		set_pte(pte, pfn_pte(pfn, PAGE_KERNEL_EXEC));
		pfn++;
	} while (pte++, i++, i < PTRS_PER_PTE);
}

static void alloc_init_pte(pmd_t *pmd, unsigned long addr,
				  unsigned long end, unsigned long pfn,
				  pgprot_t prot,
				  void *(*alloc)(unsigned long size))
{
	pte_t *pte;

	if (pmd_none(*pmd) || pmd_bad(*pmd) ||
	    (config_enabled(CONFIG_DEBUG_PAGEALLOC) && pmd_sect(*pmd))) {
		pte = alloc(PTRS_PER_PTE * sizeof(pte_t));
		if (pmd_sect(*pmd))
			split_pmd(pmd, pte);
		__pmd_populate(pmd, __pa(pte), PMD_TYPE_TABLE);
		flush_tlb_all();
	}

	pte = pte_offset_kernel(pmd, addr);
	do {
		set_pte(pte, pfn_pte(pfn, prot));
		pfn++;
	} while (pte++, addr += PAGE_SIZE, addr != end);
}

void split_pud(pud_t *old_pud, pmd_t *pmd)
{
	unsigned long addr = pud_pfn(*old_pud) << PAGE_SHIFT;
	pgprot_t prot = __pgprot(pud_val(*old_pud) ^ addr);
	int i = 0;

	do {
		set_pmd(pmd, __pmd(addr | prot));
		addr += PMD_SIZE;
	} while (pmd++, i++, i < PTRS_PER_PMD);
}

static void alloc_init_pmd(pud_t *pud, unsigned long addr,
			   unsigned long end, phys_addr_t phys,
			   unsigned int type,
			   void *(*alloc)(unsigned long size))
{
	pmd_t *pmd;
	unsigned long next;
	pmdval_t prot_sect;
	pgprot_t prot_pte;

	switch(type) {
	case MT_NORMAL_NC:
		prot_sect = PROT_SECT_NORMAL_NC;
		prot_pte = PROT_NORMAL_NC;
		break;
	case MT_DEVICE_nGnRE:
		prot_sect = PROT_SECT_DEVICE_nGnRE;
		prot_pte = __pgprot(PROT_DEVICE_nGnRE);
		break;
	case MT_MEMORY_KERNEL_EXEC:
		prot_sect = PROT_SECT_NORMAL_EXEC;
		prot_pte = PAGE_KERNEL_EXEC;
		break;
	case MT_MEMORY_KERNEL_EXEC_RDONLY:
		prot_sect = PROT_SECT_NORMAL_EXEC | PMD_SECT_RDONLY;
		prot_pte = PAGE_KERNEL_EXEC | PTE_RDONLY;
		break;
	case MT_MEMORY_KERNEL:
		prot_sect = PROT_SECT_NORMAL;
		prot_pte = PAGE_KERNEL;
		break;
	default:
		BUG();
	}

	/*
	 * Check for initial section mappings in the pgd/pud and remove them.
	 */
	if (pud_none(*pud) || pud_bad(*pud) ||
	    (config_enabled(CONFIG_DEBUG_PAGEALLOC) && pud_sect(*pud))) {
		pmd = alloc(PTRS_PER_PMD * sizeof(pmd_t));
		if (pud_sect(*pud)) {
			/*
			 * need to have the 1G of mappings continue to be
			 * present
			 */
			split_pud(pud, pmd);
		}
		pud_populate(&init_mm, pud, pmd);
		flush_tlb_all();
	}

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		/* try section mapping first */
		if (((addr | next | phys) & ~SECTION_MASK) == 0) {
			pmd_t old_pmd =*pmd;
			set_pmd(pmd, __pmd(phys | prot_sect));
			/*
			 * Check for previous table entries created during
			 * boot (__create_page_tables) and flush them.
			 */
			if (!pmd_none(old_pmd))
				flush_tlb_all();
#ifdef CONFIG_DEBUG_PAGEALLOC
			goto splitpmd;
#endif
		} else {
#ifdef CONFIG_DEBUG_PAGEALLOC
splitpmd:
#endif
			alloc_init_pte(pmd, addr, next, __phys_to_pfn(phys),
				       prot_pte, alloc);
		}
		phys += next - addr;
	} while (pmd++, addr = next, addr != end);
}

static inline bool use_1G_block(unsigned long addr, unsigned long next,
			unsigned long phys)
{
	if (PAGE_SHIFT != 12)
		return false;

	if (((addr | next | phys) & ~PUD_MASK) != 0)
		return false;

	return true;
}

static void alloc_init_pud(pgd_t *pgd, unsigned long addr,
			   unsigned long end, phys_addr_t phys,
			   unsigned int type,
			   void *(*alloc)(unsigned long size))
{
	pud_t *pud;
	unsigned long next;
	pudval_t prot_pud;

	switch(type) {
	case MT_NORMAL_NC:
		prot_pud = PROT_SECT_NORMAL_NC;
		break;
	case MT_DEVICE_nGnRE:
		prot_pud = PROT_SECT_DEVICE_nGnRE;
		break;
	case MT_MEMORY_KERNEL_EXEC:
		prot_pud = PROT_SECT_NORMAL_EXEC;
		break;
	case MT_MEMORY_KERNEL_EXEC_RDONLY:
		prot_pud = PROT_SECT_NORMAL_EXEC | PMD_SECT_RDONLY;
		break;
	case MT_MEMORY_KERNEL:
		prot_pud = PROT_SECT_NORMAL;
		break;
	default:
		BUG();
	}


	if (pgd_none(*pgd)) {
		pud = alloc(PTRS_PER_PUD * sizeof(pud_t));
		pgd_populate(&init_mm, pgd, pud);
	}
	BUG_ON(pgd_bad(*pgd));

	pud = pud_offset(pgd, addr);
	do {
		next = pud_addr_end(addr, end);

		/*
		 * For 4K granule only, attempt to put down a 1GB block
		 */
		if (use_1G_block(addr, next, phys)) {
			pud_t old_pud = *pud;
			set_pud(pud, __pud(phys | prot_pud));

			/*
			 * If we have an old value for a pud, it will
			 * be pointing to a pmd table that we no longer
			 * need (from swapper_pg_dir).
			 *
			 * Look up the old pmd table and free it.
			 */
			if (!pud_none(old_pud)) {
				phys_addr_t table = __pa(pmd_offset(&old_pud, 0));
				memblock_free(table, PAGE_SIZE);
				flush_tlb_all();
			}
#ifdef CONFIG_DEBUG_PAGEALLOC
			goto splitpud;
#endif
		} else {
#ifdef CONFIG_DEBUG_PAGEALLOC
splitpud:
#endif
			alloc_init_pmd(pud, addr, next, phys, type, alloc);
		}
		phys += next - addr;
	} while (pud++, addr = next, addr != end);
}

/*
 * Create the page directory entries and any necessary page tables for the
 * mapping specified by 'md'.
 */
static void __create_mapping(pgd_t *pgd, phys_addr_t phys,
			     unsigned long virt, phys_addr_t size,
			     unsigned int type,
			     void *(*alloc)(unsigned long size))
{
	unsigned long addr, length, end, next;

	addr = virt & PAGE_MASK;
	length = PAGE_ALIGN(size + (virt & ~PAGE_MASK));

	end = addr + length;
	do {
		next = pgd_addr_end(addr, end);
		alloc_init_pud(pgd, addr, next, phys, type, alloc);
		phys += next - addr;
	} while (pgd++, addr = next, addr != end);
}

static void *late_alloc(unsigned long size)
{
	void *ptr;

	BUG_ON(size > PAGE_SIZE);
	ptr = (void *)__get_free_page(PGALLOC_GFP);
	BUG_ON(!ptr);
	return ptr;
}

static void __init _create_mapping(phys_addr_t phys, unsigned long virt,
				   phys_addr_t size, unsigned int type)
{
	if (virt < VMALLOC_START) {
		pr_warn("BUG: not creating mapping for %pa at 0x%016lx - outside kernel range\n",
			&phys, virt);
		return;
	}

	return __create_mapping(pgd_offset_k(virt & PAGE_MASK),
				phys, virt, size, type, early_alloc);
}

static void __init create_mapping(struct map_desc *io_desc)
{
	unsigned long virt = io_desc->virtual;
	unsigned long phys = __pfn_to_phys(io_desc->pfn);

	_create_mapping(phys, virt, io_desc->length, io_desc->type);
}

void __init create_id_mapping(phys_addr_t addr, phys_addr_t size, int map_io)
{
	if ((addr >> PGDIR_SHIFT) >= ARRAY_SIZE(idmap_pg_dir)) {
		pr_warn("BUG: not creating id mapping for %pa\n", &addr);
		return;
	}
	__create_mapping(&idmap_pg_dir[pgd_index(addr)],
			 addr, addr, size,
			 map_io ? MT_DEVICE_nGnRE : MT_MEMORY_KERNEL_EXEC,
			 early_alloc);
}

/* To support old-style static device memory mapping. */
__init void iotable_init(struct map_desc *io_desc, int nr)
{
	struct map_desc *md;
	struct vm_struct *vm;

	vm = early_alloc(sizeof(*vm) * nr);

	for (md = io_desc; nr; md++, nr--) {
		create_mapping(md);
		vm->addr = (void *)(md->virtual & PAGE_MASK);
		vm->size = PAGE_ALIGN(md->length + (md->virtual & ~PAGE_MASK));
		vm->phys_addr = __pfn_to_phys(md->pfn);
		vm->flags = VM_IOREMAP;
		vm->caller = iotable_init;
		vm_area_add_early(vm++);
	}
}

__init void iotable_init_va(struct map_desc *io_desc, int nr)
{
	struct map_desc *md;
	struct vm_struct *vm;

	vm = early_alloc(sizeof(*vm) * nr);

	for (md = io_desc; nr; md++, nr--) {
		vm->addr = (void *)(md->virtual & PAGE_MASK);
		vm->size = PAGE_ALIGN(md->length + (md->virtual & ~PAGE_MASK));
		vm->phys_addr = __pfn_to_phys(md->pfn);
		vm->flags = VM_IOREMAP;
		vm->caller = iotable_init;
		vm_area_add_early(vm++);
	}
}

__init void iotable_init_mapping(struct map_desc *io_desc, int nr)
{
	struct map_desc *md;

	for (md = io_desc; nr; md++, nr--)
		create_mapping(md);
}

static void create_mapping_late(phys_addr_t phys, unsigned long virt,
				  phys_addr_t size, unsigned int type)
{
	if (virt < VMALLOC_START) {
		pr_warn("BUG: not creating mapping for %pa at 0x%016lx - outside kernel range\n",
			&phys, virt);
		return;
	}

	return __create_mapping(pgd_offset_k(virt & PAGE_MASK),
				phys, virt, size, type, late_alloc);
}

#ifdef CONFIG_DEBUG_RODATA
static void __init __map_memblock(phys_addr_t start, phys_addr_t end)
{
	/*
	 * Set up the executable regions using the existing section mappings
	 * for now. This will get more fine grained later once all memory
	 * is mapped
	 */
	unsigned long kernel_x_start = round_down(__pa(_stext), SECTION_SIZE);
	unsigned long kernel_x_end = round_up(__pa(__init_end), SECTION_SIZE);

	if (end < kernel_x_start) {
		_create_mapping(start, __phys_to_virt(start),
			end - start, MT_MEMORY_KERNEL);
	} else if (start >= kernel_x_end) {
		_create_mapping(start, __phys_to_virt(start),
			end - start, MT_MEMORY_KERNEL);
	} else {
		if (start < kernel_x_start)
			_create_mapping(start, __phys_to_virt(start),
				kernel_x_start - start,
				MT_MEMORY_KERNEL);
		_create_mapping(kernel_x_start,
				__phys_to_virt(kernel_x_start),
				kernel_x_end - kernel_x_start,
				MT_MEMORY_KERNEL_EXEC);
		if (kernel_x_end < end)
			_create_mapping(kernel_x_end,
				__phys_to_virt(kernel_x_end),
				end - kernel_x_end,
				MT_MEMORY_KERNEL);
	}

}
#else
static void __init __map_memblock(phys_addr_t start, phys_addr_t end)
{
	_create_mapping(start, __phys_to_virt(start), end - start,
			MT_MEMORY_KERNEL_EXEC);
}
#endif

static void __init map_mem(void)
{
	struct memblock_region *reg;
	phys_addr_t limit;

	/*
	 * Temporarily limit the memblock range. We need to do this as
	 * create_mapping requires puds, pmds and ptes to be allocated from
	 * memory addressable from the initial direct kernel mapping.
	 *
	 * The initial direct kernel mapping, located at swapper_pg_dir, gives
	 * us PUD_SIZE (4K pages) or PMD_SIZE (64K pages) memory starting from
	 * PHYS_OFFSET (which must be aligned to 2MB as per
	 * Documentation/arm64/booting.txt).
	 */
	if (IS_ENABLED(CONFIG_ARM64_64K_PAGES))
		limit = PHYS_OFFSET + PMD_SIZE;
	else {
		if (PUD_SIZE > SZ_512M)
			limit = PHYS_OFFSET + SZ_512M;
		else
			limit = PHYS_OFFSET + PUD_SIZE;
	}
	memblock_set_current_limit(limit);

	/* map all the memory banks */
	for_each_memblock(memory, reg) {
		phys_addr_t start = reg->base;
		phys_addr_t end = start + reg->size;

		if (start >= end)
			break;

#ifndef CONFIG_ARM64_64K_PAGES
		/*
		 * For the first memory bank align the start address and
		 * current memblock limit to prevent create_mapping() from
		 * allocating pte page tables from unmapped memory.
		 * When 64K pages are enabled, the pte page table for the
		 * first PGDIR_SIZE is already present in swapper_pg_dir.
		 */
		if (start < limit)
			start = ALIGN(start, PMD_SIZE);
		if (end < limit) {
			limit = end & PMD_MASK;
			memblock_set_current_limit(limit);
		}
#endif
		__map_memblock(start, end);
	}

	/* Limit no longer required. */
	memblock_set_current_limit(MEMBLOCK_ALLOC_ANYWHERE);
}

void __init fixup_executable(void)
{
#ifdef CONFIG_DEBUG_RODATA
	/* now that we are actually fully mapped, make the start/end more fine grained */
	if (!IS_ALIGNED((unsigned long)_stext, SECTION_SIZE)) {
		unsigned long aligned_start = round_down(__pa(_stext),
							SECTION_SIZE);

		_create_mapping(aligned_start, __phys_to_virt(aligned_start),
				__pa(_stext) - aligned_start,
				MT_MEMORY_KERNEL);
	}

	if (!IS_ALIGNED((unsigned long)__init_end, SECTION_SIZE)) {
		unsigned long aligned_end = round_up(__pa(__init_end),
							SECTION_SIZE);
		_create_mapping(__pa(__init_end), (unsigned long)__init_end,
				aligned_end - __pa(__init_end),
				MT_MEMORY_KERNEL);
	}
#endif
}

#ifdef CONFIG_DEBUG_RODATA
#ifdef CONFIG_DEBUG_RODATA_TEST
static const int rodata_test_data = 0xC3;

static noinline void rodata_test(void)
{
	int result;

	pr_info("%s: attempting to write to read-only section:\n", __func__);

	if (*(volatile int *)&rodata_test_data != 0xC3) {
		pr_err("read only data changed before test\n");
		return;
	}

	/*
	 * Attempt to write to rodata_test_data, trappint the expected
	 * data abort. If the trap executer, result will be 1. If it didn't,
	 * result will be oxFF.
	 */
	asm volatile(
		"0:	str	%[zero], [%[rodata_test_data]]\n"
		"	mov	%[result], #0xFF\n"
		"	b 	2f\n"
		"1:	mov	%[result], #1\n"
		"2:\n"

		/* Exception fixup - if store at label 0 faults, jumps to 1 */
		".pushsection __ex_table,\"a\"\n"
		"	.quad 0b, 1b\n"
		".popsection\n"

		: [result] "=r" (result)
		: [rodata_test_data] "r" (&rodata_test_data), [zero] "r" (0)
		: "memory"
	);

	if (result == 1)
		pr_info("write to read-only section trapped, success\n");
	else
		pr_err("write to read-only section NOT trapped, test failed\n");

	if (*(volatile int *)&rodata_test_data != 0xC3)
		pr_err("read only data changed during write\n");
}
#else
static inline void rodata_test(void) { }
#endif

void mark_rodata_ro(void)
{
	create_mapping_late(__pa(_stext), (unsigned long)_stext,
				(unsigned long)_etext - (unsigned long)_stext,
				MT_MEMORY_KERNEL_EXEC_RDONLY);

	rodata_test();
}
#endif

void fixup_init(void)
{
	create_mapping_late(__pa(__init_begin), (unsigned long)__init_begin,
			(unsigned long)__init_end - (unsigned long)__init_begin,
			MT_MEMORY_KERNEL);
}

/*
 * paging_init() sets up the page tables, initialises the zone memory
 * maps and sets up the zero page.
 */
void __init paging_init(void)
{
	map_mem();
	fixup_executable();

#ifdef CONFIG_ARM64_MACH_FRAMEWORK
	/*
	 * Ask the machine support to map in the statically mapped devices.
	 */
	if (machine_desc->map_io)
		machine_desc->map_io();
#endif

	/*
	 * Finally flush the caches and tlb to ensure that we're in a
	 * consistent state.
	 */
	flush_cache_all();
	flush_tlb_all();

	/* allocate the zero page. */
	empty_zero_page = (ulong)early_alloc(PAGE_SIZE << zero_page_order);
	zero_page_mask = ((PAGE_SIZE << zero_page_order) - 1) & PAGE_MASK;

	bootmem_init();


	dma_contiguous_remap();
	/*
	 * TTBR0 is only used for the identity mapping at this stage. Make it
	 * point to zero page to avoid speculatively fetching new entries.
	 */
	cpu_set_reserved_ttbr0();
	flush_tlb_all();
}

/*
 * Enable the identity mapping to allow the MMU disabling.
 */
void setup_mm_for_reboot(void)
{
	cpu_switch_mm(idmap_pg_dir, &init_mm);
	flush_tlb_all();
}

/*
 * Check whether a kernel address is valid (derived from arch/x86/).
 */
int kern_addr_valid(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if ((((long)addr) >> VA_BITS) != -1UL)
		return 0;

	pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd))
		return 0;

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud))
		return 0;

	if (pud_sect(*pud))
		return pfn_valid(pud_pfn(*pud));

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return 0;

	if (pmd_sect(*pmd))
		return pfn_valid(pmd_pfn(*pmd));

	pte = pte_offset_kernel(pmd, addr);
	if (pte_none(*pte))
		return 0;

	return pfn_valid(pte_pfn(*pte));
}
#ifdef CONFIG_SPARSEMEM_VMEMMAP
#ifdef CONFIG_ARM64_64K_PAGES
int __meminit vmemmap_populate(unsigned long start, unsigned long end, int node)
{
	return vmemmap_populate_basepages(start, end, node);
}
#else	/* !CONFIG_ARM64_64K_PAGES */
int __meminit vmemmap_populate(unsigned long start, unsigned long end, int node)
{
	unsigned long addr = start;
	unsigned long next;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	do {
		next = pmd_addr_end(addr, end);

		pgd = vmemmap_pgd_populate(addr, node);
		if (!pgd)
			return -ENOMEM;

		pud = vmemmap_pud_populate(pgd, addr, node);
		if (!pud)
			return -ENOMEM;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd)) {
			void *p = NULL;

			p = vmemmap_alloc_block_buf(PMD_SIZE, node);
			if (!p)
				return -ENOMEM;

			set_pmd(pmd, __pmd(__pa(p) | PROT_SECT_NORMAL));
		} else
			vmemmap_verify((pte_t *)pmd, node, addr, next);
	} while (addr = next, addr != end);

	return 0;
}
#endif	/* CONFIG_ARM64_64K_PAGES */
void vmemmap_free(unsigned long start, unsigned long end)
{
}
#endif	/* CONFIG_SPARSEMEM_VMEMMAP */
