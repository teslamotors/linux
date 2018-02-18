/*
 * mods_internal.h - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MODS_INTERNAL_H_
#define _MODS_INTERNAL_H_

#include <linux/version.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>

#include "mods_config.h"
#include "mods.h"

#ifndef true
#define true	1
#define false	0
#endif

/* function return code */
#define OK		 0
#define ERROR		-1

#define IRQ_FOUND	 1
#define IRQ_NOT_FOUND	 0

#define DEV_FOUND	 1
#define DEV_NOT_FOUND	 0

#define MSI_DEV_FOUND	  1
#define MSI_DEV_NOT_FOUND 0

struct en_dev_entry {
	struct pci_dev	    *dev;
	struct en_dev_entry *next;
};

struct mem_type {
	u64 dma_addr;
	u64 size;
	u32 type;
};

/* file private data */
struct mods_file_private_data {
	struct list_head    *mods_alloc_list;
	struct list_head    *mods_mapping_list;
	struct list_head    *mods_pci_res_map_list;
#if defined(MODS_HAS_SET_PPC_TCE_BYPASS)
	struct list_head    *mods_ppc_tce_bypass_list;
#endif
	wait_queue_head_t    interrupt_event;
	struct en_dev_entry *enabled_devices;
	int                  mods_id;
	struct mem_type      mem_type;
	struct mutex         mtx;
};

/* VM private data */
struct mods_vm_private_data {
	struct file *fp;
	atomic_t     usage_count;
};

/* PCI Resource mapping private data*/
struct MODS_PCI_RES_MAP_INFO {
	struct pci_dev  *dev;          /* pci_dev the mapping was on */
	u64              page_count;   /* number of pages for the mapping */
	u64              va;           /* va address of the mapping */
	struct list_head list;
};

struct MODS_PHYS_CHUNK {
	u64          dma_addr:58; /* phys addr (or machine addr on XEN) */
	u32          order:5;     /* 1<<order = number of contig pages */
	int          allocated:1;
	struct page *p_page;
};

struct MODS_MAP_CHUNK {
	struct MODS_PHYS_CHUNK *pt;
	u64 map_addr;
};

struct MODS_DMA_MAP {
	struct pci_dev  *dev;          /* pci_dev to map the page to */
	struct list_head list;
	struct MODS_MAP_CHUNK mapping[1];
};

/* system memory allocation tracking */
struct MODS_MEM_INFO {
	u64		 logical_addr; /* kernel logical address */
	u32		 num_pages;    /* number of allocated pages */
	u32		 alloc_type;   /* MODS_ALLOC_TYPE_* */
	u32		 cache_type;   /* MODS_MEMORY_* */
	u32		 length;       /* actual number of bytes allocated */
	u32		 max_chunks;   /* max number of contig chunks */
	u32		 addr_bits;    /* phys addr size requested */
	int		 numa_node;    /* numa node for the allocation */
	struct pci_dev  *dev;  /* backwards compatibility : pci_dev that the
				* memory was allocated on
				*/

	struct list_head *dma_map_list;

	struct list_head list;

	/* information about allocated pages */
	struct MODS_PHYS_CHUNK pages[1];
};

#define MODS_ALLOC_TYPE_NON_CONTIG	0
#define MODS_ALLOC_TYPE_CONTIG		1
#define MODS_ALLOC_TYPE_BIGPHYS_AREA	2

/* map memory tracking */
struct SYS_MAP_MEMORY {
	/* used for offset lookup, NULL for device memory */
	struct MODS_MEM_INFO *p_mem_info;

	u64 dma_addr;	    /* first physical address of given mapping,
			       machine address on Xen */
	u64 virtual_addr;   /* virtual address of given mapping */
	u32 mapping_length; /* tells how many bytes were mapped */

	struct list_head   list;
};

/* functions used to avoid global debug variables */
void mods_set_debug_level(int);
int mods_get_debug_level(void);
int mods_check_debug_level(int);
int mods_get_mem4g(void);
int mods_get_highmem4g(void);
void mods_set_highmem4g(int);
int mods_get_multi_instance(void);
void mods_set_multi_instance(int);
int mods_get_mem4goffset(void);

