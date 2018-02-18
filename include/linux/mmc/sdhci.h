/*
 *  linux/include/linux/mmc/sdhci.h - Secure Digital Host Controller Interface
 *
 *  Copyright (C) 2005-2008 Pierre Ossman, All Rights Reserved.
 *
 *  Copyright (c) 2013-2016, NVIDIA CORPORATION. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */
#ifndef LINUX_MMC_SDHCI_H
#define LINUX_MMC_SDHCI_H

#include <linux/scatterlist.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/mmc/host.h>
#include <linux/sysedp.h>

#define ADMA_DESC_ALLOC_SIZE ((128 * 2 + 1) * 8)
#define ADMA_ALIGN_BUFFER_SIZE (128 * 8)

#ifdef CONFIG_DEBUG_FS
struct data_stat_entry {
	u32 max_kbps;
	u32 min_kbps;
	ktime_t start_ktime;
	u32 duration_usecs;
	u64 total_usecs;
	u32 total_transfers;
	u32 current_transferred_bytes;
	u64 total_bytes;
	u32 stat_blk_size;
	u32 stat_blks_per_transfer;
	bool is_read;
	struct data_stat_entry *next;
};

/*
 * 1. Each block size has a element of type struct data_stat_entry
 * 2. For a particular block size we maintain a table of
 *    performance values observed. This table is used to
 *    find the most frequent performance value
 */
struct data_stat {
	/*
	 * use insertion sort to keep the list sorted with
	 * increasing block size
	 */
	struct data_stat_entry *head;
	/* actual number of stat entries */
	u8	stat_size;
};
#endif

#ifdef CONFIG_MMC_ADMA3
struct sdhci_adma3_desc_pair {
	u64 cmd_desc[4];
	u8 adma_desc[ADMA_DESC_ALLOC_SIZE];
};
#endif

struct sdhci_adma_info {
#ifdef CONFIG_MMC_ADMA3
	struct sdhci_adma3_desc_pair *adma3_desc;
#else
	u8 *adma_desc;
#endif
	dma_addr_t adma_addr;

	u8 *align_buffer;	/* Bounce buffer */
	dma_addr_t align_addr;	/* Mapped bounce buffer */

	int sg_count;		/* Mapped sg entries */
};

#ifdef CONFIG_MMC_ADMA3
#define DEFAULT_ADMA3_Q_ALLOC_SIZE 16
struct adma3_cmd_node {
	struct mmc_command *cmd;
	struct mmc_data *data;
	unsigned int  command;
	u16 mode;
	struct mmc_request *mrq;
	struct sdhci_adma_info *adma_info;
	struct adma3_cmd_node *next;
};
#endif

struct sdhci_host {
	bool is_reset_once;
	bool wake_enable_failed;
	bool disable_command_to_media;
#ifdef CONFIG_MMC_ADMA3
	atomic_t is_adma3_cmd_issued;
	struct adma3_cmd_node *last_adma3_entry;
	unsigned adma3_index;
	bool is_cq_check_done;
	bool is_adma_mode_printed;
	unsigned int last_flush_size;
	struct adma3_cmd_node
		adma3_data_cmd_array[MMC_QUEUE_FLUSH_ADMA3_DEPTH];
	unsigned int queue_length;
	unsigned int size_integ_desc_entry;
	struct work_struct	adma3_post_wrk;
	unsigned int saved_last_data_error;
#endif
	bool	enable_64bit_addr;
	bool	enable_adma2_26bit;
	unsigned int	data_len;

