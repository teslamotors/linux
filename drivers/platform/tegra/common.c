/*
 * drivers/platform/tegra/common.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010-2016 NVIDIA Corporation. All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
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

#include <linux/platform_device.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/clk-provider.h>
#include <linux/highmem.h>
#include <linux/memblock.h>
#include <linux/bitops.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/pstore_ram.h>
#include <linux/dma-mapping.h>
#include <linux/sys_soc.h>
#include <linux/tegra-pmc.h>
#if defined(CONFIG_SMSC911X)
#include <linux/smsc911x.h>
#endif
#include <linux/pm.h>
#include <linux/tegra-powergate.h>

#include <linux/export.h>
#include <linux/bootmem.h>
#include <linux/tegra-soc.h>
#include <linux/dma-contiguous.h>
#include <linux/tegra-fuse.h>
#include <linux/gk20a.h>
#include <linux/tegra_smmu.h>
#include <linux/tegra_pm_domains.h>

#ifdef CONFIG_ARM64
#include <linux/irqchip/arm-gic.h>
#else
#include <asm/system.h>
#include <asm/hardware/cache-l2x0.h>
#endif
#include <asm/dma-mapping.h>
#include <asm/dma-contiguous.h>

#include <mach/nct.h>
#include <mach/dc.h>

#include "apbio.h"
#include "board.h"
#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/common.h>
#include <linux/platform/tegra/dvfs.h>
#include "iomap.h"
#include "pm.h"
#include "sleep.h"
#include <linux/platform/tegra/reset.h>
#include "devices.h"

#define MC_SECURITY_CFG2	0x7c

#define AHB_ARBITRATION_PRIORITY_CTRL		0x4
#define   AHB_PRIORITY_WEIGHT(x)	(((x) & 0x7) << 29)
#define   PRIORITY_SELECT_USB	BIT(6)
#define   PRIORITY_SELECT_USB2	BIT(18)
#define   PRIORITY_SELECT_USB3	BIT(17)
#define   PRIORITY_SELECT_SE BIT(14)

#define AHB_GIZMO_AHB_MEM		0xc
#define   ENB_FAST_REARBITRATE	BIT(2)
#define   DONT_SPLIT_AHB_WR     BIT(7)
#define   WR_WAIT_COMMIT_ON_1K	BIT(8)
#define   EN_USB_WAIT_COMMIT_ON_1K_STALL	BIT(9)

#define   RECOVERY_MODE	BIT(31)
#define   BOOTLOADER_MODE	BIT(30)
#define   FORCED_RECOVERY_MODE	BIT(1)

#define AHB_GIZMO_USB		0x1c
#define AHB_GIZMO_USB2		0x78
#define AHB_GIZMO_USB3		0x7c
#define AHB_GIZMO_SE		0x4c
#define   IMMEDIATE	BIT(18)

#define AHB_MEM_PREFETCH_CFG5	0xc8
#define AHB_MEM_PREFETCH_CFG3	0xe0
#define AHB_MEM_PREFETCH_CFG4	0xe4
#define AHB_MEM_PREFETCH_CFG1	0xec
#define AHB_MEM_PREFETCH_CFG2	0xf0
#define AHB_MEM_PREFETCH_CFG6	0xcc
#define   PREFETCH_ENB	BIT(31)
#define   MST_ID(x)	(((x) & 0x1f) << 26)
#define   AHBDMA_MST_ID	MST_ID(5)
#define   USB_MST_ID	MST_ID(6)
#define SDMMC4_MST_ID	MST_ID(12)
#define   USB2_MST_ID	MST_ID(18)
#define   USB3_MST_ID	MST_ID(17)
#define   SE_MST_ID	MST_ID(14)
#define   ADDR_BNDRY(x)	(((x) & 0xf) << 21)
#define   INACTIVITY_TIMEOUT(x)	(((x) & 0xffff) << 0)

#ifdef CONFIG_PSTORE_RAM
#define RAMOOPS_MEM_SIZE SZ_2M
#define RECORD_MEM_SIZE SZ_64K
#define CONSOLE_MEM_SIZE SZ_512K
#define FTRACE_MEM_SIZE SZ_512K
#define RTRACE_MEM_SIZE SZ_512K
#endif

#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
static struct of_device_id tegra_sata_pd[] = {
	{ .compatible = "nvidia,tegra186-sata-pd", },
	{ .compatible = "nvidia,tegra210-sata-pd", },
	{ .compatible = "nvidia,tegra132-sata-pd", },
	{},
};
#endif

#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
static struct of_device_id tegra_pcie_pd[] = {
	{ .compatible = "nvidia,tegra186-pcie-pd", },
	{ .compatible = "nvidia,tegra210-pcie-pd", },
	{ .compatible = "nvidia,tegra132-pcie-pd", },
	{},
};
#endif

#ifdef CONFIG_TEGRA_XUSB_PLATFORM
static struct of_device_id tegra_xusba_pd[] = {
	{ .compatible = "nvidia,tegra186-xusba-pd", },
	{ .compatible = "nvidia,tegra210-xusba-pd", },
	{ .compatible = "nvidia,tegra132-xusba-pd", },
	{},
};

static struct of_device_id tegra_xusbb_pd[] = {
	{ .compatible = "nvidia,tegra186-xusbb-pd", },
	{ .compatible = "nvidia,tegra210-xusbb-pd", },
	{ .compatible = "nvidia,tegra132-xusbb-pd", },
	{},
};

static struct of_device_id tegra_xusbc_pd[] = {
	{ .compatible = "nvidia,tegra186-xusbc-pd", },
	{ .compatible = "nvidia,tegra210-xusbc-pd", },
	{ .compatible = "nvidia,tegra132-xusbc-pd", },
	{},
};
#endif

phys_addr_t tegra_bootloader_fb_start;
phys_addr_t tegra_bootloader_fb_size;
phys_addr_t tegra_bootloader_fb2_start;
phys_addr_t tegra_bootloader_fb2_size;
phys_addr_t tegra_fb_start;
phys_addr_t tegra_fb_size;
phys_addr_t tegra_fb2_start;
phys_addr_t tegra_fb2_size;
phys_addr_t tegra_carveout_start;
phys_addr_t tegra_carveout_size;
phys_addr_t tegra_tsec_start;
phys_addr_t tegra_tsec_size;
phys_addr_t tegra_lp0_vec_start;
phys_addr_t tegra_lp0_vec_size;

bool tegra_lp0_vec_relocate;
unsigned long tegra_grhost_aperture = ~0ul;
static struct board_info main_board_info;
static struct board_info pmu_board_info;
static struct board_info display_board_info;
static int panel_id;
static struct board_info camera_board_info;
static int touch_vendor_id;
static int touch_panel_id;
static struct board_info io_board_info;
static struct board_info button_board_info;
static struct board_info joystick_board_info;
static struct board_info rightspeaker_board_info;
static struct board_info leftspeaker_board_info;
#ifdef CONFIG_TEGRA_USE_NCT
unsigned long tegra_nck_start;
unsigned long tegra_nck_size;
#endif

int tegra_with_secure_firmware;

static int pmu_core_edp;
static int board_panel_type;
static int pwr_i2c_clk = 400;
static u8 power_config;
static u8 display_config;

static int tegra_split_mem_set;

/* Bootloader configured dfll frequency */
static unsigned long int dfll_boot_req_khz;

struct device tegra_generic_cma_dev;
struct device tegra_vpr_cma_dev;

struct device tegra_generic_dev;

struct device tegra_vpr_dev;

struct device tegra_iram_dev;
EXPORT_SYMBOL(tegra_iram_dev);

int (*nvgpu_do_idle)(void) = NULL;
int (*nvgpu_do_unidle)(void) = NULL;

#define CREATE_TRACE_POINTS
#include <trace/events/nvsecurity.h>

void tegra_register_idle_unidle(int (*gk20a_do_idle)(void),
				int (*gk20a_do_unidle)(void))
{
	nvgpu_do_idle = gk20a_do_idle;
	nvgpu_do_unidle = gk20a_do_unidle;
}
EXPORT_SYMBOL(tegra_register_idle_unidle);

void tegra_unregister_idle_unidle(void)
{
	nvgpu_do_idle = NULL;
	nvgpu_do_unidle = NULL;
}
EXPORT_SYMBOL(tegra_unregister_idle_unidle);

u32 notrace tegra_read_cycle(void)
{
	u32 cycle_count;

#ifdef CONFIG_ARM64
	asm volatile("mrs %0, pmccntr_el0" : "=r"(cycle_count));
#else
	asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(cycle_count));
#endif

	return cycle_count;
}

/*
 * Storage for debug-macro.S's state.
 *
 * This must be in .data not .bss so that it gets initialized each time the
 * kernel is loaded. The data is declared here rather than debug-macro.S so
 * that multiple inclusions of debug-macro.S point at the same data.
 */
u32 tegra_uart_config[4] = {
	/* Debug UART initialization required */
	1,
	/* Debug UART physical address */
	0,
	/* Debug UART virtual address */
	0,
	/* Scratch space for debug macro */
	0,
};


static int modem_id;
static int commchip_id;
static int sku_override;
static bool uart_over_sd;
static enum audio_codec_type audio_codec_name;
static enum image_type board_image_type = system_image;
static int max_core_current;
static int emc_max_dvfs;
static unsigned int memory_type;
static int usb_port_owner_info;
static int lane_owner_info;
static int chip_personality;

