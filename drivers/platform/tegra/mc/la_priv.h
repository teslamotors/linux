/*
 * arch/arm/mach-tegra/la_priv.h
 *
 * Copyright (C) 2012-2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _MACH_TEGRA_LA_PRIV_H_
#define _MACH_TEGRA_LA_PRIV_H_

#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0A_LOW_SHIFT		0
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0A_LOW_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0A_LOW_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0A_HIGH_SHIFT	16
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0A_HIGH_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0A_HIGH_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0AB_LOW_SHIFT	0
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0AB_LOW_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0AB_LOW_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0AB_HIGH_SHIFT	16
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0AB_HIGH_MASK	\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0AB_HIGH_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0B_LOW_SHIFT		0
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0B_LOW_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0B_LOW_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0B_HIGH_SHIFT	16
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0B_HIGH_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0B_HIGH_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0BB_LOW_SHIFT	0
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0BB_LOW_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0BB_LOW_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0BB_HIGH_SHIFT	16
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0BB_HIGH_MASK	\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0BB_HIGH_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0C_LOW_SHIFT		0
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0C_LOW_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0C_LOW_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0C_HIGH_SHIFT	16
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0C_HIGH_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0C_HIGH_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0CB_LOW_SHIFT	0
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0CB_LOW_MASK		\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0CB_LOW_SHIFT)
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0CB_HIGH_SHIFT	16
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0CB_HIGH_MASK	\
	(0xff << MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0CB_HIGH_SHIFT)

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
#define EMEM_PTSA_RATE_WIDTH		8
#define MAX_DDA_RATE			0xff
#else
#define EMEM_PTSA_RATE_WIDTH		12
#define MAX_DDA_RATE			0xfff
#endif

#define LA_FP_FACTOR			1000U
#define LA_REAL_TO_FP(val)		((val) * LA_FP_FACTOR)
#define LA_FP_TO_REAL(val)		((val) / LA_FP_FACTOR)
#define LA_ADDITIONAL_FP_FACTOR		10U
#define LA_FP_TO_FPA(val)		((val) * LA_ADDITIONAL_FP_FACTOR)
#define LA_FPA_TO_FP(val)		((val) / LA_ADDITIONAL_FP_FACTOR)
#define LA_FPA_TO_REAL(val)		((val) / LA_FP_FACTOR /		\
					 LA_ADDITIONAL_FP_FACTOR)
#define LA_REAL_TO_FPA(val)		((val) * LA_FP_FACTOR *	\
					 LA_ADDITIONAL_FP_FACTOR)

#define MASK(x) \
	((0xFFFFFFFFUL >> (31 - (1 ? x) + (0 ? x))) << (0 ? x))
#define SHIFT(x) \
	(0 ? x)
#define ID(id) \
	TEGRA_LA_##id

#define VALIDATE_ID(id, p) \
do { \
	if (id >= TEGRA_LA_MAX_ID || (p)->id_to_index[(id)] == 0xFFFF) { \
		WARN_ONCE(1, "%s: invalid Id=%d", __func__, (id)); \
		return -EINVAL; \
	} \
	BUG_ON((p)->la_info_array[(p)->id_to_index[(id)]].id != (id)); \
} while (0)

#define VALIDATE_BW(bw_in_mbps) \
do { \
	if (bw_in_mbps >= 4096) \
		return -EINVAL; \
} while (0)

#define VALIDATE_THRESHOLDS(tl, tm, th) \
do { \
	if ((tl) > 100 || (tm) > 100 || (th) > 100) \
		return -EINVAL; \
} while (0)

#define LAST_DISP_CLIENT_ID	ID(DISPLAYD)
#define NUM_DISP_CLIENTS	(LAST_DISP_CLIENT_ID - FIRST_DISP_CLIENT_ID + 1)
#define DISP_CLIENT_ID(id)	(ID(id) - FIRST_DISP_CLIENT_ID)

#define FIRST_CAMERA_CLIENT_ID	ID(VI_W)
#define LAST_CAMERA_CLIENT_ID	ID(ISP_WBB)
#define NUM_CAMERA_CLIENTS	(LAST_CAMERA_CLIENT_ID - \
				FIRST_CAMERA_CLIENT_ID + \
				1)
#define CAMERA_IDX(id)		(ID(id) - FIRST_CAMERA_CLIENT_ID)
#define CAMERA_LA_IDX(id)	(id - FIRST_CAMERA_CLIENT_ID)
#define AGG_CAMERA_ID(id)	TEGRA_LA_AGG_CAMERA_##id

#define MC_LA_MAX_VALUE					255U
#define MC_PTSA_MIN_DEFAULT_MASK			0x3f
#define MC_PTSA_MAX_DEFAULT_MASK			0x3f
#define MC_PTSA_RATE_DEFAULT_MASK			0xfff

#define LA_USEC_TO_NSEC_FACTOR				1000
#define LA_HZ_TO_MHZ_FACTOR				1000000

/*
 * Setup macro for la_client_info array.
 */
