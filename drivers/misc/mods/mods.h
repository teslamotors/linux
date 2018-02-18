/*
 * mods.h - This file is part of NVIDIA MODS kernel driver.
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

#ifndef _MODS_H_
#define _MODS_H_

#include <linux/types.h>

/* Driver version */
#define MODS_DRIVER_VERSION_MAJOR 3
#define MODS_DRIVER_VERSION_MINOR 65
#define MODS_DRIVER_VERSION ((MODS_DRIVER_VERSION_MAJOR << 8) | \
			     ((MODS_DRIVER_VERSION_MINOR/10) << 4) | \
			     (MODS_DRIVER_VERSION_MINOR%10))

#pragma pack(push, 1)

/* ************************************************************************* */
/* ** ESCAPE INTERFACE STRUCTURE					     */
/* ************************************************************************* */

struct mods_pci_dev_2 {
	__u16 domain;
	__u16 bus;
	__u16 device;
	__u16 function;
};

struct mods_pci_dev {
	__u16 bus;
	__u8  device;
	__u8  function;
};

/* MODS_ESC_SET_PPC_TCE_BYPASS */
#define MODS_PPC_TCE_BYPASS_DEFAULT  0
#define MODS_PPC_TCE_BYPASS_ON       1
#define MODS_PPC_TCE_BYPASS_OFF      2
struct MODS_SET_PPC_TCE_BYPASS {
	/* IN */
	__u8                  mode;
	__u8                  _dummy_align[7];
	struct mods_pci_dev_2 pci_device;
	__u64	              device_dma_mask;

	/* OUT */
	__u64	              dma_base_address;
};

/* MODS_ESC_ALLOC_PAGES */
struct MODS_ALLOC_PAGES {
	/* IN */
	__u32	num_bytes;
	__u32	contiguous;
	__u32	address_bits;
	__u32	attrib;

	/* OUT */
	__u64	memory_handle;
};

/* MODS_ESC_DEVICE_ALLOC_PAGES_2 */
struct MODS_DEVICE_ALLOC_PAGES_2 {
	/* IN */
	__u32			num_bytes;
	__u32			contiguous;
	__u32			address_bits;
	__u32			attrib;
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__u64			memory_handle;
};

/* MODS_ESC_DEVICE_ALLOC_PAGES */
struct MODS_DEVICE_ALLOC_PAGES {
	/* IN */
	__u32		    num_bytes;
	__u32		    contiguous;
	__u32		    address_bits;
	__u32		    attrib;
	struct mods_pci_dev pci_device;

	/* OUT */
	__u64		    memory_handle;
};

/* MODS_ESC_FREE_PAGES */
struct MODS_FREE_PAGES {
	/* IN */
	__u64	memory_handle;
};

/* MODS_ESC_GET_PHYSICAL_ADDRESS */
struct MODS_GET_PHYSICAL_ADDRESS {
	/* IN */
	__u64	memory_handle;
	__u32	offset;

	/* OUT */
	__u64	physical_address;
};

/* MODS_ESC_GET_PHYSICAL_ADDRESS */
struct MODS_GET_PHYSICAL_ADDRESS_2 {
	/* IN */
	__u64                 memory_handle;
	__u32	              offset;
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__u64                 physical_address;
};

/* MODS_ESC_VIRTUAL_TO_PHYSICAL */
struct MODS_VIRTUAL_TO_PHYSICAL {
	/* IN */
	__u64	virtual_address;

	/* OUT */
	__u64	physical_address;
};

/* MODS_ESC_PHYSICAL_TO_VIRTUAL */
struct MODS_PHYSICAL_TO_VIRTUAL {
	/* IN */
	__u64	physical_address;

	/* OUT */
	__u64	virtual_address;

};

/* MODS_ESC_FLUSH_CACHE_RANGE */
#define MODS_FLUSH_CPU_CACHE	  1
#define MODS_INVALIDATE_CPU_CACHE 2

struct MODS_FLUSH_CPU_CACHE_RANGE {
	/* IN */
	__u64 virt_addr_start;
	__u64 virt_addr_end;
	__u32 flags;
};

/* MODS_ESC_DMA_MAP_MEMORY */
struct MODS_DMA_MAP_MEMORY {
	/* IN */
	__u64                 memory_handle;
	struct mods_pci_dev_2 pci_device;
};

/* MODS_ESC_FIND_PCI_DEVICE_2 */
struct MODS_FIND_PCI_DEVICE_2 {
	/* IN */
	__u32	device_id;
	__u32	vendor_id;
	__u32	index;

	/* OUT */
	struct mods_pci_dev_2 pci_device;
};

/* MODS_ESC_FIND_PCI_DEVICE */
struct MODS_FIND_PCI_DEVICE {
	/* IN */
	__u32	  device_id;
	__u32	  vendor_id;
	__u32	  index;

	/* OUT */
	__u32	  bus_number;
	__u32	  device_number;
	__u32	  function_number;
};

/* MODS_ESC_FIND_PCI_CLASS_CODE_2 */
struct MODS_FIND_PCI_CLASS_CODE_2 {
	/* IN */
	__u32	class_code;
	__u32	index;

	/* OUT */
	struct mods_pci_dev_2 pci_device;
};

/* MODS_ESC_FIND_PCI_CLASS_CODE */
struct MODS_FIND_PCI_CLASS_CODE {
	/* IN */
	__u32	class_code;
	__u32	index;

