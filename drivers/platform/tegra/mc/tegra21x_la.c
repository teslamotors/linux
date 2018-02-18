/*
 * Copyright (C) 2014-2016, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/types.h>
#include <linux/clk.h>

#include <asm/io.h>

#include <linux/platform/tegra/latency_allowance.h>
#include <linux/platform/tegra/tegra_emc.h>
#include <linux/platform/tegra/mc.h>
#include <linux/platform/tegra/clock.h>

#include "la_priv.h"

#define ON_LPDDR4() (tegra_emc_get_dram_type() == DRAM_TYPE_LPDDR4)

#define LA_ST_LA_MINUS_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP	70000
#define LA_DRAM_WIDTH_BITS				64
#define LA_DISP_CATCHUP_FACTOR_FP			1100
#define MC_MAX_FREQ_MHZ					533
#define MAX_GRANT_DEC					511

#define EXP_TIME_EMCCLKS_FP				88000
#define MAX_LA_NSEC					7650
#define DDA_BW_MARGIN_FP				1100
#define ONE_DDA_FRAC_FP					10
#define CPU_RD_BW_PERC					9
#define CPU_WR_BW_PERC					1
#define MIN_CYCLES_PER_GRANT				2
#define EMEM_PTSA_MINMAX_WIDTH				5
#define RING1_FEEDER_SISO_ALLOC_DIV			2

static struct la_client_info t21x_la_info_array[] = {
	LA(0, 0, AFI_0,		23 : 16, AFIW,		false, 128, 800),
	LA(0, 0, AFI_0,		7  : 0,  AFIR,		false, 59,  400),
	LA(0, 0, AVPC_0,	7  : 0,  AVPC_ARM7R,	false, 4,   0),
	LA(0, 0, AVPC_0,	23 : 16, AVPC_ARM7W,	false, 128, 800),
	LA(0, 0, DC_0,		7  : 0,  DISPLAY_0A,	true,  80,  0),
	LA(0, 0, DCB_0,		7  : 0,  DISPLAY_0AB,	true,  80,  0),
	LA(0, 0, DC_0,		23 : 16, DISPLAY_0B,	true,  80,  0),
	LA(0, 0, DCB_0,		23 : 16, DISPLAY_0BB,	true,  80,  0),
	LA(0, 0, DC_1,		7  : 0,  DISPLAY_0C,	true,  80,  0),
	LA(0, 0, DCB_1,		7  : 0,  DISPLAY_0CB,	true,  80,  0),
	LA(0, 0, DC_3,		7  : 0,  DISPLAYD,	false, 80,  0),
	LA(0, 0, DC_2,		7  : 0,  DISPLAY_HC,	false, 80,  0),
	LA(0, 0, DCB_2,		7  : 0,  DISPLAY_HCB,	false, 80,  0),
	LA(0, 0, DC_2,		23 : 16, DISPLAY_T,	false, 80,  0),
	LA(0, 0, GPU_0,		7  : 0,  GPUSRD,	false, 25,  800),
	LA(0, 0, GPU_0,		23 : 16, GPUSWR,	false, 128, 800),
	LA(0, 0, HDA_0,		7  : 0,  HDAR,		false, 36,  0),
	LA(0, 0, HDA_0,		23 : 16, HDAW,		false, 128, 800),
	LA(0, 0, TSEC_0,	7  : 0,  TSECSRD,	false, 157, 200),
	LA(0, 0, TSEC_0,	23 : 16, TSECSWR,	false, 128, 800),
	LA(0, 0, TSECB_0,	7  : 0,  TSECBSRD,	false, 157, 200),
	LA(0, 0, TSECB_0,	23 : 16, TSECBSWR,	false, 128, 800),
	LA(0, 0, HC_0,		7  : 0,  HOST1X_DMAR,	false, 28,  800),
	LA(0, 0, HC_0,		23 : 16, HOST1XR,	false, 80,  0),
	LA(0, 0, HC_1,		7  : 0,  HOST1XW,	false, 128, 800),
	LA(0, 0, ISP2_0,	7  : 0,  ISP_RA,	false, 51,  300),
	LA(0, 0, ISP2_1,	7  : 0,  ISP_WA,	false, 128, 800),
	LA(0, 0, ISP2_1,	23 : 16, ISP_WB,	false, 128, 800),
	LA(0, 0, ISP2B_0,	7  : 0,  ISP_RAB,	false, 54,  300),
	LA(0, 0, ISP2B_1,	7  : 0,  ISP_WAB,	false, 128, 800),
	LA(0, 0, ISP2B_1,	23 : 16, ISP_WBB,	false, 128, 800),
	LA(0, 0, MPCORE_0,	7  : 0,  MPCORER,	false, 4,   0),
	LA(0, 0, MPCORE_0,	23 : 16, MPCOREW,	false, 128, 800),
	LA(0, 0, NVDEC_0,	7  : 0,  NVDECR,	false, 4,   0),
	LA(0, 0, NVDEC_0,	23 : 16, NVDECW,	false, 128, 800),
	/* No entries for nvenc and nvjpg ?? */
	/* LA(0, 0, PPCS_1,	7  : 0,  PPCS_AHBDMAW,	true,  128, 800), */
	LA(0, 0, PPCS_0,	23 : 16, PPCS_AHBSLVR,	false, 89,  408),
	LA(0, 0, PPCS_1,	23 : 16, PPCS_AHBSLVW,	false, 128, 800),
	LA(0, 0, PTC_0,		7  : 0,  PTCR,		false, 0,   0),
	LA(0, 0, SATA_0,	7  : 0,  SATAR,		false, 104, 400),
	LA(0, 0, SATA_0,	23 : 16, SATAW,		false, 128, 800),
	LA(0, 0, SDMMC_0,	7  : 0,  SDMMCR,	false, 150, 248),
	LA(0, 0, SDMMCA_0,	7  : 0,  SDMMCRA,	false, 150, 248),
	LA(0, 0, SDMMCAA_0,	7  : 0,  SDMMCRAA,	false, 88,  248),
	LA(0, 0, SDMMCAB_0,	7  : 0,  SDMMCRAB,	false, 88,  248),
	LA(0, 0, SDMMC_0,	23 : 16, SDMMCW,	false, 128, 800),
	LA(0, 0, SDMMCA_0,	23 : 16, SDMMCWA,	false, 128, 800),
	LA(0, 0, SDMMCAA_0,	23 : 16, SDMMCWAA,	false, 128, 800),
	LA(0, 0, SDMMCAB_0,	23 : 16, SDMMCWAB,	false, 128, 800),
	LA(0, 0, VIC_0,		7  : 0,  VICSRD,	false, 29,  800),
	LA(0, 0, VIC_0,		23 : 16, VICSWR,	false, 128, 800),
	LA(0, 0, VI2_0,		7  : 0,  VI_W,		false, 128, 800),
	LA(0, 0, XUSB_1,	7  : 0,  XUSB_DEVR,	false, 59,  400),
	LA(0, 0, XUSB_1,	23 : 16, XUSB_DEVW,	false, 128, 800),
	LA(0, 0, XUSB_0,	7  : 0,  XUSB_HOSTR,	false, 66,  400),
	LA(0, 0, XUSB_0,	23 : 16, XUSB_HOSTW,	false, 128, 800),

	/* end of list */
	LA(0, 0, DC_3,		0 : 0, MAX_ID, false, 0, 0)
};

