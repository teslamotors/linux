/*
 * GK20A Graphics
 *
 * Copyright (c) 2011-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/thermal.h>
#include <asm/cacheflush.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/tegra-powergate.h>
#include <linux/clk/tegra.h>
#include <linux/kthread.h>
#include <linux/platform/tegra/common.h>
#include <linux/reset.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/version.h>

#include "gk20a.h"
#include "nvgpu_common.h"
#include "debug_gk20a.h"
#include "ctrl_gk20a.h"
#include "channel_sync_gk20a.h"

#include "hw_mc_gk20a.h"
#include "hw_timer_gk20a.h"
#include "hw_bus_gk20a.h"
#include "hw_sim_gk20a.h"
#include "hw_top_gk20a.h"
#include "hw_ltc_gk20a.h"
#include "hw_gr_gk20a.h"
#include "hw_fb_gk20a.h"
#include "gk20a_scale.h"
#include "ctxsw_trace_gk20a.h"
#include "dbg_gpu_gk20a.h"
#include "gk20a_allocator.h"
#include "hal.h"
#include "vgpu/vgpu.h"
#include "pci.h"
#include "pstate/pstate.h"


#define CREATE_TRACE_POINTS
#include <trace/events/gk20a.h>

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
#include "nvgpu_gpuid_t18x.h"
#endif

#ifdef CONFIG_TEGRA_19x_GPU
#include "nvgpu_gpuid_t19x.h"
#endif

#ifdef CONFIG_ARM64
#define __cpuc_flush_dcache_area __flush_dcache_area
#endif

#define CLASS_NAME "nvidia-gpu"
/* TODO: Change to e.g. "nvidia-gpu%s" once we have symlinks in place. */

#define GK20A_NUM_CDEVS 7

#if defined(GK20A_DEBUG)
u32 gk20a_dbg_mask = GK20A_DEFAULT_DBG_MASK;
u32 gk20a_dbg_ftrace;
#endif

#define GK20A_WAIT_FOR_IDLE_MS	2000

static int gk20a_pm_prepare_poweroff(struct device *dev);

#ifdef CONFIG_DEBUG_FS
static int railgate_residency_show(struct seq_file *s, void *data)
{
	struct device *dev = s->private;
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = get_gk20a(dev);
	unsigned long time_since_last_state_transition_ms;
	unsigned long total_rail_gate_time_ms;
	unsigned long total_rail_ungate_time_ms;

	if (platform->is_railgated(dev)) {
		time_since_last_state_transition_ms =
				jiffies_to_msecs(jiffies -
				g->pstats.last_rail_gate_complete);
		total_rail_ungate_time_ms = g->pstats.total_rail_ungate_time_ms;
		total_rail_gate_time_ms =
					g->pstats.total_rail_gate_time_ms +
					time_since_last_state_transition_ms;
	} else {
		time_since_last_state_transition_ms =
				jiffies_to_msecs(jiffies -
				g->pstats.last_rail_ungate_complete);
		total_rail_gate_time_ms = g->pstats.total_rail_gate_time_ms;
		total_rail_ungate_time_ms =
					g->pstats.total_rail_ungate_time_ms +
					time_since_last_state_transition_ms;
	}

	seq_printf(s, "Time with Rails Gated: %lu ms\n"
			"Time with Rails UnGated: %lu ms\n"
			"Total railgating cycles: %lu\n",
			total_rail_gate_time_ms,
			total_rail_ungate_time_ms,
			g->pstats.railgating_cycle_count - 1);
	return 0;

}

static int railgate_residency_open(struct inode *inode, struct file *file)
{
	return single_open(file, railgate_residency_show, inode->i_private);
}

static const struct file_operations railgate_residency_fops = {
	.open		= railgate_residency_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int gk20a_railgating_debugfs_init(struct device *dev)
{
	struct dentry *d;
	struct gk20a_platform *platform = dev_get_drvdata(dev);

	if (!platform->can_railgate)
		return 0;

	d = debugfs_create_file(
		"railgate_residency", S_IRUGO|S_IWUSR, platform->debugfs, dev,
						&railgate_residency_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}
#endif

static inline void set_gk20a(struct platform_device *pdev, struct gk20a *gk20a)
{
	gk20a_get_platform(&pdev->dev)->g = gk20a;
}

const struct file_operations gk20a_channel_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_channel_release,
	.open = gk20a_channel_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_channel_ioctl,
#endif
	.unlocked_ioctl = gk20a_channel_ioctl,
};

static const struct file_operations gk20a_ctrl_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_ctrl_dev_release,
	.open = gk20a_ctrl_dev_open,
	.unlocked_ioctl = gk20a_ctrl_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_ctrl_dev_ioctl,
#endif
};

static const struct file_operations gk20a_dbg_ops = {
	.owner = THIS_MODULE,
	.release        = gk20a_dbg_gpu_dev_release,
	.open           = gk20a_dbg_gpu_dev_open,
	.unlocked_ioctl = gk20a_dbg_gpu_dev_ioctl,
	.poll		= gk20a_dbg_gpu_dev_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_dbg_gpu_dev_ioctl,
#endif
};

static const struct file_operations gk20a_as_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_as_dev_release,
	.open = gk20a_as_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_as_dev_ioctl,
#endif
	.unlocked_ioctl = gk20a_as_dev_ioctl,
};

/*
 * Note: We use a different 'open' to trigger handling of the profiler session.
 * Most of the code is shared between them...  Though, at some point if the
 * code does get too tangled trying to handle each in the same path we can
 * separate them cleanly.
 */
static const struct file_operations gk20a_prof_ops = {
	.owner = THIS_MODULE,
	.release        = gk20a_dbg_gpu_dev_release,
	.open           = gk20a_prof_gpu_dev_open,
	.unlocked_ioctl = gk20a_dbg_gpu_dev_ioctl,
	/* .mmap           = gk20a_prof_gpu_dev_mmap,*/
	/*int (*mmap) (struct file *, struct vm_area_struct *);*/
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_dbg_gpu_dev_ioctl,
#endif
};

static const struct file_operations gk20a_tsg_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_tsg_dev_release,
	.open = gk20a_tsg_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_tsg_dev_ioctl,
#endif
	.unlocked_ioctl = gk20a_tsg_dev_ioctl,
};

static const struct file_operations gk20a_ctxsw_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_ctxsw_dev_release,
	.open = gk20a_ctxsw_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_ctxsw_dev_ioctl,
#endif
	.unlocked_ioctl = gk20a_ctxsw_dev_ioctl,
	.poll = gk20a_ctxsw_dev_poll,
	.read = gk20a_ctxsw_dev_read,
	.mmap = gk20a_ctxsw_dev_mmap,
};

static const struct file_operations gk20a_sched_ops = {
	.owner = THIS_MODULE,
	.release = gk20a_sched_dev_release,
	.open = gk20a_sched_dev_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gk20a_sched_dev_ioctl,
#endif
	.unlocked_ioctl = gk20a_sched_dev_ioctl,
	.poll = gk20a_sched_dev_poll,
	.read = gk20a_sched_dev_read,
};

void __nvgpu_check_gpu_state(struct gk20a *g)
{
	u32 boot_0 = readl(g->regs + mc_boot_0_r());

	if (boot_0 == 0xffffffff) {
		/*
		 * This typically happens when there are thermal issues.
		 * Set the panic timeout to a higher value
		 * so that we cool off.
		 */
#define DELAY_PANIC_REBOOT 20
		if (panic_timeout < DELAY_PANIC_REBOOT)
			panic_timeout = DELAY_PANIC_REBOOT;
		panic("nvgpu: GPU has disappeared from bus!!\n");
	}
}

static inline void sim_writel(struct gk20a *g, u32 r, u32 v)
{
	writel(v, g->sim.regs+r);
}

static inline u32 sim_readl(struct gk20a *g, u32 r)
{
	return readl(g->sim.regs+r);
}

/*
 * Locks out the driver from accessing GPU registers. This prevents access to
 * thse registers after the GPU has been clock or power gated. This should help
 * find annoying bugs where register reads and writes are silently dropped
 * after the GPU has been turned off. On older chips these reads and writes can
 * also lock the entire CPU up.
 */
int gk20a_lockout_registers(struct gk20a *g)
{
	g->regs = NULL;
	g->bar1 = NULL;

	return 0;
}

/*
 * Undoes gk20a_lockout_registers().
 */
int gk20a_restore_registers(struct gk20a *g)
{
	g->regs = g->regs_saved;
	g->bar1 = g->bar1_saved;

	return 0;
}

