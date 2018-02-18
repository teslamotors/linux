/*
 * Copyright (C) 2015-2016, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/of.h>
#include <linux/resource.h>
#include <linux/of_address.h>
#include <linux/version.h>
#include <asm/io.h>

#include <linux/platform/tegra/tegra_emc.h>
#include <linux/platform/tegra/mc.h>
#include <linux/platform/tegra/latency_allowance.h>
#include <linux/platform/tegra/emc_bwmgr.h>

#include "la_priv.h"


#define T18X_LA_EMEM_CHANNEL_ENABLE_MASK		0xf
#define T18X_LA_ECC_ENABLE_MASK				0x1
#define T18X_LA_2_STAGE_ECC_ISO_DDA_FACTOR_FP		1400U
#define T18X_LA_DISP_CATCHUP_FACTOR_FP			1100U
#define T18X_LA_MCCIF_BUF_SZ_BYTES_FP			30976000U
#define T18X_LA_ST_LA_SNAP_ARB_TO_ROW_SRT_EMCCLKS	54U
#define T18X_LA_ST_LA_MINUS_SNAP_ARB_TO_ROW_SRT_EMCCLKS	130U
#define T18X_LA_EXPIRATION_TIME_EMCCLKS			210U
#define T18X_LA_MAX_DRAIN_TIME_USEC			10U
#define T18X_LA_CONS_MEM_EFF_DIV_FACTOR			2U

/*
 * For T18X we need varying fixed point accuracies. "fp2" variables provide an
 * accuracy of 1/100. And "fp5" variables provide an accuracy of 1/100000.
 */
#define T18X_LA_FP2_FACTOR			100U
#define T18X_LA_REAL_TO_FP2(val)		((val) * T18X_LA_FP2_FACTOR)
#define T18X_LA_FP_TO_FP2(val)			((val) / 10U)
#define T18X_LA_FP5_TO_FPA(val)			((val) / 10U)

#define T18X_LA(a, r, i, ct, sr, la, clk)		\
{							\
	.reg_addr = MC_LATENCY_ALLOWANCE_ ## a,		\
	.mask = MASK(r),				\
	.shift = SHIFT(r),				\
	.id = ID(i),					\
	.name = __stringify(i),				\
	.client_type = TEGRA_LA_ ## ct ## _CLIENT,	\
	.min_scaling_ratio = sr,			\
	.init_la = la,					\
	.la_ref_clk_mhz = clk				\
}

#define T18X_MC_SET_INIT_PTSA_MIN_MAX(p, client, tt, min, max)		\
	do {								\
		(p)->client ## _traffic_type = TEGRA_LA_ ## tt;		\
		(p)->client ## _ptsa_min = (unsigned int)(min) &	\
			MC_PTSA_MIN_DEFAULT_MASK;			\
		(p)->client ## _ptsa_max = (unsigned int)(max) &	\
			MC_PTSA_MAX_DEFAULT_MASK;			\
	} while (0)

#define T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, client, tt, min, max, rate) \
	do {								\
		(p)->client ## _traffic_type = TEGRA_LA_ ## tt;		\
		(p)->client ## _ptsa_min = (unsigned int)(min) &	\
			MC_PTSA_MIN_DEFAULT_MASK;			\
		(p)->client ## _ptsa_max = (unsigned int)(max) &	\
			MC_PTSA_MAX_DEFAULT_MASK;			\
		(p)->client ## _ptsa_rate = (unsigned int)(rate) &	\
			MC_PTSA_RATE_DEFAULT_MASK;			\
	} while (0)

