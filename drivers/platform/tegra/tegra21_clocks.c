/*
 * arch/arm/mach-tegra/tegra21_clocks.c
 *
 * Copyright (C) 2013-2016 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/tegra_soctherm.h>
#include <soc/tegra/tegra_bpmp.h>
#include <dt-bindings/clk/tegra210-clk.h>

#include <asm/clkdev.h>
#include <asm/arch_timer.h>

#include <mach/edp.h>
#include <mach/hardware.h>

#include <linux/platform/tegra/tegra_emc.h>
#include <linux/platform/tegra/mc.h>

#include <linux/platform/tegra/clock.h>
#include "tegra_clocks_ops.h"
#include <linux/platform/tegra/dvfs.h>
#include "pm.h"
#include "sleep.h"
#include "devices.h"
#include <linux/platform/tegra/tegra_cl_dvfs.h>

#undef USE_PLLE_SS
#define USE_PLLE_SS 1

#define RST_DEVICES_L			0x004
#define RST_DEVICES_H			0x008
#define RST_DEVICES_U			0x00C
#define RST_DEVICES_V			0x358
#define RST_DEVICES_W			0x35C
#define RST_DEVICES_X			0x28C
#define RST_DEVICES_Y			0x2A4
#define RST_DEVICES_SET_L		0x300
#define RST_DEVICES_CLR_L		0x304
#define RST_DEVICES_SET_V		0x430
#define RST_DEVICES_CLR_V		0x434
#define RST_DEVICES_SET_X		0x290
#define RST_DEVICES_CLR_X		0x294
#define RST_DEVICES_SET_Y		0x2A8
#define RST_DEVICES_CLR_Y		0x2AC
#define RST_DEVICES_NUM			7

#define CLK_OUT_ENB_L			0x010
#define CLK_OUT_ENB_H			0x014
#define CLK_OUT_ENB_U			0x018
#define CLK_OUT_ENB_V			0x360
#define CLK_OUT_ENB_W			0x364
#define CLK_OUT_ENB_X			0x280
#define CLK_OUT_ENB_Y			0x298
#define CLK_OUT_ENB_SET_L		0x320
#define CLK_OUT_ENB_CLR_L		0x324
#define CLK_OUT_ENB_SET_V		0x440
#define CLK_OUT_ENB_CLR_V		0x444
#define CLK_OUT_ENB_SET_X		0x284
#define CLK_OUT_ENB_CLR_X		0x288
#define CLK_OUT_ENB_SET_Y		0x29C
#define CLK_OUT_ENB_CLR_Y		0x2A0
#define CLK_OUT_ENB_NUM			7

#define CLK_OUT_ENB_L_RESET_MASK	0xdcd7dff9
#define CLK_OUT_ENB_H_RESET_MASK	0x87d1f3e7
#define CLK_OUT_ENB_U_RESET_MASK	0xf3fed3fa
#define CLK_OUT_ENB_V_RESET_MASK	0xffc18cfb
#define CLK_OUT_ENB_W_RESET_MASK	0x793fb7ff
#define CLK_OUT_ENB_X_RESET_MASK	0x3fe66fff
#define CLK_OUT_ENB_Y_RESET_MASK	0xfc1fc7ff

#define RST_DEVICES_V_SWR_CPULP_RST_DIS	(0x1 << 1) /* Reserved on Tegra11 */
#define CLK_OUT_ENB_V_CLK_ENB_CPULP_EN	(0x1 << 1)

#define PERIPH_CLK_TO_BIT(c)		(1 << (c->u.periph.clk_num % 32))
#define PERIPH_CLK_TO_RST_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_L, RST_DEVICES_V, \
		RST_DEVICES_X, RST_DEVICES_Y, 4)
#define PERIPH_CLK_TO_RST_SET_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_SET_L, RST_DEVICES_SET_V, \
		RST_DEVICES_SET_X, RST_DEVICES_SET_Y, 8)
#define PERIPH_CLK_TO_RST_CLR_REG(c)	\
	periph_clk_to_reg((c), RST_DEVICES_CLR_L, RST_DEVICES_CLR_V, \
		RST_DEVICES_CLR_X, RST_DEVICES_CLR_Y, 8)

#define PERIPH_CLK_TO_ENB_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_L, CLK_OUT_ENB_V, \
		CLK_OUT_ENB_X, CLK_OUT_ENB_Y, 4)
#define PERIPH_CLK_TO_ENB_SET_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_SET_L, CLK_OUT_ENB_SET_V, \
		CLK_OUT_ENB_SET_X, CLK_OUT_ENB_SET_Y, 8)
#define PERIPH_CLK_TO_ENB_CLR_REG(c)	\
	periph_clk_to_reg((c), CLK_OUT_ENB_CLR_L, CLK_OUT_ENB_CLR_V, \
		CLK_OUT_ENB_CLR_X, CLK_OUT_ENB_CLR_Y, 8)

#define CLK_MASK_ARM			0x44
#define MISC_CLK_ENB			0x48

#define OSC_CTRL			0x50
#define OSC_CTRL_OSC_FREQ_MASK		(0xF<<28)
#define OSC_CTRL_OSC_FREQ_12MHZ		(0x8<<28)
#define OSC_CTRL_OSC_FREQ_13MHZ		(0x0<<28)
#define OSC_CTRL_OSC_FREQ_19_2MHZ	(0x4<<28)
#define OSC_CTRL_OSC_FREQ_38_4MHZ	(0x5<<28)
#define OSC_CTRL_MASK			(0x3f2 | OSC_CTRL_OSC_FREQ_MASK)

#define OSC_CTRL_PLL_REF_DIV_MASK	(3<<26)
#define OSC_CTRL_PLL_REF_DIV_1		(0<<26)
#define OSC_CTRL_PLL_REF_DIV_2		(1<<26)
#define OSC_CTRL_PLL_REF_DIV_4		(2<<26)

#define PERIPH_CLK_SOURCE_I2S1		0x100
#define PERIPH_CLK_SOURCE_EMC		0x19c
#define PERIPH_CLK_SOURCE_EMC_MC_SAME	(1<<16)
#define PERIPH_CLK_SOURCE_EMC_DIV2_EN	(1<<15)
#define PERIPH_CLK_SOURCE_QSPI_DIV2_EN	(1<<8)
#define PERIPH_CLK_SOURCE_LA		0x1f8
#define PERIPH_CLK_SOURCE_NUM1 \
	((PERIPH_CLK_SOURCE_LA - PERIPH_CLK_SOURCE_I2S1) / 4)

#define PERIPH_CLK_SOURCE_MSELECT	0x3b4
#define PERIPH_CLK_SOURCE_SE		0x42c
#define PERIPH_CLK_SOURCE_NUM2 \
	((PERIPH_CLK_SOURCE_SE - PERIPH_CLK_SOURCE_MSELECT) / 4 + 1)

#define AUDIO_DLY_CLK			0x49c
#define AUDIO_SYNC_CLK_SPDIF		0x4b4
#define PERIPH_CLK_SOURCE_NUM3 \
	((AUDIO_SYNC_CLK_SPDIF - AUDIO_DLY_CLK) / 4 + 1)

#define SPARE_REG			0x55c
#define SPARE_REG_CLK_M_DIVISOR_SHIFT	2
#define SPARE_REG_CLK_M_DIVISOR_MASK	(3 << SPARE_REG_CLK_M_DIVISOR_SHIFT)

#define AUDIO_SYNC_CLK_DMIC1		0x560
#define AUDIO_SYNC_CLK_DMIC2		0x564
#define PERIPH_CLK_SOURCE_NUM4 \
	((AUDIO_SYNC_CLK_DMIC2 - AUDIO_SYNC_CLK_DMIC1) / 4 + 1)

#define PERIPH_CLK_SOURCE_XUSB_HOST	0x600
#define PERIPH_CLK_SOURCE_EMC_DLL	0x664
#define PERIPH_CLK_SOURCE_VIC		0x678
#define PERIPH_CLK_SOURCE_NUM5 \
	((PERIPH_CLK_SOURCE_VIC - PERIPH_CLK_SOURCE_XUSB_HOST) / 4)

#define PERIPH_CLK_SOURCE_SDMMC_LEGACY	0x694
#define PERIPH_CLK_SOURCE_NVENC		0x6a0
#define PERIPH_CLK_SOURCE_NUM6 \
	((PERIPH_CLK_SOURCE_NVENC - PERIPH_CLK_SOURCE_SDMMC_LEGACY) / 4 + 1)

#define AUDIO_SYNC_CLK_DMIC3		0x6b8
#define PERIPH_CLK_SOURCE_DBGAPB	0x718
#define PERIPH_CLK_SOURCE_NUM7 \
	((PERIPH_CLK_SOURCE_DBGAPB - AUDIO_SYNC_CLK_DMIC3) / 4 + 1)

#define PERIPH_CLK_SOURCE_NUM		(PERIPH_CLK_SOURCE_NUM1 + \
					 PERIPH_CLK_SOURCE_NUM2 + \
					 PERIPH_CLK_SOURCE_NUM3 + \
					 PERIPH_CLK_SOURCE_NUM4 + \
					 PERIPH_CLK_SOURCE_NUM5 + \
					 PERIPH_CLK_SOURCE_NUM6 + \
					 PERIPH_CLK_SOURCE_NUM7)

#define CPU_SOFTRST_CTRL		0x380
#define CPU_SOFTRST_CTRL1		0x384
#define CPU_SOFTRST_CTRL2		0x388

#define PERIPH_CLK_SOURCE_DIVU71_MASK	0xFF
#define PERIPH_CLK_SOURCE_DIVU16_MASK	0xFFFF
#define PERIPH_CLK_SOURCE_DIV_SHIFT	0
#define PERIPH_CLK_SOURCE_DIVIDLE_SHIFT	8
#define PERIPH_CLK_SOURCE_DIVIDLE_VAL	50
#define PERIPH_CLK_UART_DIV_ENB		(1<<24)
#define PERIPH_CLK_VI_SEL_EX_SHIFT	24
#define PERIPH_CLK_VI_SEL_EX_MASK	(0x3<<PERIPH_CLK_VI_SEL_EX_SHIFT)
#define PERIPH_CLK_NAND_DIV_EX_ENB	(1<<8)
#define PERIPH_CLK_DTV_POLARITY_INV	(1<<25)

#define AUDIO_SYNC_SOURCE_MASK		0x0F
#define AUDIO_SYNC_DISABLE_BIT		0x10
#define AUDIO_SYNC_TAP_NIBBLE_SHIFT(c)	((c->reg_shift - 24) * 4)

#define PERIPH_CLK_SOR_CLK_SEL_SHIFT	14
#define PERIPH_CLK_SOR0_CLK_SEL_MASK	(0x1<<PERIPH_CLK_SOR_CLK_SEL_SHIFT)
#define PERIPH_CLK_SOR1_CLK_SEL_MASK	(0x3<<PERIPH_CLK_SOR_CLK_SEL_SHIFT)

/* Secondary PLL dividers */
#define PLL_OUT_RATIO_MASK		(0xFF<<8)
#define PLL_OUT_RATIO_SHIFT		8
#define PLL_OUT_OVERRIDE		(1<<2)
#define PLL_OUT_CLKEN			(1<<1)
#define PLL_OUT_RESET_DISABLE		(1<<0)

/* PLLC, PLLC2, PLLC3 and PLLA1 */
#define PLLCX_USE_DYN_RAMP		0
#define PLLCX_BASE_LOCK			(1 << 26)

#define PLLCX_MISC0_RESET		(1 << 30)
#define PLLCX_MISC0_LOOP_CTRL_SHIFT	0
#define PLLCX_MISC0_LOOP_CTRL_MASK	(0x3 << PLLCX_MISC0_LOOP_CTRL_SHIFT)

#define PLLCX_MISC1_IDDQ		(1 << 27)

#define PLLCX_MISC0_DEFAULT_VALUE	0x40080000
#define PLLCX_MISC0_WRITE_MASK		0x400ffffb
#define PLLCX_MISC1_DEFAULT_VALUE	0x08000000
#define PLLCX_MISC1_WRITE_MASK		0x08003cff
#define PLLCX_MISC2_DEFAULT_VALUE	0x1f720f05
#define PLLCX_MISC2_WRITE_MASK		0xffffff17
#define PLLCX_MISC3_DEFAULT_VALUE	0x000000c4
#define PLLCX_MISC3_WRITE_MASK		0x00ffffff

/* PLLA */
#define PLLA_BASE_LOCK			(1 << 27)
#define PLLA_BASE_IDDQ			(1 << 25)

#define PLLA_MISC0_LOCK_ENABLE		(1 << 28)
#define PLLA_MISC0_LOCK_OVERRIDE	(1 << 27)

#define PLLA_MISC2_EN_SDM		(1 << 26)
#define PLLA_MISC2_EN_DYNRAMP		(1 << 25)

#define PLLA_MISC0_DEFAULT_VALUE	0x12000020
#define PLLA_MISC0_WRITE_MASK		0x7fffffff
#define PLLA_MISC2_DEFAULT_VALUE	0x0
#define PLLA_MISC2_WRITE_MASK		0x06ffffff

/* PLLD */
#define PLLD_BASE_LOCK			(1 << 27)
#define PLLD_BASE_DSI_MUX_SHIFT		25
#define PLLD_BASE_DSI_MUX_MASK		(0x1 << PLLD_BASE_DSI_MUX_SHIFT)
#define PLLD_BASE_CSI_CLKSOURCE		(1 << 23)

#define PLLD_MISC0_DSI_CLKENABLE	(1 << 21)
#define PLLD_MISC0_IDDQ			(1 << 20)
#define PLLD_MISC0_LOCK_ENABLE		(1 << 18)
#define PLLD_MISC0_LOCK_OVERRIDE	(1 << 17)
#define PLLD_MISC0_EN_SDM		(1 << 16)

#define PLLD_MISC0_DEFAULT_VALUE	0x00140000
#define PLLD_MISC0_WRITE_MASK		0x3ff7ffff
#define PLLD_MISC1_DEFAULT_VALUE	0x20
#define PLLD_MISC1_WRITE_MASK		0x00ffffff

/* PLLD2 and PLLDP  and PLLC4 */
#define PLLDSS_BASE_LOCK		(1 << 27)
#define PLLDSS_BASE_LOCK_OVERRIDE	(1 << 24)
#define PLLDSS_BASE_IDDQ		(1 << 18)
#define PLLDSS_BASE_REF_SEL_SHIFT	25
#define PLLDSS_BASE_REF_SEL_MASK	(0x3 << PLLDSS_BASE_REF_SEL_SHIFT)

#define PLLDSS_MISC0_LOCK_ENABLE	(1 << 30)

#define PLLDSS_MISC1_CFG_EN_SDM		(1 << 31)
#define PLLDSS_MISC1_CFG_EN_SSC		(1 << 30)

#define PLLD2_MISC0_DEFAULT_VALUE	0x40000020
#define PLLD2_MISC1_CFG_DEFAULT_VALUE	0x10000000
#define PLLD2_MISC2_CTRL1_DEFAULT_VALUE	0x0
#define PLLD2_MISC3_CTRL2_DEFAULT_VALUE	0x0

#define PLLDP_MISC0_DEFAULT_VALUE	0x40000020
#define PLLDP_MISC1_CFG_DEFAULT_VALUE	0xc0000000
#define PLLDP_MISC2_CTRL1_DEFAULT_VALUE	0xf400f0da
#define PLLDP_MISC3_CTRL2_DEFAULT_VALUE	0x2004f400

#define PLLDSS_MISC0_WRITE_MASK		0x47ffffff
#define PLLDSS_MISC1_CFG_WRITE_MASK	0xf8000000
#define PLLDSS_MISC2_CTRL1_WRITE_MASK	0xffffffff
#define PLLDSS_MISC3_CTRL2_WRITE_MASK	0xffffffff

#define PLLC4_MISC0_DEFAULT_VALUE	0x40000000

/* PLLRE */
#define PLLRE_MISC0_LOCK_ENABLE		(1 << 30)
#define PLLRE_MISC0_LOCK_OVERRIDE	(1 << 29)
#define PLLRE_MISC0_LOCK		(1 << 27)
#define PLLRE_MISC0_IDDQ		(1 << 24)

#define PLLRE_BASE_DEFAULT_VALUE	0x0
#define PLLRE_MISC0_DEFAULT_VALUE	0x41000000

#define PLLRE_BASE_DEFAULT_MASK		0x1c000000
#define PLLRE_MISC0_WRITE_MASK		0x67ffffff

/* PLLU */
#define PLLU_BASE_LOCK			(1 << 27)
#define PLLU_BASE_OVERRIDE		(1 << 24)
#define PLLU_BASE_CLKENABLE_USB		(1 << 21)

#define PLLU_MISC0_IDDQ			(1 << 31)
#define PLLU_MISC0_LOCK_ENABLE		(1 << 29)
#define PLLU_MISC1_LOCK_OVERRIDE	(1 << 0)

#define PLLU_MISC0_DEFAULT_VALUE	0xa0000000
#define PLLU_MISC1_DEFAULT_VALUE	0x0

#define PLLU_MISC0_WRITE_MASK		0xbfffffff
#define PLLU_MISC1_WRITE_MASK		0x00000007

/* PLLP */
#define PLLP_BASE_OVERRIDE		(1 << 28)
#define PLLP_BASE_LOCK			(1 << 27)

#define PLLP_MISC0_LOCK_ENABLE		(1 << 18)
#define PLLP_MISC0_LOCK_OVERRIDE	(1 << 17)
#define PLLP_MISC0_IDDQ			(1 << 3)

#define PLLP_MISC1_HSIO_EN_SHIFT	29
#define PLLP_MISC1_HSIO_EN		(1 << PLLP_MISC1_HSIO_EN_SHIFT)
#define PLLP_MISC1_XUSB_EN_SHIFT	28
#define PLLP_MISC1_XUSB_EN		(1 << PLLP_MISC1_XUSB_EN_SHIFT)

#define PLLP_MISC0_DEFAULT_VALUE	0x00040008
#define PLLP_MISC1_DEFAULT_VALUE	0x0

#define PLLP_MISC0_WRITE_MASK		0xdc6000f
#define PLLP_MISC1_WRITE_MASK		0x70ffffff

/* PLLX */
#define PLLX_USE_DYN_RAMP		1
#define PLLX_BASE_LOCK			(1 << 27)

#define PLLX_MISC0_FO_G_DISABLE		(0x1 << 28)
#define PLLX_MISC0_LOCK_ENABLE		(0x1 << 18)

#define PLLX_MISC2_DYNRAMP_STEPB_SHIFT	24
#define PLLX_MISC2_DYNRAMP_STEPB_MASK	(0xFF << PLLX_MISC2_DYNRAMP_STEPB_SHIFT)
#define PLLX_MISC2_DYNRAMP_STEPA_SHIFT	16
#define PLLX_MISC2_DYNRAMP_STEPA_MASK	(0xFF << PLLX_MISC2_DYNRAMP_STEPA_SHIFT)
#define PLLX_MISC2_NDIV_NEW_SHIFT	8
#define PLLX_MISC2_NDIV_NEW_MASK	(0xFF << PLLX_MISC2_NDIV_NEW_SHIFT)
#define PLLX_MISC2_LOCK_OVERRIDE	(0x1 << 4)
#define PLLX_MISC2_DYNRAMP_DONE		(0x1 << 2)
#define PLLX_MISC2_EN_DYNRAMP		(0x1 << 0)

#define PLLX_MISC3_IDDQ			(0x1 << 3)

#define PLLX_MISC0_DEFAULT_VALUE	PLLX_MISC0_LOCK_ENABLE
#define PLLX_MISC0_WRITE_MASK		0x10c40000
#define PLLX_MISC1_DEFAULT_VALUE	0x20
#define PLLX_MISC1_WRITE_MASK		0x00ffffff
#define PLLX_MISC2_DEFAULT_VALUE	0x0
#define PLLX_MISC2_WRITE_MASK		0xffffff11
#define PLLX_MISC3_DEFAULT_VALUE	PLLX_MISC3_IDDQ
#define PLLX_MISC3_WRITE_MASK		0x01ff0f0f
#define PLLX_MISC4_DEFAULT_VALUE	0x0
#define PLLX_MISC4_WRITE_MASK		0x8000ffff
#define PLLX_MISC5_DEFAULT_VALUE	0x0
#define PLLX_MISC5_WRITE_MASK		0x0000ffff

#define PLLX_HW_CTRL_CFG		0x548
#define PLLX_HW_CTRL_CFG_SWCTRL		(0x1 << 0)

/* PLLM */
#define PLLM_BASE_LOCK			(1 << 27)

#define PLLM_MISC0_SYNCMUX_CTRL_SHIFT	10
#define PLLM_MISC0_SYNCMUX_CTRL_MASK	(0xF << PLLM_MISC0_SYNCMUX_CTRL_SHIFT)
#define PLLM_MISC0_IDDQ			(1 << 5)
#define PLLM_MISC0_LOCK_ENABLE		(1 << 4)
#define PLLM_MISC0_LOCK_OVERRIDE	(1 << 3)

#define PMC_PLLP_WB0_OVERRIDE			0xf8
#define PMC_PLLP_WB0_OVERRIDE_PLLM_IDDQ		(1 << 14)
#define PMC_PLLP_WB0_OVERRIDE_PLLM_SEL_VCO	(1 << 13)
#define PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE	(1 << 12)
#define PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE	(1 << 11)

/* M, N layout for PLLM override and base registers are the same */
#define PMC_PLLM_WB0_OVERRIDE			0x1dc

/* PDIV override and base layouts are different */
#define PMC_PLLM_WB0_OVERRIDE_2			0x2b0
#define PMC_PLLM_WB0_OVERRIDE_2_DIVP_SHIFT	27
#define PMC_PLLM_WB0_OVERRIDE_2_DIVP_MASK	(0x1F << 27)

/* PLLMB */
#define PLLMB_BASE_LOCK			(1 << 27)

#define PLLMB_MISC0_LOCK_OVERRIDE	(1 << 18)
#define PLLMB_MISC0_IDDQ		(1 << 17)
#define PLLMB_MISC0_LOCK_ENABLE		(1 << 16)

#define PLLMB_MISC0_DEFAULT_VALUE	0x00030000
#define PLLMB_MISC0_WRITE_MASK		0x0007ffff

#define SUPER_CLK_MUX			0x00
#define SUPER_STATE_SHIFT		28
#define SUPER_STATE_MASK		(0xF << SUPER_STATE_SHIFT)
#define SUPER_STATE_STANDBY		(0x0 << SUPER_STATE_SHIFT)
#define SUPER_STATE_IDLE		(0x1 << SUPER_STATE_SHIFT)
#define SUPER_STATE_RUN			(0x2 << SUPER_STATE_SHIFT)
#define SUPER_STATE_IRQ			(0x3 << SUPER_STATE_SHIFT)
#define SUPER_STATE_FIQ			(0x4 << SUPER_STATE_SHIFT)
#define SUPER_SOURCE_MASK		0xF
#define	SUPER_FIQ_SOURCE_SHIFT		12
#define	SUPER_IRQ_SOURCE_SHIFT		8
#define	SUPER_RUN_SOURCE_SHIFT		4
#define	SUPER_IDLE_SOURCE_SHIFT		0

#define SUPER_CLK_DIVIDER		0x04
#define SUPER_CLOCK_DIV_U71_SHIFT	16
#define SUPER_CLOCK_DIV_U71_MASK	(0xff << SUPER_CLOCK_DIV_U71_SHIFT)

#define SUPER_SKIPPER_ENABLE		(1 << 31)
#define SUPER_SKIPPER_TERM_SIZE		8
#define SUPER_SKIPPER_MUL_SHIFT		8
#define SUPER_SKIPPER_MUL_MASK		(((1 << SUPER_SKIPPER_TERM_SIZE) - 1) \
					<< SUPER_SKIPPER_MUL_SHIFT)
#define SUPER_SKIPPER_DIV_SHIFT		0
#define SUPER_SKIPPER_DIV_MASK		(((1 << SUPER_SKIPPER_TERM_SIZE) - 1) \
					<< SUPER_SKIPPER_DIV_SHIFT)

#define BUS_CLK_DISABLE			(1<<3)
#define BUS_CLK_DIV_MASK		0x3

#define PMC_CTRL			0x0
 #define PMC_CTRL_BLINK_ENB		(1 << 7)

#define PMC_DPD_PADS_ORIDE		0x1c
 #define PMC_DPD_PADS_ORIDE_BLINK_ENB	(1 << 20)

#define PMC_BLINK_TIMER_DATA_ON_SHIFT	0
#define PMC_BLINK_TIMER_DATA_ON_MASK	0x7fff
#define PMC_BLINK_TIMER_ENB		(1 << 15)
#define PMC_BLINK_TIMER_DATA_OFF_SHIFT	16
#define PMC_BLINK_TIMER_DATA_OFF_MASK	0xffff

#define UTMIP_PLL_CFG2					0x488
#define UTMIP_PLL_CFG2_STABLE_COUNT(x)			(((x) & 0xfff) << 6)
#define UTMIP_PLL_CFG2_ACTIVE_DLY_COUNT(x)		(((x) & 0x3f) << 18)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERDOWN	(1 << 0)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERUP		(1 << 1)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERDOWN	(1 << 2)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERUP		(1 << 3)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERDOWN	(1 << 4)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_C_POWERUP		(1 << 5)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERDOWN	(1 << 24)
#define UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERUP		(1 << 25)

#define UTMIP_PLL_CFG1					0x484
#define UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(x)		(((x) & 0x1f) << 27)
#define UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(x)		(((x) & 0xfff) << 0)
#define UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERUP	(1 << 15)
#define UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN	(1 << 14)
#define UTMIP_PLL_CFG1_FORCE_PLL_ACTIVE_POWERDOWN	(1 << 12)
#define UTMIP_PLL_CFG1_FORCE_PLLU_POWERUP		(1 << 17)
#define UTMIP_PLL_CFG1_FORCE_PLLU_POWERDOWN		(1 << 16)

/* PLLE */
#define PLLE_BASE_ENABLE		(1 << 31)
#define PLLE_BASE_LOCK_OVERRIDE		(1 << 30)
#define PLLE_BASE_DIVCML_SHIFT		24
#define PLLE_BASE_DIVCML_MASK		(0x1F<<PLLE_BASE_DIVCML_SHIFT)
#define PLLE_BASE_DIVN_SHIFT		8
#define PLLE_BASE_DIVN_MASK		(0xFF<<PLLE_BASE_DIVN_SHIFT)
#define PLLE_BASE_DIVM_SHIFT		0
#define PLLE_BASE_DIVM_MASK		(0xFF<<PLLE_BASE_DIVM_SHIFT)

#define PLLE_MISC_IDDQ_SW_CTRL		(1<<14)
#define PLLE_MISC_IDDQ_SW_VALUE		(1<<13)
#define PLLE_MISC_LOCK			(1<<11)
#define PLLE_MISC_LOCK_ENABLE		(1<<9)
#define PLLE_MISC_PLLE_PTS		(1<<8)
#define PLLE_MISC_VREG_BG_CTRL_SHIFT	4
#define PLLE_MISC_VREG_BG_CTRL_MASK	(0x3<<PLLE_MISC_VREG_BG_CTRL_SHIFT)
#define PLLE_MISC_VREG_CTRL_SHIFT	2
#define PLLE_MISC_VREG_CTRL_MASK	(0x3<<PLLE_MISC_VREG_CTRL_SHIFT)

#define PLLE_SS_CTRL			0x68
#define	PLLE_SS_INCINTRV_SHIFT		24
#define	PLLE_SS_INCINTRV_MASK		(0x3f<<PLLE_SS_INCINTRV_SHIFT)
#define	PLLE_SS_INC_SHIFT		16
#define	PLLE_SS_INC_MASK		(0xff<<PLLE_SS_INC_SHIFT)
#define	PLLE_SS_CNTL_INVERT		(0x1 << 15)
#define	PLLE_SS_CNTL_CENTER		(0x1 << 14)
#define	PLLE_SS_CNTL_SSC_BYP		(0x1 << 12)
#define	PLLE_SS_CNTL_INTERP_RESET	(0x1 << 11)
#define	PLLE_SS_CNTL_BYPASS_SS		(0x1 << 10)
#define	PLLE_SS_MAX_SHIFT		0
#define	PLLE_SS_MAX_MASK		(0x1ff<<PLLE_SS_MAX_SHIFT)
#define PLLE_SS_COEFFICIENTS_MASK	\
	(PLLE_SS_INCINTRV_MASK | PLLE_SS_INC_MASK | PLLE_SS_MAX_MASK)
#define PLLE_SS_COEFFICIENTS_VAL	\
	((0x23<<PLLE_SS_INCINTRV_SHIFT) | (0x1<<PLLE_SS_INC_SHIFT) | \
	 (0x21<<PLLE_SS_MAX_SHIFT))
#define PLLE_SS_DISABLE			(PLLE_SS_CNTL_SSC_BYP |\
	PLLE_SS_CNTL_INTERP_RESET | PLLE_SS_CNTL_BYPASS_SS)

#define PLLE_AUX			0x48c
#define PLLE_AUX_PLLRE_SEL		(1<<28)
#define PLLE_AUX_SEQ_STATE_SHIFT	26
#define PLLE_AUX_SEQ_STATE_MASK		(0x3<<PLLE_AUX_SEQ_STATE_SHIFT)
#define PLLE_AUX_SS_SEQ_INCLUDE		(1<<31)
#define PLLE_AUX_SEQ_START_STATE	(1<<25)
#define PLLE_AUX_SEQ_ENABLE		(1<<24)
#define PLLE_AUX_SS_SWCTL		(1<<6)
#define PLLE_AUX_ENABLE_SWCTL		(1<<4)
#define PLLE_AUX_USE_LOCKDET		(1<<3)
#define PLLE_AUX_PLLP_SEL		(1<<2)

#define PLLE_AUX_CML_SATA_ENABLE	(1<<1)

#define SOURCE_SATA			0x424

/* USB PLLs PD HW controls */
#define XUSBIO_PLL_CFG0				0x51c
#define XUSBIO_PLL_CFG0_SEQ_START_STATE		(1<<25)
#define XUSBIO_PLL_CFG0_SEQ_ENABLE		(1<<24)
#define XUSBIO_PLL_CFG0_PADPLL_USE_LOCKDET	(1<<6)
#define XUSBIO_PLL_CFG0_CLK_ENABLE_SWCTL	(1<<2)
#define XUSBIO_PLL_CFG0_PADPLL_RESET_SWCTL	(1<<0)

#define UTMIPLL_HW_PWRDN_CFG0			0x52c
#define UTMIPLL_HW_PWRDN_CFG0_SEQ_START_STATE	(1<<25)
#define UTMIPLL_HW_PWRDN_CFG0_SEQ_ENABLE	(1<<24)
#define UTMIPLL_HW_PWRDN_CFG0_USE_LOCKDET	(1<<6)
#define UTMIPLL_HW_PWRDN_CFG0_SEQ_RESET_INPUT_VALUE	(1<<5)
#define UTMIPLL_HW_PWRDN_CFG0_SEQ_IN_SWCTL	(1<<4)
#define UTMIPLL_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL	(1<<2)
#define UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE	(1<<1)
#define UTMIPLL_HW_PWRDN_CFG0_IDDQ_SWCTL	(1<<0)

#define PLLU_HW_PWRDN_CFG0			0x530
#define PLLU_HW_PWRDN_CFG0_IDDQ_PD_INCLUDE	(1<<28)
#define PLLU_HW_PWRDN_CFG0_SEQ_START_STATE	(1<<25)
#define PLLU_HW_PWRDN_CFG0_SEQ_ENABLE		(1<<24)
#define PLLU_HW_PWRDN_CFG0_USE_SWITCH_DETECT	(1<<7)
#define PLLU_HW_PWRDN_CFG0_USE_LOCKDET		(1<<6)
#define PLLU_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL	(1<<2)
#define PLLU_HW_PWRDN_CFG0_CLK_SWITCH_SWCTL	(1<<0)

#define XUSB_PLL_CFG0				0x534
#define XUSB_PLL_CFG0_UTMIPLL_LOCK_DLY		(0x3ff<<0)
#define XUSB_PLL_CFG0_PLLU_LOCK_DLY_SHIFT	14
#define XUSB_PLL_CFG0_PLLU_LOCK_DLY_MASK	\
	(0x3ff<<XUSB_PLL_CFG0_PLLU_LOCK_DLY_SHIFT)

/* DFLL */
#define DFLL_BASE				0x2f4
#define DFLL_BASE_RESET				(1<<0)

/* ADSP */
#define ADSP_NEON		(1 << 26)
#define ADSP_SCU		(1 << 25)
#define ADSP_WDT		(1 << 24)
#define ADSP_DBG		(1 << 23)
#define ADSP_PERIPH		(1 << 22)
#define ADSP_INTF		(1 << 21)
#define ADSP_CORE		(1 << 7)

#define ROUND_DIVIDER_UP	0
#define ROUND_DIVIDER_DOWN	1
#define DIVIDER_1_5_ALLOWED	0

/* PLLP default fixed rate in h/w controlled mode */
#define PLLP_DEFAULT_FIXED_RATE		408000000

/* Use PLL_RE as PLLE input (default - OSC via pll reference divider) */
#define USE_PLLE_INPUT_PLLRE    0

static void pllc4_set_fixed_rates(unsigned long cf);
static void tegra21_dfll_cpu_late_init(struct clk *c);
static void tegra21_pllp_init_dependencies(unsigned long pllp_rate);

static unsigned long tegra21_clk_shared_bus_update(struct clk *bus,
	struct clk **bus_top, struct clk **bus_slow, unsigned long *rate_cap);
static unsigned long tegra21_clk_cap_shared_bus(struct clk *bus,
	unsigned long rate, unsigned long ceiling);

static struct clk *pll_u;

static bool detach_shared_bus;
module_param(detach_shared_bus, bool, 0644);

/* Defines default range for dynamic frequency lock loop (DFLL)
   to be used as CPU clock source:
   "0" - DFLL is not used,
   "1" - DFLL is used as a source for all CPU rates
   "2" - DFLL is used only for high rates above crossover with PLL dvfs curve
*/
static int use_dfll;

/**
* Structure defining the fields for USB UTMI clocks Parameters.
*/
struct utmi_clk_param
{
	/* Oscillator Frequency in KHz */
	u32 osc_frequency;
	/* UTMIP PLL Enable Delay Count  */
	u8 enable_delay_count;
	/* UTMIP PLL Stable count */
	u8 stable_count;
	/*  UTMIP PLL Active delay count */
	u8 active_delay_count;
	/* UTMIP PLL Xtal frequency count */
	u8 xtal_freq_count;
};

static const struct utmi_clk_param utmi_parameters[] =
{
/*	OSC_FREQUENCY,	ENABLE_DLY,	STABLE_CNT,	ACTIVE_DLY,	XTAL_FREQ_CNT */
	{13000000,	0x02,		0x33,		0x05,		0x7F},
	{12000000,	0x02,		0x2F,		0x04,		0x76},

	{19200000,	0x03,		0x4B,		0x06,		0xBB},
	/* HACK!!! FIXME!!! following entry for 38.4MHz is a stub */
	{38400000,      0x0,           0x0,           0x06,           0x80}
};

static void __iomem *reg_pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
static void __iomem *misc_gp_base = IO_ADDRESS(TEGRA_APB_MISC_BASE);

#define MISC_GP_HIDREV				0x804
#define MISC_GP_TRANSACTOR_SCRATCH_0		0x864
#define MISC_GP_TRANSACTOR_SCRATCH_LA_ENABLE	(0x1 << 1)
#define MISC_GP_TRANSACTOR_SCRATCH_DDS_ENABLE	(0x1 << 2)
#define MISC_GP_TRANSACTOR_SCRATCH_DP2_ENABLE	(0x1 << 3)

#define pmc_writel(value, reg) \
	__raw_writel(value, reg_pmc_base + (reg))
#define pmc_readl(reg) \
	readl(reg_pmc_base + (reg))
#define chipid_readl() \
	__raw_readl(misc_gp_base + MISC_GP_HIDREV)

/*
 * Some peripheral clocks share an enable bit, so refcount the enable bits
 * in registers CLK_ENABLE_L, ... CLK_ENABLE_W, and protect refcount updates
 * with lock
 */
static DEFINE_SPINLOCK(periph_refcount_lock);
static int tegra_periph_clk_enable_refcount[CLK_OUT_ENB_NUM * 32];

static inline int clk_set_div(struct clk *c, u32 n)
{
	return clk_set_rate(c, (clk_get_rate(c->parent) + n-1) / n);
}

static inline u32 periph_clk_to_reg(
	struct clk *c, u32 reg_L, u32 reg_V, u32 reg_X, u32 reg_Y, int offs)
{
	u32 reg = c->u.periph.clk_num / 32;
	BUG_ON(reg >= RST_DEVICES_NUM);
	if (reg < 3)
		reg = reg_L + (reg * offs);
	else if (reg < 5)
		reg = reg_V + ((reg - 3) * offs);
	else if (reg == 5)
		reg = reg_X;
	else
		reg = reg_Y;
	return reg;
}

static int clk_div_x1_get_divider(unsigned long parent_rate, unsigned long rate,
			u32 max_x,
				 u32 flags, u32 round_mode)
{
	s64 divider_ux1 = parent_rate;
	if (!rate)
		return -EINVAL;

	if (!(flags & DIV_U71_INT))
		divider_ux1 *= 2;
	if (round_mode == ROUND_DIVIDER_UP)
		divider_ux1 += rate - 1;
	do_div(divider_ux1, rate);
	if (flags & DIV_U71_INT)
		divider_ux1 *= 2;

	if (divider_ux1 - 2 < 0)
		return 0;

	if (divider_ux1 - 2 > max_x)
		return -EINVAL;

#if !DIVIDER_1_5_ALLOWED
	if (divider_ux1 == 3)
		divider_ux1 = (round_mode == ROUND_DIVIDER_UP) ? 4 : 2;
#endif
	return divider_ux1 - 2;
}

static int clk_div71_get_divider(unsigned long parent_rate, unsigned long rate,
				 u32 flags, u32 round_mode)
{
	return clk_div_x1_get_divider(parent_rate, rate, 0xFF,
			flags, round_mode);
}

static int clk_div151_get_divider(unsigned long parent_rate, unsigned long rate,
				 u32 flags, u32 round_mode)
{
	return clk_div_x1_get_divider(parent_rate, rate, 0xFFFF,
			flags, round_mode);
}

static int clk_div16_get_divider(unsigned long parent_rate, unsigned long rate)
{
	s64 divider_u16;

	divider_u16 = parent_rate;
	if (!rate)
		return -EINVAL;
	divider_u16 += rate - 1;
	do_div(divider_u16, rate);

	if (divider_u16 - 1 < 0)
		return 0;

	if (divider_u16 - 1 > 0xFFFF)
		return -EINVAL;

	return divider_u16 - 1;
}

static long fixed_src_bus_round_updown(struct clk *c, struct clk *src,
			u32 flags, unsigned long rate, bool up, u32 *div)
{
	int divider;
	unsigned long source_rate, round_rate;

	source_rate = clk_get_rate(src);

	divider = clk_div71_get_divider(source_rate, rate + (up ? -1 : 1),
		flags, up ? ROUND_DIVIDER_DOWN : ROUND_DIVIDER_UP);
	if (divider < 0) {
		divider = flags & DIV_U71_INT ? 0xFE : 0xFF;
		round_rate = source_rate * 2 / (divider + 2);
		goto _out;
	}

	round_rate = source_rate * 2 / (divider + 2);

	if (round_rate > c->max_rate) {
		divider += flags & DIV_U71_INT ? 2 : 1;
#if !DIVIDER_1_5_ALLOWED
		divider = max(2, divider);
#endif
		round_rate = source_rate * 2 / (divider + 2);
	}
_out:
	if (div)
		*div = divider + 2;
	return round_rate;
}

static inline bool bus_user_is_slower(struct clk *a, struct clk *b)
{
	return a->u.shared_bus_user.client->max_rate <
		b->u.shared_bus_user.client->max_rate;
}

static inline bool bus_user_request_is_lower(struct clk *a, struct clk *b)
{
	return a->u.shared_bus_user.rate <
		b->u.shared_bus_user.rate;
}

static void super_clk_set_u71_div_no_skip(struct clk *c)
{
	clk_writel_delay(c->u.cclk.div71 << SUPER_CLOCK_DIV_U71_SHIFT,
			 c->reg + SUPER_CLK_DIVIDER);
	c->mul = 2;
	c->div = c->u.cclk.div71 + 2;
}

static void super_clk_clr_u71_div_no_skip(struct clk *c)
{
	clk_writel_delay(0, c->reg + SUPER_CLK_DIVIDER);
	c->mul = 2;
	c->div = 2;
}

/* clk_m functions */
static unsigned long tegra21_osc_autodetect_rate(struct clk *c)
{
	u32 osc_ctrl = clk_readl(OSC_CTRL);
	u32 pll_ref_div = clk_readl(OSC_CTRL) & OSC_CTRL_PLL_REF_DIV_MASK;

	switch (osc_ctrl & OSC_CTRL_OSC_FREQ_MASK) {
	case OSC_CTRL_OSC_FREQ_12MHZ:
		c->rate = 12000000;
		break;
	case OSC_CTRL_OSC_FREQ_13MHZ:
		/* 13MHz for FPGA only, BUG_ON otherwise */
		BUG_ON(!tegra_platform_is_fpga());
		c->rate = 13000000;
		break;
	case OSC_CTRL_OSC_FREQ_38_4MHZ:
		c->rate = 38400000;
		break;
	default:
		pr_err("supported OSC freq: %08x\n", osc_ctrl);
		BUG();
	}

	BUG_ON(pll_ref_div != OSC_CTRL_PLL_REF_DIV_1);

	return c->rate;
}

static void tegra21_osc_init(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	tegra21_osc_autodetect_rate(c);
	c->state = ON;
}

static int tegra21_osc_enable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	return 0;
}

static void tegra21_osc_disable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	WARN(1, "Attempting to disable main SoC clock\n");
}

static struct clk_ops tegra_osc_ops = {
	.init		= tegra21_osc_init,
	.enable		= tegra21_osc_enable,
	.disable	= tegra21_osc_disable,
};

static void tegra21_clk_m_init(struct clk *c)
{
	u32 rate;
	u32 spare = clk_readl(SPARE_REG);

	pr_debug("%s on clock %s\n", __func__, c->name);

	rate = clk_get_rate(c->parent); /* the rate of osc clock */

	if (tegra_platform_is_fpga()) {
		if (rate == 38400000) {
			/* Set divider to (2 + 1) to still maintain
			clk_m to 13MHz instead of reporting clk_m as
			19.2 MHz when it is actually set to 13MHz */
			spare &= ~SPARE_REG_CLK_M_DIVISOR_MASK;
			spare |= (2 << SPARE_REG_CLK_M_DIVISOR_SHIFT);
			clk_writel(spare, SPARE_REG);
		}
	}

	c->div = ((spare & SPARE_REG_CLK_M_DIVISOR_MASK)
		>> SPARE_REG_CLK_M_DIVISOR_SHIFT) + 1;
	c->mul = 1;
	c->state = ON;
}

static int tegra21_clk_m_enable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	return 0;
}

static void tegra21_clk_m_disable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);
	WARN(1, "Attempting to disable main SoC clock\n");
}

static struct clk_ops tegra_clk_m_ops = {
	.init		= tegra21_clk_m_init,
	.enable		= tegra21_clk_m_enable,
	.disable	= tegra21_clk_m_disable,
};

static struct clk_ops tegra_clk_m_div_ops = {
	.enable		= tegra21_clk_m_enable,
};

/* PLL reference divider functions */
static void tegra21_pll_ref_init(struct clk *c)
{
	u32 pll_ref_div = clk_readl(OSC_CTRL) & OSC_CTRL_PLL_REF_DIV_MASK;
	pr_debug("%s on clock %s\n", __func__, c->name);

	switch (pll_ref_div) {
	case OSC_CTRL_PLL_REF_DIV_1:
		c->div = 1;
		break;
	case OSC_CTRL_PLL_REF_DIV_2:
		c->div = 2;
		break;
	case OSC_CTRL_PLL_REF_DIV_4:
		c->div = 4;
		break;
	default:
		pr_err("%s: Invalid pll ref divider %d", __func__, pll_ref_div);
		BUG();
	}
	c->mul = 1;
	c->state = ON;
}

static struct clk_ops tegra_pll_ref_ops = {
	.init		= tegra21_pll_ref_init,
	.enable		= tegra21_clk_m_enable,
	.disable	= tegra21_clk_m_disable,
};

/* super clock functions */
/* "super clocks" on tegra21x have two-stage muxes, fractional 7.1 divider and
 * clock skipping super divider.  We will ignore the clock skipping divider,
 * since we can't lower the voltage when using the clock skip, but we can if
 * we lower the PLL frequency. Note that skipping divider can and will be used
 * by thermal control h/w for automatic throttling. There is also a 7.1 divider
 * that most CPU super-clock inputs can be routed through. We will not use it
 * as well (keep default 1:1 state), to avoid high jitter on PLLX and DFLL path
 * and possible concurrency access issues with thermal h/w (7.1 divider setting
 * share register with clock skipping divider)
 */
static void tegra21_super_clk_init(struct clk *c)
{
	u32 val;
	int source;
	int shift;
	const struct clk_mux_sel *sel;
	val = clk_readl(c->reg + SUPER_CLK_MUX);
	c->state = ON;
	BUG_ON(((val & SUPER_STATE_MASK) != SUPER_STATE_RUN) &&
		((val & SUPER_STATE_MASK) != SUPER_STATE_IDLE));
	shift = ((val & SUPER_STATE_MASK) == SUPER_STATE_IDLE) ?
		SUPER_IDLE_SOURCE_SHIFT : SUPER_RUN_SOURCE_SHIFT;
	source = (val >> shift) & SUPER_SOURCE_MASK;

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->value == source)
			break;
	}
	BUG_ON(sel->input == NULL);
	c->parent = sel->input;

	if (c->flags & DIV_U71) {
		c->mul = 2;
		c->div = 2;

		/*
		 * Make sure 7.1 divider is 1:1, clear h/w skipper control -
		 * it will be enabled by soctherm later
		 */
		val = clk_readl(c->reg + SUPER_CLK_DIVIDER);
		BUG_ON(val & SUPER_CLOCK_DIV_U71_MASK);
		val = 0;
		clk_writel(val, c->reg + SUPER_CLK_DIVIDER);
	}
	else
		clk_writel(0, c->reg + SUPER_CLK_DIVIDER);
}

static int tegra21_super_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra21_super_clk_disable(struct clk *c)
{
	/* since tegra 3 has 2 CPU super clocks - low power lp-mode clock and
	   geared up g-mode super clock - mode switch may request to disable
	   either of them; accept request with no affect on h/w */
}

static int tegra21_super_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;
	int shift;

	val = clk_readl(c->reg + SUPER_CLK_MUX);
	BUG_ON(((val & SUPER_STATE_MASK) != SUPER_STATE_RUN) &&
		((val & SUPER_STATE_MASK) != SUPER_STATE_IDLE));
	shift = ((val & SUPER_STATE_MASK) == SUPER_STATE_IDLE) ?
		SUPER_IDLE_SOURCE_SHIFT : SUPER_RUN_SOURCE_SHIFT;
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			val &= ~(SUPER_SOURCE_MASK << shift);
			val |= (sel->value & SUPER_SOURCE_MASK) << shift;

			if ((c->flags & DIV_U71) && !c->u.cclk.div71) {
				/* Make sure 7.1 divider is 1:1 */
				u32 div = clk_readl(c->reg + SUPER_CLK_DIVIDER);
				BUG_ON(div & SUPER_CLOCK_DIV_U71_MASK);
			}

			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			clk_writel_delay(val, c->reg);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

/*
 * Do not use super clocks "skippers", since dividing using a clock skipper
 * does not allow the voltage to be scaled down. Instead adjust the rate of
 * the parent clock. This requires that the parent of a super clock have no
 * other children, otherwise the rate will change underneath the other
 * children.
 */
static int tegra21_super_clk_set_rate(struct clk *c, unsigned long rate)
{
	/*
	 * In tegra21_cpu_clk_set_plls(), tegra21_sbus_cmplx_set_rate(), and
	 * tegra21_adsp_bus_clk_set_rate() this interface is skipped by directly
	 * setting rate of source plls.
	 */
	return clk_set_rate(c->parent, rate);
}

static struct clk_ops tegra_super_ops = {
	.init			= tegra21_super_clk_init,
	.enable			= tegra21_super_clk_enable,
	.disable		= tegra21_super_clk_disable,
	.set_parent		= tegra21_super_clk_set_parent,
	.set_rate		= tegra21_super_clk_set_rate,
};

/* virtual cpu clock functions */
/* some clocks can not be stopped (cpu, memory bus) while the SoC is running.
   To change the frequency of these clocks, the parent pll may need to be
   reprogrammed, so the clock must be moved off the pll, the pll reprogrammed,
   and then the clock moved back to the pll.  To hide this sequence, a virtual
   clock handles it.
 */
static void tegra21_cpu_clk_init(struct clk *c)
{
	c->state = (!is_lp_cluster() == (c->u.cpu.mode == MODE_G)) ? ON : OFF;
}

static int tegra21_cpu_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra21_cpu_clk_disable(struct clk *c)
{
	/* since tegra 3 has 2 virtual CPU clocks - low power lp-mode clock
	   and geared up g-mode clock - mode switch may request to disable
	   either of them; accept request with no affect on h/w */
}

static int tegra21_cpu_clk_set_plls(struct clk *c, unsigned long rate,
				    unsigned long old_rate)
{
	int ret = 0;
	bool dramp = false;
	bool on_main = false;
	unsigned long backup_rate, main_rate;
	struct clk *main_pll = c->u.cpu.main;

	/*
	 * Take an extra reference to the main pll so it doesn't turn off when
	 * we move the cpu off of it. If possible, use main pll dynamic ramp
	 * to reach target rate in one shot. Otherwise, use dynamic ramp to
	 * lower current rate to pll VCO minimum level before switching to
	 * backup source.
	 */
	if (c->parent->parent == main_pll) {
		tegra_clk_prepare_enable(main_pll);
		on_main = true;
		main_rate = rate;

		dramp = (rate > c->u.cpu.backup_rate) &&
			tegra_pll_can_ramp_to_rate(main_pll, rate);

		if (dramp || tegra_pll_can_ramp_to_min(main_pll, &main_rate)) {
			ret = clk_set_rate(main_pll, main_rate);
			if (ret) {
				pr_err("Failed to set cpu rate %lu on source"
				       " %s\n", main_rate, main_pll->name);
				goto out;
			}

			if (main_rate == rate)
				goto out;
		} else if (main_pll->u.pll.dyn_ramp) {
			pr_warn("%s: not ready for dynamic ramp to %lu\n",
				main_pll->name, rate);
		}
	}

	/* Switch to back-up source, and stay on it if target rate is below
	   backup rate */
	if (c->parent->parent != c->u.cpu.backup) {
		ret = clk_set_parent(c->parent, c->u.cpu.backup);
		if (ret) {
			pr_err("Failed to switch cpu to %s\n",
			       c->u.cpu.backup->name);
			goto out;
		}
	}

	backup_rate = min(rate, c->u.cpu.backup_rate);
	if (backup_rate != clk_get_rate_locked(c)) {
		ret = clk_set_rate(c->u.cpu.backup, backup_rate);
		if (ret) {
			pr_err("Failed to set cpu rate %lu on backup source\n",
			       backup_rate);
			goto out;
		}
	}
	if (rate == backup_rate)
		goto out;

	/* Switch from backup source to main at rate not exceeding pll VCO
	   minimum. Use dynamic ramp to reach target rate if it is above VCO
	   minimum. */
	main_rate = rate;
	if (!tegra_pll_can_ramp_from_min(main_pll, rate, &main_rate)) {
		if (main_pll->u.pll.dyn_ramp)
			pr_warn("%s: not ready for dynamic ramp to %lu\n",
				main_pll->name, rate);
	}

	ret = clk_set_rate(main_pll, main_rate);
	if (ret) {
		pr_err("Failed to set cpu rate %lu on source"
		       " %s\n", main_rate, main_pll->name);
		goto out;
	}

	ret = clk_set_parent(c->parent, main_pll);
	if (ret) {
		pr_err("Failed to switch cpu to %s\n", main_pll->name);
		goto out;
	}

	if (rate != main_rate) {
		ret = clk_set_rate(main_pll, rate);
		if (ret) {
			pr_err("Failed to set cpu rate %lu on source"
			       " %s\n", rate, main_pll->name);
			goto out;
		}
	}

out:
	if (on_main)
		tegra_clk_disable_unprepare(main_pll);

	return ret;
}

static int tegra21_cpu_clk_dfll_on(struct clk *c, unsigned long rate,
				   unsigned long old_rate)
{
	int ret;
	struct clk *dfll = c->u.cpu.dynamic;
	unsigned long dfll_rate_min = c->dvfs->dfll_data.use_dfll_rate_min;

	/* dfll rate request */
	ret = clk_set_rate(dfll, rate);
	if (ret) {
		pr_err("Failed to set cpu rate %lu on source"
		       " %s\n", rate, dfll->name);
		return ret;
	}

	/* 1st time - switch to dfll */
	if (c->parent->parent != dfll) {
		if (max(old_rate, rate) < dfll_rate_min) {
			/* set interim cpu dvfs rate at dfll_rate_min to
			   prevent voltage drop below dfll Vmin */
			ret = tegra_dvfs_set_rate(c, dfll_rate_min);
			if (ret) {
				pr_err("Failed to set cpu dvfs rate %lu\n",
				       dfll_rate_min);
				return ret;
			}
		}

		tegra_dvfs_rail_mode_updating(tegra_cpu_rail, true);
		ret = clk_set_parent(c->parent, dfll);
		if (ret) {
			tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
			pr_err("Failed to switch cpu to %s\n", dfll->name);
			return ret;
		}
		ret = tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
		WARN(ret, "Failed to lock %s at rate %lu\n", dfll->name, rate);

		/* prevent legacy dvfs voltage scaling */
		tegra_dvfs_dfll_mode_set(c->dvfs, rate);
		tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
	}
	return 0;
}

static int tegra21_cpu_clk_dfll_off(struct clk *c, unsigned long rate,
				    unsigned long old_rate)
{
	int ret;
	struct clk *pll;
	struct clk *dfll = c->u.cpu.dynamic;
	unsigned long dfll_rate_min = c->dvfs->dfll_data.use_dfll_rate_min;

	rate = min(rate, c->max_rate - c->dvfs->dfll_data.max_rate_boost);
	pll = (rate <= c->u.cpu.backup_rate) ? c->u.cpu.backup : c->u.cpu.main;
	dfll_rate_min = max(rate, dfll_rate_min);

	/* set target rate last time in dfll mode */
	if (old_rate != dfll_rate_min) {
		ret = tegra_dvfs_set_rate(c, dfll_rate_min);
		if (!ret)
			ret = clk_set_rate(dfll, dfll_rate_min);

		if (ret) {
			pr_err("Failed to set cpu rate %lu on source %s\n",
			       dfll_rate_min, dfll->name);
			return ret;
		}
	}

	/* unlock dfll - release volatge rail control */
	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, true);
	ret = tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 0);
	if (ret) {
		pr_err("Failed to unlock %s\n", dfll->name);
		goto back_to_dfll;
	}

	/* restore legacy dvfs operations and set appropriate voltage */
	ret = tegra_dvfs_dfll_mode_clear(c->dvfs, dfll_rate_min);
	if (ret) {
		pr_err("Failed to set cpu rail for rate %lu\n", rate);
		goto back_to_dfll;
	}

	/* set pll to target rate and return to pll source */
	ret = clk_set_rate(pll, rate);
	if (ret) {
		pr_err("Failed to set cpu rate %lu on source"
		       " %s\n", rate, pll->name);
		goto back_to_dfll;
	}
	ret = clk_set_parent(c->parent, pll);
	if (ret) {
		pr_err("Failed to switch cpu to %s\n", pll->name);
		goto back_to_dfll;
	}

	/* If going up, adjust voltage here (down path is taken care of by the
	   framework after set rate exit) */
	if (old_rate <= rate)
		tegra_dvfs_set_rate(c, rate);

	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
	return 0;

back_to_dfll:
	tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
	tegra_dvfs_dfll_mode_set(c->dvfs, old_rate);
	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
	return ret;
}

static int tegra21_cpu_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long old_rate = clk_get_rate_locked(c);
	bool has_dfll = c->u.cpu.dynamic &&
		(c->u.cpu.dynamic->state != UNINITIALIZED);
	bool is_dfll = c->parent->parent == c->u.cpu.dynamic;

	/* On SILICON allow CPU rate change only if cpu regulator is connected.
	   Ignore regulator connection on FPGA and SIMULATION platforms. */
	if (c->dvfs && tegra_platform_is_silicon()) {
		if (!c->dvfs->dvfs_rail)
			return -ENOSYS;
		else if ((!c->dvfs->dvfs_rail->reg) && (old_rate < rate) &&
			 (c->boot_rate < rate)) {
			WARN(1, "Increasing CPU rate while regulator is not"
				" ready is not allowed\n");
			return -ENOSYS;
		}
	}
	if (has_dfll && c->dvfs && c->dvfs->dvfs_rail) {
		if (tegra_dvfs_is_dfll_range(c->dvfs, rate))
			return tegra21_cpu_clk_dfll_on(c, rate, old_rate);
		else if (is_dfll)
			return tegra21_cpu_clk_dfll_off(c, rate, old_rate);
	}
	return tegra21_cpu_clk_set_plls(c, rate, old_rate);
}

static long tegra21_cpu_clk_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long max_rate = c->max_rate;

	/* Remove dfll boost to maximum rate when running on PLL */
	if (c->dvfs && !tegra_dvfs_is_dfll_scale(c->dvfs, rate))
		max_rate -= c->dvfs->dfll_data.max_rate_boost;

	if (rate > max_rate)
		rate = max_rate;
	else if (rate < c->min_rate)
		rate = c->min_rate;
	return rate;
}

static struct clk_ops tegra_cpu_ops = {
	.init     = tegra21_cpu_clk_init,
	.enable   = tegra21_cpu_clk_enable,
	.disable  = tegra21_cpu_clk_disable,
	.set_rate = tegra21_cpu_clk_set_rate,
	.round_rate = tegra21_cpu_clk_round_rate,
};


static void tegra21_cpu_cmplx_clk_init(struct clk *c)
{
	int i = is_lp_cluster() ? 1 : 0;

	BUG_ON(c->inputs[0].input->u.cpu.mode != MODE_G);
	BUG_ON(c->inputs[1].input->u.cpu.mode != MODE_LP);

	c->parent = c->inputs[i].input;
}

/* cpu complex clock provides second level vitualization (on top of
   cpu virtual cpu rate control) in order to hide the CPU mode switch
   sequence */
#if PARAMETERIZE_CLUSTER_SWITCH
static unsigned int switch_delay;
static unsigned int switch_flags;
static DEFINE_SPINLOCK(parameters_lock);

void tegra_cluster_switch_set_parameters(unsigned int us, unsigned int flags)
{
	spin_lock(&parameters_lock);
	switch_delay = us;
	switch_flags = flags;
	spin_unlock(&parameters_lock);
}
#endif

static int tegra21_cpu_cmplx_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra21_cpu_cmplx_clk_disable(struct clk *c)
{
	pr_debug("%s on clock %s\n", __func__, c->name);

	/* oops - don't disable the CPU complex clock! */
	BUG();
}

static int tegra21_cpu_cmplx_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long flags;
	int ret;
	struct clk *parent = c->parent;

	if (!parent->ops || !parent->ops->set_rate)
		return -ENOSYS;

	clk_lock_save(parent, &flags);

	ret = clk_set_rate_locked(parent, rate);

	clk_unlock_restore(parent, &flags);

	return ret;
}

static int cpu_cmplx_find_edp_safe_rates(unsigned long rate,
	struct clk *c_from, unsigned long *rate_from,
	struct clk *c_to, unsigned long *rate_to)
{
	int ret;
	*rate_from = rate; /* current rate is EDP safe on current cluster */
	*rate_to = clk_get_cpu_edp_safe_rate(c_to);

	/* EDP safe rate on target cluster is not specified: iso rate switch */
	if (*rate_to == 0) {
		rate = max(rate, c_to->min_rate);
		rate = min(rate, c_to->max_rate);
		BUG_ON((rate < c_from->min_rate) || (rate > c_from->max_rate));
		*rate_from = rate;
		*rate_to = rate;
		return 0;
	}

	/* Determine EDP safe rates, given DVFS voltage interdependencies */
	ret = tegra_dvfs_butterfly_throttle(c_from, rate_from, c_to, rate_to);
	if (ret)
		return ret;

	if ((*rate_from < c_from->min_rate) ||
	    (*rate_from > c_from->max_rate)) {
		pr_err("%s: EDP safe %s rate %lu out of range (%lu %lu)\n",
		       __func__, c_from->name, *rate_from,
		       c_from->min_rate, c_from->max_rate);
		return -EINVAL;
	}

	if ((*rate_to < c_to->min_rate) || (*rate_to > c_to->max_rate)) {
		pr_err("%s: EDP safe %s rate %lu out of range (%lu %lu)\n",
		       __func__, c_to->name, *rate_to,
		       c_to->min_rate, c_to->max_rate);
		return -EINVAL;
	}

	return 0;
}

static int cpu_cmplx_set_rate_from(struct clk *c_from, unsigned long rate_from,
				   unsigned long rate, struct clk **c_source)
{
	if (rate_from != rate) {
		int ret = clk_set_rate(c_from, rate_from);
		if (ret) {
			pr_err("%s: Failed to set rate %lu for %s\n",
			       __func__, rate_from, c_from->name);
			return ret;
		}
		*c_source = c_from->parent->parent;
	}
	return 0;
}

static int tegra21_cpu_cmplx_clk_set_parent(struct clk *c, struct clk *p)
{
	int ret;
	unsigned int flags = 0, delay = 0;
	const struct clk_mux_sel *sel;
	unsigned long rate_from, rate_to;
	unsigned long rate = clk_get_rate(c->parent);
	struct clk *dfll = c->parent->u.cpu.dynamic ? : p->u.cpu.dynamic;
	struct clk *c_source, *p_source;
	bool dfll_range_to;

	pr_debug("%s: %s %s\n", __func__, c->name, p->name);
	if (!c->refcnt) {
		WARN(1, "%s: cpu refcnt is 0: aborted switch from %s to %s\n",
		     __func__, c->parent->name, p->name);
		return -EINVAL;
	}
	BUG_ON(c->parent->u.cpu.mode != (is_lp_cluster() ? MODE_LP : MODE_G));

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p)
			break;
	}
	if (!sel->input)
		return -EINVAL;

#if PARAMETERIZE_CLUSTER_SWITCH
	spin_lock(&parameters_lock);
	flags = switch_flags;
	delay = switch_delay;
	switch_flags = 0;
	spin_unlock(&parameters_lock);
#endif
	if (!flags) {
		flags = TEGRA_POWER_CLUSTER_IMMEDIATE;
		flags |= TEGRA_POWER_CLUSTER_PART_DEFAULT;
		delay = 0;
	}
	flags |= (p->u.cpu.mode == MODE_LP) ? TEGRA_POWER_CLUSTER_LP :
		TEGRA_POWER_CLUSTER_G;

	if (p == c->parent) {
		if (flags & TEGRA_POWER_CLUSTER_FORCE) {
			/* Allow parameterized switch to the same mode */
			ret = tegra_cluster_control(delay, flags);
			if (ret)
				pr_err("%s: Failed to force %s mode to %s\n",
				       __func__, c->name, p->name);
			return ret;
		}
		return 0;	/* already switched - exit */
	}

	/* Find EDP safe rates for switch */
	ret = cpu_cmplx_find_edp_safe_rates(
		rate, c->parent, &rate_from, p, &rate_to);
	if (ret) {
		pr_err("%s: Didn't find EDP safe rates for %s to %s switch\n",
		       __func__, c->parent->name, p->name);
		return ret;
	}

	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, true);
	c_source = c->parent->parent->parent;
	dfll_range_to = p->dvfs && tegra_dvfs_is_dfll_range(p->dvfs, rate_to);
	if (c_source == dfll) {
		/*
		 * G (DFLL selected as clock source) => LP switch:
		 * - set EDP safe rate on current cluster
		 * - turn DFLL into open loop mode ("release" VDD_CPU rail)
		 * - select target p_source for LP, and get EDP safe rate ready
		 */
		ret = cpu_cmplx_set_rate_from(c->parent, rate_from, rate,
					      &c_source);
		if (ret)
			goto abort;

		ret = tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 0);
		if (ret)
			goto abort;

		p_source = rate_to <= p->u.cpu.backup_rate ?
			p->u.cpu.backup : p->u.cpu.main;
		ret = clk_set_rate(p_source, rate_to);
		if (ret)
			goto abort;
	} else if ((p->parent->parent == dfll) || dfll_range_to) {
		/*
		 * LP => G (DFLL selected as clock source) switch:
		 * - set EDP safe rate on current cluster
		 * - select target p_source as dfll, and set DFLL rate ready.
		 * DFLL is still disabled, initial rate only set skipper so
		 * that when enabled in open loop DFLL is running at low EDP
		 * safe rate. In case when the entire G cluster rate range is
		 * sourced from DFLL any EDP safe rate also guarantees voltage
		 * above DFLL Vmin. In case when there is low PLL sourced rate
		 * range, we need explicitly apply DFLL rate floor, which may
		 * violate EDP limitations. Only former case is productized on
		 * Tegra21.
		 */
		ret = cpu_cmplx_set_rate_from(c->parent, rate_from, rate,
					      &c_source);
		if (ret)
			goto abort;

		BUG_ON(!dfll_range_to);

		p_source = dfll;
		ret = clk_set_rate(dfll, dfll_range_to ? rate_to :
			max(rate_to, p->dvfs->dfll_data.use_dfll_rate_min));
		if (ret)
			goto abort;
	} else {
		/*
		 * DFLL is not selected on either side of the switch:
		 * - set common EDP safe rate (since we use the same PLL on
		 *   both clusters in this case)
		 * - set target p_source equal to current clock source
		 */
		rate_from = min(rate_from, rate_to);
		if ((rate_from < c->parent->min_rate) ||
		    (rate_from < p->min_rate)) {
			pr_err("%s: common EDP safe rate %lu out of range\n",
			       __func__, rate_from);
			ret = -EINVAL;
			goto abort;
		}

		ret = cpu_cmplx_set_rate_from(c->parent, rate_from, rate,
					      &c_source);
		if (ret)
			goto abort;

		p_source = c_source;
	}

	/* Switch new parent to target clock source if necessary */
	if (p->parent->parent != p_source) {
		ret = clk_set_parent(p->parent, p_source);
		if (ret) {
			pr_err("%s: Failed to set parent %s for %s\n",
			       __func__, p_source->name, p->name);
			goto abort;
		}
	}

	/*
	 * G=>LP switch with c_source DFLL in open loop mode: restore legacy
	 * DVFS rail control with no changes in voltage (DFLL is still sourcing
	 * G cluster, and no need to bump up voltage to PLLX V/F curve); then
	 * enabling new parent (LP cluster clock) will set voltage to the max of
	 * G-on-DFLL and LP-on-PLLX required levels.
	 *
	 * For any other combination of sources, rail is already under legacy
	 * DVFS control, and enabling new parent scales new mode voltage rail
	 * in advance before the switch happens. If target p_source is DFLL it
	 * is switched here to open loop mode.
	 */
	if (c_source == dfll)
		tegra_dvfs_dfll_mode_clear(c->parent->dvfs, 0);

	ret = tegra_clk_prepare_enable(p);
	if (ret) {
		pr_err("%s: Failed to enable parent clock %s\n",
			__func__, p->name);
		goto abort;
	}

	pr_debug("%s: switch %s(rate %lu) to %s(rate %lu) at %d mV\n", __func__,
		 c->parent->name, clk_get_rate(c->parent),
		 p->name, clk_get_rate(p),
		 tegra_dvfs_rail_get_current_millivolts(tegra_cpu_rail));

	/* switch CPU mode */
	ret = tegra_cluster_control(delay, flags);
	if (ret) {
		tegra_clk_disable_unprepare(p);
		pr_err("%s: Failed to switch %s mode to %s\n",
		       __func__, c->name, p->name);
		goto abort;
	}

	/*
	 * Lock DFLL now (resume closed loop VDD_CPU control).
	 * G CPU operations are resumed on DFLL if it was the last G CPU
	 * clock source, or if resume rate is in DFLL usage range in case
	 * when auto-switch between PLL and DFLL is enabled.
	 */
	if (p_source == dfll) {
		clk_set_rate(dfll, rate_to);
		tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
		tegra_dvfs_dfll_mode_set(p->dvfs, rate_to);
	}

	/* Disabling old parent scales old mode voltage rail */
	tegra_clk_disable_unprepare(c->parent);

	clk_reparent(c, p);

	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);
	return 0;

abort:
	/* Restore rate, and re-lock DFLL if necessary after aborted switch */
	clk_set_rate(c->parent, rate);
	if (c_source == dfll) {
		tegra_clk_cfg_ex(dfll, TEGRA_CLK_DFLL_LOCK, 1);
		tegra_dvfs_dfll_mode_set(c->parent->dvfs, rate);
	}
	tegra_dvfs_rail_mode_updating(tegra_cpu_rail, false);

	pr_err("%s: aborted switch from %s to %s\n",
	       __func__, c->parent->name, p->name);
	return ret;
}

static long tegra21_cpu_cmplx_round_rate(struct clk *c,
	unsigned long rate)
{
	return clk_round_rate(c->parent, rate);
}

static struct clk_ops tegra_cpu_cmplx_ops = {
	.init     = tegra21_cpu_cmplx_clk_init,
	.enable   = tegra21_cpu_cmplx_clk_enable,
	.disable  = tegra21_cpu_cmplx_clk_disable,
	.set_rate = tegra21_cpu_cmplx_clk_set_rate,
	.set_parent = tegra21_cpu_cmplx_clk_set_parent,
	.round_rate = tegra21_cpu_cmplx_round_rate,
};

/* virtual cop clock functions. Used to acquire the fake 'cop' clock to
 * reset the COP block (i.e. AVP) */
static void tegra21_cop_clk_reset(struct clk *c, bool assert)
{
	unsigned long reg = assert ? RST_DEVICES_SET_L : RST_DEVICES_CLR_L;

	pr_debug("%s %s\n", __func__, assert ? "assert" : "deassert");
	clk_writel(1 << 1, reg);
}

static struct clk_ops tegra_cop_ops = {
	.reset    = tegra21_cop_clk_reset,
};

/* bus clock functions */
static DEFINE_SPINLOCK(bus_clk_lock);

static int bus_set_div(struct clk *c, int div)
{
	u32 val;
	unsigned long flags;

	if (!div || (div > (BUS_CLK_DIV_MASK + 1)))
		return -EINVAL;

	spin_lock_irqsave(&bus_clk_lock, flags);
	val = clk_readl(c->reg);
	val &= ~(BUS_CLK_DIV_MASK << c->reg_shift);
	val |= (div - 1) << c->reg_shift;
	clk_writel(val, c->reg);
	c->div = div;
	spin_unlock_irqrestore(&bus_clk_lock, flags);

	return 0;
}

static void tegra21_bus_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->state = ((val >> c->reg_shift) & BUS_CLK_DISABLE) ? OFF : ON;
	c->div = ((val >> c->reg_shift) & BUS_CLK_DIV_MASK) + 1;
	c->mul = 1;
}

static int tegra21_bus_clk_enable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	val &= ~(BUS_CLK_DISABLE << c->reg_shift);
	clk_writel(val, c->reg);
	return 0;
}

static void tegra21_bus_clk_disable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	val |= BUS_CLK_DISABLE << c->reg_shift;
	clk_writel(val, c->reg);
}

static int tegra21_bus_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long parent_rate = clk_get_rate(c->parent);
	int i;

	for (i = 1; i <= 4; i++) {
		if (rate >= parent_rate / i)
			return bus_set_div(c, i);
	}
	return -EINVAL;
}

static struct clk_ops tegra_bus_ops = {
	.init			= tegra21_bus_clk_init,
	.enable			= tegra21_bus_clk_enable,
	.disable		= tegra21_bus_clk_disable,
	.set_rate		= tegra21_bus_clk_set_rate,
};

/*
 * Virtual system bus complex clock is used to hide the sequence of
 * changing sclk/hclk/pclk parents and dividers to configure requested
 * sclk target rate.
 */
#define BUS_AHB_DIV_MAX			(BUS_CLK_DIV_MASK + 1UL)
#define BUS_APB_DIV_MAX			(BUS_CLK_DIV_MASK + 1UL)

static unsigned long sclk_pclk_unity_ratio_rate_max = 136000000;

struct clk_div_sel {
	struct clk *src;
	u32 div;
	unsigned long rate;
};
static struct clk_div_sel sbus_round_table[MAX_DVFS_FREQS + 1];
static int sbus_round_table_size;

static int last_round_idx;
static int get_start_idx(unsigned long rate)
{
	int i = last_round_idx;
	if (rate == sbus_round_table[i].rate)
		return i;
	return 0;
}


static void tegra21_sbus_cmplx_init(struct clk *c)
{
	unsigned long rate;
	struct clk *sclk_div = c->parent->parent;

	c->max_rate = sclk_div->max_rate;
	c->min_rate = sclk_div->min_rate;

	rate = clk_get_rate(c->u.system.sclk_low);
	if (tegra_platform_is_qt())
		return;

	/*
	 * Unity and low range thresholds must be an exact proper factors of
	 * low range parent. Unity threshold must be set within low range
	 */
	if (c->u.system.threshold &&
	    (sclk_pclk_unity_ratio_rate_max > c->u.system.threshold))
		sclk_pclk_unity_ratio_rate_max = c->u.system.threshold;
	if (sclk_pclk_unity_ratio_rate_max)
		BUG_ON((rate % sclk_pclk_unity_ratio_rate_max) != 0);
	if (c->u.system.threshold)
		BUG_ON((rate % c->u.system.threshold) != 0);
	BUG_ON(!(sclk_div->flags & DIV_U71));
}

/* This special sbus round function is implemented because:
 *
 * (a) sbus complex clock source is selected automatically based on rate
 *
 * (b) since sbus is a shared bus, and its frequency is set to the highest
 * enabled shared_bus_user clock, the target rate should be rounded up divider
 * ladder (if max limit allows it) - for pll_div and peripheral_div common is
 * rounding down - special case again.
 *
 * Note that final rate is trimmed (not rounded up) to avoid spiraling up in
 * recursive calls. Lost 1Hz is added in tegra21_sbus_cmplx_set_rate before
 * actually setting divider rate.
 */
static void sbus_build_round_table_one(struct clk *c, unsigned long rate, int j)
{
	struct clk_div_sel sel;
	struct clk *sclk_div = c->parent->parent;
	u32 flags = sclk_div->flags;

	sel.src = c->u.system.sclk_low;
	sel.rate = fixed_src_bus_round_updown(
		c, sel.src, flags, rate, false, &sel.div);
	sbus_round_table[j] = sel;

	/* Don't use high frequency source above threshold */
	if (rate <= c->u.system.threshold)
		return;

	sel.src = c->u.system.sclk_high;
	sel.rate = fixed_src_bus_round_updown(
		c, sel.src, flags, rate, false, &sel.div);
	if (sbus_round_table[j].rate < sel.rate)
		sbus_round_table[j] = sel;
}

/* Populate sbus (not Avalon) round table with dvfs entries (not knights) */
static void sbus_build_round_table(struct clk *c)
{
	int i, j = 0;
	unsigned long rate;
	bool inserted_u = false;
	bool inserted_t = false;
	unsigned long threshold = c->u.system.threshold;

	/*
	 * Make sure unity ratio threshold always inserted into the table.
	 * If no dvfs specified, just add maximum rate entry. Othrwise, add
	 * entries for all dvfs rates.
	 */
	if (!c->dvfs || !c->dvfs->num_freqs) {
		sbus_build_round_table_one(
			c, sclk_pclk_unity_ratio_rate_max, j++);
		sbus_build_round_table_one(c, threshold, j++);
		sbus_build_round_table_one(c, c->max_rate, j++);
		sbus_round_table_size = j;
		return;
	}

	for (i = 0; i < c->dvfs->num_freqs; i++) {
		rate = c->dvfs->freqs[i];
		if (rate <= 1 * c->dvfs->freqs_mult)
			continue; /* skip 1kHz place holders */

		if (i && (rate == c->dvfs->freqs[i - 1]))
			continue; /* skip duplicated rate */

		if (!inserted_u && (rate >= sclk_pclk_unity_ratio_rate_max)) {
			inserted_u = true;
			if (sclk_pclk_unity_ratio_rate_max == threshold)
				inserted_t = true;

			if (rate > sclk_pclk_unity_ratio_rate_max)
				sbus_build_round_table_one(
					c, sclk_pclk_unity_ratio_rate_max, j++);
		}
		if (!inserted_t && (rate >= threshold)) {
			inserted_t = true;
			if (rate > threshold)
				sbus_build_round_table_one(c, threshold, j++);
		}
		sbus_build_round_table_one(c, rate, j++);
	}
	sbus_round_table_size = j;
}

/* Clip requested rate to the entry in the round table. Allow +/-1Hz slack. */
static long tegra21_sbus_cmplx_round_updown(struct clk *c, unsigned long rate,
					    bool up)
{
	int i;

	if (!sbus_round_table_size) {
		sbus_build_round_table(c);
		if (!sbus_round_table_size) {
			WARN(1, "Invalid sbus round table\n");
			return -EINVAL;
		}
	}

	rate = max(rate, c->min_rate);

	i = get_start_idx(rate);
	for (; i < sbus_round_table_size - 1; i++) {
		unsigned long sel_rate = sbus_round_table[i].rate;
		if (abs(rate - sel_rate) <= 1) {
			break;
		} else if (rate < sel_rate) {
			if (!up && i)
				i--;
			break;
		}
	}
	last_round_idx = i;
	return sbus_round_table[i].rate;
}

static long tegra21_sbus_cmplx_round_rate(struct clk *c, unsigned long rate)
{
	return tegra21_sbus_cmplx_round_updown(c, rate, true);
}

/*
 * Select {source : divider} setting from pre-built round table, and actually
 * change the configuration. Since voltage during switch is at safe level for
 * current and new sbus rates (but not above) over-clocking during the switch
 * is not allowed. Hence, the order of switch: 1st change divider if its setting
 * increases, then switch source clock, and finally change divider if it goes
 * down. No over-clocking is guaranteed, but dip below both initial and final
 * rates is possible.
 */
static int tegra21_sbus_cmplx_set_rate(struct clk *c, unsigned long rate)
{
	int ret, i;
	struct clk *skipper = c->parent;
	struct clk *sclk_div = skipper->parent;
	struct clk *sclk_mux = sclk_div->parent;
	struct clk_div_sel *new_sel = NULL;
	unsigned long sclk_div_rate = clk_get_rate(sclk_div);

	/*
	 * Configure SCLK/HCLK/PCLK guranteed safe combination:
	 * - keep hclk at the same rate as sclk
	 * - set pclk at 1:2 rate of hclk
	 * - disable sclk skipper
	 */
	bus_set_div(c->u.system.pclk, 2);
	bus_set_div(c->u.system.hclk, 1);
	c->child_bus->child_bus->div = 2;
	c->child_bus->div = 1;
	clk_set_rate(skipper, c->max_rate);

	/* Select new source/divider */
	i = get_start_idx(rate);
	for (; i < sbus_round_table_size; i++) {
		if (rate == sbus_round_table[i].rate) {
			new_sel = &sbus_round_table[i];
			break;
		}
	}
	if (!new_sel)
		return -EINVAL;

	if (sclk_div_rate == rate) {
		pr_debug("sbus_set_rate: no change in rate %lu on parent %s\n",
			 clk_get_rate_locked(c), sclk_mux->parent->name);
		return 0;
	}

	/* Raise voltage on the way up */
	if (c->dvfs && (rate > sclk_div_rate)) {
		ret = tegra_dvfs_set_rate(c, rate);
		if (ret)
			return ret;
		pr_debug("sbus_set_rate: set %d mV\n", c->dvfs->cur_millivolts);
	}

	/* Do switch */
	if (sclk_div->div < new_sel->div) {
		unsigned long sdiv_rate = sclk_div_rate * sclk_div->div;
		sdiv_rate = DIV_ROUND_UP(sdiv_rate, new_sel->div);
		ret = clk_set_rate(sclk_div, sdiv_rate);
		if (ret) {
			pr_err("%s: Failed to set %s rate to %lu\n",
			       __func__, sclk_div->name, sdiv_rate);
			return ret;
		}
		pr_debug("sbus_set_rate: rate %lu on parent %s\n",
			 clk_get_rate_locked(c), sclk_mux->parent->name);

	}

	if (new_sel->src != sclk_mux->parent) {
		ret = clk_set_parent(sclk_mux, new_sel->src);
		if (ret) {
			pr_err("%s: Failed to switch sclk source to %s\n",
			       __func__, new_sel->src->name);
			return ret;
		}
		pr_debug("sbus_set_rate: rate %lu on parent %s\n",
			 clk_get_rate_locked(c), sclk_mux->parent->name);
	}

	if (sclk_div->div > new_sel->div) {
		ret = clk_set_rate(sclk_div, rate + 1);
		if (ret) {
			pr_err("%s: Failed to set %s rate to %lu\n",
			       __func__, sclk_div->name, rate);
			return ret;
		}
		pr_debug("sbus_set_rate: rate %lu on parent %s\n",
			 clk_get_rate_locked(c), sclk_mux->parent->name);
	}

	/* Lower voltage on the way down */
	if (c->dvfs && (rate < sclk_div_rate)) {
		ret = tegra_dvfs_set_rate(c, rate);
		if (ret)
			return ret;
		pr_debug("sbus_set_rate: set %d mV\n", c->dvfs->cur_millivolts);
	}

	return 0;
}

/*
 * Limitations on SCLK/HCLK/PCLK ratios:
 * (A) H/w limitation:
 *	if SCLK >= 136MHz, SCLK:PCLK >= 2
 * (B) S/w policy limitation, in addition to (A):
 *	if any APB bus shared user request is enabled, HCLK:PCLK >= 2
 *  Reason for (B): assuming APB bus shared user has requested X < 136MHz,
 *  HCLK = PCLK = X, and new AHB user is coming on-line requesting Y >= 136MHz,
 *  we can consider 2 paths depending on order of changing HCLK rate and
 *  HCLK:PCLK ratio
 *  (i)  HCLK:PCLK = X:X => Y:Y* => Y:Y/2,   (*) violates rule (A)
 *  (ii) HCLK:PCLK = X:X => X:X/2* => Y:Y/2, (*) under-clocks APB user
 *  In this case we can not guarantee safe transition from HCLK:PCLK = 1:1
 *  below 60MHz to HCLK rate above 60MHz without under-clocking APB user.
 *  Hence, policy (B).
 *
 *  When there are no request from APB users, path (ii) can be used to
 *  increase HCLK above 136MHz, and HCLK:PCLK = 1:1 is allowed.
 *
 *  Note: with common divider used in the path for all SCLK sources SCLK rate
 *  during switching may dip down, anyway. So, in general, policy (ii) does not
 *  prevent underclocking users during clock transition.
 */
static int tegra21_clk_sbus_update(struct clk *bus)
{
	int ret, div;
	bool p_requested;
	unsigned long s_rate, h_rate, p_rate, ceiling, s_rate_raw;
	struct clk *ahb, *apb;
	struct clk *skipper = bus->parent;

	if (detach_shared_bus)
		return 0;

	s_rate = tegra21_clk_shared_bus_update(bus, &ahb, &apb, &ceiling);
	if (bus->override_rate) {
		ret = clk_set_rate_locked(bus, s_rate);
		if (!ret)
			clk_set_rate(skipper, s_rate);
		return ret;
	}

	ahb = bus->child_bus;
	apb = ahb->child_bus;
	h_rate = ahb->u.shared_bus_user.rate;
	p_rate = apb->u.shared_bus_user.rate;
	p_requested = apb->refcnt > 1;

	/* Propagate ratio requirements up from PCLK to SCLK */
	if (p_requested)
		h_rate = max(h_rate, p_rate * 2);
	s_rate = max(s_rate, h_rate);
	if (s_rate >= sclk_pclk_unity_ratio_rate_max)
		s_rate = max(s_rate, p_rate * 2);

	/* Propagate cap requirements down from SCLK to PCLK */
	s_rate_raw = s_rate;
	s_rate = tegra21_clk_cap_shared_bus(bus, s_rate, ceiling);
	if (s_rate >= sclk_pclk_unity_ratio_rate_max)
		p_rate = min(p_rate, s_rate / 2);
	h_rate = min(h_rate, s_rate);
	if (p_requested)
		p_rate = min(p_rate, h_rate / 2);


	/* Set new sclk rate in safe 1:1:2, rounded "up" configuration */
	ret = clk_set_rate_locked(bus, s_rate);
	if (ret)
		return ret;
	clk_set_rate(skipper, s_rate_raw);

	/* Finally settle new bus divider values */
	s_rate = clk_get_rate_locked(bus);
	div = min(s_rate / h_rate, BUS_AHB_DIV_MAX);
	if (div != 1) {
		bus_set_div(bus->u.system.hclk, div);
		ahb->div = div;
	}

	h_rate = clk_get_rate(bus->u.system.hclk);
	div = min(h_rate / p_rate, BUS_APB_DIV_MAX);
	if (div != 2) {
		bus_set_div(bus->u.system.pclk, div);
		apb->div = div;
	}

	return 0;
}

static struct clk_ops tegra_sbus_cmplx_ops = {
	.init = tegra21_sbus_cmplx_init,
	.set_rate = tegra21_sbus_cmplx_set_rate,
	.round_rate = tegra21_sbus_cmplx_round_rate,
	.round_rate_updown = tegra21_sbus_cmplx_round_updown,
	.shared_bus_update = tegra21_clk_sbus_update,
};

/*
 * Virtual ADSP bus clock operations. Used to hide the sequence of changing
 * and re-locking ADSP source PLLA1 in flight to configure requested ADSP
 * target rate.
 */
static void tegra21_adsp_bus_clk_init(struct clk *c)
{
	c->state = c->parent->state;
	c->min_rate = c->u.cpu.main->min_rate;
}

static int tegra21_adsp_bus_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra21_adsp_bus_clk_disable(struct clk *c)
{
}

static int tegra21_adsp_bus_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret = 0;
	struct clk *main_pll = c->u.cpu.main;

	/*
	 * If ADSP clock is disabled or it is not on main pll (may happen after
	 * boot), set main pll rate, and make sure it is selected as adsp clock
	 * source.
	 */
	if (!c->refcnt || (c->parent->parent != main_pll)) {
		ret = clk_set_rate(main_pll, rate);
		if (ret) {
			pr_err("Failed to set adsp rate %lu on %s\n",
			       rate, main_pll->name);
			return ret;
		}

		if (c->parent->parent == main_pll)
			return ret;

		ret = clk_set_parent(c->parent, main_pll);
		if (ret)
			pr_err("Failed to switch adsp to %s\n", main_pll->name);

		return ret;
	}

	/*
	 * Take an extra reference to the main pll so it doesn't turn off when
	 * we move the cpu off of it. If possible, use main pll dynamic ramp
	 * to reach target rate in one shot. Otherwise use backup source while
	 * re-locking main.
	 */
	tegra_clk_prepare_enable(main_pll);

	if (tegra_pll_can_ramp_to_rate(main_pll, rate)) {
		ret = clk_set_rate(main_pll, rate);
		if (ret)
			pr_err("Failed to ramp %s to adsp rate %lu\n",
			       main_pll->name, rate);
		goto out;
	}

	/* Set backup divider, and switch to backup source */
	super_clk_set_u71_div_no_skip(c->parent);
	ret = clk_set_parent(c->parent, c->u.cpu.backup);
	if (ret) {
		super_clk_clr_u71_div_no_skip(c->parent);
		pr_err("Failed to switch adsp to %s\n", c->u.cpu.backup->name);
		goto out;
	}

	/* Set main pll rate, switch back to main, and clear backup divider */
	ret = clk_set_rate(main_pll, rate);
	if (ret) {
		pr_err("Failed set adsp rate %lu on %s\n",
		       rate, main_pll->name);
		goto out;
	}

	ret = clk_set_parent(c->parent, main_pll);
	if (ret) {
		pr_err("Failed to switch adsp to %s\n", main_pll->name);
		goto out;
	}
	super_clk_clr_u71_div_no_skip(c->parent);

out:
	tegra_clk_disable_unprepare(main_pll);
	return ret;
}

static long tegra21_adsp_bus_clk_round_rate(struct clk *c, unsigned long rate)
{
	if (rate > c->max_rate)
		rate = c->max_rate;
	else if (rate < c->min_rate)
		rate = c->min_rate;
	return rate;
}

/* Blink output functions */
static void tegra21_blink_clk_init(struct clk *c)
{
	u32 val;

	val = pmc_readl(PMC_CTRL);
	c->state = (val & PMC_CTRL_BLINK_ENB) ? ON : OFF;
	c->mul = 1;
	val = pmc_readl(c->reg);

	if (val & PMC_BLINK_TIMER_ENB) {
		unsigned int on_off;

		on_off = (val >> PMC_BLINK_TIMER_DATA_ON_SHIFT) &
			PMC_BLINK_TIMER_DATA_ON_MASK;
		val >>= PMC_BLINK_TIMER_DATA_OFF_SHIFT;
		val &= PMC_BLINK_TIMER_DATA_OFF_MASK;
		on_off += val;
		/* each tick in the blink timer is 16 32KHz clocks */
		c->div = on_off * 16;
	} else {
		c->div = 1;
	}
}

static int tegra21_blink_clk_enable(struct clk *c)
{
	u32 val;

	val = pmc_readl(PMC_DPD_PADS_ORIDE);
	pmc_writel(val | PMC_DPD_PADS_ORIDE_BLINK_ENB, PMC_DPD_PADS_ORIDE);

	val = pmc_readl(PMC_CTRL);
	pmc_writel(val | PMC_CTRL_BLINK_ENB, PMC_CTRL);
	pmc_readl(PMC_CTRL);

	return 0;
}

static void tegra21_blink_clk_disable(struct clk *c)
{
	u32 val;

	val = pmc_readl(PMC_CTRL);
	pmc_writel(val & ~PMC_CTRL_BLINK_ENB, PMC_CTRL);

	val = pmc_readl(PMC_DPD_PADS_ORIDE);
	pmc_writel(val & ~PMC_DPD_PADS_ORIDE_BLINK_ENB, PMC_DPD_PADS_ORIDE);
	pmc_readl(PMC_DPD_PADS_ORIDE);
}

static int tegra21_blink_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long parent_rate = clk_get_rate(c->parent);
	if (rate >= parent_rate) {
		c->div = 1;
		pmc_writel(0, c->reg);
	} else {
		unsigned int on_off;
		u32 val;

		on_off = DIV_ROUND_UP(parent_rate / 32, rate);
		c->div = on_off * 32;

		val = (on_off & PMC_BLINK_TIMER_DATA_ON_MASK) <<
			PMC_BLINK_TIMER_DATA_ON_SHIFT;
		on_off &= PMC_BLINK_TIMER_DATA_OFF_MASK;
		on_off <<= PMC_BLINK_TIMER_DATA_OFF_SHIFT;
		val |= on_off;
		val |= PMC_BLINK_TIMER_ENB;
		pmc_writel(val, c->reg);
	}
	pmc_readl(c->reg);

	return 0;
}

static struct clk_ops tegra_blink_clk_ops = {
	.init			= &tegra21_blink_clk_init,
	.enable			= &tegra21_blink_clk_enable,
	.disable		= &tegra21_blink_clk_disable,
	.set_rate		= &tegra21_blink_clk_set_rate,
};

/* UTMIP PLL configuration */
static void tegra21_utmi_param_configure(struct clk *c)
{
	u32 reg;
	int i;
	unsigned long main_rate =
		clk_get_rate(c->parent->parent);

	for (i = 0; i < ARRAY_SIZE(utmi_parameters); i++) {
		if (main_rate == utmi_parameters[i].osc_frequency) {
			break;
		}
	}

	if (i >= ARRAY_SIZE(utmi_parameters)) {
		pr_err("%s: Unexpected main rate %lu\n", __func__, main_rate);
		return;
	}

	reg = clk_readl(UTMIPLL_HW_PWRDN_CFG0);
	reg &= ~UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE;
	pll_writel_delay(reg, UTMIPLL_HW_PWRDN_CFG0);

	reg = clk_readl(UTMIP_PLL_CFG2);
	/* Program UTMIP PLL stable and active counts */
	/* [FIXME] arclk_rst.h says WRONG! This should be 1ms -> 0x50 Check! */
	reg &= ~UTMIP_PLL_CFG2_STABLE_COUNT(~0);
	reg |= UTMIP_PLL_CFG2_STABLE_COUNT(
			utmi_parameters[i].stable_count);
	clk_writel(reg, UTMIP_PLL_CFG2);

	/* Program UTMIP PLL delay and oscillator frequency counts */
	reg = clk_readl(UTMIP_PLL_CFG1);
	reg &= ~UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(~0);
	reg |= UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(
		utmi_parameters[i].enable_delay_count);

	reg &= ~UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(~0);
	reg |= UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(
		utmi_parameters[i].xtal_freq_count);

	/* Remove power downs from UTMIP PLL control bits */
	reg |= UTMIP_PLL_CFG1_FORCE_PLLU_POWERDOWN;
	clk_writel(reg, UTMIP_PLL_CFG1);

	/* Enable PLL with SW programming */
	reg = clk_readl(UTMIP_PLL_CFG1);
	reg |= UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERUP;
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN;
	pll_writel_delay(reg, UTMIP_PLL_CFG1);

	/* Enable Samplers for SNPS IP, XUSB_HOST, XUSB_DEV */
	reg = clk_readl(UTMIP_PLL_CFG2);
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERUP;
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERUP;
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERUP;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERDOWN;
	clk_writel(reg, UTMIP_PLL_CFG2);

	/* Enable HW Power Sequencer */
	reg = clk_readl(UTMIP_PLL_CFG1);
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERUP;
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN;
	clk_writel(reg, UTMIP_PLL_CFG1);

	reg = clk_readl(UTMIPLL_HW_PWRDN_CFG0);
	reg &= ~UTMIPLL_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL;
	reg |= UTMIPLL_HW_PWRDN_CFG0_USE_LOCKDET;
	pll_writel_delay(reg, UTMIPLL_HW_PWRDN_CFG0);

	reg = clk_readl(XUSB_PLL_CFG0);
	reg &= ~XUSB_PLL_CFG0_UTMIPLL_LOCK_DLY;
	pll_writel_delay(reg, XUSB_PLL_CFG0);

	/* Enable HW control UTMIPLL */
	reg = clk_readl(UTMIPLL_HW_PWRDN_CFG0);
	reg |= UTMIPLL_HW_PWRDN_CFG0_SEQ_ENABLE;
	pll_writel_delay(reg, UTMIPLL_HW_PWRDN_CFG0);
}

/*
 * PLL post divider maps - two types: quasi-linear and exponential
 * post divider.
 */
#define PLL_QLIN_PDIV_MAX	16
static u8 pll_qlin_pdiv_to_p[PLL_QLIN_PDIV_MAX + 1] = {
/* PDIV: 0, 1, 2, 3, 4, 5, 6, 7,  8,  9, 10, 11, 12, 13, 14, 15, 16 */
/* p: */ 1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 30, 32 };

static u32 pll_qlin_p_to_pdiv(u32 p, u32 *pdiv)
{
	int i;

	if (p) {
		for (i = 0; i <= PLL_QLIN_PDIV_MAX; i++) {
			if (p <= pll_qlin_pdiv_to_p[i]) {
				if (pdiv)
					*pdiv = i;
				return pll_qlin_pdiv_to_p[i];
			}
		}
	}
	return -EINVAL;
}

#define PLL_EXPO_PDIV_MAX	7
static u8 pll_expo_pdiv_to_p[PLL_EXPO_PDIV_MAX + 1] = {
/* PDIV: 0, 1, 2, 3,  4,  5,  6,   7 */
/* p: */ 1, 2, 4, 8, 16, 32, 64, 128 };

static u32 pll_expo_p_to_pdiv(u32 p, u32 *pdiv)
{
	if (p) {
		u32 i = fls(p);
		if (i == ffs(p))
			i--;

		if (i <= PLL_EXPO_PDIV_MAX) {
			if (pdiv)
				*pdiv = i;
			return 1 << i;
		}
	}
	return -EINVAL;
}

/*
 * PLLCX: PLLC, PLLC2, PLLC3, PLLA1
 * Hybrid PLLs with dynamic ramp. Dynamic ramp is allowed for any transition
 * that changes NDIV only, while PLL is already locked.
 */
static void pllcx_check_defaults(struct clk *c, unsigned long input_rate)
{
	u32 default_val;

	default_val = PLLCX_MISC0_DEFAULT_VALUE & (~PLLCX_MISC0_RESET);
	PLL_MISC_CHK_DEFAULT(c, 0, default_val, PLLCX_MISC0_WRITE_MASK);

	default_val = PLLCX_MISC1_DEFAULT_VALUE & (~PLLCX_MISC1_IDDQ);
	PLL_MISC_CHK_DEFAULT(c, 1, default_val, PLLCX_MISC1_WRITE_MASK);

	default_val = PLLCX_MISC2_DEFAULT_VALUE;
	PLL_MISC_CHK_DEFAULT(c, 2, default_val, PLLCX_MISC2_WRITE_MASK);

	default_val = PLLCX_MISC3_DEFAULT_VALUE;
	PLL_MISC_CHK_DEFAULT(c, 3, default_val, PLLCX_MISC3_WRITE_MASK);
}

static void pllcx_set_defaults(struct clk *c, unsigned long input_rate)
{
	c->u.pll.defaults_set = true;

	if (clk_readl(c->reg) & c->u.pll.controls->enable_mask) {
		/* PLL is ON: only check if defaults already set */
		pllcx_check_defaults(c, input_rate);
		return;
	}

	/* Defaults assert PLL reset, and set IDDQ */
	clk_writel(PLLCX_MISC0_DEFAULT_VALUE, c->reg + c->u.pll.misc0);
	clk_writel(PLLCX_MISC1_DEFAULT_VALUE, c->reg + c->u.pll.misc1);
	clk_writel(PLLCX_MISC2_DEFAULT_VALUE, c->reg + c->u.pll.misc2);
	pll_writel_delay(PLLCX_MISC3_DEFAULT_VALUE, c->reg + c->u.pll.misc3);
}

#if PLLCX_USE_DYN_RAMP
static int pllcx_dyn_ramp(struct clk *c, struct clk_pll_freq_table *cfg)
{
	u32 reg;
	struct clk_pll_controls *ctrl = c->u.pll.controls;
	struct clk_pll_div_layout *divs = c->u.pll.div_layout;

	u32 val = clk_readl(c->reg);
	val &= ~divs->ndiv_mask;
	val |= cfg->n << divs->ndiv_shift;
	pll_writel_delay(val, c->reg);

	reg = pll_reg_idx_to_addr(c, ctrl->lock_reg_idx);
	tegra_pll_clk_wait_for_lock(c, reg, ctrl->lock_mask);

	return 0;
}
#else
#define pllcx_dyn_ramp NULL
#endif

static void tegra21_pllcx_clk_init(struct clk *c)
{
	tegra_pll_clk_init(c);
}

static struct clk_ops tegra_pllcx_ops = {
	.init			= tegra21_pllcx_clk_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

/*
 * PLLA
 * PLL with dynamic ramp and fractional SDM. Dynamic ramp is not used.
 * Fractional SDM is allowed to provide exact audio rates.
 */
static void plla_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 mask;
	u32 val = clk_readl(c->reg);
	c->u.pll.defaults_set = true;

	if (val & c->u.pll.controls->enable_mask) {
		/*
		 * PLL is ON: check if defaults already set, then set those
		 * that can be updated in flight.
		 */
		if (val & PLLA_BASE_IDDQ) {
			pr_warn("%s boot enabled with IDDQ set\n", c->name);
			c->u.pll.defaults_set = false;
		}

		val = PLLA_MISC0_DEFAULT_VALUE;	/* ignore lock enable */
		mask = PLLA_MISC0_LOCK_ENABLE | PLLA_MISC0_LOCK_OVERRIDE;
		PLL_MISC_CHK_DEFAULT(c, 0, val, ~mask & PLLA_MISC0_WRITE_MASK);

		val = PLLA_MISC2_DEFAULT_VALUE; /* ignore all but control bit */
		PLL_MISC_CHK_DEFAULT(c, 2, val, PLLA_MISC2_EN_DYNRAMP);

		/* Enable lock detect */
		val = clk_readl(c->reg + c->u.pll.misc0);
		val &= ~mask;
		val |= PLLA_MISC0_DEFAULT_VALUE & mask;
		pll_writel_delay(val, c->reg + c->u.pll.misc0);

		return;
	}

	/* set IDDQ, enable lock detect, disable dynamic ramp and SDM */
	val |= PLLA_BASE_IDDQ;
	clk_writel(val, c->reg);
	clk_writel(PLLA_MISC0_DEFAULT_VALUE, c->reg + c->u.pll.misc0);
	pll_writel_delay(PLLA_MISC2_DEFAULT_VALUE, c->reg + c->u.pll.misc2);
}

static void tegra21_plla_clk_init(struct clk *c)
{
	tegra_pll_clk_init(c);
}

static struct clk_ops tegra_plla_ops = {
	.init			= tegra21_plla_clk_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

/*
 * PLLD
 * PLL with fractional SDM.
 */
static void plld_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 val = clk_readl(c->reg);
	u32 mask = c->u.pll.div_layout->sdm_din_mask;

	c->u.pll.defaults_set = true;

	if (val & c->u.pll.controls->enable_mask) {
		/*
		 * PLL is ON: check if defaults already set, then set those
		 * that can be updated in flight.
		 */
		val = PLLD_MISC1_DEFAULT_VALUE;
		PLL_MISC_CHK_DEFAULT(c, 1, val, PLLD_MISC1_WRITE_MASK);

		/* ignore lock, DSI and SDM controls, make sure IDDQ not set */
		val = PLLD_MISC0_DEFAULT_VALUE & (~PLLD_MISC0_IDDQ);
		mask |= PLLD_MISC0_DSI_CLKENABLE | PLLD_MISC0_LOCK_ENABLE |
			PLLD_MISC0_LOCK_OVERRIDE | PLLD_MISC0_EN_SDM;
		PLL_MISC_CHK_DEFAULT(c, 0, val, ~mask & PLLD_MISC0_WRITE_MASK);

		/* Enable lock detect */
		mask = PLLD_MISC0_LOCK_ENABLE | PLLD_MISC0_LOCK_OVERRIDE;
		val = clk_readl(c->reg + c->u.pll.misc0);
		val &= ~mask;
		val |= PLLD_MISC0_DEFAULT_VALUE & mask;
		pll_writel_delay(val, c->reg + c->u.pll.misc0);

		return;
	}

	/* set IDDQ, enable lock detect, disable SDM */
	val = clk_readl(c->reg + c->u.pll.misc0) & PLLD_MISC0_DSI_CLKENABLE;
	clk_writel(val | PLLD_MISC0_DEFAULT_VALUE, c->reg + c->u.pll.misc0);
	pll_writel_delay(PLLD_MISC1_DEFAULT_VALUE, c->reg + c->u.pll.misc1);
}

static void tegra21_plld_clk_init(struct clk *c)
{
	tegra_pll_clk_init(c);
}

static int
tegra21_plld_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	u32 val, mask, reg;
	u32 clear = 0;

	switch (p) {
	case TEGRA_CLK_PLLD_CSI_OUT_ENB:
		mask = PLLD_BASE_CSI_CLKSOURCE;
		reg = c->reg;
		break;
	case TEGRA_CLK_MIPI_CSI_OUT_ENB:
		mask = 0;
		clear = PLLD_BASE_CSI_CLKSOURCE;
		reg = c->reg;
		break;
	case TEGRA_CLK_PLLD_DSI_OUT_ENB:
		mask = PLLD_MISC0_DSI_CLKENABLE;
		reg = c->reg + c->u.pll.misc0;
		break;
	case TEGRA_CLK_PLLD_MIPI_MUX_SEL:
		mask = PLLD_BASE_DSI_MUX_MASK;
		reg = c->reg;
		break;
	default:
		return -EINVAL;
	}

	val = clk_readl(reg);
	if (setting) {
		val |= mask;
		val &= ~clear;
	} else
		val &= ~mask;
	clk_writel(val, reg);
	return 0;
}

static struct clk_ops tegra_plld_ops = {
	.init			= tegra21_plld_clk_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
	.clk_cfg_ex		= tegra21_plld_clk_cfg_ex,
};

/*
 * PLLD2, PLLDP
 * PLL with fractional SDM and Spread Spectrum (SDM is a must if SSC is used).
 */
static void plldss_defaults(struct clk *c, u32 misc0_val, u32 misc1_val,
			    u32 misc2_val, u32 misc3_val)
{
	u32 default_val;
	u32 val = clk_readl(c->reg);
	c->u.pll.defaults_set = true;

	if (val & c->u.pll.controls->enable_mask) {
		/*
		 * PLL is ON: check if defaults already set, then set those
		 * that can be updated in flight.
		 */
		if (val & PLLDSS_BASE_IDDQ) {
			pr_warn("%s boot enabled with IDDQ set\n", c->name);
			c->u.pll.defaults_set = false;
		}

		default_val = misc0_val;
		PLL_MISC_CHK_DEFAULT(c, 0, default_val,	/* ignore lock enable */
				     PLLDSS_MISC0_WRITE_MASK &
				     (~PLLDSS_MISC0_LOCK_ENABLE));

		/*
		 * If SSC is used, check all settings, otherwise just confirm
		 * that SSC is not used on boot as well. Do nothing when using
		 * this function for PLLC4 that has only MISC0.
		 */
		if (c->u.pll.controls->ssc_en_mask) {
			default_val = misc1_val;
			PLL_MISC_CHK_DEFAULT(c, 1, default_val,
					     PLLDSS_MISC1_CFG_WRITE_MASK);
			default_val = misc2_val;
			PLL_MISC_CHK_DEFAULT(c, 2, default_val,
					     PLLDSS_MISC2_CTRL1_WRITE_MASK);
			default_val = misc3_val;
			PLL_MISC_CHK_DEFAULT(c, 3, default_val,
					     PLLDSS_MISC3_CTRL2_WRITE_MASK);
		} else if (c->u.pll.misc1) {
			default_val = misc1_val;
			PLL_MISC_CHK_DEFAULT(c, 1, default_val,
					     PLLDSS_MISC1_CFG_WRITE_MASK &
					     (~PLLDSS_MISC1_CFG_EN_SDM));
		}

		/* Enable lock detect */
		if (val & PLLDSS_BASE_LOCK_OVERRIDE) {
			val &= ~PLLDSS_BASE_LOCK_OVERRIDE;
			clk_writel(val, c->reg);
		}

		val = clk_readl(c->reg + c->u.pll.misc0);
		val &= ~PLLDSS_MISC0_LOCK_ENABLE;
		val |= misc0_val & PLLDSS_MISC0_LOCK_ENABLE;
		pll_writel_delay(val, c->reg + c->u.pll.misc0);

		return;
	}

	/* set IDDQ, enable lock detect, configure SDM/SSC  */
	val |= PLLDSS_BASE_IDDQ;
	val &= ~PLLDSS_BASE_LOCK_OVERRIDE;
	clk_writel(val, c->reg);

	/* When using this function for PLLC4 exit here */
	if (!c->u.pll.misc1) {
		pll_writel_delay(misc0_val, c->reg + c->u.pll.misc0);
		return;
	}

	clk_writel(misc0_val, c->reg + c->u.pll.misc0);
	clk_writel(misc1_val & (~PLLDSS_MISC1_CFG_EN_SSC),
		   c->reg + c->u.pll.misc1); /* if SSC used set by 1st enable */
	clk_writel(misc2_val, c->reg + c->u.pll.misc2);
	pll_writel_delay(misc3_val, c->reg + c->u.pll.misc3);
}

static void plldss_select_ref(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	u32 ref = (val & PLLDSS_BASE_REF_SEL_MASK) >> PLLDSS_BASE_REF_SEL_SHIFT;

	/*
	 * The only productized reference clock is tegra_pll_ref. Made sure
	 * it is selected on boot. If pll is enabled, shut it down before
	 * changing reference selection.
	 */
	if (ref) {
		if (val & c->u.pll.controls->enable_mask) {
			WARN(1, "%s boot enabled with not supported ref %u\n",
			     c->name, ref);
			val &= ~c->u.pll.controls->enable_mask;
			pll_writel_delay(val, c->reg);
		}
		val &= ~PLLDSS_BASE_REF_SEL_MASK;
		pll_writel_delay(val, c->reg);
	}
}

static void plld2_set_defaults(struct clk *c, unsigned long input_rate)
{
	plldss_defaults(c, PLLD2_MISC0_DEFAULT_VALUE,
			PLLD2_MISC1_CFG_DEFAULT_VALUE,
			PLLD2_MISC2_CTRL1_DEFAULT_VALUE,
			PLLD2_MISC3_CTRL2_DEFAULT_VALUE);
}

static void tegra21_plld2_clk_init(struct clk *c)
{
	if (PLLD2_MISC1_CFG_DEFAULT_VALUE & PLLDSS_MISC1_CFG_EN_SSC) {
		/* SSC requires SDM enabled */
		BUILD_BUG_ON(!(PLLD2_MISC1_CFG_DEFAULT_VALUE &
			       PLLDSS_MISC1_CFG_EN_SDM));
	} else {
		/* SSC should be disabled */
		c->u.pll.controls->ssc_en_mask = 0;
	}
	plldss_select_ref(c);

	tegra_pll_clk_init(c);
}

static struct clk_ops tegra_plld2_ops = {
	.init			= tegra21_plld2_clk_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

static void plldp_set_defaults(struct clk *c, unsigned long input_rate)
{
	plldss_defaults(c, PLLDP_MISC0_DEFAULT_VALUE,
			PLLDP_MISC1_CFG_DEFAULT_VALUE,
			PLLDP_MISC2_CTRL1_DEFAULT_VALUE,
			PLLDP_MISC3_CTRL2_DEFAULT_VALUE);
}

static void tegra21_plldp_clk_init(struct clk *c)
{

	if (PLLDP_MISC1_CFG_DEFAULT_VALUE & PLLDSS_MISC1_CFG_EN_SSC) {
		/* SSC requires SDM enabled */
		BUILD_BUG_ON(!(PLLDP_MISC1_CFG_DEFAULT_VALUE &
			       PLLDSS_MISC1_CFG_EN_SDM));
	} else {
		/* SSC should be disabled */
		c->u.pll.controls->ssc_en_mask = 0;
	}
	plldss_select_ref(c);

	tegra_pll_clk_init(c);
}

static struct clk_ops tegra_plldp_ops = {
	.init			= tegra21_plldp_clk_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

/*
 * PLLC4
 * Base and misc0 layout is the same as PLLD2/PLLDP, but no SDM/SSC support.
 * VCO is exposed to the clock tree via fixed 1/3 and 1/5 dividers.
 */
static void pllc4_set_defaults(struct clk *c, unsigned long input_rate)
{
	plldss_defaults(c, PLLC4_MISC0_DEFAULT_VALUE, 0, 0, 0);
}

static void tegra21_pllc4_vco_init(struct clk *c)
{
	unsigned long input_rate = clk_get_rate(c->parent);
	unsigned long cf = input_rate / pll_get_fixed_mdiv(c, input_rate);
	pllc4_set_fixed_rates(cf);

	plldss_select_ref(c);
	tegra_pll_clk_init(c);
}

static struct clk_ops tegra_pllc4_vco_ops = {
	.init			= tegra21_pllc4_vco_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

/*
 * PLLRE
 * VCO is exposed to the clock tree directly along with post-divider output
 */
static void pllre_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 mask;
	u32 val = clk_readl(c->reg);
	c->u.pll.defaults_set = true;

	if (val & c->u.pll.controls->enable_mask) {
		/*
		 * PLL is ON: check if defaults already set, then set those
		 * that can be updated in flight.
		 */
		val &= PLLRE_BASE_DEFAULT_MASK;
		if (val != PLLRE_BASE_DEFAULT_VALUE) {
			pr_warn("%s boot base 0x%x : expected 0x%x\n",
				(c)->name, val, PLLRE_BASE_DEFAULT_VALUE);
			pr_warn("(comparison mask = 0x%x)\n",
				PLLRE_BASE_DEFAULT_MASK);
			c->u.pll.defaults_set = false;
		}

		/* Ignore lock enable */
		val = PLLRE_MISC0_DEFAULT_VALUE & (~PLLRE_MISC0_IDDQ);
		mask = PLLRE_MISC0_LOCK_ENABLE | PLLRE_MISC0_LOCK_OVERRIDE;
		PLL_MISC_CHK_DEFAULT(c, 0, val, ~mask & PLLRE_MISC0_WRITE_MASK);

		/* Enable lock detect */
		val = clk_readl(c->reg + c->u.pll.misc0);
		val &= ~mask;
		val |= PLLRE_MISC0_DEFAULT_VALUE & mask;
		pll_writel_delay(val, c->reg + c->u.pll.misc0);

		return;
	}

	/* set IDDQ, enable lock detect */
	val &= ~PLLRE_BASE_DEFAULT_MASK;
	val |= PLLRE_BASE_DEFAULT_VALUE & PLLRE_BASE_DEFAULT_MASK;
	clk_writel(val, c->reg);
	pll_writel_delay(PLLRE_MISC0_DEFAULT_VALUE, c->reg + c->u.pll.misc0);
}

static void tegra21_pllre_vco_init(struct clk *c)
{
	tegra_pll_clk_init(c);
}

static struct clk_ops tegra_pllre_vco_ops = {
	.init			= tegra21_pllre_vco_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

/*
 * PLLU
 * VCO is exposed to the clock tree directly along with post-divider output.
 * Both VCO and post-divider output rates are fixed at 480MHz and 240MHz,
 * respectively.
 */
static void pllu_check_defaults(struct clk *c, bool hw_control)
{
	u32 val, mask;

	/* Ignore lock enable (will be set) and IDDQ if under h/w control */
	val = PLLU_MISC0_DEFAULT_VALUE & (~PLLU_MISC0_IDDQ);
	mask = PLLU_MISC0_LOCK_ENABLE | (hw_control ? PLLU_MISC0_IDDQ : 0);
	PLL_MISC_CHK_DEFAULT(c, 0, val, ~mask & PLLU_MISC0_WRITE_MASK);

	val = PLLU_MISC1_DEFAULT_VALUE;
	mask = PLLU_MISC1_LOCK_OVERRIDE;
	PLL_MISC_CHK_DEFAULT(c, 1, val, ~mask & PLLU_MISC1_WRITE_MASK);
}

static void pllu_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 val = clk_readl(c->reg);
	c->u.pll.defaults_set = true;

	if (val & c->u.pll.controls->enable_mask) {
		/*
		 * PLL is ON: check if defaults already set, then set those
		 * that can be updated in flight.
		 */
		pllu_check_defaults(c, false);

		/* Enable lock detect */
		val = clk_readl(c->reg + c->u.pll.misc0);
		val &= ~PLLU_MISC0_LOCK_ENABLE;
		val |= PLLU_MISC0_DEFAULT_VALUE & PLLU_MISC0_LOCK_ENABLE;
		pll_writel_delay(val, c->reg + c->u.pll.misc0);

		val = clk_readl(c->reg + c->u.pll.misc1);
		val &= ~PLLU_MISC1_LOCK_OVERRIDE;
		val |= PLLU_MISC1_DEFAULT_VALUE & PLLU_MISC1_LOCK_OVERRIDE;
		pll_writel_delay(val, c->reg + c->u.pll.misc1);

		return;
	}

	/* set IDDQ, enable lock detect */
	clk_writel(PLLU_MISC0_DEFAULT_VALUE, c->reg + c->u.pll.misc0);
	pll_writel_delay(PLLU_MISC1_DEFAULT_VALUE, c->reg + c->u.pll.misc1);
}

static void tegra21_pllu_vco_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	pll_u = c;

	/*
	 * If PLLU state is already under h/w control just check defaults, and
	 * verify expected fixed VCO rate (pll dividers can still be read from
	 * the base register).
	 */
	if (!(val & PLLU_BASE_OVERRIDE)) {
		struct clk_pll_freq_table cfg = { };
		c->state = (val & c->u.pll.controls->enable_mask) ? ON : OFF;

		c->u.pll.defaults_set = true;
		pllu_check_defaults(c, true);
		pll_base_parse_cfg(c, &cfg);
		pll_clk_set_gain(c, &cfg);

		pll_clk_verify_fixed_rate(c);
		pr_info("%s: boot with h/w control already set\n", c->name);
		return;
	}

	/* S/w controlled initialization */
	tegra_pll_clk_init(c);
}

static void tegra21_pllu_hw_ctrl_set(struct clk *c)
{
	u32 val = clk_readl(c->reg);

	/* Put PLLU under h/w control (if not already) */
	if (val & PLLU_BASE_OVERRIDE) {
		val &= ~PLLU_BASE_OVERRIDE;
		pll_writel_delay(val, c->reg);

		val = clk_readl(PLLU_HW_PWRDN_CFG0);
		val |= PLLU_HW_PWRDN_CFG0_IDDQ_PD_INCLUDE |
			PLLU_HW_PWRDN_CFG0_USE_SWITCH_DETECT |
			PLLU_HW_PWRDN_CFG0_USE_LOCKDET;
		val &= ~(PLLU_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL |
			 PLLU_HW_PWRDN_CFG0_CLK_SWITCH_SWCTL);
		clk_writel(val, PLLU_HW_PWRDN_CFG0);

		val = clk_readl(XUSB_PLL_CFG0);
		val &= ~XUSB_PLL_CFG0_PLLU_LOCK_DLY_MASK;
		pll_writel_delay(val, XUSB_PLL_CFG0);

		val = clk_readl(PLLU_HW_PWRDN_CFG0);
		val |= PLLU_HW_PWRDN_CFG0_SEQ_ENABLE;
		pll_writel_delay(val, PLLU_HW_PWRDN_CFG0);
	}

	/* Disable clock branch to UTMIP PLL (using OSC directly) */
	val = clk_readl(c->reg);
	val &= ~PLLU_BASE_CLKENABLE_USB;
	clk_writel(val, c->reg);

	/* Put UTMIP PLL under h/w control  (if not already) */
	val = clk_readl(UTMIPLL_HW_PWRDN_CFG0);
	if (!(val & UTMIPLL_HW_PWRDN_CFG0_SEQ_ENABLE))
		tegra21_utmi_param_configure(c);
}

static int tegra21_pllu_clk_enable(struct clk *c)
{
	int ret = tegra_pll_clk_enable(c);
	if (ret)
		return ret;

	tegra21_pllu_hw_ctrl_set(c);
	return 0;
}

static struct clk_ops tegra_pllu_vco_ops = {
	.init			= tegra21_pllu_vco_init,
	.enable			= tegra21_pllu_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

/*
 * PLLP
 * VCO is exposed to the clock tree directly along with post-divider output.
 * Both VCO and post-divider output rates are fixed at 408MHz and 204MHz,
 * respectively.
 */
static void pllp_check_defaults(struct clk *c, bool enabled)
{
	u32 val, mask;

	/* Ignore lock enable (will be set), make sure not in IDDQ if enabled */
	val = PLLP_MISC0_DEFAULT_VALUE & (~PLLP_MISC0_IDDQ);
	mask = PLLP_MISC0_LOCK_ENABLE | PLLP_MISC0_LOCK_OVERRIDE;
	if (!enabled)
		mask |= PLLP_MISC0_IDDQ;
	PLL_MISC_CHK_DEFAULT(c, 0, val, ~mask & PLLP_MISC0_WRITE_MASK);

	/* Ignore branch controls */
	val = PLLP_MISC1_DEFAULT_VALUE;
	mask = PLLP_MISC1_HSIO_EN | PLLP_MISC1_XUSB_EN;
	PLL_MISC_CHK_DEFAULT(c, 1, val, ~mask & PLLP_MISC1_WRITE_MASK);
}

static void pllp_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 mask;
	u32 val = clk_readl(c->reg);
	c->u.pll.defaults_set = true;

	if (val & c->u.pll.controls->enable_mask) {
		/*
		 * PLL is ON: check if defaults already set, then set those
		 * that can be updated in flight.
		 */
		pllp_check_defaults(c, true);

		/* Enable lock detect */
		val = clk_readl(c->reg + c->u.pll.misc0);
		mask = PLLP_MISC0_LOCK_ENABLE | PLLP_MISC0_LOCK_OVERRIDE;
		val &= ~mask;
		val |= PLLP_MISC0_DEFAULT_VALUE & mask;
		pll_writel_delay(val, c->reg + c->u.pll.misc0);

		return;
	}

	/* set IDDQ, enable lock detect */
	clk_writel(PLLP_MISC0_DEFAULT_VALUE, c->reg + c->u.pll.misc0);

	/* Preserve branch control */
	val = clk_readl(c->reg + c->u.pll.misc1);
	mask = PLLP_MISC1_HSIO_EN | PLLP_MISC1_XUSB_EN;
	val &= mask;
	val |= ~mask & PLLP_MISC1_DEFAULT_VALUE;
	pll_writel_delay(val, c->reg + c->u.pll.misc1);
}

static void pllp_vco_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);

	/*
	 * If PLLP state is already under h/w control just check defaults, and
	 * set expected fixed VCO rate (pll dividers cannot be read in this case
	 * from the base register).
	 */
	if (!(val & PLLP_BASE_OVERRIDE)) {
		const struct clk_pll_freq_table *sel;
		unsigned long input_rate = clk_get_rate(c->parent);
		c->state = (val & c->u.pll.controls->enable_mask) ? ON : OFF;

		c->u.pll.defaults_set = true;
		pllp_check_defaults(c, c->state == ON);

		for (sel = c->u.pll.freq_table; sel->input_rate != 0; sel++) {
			if (sel->input_rate == input_rate &&
				sel->output_rate == c->u.pll.fixed_rate) {
				c->mul = sel->n;
				c->div = sel->m;
				return;
			}
		}
		WARN(1, "%s: unexpected fixed rate %lu\n",
		     c->name, c->u.pll.fixed_rate);

		c->mul = c->u.pll.fixed_rate / 100000;
		c->div = input_rate / 100000;
		pll_clk_verify_fixed_rate(c);
		return;
	}

	/* S/w controlled initialization */
	tegra_pll_clk_init(c);
}

static void tegra21_pllp_vco_init(struct clk *c)
{
	pllp_vco_init(c);
	tegra21_pllp_init_dependencies(c->u.pll.fixed_rate);
}

static struct clk_ops tegra_pllp_vco_ops = {
	.init			= tegra21_pllp_vco_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

/*
 * PLLX
 * PLL with dynamic ramp and fractional SDM. Dynamic ramp is allowed for any
 * transition that changes NDIV only, while PLL is already locked. SDM is not
 * used and always disabled.
 */
static void pllx_get_dyn_steps(struct clk *c, unsigned long input_rate,
				u32 *step_a, u32 *step_b)
{
	input_rate /= pll_get_fixed_mdiv(c, input_rate); /* cf rate */
	switch (input_rate) {
	case 12000000:
	case 12800000:
	case 13000000:
		*step_a = 0x2B;
		*step_b = 0x0B;
		return;
	case 19200000:
		*step_a = 0x12;
		*step_b = 0x08;
		return;
	case 38400000:
		*step_a = 0x04;
		*step_b = 0x05;
		return;
	default:
		pr_err("%s: Unexpected reference rate %lu\n",
			__func__, input_rate);
		BUG();
	}
}

static void pllx_check_defaults(struct clk *c, unsigned long input_rate)
{
	u32 default_val;

	default_val = PLLX_MISC0_DEFAULT_VALUE;
	PLL_MISC_CHK_DEFAULT(c, 0, default_val,	/* ignore lock enable */
			     PLLX_MISC0_WRITE_MASK & (~PLLX_MISC0_LOCK_ENABLE));

	default_val = PLLX_MISC1_DEFAULT_VALUE;
	PLL_MISC_CHK_DEFAULT(c, 1, default_val, PLLX_MISC1_WRITE_MASK);

	default_val = PLLX_MISC2_DEFAULT_VALUE; /* ignore all but control bit */
	PLL_MISC_CHK_DEFAULT(c, 2, default_val, PLLX_MISC2_EN_DYNRAMP);

	default_val = PLLX_MISC3_DEFAULT_VALUE & (~PLLX_MISC3_IDDQ);
	PLL_MISC_CHK_DEFAULT(c, 3, default_val, PLLX_MISC3_WRITE_MASK);

	default_val = PLLX_MISC4_DEFAULT_VALUE;
	PLL_MISC_CHK_DEFAULT(c, 4, default_val, PLLX_MISC4_WRITE_MASK);

	default_val = PLLX_MISC5_DEFAULT_VALUE;
	PLL_MISC_CHK_DEFAULT(c, 5, default_val, PLLX_MISC5_WRITE_MASK);
}

static void pllx_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 val;
	u32 step_a, step_b;

	c->u.pll.defaults_set = true;

	/* Get ready dyn ramp state machine settings */
	pllx_get_dyn_steps(c, input_rate, &step_a, &step_b);
	val = PLLX_MISC2_DEFAULT_VALUE & (~PLLX_MISC2_DYNRAMP_STEPA_MASK) &
		(~PLLX_MISC2_DYNRAMP_STEPB_MASK);
	val |= step_a << PLLX_MISC2_DYNRAMP_STEPA_SHIFT;
	val |= step_b << PLLX_MISC2_DYNRAMP_STEPB_SHIFT;

	if (clk_readl(c->reg) & c->u.pll.controls->enable_mask) {
		/*
		 * PLL is ON: check if defaults already set, then set those
		 * that can be updated in flight.
		 */
		pllx_check_defaults(c, input_rate);

		/* Configure dyn ramp, disable lock override */
		clk_writel(val, c->reg + c->u.pll.misc2);

		/* Enable lock detect */
		val = clk_readl(c->reg + c->u.pll.misc0);
		val &= ~PLLX_MISC0_LOCK_ENABLE;
		val |= PLLX_MISC0_DEFAULT_VALUE & PLLX_MISC0_LOCK_ENABLE;
		pll_writel_delay(val, c->reg + c->u.pll.misc0);

		return;
	}

	/* Enable lock detect and CPU output */
	clk_writel(PLLX_MISC0_DEFAULT_VALUE, c->reg + c->u.pll.misc0);

	/* Setup */
	clk_writel(PLLX_MISC1_DEFAULT_VALUE, c->reg + c->u.pll.misc1);

	/* Configure dyn ramp state machine, disable lock override */
	clk_writel(val, c->reg + c->u.pll.misc2);

	/* Set IDDQ */
	clk_writel(PLLX_MISC3_DEFAULT_VALUE, c->reg + c->u.pll.misc3);

	/* Disable SDM */
	clk_writel(PLLX_MISC4_DEFAULT_VALUE, c->reg + c->u.pll.misc4);
	pll_writel_delay(PLLX_MISC5_DEFAULT_VALUE, c->reg + c->u.pll.misc5);
}

#if PLLX_USE_DYN_RAMP
static int pllx_dyn_ramp(struct clk *c, struct clk_pll_freq_table *cfg)
{
	u32 reg, val, base, ndiv_new_mask;
	struct clk_pll_controls *ctrl = c->u.pll.controls;
	struct clk_pll_div_layout *divs = c->u.pll.div_layout;

	reg = pll_reg_idx_to_addr(c, divs->ndiv_new_reg_idx);
	ndiv_new_mask = (divs->ndiv_mask >> divs->ndiv_shift) <<
		divs->ndiv_new_shift;
	val = clk_readl(reg) & (~ndiv_new_mask);
	val |= cfg->n << divs->ndiv_new_shift;
	pll_writel_delay(val, reg);

	reg = pll_reg_idx_to_addr(c, ctrl->dramp_ctrl_reg_idx);
	val = clk_readl(reg);
	val |= ctrl->dramp_en_mask;
	pll_writel_delay(val, reg);
	tegra_pll_clk_wait_for_lock(c, reg, ctrl->dramp_done_mask);

	base = clk_readl(c->reg) & (~divs->ndiv_mask);
	base |= cfg->n << divs->ndiv_shift;
	pll_writel_delay(base, c->reg);

	val &= ~ctrl->dramp_en_mask;
	pll_writel_delay(val, reg);

	pr_debug("%s: dynamic ramp to m = %u n = %u p = %u, Fout = %lu kHz\n",
		 c->name, cfg->m, cfg->n, cfg->p,
		 cfg->input_rate / cfg->m * cfg->n / cfg->p / 1000);

	return 0;
}
#endif

static void tegra21_pllx_clk_init(struct clk *c)
{
	/* Only s/w dyn ramp control is supported */
	u32 val = clk_readl(PLLX_HW_CTRL_CFG);
	BUG_ON(!(val & PLLX_HW_CTRL_CFG_SWCTRL) && !tegra_platform_is_linsim());

	tegra_pll_clk_init(c);
}

static struct clk_ops tegra_pllx_ops = {
	.init			= tegra21_pllx_clk_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra_pll_clk_set_rate,
};

/*
 * Dual PLLM/PLLMB
 *
 * Identical PLLs used exclusively as memory clock source. Configuration
 * and control fields in CAR module are separate for both PLL, and can
 * be set independently. Most control fields (ENABLE, IDDQ, M/N/P dividers)
 * have override counterparts in PMC module, however there is only one set
 * of registers that applied either to PLLM or PLLMB depending on override
 * VCO selection.
 *
 * By default selection between PLLM and PLLMB outputs is controlled by EMC
 * clock switch h/w state machine. If any s/w override either in CAR, or in
 * PMC is enabled on boot, PLLMB is no longer can be used as EMC parent.
 *
 * Only PLLM is used as boot PLL, setup by boot-rom.
 */
static void pllm_check_defaults(struct clk *c)
{
	u32 val;
	c->u.pll.defaults_set = true;

	/*
	 * PLLM is setup on boot/suspend exit by boot-rom.
	 * Just enable lock, and check default configuration:
	 * - PLLM syncmux is under h/w control
	 * - PMC override is disabled
	 */
	val = clk_readl(c->reg + c->u.pll.misc0);
	val &= ~PLLM_MISC0_LOCK_OVERRIDE;
	val |= PLLM_MISC0_LOCK_ENABLE;
	pll_writel_delay(val, c->reg + c->u.pll.misc0);

	/* FIXME: no support for invalid configurations; replace with BUG() */
	if (val & PLLM_MISC0_SYNCMUX_CTRL_MASK) {
		WARN(1, "%s: Unsupported config: PLLM syncmux s/w control\n",
		     __func__);
		c->u.pll.defaults_set = false;
	}

	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
	if (val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE) {
		WARN(1, "%s: Unsupported config: PMC override enabled\n",
		     __func__);
		c->u.pll.defaults_set = false;

		/* No boot on PLLMB */
		BUG_ON(val & PMC_PLLP_WB0_OVERRIDE_PLLM_SEL_VCO);
	}
}

static void tegra21_pllm_clk_init(struct clk *c)
{
	u32 val;
	unsigned long input_rate = clk_get_rate(c->parent);
	unsigned long cf = input_rate / pll_get_fixed_mdiv(c, input_rate);

	struct clk_pll_freq_table cfg = { };
	struct clk_pll_controls *ctrl = c->u.pll.controls;
	struct clk_pll_div_layout *divs = c->u.pll.div_layout;
	BUG_ON(!ctrl || !divs);

	/* clip vco_min to exact multiple of comparison rate */
	c->u.pll.vco_min = DIV_ROUND_UP(c->u.pll.vco_min, cf) * cf;
	c->min_rate = DIV_ROUND_UP(c->u.pll.vco_min,
				   divs->pdiv_to_p[divs->pdiv_max]);

	/* Check PLLM setup */
	pllm_check_defaults(c);

	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
	if ((val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE) &&
	    (~val & PMC_PLLP_WB0_OVERRIDE_PLLM_SEL_VCO)) {
		c->state = (val & PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE) ? ON : OFF;
		if (c->state == ON) {
			/* PLLM boot control from PMC + enabled: record rate */
			val = pmc_readl(PMC_PLLM_WB0_OVERRIDE);
			cfg.m = (val & divs->mdiv_mask) >> divs->mdiv_shift;
			cfg.n = (val & divs->ndiv_mask) >> divs->ndiv_shift;

			val = pmc_readl(PMC_PLLM_WB0_OVERRIDE_2);
			cfg.p = (val & PMC_PLLM_WB0_OVERRIDE_2_DIVP_MASK) >>
				PMC_PLLM_WB0_OVERRIDE_2_DIVP_SHIFT;
			cfg.p = divs->pdiv_to_p[cfg.p];
			pll_clk_set_gain(c, &cfg);
			return;
		}
	} else {
		val = clk_readl(c->reg);
		c->state = (val & ctrl->enable_mask) ? ON : OFF;
		if (c->state == ON) {
			/* PLLM boot control from CAR + enabled: record rate */
			pll_base_parse_cfg(c, &cfg);
			pll_clk_set_gain(c, &cfg);
			return;
		}
	}

	/* PLLM is disabled on boot: set rate close to to 1/4 of minimum VCO */
	c->ops->set_rate(c, c->u.pll.vco_min / 4);
}

static int tegra21_pllm_clk_enable(struct clk *c)
{
	u32 val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);

	if ((val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE) &&
	    (~val & PMC_PLLP_WB0_OVERRIDE_PLLM_SEL_VCO)) {
		pr_debug("%s on clock %s\n", __func__, c->name);

		val &= ~PMC_PLLP_WB0_OVERRIDE_PLLM_IDDQ;
		pmc_writel(val, PMC_PLLP_WB0_OVERRIDE);
		val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
		udelay(5); /* out of IDDQ delay to 5us */

		val |= PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE;
		pmc_writel(val, PMC_PLLP_WB0_OVERRIDE);
		val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);

		tegra_pll_clk_wait_for_lock(c, c->reg, PLLM_BASE_LOCK);
		return 0;
	}
	return tegra_pll_clk_enable(c);
}

static void tegra21_pllm_clk_disable(struct clk *c)
{
	u32 val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);

	if ((val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE) &&
	    (~val & PMC_PLLP_WB0_OVERRIDE_PLLM_SEL_VCO)) {
		pr_debug("%s on clock %s\n", __func__, c->name);

		val &= ~PMC_PLLP_WB0_OVERRIDE_PLLM_ENABLE;
		pmc_writel(val, PMC_PLLP_WB0_OVERRIDE);
		val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
		udelay(1);

		val |= PMC_PLLP_WB0_OVERRIDE_PLLM_IDDQ;
		pmc_writel(val, PMC_PLLP_WB0_OVERRIDE);
		val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
		udelay(1);
		return;
	}
	tegra_pll_clk_disable(c);
}

static int tegra21_pllm_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, pdiv;
	struct clk_pll_freq_table cfg = { };
	unsigned long input_rate = clk_get_rate(c->parent);

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->state == ON) {
		if (rate != clk_get_rate_locked(c)) {
			pr_err("%s: Can not change memory %s rate in flight\n",
			       __func__, c->name);
			return -EINVAL;
		}
		return 0;
	}

	if (pll_clk_find_cfg(c, &cfg, rate, input_rate, &pdiv))
		return -EINVAL;

	pll_clk_set_gain(c, &cfg);

	val = pmc_readl(PMC_PLLP_WB0_OVERRIDE);
	if ((val & PMC_PLLP_WB0_OVERRIDE_PLLM_OVERRIDE) &&
	    (~val & PMC_PLLP_WB0_OVERRIDE_PLLM_SEL_VCO)) {
		struct clk_pll_div_layout *divs = c->u.pll.div_layout;

		val = pmc_readl(PMC_PLLM_WB0_OVERRIDE_2);
		val &= ~PMC_PLLM_WB0_OVERRIDE_2_DIVP_MASK;
		val |= pdiv << PMC_PLLM_WB0_OVERRIDE_2_DIVP_SHIFT;
		pmc_writel(val, PMC_PLLM_WB0_OVERRIDE_2);

		val = pmc_readl(PMC_PLLM_WB0_OVERRIDE);
		val &= ~(divs->mdiv_mask | divs->ndiv_mask);
		val |= cfg.m << divs->mdiv_shift | cfg.n << divs->ndiv_shift;
		pmc_writel(val, PMC_PLLM_WB0_OVERRIDE);
		val = pmc_readl(PMC_PLLM_WB0_OVERRIDE);
		udelay(1);
	} else {
		val = clk_readl(c->reg);
		pll_base_set_div(c, cfg.m, cfg.n, pdiv, val);
	}

	return 0;
}

static struct clk_ops tegra_pllm_ops = {
	.init			= tegra21_pllm_clk_init,
	.enable			= tegra21_pllm_clk_enable,
	.disable		= tegra21_pllm_clk_disable,
	.set_rate		= tegra21_pllm_clk_set_rate,
};

/* PLLMB */
static void pllmb_set_defaults(struct clk *c, unsigned long input_rate)
{
	u32 mask, val = clk_readl(c->reg);
	c->u.pll.defaults_set = true;

	if (val & c->u.pll.controls->enable_mask) {
		/*
		 * PLL is ON: check if defaults already set, then set those
		 * that can be updated in flight.
		 */
		val = PLLMB_MISC0_DEFAULT_VALUE & (~PLLMB_MISC0_IDDQ);
		mask = PLLMB_MISC0_LOCK_ENABLE | PLLMB_MISC0_LOCK_OVERRIDE;
		PLL_MISC_CHK_DEFAULT(c, 0, val, ~mask & PLLMB_MISC0_WRITE_MASK);

		/* Enable lock detect */
		val = clk_readl(c->reg + c->u.pll.misc0);
		val &= ~mask;
		val |= PLLMB_MISC0_DEFAULT_VALUE & mask;
		pll_writel_delay(val, c->reg + c->u.pll.misc0);

		return;
	}

	/* set IDDQ, enable lock detect */
	pll_writel_delay(PLLMB_MISC0_DEFAULT_VALUE, c->reg + c->u.pll.misc0);
}

static int tegra21_pllmb_clk_set_rate(struct clk *c, unsigned long rate)
{
	if (c->state == ON) {
		if (rate != clk_get_rate_locked(c)) {
			pr_err("%s: Can not change memory %s rate in flight\n",
			       __func__, c->name);
			return -EINVAL;
		}
		return 0;
	}
	return tegra_pll_clk_set_rate(c, rate);
}

static void tegra21_pllmb_clk_init(struct clk *c)
{
	tegra_pll_clk_init(c);
}

static struct clk_ops tegra_pllmb_ops = {
	.init			= tegra21_pllmb_clk_init,
	.enable			= tegra_pll_clk_enable,
	.disable		= tegra_pll_clk_disable,
	.set_rate		= tegra21_pllmb_clk_set_rate,
};

/*
 * PLLE
 * Analog interpolator based SS PLL (with optional SDM SS - not used).
 */
static void select_pll_e_input(struct clk *c)
{
	struct clk *p;
	u32 aux_reg = clk_readl(PLLE_AUX);

#if USE_PLLE_INPUT_PLLRE
	aux_reg |= PLLE_AUX_PLLRE_SEL;
	p = c->inputs[2].input;
#else
	aux_reg &= ~(PLLE_AUX_PLLRE_SEL | PLLE_AUX_PLLP_SEL);
	p = c->inputs[0].input;
#endif
	if (p != c->parent)
		tegra_clk_prepare_enable(p);

	pll_writel_delay(aux_reg, PLLE_AUX);

	if (p != c->parent) {
		tegra_clk_disable_unprepare(c->parent);
		clk_reparent(c, p);
	}
}

static void tegra21_plle_clk_init(struct clk *c)
{
	u32 val, p;
	struct clk *pll_ref = tegra_get_clock_by_name("pll_ref");
	struct clk *re_vco = tegra_get_clock_by_name("pll_re_vco");
	struct clk *pllp = tegra_get_clock_by_name("pllp");
#if USE_PLLE_INPUT_PLLRE
	struct clk *ref = re_vco;
#else
	struct clk *ref = pll_ref;
#endif

	val = clk_readl(c->reg);
	c->state = (val & PLLE_BASE_ENABLE) ? ON : OFF;

	c->mul = (val & PLLE_BASE_DIVN_MASK) >> PLLE_BASE_DIVN_SHIFT;
	c->div = (val & PLLE_BASE_DIVM_MASK) >> PLLE_BASE_DIVM_SHIFT;
	p = (val & PLLE_BASE_DIVCML_MASK) >> PLLE_BASE_DIVCML_SHIFT;
	c->div *= pll_qlin_pdiv_to_p[p];

	val = clk_readl(PLLE_AUX);
	c->parent = (val & PLLE_AUX_PLLRE_SEL) ? re_vco :
		(val & PLLE_AUX_PLLP_SEL) ? pllp : pll_ref;
	if (c->parent != ref) {
		if (c->state == ON) {
			WARN(1, "%s: pll_e is left enabled with %s input\n",
			     __func__, c->parent->name);
		}
	}

#if USE_PLLE_SWCTL
	if (val & PLLE_AUX_SEQ_ENABLE) {
		WARN(1, "%s: Turning OFF not supported hw control\n", __func__);
		val &= ~PLLE_AUX_SEQ_ENABLE;
		clk_writel(val, PLLE_AUX);

		val = clk_readl(PLLE_AUX);
		val |= PLLE_AUX_ENABLE_SWCTL | PLLE_AUX_SS_SWCTL;
		clk_writel(val, PLLE_AUX);
	}
#endif
}

static void tegra21_plle_clk_disable(struct clk *c)
{
	u32 val;
	pr_debug("%s on clock %s\n", __func__, c->name);

	/* If enable PLLE HW sequencer, SW do not need to disable PLLE */
	val = clk_readl(PLLE_AUX);
	if (val & PLLE_AUX_SEQ_ENABLE)
		return;

	val = clk_readl(c->reg);
	val &= ~PLLE_BASE_ENABLE;
	clk_writel(val, c->reg);

	val = clk_readl(PLLE_AUX);
	val |= PLLE_AUX_ENABLE_SWCTL | PLLE_AUX_SS_SWCTL;
	clk_writel(val, PLLE_AUX);

	val = clk_readl(c->reg + c->u.pll.misc0);
	val |= PLLE_MISC_IDDQ_SW_CTRL | PLLE_MISC_IDDQ_SW_VALUE;
	pll_writel_delay(val, c->reg + c->u.pll.misc0);
}

static int tegra21_plle_clk_enable(struct clk *c)
{
	int p;
	u32 val, pdiv;
	const struct clk_pll_freq_table *sel;
	unsigned long rate = c->u.pll.fixed_rate;
	unsigned long input_rate = clk_get_rate(c->parent);

	if (c->state == ON) {
		/* BL left plle enabled - don't change configuartion */
		pr_warn("%s: pll_e is already enabled\n", __func__);
		return 0;
	}

	val = clk_readl(PLLE_AUX);
	if (val & PLLE_AUX_SEQ_ENABLE) {
		pr_info("%s: pll_e hw sequencer is already on\n", __func__);
		return 0;
	}

	/* Fixed per prod settings input */
	select_pll_e_input(c);
	input_rate = clk_get_rate(c->parent);

	/* PLLE config must be tabulated */
	for (sel = c->u.pll.freq_table; sel->input_rate != 0; sel++) {
		if (sel->input_rate == input_rate && sel->output_rate == rate)
			break;
	}

	if (sel->input_rate == 0) {
		pr_err("%s: %s input rate %lu is out-of-table\n",
		       __func__, c->name, input_rate);
		return -EINVAL;
	}

	p = c->u.pll.round_p_to_pdiv(sel->p, &pdiv);
	BUG_ON(IS_ERR_VALUE(p) || (p != sel->p));

	/* setup locking configuration, s/w control of IDDQ and enable modes,
	   take pll out of IDDQ via s/w control, setup VREG */
	val = clk_readl(c->reg);
	val &= ~PLLE_BASE_LOCK_OVERRIDE;
	clk_writel(val, c->reg);

	val = clk_readl(c->reg + c->u.pll.misc0);
	val |= PLLE_MISC_LOCK_ENABLE;
	val |= PLLE_MISC_IDDQ_SW_CTRL;
	val &= ~PLLE_MISC_IDDQ_SW_VALUE;
	val |= PLLE_MISC_PLLE_PTS;
	clk_writel(val, c->reg + c->u.pll.misc0);
	udelay(5);

	/* configure dividers, disable SS */
	val = clk_readl(PLLE_SS_CTRL);
	val |= PLLE_SS_DISABLE;
	clk_writel(val, PLLE_SS_CTRL);

	val = clk_readl(c->reg);
	val &= ~(PLLE_BASE_DIVM_MASK | PLLE_BASE_DIVN_MASK |
		 PLLE_BASE_DIVCML_MASK);
	val |= (sel->m << PLLE_BASE_DIVM_SHIFT) |
		(sel->n << PLLE_BASE_DIVN_SHIFT) |
		(pdiv << PLLE_BASE_DIVCML_SHIFT);
	pll_writel_delay(val, c->reg);
	c->mul = sel->n;
	c->div = sel->m * sel->p;

	/* enable and lock pll */
	val |= PLLE_BASE_ENABLE;
	clk_writel(val, c->reg);
	tegra_pll_clk_wait_for_lock(
		c, c->reg + c->u.pll.misc0, PLLE_MISC_LOCK);

#if USE_PLLE_SS
	val = clk_readl(PLLE_SS_CTRL);
	val &= ~(PLLE_SS_CNTL_CENTER | PLLE_SS_CNTL_INVERT);
	val &= ~PLLE_SS_COEFFICIENTS_MASK;
	val |= PLLE_SS_COEFFICIENTS_VAL;
	clk_writel(val, PLLE_SS_CTRL);
	val &= ~(PLLE_SS_CNTL_SSC_BYP | PLLE_SS_CNTL_BYPASS_SS);
	pll_writel_delay(val, PLLE_SS_CTRL);
	val &= ~PLLE_SS_CNTL_INTERP_RESET;
	pll_writel_delay(val, PLLE_SS_CTRL);
#endif
	return 0;
}

static struct clk_ops tegra_plle_ops = {
	.init			= tegra21_plle_clk_init,
	.enable			= tegra21_plle_clk_enable,
	.disable		= tegra21_plle_clk_disable,
};

static void tegra21_plle_hw_clk_init(struct clk *c)
{
	u32 val = clk_readl(PLLE_AUX);

	c->state = val & PLLE_AUX_SEQ_ENABLE ? ON : OFF;
}

static int tegra21_plle_hw_clk_enable(struct clk *c)
{
#if USE_PLLE_SWCTL
	return -EPERM;
#else
	u32 val;
	unsigned long flags;
	struct clk *p = c->parent;

	clk_lock_save(p, &flags);

	val = clk_readl(PLLE_AUX);
	if (!(val & PLLE_AUX_SEQ_ENABLE)) {
		/* switch pll under h/w control */
		val = clk_readl(p->reg + p->u.pll.misc0);
		val &= ~PLLE_MISC_IDDQ_SW_CTRL;
		clk_writel(val, p->reg + p->u.pll.misc0);

		val = clk_readl(PLLE_AUX);
		val |= PLLE_AUX_USE_LOCKDET | PLLE_AUX_SS_SEQ_INCLUDE;
		val &= ~(PLLE_AUX_ENABLE_SWCTL | PLLE_AUX_SS_SWCTL);
		pll_writel_delay(val, PLLE_AUX);
		val |= PLLE_AUX_SEQ_ENABLE;
		pll_writel_delay(val, PLLE_AUX);
	}
	clk_unlock_restore(p, &flags);

	return 0;
#endif
}

static struct clk_ops tegra_plle_hw_ops = {
	.init			= tegra21_plle_hw_clk_init,
	.enable			= tegra21_plle_hw_clk_enable,
};

/*
 * Tegra21 includes dynamic frequency lock loop (DFLL) with automatic voltage
 * control as possible CPU clock source. It is included in the Tegra12 clock
 * tree as "complex PLL" with standard Tegra clock framework APIs. However,
 * DFLL locking logic h/w access APIs are separated in the tegra_cl_dvfs.c
 * module. Hence, DFLL operations, with the exception of initialization, are
 * basically cl-dvfs wrappers.
 */

/* DFLL operations */
#ifdef	CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static void tune_cpu_trimmers(bool trim_high)
{
	tegra_soctherm_adjust_cpu_zone(trim_high);
	pr_info_once("%s: init soctherm cpu zone %s\n", __func__,
		 trim_high ? "HIGH" : "LOW");
	pr_debug("%s: adjust soctherm cpu zone %s\n", __func__,
		 trim_high ? "HIGH" : "LOW");
}
#endif

static void __init tegra21_dfll_clk_init(struct clk *c)
{
	unsigned long int dfll_boot_req_khz = tegra_dfll_boot_req_khz();
	c->ops->init = tegra21_dfll_cpu_late_init;

	/*
	 * If boot loader has set dfll clock, then dfll freq is
	 * passed in kernel command line from bootloader
	 */
	if (dfll_boot_req_khz)
		c->rate = dfll_boot_req_khz * 1000;
}

static int tegra21_dfll_clk_enable(struct clk *c)
{
	if (c->u.dfll.cl_dvfs)
		return tegra_cl_dvfs_enable(c->u.dfll.cl_dvfs);
	return 0;
}

static void tegra21_dfll_clk_disable(struct clk *c)
{
	if (c->u.dfll.cl_dvfs)
		tegra_cl_dvfs_disable(c->u.dfll.cl_dvfs);
}

static unsigned long boost_dfll_rate(struct clk *c, unsigned long rate)
{
	struct clk *consumer = c->u.dfll.consumer;
	if (!consumer || !consumer->dvfs || !consumer->dvfs->boost_table ||
	    (consumer->dvfs->num_freqs <= 1))
		return rate;

	/* Only one top DVFS step boost is allowed */
	if (rate < consumer->dvfs->freqs[consumer->dvfs->num_freqs-2])
		return rate;

	return consumer->dvfs->freqs[consumer->dvfs->num_freqs-1];
}

static int tegra21_dfll_clk_set_rate(struct clk *c, unsigned long rate)
{
	if (c->u.dfll.cl_dvfs) {
		unsigned long boost_rate = boost_dfll_rate(c, rate);
		int ret = tegra_cl_dvfs_request_rate(c->u.dfll.cl_dvfs,
						     boost_rate);
		/*
		 * Record requested rate. Thus, non boosted rate is reported.
		 * It also ensures correct clock settings when switching back
		 * and forth to a clock source with different than DFLL
		 * granularity, and prevents other than DFLL clock source to run
		 * at boost rate that is supported on DFLL only.
		 */
		if (!ret)
			c->rate = rate;

		return ret;
	}
	return 0;
}

static void tegra21_dfll_clk_reset(struct clk *c, bool assert)
{
	u32 val = assert ? DFLL_BASE_RESET : 0;
	clk_writel_delay(val, c->reg);
}

static int
tegra21_dfll_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_DFLL_LOCK) {
		if (c->u.dfll.cl_dvfs)
			return setting ? tegra_cl_dvfs_lock(c->u.dfll.cl_dvfs) :
					tegra_cl_dvfs_unlock(c->u.dfll.cl_dvfs);
		return 0;
	}
	return -EINVAL;
}

#ifdef CONFIG_PM_SLEEP
static void tegra21_dfll_clk_resume(struct clk *c)
{
	if (!(clk_readl(c->reg) & DFLL_BASE_RESET))
		return;		/* already resumed */

	if (c->u.dfll.cl_dvfs) {
		tegra_periph_reset_deassert(c);
		tegra_cl_dvfs_resume(c->u.dfll.cl_dvfs);
	}
}
#endif

static struct clk_ops tegra_dfll_ops = {
	.init			= tegra21_dfll_clk_init,
	.enable			= tegra21_dfll_clk_enable,
	.disable		= tegra21_dfll_clk_disable,
	.set_rate		= tegra21_dfll_clk_set_rate,
	.reset			= tegra21_dfll_clk_reset,
	.clk_cfg_ex		= tegra21_dfll_clk_cfg_ex,
};

/* DFLL sysfs interface */
static int tegra21_use_dfll_cb(const char *arg, const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int old_use_dfll;
	if (CONFIG_TEGRA_USE_DFLL_RANGE != TEGRA_USE_DFLL_CDEV_CNTRL) {
		old_use_dfll = use_dfll;
		param_set_int(arg, kp);
		ret =  tegra_clk_dfll_range_control(use_dfll);
		if (ret)
			use_dfll = old_use_dfll;
		return ret;
	} else {
		pr_warn("\n%s: Failed to set use_dfll\n", __func__);
		pr_warn("DFLL usage is under thermal cooling device control\n");
		return -EACCES;
	}
}

static struct kernel_param_ops tegra21_use_dfll_ops = {
	.set = tegra21_use_dfll_cb,
	.get = param_get_int,
};
module_param_cb(use_dfll, &tegra21_use_dfll_ops, &use_dfll, 0644);

/*
 * PLL internal post divider ops for PLLs that have both VCO and post-divider
 * output separtely connected to the clock tree.
 */
static struct clk_ops tegra_pll_out_ops = {
	.init			= tegra_pll_out_clk_init,
	.enable			= tegra_pll_out_clk_enable,
	.disable		= tegra_pll_out_clk_disable,
	.set_rate		= tegra_pll_out_clk_set_rate,
};

static struct clk_ops tegra_pll_out_fixed_ops = {
	.init			= tegra_pll_out_clk_init,
	.enable			= tegra_pll_out_clk_enable,
	.disable		= tegra_pll_out_clk_disable,
};

static void tegra21_pllu_out_clk_init(struct clk *c)
{
	u32 p, val;
	struct clk *pll = c->parent;
	val = clk_readl(pll->reg);

	/*
	 * If PLLU state is already under h/w control just record output ratio,
	 * and verify expected fixed output rate.
	 */
	if (!(val & PLLU_BASE_OVERRIDE)) {
		struct clk_pll_div_layout *divs = pll->u.pll.div_layout;

		c->state = c->parent->state;
		c->max_rate = pll->u.pll.vco_max;

		p = (val & divs->pdiv_mask) >> divs->pdiv_shift;
		if (p > divs->pdiv_max)
			p = divs->pdiv_max; /* defer invalid p WARN to verify */
		p = divs->pdiv_to_p[p];
		c->div = p;
		c->mul = 1;

		pll_clk_verify_fixed_rate(c);
	} else {
		/* Complete PLLU output s/w controlled initialization */
		tegra_pll_out_clk_init(c);
	}
}

static struct clk_ops tegra_pllu_out_ops = {
	.init			= tegra21_pllu_out_clk_init,
	.enable			= tegra_pll_out_clk_enable,
	.disable		= tegra_pll_out_clk_disable,
	.set_rate		= tegra_pll_out_clk_set_rate,
};

static void tegra21_pllp_out_clk_init(struct clk *c)
{
	u32 p, val;
	struct clk *pll = c->parent;
	val = clk_readl(pll->reg);

	/*
	 * If PLLP state is already under h/w control just record expected
	 * fixed output rate.
	 */
	if (!(val & PLLP_BASE_OVERRIDE)) {
		c->state = c->parent->state;
		c->max_rate = pll->u.pll.vco_max;
		c->div = pll->u.pll.fixed_rate / c->u.pll.fixed_rate;
		c->mul = 1;
		pll_clk_verify_fixed_rate(c);
	} else {
		/* Complete pllp output s/w controlled initialization */
		tegra_pll_out_clk_init(c);
	}

	/* Reset not used PLL_P_OUT1 branch. */
	clk_writel(0x0, pll->reg + 0x4);
}

static struct clk_ops tegra_pllp_out_ops = {
	.init			= tegra21_pllp_out_clk_init,
	.enable			= tegra_pll_out_clk_enable,
	.disable		= tegra_pll_out_clk_disable,
	.set_rate		= tegra_pll_out_clk_set_rate,
};

#ifdef CONFIG_PM_SLEEP
static void tegra_pllu_out_resume_enable(struct clk *c)
{
	u32 val = clk_readl(c->parent->reg);
	if (!(val & PLLU_BASE_OVERRIDE) ||
	    val & c->parent->u.pll.controls->enable_mask)
		return;		/* already resumed */

	tegra_pll_out_resume_enable(c);
}

static void tegra_pllp_out_resume_enable(struct clk *c)
{
	u32 val = clk_readl(c->parent->reg);

	/* Reset not used PLL_P_OUT1 branch. */
	clk_writel(0x0, c->parent->reg + 0x4);

	if (!(val & PLLP_BASE_OVERRIDE) ||
	    val & c->parent->u.pll.controls->enable_mask)
		return;		/* already resumed */

	tegra_pll_out_resume_enable(c);
}

#endif

/* PLL external secondary divider ops (non-atomic shared register access) */
static DEFINE_SPINLOCK(pll_div_lock);

static int tegra21_pll_div_clk_set_rate(struct clk *c, unsigned long rate);
static void tegra21_pll_div_clk_init(struct clk *c)
{
	if (c->flags & DIV_U71) {
		u32 val, divu71;
		if ((c->parent->state == OFF) || (!c->parent->ops->disable &&
		     (c->parent->parent->state == OFF)))
			c->ops->disable(c);

		val = clk_readl(c->reg);
		val >>= c->reg_shift;
		c->state = (val & PLL_OUT_CLKEN) ? ON : OFF;
		if (!(val & PLL_OUT_RESET_DISABLE))
			c->state = OFF;

		if (c->u.pll_div.default_rate) {
			int ret = tegra21_pll_div_clk_set_rate(
					c, c->u.pll_div.default_rate);
			if (!ret)
				return;
		}
		divu71 = (val & PLL_OUT_RATIO_MASK) >> PLL_OUT_RATIO_SHIFT;
		c->div = (divu71 + 2);
		c->mul = 2;
	} else if (c->flags & DIV_2) {
		c->state = ON;
		if (c->flags & (PLLD | PLLX)) {
			c->div = 2;
			c->mul = 1;
		}
		else
			BUG();
	} else {
		u32 val = clk_readl(c->reg);
		c->state = val & (0x1 << c->reg_shift) ? ON : OFF;
		c->max_rate = c->parent->max_rate;
	}
}

static int tegra21_pll_div_clk_enable(struct clk *c)
{
	u32 val;
	u32 new_val;
	unsigned long flags;

	pr_debug("%s: %s\n", __func__, c->name);
	if (c->flags & DIV_U71) {
		spin_lock_irqsave(&pll_div_lock, flags);
		val = clk_readl(c->reg);
		new_val = val >> c->reg_shift;
		new_val &= 0xFFFF;

		new_val |= PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;

		val &= ~(0xFFFF << c->reg_shift);
		val |= new_val << c->reg_shift;
		clk_writel_delay(val, c->reg);
		spin_unlock_irqrestore(&pll_div_lock, flags);
		return 0;
	} else if (c->flags & DIV_2) {
		return 0;
	} else if (c->flags & PLLU) {
		clk_lock_save(pll_u, &flags);
		val = clk_readl(c->reg) | (0x1 << c->reg_shift);
		clk_writel_delay(val, c->reg);
		clk_unlock_restore(pll_u, &flags);
		return 0;
	} else {
		spin_lock_irqsave(&pll_div_lock, flags);
		val = clk_readl(c->reg) | (0x1 << c->reg_shift);
		clk_writel_delay(val, c->reg);
		spin_unlock_irqrestore(&pll_div_lock, flags);
		return 0;
	}
	return -EINVAL;
}

static void tegra21_pll_div_clk_disable(struct clk *c)
{
	u32 val;
	u32 new_val;
	unsigned long flags;

	pr_debug("%s: %s\n", __func__, c->name);
	if (c->flags & DIV_U71) {
		spin_lock_irqsave(&pll_div_lock, flags);
		val = clk_readl(c->reg);
		new_val = val >> c->reg_shift;
		new_val &= 0xFFFF;

		new_val &= ~(PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE);

		val &= ~(0xFFFF << c->reg_shift);
		val |= new_val << c->reg_shift;
		clk_writel_delay(val, c->reg);
		spin_unlock_irqrestore(&pll_div_lock, flags);
	} else if (c->flags & PLLU) {
		clk_lock_save(pll_u, &flags);
		val = clk_readl(c->reg) & (~(0x1 << c->reg_shift));
		clk_writel_delay(val, c->reg);
		clk_unlock_restore(pll_u, &flags);
	} else {
		spin_lock_irqsave(&pll_div_lock, flags);
		val = clk_readl(c->reg) & (~(0x1 << c->reg_shift));
		clk_writel_delay(val, c->reg);
		spin_unlock_irqrestore(&pll_div_lock, flags);
	}
}

static int tegra21_pll_div_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val;
	u32 new_val;
	int divider_u71;
	unsigned long parent_rate = clk_get_rate(c->parent);
	unsigned long flags;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);
	if (c->flags & DIV_U71) {
		divider_u71 = clk_div71_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider_u71 >= 0) {
			spin_lock_irqsave(&pll_div_lock, flags);
			val = clk_readl(c->reg);
			new_val = val >> c->reg_shift;
			new_val &= 0xFFFF;
			if (c->flags & DIV_U71_FIXED)
				new_val |= PLL_OUT_OVERRIDE;
			new_val &= ~PLL_OUT_RATIO_MASK;
			new_val |= divider_u71 << PLL_OUT_RATIO_SHIFT;

			val &= ~(0xFFFF << c->reg_shift);
			val |= new_val << c->reg_shift;
			clk_writel_delay(val, c->reg);
			c->div = divider_u71 + 2;
			c->mul = 2;
			spin_unlock_irqrestore(&pll_div_lock, flags);
			return 0;
		}
	} else if (c->flags & DIV_2)
		return clk_set_rate(c->parent, rate * 2);

	return -EINVAL;
}

static long tegra21_pll_div_clk_round_rate(struct clk *c, unsigned long rate)
{
	int divider;
	unsigned long parent_rate = clk_get_rate(c->parent);
	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->flags & DIV_U71) {
		divider = clk_div71_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider < 0)
			return divider;
		return DIV_ROUND_UP(parent_rate * 2, divider + 2);
	} else if (c->flags & DIV_2)
		/* no rounding - fixed DIV_2 dividers pass rate to parent PLL */
		return rate;

	return -EINVAL;
}

static struct clk_ops tegra_pll_div_ops = {
	.init			= tegra21_pll_div_clk_init,
	.enable			= tegra21_pll_div_clk_enable,
	.disable		= tegra21_pll_div_clk_disable,
	.set_rate		= tegra21_pll_div_clk_set_rate,
	.round_rate		= tegra21_pll_div_clk_round_rate,
};

/* Periph clk ops */
static inline u32 periph_clk_source_mask(struct clk *c)
{
	if (c->u.periph.src_mask)
		return c->u.periph.src_mask;
	else if (c->flags & MUX_PWM)
		return 3 << 28;
	else if (c->flags & MUX_CLK_OUT)
		return 3 << (c->u.periph.clk_num + 4);
	else if (c->flags & PLLD)
		return PLLD_BASE_DSI_MUX_MASK;
	else
		return 7 << 29;
}

static inline u32 periph_clk_source_shift(struct clk *c)
{
	if (c->u.periph.src_shift)
		return c->u.periph.src_shift;
	else if (c->flags & MUX_PWM)
		return 28;
	else if (c->flags & MUX_CLK_OUT)
		return c->u.periph.clk_num + 4;
	else if (c->flags & PLLD)
		return PLLD_BASE_DSI_MUX_SHIFT;
	else
		return 29;
}

static void periph_clk_state_init(struct clk *c)
{
	if (c->flags & PERIPH_NO_ENB) {
		c->state = c->parent->state;
		return;
	}

	c->state = ON;

	if (!(clk_readl(PERIPH_CLK_TO_ENB_REG(c)) & PERIPH_CLK_TO_BIT(c)))
		c->state = OFF;
	if (!(c->flags & PERIPH_NO_RESET))
		if (clk_readl(PERIPH_CLK_TO_RST_REG(c)) & PERIPH_CLK_TO_BIT(c))
			c->state = OFF;
}

static bool periph_clk_can_force_safe_rate(struct clk *c)
{
	/*
	 * As a general rule, force safe rate always for clocks supplied
	 * to peripherals under reset. If clock does not have associated reset
	 * at all, force safe rate if clock is disabled (can do it on Tegra21
	 * that allow to change dividers of the disabled clocks).
	 */
	if (c->flags & PERIPH_NO_RESET)
			return c->state == OFF;

	return clk_readl(PERIPH_CLK_TO_RST_REG(c)) & PERIPH_CLK_TO_BIT(c);
}

static void tegra21_periph_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	const struct clk_mux_sel *mux = NULL;
	const struct clk_mux_sel *sel;
	if (c->flags & MUX) {
		for (sel = c->inputs; sel->input != NULL; sel++) {
			if (((val & periph_clk_source_mask(c)) >>
			    periph_clk_source_shift(c)) == sel->value)
				mux = sel;
		}
		BUG_ON(!mux);

		c->parent = mux->input;
	} else {
		c->parent = c->inputs[0].input;
	}

	if (c->flags & DIV_U71) {
		u32 divu71 = val & PERIPH_CLK_SOURCE_DIVU71_MASK;
		if (c->flags & DIV_U71_IDLE) {
			val &= ~(PERIPH_CLK_SOURCE_DIVU71_MASK <<
				PERIPH_CLK_SOURCE_DIVIDLE_SHIFT);
			val |= (PERIPH_CLK_SOURCE_DIVIDLE_VAL <<
				PERIPH_CLK_SOURCE_DIVIDLE_SHIFT);
			clk_writel(val, c->reg);
		}
		c->div = divu71 + 2;
		c->mul = 2;
	} else if (c->flags & DIV_U151) {
		u32 divu151 = val & PERIPH_CLK_SOURCE_DIVU16_MASK;
		if ((c->flags & DIV_U151_UART) &&
		    (!(val & PERIPH_CLK_UART_DIV_ENB))) {
			divu151 = 0;
		}
		c->div = divu151 + 2;
		c->mul = 2;
	} else if (c->flags & DIV_U16) {
		u32 divu16 = val & PERIPH_CLK_SOURCE_DIVU16_MASK;
		c->div = divu16 + 1;
		c->mul = 1;
	} else if (!c->div || !c->mul) {
		c->div = 1;
		c->mul = 1;
	}

	periph_clk_state_init(c);

	/* if peripheral is left under reset - enforce safe rate */
	if (periph_clk_can_force_safe_rate(c))
		tegra_periph_clk_safe_rate_init(c);
}

static int tegra21_periph_clk_enable(struct clk *c)
{
	unsigned long flags;
	pr_debug("%s on clock %s\n", __func__, c->name);

	if (c->flags & PERIPH_NO_ENB)
		return 0;

	spin_lock_irqsave(&periph_refcount_lock, flags);

	tegra_periph_clk_enable_refcount[c->u.periph.clk_num]++;
	if (tegra_periph_clk_enable_refcount[c->u.periph.clk_num] > 1) {
		spin_unlock_irqrestore(&periph_refcount_lock, flags);
		return 0;
	}

	clk_writel_delay(PERIPH_CLK_TO_BIT(c), PERIPH_CLK_TO_ENB_SET_REG(c));

	if (!(c->flags & PERIPH_NO_RESET) && !(c->flags & PERIPH_MANUAL_RESET)) {
		if (clk_readl(PERIPH_CLK_TO_RST_REG(c)) & PERIPH_CLK_TO_BIT(c)) {
			udelay(RESET_PROPAGATION_DELAY);
			clk_writel_delay(PERIPH_CLK_TO_BIT(c),
					 PERIPH_CLK_TO_RST_CLR_REG(c));
		}
	}
	spin_unlock_irqrestore(&periph_refcount_lock, flags);
	return 0;
}

static void tegra21_periph_clk_disable(struct clk *c)
{
	unsigned long val, flags;
	pr_debug("%s on clock %s\n", __func__, c->name);

	if (c->flags & PERIPH_NO_ENB)
		return;

	spin_lock_irqsave(&periph_refcount_lock, flags);

	if (c->refcnt)
		tegra_periph_clk_enable_refcount[c->u.periph.clk_num]--;

	if (tegra_periph_clk_enable_refcount[c->u.periph.clk_num] == 0) {
		/* If peripheral is in the APB bus then read the APB bus to
		 * flush the write operation in apb bus. This will avoid the
		 * peripheral access after disabling clock*/
		if (c->flags & PERIPH_ON_APB)
			val = chipid_readl();

		clk_writel_delay(PERIPH_CLK_TO_BIT(c),
				 PERIPH_CLK_TO_ENB_CLR_REG(c));
	}
	spin_unlock_irqrestore(&periph_refcount_lock, flags);
}

static void tegra21_periph_clk_reset(struct clk *c, bool assert)
{
	unsigned long val;
	pr_debug("%s %s on clock %s\n", __func__,
		 assert ? "assert" : "deassert", c->name);

	if (!(c->flags & PERIPH_NO_RESET)) {
		if (assert) {
			/* If peripheral is in the APB bus then read the APB
			 * bus to flush the write operation in apb bus. This
			 * will avoid the peripheral access after disabling
			 * clock */
			if (c->flags & PERIPH_ON_APB)
				val = chipid_readl();

			clk_writel_delay(PERIPH_CLK_TO_BIT(c),
					 PERIPH_CLK_TO_RST_SET_REG(c));
		} else
			clk_writel_delay(PERIPH_CLK_TO_BIT(c),
					 PERIPH_CLK_TO_RST_CLR_REG(c));
	}
}

static int tegra21_periph_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;
	pr_debug("%s: %s %s\n", __func__, c->name, p->name);

	if (!(c->flags & MUX))
		return (p == c->parent) ? 0 : (-EINVAL);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			val = clk_readl(c->reg);
			val &= ~periph_clk_source_mask(c);
			val |= (sel->value << periph_clk_source_shift(c));

			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			clk_writel_delay(val, c->reg);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static int tegra21_periph_clk_set_rate(struct clk *c, unsigned long rate)
{
	u32 val;
	int divider;
	unsigned long parent_rate = clk_get_rate(c->parent);

	if (c->flags & DIV_U71) {
		divider = clk_div71_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider >= 0) {
			val = clk_readl(c->reg);
			val &= ~PERIPH_CLK_SOURCE_DIVU71_MASK;
			val |= divider;
			clk_writel_delay(val, c->reg);
			c->div = divider + 2;
			c->mul = 2;
			return 0;
		}
	} else if (c->flags & DIV_U151) {
		divider = clk_div151_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider >= 0) {
			val = clk_readl(c->reg);
			val &= ~PERIPH_CLK_SOURCE_DIVU16_MASK;
			val |= divider;
			if (c->flags & DIV_U151_UART) {
				if (divider)
					val |= PERIPH_CLK_UART_DIV_ENB;
				else
					val &= ~PERIPH_CLK_UART_DIV_ENB;
			}
			clk_writel_delay(val, c->reg);
			c->div = divider + 2;
			c->mul = 2;
			return 0;
		}
	} else if (c->flags & DIV_U16) {
		divider = clk_div16_get_divider(parent_rate, rate);
		if (divider >= 0) {
			val = clk_readl(c->reg);
			val &= ~PERIPH_CLK_SOURCE_DIVU16_MASK;
			val |= divider;
			clk_writel_delay(val, c->reg);
			c->div = divider + 1;
			c->mul = 1;
			return 0;
		}
	} else if (parent_rate <= rate) {
		c->div = 1;
		c->mul = 1;
		return 0;
	}
	return -EINVAL;
}

static long tegra21_periph_clk_round_rate(struct clk *c,
	unsigned long rate)
{
	int divider;
	unsigned long parent_rate = clk_get_rate(c->parent);
	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	if (c->flags & DIV_U71) {
		divider = clk_div71_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider < 0)
			return divider;

		return DIV_ROUND_UP(parent_rate * 2, divider + 2);
	} else if (c->flags & DIV_U151) {
		divider = clk_div151_get_divider(
			parent_rate, rate, c->flags, ROUND_DIVIDER_UP);
		if (divider < 0)
			return divider;

		return DIV_ROUND_UP(parent_rate * 2, divider + 2);
	} else if (c->flags & DIV_U16) {
		divider = clk_div16_get_divider(parent_rate, rate);
		if (divider < 0)
			return divider;
		return DIV_ROUND_UP(parent_rate, divider + 1);
	}
	return -EINVAL;
}

static struct clk_ops tegra_periph_clk_ops = {
	.init			= &tegra21_periph_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_periph_clk_set_parent,
	.set_rate		= &tegra21_periph_clk_set_rate,
	.round_rate		= &tegra21_periph_clk_round_rate,
	.reset			= &tegra21_periph_clk_reset,
};

static int tegra21_qspi_clk_cfg_ex(struct clk *c,
		enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_QSPI_DIV2_ENB) {
		u32 div2_val = 0;
		u32 val = clk_readl(c->reg);

		div2_val = val & PERIPH_CLK_SOURCE_QSPI_DIV2_EN;

		if (setting && !div2_val) {
			val |= PERIPH_CLK_SOURCE_QSPI_DIV2_EN;
			clk_writel_delay(val, c->reg);
		} else if (!setting && div2_val) {
			val &= ~PERIPH_CLK_SOURCE_QSPI_DIV2_EN;
			clk_writel_delay(val, c->reg);
		}
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_qspi_clk_ops = {
	.init			= &tegra21_periph_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_periph_clk_set_parent,
	.set_rate		= &tegra21_periph_clk_set_rate,
	.round_rate		= &tegra21_periph_clk_round_rate,
	.reset			= &tegra21_periph_clk_reset,
	.clk_cfg_ex		= &tegra21_qspi_clk_cfg_ex,
};

/* Supper skipper ops */
static void tegra21_clk_super_skip_init(struct clk *c)
{
	if (!c->parent)
		c->parent = c - 1;
	c->parent->skipper = c;

	/* Skipper is always ON (does not gate the clock) */
	c->state = ON;
	c->max_rate = c->parent->max_rate;

	/* Always set initial 1:1 ratio */
	clk_writel(0, c->reg);
	c->div = 1;
	c->mul = 1;
}

static int tegra21_clk_super_skip_enable(struct clk *c)
{
	/* no clock gate in skipper, just pass thru to parent */
	return 0;
}

#ifdef CONFIG_TEGRA_BPMP_SCLK_SKIP
static void tegra_bpmp_sclk_skip_set_rate(unsigned long input_rate,
		unsigned long rate)
{
	uint32_t mb[] = { cpu_to_le32(input_rate), cpu_to_le32(rate) };
	int r = tegra_bpmp_send_receive_atomic(MRQ_SCLK_SKIP_SET_RATE,
			&mb, sizeof(mb), NULL, 0);
	WARN_ON(r);
}
#endif

static int tegra21_clk_super_skip_set_rate(struct clk *c, unsigned long rate)
{
	u32 val, mul, div;
	u64 output_rate = rate;
	unsigned long input_rate, flags;

	pr_debug("%s: %s %lu\n", __func__, c->name, rate);

	/*
	 * Locking parent clock prevents parent rate change while super skipper
	 * is updated. It also takes care of super skippers that share h/w
	 * register with parent clock divider. Skipper output rate can be
	 * rounded up, since volatge is set based on source clock rate.
	 */
	clk_lock_save(c->parent, &flags);
	input_rate = clk_get_rate_locked(c->parent);

	div = 1 << SUPER_SKIPPER_TERM_SIZE;
	output_rate <<= SUPER_SKIPPER_TERM_SIZE;
	output_rate += input_rate - 1;
	do_div(output_rate, input_rate);
	mul = output_rate ? : 1;

	if (mul < div) {
		val = SUPER_SKIPPER_ENABLE |
			((mul - 1) << SUPER_SKIPPER_MUL_SHIFT) |
			((div - 1) << SUPER_SKIPPER_DIV_SHIFT);
		c->div = div;
		c->mul = mul;
	} else {
		val = 0;
		c->div = 1;
		c->mul = 1;
	}

#ifdef CONFIG_TEGRA_BPMP_SCLK_SKIP
	/* For SCLK do not touch the register directly - send IPC to BPMP */
	if (c->flags & DIV_BUS)
		tegra_bpmp_sclk_skip_set_rate(input_rate, rate);
	else
#endif
		clk_writel(val, c->reg);

	clk_unlock_restore(c->parent, &flags);
	return 0;
}

static struct clk_ops tegra_clk_super_skip_ops = {
	.init			= &tegra21_clk_super_skip_init,
	.enable			= &tegra21_clk_super_skip_enable,
	.set_rate		= &tegra21_clk_super_skip_set_rate,
};

/* 1x shared bus ops */
static long _1x_round_updown(struct clk *c, struct clk *src,
				unsigned long rate, bool up)
{
	return fixed_src_bus_round_updown(c, src, c->flags, rate, up, NULL);
}

static long tegra21_1xbus_round_updown(struct clk *c, unsigned long rate,
					    bool up)
{
	unsigned long pll_low_rate, pll_high_rate;

	rate = max(rate, c->min_rate);

	pll_low_rate = _1x_round_updown(c, c->u.periph.pll_low, rate, up);
	if (rate <= c->u.periph.threshold) {
		c->u.periph.pll_selected = c->u.periph.pll_low;
		return pll_low_rate;
	}

	pll_high_rate = _1x_round_updown(c, c->u.periph.pll_high, rate, up);
	if (pll_high_rate <= c->u.periph.threshold) {
		c->u.periph.pll_selected = c->u.periph.pll_low;
		return pll_low_rate;  /* prevent oscillation across threshold */
	}

	if (up) {
		/* rounding up: both plls may hit max, and round down */
		if (pll_high_rate < rate) {
			if (pll_low_rate < pll_high_rate) {
				c->u.periph.pll_selected = c->u.periph.pll_high;
				return pll_high_rate;
			}
		} else {
			if ((pll_low_rate < rate) ||
			    (pll_low_rate > pll_high_rate)) {
				c->u.periph.pll_selected = c->u.periph.pll_high;
				return pll_high_rate;
			}
		}
	} else if (pll_low_rate < pll_high_rate) {
		/* rounding down: to get here both plls able to round down */
		c->u.periph.pll_selected = c->u.periph.pll_high;
		return pll_high_rate;
	}
	c->u.periph.pll_selected = c->u.periph.pll_low;
	return pll_low_rate;
}

static long tegra21_1xbus_round_rate(struct clk *c, unsigned long rate)
{
	return tegra21_1xbus_round_updown(c, rate, true);
}

static int tegra21_1xbus_set_rate(struct clk *c, unsigned long rate)
{
	/* Compensate rate truncating during rounding */
	return tegra21_periph_clk_set_rate(c, rate + 1);
}

static int tegra21_clk_1xbus_update(struct clk *c)
{
	int ret;
	struct clk *new_parent;
	unsigned long rate, old_rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra21_clk_shared_bus_update(c, NULL, NULL, NULL);

	old_rate = clk_get_rate_locked(c);
	pr_debug("\n1xbus %s: rate %lu on parent %s: new request %lu\n",
		 c->name, old_rate, c->parent->name, rate);
	if (rate == old_rate)
		return 0;

	if (!c->u.periph.min_div_low || !c->u.periph.min_div_high) {
		unsigned long r, m = c->max_rate;
		r = clk_get_rate(c->u.periph.pll_low);
		c->u.periph.min_div_low = DIV_ROUND_UP(r, m) * c->mul;
		r = clk_get_rate(c->u.periph.pll_high);
		c->u.periph.min_div_high = DIV_ROUND_UP(r, m) * c->mul;
	}

	new_parent = c->u.periph.pll_selected;

	/*
	 * The transition procedure below is guaranteed to switch to the target
	 * parent/rate without violation of max clock limits. It would attempt
	 * to switch without dip in bus rate if it is possible, but this cannot
	 * be guaranteed (example: switch from 408 MHz : 1 to 624 MHz : 2 with
	 * maximum bus limit 408 MHz will be executed as 408 => 204 => 312 MHz,
	 * and there is no way to avoid rate dip in this case).
	 */
	if (new_parent != c->parent) {
		int interim_div = 0;
		/* Switching to pll_high may over-clock bus if current divider
		   is too small - increase divider to safe value */
		if ((new_parent == c->u.periph.pll_high) &&
		    (c->div < c->u.periph.min_div_high))
			interim_div = c->u.periph.min_div_high;

		/* Switching to pll_low may dip down rate if current divider
		   is too big - decrease divider as much as we can */
		if ((new_parent == c->u.periph.pll_low) &&
		    (c->div > c->u.periph.min_div_low) &&
		    (c->div > c->u.periph.min_div_high))
			interim_div = c->u.periph.min_div_low;

		if (interim_div) {
			u64 interim_rate = old_rate * c->div;
			do_div(interim_rate, interim_div);
			ret = clk_set_rate_locked(c, interim_rate);
			if (ret) {
				pr_err("Failed to set %s rate to %lu\n",
				       c->name, (unsigned long)interim_rate);
				return ret;
			}
			pr_debug("1xbus %s: rate %lu on parent %s\n", c->name,
				 clk_get_rate_locked(c), c->parent->name);
		}

		ret = clk_set_parent_locked(c, new_parent);
		if (ret) {
			pr_err("Failed to set %s parent %s\n",
			       c->name, new_parent->name);
			return ret;
		}

		old_rate = clk_get_rate_locked(c);
		pr_debug("1xbus %s: rate %lu on parent %s\n", c->name,
			 old_rate, c->parent->name);
		if (rate == old_rate)
			return 0;
	}

	ret = clk_set_rate_locked(c, rate);
	if (ret) {
		pr_err("Failed to set %s rate to %lu\n", c->name, rate);
		return ret;
	}
	pr_debug("1xbus %s: rate %lu on parent %s\n", c->name,
		 clk_get_rate_locked(c), c->parent->name);
	return 0;

}

static struct clk_ops tegra_1xbus_clk_ops = {
	.init			= &tegra21_periph_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_periph_clk_set_parent,
	.set_rate		= &tegra21_1xbus_set_rate,
	.round_rate		= &tegra21_1xbus_round_rate,
	.round_rate_updown	= &tegra21_1xbus_round_updown,
	.reset			= &tegra21_periph_clk_reset,
	.shared_bus_update	= &tegra21_clk_1xbus_update,
};

/* Periph VI extended clock configuration ops */
static int
tegra21_vi_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_VI_INP_SEL) {
		u32 val = clk_readl(c->reg);
		val &= ~PERIPH_CLK_VI_SEL_EX_MASK;
		val |= (setting << PERIPH_CLK_VI_SEL_EX_SHIFT) &
			PERIPH_CLK_VI_SEL_EX_MASK;
		clk_writel(val, c->reg);
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_vi_clk_ops = {
	.init			= &tegra21_periph_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_periph_clk_set_parent,
	.set_rate		= &tegra21_periph_clk_set_rate,
	.round_rate		= &tegra21_periph_clk_round_rate,
	.clk_cfg_ex		= &tegra21_vi_clk_cfg_ex,
	.reset			= &tegra21_periph_clk_reset,
};

/* SOR clocks operations */
static void tegra21_sor_brick_init(struct clk *c)
{
	/*
	 * Brick configuration is under DC driver control - wiil be in sync
	 * with h/w after DC SOR init.
	 */
	c->parent = c->inputs[0].input;
	c->state = c->parent->state;
	c->div = 10;
	c->mul = 10;
	return;
}

static int tegra21_sor_brick_set_rate(struct clk *c, unsigned long rate)
{
	/*
	 * Brick rate is under DC driver control, this interface is used for
	 * information  purposes, as well as for DVFS update.
	 */
	unsigned long parent_rate = clk_get_rate(c->parent);
	u64 mul = 10;
	c->div = 10;	/* brick link divisor */

	mul = mul * rate + parent_rate / 2;
	do_div(mul, parent_rate);
	c->mul = mul;

	return 0;
}

static int tegra21_sor_brick_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;

	if (!(c->flags & MUX))
		return (p == c->parent) ? 0 : (-EINVAL);
	/*
	 * Brick parent selection is under DC driver control, this interface
	 * is used to propagate enable/disable relations, for information
	 * purposes, as well as for DVFS update.
	 */
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			if (c->refcnt) {
				tegra_clk_prepare_enable(p);
				tegra_clk_disable_unprepare(c->parent);
			}
			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

static struct clk_ops tegra_sor_brick_ops = {
	.init			= &tegra21_sor_brick_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_sor_brick_set_parent,
	.set_rate		= &tegra21_sor_brick_set_rate,
};

static void tegra21_sor0_clk_init(struct clk *c)
{
	c->u.periph.src_mask = PERIPH_CLK_SOR0_CLK_SEL_MASK;
	c->u.periph.src_shift = PERIPH_CLK_SOR_CLK_SEL_SHIFT;
	tegra21_periph_clk_init(c);
}

static int
tegra21_sor0_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_SOR_CLK_SEL) {
		int i = setting ? 1 : 0;
		return clk_set_parent_locked(c, c->inputs[i].input);
	}
	return -EINVAL;
}

static struct clk_ops tegra_sor0_clk_ops = {
	.init			= &tegra21_sor0_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_periph_clk_set_parent,
	.clk_cfg_ex		= &tegra21_sor0_clk_cfg_ex,
	.reset			= &tegra21_periph_clk_reset,
};

static void tegra21_sor1_clk_init(struct clk *c)
{
	c->u.periph.src_mask = PERIPH_CLK_SOR1_CLK_SEL_MASK;
	c->u.periph.src_shift = PERIPH_CLK_SOR_CLK_SEL_SHIFT;
	tegra21_periph_clk_init(c);
}

static int tegra21_sor1_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	unsigned long flags;
	const struct clk_mux_sel *sel;
	struct clk *src = c->inputs[2].input;	/* sor1_src entry in sor1 mux */

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			if (c->refcnt)
				tegra_clk_prepare_enable(p);
			/*
			 * Since sor1 output mux control setting share the same
			 * register with source mux/divider switch, use switch
			 * lock to protect r-m-w of output mux controls
			 */
			clk_lock_save(src, &flags);
			val = clk_readl(c->reg);
			val &= ~periph_clk_source_mask(c);
			val |= (sel->value << periph_clk_source_shift(c));
			clk_writel_delay(val, c->reg);
			clk_unlock_restore(src, &flags);

			if (c->refcnt)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

static int
tegra21_sor1_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{

	if ((p != TEGRA_CLK_SOR_CLK_SEL) || (setting > 3))
		return -EINVAL;
	/*
	 * If LSb is set, sor1_brick is selected regardless of MSb. As such
	 * setting 3 and 1 result in the same clock source. The set parent api
	 * always selects the 1st match for requested parent, so effectively
	 * setting 3 is reduced to 1.
	 */
	return clk_set_parent_locked(c, c->inputs[setting].input);
}

static struct clk_ops tegra_sor1_clk_ops = {
	.init			= &tegra21_sor1_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_sor1_clk_set_parent,
	.clk_cfg_ex		= &tegra21_sor1_clk_cfg_ex,
	.reset			= &tegra21_periph_clk_reset,
};

/* Periph DTV extended clock configuration ops */
static int
tegra21_dtv_clk_cfg_ex(struct clk *c, enum tegra_clk_ex_param p, u32 setting)
{
	if (p == TEGRA_CLK_DTV_INVERT) {
		u32 val = clk_readl(c->reg);
		if (setting)
			val |= PERIPH_CLK_DTV_POLARITY_INV;
		else
			val &= ~PERIPH_CLK_DTV_POLARITY_INV;
		clk_writel(val, c->reg);
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_dtv_clk_ops = {
	.init			= &tegra21_periph_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_periph_clk_set_parent,
	.set_rate		= &tegra21_periph_clk_set_rate,
	.round_rate		= &tegra21_periph_clk_round_rate,
	.clk_cfg_ex		= &tegra21_dtv_clk_cfg_ex,
	.reset			= &tegra21_periph_clk_reset,
};

static void tegra21_dpaux_clk_init(struct clk *c)
{
	c->mul = 1;
	c->div = 17;;
	tegra21_periph_clk_init(c);
}

static struct clk_ops tegra_dpaux_clk_ops = {
	.init			= &tegra21_dpaux_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_periph_clk_set_parent,
	.set_rate		= &tegra21_periph_clk_set_rate,
	.reset			= &tegra21_periph_clk_reset,
};

/* XUSB SS clock ops */
static DEFINE_SPINLOCK(xusb_ss_lock);

static int tegra21_xusb_ss_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&xusb_ss_lock, flags);
	ret = tegra21_periph_clk_set_rate(c, rate);
	spin_unlock_irqrestore(&xusb_ss_lock, flags);
	return ret;
}

static int tegra21_xusb_ss_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	unsigned long flags;
	const struct clk_mux_sel *sel;
	pr_debug("%s: %s %s\n", __func__, c->name, p->name);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			spin_lock_irqsave(&xusb_ss_lock, flags);
			val = clk_readl(c->reg);
			val &= ~periph_clk_source_mask(c);
			val |= (sel->value << periph_clk_source_shift(c));
			clk_writel_delay(val, c->reg);
			spin_unlock_irqrestore(&xusb_ss_lock, flags);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static struct clk_ops tegra_xusb_ss_ops = {
	.init			= &tegra21_periph_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_xusb_ss_set_parent,
	.set_rate		= &tegra21_xusb_ss_set_rate,
	.round_rate		= &tegra21_periph_clk_round_rate,
	.reset			= &tegra21_periph_clk_reset,
};

/* pciex clock support only reset function */
static void tegra21_pciex_clk_init(struct clk *c)
{
	c->state = c->parent->state;
}

static int tegra21_pciex_clk_enable(struct clk *c)
{
	return 0;
}

static void tegra21_pciex_clk_disable(struct clk *c)
{
}

static int tegra21_pciex_clk_set_rate(struct clk *c, unsigned long rate)
{
	unsigned long parent_rate = clk_get_rate(c->parent);

	/*
	 * the only supported pcie configurations:
	 * Gen1: plle = 100MHz, link at 250MHz
	 * Gen2: plle = 100MHz, link at 500MHz
	 */
	if (parent_rate == 100000000) {
		if (rate == 500000000) {
			c->mul = 5;
			c->div = 1;
			return 0;
		} else if (rate == 250000000) {
			c->mul = 5;
			c->div = 2;
			return 0;
		}
	}
	return -EINVAL;
}

static struct clk_ops tegra_pciex_clk_ops = {
	.init     = tegra21_pciex_clk_init,
	.enable	  = tegra21_pciex_clk_enable,
	.disable  = tegra21_pciex_clk_disable,
	.set_rate = tegra21_pciex_clk_set_rate,
	.reset    = tegra21_periph_clk_reset,
};


/* SDMMC2 and SDMMC4 ops */
static u32 sdmmc24_input_to_lj[] = {
	/* 0, 1, 2, 3, 4, 5, 6, 7 */
	   0, 1, 2, 1, 5, 5, 6, 2
};

static u32 sdmmc24_input_from_lj[] = {
	/* 0, 1, 2, 3, 4, 5, 6, 7 */
	   0, 3, 7, 3, 4, 4, 6, 7
};

/*
 * The same PLLC4 output branches connected to the low jitter (LJ) and divided
 * inputs of the SDMMC2/4 source selection mux. The difference is that clock
 * from LJ input does not go through SDMMC2/4 divider. Although it is possible
 * to run output clock at the same rate as source from divided input (with
 * divider 1:1 setting), the implementation below automatically switches to LJ
 * input when SDMMC2/4 clock rate is set equal to the source rate, and one of
 * PLLC4 branches is used as a source. Changing SDMMC rate-to-source ratio
 * from 1:1 automatically selects divided input. This switching mechanism has
 * no effect when PLLP or CLK_M is used as clock source.
 *
 * See also mux_pllp_clk_m_pllc4_out2_out1_out0_lj definition for detailed list
 * of LJ and divided inputs.
*/

static void sdmmc24_remap_inputs(struct clk *c)
{
	u32 sel_out, sel_in;
	u32 val = clk_readl(c->reg);

	sel_in = (val & periph_clk_source_mask(c)) >>
		periph_clk_source_shift(c);

	sel_out = (c->mul == c->div) ? sdmmc24_input_to_lj[sel_in] :
		sdmmc24_input_from_lj[sel_in];

	if (sel_out != sel_in) {
		val &= ~periph_clk_source_mask(c);
		val |= (sel_out << periph_clk_source_shift(c));

		clk_writel_delay(val, c->reg);
	}

}

static void tegra21_sdmmc24_clk_init(struct clk *c)
{
	tegra21_periph_clk_init(c);
	sdmmc24_remap_inputs(c);
}

static int tegra21_sdmmc24_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret = tegra21_periph_clk_set_rate(c, rate);
	if (!ret)
		sdmmc24_remap_inputs(c);
	return ret;
}

static int tegra21_sdmmc24_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val, sel_val;
	const struct clk_mux_sel *sel;
	pr_debug("%s: %s %s\n", __func__, c->name, p->name);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			sel_val = (c->mul == c->div) ?
				sdmmc24_input_to_lj[sel->value] :
				sdmmc24_input_from_lj[sel->value];

			val = clk_readl(c->reg);
			val &= ~periph_clk_source_mask(c);
			val |= (sel_val << periph_clk_source_shift(c));

			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			clk_writel_delay(val, c->reg);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

static struct clk_ops tegra_sdmmc24_clk_ops = {
	.init			= &tegra21_sdmmc24_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_parent		= &tegra21_sdmmc24_clk_set_parent,
	.set_rate		= &tegra21_sdmmc24_clk_set_rate,
	.round_rate		= &tegra21_periph_clk_round_rate,
	.reset			= &tegra21_periph_clk_reset,
};

/* SLCG clock ops */
static DEFINE_SPINLOCK(clk_slcg_lock);

static void tegra21_clk_slcg_init(struct clk *c)
{
	char *end;
	char root_name[32];
	u32 val = clk_readl(c->reg);

	c->state = (val & (0x1 << c->u.periph.clk_num)) ? ON : OFF;

	strcpy(root_name, c->name);
	end = strstr(root_name, "_slcg_ovr");
	if (end) {
		*end = '\0';
		c->parent = tegra_get_clock_by_name(root_name);
	}

	if (WARN(!c->parent, "%s: %s parent %s not found\n",
		 __func__, c->name, root_name))
		return;

	c->max_rate = c->parent->max_rate;
}


static int tegra21_clk_slcg_enable(struct clk *c)
{
	u32 val;
	unsigned long flags;

	pr_debug("%s on clock %s\n", __func__, c->name);

	spin_lock_irqsave(&clk_slcg_lock, flags);
	val = clk_readl(c->reg);
	val |= (0x1 << c->u.periph.clk_num);
	clk_writel_delay(val, c->reg);
	spin_unlock_irqrestore(&clk_slcg_lock, flags);

	return 0;
}

static void tegra21_clk_slcg_disable(struct clk *c)
{
	u32 val;
	unsigned long flags;

	pr_debug("%s on clock %s\n", __func__, c->name);

	spin_lock_irqsave(&clk_slcg_lock, flags);
	val = clk_readl(c->reg);
	val &= ~(0x1 << c->u.periph.clk_num);
	clk_writel_delay(val, c->reg);
	spin_unlock_irqrestore(&clk_slcg_lock, flags);
}

static struct clk_ops tegra_clk_slcg_ops = {
	.init			= &tegra21_clk_slcg_init,
	.enable			= &tegra21_clk_slcg_enable,
	.disable		= &tegra21_clk_slcg_disable,
};

/* Output clock ops */
static DEFINE_SPINLOCK(clk_out_lock);

static void tegra21_clk_out_init(struct clk *c)
{
	const struct clk_mux_sel *mux = NULL;
	const struct clk_mux_sel *sel;
	u32 val = pmc_readl(c->reg);

	c->state = (val & (0x1 << c->u.periph.clk_num)) ? ON : OFF;
	c->mul = 1;
	c->div = 1;

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (((val & periph_clk_source_mask(c)) >>
		     periph_clk_source_shift(c)) == sel->value)
			mux = sel;
	}
	BUG_ON(!mux);
	c->parent = mux->input;
}

static int tegra21_clk_out_enable(struct clk *c)
{
	u32 val;
	unsigned long flags;

	pr_debug("%s on clock %s\n", __func__, c->name);

	spin_lock_irqsave(&clk_out_lock, flags);
	val = pmc_readl(c->reg);
	val |= (0x1 << c->u.periph.clk_num);
	pmc_writel(val, c->reg);
	pmc_readl(c->reg);
	spin_unlock_irqrestore(&clk_out_lock, flags);

	return 0;
}

static void tegra21_clk_out_disable(struct clk *c)
{
	u32 val;
	unsigned long flags;

	pr_debug("%s on clock %s\n", __func__, c->name);

	spin_lock_irqsave(&clk_out_lock, flags);
	val = pmc_readl(c->reg);
	val &= ~(0x1 << c->u.periph.clk_num);
	pmc_writel(val, c->reg);
	pmc_readl(c->reg);
	spin_unlock_irqrestore(&clk_out_lock, flags);
}

static int tegra21_clk_out_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	unsigned long flags;
	const struct clk_mux_sel *sel;

	pr_debug("%s: %s %s\n", __func__, c->name, p->name);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			spin_lock_irqsave(&clk_out_lock, flags);
			val = pmc_readl(c->reg);
			val &= ~periph_clk_source_mask(c);
			val |= (sel->value << periph_clk_source_shift(c));
			pmc_writel(val, c->reg);
			pmc_readl(c->reg);
			spin_unlock_irqrestore(&clk_out_lock, flags);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}
	return -EINVAL;
}

static struct clk_ops tegra_clk_out_ops = {
	.init			= &tegra21_clk_out_init,
	.enable			= &tegra21_clk_out_enable,
	.disable		= &tegra21_clk_out_disable,
	.set_parent		= &tegra21_clk_out_set_parent,
};


/* External memory controller clock ops */
static void tegra21_emc_clk_init(struct clk *c)
{
	tegra21_periph_clk_init(c);
	tegra_emc_dram_type_init(c);
}

static long tegra21_emc_clk_round_updown(struct clk *c, unsigned long rate,
					 bool up)
{
	unsigned long new_rate = max(rate, c->min_rate);

	new_rate = tegra_emc_round_rate_updown(new_rate, up);
	if (IS_ERR_VALUE(new_rate))
		new_rate = c->max_rate;

	return new_rate;
}

static long tegra21_emc_clk_round_rate(struct clk *c, unsigned long rate)
{
	return tegra21_emc_clk_round_updown(c, rate, true);
}

void tegra_mc_divider_update(struct clk *emc)
{
	u32 val = clk_readl(emc->reg) &
		(PERIPH_CLK_SOURCE_EMC_MC_SAME | PERIPH_CLK_SOURCE_EMC_DIV2_EN);
	emc->child_bus->div = val == 1 ? 4 : (val == 2 ? 1 : 2);
}

static int tegra21_emc_clk_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	u32 div_value;
	struct clk *p;

	/* The tegra21x memory controller has an interlock with the clock
	 * block that allows memory shadowed registers to be updated,
	 * and then transfer them to the main registers at the same
	 * time as the clock update without glitches. During clock change
	 * operation both clock parent and divider may change simultaneously
	 * to achieve requested rate. */
	p = tegra_emc_predict_parent(rate, &div_value);
	div_value += 2;		/* emc has fractional DIV_U71 divider */
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: Failed to predict emc parent for rate %lu\n",
		       __func__, rate);
		return -EINVAL;
	}

	if (p == c->parent) {
		if (div_value == c->div)
			return 0;
	} else if (c->refcnt)
		tegra_clk_prepare_enable(p);

	ret = tegra_emc_set_rate_on_parent(rate, p);
	if (ret < 0)
		return ret;

	if (p != c->parent) {
		if(c->refcnt && c->parent)
			tegra_clk_disable_unprepare(c->parent);
		clk_reparent(c, p);
	}
	c->div = div_value;
	c->mul = 2;
	return 0;
}

static int tegra21_clk_emc_bus_update(struct clk *bus)
{
	struct clk *p = NULL;
	unsigned long rate, parent_rate, backup_rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra21_clk_shared_bus_update(bus, NULL, NULL, NULL);

	if (rate == clk_get_rate_locked(bus))
		return 0;

	return clk_set_rate_locked(bus, rate);
}

static struct clk_ops tegra_emc_clk_ops = {
	.init			= &tegra21_emc_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_rate		= &tegra21_emc_clk_set_rate,
	.round_rate		= &tegra21_emc_clk_round_rate,
	.round_rate_updown	= &tegra21_emc_clk_round_updown,
	.reset			= &tegra21_periph_clk_reset,
	.shared_bus_update	= &tegra21_clk_emc_bus_update,
};

static void tegra21_mc_clk_init(struct clk *c)
{
	c->state = ON;
	if (!(clk_readl(PERIPH_CLK_TO_ENB_REG(c)) & PERIPH_CLK_TO_BIT(c)))
		c->state = OFF;

	c->parent->child_bus = c;
	tegra_mc_divider_update(c->parent);
	c->mul = 1;
}

static struct clk_ops tegra_mc_clk_ops = {
	.init			= &tegra21_mc_clk_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
};

/*
 * EMC parent selection algorithm always keeps PLLM and/or PLLMB disabled when
 * the respective PLL is not used as EMC clock source. However, boot-loader may
 * leave both PLLs enabled. To allow proper start of the algorithm, make sure
 * PLLs are disabled if not used.
 */
static void tegra21_emc_sync_plls(struct clk *emc,
				  struct clk *pllm, struct clk *pllmb)
{
	if (emc->parent != pllm) {
		if (pllm->state == ON) {
			pllm->ops->disable(pllm);
			pllm->state = OFF;
		}
	}

	if (emc->parent != pllmb) {
		if (pllmb->state == ON) {
			pllmb->ops->disable(pllmb);
			pllmb->state = OFF;
		}
	}
}

#ifdef CONFIG_PM_SLEEP

/*
 * WAR: fixing SC7 entry rate is to ensure stable boot up after SC7 resume on
 * LP4 platforms. This ensures that there are no MRWs necesary by the bootrom
 * when waking the DRAM (since the MRs will already have valid settings for 204
 * MHz). Problem is that the BR is not aware of what FSP is in use.
 */
#define FIXED_SC7_ENTRY_RATE	204000000

static void tegra21_emc_clk_suspend(struct clk *c, unsigned long rate)
{
	/* No change if other than LPDDR4 */
	if (tegra_emc_get_dram_type() != DRAM_TYPE_LPDDR4)
		return;

	/* No change in emc configuration for LP1 */
	if (!tegra_is_lp0_suspend_mode())
		return;

	/*
	 * Scale EMC rate at boot rate - required for entering SC7(LP0)
	 * on LPDDR4.
	 */
	if (rate != FIXED_SC7_ENTRY_RATE)
		tegra21_emc_clk_set_rate(c, FIXED_SC7_ENTRY_RATE);
}
#endif

/* Clock doubler ops (non-atomic shared register access) */
static DEFINE_SPINLOCK(doubler_lock);

static void tegra21_clk_double_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->mul = val & (0x1 << c->reg_shift) ? 1 : 2;
	c->div = 1;
	c->state = ON;
	if (!(clk_readl(PERIPH_CLK_TO_ENB_REG(c)) & PERIPH_CLK_TO_BIT(c)))
		c->state = OFF;
};

static int tegra21_clk_double_set_rate(struct clk *c, unsigned long rate)
{
	u32 val;
	unsigned long parent_rate = clk_get_rate(c->parent);
	unsigned long flags;

	if (rate == parent_rate) {
		spin_lock_irqsave(&doubler_lock, flags);
		val = clk_readl(c->reg) | (0x1 << c->reg_shift);
		clk_writel(val, c->reg);
		c->mul = 1;
		c->div = 1;
		spin_unlock_irqrestore(&doubler_lock, flags);
		return 0;
	} else if (rate == 2 * parent_rate) {
		spin_lock_irqsave(&doubler_lock, flags);
		val = clk_readl(c->reg) & (~(0x1 << c->reg_shift));
		clk_writel(val, c->reg);
		c->mul = 2;
		c->div = 1;
		spin_unlock_irqrestore(&doubler_lock, flags);
		return 0;
	}
	return -EINVAL;
}

static struct clk_ops tegra_clk_double_ops = {
	.init			= &tegra21_clk_double_init,
	.enable			= &tegra21_periph_clk_enable,
	.disable		= &tegra21_periph_clk_disable,
	.set_rate		= &tegra21_clk_double_set_rate,
};

/* Audio sync clock ops */
static int tegra21_sync_source_set_rate(struct clk *c, unsigned long rate)
{
	c->rate = rate;
	return 0;
}

static struct clk_ops tegra_sync_source_ops = {
	.set_rate		= &tegra21_sync_source_set_rate,
};

static void tegra21_audio_sync_clk_init(struct clk *c)
{
	int source;
	const struct clk_mux_sel *sel;
	u32 val = clk_readl(c->reg);
	c->state = (val & AUDIO_SYNC_DISABLE_BIT) ? OFF : ON;
	source = val & AUDIO_SYNC_SOURCE_MASK;
	for (sel = c->inputs; sel->input != NULL; sel++)
		if (sel->value == source)
			break;
	BUG_ON(sel->input == NULL);
	c->parent = sel->input;
}

static int tegra21_audio_sync_clk_enable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	clk_writel((val & (~AUDIO_SYNC_DISABLE_BIT)), c->reg);
	return 0;
}

static void tegra21_audio_sync_clk_disable(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	clk_writel((val | AUDIO_SYNC_DISABLE_BIT), c->reg);
}

static int tegra21_audio_sync_clk_set_parent(struct clk *c, struct clk *p)
{
	u32 val;
	const struct clk_mux_sel *sel;
	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p) {
			val = clk_readl(c->reg);
			val &= ~AUDIO_SYNC_SOURCE_MASK;
			val |= sel->value;

			if (c->refcnt)
				tegra_clk_prepare_enable(p);

			clk_writel(val, c->reg);

			if (c->refcnt && c->parent)
				tegra_clk_disable_unprepare(c->parent);

			clk_reparent(c, p);
			return 0;
		}
	}

	return -EINVAL;
}

static struct clk_ops tegra_audio_sync_clk_ops = {
	.init       = tegra21_audio_sync_clk_init,
	.enable     = tegra21_audio_sync_clk_enable,
	.disable    = tegra21_audio_sync_clk_disable,
	.set_parent = tegra21_audio_sync_clk_set_parent,
};

/* cml1 (sata) clock ops */
static void tegra21_cml_clk_init(struct clk *c)
{
	u32 val = clk_readl(c->reg);
	c->state = val & (0x1 << c->u.periph.clk_num) ? ON : OFF;
}

static int tegra21_cml_clk_enable(struct clk *c)
{
	u32 val;
	unsigned long flags;
	struct clk *parent = c->parent;

	clk_lock_save(parent, &flags);
	val = clk_readl(c->reg);
	val |= (0x1 << c->u.periph.clk_num);
	clk_writel(val, c->reg);
	clk_unlock_restore(parent, &flags);

	return 0;
}

static void tegra21_cml_clk_disable(struct clk *c)
{
	u32 val;
	unsigned long flags;
	struct clk *parent = c->parent;

	clk_lock_save(parent, &flags);
	val = clk_readl(c->reg);
	val &= ~(0x1 << c->u.periph.clk_num);
	clk_writel(val, c->reg);
	clk_unlock_restore(parent, &flags);
}

static struct clk_ops tegra_cml_clk_ops = {
	.init			= &tegra21_cml_clk_init,
	.enable			= &tegra21_cml_clk_enable,
	.disable		= &tegra21_cml_clk_disable,
};

/* cbus ops */
/*
 * Some clocks require dynamic re-locking of source PLL in order to
 * achieve frequency scaling granularity that matches characterized
 * core voltage steps. The cbus clock creates a shared bus that
 * provides a virtual root for such clocks to hide and synchronize
 * parent PLL re-locking as well as backup operations.
*/

static void tegra21_clk_cbus_init(struct clk *c)
{
	c->state = OFF;
	c->set = true;
}

static int tegra21_clk_cbus_enable(struct clk *c)
{
	return 0;
}

static long tegra21_clk_cbus_round_updown(struct clk *c, unsigned long rate,
					  bool up)
{
	int i;

	if (!c->dvfs) {
		if (!c->min_rate)
			c->min_rate = c->parent->min_rate;
		rate = max(rate, c->min_rate);
		return rate;
	}

	/* update min now, since no dvfs table was available during init
	   (skip placeholder entries set to 1 kHz) */
	if (!c->min_rate) {
		for (i = 0; i < c->dvfs->num_freqs; i++) {
			if (c->dvfs->freqs[i] > 1 * c->dvfs->freqs_mult) {
				c->min_rate = c->dvfs->freqs[i];
				break;
			}
		}
		BUG_ON(!c->min_rate);

		if (c->stats.histogram) {
			c->stats.histogram->rates_num = c->dvfs->num_freqs;
			tegra_shared_bus_stats_allocate(c, c->stats.histogram);
		}
	}
	rate = max(rate, c->min_rate);

	for (i = 0; ; i++) {
		unsigned long f = c->dvfs->freqs[i];
		if ((f >= rate) || ((i + 1) >=  c->dvfs->num_freqs)) {
			if (!up && i && (f > rate))
				i--;
			break;
		}
	}

	if (c->stats.histogram)
		c->stats.histogram->new_rate_idx = i;
	return c->dvfs->freqs[i];
}

static long tegra21_clk_cbus_round_rate(struct clk *c, unsigned long rate)
{
	return tegra21_clk_cbus_round_updown(c, rate, true);
}

static int cbus_switch_one(struct clk *c, struct clk *p, u32 div, bool abort)
{
	int ret = 0;

	/* set new divider if it is bigger than the current one */
	if (c->div < c->mul * div) {
		ret = clk_set_div(c, div);
		if (ret) {
			pr_err("%s: failed to set %s clock divider %u: %d\n",
			       __func__, c->name, div, ret);
			if (abort)
				return ret;
		}
	}

	if (c->parent != p) {
		ret = clk_set_parent(c, p);
		if (ret) {
			pr_err("%s: failed to set %s clock parent %s: %d\n",
			       __func__, c->name, p->name, ret);
			if (abort)
				return ret;
		}
	}

	/* set new divider if it is smaller than the current one */
	if (c->div > c->mul * div) {
		ret = clk_set_div(c, div);
		if (ret)
			pr_err("%s: failed to set %s clock divider %u: %d\n",
			       __func__, c->name, div, ret);
	}

	return ret;
}

static int cbus_backup(struct clk *c)
{
	int ret;
	struct clk *user;

	list_for_each_entry(user, &c->shared_bus_list,
			u.shared_bus_user.node) {
		struct clk *client = user->u.shared_bus_user.client;
		if (client && (client->state == ON) &&
		    (client->parent == c->parent)) {
			ret = cbus_switch_one(
				client, c->shared_bus_backup.input,
				c->shared_bus_backup.value, true);
			if (ret)
				return ret;
		}
	}
	return 0;
}

static int cbus_dvfs_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	struct clk *user;

	list_for_each_entry(user, &c->shared_bus_list,
			u.shared_bus_user.node) {
		struct clk *client =  user->u.shared_bus_user.client;
		if (client && client->refcnt && (client->parent == c->parent)) {
			ret = tegra_dvfs_set_rate(client, rate);
			if (ret)
				return ret;
		}
	}
	return 0;
}

static void cbus_restore(struct clk *c)
{
	struct clk *user;

	list_for_each_entry(user, &c->shared_bus_list,
			u.shared_bus_user.node) {
		struct clk *client = user->u.shared_bus_user.client;
		if (client)
			cbus_switch_one(client, c->parent, c->div, false);
	}
}

static void cbus_skip(struct clk *c, unsigned long bus_rate)
{
	struct clk *user;
	unsigned long rate;

	if (!(c->shared_bus_flags & SHARED_BUS_USE_SKIPPERS))
		return;

	list_for_each_entry(user, &c->shared_bus_list,
			u.shared_bus_user.node) {
		struct clk *client = user->u.shared_bus_user.client;
		if (client && client->skipper &&
		    user->u.shared_bus_user.enabled) {
			/* Make sure skipper output is above the target */
			rate = user->u.shared_bus_user.rate;
			rate += bus_rate >> SUPER_SKIPPER_TERM_SIZE;

			clk_set_rate(client->skipper, rate);
			user->div = client->skipper->div;
			user->mul = client->skipper->mul;
		}
	}
}

static int get_next_backup_div(struct clk *c, unsigned long rate)
{
	u32 div = c->div;
	unsigned long backup_rate = clk_get_rate(c->shared_bus_backup.input);

	rate = max(rate, clk_get_rate_locked(c));
	rate = rate - (rate >> 2);	/* 25% margin for backup rate */
	if ((u64)rate * div < backup_rate)
		div = DIV_ROUND_UP(backup_rate, rate);

	BUG_ON(!div);
	return div;
}

static int tegra21_clk_cbus_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	bool dramp;

	if (rate == 0)
		return 0;

	ret = tegra_clk_prepare_enable(c->parent);
	if (ret) {
		pr_err("%s: failed to enable %s clock: %d\n",
		       __func__, c->name, ret);
		return ret;
	}

	dramp = tegra_pll_can_ramp_to_rate(c->parent, rate * c->div);
	if (!dramp) {
		c->shared_bus_backup.value = get_next_backup_div(c, rate);
		ret = cbus_backup(c);
		if (ret)
			goto out;
	}

	ret = clk_set_rate(c->parent, rate * c->div);
	if (ret) {
		pr_err("%s: failed to set %s clock rate %lu: %d\n",
		       __func__, c->name, rate, ret);
		goto out;
	}

	/* Safe voltage setting is taken care of by cbus clock dvfs; the call
	 * below only records requirements for each enabled client.
	 */
	if (dramp)
		ret = cbus_dvfs_set_rate(c, rate);

	cbus_restore(c);
	cbus_skip(c, rate);

out:
	tegra_clk_disable_unprepare(c->parent);
	return ret;
}

static inline void cbus_move_enabled_user(
	struct clk *user, struct clk *dst, struct clk *src)
{
	tegra_clk_prepare_enable(dst);
	list_move_tail(&user->u.shared_bus_user.node, &dst->shared_bus_list);
	tegra_clk_disable_unprepare(src);
	clk_reparent(user, dst);
}

#ifdef CONFIG_TEGRA_DYNAMIC_CBUS
static int tegra21_clk_cbus_update(struct clk *bus)
{
	int ret, mv;
	struct clk *slow = NULL;
	struct clk *top = NULL;
	unsigned long rate;
	unsigned long old_rate;
	unsigned long ceiling;

	if (detach_shared_bus)
		return 0;

	rate = tegra21_clk_shared_bus_update(bus, &top, &slow, &ceiling);

	/* use dvfs table of the slowest enabled client as cbus dvfs table */
	if (bus->dvfs && slow && (slow != bus->u.cbus.slow_user)) {
		unsigned long *dest = &bus->dvfs->freqs[0];
		unsigned long *src =
			&slow->u.shared_bus_user.client->dvfs->freqs[0];
		memcpy(dest, src, sizeof(*dest) * bus->dvfs->num_freqs);
	}

	/* update bus state variables and rate */
	bus->u.cbus.slow_user = slow;
	bus->u.cbus.top_user = top;

	rate = tegra21_clk_cap_shared_bus(bus, rate, ceiling);
	mv = tegra_dvfs_predict_mv_at_hz_no_tfloor(bus, rate);
	if (IS_ERR_VALUE(mv))
		return -EINVAL;

	if (bus->dvfs) {
		mv -= bus->dvfs->cur_millivolts;
		if (bus->refcnt && (mv > 0)) {
			ret = tegra_dvfs_set_rate(bus, rate);
			if (ret)
				return ret;
		}
	}

	old_rate = clk_get_rate_locked(bus);
	if (IS_ENABLED(CONFIG_TEGRA_MIGRATE_CBUS_USERS) || (old_rate != rate)) {
		ret = bus->ops->set_rate(bus, rate);
		if (ret)
			return ret;
	} else {
		/* Skippers may change even if bus rate is the same */
		cbus_skip(bus, rate);
	}

	if (bus->dvfs) {
		if (bus->refcnt && (mv <= 0)) {
			ret = tegra_dvfs_set_rate(bus, rate);
			if (ret)
				return ret;
		}
	}

	clk_rate_change_notify(bus, rate);
	return 0;
};
#else
static int tegra21_clk_cbus_update(struct clk *bus)
{
	unsigned long rate, old_rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra21_clk_shared_bus_update(bus, NULL, NULL, NULL);

	old_rate = clk_get_rate_locked(bus);
	if (rate == old_rate) {
		/* Skippers may change even if bus rate is the same */
		cbus_skip(bus, rate);
		return 0;
	}

	return clk_set_rate_locked(bus, rate);
}
#endif

static int tegra21_clk_cbus_migrate_users(struct clk *user)
{
#ifdef CONFIG_TEGRA_MIGRATE_CBUS_USERS
	struct clk *src_bus, *dst_bus, *top_user, *c;
	struct list_head *pos, *n;

	if (!user->u.shared_bus_user.client || !user->inputs)
		return 0;

	/* Dual cbus on Tegra12 */
	src_bus = user->inputs[0].input;
	dst_bus = user->inputs[1].input;

	if (!src_bus->u.cbus.top_user && !dst_bus->u.cbus.top_user)
		return 0;

	/* Make sure top user on the source bus is requesting highest rate */
	if (!src_bus->u.cbus.top_user || (dst_bus->u.cbus.top_user &&
		bus_user_request_is_lower(src_bus->u.cbus.top_user,
					   dst_bus->u.cbus.top_user)))
		swap(src_bus, dst_bus);

	/* If top user is the slow one on its own (source) bus, do nothing */
	top_user = src_bus->u.cbus.top_user;
	BUG_ON(!top_user->u.shared_bus_user.client);
	if (!bus_user_is_slower(src_bus->u.cbus.slow_user, top_user))
		return 0;

	/* If source bus top user is slower than all users on destination bus,
	   move top user; otherwise move all users slower than the top one */
	if (!dst_bus->u.cbus.slow_user ||
	    !bus_user_is_slower(dst_bus->u.cbus.slow_user, top_user)) {
		cbus_move_enabled_user(top_user, dst_bus, src_bus);
	} else {
		list_for_each_safe(pos, n, &src_bus->shared_bus_list) {
			c = list_entry(pos, struct clk, u.shared_bus_user.node);
			if (c->u.shared_bus_user.enabled &&
			    c->u.shared_bus_user.client &&
			    bus_user_is_slower(c, top_user))
				cbus_move_enabled_user(c, dst_bus, src_bus);
		}
	}

	/* Update destination bus 1st (move clients), then source */
	tegra_clk_shared_bus_update(dst_bus);
	tegra_clk_shared_bus_update(src_bus);
#endif
	return 0;
}

static struct clk_ops tegra_clk_cbus_ops = {
	.init = tegra21_clk_cbus_init,
	.enable = tegra21_clk_cbus_enable,
	.set_rate = tegra21_clk_cbus_set_rate,
	.round_rate = tegra21_clk_cbus_round_rate,
	.round_rate_updown = tegra21_clk_cbus_round_updown,
	.shared_bus_update = tegra21_clk_cbus_update,
};

/* shared bus ops */
/*
 * Some clocks may have multiple downstream users that need to request a
 * higher clock rate.  Shared bus clocks provide a unique shared_bus_user
 * clock to each user.  The frequency of the bus is set to the highest
 * enabled shared_bus_user clock, with a minimum value set by the
 * shared bus.
 *
 * Optionally shared bus may support users migration. Since shared bus and
 * its * children (users) have reversed rate relations: user rates determine
 * bus rate, * switching user from one parent/bus to another may change rates
 * of both parents. Therefore we need a cross-bus lock on top of individual
 * user and bus locks. For now, limit bus switch support to cbus only if
 * CONFIG_TEGRA_MIGRATE_CBUS_USERS is set.
 */

static unsigned long tegra21_clk_shared_bus_update(struct clk *bus,
	struct clk **bus_top, struct clk **bus_slow, unsigned long *rate_cap)
{
	struct clk *c;
	struct clk *slow = NULL;
	struct clk *top = NULL;

	unsigned long override_rate = 0;
	unsigned long top_rate = 0;
	unsigned long rate = bus->min_rate;
	unsigned long bw = 0;
	unsigned long iso_bw = 0;
	unsigned long ceiling = bus->max_rate;
	unsigned long ceiling_but_iso = bus->max_rate;
	u32 usage_flags = 0;
	bool rate_set = false;

	struct clk *crit_cap = NULL;
	struct clk *crit_cap_but_iso = NULL;
	struct clk *crit_floor = NULL;
	struct clk *override_clk = NULL;

	list_for_each_entry(c, &bus->shared_bus_list,
			u.shared_bus_user.node) {
		bool cap_user = (c->u.shared_bus_user.mode == SHARED_CEILING) ||
			(c->u.shared_bus_user.mode == SHARED_CEILING_BUT_ISO);
		/*
		 * Ignore requests from disabled floor and bw users, and from
		 * auto-users riding the bus. Always honor ceiling users, even
		 * if they are disabled - we do not want to keep enabled parent
		 * bus just because ceiling is set.
		 */
		if (c->u.shared_bus_user.enabled || cap_user) {
			unsigned long request_rate = c->u.shared_bus_user.rate;
			usage_flags |= c->u.shared_bus_user.usage_flag;

			if (!(c->flags & BUS_RATE_LIMIT))
				rate_set = true;

			switch (c->u.shared_bus_user.mode) {
			case SHARED_ISO_BW:
				iso_bw += request_rate;
				if (iso_bw > bus->max_rate)
					iso_bw = bus->max_rate;
				/* fall thru */
			case SHARED_BW:
				bw += request_rate;
				if (bw > bus->max_rate)
					bw = bus->max_rate;
				break;
			case SHARED_CEILING_BUT_ISO:
				if (ceiling_but_iso > request_rate) {
					ceiling_but_iso = request_rate;
					crit_cap_but_iso = c;
				}
				break;
			case SHARED_CEILING:
				if (ceiling > request_rate) {
					ceiling = request_rate;
					crit_cap = c;
				}
				break;
			case SHARED_OVERRIDE:
				if (override_rate == 0) {
					override_rate = request_rate;
					override_clk = c;
				}
				break;
			case SHARED_AUTO:
				break;
			case SHARED_FLOOR:
			default:
				if (rate <= request_rate) {
					if (!(c->flags & BUS_RATE_LIMIT) ||
					    (rate < request_rate))
						crit_floor = c;
					rate = request_rate;
				}
				if (c->u.shared_bus_user.client
							&& request_rate) {
					if (top_rate < request_rate) {
						top_rate = request_rate;
						top = c;
					} else if ((top_rate == request_rate) &&
						bus_user_is_slower(c, top)) {
						top = c;
					}
				}
			}
			if (c->u.shared_bus_user.client &&
				(!slow || bus_user_is_slower(c, slow)))
				slow = c;
		}
	}

	if (bus->flags & PERIPH_EMC_ENB) {
		unsigned long iso_bw_min;
		bw = tegra_emc_apply_efficiency(
			bw, iso_bw, bus->max_rate, usage_flags, &iso_bw_min);
		if (bus->ops && bus->ops->round_rate)
			iso_bw_min = bus->ops->round_rate(bus, iso_bw_min);
		ceiling_but_iso = max(ceiling_but_iso, iso_bw_min);
	}

	if (rate < bw) {
		rate = bw;
		crit_floor = NULL;
	}
	rate = override_rate ? : rate;

	if (ceiling > ceiling_but_iso) {
		ceiling = ceiling_but_iso;
		crit_cap = crit_cap_but_iso;
	}

	if (override_rate) {
		ceiling = bus->max_rate;
		crit_cap = NULL;
		crit_floor = override_clk;
	}
	bus->override_rate = override_rate;

	if (bus->stats.histogram) {
		struct bus_stats *stats = bus->stats.histogram;

		if (!rate_set) {
			crit_cap = NULL;
			crit_floor = NULL;
		}
		stats->new_cap_idx = !crit_cap ? stats->users_num :
			crit_cap->u.shared_bus_user.stats_idx;
		stats->new_floor_idx = !crit_floor ? stats->users_num :
			crit_floor->u.shared_bus_user.stats_idx;
	}

	if (bus_top && bus_slow && rate_cap) {
		/* If dynamic bus dvfs table, let the caller to complete
		   rounding and aggregation */
		*bus_top = top;
		*bus_slow = slow;
		*rate_cap = ceiling;
	} else {
		/*
		 * If satic bus dvfs table, complete rounding and aggregation.
		 * In case when no user requested bus rate, and bus retention
		 * is enabled, don't scale down - keep current rate.
		 */
		if (!rate_set && (bus->shared_bus_flags & SHARED_BUS_RETENTION))
			rate = clk_get_rate_locked(bus);

		rate = tegra21_clk_cap_shared_bus(bus, rate, ceiling);
	}

	return rate;
};

static unsigned long tegra21_clk_cap_shared_bus(struct clk *bus,
	unsigned long rate, unsigned long ceiling)
{
	bool capped;
	struct bus_stats *stats = bus->stats.histogram;

	if (bus->ops && bus->ops->round_rate_updown)
		ceiling = bus->ops->round_rate_updown(bus, ceiling, false);

	capped = rate > ceiling;
	if (capped) {
		rate = ceiling;
		if (stats)
			tegra_shared_bus_stats_update(stats,
				stats->new_cap_idx, stats->new_rate_idx);
	}

	if (bus->ops && bus->ops->round_rate)
		rate = bus->ops->round_rate(bus, rate);

	if (!capped && stats) {
		tegra_shared_bus_stats_update(stats,
			stats->new_floor_idx, stats->new_rate_idx);
	}

	return rate;
}

static int tegra_clk_shared_bus_migrate_users(struct clk *user)
{
	if (detach_shared_bus)
		return 0;

	/* Only cbus migration is supported */
	if (user->flags & PERIPH_ON_CBUS)
		return tegra21_clk_cbus_migrate_users(user);
	return -ENOSYS;
}

static void tegra_clk_shared_bus_user_init(struct clk *c)
{
	c->max_rate = c->parent->max_rate;
	c->u.shared_bus_user.rate = c->parent->max_rate;
	c->state = OFF;
	c->set = true;

	if ((c->u.shared_bus_user.mode == SHARED_CEILING) ||
	    (c->u.shared_bus_user.mode == SHARED_CEILING_BUT_ISO)) {
		c->state = ON;
		c->refcnt++;
	}

	if (c->parent->stats.histogram) {
		int i = c->parent->stats.histogram->users_num++;
		c->u.shared_bus_user.stats_idx = i;
		if (i < STATS_USERS_LIST_SIZE)
			c->parent->stats.histogram->users_list[i] = c;
	}

	if (c->u.shared_bus_user.client_id) {
		c->u.shared_bus_user.client =
			tegra_get_clock_by_name(c->u.shared_bus_user.client_id);
		if (!c->u.shared_bus_user.client) {
			pr_err("%s: could not find clk %s\n", __func__,
			       c->u.shared_bus_user.client_id);
			return;
		}
		c->u.shared_bus_user.client->flags |=
			c->parent->flags & PERIPH_ON_CBUS;
		c->flags |= c->parent->flags & PERIPH_ON_CBUS;
		c->div = 1;
		c->mul = 1;
	}

	list_add_tail(&c->u.shared_bus_user.node,
		&c->parent->shared_bus_list);
}

static int tegra_clk_shared_bus_user_set_parent(struct clk *c, struct clk *p)
{
	int ret;
	const struct clk_mux_sel *sel;

	if (detach_shared_bus)
		return 0;

	if (c->parent == p)
		return 0;

	if (!(c->inputs && c->cross_clk_mutex && clk_cansleep(c)))
		return -ENOSYS;

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == p)
			break;
	}
	if (!sel->input)
		return -EINVAL;

	if (c->refcnt)
		tegra_clk_prepare_enable(p);

	list_move_tail(&c->u.shared_bus_user.node, &p->shared_bus_list);
	ret = tegra_clk_shared_bus_update(p);
	if (ret) {
		list_move_tail(&c->u.shared_bus_user.node,
			       &c->parent->shared_bus_list);
		tegra_clk_shared_bus_update(c->parent);
		tegra_clk_disable_unprepare(p);
		return ret;
	}

	tegra_clk_shared_bus_update(c->parent);

	if (c->refcnt)
		tegra_clk_disable_unprepare(c->parent);

	clk_reparent(c, p);

	return 0;
}

static int tegra_clk_shared_bus_user_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	unsigned long flags;

	clk_lock_save(c->parent, &flags);
	c->u.shared_bus_user.rate = rate;
	ret = tegra_clk_shared_bus_update_locked(c->parent);
	clk_unlock_restore(c->parent, &flags);

	if (!ret && c->cross_clk_mutex && clk_cansleep(c))
		tegra_clk_shared_bus_migrate_users(c);

	return ret;
}

static long tegra_clk_shared_bus_user_round_rate(
	struct clk *c, unsigned long rate)
{
	/*
	 * Defer rounding requests until aggregated. BW users must not be
	 * rounded at all, others just clipped to bus range (some clients
	 * may use round api to find limits).
	 */

	if ((c->u.shared_bus_user.mode != SHARED_BW) &&
	    (c->u.shared_bus_user.mode != SHARED_ISO_BW)) {
		if (rate > c->parent->max_rate) {
			rate = c->parent->max_rate;
		} else {
			/* Skippers allow to run below bus minimum */
			bool use_skip = c->parent->shared_bus_flags &
				SHARED_BUS_USE_SKIPPERS;
			struct clk *client = c->u.shared_bus_user.client;
			int skip = (use_skip && client && client->skipper) ?
				SUPER_SKIPPER_TERM_SIZE : 0;
			unsigned long min_rate = c->parent->min_rate >> skip;

			if (rate < min_rate)
				rate = min_rate;
		}
	}
	return rate;
}

static int tegra_clk_shared_bus_user_enable(struct clk *c)
{
	int ret;
	unsigned long flags;

	clk_lock_save(c->parent, &flags);
	c->u.shared_bus_user.enabled = true;
	ret = tegra_clk_shared_bus_update_locked(c->parent);
	if (!ret && c->u.shared_bus_user.client)
		ret = tegra_clk_prepare_enable(c->u.shared_bus_user.client);
	clk_unlock_restore(c->parent, &flags);

	if (!ret && c->cross_clk_mutex && clk_cansleep(c))
		tegra_clk_shared_bus_migrate_users(c);

	return ret;
}

static void tegra_clk_shared_bus_user_disable(struct clk *c)
{
	unsigned long flags;

	clk_lock_save(c->parent, &flags);
	if (c->u.shared_bus_user.client)
		tegra_clk_disable_unprepare(c->u.shared_bus_user.client);
	c->u.shared_bus_user.enabled = false;
	tegra_clk_shared_bus_update_locked(c->parent);
	clk_unlock_restore(c->parent, &flags);

	if (c->cross_clk_mutex && clk_cansleep(c))
		tegra_clk_shared_bus_migrate_users(c);
}

static void tegra_clk_shared_bus_user_reset(struct clk *c, bool assert)
{
	if (c->u.shared_bus_user.client) {
		if (c->u.shared_bus_user.client->ops &&
		    c->u.shared_bus_user.client->ops->reset)
			c->u.shared_bus_user.client->ops->reset(
				c->u.shared_bus_user.client, assert);
	}
}

static struct clk_ops tegra_clk_shared_bus_user_ops = {
	.init = tegra_clk_shared_bus_user_init,
	.enable = tegra_clk_shared_bus_user_enable,
	.disable = tegra_clk_shared_bus_user_disable,
	.set_parent = tegra_clk_shared_bus_user_set_parent,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.reset = tegra_clk_shared_bus_user_reset,
};

/* shared bus connector ops (user/bus connector to cascade shared buses) */
static int tegra21_clk_shared_connector_update(struct clk *bus)
{
	unsigned long rate, old_rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra21_clk_shared_bus_update(bus, NULL, NULL, NULL);

	old_rate = clk_get_rate_locked(bus);
	if (rate == old_rate)
		return 0;

	return clk_set_rate_locked(bus, rate);
}

static struct clk_ops tegra_clk_shared_connector_ops = {
	.init = tegra_clk_shared_bus_user_init,
	.enable = tegra_clk_shared_bus_user_enable,
	.disable = tegra_clk_shared_bus_user_disable,
	.set_parent = tegra_clk_shared_bus_user_set_parent,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.reset = tegra_clk_shared_bus_user_reset,
	.shared_bus_update = tegra21_clk_shared_connector_update,
};

/* coupled gate ops */
/*
 * Some clocks may have common enable/disable control, but run at different
 * rates, and have different dvfs tables. Coupled gate clock synchronize
 * enable/disable operations for such clocks.
 */

static int tegra21_clk_coupled_gate_enable(struct clk *c)
{
	int ret;
	const struct clk_mux_sel *sel;

	BUG_ON(!c->inputs);
	pr_debug("%s on clock %s\n", __func__, c->name);

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == c->parent)
			continue;

		ret = tegra_clk_prepare_enable(sel->input);
		if (ret) {
			while (sel != c->inputs) {
				sel--;
				if (sel->input == c->parent)
					continue;
				tegra_clk_disable_unprepare(sel->input);
			}
			return ret;
		}
	}

	return tegra21_periph_clk_enable(c);
}

static void tegra21_clk_coupled_gate_disable(struct clk *c)
{
	const struct clk_mux_sel *sel;

	BUG_ON(!c->inputs);
	pr_debug("%s on clock %s\n", __func__, c->name);

	tegra21_periph_clk_disable(c);

	if (!c->refcnt)	/* happens only on boot clean-up: don't propagate */
		return;

	for (sel = c->inputs; sel->input != NULL; sel++) {
		if (sel->input == c->parent)
			continue;

		if (sel->input->set)	/* enforce coupling after boot only */
			tegra_clk_disable_unprepare(sel->input);
	}
}

/*
 * AHB and APB shared bus operations
 * APB shared bus is a user of AHB shared bus
 * AHB shared bus is a user of SCLK complex shared bus
 * SCLK/AHB and AHB/APB dividers can be dynamically changed. When AHB and APB
 * users requests are propagated to SBUS target rate, current values of the
 * dividers are ignored, and flat maximum request is selected as SCLK bus final
 * target. Then the dividers will be re-evaluated, based on AHB and APB targets.
 * Both AHB and APB buses are always enabled.
 */
static void tegra21_clk_ahb_apb_init(struct clk *c, struct clk *bus_clk)
{
	tegra_clk_shared_bus_user_init(c);
	c->max_rate = bus_clk->max_rate;
	c->min_rate = bus_clk->min_rate;
	c->mul = bus_clk->mul;
	c->div = bus_clk->div;

	c->u.shared_bus_user.rate = clk_get_rate(bus_clk);
	c->u.shared_bus_user.enabled = true;
	c->parent->child_bus = c;
}

static void tegra21_clk_ahb_init(struct clk *c)
{
	struct clk *bus_clk = c->parent->u.system.hclk;
	tegra21_clk_ahb_apb_init(c, bus_clk);
}

static void tegra21_clk_apb_init(struct clk *c)
{
	struct clk *bus_clk = c->parent->parent->u.system.pclk;
	tegra21_clk_ahb_apb_init(c, bus_clk);
}

static int tegra21_clk_ahb_apb_update(struct clk *bus)
{
	unsigned long rate;

	if (detach_shared_bus)
		return 0;

	rate = tegra21_clk_shared_bus_update(bus, NULL, NULL, NULL);
	return clk_set_rate_locked(bus, rate);
}

static struct clk_ops tegra_clk_ahb_ops = {
	.init = tegra21_clk_ahb_init,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.shared_bus_update = tegra21_clk_ahb_apb_update,
};

static struct clk_ops tegra_clk_apb_ops = {
	.init = tegra21_clk_apb_init,
	.set_rate = tegra_clk_shared_bus_user_set_rate,
	.round_rate = tegra_clk_shared_bus_user_round_rate,
	.shared_bus_update = tegra21_clk_ahb_apb_update,
};

static struct clk_ops tegra_clk_coupled_gate_ops = {
	.init			= tegra21_periph_clk_init,
	.enable			= tegra21_clk_coupled_gate_enable,
	.disable		= tegra21_clk_coupled_gate_disable,
	.reset			= &tegra21_periph_clk_reset,
};


/* Clock definitions */
static struct clk tegra_clk_32k = {
	.name = "clk_32k",
	.clk_id = TEGRA210_CLK_ID_CLK_32K,
	.rate = 32768,
	.ops  = NULL,
	.max_rate = 32768,
};

static struct clk tegra_clk_osc = {
	.name      = "osc",
	.clk_id    = TEGRA210_CLK_ID_CLK_OSC,
	.flags     = ENABLE_ON_INIT,
	.ops       = &tegra_osc_ops,
	.max_rate  = 48000000,
};

static struct clk tegra_clk_m = {
	.name      = "clk_m",
	.clk_id    = TEGRA210_CLK_ID_CLK_M,
	.flags     = ENABLE_ON_INIT,
	.parent    = &tegra_clk_osc,
	.ops       = &tegra_clk_m_ops,
	.max_rate  = 48000000,
};

static struct clk tegra_clk_m_div2 = {
	.name      = "clk_m_div2",
	.clk_id    = TEGRA210_CLK_ID_CLK_M_DIV2,
	.ops       = &tegra_clk_m_div_ops,
	.parent    = &tegra_clk_m,
	.mul       = 1,
	.div       = 2,
	.state     = ON,
	.max_rate  = 24000000,
};

static struct clk tegra_clk_m_div4 = {
	.name      = "clk_m_div4",
	.clk_id    = TEGRA210_CLK_ID_CLK_M_DIV4,
	.ops       = &tegra_clk_m_div_ops,
	.parent    = &tegra_clk_m,
	.mul       = 1,
	.div       = 4,
	.state     = ON,
	.max_rate  = 12000000,
};

static struct clk tegra_pll_ref = {
	.name      = "pll_ref",
	.clk_id    = TEGRA210_CLK_ID_PLL_REF,
	.flags     = ENABLE_ON_INIT,
	.ops       = &tegra_pll_ref_ops,
	.parent    = &tegra_clk_osc,
	.max_rate  = 38400000,
};

static struct clk_pll_freq_table tegra_pll_cx_freq_table[] = {
	{ 12000000, 510000000,  85, 1, 2},
	{ 13000000, 510000000,  78, 1, 2},	/* actual: 507.0 MHz */
	{ 38400000, 510000000,  79, 3, 2},	/* actual: 505.6 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls pllcx_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.reset_mask = PLLCX_MISC0_RESET,
	.reset_reg_idx = PLL_MISC0_IDX,
	.iddq_mask = PLLCX_MISC1_IDDQ,
	.iddq_reg_idx = PLL_MISC1_IDX,
	.lock_mask = PLLCX_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,
};

static struct clk_pll_div_layout pllcx_div_layout = {
	.mdiv_shift = 0,
	.mdiv_mask = 0xff,
	.ndiv_shift = 10,
	.ndiv_mask = 0xff << 10,
	.pdiv_shift = 20,
	.pdiv_mask = 0x1f << 20,
	.pdiv_to_p = pll_qlin_pdiv_to_p,
	.pdiv_max = PLL_QLIN_PDIV_MAX,
};

static struct clk tegra_pll_c = {
	.name      = "pll_c",
	.clk_id    = TEGRA210_CLK_ID_PLL_C,
	.ops       = &tegra_pllcx_ops,
	.reg       = 0x80,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1200000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 700000000,
		.cf_min    = 12000000,
		.cf_max    = 50000000,
		.vco_min   = 600000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_pll_cx_freq_table,
		.lock_delay = 300,
		.misc0 = 0x88 - 0x80,
		.misc1 = 0x8c - 0x80,
		.misc2 = 0x5d0 - 0x80,
		.misc3 = 0x5d4 - 0x80,
		.controls = &pllcx_controls,
		.div_layout = &pllcx_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.dyn_ramp = pllcx_dyn_ramp,
		.set_defaults = pllcx_set_defaults,
	},
};

static struct clk tegra_pll_c_out1 = {
	.name      = "pll_c_out1",
	.clk_id    = TEGRA210_CLK_ID_PLL_C_OUT1,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | PERIPH_ON_CBUS,
	.parent    = &tegra_pll_c,
	.reg       = 0x84,
	.reg_shift = 0,
	.max_rate  = 700000000,
};

static struct clk tegra_pll_c2 = {
	.name      = "pll_c2",
	.clk_id    = TEGRA210_CLK_ID_PLL_C2,
	.ops       = &tegra_pllcx_ops,
	.reg       = 0x4e8,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1200000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 700000000,
		.cf_min    = 12000000,
		.cf_max    = 50000000,
		.vco_min   = 600000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_pll_cx_freq_table,
		.lock_delay = 300,
		.misc0 = 0x4ec - 0x4e8,
		.misc1 = 0x4f0 - 0x4e8,
		.misc2 = 0x4f4 - 0x4e8,
		.misc3 = 0x4f8 - 0x4e8,
		.controls = &pllcx_controls,
		.div_layout = &pllcx_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.dyn_ramp = pllcx_dyn_ramp,
		.set_defaults = pllcx_set_defaults,
	},
};

static struct clk tegra_pll_c3 = {
	.name      = "pll_c3",
	.clk_id    = TEGRA210_CLK_ID_PLL_C3,
	.ops       = &tegra_pllcx_ops,
	.reg       = 0x4fc,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1200000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 700000000,
		.cf_min    = 12000000,
		.cf_max    = 50000000,
		.vco_min   = 600000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_pll_cx_freq_table,
		.lock_delay = 300,
		.misc0 = 0x500 - 0x4fc,
		.misc1 = 0x504 - 0x4fc,
		.misc2 = 0x508 - 0x4fc,
		.misc3 = 0x50c - 0x4fc,
		.controls = &pllcx_controls,
		.div_layout = &pllcx_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.dyn_ramp = pllcx_dyn_ramp,
		.set_defaults = pllcx_set_defaults,
	},
};

static struct clk tegra_pll_a1 = {
	.name      = "pll_a1",
	.clk_id    = TEGRA210_CLK_ID_PLL_A1,
	.ops       = &tegra_pllcx_ops,
	.reg       = 0x6a4,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1200000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 700000000,
		.cf_min    = 12000000,
		.cf_max    = 50000000,
		.vco_min   = 600000000,
		.vco_max   = 1200000000,
		.freq_table = tegra_pll_cx_freq_table,
		.lock_delay = 300,
		.misc0 = 0x6a8 - 0x6a4,
		.misc1 = 0x6ac - 0x6a4,
		.misc2 = 0x6b0 - 0x6a4,
		.misc3 = 0x6b4 - 0x6a4,
		.controls = &pllcx_controls,
		.div_layout = &pllcx_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.dyn_ramp = pllcx_dyn_ramp,
		.set_defaults = pllcx_set_defaults,
	},
};

static struct clk_pll_freq_table tegra_pll_a_freq_table[] = {
	{ 12000000, 282240000, 47, 1, 2, 1, 0xf148},	/* actual: 282240234 */
	{ 12000000, 368640000, 61, 1, 2, 1, 0xfe15},	/* actual: 368640381 */
	{ 12000000, 240000000, 60, 1, 3, 1, },

	{ 13000000, 282240000, 43, 1, 2, 1, 0xfd7d},	/* actual: 282239807 */
	{ 13000000, 368640000, 56, 1, 2, 1, 0x06d8},	/* actual: 368640137 */
	{ 13000000, 240000000, 55, 1, 3, 1, },		/* actual: 238.3 MHz */

	{ 38400000, 282240000, 44, 3, 2, 1, 0xf333},	/* actual: 282239844 */
	{ 38400000, 368640000, 57, 3, 2, 1, 0x0333},	/* actual: 368639844 */
	{ 38400000, 240000000, 75, 3, 4, 1,},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls plla_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLA_BASE_IDDQ,
	.iddq_reg_idx = PLL_BASE_IDX,
	.lock_mask = PLLA_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,

	.sdm_en_mask = PLLA_MISC2_EN_SDM,
	.sdm_ctrl_reg_idx = PLL_MISC2_IDX,
};

static struct clk_pll_div_layout plla_div_layout = {
	.mdiv_shift = 0,
	.mdiv_mask = 0xff,
	.ndiv_shift = 8,
	.ndiv_mask = 0xff << 8,
	.pdiv_shift = 20,
	.pdiv_mask = 0x1f << 20,
	.pdiv_to_p = pll_qlin_pdiv_to_p,
	.pdiv_max = PLL_QLIN_PDIV_MAX,

	.sdm_din_shift = 0,
	.sdm_din_mask = 0xffff,
	.sdm_din_reg_idx = PLL_MISC1_IDX,
};

static struct clk tegra_pll_a = {
	.name      = "pll_a",
	.clk_id    = TEGRA210_CLK_ID_PLL_A,
	.ops       = &tegra_plla_ops,
	.reg       = 0xb0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1000000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 800000000,
		.cf_min    = 12000000,
		.cf_max    = 19200000,
		.vco_min   = 500000000,
		.vco_max   = 1000000000,
		.freq_table = tegra_pll_a_freq_table,
		.lock_delay = 300,
		.misc0 = 0xbc - 0xb0,
		.misc1 = 0xb8 - 0xb0,
		.misc2 = 0x5d8 - 0xb0,
		.controls = &plla_controls,
		.div_layout = &plla_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.set_defaults = plla_set_defaults,
	},
};

static struct clk tegra_pll_a_out0 = {
	.name      = "pll_a_out0",
	.clk_id    = TEGRA210_CLK_ID_PLL_A_OUT0,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71,
	.parent    = &tegra_pll_a,
	.reg       = 0xb4,
	.reg_shift = 0,
	.max_rate  = 600000000,
};

static struct clk_mux_sel mux_plla[] = {
	{ .input = &tegra_pll_a, .value = 0},
	{ NULL, 0},
};

static struct clk tegra_pll_a_out_adsp = {
	.name       = "pll_a_out_adsp",
	.clk_id     = TEGRA210_CLK_ID_PLL_A_OUT_ADSP,
	.ops        = &tegra_periph_clk_ops,
	.inputs     = mux_plla,
	.flags      = PERIPH_NO_RESET,
	.max_rate   = 1000000000,
	.u.periph = {
		.clk_num = 188,
	},
};

static struct clk_mux_sel mux_plla_out0[] = {
	{ .input = &tegra_pll_a_out0, .value = 0},
	{ NULL, 0},
};

static struct clk tegra_pll_a_out0_out_adsp = {
	.name       = "pll_a_out0_out_adsp",
	.clk_id     = TEGRA210_CLK_ID_PLL_A_OUT0_OUT_ADSP,
	.ops        = &tegra_periph_clk_ops,
	.inputs     = mux_plla_out0,
	.flags      = PERIPH_NO_RESET,
	.max_rate   = 600000000,
	.u.periph = {
		.clk_num = 188,
	},
};

static struct clk_pll_freq_table tegra_pll_d_freq_table[] = {
	{ 12000000, 594000000,  99, 1, 2},
	{ 13000000, 594000000,  91, 1, 2, 0, 0xfc4f},	/* actual: 594000183 */
	{ 38400000, 594000000,  30, 1, 2, 0, 0x0e00},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls plld_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLD_MISC0_IDDQ,
	.iddq_reg_idx = PLL_MISC0_IDX,
	.lock_mask = PLLD_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,

	.sdm_en_mask = PLLD_MISC0_EN_SDM,
	.sdm_ctrl_reg_idx = PLL_MISC0_IDX,
};

static struct clk_pll_div_layout plld_div_layout = {
	.mdiv_shift = 0,
	.mdiv_mask = 0xff,
	.ndiv_shift = 11,
	.ndiv_mask = 0xff << 11,
	.pdiv_shift = 20,
	.pdiv_mask = 0x7 << 20,
	.pdiv_to_p = pll_expo_pdiv_to_p,
	.pdiv_max = PLL_EXPO_PDIV_MAX,

	.sdm_din_shift = 0,
	.sdm_din_mask = 0xffff,
	.sdm_din_reg_idx = PLL_MISC0_IDX,
};

static struct clk tegra_pll_d = {
	.name      = "pll_d",
	.clk_id    = TEGRA210_CLK_ID_PLL_D,
	.flags     = PLLD,
	.ops       = &tegra_plld_ops,
	.reg       = 0xd0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 1500000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 800000000,
		.cf_min    = 12000000,
		.cf_max    = 38400000,
		.vco_min   = 750000000,
		.vco_max   = 1500000000,
		.mdiv_default = 1,
		.freq_table = tegra_pll_d_freq_table,
		.lock_delay = 300,
		.misc0 = 0xdc - 0xd0,
		.misc1 = 0xd8 - 0xd0,
		.controls = &plld_controls,
		.div_layout = &plld_div_layout,
		.round_p_to_pdiv = pll_expo_p_to_pdiv,
		.set_defaults = plld_set_defaults,
	},
};

static struct clk tegra_pll_d_out0 = {
	.name      = "pll_d_out0",
	.clk_id    = TEGRA210_CLK_ID_PLL_D_OUT0,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_2 | PLLD,
	.parent    = &tegra_pll_d,
	.max_rate  = 750000000,
};

static struct clk_pll_freq_table tegra_pll_d2_freq_table[] = {
	{ 12000000, 594000000,  99, 1, 2, 0, 0xf000},
	{ 13000000, 594000000,  91, 1, 2, 0, 0xfc4f},	/* actual: 594000183 */
	{ 38400000, 594000000,  30, 1, 2, 0, 0x0e00},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls plld2_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLDSS_BASE_IDDQ,
	.iddq_reg_idx = PLL_BASE_IDX,
	.lock_mask = PLLDSS_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,

	.sdm_en_mask = PLLDSS_MISC1_CFG_EN_SDM,
	.ssc_en_mask = PLLDSS_MISC1_CFG_EN_SSC,
	.sdm_ctrl_reg_idx = PLL_MISC1_IDX,
};

static struct clk_pll_div_layout plldss_div_layout = {
	.mdiv_shift = 0,
	.mdiv_mask = 0xff,
	.ndiv_shift = 8,
	.ndiv_mask = 0xff << 8,
	.pdiv_shift = 19,
	.pdiv_mask = 0x1f << 19,
	.pdiv_to_p = pll_qlin_pdiv_to_p,
	.pdiv_max = PLL_QLIN_PDIV_MAX,

	.sdm_din_shift = 0,
	.sdm_din_mask = 0xffff,
	.sdm_din_reg_idx = PLL_MISC3_IDX,
};

static struct clk tegra_pll_d2 = {
	.name      = "pll_d2",
	.clk_id    = TEGRA210_CLK_ID_PLL_D2,
	.ops       = &tegra_plld2_ops,
	.reg       = 0x4b8,
	.parent    = &tegra_pll_ref,	/* s/w policy, always tegra_pll_ref */
	.max_rate  = 1500000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 800000000,
		.cf_min    = 12000000,
		.cf_max    = 38400000,
		.vco_min   = 750000000,
		.vco_max   = 1500000000,
		.mdiv_default = 1,
		.freq_table = tegra_pll_d2_freq_table,
		.lock_delay = 300,
		.misc0 = 0x4bc - 0x4b8,
		.misc1 = 0x570 - 0x4b8,
		.misc2 = 0x574 - 0x4b8,
		.misc3 = 0x578 - 0x4b8,
		.controls = &plld2_controls,
		.div_layout = &plldss_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.set_defaults = plld2_set_defaults,
	},
};

static struct clk_pll_freq_table tegra_pll_dp_freq_table[] = {
	{ 12000000, 270000000,  90, 1, 4, 0, 0xf000},
	{ 13000000, 270000000,  83, 1, 4, 0, 0xf000},	/* actual: 269.8 MHz */
	{ 38400000, 270000000,  28, 1, 4, 0, 0xf400},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls plldp_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLDSS_BASE_IDDQ,
	.iddq_reg_idx = PLL_BASE_IDX,
	.lock_mask = PLLDSS_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,

	.sdm_en_mask = PLLDSS_MISC1_CFG_EN_SDM,
	.ssc_en_mask = PLLDSS_MISC1_CFG_EN_SSC,
	.sdm_ctrl_reg_idx = PLL_MISC1_IDX,
};

static struct clk tegra_pll_dp = {
	.name      = "pll_dp",
	.clk_id    = TEGRA210_CLK_ID_PLL_DP,
	.flags     = PLL_FIXED,
	.ops       = &tegra_plldp_ops,
	.reg       = 0x590,
	.parent    = &tegra_pll_ref,	/* s/w policy, always tegra_pll_ref */
	.max_rate  = 1500000000,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 800000000,
		.cf_min    = 12000000,
		.cf_max    = 38400000,
		.vco_min   = 750000000,
		.vco_max   = 1500000000,
		.mdiv_default = 1,
		.freq_table = tegra_pll_dp_freq_table,
		.lock_delay = 300,
		.misc0 = 0x594 - 0x590,
		.misc1 = 0x598 - 0x590,
		.misc2 = 0x59c - 0x590,
		.misc3 = 0x5a0 - 0x590,
		.controls = &plldp_controls,
		.div_layout = &plldss_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.set_defaults = plldp_set_defaults,
		.fixed_rate = 270000000,
	},
};

static struct clk_pll_freq_table tegra_pllc4_vco_freq_table[] = {
	{ 12000000, 600000000,  50, 1, 1},
	{ 13000000, 600000000,  46, 1, 1},	/* actual: 598.0 MHz */
	{ 38400000, 600000000,  62, 4, 1},	/* actual: 595.2 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls pllc4_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLDSS_BASE_IDDQ,
	.iddq_reg_idx = PLL_BASE_IDX,
	.lock_mask = PLLDSS_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,
};

static struct clk_pll_div_layout pllc4_div_layout = {
	.mdiv_shift = 0,
	.mdiv_mask = 0xff,
	.ndiv_shift = 8,
	.ndiv_mask = 0xff << 8,
	.pdiv_shift = 19,
	.pdiv_mask = 0x1f << 19,
	.pdiv_to_p = pll_qlin_pdiv_to_p,
	.pdiv_max = PLL_QLIN_PDIV_MAX,
};

static struct clk tegra_pll_c4_vco = {
	.name      = "pll_c4",
	.clk_id    = TEGRA210_CLK_ID_PLL_C4,
	.ops       = &tegra_pllc4_vco_ops,
	.reg       = 0x5a4,
	.parent    = &tegra_pll_ref,	/* s/w policy, always tegra_pll_ref */
	.max_rate  = 1080000000,
	.u.pll = {
		.input_min = 9600000,
		.input_max = 800000000,
		.cf_min    = 9600000,
		.cf_max    = 19200000,
		.vco_min   = 500000000,
		.vco_max   = 1080000000,
		.freq_table = tegra_pllc4_vco_freq_table,
		.lock_delay = 300,
		.misc0 = 0x5a8 - 0x5a4,
		.controls = &pllc4_controls,
		.div_layout = &pllc4_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.set_defaults = pllc4_set_defaults,
		.vco_out = true,
	},
};

static struct clk tegra_pll_c4_out0 = {
	.name      = "pll_c4_out0",
	.clk_id    = TEGRA210_CLK_ID_PLL_C4_OUT0,
	.ops       = &tegra_pll_out_ops,
	.parent    = &tegra_pll_c4_vco,
};

static struct clk tegra_pll_c4_out1 = {
	.name      = "pll_c4_out1",
	.clk_id    = TEGRA210_CLK_ID_PLL_C4_OUT1,
	.ops       = &tegra_pll_out_fixed_ops,
	.parent    = &tegra_pll_c4_vco,
	.mul       = 1,
	.div       = 3,
};

static struct clk tegra_pll_c4_out2 = {
	.name      = "pll_c4_out2",
	.clk_id    = TEGRA210_CLK_ID_PLL_C4_OUT2,
	.ops       = &tegra_pll_out_fixed_ops,
	.parent    = &tegra_pll_c4_vco,
	.mul       = 1,
	.div       = 5,
};

static struct clk tegra_pll_c4_out3 = {
	.name      = "pll_c4_out3",
	.clk_id    = TEGRA210_CLK_ID_PLL_C4_OUT3,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71,
	.parent    = &tegra_pll_c4_out0,
	.reg       = 0x5e4,
	.reg_shift = 0,
	.max_rate  = 1080000000,
};

static struct clk_pll_freq_table tegra_pllre_vco_freq_table[] = {
	{ 12000000, 672000000,  56, 1, 1},
	{ 13000000, 672000000,  51, 1, 1},	/* actual: 663.0 MHz */
	{ 38400000, 672000000,  70, 4, 1},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls pllre_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLRE_MISC0_IDDQ,
	.iddq_reg_idx = PLL_MISC0_IDX,
	.lock_mask = PLLRE_MISC0_LOCK,
	.lock_reg_idx = PLL_MISC0_IDX,
};

static struct clk_pll_div_layout pllre_div_layout = {
	.mdiv_shift = 0,
	.mdiv_mask = 0xff,
	.ndiv_shift = 8,
	.ndiv_mask = 0xff << 8,
	.pdiv_shift = 16,
	.pdiv_mask = 0x1f << 16,
	.pdiv_to_p = pll_qlin_pdiv_to_p,
	.pdiv_max = PLL_QLIN_PDIV_MAX,
};

static struct clk tegra_pll_re_vco = {
	.name      = "pll_re_vco",
	.clk_id    = TEGRA210_CLK_ID_PLL_RE,
	.ops       = &tegra_pllre_vco_ops,
	.reg       = 0x4c4,
	.parent    = &tegra_pll_ref,
	.max_rate  = 700000000,
	.u.pll = {
		.input_min = 9600000,
		.input_max = 800000000,
		.cf_min    = 9600000,
		.cf_max    = 19200000,
		.vco_min   = 350000000,
		.vco_max   = 700000000,
		.freq_table = tegra_pllre_vco_freq_table,
		.lock_delay = 300,
		.misc0 = 0x4c8 - 0x4c4,
		.controls = &pllre_controls,
		.div_layout = &pllre_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.set_defaults = pllre_set_defaults,
		.vco_out = true,
	},
};

static struct clk tegra_pll_re_out = {
	.name      = "pll_re_out",
	.clk_id    = TEGRA210_CLK_ID_PLL_RE_OUT,
	.ops       = &tegra_pll_out_ops,
	.parent    = &tegra_pll_re_vco,
};

static struct clk tegra_pll_re_out1 = {
	.name      = "pll_re_out1",
	.clk_id    = TEGRA210_CLK_ID_PLL_RE_OUT1,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71,
	.parent    = &tegra_pll_re_vco,
	.reg       = 0x4cc,
	.reg_shift = 0,
	.max_rate  = 700000000,
};

static struct clk_pll_freq_table tegra_pll_x_freq_table[] = {
	/* 1 GHz */
	{ 12000000, 1000000000, 166, 1, 2},	/* actual: 996.0 MHz */
	{ 13000000, 1000000000, 153, 1, 2},	/* actual: 994.0 MHz */
	{ 38400000, 1000000000, 156, 3, 2},	/* actual: 998.4 MHz */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls pllx_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLX_MISC3_IDDQ,
	.iddq_reg_idx = PLL_MISC3_IDX,
	.lock_mask = PLLX_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,

	.dramp_en_mask = PLLX_MISC2_EN_DYNRAMP,
	.dramp_done_mask = PLLX_MISC2_DYNRAMP_DONE,
	.dramp_ctrl_reg_idx = PLL_MISC2_IDX,
};

static struct clk_pll_div_layout pllx_div_layout = {
	.mdiv_shift = 0,
	.mdiv_mask = 0xff,
	.ndiv_shift = 8,
	.ndiv_mask = 0xff << 8,
	.pdiv_shift = 20,
	.pdiv_mask = 0x1f << 20,
	.pdiv_to_p = pll_qlin_pdiv_to_p,
	.pdiv_max = PLL_QLIN_PDIV_MAX,

	.ndiv_new_shift = PLLX_MISC2_NDIV_NEW_SHIFT,
	.ndiv_new_reg_idx = PLL_MISC2_IDX,
};

static struct clk tegra_pll_x = {
	.name      = "pll_x",
	.clk_id    = TEGRA210_CLK_ID_PLL_X,
	.flags     = PLLX,
	.ops       = &tegra_pllx_ops,
	.reg       = 0xe0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 3000000000UL,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 800000000,
		.cf_min    = 12000000,
		.cf_max    = 38400000,
		.vco_min   = 1350000000,
		.vco_max   = 3000000000UL,
		.mdiv_default = 2,
		.freq_table = tegra_pll_x_freq_table,
		.lock_delay = 300,
		.misc0 = 0xe4 - 0xe0,
		.misc1 = 0x510 - 0xe0,
		.misc2 = 0x514 - 0xe0,
		.misc3 = 0x518 - 0xe0,
		.misc4 = 0x5f0 - 0xe0,
		.misc5 = 0x5f4 - 0xe0,
		.controls = &pllx_controls,
		.div_layout = &pllx_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
#if PLLX_USE_DYN_RAMP
		.dyn_ramp = pllx_dyn_ramp,
#endif
		.set_defaults = pllx_set_defaults,
	},
};

static struct clk_pll_freq_table tegra_pll_m_freq_table[] = {
	{ 12000000,  800000000,  66, 1, 1},	/* actual: 792.0 MHz */
	{ 13000000,  800000000,  61, 1, 1},	/* actual: 793.0 MHz */
	{ 38400000,  297600000,  93, 4, 3},
	{ 38400000,  400000000, 125, 4, 3},
	{ 38400000,  532800000, 111, 4, 2},
	{ 38400000,  665600000, 104, 3, 2},
	{ 38400000,  800000000, 125, 3, 2},
	{ 38400000,  931200000,  97, 4, 1},
	{ 38400000, 1065600000, 111, 4, 1},
	{ 38400000, 1200000000, 125, 4, 1},
	{ 38400000, 1331200000, 104, 3, 1},
	{ 38400000, 1459200000,  76, 2, 1},
	{ 38400000, 1600000000, 125, 3, 1},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls pllm_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.iddq_mask = PLLM_MISC0_IDDQ,
	.iddq_reg_idx = PLL_MISC0_IDX,
	.lock_mask = PLLM_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,
};

static struct clk_pll_div_layout pllm_div_layout = {
	.mdiv_shift = 0,
	.mdiv_mask = 0xff,
	.ndiv_shift = 8,
	.ndiv_mask = 0xff << 8,
	.pdiv_shift = 20,
	.pdiv_mask = 0x1f << 20,
	.pdiv_to_p = pll_qlin_pdiv_to_p,
	.pdiv_max = PLL_QLIN_PDIV_MAX,
};

static struct clk tegra_pll_m = {
	.name      = "pll_m",
	.clk_id    = TEGRA210_CLK_ID_PLL_M,
	.flags     = PLLM,
	.ops       = &tegra_pllm_ops,
	.reg       = 0x90,
	.parent    = &tegra_clk_osc,
	.max_rate  = 1866000000,
	.u.pll = {
		.input_min = 9600000,
		.input_max = 500000000,
		.cf_min    = 9600000,
		.cf_max    = 19200000,
		.vco_min   = 800000000,
		.vco_max   = 1866000000,
		.freq_table = tegra_pll_m_freq_table,
		.lock_delay = 300,
		.misc0 = 0x9c - 0x90,
		.misc1 = 0x98 - 0x90,
		.controls = &pllm_controls,
		.div_layout = &pllm_div_layout,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
	},
};

static struct clk_pll_controls pllmb_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.iddq_mask = PLLMB_MISC0_IDDQ,
	.iddq_reg_idx = PLL_MISC0_IDX,
	.lock_mask = PLLMB_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,
};

static struct clk tegra_pll_mb = {
	.name      = "pll_mb",
	.clk_id    = TEGRA210_CLK_ID_PLL_MB,
	.ops       = &tegra_pllmb_ops,
	.reg       = 0x5e8,
	.parent    = &tegra_clk_osc,
	.max_rate  = 1866000000,
	.u.pll = {
		.input_min = 9600000,
		.input_max = 500000000,
		.cf_min    = 9600000,
		.cf_max    = 19200000,
		.vco_min   = 800000000,
		.vco_max   = 1866000000,
		.freq_table = tegra_pll_m_freq_table,
		.lock_delay = 300,
		.misc0 = 0x5ec - 0x5e8,
		.controls = &pllmb_controls,
		.div_layout = &pllm_div_layout,		/* same, re-used */
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.set_defaults = pllmb_set_defaults,
	},
};

static struct clk_pll_freq_table tegra_pll_p_vco_freq_table[] = {
	{ 12000000, 408000000,  34,  1, 1},
	{ 13000000, 408000000, 408, 13, 1}, /* cf = 1MHz - only on FPGA */
	{ 38400000, 408000000,  85,  8, 1}, /* cf = 4.8MHz, allowed exception */
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls pllp_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLP_MISC0_IDDQ,
	.iddq_reg_idx = PLL_MISC0_IDX,
	.lock_mask = PLLP_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,
};

static struct clk tegra_pll_p = {
	.name      = "pll_p",
	.clk_id    = TEGRA210_CLK_ID_PLL_P,
	.flags     = ENABLE_ON_INIT | PLL_FIXED,
	.ops       = &tegra_pllp_vco_ops,
	.reg       = 0xa0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 700000000,
	.u.pll = {
		.input_min = 9600000,
		.input_max = 800000000,
		.cf_min    = 9600000,
		.cf_max    = 19200000,
		.vco_min   = 350000000,
		.vco_max   = 700000000,
		.freq_table = tegra_pll_p_vco_freq_table,
		.lock_delay = 300,
		.misc0 = 0xac - 0xa0,
		.misc1 = 0x680 - 0xa0,
		.controls = &pllp_controls,
		.div_layout = &pllre_div_layout,	/* same, re-used */
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.set_defaults = pllp_set_defaults,
		.vco_out = true,
		.fixed_rate = 408000000,
	},
};

static struct clk_mux_sel mux_clk_osc[] = {
	{ .input = &tegra_clk_osc, .value = 0},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp[] = {
	{ .input = &tegra_pll_p, .value = 0},
	{ NULL, 0},
};

static struct clk tegra_pll_p_out_adsp = {
	.name      = "pll_p_out_adsp",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT_ADSP,
	.ops       = &tegra_periph_clk_ops,
	.inputs    = mux_pllp,
	.flags     = PERIPH_NO_RESET,
	.max_rate  = 700000000,
	.u.periph = {
		.clk_num   = 187,
	},
};

static struct clk tegra_pll_p_out_cpu = {
	.name      = "pll_p_out_cpu",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT_CPU,
	.ops       = &tegra_periph_clk_ops,
	.inputs    = mux_pllp,
	.flags     = PERIPH_NO_RESET,
	.max_rate  = 700000000,
	.u.periph = {
		.clk_num   = 223,
	},
};

static struct clk tegra_pll_p_out_hsio = {
	.name      = "pll_p_out_hsio",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT_HSIO,
	.ops       = &tegra_pll_div_ops,
	.parent    = &tegra_pll_p,
	.reg       = 0x680,
	.reg_shift = PLLP_MISC1_HSIO_EN_SHIFT,
};

static struct clk tegra_pll_p_out_xusb = {
	.name      = "pll_p_out_xusb",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT_XUSB,
	.ops       = &tegra_pll_div_ops,
	.parent    = &tegra_pll_p_out_hsio,
	.reg       = 0x680,
	.reg_shift = PLLP_MISC1_XUSB_EN_SHIFT,
};

static struct clk tegra_pll_p_out2 = {
	.name      = "pll_p_out2",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT2,
	.flags     = PLL_FIXED,
	.ops       = &tegra_pllp_out_ops,
	.parent    = &tegra_pll_p,
	.u.pll = {
		.fixed_rate = 204000000,
	},
};

static struct clk tegra_pll_p_out3 = {
	.name      = "pll_p_out3",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT3,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_p,
	.reg       = 0xa8,
	.reg_shift = 0,
	.max_rate  = 408000000,
};

static struct clk tegra_pll_p_out4 = {
	.name      = "pll_p_out4",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT4,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_INT | DIV_U71_FIXED,
	.parent    = &tegra_pll_p_out_cpu,
	.reg       = 0xa8,
	.reg_shift = 16,
	.max_rate  = 408000000,
};

static struct clk tegra_pll_p_out5 = {
	.name      = "pll_p_out5",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT5,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_p,
	.reg       = 0x67c,
	.reg_shift = 16,
	.max_rate  = 408000000,
};

static struct clk tegra_pll_p_out_sor = {
	.name      = "sor_safe",
	.clk_id    = TEGRA210_CLK_ID_PLL_P_OUT_SOR,
	.ops       = &tegra_periph_clk_ops,
	.inputs    = mux_pllp,
	.flags     = PERIPH_NO_RESET,
	.max_rate  = 24000000,
	.mul       = 1,
	.div       = 17,
	.u.periph = {
		.clk_num   = 222,
	},
};

static struct clk_pll_freq_table tegra_pll_u_vco_freq_table[] = {
	{ 12000000, 480000000, 40, 1, 1},
	{ 13000000, 480000000, 36, 1, 1},	/* actual: 468.0 MHz */
	{ 38400000, 480000000, 25, 2, 1},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_pll_controls pllu_controls = {
	.enable_mask = PLL_BASE_ENABLE,
	.bypass_mask = PLL_BASE_BYPASS,
	.iddq_mask = PLLU_MISC0_IDDQ,
	.iddq_reg_idx = PLL_MISC0_IDX,
	.lock_mask = PLLU_BASE_LOCK,
	.lock_reg_idx = PLL_BASE_IDX,
};

static struct clk tegra_pll_u_vco = {
	.name      = "pll_u",
	.clk_id    = TEGRA210_CLK_ID_PLL_U,
	.flags     = PLLU | PLL_FIXED,
	.ops       = &tegra_pllu_vco_ops,
	.reg       = 0xc0,
	.parent    = &tegra_pll_ref,
	.max_rate  = 700000000,
	.u.pll = {
		.input_min = 9600000,
		.input_max = 800000000,
		.cf_min    = 9600000,
		.cf_max    = 19200000,
		.vco_min   = 350000000,
		.vco_max   = 700000000,
		.freq_table = tegra_pll_u_vco_freq_table,
		.lock_delay = 300,
		.misc0 = 0xcc - 0xc0,
		.misc1 = 0xc8 - 0xc0,
		.controls = &pllu_controls,
		.div_layout = &pllre_div_layout,	/* same, re-used */
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
		.set_defaults = pllu_set_defaults,
		.vco_out = true,
		.fixed_rate = 480000000,
	},
};

static struct clk tegra_pll_u_out = {
	.name      = "pll_u_out",
	.clk_id    = TEGRA210_CLK_ID_PLL_U_OUT,
	.flags     = PLLU | PLL_FIXED,
	.ops       = &tegra_pllu_out_ops,
	.parent    = &tegra_pll_u_vco,
	.u.pll = {
		.fixed_rate = 240000000,
	},
};

static struct clk tegra_pll_u_out1 = {
	.name      = "pll_u_out1",
	.clk_id    = TEGRA210_CLK_ID_PLL_U_OUT1,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_u_out,
	.reg       = 0xc4,
	.reg_shift = 0,
	.max_rate  = 480000000,
	.u.pll_div = {
		.default_rate = 48000000,
	},
};

static struct clk tegra_pll_u_out2 = {
	.name      = "pll_u_out2",
	.clk_id    = TEGRA210_CLK_ID_PLL_U_OUT2,
	.ops       = &tegra_pll_div_ops,
	.flags     = DIV_U71 | DIV_U71_FIXED,
	.parent    = &tegra_pll_u_out,
	.reg       = 0xc4,
	.reg_shift = 16,
	.max_rate  = 480000000,
	.u.pll_div = {
		.default_rate = 60000000,
	},
};

static struct clk tegra_pll_u_480M = {
	.name      = "pll_u_480M",
	.clk_id    = TEGRA210_CLK_ID_PLL_U_480M,
	.flags     = PLLU,
	.ops       = &tegra_pll_div_ops,
	.reg       = 0xc0,
	.reg_shift = 22,
	.parent    = &tegra_pll_u_vco,
	.mul       = 1,
	.div       = 1,
};

static struct clk tegra_pll_u_60M = {
	.name      = "pll_u_60M",
	.clk_id    = TEGRA210_CLK_ID_PLL_U_60M,
	.flags     = PLLU,
	.ops       = &tegra_pll_div_ops,
	.reg       = 0xc0,
	.reg_shift = 23,
	.parent    = &tegra_pll_u_out2,
	.mul       = 1,
	.div       = 1,
};

static struct clk tegra_pll_u_48M = {
	.name      = "pll_u_48M",
	.clk_id    = TEGRA210_CLK_ID_PLL_U_48M,
	.flags     = PLLU,
	.ops       = &tegra_pll_div_ops,
	.reg       = 0xc0,
	.reg_shift = 25,
	.parent    = &tegra_pll_u_out1,
	.mul       = 1,
	.div       = 1,
};

static struct clk tegra_dfll_cpu = {
	.name      = "dfll_cpu",
	.clk_id    = TEGRA210_CLK_ID_DFLL_CPU,
	.flags     = DFLL,
	.ops       = &tegra_dfll_ops,
	.reg	   = 0x2f4,
	.max_rate  = 3000000000UL,
};

static struct clk_pll_freq_table tegra_pll_e_freq_table[] = {
	{ 672000000, 100000000, 125, 42,  20,},
	{ 624000000, 100000000, 125, 39,  20,},
	{ 336000000, 100000000, 125, 21,  20,},
	{ 312000000, 100000000, 200, 26,  24,},
	{ 38400000,  100000000, 125, 2,   24,},
	{ 13000000,  100000000, 184, 1,   24,},	/* actual: 99.6MHz (FPGA) */
	{ 12000000,  100000000, 200, 1,   24,},
	{ 0, 0, 0, 0, 0, 0 },
};

static struct clk_mux_sel mux_pll_e_inputs[] = {
	{ .input = &tegra_pll_ref,	.value = 0},
	{ .input = &tegra_pll_p,	.value = 1},
	{ .input = &tegra_pll_re_vco,	.value = 2},
	{ .input = &tegra_pll_re_vco,	.value = 3},
	{ NULL, 0},
};

static struct clk tegra_pll_e = {
	.name      = "pll_e",
	.clk_id    = TEGRA210_CLK_ID_PLL_E,
	.flags     = PLL_FIXED,
	.ops       = &tegra_plle_ops,
	.reg       = 0xe8,
	.max_rate  = 100000000,
	.inputs    = mux_pll_e_inputs,
	.u.pll = {
		.input_min = 12000000,
		.input_max = 800000000,
		.cf_min    = 12000000,
		.cf_max    = 38400000,
		.vco_min   = 1600000000,
		.vco_max   = 2500000000U,
		.freq_table = tegra_pll_e_freq_table,
		.lock_delay = 300,
		.fixed_rate = 100000000,
		.misc0 = 0xec - 0xe8,
		.round_p_to_pdiv = pll_qlin_p_to_pdiv,
	},
};

static struct clk tegra_pll_e_hw = {
	.name      = "pll_e_hw",
	.parent    = &tegra_pll_e,
	.ops       = &tegra_plle_hw_ops,
	.reg       = PLLE_AUX,
	.max_rate  = 100000000,
};

static struct clk tegra_pll_e_gate = {
	.name      = "plle_gate",
	.clk_id    = TEGRA210_CLK_ID_PLL_E_GATE,
	.parent    = &tegra_pll_e,
	.ops       = &tegra_cml_clk_ops,
	.reg       = 0xec,
        .max_rate  = 100000000,
	.u.periph  = {
		.clk_num = 15,
	},
};

static struct clk tegra_cml1_clk = {
	.name      = "cml1",
	.clk_id    = TEGRA210_CLK_ID_PLL_E_CML1,
	.parent    = &tegra_pll_e,
	.ops       = &tegra_cml_clk_ops,
	.reg       = PLLE_AUX,
	.max_rate  = 100000000,
	.u.periph  = {
		.clk_num   = 1,
	},
};

static struct clk tegra_pciex_clk = {
	.name      = "pciex",
	.clk_id    = TEGRA210_CLK_ID_PCIEX,
	.parent    = &tegra_pll_e,
	.ops       = &tegra_pciex_clk_ops,
	.max_rate  = 500000000,
	.u.periph  = {
		.clk_num   = 74,
	},
};

/* Audio sync clocks */
#define SYNC_SOURCE(_id, _dev, _clk_id)			\
	{						\
		.name      = #_id "_sync",		\
		.clk_id    = TEGRA210_CLK_ID_##_clk_id##_SYNC, \
		.lookup    = {				\
			.dev_id    = #_dev ,		\
			.con_id    = "ext_audio_sync",	\
		},					\
		.rate      = 24000000,			\
		.max_rate  = 24576000,			\
		.ops       = &tegra_sync_source_ops	\
	}
static struct clk tegra_sync_source_list[] = {
	SYNC_SOURCE(spdif_in, tegra210-spdif, SPDIF_IN),
	SYNC_SOURCE(i2s0, tegra210-i2s.0, I2S0),
	SYNC_SOURCE(i2s1, tegra210-i2s.1, I2S1),
	SYNC_SOURCE(i2s2, tegra210-i2s.2, I2S2),
	SYNC_SOURCE(i2s3, tegra210-i2s.3, I2S3),
	SYNC_SOURCE(i2s4, tegra210-i2s.4, I2S4),
	SYNC_SOURCE(vimclk, vimclk, VIMCLK),
};

static struct clk_mux_sel mux_d_audio_clk[] = {
	{ .input = &tegra_pll_a_out0,		.value = 0},
	{ .input = &tegra_pll_p,		.value = 0x8000},
	{ .input = &tegra_clk_m,		.value = 0xc000},
	{ .input = &tegra_sync_source_list[0],	.value = 0xE000},
	{ .input = &tegra_sync_source_list[1],	.value = 0xE001},
	{ .input = &tegra_sync_source_list[2],	.value = 0xE002},
	{ .input = &tegra_sync_source_list[3],	.value = 0xE003},
	{ .input = &tegra_sync_source_list[4],	.value = 0xE004},
	{ .input = &tegra_sync_source_list[5],	.value = 0xE005},
	{ .input = &tegra_pll_a_out0,		.value = 0xE006},
	{ .input = &tegra_sync_source_list[6],	.value = 0xE007},
	{ NULL, 0 }
};

static struct clk_mux_sel mux_audio_sync_clk[] =
{
	{ .input = &tegra_sync_source_list[0],	.value = 0},
	{ .input = &tegra_sync_source_list[1],	.value = 1},
	{ .input = &tegra_sync_source_list[2],	.value = 2},
	{ .input = &tegra_sync_source_list[3],	.value = 3},
	{ .input = &tegra_sync_source_list[4],	.value = 4},
	{ .input = &tegra_sync_source_list[5],	.value = 5},
	{ .input = &tegra_pll_a_out0,		.value = 6},
	{ .input = &tegra_sync_source_list[6],	.value = 7},
	{ NULL, 0 }
};

#define AUDIO_SYNC_CLK(_id, _dev, _index, _clk_id)	\
	{						\
		.name      = #_id,			\
		.clk_id    = TEGRA210_CLK_ID_##_clk_id, \
		.lookup    = {				\
			.dev_id    = #_dev,		\
			.con_id    = "audio_sync",	\
		},					\
		.inputs    = mux_audio_sync_clk,	\
		.reg       = 0x4A0 + (_index) * 4,	\
		.max_rate  = 24576000,			\
		.ops       = &tegra_audio_sync_clk_ops	\
	}
static struct clk tegra_clk_audio_list[] = {
	AUDIO_SYNC_CLK(audio0, tegra210-i2s.0, 0, AUDIO0),
	AUDIO_SYNC_CLK(audio1, tegra210-i2s.1, 1, AUDIO1),
	AUDIO_SYNC_CLK(audio2, tegra210-i2s.2, 2, AUDIO2),
	AUDIO_SYNC_CLK(audio3, tegra210-i2s.3, 3, AUDIO3),
	AUDIO_SYNC_CLK(audio4, tegra210-i2s.4, 4, AUDIO4),
	AUDIO_SYNC_CLK(audio, tegra210-spdif,  5, AUDIO),	/* SPDIF */
};

#define AUDIO_SYNC_2X_CLK(_id, _dev, _index, _clk_id)		\
	{							\
		.name      = #_id "_2x",			\
		.clk_id    = TEGRA210_CLK_ID_##_clk_id##_2x,	\
		.lookup    = {					\
			.dev_id    = #_dev,			\
			.con_id    = "audio_sync_2x"		\
		},						\
		.flags     = PERIPH_NO_RESET,			\
		.max_rate  = 49152000,				\
		.ops       = &tegra_clk_double_ops,		\
		.reg       = 0x49C,				\
		.reg_shift = 24 + (_index),			\
		.parent    = &tegra_clk_audio_list[(_index)],	\
		.u.periph = {					\
			.clk_num = 113 + (_index),		\
		},						\
	}
static struct clk tegra_clk_audio_2x_list[] = {
	AUDIO_SYNC_2X_CLK(audio, tegra210-spdif, 5, AUDIO),	/* SPDIF */
};

#define MUX_I2S_SPDIF(_id, _index)					\
static struct clk_mux_sel mux_pllaout0_##_id##_pllp_clkm[] = {		\
	{.input = &tegra_pll_a_out0, .value = 0},			\
	{.input = &tegra_clk_audio_list[(_index)], .value = 2},		\
	{.input = &tegra_pll_p, .value = 4},				\
	{.input = &tegra_clk_m, .value = 6},				\
	{ NULL, 0},							\
}
MUX_I2S_SPDIF(audio0, 0);
MUX_I2S_SPDIF(audio1, 1);
MUX_I2S_SPDIF(audio2, 2);
MUX_I2S_SPDIF(audio3, 3);
MUX_I2S_SPDIF(audio4, 4);

/* SPDIF */
static struct clk_mux_sel mux_pllaout0_audio_2x_pllp_clkm[] = {
	{.input = &tegra_pll_a_out0, .value = 0},
	{.input = &tegra_clk_audio_2x_list[0], .value = 2},
	{.input = &tegra_pll_p, .value = 4},
	{.input = &tegra_clk_m, .value = 6},
	{ NULL, 0},
};

/* Audio sync dmic clocks */
#define AUDIO_SYNC_DMIC_CLK(_id, _dev, _reg, _clk_id)	\
	{						\
		.name      = #_id "_dmic",		\
		.clk_id    = TEGRA210_CLK_ID_##_clk_id##_DMIC, \
		.lookup    = {				\
			.dev_id    = #_dev,		\
			.con_id    = "audio_sync_dmic",	\
		},					\
		.inputs    = mux_audio_sync_clk,	\
		.reg       = _reg,			\
		.max_rate  = 24576000,			\
		.ops       = &tegra_audio_sync_clk_ops	\
	}

static struct clk tegra_clk_audio_dmic_list[] = {
	AUDIO_SYNC_DMIC_CLK(audio0, tegra210-i2s.0, 0x560, AUDIO0),
	AUDIO_SYNC_DMIC_CLK(audio1, tegra210-i2s.1, 0x564, AUDIO1),
	AUDIO_SYNC_DMIC_CLK(audio2, tegra210-i2s.2, 0x6b8, AUDIO2),
};

#define MUX_AUDIO_DMIC(_id, _index)					\
static struct clk_mux_sel mux_pllaout0_##_id##_dmic_pllp_clkm[] = {	\
	{.input = &tegra_pll_a_out0, .value = 0},			\
	{.input = &tegra_clk_audio_dmic_list[(_index)], .value = 1},		\
	{.input = &tegra_pll_p, .value = 2},				\
	{.input = &tegra_clk_m, .value = 3},				\
	{ NULL, 0},							\
}
MUX_AUDIO_DMIC(audio0, 0);
MUX_AUDIO_DMIC(audio1, 1);
MUX_AUDIO_DMIC(audio2, 2);

/* External clock outputs (through PMC) */
#define MUX_EXTERN_OUT(_id)						\
static struct clk_mux_sel mux_clkm_clkm2_clkm4_extern##_id[] = {	\
	{.input = &tegra_clk_m,		.value = 0},			\
	{.input = &tegra_clk_m_div2,	.value = 1},			\
	{.input = &tegra_clk_m_div4,	.value = 2},			\
	{.input = NULL,			.value = 3}, /* placeholder */	\
	{ NULL, 0},							\
}
MUX_EXTERN_OUT(1);
MUX_EXTERN_OUT(2);
MUX_EXTERN_OUT(3);

static struct clk_mux_sel *mux_extern_out_list[] = {
	mux_clkm_clkm2_clkm4_extern1,
	mux_clkm_clkm2_clkm4_extern2,
	mux_clkm_clkm2_clkm4_extern3,
};

#define CLK_OUT_CLK(_id, _max_rate)				\
	{							\
		.name      = "clk_out_" #_id,			\
		.clk_id    = TEGRA210_CLK_ID_CLK_OUT_##_id,	\
		.lookup    = {					\
			.dev_id    = "clk_out_" #_id,		\
			.con_id	   = "extern" #_id,		\
		},						\
		.ops       = &tegra_clk_out_ops,		\
		.reg       = 0x1a8,				\
		.inputs    = mux_clkm_clkm2_clkm4_extern##_id,	\
		.flags     = MUX_CLK_OUT,			\
		.max_rate  = _max_rate,				\
		.u.periph = {					\
			.clk_num   = (_id - 1) * 8 + 2,		\
		},						\
	}
static struct clk tegra_clk_out_list[] = {
	CLK_OUT_CLK(1, 40800000),
	CLK_OUT_CLK(2, 40800000),
	CLK_OUT_CLK(3, 40800000),
};

/* called after peripheral external clocks are initialized */
static void init_clk_out_mux(void)
{
	int i;
	struct clk *c;

	/* output clock con_id is the name of peripheral
	   external clock connected to input 3 of the output mux */
	for (i = 0; i < ARRAY_SIZE(tegra_clk_out_list); i++) {
		c = tegra_get_clock_by_name(
			tegra_clk_out_list[i].lookup.con_id);
		if (!c)
			pr_err("%s: could not find clk %s\n", __func__,
			       tegra_clk_out_list[i].lookup.con_id);
		mux_extern_out_list[i][3].input = c;
	}
}

/* Peripheral muxes */
static struct clk_mux_sel mux_cclk_g[] = {
	{ .input = &tegra_clk_m,	.value = 0},
	{ .input = &tegra_clk_32k,	.value = 2},
	{ .input = &tegra_pll_p_out_cpu, .value = 4},
	{ .input = &tegra_pll_p_out4,	.value = 5},
	{ .input = &tegra_pll_x,	.value = 8},
	{ .input = &tegra_dfll_cpu,	.value = 15},
	{ NULL, 0},
};

static struct clk_mux_sel mux_cclk_lp[] = {
	{ .input = &tegra_clk_m,	.value = 0},
	{ .input = &tegra_clk_32k,	.value = 2},
	{ .input = &tegra_pll_p_out_cpu, .value = 4},
	{ .input = &tegra_pll_p_out4,	.value = 5},
	{ .input = &tegra_pll_x,	.value = 8},
	{ NULL, 0},
};

static struct clk_mux_sel mux_sclk[] = {
	{ .input = &tegra_clk_m,	.value = 0},
	{ .input = &tegra_pll_c_out1,	.value = 1},
	{ .input = &tegra_pll_c4_out3,	.value = 2},
	{ .input = &tegra_pll_p,	.value = 3},
	{ .input = &tegra_pll_p_out2,	.value = 4},
	{ .input = &tegra_pll_c4_out1,	.value = 5},
	{ .input = &tegra_clk_32k,	.value = 6},
	{ .input = &tegra_pll_c4_out2,	.value = 7},
	{ NULL, 0},
};

/* ADSP cluster clocks */
static struct clk_mux_sel mux_aclk_adsp[] = {
	{ .input = &tegra_pll_p_out_adsp, .value = 2},
	{ .input = &tegra_pll_a_out0_out_adsp, .value = 3},
	{ .input = &tegra_clk_m,	.value = 6},
	{ .input = &tegra_pll_a_out_adsp, .value = 7},
	{ .input = &tegra_pll_a1,	.value = 8},
	{ NULL, 0},
};

static void tegra21_adsp_clk_reset(struct clk *c, bool assert)
{
	unsigned long reg = assert ? RST_DEVICES_SET_Y : RST_DEVICES_CLR_Y;
	u32 val = ADSP_NEON | ADSP_SCU | ADSP_WDT | ADSP_DBG
		| ADSP_PERIPH | ADSP_CORE;

	pr_debug("%s %s\n", __func__, assert ? "assert" : "deassert");

	if (assert)
		clk_writel_delay(val | ADSP_INTF, reg);
	else {
		clk_writel_delay(ADSP_INTF, reg);
		/*
		 * Considering adsp cpu clock (min: 12.5MHZ, max: 1GHz)
		 * a delay of 5us ensures that it's at least
		 * 6 * adsp_cpu_cycle_period long.
		 */
		udelay(5);
		clk_writel_delay(val, reg);
	}
}

static int tegra21_adsp_clk_enable(struct clk *c)
{
	u32 val = ADSP_NEON | ADSP_CORE;

	clk_writel_delay(val, CLK_OUT_ENB_SET_Y);
	return 0;
}

static void tegra21_adsp_clk_disable(struct clk *c)
{
	u32 val = ADSP_NEON | ADSP_CORE;

	clk_writel_delay(val, CLK_OUT_ENB_CLR_Y);
}

static void tegra21_adsp_clk_init(struct clk *c)
{
	/*
	 * Check/enforce ADSP clock default configuration:
	 * - parent clk_m
	 * - disabled
	 */
	tegra21_super_clk_set_parent(c, &tegra_clk_m);
	tegra21_super_clk_init(c);

	/*
	 * CPU and system super clocks cannot be disabled, and super clock init
	 * always marks clock sate ON, which is not true for ADSP. Need explicit
	 * check here.
	 */
	c->state = clk_readl(CLK_OUT_ENB_Y)&(ADSP_NEON | ADSP_CORE) ? ON : OFF;
	if (c->state == ON) {
		WARN(1, "Tegra ADSP clock is running on boot: turning Off\n");
		tegra21_adsp_clk_disable(c);
		c->state = OFF;
	}
}

static struct clk_ops tegra_adsp_ops = {
	.init		= tegra21_adsp_clk_init,
	.enable		= tegra21_adsp_clk_enable,
	.disable	= tegra21_adsp_clk_disable,
	.set_parent	= tegra21_super_clk_set_parent,
	.reset		= tegra21_adsp_clk_reset,
};

static struct clk tegra_clk_aclk_adsp = {
	.name   = "adsp",
	.clk_id = TEGRA210_CLK_ID_ADSP,
	.flags  = DIV_U71 | DIV_U71_INT,
	.inputs	= mux_aclk_adsp,
	.reg	= 0x6e0,
	.ops	= &tegra_adsp_ops,
	.max_rate = 1200000000,
};

static struct clk_ops tegra_adsp_bus_ops = {
	.init     = tegra21_adsp_bus_clk_init,
	.enable   = tegra21_adsp_bus_clk_enable,
	.disable  = tegra21_adsp_bus_clk_disable,
	.set_rate = tegra21_adsp_bus_clk_set_rate,
	.round_rate = tegra21_adsp_bus_clk_round_rate,
	.shared_bus_update = tegra21_clk_shared_connector_update, /* re-use */
};

static struct raw_notifier_head adsp_bus_rate_change_nh;
static struct clk tegra_clk_virtual_adsp_bus = {
	.name      = "adsp_bus",
	.clk_id    = TEGRA210_CLK_ID_ADSP_BUS,
	.parent    = &tegra_clk_aclk_adsp,
	.ops       = &tegra_adsp_bus_ops,
	.shared_bus_flags = SHARED_BUS_RETENTION,
	.max_rate  = 1200000000,
	.u.cpu = {
		.main      = &tegra_pll_a1,
		.backup    = &tegra_pll_p_out_adsp,
	},
	.rate_change_nh = &adsp_bus_rate_change_nh,
};

/* CPU clusters clocks */
static struct clk tegra_clk_cclk_g = {
	.name	= "cclk_g",
	.clk_id = TEGRA210_CLK_ID_CCLK_G,
	.flags  = DIV_U71 | DIV_U71_INT,
	.inputs	= mux_cclk_g,
	.reg	= 0x368,
	.ops	= &tegra_super_ops,
	.max_rate = 3000000000UL,
};

static struct clk tegra_clk_cclk_lp = {
	.name	= "cclk_lp",
	.clk_id = TEGRA210_CLK_ID_CCLK_LP,
	.flags  = DIV_U71 | DIV_U71_INT,
	.inputs	= mux_cclk_lp,
	.reg	= 0x370,
	.ops	= &tegra_super_ops,
	.max_rate = 1350000000,
};

static struct clk tegra_clk_virtual_cpu_g = {
	.name      = "cpu_g",
	.clk_id    = TEGRA210_CLK_ID_CPU_G,
	.parent    = &tegra_clk_cclk_g,
	.ops       = &tegra_cpu_ops,
	.max_rate  = 3000000000UL,
	.u.cpu = {
		.main      = &tegra_pll_x,
		.backup    = &tegra_pll_p_out4,
		.dynamic   = &tegra_dfll_cpu,
		.mode      = MODE_G,
	},
};

static struct clk tegra_clk_virtual_cpu_lp = {
	.name      = "cpu_lp",
	.clk_id    = TEGRA210_CLK_ID_CPU_LP,
	.parent    = &tegra_clk_cclk_lp,
	.ops       = &tegra_cpu_ops,
	.max_rate  = 1350000000,
	.min_rate  = 3187500,
	.u.cpu = {
		.main      = &tegra_pll_x,
		.backup    = &tegra_pll_p_out4,
		.mode      = MODE_LP,
	},
};

static struct clk_mux_sel mux_cpu_cmplx[] = {
	{ .input = &tegra_clk_virtual_cpu_g,	.value = 0},
	{ .input = &tegra_clk_virtual_cpu_lp,	.value = 1},
	{ NULL, 0},
};

static struct clk tegra_clk_cpu_cmplx = {
	.name      = "cpu",
	.clk_id    = TEGRA210_CLK_ID_CCPLEX,
	.inputs    = mux_cpu_cmplx,
	.ops       = &tegra_cpu_cmplx_ops,
	.max_rate  = 3000000000UL,
};

/* System bus clocks */
static struct clk tegra_clk_sclk_mux = {
	.name	= "sclk_mux",
	.clk_id = TEGRA210_CLK_ID_SCLK_MUX,
	.inputs	= mux_sclk,
	.reg	= 0x28,
	.ops	= &tegra_super_ops,
	.max_rate = 1200000000,
	.min_rate = 12000000,
};

static struct clk_mux_sel sclk_mux_out[] = {
	{ .input = &tegra_clk_sclk_mux, .value = 0},
	{ NULL, 0},
};

static struct clk tegra_clk_sclk_div = {
	.name = "sclk_div",
	.clk_id = TEGRA210_CLK_ID_SCLK_DIV,
	.ops = &tegra_periph_clk_ops,
	.reg = 0x400,
	.max_rate = 600000000,
	.min_rate = 12000000,
	.inputs = sclk_mux_out,
	.flags = DIV_U71 | PERIPH_NO_ENB | PERIPH_NO_RESET,
};

static struct clk tegra_clk_sclk = {
	.name = "sclk",
	.clk_id = TEGRA210_CLK_ID_SCLK_SKIP,
	.flags	= DIV_BUS,
	.ops = &tegra_clk_super_skip_ops,
	.reg = 0x2c,
	.parent = &tegra_clk_sclk_div,
};

static struct clk tegra_clk_cop = {
	.name      = "cop",
	.parent    = &tegra_clk_sclk,
	.ops       = &tegra_cop_ops,
	.max_rate  = 600000000,
};

static struct clk tegra_clk_hclk = {
	.name		= "hclk",
	.clk_id		= TEGRA210_CLK_ID_HCLK,
	.flags		= DIV_BUS,
	.parent		= &tegra_clk_sclk,
	.reg		= 0x30,
	.reg_shift	= 4,
	.ops		= &tegra_bus_ops,
	.max_rate       = 600000000,
	.min_rate       = 12000000,
};

static struct clk tegra_clk_pclk = {
	.name		= "pclk",
	.clk_id		= TEGRA210_CLK_ID_PCLK,
	.flags		= DIV_BUS,
	.parent		= &tegra_clk_hclk,
	.reg		= 0x30,
	.reg_shift	= 0,
	.ops		= &tegra_bus_ops,
	.max_rate       = 600000000,
	.min_rate       = 12000000,
};

static struct raw_notifier_head sbus_rate_change_nh;

static struct clk tegra_clk_sbus_cmplx = {
	.name	   = "sbus",
	.clk_id    = TEGRA210_CLK_ID_SBUS,
	.parent    = &tegra_clk_sclk,
	.ops       = &tegra_sbus_cmplx_ops,
	.u.system  = {
		.pclk = &tegra_clk_pclk,
		.hclk = &tegra_clk_hclk,
		.sclk_low = &tegra_pll_p,
		.sclk_high = &tegra_pll_p,
	},
	.rate_change_nh = &sbus_rate_change_nh,
};

static struct clk tegra_clk_ahb = {
	.name	   = "ahb.sclk",
	.flags	   = DIV_BUS,
	.parent    = &tegra_clk_sbus_cmplx,
	.ops       = &tegra_clk_ahb_ops,
};

static struct clk tegra_clk_apb = {
	.name	   = "apb.sclk",
	.flags	   = DIV_BUS,
	.parent    = &tegra_clk_ahb,
	.ops       = &tegra_clk_apb_ops,
};

static struct clk tegra_clk_blink = {
	.name		= "blink",
	.clk_id		= TEGRA210_CLK_ID_BLINK,
	.parent		= &tegra_clk_32k,
	.reg		= 0x40,
	.ops		= &tegra_blink_clk_ops,
	.max_rate	= 32768,
};

static struct clk_ops tegra_xusb_padctl_ops = {
	.reset    = tegra21_periph_clk_reset,
};

static struct clk tegra_clk_xusb_padctl = {
	.name      = "xusb_padctl",
	.clk_id = TEGRA210_CLK_ID_XUSB_PADCTRL,
	.ops       = &tegra_xusb_padctl_ops,
	.u.periph  = {
		.clk_num = 142,
	},
};

static struct clk tegra_clk_sata_uphy = {
	.name      = "sata_uphy",
	.clk_id = TEGRA210_CLK_ID_SATA_UPHY,
	.ops       = &tegra_xusb_padctl_ops,
	.u.periph  = {
		.clk_num = 204,
	},
};

static struct clk tegra_pex_uphy_clk = {
	.name      = "pex_uphy",
	.clk_id = TEGRA210_CLK_ID_PEX_UPHY,
	.ops       = &tegra_xusb_padctl_ops,
	.parent		= &tegra_pll_e,
	.max_rate	= 100000000,
	.u.periph  = {
		.clk_num   = 205,
	},
};

static struct clk tegra_clk_usb2_hsic_trk = {
	.name      = "usb2_hsic_trk",
	.flags     = DIV_U71 | PERIPH_NO_ENB | PERIPH_NO_RESET,
	.inputs    = mux_clk_osc,
	.reg       = 0x6cc,
	.ops       = &tegra_periph_clk_ops,
	.max_rate  = 38400000,
};

/* Multimedia modules muxes */
static struct clk_mux_sel mux_pllc2_c_c3_pllp_plla1_clkm[] = {
	{ .input = &tegra_pll_c2, .value = 1},
	{ .input = &tegra_pll_c,  .value = 2},
	{ .input = &tegra_pll_c3, .value = 3},
	{ .input = &tegra_pll_p,  .value = 4},
	{ .input = &tegra_pll_a1, .value = 6},
	{ .input = &tegra_clk_m, .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllc4_out1_pllc_pllc4_out2_pllp_clkm_plla_pllc4_out0[] = {
	{ .input = &tegra_pll_c4_out1,  .value = 0},
	{ .input = &tegra_pll_c,  .value = 2},
	{ .input = &tegra_pll_c4_out2,  .value = 3},
	{ .input = &tegra_pll_p,  .value = 4},
	{ .input = &tegra_clk_m,  .value = 5},
	{ .input = &tegra_pll_a_out0, .value = 6},
	{ .input = &tegra_pll_c4_out0, .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllc_pllp_plla[] = {
	{ .input = &tegra_pll_c, .value = 2},
	{ .input = &tegra_pll_p, .value = 4},
	{ .input = &tegra_pll_a_out0, .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_clkm_pllc_pllp_plla[] = {
	{ .input = &tegra_clk_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_pll_a_out0, .value = 3},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllc_pllp_plla1_pllc2_c3_clkm[] = {
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_pll_a1, .value = 3},
	{ .input = &tegra_pll_c2, .value = 4},
	{ .input = &tegra_pll_c3, .value = 5},
	{ .input = &tegra_clk_m, .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllc2_c_c3_pllp_clkm_plla1_pllc4[] = {
	{ .input = &tegra_pll_c2, .value = 1},
	{ .input = &tegra_pll_c, .value = 2},
	{ .input = &tegra_pll_c3, .value = 3},
	{ .input = &tegra_pll_p, .value = 4},
	{ .input = &tegra_clk_m, .value = 5},
	{ .input = &tegra_pll_a1, .value = 6},
	{ .input = &tegra_pll_c4_out0, .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllc_pllp_plla1_pllc2_c3_clkm_pllc4[] = {
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_pll_a1, .value = 3},
	{ .input = &tegra_pll_c2, .value = 4},
	{ .input = &tegra_pll_c3, .value = 5},
	{ .input = &tegra_clk_m, .value = 6},
	{ .input = &tegra_pll_c4_out0, .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_plla_pllc4_out0_pllc_pllc4_out1_pllp_clkm_pllc4_out2[] = {
	{ .input = &tegra_pll_a_out0, .value = 0},
	{ .input = &tegra_pll_c4_out0, .value = 1},
	{ .input = &tegra_pll_c, .value = 2},
	{ .input = &tegra_pll_c4_out1, .value = 3},
	{ .input = &tegra_pll_p, .value = 4},
	{ .input = &tegra_pll_c4_out2, .value = 5},
	{ .input = &tegra_clk_m, .value = 6},
	{ NULL, 0},
};

/* EMC muxes */
/* FIXME: add EMC latency mux */
/* FIXME: add EMC PLLP branch */
static struct clk_mux_sel mux_pllm_pllc_pllp_clkm[] = {
	{ .input = &tegra_pll_m, .value = 0},
	{ .input = &tegra_pll_c, .value = 1},
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_clk_m, .value = 3},
	{ .input = &tegra_pll_m, .value = 4},  /* low jitter PLLM output */
	{ .input = &tegra_pll_mb, .value = 5}, /* low jitter PLLMB output */
	{ .input = &tegra_pll_mb, .value = 6},
	{ .input = &tegra_pll_p, .value = 7}, /* low jitter PLLP output */
	{ NULL, 0},
};


/* Display subsystem muxes */
static struct clk_mux_sel mux_pllp_plld_plld2_clkm[] = {
	{.input = &tegra_pll_p_out_hsio, .value = 0},
	{.input = &tegra_pll_d_out0, .value = 2},
	{.input = &tegra_pll_d2, .value = 5},
	{.input = &tegra_clk_m, .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_plld_out0[] = {
	{ .input = &tegra_pll_d_out0,  .value = 0},
	{ NULL, 0},
};

static struct clk_mux_sel mux_plldp[] = {
	{ .input = &tegra_pll_dp,  .value = 0},
	{ NULL, 0},
};

static struct clk tegra_clk_sor0_brick = {
	.name = "sor0_brick",
	.clk_id = TEGRA210_CLK_ID_SOR0_BRICK,
	.ops = &tegra_sor_brick_ops,
	.max_rate = 600000000,
	.inputs = mux_plldp,
	.flags = PERIPH_NO_ENB | PERIPH_NO_RESET,
};

static struct clk_mux_sel mux_pllp_sor_sor0_brick[] = {
	{ .input = &tegra_pll_p_out_sor,  .value = 0},
	{ .input = &tegra_clk_sor0_brick,  .value = 1},
	{ NULL, 0},
};

static struct clk tegra_clk_sor1_src = {
	.name = "sor1_src",
	.clk_id = TEGRA210_CLK_ID_SOR1_SRC,
	.ops = &tegra_periph_clk_ops,
	.reg = 0x410,
	.max_rate = 600000000,
	.inputs = mux_pllp_plld_plld2_clkm,
	.flags = MUX | DIV_U71 | PERIPH_NO_ENB | PERIPH_NO_RESET,
};

static struct clk_mux_sel mux_plldp_sor1_src[] = {
	{ .input = &tegra_pll_dp,  .value = 0},
	{ .input = &tegra_clk_sor1_src,  .value = 1},
	{ NULL, 0},
};

static struct clk tegra_clk_sor1_brick = {
	.name = "sor1_brick",
	.clk_id = TEGRA210_CLK_ID_SOR1_BRICK,
	.ops = &tegra_sor_brick_ops,
	.max_rate = 600000000,
	.inputs = mux_plldp_sor1_src,
	.flags = MUX | PERIPH_NO_ENB | PERIPH_NO_RESET,
};

static struct clk_mux_sel mux_pllp_sor_sor1_brick_sor1_src[] = {
	{ .input = &tegra_pll_p_out_sor, .value = 0},
	{ .input = &tegra_clk_sor1_brick, .value = 1},
	{ .input = &tegra_clk_sor1_src, .value = 2},
	{ .input = &tegra_clk_sor1_brick, .value = 3},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllre_clkm[] = {
	{ .input = &tegra_pll_p,    .value = 0 },
	{ .input = &tegra_pll_re_out1, .value = 4 },
	{ .input = &tegra_clk_m,    .value = 6 },
};

/* Peripheral muxes */
static struct clk_mux_sel mux_pllp_pllc_clkm[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_clk_m,     .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllc_plla_clkm[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_pll_a_out0,     .value = 4},
	{.input = &tegra_clk_m,     .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllc_pllc4_out0_pllc4_out1_clkm_pllc4_out2[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_pll_c4_out0,     .value = 3},
	{.input = &tegra_pll_c4_out1,     .value = 5},
	{.input = &tegra_clk_m,     .value = 6},
	{.input = &tegra_pll_c4_out2,     .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllc_pllc_out1_pllc4_out2_pllc4_out1_clkm_pllc4_out0[] = {
	{.input = &tegra_pll_p,             .value = 0},
	{.input = &tegra_pll_c_out1,        .value = 1},
	{.input = &tegra_pll_c,             .value = 2},
	{.input = &tegra_pll_c4_out2,       .value = 4},
	{.input = &tegra_pll_c4_out1,       .value = 5},
	{.input = &tegra_clk_m,             .value = 6},
	{.input = &tegra_pll_c4_out0,       .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllc4_out2_pllc4_out1_clkm_pllc4_out0[] = {
	{.input = &tegra_pll_p,             .value = 0},
	{.input = &tegra_pll_c4_out2,       .value = 3},
	{.input = &tegra_pll_c4_out1,       .value = 4},
	{.input = &tegra_clk_m,             .value = 6},
	{.input = &tegra_pll_c4_out0,       .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_clk_m_pllc4_out2_out1_out0_lj[] = {
	{.input = &tegra_pll_p,             .value = 0},
	{.input = &tegra_pll_c4_out2,       .value = 1},  /* LJ input */
	{.input = &tegra_pll_c4_out0,       .value = 2},  /* LJ input */
	{.input = &tegra_pll_c4_out2,       .value = 3},
	{.input = &tegra_pll_c4_out1,       .value = 4},
	{.input = &tegra_pll_c4_out1,       .value = 5},  /* LJ input */
	{.input = &tegra_clk_m,             .value = 6},
	{.input = &tegra_pll_c4_out0,       .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllc2_c_c3_clkm[] = {
	{ .input = &tegra_pll_p,  .value = 0},
	{ .input = &tegra_pll_c2, .value = 1},
	{ .input = &tegra_pll_c,  .value = 2},
	{ .input = &tegra_pll_c3, .value = 3},
	{ .input = &tegra_clk_m,  .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllc_clkm_1[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_clk_m,     .value = 5},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_clkm_2[] = {
	{ .input = &tegra_pll_p, .value = 2},
	{ .input = &tegra_clk_m, .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_clkm_clk32_plle[] = {
	{ .input = &tegra_pll_p, .value = 0},
	{ .input = &tegra_clk_m, .value = 2},
	{ .input = &tegra_clk_32k, .value = 4},
	{ .input = &tegra_pll_e_gate, .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_clkm[] = {
	{ .input = &tegra_pll_p, .value = 0},
	{ .input = &tegra_clk_m, .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllp_out3_clkm_clk32k_plla[] = {
	{ .input = &tegra_pll_p, .value = 0},
	{ .input = &tegra_pll_p_out3, .value = 1},
	{ .input = &tegra_clk_m, .value = 2},
	{ .input = &tegra_clk_32k, .value = 3},
	{ .input = &tegra_pll_a_out0, .value = 4},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_out3_clkm_pllp_pllc4[] = {
	{ .input = &tegra_pll_p_out3, .value = 0},
	{ .input = &tegra_clk_m, .value = 3},
	{ .input = &tegra_pll_p, .value = 4},
	{ .input = &tegra_pll_c4_out0, .value = 5},
	{ .input = &tegra_pll_c4_out1, .value = 6},
	{ .input = &tegra_pll_c4_out2, .value = 7},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllc_clk32_clkm[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_clk_32k,   .value = 4},
	{.input = &tegra_clk_m,     .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_pllc_clkm_clk32[] = {
	{.input = &tegra_pll_p,     .value = 0},
	{.input = &tegra_pll_c,     .value = 2},
	{.input = &tegra_clk_m,     .value = 4},
	{.input = &tegra_clk_32k,   .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_plla_clk32_pllp_clkm_plle[] = {
	{ .input = &tegra_pll_a_out0, .value = 0},
	{ .input = &tegra_clk_32k,    .value = 1},
	{ .input = &tegra_pll_p,      .value = 2},
	{ .input = &tegra_clk_m,      .value = 3},
	{ .input = &tegra_pll_e_gate, .value = 4},
	{ NULL, 0},
};

static struct clk_mux_sel mux_clkm_pllp_pllre[] = {
	{ .input = &tegra_clk_m,  .value = 0},
	{ .input = &tegra_pll_p_out_xusb, .value = 1},
	{ .input = &tegra_pll_re_out,  .value = 5},
	{ NULL, 0},
};

static struct clk_mux_sel mux_clkm_48M_pllp_480M[] = {
	{ .input = &tegra_clk_m,      .value = 0},
	{ .input = &tegra_pll_u_48M,  .value = 2},
	{ .input = &tegra_pll_p_out_xusb, .value = 4},
	{ .input = &tegra_pll_u_480M, .value = 6},
	{ NULL, 0},
};

static struct clk_mux_sel mux_clkm_pllre_clk32_480M[] = {
	{ .input = &tegra_clk_m,      .value = 0},
	{ .input = &tegra_pll_re_out, .value = 1},
	{ .input = &tegra_clk_32k,    .value = 2},
	{ .input = &tegra_pll_u_480M, .value = 3},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_out3_pllp_pllc_clkm[] = {
	{ .input = &tegra_pll_p_out3, .value = 0},
	{ .input = &tegra_pll_p,  .value = 1},
	{ .input = &tegra_pll_c,  .value = 2},
	{ .input = &tegra_clk_m,  .value = 6},
	{ NULL, 0},
};

/* Single clock source ("fake") muxes */
static struct clk_mux_sel mux_clk_m[] = {
	{ .input = &tegra_clk_m, .value = 0},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pex_uphy_clk[] = {
	{ .input = &tegra_pex_uphy_clk, .value = 0},
	{ NULL, 0},
};

static struct clk_mux_sel mux_pllp_out3[] = {
	{ .input = &tegra_pll_p_out3, .value = 0},
	{ NULL, 0},
};

static struct clk_mux_sel mux_clk_32k[] = {
	{ .input = &tegra_clk_32k, .value = 0},
	{ NULL, 0},
};

static struct clk_mux_sel mux_clk_usb2_hsic_trk[] = {
	{ .input = &tegra_clk_usb2_hsic_trk, .value = 0},
	{ NULL, 0},
};

static struct clk tegra_clk_mc;
static struct clk_mux_sel mux_clk_mc[] = {
	{ .input = &tegra_clk_mc, .value = 0},
	{ NULL, 0},
};

static struct raw_notifier_head emc_rate_change_nh;

static struct clk tegra_clk_sata = {
	.name      = "sata",
	.clk_id = TEGRA210_CLK_ID_SATA,
	.lookup    = {
		.dev_id = "tegra_sata",
	},
	.ops       = &tegra_periph_clk_ops,
	.reg       = SOURCE_SATA,
	.max_rate  = 102000000,
	.u.periph  = {
		.clk_num = 124,
	},
	.inputs	   = mux_pllp_pllc_clkm,
	.flags	   = MUX | DIV_U71 | PERIPH_ON_APB,
};

static struct clk tegra_sata_aux_clk = {
	.name      = "sata_aux",
	.clk_id	= TEGRA210_CLK_ID_SATA_AUX,
	.lookup    = {
		.dev_id = "tegra_sata_aux",
	},
	.parent    = &tegra_clk_sata,
	.ops       = &tegra_cml_clk_ops,
	.reg       = SOURCE_SATA,
	.max_rate  = 102000000,
	.u.periph  = {
		.clk_num = 24,
	},
};

static struct clk tegra_clk_emc = {
	.name = "emc",
	.clk_id	= TEGRA210_CLK_ID_EMC,
	.lookup = {
		.dev_id = "tegra_emc",
		.con_id = "emc",
	},
	.ops = &tegra_emc_clk_ops,
	.reg = 0x19c,
	.max_rate = 1800000000,
	.min_rate = 12750000,
	.inputs = mux_pllm_pllc_pllp_clkm,
	.flags = MUX | DIV_U71 | PERIPH_EMC_ENB,
	.u.periph = {
		.clk_num = 57,
	},
	.rate_change_nh = &emc_rate_change_nh,
};

static struct clk tegra_clk_mc = {
	.name = "mc",
	.clk_id	= TEGRA210_CLK_ID_MC,
	.ops = &tegra_mc_clk_ops,
	.max_rate = 1066000000,
	.parent = &tegra_clk_emc,
	.flags = PERIPH_NO_RESET,
	.u.periph = {
		.clk_num = 32,
	},
};

static struct raw_notifier_head host1x_rate_change_nh;

static struct clk tegra_clk_host1x = {
	.name      = "host1x",
	.clk_id = TEGRA210_CLK_ID_HOST1X,
	.lookup    = {
		.dev_id = "host1x",
	},
	.ops       = &tegra_1xbus_clk_ops,
	.reg       = 0x180,
	.inputs    = mux_pllc4_out1_pllc_pllc4_out2_pllp_clkm_plla_pllc4_out0,
	.flags     = MUX | DIV_U71 | DIV_U71_INT,
	.max_rate  = 600000000,
	.min_rate  = 12000000,
	.u.periph = {
		.clk_num   = 28,
		.pll_low = &tegra_pll_p,
		.pll_high = &tegra_pll_p,
	},
	.rate_change_nh = &host1x_rate_change_nh,
};

static struct raw_notifier_head mselect_rate_change_nh;

static struct clk tegra_clk_mselect = {
	.name      = "mselect",
	.clk_id = TEGRA210_CLK_ID_MSELECT,
	.lookup    = {
		.dev_id = "mselect",
	},
	.ops       = &tegra_1xbus_clk_ops,
	.reg       = 0x3b4,
	.inputs    = mux_pllp_clkm,
	.flags     = MUX | DIV_U71 | DIV_U71_INT,
	.max_rate  = 408000000,
	.min_rate  = 12000000,
	.u.periph = {
		.clk_num   = 99,
		.pll_low = &tegra_pll_p,
		.pll_high = &tegra_pll_p,
		.threshold = 408000000,
	},
	.rate_change_nh = &mselect_rate_change_nh,
};

static struct clk_mux_sel mux_clk_mselect[] = {
	{ .input = &tegra_clk_mselect, .value = 0},
	{ NULL, 0},
};

static struct raw_notifier_head ape_rate_change_nh;

static struct clk tegra_clk_ape = {
	.name      = "ape",
	.clk_id = TEGRA210_CLK_ID_APE,
	.ops       = &tegra_1xbus_clk_ops,
	.reg       = 0x6c0,
	.inputs    = mux_plla_pllc4_out0_pllc_pllc4_out1_pllp_clkm_pllc4_out2,
	.flags     = MUX | DIV_U71 | PERIPH_ON_APB,
	.shared_bus_flags = SHARED_BUS_RETENTION,
	.max_rate  = 408000000,
	.min_rate  = 12000000,
	.u.periph = {
		.clk_num   = 198,
		.pll_low = &tegra_pll_p,
		.pll_high = &tegra_pll_p,
		.threshold = 408000000,
	},
	.rate_change_nh = &ape_rate_change_nh,
};

static struct raw_notifier_head c2bus_rate_change_nh;
static struct raw_notifier_head c3bus_rate_change_nh;

static struct clk tegra_clk_c2bus = {
	.name      = "c2bus",
	.clk_id    = TEGRA210_CLK_ID_C2BUS,
	.parent    = &tegra_pll_c2,
	.ops       = &tegra_clk_cbus_ops,
	.max_rate  = 1000000000,
	.mul       = 1,
	.div       = 1,
	.flags     = PERIPH_ON_CBUS,
	.shared_bus_backup = {
		.input = &tegra_pll_p,
	},
#ifdef	CONFIG_TEGRA_CBUS_CAN_USE_SKIPPERS
	.shared_bus_flags = SHARED_BUS_USE_SKIPPERS,
#endif
	.rate_change_nh = &c2bus_rate_change_nh,
};
static struct clk tegra_clk_c3bus = {
	.name      = "c3bus",
	.clk_id    = TEGRA210_CLK_ID_C3BUS,
	.parent    = &tegra_pll_c3,
	.ops       = &tegra_clk_cbus_ops,
	.max_rate  = 1000000000,
	.mul       = 1,
	.div       = 1,
	.flags     = PERIPH_ON_CBUS,
	.shared_bus_backup = {
		.input = &tegra_pll_p,
	},
#ifdef	CONFIG_TEGRA_CBUS_CAN_USE_SKIPPERS
	.shared_bus_flags = SHARED_BUS_USE_SKIPPERS,
#endif
	.rate_change_nh = &c3bus_rate_change_nh,
};

#ifdef CONFIG_TEGRA_MIGRATE_CBUS_USERS
static DEFINE_MUTEX(cbus_mutex);
#define CROSS_CBUS_MUTEX (&cbus_mutex)
#else
#define CROSS_CBUS_MUTEX NULL
#endif


static struct clk_mux_sel mux_clk_cbus[] = {
	{ .input = &tegra_clk_c2bus, .value = 0},
	{ .input = &tegra_clk_c3bus, .value = 1},
	{ NULL, 0},
};

#define DUAL_CBUS_CLK(_name, _dev, _con, _parent, _id, _div, _mode, _clk_id)\
	{						\
		.name      = _name,			\
		.clk_id    = _clk_id,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.inputs = mux_clk_cbus,			\
		.flags = MUX,				\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
		.cross_clk_mutex = CROSS_CBUS_MUTEX,	\
	}

static struct clk_ops tegra_clk_gpu_ops = {
	.enable		= &tegra21_periph_clk_enable,
	.disable	= &tegra21_periph_clk_disable,
	.reset		= &tegra21_periph_clk_reset,
};

/*
 * This clock provides a common gate control for branches of different clocks
 * supplied to GPU: mselect, memory controller, pll_p_out5, jtag and la. The
 * mselect and mc clocks are always running; pll_p_out5 is explicitly controlled
 * by GPU driver; jtag and la clocks are set by debug interface configuration.
 * Therefore we don't need to propagate refcount to those clocks, and just set
 * GPU gate clock as OSC child.
 */
static struct clk tegra_clk_gpu_gate = {
	.name      = "gpu_gate",
	.clk_id    = TEGRA210_CLK_ID_GPU_GATE,
	.parent    = &tegra_clk_osc,
	.ops       = &tegra_clk_gpu_ops,
	.u.periph  = {
		.clk_num = 184,
	},
	.flags	   = PERIPH_MANUAL_RESET,
	.max_rate  = 48000000,
	.min_rate  = 12000000,
};

/*
 * This is a reference clock for GPU. The enable/disable routine control input
 * clock of the GPCPLL. The reference clock itself has a fixed frequency. The
 * actual GPU clock's frequency is controlled via gbus, which is a child of this
 * clock.
 */
static struct clk tegra_clk_gpu_ref = {
	.name      = "gpu_ref",
	.clk_id    = TEGRA210_CLK_ID_GPU_REF,
	.ops       = &tegra_clk_gpu_ops,
	.parent    = &tegra_pll_ref,
	.u.periph  = {
		.clk_num = 189,
	},
	.flags	   = PERIPH_NO_RESET,
	.max_rate  = 48000000,
	.min_rate  = 12000000,
};

#define RATE_GRANULARITY	100000 /* 0.1 MHz */
#if defined(CONFIG_TEGRA_CLOCK_DEBUG_FUNC)
static int gbus_round_pass_thru;
void tegra_gbus_round_pass_thru_enable(bool enable)
{
	if (enable)
		gbus_round_pass_thru = 1;
	else
		gbus_round_pass_thru = 0;
}
int tegra_gbus_round_pass_thru_get(void)
{
	return gbus_round_pass_thru;
}
EXPORT_SYMBOL(tegra_gbus_round_pass_thru_enable);
EXPORT_SYMBOL(tegra_gbus_round_pass_thru_get);
#else
#define gbus_round_pass_thru	0
#endif

static void tegra21_clk_gbus_init(struct clk *c)
{
	unsigned long rate;
	bool enabled;

	pr_debug("%s on clock %s (export ops %s)\n", __func__,
		 c->name, c->u.export_clk.ops ? "ready" : "not ready");

	if (!c->u.export_clk.ops || !c->u.export_clk.ops->init)
		return;

	c->u.export_clk.ops->init(c->u.export_clk.ops->data, &rate, &enabled);
	c->div = clk_get_rate(c->parent) / RATE_GRANULARITY;
	c->mul = rate / RATE_GRANULARITY;
	c->state = enabled ? ON : OFF;
}

static int tegra21_clk_gbus_enable(struct clk *c)
{
	pr_debug("%s on clock %s (export ops %s)\n", __func__,
		 c->name, c->u.export_clk.ops ? "ready" : "not ready");

	if (!c->u.export_clk.ops || !c->u.export_clk.ops->enable)
		return -ENOENT;

	return c->u.export_clk.ops->enable(c->u.export_clk.ops->data);
}

static void tegra21_clk_gbus_disable(struct clk *c)
{
	pr_debug("%s on clock %s (export ops %s)\n", __func__,
		 c->name, c->u.export_clk.ops ? "ready" : "not ready");

	if (!c->u.export_clk.ops || !c->u.export_clk.ops->disable)
		return;

	c->u.export_clk.ops->disable(c->u.export_clk.ops->data);
}

static int tegra21_clk_gbus_set_rate(struct clk *c, unsigned long rate)
{
	int ret;

	pr_debug("%s %lu on clock %s (export ops %s)\n", __func__,
		 rate, c->name, c->u.export_clk.ops ? "ready" : "not ready");

	if (!c->u.export_clk.ops || !c->u.export_clk.ops->set_rate)
		return -ENOENT;

	ret = c->u.export_clk.ops->set_rate(c->u.export_clk.ops->data, &rate);
	if (!ret)
		c->mul = rate / RATE_GRANULARITY;
	return ret;
}

static long tegra21_clk_gbus_round_updown(struct clk *c, unsigned long rate,
					  bool up)
{
	return gbus_round_pass_thru ? rate :
		tegra21_clk_cbus_round_updown(c, rate, up);
}

static long tegra21_clk_gbus_round_rate(struct clk *c, unsigned long rate)
{
	return tegra21_clk_gbus_round_updown(c, rate, true);
}

static struct clk_ops tegra_clk_gbus_ops = {
	.init		= tegra21_clk_gbus_init,
	.enable		= tegra21_clk_gbus_enable,
	.disable	= tegra21_clk_gbus_disable,
	.set_rate	= tegra21_clk_gbus_set_rate,
	.round_rate	= tegra21_clk_gbus_round_rate,
	.round_rate_updown = tegra21_clk_gbus_round_updown,
	.shared_bus_update = tegra21_clk_shared_connector_update, /* re-use */
};

static struct raw_notifier_head gbus_rate_change_nh;
static wait_queue_head_t gbus_poll_rate;
static struct bus_stats gpu_histogram;

static struct clk tegra_clk_gbus = {
	.name      = "gbus",
	.clk_id    = TEGRA210_CLK_ID_GBUS,
	.ops       = &tegra_clk_gbus_ops,
	.parent    = &tegra_clk_gpu_ref,
	.max_rate  = 1300000000,
	.shared_bus_flags = SHARED_BUS_RETENTION,
	.stats = {
		.histogram = &gpu_histogram,
	},
	.rate_change_nh = &gbus_rate_change_nh,
	.debug_poll_qh  = &gbus_poll_rate,
};

static void tegra21_camera_mclk_init(struct clk *c)
{
	c->state = OFF;
	c->set = true;

	if (!strcmp(c->name, "mclk")) {
		c->parent = tegra_get_clock_by_name("vi_sensor");
		c->max_rate = c->parent->max_rate;
	} else if (!strcmp(c->name, "mclk2")) {
		c->parent = tegra_get_clock_by_name("vi_sensor2");
		c->max_rate = c->parent->max_rate;
	} else if (!strcmp(c->name, "mclk3")) {
		c->parent = tegra_get_clock_by_name("extern3");
		c->max_rate = c->parent->max_rate;
	} else if (!strcmp(c->name, "cam-mipi-cal")) {
		c->parent = tegra_get_clock_by_name("uart_mipi_cal");
		c->max_rate = c->parent->max_rate;
	}
}

static int tegra21_camera_mclk_set_rate(struct clk *c, unsigned long rate)
{
	return clk_set_rate(c->parent, rate);
}

static struct clk_ops tegra_camera_mclk_ops = {
	.init     = tegra21_camera_mclk_init,
	.enable   = tegra21_periph_clk_enable,
	.disable  = tegra21_periph_clk_disable,
	.set_rate = tegra21_camera_mclk_set_rate,
};

static struct clk tegra_camera_mclk = {
	.name = "mclk",
	.clk_id = TEGRA210_CLK_ID_MCLK,
	.ops = &tegra_camera_mclk_ops,
	.u.periph = {
		.clk_num = 92, /* csus */
	},
	.flags = PERIPH_NO_RESET,
};

static struct clk tegra_camera_mclk2 = {
	.name = "mclk2",
	.clk_id = TEGRA210_CLK_ID_MCLK2,
	.ops = &tegra_camera_mclk_ops,
	.u.periph = {
		.clk_num = 171, /* vim2_clk */
	},
	.flags = PERIPH_NO_RESET,
};

static struct clk tegra_camera_mipical = {
	.name = "cam-mipi-cal",
	.clk_id = TEGRA210_CLK_ID_CAM_MIPI_CAL,
	.ops = &tegra_camera_mclk_ops,
	.u.periph = {
		.clk_num = 56, /* mipi_cal */
	},
	.flags = PERIPH_NO_RESET,
};

static struct clk_ops tegra_camera_mclk3_ops = {
	.init		= &tegra21_camera_mclk_init,
	.enable		= &tegra21_clk_out_enable,
	.disable	= &tegra21_clk_out_disable,
	.set_rate	= &tegra21_camera_mclk_set_rate,
};

static struct clk tegra_camera_mclk3 = {
	.name = "mclk3",
	.clk_id = TEGRA210_CLK_ID_MCLK3,
	.ops = &tegra_camera_mclk3_ops,
	.reg = 0x1a8,
	.flags = MUX_CLK_OUT,
	.u.periph = {
		.clk_num   = (3 - 1) * 8 + 2,
	},
};

static struct clk tegra_clk_isp = {
	.name = "isp",
	.clk_id = TEGRA210_CLK_ID_ISP,
	.ops = &tegra_periph_clk_ops,
	.reg = 0x144,
	.max_rate = 1000000000,
	.inputs = mux_pllc_pllp_plla1_pllc2_c3_clkm_pllc4,
	.flags = MUX | DIV_U71 | DIV_U71_INT | PERIPH_NO_ENB | PERIPH_NO_RESET,
};

static struct clk_mux_sel mux_isp[] = {
	{ .input = &tegra_clk_isp, .value = 0},
	{ NULL, 0},
};

static struct clk tegra_clk_hda2codec_2x = {
	.name = "hda2codec_2x",
	.clk_id = TEGRA210_CLK_ID_HDA2CODEC_2X,
	.lookup = {
		.dev_id = "tegra30-hda",
		.con_id = "hda2codec_2x",
	},
	.ops = &tegra_periph_clk_ops,
	.u.periph = {
		.clk_num = 111,
	},
	.reg = 0x3e4,
	.max_rate = 48000000,
	.inputs = mux_pllp_pllc_plla_clkm,
	.flags = MUX | DIV_U71 | PERIPH_ON_APB,
};

static struct clk_mux_sel mux_hda2codec_2x[] = {
	{ .input = &tegra_clk_hda2codec_2x, .value = 0},
	{ NULL, 0},
};

static struct raw_notifier_head cbus_rate_change_nh;

static struct clk tegra_clk_cbus = {
	.name	   = "cbus",
	.clk_id    = TEGRA210_CLK_ID_CBUS,
	.parent    = &tegra_pll_c,
	.ops       = &tegra_clk_cbus_ops,
	.max_rate  = 1000000000,
	.mul	   = 1,
	.div	   = 1,
	.flags     = PERIPH_ON_CBUS,
	.shared_bus_backup = {
		.input = &tegra_pll_p,
	},
	.rate_change_nh = &cbus_rate_change_nh,
};

#define PERIPH_CLK(_name, _dev, _con, _clk_num, _reg, _max, _inputs, _flags, \
			_clk_id)					     \
	{						\
		.name      = _name,			\
		.clk_id    = _clk_id,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id	   = _con,		\
		},					\
		.ops       = &tegra_periph_clk_ops,	\
		.reg       = _reg,			\
		.inputs    = _inputs,			\
		.flags     = _flags,			\
		.max_rate  = _max,			\
		.u.periph = {				\
			.clk_num   = _clk_num,		\
		},					\
	}

#define PERIPH_CLK_EX(_name, _dev, _con, _clk_num, _reg, _max, _inputs,	\
			_flags, _clk_id, _ops)				\
	{						\
		.name      = _name,			\
		.clk_id    = _clk_id,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id	   = _con,		\
		},					\
		.ops       = _ops,			\
		.reg       = _reg,			\
		.inputs    = _inputs,			\
		.flags     = _flags,			\
		.max_rate  = _max,			\
		.u.periph = {				\
			.clk_num   = _clk_num,		\
		},					\
	}

#define SUPER_SKIP_CLK(_name, _dev, _con, _reg, _parent, _flags) \
	{						\
		.name      = _name,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id	   = _con,		\
		},					\
		.ops       = &tegra_clk_super_skip_ops,	\
		.reg       = _reg,			\
		.parent    = _parent,			\
		.flags     = _flags,			\
	}

#define PERIPH_CLK_SKIP(_name, _dev, _con, _clk_num, _reg, _reg_skip, _max, \
			_inputs, _flags, _clk_id) \
	PERIPH_CLK(_name, _dev, _con, _clk_num, _reg, _max, _inputs, _flags, \
			_clk_id), \
	SUPER_SKIP_CLK(_name "_skip", _dev, "skip", _reg_skip, NULL, 0)

#define D_AUDIO_CLK(_name, _dev, _con, _clk_num, _reg, _max, _inputs, _flags, _clk_id) \
	{						\
		.name      = _name,			\
		.clk_id    = _clk_id,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id	   = _con,		\
		},					\
		.ops       = &tegra_periph_clk_ops,	\
		.reg       = _reg,			\
		.inputs    = _inputs,			\
		.flags     = _flags,			\
		.max_rate  = _max,			\
		.u.periph = {				\
			.clk_num   = _clk_num,		\
			.src_mask  = 0xE01F << 16,	\
			.src_shift = 16,		\
		},					\
	}

#define SHARED_CLK(_name, _dev, _con, _parent, _id, _div, _mode, _clk_id)\
	{						\
		.name      = _name,			\
		.clk_id    = _clk_id,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
	}
#define SHARED_LIMIT(_name, _dev, _con, _parent, _id, _div, _mode, _clk_id)\
	{						\
		.name      = _name,			\
		.clk_id    = _clk_id,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.flags     = BUS_RATE_LIMIT,		\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
	}
#define SHARED_CONNECT(_name, _dev, _con, _parent, _id, _div, _mode, _clk_id)\
	{						\
		.name      = _name,			\
		.clk_id    = _clk_id,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_connector_ops,	\
		.parent = _parent,			\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
	}
#define SHARED_EMC_CLK(_name, _dev, _con, _parent, _id, _div, _mode, _flag, _clk_id)\
	{						\
		.name      = _name,			\
		.clk_id    = _clk_id,			\
		.lookup    = {				\
			.dev_id    = _dev,		\
			.con_id    = _con,		\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
			.usage_flag = _flag,		\
		},					\
	}

static DEFINE_MUTEX(sbus_cross_mutex);
#define SHARED_SCLK(_name, _dev, _con, _parent, _id, _div, _mode, _clk_id)\
	{						\
		.name = _name,				\
		.clk_id    = _clk_id,			\
		.lookup = {				\
			.dev_id = _dev,			\
			.con_id = _con,			\
		},					\
		.ops = &tegra_clk_shared_bus_user_ops,	\
		.parent = _parent,			\
		.u.shared_bus_user = {			\
			.client_id = _id,		\
			.client_div = _div,		\
			.mode = _mode,			\
		},					\
		.cross_clk_mutex = &sbus_cross_mutex,	\
}

static struct clk tegra_list_clks[] = {
	PERIPH_CLK("ahbdma",	"ahbdma",		NULL,	33,	0,	38400000,  mux_clk_m,			0, TEGRA210_CLK_ID_AHBDMA),
	PERIPH_CLK("apbdma",	"60020000.dma",		NULL,	34,	0,	38400000,  mux_clk_m,			0, TEGRA210_CLK_ID_APBDMA),
	PERIPH_CLK("rtc",	"rtc-tegra",		NULL,	4,	0,	32768,     mux_clk_32k,			PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_RTC),
	PERIPH_CLK("timer",	"timer",		NULL,	5,	0,	38400000,  mux_clk_m,			0, TEGRA210_CLK_ID_TIMER),
	PERIPH_CLK("spare1",	"spare1",		NULL,	192,	0,	38400000,  mux_clk_m,			0, TEGRA210_CLK_ID_SPARE1),
	PERIPH_CLK("axiap",	"axiap",		NULL,	196,	0,	38400000,  mux_clk_m,			0, TEGRA210_CLK_ID_AXIAP),
	PERIPH_CLK("iqc1",	"tegra210-iqc.0",	NULL,	221,	0,	38400000,  mux_clk_m,			PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_IQC1),
	PERIPH_CLK("iqc2",	"tegra210-iqc.1",	NULL,	220,	0,	38400000,  mux_clk_m,			PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_IQC2),
	PERIPH_CLK("kfuse",	"kfuse-tegra",		NULL,	40,	0,	38400000,  mux_clk_m,			PERIPH_ON_APB, TEGRA210_CLK_ID_KFUSE),
	PERIPH_CLK("fuse",	"fuse-tegra",	      "fuse",	39,	0,	38400000,  mux_clk_m,			PERIPH_ON_APB, TEGRA210_CLK_ID_FUSE),
	PERIPH_CLK("fuse_burn",	"fuse-tegra",	 "fuse_burn",	39,	0,	38400000,  mux_clk_m,			PERIPH_ON_APB, TEGRA210_CLK_ID_FUSE_BURN),

	PERIPH_CLK("i2s0",	"tegra210-i2s.0",	NULL,	30,	0x1d8,	 24576000, mux_pllaout0_audio0_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_I2S0),
	PERIPH_CLK("i2s1",	"tegra210-i2s.1",	NULL,	11,	0x100,	 24576000, mux_pllaout0_audio1_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_I2S1),
	PERIPH_CLK("i2s2",	"tegra210-i2s.2",	NULL,	18,	0x104,	 24576000, mux_pllaout0_audio2_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_I2S2),
	PERIPH_CLK("i2s3",	"tegra210-i2s.3",	NULL,	101,	0x3bc,	 24576000, mux_pllaout0_audio3_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_I2S3),
	PERIPH_CLK("i2s4",	"tegra210-i2s.4",	NULL,	102,	0x3c0,	 24576000, mux_pllaout0_audio4_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_I2S4),
	PERIPH_CLK("spdif_out",	"tegra210-spdif", "spdif_out",	10,	0x108,	 24727300, mux_pllaout0_audio_2x_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_SPDIF_OUT),
	PERIPH_CLK("spdif_in",	"tegra210-spdif",  "spdif_in",	10,	0x10c,	408000000, mux_pllp_pllc_clkm_1,		MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_SPDIF_IN),
	PERIPH_CLK("dmic1",	"tegra210-dmic.0",	NULL,	161,	0x64c,	 12288000, mux_pllaout0_audio0_dmic_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_DMIC1),
	PERIPH_CLK("dmic2",	"tegra210-dmic.1",	NULL,	162,	0x650,	 12288000, mux_pllaout0_audio1_dmic_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_DMIC2),
	PERIPH_CLK("dmic3",	"tegra210-dmic.2",	NULL,	197,	0x6bc,	 12288000, mux_pllaout0_audio2_dmic_pllp_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_DMIC3),
	PERIPH_CLK("apb2ape",	NULL,		    "apb2ape",	107,	0,	38400000,  mux_clk_m,			PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_APB2APE),
	PERIPH_CLK("maud",	"maud",			NULL,	202,	0x6d4,	102000000, mux_pllp_pllp_out3_clkm_clk32k_plla,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_MAUD),
	PERIPH_CLK("pwm",	"7000a000.pwm",		"pwm",	17,	0x110,	 48000000, mux_pllp_pllc_clk32_clkm,		MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_PWM),
	D_AUDIO_CLK("d_audio",	"tegra210-axbar",	"ahub",	106,	0x3d0,	 98304000, mux_d_audio_clk,			MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_AHUB),
	PERIPH_CLK("hda",	"tegra30-hda",		"hda",  125,	0x428,	102000000, mux_pllp_pllc_clkm,			MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_HDA),
	PERIPH_CLK("hda2hdmi",	"tegra30-hda",	   "hda2hdmi",	128,	0,	 48000000,  mux_hda2codec_2x,				PERIPH_ON_APB, TEGRA210_CLK_ID_HDA2HDMI),

	PERIPH_CLK_EX("qspi",	"qspi",			NULL,	211,	0x6c4, 163200000, mux_pllp_pllc_pllc_out1_pllc4_out2_pllc4_out1_clkm_pllc4_out0, MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_QSPI, &tegra_qspi_clk_ops),
	PERIPH_CLK("sbc1",	"7000d400.spi",		NULL,	41,	0x134,  65000000, mux_pllp_pllc_clkm,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SBC1),
	PERIPH_CLK("sbc2",	"7000d600.spi",		NULL,	44,	0x118,  65000000, mux_pllp_pllc_clkm,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SBC2),
	PERIPH_CLK("sbc3",	"7000d800.spi",		NULL,	46,	0x11c,  65000000, mux_pllp_pllc_clkm,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SBC3),
	PERIPH_CLK("sbc4",	"7000da00.spi",		NULL,	68,	0x1b4,  65000000, mux_pllp_pllc_clkm,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SBC4),
	PERIPH_CLK("sata_oob",	"tegra_sata_oob",	NULL,	123,	0x420,	204000000, mux_pllp_pllc_clkm,	MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_SATA_OOB),
	PERIPH_CLK("sata_cold",	"tegra_sata_cold",	NULL,	129,	0,	38400000,  mux_clk_m,		PERIPH_ON_APB, TEGRA210_CLK_ID_SATA_COLD),
	PERIPH_CLK("sdmmc1",	"sdhci-tegra.0",	NULL,	14,	0x150,	204000000, mux_pllp_pllc4_out2_pllc4_out1_clkm_pllc4_out0,	MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC1),
	PERIPH_CLK("sdmmc3",	"sdhci-tegra.2",	NULL,	69,	0x1bc,	204000000, mux_pllp_pllc4_out2_pllc4_out1_clkm_pllc4_out0,	MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC3),
	PERIPH_CLK("sdmmc1_ddr", "sdhci-tegra.0",	"ddr",	14,	0x150,	 96000000, mux_pllp_pllc4_out2_pllc4_out1_clkm_pllc4_out0,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC1_DDR),
	PERIPH_CLK("sdmmc3_ddr", "sdhci-tegra.2",	"ddr",	69,	0x1bc,	 96000000, mux_pllp_pllc4_out2_pllc4_out1_clkm_pllc4_out0,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC3_DDR),
	PERIPH_CLK_EX("sdmmc2",	"sdhci-tegra.1",	NULL,	9,	0x154,	266000000, mux_pllp_clk_m_pllc4_out2_out1_out0_lj,	MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC2, &tegra_sdmmc24_clk_ops),
	PERIPH_CLK_EX("sdmmc4",	"sdhci-tegra.3",	NULL,	15,	0x164,	266000000, mux_pllp_clk_m_pllc4_out2_out1_out0_lj,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC4, &tegra_sdmmc24_clk_ops),
	PERIPH_CLK_EX("sdmmc2_ddr", "sdhci-tegra.1",	"ddr",	9,	0x154,	102000000, mux_pllp_clk_m_pllc4_out2_out1_out0_lj,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC2_DDR, &tegra_sdmmc24_clk_ops),
	PERIPH_CLK_EX("sdmmc4_ddr", "sdhci-tegra.3",	"ddr",	15,	0x164,	102000000, mux_pllp_clk_m_pllc4_out2_out1_out0_lj,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC4_DDR, &tegra_sdmmc24_clk_ops),
	PERIPH_CLK("sdmmc_legacy", "sdmmc_legacy",	NULL,	193,	0x694,	208000000, mux_pllp_out3_clkm_pllp_pllc4, MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_SDMMC_LEGACY),

	PERIPH_CLK("vcp",	"nvavp",		"vcp",	29,	0,	250000000, mux_clk_m, 			0, TEGRA210_CLK_ID_VCP),
	PERIPH_CLK("bsev",	"tegra-aes",		"bsev",	63,	0,	250000000, mux_clk_m, 			0, TEGRA210_CLK_ID_BSEV),
	PERIPH_CLK("cec",	"tegra_cec",		NULL,	136,	0,	250000000, mux_clk_m,			PERIPH_ON_APB, TEGRA210_CLK_ID_CEC),
	PERIPH_CLK("csite",	"csite",		NULL,	73,	0x1d4,	624000000, mux_pllp_pllre_clkm,		MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_CSITE),
	PERIPH_CLK("la",	"la",			NULL,	76,	0x1f8,	408000000,  mux_pllp_pllc_clkm,		MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_LA),

	PERIPH_CLK("owr",	"tegra_w1",		NULL,	71,	0x1cc,	38400000,  mux_pllp_pllc_clkm,		MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_OWR),
	PERIPH_CLK("i2c1",	"7000c000.i2c",   "div-clk",	12,	0x124,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB, TEGRA210_CLK_ID_I2C1),
	PERIPH_CLK("i2c2",	"7000c400.i2c",   "div-clk",	54,	0x198,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB, TEGRA210_CLK_ID_I2C2),
	PERIPH_CLK("i2c3",	"7000c500.i2c",   "div-clk",	67,	0x1b8,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB, TEGRA210_CLK_ID_I2C3),
	PERIPH_CLK("i2c4",	"7000c700.i2c",   "div-clk",	103,	0x3c4,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB, TEGRA210_CLK_ID_I2C4),
	PERIPH_CLK("i2c5",	"7000d000.i2c",   "div-clk",	47,	0x128,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB, TEGRA210_CLK_ID_I2C5),
	PERIPH_CLK("i2c6",	"7000d100.i2c",   "div-clk",	166,	0x65c,	136000000, mux_pllp_clkm,	MUX | DIV_U16 | PERIPH_ON_APB, TEGRA210_CLK_ID_I2C6),
	PERIPH_CLK("vii2c",	"546c0000.i2c",   "vii2c",	208,	0x6c8, 136000000, mux_pllp_pllc_clkm,	MUX | DIV_U16 | PERIPH_ON_APB, TEGRA210_CLK_ID_VII2C),
	PERIPH_CLK("i2cslow",	"546c0000.i2c",   "i2cslow",	81,	0x3fc,	50000000,  mux_pllp_pllc_clk32_clkm,	MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_I2CSLOW),
	PERIPH_CLK("mipibif",	"mipibif",		NULL,	173,	0x660,	408000000,  mux_pllp_clkm,	MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_MIPIIF),
	PERIPH_CLK("mipi-cal",	"mipi-cal",		NULL,	56,	0,	102000000,  mux_clk_m, PERIPH_ON_APB, TEGRA210_CLK_ID_MIPI_CAL),
	PERIPH_CLK("uarta",	"70006000.serial",	NULL,	6,	0x178, 1190400000, mux_pllp_pllc_pllc4_out0_pllc4_out1_clkm_pllc4_out2,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB, TEGRA210_CLK_ID_UARTA),
	PERIPH_CLK("uartb",	"70006040.serial",	NULL,	7,	0x17c, 1190400000, mux_pllp_pllc_pllc4_out0_pllc4_out1_clkm_pllc4_out2,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB, TEGRA210_CLK_ID_UARTB),
	PERIPH_CLK("uartc",	"70006200.serial",	NULL,	55,	0x1a0, 1190400000, mux_pllp_pllc_pllc4_out0_pllc4_out1_clkm_pllc4_out2,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB, TEGRA210_CLK_ID_UARTC),
	PERIPH_CLK("uartd",	"70006300.serial",	NULL,	65,	0x1c0, 1190400000, mux_pllp_pllc_pllc4_out0_pllc4_out1_clkm_pllc4_out2,	MUX | DIV_U151 | DIV_U151_UART | PERIPH_ON_APB, TEGRA210_CLK_ID_UARTD),
	PERIPH_CLK("uartape",	"uartape",		NULL,	212,	0x710,	204000000, mux_pllp_pllc_clkm, MUX | DIV_U151 | DIV_U151_UART | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_UARTAPE),

	PERIPH_CLK("vi_sensor",	 NULL,		"vi_sensor",	164,	0x1a8,	408000000, mux_pllc_pllp_plla,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_VI_SENSOR),
	PERIPH_CLK("vi_sensor2", NULL,		"vi_sensor2",	165,	0x658,	408000000, mux_pllc_pllp_plla,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_VI_SENSOR2),
	PERIPH_CLK_EX("dtv",	"dtv",			NULL,	79,	0x1dc,	 38400000, mux_clk_m,			PERIPH_ON_APB,	TEGRA210_CLK_ID_DTV, &tegra_dtv_clk_ops),
	PERIPH_CLK("disp1",	"tegradc.0",		NULL,	27,	0x138, 1500000000, mux_pllp_plld_plld2_clkm,	MUX, TEGRA210_CLK_ID_DISP1),
	PERIPH_CLK("disp2",	"tegradc.1",		NULL,	26,	0x13c, 1500000000, mux_pllp_plld_plld2_clkm,	MUX, TEGRA210_CLK_ID_DISP2),
	PERIPH_CLK_EX("sor0",	NULL,			"sor0",	182,	0x414,	600000000, mux_pllp_sor_sor0_brick,		MUX,	TEGRA210_CLK_ID_SOR0, &tegra_sor0_clk_ops),
	PERIPH_CLK_EX("sor1",	NULL,			"sor1",	183,	0x410,	600000000, mux_pllp_sor_sor1_brick_sor1_src,	MUX,	TEGRA210_CLK_ID_SOR1, &tegra_sor1_clk_ops),
	PERIPH_CLK_EX("dpaux",	"dpaux",		NULL,	181,	0,	 24000000, mux_pllp,			0, TEGRA210_CLK_ID_DPAUX, &tegra_dpaux_clk_ops),
	PERIPH_CLK_EX("dpaux1",	"dpaux1",		NULL,	207,	0,	 24000000, mux_pllp,			0, TEGRA210_CLK_ID_DPAUX1, &tegra_dpaux_clk_ops),

	PERIPH_CLK("usbd",	"tegra-udc.0",		NULL,	22,	0,	480000000, mux_clk_m,			0, TEGRA210_CLK_ID_USBD),
	PERIPH_CLK("usb2",	"tegra-ehci.1",		NULL,	58,	0,	480000000, mux_clk_m,			0, TEGRA210_CLK_ID_USB2),
	PERIPH_CLK("hsic_trk",	NULL,		  "hsic_trk",	209,	0,	38400000, mux_clk_usb2_hsic_trk,	PERIPH_NO_RESET, TEGRA210_CLK_ID_HSIC_TRK),
	PERIPH_CLK("usb2_trk",	NULL,		  "usb2_trk",	210,	0,	38400000, mux_clk_usb2_hsic_trk,	PERIPH_NO_RESET, TEGRA210_CLK_ID_USB2_TRK),

	PERIPH_CLK("dsia",	"tegradc.0",	      "dsia",	48,	0xd0,   750000000, mux_plld_out0,		PLLD,	TEGRA210_CLK_ID_DSIA),
	PERIPH_CLK("dsib",	"tegradc.1",	      "dsib",	82,	0x4b8,  750000000, mux_plld_out0,		PLLD,	TEGRA210_CLK_ID_DSIB),
	PERIPH_CLK("dsi1-fixed", "tegradc.0",    "dsi-fixed",	0,	0,	108000000, mux_pllp_out3,		PERIPH_NO_ENB | PERIPH_NO_RESET, TEGRA210_CLK_ID_DSI1_FIXED),
	PERIPH_CLK("dsi2-fixed", "tegradc.1",    "dsi-fixed",	0,	0,	108000000, mux_pllp_out3,		PERIPH_NO_ENB | PERIPH_NO_RESET, TEGRA210_CLK_ID_DSI2_FIXED),
	PERIPH_CLK("csi",	"vi",		       "csi",	52,	0,      750000000, mux_plld_out0,		PLLD, TEGRA210_CLK_ID_CSI),
	PERIPH_CLK("csus",	"vi",		      "csus",	92,	0,	150000000, mux_clk_m,			PERIPH_NO_RESET, TEGRA210_CLK_ID_CSUS),
	PERIPH_CLK("vim2_clk",	"vi",		  "vim2_clk",	171,	0,	150000000, mux_clk_m,			PERIPH_NO_RESET, TEGRA210_CLK_ID_VIM2_CLK),
	PERIPH_CLK("cilab",	"vi",		     "cilab",	144,	0x614,	102000000, mux_pllp_pllc_clkm,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_CILAB),
	PERIPH_CLK("cilcd",	"vi",		     "cilcd", 	145,	0x618,	102000000, mux_pllp_pllc_clkm,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_CILCD),
	PERIPH_CLK("cile",	"vi",		      "cile",	146,	0x61c,	102000000, mux_pllp_pllc_clkm,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_CILE),
	PERIPH_CLK("dsialp",	"tegradc.0",	    "dsialp",	147,	0x620,	102000000, mux_pllp_pllc_clkm,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_DSIALP),
	PERIPH_CLK("dsiblp",	"tegradc.1",	    "dsiblp",	148,	0x624,	102000000, mux_pllp_pllc_clkm,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_DSIBLP),

	PERIPH_CLK("entropy",	"entropy",		NULL,	149,	0x628,	102000000, mux_pllp_clkm_clk32_plle,		MUX | DIV_U71, TEGRA210_CLK_ID_ENTROPY),
	PERIPH_CLK("uart_mipi_cal", "uart_mipi_cal",	NULL,	177,	0x66c,	102000000, mux_pllp_out3_pllp_pllc_clkm,	MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_UART_MIPI_CAL),
	PERIPH_CLK("dbgapb",	"dbgapb",		NULL,	185,	0x718,	136000000, mux_pllp_clkm_2,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_DBGAPB),
	PERIPH_CLK("tsensor",	"tegra-tsensor",	NULL,	100,	0x3b8,	 19200000, mux_pllp_pllc_clkm_clk32,	MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_TSENSOR),
	PERIPH_CLK("actmon",	"actmon",		NULL,	119,	0x3e8,	204000000, mux_pllp_pllc_clk32_clkm,	MUX | DIV_U71, TEGRA210_CLK_ID_ACTMON),
	PERIPH_CLK("extern1",	"extern1",		NULL,	120,	0x3ec,	408000000, mux_plla_clk32_pllp_clkm_plle,	MUX | DIV_U71, TEGRA210_CLK_ID_EXTERN1),
	PERIPH_CLK("extern2",	"extern2",		NULL,	121,	0x3f0,	408000000, mux_plla_clk32_pllp_clkm_plle,	MUX | DIV_U71, TEGRA210_CLK_ID_EXTERN2),
	PERIPH_CLK("extern3",	"extern3",		NULL,	122,	0x3f4,	408000000, mux_plla_clk32_pllp_clkm_plle,	MUX | DIV_U71, TEGRA210_CLK_ID_EXTERN3),
	PERIPH_CLK("pcie",	"tegra_pcie",		"pcie",	70,	0,	250000000, mux_pex_uphy_clk, PERIPH_MANUAL_RESET, TEGRA210_CLK_ID_PCIE),
	PERIPH_CLK("afi",	"tegra_pcie",		"afi",	72,	0,	408000000, mux_clk_mselect, PERIPH_MANUAL_RESET, TEGRA210_CLK_ID_AFI),
	PERIPH_CLK("cl_dvfs_ref", "tegra_cl_dvfs",	"ref",	155,	0x62c,	54000000,  mux_pllp_clkm,		MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_DFLL_REF),
	PERIPH_CLK("cl_dvfs_soc", "tegra_cl_dvfs",	"soc",	155,	0x630,	54000000,  mux_pllp_clkm,		MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_DFLL_SOC),
	PERIPH_CLK("soc_therm",	"soc_therm",		NULL,   78,	0x644,	136000000, mux_clkm_pllc_pllp_plla,	MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_SOC_THERM),

	PERIPH_CLK("dp2",	"dp2",			NULL,	152,	0,	38400000, mux_clk_m,			PERIPH_ON_APB, TEGRA210_CLK_ID_DP2),
	PERIPH_CLK("mc_bbc",	"mc_bbc",		NULL,	170,	0,	1066000000, mux_clk_mc,			PERIPH_NO_RESET, TEGRA210_CLK_ID_MC_BBC),
	PERIPH_CLK("mc_capa",	"mc_capa",		NULL,	167,	0,	1066000000, mux_clk_mc,			PERIPH_NO_RESET, TEGRA210_CLK_ID_MC_CAPA),
	PERIPH_CLK("mc_cbpa",	"mc_cbpa",		NULL,	168,	0,	1066000000, mux_clk_mc,			PERIPH_NO_RESET, TEGRA210_CLK_ID_MC_CBPA),
	PERIPH_CLK("mc_cpu",	"mc_cpu",		NULL,	169,	0,	1066000000, mux_clk_mc,			PERIPH_NO_RESET, TEGRA210_CLK_ID_MC_CPU),
	PERIPH_CLK("mc_ccpa",	"mc_ccpa",		NULL,	201,	0,	1066000000, mux_clk_mc,			PERIPH_NO_RESET, TEGRA210_CLK_ID_MC_CCPA),
	PERIPH_CLK("mc_cdpa",	"mc_cdpa",		NULL,	200,	0,	1066000000, mux_clk_mc,			PERIPH_NO_RESET, TEGRA210_CLK_ID_MC_CDPA),

	PERIPH_CLK_SKIP("vic03", "vic03",	NULL,	178,	0x678,	0x6f0, 1000000000, mux_pllc_pllp_plla1_pllc2_c3_clkm,	MUX | DIV_U71 | DIV_U71_INT, TEGRA210_CLK_ID_VIC),
	PERIPH_CLK_SKIP("msenc", "msenc",	NULL,	219,	0x6a0,	0x6e8, 1000000000, mux_pllc2_c_c3_pllp_plla1_clkm,	MUX | DIV_U71 | DIV_U71_INT, TEGRA210_CLK_ID_NVENC),
	PERIPH_CLK_SKIP("nvdec", "nvdec",	NULL,	194,	0x698,	0x6f4, 1000000000, mux_pllc2_c_c3_pllp_plla1_clkm,	MUX | DIV_U71 | DIV_U71_INT, TEGRA210_CLK_ID_NVDEC),
	PERIPH_CLK_SKIP("nvjpg", "nvjpg",	NULL,	195,	0x69c,	0x700, 1000000000, mux_pllc2_c_c3_pllp_plla1_clkm,	MUX | DIV_U71 | DIV_U71_INT, TEGRA210_CLK_ID_NVJPG),
	PERIPH_CLK_SKIP("se",	"se",		NULL,	127,	0x42c,	0x704, 1000000000, mux_pllp_pllc2_c_c3_clkm,	MUX | DIV_U71 | DIV_U71_INT | PERIPH_ON_APB, TEGRA210_CLK_ID_SE),
	PERIPH_CLK_SKIP("tsec",	"tegra_tsec",	"tsec",	83,	0x1f4,	0x708,	408000000, mux_pllp_pllc_clkm,		MUX | DIV_U71 | DIV_U71_INT, TEGRA210_CLK_ID_TSEC),
	PERIPH_CLK_SKIP("tsecb", "tsecb",	NULL,	206,	0x6d8,	0x70c, 1000000000, mux_pllp_pllc2_c_c3_clkm,	MUX | DIV_U71 | DIV_U71_INT, TEGRA210_CLK_ID_TSECB),
	PERIPH_CLK_SKIP("ispa",	"isp",		"ispa",	23,	0,	0x6f8, 1000000000, mux_isp,				PERIPH_ON_APB, TEGRA210_CLK_ID_ISPA),
	PERIPH_CLK_SKIP("ispb",	"isp",		"ispb",	3,	0,	0x6fc, 1000000000, mux_isp,				PERIPH_ON_APB, TEGRA210_CLK_ID_ISPB),
	PERIPH_CLK_EX("vi",	"vi",		"vi",	20,	0x148,	       1000000000, mux_pllc2_c_c3_pllp_clkm_plla1_pllc4, MUX | DIV_U71 | DIV_U71_INT, TEGRA210_CLK_ID_VI, &tegra_vi_clk_ops),
	SUPER_SKIP_CLK("vi_skip", "vi",		"skip",		0x6ec,		   NULL, 0),

	SHARED_SCLK("avp.sclk",	"nvavp",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_AVP_USER),
	SHARED_SCLK("usbd.sclk", "tegra-udc.0",		"sclk",	&tegra_clk_ahb, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_USBD_USER),
	SHARED_SCLK("usb1.sclk", "tegra-ehci.0",	"sclk",	&tegra_clk_ahb, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_USBD1_USER),
	SHARED_SCLK("usb2.sclk", "tegra-ehci.1",	"sclk",	&tegra_clk_ahb, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_USBD2_USER),
	SHARED_SCLK("sdmmc4.sclk",    "sdhci-tegra.3",  "sclk", &tegra_clk_ahb, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_SDMMC4_USER),
	SHARED_SCLK("wake.sclk",   "wake_sclk",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_WAKE_USER),
	SHARED_SCLK("camera.sclk", "vi",		"sclk",	&tegra_clk_sbus_cmplx, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_CAMERA_USER),
	SHARED_SCLK("mon.avp",	 "tegra_actmon",	"avp",	&tegra_clk_sbus_cmplx, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_MON_AVP_USER),
	SHARED_SCLK("cap.sclk",	 "cap_sclk",		NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_CEILING, 0),
	SHARED_SCLK("cap.vcore.sclk", "cap.vcore.sclk",	NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_CEILING, 0),
	SHARED_SCLK("cap.throttle.sclk", "cap_throttle", NULL,	&tegra_clk_sbus_cmplx, NULL, 0, SHARED_CEILING, 0),
	SHARED_SCLK("floor.sclk", "floor_sclk",		NULL,	&tegra_clk_sbus_cmplx, NULL, 0, 0, 0),
	SHARED_SCLK("override.sclk", "override_sclk",	NULL,   &tegra_clk_sbus_cmplx, NULL, 0, SHARED_OVERRIDE, 0),
	SHARED_SCLK("sbc1.sclk", "7000d400.spi",	"sclk", &tegra_clk_apb,        NULL, 0, 0, TEGRA210_CLK_ID_SBUS_SBC1_USER),
	SHARED_SCLK("sbc2.sclk", "7000d600.spi",	"sclk", &tegra_clk_apb,        NULL, 0, 0, TEGRA210_CLK_ID_SBUS_SBC2_USER),
	SHARED_SCLK("sbc3.sclk", "7000d800.spi",	"sclk", &tegra_clk_apb,        NULL, 0, 0, TEGRA210_CLK_ID_SBUS_SBC3_USER),
	SHARED_SCLK("sbc4.sclk", "7000da00.spi",	"sclk", &tegra_clk_apb,        NULL, 0, 0, TEGRA210_CLK_ID_SBUS_SBC4_USER),
	SHARED_SCLK("qspi.sclk", "qspi",	        "sclk", &tegra_clk_apb,        NULL, 0, 0, TEGRA210_CLK_ID_SBUS_QSPI_USER),
	SHARED_SCLK("boot.apb.sclk", "boot.apb.sclk",	NULL,	&tegra_clk_apb,        NULL, 0, 0, TEGRA210_CLK_ID_SBUS_BOOT_APB_USER),
	SHARED_SCLK("vcm.sclk",  "vcm",			"sclk", &tegra_clk_sbus_cmplx, NULL, 0, 0, TEGRA210_CLK_ID_SBUS_VCM_USER),
	SHARED_SCLK("vcm.ahb.sclk", "vcm",		"sclk", &tegra_clk_ahb,        NULL, 0, 0, TEGRA210_CLK_ID_SBUS_VCM_AHB_USER),
	SHARED_SCLK("vcm.apb.sclk", "vcm",		"sclk", &tegra_clk_apb,        NULL, 0, 0, TEGRA210_CLK_ID_SBUS_VCM_APB_USER),

	SHARED_EMC_CLK("avp.emc",	"nvavp",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_AVP_USER),
	SHARED_EMC_CLK("cpu.emc",	"tegra-cpu", "cpu_emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_CPU_USER),
	SHARED_EMC_CLK("disp1.emc",	"tegradc.0",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW, BIT(EMC_USER_DC1), TEGRA210_CLK_ID_EMC_DISP1_USER),
	SHARED_EMC_CLK("disp2.emc",	"tegradc.1",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW, BIT(EMC_USER_DC2), TEGRA210_CLK_ID_EMC_DISP2_USER),
	SHARED_EMC_CLK("disp1.la.emc",	"tegradc.0",	"emc.la", &tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_DISP1_LA_USER),
	SHARED_EMC_CLK("disp2.la.emc",	"tegradc.1",	"emc.la", &tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_DISP2_LA_USER),
	SHARED_EMC_CLK("usbd.emc",	"tegra-udc.0",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_USBD_USER),
	SHARED_EMC_CLK("usb1.emc",	"tegra-ehci.0",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_USB1_USER),
	SHARED_EMC_CLK("usb2.emc",	"tegra-ehci.1",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_USB2_USER),
	SHARED_EMC_CLK("sdmmc3.emc",    "sdhci-tegra.2", "emc", &tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_SDMMC3_USER),
	SHARED_EMC_CLK("sdmmc4.emc",    "sdhci-tegra.3", "emc", &tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_SDMMC4_USER),
	SHARED_EMC_CLK("mon.emc",	"tegra_actmon",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_MON_USER),
	SHARED_EMC_CLK("cap.emc",	"cap.emc",	NULL,	&tegra_clk_emc, NULL, 0, SHARED_CEILING, 0, 0),
	SHARED_EMC_CLK("cap.vcore.emc",	"cap.vcore.emc", NULL,	&tegra_clk_emc, NULL, 0, SHARED_CEILING, 0, 0),
	SHARED_EMC_CLK("cap.throttle.emc", "cap_throttle", NULL, &tegra_clk_emc, NULL, 0, SHARED_CEILING_BUT_ISO, 0, 0),
	SHARED_EMC_CLK("3d.emc",	"tegra_gpu.0",	"emc",	&tegra_clk_emc, NULL, 0, 0,		BIT(EMC_USER_3D), TEGRA210_CLK_ID_EMC_3D_USER),
	SHARED_EMC_CLK("msenc.emc",	"tegra_msenc",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_BW,	BIT(EMC_USER_MSENC), TEGRA210_CLK_ID_EMC_NVENC_USER),
	SHARED_EMC_CLK("nvjpg.emc",	"tegra_nvjpg",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_BW,	BIT(EMC_USER_NVJPG), TEGRA210_CLK_ID_EMC_NVJPG_USER),
	SHARED_EMC_CLK("nvdec.emc",	"tegra_nvdec",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_NVDEC_USER),
	SHARED_EMC_CLK("tsec.emc",	"tegra_tsec",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_TSEC_USER),
	SHARED_EMC_CLK("tsecb.emc",	"tegra_tsecb",	"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_TSECB_USER),
	SHARED_EMC_CLK("vi.emc",	"tegra_vi",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW,	BIT(EMC_USER_VI), TEGRA210_CLK_ID_EMC_VI_USER),
	SHARED_EMC_CLK("ispa.emc",	"tegra_isp.0",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW,	BIT(EMC_USER_ISP1), TEGRA210_CLK_ID_EMC_ISPA_USER),
	SHARED_EMC_CLK("ispb.emc",	"tegra_isp.1",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_ISO_BW,	BIT(EMC_USER_ISP2), TEGRA210_CLK_ID_EMC_ISPB_USER),
	SHARED_EMC_CLK("camera.emc", "tegra_camera_ctrl",	"emc",	&tegra_clk_emc, NULL, 0, SHARED_BW, 0, TEGRA210_CLK_ID_EMC_CAMERA_USER),
	SHARED_EMC_CLK("camera_iso.emc", "tegra_camera_ctrl",	"iso.emc",	&tegra_clk_emc,	NULL, 0, SHARED_ISO_BW, 0, TEGRA210_CLK_ID_EMC_CAMERA_ISO_USER),
	SHARED_EMC_CLK("iso.emc",	"iso",		"emc",	&tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_ISO_USER),
	SHARED_EMC_CLK("floor.emc",	"floor.emc",	NULL,	&tegra_clk_emc, NULL, 0, 0, 0, 0),
	SHARED_EMC_CLK("override.emc", "override.emc",	NULL,	&tegra_clk_emc, NULL, 0, SHARED_OVERRIDE, 0, 0),
	SHARED_EMC_CLK("vic.emc",	"tegra_vic03",	"emc",  &tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_VIC_USER),
	SHARED_EMC_CLK("vic_shared.emc",	"tegra_vic03",	"emc_shared",  &tegra_clk_emc, NULL, 0, SHARED_BW, 0, TEGRA210_CLK_ID_EMC_VIC_SHARED_USER),
	SHARED_EMC_CLK("ape.emc", "ape", "emc",  &tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_APE_USER),
	SHARED_EMC_CLK("pcie.emc", "tegra_pcie", "emc",  &tegra_clk_emc, NULL, 0, 0, 0, TEGRA210_CLK_ID_EMC_PCIE_USER),

	DUAL_CBUS_CLK("vic03.cbus",	"tegra_vic03",		"vic03", &tegra_clk_c2bus, "vic03", 0, 0, TEGRA210_CLK_ID_CXBUS_VIC_USER),
	DUAL_CBUS_CLK("nvjpg.cbus",	"tegra_nvjpg",		"nvjpg", &tegra_clk_c2bus, "nvjpg", 0, 0, TEGRA210_CLK_ID_CXBUS_NVJPG_USER),
	DUAL_CBUS_CLK("se.cbus",	"tegra21-se",		NULL,	 &tegra_clk_c2bus, "se",    0, 0, TEGRA210_CLK_ID_CXBUS_SE_USER),
	DUAL_CBUS_CLK("tsecb.cbus",	"tegra_tsecb",		"tsecb", &tegra_clk_c2bus, "tsecb", 0, 0, TEGRA210_CLK_ID_CXBUS_TSECB_USER),
	SHARED_LIMIT("cap.c2bus",	"cap.c2bus",		NULL,	 &tegra_clk_c2bus, NULL,    0, SHARED_CEILING, 0),
	SHARED_LIMIT("cap.vcore.c2bus",	"cap.vcore.c2bus",	NULL,	 &tegra_clk_c2bus, NULL,    0, SHARED_CEILING, 0),
	SHARED_LIMIT("cap.throttle.c2bus", "cap_throttle",	NULL,	 &tegra_clk_c2bus, NULL,    0, SHARED_CEILING, 0),
	SHARED_LIMIT("floor.c2bus",	"floor.c2bus",		NULL,	 &tegra_clk_c2bus, NULL,    0, 0, 0),
	SHARED_CLK("override.c2bus",	"override.c2bus",	NULL,	 &tegra_clk_c2bus, NULL,    0, SHARED_OVERRIDE, 0),
	SHARED_LIMIT("edp.c2bus",	"edp.c2bus",            NULL,    &tegra_clk_c2bus, NULL,    0, SHARED_CEILING, 0),
	SHARED_LIMIT("vic_floor.cbus",	"tegra_vic03",		"vic_floor", &tegra_clk_c2bus, NULL, 0, 0, TEGRA210_CLK_ID_CXBUS_VIC_FLOOR_USER),

	DUAL_CBUS_CLK("msenc.cbus",	"tegra_msenc",		"msenc", &tegra_clk_c3bus, "msenc", 0, 0, TEGRA210_CLK_ID_CXBUS_NVENC_USER),
	DUAL_CBUS_CLK("nvdec.cbus",	"tegra_nvdec",		"nvdec", &tegra_clk_c3bus, "nvdec", 0, 0, TEGRA210_CLK_ID_CXBUS_NVDEC_USER),
	SHARED_LIMIT("cap.c3bus",	"cap.c3bus",		NULL,	 &tegra_clk_c3bus, NULL,    0, SHARED_CEILING, 0),
	SHARED_LIMIT("cap.vcore.c3bus",	"cap.vcore.c3bus",	NULL,	 &tegra_clk_c3bus, NULL,    0, SHARED_CEILING, 0),
	SHARED_LIMIT("cap.throttle.c3bus", "cap_throttle",	NULL,	 &tegra_clk_c3bus, NULL,    0, SHARED_CEILING, 0),
	SHARED_LIMIT("floor.c3bus",	"floor.c3bus",		NULL,	 &tegra_clk_c3bus, NULL,    0, 0, 0),
	SHARED_CLK("override.c3bus",	"override.c3bus",	NULL,	 &tegra_clk_c3bus, NULL,    0, SHARED_OVERRIDE, 0),

	SHARED_CLK("gm20b.gbus",	"tegra_gpu.0",		"gpu",	&tegra_clk_gbus, NULL,  0, 0, TEGRA210_CLK_ID_GBUS_GM20B_USER),
	SHARED_LIMIT("cap.gbus",	"cap.gbus",		NULL,	&tegra_clk_gbus, NULL,  0, SHARED_CEILING, 0),
	SHARED_LIMIT("edp.gbus",	"edp.gbus",		NULL,	&tegra_clk_gbus, NULL,  0, SHARED_CEILING, 0),
	SHARED_LIMIT("cap.vgpu.gbus",	"cap.vgpu.gbus",	NULL,	&tegra_clk_gbus, NULL,  0, SHARED_CEILING, 0),
	SHARED_LIMIT("cap.throttle.gbus", "cap_throttle",	NULL,	&tegra_clk_gbus, NULL,  0, SHARED_CEILING, 0),
	SHARED_LIMIT("cap.profile.gbus", "profile.gbus",	"cap",	&tegra_clk_gbus, NULL,  0, SHARED_CEILING, 0),
	SHARED_CLK("override.gbus",	"override.gbus",	NULL,	&tegra_clk_gbus, NULL,  0, SHARED_OVERRIDE, 0),
	SHARED_LIMIT("floor.gbus",	"floor.gbus",		NULL,	&tegra_clk_gbus, NULL,  0, 0, 0),
	SHARED_LIMIT("floor.profile.gbus", "profile.gbus",	"floor", &tegra_clk_gbus, NULL, 0, 0, 0),

	SHARED_CLK("nv.host1x",	"tegra_host1x",		"host1x", &tegra_clk_host1x, NULL,  0, 0, TEGRA210_CLK_ID_HOST1X_NV_USER),
	SHARED_CLK("vi.host1x",	"tegra_vi",		"host1x", &tegra_clk_host1x, NULL,  0, 0, TEGRA210_CLK_ID_HOST1X_VI_USER),
	SHARED_CLK("vii2c.host1x", "546c0000.i2c",	"host1x", &tegra_clk_host1x, NULL,  0, 0, TEGRA210_CLK_ID_HOST1X_VII2C_USER),
	SHARED_LIMIT("cap.host1x", "cap.host1x",	NULL,	  &tegra_clk_host1x, NULL,  0, SHARED_CEILING, 0),
	SHARED_LIMIT("cap.vcore.host1x", "cap.vcore.host1x", NULL, &tegra_clk_host1x, NULL, 0, SHARED_CEILING, 0),
	SHARED_LIMIT("floor.host1x", "floor.host1x",	NULL,	  &tegra_clk_host1x, NULL,  0, 0, 0),
	SHARED_CLK("override.host1x", "override.host1x", NULL,	  &tegra_clk_host1x, NULL,  0, SHARED_OVERRIDE, 0),

	SHARED_CLK("cpu.mselect",	  "cpu",        "mselect",   &tegra_clk_mselect, NULL,  0, 0, TEGRA210_CLK_ID_MSELECT_CPU_USER),
	SHARED_CLK("pcie.mselect",	  "tegra_pcie", "mselect",   &tegra_clk_mselect, NULL,  0, 0, TEGRA210_CLK_ID_MSELECT_PCIE_USER),
	SHARED_LIMIT("cap.vcore.mselect", "cap.vcore.mselect", NULL, &tegra_clk_mselect, NULL,  0, SHARED_CEILING, 0),
	SHARED_CLK("override.mselect",    "override.mselect",  NULL, &tegra_clk_mselect, NULL,  0, SHARED_OVERRIDE, 0),

	SHARED_CLK("adma.ape",		NULL,           "adma.ape",  &tegra_clk_ape, NULL,  0, 0, TEGRA210_CLK_ID_APE_ADMA_USER),
	SHARED_CLK("adsp.ape",		NULL,           "adsp.ape",  &tegra_clk_ape, NULL,  0, 0, TEGRA210_CLK_ID_APE_ADSP_USER),
	SHARED_CLK("xbar.ape",		NULL,           "xbar.ape",  &tegra_clk_ape, NULL,  0, 0, TEGRA210_CLK_ID_APE_XBAR_USER),
	SHARED_LIMIT("cap.vcore.ape",	"cap.vcore.ape", NULL,       &tegra_clk_ape, NULL,  0, SHARED_CEILING, 0),
	SHARED_CLK("override.ape",	"override.ape",  NULL,       &tegra_clk_ape, NULL,  0, SHARED_OVERRIDE, 0),

	SHARED_CLK("adsp_cpu.abus",	NULL,		  "adsp_cpu",	&tegra_clk_virtual_adsp_bus, NULL,  0, 0, TEGRA210_CLK_ID_ADSP_CPU),
	SHARED_LIMIT("cap.vcore.abus",	"cap.vcore.abus", NULL,		&tegra_clk_virtual_adsp_bus, NULL,  0, SHARED_CEILING, 0),
	SHARED_CLK("override.abus",	"override.abus",  NULL,		&tegra_clk_virtual_adsp_bus, NULL,  0, SHARED_OVERRIDE, 0),
};

/* VI, ISP buses */
static struct clk tegra_visp_clks[] = {
	SHARED_CONNECT("vi.cbus",	"vi.cbus",	NULL,	&tegra_clk_cbus,   "vi",    0, 0, TEGRA210_CLK_ID_CXBUS_VI_USER),
	SHARED_CONNECT("isp.cbus",	"isp.cbus",	NULL,	&tegra_clk_cbus,   "isp",   0, 0, TEGRA210_CLK_ID_CXBUS_ISP_USER),
	SHARED_CLK("override.cbus",	"override.cbus", NULL,	&tegra_clk_cbus,    NULL,   0, SHARED_OVERRIDE, 0),
	SHARED_LIMIT("cap.vcore.cbus",	"cap.vcore.cbus", NULL,	&tegra_clk_cbus,    NULL,   0, SHARED_CEILING, 0),

	SHARED_CLK("ispa.isp.cbus",	"ispa.isp",	NULL,	&tegra_visp_clks[1], "ispa", 0, 0, TEGRA210_CLK_ID_CXBUS_ISP_ISPA_USER),
	SHARED_CLK("ispb.isp.cbus",	"ispb.isp",	NULL,	&tegra_visp_clks[1], "ispb", 0, 0, TEGRA210_CLK_ID_CXBUS_ISP_ISPB_USER),
};

/* XUSB clocks */
#define XUSB_ID "tegra-xhci"
#define XUDC_ID "tegra-xudc"
/* xusb common clock gate - enabled on init and never disabled */
static void tegra21_xusb_gate_clk_init(struct clk *c)
{
	tegra21_periph_clk_enable(c);
}

static struct clk_ops tegra_xusb_gate_clk_ops = {
	.init    = tegra21_xusb_gate_clk_init,
};

static struct clk tegra_clk_xusb_gate = {
	.name      = "xusb_gate",
	.clk_id    = TEGRA210_CLK_ID_XUSB_GATE,
	.flags     = ENABLE_ON_INIT | PERIPH_NO_RESET,
	.ops       = &tegra_xusb_gate_clk_ops,
	.parent    = &tegra_clk_osc,
	.rate      = 12000000,
	.max_rate  = 48000000,
	.u.periph = {
		.clk_num   = 143,
	},
};

static struct clk tegra_xusb_source_clks[] = {
	PERIPH_CLK("xusb_host_src",	XUSB_ID, "host_src",	143,	0x600,	112000000, mux_clkm_pllp_pllre,		MUX | DIV_U71 | PERIPH_NO_RESET | PERIPH_ON_APB, TEGRA210_CLK_ID_XUSB_HOST_SRC),
	PERIPH_CLK("xusb_falcon_src",	XUSB_ID, "falcon_src",	143,	0x604,	336000000, mux_clkm_pllp_pllre,		MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_XUSB_FALCON_SRC),
	PERIPH_CLK("xusb_fs_src",	NULL, "fs_src",		143,	0x608,	 48000000, mux_clkm_48M_pllp_480M,	MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_XUSB_FS_SRC),
	PERIPH_CLK_EX("xusb_ss_src",	NULL, "ss_src",		143,	0x610,	120000000, mux_clkm_pllre_clk32_480M,	MUX | DIV_U71 | PERIPH_NO_RESET, TEGRA210_CLK_ID_XUSB_SS_SRC, &tegra_xusb_ss_ops),
	PERIPH_CLK("xusb_dev_src",	XUDC_ID, "dev_src",	95,	0x60c,	112000000, mux_clkm_pllp_pllre,		MUX | DIV_U71 | PERIPH_ON_APB, TEGRA210_CLK_ID_XUSB_DEV_SRC),
	SHARED_EMC_CLK("xusb.emc",	XUSB_ID, "emc",	&tegra_clk_emc,	NULL,	0,	SHARED_BW, 0, TEGRA210_CLK_ID_EMC_XUSB_USER),
};

static struct clk_mux_sel mux_ss_clk_m[] = {
	{ .input = &tegra_xusb_source_clks[3], .value = 0},
	{ .input = &tegra_clk_m,	       .value = 1},
	{ NULL, 0},
};

static struct clk tegra_xusb_ssp_src = {
	.name      = "xusb_ssp_src",
	.clk_id    = TEGRA210_CLK_ID_XUSB_SSP_SRC,
	.lookup    = {
		.dev_id    = NULL,
		.con_id	   = "ssp_src",
	},
	.ops       = &tegra_xusb_ss_ops,
	.reg       = 0x610,
	.inputs    = mux_ss_clk_m,
	.flags     = MUX | PERIPH_NO_ENB | PERIPH_NO_RESET,
	.max_rate  = 120000000,
	.u.periph = {
		.src_mask  = 0x1 << 24,
		.src_shift = 24,
	},
};

static struct clk tegra_xusb_ss_div2 = {
	.name      = "xusb_ss_div2",
	.clk_id    = TEGRA210_CLK_ID_XUSB_SS_DIV2,
	.ops       = &tegra_clk_m_div_ops,
	.parent    = &tegra_xusb_source_clks[3],
	.mul       = 1,
	.div       = 2,
	.state     = OFF,
	.max_rate  = 60000000,
};

static struct clk_mux_sel mux_ss_div2_pllu_60M[] = {
	{ .input = &tegra_xusb_ss_div2, .value = 0},
	{ .input = &tegra_pll_u_60M,    .value = 1},
	{ .input = &tegra_xusb_source_clks[3], .value = 2},
	{ .input = &tegra_xusb_source_clks[3], .value = 3},
	{ NULL, 0},
};

static struct clk tegra_xusb_hs_src = {
	.name      = "xusb_hs_src",
	.clk_id    = TEGRA210_CLK_ID_XUSB_HS_SRC,
	.lookup    = {
		.dev_id    = NULL,
		.con_id	   = "hs_src",
	},
	.ops       = &tegra_xusb_ss_ops,
	.reg       = 0x610,
	.inputs    = mux_ss_div2_pllu_60M,
	.flags     = MUX | PLLU | PERIPH_NO_ENB | PERIPH_NO_RESET,
	.max_rate  = 120000000,
	.u.periph = {
		.src_mask  = 0x3 << 25,
		.src_shift = 25,
	},
};

static struct clk_mux_sel mux_xusb_host[] = {
	{ .input = &tegra_xusb_source_clks[0], .value = 0},
	{ .input = &tegra_xusb_source_clks[1], .value = 1},
	{ .input = &tegra_xusb_source_clks[2], .value = 2},
	{ .input = &tegra_xusb_hs_src,         .value = 5},
	{ NULL, 0},
};

static struct clk_mux_sel mux_xusb_ss[] = {
	{ .input = &tegra_xusb_ssp_src,	       .value = 3},
	{ .input = &tegra_xusb_source_clks[0], .value = 0},
	{ .input = &tegra_xusb_source_clks[1], .value = 1},
	{ NULL, 0},
};

static struct clk_mux_sel mux_xusb_dev[] = {
	{ .input = &tegra_xusb_source_clks[4], .value = 4},
	{ .input = &tegra_xusb_source_clks[2], .value = 2},
	{ .input = &tegra_xusb_ssp_src,	       .value = 3},
	{ .input = &tegra_xusb_hs_src,         .value = 5},
	{ NULL, 0},
};

static struct clk tegra_xusb_coupled_clks[] = {
	PERIPH_CLK_EX("xusb_host", XUSB_ID, "host", 89,	0, 336000000, mux_xusb_host, 0,	TEGRA210_CLK_ID_XUSB_HOST, &tegra_clk_coupled_gate_ops),
	PERIPH_CLK_EX("xusb_ss",   NULL,    "ss",  156,	0, 336000000, mux_xusb_ss,   0,	TEGRA210_CLK_ID_XUSB_SS, &tegra_clk_coupled_gate_ops),
	PERIPH_CLK_EX("xusb_dev",  XUDC_ID, "dev",  95, 0, 120000000, mux_xusb_dev,  0,	TEGRA210_CLK_ID_XUSB_DEV, &tegra_clk_coupled_gate_ops),
};

#define SLCG_CLK(_root_name, _name_ext, _dev, _con, _reg, _bit)		\
	{								\
		.name      = _root_name "_slcg_ovr" _name_ext,		\
		.lookup	= {						\
			.dev_id	= _dev,					\
			.con_id	= _con,					\
		},							\
		.ops       = &tegra_clk_slcg_ops,			\
		.reg       = _reg,					\
		.max_rate  = 38400000, /* replaced by root max */	\
		.u.periph = {						\
			.clk_num   = _bit,				\
		},							\
	}

static struct clk tegra_slcg_clks[] = {
	SLCG_CLK("disp2",	"",	"tegradc.1",	"slcg",		0xf8,	2),
	SLCG_CLK("disp1",	"",	"tegradc.0",	"slcg",		0xf8,	1),

	SLCG_CLK("vi",		"",	"tegra_vi",	"slcg",		0xf8,	15),
	SLCG_CLK("ispa",	"",	"tegra_isp.0",	"slcg",		0x554,	3),
	SLCG_CLK("ispb",	"",	"tegra_isp.1",	"slcg",		0x3a4,	22),

	SLCG_CLK("nvdec",	"",	"tegra_nvdec",	"slcg",		0x554,	31),
	SLCG_CLK("msenc",	"",	"tegra_msenc",	"slcg",		0x554,	29),
	SLCG_CLK("nvjpg",	"",	"tegra_nvjpg",	"slcg",		0x554,	9),
	SLCG_CLK("vic03",	"",	"tegra_vic03",	"slcg",		0x554,	5),

	SLCG_CLK("xusb_dev",	"",	XUDC_ID,	"slcg",		0x3a0,	31),
	SLCG_CLK("xusb_host",	"",	XUSB_ID,	"slcg",		0x3a0,	30),
	SLCG_CLK("sata",	"_fpci", "tegra_sata",	"slcg_fpci",	0x3a0,	19),
	SLCG_CLK("sata",	"_ipfs", "tegra_sata",	"slcg_ipfs",	0x3a0,	17),
	SLCG_CLK("sata",	"",	"tegra_sata",	"slcg",		0x3a0,	0),

	SLCG_CLK("d_audio",	"",   "tegra210-axbar", "slcg",		0x3a0,	1),
	SLCG_CLK("adsp",	"",	NULL,		"slcg_adsp",	0x554,	11),
	SLCG_CLK("ape",		"",	NULL,		"slcg_ape",	0x554,	10),
};

#define CLK_DUPLICATE(_name, _dev, _con)		\
	{						\
		.name	= _name,			\
		.lookup	= {				\
			.dev_id	= _dev,			\
			.con_id	= _con,			\
		},					\
	}

/* Some clocks may be used by different drivers depending on the board
 * configuration.  List those here to register them twice in the clock lookup
 * table under two names.
 */
static struct clk_duplicate tegra_clk_duplicates[] = {
	CLK_DUPLICATE("uarta",  "serial8250.0", NULL),
	CLK_DUPLICATE("uartb",  "serial8250.1", NULL),
	CLK_DUPLICATE("uartc",  "serial8250.2", NULL),
	CLK_DUPLICATE("uartd",  "serial8250.3", NULL),
	CLK_DUPLICATE("uarta",  "70006000.serial", "serial"),
	CLK_DUPLICATE("uartb",  "70006040.serial", "serial"),
	CLK_DUPLICATE("uartc",  "70006200.serial", "serial"),
	CLK_DUPLICATE("uartd",  "70006300.serial", "serial"),
	CLK_DUPLICATE("pll_p",  "70006000.serial", "parent"),
	CLK_DUPLICATE("pll_p",  "70006040.serial", "parent"),
	CLK_DUPLICATE("pll_p",  "70006200.serial", "parent"),
	CLK_DUPLICATE("pll_p",  "70006300.serial", "parent"),
	CLK_DUPLICATE("usbd", XUSB_ID, "utmip-pad"),
	CLK_DUPLICATE("usbd", "utmip-pad", NULL),
	CLK_DUPLICATE("usbd", "tegra-ehci.0", NULL),
	CLK_DUPLICATE("usbd", "tegra-otg", NULL),
	CLK_DUPLICATE("disp1", "tegra_dc_dsi_vs1.0", NULL),
	CLK_DUPLICATE("disp1.emc", "tegra_dc_dsi_vs1.0", "emc"),
	CLK_DUPLICATE("disp1", "tegra_dc_dsi_vs1.1", NULL),
	CLK_DUPLICATE("disp1.emc", "tegra_dc_dsi_vs1.1", "emc"),
	CLK_DUPLICATE("dsib", "tegradc.0", "dsib"),
	CLK_DUPLICATE("dsia", "tegradc.1", "dsia"),
	CLK_DUPLICATE("dsiblp", "tegradc.0", "dsiblp"),
	CLK_DUPLICATE("dsialp", "tegradc.1", "dsialp"),
	CLK_DUPLICATE("dsia", "tegra_dc_dsi_vs1.0", "dsia"),
	CLK_DUPLICATE("dsia", "tegra_dc_dsi_vs1.1", "dsia"),
	CLK_DUPLICATE("dsialp", "tegra_dc_dsi_vs1.0", "dsialp"),
	CLK_DUPLICATE("dsialp", "tegra_dc_dsi_vs1.1", "dsialp"),
	CLK_DUPLICATE("dsi1-fixed", "tegra_dc_dsi_vs1.0", "dsi-fixed"),
	CLK_DUPLICATE("dsi1-fixed", "tegra_dc_dsi_vs1.1", "dsi-fixed"),
	CLK_DUPLICATE("pwm", "tegra_pwm.0", NULL),
	CLK_DUPLICATE("pwm", "tegra_pwm.1", NULL),
	CLK_DUPLICATE("pwm", "tegra_pwm.2", NULL),
	CLK_DUPLICATE("pwm", "tegra_pwm.3", NULL),
	CLK_DUPLICATE("cop", "nvavp", "cop"),
	CLK_DUPLICATE("bsev", "nvavp", "bsev"),
	CLK_DUPLICATE("cml1", "tegra_sata_cml", NULL),
	CLK_DUPLICATE("pciex", "tegra_pcie", "pciex"),
	CLK_DUPLICATE("pex_uphy", "tegra_pcie", "pex_uphy"),
	CLK_DUPLICATE("clk_m", NULL, "apb_pclk"),
	CLK_DUPLICATE("i2c1", "tegra-i2c-slave.0", NULL),
	CLK_DUPLICATE("i2c2", "tegra-i2c-slave.1", NULL),
	CLK_DUPLICATE("i2c3", "tegra-i2c-slave.2", NULL),
	CLK_DUPLICATE("i2c4", "tegra-i2c-slave.3", NULL),
	CLK_DUPLICATE("i2c5", "tegra-i2c-slave.4", NULL),

	CLK_DUPLICATE("i2c1",	"7000c000.i2c",   "i2c"),
	CLK_DUPLICATE("i2c2",	"7000c400.i2c",   "i2c"),
	CLK_DUPLICATE("i2c3",	"7000c500.i2c",   "i2c"),
	CLK_DUPLICATE("i2c4",	"7000c700.i2c",   "i2c"),
	CLK_DUPLICATE("i2c5",	"7000d000.i2c",   "i2c"),
	CLK_DUPLICATE("i2c6",	"7000d100.i2c",   "i2c"),
	CLK_DUPLICATE("pll_p",	"7000c000.i2c",   "parent"),
	CLK_DUPLICATE("pll_p",	"7000c400.i2c",   "parent"),
	CLK_DUPLICATE("pll_p",	"7000c500.i2c",   "parent"),
	CLK_DUPLICATE("pll_p",	"7000c700.i2c",   "parent"),
	CLK_DUPLICATE("pll_p",	"7000d000.i2c",   "parent"),
	CLK_DUPLICATE("pll_p",	"7000d100.i2c",   "parent"),

	CLK_DUPLICATE("afi",	"1003000.pcie-controller",   "afi"),
	CLK_DUPLICATE("pcie",	"1003000.pcie-controller",   "pcie"),
	CLK_DUPLICATE("pciex",	"1003000.pcie-controller",   "pciex"),

	CLK_DUPLICATE("sbc1",	"7000d400.sbc",   "spi"),
	CLK_DUPLICATE("sbc2",	"7000d600.sbc",   "spi"),
	CLK_DUPLICATE("sbc3",	"7000d800.sbc",   "spi"),
	CLK_DUPLICATE("sbc4",	"7000da00.sbc",   "spi"),
	CLK_DUPLICATE("sdmmc1",	"sdhci-tegra.0",   "sdmmc"),
	CLK_DUPLICATE("sdmmc2",	"sdhci-tegra.1",   "sdmmc"),
	CLK_DUPLICATE("sdmmc3",	"sdhci-tegra.2",   "sdmmc"),
	CLK_DUPLICATE("sdmmc4",	"sdhci-tegra.3",   "sdmmc"),

	CLK_DUPLICATE("cl_dvfs_ref", "7000d000.i2c", NULL),
	CLK_DUPLICATE("cl_dvfs_soc", "7000d000.i2c", NULL),
	CLK_DUPLICATE("sbc1", "tegra11-spi-slave.0", NULL),
	CLK_DUPLICATE("sbc2", "tegra11-spi-slave.1", NULL),
	CLK_DUPLICATE("sbc3", "tegra11-spi-slave.2", NULL),
	CLK_DUPLICATE("sbc4", "tegra11-spi-slave.3", NULL),
	CLK_DUPLICATE("pll_p", "7000d400.spi", "pll_p"),
	CLK_DUPLICATE("pll_p", "7000d600.spi", "pll_p"),
	CLK_DUPLICATE("pll_p", "7000d800.spi", "pll_p"),
	CLK_DUPLICATE("pll_p", "7000da00.spi", "pll_p"),
	CLK_DUPLICATE("pll_p", "7000dc00.spi", "pll_p"),
	CLK_DUPLICATE("pll_p", "7000de00.spi", "pll_p"),
	CLK_DUPLICATE("clk_m", "7000d400.spi", "clk_m"),
	CLK_DUPLICATE("clk_m", "7000d600.spi", "clk_m"),
	CLK_DUPLICATE("clk_m", "7000d800.spi", "clk_m"),
	CLK_DUPLICATE("clk_m", "7000da00.spi", "clk_m"),
	CLK_DUPLICATE("clk_m", "7000dc00.spi", "clk_m"),
	CLK_DUPLICATE("clk_m", "7000de00.spi", "clk_m"),
	CLK_DUPLICATE("i2c5", "tegra_cl_dvfs", "i2c"),
	CLK_DUPLICATE("cpu_g", "tegra_cl_dvfs", "safe_dvfs"),
	CLK_DUPLICATE("actmon", "tegra_host1x", "actmon"),
	CLK_DUPLICATE("gpu_ref", "tegra_gpu.0", "PLLG_ref"),
	CLK_DUPLICATE("pll_p_out5", "tegra_gpu.0", "pwr"),
	CLK_DUPLICATE("ispa.isp.cbus", "tegra_isp.0", "isp"),
	CLK_DUPLICATE("ispb.isp.cbus", "tegra_isp.1", "isp"),
	CLK_DUPLICATE("vii2c", "tegra_vi", "vii2c"),
	CLK_DUPLICATE("i2cslow", "tegra_vi", "i2cslow"),
	CLK_DUPLICATE("mclk3", NULL, "cam_mclk1"),
	CLK_DUPLICATE("mclk", NULL, "cam_mclk2"),
	CLK_DUPLICATE("mclk2", NULL, "cam_mclk3"),
	CLK_DUPLICATE("vi.cbus", "tegra_vi", "vi"),
	CLK_DUPLICATE("csi", "tegra_vi", "csi"),
	CLK_DUPLICATE("csus", "tegra_vi", "csus"),
	CLK_DUPLICATE("vim2_clk", "tegra_vi", "vim2_clk"),
	CLK_DUPLICATE("cilab", "tegra_vi", "cilab"),
	CLK_DUPLICATE("cilcd", "tegra_vi", "cilcd"),
	CLK_DUPLICATE("cile", "tegra_vi", "cile"),
	CLK_DUPLICATE("spdif_in", NULL, "spdif_in"),
	CLK_DUPLICATE("dmic1", "tegra-dmic.0", NULL),
	CLK_DUPLICATE("dmic2", "tegra-dmic.1", NULL),
	CLK_DUPLICATE("dmic3", "tegra-dmic.2", NULL),
	CLK_DUPLICATE("d_audio", "tegra210-adma", NULL),
	CLK_DUPLICATE("d_audio", "nvadsp", "ahub"),
	CLK_DUPLICATE("d_audio", "tegra210-adsp", "ahub"),
	CLK_DUPLICATE("mclk", NULL, "default_mclk"),
	CLK_DUPLICATE("uart_mipi_cal", "clk72mhz", NULL),
	CLK_DUPLICATE("tsec", "tsec", NULL),
	CLK_DUPLICATE("ispa", "isp.0", NULL),
	CLK_DUPLICATE("ispb", "isp.1", NULL),
};


static struct clk *tegra_main_clks[] = {
	&tegra_clk_32k,
	&tegra_clk_osc,
	&tegra_clk_m,
};

static struct clk *tegra_ptr_clks[] = {
	&tegra_clk_m_div2,
	&tegra_clk_m_div4,
	&tegra_pll_ref,
	&tegra_pll_m,
	&tegra_pll_mb,
	&tegra_pll_c,
	&tegra_pll_c_out1,
	&tegra_pll_c2,
	&tegra_pll_c3,
	&tegra_pll_a1,
	&tegra_pll_p,
	&tegra_pll_p_out_adsp,
	&tegra_pll_p_out_cpu,
	&tegra_pll_p_out_hsio,
	&tegra_pll_p_out_xusb,
	&tegra_pll_p_out2,
	&tegra_pll_p_out3,
	&tegra_pll_p_out4,
	&tegra_pll_p_out5,
	&tegra_pll_p_out_sor,
	&tegra_pll_a,
	&tegra_pll_a_out_adsp,
	&tegra_pll_a_out0,
	&tegra_pll_a_out0_out_adsp,
	&tegra_pll_d,
	&tegra_pll_d_out0,
	&tegra_clk_xusb_gate,
	&tegra_pll_u_vco,
	&tegra_pll_u_out,
	&tegra_pll_u_out1,
	&tegra_pll_u_out2,
	&tegra_pll_u_480M,
	&tegra_pll_u_60M,
	&tegra_pll_u_48M,
	&tegra_pll_x,
	&tegra_dfll_cpu,
	&tegra_pll_d2,
	&tegra_pll_c4_vco,
	&tegra_pll_c4_out0,
	&tegra_pll_c4_out1,
	&tegra_pll_c4_out2,
	&tegra_pll_c4_out3,
	&tegra_pll_dp,
	&tegra_pll_re_vco,
	&tegra_pll_re_out,
	&tegra_pll_re_out1,
	&tegra_pll_e,
	&tegra_pll_e_hw,
	&tegra_pll_e_gate,
	&tegra_cml1_clk,
	&tegra_pciex_clk,
	&tegra_pex_uphy_clk,
	&tegra_clk_cclk_g,
	&tegra_clk_cclk_lp,
	&tegra_clk_sclk_mux,
	&tegra_clk_sclk_div,
	&tegra_clk_sclk,
	&tegra_clk_hclk,
	&tegra_clk_pclk,
	&tegra_clk_aclk_adsp,
	&tegra_clk_virtual_adsp_bus,
	&tegra_clk_virtual_cpu_g,
	&tegra_clk_virtual_cpu_lp,
	&tegra_clk_cpu_cmplx,
	&tegra_clk_blink,
	&tegra_clk_cop,
	&tegra_clk_sbus_cmplx,
	&tegra_clk_ahb,
	&tegra_clk_apb,
	&tegra_clk_emc,
	&tegra_clk_mc,
	&tegra_clk_host1x,
	&tegra_clk_mselect,
	&tegra_clk_ape,
	&tegra_clk_c2bus,
	&tegra_clk_c3bus,
	&tegra_clk_gpu_gate,
	&tegra_clk_gpu_ref,
	&tegra_clk_gbus,
	&tegra_clk_isp,
	&tegra_clk_cbus,
	&tegra_clk_sor1_src,
	&tegra_clk_sor0_brick,
	&tegra_clk_sor1_brick,
	&tegra_clk_xusb_padctl,
	&tegra_clk_sata,
	&tegra_sata_aux_clk,
	&tegra_clk_sata_uphy,
	&tegra_clk_usb2_hsic_trk,
	&tegra_clk_hda2codec_2x,
};

static struct clk *tegra_ptr_camera_mclks[] = {
	&tegra_camera_mclk,
	&tegra_camera_mclk2,
	&tegra_camera_mclk3,
	&tegra_camera_mipical,
};

#ifdef CONFIG_DEBUG_FS
static struct tegra_pto_table ptodefs[] = {
	{ .name = "pll_c",  .divider = 2, .pto_id = 1,   .presel_reg = 0x088, .presel_value = BIT(3),  .presel_mask = BIT(3) },
	{ .name = "pll_a1", .divider = 2, .pto_id = 85,  .presel_reg = 0x6a8, .presel_value = BIT(3),  .presel_mask = BIT(3) },
	{ .name = "pll_c2", .divider = 2, .pto_id = 88,  .presel_reg = 0x4ec, .presel_value = BIT(3),  .presel_mask = BIT(3) },
	{ .name = "pll_c3", .divider = 2, .pto_id = 89,  .presel_reg = 0x500, .presel_value = BIT(3),  .presel_mask = BIT(3) },

	{ .name = "pll_a",  .divider = 2, .pto_id = 4,   .presel_reg = 0x0bc, .presel_value = BIT(29), .presel_mask = BIT(29) },
	{ .name = "pll_x",  .divider = 2, .pto_id = 5,   .presel_reg = 0x0e4, .presel_value = BIT(22), .presel_mask = BIT(22) },

	{ .name = "pll_d",  .divider = 2, .pto_id = 203, .presel_reg = 0x0dc, .presel_value = BIT(25), .presel_mask = BIT(25) },
	{ .name = "pll_d2", .divider = 2, .pto_id = 205, .presel_reg = 0x4b8, .presel_value = BIT(16), .presel_mask = BIT(16) },
	{ .name = "pll_dp", .divider = 2, .pto_id = 207, .presel_reg = 0x590, .presel_value = BIT(16), .presel_mask = BIT(16) },
	{ .name = "pll_c4", .divider = 2, .pto_id = 81,  .presel_reg = 0x5a4, .presel_value = BIT(16), .presel_mask = BIT(16) },

	{ .name = "pll_m",  .divider = 2, .pto_id = 2,   .presel_reg = 0x9c,  .presel_value = BIT(8),  .presel_mask = BIT(8) },
	{ .name = "pll_mb", .divider = 2, .pto_id = 37,  .presel_reg = 0x9c,  .presel_value = BIT(9),  .presel_mask = BIT(9) },

	{ .name = "pll_u", .divider = 2,  .pto_id = 269, .presel_reg = 0xcc,  .presel_value = BIT(27), .presel_mask = BIT(27) },
	{ .name = "pll_re_vco", .divider = 2, .pto_id = 271, .presel_reg = 0x4c8, .presel_value = BIT(26), .presel_mask = BIT(26) },

	{ .name = "cpu",    .divider = 1,  .pto_id = 18, },

	{ .name = NULL, },
};
#endif

/*
 * Handle special clocks to check if they can be set to safe rate
 */
static void check_isp_reset(void)
{
	struct clk *isp = &tegra_clk_isp;
	struct clk *ispa = tegra_get_clock_by_name("ispa");
	struct clk *ispb = tegra_get_clock_by_name("ispb");

	if (periph_clk_can_force_safe_rate(ispa) &&
	    periph_clk_can_force_safe_rate(ispb))
		tegra_periph_clk_safe_rate_init(isp);
}

static void tegra21_periph_check_special_reset(void)
{
	check_isp_reset();
}

/* DFLL late init called with CPU clock lock taken */
static void __init tegra21_dfll_cpu_late_init(struct clk *c)
{
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	int ret;
	struct clk *cpu = &tegra_clk_virtual_cpu_g;
	unsigned long int dfll_boot_req_khz = tegra_dfll_boot_req_khz();

	if (!cpu || !cpu->dvfs) {
		pr_err("%s: CPU dvfs is not present\n", __func__);
		return;
	}
	tegra_dvfs_set_dfll_tune_trimmers(cpu->dvfs, tune_cpu_trimmers);

	/* release dfll clock source reset */
	tegra_periph_reset_deassert(c);

	/*
	 * Init DFLL control logic (cl_dvfs):
	 * - if CPU is already set on DFLL by boot-loader, this call would sync
	 *   cl_dvfs driver with cl_dvfs h/w state.
	 * - if CPU booted on PLLX this call initializes cl_dvfs h/w, so DFLL
	 *   can be used as CPU source.
	 */
	ret = tegra_init_cl_dvfs();

	/*
	 * Boot on DFLL.
	 * cl_dvfs device may not be present in DT for virtualized platform.
	 */
	if (dfll_boot_req_khz) {
		if (!ret || (ret == -ENODEV)) {
			c->u.dfll.consumer = cpu;
			c->state = ON;
			use_dfll = DFLL_RANGE_ALL_RATES;
			tegra_dvfs_set_dfll_range(cpu->dvfs, use_dfll);

			/* To read regulator (not cache), set volatile mode */
			tegra_dvfs_rail_set_reg_volatile(tegra_cpu_rail, true);

			if (!ret)
				tegra_cl_dvfs_debug_init(c);

			pr_info("Tegra CPU DFLL booted with use_dfll = %d\n",
				use_dfll);
			return;
		}
		pr_err("Tegra CPU DFLL failed to sync boot state\n");
		return;
	}

	/* Boot on PLLX */
	if (!ret) {
		c->u.dfll.consumer = cpu;
		c->state = OFF;
		if (tegra_platform_is_silicon())
			use_dfll = CONFIG_TEGRA_USE_DFLL_RANGE;
		tegra_dvfs_set_dfll_range(cpu->dvfs, use_dfll);
		tegra_cl_dvfs_debug_init(c);
		pr_info("Tegra CPU DFLL is initialized with use_dfll = %d\n",
			use_dfll);
	}
#endif
}

/*
 * Tegra21 clock policy: PLLC4 is a fixed frequency PLL with VCO output catered
 * to maximum eMMC rate. By default PLLC4 VCO is fixed at ~1GHz, that provides
 * target rates for DDR HS667 mode (PLLC4_OUT1 = VCO/3), DDR HS400 mode, and SDR
 * HS200 mode (PLLC4_OUT2 = VCO/5). For other possible eMMC rate targets VCO
 * rate is tabulated below.
 *
 * PLLC4 post-divider PLLC_OUT0 and secondary divider PLLC_OUT3 ratios specified
 * in the table are selected to provide max input frequency to downstream module
 * dividers (fort better granularity of possible rates).
 *
 * Commonly PLLC_OUT3 is also used as second clock source (along with PLLP) for
 * the system bus.
 */
struct pllc4_sdmmc_map {
	unsigned long sdmmc_max_rate;
	unsigned long vco_rate;
	u32	      out0_ratio;
	u32	      out3_ratio;
	struct clk    *bus_source;
};

static struct pllc4_sdmmc_map  tegra21_pllc4_sdmmc_map[] = {
	{	  0, 1000000000, 1, 1, &tegra_pll_c4_out3 }, /* default cfg */
	{ 266000000,  798000000, 4, 1, &tegra_pll_c4_out1 },
};

static void pllc4_set_fixed_rates(unsigned long cf)
{
	int i;
	u32 val;
	struct device_node *dn;
	unsigned long rate, sdmmc_max_rate = 0;
	struct pllc4_sdmmc_map *pllc4_cfg = &tegra21_pllc4_sdmmc_map[0];

	for_each_compatible_node(dn, NULL, "nvidia,tegra210-sdhci") {
		if (!of_device_is_available(dn))
			continue;

		if (!of_property_read_u32(dn, "max-clk-limit", &val)
		    && (sdmmc_max_rate < val))
			sdmmc_max_rate = val;
	}

	if (sdmmc_max_rate) {
		for (i = 1; i < ARRAY_SIZE(tegra21_pllc4_sdmmc_map); i++) {
			struct pllc4_sdmmc_map *m = &tegra21_pllc4_sdmmc_map[i];
			if (m->sdmmc_max_rate == sdmmc_max_rate) {
				pllc4_cfg = m;
				break;
			}
		}
	}

	/* Align VCO rate on comparison frequency boundary */
	rate = (pllc4_cfg->vco_rate / cf) * cf;
	tegra_pll_c4_vco.u.pll.fixed_rate = rate;
	tegra_pll_c4_vco.flags |= PLL_FIXED;

	rate = DIV_ROUND_UP(rate, pllc4_cfg->out0_ratio);
	tegra_pll_c4_out0.u.pll.fixed_rate = rate;
	tegra_pll_c4_out0.flags |= PLL_FIXED;

	rate = DIV_ROUND_UP(rate, pllc4_cfg->out3_ratio);
	tegra_pll_c4_out3.u.pll_div.default_rate = rate;

	tegra_clk_sbus_cmplx.u.system.sclk_high = pllc4_cfg->bus_source;

	pr_info("pll_c4 rates match %lu max sdmmc: vco=%lu out0=%lu out3=%lu\n",
		sdmmc_max_rate,
		tegra_pll_c4_vco.u.pll.fixed_rate,
		tegra_pll_c4_out0.u.pll.fixed_rate,
		tegra_pll_c4_out3.u.pll_div.default_rate);
}

/*
 * Backup pll is used as transitional CPU clock source while main pll is
 * relocking; in addition all CPU rates below backup level are sourced from
 * backup pll only. Target backup levels for each CPU mode are selected high
 * enough to avoid voltage droop when CPU clock is switched between backup and
 * main plls. Actual backup rates will be rounded to match backup source fixed
 * frequency. Backup rates are also used as stay-on-backup thresholds, and must
 * be kept the same in G and LP mode (will need to add a separate stay-on-backup
 * parameter to allow different backup rates if necessary).
 *
 * Sbus threshold must be exact factor of pll_p rate.
 */
#define CPU_G_BACKUP_RATE_TARGET	200000000
#define CPU_LP_BACKUP_RATE_TARGET	200000000

static void tegra21_pllp_init_dependencies(unsigned long pllp_rate)
{
	u32 div;
	unsigned long backup_rate;
	unsigned long adsp_backup_rate;

	switch (pllp_rate) {
	case 216000000:
		tegra_pll_p_out3.u.pll_div.default_rate = 72000000;
		tegra_pll_p_out4.u.pll_div.default_rate = 108000000;
		tegra_pll_p_out5.u.pll_div.default_rate = 108000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 108000000;
		tegra_clk_host1x.u.periph.threshold = 108000000;
		adsp_backup_rate = 108000000;
		break;
	case 408000000:
		tegra_pll_p_out3.u.pll_div.default_rate = 102000000;
		tegra_pll_p_out4.u.pll_div.default_rate = 204000000;
		tegra_pll_p_out5.u.pll_div.default_rate = 204000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 204000000;
		tegra_clk_host1x.u.periph.threshold = 204000000;
		adsp_backup_rate = 136000000;
		break;
	case 204000000:
		tegra_pll_p_out3.u.pll_div.default_rate = 102000000;
		tegra_pll_p_out4.u.pll_div.default_rate = 204000000;
		tegra_pll_p_out5.u.pll_div.default_rate = 204000000;
		tegra_clk_sbus_cmplx.u.system.threshold = 204000000;
		tegra_clk_host1x.u.periph.threshold = 204000000;
		adsp_backup_rate = 102000000;
		break;
	default:
		pr_err("tegra: PLLP rate: %lu is not supported\n", pllp_rate);
		BUG();
	}
	pr_info("tegra: PLLP fixed rate: %lu\n", pllp_rate);

	div = pllp_rate / CPU_G_BACKUP_RATE_TARGET;
	backup_rate = pllp_rate / div;
	tegra_clk_virtual_cpu_g.u.cpu.backup_rate = backup_rate;

	div = pllp_rate / CPU_LP_BACKUP_RATE_TARGET;
	backup_rate = pllp_rate / div;
	tegra_clk_virtual_cpu_lp.u.cpu.backup_rate = backup_rate;

	div = pllp_rate / adsp_backup_rate;
	tegra_clk_aclk_adsp.u.cclk.div71 = 2 * div - 2; /* reg settings */
	backup_rate = pllp_rate / div;
	tegra_clk_virtual_adsp_bus.u.cpu.backup_rate = adsp_backup_rate;
}

static void tegra21_init_one_clock(struct clk *c)
{
	clk_init(c);
	INIT_LIST_HEAD(&c->shared_bus_list);
	if (!c->lookup.dev_id && !c->lookup.con_id)
		c->lookup.con_id = c->name;
	c->lookup.clk = c;
	clkdev_add(&c->lookup);

	/* Sanity check for clock IDs within CLK_OUT_ENB_NUM */
	if (c->clk_id && (c->clk_id < CLK_OUT_ENB_NUM * 32) &&
	    (c->clk_id != c->u.periph.clk_num))
		WARN(1, "%s clock id %u does not match enable bit %u\n",
		     c->name, c->clk_id, c->u.periph.clk_num);
}

void tegra_edp_throttle_cpu_now(u8 factor)
{
	/* empty definition for tegra21 */
	return;
}

bool tegra_clk_is_parent_allowed(struct clk *c, struct clk *p)
{
	/*
	 * Most of the Tegra21 multimedia and peripheral muxes include pll_c2
	 * and pll_c3 as possible inputs. However, per clock policy these plls
	 * are allowed to be used only by handful devices aggregated on cbus.
	 * For all others, instead of enforcing policy at run-time in this
	 * function, we simply stripped out pll_c2 and pll_c3 options from the
	 * respective muxes statically. Similarly pll_a1 is removed from the
	 * set of possible parents of non-cbus  modules.
	 *
	 * Traditionally ubiquitous on tegra chips pll_c is allowed to be used
	 * as parent for cbus modules only as well, but it is not stripped out
	 * from muxes statically, and the respective policy is enforced by this
	 * function.
	 */
	if ((p == &tegra_pll_c) && (c != &tegra_pll_c_out1))
		return c->flags & PERIPH_ON_CBUS;

	/*
	 * On Tegra21 both PLLM and PLLMB are statically connected to EMC mux
	 * only. However, PLLMB allowed to be used only if PLLM is in default
	 * configuration (i.e., all overrides are disabled).
	 */
	if (p == &tegra_pll_mb)
		return tegra_pll_m.u.pll.defaults_set;

	return true;
}

/* Internal LA may request some clocks to be enabled on init via TRANSACTION
   SCRATCH register settings */
static void __init tegra21x_clk_init_la(void)
{
	struct clk *c;
	u32 reg = readl((void *)
		((uintptr_t)misc_gp_base + MISC_GP_TRANSACTOR_SCRATCH_0));

	if (!(reg & MISC_GP_TRANSACTOR_SCRATCH_LA_ENABLE))
		return;

	c = tegra_get_clock_by_name("la");
	if (WARN(!c, "%s: could not find la clk\n", __func__))
		return;
	tegra_clk_prepare_enable(c);

	if (reg & MISC_GP_TRANSACTOR_SCRATCH_DP2_ENABLE) {
		c = tegra_get_clock_by_name("dp2");
		if (WARN(!c, "%s: could not find dp2 clk\n", __func__))
			return;
		tegra_clk_prepare_enable(c);
	}
}

#ifdef CONFIG_PM_SLEEP
static u32 clk_rst_suspend[RST_DEVICES_NUM + CLK_OUT_ENB_NUM +
			   PERIPH_CLK_SOURCE_NUM + 25];

static int tegra21_clk_suspend(void)
{
	unsigned long off;
	u32 *ctx = clk_rst_suspend;

	*ctx++ = clk_readl(OSC_CTRL) & OSC_CTRL_OSC_FREQ_MASK;
	*ctx++ = clk_readl(CPU_SOFTRST_CTRL);
	*ctx++ = clk_readl(CPU_SOFTRST_CTRL1);
	*ctx++ = clk_readl(CPU_SOFTRST_CTRL2);

	*ctx++ = clk_readl(SPARE_REG);
	*ctx++ = clk_readl(MISC_CLK_ENB);
	*ctx++ = clk_readl(CLK_MASK_ARM);

	*ctx++ = clk_readl(tegra_pll_p_out3.reg);
	*ctx++ = clk_readl(tegra_pll_p_out5.reg);
	*ctx++ = clk_readl(tegra_pll_p_out_hsio.reg);

	*ctx++ = clk_readl(tegra_pll_u_out1.reg);
	*ctx++ = clk_readl(tegra_pll_u_vco.reg);

	*ctx++ = clk_readl(tegra_pll_a_out0.reg);
	*ctx++ = clk_readl(tegra_pll_c_out1.reg);
	*ctx++ = clk_readl(tegra_pll_c4_out3.reg);
	*ctx++ = clk_readl(tegra_pll_re_out1.reg);

	*ctx++ = clk_readl(tegra_clk_sclk.reg);
	*ctx++ = clk_readl(tegra_clk_sclk_div.reg);
	*ctx++ = clk_readl(tegra_clk_sclk_mux.reg);
	*ctx++ = clk_readl(tegra_clk_pclk.reg);

	for (off = PERIPH_CLK_SOURCE_I2S1; off <= PERIPH_CLK_SOURCE_LA;
			off += 4) {
		if (off == PERIPH_CLK_SOURCE_EMC)
			continue;
		*ctx++ = clk_readl(off);
	}
	for (off = PERIPH_CLK_SOURCE_MSELECT; off <= PERIPH_CLK_SOURCE_SE;
			off+=4) {
		*ctx++ = clk_readl(off);
	}
	for (off = AUDIO_DLY_CLK; off <= AUDIO_SYNC_CLK_SPDIF; off+=4) {
		*ctx++ = clk_readl(off);
	}
	for (off = AUDIO_SYNC_CLK_DMIC1;
	      off <= AUDIO_SYNC_CLK_DMIC2; off += 4) {
		*ctx++ = clk_readl(off);
	}
	for (off = PERIPH_CLK_SOURCE_XUSB_HOST;
		off <= PERIPH_CLK_SOURCE_VIC; off += 4) {
		if (off == PERIPH_CLK_SOURCE_EMC_DLL)
			continue;
		*ctx++ = clk_readl(off);
	}
	for (off = PERIPH_CLK_SOURCE_SDMMC_LEGACY;
	      off <= PERIPH_CLK_SOURCE_NVENC; off += 4) {
		*ctx++ = clk_readl(off);
	}
	for (off = AUDIO_SYNC_CLK_DMIC3;
	      off <= PERIPH_CLK_SOURCE_DBGAPB; off += 4) {
		*ctx++ = clk_readl(off);
	}

	*ctx++ = clk_readl(RST_DEVICES_L);
	*ctx++ = clk_readl(RST_DEVICES_H);
	*ctx++ = clk_readl(RST_DEVICES_U);
	*ctx++ = clk_readl(RST_DEVICES_V);
	*ctx++ = clk_readl(RST_DEVICES_W);
	*ctx++ = clk_readl(RST_DEVICES_X);
	*ctx++ = clk_readl(RST_DEVICES_Y);

	*ctx++ = clk_readl(CLK_OUT_ENB_L);
	*ctx++ = clk_readl(CLK_OUT_ENB_H);
	*ctx++ = clk_readl(CLK_OUT_ENB_U);
	*ctx++ = clk_readl(CLK_OUT_ENB_V);
	*ctx++ = clk_readl(CLK_OUT_ENB_W);
	*ctx++ = clk_readl(CLK_OUT_ENB_X);
	*ctx++ = clk_readl(CLK_OUT_ENB_Y);

	*ctx++ = clk_readl(tegra_clk_cclk_g.reg);
	*ctx++ = clk_readl(tegra_clk_cclk_g.reg + SUPER_CLK_DIVIDER);

	*ctx++ = clk_readl(tegra_clk_cclk_lp.reg);
	*ctx++ = clk_readl(tegra_clk_cclk_lp.reg + SUPER_CLK_DIVIDER);

	*ctx = clk_get_rate_all_locked(&tegra_clk_emc);
	tegra21_emc_clk_suspend(&tegra_clk_emc, *ctx++);

	pr_debug("%s: suspend entries: %d, suspend array: %u\n", __func__,
		(s32)(ctx - clk_rst_suspend), (u32)ARRAY_SIZE(clk_rst_suspend));
	BUG_ON((ctx - clk_rst_suspend) > ARRAY_SIZE(clk_rst_suspend));
	return 0;
}

static void tegra21_clk_resume(void)
{
	unsigned long off, rate;
	const u32 *ctx = clk_rst_suspend;
	u32 val, clk_y, pll_u_mask, pll_u_base, pll_u_out12;
	u32 pll_p_out_hsio, pll_p_out_mask, pll_p_out34;
	u32 pll_a_out0, pll_c_out1, pll_c4_out3, pll_re_out1;
	struct clk *p;

	pll_p_out_mask = PLLP_MISC1_HSIO_EN | PLLP_MISC1_XUSB_EN;
	pll_u_mask = (1 << tegra_pll_u_480M.reg_shift) |
		(1 << tegra_pll_u_60M.reg_shift) |
		(1 << tegra_pll_u_48M.reg_shift);

	/*
	 * OSC frequency selection should be already restored by warm boot
	 * code, but just in case make sure it does not change across suspend.
	 */
	val = clk_readl(OSC_CTRL) & ~OSC_CTRL_OSC_FREQ_MASK;
	val |= *ctx++;
	clk_writel(val, OSC_CTRL);
	clk_writel(*ctx++, CPU_SOFTRST_CTRL);
	clk_writel(*ctx++, CPU_SOFTRST_CTRL1);
	clk_writel(*ctx++, CPU_SOFTRST_CTRL2);

	clk_writel(*ctx++, SPARE_REG);
	clk_writel(*ctx++, MISC_CLK_ENB);
	clk_writel(*ctx++, CLK_MASK_ARM);

	/* Since we are going to reset devices and switch clock sources in this
	 * function, plls and secondary dividers is required to be enabled. The
	 * actual value will be restored back later. Note that boot plls: pllm,
	 * and pllp, are already configured and enabled
	 */
	tegra_pllp_out_resume_enable(&tegra_pll_p_out2); /* expected: NOP */
	val = PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;
	val |= val << 16;
	pll_p_out34 = *ctx++;
	clk_writel(pll_p_out34 | val, tegra_pll_p_out3.reg);

	/* Restore as is, GPU is rail-gated, anyway */
	clk_writel(*ctx++, tegra_pll_p_out5.reg);

	pll_p_out_hsio = *ctx++;
	val = clk_readl(tegra_pll_p_out_hsio.reg) | pll_p_out_mask;
	clk_writel(val, tegra_pll_p_out_hsio.reg);

	tegra_pllu_out_resume_enable(&tegra_pll_u_out);
	val = PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;
	pll_u_out12 = *ctx++;
	clk_writel(pll_u_out12 | val, tegra_pll_u_out1.reg);

	pll_u_base = *ctx++;
	val = clk_readl(tegra_pll_u_vco.reg) | pll_u_mask;
	clk_writel(val, tegra_pll_u_vco.reg);

	tegra_pll_clk_resume_enable(&tegra_pll_c);
	tegra_pll_clk_resume_enable(&tegra_pll_c2);
	tegra_pll_clk_resume_enable(&tegra_pll_c3);
	tegra_pll_clk_resume_enable(&tegra_pll_a1);
	tegra_pll_clk_resume_enable(&tegra_pll_a);
	tegra_pll_clk_resume_enable(&tegra_pll_d);
	tegra_pll_clk_resume_enable(&tegra_pll_d2);
	tegra_pll_clk_resume_enable(&tegra_pll_dp);
	tegra_pll_clk_resume_enable(&tegra_pll_x);
	tegra_pll_out_resume_enable(&tegra_pll_c4_out0);
	tegra_pll_out_resume_enable(&tegra_pll_re_out);

	udelay(1000);

	val = PLL_OUT_CLKEN | PLL_OUT_RESET_DISABLE;
	pll_a_out0 = *ctx++;
	clk_writel(pll_a_out0 | val, tegra_pll_a_out0.reg);
	pll_c_out1 = *ctx++;
	clk_writel(pll_c_out1 | val, tegra_pll_c_out1.reg);
	pll_c4_out3 = *ctx++;
	clk_writel(pll_c4_out3 | val, tegra_pll_c4_out3.reg);
	pll_re_out1 = *ctx++;
	clk_writel(pll_re_out1 | val, tegra_pll_re_out1.reg);

	/*
	 * To make sure system bus is not over-clocked during restoration:
	 * - use fixed safe 1:1:2 SCLK:HCLK:PCLK ratio until SCLK source is set
	 * - defer SCLK divider restoration after SCLK source is restored if
	 *   divider set by boot-rom is above saved value.
	 */
	clk_writel(0x01, tegra_clk_pclk.reg);
	clk_writel_delay(*ctx++, tegra_clk_sclk.reg);
	val = *ctx++;
	if (val > clk_readl(tegra_clk_sclk_div.reg))
		clk_writel_delay(val, tegra_clk_sclk_div.reg);
	clk_writel_delay(*ctx++, tegra_clk_sclk_mux.reg);
	if (val != clk_readl(tegra_clk_sclk_div.reg))
		clk_writel_delay(val, tegra_clk_sclk_div.reg);
	clk_writel(*ctx++, tegra_clk_pclk.reg);

	/* enable all clocks before configuring clock sources */
	clk_writel(CLK_OUT_ENB_L_RESET_MASK, CLK_OUT_ENB_L);
	clk_writel(CLK_OUT_ENB_H_RESET_MASK, CLK_OUT_ENB_H);
	clk_writel(CLK_OUT_ENB_U_RESET_MASK, CLK_OUT_ENB_U);
	clk_writel(CLK_OUT_ENB_V_RESET_MASK, CLK_OUT_ENB_V);
	clk_writel(CLK_OUT_ENB_W_RESET_MASK, CLK_OUT_ENB_W);
	clk_writel(CLK_OUT_ENB_X_RESET_MASK, CLK_OUT_ENB_X);
	clk_writel_delay(CLK_OUT_ENB_Y_RESET_MASK, CLK_OUT_ENB_Y);

	for (off = PERIPH_CLK_SOURCE_I2S1; off <= PERIPH_CLK_SOURCE_LA;
			off += 4) {
		if (off == PERIPH_CLK_SOURCE_EMC)
			continue;
		clk_writel(*ctx++, off);
	}
	for (off = PERIPH_CLK_SOURCE_MSELECT; off <= PERIPH_CLK_SOURCE_SE;
			off += 4) {
		clk_writel(*ctx++, off);
	}
	for (off = AUDIO_DLY_CLK; off <= AUDIO_SYNC_CLK_SPDIF; off+=4) {
		clk_writel(*ctx++, off);
	}
	for (off = AUDIO_SYNC_CLK_DMIC1;
	      off <= AUDIO_SYNC_CLK_DMIC2; off += 4) {
		clk_writel(*ctx++, off);
	}
	for (off = PERIPH_CLK_SOURCE_XUSB_HOST;
		off <= PERIPH_CLK_SOURCE_VIC; off += 4) {
		if (off == PERIPH_CLK_SOURCE_EMC_DLL)
			continue;
		clk_writel(*ctx++, off);
	}
	for (off = PERIPH_CLK_SOURCE_SDMMC_LEGACY;
	      off <= PERIPH_CLK_SOURCE_NVENC; off += 4) {
		clk_writel(*ctx++, off);
	}
	for (off = AUDIO_SYNC_CLK_DMIC3;
	      off <= PERIPH_CLK_SOURCE_DBGAPB; off += 4) {
		clk_writel(*ctx++, off);
	}

	clk_readl(off);
	udelay(RESET_PROPAGATION_DELAY);

	clk_writel(*ctx++, RST_DEVICES_L);
	clk_writel(*ctx++, RST_DEVICES_H);
	clk_writel(*ctx++, RST_DEVICES_U);
	clk_writel(*ctx++, RST_DEVICES_V);
	clk_writel(*ctx++, RST_DEVICES_W);
	clk_writel(*ctx++, RST_DEVICES_X);
	clk_writel_delay(*ctx++, RST_DEVICES_Y);

	clk_writel(*ctx++, CLK_OUT_ENB_L);
	clk_writel(*ctx++, CLK_OUT_ENB_H);
	clk_writel(*ctx++, CLK_OUT_ENB_U);
	clk_writel(*ctx++, CLK_OUT_ENB_V);
	clk_writel(*ctx++, CLK_OUT_ENB_W);
	clk_writel(*ctx++, CLK_OUT_ENB_X);

	/* Keep pllp_out_cpu enabled */
	clk_y = *ctx++;
	val = 1 << (tegra_pll_p_out_cpu.u.periph.clk_num % 32);
	clk_writel_delay(clk_y | val, CLK_OUT_ENB_Y);

	/* DFLL resume after cl_dvfs and i2c5 clocks are resumed */
	tegra21_dfll_clk_resume(&tegra_dfll_cpu);

	/* CPU G clock restored after DFLL and PLLs */
	clk_writel(*ctx++, tegra_clk_cclk_g.reg);
	clk_writel(*ctx++, tegra_clk_cclk_g.reg + SUPER_CLK_DIVIDER);

	/* CPU LP clock restored after PLLs and pllp_out_cpu branch */
	clk_writel(*ctx++, tegra_clk_cclk_lp.reg);
	clk_writel_delay(*ctx++, tegra_clk_cclk_lp.reg + SUPER_CLK_DIVIDER);

	/* Restore back the actual pll and secondary divider values */
	clk_writel(pll_p_out34, tegra_pll_p_out3.reg);
	val = clk_readl(tegra_pll_p_out_hsio.reg) & (~pll_p_out_mask);
	val |= pll_p_out_hsio & pll_p_out_mask;
	clk_writel(val, tegra_pll_p_out_hsio.reg);

	clk_writel(pll_u_out12, tegra_pll_u_out1.reg);
	val = clk_readl(tegra_pll_u_vco.reg) & (~pll_u_mask);
	clk_writel(val | (pll_u_base & pll_u_mask), tegra_pll_u_vco.reg);

	clk_writel_delay(clk_y, CLK_OUT_ENB_Y);

	p = &tegra_pll_c;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_c2;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_c3;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_a1;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_a;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_d;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_d2;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_dp;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_x;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_c4_vco;
	if (p->state == OFF)
		p->ops->disable(p);
	p = &tegra_pll_re_vco;
	if (p->state == OFF)
		p->ops->disable(p);

	clk_writel(pll_a_out0, tegra_pll_a_out0.reg);
	clk_writel(pll_c_out1, tegra_pll_c_out1.reg);
	clk_writel(pll_c4_out3, tegra_pll_c4_out3.reg);
	clk_writel(pll_re_out1, tegra_pll_re_out1.reg);

	/* Since EMC clock is not restored, and may not preserve parent across
	   suspend, update current state, and mark EMC DFS as out of sync */
	p = tegra_clk_emc.parent;
	tegra21_periph_clk_init(&tegra_clk_emc);

	/* Turn Off pll_m if it was OFF before suspend, and emc was not switched
	   to pll_m across suspend; re-init pll_m to sync s/w and h/w states */
	if ((tegra_pll_m.state == OFF) &&
	    (&tegra_pll_m != tegra_clk_emc.parent))
		tegra21_pllm_clk_disable(&tegra_pll_m);
	tegra21_pllm_clk_init(&tegra_pll_m);

	/* Resume pll_mb (FIXME: don't need to enable/disable) */
	tegra_pll_clk_resume_enable(&tegra_pll_mb);
	if (tegra_pll_mb.state == OFF)
		tegra_pll_mb.ops->disable(&tegra_pll_mb);

	if (p != tegra_clk_emc.parent) {
		pr_debug("EMC parent(refcount) across suspend: %s(%d) : %s(%d)",
			p->name, p->refcnt, tegra_clk_emc.parent->name,
			tegra_clk_emc.parent->refcnt);

		/* emc switched to the new parent by low level code, but ref
		   count and s/w state need to be updated */
		clk_disable_locked(p);
		clk_enable_locked(tegra_clk_emc.parent);
	}

	/* update DVFS voltage requirements */
	rate = clk_get_rate_all_locked(&tegra_clk_emc);
	if (*ctx != rate) {
		tegra_dvfs_set_rate(&tegra_clk_emc, rate);
		if (p == tegra_clk_emc.parent) {
			rate = clk_get_rate_all_locked(p);
			tegra_dvfs_set_rate(p, rate);
		}
	}
	tegra_emc_timing_invalidate();

	tegra21_pllu_hw_ctrl_set(&tegra_pll_u_vco); /* PLLU, UTMIP h/w ctrl */
}

static struct syscore_ops tegra_clk_syscore_ops = {
	.suspend = tegra21_clk_suspend,
	.resume = tegra21_clk_resume,
	.save = tegra21_clk_suspend,
	.restore = tegra21_clk_resume,
};
#endif

static void tegra21_init_xusb_clocks(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tegra_xusb_source_clks); i++)
		tegra21_init_one_clock(&tegra_xusb_source_clks[i]);

	tegra21_init_one_clock(&tegra_xusb_ss_div2);
	tegra21_init_one_clock(&tegra_xusb_hs_src);
	tegra21_init_one_clock(&tegra_xusb_ssp_src);

	for (i = 0; i < ARRAY_SIZE(tegra_xusb_coupled_clks); i++)
		tegra21_init_one_clock(&tegra_xusb_coupled_clks[i]);
}

/*
 * Check if arch timer frequency specified either in DT or in CNTFRQ register is
 * matching actual clk_m rate.
 */
static void tegra21_check_timer_clock(u32 clk_m_rate)
{

	struct device_node *dn;
	u32 dt_rate = 0;
	u32 timer_rate = 0;

	for_each_compatible_node(dn, NULL, "arm,armv8-timer") {
		if (!of_device_is_available(dn))
			continue;

		if (!of_property_read_u32(dn, "clock-frequency", &dt_rate))
			timer_rate = dt_rate;
		break;
	}

	if (!dn) {
		WARN(1, "No arch timer node in DT\n");
		return;
	}

	if (!timer_rate)
		timer_rate = arch_timer_get_cntfrq();

	if (timer_rate == clk_m_rate)
		return;

	WARN(1, "Arch timer %s rate %u doesn't match clk_m %u - kernel timing is broken\n",
		dt_rate ? "DT" : "CNTFRQ", timer_rate, clk_m_rate);
}

/*
 * The udelay() is implemented based on arch timers, using loops_per_jiffy as
 * scaling factor. To make it functional during early clock initialization -
 *  before timers are initialized - set loops_per_jiffy here.
 */
static void tegra21_init_main_clock(void)
{
	int i;
	unsigned long clk_m_rate;

	for (i = 0; i < ARRAY_SIZE(tegra_main_clks); i++)
		tegra21_init_one_clock(tegra_main_clks[i]);

	clk_m_rate = clk_get_rate_all_locked(&tegra_clk_m);

	loops_per_jiffy = clk_m_rate / HZ;

	tegra21_check_timer_clock(clk_m_rate);
}

void __init tegra21x_init_clocks(void)
{
	int i;
	struct clk *c;

#ifndef	CONFIG_TEGRA_DUAL_CBUS
	BUILD_BUG()
#endif
	tegra21_init_main_clock();

	for (i = 0; i < ARRAY_SIZE(tegra_ptr_clks); i++)
		tegra21_init_one_clock(tegra_ptr_clks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_list_clks); i++)
		tegra21_init_one_clock(&tegra_list_clks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_visp_clks); i++)
		tegra21_init_one_clock(&tegra_visp_clks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_ptr_camera_mclks); i++)
		tegra21_init_one_clock(tegra_ptr_camera_mclks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_sync_source_list); i++)
		tegra21_init_one_clock(&tegra_sync_source_list[i]);
	for (i = 0; i < ARRAY_SIZE(tegra_clk_audio_list); i++)
		tegra21_init_one_clock(&tegra_clk_audio_list[i]);
	for (i = 0; i < ARRAY_SIZE(tegra_clk_audio_2x_list); i++)
		tegra21_init_one_clock(&tegra_clk_audio_2x_list[i]);
	for (i = 0; i < ARRAY_SIZE(tegra_clk_audio_dmic_list); i++)
		tegra21_init_one_clock(&tegra_clk_audio_dmic_list[i]);

	init_clk_out_mux();
	for (i = 0; i < ARRAY_SIZE(tegra_clk_out_list); i++)
		tegra21_init_one_clock(&tegra_clk_out_list[i]);

	tegra21_init_xusb_clocks();

	for (i = 0; i < ARRAY_SIZE(tegra_slcg_clks); i++)
		tegra21_init_one_clock(&tegra_slcg_clks[i]);

	for (i = 0; i < ARRAY_SIZE(tegra_clk_duplicates); i++) {
		c = tegra_get_clock_by_name(tegra_clk_duplicates[i].name);
		if (!c) {
			pr_err("%s: Unknown duplicate clock %s\n", __func__,
				tegra_clk_duplicates[i].name);
			continue;
		}

		tegra_clk_duplicates[i].lookup.clk = c;
		clkdev_add(&tegra_clk_duplicates[i].lookup);
	}

	/* Check/apply safe rate to clocks with reset dependency on others */
	tegra21_periph_check_special_reset();

	/* Make sure at list one of dual PLLM/PLLMB is disabled */
	tegra21_emc_sync_plls(&tegra_clk_emc, &tegra_pll_m, &tegra_pll_mb);

	/* Tegra21 allows to change dividers of disabled clocks */
	tegra_clk_set_disabled_div_all();

#ifdef CONFIG_PM_SLEEP
	register_syscore_ops(&tegra_clk_syscore_ops);
#endif

#ifdef CONFIG_DEBUG_FS
	tegra_clk_add_pto_entries(ptodefs);
#endif
}

static int __init tegra21x_clk_late_init(void)
{
	tegra_clk_disable_unprepare(&tegra_pll_re_vco);
	tegra_clk_disable_unprepare(tegra_get_clock_by_name("boot.apb.sclk"));
	if (of_property_read_bool(of_chosen, "nvidia,gpu_clk_to_max_on_boot"))
		clk_set_rate(tegra_get_clock_by_name("gm20b.gbus"), UINT_MAX);
	return 0;
}
late_initcall(tegra21x_clk_late_init);
