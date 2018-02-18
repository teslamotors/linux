/*
 * arch/arm/mach-tegra/mcerr.c
 *
 * MC error code common to T3x and T11x. T20 has been left alone.
 *
 * Copyright (c) 2010-2016, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define pr_fmt(fmt) "mc-err: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/atomic.h>

#include <linux/platform/tegra/mc.h>
#include <linux/platform/tegra/mcerr.h>
#include <linux/platform/tegra/tegra_emc_err.h>

#define mcerr_pr(fmt, ...)					\
	do {							\
		if (!mcerr_silenced) {				\
			trace_printk(fmt, ##__VA_ARGS__);	\
			pr_err(fmt, ##__VA_ARGS__);		\
		}						\
	} while (0)

static bool mcerr_throttle_enabled = true;
static u32  mcerr_silenced;
static u32  mcerr_print_spurious_irq; /* debug */

static int arb_intr_mma_set(const char *arg, const struct kernel_param *kp);
static int arb_intr_mma_get(char *buff, const struct kernel_param *kp);
static void unthrottle_prints(struct work_struct *work);

static int spurious_intrs;

static struct arb_emem_intr_info arb_intr_info = {
	.lock = __SPIN_LOCK_UNLOCKED(arb_intr_info.lock),
};
static int arb_intr_count;

static struct kernel_param_ops arb_intr_mma_ops = {
	.get = arb_intr_mma_get,
	.set = arb_intr_mma_set,
};

module_param_cb(arb_intr_mma_in_ms, &arb_intr_mma_ops,
		&arb_intr_info.arb_intr_mma, S_IRUGO | S_IWUSR);
module_param(arb_intr_count, int, S_IRUGO | S_IWUSR);
module_param(spurious_intrs, int, S_IRUGO | S_IWUSR);

/*
 * Some platforms report SMMU errors via the SMMU driver.
 */
static int report_smmu_errs;
static const char *const smmu_page_attrib[] = {
	"nr-nw-s",
	"nr-nw-ns",
	"nr-wr-s",
	"nr-wr-ns",
	"rd-nw-s",
	"rd-nw-ns",
	"rd-wr-s",
	"rd-wr-ns"
};

/*
 * Table of known errors and their interrupt signatures.
 */