void __gk20a_warn_on_no_regs(void)
{
	WARN_ONCE(1, "Attempted access to GPU regs after unmapping!");
}

static void kunmap_and_free_iopage(void **kvaddr, struct page **page)
{
	if (*kvaddr) {
		kunmap(*kvaddr);
		*kvaddr = NULL;
	}
	if (*page) {
		__free_page(*page);
		*page = NULL;
	}
}

static void gk20a_free_sim_support(struct gk20a *g)
{
	/* free sim mappings, bfrs */
	kunmap_and_free_iopage(&g->sim.send_bfr.kvaddr,
			       &g->sim.send_bfr.page);

	kunmap_and_free_iopage(&g->sim.recv_bfr.kvaddr,
			       &g->sim.recv_bfr.page);

	kunmap_and_free_iopage(&g->sim.msg_bfr.kvaddr,
			       &g->sim.msg_bfr.page);
}

static void gk20a_remove_sim_support(struct sim_gk20a *s)
{
	struct gk20a *g = s->g;
	if (g->sim.regs)
		sim_writel(g, sim_config_r(), sim_config_mode_disabled_v());
	gk20a_free_sim_support(g);
}

static int alloc_and_kmap_iopage(struct device *d,
				 void **kvaddr,
				 u64 *phys,
				 struct page **page)
{
	int err = 0;
	*page = alloc_page(GFP_KERNEL);

	if (!*page) {
		err = -ENOMEM;
		dev_err(d, "couldn't allocate io page\n");
		goto fail;
	}

	*kvaddr = kmap(*page);
	if (!*kvaddr) {
		err = -ENOMEM;
		dev_err(d, "couldn't kmap io page\n");
		goto fail;
	}
	*phys = page_to_phys(*page);
	return 0;

 fail:
	kunmap_and_free_iopage(kvaddr, page);
	return err;

}

static void __iomem *gk20a_ioremap_resource(struct platform_device *dev, int i,
					    struct resource **out)
{
	struct resource *r = platform_get_resource(dev, IORESOURCE_MEM, i);
	if (!r)
		return NULL;
	if (out)
		*out = r;
	return devm_ioremap_resource(&dev->dev, r);
}

/* TBD: strip from released */
static int gk20a_init_sim_support(struct platform_device *pdev)
{
	int err = 0;
	struct device *dev = &pdev->dev;
	struct gk20a *g = get_gk20a(dev);
	u64 phys;

	g->sim.g = g;
	g->sim.regs = gk20a_ioremap_resource(pdev, GK20A_SIM_IORESOURCE_MEM,
					     &g->sim.reg_mem);
	if (IS_ERR(g->sim.regs)) {
		dev_err(dev, "failed to remap gk20a sim regs\n");
		err = PTR_ERR(g->sim.regs);
		goto fail;
	}

	/* allocate sim event/msg buffers */
	err = alloc_and_kmap_iopage(dev, &g->sim.send_bfr.kvaddr,
				    &g->sim.send_bfr.phys,
				    &g->sim.send_bfr.page);

	err = err || alloc_and_kmap_iopage(dev, &g->sim.recv_bfr.kvaddr,
					   &g->sim.recv_bfr.phys,
					   &g->sim.recv_bfr.page);

	err = err || alloc_and_kmap_iopage(dev, &g->sim.msg_bfr.kvaddr,
					   &g->sim.msg_bfr.phys,
					   &g->sim.msg_bfr.page);

	if (!(g->sim.send_bfr.kvaddr && g->sim.recv_bfr.kvaddr &&
	      g->sim.msg_bfr.kvaddr)) {
		dev_err(dev, "couldn't allocate all sim buffers\n");
		goto fail;
	}

	/*mark send ring invalid*/
	sim_writel(g, sim_send_ring_r(), sim_send_ring_status_invalid_f());

	/*read get pointer and make equal to put*/
	g->sim.send_ring_put = sim_readl(g, sim_send_get_r());
	sim_writel(g, sim_send_put_r(), g->sim.send_ring_put);

	/*write send ring address and make it valid*/
	phys = g->sim.send_bfr.phys;
	sim_writel(g, sim_send_ring_hi_r(),
		   sim_send_ring_hi_addr_f(u64_hi32(phys)));
	sim_writel(g, sim_send_ring_r(),
		   sim_send_ring_status_valid_f() |
		   sim_send_ring_target_phys_pci_coherent_f() |
		   sim_send_ring_size_4kb_f() |
		   sim_send_ring_addr_lo_f(phys >> PAGE_SHIFT));

	/*repeat for recv ring (but swap put,get as roles are opposite) */
	sim_writel(g, sim_recv_ring_r(), sim_recv_ring_status_invalid_f());

	/*read put pointer and make equal to get*/
	g->sim.recv_ring_get = sim_readl(g, sim_recv_put_r());
	sim_writel(g, sim_recv_get_r(), g->sim.recv_ring_get);

	/*write send ring address and make it valid*/
	phys = g->sim.recv_bfr.phys;
	sim_writel(g, sim_recv_ring_hi_r(),
		   sim_recv_ring_hi_addr_f(u64_hi32(phys)));
	sim_writel(g, sim_recv_ring_r(),
		   sim_recv_ring_status_valid_f() |
		   sim_recv_ring_target_phys_pci_coherent_f() |
		   sim_recv_ring_size_4kb_f() |
		   sim_recv_ring_addr_lo_f(phys >> PAGE_SHIFT));

	g->sim.remove_support = gk20a_remove_sim_support;
	return 0;

 fail:
	gk20a_free_sim_support(g);
	return err;
}

static inline u32 sim_msg_header_size(void)
{
	return 24;/*TBD: fix the header to gt this from NV_VGPU_MSG_HEADER*/
}

static inline u32 *sim_msg_bfr(struct gk20a *g, u32 byte_offset)
{
	return (u32 *)(g->sim.msg_bfr.kvaddr + byte_offset);
}

static inline u32 *sim_msg_hdr(struct gk20a *g, u32 byte_offset)
{
	return sim_msg_bfr(g, byte_offset); /*starts at 0*/
}

static inline u32 *sim_msg_param(struct gk20a *g, u32 byte_offset)
{
	/*starts after msg header/cmn*/
	return sim_msg_bfr(g, byte_offset + sim_msg_header_size());
}

static inline void sim_write_hdr(struct gk20a *g, u32 func, u32 size)
{
	/*memset(g->sim.msg_bfr.kvaddr,0,min(PAGE_SIZE,size));*/
	*sim_msg_hdr(g, sim_msg_signature_r()) = sim_msg_signature_valid_v();
	*sim_msg_hdr(g, sim_msg_result_r())    = sim_msg_result_rpc_pending_v();
	*sim_msg_hdr(g, sim_msg_spare_r())     = sim_msg_spare__init_v();
	*sim_msg_hdr(g, sim_msg_function_r())  = func;
	*sim_msg_hdr(g, sim_msg_length_r())    = size + sim_msg_header_size();
}

static inline u32 sim_escape_read_hdr_size(void)
{
	return 12; /*TBD: fix NV_VGPU_SIM_ESCAPE_READ_HEADER*/
}

static u32 *sim_send_ring_bfr(struct gk20a *g, u32 byte_offset)
{
	return (u32 *)(g->sim.send_bfr.kvaddr + byte_offset);
}

static int rpc_send_message(struct gk20a *g)
{
	/* calculations done in units of u32s */
	u32 send_base = sim_send_put_pointer_v(g->sim.send_ring_put) * 2;
	u32 dma_offset = send_base + sim_dma_r()/sizeof(u32);
	u32 dma_hi_offset = send_base + sim_dma_hi_r()/sizeof(u32);

	*sim_send_ring_bfr(g, dma_offset*sizeof(u32)) =
		sim_dma_target_phys_pci_coherent_f() |
		sim_dma_status_valid_f() |
		sim_dma_size_4kb_f() |
		sim_dma_addr_lo_f(g->sim.msg_bfr.phys >> PAGE_SHIFT);

	*sim_send_ring_bfr(g, dma_hi_offset*sizeof(u32)) =
		u64_hi32(g->sim.msg_bfr.phys);

	*sim_msg_hdr(g, sim_msg_sequence_r()) = g->sim.sequence_base++;

	g->sim.send_ring_put = (g->sim.send_ring_put + 2 * sizeof(u32)) %
		PAGE_SIZE;

	__cpuc_flush_dcache_area(g->sim.msg_bfr.kvaddr, PAGE_SIZE);
	__cpuc_flush_dcache_area(g->sim.send_bfr.kvaddr, PAGE_SIZE);
	__cpuc_flush_dcache_area(g->sim.recv_bfr.kvaddr, PAGE_SIZE);

	/* Update the put pointer. This will trap into the host. */
	sim_writel(g, sim_send_put_r(), g->sim.send_ring_put);

	return 0;
}

