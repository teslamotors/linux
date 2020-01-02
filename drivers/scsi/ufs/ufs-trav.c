#include <linux/types.h>
#include "ufs-trav.h"

#ifndef _UFS_CAL_
#define _UFS_CAL_

/* UFSHCI */
#define UIC_ARG_MIB_SEL(attr, sel)	((((attr) & 0xFFFF) << 16) |\
					 ((sel) & 0xFFFF))
#define UIC_ARG_MIB(attr)		UIC_ARG_MIB_SEL(attr, 0)

/* Unipro.h */
#define IS_PWR_MODE_HS(m)        (((m) == FAST_MODE) || ((m) == FASTAUTO_MODE))
#define IS_PWR_MODE_PWM(m)       (((m) == SLOW_MODE) || ((m) == SLOWAUTO_MODE))

enum {
	PA_HS_MODE_A	= 1,
	PA_HS_MODE_B	= 2,
};

enum {
	FAST_MODE	= 1,
	SLOW_MODE	= 2,
	FASTAUTO_MODE	= 4,
	SLOWAUTO_MODE	= 5,
	UNCHANGED	= 7,
};

/* User defined */
#define UNIPRO_MCLK_PERIOD(p) (1000000000L / p->mclk_rate)

#define NUM_OF_UFS_HOST	2

enum {
	PHY_CFG_NONE = 0,
	PHY_PCS_COMN,
	PHY_PCS_RXTX,
	UNIPRO_STD_MIB,
	UNIPRO_DBG_MIB,
	UNIPRO_DBG_APB,

	PHY_PCS_RX,
	PHY_PCS_TX,
	PHY_PCS_RX_PRD,
	PHY_PCS_TX_PRD,
	UNIPRO_DBG_PRD,
	PHY_PMA_COMN,
	PHY_PMA_TRSV,
};

enum {
	TX_LANE_0 = 0,
	TX_LANE_1 = 1,
	TX_LANE_2 = 2,
	TX_LANE_3 = 3,
	RX_LANE_0 = 4,
	RX_LANE_1 = 5,
	RX_LANE_2 = 6,
	RX_LANE_3 = 7,
};

enum {
	__PMD_PWM_G1_L1,
	__PMD_PWM_G1_L2,
	__PMD_PWM_G2_L1,
	__PMD_PWM_G2_L2,
	__PMD_PWM_G3_L1,
	__PMD_PWM_G3_L2,
	__PMD_PWM_G4_L1,
	__PMD_PWM_G4_L2,
	__PMD_PWM_G5_L1,
	__PMD_PWM_G5_L2,
	__PMD_HS_G1_L1,
	__PMD_HS_G1_L2,
	__PMD_HS_G2_L1,
	__PMD_HS_G2_L2,
	__PMD_HS_G3_L1,
	__PMD_HS_G3_L2,
};

#define PMD_PWM_G1_L1	(1U << __PMD_PWM_G1_L1)
#define PMD_PWM_G1_L2	(1U << __PMD_PWM_G1_L2)
#define PMD_PWM_G2_L1	(1U << __PMD_PWM_G2_L1)
#define PMD_PWM_G2_L2	(1U << __PMD_PWM_G2_L2)
#define PMD_PWM_G3_L1	(1U << __PMD_PWM_G3_L1)
#define PMD_PWM_G3_L2	(1U << __PMD_PWM_G3_L2)
#define PMD_PWM_G4_L1	(1U << __PMD_PWM_G4_L1)
#define PMD_PWM_G4_L2	(1U << __PMD_PWM_G4_L2)
#define PMD_PWM_G5_L1	(1U << __PMD_PWM_G5_L1)
#define PMD_PWM_G5_L2	(1U << __PMD_PWM_G5_L2)
#define PMD_HS_G1_L1	(1U << __PMD_HS_G1_L1)
#define PMD_HS_G1_L2	(1U << __PMD_HS_G1_L2)
#define PMD_HS_G2_L1	(1U << __PMD_HS_G2_L1)
#define PMD_HS_G2_L2	(1U << __PMD_HS_G2_L2)
#define PMD_HS_G3_L1	(1U << __PMD_HS_G3_L1)
#define PMD_HS_G3_L2	(1U << __PMD_HS_G3_L2)