static const struct mc_error mc_errors[] = {
	MC_ERR(MC_INT_DECERR_EMEM,
	       "EMEM address decode error",
	       0, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_DECERR_EMEM | MC_INT_SECURITY_VIOLATION,
	       "EMEM address decode error",
	       0, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_DECERR_VPR,
	       "MC request violates VPR requirements",
	       0, MC_ERR_VPR_STATUS, MC_ERR_VPR_ADR),
	MC_ERR(MC_INT_SECURITY_VIOLATION,
	       "non secure access to secure region",
	       0, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_SECERR_SEC,
	       "MC request violated SEC carveout requirements",
	       0, MC_ERR_SEC_STATUS, MC_ERR_SEC_ADR),

	/*
	 * SMMU related faults.
	 */
	MC_ERR(MC_INT_INVALID_SMMU_PAGE,
	       "SMMU address translation fault",
	       E_SMMU, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_INVALID_SMMU_PAGE | MC_INT_DECERR_EMEM,
	       "EMEM decode error on PDE or PTE entry",
	       E_SMMU, MC_ERR_STATUS, MC_ERR_ADR),
	MC_ERR(MC_INT_INVALID_SMMU_PAGE | MC_INT_SECERR_SEC,
	       "secure SMMU address translation fault",
	       E_SMMU, MC_ERR_SEC_STATUS, MC_ERR_SEC_ADR),
	MC_ERR(MC_INT_INVALID_SMMU_PAGE | MC_INT_DECERR_VPR,
	       "VPR SMMU address translation fault",
	       E_SMMU, MC_ERR_VPR_STATUS, MC_ERR_VPR_ADR),
	MC_ERR(MC_INT_INVALID_SMMU_PAGE | MC_INT_DECERR_VPR |
	       MC_INT_DECERR_EMEM,
	       "EMEM decode error on PDE or PTE entry on VPR context",
	       E_SMMU, MC_ERR_VPR_STATUS, MC_ERR_VPR_ADR),

	/*
	 * Baseband controller related faults.
	 */
	MC_ERR(MC_INT_BBC_PRIVATE_MEM_VIOLATION,
	       "client accessed BBC aperture",
	       0, MC_ERR_BBC_STATUS, MC_ERR_BBC_ADR),
	MC_ERR(MC_INT_DECERR_BBC,
	       "BBC accessed memory outside of its aperture",
	       E_NO_STATUS, 0, 0),

	/*
	 * MTS access violation.
	 */
	MC_ERR(MC_INT_DECERR_MTS,
	       "MTS carveout access violation",
	       0, MC_ERR_MTS_STATUS, MC_ERR_MTS_ADR),

	/*
	 * Generalized carveouts.
	 */
#ifndef CONFIG_ARCH_TEGRA_12x_SOC
	MC_ERR(MC_INT_DECERR_GENERALIZED_CARVEOUT,
	       "GSC access violation", 0,
	       MC_ERR_GENERALIZED_CARVEOUT_STATUS,
	       MC_ERR_GENERALIZED_CARVEOUT_ADR),
	MC_ERR(MC_INT_DECERR_GENERALIZED_CARVEOUT | MC_INT_DECERR_EMEM,
	       "EMEM GSC access violation", 0,
	       MC_ERR_GENERALIZED_CARVEOUT_STATUS,
	       MC_ERR_GENERALIZED_CARVEOUT_ADR),
#endif

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	MC_ERR(MC_INT_WCAM_ERR, "WCAM error", E_TWO_STATUS,
	       MC_WCAM_IRQ_P0_STATUS0,
	       MC_WCAM_IRQ_P1_STATUS0),
#endif

	/*
	 * Miscellaneous errors.
	 */
	MC_ERR(MC_INT_INVALID_APB_ASID_UPDATE,
	       "invalid APB ASID update", 0,
	       MC_ERR_STATUS, MC_ERR_ADR),

	/* NULL terminate. */
	MC_ERR(0, NULL, 0, 0, 0),
};

static atomic_t error_count;

static DECLARE_DELAYED_WORK(unthrottle_prints_work, unthrottle_prints);

static struct dentry *mcerr_debugfs_dir;

u32 mc_int_mask;
int mc_err_channel;
int mc_intstatus_reg;

/*
 * Allow the top and bottom halfs of the MC error handlers to be overwritten.
 */
irqreturn_t __weak tegra_mc_error_thread_ovr(int irq, void *data);
irqreturn_t __weak tegra_mc_error_hard_irq_ovr(int irq, void *data);
irqreturn_t __weak tegra_mc_handle_hubc_fault(int src_chan, int intstatus);

/*
 * Chip specific functions.
 */
static struct mcerr_chip_specific chip_specific;

static int arb_intr_mma_set(const char *arg, const struct kernel_param *kp)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&arb_intr_info.lock, flags);
	ret = param_set_int(arg, kp);
	spin_unlock_irqrestore(&arb_intr_info.lock, flags);
	return ret;
}

static int arb_intr_mma_get(char *buff, const struct kernel_param *kp)
{
	return param_get_int(buff, kp);
}

static void arb_intr(void)
{
	u64 time;
	u32 time_diff_ms;
	unsigned long flags;

	spin_lock_irqsave(&arb_intr_info.lock, flags);
	arb_intr_count++;
	time = sched_clock();
	time_diff_ms = (time - arb_intr_info.time) >> 20;
	arb_intr_info.time = time;
	arb_intr_info.arb_intr_mma =
		((MMA_HISTORY_SAMPLES - 1) * time_diff_ms +
		 arb_intr_info.arb_intr_mma) / MMA_HISTORY_SAMPLES;
	spin_unlock_irqrestore(&arb_intr_info.lock, flags);
}