	/* OUT */
	__u32	bus_number;
	__u32	device_number;
	__u32	function_number;
};

/* MODS_ESC_PCI_GET_BAR_INFO_2 */
struct MODS_PCI_GET_BAR_INFO_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u32			bar_index;

	/* OUT */
	__u64 base_address;
	__u64 bar_size;
};

/* MODS_ESC_PCI_GET_BAR_INFO */
struct MODS_PCI_GET_BAR_INFO {
	/* IN */
	struct mods_pci_dev pci_device;
	__u32		    bar_index;

	/* OUT */
	__u64 base_address;
	__u64 bar_size;
};

/* MODS_ESC_PCI_GET_IRQ_2 */
struct MODS_PCI_GET_IRQ_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__u32 irq;
};

/* MODS_ESC_PCI_GET_IRQ */
struct MODS_PCI_GET_IRQ {
	/* IN */
	struct mods_pci_dev pci_device;

	/* OUT */
	__u32 irq;
};

/* MODS_ESC_PCI_READ_2 */
struct MODS_PCI_READ_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u32			address;
	__u32			data_size;

	/* OUT */
	__u32			data;
};

/* MODS_ESC_PCI_READ */
struct MODS_PCI_READ {
	/* IN */
	__u32	bus_number;
	__u32	device_number;
	__u32	function_number;
	__u32	address;
	__u32	data_size;

	/* OUT */
	__u32	 data;
};

/* MODS_ESC_PCI_WRITE_2 */
struct MODS_PCI_WRITE_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u32			address;
	__u32			data;
	__u32			data_size;
};

/* MODS_ESC_PCI_WRITE */
struct MODS_PCI_WRITE {
	/* IN */
	__u32	bus_number;
	__u32	device_number;
	__u32	function_number;
	__u32	address;
	__u32	data;
	__u32	data_size;
};

/* MODS_ESC_PCI_HOT_RESET */
struct MODS_PCI_HOT_RESET {
	/* IN */
	struct mods_pci_dev_2 pci_device;
};

/* MODS_ESC_PCI_BUS_ADD_DEVICES*/
struct MODS_PCI_BUS_ADD_DEVICES {
	/* IN */
	__u32	 bus;
};

/* MODS_ESC_PCI_MAP_RESOURCE */
struct MODS_PCI_MAP_RESOURCE {
	/* IN */
	struct mods_pci_dev_2 local_pci_device;
	struct mods_pci_dev_2 remote_pci_device;
	__u32                 resource_index;
	__u64                 page_count;

	/* IN/OUT */
	__u64                 va;
};

/* MODS_ESC_PCI_UNMAP_RESOURCE */
struct MODS_PCI_UNMAP_RESOURCE {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u64                 va;
};

/* MODS_ESC_PIO_READ */
struct MODS_PIO_READ {
	/* IN */
	__u16	port;
	__u32	data_size;

	/* OUT */
	__u32	data;
};

/* MODS_ESC_PIO_WRITE */
struct MODS_PIO_WRITE {
	/* IN */
	__u16	port;
	__u32	data;
	__u32	data_size;
};

#define INQ_CNT 8

struct mods_irq_data {
	__u32 irq;
	__u32 delay;
};

struct mods_irq_status {
	struct mods_irq_data data[INQ_CNT];
	__u32 irqbits:INQ_CNT;
	__u32 otherirq:1;
};

/* MODS_ESC_IRQ */
struct MODS_IRQ {
	/* IN */
	__u32 cmd;
	__u32 size;		/* memory size */
	__u32 irq;		/* the irq number to be registered in driver */

	/* IN OUT */
	__u32 channel;		/* application id allocated by driver. */

	/* OUT */
	struct mods_irq_status stat;	/* for querying irq */
	__u64		 phys;	/* the memory physical address */
};
#define MODS_IRQ_MAX_MASKS 16

/* MODS_ESC_REGISTER_IRQ_3 */
struct mods_mask_info2 {
	__u8	mask_type;  /*mask type 32/64 bit access*/
	__u8	reserved[7]; /*force 64bit alignment */
	__u32	irq_pending_offset; /* register to read IRQ pending status*/
	__u32   irq_enabled_offset; /* register to read IRQ enabled status */
	__u32   irq_enable_offset;  /* register to write to enable IRQs */
	__u32   irq_disable_offset; /* register to write to disable IRQs */
	__u64	and_mask;   /*and mask for clearing bits in this register */
	__u64	or_mask;    /*or mask for setting bit in this register */
};

struct MODS_REGISTER_IRQ_3 {
	/* IN */
	struct	mods_pci_dev_2 dev; /* device identifying interrupt for */
				    /* which the mask will be applied */
	__u64	aperture_addr;	    /* physical address of aperture */
	__u32	aperture_size;	    /* size of the mapped region */
	__u32	mask_info_cnt;	    /* number of entries in mask_info[]*/
	struct	mods_mask_info2 mask_info[MODS_IRQ_MAX_MASKS];
	__u8	irq_type;	    /* MODS_IRQ_TYPE_* */
	__u8	reserved[7];	    /* keep alignment to 64bits */
};

/* MODS_ESC_REGISTER_IRQ_2 */
/* MODS_ESC_UNREGISTER_IRQ_2 */
/* MODS_ESC_IRQ_HANDLED_2 */
struct MODS_REGISTER_IRQ_2 {
	/* IN */
	struct mods_pci_dev_2 dev;  /* device which generates the interrupt */
	__u8			type; /* MODS_IRQ_TYPE_* */
};

