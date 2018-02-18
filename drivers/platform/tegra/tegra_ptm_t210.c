/*
 * arch/arm/mach-tegra/tegra_ptm.c
 *
 * Copyright (c) 2012-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/sysrq.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>
#include <linux/kallsyms.h>
#include <linux/clk.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>
#include <linux/cpu.h>
#include "pm.h"
#include <linux/tegra_ptm.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/cpu_pm.h>

/*
_________________________________________________________
|CLUSTER		ATID	Processor	Protocol|
|_______________________________________________________|
|BCCPLEX,Fast Cluster,	0x40	CPU0		ETMv4	|
|Big cluster=Atlas=	0x41	CPU1		ETMv4	|
|Cortex A57		0x42	CPU2		ETMv4	|
|			0x43	CPU3		ETMv4	|
|							|
|LCCPLEX,Slow Cluster,	0x30	CPU0            ETMv4	|
|Little cluster=	0x31	CPU1		ETMv4	|
|Apollo=Cortex A53	0x32	CPU2		ETMv4	|
|			0x33	CPU3		ETMv4	|
|							|
|APE, Cortex A9		0x20	CPU0		PFT1.0	|
|							|
|STM			0x10	NA		MIPI STP|
|_______________________________________________________|

*/

#define ETR_SIZE (16 << 20) /* ETR size: 16MB */

#define TRACE_STATUS_PMSTABLE 2
#define TRACE_STATUS_IDLE     1

#define ADDR_REG_PAIRS 4
#define ADDR_REGS 8

#define AFREADY_LOOP_MAX 100
#define AFREADY_LOOP_MS 4

#define CPU_DUMP_BUFF_SZ 64    /* size of dump buffer in bytes
				  (word size * reg num) */

/* PTM tracer state */
struct tracectx {
	struct device	*dev;
	struct clk	*csite_clk;
	struct mutex	mutex;
	void __iomem	*funnel_major_regs;
	void __iomem	*etf_regs;
	void __iomem	*replicator_regs;
	void __iomem	*etr_regs;
	void __iomem	*funnel_minor_regs;
	void __iomem	*ape_regs;
	void __iomem	*ptm_regs[CONFIG_NR_CPUS];
	void __iomem	*funnel_bccplex_regs;
	void __iomem	*tpiu_regs;
	u8		*etr_address;
	uintptr_t	etr_ll;
	int		*trc_buf;
	u8		*etr_buf;
	int		buf_size;
	uintptr_t	start_address[ADDR_REG_PAIRS];
	uintptr_t	stop_address[ADDR_REG_PAIRS];
	u8		start_stop_mask;

	u32		reg_ctx[CONFIG_NR_CPUS][100];

	unsigned long	enable;
	unsigned long	userspace;
	unsigned long	branch_broadcast;
	unsigned long	return_stack;
	unsigned long	formatter;
	unsigned long	timestamp;
	unsigned long	etr;
	unsigned long	ape;

	int		trigger;
	uintptr_t	trigger_address[ADDR_REGS];
	u8		trigger_mask;
	unsigned int	cycle_count;
	char		cpu_dump_buffer[CPU_DUMP_BUFF_SZ];
};

/* initialising some values of the structure */
static struct tracectx tracer = {
	.start_address			=	{ 0 },
	.stop_address			=	{ 0xFFFFFFC100000000 },
	.enable				=	0,
	.userspace			=	0,
	.branch_broadcast		=	1,
	.return_stack			=	0,
	.trigger			=	0,
	.formatter			=	1,
	.timestamp			=	0,
	.cycle_count			=	0,
	.trigger_address		=	{ 0 },
	.etr			=	0,
	.ape				=	0,
	.start_stop_mask		=	0x3,
	.trigger_mask			=	0,
	.cpu_dump_buffer		=	{ 0 }
};

static struct clk *csite_clk;

/* Allocate a buffer to hold the ETF traces */
static void trc_buf_init(struct tracectx *t)
{
	t->trc_buf = devm_kzalloc(t->dev, MAX_ETF_SIZE * sizeof(*t->trc_buf),
					GFP_KERNEL);

	if (NULL == t->trc_buf)
		dev_err(t->dev, "failes to allocate memory to hold ETF\n");
}

/* Initialise the APE registers */
static void ape_init(struct tracectx *t)
{
	ape_regs_unlock(t);

	ape_writel(t, 0x500, CORESIGHT_APE_CPU0_ETM_ETMCR_0);

	ape_writel(t, 1, CORESIGHT_APE_CPU0_ETM_ETMTECR1_0);

	ape_writel(t, 0x10, CORESIGHT_APE_CPU0_ETM_ETMTEEVR_0);

	ape_writel(t, 0x80000000, CORESIGHT_APE_CPU0_ETM_ETMACVR1_0);
	ape_writel(t, 0xffffffff, CORESIGHT_APE_CPU0_ETM_ETMACVR2_0);

	ape_writel(t, 0, CORESIGHT_APE_CPU0_ETM_ETMACTR1_0);
	ape_writel(t, 0, CORESIGHT_APE_CPU0_ETM_ETMACTR2_0);

	ape_writel(t, 0x20, CORESIGHT_APE_CPU0_ETM_ETMATID_0);

	dev_dbg(t->dev, "APE is initialized.\n");
}

static inline void ptm_check_trace_stable(struct tracectx *t, int id, int bit)
{
	int trcstat_idle;

	trcstat_idle = 0x0;
	while (trcstat_idle == 0x0) {
		trcstat_idle = ptm_readl(t, id, TRCSTAT);
		trcstat_idle = trcstat_idle & bit;
	}
}