#define T18X_CALC_AND_UPDATE_ISO_PTSA(p, field, reg, bw_mbps)		\
	do {								\
		(p)->field ##_ptsa_rate =				\
			__t18x_fraction2dda_fp(				\
				t18x_bwfp_to_fractionfpa(		\
					bw_mbps * iso_dda_factor_fp),	\
				hub_dda_div,				\
				MC_PTSA_RATE_DEFAULT_MASK,		\
				(p)->field ##_traffic_type);		\
		mc_writel((p)->field ##_ptsa_rate,			\
			MC_ ## reg ## _PTSA_RATE);			\
	} while (0)


static struct la_chip_specific *cs;
static unsigned int dram_type;
static unsigned int num_channels;
static unsigned int row_srt_sz_bytes;
static unsigned int dram_emc_freq_factor;
static unsigned int hi_freq_fp;
static unsigned int lo_freq_fp;
static unsigned int hub_dda_div;
static unsigned int r0_dda_div;
static unsigned int dram_width_bytes;
static unsigned int hi_gd_fpa;
static unsigned int hi_gd_fp5;
static unsigned int lo_gd_fpa;
static unsigned int lo_gd_fp5;
static unsigned int iso_dda_factor_fp;

static struct la_client_info t18x_la_info_array[] = {
	T18X_LA(AFI_0, 7 : 0,  AFIR, DYNAMIC_READ, 0, 105, 250),
	T18X_LA(AFI_0, 23 : 16, AFIW, WRITE, 0, 128, 0),
	T18X_LA(AON_0, 7 : 0, AONR, CONSTANT_READ, 0, 10, 0),
	T18X_LA(AON_0, 23 : 16, AONW, WRITE, 0, 128, 0),
	T18X_LA(AONDMA_0, 7 : 0, AONDMAR, DYNAMIC_READ, 1, 189, 102),
	T18X_LA(AONDMA_0, 23 : 16, AONDMAW, WRITE, 0, 128, 0),
	T18X_LA(APEDMA_0, 7 : 0, APEDMAR, DYNAMIC_READ, 1, 188, 102),
	T18X_LA(APEDMA_0, 23 : 16, APEDMAW, WRITE, 0, 128, 0),
	T18X_LA(APE_0, 7 : 0, APER, CONSTANT_READ, 0, 10, 0),
	T18X_LA(APE_0, 23 : 16, APEW, WRITE, 0, 128, 0),
	T18X_LA(AXIS_0, 7 : 0, AXISR, DYNAMIC_READ, 1, 124, 204),
	T18X_LA(AXIS_0, 23 : 16, AXISW, WRITE, 0, 128, 0),
	T18X_LA(BPMP_0, 7 : 0, BPMPR, CONSTANT_READ, 0, 10, 0),
	T18X_LA(BPMP_0, 23 : 16, BPMPW, WRITE, 0, 128, 0),
	T18X_LA(BPMPDMA_0, 7 : 0, BPMPDMAR, DYNAMIC_READ, 1, 189, 102),
	T18X_LA(BPMPDMA_0, 23 : 16, BPMPDMAW, WRITE, 0, 128, 0),
	T18X_LA(EQOS_0, 7 : 0, EQOSR, DYNAMIC_READ, 0, 42, 600),
	T18X_LA(EQOS_0, 23 : 16, EQOSW, WRITE, 0, 128, 0),
	T18X_LA(ETR_0, 7 : 0, ETRR, CONSTANT_READ, 0, 80, 0),
	T18X_LA(ETR_0, 23 : 16, ETRW, WRITE, 0, 128, 0),
	T18X_LA(GPU_0, 7 : 0, GPUSRD, DYNAMIC_READ, 0, 32, 800),
	T18X_LA(GPU_0, 23 : 16, GPUSWR, WRITE, 0, 128, 0),
	T18X_LA(GPU2_0, 7 : 0, GPUSRD2, DYNAMIC_READ, 0, 32, 800),
	T18X_LA(GPU2_0, 23 : 16, GPUSWR2, WRITE, 0, 128, 0),
	T18X_LA(HDA_0, 7 : 0, HDAR, CONSTANT_READ, 0, 36, 0),
	T18X_LA(HDA_0, 23 : 16, HDAW, WRITE, 0, 128, 0),
	T18X_LA(HC_0, 7 : 0, HOST1X_DMAR, DYNAMIC_READ, 1, 189, 102),
	T18X_LA(ISP2_0, 7 : 0, ISP_RA, DYNAMIC_READ, 0, 83, 307),
	T18X_LA(ISP2_1, 7 : 0, ISP_WA, WRITE, 0, 128, 0),
	T18X_LA(ISP2_1, 23 : 16, ISP_WB, WRITE, 0, 128, 0),
	T18X_LA(MPCORE_0, 7 : 0, MPCORER, CONSTANT_READ, 0, 4, 0),
	T18X_LA(MPCORE_0, 23 : 16, MPCOREW, WRITE, 0, 128, 0),
	T18X_LA(NVDEC_0, 7 : 0, NVDECR, DYNAMIC_READ, 0, 58, 203),
	T18X_LA(NVDEC_0, 23 : 16, NVDECW, WRITE, 0, 128, 0),
	T18X_LA(NVDISPLAY_0, 7 : 0, NVDISPLAYR, DISPLAY_READ, 0, 0, 0),
	T18X_LA(NVENC_0, 7 : 0, NVENCSRD, DYNAMIC_READ, 0, 34, 535),
	T18X_LA(NVENC_0, 23 : 16, NVENCSWR, WRITE, 0, 128, 0),
	T18X_LA(NVJPG_0, 7 : 0, NVJPGSRD, DYNAMIC_READ, 0, 127, 197),
	T18X_LA(NVJPG_0, 23 : 16, NVJPGSWR, WRITE, 0, 128, 0),
	T18X_LA(PTC_0, 7 : 0, PTCR, CONSTANT_READ, 0, 0, 0),
	T18X_LA(SATA_0, 7 : 0, SATAR, DYNAMIC_READ, 1, 57, 450),
	T18X_LA(SATA_0, 23 : 16, SATAW, WRITE, 0, 128, 0),
	T18X_LA(SCE_0, 7 : 0, SCER, CONSTANT_READ, 0, 10, 0),
	T18X_LA(SCE_0, 23 : 16, SCEW, WRITE, 0, 128, 0),
	T18X_LA(SCEDMA_0, 7 : 0, SCEDMAR, DYNAMIC_READ, 1, 189, 102),
	T18X_LA(SCEDMA_0, 23 : 16, SCEDMAW, WRITE, 0, 128, 0),
	T18X_LA(SDMMC_0, 7 : 0, SDMMCR, DYNAMIC_READ, 1, 271, 30),
	T18X_LA(SDMMC_0, 23 : 16, SDMMCW, WRITE, 0, 128, 0),
	T18X_LA(SDMMCA_0, 7 : 0, SDMMCRA, DYNAMIC_READ, 1, 269, 30),
	T18X_LA(SDMMCA_0, 23 : 16, SDMMCWA, WRITE, 0, 128, 0),
	T18X_LA(SDMMCAA_0, 7 : 0, SDMMCRAA, DYNAMIC_READ, 1, 271, 30),
	T18X_LA(SDMMCAA_0, 23 : 16, SDMMCWAA, WRITE, 0, 128, 0),
	T18X_LA(SDMMCAB_0, 7 : 0, SDMMCRAB, DYNAMIC_READ, 1, 241, 67),
	T18X_LA(SDMMCAB_0, 23 : 16, SDMMCWAB, WRITE, 0, 128, 0),
	T18X_LA(SE_0, 7 : 0, SESRD, DYNAMIC_READ, 0, 122, 208),
	T18X_LA(SE_0, 23 : 16, SESWR, WRITE, 0, 128, 0),
	T18X_LA(TSEC_0, 7 : 0, TSECSRD, DYNAMIC_READ, 1, 189, 102),
	T18X_LA(TSEC_0, 23 : 16, TSECSWR, WRITE, 0, 128, 0),
	T18X_LA(TSECB_0, 7 : 0, TSECBSRD, DYNAMIC_READ, 1, 189, 102),
	T18X_LA(TSECB_0, 23 : 16, TSECBSWR, WRITE, 0, 128, 0),
	T18X_LA(UFSHC_0, 7 : 0, UFSHCR, DYNAMIC_READ, 0, 135, 187),
	T18X_LA(UFSHC_0, 23 : 16, UFSHCW, WRITE, 0, 128, 0),
	T18X_LA(VI2_0, 7 : 0, VI_W, WRITE, 0, 128, 0),
	T18X_LA(VIC_0, 7 : 0, VICSRD, DYNAMIC_READ, 0, 32, 800),
	T18X_LA(VIC_0, 23 : 16, VICSWR, WRITE, 0, 128, 0),
	T18X_LA(XUSB_1, 7 : 0, XUSB_DEVR, DYNAMIC_READ, 1, 123, 204),
	T18X_LA(XUSB_1, 23 : 16, XUSB_DEVW, WRITE, 0, 128, 0),
	T18X_LA(XUSB_0, 7 : 0, XUSB_HOSTR, DYNAMIC_READ, 1, 123, 204),
	T18X_LA(XUSB_0, 23 : 16, XUSB_HOSTW, WRITE, 0, 128, 0),

	/* end of list */
	T18X_LA(ROC_DMA_R_0, 0 : 0, MAX_ID, CONSTANT_READ, 0, 0, 0)
};


static inline unsigned int __t18x_fraction2dda_fp(unsigned int fraction_fpa,
					unsigned int div,
					unsigned int mask,
					enum la_traffic_type traffic_type)
{
	unsigned int dda = 0;
	int i = 0;
	unsigned int r = 0;

	fraction_fpa /= div;

	for (i = 0; i < EMEM_PTSA_RATE_WIDTH; i++) {
		fraction_fpa *= 2;
		r = LA_FPA_TO_REAL(fraction_fpa);
		dda = (dda << 1) | (unsigned int)(r);
		fraction_fpa -= LA_REAL_TO_FPA(r);
	}
	if (fraction_fpa > 0) {
		/* Do not round up if the calculated dda is at the mask value
		   already, it will overflow */
		if (dda != mask) {
			if (traffic_type != TEGRA_LA_NISO ||
				fraction_fpa >= 5000 ||
				dda == 0) {
				/* to round up dda value */
				dda++;
			}
		}
	}

	return min(dda, (unsigned int)MAX_DDA_RATE);
}

static inline unsigned int t18x_bw_to_fractionfpa(unsigned int bw_mbps)
{
	unsigned int ret_val =
			(lo_gd_fpa * T18X_LA_REAL_TO_FP2(bw_mbps)) /
			(T18X_LA_FP_TO_FP2(lo_freq_fp) * dram_width_bytes);

	if (bw_mbps == 0)
		return 0;
	else
		return max((unsigned int)1, ret_val);
}

static inline unsigned int t18x_bwfp_to_fractionfpa(unsigned int bw_mbps_fp)
{
	unsigned int ret_val =
			(lo_gd_fpa * T18X_LA_FP_TO_FP2(bw_mbps_fp)) /
			(T18X_LA_FP_TO_FP2(lo_freq_fp) * dram_width_bytes);

	if (bw_mbps_fp == 0)
		return 0;
	else
		return max((unsigned int)1, ret_val);
}

static void program_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;

	WRITE_PTSA_MIN_MAX_RATE(p, dis, DIS);
	WRITE_PTSA_MIN_MAX_RATE(p, ve, VE);
	WRITE_PTSA_MIN_MAX_RATE(p, isp, ISP);
	WRITE_PTSA_MIN_MAX_RATE(p, apedmapc, APEDMAPC);
	WRITE_PTSA_MIN_MAX_RATE(p, eqospc, EQOSPC);
	WRITE_PTSA_MIN_MAX(p, ring1_rd_nb, RING1_RD_NB);
	WRITE_PTSA_MIN_MAX(p, ring1_wr_nb, RING1_WR_NB);
	WRITE_PTSA_MIN_MAX_RATE(p, ring1_rd_b, RING1_RD_B);
	WRITE_PTSA_MIN_MAX_RATE(p, ring1_wr_b, RING1_WR_B);
	WRITE_PTSA_MIN_MAX_RATE(p, ring2, RING2);
	WRITE_PTSA_MIN_MAX(p, mll_mpcorer, MLL_MPCORER);
	WRITE_PTSA_MIN_MAX_RATE(p, smmu, SMMU_SMMU);
	WRITE_PTSA_MIN_MAX_RATE(p, bpmpdmapc, BPMPDMAPC);

	WRITE_PTSA_MIN_MAX_RATE(p, aondmapc, AONDMAPC);
	WRITE_PTSA_MIN_MAX_RATE(p, aonpc, AONPC);
	WRITE_PTSA_MIN_MAX_RATE(p, apb, APB);
	WRITE_PTSA_MIN_MAX_RATE(p, aud, AUD);
	WRITE_PTSA_MIN_MAX_RATE(p, bpmppc, BPMPPC);
	WRITE_PTSA_MIN_MAX_RATE(p, dfd, DFD);
	WRITE_PTSA_MIN_MAX_RATE(p, ftop, FTOP);
	WRITE_PTSA_MIN_MAX_RATE(p, gk, GK);
	WRITE_PTSA_MIN_MAX_RATE(p, gk2, GK2);
	WRITE_PTSA_MIN_MAX_RATE(p, hdapc, HDAPC);
	WRITE_PTSA_MIN_MAX_RATE(p, host, HOST);
	WRITE_PTSA_MIN_MAX_RATE(p, jpg, JPG);
	WRITE_PTSA_MIN_MAX_RATE(p, mse, MSE);
	WRITE_PTSA_MIN_MAX_RATE(p, mse2, MSE2);
	WRITE_PTSA_MIN_MAX_RATE(p, nic, NIC);
	WRITE_PTSA_MIN_MAX_RATE(p, nvd, NVD);
	WRITE_PTSA_MIN_MAX_RATE(p, nvd3, NVD3);
	WRITE_PTSA_MIN_MAX_RATE(p, pcx, PCX);
	WRITE_PTSA_MIN_MAX_RATE(p, roc_dma_r, ROC_DMA_R);
	WRITE_PTSA_MIN_MAX_RATE(p, sax, SAX);
	WRITE_PTSA_MIN_MAX_RATE(p, scedmapc, SCEDMAPC);
	WRITE_PTSA_MIN_MAX_RATE(p, scepc, SCEPC);
	WRITE_PTSA_MIN_MAX_RATE(p, sd, SD);
	WRITE_PTSA_MIN_MAX_RATE(p, sdm, SDM);
	WRITE_PTSA_MIN_MAX_RATE(p, sdm1, SDM1);
	WRITE_PTSA_MIN_MAX_RATE(p, ufshcpc, UFSHCPC);
	WRITE_PTSA_MIN_MAX_RATE(p, usbd, USBD);
	WRITE_PTSA_MIN_MAX_RATE(p, usbx, USBX);
	WRITE_PTSA_MIN_MAX_RATE(p, vicpc, VICPC);
	WRITE_PTSA_MIN_MAX_RATE(p, vicpc3, VICPC3);

	/* update shadowed registers */
	mc_writel(1, MC_TIMING_CONTROL);
}

static void save_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;

	READ_PTSA_MIN_MAX_RATE(p, dis, DIS);
	READ_PTSA_MIN_MAX_RATE(p, ve, VE);
	READ_PTSA_MIN_MAX_RATE(p, isp, ISP);
	READ_PTSA_MIN_MAX_RATE(p, apedmapc, APEDMAPC);
	READ_PTSA_MIN_MAX_RATE(p, eqospc, EQOSPC);
	READ_PTSA_MIN_MAX(p, ring1_rd_nb, RING1_RD_NB);
	READ_PTSA_MIN_MAX(p, ring1_wr_nb, RING1_WR_NB);
	READ_PTSA_MIN_MAX_RATE(p, ring1_rd_b, RING1_RD_B);
	READ_PTSA_MIN_MAX_RATE(p, ring1_wr_b, RING1_WR_B);
	READ_PTSA_MIN_MAX_RATE(p, ring2, RING2);
	READ_PTSA_MIN_MAX(p, mll_mpcorer, MLL_MPCORER);
	READ_PTSA_MIN_MAX_RATE(p, smmu, SMMU_SMMU);
	READ_PTSA_MIN_MAX_RATE(p, bpmpdmapc, BPMPDMAPC);

	READ_PTSA_MIN_MAX_RATE(p, aondmapc, AONDMAPC);
	READ_PTSA_MIN_MAX_RATE(p, aonpc, AONPC);
	READ_PTSA_MIN_MAX_RATE(p, apb, APB);
	READ_PTSA_MIN_MAX_RATE(p, aud, AUD);
	READ_PTSA_MIN_MAX_RATE(p, bpmppc, BPMPPC);
	READ_PTSA_MIN_MAX_RATE(p, dfd, DFD);
	READ_PTSA_MIN_MAX_RATE(p, ftop, FTOP);
	READ_PTSA_MIN_MAX_RATE(p, gk, GK);
	READ_PTSA_MIN_MAX_RATE(p, gk2, GK2);
	READ_PTSA_MIN_MAX_RATE(p, hdapc, HDAPC);
	READ_PTSA_MIN_MAX_RATE(p, host, HOST);
	READ_PTSA_MIN_MAX_RATE(p, jpg, JPG);
	READ_PTSA_MIN_MAX_RATE(p, mse, MSE);
	READ_PTSA_MIN_MAX_RATE(p, mse2, MSE2);
	READ_PTSA_MIN_MAX_RATE(p, nic, NIC);
	READ_PTSA_MIN_MAX_RATE(p, nvd, NVD);
	READ_PTSA_MIN_MAX_RATE(p, nvd3, NVD3);
	READ_PTSA_MIN_MAX_RATE(p, pcx, PCX);
	READ_PTSA_MIN_MAX_RATE(p, roc_dma_r, ROC_DMA_R);
	READ_PTSA_MIN_MAX_RATE(p, sax, SAX);
	READ_PTSA_MIN_MAX_RATE(p, scedmapc, SCEDMAPC);
	READ_PTSA_MIN_MAX_RATE(p, scepc, SCEPC);
	READ_PTSA_MIN_MAX_RATE(p, sd, SD);
	READ_PTSA_MIN_MAX_RATE(p, sdm, SDM);
	READ_PTSA_MIN_MAX_RATE(p, sdm1, SDM1);
	READ_PTSA_MIN_MAX_RATE(p, ufshcpc, UFSHCPC);
	READ_PTSA_MIN_MAX_RATE(p, usbd, USBD);
	READ_PTSA_MIN_MAX_RATE(p, usbx, USBX);
	READ_PTSA_MIN_MAX_RATE(p, vicpc, VICPC);
	READ_PTSA_MIN_MAX_RATE(p, vicpc3, VICPC3);
}

