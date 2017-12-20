/*
 * drivers/media/video/tegra/nvavp/nvavp_dev.c
 *
 * Copyright (C) 2011 NVIDIA Corp.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/irq.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/nvhost.h>
#include <linux/platform_device.h>
#include <linux/rbtree.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tegra_nvavp.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <mach/clk.h>
#include <mach/hardware.h>
#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/legacy_irq.h>
#include <mach/nvmap.h>

#include "../../../video/tegra/nvmap/nvmap.h"
#include "../../../video/tegra/host/t20/syncpt_t20.h"
#include "../../../video/tegra/host/dev.h"
#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU)
#include "../avp/headavp.h"
#endif
#include "nvavp_os.h"

/* Super hacky: tesla user is always uid 1111 */
#define GLOBAL_TESLA_UID KUIDT_INIT(1111)

#define TEGRA_NVAVP_NAME			"tegra-avp"

#define NVAVP_PUSHBUFFER_SIZE			4096

#define NVAVP_PUSHBUFFER_MIN_UPDATE_SPACE	(sizeof(u32) * 3)

#define TEGRA_NVAVP_RESET_VECTOR_ADDR	\
		(IO_ADDRESS(TEGRA_EXCEPTION_VECTORS_BASE) + 0x200)

#define FLOW_CTRL_HALT_COP_EVENTS	IO_ADDRESS(TEGRA_FLOW_CTRL_BASE + 0x4)
#define FLOW_MODE_STOP			(0x2 << 29)
#define FLOW_MODE_NONE			0x0

#define NVAVP_OS_INBOX			IO_ADDRESS(TEGRA_RES_SEMA_BASE + 0x10)
#define NVAVP_OS_OUTBOX			IO_ADDRESS(TEGRA_RES_SEMA_BASE + 0x20)

#define NVAVP_INBOX_VALID		(1 << 29)

struct nvavp_info {
	struct clk			*bsev_clk;
	struct clk			*vde_clk;
	struct clk			*cop_clk;

	struct clk			*sclk;
	struct clk			*emc_clk;

	int				mbox_from_avp_pend_irq;

	struct mutex			open_lock;
	int				refcount;

	/* os information */
	struct nvavp_os_info		os_info;

	/* ucode information */
	struct nvavp_ucode_info		ucode_info;

	/* client for driver allocations, persistent */
	struct nvmap_client		*nvmap;

	struct mutex			pushbuffer_lock;
	struct nvmap_handle_ref		*pushbuf_handle;
	unsigned long			pushbuf_phys;
	u8				*pushbuf_data;
	u32				pushbuf_index;
	u32				pushbuf_fence;

	struct nv_e276_control		*os_control;

	struct nvhost_syncpt		*nvhost_syncpt;
	u32				syncpt_id;
	u32				syncpt_value;

	struct nvhost_device		*nvhost_dev;
	struct miscdevice		misc_dev;
};

struct nvavp_clientctx {
	struct nvmap_client *nvmap;
	struct nvavp_pushbuffer_submit_hdr submit_hdr;
	struct nvavp_reloc relocs[NVAVP_MAX_RELOCATION_COUNT];
	struct nvmap_handle_ref *gather_mem;
	int num_relocs;
	struct nvavp_info *nvavp;
};

static int nvavp_service(struct nvavp_info *nvavp)
{
	struct nvavp_os_info *os = &nvavp->os_info;
	u8 *debug_print;
	u32 inbox;

	inbox = readl(NVAVP_OS_INBOX);
	if (!(inbox & NVAVP_INBOX_VALID))
		inbox = 0x00000000;

	writel(0x00000000, NVAVP_OS_INBOX);

	if (inbox & NVE276_OS_INTERRUPT_DEBUG_STRING) {
		/* Should only occur with debug AVP OS builds */
		debug_print = os->data;
		debug_print += os->debug_offset;
		dev_info(&nvavp->nvhost_dev->dev, "%s\n", debug_print);
	}
	if (inbox & NVE276_OS_INTERRUPT_VDE_SHUTDOWN) {
		/* TODO: We may want to put some sort of
		 * synchronization in place (resource semaphores ?)
		 * so that AVP can set VDE clocks without
		 * involving RM (as this is likely to occur often)
		 */
		dev_info(&nvavp->nvhost_dev->dev, "shutting down VDE...\n");
	}
	if (inbox & (NVE276_OS_INTERRUPT_SEMAPHORE_AWAKEN |
		     NVE276_OS_INTERRUPT_EXECUTE_AWAKEN)) {
		dev_info(&nvavp->nvhost_dev->dev,
			"AVP awaken event (0x%x)\n", inbox);
	}
	if (inbox & NVE276_OS_INTERRUPT_AVP_FATAL_ERROR) {
		dev_err(&nvavp->nvhost_dev->dev,
			"fatal AVP error (0x%08X)\n", inbox);
	}
	if (inbox & NVE276_OS_INTERRUPT_AVP_BREAKPOINT)
		dev_err(&nvavp->nvhost_dev->dev, "AVP breakpoint hit\n");
	if (inbox & NVE276_OS_INTERRUPT_TIMEOUT)
		dev_err(&nvavp->nvhost_dev->dev, "AVP timeout\n");

	return 0;
}

static irqreturn_t nvavp_mbox_pending_isr(int irq, void *data)
{
	struct nvavp_info *nvavp = data;

	nvavp_service(nvavp);

	return IRQ_HANDLED;
}

static void nvavp_halt_avp(struct nvavp_info *nvavp)
{
	/* ensure the AVP is halted */
	writel(FLOW_MODE_STOP, FLOW_CTRL_HALT_COP_EVENTS);
	tegra_periph_reset_assert(nvavp->cop_clk);

	writel(0, NVAVP_OS_OUTBOX);
	writel(0, NVAVP_OS_INBOX);
}