#ifdef CONFIG_ARCH_TEGRA_11x_SOC
static __initdata struct tegra_clk_init_table tegra11x_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "clk_m",	NULL,		0,		true },
	{ "emc",	NULL,		0,		true },
	{ "cpu",	NULL,		0,		true },
	{ "kfuse",	NULL,		0,		true },
	{ "fuse",	NULL,		0,		true },
	{ "sclk",	NULL,		0,		true },
	{ "pll_p",	NULL,		0,		true },
	{ "pll_p_out1",	"pll_p",	0,		false },
	{ "pll_p_out3",	"pll_p",	0,		false },
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	{ "pll_m_out1",	"pll_m",	275000000,	false },
	{ "pll_p_out2",	 "pll_p",	102000000,	false },
	{ "sclk",	 "pll_p_out2",	102000000,	true },
	{ "pll_p_out4",	 "pll_p",	204000000,	true },
	{ "hclk",	"sclk",		102000000,	true },
	{ "pclk",	"hclk",		51000000,	true },
	{ "mselect",	"pll_p",	102000000,	true },
	{ "host1x",	"pll_p",	102000000,	false },
	{ "cl_dvfs_ref", "pll_p",       51000000,       true },
	{ "cl_dvfs_soc", "pll_p",       51000000,       true },
	{ "dsialp", "pll_p",	70000000,	false },
	{ "dsiblp", "pll_p",	70000000,	false },
#else
	{ "pll_m_out1",	"pll_m",	275000000,	true },
	{ "pll_p_out2",	"pll_p",	108000000,	false },
	{ "sclk",	"pll_p_out2",	108000000,	true },
	{ "pll_p_out4",	"pll_p",	216000000,	true },
	{ "hclk",	"sclk",		108000000,	true },
	{ "pclk",	"hclk",		54000000,	true },
	{ "mselect",	"pll_p",	108000000,	true },
	{ "host1x",	"pll_p",	108000000,	false },
	{ "cl_dvfs_ref", "clk_m",	13000000,	false },
	{ "cl_dvfs_soc", "clk_m",	13000000,	false },
#endif
#ifdef CONFIG_TEGRA_SLOW_CSITE
	{ "csite",	"clk_m",	1000000,	true },
#else
	{ "csite",      NULL,           0,              true },
#endif
	{ "pll_u",	NULL,		480000000,	true },
	{ "pll_re_vco",	NULL,		612000000,	true },
	{ "xusb_falcon_src",	"pll_p",	204000000,	false},
	{ "xusb_host_src",	"pll_p",	102000000,	false},
	{ "xusb_ss_src",	"pll_re_vco",	122400000,	false},
	{ "xusb_hs_src",	"xusb_ss_div2",	61200000,	false},
	{ "xusb_fs_src",	"pll_u_48M",	48000000,	false},
	{ "sdmmc1",	"pll_p",	48000000,	false},
	{ "sdmmc3",	"pll_p",	48000000,	false},
	{ "sdmmc4",	"pll_p",	48000000,	false},
	{ "sbc1.sclk",	NULL,		40000000,	false},
	{ "sbc2.sclk",	NULL,		40000000,	false},
	{ "sbc3.sclk",	NULL,		40000000,	false},
	{ "sbc4.sclk",	NULL,		40000000,	false},
	{ "sbc5.sclk",	NULL,		40000000,	false},
	{ "sbc6.sclk",	NULL,		40000000,	false},
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ "c2bus",	"pll_c2",	250000000,	false },
	{ "c3bus",	"pll_c3",	250000000,	false },
	{ "pll_c",	NULL,		624000000,	false },
#else
	{ "cbus",	"pll_c",	250000000,	false },
#endif
	{ "pll_c_out1",	"pll_c",	150000000,	false },
#ifdef CONFIG_TEGRA_PLLM_SCALED
	{ "vi",		"pll_p",	0,		false},
#endif
#ifdef CONFIG_TEGRA_SOCTHERM
	{ "soc_therm",	"pll_p",	136000000,	false },
	{ "tsensor",	"clk_m",	500000,		false },
#endif
	{ "csite",	NULL,		0,		true },
	{ NULL,		NULL,		0,		0},
};
static __initdata struct tegra_clk_init_table tegra11x_cbus_init_table[] = {
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ "c2bus",	"pll_c2",	250000000,	false },
	{ "c3bus",	"pll_c3",	250000000,	false },
	{ "pll_c",	NULL,		624000000,	false },
#else
	{ "cbus",	"pll_c",	250000000,	false },
#endif
	{ "pll_c_out1",	"pll_c",	150000000,	false },
	{ NULL,		NULL,		0,		0},
};
#endif
/*
 * Set system level PLLs and common clocks. If for a board, clock rate/parent
 * is different, it can be overridden in the board specific clock table.
 */
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
static __initdata struct tegra_clk_init_table tegra12x_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "clk_m",	NULL,		0,		true },
	{ "mc",		NULL,		0,		true },
	{ "cpu",	NULL,		0,		true },
	{ "kfuse",	NULL,		0,		true },
	{ "fuse",	NULL,		0,		true },
	{ "sclk",	NULL,		0,		true },
	{ "pll_p",	NULL,		0,		true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out1",	"pll_p",	0,		false,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out3",	"pll_p",	0,		true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_m_out1",	"pll_m",	275000000,	false,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out2",	"pll_p",	102000000,	false,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "sclk",	"pll_p_out2",	102000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out4",	"pll_p",	204000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "host1x",	"pll_p",	102000000,	false,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "cl_dvfs_ref", "pll_p",	54000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "cl_dvfs_soc", "pll_p",	54000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "hclk",	"sclk",		102000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pclk",	"hclk",		51000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "mselect",	"pll_p",	102000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
	{ "pll_p_out5", "pll_p",	102000000,	true,
		TEGRA_CLK_INIT_PLATFORM_SI },
#ifdef CONFIG_TEGRA_SLOW_CSITE
	{ "csite",	"clk_m",	1000000,	true },
#else
	{ "csite",      NULL,           0,              true },
#endif
	{ "pll_u",	NULL,		480000000,	true },
	{ "pll_re_vco",	NULL,		672000000,	true },
	{ "xusb_falcon_src",	"pll_re_out",	336000000,	false},
	{ "xusb_host_src",	"pll_re_out",	112000000,	false},
	{ "xusb_ss_src",	"pll_u_480M",	120000000,	false},
	{ "xusb_hs_src",	"pll_u_60M",	60000000,	false},
	{ "xusb_fs_src",	"pll_u_48M",	48000000,	false},
	{ "sdmmc1",	"pll_p",	48000000,	false},
	{ "sdmmc3",	"pll_p",	48000000,	false},
	{ "sdmmc4",	"pll_p",	48000000,	false},
	{ "sdmmc1_ddr",	"pll_p",	48000000,	false},
	{ "sdmmc3_ddr",	"pll_p",	48000000,	false},
	{ "sdmmc4_ddr",	"pll_p",	48000000,	false},
	{ "cpu.mselect", NULL,		102000000,	true},
	{ "gpu_ref",	NULL,		0,		true},
	{ "gk20a.gbus",	NULL,		252000000,	false},
#ifdef CONFIG_TEGRA_PLLM_SCALED
	{ "vi",		"pll_p",	0,		false},
	{ "isp",	"pll_p",	0,		false},
#endif
#ifdef CONFIG_TEGRA_SOCTHERM
	{ "soc_therm",	"pll_p",	51000000,	false },
	{ "tsensor",	"clk_m",	500000,		false },
#endif
	{ "pll_d",	NULL,		0,		true },
	{ "dsialp",	"pll_p",	70000000,       false },
	{ "dsiblp",     "pll_p",        70000000,       false },
	{ NULL,		NULL,		0,		0},
};

#ifdef CONFIG_TEGRA_PLLCX_FIXED
	/* Set PLLCx rates for automotive */
static __initdata struct tegra_clk_init_table tegra12x_cbus_init_table[] = {
	/* Initialize c2bus, c3bus, or cbus at the end of the list
	 * after all the clocks are moved under the proper parents.
	 */
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ "c2bus",	"pll_c2",	372000000,	false },
	{ "c3bus",	"pll_c3",	450000000,	false },
	{ "pll_c",	NULL,		564000000,	false },
#else
	{ "cbus",	"pll_c",	564000000,	false },
#endif
	{ "pll_c_out1",	"pll_c",	282000000,	false },
	{ "c4bus",	"pll_c4",	600000000,	false },
	{ NULL,		NULL,		0,		0},
};
#else

#ifdef CONFIG_TEGRA_DUAL_CBUS
static __initdata struct tegra_clk_init_table tegra12x_cbus_init_table[] = {
	/* Initialize c2bus, c3bus, or cbus at the end of the list
	 * after all the clocks are moved under the proper parents.
	 */
	{ "c2bus",	"pll_c2",	250000000,	false },
	{ "c3bus",	"pll_c3",	250000000,	false },
	{ "pll_c",	NULL,		600000000,	false },
#else
	{ "cbus",	"pll_c",	200000000,	false },
#endif
	{ "pll_c_out1",	"pll_c",	100000000,	false },
	{ "c4bus",	"pll_c4",	200000000,	false },
	{ NULL,		NULL,		0,		0},
};
#endif

