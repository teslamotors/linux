/*
 * arch/arm/mach-tegra/tegra_ptm.h
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

#ifndef __MACH_TEGRA_PTM_H
#define __MACH_TEGRA_PTM_H

#define TRACER_ACCESSED		BIT(0)
#define TRACER_RUNNING		BIT(1)
#define TRACER_CYCLE_ACC	BIT(2)
#define TRACER_TRACE_DATA	BIT(3)
#define TRACER_TIMESTAMP	BIT(4)
#define TRACER_BRANCHOUTPUT	BIT(5)
#define TRACER_RETURN_STACK	BIT(6)

#define TRACER_TIMEOUT 10000

/*
 * CoreSight Management Register Offsets:
 * These registers offsets are same for both ETB, PTM, FUNNEL and TPIU
 */
#define CORESIGHT_LOCKACCESS	0xfb0
#define CORESIGHT_LOCKSTATUS	0xfb4
#define CORESIGHT_AUTHSTATUS	0xfb8
#define CORESIGHT_DEVID		0xfc8
#define CORESIGHT_DEVTYPE	0xfcc

/* PTM control register */
#define PTM_CTRL			0x0
#define PTM_CTRL_POWERDOWN		(1 << 0)
#define PTM_CTRL_PROGRAM		(1 << 10)
#define PTM_CTRL_CONTEXTIDSIZE(x)	(((x) & 3) << 14)
#define PTM_CTRL_STALLPROCESSOR		(1 << 7)
#define PTM_CTRL_BRANCH_OUTPUT		(1 << 8)
#define PTM_CTRL_CYCLEACCURATE		(1 << 12)
#define PTM_CTRL_TIMESTAMP_EN		(1 << 28)
#define PTM_CTRL_RETURN_STACK_EN	(1 << 29)

/* PTM configuration code register */
#define PTM_CONFCODE		(0x04)

/* PTM trigger event register */
#define PTM_TRIGEVT		(0x08)

/* address access type register bits, "PTM architecture",
 * table 3-27 */
/* - access type */
#define PTM_COMP_ACC_TYPE(x)		(0x80 + (x) * 4)
/* this is a read only bit */
#define PTM_ACC_TYPE_INSTR_ONLY		(1 << 0)
/* - context id comparator control */
#define PTM_ACC_TYPE_IGN_CONTEXTID	(0 << 8)
#define PTM_ACC_TYPE_CONTEXTID_CMP1	(1 << 8)
#define PTM_ACC_TYPE_CONTEXTID_CMP2	(2 << 8)
#define PTM_ACC_TYPE_CONTEXTID_CMP3	(3 << 8)
/* - security level control */
#define PTM_ACC_TYPE_PFT10_IGN_SECURITY	(0 << 10)
#define PTM_ACC_TYPE_PFT10_NS_ONLY	(1 << 10)
#define PTM_ACC_TYPE_PFT10_S_ONLY	(2 << 10)
#define PTM_ACC_TYPE_PFT11_NS_CTRL(x)	(((x & 1) << 11) | ((x & 2) << 13))
#define PTM_ACC_TYPE_PFT11_S_CTRL(x)	(((x & 1) << 10) | ((x & 2) << 12))
#define PTM_ACC_TYPE_PFT11_MATCH_ALL		0
#define PTM_ACC_TYPE_PFT11_MATCH_NONE		1
#define PTM_ACC_TYPE_PFT11_MATCH_PRIVILEGE	2
#define PTM_ACC_TYPE_PFT11_MATCH_USER		3
/*
 * If secureity extension is not implemented, the state mode is same as
 * secure state under security extension.
 */
#define PTM_ACC_TYPE_PFT11_NO_S_EXT_CTRL(x) PTM_ACC_TYPE_PFT11_S_CTRL(x)

#define PTM_COMP_VAL(x)			(0x40 + (x) * 4)