/* MODS_ESC_REGISTER_IRQ */
/* MODS_ESC_UNREGISTER_IRQ */
/* MODS_ESC_IRQ_HANDLED */
struct MODS_REGISTER_IRQ {
	/* IN */
	struct mods_pci_dev dev;   /* device which generates
				      the interrupt */
	__u8		    type;  /* MODS_IRQ_TYPE_* */
};

struct mods_irq_2 {
	__u32			delay; /* delay in ns between the irq occuring
					  and MODS querying for it */
	struct mods_pci_dev_2 dev;  /* device which generated the interrupt */
};

struct mods_irq {
	__u32		    delay; /* delay in ns between the irq
				      occuring and MODS querying
				      for it */
	struct mods_pci_dev dev;   /* device which generated
				      the interrupt */
};

#define MODS_MAX_IRQS 32

/* MODS_ESC_QUERY_IRQ_2 */
struct MODS_QUERY_IRQ_2 {
	/* OUT */
	struct mods_irq_2 irq_list[MODS_MAX_IRQS];
	__u8		    more;	/* indicates that more interrupts
					   are waiting */
};

/* MODS_ESC_QUERY_IRQ */
struct MODS_QUERY_IRQ {
	/* OUT */
	struct mods_irq irq_list[MODS_MAX_IRQS];
	__u8		more;  /* indicates that more interrupts are waiting */
};

#define MODS_IRQ_TYPE_INT  0
#define MODS_IRQ_TYPE_MSI  1
#define MODS_IRQ_TYPE_CPU  2

/* MODS_ESC_SET_IRQ_MULTIMASK */
struct mods_mask_info {
	__u8	mask_type;  /*mask type 32/64 bit access*/
	__u8	reserved[3];
	__u32	reg_offset; /* offset of register within the bar aperture*/
	__u64	and_mask;   /*and mask for clearing bits in this register */
	__u64	or_mask;    /*or value for setting bit in this register */
};
struct MODS_SET_IRQ_MULTIMASK {
	/* IN */
	__u64	aperture_addr;	    /* physical address of aperture */
	__u32	aperture_size;	    /* size of the mapped region */
	struct	mods_pci_dev_2 dev; /* device identifying interrupt for */
				    /* which the mask will be applied */
	__u32	mask_info_cnt;	    /* number of entries in mask_info[]*/
	struct	mods_mask_info mask_info[MODS_IRQ_MAX_MASKS];
	__u8	irq_type;	    /* irq type */
};


/* MODS_ESC_SET_IRQ_MASK_2 */
struct MODS_SET_IRQ_MASK_2 {
	/* IN */
	__u64			aperture_addr;/* physical address of aperture */
	__u32			aperture_size;/* size of the mapped region */
	__u32			reg_offset;   /* offset of the irq mask register
						 within the aperture */
	__u64			and_mask;     /* and mask for clearing bits in
						 the irq mask register */
	__u64			or_mask;      /* or mask for setting bits in
						 the irq mask register */
	struct mods_pci_dev_2 dev;	      /* device identifying interrupt
						 for which the mask will be
						 applied */
	__u8			irq_type;     /* irq type */
	__u8			mask_type;    /* mask type */
};

/* MODS_ESC_SET_IRQ_MASK */
struct MODS_SET_IRQ_MASK {
	/* IN */
	__u64		    aperture_addr; /* physical address of aperture */
	__u32		    aperture_size; /* size of the mapped region */
	__u32		    reg_offset;	   /* offset of the irq mask register
					      within the aperture */
	__u32		    and_mask;	   /* and mask for clearing bits in
					      the irq mask register */
	__u32		    or_mask;	   /* or mask for setting bits in
					      the irq mask register */
	struct mods_pci_dev dev;	   /* device identifying interrupt for
					      which the mask will be applied */
	__u8		    irq_type;	   /* irq type */
	__u8		    mask_type;	   /* mask type */
};

#define MODS_MASK_TYPE_IRQ_DISABLE      0
#define MODS_MASK_TYPE_IRQ_DISABLE64    1

#define ACPI_MODS_TYPE_INTEGER		1
#define ACPI_MODS_TYPE_BUFFER		2
#define ACPI_MAX_BUFFER_LENGTH		4096
#define ACPI_MAX_METHOD_LENGTH		12
#define ACPI_MAX_ARGUMENT_NUMBER	12

union ACPI_ARGUMENT {
	__u32	type;

	struct {
		__u32 type;
		__u32 value;
	} integer;

	struct {
		__u32	type;
		__u32	length;
		__u32	offset;
	} buffer;
};

/* MODS_ESC_EVAL_ACPI_METHOD */
struct MODS_EVAL_ACPI_METHOD {
	/* IN */
	char		    method_name[ACPI_MAX_METHOD_LENGTH];
	__u32		    argument_count;
	union ACPI_ARGUMENT argument[ACPI_MAX_ARGUMENT_NUMBER];
	__u8		    in_buffer[ACPI_MAX_BUFFER_LENGTH];

	/* IN OUT */
	__u32		    out_data_size;

	/* OUT */
	__u8		    out_buffer[ACPI_MAX_BUFFER_LENGTH];
	__u32		    out_status;
};