static __initdata struct tegra_clk_init_table tegra12x_sbus_init_table[] = {
	/* Initialize sbus (system clock) users after cbus init PLLs */
	{ "sbc1.sclk",	NULL,		40000000,	false},
	{ "sbc2.sclk",	NULL,		40000000,	false},
	{ "sbc3.sclk",	NULL,		40000000,	false},
	{ "sbc4.sclk",	NULL,		40000000,	false},
	{ "sbc5.sclk",	NULL,		40000000,	false},
	{ "sbc6.sclk",	NULL,		40000000,	false},
	{ "wake.sclk",	NULL,		40000000,	true, TEGRA_CLK_INIT_PLATFORM_SI },
	{ NULL,		NULL,		0,		0},
};
#endif

#ifdef CONFIG_CACHE_L2X0
static void tegra_cache_smc(bool enable, u32 arg)
{
	void __iomem *p = IO_ADDRESS(TEGRA_ARM_PL310_BASE);
	bool need_affinity_switch;
	bool can_switch_affinity;
	bool l2x0_enabled;
	cpumask_t local_cpu_mask;
	cpumask_t saved_cpu_mask;
	unsigned long flags;
	long ret;

	/*
	 * ISSUE : Some registers of PL310 controler must be written
	 *              from Secure context (and from CPU0)!
	 *
	 * When called form Normal we obtain an abort or do nothing.
	 * Instructions that must be called in Secure:
	 *      - Write to Control register (L2X0_CTRL==0x100)
	 *      - Write in Auxiliary controler (L2X0_AUX_CTRL==0x104)
	 *      - Invalidate all entries (L2X0_INV_WAY==0x77C),
	 *              mandatory at boot time.
	 *      - Tag and Data RAM Latency Control Registers
	 *              (0x108 & 0x10C) must be written in Secure.
	 */
	need_affinity_switch = (smp_processor_id() != 0);
	can_switch_affinity = !irqs_disabled();

	WARN_ON(need_affinity_switch && !can_switch_affinity);
	if (need_affinity_switch && can_switch_affinity) {
		cpu_set(0, local_cpu_mask);
		sched_getaffinity(0, &saved_cpu_mask);
		ret = sched_setaffinity(0, &local_cpu_mask);
		WARN_ON(ret != 0);
	}

	local_irq_save(flags);
	l2x0_enabled = readl_relaxed(p + L2X0_CTRL) & 1;
	if (enable && !l2x0_enabled)
		tegra_generic_smc(0x82000002, 0x00000001, arg);
	else if (!enable && l2x0_enabled)
		tegra_generic_smc(0x82000002, 0x00000002, arg);
	local_irq_restore(flags);

	if (need_affinity_switch && can_switch_affinity) {
		ret = sched_setaffinity(0, &saved_cpu_mask);
		WARN_ON(ret != 0);
	}
}

static void tegra_l2x0_disable(void)
{
	unsigned long flags;
	static u32 l2x0_way_mask;

	if (!l2x0_way_mask) {
		void __iomem *p = IO_ADDRESS(TEGRA_ARM_PL310_BASE);
		u32 aux_ctrl;
		u32 ways;

		aux_ctrl = readl_relaxed(p + L2X0_AUX_CTRL);
		ways = (aux_ctrl & (1 << 16)) ? 16 : 8;
		l2x0_way_mask = (1 << ways) - 1;
	}

	local_irq_save(flags);
	tegra_cache_smc(false, l2x0_way_mask);
	local_irq_restore(flags);
}

void tegra_init_cache(bool init)
{
	void __iomem *p = IO_ADDRESS(TEGRA_ARM_PL310_BASE);
	u32 aux_ctrl;
	u32 cache_type;
	u32 tag_latency, data_latency;

	if (tegra_cpu_is_secure()) {
		/* issue the SMC to enable the L2 */
		aux_ctrl = readl_relaxed(p + L2X0_AUX_CTRL);
		trace_smc_init_cache(NVSEC_SMC_START);
		tegra_cache_smc(true, aux_ctrl);
		trace_smc_init_cache(NVSEC_SMC_DONE);

		/* after init, reread aux_ctrl and register handlers */
		aux_ctrl = readl_relaxed(p + L2X0_AUX_CTRL);
		l2x0_init(p, aux_ctrl, 0xFFFFFFFF);

		/* override outer_disable() with our disable */
		outer_cache.disable = tegra_l2x0_disable;
	} else {
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		tag_latency = 0x331;
		data_latency = 0x441;
#else
		if (!tegra_platform_is_silicon()) {
			tag_latency = 0x770;
			data_latency = 0x770;
		} else if (is_lp_cluster()) {
			tag_latency = tegra_cpu_c1_l2_tag_latency;
			data_latency = tegra_cpu_c1_l2_data_latency;
		} else {
			tag_latency = tegra_cpu_c0_l2_tag_latency;
			data_latency = tegra_cpu_c0_l2_data_latency;
		}
#endif
		writel_relaxed(tag_latency, p + L2X0_TAG_LATENCY_CTRL);
		writel_relaxed(data_latency, p + L2X0_DATA_LATENCY_CTRL);

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
		if (!tegra_platform_is_fpga()) {
			writel(0x7, p + L2X0_PREFETCH_CTRL);
			writel(0x3, p + L2X0_POWER_CTRL);
		}
#endif
		cache_type = readl(p + L2X0_CACHE_TYPE);
		aux_ctrl = (cache_type & 0x700) << (17-8);
		aux_ctrl |= 0x7C400001;
		if (init) {
			l2x0_init(p, aux_ctrl, 0x8200c3fe);
		} else {
			u32 tmp;

			tmp = aux_ctrl;
			aux_ctrl = readl(p + L2X0_AUX_CTRL);
			aux_ctrl &= 0x8200c3fe;
			aux_ctrl |= tmp;
			writel(aux_ctrl, p + L2X0_AUX_CTRL);
		}
		l2x0_enable();
	}
}
#endif

static void __init tegra_perf_init(void)
{
	u32 reg;

#ifdef CONFIG_ARM64
	asm volatile("mrs %0, PMCR_EL0" : "=r"(reg));
	reg >>= 11;
	reg = (1 << (reg & 0x1f))-1;
	reg |= 0x80000000;
	asm volatile("msr PMINTENCLR_EL1, %0" : : "r"(reg));
	reg = 1;
	asm volatile("msr PMUSERENR_EL0, %0" : : "r"(reg));
#else
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(reg));
	reg >>= 11;
	reg = (1 << (reg & 0x1f))-1;
	reg |= 0x80000000;
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r"(reg));
	reg = 1;
	asm volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(reg));
#endif
}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
static void __init tegra_ramrepair_init(void)
{
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	if (tegra_spare_fuse(10)  | tegra_spare_fuse(11)) {
#endif
		u32 reg;
		reg = readl(FLOW_CTRL_RAM_REPAIR);
		reg &= ~FLOW_CTRL_RAM_REPAIR_BYPASS_EN;
		writel(reg, FLOW_CTRL_RAM_REPAIR);
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	}
#endif
}
#endif

static void __init tegra_init_power(void)
{
#if defined(CONFIG_ARCH_TEGRA_HAS_SATA) || defined(CONFIG_ARCH_TEGRA_HAS_PCIE)
	int partition_id;
#endif
#ifdef CONFIG_TEGRA_XUSB_PLATFORM
	int partition_id_xusba, partition_id_xusbb, partition_id_xusbc;
#endif

#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
	partition_id = tegra_pd_get_powergate_id(tegra_sata_pd);
	if (partition_id < 0)
		return;
	tegra_powergate_partition(partition_id);
#endif

#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
	partition_id = tegra_pd_get_powergate_id(tegra_pcie_pd);
	if (partition_id < 0)
		return;
	tegra_powergate_partition(partition_id);
#endif

#if defined(CONFIG_TEGRA_XUSB_PLATFORM)
	/* powergate xusb partitions by default */
	partition_id_xusbb = tegra_pd_get_powergate_id(tegra_xusbb_pd);
	if (partition_id < 0)
		return;

	partition_id_xusba = tegra_pd_get_powergate_id(tegra_xusba_pd);
	if (partition_id < 0)
		return;

	partition_id_xusbc = tegra_pd_get_powergate_id(tegra_xusbc_pd);
	if (partition_id < 0)
		return;

	tegra_powergate_partition(partition_id_xusbb);
	tegra_powergate_partition(partition_id_xusba);
	tegra_powergate_partition(partition_id_xusbc);
#endif

}

static inline unsigned long gizmo_readl(unsigned long offset)
{
	return readl(IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static inline void gizmo_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static void __init tegra_init_ahb_gizmo_settings(void)
{
	unsigned long val;

	val = gizmo_readl(AHB_GIZMO_AHB_MEM);
	val |= ENB_FAST_REARBITRATE | IMMEDIATE | DONT_SPLIT_AHB_WR;

	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA11)
		val |= WR_WAIT_COMMIT_ON_1K;
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	val |= WR_WAIT_COMMIT_ON_1K | EN_USB_WAIT_COMMIT_ON_1K_STALL;
#endif
	gizmo_writel(val, AHB_GIZMO_AHB_MEM);

	val = gizmo_readl(AHB_GIZMO_USB);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB);

	val = gizmo_readl(AHB_GIZMO_USB2);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB2);

	val = gizmo_readl(AHB_GIZMO_USB3);
	val |= IMMEDIATE;
	gizmo_writel(val, AHB_GIZMO_USB3);

	val = gizmo_readl(AHB_ARBITRATION_PRIORITY_CTRL);
	val |= PRIORITY_SELECT_USB | PRIORITY_SELECT_USB2 | PRIORITY_SELECT_USB3
				| AHB_PRIORITY_WEIGHT(7);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	val |= PRIORITY_SELECT_SE;