#define PTM_REG_SAVE_RESTORE_LIST \
	do { \
		/* main cfg and control registers */ \
		SAVE_RESTORE_PTM(TRCPRGCTLR); \
		SAVE_RESTORE_PTM(TRCPROCSELR); \
		SAVE_RESTORE_PTM(TRCCONFIGR); \
		SAVE_RESTORE_PTM(TRCEVENTCTL0R); \
		SAVE_RESTORE_PTM(TRCEVENTCTL1R); \
		SAVE_RESTORE_PTM(TRCQCTLR); \
		SAVE_RESTORE_PTM(TRCTRACEIDR); \
		SAVE_RESTORE_PTM(TRCSTALLCTLR); \
		SAVE_RESTORE_PTM(TRCTSCTLR); \
		SAVE_RESTORE_PTM(TRCSYNCPR); \
		SAVE_RESTORE_PTM(TRCCCCTLR); \
		SAVE_RESTORE_PTM(TRCBBCTLR); \
\
		/* filtering control registers */ \
		SAVE_RESTORE_PTM(TRCVICTLR); \
		SAVE_RESTORE_PTM(TRCVIIECTLR); \
		SAVE_RESTORE_PTM(TRCVISSCTLR); \
		SAVE_RESTORE_PTM(TRCVIPCSSCTLR); \
\
		/* derived resources registers */ \
		SAVE_RESTORE_PTM(TRCSEQEVR0); \
		SAVE_RESTORE_PTM(TRCSEQEVR1); \
		SAVE_RESTORE_PTM(TRCSEQEVR1); \
		SAVE_RESTORE_PTM(TRCSEQRSTEVR); \
		SAVE_RESTORE_PTM(TRCSEQSTR); \
		SAVE_RESTORE_PTM(TRCCNTRLDVR0); \
		SAVE_RESTORE_PTM(TRCCNTRLDVR1); \
		SAVE_RESTORE_PTM(TRCCNTVR0); \
		SAVE_RESTORE_PTM(TRCCNTVR1); \
		SAVE_RESTORE_PTM(TRCCNTCTLR0); \
		SAVE_RESTORE_PTM(TRCCNTCTLR1); \
		SAVE_RESTORE_PTM(TRCEXTINSELR); \
\
		/* resource selection registers */ \
		SAVE_RESTORE_PTM(TRCRSCTLR2); \
		SAVE_RESTORE_PTM(TRCRSCTLR3); \
		SAVE_RESTORE_PTM(TRCRSCTLR4); \
		SAVE_RESTORE_PTM(TRCRSCTLR5); \
		SAVE_RESTORE_PTM(TRCRSCTLR6); \
		SAVE_RESTORE_PTM(TRCRSCTLR7); \
		SAVE_RESTORE_PTM(TRCRSCTLR8); \
		SAVE_RESTORE_PTM(TRCRSCTLR9); \
		SAVE_RESTORE_PTM(TRCRSCTLR10); \
		SAVE_RESTORE_PTM(TRCRSCTLR11); \
		SAVE_RESTORE_PTM(TRCRSCTLR12); \
		SAVE_RESTORE_PTM(TRCRSCTLR13); \
		SAVE_RESTORE_PTM(TRCRSCTLR14); \
		SAVE_RESTORE_PTM(TRCRSCTLR15); \
\
		/* comparator registers */ \
		SAVE_RESTORE_PTM(TRCACVR0); \
		SAVE_RESTORE_PTM(TRCACVR0+4); \
		SAVE_RESTORE_PTM(TRCACVR1); \
		SAVE_RESTORE_PTM(TRCACVR1+4); \
		SAVE_RESTORE_PTM(TRCACVR2); \
		SAVE_RESTORE_PTM(TRCACVR2+4); \
		SAVE_RESTORE_PTM(TRCACVR3); \
		SAVE_RESTORE_PTM(TRCACVR3+4); \
		SAVE_RESTORE_PTM(TRCACVR4); \
		SAVE_RESTORE_PTM(TRCACVR4+4); \
		SAVE_RESTORE_PTM(TRCACVR5); \
		SAVE_RESTORE_PTM(TRCACVR5+4); \
		SAVE_RESTORE_PTM(TRCACVR6); \
		SAVE_RESTORE_PTM(TRCACVR6+4); \
		SAVE_RESTORE_PTM(TRCACVR7); \
		SAVE_RESTORE_PTM(TRCACVR7+4); \
\
		SAVE_RESTORE_PTM(TRCACATR0); \
		SAVE_RESTORE_PTM(TRCACATR1); \
		SAVE_RESTORE_PTM(TRCACATR2); \
		SAVE_RESTORE_PTM(TRCACATR3); \
		SAVE_RESTORE_PTM(TRCACATR4); \
		SAVE_RESTORE_PTM(TRCACATR5); \
		SAVE_RESTORE_PTM(TRCACATR6); \
		SAVE_RESTORE_PTM(TRCACATR7); \
\
		SAVE_RESTORE_PTM(TRCCIDCVR0); \
		SAVE_RESTORE_PTM(TRCCIDCCTLR0); \
		SAVE_RESTORE_PTM(TRCVMIDCVR0); \
\
		/* single-shot comparator registers */ \
		SAVE_RESTORE_PTM(TRCSSCCR0); \
		SAVE_RESTORE_PTM(TRCSSCSR0); \
\
		/* claim tag registers */ \
		SAVE_RESTORE_PTM(TRCCLAIMCLR); \
	} while (0);

#define SAVE_RESTORE_PTM(reg) \
	(addr[cnt++] = readl_relaxed(reg_base + reg))

static void ptm_save(struct tracectx *t)
{
	u32 *addr;
	int cnt = 0;
	int id = raw_smp_processor_id();
	void __iomem *reg_base;

	dsb(sy);
	isb();

	addr = t->reg_ctx[id];
	reg_base = t->ptm_regs[id];

	ptm_regs_unlock(t, id);

	PTM_REG_SAVE_RESTORE_LIST;
	rmb();
}

#undef SAVE_RESTORE_PTM
#define SAVE_RESTORE_PTM(reg) ptm_writel(t, id, addr[cnt++], reg)

static void ptm_restore(struct tracectx *t)
{
	u32 *addr;
	int cnt = 0;
	int id = raw_smp_processor_id();

	addr = t->reg_ctx[id];

	ptm_regs_unlock(t, id);
	ptm_os_lock(t, id);

	PTM_REG_SAVE_RESTORE_LIST;

	ptm_os_unlock(t, id);
}