/* MODS_ESC_EVAL_DEV_ACPI_METHOD_2 */
struct MODS_EVAL_DEV_ACPI_METHOD_2 {
	/* IN OUT */
	struct MODS_EVAL_ACPI_METHOD method;

	/* IN */
	struct mods_pci_dev_2 device;
};

/* MODS_ESC_EVAL_DEV_ACPI_METHOD */
struct MODS_EVAL_DEV_ACPI_METHOD {
	/* IN OUT */
	struct MODS_EVAL_ACPI_METHOD method;

	/* IN */
	struct mods_pci_dev device;
};

/* MODS_ESC_ACPI_GET_DDC_2 */
struct MODS_ACPI_GET_DDC_2 {
	/* OUT */
	__u32			out_data_size;
	__u8			out_buffer[ACPI_MAX_BUFFER_LENGTH];

	/* IN */
	struct mods_pci_dev_2 device;
};

/* MODS_ESC_ACPI_GET_DDC */
struct MODS_ACPI_GET_DDC {
	/* OUT */
	__u32		    out_data_size;
	__u8		    out_buffer[ACPI_MAX_BUFFER_LENGTH];

	/* IN */
	struct mods_pci_dev device;
};

/* MODS_ESC_GET_VERSION */
struct MODS_GET_VERSION {
	/* OUT */
	__u64 version;
};

/* MODS_ESC_SET_PARA */
struct MODS_SET_PARA {
	/* IN */
	__u64 Highmem4g;
	__u64 debug;
};

/* MODS_ESC_SET_MEMORY_TYPE */
struct MODS_MEMORY_TYPE {
	/* IN */
	__u64 physical_address;
	__u64 size;
	__u32 type;
};

#define MAX_CLOCK_HANDLE_NAME 64

/* MODS_ESC_GET_CLOCK_HANDLE */
struct MODS_GET_CLOCK_HANDLE {
	/* OUT */
	__u32 clock_handle;

	/* IN */
	char  device_name[MAX_CLOCK_HANDLE_NAME];
	char  controller_name[MAX_CLOCK_HANDLE_NAME];
};

/* MODS_ESC_SET_CLOCK_RATE, MODS_ESC_GET_CLOCK_RATE,
 * MODS_ESC_GET_CLOCK_MAX_RATE, MODS_ESC_SET_CLOCK_MAX_RATE */
struct MODS_CLOCK_RATE {
	/* IN/OUT */
	__u64 clock_rate_hz;

	/* IN */
	__u32 clock_handle;
};

/* MODS_ESC_SET_CLOCK_PARENT, MODS_ESC_GET_CLOCK_PARENT */
struct MODS_CLOCK_PARENT {
	/* IN */
	__u32 clock_handle;

	/* IN/OUT */
	__u32 clock_parent_handle;
};

/* MODS_ESC_ENABLE_CLOCK, MODS_ESC_DISABLE_CLOCK, MODS_ESC_CLOCK_RESET_ASSERT,
 * MODS_ESC_CLOCK_RESET_DEASSERT */
struct MODS_CLOCK_HANDLE {
	/* IN */
	__u32 clock_handle;
};

/* MODS_ESC_IS_CLOCK_ENABLED */
struct MODS_CLOCK_ENABLED {
	/* IN */
	__u32 clock_handle;

	/* OUT */
	__u32 enable_count;
};

#if defined(CONFIG_PPC64) || defined(PPC64LE)
#define MAX_CPU_MASKS 64  /* 32 masks of 32bits = 2048 CPUs max */
#else
#define MAX_CPU_MASKS 32  /* 32 masks of 32bits = 1024 CPUs max */
#endif
/* MODS_ESC_DEVICE_NUMA_INFO_2 */
struct MODS_DEVICE_NUMA_INFO_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__s32  node;
	__u32  node_count;
	__u32  node_cpu_mask[MAX_CPU_MASKS];
	__u32  cpu_count;
};

/* MODS_ESC_DEVICE_NUMA_INFO */
struct MODS_DEVICE_NUMA_INFO {
	/* IN */
	struct mods_pci_dev pci_device;

	/* OUT */
	__s32  node;
	__u32  node_count;
	__u32  node_cpu_mask[MAX_CPU_MASKS];
	__u32  cpu_count;
};

/* The ids match MODS ids */
#define MODS_MEMORY_CACHED		5
#define MODS_MEMORY_UNCACHED		1
#define MODS_MEMORY_WRITECOMBINE	2

struct MODS_TEGRA_DC_WINDOW {
	__s32 index;
	__u32 flags;
	__u32 x;
	__u32 y;
	__u32 w;
	__u32 h;
	__u32 out_x;
	__u32 out_y;
	__u32 out_w;
	__u32 out_h;
	__u32 pixformat; /* NVDC pix format */

	__u32 bandwidth;
};
#define MODS_TEGRA_DC_WINDOW_FLAG_ENABLED   (1 << 0)
#define MODS_TEGRA_DC_WINDOW_FLAG_TILED     (1 << 1)
#define MODS_TEGRA_DC_WINDOW_FLAG_SCAN_COL  (1 << 2)
#define MODS_TEGRA_DC_MAX_WINDOWS           (6)

/* MODS_ESC_TEGRA_DC_CONFIG_POSSIBLE */
struct MODS_TEGRA_DC_CONFIG_POSSIBLE {
	/* IN/OUT */
	struct MODS_TEGRA_DC_WINDOW windows[MODS_TEGRA_DC_MAX_WINDOWS];