static inline u32 *sim_recv_ring_bfr(struct gk20a *g, u32 byte_offset)
{
	return (u32 *)(g->sim.recv_bfr.kvaddr + byte_offset);
}

static int rpc_recv_poll(struct gk20a *g)
{
	u64 recv_phys_addr;

	/* XXX This read is not required (?) */
	/*pVGpu->recv_ring_get = VGPU_REG_RD32(pGpu, NV_VGPU_RECV_GET);*/

	/* Poll the recv ring get pointer in an infinite loop*/
	do {
		g->sim.recv_ring_put = sim_readl(g, sim_recv_put_r());
	} while (g->sim.recv_ring_put == g->sim.recv_ring_get);

	/* process all replies */
	while (g->sim.recv_ring_put != g->sim.recv_ring_get) {
		/* these are in u32 offsets*/
		u32 dma_lo_offset =
			sim_recv_put_pointer_v(g->sim.recv_ring_get)*2 + 0;
		u32 dma_hi_offset = dma_lo_offset + 1;
		u32 recv_phys_addr_lo = sim_dma_addr_lo_v(
				*sim_recv_ring_bfr(g, dma_lo_offset*4));
		u32 recv_phys_addr_hi = sim_dma_hi_addr_v(
				*sim_recv_ring_bfr(g, dma_hi_offset*4));

		recv_phys_addr = (u64)recv_phys_addr_hi << 32 |
				 (u64)recv_phys_addr_lo << PAGE_SHIFT;

		if (recv_phys_addr != g->sim.msg_bfr.phys) {
			dev_err(dev_from_gk20a(g), "%s Error in RPC reply\n",
				__func__);
			return -1;
		}

		/* Update GET pointer */
		g->sim.recv_ring_get = (g->sim.recv_ring_get + 2*sizeof(u32)) %
			PAGE_SIZE;

		__cpuc_flush_dcache_area(g->sim.msg_bfr.kvaddr, PAGE_SIZE);
		__cpuc_flush_dcache_area(g->sim.send_bfr.kvaddr, PAGE_SIZE);
		__cpuc_flush_dcache_area(g->sim.recv_bfr.kvaddr, PAGE_SIZE);

		sim_writel(g, sim_recv_get_r(), g->sim.recv_ring_get);

		g->sim.recv_ring_put = sim_readl(g, sim_recv_put_r());
	}

	return 0;
}

static int issue_rpc_and_wait(struct gk20a *g)
{
	int err;

	err = rpc_send_message(g);
	if (err) {
		dev_err(dev_from_gk20a(g), "%s failed rpc_send_message\n",
			__func__);
		return err;
	}

	err = rpc_recv_poll(g);
	if (err) {
		dev_err(dev_from_gk20a(g), "%s failed rpc_recv_poll\n",
			__func__);
		return err;
	}

	/* Now check if RPC really succeeded */
	if (*sim_msg_hdr(g, sim_msg_result_r()) != sim_msg_result_success_v()) {
		dev_err(dev_from_gk20a(g), "%s received failed status!\n",
			__func__);
		return -(*sim_msg_hdr(g, sim_msg_result_r()));
	}
	return 0;
}

int gk20a_sim_esc_read(struct gk20a *g, char *path, u32 index, u32 count, u32 *data)
{
	int err;
	size_t pathlen = strlen(path);
	u32 data_offset;

	sim_write_hdr(g, sim_msg_function_sim_escape_read_v(),
		      sim_escape_read_hdr_size());
	*sim_msg_param(g, 0) = index;
	*sim_msg_param(g, 4) = count;
	data_offset = roundup(0xc +  pathlen + 1, sizeof(u32));
	*sim_msg_param(g, 8) = data_offset;
	strcpy((char *)sim_msg_param(g, 0xc), path);

	err = issue_rpc_and_wait(g);

	if (!err)
		memcpy(data, sim_msg_param(g, data_offset), count);
	return err;
}

static irqreturn_t gk20a_intr_isr_stall(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;

	return g->ops.mc.isr_stall(g);
}

static irqreturn_t gk20a_intr_isr_nonstall(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;

	return g->ops.mc.isr_nonstall(g);
}

void gk20a_pbus_isr(struct gk20a *g)
{
	u32 val, err_code;
	val = gk20a_readl(g, bus_intr_0_r());
	if (val & (bus_intr_0_pri_squash_m() |
			bus_intr_0_pri_fecserr_m() |
			bus_intr_0_pri_timeout_m())) {
		gk20a_dbg(gpu_dbg_intr, "pmc_enable : 0x%x",
			gk20a_readl(g, mc_enable_r()));
		gk20a_dbg(gpu_dbg_intr, "NV_PBUS_INTR_0 : 0x%x", val);
		gk20a_dbg(gpu_dbg_intr,
			"NV_PTIMER_PRI_TIMEOUT_SAVE_0: 0x%x\n",
			gk20a_readl(g, timer_pri_timeout_save_0_r()));
		gk20a_dbg(gpu_dbg_intr,
			"NV_PTIMER_PRI_TIMEOUT_SAVE_1: 0x%x\n",
			gk20a_readl(g, timer_pri_timeout_save_1_r()));
		err_code = gk20a_readl(g, timer_pri_timeout_fecs_errcode_r());
		gk20a_dbg(gpu_dbg_intr,
			"NV_PTIMER_PRI_TIMEOUT_FECS_ERRCODE: 0x%x\n",
			err_code);
		if (err_code == 0xbadf13)
			gk20a_dbg(gpu_dbg_intr,
			"NV_PGRAPH_PRI_GPC0_GPCCS_FS_GPC: 0x%x\n",
			gk20a_readl(g, gr_gpc0_fs_gpc_r()));

	}

	if (val)
		gk20a_dbg(gpu_dbg_intr,
			"Unhandled pending pbus interrupt\n");

	gk20a_writel(g, bus_intr_0_r(), val);
}

static irqreturn_t gk20a_intr_thread_stall(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;
	return g->ops.mc.isr_thread_stall(g);
}

static irqreturn_t gk20a_intr_thread_nonstall(int irq, void *dev_id)
{
	struct gk20a *g = dev_id;
	return g->ops.mc.isr_thread_nonstall(g);
}

void gk20a_remove_support(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);

#ifdef CONFIG_TEGRA_COMMON
	tegra_unregister_idle_unidle();
#endif
	if (g->dbg_regops_tmp_buf)
		kfree(g->dbg_regops_tmp_buf);

	nvgpu_wait_for_deferred_interrupts(g);

	gk20a_channel_cancel_pending_sema_waits(g);

	if (g->pmu.remove_support)
		g->pmu.remove_support(&g->pmu);

	if (g->gr.remove_support)
		g->gr.remove_support(&g->gr);

	if (g->mm.remove_ce_support)
		g->mm.remove_ce_support(&g->mm);

	if (g->fifo.remove_support)
		g->fifo.remove_support(&g->fifo);

	if (g->mm.remove_support)
		g->mm.remove_support(&g->mm);

	if (g->sim.remove_support)
		g->sim.remove_support(&g->sim);

	/* free mappings to registers, etc */

	if (g->regs) {
		iounmap(g->regs);
		g->regs = NULL;
	}
	if (g->bar1) {
		iounmap(g->bar1);
		g->bar1 = NULL;
	}
}

static int gk20a_init_support(struct platform_device *dev)
{
	int err = 0;
	struct gk20a *g = get_gk20a(&dev->dev);

#ifdef CONFIG_TEGRA_COMMON
	tegra_register_idle_unidle(gk20a_do_idle, gk20a_do_unidle);
#endif

	g->regs = gk20a_ioremap_resource(dev, GK20A_BAR0_IORESOURCE_MEM,
					 &g->reg_mem);
	if (IS_ERR(g->regs)) {
		dev_err(dev_from_gk20a(g), "failed to remap gk20a registers\n");
		err = PTR_ERR(g->regs);
		goto fail;
	}

	g->bar1 = gk20a_ioremap_resource(dev, GK20A_BAR1_IORESOURCE_MEM,
					 &g->bar1_mem);
	if (IS_ERR(g->bar1)) {
		dev_err(dev_from_gk20a(g), "failed to remap gk20a bar1\n");
		err = PTR_ERR(g->bar1);
		goto fail;
	}

	if (tegra_cpu_is_asim()) {
		err = gk20a_init_sim_support(dev);
		if (err)
			goto fail;
	}

	return 0;

 fail:
	return err;
}