#define PMD_ALL		(PMD_HS_G3_L2 - 1)
#define PMD_PWM		(PMD_PWM_G4_L2 - 1)
#define PMD_HS		(PMD_ALL ^ PMD_PWM)

struct ufs_cal_phy_cfg {
	u32 addr;
	u32 val;
	u32 flg;
	u32 lyr;
	u8 board;
};

#define for_each_phy_cfg(cfg) \
	for (; (cfg)->flg != PHY_CFG_NONE; (cfg)++)

#endif /*_UFS_CAL_ */

static struct ufs_cal_param *ufs_cal[NUM_OF_UFS_HOST];

static const struct ufs_cal_phy_cfg init_cfg[] = {
	{0x9514, 0x00, PMD_ALL, UNIPRO_DBG_PRD, 0},
	{0x201, 0x12, PMD_ALL, PHY_PCS_COMN},
	{0x200, 0x40, PMD_ALL, PHY_PCS_COMN, 0},
	{0x12, 0x06, PMD_ALL, PHY_PCS_RX_PRD, 0},
	{0xAA, 0x06, PMD_ALL, PHY_PCS_TX_PRD, 0},
	{0x8F, 0x3F, PMD_ALL, PHY_PCS_TX, 0},
	{0x5C, 0x38, PMD_ALL, PHY_PCS_RX, 0},
	{0x0F, 0x0, PMD_ALL, PHY_PCS_RX, 0},
	{0x65, 0x01, PMD_ALL, PHY_PCS_RX, 0},
	{0x69, 0x01, PMD_ALL, PHY_PCS_RX, 0},
	{0x21, 0x00, PMD_ALL, PHY_PCS_RX, 0},
	{0x22, 0x00, PMD_ALL, PHY_PCS_RX, 0},
	{0x200, 0x0, PMD_ALL, PHY_PCS_COMN, 0},
	{0x9536, 0x4E20, PMD_ALL, UNIPRO_DBG_MIB, 0},
	{0x9564, 0x2e820183, PMD_ALL, UNIPRO_DBG_MIB, 0},
	{0x155E, 0x0, PMD_ALL, UNIPRO_STD_MIB, 0},
	{0x3000, 0x0, PMD_ALL, UNIPRO_STD_MIB, 0},
	{0x3001, 0x1, PMD_ALL, UNIPRO_STD_MIB, 0},
	{0x4021, 0x1, PMD_ALL, UNIPRO_STD_MIB, 0},
	{0x4020, 0x1, PMD_ALL, UNIPRO_STD_MIB, 0},
	//PMA Init settings starts
	{0x3C, 0xFA, PMD_ALL, PHY_PMA_COMN},
	{0x40, 0x96, PMD_ALL, PHY_PMA_COMN},
	{0x44, 0x14, PMD_ALL, PHY_PMA_COMN},
	{0x5C, 0x94, PMD_ALL, PHY_PMA_COMN},
	{0xC4, 0xC4, PMD_ALL, PHY_PMA_TRSV},
	{0xC8, 0xC4, PMD_ALL, PHY_PMA_TRSV},
	{0xCC, 0x80, PMD_ALL, PHY_PMA_TRSV},
	{0xD0, 0x25, PMD_ALL, PHY_PMA_TRSV},
	{0xD4, 0x40, PMD_ALL, PHY_PMA_TRSV},
	{0xD8, 0x0, PMD_ALL, PHY_PMA_TRSV},
	{0xDC, 0x3, PMD_ALL, PHY_PMA_TRSV},
	{0x108, 0x38, PMD_ALL, PHY_PMA_TRSV},
	{0x10C, 0xa4, PMD_ALL, PHY_PMA_TRSV},
	{0x120, 0x69, PMD_ALL, PHY_PMA_TRSV},
	{0x124, 0x3, PMD_ALL, PHY_PMA_TRSV},
	{0x128, 0x10, PMD_ALL, PHY_PMA_TRSV},
	{0x130, 0x1b, PMD_ALL, PHY_PMA_TRSV},
	{0x134, 0x1, PMD_ALL, PHY_PMA_TRSV},
	{0x14c, 0x0, PMD_ALL, PHY_PMA_TRSV},
	{0x160, 0x1, PMD_ALL, PHY_PMA_TRSV},
	{0x170, 0x14, PMD_ALL, PHY_PMA_TRSV},
	{0x1b4, 0x0, PMD_ALL, PHY_PMA_TRSV},
	{0x1bc, 0x0, PMD_ALL, PHY_PMA_TRSV},
	// Lane 2 settings
	{0x1C4, 0xC4, PMD_ALL, PHY_PMA_TRSV},
	{0x1C8, 0xC4, PMD_ALL, PHY_PMA_TRSV},
	{0x1CC, 0x80, PMD_ALL, PHY_PMA_TRSV},
	{0x1D0, 0x25, PMD_ALL, PHY_PMA_TRSV},
	{0x1D4, 0x40, PMD_ALL, PHY_PMA_TRSV},
	{0x1D8, 0x0, PMD_ALL, PHY_PMA_TRSV},
	{0x1DC, 0x3, PMD_ALL, PHY_PMA_TRSV},
	{0x208, 0x38, PMD_ALL, PHY_PMA_TRSV},
	{0x20C, 0xa4, PMD_ALL, PHY_PMA_TRSV},
	{0x220, 0x69, PMD_ALL, PHY_PMA_TRSV},
	{0x224, 0x3, PMD_ALL, PHY_PMA_TRSV},
	{0x228, 0x10, PMD_ALL, PHY_PMA_TRSV},
	{0x230, 0x1b, PMD_ALL, PHY_PMA_TRSV},
	{0x234, 0x1, PMD_ALL, PHY_PMA_TRSV},
	{0x24c, 0x0, PMD_ALL, PHY_PMA_TRSV},
	{0x260, 0x1, PMD_ALL, PHY_PMA_TRSV},
	{0x270, 0x14, PMD_ALL, PHY_PMA_TRSV},
	{0x2b4, 0x0, PMD_ALL, PHY_PMA_TRSV},
	{0x2bc, 0x0, PMD_ALL, PHY_PMA_TRSV},
	//PMA Init settings end
	{},
};