/* Initialise the PTM registers */
static void ptm_init(void *p_info)
{
	u32 id, level_val, addr_mask;
	int trccfg, i;
	struct tracectx *t;

	t = (struct tracectx *)p_info;
	id = raw_smp_processor_id();

	/* Power up the CPU PTM */
	ptm_writel(t, id, 8, TRCPDCR);

	/* Unlock the CPU PTM */
	ptm_regs_unlock(t, id);

	/* clear OS lock */
	ptm_writel(t, id, 0, TRCOSLAR);

	/* clear main enable bit to enable register programming */
	ptm_writel(t, id, 0, TRCPRGCTLR);

	ptm_check_trace_stable(t, id, TRACE_STATUS_IDLE);

	/* Clear uninitialised flopes , else we get event packets */
	ptm_writel(t, id, 0, TRCEVENTCTL1R);
	ptm_writel(t, id, 0, TRCEVENTCTL0R);
	ptm_writel(t, id, 0, TRCTSCTLR);
	ptm_writel(t, id, 0, TRCCCCTLR);
	ptm_writel(t, id, 0, TRCVISSCTLR);
	ptm_writel(t, id, 0, TRCSEQEVR0);
	ptm_writel(t, id, 0, TRCSEQEVR1);
	ptm_writel(t, id, 0, TRCSEQEVR2);
	ptm_writel(t, id, 0, TRCSEQRSTEVR);
	ptm_writel(t, id, 0, TRCSEQSTR);
	ptm_writel(t, id, 0, TRCEXTINSELR);
	ptm_writel(t, id, 0, TRCCNTRLDVR0);
	ptm_writel(t, id, 0, TRCCNTRLDVR1);
	ptm_writel(t, id, 0, TRCCNTCTLR0);
	ptm_writel(t, id, 0, TRCCNTCTLR1);
	ptm_writel(t, id, 0, TRCCNTVR0);
	ptm_writel(t, id, 0, TRCCNTVR1);
	ptm_writel(t, id, 0, TRCRSCTLR2);
	ptm_writel(t, id, 0, TRCRSCTLR3);
	ptm_writel(t, id, 0, TRCRSCTLR4);
	ptm_writel(t, id, 0, TRCRSCTLR5);
	ptm_writel(t, id, 0, TRCRSCTLR6);
	ptm_writel(t, id, 0, TRCRSCTLR7);
	ptm_writel(t, id, 0, TRCRSCTLR8);
	ptm_writel(t, id, 0, TRCRSCTLR9);
	ptm_writel(t, id, 0, TRCRSCTLR10);
	ptm_writel(t, id, 0, TRCRSCTLR11);
	ptm_writel(t, id, 0, TRCRSCTLR12);
	ptm_writel(t, id, 0, TRCRSCTLR13);
	ptm_writel(t, id, 0, TRCRSCTLR14);
	ptm_writel(t, id, 0, TRCRSCTLR15);
	ptm_writel(t, id, 0, TRCSSCSR0);
	ptm_writel(t, id, 0, TRCACVR0);
	ptm_writel(t, id, 0, TRCACVR1);
	ptm_writel(t, id, 0, TRCACVR2);
	ptm_writel(t, id, 0, TRCACVR3);
	ptm_writel(t, id, 0, TRCACVR4);
	ptm_writel(t, id, 0, TRCACVR5);
	ptm_writel(t, id, 0, TRCACVR6);
	ptm_writel(t, id, 0, TRCACVR7);
	ptm_writel(t, id, 0, TRCACATR0);
	ptm_writel(t, id, 0, TRCACATR1);
	ptm_writel(t, id, 0, TRCACATR2);
	ptm_writel(t, id, 0, TRCACATR3);
	ptm_writel(t, id, 0, TRCACATR4);
	ptm_writel(t, id, 0, TRCACATR5);
	ptm_writel(t, id, 0, TRCACATR6);
	ptm_writel(t, id, 0, TRCACATR7);
	ptm_writel(t, id, 0, TRCCIDCVR0);
	ptm_writel(t, id, 0, TRCVMIDCVR0);
	ptm_writel(t, id, 0, TRCCIDCCTLR0);
	ptm_writel(t, id, 0, TRCVIIECTLR);
	ptm_writel(t, id, 0, TRCBBCTLR);
	ptm_writel(t, id, 0, TRCSTALLCTLR);

	/* Configure basic controls RS, BB, TS, VMID, Context tracing */
	trccfg = ptm_readl(t, id, TRCCONFIGR);

	/* enable BB for address range in TRCACVR0/TRCACVR1 pair */
	if (t->branch_broadcast) {
		ptm_writel(t, id, 0x101, TRCBBCTLR);
	}

	trccfg = trccfg | (t->branch_broadcast << 3) | (t->return_stack << 12) |
			(t->timestamp<<11);

	/* set cycle count threshold */
	if (t->cycle_count) {
		trccfg = trccfg | (1<<4);
		i = ptm_readl(t, id, TRCIDR3) & 0xFFF;
		if (t->cycle_count < i)
			t->cycle_count = i;
		ptm_writel(t, id, t->cycle_count, TRCCCCTLR);
	}
	ptm_writel(t, id, trccfg, TRCCONFIGR);

	/* Enable Periodic Synchronisation after 1024 bytes */
	ptm_writel(t, id, 0xa, TRCSYNCPR);

	/* Program Trace ID register */
	ptm_writel(t, id, (!(is_lp_cluster()) ?
			BCCPLEX_BASE_ID : LCCPLEX_BASE_ID) + id,
			TRCTRACEIDR);

	/* Program Address range comparators */
	for (i = 0; i < ADDR_REG_PAIRS; i++) {
		if ((t->start_stop_mask >> (i*2)) & 0x3) {
			ptm_writeq(t, id, t->start_address[i],
					TRCACVR0 + 8 * i * 2);
			ptm_writeq(t, id, t->stop_address[i],
					TRCACVR0 + 8 * (i * 2 + 1));
		}
	}

	for (i = 0; i < ADDR_REGS; i++)
		if ((t->trigger_mask >> i) & 0x1)
			ptm_writeq(t, id, t->trigger_address[i],
					TRCACVR0 + 8 * i);

	/* control user space tracing for each address range */
	level_val = (t->userspace ? 0x0 : 0x1100);
	for (i = 0; i < ADDR_REGS; i++)
		ptm_writel(t, id, level_val, TRCACATR0 + 8 * i);

	/* Select Resource 2 to include all ARC */
	addr_mask = 0x50000;
	for (i = 0; i < ADDR_REG_PAIRS; i++)
		if ((t->start_stop_mask >> (i * 2)) & 0x3)
			addr_mask += (0x1 << i);

	ptm_writel(t, id, addr_mask, TRCRSCTLR2);

	/* Enable ViewInst and Include logic for ARC. Disable the SSC */
	/* this is unecessary in addition to start/stop programming */
	/*ptm_writel(t, id, 0xf, TRCVIIECTLR);*/

	/* Select Start/Stop as Started. And select Resource 2 as the Event */
	if (t->userspace)
		ptm_writel(t, id, 0xE02, TRCVICTLR);
	else
		ptm_writel(t, id, 0x110E02, TRCVICTLR);

	/* Trace everything till Address match packet hits. Use Single Shot
	comparator and generate Event Packet */
	if (t->trigger) {
		/* Select SAC2 for SSC0 and enable SSC to re-fire on match */
		ptm_writel(t, id, t->trigger_mask, TRCSSCCR0);
		/* Clear SSC Status bit 31 */
		ptm_writel(t, id, 0x0, TRCSSCSR0);
		/* Select Resource 3 to include SSC0 */
		ptm_writel(t, id, 0x30001, TRCRSCTLR3);
		/* Select Resource3 as part of Event0 */
		ptm_writel(t, id, 0x3,
				TRCEVENTCTL0R);
		/* Insert ATB packet with ATID=0x7D and Payload=Master ATID.
		ETF can use this to Stop and flush based on this */
		ptm_writel(t, id, 0x800, TRCEVENTCTL1R);
		if (t->timestamp)
			/* insert global timestamp if enabled */
			ptm_writel(t, id, 0x3, TRCTSCTLR);
	}
	ptm_writel(t, id, 1, TRCPRGCTLR);

	dev_dbg(t->dev, "PTM%d initialized.\n", id);
}

