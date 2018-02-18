/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#include "m_ttcan.h"

/* TT Reference Message Config */
#define TTRMC_REF_PAYLOAD	(0x1 << 31)
#define TTRMC_EXT_ID		(0x1 << 30)
#define TTRMC_STD_ID_SHIFT	0x12

/* TT Operation Config */

#define TTOCF_MODE_0	0x3
#define TTOCF_MODE_1	0x2
#define TTCOF_MODE_2	0x1
#define TTOCF_EVT_TRIG	0x0

#define TTOCF_GAP		0x4
#define TTOCF_POTENTIAL_TM	0x8
#define TTOCF_LDSDL_SHIFT	0x5
#define TTOCF_IRTO_SHIFT	0x8

#define TTOCF_EXT_CLK_SYNC	0x8000
#define TTOCF_AWL_SHIFT		0x10
#define TTOCF_GBL_TIME_FLT	(0x1 << 24)
#define TTOCF_CLK_CALIB		(0x1 << 25)
#define TTOCF_EVT_POLARITY	(0x1 << 26)

enum time_master_info {
	TM_MASTER = 0,
	TM_POTENTIAL,
	TM_SLAVE,
};

inline void ttcan_clean_tt_intr(struct ttcan_controller *ttcan)
{
	ttcan_write32(ttcan, ADR_MTTCAN_TTIR, 0xFFFFFFFF);
}

void ttcan_set_ref_mesg(struct ttcan_controller *ttcan,
	u32 id, u32 rmps, u32 xtd)
{
	u32 rid;
	u32 ttrmc = (xtd << MTT_TTRMC_XTD_SHIFT) &
		MTT_TTRMC_XTD_MASK;
	ttrmc |= (rmps << MTT_TTRMC_RMPS_SHIFT) &
		MTT_TTRMC_RMPS_MASK;
	if (xtd)
		rid = (id & 0x1FFFFFFF);
	else
		rid = ((id & 0x7FF) << TTRMC_STD_ID_SHIFT);
	ttrmc |= (rid << MTT_TTRMC_RID_SHIFT) & MTT_TTRMC_RID_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_TTRMC, ttrmc);
}

void ttcan_set_tt_config(struct ttcan_controller *ttcan, u32 evtp,
	u32 ecc, u32 egtf, u32 awl, u32 eecs,
	u32 irto, u32 ldsdl, u32 tm, u32 gen, u32 om)
{
	u32 ttocf = 0;

	ttocf = (evtp << MTT_TTOCF_EVTP_SHIFT) & MTT_TTOCF_EVTP_MASK;
	ttocf |= (ecc << MTT_TTOCF_ECC_SHIFT) & MTT_TTOCF_ECC_MASK;
	ttocf |= (egtf << MTT_TTOCF_EGTF_SHIFT) & MTT_TTOCF_EGTF_MASK;
	ttocf |= (awl << MTT_TTOCF_AWL_SHIFT) & MTT_TTOCF_AWL_MASK;
	ttocf |= (eecs << MTT_TTOCF_EECS_SHIFT) & MTT_TTOCF_EECS_MASK;
	ttocf |= (irto << MTT_TTOCF_IRTO_SHIFT) & MTT_TTOCF_IRTO_MASK;
	ttocf |= (ldsdl << MTT_TTOCF_LDSDL_SHIFT) &
		MTT_TTOCF_LDSDL_MASK;
	ttocf |= (tm << MTT_TTOCF_TM_SHIFT) & MTT_TTOCF_TM_MASK;
	ttocf |= (gen << MTT_TTOCF_GEN_SHIFT) & MTT_TTOCF_GEN_MASK;
	ttocf |= (om << MTT_TTOCF_OM_SHIFT) & MTT_TTOCF_OM_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_TTOCF, ttocf);
}

inline u32 ttcan_get_ttocf(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_TTOCF);
}

inline void ttcan_set_tttmc(struct ttcan_controller *ttcan, u32 value)
{
	ttcan_write32(ttcan, ADR_MTTCAN_TTTMC, value);
}