static void t18x_init_ptsa(void)
{
	struct ptsa_info *p = &cs->ptsa_info;
	unsigned int eqos_bw;

	/* initialize PTSA min/max values */
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, aondmapc, SISO, 1, 1, 0);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, aonpc, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, apb, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX(p, apedmapc, HISO, -5, 31);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, aud, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, bpmpdmapc, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, bpmppc, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, dfd, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX(p, dis, HISO, -5, 31);
	T18X_MC_SET_INIT_PTSA_MIN_MAX(p, eqospc, HISO, -5, 31);
	T18X_MC_SET_INIT_PTSA_MIN_MAX(p, ftop, NISO, -2, 0);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, gk, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, gk2, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, hdapc, SISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, host, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, isp, SISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, jpg, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX(p, mll_mpcorer, NISO, -4, 4);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, mse, SISO, 1, 1, 0);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, mse2, SISO, 1, 1, 0);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, nic, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, nvd, SISO, 1, 1, 0);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, nvd3, SISO, 1, 1, 0);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, pcx, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, roc_dma_r, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, ring1_rd_b, NISO, 62, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX(p, ring1_rd_nb, HISO, -5, 31);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, ring1_wr_b, NISO, 62, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX(p, ring1_wr_nb, HISO, -5, 31);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, ring2, NISO, -2, 0, 12);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, sax, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, scedmapc, SISO, 1, 1, 0);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, scepc, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, sd, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, sdm, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, sdm1, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, smmu, NISO, 1, 1, 0);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, ufshcpc, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, usbd, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, usbx, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX(p, ve, HISO, 1, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, vicpc, NISO, -2, 0, 1);
	T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, vicpc3, NISO, -2, 0, 1);


	p->ring1_rd_b_ptsa_rate = (unsigned int)(1) & MC_PTSA_RATE_DEFAULT_MASK;
	p->ring1_wr_b_ptsa_rate = (unsigned int)(1) & MC_PTSA_RATE_DEFAULT_MASK;

	p->ring2_ptsa_rate = (unsigned int)(12) & MC_PTSA_RATE_DEFAULT_MASK;

	p->bpmpdmapc_ptsa_rate = (unsigned int)(1) & MC_PTSA_RATE_DEFAULT_MASK;

	eqos_bw = LA_FP_TO_REAL(250 * T18X_LA_2_STAGE_ECC_ISO_DDA_FACTOR_FP);
	p->eqospc_ptsa_rate =
		__t18x_fraction2dda_fp(t18x_bw_to_fractionfpa(eqos_bw),
					hub_dda_div,
					MC_PTSA_RATE_DEFAULT_MASK,
					p->eqospc_traffic_type);

	program_ptsa();
}