static int gk20a_pm_prepare_poweroff(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	int ret = 0;
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	gk20a_dbg_fn("");

	mutex_lock(&g->poweroff_lock);

	if (!g->power_on)
		goto done;

	if (gk20a_fifo_is_engine_busy(g)) {
		mutex_unlock(&g->poweroff_lock);
		return -EBUSY;
	}
	gk20a_scale_suspend(dev);

	/* cancel any pending cde work */
	gk20a_cde_suspend(g);

	if (platform->has_ce)
		gk20a_ce_suspend(g);

	ret = gk20a_channel_suspend(g);
	if (ret)
		goto done;

	/* disable elpg before gr or fifo suspend */
	ret |= gk20a_pmu_destroy(g);
	/*
	 * After this point, gk20a interrupts should not get
	 * serviced.
	 */
	disable_irq(g->irq_stall);
	if (g->irq_stall != g->irq_nonstall)
		disable_irq(g->irq_nonstall);

	ret |= gk20a_gr_suspend(g);
	ret |= gk20a_mm_suspend(g);
	ret |= gk20a_fifo_suspend(g);

	/* Disable GPCPLL */
	if (g->ops.clk.suspend_clk_support)
		ret |= g->ops.clk.suspend_clk_support(g);

	g->power_on = false;

	/* Decrement platform power refcount */
	if (platform->idle)
		platform->idle(dev);

	/* Stop CPU from accessing the GPU registers. */
	gk20a_lockout_registers(g);

done:
	mutex_unlock(&g->poweroff_lock);

	return ret;
}

static int gk20a_detect_chip(struct gk20a *g)
{
	struct nvgpu_gpu_characteristics *gpu = &g->gpu_characteristics;
	u32 mc_boot_0_value;

	if (gpu->arch)
		return 0;

	mc_boot_0_value = gk20a_readl(g, mc_boot_0_r());
	gpu->arch = mc_boot_0_architecture_v(mc_boot_0_value) <<
		NVGPU_GPU_ARCHITECTURE_SHIFT;
	gpu->impl = mc_boot_0_implementation_v(mc_boot_0_value);
	gpu->rev =
		(mc_boot_0_major_revision_v(mc_boot_0_value) << 4) |
		mc_boot_0_minor_revision_v(mc_boot_0_value);

	gk20a_dbg_info("arch: %x, impl: %x, rev: %x\n",
			g->gpu_characteristics.arch,
			g->gpu_characteristics.impl,
			g->gpu_characteristics.rev);

	return gpu_init_hal(g);
}

int gk20a_pm_finalize_poweron(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int err, nice_value;

	gk20a_dbg_fn("");

	if (g->power_on)
		return 0;

	trace_gk20a_finalize_poweron(dev_name(dev));

	/* Increment platform power refcount */
	if (platform->busy) {
		err = platform->busy(dev);
		if (err < 0) {
			dev_err(dev, "%s: failed to poweron platform dependency\n",
				__func__);
			goto done;
		}
	}

	err = gk20a_restore_registers(g);
	if (err)
		return err;

	nice_value = task_nice(current);
	set_user_nice(current, -20);

	g->power_on = true;

	err = gk20a_detect_chip(g);
	if (err)
		goto done;

	/*
	 * Before probing the GPU make sure the GPU's state is cleared. This is
	 * relevant for rebind operations.
	 */
	if (g->ops.xve.reset_gpu && !g->gpu_reset_done) {
		pr_info("%s: Resetting GPU!!!\n", __func__);
		g->ops.xve.reset_gpu(g);
		g->gpu_reset_done = true;
	}

	if (g->ops.bios.init)
		err = g->ops.bios.init(g);
	if (err)
		goto done;

	if (!tegra_platform_is_silicon())
		gk20a_writel(g, bus_intr_en_0_r(), 0x0);
	else
		gk20a_writel(g, bus_intr_en_0_r(),
			        bus_intr_en_0_pri_squash_m() |
			        bus_intr_en_0_pri_fecserr_m() |
			        bus_intr_en_0_pri_timeout_m());

	if (g->ops.clk.disable_slowboot)
		g->ops.clk.disable_slowboot(g);

	gk20a_reset_priv_ring(g);

	/* TBD: move this after graphics init in which blcg/slcg is enabled.
	   This function removes SlowdownOnBoot which applies 32x divider
	   on gpcpll bypass path. The purpose of slowdown is to save power
	   during boot but it also significantly slows down gk20a init on
	   simulation and emulation. We should remove SOB after graphics power
	   saving features (blcg/slcg) are enabled. For now, do it here. */
	if (g->ops.clk.init_clk_support) {
		err = g->ops.clk.init_clk_support(g);
		if (err) {
			gk20a_err(dev, "failed to init gk20a clk");
			goto done;
		}
	}

	/* enable pri timeout only on silicon */
	if (tegra_platform_is_silicon()) {
		gk20a_writel(g,
			timer_pri_timeout_r(),
			timer_pri_timeout_period_f(
				platform->default_pri_timeout ?
					platform->default_pri_timeout :
								0x186A0) |
			timer_pri_timeout_en_en_enabled_f());
	} else {
		gk20a_writel(g,
			timer_pri_timeout_r(),
			timer_pri_timeout_period_f(0x186A0) |
			timer_pri_timeout_en_en_disabled_f());
	}

	err = gk20a_init_fifo_reset_enable_hw(g);
	if (err) {
		gk20a_err(dev, "failed to reset gk20a fifo");
		goto done;
	}

	if (g->ops.ltc.init_fs_state)
		g->ops.ltc.init_fs_state(g);

	err = gk20a_init_mm_support(g);
	if (err) {
		gk20a_err(dev, "failed to init gk20a mm");
		goto done;
	}

	err = gk20a_init_fifo_support(g);
	if (err) {
		gk20a_err(dev, "failed to init gk20a fifo");
		goto done;
	}

	if (g->ops.therm.elcg_init_idle_filters)
		g->ops.therm.elcg_init_idle_filters(g);

	g->ops.mc.intr_enable(g);

	err = gk20a_enable_gr_hw(g);
	if (err) {
		gk20a_err(dev, "failed to enable gr");
		goto done;
	}

	if (g->ops.pmu.prepare_ucode)
		err = g->ops.pmu.prepare_ucode(g);
	if (err) {
		gk20a_err(dev, "failed to init pmu ucode");
		goto done;
	}

	if (g->ops.pmupstate)
		err = gk20a_init_pstate_support(g);

	err = gk20a_init_pmu_support(g);
	if (err) {
		gk20a_err(dev, "failed to init gk20a pmu");
		goto done;
	}

	err = gk20a_init_gr_support(g);
	if (err) {
		gk20a_err(dev, "failed to init gk20a gr");
		goto done;
	}

	if (g->ops.pmu.mclk_init) {
		err = g->ops.pmu.mclk_init(g);
		if (err) {
			gk20a_err(dev, "failed to set mclk");
			/* Indicate error dont goto done */
		}
	}

	if (g->ops.pmupstate)
		err = gk20a_init_pstate_pmu_support(g);

	err = gk20a_init_therm_support(g);
	if (err) {
		gk20a_err(dev, "failed to init gk20a therm");
		goto done;
	}

	err = nvgpu_clk_arb_init_arbiter(g);
	if (err) {
		gk20a_err(dev, "failed to init clk arb");
		goto done;
	}

	err = g->ops.chip_init_gpu_characteristics(g);
	if (err) {
		gk20a_err(dev, "failed to init gk20a gpu characteristics");
		goto done;
	}

	err = gk20a_ctxsw_trace_init(g);
	if (err)
		gk20a_warn(dev, "could not initialize ctxsw tracing");

	err = gk20a_sched_ctrl_init(g);
	if (err) {
		gk20a_err(dev, "failed to init sched control");
		goto done;
	}

	/* Restore the debug setting */
	g->ops.mm.set_debug_mode(g, g->mmu_debug_ctrl);

	gk20a_channel_resume(g);
	set_user_nice(current, nice_value);

	gk20a_scale_resume(dev);

	trace_gk20a_finalize_poweron_done(dev_name(dev));

	if (platform->has_cde)
		gk20a_init_cde_support(g);

	if (platform->has_ce)
		gk20a_init_ce_support(g);

	gk20a_init_mm_ce_context(g);

	enable_irq(g->irq_stall);
	if (g->irq_stall != g->irq_nonstall)
		enable_irq(g->irq_nonstall);

	if (g->ops.xve.available_speeds) {
		u32 speed;

		g->ops.xve.sw_init(dev);
		g->ops.xve.hw_init(dev);

		g->ops.xve.available_speeds(g, &speed);

		/* Set to max speed */
		speed = 1 << (fls(speed) - 1);
		err = g->ops.xve.set_speed(g, speed);
		if (err) {
			gk20a_err(dev, "Failed to set PCIe bus speed!\n");
			goto done;
		}
	}

done:
	if (err)
		g->power_on = false;

	return err;
}