static unsigned int emc_min_freq_mhz_fp;
static unsigned int emc_min_freq_mhz;
static unsigned int emc_max_freq_mhz;
static unsigned int hi_gd_fp;
static unsigned int lo_gd_fp;
static unsigned int hi_gd_fpa;
static unsigned int lo_gd_fpa;
static unsigned int low_freq_bw;
static unsigned int dda_div;

static struct la_chip_specific *cs;
const struct disp_client *tegra_la_disp_clients_info;
static unsigned int total_dc0_bw;
static unsigned int total_dc1_bw;
static DEFINE_MUTEX(disp_and_camera_ptsa_lock);

static void program_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;

	mc_writel(p->ptsa_grant_dec, MC_PTSA_GRANT_DECREMENT);
	mc_writel(1, MC_TIMING_CONTROL);
	mc_writel(p->dis_extra_snap_level, MC_DIS_EXTRA_SNAP_LEVELS);

	WRITE_PTSA_MIN_MAX_RATE(p, dis, DIS);
	WRITE_PTSA_MIN_MAX_RATE(p, disb, DISB);
	WRITE_PTSA_MIN_MAX_RATE(p, ve, VE);
	WRITE_PTSA_MIN_MAX_RATE(p, ve2, VE2);
	WRITE_PTSA_MIN_MAX_RATE(p, ring2, RING2);
	WRITE_PTSA_MIN_MAX_RATE(p, mpcorer, MLL_MPCORER);
	WRITE_PTSA_MIN_MAX_RATE(p, smmu, SMMU_SMMU);
	WRITE_PTSA_MIN_MAX_RATE(p, ring1, RING1);
	WRITE_PTSA_MIN_MAX_RATE(p, isp, ISP);

	WRITE_PTSA_MIN_MAX(p, a9avppc, A9AVPPC);
	WRITE_PTSA_MIN_MAX(p, avp, AVP);
	WRITE_PTSA_MIN_MAX(p, mse, MSE);
	WRITE_PTSA_MIN_MAX(p, gk, GK);
	WRITE_PTSA_MIN_MAX(p, vicpc, VICPC);
	WRITE_PTSA_MIN_MAX(p, apb, APB);
	WRITE_PTSA_MIN_MAX(p, pcx, PCX);
	WRITE_PTSA_MIN_MAX(p, host, HOST);
	WRITE_PTSA_MIN_MAX(p, ahb, AHB);
	WRITE_PTSA_MIN_MAX(p, sax, SAX);
	WRITE_PTSA_MIN_MAX(p, aud, AUD);
	WRITE_PTSA_MIN_MAX(p, sd, SD);
	WRITE_PTSA_MIN_MAX(p, usbx, USBX);
	WRITE_PTSA_MIN_MAX(p, usbd, USBD);
	WRITE_PTSA_MIN_MAX(p, ftop, FTOP);
}

