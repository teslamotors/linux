/*
 * os.c
 *
 * ADSP OS management
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * Copyright (C) 2014-2016 NVIDIA Corporation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/tegra_nvadsp.h>
#include <linux/tegra-soc.h>
#include <linux/elf.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/irqchip/tegra-agic.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/tegra-firmwares.h>

#include <asm/uaccess.h>

#include "ape_actmon.h"
#include "os.h"
#include "dev.h"
#include "dram_app_mem_manager.h"
#include "adsp_console_dbfs.h"
#include "hwmailbox.h"

#define NVADSP_ELF "adsp.elf"
#define NVADSP_FIRMWARE NVADSP_ELF

#define MAILBOX_REGION		".mbox_shared_data"
#define DEBUG_RAM_REGION	".debug_mem_logs"

/* Maximum number of LOAD MAPPINGS supported */
#define NM_LOAD_MAPPINGS 20

#define EOT	0x04 /* End of Transmission */
#define SOH	0x01 /* Start of Header */

#define ADSP_TAG	"\n[ADSP OS]"

#define UART_BAUD_RATE	9600

/* Intiialize with FIXED rate, once OS boots up DFS will set required freq */
#define ADSP_TO_APE_CLK_RATIO	2
/* 13.5 MHz, should be changed at bringup time */
#define APE_CLK_FIX_RATE	13500
/*
 * ADSP CLK = APE_CLK * ADSP_TO_APE_CLK_RATIO
 * or
 * ADSP CLK = APE_CLK >> ADSP_TO_APE_CLK_RATIO
 */
#define ADSP_CLK_FIX_RATE (APE_CLK_FIX_RATE * ADSP_TO_APE_CLK_RATIO)

/* total number of crashes allowed on adsp */
#define ALLOWED_CRASHES	1

#define DISABLE_MBOX2_FULL_INT	0x0
#define ENABLE_MBOX2_FULL_INT	0xFFFFFFFF

#define LOGGER_TIMEOUT		20 /* in ms */
#define ADSP_WFE_TIMEOUT	5000 /* in ms */
#define LOGGER_COMPLETE_TIMEOUT	5000 /* in ms */

#define DUMP_BUFF 128

struct nvadsp_debug_log {
	struct device		*dev;
	char			*debug_ram_rdr;
	int			debug_ram_sz;
	int			ram_iter;
	atomic_t		is_opened;
	wait_queue_head_t	wait_queue;
	struct completion	complete;
};

struct nvadsp_os_data {
	void __iomem		*unit_fpga_reset_reg;
	const struct firmware	*os_firmware;
	struct platform_device	*pdev;
	struct global_sym_info	*adsp_glo_sym_tbl;
	void __iomem		*hwmailbox_base;
	struct resource		**dram_region;
	struct nvadsp_debug_log	logger;
	struct nvadsp_cnsl   console;
	struct work_struct	restart_os_work;
	int			adsp_num_crashes;
	bool			adsp_os_fw_loaded;
	struct mutex		fw_load_lock;
	bool			os_running;
	struct mutex		os_run_lock;
	dma_addr_t		adsp_os_addr;
	size_t			adsp_os_size;
	dma_addr_t		app_alloc_addr;
	size_t			app_size;
};

static struct nvadsp_os_data priv;

struct nvadsp_mappings {
	phys_addr_t da;
	void *va;
	int len;
};

static struct nvadsp_mappings adsp_map[NM_LOAD_MAPPINGS];
static int map_idx;
static struct nvadsp_mbox adsp_com_mbox;

static DECLARE_COMPLETION(entered_wfi);

static void __nvadsp_os_stop(bool);

#ifdef CONFIG_DEBUG_FS
static int adsp_logger_open(struct inode *inode, struct file *file)
{
	struct nvadsp_debug_log *logger = inode->i_private;
	struct nvadsp_os_data *os_data;
	int ret = -EBUSY;
	char *start;

	os_data = container_of(logger, struct nvadsp_os_data, logger);

	/*
	 * checks if os_opened decrements to zero and if returns true. If true
	 * then there has been no open.
	*/
	if (!atomic_dec_and_test(&logger->is_opened)) {
		atomic_inc(&logger->is_opened);
		goto err_ret;
	}

	ret = wait_event_interruptible(logger->wait_queue,
				os_data->adsp_os_fw_loaded);
	if (ret == -ERESTARTSYS)  /* check if interrupted */
		goto err;

	/* loop till writer is initilized with SOH */
	do {

		ret = wait_event_interruptible_timeout(logger->wait_queue,
			memchr(logger->debug_ram_rdr, SOH,
			logger->debug_ram_sz),
			msecs_to_jiffies(LOGGER_TIMEOUT));
		if (ret == -ERESTARTSYS)  /* check if interrupted */
			goto err;

		start = memchr(logger->debug_ram_rdr, SOH,
			logger->debug_ram_sz);
	} while (!start);

	/* maxdiff can be 0, therefore valid */
	logger->ram_iter = start - logger->debug_ram_rdr;

	file->private_data = logger;
	return 0;
err:
	/* reset to 1 so as to mention the node is free */
	atomic_set(&logger->is_opened, 1);
err_ret:
	return ret;
}


static int adsp_logger_flush(struct file *file, fl_owner_t id)
{
	struct nvadsp_debug_log *logger = file->private_data;
	struct device *dev = logger->dev;

	dev_dbg(dev, "%s\n", __func__);

	/* reset to 1 so as to mention the node is free */
	atomic_set(&logger->is_opened, 1);
	return 0;
}

static int adsp_logger_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t adsp_logger_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct nvadsp_debug_log *logger = file->private_data;
	struct device *dev = logger->dev;
	ssize_t ret_num_char = 1;
	char last_char;

loop:
	last_char = logger->debug_ram_rdr[logger->ram_iter];

	if ((last_char != EOT) && (last_char != 0)) {
#if CONFIG_ADSP_DRAM_LOG_WITH_TAG
		if ((last_char == '\n') || (last_char == '\r')) {

			if (copy_to_user(buf, ADSP_TAG, sizeof(ADSP_TAG) - 1)) {
				dev_err(dev, "%s failed\n", __func__);
				ret_num_char = -EFAULT;
				goto exit;
			}
			ret_num_char = sizeof(ADSP_TAG) - 1;

		} else
#endif
		if (copy_to_user(buf, &last_char, 1)) {
			dev_err(dev, "%s failed\n", __func__);
			ret_num_char = -EFAULT;
			goto exit;
		}

		logger->ram_iter =
			(logger->ram_iter + 1) % logger->debug_ram_sz;
		goto exit;
	}

	complete(&logger->complete);
	ret_num_char = wait_event_interruptible_timeout(logger->wait_queue,
		logger->debug_ram_rdr[logger->ram_iter] != EOT,
		msecs_to_jiffies(LOGGER_TIMEOUT));
	if (ret_num_char == -ERESTARTSYS) {
		goto exit;
	}
	goto loop;