#if defined(MODS_HAS_SET_PPC_TCE_BYPASS)
void mods_set_ppc_tce_bypass(int bypass);
int mods_get_ppc_tce_bypass(void);

/* PPC TCE bypass tracking */
struct PPC_TCE_BYPASS {
	struct pci_dev *dev;
	u64 dma_mask;
	struct list_head   list;
};
#endif

#define IRQ_MAX			(256+PCI_IRQ_MAX)
#define PCI_IRQ_MAX		15
#define MODS_CHANNEL_MAX	32

#define IRQ_VAL_POISON		0xfafbfcfdU

/* debug print masks */
#define DEBUG_IOCTL		0x2
#define DEBUG_PCICFG		0x4
#define DEBUG_ACPI		0x8
#define DEBUG_ISR		0x10
#define DEBUG_MEM		0x20
#define DEBUG_FUNC		0x40
#define DEBUG_CLOCK		0x80
#define DEBUG_DETAILED		0x100
#define DEBUG_TEGRADC		0x200
#define DEBUG_TEGRADMA		0x400
#define DEBUG_ISR_DETAILED	(DEBUG_ISR | DEBUG_DETAILED)
#define DEBUG_MEM_DETAILED	(DEBUG_MEM | DEBUG_DETAILED)
#define DEBUG_ALL	        (DEBUG_IOCTL | DEBUG_PCICFG | DEBUG_ACPI | \
	DEBUG_ISR | DEBUG_MEM | DEBUG_FUNC | DEBUG_CLOCK | DEBUG_DETAILED | \
	DEBUG_TEGRADC | DEBUG_TEGRADMA)

#define LOG_ENT() mods_debug_printk(DEBUG_FUNC, "> %s\n", __func__)
#define LOG_EXT() mods_debug_printk(DEBUG_FUNC, "< %s\n", __func__)