	/* IN */
	__u8 head;
	__u8 win_num;

	/* OUT */
	__u8  possible;
};


#define MODS_TEGRA_DC_SETUP_SD_LUT_SIZE  9
#define MODS_TEGRA_DC_SETUP_BLTF_SIZE   16
/* MODS_ESC_TEGRA_DC_SETUP_SD */
struct MODS_TEGRA_DC_SETUP_SD {
	/* IN */
	__u8 head;
	__u8 enable;

	__u8 use_vid_luma;
	__u8 csc_r;
	__u8 csc_g;
	__u8 csc_b;
	__u8 aggressiveness;
	__u8 bin_width_log2;

	__u32 lut[MODS_TEGRA_DC_SETUP_SD_LUT_SIZE];
	__u32 bltf[MODS_TEGRA_DC_SETUP_BLTF_SIZE];

	__u32 klimit;
	__u32 soft_clipping_threshold;
	__u32 smooth_k_inc;
	__u8  k_init_bias;


	__u32 win_x;
	__u32 win_y;
	__u32 win_w;
	__u32 win_h;
};

/* MODS_ESC_DMABUF_GET_PHYSICAL_ADDRESS */
struct MODS_DMABUF_GET_PHYSICAL_ADDRESS {
	/* IN */
	__s32 buf_fd;
	__u32 padding;
	__u64 offset;

	/* OUT */
	__u64 physical_address;
	__u64 segment_size;
};

#define MODS_ADSP_APP_NAME_SIZE 64
#define MODS_ADSP_APP_MAX_PARAM 128
struct MODS_ADSP_RUN_APP_INFO {
	char app_name[MODS_ADSP_APP_NAME_SIZE];
	char app_file_name[MODS_ADSP_APP_NAME_SIZE];
	__u32 argc;
	__u32 argv[MODS_ADSP_APP_MAX_PARAM];
	__u32 timeout;
};

/* MODS_ESC_GET_SCREEN_INFO */
struct MODS_SCREEN_INFO {
	/* OUT */
	__u8  orig_video_mode;
	__u8  orig_video_is_vga;
	__u16 lfb_width;
	__u16 lfb_height;
	__u16 lfb_depth;
	__u32 lfb_base;
	__u32 lfb_size;
	__u16 lfb_linelength;
};

struct MODS_DMA_HANDLE {
	/* OUT */
	__u32	dma_id; /* Inditify for the DMA */
};

enum MODS_DMA_TRANSFER_DIRECTION {
	MODS_DMA_MEM_TO_MEM,
	MODS_DMA_MEM_TO_DEV,
	MODS_DMA_DEV_TO_MEM,
	MODS_DMA_DEV_TO_DEV,
	MODS_DMA_TRANS_NONE,
};

enum MODS_DMA_BUSWIDTH {
	MODS_DMA_BUSWIDTH_UNDEFINED = 0,
	MODS_DMA_BUSWIDTH_1_BYTE = 1,
	MODS_DMA_BUSWIDTH_2_BYTES = 2,
	MODS_DMA_BUSWIDTH_4_BYTES = 4,
	MODS_DMA_BUSWIDTH_8_BYTES = 8,
};

struct MODS_DMA_CHANNEL_CONFIG {
	__u64 src_addr;
	__u64 dst_addr;
	struct MODS_DMA_HANDLE handle;
	__u32 direction;
	__u32 src_addr_width;
	__u32 dst_addr_width;
	__u32 src_maxburst;
	__u32 dst_maxburst;
	__u32 slave_id;
	__u32 device_fc;
};

/* Node: Only support SINGLE MODS so far*/
enum MODS_DMA_TX_MODE {
	MODS_DMA_SINGLE = 0,
	MODS_DMA_CYCLIC,
	MODS_DMA_INTERLEAVED, /* Common to Slave as well as M2M clients. */
};

typedef __s32 mods_dma_cookie_t;

struct MODS_DMA_TX_DESC	{
	/* IN */
	__u64 phys;
	__u64 phys_2; /* only valid for MEMCPY */
	struct MODS_DMA_HANDLE handle;
	__u32 mode;
	__u32 data_dir;
	__u32 length;
	__u32 flags;
	/* OUT */
	mods_dma_cookie_t cookie;
};

enum MODS_DMA_WAIT_TYPE {
	MODS_DMA_SYNC_WAIT,	/* wait until finished */
	MODS_DMA_ASYNC_WAIT, /* just check tx status */
};

struct MODS_DMA_WAIT_DESC {
	struct MODS_DMA_HANDLE handle;
	mods_dma_cookie_t cookie;
	__u32 type;
	/* OUT */
	__u32 tx_complete;
};

#define MAX_NET_DEVICE_NAME_LENGTH 16
struct MODS_NET_DEVICE_NAME {
	/* in */
	char device_name[MAX_NET_DEVICE_NAME_LENGTH];
};

struct MODS_DMA_COHERENT_MEM_HANDLE {
	__u32	num_bytes;
	__u32	attrib;
	__u64	memory_handle_phys;
	__u64	memory_handle_virt;
};

/* MODS_ESC_DMA_COPY_TO_USER */
struct MODS_DMA_COPY_TO_USER {
	__u32	num_bytes;
	__u32	attrib;
	__u64	memory_handle_src;
	__u64	memory_handle_dst;
};