#endif

	gizmo_writel(val, AHB_ARBITRATION_PRIORITY_CTRL);

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG1);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | AHBDMA_MST_ID |
		ADDR_BNDRY(0xc) | INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG1));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG2);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG2));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG3);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB3_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG3));

	val = gizmo_readl(AHB_MEM_PREFETCH_CFG4);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | USB2_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG4));

	/*
	 * SDMMC controller is removed from AHB interface in T124 and
	 * later versions of Tegra. Configure AHB prefetcher for SDMMC4
	 * in T11x.
	 */
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	val = gizmo_readl(AHB_MEM_PREFETCH_CFG5);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | SDMMC4_MST_ID;
	val &= ~SDMMC4_MST_ID;
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG5));
#endif

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	val = gizmo_readl(AHB_MEM_PREFETCH_CFG6);
	val &= ~MST_ID(~0);
	val |= PREFETCH_ENB | SE_MST_ID | ADDR_BNDRY(0xc) |
		INACTIVITY_TIMEOUT(0x1000);
	ahb_gizmo_writel(val,
		IO_ADDRESS(TEGRA_AHB_GIZMO_BASE + AHB_MEM_PREFETCH_CFG6));
#endif
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
void __init tegra20_init_early(void)
{
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	tegra_init_cache(true);
	tegra_powergate_init();
	tegra20_hotplug_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();
}
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
void __init tegra30_init_early(void)
{
	u32 speedo;
	u32 tag_latency, data_latency;

	display_tegra_dt_info();
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	/*
	 * Store G/LP cluster L2 latencies to IRAM and DRAM
	 */
	tegra_cpu_c1_l2_tag_latency = 0x221;
	tegra_cpu_c1_l2_data_latency = 0x221;
	writel_relaxed(0x221, tegra_cpu_c1_l2_tag_latency_iram);
	writel_relaxed(0x221, tegra_cpu_c1_l2_data_latency_iram);
	/* relax l2-cache latency for speedos 4,5,6 (T33's chips) */
	speedo = tegra_cpu_speedo_id();
	if (speedo == 4 || speedo == 5 || speedo == 6 ||
	    speedo == 12 || speedo == 13) {
		tag_latency = 0x442;
		data_latency = 0x552;
	} else {
		tag_latency = 0x441;
		data_latency = 0x551;
	}
	tegra_cpu_c0_l2_tag_latency = tag_latency;
	tegra_cpu_c0_l2_data_latency = data_latency;
	writel_relaxed(tag_latency, tegra_cpu_c0_l2_tag_latency_iram);
	writel_relaxed(data_latency, tegra_cpu_c0_l2_data_latency_iram);
	tegra_init_cache(true);
	tegra_powergate_init();
	tegra30_hotplug_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();

	init_dma_coherent_pool_size(SZ_1M);
}
#endif
#ifdef CONFIG_ARCH_TEGRA_11x_SOC
void __init tegra11x_init_early(void)
{
	display_tegra_dt_info();
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	tegra_ramrepair_init();
	tegra11x_init_clocks();
	tegra11x_init_dvfs();
	tegra_common_init_clock();
	tegra_clk_init_from_table(tegra11x_clk_init_table);
	tegra_clk_init_cbus_plls_from_table(tegra11x_cbus_init_table);
	tegra11x_clk_init_la();
	tegra_powergate_init();
	tegra30_hotplug_init();
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();

	init_dma_coherent_pool_size(SZ_2M);
}
#endif
#ifdef CONFIG_ARCH_TEGRA_12x_SOC
void __init tegra12x_init_early(void)
{
	if (of_find_compatible_node(NULL, NULL, "arm,psci") ||
	    of_find_compatible_node(NULL, NULL, "arm,psci-0.2"))
			tegra_with_secure_firmware = 1;

	display_tegra_dt_info();
	tegra_apb_io_init();
	tegra_perf_init();
	tegra_init_fuse();
	tegra_ramrepair_init();
	tegra12x_init_clocks();
#ifdef CONFIG_ARCH_TEGRA_13x_SOC
	tegra13x_init_dvfs();
#else
	tegra12x_init_dvfs();
#endif
	tegra_common_init_clock();
	tegra_clk_init_from_table(tegra12x_clk_init_table);
	tegra_clk_init_cbus_plls_from_table(tegra12x_cbus_init_table);
	tegra_clk_init_from_table(tegra12x_sbus_init_table);
	tegra_non_dt_clock_reset_init();
	tegra_powergate_init();
#ifndef CONFIG_ARM64
	tegra30_hotplug_init();
#endif
	tegra_init_power();
	tegra_init_ahb_gizmo_settings();
}
#endif
static int __init tegra_lp0_vec_arg(char *options)
{
	char *p = options;

	tegra_lp0_vec_size = memparse(p, &p);
	if (*p == '@')
		tegra_lp0_vec_start = memparse(p+1, &p);
	if (!tegra_lp0_vec_size || !tegra_lp0_vec_start) {
		tegra_lp0_vec_size = 0;
		tegra_lp0_vec_start = 0;
	}

	return 0;
}
early_param("lp0_vec", tegra_lp0_vec_arg);

static int __init tegra_bootloader_fb_arg(char *options)
{
	char *p = options;

	tegra_bootloader_fb_size = memparse(p, &p);
	if (*p == '@')
		tegra_bootloader_fb_start = memparse(p+1, &p);

	pr_info("Found tegra_fbmem: %08llx@%08llx\n",
		(u64)tegra_bootloader_fb_size, (u64)tegra_bootloader_fb_start);

	return 0;
}
early_param("tegra_fbmem", tegra_bootloader_fb_arg);

static int __init tegra_bootloader_fb2_arg(char *options)
{
	char *p = options;

	tegra_bootloader_fb2_size = memparse(p, &p);
	if (*p == '@')
		tegra_bootloader_fb2_start = memparse(p+1, &p);

	pr_info("Found tegra_fbmem2: %08llx@%08llx\n",
		(u64)tegra_bootloader_fb2_size,
		(u64)tegra_bootloader_fb2_start);

	return 0;
}
early_param("tegra_fbmem2", tegra_bootloader_fb2_arg);

static int __init tegra_sku_override(char *id)
{
	char *p = id;

	sku_override = memparse(p, &p);

	return 0;
}
early_param("sku_override", tegra_sku_override);

int tegra_get_sku_override(void)
{
	return sku_override;
}

static int __init tegra_chip_personality(char *id)
{
	char *p = id;

	chip_personality = memparse(p, &p);

	return 0;
}
early_param("chip_personality", tegra_chip_personality);

int tegra_get_chip_personality(void)
{
	return chip_personality;
}

static int __init tegra_tsec_arg(char *options)
{
	char *p = options;

	tegra_tsec_size = memparse(p, &p);
	if (*p == '@')
		tegra_tsec_start = memparse(p+1, &p);
	pr_info("Found tsec, start=0x%llx size=%llx",
		(u64)tegra_tsec_start, (u64)tegra_tsec_size);
	return 0;
}
early_param("tsec", tegra_tsec_arg);

#ifdef CONFIG_TEGRA_USE_NCT
static int __init tegra_nck_arg(char *options)
{
	char *p = options;

	tegra_nck_size = memparse(p, &p);
	if (*p == '@')
		tegra_nck_start = memparse(p+1, &p);
	if (!tegra_nck_size || !tegra_nck_start) {
		tegra_nck_size = 0;
		tegra_nck_start = 0;
	}

	return 0;
}
early_param("nck", tegra_nck_arg);
#endif	/* CONFIG_TEGRA_USE_NCT */

enum panel_type get_panel_type(void)
{
	return board_panel_type;
}
static int __init tegra_board_panel_type(char *options)
{
	if (!strcmp(options, "lvds"))
		board_panel_type = panel_type_lvds;
	else if (!strcmp(options, "dsi"))
		board_panel_type = panel_type_dsi;
	else
		return 0;
	return 1;
}
__setup("panel=", tegra_board_panel_type);

int tegra_get_board_panel_id(void)
{
	return panel_id;
}
static int __init tegra_board_panel_id(char *options)
{
	char *p = options;
	panel_id = memparse(p, &p);
	return panel_id;
}
__setup("display_panel=", tegra_board_panel_id);

/* returns true if bl initialized the display */
bool tegra_is_bl_display_initialized(int instance)
{
	/* display initialized implies non-zero
	 * fb size is passed from bl to kernel
	 */
	switch (instance) {
	case 0:
		return tegra_bootloader_fb_start && tegra_bootloader_fb_size;
	case 1:
		return tegra_bootloader_fb2_start && tegra_bootloader_fb2_size;
	default:
		return false;
	}
}

int tegra_get_touch_vendor_id(void)
{
	return touch_vendor_id;
}
int tegra_get_touch_panel_id(void)
{
	return touch_panel_id;
}
static int __init tegra_touch_id(char *options)
{
	char *p = options;
	touch_vendor_id = memparse(p, &p);
	if (*p == '@')
		touch_panel_id = memparse(p+1, &p);
	return 1;
}
__setup("touch_id=", tegra_touch_id);