#define mods_debug_printk(level, fmt, args...)\
	({ \
		if (mods_check_debug_level(level)) \
			pr_info("mods debug: " fmt, ##args); \
	})

#define mods_info_printk(fmt, args...)\
	pr_info("mods: " fmt, ##args)

#define mods_error_printk(fmt, args...)\
	pr_info("mods error: " fmt, ##args)

#define mods_warning_printk(fmt, args...)\
	pr_info("mods warning: " fmt, ##args)

struct irq_q_data {
	u32		time;
	struct pci_dev *dev;
	u32		irq;
};

struct irq_q_info {
	struct irq_q_data data[MODS_MAX_IRQS];
	u32		  head;
	u32		  tail;
};

struct irq_mask_info {
	u32	*dev_irq_mask_reg;  /*IRQ mask register, read-only reg*/
	u32	*dev_irq_state;     /* IRQ status register*/
	u32 *dev_irq_disable_reg; /* potentionally a write-only reg*/
	u64	irq_and_mask;
	u64	 irq_or_mask;
	u8	 mask_type;
};

struct dev_irq_map {
	void	*dev_irq_aperture;
	u32	apic_irq;
	u8	type;
	u8	channel;
	u8	mask_info_cnt;
	struct	irq_mask_info mask_info[MODS_IRQ_MAX_MASKS];
	struct	pci_dev      *dev;
	struct	list_head     list;
};

struct mods_priv {
	/* map info from pci irq to apic irq */
	struct list_head  irq_head[MODS_CHANNEL_MAX];

	/* bits map for each allocated id. Each mods has an id. */
	/* the design is to take  into	account multi mods. */
	unsigned long	  channel_flags;

	/* fifo loop queue */
	struct irq_q_info rec_info[MODS_CHANNEL_MAX];
	spinlock_t	  lock;
};

#ifndef MODS_HAS_SET_MEMORY
#	define MODS_SET_MEMORY_UC(addr, pages) \
	       change_page_attr(virt_to_page(addr), pages, PAGE_KERNEL_NOCACHE)
#	define MODS_SET_MEMORY_WC MODS_SET_MEMORY_UC
#	define MODS_SET_MEMORY_WB(addr, pages) \
	       change_page_attr(virt_to_page(addr), pages, PAGE_KERNEL)
#elif ((defined(CONFIG_ARM) || defined(CONFIG_ARM64)) && \
	  !defined(CONFIG_CPA) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)) || \
	  defined(CONFIG_PPC64)
#	define MODS_SET_MEMORY_UC(addr, pages) 0
#	define MODS_SET_MEMORY_WC(addr, pages) 0
#	define MODS_SET_MEMORY_WB(addr, pages) 0
#else
#	define MODS_SET_MEMORY_UC(addr, pages) set_memory_uc(addr, pages)
#	ifdef MODS_HAS_WC
#		define MODS_SET_MEMORY_WC(addr, pages)\
		       set_memory_wc(addr, pages)
#	else
#		define MODS_SET_MEMORY_WC(addr, pages)\
		       MODS_SET_MEMORY_UC(addr, pages)
#	endif
#	define MODS_SET_MEMORY_WB(addr, pages) set_memory_wb(addr, pages)
#endif

#define MODS_PGPROT_UC pgprot_noncached
#ifdef MODS_HAS_WC
#	define MODS_PGPROT_WC pgprot_writecombine
#else
#	define MODS_PGPROT_WC pgprot_noncached
#endif

/* VMA */
#define MODS_VMA_PGOFF(vma)	((vma)->vm_pgoff)
#define MODS_VMA_SIZE(vma)	((vma)->vm_end - (vma)->vm_start)
#define MODS_VMA_OFFSET(vma)	(((u64)(vma)->vm_pgoff) << PAGE_SHIFT)
#define MODS_VMA_PRIVATE(vma)	((vma)->vm_private_data)
#define MODS_VMA_FILE(vma)	((vma)->vm_file)

/* Xen adds a translation layer between the physical address
 * and real system memory address space.
 *
 * To illustrate if a PC has 2 GBs of RAM and each VM is given 1GB, then:
 * for guest OS in domain 0, physical address = machine address;
 * for guest OS in domain 1, physical address x = machine address 1GB+x
 *
 * In reality even domain's 0 physical address is not equal to machine
 * address and the mappings are not continuous.
 */

#if defined(CONFIG_XEN) && !defined(CONFIG_PARAVIRT) && \
	  !defined(CONFIG_ARM) && !defined(CONFIG_ARM64)
	#define MODS_PHYS_TO_DMA(phys_addr) phys_to_machine(phys_addr)
	#define MODS_DMA_TO_PHYS(dma_addr)  machine_to_phys(dma_addr)
#else
	#define MODS_PHYS_TO_DMA(phys_addr) (phys_addr)
	#define MODS_DMA_TO_PHYS(dma_addr)  (dma_addr)
#endif

/* PCI */
#define MODS_PCI_GET_SLOT(mydomain, mybus, devfn)			     \
({									     \
	struct pci_dev *__dev = NULL;					     \
	while ((__dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, __dev))) {    \
		if (pci_domain_nr(__dev->bus) == mydomain		     \
		    && __dev->bus->number == mybus			     \
		    && __dev->devfn == devfn)				     \
			break;						     \
	}								     \
	__dev;								     \
})

/* ACPI */
#ifdef MODS_HAS_NEW_ACPI_WALK
#define MODS_ACPI_WALK_NAMESPACE(type, start_object, max_depth, user_function, \
				 context, return_value)\
	acpi_walk_namespace(type, start_object, max_depth, user_function, NULL,\
			    context, return_value)
#else
#define MODS_ACPI_WALK_NAMESPACE acpi_walk_namespace
#endif
#ifdef MODS_HAS_NEW_ACPI_HANDLE
#define MODS_ACPI_HANDLE(dev) ACPI_HANDLE(dev)
#else
#define MODS_ACPI_HANDLE(dev) DEVICE_ACPI_HANDLE(dev)
#endif

/* FILE */
#define MODS_PRIVATE_DATA(var, fp) \
	struct mods_file_private_data *var = (fp)->private_data