/* PTM status register */
#define PTM_STATUS			0x10
#define PTM_STATUS_OVERFLOW		BIT(0)
#define PTM_STATUS_PROGBIT		BIT(1)
#define PTM_STATUS_STARTSTOP		BIT(2)
#define PTM_STATUS_TRIGGER		BIT(3)
#define ptm_progbit(t, id) (ptm_readl((t), id, PTM_STATUS) & PTM_STATUS_PROGBIT)
#define ptm_started(t) (ptm_readl((t), PTM_STATUS) & PTM_STATUS_STARTSTOP)
#define ptm_triggered(t) (ptm_readl((t), PTM_STATUS) & PTM_STATUS_TRIGGER)

/* PTM trace start/stop resource control register */
#define PTM_START_STOP_CTRL	(0x18)

/* PTM trace enable control */
#define PTM_TRACE_ENABLE_CTRL1			0x24
#define PTM_TRACE_ENABLE_EXCL_CTRL		BIT(24)
#define PTM_TRACE_ENABLE_INCL_CTRL		0
#define PTM_TRACE_ENABLE_CTRL1_START_STOP_CTRL	BIT(25)

/* PTM trace enable event configuration */
#define PTM_TRACE_ENABLE_EVENT		0x20
/* PTM event resource */
#define COUNTER0			((0x4 << 4) | 0)
#define COUNTER1			((0x4 << 4) | 1)
#define COUNTER2			((0x4 << 4) | 2)
#define COUNTER3			((0x4 << 4) | 3)
#define EXT_IN0				((0x6 << 4) | 0)
#define EXT_IN1				((0x6 << 4) | 1)
#define EXT_IN2				((0x6 << 4) | 2)
#define EXT_IN3				((0x6 << 4) | 3)
#define EXTN_EXT_IN0			((0x6 << 4) | 8)
#define EXTN_EXT_IN1			((0x6 << 4) | 9)
#define EXTN_EXT_IN2			((0x6 << 4) | 10)
#define EXTN_EXT_IN3			((0x6 << 4) | 11)
#define IN_NS_STATE			((0x6 << 4) | 13)
#define TRACE_PROHIBITED		((0x6 << 4) | 14)
#define ALWAYS_TRUE			((0x6 << 4) | 15)
#define	LOGIC_A				(0x0 << 14)
#define	LOGIC_NOT_A			(0x1 << 14)
#define	LOGIC_A_AND_B			(0x2 << 14)
#define	LOGIC_NOT_A_AND_B		(0x3 << 14)
#define	LOGIC_NOT_A_AND_NOT_B		(0x4 << 14)
#define	LOGIC_A_OR_B			(0x5 << 14)
#define	LOGIC_NOT_A_OR_B		(0x6 << 14)
#define	LOGIC_NOT_A_OR_NOT_B		(0x7 << 14)
#define SET_RES_A(RES)			((RES) << 0)
#define SET_RES_B(RES)			((RES) << 7)
#define DEF_PTM_EVENT(LOGIC, B, A)	((LOGIC) | SET_RES_B(B) | SET_RES_A(A))

#define PTM_SYNC_FREQ		0x1e0

#define PTM_ID			0x1e4
#define PTMIDR_VERSION(x)	(((x) >> 4) & 0xff)
#define PTMIDR_VERSION_3_1	0x21
#define PTMIDR_VERSION_PFT_1_0	0x30
#define PTMIDR_VERSION_PFT_1_1	0x31

/* PTM configuration register */
#define PTM_CCE			0x1e8
#define PTMCCER_RETURN_STACK_IMPLEMENTED	BIT(23)
#define PTMCCER_TIMESTAMPING_IMPLEMENTED	BIT(22)
/* PTM management registers */
#define PTMMR_OSLAR		0x300
#define PTMMR_OSLSR		0x304
#define PTMMR_OSSRR		0x308
#define PTMMR_PDSR		0x314
/* PTM sequencer registers */
#define PTMSQ12EVR		0x180
#define PTMSQ21EVR		0x184
#define PTMSQ23EVR		0x188
#define PTMSQ31EVR		0x18C
#define PTMSQ32EVR		0x190
#define PTMSQ13EVR		0x194
#define PTMSQR			0x19C
/* PTM counter event */
#define PTMCNTRLDVR(n)		((0x50 + (n)) << 2)
#define PTMCNTENR(n)		((0x54 + (n)) << 2)
#define PTMCNTRLDEVR(n)		((0x58 + (n)) << 2)
#define PTMCNTVR(n)		((0x5c + (n)) << 2)
/* PTM timestamp event */
#define PTMTSEVR		0x1F8
/* PTM ATID registers */
#define PTM_TRACEIDR		0x200