u8 get_power_config(void)
{
	return power_config;
}
static int __init tegra_board_power_config(char *options)
{
	char *p = options;
	power_config = memparse(p, &p);
	return 1;
}
__setup("power-config=", tegra_board_power_config);

u8 get_display_config(void)
{
	return display_config;
}
static int __init tegra_board_display_config(char *options)
{
	char *p = options;
	display_config = memparse(p, &p);
	return 1;
}
__setup("display-config=", tegra_board_display_config);

int get_core_edp(void)
{
	return pmu_core_edp;
}
static int __init tegra_pmu_core_edp(char *options)
{
	char *p = options;
	int core_edp = memparse(p, &p);
	if (core_edp != 0)
		pmu_core_edp = core_edp;
	return 0;
}
early_param("core_edp_mv", tegra_pmu_core_edp);

int get_maximum_core_current_supported(void)
{
	return max_core_current;
}
static int __init tegra_max_core_current(char *options)
{
	char *p = options;
	max_core_current = memparse(p, &p);
	return 0;
}
early_param("core_edp_ma", tegra_max_core_current);

int get_emc_max_dvfs(void)
{
	return emc_max_dvfs;
}
static int __init tegra_emc_max_dvfs(char *options)
{
	char *p = options;
	emc_max_dvfs = memparse(p, &p);
	return 1;
}
__setup("emc_max_dvfs=", tegra_emc_max_dvfs);

int tegra_get_memory_type(void)
{
	return memory_type;
}
static int __init tegra_memory_type(char *options)
{
	char *p = options;
	memory_type = memparse(p, &p);
	return 1;
}
__setup("memtype=", tegra_memory_type);

static int tegra_get_uart_over_sd_property(void)
{
	struct device_node *np;
	u32 pval;
	int ret;

	np = of_find_node_by_path("/chosen");
	if (!np)
		return -EINVAL;

	ret = of_property_read_u32(np, "nvidia,debug-console-over-sd", &pval);
	if (ret < 0) {
		pr_err("/chosen/nvidia,debug-console-over-sd read failed: %d\n",
			ret);
		return ret;
	}
	return (int)pval;
}

static int __init tegra_debug_uartport(char *info)
{
	char *p = info;
	unsigned long long port_id;
	int uart_port_id;

	if (p[6] == ',') {
		if (p[7] != '-') {
			port_id = memparse(p + 7, &p);
			uart_port_id = (int) port_id;
			if (uart_port_id == 5)
				uart_over_sd = true;
		}
	}

	return 1;
}

bool is_uart_over_sd_enabled(void)
{
	static bool dt_parsed = 0;
	int ret;

	if (!dt_parsed) {
		dt_parsed = 1;
		ret = tegra_get_uart_over_sd_property();
		if (ret == 1)
			 uart_over_sd = true;
	}
	return uart_over_sd;
}
__setup("debug_uartport=", tegra_debug_uartport);

static int __init tegra_image_type(char *options)
{
	if (!strcmp(options, "RCK"))
		board_image_type = rck_image;

	return 0;
}

enum image_type get_tegra_image_type(void)
{
	return board_image_type;
}

__setup("image=", tegra_image_type);

static int __init tegra_audio_codec_type(char *info)
{
	char *p = info;
	if (!strncmp(p, "wm8903", 6))
		audio_codec_name = audio_codec_wm8903;
	else
		audio_codec_name = audio_codec_none;

	return 1;
}

enum audio_codec_type get_audio_codec_type(void)
{
	return audio_codec_name;
}
__setup("audio_codec=", tegra_audio_codec_type);

static int tegra_get_pwr_i2c_clk_rate(char *options)
{
	int clk = simple_strtol(options, NULL, 16);
	if (clk != 0)
		pwr_i2c_clk = clk;
	return 0;
}

int get_pwr_i2c_clk_rate(void)
{
	return pwr_i2c_clk;
}
__setup("pwr_i2c=", tegra_get_pwr_i2c_clk_rate);

#ifdef CONFIG_OF
void find_dc_node(struct device_node **dc1_node,
		struct device_node **dc2_node) {
	*dc1_node =
		of_find_node_by_path("/host1x/dc@54200000");
	*dc2_node =
		of_find_node_by_path("/host1x/dc@54240000");
}
#else
void find_dc_node(struct device_node *dc1_node,
		struct device_node *dc2_node) {
	return;
}
#endif

static int tegra_get_board_info_properties(struct board_info *bi,
		const char *property_name)
{
	struct device_node *board_info;
	char board_info_path[50] = {0};
	u32 prop_val;
	int err;

	strcpy(board_info_path, "/chosen/");
	strcat(board_info_path, property_name);

	board_info = of_find_node_by_path(board_info_path);
	memset(bi, 0, sizeof(*bi));

	if (board_info) {
		err = of_property_read_u32(board_info, "id", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/id\n", board_info_path);
			goto out;
		}
		bi->board_id = prop_val;

		err = of_property_read_u32(board_info, "sku", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/sku\n", board_info_path);
			goto out;
		}
		bi->sku = prop_val;

		err = of_property_read_u32(board_info, "fab", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/fab\n", board_info_path);
			goto out;
		}
		bi->fab = prop_val;

		err = of_property_read_u32(board_info, "major_revision", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/major_revision\n",
					board_info_path);
			goto out;
		}
		bi->major_revision = prop_val;

		err = of_property_read_u32(board_info, "minor_revision", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/minor_revision\n",
					board_info_path);
			goto out;
		}
		bi->minor_revision = prop_val;
		return 0;
	}

	pr_err("Node path %s not found\n", board_info_path);
out:
	return -1;
}

void tegra_get_board_info(struct board_info *bi)
{
	int ret;
	static bool parsed = 0;

	if (!parsed) {
		unsigned int system_serial_high, system_serial_low;
		parsed = 1;
		ret = tegra_get_board_info_properties(bi, "proc-board");
		if (!ret) {
			memcpy(&main_board_info, bi, sizeof(struct board_info));
			system_serial_high = (bi->board_id << 16) | bi->sku;
			system_serial_low = (bi->fab << 24) |
						(bi->major_revision << 16) |
						(bi->minor_revision << 8);
			return;
		}
	}
	memcpy(bi, &main_board_info, sizeof(struct board_info));
}
EXPORT_SYMBOL(tegra_get_board_info);

void tegra_get_pmu_board_info(struct board_info *bi)
{
	static bool parsed = 0;

	if (!parsed) {
		int ret;
		parsed = 1;
		ret = tegra_get_board_info_properties(bi, "pmu-board");
		if (!ret)
			memcpy(&pmu_board_info, bi, sizeof(struct board_info));
	}
	memcpy(bi, &pmu_board_info, sizeof(struct board_info));
}

void tegra_get_display_board_info(struct board_info *bi)
{
	static bool parsed = 0;

	if (!parsed) {
		int ret;
		parsed = 1;
		ret = tegra_get_board_info_properties(bi, "display-board");
		if (!ret)
			memcpy(&display_board_info, bi, sizeof(struct board_info));
	}
	memcpy(bi, &display_board_info, sizeof(struct board_info));
}

void tegra_get_camera_board_info(struct board_info *bi)
{
	static bool parsed = 0;

	if (!parsed) {
		int ret;
		parsed = 1;

		ret = tegra_get_board_info_properties(bi, "camera-board");
		if (!ret)
			memcpy(&camera_board_info, bi, sizeof(*bi));
	}
	memcpy(bi, &camera_board_info, sizeof(struct board_info));
}

void tegra_get_leftspeaker_board_info(struct board_info *bi)
{
	static bool parsed = 0;

	if (!parsed) {
		int ret;
		parsed = 1;

		ret = tegra_get_board_info_properties(bi, "left-speaker-board");
		if (!ret)
			memcpy(&leftspeaker_board_info, bi, sizeof(*bi));
	}
	memcpy(bi, &leftspeaker_board_info, sizeof(struct board_info));
}

void tegra_get_rightspeaker_board_info(struct board_info *bi)
{
	static bool parsed = 0;

	if (!parsed) {
		int ret;
		parsed = 1;

		ret = tegra_get_board_info_properties(bi, "right-speaker-board");
		if (!ret)
			memcpy(&rightspeaker_board_info, bi, sizeof(*bi));
	}
	memcpy(bi, &rightspeaker_board_info, sizeof(struct board_info));
}

void tegra_get_joystick_board_info(struct board_info *bi)
{
	static bool parsed = 0;

	if (!parsed) {
		int ret;
		parsed = 1;

		ret = tegra_get_board_info_properties(bi, "joystick-board");
		if (!ret)
			memcpy(&joystick_board_info, bi, sizeof(*bi));
	}
	memcpy(bi, &joystick_board_info, sizeof(struct board_info));
}

void tegra_get_button_board_info(struct board_info *bi)
{
	static bool parsed = 0;

	if (!parsed) {
		int ret;
		parsed = 1;

		ret = tegra_get_board_info_properties(bi, "button-board");
		if (!ret)
			memcpy(&button_board_info, bi, sizeof(*bi));
	}
	memcpy(bi, &button_board_info, sizeof(struct board_info));
}

void tegra_get_io_board_info(struct board_info *bi)
{
	static bool parsed = 0;

	if (!parsed) {
		int ret;
		parsed = 1;

		ret = tegra_get_board_info_properties(bi, "io-board");
		if (!ret)
			memcpy(&io_board_info, bi, sizeof(*bi));
	}
	memcpy(bi, &io_board_info, sizeof(struct board_info));
}