static int nvavp_reset_avp(struct nvavp_info *nvavp, unsigned long reset_addr)
{
#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU)
	unsigned long stub_code_phys = virt_to_phys(_tegra_avp_boot_stub);
	dma_addr_t stub_data_phys;

	_tegra_avp_boot_stub_data.map_phys_addr = avp->kernel_phys;
	_tegra_avp_boot_stub_data.jump_addr = reset_addr;
	wmb();
	stub_data_phys = dma_map_single(NULL, &_tegra_avp_boot_stub_data,
					sizeof(_tegra_avp_boot_stub_data),
					DMA_TO_DEVICE);
	rmb();
	reset_addr = (unsigned long)stub_data_phys;
#endif
	writel(FLOW_MODE_STOP, FLOW_CTRL_HALT_COP_EVENTS);

	writel(reset_addr, TEGRA_NVAVP_RESET_VECTOR_ADDR);

	tegra_periph_reset_assert(nvavp->cop_clk);
	udelay(2);
	tegra_periph_reset_deassert(nvavp->cop_clk);

	writel(FLOW_MODE_NONE, FLOW_CTRL_HALT_COP_EVENTS);

#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU)
	dma_unmap_single(NULL, stub_data_phys,
			 sizeof(_tegra_avp_boot_stub_data),
			 DMA_TO_DEVICE);
#endif
	return 0;
}

static void nvavp_halt_vde(struct nvavp_info *nvavp)
{
	tegra_periph_reset_assert(nvavp->bsev_clk);
	clk_disable(nvavp->bsev_clk);
	tegra_periph_reset_assert(nvavp->vde_clk);
	clk_disable(nvavp->vde_clk);
}

static int nvavp_reset_vde(struct nvavp_info *nvavp)
{
	clk_enable(nvavp->bsev_clk);
	tegra_periph_reset_assert(nvavp->bsev_clk);
	udelay(2);
	tegra_periph_reset_deassert(nvavp->bsev_clk);

	clk_enable(nvavp->vde_clk);
	tegra_periph_reset_assert(nvavp->vde_clk);
	udelay(2);
	tegra_periph_reset_deassert(nvavp->vde_clk);
	return 0;
}

static int nvavp_pushbuffer_alloc(struct nvavp_info *nvavp)
{
	int ret = 0;

	nvavp->pushbuf_handle = nvmap_alloc(nvavp->nvmap, NVAVP_PUSHBUFFER_SIZE,
				SZ_1M, NVMAP_HANDLE_UNCACHEABLE);
	if (IS_ERR(nvavp->pushbuf_handle)) {
		dev_err(&nvavp->nvhost_dev->dev,
			"cannot create pushbuffer handle\n");
		ret = PTR_ERR(nvavp->pushbuf_handle);
		goto err_pushbuf_alloc;
	}
	nvavp->pushbuf_data = (u8 *)nvmap_mmap(nvavp->pushbuf_handle);
	if (!nvavp->pushbuf_data) {
		dev_err(&nvavp->nvhost_dev->dev,
			"cannot map pushbuffer handle\n");
		ret = -ENOMEM;
		goto err_pushbuf_mmap;
	}
	nvavp->pushbuf_phys = nvmap_pin(nvavp->nvmap, nvavp->pushbuf_handle);
	if (IS_ERR((void *)nvavp->pushbuf_phys)) {
		dev_err(&nvavp->nvhost_dev->dev,
			"cannot pin pushbuffer handle\n");
		ret = PTR_ERR((void *)nvavp->pushbuf_phys);
		goto err_pushbuf_pin;
	}

	memset(nvavp->pushbuf_data, 0, NVAVP_PUSHBUFFER_SIZE);

	return 0;

err_pushbuf_pin:
	nvmap_munmap(nvavp->pushbuf_handle, nvavp->pushbuf_data);
err_pushbuf_mmap:
	nvmap_free(nvavp->nvmap, nvavp->pushbuf_handle);
err_pushbuf_alloc:
	return ret;
}

static void nvavp_pushbuffer_free(struct nvavp_info *nvavp)
{
	nvmap_unpin(nvavp->nvmap, nvavp->pushbuf_handle);
	nvmap_munmap(nvavp->pushbuf_handle, nvavp->pushbuf_data);
	nvmap_free(nvavp->nvmap, nvavp->pushbuf_handle);
}

static int nvavp_pushbuffer_init(struct nvavp_info *nvavp)
{
	void *ptr;
	struct nvavp_os_info *os = &nvavp->os_info;
	struct nv_e276_control *control;
	u32 temp;
	int ret;

	ret = nvavp_pushbuffer_alloc(nvavp);
	if (ret) {
		dev_err(&nvavp->nvhost_dev->dev,
			"unable to alloc pushbuffer\n");
		return ret;
	}

	ptr = os->data;
	ptr += os->control_offset;
	nvavp->os_control = (struct nv_e276_control *)ptr;

	control = nvavp->os_control;

	/* init get and put pointers */
	writel(0x0, &control->put);
	writel(0x0, &control->get);
	/* enable host clock control and disable iram clock gating */
	writel(0x1, &control->idle_clk_enable);
	writel(0x0, &control->iram_clk_gating);

	/* init dma start and end pointers */
	writel(nvavp->pushbuf_phys, &control->dma_start);
	writel((nvavp->pushbuf_phys + NVAVP_PUSHBUFFER_SIZE),
					&control->dma_end);

	writel(0x00, &nvavp->pushbuf_index);
	temp = NVAVP_PUSHBUFFER_SIZE - NVAVP_PUSHBUFFER_MIN_UPDATE_SPACE;
	writel(temp, &nvavp->pushbuf_fence);

	nvavp->syncpt_id = NVSYNCPT_AVP_0;
	nvavp->syncpt_value = nvhost_syncpt_read(nvavp->nvhost_syncpt,
						 nvavp->syncpt_id);

	return 0;
}