#define LA(f, e, a, r, i, ss, la, clk)			\
{							\
	.fifo_size_in_atoms = f,			\
	.expiration_in_ns = e,				\
	.reg_addr = MC_LATENCY_ALLOWANCE_ ## a,		\
	.mask = MASK(r),				\
	.shift = SHIFT(r),				\
	.id = ID(i),					\
	.name = __stringify(i),				\
	.scaling_supported = ss,			\
	.init_la = la,					\
	.la_ref_clk_mhz = clk				\
}

/*
 * Several macros for initializing and accessing/modifying the PTSA registers.
 */
#define MC_SET_INIT_PTSA(p, client, min, max)				\
	do {								\
		(p)->client ## _ptsa_min = (unsigned int)(min) &	\
			MC_PTSA_MIN_DEFAULT_MASK;			\
		(p)->client ## _ptsa_max = (unsigned int)(max) &	\
			MC_PTSA_MAX_DEFAULT_MASK;			\
	} while (0)

#define READ_PTSA_MIN_MAX(p, field, reg)				\
	do {								\
		(p)->field ##_ptsa_min = mc_readl(MC_ ## reg ## _PTSA_MIN); \
		(p)->field ##_ptsa_max = mc_readl(MC_ ## reg ## _PTSA_MAX); \
	} while (0)
#define READ_PTSA_MIN_MAX_RATE(p, field, reg)				\
	do {								\
		(p)->field ##_ptsa_min = mc_readl(MC_ ## reg ## _PTSA_MIN); \
		(p)->field ##_ptsa_max = mc_readl(MC_ ## reg ## _PTSA_MAX); \
		(p)->field ##_ptsa_rate = mc_readl(MC_ ## reg ## _PTSA_RATE); \
	} while (0)