static void save_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;

	p->ptsa_grant_dec = mc_readl(MC_PTSA_GRANT_DECREMENT);
	p->dis_extra_snap_level = mc_readl(MC_DIS_EXTRA_SNAP_LEVELS);

	READ_PTSA_MIN_MAX_RATE(p, dis, DIS);
	READ_PTSA_MIN_MAX_RATE(p, disb, DISB);
	READ_PTSA_MIN_MAX_RATE(p, ve, VE);
	READ_PTSA_MIN_MAX_RATE(p, ve2, VE2);
	READ_PTSA_MIN_MAX_RATE(p, ring2, RING2);
	READ_PTSA_MIN_MAX_RATE(p, mpcorer, MLL_MPCORER);
	READ_PTSA_MIN_MAX_RATE(p, smmu, SMMU_SMMU);
	READ_PTSA_MIN_MAX_RATE(p, ring1, RING1);
	READ_PTSA_MIN_MAX_RATE(p, isp, ISP);

	READ_PTSA_MIN_MAX(p, a9avppc, A9AVPPC);
	READ_PTSA_MIN_MAX(p, avp, AVP);
	READ_PTSA_MIN_MAX(p, mse, MSE);
	READ_PTSA_MIN_MAX(p, gk, GK);
	READ_PTSA_MIN_MAX(p, vicpc, VICPC);
	READ_PTSA_MIN_MAX(p, apb, APB);
	READ_PTSA_MIN_MAX(p, apb, APB);
	READ_PTSA_MIN_MAX(p, host, HOST);
	READ_PTSA_MIN_MAX(p, ahb, AHB);
	READ_PTSA_MIN_MAX(p, sax, SAX);
	READ_PTSA_MIN_MAX(p, aud, AUD);
	READ_PTSA_MIN_MAX(p, sd, SD);
	READ_PTSA_MIN_MAX(p, usbx, USBX);
	READ_PTSA_MIN_MAX(p, usbd, USBD);
	READ_PTSA_MIN_MAX(p, ftop, FTOP);
}

/*
 * Gets the memory BW in MBps. @emc_freq should be in MHz.
 */
static u32 get_mem_bw_mbps(u32 dram_freq)
{
	return dram_freq * 16;
}

/*
 * EMC frequency is actually DRAM frequency. Normally they are one in the same;
 * however, with LPDDR4 DRAM, the DRAM clock goes to 1600MHz which the EMC clock
 * cannot. Thus the EMC is actually running at DRAM clk / 2.
 */