exit:
	return ret_num_char;
}

static const struct file_operations adsp_logger_operations = {
	.read		= adsp_logger_read,
	.open		= adsp_logger_open,
	.release	= adsp_logger_release,
	.llseek		= generic_file_llseek,
	.flush		= adsp_logger_flush,
};

static int adsp_create_debug_logger(struct dentry *adsp_debugfs_root)
{
	struct nvadsp_debug_log *logger = &priv.logger;
	struct device *dev = &priv.pdev->dev;
	int ret = 0;

	if (IS_ERR_OR_NULL(adsp_debugfs_root)) {
		ret = -ENOENT;
		goto err_out;
	}

	atomic_set(&logger->is_opened, 1);
	init_waitqueue_head(&logger->wait_queue);
	init_completion(&logger->complete);
	if (!debugfs_create_file("adsp_logger", S_IRUGO,
					adsp_debugfs_root, logger,
					&adsp_logger_operations)) {
		dev_err(dev, "unable to create adsp logger debug fs file\n");
		ret = -ENOENT;
	}

err_out:
	return ret;
}
#endif

bool is_adsp_dram_addr(u64 addr)
{
	int i;
	struct resource **dram = priv.dram_region;

	for (i = 0; i < ADSP_MAX_DRAM_MAP; i++) {
		if ((dram[i]->start) && (addr >= dram[i]->start) &&
			(addr <= dram[i]->end))
			return true;
	}
	return false;
}

int adsp_add_load_mappings(phys_addr_t pa, void *mapping, int len)
{
	if (map_idx >= NM_LOAD_MAPPINGS)
		return -EINVAL;

	adsp_map[map_idx].da = pa;
	adsp_map[map_idx].va = mapping;
	adsp_map[map_idx].len = len;
	map_idx++;
	return 0;
}

void *nvadsp_da_to_va_mappings(u64 da, int len)
{
	void *ptr = NULL;
	int i;

	for (i = 0; i < map_idx; i++) {
		int offset = da - adsp_map[i].da;

		/* try next carveout if da is too small */
		if (offset < 0)
			continue;

		/* try next carveout if da is too large */
		if (offset + len > adsp_map[i].len)
			continue;

		ptr = adsp_map[i].va + offset;
		break;
	}
	return ptr;
}
EXPORT_SYMBOL(nvadsp_da_to_va_mappings);

void *nvadsp_alloc_coherent(size_t size, dma_addr_t *da, gfp_t flags)
{
	struct device *dev;
	void *va = NULL;

	if (!priv.pdev) {
		pr_err("ADSP Driver is not initialized\n");
		goto end;
	}

	dev = &priv.pdev->dev;
	va = dma_alloc_coherent(dev, size, da, flags);
	if (!va) {
		dev_err(dev, "unable to allocate the memory for size %lu\n",
				size);
		goto end;
	}
	WARN(!is_adsp_dram_addr(*da), "bus addr %llx beyond %x\n",
				*da, UINT_MAX);
end:
	return va;
}
EXPORT_SYMBOL(nvadsp_alloc_coherent);

void nvadsp_free_coherent(size_t size, void *va, dma_addr_t da)
{
	struct device *dev;

	if (!priv.pdev) {
		pr_err("ADSP Driver is not initialized\n");
		return;
	}
	dev = &priv.pdev->dev;
	dma_free_coherent(dev, size, va, da);
}
EXPORT_SYMBOL(nvadsp_free_coherent);

struct elf32_shdr *
nvadsp_get_section(const struct firmware *fw, char *sec_name)
{
	int i;
	struct device *dev = &priv.pdev->dev;
	const u8 *elf_data = fw->data;
	struct elf32_hdr *ehdr = (struct elf32_hdr *)elf_data;
	struct elf32_shdr *shdr;
	const char *name_table;

	/* look for the resource table and handle it */
	shdr = (struct elf32_shdr *)(elf_data + ehdr->e_shoff);
	name_table = elf_data + shdr[ehdr->e_shstrndx].sh_offset;

	for (i = 0; i < ehdr->e_shnum; i++, shdr++)
		if (!strcmp(name_table + shdr->sh_name, sec_name)) {
			dev_dbg(dev, "found the section %s\n",
					name_table + shdr->sh_name);
			return shdr;
		}
	return NULL;
}

static inline void __maybe_unused dump_global_symbol_table(void)
{
	struct device *dev = &priv.pdev->dev;
	struct global_sym_info *table = priv.adsp_glo_sym_tbl;
	int num_ent;
	int i;

	if (!table) {
		dev_err(dev, "no table not created\n");
		return;
	}
	num_ent = table[0].addr;
	dev_info(dev, "total number of entries in global symbol table %d\n",
			num_ent);

	pr_info("NAME ADDRESS TYPE\n");
	for (i = 1; i < num_ent; i++)
		pr_info("%s %x %s\n", table[i].name, table[i].addr,
			ELF32_ST_TYPE(table[i].info) == STT_FUNC ?
				"STT_FUNC" : "STT_OBJECT");
}

static int
create_global_symbol_table(const struct firmware *fw)
{
	int i;
	struct device *dev = &priv.pdev->dev;
	struct elf32_shdr *sym_shdr = nvadsp_get_section(fw, ".symtab");
	struct elf32_shdr *str_shdr = nvadsp_get_section(fw, ".strtab");
	const u8 *elf_data = fw->data;
	const char *name_table;
	/* The first entry stores the number of entries in the array */
	int num_ent = 1;
	struct elf32_sym *sym;
	struct elf32_sym *last_sym;

	sym = (struct elf32_sym *)(elf_data + sym_shdr->sh_offset);
	name_table = elf_data + str_shdr->sh_offset;

	num_ent += sym_shdr->sh_size / sizeof(struct elf32_sym);
	priv.adsp_glo_sym_tbl = devm_kzalloc(dev,
		sizeof(struct global_sym_info) * num_ent, GFP_KERNEL);
	if (!priv.adsp_glo_sym_tbl)
		return -ENOMEM;

	last_sym = sym + num_ent;

	for (i = 1; sym < last_sym; sym++) {
		unsigned char info = sym->st_info;
		unsigned char type = ELF32_ST_TYPE(info);
		if ((ELF32_ST_BIND(sym->st_info) == STB_GLOBAL) &&
		((type == STT_OBJECT) || (type == STT_FUNC))) {
			char *name = priv.adsp_glo_sym_tbl[i].name;

			strlcpy(name, name_table + sym->st_name, SYM_NAME_SZ);
			priv.adsp_glo_sym_tbl[i].addr = sym->st_value;
			priv.adsp_glo_sym_tbl[i].info = info;
			i++;
		}
	}
	priv.adsp_glo_sym_tbl[0].addr = i;
	return 0;
}