static void unthrottle_prints(struct work_struct *work)
{
	atomic_set(&error_count, 0);
}

static void mcerr_info_update(struct mc_client *c, u32 stat)
{
	int i;

	for (i = 0; i < (sizeof(stat) * 8); i++) {
		if (stat & (1 << i))
			c->intr_counts[i]++;
	}
}

irqreturn_t tegra_mc_handle_general_fault(int src_chan, int intstatus)
{
	struct mc_client *client;
	const struct mc_error *fault;
	const char *smmu_info;
	phys_addr_t addr;
	u32 status, write, secure, client_id;

	if (intstatus & MC_INT_ARBITRATION_EMEM) {
		arb_intr();
		if (intstatus == MC_INT_ARBITRATION_EMEM)
			return IRQ_HANDLED;
		intstatus &= ~MC_INT_ARBITRATION_EMEM;
	}

	fault = chip_specific.mcerr_info(intstatus & mc_int_mask);
	if (WARN(!fault, "Unknown error! intr sig: 0x%08x\n",
		 intstatus & mc_int_mask))
		return IRQ_HANDLED;

	if (fault->flags & E_NO_STATUS) {
		mcerr_pr("MC fault - no status: %s\n", fault->msg);
		return IRQ_HANDLED;
	}

	status = __mc_readl(src_chan, fault->stat_reg);
	addr = __mc_readl(src_chan, fault->addr_reg);

	if (fault->flags & E_TWO_STATUS) {
		mcerr_pr("MC fault - %s\n", fault->msg);
		mcerr_pr("status: 0x%08x status2: 0x%08llx\n",
			status, addr);
		return IRQ_HANDLED;
	}

	secure = !!(status & MC_ERR_STATUS_SECURE);
	write = !!(status & MC_ERR_STATUS_WRITE);
	client_id = status & 0xff;
	client = &mc_clients[client_id <= mc_client_last
			     ? client_id : mc_client_last];

	mcerr_info_update(client, intstatus & mc_int_mask);

	/*
	 * LPAE: make sure we get the extra 2 physical address bits available
	 * and pass them down to the printing function.
	 */
	addr |= (((phys_addr_t)(status & MC_ERR_STATUS_ADR_HI)) << 12);

	if (fault->flags & E_SMMU)
		smmu_info = smmu_page_attrib[MC_ERR_SMMU_BITS(status)];
	else
		smmu_info = NULL;

	chip_specific.mcerr_print(fault, client, status, addr, secure, write,
				  smmu_info);

	return IRQ_HANDLED;
}

/*
 * Common MC error handling code.
 */
static irqreturn_t tegra_mc_error_thread(int irq, void *data)
{
	unsigned long count;
	irqreturn_t ret = IRQ_HANDLED;
	u32 intstatus = mc_int_mask &
		__mc_readl(mc_err_channel, mc_intstatus_reg);

	/*
	 * Sometimes the MC seems to generate spurious interrupts - that
	 * is interrupts with an interrupt status register equal to 0.
	 * Not much we can do other than keep a count of them.
	 */
	if (!intstatus) {
		if (mcerr_print_spurious_irq)
			pr_warn("%s(): irq=%d spurious interrupt %d",
				__func__, irq, spurious_intrs);
		spurious_intrs++;
		return IRQ_NONE;
	}

	cancel_delayed_work(&unthrottle_prints_work);
	count = atomic_inc_return(&error_count);

	if (mcerr_throttle_enabled && count >= MAX_PRINTS) {
		schedule_delayed_work(&unthrottle_prints_work, HZ/2);
		if (count == MAX_PRINTS)
			mcerr_pr("Too many MC errors; throttling prints\n");
		goto clear_intr;
	}

	if (mc_intstatus_reg == MC_INTSTATUS)
		ret = tegra_mc_handle_general_fault(mc_err_channel, intstatus);
#ifdef MC_HUBC_INTSTATUS
	else if (mc_intstatus_reg == MC_HUBC_INTSTATUS)
		ret = tegra_mc_handle_general_fault(mc_err_channel, intstatus);
#endif
	else
		WARN(1, "Unknown MC intstatus register ???");

	/*
	 * Clear current interrupt and make sure the write has completed before
	 * re-enabling MC interrupts.
	 */
clear_intr:
	__mc_writel(mc_err_channel, intstatus, MC_INTSTATUS);
	__mc_readl(mc_err_channel, MC_INTMASK);
	mc_writel(mc_int_mask, MC_INTMASK);

	return ret;
}