static void t21x_init_ptsa(void)
{
	struct clk *emc_clk;
	unsigned int emc_freq_mhz;
	unsigned int mc_freq_mhz;
	unsigned int same_freq;
	struct ptsa_info *p = &cs->ptsa_info;
	unsigned int cpu_rd_bw, cpu_wr_bw;
	unsigned int gd_int, gd_frac_fp;
	int gd_fpa;

	/* get emc frequency */
	emc_clk = clk_get_sys("tegra_emc", "emc");
	emc_freq_mhz = clk_get_rate(emc_clk) /
			LA_HZ_TO_MHZ_FACTOR;
	la_debug("emc clk_rate = %u MHz", emc_freq_mhz);

	/* get mc frequency */
	same_freq = mc_readl(MC_EMEM_ARB_MISC0) &&
		MC_EMEM_ARB_MISC0_MC_EMC_SAME_FREQ_BIT;
	mc_freq_mhz = same_freq ? emc_freq_mhz : emc_freq_mhz / 2;
	la_debug("mc clk_rate = %u MHz", mc_freq_mhz);

	/* compute initial value for grant dec */
	gd_fpa = (LA_FP_TO_FPA(lo_gd_fp) * emc_freq_mhz) / emc_min_freq_mhz;
	if (gd_fpa >= LA_REAL_TO_FPA(1)) {
		gd_int = 1;
		gd_fpa -= LA_REAL_TO_FPA(1);
	} else {
		gd_int = 0;
	}

	gd_frac_fp = __fraction2dda_fp(gd_fpa, 1,
				       MC_PTSA_RATE_DEFAULT_MASK);
	p->ptsa_grant_dec = (gd_int << 12) | gd_frac_fp;


	/* initialize PTSA reg values */
	MC_SET_INIT_PTSA(p, ve,      -5, 31);
	MC_SET_INIT_PTSA(p, isp,     -5, 31);
	MC_SET_INIT_PTSA(p, ve2,     -5, 31);
	MC_SET_INIT_PTSA(p, a9avppc, -5, 16);
	MC_SET_INIT_PTSA(p, ring2,   -2, 0);
	MC_SET_INIT_PTSA(p, dis,     -5, 31);
	MC_SET_INIT_PTSA(p, disb,    -5, 31);
	MC_SET_INIT_PTSA(p, ring1,   -5, 31);
	MC_SET_INIT_PTSA(p, mpcorer, -4, 4);
	MC_SET_INIT_PTSA(p, smmu,     1, 1);
	MC_SET_INIT_PTSA(p, mse,     -2, 0);
	MC_SET_INIT_PTSA(p, gk,      -2, 0);
	MC_SET_INIT_PTSA(p, vicpc,   -2, 0);
	MC_SET_INIT_PTSA(p, apb,     -2, 0);
	MC_SET_INIT_PTSA(p, pcx,     -2, 0);
	MC_SET_INIT_PTSA(p, host,    -2, 0);
	MC_SET_INIT_PTSA(p, ahb,     -2, 0);
	MC_SET_INIT_PTSA(p, sax,     -2, 0);
	MC_SET_INIT_PTSA(p, sd,      -2, 0);
	MC_SET_INIT_PTSA(p, usbx,    -2, 0);
	MC_SET_INIT_PTSA(p, usbd,    -2, 0);
	MC_SET_INIT_PTSA(p, ftop,    -2, 0);
	MC_SET_INIT_PTSA(p, avp,     -2, 0);
	MC_SET_INIT_PTSA(p, aud,     -5, 31);
	MC_SET_INIT_PTSA(p, jpg,     -2, 0);
	MC_SET_INIT_PTSA(p, gk2,     -2, 0);

	p->ring2_ptsa_rate = (unsigned int)(12) & MC_PTSA_RATE_DEFAULT_MASK;

	/* mpcorer */
	cpu_rd_bw = (get_mem_bw_mbps(emc_freq_mhz) * CPU_RD_BW_PERC) / 100;
	if (ON_LPDDR4())
		cpu_rd_bw /= 2;

	p->mpcorer_ptsa_rate = __fraction2dda_fp(lo_gd_fpa * cpu_rd_bw /
						 low_freq_bw,
						 dda_div,
						 MC_PTSA_RATE_DEFAULT_MASK);

	/* FTOP (mpcorew it seems)*/
	cpu_wr_bw = (get_mem_bw_mbps(emc_freq_mhz) * CPU_WR_BW_PERC) / 100;
	if (ON_LPDDR4())
		cpu_rd_bw /= 2;

	p->ftop_ptsa_rate = __fraction2dda_fp(lo_gd_fpa * cpu_wr_bw /
					      low_freq_bw,
					      dda_div,
					      MC_PTSA_RATE_DEFAULT_MASK);

	/* Ring1 */
	p->ring1_ptsa_rate =  p->mpcorer_ptsa_rate + p->ftop_ptsa_rate;
	p->ring1_ptsa_rate += p->dis_ptsa_rate + p->disb_ptsa_rate;
	p->ring1_ptsa_rate += p->ve_ptsa_rate + p->ring2_ptsa_rate;

	program_ptsa();
}

/*
 * This now also includes ring1 since that needs to have its PTSA updated
 * based on freq and usecase.
 */