#define MODS_GET_FILE_PRIVATE_ID(fp) (((struct mods_file_private_data *)(fp) \
				      ->private_data)->mods_id)

/* ************************************************************************* */
/* ** MODULE WIDE FUNCTIONS						     */
/* ************************************************************************* */

/* irq */
void mods_init_irq(void);
void mods_cleanup_irq(void);
unsigned char mods_alloc_channel(void);
void mods_free_channel(unsigned char);
void mods_irq_dev_clr_pri(unsigned char);
void mods_irq_dev_set_pri(unsigned char id, void *pri);
int mods_irq_event_check(unsigned char);

/* mem */
const char *mods_get_prot_str(u32 mem_type);
int mods_unregister_all_alloc(struct file *fp);
struct MODS_MEM_INFO *mods_find_alloc(struct file *, u64);

#if defined(MODS_HAS_SET_PPC_TCE_BYPASS)
int mods_unregister_all_ppc_tce_bypass(struct file *fp);
#endif

#ifdef CONFIG_PCI
int mods_unregister_all_pci_res_mappings(struct file *fp);
#define MODS_UNREGISTER_PCI_MAP(fp) mods_unregister_all_pci_res_mappings(fp)
#else
#define MODS_UNREGISTER_PCI_MAP(fp) 0
#endif

/* clock */
#ifdef MODS_TEGRA
void mods_init_clock_api(void);
void mods_shutdown_clock_api(void);
#endif

/* ioctl hanndlers */

/* mem */
int esc_mods_alloc_pages(struct file *, struct MODS_ALLOC_PAGES *);
int esc_mods_device_alloc_pages(struct file *,
				struct MODS_DEVICE_ALLOC_PAGES *);
int esc_mods_device_alloc_pages_2(struct file *,
				  struct MODS_DEVICE_ALLOC_PAGES_2 *);
int esc_mods_free_pages(struct file *, struct MODS_FREE_PAGES *);
int esc_mods_set_mem_type(struct file *, struct MODS_MEMORY_TYPE *);
int esc_mods_get_phys_addr(struct file *,
			   struct MODS_GET_PHYSICAL_ADDRESS *);
int esc_mods_get_mapped_phys_addr(struct file *,
			  struct MODS_GET_PHYSICAL_ADDRESS *);
int esc_mods_get_mapped_phys_addr_2(struct file *fp,
				    struct MODS_GET_PHYSICAL_ADDRESS_2 *p);
int esc_mods_virtual_to_phys(struct file *,
			     struct MODS_VIRTUAL_TO_PHYSICAL *);
int esc_mods_phys_to_virtual(struct file *,
			     struct MODS_PHYSICAL_TO_VIRTUAL *);
int esc_mods_memory_barrier(struct file *);
#if defined(MODS_HAS_SET_PPC_TCE_BYPASS)
int esc_mods_set_ppc_tce_bypass(struct file *,
				struct MODS_SET_PPC_TCE_BYPASS *);
#endif

int esc_mods_dma_map_memory(struct file *,
			    struct MODS_DMA_MAP_MEMORY *);
int esc_mods_dma_unmap_memory(struct file *,
			      struct MODS_DMA_MAP_MEMORY *);

/* acpi */
#ifdef CONFIG_ACPI
int esc_mods_eval_acpi_method(struct file *,
			      struct MODS_EVAL_ACPI_METHOD *);
int esc_mods_eval_dev_acpi_method(struct file *,
				  struct MODS_EVAL_DEV_ACPI_METHOD *);
int esc_mods_eval_dev_acpi_method_2(struct file *,
				    struct MODS_EVAL_DEV_ACPI_METHOD_2 *);
int esc_mods_acpi_get_ddc(struct file *, struct MODS_ACPI_GET_DDC *);
int esc_mods_acpi_get_ddc_2(struct file *, struct MODS_ACPI_GET_DDC_2 *);
#endif
/* pci */
#ifdef CONFIG_PCI
int esc_mods_find_pci_dev(struct file *, struct MODS_FIND_PCI_DEVICE *);
int esc_mods_find_pci_dev_2(struct file *,
			    struct MODS_FIND_PCI_DEVICE_2 *);