	/* Data set by hardware interface driver */
	const char *hw_name;	/* Hardware bus name */
	unsigned int quirks;	/* Deviations from spec. */
#ifdef CONFIG_DEBUG_FS
	struct dentry           *debugfs_root;
	/* collect data transfer rate statistics */
	struct data_stat sdhci_data_stat;
	unsigned int no_data_transfer_count;
#endif

/* Controller doesn't honor resets unless we touch the clock register */
#define SDHCI_QUIRK_CLOCK_BEFORE_RESET			(1<<0)
/* Controller has bad caps bits, but really supports DMA */
#define SDHCI_QUIRK_FORCE_DMA				(1<<1)
/* Controller doesn't like to be reset when there is no card inserted. */
#define SDHCI_QUIRK_NO_CARD_NO_RESET			(1<<2)
/* Controller doesn't like clearing the power reg before a change */
#define SDHCI_QUIRK_SINGLE_POWER_WRITE			(1<<3)
/* Controller has flaky internal state so reset it on each ios change */
#define SDHCI_QUIRK_RESET_CMD_DATA_ON_IOS		(1<<4)
/* Controller has an unusable DMA engine */
#define SDHCI_QUIRK_BROKEN_DMA				(1<<5)
/* Controller has an unusable ADMA engine */
#define SDHCI_QUIRK_BROKEN_ADMA				(1<<6)
/* Controller can only DMA from 32-bit aligned addresses */
#define SDHCI_QUIRK_32BIT_DMA_ADDR			(1<<7)
/* Controller can only DMA chunk sizes that are a multiple of 32 bits */
#define SDHCI_QUIRK_32BIT_DMA_SIZE			(1<<8)
/* Controller can only ADMA chunks that are a multiple of 32 bits */
#define SDHCI_QUIRK_32BIT_ADMA_SIZE			(1<<9)
/* Controller needs to be reset after each request to stay stable */
#define SDHCI_QUIRK_RESET_AFTER_REQUEST			(1<<10)
/* Controller needs voltage and power writes to happen separately */
#define SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER		(1<<11)
/* Controller provides an incorrect timeout value for transfers */
#define SDHCI_QUIRK_BROKEN_TIMEOUT_VAL			(1<<12)
/* Controller has an issue with buffer bits for small transfers */
#define SDHCI_QUIRK_BROKEN_SMALL_PIO			(1<<13)
/* Controller does not provide transfer-complete interrupt when not busy */
#define SDHCI_QUIRK_NO_BUSY_IRQ				(1<<14)
/* Controller has unreliable card detection */
#define SDHCI_QUIRK_BROKEN_CARD_DETECTION		(1<<15)
/* Controller reports inverted write-protect state */
#define SDHCI_QUIRK_INVERTED_WRITE_PROTECT		(1<<16)
/* Controller does not like fast PIO transfers */
#define SDHCI_QUIRK_PIO_NEEDS_DELAY			(1<<18)
/* Controller has to be forced to use block size of 2048 bytes */
#define SDHCI_QUIRK_FORCE_BLK_SZ_2048			(1<<20)
/* Controller cannot do multi-block transfers */
#define SDHCI_QUIRK_NO_MULTIBLOCK			(1<<21)
/* Controller can only handle 1-bit data transfers */
#define SDHCI_QUIRK_FORCE_1_BIT_DATA			(1<<22)
/* Controller needs 10ms delay between applying power and clock */
#define SDHCI_QUIRK_DELAY_AFTER_POWER			(1<<23)
/* Controller uses SDCLK instead of TMCLK for data timeouts */
#define SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK		(1<<24)
/* Controller reports wrong base clock capability */
#define SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN		(1<<25)
/* Controller cannot support End Attribute in NOP ADMA descriptor */
#define SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC		(1<<26)
/* Controller is missing device caps. Use caps provided by host */
#define SDHCI_QUIRK_MISSING_CAPS			(1<<27)
/* Controller uses Auto CMD12 command to stop the transfer */
#define SDHCI_QUIRK_MULTIBLOCK_READ_ACMD12		(1<<28)
/* Controller doesn't have HISPD bit field in HI-SPEED SD card */
#define SDHCI_QUIRK_NO_HISPD_BIT			(1<<29)
/* Controller treats ADMA descriptors with length 0000h incorrectly */
#define SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC		(1<<30)
/* The read-only detection via SDHCI_PRESENT_STATE register is unstable */
#define SDHCI_QUIRK_UNSTABLE_RO_DETECT			(1<<31)