static struct of_device_id tegra_gk20a_of_match[] = {
#ifdef CONFIG_TEGRA_GK20A
	{ .compatible = "nvidia,tegra124-gk20a",
		.data = &gk20a_tegra_platform },
	{ .compatible = "nvidia,tegra210-gm20b",
		.data = &gm20b_tegra_platform },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = TEGRA_18x_GPU_COMPAT_TEGRA,
		.data = &t18x_gpu_tegra_platform },
#endif
#ifdef CONFIG_TEGRA_19x_GPU
	{ .compatible = TEGRA_19x_GPU_COMPAT_TEGRA,
		.data = &t19x_gpu_tegra_platform },
#endif
#ifdef CONFIG_TEGRA_GR_VIRTUALIZATION
	{ .compatible = "nvidia,tegra124-gk20a-vgpu",
		.data = &vgpu_tegra_platform },
#endif
#else
	{ .compatible = "nvidia,tegra124-gk20a",
		.data = &gk20a_generic_platform },
	{ .compatible = "nvidia,tegra210-gm20b",
		.data = &gk20a_generic_platform },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = TEGRA_18x_GPU_COMPAT_TEGRA,
		.data = &gk20a_generic_platform },
#endif

#endif
	{ .compatible = "nvidia,generic-gk20a",
		.data = &gk20a_generic_platform },
	{ .compatible = "nvidia,generic-gm20b",
		.data = &gk20a_generic_platform },
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = TEGRA_18x_GPU_COMPAT_GENERIC,
		.data = &gk20a_generic_platform },
#endif
	{ },
};

static int gk20a_create_device(
	struct device *dev, int devno,
	const char *interface_name, const char *cdev_name,
	struct cdev *cdev, struct device **out,
	const struct file_operations *ops,
	struct class *class)
{
	struct device *subdev;
	int err;

	gk20a_dbg_fn("");

	cdev_init(cdev, ops);
	cdev->owner = THIS_MODULE;

	err = cdev_add(cdev, devno, 1);
	if (err) {
		dev_err(dev, "failed to add %s cdev\n", cdev_name);
		return err;
	}

	subdev = device_create(class, NULL, devno, NULL,
		interface_name, cdev_name);

	if (IS_ERR(subdev)) {
		err = PTR_ERR(dev);
		cdev_del(cdev);
		dev_err(dev, "failed to create %s device for %s\n",
			cdev_name, dev_name(dev));
		return err;
	}

	*out = subdev;
	return 0;
}

void gk20a_user_deinit(struct device *dev, struct class *class)
{
	struct gk20a *g = gk20a_from_dev(dev);

	if (g->channel.node) {
		device_destroy(class, g->channel.cdev.dev);
		cdev_del(&g->channel.cdev);
	}

	if (g->as.node) {
		device_destroy(class, g->as.cdev.dev);
		cdev_del(&g->as.cdev);
	}

	if (g->ctrl.node) {
		device_destroy(class, g->ctrl.cdev.dev);
		cdev_del(&g->ctrl.cdev);
	}

	if (g->dbg.node) {
		device_destroy(class, g->dbg.cdev.dev);
		cdev_del(&g->dbg.cdev);
	}

	if (g->prof.node) {
		device_destroy(class, g->prof.cdev.dev);
		cdev_del(&g->prof.cdev);
	}

	if (g->tsg.node) {
		device_destroy(class, g->tsg.cdev.dev);
		cdev_del(&g->tsg.cdev);
	}

	if (g->ctxsw.node) {
		device_destroy(class, g->ctxsw.cdev.dev);
		cdev_del(&g->ctxsw.cdev);
	}

	if (g->sched.node) {
		device_destroy(class, g->sched.cdev.dev);
		cdev_del(&g->sched.cdev);
	}

	if (g->cdev_region)
		unregister_chrdev_region(g->cdev_region, GK20A_NUM_CDEVS);
}

int gk20a_user_init(struct device *dev, const char *interface_name,
		    struct class *class)
{
	int err;
	dev_t devno;
	struct gk20a *g = gk20a_from_dev(dev);

	err = alloc_chrdev_region(&devno, 0, GK20A_NUM_CDEVS, dev_name(dev));
	if (err) {
		dev_err(dev, "failed to allocate devno\n");
		goto fail;
	}
	g->cdev_region = devno;

	err = gk20a_create_device(dev, devno++, interface_name, "",
				  &g->channel.cdev, &g->channel.node,
				  &gk20a_channel_ops,
				  class);
	if (err)
		goto fail;

	err = gk20a_create_device(dev, devno++, interface_name, "-as",
				  &g->as.cdev, &g->as.node,
				  &gk20a_as_ops,
				  class);
	if (err)
		goto fail;

	err = gk20a_create_device(dev, devno++, interface_name, "-ctrl",
				  &g->ctrl.cdev, &g->ctrl.node,
				  &gk20a_ctrl_ops,
				  class);
	if (err)
		goto fail;

	err = gk20a_create_device(dev, devno++, interface_name, "-dbg",
				  &g->dbg.cdev, &g->dbg.node,
				  &gk20a_dbg_ops,
				  class);
	if (err)
		goto fail;

	err = gk20a_create_device(dev, devno++, interface_name, "-prof",
				  &g->prof.cdev, &g->prof.node,
				  &gk20a_prof_ops,
				  class);
	if (err)
		goto fail;

	err = gk20a_create_device(dev, devno++, interface_name, "-tsg",
				  &g->tsg.cdev, &g->tsg.node,
				  &gk20a_tsg_ops,
				  class);
	if (err)
		goto fail;

#ifdef CONFIG_GK20A_CTXSW_TRACE
	err = gk20a_create_device(dev, devno++, interface_name, "-ctxsw",
				  &g->ctxsw.cdev, &g->ctxsw.node,
				  &gk20a_ctxsw_ops,
				  class);
	if (err)
		goto fail;
#endif

	err = gk20a_create_device(dev, devno++, interface_name, "-sched",
				  &g->sched.cdev, &g->sched.node,
				  &gk20a_sched_ops,
				  class);
	if (err)
		goto fail;

	return 0;
fail:
	gk20a_user_deinit(dev, &nvgpu_class);
	return err;
}

static int gk20a_pm_railgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;
#ifdef CONFIG_DEBUG_FS
	struct gk20a *g = get_gk20a(dev);

	g->pstats.last_rail_gate_start = jiffies;

	if (g->pstats.railgating_cycle_count >= 1)
		g->pstats.total_rail_ungate_time_ms =
			g->pstats.total_rail_ungate_time_ms +
			jiffies_to_msecs(g->pstats.last_rail_gate_start -
					g->pstats.last_rail_ungate_complete);
#endif

	if (platform->railgate)
		ret = platform->railgate(dev);

#ifdef CONFIG_DEBUG_FS
	g->pstats.last_rail_gate_complete = jiffies;
#endif

	return ret;
}

static int gk20a_pm_unrailgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;
#ifdef CONFIG_DEBUG_FS
	struct gk20a *g = get_gk20a(dev);

	g->pstats.last_rail_ungate_start = jiffies;
	if (g->pstats.railgating_cycle_count >= 1)
		g->pstats.total_rail_gate_time_ms =
			g->pstats.total_rail_gate_time_ms +
			jiffies_to_msecs(g->pstats.last_rail_ungate_start -
				g->pstats.last_rail_gate_complete);

	g->pstats.railgating_cycle_count++;
#endif

	trace_gk20a_pm_unrailgate(dev_name(dev));

	if (platform->unrailgate) {
		mutex_lock(&platform->railgate_lock);
		ret = platform->unrailgate(dev);
		mutex_unlock(&platform->railgate_lock);
	}