#pragma pack(pop)

/* ************************************************************************* */
/* ************************************************************************* */
/* **									     */
/* ** ESCAPE CALLS							     */
/* **									     */
/* ************************************************************************* */
/* ************************************************************************* */
#define MODS_IOC_MAGIC	  'x'
#define MODS_ESC_ALLOC_PAGES			\
		   _IOWR(MODS_IOC_MAGIC, 0, struct MODS_ALLOC_PAGES)
#define MODS_ESC_FREE_PAGES			\
		   _IOWR(MODS_IOC_MAGIC, 1, struct MODS_FREE_PAGES)
#define MODS_ESC_GET_PHYSICAL_ADDRESS		\
		   _IOWR(MODS_IOC_MAGIC, 2, struct MODS_GET_PHYSICAL_ADDRESS)
#define MODS_ESC_VIRTUAL_TO_PHYSICAL		\
		   _IOWR(MODS_IOC_MAGIC, 3, struct MODS_VIRTUAL_TO_PHYSICAL)
#define MODS_ESC_PHYSICAL_TO_VIRTUAL		\
		   _IOWR(MODS_IOC_MAGIC, 4, struct MODS_PHYSICAL_TO_VIRTUAL)
#define MODS_ESC_FIND_PCI_DEVICE		\
		   _IOWR(MODS_IOC_MAGIC, 5, struct MODS_FIND_PCI_DEVICE)
#define MODS_ESC_FIND_PCI_CLASS_CODE		\
		   _IOWR(MODS_IOC_MAGIC, 6, struct MODS_FIND_PCI_CLASS_CODE)
#define MODS_ESC_PCI_READ			\
		   _IOWR(MODS_IOC_MAGIC, 7, struct MODS_PCI_READ)
#define MODS_ESC_PCI_WRITE			\
		   _IOWR(MODS_IOC_MAGIC, 8, struct MODS_PCI_WRITE)
#define MODS_ESC_PIO_READ			\
		   _IOWR(MODS_IOC_MAGIC, 9, struct MODS_PIO_READ)
#define MODS_ESC_PIO_WRITE			\
		   _IOWR(MODS_IOC_MAGIC, 10, struct MODS_PIO_WRITE)
#define MODS_ESC_IRQ_REGISTER			\
		   _IOWR(MODS_IOC_MAGIC, 11, struct MODS_IRQ)
#define MODS_ESC_IRQ_FREE			\
		   _IOWR(MODS_IOC_MAGIC, 12, struct MODS_IRQ)
#define MODS_ESC_IRQ_INQUIRY			\
		   _IOWR(MODS_IOC_MAGIC, 13, struct MODS_IRQ)
#define MODS_ESC_EVAL_ACPI_METHOD		\
		   _IOWR(MODS_IOC_MAGIC, 16, struct MODS_EVAL_ACPI_METHOD)
#define MODS_ESC_GET_API_VERSION		\
		   _IOWR(MODS_IOC_MAGIC, 17, struct MODS_GET_VERSION)
#define MODS_ESC_GET_KERNEL_VERSION		\
		   _IOWR(MODS_IOC_MAGIC, 18, struct MODS_GET_VERSION)
#define MODS_ESC_SET_DRIVER_PARA		\
		   _IOWR(MODS_IOC_MAGIC, 19, struct MODS_SET_PARA)
#define MODS_ESC_MSI_REGISTER			\
		   _IOWR(MODS_IOC_MAGIC, 20, struct MODS_IRQ)
#define MODS_ESC_REARM_MSI			\
		   _IOWR(MODS_IOC_MAGIC, 21, struct MODS_IRQ)
#define MODS_ESC_SET_MEMORY_TYPE		\
		    _IOW(MODS_IOC_MAGIC, 22, struct MODS_MEMORY_TYPE)
#define MODS_ESC_PCI_BUS_ADD_DEVICES		\
		    _IOW(MODS_IOC_MAGIC, 23, struct MODS_PCI_BUS_ADD_DEVICES)
#define MODS_ESC_REGISTER_IRQ			\
		    _IOW(MODS_IOC_MAGIC, 24, struct MODS_REGISTER_IRQ)
#define MODS_ESC_UNREGISTER_IRQ			\
		    _IOW(MODS_IOC_MAGIC, 25, struct MODS_REGISTER_IRQ)
#define MODS_ESC_QUERY_IRQ			\
		    _IOR(MODS_IOC_MAGIC, 26, struct MODS_QUERY_IRQ)
#define MODS_ESC_EVAL_DEV_ACPI_METHOD		\
		   _IOWR(MODS_IOC_MAGIC, 27, struct MODS_EVAL_DEV_ACPI_METHOD)
#define MODS_ESC_ACPI_GET_DDC			\
		   _IOWR(MODS_IOC_MAGIC, 28, struct MODS_ACPI_GET_DDC)
#define MODS_ESC_GET_CLOCK_HANDLE		\
		   _IOWR(MODS_IOC_MAGIC, 29, struct MODS_GET_CLOCK_HANDLE)
#define MODS_ESC_SET_CLOCK_RATE			\
		    _IOW(MODS_IOC_MAGIC, 30, struct MODS_CLOCK_RATE)
#define MODS_ESC_GET_CLOCK_RATE			\
		   _IOWR(MODS_IOC_MAGIC, 31, struct MODS_CLOCK_RATE)