static void nvavp_pushbuffer_deinit(struct nvavp_info *nvavp)
{
	nvavp_pushbuffer_free(nvavp);
}

static int nvavp_pushbuffer_update(struct nvavp_info *nvavp, u32 phys_addr,
			u32 gather_count, struct nvavp_syncpt *syncpt,
			u32 ext_ucode_flag)
{
	struct nv_e276_control *control = nvavp->os_control;
	u32 gather_cmd, setucode_cmd, sync = 0;
	u32 wordcount = 0;
	u32 index, value = -1;

	mutex_lock(&nvavp->pushbuffer_lock);

	/* check for pushbuffer wrapping */
	if (nvavp->pushbuf_index >= nvavp->pushbuf_fence)
		nvavp->pushbuf_index = 0;

	if (!ext_ucode_flag) {
		/* additional wrap check for this case */
		if (nvavp->pushbuf_index + (4 * sizeof(u32)) >=
				nvavp->pushbuf_fence)
			nvavp->pushbuf_index = 0;

		setucode_cmd =
			NVE26E_CH_OPCODE_INCR(NVE276_SET_MICROCODE_A, 3);

		index = wordcount + nvavp->pushbuf_index;
		writel(setucode_cmd, (nvavp->pushbuf_data + index));
		wordcount += sizeof(u32);

		index = wordcount + nvavp->pushbuf_index;
		writel(0, (nvavp->pushbuf_data + index));
		wordcount += sizeof(u32);

		index = wordcount + nvavp->pushbuf_index;
		writel(nvavp->ucode_info.phys, (nvavp->pushbuf_data + index));
		wordcount += sizeof(u32);

		index = wordcount + nvavp->pushbuf_index;
		writel(nvavp->ucode_info.size, (nvavp->pushbuf_data + index));
		wordcount += sizeof(u32);
	}

	gather_cmd = NVE26E_CH_OPCODE_GATHER(0, 0, 0, gather_count);

	if (syncpt) {
		value = ++nvavp->syncpt_value;
		/* XXX: NvSchedValueWrappingComparison */
		sync = NVE26E_CH_OPCODE_IMM(NVE26E_HOST1X_INCR_SYNCPT,
			(NVE26E_HOST1X_INCR_SYNCPT_COND_OP_DONE << 8) |
			(nvavp->syncpt_id & 0xFF));
	}

	/* write commands out */
	index = wordcount + nvavp->pushbuf_index;
	writel(gather_cmd, (nvavp->pushbuf_data + index));
	wordcount += sizeof(u32);

	index = wordcount + nvavp->pushbuf_index;
	writel(phys_addr, (nvavp->pushbuf_data + index));
	wordcount += sizeof(u32);

	if (syncpt) {
		index = wordcount + nvavp->pushbuf_index;
		writel(sync, (nvavp->pushbuf_data + index));
		wordcount += sizeof(u32);
	}
	/* update put pointer */
	nvavp->pushbuf_index = (nvavp->pushbuf_index + wordcount) &
					(NVAVP_PUSHBUFFER_SIZE - 1);
	writel(nvavp->pushbuf_index, &control->put);
	wmb();

	/* wake up avp */
	writel(0xA0000001, NVAVP_OS_OUTBOX);

	/* Fill out fence struct */
	if (syncpt) {
		syncpt->id = nvavp->syncpt_id;
		syncpt->value = value;
	}

	mutex_unlock(&nvavp->pushbuffer_lock);

	return 0;
}

static void nvavp_unload_ucode(struct nvavp_info *nvavp)
{
	nvmap_unpin(nvavp->nvmap, nvavp->ucode_info.handle);
	nvmap_munmap(nvavp->ucode_info.handle, nvavp->ucode_info.data);
	nvmap_free(nvavp->nvmap, nvavp->ucode_info.handle);
	kfree(nvavp->ucode_info.ucode_bin);
}

static int nvavp_load_ucode(struct nvavp_info *nvavp)
{
	struct nvavp_ucode_info *ucode_info = &nvavp->ucode_info;
	const struct firmware *nvavp_ucode_fw;
	char fw_ucode_file[32];
	void *ptr;
	int ret = 0;

	if (!ucode_info->ucode_bin) {
		sprintf(fw_ucode_file, "nvavp_vid_ucode.bin");

		ret = request_firmware(&nvavp_ucode_fw, fw_ucode_file,
					nvavp->misc_dev.this_device);
		if (ret) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot read ucode firmware '%s'\n",
				fw_ucode_file);
			goto err_req_ucode;
		}
		dev_info(&nvavp->nvhost_dev->dev,
			"read ucode firmware from '%s' (%d bytes)\n",
			fw_ucode_file, nvavp_ucode_fw->size);

		ptr = (void *)nvavp_ucode_fw->data;

		if (strncmp((const char *)ptr, "NVAVPAPP", 8)) {
			dev_info(&nvavp->nvhost_dev->dev,
				"ucode hdr string mismatch\n");
			ret = -EINVAL;
			goto err_req_ucode;
		}
		ptr += 8;
		ucode_info->size = nvavp_ucode_fw->size - 8;

		ucode_info->ucode_bin = kzalloc(ucode_info->size,
						GFP_KERNEL);
		if (!ucode_info->ucode_bin) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot allocate ucode bin\n");
			ret = -ENOMEM;
			goto err_ubin_alloc;
		}

		ucode_info->handle = nvmap_alloc(nvavp->nvmap,
						nvavp->ucode_info.size,
					SZ_1M, NVMAP_HANDLE_UNCACHEABLE);
		if (IS_ERR(ucode_info->handle)) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot create ucode handle\n");
			ret = PTR_ERR(ucode_info->handle);
			goto err_ucode_alloc;
		}
		ucode_info->data = (u8 *)nvmap_mmap(ucode_info->handle);
		if (!ucode_info->data) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot map ucode handle\n");
			ret = -ENOMEM;
			goto err_ucode_mmap;
		}
		ucode_info->phys = nvmap_pin(nvavp->nvmap, ucode_info->handle);
		if (IS_ERR((void *)ucode_info->phys)) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot pin ucode handle\n");
			ret = PTR_ERR((void *)ucode_info->phys);
			goto err_ucode_pin;
		}
		memcpy(ucode_info->ucode_bin, ptr, ucode_info->size);
		release_firmware(nvavp_ucode_fw);
	}

	memcpy(ucode_info->data, ucode_info->ucode_bin, ucode_info->size);
	return 0;