#ifdef CONFIG_DEBUG_FS
	g->pstats.last_rail_ungate_complete = jiffies;
#endif

	return ret;
}

static void gk20a_pm_shutdown(struct platform_device *pdev)
{
	struct gk20a_platform *platform = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "shutting down");

	/* If GPU is already railgated,
	 * just prevent more requests, and return */
	if (platform->is_railgated && platform->is_railgated(&pdev->dev)) {
		__pm_runtime_disable(&pdev->dev, false);
		return;
	}

	/* Prevent more requests by disabling Runtime PM */
	__pm_runtime_disable(&pdev->dev, false);

	/* Be ready for rail-gate after this point */
	if (gk20a_gpu_is_virtual(&pdev->dev))
		vgpu_pm_prepare_poweroff(&pdev->dev);
	else
		gk20a_pm_prepare_poweroff(&pdev->dev);

	gk20a_pm_railgate(&pdev->dev);

	dev_info(&pdev->dev, "shut down complete\n");
}

static int gk20a_pm_runtime_resume(struct device *dev)
{
	int err = 0;

	err = gk20a_pm_unrailgate(dev);
	if (err)
		goto fail;

	err = gk20a_pm_finalize_poweron(dev);
	if (err)
		goto fail_poweron;

	return 0;

fail_poweron:
	gk20a_pm_railgate(dev);
fail:
	return err;
}

static int gk20a_pm_runtime_suspend(struct device *dev)
{
	int err = 0;

	err = gk20a_pm_prepare_poweroff(dev);
	if (err)
		goto fail;

	err = gk20a_pm_railgate(dev);
	if (err)
		goto fail_railgate;

	return 0;

fail_railgate:
	gk20a_pm_finalize_poweron(dev);
fail:
	pm_runtime_mark_last_busy(dev);
	return err;
}

static int gk20a_pm_suspend(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = get_gk20a(dev);
	int ret = 0;

	if (platform->user_railgate_disabled)
		gk20a_idle_nosuspend(dev);

#ifdef CONFIG_PM
	if (atomic_read(&dev->power.usage_count) > 1) {
		ret = -EBUSY;
		goto fail;
	}
#endif

	if (!g->power_on)
		return 0;

	ret = gk20a_pm_runtime_suspend(dev);
	if (ret)
		goto fail;

	if (platform->suspend)
		platform->suspend(dev);

	g->suspended = true;

	return 0;

fail:
	if (platform->user_railgate_disabled)
		gk20a_busy_noresume(dev);

	return ret;
}

static int gk20a_pm_resume(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;

	if (platform->user_railgate_disabled)
		gk20a_busy_noresume(dev);

	if (!g->suspended)
		return 0;

	ret = gk20a_pm_runtime_resume(dev);

	g->suspended = false;

	return ret;
}

#ifdef CONFIG_PM
static const struct dev_pm_ops gk20a_pm_ops = {
	.runtime_resume = gk20a_pm_runtime_resume,
	.runtime_suspend = gk20a_pm_runtime_suspend,
	.resume = gk20a_pm_resume,
	.suspend = gk20a_pm_suspend,
};
#endif

int gk20a_pm_init(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int err = 0;

	gk20a_dbg_fn("");

	/* Initialise pm runtime */
	if (platform->railgate_delay) {
		pm_runtime_set_autosuspend_delay(dev,
				 platform->railgate_delay);
		pm_runtime_use_autosuspend(dev);
	}

	if (platform->can_railgate) {
		pm_runtime_enable(dev);
		if (!pm_runtime_enabled(dev))
			gk20a_pm_unrailgate(dev);
		else
			gk20a_pm_railgate(dev);
	} else {
		__pm_runtime_disable(dev, false);
		gk20a_pm_unrailgate(dev);
	}

	return err;
}

int gk20a_secure_page_alloc(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int err = 0;

	if (platform->secure_page_alloc) {
		err = platform->secure_page_alloc(dev);
		if (!err)
			platform->secure_alloc_ready = true;
	}

	return err;
}

static int gk20a_probe(struct platform_device *dev)
{
	struct gk20a *gk20a;
	int err;
	struct gk20a_platform *platform = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_gk20a_of_match, &dev->dev);
		if (match)
			platform = (struct gk20a_platform *)match->data;
	} else
		platform = (struct gk20a_platform *)dev->dev.platform_data;

	if (!platform) {
		dev_err(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	gk20a_dbg_fn("");

	platform_set_drvdata(dev, platform);

	if (gk20a_gpu_is_virtual(&dev->dev))
		return vgpu_probe(dev);

	gk20a = kzalloc(sizeof(struct gk20a), GFP_KERNEL);
	if (!gk20a) {
		dev_err(&dev->dev, "couldn't allocate gk20a support");
		return -ENOMEM;
	}

	set_gk20a(dev, gk20a);
	gk20a->dev = &dev->dev;

	gk20a->irq_stall = platform_get_irq(dev, 0);
	gk20a->irq_nonstall = platform_get_irq(dev, 1);
	if (gk20a->irq_stall < 0 || gk20a->irq_nonstall < 0)
		return -ENXIO;

	err = devm_request_threaded_irq(&dev->dev,
			gk20a->irq_stall,
			gk20a_intr_isr_stall,
			gk20a_intr_thread_stall,
			0, "gk20a_stall", gk20a);
	if (err) {
		dev_err(&dev->dev,
			"failed to request stall intr irq @ %d\n",
				gk20a->irq_stall);
		return err;
	}
	err = devm_request_threaded_irq(&dev->dev,
			gk20a->irq_nonstall,
			gk20a_intr_isr_nonstall,
			gk20a_intr_thread_nonstall,
			0, "gk20a_nonstall", gk20a);
	if (err) {
		dev_err(&dev->dev,
			"failed to request non-stall intr irq @ %d\n",
				gk20a->irq_nonstall);
		return err;
	}
	disable_irq(gk20a->irq_stall);
	if (gk20a->irq_stall != gk20a->irq_nonstall)
		disable_irq(gk20a->irq_nonstall);

	err = gk20a_init_support(dev);
	if (err)
		return err;

#ifdef CONFIG_RESET_CONTROLLER
	platform->reset_control = devm_reset_control_get(&dev->dev, NULL);
	if (IS_ERR(platform->reset_control))
		platform->reset_control = NULL;
#endif

	err = nvgpu_probe(gk20a, "gpu.0", INTERFACE_NAME, &nvgpu_class);
	if (err)
		return err;

	err = gk20a_pm_init(&dev->dev);
	if (err) {
		dev_err(&dev->dev, "pm init failed");
		return err;
	}

	gk20a->mm.has_physical_mode = !is_tegra_hypervisor_mode();

	return 0;
}

static int __exit gk20a_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	gk20a_dbg_fn("");

	if (gk20a_gpu_is_virtual(dev))
		return vgpu_remove(pdev);

	if (platform->has_cde)
		gk20a_cde_destroy(g);

	gk20a_ctxsw_trace_cleanup(g);

	gk20a_sched_ctrl_cleanup(g);

	if (IS_ENABLED(CONFIG_GK20A_DEVFREQ))
		gk20a_scale_exit(dev);

	if (g->remove_support)
		g->remove_support(dev);

	if (platform->has_ce)
		gk20a_ce_destroy(g);

	nvgpu_clk_arb_cleanup_arbiter(g);

	gk20a_user_deinit(dev, &nvgpu_class);

	debugfs_remove_recursive(platform->debugfs);
	debugfs_remove_recursive(platform->debugfs_alias);

	gk20a_remove_sysfs(dev);

	if (platform->secure_buffer.destroy)
		platform->secure_buffer.destroy(dev,
				&platform->secure_buffer);

	if (pm_runtime_enabled(dev))
		pm_runtime_disable(dev);

	if (platform->remove)
		platform->remove(dev);

	set_gk20a(pdev, NULL);
	kfree(g);

	gk20a_dbg_fn("removed");

	return 0;
}

static struct platform_driver gk20a_driver = {
	.probe = gk20a_probe,
	.remove = __exit_p(gk20a_remove),
	.shutdown = gk20a_pm_shutdown,
	.driver = {
		.owner = THIS_MODULE,
		.name = "gk20a",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#endif
#ifdef CONFIG_OF
		.of_match_table = tegra_gk20a_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &gk20a_pm_ops,
#endif
	}
};

struct class nvgpu_class = {
	.owner = THIS_MODULE,
	.name = CLASS_NAME,
};