static void t21x_calc_disp_and_camera_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;
	unsigned int ve_bw_fp = cs->camera_bw_array[CAMERA_IDX(VI_W)] *
				DDA_BW_MARGIN_FP;
	unsigned int ve2_bw_fp = 0;
	unsigned int isp_bw_fp = 0;
	unsigned int total_dc0_bw_fp = total_dc0_bw * DDA_BW_MARGIN_FP;
	unsigned int total_dc1_bw_fp = total_dc1_bw * DDA_BW_MARGIN_FP;
	unsigned int low_freq_bw_fp = la_real_to_fp(low_freq_bw);
	unsigned int dis_frac_fp = LA_FPA_TO_FP(lo_gd_fpa * total_dc0_bw_fp /
						low_freq_bw_fp);
	unsigned int disb_frac_fp = LA_FPA_TO_FP(lo_gd_fpa * total_dc1_bw_fp /
						 low_freq_bw_fp);
	unsigned int total_iso_bw_fp = total_dc0_bw_fp + total_dc1_bw_fp;
	int max_max = (1 << EMEM_PTSA_MINMAX_WIDTH) - 1;
	int i = 0;

	if (cs->agg_camera_array[AGG_CAMERA_ID(VE2)].is_hiso) {
		ve2_bw_fp = (cs->camera_bw_array[CAMERA_IDX(ISP_RAB)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WAB)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WBB)]) *
				DDA_BW_MARGIN_FP;
	} else {
		ve2_bw_fp = LA_REAL_TO_FP(
				cs->camera_bw_array[CAMERA_IDX(ISP_RAB)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WAB)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WBB)]);
	}

	if (cs->agg_camera_array[AGG_CAMERA_ID(ISP)].is_hiso) {
		isp_bw_fp = (cs->camera_bw_array[CAMERA_IDX(ISP_RA)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WA)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WB)]) *
				DDA_BW_MARGIN_FP;
	} else {
		isp_bw_fp = LA_REAL_TO_FP(
				cs->camera_bw_array[CAMERA_IDX(ISP_RA)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WA)] +
				cs->camera_bw_array[CAMERA_IDX(ISP_WB)]);
	}

	cs->agg_camera_array[AGG_CAMERA_ID(VE)].bw_fp = ve_bw_fp;
	cs->agg_camera_array[AGG_CAMERA_ID(VE2)].bw_fp = ve2_bw_fp;
	cs->agg_camera_array[AGG_CAMERA_ID(ISP)].bw_fp = isp_bw_fp;

	for (i = 0; i < TEGRA_LA_AGG_CAMERA_NUM_CLIENTS; i++) {
		struct agg_camera_client_info *agg_client =
						&cs->agg_camera_array[i];

		if (agg_client->is_hiso) {
			agg_client->frac_fp = LA_FPA_TO_FP(
						lo_gd_fpa * agg_client->bw_fp /
						low_freq_bw_fp);
			agg_client->ptsa_min = (unsigned int)(-5) &
						MC_PTSA_MIN_DEFAULT_MASK;
			agg_client->ptsa_max = (unsigned int)(max_max) &
						MC_PTSA_MAX_DEFAULT_MASK;

			total_iso_bw_fp += agg_client->bw_fp;
		} else {
			agg_client->frac_fp = ONE_DDA_FRAC_FP;
			agg_client->ptsa_min = (unsigned int)(-2) &
						MC_PTSA_MIN_DEFAULT_MASK;
			agg_client->ptsa_max = (unsigned int)(0) &
						MC_PTSA_MAX_DEFAULT_MASK;
		}
	}

	MC_SET_INIT_PTSA(p, dis, -5, max_max);
	p->dis_ptsa_rate = fraction2dda_fp(
				dis_frac_fp,
				4,
				MC_PTSA_RATE_DEFAULT_MASK) &
		MC_PTSA_RATE_DEFAULT_MASK;

	MC_SET_INIT_PTSA(p, disb, -5, max_max);
	p->disb_ptsa_rate = fraction2dda_fp(
				disb_frac_fp,
				4,
				MC_PTSA_RATE_DEFAULT_MASK) &
		MC_PTSA_RATE_DEFAULT_MASK;

	p->ve_ptsa_min = cs->agg_camera_array[AGG_CAMERA_ID(VE)].ptsa_min &
					MC_PTSA_MIN_DEFAULT_MASK;
	p->ve_ptsa_max = cs->agg_camera_array[AGG_CAMERA_ID(VE)].ptsa_max &
					MC_PTSA_MAX_DEFAULT_MASK;
	p->ve_ptsa_rate = fraction2dda_fp(
				cs->agg_camera_array[AGG_CAMERA_ID(VE)].frac_fp,
				4,
				MC_PTSA_RATE_DEFAULT_MASK) &
		MC_PTSA_RATE_DEFAULT_MASK;

	p->ve2_ptsa_min = cs->agg_camera_array[AGG_CAMERA_ID(VE2)].ptsa_min &
					MC_PTSA_MIN_DEFAULT_MASK;
	p->ve2_ptsa_max = cs->agg_camera_array[AGG_CAMERA_ID(VE2)].ptsa_max &
					MC_PTSA_MAX_DEFAULT_MASK;
	p->ve2_ptsa_rate = fraction2dda_fp(
			cs->agg_camera_array[AGG_CAMERA_ID(VE2)].frac_fp,
			4,
			MC_PTSA_RATE_DEFAULT_MASK) &
		MC_PTSA_RATE_DEFAULT_MASK;

	p->isp_ptsa_min = cs->agg_camera_array[AGG_CAMERA_ID(ISP)].ptsa_min &
					MC_PTSA_MIN_DEFAULT_MASK;
	p->isp_ptsa_max = cs->agg_camera_array[AGG_CAMERA_ID(ISP)].ptsa_max &
					MC_PTSA_MAX_DEFAULT_MASK;
	p->isp_ptsa_rate = fraction2dda_fp(
			cs->agg_camera_array[AGG_CAMERA_ID(ISP)].frac_fp,
			4,
			MC_PTSA_RATE_DEFAULT_MASK) &
		MC_PTSA_RATE_DEFAULT_MASK;

	MC_SET_INIT_PTSA(p, ring1, -5, max_max);


	p->ring1_ptsa_rate = p->dis_ptsa_rate + p->disb_ptsa_rate +
		p->ve_ptsa_rate;
	p->ring1_ptsa_rate += cs->agg_camera_array[AGG_CAMERA_ID(VE2)].is_hiso ?
		p->ve2_ptsa_rate : 0;
	p->ring1_ptsa_rate += cs->agg_camera_array[AGG_CAMERA_ID(ISP)].is_hiso ?
		p->isp_ptsa_rate : 0;

	if (ON_LPDDR4())
		p->ring1_ptsa_rate /= 2;

	/* These need to be read from the registers since the MC/EMC clock may
	   be different than last time these were read into *p. */
	p->ring1_ptsa_rate += mc_readl(MC_MLL_MPCORER_PTSA_RATE) +
		mc_readl(MC_FTOP_PTSA_RATE);

	if (p->ring1_ptsa_rate == 0)
		p->ring1_ptsa_rate = 0x1;
}