struct global_sym_info *find_global_symbol(const char *sym_name)
{
	struct device *dev = &priv.pdev->dev;
	struct global_sym_info *table = priv.adsp_glo_sym_tbl;
	int num_ent;
	int i;

	if (unlikely(!table)) {
		dev_err(dev, "symbol table not present\n");
		return NULL;
	}
	num_ent = table[0].addr;

	for (i = 1; i < num_ent; i++) {
		if (!strncmp(table[i].name, sym_name, SYM_NAME_SZ))
			return &table[i];
	}
	return NULL;
}

static void *get_mailbox_shared_region(const struct firmware *fw)
{
	struct device *dev;
	struct elf32_shdr *shdr;
	int addr;
	int size;

	if (!priv.pdev) {
		pr_err("ADSP Driver is not initialized\n");
		return ERR_PTR(-EINVAL);
	}

	dev = &priv.pdev->dev;

	shdr = nvadsp_get_section(fw, MAILBOX_REGION);
	if (!shdr) {
		dev_err(dev, "section %s not found\n", MAILBOX_REGION);
		return ERR_PTR(-EINVAL);
	}

	dev_dbg(dev, "the shared section is present at 0x%x\n", shdr->sh_addr);
	addr = shdr->sh_addr;
	size = shdr->sh_size;
	return nvadsp_da_to_va_mappings(addr, size);
}

static void copy_io_in_l(void *to, const void *from, int sz)
{
	int i;
	for (i = 0; i < sz; i += 4) {
		int val = *(int *)(from + i);
		writel(val, to + i);
	}
}

static int nvadsp_os_elf_load(const struct firmware *fw)
{
	struct device *dev = &priv.pdev->dev;
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(priv.pdev);
	struct elf32_hdr *ehdr;
	struct elf32_phdr *phdr;
	int i, ret = 0;
	const u8 *elf_data = fw->data;

	ehdr = (struct elf32_hdr *)elf_data;
	phdr = (struct elf32_phdr *)(elf_data + ehdr->e_phoff);

	/* go through the available ELF segments */
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		void *va;
		u32 da = phdr->p_paddr;
		u32 memsz = phdr->p_memsz;
		u32 filesz = phdr->p_filesz;
		u32 offset = phdr->p_offset;

		if (phdr->p_type != PT_LOAD)
			continue;

		dev_dbg(dev, "phdr: type %d da 0x%x memsz 0x%x filesz 0x%x\n",
				phdr->p_type, da, memsz, filesz);

		va = nvadsp_da_to_va_mappings(da, filesz);
		if (!va) {
			dev_err(dev, "no va for da 0x%x filesz 0x%x\n",
					da, filesz);
			ret = -EINVAL;
			break;
		}

		if (filesz > memsz) {
			dev_err(dev, "bad phdr filesz 0x%x memsz 0x%x\n",
					filesz, memsz);
			ret = -EINVAL;
			break;
		}

		if (offset + filesz > fw->size) {
			dev_err(dev, "truncated fw: need 0x%x avail 0x%zx\n",
					offset + filesz, fw->size);
			ret = -EINVAL;
			break;
		}

		/* put the segment where the remote processor expects it */
		if (filesz) {
			if (is_adsp_dram_addr(da))
				memcpy(va, elf_data + offset, filesz);
			else if ((da == drv_data->evp_base[ADSP_EVP_BASE]) &&
				(filesz == drv_data->evp_base[ADSP_EVP_SIZE])) {

				drv_data->state.evp_ptr = va;
				memcpy(drv_data->state.evp,
					elf_data + offset, filesz);
			} else {
				dev_err(dev, "can't load mem pa:0x%x va:%p\n",
						da, va);
				ret = -EINVAL;
				break;
			}
		}
	}

	return ret;
}

static int allocate_memory_for_adsp_os(void)
{
	struct platform_device *pdev = priv.pdev;
	struct device *dev = &pdev->dev;
#if defined(CONFIG_TEGRA_NVADSP_ON_SMMU)
	dma_addr_t addr;
#else
	phys_addr_t addr;
#endif
	void *dram_va;
	size_t size;
	int ret = 0;

	addr = priv.adsp_os_addr;
	size = priv.adsp_os_size;
#if defined(CONFIG_TEGRA_NVADSP_ON_SMMU)
	dram_va = dma_alloc_at_coherent(dev, size, &addr, GFP_KERNEL);
	if (!dram_va) {
		dev_err(dev, "unable to allocate SMMU pages\n");
		ret = -ENOMEM;
		goto end;
	}
#else
	dram_va = ioremap_nocache(addr, size);
	if (!dram_va) {
		dev_err(dev, "remap failed for addr 0x%llx\n", addr);
		ret = -ENOMEM;
		goto end;
	}
#endif
	adsp_add_load_mappings(addr, dram_va, size);
end:
	return ret;
}

static void deallocate_memory_for_adsp_os(struct device *dev)
{
#if defined(CONFIG_TEGRA_NVADSP_ON_SMMU)
	void *va = nvadsp_da_to_va_mappings(priv.adsp_os_addr,
			priv.adsp_os_size);
	dma_free_coherent(dev, priv.adsp_os_addr, va, priv.adsp_os_size);
#endif
}

static int __nvadsp_os_secload(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	dma_addr_t addr = drv_data->adsp_mem[ACSR_ADDR];
	size_t size = drv_data->adsp_mem[ACSR_SIZE];
	struct nvadsp_shared_mem *shared_mem;
	struct device *dev = &pdev->dev;
	void *dram_va;

	dram_va = dma_alloc_at_coherent(dev, size, &addr, GFP_KERNEL);
	if (!dram_va) {
		dev_err(dev, "unable to allocate shared region\n");
		return -ENOMEM;
	}

	shared_mem = dram_va;
	drv_data->shared_adsp_os_data = shared_mem;
	/* set logger strcuture with required properties */
	priv.logger.debug_ram_rdr = shared_mem->os_args.logger;
	priv.logger.debug_ram_sz = sizeof(shared_mem->os_args.logger);
	priv.logger.dev = dev;

	priv.adsp_os_fw_loaded = true;
	wake_up(&priv.logger.wait_queue);

	return 0;
}