	unsigned int quirks2;	/* More deviations from spec. */

#define SDHCI_QUIRK2_HOST_OFF_CARD_ON			(1<<0)
#define SDHCI_QUIRK2_HOST_NO_CMD23			(1<<1)
/* The system physically doesn't support 1.8v, even if the host does */
#define SDHCI_QUIRK2_NO_1_8_V				(1<<2)
#define SDHCI_QUIRK2_PRESET_VALUE_BROKEN		(1<<3)
#define SDHCI_QUIRK2_CARD_ON_NEEDS_BUS_ON		(1<<4)
/* Controller has a non-standard host control register */
#define SDHCI_QUIRK2_BROKEN_HOST_CONTROL		(1<<5)
/* Controller does not support HS200 */
#define SDHCI_QUIRK2_BROKEN_HS200			(1<<6)
/* Controller does not support DDR50 */
#define SDHCI_QUIRK2_BROKEN_DDR50			(1<<7)
/* Stop command (CMD12) can set Transfer Complete when not using MMC_RSP_BUSY */
#define SDHCI_QUIRK2_STOP_WITH_TC			(1<<8)
/* Controller supports 64 BIT DMA mode */
#define SDHCI_QUIRK2_SUPPORT_64BIT_DMA			(1<<9)
/* Use 64 BIT addressing */
#define SDHCI_QUIRK2_USE_64BIT_ADDR			(1<<10)
/* Turn off/on card clock before sending/after tuning command*/
#define SDHCI_QUIRK2_NON_STD_TUN_CARD_CLOCK		(1<<13)
#define SDHCI_QUIRK2_NON_STD_TUNING_LOOP_CNTR		(1<<14)
#define SDHCI_QUIRK2_PERIODIC_CALIBRATION		(1<<15)
#define SDHCI_QUIRK2_DISABLE_CARD_CLOCK_FIRST		(1<<16)
/*Controller skips tuning if it is already done*/
#define SDHCI_QUIRK2_SKIP_TUNING			(1<<17)
#define SDHCI_QUIRK2_USE_32BIT_BLK_COUNT		(1<<18)
#define SDHCI_QUIRK2_BROKEN_ADMA3			(1<<19)
#define SDHCI_QUIRK2_ADMA2_26BIT_DATA_LEN		(1<<20)

	unsigned int  acmd12_ctrl;
	unsigned int  command;
	int irq;		/* Device IRQ */
	void __iomem *ioaddr;	/* Mapped address */

	const struct sdhci_ops *ops;	/* Low level hw interface */

	/* Internal data */
	struct mmc_host *mmc;	/* MMC structure */
	u64 dma_mask;		/* custom DMA mask */

#if defined(CONFIG_LEDS_CLASS) || defined(CONFIG_LEDS_CLASS_MODULE)
	struct led_classdev led;	/* LED control */
	char led_name[32];
#endif

	spinlock_t lock;	/* Mutex */

	int flags;		/* Host attributes */
#define SDHCI_USE_SDMA		(1<<0)	/* Host is SDMA capable */
#define SDHCI_USE_ADMA		(1<<1)	/* Host is ADMA capable */
#define SDHCI_REQ_USE_DMA	(1<<2)	/* Use DMA for this req. */
#define SDHCI_DEVICE_DEAD	(1<<3)	/* Device unresponsive */
#define SDHCI_SDR50_NEEDS_TUNING (1<<4)	/* SDR50 needs tuning */
#define SDHCI_NEEDS_RETUNING	(1<<5)	/* Host needs retuning */
#define SDHCI_AUTO_CMD12	(1<<6)	/* Auto CMD12 support */
#define SDHCI_AUTO_CMD23	(1<<7)	/* Auto CMD23 support */
#define SDHCI_PV_ENABLED	(1<<8)	/* Preset value enabled */
#define SDHCI_SDIO_IRQ_ENABLED	(1<<9)	/* SDIO irq enabled */
#define SDHCI_SDR104_NEEDS_TUNING (1<<10)	/* SDR104/HS200 needs tuning */
#define SDHCI_USING_RETUNING_TIMER (1<<11)	/* Host is using a retuning timer for the card */
#define SDHCI_USE_ADMA_64BIT	(1<<12)	/* Host is 64bit ADMA capable */
#define SDHCI_USE_ADMA3		(1<<13)	/* Host is ADMA3 capable */
#define SDHCI_USE_32BIT_BLK_COUNT	(1<<14)	/* Host supports 32bit block count */
#define SDHCI_FORCE_PIO_MODE	(1<<15)	/* Force host to use PIO mode */