/*
 * The actual error handling takes longer than is ideal so this must be
 * threaded.
 */
static irqreturn_t tegra_mc_error_hard_irq(int irq, void *data)
{
	mc_err_channel = MC_BROADCAST_CHANNEL;
	mc_intstatus_reg = MC_INTSTATUS;

	trace_printk("MCERR detected.\n");

	/*
	 * We have an interrupt; disable the rest until this one is handled.
	 * This means we will potentially miss interrupts. We can live with
	 * that.
	 */
	mc_writel(0, MC_INTMASK);
	mc_readl(MC_INTMASK);

	return IRQ_WAKE_THREAD;
}

static const struct mc_error *mcerr_default_info(u32 intr)
{
	const struct mc_error *err;

	for (err = mc_errors; err->sig && err->msg; err++) {
		if (intr != err->sig)
			continue;
		return err;
	}

	return NULL;
}

void __weak smmu_dump_pagetable(int swgid, dma_addr_t addr)
{
}

/*
 * This will print at least 8 hex digits for address. If the address is bigger
 * then more digits will be printed but the full 16 hex digits for a 64 bit
 * address will not get printed by the current code.
 */
static void mcerr_default_print(const struct mc_error *err,
				const struct mc_client *client,
				u32 status, phys_addr_t addr,
				int secure, int rw, const char *smmu_info)
{
	if (smmu_info)
		smmu_dump_pagetable(client->swgid, addr);

	mcerr_pr("(%d) %s: %s\n", client->swgid, client->name, err->msg);
	mcerr_pr("  status = 0x%08x; addr = 0x%08llx\n", status,
		 (long long unsigned int)addr);
	if (report_smmu_errs)
		mcerr_pr("  secure: %s, access-type: %s, SMMU fault: %s\n",
			secure ? "yes" : "no", rw ? "write" : "read",
			smmu_info ? smmu_info : "none");
	else
		mcerr_pr("  secure: %s, access-type: %s\n",
			secure ? "yes" : "no", rw ? "write" : "read");
}

/*
 * Print the MC err stats for each client.
 */
static int mcerr_default_debugfs_show(struct seq_file *s, void *v)
{
	int i, j;
	int do_print;

	seq_printf(s, "%-18s %-18s", "swgroup", "client");
	for (i = 0; i < (sizeof(u32) * 8); i++) {
		if (chip_specific.intr_descriptions[i])
			seq_printf(s, " %-12s",
				   chip_specific.intr_descriptions[i]);
	}
	seq_puts(s, "\n");

	for (i = 0; i < chip_specific.nr_clients; i++) {
		do_print = 0;

		/* Only print clients who actually have errors. */
		for (j = 0; j < (sizeof(u32) * 8); j++) {
			if (chip_specific.intr_descriptions[j] &&
			    mc_clients[i].intr_counts[j]) {
				do_print = 1;
				break;
			}
		}

		if (do_print) {
			seq_printf(s, "%-18s %-18s",
				   mc_clients[i].name,
				   mc_clients[i].swgroup);
			for (j = 0; j < (sizeof(u32) * 8); j++) {
				if (!chip_specific.intr_descriptions[j])
					continue;
				seq_printf(s, " %-12u",
					   mc_clients[i].intr_counts[j]);
			}
			seq_puts(s, "\n");
		}
	}

	return 0;
}