#define MODS_ESC_SET_CLOCK_PARENT		\
		    _IOW(MODS_IOC_MAGIC, 32, struct MODS_CLOCK_PARENT)
#define MODS_ESC_GET_CLOCK_PARENT		\
		   _IOWR(MODS_IOC_MAGIC, 33, struct MODS_CLOCK_PARENT)
#define MODS_ESC_ENABLE_CLOCK			\
		    _IOW(MODS_IOC_MAGIC, 34, struct MODS_CLOCK_HANDLE)
#define MODS_ESC_DISABLE_CLOCK			\
		    _IOW(MODS_IOC_MAGIC, 35, struct MODS_CLOCK_HANDLE)
#define MODS_ESC_IS_CLOCK_ENABLED		\
		   _IOWR(MODS_IOC_MAGIC, 36, struct MODS_CLOCK_ENABLED)
#define MODS_ESC_CLOCK_RESET_ASSERT		\
		    _IOW(MODS_IOC_MAGIC, 37, struct MODS_CLOCK_HANDLE)
#define MODS_ESC_CLOCK_RESET_DEASSERT		\
		    _IOW(MODS_IOC_MAGIC, 38, struct MODS_CLOCK_HANDLE)
#define MODS_ESC_SET_IRQ_MASK			\
		    _IOW(MODS_IOC_MAGIC, 39, struct MODS_SET_IRQ_MASK)
#define MODS_ESC_MEMORY_BARRIER			\
		     _IO(MODS_IOC_MAGIC, 40)
#define MODS_ESC_IRQ_HANDLED			\
		    _IOW(MODS_IOC_MAGIC, 41, struct MODS_REGISTER_IRQ)
#define MODS_ESC_FLUSH_CPU_CACHE_RANGE		\
		    _IOW(MODS_IOC_MAGIC, 42, struct MODS_FLUSH_CPU_CACHE_RANGE)
#define MODS_ESC_GET_CLOCK_MAX_RATE		\
		   _IOWR(MODS_IOC_MAGIC, 43, struct MODS_CLOCK_RATE)
#define MODS_ESC_SET_CLOCK_MAX_RATE		\
		    _IOW(MODS_IOC_MAGIC, 44, struct MODS_CLOCK_RATE)
#define MODS_ESC_DEVICE_ALLOC_PAGES		\
		   _IOWR(MODS_IOC_MAGIC, 45, struct MODS_DEVICE_ALLOC_PAGES)
#define MODS_ESC_DEVICE_NUMA_INFO		\
		   _IOWR(MODS_IOC_MAGIC, 46, struct MODS_DEVICE_NUMA_INFO)
#define MODS_ESC_TEGRA_DC_CONFIG_POSSIBLE	\
		   _IOWR(MODS_IOC_MAGIC, 47,	\
			 struct MODS_TEGRA_DC_CONFIG_POSSIBLE)
#define MODS_ESC_TEGRA_DC_SETUP_SD		\
		    _IOW(MODS_IOC_MAGIC, 48, struct MODS_TEGRA_DC_SETUP_SD)
#define MODS_ESC_DMABUF_GET_PHYSICAL_ADDRESS	\
		   _IOWR(MODS_IOC_MAGIC, 49,    \
			 struct MODS_DMABUF_GET_PHYSICAL_ADDRESS)
#define MODS_ESC_ADSP_LOAD			\
		     _IO(MODS_IOC_MAGIC, 50)
#define MODS_ESC_ADSP_START			\
		     _IO(MODS_IOC_MAGIC, 51)
#define MODS_ESC_ADSP_STOP			\
		     _IO(MODS_IOC_MAGIC, 52)
#define MODS_ESC_ADSP_RUN_APP			\
		    _IOW(MODS_IOC_MAGIC, 53, struct MODS_ADSP_RUN_APP_INFO)
#define MODS_ESC_PCI_GET_BAR_INFO		\
		   _IOWR(MODS_IOC_MAGIC, 54, struct MODS_PCI_GET_BAR_INFO)
#define MODS_ESC_PCI_GET_IRQ			\
		   _IOWR(MODS_IOC_MAGIC, 55, struct MODS_PCI_GET_IRQ)
#define MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS	\
		   _IOWR(MODS_IOC_MAGIC, 56,	\
			 struct MODS_GET_PHYSICAL_ADDRESS)
#define MODS_ESC_DEVICE_ALLOC_PAGES_2		\
		   _IOWR(MODS_IOC_MAGIC, 57, struct MODS_DEVICE_ALLOC_PAGES_2)
#define MODS_ESC_FIND_PCI_DEVICE_2		\
		   _IOWR(MODS_IOC_MAGIC, 58, struct MODS_FIND_PCI_DEVICE_2)
#define MODS_ESC_FIND_PCI_CLASS_CODE_2		\
		   _IOWR(MODS_IOC_MAGIC, 59,	\
			 struct MODS_FIND_PCI_CLASS_CODE_2)
#define MODS_ESC_PCI_GET_BAR_INFO_2		\
		   _IOWR(MODS_IOC_MAGIC, 60, struct MODS_PCI_GET_BAR_INFO_2)
#define MODS_ESC_PCI_GET_IRQ_2			\
		   _IOWR(MODS_IOC_MAGIC, 61, struct MODS_PCI_GET_IRQ_2)