static int t18x_update_camera_ptsa_rate(enum tegra_la_id id,
					unsigned int bw_mbps,
					int is_hiso)
{
	struct ptsa_info *p = &cs->ptsa_info;

	if ((id == ID(ISP_RA)) ||
		(id == ID(ISP_WA)) ||
		(id == ID(ISP_WB))) {
		unsigned int client_traffic_type_config_2 =
				mc_readl(MC_CLIENT_TRAFFIC_TYPE_CONFIG_2);

		if (is_hiso) {
			T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, isp, HISO,
								1, 1, 0);
			WRITE_PTSA_MIN_MAX_RATE(p, isp, ISP);

			/* Make ISPWA and ISPWB non-blocking clients */
			mc_writel(client_traffic_type_config_2 & ~0xc0,
					MC_CLIENT_TRAFFIC_TYPE_CONFIG_2);

			/* Make ISP_RA an ISO client */
			mc_writel(0x10, MC_EMEM_ARB_ISOCHRONOUS_2);
		} else {
			T18X_MC_SET_INIT_PTSA_MIN_MAX_RATE(p, isp, SISO,
								-2, 0, 1);
			WRITE_PTSA_MIN_MAX_RATE(p, isp, ISP);

			/* Make ISPWA and ISPWB blocking clients */
			mc_writel(client_traffic_type_config_2 | 0xc0,
					MC_CLIENT_TRAFFIC_TYPE_CONFIG_2);

			/* Make ISP_RA a non-ISO client */
			mc_writel(0x0, MC_EMEM_ARB_ISOCHRONOUS_2);
		}
	} else if (id == ID(VI_W)) {
		if (!is_hiso)
			pr_err("%s: Someone is trying to set VI\\VE into SISO "
				"mode. Ignoring request because VI\\VE is "
				"always HISO.\n",
				__func__);

		T18X_CALC_AND_UPDATE_ISO_PTSA(p, ve, VE, bw_mbps);
	} else {
		int idx = cs->id_to_index[id];
		char *name = cs->la_info_array[idx].name;

		pr_err("%s: Ignoring PTSA update request from %s because "
			"its not a camera client.\n",
			__func__, name);
		return -1;
	}

	/* update shadowed registers */
	mc_writel(1, MC_TIMING_CONTROL);
	return 0;
}