static int __init tegra_modem_id(char *id)
{
	char *p = id;

	modem_id = memparse(p, &p);
	return 1;
}

int tegra_get_modem_id(void)
{
	return modem_id;
}
EXPORT_SYMBOL(tegra_get_modem_id);

__setup("modem_id=", tegra_modem_id);

static int __init tegra_usb_port_owner_info(char *id)
{
	char *p = id;

	usb_port_owner_info = memparse(p, &p);
	return 1;
}

int tegra_get_usb_port_owner_info(void)
{
	return usb_port_owner_info;
}
EXPORT_SYMBOL(tegra_get_usb_port_owner_info);

__setup("usb_port_owner_info=", tegra_usb_port_owner_info);

static int __init tegra_lane_owner_info(char *id)
{
	char *p = id;

	lane_owner_info = memparse(p, &p);
	return 1;
}

int tegra_get_lane_owner_info(void)
{
	return lane_owner_info;
}
EXPORT_SYMBOL(tegra_get_lane_owner_info);

__setup("lane_owner_info=", tegra_lane_owner_info);

static int __init tegra_commchip_id(char *id)
{
	char *p = id;

	if (get_option(&p, &commchip_id) != 1)
		return 0;
	return 1;
}

int tegra_get_commchip_id(void)
{
	return commchip_id;
}

__setup("commchip_id=", tegra_commchip_id);

#ifdef CONFIG_ANDROID
static bool androidboot_mode_charger;

bool get_androidboot_mode_charger(void)
{
	return androidboot_mode_charger;
}
static int __init tegra_androidboot_mode(char *options)
{
	if (!strcmp(options, "charger"))
		androidboot_mode_charger = true;
	else
		androidboot_mode_charger = false;
	return 1;
}
__setup("androidboot.mode=", tegra_androidboot_mode);
#endif

/*
 * Tegra has a protected aperture that prevents access by most non-CPU
 * memory masters to addresses above the aperture value.  Enabling it
 * secures the CPU's memory from the GPU, except through the GART.
 */
void __init tegra_protected_aperture_init(unsigned long aperture)
{
	void __iomem *mc_base = IO_ADDRESS(TEGRA_MC_BASE);
	pr_info("Enabling Tegra protected aperture at 0x%08lx\n", aperture);
	writel(aperture, mc_base + MC_SECURITY_CFG2);
}

/*
 * Due to conflicting restrictions on the placement of the framebuffer,
 * the bootloader is likely to leave the framebuffer pointed at a location
 * in memory that is outside the grhost aperture.  This function will move
 * the framebuffer contents from a physical address that is anywhere (lowmem,
 * highmem, or outside the memory map) to a physical address that is outside
 * the memory map.
 */
void __tegra_move_framebuffer(struct platform_device *pdev,
	phys_addr_t to, phys_addr_t from,
	size_t size)
{
	struct page *page;
	void __iomem *to_io;
	void *from_virt;
	unsigned long i;

	BUG_ON(PAGE_ALIGN((unsigned long)to) != (unsigned long)to);
	BUG_ON(PAGE_ALIGN(from) != from);
	BUG_ON(PAGE_ALIGN(size) != size);

	to_io = ioremap_wc(to, size);
	if (!to_io) {
		pr_err("%s: Failed to map target framebuffer\n", __func__);
		return;
	}

	if (from && pfn_valid(page_to_pfn(phys_to_page(from)))) {
		for (i = 0 ; i < size; i += PAGE_SIZE) {
			page = phys_to_page(from + i);
			from_virt = kmap(page);
			memcpy(to_io + i, from_virt, PAGE_SIZE);
			kunmap(page);
		}
	} else if (from) {
		void __iomem *from_io = ioremap_wc(from, size);
		if (!from_io) {
			pr_err("%s: Failed to map source framebuffer\n",
				__func__);
			goto out;
		}

		for (i = 0; i < size; i += 4)
			writel_relaxed(readl_relaxed(from_io + i), to_io + i);
		dmb(ishld);

		iounmap(from_io);
	}

out:
	iounmap(to_io);
}

void __tegra_clear_framebuffer(struct platform_device *pdev,
			       unsigned long to, unsigned long size)
{
	void __iomem *to_io;
	unsigned long i;

	BUG_ON(PAGE_ALIGN((unsigned long)to) != (unsigned long)to);
	BUG_ON(PAGE_ALIGN(size) != size);

	to_io = ioremap_wc(to, size);
	if (!to_io) {
		pr_err("%s: Failed to map target framebuffer\n", __func__);
		return;
	}

	if (pfn_valid(page_to_pfn(phys_to_page(to)))) {
		for (i = 0 ; i < size; i += PAGE_SIZE)
			memset(to_io + i, 0, PAGE_SIZE);
	} else {
		for (i = 0; i < size; i += 4)
			writel_relaxed(0, to_io + i);
		dmb(ishld);
	}

	iounmap(to_io);
}

#ifdef CONFIG_PSTORE_RAM
static struct ramoops_platform_data ramoops_data;

static struct platform_device ramoops_dev  = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ramoops_data,
	},
};


static void __init tegra_reserve_ramoops_memory(unsigned long reserve_size)
{
	ramoops_data.mem_size = reserve_size;
	ramoops_data.mem_address = memblock_end_of_4G() - reserve_size;
	ramoops_data.record_size = RECORD_MEM_SIZE;
#ifdef CONFIG_PSTORE_CONSOLE
	ramoops_data.console_size = CONSOLE_MEM_SIZE;
#endif
#ifdef CONFIG_PSTORE_FTRACE
	ramoops_data.ftrace_size = FTRACE_MEM_SIZE;
#endif
#ifdef CONFIG_PSTORE_RTRACE
	ramoops_data.rtrace_size = RTRACE_MEM_SIZE;
#endif
	ramoops_data.dump_oops = 1;
	if (memblock_remove(ramoops_data.mem_address, ramoops_data.mem_size))
		pr_err("Failed to remove carveout %08lx@%08llx from memory map\n",
			reserve_size, (u64)ramoops_data.mem_address);
}

static int __init tegra_register_ramoops_device(void)
{
	int ret = platform_device_register(&ramoops_dev);
	if (ret) {
		pr_info("Unable to register ramoops platform device\n");
		return ret;
	}
	return ret;
}
core_initcall(tegra_register_ramoops_device);
#endif

phys_addr_t __init tegra_reserve_adsp(unsigned long size)
{
	phys_addr_t reserve = memblock_end_of_4G() - size;
	if (memblock_remove(reserve, size)) {
		pr_err("%s failed for %llx:%lx\n",
				__func__, (u64)reserve, size);
	}
	pr_info("%s: reserved %llx:%lx\n",
				__func__, (u64)reserve, size);
	return reserve;
}