int esc_mods_find_pci_class_code(struct file *,
				 struct MODS_FIND_PCI_CLASS_CODE *);
int esc_mods_find_pci_class_code_2(struct file *,
				   struct MODS_FIND_PCI_CLASS_CODE_2 *);
int esc_mods_pci_get_bar_info(struct file *, struct MODS_PCI_GET_BAR_INFO *);
int esc_mods_pci_get_bar_info_2(struct file *,
				struct MODS_PCI_GET_BAR_INFO_2 *);
int esc_mods_pci_get_irq(struct file *, struct MODS_PCI_GET_IRQ *);
int esc_mods_pci_get_irq_2(struct file *,
			   struct MODS_PCI_GET_IRQ_2 *);
int esc_mods_pci_read(struct file *, struct MODS_PCI_READ *);
int esc_mods_pci_read_2(struct file *, struct MODS_PCI_READ_2 *);
int esc_mods_pci_write(struct file *, struct MODS_PCI_WRITE *);
int esc_mods_pci_write_2(struct file *, struct MODS_PCI_WRITE_2 *);
int esc_mods_pci_bus_add_dev(struct file *,
			     struct MODS_PCI_BUS_ADD_DEVICES *);
int esc_mods_pci_hot_reset(struct file *,
			   struct MODS_PCI_HOT_RESET *);
int esc_mods_pio_read(struct file *, struct MODS_PIO_READ *);
int esc_mods_pio_write(struct file *, struct MODS_PIO_WRITE  *);
int esc_mods_device_numa_info(struct file *,
			      struct MODS_DEVICE_NUMA_INFO  *);
int esc_mods_device_numa_info_2(struct file *,
				struct MODS_DEVICE_NUMA_INFO_2  *);
int esc_mods_pci_map_resource(struct file *,
			      struct MODS_PCI_MAP_RESOURCE  *);
int esc_mods_pci_unmap_resource(struct file *,
				struct MODS_PCI_UNMAP_RESOURCE  *);
#endif
/* irq */
int esc_mods_register_irq(struct file *, struct MODS_REGISTER_IRQ *);
int esc_mods_register_irq_2(struct file *,
			    struct MODS_REGISTER_IRQ_2 *);
int esc_mods_register_irq_3(struct file *,
			    struct MODS_REGISTER_IRQ_3 *);
int esc_mods_unregister_irq(struct file *, struct MODS_REGISTER_IRQ *);
int esc_mods_unregister_irq_2(struct file *,
			      struct MODS_REGISTER_IRQ_2 *);
int esc_mods_query_irq(struct file *, struct MODS_QUERY_IRQ *);
int esc_mods_query_irq_2(struct file *, struct MODS_QUERY_IRQ_2 *);
int esc_mods_set_irq_mask(struct file *, struct MODS_SET_IRQ_MASK *);
int esc_mods_set_irq_mask_2(struct file *,
			    struct MODS_SET_IRQ_MASK_2 *);
int esc_mods_set_irq_multimask(struct file *,
			       struct MODS_SET_IRQ_MULTIMASK *);
int esc_mods_irq_handled(struct file *, struct MODS_REGISTER_IRQ *);
int esc_mods_irq_handled_2(struct file *,
			   struct MODS_REGISTER_IRQ_2 *);
/* clock */
#ifdef MODS_TEGRA
int esc_mods_get_clock_handle(struct file *,
			      struct MODS_GET_CLOCK_HANDLE *);
int esc_mods_set_clock_rate(struct file *, struct MODS_CLOCK_RATE *);
int esc_mods_get_clock_rate(struct file *, struct MODS_CLOCK_RATE *);
int esc_mods_get_clock_max_rate(struct file *, struct MODS_CLOCK_RATE *);
int esc_mods_set_clock_max_rate(struct file *, struct MODS_CLOCK_RATE *);
int esc_mods_set_clock_parent(struct file *, struct MODS_CLOCK_PARENT *);
int esc_mods_get_clock_parent(struct file *, struct MODS_CLOCK_PARENT *);
int esc_mods_enable_clock(struct file *, struct MODS_CLOCK_HANDLE *);
int esc_mods_disable_clock(struct file *, struct MODS_CLOCK_HANDLE *);
int esc_mods_is_clock_enabled(struct file *pfile,
			      struct MODS_CLOCK_ENABLED *p);