static void t21x_update_display_ptsa_rate(unsigned int *disp_bw_array)
{
	struct ptsa_info *p = &cs->ptsa_info;

	t21x_calc_disp_and_camera_ptsa();

	mc_writel(p->ring1_ptsa_min, MC_RING1_PTSA_MIN);
	mc_writel(p->ring1_ptsa_max, MC_RING1_PTSA_MAX);
	mc_writel(p->ring1_ptsa_rate, MC_RING1_PTSA_RATE);

	mc_writel(p->dis_ptsa_min, MC_DIS_PTSA_MIN);
	mc_writel(p->dis_ptsa_max, MC_DIS_PTSA_MAX);
	mc_writel(p->dis_ptsa_rate, MC_DIS_PTSA_RATE);

	mc_writel(p->disb_ptsa_min, MC_DISB_PTSA_MIN);
	mc_writel(p->disb_ptsa_max, MC_DISB_PTSA_MAX);
	mc_writel(p->disb_ptsa_rate, MC_DISB_PTSA_RATE);
}

static int t21x_update_camera_ptsa_rate(enum tegra_la_id id,
					unsigned int bw_mbps,
					int is_hiso)
{
	struct ptsa_info *p = NULL;
	int ret_code = 0;

	mutex_lock(&disp_and_camera_ptsa_lock);

	if (!is_camera_client(id)) {
		/* Non-camera clients should be handled by t21x_set_la(...) or
		   t21x_set_disp_la(...). */
		pr_err("%s: Ignoring request from a non-camera client.\n",
			__func__);
		pr_err("%s: Non-camera clients should be handled by "
			"t21x_set_la(...) or t21x_set_disp_la(...).\n",
			__func__);
		ret_code = -1;
		goto exit;
	}

	if ((id == ID(VI_W)) &&
		(!is_hiso)) {
		pr_err("%s: VI is stating that its not HISO.\n", __func__);
		pr_err("%s: Ignoring and assuming that VI is HISO because VI "
			"is always supposed to be HISO.\n",
			__func__);
		is_hiso = 1;
	}


	p = &cs->ptsa_info;

	if (id == ID(VI_W)) {
		cs->agg_camera_array[AGG_CAMERA_ID(VE)].is_hiso = is_hiso;
	} else if ((id == ID(ISP_RAB)) ||
			(id == ID(ISP_WAB)) ||
			(id == ID(ISP_WBB))) {
		cs->agg_camera_array[AGG_CAMERA_ID(VE2)].is_hiso = is_hiso;
	} else {
		cs->agg_camera_array[AGG_CAMERA_ID(ISP)].is_hiso = is_hiso;
	}

	cs->camera_bw_array[CAMERA_LA_IDX(id)] = bw_mbps;

	t21x_calc_disp_and_camera_ptsa();

	mc_writel(p->ring1_ptsa_min, MC_RING1_PTSA_MIN);
	mc_writel(p->ring1_ptsa_max, MC_RING1_PTSA_MAX);
	mc_writel(p->ring1_ptsa_rate, MC_RING1_PTSA_RATE);

	mc_writel(p->ve_ptsa_min, MC_VE_PTSA_MIN);
	mc_writel(p->ve_ptsa_max, MC_VE_PTSA_MAX);
	mc_writel(p->ve_ptsa_rate, MC_VE_PTSA_RATE);

	mc_writel(p->ve2_ptsa_min, MC_VE2_PTSA_MIN);
	mc_writel(p->ve2_ptsa_max, MC_VE2_PTSA_MAX);
	mc_writel(p->ve2_ptsa_rate, MC_VE2_PTSA_RATE);

	mc_writel(p->isp_ptsa_min, MC_ISP_PTSA_MIN);
	mc_writel(p->isp_ptsa_max, MC_ISP_PTSA_MAX);
	mc_writel(p->isp_ptsa_rate, MC_ISP_PTSA_RATE);

exit:
	mutex_unlock(&disp_and_camera_ptsa_lock);

	return ret_code;
}

static int t21x_set_la(enum tegra_la_id id,
			unsigned int bw_mbps)
{
	int idx = cs->id_to_index[id];
	struct la_client_info *ci = &cs->la_info_array[idx];
	unsigned int la_to_set = 0;

	if (is_display_client(id)) {
		/* Display clients should be handled by
		   t21x_set_disp_la(...). */
		return -1;
	} else if (id == ID(MSENCSRD)) {
		/* This is a special case. */
		struct clk *emc_clk = clk_get_sys("tegra_emc", "emc");
		unsigned int emc_freq_mhz = clk_get_rate(emc_clk) /
						LA_HZ_TO_MHZ_FACTOR;
		unsigned int val_1 = 53;
		unsigned int val_2 = 24;

		if (210 > emc_freq_mhz)
			val_1 = val_1 * 210 / emc_freq_mhz;

		if (574 > emc_freq_mhz)
			val_2 = val_2 * 574 / emc_freq_mhz;

		la_to_set = min3((unsigned int)MC_LA_MAX_VALUE,
				val_1,
				val_2);
	} else if (ci->la_ref_clk_mhz != 0) {
		/* In this case we need to scale LA with emc frequency. */
		struct clk *emc_clk = clk_get_sys("tegra_emc", "emc");
		unsigned long emc_freq_mhz = clk_get_rate(emc_clk) /
					(unsigned long)LA_HZ_TO_MHZ_FACTOR;

		if (ci->la_ref_clk_mhz <= emc_freq_mhz) {
			la_to_set = min(ci->init_la,
				(unsigned int)MC_LA_MAX_VALUE);
		} else {
			la_to_set = min((unsigned int)(ci->init_la *
					 ci->la_ref_clk_mhz / emc_freq_mhz),
				(unsigned int)MC_LA_MAX_VALUE);
		}
	} else {
		/* In this case we have a client with a static LA value. */
		la_to_set = ci->init_la;
	}

	program_la(ci, la_to_set);
	return 0;
}