/* Initialise the funnel bccplex registers */
static void funnel_bccplex_init(struct tracectx *t)
{
	/* unlock BCCPLEX funnel */
	funnel_bccplex_regs_unlock(t);

	/* set up all funnel ports  and set HT as 0x04 */
	funnel_bccplex_writel(t, 0x30F,
		CORESIGHT_ATBFUNNEL_BCCPLEX_CXATBFUNNEL_REGS_CTRL_REG_0);

	dev_dbg(t->dev, "Funnel bccplex initialized.\n");
}

/* Initialise the funnel major registers */
static void funnel_major_init(struct tracectx *t)
{
	/* unlock  funnel */
	funnel_major_regs_unlock(t);

	/* enable funnel port 0 and 2  and make HT as 0x04 */
	funnel_major_writel(t, 0x305,
		CORESIGHT_ATBFUNNEL_HUGO_MAJOR_CXATBFUNNEL_REGS_CTRL_REG_0);

	dev_dbg(t->dev, "Funnel Major initialized.\n");
}

/* Initialise the funnel minor registers */
static void funnel_minor_init(struct tracectx *t)
{
	/* unlock  funnel */
	funnel_minor_regs_unlock(t);

	/* enable funnel port 0 and make HT as 0x04 */
	funnel_minor_writel(t, 0x302,
		CORESIGHT_ATBFUNNEL_MINOR_CXATBFUNNEL_REGS_CTRL_REG_0);

	dev_dbg(t->dev, "Funnel Minor initialized.\n");
}

/* Initialize the ETF registers */
static void etf_init(struct tracectx *t)
{
	int ffcr, words_after_trigger;

	/*unlock ETF */
	etf_regs_unlock(t);

	/* Reset RWP and RRP when disabled */
	etf_writel(t, 0, CXTMC_REGS_RWP_0);
	etf_writel(t, 0, CXTMC_REGS_RRP_0);

	/* Enabling capturing of trace data and Circular mode */
	etf_writel(t, 0, CXTMC_REGS_MODE_0);
	if (t->trigger) {
		/* Capture data for specified cycles after TRIGIN */
		words_after_trigger = (t->trigger
				* MAX_ETF_SIZE) / 100;
		etf_writel(t, words_after_trigger, CXTMC_REGS_TRG_0);
		/* Flush on Trigin, insert trigger and stop on
						Flush Complete */
		etf_writel(t, 0x1120, CXTMC_REGS_FFCR_0);
	}

	ffcr = etf_readl(t, CXTMC_REGS_FFCR_0);
	ffcr &= ~1;
	ffcr |= t->formatter;
	etf_writel(t, ffcr, CXTMC_REGS_FFCR_0);

	/* Enable ETF */
	etf_writel(t, 1, CXTMC_REGS_CTL_0);
	dev_dbg(t->dev, "ETF initialized.\n");
}

/* Initialise the replicator registers*/
static void replicator_init(struct tracectx *t)
{
	/* unlock replicator */
	replicator_regs_unlock(t);

	/* disabling the replicator */
	replicator_writel(t, 0xff ,
	CORESIGHT_ATBREPLICATOR_HUGO_CXATBREPLICATOR_REGISTERS_IDFILTER1_0);

	dev_dbg(t->dev, "Replicator initialized.\n");
}

static void tpiu_init(struct tracectx *t)
{
	tpiu_regs_unlock(t);
	tpiu_writel(t, 0x1040, TPIU_FF_CTRL);
	tpiu_regs_lock(t);
}

/* Initialise the ETR registers */
static void etr_init(struct tracectx *t)
{
	int words_after_trigger, ffcr;

	__flush_dcache_area(t->etr_address, ETR_SIZE);

	/* trace data is passing through ETF to ETR */
	etf_regs_unlock(t);

	/* to use ETR, ETF must be configured as HW FiFo first */
	etf_writel(t, 0x2, CXTMC_REGS_MODE_0);
	/* enable formatter, required in HW FiFo mode */
	etf_writel(t, 0x1, CXTMC_REGS_FFCR_0);
	/* enable ETF capture */
	etf_writel(t, 1, CXTMC_REGS_CTL_0);

	etf_regs_lock(t);

	etr_regs_unlock(t);

	etr_writel(t, (ETR_SIZE >> 2), CXTMC_REGS_RSZ_0);
	etr_writel(t, 0, CXTMC_REGS_MODE_0); /* circular mode */

	/* non-secure, SG mode, WrBurstLen 0x3 */
	etr_writel(t, 0x380, CXTMC_REGS_AXICTL_0);

	/*allocate space for ETR */
	etr_writeq(t, virt_to_phys((void *)t->etr_ll), CXTMC_REGS_DBALO_0);

	if (t->trigger) {
		/* Capture data for specified cycles after seeing the TRIGIN */
		words_after_trigger = t->trigger * (ETR_SIZE >> 2) / 100;
		etr_writel(t, words_after_trigger,
					CXTMC_REGS_TRG_0);
		/* Flush on Trigin, insert trigger and Stop on Flush Complete */
		etr_writel(t, 0x1120, CXTMC_REGS_FFCR_0);
	}
	ffcr = etr_readl(t, CXTMC_REGS_FFCR_0);
	ffcr &= ~1;
	ffcr |= t->formatter;
	etr_writel(t, ffcr, CXTMC_REGS_FFCR_0);

	/* Enable ETF */
	etr_writel(t, 1, CXTMC_REGS_CTL_0);
	dev_dbg(t->dev, "ETR initialized.\n");
}

/* This function enables PTM traces */
static int trace_t210_start(struct tracectx *t)
{
	clk_enable(csite_clk);

	if (t->ape) {
		/* Programming APE */
		ape_init(t);
		/* Programming minor funnel */
		funnel_minor_init(t);
		/* Enable APE traces */
		ape_writel(t, 0x100, CORESIGHT_APE_CPU0_ETM_ETMCR_0);
	} else {
		/* Programming BCCPLEX funnel */
		funnel_bccplex_init(t);

		/* Programming major funnel */
		funnel_major_init(t);

		if (t->etr)
			/* Program the ETR */
			etr_init(t);
		else
			/* programming the ETF */
			etf_init(t);

		/* programming the replicator */
		replicator_init(t);

		/* disable tpiu */
		tpiu_init(t);

		/* Enabling the ETM */
		on_each_cpu(ptm_init, t, false);
		dsb(sy);
		isb();
	}

	return 0;
}