static int __init gk20a_init(void)
{

	int ret;

	ret = class_register(&nvgpu_class);
	if (ret)
		return ret;

	ret = nvgpu_pci_init();
	if (ret)
		return ret;

	return platform_driver_register(&gk20a_driver);
}

static void __exit gk20a_exit(void)
{
	nvgpu_pci_exit();
	platform_driver_unregister(&gk20a_driver);
	class_unregister(&nvgpu_class);
}

void gk20a_busy_noresume(struct device *dev)
{
	pm_runtime_get_noresume(dev);
}

/*
 * Start the process for unloading the driver. Set g->driver_is_dying.
 */
void gk20a_driver_start_unload(struct gk20a *g)
{
	gk20a_dbg(gpu_dbg_shutdown, "Driver is now going down!\n");
	g->driver_is_dying = 1;
}

int gk20a_wait_for_idle(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	int wait_length = 150; /* 3 second overall max wait. */

	if (!g)
		return -ENODEV;

	while (atomic_read(&g->usage_count) && wait_length-- >= 0)
		msleep(20);

	if (wait_length < 0) {
		pr_warn("%s: Timed out waiting for idle (%d)!\n",
			__func__, atomic_read(&g->usage_count));
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * Check if the device can go busy. Basically if the driver is currently
 * in the process of dying then do not let new places make the driver busy.
 */
static int gk20a_can_busy(struct gk20a *g)
{
	if (g->driver_is_dying)
		return 0;
	return 1;
}

int gk20a_busy(struct device *dev)
{
	int ret = 0;
	struct gk20a *g;
	struct gk20a_platform *platform;

	if (!dev)
		return -ENODEV;

	g = get_gk20a(dev);
	platform = gk20a_get_platform(dev);

	if (!g || !gk20a_can_busy(g))
		return -ENODEV;

	atomic_inc(&g->usage_count);

	down_read(&g->busy_lock);
	if (pm_runtime_enabled(dev)) {
		ret = pm_runtime_get_sync(dev);
		if (ret < 0) {
			pm_runtime_put_noidle(dev);
			atomic_dec(&g->usage_count);
			goto fail;
		}
	} else {
		if (!g->power_on) {
			ret = gk20a_gpu_is_virtual(dev) ?
				vgpu_pm_finalize_poweron(dev)
				: gk20a_pm_finalize_poweron(dev);
			if (ret) {
				atomic_dec(&g->usage_count);
				goto fail;
			}
		}
	}

	if (!platform->disable_load_scaling)
		gk20a_scale_notify_busy(dev);

fail:
	up_read(&g->busy_lock);

	return ret < 0 ? ret : 0;
}

void gk20a_idle_nosuspend(struct device *dev)
{
	pm_runtime_put_noidle(dev);
}

void gk20a_idle(struct device *dev)
{
	struct gk20a_platform *platform;
	struct gk20a *g;

	if (!dev)
		return;

	g = get_gk20a(dev);
	platform = gk20a_get_platform(dev);

	atomic_dec(&g->usage_count);

	if (pm_runtime_enabled(dev)) {
#ifdef CONFIG_PM
		if (atomic_read(&dev->power.usage_count) == 1 &&
				!platform->disable_load_scaling)
			gk20a_scale_notify_idle(dev);
#endif

		pm_runtime_mark_last_busy(dev);
		pm_runtime_put_sync_autosuspend(dev);

	} else {
		if (!platform->disable_load_scaling)
			gk20a_scale_notify_idle(dev);
	}
}

void gk20a_disable(struct gk20a *g, u32 units)
{
	u32 pmc;

	gk20a_dbg(gpu_dbg_info, "pmc disable: %08x\n", units);

	spin_lock(&g->mc_enable_lock);
	pmc = gk20a_readl(g, mc_enable_r());
	pmc &= ~units;
	gk20a_writel(g, mc_enable_r(), pmc);
	spin_unlock(&g->mc_enable_lock);
}

void gk20a_enable(struct gk20a *g, u32 units)
{
	u32 pmc;

	gk20a_dbg(gpu_dbg_info, "pmc enable: %08x\n", units);

	spin_lock(&g->mc_enable_lock);
	pmc = gk20a_readl(g, mc_enable_r());
	pmc |= units;
	gk20a_writel(g, mc_enable_r(), pmc);
	gk20a_readl(g, mc_enable_r());
	spin_unlock(&g->mc_enable_lock);

	udelay(20);
}

void gk20a_reset(struct gk20a *g, u32 units)
{
	gk20a_disable(g, units);
	if (units & gk20a_fifo_get_all_ce_engine_reset_mask(g))
		udelay(500);
	else
		udelay(20);
	gk20a_enable(g, units);
}

#ifdef CONFIG_PM
/**
 * __gk20a_do_idle() - force the GPU to idle and railgate
 *
 * In success, this call MUST be balanced by caller with __gk20a_do_unidle()
 *
 * Acquires two locks : &g->busy_lock and &platform->railgate_lock
 * In success, we hold these locks and return
 * In failure, we release these locks and return
 */
int __gk20a_do_idle(struct device *dev, bool force_reset)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	unsigned long timeout = jiffies +
		msecs_to_jiffies(GK20A_WAIT_FOR_IDLE_MS);
	int ref_cnt;
	int target_ref_cnt = 0;
	bool is_railgated;
	int err = 0;

	/* acquire busy lock to block other busy() calls */
	down_write(&g->busy_lock);

	/* acquire railgate lock to prevent unrailgate in midst of do_idle() */
	mutex_lock(&platform->railgate_lock);

	/* check if it is already railgated ? */
	if (platform->is_railgated(dev))
		return 0;

	/*
	 * release railgate_lock, prevent suspend by incrementing usage counter,
	 * re-acquire railgate_lock
	 */
	mutex_unlock(&platform->railgate_lock);
	pm_runtime_get_sync(dev);

	/*
	 * One refcount taken in this API
	 * If User disables rail gating, we take one more
	 * extra refcount
	 */
	if (platform->user_railgate_disabled)
		target_ref_cnt = 2;
	else
		target_ref_cnt = 1;
	mutex_lock(&platform->railgate_lock);

	/* check and wait until GPU is idle (with a timeout) */
	do {
		msleep(1);
		ref_cnt = atomic_read(&dev->power.usage_count);
	} while (ref_cnt != target_ref_cnt && time_before(jiffies, timeout));

	if (ref_cnt != target_ref_cnt) {
		gk20a_err(dev, "failed to idle - refcount %d != 1\n",
			ref_cnt);
		goto fail_drop_usage_count;
	}

	/* check if global force_reset flag is set */
	force_reset |= platform->force_reset_in_do_idle;

	if (platform->can_railgate && !force_reset) {
		/*
		 * Case 1 : GPU railgate is supported
		 *
		 * if GPU is now idle, we will have only one ref count,
		 * drop this ref which will rail gate the GPU
		 */
		pm_runtime_put_sync(dev);

		/* add sufficient delay to allow GPU to rail gate */
		msleep(platform->railgate_delay);

		timeout = jiffies + msecs_to_jiffies(GK20A_WAIT_FOR_IDLE_MS);

		/* check in loop if GPU is railgated or not */
		do {
			msleep(1);
			is_railgated = platform->is_railgated(dev);
		} while (!is_railgated && time_before(jiffies, timeout));

		if (is_railgated) {
			return 0;
		} else {
			gk20a_err(dev, "failed to idle in timeout\n");
			goto fail_timeout;
		}
	} else {
		/*
		 * Case 2 : GPU railgate is not supported or we explicitly
		 * do not want to depend on runtime PM
		 *
		 * if GPU is now idle, call prepare_poweroff() to save the
		 * state and then do explicit railgate
		 *
		 * __gk20a_do_unidle() needs to unrailgate, call
		 * finalize_poweron(), and then call pm_runtime_put_sync()
		 * to balance the GPU usage counter
		 */

		/* Save the GPU state */
		err = gk20a_pm_prepare_poweroff(dev);
		if (err)
			goto fail_drop_usage_count;

		/* railgate GPU */
		platform->railgate(dev);

		udelay(10);

		g->forced_reset = true;
		return 0;
	}

fail_drop_usage_count:
	pm_runtime_put_noidle(dev);
fail_timeout:
	mutex_unlock(&platform->railgate_lock);
	up_write(&g->busy_lock);
	return -EBUSY;
}

/**
 * gk20a_do_idle() - wrap up for __gk20a_do_idle() to be called
 * from outside of GPU driver
 *
 * In success, this call MUST be balanced by caller with gk20a_do_unidle()
 */
int gk20a_do_idle(void)
{
	struct device_node *node =
			of_find_matching_node(NULL, tegra_gk20a_of_match);
	struct platform_device *pdev = of_find_device_by_node(node);

	int ret =  __gk20a_do_idle(&pdev->dev, true);

	of_node_put(node);

	return ret;
}

/**
 * __gk20a_do_unidle() - unblock all the tasks blocked by __gk20a_do_idle()
 */
int __gk20a_do_unidle(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = dev_get_drvdata(dev);

	if (g->forced_reset) {
		/*
		 * If we did a forced-reset/railgate
		 * then unrailgate the GPU here first
		 */
		platform->unrailgate(dev);

		/* restore the GPU state */
		gk20a_pm_finalize_poweron(dev);

		/* balance GPU usage counter */
		pm_runtime_put_sync(dev);

		g->forced_reset = false;
	}

	/* release the lock and open up all other busy() calls */
	mutex_unlock(&platform->railgate_lock);
	up_write(&g->busy_lock);

	return 0;
}

/**
 * gk20a_do_unidle() - wrap up for __gk20a_do_unidle()
 */
int gk20a_do_unidle(void)
{
	struct device_node *node =
			of_find_matching_node(NULL, tegra_gk20a_of_match);
	struct platform_device *pdev = of_find_device_by_node(node);

	int ret = __gk20a_do_unidle(&pdev->dev);

	of_node_put(node);

	return ret;
}
#endif

int gk20a_init_gpu_characteristics(struct gk20a *g)
{
	struct nvgpu_gpu_characteristics *gpu = &g->gpu_characteristics;
	struct gk20a_platform *platform = dev_get_drvdata(g->dev);

	gpu->L2_cache_size = g->ops.ltc.determine_L2_size_bytes(g);
	gpu->on_board_video_memory_size = 0; /* integrated GPU */

	gpu->num_gpc = g->gr.gpc_count;
	gpu->max_gpc_count = g->gr.max_gpc_count;

	gpu->num_tpc_per_gpc = g->gr.max_tpc_per_gpc_count;

	gpu->bus_type = NVGPU_GPU_BUS_TYPE_AXI; /* always AXI for now */

	gpu->big_page_size = g->mm.pmu.vm.big_page_size;
	gpu->compression_page_size = g->ops.fb.compression_page_size(g);
	gpu->pde_coverage_bit_count =
		gk20a_mm_pde_coverage_bit_count(&g->mm.pmu.vm);

	if (g->mm.disable_bigpage) {
		gpu->big_page_size = 0;
		gpu->available_big_page_sizes = 0;
	} else {
		gpu->available_big_page_sizes = gpu->big_page_size;
		if (g->ops.mm.get_big_page_sizes)
			gpu->available_big_page_sizes |= g->ops.mm.get_big_page_sizes();
	}

	gpu->flags = NVGPU_GPU_FLAGS_SUPPORT_PARTIAL_MAPPINGS
		| NVGPU_GPU_FLAGS_SUPPORT_SYNC_FENCE_FDS;

	if (g->ops.mm.support_sparse && g->ops.mm.support_sparse(g))
		gpu->flags |= NVGPU_GPU_FLAGS_SUPPORT_SPARSE_ALLOCS;

	if (IS_ENABLED(CONFIG_TEGRA_GK20A) &&
	    gk20a_platform_has_syncpoints(g->dev))
		gpu->flags |= NVGPU_GPU_FLAGS_HAS_SYNCPOINTS;

	gpu->flags |= NVGPU_GPU_FLAGS_SUPPORT_USERSPACE_MANAGED_AS;
	gpu->flags |= NVGPU_GPU_FLAGS_SUPPORT_TSG;

	if (g->ops.clk_arb.get_arbiter_clk_domains)
		gpu->flags |= NVGPU_GPU_FLAGS_SUPPORT_CLOCK_CONTROLS;

	gpu->gpc_mask = (1 << g->gr.gpc_count)-1;

	g->ops.gr.detect_sm_arch(g);

	if (g->ops.gr.init_cyclestats)
		g->ops.gr.init_cyclestats(g);

	gpu->gpu_ioctl_nr_last = NVGPU_GPU_IOCTL_LAST;
	gpu->tsg_ioctl_nr_last = NVGPU_TSG_IOCTL_LAST;
	gpu->dbg_gpu_ioctl_nr_last = NVGPU_DBG_GPU_IOCTL_LAST;
	gpu->ioctl_channel_nr_last = NVGPU_IOCTL_CHANNEL_LAST;
	gpu->as_ioctl_nr_last = NVGPU_AS_IOCTL_LAST;
	gpu->event_ioctl_nr_last = NVGPU_EVENT_IOCTL_LAST;
	gpu->gpu_va_bit_count = 40;

	strlcpy(gpu->chipname, g->ops.name, sizeof(gpu->chipname));
	gpu->max_fbps_count = g->ops.gr.get_max_fbps_count(g);
	gpu->fbp_en_mask = g->ops.gr.get_fbp_en_mask(g);
	gpu->max_ltc_per_fbp =  g->ops.gr.get_max_ltc_per_fbp(g);
	gpu->max_lts_per_ltc = g->ops.gr.get_max_lts_per_ltc(g);
	g->ops.gr.get_rop_l2_en_mask(g);
	gpu->gr_compbit_store_base_hw = g->gr.compbit_store.base_hw;
	gpu->gr_gobs_per_comptagline_per_slice =
		g->gr.gobs_per_comptagline_per_slice;
	gpu->num_ltc = g->ltc_count;
	gpu->lts_per_ltc = g->gr.slices_per_ltc;
	gpu->cbc_cache_line_size = g->gr.cacheline_size;
	gpu->cbc_comptags_per_line = g->gr.comptags_per_cacheline;

	gpu->map_buffer_batch_limit = 256;

	if (platform->clk_round_rate)
		gpu->max_freq = platform->clk_round_rate(g->dev, UINT_MAX);

	g->ops.gr.get_preemption_mode_flags(g, &g->gr.preemption_mode_rec);
	gpu->graphics_preemption_mode_flags =
		g->gr.preemption_mode_rec.graphics_preemption_mode_flags;
	gpu->compute_preemption_mode_flags =
		g->gr.preemption_mode_rec.compute_preemption_mode_flags;
	gpu->default_graphics_preempt_mode =
		g->gr.preemption_mode_rec.default_graphics_preempt_mode;
	gpu->default_compute_preempt_mode =
		g->gr.preemption_mode_rec.default_compute_preempt_mode;

	gpu->local_video_memory_size = g->mm.vidmem.size;

	gpu->pci_vendor_id = g->pci_vendor_id;
	gpu->pci_device_id = g->pci_device_id;
	gpu->pci_subsystem_vendor_id = g->pci_subsystem_vendor_id;
	gpu->pci_subsystem_device_id = g->pci_subsystem_device_id;
	gpu->pci_class = g->pci_class;
	gpu->pci_revision = g->pci_revision;

	gpu->reg_ops_limit = 1024;

	return 0;
}

int gk20a_read_ptimer(struct gk20a *g, u64 *value)
{
	const unsigned int max_iterations = 3;
	unsigned int i = 0;
	u32 gpu_timestamp_hi_prev = 0;

	if (!value)
		return -EINVAL;

	/* Note. The GPU nanosecond timer consists of two 32-bit
	 * registers (high & low). To detect a possible low register
	 * wrap-around between the reads, we need to read the high
	 * register before and after low. The wraparound happens
	 * approximately once per 4 secs. */

	/* get initial gpu_timestamp_hi value */
	gpu_timestamp_hi_prev = gk20a_readl(g, timer_time_1_r());

	for (i = 0; i < max_iterations; ++i) {
		u32 gpu_timestamp_hi = 0;
		u32 gpu_timestamp_lo = 0;

		gpu_timestamp_lo = gk20a_readl(g, timer_time_0_r());
		gpu_timestamp_hi = gk20a_readl(g, timer_time_1_r());

		if (gpu_timestamp_hi == gpu_timestamp_hi_prev) {
			*value = (((u64)gpu_timestamp_hi) << 32) |
				gpu_timestamp_lo;
			return 0;
		}

		/* wrap-around detected, retry */
		gpu_timestamp_hi_prev = gpu_timestamp_hi;
	}

	/* too many iterations, bail out */
	gk20a_err(dev_from_gk20a(g), "failed to read ptimer");
	return -EBUSY;
}

MODULE_LICENSE("GPL v2");
module_init(gk20a_init);
module_exit(gk20a_exit);