err_ucode_pin:
	nvmap_munmap(ucode_info->handle, ucode_info->data);
err_ucode_mmap:
	nvmap_free(nvavp->nvmap, ucode_info->handle);
err_ucode_alloc:
	kfree(nvavp);
err_ubin_alloc:
	release_firmware(nvavp_ucode_fw);
err_req_ucode:
	return ret;
}

static void nvavp_unload_os(struct nvavp_info *nvavp)
{
#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU)
	nvmap_unpin(nvavp->nvmap, nvavp->os_info.handle);
	nvmap_munmap(nvavp->os_info.handle, nvavp->os_info.data);
	nvmap_free(nvavp->nvmap, nvavp->os_info.handle);
#endif
	kfree(nvavp->os_info.os_bin);
}

static int nvavp_load_os(struct nvavp_info *nvavp, char *fw_os_file)
{
	struct nvavp_os_info *os_info = &nvavp->os_info;
	const struct firmware *nvavp_os_fw;
	void *ptr;
	u32 size;
	int ret = 0;

	if (!os_info->os_bin) {
		ret = request_firmware(&nvavp_os_fw, fw_os_file,
					nvavp->misc_dev.this_device);
		if (ret) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot read os firmware '%s'\n", fw_os_file);
			goto err_req_fw;
		}

		dev_info(&nvavp->nvhost_dev->dev,
			"read firmware from '%s' (%d bytes)\n",
			fw_os_file, nvavp_os_fw->size);

		ptr = (void *)nvavp_os_fw->data;

		if (strncmp((const char *)ptr, "NVAVP-OS", 8)) {
			dev_info(&nvavp->nvhost_dev->dev,
				"os hdr string mismatch\n");
			ret = -EINVAL;
			goto err_os_bin;
		}

		ptr += 8;
		os_info->entry_offset = *((u32 *)ptr);
		ptr += sizeof(u32);
		os_info->control_offset = *((u32 *)ptr);
		ptr += sizeof(u32);
		os_info->debug_offset = *((u32 *)ptr);
		ptr += sizeof(u32);

		size = *((u32 *)ptr);    ptr += sizeof(u32);

		os_info->size = size;
		os_info->os_bin = kzalloc(os_info->size,
						GFP_KERNEL);
		if (!os_info->os_bin) {
			dev_err(&nvavp->nvhost_dev->dev,
				"cannot allocate os bin\n");
			ret = -ENOMEM;
			goto err_os_bin;
		}

		memcpy(os_info->os_bin, ptr, os_info->size);
		memset(os_info->data + os_info->size, 0, SZ_1M - os_info->size);

		dev_info(&nvavp->nvhost_dev->dev,
			"entry=%08x control=%08x debug=%08x size=%d\n",
			os_info->entry_offset, os_info->control_offset,
			os_info->debug_offset, os_info->size);
		release_firmware(nvavp_os_fw);
	}

	memcpy(os_info->data, os_info->os_bin, os_info->size);
	os_info->reset_addr = os_info->phys + os_info->entry_offset;

	dev_info(&nvavp->nvhost_dev->dev,
		"AVP os at vaddr=%p paddr=%lx reset_addr=%p\n",
		os_info->data, (unsigned long)(os_info->phys),
				(void *)os_info->reset_addr);
	return 0;

err_os_bin:
	release_firmware(nvavp_os_fw);
err_req_fw:
	return ret;
}

static int nvavp_init(struct nvavp_info *nvavp)
{
	char fw_os_file[32];
	int ret = 0;

#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU) /* Tegra2 with AVP MMU */
	/* paddr is any address returned from nvmap_pin */
	/* vaddr is AVP_KERNEL_VIRT_BASE */
	dev_info(&nvavp->nvhost_dev->dev,
		"using AVP MMU to relocate AVP os\n");
	sprintf(fw_os_file, "nvavp_os.bin");
	nvavp->os_info.reset_addr = AVP_KERNEL_VIRT_BASE;
#elif defined(CONFIG_TEGRA_AVP_KERNEL_ON_SMMU) /* Tegra3 with SMMU */
	/* paddr is any address behind SMMU */
	/* vaddr is TEGRA_SMMU_BASE */
	dev_info(&nvavp->nvhost_dev->dev,
		"using SMMU at %lx to load AVP kernel\n",
		(unsigned long)nvavp->os_info.phys);
	BUG_ON(nvavp->os_info.phys != 0xeff00000
		&& nvavp->os_info.phys != 0x0ff00000);
	sprintf(fw_os_file, "nvavp_os_%08lx.bin",
		(unsigned long)nvavp->os_info.phys);
	nvavp->os_info.reset_addr = nvavp->os_info.phys;
#else /* nvmem= carveout */
	/* paddr is found in nvmem= carveout */
	/* vaddr is same as paddr */
	/* Find nvmem carveout */
	if (!pfn_valid(__phys_to_pfn(0x8e000000))) {
		nvavp->os_info.phys = 0x8e000000;
	} else if (!pfn_valid(__phys_to_pfn(0x9e000000))) {
		nvavp->os_info.phys = 0x9e000000;
	} else if (!pfn_valid(__phys_to_pfn(0xbe000000))) {
		nvavp->os_info.phys = 0xbe000000;
	} else {
		dev_err(&nvavp->nvhost_dev->dev,
			"cannot find nvmem= carveout to load AVP os\n");
		dev_err(&nvavp->nvhost_dev->dev,
			"check kernel command line "
			"to see if nvmem= is defined\n");
		BUG();
	}
	dev_info(&nvavp->nvhost_dev->dev,
		"using nvmem= carveout at %lx to load AVP os\n",
		nvavp->os_info.phys);
	sprintf(fw_os_file, "nvavp_os_%08lx.bin", nvavp->os_info.phys);
	nvavp->os_info.reset_addr = nvavp->os_info.phys;
	nvavp->os_info.data = ioremap(nvavp->os_info.phys, SZ_1M);
#endif

	ret = nvavp_load_os(nvavp, fw_os_file);
	if (ret) {
		dev_err(&nvavp->nvhost_dev->dev,
			"unable to load os firmware '%s'\n", fw_os_file);
		goto err_exit;
	}

	ret = nvavp_pushbuffer_init(nvavp);
	if (ret) {
		dev_err(&nvavp->nvhost_dev->dev,
			"unable to init pushbuffer\n");
		goto err_exit;
	}

	ret = nvavp_load_ucode(nvavp);
	if (ret) {
		dev_err(&nvavp->nvhost_dev->dev,
			"unable to load ucode\n");
		goto err_exit;
	}

	tegra_init_legacy_irq_cop();

	nvavp_reset_vde(nvavp);
	nvavp_reset_avp(nvavp, nvavp->os_info.reset_addr);
	enable_irq(nvavp->mbox_from_avp_pend_irq);

err_exit:
	return ret;
}