/* ETB registers, "CoreSight Components TRM", 9.3 */
#define ETB_DEPTH		0x04
#define ETB_STATUS		0x0c
#define ETB_STATUS_FULL		BIT(0)
#define ETB_READMEM		0x10
#define ETB_READADDR		0x14
#define ETB_WRITEADDR		0x18
#define ETB_TRIGGERCOUNT	0x1c
#define ETB_CTRL		0x20
#define TRACE_CAPATURE_ENABLE	0x1
#define TRACE_CAPATURE_DISABLE	0x0
#define ETB_RWD			0x24
#define ETB_FF_CTRL		0x304
#define ETB_FF_CTRL_ENFTC	BIT(0)
#define ETB_FF_CTRL_ENFCONT	BIT(1)
#define ETB_FF_CTRL_FONFLIN	BIT(4)
#define ETB_FF_CTRL_MANUAL_FLUSH BIT(6)
#define ETB_FF_CTRL_TRIGIN	BIT(8)
#define ETB_FF_CTRL_TRIGEVT	BIT(9)
#define ETB_FF_CTRL_TRIGFL	BIT(10)
#define ETB_FF_CTRL_STOPFL	BIT(12)

#define FUNNEL_CTRL		0x0
#define FUNNEL_CTRL_CPU0	BIT(0)
#define FUNNEL_CTRL_CPU1	BIT(1)
#define FUNNEL_CTRL_CPU2	BIT(2)
#define FUNNEL_CTRL_ITM		BIT(3)
#define FUNNEL_CTRL_CPU3	BIT(4)
#define FUNNEL_MINIMUM_HOLD_TIME(x) ((x) << 8)
#define FUNNEL_PRIORITY		0x4
#define FUNNEL_PRIORITY_CPU0(p) ((p & 0x7)  << 0)
#define FUNNEL_PRIORITY_CPU1(p) ((p & 0x7)  << 3)
#define FUNNEL_PRIORITY_CPU2(p) ((p & 0x7)  << 6)
#define FUNNEL_PRIORITY_ITM(p)	((p & 0x7)  << 9)
#define FUNNEL_PRIORITY_CPU3(p) ((p & 0x7)  << 12)

#define TPIU_ITCTRL		0xf00
#define TPIU_FF_CTRL		0x304
#define TPIU_FF_CTRL_ENFTC	BIT(0)
#define TPIU_FF_CTRL_ENFCONT	BIT(1)
#define TPIU_FF_CTRL_FONFLIN	BIT(4)
#define TPIU_FF_CTRL_MANUAL_FLUSH BIT(6)
#define TPIU_FF_CTRL_TRIGIN	BIT(8)
#define TPIU_FF_CTRL_TRIGEVT	BIT(9)
#define TPIU_FF_CTRL_TRIGFL	BIT(10)
#define TPIU_FF_CTRL_STOPFL	BIT(12)
#define TPIU_ITATBCTR2		0xef0
#define TPIU_IME		1
#define INTEGRATION_ATREADY	1

#define CLK_TPIU_OUT_ENB_U			0x018
#define CLK_TPIU_OUT_ENB_U_TRACKCLK_IN		(0x1 << 13)
#define CLK_TPIU_SRC_TRACECLKIN			0x634
#define CLK_TPIU_SRC_TRACECLKIN_SRC_MASK	(0x7 << 29)
#define CLK_TPIU_SRC_TRACECLKIN_PLLP		(0x0 << 29)