static unsigned int t21x_min_la(struct dc_to_la_params *disp_params)
{
	unsigned int min_la_fp = disp_params->drain_time_usec_fp *
				1000 /
				cs->ns_per_tick;

	/* round up */
	if (min_la_fp % LA_FP_FACTOR != 0)
		min_la_fp += LA_FP_FACTOR;

	return LA_FP_TO_REAL(min_la_fp);
}

static int t21x_handle_disp_la(enum tegra_la_id id,
			       unsigned long emc_freq_hz,
			       unsigned int bw_mbps,
			       struct dc_to_la_params disp_params,
			       int write_la)
{
	int idx = 0;
	struct la_client_info *ci = NULL;
	long long la_to_set = 0;
	unsigned int dvfs_time_nsec = 0;
	unsigned int dvfs_buffering_reqd_bytes = 0;
	unsigned int thresh_dvfs_bytes = 0;
	unsigned int total_buf_sz_bytes = 0;
	int effective_mccif_buf_sz = 0;
	long long la_bw_upper_bound_nsec_fp = 0;
	long long la_bw_upper_bound_nsec = 0;
	long long la_nsec = 0;

	if (!is_display_client(id)) {
		/* Non-display clients should be handled by t21x_set_la(...). */
		return -1;
	}

	mutex_lock(&disp_and_camera_ptsa_lock);
	total_dc0_bw = disp_params.total_dc0_bw;
	total_dc1_bw = disp_params.total_dc1_bw;
	cs->update_display_ptsa_rate(cs->disp_bw_array);
	mutex_unlock(&disp_and_camera_ptsa_lock);

	idx = cs->id_to_index[id];
	ci = &cs->la_info_array[idx];
	la_to_set = 0;
	dvfs_time_nsec =
		tegra_get_dvfs_clk_change_latency_nsec(emc_freq_hz / 1000);
	dvfs_buffering_reqd_bytes = bw_mbps *
					dvfs_time_nsec /
					LA_USEC_TO_NSEC_FACTOR;

	thresh_dvfs_bytes =
			disp_params.thresh_lwm_bytes +
			dvfs_buffering_reqd_bytes +
			disp_params.spool_up_buffering_adj_bytes;
	total_buf_sz_bytes =
		cs->disp_clients[DISP_CLIENT_LA_ID(id)].line_buf_sz_bytes +
		cs->disp_clients[DISP_CLIENT_LA_ID(id)].mccif_size_bytes;
	effective_mccif_buf_sz =
		(cs->disp_clients[DISP_CLIENT_LA_ID(id)].line_buf_sz_bytes >
		thresh_dvfs_bytes) ?
		cs->disp_clients[DISP_CLIENT_LA_ID(id)].mccif_size_bytes :
		total_buf_sz_bytes - thresh_dvfs_bytes;

	if (effective_mccif_buf_sz < 0)
		return -1;

	la_bw_upper_bound_nsec_fp = (long long)effective_mccif_buf_sz *
					LA_FP_FACTOR /
					bw_mbps;
	la_bw_upper_bound_nsec_fp = la_bw_upper_bound_nsec_fp *
					LA_FP_FACTOR /
					LA_DISP_CATCHUP_FACTOR_FP;
	la_bw_upper_bound_nsec_fp =
		la_bw_upper_bound_nsec_fp -
		(LA_ST_LA_MINUS_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP +
		 EXP_TIME_EMCCLKS_FP) /
		(emc_freq_hz / LA_HZ_TO_MHZ_FACTOR);
	la_bw_upper_bound_nsec_fp *= LA_USEC_TO_NSEC_FACTOR;
	la_bw_upper_bound_nsec = LA_FP_TO_REAL(la_bw_upper_bound_nsec_fp);


	la_nsec = min(la_bw_upper_bound_nsec,
			(long long)MAX_LA_NSEC);

	la_to_set = min((long long)(la_nsec/cs->ns_per_tick),
			(long long)MC_LA_MAX_VALUE);

	if ((la_to_set < t21x_min_la(&disp_params)) || (la_to_set > 255))
		return -1;

	if (write_la)
		program_la(ci, la_to_set);
	return 0;
}

static int t21x_set_disp_la(enum tegra_la_id id,
			    unsigned long emc_freq_hz,
			    unsigned int bw_mbps,
			    struct dc_to_la_params disp_params)
{
	return t21x_handle_disp_la(id, emc_freq_hz, bw_mbps, disp_params, 1);
}