static int mcerr_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, chip_specific.mcerr_debugfs_show, NULL);
}

static const struct file_operations mcerr_debugfs_fops = {
	.open           = mcerr_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int __get_throttle(void *data, u64 *val)
{
	*val = mcerr_throttle_enabled;
	return 0;
}

static int __set_throttle(void *data, u64 val)
{
	atomic_set(&error_count, 0);

	mcerr_throttle_enabled = (bool) val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(mcerr_throttle_debugfs_fops, __get_throttle,
			__set_throttle, "%llu\n");

/*
 * This will always be successful. However, if something goes wrong in the
 * init a message will be printed to the kernel log. Since this is a
 * non-essential piece of the kernel no reason to fail the entire MC init
 * if this fails.
 */
int tegra_mcerr_init(struct dentry *mc_parent, struct platform_device *pdev)
{
	int irq;
	const void *prop;

	irqreturn_t (*irq_top)(int irq, void *data) =
		tegra_mc_error_hard_irq;
	irqreturn_t (*irq_bot)(int irq, void *data) =
		tegra_mc_error_thread;

	chip_specific.mcerr_info         = mcerr_default_info;
	chip_specific.mcerr_print        = mcerr_default_print;
	chip_specific.mcerr_debugfs_show = mcerr_default_debugfs_show;
	chip_specific.nr_clients = 0;

	prop = of_get_property(pdev->dev.of_node,"compatible", NULL);
	if (prop && strcmp(prop, "nvidia,tegra-t18x-mc") == 0)
		report_smmu_errs = 0;
	else
		report_smmu_errs = 1;

	/*
	 * mcerr_chip_specific_setup() can override any of the default
	 * functions as it wishes.
	 */
	mcerr_chip_specific_setup(&chip_specific);
	if (chip_specific.nr_clients == 0 ||
	    chip_specific.intr_descriptions == NULL) {
		pr_err("Missing necessary chip_specific functionality!\n");
		return -ENODEV;
	}

	if (tegra_mc_error_hard_irq_ovr)
		irq_top = tegra_mc_error_hard_irq_ovr;
	if (tegra_mc_error_thread_ovr)
		irq_bot = tegra_mc_error_thread_ovr;

	prop = of_get_property(pdev->dev.of_node, "int_mask", NULL);
	if (!prop) {
		pr_err("No int_mask prop for mcerr!\n");
		return -EINVAL;
	}

	mc_int_mask = be32_to_cpup(prop);
	mc_writel(mc_int_mask, MC_INTMASK);
	pr_info("Set intmask: 0x%x\n", mc_readl(MC_INTMASK));

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (irq < 0) {
		pr_err("Unable to parse/map MC error interrupt\n");
		goto done;
	}

	if (request_threaded_irq(irq, irq_top, irq_bot, 0, "mc_status", NULL)) {
		pr_err("Unable to register MC error interrupt\n");
		goto done;
	}

	tegra_emcerr_init(mc_parent, pdev);

	if (!mc_parent)
		goto done;

	mcerr_debugfs_dir = debugfs_create_dir("err", mc_parent);
	if (mcerr_debugfs_dir == NULL) {
		pr_err("Failed to make debugfs node: %ld\n",
		       PTR_ERR(mcerr_debugfs_dir));
		goto done;
	}
	debugfs_create_file("mcerr", 0644, mcerr_debugfs_dir, NULL,
			    &mcerr_debugfs_fops);
	debugfs_create_file("mcerr_throttle", S_IRUGO | S_IWUSR,
			    mcerr_debugfs_dir, NULL,
			    &mcerr_throttle_debugfs_fops);
	debugfs_create_u32("quiet", 0644, mcerr_debugfs_dir, &mcerr_silenced);
	debugfs_create_bool("print_spurious_irq", 0644, mcerr_debugfs_dir,
			    &mcerr_print_spurious_irq);
done:
	return 0;
}