#define LOCK_MAGIC		0xc5acce55

/* Clock Registers */
#define CLK_RST_CONTROLLER_CLK_SOURCE_CSITE_0 0x1d4
#define CLK_RST_CONTROLLER_CLK_CPUG_MISC2_0 0x714
#define CLK_RST_CONTROLLER_CLK_CPUG_MISC_0 0x540

/* Funnel major Registers */
#define CORESIGHT_ATBFUNNEL_HUGO_MAJOR_CXATBFUNNEL_REGS_CTRL_REG_0 0x0

/* Funnel Minor registers */
#define CORESIGHT_ATBFUNNEL_MINOR_CXATBFUNNEL_REGS_CTRL_REG_0 0x0

/* TMC Registers */
#define CXTMC_REGS_RSZ_0 0x4
#define CXTMC_REGS_STS_0 0x0c
#define CXTMC_REGS_RRD_0 0x10
#define CXTMC_REGS_RRP_0 0x14
#define CXTMC_REGS_RWP_0 0x18
#define CXTMC_REGS_TRG_0 0x1c
#define CXTMC_REGS_CTL_0 0x20
#define CXTMC_REGS_MODE_0 0x28
#define CXTMC_REGS_BUFWM_0 0x34
#define CXTMC_REGS_RWPHI_0 0x3c

#define CXTMC_REGS_AXICTL_0 0x110
#define CXTMC_REGS_DBALO_0 0x0118
#define CXTMC_REGS_DBAHI_0 0x011c
#define CXTMC_REGS_FFCR_0 0x304
#define CXTMC_REGS_PSCR_0 0x308

#define MAX_ETF_SIZE 0x1000

/* Replicator Registers */
#define CORESIGHT_ATBREPLICATOR_HUGO_CXATBREPLICATOR_REGISTERS_IDFILTER1_0 0x4

/* Funnel BCCPLEX Registers */
#define CORESIGHT_ATBFUNNEL_BCCPLEX_CXATBFUNNEL_REGS_CTRL_REG_0 0x0

/* CPU Debug Registers */
#define CORESIGHT_BCCPLEX_CPU_DEBUG_EDPCSRHI_0 0xac
#define CORESIGHT_BCCPLEX_CPU_DEBUG_EDPCSRLO_0 0xa0
#define CORESIGHT_BCCPLEX_CPU_DEBUG_OSLAR_EL1_0 0x300

/* PTM Rgisters */
#define TRCPRGCTLR    0x4
#define TRCPROCSELR   0x8
#define TRCSTAT       0xc
#define TRCCONFIGR    0x10
#define TRCEVENTCTL0R 0x20
#define TRCEVENTCTL1R 0x24
#define TRCSTALLCTLR  0x2c
#define TRCTSCTLR     0x30
#define TRCSYNCPR     0x34
#define TRCCCCTLR     0x38
#define TRCBBCTLR     0x3c
#define TRCTRACEIDR   0x40
#define TRCQCTLR      0x44
#define TRCVICTLR     0x80
#define TRCVIIECTLR   0x84
#define TRCVISSCTLR   0x88
#define TRCVIPCSSCTLR 0x88
#define TRCSEQEVR0    0x100
#define TRCSEQEVR1    0x104
#define TRCSEQEVR2    0x108
#define TRCSEQRSTEVR  0x118
#define TRCSEQSTR     0x11c
#define TRCEXTINSELR  0x120
#define TRCCNTRLDVR0  0x140
#define TRCCNTRLDVR1  0x144
#define TRCCNTCTLR0   0x150
#define TRCCNTCTLR1   0x154
#define TRCCNTVR0     0x160
#define TRCCNTVR1     0x164
#define TRCIDR3       0x1ec
#define TRCRSCTLR2    0x208
#define TRCRSCTLR3    0x20c
#define TRCRSCTLR4    0x210
#define TRCRSCTLR5    0x214
#define TRCRSCTLR6    0x218
#define TRCRSCTLR7    0x21c
#define TRCRSCTLR8    0x220
#define TRCRSCTLR9    0x224
#define TRCRSCTLR10   0x228
#define TRCRSCTLR11   0x22c
#define TRCRSCTLR12   0x230
#define TRCRSCTLR13   0x234
#define TRCRSCTLR14   0x238
#define TRCRSCTLR15   0x23c
#define TRCSSCCR0     0x280
#define TRCSSCSR0     0x2a0
#define TRCOSLAR      0x300
#define TRCPDCR       0x310
#define TRCPDSR       0x314
#define TRCACVR0      0x400
#define TRCACVR1      0x408
#define TRCACVR2      0x410
#define TRCACVR3      0x418
#define TRCACVR4      0x420
#define TRCACVR5      0x428
#define TRCACVR6      0x430
#define TRCACVR7      0x438
#define TRCACATR0     0x480
#define TRCACATR1     0x488
#define TRCACATR2     0x490
#define TRCACATR3     0x498
#define TRCACATR4     0x4a0
#define TRCACATR5     0x4a8
#define TRCACATR6     0x4b0
#define TRCACATR7     0x4b8
#define TRCCIDCVR0    0x600
#define TRCVMIDCVR0   0x640
#define TRCCIDCCTLR0  0x680
#define TRCCLAIMSET   0xfa0
#define TRCCLAIMCLR   0xfa4
#define BCCPLEX_BASE_ID 0x40
#define LCCPLEX_BASE_ID 0x30