static struct ufs_cal_phy_cfg post_init_cfg[] = {
	{0x9529, 0x01, PMD_ALL, UNIPRO_DBG_MIB, 0},
	{0x15A4, 0xFA, PMD_ALL, UNIPRO_STD_MIB, 0},
	{0x9529, 0x00, PMD_ALL, UNIPRO_DBG_MIB, 0},
	{0x200, 0x40, PMD_ALL, PHY_PCS_COMN, 0},
	{0x35, 0x05, PMD_ALL, PHY_PCS_RX, 0},
	{0x73, 0x01, PMD_ALL, PHY_PCS_RX, 0},
	{0x41, 0x02, PMD_ALL, PHY_PCS_RX, 0},
	{0x42, 0xAC, PMD_ALL, PHY_PCS_RX, 0},
	{0x200, 0x00, PMD_ALL, PHY_PCS_COMN, 0},
	{},
};

static struct ufs_cal_phy_cfg calib_of_pwm[] = {
	{0x1569, 0x0, PMD_PWM, UNIPRO_STD_MIB, 0},
	{0x1584, 0x0, PMD_PWM, UNIPRO_STD_MIB, 0},

	{0x2041, 8064, PMD_PWM, UNIPRO_STD_MIB, 0},
	{0x2042, 28224, PMD_PWM, UNIPRO_STD_MIB, 0},
	{0x2043, 20160, PMD_PWM, UNIPRO_STD_MIB, 0},
	{0x15B0, 12000, PMD_PWM, UNIPRO_STD_MIB, 0},
	{0x15B1, 32000, PMD_PWM, UNIPRO_STD_MIB, 0},
	{0x15B2, 16000, PMD_PWM, UNIPRO_STD_MIB, 0},