static void ptm_disable(void *p_info)
{
	struct tracectx *t = (struct tracectx *)p_info;
	u32 id = raw_smp_processor_id();

	ptm_writel(t, id, 0, TRCPRGCTLR);
}

/* Disable the traces */
static int trace_t210_stop(struct tracectx *t)
{
	if (!t->ape)
		/* Disabling the ETM */
		on_each_cpu(ptm_disable, &tracer, true);
	else
		/* Disable APE traces */
		ape_writel(t, 0x500, CORESIGHT_APE_CPU0_ETM_ETMCR_0);

	clk_disable(csite_clk);

	return 0;
}


/* This function transfers the traces from kernel space to user space
 * data: The destination of trace data in user space
 * offset: Position in the trace to begin reading from
 * len: total amount of data to read
 */
static ssize_t trc_read(struct file *file, char __user *data,
	size_t len, loff_t *offset)
{
	struct tracectx *t = file->private_data;
	loff_t len_t = len; /* length of trace data */
	loff_t len_w = 0; /* length of wrap around data (etr only) */
	u8 *start;

	if (*offset + len_t >= t->buf_size)
		len_t = t->buf_size - *offset;

	if (t->etr)
		start = t->etr_buf;
	else
		start = (u8 *)t->trc_buf;

	if (t->etr)
		if (start + *offset + len_t > t->etr_address + ETR_SIZE) {
			len_w = (start + *offset + len_t) -
			(t->etr_address + ETR_SIZE);
			len_t = (t->etr_address + ETR_SIZE) - (start + *offset);
		}

	/* read trace data */
	if (len_t > 0)
		if (copy_to_user(data, start + *offset, len_t))
			return -EFAULT;

	/* read wrap around data (etr only) */
	if (len_w > 0)
		if (copy_to_user(data + len_t, t->etr_address, len_w))
			return -EFAULT;

	*offset += (len_t + len_w);
	return len_t + len_w;
}

/* this function copies traces from the ETF to an array */
static ssize_t trc_fill_buf(struct tracectx *t)
{
	int rwp, count = 0, count_sts=0, overflow;
	int trig_stat, tmcready;
	u64 rwp64;
	u32 pfn;

	if (!t->etf_regs)
		return -EINVAL;

	if (!t->etr_regs)
		return -EINVAL;

	mutex_lock(&t->mutex);
	clk_enable(csite_clk);
	etf_regs_unlock(t);
	etr_regs_unlock(t);

	if (t->trigger) {
		if (t->etr) {
			trig_stat = 0;
			while (trig_stat == 0) {
				trig_stat = etr_readl(t, CXTMC_REGS_STS_0);
				trig_stat = (trig_stat >> 1) & 0x1;
				if (++count_sts > AFREADY_LOOP_MAX)
					break;
				if (!trig_stat)
					mdelay(AFREADY_LOOP_MS);
			}
			do {
				trig_stat = etr_readl(t, CXTMC_REGS_STS_0);
				tmcready = (trig_stat >> 2) & 0x1;
				if (++count > AFREADY_LOOP_MAX)
					break;
				if (!tmcready)
					mdelay(AFREADY_LOOP_MS);
			} while (!tmcready);
		} else {
			trig_stat = 0;
			while (trig_stat == 0) {
				trig_stat = etf_readl(t, CXTMC_REGS_STS_0);
				trig_stat = (trig_stat >> 1) & 0x1;
				if (++count_sts > AFREADY_LOOP_MAX)
					break;
				if (!trig_stat)
					mdelay(AFREADY_LOOP_MS);
			}
			do {
				trig_stat = etf_readl(t, CXTMC_REGS_STS_0);
				tmcready = (trig_stat >> 2) & 0x1;
				if (++count > AFREADY_LOOP_MAX)
					break;
				if (!tmcready)
					mdelay(AFREADY_LOOP_MS);
			} while (!tmcready);
		}
	} else {
		if (t->etr == 1) {
			/* Manual flush and stop */
			etr_writel(t, 0x1001, CXTMC_REGS_FFCR_0);
			dsb(sy);
			isb();
			etr_writel(t, 0x1041, CXTMC_REGS_FFCR_0);
			dsb(sy);
			isb();
			do {
				tmcready = etr_readl(t, CXTMC_REGS_STS_0);
				tmcready = (tmcready >> 2) & 0x1;
				if (++count > AFREADY_LOOP_MAX)
					break;
				if (!tmcready)
					mdelay(AFREADY_LOOP_MS);
			} while (!tmcready);
		} else {
			/* Manual flush and stop */
			etf_writel(t, 0x1001, CXTMC_REGS_FFCR_0);
			dsb(sy);
			isb();
			etf_writel(t, 0x1041, CXTMC_REGS_FFCR_0);
			dsb(sy);
			isb();
			do {
				tmcready = etf_readl(t, CXTMC_REGS_STS_0);
				tmcready = (tmcready >> 2) & 0x1;
				if (++count > AFREADY_LOOP_MAX)
					break;
				if (!tmcready)
					mdelay(AFREADY_LOOP_MS);
			} while (!tmcready);
		}
	}

	if (t->etr == 1) {
		/* Disable ETR */
		etr_writel(t, 0, CXTMC_REGS_CTL_0);

		/* save write pointer */
		rwp = etr_readl(t, CXTMC_REGS_RWP_0);
		rwp64 = etr_readl(t, CXTMC_REGS_RWPHI_0);
		rwp64 = (rwp64 << 32) | rwp;

		pfn = rwp64 >> PAGE_SHIFT;
		for (count = 0; count < PAGE_SIZE; count++) {
			/* pfn in scatter gather table is b[31..4] */
			if (pfn == (*(u32 *)(t->etr_ll + (count << 2)) >> 4))
				break;
		}
		count %= PAGE_SIZE;
		t->etr_buf = t->etr_address + (count << PAGE_SHIFT);
		t->etr_buf += (rwp64 & (PAGE_SIZE-1));

		 /* Get etr data and Check for overflow */
		overflow = etr_readl(t, CXTMC_REGS_STS_0);
		overflow &= 0x01;

		/* Disabling the ETM */
		if (t->enable) {
			on_each_cpu(ptm_disable, t, true);
			ape_writel(t, 0x500, CORESIGHT_APE_CPU0_ETM_ETMCR_0);
			t->enable = 0;
		}
		t->buf_size = t->etr_buf - t->etr_address;
		t->etr_buf = t->etr_address;
		if (overflow) {
			t->etr_buf += t->buf_size;
			t->buf_size = ETR_SIZE;
		}
	} else {
		/* Disable ETF */
		etf_writel(t, 0, CXTMC_REGS_CTL_0);

		/* save write pointer */
		rwp = etf_readl(t, CXTMC_REGS_RWP_0);

		/* check current status for overflow */
		overflow = etf_readl(t, CXTMC_REGS_STS_0);
		overflow &= 0x1;

		/* Disabling the ETM */
		if (t->enable) {
			on_each_cpu(ptm_disable, t, true);
			ape_writel(t, 0x500, CORESIGHT_APE_CPU0_ETM_ETMCR_0);
			t->enable = 0;
		}

		/* by default, start reading from 0 */
		t->buf_size = rwp;
		etf_writel(t, 0, CXTMC_REGS_RRP_0);

		if (overflow) {
			/* in case of overflow, the size is max ETF size */
			t->buf_size = MAX_ETF_SIZE * 4;
			/* start from RWP for read */
			etf_writel(t, rwp, CXTMC_REGS_RRP_0);
			rwp = MAX_ETF_SIZE;
		} else
			rwp >>= 2;
		/*
		 * start from either RWP in case of overflow, or 0 in case
		 * no overflow. take advantage of RRP rollover
		 */
		for (count = 0; count < rwp-1; count++) {
			t->trc_buf[count] = etf_readl(t, CXTMC_REGS_RRD_0);
		}
	}
	etf_regs_lock(t);
	etr_regs_lock(t);
	clk_disable(csite_clk);
	mutex_unlock(&t->mutex);

	return t->buf_size;
}