	unsigned int version;	/* SDHCI spec. version */

	unsigned int max_clk;	/* Max possible freq (MHz) */
	unsigned int timeout_clk;	/* Timeout freq (KHz) */
	unsigned int clk_mul;	/* Clock Muliplier value */

	unsigned int clock;	/* Current clock (MHz) */
	u8 pwr;			/* Current voltage */

	bool runtime_suspended;	/* Host is runtime suspended */
	bool bus_on;		/* Bus power prevents runtime suspend */
	bool preset_enabled;	/* Preset is enabled */

	struct mmc_request *mrq_cmd;	/* Current request */
	struct mmc_request *mrq_dat;	/* Current request with data*/
	struct mmc_command *cmd;	/* Current command */
	struct mmc_data *data;	/* Current data request */
	unsigned int data_early:1;	/* Data finished before cmd */
	unsigned int busy_handle:1;	/* Handling the order of Busy-end */

	struct sg_mapping_iter sg_miter;	/* SG state for PIO */
	unsigned int blocks;	/* remaining PIO blocks */
	unsigned int max_pio_size;
	unsigned int max_pio_blocks;

#ifdef CONFIG_MMC_ADMA3
	struct sdhci_adma_info adma_info[DEFAULT_ADMA3_Q_ALLOC_SIZE];
	unsigned int current_adma_info_index;
#else
	struct sdhci_adma_info adma_info;
#endif
	u8 *int_desc;		/* ADMA3 integrated descriptor table */
	dma_addr_t int_adma_addr; /* Mapped ADMA3 integrated descr. table */
#ifndef CONFIG_MMC_ADMA3
	u8 *cmd_desc;		/* ADMA3 command descriptor table */
	dma_addr_t cmd_adma_addr; /* Mapped ADMA3 command descr. table */
#endif

	struct tasklet_struct finish_tasklet;	/* Tasklet structures */
	struct tasklet_struct finish_dat_tasklet;
	struct tasklet_struct finish_cmd_tasklet;
#ifdef CONFIG_MMC_ADMA3
	struct tasklet_struct finish_adma3_tasklet;
#endif

	struct timer_list timer;	/* Timer for timeouts */

	u32 caps;		/* Alternative CAPABILITY_0 */
	u32 caps1;		/* Alternative CAPABILITY_1 */
	u32 caps_timing_orig;	/* Save the original host timing caps*/

	unsigned int            ocr_avail_sdio;	/* OCR bit masks */
	unsigned int            ocr_avail_sd;
	unsigned int            ocr_avail_mmc;
	u32 ocr_mask;		/* available voltages */

	unsigned		timing;		/* Current timing */

	u32			thread_isr;

	/* cached registers */
	u32			ier;

	wait_queue_head_t	buf_ready_int;	/* Waitqueue for Buffer Read Ready interrupt */
	unsigned int		tuning_done;	/* Condition flag set when CMD19 succeeds */

	unsigned int		tuning_count;	/* Timer count for re-tuning */
	unsigned int		tuning_mode;	/* Re-tuning mode supported by host */
#define SDHCI_TUNING_MODE_1	0
	struct timer_list	tuning_timer;	/* Timer for tuning */

	struct cmdq_host *cq_host;
	struct sysedp_consumer *sysedpc;

#ifdef CONFIG_DEBUG_FS
	bool			enable_sdhci_perf_stats;
#endif
	ktime_t timestamp;
	bool is_calibration_done;

	unsigned long private[0] ____cacheline_aligned;
};
#endif /* LINUX_MMC_SDHCI_H */