/* APE registers */
#define CORESIGHT_APE_CPU0_ETM_ETMCR_0 0x0
#define CORESIGHT_APE_CPU0_ETM_ETMTECR1_0 0x24
#define CORESIGHT_APE_CPU0_ETM_ETMTEEVR_0 0x20
#define CORESIGHT_APE_CPU0_ETM_ETMACVR1_0 0x40
#define CORESIGHT_APE_CPU0_ETM_ETMACVR2_0 0x44
#define CORESIGHT_APE_CPU0_ETM_ETMACTR1_0 0x80
#define CORESIGHT_APE_CPU0_ETM_ETMACTR2_0 0x84
#define CORESIGHT_APE_CPU0_ETM_ETMATID_0 0x200

/* T210 Macro Definitions */
#define funnel_major_writel(t, v, x)	writel((v),	\
				(t)->funnel_major_regs + (x))
#define funnel_major_readl(t, x)	readl(		\
				(t)->funnel_major_regs + (x))
#define funnel_major_regs_unlock(t)	funnel_major_writel((t), \
				LOCK_MAGIC, CORESIGHT_LOCKACCESS);

#define etf_writel(t, v, x) writel((v), (t)->etf_regs + (x))
#define etf_readl(t, x)     readl((t)->etf_regs + (x))
#define etf_regs_unlock(t)	etf_writel((t), LOCK_MAGIC,	\
					CORESIGHT_LOCKACCESS);
#define etf_regs_lock(t)	etf_writel((t), 0,		\
					CORESIGHT_LOCKACCESS)

#define etr_writel(t, v, x)	writel((v),     \
				(t)->etr_regs + (x))
#define etr_readl(t, x)         readl(          \
				(t)->etr_regs + (x))
#define etr_writeq(t, v, x)	writeq((v),     \
				(t)->etr_regs + (x))
#define etr_regs_unlock(t)	etr_writel((t), LOCK_MAGIC,     \
					CORESIGHT_LOCKACCESS);
#define etr_regs_lock(t)	etr_writel((t), 0,              \
					CORESIGHT_LOCKACCESS)

#define funnel_minor_writel(t, v, x)	writel((v),	\
				(t)->funnel_minor_regs + (x))
#define funnel_minor_readl(t, x)	readl(		\
				(t)->funnel_minor_regs + (x))
#define funnel_minor_regs_unlock(t)	funnel_minor_writel((t),\
				LOCK_MAGIC, CORESIGHT_LOCKACCESS);

