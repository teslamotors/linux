extern void __init bootmem_init(void);
extern void __init arm64_swiotlb_init(void);
extern void dma_contiguous_remap(void);

static inline pmd_t *pmd_off_k(unsigned long virt)
{
	return pmd_offset(pud_offset(pgd_offset_k(virt), virt), virt);
}

/* consistent regions used by dma_alloc_attrs() */
#define VM_ARM_DMA_CONSISTENT  0x20000000

#ifdef CONFIG_ZONE_DMA
#define arm_dma_limit (0xffffffff)
#else
#define arm_dma_limit ((phys_addr_t)~0)
#endif

void fixup_init(void);