#define MODS_ESC_PCI_READ_2			\
		   _IOWR(MODS_IOC_MAGIC, 62, struct MODS_PCI_READ_2)
#define MODS_ESC_PCI_WRITE_2			\
		    _IOW(MODS_IOC_MAGIC, 63, struct MODS_PCI_WRITE_2)
#define MODS_ESC_REGISTER_IRQ_2			\
		    _IOW(MODS_IOC_MAGIC, 64, struct MODS_REGISTER_IRQ_2)
#define MODS_ESC_UNREGISTER_IRQ_2		\
		    _IOW(MODS_IOC_MAGIC, 65, struct MODS_REGISTER_IRQ_2)
#define MODS_ESC_IRQ_HANDLED_2			\
		    _IOW(MODS_IOC_MAGIC, 66, struct MODS_REGISTER_IRQ_2)
#define MODS_ESC_QUERY_IRQ_2			\
		    _IOR(MODS_IOC_MAGIC, 67, struct MODS_QUERY_IRQ_2)
#define MODS_ESC_SET_IRQ_MASK_2			\
		    _IOW(MODS_IOC_MAGIC, 68, struct MODS_SET_IRQ_MASK_2)
#define MODS_ESC_EVAL_DEV_ACPI_METHOD_2		\
		   _IOWR(MODS_IOC_MAGIC, 69,	\
			 struct MODS_EVAL_DEV_ACPI_METHOD_2)
#define MODS_ESC_DEVICE_NUMA_INFO_2		\
		   _IOWR(MODS_IOC_MAGIC, 70, struct MODS_DEVICE_NUMA_INFO_2)
#define MODS_ESC_ACPI_GET_DDC_2			\
		   _IOWR(MODS_IOC_MAGIC, 71, struct MODS_ACPI_GET_DDC_2)
#define MODS_ESC_GET_SCREEN_INFO			\
		    _IOR(MODS_IOC_MAGIC, 72, struct MODS_SCREEN_INFO)
#define MODS_ESC_PCI_HOT_RESET			\
		    _IOW(MODS_IOC_MAGIC, 73, struct MODS_PCI_HOT_RESET)
#define MODS_ESC_SET_PPC_TCE_BYPASS			\
		    _IOWR(MODS_IOC_MAGIC, 74, struct MODS_SET_PPC_TCE_BYPASS)
#define MODS_ESC_DMA_MAP_MEMORY			\
		    _IOW(MODS_IOC_MAGIC, 75, struct MODS_DMA_MAP_MEMORY)
#define MODS_ESC_DMA_UNMAP_MEMORY			\
		    _IOW(MODS_IOC_MAGIC, 76, struct MODS_DMA_MAP_MEMORY)
#define MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_2			\
		    _IOWR(MODS_IOC_MAGIC, 77,                   \
			  struct MODS_GET_PHYSICAL_ADDRESS_2)
#define MODS_ESC_PCI_MAP_RESOURCE			\
		    _IOWR(MODS_IOC_MAGIC, 78, struct MODS_PCI_MAP_RESOURCE)
#define MODS_ESC_PCI_UNMAP_RESOURCE			\
		    _IOW(MODS_IOC_MAGIC, 79, struct MODS_PCI_UNMAP_RESOURCE)
#define MODS_ESC_DMA_REQUEST_HANDLE		\
		   _IOR(MODS_IOC_MAGIC,  80, struct MODS_DMA_HANDLE)
#define MODS_ESC_DMA_RELEASE_HANDLE		\
		   _IOW(MODS_IOC_MAGIC,  81, struct MODS_DMA_HANDLE)
#define MODS_ESC_DMA_SET_CONFIG			\
		   _IOW(MODS_IOC_MAGIC,  82, struct MODS_DMA_CHANNEL_CONFIG)
#define MODS_ESC_DMA_TX_SUBMIT			\
		   _IOW(MODS_IOC_MAGIC,  83, struct MODS_DMA_TX_DESC)
#define MODS_ESC_DMA_TX_WAIT			\
		   _IOWR(MODS_IOC_MAGIC, 84, struct MODS_DMA_WAIT_DESC)
#define MODS_ESC_DMA_ISSUE_PENDING		\
		   _IOW(MODS_IOC_MAGIC,  85, struct MODS_DMA_HANDLE)
#define MODS_ESC_SET_IRQ_MULTIMASK			\
		    _IOW(MODS_IOC_MAGIC, 86, struct MODS_SET_IRQ_MULTIMASK)
#define MODS_ESC_NET_FORCE_LINK			\
		    _IOW(MODS_IOC_MAGIC, 87, struct MODS_NET_DEVICE_NAME)
#define MODS_ESC_REGISTER_IRQ_3			\
		    _IOW(MODS_IOC_MAGIC, 88, struct MODS_REGISTER_IRQ_3)
#define MODS_ESC_DMA_ALLOC_COHERENT			\
		   _IOWR(MODS_IOC_MAGIC, 89,		\
		   struct MODS_DMA_COHERENT_MEM_HANDLE)
#define MODS_ESC_DMA_FREE_COHERENT			\
		   _IOWR(MODS_IOC_MAGIC, 90,		\
		   struct MODS_DMA_COHERENT_MEM_HANDLE)
#define MODS_ESC_DMA_COPY_TO_USER			\
		   _IOWR(MODS_IOC_MAGIC, 91,		\
		   struct MODS_DMA_COPY_TO_USER)
#endif /* _MODS_H_  */