static int t18x_set_init_la(enum tegra_la_id id,
			unsigned int bw_mbps)
{
	int idx = cs->id_to_index[id];
	struct la_client_info *ci = &cs->la_info_array[idx];
	unsigned int la_to_set = 0;

	/* We only have to program init LA values for constant read
	   clients. All other clients will either be handled by BPMP or
	   t18x_handle_disp_la(). */
	if (ci->client_type == TEGRA_LA_CONSTANT_READ_CLIENT) {
		la_to_set = ci->init_la;
	}

	program_la(ci, la_to_set);
	return 0;
}

static int t18x_set_dynamic_la(enum tegra_la_id id,
			unsigned int bw_mbps)
{
	struct ptsa_info *p = &cs->ptsa_info;

	if ((id == ID(APEDMAR)) ||
		(id == ID(APEDMAW))) {
		T18X_CALC_AND_UPDATE_ISO_PTSA(p, apedmapc, APEDMAPC, bw_mbps);
	} else if ((id == ID(ISP_RA)) ||
			(id == ID(ISP_WA)) ||
			(id == ID(ISP_WB)) ||
			(id == ID(VI_W))) {
		int idx = cs->id_to_index[id];
		char *name = cs->la_info_array[idx].name;

		pr_warn("%s: Ignoring LA update request from %s because its "
			"LA programming is part of the EMC DVFS table.\n",
			__func__, name);
		return 0;
	} else if ((id == ID(EQOSR)) ||
			(id == ID(EQOSW))) {
		pr_err("%s: Ignoring LA\\PTSA update request from EQOS "
			"because its PTSA rate is static.\n",
			__func__);
		return -1;
	} else if (id == ID(NVDISPLAYR)) {
		pr_err("%s: Ignoring LA\\PTSA update request from display. "
			"Display should be handled by t18x_handle_disp_la(...).\n",
			__func__);
		return -1;
	} else {
		int idx = cs->id_to_index[id];
		char *name = cs->la_info_array[idx].name;

		pr_err("%s: Ignoring LA\\PTSA update request from %s because "
			"its not a HISO\\SISO client.\n",
			__func__, name);
		return -1;
	}

	/* update shadowed registers */
	mc_writel(1, MC_TIMING_CONTROL);
	return 0;
}