	{0x7888, 8064, PMD_PWM, UNIPRO_DBG_APB, 0},
	{0x788C, 28224, PMD_PWM, UNIPRO_DBG_APB, 0},
	{0x7890, 20160, PMD_PWM, UNIPRO_DBG_APB, 0},
	{0x78B8, 12000, PMD_PWM, UNIPRO_DBG_APB, 0},
	{0x78BC, 32000, PMD_PWM, UNIPRO_DBG_APB, 0},
	{0x78C0, 16000, PMD_PWM, UNIPRO_DBG_APB, 0},
	{},
};

static struct ufs_cal_phy_cfg post_calib_of_pwm[] = {
	/*TODO: Place holder for now, will be populated for board */
	{},
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_a[] = {
	{0x1569, 0x1, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x1584, 0x1, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x2041, 8064, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x2042, 28224, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x2043, 20160, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x15B0, 12000, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x15B1, 32000, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x15B2, 16000, PMD_HS, UNIPRO_STD_MIB, 0},

	{0x7888, 8064, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, 0},
	{},
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_a[] = {
	/*TODO: Place holder for now, will be populated for board */
	{},
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_b[] = {
	{0x1569, 0x1, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x1584, 0x1, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x2041, 8064, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x2042, 28224, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x2043, 20160, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x15B0, 12000, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x15B1, 32000, PMD_HS, UNIPRO_STD_MIB, 0},
	{0x15B2, 16000, PMD_HS, UNIPRO_STD_MIB, 0},

	{0x7888, 8064, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, 0},
	{0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, 0},
	{},
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_b[] = {
	/*TODO: Place holder for now, will be populated for board */
	{},
};

static struct ufs_cal_phy_cfg lane1_sq_off[] = {
	/*TODO: Place holder for now, will be populated for board */
	{},
};

static struct ufs_cal_phy_cfg post_h8_enter[] = {
	/*TODO: Place holder for now, will be populated for board */
	{},
};

static struct ufs_cal_phy_cfg pre_h8_exit[] = {
	/*TODO: Place holder for now, will be populated for board */
	{},
};

static inline ufs_cal_errno __match_board_by_cfg(u8 board, u8 cfg_board)
{
	ufs_cal_errno match = UFS_CAL_ERROR;

	if (cfg_board == 0)
		match = UFS_CAL_NO_ERROR;
	else if (board == cfg_board)
		match = UFS_CAL_NO_ERROR;

	return match;
}

static inline ufs_cal_errno __match_mode_by_cfg(struct uic_pwr_mode *pmd,
								int mode)
{
	ufs_cal_errno match = UFS_CAL_ERROR;
	u8 _m, _l, _g;

	_m = pmd->mode;
	_g = pmd->gear;
	_l = pmd->lane;

	if (mode == PMD_ALL)
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && mode == PMD_HS)
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && mode == PMD_PWM)
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 1 && _l == 1
		&& (mode & (PMD_HS_G1_L1|PMD_HS_G1_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 1 && _l == 2
		&& (mode & (PMD_HS_G1_L1|PMD_HS_G1_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 2 && _l == 1
		&& (mode & (PMD_HS_G2_L1|PMD_HS_G2_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 2 && _l == 2
		&& (mode & (PMD_HS_G2_L1|PMD_HS_G2_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 3 && _l == 1
		&& (mode & (PMD_HS_G3_L1|PMD_HS_G3_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_HS(_m) && _g == 3 && _l == 2
		&& (mode & (PMD_HS_G3_L1|PMD_HS_G3_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 1 && _l == 1
		&& (mode & (PMD_PWM_G1_L1|PMD_PWM_G1_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 1 && _l == 2
		&& (mode & (PMD_PWM_G1_L1|PMD_PWM_G1_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 2 && _l == 1
		&& (mode & (PMD_PWM_G2_L1|PMD_PWM_G2_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 2 && _l == 2
		&& (mode & (PMD_PWM_G2_L1|PMD_PWM_G2_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 3 && _l == 1
		&& (mode & (PMD_PWM_G3_L1|PMD_PWM_G3_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 3 && _l == 2
		&& (mode & (PMD_PWM_G3_L1|PMD_PWM_G3_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 4 && _l == 1
		&& (mode & (PMD_PWM_G4_L1|PMD_PWM_G4_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 4 && _l == 2
		&& (mode & (PMD_PWM_G4_L1|PMD_PWM_G4_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 5 && _l == 1
		&& (mode & (PMD_PWM_G5_L1|PMD_PWM_G5_L2)))
		match = UFS_CAL_NO_ERROR;
	else if (IS_PWR_MODE_PWM(_m) && _g == 5 && _l == 2
		&& (mode & (PMD_PWM_G5_L1|PMD_PWM_G5_L2)))
		match = UFS_CAL_NO_ERROR;

	return match;
}

static ufs_cal_errno ufs_cal_config_uic(struct ufs_cal_param *p,
				  const struct ufs_cal_phy_cfg *cfg,
				  struct uic_pwr_mode *pmd)
{
	void *hba = p->host;
	u8 i = 0;

	if (!cfg)
		return UFS_CAL_INV_ARG;

	for_each_phy_cfg(cfg) {
		for (i = 0; i < p->target_lane; i++) {
			if (p->board && UFS_CAL_ERROR ==
					__match_board_by_cfg(p->board, cfg->board))
				continue;
			if (pmd && UFS_CAL_ERROR ==
					__match_mode_by_cfg(pmd, cfg->flg))
				continue;

			switch (cfg->lyr) {
			case PHY_PCS_COMN:
			case UNIPRO_STD_MIB:
			case UNIPRO_DBG_MIB:
				if (i == 0)
					ufs_lld_dme_set(hba, UIC_ARG_MIB(cfg->addr),
						cfg->val);
				break;
			case PHY_PCS_RXTX:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr, i),
						cfg->val);
				break;
			case UNIPRO_DBG_PRD:
				if (i == 0)
					ufs_lld_dme_set(hba, UIC_ARG_MIB(cfg->addr),
						UNIPRO_MCLK_PERIOD(p));
				break;
			case PHY_PCS_RX:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr,
					RX_LANE_0+i), cfg->val);
				break;
			case PHY_PCS_TX:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr,
					TX_LANE_0+i), cfg->val);
				break;
			case PHY_PCS_RX_PRD:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr,
					RX_LANE_0+i), UNIPRO_MCLK_PERIOD(p));
				break;

			case PHY_PCS_TX_PRD:
				ufs_lld_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr,
					TX_LANE_0+i), UNIPRO_MCLK_PERIOD(p));
				break;

			case UNIPRO_DBG_APB:
				if (i == 0)
					ufs_lld_unipro_write(hba, cfg->val, cfg->addr);
				break;

			case PHY_PMA_COMN:
				if (i == 0)
					ufs_lld_pma_write(hba, cfg->val, cfg->addr);
				break;

			case PHY_PMA_TRSV:
				if (i == 0)
					ufs_lld_pma_write(hba, cfg->val, cfg->addr);
				break;

			default:
				break;
			}
		}
	}

	return UFS_CAL_NO_ERROR;
}

/*
 * This is a recommendation from Samsung UFS device vendor.
 *
 * Activate time: host < device
 * Hibern time: host > device
 */
static void ufs_cal_calib_hibern8_values(void *hba)
{
	u32 hw_cap_min_tactivate;
	u32 peer_rx_min_actv_time_cap;
	u32 max_rx_hibern8_time_cap;

	ufs_lld_dme_get(hba, UIC_ARG_MIB_SEL(0x8F, RX_LANE_0),
			&hw_cap_min_tactivate);	/* HW Capability of MIN_TACTIVATE */

	ufs_lld_dme_get(hba, UIC_ARG_MIB(0x15A8),
			&peer_rx_min_actv_time_cap);	/* PA_TActivate */
	ufs_lld_dme_get(hba, UIC_ARG_MIB(0x15A7),
			&max_rx_hibern8_time_cap);	/* PA_Hibern8Time */

	if (peer_rx_min_actv_time_cap >= hw_cap_min_tactivate)
		ufs_lld_dme_peer_set(hba, UIC_ARG_MIB(0x15A8),
				peer_rx_min_actv_time_cap + 1);
	ufs_lld_dme_set(hba, UIC_ARG_MIB(0x15A7), max_rx_hibern8_time_cap + 1);
}

ufs_cal_errno ufs_cal_post_h8_enter(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ret = ufs_cal_config_uic(p, post_h8_enter, p->pmd);

	return ret;
}

ufs_cal_errno ufs_cal_pre_h8_exit(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ret = ufs_cal_config_uic(p, pre_h8_exit, p->pmd);

	return ret;
}

/*
 * This currently uses only SLOW_MODE and FAST_MODE.
 * If you want others, you should modify this function.
 */
ufs_cal_errno ufs_cal_pre_pmc(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_phy_cfg *cfg;
	p->pmd->mode = FAST_MODE;

	if ((p->pmd->mode == SLOW_MODE) || (p->pmd->mode == SLOWAUTO_MODE))
		cfg = calib_of_pwm;
	else if (p->pmd->hs_series == PA_HS_MODE_B)
		cfg = calib_of_hs_rate_b;
	else if (p->pmd->hs_series == PA_HS_MODE_A)
		cfg = calib_of_hs_rate_a;
	else
		return UFS_CAL_INV_ARG;

	/*
	 * If a number of target lanes is 1 and a host's
	 * a number of available lanes is 2,
	 * you should turn off phy power of lane #1.
	 *
	 * This must be modified when a number of avaiable lanes
	 * would grow in the future.
	 */
	if ((p->available_lane == 2) && (p->target_lane == 1))
		ret = ufs_cal_config_uic(p, lane1_sq_off, NULL);

	ret = ufs_cal_config_uic(p, cfg, p->pmd);

	return ret;
}

/*
 * This currently uses only SLOW_MODE and FAST_MODE.
 * If you want others, you should modify this function.
 */
ufs_cal_errno ufs_cal_post_pmc(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;
	struct ufs_cal_phy_cfg *cfg;
	p->pmd->mode = FAST_MODE;

	if ((p->pmd->mode == SLOWAUTO_MODE) || (p->pmd->mode == SLOW_MODE))
		cfg = post_calib_of_pwm;
	else if (p->pmd->hs_series == PA_HS_MODE_B)
		cfg = post_calib_of_hs_rate_b;
	else if (p->pmd->hs_series == PA_HS_MODE_A)
		cfg = post_calib_of_hs_rate_a;
	else
		return UFS_CAL_INV_ARG;

	ret = ufs_cal_config_uic(p, cfg, p->pmd);

	return ret;
}

ufs_cal_errno ufs_cal_post_link(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ufs_cal_calib_hibern8_values(p->host);

	ret = ufs_cal_config_uic(p, post_init_cfg, NULL);

	return ret;
}

ufs_cal_errno ufs_cal_pre_link(struct ufs_cal_param *p)
{
	ufs_cal_errno ret = UFS_CAL_NO_ERROR;

	ret = ufs_cal_config_uic(p, init_cfg, NULL);

	return ret;
}

ufs_cal_errno ufs_cal_init(struct ufs_cal_param *p, int idx)
{
	/*
	 * Return if innput index is greater than
	 * the maximum that cal supports
	 */
	if (idx >= NUM_OF_UFS_HOST)
		return UFS_CAL_INV_ARG;

	ufs_cal[idx] = p;

	return UFS_CAL_NO_ERROR;
}