int nvadsp_os_load(void)
{
	struct nvadsp_shared_mem *shared_mem;
	struct nvadsp_drv_data *drv_data;
	const struct firmware *fw;
	struct device *dev;
	int ret = 0;

	if (!priv.pdev) {
		pr_err("ADSP Driver is not initialized\n");
		ret = -EINVAL;
		goto end;
	}

	mutex_lock(&priv.fw_load_lock);
	if (priv.adsp_os_fw_loaded)
		goto end;

	dev = &priv.pdev->dev;

	drv_data = platform_get_drvdata(priv.pdev);

	if (drv_data->adsp_os_secload) {
		dev_info(dev, "ADSP OS firmware already loaded\n");
		ret = __nvadsp_os_secload(priv.pdev);
		goto end;
	}

	ret = request_firmware(&fw, NVADSP_FIRMWARE, dev);
	if (ret < 0) {
		dev_err(dev, "reqest firmware for %s failed with %d\n",
				NVADSP_FIRMWARE, ret);
		goto end;
	}

	ret = create_global_symbol_table(fw);
	if (ret) {
		dev_err(dev, "unable to create global symbol table\n");
		goto release_firmware;
	}

	ret = allocate_memory_for_adsp_os();
	if (ret) {
		dev_err(dev, "unable to allocate memory for adsp os\n");
		goto release_firmware;
	}

	shared_mem = get_mailbox_shared_region(fw);
	drv_data->shared_adsp_os_data = shared_mem;
	/* set logger strcuture with required properties */
	priv.logger.debug_ram_rdr = shared_mem->os_args.logger;
	priv.logger.debug_ram_sz = sizeof(shared_mem->os_args.logger);
	priv.logger.dev = dev;

	dev_info(dev, "Loading ADSP OS firmware %s\n", NVADSP_FIRMWARE);

	ret = nvadsp_os_elf_load(fw);
	if (ret) {
		dev_err(dev, "failed to load %s\n", NVADSP_FIRMWARE);
		goto deallocate_os_memory;
	}

	ret = dram_app_mem_init(priv.app_alloc_addr, priv.app_size);
	if (ret) {
		dev_err(dev, "Memory allocation dynamic apps failed\n");
		goto deallocate_os_memory;
	}
	priv.os_firmware = fw;
	priv.adsp_os_fw_loaded = true;
	wake_up(&priv.logger.wait_queue);

	mutex_unlock(&priv.fw_load_lock);
	return 0;

deallocate_os_memory:
	deallocate_memory_for_adsp_os(dev);
release_firmware:
	release_firmware(fw);
end:
	mutex_unlock(&priv.fw_load_lock);
	return ret;
}
EXPORT_SYMBOL(nvadsp_os_load);

/*
 * Static adsp freq to emc freq lookup table
 *
 * arg:
 *	adspfreq - adsp freq in KHz
 * return:
 *	0 - min emc freq
 *	> 0 - expected emc freq at this adsp freq
 */
u32 adsp_to_emc_freq(u32 adspfreq)
{
	/*
	 * Vote on memory bus frequency based on adsp frequency
	 * cpu rate is in kHz, emc rate is in Hz
	 */
	if (adspfreq >= 204800)
		return 102000;	/* adsp >= 204.8 MHz, emc 102 MHz */
	else
		return 0;		/* emc min */
}

static int nvadsp_set_ape_emc_freq(struct nvadsp_drv_data *drv_data)
{
	unsigned long ape_emc_freq = drv_data->ape_emc_freq * 1000; /* in Hz */
	struct device *dev = &priv.pdev->dev;
	int ret;

#ifdef CONFIG_TEGRA_ADSP_DFS
	 /* pass adsp freq in KHz. adsp_emc_freq in Hz */
	ape_emc_freq = adsp_to_emc_freq(drv_data->adsp_freq / 1000) * 1000;
#endif
	dev_dbg(dev, "requested adsp cpu freq %luKHz",
		drv_data->adsp_freq / 1000);
	dev_dbg(dev, "ape.emc freq %luHz\n", ape_emc_freq / 1000);


	if (!ape_emc_freq)
		return 0;

	ret = clk_set_rate(drv_data->ape_emc_clk, ape_emc_freq);

	dev_dbg(dev, "ape.emc freq %luKHz\n",
		clk_get_rate(drv_data->ape_emc_clk) / 1000);
	return ret;
}

static int nvadsp_set_ape_freq(struct nvadsp_drv_data *drv_data)
{
	unsigned long ape_freq = drv_data->ape_freq * 1000; /* in Hz*/
	struct device *dev = &priv.pdev->dev;
	int ret;

#ifdef CONFIG_TEGRA_ADSP_ACTMON
	ape_freq = drv_data->adsp_freq / ADSP_TO_APE_CLK_RATIO;
#endif
	dev_dbg(dev, "ape freq %luKHz", ape_freq / 1000);

	if (!ape_freq)
		return 0;

	ret = clk_set_rate(drv_data->ape_clk, ape_freq);

	dev_dbg(dev, "ape freq %luKHz\n",
		clk_get_rate(drv_data->ape_clk) / 1000);
	return ret;
}