static void nvavp_uninit(struct nvavp_info *nvavp)
{
	disable_irq(nvavp->mbox_from_avp_pend_irq);
	nvavp_pushbuffer_deinit(nvavp);

	nvavp_halt_vde(nvavp);
	nvavp_halt_avp(nvavp);
}

static struct clk *nvavp_clk_get(struct nvavp_info *nvavp, int id)
{
	if (!nvavp)
		return NULL;

	if (id == NVAVP_MODULE_ID_AVP)
		return nvavp->sclk;
	if (id == NVAVP_MODULE_ID_VDE)
		return nvavp->vde_clk;
	if (id == NVAVP_MODULE_ID_EMC)
		return nvavp->emc_clk;

	return NULL;
}

static int nvavp_get_syncpointid_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	u32 id = nvavp->syncpt_id;

	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &id, sizeof(u32)))
			return -EFAULT;
		else
			return 0;
	}
	return -EFAULT;
}

static int nvavp_set_nvmapfd_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_set_nvmap_fd_args buf;
	struct nvmap_client *new_client;
	int fd;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	fd = buf.fd;
	new_client = nvmap_client_get_file(fd);
	if (IS_ERR(new_client))
		return PTR_ERR(new_client);

	clientctx->nvmap = new_client;
	return 0;
}

static int nvavp_pushbuffer_submit_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct nvavp_pushbuffer_submit_hdr hdr;
	u32 *cmdbuf_data;
	struct nvmap_handle *cmdbuf_handle = NULL;
	struct nvmap_handle_ref *cmdbuf_dupe;
	int ret = 0, i;
	unsigned long phys_addr;
	unsigned long virt_addr;
	struct nvavp_syncpt syncpt;

	syncpt.id = NVSYNCPT_INVALID;
	syncpt.value = 0;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&hdr, (void __user *)arg,
			sizeof(struct nvavp_pushbuffer_submit_hdr)))
			return -EFAULT;
	}

	if (!hdr.cmdbuf.mem)
		return 0;

	if (hdr.flags & NVAVP_UCODE_EXT) {
		if ((!uid_eq(current_real_cred()->uid, GLOBAL_ROOT_UID)) &&
			(!uid_eq(current_real_cred()->uid, GLOBAL_TESLA_UID))) {
			dev_err(&nvavp->nvhost_dev->dev,
				"ucode not supported in unprivileged process.\n");
			return -EINVAL;
        }
	}

	if (hdr.num_relocs > ARRAY_SIZE(clientctx->relocs)) {
		dev_err(&nvavp->nvhost_dev->dev,
			"invalid num_relocs %d\n", hdr.num_relocs);
		return -EINVAL;
	}

	if (copy_from_user(clientctx->relocs, (void __user *)hdr.relocs,
			sizeof(struct nvavp_reloc) * hdr.num_relocs)) {
		return -EFAULT;
	}

	cmdbuf_handle = nvmap_get_handle_id(clientctx->nvmap, hdr.cmdbuf.mem);
	if (cmdbuf_handle == NULL) {
		dev_err(&nvavp->nvhost_dev->dev,
			"invalid cmd buffer handle %08x\n", hdr.cmdbuf.mem);
		return -EPERM;
	}

	/* duplicate the new pushbuffer's handle into the nvavp driver's
	 * nvmap context, to ensure that the handle won't be freed as
	 * long as it is in-use by the fb driver */
	cmdbuf_dupe = nvmap_duplicate_handle_id(nvavp->nvmap, hdr.cmdbuf.mem);
	nvmap_handle_put(cmdbuf_handle);

	if (IS_ERR(cmdbuf_dupe)) {
		dev_err(&nvavp->nvhost_dev->dev,
			"could not duplicate handle\n");
		return PTR_ERR(cmdbuf_dupe);
	}

	phys_addr = nvmap_pin(nvavp->nvmap, cmdbuf_dupe);
	if (IS_ERR((void *)phys_addr)) {
		dev_err(&nvavp->nvhost_dev->dev, "could not pin handle\n");
		nvmap_free(nvavp->nvmap, cmdbuf_dupe);
		return PTR_ERR((void *)phys_addr);
	}

	virt_addr = (unsigned long)nvmap_mmap(cmdbuf_dupe);
	if (!virt_addr) {
		dev_err(&nvavp->nvhost_dev->dev, "cannot map cmdbuf handle\n");
		ret = -ENOMEM;
		goto err_cmdbuf_mmap;
	}

	if ((hdr.cmdbuf.offset & 3) ||
			hdr.cmdbuf.offset > cmdbuf_handle->orig_size) {
		dev_err(&nvavp->nvhost_dev->dev, "invalid cmdbuf offset %d\n",
				hdr.cmdbuf.offset);
		ret = -EINVAL;
		goto err_reloc_info;
	}

	cmdbuf_data = (u32 *)(virt_addr + hdr.cmdbuf.offset);

	for (i = 0; i < hdr.num_relocs; i++) {
		u32 *reloc_addr, target_phys_addr, cmdbuf_offset;

		if (clientctx->relocs[i].cmdbuf_mem != hdr.cmdbuf.mem) {
			dev_err(&nvavp->nvhost_dev->dev,
				"reloc info does not match target bufferID\n");
			ret = -EPERM;
			goto err_reloc_info;
		}

		cmdbuf_offset = clientctx->relocs[i].cmdbuf_offset;
		if ((cmdbuf_offset & 3) || cmdbuf_offset >=
				cmdbuf_handle->orig_size - hdr.cmdbuf.offset) {
			dev_err(&nvavp->nvhost_dev->dev,
				"invalid reloc offset in cmdbuf %d\n",
				cmdbuf_offset);
			ret = -EINVAL;
			goto err_reloc_info;
		}

		reloc_addr = cmdbuf_data + (cmdbuf_offset >> 2);

		target_phys_addr = nvmap_handle_address(clientctx->nvmap,
					    clientctx->relocs[i].target);
		target_phys_addr += clientctx->relocs[i].target_offset;
		writel(target_phys_addr, reloc_addr);
	}

	if (hdr.syncpt) {
		struct nvavp_pushbuffer_submit_hdr *user_hdr;
		struct nvavp_syncpt *user_pt;

		user_hdr = (struct nvavp_pushbuffer_submit_hdr *) arg;

		ret = nvavp_pushbuffer_update(nvavp,
					     (phys_addr + hdr.cmdbuf.offset),
					      hdr.cmdbuf.words, &syncpt,
					      (hdr.flags & NVAVP_UCODE_EXT));

		if (copy_from_user(&user_pt, (void __user *)&user_hdr->syncpt,
				   sizeof(user_pt))) {
			ret = -EFAULT;
			goto err_reloc_info;
		}
		if (copy_to_user((void __user *)user_pt, &syncpt,
				sizeof(struct nvavp_syncpt))) {
			ret = -EFAULT;
			goto err_reloc_info;
		}
	} else {
		ret = nvavp_pushbuffer_update(nvavp,
					     (phys_addr + hdr.cmdbuf.offset),
					      hdr.cmdbuf.words, NULL,
					      (hdr.flags & NVAVP_UCODE_EXT));
	}