void __init tegra_reserve(unsigned long carveout_size, unsigned long fb_size,
	unsigned long fb2_size)
{
	struct iommu_linear_map map[4];

	if (!tegra_vpr_resize && carveout_size) {
		/*
		 * Place the carveout below the 4 GB physical address limit
		 * because IOVAs are only 32 bit wide.
		 */
		BUG_ON(memblock_end_of_4G() == 0);
		tegra_carveout_start = memblock_end_of_4G() - carveout_size;
		if (memblock_remove(tegra_carveout_start, carveout_size)) {
			pr_err("Failed to remove carveout %08lx@%08llx "
				"from memory map\n",
				carveout_size, (u64)tegra_carveout_start);
			tegra_carveout_start = 0;
			tegra_carveout_size = 0;
		} else
			tegra_carveout_size = carveout_size;
	}

	if (fb2_size) {
#ifdef CONFIG_MODS
		if (fb2_size < (70 * SZ_1M))
			fb2_size = 70 * SZ_1M;
#endif
		/*
		 * Place fb2 below the 4 GB physical address limit because
		 * IOVAs are only 32 bit wide.
		 */
		BUG_ON(memblock_end_of_4G() == 0);
		tegra_fb2_start = memblock_end_of_4G() - fb2_size;
		if (memblock_remove(tegra_fb2_start, fb2_size)) {
			pr_err("Failed to remove second framebuffer "
				"%08lx@%08llx from memory map\n",
				fb2_size, (u64)tegra_fb2_start);
			tegra_fb2_start = 0;
			tegra_fb2_size = 0;
		} else
			tegra_fb2_size = fb2_size;
	}

	if (fb_size) {
#ifdef CONFIG_MODS
		if (fb_size < (70 * SZ_1M))
			fb_size = 70 * SZ_1M;
#endif
		/*
		 * Place fb below the 4 GB physical address limit because
		 * IOVAs are only 32 bit wide.
		 */
		BUG_ON(memblock_end_of_4G() == 0);
		tegra_fb_start = memblock_end_of_4G() - fb_size;
		if (memblock_remove(tegra_fb_start, fb_size)) {
			pr_err("Failed to remove framebuffer %08lx@%08llx "
				"from memory map\n",
				fb_size, (u64)tegra_fb_start);
			tegra_fb_start = 0;
			tegra_fb_size = 0;
		} else
			tegra_fb_size = fb_size;
	}

	if (tegra_split_mem_active()) {
		tegra_fb_start = TEGRA_ASIM_QT_FB_START;
		tegra_fb_size = TEGRA_ASIM_QT_FB_SIZE;

		if (tegra_vpr_size == 0) {
			tegra_carveout_start =
			   TEGRA_ASIM_QT_CARVEOUT_VPR_DISABLED_START;
			tegra_carveout_size =
			   TEGRA_ASIM_QT_CARVEOUT_VPR_DISABLED_SIZE;
		} else if (
			    (tegra_vpr_start <
			     TEGRA_ASIM_QT_FB_START +
			     TEGRA_ASIM_QT_FB_SIZE) ||
			     (tegra_vpr_start + tegra_vpr_size - 1 >
			     TEGRA_ASIM_QT_FRONT_DOOR_MEM_START +
			     TEGRA_ASIM_QT_FRONT_DOOR_MEM_SIZE - 1)) {
			/*
			 * On ASIM/ASIM + QT with
			 * CONFIG_TEGRA_SIMULATION_SPLIT_MEM enabled,
			 * the VPR region needs to be within the front
			 * door memory region. Moreover, the VPR region
			 * can't exist where the framebuffer resides.
			 */
			BUG();
		} else if (
				(tegra_vpr_start -
				 (TEGRA_ASIM_QT_FB_START +
				  TEGRA_ASIM_QT_FB_SIZE) <
				 TEGRA_ASIM_QT_CARVEOUT_MIN_SIZE) &&
				(TEGRA_ASIM_QT_FRONT_DOOR_MEM_START +
				 TEGRA_ASIM_QT_FRONT_DOOR_MEM_SIZE -
				 (tegra_vpr_start + tegra_vpr_size) <
				 TEGRA_ASIM_QT_CARVEOUT_MIN_SIZE)) {
			/*
			 * The tegra ASIM/QT carveout has a min size:-
			 * TEGRA_ASIM_QT_CARVEOUT_MIN_SIZE. All free
			 * regions in front door mem are smaller than
			 * the min carveout size. Therefore, we can't
			 * fit the carveout in front door mem.
			 */
			BUG();
		} else if (
				(tegra_vpr_start -
				 (TEGRA_ASIM_QT_FB_START +
				  TEGRA_ASIM_QT_FB_SIZE)) >=
				(TEGRA_ASIM_QT_FRONT_DOOR_MEM_START +
				 TEGRA_ASIM_QT_FRONT_DOOR_MEM_SIZE -
				 (tegra_vpr_start + tegra_vpr_size))) {
			/*
			 * Place the tegra ASIM/QT carveout between the
			 * framebuffer and VPR.
			 */
			tegra_carveout_start =
			  TEGRA_ASIM_QT_CARVEOUT_VPR_DISABLED_START;
			tegra_carveout_size = tegra_vpr_start -
				(TEGRA_ASIM_QT_FB_START +
				 TEGRA_ASIM_QT_FB_SIZE);
		} else {
			/*
			 * Place the tegra ASIM/QT carveout after VPR.
			 */
			tegra_carveout_start = tegra_vpr_start +
						 tegra_vpr_size;
			tegra_carveout_size =
				TEGRA_ASIM_QT_FRONT_DOOR_MEM_START +
				TEGRA_ASIM_QT_FRONT_DOOR_MEM_SIZE -
				(tegra_vpr_start + tegra_vpr_size);
		}
	}

	if (tegra_fb_size)
		tegra_grhost_aperture = tegra_fb_start;

	if (tegra_fb2_size && tegra_fb2_start < tegra_grhost_aperture)
		tegra_grhost_aperture = tegra_fb2_start;

	if (tegra_carveout_size && tegra_carveout_start < tegra_grhost_aperture)
		tegra_grhost_aperture = tegra_carveout_start;

	if (tegra_lp0_vec_size &&
	   (tegra_lp0_vec_start < memblock_end_of_DRAM())) {
		if (memblock_reserve(tegra_lp0_vec_start, tegra_lp0_vec_size)) {
			pr_err("Failed to reserve lp0_vec %08llx@%08llx\n",
				(u64)tegra_lp0_vec_size,
				(u64)tegra_lp0_vec_start);
			tegra_lp0_vec_start = 0;
			tegra_lp0_vec_size = 0;
		}
		tegra_lp0_vec_relocate = false;
	} else
		tegra_lp0_vec_relocate = true;

#ifdef CONFIG_TEGRA_NVDUMPER
	if (nvdumper_reserved) {
		if (memblock_reserve(nvdumper_reserved, NVDUMPER_RESERVED_SIZE)) {
			pr_err("Failed to reserve nvdumper page %08lx@%08lx\n",
			       nvdumper_reserved, NVDUMPER_RESERVED_SIZE);
			nvdumper_reserved = 0;
		}
	}
#endif

#ifdef CONFIG_TEGRA_USE_NCT
	if (tegra_nck_size &&
	   (tegra_nck_start < memblock_end_of_DRAM())) {
		if (memblock_reserve(tegra_nck_start, tegra_nck_size)) {
			pr_err("Failed to reserve nck %08lx@%08lx\n",
				tegra_nck_size, tegra_nck_start);
			tegra_nck_start = 0;
			tegra_nck_size = 0;
		}
	}
#endif

	/*
	 * We copy the bootloader's framebuffer to the framebuffer allocated
	 * above, and then free this one.
	 * */
	if (tegra_bootloader_fb_size) {
		tegra_bootloader_fb_size = PAGE_ALIGN(tegra_bootloader_fb_size);
		if (memblock_reserve(tegra_bootloader_fb_start,
				tegra_bootloader_fb_size)) {
			pr_err("Failed to reserve bootloader frame buffer "
				"%08llx@%08llx\n",
				(u64)tegra_bootloader_fb_size,
				(u64)tegra_bootloader_fb_start);
			tegra_bootloader_fb_start = 0;
			tegra_bootloader_fb_size = 0;
		}
	}

	if (tegra_bootloader_fb2_size) {
		tegra_bootloader_fb2_size =
				PAGE_ALIGN(tegra_bootloader_fb2_size);
		if (memblock_reserve(tegra_bootloader_fb2_start,
				tegra_bootloader_fb2_size)) {
			pr_err("Failed to reserve bootloader fb2 %08llx@%08llx\n",
				(u64)tegra_bootloader_fb2_size,
				(u64)tegra_bootloader_fb2_start);
			tegra_bootloader_fb2_start = 0;
			tegra_bootloader_fb2_size = 0;
		}
	}

	pr_info("Tegra reserved memory:\n"
		"LP0:                    %08llx - %08llx\n"
		"Bootloader framebuffer: %08llx - %08llx\n"
		"Bootloader framebuffer2: %08llx - %08llx\n"
		"Framebuffer:            %08llx - %08llx\n"
		"2nd Framebuffer:        %08llx - %08llx\n"
		"Carveout:               %08llx - %08llx\n"
		"Vpr:                    %08llx - %08llx\n"
		"Tsec:                   %08llx - %08llx\n"
		,
		(u64)tegra_lp0_vec_start,
		(u64)(tegra_lp0_vec_size ?
			tegra_lp0_vec_start + tegra_lp0_vec_size - 1 : 0),
		(u64)tegra_bootloader_fb_start,
		(u64)(tegra_bootloader_fb_size ?
			tegra_bootloader_fb_start +
			tegra_bootloader_fb_size - 1 : 0),
		(u64)tegra_bootloader_fb2_start,
		(u64)(tegra_bootloader_fb2_size ?
			tegra_bootloader_fb2_start +
			tegra_bootloader_fb2_size - 1 : 0),
		(u64)tegra_fb_start,
		(u64)(tegra_fb_size ?
			tegra_fb_start + tegra_fb_size - 1 : 0),
		(u64)tegra_fb2_start,
		(u64)(tegra_fb2_size ?
			tegra_fb2_start + tegra_fb2_size - 1 : 0),
		(u64)tegra_carveout_start,
		(u64)(tegra_carveout_size ?
			tegra_carveout_start + tegra_carveout_size - 1 : 0),
		(u64)tegra_vpr_start,
		(u64)(tegra_vpr_size ?
			tegra_vpr_start + tegra_vpr_size - 1 : 0),
		(u64)tegra_tsec_start,
		(u64)(tegra_tsec_size ?
			tegra_tsec_start + tegra_tsec_size - 1 : 0)
		);

#ifdef CONFIG_TEGRA_NVDUMPER
	if (nvdumper_reserved) {
		pr_info("Nvdumper:               %08lx - %08lx\n",
			nvdumper_reserved,
			nvdumper_reserved + NVDUMPER_RESERVED_SIZE - 1);
	}
#endif

#ifdef CONFIG_TEGRA_USE_NCT
	if (tegra_nck_size) {
		pr_info("Nck:                    %08lx - %08lx\n",
			tegra_nck_start,
			tegra_nck_size ?
				tegra_nck_start + tegra_nck_size - 1 : 0);
	}
#endif

#ifdef CONFIG_PSTORE_RAM
	tegra_reserve_ramoops_memory(RAMOOPS_MEM_SIZE);
#endif

	/* Keep these at the end */
	if (tegra_vpr_resize && carveout_size &&
	    !dev_get_cma_area(&tegra_generic_cma_dev)) {
		if (dma_declare_contiguous(&tegra_generic_cma_dev,
			carveout_size, 0, memblock_end_of_4G()))
			pr_err("dma_declare_contiguous failed for generic\n");
		tegra_carveout_size = carveout_size;
	}

	if (tegra_vpr_resize && tegra_vpr_size &&
	    !dev_get_cma_area(&tegra_vpr_cma_dev))
		if (dma_declare_contiguous(&tegra_vpr_cma_dev,
			tegra_vpr_size, 0, memblock_end_of_4G()))
			pr_err("dma_declare_contiguous failed VPR carveout\n");

	tegra_fb_linear_set(map);
}