#define WRITE_PTSA_MIN_MAX(p, field, reg)				\
	do {								\
		mc_writel((p)->field ##_ptsa_min, MC_ ## reg ## _PTSA_MIN); \
		mc_writel((p)->field ##_ptsa_max, MC_ ## reg ## _PTSA_MAX); \
	} while (0)
#define WRITE_PTSA_MIN_MAX_RATE(p, field, reg)				\
	do {								\
		mc_writel((p)->field ##_ptsa_min, MC_ ## reg ## _PTSA_MIN); \
		mc_writel((p)->field ##_ptsa_max, MC_ ## reg ## _PTSA_MAX); \
		mc_writel((p)->field ##_ptsa_rate, MC_ ## reg ## _PTSA_RATE); \
	} while (0)

/*
 * Some common functions for both t12x and t21x.
 */
struct la_client_info;
void program_la(struct la_client_info *ci, int la);
int la_suspend(void);
void la_resume(void);

/*
 * Note about fixed point arithmetic:
 * ----------------------------------
 * This file contains fixed point values and arithmetic due to the need to use
 * floating point values. All fixed point values have the "_fp" or "_FP" suffix
 * in their name. Macros used to convert between real and fixed point values are
 * listed below:
 *    - LA_FP_FACTOR
 *    - LA_REAL_TO_FP(val)
 *    - LA_FP_TO_REAL(val)
 *
 * Some scenarios require additional accuracy than what can be provided with
 * T12X_LA_FP_FACTOR. For these special cases we use the following additional
 * fixed point factor:- T12X_LA_ADDITIONAL_FP_FACTOR. Fixed point values which
 * use the addtional fixed point factor have a suffix of "_fpa" or "_FPA" in
 * their name. Macros used to convert between fpa values and other forms (i.e.
 * fp and real) are as follows:
 *    - LA_FP_TO_FPA(val)
 *    - LA_FPA_TO_FP(val)
 *    - LA_FPA_TO_REAL(val)
 *    - LA_REAL_TO_FPA(val)
 */

static inline unsigned int la_real_to_fp(unsigned int val)
{
	return val * LA_FP_FACTOR;
}

static inline unsigned int la_fp_to_real(unsigned int val)
{
	return val / LA_FP_FACTOR;
}

static inline bool is_display_client(enum tegra_la_id id)
{
	return ((id >= FIRST_DISP_CLIENT_ID) && (id <= LAST_DISP_CLIENT_ID));
}

static inline bool is_camera_client(enum tegra_la_id id)
{
	return ((id >= FIRST_CAMERA_CLIENT_ID) &&
		(id <= LAST_CAMERA_CLIENT_ID));
}

/*
 * TODO: this doesn't actually return an FP per se. It returns a value suitable
 * for placing in the DDA register fields; therefor this function needs
 * renaming.
 */
static inline unsigned int __fraction2dda_fp(unsigned int fraction_fpa,
					     unsigned int div,
					     unsigned int mask)
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
		if (dda != mask)
			dda++;		/* to round up dda value */
	}

	return min(dda, (unsigned int)MAX_DDA_RATE);
}

static inline unsigned int fraction2dda_fp(unsigned int fraction_fp,
					   unsigned int div,
					   unsigned int mask) {
	unsigned int fraction_fpa = LA_FP_TO_FPA(fraction_fp);

	return __fraction2dda_fp(fraction_fpa, div, mask);
}

#define ENABLE_LA_DEBUG		0
#define la_debug(fmt, ...)						\
	do {								\
		if (ENABLE_LA_DEBUG)					\
			pr_info("la_debug: " fmt, ##__VA_ARGS__);	\
	} while (0)

/* The following enum defines IDs for aggregated camera clients. In some cases
   we have to deal with groups of camera clients rather than individual
   clients. */
enum agg_camera_client_id {
	TEGRA_LA_AGG_CAMERA_VE = 0,
	TEGRA_LA_AGG_CAMERA_VE2,
	TEGRA_LA_AGG_CAMERA_ISP,
	TEGRA_LA_AGG_CAMERA_NUM_CLIENTS
};

enum la_client_type {
	TEGRA_LA_DYNAMIC_READ_CLIENT,
	TEGRA_LA_CONSTANT_READ_CLIENT,
	TEGRA_LA_DISPLAY_READ_CLIENT,
	TEGRA_LA_WRITE_CLIENT
};

enum la_traffic_type {
	TEGRA_LA_HISO,
	TEGRA_LA_SISO,
	TEGRA_LA_NISO
};

struct la_client_info {
	enum la_client_type client_type;
	unsigned int fifo_size_in_atoms;
	unsigned int expiration_in_ns;	/* worst case expiration value */
	unsigned int reg_addr;
	unsigned long mask;
	unsigned long shift;
	enum tegra_la_id id;
	char *name;
	bool scaling_supported;
	unsigned int min_scaling_ratio;
	unsigned int init_la;		/* initial la to set for client */
	unsigned int la_set;
	unsigned int la_ref_clk_mhz;
};

struct agg_camera_client_info {
	unsigned int bw_fp;
	unsigned int frac_fp;
	unsigned int ptsa_min;
	unsigned int ptsa_max;
	bool is_hiso;
};

struct la_scaling_info {
	unsigned int threshold_low;
	unsigned int threshold_mid;
	unsigned int threshold_high;
	int scaling_ref_count;
	int actual_la_to_set;
	int la_set;
};

struct la_scaling_reg_info {
	enum tegra_la_id id;
	void *tl_reg_addr;
	unsigned int tl_mask;
	unsigned int tl_shift;
	void *tm_reg_addr;
	unsigned int tm_mask;
	unsigned int tm_shift;
	void *th_reg_addr;
	unsigned int th_mask;
	unsigned int th_shift;
};

struct ptsa_info {
	unsigned int dis_ptsa_rate;
	unsigned int dis_ptsa_min;
	unsigned int dis_ptsa_max;
	enum la_traffic_type dis_traffic_type;
	unsigned int disb_ptsa_rate;
	unsigned int disb_ptsa_min;
	unsigned int disb_ptsa_max;
	enum la_traffic_type disb_traffic_type;
	unsigned int ve_ptsa_rate;
	unsigned int ve_ptsa_min;
	unsigned int ve_ptsa_max;
	enum la_traffic_type ve_traffic_type;
	unsigned int ve2_ptsa_rate;
	unsigned int ve2_ptsa_min;
	unsigned int ve2_ptsa_max;
	enum la_traffic_type ve2_traffic_type;
	unsigned int ring2_ptsa_rate;
	unsigned int ring2_ptsa_min;
	unsigned int ring2_ptsa_max;
	enum la_traffic_type ring2_traffic_type;
	unsigned int bbc_ptsa_rate;
	unsigned int bbc_ptsa_min;
	unsigned int bbc_ptsa_max;
	enum la_traffic_type bbc_traffic_type;
	unsigned int mpcorer_ptsa_rate;
	unsigned int mpcorer_ptsa_min;
	unsigned int mpcorer_ptsa_max;
	enum la_traffic_type mpcorer_traffic_type;
	unsigned int ftop_ptsa_min;
	unsigned int ftop_ptsa_max;
	unsigned int ftop_ptsa_rate;
	enum la_traffic_type ftop_traffic_type;
	unsigned int smmu_ptsa_rate;
	unsigned int smmu_ptsa_min;
	unsigned int smmu_ptsa_max;
	enum la_traffic_type smmu_traffic_type;
	unsigned int ring1_ptsa_rate;
	unsigned int ring1_ptsa_min;
	unsigned int ring1_ptsa_max;
	enum la_traffic_type ring1_traffic_type;

	unsigned int dis_extra_snap_level;
	unsigned int heg_extra_snap_level;
	unsigned int ptsa_grant_dec;
	unsigned int bbcll_earb_cfg;

	unsigned int isp_ptsa_rate;
	unsigned int isp_ptsa_min;
	unsigned int isp_ptsa_max;
	enum la_traffic_type isp_traffic_type;
	unsigned int a9avppc_ptsa_min;
	unsigned int a9avppc_ptsa_max;
	enum la_traffic_type a9avppc_traffic_type;
	unsigned int avp_ptsa_min;
	unsigned int avp_ptsa_max;
	enum la_traffic_type avp_traffic_type;
	unsigned int mse_ptsa_rate;
	unsigned int mse_ptsa_min;
	unsigned int mse_ptsa_max;
	enum la_traffic_type mse_traffic_type;
	unsigned int gk_ptsa_rate;
	unsigned int gk_ptsa_min;
	unsigned int gk_ptsa_max;
	enum la_traffic_type gk_traffic_type;
	unsigned int vicpc_ptsa_rate;
	unsigned int vicpc_ptsa_min;
	unsigned int vicpc_ptsa_max;
	enum la_traffic_type vicpc_traffic_type;
	unsigned int apb_ptsa_rate;
	unsigned int apb_ptsa_min;
	unsigned int apb_ptsa_max;
	enum la_traffic_type apb_traffic_type;
	unsigned int pcx_ptsa_rate;
	unsigned int pcx_ptsa_min;
	unsigned int pcx_ptsa_max;
	enum la_traffic_type pcx_traffic_type;
	unsigned int host_ptsa_rate;
	unsigned int host_ptsa_min;
	unsigned int host_ptsa_max;
	enum la_traffic_type host_traffic_type;
	unsigned int ahb_ptsa_min;
	unsigned int ahb_ptsa_max;
	enum la_traffic_type ahb_traffic_type;
	unsigned int sax_ptsa_rate;
	unsigned int sax_ptsa_min;
	unsigned int sax_ptsa_max;
	enum la_traffic_type sax_traffic_type;
	unsigned int aud_ptsa_rate;
	unsigned int aud_ptsa_min;
	unsigned int aud_ptsa_max;
	enum la_traffic_type aud_traffic_type;
	unsigned int sd_ptsa_rate;
	unsigned int sd_ptsa_min;
	unsigned int sd_ptsa_max;
	enum la_traffic_type sd_traffic_type;
	unsigned int usbx_ptsa_rate;
	unsigned int usbx_ptsa_min;
	unsigned int usbx_ptsa_max;
	enum la_traffic_type usbx_traffic_type;
	unsigned int usbd_ptsa_rate;
	unsigned int usbd_ptsa_min;
	unsigned int usbd_ptsa_max;
	enum la_traffic_type usbd_traffic_type;

	/* Tegra12x */
	unsigned int r0_dis_ptsa_min;
	unsigned int r0_dis_ptsa_max;
	enum la_traffic_type r0_dis_traffic_type;
	unsigned int r0_disb_ptsa_min;
	unsigned int r0_disb_ptsa_max;
	enum la_traffic_type r0_disb_traffic_type;
	unsigned int vd_ptsa_min;
	unsigned int vd_ptsa_max;
	enum la_traffic_type vd_traffic_type;

	/* Tegra18x */
	unsigned int aondmapc_ptsa_rate;
	unsigned int aondmapc_ptsa_min;
	unsigned int aondmapc_ptsa_max;
	enum la_traffic_type aondmapc_traffic_type;
	unsigned int aonpc_ptsa_rate;
	unsigned int aonpc_ptsa_min;
	unsigned int aonpc_ptsa_max;
	enum la_traffic_type aonpc_traffic_type;
	unsigned int apedmapc_ptsa_rate;
	unsigned int apedmapc_ptsa_min;
	unsigned int apedmapc_ptsa_max;
	enum la_traffic_type apedmapc_traffic_type;
	unsigned int bpmpdmapc_ptsa_rate;
	unsigned int bpmpdmapc_ptsa_min;
	unsigned int bpmpdmapc_ptsa_max;
	enum la_traffic_type bpmpdmapc_traffic_type;
	unsigned int bpmppc_ptsa_rate;
	unsigned int bpmppc_ptsa_min;
	unsigned int bpmppc_ptsa_max;
	enum la_traffic_type bpmppc_traffic_type;
	unsigned int dfd_ptsa_rate;
	unsigned int dfd_ptsa_min;
	unsigned int dfd_ptsa_max;
	enum la_traffic_type dfd_traffic_type;
	unsigned int eqospc_ptsa_rate;
	unsigned int eqospc_ptsa_min;
	unsigned int eqospc_ptsa_max;
	enum la_traffic_type eqospc_traffic_type;
	unsigned int hdapc_ptsa_rate;
	unsigned int hdapc_ptsa_min;
	unsigned int hdapc_ptsa_max;
	enum la_traffic_type hdapc_traffic_type;
	unsigned int mll_mpcorer_ptsa_rate;
	unsigned int mll_mpcorer_ptsa_min;
	unsigned int mll_mpcorer_ptsa_max;
	enum la_traffic_type mll_mpcorer_traffic_type;
	unsigned int mse2_ptsa_rate;
	unsigned int mse2_ptsa_min;
	unsigned int mse2_ptsa_max;
	enum la_traffic_type mse2_traffic_type;
	unsigned int nic_ptsa_rate;
	unsigned int nic_ptsa_min;
	unsigned int nic_ptsa_max;
	enum la_traffic_type nic_traffic_type;
	unsigned int nvd_ptsa_rate;
	unsigned int nvd_ptsa_min;
	unsigned int nvd_ptsa_max;
	enum la_traffic_type nvd_traffic_type;
	unsigned int nvd3_ptsa_rate;
	unsigned int nvd3_ptsa_min;
	unsigned int nvd3_ptsa_max;
	enum la_traffic_type nvd3_traffic_type;
	unsigned int roc_dma_r_ptsa_rate;
	unsigned int roc_dma_r_ptsa_min;
	unsigned int roc_dma_r_ptsa_max;
	enum la_traffic_type roc_dma_r_traffic_type;
	unsigned int ring1_rd_b_ptsa_rate;
	unsigned int ring1_rd_b_ptsa_min;
	unsigned int ring1_rd_b_ptsa_max;
	enum la_traffic_type ring1_rd_b_traffic_type;
	unsigned int ring1_rd_nb_ptsa_rate;
	unsigned int ring1_rd_nb_ptsa_min;
	unsigned int ring1_rd_nb_ptsa_max;
	enum la_traffic_type ring1_rd_nb_traffic_type;
	unsigned int ring1_wr_b_ptsa_rate;
	unsigned int ring1_wr_b_ptsa_min;
	unsigned int ring1_wr_b_ptsa_max;
	enum la_traffic_type ring1_wr_b_traffic_type;
	unsigned int ring1_wr_nb_ptsa_rate;
	unsigned int ring1_wr_nb_ptsa_min;
	unsigned int ring1_wr_nb_ptsa_max;
	enum la_traffic_type ring1_wr_nb_traffic_type;
	unsigned int scedmapc_ptsa_rate;
	unsigned int scedmapc_ptsa_min;
	unsigned int scedmapc_ptsa_max;
	enum la_traffic_type scedmapc_traffic_type;
	unsigned int scepc_ptsa_rate;
	unsigned int scepc_ptsa_min;
	unsigned int scepc_ptsa_max;
	enum la_traffic_type scepc_traffic_type;
	unsigned int sdm_ptsa_rate;
	unsigned int sdm_ptsa_min;
	unsigned int sdm_ptsa_max;
	enum la_traffic_type sdm_traffic_type;
	unsigned int sdm1_ptsa_rate;
	unsigned int sdm1_ptsa_min;
	unsigned int sdm1_ptsa_max;
	enum la_traffic_type sdm1_traffic_type;
	unsigned int ufshcpc_ptsa_rate;
	unsigned int ufshcpc_ptsa_min;
	unsigned int ufshcpc_ptsa_max;
	enum la_traffic_type ufshcpc_traffic_type;
	unsigned int vicpc3_ptsa_rate;
	unsigned int vicpc3_ptsa_min;
	unsigned int vicpc3_ptsa_max;
	enum la_traffic_type vicpc3_traffic_type;

	/* Tegra21x */
	unsigned int jpg_ptsa_rate;
	unsigned int jpg_ptsa_min;
	unsigned int jpg_ptsa_max;
	enum la_traffic_type jpg_traffic_type;
	unsigned int gk2_ptsa_rate;
	unsigned int gk2_ptsa_min;
	unsigned int gk2_ptsa_max;
	enum la_traffic_type gk2_traffic_type;
};


struct la_chip_specific {
	int ns_per_tick;
	int atom_size;
	int la_max_value;
	spinlock_t lock;
	int la_info_array_size;
	struct la_client_info *la_info_array;
	unsigned short id_to_index[ID(MAX_ID) + 1];
	unsigned int disp_bw_array[NUM_DISP_CLIENTS];
	struct disp_client disp_clients[NUM_DISP_CLIENTS];
	unsigned int bbc_bw_array[ID(BBCLLR) - ID(BBCR) + 1];
	unsigned int camera_bw_array[NUM_CAMERA_CLIENTS];
	struct agg_camera_client_info
			agg_camera_array[TEGRA_LA_AGG_CAMERA_NUM_CLIENTS];
	struct la_scaling_info scaling_info[ID(MAX_ID)];
	int la_scaling_enable_count;
	struct dentry *latency_debug_dir;
	struct ptsa_info ptsa_info;
	bool disable_la;
	bool disable_ptsa;
	struct la_to_dc_params la_params;
	bool disable_disp_ptsa;
	bool disable_bbc_ptsa;

	void (*init_ptsa)(void);
	void (*update_display_ptsa_rate)(unsigned int *disp_bw_array);
	int (*update_camera_ptsa_rate)(enum tegra_la_id id,
					unsigned int bw_mbps,
					int is_hiso);
	int (*set_disp_la)(enum tegra_la_id id,
				unsigned long emc_freq_hz,
				unsigned int bw_mbps,
				struct dc_to_la_params disp_params);
	int (*check_disp_la)(enum tegra_la_id id,
				unsigned long emc_freq_hz,
				unsigned int bw_mbps,
				struct dc_to_la_params disp_params);
	int (*set_init_la)(enum tegra_la_id id, unsigned int bw_mbps);
	int (*set_dynamic_la)(enum tegra_la_id id, unsigned int bw_mbps);
	int (*enable_la_scaling)(enum tegra_la_id id,
				unsigned int threshold_low,
				unsigned int threshold_mid,
				unsigned int threshold_high);
	void (*disable_la_scaling)(enum tegra_la_id id);
	void (*save_ptsa)(void);
	void (*program_ptsa)(void);
	int (*suspend)(void);
	void (*resume)(void);
};


void tegra_la_get_t3_specific(struct la_chip_specific *cs);
void tegra_la_get_t14x_specific(struct la_chip_specific *cs);
void tegra_la_get_t11x_specific(struct la_chip_specific *cs);
void tegra_la_get_t12x_specific(struct la_chip_specific *cs);
void tegra_la_get_t18x_specific(struct la_chip_specific *cs);
void tegra_la_get_t21x_specific(struct la_chip_specific *cs);

#endif /* _MACH_TEGRA_LA_PRIV_H_ */