static int trc_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct tracectx *t = dev_get_drvdata(miscdev->parent);

	if (NULL == t->etf_regs)
		return -ENODEV;

	if (NULL == t->etr_regs)
		return -ENODEV;

	file->private_data = t;

	trc_fill_buf(t);

	return nonseekable_open(inode, file);
}

static int trc_release(struct inode *inode, struct file *file)
{
	/* there's nothing to do here, actually */
	return 0;
}

#ifdef CONFIG_CPU_PM
static int ptm_cpu_pm_notifier(struct notifier_block *self,
	unsigned long action, void *not_used)
{
	int ret = NOTIFY_OK;

	switch (action) {
	case CPU_PM_ENTER:
		if (tracer.enable)
			ptm_save(&tracer);
		break;
	case CPU_PM_EXIT:
		if (tracer.enable)
			ptm_restore(&tracer);
		break;
	default:
		ret = NOTIFY_DONE;
	}
	return ret;
}

static struct notifier_block ptm_cpu_pm_notifier_block = {
	.notifier_call = ptm_cpu_pm_notifier,
};
#endif

#ifdef CONFIG_HOTPLUG_CPU
static int ptm_cpu_hotplug_notifier(struct notifier_block *self,
	unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_STARTING:
		if (tracer.enable) {
			ptm_init(&tracer);
		}
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

/* configure per-CPU trace unit in hotplug path */
static struct notifier_block ptm_cpu_hotplug_notifier_block = {
	.notifier_call = ptm_cpu_hotplug_notifier,
};
#endif

/* use a sysfs file "trace_range_address" to allow user to
   specify specific areas of code to be traced */
static ssize_t trace_range_address_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	int i;

	/* clear buffer */
	if (tracer.trigger_mask & 0x3)
		sprintf(buf + strlen(buf),
			"start address 0 : TRIG\nstop address 0  : TRIG\n");
	else if (tracer.start_stop_mask & 0x3)
		sprintf(buf + strlen(buf),
			"start address 0 : %16lx\nstop address 0  : %16lx\n",
			tracer.start_address[0], tracer.stop_address[0]);
	else
		sprintf(buf + strlen(buf),
			"start address 0 : free\nstop address 0  : free\n");

	for (i = 1; i < ADDR_REG_PAIRS; i++) {
		if ((tracer.trigger_mask >> (i * 2)) & 0x3)
			sprintf(buf + strlen(buf),
				"start address %d : TRIG\nstop address %d  : TRIG\n",
				i, i);
		else if ((tracer.start_stop_mask >> (i*2)) & 0x3)
			sprintf(buf + strlen(buf),
				"start address %d : %16lx\nstop address %d  : %16lx\n",
				i, tracer.start_address[i],
				i, tracer.stop_address[i]);
		else
			sprintf(buf + strlen(buf),
				"start address %d : free\nstop address %d  : free\n",
				i, i);

	}

	return strlen(buf);
}

/* use a sysfs file "trace_address_trigger" to allow user to
   specify specific areas of code to be traced */
static ssize_t trace_trigger_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	int i;
	if (tracer.trigger) {
		sprintf(buf, "Address match is enabled\n");
		for (i = 0; i < ADDR_REG_PAIRS * 2; i++) {
			if ((tracer.trigger_mask >> i) & 0x1)
				sprintf(buf + strlen(buf),
					"Address trigger at %16lx\n",
					tracer.trigger_address[i]);
		}
		return strlen(buf);
	}
	else
		return sprintf(buf, "Address match is disabled\n");
}

/* use a sysfs file "trace_cycle_count" to allow/disallow cycle count
   to be captured */
static ssize_t trace_cycle_count_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%u\n", tracer.cycle_count);
}

static ssize_t trace_config_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	u32 *data = (u32 *)buf;
	struct tracectx *t = &tracer;
	u32 id = raw_smp_processor_id();
	int i = 0;

	data[i++] = ptm_readl(t, id, TRCTRACEIDR);
	data[i++] = ptm_readl(t, id, TRCCONFIGR);
	data[i++] = ptm_readl(t, id, TRCACVR0);
	data[i++] = ptm_readl(t, id, TRCACVR0 + 0x4);
	data[i++] = ptm_readl(t, id, TRCACVR1);
	data[i++] = ptm_readl(t, id, TRCACVR1 + 0x4);
	data[i++] = ptm_readl(t, id, TRCACVR2);
	data[i++] = ptm_readl(t, id, TRCACVR2 + 0x4);
	/* reserved for future use */
	data[i++] = 0x00000000;
	data[i++] = 0x00000000;
	data[i++] = 0x00000000;
	data[i++] = 0x00000000;
	data[i++] = 0x00000000;
	data[i++] = 0x00000000;
	data[i++] = 0x00000000;
	data[i++] = 0x00000000;

	return 64;
}