void tegra_reserve4(ulong carveout_size, ulong fb_size,
		       ulong fb2_size, ulong vpr_size)
{
	if (tegra_vpr_resize) {
		tegra_vpr_start = 0;
		tegra_vpr_size = vpr_size;
	}
	tegra_reserve(carveout_size, fb_size, fb2_size);
}

void tegra_get_fb_resource(struct resource *fb_res)
{
	fb_res->start = (resource_size_t) tegra_fb_start;
	fb_res->end = fb_res->start +
			(resource_size_t) tegra_fb_size - 1;
}

void tegra_get_fb2_resource(struct resource *fb2_res)
{
	fb2_res->start = (resource_size_t) tegra_fb2_start;
	fb2_res->end = fb2_res->start +
			(resource_size_t) tegra_fb2_size - 1;
}



int __init tegra_register_fuse(void)
{
	return platform_device_register(&tegra_fuse_device);
}

int __init tegra_release_bootloader_fb(void)
{
	/* Since bootloader fb is reserved in common.c, it is freed here. */
	if (tegra_bootloader_fb_size) {
		if (memblock_free(tegra_bootloader_fb_start,
						tegra_bootloader_fb_size))
			pr_err("Failed to free bootloader fb.\n");
		else
			free_bootmem_late(tegra_bootloader_fb_start,
						tegra_bootloader_fb_size);
	}
	if (tegra_bootloader_fb2_size) {
		if (memblock_free(tegra_bootloader_fb2_start,
						tegra_bootloader_fb2_size))
			pr_err("Failed to free bootloader fb2.\n");
		else
			free_bootmem_late(tegra_bootloader_fb2_start,
						tegra_bootloader_fb2_size);
	}
	return 0;
}
late_initcall(tegra_release_bootloader_fb);

static const char *tegra_revision_name[TEGRA_REVISION_MAX] = {
	[TEGRA_REVISION_UNKNOWN] = "unknown",
	[TEGRA_REVISION_A01]     = "A01",
	[TEGRA_REVISION_A01q]    = "A01Q",
	[TEGRA_REVISION_A02]     = "A02",
	[TEGRA_REVISION_A03]     = "A03",
	[TEGRA_REVISION_A03p]    = "A03 prime",
	[TEGRA_REVISION_A04]     = "A04",
	[TEGRA_REVISION_A04p]    = "A04 prime",
};

static const char * __init tegra_get_revision(void)
{
	return kasprintf(GFP_KERNEL, "%s", tegra_revision_name[tegra_revision]);
}

static const char * __init tegra_get_family(void)
{
	void __iomem *chip_id = IO_ADDRESS(TEGRA_APB_MISC_BASE) + 0x804;
	u32 cid = readl(chip_id);
	cid = (cid >> 8) & 0xFF;

	switch (cid) {
	case TEGRA_CHIPID_TEGRA2:
		cid = 2;
		break;
	case TEGRA_CHIPID_TEGRA3:
		cid = 3;
		break;
	case TEGRA_CHIPID_TEGRA11:
		cid = 11;
		break;
	case TEGRA_CHIPID_TEGRA12:
		cid = 12;
		break;
	case TEGRA_CHIPID_TEGRA13:
		cid = 13;
		break;
	case TEGRA_CHIPID_TEGRA14:
		cid = 14;
		break;
	case TEGRA_CHIPID_TEGRA21:
		cid = 21;
		break;

	case TEGRA_CHIPID_UNKNOWN:
	default:
		cid = 0;
	}
	return kasprintf(GFP_KERNEL, "Tegra%d", cid);
}

static const char * __init tegra_get_soc_id(void)
{
	int package_id = tegra_package_id();

	return kasprintf(GFP_KERNEL, "REV=%s:SKU=0x%x:PID=0x%x",
		tegra_revision_name[tegra_revision],
		tegra_get_sku_id(), package_id);
}

static void __init tegra_soc_info_populate(struct soc_device_attribute
	*soc_dev_attr, const char *machine)
{
	soc_dev_attr->soc_id = tegra_get_soc_id();
	soc_dev_attr->machine  = machine;
	soc_dev_attr->family   = tegra_get_family();
	soc_dev_attr->revision = tegra_get_revision();
}

int __init tegra_soc_device_init(const char *machine)
{
	struct soc_device *soc_dev;
	struct soc_device_attribute *soc_dev_attr;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENOMEM;

	tegra_soc_info_populate(soc_dev_attr, machine);

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR_OR_NULL(soc_dev)) {
		kfree(soc_dev_attr);
		return -1;
	}

	return 0;
}

void __init tegra_init_late(void)
{
#ifndef CONFIG_COMMON_CLK
	tegra_clk_debugfs_init();
#endif
	tegra_powergate_debugfs_init();
}

int tegra_split_mem_active(void)
{
	return tegra_split_mem_set;
}

static int __init set_tegra_split_mem(char *options)
{
	tegra_split_mem_set = 1;
	return 0;
}
early_param("tegra_split_mem", set_tegra_split_mem);

unsigned long int tegra_dfll_boot_req_khz(void)
{
	return dfll_boot_req_khz;
}

static int __init dfll_freq_cmd_line(char *line)
{
	int status = kstrtoul(line, 0, &dfll_boot_req_khz);
	if (status) {
		pr_err("\n%s:Error in parsing dfll_boot_req_khz:%d\n",
			__func__, status);
		dfll_boot_req_khz = 0;
	}
	return status;
}
early_param("dfll_boot_req_khz", dfll_freq_cmd_line);

void __init display_tegra_dt_info(void)
{
	int ret_d;
	int ret_t;
	unsigned long dt_root;
	const char *dts_fname;
	const char *dtb_bdate;
	const char *dtb_btime;

	dt_root = of_get_flat_dt_root();

	dts_fname = of_get_flat_dt_prop(dt_root, "nvidia,dtsfilename", NULL);
	if (dts_fname)
		pr_info("DTS File Name: %s\n", dts_fname);
	else
		pr_info("DTS File Name: <unknown>\n");

	ret_d = of_property_read_string_index(of_find_node_by_path("/"),
			"nvidia,dtbbuildtime", 0, &dtb_bdate);
	ret_t = of_property_read_string_index(of_find_node_by_path("/"),
			"nvidia,dtbbuildtime", 1, &dtb_btime);
	if (!ret_d && !ret_t)
		pr_info("DTB Build time: %s %s\n", dtb_bdate, dtb_btime);
	else
		pr_info("DTB Build time: <unknown>\n");
}

static void tegra_get_bl_reset_status(void)
{
	struct device_node *reset_info;
	u32 prop_val;
	int err;

	reset_info = of_find_node_by_path("/chosen/reset");
	if (reset_info) {
		err = of_property_read_u32(reset_info, "pmc_reset_status",
				&prop_val);
		if (err < 0)
			goto out;
		else
			pr_info("BL: PMC reset status reg: 0x%x\n",
					prop_val);

		err = of_property_read_u32(reset_info, "pmic_reset_status",
				&prop_val);
		if (err < 0)
			goto out;
		else
			pr_info("BL: PMIC poweroff Event Recorder: 0x%x\n",
					prop_val);
	}
out:
	return;
}

static int __init tegra_get_last_reset_reason(void)
{
#define PMC_RST_STATUS 0x1b4
#define RESET_STR(REASON) "last reset is due to "#REASON"\n"
	char *reset_reason[] = {
		RESET_STR(power on reset),
		RESET_STR(watchdog timeout),
		RESET_STR(sensor),
		RESET_STR(software reset),
		RESET_STR(deep sleep reset),
	};

	u32 val = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_RST_STATUS) & 0x7;
	if (val >= ARRAY_SIZE(reset_reason))
		pr_info("last reset value is invalid 0x%x\n", val);
	else {
		pr_info("%s\n", reset_reason[val]);
		pr_info("KERNEL: PMC reset status reg: 0x%x\n", val);
	}

	tegra_get_bl_reset_status();
	return 0;
}
late_initcall(tegra_get_last_reset_reason);

int tegra_state_idx_from_name(char *state_name)
{
	struct device_node *state_node, *cpu_node;
	int i, id_found = 0;

	/*
	 * We get the idle states for the first logical cpu in the
	 * cpu_online_mask and parse all entries to get the requested
	 * index value. All the cpu idle states are uniform
	 * across CPUs, so we can use any CPU number to get the index
	 * value.
	 */
	cpu_node = of_cpu_device_node_get(0);

	for (i = 0; ; i++) {
		state_node = of_parse_phandle(cpu_node, "cpu-idle-states", i);
		if (!state_node)
			break;

		if (!strcmp(state_node->name, state_name))
			id_found = 1;

		of_node_put(state_node);

		if (id_found)
			break;
	}

	of_node_put(cpu_node);

	/*
	 * The state index needs to be incremented by 1 to get to the
	 * actual value in the device tree. Index value of '0' points
	 * to the default 'wfi' state which is not part of the DT. Hence
	 * we increment the actual ID by one to compensate for the default
	 * state.
	 */
	return id_found ? (i + 1) : 0;
}