static int set_adsp_clks_and_timer_prescalar(struct nvadsp_drv_data *drv_data)
{
	struct nvadsp_shared_mem *shared_mem = drv_data->shared_adsp_os_data;
	struct nvadsp_os_args *os_args = &shared_mem->os_args;
	struct device *dev = &priv.pdev->dev;
	unsigned long max_adsp_freq;
	unsigned long adsp_freq;
	u32 max_index;
	u32 cur_index;
	int ret = 0;

	adsp_freq = drv_data->adsp_freq * 1000; /* in Hz*/

	max_adsp_freq = clk_round_rate(drv_data->adsp_cpu_clk,
				ULONG_MAX);
	max_index = max_adsp_freq / MIN_ADSP_FREQ;
	cur_index = adsp_freq / MIN_ADSP_FREQ;


	if (!adsp_freq)
		/* Set max adsp boot freq */
		cur_index = max_index;

	if (adsp_freq % MIN_ADSP_FREQ) {
		if (cur_index >= max_index)
			cur_index = max_index;
		else
			cur_index++;
	} else if (cur_index >= max_index)
		cur_index = max_index;

	/*
	 * timer interval = (prescalar + 1) * (count + 1) / periph_freq
	 * therefore for 0 count,
	 * 1 / TIMER_CLK_HZ =  (prescalar + 1) / periph_freq
	 * Hence, prescalar = periph_freq / TIMER_CLK_HZ - 1
	 */
	os_args->timer_prescalar = cur_index - 1;

	adsp_freq = cur_index * MIN_ADSP_FREQ;

	ret = clk_set_rate(drv_data->adsp_cpu_clk, adsp_freq);
	if (ret)
		goto end;

	drv_data->adsp_freq = adsp_freq / 1000; /* adsp_freq in KHz*/

end:
	dev_dbg(dev, "adsp cpu freq %luKHz\n",
		clk_get_rate(drv_data->adsp_cpu_clk) / 1000);
	dev_dbg(dev, "timer prescalar %x\n", os_args->timer_prescalar);

	return ret;
}

static int set_adsp_clks(struct nvadsp_drv_data *drv_data)
{
	struct nvadsp_shared_mem *shared_mem = drv_data->shared_adsp_os_data;
	struct nvadsp_os_args *os_args = &shared_mem->os_args;
	struct platform_device *pdev = drv_data->pdev;
	struct device *dev = &pdev->dev;
	unsigned long max_adsp_freq;
	unsigned long adsp_freq;
	int ret = 0;

	adsp_freq = drv_data->adsp_freq_hz; /* in Hz*/
	max_adsp_freq = clk_round_rate(drv_data->adsp_clk, ULONG_MAX);

	/* Set max adsp boot freq */
	if (!adsp_freq)
		adsp_freq = max_adsp_freq;

	ret = clk_set_rate(drv_data->adsp_clk, adsp_freq);
	if (ret) {
		dev_err(dev, "setting adsp_freq:%luHz failed.\n", adsp_freq);
		dev_err(dev, "max_adsp_freq:%luHz\n", max_adsp_freq);
		goto end;
	}

	drv_data->adsp_freq = adsp_freq / 1000; /* adsp_freq in KHz*/
	drv_data->adsp_freq_hz = adsp_freq;
	os_args->adsp_freq_hz = adsp_freq;
end:
	dev_dbg(dev, "adsp cpu freq %luKHz\n",
		clk_get_rate(drv_data->adsp_clk) / 1000);
	return ret;
}

static int __deassert_adsp(struct nvadsp_drv_data *drv_data)
{
	struct device *dev = &priv.pdev->dev;

	if (drv_data->adsp_unit_fpga) {
		dev_info(dev, "De-asserting ADSP UNIT-FPGA\n");
		writel(drv_data->unit_fpga_reset[ADSP_DEASSERT],
				priv.unit_fpga_reset_reg);
		return 0;
	}

	if (drv_data->adsp_clk) {
		dev_dbg(dev, "deasserting adsp...\n");
		tegra_periph_reset_deassert(drv_data->adsp_clk);
		udelay(200);
		return 0;
	}

	return -EINVAL;
}

static int nvadsp_deassert_adsp(struct nvadsp_drv_data *drv_data)
{
	int ret = -EINVAL;

	if (drv_data->deassert_adsp)
		ret = drv_data->deassert_adsp(drv_data);

	return ret;
}

static int __assert_adsp(struct nvadsp_drv_data *drv_data)
{
	struct device *dev = &priv.pdev->dev;

	if (drv_data->adsp_unit_fpga) {
		if (drv_data->unit_fpga_reset[ADSP_ASSERT]) {
			dev_info(dev, "Asserting ADSP UNIT-FPGA\n");
			writel(drv_data->unit_fpga_reset[ADSP_ASSERT],
				priv.unit_fpga_reset_reg);
		}
		return 0;
	}

	if (drv_data->adsp_clk) {
		tegra_periph_reset_assert(drv_data->adsp_clk);
		udelay(200);
		return 0;
	}

	return -EINVAL;
}

static int nvadsp_assert_adsp(struct nvadsp_drv_data *drv_data)
{
	int ret = -EINVAL;

	if (drv_data->assert_adsp)
		ret = drv_data->assert_adsp(drv_data);

	return ret;
}

static int nvadsp_set_boot_freqs(struct nvadsp_drv_data *drv_data)
{
	struct device *dev = &priv.pdev->dev;
	struct device_node *node = dev->of_node;
	int ret = 0;

	/* on Unit-FPGA do not set clocks, return success */
	if (drv_data->adsp_unit_fpga)
		return 0;

	if (of_device_is_compatible(node, "nvidia,tegra210-adsp")) {
		if (drv_data->adsp_cpu_clk) {
			ret = set_adsp_clks_and_timer_prescalar(drv_data);
			if (ret)
				goto end;
		} else {
			ret = -EINVAL;
			goto end;
		}
	} else {
		if (drv_data->adsp_clk) {
			ret = set_adsp_clks(drv_data);
			if (ret)
				goto end;
		} else {
			ret = -EINVAL;
			goto end;
		}
	}

	if (drv_data->ape_clk) {
		ret = nvadsp_set_ape_freq(drv_data);
		if (ret)
			goto end;
	}

	if (drv_data->ape_emc_clk) {
		ret = nvadsp_set_ape_emc_freq(drv_data);
		if (ret)
			goto end;
	}

end:
	return ret;
}

static int wait_for_adsp_os_load_complete(void)
{
	struct device *dev = &priv.pdev->dev;
	uint32_t data;
	status_t ret;

	ret = nvadsp_mbox_recv(&adsp_com_mbox, &data,
			true, ADSP_OS_LOAD_TIMEOUT);
	if (ret) {
		dev_err(dev, "ADSP OS loading timed out\n");
		goto end;
	}
	dev_dbg(dev, "ADSP has been %s\n",
		data == ADSP_OS_BOOT_COMPLETE ? "BOOTED" : "RESUMED");

	switch (data) {
	case ADSP_OS_BOOT_COMPLETE:
		ret = load_adsp_static_apps();
		break;
	case ADSP_OS_RESUME:
	default:
		break;
	}
end:
	return ret;
}