static ssize_t trace_config_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	return 0;
}
static ssize_t trace_enable_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	unsigned int value;
	int ret = -EAGAIN;

	if (sscanf(buf, "%u", &value) != 1)
		return -EINVAL;

	if (!tracer.etf_regs)
		return -ENODEV;

	mutex_lock(&tracer.mutex);

	if (value) {
		if (!tracer.enable)
			ret = trace_t210_start(&tracer);
		tracer.enable = 1;
	} else {
		if (tracer.enable) {
			tracer.enable = 0;
			ret = trace_t210_stop(&tracer);
		}
	}

	mutex_unlock(&tracer.mutex);

	return ret ? ret : n;
}

static ssize_t trace_range_address_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	char const *buf, size_t n)
{
	uintptr_t start_address[ADDR_REG_PAIRS], stop_address[ADDR_REG_PAIRS];
	int cnt, i;
	int addr_pair = 0;

	cnt = sscanf(buf, "%lx %lx %lx %lx %lx %lx %lx %lx",
		&start_address[0], &stop_address[0],
		&start_address[1], &stop_address[1],
		&start_address[2], &stop_address[2],
		&start_address[3], &stop_address[3]);
	if (cnt % 2)
		return -EINVAL;

	mutex_lock(&tracer.mutex);

	tracer.start_stop_mask = 0;

	for (i = 0; i < ADDR_REG_PAIRS; i++) {
		if (start_address[addr_pair] >= stop_address[addr_pair]) {
			mutex_unlock(&tracer.mutex);
			return -EINVAL;
		}

		if ((tracer.trigger_mask >> (i * 2)) & 0x3)
			continue;

		tracer.start_address[i] = start_address[addr_pair];
		tracer.stop_address[i] = stop_address[addr_pair];
		tracer.start_stop_mask += (0x3 << (i*2));
		addr_pair++;
		if (addr_pair == cnt/2) {
			i++;
			break;
		}
	}

	mutex_unlock(&tracer.mutex);

	if (addr_pair < cnt/2) {
		pr_info("Tegra PTM: Not enough address registers for request\n");
		return -EINVAL;
	}

	return n;
}

static ssize_t trace_trigger_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	unsigned int trigger = 0;
	uintptr_t trigger_address[ADDR_REG_PAIRS * 2];
	int cnt, i;
	int trig_pos = 0;

	cnt = sscanf(buf, "%u %lx %lx %lx %lx %lx %lx %lx %lx",
		&trigger,
		&trigger_address[0], &trigger_address[1],
		&trigger_address[2], &trigger_address[3],
		&trigger_address[4], &trigger_address[5],
		&trigger_address[6], &trigger_address[7]);
	if (cnt-- < 1)
		return -EINVAL;

	/* max 99% to leave room for preventing buffer overwrite */
	if (trigger >= 100)
		trigger = 99;

	mutex_lock(&tracer.mutex);
	tracer.trigger_mask = 0;

	if (!trigger || !cnt) {
		tracer.trigger = 0;
		mutex_unlock(&tracer.mutex);
		return n;
	}

	tracer.trigger = trigger;
	for (i = 0; i < ADDR_REG_PAIRS * 2; i++) {
		if ((tracer.start_stop_mask >> i) & 0x1)
			continue;

		tracer.trigger_address[i] = trigger_address[trig_pos];
		tracer.trigger_mask += (1 << i);
		trig_pos++;
		if (trig_pos == cnt)
			break;

	}

	mutex_unlock(&tracer.mutex);

	if (trig_pos != cnt) {
		pr_info("Tegra PTM: Not enough address registers for request\n");
		return -EINVAL;
	}

	return n;
}

static ssize_t trace_cycle_count_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	unsigned int cycle_count;

	if (sscanf(buf, "%u", &cycle_count) != 1)
		return -EINVAL;

	mutex_lock(&tracer.mutex);

	tracer.cycle_count = cycle_count;

	mutex_unlock(&tracer.mutex);

	return n;
}

static void tmc_build_sg_table(void)
{
	unsigned long pfn;
	void *virt;
	u32 index;
	u32 *sg_ptr;

	virt = tracer.etr_address;
	sg_ptr = (u32 *)tracer.etr_ll;
	for (index = 0; index < PAGE_SIZE; index++) {/* 4k entries in total */
		pfn = vmalloc_to_pfn(virt);
		virt += PAGE_SIZE;
		*sg_ptr++ = (pfn << 4) | 0x2;
	}
	*(--sg_ptr) = (pfn << 4) | 0x1;
	__flush_dcache_area((void *)tracer.etr_ll, PAGE_SIZE << 2);
}

static ssize_t trace_etr_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	unsigned int etr_enable;

	if (sscanf(buf, "%u", &etr_enable) != 1)
		return -EINVAL;
	tracer.etr = etr_enable ? 1 : 0;
	if (tracer.etr_address && !tracer.etr) {
		vfree(tracer.etr_address);
		tracer.etr_address = NULL;
		free_pages(tracer.etr_ll, 2);
		tracer.etr_ll = 0;
	} else if (tracer.etr_address) {
		BUG_ON(!tracer.etr);
		return n; /* enabling, but already enabled */
	} else if (tracer.etr) {
		tracer.etr_address = vmalloc(ETR_SIZE);
		if (tracer.etr_address) {
			/*
			 * each page holds entries for 1K pages, i.e. 4MB data
			 * so 16MB needs 4 pages, hence order 2
			 */
			tracer.etr_ll = __get_free_pages(GFP_KERNEL, 2);
			if (!tracer.etr_ll) {
				vfree(tracer.etr_address);
				tracer.etr = 0;
			} else
				tmc_build_sg_table();
		} else
			tracer.etr = 0;
	}
	return n;
}

#define define_show_state_func(_name) \
static ssize_t trace_##_name##_show(struct kobject *kobj, \
				struct kobj_attribute *attr, \
				char *buf) \
{ \
	return sprintf(buf, "%lu\n", tracer._name); \
}

define_show_state_func(enable)
define_show_state_func(userspace)
define_show_state_func(branch_broadcast)
define_show_state_func(return_stack)
define_show_state_func(timestamp)
define_show_state_func(formatter)
define_show_state_func(etr)
define_show_state_func(ape)

#define define_store_state_func(_name) \
static ssize_t trace_##_name##_store(struct kobject *kboj, \
				struct kobj_attribute *attr, \
				const char *buf, size_t n) \
{ \
	unsigned long value; \
	if (kstrtoul(buf, 0, &value)) \
		return -EINVAL; \
	mutex_lock(&tracer.mutex); \
	tracer._name = (value) ? 1 : 0; \
	mutex_unlock(&tracer.mutex); \
	return n; \
}