static int t21x_check_disp_la(enum tegra_la_id id,
			      unsigned long emc_freq_hz,
			      unsigned int bw_mbps,
			      struct dc_to_la_params disp_params)
{
	return t21x_handle_disp_la(id, emc_freq_hz, bw_mbps, disp_params, 0);
}

void tegra_la_get_t21x_specific(struct la_chip_specific *cs_la)
{
	int i = 0;

	cs_la->ns_per_tick = 30;
	cs_la->atom_size = 64;
	cs_la->la_max_value = MC_LA_MAX_VALUE;
	cs_la->la_info_array = t21x_la_info_array;
	cs_la->la_info_array_size = ARRAY_SIZE(t21x_la_info_array);

	cs_la->la_params.fp_factor = LA_FP_FACTOR;
	cs_la->la_params.la_real_to_fp = la_real_to_fp;
	cs_la->la_params.la_fp_to_real = la_fp_to_real;
	cs_la->la_params.static_la_minus_snap_arb_to_row_srt_emcclks_fp =
			LA_ST_LA_MINUS_SNAP_ARB_TO_ROW_SRT_EMCCLKS_FP;
	cs_la->la_params.dram_width_bits = LA_DRAM_WIDTH_BITS;
	cs_la->la_params.disp_catchup_factor_fp = LA_DISP_CATCHUP_FACTOR_FP;

	cs_la->init_ptsa = t21x_init_ptsa;
	cs_la->update_display_ptsa_rate = t21x_update_display_ptsa_rate;
	cs_la->update_camera_ptsa_rate = t21x_update_camera_ptsa_rate;
	cs_la->set_init_la = t21x_set_la;
	cs_la->set_dynamic_la = t21x_set_la;
	cs_la->set_disp_la = t21x_set_disp_la;
	cs_la->check_disp_la = t21x_check_disp_la;
	cs_la->save_ptsa = save_ptsa;
	cs_la->program_ptsa = program_ptsa;
	cs_la->suspend = la_suspend;
	cs_la->resume = la_resume;
	cs = cs_la;

	if (ON_LPDDR4()) {
		emc_min_freq_mhz_fp = 25000;
		emc_min_freq_mhz = 25;
		emc_max_freq_mhz = 2132;
		hi_gd_fp = 1500;
		lo_gd_fp = 18;
		hi_gd_fpa = 14998;
		lo_gd_fpa = 176;
		dda_div = 1;
	} else {
		emc_min_freq_mhz_fp = 12500;
		emc_min_freq_mhz = 12;
		emc_max_freq_mhz = 1200;
		hi_gd_fp = 2000;
		lo_gd_fp = 21;
		hi_gd_fpa = 19998;
		lo_gd_fpa = 208;
		dda_div = 2;
	}

	low_freq_bw = emc_min_freq_mhz_fp * 2 * LA_DRAM_WIDTH_BITS / 8;
	low_freq_bw /= 1000;
	tegra_la_disp_clients_info = cs_la->disp_clients;

	/* set some entries to zero */
	for (i = 0; i < NUM_CAMERA_CLIENTS; i++)
		cs_la->camera_bw_array[i] = 0;
	for (i = 0; i < TEGRA_LA_AGG_CAMERA_NUM_CLIENTS; i++) {
		cs_la->agg_camera_array[i].bw_fp = 0;
		cs_la->agg_camera_array[i].frac_fp = 0;
		cs_la->agg_camera_array[i].ptsa_min = 0;
		cs_la->agg_camera_array[i].ptsa_max = 0;
		cs_la->agg_camera_array[i].is_hiso = false;
	}

	/* set mccif_size_bytes values */
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0A)].mccif_size_bytes = 6144;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0B)].mccif_size_bytes = 6144;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0C)].mccif_size_bytes =
									11520;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAYD)].mccif_size_bytes = 4672;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_T)].mccif_size_bytes = 4672;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HC)].mccif_size_bytes = 4992;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0AB)].mccif_size_bytes =
									11520;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0BB)].mccif_size_bytes =
									6144;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0CB)].mccif_size_bytes =
									6144;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HCB)].mccif_size_bytes =
									4992;

	/* set line_buf_sz_bytes values */
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0A)].line_buf_sz_bytes =
									151552;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0B)].line_buf_sz_bytes =
									112640;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0C)].line_buf_sz_bytes =
									112640;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAYD)].line_buf_sz_bytes = 18432;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_T)].line_buf_sz_bytes =
									18432;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HC)].line_buf_sz_bytes = 320;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0AB)].line_buf_sz_bytes =
									112640;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0BB)].line_buf_sz_bytes =
									112640;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0CB)].line_buf_sz_bytes =
									112640;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HCB)].line_buf_sz_bytes =
									320;

	/* set win_type values */
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0A)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULL;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0B)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLA;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0C)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLA;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAYD)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_SIMPLE;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HC)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_CURSOR;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_T)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_SIMPLE;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0AB)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLB;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0BB)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLB;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_0CB)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_FULLB;
	cs_la->disp_clients[DISP_CLIENT_ID(DISPLAY_HCB)].win_type =
						TEGRA_LA_DISP_WIN_TYPE_CURSOR;
}
