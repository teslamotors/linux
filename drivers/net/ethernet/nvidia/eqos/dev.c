/* =========================================================================
 * The Synopsys DWC ETHER QOS Software Driver and documentation (hereinafter
 * "Software") is an unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto.  Permission is hereby granted,
 * free of charge, to any person obtaining a copy of this software annotated
 * with this license and the Software, to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * ========================================================================= */
/*
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
/*!@file: eqos_dev.c
 * @brief: Driver functions.
 */
#include <linux/tegra-soc.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include "yheader.h"
#include "yapphdr.h"

extern ULONG eqos_base_addr;
#include "yregacc.h"
#include "nvregacc.h"

/*!
* \brief This sequence is used to enable/disable MAC loopback mode
* \param[in] enb_dis
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_mac_loopback_mode(UINT enb_dis)
{

	MAC_MCR_LM_WR(enb_dis);

	return Y_SUCCESS;
}

/* enable/disable PFC(Priority Based Flow Control) */
static void config_pfc(int enb_dis)
{
	MAC_RFCR_PFCE_WR(enb_dis);
}

/*!
* \brief This sequence is used to enable/disable ARP offload
* \param[in] enb_dis
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int config_arp_offload(int enb_dis)
{

	MAC_MCR_ARPEN_WR(enb_dis);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update the IP addr into MAC ARP Add reg,
* which is used by MAC for replying to ARP packets
* \param[in] addr
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int update_arp_offload_ip_addr(UCHAR addr[])
{

	MAC_ARPA_WR((addr[3] | (addr[2] << 8) | (addr[1] << 16) | addr[0] <<
		     24));

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to get the status of LPI/EEE mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static u32 get_lpi_status(void)
{
	u32 varmac_lps;

	MAC_LPS_RD(varmac_lps);

	return varmac_lps;
}

/*!
* \brief This sequence is used to enable EEE mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int set_eee_mode(void)
{

	DBGPR_EEE("set_eee_mode\n");
	MAC_LPS_LPIEN_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to disable EEE mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int reset_eee_mode(void)
{

	MAC_LPS_LPITXA_WR(0);
	MAC_LPS_LPIEN_WR(0);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to set PLS bit
* \param[in] phy_link
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int set_eee_pls(int phy_link)
{

	if (phy_link == 1) {
		MAC_LPS_PLS_WR(0x1);
	} else {
		MAC_LPS_PLS_WR(0);
	}

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to set EEE timer values
* \param[in] lpi_lst
* \param[in] lpi_twt
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int set_eee_timer(int lpi_lst, int lpi_twt)
{

	/* mim time(us) for which the MAC waits after it stops transmitting */
	/* the LPI pattern to the PHY and before it resumes the normal transmission. */
	MAC_LPC_TWT_WR(lpi_twt);
	/* mim time(ms) for which the link status from the PHY should be Up before */
	/* the LPI pattern can be transmitted to the PHY. */
	MAC_LPC_TLPIEX_WR(lpi_lst);

	return Y_SUCCESS;
}