define_store_state_func(userspace)
define_store_state_func(branch_broadcast)
define_store_state_func(return_stack)
define_store_state_func(timestamp)
define_store_state_func(formatter)
define_store_state_func(ape)

#define A(a, b, c, d)   __ATTR(trace_##a, b, \
	trace_##c##_show, trace_##d##_store)
static const struct kobj_attribute trace_attr[] = {
	A(enable,		0644,	enable,		enable),
	A(config,		0444,	config,		config),
	A(userspace,		0644,	userspace,	userspace),
	A(branch_broadcast,	0644,	branch_broadcast, branch_broadcast),
	A(return_stack,		0644,	return_stack, return_stack),
	A(range_address,	0644,	range_address, range_address),
	A(trigger,		0644,	trigger,	trigger),
	A(timestamp,		0644,	timestamp,	timestamp),
	A(cycle_count,		0644,	cycle_count,	cycle_count),
	A(formatter,		0644,	formatter,	formatter),
	A(etr,			0644,	etr,		etr),
	A(ape,			0644,	ape,		ape)
};

static const struct file_operations trc_fops = {
	.owner = THIS_MODULE,
	.read = trc_read,
	.open = trc_open,
	.release = trc_release,
	.llseek = no_llseek,
};

static struct miscdevice trc_miscdev = {
	.name = "trc",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &trc_fops,
};

static struct miscdevice last_trc_miscdev = {
	.name = "last_trc",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &trc_fops,
};

static void trc_nodes(struct tracectx *t)
{
	int ret = 0;

	trc_miscdev.parent = t->dev;
	ret = misc_register(&trc_miscdev);
	if (ret)
		dev_err(t->dev, "failes to register /dev/trc\n");

	last_trc_miscdev.parent = t->dev;
	ret = misc_register(&last_trc_miscdev);
	if (ret)
		dev_err(t->dev, "failes to register /dev/last_trc\n");

	dev_dbg(t->dev, "Trace nodes are initialized.\n");
}

static int ptm_probe(struct platform_device  *dev)
{
	int i, ptm_cnt = 0, ret = 0;

	struct tracectx *t = &tracer;

	t->dev = &dev->dev;
	platform_set_drvdata(dev, t);

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *res;
		void __iomem *addr;

		res = platform_get_resource(dev, IORESOURCE_MEM, i);
		if (NULL == res)
			goto out;

		addr = devm_ioremap_nocache(&dev->dev, res->start,
				resource_size(res));
		if (!addr || !dev->dev.of_node)
			goto out;

		switch (i) {
		case 0:
			t->funnel_major_regs = addr;
			break;
		case 1:
			t->etf_regs = addr;
			break;
		case 2:
			t->replicator_regs = addr;
			break;
		case 3:
			t->etr_regs = addr;
			break;
		case 4:
			t->tpiu_regs = addr;
			break;
		case 5:
			t->funnel_bccplex_regs = addr;
			break;
		case 6:
		case 7:
		case 8:
		case 9:
			if (ptm_cnt < CONFIG_NR_CPUS)
				t->ptm_regs[ptm_cnt++] = addr;
			break;
		case 10:
			t->funnel_minor_regs = addr;
			break;
		case 11:
			t->ape_regs = addr;
			break;
		default:
			dev_err(&dev->dev,
				"unknown resource for the PTM device\n");
			break;
		}
	}

	WARN_ON(ptm_cnt < num_possible_cpus());

	/* Configure Coresight Clocks */
	csite_clk = clk_get_sys("csite", NULL);

	/* Initialise data structures required for saving trace after reset */
	trc_buf_init(t);

	/* save ETF contents to DRAM when system is reset */
	trc_fill_buf(t);

	/* initialising dev nodes */
	trc_nodes(t);

	/* create sysfs */
	for (i = 0; i < ARRAY_SIZE(trace_attr); i++) {
		ret = sysfs_create_file(&dev->dev.kobj, &trace_attr[i].attr);
		if (ret)
			dev_err(&dev->dev, "failed to create %s\n",
				trace_attr[i].attr.name);
	}

#ifdef CONFIG_CPU_PM
	cpu_pm_register_notifier(&ptm_cpu_pm_notifier_block);
#endif
#ifdef CONFIG_HOTPLUG_CPU
	register_cpu_notifier(&ptm_cpu_hotplug_notifier_block);
#endif

	dev_dbg(&dev->dev, "PTM driver initialized.\n");

out:
	if (ret)
		dev_err(&dev->dev, "Failed to start the PTM device\n");
	return ret;
}

static int ptm_remove(struct platform_device *dev)
{
	struct tracectx *t = &tracer;
	int i;

#ifdef CONFIG_CPU_PM
	cpu_pm_unregister_notifier(&ptm_cpu_pm_notifier_block);
#endif
#ifdef CONFIG_HOTPLUG_CPU
	unregister_cpu_notifier(&ptm_cpu_hotplug_notifier_block);
#endif

	for (i = 0; i < ARRAY_SIZE(trace_attr); i++)
		sysfs_remove_file(&dev->dev.kobj, &trace_attr[i].attr);

	mutex_lock(&t->mutex);

	for (i = 0; i < CONFIG_NR_CPUS; i++)
		devm_iounmap(&dev->dev, t->ptm_regs[i]);
	devm_iounmap(&dev->dev, t->funnel_major_regs);
	devm_iounmap(&dev->dev, t->funnel_minor_regs);
	devm_iounmap(&dev->dev, t->ape_regs);
	devm_iounmap(&dev->dev, t->etr_regs);
	devm_iounmap(&dev->dev, t->tpiu_regs);
	devm_iounmap(&dev->dev, t->funnel_bccplex_regs);
	devm_iounmap(&dev->dev, t->replicator_regs);
	t->etf_regs = NULL;

	mutex_unlock(&t->mutex);

	return 0;
}

static struct of_device_id ptm_of_match[] = {
	    { .compatible = "nvidia,ptm", },
	    {   },
};

MODULE_DEVICE_TABLE(of, ptm_of_match)

static struct platform_driver ptm_driver = {
	.probe          = ptm_probe,
	.remove         = ptm_remove,
	.driver         = {
		.name   = "ptm",
		.of_match_table = of_match_ptr(ptm_of_match),
		.owner  = THIS_MODULE,
	},
};

static int __init tegra_ptm_driver_init(void)
{
	int retval;

	mutex_init(&tracer.mutex);
	retval = platform_driver_register(&ptm_driver);
	if (retval) {
		pr_err("Failed to probe ptm\n");
		return retval;
	}

	return 0;
}
device_initcall(tegra_ptm_driver_init);