static int t18x_handle_disp_la(enum tegra_la_id id,
			       unsigned long emc_freq_hz,
			       unsigned int bw_mbps,
			       struct dc_to_la_params disp_params,
			       int write_la)
{
	/* NOTE: We may need to divide "emc_freq_hz" with dram_emc_freq_factor
	   because "emc_freq_hz" may actually be the dram frequency. */
	int idx = cs->id_to_index[id];
	struct la_client_info *ci = &cs->la_info_array[idx];
	struct ptsa_info *p = &cs->ptsa_info;
	unsigned long emc_freq_mhz = 0;
	unsigned int bw_mbps_disp_catchup_factor_fp =
						bw_mbps *
						T18X_LA_DISP_CATCHUP_FACTOR_FP;
	unsigned int eff_row_srt_sz_bytes = 0;
	unsigned int drain_time_usec_fp = 0;
	int la_bw_upper_bound_usec_fp = 0;
	int la_to_set_fp = 0;
	unsigned int la_to_set = 0;

	if (id != ID(NVDISPLAYR)) {
		char *name = ci->name;

		pr_err("%s: Ignoring LA\\PTSA update request from %s because "
			"its not a display client.\n",
			__func__, name);
		return -1;
	}

	emc_freq_mhz = emc_freq_hz / LA_HZ_TO_MHZ_FACTOR;
	la_bw_upper_bound_usec_fp =
		(unsigned long)T18X_LA_MCCIF_BUF_SZ_BYTES_FP *
			(unsigned long)LA_FP_FACTOR /
			(unsigned long)bw_mbps_disp_catchup_factor_fp -
		LA_REAL_TO_FP(T18X_LA_ST_LA_MINUS_SNAP_ARB_TO_ROW_SRT_EMCCLKS +
				T18X_LA_EXPIRATION_TIME_EMCCLKS) /
			emc_freq_mhz;
	eff_row_srt_sz_bytes = min(row_srt_sz_bytes,
					(unsigned int)(2 * dram_width_bytes *
							(emc_freq_mhz + 50)));
	eff_row_srt_sz_bytes =
		min(eff_row_srt_sz_bytes,
			(unsigned int)((T18X_LA_MAX_DRAIN_TIME_USEC *
				emc_freq_mhz -
				T18X_LA_ST_LA_SNAP_ARB_TO_ROW_SRT_EMCCLKS) *
				2 *
				dram_width_bytes /
				T18X_LA_CONS_MEM_EFF_DIV_FACTOR));
	drain_time_usec_fp = LA_REAL_TO_FP(eff_row_srt_sz_bytes) /
				(emc_freq_mhz *
				dram_width_bytes *
				2 /
				T18X_LA_CONS_MEM_EFF_DIV_FACTOR);
	drain_time_usec_fp +=
		LA_REAL_TO_FP(T18X_LA_ST_LA_SNAP_ARB_TO_ROW_SRT_EMCCLKS) /
		emc_freq_mhz;
	if ((int)drain_time_usec_fp > (int)la_bw_upper_bound_usec_fp)
		return -1;

	la_to_set_fp = la_bw_upper_bound_usec_fp *
			(int)1000 /
			(int)cs->ns_per_tick;
	if (la_to_set_fp < 0)
		return -1;
	/* rounding */
	la_to_set_fp += 500;
	la_to_set = LA_FP_TO_REAL(la_to_set_fp);
	la_to_set = min(la_to_set, MC_LA_MAX_VALUE);
	if (write_la) {
		program_la(ci, la_to_set);
		T18X_CALC_AND_UPDATE_ISO_PTSA(p, dis, DIS, bw_mbps);
	}
	return 0;
}