static int __nvadsp_os_start(void)
{
	struct nvadsp_drv_data *drv_data;
	struct device *dev;
	int ret = 0;

	dev = &priv.pdev->dev;
	drv_data = platform_get_drvdata(priv.pdev);


	dev_dbg(dev, "ADSP is booting on %s\n",
		drv_data->adsp_unit_fpga ? "UNIT-FPGA" : "SILICON");

	nvadsp_assert_adsp(drv_data);

	if (!drv_data->adsp_os_secload) {
		dev_dbg(dev, "Copying EVP...\n");
		copy_io_in_l(drv_data->state.evp_ptr,
			     drv_data->state.evp,
			     AMC_EVP_SIZE);
	}

	dev_dbg(dev, "Setting freqs\n");
	ret = nvadsp_set_boot_freqs(drv_data);
	if (ret) {
		dev_err(dev, "failed to set boot freqs\n");
		goto end;
	}

	dev_dbg(dev, "De-asserting adsp\n");
	ret = nvadsp_deassert_adsp(drv_data);
	if (ret) {
		dev_err(dev, "failed to deassert ADSP\n");
		goto end;
	}

	dev_dbg(dev, "Waiting for ADSP OS to boot up...\n");

	ret = wait_for_adsp_os_load_complete();
	if (ret) {
		dev_err(dev, "Unable to start ADSP OS\n");
		goto end;
	}
	dev_dbg(dev, "ADSP OS boot up... Done!\n");

#ifdef CONFIG_TEGRA_ADSP_DFS
	ret = adsp_dfs_core_init(priv.pdev);
	if (ret) {
		dev_err(dev, "adsp dfs initialization failed\n");
		goto err;
	}
#endif

#ifdef CONFIG_TEGRA_ADSP_ACTMON
	ret = ape_actmon_init(priv.pdev);
	if (ret) {
		dev_err(dev, "ape actmon initialization failed\n");
		goto err;
	}
#endif

#ifdef CONFIG_TEGRA_ADSP_CPUSTAT
	ret = adsp_cpustat_init(priv.pdev);
	if (ret) {
		dev_err(dev, "adsp cpustat initialisation failed\n");
		goto err;
	}
#endif
end:
	return ret;

#if defined(CONFIG_TEGRA_ADSP_DFS) || defined(CONFIG_TEGRA_ADSP_CPUSTAT)
err:
	__nvadsp_os_stop(true);
	return ret;
#endif
}

static void dump_adsp_logs(void)
{
	int i = 0;
	char buff[DUMP_BUFF] = { };
	int buff_iter = 0;
	char last_char;
	struct nvadsp_debug_log *logger = &priv.logger;
	struct device *dev = &priv.pdev->dev;
	char *ptr = logger->debug_ram_rdr;

	dev_err(dev, "Dumping ADSP logs ........\n");

	for (i = 0; i < logger->debug_ram_sz; i++) {
		last_char = *(ptr + i);
		if ((last_char != EOT) && (last_char != 0)) {
			if ((last_char == '\n') || (last_char == '\r') ||
					(buff_iter == DUMP_BUFF)) {
				dev_err(dev, "[ADSP OS] %s\n", buff);
				memset(buff, 0, sizeof(buff));
				buff_iter = 0;
			} else {
				buff[buff_iter++] = last_char;
			}
		}
	}
	dev_err(dev, "End of ADSP log dump  .....\n");
}

static void print_agic_irq_states(void)
{
	struct device *dev = &priv.pdev->dev;
	int i;

	for (i = INT_AGIC_START; i < INT_AGIC_END; i++) {
		dev_info(dev, "irq %d is %s and %s\n", i,
		tegra_agic_irq_is_pending(i) ?
			"pending" : "not pending",
		tegra_agic_irq_is_active(i) ?
			"active" : "not active");
	}
}

void dump_adsp_sys(void)
{
	if (!priv.pdev) {
		pr_err("ADSP Driver is not initialized\n");
		return;
	}

	dump_adsp_logs();
	dump_mailbox_regs();
	print_agic_irq_states();
}
EXPORT_SYMBOL(dump_adsp_sys);

int nvadsp_os_start(void)
{
	struct nvadsp_drv_data *drv_data;
	struct device *dev;
	int ret = 0;

	if (!priv.pdev) {
		pr_err("ADSP Driver is not initialized\n");
		ret = -EINVAL;
		goto end;
	}

	drv_data = platform_get_drvdata(priv.pdev);
	dev = &priv.pdev->dev;

	/* check if fw is loaded then start the adsp os */
	if (!priv.adsp_os_fw_loaded) {
		dev_err(dev, "Call to nvadsp_os_load not made\n");
		ret = -EINVAL;
		goto end;
	}

	mutex_lock(&priv.os_run_lock);
	/* if adsp is started/running exit gracefully */
	if (priv.os_running)
		goto unlock;

#ifdef CONFIG_PM
	ret = pm_runtime_get_sync(&priv.pdev->dev);
	if (ret < 0)
		goto unlock;
#endif
	ret = __nvadsp_os_start();
	if (ret) {
		priv.os_running = drv_data->adsp_os_running = false;
		/* if start fails call pm suspend of adsp driver */
		dev_err(dev, "adsp failed to boot with ret = %d\n", ret);
		dump_adsp_sys();
#ifdef CONFIG_PM
		pm_runtime_put_sync(&priv.pdev->dev);
#endif
		goto unlock;

	}
	priv.os_running = drv_data->adsp_os_running = true;
#if defined(CONFIG_TEGRA_ADSP_FILEIO)
	if (!drv_data->adspff_init) {
		ret = adspff_init();
		if (!ret)
			drv_data->adspff_init = true;
	}
#endif
	drv_data->adsp_os_suspended = false;
	wake_up(&priv.logger.wait_queue);
unlock:
	mutex_unlock(&priv.os_run_lock);
end:
	return ret;
}
EXPORT_SYMBOL(nvadsp_os_start);

static int __nvadsp_os_suspend(void)
{
	struct device *dev = &priv.pdev->dev;
	struct nvadsp_drv_data *drv_data;
	int ret;

	drv_data = platform_get_drvdata(priv.pdev);

#ifdef CONFIG_TEGRA_ADSP_ACTMON
	ape_actmon_exit(priv.pdev);
#endif

#ifdef CONFIG_TEGRA_ADSP_DFS
	adsp_dfs_core_exit(priv.pdev);
#endif

#ifdef CONFIG_TEGRA_ADSP_CPUSTAT
	adsp_cpustat_exit(priv.pdev);
#endif

	ret = nvadsp_mbox_send(&adsp_com_mbox, ADSP_OS_SUSPEND,
			       NVADSP_MBOX_SMSG, true, UINT_MAX);
	if (ret) {
		dev_err(dev, "failed to send with adsp com mbox\n");
		goto out;
	}

	dev_dbg(dev, "Waiting for ADSP OS suspend...\n");
	ret = wait_for_completion_interruptible_timeout(&entered_wfi,
		msecs_to_jiffies(ADSP_WFE_TIMEOUT));
	if (WARN_ON(ret <= 0)) {
		dev_err(dev, "Unable to suspend ADSP OS\n");
		ret = -EINVAL;
		goto out;
	}
	ret = 0;
	dev_dbg(dev, "ADSP OS suspended!\n");

	drv_data->adsp_os_suspended = true;

	nvadsp_assert_adsp(drv_data);

 out:
	return ret;
}