static int set_lpi_tx_automate(void)
{
	MAC_LPS_LPITXA_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable/disable Auto-Negotiation
* and restart the autonegotiation
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int control_an(bool enable, bool restart)
{

	MAC_ANC_ANE_WR(enable);
	MAC_ANC_RAN_WR(restart);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to get Auto-Negotiation advertisment
* pause parameter
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int get_an_adv_pause_param(void)
{
	unsigned long varmac_aad;

	MAC_AAD_RD(varmac_aad);

	return GET_VALUE(varmac_aad, MAC_AAD_PSE_LPOS, MAC_AAD_PSE_HPOS);
}

/*!
* \brief This sequence is used to get Auto-Negotiation advertisment
* duplex parameter. Returns one if Full duplex mode is selected
* else returns zero
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int get_an_adv_duplex_param(void)
{
	unsigned long varmac_aad;

	MAC_AAD_RD(varmac_aad);
	if (GET_VALUE(varmac_aad, MAC_AAD_FD_LPOS, MAC_AAD_FD_HPOS) == 1) {
		return 1;
	} else {
		return 0;
	}
}

/*!
* \brief This sequence is used to get Link partner Auto-Negotiation
* advertisment pause parameter
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int get_lp_an_adv_pause_param(void)
{
	unsigned long varmac_alpa;

	MAC_ALPA_RD(varmac_alpa);

	return GET_VALUE(varmac_alpa, MAC_ALPA_PSE_LPOS, MAC_ALPA_PSE_HPOS);
}

/*!
* \brief This sequence is used to get Link partner Auto-Negotiation
* advertisment duplex parameter. Returns one if Full duplex mode
* is selected else returns zero
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int get_lp_an_adv_duplex_param(void)
{
	unsigned long varmac_alpa;

	MAC_ALPA_RD(varmac_alpa);
	if (GET_VALUE(varmac_alpa, MAC_ALPA_FD_LPOS, MAC_ALPA_FD_HPOS) == 1) {
		return 1;
	} else {
		return 0;
	}
}

static UINT get_vlan_tag_comparison(void)
{
	UINT etv;

	MAC_VLANTR_ETV_RD(etv);

	return etv;
}

/*!
* \brief This sequence is used to enable/disable VLAN filtering and
* also selects VLAN filtering mode- perfect/hash
* \param[in] filter_enb_dis
* \param[in] perfect_hash
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_vlan_filtering(INT filter_enb_dis,
				 INT perfect_hash_filtering,
				 INT perfect_inverse_match)
{
	MAC_MPFR_VTFE_WR(filter_enb_dis);
	MAC_VLANTR_VTIM_WR(perfect_inverse_match);
	MAC_VLANTR_VTHM_WR(perfect_hash_filtering);
	/* To enable only HASH filtering then VL/VID
	 * should be > zero. Hence we are writting 1 into VL.
	 * It also means that MAC will always receive VLAN pkt with
	 * VID = 1 if inverse march is not set.
	 * */
	if (perfect_hash_filtering)
		MAC_VLANTR_VL_WR(0x1);

	/* By default enable MAC to calculate vlan hash on
	 * only 12-bits of received VLAN tag (ie only on
	 * VLAN id and ignore priority and other fields)
	 * */
	if (perfect_hash_filtering)
		MAC_VLANTR_ETV_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update the VLAN ID for perfect filtering
* \param[in] vid
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_vlan_id(USHORT vid)
{

	MAC_VLANTR_VL_WR(vid);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update the VLAN Hash Table reg with new VLAN ID
* \param[in] data
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_vlan_hash_table_reg(USHORT data)
{

	MAC_VLANHTR_VLHT_WR(data);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to get the content of VLAN Hash Table reg
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT get_vlan_hash_table_reg(void)
{
	ULONG mac_vlanhtr;

	MAC_VLANHTR_RD(mac_vlanhtr);

	return GET_VALUE(mac_vlanhtr, MAC_VLANHTR_VLHT_LPOS,
			 MAC_VLANHTR_VLHT_HPOS);
}

/*!
* \brief This sequence is used to update Destination Port Number for
* L4(TCP/UDP) layer filtering
* \param[in] filter_no
* \param[in] port_no
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_l4_da_port_no(INT filter_no, USHORT port_no)
{

	MAC_L4AR_L4DP0_WR(filter_no, port_no);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update Source Port Number for
* L4(TCP/UDP) layer filtering
* \param[in] filter_no
* \param[in] port_no
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_l4_sa_port_no(INT filter_no, USHORT port_no)
{

	MAC_L4AR_L4SP0_WR(filter_no, port_no);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure L4(TCP/UDP) filters for
* SA and DA Port Number matching
* \param[in] filter_no
* \param[in] tcp_udp_match
* \param[in] src_dst_port_match
* \param[in] perfect_inverse_match
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_l4_filters(INT filter_no,
			     INT enb_dis,
			     INT tcp_udp_match,
			     INT src_dst_port_match, INT perfect_inverse_match)
{

	MAC_L3L4CR_L4PEN0_WR(filter_no, tcp_udp_match);

	if (src_dst_port_match == 0) {
		if (enb_dis == 1) {
			/* Enable L4 filters for SOURCE Port No matching */
			MAC_L3L4CR_L4SPM0_WR(filter_no, 0x1);
			MAC_L3L4CR_L4SPIM0_WR(filter_no, perfect_inverse_match);
		} else {
			/* Disable L4 filters for SOURCE Port No matching */
			MAC_L3L4CR_L4SPM0_WR(filter_no, 0x0);
			MAC_L3L4CR_L4SPIM0_WR(filter_no, 0x0);
		}
	} else {
		if (enb_dis == 1) {
			/* Enable L4 filters for DESTINATION port No matching */
			MAC_L3L4CR_L4DPM0_WR(filter_no, 0x1);
			MAC_L3L4CR_L4DPIM0_WR(filter_no, perfect_inverse_match);
		} else {
			/* Disable L4 filters for DESTINATION port No matching */
			MAC_L3L4CR_L4DPM0_WR(filter_no, 0x0);
			MAC_L3L4CR_L4DPIM0_WR(filter_no, 0x0);
		}
	}

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update IPv6 source/destination Address for L3 layer filtering
* \param[in] filter_no
* \param[in] addr
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_ip6_addr(INT filter_no, USHORT addr[])
{
	/* update Bits[31:0] of 128-bit IP addr */
	MAC_L3A0R_WR(filter_no, (addr[7] | (addr[6] << 16)));
	/* update Bits[63:32] of 128-bit IP addr */
	MAC_L3A1R_WR(filter_no, (addr[5] | (addr[4] << 16)));
	/* update Bits[95:64] of 128-bit IP addr */
	MAC_L3A2R_WR(filter_no, (addr[3] | (addr[2] << 16)));
	/* update Bits[127:96] of 128-bit IP addr */
	MAC_L3A3R_WR(filter_no, (addr[1] | (addr[0] << 16)));

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update IPv4 destination Address for L3 layer filtering
* \param[in] filter_no
* \param[in] addr
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_ip4_addr1(INT filter_no, UCHAR addr[])
{
	MAC_L3A1R_WR(filter_no,
		     (addr[3] | (addr[2] << 8) | (addr[1] << 16) |
		      (addr[0] << 24)));

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update IPv4 source Address for L3 layer filtering
* \param[in] filter_no
* \param[in] addr
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_ip4_addr0(INT filter_no, UCHAR addr[])
{
	MAC_L3A0R_WR(filter_no,
		     (addr[3] | (addr[2] << 8) | (addr[1] << 16) |
		      (addr[0] << 24)));

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure L3(IPv4/IPv6) filters
* for SA/DA Address matching
* \param[in] filter_no
* \param[in] ipv4_ipv6_match
* \param[in] src_dst_addr_match
* \param[in] perfect_inverse_match
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_l3_filters(INT filter_no,
			     INT enb_dis,
			     INT ipv4_ipv6_match,
			     INT src_dst_addr_match, INT perfect_inverse_match)
{
	MAC_L3L4CR_L3PEN0_WR(filter_no, ipv4_ipv6_match);

	/* For IPv6 either SA/DA can be checked, not both */
	if (ipv4_ipv6_match == 1) {
		if (enb_dis == 1) {
			if (src_dst_addr_match == 0) {
				/* Enable L3 filters for IPv6 SOURCE addr matching */
				MAC_L3L4CR_L3SAM0_WR(filter_no, 0x1);
				MAC_L3L4CR_L3SAIM0_WR(filter_no,
						      perfect_inverse_match);
				MAC_L3L4CR_L3DAM0_WR(filter_no, 0x0);
				MAC_L3L4CR_L3DAIM0_WR(filter_no, 0x0);
			} else {
				/* Enable L3 filters for IPv6 DESTINATION addr matching */
				MAC_L3L4CR_L3SAM0_WR(filter_no, 0x0);
				MAC_L3L4CR_L3SAIM0_WR(filter_no, 0x0);
				MAC_L3L4CR_L3DAM0_WR(filter_no, 0x1);
				MAC_L3L4CR_L3DAIM0_WR(filter_no,
						      perfect_inverse_match);
			}
		} else {
			/* Disable L3 filters for IPv6 SOURCE/DESTINATION addr matching */
			MAC_L3L4CR_L3PEN0_WR(filter_no, 0x0);
			MAC_L3L4CR_L3SAM0_WR(filter_no, 0x0);
			MAC_L3L4CR_L3SAIM0_WR(filter_no, 0x0);
			MAC_L3L4CR_L3DAM0_WR(filter_no, 0x0);
			MAC_L3L4CR_L3DAIM0_WR(filter_no, 0x0);
		}
	} else {
		if (src_dst_addr_match == 0) {
			if (enb_dis == 1) {
				/* Enable L3 filters for IPv4 SOURCE addr matching */
				MAC_L3L4CR_L3SAM0_WR(filter_no, 0x1);
				MAC_L3L4CR_L3SAIM0_WR(filter_no,
						      perfect_inverse_match);
			} else {
				/* Disable L3 filters for IPv4 SOURCE addr matching */
				MAC_L3L4CR_L3SAM0_WR(filter_no, 0x0);
				MAC_L3L4CR_L3SAIM0_WR(filter_no, 0x0);
			}
		} else {
			if (enb_dis == 1) {
				/* Enable L3 filters for IPv4 DESTINATION addr matching */
				MAC_L3L4CR_L3DAM0_WR(filter_no, 0x1);
				MAC_L3L4CR_L3DAIM0_WR(filter_no,
						      perfect_inverse_match);
			} else {
				/* Disable L3 filters for IPv4 DESTINATION addr matching */
				MAC_L3L4CR_L3DAM0_WR(filter_no, 0x0);
				MAC_L3L4CR_L3DAIM0_WR(filter_no, 0x0);
			}
		}
	}

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure MAC in differnet pkt processing
* modes like promiscuous, multicast, unicast, hash unicast/multicast.
* \param[in] pr_mode
* \param[in] huc_mode
* \param[in] hmc_mode
* \param[in] pm_mode
* \param[in] hpf_mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_mac_pkt_filter_reg(UCHAR pr_mode,
				     UCHAR huc_mode,
				     UCHAR hmc_mode,
				     UCHAR pm_mode, UCHAR hpf_mode)
{
	ULONG mac_mpfr;

	/* configure device in differnet modes */
	/* promiscuous, hash unicast, hash multicast, */
	/* all multicast and perfect/hash filtering mode. */
	MAC_MPFR_RD(mac_mpfr);
	mac_mpfr = mac_mpfr & (ULONG) (0x803103e8);
	mac_mpfr =
	    mac_mpfr | ((pr_mode) << 0) | ((huc_mode) << 1) | ((hmc_mode) << 2)
	    | ((pm_mode) << 4) | ((hpf_mode) << 10);
	MAC_MPFR_WR(mac_mpfr);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable/disable L3 and L4 filtering
* \param[in] filter_enb_dis
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_l3_l4_filter_enable(INT filter_enb_dis)
{

	MAC_MPFR_IPFE_WR(filter_enb_dis);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to select perfect/inverse matching for L2 DA
* \param[in] perfect_inverse_match
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_l2_da_perfect_inverse_match(INT perfect_inverse_match)
{

	MAC_MPFR_DAIF_WR(perfect_inverse_match);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update the MAC address in last 96 MAC
* address Low and High register(32-127) for L2 layer filtering
* \param[in] idx
* \param[in] addr
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_mac_addr32_127_low_high_reg(INT idx, UCHAR addr[])
{

	MAC_MA32_127LR_WR(idx,
			  (addr[0] | (addr[1] << 8) | (addr[2] << 16) |
			   (addr[3] << 24)));
	MAC_MA32_127HR_ADDRHI_WR(idx, (addr[4] | (addr[5] << 8)));
	MAC_MA32_127HR_AE_WR(idx, 0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to update the MAC address in first 31 MAC
* address Low and High register(1-31) for L2 layer filtering
* \param[in] idx
* \param[in] addr
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_mac_addr1_31_low_high_reg(INT idx, UCHAR addr[])
{

	MAC_MA1_31LR_WR(idx,
			(addr[0] | (addr[1] << 8) | (addr[2] << 16) |
			 (addr[3] << 24)));
	MAC_MA1_31HR_ADDRHI_WR(idx, (addr[4] | (addr[5] << 8)));
	MAC_MA1_31HR_AE_WR(idx, 0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure hash table register for
* hash address filtering
* \param[in] idx
* \param[in] data
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT update_hash_table_reg(INT idx, UINT data)
{

	MAC_HTR_WR(idx, data);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used check whether Tx drop status in the
* MTL is enabled or not, returns 1 if it is enabled and 0 if
* it is disabled.
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT drop_tx_status_enabled(void)
{
	ULONG mtl_omr;

	MTL_OMR_RD(mtl_omr);

	return GET_VALUE(mtl_omr, MTL_OMR_DTXSTS_LPOS, MTL_OMR_DTXSTS_HPOS);
}

/*!
* \brief This sequence is used configure MAC SSIR
* \param[in] ptp_clock
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_sub_second_increment(ULONG ptp_clock)
{
	ULONG val;
	ULONG mac_tcr;

	MAC_TCR_RD(mac_tcr);

	/* convert the PTP_CLOCK to nano second */
	/*  formula is : ((1/ptp_clock) * 1000000000) */
	/*  where, ptp_clock = 50MHz if FINE correction */
	/*  and ptp_clock = EQOS_SYSCLOCK if COARSE correction */
	if (GET_VALUE(mac_tcr, MAC_TCR_TSCFUPDT_LPOS, MAC_TCR_TSCFUPDT_HPOS) ==
	    1) {
		val = ((1 * 1000000000ull) / 62500000);
	} else {
		val = ((1 * 1000000000ull) / ptp_clock);
	}

	/* 0.465ns accurecy */
	if (GET_VALUE(mac_tcr, MAC_TCR_TSCTRLSSR_LPOS, MAC_TCR_TSCTRLSSR_HPOS)
	    == 0) {
		val = (val * 1000) / 465;
	}
	MAC_SSIR_SSINC_WR(val);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used get 64-bit system time in nano sec
* \return (unsigned long long) on success
* \retval ns
*/

static ULONG_LONG get_systime(void)
{
	ULONG_LONG ns;
	ULONG varmac_stnsr;
	ULONG varmac_stsr;

	MAC_STNSR_RD(varmac_stnsr);
	ns = GET_VALUE(varmac_stnsr, MAC_STNSR_TSSS_LPOS, MAC_STNSR_TSSS_HPOS);
	/* convert sec/high time value to nanosecond */
	MAC_STSR_RD(varmac_stsr);
	ns = ns + (varmac_stsr * 1000000000ull);

	return ns;
}

/*!
* \brief This sequence is used to adjust/update the system time
* \param[in] sec
* \param[in] nsec
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT adjust_systime(UINT sec,
			  UINT nsec, INT add_sub, bool one_nsec_accuracy)
{
	ULONG retry_cnt = 100000;
	ULONG vy_count;
	volatile ULONG mac_tcr;

	/* wait for previous(if any) time adjust/update to complete. */

	/*Poll */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		}

		MAC_TCR_RD(mac_tcr);
		if (GET_VALUE(mac_tcr, MAC_TCR_TSUPDT_LPOS, MAC_TCR_TSUPDT_HPOS)
		    == 0) {
			break;
		}
		vy_count++;
		mdelay(1);
	}

	if (add_sub) {
		/* If the new sec value needs to be subtracted with
		 * the system time, then MAC_STSUR reg should be
		 * programmed with (2^32 – <new_sec_value>)
		 * */
		sec = (0x100000000ull - sec);

		/* If the new nsec value need to be subtracted with
		 * the system time, then MAC_STNSUR.TSSS field should be
		 * programmed with,
		 * (10^9 - <new_nsec_value>) if MAC_TCR.TSCTRLSSR is set or
		 * (2^31 - <new_nsec_value> if MAC_TCR.TSCTRLSSR is reset)
		 * */
		if (one_nsec_accuracy)
			nsec = (0x3B9ACA00 - nsec);
		else
			nsec = (0x80000000 - nsec);
	}

	MAC_STSUR_WR(sec);
	MAC_STNSUR_TSSS_WR(nsec);
	MAC_STNSUR_ADDSUB_WR(add_sub);

	/* issue command to initialize system time with the value */
	/* specified in MAC_STSUR and MAC_STNSUR. */
	MAC_TCR_TSUPDT_WR(0x1);
	/* wait for present time initialize to complete. */

	/*Poll */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		}

		MAC_TCR_RD(mac_tcr);
		if (GET_VALUE(mac_tcr, MAC_TCR_TSUPDT_LPOS, MAC_TCR_TSUPDT_HPOS)
		    == 0) {
			break;
		}
		vy_count++;
		mdelay(1);
	}

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to adjust the ptp operating frequency.
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_addend(UINT data)
{
	ULONG retry_cnt = 100000;
	ULONG vy_count;
	volatile ULONG mac_tcr;

	/* wait for previous(if any) added update to complete. */

	/*Poll */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		}

		MAC_TCR_RD(mac_tcr);
		if (GET_VALUE
		    (mac_tcr, MAC_TCR_TSADDREG_LPOS,
		     MAC_TCR_TSADDREG_HPOS) == 0) {
			break;
		}
		vy_count++;
		mdelay(1);
	}

	MAC_TAR_WR(data);
	/* issue command to update the added value */
	MAC_TCR_TSADDREG_WR(0x1);
	/* wait for present added update to complete. */

	/*Poll */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		}
		MAC_TCR_RD(mac_tcr);
		if (GET_VALUE
		    (mac_tcr, MAC_TCR_TSADDREG_LPOS,
		     MAC_TCR_TSADDREG_HPOS) == 0) {
			break;
		}
		vy_count++;
		mdelay(1);
	}

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to initialize the system time
* \param[in] sec
* \param[in] nsec
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT init_systime(UINT sec, UINT nsec)
{
	ULONG retry_cnt = 100000;
	ULONG vy_count;
	volatile ULONG mac_tcr;

	/* wait for previous(if any) time initialize to complete. */

	/*Poll */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		}

		MAC_TCR_RD(mac_tcr);
		if (GET_VALUE(mac_tcr, MAC_TCR_TSINIT_LPOS, MAC_TCR_TSINIT_HPOS)
		    == 0) {
			break;
		}
		vy_count++;
		mdelay(1);
	}
	MAC_STSUR_WR(sec);
	MAC_STNSUR_WR(nsec);
	/* issue command to initialize system time with the value */
	/* specified in MAC_STSUR and MAC_STNSUR. */
	MAC_TCR_TSINIT_WR(0x1);
	/* wait for present time initialize to complete. */

	/*Poll */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		}

		MAC_TCR_RD(mac_tcr);
		if (GET_VALUE(mac_tcr, MAC_TCR_TSINIT_LPOS, MAC_TCR_TSINIT_HPOS)
		    == 0) {
			break;
		}
		vy_count++;
		mdelay(1);
	}

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable HW time stamping
* and receive frames
* \param[in] count
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_hw_time_stamping(UINT config_val)
{

	MAC_TCR_WR(config_val);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used get the 64-bit of the timestamp
* captured by the device for the corresponding received packet
* in nanosecond.
* \param[in] rxdesc
* \return (unsigned long long) on success
* \retval ns
*/

static ULONG_LONG get_rx_tstamp(t_rx_context_desc *rxdesc)
{
	ULONG_LONG ns;
	ULONG rdes1;

	RX_CONTEXT_DESC_RDES0_RD(rxdesc->rdes0, ns);
	RX_CONTEXT_DESC_RDES1_RD(rxdesc->rdes1, rdes1);
	ns = ns + (rdes1 * 1000000000ull);

	return ns;
}

/*!
* \brief This sequence is used to check whether the captured timestamp
* for the corresponding received packet is valid or not.
* Returns 0 if no context descriptor
* Returns 1 if timestamp is valid
* Returns 2 if time stamp is corrupted
* \param[in] rxdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static UINT get_rx_tstamp_status(t_rx_context_desc *rxdesc)
{
	UINT own;
	UINT ctxt;
	UINT rdes0;
	UINT rdes1;

	/* check for own bit and CTXT bit */
	RX_CONTEXT_DESC_RDES3_OWN_RD(rxdesc->rdes3, own);
	RX_CONTEXT_DESC_RDES3_CTXT_RD(rxdesc->rdes3, ctxt);
	if ((own == 0) && (ctxt == 0x1)) {
		RX_CONTEXT_DESC_RDES0_RD(rxdesc->rdes0, rdes0);
		RX_CONTEXT_DESC_RDES1_RD(rxdesc->rdes1, rdes1);
		if ((rdes0 == 0xffffffff) && (rdes1 == 0xffffffff)) {
			/* time stamp is corrupted */
			return 2;
		} else {
			/* time stamp is valid */
			return 1;
		}
	} else {
		/* no CONTEX desc to hold time stamp value */
		return 0;
	}
}

/*!
* \brief This sequence is used to check whether the timestamp value
* is available in a context descriptor or not. Returns 1 if timestamp
* is available else returns 0
* \param[in] rxdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static UINT rx_tstamp_available(t_rx_desc *rxdesc)
{
	UINT rs1v;
	UINT tsa;

	RX_NORMAL_DESC_RDES3_RS1V_RD(rxdesc->rdes3, rs1v);
	if (rs1v == 1) {
		RX_NORMAL_DESC_RDES1_TSA_RD(rxdesc->rdes1, tsa);
		return tsa;
	} else {
		return 0;
	}
}

/*!
* \brief This sequence is used get the least 64-bit of the timestamp
* captured by the device for the corresponding transmit packet in nanosecond
* \return (unsigned long long) on success
* \retval ns
*/

static ULONG_LONG get_tx_tstamp_via_reg(void)
{
	ULONG_LONG ns;
	ULONG varmac_ttn;

	MAC_TTSN_TXTSSTSLO_RD(ns);
	MAC_TTN_TXTSSTSHI_RD(varmac_ttn);
	ns = ns + (varmac_ttn * 1000000000ull);

	return ns;
}

/*!
* \brief This sequence is used to check whether a timestamp has been
* captured for the corresponding transmit packet. Returns 1 if
* timestamp is taken else returns 0
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static UINT get_tx_tstamp_status_via_reg(void)
{
	ULONG mac_tcr;
	ULONG mac_ttsn;

	/* device is configured to overwrite the timesatmp of */
	/* eariler packet if driver has not yet read it. */
	MAC_TCR_RD(mac_tcr);
	if (GET_VALUE(mac_tcr, MAC_TCR_TXTSSTSM_LPOS, MAC_TCR_TXTSSTSM_HPOS) ==
	    1) {
		/* nothing to do */
	} else {
		/* timesatmp of the current pkt is ignored or not captured */
		MAC_TTSN_RD(mac_ttsn);
		if (GET_VALUE
		    (mac_ttsn, MAC_TTSN_TXTSSTSMIS_LPOS,
		     MAC_TTSN_TXTSSTSMIS_HPOS) == 1) {
			return 0;
		} else {
			return 1;
		}
	}

	return 0;
}

/*!
* \brief This sequence is used get the 64-bit of the timestamp captured
* by the device for the corresponding transmit packet in nanosecond.
* \param[in] txdesc
* \return (unsigned long long) on success
* \retval ns
*/

static ULONG_LONG get_tx_tstamp(t_tx_desc *txdesc)
{
	ULONG_LONG ns;
	ULONG vartdes1;

	TX_NORMAL_DESC_TDES0_RD(txdesc->tdes0, ns);
	TX_NORMAL_DESC_TDES1_RD(txdesc->tdes1, vartdes1);
	ns = ns + (vartdes1 * 1000000000ull);

	return ns;
}

/*!
* \brief This sequence is used to check whether a timestamp has been
* captured for the corresponding transmit packet. Returns 1 if
* timestamp is taken else returns 0
* \param[in] txdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static UINT get_tx_tstamp_status(t_tx_desc *txdesc)
{
	UINT tdes3;

	TX_NORMAL_DESC_TDES3_RD(txdesc->tdes3, tdes3);

	return tdes3 & 0x20000;
}

/*!
* \brief This sequence is used to set tx queue operating mode for Queue[0 - 7]
* \param[in] qinx
* \param[in] q_mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_tx_queue_operating_mode(UINT qinx, UINT q_mode)
{

	MTL_QTOMR_TXQEN_WR(qinx, q_mode);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to select Tx Scheduling Algorithm for AVB feature for Queue[1 - 7]
* \param[in] avb_algo
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_avb_algorithm(UINT qinx, UCHAR avb_algo)
{

	MTL_QECR_AVALG_WR(qinx, avb_algo);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure credit-control for Queue[1 - 7]
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_credit_control(UINT qinx, UINT cc)
{

	MTL_QECR_CC_WR(qinx, cc);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure send slope credit value
* required for the credit-based shaper alogorithm for Queue[1 - 7]
* \param[in] qinx
* \param[in] send_slope
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_send_slope(UINT qinx, UINT send_slope)
{

	MTL_QSSCR_SSC_WR(qinx, send_slope);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure idle slope credit value
* required for the credit-based shaper alogorithm for Queue[1 - 7]
* \param[in] qinx
* \param[in] idle_slope
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_idle_slope(UINT qinx, UINT idle_slope)
{

	MTL_QW_ISCQW_WR(qinx, idle_slope);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure low credit value
* required for the credit-based shaper alogorithm for Queue[1 - 7]
* \param[in] qinx
* \param[in] low_credit
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_low_credit(UINT qinx, UINT low_credit)
{

	MTL_QLCR_LC_WR(qinx, low_credit);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable/disable slot number check When set,
* this bit enables the checking of the slot number programmed in the TX
* descriptor with the current reference given in the RSN field. The DMA fetches
* the data from the corresponding buffer only when the slot number is: equal to
* the reference slot number or  ahead of the reference slot number by one.
*
* \param[in] qinx
* \param[in] slot_check
*
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_slot_num_check(UINT qinx, UCHAR slot_check)
{

	DMA_SFCSR_ESC_WR(qinx, slot_check);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable/disable advance slot check When set,
* this bit enables the DAM to fetch the data from the buffer when the slot
* number programmed in TX descriptor is equal to the reference slot number
* given in RSN field or ahead of the reference number by upto two slots
*
* \param[in] qinx
* \param[in] adv_slot_check
*
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_advance_slot_num_check(UINT qinx, UCHAR adv_slot_check)
{

	DMA_SFCSR_ASC_WR(qinx, adv_slot_check);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure high credit value required
* for the credit-based shaper alogorithm for Queue[1 - 7]
* \param[in] qinx
* \param[in] hi_credit
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_high_credit(UINT qinx, UINT hi_credit)
{

	MTL_QHCR_HC_WR(qinx, hi_credit);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to set weights for DCB feature for Queue[0 - 7]
* \param[in] qinx
* \param[in] q_weight
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_dcb_queue_weight(UINT qinx, UINT q_weight)
{

	MTL_QW_ISCQW_WR(qinx, q_weight);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to select Tx Scheduling Algorithm for DCB feature
* \param[in] dcb_algo
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_dcb_algorithm(UCHAR dcb_algo)
{

	MTL_OMR_SCHALG_WR(dcb_algo);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to get Tx queue count
* \param[in] count
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

UCHAR get_tx_queue_count(void)
{
	UCHAR count;
	ULONG mac_hfr2;

	MAC_HFR2_RD(mac_hfr2);
	count = GET_VALUE(mac_hfr2, MAC_HFR2_TXQCNT_LPOS, MAC_HFR2_TXQCNT_HPOS);

	return count + 1;
}

/*!
* \brief This sequence is used to get Rx queue count
* \param[in] count
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

UCHAR get_rx_queue_count(void)
{
	UCHAR count;
	ULONG mac_hfr2;

	MAC_HFR2_RD(mac_hfr2);
	count = GET_VALUE(mac_hfr2, MAC_HFR2_RXQCNT_LPOS, MAC_HFR2_RXQCNT_HPOS);

	return count + 1;
}

/*!
* \brief This sequence is used to disables all Tx/Rx MMC interrupts
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT disable_mmc_interrupts(void)
{

	/* disable all TX interrupts */
	MMC_INTR_MASK_TX_WR(0xffffffff);
	/* disable all RX interrupts */
	MMC_INTR_MASK_RX_WR(0xffffffff);
	MMC_IPC_INTR_MASK_RX_WR(0xffffffff);	/* Disable MMC Rx Interrupts for IPC */
	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure MMC counters
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_mmc_counters(void)
{
	ULONG mmc_cntrl;

	/* set COUNTER RESET */
	/* set RESET ON READ */
	/* set COUNTER PRESET */
	/* set FULL_HALF PRESET */
	MMC_CNTRL_RD(mmc_cntrl);
	mmc_cntrl = mmc_cntrl & (ULONG) (0x10a);
	mmc_cntrl = mmc_cntrl | ((0x1) << 0) | ((0x1) << 2) | ((0x1) << 4) |
	    ((0x1) << 5);
	MMC_CNTRL_WR(mmc_cntrl);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to disable given DMA channel rx interrupts
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT disable_rx_interrupt(UINT qinx, struct eqos_prv_data *pdata)
{
	u32 reg;

	VIRT_INTR_CH_CRTL_RD(qinx, reg);
	reg &= ~VIRT_INTR_CH_CRTL_RX_WR_MASK;
	VIRT_INTR_CH_CRTL_WR(qinx, reg);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable given DMA channel rx interrupts
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT enable_rx_interrupt(UINT qinx, struct eqos_prv_data *pdata)
{
	u32 reg;

	VIRT_INTR_CH_CRTL_RD(qinx, reg);
	reg |= VIRT_INTR_CH_CRTL_RX_WR_MASK;
	VIRT_INTR_CH_CRTL_WR(qinx, reg);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to disable given DMA channel tx/rx interrupts
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT disable_chan_interrupts(UINT qinx, struct eqos_prv_data *pdata)
{
	u32 reg;

	VIRT_INTR_CH_CRTL_RD(qinx, reg);
	reg &= ~pdata->chinfo[qinx].int_mask;
	VIRT_INTR_CH_CRTL_WR(qinx, reg);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable given DMA channel tx/rx interrupts
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT enable_chan_interrupts(UINT qinx, struct eqos_prv_data *pdata)
{
	u32 reg;

	VIRT_INTR_CH_CRTL_RD(qinx, reg);
	reg |= pdata->chinfo[qinx].int_mask;
	VIRT_INTR_CH_CRTL_WR(qinx, reg);

	return Y_SUCCESS;
}

static VOID configure_sa_via_reg(u32 cmd)
{
	MAC_MCR_SARC_WR(cmd);
}

static VOID configure_mac_addr1_reg(UCHAR *mac_addr)
{
	MAC_MA1HR_WR(((mac_addr[5] << 8) | (mac_addr[4])));
	MAC_MA1LR_WR(((mac_addr[3] << 24) | (mac_addr[2] << 16) |
		      (mac_addr[1] << 8) | (mac_addr[0])));
}

static VOID configure_mac_addr0_reg(UCHAR *mac_addr)
{
	MAC_MA0HR_WR(((mac_addr[5] << 8) | (mac_addr[4])));
	MAC_MA0LR_WR(((mac_addr[3] << 24) | (mac_addr[2] << 16) |
		      (mac_addr[1] << 8) | (mac_addr[0])));
}

static VOID config_rx_outer_vlan_stripping(u32 cmd)
{
	MAC_VLANTR_EVLS_WR(cmd);
}

static VOID config_rx_inner_vlan_stripping(u32 cmd)
{
	MAC_VLANTR_EIVLS_WR(cmd);
}

static VOID config_ptpoffload_engine(UINT pto_cr, UINT mc_uc)
{
	MAC_PTO_CR_WR(pto_cr);
	MAC_TCR_TSENMACADDR_WR(mc_uc);
}

static VOID config_ptp_channel(UINT ptp_ch_id, UINT addr_index)
{
	u32 dynamic_map_on;

	DBGPR_FILTER("-->config_ptp_channel\n");

	/* Set Q0 to do DA MAC based DMA Channel selection if not already set */
	MTL_RQDCM0R_RXQ0DADMACH_RD(dynamic_map_on);
	if (!dynamic_map_on)
		MTL_RQDCM0R_RXQ0DADMACH_WR(0x1);

	/* We do not need to explictly set the MAC DA based DMA Channel select
	 * bit (DCS) in all otherMAC Address filter registers. the reset value
	 * of this field is 0 and the driver does not touch this bit.
	 */

	/* Set the DMA channel selection for the PTP addresses */
	if (addr_index < 32)
		MAC_MA1_31HR_DCS_WR(addr_index, ptp_ch_id);
	else
		MAC_MA32_127HR_DCS_WR(addr_index, ptp_ch_id);

	DBGPR_FILTER("<--config_ptp_channel\n");
}

static VOID configure_reg_vlan_control(struct tx_ring
				       *ptx_ring)
{
	USHORT vlan_id = ptx_ring->vlan_tag_id;
	UINT vlan_control = ptx_ring->tx_vlan_tag_ctrl;

	MAC_VLANTIRR_WR(((1 << 18) | (vlan_control << 16) | (vlan_id << 0)));
}

static VOID configure_desc_vlan_control(struct eqos_prv_data *pdata)
{
	MAC_VLANTIRR_WR((1 << 20));
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/
#ifdef EQOS_ENABLE_VLAN_TAG
static INT configure_mac_for_vlan_pkt(void)
{

	/* Enable VLAN Tag stripping always */
	MAC_VLANTR_EVLS_WR(0x3);
	/* Enable operation on the outer VLAN Tag, if present */
	MAC_VLANTR_ERIVLT_WR(0);
	/* Disable double VLAN Tag processing on TX and RX */
	MAC_VLANTR_EDVLP_WR(0);
	/* Enable VLAN Tag in RX Status. */
	MAC_VLANTR_EVLRXS_WR(0x1);
	/* Disable VLAN Type Check */
	MAC_VLANTR_DOVLTC_WR(0x1);

	/* configure MAC to get VLAN Tag to be inserted/replaced from */
	/* TX descriptor(context desc) */
	MAC_VLANTIRR_VLTI_WR(0x1);
	/* insert/replace C_VLAN in 13th ans 14th bytes of transmitted frames */
	MAC_VLANTIRR_CSVL_WR(0);

	return Y_SUCCESS;
}
#endif
/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_pblx8(UINT qinx, UINT val)
{
	DMA_CR_PBLX8_WR(qinx, val);

	return Y_SUCCESS;
}

/*!
* \return INT
* \retval programmed Tx PBL value
*/

static INT get_tx_pbl_val(UINT qinx)
{
	UINT tx_pbl;

	DMA_TCR_PBL_RD(qinx, tx_pbl);

	return tx_pbl;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_tx_pbl_val(UINT qinx, UINT tx_pbl)
{
	DMA_TCR_PBL_WR(qinx, tx_pbl);

	return Y_SUCCESS;
}

/*!
* \return INT
* \retval programmed Rx PBL value
*/

static INT get_rx_pbl_val(UINT qinx)
{
	UINT rx_pbl;

	DMA_RCR_PBL_RD(qinx, rx_pbl);

	return rx_pbl;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_rx_pbl_val(UINT qinx, UINT rx_pbl)
{
	DMA_RCR_PBL_WR(qinx, rx_pbl);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_axi_rorl_val(UINT axi_rorl)
{
	DMA_SBUS_RD_OSR_LMT_WR(axi_rorl);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_axi_worl_val(UINT axi_worl)
{
	DMA_SBUS_WR_OSR_LMT_WR(axi_worl);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_axi_pbl_val(UINT axi_pbl)
{
	UINT dma_sbus;

	DMA_SBUS_RD(dma_sbus);
	dma_sbus &= ~DMA_SBUS_AXI_PBL_MASK;
	dma_sbus |= axi_pbl;
	DMA_SBUS_WR(dma_sbus);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_incr_incrx_mode(UINT val)
{
	DMA_SBUS_UNDEF_FB_WR(val);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_osf_mode(UINT qinx, UINT val)
{
	DMA_TCR_OSP_WR(qinx, val);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_rsf_mode(UINT qinx, UINT val)
{
	MTL_QROMR_RSF_WR(qinx, val);
	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_tsf_mode(UINT qinx, UINT val)
{
	MTL_QTOMR_TSF_WR(qinx, val);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_rx_threshold(UINT qinx, UINT val)
{
	MTL_QROMR_RTC_WR(qinx, val);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_tx_threshold(UINT qinx, UINT val)
{
	MTL_QTOMR_TTC_WR(qinx, val);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT config_rx_watchdog_timer(UINT qinx, u32 riwt)
{
	DMA_RIWTR_RWT_WR(qinx, riwt);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT enable_magic_pmt_operation(void)
{
	MAC_PMTCSR_MGKPKTEN_WR(0x1);
	MAC_PMTCSR_PWRDWN_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT disable_magic_pmt_operation(void)
{
	UINT pmtcsr_pwrdn;

	MAC_PMTCSR_MGKPKTEN_WR(0x0);
	MAC_PMTCSR_PWRDWN_RD(pmtcsr_pwrdn);
	if (pmtcsr_pwrdn == 0x1) {
		MAC_PMTCSR_PWRDWN_WR(0x0);
	}

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT enable_remote_pmt_operation(void)
{
	MAC_PMTCSR_RWKPKTEN_WR(0x1);
	MAC_PMTCSR_PWRDWN_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT disable_remote_pmt_operation(void)
{
	UINT pmtcsr_pwrdn;

	MAC_PMTCSR_RWKPKTEN_WR(0x0);
	MAC_PMTCSR_PWRDWN_RD(pmtcsr_pwrdn);
	if (pmtcsr_pwrdn == 0x1) {
		MAC_PMTCSR_PWRDWN_WR(0x0);
	}

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT configure_rwk_filter_registers(UINT *value, UINT count)
{
	UINT i;

	MAC_PMTCSR_RWKFILTRST_WR(1);
	for (i = 0; i < count; i++)
		MAC_RWPFFR_WR(value[i]);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT disable_tx_flow_ctrl(UINT qinx)
{

	MAC_QTFCR_TFE_WR(qinx, 0);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT enable_tx_flow_ctrl(UINT qinx)
{

	MAC_QTFCR_TFE_WR(qinx, 1);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT disable_rx_flow_ctrl(void)
{

	MAC_RFCR_RFE_WR(0);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT enable_rx_flow_ctrl(void)
{

	MAC_RFCR_RFE_WR(0x1);

	return Y_SUCCESS;
}

static uint get_dma_state(uint qinx, bool is_rx)
{
	u32 dma_dsr0;
	u32 dma_dsr1;
	u32 dma_dsr2;
	uint val;
	uint lpos, hpos;

	if (qinx <= 2) {
		lpos = (is_rx ? DMA_DSR0_RPS0_LPOS : DMA_DSR0_TPS0_LPOS)
		    + (qinx * 8);
		hpos = (is_rx ? DMA_DSR0_RPS0_HPOS : DMA_DSR0_TPS0_HPOS)
		    + (qinx * 8);
		DMA_DSR0_RD(dma_dsr0);
		val = GET_VALUE(dma_dsr0, lpos, hpos);
	} else if (qinx <= 6) {
		lpos = (is_rx ? DMA_DSR1_RPS3_LPOS : DMA_DSR1_TPS3_LPOS)
		    + ((qinx - 3) * 8);
		hpos = (is_rx ? DMA_DSR1_RPS3_HPOS : DMA_DSR1_TPS3_HPOS)
		    + ((qinx - 3) * 8);
		DMA_DSR1_RD(dma_dsr1);
		val = GET_VALUE(dma_dsr1, lpos, hpos);
	} else {
		lpos = (is_rx ? DMA_DSR2_RPS7_LPOS : DMA_DSR2_TPS7_LPOS)
		    + ((qinx - 3) * 8);
		hpos = (is_rx ? DMA_DSR2_RPS7_HPOS : DMA_DSR2_TPS7_HPOS)
		    + ((qinx - 3) * 8);
		DMA_DSR2_RD(dma_dsr2);
		val = GET_VALUE(dma_dsr2, lpos, hpos);
	}

	return val;
}

static void wait_for_dma_to_go_idle(uint qinx, bool is_rx)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(20);
	bool wait_for_idle;
	uint val;

	/* make sure DMA is suspended or stopped before stop command */
	do {
		val = get_dma_state(qinx, is_rx);

		/* wait if dma status is not suspended yet */
		if ((is_rx &&
		     ((val == DMA_RX_STATE_IDLE) ||
		      (val == DMA_RX_STATE_SUSPENDED))) ||
		    (!is_rx && (val == DMA_TX_STATE_SUSPENDED)))
			wait_for_idle = false;
		else {
			wait_for_idle = true;
			usleep_range(50, 60);
		}
	} while (wait_for_idle && time_is_after_jiffies(timeout));

	if (wait_for_idle)
		pr_err("%s%dDMA is not suspended\n", is_rx ? "Rx" : "Tx", qinx);
}

/*!
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT stop_dma_rx(UINT qinx)
{
	/* wait for dma to get idle or suspended */
	wait_for_dma_to_go_idle(qinx, true);

	/* issue Rx dma stop command */
	DMA_RCR_ST_WR(qinx, 0);

	return Y_SUCCESS;
}

/*!
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT start_dma_rx(UINT qinx)
{

	DMA_RCR_ST_WR(qinx, 0x1);

	return Y_SUCCESS;
}

/*!
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT stop_dma_tx(struct eqos_prv_data *pdata, UINT qinx)
{
	struct tx_ring *ptxdwr = GET_TX_WRAPPER_DESC(qinx);

	int start_idx = ptxdwr->dirty_tx;
	int end_idx = ptxdwr->cur_tx;
	uint own, fd, idx, cnt;
	t_tx_desc *ptxd;

	/* find desc hw is working on */
	idx = start_idx;
	while (idx != end_idx) {
		ptxd = GET_TX_DESC_PTR(qinx, idx);
		TX_NORMAL_DESC_TDES3_OWN_RD(ptxd->tdes3, own);
		if (own)
			break;
		idx = INCR_TX_LOCAL_INDEX(idx, 1);
	}
	if (idx == end_idx)
		goto done;

	/* skip past 4 descriptors to avoid possible race condtions */
	cnt = 4;
	while (cnt) {
		idx = INCR_TX_LOCAL_INDEX(idx, 1);
		if (idx == end_idx)
			goto done;
		cnt--;
	}

	/* find desc which is start of packet */
	while (idx != end_idx) {
		ptxd = GET_TX_DESC_PTR(qinx, idx);
		TX_NORMAL_DESC_TDES3_FD_RD(ptxd->tdes3, fd);
		if (fd) {
			/* turn off ownership bit */
			TX_NORMAL_DESC_TDES3_OWN_WR(ptxd->tdes3, 0);
			idx = INCR_TX_LOCAL_INDEX(idx, 1);
			break;
		}
		idx = INCR_TX_LOCAL_INDEX(idx, 1);
	}

	/* turn off ownership bit of remaining desc in hw owned region */
	while (idx != end_idx) {
		ptxd = GET_TX_DESC_PTR(qinx, idx);
		TX_NORMAL_DESC_TDES3_OWN_WR(ptxd->tdes3, 0);
		idx = INCR_TX_LOCAL_INDEX(idx, 1);
	}
 done:

	/* wait for dma to get idle or suspended */
	wait_for_dma_to_go_idle(qinx, false);

	/* issue Tx dma stop command */
	DMA_TCR_ST_WR(qinx, 0);

	return Y_SUCCESS;
}

/*!
* \param[in] qinx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT start_dma_tx(UINT qinx)
{

	DMA_TCR_ST_WR(qinx, 0x1);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/
static INT stop_mac_tx(void)
{
	ULONG mac_mcr;

	MAC_MCR_RD(mac_mcr);
	mac_mcr = mac_mcr & (ULONG) (0xffffff7d);
	MAC_MCR_WR(mac_mcr);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/
static INT stop_mac_rx(void)
{
	ULONG mac_mcr;

	MAC_MCR_RD(mac_mcr);
	mac_mcr = mac_mcr & (ULONG) (0xffffff7e);
	MAC_MCR_WR(mac_mcr);

	return Y_SUCCESS;
}

/*!
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT start_mac_tx_rx(void)
{
	ULONG mac_mcr;

	MAC_MCR_RD(mac_mcr);
	mac_mcr = mac_mcr & (ULONG) (0xffffff7c);
	mac_mcr = mac_mcr | ((0x1) << 1) | ((0x1) << 0);
	MAC_MCR_WR(mac_mcr);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable DMA interrupts
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT enable_dma_interrupts(UINT qinx, struct eqos_prv_data *pdata)
{
	UINT tmp;
	ULONG dma_sr;
	ULONG dma_ier;

	/* clear all the interrupts which are set */
	DMA_SR_RD(qinx, dma_sr);
	tmp = dma_sr;
	DMA_SR_WR(qinx, tmp);
	/* Enable following interrupts for Queue 0 */
	/* RIE - Receive Interrupt Enable */
	/* RBUE - Receive Buffer Unavailable Enable  */
	/* AIE - Abnormal Interrupt Summary Enable */
	/* NIE - Normal Interrupt Summary Enable */
	/* FBE - Fatal Bus Error Enable */
	DMA_IER_RD(qinx, dma_ier);
	dma_ier = dma_ier & (ULONG) (0x2e00);
#ifdef EQOS_VER_4_0
	dma_ier = dma_ier |
	    ((0x1) << 6) | ((0x1) << 7) | ((0x1) << 15) |
	    ((0x1) << 16) | ((0x1) << 12);
#else
	dma_ier = dma_ier |
	    ((0x1) << 6) | ((0x1) << 7) | ((0x1) << 14) |
	    ((0x1) << 15) | ((0x1) << 12);
#endif

#ifdef EQOS_TXPOLLING_MODE_ENABLE
	/* Disable TIE and TBUE */
	/* TIE - Transmit Interrupt Enable */
	/* TBUE - Transmit Buffer Unavailable Enable */
	dma_ier &= ~(((0x1) << 0) | ((0x1) << 2));
#else
	/* Enable TIE and TBUE */
	/* TIE - Transmit Interrupt Enable */
	/* TBUE - Transmit Buffer Unavailable Enable */
	dma_ier |= ((0x1) << 0) | ((0x1) << 2);
#endif
	/* For multi-irqs to work nie needs to be disabled.
	 * And disable tx ints for now
	 */
	dma_ier &= ~((0x1) << 15);

	DMA_IER_WR(qinx, dma_ier);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to change tx clock speed
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_tx_clk_speed(struct eqos_prv_data *pdata, INT speed)
{
	/* set eqos_tx clock to 125/25/2.5MHz based on speed */
	if (tegra_platform_is_silicon()) {
		struct platform_device *pdev = pdata->pdev;
		int ret;
		ret = clk_set_rate(pdata->tx_clk,
				   (speed == SPEED_10) ? 2500 * 1000 :
				   (speed ==
				    SPEED_100) ? 25000 * 1000 : 125000 * 1000);
		if (ret) {
			dev_err(&pdev->dev, "failed to set tx_clk to %sMHz\n",
				(speed == SPEED_10) ? "2.5" :
				(speed == SPEED_100) ? "25" : "125");
			return Y_FAILURE;
		}
	}
	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the MAC registers for
* GMII-1000Mbps speed
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_gmii_speed(struct eqos_prv_data *pdata)
{

	MAC_MCR_PS_WR(0);
	MAC_MCR_FES_WR(0);
	if (tegra_platform_is_unit_fpga())
		CLK_CRTL0_TX_CLK_WR(0);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the MAC registers for
* MII-10Mpbs speed
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_mii_speed_10(struct eqos_prv_data *pdata)
{

	MAC_MCR_PS_WR(0x1);
	MAC_MCR_FES_WR(0);
	if (tegra_platform_is_unit_fpga())
		CLK_CRTL0_TX_CLK_WR(1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the MAC registers for
* MII-100Mpbs speed
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_mii_speed_100(struct eqos_prv_data *pdata)
{

	MAC_MCR_PS_WR(0x1);
	MAC_MCR_FES_WR(0x1);
	if (tegra_platform_is_unit_fpga())
		CLK_CRTL0_TX_CLK_WR(0);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the MAC registers for
* half duplex mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_half_duplex(void)
{

	MAC_MCR_DM_WR(0);

	/* Need to do WAR to flush tx q when going to hd */
	MTL_Q0TOMR_FTQ_WR(1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the MAC registers for
* full duplex mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_full_duplex(void)
{

	MAC_MCR_DM_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the device in list of
* multicast mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_multicast_list_mode(void)
{

	MAC_MPFR_HMC_WR(0);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the device in unicast mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_unicast_mode(void)
{

	MAC_MPFR_HUC_WR(0);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the device in all multicast mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_all_multicast_mode(void)
{

	MAC_MPFR_PM_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to configure the device in promiscuous mode
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT set_promiscuous_mode(void)
{

	MAC_MPFR_PR_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to write into phy registers
* \param[in] phy_id
* \param[in] phy_reg
* \param[in] phy_reg_data
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT write_phy_regs(INT phy_id, INT phy_reg, INT phy_reg_data)
{
	ULONG retry_cnt = 1000;
	ULONG vy_count;
	volatile ULONG mac_gmiiar;

	/* wait for any previous MII read/write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		MAC_GMIIAR_RD(mac_gmiiar);
		if (GET_VALUE
		    (mac_gmiiar, MAC_GMIIAR_GB_LPOS, MAC_GMIIAR_GB_HPOS) == 0) {
			break;
		}

		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		} else {
			vy_count++;
			in_interrupt() ? mdelay(1) : msleep(1);
		}
	}
	/* write the data */
	MAC_GMIIDR_GD_WR(phy_reg_data);
	/* initiate the MII write operation by updating desired */
	/* phy address/id (0 - 31) */
	/* phy register offset */
	/* CSR Clock Range (20 - 35MHz) */
	/* Select write operation */
	/* set busy bit */
	MAC_GMIIAR_RD(mac_gmiiar);
	mac_gmiiar = mac_gmiiar & (ULONG) (0x12);
	mac_gmiiar =
	    mac_gmiiar | ((phy_id) << 21) | ((phy_reg) << 16) | ((0x2) << 8)
	    | ((0x1) << 2) | ((0x1) << 0);
	MAC_GMIIAR_WR(mac_gmiiar);

	/*DELAY IMPLEMENTATION USING udelay() */
	udelay(10);
	/* wait for MII write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		MAC_GMIIAR_RD(mac_gmiiar);
		if (GET_VALUE
		    (mac_gmiiar, MAC_GMIIAR_GB_LPOS, MAC_GMIIAR_GB_HPOS) == 0) {
			break;
		}
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		} else {
			vy_count++;
			in_interrupt() ? mdelay(1) : msleep(1);
		}
	}

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to read the phy registers
* \param[in] phy_id
* \param[in] phy_reg
* \param[out] phy_reg_data
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT read_phy_regs(INT phy_id, INT phy_reg, INT *phy_reg_data)
{
	ULONG retry_cnt = 1000;
	ULONG vy_count;
	volatile ULONG mac_gmiiar;
	ULONG mac_gmiidr;

	/* wait for any previous MII read/write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		MAC_GMIIAR_RD(mac_gmiiar);
		if (GET_VALUE
		    (mac_gmiiar, MAC_GMIIAR_GB_LPOS, MAC_GMIIAR_GB_HPOS) == 0) {
			break;
		}
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		} else {
			vy_count++;
			in_interrupt() ? mdelay(1) : msleep(1);
		}

	}
	/* initiate the MII read operation by updating desired */
	/* phy address/id (0 - 31) */
	/* phy register offset */
	/* CSR Clock Range (20 - 35MHz) */
	/* Select read operation */
	/* set busy bit */
	MAC_GMIIAR_RD(mac_gmiiar);
	mac_gmiiar = mac_gmiiar & (ULONG) (0x12);
	mac_gmiiar =
	    mac_gmiiar | ((phy_id) << 21) | ((phy_reg) << 16) | ((0x2) << 8)
	    | ((0x3) << 2) | ((0x1) << 0);
	MAC_GMIIAR_WR(mac_gmiiar);

	/*DELAY IMPLEMENTATION USING udelay() */
	udelay(10);
	/* wait for MII write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		MAC_GMIIAR_RD(mac_gmiiar);
		if (GET_VALUE
		    (mac_gmiiar, MAC_GMIIAR_GB_LPOS, MAC_GMIIAR_GB_HPOS) == 0) {
			break;
		}
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		} else {
			vy_count++;
			in_interrupt() ? mdelay(1) : msleep(1);
		}

	}
	/* read the data */
	MAC_GMIIDR_RD(mac_gmiidr);
	*phy_reg_data =
	    GET_VALUE(mac_gmiidr, MAC_GMIIDR_GD_LPOS, MAC_GMIIDR_GD_HPOS);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to check whether transmitted pkts have
* fifo under run loss error or not, returns 1 if fifo under run error
* else returns 0
* \param[in] txdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT tx_fifo_underrun(t_tx_desc *txdesc)
{
	UINT tdes3;

	/* check tdes3.UF bit */
	TX_NORMAL_DESC_TDES3_RD(txdesc->tdes3, tdes3);
	if ((tdes3 & 0x4) == 0x4) {
		return 1;
	} else {
		return 0;
	}
}

/*!
* \brief This sequence is used to check whether transmitted pkts have
* carrier loss error or not, returns 1 if carrier loss error else returns 0
* \param[in] txdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT tx_carrier_lost_error(t_tx_desc *txdesc)
{
	UINT tdes3;

	/* check tdes3.LoC and tdes3.NC bits */
	TX_NORMAL_DESC_TDES3_RD(txdesc->tdes3, tdes3);
	if (((tdes3 & 0x800) == 0x800) || ((tdes3 & 0x400) == 0x400)) {
		return 1;
	} else {
		return 0;
	}
}

/*!
* \brief This sequence is used to check whether transmission is aborted
* or not returns 1 if transmission is aborted else returns 0
* \param[in] txdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT tx_aborted_error(t_tx_desc *txdesc)
{
	UINT tdes3;

	/* check for tdes3.LC and tdes3.EC */
	TX_NORMAL_DESC_TDES3_RD(txdesc->tdes3, tdes3);
	if (((tdes3 & 0x200) == 0x200) || ((tdes3 & 0x100) == 0x100)) {
		return 1;
	} else {
		return 0;
	}
}

/*!
* \brief This sequence is used to check whether the pkt transmitted is
* successfull or not, returns 1 if transmission is success else returns 0
* \param[in] txdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT tx_complete(t_tx_desc *txdesc)
{
	UINT own;

	TX_NORMAL_DESC_TDES3_OWN_RD(txdesc->tdes3, own);
	if (own == 0) {
		return 1;
	} else {
		return 0;
	}
}

/*!
* \brief This sequence is used to check whethet rx csum is enabled/disabled
* returns 1 if rx csum is enabled else returns 0
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT get_rx_csum_status(void)
{
	ULONG mac_mcr;

	MAC_MCR_RD(mac_mcr);
	if (GET_VALUE(mac_mcr, MAC_MCR_IPC_LPOS, MAC_MCR_IPC_HPOS) == 0x1) {
		return 1;
	} else {
		return 0;
	}
}

/*!
* \brief This sequence is used to disable the rx csum
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT disable_rx_csum(void)
{

	/* enable rx checksum */
	MAC_MCR_IPC_WR(0);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable the rx csum
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT enable_rx_csum(void)
{

	/* enable rx checksum */
	MAC_MCR_IPC_WR(0x1);

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to reinitialize the TX descriptor fields,
* so that device can reuse the descriptors
* \param[in] idx
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT tx_descriptor_reset(UINT idx, struct eqos_prv_data *pdata, UINT qinx)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx, idx);

	DBGPR("-->tx_descriptor_reset\n");

	/* update buffer 1 address pointer to zero */
	TX_NORMAL_DESC_TDES0_WR(ptx_desc->tdes0, 0);
	/* update buffer 2 address pointer to zero */
	TX_NORMAL_DESC_TDES1_WR(ptx_desc->tdes1, 0);
	/* set all other control bits (IC, TTSE, B2L & B1L) to zero */
	TX_NORMAL_DESC_TDES2_WR(ptx_desc->tdes2, 0);
	/* set all other control bits (OWN, CTXT, FD, LD, CPC, CIC etc) to zero */
	TX_NORMAL_DESC_TDES3_WR(ptx_desc->tdes3, 0);

	DBGPR("<--tx_descriptor_reset\n");

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to reinitialize the RX descriptor fields,
* so that device can reuse the descriptors
* \param[in] idx
* \param[in] pdata
*/

static void rx_descriptor_reset(UINT idx,
				struct eqos_prv_data *pdata,
				unsigned int inte, UINT qinx)
{
	struct rx_swcx_desc *prx_swcx_desc = GET_RX_BUF_PTR(qinx, idx);
	struct s_rx_desc *prx_desc = GET_RX_DESC_PTR(qinx, idx);

	DBGPR("-->rx_descriptor_reset\n");

	memset(prx_desc, 0, sizeof(struct s_rx_desc));
	/* update buffer 1 address pointer */
	RX_NORMAL_DESC_RDES0_WR(prx_desc->rdes0, L32(prx_swcx_desc->dma));
	RX_NORMAL_DESC_RDES1_WR(prx_desc->rdes1, H32(prx_swcx_desc->dma));

	if (pdata->dev->mtu > EQOS_ETH_FRAME_LEN) {
		/* update buffer 2 address pointer */
		RX_NORMAL_DESC_RDES2_WR(prx_desc->rdes2, 0);
		/* set control bits - OWN, INTE, BUF1V and BUF2V */
		RX_NORMAL_DESC_RDES3_WR(prx_desc->rdes3,
					(0x83000000 | inte));
	} else {
		/* set buffer 2 address pointer to zero */
		RX_NORMAL_DESC_RDES2_WR(prx_desc->rdes2, 0);
		/* set control bits - OWN, INTE and BUF1V */
		RX_NORMAL_DESC_RDES3_WR(prx_desc->rdes3,
					(0x81000000 | inte));
	}

	DBGPR("<--rx_descriptor_reset\n");
}

/*!
* \brief This sequence is used to initialize the rx descriptors.
* \param[in] pdata
*/

static void rx_descriptor_init(struct eqos_prv_data *pdata, UINT qinx)
{
	struct rx_ring *prx_ring =
	    GET_RX_WRAPPER_DESC(qinx);
	struct rx_swcx_desc *prx_swcx_desc =
	    GET_RX_BUF_PTR(qinx, prx_ring->cur_rx);
	struct s_rx_desc *prx_desc =
	    GET_RX_DESC_PTR(qinx, prx_ring->cur_rx);
	INT i;
	INT start_index = prx_ring->cur_rx;
	INT last_index;

	DBGPR("-->rx_descriptor_init\n");

	/* initialize all desc */

	for (i = 0; i < RX_DESC_CNT; i++) {
		memset(prx_desc, 0, sizeof(struct s_rx_desc));
		/* update buffer 1 address pointer */
		RX_NORMAL_DESC_RDES0_WR(prx_desc->rdes0,
					(L32(prx_swcx_desc->dma)));
		/* set to zero  */
		RX_NORMAL_DESC_RDES1_WR(prx_desc->rdes1,
					(H32(prx_swcx_desc->dma)));

		/* set buffer 2 address pointer to zero */
		RX_NORMAL_DESC_RDES2_WR(prx_desc->rdes2, 0);
		/* set control bits - OWN, INTE and BUF1V */
		RX_NORMAL_DESC_RDES3_WR(prx_desc->rdes3, 0xc1000000);

		prx_swcx_desc->inte = (1 << 30);

		/* reconfigure INTE bit if RX watchdog timer is enabled */
		if (prx_ring->use_riwt) {
			if ((i % prx_ring->rx_coal_frames) != 0) {
				UINT rdes3 = 0;
				RX_NORMAL_DESC_RDES3_RD(prx_desc->rdes3,
							rdes3);
				/* reset INTE */
				RX_NORMAL_DESC_RDES3_WR(prx_desc->rdes3,
							(rdes3 & ~(1 << 30)));
				prx_swcx_desc->inte = 0;
			}
		}

		INCR_RX_DESC_INDEX(prx_ring->cur_rx, 1);
		prx_desc = GET_RX_DESC_PTR(qinx, prx_ring->cur_rx);
		prx_swcx_desc = GET_RX_BUF_PTR(qinx, prx_ring->cur_rx);
	}
	/* update the total no of Rx descriptors count */
	DMA_RDRLR_WR(qinx, (RX_DESC_CNT - 1));
	/* update the Rx Descriptor Tail Pointer */
	last_index = GET_CURRENT_RCVD_LAST_DESC_INDEX(start_index, 0);
	DMA_RDTP_RPDR_WR(qinx, GET_RX_DESC_DMA_ADDR(qinx, last_index));
	/* update the starting address of desc chain/ring */
	DMA_RDLAR_WR(qinx, GET_RX_DESC_DMA_ADDR(qinx, start_index));

	prx_ring->hw_last_rx_desc_addr =
		GET_RX_DESC_DMA_ADDR(qinx, start_index);

	DBGPR("<--rx_descriptor_init\n");
}

/*!
* \brief This sequence is used to initialize the tx descriptors.
* \param[in] pdata
*/

static void tx_descriptor_init(struct eqos_prv_data *pdata, UINT qinx)
{
	struct tx_ring *ptx_ring =
	    GET_TX_WRAPPER_DESC(qinx);
	struct s_tx_desc *ptx_desc =
	    GET_TX_DESC_PTR(qinx, ptx_ring->cur_tx);
	INT i;
	INT start_index = ptx_ring->cur_tx;

	DBGPR("-->tx_descriptor_init\n");

	/* initialze all descriptors. */

	for (i = 0; i < TX_DESC_CNT; i++) {
		/* update buffer 1 address pointer to zero */
		TX_NORMAL_DESC_TDES0_WR(ptx_desc->tdes0, 0);
		/* update buffer 2 address pointer to zero */
		TX_NORMAL_DESC_TDES1_WR(ptx_desc->tdes1, 0);
		/* set all other control bits (IC, TTSE, B2L & B1L) to zero */
		TX_NORMAL_DESC_TDES2_WR(ptx_desc->tdes2, 0);
		/* set all other control bits (OWN, CTXT, FD, LD, CPC, CIC etc) to zero */
		TX_NORMAL_DESC_TDES3_WR(ptx_desc->tdes3, 0);

		INCR_TX_DESC_INDEX(ptx_ring->cur_tx, 1);
		ptx_desc = GET_TX_DESC_PTR(qinx, ptx_ring->cur_tx);
	}
	/* update the total no of Tx descriptors count */
	DMA_TDRLR_WR(qinx, (TX_DESC_CNT - 1));
	/* update the starting address of desc chain/ring */
	DMA_TDLAR_WR(qinx, GET_TX_DESC_DMA_ADDR(qinx, start_index));

	DBGPR("<--tx_descriptor_init\n");
}

/*!
* \brief This sequence is used to prepare tx descriptor for
* packet transmission and issue the poll demand command to TxDMA
*
* \param[in] pdata
*/

static void pre_transmit(struct eqos_prv_data *pdata, UINT qinx)
{
	struct tx_ring *ptx_ring =
	    GET_TX_WRAPPER_DESC(qinx);
	struct tx_swcx_desc *ptx_swcx_desc =
	    GET_TX_BUF_PTR(qinx, ptx_ring->cur_tx);
	struct s_tx_desc *ptx_desc =
	    GET_TX_DESC_PTR(qinx, ptx_ring->cur_tx);
	struct s_tx_desc *plast_desc;
	struct s_tx_context_desc *TX_CONTEXT_DESC =
	    GET_TX_DESC_PTR(qinx, ptx_ring->cur_tx);
	UINT varcsum_enable;
#ifdef EQOS_ENABLE_VLAN_TAG
	UINT varvlan_pkt;
	UINT varvt;
#endif
	INT i;
	INT start_index = ptx_ring->cur_tx;
	INT original_start_index = ptx_ring->cur_tx;
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR(qinx);
	UINT vartso_enable = 0;
	UINT varmss = 0;
	UINT varpay_len = 0;
	UINT vartcp_hdr_len = 0;
	UINT varptp_enable = 0;
	uint desc_cnt = tx_pkt_features->desc_cnt;

	DBGPR("-->pre_transmit: qinx = %u\n", qinx);

#ifdef EQOS_ENABLE_VLAN_TAG
	TX_PKT_FEATURES_PKT_ATTRIBUTES_VLAN_PKT_RD(tx_pkt_features->
						   pkt_attributes, varvlan_pkt);
	if (varvlan_pkt == 0x1) {
		/* put vlan tag in contex descriptor and set other control
		 * bits accordingly */
		TX_PKT_FEATURES_VLAN_TAG_VT_RD(tx_pkt_features->vlan_tag,
					       varvt);
		TX_CONTEXT_DESC_TDES3_VT_WR(TX_CONTEXT_DESC->tdes3, varvt);
		TX_CONTEXT_DESC_TDES3_VLTV_WR(TX_CONTEXT_DESC->tdes3, 0x1);
		TX_CONTEXT_DESC_TDES3_CTXT_WR(TX_CONTEXT_DESC->tdes3, 0x1);
		TX_CONTEXT_DESC_TDES3_OWN_WR(TX_CONTEXT_DESC->tdes3, 0x1);

		original_start_index = ptx_ring->cur_tx;
		INCR_TX_DESC_INDEX(ptx_ring->cur_tx, 1);
		start_index = ptx_ring->cur_tx;
		ptx_desc = GET_TX_DESC_PTR(qinx, ptx_ring->cur_tx);
		ptx_swcx_desc = GET_TX_BUF_PTR(qinx, ptx_ring->cur_tx);
		desc_cnt--;
	}
#endif				/* EQOS_ENABLE_VLAN_TAG */

	/* prepare CONTEXT descriptor for TSO */
	TX_PKT_FEATURES_PKT_ATTRIBUTES_TSO_ENABLE_RD(tx_pkt_features->
						     pkt_attributes,
						     vartso_enable);
	if (vartso_enable) {
		/* get MSS and update */
		TX_PKT_FEATURES_MSS_MSS_RD(tx_pkt_features->mss, varmss);
		TX_CONTEXT_DESC_TDES2_MSS_WR(TX_CONTEXT_DESC->tdes2, varmss);
		/* set MSS valid, CTXT and OWN bits */
		TX_CONTEXT_DESC_TDES3_TCMSSV_WR(TX_CONTEXT_DESC->tdes3, 0x1);
		TX_CONTEXT_DESC_TDES3_CTXT_WR(TX_CONTEXT_DESC->tdes3, 0x1);
		TX_CONTEXT_DESC_TDES3_OWN_WR(TX_CONTEXT_DESC->tdes3, 0x1);

		ptx_ring->default_mss = tx_pkt_features->mss;

		original_start_index = ptx_ring->cur_tx;
		INCR_TX_DESC_INDEX(ptx_ring->cur_tx, 1);
		start_index = ptx_ring->cur_tx;
		ptx_desc = GET_TX_DESC_PTR(qinx, ptx_ring->cur_tx);
		ptx_swcx_desc = GET_TX_BUF_PTR(qinx, ptx_ring->cur_tx);
		desc_cnt--;
	}

	/* update the first buffer pointer and length */
	TX_NORMAL_DESC_TDES0_WR(ptx_desc->tdes0,
				(ptx_swcx_desc->dma) & 0xFFFFFFFF);
	TX_NORMAL_DESC_TDES1_WR(ptx_desc->tdes1,
				(((ptx_swcx_desc->
				   dma) & 0xFFFFFFFF00000000) >> 32) &
				0xFFFFFFFF);
	TX_NORMAL_DESC_TDES2_HL_B1L_WR(ptx_desc->tdes2, ptx_swcx_desc->len);

	/* update TCP payload length (only for the descriptor with FD set) */
	TX_PKT_FEATURES_PAY_LEN_RD(tx_pkt_features->pay_len, varpay_len);
	/* tdes3[17:0] will be TCP payload length */
	ptx_desc->tdes3 |= varpay_len;

#ifdef EQOS_ENABLE_VLAN_TAG
	/* Insert a VLAN tag with a tag value programmed in MAC Reg 24 or
	 * CONTEXT descriptor
	 * */
	if (ptx_ring->vlan_tag_present &&
	    Y_FALSE == ptx_ring->tx_vlan_tag_via_reg) {
		TX_NORMAL_DESC_TDES2_VTIR_WR(ptx_desc->tdes2,
					     ptx_ring->tx_vlan_tag_ctrl);
	}
#endif				/* EQOS_ENABLE_VLAN_TAG */

	/* Mark it as First Descriptor */
	TX_NORMAL_DESC_TDES3_FD_WR(ptx_desc->tdes3, 0x1);
	/* Enable CRC and Pad Insertion (NOTE: set this only
	 * for FIRST descriptor) */
	TX_NORMAL_DESC_TDES3_CPC_WR(ptx_desc->tdes3, 0);
	/* Mark it as NORMAL descriptor */
	TX_NORMAL_DESC_TDES3_CTXT_WR(ptx_desc->tdes3, 0);
	/* Enable HW CSUM */
	TX_PKT_FEATURES_PKT_ATTRIBUTES_CSUM_ENABLE_RD(tx_pkt_features->
						      pkt_attributes,
						      varcsum_enable);
	if (varcsum_enable == 0x1) {
		TX_NORMAL_DESC_TDES3_CIC_WR(ptx_desc->tdes3, 0x3);
	}
	/* configure SA Insertion Control */
	TX_NORMAL_DESC_TDES3_SAIC_WR(ptx_desc->tdes3,
				     pdata->tx_sa_ctrl_via_desc);
	if (vartso_enable) {
		/* set TSE bit */
		TX_NORMAL_DESC_TDES3_TSE_WR(ptx_desc->tdes3, 0x1);

		/* update tcp data offset or tcp hdr len */
		TX_PKT_FEATURES_TCP_HDR_LEN_RD(tx_pkt_features->tcp_hdr_len,
					       vartcp_hdr_len);
		/* convert to bit value */
		vartcp_hdr_len = vartcp_hdr_len / 4;
		TX_NORMAL_DESC_TDES3_SLOTNUM_TCPHDRLEN_WR(ptx_desc->tdes3,
							  vartcp_hdr_len);
	}

	/* enable timestamping */
	TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_RD(tx_pkt_features->
						     pkt_attributes,
						     varptp_enable);
	if (varptp_enable) {
		TX_NORMAL_DESC_TDES2_TTSE_WR(ptx_desc->tdes2, 0x1);
	}

	INCR_TX_DESC_INDEX(ptx_ring->cur_tx, 1);
	plast_desc = ptx_desc;
	ptx_desc = GET_TX_DESC_PTR(qinx, ptx_ring->cur_tx);
	ptx_swcx_desc = GET_TX_BUF_PTR(qinx, ptx_ring->cur_tx);
	desc_cnt--;

	for (i = 0; i < desc_cnt; i++) {
		/* update the first buffer pointer and length */
		TX_NORMAL_DESC_TDES0_WR(ptx_desc->tdes0,
					(ptx_swcx_desc->dma) & 0xFFFFFFFF);
		TX_NORMAL_DESC_TDES1_WR(ptx_desc->tdes1,
					(((ptx_swcx_desc->
					   dma) & 0xFFFFFFFF00000000) >> 32) &
					0xFFFFFFFF);
		TX_NORMAL_DESC_TDES2_HL_B1L_WR(ptx_desc->tdes2,
					       ptx_swcx_desc->len);

		/* set own bit */
		TX_NORMAL_DESC_TDES3_OWN_WR(ptx_desc->tdes3, 0x1);
		/* Mark it as NORMAL descriptor */
		TX_NORMAL_DESC_TDES3_CTXT_WR(ptx_desc->tdes3, 0);

		INCR_TX_DESC_INDEX(ptx_ring->cur_tx, 1);
		plast_desc = ptx_desc;
		ptx_desc = GET_TX_DESC_PTR(qinx, ptx_ring->cur_tx);
		ptx_swcx_desc = GET_TX_BUF_PTR(qinx, ptx_ring->cur_tx);
	}
	/* Mark it as LAST descriptor */
	TX_NORMAL_DESC_TDES3_LD_WR(plast_desc->tdes3, 0x1);

	/* set Interrupt on Completion for last descriptor */
	TX_NORMAL_DESC_TDES2_IC_WR(plast_desc->tdes2, 0x1);

	/* set OWN bit of FIRST descriptor at end to avoid race condition */
	ptx_desc = GET_TX_DESC_PTR(qinx, start_index);
	TX_NORMAL_DESC_TDES3_OWN_WR(ptx_desc->tdes3, 0x1);

#ifdef EQOS_ENABLE_TX_DESC_DUMP
	dump_tx_desc(pdata, original_start_index, (ptx_ring->cur_tx - 1),
		     1, qinx);
#endif

	/* issue a poll command to Tx DMA by writing address
	 * of next immediate free descriptor */
	DMA_TDTP_TPDR_WR(qinx, GET_TX_DESC_DMA_ADDR(qinx, ptx_ring->cur_tx));

	if (pdata->eee_enabled) {
		/* restart EEE timer */
		mod_timer(&pdata->eee_ctrl_timer,
			  EQOS_LPI_TIMER(EQOS_DEFAULT_LPI_TIMER));
	}

	DBGPR("<--pre_transmit\n");
}

static void update_rx_tail_ptr(unsigned int qinx, unsigned int dma_addr)
{
	DMA_RDTP_RPDR_WR(qinx, dma_addr);
}

/*!
* \brief This sequence is used to check whether CTXT bit is
* set or not returns 1 if CTXT is set else returns zero
* \param[in] txdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT get_tx_descriptor_ctxt(t_tx_desc *txdesc)
{
	ULONG ctxt;

	/* check tdes3.CTXT bit */
	TX_NORMAL_DESC_TDES3_CTXT_RD(txdesc->tdes3, ctxt);
	if (ctxt == 1) {
		return 1;
	} else {
		return 0;
	}
}

/*!
* \brief This sequence is used to check whether LD bit is set or not
* returns 1 if LD is set else returns zero
* \param[in] txdesc
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static INT get_tx_descriptor_last(t_tx_desc *txdesc)
{
	ULONG ld;

	/* check tdes3.LD bit */
	TX_NORMAL_DESC_TDES3_LD_RD(txdesc->tdes3, ld);
	if (ld == 1) {
		return 1;
	} else {
		return 0;
	}
}

static void eqos_disable_pad_cal(struct eqos_prv_data *pdata)
{
	u32 hwreg;

	PAD_AUTO_CAL_CFG_RD(hwreg);
	hwreg &= ~PAD_AUTO_CAL_CFG_ENABLE_MASK;
	PAD_AUTO_CAL_CFG_WR(hwreg);
}

static void eqos_read_err_counter(struct eqos_prv_data *pdata, bool save)
{
	u32 val;
	if (save) {
		MMC_TXUNDERFLOWERROR_RD(val);
		pdata->mmc.mmc_tx_underflow_error_pre_recalib += val;
		MMC_TXCARRIERERROR_RD(val);
		pdata->mmc.mmc_tx_carrier_error_pre_recalib += val;
		MMC_TXEXCESSDEF_RD(val);
		pdata->mmc.mmc_tx_excessdef_pre_recalib += val;
		MMC_RXCRCERROR_RD(val);
		pdata->mmc.mmc_rx_crc_errror_pre_recalib += val;
		MMC_RXALIGNMENTERROR_RD(val);
		pdata->mmc.mmc_rx_align_error_pre_recalib += val;
		MMC_RXRUNTERROR_RD(val);
		pdata->mmc.mmc_rx_run_error_pre_recalib += val;
		MMC_RXJABBERERROR_RD(val);
		pdata->mmc.mmc_rx_jabber_error_pre_recalib += val;
		MMC_RXLENGTHERROR_RD(val);
		pdata->mmc.mmc_rx_length_error_pre_recalib += val;
		MMC_RXOUTOFRANGETYPE_RD(val);
		pdata->mmc.mmc_rx_outofrangetype_pre_recalib += val;
		MMC_RXFIFOOVERFLOW_RD(val);
		pdata->mmc.mmc_rx_fifo_overflow_pre_recalib += val;
		MMC_RXWATCHDOGERROR_RD(val);
		pdata->mmc.mmc_rx_watchdog_error_pre_recalib += val;
		MMC_RXRCVERROR_RD(val);
		pdata->mmc.mmc_rx_receive_error_pre_recalib += val;
		MMC_RXIPV4_HDRERR_PKTS_RD(val);
		pdata->mmc.mmc_rx_ipv4_hderr_pre_recalib += val;
		MMC_RXIPV6_HDRERR_PKTS_RD(val);
		pdata->mmc.mmc_rx_ipv6_hderr_octets_pre_recalib += val;
		MMC_RXUDP_ERR_PKTS_RD(val);
		pdata->mmc.mmc_rx_udp_err_pre_recalib += val;
		MMC_RXTCP_ERR_PKTS_RD(val);
		pdata->mmc.mmc_rx_tcp_err_pre_recalib += val;
		MMC_RXICMP_ERR_PKTS_RD(val);
		pdata->mmc.mmc_rx_icmp_err_pre_recalib += val;
		MMC_RXIPV4_HDRERR_OCTETS_RD(val);
		pdata->mmc.mmc_rx_ipv4_hderr_octets_pre_recalib += val;
		MMC_RXIPV6_HDRERR_OCTETS_RD(val);
		pdata->mmc.mmc_rx_ipv6_hderr_pre_recalib += val;
		MMC_RXUDP_ERR_OCTETS_RD(val);
		pdata->mmc.mmc_rx_udp_err_octets_pre_recalib += val;
		MMC_RXTCP_ERR_OCTETS_RD(val);
		pdata->mmc.mmc_rx_tcp_err_octets_pre_recalib += val;
		MMC_RXICMP_ERR_OCTETS_RD(val);
		pdata->mmc.mmc_rx_icmp_err_octets_pre_recalib += val;
	} else {
		MMC_TXUNDERFLOWERROR_RD(val);
		MMC_TXCARRIERERROR_RD(val);
		MMC_TXEXCESSDEF_RD(val);
		MMC_RXCRCERROR_RD(val);
		MMC_RXALIGNMENTERROR_RD(val);
		MMC_RXRUNTERROR_RD(val);
		MMC_RXJABBERERROR_RD(val);
		MMC_RXLENGTHERROR_RD(val);
		MMC_RXOUTOFRANGETYPE_RD(val);
		MMC_RXFIFOOVERFLOW_RD(val);
		MMC_RXWATCHDOGERROR_RD(val);
		MMC_RXRCVERROR_RD(val);
		MMC_RXIPV4_HDRERR_PKTS_RD(val);
		MMC_RXIPV6_HDRERR_PKTS_RD(val);
		MMC_RXUDP_ERR_PKTS_RD(val);
		MMC_RXTCP_ERR_PKTS_RD(val);
		MMC_RXICMP_ERR_PKTS_RD(val);
		MMC_RXIPV4_HDRERR_OCTETS_RD(val);
		MMC_RXIPV6_HDRERR_OCTETS_RD(val);
		MMC_RXUDP_ERR_OCTETS_RD(val);
		MMC_RXTCP_ERR_OCTETS_RD(val);
		MMC_RXICMP_ERR_OCTETS_RD(val);
	}
}

static INT eqos_pad_calibrate(struct eqos_prv_data *pdata)
{
	struct platform_device *pdev = pdata->pdev;
	int ret;
	uint i;
	u32 hwreg;

	if (tegra_platform_is_unit_fpga())
		return 0;

	DBGPR("-->%s()\n", __func__);

	/* 1. Set field PAD_E_INPUT_OR_E_PWRD in
	 * reg ETHER_QOS_SDMEMCOMPPADCTRL_0
	 */
	PAD_CRTL_E_INPUT_OR_E_PWRD_WR(1);

	/* 2. delay for 1 usec */
	usleep_range(1, 3);

	/* 3. Set AUTO_CAL_ENABLE and AUTO_CAL_START in
	 * reg ETHER_QOS_AUTO_CAL_CONFIG_0.
	 */
	PAD_AUTO_CAL_CFG_RD(hwreg);
	hwreg |=
	    ((PAD_AUTO_CAL_CFG_START_MASK) | (PAD_AUTO_CAL_CFG_ENABLE_MASK));

	PAD_AUTO_CAL_CFG_WR(hwreg);

	/* 4. Wait on 1 to 3 us before start checking for calibration done.
	 *    This delay is consumed in delay inside while loop.
	 */

	/* 5. Wait on AUTO_CAL_ACTIVE until it is 0. 10ms is the timeout */
	i = 1000;
	while (i--) {
		usleep_range(10, 12);
		PAD_AUTO_CAL_STAT_RD(hwreg);

		/* calibration done when CAL_STAT_ACTIVE is zero */
		if (!(hwreg & PAD_AUTO_CAL_STAT_ACTIVE_MASK))
			break;
	}
	if (!i) {
		ret = -1;
		dev_err(&pdev->dev,
			"eqos pad calibration took too long to complete\n");
		goto calibration_failed;
	}
	ret = 0;

 calibration_failed:
	/* 6. Disable field PAD_E_INPUT_OR_E_PWRD in
	 * reg ETHER_QOS_SDMEMCOMPPADCTRL_0 to save power.
	 */
	PAD_CRTL_E_INPUT_OR_E_PWRD_WR(0);

	DBGPR("<--%s()\n", __func__);

	return ret;
}

static INT eqos_car_reset(struct eqos_prv_data *pdata)
{
	ULONG retry_cnt = 1000;
	ULONG vy_count;
	volatile ULONG dma_bmr;

	DBGPR("-->eqos_car_reset\n");

	/* Issue a CAR reset */
	if (!IS_ERR_OR_NULL(pdata->eqos_rst))
		reset_control_reset(pdata->eqos_rst);

	/* add delay of 10 usec */
	udelay(10);

	/* Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			dev_err(&pdata->pdev->dev,
				"%s():%d: Timed out polling on DMA_BMR_SWR\n",
				__func__, __LINE__);
			return -Y_FAILURE;
		} else {
			vy_count++;
			mdelay(1);
		}
		DMA_BMR_RD(dma_bmr);
		if (GET_VALUE(dma_bmr, DMA_BMR_SWR_LPOS, DMA_BMR_SWR_HPOS) == 0) {
			break;
		}
	}

	DBGPR("<--eqos_car_reset\n");

	return Y_SUCCESS;
}

/*!
* \brief Exit routine
* \details Exit function that unregisters the device, deallocates buffers,
* unbinds the driver from controlling the device etc.
*
* \return Returns successful execution of the routine
* \retval Y_SUCCESS Function executed successfully
*/

static INT eqos_yexit(void)
{
	ULONG retry_cnt = 1000;
	ULONG vy_count;
	volatile ULONG dma_bmr;
	int i, j;

	DBGPR("-->eqos_yexit\n");

	/* issue a software reset */
	DMA_BMR_SWR_WR(0x1);

	/*DELAY IMPLEMENTATION USING udelay() */
	udelay(10);

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			printk("%s():%d: Timed out polling on DMA_BMR_SWR\n",
			       __func__, __LINE__);
			return -Y_FAILURE;
		} else {
			vy_count++;
			mdelay(1);
		}
		DMA_BMR_RD(dma_bmr);
		if (GET_VALUE(dma_bmr, DMA_BMR_SWR_LPOS, DMA_BMR_SWR_HPOS) == 0) {
			break;
		}
	}

	/* ack and disable all wrapper ints */
	i = (VIRT_INTR_CH_CRTL_RX_WR_MASK | VIRT_INTR_CH_CRTL_TX_WR_MASK);
	for (j = 0; j < MAX_CHANS; j++) {
		VIRT_INTR_CH_CRTL_WR(j, ~i);
		VIRT_INTR_CH_STAT_WR(j, i);
	}

	DBGPR("<--eqos_yexit\n");

	return Y_SUCCESS;
}

/*!
* \details This API will calculate burst size given
*           queue FIFO size.
*
* \param[in] fifo_size - total fifo size in h/w register
*
* \return returns integer
* \retval - value of pbl
*/
static UINT calculate_dma_pbl(ULONG p_fifo)
{
	UINT pbl = 0;

	/* given fifo size, need to ensure burst is never larger than half
	 * fifo size.
	 * ex: for fifo of 4kb, plb=16 and pblx8=1 results in 2kb burst (16*8*16)
	 * pbl is in granulatiries of 16 bytes
	 */
	switch (p_fifo) {
	case EQOS_32K:
	case EQOS_16K:
	case EQOS_8K:
		/* this is max which can be specified */
		pbl = 32;
		break;
	case EQOS_4K:
		pbl = 16;
		break;
	case EQOS_2K:
		pbl = 8;
		break;
	case EQOS_1K:
		pbl = 4;
		break;
	case EQOS_512:
		pbl = 2;
		break;
	case EQOS_256:
		pbl = 1;
		break;
	default:
		pr_err("%s(): Invalid Fifo Size specified (0x%.8x)\n", __func__,
		       (UINT) p_fifo);
		break;
	}

	return pbl;
}

/*!
* \details This API will calculate per queue FIFO size.
*
* \param[in] fifo_size - total fifo size in h/w register
* \param[in] queue_count - total queue count
*
* \return returns integer
* \retval - fifo size per queue.
*/
static UINT calculate_per_queue_fifo(ULONG fifo_size, UCHAR queue_count)
{
	ULONG q_fifo_size = 0;	/* calculated fifo size per queue */
	ULONG p_fifo = EQOS_256;	/* per queue fifo size programmable value */

	/* calculate Tx/Rx fifo share per queue */
	switch (fifo_size) {
	case 0:
		q_fifo_size = FIFO_SIZE_B(128);
		break;
	case 1:
		q_fifo_size = FIFO_SIZE_B(256);
		break;
	case 2:
		q_fifo_size = FIFO_SIZE_B(512);
		break;
	case 3:
		q_fifo_size = FIFO_SIZE_KB(1);
		break;
	case 4:
		q_fifo_size = FIFO_SIZE_KB(2);
		break;
	case 5:
		q_fifo_size = FIFO_SIZE_KB(4);
		break;
	case 6:
		q_fifo_size = FIFO_SIZE_KB(8);
		break;
	case 7:
		q_fifo_size = FIFO_SIZE_KB(16);
		break;
	case 8:
		q_fifo_size = FIFO_SIZE_KB(32);
		break;
	case 9:
		q_fifo_size = FIFO_SIZE_KB(64);
		break;
	case 10:
		q_fifo_size = FIFO_SIZE_KB(128);
		break;
	case 11:
		q_fifo_size = FIFO_SIZE_KB(256);
		break;
	}

	q_fifo_size = q_fifo_size / queue_count;

	if (q_fifo_size >= FIFO_SIZE_KB(32)) {
		p_fifo = EQOS_32K;
	} else if (q_fifo_size >= FIFO_SIZE_KB(16)) {
		p_fifo = EQOS_16K;
	} else if (q_fifo_size >= FIFO_SIZE_KB(8)) {
		p_fifo = EQOS_8K;
	} else if (q_fifo_size >= FIFO_SIZE_KB(4)) {
		p_fifo = EQOS_4K;
	} else if (q_fifo_size >= FIFO_SIZE_KB(2)) {
		p_fifo = EQOS_2K;
	} else if (q_fifo_size >= FIFO_SIZE_KB(1)) {
		p_fifo = EQOS_1K;
	} else if (q_fifo_size >= FIFO_SIZE_B(512)) {
		p_fifo = EQOS_512;
	} else if (q_fifo_size >= FIFO_SIZE_B(256)) {
		p_fifo = EQOS_256;
	}

	return p_fifo;
}

static INT configure_mtl_queue(UINT qinx, struct eqos_prv_data *pdata)
{
	struct eqos_tx_queue *queue_data = GET_TX_QUEUE_PTR(qinx);
	ULONG retry_cnt = 1000;
	ULONG vy_count;
	volatile ULONG mtl_qtomr;
	UINT p_rx_fifo = EQOS_256, p_tx_fifo = EQOS_256;
	uint i;

	DBGPR("-->configure_mtl_queue\n");

	/*Flush Tx Queue */
	MTL_QTOMR_FTQ_WR(qinx, 0x1);

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count > retry_cnt) {
			return -Y_FAILURE;
		} else {
			vy_count++;
			mdelay(1);
		}
		MTL_QTOMR_RD(qinx, mtl_qtomr);
		if (GET_VALUE(mtl_qtomr, MTL_QTOMR_FTQ_LPOS, MTL_QTOMR_FTQ_HPOS)
		    == 0) {
			break;
		}
	}

	/*Enable Store and Forward mode for TX */
	MTL_QTOMR_TSF_WR(qinx, 0x1);
	/* Program Tx operating mode */
	MTL_QTOMR_TXQEN_WR(qinx, queue_data->q_op_mode);
	/* Transmit Queue weight */
	MTL_QW_ISCQW_WR(qinx, (0x10 + qinx));

	/* Configure for Jumbo frame in MTL */
	i = 1;
	if (pdata->dev->mtu > EQOS_ETH_FRAME_LEN) {
		/* Disable RX Store and Forward mode */
		i = 0;
	}
	MTL_QROMR_RSF_WR(qinx, i);

	p_rx_fifo =
	    calculate_per_queue_fifo(pdata->hw_feat.rx_fifo_size,
				     EQOS_RX_QUEUE_CNT);
	p_tx_fifo =
	    calculate_per_queue_fifo(pdata->hw_feat.tx_fifo_size,
				     EQOS_TX_QUEUE_CNT);

	/* Transmit/Receive queue fifo size programmed */
	MTL_QROMR_RQS_WR(qinx, p_rx_fifo);
	MTL_QTOMR_TQS_WR(qinx, p_tx_fifo);

	/* flow control will be used only if
	 * each channel gets 8KB or more fifo */
	if (p_rx_fifo >= EQOS_4K) {
		/* Enable Rx FLOW CTRL in MTL and MAC
		   Programming is valid only if Rx fifo size is greater than
		   or equal to 8k */
		if ((pdata->flow_ctrl & EQOS_FLOW_CTRL_TX) == EQOS_FLOW_CTRL_TX) {

			MTL_QROMR_EHFC_WR(qinx, 0x1);

#ifdef EQOS_VER_4_0
			if (p_rx_fifo == EQOS_4K) {
				/* This violates the above formula because of FIFO size limit
				 * therefore overflow may occur inspite of this
				 * */
				MTL_QROMR_RFD_WR(qinx, 0x2);
				MTL_QROMR_RFA_WR(qinx, 0x1);
			} else if (p_rx_fifo == EQOS_8K) {
				MTL_QROMR_RFD_WR(qinx, 0x4);
				MTL_QROMR_RFA_WR(qinx, 0x2);
			} else if (p_rx_fifo == EQOS_16K) {
				MTL_QROMR_RFD_WR(qinx, 0x5);
				MTL_QROMR_RFA_WR(qinx, 0x2);
			} else if (p_rx_fifo == EQOS_32K) {
				MTL_QROMR_RFD_WR(qinx, 0x7);
				MTL_QROMR_RFA_WR(qinx, 0x2);
			}
#else
			/* Set Threshold for Activating Flow Contol space for min 2 frames
			 * ie, (1500 * 1) = 1500 bytes
			 *
			 * Set Threshold for Deactivating Flow Contol for space of
			 * min 1 frame (frame size 1500bytes) in receive fifo */
			if (p_rx_fifo == EQOS_4K) {
				/* This violates the above formula because of FIFO size limit
				 * therefore overflow may occur inspite of this
				 * */
				MTL_QROMR_RFD_WR(qinx, 0x3);	/* Full-3K */
				MTL_QROMR_RFA_WR(qinx, 0x1);	/* Full-1.5K */
			} else if (p_rx_fifo == EQOS_8K) {
				MTL_QROMR_RFD_WR(qinx, 0x6);	/* Full-4K */
				MTL_QROMR_RFA_WR(qinx, 0xA);	/* Full-6K */
			} else if (p_rx_fifo == EQOS_16K) {
				MTL_QROMR_RFD_WR(qinx, 0x6);	/* Full-4K */
				MTL_QROMR_RFA_WR(qinx, 0x12);	/* Full-10K */
			} else if (p_rx_fifo == EQOS_32K) {
				MTL_QROMR_RFD_WR(qinx, 0x6);	/* Full-4K */
				MTL_QROMR_RFA_WR(qinx, 0x1E);	/* Full-16K */
			}
#endif
		}
	}

	DBGPR("<--configure_mtl_queue\n");

	return Y_SUCCESS;
}

static INT configure_dma_channel(UINT qinx, struct eqos_prv_data *pdata)
{
	struct rx_ring *prx_ring =
	    GET_RX_WRAPPER_DESC(qinx);
	ULONG p_fifo, pbl;
	uint rx_buf_size;

	DBGPR("-->configure_dma_channel\n");

	/*Enable OSF mode */
	DMA_TCR_OSP_WR(qinx, 0x1);

	/* Select Rx Buffer size.  Needs to be rounded up to next multiple of
	 * bus width
	 */
	rx_buf_size = ((pdata->rx_max_frame_size + (AXI_BUS_WIDTH - 1)) &
		       ~(AXI_BUS_WIDTH - 1));
	DMA_RCR_RBSZ_WR(qinx, rx_buf_size);

	/* program RX watchdog timer */
	if (prx_ring->use_riwt)
		DMA_RIWTR_RWT_WR(qinx, prx_ring->rx_riwt);
	else
		DMA_RIWTR_RWT_WR(qinx, 0);

	enable_dma_interrupts(qinx, pdata);
	/* set PBLX8 */
	DMA_CR_PBLX8_WR(qinx, 0x1);

	p_fifo =
	    calculate_per_queue_fifo(pdata->hw_feat.tx_fifo_size,
				     EQOS_TX_QUEUE_CNT);
	pbl = calculate_dma_pbl(p_fifo);
	DMA_TCR_PBL_WR(qinx, pbl);

	p_fifo =
	    calculate_per_queue_fifo(pdata->hw_feat.rx_fifo_size,
				     EQOS_RX_QUEUE_CNT);
	DMA_RCR_PBL_WR(qinx, min(RXPBL, MAX_RXPBL));

	/* To get Best Performance */
	DMA_SBUS_BLEN16_WR(1);
	DMA_SBUS_BLEN8_WR(1);
	DMA_SBUS_BLEN4_WR(1);
	DMA_SBUS_RD_OSR_LMT_WR(2);
	DMA_SBUS_EAME_WR(1);

	/* enable TSO if HW supports */
	if (pdata->hw_feat.tso_en)
		DMA_TCR_TSE_WR(qinx, 0x1);

	/* Add 10us delay as per bug 1611959 */
	udelay(10);

	/* start TX DMA */
	DMA_TCR_ST_WR(qinx, 0x1);
	/* start RX DMA */
	DMA_RCR_ST_WR(qinx, 0x1);

	DBGPR("<--configure_dma_channel\n");

	return Y_SUCCESS;
}

/*!
* \brief This sequence is used to enable MAC interrupts
* \return Success or Failure
* \retval  0 Success
* \retval -1 Failure
*/

static int enable_mac_interrupts(void)
{
	unsigned long varmac_imr;

	/* Enable following interrupts */
	/* RGSMIIIM - RGMII/SMII interrupt Enable */
	/* PCSLCHGIM -  PCS Link Status Interrupt Enable */
	/* PCSANCIM - PCS AN Completion Interrupt Enable */
	/* PMTIM - PMT Interrupt Enable */
	/* LPIIM - LPI Interrupt Enable */
	MAC_IMR_RD(varmac_imr);
	varmac_imr = varmac_imr & (unsigned long)(0x1008);
	varmac_imr = varmac_imr | ((0x1) << 0) | ((0x1) << 1) | ((0x1) << 2) |
	    ((0x1) << 4) | ((0x1) << 5);
	MAC_IMR_WR(varmac_imr);

	return Y_SUCCESS;
}

static INT configure_mac(struct eqos_prv_data *pdata)
{
	struct eqos_cfg *pdt_cfg = (struct eqos_cfg *)&pdata->dt_cfg;
	ULONG mac_mcr;
	UINT qinx;

	DBGPR("-->configure_mac\n");

	for (qinx = 0; qinx < EQOS_RX_QUEUE_CNT; qinx++) {
		MAC_RQC0R_RXQEN_WR(qinx, pdata->dt_cfg.rxq_ctrl[qinx] & 0x3);
	}

	/* Set Tx flow control parameters */
	for (qinx = 0; qinx < EQOS_TX_QUEUE_CNT; qinx++) {
		/* set Pause Time */
		MAC_QTFCR_PT_WR(qinx, 0xffff);
		/* Assign priority for RX flow control */
		/* Assign priority for TX flow control */
		switch (qinx) {
		case 0:
			MAC_TQPM0R_PSTQ0_WR(0);
			MAC_RQC2R_PSRQ0_WR(0x1 << pdt_cfg->q_prio[qinx]);
			break;
		case 1:
			MAC_TQPM0R_PSTQ1_WR(1);
			MAC_RQC2R_PSRQ1_WR(0x1 << pdt_cfg->q_prio[qinx]);
			break;
		case 2:
			MAC_TQPM0R_PSTQ2_WR(2);
			MAC_RQC2R_PSRQ2_WR(0x1 << pdt_cfg->q_prio[qinx]);
			break;
		case 3:
			MAC_TQPM0R_PSTQ3_WR(3);
			MAC_RQC2R_PSRQ3_WR(0x1 << pdt_cfg->q_prio[qinx]);
			break;
		}

		if (pdata->dt_cfg.pause_frames == PAUSE_FRAMES_ENABLED) {
			if ((pdata->flow_ctrl & EQOS_FLOW_CTRL_TX)
			    == EQOS_FLOW_CTRL_TX)
				enable_tx_flow_ctrl(qinx);
			else
				disable_tx_flow_ctrl(qinx);
		}
	}

	/* Set Rx flow control parameters */
	if (pdata->dt_cfg.pause_frames == PAUSE_FRAMES_ENABLED) {
		if ((pdata->flow_ctrl & EQOS_FLOW_CTRL_RX) == EQOS_FLOW_CTRL_RX)
			enable_rx_flow_ctrl();
		else
			disable_rx_flow_ctrl();
	}

	if (pdata->dev->mtu > EQOS_ETH_FRAME_LEN) {
		/* Configure for Jumbo frame in MAC */
		if (pdata->dev->mtu <= EQOS_MAX_GPSL) {
			MAC_MCR_JE_WR(0x1);
			MAC_MCR_WD_WR(0x0);
			MAC_MCR_GPSLCE_WR(0x0);
			MAC_MCR_JD_WR(0x0);
		} else {
			MAC_MCR_JE_WR(0x0);
			MAC_MCR_WD_WR(0x1);
			MAC_MCR_GPSLCE_WR(0x1);
			MAC_MECR_GPSL_WR(EQOS_MAX_SUPPORTED_MTU);
			MAC_MCR_JD_WR(0x1);
		}
	} else {
		MAC_MCR_JE_WR(0x0);
		MAC_MCR_WD_WR(0x0);
		MAC_MCR_GPSLCE_WR(0x0);
		MAC_MCR_JD_WR(0x0);
	}

	/* PLSEN is set to 1 so that LPI is not initiated */
	MAC_LPS_PLSEN_WR(1);

	/* update the MAC address */
	MAC_MA0HR_WR(((pdata->dev->dev_addr[5] << 8) |
		      (pdata->dev->dev_addr[4])));
	MAC_MA0LR_WR(((pdata->dev->dev_addr[3] << 24) |
		      (pdata->dev->dev_addr[2] << 16) |
		      (pdata->dev->dev_addr[1] << 8) |
		      (pdata->dev->dev_addr[0])));

	/* Enable MAC Transmit process */
	/* Enable MAC Receive process */
	/* Enable padding - disabled */
	/* Enable CRC stripping - disabled */
	/* Enable disable receive own */
	MAC_MCR_RD(mac_mcr);
	mac_mcr = mac_mcr & (ULONG) (0xffcfff7c);
	mac_mcr = mac_mcr | ((0x1) << 0) | ((0x1) << 20) | ((0x1) << 21);
	mac_mcr |= ((0x1) << 1);

	/* if multi channels are enabled, default duplex
	 * is full
	 */
	if (pdata->dt_cfg.use_multi_q)
		mac_mcr |= ((0x1) << 13);

	MAC_MCR_WR(mac_mcr);

	if (pdata->hw_feat.rx_coe_sel &&
	    ((pdata->dev_state & NETIF_F_RXCSUM) == NETIF_F_RXCSUM))
		MAC_MCR_IPC_WR(0x1);

#ifdef EQOS_ENABLE_VLAN_TAG
	configure_mac_for_vlan_pkt();
	if (pdata->hw_feat.vlan_hash_en)
		config_vlan_filtering(1, 1, 0);
#endif

	if (pdata->hw_feat.mmc_sel) {
		/* disable all MMC intterrupt as MMC are managed in SW and
		 * registers are cleared on each READ eventually
		 * */
		disable_mmc_interrupts();
		config_mmc_counters();
	}

	enable_mac_interrupts();

	DBGPR("<--configure_mac\n");

	return Y_SUCCESS;
}

/*!
* \brief Initialises device registers.
* \details This function initialises device registers.
*
* \return none
*/

static INT eqos_yinit(struct eqos_prv_data *pdata)
{
	UINT qinx;
	int i, j;

	DBGPR("-->eqos_yinit\n");

	/* reset mmc counters */
	MMC_CNTRL_WR(0x1);

	for (qinx = 0; qinx < EQOS_TX_QUEUE_CNT; qinx++) {
		configure_mtl_queue(qinx, pdata);
	}
	/* Mapping MTL Rx queue and DMA Rx channel */
	MTL_RQDCM0R_WR(0x3020100);
	MTL_RQDCM1R_WR(0x7060504);

	i = (VIRT_INTR_CH_CRTL_RX_WR_MASK | VIRT_INTR_CH_CRTL_TX_WR_MASK);
	for (j = 0; j < pdata->num_chans; j++) {

		VIRT_INTR_CH_STAT_WR(j, i);
		VIRT_INTR_CH_CRTL_WR(j, pdata->chinfo[j].int_mask);
	}
	configure_mac(pdata);

	/* Setting INCRx */
	DMA_SBUS_WR(0x0);
	for (qinx = 0; qinx < EQOS_TX_QUEUE_CNT; qinx++) {
		configure_dma_channel(qinx, pdata);
	}

	DBGPR("<--eqos_yinit\n");

	return Y_SUCCESS;
}

/*!
* \brief API to initialize the function pointers.
*
* \details This function is called in probe to initialize all the
* function pointers which are used in other functions to capture
* the different device features.
*
* \param[in] hw_if - pointer to hw_if_struct structure.
*
* \return void.
*/

void eqos_init_function_ptrs_dev(struct hw_if_struct *hw_if)
{

	DBGPR("-->eqos_init_function_ptrs_dev\n");

	hw_if->tx_complete = tx_complete;
	hw_if->tx_window_error = NULL;
	hw_if->tx_aborted_error = tx_aborted_error;
	hw_if->tx_carrier_lost_error = tx_carrier_lost_error;
	hw_if->tx_fifo_underrun = tx_fifo_underrun;
	hw_if->tx_get_collision_count = NULL;
	hw_if->tx_handle_aborted_error = NULL;
	hw_if->tx_update_fifo_threshold = NULL;
	hw_if->tx_config_threshold = NULL;

	hw_if->set_promiscuous_mode = set_promiscuous_mode;
	hw_if->set_all_multicast_mode = set_all_multicast_mode;
	hw_if->set_multicast_list_mode = set_multicast_list_mode;
	hw_if->set_unicast_mode = set_unicast_mode;

	hw_if->enable_rx_csum = enable_rx_csum;
	hw_if->disable_rx_csum = disable_rx_csum;
	hw_if->get_rx_csum_status = get_rx_csum_status;

	hw_if->write_phy_regs = write_phy_regs;
	hw_if->read_phy_regs = read_phy_regs;
	hw_if->set_full_duplex = set_full_duplex;
	hw_if->set_half_duplex = set_half_duplex;
	hw_if->set_mii_speed_100 = set_mii_speed_100;
	hw_if->set_mii_speed_10 = set_mii_speed_10;
	hw_if->set_gmii_speed = set_gmii_speed;
	hw_if->set_tx_clk_speed = set_tx_clk_speed;
	/* for PMT */
	hw_if->start_dma_rx = start_dma_rx;
	hw_if->stop_dma_rx = stop_dma_rx;
	hw_if->start_dma_tx = start_dma_tx;
	hw_if->stop_dma_tx = stop_dma_tx;
	hw_if->start_mac_tx_rx = start_mac_tx_rx;
	hw_if->stop_mac_tx = stop_mac_tx;
	hw_if->stop_mac_rx = stop_mac_rx;

	hw_if->pre_xmit = pre_transmit;
	hw_if->init = eqos_yinit;
	hw_if->exit = eqos_yexit;
	hw_if->car_reset = eqos_car_reset;
	hw_if->pad_calibrate = eqos_pad_calibrate;
	hw_if->disable_pad_cal = eqos_disable_pad_cal;
	hw_if->read_err_counter = eqos_read_err_counter;
	/* Descriptor related Sequences have to be initialized here */
	hw_if->tx_desc_init = tx_descriptor_init;
	hw_if->rx_desc_init = rx_descriptor_init;
	hw_if->rx_desc_reset = rx_descriptor_reset;
	hw_if->tx_desc_reset = tx_descriptor_reset;
	hw_if->get_tx_desc_ls = get_tx_descriptor_last;
	hw_if->get_tx_desc_ctxt = get_tx_descriptor_ctxt;
	hw_if->update_rx_tail_ptr = update_rx_tail_ptr;

	/* for FLOW ctrl */
	hw_if->enable_rx_flow_ctrl = enable_rx_flow_ctrl;
	hw_if->disable_rx_flow_ctrl = disable_rx_flow_ctrl;
	hw_if->enable_tx_flow_ctrl = enable_tx_flow_ctrl;
	hw_if->disable_tx_flow_ctrl = disable_tx_flow_ctrl;

	/* for PMT operation */
	hw_if->enable_magic_pmt = enable_magic_pmt_operation;
	hw_if->disable_magic_pmt = disable_magic_pmt_operation;
	hw_if->enable_remote_pmt = enable_remote_pmt_operation;
	hw_if->disable_remote_pmt = disable_remote_pmt_operation;
	hw_if->configure_rwk_filter = configure_rwk_filter_registers;

	/* for TX vlan control */
	hw_if->enable_vlan_reg_control = configure_reg_vlan_control;
	hw_if->enable_vlan_desc_control = configure_desc_vlan_control;

	/* for rx vlan stripping */
	hw_if->config_rx_outer_vlan_stripping = config_rx_outer_vlan_stripping;
	hw_if->config_rx_inner_vlan_stripping = config_rx_inner_vlan_stripping;

	/* for sa(source address) insert/replace */
	hw_if->configure_mac_addr0_reg = configure_mac_addr0_reg;
	hw_if->configure_mac_addr1_reg = configure_mac_addr1_reg;
	hw_if->configure_sa_via_reg = configure_sa_via_reg;

	/* for RX watchdog timer */
	hw_if->config_rx_watchdog = config_rx_watchdog_timer;

	/* for RX and TX threshold config */
	hw_if->config_rx_threshold = config_rx_threshold;
	hw_if->config_tx_threshold = config_tx_threshold;

	/* for RX and TX Store and Forward Mode config */
	hw_if->config_rsf_mode = config_rsf_mode;
	hw_if->config_tsf_mode = config_tsf_mode;

	/* for TX DMA Operating on Second Frame config */
	hw_if->config_osf_mode = config_osf_mode;

	/* for INCR/INCRX config */
	hw_if->config_incr_incrx_mode = config_incr_incrx_mode;
	/* for AXI PBL config */
	hw_if->config_axi_pbl_val = config_axi_pbl_val;
	/* for AXI WORL config */
	hw_if->config_axi_worl_val = config_axi_worl_val;
	/* for AXI RORL config */
	hw_if->config_axi_rorl_val = config_axi_rorl_val;

	/* for RX and TX PBL config */
	hw_if->config_rx_pbl_val = config_rx_pbl_val;
	hw_if->get_rx_pbl_val = get_rx_pbl_val;
	hw_if->config_tx_pbl_val = config_tx_pbl_val;
	hw_if->get_tx_pbl_val = get_tx_pbl_val;
	hw_if->config_pblx8 = config_pblx8;

	hw_if->disable_rx_interrupt = disable_rx_interrupt;
	hw_if->enable_rx_interrupt = enable_rx_interrupt;
	hw_if->disable_chan_interrupts = disable_chan_interrupts;
	hw_if->enable_chan_interrupts = enable_chan_interrupts;

	/* for handling MMC */
	hw_if->disable_mmc_interrupts = disable_mmc_interrupts;
	hw_if->config_mmc_counters = config_mmc_counters;

	hw_if->set_dcb_algorithm = set_dcb_algorithm;
	hw_if->set_dcb_queue_weight = set_dcb_queue_weight;

	hw_if->set_tx_queue_operating_mode = set_tx_queue_operating_mode;
	hw_if->set_avb_algorithm = set_avb_algorithm;
	hw_if->config_credit_control = config_credit_control;
	hw_if->config_send_slope = config_send_slope;
	hw_if->config_idle_slope = config_idle_slope;
	hw_if->config_high_credit = config_high_credit;
	hw_if->config_low_credit = config_low_credit;
	hw_if->config_slot_num_check = config_slot_num_check;
	hw_if->config_advance_slot_num_check = config_advance_slot_num_check;

	/* for hw time stamping */
	hw_if->config_hw_time_stamping = config_hw_time_stamping;
	hw_if->config_sub_second_increment = config_sub_second_increment;
	hw_if->init_systime = init_systime;
	hw_if->config_addend = config_addend;
	hw_if->adjust_systime = adjust_systime;
	hw_if->get_systime = get_systime;
	hw_if->get_tx_tstamp_status = get_tx_tstamp_status;
	hw_if->get_tx_tstamp = get_tx_tstamp;
	hw_if->get_tx_tstamp_status_via_reg = get_tx_tstamp_status_via_reg;
	hw_if->get_tx_tstamp_via_reg = get_tx_tstamp_via_reg;
	hw_if->rx_tstamp_available = rx_tstamp_available;
	hw_if->get_rx_tstamp_status = get_rx_tstamp_status;
	hw_if->get_rx_tstamp = get_rx_tstamp;
	hw_if->drop_tx_status_enabled = drop_tx_status_enabled;

	/* for l3 and l4 layer filtering */
	hw_if->config_l2_da_perfect_inverse_match =
	    config_l2_da_perfect_inverse_match;
	hw_if->update_mac_addr32_127_low_high_reg =
	    update_mac_addr32_127_low_high_reg;
	hw_if->update_mac_addr1_31_low_high_reg =
	    update_mac_addr1_31_low_high_reg;
	hw_if->update_hash_table_reg = update_hash_table_reg;
	hw_if->config_mac_pkt_filter_reg = config_mac_pkt_filter_reg;
	hw_if->config_l3_l4_filter_enable = config_l3_l4_filter_enable;
	hw_if->config_l3_filters = config_l3_filters;
	hw_if->update_ip4_addr0 = update_ip4_addr0;
	hw_if->update_ip4_addr1 = update_ip4_addr1;
	hw_if->update_ip6_addr = update_ip6_addr;
	hw_if->config_l4_filters = config_l4_filters;
	hw_if->update_l4_sa_port_no = update_l4_sa_port_no;
	hw_if->update_l4_da_port_no = update_l4_da_port_no;

	/* for VLAN filtering */
	hw_if->get_vlan_hash_table_reg = get_vlan_hash_table_reg;
	hw_if->update_vlan_hash_table_reg = update_vlan_hash_table_reg;
	hw_if->update_vlan_id = update_vlan_id;
	hw_if->config_vlan_filtering = config_vlan_filtering;
#ifdef EQOS_ENABLE_VLAN_TAG
	hw_if->config_mac_for_vlan_pkt = configure_mac_for_vlan_pkt;
#endif
	hw_if->get_vlan_tag_comparison = get_vlan_tag_comparison;

	/* for differnet PHY interconnect */
	hw_if->control_an = control_an;
	hw_if->get_an_adv_pause_param = get_an_adv_pause_param;
	hw_if->get_an_adv_duplex_param = get_an_adv_duplex_param;
	hw_if->get_lp_an_adv_pause_param = get_lp_an_adv_pause_param;
	hw_if->get_lp_an_adv_duplex_param = get_lp_an_adv_duplex_param;

	/* for EEE */
	hw_if->set_eee_mode = set_eee_mode;
	hw_if->reset_eee_mode = reset_eee_mode;
	hw_if->set_eee_pls = set_eee_pls;
	hw_if->set_eee_timer = set_eee_timer;
	hw_if->get_lpi_status = get_lpi_status;
	hw_if->set_lpi_tx_automate = set_lpi_tx_automate;

	/* for ARP */
	hw_if->config_arp_offload = config_arp_offload;
	hw_if->update_arp_offload_ip_addr = update_arp_offload_ip_addr;

	/* for MAC loopback */
	hw_if->config_mac_loopback_mode = config_mac_loopback_mode;

	/* for PFC */
	hw_if->config_pfc = config_pfc;

	/* for MAC Double VLAN Processing config */
	hw_if->config_rx_outer_vlan_stripping = config_rx_outer_vlan_stripping;
	hw_if->config_rx_inner_vlan_stripping = config_rx_inner_vlan_stripping;

	/* for PTP Offloading */
	hw_if->config_ptpoffload_engine = config_ptpoffload_engine;

	/*for PTP channel routingi */
	hw_if->config_ptp_channel = config_ptp_channel;

	DBGPR("<--eqos_init_function_ptrs_dev\n");
}