static int t18x_set_disp_la(enum tegra_la_id id,
			    unsigned long emc_freq_hz,
			    unsigned int bw_mbps,
			    struct dc_to_la_params disp_params)
{
	return t18x_handle_disp_la(id, emc_freq_hz, bw_mbps, disp_params, 1);
}

static int t18x_check_disp_la(enum tegra_la_id id,
			      unsigned long emc_freq_hz,
			      unsigned int bw_mbps,
			      struct dc_to_la_params disp_params)
{
	return t18x_handle_disp_la(id, emc_freq_hz, bw_mbps, disp_params, 0);
}

void tegra_la_get_t18x_specific(struct la_chip_specific *cs_la)
{
	int i;
	unsigned int channel_enable;
	unsigned int adj_lo_freq_fp;
	int dram_ecc_enabled;
	unsigned int client_traffic_type_config_2;

	cs_la->ns_per_tick = 30;

	cs_la->la_info_array = t18x_la_info_array;
	cs_la->la_info_array_size = ARRAY_SIZE(t18x_la_info_array);

	cs_la->init_ptsa = t18x_init_ptsa;
	cs_la->update_camera_ptsa_rate = t18x_update_camera_ptsa_rate;
	cs_la->set_init_la = t18x_set_init_la;
	cs_la->set_dynamic_la = t18x_set_dynamic_la;
	cs_la->set_disp_la = t18x_set_disp_la;
	cs_la->check_disp_la = t18x_check_disp_la;
	cs_la->save_ptsa = save_ptsa;
	cs_la->program_ptsa = program_ptsa;
	cs_la->suspend = la_suspend;
	cs_la->resume = la_resume;
	cs = cs_la;

	dram_type = tegra_emc_get_dram_type();

	channel_enable = mc_readl(MC_EMEM_ADR_CFG_CHANNEL_ENABLE) &
					T18X_LA_EMEM_CHANNEL_ENABLE_MASK;
	num_channels = 0;
	for (i = 0; i < 4; i++) {
		if (channel_enable & 0x1)
			num_channels++;
		channel_enable >>= 1;
	}
	row_srt_sz_bytes = 64 * 64 * num_channels;

	dram_emc_freq_factor = 1;
	hi_gd_fpa = 14998;
	hi_gd_fp5 = 149976;

	if (dram_type == DRAM_TYPE_DDR3) {
		hi_freq_fp = LA_REAL_TO_FP(1200);
		lo_freq_fp = 12500;
		hub_dda_div = 2;
		r0_dda_div = 1;
		hi_gd_fpa = 19998;
		hi_gd_fp5 = 199976;

		if (num_channels == 1)
			dram_width_bytes = 16;
		else if (num_channels == 2)
			dram_width_bytes = 32;
	} else if (dram_type == DRAM_TYPE_LPDDR4) {
		dram_emc_freq_factor = 2;
		hi_freq_fp = LA_REAL_TO_FP(2132);
		lo_freq_fp = LA_REAL_TO_FP(25);
		hub_dda_div = 1;
		r0_dda_div = 2;

		if (num_channels == 2)
			dram_width_bytes = 16;
		else if (num_channels == 4)
			dram_width_bytes = 32;
	}

	adj_lo_freq_fp = lo_freq_fp / 2;
	lo_gd_fpa = (hi_gd_fpa * adj_lo_freq_fp) / (hi_freq_fp / 2);
	lo_gd_fp5 = (hi_gd_fp5 * adj_lo_freq_fp) / (hi_freq_fp / 2);

	dram_ecc_enabled = mc_readl(MC_ECC_CONTROL) & T18X_LA_ECC_ENABLE_MASK;
	if (dram_ecc_enabled)
		iso_dda_factor_fp = 1400;
	else
		iso_dda_factor_fp = 1200;

	/* Make ISPWA and ISPWB blocking clients */
	client_traffic_type_config_2 =
				mc_readl(MC_CLIENT_TRAFFIC_TYPE_CONFIG_2);
	mc_writel(client_traffic_type_config_2 | 0xc0,
			MC_CLIENT_TRAFFIC_TYPE_CONFIG_2);

	/* Set arbiter iso client types */
	mc_writel(0x1, MC_EMEM_ARB_ISOCHRONOUS_0);
	mc_writel(0x0, MC_EMEM_ARB_ISOCHRONOUS_1);
	mc_writel(0x0, MC_EMEM_ARB_ISOCHRONOUS_2);
	mc_writel(0x0, MC_EMEM_ARB_ISOCHRONOUS_3);
	mc_writel(0x80044000, MC_EMEM_ARB_ISOCHRONOUS_4);
	mc_writel(0x2, MC_EMEM_ARB_ISOCHRONOUS_5);
}