err_reloc_info:
	nvmap_munmap(cmdbuf_dupe, (void *)virt_addr);
err_cmdbuf_mmap:
	nvmap_unpin(nvavp->nvmap, cmdbuf_dupe);
	nvmap_free(nvavp->nvmap, cmdbuf_dupe);

	return ret;
}

static int nvavp_set_clock_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct clk *c;
	struct nvavp_clock_args config;

	if (copy_from_user(&config, (void __user *)arg, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	if (config.id == NVAVP_MODULE_ID_AVP)
		return 0;

	c = nvavp_clk_get(nvavp, config.id);
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: failed to find clock %u\n", __func__, config.id);
		return -EINVAL;
	}

	clk_set_rate(c, config.rate);

	config.rate = clk_get_rate(c);

	if (copy_to_user((void __user *)arg, &config, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	return 0;
}

static int nvavp_get_clock_ioctl(struct file *filp, unsigned int cmd,
							unsigned long arg)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	struct clk *c;
	struct nvavp_clock_args config;

	if (copy_from_user(&config, (void __user *)arg, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	c = nvavp_clk_get(nvavp, config.id);
	if (IS_ERR_OR_NULL(c))
		return -EINVAL;

	config.rate = clk_get_rate(c);

	if (copy_to_user((void __user *)arg, &config, sizeof(struct nvavp_clock_args)))
		return -EFAULT;

	return 0;
}

static int tegra_nvavp_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct nvavp_info *nvavp = dev_get_drvdata(miscdev->parent);
	int ret = 0;
	struct nvavp_clientctx *clientctx;

	nonseekable_open(inode, filp);

	clientctx = kzalloc(sizeof(*clientctx), GFP_KERNEL);
	if (!clientctx)
		return -ENOMEM;

	mutex_lock(&nvavp->open_lock);

	if (!nvavp->refcount)
		ret = nvavp_init(nvavp);

	if (!ret)
		nvavp->refcount++;

	clientctx->nvmap = nvavp->nvmap;
	clientctx->nvavp = nvavp;

	filp->private_data = clientctx;

	mutex_unlock(&nvavp->open_lock);

	return ret;
}

static int tegra_nvavp_release(struct inode *inode, struct file *filp)
{
	struct nvavp_clientctx *clientctx = filp->private_data;
	struct nvavp_info *nvavp = clientctx->nvavp;
	int ret = 0;

	filp->private_data = NULL;

	mutex_lock(&nvavp->open_lock);

	if (!nvavp->refcount) {
		dev_err(&nvavp->nvhost_dev->dev,
			"releasing while in invalid state\n");
		ret = -EINVAL;
		goto out;
	}

	if (nvavp->refcount > 0)
		nvavp->refcount--;
	if (!nvavp->refcount)
		nvavp_uninit(nvavp);

out:
	mutex_unlock(&nvavp->open_lock);
	kfree(clientctx);
	return ret;
}

static long tegra_nvavp_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;

	if (_IOC_TYPE(cmd) != NVAVP_IOCTL_MAGIC ||
	    _IOC_NR(cmd) < NVAVP_IOCTL_MIN_NR ||
	    _IOC_NR(cmd) > NVAVP_IOCTL_MAX_NR)
		return -EFAULT;

	switch (cmd) {
	case NVAVP_IOCTL_SET_NVMAP_FD:
		ret = nvavp_set_nvmapfd_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_GET_SYNCPOINT_ID:
		ret = nvavp_get_syncpointid_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_PUSH_BUFFER_SUBMIT:
		ret = nvavp_pushbuffer_submit_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_SET_CLOCK:
		ret = nvavp_set_clock_ioctl(filp, cmd, arg);
		break;
	case NVAVP_IOCTL_GET_CLOCK:
		ret = nvavp_get_clock_ioctl(filp, cmd, arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const struct file_operations tegra_nvavp_fops = {
	.owner		= THIS_MODULE,
	.open		= tegra_nvavp_open,
	.release	= tegra_nvavp_release,
	.unlocked_ioctl	= tegra_nvavp_ioctl,
};

static int tegra_nvavp_probe(struct nvhost_device *ndev)
{
	struct nvavp_info *nvavp;
	int irq;
	unsigned int heap_mask;
	u32 iovmm_addr;
	int ret = 0;

	irq = nvhost_get_irq_byname(ndev, "mbox_from_avp_pending");
	if (irq < 0) {
		dev_err(&ndev->dev, "invalid nvhost data\n");
		return -EINVAL;
	}


	nvavp = kzalloc(sizeof(struct nvavp_info), GFP_KERNEL);
	if (!nvavp) {
		dev_err(&ndev->dev, "cannot allocate avp_info\n");
		return -ENOMEM;
	}

	memset(nvavp, 0, sizeof(*nvavp));

	if (!ndev->host) {
		dev_err(&ndev->dev, "ndev->host not set\n");
		ret = -EINVAL;
		goto err_get_syncpt;
	}

	nvavp->nvhost_syncpt = &ndev->host->syncpt;
	if (!nvavp->nvhost_syncpt) {
		dev_err(&ndev->dev, "cannot get syncpt handle\n");
		ret = -ENOENT;
		goto err_get_syncpt;
	}

	nvavp->nvmap = nvmap_create_client(nvmap_dev, "nvavp_drv");
	if (IS_ERR_OR_NULL(nvavp->nvmap)) {
		dev_err(&ndev->dev, "cannot create nvmap client\n");
		ret = PTR_ERR(nvavp->nvmap);
		goto err_nvmap_create_drv_client;
	}

#if defined(CONFIG_TEGRA_AVP_KERNEL_ON_MMU) /* Tegra2 with AVP MMU */
	heap_mask = NVMAP_HEAP_CARVEOUT_GENERIC;
#elif defined(CONFIG_TEGRA_AVP_KERNEL_ON_SMMU) /* Tegra3 with SMMU */
	heap_mask = NVMAP_HEAP_IOVMM;
#else /* nvmem= carveout */
	heap_mask = 0;
#endif
	switch (heap_mask) {
	case NVMAP_HEAP_IOVMM:
		iovmm_addr = 0x0ff00000;

		/* Tegra3 A01 has different SMMU address */
		if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3
			&& tegra_get_revision() == TEGRA_REVISION_A01) {
			iovmm_addr = 0xeff00000;
		}

		nvavp->os_info.handle = nvmap_alloc_iovm(nvavp->nvmap, SZ_1M,
						L1_CACHE_BYTES,
						NVMAP_HANDLE_UNCACHEABLE,
						iovmm_addr);
		if (IS_ERR_OR_NULL(nvavp->os_info.handle)) {
			dev_err(&ndev->dev,
				"cannot create os handle\n");
			ret = PTR_ERR(nvavp->os_info.handle);
			goto err_nvmap_alloc;
		}

		nvavp->os_info.data = nvmap_mmap(nvavp->os_info.handle);
		if (!nvavp->os_info.data) {
			dev_err(&ndev->dev,
				"cannot map os handle\n");
			ret = -ENOMEM;
			goto err_nvmap_mmap;
		}

		nvavp->os_info.phys =
			nvmap_pin(nvavp->nvmap, nvavp->os_info.handle);
		if (IS_ERR_OR_NULL((void *)nvavp->os_info.phys)) {
			dev_err(&ndev->dev,
				"cannot pin os handle\n");
			ret = PTR_ERR((void *)nvavp->os_info.phys);
			goto err_nvmap_pin;
		}

		dev_info(&ndev->dev,
			"allocated IOVM at %lx for AVP os\n",
			(unsigned long)nvavp->os_info.phys);
		break;
	case NVMAP_HEAP_CARVEOUT_GENERIC:
		nvavp->os_info.handle = nvmap_alloc(nvavp->nvmap, SZ_1M, SZ_1M,
						NVMAP_HANDLE_UNCACHEABLE);
		if (IS_ERR_OR_NULL(nvavp->os_info.handle)) {
			dev_err(&ndev->dev, "cannot create AVP os handle\n");
			ret = PTR_ERR(nvavp->os_info.handle);
			goto err_nvmap_alloc;
		}

		nvavp->os_info.data = nvmap_mmap(nvavp->os_info.handle);
		if (!nvavp->os_info.data) {
			dev_err(&ndev->dev, "cannot map AVP os handle\n");
			ret = -ENOMEM;
			goto err_nvmap_mmap;
		}

		nvavp->os_info.phys = nvmap_pin(nvavp->nvmap,
					nvavp->os_info.handle);
		if (IS_ERR_OR_NULL((void *)nvavp->os_info.phys)) {
			dev_err(&ndev->dev, "cannot pin AVP os handle\n");
			ret = PTR_ERR((void *)nvavp->os_info.phys);
			goto err_nvmap_pin;
		}

		dev_info(&ndev->dev,
			"allocated carveout memory at %lx for AVP os\n",
			(unsigned long)nvavp->os_info.phys);
		break;
	default:
		dev_err(&ndev->dev, "invalid/non-supported heap for AVP os\n");
		ret = -EINVAL;
		goto err_get_syncpt;
	}

	nvavp->mbox_from_avp_pend_irq = irq;
	mutex_init(&nvavp->open_lock);
	mutex_init(&nvavp->pushbuffer_lock);

	/* TODO DO NOT USE NVAVP DEVICE */
	nvavp->cop_clk = clk_get(&ndev->dev, "cop");
	if (IS_ERR(nvavp->cop_clk)) {
		dev_err(&ndev->dev, "cannot get cop clock\n");
		ret = -ENOENT;
		goto err_get_cop_clk;
	}

	nvavp->vde_clk = clk_get(&ndev->dev, "vde");
	if (IS_ERR(nvavp->vde_clk)) {
		dev_err(&ndev->dev, "cannot get vde clock\n");
		ret = -ENOENT;
		goto err_get_vde_clk;
	}

	nvavp->bsev_clk = clk_get(&ndev->dev, "bsev");
	if (IS_ERR(nvavp->bsev_clk)) {
		dev_err(&ndev->dev, "cannot get bsev clock\n");
		ret = -ENOENT;
		goto err_get_bsev_clk;
	}

	nvavp->sclk = clk_get(&ndev->dev, "sclk");
	if (IS_ERR(nvavp->sclk)) {
		dev_err(&ndev->dev, "cannot get avp.sclk clock\n");
		ret = -ENOENT;
		goto err_get_sclk;
	}

	nvavp->emc_clk = clk_get(&ndev->dev, "emc");
	if (IS_ERR(nvavp->emc_clk)) {
		dev_err(&ndev->dev, "cannot get emc clock\n");
		ret = -ENOENT;
		goto err_get_emc_clk;
	}

	nvavp_halt_avp(nvavp);

	nvavp->misc_dev.minor = MISC_DYNAMIC_MINOR;
	nvavp->misc_dev.name = "tegra_avpchannel";
	nvavp->misc_dev.fops = &tegra_nvavp_fops;
	nvavp->misc_dev.mode = S_IRWXUGO;
	nvavp->misc_dev.parent = &ndev->dev;

	ret = misc_register(&nvavp->misc_dev);
	if (ret) {
		dev_err(&ndev->dev, "unable to register misc device!\n");
		goto err_misc_reg;
	}

	ret = request_irq(irq, nvavp_mbox_pending_isr, 0,
			  TEGRA_NVAVP_NAME, nvavp);
	if (ret) {
		dev_err(&ndev->dev, "cannot register irq handler\n");
		goto err_req_irq_pend;
	}
	disable_irq(nvavp->mbox_from_avp_pend_irq);

	nvhost_set_drvdata(ndev, nvavp);
	nvavp->nvhost_dev = ndev;

	return 0;

err_req_irq_pend:
	misc_deregister(&nvavp->misc_dev);
err_misc_reg:
	clk_put(nvavp->emc_clk);
err_get_emc_clk:
	clk_put(nvavp->sclk);
err_get_sclk:
	clk_put(nvavp->bsev_clk);
err_get_bsev_clk:
	clk_put(nvavp->vde_clk);
err_get_vde_clk:
	clk_put(nvavp->cop_clk);
err_get_cop_clk:
	nvmap_unpin(nvavp->nvmap, nvavp->os_info.handle);
err_nvmap_pin:
	nvmap_munmap(nvavp->os_info.handle, nvavp->os_info.data);
err_nvmap_mmap:
	nvmap_free(nvavp->nvmap, nvavp->os_info.handle);
err_nvmap_alloc:
	nvmap_client_put(nvavp->nvmap);
err_nvmap_create_drv_client:
err_get_syncpt:
	kfree(nvavp);
	return ret;
}

static int tegra_nvavp_remove(struct nvhost_device *ndev)
{
	struct nvavp_info *nvavp = nvhost_get_drvdata(ndev);

	if (!nvavp)
		return 0;

	mutex_lock(&nvavp->open_lock);
	if (nvavp->refcount) {
		mutex_unlock(&nvavp->open_lock);
		return -EBUSY;
	}
	mutex_unlock(&nvavp->open_lock);

	nvavp_unload_ucode(nvavp);
	nvavp_unload_os(nvavp);

	misc_deregister(&nvavp->misc_dev);

	clk_put(nvavp->vde_clk);
	clk_put(nvavp->cop_clk);

	nvmap_client_put(nvavp->nvmap);

	kfree(nvavp);
	return 0;
}

#ifdef CONFIG_PM
static int tegra_nvavp_suspend(struct nvhost_device *ndev, pm_message_t state)
{
	return 0;
}

static int tegra_nvavp_resume(struct nvhost_device *ndev)
{
	return 0;
}
#endif

static struct nvhost_driver tegra_nvavp_driver = {
	.driver	= {
		.name	= TEGRA_NVAVP_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= tegra_nvavp_probe,
	.remove		= tegra_nvavp_remove,
#ifdef CONFIG_PM
	.suspend	= tegra_nvavp_suspend,
	.resume		= tegra_nvavp_resume,
#endif
};

static int __init tegra_nvavp_init(void)
{
	return nvhost_driver_register(&tegra_nvavp_driver);
}

static void __exit tegra_nvavp_exit(void)
{
	nvhost_driver_unregister(&tegra_nvavp_driver);
}

module_init(tegra_nvavp_init);
module_exit(tegra_nvavp_exit);

MODULE_AUTHOR("NVIDIA");
MODULE_DESCRIPTION("Channel based AVP driver for Tegra");
MODULE_VERSION("1.0");
MODULE_LICENSE("Dual BSD/GPL");