static void __nvadsp_os_stop(bool reload)
{
	const struct firmware *fw = priv.os_firmware;
	struct nvadsp_drv_data *drv_data;
	struct device *dev;
	int err = 0;

	dev = &priv.pdev->dev;
	drv_data = platform_get_drvdata(priv.pdev);

#ifdef CONFIG_TEGRA_ADSP_ACTMON
	ape_actmon_exit(priv.pdev);
#endif

#ifdef CONFIG_TEGRA_ADSP_DFS
	adsp_dfs_core_exit(priv.pdev);
#endif

#ifdef CONFIG_TEGRA_ADSP_CPUSTAT
	adsp_cpustat_exit(priv.pdev);
#endif
#if defined(CONFIG_TEGRA_ADSP_FILEIO)
	if (drv_data->adspff_init) {
		adspff_exit();
		drv_data->adspff_init = false;
	}
#endif

	writel(ENABLE_MBOX2_FULL_INT, priv.hwmailbox_base + HWMBOX2_REG);
	err = wait_for_completion_interruptible_timeout(&entered_wfi,
		msecs_to_jiffies(ADSP_WFE_TIMEOUT));
	writel(DISABLE_MBOX2_FULL_INT, priv.hwmailbox_base + HWMBOX2_REG);

	/*
	 * ADSP needs to be in WFI/WFE state to properly reset it.
	 * However, when ADSPOS is getting stopped on error path,
	 * it cannot gaurantee that ADSP is in WFI/WFE state.
	 * Reset it in either case. On failure, whole APE reset is
	 * required (happens on next APE power domain cycle).
	 */
	nvadsp_assert_adsp(drv_data);

	/* Don't reload ADSPOS if ADSP state is not WFI/WFE */
	if (WARN_ON(err <= 0)) {
		dev_err(dev, "ADSP is unable to enter wfi state\n");
		goto end;
	}

	if (reload && !drv_data->adsp_os_secload) {
		struct nvadsp_debug_log *logger = &priv.logger;

		wake_up(&logger->wait_queue);
		/* wait for LOGGER_TIMEOUT to complete filling the buffer */
		wait_for_completion_interruptible_timeout(&logger->complete,
			msecs_to_jiffies(LOGGER_COMPLETE_TIMEOUT));
		/*
		 * move ram iterator to 0, since after restart the iterator
		 * will be pointing to initial position of start.
		 */
		logger->debug_ram_rdr[0] = EOT;
		logger->ram_iter = 0;

		/* load a fresh copy of adsp.elf */
		if (nvadsp_os_elf_load(fw))
			dev_err(dev, "failed to reload %s\n", NVADSP_FIRMWARE);
	}

 end:
	return;
}


void nvadsp_os_stop(void)
{
	struct nvadsp_drv_data *drv_data;
	struct device *dev;
	int err;

	if (!priv.pdev) {
		pr_err("ADSP Driver is not initialized\n");
		return;
	}

	dev = &priv.pdev->dev;
	drv_data = platform_get_drvdata(priv.pdev);

	mutex_lock(&priv.os_run_lock);
	/* check if os is running else exit */
	if (!priv.os_running)
		goto end;

	__nvadsp_os_stop(true);

	priv.os_running = drv_data->adsp_os_running = false;

#ifdef CONFIG_PM
	err = pm_runtime_put_sync(dev);
	if (err)
		dev_err(dev, "failed in pm_runtime_put_sync\n");
#endif
end:
	mutex_unlock(&priv.os_run_lock);
}
EXPORT_SYMBOL(nvadsp_os_stop);

int nvadsp_os_suspend(void)
{
	struct device *dev = &priv.pdev->dev;
	struct nvadsp_drv_data *drv_data;
	int ret = -EINVAL;

	if (!priv.pdev) {
		pr_err("ADSP Driver is not initialized\n");
		goto end;
	}

	/*
	 * No os suspend/stop on linsim as
	 * APE can be reset only once.
	 */
	if (tegra_platform_is_linsim())
		goto end;

	drv_data = platform_get_drvdata(priv.pdev);

	mutex_lock(&priv.os_run_lock);
	/* check if os is running else exit */
	if (!priv.os_running) {
		ret = 0;
		goto unlock;
	}
	ret = __nvadsp_os_suspend();
	if (!ret) {
#ifdef CONFIG_PM
		ret = pm_runtime_put_sync(&priv.pdev->dev);
		if (ret)
			dev_err(dev, "failed in pm_runtime_put_sync\n");
#endif
		priv.os_running = drv_data->adsp_os_running = false;
	} else {
		dev_err(&priv.pdev->dev, "suspend failed with %d\n", ret);
		dump_adsp_sys();
	}
unlock:
	mutex_unlock(&priv.os_run_lock);
end:
	return ret;
}
EXPORT_SYMBOL(nvadsp_os_suspend);

static void nvadsp_os_restart(struct work_struct *work)
{
	struct nvadsp_os_data *data =
		container_of(work, struct nvadsp_os_data, restart_os_work);
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(data->pdev);
	int wdt_virq = drv_data->agic_irqs[WDT_VIRQ];
	struct device *dev = &data->pdev->dev;

	disable_irq(wdt_virq);
	dump_adsp_sys();
	nvadsp_os_stop();

	if (tegra_agic_irq_is_active(ADSP_WDT_INT)) {
		dev_info(dev, "wdt interrupt is active hence clearing\n");
		tegra_agic_clear_active(ADSP_WDT_INT);
	}

	if (tegra_agic_irq_is_pending(ADSP_WDT_INT)) {
		dev_info(dev, "wdt interrupt is pending hence clearing\n");
		tegra_agic_clear_pending(ADSP_WDT_INT);
	}

	dev_info(dev, "wdt interrupt is not pending or active...enabling\n");
	enable_irq(wdt_virq);

	data->adsp_num_crashes++;
	if (data->adsp_num_crashes >= ALLOWED_CRASHES) {
		/* making pdev NULL so that externally start is not called */
		priv.pdev = NULL;
		dev_crit(dev, "ADSP has crashed too many times(%d)\n",
			 data->adsp_num_crashes);
		return;
	}

	if (nvadsp_os_start())
		dev_crit(dev, "Unable to restart ADSP OS\n");
}