#define ape_writel(t, v, x)		writel((v),	\
				(t)->ape_regs + (x))
#define ape_readl(t, x)			readl(		\
				(t)->ape_regs + (x))
#define ape_regs_unlock(t)	ape_writel((t), LOCK_MAGIC,	\
					CORESIGHT_LOCKACCESS);

#define replicator_writel(t, v, x)	writel((v),	\
						(t)->replicator_regs + (x))
#define replicator_readl(t, x)		readl(		\
						(t)->replicator_regs + (x))
#define replicator_regs_unlock(t)	replicator_writel((t),	\
					LOCK_MAGIC, CORESIGHT_LOCKACCESS);

#define funnel_bccplex_writel(t, v, x)	writel((v),	\
					(t)->funnel_bccplex_regs + (x))
#define funnel_bccplex_readl(t, x)	readl(		\
					(t)->funnel_bccplex_regs + (x))
#define funnel_bccplex_regs_unlock(t)	funnel_bccplex_writel((t),	\
					LOCK_MAGIC, CORESIGHT_LOCKACCESS);

#define cpu_debug_writel(t, id, v, x)	writel((v),	\
						(t)->cpu_regs[id] + (x))
#define cpu_debug_regs_unlock(t, id)	cpu_debug_writel((t), id,	\
					LOCK_MAGIC, CORESIGHT_LOCKACCESS);
#define cpu_debug_readl(t, id, x)	readl((t)->cpu_regs[id] + (x))


#define etb_writel(t, v, x)	writel((v), (t)->etb_regs + (x))
#define etb_readl(t, x)		readl((t)->etb_regs + (x))
#define etb_regs_lock(t)	etb_writel((t), 0,		\
							CORESIGHT_LOCKACCESS)
#define etb_regs_unlock(t)	etb_writel((t), LOCK_MAGIC,	\
						CORESIGHT_LOCKACCESS)

#define funnel_writel(t, v, x)	writel((v),		\
							(t)->funnel_regs + (x))
#define  funnel_readl(t, x)	readl((t)->funnel_regs + (x))
#define  funnel_regs_lock(t)	funnel_writel((t), 0,		\
							CORESIGHT_LOCKACCESS)
#define  funnel_regs_unlock(t)	funnel_writel((t), LOCK_MAGIC,	\
						CORESIGHT_LOCKACCESS)

#define  tpiu_writel(t, v, x)	writel((v), (t)->tpiu_regs + (x))
#define  tpiu_readl(t, x)	readl((t)->tpiu_regs + (x))
#define  tpiu_regs_lock(t)	tpiu_writel((t), 0,		\
					CORESIGHT_LOCKACCESS)
#define  tpiu_regs_unlock(t)	tpiu_writel((t), LOCK_MAGIC,	\
						CORESIGHT_LOCKACCESS)

#define  ptm_writel(t, id, v, x) writel((v), (t)->ptm_regs[(id)] + (x))
#define  ptm_readl(t, id, x)	readl((t)->ptm_regs[(id)] + (x))
#define  ptm_writeq(t, id, v, x) writeq((v), (t)->ptm_regs[id] + (x))
#define  ptm_readq(t, id, x)     readq((t)->ptm_regs[id] + (x))
#define  ptm_regs_lock(t, id)	ptm_writel((t), (id), 0,	\
						CORESIGHT_LOCKACCESS);
#define  ptm_regs_unlock(t, id)	ptm_writel((t), (id),		\
			LOCK_MAGIC, CORESIGHT_LOCKACCESS);
#define  ptm_os_lock(t, id)	ptm_writel((t), (id),		\
						LOCK_MAGIC, PTMMR_OSLAR)
#define  ptm_os_unlock(t, id)	ptm_writel((t), (id), 0, PTMMR_OSLAR)

#ifdef CONFIG_TEGRA_PTM
/* PTM required re-energized CPU enterring LP2 mode */
void ptm_power_idle_resume(int cpu);
#else
static inline void ptm_power_idle_resume(int cpu) {}
#endif

#endif