inline u32 ttcan_get_tttmc(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_TTTMC);
}

inline void ttcan_set_txbar(struct ttcan_controller *ttcan, u32 value)
{
	ttcan_write32(ttcan, ADR_MTTCAN_TXBAR, value);
}

inline u32 ttcan_get_ttmlm(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_TTMLM);
}

inline u32 ttcan_get_cccr(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_CCCR);
}

inline u32 ttcan_get_ttost(struct ttcan_controller *ttcan)
{
	return ttcan_read32(ttcan, ADR_MTTCAN_TTOST);
}

static bool validate_matrix_cyc_cnt(u32 cyc_num)
{
	if (cyc_num && ((cyc_num + 1) & cyc_num))
		return false;
	return true;
}

int ttcan_set_matrix_limits(struct ttcan_controller *ttcan,
			    u32 entt, u32 txew, u32 css, u32 ccm)
{
	u32 ttmlm = 0;

	if (!validate_matrix_cyc_cnt(ccm) ||
	    (txew > 15) || (css > 2) || (entt > 4095)) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -1;
	}
	ttmlm = (entt << MTT_TTMLM_ENTT_SHIFT) & MTT_TTMLM_ENTT_MASK;
	ttmlm |= (txew << MTT_TTMLM_TXEW_SHIFT) & MTT_TTMLM_TXEW_MASK;
	ttmlm |= (css << MTT_TTMLM_CSS_SHIFT) & MTT_TTMLM_CSS_MASK;
	ttmlm |= (ccm << MTT_TTMLM_CCM_SHIFT) & MTT_TTMLM_CCM_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_TTMLM, ttmlm);

	return 0;
}

#define MTTCAN_TT_TIMEOUT 1000
int ttcan_set_tur_config(struct ttcan_controller *ttcan, u16 denominator,
			 u16 numerator, int local_timing_enable)
{
	int timeout = MTTCAN_TT_TIMEOUT;
	u32 turcf = ttcan_read32(ttcan, ADR_MTTCAN_TURCF);

	if (!denominator && local_timing_enable) {
		pr_err("%s: Invalid Denominator value\n", __func__);
		return -1;
	}

	if (turcf & MTT_TURCF_ELT_MASK) {
		turcf &= ~(MTT_TURCF_ELT_MASK);
		ttcan_write32(ttcan, ADR_MTTCAN_TURCF, turcf);
	}

	if (!local_timing_enable)
		return 0;

	if (turcf & MTT_TURCF_ELT_MASK) {
		while ((turcf & MTT_TURCF_ELT_MASK) && timeout) {
			udelay(1);
			timeout--;
			turcf = ttcan_read32(ttcan, ADR_MTTCAN_TURCF);
		}
		if (!timeout) {
			pr_err("%s: TURCF access is locked\n", __func__);
			return -1;
		}
	}

	/* TBD: Take care when the DEN or NUM is programmed
	 * outside TT Conf mode
	 */

	turcf = (numerator << MTT_TURCF_NCL_SHIFT) & MTT_TURCF_NCL_MASK;
	turcf |= (denominator << MTT_TURCF_DC_SHIFT) & MTT_TURCF_DC_MASK;
	turcf |= MTT_TURCF_ELT_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_TURCF, turcf);

	return 0;
}

void ttcan_set_trigger_mem_conf(struct ttcan_controller *ttcan)
{
	u32 tttmc = 0;
	u32 rel_start_addr = ttcan->mram_cfg[MRAM_TMC].off >> 2;
	u8 elem_num = ttcan->mram_cfg[MRAM_TMC].num;

	if (elem_num > 64)
		elem_num = 64;

	tttmc = (elem_num << MTT_TTTMC_TME_SHIFT) & MTT_TTTMC_TME_MASK;
	tttmc |= (rel_start_addr << MTT_TTTMC_TMSA_SHIFT) &
		MTT_TTTMC_TMSA_MASK;

	ttcan_write32(ttcan, ADR_MTTCAN_TTTMC, tttmc);
}