int esc_mods_clock_reset_assert(struct file *,
				struct MODS_CLOCK_HANDLE *);
int esc_mods_clock_reset_deassert(struct file *,
				  struct MODS_CLOCK_HANDLE *);
int esc_mods_flush_cpu_cache_range(struct file *,
				   struct MODS_FLUSH_CPU_CACHE_RANGE *);
int esc_mods_dma_alloc_coherent(struct file *,
				struct MODS_DMA_COHERENT_MEM_HANDLE *);
int esc_mods_dma_free_coherent(struct file *,
				struct MODS_DMA_COHERENT_MEM_HANDLE *);
int esc_mods_dma_copy_to_user(struct file *,
				struct MODS_DMA_COPY_TO_USER *);
#ifdef CONFIG_DMA_ENGINE
int esc_mods_dma_request_channel(struct file *, struct MODS_DMA_HANDLE *);
int esc_mods_dma_release_channel(struct file *, struct MODS_DMA_HANDLE *);
int esc_mods_dma_set_config(struct file *, struct MODS_DMA_CHANNEL_CONFIG *);
int esc_mods_dma_wait(struct file *, struct MODS_DMA_WAIT_DESC *p_wait_desc);
int esc_mods_dma_submit_request(struct file *,
				struct MODS_DMA_TX_DESC *p_mods_desc);
int esc_mods_dma_async_issue_pending(struct file *,
				struct MODS_DMA_HANDLE *p_handle);
#endif
#ifdef CONFIG_TEGRA_DC
int esc_mods_tegra_dc_config_possible(struct file *,
				struct MODS_TEGRA_DC_CONFIG_POSSIBLE *);
int esc_mods_tegra_dc_setup_sd(struct file *, struct MODS_TEGRA_DC_SETUP_SD *);
#endif
#ifdef MODS_HAS_NET
int esc_mods_net_force_link(struct file *pfile, struct MODS_NET_DEVICE_NAME *p);
#endif

#ifdef MODS_HAS_DMABUF
int esc_mods_dmabuf_get_phys_addr(struct file *,
				  struct MODS_DMABUF_GET_PHYSICAL_ADDRESS *);
#else
static inline int esc_mods_dmabuf_get_phys_addr(struct file *f,
				  struct MODS_DMABUF_GET_PHYSICAL_ADDRESS *a)
				  { return -EINVAL; }
#endif
#ifdef CONFIG_TEGRA_NVADSP
int esc_mods_adsp_load(struct file *pfile);
int esc_mods_adsp_start(struct file *pfile);
int esc_mods_adsp_stop(struct file *pfile);
int esc_mods_adsp_run_app(struct file *pfile, struct MODS_ADSP_RUN_APP_INFO *p);
#endif
#endif

#ifdef CONFIG_DEBUG_FS
int mods_create_debugfs(struct miscdevice *modsdev);
void mods_remove_debugfs(void);
#else
static inline int mods_create_debugfs(struct miscdevice *modsdev)
{
	return 0;
}
static inline void mods_remove_debugfs(void) {}
#endif /* CONFIG_DEBUG_FS */

#ifdef CONFIG_TEGRA_DC
int mods_init_tegradc(void);
void mods_exit_tegradc(void);
#else
static inline int mods_init_tegradc(void) { return 0; }
static inline void mods_exit_tegradc(void) {}
#endif

#if defined(MODS_TEGRA) && defined(MODS_HAS_DMABUF)
int mods_init_dmabuf(void);
void mods_exit_dmabuf(void);
#else
static inline int mods_init_dmabuf(void) { return 0; }
static inline void mods_exit_dmabuf(void) {}
#endif

#endif	/* _MODS_INTERNAL_H_  */