static  irqreturn_t adsp_wfi_handler(int irq, void *arg)
{
	struct nvadsp_os_data *data = arg;
	struct device *dev = &data->pdev->dev;

	dev_dbg(dev, "%s\n", __func__);
	complete(&entered_wfi);

	return IRQ_HANDLED;
}

static irqreturn_t adsp_wdt_handler(int irq, void *arg)
{
	struct nvadsp_os_data *data = arg;
	struct nvadsp_drv_data *drv_data;
	struct device *dev = &data->pdev->dev;

	drv_data = platform_get_drvdata(data->pdev);
	if (!drv_data->adsp_unit_fpga) {
		dev_crit(dev, "ADSP OS Hanged or Crashed! Restarting...\n");
		schedule_work(&data->restart_os_work);
	} else {
		dev_crit(dev, "ADSP OS Hanged or Crashed!\n");
	}
	return IRQ_HANDLED;
}

void nvadsp_get_os_version(char *buf, int buf_size)
{
	struct nvadsp_drv_data *drv_data;
	struct nvadsp_shared_mem *shared_mem;
	struct nvadsp_os_info *os_info;

	memset(buf, 0, buf_size);

	if (!priv.pdev)
		return;

	drv_data = platform_get_drvdata(priv.pdev);
	shared_mem = drv_data->shared_adsp_os_data;
	if (shared_mem) {
		os_info = &shared_mem->os_info;
		strlcpy(buf, os_info->version, buf_size);
	} else {
		strlcpy(buf, "unavailable", buf_size);
	}
}
EXPORT_SYMBOL(nvadsp_get_os_version);

#ifdef CONFIG_DEBUG_FS
static int show_os_version(struct seq_file *s, void *data)
{
	char ver_buf[MAX_OS_VERSION_BUF] = "";

	nvadsp_get_os_version(ver_buf, MAX_OS_VERSION_BUF);
	seq_printf(s, "version=\"%s\"\n", ver_buf);

	return 0;
}

static int os_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_os_version, inode->i_private);
}

static const struct file_operations version_fops = {
	.open = os_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#define RO_MODE S_IRUSR

int adsp_create_os_version(struct dentry *adsp_debugfs_root)
{
	struct device *dev = &priv.pdev->dev;
	struct dentry *d;

	d = debugfs_create_file("adspos_version", RO_MODE, adsp_debugfs_root,
				NULL, &version_fops);
	if (!d) {
		dev_err(dev, "failed to create adsp_version\n");
		return -EINVAL;
	}
	return 0;
}
#endif

static ssize_t tegrafw_read_adsp(struct device *dev,
				char *data, size_t size)
{
	nvadsp_get_os_version(data, size);
	return strlen(data);
}

int __init nvadsp_os_probe(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	int wdt_virq = drv_data->agic_irqs[WDT_VIRQ];
	int wfi_virq = drv_data->agic_irqs[WFI_VIRQ];
	uint16_t com_mid = ADSP_COM_MBOX_ID;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int ret = 0;

	priv.unit_fpga_reset_reg = drv_data->base_regs[UNIT_FPGA_RST];
	priv.hwmailbox_base = drv_data->base_regs[HWMB_REG_IDX];
	priv.dram_region = drv_data->dram_region;

	priv.adsp_os_addr = drv_data->adsp_mem[ADSP_OS_ADDR];
	priv.adsp_os_size = drv_data->adsp_mem[ADSP_OS_SIZE];
	priv.app_alloc_addr = drv_data->adsp_mem[ADSP_APP_ADDR];
	priv.app_size = drv_data->adsp_mem[ADSP_APP_SIZE];

	ret = devm_request_irq(dev, wdt_virq, adsp_wdt_handler,
			IRQF_TRIGGER_RISING, "adsp watchdog", &priv);
	if (ret) {
		dev_err(dev, "failed to get adsp watchdog interrupt\n");
		goto end;
	}

	ret = devm_request_irq(dev, wfi_virq, adsp_wfi_handler,
			IRQF_TRIGGER_RISING, "adsp wfi", &priv);
	if (ret) {
		dev_err(dev, "cannot request for wfi interrupt\n");
		goto end;
	}

	writel(DISABLE_MBOX2_FULL_INT, priv.hwmailbox_base + HWMBOX2_REG);

	if (of_device_is_compatible(dev->of_node, "nvidia,tegra210-adsp")) {
		drv_data->assert_adsp = __assert_adsp;
		drv_data->deassert_adsp = __deassert_adsp;
	}

	if (!of_device_is_compatible(node, "nvidia,tegra18x-adsp-hv")) {
		ret = nvadsp_os_init(pdev);
		if (ret) {
			dev_err(dev, "failed to init os\n");
			goto end;
		}
	}

	ret = nvadsp_mbox_open(&adsp_com_mbox, &com_mid, "adsp_com_mbox",
			       NULL, NULL);
	if (ret) {
		dev_err(dev, "failed to open adsp com mbox\n");
		goto end;
	}

	INIT_WORK(&priv.restart_os_work, nvadsp_os_restart);
	mutex_init(&priv.fw_load_lock);
	mutex_init(&priv.os_run_lock);

	priv.pdev = pdev;
#ifdef CONFIG_DEBUG_FS
	priv.logger.dev = &pdev->dev;
	if (adsp_create_debug_logger(drv_data->adsp_debugfs_root))
		dev_err(dev, "unable to create adsp debug logger file\n");

#ifdef CONFIG_TEGRA_ADSP_CONSOLE
	priv.console.dev = &pdev->dev;
	if (adsp_create_cnsl(drv_data->adsp_debugfs_root, &priv.console))
		dev_err(dev, "unable to create adsp console file\n");
#endif /* CONFIG_TEGRA_ADSP_CONSOLE */

	if (adsp_create_os_version(drv_data->adsp_debugfs_root))
		dev_err(dev, "unable to create adsp_version file\n");

#endif /* CONFIG_DEBUG_FS */

	devm_tegrafw_register(dev, "APE", TFW_DONT_CACHE,
			tegrafw_read_adsp, NULL);
end:
	return ret;
}
