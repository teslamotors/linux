/*
 * DWC Ether QOS controller driver for Samsung TRAV SoCs
 *
 * Copyright (C) Synopsys India Pvt. Ltd.
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 * 
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/netdevice.h>
#include "dwc_eth_qos_yheader.h"

extern unsigned long dwc_eqos_base_addr;

#include "dwc_eth_qos_yregacc.h"

#define DEBUGFS_MAX_SIZE 100
static char debugfs_buf[DEBUGFS_MAX_SIZE];

struct dwc_eqos_prv_data *pdata;

/*
 * This structure hold information about the /debug file
 */
static struct dentry *dir;

/* Variables used to store the register value. */
static unsigned int registers_val;
static unsigned int mac_ma32_127lr127_val;
static unsigned int mac_ma32_127lr126_val;
static unsigned int mac_ma32_127lr125_val;
static unsigned int mac_ma32_127lr124_val;
static unsigned int mac_ma32_127lr123_val;
static unsigned int mac_ma32_127lr122_val;
static unsigned int mac_ma32_127lr121_val;
static unsigned int mac_ma32_127lr120_val;
static unsigned int mac_ma32_127lr119_val;
static unsigned int mac_ma32_127lr118_val;
static unsigned int mac_ma32_127lr117_val;
static unsigned int mac_ma32_127lr116_val;
static unsigned int mac_ma32_127lr115_val;
static unsigned int mac_ma32_127lr114_val;
static unsigned int mac_ma32_127lr113_val;
static unsigned int mac_ma32_127lr112_val;
static unsigned int mac_ma32_127lr111_val;
static unsigned int mac_ma32_127lr110_val;
static unsigned int mac_ma32_127lr109_val;
static unsigned int mac_ma32_127lr108_val;
static unsigned int mac_ma32_127lr107_val;
static unsigned int mac_ma32_127lr106_val;
static unsigned int mac_ma32_127lr105_val;
static unsigned int mac_ma32_127lr104_val;
static unsigned int mac_ma32_127lr103_val;
static unsigned int mac_ma32_127lr102_val;
static unsigned int mac_ma32_127lr101_val;
static unsigned int mac_ma32_127lr100_val;
static unsigned int mac_ma32_127lr99_val;
static unsigned int mac_ma32_127lr98_val;
static unsigned int mac_ma32_127lr97_val;
static unsigned int mac_ma32_127lr96_val;
static unsigned int mac_ma32_127lr95_val;
static unsigned int mac_ma32_127lr94_val;
static unsigned int mac_ma32_127lr93_val;
static unsigned int mac_ma32_127lr92_val;
static unsigned int mac_ma32_127lr91_val;
static unsigned int mac_ma32_127lr90_val;
static unsigned int mac_ma32_127lr89_val;
static unsigned int mac_ma32_127lr88_val;
static unsigned int mac_ma32_127lr87_val;
static unsigned int mac_ma32_127lr86_val;
static unsigned int mac_ma32_127lr85_val;
static unsigned int mac_ma32_127lr84_val;
static unsigned int mac_ma32_127lr83_val;
static unsigned int mac_ma32_127lr82_val;
static unsigned int mac_ma32_127lr81_val;
static unsigned int mac_ma32_127lr80_val;
static unsigned int mac_ma32_127lr79_val;
static unsigned int mac_ma32_127lr78_val;
static unsigned int mac_ma32_127lr77_val;
static unsigned int mac_ma32_127lr76_val;
static unsigned int mac_ma32_127lr75_val;
static unsigned int mac_ma32_127lr74_val;
static unsigned int mac_ma32_127lr73_val;
static unsigned int mac_ma32_127lr72_val;
static unsigned int mac_ma32_127lr71_val;
static unsigned int mac_ma32_127lr70_val;
static unsigned int mac_ma32_127lr69_val;
static unsigned int mac_ma32_127lr68_val;
static unsigned int mac_ma32_127lr67_val;
static unsigned int mac_ma32_127lr66_val;
static unsigned int mac_ma32_127lr65_val;
static unsigned int mac_ma32_127lr64_val;
static unsigned int mac_ma32_127lr63_val;
static unsigned int mac_ma32_127lr62_val;
static unsigned int mac_ma32_127lr61_val;
static unsigned int mac_ma32_127lr60_val;
static unsigned int mac_ma32_127lr59_val;
static unsigned int mac_ma32_127lr58_val;
static unsigned int mac_ma32_127lr57_val;
static unsigned int mac_ma32_127lr56_val;
static unsigned int mac_ma32_127lr55_val;
static unsigned int mac_ma32_127lr54_val;
static unsigned int mac_ma32_127lr53_val;
static unsigned int mac_ma32_127lr52_val;
static unsigned int mac_ma32_127lr51_val;
static unsigned int mac_ma32_127lr50_val;
static unsigned int mac_ma32_127lr49_val;
static unsigned int mac_ma32_127lr48_val;
static unsigned int mac_ma32_127lr47_val;
static unsigned int mac_ma32_127lr46_val;
static unsigned int mac_ma32_127lr45_val;
static unsigned int mac_ma32_127lr44_val;
static unsigned int mac_ma32_127lr43_val;
static unsigned int mac_ma32_127lr42_val;
static unsigned int mac_ma32_127lr41_val;
static unsigned int mac_ma32_127lr40_val;
static unsigned int mac_ma32_127lr39_val;
static unsigned int mac_ma32_127lr38_val;
static unsigned int mac_ma32_127lr37_val;
static unsigned int mac_ma32_127lr36_val;
static unsigned int mac_ma32_127lr35_val;
static unsigned int mac_ma32_127lr34_val;
static unsigned int mac_ma32_127lr33_val;
static unsigned int mac_ma32_127lr32_val;
static unsigned int mac_ma32_127hr127_val;
static unsigned int mac_ma32_127hr126_val;
static unsigned int mac_ma32_127hr125_val;
static unsigned int mac_ma32_127hr124_val;
static unsigned int mac_ma32_127hr123_val;
static unsigned int mac_ma32_127hr122_val;
static unsigned int mac_ma32_127hr121_val;
static unsigned int mac_ma32_127hr120_val;
static unsigned int mac_ma32_127hr119_val;
static unsigned int mac_ma32_127hr118_val;
static unsigned int mac_ma32_127hr117_val;
static unsigned int mac_ma32_127hr116_val;
static unsigned int mac_ma32_127hr115_val;
static unsigned int mac_ma32_127hr114_val;
static unsigned int mac_ma32_127hr113_val;
static unsigned int mac_ma32_127hr112_val;
static unsigned int mac_ma32_127hr111_val;
static unsigned int mac_ma32_127hr110_val;
static unsigned int mac_ma32_127hr109_val;
static unsigned int mac_ma32_127hr108_val;
static unsigned int mac_ma32_127hr107_val;
static unsigned int mac_ma32_127hr106_val;
static unsigned int mac_ma32_127hr105_val;
static unsigned int mac_ma32_127hr104_val;
static unsigned int mac_ma32_127hr103_val;
static unsigned int mac_ma32_127hr102_val;
static unsigned int mac_ma32_127hr101_val;
static unsigned int mac_ma32_127hr100_val;
static unsigned int mac_ma32_127hr99_val;
static unsigned int mac_ma32_127hr98_val;
static unsigned int mac_ma32_127hr97_val;
static unsigned int mac_ma32_127hr96_val;
static unsigned int mac_ma32_127hr95_val;
static unsigned int mac_ma32_127hr94_val;
static unsigned int mac_ma32_127hr93_val;
static unsigned int mac_ma32_127hr92_val;
static unsigned int mac_ma32_127hr91_val;
static unsigned int mac_ma32_127hr90_val;
static unsigned int mac_ma32_127hr89_val;
static unsigned int mac_ma32_127hr88_val;
static unsigned int mac_ma32_127hr87_val;
static unsigned int mac_ma32_127hr86_val;
static unsigned int mac_ma32_127hr85_val;
static unsigned int mac_ma32_127hr84_val;
static unsigned int mac_ma32_127hr83_val;
static unsigned int mac_ma32_127hr82_val;
static unsigned int mac_ma32_127hr81_val;
static unsigned int mac_ma32_127hr80_val;
static unsigned int mac_ma32_127hr79_val;
static unsigned int mac_ma32_127hr78_val;
static unsigned int mac_ma32_127hr77_val;
static unsigned int mac_ma32_127hr76_val;
static unsigned int mac_ma32_127hr75_val;
static unsigned int mac_ma32_127hr74_val;
static unsigned int mac_ma32_127hr73_val;
static unsigned int mac_ma32_127hr72_val;
static unsigned int mac_ma32_127hr71_val;
static unsigned int mac_ma32_127hr70_val;
static unsigned int mac_ma32_127hr69_val;
static unsigned int mac_ma32_127hr68_val;
static unsigned int mac_ma32_127hr67_val;
static unsigned int mac_ma32_127hr66_val;
static unsigned int mac_ma32_127hr65_val;
static unsigned int mac_ma32_127hr64_val;
static unsigned int mac_ma32_127hr63_val;
static unsigned int mac_ma32_127hr62_val;
static unsigned int mac_ma32_127hr61_val;
static unsigned int mac_ma32_127hr60_val;
static unsigned int mac_ma32_127hr59_val;
static unsigned int mac_ma32_127hr58_val;
static unsigned int mac_ma32_127hr57_val;
static unsigned int mac_ma32_127hr56_val;
static unsigned int mac_ma32_127hr55_val;
static unsigned int mac_ma32_127hr54_val;
static unsigned int mac_ma32_127hr53_val;
static unsigned int mac_ma32_127hr52_val;
static unsigned int mac_ma32_127hr51_val;
static unsigned int mac_ma32_127hr50_val;
static unsigned int mac_ma32_127hr49_val;
static unsigned int mac_ma32_127hr48_val;
static unsigned int mac_ma32_127hr47_val;
static unsigned int mac_ma32_127hr46_val;
static unsigned int mac_ma32_127hr45_val;
static unsigned int mac_ma32_127hr44_val;
static unsigned int mac_ma32_127hr43_val;
static unsigned int mac_ma32_127hr42_val;
static unsigned int mac_ma32_127hr41_val;
static unsigned int mac_ma32_127hr40_val;
static unsigned int mac_ma32_127hr39_val;
static unsigned int mac_ma32_127hr38_val;
static unsigned int mac_ma32_127hr37_val;
static unsigned int mac_ma32_127hr36_val;
static unsigned int mac_ma32_127hr35_val;
static unsigned int mac_ma32_127hr34_val;
static unsigned int mac_ma32_127hr33_val;
static unsigned int mac_ma32_127hr32_val;
static unsigned int mac_ma1_31lr31_val;
static unsigned int mac_ma1_31lr30_val;
static unsigned int mac_ma1_31lr29_val;
static unsigned int mac_ma1_31lr28_val;
static unsigned int mac_ma1_31lr27_val;
static unsigned int mac_ma1_31lr26_val;
static unsigned int mac_ma1_31lr25_val;
static unsigned int mac_ma1_31lr24_val;
static unsigned int mac_ma1_31lr23_val;
static unsigned int mac_ma1_31lr22_val;
static unsigned int mac_ma1_31lr21_val;
static unsigned int mac_ma1_31lr20_val;
static unsigned int mac_ma1_31lr19_val;
static unsigned int mac_ma1_31lr18_val;
static unsigned int mac_ma1_31lr17_val;
static unsigned int mac_ma1_31lr16_val;
static unsigned int mac_ma1_31lr15_val;
static unsigned int mac_ma1_31lr14_val;
static unsigned int mac_ma1_31lr13_val;
static unsigned int mac_ma1_31lr12_val;
static unsigned int mac_ma1_31lr11_val;
static unsigned int mac_ma1_31lr10_val;
static unsigned int mac_ma1_31lr9_val;
static unsigned int mac_ma1_31lr8_val;
static unsigned int mac_ma1_31lr7_val;
static unsigned int mac_ma1_31lr6_val;
static unsigned int mac_ma1_31lr5_val;
static unsigned int mac_ma1_31lr4_val;
static unsigned int mac_ma1_31lr3_val;
static unsigned int mac_ma1_31lr2_val;
static unsigned int mac_ma1_31lr1_val;
static unsigned int mac_ma1_31hr31_val;
static unsigned int mac_ma1_31hr30_val;
static unsigned int mac_ma1_31hr29_val;
static unsigned int mac_ma1_31hr28_val;
static unsigned int mac_ma1_31hr27_val;
static unsigned int mac_ma1_31hr26_val;
static unsigned int mac_ma1_31hr25_val;
static unsigned int mac_ma1_31hr24_val;
static unsigned int mac_ma1_31hr23_val;
static unsigned int mac_ma1_31hr22_val;
static unsigned int mac_ma1_31hr21_val;
static unsigned int mac_ma1_31hr20_val;
static unsigned int mac_ma1_31hr19_val;
static unsigned int mac_ma1_31hr18_val;
static unsigned int mac_ma1_31hr17_val;
static unsigned int mac_ma1_31hr16_val;
static unsigned int mac_ma1_31hr15_val;
static unsigned int mac_ma1_31hr14_val;
static unsigned int mac_ma1_31hr13_val;
static unsigned int mac_ma1_31hr12_val;
static unsigned int mac_ma1_31hr11_val;
static unsigned int mac_ma1_31hr10_val;
static unsigned int mac_ma1_31hr9_val;
static unsigned int mac_ma1_31hr8_val;
static unsigned int mac_ma1_31hr7_val;
static unsigned int mac_ma1_31hr6_val;
static unsigned int mac_ma1_31hr5_val;
static unsigned int mac_ma1_31hr4_val;
static unsigned int mac_ma1_31hr3_val;
static unsigned int mac_ma1_31hr2_val;
static unsigned int mac_ma1_31hr1_val;
static unsigned int mac_arpa_val;
static unsigned int mac_l3a3r7_val;
static unsigned int mac_l3a3r6_val;
static unsigned int mac_l3a3r5_val;
static unsigned int mac_l3a3r4_val;
static unsigned int mac_l3a3r3_val;
static unsigned int mac_l3a3r2_val;
static unsigned int mac_l3a3r1_val;
static unsigned int mac_l3a3r0_val;
static unsigned int mac_l3a2r7_val;
static unsigned int mac_l3a2r6_val;
static unsigned int mac_l3a2r5_val;
static unsigned int mac_l3a2r4_val;
static unsigned int mac_l3a2r3_val;
static unsigned int mac_l3a2r2_val;
static unsigned int mac_l3a2r1_val;
static unsigned int mac_l3a2r0_val;
static unsigned int mac_l3a1r7_val;
static unsigned int mac_l3a1r6_val;
static unsigned int mac_l3a1r5_val;
static unsigned int mac_l3a1r4_val;
static unsigned int mac_l3a1r3_val;
static unsigned int mac_l3a1r2_val;
static unsigned int mac_l3a1r1_val;
static unsigned int mac_l3a1r0_val;
static unsigned int mac_l3a0r7_val;
static unsigned int mac_l3a0r6_val;
static unsigned int mac_l3a0r5_val;
static unsigned int mac_l3a0r4_val;
static unsigned int mac_l3a0r3_val;
static unsigned int mac_l3a0r2_val;
static unsigned int mac_l3a0r1_val;
static unsigned int mac_l3a0r0_val;
static unsigned int mac_l4ar7_val;
static unsigned int mac_l4ar6_val;
static unsigned int mac_l4ar5_val;
static unsigned int mac_l4ar4_val;
static unsigned int mac_l4ar3_val;
static unsigned int mac_l4ar2_val;
static unsigned int mac_l4ar1_val;
static unsigned int mac_l4ar0_val;
static unsigned int mac_l3l4cr7_val;
static unsigned int mac_l3l4cr6_val;
static unsigned int mac_l3l4cr5_val;
static unsigned int mac_l3l4cr4_val;
static unsigned int mac_l3l4cr3_val;
static unsigned int mac_l3l4cr2_val;
static unsigned int mac_l3l4cr1_val;
static unsigned int mac_l3l4cr0_val;
static unsigned int mac_gpio_val;
static unsigned int mac_pcs_val;
static unsigned int mac_tes_val;
static unsigned int mac_ae_val;
static unsigned int mac_alpa_val;
static unsigned int mac_aad_val;
static unsigned int mac_ans_val;
static unsigned int mac_anc_val;
static unsigned int mac_lpc_val;
static unsigned int mac_lps_val;
static unsigned int mac_lmir_val;
static unsigned int mac_spi2r_val;
static unsigned int mac_spi1r_val;
static unsigned int mac_spi0r_val;
static unsigned int mac_pto_cr_val;
static unsigned int mac_pps_width3_val;
static unsigned int mac_pps_width2_val;
static unsigned int mac_pps_width1_val;
static unsigned int mac_pps_width0_val;
static unsigned int mac_pps_intval3_val;
static unsigned int mac_pps_intval2_val;
static unsigned int mac_pps_intval1_val;
static unsigned int mac_pps_intval0_val;
static unsigned int mac_pps_ttns3_val;
static unsigned int mac_pps_ttns2_val;
static unsigned int mac_pps_ttns1_val;
static unsigned int mac_pps_ttns0_val;
static unsigned int mac_pps_tts3_val;
static unsigned int mac_pps_tts2_val;
static unsigned int mac_pps_tts1_val;
static unsigned int mac_pps_tts0_val;
static unsigned int mac_ppsc_val;
static unsigned int mac_teac_val;
static unsigned int mac_tiac_val;
static unsigned int mac_ats_val;
static unsigned int mac_atn_val;
static unsigned int mac_ac_val;
static unsigned int mac_ttn_val;
static unsigned int mac_ttsn_val;
static unsigned int mac_tsr_val;
static unsigned int mac_sthwr_val;
static unsigned int mac_tar_val;
static unsigned int mac_stnsur_val;
static unsigned int mac_stsur_val;
static unsigned int mac_stnsr_val;
static unsigned int mac_stsr_val;
static unsigned int mac_ssir_val;
static unsigned int mac_tcr_val;
static unsigned int mtl_dsr_val;
static unsigned int mac_rwpffr_val;
static unsigned int mac_rtsr_val;
static unsigned int mtl_ier_val;
static unsigned int mtl_qrcr7_val;
static unsigned int mtl_qrcr6_val;
static unsigned int mtl_qrcr5_val;
static unsigned int mtl_qrcr4_val;
static unsigned int mtl_qrcr3_val;
static unsigned int mtl_qrcr2_val;
static unsigned int mtl_qrcr1_val;
static unsigned int mtl_qrdr7_val;
static unsigned int mtl_qrdr6_val;
static unsigned int mtl_qrdr5_val;
static unsigned int mtl_qrdr4_val;
static unsigned int mtl_qrdr3_val;
static unsigned int mtl_qrdr2_val;
static unsigned int mtl_qrdr1_val;
static unsigned int mtl_qocr7_val;
static unsigned int mtl_qocr6_val;
static unsigned int mtl_qocr5_val;
static unsigned int mtl_qocr4_val;
static unsigned int mtl_qocr3_val;
static unsigned int mtl_qocr2_val;
static unsigned int mtl_qocr1_val;
static unsigned int mtl_qromr7_val;
static unsigned int mtl_qromr6_val;
static unsigned int mtl_qromr5_val;
static unsigned int mtl_qromr4_val;
static unsigned int mtl_qromr3_val;
static unsigned int mtl_qromr2_val;
static unsigned int mtl_qromr1_val;
static unsigned int mtl_qlcr7_val;
static unsigned int mtl_qlcr6_val;
static unsigned int mtl_qlcr5_val;
static unsigned int mtl_qlcr4_val;
static unsigned int mtl_qlcr3_val;
static unsigned int mtl_qlcr2_val;
static unsigned int mtl_qlcr1_val;
static unsigned int mtl_qhcr7_val;
static unsigned int mtl_qhcr6_val;
static unsigned int mtl_qhcr5_val;
static unsigned int mtl_qhcr4_val;
static unsigned int mtl_qhcr3_val;
static unsigned int mtl_qhcr2_val;
static unsigned int mtl_qhcr1_val;
static unsigned int mtl_qsscr7_val;
static unsigned int mtl_qsscr6_val;
static unsigned int mtl_qsscr5_val;
static unsigned int mtl_qsscr4_val;
static unsigned int mtl_qsscr3_val;
static unsigned int mtl_qsscr2_val;
static unsigned int mtl_qsscr1_val;
static unsigned int mtl_qw7_val;
static unsigned int mtl_qw6_val;
static unsigned int mtl_qw5_val;
static unsigned int mtl_qw4_val;
static unsigned int mtl_qw3_val;
static unsigned int mtl_qw2_val;
static unsigned int mtl_qw1_val;
static unsigned int mtl_qesr7_val;
static unsigned int mtl_qesr6_val;
static unsigned int mtl_qesr5_val;
static unsigned int mtl_qesr4_val;
static unsigned int mtl_qesr3_val;
static unsigned int mtl_qesr2_val;
static unsigned int mtl_qesr1_val;
static unsigned int mtl_qecr7_val;
static unsigned int mtl_qecr6_val;
static unsigned int mtl_qecr5_val;
static unsigned int mtl_qecr4_val;
static unsigned int mtl_qecr3_val;
static unsigned int mtl_qecr2_val;
static unsigned int mtl_qecr1_val;
static unsigned int mtl_qtdr7_val;
static unsigned int mtl_qtdr6_val;
static unsigned int mtl_qtdr5_val;
static unsigned int mtl_qtdr4_val;
static unsigned int mtl_qtdr3_val;
static unsigned int mtl_qtdr2_val;
static unsigned int mtl_qtdr1_val;
static unsigned int mtl_qucr7_val;
static unsigned int mtl_qucr6_val;
static unsigned int mtl_qucr5_val;
static unsigned int mtl_qucr4_val;
static unsigned int mtl_qucr3_val;
static unsigned int mtl_qucr2_val;
static unsigned int mtl_qucr1_val;
static unsigned int mtl_qtomr7_val;
static unsigned int mtl_qtomr6_val;
static unsigned int mtl_qtomr5_val;
static unsigned int mtl_qtomr4_val;
static unsigned int mtl_qtomr3_val;
static unsigned int mtl_qtomr2_val;
static unsigned int mtl_qtomr1_val;
static unsigned int mac_pmtcsr_val;
static unsigned int mmc_rxicmp_err_octets_val;
static unsigned int mmc_rxicmp_gd_octets_val;
static unsigned int mmc_rxtcp_err_octets_val;
static unsigned int mmc_rxtcp_gd_octets_val;
static unsigned int mmc_rxudp_err_octets_val;
static unsigned int mmc_rxudp_gd_octets_val;
static unsigned int mmc_rxipv6_nopay_octets_val;
static unsigned int mmc_rxipv6_hdrerr_octets_val;
static unsigned int mmc_rxipv6_gd_octets_val;
static unsigned int mmc_rxipv4_udsbl_octets_val;
static unsigned int mmc_rxipv4_frag_octets_val;
static unsigned int mmc_rxipv4_nopay_octets_val;
static unsigned int mmc_rxipv4_hdrerr_octets_val;
static unsigned int mmc_rxipv4_gd_octets_val;
static unsigned int mmc_rxicmp_err_pkts_val;
static unsigned int mmc_rxicmp_gd_pkts_val;
static unsigned int mmc_rxtcp_err_pkts_val;
static unsigned int mmc_rxtcp_gd_pkts_val;
static unsigned int mmc_rxudp_err_pkts_val;
static unsigned int mmc_rxudp_gd_pkts_val;
static unsigned int mmc_rxipv6_nopay_pkts_val;
static unsigned int mmc_rxipv6_hdrerr_pkts_val;
static unsigned int mmc_rxipv6_gd_pkts_val;
static unsigned int mmc_rxipv4_ubsbl_pkts_val;
static unsigned int mmc_rxipv4_frag_pkts_val;
static unsigned int mmc_rxipv4_nopay_pkts_val;
static unsigned int mmc_rxipv4_hdrerr_pkts_val;
static unsigned int mmc_rxipv4_gd_pkts_val;
static unsigned int mmc_rxctrlpackets_g_val;
static unsigned int mmc_rxrcverror_val;
static unsigned int mmc_rxwatchdogerror_val;
static unsigned int mmc_rxvlanpackets_gb_val;
static unsigned int mmc_rxfifooverflow_val;
static unsigned int mmc_rxpausepackets_val;
static unsigned int mmc_rxoutofrangetype_val;
static unsigned int mmc_rxlengtherror_val;
static unsigned int mmc_rxunicastpackets_g_val;
static unsigned int mmc_rx1024tomaxoctets_gb_val;
static unsigned int mmc_rx512to1023octets_gb_val;
static unsigned int mmc_rx256to511octets_gb_val;
static unsigned int mmc_rx128to255octets_gb_val;
static unsigned int mmc_rx65to127octets_gb_val;
static unsigned int mmc_rx64octets_gb_val;
static unsigned int mmc_rxoversize_g_val;
static unsigned int mmc_rxundersize_g_val;
static unsigned int mmc_rxjabbererror_val;
static unsigned int mmc_rxrunterror_val;
static unsigned int mmc_rxalignmenterror_val;
static unsigned int mmc_rxcrcerror_val;
static unsigned int mmc_rxmulticastpackets_g_val;
static unsigned int mmc_rxbroadcastpackets_g_val;
static unsigned int mmc_rxoctetcount_g_val;
static unsigned int mmc_rxoctetcount_gb_val;
static unsigned int mmc_rxpacketcount_gb_val;
static unsigned int mmc_txoversize_g_val;
static unsigned int mmc_txvlanpackets_g_val;
static unsigned int mmc_txpausepackets_val;
static unsigned int mmc_txexcessdef_val;
static unsigned int mmc_txpacketscount_g_val;
static unsigned int mmc_txoctetcount_g_val;
static unsigned int mmc_txcarriererror_val;
static unsigned int mmc_txexesscol_val;
static unsigned int mmc_txlatecol_val;
static unsigned int mmc_txdeferred_val;
static unsigned int mmc_txmulticol_g_val;
static unsigned int mmc_txsinglecol_g_val;
static unsigned int mmc_txunderflowerror_val;
static unsigned int mmc_txbroadcastpackets_gb_val;
static unsigned int mmc_txmulticastpackets_gb_val;
static unsigned int mmc_txunicastpackets_gb_val;
static unsigned int mmc_tx1024tomaxoctets_gb_val;
static unsigned int mmc_tx512to1023octets_gb_val;
static unsigned int mmc_tx256to511octets_gb_val;
static unsigned int mmc_tx128to255octets_gb_val;
static unsigned int mmc_tx65to127octets_gb_val;
static unsigned int mmc_tx64octets_gb_val;
static unsigned int mmc_txmulticastpackets_g_val;
static unsigned int mmc_txbroadcastpackets_g_val;
static unsigned int mmc_txpacketcount_gb_val;
static unsigned int mmc_txoctetcount_gb_val;
static unsigned int mmc_ipc_intr_rx_val;
static unsigned int mmc_ipc_intr_mask_rx_val;
static unsigned int mmc_intr_mask_tx_val;
static unsigned int mmc_intr_mask_rx_val;
static unsigned int mmc_intr_tx_val;
static unsigned int mmc_intr_rx_val;
static unsigned int mmc_cntrl_val;
static unsigned int mac_ma1lr_val;
static unsigned int mac_ma1hr_val;
static unsigned int mac_ma0lr_val;
static unsigned int mac_ma0hr_val;
static unsigned int mac_gpior_val;
static unsigned int mac_gmiidr_val;
static unsigned int mac_gmiiar_val;
static unsigned int mac_hfr2_val;
static unsigned int mac_hfr1_val;
static unsigned int mac_hfr0_val;
static unsigned int mac_mdr_val;
static unsigned int mac_vr_val;
static unsigned int mac_htr7_val;
static unsigned int mac_htr6_val;
static unsigned int mac_htr5_val;
static unsigned int mac_htr4_val;
static unsigned int mac_htr3_val;
static unsigned int mac_htr2_val;
static unsigned int mac_htr1_val;
static unsigned int mac_htr0_val;
static unsigned int dma_riwtr7_val;
static unsigned int dma_riwtr6_val;
static unsigned int dma_riwtr5_val;
static unsigned int dma_riwtr4_val;
static unsigned int dma_riwtr3_val;
static unsigned int dma_riwtr2_val;
static unsigned int dma_riwtr1_val;
static unsigned int dma_riwtr0_val;
static unsigned int dma_rdrlr7_val;
static unsigned int dma_rdrlr6_val;
static unsigned int dma_rdrlr5_val;
static unsigned int dma_rdrlr4_val;
static unsigned int dma_rdrlr3_val;
static unsigned int dma_rdrlr2_val;
static unsigned int dma_rdrlr1_val;
static unsigned int dma_rdrlr0_val;
static unsigned int dma_tdrlr7_val;
static unsigned int dma_tdrlr6_val;
static unsigned int dma_tdrlr5_val;
static unsigned int dma_tdrlr4_val;
static unsigned int dma_tdrlr3_val;
static unsigned int dma_tdrlr2_val;
static unsigned int dma_tdrlr1_val;
static unsigned int dma_tdrlr0_val;
static unsigned int dma_rdtp_rpdr7_val;
static unsigned int dma_rdtp_rpdr6_val;
static unsigned int dma_rdtp_rpdr5_val;
static unsigned int dma_rdtp_rpdr4_val;
static unsigned int dma_rdtp_rpdr3_val;
static unsigned int dma_rdtp_rpdr2_val;
static unsigned int dma_rdtp_rpdr1_val;
static unsigned int dma_rdtp_rpdr0_val;
static unsigned int dma_tdtp_tpdr7_val;
static unsigned int dma_tdtp_tpdr6_val;
static unsigned int dma_tdtp_tpdr5_val;
static unsigned int dma_tdtp_tpdr4_val;
static unsigned int dma_tdtp_tpdr3_val;
static unsigned int dma_tdtp_tpdr2_val;
static unsigned int dma_tdtp_tpdr1_val;
static unsigned int dma_tdtp_tpdr0_val;
static unsigned int dma_rdlar7_val;
static unsigned int dma_rdlar6_val;
static unsigned int dma_rdlar5_val;
static unsigned int dma_rdlar4_val;
static unsigned int dma_rdlar3_val;
static unsigned int dma_rdlar2_val;
static unsigned int dma_rdlar1_val;
static unsigned int dma_rdlar0_val;
static unsigned int dma_tdlar7_val;
static unsigned int dma_tdlar6_val;
static unsigned int dma_tdlar5_val;
static unsigned int dma_tdlar4_val;
static unsigned int dma_tdlar3_val;
static unsigned int dma_tdlar2_val;
static unsigned int dma_tdlar1_val;
static unsigned int dma_tdlar0_val;
static unsigned int dma_ier7_val;
static unsigned int dma_ier6_val;
static unsigned int dma_ier5_val;
static unsigned int dma_ier4_val;
static unsigned int dma_ier3_val;
static unsigned int dma_ier2_val;
static unsigned int dma_ier1_val;
static unsigned int dma_ier0_val;
static unsigned int mac_imr_val;
static unsigned int mac_isr_val;
static unsigned int mtl_isr_val;
static unsigned int dma_sr7_val;
static unsigned int dma_sr6_val;
static unsigned int dma_sr5_val;
static unsigned int dma_sr4_val;
static unsigned int dma_sr3_val;
static unsigned int dma_sr2_val;
static unsigned int dma_sr1_val;
static unsigned int dma_sr0_val;
static unsigned int dma_isr_val;
static unsigned int dma_dsr2_val;
static unsigned int dma_dsr1_val;
static unsigned int dma_dsr0_val;
static unsigned int mtl_q0rdr_val;
static unsigned int mtl_q0esr_val;
static unsigned int mtl_q0tdr_val;
static unsigned int dma_chrbar7_val;
static unsigned int dma_chrbar6_val;
static unsigned int dma_chrbar5_val;
static unsigned int dma_chrbar4_val;
static unsigned int dma_chrbar3_val;
static unsigned int dma_chrbar2_val;
static unsigned int dma_chrbar1_val;
static unsigned int dma_chrbar0_val;
static unsigned int dma_chtbar7_val;
static unsigned int dma_chtbar6_val;
static unsigned int dma_chtbar5_val;
static unsigned int dma_chtbar4_val;
static unsigned int dma_chtbar3_val;
static unsigned int dma_chtbar2_val;
static unsigned int dma_chtbar1_val;
static unsigned int dma_chtbar0_val;
static unsigned int dma_chrdr7_val;
static unsigned int dma_chrdr6_val;
static unsigned int dma_chrdr5_val;
static unsigned int dma_chrdr4_val;
static unsigned int dma_chrdr3_val;
static unsigned int dma_chrdr2_val;
static unsigned int dma_chrdr1_val;
static unsigned int dma_chrdr0_val;
static unsigned int dma_chtdr7_val;
static unsigned int dma_chtdr6_val;
static unsigned int dma_chtdr5_val;
static unsigned int dma_chtdr4_val;
static unsigned int dma_chtdr3_val;
static unsigned int dma_chtdr2_val;
static unsigned int dma_chtdr1_val;
static unsigned int dma_chtdr0_val;
static unsigned int dma_sfcsr7_val;
static unsigned int dma_sfcsr6_val;
static unsigned int dma_sfcsr5_val;
static unsigned int dma_sfcsr4_val;
static unsigned int dma_sfcsr3_val;
static unsigned int dma_sfcsr2_val;
static unsigned int dma_sfcsr1_val;
static unsigned int dma_sfcsr0_val;
static unsigned int mac_ivlantirr_val;
static unsigned int mac_vlantirr_val;
static unsigned int mac_vlanhtr_val;
static unsigned int mac_vlantr_val;
static unsigned int dma_sbus_val;
static unsigned int dma_bmr_val;
static unsigned int mtl_q0rcr_val;
static unsigned int mtl_q0ocr_val;
static unsigned int mtl_q0romr_val;
static unsigned int mtl_q0qr_val;
static unsigned int mtl_q0ecr_val;
static unsigned int mtl_q0ucr_val;
static unsigned int mtl_q0tomr_val;
static unsigned int mtl_rqdcm1r_val;
static unsigned int mtl_rqdcm0r_val;
static unsigned int mtl_fddr_val;
static unsigned int mtl_fdacs_val;
static unsigned int mtl_omr_val;
static unsigned int mac_rqc3r_val;
static unsigned int mac_rqc2r_val;
static unsigned int mac_rqc1r_val;
static unsigned int mac_rqc0r_val;
static unsigned int mac_tqpm1r_val;
static unsigned int mac_tqpm0r_val;
static unsigned int mac_rfcr_val;
static unsigned int mac_qtfcr7_val;
static unsigned int mac_qtfcr6_val;
static unsigned int mac_qtfcr5_val;
static unsigned int mac_qtfcr4_val;
static unsigned int mac_qtfcr3_val;
static unsigned int mac_qtfcr2_val;
static unsigned int mac_qtfcr1_val;
static unsigned int mac_q0tfcr_val;
static unsigned int dma_axi4cr7_val;
static unsigned int dma_axi4cr6_val;
static unsigned int dma_axi4cr5_val;
static unsigned int dma_axi4cr4_val;
static unsigned int dma_axi4cr3_val;
static unsigned int dma_axi4cr2_val;
static unsigned int dma_axi4cr1_val;
static unsigned int dma_axi4cr0_val;
static unsigned int dma_rcr7_val;
static unsigned int dma_rcr6_val;
static unsigned int dma_rcr5_val;
static unsigned int dma_rcr4_val;
static unsigned int dma_rcr3_val;
static unsigned int dma_rcr2_val;
static unsigned int dma_rcr1_val;
static unsigned int dma_rcr0_val;
static unsigned int dma_tcr7_val;
static unsigned int dma_tcr6_val;
static unsigned int dma_tcr5_val;
static unsigned int dma_tcr4_val;
static unsigned int dma_tcr3_val;
static unsigned int dma_tcr2_val;
static unsigned int dma_tcr1_val;
static unsigned int dma_tcr0_val;
static unsigned int dma_cr7_val;
static unsigned int dma_cr6_val;
static unsigned int dma_cr5_val;
static unsigned int dma_cr4_val;
static unsigned int dma_cr3_val;
static unsigned int dma_cr2_val;
static unsigned int dma_cr1_val;
static unsigned int dma_cr0_val;
static unsigned int mac_wtr_val;
static unsigned int mac_mpfr_val;
static unsigned int mac_mecr_val;
static unsigned int mac_mcr_val;

/* For MII/GMII register read/write */
static unsigned int mac_bmcr_reg_val;
static unsigned int mac_bmsr_reg_val;
static unsigned int mii_physid1_reg_val;
static unsigned int mii_physid2_reg_val;
static unsigned int mii_advertise_reg_val;
static unsigned int mii_lpa_reg_val;
static unsigned int mii_expansion_reg_val;
static unsigned int auto_nego_np_reg_val;
static unsigned int mii_estatus_reg_val;
static unsigned int mii_ctrl1000_reg_val;
static unsigned int mii_stat1000_reg_val;
static unsigned int phy_ctl_reg_val;
static unsigned int phy_sts_reg_val;

/* For controlling properties/features of the device */
static unsigned int feature_drop_tx_pktburstcnt_val = 1;

/* for mq */
static unsigned int qInx_val;

void dwc_eqos_get_pdata(struct dwc_eqos_prv_data *prv_pdata)
{
	pdata = prv_pdata;
}

/*!
* \brief API extract the reg name from the buffer.
*
* \details This function extracts the register name from the command line 
* argument passed to the file belonging to type debugfs.
* 
* @param[in]  regname: An empty char pointer.
* @param[in]  buffer: It contains command line input reg name & value.
* @param[in]  buffer_size: buffer size. 
* 
* \return  void
*/

void get_reg_name(char *regname, char *buffer, unsigned long buffer_size)
{
	int i = 0, j = 0;
	unsigned long cnt = buffer_size;

	DBGPR("--> get_reg_name\n");

	while (cnt > 0) {
		if ((buffer[j] == '\t') || (buffer[j]) == ' ') {
			break;
		} else {
			regname[i] = buffer[j];
			i++;
		}
		j++;
		cnt--;
	}
	regname[i] = '\0';

	DBGPR("<-- get_reg_name\n");
	return;
}

/*!
* \brief API extract the reg value from the buffer.
*
* \details This function extracts the value to be written into the register 
* from the command line argument passed to the debug file.
* 
* @param[in]  value: An empty char pointer.
* @param[in]  buffer: It contains command line input reg name & value.
* @param[in]  buffer_size: buffer size. 
* 
* \return  void
*/

void get_reg_value(char *value, char *buffer, unsigned long buffer_size)
{
	int i = 0, j = 0;
	int cnt = buffer_size;
	bool value_present = 0;

	DBGPR("--> get_reg_value\n");

	while (cnt) {
		if ((buffer[j] == ' ') || (buffer[j] == '\t')) {
			value_present = 1;
		}

		if (value_present == 1 && (buffer[j] != ' ')
		    && (buffer[j] != '\t')) {
			value[i] = buffer[j];
			i++;
		}
		j++;
		cnt--;
	}
	value[i] = '\0';

	DBGPR("<-- get_reg_value\n");
	return;
}

/*! 
*  \brief  API to write the reg value to specified register. 
*
* \details This function extracts the value to be written into the register 
* from the command line argument passed to the file belonging to the debugfs.
*
* @param[in]  file: debug file pointer.
* @param[in]  buf: It contains command line input reg name & value.
* @param[in]  count: contains buffer size. 
* @param[in]  ppos: offset value. 
*
* \retval  0 on Success. 
* \retval  error number on Failure.
*/

static ssize_t dwc_eqos_write(struct file *file, const char __user * buf,
				 size_t count, loff_t * ppos)
{
	int ret = 0;
	char regName[50];
	char regValue[25];
	unsigned long integer_value;
	char *end_ptr;

	DBGPR("--> dwc_eqos_write\n");

	if (count > DEBUGFS_MAX_SIZE)
		return -EINVAL;

	if (copy_from_user(debugfs_buf, buf, count)) {
		ret = -1;
	} else {
		get_reg_name(regName, debugfs_buf, count);
		get_reg_value(regValue, debugfs_buf, count);
		ret = count;

		integer_value = simple_strtoul(regValue, (char **)&end_ptr, 16);
		if ((*end_ptr != '\0') && (*end_ptr != '\n')) {
			printk(KERN_ERR
			       "Invalid value specified for register write");
			return -EINVAL;
		}

		if (!strcmp(regName, "mac_ma32_127lr127")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(127), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr126")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(126), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr125")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(125), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr124")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(124), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr123")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(123), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr122")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(122), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr121")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(121), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr120")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(120), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr119")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(119), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr118")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(118), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr117")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(117), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr116")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(116), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr115")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(115), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr114")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(114), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr113")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(113), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr112")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(112), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr111")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(111), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr110")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(110), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr109")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(109), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr108")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(108), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr107")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(107), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr106")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(106), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr105")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(105), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr104")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(104), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr103")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(103), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr102")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(102), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr101")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(101), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr100")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(100), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr99")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(99), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr98")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(98), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr97")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(97), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr96")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(96), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr95")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(95), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr94")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(94), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr93")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(93), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr92")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(92), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr91")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(91), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr90")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(90), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr89")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(89), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr88")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(88), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr87")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(87), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr86")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(86), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr85")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(85), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr84")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(84), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr83")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(83), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr82")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(82), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr81")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(81), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr80")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(80), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr79")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(79), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr78")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(78), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr77")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(77), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr76")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(76), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr75")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(75), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr74")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(74), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr73")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(73), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr72")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(72), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr71")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(71), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr70")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(70), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr69")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(69), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr68")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(68), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr67")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(67), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr66")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(66), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr65")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(65), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr64")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(64), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr63")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(63), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr62")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(62), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr61")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(61), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr60")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(60), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr59")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(59), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr58")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(58), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr57")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(57), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr56")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(56), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr55")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(55), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr54")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(54), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr53")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(53), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr52")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(52), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr51")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(51), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr50")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(50), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr49")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(49), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr48")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(48), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr47")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(47), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr46")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(46), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr45")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(45), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr44")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(44), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr43")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(43), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr42")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(42), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr41")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(41), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr40")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(40), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr39")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(39), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr38")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(38), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr37")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(37), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr36")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(36), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr35")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(35), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr34")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(34), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr33")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(33), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127lr32")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(32), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr127")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(127), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr126")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(126), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr125")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(125), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr124")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(124), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr123")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(123), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr122")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(122), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr121")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(121), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr120")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(120), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr119")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(119), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr118")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(118), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr117")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(117), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr116")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(116), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr115")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(115), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr114")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(114), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr113")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(113), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr112")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(112), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr111")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(111), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr110")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(110), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr109")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(109), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr108")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(108), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr107")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(107), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr106")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(106), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr105")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(105), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr104")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(104), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr103")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(103), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr102")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(102), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr101")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(101), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr100")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(100), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr99")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(99), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr98")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(98), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr97")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(97), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr96")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(96), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr95")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(95), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr94")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(94), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr93")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(93), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr92")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(92), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr91")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(91), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr90")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(90), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr89")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(89), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr88")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(88), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr87")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(87), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr86")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(86), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr85")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(85), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr84")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(84), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr83")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(83), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr82")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(82), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr81")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(81), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr80")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(80), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr79")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(79), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr78")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(78), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr77")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(77), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr76")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(76), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr75")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(75), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr74")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(74), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr73")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(73), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr72")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(72), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr71")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(71), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr70")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(70), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr69")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(69), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr68")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(68), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr67")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(67), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr66")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(66), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr65")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(65), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr64")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(64), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr63")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(63), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr62")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(62), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr61")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(61), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr60")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(60), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr59")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(59), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr58")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(58), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr57")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(57), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr56")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(56), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr55")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(55), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr54")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(54), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr53")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(53), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr52")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(52), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr51")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(51), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr50")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(50), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr49")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(49), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr48")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(48), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr47")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(47), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr46")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(46), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr45")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(45), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr44")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(44), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr43")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(43), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr42")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(42), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr41")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(41), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr40")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(40), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr39")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(39), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr38")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(38), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr37")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(37), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr36")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(36), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr35")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(35), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr34")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(34), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr33")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(33), integer_value);
		} else if (!strcmp(regName, "mac_ma32_127hr32")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(32), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr31")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(31), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr30")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(30), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr29")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(29), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr28")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(28), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr27")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(27), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr26")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(26), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr25")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(25), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr24")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(24), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr23")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(23), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr22")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(22), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr21")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(21), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr20")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(20), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr19")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(19), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr18")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(18), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr17")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(17), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr16")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(16), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr15")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(15), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr14")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(14), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr13")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(13), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr12")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(12), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr11")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(11), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr10")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(10), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr9")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(9), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr8")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(8), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(7), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(6), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(5), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(4), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(3), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(2), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31lr1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(1), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr31")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(31), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr30")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(30), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr29")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(29), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr28")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(28), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr27")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(27), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr26")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(26), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr25")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(25), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr24")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(24), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr23")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(23), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr22")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(22), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr21")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(21), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr20")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(20), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr19")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(19), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr18")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(18), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr17")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(17), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr16")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(16), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr15")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(15), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr14")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(14), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr13")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(13), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr12")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(12), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr11")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(11), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr10")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(10), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr9")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(9), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr8")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(8), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(7), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(6), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(5), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(4), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(3), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(2), integer_value);
		} else if (!strcmp(regName, "mac_ma1_31hr1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(1), integer_value);
		} else if (!strcmp(regName, "mac_arpa")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ARP_ADDR, integer_value);
		} else if (!strcmp(regName, "mac_l3a3r7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(7), integer_value);
		} else if (!strcmp(regName, "mac_l3a3r6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(6), integer_value);
		} else if (!strcmp(regName, "mac_l3a3r5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(5), integer_value);
		} else if (!strcmp(regName, "mac_l3a3r4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(4), integer_value);
		} else if (!strcmp(regName, "mac_l3a3r3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(3), integer_value);
		} else if (!strcmp(regName, "mac_l3a3r2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(2), integer_value);
		} else if (!strcmp(regName, "mac_l3a3r1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(1), integer_value);
		} else if (!strcmp(regName, "mac_l3a3r0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR3(0), integer_value);
		} else if (!strcmp(regName, "mac_l3a2r7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(7), integer_value);
		} else if (!strcmp(regName, "mac_l3a2r6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(6), integer_value);
		} else if (!strcmp(regName, "mac_l3a2r5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(5), integer_value);
		} else if (!strcmp(regName, "mac_l3a2r4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(4), integer_value);
		} else if (!strcmp(regName, "mac_l3a2r3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(3), integer_value);
		} else if (!strcmp(regName, "mac_l3a2r2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(2), integer_value);
		} else if (!strcmp(regName, "mac_l3a2r1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(1), integer_value);
		} else if (!strcmp(regName, "mac_l3a2r0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR2(0), integer_value);
		} else if (!strcmp(regName, "mac_l3a1r7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(7), integer_value);
		} else if (!strcmp(regName, "mac_l3a1r6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(6), integer_value);
		} else if (!strcmp(regName, "mac_l3a1r5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(5), integer_value);
		} else if (!strcmp(regName, "mac_l3a1r4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(4), integer_value);
		} else if (!strcmp(regName, "mac_l3a1r3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(3), integer_value);
		} else if (!strcmp(regName, "mac_l3a1r2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(2), integer_value);
		} else if (!strcmp(regName, "mac_l3a1r1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(1), integer_value);
		} else if (!strcmp(regName, "mac_l3a1r0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR1(0), integer_value);
		} else if (!strcmp(regName, "mac_l3a0r7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(7), integer_value);
		} else if (!strcmp(regName, "mac_l3a0r6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(6), integer_value);
		} else if (!strcmp(regName, "mac_l3a0r5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(5), integer_value);
		} else if (!strcmp(regName, "mac_l3a0r4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(4), integer_value);
		} else if (!strcmp(regName, "mac_l3a0r3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(3), integer_value);
		} else if (!strcmp(regName, "mac_l3a0r2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(2), integer_value);
		} else if (!strcmp(regName, "mac_l3a0r1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(1), integer_value);
		} else if (!strcmp(regName, "mac_l3a0r0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3_ADDR0(0), integer_value);
		} else if (!strcmp(regName, "mac_l4ar7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(7), integer_value);
		} else if (!strcmp(regName, "mac_l4ar6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(6), integer_value);
		} else if (!strcmp(regName, "mac_l4ar5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(5), integer_value);
		} else if (!strcmp(regName, "mac_l4ar4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(4), integer_value);
		} else if (!strcmp(regName, "mac_l4ar3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(3), integer_value);
		} else if (!strcmp(regName, "mac_l4ar2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(2), integer_value);
		} else if (!strcmp(regName, "mac_l4ar1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(1), integer_value);
		} else if (!strcmp(regName, "mac_l4ar0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L4_ADDR(0), integer_value);
		} else if (!strcmp(regName, "mac_l3l4cr7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(7), integer_value);
		} else if (!strcmp(regName, "mac_l3l4cr6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(6), integer_value);
		} else if (!strcmp(regName, "mac_l3l4cr5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(5), integer_value);
		} else if (!strcmp(regName, "mac_l3l4cr4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(4), integer_value);
		} else if (!strcmp(regName, "mac_l3l4cr3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(3), integer_value);
		} else if (!strcmp(regName, "mac_l3l4cr2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(2), integer_value);
		} else if (!strcmp(regName, "mac_l3l4cr1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(1), integer_value);
		} else if (!strcmp(regName, "mac_l3l4cr0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_L3L4_CTRL(0), integer_value);
		} else if (!strcmp(regName, "mac_gpio")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_GPIO_STAT, integer_value);
		} else if (!strcmp(regName, "mac_pcs")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PHYIF_CS, integer_value);
		} else if (!strcmp(regName, "mac_tes")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_tes : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_ae")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_ae : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_alpa")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_alpa : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_aad")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_AN_ADVERTISEMENT, integer_value);
		} else if (!strcmp(regName, "mac_ans")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_ans : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_anc")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_AN_CTRL, integer_value);
		} else if (!strcmp(regName, "mac_lpc")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_LPI_TIMERS_CTRL, integer_value);
		} else if (!strcmp(regName, "mac_lps")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_LPI_CS, integer_value);
		} else if(!strcmp(regName,"mac_lmir")){
			dwceqos_writel(pdata, DWCEQOS_MAC_LOG_MSG_INTVL, integer_value);
		} else if(!strcmp(regName,"mac_spi2r")){
			dwceqos_writel(pdata, DWCEQOS_MAC_SRC_PORT_ID2, integer_value);
		} else if(!strcmp(regName,"mac_spi1r")){
			dwceqos_writel(pdata, DWCEQOS_MAC_SRC_PORT_ID1, integer_value);
		} else if(!strcmp(regName,"mac_spi0r")){
			dwceqos_writel(pdata, DWCEQOS_MAC_SRC_PORT_ID0, integer_value);
		} else if(!strcmp(regName,"mac_pto_cr")){
			dwceqos_writel(pdata, DWCEQOS_MAC_PTO_CTRL, integer_value);
		} else if (!strcmp(regName, "mac_pps_width3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_WIDTH(3), integer_value);
		} else if (!strcmp(regName, "mac_pps_width2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_WIDTH(2), integer_value);
		} else if (!strcmp(regName, "mac_pps_width1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_WIDTH(1), integer_value);
		} else if (!strcmp(regName, "mac_pps_width0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_WIDTH(0), integer_value);
		} else if (!strcmp(regName, "mac_pps_intval3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_INTERVAL(3), integer_value);
		} else if (!strcmp(regName, "mac_pps_intval2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_INTERVAL(2), integer_value);
		} else if (!strcmp(regName, "mac_pps_intval1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_INTERVAL(1), integer_value);
		} else if (!strcmp(regName, "mac_pps_intval0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_INTERVAL(0), integer_value);
		} else if (!strcmp(regName, "mac_pps_ttns3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_TTNS(3), integer_value);
		} else if (!strcmp(regName, "mac_pps_ttns2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_TTNS(2), integer_value);
		} else if (!strcmp(regName, "mac_pps_ttns1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_TTNS(1), integer_value);
		} else if (!strcmp(regName, "mac_pps_ttns0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_TTNS(0), integer_value);
		} else if (!strcmp(regName, "mac_pps_tts3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_TTS(3), integer_value);
		} else if (!strcmp(regName, "mac_pps_tts2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_TTS(2), integer_value);
		} else if (!strcmp(regName, "mac_pps_tts1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_TTS(1), integer_value);
		} else if (!strcmp(regName, "mac_pps_tts0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_TTS(0), integer_value);
		} else if (!strcmp(regName, "mac_ppsc")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PPS_CTRL, integer_value);
		} else if (!strcmp(regName, "mac_teac")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_TS_EG_ASYM_CORR, integer_value);
		} else if (!strcmp(regName, "mac_tiac")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_TS_ING_ASYM_CORR, integer_value);
		} else if (!strcmp(regName, "mac_ats")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_ats : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_atn")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_atn : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_ac")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_AUX_CTRL, integer_value);
		} else if (!strcmp(regName, "mac_ttn")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_ttn : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_ttsn")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_ttsn : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_tsr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_tsr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_sthwr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_SYS_TIME_H_WORD_SEC, integer_value);
		} else if (!strcmp(regName, "mac_tar")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_TS_ADDEND, integer_value);
		} else if (!strcmp(regName, "mac_stnsur")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_SYS_TIME_NSEC_UPD, integer_value);
		} else if (!strcmp(regName, "mac_stsur")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_SYS_TIME_SEC_UPD, integer_value);
		} else if (!strcmp(regName, "mac_stnsr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_stnsr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_stsr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_stsr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_ssir")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_SUB_SEC_INC, integer_value);
		} else if (!strcmp(regName, "mac_tcr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_TS_CTRL, integer_value);
		} else if (!strcmp(regName, "mtl_dsr")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_DBG_STS, integer_value);
		} else if (!strcmp(regName, "mac_rwpffr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_RWK_PKT_FILT, integer_value);
		} else if (!strcmp(regName, "mac_rtsr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_rtsr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_ier")) {
			//MTL_IER_RgWr(integer_value);
		} else if (!strcmp(regName, "mtl_qrcr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_CTRL(7), integer_value);
		} else if (!strcmp(regName, "mtl_qrcr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_CTRL(6), integer_value);
		} else if (!strcmp(regName, "mtl_qrcr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_CTRL(5), integer_value);
		} else if (!strcmp(regName, "mtl_qrcr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_CTRL(4), integer_value);
		} else if (!strcmp(regName, "mtl_qrcr3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_CTRL(3), integer_value);
		} else if (!strcmp(regName, "mtl_qrcr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_CTRL(2), integer_value);
		} else if (!strcmp(regName, "mtl_qrcr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_CTRL(1), integer_value);
		} else if (!strcmp(regName, "mtl_qrdr7")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qrdr7 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qrdr6")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qrdr6 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qrdr5")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qrdr5 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qrdr4")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qrdr4 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qrdr3")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qrdr3 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qrdr2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qrdr2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qrdr1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qrdr1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qocr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_MPOC(7), integer_value);
		} else if (!strcmp(regName, "mtl_qocr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_MPOC(6), integer_value);
		} else if (!strcmp(regName, "mtl_qocr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_MPOC(5), integer_value);
		} else if (!strcmp(regName, "mtl_qocr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_MPOC(4), integer_value);
		} else if (!strcmp(regName, "mtl_qocr3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_MPOC(3), integer_value);
		} else if (!strcmp(regName, "mtl_qocr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_MPOC(2), integer_value);
		} else if (!strcmp(regName, "mtl_qocr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_MPOC(1), integer_value);
		} else if (!strcmp(regName, "mtl_qromr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(7), integer_value);
		} else if (!strcmp(regName, "mtl_qromr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(6), integer_value);
		} else if (!strcmp(regName, "mtl_qromr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(5), integer_value);
		} else if (!strcmp(regName, "mtl_qromr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(4), integer_value);
		} else if (!strcmp(regName, "mtl_qromr3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(3), integer_value);
		} else if (!strcmp(regName, "mtl_qromr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(2), integer_value);
		} else if (!strcmp(regName, "mtl_qromr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(1), integer_value);
		} else if (!strcmp(regName, "mtl_qlcr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_LC(7), integer_value);
		} else if (!strcmp(regName, "mtl_qlcr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_LC(6), integer_value);
		} else if (!strcmp(regName, "mtl_qlcr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_LC(5), integer_value);
		} else if (!strcmp(regName, "mtl_qlcr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_LC(4), integer_value);
		} else if (!strcmp(regName, "mtl_qlcr3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_LC(3), integer_value);
		} else if (!strcmp(regName, "mtl_qlcr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_LC(2), integer_value);
		} else if (!strcmp(regName, "mtl_qlcr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_LC(1), integer_value);
		} else if (!strcmp(regName, "mtl_qhcr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_HC(7), integer_value);
		} else if (!strcmp(regName, "mtl_qhcr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_HC(6), integer_value);
		} else if (!strcmp(regName, "mtl_qhcr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_HC(5), integer_value);
		} else if (!strcmp(regName, "mtl_qhcr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_HC(4), integer_value);
		} else if (!strcmp(regName, "mtl_qhcr3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_HC(3), integer_value);
		} else if (!strcmp(regName, "mtl_qhcr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_HC(2), integer_value);
		} else if (!strcmp(regName, "mtl_qhcr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_HC(1), integer_value);
		} else if (!strcmp(regName, "mtl_qsscr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_SSC(7), integer_value);
		} else if (!strcmp(regName, "mtl_qsscr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_SSC(6), integer_value);
		} else if (!strcmp(regName, "mtl_qsscr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_SSC(5), integer_value);
		} else if (!strcmp(regName, "mtl_qsscr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_SSC(4), integer_value);
		} else if (!strcmp(regName, "mtl_qsscr3")) { 
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_SSC(3), integer_value);
		} else if (!strcmp(regName, "mtl_qsscr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_SSC(2), integer_value);
		} else if (!strcmp(regName, "mtl_qsscr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_SSC(1), integer_value);
		} else if (!strcmp(regName, "mtl_qw7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(7), integer_value);
		} else if (!strcmp(regName, "mtl_qw6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(6), integer_value);
		} else if (!strcmp(regName, "mtl_qw5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(5), integer_value);
		} else if (!strcmp(regName, "mtl_qw4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(4), integer_value);
		} else if (!strcmp(regName, "mtl_qw3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(3), integer_value);
		} else if (!strcmp(regName, "mtl_qw2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(2), integer_value);
		} else if (!strcmp(regName, "mtl_qw1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(1), integer_value);
		} else if (!strcmp(regName, "mtl_qesr7")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qesr7 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qesr6")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qesr6 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qesr5")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qesr5 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qesr4")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qesr4 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qesr3")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qesr3 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qesr2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qesr2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qesr1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qesr1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qecr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(7), integer_value);
		} else if (!strcmp(regName, "mtl_qecr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(6), integer_value);
		} else if (!strcmp(regName, "mtl_qecr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(5), integer_value);
		} else if (!strcmp(regName, "mtl_qecr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(4), integer_value);
		} else if (!strcmp(regName, "mtl_qecr3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(3), integer_value);
		} else if (!strcmp(regName, "mtl_qecr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(2), integer_value);
		} else if (!strcmp(regName, "mtl_qecr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(1), integer_value);
		} else if (!strcmp(regName, "mtl_qtdr7")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qtdr7 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qtdr6")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qtdr6 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qtdr5")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qtdr5 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qtdr4")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qtdr4 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qtdr3")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qtdr3 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qtdr2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qtdr2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qtdr1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_qtdr1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_qucr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_UFLOW(7), integer_value);
		} else if (!strcmp(regName, "mtl_qucr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_UFLOW(6), integer_value);
		} else if (!strcmp(regName, "mtl_qucr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_UFLOW(5), integer_value);
		} else if (!strcmp(regName, "mtl_qucr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_UFLOW(4), integer_value);
		} else if (!strcmp(regName, "mtl_qucr3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_UFLOW(3), integer_value);
		} else if (!strcmp(regName, "mtl_qucr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_UFLOW(2), integer_value);
		} else if (!strcmp(regName, "mtl_qucr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_UFLOW(1), integer_value);
		} else if (!strcmp(regName, "mtl_qtomr7")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(7), integer_value);
		} else if (!strcmp(regName, "mtl_qtomr6")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(6), integer_value);
		} else if (!strcmp(regName, "mtl_qtomr5")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(5), integer_value);
		} else if (!strcmp(regName, "mtl_qtomr4")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(4), integer_value);
		} else if (!strcmp(regName, "mtl_qtomr3")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(3), integer_value);
		} else if (!strcmp(regName, "mtl_qtomr2")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(2), integer_value);
		} else if (!strcmp(regName, "mtl_qtomr1")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(1), integer_value);
		} else if (!strcmp(regName, "mac_pmtcsr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PMT_CS, integer_value);
		} else if (!strcmp(regName, "mmc_rxicmp_err_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxicmp_err_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxicmp_gd_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxicmp_gd_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxtcp_err_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxtcp_err_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxtcp_gd_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxtcp_gd_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxudp_err_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxudp_err_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxudp_gd_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxudp_gd_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv6_nopay_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv6_nopay_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv6_hdrerr_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv6_hdrerr_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv6_gd_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv6_gd_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_udsbl_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_udsbl_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_frag_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_frag_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_nopay_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_nopay_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_hdrerr_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_hdrerr_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_gd_octets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_gd_octets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxicmp_err_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxicmp_err_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxicmp_gd_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxicmp_gd_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxtcp_err_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxtcp_err_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxtcp_gd_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxtcp_gd_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxudp_err_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxudp_err_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxudp_gd_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxudp_gd_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv6_nopay_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv6_nopay_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv6_hdrerr_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv6_hdrerr_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv6_gd_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv6_gd_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_ubsbl_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_ubsbl_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_frag_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_frag_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_nopay_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_nopay_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_hdrerr_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_hdrerr_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxipv4_gd_pkts")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxipv4_gd_pkts : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxctrlpackets_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxctrlpackets_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxrcverror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxrcverror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxwatchdogerror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxwatchdogerror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxvlanpackets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxvlanpackets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxfifooverflow")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxfifooverflow : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxpausepackets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxpausepackets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxoutofrangetype")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxoutofrangetype : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxlengtherror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxlengtherror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxunicastpackets_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxunicastpackets_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rx1024tomaxoctets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rx1024tomaxoctets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rx512to1023octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rx512to1023octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rx256to511octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rx256to511octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rx128to255octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rx128to255octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rx65to127octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rx65to127octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rx64octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rx64octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxoversize_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxoversize_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxundersize_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxundersize_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxjabbererror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxjabbererror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxrunterror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxrunterror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxalignmenterror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxalignmenterror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxcrcerror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxcrcerror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxmulticastpackets_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxmulticastpackets_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxbroadcastpackets_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxbroadcastpackets_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxoctetcount_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxoctetcount_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxoctetcount_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxoctetcount_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_rxpacketcount_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_rxpacketcount_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txoversize_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txoversize_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txvlanpackets_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txvlanpackets_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txpausepackets")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txpausepackets : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txexcessdef")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txexcessdef : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txpacketscount_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txpacketscount_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txoctetcount_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txoctetcount_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txcarriererror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txcarriererror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txexesscol")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txexesscol : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txlatecol")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txlatecol : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txdeferred")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txdeferred : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txmulticol_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txmulticol_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txsinglecol_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txsinglecol_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txunderflowerror")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txunderflowerror : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txbroadcastpackets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txbroadcastpackets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txmulticastpackets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txmulticastpackets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txunicastpackets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txunicastpackets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_tx1024tomaxoctets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_tx1024tomaxoctets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_tx512to1023octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_tx512to1023octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_tx256to511octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_tx256to511octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_tx128to255octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_tx128to255octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_tx65to127octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_tx65to127octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_tx64octets_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_tx64octets_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txmulticastpackets_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txmulticastpackets_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txbroadcastpackets_g")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txbroadcastpackets_g : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txpacketcount_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txpacketcount_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_txoctetcount_gb")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_txoctetcount_gb : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_ipc_intr_rx")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mmc_ipc_intr_rx : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mmc_ipc_intr_mask_rx")) {
			dwceqos_writel(pdata, DWCEQOS_MMC_IPC_RX_INTR_MASK, integer_value);
		} else if (!strcmp(regName, "mmc_intr_mask_tx")) {
			dwceqos_writel(pdata, DWCEQOS_MMC_TX_INTR_MASK, integer_value);
		} else if (!strcmp(regName, "mmc_intr_mask_rx")) {
			dwceqos_writel(pdata, DWCEQOS_MMC_RX_INTR_MASK, integer_value);
		} else if (!strcmp(regName, "mmc_intr_tx")) {
			dwceqos_writel(pdata, DWCEQOS_MMC_TX_INTR, integer_value);
		} else if (!strcmp(regName, "mmc_intr_rx")) {
			dwceqos_writel(pdata, DWCEQOS_MMC_RX_INTR, integer_value);
		} else if (!strcmp(regName, "mmc_cntrl")) {
			dwceqos_writel(pdata, DWCEQOS_MMC_CTRL, integer_value);
		} else if (!strcmp(regName, "mac_ma1lr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(1), integer_value);
		} else if (!strcmp(regName, "mac_ma1hr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(1), integer_value);
		} else if (!strcmp(regName, "mac_ma0lr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_LOW(0), integer_value);
		} else if (!strcmp(regName, "mac_ma0hr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_ADDR_HIGH(0), integer_value);
		} else if (!strcmp(regName, "mac_gpior")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_GPIO_CTRL, integer_value);
		} else if (!strcmp(regName, "mac_gmiidr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_MDIO_DATA, integer_value);
		} else if (!strcmp(regName, "mac_gmiiar")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_MDIO_ADDR, integer_value);
		} else if (!strcmp(regName, "mac_hfr2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_hfr2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_hfr1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_hfr1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_hfr0")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_hfr0 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_mdr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_mdr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_vr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_vr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mac_htr7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_HT_REG7, integer_value);
		} else if (!strcmp(regName, "mac_htr6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_HT_REG6, integer_value);
		} else if (!strcmp(regName, "mac_htr5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_HT_REG5, integer_value);
		} else if (!strcmp(regName, "mac_htr4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_HT_REG4, integer_value);
		} else if (!strcmp(regName, "mac_htr3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_HT_REG3, integer_value);
		} else if (!strcmp(regName, "mac_htr2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_HT_REG2, integer_value);
		} else if (!strcmp(regName, "mac_htr1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_HT_REG1, integer_value);
		} else if (!strcmp(regName, "mac_htr0")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_HT_REG0, integer_value);
		} else if (!strcmp(regName, "dma_riwtr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(7), integer_value);
		} else if (!strcmp(regName, "dma_riwtr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(6), integer_value);
		} else if (!strcmp(regName, "dma_riwtr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(5), integer_value);
		} else if (!strcmp(regName, "dma_riwtr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(4), integer_value);
		} else if (!strcmp(regName, "dma_riwtr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(3), integer_value);
		} else if (!strcmp(regName, "dma_riwtr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(2), integer_value);
		} else if (!strcmp(regName, "dma_riwtr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(1), integer_value);
		} else if (!strcmp(regName, "dma_riwtr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(0), integer_value);
		} else if (!strcmp(regName, "dma_rdrlr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(7), integer_value);
		} else if (!strcmp(regName, "dma_rdrlr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(6), integer_value);
		} else if (!strcmp(regName, "dma_rdrlr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(5), integer_value);
		} else if (!strcmp(regName, "dma_rdrlr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(4), integer_value);
		} else if (!strcmp(regName, "dma_rdrlr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(3), integer_value);
		} else if (!strcmp(regName, "dma_rdrlr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(2), integer_value);
		} else if (!strcmp(regName, "dma_rdrlr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(1), integer_value);
		} else if (!strcmp(regName, "dma_rdrlr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(0), integer_value);
		} else if (!strcmp(regName, "dma_tdrlr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(7), integer_value);
		} else if (!strcmp(regName, "dma_tdrlr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(6), integer_value);
		} else if (!strcmp(regName, "dma_tdrlr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(5), integer_value);
		} else if (!strcmp(regName, "dma_tdrlr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(4), integer_value);
		} else if (!strcmp(regName, "dma_tdrlr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(3), integer_value);
		} else if (!strcmp(regName, "dma_tdrlr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(2), integer_value);
		} else if (!strcmp(regName, "dma_tdrlr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(1), integer_value);
		} else if (!strcmp(regName, "dma_tdrlr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(0), integer_value);
		} else if (!strcmp(regName, "dma_rdtp_rpdr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(7), integer_value);
		} else if (!strcmp(regName, "dma_rdtp_rpdr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(6), integer_value);
		} else if (!strcmp(regName, "dma_rdtp_rpdr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(5), integer_value);
		} else if (!strcmp(regName, "dma_rdtp_rpdr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(4), integer_value);
		} else if (!strcmp(regName, "dma_rdtp_rpdr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(3), integer_value);
		} else if (!strcmp(regName, "dma_rdtp_rpdr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(2), integer_value);
		} else if (!strcmp(regName, "dma_rdtp_rpdr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(1), integer_value);
		} else if (!strcmp(regName, "dma_rdtp_rpdr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(0), integer_value);
		} else if (!strcmp(regName, "dma_tdtp_tpdr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(7), integer_value);
		} else if (!strcmp(regName, "dma_tdtp_tpdr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(6), integer_value);
		} else if (!strcmp(regName, "dma_tdtp_tpdr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(5), integer_value);
		} else if (!strcmp(regName, "dma_tdtp_tpdr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(4), integer_value);
		} else if (!strcmp(regName, "dma_tdtp_tpdr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(3), integer_value);
		} else if (!strcmp(regName, "dma_tdtp_tpdr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(2), integer_value);
		} else if (!strcmp(regName, "dma_tdtp_tpdr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(1), integer_value);
		} else if (!strcmp(regName, "dma_tdtp_tpdr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(0), integer_value);
		} else if (!strcmp(regName, "dma_rdlar7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(7), integer_value);
		} else if (!strcmp(regName, "dma_rdlar6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(6), integer_value);
		} else if (!strcmp(regName, "dma_rdlar5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(5), integer_value);
		} else if (!strcmp(regName, "dma_rdlar4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(4), integer_value);
		} else if (!strcmp(regName, "dma_rdlar3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(3), integer_value);
		} else if (!strcmp(regName, "dma_rdlar2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(2), integer_value);
		} else if (!strcmp(regName, "dma_rdlar1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(1), integer_value);
		} else if (!strcmp(regName, "dma_rdlar0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(0), integer_value);
		} else if (!strcmp(regName, "dma_tdlar7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(7), integer_value);
		} else if (!strcmp(regName, "dma_tdlar6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(6), integer_value);
		} else if (!strcmp(regName, "dma_tdlar5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(5), integer_value);
		} else if (!strcmp(regName, "dma_tdlar4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(4), integer_value);
		} else if (!strcmp(regName, "dma_tdlar3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(3), integer_value);
		} else if (!strcmp(regName, "dma_tdlar2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(2), integer_value);
		} else if (!strcmp(regName, "dma_tdlar1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(1), integer_value);
		} else if (!strcmp(regName, "dma_tdlar0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(0), integer_value);
		} else if (!strcmp(regName, "dma_ier7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(7), integer_value);
		} else if (!strcmp(regName, "dma_ier6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(6), integer_value);
		} else if (!strcmp(regName, "dma_ier5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(5), integer_value);
		} else if (!strcmp(regName, "dma_ier4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(4), integer_value);
		} else if (!strcmp(regName, "dma_ier3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(3), integer_value);
		} else if (!strcmp(regName, "dma_ier2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(2), integer_value);
		} else if (!strcmp(regName, "dma_ier1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(1), integer_value);
		} else if (!strcmp(regName, "dma_ier0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_INTR_EN(0), integer_value);
		} else if (!strcmp(regName, "mac_imr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_INTR_EN, integer_value);
		} else if (!strcmp(regName, "mac_isr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_isr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_isr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_isr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_sr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(7), integer_value);
		} else if (!strcmp(regName, "dma_sr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(6), integer_value);
		} else if (!strcmp(regName, "dma_sr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(5), integer_value);
		} else if (!strcmp(regName, "dma_sr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(4), integer_value);
		} else if (!strcmp(regName, "dma_sr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(3), integer_value);
		} else if (!strcmp(regName, "dma_sr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(2), integer_value);
		} else if (!strcmp(regName, "dma_sr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(1), integer_value);
		} else if (!strcmp(regName, "dma_sr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_STAT(0), integer_value);
		} else if (!strcmp(regName, "dma_isr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_isr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_dsr2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_dsr2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_dsr1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_dsr1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_dsr0")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_dsr0 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_q0rdr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_q0rdr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_q0esr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_q0esr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_q0tdr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_q0tdr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrbar7")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrbar7 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrbar6")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrbar6 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrbar5")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrbar5 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrbar4")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrbar4 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrbar3")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrbar3 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrbar2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrbar2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrbar1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrbar1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrbar0")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrbar0 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtbar7")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtbar7 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtbar6")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtbar6 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtbar5")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtbar5 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtbar4")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtbar4 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtbar3")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtbar3 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtbar2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtbar2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtbar1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtbar1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtbar0")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtbar0 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrdr7")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrdr7 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrdr6")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrdr6 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrdr5")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrdr5 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrdr4")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrdr4 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrdr3")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrdr3 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrdr2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrdr2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrdr1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrdr1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chrdr0")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chrdr0 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtdr7")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtdr7 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtdr6")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtdr6 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtdr5")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtdr5 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtdr4")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtdr4 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtdr3")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtdr3 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtdr2")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtdr2 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtdr1")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtdr1 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_chtdr0")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation dma_chtdr0 : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "dma_sfcsr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(7), integer_value);
		} else if (!strcmp(regName, "dma_sfcsr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(6), integer_value);
		} else if (!strcmp(regName, "dma_sfcsr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(5), integer_value);
		} else if (!strcmp(regName, "dma_sfcsr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(4), integer_value);
		} else if (!strcmp(regName, "dma_sfcsr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(3), integer_value);
		} else if (!strcmp(regName, "dma_sfcsr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(2), integer_value);
		} else if (!strcmp(regName, "dma_sfcsr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(1), integer_value);
		} else if (!strcmp(regName, "dma_sfcsr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(0), integer_value);
		} else if (!strcmp(regName, "mac_ivlantirr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_INNER_VLAN_INCL, integer_value);
		} else if (!strcmp(regName, "mac_vlantirr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_INCL, integer_value);
		} else if (!strcmp(regName, "mac_vlanhtr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_HT, integer_value);
		} else if (!strcmp(regName, "mac_vlantr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL, integer_value);
		} else if (!strcmp(regName, "dma_sbus")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_SYSBUS_MODE, integer_value);
		} else if (!strcmp(regName, "dma_bmr")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_MODE, integer_value);
		} else if (!strcmp(regName, "mtl_q0rcr")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_CTRL(0), integer_value);
		} else if (!strcmp(regName, "mtl_q0ocr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_q0ocr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_q0romr")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_OP_MODE(0), integer_value);
		} else if (!strcmp(regName, "mtl_q0qr")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_QW(0), integer_value);
		} else if (!strcmp(regName, "mtl_q0ecr")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(0), integer_value);
		} else if (!strcmp(regName, "mtl_q0ucr")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_UFLOW(0), integer_value);
		} else if (!strcmp(regName, "mtl_q0tomr")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_TQ_OP_MODE(0), integer_value);
		} else if (!strcmp(regName, "mtl_rqdcm1r")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_DMA_MAP1, integer_value);
		} else if (!strcmp(regName, "mtl_rqdcm0r")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_RQ_DMA_MAP0, integer_value);
		} else if (!strcmp(regName, "mtl_fddr")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mtl_fddr : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mtl_fdacs")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_DBG_CTL, integer_value);
		} else if (!strcmp(regName, "mtl_omr")) {
			dwceqos_writel(pdata, DWCEQOS_MTL_OP_MODE, integer_value);
		} else if (!strcmp(regName, "mac_rqc3r")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_RQ_CTRL3, integer_value);
		} else if (!strcmp(regName, "mac_rqc2r")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_RQ_CTRL2, integer_value);
		} else if (!strcmp(regName, "mac_rqc1r")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_RQ_CTRL1, integer_value);
		} else if (!strcmp(regName, "mac_rqc0r")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_RQ_CTRL0, integer_value);
		} else if (!strcmp(regName, "mac_tqpm1r")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_TQPM1, integer_value);
		} else if (!strcmp(regName, "mac_tqpm0r")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_TQPM1, integer_value);
		} else if (!strcmp(regName, "mac_rfcr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_RX_FLOW_CTRL, integer_value);
		} else if (!strcmp(regName, "mac_qtfcr7")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(7), integer_value);
		} else if (!strcmp(regName, "mac_qtfcr6")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(6), integer_value);
		} else if (!strcmp(regName, "mac_qtfcr5")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(5), integer_value);
		} else if (!strcmp(regName, "mac_qtfcr4")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(4), integer_value);
		} else if (!strcmp(regName, "mac_qtfcr3")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(3), integer_value);
		} else if (!strcmp(regName, "mac_qtfcr2")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(2), integer_value);
		} else if (!strcmp(regName, "mac_qtfcr1")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(1), integer_value);
		} else if (!strcmp(regName, "mac_q0tfcr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(0), integer_value);
		#if 0
		} else if (!strcmp(regName, "dma_axi4cr7")) {
			DMA_AXI4CR7_RgWr(integer_value);
		} else if (!strcmp(regName, "dma_axi4cr6")) {
			DMA_AXI4CR6_RgWr(integer_value);
		} else if (!strcmp(regName, "dma_axi4cr5")) {
			DMA_AXI4CR5_RgWr(integer_value);
		} else if (!strcmp(regName, "dma_axi4cr4")) {
			DMA_AXI4CR4_RgWr(integer_value);
		} else if (!strcmp(regName, "dma_axi4cr3")) {
			DMA_AXI4CR3_RgWr(integer_value);
		} else if (!strcmp(regName, "dma_axi4cr2")) {
			DMA_AXI4CR2_RgWr(integer_value);
		} else if (!strcmp(regName, "dma_axi4cr1")) {
			DMA_AXI4CR1_RgWr(integer_value);
		} else if (!strcmp(regName, "dma_axi4cr0")) {
			DMA_AXI4CR0_RgWr(integer_value);
		#endif
		} else if (!strcmp(regName, "dma_rcr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(7), integer_value);
		} else if (!strcmp(regName, "dma_rcr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(6), integer_value);
		} else if (!strcmp(regName, "dma_rcr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(5), integer_value);
		} else if (!strcmp(regName, "dma_rcr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(4), integer_value);
		} else if (!strcmp(regName, "dma_rcr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(3), integer_value);
		} else if (!strcmp(regName, "dma_rcr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(2), integer_value);
		} else if (!strcmp(regName, "dma_rcr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(1), integer_value);
		} else if (!strcmp(regName, "dma_rcr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_RX_CTRL(0), integer_value);
		} else if (!strcmp(regName, "dma_tcr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(7), integer_value);
		} else if (!strcmp(regName, "dma_tcr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(6), integer_value);
		} else if (!strcmp(regName, "dma_tcr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(5), integer_value);
		} else if (!strcmp(regName, "dma_tcr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(4), integer_value);
		} else if (!strcmp(regName, "dma_tcr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(3), integer_value);
		} else if (!strcmp(regName, "dma_tcr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(2), integer_value);
		} else if (!strcmp(regName, "dma_tcr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(1), integer_value);
		} else if (!strcmp(regName, "dma_tcr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_TX_CTRL(0), integer_value);
		} else if (!strcmp(regName, "dma_cr7")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(7), integer_value);
		} else if (!strcmp(regName, "dma_cr6")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(6), integer_value);
		} else if (!strcmp(regName, "dma_cr5")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(5), integer_value);
		} else if (!strcmp(regName, "dma_cr4")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(4), integer_value);
		} else if (!strcmp(regName, "dma_cr3")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(3), integer_value);
		} else if (!strcmp(regName, "dma_cr2")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(2), integer_value);
		} else if (!strcmp(regName, "dma_cr1")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(1), integer_value);
		} else if (!strcmp(regName, "dma_cr0")) {
			dwceqos_writel(pdata, DWCEQOS_DMA_CH_CTRL(0), integer_value);
		} else if (!strcmp(regName, "mac_wtr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_WD_TIMEOUT, integer_value);
		} else if (!strcmp(regName, "mac_mpfr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_PKT_FILT, integer_value);
		} else if (!strcmp(regName, "mac_mecr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_EXT_CFG, integer_value);
		} else if (!strcmp(regName, "mac_mcr")) {
			dwceqos_writel(pdata, DWCEQOS_MAC_CFG, integer_value); 
			//MAC_MCR_RgWr(integer_value);
		}
		/* For MII/GMII register read */
		else if (!strcmp(regName, "mac_bmcr_reg")) {
			dwc_eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      MII_BMCR,
						      (int)integer_value);
		} else if (!strcmp(regName, "mac_bmsr_reg")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mac_bmsr_reg : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mii_physid1_reg")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mii_physid1_reg : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mii_physid2_reg")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mii_physid2_reg : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mii_advertise_reg")) {
			dwc_eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      MII_ADVERTISE,
						      (int)integer_value);
		} else if (!strcmp(regName, "mii_lpa_reg")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mii_lpa_reg : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mii_expansion_reg")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mii_expansion_reg : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "auto_nego_np_reg")) {
			dwc_eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      DWC_EQOS_AUTO_NEGO_NP,
						      (int)integer_value);
		} else if (!strcmp(regName, "mii_estatus_reg")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mii_estatus_reg : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "mii_ctrl1000_reg")) {
			dwc_eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      MII_CTRL1000,
						      (int)integer_value);
		} else if (!strcmp(regName, "mii_stat1000_reg")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation mii_stat1000_reg : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "phy_ctl_reg")) {
			dwc_eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      DWC_EQOS_PHY_CTL,
						      (int)integer_value);
		} else if (!strcmp(regName, "phy_sts_reg")) {
			printk(KERN_ALERT
			       "Could not complete Write Operation phy_sts_reg : ReadOnly Register");
			ret = -EFAULT;
		} else if (!strcmp(regName, "feature_drop_tx_pktburstcnt")) {
			feature_drop_tx_pktburstcnt_val = (int)integer_value;
			if (feature_drop_tx_pktburstcnt_val == 0) {
				feature_drop_tx_pktburstcnt_val++;
				printk(KERN_ALERT
				       "Drop Tx frame count should be a positive non-zero number only\n");
			}
			pdata->drop_tx_pktburstcnt =
			    feature_drop_tx_pktburstcnt_val;
		} else if (!strcmp(regName, "qInx")) {
			qInx_val = (int)integer_value;
			if (qInx_val != 0
			    && ((qInx_val < 0)
				|| (qInx_val > (DWC_EQOS_QUEUE_CNT - 1)))) {
				qInx_val = 0;
				printk(KERN_ALERT "Invalid queue number\n");
				ret = -EFAULT;
			}
		} else {
			printk(KERN_ALERT
			       "Could not complete Write Operation to Register. Register not found.\n");
			ret = -EFAULT;
		}
	}

	DBGPR("<-- dwc_eqos_write\n");

	return ret;
}

static ssize_t registers_write(struct file *file, const char __user * buf,
			       size_t count, loff_t * ppos)
{
	DBGPR("--> registers_write\n");
	printk(KERN_INFO
	       "Specify the correct file name for write operation: write error\n");
	DBGPR("<-- registers_write\n");

	return -1;
}

static ssize_t registers_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	char *debug_buf = NULL;

	DBGPR("--> registers_read\n");

	mac_ma32_127lr127_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(127));
	mac_ma32_127lr126_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(126));
	mac_ma32_127lr125_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(125));
	mac_ma32_127lr124_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(124));
	mac_ma32_127lr123_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(123));
	mac_ma32_127lr122_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(122));
	mac_ma32_127lr121_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(121));
	mac_ma32_127lr120_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(120));
	mac_ma32_127lr119_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(119));
	mac_ma32_127lr118_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(118));
	mac_ma32_127lr117_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(117));
	mac_ma32_127lr116_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(116));
	mac_ma32_127lr115_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(115));
	mac_ma32_127lr114_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(114));
	mac_ma32_127lr113_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(113));
	mac_ma32_127lr112_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(112));
	mac_ma32_127lr111_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(111));
	mac_ma32_127lr110_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(110));
	mac_ma32_127lr109_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(109));
	mac_ma32_127lr108_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(108));
	mac_ma32_127lr107_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(107));
	mac_ma32_127lr106_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(106));
	mac_ma32_127lr105_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(105));
	mac_ma32_127lr104_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(104));
	mac_ma32_127lr103_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(103));
	mac_ma32_127lr102_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(102));
	mac_ma32_127lr101_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(101));
	mac_ma32_127lr100_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(100));
	mac_ma32_127lr99_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(99));
	mac_ma32_127lr98_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(98));
	mac_ma32_127lr97_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(97));
	mac_ma32_127lr96_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(96));
	mac_ma32_127lr95_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(95));
	mac_ma32_127lr94_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(94));
	mac_ma32_127lr93_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(93));
	mac_ma32_127lr92_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(92));
	mac_ma32_127lr91_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(91));
	mac_ma32_127lr90_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(90));
	mac_ma32_127lr89_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(89));
	mac_ma32_127lr88_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(88));
	mac_ma32_127lr87_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(87));
	mac_ma32_127lr86_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(86));
	mac_ma32_127lr85_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(85));
	mac_ma32_127lr84_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(84));
	mac_ma32_127lr83_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(83));
	mac_ma32_127lr82_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(82));
	mac_ma32_127lr81_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(81));
	mac_ma32_127lr80_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(80));
	mac_ma32_127lr79_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(79));
	mac_ma32_127lr78_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(78));
	mac_ma32_127lr77_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(77));
	mac_ma32_127lr76_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(76));
	mac_ma32_127lr75_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(75));
	mac_ma32_127lr74_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(74));
	mac_ma32_127lr73_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(73));
	mac_ma32_127lr72_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(72));
	mac_ma32_127lr71_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(71));
	mac_ma32_127lr70_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(70));
	mac_ma32_127lr69_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(69));
	mac_ma32_127lr68_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(68));
	mac_ma32_127lr67_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(67));
	mac_ma32_127lr66_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(66));
	mac_ma32_127lr65_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(65));
	mac_ma32_127lr64_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(64));
	mac_ma32_127lr63_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(63));
	mac_ma32_127lr62_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(62));
	mac_ma32_127lr61_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(61));
	mac_ma32_127lr60_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(60));
	mac_ma32_127lr59_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(59));
	mac_ma32_127lr58_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(58));
	mac_ma32_127lr57_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(57));
	mac_ma32_127lr56_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(56));
	mac_ma32_127lr55_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(55));
	mac_ma32_127lr54_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(54));
	mac_ma32_127lr53_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(53));
	mac_ma32_127lr52_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(52));
	mac_ma32_127lr51_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(51));
	mac_ma32_127lr50_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(50));
	mac_ma32_127lr49_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(49));
	mac_ma32_127lr48_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(48));
	mac_ma32_127lr47_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(47));
	mac_ma32_127lr46_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(46));
	mac_ma32_127lr45_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(45));
	mac_ma32_127lr44_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(44));
	mac_ma32_127lr43_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(43));
	mac_ma32_127lr42_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(42));
	mac_ma32_127lr41_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(41));
	mac_ma32_127lr40_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(40));
	mac_ma32_127lr39_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(39));
	mac_ma32_127lr38_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(38));
	mac_ma32_127lr37_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(37));
	mac_ma32_127lr36_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(36));
	mac_ma32_127lr35_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(35));
	mac_ma32_127lr34_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(34));
	mac_ma32_127lr33_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(33));
	mac_ma32_127lr32_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(32));
	mac_ma32_127hr127_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(127));
	mac_ma32_127hr126_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(126));
	mac_ma32_127hr125_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(125));
	mac_ma32_127hr124_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(124));
	mac_ma32_127hr123_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(123));
	mac_ma32_127hr122_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(122));
	mac_ma32_127hr121_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(121));
	mac_ma32_127hr120_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(120));
	mac_ma32_127hr119_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(119));
	mac_ma32_127hr118_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(118));
	mac_ma32_127hr117_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(117));
	mac_ma32_127hr116_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(116));
	mac_ma32_127hr115_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(115));
	mac_ma32_127hr114_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(114));
	mac_ma32_127hr113_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(113));
	mac_ma32_127hr112_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(112));
	mac_ma32_127hr111_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(111));
	mac_ma32_127hr110_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(110));
	mac_ma32_127hr109_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(109));
	mac_ma32_127hr108_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(108));
	mac_ma32_127hr107_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(107));
	mac_ma32_127hr106_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(106));
	mac_ma32_127hr105_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(105));
	mac_ma32_127hr104_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(104));
	mac_ma32_127hr103_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(103));
	mac_ma32_127hr102_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(102));
	mac_ma32_127hr101_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(101));
	mac_ma32_127hr100_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(100));
	mac_ma32_127hr99_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(99));
	mac_ma32_127hr98_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(98));
	mac_ma32_127hr97_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(97));
	mac_ma32_127hr96_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(96));
	mac_ma32_127hr95_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(95));
	mac_ma32_127hr94_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(94));
	mac_ma32_127hr93_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(93));
	mac_ma32_127hr92_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(92));
	mac_ma32_127hr91_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(91));
	mac_ma32_127hr90_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(90));
	mac_ma32_127hr89_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(89));
	mac_ma32_127hr88_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(88));
	mac_ma32_127hr87_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(87));
	mac_ma32_127hr86_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(86));
	mac_ma32_127hr85_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(85));
	mac_ma32_127hr84_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(84));
	mac_ma32_127hr83_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(83));
	mac_ma32_127hr82_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(82));
	mac_ma32_127hr81_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(81));
	mac_ma32_127hr80_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(80));
	mac_ma32_127hr79_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(79));
	mac_ma32_127hr78_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(78));
	mac_ma32_127hr77_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(77));
	mac_ma32_127hr76_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(76));
	mac_ma32_127hr75_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(75));
	mac_ma32_127hr74_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(74));
	mac_ma32_127hr73_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(73));
	mac_ma32_127hr72_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(72));
	mac_ma32_127hr71_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(71));
	mac_ma32_127hr70_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(70));
	mac_ma32_127hr69_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(69));
	mac_ma32_127hr68_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(68));
	mac_ma32_127hr67_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(67));
	mac_ma32_127hr66_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(66));
	mac_ma32_127hr65_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(65));
	mac_ma32_127hr64_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(64));
	mac_ma32_127hr63_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(63));
	mac_ma32_127hr62_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(62));
	mac_ma32_127hr61_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(61));
	mac_ma32_127hr60_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(60));
	mac_ma32_127hr59_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(59));
	mac_ma32_127hr58_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(58));
	mac_ma32_127hr57_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(57));
	mac_ma32_127hr56_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(56));
	mac_ma32_127hr55_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(55));
	mac_ma32_127hr54_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(54));
	mac_ma32_127hr53_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(53));
	mac_ma32_127hr52_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(52));
	mac_ma32_127hr51_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(51));
	mac_ma32_127hr50_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(50));
	mac_ma32_127hr49_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(49));
	mac_ma32_127hr48_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(48));
	mac_ma32_127hr47_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(47));
	mac_ma32_127hr46_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(46));
	mac_ma32_127hr45_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(45));
	mac_ma32_127hr44_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(44));
	mac_ma32_127hr43_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(43));
	mac_ma32_127hr42_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(42));
	mac_ma32_127hr41_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(41));
	mac_ma32_127hr40_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(40));
	mac_ma32_127hr39_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(39));
	mac_ma32_127hr38_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(38));
	mac_ma32_127hr37_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(37));
	mac_ma32_127hr36_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(36));
	mac_ma32_127hr35_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(35));
	mac_ma32_127hr34_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(34));
	mac_ma32_127hr33_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(33));
	mac_ma32_127hr32_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(32));
	mac_ma1_31lr31_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(31));
	mac_ma1_31lr30_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(30));
	mac_ma1_31lr29_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(29));
	mac_ma1_31lr28_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(28));
	mac_ma1_31lr27_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(27));
	mac_ma1_31lr26_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(26));
	mac_ma1_31lr25_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(25));
	mac_ma1_31lr24_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(24));
	mac_ma1_31lr23_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(23));
	mac_ma1_31lr22_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(22));
	mac_ma1_31lr21_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(21));
	mac_ma1_31lr20_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(20));
	mac_ma1_31lr19_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(19));
	mac_ma1_31lr18_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(18));
	mac_ma1_31lr17_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(17));
	mac_ma1_31lr16_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(16));
	mac_ma1_31lr15_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(15));
	mac_ma1_31lr14_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(14));
	mac_ma1_31lr13_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(13));
	mac_ma1_31lr12_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(12));
	mac_ma1_31lr11_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(11));
	mac_ma1_31lr10_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(10));
	mac_ma1_31lr9_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(9));
	mac_ma1_31lr8_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(8));
	mac_ma1_31lr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(7));
	mac_ma1_31lr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(6));
	mac_ma1_31lr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(5));
	mac_ma1_31lr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(4));
	mac_ma1_31lr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(3));
	mac_ma1_31lr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(2));
	mac_ma1_31lr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(1));
	mac_ma1_31hr31_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(31));
	mac_ma1_31hr30_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(30));
	mac_ma1_31hr29_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(29));
	mac_ma1_31hr28_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(28));
	mac_ma1_31hr27_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(27));
	mac_ma1_31hr26_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(26));
	mac_ma1_31hr25_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(25));
	mac_ma1_31hr24_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(24));
	mac_ma1_31hr23_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(23));
	mac_ma1_31hr22_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(22));
	mac_ma1_31hr21_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(21));
	mac_ma1_31hr20_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(20));
	mac_ma1_31hr19_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(19));
	mac_ma1_31hr18_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(18));
	mac_ma1_31hr17_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(17));
	mac_ma1_31hr16_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(16));
	mac_ma1_31hr15_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(15));
	mac_ma1_31hr14_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(14));
	mac_ma1_31hr13_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(13));
	mac_ma1_31hr12_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(12));
	mac_ma1_31hr11_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(11));
	mac_ma1_31hr10_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(10));
	mac_ma1_31hr9_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(9));
	mac_ma1_31hr8_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(8));
	mac_ma1_31hr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(7));
	mac_ma1_31hr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(6));
	mac_ma1_31hr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(5));
	mac_ma1_31hr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(4));
	mac_ma1_31hr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(3));
	mac_ma1_31hr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(2));
	mac_ma1_31hr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(1));
	mac_arpa_val = dwceqos_readl(pdata, DWCEQOS_MAC_ARP_ADDR);
	mac_l3a3r7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(7)); 
	mac_l3a3r6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(6));
	mac_l3a3r5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(5));
	mac_l3a3r4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(4));
	mac_l3a3r3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(3));	
	mac_l3a3r2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(2));
	mac_l3a3r1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(1));
	mac_l3a3r0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(0));
	mac_l3a2r7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(7));
	mac_l3a2r6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(6));
	mac_l3a2r5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(5));
	mac_l3a2r4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(4));
	mac_l3a2r3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(3));
	mac_l3a2r2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(2));
	mac_l3a2r1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(1));
	mac_l3a2r0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(0));
	mac_l3a1r7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(7));
	mac_l3a1r6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(6));
	mac_l3a1r5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(5));
	mac_l3a1r4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(4));
	mac_l3a1r3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(3));
	mac_l3a1r2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(2));
	mac_l3a1r1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(1));
	mac_l3a1r0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(0));
	mac_l3a0r7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(7));
	mac_l3a0r6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(6));
	mac_l3a0r5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(5));
	mac_l3a0r4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(4));
	mac_l3a0r3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(3));
	mac_l3a0r2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(2));
	mac_l3a0r1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(1));
	mac_l4ar7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(7));
	mac_l4ar6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(6));
	mac_l4ar5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(5));
	mac_l4ar4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(4));
	mac_l4ar3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(3));
	mac_l4ar2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(2));
	mac_l4ar1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(1));
	mac_l4ar0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(0));
	mac_l3l4cr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(7));
	mac_l3l4cr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(6));
	mac_l3l4cr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(5));
	mac_l3l4cr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(4));
	mac_l3l4cr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(3));
	mac_l3l4cr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(2));
	mac_l3l4cr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(1));
	mac_l3l4cr0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(0));
	mac_gpio_val = dwceqos_readl(pdata, DWCEQOS_MAC_GPIO_STAT);
	mac_pcs_val = dwceqos_readl(pdata, DWCEQOS_MAC_PHYIF_CS);
	mac_tes_val = dwceqos_readl(pdata, DWCEQOS_MAC_TBI_EXTENDED_STAT);
	mac_ae_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_EXPANSION);
	mac_alpa_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_LPA);
	mac_aad_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_ADVERTISEMENT);
	mac_ans_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_STAT);
	mac_anc_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_CTRL);
	mac_lpc_val = dwceqos_readl(pdata, DWCEQOS_MAC_LPI_TIMERS_CTRL);
   	mac_lmir_val = dwceqos_readl(pdata, DWCEQOS_MAC_LOG_MSG_INTVL); 
	//MAC_SPI2R_RgRd(mac_lmir_val);
	//MAC_SPI2R_RgRd(mac_spi2r_val);
	mac_spi2r_val = dwceqos_readl(pdata, DWCEQOS_MAC_SRC_PORT_ID2);
	mac_spi1r_val = dwceqos_readl(pdata, DWCEQOS_MAC_SRC_PORT_ID1);
	mac_spi0r_val = dwceqos_readl(pdata, DWCEQOS_MAC_SRC_PORT_ID0);
	mac_pto_cr_val = dwceqos_readl(pdata, DWCEQOS_MAC_PTO_CTRL);
	mac_pps_width3_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_WIDTH(3));
	mac_pps_width2_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_WIDTH(2));
	mac_pps_width1_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_WIDTH(1));
	mac_pps_width0_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_WIDTH(0));
	mac_pps_intval3_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_INTERVAL(3));
	mac_pps_intval2_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_INTERVAL(2));
	mac_pps_intval1_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_INTERVAL(1));
	mac_pps_intval0_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_INTERVAL(0));
	mac_pps_ttns3_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTNS(3));
	mac_pps_ttns2_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTNS(2));
	mac_pps_ttns1_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTNS(1));
	mac_pps_ttns0_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTNS(0));
	mac_pps_tts3_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTS(3));
	mac_pps_tts2_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTS(2));
	mac_pps_tts1_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTS(1));
	mac_pps_tts0_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTS(0));
	mac_ppsc_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_CTRL);
	mac_teac_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_EG_ASYM_CORR);
	mac_tiac_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_ING_ASYM_CORR);
	mac_ats_val = dwceqos_readl(pdata, DWCEQOS_MAC_AUX_TS_SEC);
	mac_atn_val = dwceqos_readl(pdata, DWCEQOS_MAC_AUX_TS_NSEC);
	mac_ac_val = dwceqos_readl(pdata, DWCEQOS_MAC_AUX_CTRL);
	mac_ttn_val = dwceqos_readl(pdata, DWCEQOS_MAC_TX_TS_STAT_SEC);
	mac_ttsn_val = dwceqos_readl(pdata, DWCEQOS_MAC_TX_TS_STAT_NSEC);
	mac_tsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_STAT);
	mac_sthwr_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_H_WORD_SEC);
	mac_tar_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_ADDEND);
	mac_stnsur_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_NSEC_UPD);
	mac_stsur_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_SEC_UPD);
	mac_stnsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_NSEC);
	mac_stsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_SEC);
	mac_ssir_val = dwceqos_readl(pdata, DWCEQOS_MAC_SUB_SEC_INC);
	mac_tcr_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
	mtl_dsr_val = dwceqos_readl(pdata, DWCEQOS_MTL_DBG_STS);
	mac_rwpffr_val = dwceqos_readl(pdata, DWCEQOS_MAC_RWK_PKT_FILT);
	mac_rtsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_RX_TX_STAT);
	//MTL_IER_RgRd(mtl_ier_val);
	mtl_qrcr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(7));
	mtl_qrcr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(6));
	mtl_qrcr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(5));
	mtl_qrcr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(4));
	mtl_qrcr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(3));
	mtl_qrcr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(2));
	mtl_qrcr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(1));
	mtl_qrdr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(7));
	mtl_qrdr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(6));
	mtl_qrdr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(5));
	mtl_qrdr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(4));
	mtl_qrdr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(3));
	mtl_qrdr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(2));
	mtl_qrdr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(1));
	mtl_qocr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(7));
	mtl_qocr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(6));
	mtl_qocr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(5));
	mtl_qocr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(4));
	mtl_qocr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(3));
	mtl_qocr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(2));
	mtl_qocr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(1));
	mtl_qromr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(7));
	mtl_qromr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(6));
	mtl_qromr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(5));
	mtl_qromr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(4));
	mtl_qromr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(3));
	mtl_qromr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(2));
	mtl_qromr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(1));
	mtl_qlcr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(7));
	mtl_qlcr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(6));
	mtl_qlcr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(5));
	mtl_qlcr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(4));
	mtl_qlcr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(3));
	mtl_qlcr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(2));
	mtl_qlcr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(1));
	mtl_qhcr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(7));
	mtl_qhcr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(6));
	mtl_qhcr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(5));
	mtl_qhcr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(4));
	mtl_qhcr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(3));
	mtl_qhcr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(2));
	mtl_qhcr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(1));
	mtl_qsscr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(7));
	mtl_qsscr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(6));
	mtl_qsscr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(5));
	mtl_qsscr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(4));
	mtl_qsscr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(3));
	mtl_qsscr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(2));
	mtl_qsscr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(1));
	mtl_qw7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(7));
	mtl_qw6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(6));
	mtl_qw5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(5));
	mtl_qw4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(4));
	mtl_qw3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(3));
	mtl_qw2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(2));
	mtl_qw1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(1));
	mtl_qesr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(7));
	mtl_qesr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(6));
	mtl_qesr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(5));
	mtl_qesr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(4));
	mtl_qesr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(3));
	mtl_qesr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(2));
	mtl_qesr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(1));
	mtl_qecr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(7));
	mtl_qecr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(6));
	mtl_qecr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(5));
	mtl_qecr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(4));
	mtl_qecr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(3));
	mtl_qecr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(2));
	mtl_qecr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(1));
	mtl_qtdr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(7));
	mtl_qtdr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(6));
	mtl_qtdr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(5));
	mtl_qtdr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(4));
	mtl_qtdr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(3));
	mtl_qtdr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(2));
	mtl_qtdr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(1));
	mtl_qucr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(7));
	mtl_qucr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(6));
	mtl_qucr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(5));
	mtl_qucr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(4));
	mtl_qucr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(3));
	mtl_qucr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(2));
	mtl_qucr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(1));
	mtl_qtomr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(7));
	mtl_qtomr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(6));
	mtl_qtomr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(5));
	mtl_qtomr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(4));
	mtl_qtomr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(3));
	mtl_qtomr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(2));
	mtl_qtomr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(1));
	mac_pmtcsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_PMT_CS);
	if (pdata->hw_feat.mmc_sel) {
		mmc_rxicmp_err_octets_val = dwceqos_readl(pdata, DWCEQOS_RXICMP_ERR_OCTS);
		mmc_rxicmp_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXICMP_GOOD_OCTS);
		mmc_rxtcp_err_octets_val = dwceqos_readl(pdata, DWCEQOS_RXTCP_ERR_OCTS);
		mmc_rxtcp_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXTCP_GOOD_OCTS);
		mmc_rxudp_err_octets_val = dwceqos_readl(pdata, DWCEQOS_RXUDP_ERR_OCTS);
		mmc_rxudp_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXUDP_GOOD_OCTS);
		mmc_rxipv6_nopay_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_NO_PAYLOAD_OCTS);
		mmc_rxipv6_hdrerr_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_HDR_ERR_OCTS);
		mmc_rxipv6_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_GOOD_OCTS);
		mmc_rxipv4_udsbl_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_UDP_CKSM_DSBL_OCTS);
		mmc_rxipv4_frag_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_FRAGMENTED_OCTS);
		mmc_rxipv4_nopay_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_NO_PAYLOAD_OCTS); 
		mmc_rxipv4_hdrerr_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_HDR_ERR_OCTS);
		mmc_rxipv4_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_GOOD_OCTS);
		mmc_rxicmp_err_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXICMP_ERR_PKTS);
		mmc_rxicmp_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXICMP_GOOD_PKTS);
		mmc_rxtcp_err_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXTCP_ERR_PKTS);
		mmc_rxtcp_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXTCP_GOOD_PKTS);
		mmc_rxudp_err_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXUDP_ERR_PKT);
		mmc_rxudp_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXUDP_GOOD_PKTS);
		mmc_rxipv6_nopay_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_NO_PAYLOAD_PKTS);
		mmc_rxipv6_hdrerr_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_HDR_ERR_PKTS);
		mmc_rxipv6_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_GOOD_PKTS);
		mmc_rxipv4_ubsbl_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_UDP_CKSM_DSBLD_PKT);
		mmc_rxipv4_frag_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_FRAGMENTED_PKTS);
		mmc_rxipv4_nopay_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_NO_PAYLOAD_PKTS);
		mmc_rxipv4_hdrerr_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_HDR_ERR_PKTS);
		mmc_rxipv4_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_GOOD_PKTS);
		mmc_rxctrlpackets_g_val = dwceqos_readl(pdata, DWCEQOS_RX_CTRL_PKTS_GOOD);
		mmc_rxrcverror_val = dwceqos_readl(pdata, DWCEQOS_RX_RECEIVE_ERR_PKTS);
		mmc_rxwatchdogerror_val = dwceqos_readl(pdata, DWCEQOS_RX_WDOG_ERR_PKTS);
		mmc_rxvlanpackets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_VLAN_PKTS_GOOD_BAD);
		mmc_rxfifooverflow_val = dwceqos_readl(pdata, DWCEQOS_RX_FIFO_OFLOW_PKTS);
		mmc_rxpausepackets_val = dwceqos_readl(pdata, DWCEQOS_RX_PAUSE_PKTS);
		mmc_rxoutofrangetype_val = dwceqos_readl(pdata, DWCEQOS_RX_OUT_OF_RNG_TYPE_PKTS);
		mmc_rxlengtherror_val = dwceqos_readl(pdata, DWCEQOS_RX_LEN_ERR_PKTS);
		mmc_rxunicastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_RX_UCT_PKTS_GOOD);
		mmc_rx1024tomaxoctets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_1024_MAX_OCTS_PKTS_GB);
		mmc_rx512to1023octets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_512_1023_OCTS_PKTS_GB);
		mmc_rx256to511octets_gb_val =dwceqos_readl(pdata, DWCEQOS_RX_256_511_OCTS_PKTS_GB);
		mmc_rx128to255octets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_128_255_OCTS_PKTS_GB);
		mmc_rx65to127octets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_65_127_OCTS_PKTS_GB);
		mmc_rx64octets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_64_OCTS_PKTS_GB);
		mmc_rxoversize_g_val = dwceqos_readl(pdata, DWCEQOS_RX_OSIZE_PKTS_GOOD);
		mmc_rxundersize_g_val = dwceqos_readl(pdata, DWCEQOS_RX_USIZE_PKTS_GOOD);
		mmc_rxjabbererror_val = dwceqos_readl(pdata, DWCEQOS_RX_JABBER_ERR_PKTS);
		mmc_rxrunterror_val = dwceqos_readl(pdata, DWCEQOS_RX_RUNT_ERR_PKTS);
		mmc_rxalignmenterror_val = dwceqos_readl(pdata, DWCEQOS_RX_ALGMNT_ERR_PKTS);
		mmc_rxcrcerror_val = dwceqos_readl(pdata, DWCEQOS_RX_CRC_ERR_PKTS);
		mmc_rxmulticastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_RX_MLT_PKTS_GOOD);
		mmc_rxbroadcastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_RX_BDT_PKTS_GOOD);
		mmc_rxoctetcount_g_val = dwceqos_readl(pdata, DWCEWOS_RX_OCT_CNT_GOOD);
		mmc_rxoctetcount_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_OCTET_CNT_GOOD_BAD);
		mmc_rxpacketcount_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_PKTS_CNT_GOOD_BAD);
		mmc_txoversize_g_val = dwceqos_readl(pdata, DWCEQOS_TX_OSIZE_PKTS_GOOD);
		mmc_txvlanpackets_g_val = dwceqos_readl(pdata, DWCEQOS_TX_VLAN_PKTS_GOOD);
		mmc_txpausepackets_val = dwceqos_readl(pdata, DWCEQOS_TX_PAUSE_PKTS);
		mmc_txexcessdef_val = dwceqos_readl(pdata, DWCEQOS_TX_EXCESS_DEF_ERR);
		mmc_txpacketscount_g_val = dwceqos_readl(pdata, DWCEQOS_TX_PKT_CNT_GOOD);
		mmc_txoctetcount_g_val = dwceqos_readl(pdata, DWCEWOS_TX_OCT_CNT_GOOD);
		mmc_txcarriererror_val = dwceqos_readl(pdata, DWCEQOS_TX_CARRIER_ERR_PKTS);
		mmc_txexesscol_val = dwceqos_readl(pdata, DWCEQOS_TX_EXCESS_COLLSN_PKTS);
		mmc_txlatecol_val = dwceqos_readl(pdata, DWCEQOS_TX_LATE_COLLSN_PKTS);
		mmc_txdeferred_val = dwceqos_readl(pdata, DWCEQOS_TX_DEF_PKTS);
		mmc_txmulticol_g_val = dwceqos_readl(pdata, DWCEQOS_TX_MUL_COLLSN_GOOD_PKTS);
		mmc_txsinglecol_g_val = dwceqos_readl(pdata, DWCEQOS_TX_SGL_COLLSN_GOOD_PKTS);
		mmc_txunderflowerror_val = dwceqos_readl(pdata, DWCEQOS_TX_UFLOW_ERR_PKTS);
		mmc_txbroadcastpackets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_BDT_PKTS_GOOD_BAD);
		mmc_txmulticastpackets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_MLT_PKTS_GOOD_BAD);
		mmc_txunicastpackets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_UCT_PKTS_GOOD_BAD);
		mmc_tx1024tomaxoctets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_1024_MAX_OCTS_PKTS_GB);
		mmc_tx512to1023octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_512_1023_OCTS_PKTS_GB);
		mmc_tx256to511octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_256_511_OCTS_PKTS_GB);
		mmc_tx128to255octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_128_255_OCTS_PKTS_GB);
		mmc_tx65to127octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_65_127_OCTS_PKTS_GB);
		mmc_tx64octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_64_OCTS_PKTS_GB);
		mmc_txmulticastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_TX_MLT_PKTS_GOOD);
		mmc_txbroadcastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_TX_BDT_PKTS_GOOD);
		mmc_txpacketcount_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_PKT_CNT_GOOD_BAD);
		mmc_txoctetcount_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_OCTET_CNT_GOOD_BAD);
		mmc_ipc_intr_rx_val = dwceqos_readl(pdata, DWCEQOS_MMC_IPC_RX_INTR);
		mmc_ipc_intr_mask_rx_val = dwceqos_readl(pdata, DWCEQOS_MMC_IPC_RX_INTR_MASK);
		mmc_intr_mask_tx_val = dwceqos_readl(pdata, DWCEQOS_MMC_TX_INTR_MASK);
		mmc_intr_mask_rx_val = dwceqos_readl(pdata, DWCEQOS_MMC_RX_INTR_MASK);
		mmc_intr_tx_val = dwceqos_readl(pdata, DWCEQOS_MMC_TX_INTR);
		mmc_intr_rx_val = dwceqos_readl(pdata, DWCEQOS_MMC_RX_INTR);
		mmc_cntrl_val = dwceqos_readl(pdata, DWCEQOS_MMC_CTRL);
	}
	mac_ma1lr_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(1));
	mac_ma1hr_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(1));
	mac_ma0lr_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(0));
	mac_ma0hr_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(0));
	mac_gpior_val = dwceqos_readl(pdata, DWCEQOS_MAC_GPIO_CTRL);
	mac_gmiidr_val = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_DATA);
	mac_gmiiar_val = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_ADDR);
	mac_hfr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE2);
	mac_hfr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE1);
	mac_hfr0_val = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE0);
	mac_mdr_val = dwceqos_readl(pdata, DWCEQOS_MAC_DBG);
	mac_vr_val = dwceqos_readl(pdata, DWCEQOS_MAC_VERSION);
	mac_htr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG7);
	mac_htr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG6);
	mac_htr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG5);
	mac_htr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG4);
	mac_htr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG3);
	mac_htr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG2);
	mac_htr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG1);
	mac_htr0_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG0);
	dma_riwtr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(7));
	dma_riwtr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(6));
	dma_riwtr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(5));
	dma_riwtr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(4));
	dma_riwtr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(3));
	dma_riwtr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(2));
	dma_riwtr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(1));
	dma_riwtr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(0));
	dma_rdrlr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(7));
	dma_rdrlr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(6));
	dma_rdrlr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(5));
	dma_rdrlr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(4));
	dma_rdrlr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(3));
	dma_rdrlr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(2));
	dma_rdrlr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(1));
	dma_rdrlr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(0));
	dma_tdrlr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(7));
	dma_tdrlr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(6));
	dma_tdrlr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(5));
	dma_tdrlr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(4));
	dma_tdrlr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(3));
	dma_tdrlr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(2));
	dma_tdrlr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(1));
	dma_tdrlr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(0));
	dma_rdtp_rpdr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(7));
	dma_rdtp_rpdr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(6));
	dma_rdtp_rpdr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(5));
	dma_rdtp_rpdr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(4));
	dma_rdtp_rpdr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(3));
	dma_rdtp_rpdr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(2));
	dma_rdtp_rpdr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(1));
	dma_rdtp_rpdr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(0));
	dma_tdtp_tpdr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(7));
	dma_tdtp_tpdr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(6));
	dma_tdtp_tpdr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(5));
	dma_tdtp_tpdr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(4));
	dma_tdtp_tpdr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(3));
	dma_tdtp_tpdr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(2));
	dma_tdtp_tpdr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(1));
	dma_tdtp_tpdr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(0));
	dma_rdlar7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(7));
	dma_rdlar6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(6));
	dma_rdlar5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(5));
	dma_rdlar4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(4));
	dma_rdlar3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(3));
	dma_rdlar2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(2));
	dma_rdlar1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(1));
	dma_rdlar0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(0));
	dma_tdlar7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(7));
	dma_tdlar6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(6));
	dma_tdlar5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(5));
	dma_tdlar4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(4));
	dma_tdlar3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(3));
	dma_tdlar2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(2));
	dma_tdlar1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(1));
	dma_tdlar0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(0));
	dma_ier7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(7));
	dma_ier6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(6));
	dma_ier5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(5));
	dma_ier4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(4));
	dma_ier3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(3));
	dma_ier2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(2));
	dma_ier1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(1));
	dma_ier0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(0));
	mac_imr_val = dwceqos_readl(pdata, DWCEQOS_MAC_INTR_EN);
	mac_isr_val = dwceqos_readl(pdata, DWCEQOS_MAC_INTR_STAT);
	mtl_isr_val = dwceqos_readl(pdata, DWCEQOS_MTL_INTR_STAT);
	dma_sr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(7));
	dma_sr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(6));
	dma_sr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(5));
	dma_sr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(4));
	dma_sr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(3));
	dma_sr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(2));
	dma_sr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(1));
	dma_sr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(0));
	dma_isr_val = dwceqos_readl(pdata, DWCEQOS_DMA_INTR_STAT);
	dma_dsr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_DBG_STAT2);
	dma_dsr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_DBG_STAT1);
	dma_dsr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_DBG_STAT0);
	mtl_q0rdr_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(0));
	mtl_q0esr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(0));
	mtl_q0tdr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(0));
	dma_chrbar7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(7));
	dma_chrbar6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(6));
	dma_chrbar5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(5));
	dma_chrbar4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(4));
	dma_chrbar3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(3));
	dma_chrbar2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(2));
	dma_chrbar1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(1));
	dma_chrbar0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(0));
	dma_chtbar7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(7));
	dma_chtbar6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(6));
	dma_chtbar5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(5));
	dma_chtbar4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(4));
	dma_chtbar3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(3));
	dma_chtbar2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(2));
	dma_chtbar1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(1));
	dma_chtbar0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(0));
	dma_chrdr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(7));
	dma_chrdr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(6));
	dma_chrdr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(5));
	dma_chrdr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(4));
	dma_chrdr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(3));
	dma_chrdr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(2));
	dma_chrdr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(1));
	dma_chrdr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(0));
	dma_chtdr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(7));
	dma_chtdr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(6));
	dma_chtdr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(5));
	dma_chtdr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(4));
	dma_chtdr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(3));
	dma_chtdr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(2));
	dma_chtdr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(1));
	dma_chtdr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(0));
	dma_sfcsr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(7));
	dma_sfcsr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(6));
	dma_sfcsr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(5));
	dma_sfcsr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(4));
	dma_sfcsr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(3));
	dma_sfcsr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(2));
	dma_sfcsr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(1));
	dma_sfcsr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(0));
	mac_ivlantirr_val = dwceqos_readl(pdata, DWCEQOS_MAC_INNER_VLAN_INCL);
	mac_vlantirr_val = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
	mac_vlanhtr_val = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_HT);
	mac_vlantr_val = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	dma_sbus_val = dwceqos_readl(pdata, DWCEQOS_DMA_SYSBUS_MODE);
	dma_bmr_val = dwceqos_readl(pdata, DWCEQOS_DMA_MODE);
	mtl_q0rcr_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(0));
	mtl_q0ocr_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(0));
	mtl_q0romr_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(0));
	mtl_q0qr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(0));
	mtl_q0ecr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(0));
	mtl_q0ucr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(0));
	mtl_q0tomr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(0));
	mtl_rqdcm1r_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DMA_MAP1);
	mtl_rqdcm0r_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DMA_MAP0);
	mtl_fddr_val = dwceqos_readl(pdata, DWCEQOS_MTL_FIFO_DBG_DATA);
	mtl_fdacs_val = dwceqos_readl(pdata, DWCEQOS_MTL_DBG_CTL);
	mtl_omr_val = dwceqos_readl(pdata, DWCEQOS_MTL_OP_MODE);
	mac_rqc3r_val = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL3);
	mac_rqc2r_val = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL2);
	mac_rqc1r_val = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL1);
	mac_rqc0r_val = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL0);
	mac_tqpm1r_val = dwceqos_readl(pdata, DWCEQOS_MAC_TQPM1);
	mac_tqpm0r_val = dwceqos_readl(pdata, DWCEQOS_MAC_TQPM0);
	mac_rfcr_val = dwceqos_readl(pdata, DWCEQOS_MAC_RX_FLOW_CTRL);
	mac_qtfcr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(7));
	mac_qtfcr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(6));
	mac_qtfcr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(5));
	mac_qtfcr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(4));
	mac_qtfcr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(3));
	mac_qtfcr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(2));
	mac_qtfcr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(1));
	mac_q0tfcr_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(0));
	#if 0
	DMA_AXI4CR7_RgRd(dma_axi4cr7_val);
	DMA_AXI4CR6_RgRd(dma_axi4cr6_val);
	DMA_AXI4CR5_RgRd(dma_axi4cr5_val);
	DMA_AXI4CR4_RgRd(dma_axi4cr4_val);
	DMA_AXI4CR3_RgRd(dma_axi4cr3_val);
	DMA_AXI4CR2_RgRd(dma_axi4cr2_val);
	DMA_AXI4CR1_RgRd(dma_axi4cr1_val);
	DMA_AXI4CR0_RgRd(dma_axi4cr0_val);
	#endif
	dma_rcr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(7));
	dma_rcr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(6));
	dma_rcr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(5));
	dma_rcr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(4));
	dma_rcr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(3));
	dma_rcr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(2));
	dma_rcr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(1));
	dma_rcr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(0));
	dma_tcr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(7));
	dma_tcr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(6));
	dma_tcr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(5));
	dma_tcr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(4));
	dma_tcr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(3));
	dma_tcr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(2));
	dma_tcr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(1));
	dma_tcr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(0));
	dma_cr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(7));
	dma_cr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(6));
	dma_cr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(5));
	dma_cr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(4));
	dma_cr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(3));
	dma_cr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(2));
	dma_cr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(1));
	dma_cr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(0));
	mac_wtr_val = dwceqos_readl(pdata, DWCEQOS_MAC_WD_TIMEOUT);
	mac_mpfr_val = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	mac_mecr_val = dwceqos_readl(pdata, DWCEQOS_MAC_EXT_CFG);
	//MAC_MCR_RgRd(mac_mcr_val);
	mac_mcr_val = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	/* For MII/GMII register read */
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR,
				     &mac_bmcr_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMSR,
				     &mac_bmsr_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID1,
				     &mii_physid1_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID2,
				     &mii_physid2_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ADVERTISE,
				     &mii_advertise_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_LPA,
				     &mii_lpa_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_EXPANSION,
				     &mii_expansion_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr,
				     DWC_EQOS_AUTO_NEGO_NP,
				     &auto_nego_np_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ESTATUS,
				     &mii_estatus_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_CTRL1000,
				     &mii_ctrl1000_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_STAT1000,
				     &mii_stat1000_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, DWC_EQOS_PHY_CTL,
				     &phy_ctl_reg_val);
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, DWC_EQOS_PHY_STS,
				     &phy_sts_reg_val);

	debug_buf = (char *)kmalloc(26820, GFP_KERNEL);

	sprintf(debug_buf,
		"mac_ma32_127lr127            :%#x\n"
		"mac_ma32_127lr126            :%#x\n"
		"mac_ma32_127lr125            :%#x\n"
		"mac_ma32_127lr124            :%#x\n"
		"mac_ma32_127lr123            :%#x\n"
		"mac_ma32_127lr122            :%#x\n"
		"mac_ma32_127lr121            :%#x\n"
		"mac_ma32_127lr120            :%#x\n"
		"mac_ma32_127lr119            :%#x\n"
		"mac_ma32_127lr118            :%#x\n"
		"mac_ma32_127lr117            :%#x\n"
		"mac_ma32_127lr116            :%#x\n"
		"mac_ma32_127lr115            :%#x\n"
		"mac_ma32_127lr114            :%#x\n"
		"mac_ma32_127lr113            :%#x\n"
		"mac_ma32_127lr112            :%#x\n"
		"mac_ma32_127lr111            :%#x\n"
		"mac_ma32_127lr110            :%#x\n"
		"mac_ma32_127lr109            :%#x\n"
		"mac_ma32_127lr108            :%#x\n"
		"mac_ma32_127lr107            :%#x\n"
		"mac_ma32_127lr106            :%#x\n"
		"mac_ma32_127lr105            :%#x\n"
		"mac_ma32_127lr104            :%#x\n"
		"mac_ma32_127lr103            :%#x\n"
		"mac_ma32_127lr102            :%#x\n"
		"mac_ma32_127lr101            :%#x\n"
		"mac_ma32_127lr100            :%#x\n"
		"mac_ma32_127lr99            :%#x\n"
		"mac_ma32_127lr98            :%#x\n"
		"mac_ma32_127lr97            :%#x\n"
		"mac_ma32_127lr96            :%#x\n"
		"mac_ma32_127lr95            :%#x\n"
		"mac_ma32_127lr94            :%#x\n"
		"mac_ma32_127lr93            :%#x\n"
		"mac_ma32_127lr92            :%#x\n"
		"mac_ma32_127lr91            :%#x\n"
		"mac_ma32_127lr90            :%#x\n"
		"mac_ma32_127lr89            :%#x\n"
		"mac_ma32_127lr88            :%#x\n"
		"mac_ma32_127lr87            :%#x\n"
		"mac_ma32_127lr86            :%#x\n"
		"mac_ma32_127lr85            :%#x\n"
		"mac_ma32_127lr84            :%#x\n"
		"mac_ma32_127lr83            :%#x\n"
		"mac_ma32_127lr82            :%#x\n"
		"mac_ma32_127lr81            :%#x\n"
		"mac_ma32_127lr80            :%#x\n"
		"mac_ma32_127lr79            :%#x\n"
		"mac_ma32_127lr78            :%#x\n"
		"mac_ma32_127lr77            :%#x\n"
		"mac_ma32_127lr76            :%#x\n"
		"mac_ma32_127lr75            :%#x\n"
		"mac_ma32_127lr74            :%#x\n"
		"mac_ma32_127lr73            :%#x\n"
		"mac_ma32_127lr72            :%#x\n"
		"mac_ma32_127lr71            :%#x\n"
		"mac_ma32_127lr70            :%#x\n"
		"mac_ma32_127lr69            :%#x\n"
		"mac_ma32_127lr68            :%#x\n"
		"mac_ma32_127lr67            :%#x\n"
		"mac_ma32_127lr66            :%#x\n"
		"mac_ma32_127lr65            :%#x\n"
		"mac_ma32_127lr64            :%#x\n"
		"mac_ma32_127lr63            :%#x\n"
		"mac_ma32_127lr62            :%#x\n"
		"mac_ma32_127lr61            :%#x\n"
		"mac_ma32_127lr60            :%#x\n"
		"mac_ma32_127lr59            :%#x\n"
		"mac_ma32_127lr58            :%#x\n"
		"mac_ma32_127lr57            :%#x\n"
		"mac_ma32_127lr56            :%#x\n"
		"mac_ma32_127lr55            :%#x\n"
		"mac_ma32_127lr54            :%#x\n"
		"mac_ma32_127lr53            :%#x\n"
		"mac_ma32_127lr52            :%#x\n"
		"mac_ma32_127lr51            :%#x\n"
		"mac_ma32_127lr50            :%#x\n"
		"mac_ma32_127lr49            :%#x\n"
		"mac_ma32_127lr48            :%#x\n"
		"mac_ma32_127lr47            :%#x\n"
		"mac_ma32_127lr46            :%#x\n"
		"mac_ma32_127lr45            :%#x\n"
		"mac_ma32_127lr44            :%#x\n"
		"mac_ma32_127lr43            :%#x\n"
		"mac_ma32_127lr42            :%#x\n"
		"mac_ma32_127lr41            :%#x\n"
		"mac_ma32_127lr40            :%#x\n"
		"mac_ma32_127lr39            :%#x\n"
		"mac_ma32_127lr38            :%#x\n"
		"mac_ma32_127lr37            :%#x\n"
		"mac_ma32_127lr36            :%#x\n"
		"mac_ma32_127lr35            :%#x\n"
		"mac_ma32_127lr34            :%#x\n"
		"mac_ma32_127lr33            :%#x\n"
		"mac_ma32_127lr32            :%#x\n"
		"mac_ma32_127hr127            :%#x\n"
		"mac_ma32_127hr126            :%#x\n"
		"mac_ma32_127hr125            :%#x\n"
		"mac_ma32_127hr124            :%#x\n"
		"mac_ma32_127hr123            :%#x\n"
		"mac_ma32_127hr122            :%#x\n"
		"mac_ma32_127hr121            :%#x\n"
		"mac_ma32_127hr120            :%#x\n"
		"mac_ma32_127hr119            :%#x\n"
		"mac_ma32_127hr118            :%#x\n"
		"mac_ma32_127hr117            :%#x\n"
		"mac_ma32_127hr116            :%#x\n"
		"mac_ma32_127hr115            :%#x\n"
		"mac_ma32_127hr114            :%#x\n"
		"mac_ma32_127hr113            :%#x\n"
		"mac_ma32_127hr112            :%#x\n"
		"mac_ma32_127hr111            :%#x\n"
		"mac_ma32_127hr110            :%#x\n"
		"mac_ma32_127hr109            :%#x\n"
		"mac_ma32_127hr108            :%#x\n"
		"mac_ma32_127hr107            :%#x\n"
		"mac_ma32_127hr106            :%#x\n"
		"mac_ma32_127hr105            :%#x\n"
		"mac_ma32_127hr104            :%#x\n"
		"mac_ma32_127hr103            :%#x\n"
		"mac_ma32_127hr102            :%#x\n"
		"mac_ma32_127hr101            :%#x\n"
		"mac_ma32_127hr100            :%#x\n"
		"mac_ma32_127hr99            :%#x\n"
		"mac_ma32_127hr98            :%#x\n"
		"mac_ma32_127hr97            :%#x\n"
		"mac_ma32_127hr96            :%#x\n"
		"mac_ma32_127hr95            :%#x\n"
		"mac_ma32_127hr94            :%#x\n"
		"mac_ma32_127hr93            :%#x\n"
		"mac_ma32_127hr92            :%#x\n"
		"mac_ma32_127hr91            :%#x\n"
		"mac_ma32_127hr90            :%#x\n"
		"mac_ma32_127hr89            :%#x\n"
		"mac_ma32_127hr88            :%#x\n"
		"mac_ma32_127hr87            :%#x\n"
		"mac_ma32_127hr86            :%#x\n"
		"mac_ma32_127hr85            :%#x\n"
		"mac_ma32_127hr84            :%#x\n"
		"mac_ma32_127hr83            :%#x\n"
		"mac_ma32_127hr82            :%#x\n"
		"mac_ma32_127hr81            :%#x\n"
		"mac_ma32_127hr80            :%#x\n"
		"mac_ma32_127hr79            :%#x\n"
		"mac_ma32_127hr78            :%#x\n"
		"mac_ma32_127hr77            :%#x\n"
		"mac_ma32_127hr76            :%#x\n"
		"mac_ma32_127hr75            :%#x\n"
		"mac_ma32_127hr74            :%#x\n"
		"mac_ma32_127hr73            :%#x\n"
		"mac_ma32_127hr72            :%#x\n"
		"mac_ma32_127hr71            :%#x\n"
		"mac_ma32_127hr70            :%#x\n"
		"mac_ma32_127hr69            :%#x\n"
		"mac_ma32_127hr68            :%#x\n"
		"mac_ma32_127hr67            :%#x\n"
		"mac_ma32_127hr66            :%#x\n"
		"mac_ma32_127hr65            :%#x\n"
		"mac_ma32_127hr64            :%#x\n"
		"mac_ma32_127hr63            :%#x\n"
		"mac_ma32_127hr62            :%#x\n"
		"mac_ma32_127hr61            :%#x\n"
		"mac_ma32_127hr60            :%#x\n"
		"mac_ma32_127hr59            :%#x\n"
		"mac_ma32_127hr58            :%#x\n"
		"mac_ma32_127hr57            :%#x\n"
		"mac_ma32_127hr56            :%#x\n"
		"mac_ma32_127hr55            :%#x\n"
		"mac_ma32_127hr54            :%#x\n"
		"mac_ma32_127hr53            :%#x\n"
		"mac_ma32_127hr52            :%#x\n"
		"mac_ma32_127hr51            :%#x\n"
		"mac_ma32_127hr50            :%#x\n"
		"mac_ma32_127hr49            :%#x\n"
		"mac_ma32_127hr48            :%#x\n"
		"mac_ma32_127hr47            :%#x\n"
		"mac_ma32_127hr46            :%#x\n"
		"mac_ma32_127hr45            :%#x\n"
		"mac_ma32_127hr44            :%#x\n"
		"mac_ma32_127hr43            :%#x\n"
		"mac_ma32_127hr42            :%#x\n"
		"mac_ma32_127hr41            :%#x\n"
		"mac_ma32_127hr40            :%#x\n"
		"mac_ma32_127hr39            :%#x\n"
		"mac_ma32_127hr38            :%#x\n"
		"mac_ma32_127hr37            :%#x\n"
		"mac_ma32_127hr36            :%#x\n"
		"mac_ma32_127hr35            :%#x\n"
		"mac_ma32_127hr34            :%#x\n"
		"mac_ma32_127hr33            :%#x\n"
		"mac_ma32_127hr32            :%#x\n"
		"mac_ma1_31lr31              :%#x\n"
		"mac_ma1_31lr30              :%#x\n"
		"mac_ma1_31lr29              :%#x\n"
		"mac_ma1_31lr28              :%#x\n"
		"mac_ma1_31lr27              :%#x\n"
		"mac_ma1_31lr26              :%#x\n"
		"mac_ma1_31lr25              :%#x\n"
		"mac_ma1_31lr24              :%#x\n"
		"mac_ma1_31lr23              :%#x\n"
		"mac_ma1_31lr22              :%#x\n"
		"mac_ma1_31lr21              :%#x\n"
		"mac_ma1_31lr20              :%#x\n"
		"mac_ma1_31lr19              :%#x\n"
		"mac_ma1_31lr18              :%#x\n"
		"mac_ma1_31lr17              :%#x\n"
		"mac_ma1_31lr16              :%#x\n"
		"mac_ma1_31lr15              :%#x\n"
		"mac_ma1_31lr14              :%#x\n"
		"mac_ma1_31lr13              :%#x\n"
		"mac_ma1_31lr12              :%#x\n"
		"mac_ma1_31lr11              :%#x\n"
		"mac_ma1_31lr10              :%#x\n"
		"mac_ma1_31lr9              :%#x\n"
		"mac_ma1_31lr8              :%#x\n"
		"mac_ma1_31lr7              :%#x\n"
		"mac_ma1_31lr6              :%#x\n"
		"mac_ma1_31lr5              :%#x\n"
		"mac_ma1_31lr4              :%#x\n"
		"mac_ma1_31lr3              :%#x\n"
		"mac_ma1_31lr2              :%#x\n"
		"mac_ma1_31lr1              :%#x\n"
		"mac_ma1_31hr31              :%#x\n"
		"mac_ma1_31hr30              :%#x\n"
		"mac_ma1_31hr29              :%#x\n"
		"mac_ma1_31hr28              :%#x\n"
		"mac_ma1_31hr27              :%#x\n"
		"mac_ma1_31hr26              :%#x\n"
		"mac_ma1_31hr25              :%#x\n"
		"mac_ma1_31hr24              :%#x\n"
		"mac_ma1_31hr23              :%#x\n"
		"mac_ma1_31hr22              :%#x\n"
		"mac_ma1_31hr21              :%#x\n"
		"mac_ma1_31hr20              :%#x\n"
		"mac_ma1_31hr19              :%#x\n"
		"mac_ma1_31hr18              :%#x\n"
		"mac_ma1_31hr17              :%#x\n"
		"mac_ma1_31hr16              :%#x\n"
		"mac_ma1_31hr15              :%#x\n"
		"mac_ma1_31hr14              :%#x\n"
		"mac_ma1_31hr13              :%#x\n"
		"mac_ma1_31hr12              :%#x\n"
		"mac_ma1_31hr11              :%#x\n"
		"mac_ma1_31hr10              :%#x\n"
		"mac_ma1_31hr9              :%#x\n"
		"mac_ma1_31hr8              :%#x\n"
		"mac_ma1_31hr7              :%#x\n"
		"mac_ma1_31hr6              :%#x\n"
		"mac_ma1_31hr5              :%#x\n"
		"mac_ma1_31hr4              :%#x\n"
		"mac_ma1_31hr3              :%#x\n"
		"mac_ma1_31hr2              :%#x\n"
		"mac_ma1_31hr1              :%#x\n"
		"mac_arpa                   :%#x\n"
		"mac_l3a3r7                 :%#x\n"
		"mac_l3a3r6                 :%#x\n"
		"mac_l3a3r5                 :%#x\n"
		"mac_l3a3r4                 :%#x\n"
		"mac_l3a3r3                 :%#x\n"
		"mac_l3a3r2                 :%#x\n"
		"mac_l3a3r1                 :%#x\n"
		"mac_l3a3r0                 :%#x\n"
		"mac_l3a2r7                 :%#x\n"
		"mac_l3a2r6                 :%#x\n"
		"mac_l3a2r5                 :%#x\n"
		"mac_l3a2r4                 :%#x\n"
		"mac_l3a2r3                 :%#x\n"
		"mac_l3a2r2                 :%#x\n"
		"mac_l3a2r1                 :%#x\n"
		"mac_l3a2r0                 :%#x\n"
		"mac_l3a1r7                 :%#x\n"
		"mac_l3a1r6                 :%#x\n"
		"mac_l3a1r5                 :%#x\n"
		"mac_l3a1r4                 :%#x\n"
		"mac_l3a1r3                 :%#x\n"
		"mac_l3a1r2                 :%#x\n"
		"mac_l3a1r1                 :%#x\n"
		"mac_l3a1r0                 :%#x\n"
		"mac_l3a0r7                 :%#x\n"
		"mac_l3a0r6                 :%#x\n"
		"mac_l3a0r5                 :%#x\n"
		"mac_l3a0r4                 :%#x\n"
		"mac_l3a0r3                 :%#x\n"
		"mac_l3a0r2                 :%#x\n"
		"mac_l3a0r1                 :%#x\n"
		"mac_l3a0r0                 :%#x\n"
		"mac_l4ar7                 :%#x\n"
		"mac_l4ar6                 :%#x\n"
		"mac_l4ar5                 :%#x\n"
		"mac_l4ar4                 :%#x\n"
		"mac_l4ar3                 :%#x\n"
		"mac_l4ar2                 :%#x\n"
		"mac_l4ar1                 :%#x\n"
		"mac_l4ar0                 :%#x\n"
		"mac_l3l4cr7                :%#x\n"
		"mac_l3l4cr6                :%#x\n"
		"mac_l3l4cr5                :%#x\n"
		"mac_l3l4cr4                :%#x\n"
		"mac_l3l4cr3                :%#x\n"
		"mac_l3l4cr2                :%#x\n"
		"mac_l3l4cr1                :%#x\n"
		"mac_l3l4cr0                :%#x\n"
		"mac_gpio                  :%#x\n"
		"mac_pcs                    :%#x\n"
		"mac_tes                    :%#x\n"
		"mac_ae                     :%#x\n"
		"mac_alpa                   :%#x\n"
		"mac_aad                    :%#x\n"
		"mac_ans                    :%#x\n"
		"mac_anc                    :%#x\n"
		"mac_lpc                    :%#x\n"
		"mac_lps                    :%#x\n"
        "mac_lmir                   :%#x\n"
		"mac_spi2r                  :%#x\n"
		"mac_spi1r                  :%#x\n"
		"mac_spi0r                  :%#x\n"
		"mac_pto_cr                 :%#x\n"
		"mac_pps_width3             :%#x\n"
		"mac_pps_width2             :%#x\n"
		"mac_pps_width1             :%#x\n"
		"mac_pps_width0             :%#x\n"
		"mac_pps_intval3            :%#x\n"
		"mac_pps_intval2            :%#x\n"
		"mac_pps_intval1            :%#x\n"
		"mac_pps_intval0            :%#x\n"
		"mac_pps_ttns3              :%#x\n"
		"mac_pps_ttns2              :%#x\n"
		"mac_pps_ttns1              :%#x\n"
		"mac_pps_ttns0              :%#x\n"
		"mac_pps_tts3               :%#x\n"
		"mac_pps_tts2               :%#x\n"
		"mac_pps_tts1               :%#x\n"
		"mac_pps_tts0               :%#x\n"
		"mac_ppsc                   :%#x\n"
		"mac_teac                   :%#x\n"
		"mac_tiac                   :%#x\n"
		"mac_ats                    :%#x\n"
		"mac_atn                    :%#x\n"
		"mac_ac                     :%#x\n"
		"mac_ttn                    :%#x\n"
		"mac_ttsn                   :%#x\n"
		"mac_tsr                    :%#x\n"
		"mac_sthwr                  :%#x\n"
		"mac_tar                    :%#x\n"
		"mac_stnsur                 :%#x\n"
		"mac_stsur                  :%#x\n"
		"mac_stnsr                  :%#x\n"
		"mac_stsr                   :%#x\n"
		"mac_ssir                   :%#x\n"
		"mac_tcr                    :%#x\n"
		"mtl_dsr                    :%#x\n"
		"mac_rwpffr                 :%#x\n"
		"mac_rtsr                   :%#x\n"
		"mtl_ier                    :%#x\n"
		"mtl_qrcr7                  :%#x\n"
		"mtl_qrcr6                  :%#x\n"
		"mtl_qrcr5                  :%#x\n"
		"mtl_qrcr4                  :%#x\n"
		"mtl_qrcr3                  :%#x\n"
		"mtl_qrcr2                  :%#x\n"
		"mtl_qrcr1                  :%#x\n"
		"mtl_qrdr7                  :%#x\n"
		"mtl_qrdr6                  :%#x\n"
		"mtl_qrdr5                  :%#x\n"
		"mtl_qrdr4                  :%#x\n"
		"mtl_qrdr3                  :%#x\n"
		"mtl_qrdr2                  :%#x\n"
		"mtl_qrdr1                  :%#x\n"
		"mtl_qocr7                  :%#x\n"
		"mtl_qocr6                  :%#x\n"
		"mtl_qocr5                  :%#x\n"
		"mtl_qocr4                  :%#x\n"
		"mtl_qocr3                  :%#x\n"
		"mtl_qocr2                  :%#x\n"
		"mtl_qocr1                  :%#x\n"
		"mtl_qromr7                 :%#x\n"
		"mtl_qromr6                 :%#x\n"
		"mtl_qromr5                 :%#x\n"
		"mtl_qromr4                 :%#x\n"
		"mtl_qromr3                 :%#x\n"
		"mtl_qromr2                 :%#x\n"
		"mtl_qromr1                 :%#x\n"
		"mtl_qlcr7                  :%#x\n"
		"mtl_qlcr6                  :%#x\n"
		"mtl_qlcr5                  :%#x\n"
		"mtl_qlcr4                  :%#x\n"
		"mtl_qlcr3                  :%#x\n"
		"mtl_qlcr2                  :%#x\n"
		"mtl_qlcr1                  :%#x\n"
		"mtl_qhcr7                  :%#x\n"
		"mtl_qhcr6                  :%#x\n"
		"mtl_qhcr5                  :%#x\n"
		"mtl_qhcr4                  :%#x\n"
		"mtl_qhcr3                  :%#x\n"
		"mtl_qhcr2                  :%#x\n"
		"mtl_qhcr1                  :%#x\n"
		"mtl_qsscr7                 :%#x\n"
		"mtl_qsscr6                 :%#x\n"
		"mtl_qsscr5                 :%#x\n"
		"mtl_qsscr4                 :%#x\n"
		"mtl_qsscr3                 :%#x\n"
		"mtl_qsscr2                 :%#x\n"
		"mtl_qsscr1                 :%#x\n"
		"mtl_qw7                    :%#x\n"
		"mtl_qw6                    :%#x\n"
		"mtl_qw5                    :%#x\n"
		"mtl_qw4                    :%#x\n"
		"mtl_qw3                    :%#x\n"
		"mtl_qw2                    :%#x\n"
		"mtl_qw1                    :%#x\n"
		"mtl_qesr7                  :%#x\n"
		"mtl_qesr6                  :%#x\n"
		"mtl_qesr5                  :%#x\n"
		"mtl_qesr4                  :%#x\n"
		"mtl_qesr3                  :%#x\n"
		"mtl_qesr2                  :%#x\n"
		"mtl_qesr1                  :%#x\n"
		"mtl_qecr7                  :%#x\n"
		"mtl_qecr6                  :%#x\n"
		"mtl_qecr5                  :%#x\n"
		"mtl_qecr4                  :%#x\n"
		"mtl_qecr3                  :%#x\n"
		"mtl_qecr2                  :%#x\n"
		"mtl_qecr1                  :%#x\n"
		"mtl_qtdr7                  :%#x\n"
		"mtl_qtdr6                  :%#x\n"
		"mtl_qtdr5                  :%#x\n"
		"mtl_qtdr4                  :%#x\n"
		"mtl_qtdr3                  :%#x\n"
		"mtl_qtdr2                  :%#x\n"
		"mtl_qtdr1                  :%#x\n"
		"mtl_qucr7                  :%#x\n"
		"mtl_qucr6                  :%#x\n"
		"mtl_qucr5                  :%#x\n"
		"mtl_qucr4                  :%#x\n"
		"mtl_qucr3                  :%#x\n"
		"mtl_qucr2                  :%#x\n"
		"mtl_qucr1                  :%#x\n"
		"mtl_qtomr7                 :%#x\n"
		"mtl_qtomr6                 :%#x\n"
		"mtl_qtomr5                 :%#x\n"
		"mtl_qtomr4                 :%#x\n"
		"mtl_qtomr3                 :%#x\n"
		"mtl_qtomr2                 :%#x\n"
		"mtl_qtomr1                 :%#x\n"
		"mac_pmtcsr                 :%#x\n"
		"mmc_rxicmp_err_octets      :%#x\n"
		"mmc_rxicmp_gd_octets       :%#x\n"
		"mmc_rxtcp_err_octets       :%#x\n"
		"mmc_rxtcp_gd_octets        :%#x\n"
		"mmc_rxudp_err_octets       :%#x\n"
		"mmc_rxudp_gd_octets        :%#x\n"
		"mmc_rxipv6_nopay_octets    :%#x\n"
		"mmc_rxipv6_hdrerr_octets   :%#x\n"
		"mmc_rxipv6_gd_octets       :%#x\n"
		"mmc_rxipv4_udsbl_octets    :%#x\n"
		"mmc_rxipv4_frag_octets     :%#x\n"
		"mmc_rxipv4_nopay_octets    :%#x\n"
		"mmc_rxipv4_hdrerr_octets   :%#x\n"
		"mmc_rxipv4_gd_octets       :%#x\n"
		"mmc_rxicmp_err_pkts        :%#x\n"
		"mmc_rxicmp_gd_pkts         :%#x\n"
		"mmc_rxtcp_err_pkts         :%#x\n"
		"mmc_rxtcp_gd_pkts          :%#x\n"
		"mmc_rxudp_err_pkts         :%#x\n"
		"mmc_rxudp_gd_pkts          :%#x\n"
		"mmc_rxipv6_nopay_pkts      :%#x\n"
		"mmc_rxipv6_hdrerr_pkts     :%#x\n"
		"mmc_rxipv6_gd_pkts         :%#x\n"
		"mmc_rxipv4_ubsbl_pkts      :%#x\n"
		"mmc_rxipv4_frag_pkts       :%#x\n"
		"mmc_rxipv4_nopay_pkts      :%#x\n"
		"mmc_rxipv4_hdrerr_pkts     :%#x\n"
		"mmc_rxipv4_gd_pkts         :%#x\n"
		"mmc_rxctrlpackets_g        :%#x\n"
		"mmc_rxrcverror             :%#x\n"
		"mmc_rxwatchdogerror        :%#x\n"
		"mmc_rxvlanpackets_gb       :%#x\n"
		"mmc_rxfifooverflow         :%#x\n"
		"mmc_rxpausepackets         :%#x\n"
		"mmc_rxoutofrangetype       :%#x\n"
		"mmc_rxlengtherror          :%#x\n"
		"mmc_rxunicastpackets_g     :%#x\n"
		"mmc_rx1024tomaxoctets_gb   :%#x\n"
		"mmc_rx512to1023octets_gb   :%#x\n"
		"mmc_rx256to511octets_gb    :%#x\n"
		"mmc_rx128to255octets_gb    :%#x\n"
		"mmc_rx65to127octets_gb     :%#x\n"
		"mmc_rx64octets_gb          :%#x\n"
		"mmc_rxoversize_g           :%#x\n"
		"mmc_rxundersize_g          :%#x\n"
		"mmc_rxjabbererror          :%#x\n"
		"mmc_rxrunterror            :%#x\n"
		"mmc_rxalignmenterror       :%#x\n"
		"mmc_rxcrcerror             :%#x\n"
		"mmc_rxmulticastpackets_g   :%#x\n"
		"mmc_rxbroadcastpackets_g   :%#x\n"
		"mmc_rxoctetcount_g         :%#x\n"
		"mmc_rxoctetcount_gb        :%#x\n"
		"mmc_rxpacketcount_gb       :%#x\n"
		"mmc_txoversize_g           :%#x\n"
		"mmc_txvlanpackets_g        :%#x\n"
		"mmc_txpausepackets         :%#x\n"
		"mmc_txexcessdef            :%#x\n"
		"mmc_txpacketscount_g       :%#x\n"
		"mmc_txoctetcount_g         :%#x\n"
		"mmc_txcarriererror         :%#x\n"
		"mmc_txexesscol             :%#x\n"
		"mmc_txlatecol              :%#x\n"
		"mmc_txdeferred             :%#x\n"
		"mmc_txmulticol_g           :%#x\n"
		"mmc_txsinglecol_g          :%#x\n"
		"mmc_txunderflowerror       :%#x\n"
		"mmc_txbroadcastpackets_gb  :%#x\n"
		"mmc_txmulticastpackets_gb  :%#x\n"
		"mmc_txunicastpackets_gb    :%#x\n"
		"mmc_tx1024tomaxoctets_gb   :%#x\n"
		"mmc_tx512to1023octets_gb   :%#x\n"
		"mmc_tx256to511octets_gb    :%#x\n"
		"mmc_tx128to255octets_gb    :%#x\n"
		"mmc_tx65to127octets_gb     :%#x\n"
		"mmc_tx64octets_gb          :%#x\n"
		"mmc_txmulticastpackets_g   :%#x\n"
		"mmc_txbroadcastpackets_g   :%#x\n"
		"mmc_txpacketcount_gb       :%#x\n"
		"mmc_txoctetcount_gb        :%#x\n"
		"mmc_ipc_intr_rx            :%#x\n"
		"mmc_ipc_intr_mask_rx       :%#x\n"
		"mmc_intr_mask_tx           :%#x\n"
		"mmc_intr_mask_rx           :%#x\n"
		"mmc_intr_tx                :%#x\n"
		"mmc_intr_rx                :%#x\n"
		"mmc_cntrl                  :%#x\n"
		"mac_ma1lr                  :%#x\n"
		"mac_ma1hr                  :%#x\n"
		"mac_ma0lr                  :%#x\n"
		"mac_ma0hr                  :%#x\n"
		"mac_gpior                  :%#x\n"
		"mac_gmiidr                 :%#x\n"
		"mac_gmiiar                 :%#x\n"
		"mac_hfr2                   :%#x\n"
		"mac_hfr1                   :%#x\n"
		"mac_hfr0                   :%#x\n"
		"mac_mdr                    :%#x\n"
		"mac_vr                     :%#x\n"
		"mac_htr7                   :%#x\n"
		"mac_htr6                   :%#x\n"
		"mac_htr5                   :%#x\n"
		"mac_htr4                   :%#x\n"
		"mac_htr3                   :%#x\n"
		"mac_htr2                   :%#x\n"
		"mac_htr1                   :%#x\n"
		"mac_htr0                   :%#x\n"
		"dma_riwtr7                 :%#x\n"
		"dma_riwtr6                 :%#x\n"
		"dma_riwtr5                 :%#x\n"
		"dma_riwtr4                 :%#x\n"
		"dma_riwtr3                 :%#x\n"
		"dma_riwtr2                 :%#x\n"
		"dma_riwtr1                 :%#x\n"
		"dma_riwtr0                 :%#x\n"
		"dma_rdrlr7                 :%#x\n"
		"dma_rdrlr6                 :%#x\n"
		"dma_rdrlr5                 :%#x\n"
		"dma_rdrlr4                 :%#x\n"
		"dma_rdrlr3                 :%#x\n"
		"dma_rdrlr2                 :%#x\n"
		"dma_rdrlr1                 :%#x\n"
		"dma_rdrlr0                 :%#x\n"
		"dma_tdrlr7                 :%#x\n"
		"dma_tdrlr6                 :%#x\n"
		"dma_tdrlr5                 :%#x\n"
		"dma_tdrlr4                 :%#x\n"
		"dma_tdrlr3                 :%#x\n"
		"dma_tdrlr2                 :%#x\n"
		"dma_tdrlr1                 :%#x\n"
		"dma_tdrlr0                 :%#x\n"
		"dma_rdtp_rpdr7             :%#x\n"
		"dma_rdtp_rpdr6             :%#x\n"
		"dma_rdtp_rpdr5             :%#x\n"
		"dma_rdtp_rpdr4             :%#x\n"
		"dma_rdtp_rpdr3             :%#x\n"
		"dma_rdtp_rpdr2             :%#x\n"
		"dma_rdtp_rpdr1             :%#x\n"
		"dma_rdtp_rpdr0             :%#x\n"
		"dma_tdtp_tpdr7             :%#x\n"
		"dma_tdtp_tpdr6             :%#x\n"
		"dma_tdtp_tpdr5             :%#x\n"
		"dma_tdtp_tpdr4             :%#x\n"
		"dma_tdtp_tpdr3             :%#x\n"
		"dma_tdtp_tpdr2             :%#x\n"
		"dma_tdtp_tpdr1             :%#x\n"
		"dma_tdtp_tpdr0             :%#x\n"
		"dma_rdlar7                 :%#x\n"
		"dma_rdlar6                 :%#x\n"
		"dma_rdlar5                 :%#x\n"
		"dma_rdlar4                 :%#x\n"
		"dma_rdlar3                 :%#x\n"
		"dma_rdlar2                 :%#x\n"
		"dma_rdlar1                 :%#x\n"
		"dma_rdlar0                 :%#x\n"
		"dma_tdlar7                 :%#x\n"
		"dma_tdlar6                 :%#x\n"
		"dma_tdlar5                 :%#x\n"
		"dma_tdlar4                 :%#x\n"
		"dma_tdlar3                 :%#x\n"
		"dma_tdlar2                 :%#x\n"
		"dma_tdlar1                 :%#x\n"
		"dma_tdlar0                 :%#x\n"
		"dma_ier7                   :%#x\n"
		"dma_ier6                   :%#x\n"
		"dma_ier5                   :%#x\n"
		"dma_ier4                   :%#x\n"
		"dma_ier3                   :%#x\n"
		"dma_ier2                   :%#x\n"
		"dma_ier1                   :%#x\n"
		"dma_ier0                   :%#x\n"
		"mac_imr                    :%#x\n"
		"mac_isr                    :%#x\n"
		"mtl_isr                    :%#x\n"
		"dma_sr7                    :%#x\n"
		"dma_sr6                    :%#x\n"
		"dma_sr5                    :%#x\n"
		"dma_sr4                    :%#x\n"
		"dma_sr3                    :%#x\n"
		"dma_sr2                    :%#x\n"
		"dma_sr1                    :%#x\n"
		"dma_sr0                    :%#x\n"
		"dma_isr                    :%#x\n"
		"dma_dsr2                   :%#x\n"
		"dma_dsr1                   :%#x\n"
		"dma_dsr0                   :%#x\n"
		"mtl_q0rdr                  :%#x\n"
		"mtl_q0esr                  :%#x\n"
		"mtl_q0tdr                  :%#x\n"
		"dma_chrbar7                :%#x\n"
		"dma_chrbar6                :%#x\n"
		"dma_chrbar5                :%#x\n"
		"dma_chrbar4                :%#x\n"
		"dma_chrbar3                :%#x\n"
		"dma_chrbar2                :%#x\n"
		"dma_chrbar1                :%#x\n"
		"dma_chrbar0                :%#x\n"
		"dma_chtbar7                :%#x\n"
		"dma_chtbar6                :%#x\n"
		"dma_chtbar5                :%#x\n"
		"dma_chtbar4                :%#x\n"
		"dma_chtbar3                :%#x\n"
		"dma_chtbar2                :%#x\n"
		"dma_chtbar1                :%#x\n"
		"dma_chtbar0                :%#x\n"
		"dma_chrdr7                 :%#x\n"
		"dma_chrdr6                 :%#x\n"
		"dma_chrdr5                 :%#x\n"
		"dma_chrdr4                 :%#x\n"
		"dma_chrdr3                 :%#x\n"
		"dma_chrdr2                 :%#x\n"
		"dma_chrdr1                 :%#x\n"
		"dma_chrdr0                 :%#x\n"
		"dma_chtdr7                 :%#x\n"
		"dma_chtdr6                 :%#x\n"
		"dma_chtdr5                 :%#x\n"
		"dma_chtdr4                 :%#x\n"
		"dma_chtdr3                 :%#x\n"
		"dma_chtdr2                 :%#x\n"
		"dma_chtdr1                 :%#x\n"
		"dma_chtdr0                 :%#x\n"
		"dma_sfcsr7                 :%#x\n"
		"dma_sfcsr6                 :%#x\n"
		"dma_sfcsr5                 :%#x\n"
		"dma_sfcsr4                 :%#x\n"
		"dma_sfcsr3                 :%#x\n"
		"dma_sfcsr2                 :%#x\n"
		"dma_sfcsr1                 :%#x\n"
		"dma_sfcsr0                 :%#x\n"
		"mac_ivlantirr              :%#x\n"
		"mac_vlantirr               :%#x\n"
		"mac_vlanhtr                :%#x\n"
		"mac_vlantr                 :%#x\n"
		"dma_sbus                   :%#x\n"
		"dma_bmr                    :%#x\n"
		"mtl_q0rcr                  :%#x\n"
		"mtl_q0ocr                  :%#x\n"
		"mtl_q0romr                 :%#x\n"
		"mtl_q0qr                   :%#x\n"
		"mtl_q0ecr                  :%#x\n"
		"mtl_q0ucr                  :%#x\n"
		"mtl_q0tomr                 :%#x\n"
		"mtl_rqdcm1r                :%#x\n"
		"mtl_rqdcm0r                :%#x\n"
		"mtl_fddr                   :%#x\n"
		"mtl_fdacs                  :%#x\n"
		"mtl_omr                    :%#x\n"
		"mac_rqc3r                  :%#x\n"
		"mac_rqc2r                  :%#x\n"
		"mac_rqc1r                  :%#x\n"
		"mac_rqc0r                  :%#x\n"
		"mac_tqpm1r                 :%#x\n"
		"mac_tqpm0r                 :%#x\n"
		"mac_rfcr                   :%#x\n"
		"mac_qtfcr7                 :%#x\n"
		"mac_qtfcr6                 :%#x\n"
		"mac_qtfcr5                 :%#x\n"
		"mac_qtfcr4                 :%#x\n"
		"mac_qtfcr3                 :%#x\n"
		"mac_qtfcr2                 :%#x\n"
		"mac_qtfcr1                 :%#x\n"
		"mac_q0tfcr                 :%#x\n"
		"dma_axi4cr7                :%#x\n"
		"dma_axi4cr6                :%#x\n"
		"dma_axi4cr5                :%#x\n"
		"dma_axi4cr4                :%#x\n"
		"dma_axi4cr3                :%#x\n"
		"dma_axi4cr2                :%#x\n"
		"dma_axi4cr1                :%#x\n"
		"dma_axi4cr0                :%#x\n"
		"dma_rcr7                   :%#x\n"
		"dma_rcr6                   :%#x\n"
		"dma_rcr5                   :%#x\n"
		"dma_rcr4                   :%#x\n"
		"dma_rcr3                   :%#x\n"
		"dma_rcr2                   :%#x\n"
		"dma_rcr1                   :%#x\n"
		"dma_rcr0                   :%#x\n"
		"dma_tcr7                   :%#x\n"
		"dma_tcr6                   :%#x\n"
		"dma_tcr5                   :%#x\n"
		"dma_tcr4                   :%#x\n"
		"dma_tcr3                   :%#x\n"
		"dma_tcr2                   :%#x\n"
		"dma_tcr1                   :%#x\n"
		"dma_tcr0                   :%#x\n"
		"dma_cr7                    :%#x\n"
		"dma_cr6                    :%#x\n"
		"dma_cr5                    :%#x\n"
		"dma_cr4                    :%#x\n"
		"dma_cr3                    :%#x\n"
		"dma_cr2                    :%#x\n"
		"dma_cr1                    :%#x\n"
		"dma_cr0                    :%#x\n"
		"mac_wtr                    :%#x\n"
		"mac_mpfr                   :%#x\n"
		"mac_mecr                   :%#x\n"
		"mac_mcr                    :%#x\n"
		"\nMII/GMII Registers\n\n"
		"Phy Control Reg(Basic Mode Control Reg)   : %#x\n"
		"Phy Status Reg(Basic Mode Status Reg)     : %#x\n"
		"Phy Id (PHYS ID 1)                        : %#x\n"
		"Phy Id (PHYS ID 2)                        : %#x\n"
		"Auto-nego Adv (Advertisement Control Reg) : %#x\n"
		"Auto-nego Lap (Link Partner Ability Reg)  : %#x\n"
		"Auto-nego Exp (Extension Reg)             : %#x\n"
		"Auto-nego Np                              : %#x\n"
		"Extended Status Reg                       : %#x\n"
		"1000 Ctl Reg (1000BASE-T Control Reg)     : %#x\n"
		"1000 Sts Reg (1000BASE-T Status)          : %#x\n"
		"PHY Ctl Reg                               : %#x\n"
		"PHY Sts Reg                               : %#x\n"
		"\n IP Features/Properties\n\n"
		"feature_drop_tx_pktburstcnt		   : %#x\n",
		mac_ma32_127lr127_val,
		mac_ma32_127lr126_val,
		mac_ma32_127lr125_val,
		mac_ma32_127lr124_val,
		mac_ma32_127lr123_val,
		mac_ma32_127lr122_val,
		mac_ma32_127lr121_val,
		mac_ma32_127lr120_val,
		mac_ma32_127lr119_val,
		mac_ma32_127lr118_val,
		mac_ma32_127lr117_val,
		mac_ma32_127lr116_val,
		mac_ma32_127lr115_val,
		mac_ma32_127lr114_val,
		mac_ma32_127lr113_val,
		mac_ma32_127lr112_val,
		mac_ma32_127lr111_val,
		mac_ma32_127lr110_val,
		mac_ma32_127lr109_val,
		mac_ma32_127lr108_val,
		mac_ma32_127lr107_val,
		mac_ma32_127lr106_val,
		mac_ma32_127lr105_val,
		mac_ma32_127lr104_val,
		mac_ma32_127lr103_val,
		mac_ma32_127lr102_val,
		mac_ma32_127lr101_val,
		mac_ma32_127lr100_val,
		mac_ma32_127lr99_val,
		mac_ma32_127lr98_val,
		mac_ma32_127lr97_val,
		mac_ma32_127lr96_val,
		mac_ma32_127lr95_val,
		mac_ma32_127lr94_val,
		mac_ma32_127lr93_val,
		mac_ma32_127lr92_val,
		mac_ma32_127lr91_val,
		mac_ma32_127lr90_val,
		mac_ma32_127lr89_val,
		mac_ma32_127lr88_val,
		mac_ma32_127lr87_val,
		mac_ma32_127lr86_val,
		mac_ma32_127lr85_val,
		mac_ma32_127lr84_val,
		mac_ma32_127lr83_val,
		mac_ma32_127lr82_val,
		mac_ma32_127lr81_val,
		mac_ma32_127lr80_val,
		mac_ma32_127lr79_val,
		mac_ma32_127lr78_val,
		mac_ma32_127lr77_val,
		mac_ma32_127lr76_val,
		mac_ma32_127lr75_val,
		mac_ma32_127lr74_val,
		mac_ma32_127lr73_val,
		mac_ma32_127lr72_val,
		mac_ma32_127lr71_val,
		mac_ma32_127lr70_val,
		mac_ma32_127lr69_val,
		mac_ma32_127lr68_val,
		mac_ma32_127lr67_val,
		mac_ma32_127lr66_val,
		mac_ma32_127lr65_val,
		mac_ma32_127lr64_val,
		mac_ma32_127lr63_val,
		mac_ma32_127lr62_val,
		mac_ma32_127lr61_val,
		mac_ma32_127lr60_val,
		mac_ma32_127lr59_val,
		mac_ma32_127lr58_val,
		mac_ma32_127lr57_val,
		mac_ma32_127lr56_val,
		mac_ma32_127lr55_val,
		mac_ma32_127lr54_val,
		mac_ma32_127lr53_val,
		mac_ma32_127lr52_val,
		mac_ma32_127lr51_val,
		mac_ma32_127lr50_val,
		mac_ma32_127lr49_val,
		mac_ma32_127lr48_val,
		mac_ma32_127lr47_val,
		mac_ma32_127lr46_val,
		mac_ma32_127lr45_val,
		mac_ma32_127lr44_val,
		mac_ma32_127lr43_val,
		mac_ma32_127lr42_val,
		mac_ma32_127lr41_val,
		mac_ma32_127lr40_val,
		mac_ma32_127lr39_val,
		mac_ma32_127lr38_val,
		mac_ma32_127lr37_val,
		mac_ma32_127lr36_val,
		mac_ma32_127lr35_val,
		mac_ma32_127lr34_val,
		mac_ma32_127lr33_val,
		mac_ma32_127lr32_val,
		mac_ma32_127hr127_val,
		mac_ma32_127hr126_val,
		mac_ma32_127hr125_val,
		mac_ma32_127hr124_val,
		mac_ma32_127hr123_val,
		mac_ma32_127hr122_val,
		mac_ma32_127hr121_val,
		mac_ma32_127hr120_val,
		mac_ma32_127hr119_val,
		mac_ma32_127hr118_val,
		mac_ma32_127hr117_val,
		mac_ma32_127hr116_val,
		mac_ma32_127hr115_val,
		mac_ma32_127hr114_val,
		mac_ma32_127hr113_val,
		mac_ma32_127hr112_val,
		mac_ma32_127hr111_val,
		mac_ma32_127hr110_val,
		mac_ma32_127hr109_val,
		mac_ma32_127hr108_val,
		mac_ma32_127hr107_val,
		mac_ma32_127hr106_val,
		mac_ma32_127hr105_val,
		mac_ma32_127hr104_val,
		mac_ma32_127hr103_val,
		mac_ma32_127hr102_val,
		mac_ma32_127hr101_val,
		mac_ma32_127hr100_val,
		mac_ma32_127hr99_val,
		mac_ma32_127hr98_val,
		mac_ma32_127hr97_val,
		mac_ma32_127hr96_val,
		mac_ma32_127hr95_val,
		mac_ma32_127hr94_val,
		mac_ma32_127hr93_val,
		mac_ma32_127hr92_val,
		mac_ma32_127hr91_val,
		mac_ma32_127hr90_val,
		mac_ma32_127hr89_val,
		mac_ma32_127hr88_val,
		mac_ma32_127hr87_val,
		mac_ma32_127hr86_val,
		mac_ma32_127hr85_val,
		mac_ma32_127hr84_val,
		mac_ma32_127hr83_val,
		mac_ma32_127hr82_val,
		mac_ma32_127hr81_val,
		mac_ma32_127hr80_val,
		mac_ma32_127hr79_val,
		mac_ma32_127hr78_val,
		mac_ma32_127hr77_val,
		mac_ma32_127hr76_val,
		mac_ma32_127hr75_val,
		mac_ma32_127hr74_val,
		mac_ma32_127hr73_val,
		mac_ma32_127hr72_val,
		mac_ma32_127hr71_val,
		mac_ma32_127hr70_val,
		mac_ma32_127hr69_val,
		mac_ma32_127hr68_val,
		mac_ma32_127hr67_val,
		mac_ma32_127hr66_val,
		mac_ma32_127hr65_val,
		mac_ma32_127hr64_val,
		mac_ma32_127hr63_val,
		mac_ma32_127hr62_val,
		mac_ma32_127hr61_val,
		mac_ma32_127hr60_val,
		mac_ma32_127hr59_val,
		mac_ma32_127hr58_val,
		mac_ma32_127hr57_val,
		mac_ma32_127hr56_val,
		mac_ma32_127hr55_val,
		mac_ma32_127hr54_val,
		mac_ma32_127hr53_val,
		mac_ma32_127hr52_val,
		mac_ma32_127hr51_val,
		mac_ma32_127hr50_val,
		mac_ma32_127hr49_val,
		mac_ma32_127hr48_val,
		mac_ma32_127hr47_val,
		mac_ma32_127hr46_val,
		mac_ma32_127hr45_val,
		mac_ma32_127hr44_val,
		mac_ma32_127hr43_val,
		mac_ma32_127hr42_val,
		mac_ma32_127hr41_val,
		mac_ma32_127hr40_val,
		mac_ma32_127hr39_val,
		mac_ma32_127hr38_val,
		mac_ma32_127hr37_val,
		mac_ma32_127hr36_val,
		mac_ma32_127hr35_val,
		mac_ma32_127hr34_val,
		mac_ma32_127hr33_val,
		mac_ma32_127hr32_val,
		mac_ma1_31lr31_val,
		mac_ma1_31lr30_val,
		mac_ma1_31lr29_val,
		mac_ma1_31lr28_val,
		mac_ma1_31lr27_val,
		mac_ma1_31lr26_val,
		mac_ma1_31lr25_val,
		mac_ma1_31lr24_val,
		mac_ma1_31lr23_val,
		mac_ma1_31lr22_val,
		mac_ma1_31lr21_val,
		mac_ma1_31lr20_val,
		mac_ma1_31lr19_val,
		mac_ma1_31lr18_val,
		mac_ma1_31lr17_val,
		mac_ma1_31lr16_val,
		mac_ma1_31lr15_val,
		mac_ma1_31lr14_val,
		mac_ma1_31lr13_val,
		mac_ma1_31lr12_val,
		mac_ma1_31lr11_val,
		mac_ma1_31lr10_val,
		mac_ma1_31lr9_val,
		mac_ma1_31lr8_val,
		mac_ma1_31lr7_val,
		mac_ma1_31lr6_val,
		mac_ma1_31lr5_val,
		mac_ma1_31lr4_val,
		mac_ma1_31lr3_val,
		mac_ma1_31lr2_val,
		mac_ma1_31lr1_val,
		mac_ma1_31hr31_val,
		mac_ma1_31hr30_val,
		mac_ma1_31hr29_val,
		mac_ma1_31hr28_val,
		mac_ma1_31hr27_val,
		mac_ma1_31hr26_val,
		mac_ma1_31hr25_val,
		mac_ma1_31hr24_val,
		mac_ma1_31hr23_val,
		mac_ma1_31hr22_val,
		mac_ma1_31hr21_val,
		mac_ma1_31hr20_val,
		mac_ma1_31hr19_val,
		mac_ma1_31hr18_val,
		mac_ma1_31hr17_val,
		mac_ma1_31hr16_val,
		mac_ma1_31hr15_val,
		mac_ma1_31hr14_val,
		mac_ma1_31hr13_val,
		mac_ma1_31hr12_val,
		mac_ma1_31hr11_val,
		mac_ma1_31hr10_val,
		mac_ma1_31hr9_val,
		mac_ma1_31hr8_val,
		mac_ma1_31hr7_val,
		mac_ma1_31hr6_val,
		mac_ma1_31hr5_val,
		mac_ma1_31hr4_val,
		mac_ma1_31hr3_val,
		mac_ma1_31hr2_val,
		mac_ma1_31hr1_val,
		mac_arpa_val,
		mac_l3a3r7_val,
		mac_l3a3r6_val,
		mac_l3a3r5_val,
		mac_l3a3r4_val,
		mac_l3a3r3_val,
		mac_l3a3r2_val,
		mac_l3a3r1_val,
		mac_l3a3r0_val,
		mac_l3a2r7_val,
		mac_l3a2r6_val,
		mac_l3a2r5_val,
		mac_l3a2r4_val,
		mac_l3a2r3_val,
		mac_l3a2r2_val,
		mac_l3a2r1_val,
		mac_l3a2r0_val,
		mac_l3a1r7_val,
		mac_l3a1r6_val,
		mac_l3a1r5_val,
		mac_l3a1r4_val,
		mac_l3a1r3_val,
		mac_l3a1r2_val,
		mac_l3a1r1_val,
		mac_l3a1r0_val,
		mac_l3a0r7_val,
		mac_l3a0r6_val,
		mac_l3a0r5_val,
		mac_l3a0r4_val,
		mac_l3a0r3_val,
		mac_l3a0r2_val,
		mac_l3a0r1_val,
		mac_l3a0r0_val,
		mac_l4ar7_val,
		mac_l4ar6_val,
		mac_l4ar5_val,
		mac_l4ar4_val,
		mac_l4ar3_val,
		mac_l4ar2_val,
		mac_l4ar1_val,
		mac_l4ar0_val,
		mac_l3l4cr7_val,
		mac_l3l4cr6_val,
		mac_l3l4cr5_val,
		mac_l3l4cr4_val,
		mac_l3l4cr3_val,
		mac_l3l4cr2_val,
		mac_l3l4cr1_val,
		mac_l3l4cr0_val,
		mac_gpio_val,
		mac_pcs_val,
		mac_tes_val,
		mac_ae_val,
		mac_alpa_val,
		mac_aad_val,
		mac_ans_val,
		mac_anc_val,
		mac_lpc_val,
		mac_lps_val,
		mac_lmir_val,
		mac_spi2r_val,
		mac_spi1r_val,
		mac_spi0r_val,
		mac_pto_cr_val,
		mac_pps_width3_val,
		mac_pps_width2_val,
		mac_pps_width1_val,
		mac_pps_width0_val,
		mac_pps_intval3_val,
		mac_pps_intval2_val,
		mac_pps_intval1_val,
		mac_pps_intval0_val,
		mac_pps_ttns3_val,
		mac_pps_ttns2_val,
		mac_pps_ttns1_val,
		mac_pps_ttns0_val,
		mac_pps_tts3_val,
		mac_pps_tts2_val,
		mac_pps_tts1_val,
		mac_pps_tts0_val,
		mac_ppsc_val,
		mac_teac_val,
		mac_tiac_val,
		mac_ats_val,
		mac_atn_val,
		mac_ac_val,
		mac_ttn_val,
		mac_ttsn_val,
		mac_tsr_val,
		mac_sthwr_val,
		mac_tar_val,
		mac_stnsur_val,
		mac_stsur_val,
		mac_stnsr_val,
		mac_stsr_val,
		mac_ssir_val,
		mac_tcr_val,
		mtl_dsr_val,
		mac_rwpffr_val,
		mac_rtsr_val,
		mtl_ier_val,
		mtl_qrcr7_val,
		mtl_qrcr6_val,
		mtl_qrcr5_val,
		mtl_qrcr4_val,
		mtl_qrcr3_val,
		mtl_qrcr2_val,
		mtl_qrcr1_val,
		mtl_qrdr7_val,
		mtl_qrdr6_val,
		mtl_qrdr5_val,
		mtl_qrdr4_val,
		mtl_qrdr3_val,
		mtl_qrdr2_val,
		mtl_qrdr1_val,
		mtl_qocr7_val,
		mtl_qocr6_val,
		mtl_qocr5_val,
		mtl_qocr4_val,
		mtl_qocr3_val,
		mtl_qocr2_val,
		mtl_qocr1_val,
		mtl_qromr7_val,
		mtl_qromr6_val,
		mtl_qromr5_val,
		mtl_qromr4_val,
		mtl_qromr3_val,
		mtl_qromr2_val,
		mtl_qromr1_val,
		mtl_qlcr7_val,
		mtl_qlcr6_val,
		mtl_qlcr5_val,
		mtl_qlcr4_val,
		mtl_qlcr3_val,
		mtl_qlcr2_val,
		mtl_qlcr1_val,
		mtl_qhcr7_val,
		mtl_qhcr6_val,
		mtl_qhcr5_val,
		mtl_qhcr4_val,
		mtl_qhcr3_val,
		mtl_qhcr2_val,
		mtl_qhcr1_val,
		mtl_qsscr7_val,
		mtl_qsscr6_val,
		mtl_qsscr5_val,
		mtl_qsscr4_val,
		mtl_qsscr3_val,
		mtl_qsscr2_val,
		mtl_qsscr1_val,
		mtl_qw7_val,
		mtl_qw6_val,
		mtl_qw5_val,
		mtl_qw4_val,
		mtl_qw3_val,
		mtl_qw2_val,
		mtl_qw1_val,
		mtl_qesr7_val,
		mtl_qesr6_val,
		mtl_qesr5_val,
		mtl_qesr4_val,
		mtl_qesr3_val,
		mtl_qesr2_val,
		mtl_qesr1_val,
		mtl_qecr7_val,
		mtl_qecr6_val,
		mtl_qecr5_val,
		mtl_qecr4_val,
		mtl_qecr3_val,
		mtl_qecr2_val,
		mtl_qecr1_val,
		mtl_qtdr7_val,
		mtl_qtdr6_val,
		mtl_qtdr5_val,
		mtl_qtdr4_val,
		mtl_qtdr3_val,
		mtl_qtdr2_val,
		mtl_qtdr1_val,
		mtl_qucr7_val,
		mtl_qucr6_val,
		mtl_qucr5_val,
		mtl_qucr4_val,
		mtl_qucr3_val,
		mtl_qucr2_val,
		mtl_qucr1_val,
		mtl_qtomr7_val,
		mtl_qtomr6_val,
		mtl_qtomr5_val,
		mtl_qtomr4_val,
		mtl_qtomr3_val,
		mtl_qtomr2_val,
		mtl_qtomr1_val,
		mac_pmtcsr_val,
		mmc_rxicmp_err_octets_val,
		mmc_rxicmp_gd_octets_val,
		mmc_rxtcp_err_octets_val,
		mmc_rxtcp_gd_octets_val,
		mmc_rxudp_err_octets_val,
		mmc_rxudp_gd_octets_val,
		mmc_rxipv6_nopay_octets_val,
		mmc_rxipv6_hdrerr_octets_val,
		mmc_rxipv6_gd_octets_val,
		mmc_rxipv4_udsbl_octets_val,
		mmc_rxipv4_frag_octets_val,
		mmc_rxipv4_nopay_octets_val,
		mmc_rxipv4_hdrerr_octets_val,
		mmc_rxipv4_gd_octets_val,
		mmc_rxicmp_err_pkts_val,
		mmc_rxicmp_gd_pkts_val,
		mmc_rxtcp_err_pkts_val,
		mmc_rxtcp_gd_pkts_val,
		mmc_rxudp_err_pkts_val,
		mmc_rxudp_gd_pkts_val,
		mmc_rxipv6_nopay_pkts_val,
		mmc_rxipv6_hdrerr_pkts_val,
		mmc_rxipv6_gd_pkts_val,
		mmc_rxipv4_ubsbl_pkts_val,
		mmc_rxipv4_frag_pkts_val,
		mmc_rxipv4_nopay_pkts_val,
		mmc_rxipv4_hdrerr_pkts_val,
		mmc_rxipv4_gd_pkts_val,
		mmc_rxctrlpackets_g_val,
		mmc_rxrcverror_val,
		mmc_rxwatchdogerror_val,
		mmc_rxvlanpackets_gb_val,
		mmc_rxfifooverflow_val,
		mmc_rxpausepackets_val,
		mmc_rxoutofrangetype_val,
		mmc_rxlengtherror_val,
		mmc_rxunicastpackets_g_val,
		mmc_rx1024tomaxoctets_gb_val,
		mmc_rx512to1023octets_gb_val,
		mmc_rx256to511octets_gb_val,
		mmc_rx128to255octets_gb_val,
		mmc_rx65to127octets_gb_val,
		mmc_rx64octets_gb_val,
		mmc_rxoversize_g_val,
		mmc_rxundersize_g_val,
		mmc_rxjabbererror_val,
		mmc_rxrunterror_val,
		mmc_rxalignmenterror_val,
		mmc_rxcrcerror_val,
		mmc_rxmulticastpackets_g_val,
		mmc_rxbroadcastpackets_g_val,
		mmc_rxoctetcount_g_val,
		mmc_rxoctetcount_gb_val,
		mmc_rxpacketcount_gb_val,
		mmc_txoversize_g_val,
		mmc_txvlanpackets_g_val,
		mmc_txpausepackets_val,
		mmc_txexcessdef_val,
		mmc_txpacketscount_g_val,
		mmc_txoctetcount_g_val,
		mmc_txcarriererror_val,
		mmc_txexesscol_val,
		mmc_txlatecol_val,
		mmc_txdeferred_val,
		mmc_txmulticol_g_val,
		mmc_txsinglecol_g_val,
		mmc_txunderflowerror_val,
		mmc_txbroadcastpackets_gb_val,
		mmc_txmulticastpackets_gb_val,
		mmc_txunicastpackets_gb_val,
		mmc_tx1024tomaxoctets_gb_val,
		mmc_tx512to1023octets_gb_val,
		mmc_tx256to511octets_gb_val,
		mmc_tx128to255octets_gb_val,
		mmc_tx65to127octets_gb_val,
		mmc_tx64octets_gb_val,
		mmc_txmulticastpackets_g_val,
		mmc_txbroadcastpackets_g_val,
		mmc_txpacketcount_gb_val,
		mmc_txoctetcount_gb_val,
		mmc_ipc_intr_rx_val,
		mmc_ipc_intr_mask_rx_val,
		mmc_intr_mask_tx_val,
		mmc_intr_mask_rx_val,
		mmc_intr_tx_val,
		mmc_intr_rx_val,
		mmc_cntrl_val,
		mac_ma1lr_val,
		mac_ma1hr_val,
		mac_ma0lr_val,
		mac_ma0hr_val,
		mac_gpior_val,
		mac_gmiidr_val,
		mac_gmiiar_val,
		mac_hfr2_val,
		mac_hfr1_val,
		mac_hfr0_val,
		mac_mdr_val,
		mac_vr_val,
		mac_htr7_val,
		mac_htr6_val,
		mac_htr5_val,
		mac_htr4_val,
		mac_htr3_val,
		mac_htr2_val,
		mac_htr1_val,
		mac_htr0_val,
		dma_riwtr7_val,
		dma_riwtr6_val,
		dma_riwtr5_val,
		dma_riwtr4_val,
		dma_riwtr3_val,
		dma_riwtr2_val,
		dma_riwtr1_val,
		dma_riwtr0_val,
		dma_rdrlr7_val,
		dma_rdrlr6_val,
		dma_rdrlr5_val,
		dma_rdrlr4_val,
		dma_rdrlr3_val,
		dma_rdrlr2_val,
		dma_rdrlr1_val,
		dma_rdrlr0_val,
		dma_tdrlr7_val,
		dma_tdrlr6_val,
		dma_tdrlr5_val,
		dma_tdrlr4_val,
		dma_tdrlr3_val,
		dma_tdrlr2_val,
		dma_tdrlr1_val,
		dma_tdrlr0_val,
		dma_rdtp_rpdr7_val,
		dma_rdtp_rpdr6_val,
		dma_rdtp_rpdr5_val,
		dma_rdtp_rpdr4_val,
		dma_rdtp_rpdr3_val,
		dma_rdtp_rpdr2_val,
		dma_rdtp_rpdr1_val,
		dma_rdtp_rpdr0_val,
		dma_tdtp_tpdr7_val,
		dma_tdtp_tpdr6_val,
		dma_tdtp_tpdr5_val,
		dma_tdtp_tpdr4_val,
		dma_tdtp_tpdr3_val,
		dma_tdtp_tpdr2_val,
		dma_tdtp_tpdr1_val,
		dma_tdtp_tpdr0_val,
		dma_rdlar7_val,
		dma_rdlar6_val,
		dma_rdlar5_val,
		dma_rdlar4_val,
		dma_rdlar3_val,
		dma_rdlar2_val,
		dma_rdlar1_val,
		dma_rdlar0_val,
		dma_tdlar7_val,
		dma_tdlar6_val,
		dma_tdlar5_val,
		dma_tdlar4_val,
		dma_tdlar3_val,
		dma_tdlar2_val,
		dma_tdlar1_val,
		dma_tdlar0_val,
		dma_ier7_val,
		dma_ier6_val,
		dma_ier5_val,
		dma_ier4_val,
		dma_ier3_val,
		dma_ier2_val,
		dma_ier1_val,
		dma_ier0_val,
		mac_imr_val,
		mac_isr_val,
		mtl_isr_val,
		dma_sr7_val,
		dma_sr6_val,
		dma_sr5_val,
		dma_sr4_val,
		dma_sr3_val,
		dma_sr2_val,
		dma_sr1_val,
		dma_sr0_val,
		dma_isr_val,
		dma_dsr2_val,
		dma_dsr1_val,
		dma_dsr0_val,
		mtl_q0rdr_val,
		mtl_q0esr_val,
		mtl_q0tdr_val,
		dma_chrbar7_val,
		dma_chrbar6_val,
		dma_chrbar5_val,
		dma_chrbar4_val,
		dma_chrbar3_val,
		dma_chrbar2_val,
		dma_chrbar1_val,
		dma_chrbar0_val,
		dma_chtbar7_val,
		dma_chtbar6_val,
		dma_chtbar5_val,
		dma_chtbar4_val,
		dma_chtbar3_val,
		dma_chtbar2_val,
		dma_chtbar1_val,
		dma_chtbar0_val,
		dma_chrdr7_val,
		dma_chrdr6_val,
		dma_chrdr5_val,
		dma_chrdr4_val,
		dma_chrdr3_val,
		dma_chrdr2_val,
		dma_chrdr1_val,
		dma_chrdr0_val,
		dma_chtdr7_val,
		dma_chtdr6_val,
		dma_chtdr5_val,
		dma_chtdr4_val,
		dma_chtdr3_val,
		dma_chtdr2_val,
		dma_chtdr1_val,
		dma_chtdr0_val,
		dma_sfcsr7_val,
		dma_sfcsr6_val,
		dma_sfcsr5_val,
		dma_sfcsr4_val,
		dma_sfcsr3_val,
		dma_sfcsr2_val,
		dma_sfcsr1_val,
		dma_sfcsr0_val,
		mac_ivlantirr_val,
		mac_vlantirr_val,
		mac_vlanhtr_val,
		mac_vlantr_val,
		dma_sbus_val,
		dma_bmr_val,
		mtl_q0rcr_val,
		mtl_q0ocr_val,
		mtl_q0romr_val,
		mtl_q0qr_val,
		mtl_q0ecr_val,
		mtl_q0ucr_val,
		mtl_q0tomr_val,
		mtl_rqdcm1r_val,
		mtl_rqdcm0r_val,
		mtl_fddr_val,
		mtl_fdacs_val,
		mtl_omr_val,
		mac_rqc3r_val,
		mac_rqc2r_val,
		mac_rqc1r_val,
		mac_rqc0r_val,
		mac_tqpm1r_val,
		mac_tqpm0r_val,
		mac_rfcr_val,
		mac_qtfcr7_val,
		mac_qtfcr6_val,
		mac_qtfcr5_val,
		mac_qtfcr4_val,
		mac_qtfcr3_val,
		mac_qtfcr2_val,
		mac_qtfcr1_val,
		mac_q0tfcr_val,
		dma_axi4cr7_val,
		dma_axi4cr6_val,
		dma_axi4cr5_val,
		dma_axi4cr4_val,
		dma_axi4cr3_val,
		dma_axi4cr2_val,
		dma_axi4cr1_val,
		dma_axi4cr0_val,
		dma_rcr7_val,
		dma_rcr6_val,
		dma_rcr5_val,
		dma_rcr4_val,
		dma_rcr3_val,
		dma_rcr2_val,
		dma_rcr1_val,
		dma_rcr0_val,
		dma_tcr7_val,
		dma_tcr6_val,
		dma_tcr5_val,
		dma_tcr4_val,
		dma_tcr3_val,
		dma_tcr2_val,
		dma_tcr1_val,
		dma_tcr0_val,
		dma_cr7_val,
		dma_cr6_val,
		dma_cr5_val,
		dma_cr4_val,
		dma_cr3_val,
		dma_cr2_val,
		dma_cr1_val,
		dma_cr0_val,
		mac_wtr_val,
		mac_mpfr_val,
		mac_mecr_val,
		mac_mcr_val,
		mac_bmcr_reg_val,
		mac_bmsr_reg_val,
		mii_physid1_reg_val,
		mii_physid2_reg_val,
		mii_advertise_reg_val,
		mii_lpa_reg_val,
		mii_expansion_reg_val,
		auto_nego_np_reg_val,
		mii_estatus_reg_val,
		mii_ctrl1000_reg_val,
		mii_stat1000_reg_val,
		phy_ctl_reg_val,
		phy_sts_reg_val, feature_drop_tx_pktburstcnt_val);

	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(debug_buf);
	DBGPR("<-- registers_read\n");

	return ret;
}

static const struct file_operations registers_fops = {
	.read = registers_read,
	.write = registers_write,
};

static ssize_t descriptor_write(struct file *file, const char __user * buf,
				size_t count, loff_t * ppos)
{
	DBGPR("--> registers_write\n");
	printk(KERN_INFO
	       "write operation not supported for descrptors: write error\n");
	DBGPR("<-- registers_write\n");

	return -1;
}

static ssize_t mac_ma32_127lr127_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr127_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(127));
	sprintf(debugfs_buf, "mac_ma32_127lr127            :%#x\n",
		mac_ma32_127lr127_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr127_fops = {
	.read = mac_ma32_127lr127_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr126_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr126_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(126));
	sprintf(debugfs_buf, "mac_ma32_127lr126            :%#x\n",
		mac_ma32_127lr126_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr126_fops = {
	.read = mac_ma32_127lr126_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr125_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr125_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(125));
	sprintf(debugfs_buf, "mac_ma32_127lr125            :%#x\n",
		mac_ma32_127lr125_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr125_fops = {
	.read = mac_ma32_127lr125_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr124_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr124_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(124));
	sprintf(debugfs_buf, "mac_ma32_127lr124            :%#x\n",
		mac_ma32_127lr124_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr124_fops = {
	.read = mac_ma32_127lr124_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr123_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr123_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(123));
	sprintf(debugfs_buf, "mac_ma32_127lr123            :%#x\n",
		mac_ma32_127lr123_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr123_fops = {
	.read = mac_ma32_127lr123_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr122_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr122_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(122));
	sprintf(debugfs_buf, "mac_ma32_127lr122            :%#x\n",
		mac_ma32_127lr122_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr122_fops = {
	.read = mac_ma32_127lr122_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr121_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr121_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(121));
	sprintf(debugfs_buf, "mac_ma32_127lr121            :%#x\n",
		mac_ma32_127lr121_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr121_fops = {
	.read = mac_ma32_127lr121_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr120_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr120_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(120));
	sprintf(debugfs_buf, "mac_ma32_127lr120            :%#x\n",
		mac_ma32_127lr120_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr120_fops = {
	.read = mac_ma32_127lr120_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr119_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr119_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(119));
	sprintf(debugfs_buf, "mac_ma32_127lr119            :%#x\n",
		mac_ma32_127lr119_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr119_fops = {
	.read = mac_ma32_127lr119_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr118_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr118_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(118));
	sprintf(debugfs_buf, "mac_ma32_127lr118            :%#x\n",
		mac_ma32_127lr118_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr118_fops = {
	.read = mac_ma32_127lr118_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr117_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr117_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(117));
	sprintf(debugfs_buf, "mac_ma32_127lr117            :%#x\n",
		mac_ma32_127lr117_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr117_fops = {
	.read = mac_ma32_127lr117_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr116_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr116_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(116));
	sprintf(debugfs_buf, "mac_ma32_127lr116            :%#x\n",
		mac_ma32_127lr116_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr116_fops = {
	.read = mac_ma32_127lr116_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr115_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr115_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(115));
	sprintf(debugfs_buf, "mac_ma32_127lr115            :%#x\n",
		mac_ma32_127lr115_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr115_fops = {
	.read = mac_ma32_127lr115_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr114_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr114_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(114));
	sprintf(debugfs_buf, "mac_ma32_127lr114            :%#x\n",
		mac_ma32_127lr114_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr114_fops = {
	.read = mac_ma32_127lr114_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr113_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr113_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(113));
	sprintf(debugfs_buf, "mac_ma32_127lr113            :%#x\n",
		mac_ma32_127lr113_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr113_fops = {
	.read = mac_ma32_127lr113_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr112_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr112_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(112));
	sprintf(debugfs_buf, "mac_ma32_127lr112            :%#x\n",
		mac_ma32_127lr112_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr112_fops = {
	.read = mac_ma32_127lr112_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr111_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr111_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(111));
	sprintf(debugfs_buf, "mac_ma32_127lr111            :%#x\n",
		mac_ma32_127lr111_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr111_fops = {
	.read = mac_ma32_127lr111_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr110_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr110_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(110));
	sprintf(debugfs_buf, "mac_ma32_127lr110            :%#x\n",
		mac_ma32_127lr110_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr110_fops = {
	.read = mac_ma32_127lr110_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr109_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr109_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(109));
	sprintf(debugfs_buf, "mac_ma32_127lr109            :%#x\n",
		mac_ma32_127lr109_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr109_fops = {
	.read = mac_ma32_127lr109_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr108_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr108_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(108));
	sprintf(debugfs_buf, "mac_ma32_127lr108            :%#x\n",
		mac_ma32_127lr108_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr108_fops = {
	.read = mac_ma32_127lr108_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr107_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr107_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(107));
	sprintf(debugfs_buf, "mac_ma32_127lr107            :%#x\n",
		mac_ma32_127lr107_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr107_fops = {
	.read = mac_ma32_127lr107_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr106_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr106_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(106));
	sprintf(debugfs_buf, "mac_ma32_127lr106            :%#x\n",
		mac_ma32_127lr106_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr106_fops = {
	.read = mac_ma32_127lr106_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr105_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr105_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(105));
	sprintf(debugfs_buf, "mac_ma32_127lr105            :%#x\n",
		mac_ma32_127lr105_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr105_fops = {
	.read = mac_ma32_127lr105_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr104_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr104_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(104));
	sprintf(debugfs_buf, "mac_ma32_127lr104            :%#x\n",
		mac_ma32_127lr104_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr104_fops = {
	.read = mac_ma32_127lr104_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr103_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr103_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(103));
	sprintf(debugfs_buf, "mac_ma32_127lr103            :%#x\n",
		mac_ma32_127lr103_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr103_fops = {
	.read = mac_ma32_127lr103_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr102_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr102_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(102));
	sprintf(debugfs_buf, "mac_ma32_127lr102            :%#x\n",
		mac_ma32_127lr102_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr102_fops = {
	.read = mac_ma32_127lr102_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr101_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr101_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(101));
	sprintf(debugfs_buf, "mac_ma32_127lr101            :%#x\n",
		mac_ma32_127lr101_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr101_fops = {
	.read = mac_ma32_127lr101_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr100_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr100_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(100));
	sprintf(debugfs_buf, "mac_ma32_127lr100            :%#x\n",
		mac_ma32_127lr100_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr100_fops = {
	.read = mac_ma32_127lr100_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr99_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr99_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(99));
	sprintf(debugfs_buf, "mac_ma32_127lr99            :%#x\n",
		mac_ma32_127lr99_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr99_fops = {
	.read = mac_ma32_127lr99_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr98_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr98_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(98));
	sprintf(debugfs_buf, "mac_ma32_127lr98            :%#x\n",
		mac_ma32_127lr98_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr98_fops = {
	.read = mac_ma32_127lr98_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr97_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr97_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(97));
	sprintf(debugfs_buf, "mac_ma32_127lr97            :%#x\n",
		mac_ma32_127lr97_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr97_fops = {
	.read = mac_ma32_127lr97_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr96_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr96_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(96));
	sprintf(debugfs_buf, "mac_ma32_127lr96            :%#x\n",
		mac_ma32_127lr96_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr96_fops = {
	.read = mac_ma32_127lr96_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr95_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr95_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(95));
	sprintf(debugfs_buf, "mac_ma32_127lr95            :%#x\n",
		mac_ma32_127lr95_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr95_fops = {
	.read = mac_ma32_127lr95_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr94_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr94_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(94));
	sprintf(debugfs_buf, "mac_ma32_127lr94            :%#x\n",
		mac_ma32_127lr94_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr94_fops = {
	.read = mac_ma32_127lr94_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr93_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr93_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(93));
	sprintf(debugfs_buf, "mac_ma32_127lr93            :%#x\n",
		mac_ma32_127lr93_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr93_fops = {
	.read = mac_ma32_127lr93_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr92_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr92_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(92));
	sprintf(debugfs_buf, "mac_ma32_127lr92            :%#x\n",
		mac_ma32_127lr92_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr92_fops = {
	.read = mac_ma32_127lr92_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr91_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr91_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(91));
	sprintf(debugfs_buf, "mac_ma32_127lr91            :%#x\n",
		mac_ma32_127lr91_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr91_fops = {
	.read = mac_ma32_127lr91_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr90_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr90_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(90));
	sprintf(debugfs_buf, "mac_ma32_127lr90            :%#x\n",
		mac_ma32_127lr90_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr90_fops = {
	.read = mac_ma32_127lr90_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr89_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr89_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(89));
	sprintf(debugfs_buf, "mac_ma32_127lr89            :%#x\n",
		mac_ma32_127lr89_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr89_fops = {
	.read = mac_ma32_127lr89_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr88_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr88_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(88));
	sprintf(debugfs_buf, "mac_ma32_127lr88            :%#x\n",
		mac_ma32_127lr88_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr88_fops = {
	.read = mac_ma32_127lr88_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr87_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr87_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(87));
	sprintf(debugfs_buf, "mac_ma32_127lr87            :%#x\n",
		mac_ma32_127lr87_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr87_fops = {
	.read = mac_ma32_127lr87_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr86_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr86_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(86));
	sprintf(debugfs_buf, "mac_ma32_127lr86            :%#x\n",
		mac_ma32_127lr86_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr86_fops = {
	.read = mac_ma32_127lr86_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr85_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr85_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(85));
	sprintf(debugfs_buf, "mac_ma32_127lr85            :%#x\n",
		mac_ma32_127lr85_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr85_fops = {
	.read = mac_ma32_127lr85_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr84_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr84_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(84));
	sprintf(debugfs_buf, "mac_ma32_127lr84            :%#x\n",
		mac_ma32_127lr84_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr84_fops = {
	.read = mac_ma32_127lr84_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr83_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr83_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(83));
	sprintf(debugfs_buf, "mac_ma32_127lr83            :%#x\n",
		mac_ma32_127lr83_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr83_fops = {
	.read = mac_ma32_127lr83_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr82_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr82_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(82));
	sprintf(debugfs_buf, "mac_ma32_127lr82            :%#x\n",
		mac_ma32_127lr82_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr82_fops = {
	.read = mac_ma32_127lr82_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr81_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr81_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(81));
	sprintf(debugfs_buf, "mac_ma32_127lr81            :%#x\n",
		mac_ma32_127lr81_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr81_fops = {
	.read = mac_ma32_127lr81_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr80_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr80_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(80));
	sprintf(debugfs_buf, "mac_ma32_127lr80            :%#x\n",
		mac_ma32_127lr80_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr80_fops = {
	.read = mac_ma32_127lr80_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr79_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr79_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(79));
	sprintf(debugfs_buf, "mac_ma32_127lr79            :%#x\n",
		mac_ma32_127lr79_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr79_fops = {
	.read = mac_ma32_127lr79_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr78_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr78_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(78));
	sprintf(debugfs_buf, "mac_ma32_127lr78            :%#x\n",
		mac_ma32_127lr78_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr78_fops = {
	.read = mac_ma32_127lr78_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr77_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr77_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(77));
	sprintf(debugfs_buf, "mac_ma32_127lr77            :%#x\n",
		mac_ma32_127lr77_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr77_fops = {
	.read = mac_ma32_127lr77_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr76_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr76_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(76));
	sprintf(debugfs_buf, "mac_ma32_127lr76            :%#x\n",
		mac_ma32_127lr76_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr76_fops = {
	.read = mac_ma32_127lr76_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr75_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr75_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(75));
	sprintf(debugfs_buf, "mac_ma32_127lr75            :%#x\n",
		mac_ma32_127lr75_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr75_fops = {
	.read = mac_ma32_127lr75_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr74_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr74_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(74));
	sprintf(debugfs_buf, "mac_ma32_127lr74            :%#x\n",
		mac_ma32_127lr74_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr74_fops = {
	.read = mac_ma32_127lr74_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr73_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr73_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(73));
	sprintf(debugfs_buf, "mac_ma32_127lr73            :%#x\n",
		mac_ma32_127lr73_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr73_fops = {
	.read = mac_ma32_127lr73_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr72_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr72_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(72));
	sprintf(debugfs_buf, "mac_ma32_127lr72            :%#x\n",
		mac_ma32_127lr72_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr72_fops = {
	.read = mac_ma32_127lr72_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr71_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr71_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(71));
	sprintf(debugfs_buf, "mac_ma32_127lr71            :%#x\n",
		mac_ma32_127lr71_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr71_fops = {
	.read = mac_ma32_127lr71_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr70_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr70_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(70));
	sprintf(debugfs_buf, "mac_ma32_127lr70            :%#x\n",
		mac_ma32_127lr70_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr70_fops = {
	.read = mac_ma32_127lr70_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr69_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr69_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(69));
	sprintf(debugfs_buf, "mac_ma32_127lr69            :%#x\n",
		mac_ma32_127lr69_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr69_fops = {
	.read = mac_ma32_127lr69_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr68_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr68_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(68));
	sprintf(debugfs_buf, "mac_ma32_127lr68            :%#x\n",
		mac_ma32_127lr68_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr68_fops = {
	.read = mac_ma32_127lr68_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr67_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr67_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(67));
	sprintf(debugfs_buf, "mac_ma32_127lr67            :%#x\n",
		mac_ma32_127lr67_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr67_fops = {
	.read = mac_ma32_127lr67_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr66_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr66_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(66));
	sprintf(debugfs_buf, "mac_ma32_127lr66            :%#x\n",
		mac_ma32_127lr66_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr66_fops = {
	.read = mac_ma32_127lr66_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr65_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr65_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(65));
	sprintf(debugfs_buf, "mac_ma32_127lr65            :%#x\n",
		mac_ma32_127lr65_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr65_fops = {
	.read = mac_ma32_127lr65_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr64_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr64_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(64));
	sprintf(debugfs_buf, "mac_ma32_127lr64            :%#x\n",
		mac_ma32_127lr64_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr64_fops = {
	.read = mac_ma32_127lr64_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr63_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr63_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(63));
	sprintf(debugfs_buf, "mac_ma32_127lr63            :%#x\n",
		mac_ma32_127lr63_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr63_fops = {
	.read = mac_ma32_127lr63_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr62_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr62_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(62));
	sprintf(debugfs_buf, "mac_ma32_127lr62            :%#x\n",
		mac_ma32_127lr62_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr62_fops = {
	.read = mac_ma32_127lr62_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr61_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr61_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(61));
	sprintf(debugfs_buf, "mac_ma32_127lr61            :%#x\n",
		mac_ma32_127lr61_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr61_fops = {
	.read = mac_ma32_127lr61_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr60_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr60_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(60));
	sprintf(debugfs_buf, "mac_ma32_127lr60            :%#x\n",
		mac_ma32_127lr60_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr60_fops = {
	.read = mac_ma32_127lr60_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr59_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr59_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(59));
	sprintf(debugfs_buf, "mac_ma32_127lr59            :%#x\n",
		mac_ma32_127lr59_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr59_fops = {
	.read = mac_ma32_127lr59_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr58_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr58_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(58));
	sprintf(debugfs_buf, "mac_ma32_127lr58            :%#x\n",
		mac_ma32_127lr58_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr58_fops = {
	.read = mac_ma32_127lr58_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr57_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr57_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(57));
	sprintf(debugfs_buf, "mac_ma32_127lr57            :%#x\n",
		mac_ma32_127lr57_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr57_fops = {
	.read = mac_ma32_127lr57_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr56_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr56_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(56));
	sprintf(debugfs_buf, "mac_ma32_127lr56            :%#x\n",
		mac_ma32_127lr56_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr56_fops = {
	.read = mac_ma32_127lr56_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr55_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr55_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(55));
	sprintf(debugfs_buf, "mac_ma32_127lr55            :%#x\n",
		mac_ma32_127lr55_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr55_fops = {
	.read = mac_ma32_127lr55_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr54_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr54_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(54));
	sprintf(debugfs_buf, "mac_ma32_127lr54            :%#x\n",
		mac_ma32_127lr54_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr54_fops = {
	.read = mac_ma32_127lr54_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr53_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr53_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(53));
	sprintf(debugfs_buf, "mac_ma32_127lr53            :%#x\n",
		mac_ma32_127lr53_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr53_fops = {
	.read = mac_ma32_127lr53_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr52_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr52_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(52));
	sprintf(debugfs_buf, "mac_ma32_127lr52            :%#x\n",
		mac_ma32_127lr52_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr52_fops = {
	.read = mac_ma32_127lr52_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr51_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr51_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(51));
	sprintf(debugfs_buf, "mac_ma32_127lr51            :%#x\n",
		mac_ma32_127lr51_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr51_fops = {
	.read = mac_ma32_127lr51_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr50_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr50_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(50));
	sprintf(debugfs_buf, "mac_ma32_127lr50            :%#x\n",
		mac_ma32_127lr50_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr50_fops = {
	.read = mac_ma32_127lr50_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr49_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr49_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(49));
	sprintf(debugfs_buf, "mac_ma32_127lr49            :%#x\n",
		mac_ma32_127lr49_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr49_fops = {
	.read = mac_ma32_127lr49_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr48_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr48_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(48));
	sprintf(debugfs_buf, "mac_ma32_127lr48            :%#x\n",
		mac_ma32_127lr48_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr48_fops = {
	.read = mac_ma32_127lr48_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr47_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr47_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(47));
	sprintf(debugfs_buf, "mac_ma32_127lr47            :%#x\n",
		mac_ma32_127lr47_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr47_fops = {
	.read = mac_ma32_127lr47_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr46_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr46_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(46));
	sprintf(debugfs_buf, "mac_ma32_127lr46            :%#x\n",
		mac_ma32_127lr46_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr46_fops = {
	.read = mac_ma32_127lr46_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr45_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr45_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(45));
	sprintf(debugfs_buf, "mac_ma32_127lr45            :%#x\n",
		mac_ma32_127lr45_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr45_fops = {
	.read = mac_ma32_127lr45_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr44_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr44_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(44));
	sprintf(debugfs_buf, "mac_ma32_127lr44            :%#x\n",
		mac_ma32_127lr44_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr44_fops = {
	.read = mac_ma32_127lr44_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr43_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr43_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(43));
	sprintf(debugfs_buf, "mac_ma32_127lr43            :%#x\n",
		mac_ma32_127lr43_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr43_fops = {
	.read = mac_ma32_127lr43_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr42_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr42_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(42));
	sprintf(debugfs_buf, "mac_ma32_127lr42            :%#x\n",
		mac_ma32_127lr42_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr42_fops = {
	.read = mac_ma32_127lr42_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr41_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr41_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(41));
	sprintf(debugfs_buf, "mac_ma32_127lr41            :%#x\n",
		mac_ma32_127lr41_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr41_fops = {
	.read = mac_ma32_127lr41_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr40_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr40_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(40));
	sprintf(debugfs_buf, "mac_ma32_127lr40            :%#x\n",
		mac_ma32_127lr40_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr40_fops = {
	.read = mac_ma32_127lr40_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr39_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr39_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(39));
	sprintf(debugfs_buf, "mac_ma32_127lr39            :%#x\n",
		mac_ma32_127lr39_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr39_fops = {
	.read = mac_ma32_127lr39_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr38_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr38_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(38));
	sprintf(debugfs_buf, "mac_ma32_127lr38            :%#x\n",
		mac_ma32_127lr38_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr38_fops = {
	.read = mac_ma32_127lr38_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr37_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr37_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(37));
	sprintf(debugfs_buf, "mac_ma32_127lr37            :%#x\n",
		mac_ma32_127lr37_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr37_fops = {
	.read = mac_ma32_127lr37_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr36_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr36_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(36));
	sprintf(debugfs_buf, "mac_ma32_127lr36            :%#x\n",
		mac_ma32_127lr36_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr36_fops = {
	.read = mac_ma32_127lr36_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr35_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr35_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(35));
	sprintf(debugfs_buf, "mac_ma32_127lr35            :%#x\n",
		mac_ma32_127lr35_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr35_fops = {
	.read = mac_ma32_127lr35_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr34_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr34_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(34));
	sprintf(debugfs_buf, "mac_ma32_127lr34            :%#x\n",
		mac_ma32_127lr34_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr34_fops = {
	.read = mac_ma32_127lr34_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr33_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr33_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(33));
	sprintf(debugfs_buf, "mac_ma32_127lr33            :%#x\n",
		mac_ma32_127lr33_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr33_fops = {
	.read = mac_ma32_127lr33_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127lr32_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127lr32_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(32));
	sprintf(debugfs_buf, "mac_ma32_127lr32            :%#x\n",
		mac_ma32_127lr32_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127lr32_fops = {
	.read = mac_ma32_127lr32_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr127_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr127_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(127));
	sprintf(debugfs_buf, "mac_ma32_127hr127            :%#x\n",
		mac_ma32_127hr127_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr127_fops = {
	.read = mac_ma32_127hr127_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr126_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr126_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(126));
	sprintf(debugfs_buf, "mac_ma32_127hr126            :%#x\n",
		mac_ma32_127hr126_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr126_fops = {
	.read = mac_ma32_127hr126_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr125_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr125_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(125));
	sprintf(debugfs_buf, "mac_ma32_127hr125            :%#x\n",
		mac_ma32_127hr125_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr125_fops = {
	.read = mac_ma32_127hr125_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr124_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr124_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(124));
	sprintf(debugfs_buf, "mac_ma32_127hr124            :%#x\n",
		mac_ma32_127hr124_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr124_fops = {
	.read = mac_ma32_127hr124_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr123_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr123_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(123));
	sprintf(debugfs_buf, "mac_ma32_127hr123            :%#x\n",
		mac_ma32_127hr123_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr123_fops = {
	.read = mac_ma32_127hr123_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr122_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr122_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(122));
	sprintf(debugfs_buf, "mac_ma32_127hr122            :%#x\n",
		mac_ma32_127hr122_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr122_fops = {
	.read = mac_ma32_127hr122_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr121_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr121_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(121));
	sprintf(debugfs_buf, "mac_ma32_127hr121            :%#x\n",
		mac_ma32_127hr121_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr121_fops = {
	.read = mac_ma32_127hr121_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr120_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr120_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(120));
	sprintf(debugfs_buf, "mac_ma32_127hr120            :%#x\n",
		mac_ma32_127hr120_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr120_fops = {
	.read = mac_ma32_127hr120_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr119_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr119_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(119));
	sprintf(debugfs_buf, "mac_ma32_127hr119            :%#x\n",
		mac_ma32_127hr119_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr119_fops = {
	.read = mac_ma32_127hr119_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr118_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr118_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(118));
	sprintf(debugfs_buf, "mac_ma32_127hr118            :%#x\n",
		mac_ma32_127hr118_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr118_fops = {
	.read = mac_ma32_127hr118_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr117_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr117_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(117));
	sprintf(debugfs_buf, "mac_ma32_127hr117            :%#x\n",
		mac_ma32_127hr117_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr117_fops = {
	.read = mac_ma32_127hr117_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr116_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr116_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(116));
	sprintf(debugfs_buf, "mac_ma32_127hr116            :%#x\n",
		mac_ma32_127hr116_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr116_fops = {
	.read = mac_ma32_127hr116_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr115_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr115_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(115));
	sprintf(debugfs_buf, "mac_ma32_127hr115            :%#x\n",
		mac_ma32_127hr115_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr115_fops = {
	.read = mac_ma32_127hr115_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr114_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr114_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(114));
	sprintf(debugfs_buf, "mac_ma32_127hr114            :%#x\n",
		mac_ma32_127hr114_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr114_fops = {
	.read = mac_ma32_127hr114_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr113_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr113_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(113));
	sprintf(debugfs_buf, "mac_ma32_127hr113            :%#x\n",
		mac_ma32_127hr113_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr113_fops = {
	.read = mac_ma32_127hr113_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr112_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr112_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(112));
	sprintf(debugfs_buf, "mac_ma32_127hr112            :%#x\n",
		mac_ma32_127hr112_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr112_fops = {
	.read = mac_ma32_127hr112_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr111_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr111_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(111));
	sprintf(debugfs_buf, "mac_ma32_127hr111            :%#x\n",
		mac_ma32_127hr111_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr111_fops = {
	.read = mac_ma32_127hr111_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr110_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr110_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(110));
	sprintf(debugfs_buf, "mac_ma32_127hr110            :%#x\n",
		mac_ma32_127hr110_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr110_fops = {
	.read = mac_ma32_127hr110_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr109_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr109_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(109));
	sprintf(debugfs_buf, "mac_ma32_127hr109            :%#x\n",
		mac_ma32_127hr109_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr109_fops = {
	.read = mac_ma32_127hr109_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr108_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr108_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(108));
	sprintf(debugfs_buf, "mac_ma32_127hr108            :%#x\n",
		mac_ma32_127hr108_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr108_fops = {
	.read = mac_ma32_127hr108_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr107_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr107_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(107));
	sprintf(debugfs_buf, "mac_ma32_127hr107            :%#x\n",
		mac_ma32_127hr107_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr107_fops = {
	.read = mac_ma32_127hr107_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr106_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr106_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(106));
	sprintf(debugfs_buf, "mac_ma32_127hr106            :%#x\n",
		mac_ma32_127hr106_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr106_fops = {
	.read = mac_ma32_127hr106_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr105_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr105_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(105));
	sprintf(debugfs_buf, "mac_ma32_127hr105            :%#x\n",
		mac_ma32_127hr105_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr105_fops = {
	.read = mac_ma32_127hr105_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr104_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr104_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(104));
	sprintf(debugfs_buf, "mac_ma32_127hr104            :%#x\n",
		mac_ma32_127hr104_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr104_fops = {
	.read = mac_ma32_127hr104_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr103_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr103_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(103));
	sprintf(debugfs_buf, "mac_ma32_127hr103            :%#x\n",
		mac_ma32_127hr103_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr103_fops = {
	.read = mac_ma32_127hr103_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr102_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr102_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(102));
	sprintf(debugfs_buf, "mac_ma32_127hr102            :%#x\n",
		mac_ma32_127hr102_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr102_fops = {
	.read = mac_ma32_127hr102_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr101_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr101_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(101));
	sprintf(debugfs_buf, "mac_ma32_127hr101            :%#x\n",
		mac_ma32_127hr101_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr101_fops = {
	.read = mac_ma32_127hr101_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr100_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr100_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(100));
	sprintf(debugfs_buf, "mac_ma32_127hr100            :%#x\n",
		mac_ma32_127hr100_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr100_fops = {
	.read = mac_ma32_127hr100_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr99_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr99_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(99));
	sprintf(debugfs_buf, "mac_ma32_127hr99            :%#x\n",
		mac_ma32_127hr99_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr99_fops = {
	.read = mac_ma32_127hr99_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr98_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr98_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(98));
	sprintf(debugfs_buf, "mac_ma32_127hr98            :%#x\n",
		mac_ma32_127hr98_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr98_fops = {
	.read = mac_ma32_127hr98_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr97_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr97_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(97));
	sprintf(debugfs_buf, "mac_ma32_127hr97            :%#x\n",
		mac_ma32_127hr97_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr97_fops = {
	.read = mac_ma32_127hr97_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr96_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr96_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(96));
	sprintf(debugfs_buf, "mac_ma32_127hr96            :%#x\n",
		mac_ma32_127hr96_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr96_fops = {
	.read = mac_ma32_127hr96_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr95_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr95_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(95));
	sprintf(debugfs_buf, "mac_ma32_127hr95            :%#x\n",
		mac_ma32_127hr95_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr95_fops = {
	.read = mac_ma32_127hr95_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr94_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr94_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(94));
	sprintf(debugfs_buf, "mac_ma32_127hr94            :%#x\n",
		mac_ma32_127hr94_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr94_fops = {
	.read = mac_ma32_127hr94_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr93_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr93_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(93));
	sprintf(debugfs_buf, "mac_ma32_127hr93            :%#x\n",
		mac_ma32_127hr93_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr93_fops = {
	.read = mac_ma32_127hr93_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr92_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr92_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(92));
	sprintf(debugfs_buf, "mac_ma32_127hr92            :%#x\n",
		mac_ma32_127hr92_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr92_fops = {
	.read = mac_ma32_127hr92_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr91_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr91_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(91));
	sprintf(debugfs_buf, "mac_ma32_127hr91            :%#x\n",
		mac_ma32_127hr91_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr91_fops = {
	.read = mac_ma32_127hr91_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr90_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr90_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(90));
	sprintf(debugfs_buf, "mac_ma32_127hr90            :%#x\n",
		mac_ma32_127hr90_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr90_fops = {
	.read = mac_ma32_127hr90_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr89_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr89_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(89));
	sprintf(debugfs_buf, "mac_ma32_127hr89            :%#x\n",
		mac_ma32_127hr89_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr89_fops = {
	.read = mac_ma32_127hr89_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr88_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr88_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(88));
	sprintf(debugfs_buf, "mac_ma32_127hr88            :%#x\n",
		mac_ma32_127hr88_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr88_fops = {
	.read = mac_ma32_127hr88_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr87_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr87_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(87));
	sprintf(debugfs_buf, "mac_ma32_127hr87            :%#x\n",
		mac_ma32_127hr87_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr87_fops = {
	.read = mac_ma32_127hr87_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr86_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr86_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(86));
	sprintf(debugfs_buf, "mac_ma32_127hr86            :%#x\n",
		mac_ma32_127hr86_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr86_fops = {
	.read = mac_ma32_127hr86_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr85_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr85_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(85));
	sprintf(debugfs_buf, "mac_ma32_127hr85            :%#x\n",
		mac_ma32_127hr85_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr85_fops = {
	.read = mac_ma32_127hr85_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr84_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr84_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(84));
	sprintf(debugfs_buf, "mac_ma32_127hr84            :%#x\n",
		mac_ma32_127hr84_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr84_fops = {
	.read = mac_ma32_127hr84_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr83_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr83_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(83));
	sprintf(debugfs_buf, "mac_ma32_127hr83            :%#x\n",
		mac_ma32_127hr83_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr83_fops = {
	.read = mac_ma32_127hr83_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr82_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr82_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(82));
	sprintf(debugfs_buf, "mac_ma32_127hr82            :%#x\n",
		mac_ma32_127hr82_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr82_fops = {
	.read = mac_ma32_127hr82_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr81_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr81_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(81));
	sprintf(debugfs_buf, "mac_ma32_127hr81            :%#x\n",
		mac_ma32_127hr81_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr81_fops = {
	.read = mac_ma32_127hr81_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr80_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr80_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(80));
	sprintf(debugfs_buf, "mac_ma32_127hr80            :%#x\n",
		mac_ma32_127hr80_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr80_fops = {
	.read = mac_ma32_127hr80_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr79_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr79_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(79));
	sprintf(debugfs_buf, "mac_ma32_127hr79            :%#x\n",
		mac_ma32_127hr79_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr79_fops = {
	.read = mac_ma32_127hr79_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr78_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr78_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(78));
	sprintf(debugfs_buf, "mac_ma32_127hr78            :%#x\n",
		mac_ma32_127hr78_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr78_fops = {
	.read = mac_ma32_127hr78_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr77_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr77_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(77));
	sprintf(debugfs_buf, "mac_ma32_127hr77            :%#x\n",
		mac_ma32_127hr77_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr77_fops = {
	.read = mac_ma32_127hr77_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr76_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr76_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(76));
	sprintf(debugfs_buf, "mac_ma32_127hr76            :%#x\n",
		mac_ma32_127hr76_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr76_fops = {
	.read = mac_ma32_127hr76_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr75_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr75_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(75));
	sprintf(debugfs_buf, "mac_ma32_127hr75            :%#x\n",
		mac_ma32_127hr75_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr75_fops = {
	.read = mac_ma32_127hr75_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr74_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr74_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(74));
	sprintf(debugfs_buf, "mac_ma32_127hr74            :%#x\n",
		mac_ma32_127hr74_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr74_fops = {
	.read = mac_ma32_127hr74_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr73_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr73_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(73));
	sprintf(debugfs_buf, "mac_ma32_127hr73            :%#x\n",
		mac_ma32_127hr73_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr73_fops = {
	.read = mac_ma32_127hr73_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr72_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr72_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(72));
	sprintf(debugfs_buf, "mac_ma32_127hr72            :%#x\n",
		mac_ma32_127hr72_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr72_fops = {
	.read = mac_ma32_127hr72_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr71_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr71_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(71));
	sprintf(debugfs_buf, "mac_ma32_127hr71            :%#x\n",
		mac_ma32_127hr71_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr71_fops = {
	.read = mac_ma32_127hr71_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr70_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr70_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(70));
	sprintf(debugfs_buf, "mac_ma32_127hr70            :%#x\n",
		mac_ma32_127hr70_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr70_fops = {
	.read = mac_ma32_127hr70_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr69_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr69_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(69));
	sprintf(debugfs_buf, "mac_ma32_127hr69            :%#x\n",
		mac_ma32_127hr69_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr69_fops = {
	.read = mac_ma32_127hr69_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr68_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr68_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(68));
	sprintf(debugfs_buf, "mac_ma32_127hr68            :%#x\n",
		mac_ma32_127hr68_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr68_fops = {
	.read = mac_ma32_127hr68_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr67_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr67_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(67));
	sprintf(debugfs_buf, "mac_ma32_127hr67            :%#x\n",
		mac_ma32_127hr67_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr67_fops = {
	.read = mac_ma32_127hr67_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr66_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr66_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(66));
	sprintf(debugfs_buf, "mac_ma32_127hr66            :%#x\n",
		mac_ma32_127hr66_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr66_fops = {
	.read = mac_ma32_127hr66_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr65_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr65_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(65));
	sprintf(debugfs_buf, "mac_ma32_127hr65            :%#x\n",
		mac_ma32_127hr65_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr65_fops = {
	.read = mac_ma32_127hr65_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr64_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr64_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(64));
	sprintf(debugfs_buf, "mac_ma32_127hr64            :%#x\n",
		mac_ma32_127hr64_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr64_fops = {
	.read = mac_ma32_127hr64_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr63_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr63_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(63));
	sprintf(debugfs_buf, "mac_ma32_127hr63            :%#x\n",
		mac_ma32_127hr63_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr63_fops = {
	.read = mac_ma32_127hr63_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr62_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr62_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(62));
	sprintf(debugfs_buf, "mac_ma32_127hr62            :%#x\n",
		mac_ma32_127hr62_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr62_fops = {
	.read = mac_ma32_127hr62_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr61_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr61_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(61));
	sprintf(debugfs_buf, "mac_ma32_127hr61            :%#x\n",
		mac_ma32_127hr61_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr61_fops = {
	.read = mac_ma32_127hr61_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr60_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr60_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(60));
	sprintf(debugfs_buf, "mac_ma32_127hr60            :%#x\n",
		mac_ma32_127hr60_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr60_fops = {
	.read = mac_ma32_127hr60_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr59_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr59_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(59));
	sprintf(debugfs_buf, "mac_ma32_127hr59            :%#x\n",
		mac_ma32_127hr59_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr59_fops = {
	.read = mac_ma32_127hr59_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr58_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr58_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(58));
	sprintf(debugfs_buf, "mac_ma32_127hr58            :%#x\n",
		mac_ma32_127hr58_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr58_fops = {
	.read = mac_ma32_127hr58_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr57_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr57_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(57));
	sprintf(debugfs_buf, "mac_ma32_127hr57            :%#x\n",
		mac_ma32_127hr57_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr57_fops = {
	.read = mac_ma32_127hr57_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr56_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr56_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(56));
	sprintf(debugfs_buf, "mac_ma32_127hr56            :%#x\n",
		mac_ma32_127hr56_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr56_fops = {
	.read = mac_ma32_127hr56_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr55_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr55_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(55));
	sprintf(debugfs_buf, "mac_ma32_127hr55            :%#x\n",
		mac_ma32_127hr55_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr55_fops = {
	.read = mac_ma32_127hr55_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr54_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr54_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(54));
	sprintf(debugfs_buf, "mac_ma32_127hr54            :%#x\n",
		mac_ma32_127hr54_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr54_fops = {
	.read = mac_ma32_127hr54_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr53_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr53_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(53));
	sprintf(debugfs_buf, "mac_ma32_127hr53            :%#x\n",
		mac_ma32_127hr53_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr53_fops = {
	.read = mac_ma32_127hr53_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr52_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr52_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(52));
	sprintf(debugfs_buf, "mac_ma32_127hr52            :%#x\n",
		mac_ma32_127hr52_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr52_fops = {
	.read = mac_ma32_127hr52_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr51_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr51_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(51));
	sprintf(debugfs_buf, "mac_ma32_127hr51            :%#x\n",
		mac_ma32_127hr51_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr51_fops = {
	.read = mac_ma32_127hr51_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr50_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr50_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(50));
	sprintf(debugfs_buf, "mac_ma32_127hr50            :%#x\n",
		mac_ma32_127hr50_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr50_fops = {
	.read = mac_ma32_127hr50_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr49_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr49_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(49));
	sprintf(debugfs_buf, "mac_ma32_127hr49            :%#x\n",
		mac_ma32_127hr49_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr49_fops = {
	.read = mac_ma32_127hr49_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr48_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr48_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(48));
	sprintf(debugfs_buf, "mac_ma32_127hr48            :%#x\n",
		mac_ma32_127hr48_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr48_fops = {
	.read = mac_ma32_127hr48_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr47_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr47_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(47));
	sprintf(debugfs_buf, "mac_ma32_127hr47            :%#x\n",
		mac_ma32_127hr47_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr47_fops = {
	.read = mac_ma32_127hr47_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr46_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr46_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(46));
	sprintf(debugfs_buf, "mac_ma32_127hr46            :%#x\n",
		mac_ma32_127hr46_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr46_fops = {
	.read = mac_ma32_127hr46_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr45_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr45_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(45));
	sprintf(debugfs_buf, "mac_ma32_127hr45            :%#x\n",
		mac_ma32_127hr45_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr45_fops = {
	.read = mac_ma32_127hr45_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr44_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr44_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(44));
	sprintf(debugfs_buf, "mac_ma32_127hr44            :%#x\n",
		mac_ma32_127hr44_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr44_fops = {
	.read = mac_ma32_127hr44_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr43_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr43_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(43));
	sprintf(debugfs_buf, "mac_ma32_127hr43            :%#x\n",
		mac_ma32_127hr43_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr43_fops = {
	.read = mac_ma32_127hr43_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr42_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr42_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(42));
	sprintf(debugfs_buf, "mac_ma32_127hr42            :%#x\n",
		mac_ma32_127hr42_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr42_fops = {
	.read = mac_ma32_127hr42_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr41_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr41_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(41));
	sprintf(debugfs_buf, "mac_ma32_127hr41            :%#x\n",
		mac_ma32_127hr41_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr41_fops = {
	.read = mac_ma32_127hr41_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr40_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr40_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(40));
	sprintf(debugfs_buf, "mac_ma32_127hr40            :%#x\n",
		mac_ma32_127hr40_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr40_fops = {
	.read = mac_ma32_127hr40_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr39_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr39_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(39));
	sprintf(debugfs_buf, "mac_ma32_127hr39            :%#x\n",
		mac_ma32_127hr39_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr39_fops = {
	.read = mac_ma32_127hr39_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr38_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr38_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(38));
	sprintf(debugfs_buf, "mac_ma32_127hr38            :%#x\n",
		mac_ma32_127hr38_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr38_fops = {
	.read = mac_ma32_127hr38_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr37_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr37_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(37));
	sprintf(debugfs_buf, "mac_ma32_127hr37            :%#x\n",
		mac_ma32_127hr37_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr37_fops = {
	.read = mac_ma32_127hr37_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr36_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr36_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(36));
	sprintf(debugfs_buf, "mac_ma32_127hr36            :%#x\n",
		mac_ma32_127hr36_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr36_fops = {
	.read = mac_ma32_127hr36_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr35_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr35_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(35));
	sprintf(debugfs_buf, "mac_ma32_127hr35            :%#x\n",
		mac_ma32_127hr35_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr35_fops = {
	.read = mac_ma32_127hr35_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr34_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr34_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(34));
	sprintf(debugfs_buf, "mac_ma32_127hr34            :%#x\n",
		mac_ma32_127hr34_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr34_fops = {
	.read = mac_ma32_127hr34_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr33_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr33_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(33));
	sprintf(debugfs_buf, "mac_ma32_127hr33            :%#x\n",
		mac_ma32_127hr33_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr33_fops = {
	.read = mac_ma32_127hr33_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma32_127hr32_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma32_127hr32_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(32));
	sprintf(debugfs_buf, "mac_ma32_127hr32            :%#x\n",
		mac_ma32_127hr32_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma32_127hr32_fops = {
	.read = mac_ma32_127hr32_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr31_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr31_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(31));
	sprintf(debugfs_buf, "mac_ma1_31lr31              :%#x\n",
		mac_ma1_31lr31_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr31_fops = {
	.read = mac_ma1_31lr31_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr30_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr30_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(30));
	sprintf(debugfs_buf, "mac_ma1_31lr30              :%#x\n",
		mac_ma1_31lr30_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr30_fops = {
	.read = mac_ma1_31lr30_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr29_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr29_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(29));
	sprintf(debugfs_buf, "mac_ma1_31lr29              :%#x\n",
		mac_ma1_31lr29_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr29_fops = {
	.read = mac_ma1_31lr29_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr28_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr28_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(28));
	sprintf(debugfs_buf, "mac_ma1_31lr28              :%#x\n",
		mac_ma1_31lr28_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr28_fops = {
	.read = mac_ma1_31lr28_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr27_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr27_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(27));
	sprintf(debugfs_buf, "mac_ma1_31lr27              :%#x\n",
		mac_ma1_31lr27_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr27_fops = {
	.read = mac_ma1_31lr27_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr26_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr26_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(26));
	sprintf(debugfs_buf, "mac_ma1_31lr26              :%#x\n",
		mac_ma1_31lr26_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr26_fops = {
	.read = mac_ma1_31lr26_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr25_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr25_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(25));
	sprintf(debugfs_buf, "mac_ma1_31lr25              :%#x\n",
		mac_ma1_31lr25_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr25_fops = {
	.read = mac_ma1_31lr25_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr24_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr24_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(24));
	sprintf(debugfs_buf, "mac_ma1_31lr24              :%#x\n",
		mac_ma1_31lr24_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr24_fops = {
	.read = mac_ma1_31lr24_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr23_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr23_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(23));
	sprintf(debugfs_buf, "mac_ma1_31lr23              :%#x\n",
		mac_ma1_31lr23_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr23_fops = {
	.read = mac_ma1_31lr23_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr22_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr22_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(22));
	sprintf(debugfs_buf, "mac_ma1_31lr22              :%#x\n",
		mac_ma1_31lr22_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr22_fops = {
	.read = mac_ma1_31lr22_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr21_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr21_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(21));
	sprintf(debugfs_buf, "mac_ma1_31lr21              :%#x\n",
		mac_ma1_31lr21_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr21_fops = {
	.read = mac_ma1_31lr21_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr20_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr20_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(20));
	sprintf(debugfs_buf, "mac_ma1_31lr20              :%#x\n",
		mac_ma1_31lr20_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr20_fops = {
	.read = mac_ma1_31lr20_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr19_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr19_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(19));
	sprintf(debugfs_buf, "mac_ma1_31lr19              :%#x\n",
		mac_ma1_31lr19_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr19_fops = {
	.read = mac_ma1_31lr19_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr18_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr18_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(18));
	sprintf(debugfs_buf, "mac_ma1_31lr18              :%#x\n",
		mac_ma1_31lr18_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr18_fops = {
	.read = mac_ma1_31lr18_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr17_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr17_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(17));
	sprintf(debugfs_buf, "mac_ma1_31lr17              :%#x\n",
		mac_ma1_31lr17_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr17_fops = {
	.read = mac_ma1_31lr17_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr16_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr16_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(16));
	sprintf(debugfs_buf, "mac_ma1_31lr16              :%#x\n",
		mac_ma1_31lr16_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr16_fops = {
	.read = mac_ma1_31lr16_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr15_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr15_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(15));
	sprintf(debugfs_buf, "mac_ma1_31lr15              :%#x\n",
		mac_ma1_31lr15_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr15_fops = {
	.read = mac_ma1_31lr15_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr14_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr14_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(14));
	sprintf(debugfs_buf, "mac_ma1_31lr14              :%#x\n",
		mac_ma1_31lr14_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr14_fops = {
	.read = mac_ma1_31lr14_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr13_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr13_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(13));
	sprintf(debugfs_buf, "mac_ma1_31lr13              :%#x\n",
		mac_ma1_31lr13_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr13_fops = {
	.read = mac_ma1_31lr13_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr12_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr12_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(12));
	sprintf(debugfs_buf, "mac_ma1_31lr12              :%#x\n",
		mac_ma1_31lr12_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr12_fops = {
	.read = mac_ma1_31lr12_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr11_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr11_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(11));
	sprintf(debugfs_buf, "mac_ma1_31lr11              :%#x\n",
		mac_ma1_31lr11_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr11_fops = {
	.read = mac_ma1_31lr11_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr10_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr10_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(10));
	sprintf(debugfs_buf, "mac_ma1_31lr10              :%#x\n",
		mac_ma1_31lr10_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr10_fops = {
	.read = mac_ma1_31lr10_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr9_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr9_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(9));
	sprintf(debugfs_buf, "mac_ma1_31lr9              :%#x\n",
		mac_ma1_31lr9_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr9_fops = {
	.read = mac_ma1_31lr9_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr8_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr8_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(8));
	sprintf(debugfs_buf, "mac_ma1_31lr8              :%#x\n",
		mac_ma1_31lr8_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr8_fops = {
	.read = mac_ma1_31lr8_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr7_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(7));
	sprintf(debugfs_buf, "mac_ma1_31lr7              :%#x\n",
		mac_ma1_31lr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr7_fops = {
	.read = mac_ma1_31lr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr6_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(6));
	sprintf(debugfs_buf, "mac_ma1_31lr6              :%#x\n",
		mac_ma1_31lr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr6_fops = {
	.read = mac_ma1_31lr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr5_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(5));
	sprintf(debugfs_buf, "mac_ma1_31lr5              :%#x\n",
		mac_ma1_31lr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr5_fops = {
	.read = mac_ma1_31lr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr4_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(4));
	sprintf(debugfs_buf, "mac_ma1_31lr4              :%#x\n",
		mac_ma1_31lr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr4_fops = {
	.read = mac_ma1_31lr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr3_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(3));
	sprintf(debugfs_buf, "mac_ma1_31lr3              :%#x\n",
		mac_ma1_31lr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr3_fops = {
	.read = mac_ma1_31lr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr2_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(2));
	sprintf(debugfs_buf, "mac_ma1_31lr2              :%#x\n",
		mac_ma1_31lr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr2_fops = {
	.read = mac_ma1_31lr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31lr1_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31lr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(1));
	sprintf(debugfs_buf, "mac_ma1_31lr1              :%#x\n",
		mac_ma1_31lr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31lr1_fops = {
	.read = mac_ma1_31lr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr31_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr31_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(31));
	sprintf(debugfs_buf, "mac_ma1_31hr31              :%#x\n",
		mac_ma1_31hr31_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr31_fops = {
	.read = mac_ma1_31hr31_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr30_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr30_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(30));
	sprintf(debugfs_buf, "mac_ma1_31hr30              :%#x\n",
		mac_ma1_31hr30_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr30_fops = {
	.read = mac_ma1_31hr30_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr29_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr29_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(29));
	sprintf(debugfs_buf, "mac_ma1_31hr29              :%#x\n",
		mac_ma1_31hr29_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr29_fops = {
	.read = mac_ma1_31hr29_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr28_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr28_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(28));
	sprintf(debugfs_buf, "mac_ma1_31hr28              :%#x\n",
		mac_ma1_31hr28_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr28_fops = {
	.read = mac_ma1_31hr28_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr27_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr27_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(27));
	sprintf(debugfs_buf, "mac_ma1_31hr27              :%#x\n",
		mac_ma1_31hr27_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr27_fops = {
	.read = mac_ma1_31hr27_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr26_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr26_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(26));
	sprintf(debugfs_buf, "mac_ma1_31hr26              :%#x\n",
		mac_ma1_31hr26_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr26_fops = {
	.read = mac_ma1_31hr26_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr25_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr25_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(25));
	sprintf(debugfs_buf, "mac_ma1_31hr25              :%#x\n",
		mac_ma1_31hr25_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr25_fops = {
	.read = mac_ma1_31hr25_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr24_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr24_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(24));
	sprintf(debugfs_buf, "mac_ma1_31hr24              :%#x\n",
		mac_ma1_31hr24_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr24_fops = {
	.read = mac_ma1_31hr24_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr23_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr23_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(23));
	sprintf(debugfs_buf, "mac_ma1_31hr23              :%#x\n",
		mac_ma1_31hr23_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr23_fops = {
	.read = mac_ma1_31hr23_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr22_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr22_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(22));
	sprintf(debugfs_buf, "mac_ma1_31hr22              :%#x\n",
		mac_ma1_31hr22_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr22_fops = {
	.read = mac_ma1_31hr22_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr21_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr21_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(21));
	sprintf(debugfs_buf, "mac_ma1_31hr21              :%#x\n",
		mac_ma1_31hr21_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr21_fops = {
	.read = mac_ma1_31hr21_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr20_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr20_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(20));
	sprintf(debugfs_buf, "mac_ma1_31hr20              :%#x\n",
		mac_ma1_31hr20_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr20_fops = {
	.read = mac_ma1_31hr20_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr19_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr19_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(19));
	sprintf(debugfs_buf, "mac_ma1_31hr19              :%#x\n",
		mac_ma1_31hr19_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr19_fops = {
	.read = mac_ma1_31hr19_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr18_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr18_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(18));
	sprintf(debugfs_buf, "mac_ma1_31hr18              :%#x\n",
		mac_ma1_31hr18_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr18_fops = {
	.read = mac_ma1_31hr18_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr17_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr17_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(17));
	sprintf(debugfs_buf, "mac_ma1_31hr17              :%#x\n",
		mac_ma1_31hr17_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr17_fops = {
	.read = mac_ma1_31hr17_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr16_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr16_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(16));
	sprintf(debugfs_buf, "mac_ma1_31hr16              :%#x\n",
		mac_ma1_31hr16_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr16_fops = {
	.read = mac_ma1_31hr16_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr15_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr15_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(15));
	sprintf(debugfs_buf, "mac_ma1_31hr15              :%#x\n",
		mac_ma1_31hr15_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr15_fops = {
	.read = mac_ma1_31hr15_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr14_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr14_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(14));
	sprintf(debugfs_buf, "mac_ma1_31hr14              :%#x\n",
		mac_ma1_31hr14_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr14_fops = {
	.read = mac_ma1_31hr14_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr13_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr13_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(13));
	sprintf(debugfs_buf, "mac_ma1_31hr13              :%#x\n",
		mac_ma1_31hr13_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr13_fops = {
	.read = mac_ma1_31hr13_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr12_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr12_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(12));
	sprintf(debugfs_buf, "mac_ma1_31hr12              :%#x\n",
		mac_ma1_31hr12_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr12_fops = {
	.read = mac_ma1_31hr12_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr11_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr11_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(11));
	sprintf(debugfs_buf, "mac_ma1_31hr11              :%#x\n",
		mac_ma1_31hr11_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr11_fops = {
	.read = mac_ma1_31hr11_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr10_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr10_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(10));
	sprintf(debugfs_buf, "mac_ma1_31hr10              :%#x\n",
		mac_ma1_31hr10_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr10_fops = {
	.read = mac_ma1_31hr10_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr9_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr9_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(9));
	sprintf(debugfs_buf, "mac_ma1_31hr9              :%#x\n",
		mac_ma1_31hr9_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr9_fops = {
	.read = mac_ma1_31hr9_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr8_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr8_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(8));
	sprintf(debugfs_buf, "mac_ma1_31hr8              :%#x\n",
		mac_ma1_31hr8_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr8_fops = {
	.read = mac_ma1_31hr8_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr7_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(7));
	sprintf(debugfs_buf, "mac_ma1_31hr7              :%#x\n",
		mac_ma1_31hr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr7_fops = {
	.read = mac_ma1_31hr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr6_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(6));
	sprintf(debugfs_buf, "mac_ma1_31hr6              :%#x\n",
		mac_ma1_31hr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr6_fops = {
	.read = mac_ma1_31hr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr5_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(5));
	sprintf(debugfs_buf, "mac_ma1_31hr5              :%#x\n",
		mac_ma1_31hr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr5_fops = {
	.read = mac_ma1_31hr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr4_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(4));
	sprintf(debugfs_buf, "mac_ma1_31hr4              :%#x\n",
		mac_ma1_31hr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr4_fops = {
	.read = mac_ma1_31hr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr3_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(3));
	sprintf(debugfs_buf, "mac_ma1_31hr3              :%#x\n",
		mac_ma1_31hr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr3_fops = {
	.read = mac_ma1_31hr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr2_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(2));
	sprintf(debugfs_buf, "mac_ma1_31hr2              :%#x\n",
		mac_ma1_31hr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr2_fops = {
	.read = mac_ma1_31hr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1_31hr1_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1_31hr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(1));
	sprintf(debugfs_buf, "mac_ma1_31hr1              :%#x\n",
		mac_ma1_31hr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1_31hr1_fops = {
	.read = mac_ma1_31hr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_arpa_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_arpa_val = dwceqos_readl(pdata, DWCEQOS_MAC_ARP_ADDR);
	sprintf(debugfs_buf, "mac_arpa                   :%#x\n", mac_arpa_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_arpa_fops = {
	.read = mac_arpa_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a3r7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a3r7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(7));
	sprintf(debugfs_buf, "mac_l3a3r7                 :%#x\n",
		mac_l3a3r7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a3r7_fops = {
	.read = mac_l3a3r7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a3r6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a3r6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(6));
	sprintf(debugfs_buf, "mac_l3a3r6                 :%#x\n",
		mac_l3a3r6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a3r6_fops = {
	.read = mac_l3a3r6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a3r5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a3r5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(5));
	sprintf(debugfs_buf, "mac_l3a3r5                 :%#x\n",
		mac_l3a3r5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a3r5_fops = {
	.read = mac_l3a3r5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a3r4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a3r4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(4));
	sprintf(debugfs_buf, "mac_l3a3r4                 :%#x\n",
		mac_l3a3r4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a3r4_fops = {
	.read = mac_l3a3r4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a3r3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a3r3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(3));
	sprintf(debugfs_buf, "mac_l3a3r3                 :%#x\n",
		mac_l3a3r3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a3r3_fops = {
	.read = mac_l3a3r3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a3r2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a3r2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(2));
	sprintf(debugfs_buf, "mac_l3a3r2                 :%#x\n",
		mac_l3a3r2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a3r2_fops = {
	.read = mac_l3a3r2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a3r1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a3r1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(1));
	sprintf(debugfs_buf, "mac_l3a3r1                 :%#x\n",
		mac_l3a3r1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a3r1_fops = {
	.read = mac_l3a3r1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a3r0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a3r0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR3(0));
	sprintf(debugfs_buf, "mac_l3a3r0                 :%#x\n",
		mac_l3a3r0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a3r0_fops = {
	.read = mac_l3a3r0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a2r7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a2r7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(7));
	sprintf(debugfs_buf, "mac_l3a2r7                 :%#x\n",
		mac_l3a2r7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a2r7_fops = {
	.read = mac_l3a2r7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a2r6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a2r6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(6));
	sprintf(debugfs_buf, "mac_l3a2r6                 :%#x\n",
		mac_l3a2r6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a2r6_fops = {
	.read = mac_l3a2r6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a2r5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a2r5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(5));
	sprintf(debugfs_buf, "mac_l3a2r5                 :%#x\n",
		mac_l3a2r5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a2r5_fops = {
	.read = mac_l3a2r5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a2r4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a2r4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(4));
	sprintf(debugfs_buf, "mac_l3a2r4                 :%#x\n",
		mac_l3a2r4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a2r4_fops = {
	.read = mac_l3a2r4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a2r3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a2r3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(3));
	sprintf(debugfs_buf, "mac_l3a2r3                 :%#x\n",
		mac_l3a2r3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a2r3_fops = {
	.read = mac_l3a2r3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a2r2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a2r2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(2));
	sprintf(debugfs_buf, "mac_l3a2r2                 :%#x\n",
		mac_l3a2r2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a2r2_fops = {
	.read = mac_l3a2r2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a2r1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a2r1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(1));
	sprintf(debugfs_buf, "mac_l3a2r1                 :%#x\n",
		mac_l3a2r1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a2r1_fops = {
	.read = mac_l3a2r1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a2r0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a2r0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR2(0));
	sprintf(debugfs_buf, "mac_l3a2r0                 :%#x\n",
		mac_l3a2r0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a2r0_fops = {
	.read = mac_l3a2r0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a1r7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a1r7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(7));
	sprintf(debugfs_buf, "mac_l3a1r7                 :%#x\n",
		mac_l3a1r7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a1r7_fops = {
	.read = mac_l3a1r7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a1r6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a1r6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(6));
	sprintf(debugfs_buf, "mac_l3a1r6                 :%#x\n",
		mac_l3a1r6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a1r6_fops = {
	.read = mac_l3a1r6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a1r5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a1r5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(5));
	sprintf(debugfs_buf, "mac_l3a1r5                 :%#x\n",
		mac_l3a1r5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a1r5_fops = {
	.read = mac_l3a1r5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a1r4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a1r4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(4));
	sprintf(debugfs_buf, "mac_l3a1r4                 :%#x\n",
		mac_l3a1r4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a1r4_fops = {
	.read = mac_l3a1r4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a1r3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a1r3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(3));
	sprintf(debugfs_buf, "mac_l3a1r3                 :%#x\n",
		mac_l3a1r3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a1r3_fops = {
	.read = mac_l3a1r3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a1r2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a1r2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(2));
	sprintf(debugfs_buf, "mac_l3a1r2                 :%#x\n",
		mac_l3a1r2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a1r2_fops = {
	.read = mac_l3a1r2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a1r1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a1r1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(1));
	sprintf(debugfs_buf, "mac_l3a1r1                 :%#x\n",
		mac_l3a1r1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a1r1_fops = {
	.read = mac_l3a1r1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a1r0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a1r0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR1(0));
	sprintf(debugfs_buf, "mac_l3a1r0                 :%#x\n",
		mac_l3a1r0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a1r0_fops = {
	.read = mac_l3a1r0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a0r7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a0r7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(7));
	sprintf(debugfs_buf, "mac_l3a0r7                 :%#x\n",
		mac_l3a0r7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a0r7_fops = {
	.read = mac_l3a0r7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a0r6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a0r6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(6));
	sprintf(debugfs_buf, "mac_l3a0r6                 :%#x\n",
		mac_l3a0r6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a0r6_fops = {
	.read = mac_l3a0r6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a0r5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a0r5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(5));
	sprintf(debugfs_buf, "mac_l3a0r5                 :%#x\n",
		mac_l3a0r5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a0r5_fops = {
	.read = mac_l3a0r5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a0r4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a0r4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(4));
	sprintf(debugfs_buf, "mac_l3a0r4                 :%#x\n",
		mac_l3a0r4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a0r4_fops = {
	.read = mac_l3a0r4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a0r3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a0r3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(3));
	sprintf(debugfs_buf, "mac_l3a0r3                 :%#x\n",
		mac_l3a0r3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a0r3_fops = {
	.read = mac_l3a0r3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a0r2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a0r2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(2));
	sprintf(debugfs_buf, "mac_l3a0r2                 :%#x\n",
		mac_l3a0r2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a0r2_fops = {
	.read = mac_l3a0r2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a0r1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a0r1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(1));
	sprintf(debugfs_buf, "mac_l3a0r1                 :%#x\n",
		mac_l3a0r1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a0r1_fops = {
	.read = mac_l3a0r1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3a0r0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3a0r0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3_ADDR0(0));
	sprintf(debugfs_buf, "mac_l3a0r0                 :%#x\n",
		mac_l3a0r0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3a0r0_fops = {
	.read = mac_l3a0r0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l4ar7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l4ar7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(7));
	sprintf(debugfs_buf, "mac_l4ar7                 :%#x\n", mac_l4ar7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l4ar7_fops = {
	.read = mac_l4ar7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l4ar6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l4ar6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(6));
	sprintf(debugfs_buf, "mac_l4ar6                 :%#x\n", mac_l4ar6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l4ar6_fops = {
	.read = mac_l4ar6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l4ar5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l4ar5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(5));
	sprintf(debugfs_buf, "mac_l4ar5                 :%#x\n", mac_l4ar5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l4ar5_fops = {
	.read = mac_l4ar5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l4ar4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l4ar4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(4));
	sprintf(debugfs_buf, "mac_l4ar4                 :%#x\n", mac_l4ar4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l4ar4_fops = {
	.read = mac_l4ar4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l4ar3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l4ar3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(3));
	sprintf(debugfs_buf, "mac_l4ar3                 :%#x\n", mac_l4ar3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l4ar3_fops = {
	.read = mac_l4ar3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l4ar2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l4ar2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(2));
	sprintf(debugfs_buf, "mac_l4ar2                 :%#x\n", mac_l4ar2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l4ar2_fops = {
	.read = mac_l4ar2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l4ar1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l4ar1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(1));
	sprintf(debugfs_buf, "mac_l4ar1                 :%#x\n", mac_l4ar1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l4ar1_fops = {
	.read = mac_l4ar1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l4ar0_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l4ar0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L4_ADDR(0));
	sprintf(debugfs_buf, "mac_l4ar0                 :%#x\n", mac_l4ar0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l4ar0_fops = {
	.read = mac_l4ar0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3l4cr7_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3l4cr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(7));
	sprintf(debugfs_buf, "mac_l3l4cr7                :%#x\n",
		mac_l3l4cr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3l4cr7_fops = {
	.read = mac_l3l4cr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3l4cr6_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3l4cr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(6));
	sprintf(debugfs_buf, "mac_l3l4cr6                :%#x\n",
		mac_l3l4cr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3l4cr6_fops = {
	.read = mac_l3l4cr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3l4cr5_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3l4cr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(5));
	sprintf(debugfs_buf, "mac_l3l4cr5                :%#x\n",
		mac_l3l4cr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3l4cr5_fops = {
	.read = mac_l3l4cr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3l4cr4_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3l4cr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(4));
	sprintf(debugfs_buf, "mac_l3l4cr4                :%#x\n",
		mac_l3l4cr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3l4cr4_fops = {
	.read = mac_l3l4cr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3l4cr3_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3l4cr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(3));
	sprintf(debugfs_buf, "mac_l3l4cr3                :%#x\n",
		mac_l3l4cr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3l4cr3_fops = {
	.read = mac_l3l4cr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3l4cr2_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3l4cr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(2));
	sprintf(debugfs_buf, "mac_l3l4cr2                :%#x\n",
		mac_l3l4cr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3l4cr2_fops = {
	.read = mac_l3l4cr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3l4cr1_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3l4cr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(1));
	sprintf(debugfs_buf, "mac_l3l4cr1                :%#x\n",
		mac_l3l4cr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3l4cr1_fops = {
	.read = mac_l3l4cr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_l3l4cr0_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_l3l4cr0_val = dwceqos_readl(pdata, DWCEQOS_MAC_L3L4_CTRL(0));
	sprintf(debugfs_buf, "mac_l3l4cr0                :%#x\n",
		mac_l3l4cr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_l3l4cr0_fops = {
	.read = mac_l3l4cr0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_gpios_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_gpio_val = dwceqos_readl(pdata, DWCEQOS_MAC_GPIO_STAT);
	sprintf(debugfs_buf, "mac_gpio                  :%#x\n",
		mac_gpio_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_gpio_fops = {
	.read = mac_gpios_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pcs_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pcs_val = dwceqos_readl(pdata, DWCEQOS_MAC_PHYIF_CS);
	sprintf(debugfs_buf, "mac_pcs                    :%#x\n", mac_pcs_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pcs_fops = {
	.read = mac_pcs_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_tes_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_tes_val = dwceqos_readl(pdata, DWCEQOS_MAC_TBI_EXTENDED_STAT);
	sprintf(debugfs_buf, "mac_tes                    :%#x\n", mac_tes_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tes_fops = {
	.read = mac_tes_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ae_read(struct file *file, char __user * userbuf,
			   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ae_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_EXPANSION);
	sprintf(debugfs_buf, "mac_ae                     :%#x\n", mac_ae_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ae_fops = {
	.read = mac_ae_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_alpa_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_alpa_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_LPA);
	sprintf(debugfs_buf, "mac_alpa                   :%#x\n", mac_alpa_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_alpa_fops = {
	.read = mac_alpa_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_aad_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_aad_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_ADVERTISEMENT);
	sprintf(debugfs_buf, "mac_aad                    :%#x\n", mac_aad_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_aad_fops = {
	.read = mac_aad_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ans_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ans_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_STAT);
	sprintf(debugfs_buf, "mac_ans                    :%#x\n", mac_ans_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ans_fops = {
	.read = mac_ans_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_anc_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_anc_val = dwceqos_readl(pdata, DWCEQOS_MAC_AN_CTRL);
	sprintf(debugfs_buf, "mac_anc                    :%#x\n", mac_anc_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_anc_fops = {
	.read = mac_anc_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_lpc_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_lpc_val = dwceqos_readl(pdata, DWCEQOS_MAC_LPI_TIMERS_CTRL);
	sprintf(debugfs_buf, "mac_lpc                    :%#x\n", mac_lpc_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_lpc_fops = {
	.read = mac_lpc_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_lps_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_lps_val = dwceqos_readl(pdata, DWCEQOS_MAC_LPI_CS);
	sprintf(debugfs_buf, "mac_lps                    :%#x\n", mac_lps_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_lps_fops = {
	.read = mac_lps_read,
	.write = dwc_eqos_write,
};


static ssize_t mac_lmir_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;
	mac_lmir_val = dwceqos_readl(pdata, DWCEQOS_MAC_LOG_MSG_INTVL);
	sprintf(debugfs_buf, "mac_lmir                  :%#x\n", mac_lmir_val);
	ret = simple_read_from_buffer(userbuf, count, ppos, debugfs_buf, strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_lmir_fops = {
	.read = mac_lmir_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_spi2r_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;
	mac_spi2r_val = dwceqos_readl(pdata, DWCEQOS_MAC_SRC_PORT_ID2);
	sprintf(debugfs_buf, "mac_spi2r                  :%#x\n", mac_spi2r_val);
	ret = simple_read_from_buffer(userbuf, count, ppos, debugfs_buf, strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_spi2r_fops = {
	.read = mac_spi2r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_spi1r_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;
	mac_spi1r_val = dwceqos_readl(pdata, DWCEQOS_MAC_SRC_PORT_ID1);
	sprintf(debugfs_buf, "mac_spi1r                  :%#x\n", mac_spi1r_val);
	ret = simple_read_from_buffer(userbuf, count, ppos, debugfs_buf, strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_spi1r_fops = {
	.read = mac_spi1r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_spi0r_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;
	mac_spi0r_val = dwceqos_readl(pdata, DWCEQOS_MAC_SRC_PORT_ID0);
	sprintf(debugfs_buf, "mac_spi0r                  :%#x\n", mac_spi0r_val);
	ret = simple_read_from_buffer(userbuf, count, ppos, debugfs_buf, strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_spi0r_fops = {
	.read = mac_spi0r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pto_cr_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;
	mac_pto_cr_val = dwceqos_readl(pdata, DWCEQOS_MAC_PTO_CTRL);
	sprintf(debugfs_buf, "mac_pto_cr                 :%#x\n", mac_pto_cr_val);
	ret = simple_read_from_buffer(userbuf, count, ppos, debugfs_buf, strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pto_cr_fops = {
	.read = mac_pto_cr_read,
	.write = dwc_eqos_write,
};



static ssize_t mac_pps_width3_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_width3_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_WIDTH(3));
	sprintf(debugfs_buf, "mac_pps_width3             :%#x\n",
		mac_pps_width3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_width3_fops = {
	.read = mac_pps_width3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_width2_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_width2_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_WIDTH(2));
	sprintf(debugfs_buf, "mac_pps_width2             :%#x\n",
		mac_pps_width2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_width2_fops = {
	.read = mac_pps_width2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_width1_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_width1_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_WIDTH(1));
	sprintf(debugfs_buf, "mac_pps_width1             :%#x\n",
		mac_pps_width1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_width1_fops = {
	.read = mac_pps_width1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_width0_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_width0_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_WIDTH(0));
	sprintf(debugfs_buf, "mac_pps_width0             :%#x\n",
		mac_pps_width0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_width0_fops = {
	.read = mac_pps_width0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_intval3_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_intval3_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_INTERVAL(3));
	sprintf(debugfs_buf, "mac_pps_intval3            :%#x\n",
		mac_pps_intval3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_intval3_fops = {
	.read = mac_pps_intval3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_intval2_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_intval2_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_INTERVAL(2));
	sprintf(debugfs_buf, "mac_pps_intval2            :%#x\n",
		mac_pps_intval2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_intval2_fops = {
	.read = mac_pps_intval2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_intval1_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_intval1_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_INTERVAL(1));
	sprintf(debugfs_buf, "mac_pps_intval1            :%#x\n",
		mac_pps_intval1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_intval1_fops = {
	.read = mac_pps_intval1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_intval0_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_intval0_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_INTERVAL(0));
	sprintf(debugfs_buf, "mac_pps_intval0            :%#x\n",
		mac_pps_intval0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_intval0_fops = {
	.read = mac_pps_intval0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_ttns3_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_ttns3_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTNS(3));
	sprintf(debugfs_buf, "mac_pps_ttns3              :%#x\n",
		mac_pps_ttns3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_ttns3_fops = {
	.read = mac_pps_ttns3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_ttns2_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_ttns2_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTNS(2));
	sprintf(debugfs_buf, "mac_pps_ttns2              :%#x\n",
		mac_pps_ttns2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_ttns2_fops = {
	.read = mac_pps_ttns2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_ttns1_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_ttns1_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTNS(1));
	sprintf(debugfs_buf, "mac_pps_ttns1              :%#x\n",
		mac_pps_ttns1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_ttns1_fops = {
	.read = mac_pps_ttns1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_ttns0_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_ttns0_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTNS(0));
	sprintf(debugfs_buf, "mac_pps_ttns0              :%#x\n",
		mac_pps_ttns0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_ttns0_fops = {
	.read = mac_pps_ttns0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_tts3_read(struct file *file, char __user * userbuf,
				 size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_tts3_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTS(3));
	sprintf(debugfs_buf, "mac_pps_tts3               :%#x\n",
		mac_pps_tts3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_tts3_fops = {
	.read = mac_pps_tts3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_tts2_read(struct file *file, char __user * userbuf,
				 size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_tts2_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTS(2));
	sprintf(debugfs_buf, "mac_pps_tts2               :%#x\n",
		mac_pps_tts2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_tts2_fops = {
	.read = mac_pps_tts2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_tts1_read(struct file *file, char __user * userbuf,
				 size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_tts1_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTS(1));
	sprintf(debugfs_buf, "mac_pps_tts1               :%#x\n",
		mac_pps_tts1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_tts1_fops = {
	.read = mac_pps_tts1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pps_tts0_read(struct file *file, char __user * userbuf,
				 size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pps_tts0_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_TTS(0));
	sprintf(debugfs_buf, "mac_pps_tts0               :%#x\n",
		mac_pps_tts0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pps_tts0_fops = {
	.read = mac_pps_tts0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ppsc_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ppsc_val = dwceqos_readl(pdata, DWCEQOS_MAC_PPS_CTRL);
	sprintf(debugfs_buf, "mac_ppsc                   :%#x\n", mac_ppsc_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ppsc_fops = {
	.read = mac_ppsc_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_teac_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_teac_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_EG_ASYM_CORR);
	sprintf(debugfs_buf, "mac_teac                   :%#x\n", mac_teac_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_teac_fops = {
	.read = mac_teac_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_tiac_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_tiac_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_ING_ASYM_CORR);
	sprintf(debugfs_buf, "mac_tiac                   :%#x\n", mac_tiac_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tiac_fops = {
	.read = mac_tiac_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ats_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ats_val = dwceqos_readl(pdata, DWCEQOS_MAC_AUX_TS_SEC);
	sprintf(debugfs_buf, "mac_ats                    :%#x\n", mac_ats_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ats_fops = {
	.read = mac_ats_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_atn_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_atn_val = dwceqos_readl(pdata, DWCEQOS_MAC_AUX_TS_NSEC);
	sprintf(debugfs_buf, "mac_atn                    :%#x\n", mac_atn_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_atn_fops = {
	.read = mac_atn_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ac_read(struct file *file, char __user * userbuf,
			   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ac_val = dwceqos_readl(pdata, DWCEQOS_MAC_AUX_CTRL);
	sprintf(debugfs_buf, "mac_ac                     :%#x\n", mac_ac_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ac_fops = {
	.read = mac_ac_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ttn_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ttn_val = dwceqos_readl(pdata, DWCEQOS_MAC_TX_TS_STAT_SEC);
	sprintf(debugfs_buf, "mac_ttn                    :%#x\n", mac_ttn_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ttn_fops = {
	.read = mac_ttn_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ttsn_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ttsn_val = dwceqos_readl(pdata, DWCEQOS_MAC_TX_TS_STAT_NSEC);
	sprintf(debugfs_buf, "mac_ttsn                   :%#x\n", mac_ttsn_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ttsn_fops = {
	.read = mac_ttsn_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_tsr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_tsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_STAT);
	sprintf(debugfs_buf, "mac_tsr                    :%#x\n", mac_tsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tsr_fops = {
	.read = mac_tsr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_sthwr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_sthwr_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_H_WORD_SEC);
	sprintf(debugfs_buf, "mac_sthwr                  :%#x\n",
		mac_sthwr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_sthwr_fops = {
	.read = mac_sthwr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_tar_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_tar_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_ADDEND);
	sprintf(debugfs_buf, "mac_tar                    :%#x\n", mac_tar_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tar_fops = {
	.read = mac_tar_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_stnsur_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_stnsur_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_NSEC_UPD);
	sprintf(debugfs_buf, "mac_stnsur                 :%#x\n",
		mac_stnsur_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_stnsur_fops = {
	.read = mac_stnsur_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_stsur_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_stsur_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_SEC_UPD);
	sprintf(debugfs_buf, "mac_stsur                  :%#x\n",
		mac_stsur_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_stsur_fops = {
	.read = mac_stsur_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_stnsr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_stnsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_SYS_TIME_NSEC);
	sprintf(debugfs_buf, "mac_stnsr                  :%#x\n",
		mac_stnsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_stnsr_fops = {
	.read = mac_stnsr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_stsr_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_stsr_val = dwceqos_readl(pdata,  DWCEQOS_MAC_SYS_TIME_SEC);
	sprintf(debugfs_buf, "mac_stsr                   :%#x\n", mac_stsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_stsr_fops = {
	.read = mac_stsr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ssir_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ssir_val = dwceqos_readl(pdata, DWCEQOS_MAC_SUB_SEC_INC);
	sprintf(debugfs_buf, "mac_ssir                   :%#x\n", mac_ssir_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ssir_fops = {
	.read = mac_ssir_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_tcr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_tcr_val = dwceqos_readl(pdata, DWCEQOS_MAC_TS_CTRL);
	sprintf(debugfs_buf, "mac_tcr                    :%#x\n", mac_tcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tcr_fops = {
	.read = mac_tcr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_dsr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_dsr_val = dwceqos_readl(pdata, DWCEQOS_MTL_DBG_STS);
	sprintf(debugfs_buf, "mtl_dsr                    :%#x\n", mtl_dsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_dsr_fops = {
	.read = mtl_dsr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_rwpffr_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_rwpffr_val = dwceqos_readl(pdata, DWCEQOS_MAC_RWK_PKT_FILT);
	sprintf(debugfs_buf, "mac_rwpffr                 :%#x\n",
		mac_rwpffr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rwpffr_fops = {
	.read = mac_rwpffr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_rtsr_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_rtsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_RX_TX_STAT);
	sprintf(debugfs_buf, "mac_rtsr                   :%#x\n", mac_rtsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rtsr_fops = {
	.read = mac_rtsr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_ier_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	//MTL_IER_RgRd(mtl_ier_val);
	sprintf(debugfs_buf, "mtl_ier                    :%#x\n", mtl_ier_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_ier_fops = {
	.read = mtl_ier_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrcr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrcr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(7));
	sprintf(debugfs_buf, "mtl_qrcr7                  :%#x\n",
		mtl_qrcr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrcr7_fops = {
	.read = mtl_qrcr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrcr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrcr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(6));
	sprintf(debugfs_buf, "mtl_qrcr6                  :%#x\n",
		mtl_qrcr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrcr6_fops = {
	.read = mtl_qrcr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrcr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrcr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(5));
	sprintf(debugfs_buf, "mtl_qrcr5                  :%#x\n",
		mtl_qrcr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrcr5_fops = {
	.read = mtl_qrcr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrcr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrcr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(4));
	sprintf(debugfs_buf, "mtl_qrcr4                  :%#x\n",
		mtl_qrcr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrcr4_fops = {
	.read = mtl_qrcr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrcr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrcr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(3));
	sprintf(debugfs_buf, "mtl_qrcr3                  :%#x\n",
		mtl_qrcr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrcr3_fops = {
	.read = mtl_qrcr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrcr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrcr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(2));
	sprintf(debugfs_buf, "mtl_qrcr2                  :%#x\n",
		mtl_qrcr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrcr2_fops = {
	.read = mtl_qrcr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrcr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrcr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(1));
	sprintf(debugfs_buf, "mtl_qrcr1                  :%#x\n",
		mtl_qrcr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrcr1_fops = {
	.read = mtl_qrcr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrdr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrdr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(7));
	sprintf(debugfs_buf, "mtl_qrdr7                  :%#x\n",
		mtl_qrdr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrdr7_fops = {
	.read = mtl_qrdr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrdr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrdr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(6));
	sprintf(debugfs_buf, "mtl_qrdr6                  :%#x\n",
		mtl_qrdr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrdr6_fops = {
	.read = mtl_qrdr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrdr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrdr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(5));
	sprintf(debugfs_buf, "mtl_qrdr5                  :%#x\n",
		mtl_qrdr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrdr5_fops = {
	.read = mtl_qrdr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrdr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrdr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(4));
	sprintf(debugfs_buf, "mtl_qrdr4                  :%#x\n",
		mtl_qrdr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrdr4_fops = {
	.read = mtl_qrdr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrdr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrdr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(3));
	sprintf(debugfs_buf, "mtl_qrdr3                  :%#x\n",
		mtl_qrdr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrdr3_fops = {
	.read = mtl_qrdr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrdr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrdr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(2));
	sprintf(debugfs_buf, "mtl_qrdr2                  :%#x\n",
		mtl_qrdr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrdr2_fops = {
	.read = mtl_qrdr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qrdr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qrdr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(1));
	sprintf(debugfs_buf, "mtl_qrdr1                  :%#x\n",
		mtl_qrdr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qrdr1_fops = {
	.read = mtl_qrdr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qocr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qocr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(7));
	sprintf(debugfs_buf, "mtl_qocr7                  :%#x\n",
		mtl_qocr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qocr7_fops = {
	.read = mtl_qocr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qocr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qocr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(6));
	sprintf(debugfs_buf, "mtl_qocr6                  :%#x\n",
		mtl_qocr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qocr6_fops = {
	.read = mtl_qocr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qocr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qocr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(5));
	sprintf(debugfs_buf, "mtl_qocr5                  :%#x\n",
		mtl_qocr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qocr5_fops = {
	.read = mtl_qocr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qocr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qocr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(4));
	sprintf(debugfs_buf, "mtl_qocr4                  :%#x\n",
		mtl_qocr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qocr4_fops = {
	.read = mtl_qocr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qocr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qocr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(3));
	sprintf(debugfs_buf, "mtl_qocr3                  :%#x\n",
		mtl_qocr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qocr3_fops = {
	.read = mtl_qocr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qocr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qocr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(2));
	sprintf(debugfs_buf, "mtl_qocr2                  :%#x\n",
		mtl_qocr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qocr2_fops = {
	.read = mtl_qocr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qocr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qocr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(1));
	sprintf(debugfs_buf, "mtl_qocr1                  :%#x\n",
		mtl_qocr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qocr1_fops = {
	.read = mtl_qocr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qromr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qromr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(7));
	sprintf(debugfs_buf, "mtl_qromr7                 :%#x\n",
		mtl_qromr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qromr7_fops = {
	.read = mtl_qromr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qromr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qromr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(6));
	sprintf(debugfs_buf, "mtl_qromr6                 :%#x\n",
		mtl_qromr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qromr6_fops = {
	.read = mtl_qromr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qromr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qromr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(5));
	sprintf(debugfs_buf, "mtl_qromr5                 :%#x\n",
		mtl_qromr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qromr5_fops = {
	.read = mtl_qromr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qromr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qromr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(4));
	sprintf(debugfs_buf, "mtl_qromr4                 :%#x\n",
		mtl_qromr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qromr4_fops = {
	.read = mtl_qromr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qromr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qromr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(3));
	sprintf(debugfs_buf, "mtl_qromr3                 :%#x\n",
		mtl_qromr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qromr3_fops = {
	.read = mtl_qromr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qromr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qromr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(2));
	sprintf(debugfs_buf, "mtl_qromr2                 :%#x\n",
		mtl_qromr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qromr2_fops = {
	.read = mtl_qromr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qromr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qromr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(1));
	sprintf(debugfs_buf, "mtl_qromr1                 :%#x\n",
		mtl_qromr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qromr1_fops = {
	.read = mtl_qromr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qlcr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qlcr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(7));
	sprintf(debugfs_buf, "mtl_qlcr7                  :%#x\n",
		mtl_qlcr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qlcr7_fops = {
	.read = mtl_qlcr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qlcr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qlcr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(6));
	sprintf(debugfs_buf, "mtl_qlcr6                  :%#x\n",
		mtl_qlcr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qlcr6_fops = {
	.read = mtl_qlcr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qlcr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qlcr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(5));
	sprintf(debugfs_buf, "mtl_qlcr5                  :%#x\n",
		mtl_qlcr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qlcr5_fops = {
	.read = mtl_qlcr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qlcr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qlcr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(4));
	sprintf(debugfs_buf, "mtl_qlcr4                  :%#x\n",
		mtl_qlcr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qlcr4_fops = {
	.read = mtl_qlcr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qlcr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qlcr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(3));
	sprintf(debugfs_buf, "mtl_qlcr3                  :%#x\n",
		mtl_qlcr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qlcr3_fops = {
	.read = mtl_qlcr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qlcr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qlcr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(2));
	sprintf(debugfs_buf, "mtl_qlcr2                  :%#x\n",
		mtl_qlcr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qlcr2_fops = {
	.read = mtl_qlcr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qlcr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qlcr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_LC(1));
	sprintf(debugfs_buf, "mtl_qlcr1                  :%#x\n",
		mtl_qlcr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qlcr1_fops = {
	.read = mtl_qlcr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qhcr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qhcr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(7));
	sprintf(debugfs_buf, "mtl_qhcr7                  :%#x\n",
		mtl_qhcr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qhcr7_fops = {
	.read = mtl_qhcr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qhcr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qhcr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(6));
	sprintf(debugfs_buf, "mtl_qhcr6                  :%#x\n",
		mtl_qhcr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qhcr6_fops = {
	.read = mtl_qhcr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qhcr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qhcr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(5));
	sprintf(debugfs_buf, "mtl_qhcr5                  :%#x\n",
		mtl_qhcr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qhcr5_fops = {
	.read = mtl_qhcr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qhcr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qhcr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(4));
	sprintf(debugfs_buf, "mtl_qhcr4                  :%#x\n",
		mtl_qhcr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qhcr4_fops = {
	.read = mtl_qhcr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qhcr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qhcr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(3));
	sprintf(debugfs_buf, "mtl_qhcr3                  :%#x\n",
		mtl_qhcr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qhcr3_fops = {
	.read = mtl_qhcr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qhcr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qhcr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(2));
	sprintf(debugfs_buf, "mtl_qhcr2                  :%#x\n",
		mtl_qhcr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qhcr2_fops = {
	.read = mtl_qhcr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qhcr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qhcr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_HC(1));
	sprintf(debugfs_buf, "mtl_qhcr1                  :%#x\n",
		mtl_qhcr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qhcr1_fops = {
	.read = mtl_qhcr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qsscr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qsscr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(7));
	sprintf(debugfs_buf, "mtl_qsscr7                 :%#x\n",
		mtl_qsscr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qsscr7_fops = {
	.read = mtl_qsscr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qsscr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qsscr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(6));
	sprintf(debugfs_buf, "mtl_qsscr6                 :%#x\n",
		mtl_qsscr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qsscr6_fops = {
	.read = mtl_qsscr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qsscr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qsscr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(5));
	sprintf(debugfs_buf, "mtl_qsscr5                 :%#x\n",
		mtl_qsscr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qsscr5_fops = {
	.read = mtl_qsscr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qsscr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qsscr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(4));
	sprintf(debugfs_buf, "mtl_qsscr4                 :%#x\n",
		mtl_qsscr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qsscr4_fops = {
	.read = mtl_qsscr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qsscr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qsscr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(3));
	sprintf(debugfs_buf, "mtl_qsscr3                 :%#x\n",
		mtl_qsscr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qsscr3_fops = {
	.read = mtl_qsscr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qsscr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qsscr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(2));
	sprintf(debugfs_buf, "mtl_qsscr2                 :%#x\n",
		mtl_qsscr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qsscr2_fops = {
	.read = mtl_qsscr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qsscr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qsscr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_SSC(1));
	sprintf(debugfs_buf, "mtl_qsscr1                 :%#x\n",
		mtl_qsscr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qsscr1_fops = {
	.read = mtl_qsscr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qw7_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qw7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(7));
	sprintf(debugfs_buf, "mtl_qw7                    :%#x\n", mtl_qw7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qw7_fops = {
	.read = mtl_qw7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qw6_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qw6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(6));
	sprintf(debugfs_buf, "mtl_qw6                    :%#x\n", mtl_qw6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qw6_fops = {
	.read = mtl_qw6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qw5_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qw5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(5));
	sprintf(debugfs_buf, "mtl_qw5                    :%#x\n", mtl_qw5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qw5_fops = {
	.read = mtl_qw5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qw4_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qw4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(4));
	sprintf(debugfs_buf, "mtl_qw4                    :%#x\n", mtl_qw4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qw4_fops = {
	.read = mtl_qw4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qw3_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qw3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(3));
	sprintf(debugfs_buf, "mtl_qw3                    :%#x\n", mtl_qw3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qw3_fops = {
	.read = mtl_qw3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qw2_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qw2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(2));
	sprintf(debugfs_buf, "mtl_qw2                    :%#x\n", mtl_qw2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qw2_fops = {
	.read = mtl_qw2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qw1_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qw1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(1));
	sprintf(debugfs_buf, "mtl_qw1                    :%#x\n", mtl_qw1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qw1_fops = {
	.read = mtl_qw1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qesr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qesr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(7));
	sprintf(debugfs_buf, "mtl_qesr7                  :%#x\n",
		mtl_qesr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qesr7_fops = {
	.read = mtl_qesr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qesr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qesr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(6));
	sprintf(debugfs_buf, "mtl_qesr6                  :%#x\n",
		mtl_qesr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qesr6_fops = {
	.read = mtl_qesr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qesr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qesr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(5));
	sprintf(debugfs_buf, "mtl_qesr5                  :%#x\n",
		mtl_qesr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qesr5_fops = {
	.read = mtl_qesr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qesr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qesr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(4));
	sprintf(debugfs_buf, "mtl_qesr4                  :%#x\n",
		mtl_qesr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qesr4_fops = {
	.read = mtl_qesr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qesr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qesr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(3));
	sprintf(debugfs_buf, "mtl_qesr3                  :%#x\n",
		mtl_qesr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qesr3_fops = {
	.read = mtl_qesr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qesr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qesr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(2));
	sprintf(debugfs_buf, "mtl_qesr2                  :%#x\n",
		mtl_qesr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qesr2_fops = {
	.read = mtl_qesr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qesr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qesr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(1));
	sprintf(debugfs_buf, "mtl_qesr1                  :%#x\n",
		mtl_qesr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qesr1_fops = {
	.read = mtl_qesr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qecr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qecr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(7));
	sprintf(debugfs_buf, "mtl_qecr7                  :%#x\n",
		mtl_qecr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qecr7_fops = {
	.read = mtl_qecr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qecr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qecr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(6));
	sprintf(debugfs_buf, "mtl_qecr6                  :%#x\n",
		mtl_qecr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qecr6_fops = {
	.read = mtl_qecr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qecr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qecr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(5));
	sprintf(debugfs_buf, "mtl_qecr5                  :%#x\n",
		mtl_qecr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qecr5_fops = {
	.read = mtl_qecr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qecr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qecr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(4));
	sprintf(debugfs_buf, "mtl_qecr4                  :%#x\n",
		mtl_qecr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qecr4_fops = {
	.read = mtl_qecr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qecr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qecr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(3));
	sprintf(debugfs_buf, "mtl_qecr3                  :%#x\n",
		mtl_qecr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qecr3_fops = {
	.read = mtl_qecr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qecr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qecr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(2));
	sprintf(debugfs_buf, "mtl_qecr2                  :%#x\n",
		mtl_qecr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qecr2_fops = {
	.read = mtl_qecr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qecr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qecr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(1));
	sprintf(debugfs_buf, "mtl_qecr1                  :%#x\n",
		mtl_qecr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qecr1_fops = {
	.read = mtl_qecr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtdr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtdr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(7));
	sprintf(debugfs_buf, "mtl_qtdr7                  :%#x\n",
		mtl_qtdr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtdr7_fops = {
	.read = mtl_qtdr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtdr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtdr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(6));
	sprintf(debugfs_buf, "mtl_qtdr6                  :%#x\n",
		mtl_qtdr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtdr6_fops = {
	.read = mtl_qtdr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtdr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtdr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(5));
	sprintf(debugfs_buf, "mtl_qtdr5                  :%#x\n",
		mtl_qtdr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtdr5_fops = {
	.read = mtl_qtdr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtdr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtdr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(4));
	sprintf(debugfs_buf, "mtl_qtdr4                  :%#x\n",
		mtl_qtdr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtdr4_fops = {
	.read = mtl_qtdr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtdr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtdr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(3));
	sprintf(debugfs_buf, "mtl_qtdr3                  :%#x\n",
		mtl_qtdr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtdr3_fops = {
	.read = mtl_qtdr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtdr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtdr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(2));
	sprintf(debugfs_buf, "mtl_qtdr2                  :%#x\n",
		mtl_qtdr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtdr2_fops = {
	.read = mtl_qtdr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtdr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtdr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(1));
	sprintf(debugfs_buf, "mtl_qtdr1                  :%#x\n",
		mtl_qtdr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtdr1_fops = {
	.read = mtl_qtdr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qucr7_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qucr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(7));
	sprintf(debugfs_buf, "mtl_qucr7                  :%#x\n",
		mtl_qucr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qucr7_fops = {
	.read = mtl_qucr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qucr6_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qucr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(6));
	sprintf(debugfs_buf, "mtl_qucr6                  :%#x\n",
		mtl_qucr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qucr6_fops = {
	.read = mtl_qucr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qucr5_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qucr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(5));
	sprintf(debugfs_buf, "mtl_qucr5                  :%#x\n",
		mtl_qucr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qucr5_fops = {
	.read = mtl_qucr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qucr4_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qucr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(4));
	sprintf(debugfs_buf, "mtl_qucr4                  :%#x\n",
		mtl_qucr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qucr4_fops = {
	.read = mtl_qucr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qucr3_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qucr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(3));
	sprintf(debugfs_buf, "mtl_qucr3                  :%#x\n",
		mtl_qucr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qucr3_fops = {
	.read = mtl_qucr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qucr2_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qucr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(2));
	sprintf(debugfs_buf, "mtl_qucr2                  :%#x\n",
		mtl_qucr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qucr2_fops = {
	.read = mtl_qucr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qucr1_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qucr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(1));
	sprintf(debugfs_buf, "mtl_qucr1                  :%#x\n",
		mtl_qucr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qucr1_fops = {
	.read = mtl_qucr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtomr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtomr7_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(7));
	sprintf(debugfs_buf, "mtl_qtomr7                 :%#x\n",
		mtl_qtomr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtomr7_fops = {
	.read = mtl_qtomr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtomr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtomr6_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(6));
	sprintf(debugfs_buf, "mtl_qtomr6                 :%#x\n",
		mtl_qtomr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtomr6_fops = {
	.read = mtl_qtomr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtomr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtomr5_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(5));
	sprintf(debugfs_buf, "mtl_qtomr5                 :%#x\n",
		mtl_qtomr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtomr5_fops = {
	.read = mtl_qtomr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtomr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtomr4_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(4));
	sprintf(debugfs_buf, "mtl_qtomr4                 :%#x\n",
		mtl_qtomr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtomr4_fops = {
	.read = mtl_qtomr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtomr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtomr3_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(3));
	sprintf(debugfs_buf, "mtl_qtomr3                 :%#x\n",
		mtl_qtomr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtomr3_fops = {
	.read = mtl_qtomr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtomr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtomr2_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(2));
	sprintf(debugfs_buf, "mtl_qtomr2                 :%#x\n",
		mtl_qtomr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtomr2_fops = {
	.read = mtl_qtomr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_qtomr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_qtomr1_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(1));
	sprintf(debugfs_buf, "mtl_qtomr1                 :%#x\n",
		mtl_qtomr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_qtomr1_fops = {
	.read = mtl_qtomr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_pmtcsr_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_pmtcsr_val = dwceqos_readl(pdata, DWCEQOS_MAC_PMT_CS);
	sprintf(debugfs_buf, "mac_pmtcsr                 :%#x\n",
		mac_pmtcsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pmtcsr_fops = {
	.read = mac_pmtcsr_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxicmp_err_octets_read(struct file *file,
					  char __user * userbuf, size_t count,
					  loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxicmp_err_octets_val = dwceqos_readl(pdata, DWCEQOS_RXICMP_ERR_OCTS);
	sprintf(debugfs_buf, "mmc_rxicmp_err_octets      :%#x\n",
		mmc_rxicmp_err_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxicmp_err_octets_fops = {
	.read = mmc_rxicmp_err_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxicmp_gd_octets_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxicmp_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXICMP_GOOD_OCTS);
	sprintf(debugfs_buf, "mmc_rxicmp_gd_octets       :%#x\n",
		mmc_rxicmp_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxicmp_gd_octets_fops = {
	.read = mmc_rxicmp_gd_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxtcp_err_octets_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxtcp_err_octets_val = dwceqos_readl(pdata, DWCEQOS_RXTCP_ERR_OCTS);
	sprintf(debugfs_buf, "mmc_rxtcp_err_octets       :%#x\n",
		mmc_rxtcp_err_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxtcp_err_octets_fops = {
	.read = mmc_rxtcp_err_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxtcp_gd_octets_read(struct file *file,
					char __user * userbuf, size_t count,
					loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxtcp_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXTCP_GOOD_OCTS);
	sprintf(debugfs_buf, "mmc_rxtcp_gd_octets        :%#x\n",
		mmc_rxtcp_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxtcp_gd_octets_fops = {
	.read = mmc_rxtcp_gd_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxudp_err_octets_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxudp_err_octets_val = dwceqos_readl(pdata, DWCEQOS_RXUDP_ERR_OCTS);
	sprintf(debugfs_buf, "mmc_rxudp_err_octets       :%#x\n",
		mmc_rxudp_err_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxudp_err_octets_fops = {
	.read = mmc_rxudp_err_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxudp_gd_octets_read(struct file *file,
					char __user * userbuf, size_t count,
					loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxudp_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXUDP_GOOD_OCTS);
	sprintf(debugfs_buf, "mmc_rxudp_gd_octets        :%#x\n",
		mmc_rxudp_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxudp_gd_octets_fops = {
	.read = mmc_rxudp_gd_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv6_nopay_octets_read(struct file *file,
					    char __user * userbuf, size_t count,
					    loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv6_nopay_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_NO_PAYLOAD_OCTS);
	sprintf(debugfs_buf, "mmc_rxipv6_nopay_octets    :%#x\n",
		mmc_rxipv6_nopay_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv6_nopay_octets_fops = {
	.read = mmc_rxipv6_nopay_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv6_hdrerr_octets_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv6_hdrerr_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_HDR_ERR_OCTS);
	sprintf(debugfs_buf, "mmc_rxipv6_hdrerr_octets   :%#x\n",
		mmc_rxipv6_hdrerr_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv6_hdrerr_octets_fops = {
	.read = mmc_rxipv6_hdrerr_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv6_gd_octets_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv6_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_GOOD_OCTS);
	sprintf(debugfs_buf, "mmc_rxipv6_gd_octets       :%#x\n",
		mmc_rxipv6_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv6_gd_octets_fops = {
	.read = mmc_rxipv6_gd_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_udsbl_octets_read(struct file *file,
					    char __user * userbuf, size_t count,
					    loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_udsbl_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_UDP_CKSM_DSBL_OCTS);
	sprintf(debugfs_buf, "mmc_rxipv4_udsbl_octets    :%#x\n",
		mmc_rxipv4_udsbl_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_udsbl_octets_fops = {
	.read = mmc_rxipv4_udsbl_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_frag_octets_read(struct file *file,
					   char __user * userbuf, size_t count,
					   loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_frag_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_FRAGMENTED_OCTS);
	sprintf(debugfs_buf, "mmc_rxipv4_frag_octets     :%#x\n",
		mmc_rxipv4_frag_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_frag_octets_fops = {
	.read = mmc_rxipv4_frag_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_nopay_octets_read(struct file *file,
					    char __user * userbuf, size_t count,
					    loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_nopay_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_NO_PAYLOAD_OCTS);
	sprintf(debugfs_buf, "mmc_rxipv4_nopay_octets    :%#x\n",
		mmc_rxipv4_nopay_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_nopay_octets_fops = {
	.read = mmc_rxipv4_nopay_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_hdrerr_octets_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_hdrerr_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_HDR_ERR_OCTS);
	sprintf(debugfs_buf, "mmc_rxipv4_hdrerr_octets   :%#x\n",
		mmc_rxipv4_hdrerr_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_hdrerr_octets_fops = {
	.read = mmc_rxipv4_hdrerr_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_gd_octets_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_gd_octets_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_GOOD_OCTS);
	sprintf(debugfs_buf, "mmc_rxipv4_gd_octets       :%#x\n",
		mmc_rxipv4_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_gd_octets_fops = {
	.read = mmc_rxipv4_gd_octets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxicmp_err_pkts_read(struct file *file,
					char __user * userbuf, size_t count,
					loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxicmp_err_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXICMP_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxicmp_err_pkts        :%#x\n",
		mmc_rxicmp_err_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxicmp_err_pkts_fops = {
	.read = mmc_rxicmp_err_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxicmp_gd_pkts_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxicmp_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXICMP_GOOD_PKTS);
	sprintf(debugfs_buf, "mmc_rxicmp_gd_pkts         :%#x\n",
		mmc_rxicmp_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxicmp_gd_pkts_fops = {
	.read = mmc_rxicmp_gd_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxtcp_err_pkts_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxtcp_err_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXTCP_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxtcp_err_pkts         :%#x\n",
		mmc_rxtcp_err_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxtcp_err_pkts_fops = {
	.read = mmc_rxtcp_err_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxtcp_gd_pkts_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxtcp_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXTCP_GOOD_PKTS);
	sprintf(debugfs_buf, "mmc_rxtcp_gd_pkts          :%#x\n",
		mmc_rxtcp_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxtcp_gd_pkts_fops = {
	.read = mmc_rxtcp_gd_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxudp_err_pkts_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxudp_err_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXUDP_ERR_PKT);
	sprintf(debugfs_buf, "mmc_rxudp_err_pkts         :%#x\n",
		mmc_rxudp_err_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxudp_err_pkts_fops = {
	.read = mmc_rxudp_err_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxudp_gd_pkts_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxudp_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXUDP_GOOD_PKTS);
	sprintf(debugfs_buf, "mmc_rxudp_gd_pkts          :%#x\n",
		mmc_rxudp_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxudp_gd_pkts_fops = {
	.read = mmc_rxudp_gd_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv6_nopay_pkts_read(struct file *file,
					  char __user * userbuf, size_t count,
					  loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv6_nopay_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_NO_PAYLOAD_PKTS);
	sprintf(debugfs_buf, "mmc_rxipv6_nopay_pkts      :%#x\n",
		mmc_rxipv6_nopay_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv6_nopay_pkts_fops = {
	.read = mmc_rxipv6_nopay_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv6_hdrerr_pkts_read(struct file *file,
					   char __user * userbuf, size_t count,
					   loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv6_hdrerr_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_HDR_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxipv6_hdrerr_pkts     :%#x\n",
		mmc_rxipv6_hdrerr_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv6_hdrerr_pkts_fops = {
	.read = mmc_rxipv6_hdrerr_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv6_gd_pkts_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv6_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV6_GOOD_PKTS);
	sprintf(debugfs_buf, "mmc_rxipv6_gd_pkts         :%#x\n",
		mmc_rxipv6_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv6_gd_pkts_fops = {
	.read = mmc_rxipv6_gd_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_ubsbl_pkts_read(struct file *file,
					  char __user * userbuf, size_t count,
					  loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_ubsbl_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_UDP_CKSM_DSBLD_PKT);
	sprintf(debugfs_buf, "mmc_rxipv4_ubsbl_pkts      :%#x\n",
		mmc_rxipv4_ubsbl_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_ubsbl_pkts_fops = {
	.read = mmc_rxipv4_ubsbl_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_frag_pkts_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_frag_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_FRAGMENTED_PKTS);
	sprintf(debugfs_buf, "mmc_rxipv4_frag_pkts       :%#x\n",
		mmc_rxipv4_frag_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_frag_pkts_fops = {
	.read = mmc_rxipv4_frag_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_nopay_pkts_read(struct file *file,
					  char __user * userbuf, size_t count,
					  loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_nopay_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_NO_PAYLOAD_PKTS);
	sprintf(debugfs_buf, "mmc_rxipv4_nopay_pkts      :%#x\n",
		mmc_rxipv4_nopay_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_nopay_pkts_fops = {
	.read = mmc_rxipv4_nopay_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_hdrerr_pkts_read(struct file *file,
					   char __user * userbuf, size_t count,
					   loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_hdrerr_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_HDR_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxipv4_hdrerr_pkts     :%#x\n",
		mmc_rxipv4_hdrerr_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_hdrerr_pkts_fops = {
	.read = mmc_rxipv4_hdrerr_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxipv4_gd_pkts_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxipv4_gd_pkts_val = dwceqos_readl(pdata, DWCEQOS_RXIPV4_GOOD_PKTS);
	sprintf(debugfs_buf, "mmc_rxipv4_gd_pkts         :%#x\n",
		mmc_rxipv4_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxipv4_gd_pkts_fops = {
	.read = mmc_rxipv4_gd_pkts_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxctrlpackets_g_read(struct file *file,
					char __user * userbuf, size_t count,
					loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxctrlpackets_g_val = dwceqos_readl(pdata, DWCEQOS_RX_CTRL_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_rxctrlpackets_g        :%#x\n",
		mmc_rxctrlpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxctrlpackets_g_fops = {
	.read = mmc_rxctrlpackets_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxrcverror_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxrcverror_val = dwceqos_readl(pdata, DWCEQOS_RX_RECEIVE_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxrcverror             :%#x\n",
		mmc_rxrcverror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxrcverror_fops = {
	.read = mmc_rxrcverror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxwatchdogerror_read(struct file *file,
					char __user * userbuf, size_t count,
					loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxwatchdogerror_val = dwceqos_readl(pdata, DWCEQOS_RX_WDOG_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxwatchdogerror        :%#x\n",
		mmc_rxwatchdogerror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxwatchdogerror_fops = {
	.read = mmc_rxwatchdogerror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxvlanpackets_gb_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxvlanpackets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_VLAN_PKTS_GOOD_BAD);
	sprintf(debugfs_buf, "mmc_rxvlanpackets_gb       :%#x\n",
		mmc_rxvlanpackets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxvlanpackets_gb_fops = {
	.read = mmc_rxvlanpackets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxfifooverflow_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxfifooverflow_val = dwceqos_readl(pdata, DWCEQOS_RX_FIFO_OFLOW_PKTS);
	sprintf(debugfs_buf, "mmc_rxfifooverflow         :%#x\n",
		mmc_rxfifooverflow_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxfifooverflow_fops = {
	.read = mmc_rxfifooverflow_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxpausepackets_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxpausepackets_val = dwceqos_readl(pdata, DWCEQOS_RX_PAUSE_PKTS);
	sprintf(debugfs_buf, "mmc_rxpausepackets         :%#x\n",
		mmc_rxpausepackets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxpausepackets_fops = {
	.read = mmc_rxpausepackets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxoutofrangetype_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxoutofrangetype_val = dwceqos_readl(pdata, DWCEQOS_RX_OUT_OF_RNG_TYPE_PKTS);
	sprintf(debugfs_buf, "mmc_rxoutofrangetype       :%#x\n",
		mmc_rxoutofrangetype_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxoutofrangetype_fops = {
	.read = mmc_rxoutofrangetype_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxlengtherror_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxlengtherror_val = dwceqos_readl(pdata, DWCEQOS_RX_LEN_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxlengtherror          :%#x\n",
		mmc_rxlengtherror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxlengtherror_fops = {
	.read = mmc_rxlengtherror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxunicastpackets_g_read(struct file *file,
					   char __user * userbuf, size_t count,
					   loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxunicastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_RX_UCT_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_rxunicastpackets_g     :%#x\n",
		mmc_rxunicastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxunicastpackets_g_fops = {
	.read = mmc_rxunicastpackets_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rx1024tomaxoctets_gb_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rx1024tomaxoctets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_1024_MAX_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_rx1024tomaxoctets_gb   :%#x\n",
		mmc_rx1024tomaxoctets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rx1024tomaxoctets_gb_fops = {
	.read = mmc_rx1024tomaxoctets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rx512to1023octets_gb_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rx512to1023octets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_512_1023_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_rx512to1023octets_gb   :%#x\n",
		mmc_rx512to1023octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rx512to1023octets_gb_fops = {
	.read = mmc_rx512to1023octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rx256to511octets_gb_read(struct file *file,
					    char __user * userbuf, size_t count,
					    loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rx256to511octets_gb_val =dwceqos_readl(pdata, DWCEQOS_RX_256_511_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_rx256to511octets_gb    :%#x\n",
		mmc_rx256to511octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rx256to511octets_gb_fops = {
	.read = mmc_rx256to511octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rx128to255octets_gb_read(struct file *file,
					    char __user * userbuf, size_t count,
					    loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rx128to255octets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_128_255_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_rx128to255octets_gb    :%#x\n",
		mmc_rx128to255octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rx128to255octets_gb_fops = {
	.read = mmc_rx128to255octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rx65to127octets_gb_read(struct file *file,
					   char __user * userbuf, size_t count,
					   loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rx65to127octets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_65_127_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_rx65to127octets_gb     :%#x\n",
		mmc_rx65to127octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rx65to127octets_gb_fops = {
	.read = mmc_rx65to127octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rx64octets_gb_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rx64octets_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_64_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_rx64octets_gb          :%#x\n",
		mmc_rx64octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rx64octets_gb_fops = {
	.read = mmc_rx64octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxoversize_g_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxoversize_g_val = dwceqos_readl(pdata, DWCEQOS_RX_OSIZE_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_rxoversize_g           :%#x\n",
		mmc_rxoversize_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxoversize_g_fops = {
	.read = mmc_rxoversize_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxundersize_g_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxundersize_g_val = dwceqos_readl(pdata, DWCEQOS_RX_USIZE_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_rxundersize_g          :%#x\n",
		mmc_rxundersize_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxundersize_g_fops = {
	.read = mmc_rxundersize_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxjabbererror_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxjabbererror_val = dwceqos_readl(pdata, DWCEQOS_RX_JABBER_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxjabbererror          :%#x\n",
		mmc_rxjabbererror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxjabbererror_fops = {
	.read = mmc_rxjabbererror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxrunterror_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxrunterror_val = dwceqos_readl(pdata, DWCEQOS_RX_RUNT_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxrunterror            :%#x\n",
		mmc_rxrunterror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxrunterror_fops = {
	.read = mmc_rxrunterror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxalignmenterror_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxalignmenterror_val = dwceqos_readl(pdata, DWCEQOS_RX_ALGMNT_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxalignmenterror       :%#x\n",
		mmc_rxalignmenterror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxalignmenterror_fops = {
	.read = mmc_rxalignmenterror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxcrcerror_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxcrcerror_val = dwceqos_readl(pdata, DWCEQOS_RX_CRC_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_rxcrcerror             :%#x\n",
		mmc_rxcrcerror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxcrcerror_fops = {
	.read = mmc_rxcrcerror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxmulticastpackets_g_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxmulticastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_RX_MLT_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_rxmulticastpackets_g   :%#x\n",
		mmc_rxmulticastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxmulticastpackets_g_fops = {
	.read = mmc_rxmulticastpackets_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxbroadcastpackets_g_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxbroadcastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_RX_BDT_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_rxbroadcastpackets_g   :%#x\n",
		mmc_rxbroadcastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxbroadcastpackets_g_fops = {
	.read = mmc_rxbroadcastpackets_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxoctetcount_g_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxoctetcount_g_val = dwceqos_readl(pdata, DWCEWOS_RX_OCT_CNT_GOOD);
	sprintf(debugfs_buf, "mmc_rxoctetcount_g         :%#x\n",
		mmc_rxoctetcount_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxoctetcount_g_fops = {
	.read = mmc_rxoctetcount_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxoctetcount_gb_read(struct file *file,
					char __user * userbuf, size_t count,
					loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxoctetcount_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_OCTET_CNT_GOOD_BAD);
	sprintf(debugfs_buf, "mmc_rxoctetcount_gb        :%#x\n",
		mmc_rxoctetcount_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxoctetcount_gb_fops = {
	.read = mmc_rxoctetcount_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_rxpacketcount_gb_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_rxpacketcount_gb_val = dwceqos_readl(pdata, DWCEQOS_RX_PKTS_CNT_GOOD_BAD);
	sprintf(debugfs_buf, "mmc_rxpacketcount_gb       :%#x\n",
		mmc_rxpacketcount_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxpacketcount_gb_fops = {
	.read = mmc_rxpacketcount_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txoversize_g_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txoversize_g_val = dwceqos_readl(pdata, DWCEQOS_TX_OSIZE_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_txoversize_g           :%#x\n",
		mmc_txoversize_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txoversize_g_fops = {
	.read = mmc_txoversize_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txvlanpackets_g_read(struct file *file,
					char __user * userbuf, size_t count,
					loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txvlanpackets_g_val = dwceqos_readl(pdata, DWCEQOS_TX_VLAN_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_txvlanpackets_g        :%#x\n",
		mmc_txvlanpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txvlanpackets_g_fops = {
	.read = mmc_txvlanpackets_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txpausepackets_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txpausepackets_val = dwceqos_readl(pdata, DWCEQOS_TX_PAUSE_PKTS);
	sprintf(debugfs_buf, "mmc_txpausepackets         :%#x\n",
		mmc_txpausepackets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txpausepackets_fops = {
	.read = mmc_txpausepackets_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txexcessdef_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txexcessdef_val = dwceqos_readl(pdata, DWCEQOS_TX_EXCESS_DEF_ERR);
	sprintf(debugfs_buf, "mmc_txexcessdef            :%#x\n",
		mmc_txexcessdef_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txexcessdef_fops = {
	.read = mmc_txexcessdef_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txpacketscount_g_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txpacketscount_g_val = dwceqos_readl(pdata, DWCEQOS_TX_PKT_CNT_GOOD);
	sprintf(debugfs_buf, "mmc_txpacketscount_g       :%#x\n",
		mmc_txpacketscount_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txpacketscount_g_fops = {
	.read = mmc_txpacketscount_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txoctetcount_g_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txoctetcount_g_val = dwceqos_readl(pdata, DWCEWOS_TX_OCT_CNT_GOOD);
	sprintf(debugfs_buf, "mmc_txoctetcount_g         :%#x\n",
		mmc_txoctetcount_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txoctetcount_g_fops = {
	.read = mmc_txoctetcount_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txcarriererror_read(struct file *file, char __user * userbuf,
				       size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txcarriererror_val = dwceqos_readl(pdata, DWCEQOS_TX_CARRIER_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_txcarriererror         :%#x\n",
		mmc_txcarriererror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txcarriererror_fops = {
	.read = mmc_txcarriererror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txexesscol_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txexesscol_val = dwceqos_readl(pdata, DWCEQOS_TX_EXCESS_COLLSN_PKTS);
	sprintf(debugfs_buf, "mmc_txexesscol             :%#x\n",
		mmc_txexesscol_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txexesscol_fops = {
	.read = mmc_txexesscol_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txlatecol_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txlatecol_val = dwceqos_readl(pdata, DWCEQOS_TX_LATE_COLLSN_PKTS);
	sprintf(debugfs_buf, "mmc_txlatecol              :%#x\n",
		mmc_txlatecol_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txlatecol_fops = {
	.read = mmc_txlatecol_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txdeferred_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txdeferred_val = dwceqos_readl(pdata, DWCEQOS_TX_DEF_PKTS);
	sprintf(debugfs_buf, "mmc_txdeferred             :%#x\n",
		mmc_txdeferred_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txdeferred_fops = {
	.read = mmc_txdeferred_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txmulticol_g_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txmulticol_g_val = dwceqos_readl(pdata, DWCEQOS_TX_MUL_COLLSN_GOOD_PKTS);
	sprintf(debugfs_buf, "mmc_txmulticol_g           :%#x\n",
		mmc_txmulticol_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txmulticol_g_fops = {
	.read = mmc_txmulticol_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txsinglecol_g_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txsinglecol_g_val = dwceqos_readl(pdata, DWCEQOS_TX_SGL_COLLSN_GOOD_PKTS);
	sprintf(debugfs_buf, "mmc_txsinglecol_g          :%#x\n",
		mmc_txsinglecol_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txsinglecol_g_fops = {
	.read = mmc_txsinglecol_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txunderflowerror_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txunderflowerror_val = dwceqos_readl(pdata, DWCEQOS_TX_UFLOW_ERR_PKTS);
	sprintf(debugfs_buf, "mmc_txunderflowerror       :%#x\n",
		mmc_txunderflowerror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txunderflowerror_fops = {
	.read = mmc_txunderflowerror_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txbroadcastpackets_gb_read(struct file *file,
					      char __user * userbuf,
					      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txbroadcastpackets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_BDT_PKTS_GOOD_BAD);
	sprintf(debugfs_buf, "mmc_txbroadcastpackets_gb  :%#x\n",
		mmc_txbroadcastpackets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txbroadcastpackets_gb_fops = {
	.read = mmc_txbroadcastpackets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txmulticastpackets_gb_read(struct file *file,
					      char __user * userbuf,
					      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txmulticastpackets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_MLT_PKTS_GOOD_BAD);
	sprintf(debugfs_buf, "mmc_txmulticastpackets_gb  :%#x\n",
		mmc_txmulticastpackets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txmulticastpackets_gb_fops = {
	.read = mmc_txmulticastpackets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txunicastpackets_gb_read(struct file *file,
					    char __user * userbuf, size_t count,
					    loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txunicastpackets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_UCT_PKTS_GOOD_BAD);
	sprintf(debugfs_buf, "mmc_txunicastpackets_gb    :%#x\n",
		mmc_txunicastpackets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txunicastpackets_gb_fops = {
	.read = mmc_txunicastpackets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_tx1024tomaxoctets_gb_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_tx1024tomaxoctets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_1024_MAX_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_tx1024tomaxoctets_gb   :%#x\n",
		mmc_tx1024tomaxoctets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_tx1024tomaxoctets_gb_fops = {
	.read = mmc_tx1024tomaxoctets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_tx512to1023octets_gb_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_tx512to1023octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_512_1023_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_tx512to1023octets_gb   :%#x\n",
		mmc_tx512to1023octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_tx512to1023octets_gb_fops = {
	.read = mmc_tx512to1023octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_tx256to511octets_gb_read(struct file *file,
					    char __user * userbuf, size_t count,
					    loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_tx256to511octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_256_511_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_tx256to511octets_gb    :%#x\n",
		mmc_tx256to511octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_tx256to511octets_gb_fops = {
	.read = mmc_tx256to511octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_tx128to255octets_gb_read(struct file *file,
					    char __user * userbuf, size_t count,
					    loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_tx128to255octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_128_255_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_tx128to255octets_gb    :%#x\n",
		mmc_tx128to255octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_tx128to255octets_gb_fops = {
	.read = mmc_tx128to255octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_tx65to127octets_gb_read(struct file *file,
					   char __user * userbuf, size_t count,
					   loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_tx65to127octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_65_127_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_tx65to127octets_gb     :%#x\n",
		mmc_tx65to127octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_tx65to127octets_gb_fops = {
	.read = mmc_tx65to127octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_tx64octets_gb_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_tx64octets_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_64_OCTS_PKTS_GB);
	sprintf(debugfs_buf, "mmc_tx64octets_gb          :%#x\n",
		mmc_tx64octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_tx64octets_gb_fops = {
	.read = mmc_tx64octets_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txmulticastpackets_g_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txmulticastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_TX_MLT_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_txmulticastpackets_g   :%#x\n",
		mmc_txmulticastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txmulticastpackets_g_fops = {
	.read = mmc_txmulticastpackets_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txbroadcastpackets_g_read(struct file *file,
					     char __user * userbuf,
					     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txbroadcastpackets_g_val = dwceqos_readl(pdata, DWCEQOS_TX_BDT_PKTS_GOOD);
	sprintf(debugfs_buf, "mmc_txbroadcastpackets_g   :%#x\n",
		mmc_txbroadcastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txbroadcastpackets_g_fops = {
	.read = mmc_txbroadcastpackets_g_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txpacketcount_gb_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txpacketcount_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_PKT_CNT_GOOD_BAD);
	sprintf(debugfs_buf, "mmc_txpacketcount_gb       :%#x\n",
		mmc_txpacketcount_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txpacketcount_gb_fops = {
	.read = mmc_txpacketcount_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_txoctetcount_gb_read(struct file *file,
					char __user * userbuf, size_t count,
					loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_txoctetcount_gb_val = dwceqos_readl(pdata, DWCEQOS_TX_OCTET_CNT_GOOD_BAD);
	sprintf(debugfs_buf, "mmc_txoctetcount_gb        :%#x\n",
		mmc_txoctetcount_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txoctetcount_gb_fops = {
	.read = mmc_txoctetcount_gb_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_ipc_intr_rx_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_ipc_intr_rx_val = dwceqos_readl(pdata, DWCEQOS_MMC_IPC_RX_INTR);
	sprintf(debugfs_buf, "mmc_ipc_intr_rx            :%#x\n",
		mmc_ipc_intr_rx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_ipc_intr_rx_fops = {
	.read = mmc_ipc_intr_rx_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_ipc_intr_mask_rx_read(struct file *file,
					 char __user * userbuf, size_t count,
					 loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_ipc_intr_mask_rx_val = dwceqos_readl(pdata, DWCEQOS_MMC_IPC_RX_INTR_MASK);
	sprintf(debugfs_buf, "mmc_ipc_intr_mask_rx       :%#x\n",
		mmc_ipc_intr_mask_rx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_ipc_intr_mask_rx_fops = {
	.read = mmc_ipc_intr_mask_rx_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_intr_mask_tx_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_intr_mask_tx_val = dwceqos_readl(pdata, DWCEQOS_MMC_TX_INTR_MASK);
	sprintf(debugfs_buf, "mmc_intr_mask_tx           :%#x\n",
		mmc_intr_mask_tx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_intr_mask_tx_fops = {
	.read = mmc_intr_mask_tx_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_intr_mask_rx_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_intr_mask_rx_val = dwceqos_readl(pdata, DWCEQOS_MMC_RX_INTR_MASK);
	sprintf(debugfs_buf, "mmc_intr_mask_rx           :%#x\n",
		mmc_intr_mask_rx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_intr_mask_rx_fops = {
	.read = mmc_intr_mask_rx_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_intr_tx_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_intr_tx_val = dwceqos_readl(pdata, DWCEQOS_MMC_TX_INTR);
	sprintf(debugfs_buf, "mmc_intr_tx                :%#x\n",
		mmc_intr_tx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_intr_tx_fops = {
	.read = mmc_intr_tx_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_intr_rx_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_intr_rx_val = dwceqos_readl(pdata, DWCEQOS_MMC_RX_INTR);
	sprintf(debugfs_buf, "mmc_intr_rx                :%#x\n",
		mmc_intr_rx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_intr_rx_fops = {
	.read = mmc_intr_rx_read,
	.write = dwc_eqos_write,
};

static ssize_t mmc_cntrl_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;

	if (!pdata->hw_feat.mmc_sel) {
		printk(KERN_ALERT
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	mmc_cntrl_val = dwceqos_readl(pdata, DWCEQOS_MMC_CTRL);
	sprintf(debugfs_buf, "mmc_cntrl                  :%#x\n",
		mmc_cntrl_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_cntrl_fops = {
	.read = mmc_cntrl_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1lr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1lr_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(1));
	sprintf(debugfs_buf, "mac_ma1lr                  :%#x\n",
		mac_ma1lr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1lr_fops = {
	.read = mac_ma1lr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma1hr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma1hr_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(1));
	sprintf(debugfs_buf, "mac_ma1hr                  :%#x\n",
		mac_ma1hr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma1hr_fops = {
	.read = mac_ma1hr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma0lr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma0lr_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_LOW(0));
	sprintf(debugfs_buf, "mac_ma0lr                  :%#x\n",
		mac_ma0lr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma0lr_fops = {
	.read = mac_ma0lr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ma0hr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ma0hr_val = dwceqos_readl(pdata, DWCEQOS_MAC_ADDR_HIGH(0));
	sprintf(debugfs_buf, "mac_ma0hr       :%#x\n", mac_ma0hr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ma0hr_fops = {
	.read = mac_ma0hr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_gpior_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_gpior_val = dwceqos_readl(pdata, DWCEQOS_MAC_GPIO_CTRL);
	sprintf(debugfs_buf, "mac_gpior       :%#x\n", mac_gpior_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_gpior_fops = {
	.read = mac_gpior_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_gmiidr_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_gmiidr_val = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_DATA);
	sprintf(debugfs_buf, "mac_gmiidr      :%#x\n", mac_gmiidr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_gmiidr_fops = {
	.read = mac_gmiidr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_gmiiar_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_gmiiar_val = dwceqos_readl(pdata, DWCEQOS_MAC_MDIO_ADDR);
	sprintf(debugfs_buf, "mac_gmiiar      :%#x\n", mac_gmiiar_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_gmiiar_fops = {
	.read = mac_gmiiar_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_hfr2_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_hfr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE2);
	sprintf(debugfs_buf, "mac_hfr2        :%#x\n", mac_hfr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_hfr2_fops = {
	.read = mac_hfr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_hfr1_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_hfr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE1);
	sprintf(debugfs_buf, "mac_hfr1        :%#x\n", mac_hfr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_hfr1_fops = {
	.read = mac_hfr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_hfr0_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_hfr0_val = dwceqos_readl(pdata, DWCEQOS_MAC_HW_FEATURE0);
	sprintf(debugfs_buf, "mac_hfr0        :%#x\n", mac_hfr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_hfr0_fops = {
	.read = mac_hfr0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_mdr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_mdr_val = dwceqos_readl(pdata, DWCEQOS_MAC_DBG);
	sprintf(debugfs_buf, "mac_mdr         :%#x\n", mac_mdr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_mdr_fops = {
	.read = mac_mdr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_vr_read(struct file *file, char __user * userbuf,
			   size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_vr_val = dwceqos_readl(pdata, DWCEQOS_MAC_VERSION);
	sprintf(debugfs_buf, "mac_vr          :%#x\n", mac_vr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_vr_fops = {
	.read = mac_vr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_htr7_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_htr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG7);
	sprintf(debugfs_buf, "mac_htr7        :%#x\n", mac_htr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_htr7_fops = {
	.read = mac_htr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_htr6_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_htr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG6);
	sprintf(debugfs_buf, "mac_htr6        :%#x\n", mac_htr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_htr6_fops = {
	.read = mac_htr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_htr5_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_htr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG5);
	sprintf(debugfs_buf, "mac_htr5        :%#x\n", mac_htr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_htr5_fops = {
	.read = mac_htr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_htr4_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_htr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG4);
	sprintf(debugfs_buf, "mac_htr4        :%#x\n", mac_htr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_htr4_fops = {
	.read = mac_htr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_htr3_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_htr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG3);
	sprintf(debugfs_buf, "mac_htr3        :%#x\n", mac_htr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_htr3_fops = {
	.read = mac_htr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_htr2_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_htr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG2);
	sprintf(debugfs_buf, "mac_htr2        :%#x\n", mac_htr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_htr2_fops = {
	.read = mac_htr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_htr1_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_htr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG1);
	sprintf(debugfs_buf, "mac_htr1        :%#x\n", mac_htr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_htr1_fops = {
	.read = mac_htr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_htr0_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_htr0_val = dwceqos_readl(pdata, DWCEQOS_MAC_HT_REG0);
	sprintf(debugfs_buf, "mac_htr0        :%#x\n", mac_htr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_htr0_fops = {
	.read = mac_htr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_riwtr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_riwtr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(7));
	sprintf(debugfs_buf, "dma_riwtr7      :%#x\n", dma_riwtr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_riwtr7_fops = {
	.read = dma_riwtr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_riwtr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_riwtr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(6));
	sprintf(debugfs_buf, "dma_riwtr6      :%#x\n", dma_riwtr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_riwtr6_fops = {
	.read = dma_riwtr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_riwtr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_riwtr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(5));
	sprintf(debugfs_buf, "dma_riwtr5      :%#x\n", dma_riwtr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_riwtr5_fops = {
	.read = dma_riwtr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_riwtr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_riwtr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(4));
	sprintf(debugfs_buf, "dma_riwtr4      :%#x\n", dma_riwtr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_riwtr4_fops = {
	.read = dma_riwtr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_riwtr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_riwtr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(3));
	sprintf(debugfs_buf, "dma_riwtr3      :%#x\n", dma_riwtr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_riwtr3_fops = {
	.read = dma_riwtr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_riwtr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_riwtr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(2));
	sprintf(debugfs_buf, "dma_riwtr2      :%#x\n", dma_riwtr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_riwtr2_fops = {
	.read = dma_riwtr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_riwtr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_riwtr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(1));
	sprintf(debugfs_buf, "dma_riwtr1      :%#x\n", dma_riwtr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_riwtr1_fops = {
	.read = dma_riwtr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_riwtr0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_riwtr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_INTR_WD_TIMER(0));
	sprintf(debugfs_buf, "dma_riwtr0      :%#x\n", dma_riwtr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_riwtr0_fops = {
	.read = dma_riwtr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdrlr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdrlr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(7));
	sprintf(debugfs_buf, "dma_rdrlr7      :%#x\n", dma_rdrlr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdrlr7_fops = {
	.read = dma_rdrlr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdrlr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdrlr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(6));
	sprintf(debugfs_buf, "dma_rdrlr6      :%#x\n", dma_rdrlr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdrlr6_fops = {
	.read = dma_rdrlr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdrlr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdrlr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(5));
	sprintf(debugfs_buf, "dma_rdrlr5      :%#x\n", dma_rdrlr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdrlr5_fops = {
	.read = dma_rdrlr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdrlr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdrlr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(4));
	sprintf(debugfs_buf, "dma_rdrlr4      :%#x\n", dma_rdrlr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdrlr4_fops = {
	.read = dma_rdrlr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdrlr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdrlr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(3));
	sprintf(debugfs_buf, "dma_rdrlr3      :%#x\n", dma_rdrlr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdrlr3_fops = {
	.read = dma_rdrlr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdrlr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdrlr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(2));
	sprintf(debugfs_buf, "dma_rdrlr2      :%#x\n", dma_rdrlr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdrlr2_fops = {
	.read = dma_rdrlr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdrlr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdrlr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(1));
	sprintf(debugfs_buf, "dma_rdrlr1      :%#x\n", dma_rdrlr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdrlr1_fops = {
	.read = dma_rdrlr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdrlr0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdrlr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_RING_LEN(0));
	sprintf(debugfs_buf, "dma_rdrlr0      :%#x\n", dma_rdrlr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdrlr0_fops = {
	.read = dma_rdrlr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdrlr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdrlr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(7));
	sprintf(debugfs_buf, "dma_tdrlr7      :%#x\n", dma_tdrlr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdrlr7_fops = {
	.read = dma_tdrlr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdrlr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdrlr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(6));
	sprintf(debugfs_buf, "dma_tdrlr6      :%#x\n", dma_tdrlr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdrlr6_fops = {
	.read = dma_tdrlr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdrlr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdrlr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(5));
	sprintf(debugfs_buf, "dma_tdrlr5      :%#x\n", dma_tdrlr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdrlr5_fops = {
	.read = dma_tdrlr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdrlr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdrlr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(4));
	sprintf(debugfs_buf, "dma_tdrlr4      :%#x\n", dma_tdrlr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdrlr4_fops = {
	.read = dma_tdrlr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdrlr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdrlr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(3));
	sprintf(debugfs_buf, "dma_tdrlr3      :%#x\n", dma_tdrlr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdrlr3_fops = {
	.read = dma_tdrlr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdrlr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdrlr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(2));
	sprintf(debugfs_buf, "dma_tdrlr2      :%#x\n", dma_tdrlr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdrlr2_fops = {
	.read = dma_tdrlr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdrlr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdrlr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(1));
	sprintf(debugfs_buf, "dma_tdrlr1      :%#x\n", dma_tdrlr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdrlr1_fops = {
	.read = dma_tdrlr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdrlr0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdrlr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_RING_LEN(0));
	sprintf(debugfs_buf, "dma_tdrlr0      :%#x\n", dma_tdrlr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdrlr0_fops = {
	.read = dma_tdrlr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdtp_rpdr7_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdtp_rpdr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(7));
	sprintf(debugfs_buf, "dma_rdtp_rpdr7  :%#x\n", dma_rdtp_rpdr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdtp_rpdr7_fops = {
	.read = dma_rdtp_rpdr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdtp_rpdr6_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdtp_rpdr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(6));
	sprintf(debugfs_buf, "dma_rdtp_rpdr6  :%#x\n", dma_rdtp_rpdr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdtp_rpdr6_fops = {
	.read = dma_rdtp_rpdr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdtp_rpdr5_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdtp_rpdr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(5));
	sprintf(debugfs_buf, "dma_rdtp_rpdr5  :%#x\n", dma_rdtp_rpdr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdtp_rpdr5_fops = {
	.read = dma_rdtp_rpdr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdtp_rpdr4_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdtp_rpdr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(4));
	sprintf(debugfs_buf, "dma_rdtp_rpdr4  :%#x\n", dma_rdtp_rpdr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdtp_rpdr4_fops = {
	.read = dma_rdtp_rpdr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdtp_rpdr3_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdtp_rpdr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(3));
	sprintf(debugfs_buf, "dma_rdtp_rpdr3  :%#x\n", dma_rdtp_rpdr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdtp_rpdr3_fops = {
	.read = dma_rdtp_rpdr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdtp_rpdr2_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdtp_rpdr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(2));
	sprintf(debugfs_buf, "dma_rdtp_rpdr2  :%#x\n", dma_rdtp_rpdr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdtp_rpdr2_fops = {
	.read = dma_rdtp_rpdr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdtp_rpdr1_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdtp_rpdr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(1));
	sprintf(debugfs_buf, "dma_rdtp_rpdr1  :%#x\n", dma_rdtp_rpdr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdtp_rpdr1_fops = {
	.read = dma_rdtp_rpdr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdtp_rpdr0_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdtp_rpdr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(0));
	sprintf(debugfs_buf, "dma_rdtp_rpdr0  :%#x\n", dma_rdtp_rpdr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdtp_rpdr0_fops = {
	.read = dma_rdtp_rpdr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdtp_tpdr7_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdtp_tpdr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(7));
	sprintf(debugfs_buf, "dma_tdtp_tpdr7  :%#x\n", dma_tdtp_tpdr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdtp_tpdr7_fops = {
	.read = dma_tdtp_tpdr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdtp_tpdr6_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdtp_tpdr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(6));
	sprintf(debugfs_buf, "dma_tdtp_tpdr6  :%#x\n", dma_tdtp_tpdr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdtp_tpdr6_fops = {
	.read = dma_tdtp_tpdr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdtp_tpdr5_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdtp_tpdr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(5));
	sprintf(debugfs_buf, "dma_tdtp_tpdr5  :%#x\n", dma_tdtp_tpdr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdtp_tpdr5_fops = {
	.read = dma_tdtp_tpdr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdtp_tpdr4_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdtp_tpdr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(4));
	sprintf(debugfs_buf, "dma_tdtp_tpdr4  :%#x\n", dma_tdtp_tpdr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdtp_tpdr4_fops = {
	.read = dma_tdtp_tpdr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdtp_tpdr3_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdtp_tpdr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(3));
	sprintf(debugfs_buf, "dma_tdtp_tpdr3  :%#x\n", dma_tdtp_tpdr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdtp_tpdr3_fops = {
	.read = dma_tdtp_tpdr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdtp_tpdr2_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdtp_tpdr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(2));
	sprintf(debugfs_buf, "dma_tdtp_tpdr2  :%#x\n", dma_tdtp_tpdr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdtp_tpdr2_fops = {
	.read = dma_tdtp_tpdr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdtp_tpdr1_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdtp_tpdr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(1));
	sprintf(debugfs_buf, "dma_tdtp_tpdr1  :%#x\n", dma_tdtp_tpdr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdtp_tpdr1_fops = {
	.read = dma_tdtp_tpdr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdtp_tpdr0_read(struct file *file, char __user * userbuf,
				   size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdtp_tpdr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_TAIL_PTR(0));
	sprintf(debugfs_buf, "dma_tdtp_tpdr0  :%#x\n", dma_tdtp_tpdr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdtp_tpdr0_fops = {
	.read = dma_tdtp_tpdr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdlar7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdlar7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(7));
	sprintf(debugfs_buf, "dma_rdlar7      :%#x\n", dma_rdlar7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdlar7_fops = {
	.read = dma_rdlar7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdlar6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdlar6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(6));
	sprintf(debugfs_buf, "dma_rdlar6      :%#x\n", dma_rdlar6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdlar6_fops = {
	.read = dma_rdlar6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdlar5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdlar5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(5));
	sprintf(debugfs_buf, "dma_rdlar5      :%#x\n", dma_rdlar5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdlar5_fops = {
	.read = dma_rdlar5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdlar4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdlar4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(4));
	sprintf(debugfs_buf, "dma_rdlar4      :%#x\n", dma_rdlar4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdlar4_fops = {
	.read = dma_rdlar4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdlar3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdlar3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(3));
	sprintf(debugfs_buf, "dma_rdlar3      :%#x\n", dma_rdlar3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdlar3_fops = {
	.read = dma_rdlar3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdlar2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdlar2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(2));
	sprintf(debugfs_buf, "dma_rdlar2      :%#x\n", dma_rdlar2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdlar2_fops = {
	.read = dma_rdlar2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdlar1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdlar1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(1));
	sprintf(debugfs_buf, "dma_rdlar1      :%#x\n", dma_rdlar1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdlar1_fops = {
	.read = dma_rdlar1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rdlar0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rdlar0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(0));
	sprintf(debugfs_buf, "dma_rdlar0      :%#x\n", dma_rdlar0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rdlar0_fops = {
	.read = dma_rdlar0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdlar7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdlar7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(7));
	sprintf(debugfs_buf, "dma_tdlar7      :%#x\n", dma_tdlar7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdlar7_fops = {
	.read = dma_tdlar7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdlar6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdlar6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(6));
	sprintf(debugfs_buf, "dma_tdlar6      :%#x\n", dma_tdlar6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdlar6_fops = {
	.read = dma_tdlar6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdlar5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdlar5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(5));
	sprintf(debugfs_buf, "dma_tdlar5      :%#x\n", dma_tdlar5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdlar5_fops = {
	.read = dma_tdlar5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdlar4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdlar4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(4));
	sprintf(debugfs_buf, "dma_tdlar4      :%#x\n", dma_tdlar4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdlar4_fops = {
	.read = dma_tdlar4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdlar3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdlar3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(3));
	sprintf(debugfs_buf, "dma_tdlar3      :%#x\n", dma_tdlar3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdlar3_fops = {
	.read = dma_tdlar3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdlar2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdlar2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(2));
	sprintf(debugfs_buf, "dma_tdlar2      :%#x\n", dma_tdlar2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdlar2_fops = {
	.read = dma_tdlar2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdlar1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdlar1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(1));
	sprintf(debugfs_buf, "dma_tdlar1      :%#x\n", dma_tdlar1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdlar1_fops = {
	.read = dma_tdlar1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tdlar0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tdlar0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TXDESC_LIST_ADDR(0));
	sprintf(debugfs_buf, "dma_tdlar0      :%#x\n", dma_tdlar0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tdlar0_fops = {
	.read = dma_tdlar0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_ier7_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_ier7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(7));
	sprintf(debugfs_buf, "dma_ier7        :%#x\n", dma_ier7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_ier7_fops = {
	.read = dma_ier7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_ier6_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_ier6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(6));
	sprintf(debugfs_buf, "dma_ier6        :%#x\n", dma_ier6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_ier6_fops = {
	.read = dma_ier6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_ier5_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_ier5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(5));
	sprintf(debugfs_buf, "dma_ier5        :%#x\n", dma_ier5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_ier5_fops = {
	.read = dma_ier5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_ier4_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_ier4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(4));
	sprintf(debugfs_buf, "dma_ier4        :%#x\n", dma_ier4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_ier4_fops = {
	.read = dma_ier4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_ier3_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_ier3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(3));
	sprintf(debugfs_buf, "dma_ier3        :%#x\n", dma_ier3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_ier3_fops = {
	.read = dma_ier3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_ier2_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_ier2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(2));
	sprintf(debugfs_buf, "dma_ier2        :%#x\n", dma_ier2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_ier2_fops = {
	.read = dma_ier2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_ier1_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_ier1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(1));
	sprintf(debugfs_buf, "dma_ier1        :%#x\n", dma_ier1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_ier1_fops = {
	.read = dma_ier1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_ier0_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_ier0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_INTR_EN(0));
	sprintf(debugfs_buf, "dma_ier0        :%#x\n", dma_ier0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_ier0_fops = {
	.read = dma_ier0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_imr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_imr_val = dwceqos_readl(pdata, DWCEQOS_MAC_INTR_EN);
	sprintf(debugfs_buf, "mac_imr         :%#x\n", mac_imr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_imr_fops = {
	.read = mac_imr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_isr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_isr_val = dwceqos_readl(pdata, DWCEQOS_MAC_INTR_STAT);
	sprintf(debugfs_buf, "mac_isr         :%#x\n", mac_isr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_isr_fops = {
	.read = mac_isr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_isr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_isr_val = dwceqos_readl(pdata, DWCEQOS_MTL_INTR_STAT);
	sprintf(debugfs_buf, "mtl_isr         :%#x\n", mtl_isr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_isr_fops = {
	.read = mtl_isr_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sr7_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(7));
	sprintf(debugfs_buf, "dma_sr7         :%#x\n", dma_sr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sr7_fops = {
	.read = dma_sr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sr6_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(6));
	sprintf(debugfs_buf, "dma_sr6         :%#x\n", dma_sr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sr6_fops = {
	.read = dma_sr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sr5_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(5));
	sprintf(debugfs_buf, "dma_sr5         :%#x\n", dma_sr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sr5_fops = {
	.read = dma_sr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sr4_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(4));
	sprintf(debugfs_buf, "dma_sr4         :%#x\n", dma_sr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sr4_fops = {
	.read = dma_sr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sr3_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(3));
	sprintf(debugfs_buf, "dma_sr3         :%#x\n", dma_sr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sr3_fops = {
	.read = dma_sr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sr2_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(2));
	sprintf(debugfs_buf, "dma_sr2         :%#x\n", dma_sr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sr2_fops = {
	.read = dma_sr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sr1_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(1));
	sprintf(debugfs_buf, "dma_sr1         :%#x\n", dma_sr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sr1_fops = {
	.read = dma_sr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sr0_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_STAT(0));
	sprintf(debugfs_buf, "dma_sr0         :%#x\n", dma_sr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sr0_fops = {
	.read = dma_sr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_isr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_isr_val = dwceqos_readl(pdata, DWCEQOS_DMA_INTR_STAT);
	sprintf(debugfs_buf, "dma_isr         :%#x\n", dma_isr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_isr_fops = {
	.read = dma_isr_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_dsr2_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_dsr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_DBG_STAT2);
	sprintf(debugfs_buf, "dma_dsr2                   :%#x\n", dma_dsr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_dsr2_fops = {
	.read = dma_dsr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_dsr1_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_dsr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_DBG_STAT1);
	sprintf(debugfs_buf, "dma_dsr1                   :%#x\n", dma_dsr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_dsr1_fops = {
	.read = dma_dsr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_dsr0_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_dsr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_DBG_STAT0);
	sprintf(debugfs_buf, "dma_dsr0                   :%#x\n", dma_dsr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_dsr0_fops = {
	.read = dma_dsr0_read,
	.write = dwc_eqos_write,
};

static ssize_t MTL_q0rdr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0rdr_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DBG(0));
	sprintf(debugfs_buf, "mtl_q0rdr       :%#x\n", mtl_q0rdr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0rdr_fops = {
	.read = MTL_q0rdr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0esr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0esr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_STAT(0));
	sprintf(debugfs_buf, "mtl_q0esr       :%#x\n", mtl_q0esr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0esr_fops = {
	.read = mtl_q0esr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0tdr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0tdr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_DBG(0));
	sprintf(debugfs_buf, "mtl_q0tdr       :%#x\n", mtl_q0tdr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0tdr_fops = {
	.read = mtl_q0tdr_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrbar7_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrbar7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(7));
	sprintf(debugfs_buf, "dma_chrbar7     :%#x\n", dma_chrbar7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrbar7_fops = {
	.read = dma_chrbar7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrbar6_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrbar6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(6));
	sprintf(debugfs_buf, "dma_chrbar6     :%#x\n", dma_chrbar6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrbar6_fops = {
	.read = dma_chrbar6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrbar5_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrbar5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(5));
	sprintf(debugfs_buf, "dma_chrbar5     :%#x\n", dma_chrbar5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrbar5_fops = {
	.read = dma_chrbar5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrbar4_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrbar4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(4));
	sprintf(debugfs_buf, "dma_chrbar4     :%#x\n", dma_chrbar4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrbar4_fops = {
	.read = dma_chrbar4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrbar3_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrbar3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(3));
	sprintf(debugfs_buf, "dma_chrbar3     :%#x\n", dma_chrbar3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrbar3_fops = {
	.read = dma_chrbar3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrbar2_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrbar2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(2));
	sprintf(debugfs_buf, "dma_chrbar2     :%#x\n", dma_chrbar2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrbar2_fops = {
	.read = dma_chrbar2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrbar1_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrbar1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(1));
	sprintf(debugfs_buf, "dma_chrbar1     :%#x\n", dma_chrbar1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrbar1_fops = {
	.read = dma_chrbar1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrbar0_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrbar0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXBUF(0));
	sprintf(debugfs_buf, "dma_chrbar0     :%#x\n", dma_chrbar0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrbar0_fops = {
	.read = dma_chrbar0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtbar7_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtbar7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(7));
	sprintf(debugfs_buf, "dma_chtbar7     :%#x\n", dma_chtbar7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtbar7_fops = {
	.read = dma_chtbar7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtbar6_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtbar6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(6));
	sprintf(debugfs_buf, "dma_chtbar6     :%#x\n", dma_chtbar6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtbar6_fops = {
	.read = dma_chtbar6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtbar5_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtbar5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(5));
	sprintf(debugfs_buf, "dma_chtbar5     :%#x\n", dma_chtbar5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtbar5_fops = {
	.read = dma_chtbar5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtbar4_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtbar4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(4));
	sprintf(debugfs_buf, "dma_chtbar4     :%#x\n", dma_chtbar4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtbar4_fops = {
	.read = dma_chtbar4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtbar3_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtbar3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(3));
	sprintf(debugfs_buf, "dma_chtbar3     :%#x\n", dma_chtbar3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtbar3_fops = {
	.read = dma_chtbar3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtbar2_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtbar2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(2));
	sprintf(debugfs_buf, "dma_chtbar2     :%#x\n", dma_chtbar2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtbar2_fops = {
	.read = dma_chtbar2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtbar1_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtbar1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(1));
	sprintf(debugfs_buf, "dma_chtbar1     :%#x\n", dma_chtbar1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtbar1_fops = {
	.read = dma_chtbar1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtbar0_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtbar0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXBUF(0));
	sprintf(debugfs_buf, "dma_chtbar0     :%#x\n", dma_chtbar0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtbar0_fops = {
	.read = dma_chtbar0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrdr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrdr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(7));
	sprintf(debugfs_buf, "dma_chrdr7      :%#x\n", dma_chrdr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrdr7_fops = {
	.read = dma_chrdr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrdr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrdr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(6));
	sprintf(debugfs_buf, "dma_chrdr6      :%#x\n", dma_chrdr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrdr6_fops = {
	.read = dma_chrdr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrdr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrdr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(5));
	sprintf(debugfs_buf, "dma_chrdr5      :%#x\n", dma_chrdr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrdr5_fops = {
	.read = dma_chrdr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrdr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrdr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(4));
	sprintf(debugfs_buf, "dma_chrdr4      :%#x\n", dma_chrdr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrdr4_fops = {
	.read = dma_chrdr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrdr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrdr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(3));
	sprintf(debugfs_buf, "dma_chrdr3      :%#x\n", dma_chrdr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrdr3_fops = {
	.read = dma_chrdr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrdr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrdr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(2));
	sprintf(debugfs_buf, "dma_chrdr2      :%#x\n", dma_chrdr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrdr2_fops = {
	.read = dma_chrdr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrdr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrdr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(1));
	sprintf(debugfs_buf, "dma_chrdr1      :%#x\n", dma_chrdr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrdr1_fops = {
	.read = dma_chrdr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chrdr0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chrdr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(0));
	sprintf(debugfs_buf, "dma_chrdr0      :%#x\n", dma_chrdr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chrdr0_fops = {
	.read = dma_chrdr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtdr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtdr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(7));
	sprintf(debugfs_buf, "dma_chtdr7      :%#x\n", dma_chtdr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtdr7_fops = {
	.read = dma_chtdr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtdr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtdr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(6));
	sprintf(debugfs_buf, "dma_chtdr6      :%#x\n", dma_chtdr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtdr6_fops = {
	.read = dma_chtdr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtdr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtdr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(5));
	sprintf(debugfs_buf, "dma_chtdr5      :%#x\n", dma_chtdr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtdr5_fops = {
	.read = dma_chtdr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtdr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtdr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(4));
	sprintf(debugfs_buf, "dma_chtdr4      :%#x\n", dma_chtdr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtdr4_fops = {
	.read = dma_chtdr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtdr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtdr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(3));
	sprintf(debugfs_buf, "dma_chtdr3      :%#x\n", dma_chtdr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtdr3_fops = {
	.read = dma_chtdr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtdr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtdr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(2));
	sprintf(debugfs_buf, "dma_chtdr2      :%#x\n", dma_chtdr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtdr2_fops = {
	.read = dma_chtdr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtdr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtdr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(1));
	sprintf(debugfs_buf, "dma_chtdr1      :%#x\n", dma_chtdr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtdr1_fops = {
	.read = dma_chtdr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_chtdr0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_chtdr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_TXDESC(0));
	sprintf(debugfs_buf, "dma_chtdr0      :%#x\n", dma_chtdr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_chtdr0_fops = {
	.read = dma_chtdr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sfcsr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sfcsr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(7));
	sprintf(debugfs_buf, "dma_sfcsr7      :%#x\n", dma_sfcsr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sfcsr7_fops = {
	.read = dma_sfcsr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sfcsr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sfcsr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(6));
	sprintf(debugfs_buf, "dma_sfcsr6      :%#x\n", dma_sfcsr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sfcsr6_fops = {
	.read = dma_sfcsr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sfcsr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sfcsr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(5));
	sprintf(debugfs_buf, "dma_sfcsr5      :%#x\n", dma_sfcsr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sfcsr5_fops = {
	.read = dma_sfcsr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sfcsr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sfcsr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(4));
	sprintf(debugfs_buf, "dma_sfcsr4      :%#x\n", dma_sfcsr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sfcsr4_fops = {
	.read = dma_sfcsr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sfcsr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sfcsr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(3));
	sprintf(debugfs_buf, "dma_sfcsr3      :%#x\n", dma_sfcsr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sfcsr3_fops = {
	.read = dma_sfcsr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sfcsr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sfcsr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(2));
	sprintf(debugfs_buf, "dma_sfcsr2      :%#x\n", dma_sfcsr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sfcsr2_fops = {
	.read = dma_sfcsr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sfcsr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sfcsr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(1));
	sprintf(debugfs_buf, "dma_sfcsr1      :%#x\n", dma_sfcsr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sfcsr1_fops = {
	.read = dma_sfcsr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sfcsr0_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sfcsr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_SLOT_FUNC_CS(0));
	sprintf(debugfs_buf, "dma_sfcsr0      :%#x\n", dma_sfcsr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sfcsr0_fops = {
	.read = dma_sfcsr0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_ivlantirr_read(struct file *file, char __user * userbuf,
				  size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_ivlantirr_val = dwceqos_readl(pdata, DWCEQOS_MAC_INNER_VLAN_INCL);
	sprintf(debugfs_buf, "mac_ivlantirr              :%#x\n",
		mac_ivlantirr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ivlantirr_fops = {
	.read = mac_ivlantirr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_vlantirr_read(struct file *file, char __user * userbuf,
				 size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_vlantirr_val = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_INCL);
	sprintf(debugfs_buf, "mac_vlantirr               :%#x\n",
		mac_vlantirr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_vlantirr_fops = {
	.read = mac_vlantirr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_vlanhtr_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_vlanhtr_val = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_HT);
	sprintf(debugfs_buf, "mac_vlanhtr                :%#x\n",
		mac_vlanhtr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_vlanhtr_fops = {
	.read = mac_vlanhtr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_vlantr_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_vlantr_val = dwceqos_readl(pdata, DWCEQOS_MAC_VLAN_TAG_CTRL);
	sprintf(debugfs_buf, "mac_vlantr                 :%#x\n",
		mac_vlantr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_vlantr_fops = {
	.read = mac_vlantr_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_sbus_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_sbus_val = dwceqos_readl(pdata, DWCEQOS_DMA_SYSBUS_MODE);
	sprintf(debugfs_buf, "dma_sbus                   :%#x\n", dma_sbus_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sbus_fops = {
	.read = dma_sbus_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_bmr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_bmr_val = dwceqos_readl(pdata, DWCEQOS_DMA_MODE);
	sprintf(debugfs_buf, "dma_bmr                    :%#x\n", dma_bmr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_bmr_fops = {
	.read = dma_bmr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0rcr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0rcr_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_CTRL(0));
	sprintf(debugfs_buf, "mtl_q0rcr       :%#x\n", mtl_q0rcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0rcr_fops = {
	.read = mtl_q0rcr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0ocr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0ocr_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_MPOC(0));
	sprintf(debugfs_buf, "mtl_q0ocr       :%#x\n", mtl_q0ocr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0ocr_fops = {
	.read = mtl_q0ocr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0romr_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0romr_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_OP_MODE(0));
	sprintf(debugfs_buf, "mtl_q0romr      :%#x\n", mtl_q0romr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0romr_fops = {
	.read = mtl_q0romr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0qr_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0qr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_QW(0));
	sprintf(debugfs_buf, "mtl_q0qr        :%#x\n", mtl_q0qr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0qr_fops = {
	.read = mtl_q0qr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0ecr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0ecr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_ETS_CTRL(0));
	sprintf(debugfs_buf, "mtl_q0ecr       :%#x\n", mtl_q0ecr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0ecr_fops = {
	.read = mtl_q0ecr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0ucr_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0ucr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_UFLOW(0));
	sprintf(debugfs_buf, "mtl_q0ucr       :%#x\n", mtl_q0ucr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0ucr_fops = {
	.read = mtl_q0ucr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_q0tomr_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_q0tomr_val = dwceqos_readl(pdata, DWCEQOS_MTL_TQ_OP_MODE(0));
	sprintf(debugfs_buf, "mtl_q0tomr      :%#x\n", mtl_q0tomr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_q0tomr_fops = {
	.read = mtl_q0tomr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_rqdcm1r_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_rqdcm1r_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DMA_MAP1);
	sprintf(debugfs_buf, "mtl_rqdcm1r     :%#x\n", mtl_rqdcm1r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_rqdcm1r_fops = {
	.read = mtl_rqdcm1r_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_rqdcm0r_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_rqdcm0r_val = dwceqos_readl(pdata, DWCEQOS_MTL_RQ_DMA_MAP0);
	sprintf(debugfs_buf, "mtl_rqdcm0r     :%#x\n", mtl_rqdcm0r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_rqdcm0r_fops = {
	.read = mtl_rqdcm0r_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_fddr_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_fddr_val = dwceqos_readl(pdata, DWCEQOS_MTL_FIFO_DBG_DATA);
	sprintf(debugfs_buf, "mtl_fddr        :%#x\n", mtl_fddr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_fddr_fops = {
	.read = mtl_fddr_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_fdacs_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_fdacs_val = dwceqos_readl(pdata, DWCEQOS_MTL_DBG_CTL);
	sprintf(debugfs_buf, "mtl_fdacs       :%#x\n", mtl_fdacs_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_fdacs_fops = {
	.read = mtl_fdacs_read,
	.write = dwc_eqos_write,
};

static ssize_t mtl_omr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mtl_omr_val = dwceqos_readl(pdata, DWCEQOS_MTL_OP_MODE);
	sprintf(debugfs_buf, "mtl_omr         :%#x\n", mtl_omr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_omr_fops = {
	.read = mtl_omr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_rqc3r_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_rqc3r_val = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL3);
	sprintf(debugfs_buf, "mac_rqc3r       :%#x\n", mac_rqc3r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rqc3r_fops = {
	.read = mac_rqc3r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_rqc2r_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_rqc2r_val = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL2);
	sprintf(debugfs_buf, "mac_rqc2r       :%#x\n", mac_rqc2r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rqc2r_fops = {
	.read = mac_rqc2r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_rqc1r_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_rqc1r_val = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL1);
	sprintf(debugfs_buf, "mac_rqc1r       :%#x\n", mac_rqc1r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rqc1r_fops = {
	.read = mac_rqc1r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_rqc0r_read(struct file *file, char __user * userbuf,
			      size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_rqc1r_val = dwceqos_readl(pdata, DWCEQOS_MAC_RQ_CTRL1);
	sprintf(debugfs_buf, "mac_rqc0r       :%#x\n", mac_rqc0r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rqc0r_fops = {
	.read = mac_rqc0r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_tqpm1r_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_tqpm1r_val = dwceqos_readl(pdata, DWCEQOS_MAC_TQPM1);
	sprintf(debugfs_buf, "mac_tqpm1r      :%#x\n", mac_tqpm1r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tqpm1r_fops = {
	.read = mac_tqpm1r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_tqpm0r_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_tqpm0r_val = dwceqos_readl(pdata, DWCEQOS_MAC_TQPM0);
	sprintf(debugfs_buf, "mac_tqpm0r      :%#x\n", mac_tqpm0r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tqpm0r_fops = {
	.read = mac_tqpm0r_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_rfcr_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_rfcr_val = dwceqos_readl(pdata, DWCEQOS_MAC_RX_FLOW_CTRL);
	sprintf(debugfs_buf, "mac_rfcr        :%#x\n", mac_rfcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rfcr_fops = {
	.read = mac_rfcr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_qtfcr7_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_qtfcr7_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(7));
	sprintf(debugfs_buf, "mac_qtfcr7      :%#x\n", mac_qtfcr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_qtfcr7_fops = {
	.read = mac_qtfcr7_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_qtfcr6_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_qtfcr6_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(6));
	sprintf(debugfs_buf, "mac_qtfcr6      :%#x\n", mac_qtfcr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_qtfcr6_fops = {
	.read = mac_qtfcr6_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_qtfcr5_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_qtfcr5_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(5));
	sprintf(debugfs_buf, "mac_qtfcr5      :%#x\n", mac_qtfcr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_qtfcr5_fops = {
	.read = mac_qtfcr5_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_qtfcr4_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_qtfcr4_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(4));
	sprintf(debugfs_buf, "mac_qtfcr4      :%#x\n", mac_qtfcr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_qtfcr4_fops = {
	.read = mac_qtfcr4_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_qtfcr3_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_qtfcr3_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(3));
	sprintf(debugfs_buf, "mac_qtfcr3      :%#x\n", mac_qtfcr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_qtfcr3_fops = {
	.read = mac_qtfcr3_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_qtfcr2_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_qtfcr2_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(2));
	sprintf(debugfs_buf, "mac_qtfcr2      :%#x\n", mac_qtfcr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_qtfcr2_fops = {
	.read = mac_qtfcr2_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_qtfcr1_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_qtfcr1_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(1));
	sprintf(debugfs_buf, "mac_qtfcr1      :%#x\n", mac_qtfcr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_qtfcr1_fops = {
	.read = mac_qtfcr1_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_q0tfcr_read(struct file *file, char __user * userbuf,
			       size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_q0tfcr_val = dwceqos_readl(pdata, DWCEQOS_MAC_Q_TX_FLOW_CTRL(0));
	sprintf(debugfs_buf, "mac_q0tfcr      :%#x\n", mac_q0tfcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_q0tfcr_fops = {
	.read = mac_q0tfcr_read,
	.write = dwc_eqos_write,
};
#if 0
static ssize_t dma_axi4cr7_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	DMA_AXI4CR7_RgRd(dma_axi4cr7_val);
	sprintf(debugfs_buf, "dma_axi4cr7     :%#x\n", dma_axi4cr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_axi4cr7_fops = {
	.read = dma_axi4cr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_axi4cr6_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	DMA_AXI4CR6_RgRd(dma_axi4cr6_val);
	sprintf(debugfs_buf, "dma_axi4cr6     :%#x\n", dma_axi4cr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_axi4cr6_fops = {
	.read = dma_axi4cr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_axi4cr5_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	DMA_AXI4CR5_RgRd(dma_axi4cr5_val);
	sprintf(debugfs_buf, "dma_axi4cr5     :%#x\n", dma_axi4cr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_axi4cr5_fops = {
	.read = dma_axi4cr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_axi4cr4_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	DMA_AXI4CR4_RgRd(dma_axi4cr4_val);
	sprintf(debugfs_buf, "dma_axi4cr4     :%#x\n", dma_axi4cr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_axi4cr4_fops = {
	.read = dma_axi4cr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_axi4cr3_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	DMA_AXI4CR3_RgRd(dma_axi4cr3_val);
	sprintf(debugfs_buf, "dma_axi4cr3     :%#x\n", dma_axi4cr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_axi4cr3_fops = {
	.read = dma_axi4cr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_axi4cr2_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	DMA_AXI4CR2_RgRd(dma_axi4cr2_val);
	sprintf(debugfs_buf, "dma_axi4cr2     :%#x\n", dma_axi4cr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_axi4cr2_fops = {
	.read = dma_axi4cr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_axi4cr1_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	DMA_AXI4CR1_RgRd(dma_axi4cr1_val);
	sprintf(debugfs_buf, "dma_axi4cr1     :%#x\n", dma_axi4cr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_axi4cr1_fops = {
	.read = dma_axi4cr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_axi4cr0_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	DMA_AXI4CR0_RgRd(dma_axi4cr0_val);
	sprintf(debugfs_buf, "dma_axi4cr0     :%#x\n", dma_axi4cr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_axi4cr0_fops = {
	.read = dma_axi4cr0_read,
	.write = dwc_eqos_write,
};
#endif
static ssize_t dma_rcr7_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rcr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(7));
	sprintf(debugfs_buf, "dma_rcr7        :%#x\n", dma_rcr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rcr7_fops = {
	.read = dma_rcr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rcr6_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rcr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(6));
	sprintf(debugfs_buf, "dma_rcr6        :%#x\n", dma_rcr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rcr6_fops = {
	.read = dma_rcr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rcr5_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rcr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(5));
	sprintf(debugfs_buf, "dma_rcr5        :%#x\n", dma_rcr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rcr5_fops = {
	.read = dma_rcr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rcr4_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rcr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(4));
	sprintf(debugfs_buf, "dma_rcr4        :%#x\n", dma_rcr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rcr4_fops = {
	.read = dma_rcr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rcr3_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rcr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(3));
	sprintf(debugfs_buf, "dma_rcr3        :%#x\n", dma_rcr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rcr3_fops = {
	.read = dma_rcr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rcr2_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rcr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(2));
	sprintf(debugfs_buf, "dma_rcr2        :%#x\n", dma_rcr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rcr2_fops = {
	.read = dma_rcr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rcr1_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rcr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(1));
	sprintf(debugfs_buf, "dma_rcr1        :%#x\n", dma_rcr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rcr1_fops = {
	.read = dma_rcr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_rcr0_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_rcr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RX_CTRL(0));
	sprintf(debugfs_buf, "dma_rcr0        :%#x\n", dma_rcr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_rcr0_fops = {
	.read = dma_rcr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tcr7_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tcr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(7));
	sprintf(debugfs_buf, "dma_tcr7        :%#x\n", dma_tcr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tcr7_fops = {
	.read = dma_tcr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tcr6_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tcr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(6));
	sprintf(debugfs_buf, "dma_tcr6        :%#x\n", dma_tcr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tcr6_fops = {
	.read = dma_tcr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tcr5_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tcr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(5));
	sprintf(debugfs_buf, "dma_tcr5        :%#x\n", dma_tcr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tcr5_fops = {
	.read = dma_tcr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tcr4_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tcr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(4));
	sprintf(debugfs_buf, "dma_tcr4        :%#x\n", dma_tcr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tcr4_fops = {
	.read = dma_tcr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tcr3_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tcr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(3));
	sprintf(debugfs_buf, "dma_tcr3        :%#x\n", dma_tcr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tcr3_fops = {
	.read = dma_tcr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tcr2_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tcr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(2));
	sprintf(debugfs_buf, "dma_tcr2        :%#x\n", dma_tcr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tcr2_fops = {
	.read = dma_tcr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tcr1_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tcr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(1));
	sprintf(debugfs_buf, "dma_tcr1        :%#x\n", dma_tcr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tcr1_fops = {
	.read = dma_tcr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_tcr0_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_tcr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_TX_CTRL(0));
	sprintf(debugfs_buf, "dma_tcr0        :%#x\n", dma_tcr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_tcr0_fops = {
	.read = dma_tcr0_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_cr7_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_cr7_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(7));
	sprintf(debugfs_buf, "dma_cr7         :%#x\n", dma_cr7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_cr7_fops = {
	.read = dma_cr7_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_cr6_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_cr6_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(6));
	sprintf(debugfs_buf, "dma_cr6         :%#x\n", dma_cr6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_cr6_fops = {
	.read = dma_cr6_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_cr5_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_cr5_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(5));
	sprintf(debugfs_buf, "dma_cr5         :%#x\n", dma_cr5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_cr5_fops = {
	.read = dma_cr5_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_cr4_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_cr4_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(4));
	sprintf(debugfs_buf, "dma_cr4         :%#x\n", dma_cr4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_cr4_fops = {
	.read = dma_cr4_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_cr3_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_cr3_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(3));
	sprintf(debugfs_buf, "dma_cr3         :%#x\n", dma_cr3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_cr3_fops = {
	.read = dma_cr3_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_cr2_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_cr2_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(2));
	sprintf(debugfs_buf, "dma_cr2         :%#x\n", dma_cr2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_cr2_fops = {
	.read = dma_cr2_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_cr1_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_cr1_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(1));
	sprintf(debugfs_buf, "dma_cr1         :%#x\n", dma_cr1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_cr1_fops = {
	.read = dma_cr1_read,
	.write = dwc_eqos_write,
};

static ssize_t dma_cr0_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dma_cr0_val = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CTRL(0));
	sprintf(debugfs_buf, "dma_cr0         :%#x\n", dma_cr0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_cr0_fops = {
	.read = dma_cr0_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_wtr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_wtr_val = dwceqos_readl(pdata, DWCEQOS_MAC_WD_TIMEOUT);
	sprintf(debugfs_buf, "mac_wtr         :%#x\n", mac_wtr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_wtr_fops = {
	.read = mac_wtr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_mpfr_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_mpfr_val = dwceqos_readl(pdata, DWCEQOS_MAC_PKT_FILT);
	sprintf(debugfs_buf, "mac_mpfr        :%#x\n", mac_mpfr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_mpfr_fops = {
	.read = mac_mpfr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_mecr_read(struct file *file, char __user * userbuf,
			     size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_mecr_val = dwceqos_readl(pdata, DWCEQOS_MAC_EXT_CFG);
	sprintf(debugfs_buf, "mac_mecr        :%#x\n", mac_mecr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_mecr_fops = {
	.read = mac_mecr_read,
	.write = dwc_eqos_write,
};

static ssize_t mac_mcr_read(struct file *file, char __user * userbuf,
			    size_t count, loff_t * ppos)
{
	ssize_t ret;
	mac_mcr_val = dwceqos_readl(pdata, DWCEQOS_MAC_CFG);
	//MAC_MCR_RgRd(mac_mcr_val);
	sprintf(debugfs_buf, "mac_mcr         :%#x\n", mac_mcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_mcr_fops = {
	.read = mac_mcr_read,
	.write = dwc_eqos_write,
};

/* For MII/GMII registers */
static ssize_t mii_bmcr_reg_read(struct file *file, char __user * userbuf,
				 size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR,
				     &mac_bmcr_reg_val);
	sprintf(debugfs_buf,
		"Phy Control Reg(Basic Mode Control Reg)      :%#x\n",
		mac_bmcr_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_bmcr_reg_fops = {
	.read = mii_bmcr_reg_read,
	.write = dwc_eqos_write,
};

static ssize_t mii_bmsr_reg_read(struct file *file, char __user * userbuf,
				 size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMSR,
				     &mac_bmsr_reg_val);
	sprintf(debugfs_buf,
		"Phy Status Reg(Basic Mode Status Reg)        :%#x\n",
		mac_bmsr_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_bmsr_reg_fops = {
	.read = mii_bmsr_reg_read,
};

static ssize_t mii_physid1_reg_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID1,
				     &mii_physid1_reg_val);
	sprintf(debugfs_buf,
		"Phy Id (PHYS ID 1)                           :%#x\n",
		mii_physid1_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_physid1_reg_fops = {
	.read = mii_physid1_reg_read,
};

static ssize_t mii_physid2_reg_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID2,
				     &mii_physid2_reg_val);
	sprintf(debugfs_buf,
		"Phy Id (PHYS ID 2)                           :%#x\n",
		mii_physid2_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_physid2_reg_fops = {
	.read = mii_physid2_reg_read,
};

static ssize_t mii_advertise_reg_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ADVERTISE,
				     &mii_advertise_reg_val);
	sprintf(debugfs_buf,
		"Auto-nego Adv (Advertisement Control Reg)    :%#x\n",
		mii_advertise_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_advertise_reg_fops = {
	.read = mii_advertise_reg_read,
	.write = dwc_eqos_write,
};

static ssize_t mii_lpa_reg_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_LPA,
				     &mii_lpa_reg_val);
	sprintf(debugfs_buf,
		"Auto-nego Lap (Link Partner Ability Reg)     :%#x\n",
		mii_lpa_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_lpa_reg_fops = {
	.read = mii_lpa_reg_read,
};

static ssize_t mii_expansion_reg_read(struct file *file, char __user * userbuf,
				      size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_EXPANSION,
				     &mii_expansion_reg_val);
	sprintf(debugfs_buf,
		"Auto-nego Exp (Extension Reg)                :%#x\n",
		mii_expansion_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_expansion_reg_fops = {
	.read = mii_expansion_reg_read,
};

static ssize_t auto_nego_np_reg_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr,
				     DWC_EQOS_AUTO_NEGO_NP,
				     &auto_nego_np_reg_val);
	sprintf(debugfs_buf,
		"Auto-nego Np                                 :%#x\n",
		auto_nego_np_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations auto_nego_np_reg_fops = {
	.read = auto_nego_np_reg_read,
	.write = dwc_eqos_write,
};

static ssize_t mii_estatus_reg_read(struct file *file, char __user * userbuf,
				    size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ESTATUS,
				     &mii_estatus_reg_val);
	sprintf(debugfs_buf,
		"Extended Status Reg                          :%#x\n",
		mii_estatus_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_estatus_reg_fops = {
	.read = mii_estatus_reg_read,
};

static ssize_t mii_ctrl1000_reg_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_CTRL1000,
				     &mii_ctrl1000_reg_val);
	sprintf(debugfs_buf,
		"1000 Ctl Reg (1000BASE-T Control Reg)        :%#x\n",
		mii_ctrl1000_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_ctrl1000_reg_fops = {
	.read = mii_ctrl1000_reg_read,
	.write = dwc_eqos_write,
};

static ssize_t mii_stat1000_reg_read(struct file *file, char __user * userbuf,
				     size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_STAT1000,
				     &mii_stat1000_reg_val);
	sprintf(debugfs_buf,
		"1000 Sts Reg (1000BASE-T Status)             :%#x\n",
		mii_stat1000_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_stat1000_reg_fops = {
	.read = mii_stat1000_reg_read,
};

static ssize_t phy_ctl_reg_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, DWC_EQOS_PHY_CTL,
				     &phy_ctl_reg_val);
	sprintf(debugfs_buf,
		"PHY Specific Ctl Reg                         :%#x\n",
		phy_ctl_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations phy_ctl_reg_fops = {
	.read = phy_ctl_reg_read,
	.write = dwc_eqos_write,
};

static ssize_t phy_sts_reg_read(struct file *file, char __user * userbuf,
				size_t count, loff_t * ppos)
{
	ssize_t ret;
	dwc_eqos_mdio_read_direct(pdata, pdata->phyaddr, DWC_EQOS_PHY_STS,
				     &phy_sts_reg_val);
	sprintf(debugfs_buf,
		"PHY Specific Sts Reg                         :%#x\n",
		phy_sts_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations phy_sts_reg_fops = {
	.read = phy_sts_reg_read,
};

static ssize_t feature_drop_tx_pktburstcnt_read(struct file *file,
						char __user * userbuf,
						size_t count, loff_t * ppos)
{
	ssize_t ret;
	sprintf(debugfs_buf, "feature_drop_tx_pktburstcnt             :%#x\n",
		feature_drop_tx_pktburstcnt_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations feature_drop_tx_pktburstcnt_fops = {
	.read = feature_drop_tx_pktburstcnt_read,
	.write = dwc_eqos_write,
};

static ssize_t qinx_read(struct file *file,
			 char __user * userbuf, size_t count, loff_t * ppos)
{
	ssize_t ret;
	sprintf(debugfs_buf, "qInx             :%#x\n", qInx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations qInx_fops = {
	.read = qinx_read,
	.write = dwc_eqos_write,
};

static ssize_t rx_normal_desc_descriptor_read0(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read0\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num)
			, pRX_NORMAL_DESC[desc_num].rdes3,
			pRX_NORMAL_DESC[desc_num].rdes2,
			pRX_NORMAL_DESC[desc_num].rdes1,
			pRX_NORMAL_DESC[desc_num].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read0\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops0 = {
	.read = rx_normal_desc_descriptor_read0,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read1(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read1\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 20)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 20)
			, pRX_NORMAL_DESC[desc_num + 20].rdes3,
			pRX_NORMAL_DESC[desc_num + 20].rdes2,
			pRX_NORMAL_DESC[desc_num + 20].rdes1,
			pRX_NORMAL_DESC[desc_num + 20].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read1\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops1 = {
	.read = rx_normal_desc_descriptor_read1,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read2(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read2\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 40)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 40)
			, pRX_NORMAL_DESC[desc_num + 40].rdes3,
			pRX_NORMAL_DESC[desc_num + 40].rdes2,
			pRX_NORMAL_DESC[desc_num + 40].rdes1,
			pRX_NORMAL_DESC[desc_num + 40].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read2\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops2 = {
	.read = rx_normal_desc_descriptor_read2,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read3(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read3\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 60)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 60)
			, pRX_NORMAL_DESC[desc_num + 60].rdes3,
			pRX_NORMAL_DESC[desc_num + 60].rdes2,
			pRX_NORMAL_DESC[desc_num + 60].rdes1,
			pRX_NORMAL_DESC[desc_num + 60].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read3\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops3 = {
	.read = rx_normal_desc_descriptor_read3,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read4(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read4\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 80)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 80)
			, pRX_NORMAL_DESC[desc_num + 80].rdes3,
			pRX_NORMAL_DESC[desc_num + 80].rdes2,
			pRX_NORMAL_DESC[desc_num + 80].rdes1,
			pRX_NORMAL_DESC[desc_num + 80].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read4\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops4 = {
	.read = rx_normal_desc_descriptor_read4,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read5(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read5\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 100)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 100)
			, pRX_NORMAL_DESC[desc_num + 100].rdes3,
			pRX_NORMAL_DESC[desc_num + 100].rdes2,
			pRX_NORMAL_DESC[desc_num + 100].rdes1,
			pRX_NORMAL_DESC[desc_num + 100].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read5\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops5 = {
	.read = rx_normal_desc_descriptor_read5,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read6(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read6\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 120)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 120)
			, pRX_NORMAL_DESC[desc_num + 120].rdes3,
			pRX_NORMAL_DESC[desc_num + 120].rdes2,
			pRX_NORMAL_DESC[desc_num + 120].rdes1,
			pRX_NORMAL_DESC[desc_num + 120].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read6\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops6 = {
	.read = rx_normal_desc_descriptor_read6,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read7(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read7\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 140)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 140)
			, pRX_NORMAL_DESC[desc_num + 140].rdes3,
			pRX_NORMAL_DESC[desc_num + 140].rdes2,
			pRX_NORMAL_DESC[desc_num + 140].rdes1,
			pRX_NORMAL_DESC[desc_num + 140].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read7\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops7 = {
	.read = rx_normal_desc_descriptor_read7,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read8(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read8\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 160)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 160)
			, pRX_NORMAL_DESC[desc_num + 160].rdes3,
			pRX_NORMAL_DESC[desc_num + 160].rdes2,
			pRX_NORMAL_DESC[desc_num + 160].rdes1,
			pRX_NORMAL_DESC[desc_num + 160].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read8\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops8 = {
	.read = rx_normal_desc_descriptor_read8,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read9(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read9\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 180)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 180)
			, pRX_NORMAL_DESC[desc_num + 180].rdes3,
			pRX_NORMAL_DESC[desc_num + 180].rdes2,
			pRX_NORMAL_DESC[desc_num + 180].rdes1,
			pRX_NORMAL_DESC[desc_num + 180].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read9\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops9 = {
	.read = rx_normal_desc_descriptor_read9,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read10(struct file *file,
						char __user * userbuf,
						size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read10\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 200)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 200)
			, pRX_NORMAL_DESC[desc_num + 200].rdes3,
			pRX_NORMAL_DESC[desc_num + 200].rdes2,
			pRX_NORMAL_DESC[desc_num + 200].rdes1,
			pRX_NORMAL_DESC[desc_num + 200].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read10\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops10 = {
	.read = rx_normal_desc_descriptor_read10,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read11(struct file *file,
						char __user * userbuf,
						size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read11\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 220)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 220)
			, pRX_NORMAL_DESC[desc_num + 220].rdes3,
			pRX_NORMAL_DESC[desc_num + 220].rdes2,
			pRX_NORMAL_DESC[desc_num + 220].rdes1,
			pRX_NORMAL_DESC[desc_num + 220].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read11\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops11 = {
	.read = rx_normal_desc_descriptor_read11,
	.write = descriptor_write,
};

static ssize_t rx_normal_desc_descriptor_read12(struct file *file,
						char __user * userbuf,
						size_t count, loff_t * ppos)
{
	struct s_rx_normal_desc *pRX_NORMAL_DESC = GET_RX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> rx_normal_desc_descriptor_read12\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(9952, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 16; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"RX_NORMAL_DESC.rdes3             :%x\n"
			"RX_NORMAL_DESC.rdes2             :%x\n"
			"RX_NORMAL_DESC.rdes1             :%x\n"
			"RX_NORMAL_DESC.rdes0             :%x\n",
			qInx_val, (int)(desc_num + 240)
			, (unsigned long) GET_RX_DESC_DMA_ADDR(qInx_val, desc_num + 240)
			, pRX_NORMAL_DESC[desc_num + 240].rdes3,
			pRX_NORMAL_DESC[desc_num + 240].rdes2,
			pRX_NORMAL_DESC[desc_num + 240].rdes1,
			pRX_NORMAL_DESC[desc_num + 240].rdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- rx_normal_desc_descriptor_read12\n");
	return ret;
}

static const struct file_operations rx_normal_desc_desc_fops12 = {
	.read = rx_normal_desc_descriptor_read12,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read0(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read0\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num)
			, pTX_NORMAL_DESC[desc_num].tdes3,
			pTX_NORMAL_DESC[desc_num].tdes2,
			pTX_NORMAL_DESC[desc_num].tdes1,
			pTX_NORMAL_DESC[desc_num].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read0\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops0 = {
	.read = tx_normal_desc_descriptor_read0,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read1(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read1\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 20)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 20)
			, pTX_NORMAL_DESC[desc_num + 20].tdes3,
			pTX_NORMAL_DESC[desc_num + 20].tdes2,
			pTX_NORMAL_DESC[desc_num + 20].tdes1,
			pTX_NORMAL_DESC[desc_num + 20].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read1\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops1 = {
	.read = tx_normal_desc_descriptor_read1,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read2(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read2\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 40)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 40)
			, pTX_NORMAL_DESC[desc_num + 40].tdes3,
			pTX_NORMAL_DESC[desc_num + 40].tdes2,
			pTX_NORMAL_DESC[desc_num + 40].tdes1,
			pTX_NORMAL_DESC[desc_num + 40].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read2\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops2 = {
	.read = tx_normal_desc_descriptor_read2,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read3(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read3\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 60)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 60)
			, pTX_NORMAL_DESC[desc_num + 60].tdes3,
			pTX_NORMAL_DESC[desc_num + 60].tdes2,
			pTX_NORMAL_DESC[desc_num + 60].tdes1,
			pTX_NORMAL_DESC[desc_num + 60].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read3\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops3 = {
	.read = tx_normal_desc_descriptor_read3,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read4(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read4\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 80)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 80)
			, pTX_NORMAL_DESC[desc_num + 80].tdes3,
			pTX_NORMAL_DESC[desc_num + 80].tdes2,
			pTX_NORMAL_DESC[desc_num + 80].tdes1,
			pTX_NORMAL_DESC[desc_num + 80].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read4\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops4 = {
	.read = tx_normal_desc_descriptor_read4,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read5(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read5\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 100)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 100)
			, pTX_NORMAL_DESC[desc_num + 100].tdes3,
			pTX_NORMAL_DESC[desc_num + 100].tdes2,
			pTX_NORMAL_DESC[desc_num + 100].tdes1,
			pTX_NORMAL_DESC[desc_num + 100].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read5\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops5 = {
	.read = tx_normal_desc_descriptor_read5,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read6(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read6\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 120)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 120)
			, pTX_NORMAL_DESC[desc_num + 120].tdes3,
			pTX_NORMAL_DESC[desc_num + 120].tdes2,
			pTX_NORMAL_DESC[desc_num + 120].tdes1,
			pTX_NORMAL_DESC[desc_num + 120].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read6\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops6 = {
	.read = tx_normal_desc_descriptor_read6,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read7(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read7\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 140)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 140)
			, pTX_NORMAL_DESC[desc_num + 140].tdes3,
			pTX_NORMAL_DESC[desc_num + 140].tdes2,
			pTX_NORMAL_DESC[desc_num + 140].tdes1,
			pTX_NORMAL_DESC[desc_num + 140].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read7\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops7 = {
	.read = tx_normal_desc_descriptor_read7,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read8(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read8\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 160)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 160)
			, pTX_NORMAL_DESC[desc_num + 160].tdes3,
			pTX_NORMAL_DESC[desc_num + 160].tdes2,
			pTX_NORMAL_DESC[desc_num + 160].tdes1,
			pTX_NORMAL_DESC[desc_num + 160].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read8\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops8 = {
	.read = tx_normal_desc_descriptor_read8,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read9(struct file *file,
					       char __user * userbuf,
					       size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read9\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 180)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 180)
			, pTX_NORMAL_DESC[desc_num + 180].tdes3,
			pTX_NORMAL_DESC[desc_num + 180].tdes2,
			pTX_NORMAL_DESC[desc_num + 180].tdes1,
			pTX_NORMAL_DESC[desc_num + 180].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read9\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops9 = {
	.read = tx_normal_desc_descriptor_read9,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read10(struct file *file,
						char __user * userbuf,
						size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read10\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 200)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 200)
			, pTX_NORMAL_DESC[desc_num + 200].tdes3,
			pTX_NORMAL_DESC[desc_num + 200].tdes2,
			pTX_NORMAL_DESC[desc_num + 200].tdes1,
			pTX_NORMAL_DESC[desc_num + 200].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read10\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops10 = {
	.read = tx_normal_desc_descriptor_read10,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read11(struct file *file,
						char __user * userbuf,
						size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read11\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 220)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 220)
			, pTX_NORMAL_DESC[desc_num + 220].tdes3,
			pTX_NORMAL_DESC[desc_num + 220].tdes2,
			pTX_NORMAL_DESC[desc_num + 220].tdes1,
			pTX_NORMAL_DESC[desc_num + 220].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read11\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops11 = {
	.read = tx_normal_desc_descriptor_read11,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_descriptor_read12(struct file *file,
						char __user * userbuf,
						size_t count, loff_t * ppos)
{
	struct s_tx_normal_desc *pTX_NORMAL_DESC = GET_TX_DESC_PTR(qInx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> tx_normal_desc_descriptor_read12\n");
	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(9952, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 16; desc_num++) {
		sprintf(tmpBuf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"TX_NORMAL_DESC.tdes3             :%x\n"
			"TX_NORMAL_DESC.tdes2             :%x\n"
			"TX_NORMAL_DESC.tdes1             :%x\n"
			"TX_NORMAL_DESC.tdes0             :%x\n",
			qInx_val, (int)(desc_num + 240)
			, (unsigned long) GET_TX_DESC_DMA_ADDR(qInx_val, desc_num + 240)
			, pTX_NORMAL_DESC[desc_num + 240].tdes3,
			pTX_NORMAL_DESC[desc_num + 240].tdes2,
			pTX_NORMAL_DESC[desc_num + 240].tdes1,
			pTX_NORMAL_DESC[desc_num + 240].tdes0);
		strcat(debug_buf, tmpBuf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_descriptor_read12\n");
	return ret;
}

static const struct file_operations tx_normal_desc_desc_fops12 = {
	.read = tx_normal_desc_descriptor_read12,
	.write = descriptor_write,
};

static ssize_t tx_normal_desc_status_read(struct file *file,
					  char __user * userbuf, size_t count,
					  loff_t * ppos)
{
	struct dwc_eqos_tx_wrapper_descriptor *desc_data =
	    GET_TX_WRAPPER_DESC(qInx_val);
	ssize_t ret = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;
	int i;
	/* shadow variables */
	unsigned int cur_tx = desc_data->cur_tx;
	unsigned int dirty_tx = desc_data->dirty_tx;
	unsigned int free_desc_cnt = desc_data->free_desc_cnt;
	unsigned int tx_pkt_queued = desc_data->tx_pkt_queued;

	unsigned int tmp_cur_tx = 0;
	unsigned int tmp_dirty_tx = 0;

	DBGPR("--> tx_normal_desc_status_read\n");

	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(9952, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';

	sprintf(debug_buf,
		"\nCHANNEL %d TX DESCRIPTORS STATUS......................\n\n",
		qInx_val);

	sprintf(tmpBuf, "TOTAL DESCRIPTOR COUNT                   : %d\n\n"
		"TOTAL FREE DESCRIPTOR COUNT              : %d\n\n"
		"TOTAL DESCRIPTOR QUEUED FOR TRANSMISSION : %d\n\n"
		"NEXT DESCRIPTOR TO BE USED BY DRIVER     : %d\n\n"
		"NEXT DESCRIPTOR TO BE USED BY DEVICE     : %d\n\n",
		TX_DESC_CNT, free_desc_cnt, tx_pkt_queued, cur_tx, dirty_tx);
	strcat(debug_buf, tmpBuf);

	/* Free tx descriptor index */
	if ((free_desc_cnt == TX_DESC_CNT) && (tx_pkt_queued == 0)) {
		sprintf(tmpBuf,
			"ALL %d DESCRIPTORS ARE FREE, HENCE NO PACKETS ARE QUEUED FOR TRANSMISSION\n",
			TX_DESC_CNT);
		strcat(debug_buf, tmpBuf);
	} else if ((free_desc_cnt == 0) && (tx_pkt_queued == TX_DESC_CNT)) {
		sprintf(tmpBuf,
			"ALL %d DESCRIPTORS ARE USED FOR TRANSMISSION, HENCE NO FREE DESCRIPTORS\n",
			TX_DESC_CNT);
		strcat(debug_buf, tmpBuf);
	} else {
		if (free_desc_cnt > 0) {
			tmp_cur_tx = cur_tx;
			tmp_dirty_tx = dirty_tx;
			i = 1;
			sprintf(tmpBuf,
				"FREE DESCRIPTORS INDEX(es) are           :\n\n");
			strcat(debug_buf, tmpBuf);
			if (tmp_cur_tx > tmp_dirty_tx) {
				for (; tmp_cur_tx < TX_DESC_CNT; tmp_cur_tx++) {
					sprintf(tmpBuf, "%d ", tmp_cur_tx);
					strcat(debug_buf, tmpBuf);
					if ((i % 16) == 0) {
						sprintf(tmpBuf, "\n");
						strcat(debug_buf, tmpBuf);
					}
					i++;
				}
				for (tmp_cur_tx = 0; tmp_cur_tx < tmp_dirty_tx;
				     tmp_cur_tx++) {
					sprintf(tmpBuf, "%d ", tmp_cur_tx);
					strcat(debug_buf, tmpBuf);
					if ((i % 16) == 0) {
						sprintf(tmpBuf, "\n");
						strcat(debug_buf, tmpBuf);
					}
					i++;
				}
			} else {	/* (tmp_cur_tx < tmp_dirty_tx) */
				for (; tmp_cur_tx > tmp_dirty_tx; tmp_cur_tx++) {
					sprintf(tmpBuf, "%d ", tmp_cur_tx);
					strcat(debug_buf, tmpBuf);
					if ((i % 16) == 0) {
						sprintf(tmpBuf, "\n");
						strcat(debug_buf, tmpBuf);
					}
					i++;
				}
			}
			sprintf(tmpBuf, "\n");
			strcat(debug_buf, tmpBuf);
		}

		if (tx_pkt_queued > 0) {
			tmp_cur_tx = cur_tx;
			tmp_dirty_tx = dirty_tx;
			i = 1;
			sprintf(tmpBuf,
				"\nUSED DESCRIPTORS INDEX(es) are           :\n\n");
			strcat(debug_buf, tmpBuf);
			if (tmp_dirty_tx < tmp_cur_tx) {
				for (; tmp_dirty_tx < tmp_cur_tx;
				     tmp_dirty_tx++) {
					sprintf(tmpBuf, "%d ", tmp_dirty_tx);
					strcat(debug_buf, tmpBuf);
					if ((i % 16) == 0) {
						sprintf(tmpBuf, "\n");
						strcat(debug_buf, tmpBuf);
					}
					i++;
				}
			} else {	/* (tmp_dirty_tx < tmp_cur_tx) */
				for (; tmp_dirty_tx < TX_DESC_CNT;
				     tmp_dirty_tx++) {
					sprintf(tmpBuf, "%d ", tmp_dirty_tx);
					strcat(debug_buf, tmpBuf);
					if ((i % 16) == 0) {
						sprintf(tmpBuf, "\n");
						strcat(debug_buf, tmpBuf);
					}
					i++;
				}
				for (tmp_dirty_tx = 0;
				     tmp_dirty_tx < tmp_cur_tx;
				     tmp_dirty_tx++) {
					sprintf(tmpBuf, "%d ", tmp_dirty_tx);
					strcat(debug_buf, tmpBuf);
					if ((i % 16) == 0) {
						sprintf(tmpBuf, "\n");
						strcat(debug_buf, tmpBuf);
					}
					i++;
				}
			}
			sprintf(tmpBuf, "\n");
			strcat(debug_buf, tmpBuf);
		}
	}
	sprintf(tmpBuf, "\n.........................................DONE\n\n");
	strcat(debug_buf, tmpBuf);

	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<-- tx_normal_desc_status_read\n");
	return ret;
}

static const struct file_operations tx_normal_desc_status_fops = {
	.read = tx_normal_desc_status_read,
};

static ssize_t rx_normal_desc_status_read(struct file *file,
					  char __user * userbuf, size_t count,
					  loff_t * ppos)
{
	struct dwc_eqos_rx_wrapper_descriptor *desc_data =
	    GET_RX_WRAPPER_DESC(qInx_val);
	ssize_t ret = 0;
	char *tmpBuf = NULL;
	char *debug_buf = NULL;
	unsigned varDMA_CHRDR;	/* head ptr */
	unsigned varDMA_RDTP_RPDR;	/* tail ptr */
	unsigned varDMA_RDLAR;	/* tail ptr */
	unsigned tail_idx;
	unsigned head_idx;
	unsigned drv_desc_cnt = 0;
	unsigned dev_desc_cnt = 0;
	unsigned int cur_rx = desc_data->cur_rx;

	DBGPR("-->rx_normal_desc_status_read\n");

	tmpBuf = (char *)kmalloc(622, GFP_KERNEL);
	if (!tmpBuf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf = (char *)kmalloc(9952, GFP_KERNEL);
	if (!debug_buf) {
		printk(KERN_ERR "Memory allocation failed:\n");
		return -ENOMEM;
	}
	debug_buf[0] = '\0';

	cur_rx = desc_data->cur_rx;
	varDMA_CHRDR = dwceqos_readl(pdata, DWCEQOS_DMA_CH_CUR_APP_RXDESC(qInx_val));
	varDMA_RDTP_RPDR = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_TAIL_PTR(qInx_val));
	varDMA_RDLAR = dwceqos_readl(pdata, DWCEQOS_DMA_CH_RXDESC_LIST_ADDR(qInx_val));

	tail_idx =
	    (varDMA_RDTP_RPDR - varDMA_RDLAR) / sizeof(struct s_rx_normal_desc);

	head_idx =
	    (varDMA_CHRDR - varDMA_RDLAR) / sizeof(struct s_rx_normal_desc);

	if (tail_idx == head_idx) {
		dev_desc_cnt = 0;
		drv_desc_cnt = RX_DESC_CNT;
		printk(KERN_ALERT "\nhead:[%d]%#x tail:[%d]%#x cur-rx:%d\n",
		       head_idx, varDMA_CHRDR, tail_idx,
		       varDMA_RDTP_RPDR, cur_rx);
	} else if (head_idx > tail_idx) {	/* tail ptr is above head ptr */
		dev_desc_cnt = RX_DESC_CNT - (head_idx - tail_idx) + 1;
		drv_desc_cnt = RX_DESC_CNT - dev_desc_cnt;
	} else {		/* tail ptr is below head ptr */
		dev_desc_cnt = (tail_idx - head_idx + 1);
		drv_desc_cnt = RX_DESC_CNT - dev_desc_cnt;
	}

	sprintf(debug_buf,
		"\nCHANNEL %d : RX DESCRIPTORS STATUS....................\n\n",
		qInx_val);

	sprintf(tmpBuf, "TOTAL DESCRIPTOR COUNT                   : %d\n\n"
		"TOTAL DESCRIPTORS OWNED BY DEVICE        : %d\n\n"
		"TOTAL DESCRIPTOR OWNED BY DRIVER         : %d\n\n"
		"NEXT DESCRIPTOR TO BE USED BY DEVICE     : %d\n\n"
		"NEXT DESCRIPTOR TO BE USED BY DRIVER     : %d\n\n",
		RX_DESC_CNT, dev_desc_cnt, drv_desc_cnt, head_idx, cur_rx);
	strcat(debug_buf, tmpBuf);
	sprintf(tmpBuf, "\n.........................................DONE\n\n");
	strcat(debug_buf, tmpBuf);

	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmpBuf);
	kfree(debug_buf);

	DBGPR("<--rx_normal_desc_status_read\n");
	return ret;
}

static const struct file_operations rx_normal_desc_status_fops = {
	.read = rx_normal_desc_status_read,
};

/*! 
*  \brief  API to create debugfs files 
*
* \details This function will creates debug files required for debugging.
* All debug files are created inside a directory named as ddgen_dwc_eqos
* (debugfs directory /sys/kernel/debug/ddgen_dwc_eqos directory).
* Note: Before doing any read or write operation, debugfs has to be mounted on system.
*
* \retval  0 on Success. 
* \retval  error number on Failure.
*/

int create_debug_files(void)
{
	int ret = 0;
	struct dentry *registers;
	struct dentry *mac_ma32_127lr127;
	struct dentry *mac_ma32_127lr126;
	struct dentry *mac_ma32_127lr125;
	struct dentry *mac_ma32_127lr124;
	struct dentry *mac_ma32_127lr123;
	struct dentry *mac_ma32_127lr122;
	struct dentry *mac_ma32_127lr121;
	struct dentry *mac_ma32_127lr120;
	struct dentry *mac_ma32_127lr119;
	struct dentry *mac_ma32_127lr118;
	struct dentry *mac_ma32_127lr117;
	struct dentry *mac_ma32_127lr116;
	struct dentry *mac_ma32_127lr115;
	struct dentry *mac_ma32_127lr114;
	struct dentry *mac_ma32_127lr113;
	struct dentry *mac_ma32_127lr112;
	struct dentry *mac_ma32_127lr111;
	struct dentry *mac_ma32_127lr110;
	struct dentry *mac_ma32_127lr109;
	struct dentry *mac_ma32_127lr108;
	struct dentry *mac_ma32_127lr107;
	struct dentry *mac_ma32_127lr106;
	struct dentry *mac_ma32_127lr105;
	struct dentry *mac_ma32_127lr104;
	struct dentry *mac_ma32_127lr103;
	struct dentry *mac_ma32_127lr102;
	struct dentry *mac_ma32_127lr101;
	struct dentry *mac_ma32_127lr100;
	struct dentry *mac_ma32_127lr99;
	struct dentry *mac_ma32_127lr98;
	struct dentry *mac_ma32_127lr97;
	struct dentry *mac_ma32_127lr96;
	struct dentry *mac_ma32_127lr95;
	struct dentry *mac_ma32_127lr94;
	struct dentry *mac_ma32_127lr93;
	struct dentry *mac_ma32_127lr92;
	struct dentry *mac_ma32_127lr91;
	struct dentry *mac_ma32_127lr90;
	struct dentry *mac_ma32_127lr89;
	struct dentry *mac_ma32_127lr88;
	struct dentry *mac_ma32_127lr87;
	struct dentry *mac_ma32_127lr86;
	struct dentry *mac_ma32_127lr85;
	struct dentry *mac_ma32_127lr84;
	struct dentry *mac_ma32_127lr83;
	struct dentry *mac_ma32_127lr82;
	struct dentry *mac_ma32_127lr81;
	struct dentry *mac_ma32_127lr80;
	struct dentry *mac_ma32_127lr79;
	struct dentry *mac_ma32_127lr78;
	struct dentry *mac_ma32_127lr77;
	struct dentry *mac_ma32_127lr76;
	struct dentry *mac_ma32_127lr75;
	struct dentry *mac_ma32_127lr74;
	struct dentry *mac_ma32_127lr73;
	struct dentry *mac_ma32_127lr72;
	struct dentry *mac_ma32_127lr71;
	struct dentry *mac_ma32_127lr70;
	struct dentry *mac_ma32_127lr69;
	struct dentry *mac_ma32_127lr68;
	struct dentry *mac_ma32_127lr67;
	struct dentry *mac_ma32_127lr66;
	struct dentry *mac_ma32_127lr65;
	struct dentry *mac_ma32_127lr64;
	struct dentry *mac_ma32_127lr63;
	struct dentry *mac_ma32_127lr62;
	struct dentry *mac_ma32_127lr61;
	struct dentry *mac_ma32_127lr60;
	struct dentry *mac_ma32_127lr59;
	struct dentry *mac_ma32_127lr58;
	struct dentry *mac_ma32_127lr57;
	struct dentry *mac_ma32_127lr56;
	struct dentry *mac_ma32_127lr55;
	struct dentry *mac_ma32_127lr54;
	struct dentry *mac_ma32_127lr53;
	struct dentry *mac_ma32_127lr52;
	struct dentry *mac_ma32_127lr51;
	struct dentry *mac_ma32_127lr50;
	struct dentry *mac_ma32_127lr49;
	struct dentry *mac_ma32_127lr48;
	struct dentry *mac_ma32_127lr47;
	struct dentry *mac_ma32_127lr46;
	struct dentry *mac_ma32_127lr45;
	struct dentry *mac_ma32_127lr44;
	struct dentry *mac_ma32_127lr43;
	struct dentry *mac_ma32_127lr42;
	struct dentry *mac_ma32_127lr41;
	struct dentry *mac_ma32_127lr40;
	struct dentry *mac_ma32_127lr39;
	struct dentry *mac_ma32_127lr38;
	struct dentry *mac_ma32_127lr37;
	struct dentry *mac_ma32_127lr36;
	struct dentry *mac_ma32_127lr35;
	struct dentry *mac_ma32_127lr34;
	struct dentry *mac_ma32_127lr33;
	struct dentry *mac_ma32_127lr32;
	struct dentry *mac_ma32_127hr127;
	struct dentry *mac_ma32_127hr126;
	struct dentry *mac_ma32_127hr125;
	struct dentry *mac_ma32_127hr124;
	struct dentry *mac_ma32_127hr123;
	struct dentry *mac_ma32_127hr122;
	struct dentry *mac_ma32_127hr121;
	struct dentry *mac_ma32_127hr120;
	struct dentry *mac_ma32_127hr119;
	struct dentry *mac_ma32_127hr118;
	struct dentry *mac_ma32_127hr117;
	struct dentry *mac_ma32_127hr116;
	struct dentry *mac_ma32_127hr115;
	struct dentry *mac_ma32_127hr114;
	struct dentry *mac_ma32_127hr113;
	struct dentry *mac_ma32_127hr112;
	struct dentry *mac_ma32_127hr111;
	struct dentry *mac_ma32_127hr110;
	struct dentry *mac_ma32_127hr109;
	struct dentry *mac_ma32_127hr108;
	struct dentry *mac_ma32_127hr107;
	struct dentry *mac_ma32_127hr106;
	struct dentry *mac_ma32_127hr105;
	struct dentry *mac_ma32_127hr104;
	struct dentry *mac_ma32_127hr103;
	struct dentry *mac_ma32_127hr102;
	struct dentry *mac_ma32_127hr101;
	struct dentry *mac_ma32_127hr100;
	struct dentry *mac_ma32_127hr99;
	struct dentry *mac_ma32_127hr98;
	struct dentry *mac_ma32_127hr97;
	struct dentry *mac_ma32_127hr96;
	struct dentry *mac_ma32_127hr95;
	struct dentry *mac_ma32_127hr94;
	struct dentry *mac_ma32_127hr93;
	struct dentry *mac_ma32_127hr92;
	struct dentry *mac_ma32_127hr91;
	struct dentry *mac_ma32_127hr90;
	struct dentry *mac_ma32_127hr89;
	struct dentry *mac_ma32_127hr88;
	struct dentry *mac_ma32_127hr87;
	struct dentry *mac_ma32_127hr86;
	struct dentry *mac_ma32_127hr85;
	struct dentry *mac_ma32_127hr84;
	struct dentry *mac_ma32_127hr83;
	struct dentry *mac_ma32_127hr82;
	struct dentry *mac_ma32_127hr81;
	struct dentry *mac_ma32_127hr80;
	struct dentry *mac_ma32_127hr79;
	struct dentry *mac_ma32_127hr78;
	struct dentry *mac_ma32_127hr77;
	struct dentry *mac_ma32_127hr76;
	struct dentry *mac_ma32_127hr75;
	struct dentry *mac_ma32_127hr74;
	struct dentry *mac_ma32_127hr73;
	struct dentry *mac_ma32_127hr72;
	struct dentry *mac_ma32_127hr71;
	struct dentry *mac_ma32_127hr70;
	struct dentry *mac_ma32_127hr69;
	struct dentry *mac_ma32_127hr68;
	struct dentry *mac_ma32_127hr67;
	struct dentry *mac_ma32_127hr66;
	struct dentry *mac_ma32_127hr65;
	struct dentry *mac_ma32_127hr64;
	struct dentry *mac_ma32_127hr63;
	struct dentry *mac_ma32_127hr62;
	struct dentry *mac_ma32_127hr61;
	struct dentry *mac_ma32_127hr60;
	struct dentry *mac_ma32_127hr59;
	struct dentry *mac_ma32_127hr58;
	struct dentry *mac_ma32_127hr57;
	struct dentry *mac_ma32_127hr56;
	struct dentry *mac_ma32_127hr55;
	struct dentry *mac_ma32_127hr54;
	struct dentry *mac_ma32_127hr53;
	struct dentry *mac_ma32_127hr52;
	struct dentry *mac_ma32_127hr51;
	struct dentry *mac_ma32_127hr50;
	struct dentry *mac_ma32_127hr49;
	struct dentry *mac_ma32_127hr48;
	struct dentry *mac_ma32_127hr47;
	struct dentry *mac_ma32_127hr46;
	struct dentry *mac_ma32_127hr45;
	struct dentry *mac_ma32_127hr44;
	struct dentry *mac_ma32_127hr43;
	struct dentry *mac_ma32_127hr42;
	struct dentry *mac_ma32_127hr41;
	struct dentry *mac_ma32_127hr40;
	struct dentry *mac_ma32_127hr39;
	struct dentry *mac_ma32_127hr38;
	struct dentry *mac_ma32_127hr37;
	struct dentry *mac_ma32_127hr36;
	struct dentry *mac_ma32_127hr35;
	struct dentry *mac_ma32_127hr34;
	struct dentry *mac_ma32_127hr33;
	struct dentry *mac_ma32_127hr32;
	struct dentry *mac_ma1_31lr31;
	struct dentry *mac_ma1_31lr30;
	struct dentry *mac_ma1_31lr29;
	struct dentry *mac_ma1_31lr28;
	struct dentry *mac_ma1_31lr27;
	struct dentry *mac_ma1_31lr26;
	struct dentry *mac_ma1_31lr25;
	struct dentry *mac_ma1_31lr24;
	struct dentry *mac_ma1_31lr23;
	struct dentry *mac_ma1_31lr22;
	struct dentry *mac_ma1_31lr21;
	struct dentry *mac_ma1_31lr20;
	struct dentry *mac_ma1_31lr19;
	struct dentry *mac_ma1_31lr18;
	struct dentry *mac_ma1_31lr17;
	struct dentry *mac_ma1_31lr16;
	struct dentry *mac_ma1_31lr15;
	struct dentry *mac_ma1_31lr14;
	struct dentry *mac_ma1_31lr13;
	struct dentry *mac_ma1_31lr12;
	struct dentry *mac_ma1_31lr11;
	struct dentry *mac_ma1_31lr10;
	struct dentry *mac_ma1_31lr9;
	struct dentry *mac_ma1_31lr8;
	struct dentry *mac_ma1_31lr7;
	struct dentry *mac_ma1_31lr6;
	struct dentry *mac_ma1_31lr5;
	struct dentry *mac_ma1_31lr4;
	struct dentry *mac_ma1_31lr3;
	struct dentry *mac_ma1_31lr2;
	struct dentry *mac_ma1_31lr1;
	struct dentry *mac_ma1_31hr31;
	struct dentry *mac_ma1_31hr30;
	struct dentry *mac_ma1_31hr29;
	struct dentry *mac_ma1_31hr28;
	struct dentry *mac_ma1_31hr27;
	struct dentry *mac_ma1_31hr26;
	struct dentry *mac_ma1_31hr25;
	struct dentry *mac_ma1_31hr24;
	struct dentry *mac_ma1_31hr23;
	struct dentry *mac_ma1_31hr22;
	struct dentry *mac_ma1_31hr21;
	struct dentry *mac_ma1_31hr20;
	struct dentry *mac_ma1_31hr19;
	struct dentry *mac_ma1_31hr18;
	struct dentry *mac_ma1_31hr17;
	struct dentry *mac_ma1_31hr16;
	struct dentry *mac_ma1_31hr15;
	struct dentry *mac_ma1_31hr14;
	struct dentry *mac_ma1_31hr13;
	struct dentry *mac_ma1_31hr12;
	struct dentry *mac_ma1_31hr11;
	struct dentry *mac_ma1_31hr10;
	struct dentry *mac_ma1_31hr9;
	struct dentry *mac_ma1_31hr8;
	struct dentry *mac_ma1_31hr7;
	struct dentry *mac_ma1_31hr6;
	struct dentry *mac_ma1_31hr5;
	struct dentry *mac_ma1_31hr4;
	struct dentry *mac_ma1_31hr3;
	struct dentry *mac_ma1_31hr2;
	struct dentry *mac_ma1_31hr1;
	struct dentry *mac_arpa;
	struct dentry *mac_l3a3r7;
	struct dentry *mac_l3a3r6;
	struct dentry *mac_l3a3r5;
	struct dentry *mac_l3a3r4;
	struct dentry *mac_l3a3r3;
	struct dentry *mac_l3a3r2;
	struct dentry *mac_l3a3r1;
	struct dentry *mac_l3a3r0;
	struct dentry *mac_l3a2r7;
	struct dentry *mac_l3a2r6;
	struct dentry *mac_l3a2r5;
	struct dentry *mac_l3a2r4;
	struct dentry *mac_l3a2r3;
	struct dentry *mac_l3a2r2;
	struct dentry *mac_l3a2r1;
	struct dentry *mac_l3a2r0;
	struct dentry *mac_l3a1r7;
	struct dentry *mac_l3a1r6;
	struct dentry *mac_l3a1r5;
	struct dentry *mac_l3a1r4;
	struct dentry *mac_l3a1r3;
	struct dentry *mac_l3a1r2;
	struct dentry *mac_l3a1r1;
	struct dentry *mac_l3a1r0;
	struct dentry *mac_l3a0r7;
	struct dentry *mac_l3a0r6;
	struct dentry *mac_l3a0r5;
	struct dentry *mac_l3a0r4;
	struct dentry *mac_l3a0r3;
	struct dentry *mac_l3a0r2;
	struct dentry *mac_l3a0r1;
	struct dentry *mac_l3a0r0;
	struct dentry *mac_l4ar7;
	struct dentry *mac_l4ar6;
	struct dentry *mac_l4ar5;
	struct dentry *mac_l4ar4;
	struct dentry *mac_l4ar3;
	struct dentry *mac_l4ar2;
	struct dentry *mac_l4ar1;
	struct dentry *mac_l4ar0;
	struct dentry *mac_l3l4cr7;
	struct dentry *mac_l3l4cr6;
	struct dentry *mac_l3l4cr5;
	struct dentry *mac_l3l4cr4;
	struct dentry *mac_l3l4cr3;
	struct dentry *mac_l3l4cr2;
	struct dentry *mac_l3l4cr1;
	struct dentry *mac_l3l4cr0;
	struct dentry *mac_gpio;
	struct dentry *mac_pcs;
	struct dentry *mac_tes;
	struct dentry *mac_ae;
	struct dentry *mac_alpa;
	struct dentry *mac_aad;
	struct dentry *mac_ans;
	struct dentry *mac_anc;
	struct dentry *mac_lpc;
	struct dentry *mac_lps;
    struct dentry *mac_lmir;
	struct dentry *mac_spi2r;
	struct dentry *mac_spi1r;
	struct dentry *mac_spi0r;
	struct dentry *mac_pto_cr;
	struct dentry *mac_pps_width3;
	struct dentry *mac_pps_width2;
	struct dentry *mac_pps_width1;
	struct dentry *mac_pps_width0;
	struct dentry *mac_pps_intval3;
	struct dentry *mac_pps_intval2;
	struct dentry *mac_pps_intval1;
	struct dentry *mac_pps_intval0;
	struct dentry *mac_pps_ttns3;
	struct dentry *mac_pps_ttns2;
	struct dentry *mac_pps_ttns1;
	struct dentry *mac_pps_ttns0;
	struct dentry *mac_pps_tts3;
	struct dentry *mac_pps_tts2;
	struct dentry *mac_pps_tts1;
	struct dentry *mac_pps_tts0;
	struct dentry *mac_ppsc;
	struct dentry *mac_teac;
	struct dentry *mac_tiac;
	struct dentry *mac_ats;
	struct dentry *mac_atn;
	struct dentry *mac_ac;
	struct dentry *mac_ttn;
	struct dentry *mac_ttsn;
	struct dentry *mac_tsr;
	struct dentry *mac_sthwr;
	struct dentry *mac_tar;
	struct dentry *mac_stnsur;
	struct dentry *mac_stsur;
	struct dentry *mac_stnsr;
	struct dentry *mac_stsr;
	struct dentry *mac_ssir;
	struct dentry *mac_tcr;
	struct dentry *mtl_dsr;
	struct dentry *mac_rwpffr;
	struct dentry *mac_rtsr;
	struct dentry *mtl_ier;
	struct dentry *mtl_qrcr7;
	struct dentry *mtl_qrcr6;
	struct dentry *mtl_qrcr5;
	struct dentry *mtl_qrcr4;
	struct dentry *mtl_qrcr3;
	struct dentry *mtl_qrcr2;
	struct dentry *mtl_qrcr1;
	struct dentry *mtl_qrdr7;
	struct dentry *mtl_qrdr6;
	struct dentry *mtl_qrdr5;
	struct dentry *mtl_qrdr4;
	struct dentry *mtl_qrdr3;
	struct dentry *mtl_qrdr2;
	struct dentry *mtl_qrdr1;
	struct dentry *mtl_qocr7;
	struct dentry *mtl_qocr6;
	struct dentry *mtl_qocr5;
	struct dentry *mtl_qocr4;
	struct dentry *mtl_qocr3;
	struct dentry *mtl_qocr2;
	struct dentry *mtl_qocr1;
	struct dentry *mtl_qromr7;
	struct dentry *mtl_qromr6;
	struct dentry *mtl_qromr5;
	struct dentry *mtl_qromr4;
	struct dentry *mtl_qromr3;
	struct dentry *mtl_qromr2;
	struct dentry *mtl_qromr1;
	struct dentry *mtl_qlcr7;
	struct dentry *mtl_qlcr6;
	struct dentry *mtl_qlcr5;
	struct dentry *mtl_qlcr4;
	struct dentry *mtl_qlcr3;
	struct dentry *mtl_qlcr2;
	struct dentry *mtl_qlcr1;
	struct dentry *mtl_qhcr7;
	struct dentry *mtl_qhcr6;
	struct dentry *mtl_qhcr5;
	struct dentry *mtl_qhcr4;
	struct dentry *mtl_qhcr3;
	struct dentry *mtl_qhcr2;
	struct dentry *mtl_qhcr1;
	struct dentry *mtl_qsscr7;
	struct dentry *mtl_qsscr6;
	struct dentry *mtl_qsscr5;
	struct dentry *mtl_qsscr4;
	struct dentry *mtl_qsscr3;
	struct dentry *mtl_qsscr2;
	struct dentry *mtl_qsscr1;
	struct dentry *mtl_qw7;
	struct dentry *mtl_qw6;
	struct dentry *mtl_qw5;
	struct dentry *mtl_qw4;
	struct dentry *mtl_qw3;
	struct dentry *mtl_qw2;
	struct dentry *mtl_qw1;
	struct dentry *mtl_qesr7;
	struct dentry *mtl_qesr6;
	struct dentry *mtl_qesr5;
	struct dentry *mtl_qesr4;
	struct dentry *mtl_qesr3;
	struct dentry *mtl_qesr2;
	struct dentry *mtl_qesr1;
	struct dentry *mtl_qecr7;
	struct dentry *mtl_qecr6;
	struct dentry *mtl_qecr5;
	struct dentry *mtl_qecr4;
	struct dentry *mtl_qecr3;
	struct dentry *mtl_qecr2;
	struct dentry *mtl_qecr1;
	struct dentry *mtl_qtdr7;
	struct dentry *mtl_qtdr6;
	struct dentry *mtl_qtdr5;
	struct dentry *mtl_qtdr4;
	struct dentry *mtl_qtdr3;
	struct dentry *mtl_qtdr2;
	struct dentry *mtl_qtdr1;
	struct dentry *mtl_qucr7;
	struct dentry *mtl_qucr6;
	struct dentry *mtl_qucr5;
	struct dentry *mtl_qucr4;
	struct dentry *mtl_qucr3;
	struct dentry *mtl_qucr2;
	struct dentry *mtl_qucr1;
	struct dentry *mtl_qtomr7;
	struct dentry *mtl_qtomr6;
	struct dentry *mtl_qtomr5;
	struct dentry *mtl_qtomr4;
	struct dentry *mtl_qtomr3;
	struct dentry *mtl_qtomr2;
	struct dentry *mtl_qtomr1;
	struct dentry *mac_pmtcsr;
	struct dentry *mmc_rxicmp_err_octets;
	struct dentry *mmc_rxicmp_gd_octets;
	struct dentry *mmc_rxtcp_err_octets;
	struct dentry *mmc_rxtcp_gd_octets;
	struct dentry *mmc_rxudp_err_octets;
	struct dentry *mmc_rxudp_gd_octets;
	struct dentry *mmc_rxipv6_nopay_octets;
	struct dentry *mmc_rxipv6_hdrerr_octets;
	struct dentry *mmc_rxipv6_gd_octets;
	struct dentry *mmc_rxipv4_udsbl_octets;
	struct dentry *mmc_rxipv4_frag_octets;
	struct dentry *mmc_rxipv4_nopay_octets;
	struct dentry *mmc_rxipv4_hdrerr_octets;
	struct dentry *mmc_rxipv4_gd_octets;
	struct dentry *mmc_rxicmp_err_pkts;
	struct dentry *mmc_rxicmp_gd_pkts;
	struct dentry *mmc_rxtcp_err_pkts;
	struct dentry *mmc_rxtcp_gd_pkts;
	struct dentry *mmc_rxudp_err_pkts;
	struct dentry *mmc_rxudp_gd_pkts;
	struct dentry *mmc_rxipv6_nopay_pkts;
	struct dentry *mmc_rxipv6_hdrerr_pkts;
	struct dentry *mmc_rxipv6_gd_pkts;
	struct dentry *mmc_rxipv4_ubsbl_pkts;
	struct dentry *mmc_rxipv4_frag_pkts;
	struct dentry *mmc_rxipv4_nopay_pkts;
	struct dentry *mmc_rxipv4_hdrerr_pkts;
	struct dentry *mmc_rxipv4_gd_pkts;
	struct dentry *mmc_rxctrlpackets_g;
	struct dentry *mmc_rxrcverror;
	struct dentry *mmc_rxwatchdogerror;
	struct dentry *mmc_rxvlanpackets_gb;
	struct dentry *mmc_rxfifooverflow;
	struct dentry *mmc_rxpausepackets;
	struct dentry *mmc_rxoutofrangetype;
	struct dentry *mmc_rxlengtherror;
	struct dentry *mmc_rxunicastpackets_g;
	struct dentry *mmc_rx1024tomaxoctets_gb;
	struct dentry *mmc_rx512to1023octets_gb;
	struct dentry *mmc_rx256to511octets_gb;
	struct dentry *mmc_rx128to255octets_gb;
	struct dentry *mmc_rx65to127octets_gb;
	struct dentry *mmc_rx64octets_gb;
	struct dentry *mmc_rxoversize_g;
	struct dentry *mmc_rxundersize_g;
	struct dentry *mmc_rxjabbererror;
	struct dentry *mmc_rxrunterror;
	struct dentry *mmc_rxalignmenterror;
	struct dentry *mmc_rxcrcerror;
	struct dentry *mmc_rxmulticastpackets_g;
	struct dentry *mmc_rxbroadcastpackets_g;
	struct dentry *mmc_rxoctetcount_g;
	struct dentry *mmc_rxoctetcount_gb;
	struct dentry *mmc_rxpacketcount_gb;
	struct dentry *mmc_txoversize_g;
	struct dentry *mmc_txvlanpackets_g;
	struct dentry *mmc_txpausepackets;
	struct dentry *mmc_txexcessdef;
	struct dentry *mmc_txpacketscount_g;
	struct dentry *mmc_txoctetcount_g;
	struct dentry *mmc_txcarriererror;
	struct dentry *mmc_txexesscol;
	struct dentry *mmc_txlatecol;
	struct dentry *mmc_txdeferred;
	struct dentry *mmc_txmulticol_g;
	struct dentry *mmc_txsinglecol_g;
	struct dentry *mmc_txunderflowerror;
	struct dentry *mmc_txbroadcastpackets_gb;
	struct dentry *mmc_txmulticastpackets_gb;
	struct dentry *mmc_txunicastpackets_gb;
	struct dentry *mmc_tx1024tomaxoctets_gb;
	struct dentry *mmc_tx512to1023octets_gb;
	struct dentry *mmc_tx256to511octets_gb;
	struct dentry *mmc_tx128to255octets_gb;
	struct dentry *mmc_tx65to127octets_gb;
	struct dentry *mmc_tx64octets_gb;
	struct dentry *mmc_txmulticastpackets_g;
	struct dentry *mmc_txbroadcastpackets_g;
	struct dentry *mmc_txpacketcount_gb;
	struct dentry *mmc_txoctetcount_gb;
	struct dentry *mmc_ipc_intr_rx;
	struct dentry *mmc_ipc_intr_mask_rx;
	struct dentry *mmc_intr_mask_tx;
	struct dentry *mmc_intr_mask_rx;
	struct dentry *mmc_intr_tx;
	struct dentry *mmc_intr_rx;
	struct dentry *mmc_cntrl;
	struct dentry *mac_ma1lr;
	struct dentry *mac_ma1hr;
	struct dentry *mac_ma0lr;
	struct dentry *mac_ma0hr;
	struct dentry *mac_gpior;
	struct dentry *mac_gmiidr;
	struct dentry *mac_gmiiar;
	struct dentry *mac_hfr2;
	struct dentry *mac_hfr1;
	struct dentry *mac_hfr0;
	struct dentry *mac_mdr;
	struct dentry *mac_vr;
	struct dentry *mac_htr7;
	struct dentry *mac_htr6;
	struct dentry *mac_htr5;
	struct dentry *mac_htr4;
	struct dentry *mac_htr3;
	struct dentry *mac_htr2;
	struct dentry *mac_htr1;
	struct dentry *mac_htr0;
	struct dentry *dma_riwtr7;
	struct dentry *dma_riwtr6;
	struct dentry *dma_riwtr5;
	struct dentry *dma_riwtr4;
	struct dentry *dma_riwtr3;
	struct dentry *dma_riwtr2;
	struct dentry *dma_riwtr1;
	struct dentry *dma_riwtr0;
	struct dentry *dma_rdrlr7;
	struct dentry *dma_rdrlr6;
	struct dentry *dma_rdrlr5;
	struct dentry *dma_rdrlr4;
	struct dentry *dma_rdrlr3;
	struct dentry *dma_rdrlr2;
	struct dentry *dma_rdrlr1;
	struct dentry *dma_rdrlr0;
	struct dentry *dma_tdrlr7;
	struct dentry *dma_tdrlr6;
	struct dentry *dma_tdrlr5;
	struct dentry *dma_tdrlr4;
	struct dentry *dma_tdrlr3;
	struct dentry *dma_tdrlr2;
	struct dentry *dma_tdrlr1;
	struct dentry *dma_tdrlr0;
	struct dentry *dma_rdtp_rpdr7;
	struct dentry *dma_rdtp_rpdr6;
	struct dentry *dma_rdtp_rpdr5;
	struct dentry *dma_rdtp_rpdr4;
	struct dentry *dma_rdtp_rpdr3;
	struct dentry *dma_rdtp_rpdr2;
	struct dentry *dma_rdtp_rpdr1;
	struct dentry *dma_rdtp_rpdr0;
	struct dentry *dma_tdtp_tpdr7;
	struct dentry *dma_tdtp_tpdr6;
	struct dentry *dma_tdtp_tpdr5;
	struct dentry *dma_tdtp_tpdr4;
	struct dentry *dma_tdtp_tpdr3;
	struct dentry *dma_tdtp_tpdr2;
	struct dentry *dma_tdtp_tpdr1;
	struct dentry *dma_tdtp_tpdr0;
	struct dentry *dma_rdlar7;
	struct dentry *dma_rdlar6;
	struct dentry *dma_rdlar5;
	struct dentry *dma_rdlar4;
	struct dentry *dma_rdlar3;
	struct dentry *dma_rdlar2;
	struct dentry *dma_rdlar1;
	struct dentry *dma_rdlar0;
	struct dentry *dma_tdlar7;
	struct dentry *dma_tdlar6;
	struct dentry *dma_tdlar5;
	struct dentry *dma_tdlar4;
	struct dentry *dma_tdlar3;
	struct dentry *dma_tdlar2;
	struct dentry *dma_tdlar1;
	struct dentry *dma_tdlar0;
	struct dentry *dma_ier7;
	struct dentry *dma_ier6;
	struct dentry *dma_ier5;
	struct dentry *dma_ier4;
	struct dentry *dma_ier3;
	struct dentry *dma_ier2;
	struct dentry *dma_ier1;
	struct dentry *dma_ier0;
	struct dentry *mac_imr;
	struct dentry *mac_isr;
	struct dentry *mtl_isr;
	struct dentry *dma_sr7;
	struct dentry *dma_sr6;
	struct dentry *dma_sr5;
	struct dentry *dma_sr4;
	struct dentry *dma_sr3;
	struct dentry *dma_sr2;
	struct dentry *dma_sr1;
	struct dentry *dma_sr0;
	struct dentry *dma_isr;
	struct dentry *dma_dsr2;
	struct dentry *dma_dsr1;
	struct dentry *dma_dsr0;
	struct dentry *mtl_q0rdr;
	struct dentry *mtl_q0esr;
	struct dentry *mtl_q0tdr;
	struct dentry *dma_chrbar7;
	struct dentry *dma_chrbar6;
	struct dentry *dma_chrbar5;
	struct dentry *dma_chrbar4;
	struct dentry *dma_chrbar3;
	struct dentry *dma_chrbar2;
	struct dentry *dma_chrbar1;
	struct dentry *dma_chrbar0;
	struct dentry *dma_chtbar7;
	struct dentry *dma_chtbar6;
	struct dentry *dma_chtbar5;
	struct dentry *dma_chtbar4;
	struct dentry *dma_chtbar3;
	struct dentry *dma_chtbar2;
	struct dentry *dma_chtbar1;
	struct dentry *dma_chtbar0;
	struct dentry *dma_chrdr7;
	struct dentry *dma_chrdr6;
	struct dentry *dma_chrdr5;
	struct dentry *dma_chrdr4;
	struct dentry *dma_chrdr3;
	struct dentry *dma_chrdr2;
	struct dentry *dma_chrdr1;
	struct dentry *dma_chrdr0;
	struct dentry *dma_chtdr7;
	struct dentry *dma_chtdr6;
	struct dentry *dma_chtdr5;
	struct dentry *dma_chtdr4;
	struct dentry *dma_chtdr3;
	struct dentry *dma_chtdr2;
	struct dentry *dma_chtdr1;
	struct dentry *dma_chtdr0;
	struct dentry *dma_sfcsr7;
	struct dentry *dma_sfcsr6;
	struct dentry *dma_sfcsr5;
	struct dentry *dma_sfcsr4;
	struct dentry *dma_sfcsr3;
	struct dentry *dma_sfcsr2;
	struct dentry *dma_sfcsr1;
	struct dentry *dma_sfcsr0;
	struct dentry *mac_ivlantirr;
	struct dentry *mac_vlantirr;
	struct dentry *mac_vlanhtr;
	struct dentry *mac_vlantr;
	struct dentry *dma_sbus;
	struct dentry *dma_bmr;
	struct dentry *mtl_q0rcr;
	struct dentry *mtl_q0ocr;
	struct dentry *mtl_q0romr;
	struct dentry *mtl_q0qr;
	struct dentry *mtl_q0ecr;
	struct dentry *mtl_q0ucr;
	struct dentry *mtl_q0tomr;
	struct dentry *mtl_rqdcm1r;
	struct dentry *mtl_rqdcm0r;
	struct dentry *mtl_fddr;
	struct dentry *mtl_fdacs;
	struct dentry *mtl_omr;
	struct dentry *mac_rqc3r;
	struct dentry *mac_rqc2r;
	struct dentry *mac_rqc1r;
	struct dentry *mac_rqc0r;
	struct dentry *mac_tqpm1r;
	struct dentry *mac_tqpm0r;
	struct dentry *mac_rfcr;
	struct dentry *mac_qtfcr7;
	struct dentry *mac_qtfcr6;
	struct dentry *mac_qtfcr5;
	struct dentry *mac_qtfcr4;
	struct dentry *mac_qtfcr3;
	struct dentry *mac_qtfcr2;
	struct dentry *mac_qtfcr1;
	struct dentry *mac_q0tfcr;
#if 0
	struct dentry *dma_axi4cr7;
	struct dentry *dma_axi4cr6;
	struct dentry *dma_axi4cr5;
	struct dentry *dma_axi4cr4;
	struct dentry *dma_axi4cr3;
	struct dentry *dma_axi4cr2;
	struct dentry *dma_axi4cr1;
	struct dentry *dma_axi4cr0;
#endif
	struct dentry *dma_rcr7;
	struct dentry *dma_rcr6;
	struct dentry *dma_rcr5;
	struct dentry *dma_rcr4;
	struct dentry *dma_rcr3;
	struct dentry *dma_rcr2;
	struct dentry *dma_rcr1;
	struct dentry *dma_rcr0;
	struct dentry *dma_tcr7;
	struct dentry *dma_tcr6;
	struct dentry *dma_tcr5;
	struct dentry *dma_tcr4;
	struct dentry *dma_tcr3;
	struct dentry *dma_tcr2;
	struct dentry *dma_tcr1;
	struct dentry *dma_tcr0;
	struct dentry *dma_cr7;
	struct dentry *dma_cr6;
	struct dentry *dma_cr5;
	struct dentry *dma_cr4;
	struct dentry *dma_cr3;
	struct dentry *dma_cr2;
	struct dentry *dma_cr1;
	struct dentry *dma_cr0;
	struct dentry *mac_wtr;
	struct dentry *mac_mpfr;
	struct dentry *mac_mecr;
	struct dentry *mac_mcr;
	/* MII/GMII registers */
	struct dentry *mac_bmcr_reg;
	struct dentry *mac_bmsr_reg;
	struct dentry *mii_physid1_reg;
	struct dentry *mii_physid2_reg;
	struct dentry *mii_advertise_reg;
	struct dentry *mii_lpa_reg;
	struct dentry *mii_expansion_reg;
	struct dentry *auto_nego_np_reg;
	struct dentry *mii_estatus_reg;
	struct dentry *mii_ctrl1000_reg;
	struct dentry *mii_stat1000_reg;
	struct dentry *phy_ctl_reg;
	struct dentry *phy_sts_reg;
	struct dentry *feature_drop_tx_pktburstcnt;
	struct dentry *qInx;

	struct dentry *rx_normal_desc_desc0;
	struct dentry *rx_normal_desc_desc1;
	struct dentry *rx_normal_desc_desc2;
	struct dentry *rx_normal_desc_desc3;
	struct dentry *rx_normal_desc_desc4;
	struct dentry *rx_normal_desc_desc5;
	struct dentry *rx_normal_desc_desc6;
	struct dentry *rx_normal_desc_desc7;
	struct dentry *rx_normal_desc_desc8;
	struct dentry *rx_normal_desc_desc9;
	struct dentry *rx_normal_desc_desc10;
	struct dentry *rx_normal_desc_desc11;
	struct dentry *rx_normal_desc_desc12;
	struct dentry *tx_normal_desc_desc0;
	struct dentry *tx_normal_desc_desc1;
	struct dentry *tx_normal_desc_desc2;
	struct dentry *tx_normal_desc_desc3;
	struct dentry *tx_normal_desc_desc4;
	struct dentry *tx_normal_desc_desc5;
	struct dentry *tx_normal_desc_desc6;
	struct dentry *tx_normal_desc_desc7;
	struct dentry *tx_normal_desc_desc8;
	struct dentry *tx_normal_desc_desc9;
	struct dentry *tx_normal_desc_desc10;
	struct dentry *tx_normal_desc_desc11;
	struct dentry *tx_normal_desc_desc12;
	struct dentry *tx_normal_desc_status;
	struct dentry *rx_normal_desc_status;
	char debugfs_dir_name[32];
	struct net_device *ndev = pdata->ndev;

	DBGPR("--> create_debug_files\n");
	sprintf(debugfs_dir_name, "ddgen_dwc_%s_qos", ndev->name);

	dir = debugfs_create_dir((const char*)debugfs_dir_name, NULL);
	if (dir == NULL) {
		printk(KERN_INFO
		       "error creating directory: dwc_eqos_debug\n");
		return -ENODEV;
	}

	registers =
	    debugfs_create_file("registers", 744, dir, &registers_val,
				&registers_fops);
	if (registers == NULL) {
		printk(KERN_INFO "error creating file: registers\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr127 =
	    debugfs_create_file("mac_ma32_127lr127", 744, dir,
				&mac_ma32_127lr127_val,
				&mac_ma32_127lr127_fops);
	if (mac_ma32_127lr127 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr127\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr126 =
	    debugfs_create_file("mac_ma32_127lr126", 744, dir,
				&mac_ma32_127lr126_val,
				&mac_ma32_127lr126_fops);
	if (mac_ma32_127lr126 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr126\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr125 =
	    debugfs_create_file("mac_ma32_127lr125", 744, dir,
				&mac_ma32_127lr125_val,
				&mac_ma32_127lr125_fops);
	if (mac_ma32_127lr125 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr125\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr124 =
	    debugfs_create_file("mac_ma32_127lr124", 744, dir,
				&mac_ma32_127lr124_val,
				&mac_ma32_127lr124_fops);
	if (mac_ma32_127lr124 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr124\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr123 =
	    debugfs_create_file("mac_ma32_127lr123", 744, dir,
				&mac_ma32_127lr123_val,
				&mac_ma32_127lr123_fops);
	if (mac_ma32_127lr123 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr123\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr122 =
	    debugfs_create_file("mac_ma32_127lr122", 744, dir,
				&mac_ma32_127lr122_val,
				&mac_ma32_127lr122_fops);
	if (mac_ma32_127lr122 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr122\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr121 =
	    debugfs_create_file("mac_ma32_127lr121", 744, dir,
				&mac_ma32_127lr121_val,
				&mac_ma32_127lr121_fops);
	if (mac_ma32_127lr121 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr121\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr120 =
	    debugfs_create_file("mac_ma32_127lr120", 744, dir,
				&mac_ma32_127lr120_val,
				&mac_ma32_127lr120_fops);
	if (mac_ma32_127lr120 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr120\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr119 =
	    debugfs_create_file("mac_ma32_127lr119", 744, dir,
				&mac_ma32_127lr119_val,
				&mac_ma32_127lr119_fops);
	if (mac_ma32_127lr119 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr119\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr118 =
	    debugfs_create_file("mac_ma32_127lr118", 744, dir,
				&mac_ma32_127lr118_val,
				&mac_ma32_127lr118_fops);
	if (mac_ma32_127lr118 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr118\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr117 =
	    debugfs_create_file("mac_ma32_127lr117", 744, dir,
				&mac_ma32_127lr117_val,
				&mac_ma32_127lr117_fops);
	if (mac_ma32_127lr117 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr117\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr116 =
	    debugfs_create_file("mac_ma32_127lr116", 744, dir,
				&mac_ma32_127lr116_val,
				&mac_ma32_127lr116_fops);
	if (mac_ma32_127lr116 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr116\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr115 =
	    debugfs_create_file("mac_ma32_127lr115", 744, dir,
				&mac_ma32_127lr115_val,
				&mac_ma32_127lr115_fops);
	if (mac_ma32_127lr115 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr115\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr114 =
	    debugfs_create_file("mac_ma32_127lr114", 744, dir,
				&mac_ma32_127lr114_val,
				&mac_ma32_127lr114_fops);
	if (mac_ma32_127lr114 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr114\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr113 =
	    debugfs_create_file("mac_ma32_127lr113", 744, dir,
				&mac_ma32_127lr113_val,
				&mac_ma32_127lr113_fops);
	if (mac_ma32_127lr113 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr113\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr112 =
	    debugfs_create_file("mac_ma32_127lr112", 744, dir,
				&mac_ma32_127lr112_val,
				&mac_ma32_127lr112_fops);
	if (mac_ma32_127lr112 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr112\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr111 =
	    debugfs_create_file("mac_ma32_127lr111", 744, dir,
				&mac_ma32_127lr111_val,
				&mac_ma32_127lr111_fops);
	if (mac_ma32_127lr111 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr111\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr110 =
	    debugfs_create_file("mac_ma32_127lr110", 744, dir,
				&mac_ma32_127lr110_val,
				&mac_ma32_127lr110_fops);
	if (mac_ma32_127lr110 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr110\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr109 =
	    debugfs_create_file("mac_ma32_127lr109", 744, dir,
				&mac_ma32_127lr109_val,
				&mac_ma32_127lr109_fops);
	if (mac_ma32_127lr109 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr109\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr108 =
	    debugfs_create_file("mac_ma32_127lr108", 744, dir,
				&mac_ma32_127lr108_val,
				&mac_ma32_127lr108_fops);
	if (mac_ma32_127lr108 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr108\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr107 =
	    debugfs_create_file("mac_ma32_127lr107", 744, dir,
				&mac_ma32_127lr107_val,
				&mac_ma32_127lr107_fops);
	if (mac_ma32_127lr107 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr107\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr106 =
	    debugfs_create_file("mac_ma32_127lr106", 744, dir,
				&mac_ma32_127lr106_val,
				&mac_ma32_127lr106_fops);
	if (mac_ma32_127lr106 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr106\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr105 =
	    debugfs_create_file("mac_ma32_127lr105", 744, dir,
				&mac_ma32_127lr105_val,
				&mac_ma32_127lr105_fops);
	if (mac_ma32_127lr105 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr105\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr104 =
	    debugfs_create_file("mac_ma32_127lr104", 744, dir,
				&mac_ma32_127lr104_val,
				&mac_ma32_127lr104_fops);
	if (mac_ma32_127lr104 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr104\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr103 =
	    debugfs_create_file("mac_ma32_127lr103", 744, dir,
				&mac_ma32_127lr103_val,
				&mac_ma32_127lr103_fops);
	if (mac_ma32_127lr103 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr103\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr102 =
	    debugfs_create_file("mac_ma32_127lr102", 744, dir,
				&mac_ma32_127lr102_val,
				&mac_ma32_127lr102_fops);
	if (mac_ma32_127lr102 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr102\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr101 =
	    debugfs_create_file("mac_ma32_127lr101", 744, dir,
				&mac_ma32_127lr101_val,
				&mac_ma32_127lr101_fops);
	if (mac_ma32_127lr101 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr101\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr100 =
	    debugfs_create_file("mac_ma32_127lr100", 744, dir,
				&mac_ma32_127lr100_val,
				&mac_ma32_127lr100_fops);
	if (mac_ma32_127lr100 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr100\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr99 =
	    debugfs_create_file("mac_ma32_127lr99", 744, dir,
				&mac_ma32_127lr99_val, &mac_ma32_127lr99_fops);
	if (mac_ma32_127lr99 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr99\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr98 =
	    debugfs_create_file("mac_ma32_127lr98", 744, dir,
				&mac_ma32_127lr98_val, &mac_ma32_127lr98_fops);
	if (mac_ma32_127lr98 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr98\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr97 =
	    debugfs_create_file("mac_ma32_127lr97", 744, dir,
				&mac_ma32_127lr97_val, &mac_ma32_127lr97_fops);
	if (mac_ma32_127lr97 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr97\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr96 =
	    debugfs_create_file("mac_ma32_127lr96", 744, dir,
				&mac_ma32_127lr96_val, &mac_ma32_127lr96_fops);
	if (mac_ma32_127lr96 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr96\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr95 =
	    debugfs_create_file("mac_ma32_127lr95", 744, dir,
				&mac_ma32_127lr95_val, &mac_ma32_127lr95_fops);
	if (mac_ma32_127lr95 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr95\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr94 =
	    debugfs_create_file("mac_ma32_127lr94", 744, dir,
				&mac_ma32_127lr94_val, &mac_ma32_127lr94_fops);
	if (mac_ma32_127lr94 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr94\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr93 =
	    debugfs_create_file("mac_ma32_127lr93", 744, dir,
				&mac_ma32_127lr93_val, &mac_ma32_127lr93_fops);
	if (mac_ma32_127lr93 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr93\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr92 =
	    debugfs_create_file("mac_ma32_127lr92", 744, dir,
				&mac_ma32_127lr92_val, &mac_ma32_127lr92_fops);
	if (mac_ma32_127lr92 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr92\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr91 =
	    debugfs_create_file("mac_ma32_127lr91", 744, dir,
				&mac_ma32_127lr91_val, &mac_ma32_127lr91_fops);
	if (mac_ma32_127lr91 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr91\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr90 =
	    debugfs_create_file("mac_ma32_127lr90", 744, dir,
				&mac_ma32_127lr90_val, &mac_ma32_127lr90_fops);
	if (mac_ma32_127lr90 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr90\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr89 =
	    debugfs_create_file("mac_ma32_127lr89", 744, dir,
				&mac_ma32_127lr89_val, &mac_ma32_127lr89_fops);
	if (mac_ma32_127lr89 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr89\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr88 =
	    debugfs_create_file("mac_ma32_127lr88", 744, dir,
				&mac_ma32_127lr88_val, &mac_ma32_127lr88_fops);
	if (mac_ma32_127lr88 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr88\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr87 =
	    debugfs_create_file("mac_ma32_127lr87", 744, dir,
				&mac_ma32_127lr87_val, &mac_ma32_127lr87_fops);
	if (mac_ma32_127lr87 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr87\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr86 =
	    debugfs_create_file("mac_ma32_127lr86", 744, dir,
				&mac_ma32_127lr86_val, &mac_ma32_127lr86_fops);
	if (mac_ma32_127lr86 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr86\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr85 =
	    debugfs_create_file("mac_ma32_127lr85", 744, dir,
				&mac_ma32_127lr85_val, &mac_ma32_127lr85_fops);
	if (mac_ma32_127lr85 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr85\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr84 =
	    debugfs_create_file("mac_ma32_127lr84", 744, dir,
				&mac_ma32_127lr84_val, &mac_ma32_127lr84_fops);
	if (mac_ma32_127lr84 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr84\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr83 =
	    debugfs_create_file("mac_ma32_127lr83", 744, dir,
				&mac_ma32_127lr83_val, &mac_ma32_127lr83_fops);
	if (mac_ma32_127lr83 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr83\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr82 =
	    debugfs_create_file("mac_ma32_127lr82", 744, dir,
				&mac_ma32_127lr82_val, &mac_ma32_127lr82_fops);
	if (mac_ma32_127lr82 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr82\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr81 =
	    debugfs_create_file("mac_ma32_127lr81", 744, dir,
				&mac_ma32_127lr81_val, &mac_ma32_127lr81_fops);
	if (mac_ma32_127lr81 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr81\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr80 =
	    debugfs_create_file("mac_ma32_127lr80", 744, dir,
				&mac_ma32_127lr80_val, &mac_ma32_127lr80_fops);
	if (mac_ma32_127lr80 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr80\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr79 =
	    debugfs_create_file("mac_ma32_127lr79", 744, dir,
				&mac_ma32_127lr79_val, &mac_ma32_127lr79_fops);
	if (mac_ma32_127lr79 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr79\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr78 =
	    debugfs_create_file("mac_ma32_127lr78", 744, dir,
				&mac_ma32_127lr78_val, &mac_ma32_127lr78_fops);
	if (mac_ma32_127lr78 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr78\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr77 =
	    debugfs_create_file("mac_ma32_127lr77", 744, dir,
				&mac_ma32_127lr77_val, &mac_ma32_127lr77_fops);
	if (mac_ma32_127lr77 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr77\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr76 =
	    debugfs_create_file("mac_ma32_127lr76", 744, dir,
				&mac_ma32_127lr76_val, &mac_ma32_127lr76_fops);
	if (mac_ma32_127lr76 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr76\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr75 =
	    debugfs_create_file("mac_ma32_127lr75", 744, dir,
				&mac_ma32_127lr75_val, &mac_ma32_127lr75_fops);
	if (mac_ma32_127lr75 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr75\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr74 =
	    debugfs_create_file("mac_ma32_127lr74", 744, dir,
				&mac_ma32_127lr74_val, &mac_ma32_127lr74_fops);
	if (mac_ma32_127lr74 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr74\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr73 =
	    debugfs_create_file("mac_ma32_127lr73", 744, dir,
				&mac_ma32_127lr73_val, &mac_ma32_127lr73_fops);
	if (mac_ma32_127lr73 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr73\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr72 =
	    debugfs_create_file("mac_ma32_127lr72", 744, dir,
				&mac_ma32_127lr72_val, &mac_ma32_127lr72_fops);
	if (mac_ma32_127lr72 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr72\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr71 =
	    debugfs_create_file("mac_ma32_127lr71", 744, dir,
				&mac_ma32_127lr71_val, &mac_ma32_127lr71_fops);
	if (mac_ma32_127lr71 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr71\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr70 =
	    debugfs_create_file("mac_ma32_127lr70", 744, dir,
				&mac_ma32_127lr70_val, &mac_ma32_127lr70_fops);
	if (mac_ma32_127lr70 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr70\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr69 =
	    debugfs_create_file("mac_ma32_127lr69", 744, dir,
				&mac_ma32_127lr69_val, &mac_ma32_127lr69_fops);
	if (mac_ma32_127lr69 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr69\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr68 =
	    debugfs_create_file("mac_ma32_127lr68", 744, dir,
				&mac_ma32_127lr68_val, &mac_ma32_127lr68_fops);
	if (mac_ma32_127lr68 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr68\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr67 =
	    debugfs_create_file("mac_ma32_127lr67", 744, dir,
				&mac_ma32_127lr67_val, &mac_ma32_127lr67_fops);
	if (mac_ma32_127lr67 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr67\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr66 =
	    debugfs_create_file("mac_ma32_127lr66", 744, dir,
				&mac_ma32_127lr66_val, &mac_ma32_127lr66_fops);
	if (mac_ma32_127lr66 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr66\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr65 =
	    debugfs_create_file("mac_ma32_127lr65", 744, dir,
				&mac_ma32_127lr65_val, &mac_ma32_127lr65_fops);
	if (mac_ma32_127lr65 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr65\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr64 =
	    debugfs_create_file("mac_ma32_127lr64", 744, dir,
				&mac_ma32_127lr64_val, &mac_ma32_127lr64_fops);
	if (mac_ma32_127lr64 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr64\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr63 =
	    debugfs_create_file("mac_ma32_127lr63", 744, dir,
				&mac_ma32_127lr63_val, &mac_ma32_127lr63_fops);
	if (mac_ma32_127lr63 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr63\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr62 =
	    debugfs_create_file("mac_ma32_127lr62", 744, dir,
				&mac_ma32_127lr62_val, &mac_ma32_127lr62_fops);
	if (mac_ma32_127lr62 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr62\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr61 =
	    debugfs_create_file("mac_ma32_127lr61", 744, dir,
				&mac_ma32_127lr61_val, &mac_ma32_127lr61_fops);
	if (mac_ma32_127lr61 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr61\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr60 =
	    debugfs_create_file("mac_ma32_127lr60", 744, dir,
				&mac_ma32_127lr60_val, &mac_ma32_127lr60_fops);
	if (mac_ma32_127lr60 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr60\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr59 =
	    debugfs_create_file("mac_ma32_127lr59", 744, dir,
				&mac_ma32_127lr59_val, &mac_ma32_127lr59_fops);
	if (mac_ma32_127lr59 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr59\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr58 =
	    debugfs_create_file("mac_ma32_127lr58", 744, dir,
				&mac_ma32_127lr58_val, &mac_ma32_127lr58_fops);
	if (mac_ma32_127lr58 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr58\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr57 =
	    debugfs_create_file("mac_ma32_127lr57", 744, dir,
				&mac_ma32_127lr57_val, &mac_ma32_127lr57_fops);
	if (mac_ma32_127lr57 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr57\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr56 =
	    debugfs_create_file("mac_ma32_127lr56", 744, dir,
				&mac_ma32_127lr56_val, &mac_ma32_127lr56_fops);
	if (mac_ma32_127lr56 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr56\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr55 =
	    debugfs_create_file("mac_ma32_127lr55", 744, dir,
				&mac_ma32_127lr55_val, &mac_ma32_127lr55_fops);
	if (mac_ma32_127lr55 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr55\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr54 =
	    debugfs_create_file("mac_ma32_127lr54", 744, dir,
				&mac_ma32_127lr54_val, &mac_ma32_127lr54_fops);
	if (mac_ma32_127lr54 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr54\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr53 =
	    debugfs_create_file("mac_ma32_127lr53", 744, dir,
				&mac_ma32_127lr53_val, &mac_ma32_127lr53_fops);
	if (mac_ma32_127lr53 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr53\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr52 =
	    debugfs_create_file("mac_ma32_127lr52", 744, dir,
				&mac_ma32_127lr52_val, &mac_ma32_127lr52_fops);
	if (mac_ma32_127lr52 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr52\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr51 =
	    debugfs_create_file("mac_ma32_127lr51", 744, dir,
				&mac_ma32_127lr51_val, &mac_ma32_127lr51_fops);
	if (mac_ma32_127lr51 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr51\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr50 =
	    debugfs_create_file("mac_ma32_127lr50", 744, dir,
				&mac_ma32_127lr50_val, &mac_ma32_127lr50_fops);
	if (mac_ma32_127lr50 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr50\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr49 =
	    debugfs_create_file("mac_ma32_127lr49", 744, dir,
				&mac_ma32_127lr49_val, &mac_ma32_127lr49_fops);
	if (mac_ma32_127lr49 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr49\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr48 =
	    debugfs_create_file("mac_ma32_127lr48", 744, dir,
				&mac_ma32_127lr48_val, &mac_ma32_127lr48_fops);
	if (mac_ma32_127lr48 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr48\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr47 =
	    debugfs_create_file("mac_ma32_127lr47", 744, dir,
				&mac_ma32_127lr47_val, &mac_ma32_127lr47_fops);
	if (mac_ma32_127lr47 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr47\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr46 =
	    debugfs_create_file("mac_ma32_127lr46", 744, dir,
				&mac_ma32_127lr46_val, &mac_ma32_127lr46_fops);
	if (mac_ma32_127lr46 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr46\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr45 =
	    debugfs_create_file("mac_ma32_127lr45", 744, dir,
				&mac_ma32_127lr45_val, &mac_ma32_127lr45_fops);
	if (mac_ma32_127lr45 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr45\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr44 =
	    debugfs_create_file("mac_ma32_127lr44", 744, dir,
				&mac_ma32_127lr44_val, &mac_ma32_127lr44_fops);
	if (mac_ma32_127lr44 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr44\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr43 =
	    debugfs_create_file("mac_ma32_127lr43", 744, dir,
				&mac_ma32_127lr43_val, &mac_ma32_127lr43_fops);
	if (mac_ma32_127lr43 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr43\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr42 =
	    debugfs_create_file("mac_ma32_127lr42", 744, dir,
				&mac_ma32_127lr42_val, &mac_ma32_127lr42_fops);
	if (mac_ma32_127lr42 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr42\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr41 =
	    debugfs_create_file("mac_ma32_127lr41", 744, dir,
				&mac_ma32_127lr41_val, &mac_ma32_127lr41_fops);
	if (mac_ma32_127lr41 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr41\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr40 =
	    debugfs_create_file("mac_ma32_127lr40", 744, dir,
				&mac_ma32_127lr40_val, &mac_ma32_127lr40_fops);
	if (mac_ma32_127lr40 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr40\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr39 =
	    debugfs_create_file("mac_ma32_127lr39", 744, dir,
				&mac_ma32_127lr39_val, &mac_ma32_127lr39_fops);
	if (mac_ma32_127lr39 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr39\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr38 =
	    debugfs_create_file("mac_ma32_127lr38", 744, dir,
				&mac_ma32_127lr38_val, &mac_ma32_127lr38_fops);
	if (mac_ma32_127lr38 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr38\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr37 =
	    debugfs_create_file("mac_ma32_127lr37", 744, dir,
				&mac_ma32_127lr37_val, &mac_ma32_127lr37_fops);
	if (mac_ma32_127lr37 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr37\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr36 =
	    debugfs_create_file("mac_ma32_127lr36", 744, dir,
				&mac_ma32_127lr36_val, &mac_ma32_127lr36_fops);
	if (mac_ma32_127lr36 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr36\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr35 =
	    debugfs_create_file("mac_ma32_127lr35", 744, dir,
				&mac_ma32_127lr35_val, &mac_ma32_127lr35_fops);
	if (mac_ma32_127lr35 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr35\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr34 =
	    debugfs_create_file("mac_ma32_127lr34", 744, dir,
				&mac_ma32_127lr34_val, &mac_ma32_127lr34_fops);
	if (mac_ma32_127lr34 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr34\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr33 =
	    debugfs_create_file("mac_ma32_127lr33", 744, dir,
				&mac_ma32_127lr33_val, &mac_ma32_127lr33_fops);
	if (mac_ma32_127lr33 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr33\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127lr32 =
	    debugfs_create_file("mac_ma32_127lr32", 744, dir,
				&mac_ma32_127lr32_val, &mac_ma32_127lr32_fops);
	if (mac_ma32_127lr32 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127lr32\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr127 =
	    debugfs_create_file("mac_ma32_127hr127", 744, dir,
				&mac_ma32_127hr127_val,
				&mac_ma32_127hr127_fops);
	if (mac_ma32_127hr127 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr127\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr126 =
	    debugfs_create_file("mac_ma32_127hr126", 744, dir,
				&mac_ma32_127hr126_val,
				&mac_ma32_127hr126_fops);
	if (mac_ma32_127hr126 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr126\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr125 =
	    debugfs_create_file("mac_ma32_127hr125", 744, dir,
				&mac_ma32_127hr125_val,
				&mac_ma32_127hr125_fops);
	if (mac_ma32_127hr125 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr125\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr124 =
	    debugfs_create_file("mac_ma32_127hr124", 744, dir,
				&mac_ma32_127hr124_val,
				&mac_ma32_127hr124_fops);
	if (mac_ma32_127hr124 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr124\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr123 =
	    debugfs_create_file("mac_ma32_127hr123", 744, dir,
				&mac_ma32_127hr123_val,
				&mac_ma32_127hr123_fops);
	if (mac_ma32_127hr123 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr123\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr122 =
	    debugfs_create_file("mac_ma32_127hr122", 744, dir,
				&mac_ma32_127hr122_val,
				&mac_ma32_127hr122_fops);
	if (mac_ma32_127hr122 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr122\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr121 =
	    debugfs_create_file("mac_ma32_127hr121", 744, dir,
				&mac_ma32_127hr121_val,
				&mac_ma32_127hr121_fops);
	if (mac_ma32_127hr121 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr121\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr120 =
	    debugfs_create_file("mac_ma32_127hr120", 744, dir,
				&mac_ma32_127hr120_val,
				&mac_ma32_127hr120_fops);
	if (mac_ma32_127hr120 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr120\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr119 =
	    debugfs_create_file("mac_ma32_127hr119", 744, dir,
				&mac_ma32_127hr119_val,
				&mac_ma32_127hr119_fops);
	if (mac_ma32_127hr119 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr119\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr118 =
	    debugfs_create_file("mac_ma32_127hr118", 744, dir,
				&mac_ma32_127hr118_val,
				&mac_ma32_127hr118_fops);
	if (mac_ma32_127hr118 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr118\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr117 =
	    debugfs_create_file("mac_ma32_127hr117", 744, dir,
				&mac_ma32_127hr117_val,
				&mac_ma32_127hr117_fops);
	if (mac_ma32_127hr117 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr117\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr116 =
	    debugfs_create_file("mac_ma32_127hr116", 744, dir,
				&mac_ma32_127hr116_val,
				&mac_ma32_127hr116_fops);
	if (mac_ma32_127hr116 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr116\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr115 =
	    debugfs_create_file("mac_ma32_127hr115", 744, dir,
				&mac_ma32_127hr115_val,
				&mac_ma32_127hr115_fops);
	if (mac_ma32_127hr115 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr115\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr114 =
	    debugfs_create_file("mac_ma32_127hr114", 744, dir,
				&mac_ma32_127hr114_val,
				&mac_ma32_127hr114_fops);
	if (mac_ma32_127hr114 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr114\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr113 =
	    debugfs_create_file("mac_ma32_127hr113", 744, dir,
				&mac_ma32_127hr113_val,
				&mac_ma32_127hr113_fops);
	if (mac_ma32_127hr113 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr113\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr112 =
	    debugfs_create_file("mac_ma32_127hr112", 744, dir,
				&mac_ma32_127hr112_val,
				&mac_ma32_127hr112_fops);
	if (mac_ma32_127hr112 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr112\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr111 =
	    debugfs_create_file("mac_ma32_127hr111", 744, dir,
				&mac_ma32_127hr111_val,
				&mac_ma32_127hr111_fops);
	if (mac_ma32_127hr111 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr111\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr110 =
	    debugfs_create_file("mac_ma32_127hr110", 744, dir,
				&mac_ma32_127hr110_val,
				&mac_ma32_127hr110_fops);
	if (mac_ma32_127hr110 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr110\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr109 =
	    debugfs_create_file("mac_ma32_127hr109", 744, dir,
				&mac_ma32_127hr109_val,
				&mac_ma32_127hr109_fops);
	if (mac_ma32_127hr109 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr109\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr108 =
	    debugfs_create_file("mac_ma32_127hr108", 744, dir,
				&mac_ma32_127hr108_val,
				&mac_ma32_127hr108_fops);
	if (mac_ma32_127hr108 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr108\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr107 =
	    debugfs_create_file("mac_ma32_127hr107", 744, dir,
				&mac_ma32_127hr107_val,
				&mac_ma32_127hr107_fops);
	if (mac_ma32_127hr107 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr107\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr106 =
	    debugfs_create_file("mac_ma32_127hr106", 744, dir,
				&mac_ma32_127hr106_val,
				&mac_ma32_127hr106_fops);
	if (mac_ma32_127hr106 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr106\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr105 =
	    debugfs_create_file("mac_ma32_127hr105", 744, dir,
				&mac_ma32_127hr105_val,
				&mac_ma32_127hr105_fops);
	if (mac_ma32_127hr105 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr105\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr104 =
	    debugfs_create_file("mac_ma32_127hr104", 744, dir,
				&mac_ma32_127hr104_val,
				&mac_ma32_127hr104_fops);
	if (mac_ma32_127hr104 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr104\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr103 =
	    debugfs_create_file("mac_ma32_127hr103", 744, dir,
				&mac_ma32_127hr103_val,
				&mac_ma32_127hr103_fops);
	if (mac_ma32_127hr103 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr103\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr102 =
	    debugfs_create_file("mac_ma32_127hr102", 744, dir,
				&mac_ma32_127hr102_val,
				&mac_ma32_127hr102_fops);
	if (mac_ma32_127hr102 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr102\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr101 =
	    debugfs_create_file("mac_ma32_127hr101", 744, dir,
				&mac_ma32_127hr101_val,
				&mac_ma32_127hr101_fops);
	if (mac_ma32_127hr101 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr101\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr100 =
	    debugfs_create_file("mac_ma32_127hr100", 744, dir,
				&mac_ma32_127hr100_val,
				&mac_ma32_127hr100_fops);
	if (mac_ma32_127hr100 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr100\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr99 =
	    debugfs_create_file("mac_ma32_127hr99", 744, dir,
				&mac_ma32_127hr99_val, &mac_ma32_127hr99_fops);
	if (mac_ma32_127hr99 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr99\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr98 =
	    debugfs_create_file("mac_ma32_127hr98", 744, dir,
				&mac_ma32_127hr98_val, &mac_ma32_127hr98_fops);
	if (mac_ma32_127hr98 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr98\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr97 =
	    debugfs_create_file("mac_ma32_127hr97", 744, dir,
				&mac_ma32_127hr97_val, &mac_ma32_127hr97_fops);
	if (mac_ma32_127hr97 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr97\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr96 =
	    debugfs_create_file("mac_ma32_127hr96", 744, dir,
				&mac_ma32_127hr96_val, &mac_ma32_127hr96_fops);
	if (mac_ma32_127hr96 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr96\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr95 =
	    debugfs_create_file("mac_ma32_127hr95", 744, dir,
				&mac_ma32_127hr95_val, &mac_ma32_127hr95_fops);
	if (mac_ma32_127hr95 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr95\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr94 =
	    debugfs_create_file("mac_ma32_127hr94", 744, dir,
				&mac_ma32_127hr94_val, &mac_ma32_127hr94_fops);
	if (mac_ma32_127hr94 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr94\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr93 =
	    debugfs_create_file("mac_ma32_127hr93", 744, dir,
				&mac_ma32_127hr93_val, &mac_ma32_127hr93_fops);
	if (mac_ma32_127hr93 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr93\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr92 =
	    debugfs_create_file("mac_ma32_127hr92", 744, dir,
				&mac_ma32_127hr92_val, &mac_ma32_127hr92_fops);
	if (mac_ma32_127hr92 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr92\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr91 =
	    debugfs_create_file("mac_ma32_127hr91", 744, dir,
				&mac_ma32_127hr91_val, &mac_ma32_127hr91_fops);
	if (mac_ma32_127hr91 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr91\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr90 =
	    debugfs_create_file("mac_ma32_127hr90", 744, dir,
				&mac_ma32_127hr90_val, &mac_ma32_127hr90_fops);
	if (mac_ma32_127hr90 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr90\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr89 =
	    debugfs_create_file("mac_ma32_127hr89", 744, dir,
				&mac_ma32_127hr89_val, &mac_ma32_127hr89_fops);
	if (mac_ma32_127hr89 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr89\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr88 =
	    debugfs_create_file("mac_ma32_127hr88", 744, dir,
				&mac_ma32_127hr88_val, &mac_ma32_127hr88_fops);
	if (mac_ma32_127hr88 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr88\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr87 =
	    debugfs_create_file("mac_ma32_127hr87", 744, dir,
				&mac_ma32_127hr87_val, &mac_ma32_127hr87_fops);
	if (mac_ma32_127hr87 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr87\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr86 =
	    debugfs_create_file("mac_ma32_127hr86", 744, dir,
				&mac_ma32_127hr86_val, &mac_ma32_127hr86_fops);
	if (mac_ma32_127hr86 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr86\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr85 =
	    debugfs_create_file("mac_ma32_127hr85", 744, dir,
				&mac_ma32_127hr85_val, &mac_ma32_127hr85_fops);
	if (mac_ma32_127hr85 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr85\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr84 =
	    debugfs_create_file("mac_ma32_127hr84", 744, dir,
				&mac_ma32_127hr84_val, &mac_ma32_127hr84_fops);
	if (mac_ma32_127hr84 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr84\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr83 =
	    debugfs_create_file("mac_ma32_127hr83", 744, dir,
				&mac_ma32_127hr83_val, &mac_ma32_127hr83_fops);
	if (mac_ma32_127hr83 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr83\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr82 =
	    debugfs_create_file("mac_ma32_127hr82", 744, dir,
				&mac_ma32_127hr82_val, &mac_ma32_127hr82_fops);
	if (mac_ma32_127hr82 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr82\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr81 =
	    debugfs_create_file("mac_ma32_127hr81", 744, dir,
				&mac_ma32_127hr81_val, &mac_ma32_127hr81_fops);
	if (mac_ma32_127hr81 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr81\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr80 =
	    debugfs_create_file("mac_ma32_127hr80", 744, dir,
				&mac_ma32_127hr80_val, &mac_ma32_127hr80_fops);
	if (mac_ma32_127hr80 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr80\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr79 =
	    debugfs_create_file("mac_ma32_127hr79", 744, dir,
				&mac_ma32_127hr79_val, &mac_ma32_127hr79_fops);
	if (mac_ma32_127hr79 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr79\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr78 =
	    debugfs_create_file("mac_ma32_127hr78", 744, dir,
				&mac_ma32_127hr78_val, &mac_ma32_127hr78_fops);
	if (mac_ma32_127hr78 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr78\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr77 =
	    debugfs_create_file("mac_ma32_127hr77", 744, dir,
				&mac_ma32_127hr77_val, &mac_ma32_127hr77_fops);
	if (mac_ma32_127hr77 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr77\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr76 =
	    debugfs_create_file("mac_ma32_127hr76", 744, dir,
				&mac_ma32_127hr76_val, &mac_ma32_127hr76_fops);
	if (mac_ma32_127hr76 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr76\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr75 =
	    debugfs_create_file("mac_ma32_127hr75", 744, dir,
				&mac_ma32_127hr75_val, &mac_ma32_127hr75_fops);
	if (mac_ma32_127hr75 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr75\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr74 =
	    debugfs_create_file("mac_ma32_127hr74", 744, dir,
				&mac_ma32_127hr74_val, &mac_ma32_127hr74_fops);
	if (mac_ma32_127hr74 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr74\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr73 =
	    debugfs_create_file("mac_ma32_127hr73", 744, dir,
				&mac_ma32_127hr73_val, &mac_ma32_127hr73_fops);
	if (mac_ma32_127hr73 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr73\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr72 =
	    debugfs_create_file("mac_ma32_127hr72", 744, dir,
				&mac_ma32_127hr72_val, &mac_ma32_127hr72_fops);
	if (mac_ma32_127hr72 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr72\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr71 =
	    debugfs_create_file("mac_ma32_127hr71", 744, dir,
				&mac_ma32_127hr71_val, &mac_ma32_127hr71_fops);
	if (mac_ma32_127hr71 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr71\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr70 =
	    debugfs_create_file("mac_ma32_127hr70", 744, dir,
				&mac_ma32_127hr70_val, &mac_ma32_127hr70_fops);
	if (mac_ma32_127hr70 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr70\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr69 =
	    debugfs_create_file("mac_ma32_127hr69", 744, dir,
				&mac_ma32_127hr69_val, &mac_ma32_127hr69_fops);
	if (mac_ma32_127hr69 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr69\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr68 =
	    debugfs_create_file("mac_ma32_127hr68", 744, dir,
				&mac_ma32_127hr68_val, &mac_ma32_127hr68_fops);
	if (mac_ma32_127hr68 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr68\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr67 =
	    debugfs_create_file("mac_ma32_127hr67", 744, dir,
				&mac_ma32_127hr67_val, &mac_ma32_127hr67_fops);
	if (mac_ma32_127hr67 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr67\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr66 =
	    debugfs_create_file("mac_ma32_127hr66", 744, dir,
				&mac_ma32_127hr66_val, &mac_ma32_127hr66_fops);
	if (mac_ma32_127hr66 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr66\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr65 =
	    debugfs_create_file("mac_ma32_127hr65", 744, dir,
				&mac_ma32_127hr65_val, &mac_ma32_127hr65_fops);
	if (mac_ma32_127hr65 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr65\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr64 =
	    debugfs_create_file("mac_ma32_127hr64", 744, dir,
				&mac_ma32_127hr64_val, &mac_ma32_127hr64_fops);
	if (mac_ma32_127hr64 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr64\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr63 =
	    debugfs_create_file("mac_ma32_127hr63", 744, dir,
				&mac_ma32_127hr63_val, &mac_ma32_127hr63_fops);
	if (mac_ma32_127hr63 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr63\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr62 =
	    debugfs_create_file("mac_ma32_127hr62", 744, dir,
				&mac_ma32_127hr62_val, &mac_ma32_127hr62_fops);
	if (mac_ma32_127hr62 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr62\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr61 =
	    debugfs_create_file("mac_ma32_127hr61", 744, dir,
				&mac_ma32_127hr61_val, &mac_ma32_127hr61_fops);
	if (mac_ma32_127hr61 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr61\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr60 =
	    debugfs_create_file("mac_ma32_127hr60", 744, dir,
				&mac_ma32_127hr60_val, &mac_ma32_127hr60_fops);
	if (mac_ma32_127hr60 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr60\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr59 =
	    debugfs_create_file("mac_ma32_127hr59", 744, dir,
				&mac_ma32_127hr59_val, &mac_ma32_127hr59_fops);
	if (mac_ma32_127hr59 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr59\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr58 =
	    debugfs_create_file("mac_ma32_127hr58", 744, dir,
				&mac_ma32_127hr58_val, &mac_ma32_127hr58_fops);
	if (mac_ma32_127hr58 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr58\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr57 =
	    debugfs_create_file("mac_ma32_127hr57", 744, dir,
				&mac_ma32_127hr57_val, &mac_ma32_127hr57_fops);
	if (mac_ma32_127hr57 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr57\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr56 =
	    debugfs_create_file("mac_ma32_127hr56", 744, dir,
				&mac_ma32_127hr56_val, &mac_ma32_127hr56_fops);
	if (mac_ma32_127hr56 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr56\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr55 =
	    debugfs_create_file("mac_ma32_127hr55", 744, dir,
				&mac_ma32_127hr55_val, &mac_ma32_127hr55_fops);
	if (mac_ma32_127hr55 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr55\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr54 =
	    debugfs_create_file("mac_ma32_127hr54", 744, dir,
				&mac_ma32_127hr54_val, &mac_ma32_127hr54_fops);
	if (mac_ma32_127hr54 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr54\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr53 =
	    debugfs_create_file("mac_ma32_127hr53", 744, dir,
				&mac_ma32_127hr53_val, &mac_ma32_127hr53_fops);
	if (mac_ma32_127hr53 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr53\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr52 =
	    debugfs_create_file("mac_ma32_127hr52", 744, dir,
				&mac_ma32_127hr52_val, &mac_ma32_127hr52_fops);
	if (mac_ma32_127hr52 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr52\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr51 =
	    debugfs_create_file("mac_ma32_127hr51", 744, dir,
				&mac_ma32_127hr51_val, &mac_ma32_127hr51_fops);
	if (mac_ma32_127hr51 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr51\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr50 =
	    debugfs_create_file("mac_ma32_127hr50", 744, dir,
				&mac_ma32_127hr50_val, &mac_ma32_127hr50_fops);
	if (mac_ma32_127hr50 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr50\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr49 =
	    debugfs_create_file("mac_ma32_127hr49", 744, dir,
				&mac_ma32_127hr49_val, &mac_ma32_127hr49_fops);
	if (mac_ma32_127hr49 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr49\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr48 =
	    debugfs_create_file("mac_ma32_127hr48", 744, dir,
				&mac_ma32_127hr48_val, &mac_ma32_127hr48_fops);
	if (mac_ma32_127hr48 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr48\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr47 =
	    debugfs_create_file("mac_ma32_127hr47", 744, dir,
				&mac_ma32_127hr47_val, &mac_ma32_127hr47_fops);
	if (mac_ma32_127hr47 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr47\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr46 =
	    debugfs_create_file("mac_ma32_127hr46", 744, dir,
				&mac_ma32_127hr46_val, &mac_ma32_127hr46_fops);
	if (mac_ma32_127hr46 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr46\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr45 =
	    debugfs_create_file("mac_ma32_127hr45", 744, dir,
				&mac_ma32_127hr45_val, &mac_ma32_127hr45_fops);
	if (mac_ma32_127hr45 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr45\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr44 =
	    debugfs_create_file("mac_ma32_127hr44", 744, dir,
				&mac_ma32_127hr44_val, &mac_ma32_127hr44_fops);
	if (mac_ma32_127hr44 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr44\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr43 =
	    debugfs_create_file("mac_ma32_127hr43", 744, dir,
				&mac_ma32_127hr43_val, &mac_ma32_127hr43_fops);
	if (mac_ma32_127hr43 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr43\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr42 =
	    debugfs_create_file("mac_ma32_127hr42", 744, dir,
				&mac_ma32_127hr42_val, &mac_ma32_127hr42_fops);
	if (mac_ma32_127hr42 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr42\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr41 =
	    debugfs_create_file("mac_ma32_127hr41", 744, dir,
				&mac_ma32_127hr41_val, &mac_ma32_127hr41_fops);
	if (mac_ma32_127hr41 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr41\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr40 =
	    debugfs_create_file("mac_ma32_127hr40", 744, dir,
				&mac_ma32_127hr40_val, &mac_ma32_127hr40_fops);
	if (mac_ma32_127hr40 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr40\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr39 =
	    debugfs_create_file("mac_ma32_127hr39", 744, dir,
				&mac_ma32_127hr39_val, &mac_ma32_127hr39_fops);
	if (mac_ma32_127hr39 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr39\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr38 =
	    debugfs_create_file("mac_ma32_127hr38", 744, dir,
				&mac_ma32_127hr38_val, &mac_ma32_127hr38_fops);
	if (mac_ma32_127hr38 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr38\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr37 =
	    debugfs_create_file("mac_ma32_127hr37", 744, dir,
				&mac_ma32_127hr37_val, &mac_ma32_127hr37_fops);
	if (mac_ma32_127hr37 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr37\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr36 =
	    debugfs_create_file("mac_ma32_127hr36", 744, dir,
				&mac_ma32_127hr36_val, &mac_ma32_127hr36_fops);
	if (mac_ma32_127hr36 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr36\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr35 =
	    debugfs_create_file("mac_ma32_127hr35", 744, dir,
				&mac_ma32_127hr35_val, &mac_ma32_127hr35_fops);
	if (mac_ma32_127hr35 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr35\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr34 =
	    debugfs_create_file("mac_ma32_127hr34", 744, dir,
				&mac_ma32_127hr34_val, &mac_ma32_127hr34_fops);
	if (mac_ma32_127hr34 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr34\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr33 =
	    debugfs_create_file("mac_ma32_127hr33", 744, dir,
				&mac_ma32_127hr33_val, &mac_ma32_127hr33_fops);
	if (mac_ma32_127hr33 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma32_127hr33\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma32_127hr32 =
	    debugfs_create_file("mac_ma32_127hr32", 744, dir,
				&mac_ma32_127hr32_val, &mac_ma32_127hr32_fops);
	if (mac_ma32_127hr32 == NULL) {
		printk(KERN_INFO "error creating file: MAC_MA32_127HR32\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr31 =
	    debugfs_create_file("mac_ma1_31lr31", 744, dir, &mac_ma1_31lr31_val,
				&mac_ma1_31lr31_fops);
	if (mac_ma1_31lr31 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr31\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr30 =
	    debugfs_create_file("mac_ma1_31lr30", 744, dir, &mac_ma1_31lr30_val,
				&mac_ma1_31lr30_fops);
	if (mac_ma1_31lr30 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr30\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr29 =
	    debugfs_create_file("mac_ma1_31lr29", 744, dir, &mac_ma1_31lr29_val,
				&mac_ma1_31lr29_fops);
	if (mac_ma1_31lr29 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr29\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr28 =
	    debugfs_create_file("mac_ma1_31lr28", 744, dir, &mac_ma1_31lr28_val,
				&mac_ma1_31lr28_fops);
	if (mac_ma1_31lr28 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr28\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr27 =
	    debugfs_create_file("mac_ma1_31lr27", 744, dir, &mac_ma1_31lr27_val,
				&mac_ma1_31lr27_fops);
	if (mac_ma1_31lr27 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr27\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr26 =
	    debugfs_create_file("mac_ma1_31lr26", 744, dir, &mac_ma1_31lr26_val,
				&mac_ma1_31lr26_fops);
	if (mac_ma1_31lr26 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr26\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr25 =
	    debugfs_create_file("mac_ma1_31lr25", 744, dir, &mac_ma1_31lr25_val,
				&mac_ma1_31lr25_fops);
	if (mac_ma1_31lr25 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr25\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr24 =
	    debugfs_create_file("mac_ma1_31lr24", 744, dir, &mac_ma1_31lr24_val,
				&mac_ma1_31lr24_fops);
	if (mac_ma1_31lr24 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr24\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr23 =
	    debugfs_create_file("mac_ma1_31lr23", 744, dir, &mac_ma1_31lr23_val,
				&mac_ma1_31lr23_fops);
	if (mac_ma1_31lr23 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr23\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr22 =
	    debugfs_create_file("mac_ma1_31lr22", 744, dir, &mac_ma1_31lr22_val,
				&mac_ma1_31lr22_fops);
	if (mac_ma1_31lr22 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr22\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr21 =
	    debugfs_create_file("mac_ma1_31lr21", 744, dir, &mac_ma1_31lr21_val,
				&mac_ma1_31lr21_fops);
	if (mac_ma1_31lr21 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr21\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr20 =
	    debugfs_create_file("mac_ma1_31lr20", 744, dir, &mac_ma1_31lr20_val,
				&mac_ma1_31lr20_fops);
	if (mac_ma1_31lr20 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr20\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr19 =
	    debugfs_create_file("mac_ma1_31lr19", 744, dir, &mac_ma1_31lr19_val,
				&mac_ma1_31lr19_fops);
	if (mac_ma1_31lr19 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr19\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr18 =
	    debugfs_create_file("mac_ma1_31lr18", 744, dir, &mac_ma1_31lr18_val,
				&mac_ma1_31lr18_fops);
	if (mac_ma1_31lr18 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr18\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr17 =
	    debugfs_create_file("mac_ma1_31lr17", 744, dir, &mac_ma1_31lr17_val,
				&mac_ma1_31lr17_fops);
	if (mac_ma1_31lr17 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr17\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr16 =
	    debugfs_create_file("mac_ma1_31lr16", 744, dir, &mac_ma1_31lr16_val,
				&mac_ma1_31lr16_fops);
	if (mac_ma1_31lr16 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr16\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr15 =
	    debugfs_create_file("mac_ma1_31lr15", 744, dir, &mac_ma1_31lr15_val,
				&mac_ma1_31lr15_fops);
	if (mac_ma1_31lr15 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr15\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr14 =
	    debugfs_create_file("mac_ma1_31lr14", 744, dir, &mac_ma1_31lr14_val,
				&mac_ma1_31lr14_fops);
	if (mac_ma1_31lr14 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr14\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr13 =
	    debugfs_create_file("mac_ma1_31lr13", 744, dir, &mac_ma1_31lr13_val,
				&mac_ma1_31lr13_fops);
	if (mac_ma1_31lr13 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr13\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr12 =
	    debugfs_create_file("mac_ma1_31lr12", 744, dir, &mac_ma1_31lr12_val,
				&mac_ma1_31lr12_fops);
	if (mac_ma1_31lr12 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr12\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr11 =
	    debugfs_create_file("mac_ma1_31lr11", 744, dir, &mac_ma1_31lr11_val,
				&mac_ma1_31lr11_fops);
	if (mac_ma1_31lr11 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr11\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr10 =
	    debugfs_create_file("mac_ma1_31lr10", 744, dir, &mac_ma1_31lr10_val,
				&mac_ma1_31lr10_fops);
	if (mac_ma1_31lr10 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr10\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr9 =
	    debugfs_create_file("mac_ma1_31lr9", 744, dir, &mac_ma1_31lr9_val,
				&mac_ma1_31lr9_fops);
	if (mac_ma1_31lr9 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr9\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr8 =
	    debugfs_create_file("mac_ma1_31lr8", 744, dir, &mac_ma1_31lr8_val,
				&mac_ma1_31lr8_fops);
	if (mac_ma1_31lr8 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr8\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr7 =
	    debugfs_create_file("mac_ma1_31lr7", 744, dir, &mac_ma1_31lr7_val,
				&mac_ma1_31lr7_fops);
	if (mac_ma1_31lr7 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr6 =
	    debugfs_create_file("mac_ma1_31lr6", 744, dir, &mac_ma1_31lr6_val,
				&mac_ma1_31lr6_fops);
	if (mac_ma1_31lr6 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr5 =
	    debugfs_create_file("mac_ma1_31lr5", 744, dir, &mac_ma1_31lr5_val,
				&mac_ma1_31lr5_fops);
	if (mac_ma1_31lr5 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr4 =
	    debugfs_create_file("mac_ma1_31lr4", 744, dir, &mac_ma1_31lr4_val,
				&mac_ma1_31lr4_fops);
	if (mac_ma1_31lr4 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr3 =
	    debugfs_create_file("mac_ma1_31lr3", 744, dir, &mac_ma1_31lr3_val,
				&mac_ma1_31lr3_fops);
	if (mac_ma1_31lr3 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr2 =
	    debugfs_create_file("mac_ma1_31lr2", 744, dir, &mac_ma1_31lr2_val,
				&mac_ma1_31lr2_fops);
	if (mac_ma1_31lr2 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31lr1 =
	    debugfs_create_file("mac_ma1_31lr1", 744, dir, &mac_ma1_31lr1_val,
				&mac_ma1_31lr1_fops);
	if (mac_ma1_31lr1 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31lr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr31 =
	    debugfs_create_file("mac_ma1_31hr31", 744, dir, &mac_ma1_31hr31_val,
				&mac_ma1_31hr31_fops);
	if (mac_ma1_31hr31 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr31\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr30 =
	    debugfs_create_file("mac_ma1_31hr30", 744, dir, &mac_ma1_31hr30_val,
				&mac_ma1_31hr30_fops);
	if (mac_ma1_31hr30 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr30\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr29 =
	    debugfs_create_file("mac_ma1_31hr29", 744, dir, &mac_ma1_31hr29_val,
				&mac_ma1_31hr29_fops);
	if (mac_ma1_31hr29 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr29\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr28 =
	    debugfs_create_file("mac_ma1_31hr28", 744, dir, &mac_ma1_31hr28_val,
				&mac_ma1_31hr28_fops);
	if (mac_ma1_31hr28 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr28\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr27 =
	    debugfs_create_file("mac_ma1_31hr27", 744, dir, &mac_ma1_31hr27_val,
				&mac_ma1_31hr27_fops);
	if (mac_ma1_31hr27 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr27\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr26 =
	    debugfs_create_file("mac_ma1_31hr26", 744, dir, &mac_ma1_31hr26_val,
				&mac_ma1_31hr26_fops);
	if (mac_ma1_31hr26 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr26\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr25 =
	    debugfs_create_file("mac_ma1_31hr25", 744, dir, &mac_ma1_31hr25_val,
				&mac_ma1_31hr25_fops);
	if (mac_ma1_31hr25 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr25\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr24 =
	    debugfs_create_file("mac_ma1_31hr24", 744, dir, &mac_ma1_31hr24_val,
				&mac_ma1_31hr24_fops);
	if (mac_ma1_31hr24 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr24\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr23 =
	    debugfs_create_file("mac_ma1_31hr23", 744, dir, &mac_ma1_31hr23_val,
				&mac_ma1_31hr23_fops);
	if (mac_ma1_31hr23 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr23\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr22 =
	    debugfs_create_file("mac_ma1_31hr22", 744, dir, &mac_ma1_31hr22_val,
				&mac_ma1_31hr22_fops);
	if (mac_ma1_31hr22 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr22\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr21 =
	    debugfs_create_file("mac_ma1_31hr21", 744, dir, &mac_ma1_31hr21_val,
				&mac_ma1_31hr21_fops);
	if (mac_ma1_31hr21 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr21\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr20 =
	    debugfs_create_file("mac_ma1_31hr20", 744, dir, &mac_ma1_31hr20_val,
				&mac_ma1_31hr20_fops);
	if (mac_ma1_31hr20 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr20\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr19 =
	    debugfs_create_file("mac_ma1_31hr19", 744, dir, &mac_ma1_31hr19_val,
				&mac_ma1_31hr19_fops);
	if (mac_ma1_31hr19 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr19\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr18 =
	    debugfs_create_file("mac_ma1_31hr18", 744, dir, &mac_ma1_31hr18_val,
				&mac_ma1_31hr18_fops);
	if (mac_ma1_31hr18 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr18\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr17 =
	    debugfs_create_file("mac_ma1_31hr17", 744, dir, &mac_ma1_31hr17_val,
				&mac_ma1_31hr17_fops);
	if (mac_ma1_31hr17 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr17\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr16 =
	    debugfs_create_file("mac_ma1_31hr16", 744, dir, &mac_ma1_31hr16_val,
				&mac_ma1_31hr16_fops);
	if (mac_ma1_31hr16 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr16\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr15 =
	    debugfs_create_file("mac_ma1_31hr15", 744, dir, &mac_ma1_31hr15_val,
				&mac_ma1_31hr15_fops);
	if (mac_ma1_31hr15 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr15\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr14 =
	    debugfs_create_file("mac_ma1_31hr14", 744, dir, &mac_ma1_31hr14_val,
				&mac_ma1_31hr14_fops);
	if (mac_ma1_31hr14 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr14\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr13 =
	    debugfs_create_file("mac_ma1_31hr13", 744, dir, &mac_ma1_31hr13_val,
				&mac_ma1_31hr13_fops);
	if (mac_ma1_31hr13 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr13\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr12 =
	    debugfs_create_file("mac_ma1_31hr12", 744, dir, &mac_ma1_31hr12_val,
				&mac_ma1_31hr12_fops);
	if (mac_ma1_31hr12 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr12\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr11 =
	    debugfs_create_file("mac_ma1_31hr11", 744, dir, &mac_ma1_31hr11_val,
				&mac_ma1_31hr11_fops);
	if (mac_ma1_31hr11 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr11\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr10 =
	    debugfs_create_file("mac_ma1_31hr10", 744, dir, &mac_ma1_31hr10_val,
				&mac_ma1_31hr10_fops);
	if (mac_ma1_31hr10 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr10\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr9 =
	    debugfs_create_file("mac_ma1_31hr9", 744, dir, &mac_ma1_31hr9_val,
				&mac_ma1_31hr9_fops);
	if (mac_ma1_31hr9 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr9\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr8 =
	    debugfs_create_file("mac_ma1_31hr8", 744, dir, &mac_ma1_31hr8_val,
				&mac_ma1_31hr8_fops);
	if (mac_ma1_31hr8 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr8\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr7 =
	    debugfs_create_file("mac_ma1_31hr7", 744, dir, &mac_ma1_31hr7_val,
				&mac_ma1_31hr7_fops);
	if (mac_ma1_31hr7 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr6 =
	    debugfs_create_file("mac_ma1_31hr6", 744, dir, &mac_ma1_31hr6_val,
				&mac_ma1_31hr6_fops);
	if (mac_ma1_31hr6 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr5 =
	    debugfs_create_file("mac_ma1_31hr5", 744, dir, &mac_ma1_31hr5_val,
				&mac_ma1_31hr5_fops);
	if (mac_ma1_31hr5 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr4 =
	    debugfs_create_file("mac_ma1_31hr4", 744, dir, &mac_ma1_31hr4_val,
				&mac_ma1_31hr4_fops);
	if (mac_ma1_31hr4 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr3 =
	    debugfs_create_file("mac_ma1_31hr3", 744, dir, &mac_ma1_31hr3_val,
				&mac_ma1_31hr3_fops);
	if (mac_ma1_31hr3 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr2 =
	    debugfs_create_file("mac_ma1_31hr2", 744, dir, &mac_ma1_31hr2_val,
				&mac_ma1_31hr2_fops);
	if (mac_ma1_31hr2 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1_31hr1 =
	    debugfs_create_file("mac_ma1_31hr1", 744, dir, &mac_ma1_31hr1_val,
				&mac_ma1_31hr1_fops);
	if (mac_ma1_31hr1 == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1_31hr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_arpa =
	    debugfs_create_file("mac_arpa", 744, dir, &mac_arpa_val,
				&mac_arpa_fops);
	if (mac_arpa == NULL) {
		printk(KERN_INFO "error creating file: mac_arpa\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a3r7 =
	    debugfs_create_file("mac_l3a3r7", 744, dir, &mac_l3a3r7_val,
				&mac_l3a3r7_fops);
	if (mac_l3a3r7 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a3r7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a3r6 =
	    debugfs_create_file("mac_l3a3r6", 744, dir, &mac_l3a3r6_val,
				&mac_l3a3r6_fops);
	if (mac_l3a3r6 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a3r6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a3r5 =
	    debugfs_create_file("mac_l3a3r5", 744, dir, &mac_l3a3r5_val,
				&mac_l3a3r5_fops);
	if (mac_l3a3r5 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a3r5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a3r4 =
	    debugfs_create_file("mac_l3a3r4", 744, dir, &mac_l3a3r4_val,
				&mac_l3a3r4_fops);
	if (mac_l3a3r4 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a3r4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a3r3 =
	    debugfs_create_file("mac_l3a3r3", 744, dir, &mac_l3a3r3_val,
				&mac_l3a3r3_fops);
	if (mac_l3a3r3 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a3r3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a3r2 =
	    debugfs_create_file("mac_l3a3r2", 744, dir, &mac_l3a3r2_val,
				&mac_l3a3r2_fops);
	if (mac_l3a3r2 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a3r2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a3r1 =
	    debugfs_create_file("mac_l3a3r1", 744, dir, &mac_l3a3r1_val,
				&mac_l3a3r1_fops);
	if (mac_l3a3r1 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a3r1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a3r0 =
	    debugfs_create_file("mac_l3a3r0", 744, dir, &mac_l3a3r0_val,
				&mac_l3a3r0_fops);
	if (mac_l3a3r0 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a3r0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a2r7 =
	    debugfs_create_file("mac_l3a2r7", 744, dir, &mac_l3a2r7_val,
				&mac_l3a2r7_fops);
	if (mac_l3a2r7 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a2r7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a2r6 =
	    debugfs_create_file("mac_l3a2r6", 744, dir, &mac_l3a2r6_val,
				&mac_l3a2r6_fops);
	if (mac_l3a2r6 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a2r6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a2r5 =
	    debugfs_create_file("mac_l3a2r5", 744, dir, &mac_l3a2r5_val,
				&mac_l3a2r5_fops);
	if (mac_l3a2r5 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a2r5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a2r4 =
	    debugfs_create_file("mac_l3a2r4", 744, dir, &mac_l3a2r4_val,
				&mac_l3a2r4_fops);
	if (mac_l3a2r4 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a2r4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a2r3 =
	    debugfs_create_file("mac_l3a2r3", 744, dir, &mac_l3a2r3_val,
				&mac_l3a2r3_fops);
	if (mac_l3a2r3 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a2r3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a2r2 =
	    debugfs_create_file("mac_l3a2r2", 744, dir, &mac_l3a2r2_val,
				&mac_l3a2r2_fops);
	if (mac_l3a2r2 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a2r2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a2r1 =
	    debugfs_create_file("mac_l3a2r1", 744, dir, &mac_l3a2r1_val,
				&mac_l3a2r1_fops);
	if (mac_l3a2r1 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a2r1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a2r0 =
	    debugfs_create_file("mac_l3a2r0", 744, dir, &mac_l3a2r0_val,
				&mac_l3a2r0_fops);
	if (mac_l3a2r0 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a2r0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a1r7 =
	    debugfs_create_file("mac_l3a1r7", 744, dir, &mac_l3a1r7_val,
				&mac_l3a1r7_fops);
	if (mac_l3a1r7 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a1r7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a1r6 =
	    debugfs_create_file("mac_l3a1r6", 744, dir, &mac_l3a1r6_val,
				&mac_l3a1r6_fops);
	if (mac_l3a1r6 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a1r6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a1r5 =
	    debugfs_create_file("mac_l3a1r5", 744, dir, &mac_l3a1r5_val,
				&mac_l3a1r5_fops);
	if (mac_l3a1r5 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a1r5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a1r4 =
	    debugfs_create_file("mac_l3a1r4", 744, dir, &mac_l3a1r4_val,
				&mac_l3a1r4_fops);
	if (mac_l3a1r4 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a1r4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a1r3 =
	    debugfs_create_file("mac_l3a1r3", 744, dir, &mac_l3a1r3_val,
				&mac_l3a1r3_fops);
	if (mac_l3a1r3 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a1r3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a1r2 =
	    debugfs_create_file("mac_l3a1r2", 744, dir, &mac_l3a1r2_val,
				&mac_l3a1r2_fops);
	if (mac_l3a1r2 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a1r2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a1r1 =
	    debugfs_create_file("mac_l3a1r1", 744, dir, &mac_l3a1r1_val,
				&mac_l3a1r1_fops);
	if (mac_l3a1r1 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a1r1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a1r0 =
	    debugfs_create_file("mac_l3a1r0", 744, dir, &mac_l3a1r0_val,
				&mac_l3a1r0_fops);
	if (mac_l3a1r0 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a1r0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a0r7 =
	    debugfs_create_file("mac_l3a0r7", 744, dir, &mac_l3a0r7_val,
				&mac_l3a0r7_fops);
	if (mac_l3a0r7 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a0r7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a0r6 =
	    debugfs_create_file("mac_l3a0r6", 744, dir, &mac_l3a0r6_val,
				&mac_l3a0r6_fops);
	if (mac_l3a0r6 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a0r6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a0r5 =
	    debugfs_create_file("mac_l3a0r5", 744, dir, &mac_l3a0r5_val,
				&mac_l3a0r5_fops);
	if (mac_l3a0r5 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a0r5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a0r4 =
	    debugfs_create_file("mac_l3a0r4", 744, dir, &mac_l3a0r4_val,
				&mac_l3a0r4_fops);
	if (mac_l3a0r4 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a0r4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a0r3 =
	    debugfs_create_file("mac_l3a0r3", 744, dir, &mac_l3a0r3_val,
				&mac_l3a0r3_fops);
	if (mac_l3a0r3 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a0r3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a0r2 =
	    debugfs_create_file("mac_l3a0r2", 744, dir, &mac_l3a0r2_val,
				&mac_l3a0r2_fops);
	if (mac_l3a0r2 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a0r2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a0r1 =
	    debugfs_create_file("mac_l3a0r1", 744, dir, &mac_l3a0r1_val,
				&mac_l3a0r1_fops);
	if (mac_l3a0r1 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a0r1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3a0r0 =
	    debugfs_create_file("mac_l3a0r0", 744, dir, &mac_l3a0r0_val,
				&mac_l3a0r0_fops);
	if (mac_l3a0r0 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3a0r0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l4ar7 =
	    debugfs_create_file("mac_l4ar7", 744, dir, &mac_l4ar7_val,
				&mac_l4ar7_fops);
	if (mac_l4ar7 == NULL) {
		printk(KERN_INFO "error creating file: mac_l4ar7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l4ar6 =
	    debugfs_create_file("mac_l4ar6", 744, dir, &mac_l4ar6_val,
				&mac_l4ar6_fops);
	if (mac_l4ar6 == NULL) {
		printk(KERN_INFO "error creating file: mac_l4ar6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l4ar5 =
	    debugfs_create_file("mac_l4ar5", 744, dir, &mac_l4ar5_val,
				&mac_l4ar5_fops);
	if (mac_l4ar5 == NULL) {
		printk(KERN_INFO "error creating file: mac_l4ar5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l4ar4 =
	    debugfs_create_file("mac_l4ar4", 744, dir, &mac_l4ar4_val,
				&mac_l4ar4_fops);
	if (mac_l4ar4 == NULL) {
		printk(KERN_INFO "error creating file: mac_l4ar4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l4ar3 =
	    debugfs_create_file("mac_l4ar3", 744, dir, &mac_l4ar3_val,
				&mac_l4ar3_fops);
	if (mac_l4ar3 == NULL) {
		printk(KERN_INFO "error creating file: mac_l4ar3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l4ar2 =
	    debugfs_create_file("mac_l4ar2", 744, dir, &mac_l4ar2_val,
				&mac_l4ar2_fops);
	if (mac_l4ar2 == NULL) {
		printk(KERN_INFO "error creating file: mac_l4ar2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l4ar1 =
	    debugfs_create_file("mac_l4ar1", 744, dir, &mac_l4ar1_val,
				&mac_l4ar1_fops);
	if (mac_l4ar1 == NULL) {
		printk(KERN_INFO "error creating file: mac_l4ar1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l4ar0 =
	    debugfs_create_file("mac_l4ar0", 744, dir, &mac_l4ar0_val,
				&mac_l4ar0_fops);
	if (mac_l4ar0 == NULL) {
		printk(KERN_INFO "error creating file: mac_l4ar0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3l4cr7 =
	    debugfs_create_file("mac_l3l4cr7", 744, dir, &mac_l3l4cr7_val,
				&mac_l3l4cr7_fops);
	if (mac_l3l4cr7 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3l4cr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3l4cr6 =
	    debugfs_create_file("mac_l3l4cr6", 744, dir, &mac_l3l4cr6_val,
				&mac_l3l4cr6_fops);
	if (mac_l3l4cr6 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3l4cr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3l4cr5 =
	    debugfs_create_file("mac_l3l4cr5", 744, dir, &mac_l3l4cr5_val,
				&mac_l3l4cr5_fops);
	if (mac_l3l4cr5 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3l4cr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3l4cr4 =
	    debugfs_create_file("mac_l3l4cr4", 744, dir, &mac_l3l4cr4_val,
				&mac_l3l4cr4_fops);
	if (mac_l3l4cr4 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3l4cr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3l4cr3 =
	    debugfs_create_file("mac_l3l4cr3", 744, dir, &mac_l3l4cr3_val,
				&mac_l3l4cr3_fops);
	if (mac_l3l4cr3 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3l4cr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3l4cr2 =
	    debugfs_create_file("mac_l3l4cr2", 744, dir, &mac_l3l4cr2_val,
				&mac_l3l4cr2_fops);
	if (mac_l3l4cr2 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3l4cr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3l4cr1 =
	    debugfs_create_file("mac_l3l4cr1", 744, dir, &mac_l3l4cr1_val,
				&mac_l3l4cr1_fops);
	if (mac_l3l4cr1 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3l4cr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_l3l4cr0 =
	    debugfs_create_file("mac_l3l4cr0", 744, dir, &mac_l3l4cr0_val,
				&mac_l3l4cr0_fops);
	if (mac_l3l4cr0 == NULL) {
		printk(KERN_INFO "error creating file: mac_l3l4cr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_gpio =
	    debugfs_create_file("mac_gpio", 744, dir, &mac_gpio_val,
				&mac_gpio_fops);
	if (mac_gpio == NULL) {
		printk(KERN_INFO "error creating file: mac_gpio\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pcs =
	    debugfs_create_file("mac_pcs", 744, dir, &mac_pcs_val,
				&mac_pcs_fops);
	if (mac_pcs == NULL) {
		printk(KERN_INFO "error creating file: mac_pcs\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_tes =
	    debugfs_create_file("mac_tes", 744, dir, &mac_tes_val,
				&mac_tes_fops);
	if (mac_tes == NULL) {
		printk(KERN_INFO "error creating file: mac_tes\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ae =
	    debugfs_create_file("mac_ae", 744, dir, &mac_ae_val, &mac_ae_fops);
	if (mac_ae == NULL) {
		printk(KERN_INFO "error creating file: mac_ae\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_alpa =
	    debugfs_create_file("mac_alpa", 744, dir, &mac_alpa_val,
				&mac_alpa_fops);
	if (mac_alpa == NULL) {
		printk(KERN_INFO "error creating file: mac_alpa\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_aad =
	    debugfs_create_file("mac_aad", 744, dir, &mac_aad_val,
				&mac_aad_fops);
	if (mac_aad == NULL) {
		printk(KERN_INFO "error creating file: mac_aad\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ans =
	    debugfs_create_file("mac_ans", 744, dir, &mac_ans_val,
				&mac_ans_fops);
	if (mac_ans == NULL) {
		printk(KERN_INFO "error creating file: mac_ans\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_anc =
	    debugfs_create_file("mac_anc", 744, dir, &mac_anc_val,
				&mac_anc_fops);
	if (mac_anc == NULL) {
		printk(KERN_INFO "error creating file: mac_anc\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_lpc =
	    debugfs_create_file("mac_lpc", 744, dir, &mac_lpc_val,
				&mac_lpc_fops);
	if (mac_lpc == NULL) {
		printk(KERN_INFO "error creating file: mac_lpc\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_lps =
	    debugfs_create_file("mac_lps", 744, dir, &mac_lps_val,
				&mac_lps_fops);
	if (mac_lps == NULL) {
		printk(KERN_INFO "error creating file: mac_lps\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_lmir = debugfs_create_file("mac_lmir", 744, dir, &mac_lmir_val, &mac_lmir_fops);
	if(mac_lmir == NULL) {
		printk(KERN_INFO "error creating file: mac_lmir\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_spi2r = debugfs_create_file("mac_spi2r", 744, dir, &mac_spi2r_val, &mac_spi2r_fops);
	if(mac_spi2r == NULL) {
		printk(KERN_INFO "error creating file: mac_spi2r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_spi1r = debugfs_create_file("mac_spi1r", 744, dir, &mac_spi1r_val, &mac_spi1r_fops);
	if(mac_spi1r == NULL) {
		printk(KERN_INFO "error creating file: mac_spi1r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_spi0r = debugfs_create_file("mac_spi0r", 744, dir, &mac_spi0r_val, &mac_spi0r_fops);
	if(mac_spi0r == NULL) {
		printk(KERN_INFO "error creating file: mac_spi0r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pto_cr = debugfs_create_file("mac_pto_cr", 744, dir, &mac_pto_cr_val, &mac_pto_cr_fops);
	if(mac_pto_cr == NULL) {
		printk(KERN_INFO "error creating file: mac_pto_cr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_width3 =
	    debugfs_create_file("mac_pps_width3", 744, dir, &mac_pps_width3_val,
				&mac_pps_width3_fops);
	if (mac_pps_width3 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_width3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_width2 =
	    debugfs_create_file("mac_pps_width2", 744, dir, &mac_pps_width2_val,
				&mac_pps_width2_fops);
	if (mac_pps_width2 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_width2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_width1 =
	    debugfs_create_file("mac_pps_width1", 744, dir, &mac_pps_width1_val,
				&mac_pps_width1_fops);
	if (mac_pps_width1 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_width1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_width0 =
	    debugfs_create_file("mac_pps_width0", 744, dir, &mac_pps_width0_val,
				&mac_pps_width0_fops);
	if (mac_pps_width0 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_width0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_intval3 =
	    debugfs_create_file("mac_pps_intval3", 744, dir,
				&mac_pps_intval3_val, &mac_pps_intval3_fops);
	if (mac_pps_intval3 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_intval3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_intval2 =
	    debugfs_create_file("mac_pps_intval2", 744, dir,
				&mac_pps_intval2_val, &mac_pps_intval2_fops);
	if (mac_pps_intval2 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_intval2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_intval1 =
	    debugfs_create_file("mac_pps_intval1", 744, dir,
				&mac_pps_intval1_val, &mac_pps_intval1_fops);
	if (mac_pps_intval1 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_intval1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_intval0 =
	    debugfs_create_file("mac_pps_intval0", 744, dir,
				&mac_pps_intval0_val, &mac_pps_intval0_fops);
	if (mac_pps_intval0 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_intval0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_ttns3 =
	    debugfs_create_file("mac_pps_ttns3", 744, dir, &mac_pps_ttns3_val,
				&mac_pps_ttns3_fops);
	if (mac_pps_ttns3 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_ttns3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_ttns2 =
	    debugfs_create_file("mac_pps_ttns2", 744, dir, &mac_pps_ttns2_val,
				&mac_pps_ttns2_fops);
	if (mac_pps_ttns2 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_ttns2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_ttns1 =
	    debugfs_create_file("mac_pps_ttns1", 744, dir, &mac_pps_ttns1_val,
				&mac_pps_ttns1_fops);
	if (mac_pps_ttns1 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_ttns1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_ttns0 =
	    debugfs_create_file("mac_pps_ttns0", 744, dir, &mac_pps_ttns0_val,
				&mac_pps_ttns0_fops);
	if (mac_pps_ttns0 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_ttns0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_tts3 =
	    debugfs_create_file("mac_pps_tts3", 744, dir, &mac_pps_tts3_val,
				&mac_pps_tts3_fops);
	if (mac_pps_tts3 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_tts3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_tts2 =
	    debugfs_create_file("mac_pps_tts2", 744, dir, &mac_pps_tts2_val,
				&mac_pps_tts2_fops);
	if (mac_pps_tts2 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_tts2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_tts1 =
	    debugfs_create_file("mac_pps_tts1", 744, dir, &mac_pps_tts1_val,
				&mac_pps_tts1_fops);
	if (mac_pps_tts1 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_tts1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pps_tts0 =
	    debugfs_create_file("mac_pps_tts0", 744, dir, &mac_pps_tts0_val,
				&mac_pps_tts0_fops);
	if (mac_pps_tts0 == NULL) {
		printk(KERN_INFO "error creating file: mac_pps_tts0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ppsc =
	    debugfs_create_file("mac_ppsc", 744, dir, &mac_ppsc_val,
				&mac_ppsc_fops);
	if (mac_ppsc == NULL) {
		printk(KERN_INFO "error creating file: mac_ppsc\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_teac =
	    debugfs_create_file("mac_teac", 744, dir, &mac_teac_val,
				&mac_teac_fops);
	if (mac_teac == NULL) {
		printk(KERN_INFO "error creating file: mac_teac\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_tiac =
	    debugfs_create_file("mac_tiac", 744, dir, &mac_tiac_val,
				&mac_tiac_fops);
	if (mac_tiac == NULL) {
		printk(KERN_INFO "error creating file: mac_tiac\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ats =
	    debugfs_create_file("mac_ats", 744, dir, &mac_ats_val,
				&mac_ats_fops);
	if (mac_ats == NULL) {
		printk(KERN_INFO "error creating file: mac_ats\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_atn =
	    debugfs_create_file("mac_atn", 744, dir, &mac_atn_val,
				&mac_atn_fops);
	if (mac_atn == NULL) {
		printk(KERN_INFO "error creating file: mac_atn\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ac =
	    debugfs_create_file("mac_ac", 744, dir, &mac_ac_val, &mac_ac_fops);
	if (mac_ac == NULL) {
		printk(KERN_INFO "error creating file: mac_ac\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ttn =
	    debugfs_create_file("mac_ttn", 744, dir, &mac_ttn_val,
				&mac_ttn_fops);
	if (mac_ttn == NULL) {
		printk(KERN_INFO "error creating file: mac_ttn\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ttsn =
	    debugfs_create_file("mac_ttsn", 744, dir, &mac_ttsn_val,
				&mac_ttsn_fops);
	if (mac_ttsn == NULL) {
		printk(KERN_INFO "error creating file: mac_ttsn\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_tsr =
	    debugfs_create_file("mac_tsr", 744, dir, &mac_tsr_val,
				&mac_tsr_fops);
	if (mac_tsr == NULL) {
		printk(KERN_INFO "error creating file: mac_tsr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_sthwr =
	    debugfs_create_file("mac_sthwr", 744, dir, &mac_sthwr_val,
				&mac_sthwr_fops);
	if (mac_sthwr == NULL) {
		printk(KERN_INFO "error creating file: mac_sthwr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_tar =
	    debugfs_create_file("mac_tar", 744, dir, &mac_tar_val,
				&mac_tar_fops);
	if (mac_tar == NULL) {
		printk(KERN_INFO "error creating file: mac_tar\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_stnsur =
	    debugfs_create_file("mac_stnsur", 744, dir, &mac_stnsur_val,
				&mac_stnsur_fops);
	if (mac_stnsur == NULL) {
		printk(KERN_INFO "error creating file: mac_stnsur\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_stsur =
	    debugfs_create_file("mac_stsur", 744, dir, &mac_stsur_val,
				&mac_stsur_fops);
	if (mac_stsur == NULL) {
		printk(KERN_INFO "error creating file: mac_stsur\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_stnsr =
	    debugfs_create_file("mac_stnsr", 744, dir, &mac_stnsr_val,
				&mac_stnsr_fops);
	if (mac_stnsr == NULL) {
		printk(KERN_INFO "error creating file: mac_stnsr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_stsr =
	    debugfs_create_file("mac_stsr", 744, dir, &mac_stsr_val,
				&mac_stsr_fops);
	if (mac_stsr == NULL) {
		printk(KERN_INFO "error creating file: mac_stsr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ssir =
	    debugfs_create_file("mac_ssir", 744, dir, &mac_ssir_val,
				&mac_ssir_fops);
	if (mac_ssir == NULL) {
		printk(KERN_INFO "error creating file: mac_ssir\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_tcr =
	    debugfs_create_file("mac_tcr", 744, dir, &mac_tcr_val,
				&mac_tcr_fops);
	if (mac_tcr == NULL) {
		printk(KERN_INFO "error creating file: mac_tcr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_dsr =
	    debugfs_create_file("mtl_dsr", 744, dir, &mtl_dsr_val,
				&mtl_dsr_fops);
	if (mtl_dsr == NULL) {
		printk(KERN_INFO "error creating file: mtl_dsr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_rwpffr =
	    debugfs_create_file("mac_rwpffr", 744, dir, &mac_rwpffr_val,
				&mac_rwpffr_fops);
	if (mac_rwpffr == NULL) {
		printk(KERN_INFO "error creating file: mac_rwpffr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_rtsr =
	    debugfs_create_file("mac_rtsr", 744, dir, &mac_rtsr_val,
				&mac_rtsr_fops);
	if (mac_rtsr == NULL) {
		printk(KERN_INFO "error creating file: mac_rtsr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_ier =
	    debugfs_create_file("mtl_ier", 744, dir, &mtl_ier_val,
				&mtl_ier_fops);
	if (mtl_ier == NULL) {
		printk(KERN_INFO "error creating file: mtl_ier\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrcr7 =
	    debugfs_create_file("mtl_qrcr7", 744, dir, &mtl_qrcr7_val,
				&mtl_qrcr7_fops);
	if (mtl_qrcr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrcr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrcr6 =
	    debugfs_create_file("mtl_qrcr6", 744, dir, &mtl_qrcr6_val,
				&mtl_qrcr6_fops);
	if (mtl_qrcr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrcr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrcr5 =
	    debugfs_create_file("mtl_qrcr5", 744, dir, &mtl_qrcr5_val,
				&mtl_qrcr5_fops);
	if (mtl_qrcr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrcr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrcr4 =
	    debugfs_create_file("mtl_qrcr4", 744, dir, &mtl_qrcr4_val,
				&mtl_qrcr4_fops);
	if (mtl_qrcr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrcr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrcr3 =
	    debugfs_create_file("mtl_qrcr3", 744, dir, &mtl_qrcr3_val,
				&mtl_qrcr3_fops);
	if (mtl_qrcr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrcr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrcr2 =
	    debugfs_create_file("mtl_qrcr2", 744, dir, &mtl_qrcr2_val,
				&mtl_qrcr2_fops);
	if (mtl_qrcr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrcr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrcr1 =
	    debugfs_create_file("mtl_qrcr1", 744, dir, &mtl_qrcr1_val,
				&mtl_qrcr1_fops);
	if (mtl_qrcr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrcr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrdr7 =
	    debugfs_create_file("mtl_qrdr7", 744, dir, &mtl_qrdr7_val,
				&mtl_qrdr7_fops);
	if (mtl_qrdr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrdr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrdr6 =
	    debugfs_create_file("mtl_qrdr6", 744, dir, &mtl_qrdr6_val,
				&mtl_qrdr6_fops);
	if (mtl_qrdr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrdr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrdr5 =
	    debugfs_create_file("mtl_qrdr5", 744, dir, &mtl_qrdr5_val,
				&mtl_qrdr5_fops);
	if (mtl_qrdr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrdr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrdr4 =
	    debugfs_create_file("mtl_qrdr4", 744, dir, &mtl_qrdr4_val,
				&mtl_qrdr4_fops);
	if (mtl_qrdr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrdr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrdr3 =
	    debugfs_create_file("mtl_qrdr3", 744, dir, &mtl_qrdr3_val,
				&mtl_qrdr3_fops);
	if (mtl_qrdr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrdr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrdr2 =
	    debugfs_create_file("mtl_qrdr2", 744, dir, &mtl_qrdr2_val,
				&mtl_qrdr2_fops);
	if (mtl_qrdr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrdr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qrdr1 =
	    debugfs_create_file("mtl_qrdr1", 744, dir, &mtl_qrdr1_val,
				&mtl_qrdr1_fops);
	if (mtl_qrdr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qrdr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qocr7 =
	    debugfs_create_file("mtl_qocr7", 744, dir, &mtl_qocr7_val,
				&mtl_qocr7_fops);
	if (mtl_qocr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qocr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qocr6 =
	    debugfs_create_file("mtl_qocr6", 744, dir, &mtl_qocr6_val,
				&mtl_qocr6_fops);
	if (mtl_qocr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qocr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qocr5 =
	    debugfs_create_file("mtl_qocr5", 744, dir, &mtl_qocr5_val,
				&mtl_qocr5_fops);
	if (mtl_qocr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qocr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qocr4 =
	    debugfs_create_file("mtl_qocr4", 744, dir, &mtl_qocr4_val,
				&mtl_qocr4_fops);
	if (mtl_qocr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qocr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qocr3 =
	    debugfs_create_file("mtl_qocr3", 744, dir, &mtl_qocr3_val,
				&mtl_qocr3_fops);
	if (mtl_qocr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qocr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qocr2 =
	    debugfs_create_file("mtl_qocr2", 744, dir, &mtl_qocr2_val,
				&mtl_qocr2_fops);
	if (mtl_qocr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qocr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qocr1 =
	    debugfs_create_file("mtl_qocr1", 744, dir, &mtl_qocr1_val,
				&mtl_qocr1_fops);
	if (mtl_qocr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qocr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qromr7 =
	    debugfs_create_file("mtl_qromr7", 744, dir, &mtl_qromr7_val,
				&mtl_qromr7_fops);
	if (mtl_qromr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qromr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qromr6 =
	    debugfs_create_file("mtl_qromr6", 744, dir, &mtl_qromr6_val,
				&mtl_qromr6_fops);
	if (mtl_qromr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qromr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qromr5 =
	    debugfs_create_file("mtl_qromr5", 744, dir, &mtl_qromr5_val,
				&mtl_qromr5_fops);
	if (mtl_qromr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qromr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qromr4 =
	    debugfs_create_file("mtl_qromr4", 744, dir, &mtl_qromr4_val,
				&mtl_qromr4_fops);
	if (mtl_qromr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qromr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qromr3 =
	    debugfs_create_file("mtl_qromr3", 744, dir, &mtl_qromr3_val,
				&mtl_qromr3_fops);
	if (mtl_qromr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qromr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qromr2 =
	    debugfs_create_file("mtl_qromr2", 744, dir, &mtl_qromr2_val,
				&mtl_qromr2_fops);
	if (mtl_qromr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qromr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qromr1 =
	    debugfs_create_file("mtl_qromr1", 744, dir, &mtl_qromr1_val,
				&mtl_qromr1_fops);
	if (mtl_qromr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qromr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qlcr7 =
	    debugfs_create_file("mtl_qlcr7", 744, dir, &mtl_qlcr7_val,
				&mtl_qlcr7_fops);
	if (mtl_qlcr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qlcr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qlcr6 =
	    debugfs_create_file("mtl_qlcr6", 744, dir, &mtl_qlcr6_val,
				&mtl_qlcr6_fops);
	if (mtl_qlcr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qlcr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qlcr5 =
	    debugfs_create_file("mtl_qlcr5", 744, dir, &mtl_qlcr5_val,
				&mtl_qlcr5_fops);
	if (mtl_qlcr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qlcr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qlcr4 =
	    debugfs_create_file("mtl_qlcr4", 744, dir, &mtl_qlcr4_val,
				&mtl_qlcr4_fops);
	if (mtl_qlcr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qlcr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qlcr3 =
	    debugfs_create_file("mtl_qlcr3", 744, dir, &mtl_qlcr3_val,
				&mtl_qlcr3_fops);
	if (mtl_qlcr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qlcr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qlcr2 =
	    debugfs_create_file("mtl_qlcr2", 744, dir, &mtl_qlcr2_val,
				&mtl_qlcr2_fops);
	if (mtl_qlcr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qlcr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qlcr1 =
	    debugfs_create_file("mtl_qlcr1", 744, dir, &mtl_qlcr1_val,
				&mtl_qlcr1_fops);
	if (mtl_qlcr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qlcr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qhcr7 =
	    debugfs_create_file("mtl_qhcr7", 744, dir, &mtl_qhcr7_val,
				&mtl_qhcr7_fops);
	if (mtl_qhcr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qhcr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qhcr6 =
	    debugfs_create_file("mtl_qhcr6", 744, dir, &mtl_qhcr6_val,
				&mtl_qhcr6_fops);
	if (mtl_qhcr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qhcr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qhcr5 =
	    debugfs_create_file("mtl_qhcr5", 744, dir, &mtl_qhcr5_val,
				&mtl_qhcr5_fops);
	if (mtl_qhcr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qhcr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qhcr4 =
	    debugfs_create_file("mtl_qhcr4", 744, dir, &mtl_qhcr4_val,
				&mtl_qhcr4_fops);
	if (mtl_qhcr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qhcr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qhcr3 =
	    debugfs_create_file("mtl_qhcr3", 744, dir, &mtl_qhcr3_val,
				&mtl_qhcr3_fops);
	if (mtl_qhcr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qhcr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qhcr2 =
	    debugfs_create_file("mtl_qhcr2", 744, dir, &mtl_qhcr2_val,
				&mtl_qhcr2_fops);
	if (mtl_qhcr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qhcr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qhcr1 =
	    debugfs_create_file("mtl_qhcr1", 744, dir, &mtl_qhcr1_val,
				&mtl_qhcr1_fops);
	if (mtl_qhcr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qhcr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qsscr7 =
	    debugfs_create_file("mtl_qsscr7", 744, dir, &mtl_qsscr7_val,
				&mtl_qsscr7_fops);
	if (mtl_qsscr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qsscr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qsscr6 =
	    debugfs_create_file("mtl_qsscr6", 744, dir, &mtl_qsscr6_val,
				&mtl_qsscr6_fops);
	if (mtl_qsscr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qsscr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qsscr5 =
	    debugfs_create_file("mtl_qsscr5", 744, dir, &mtl_qsscr5_val,
				&mtl_qsscr5_fops);
	if (mtl_qsscr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qsscr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qsscr4 =
	    debugfs_create_file("mtl_qsscr4", 744, dir, &mtl_qsscr4_val,
				&mtl_qsscr4_fops);
	if (mtl_qsscr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qsscr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qsscr3 =
	    debugfs_create_file("mtl_qsscr3", 744, dir, &mtl_qsscr3_val,
				&mtl_qsscr3_fops);
	if (mtl_qsscr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qsscr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qsscr2 =
	    debugfs_create_file("mtl_qsscr2", 744, dir, &mtl_qsscr2_val,
				&mtl_qsscr2_fops);
	if (mtl_qsscr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qsscr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qsscr1 =
	    debugfs_create_file("mtl_qsscr1", 744, dir, &mtl_qsscr1_val,
				&mtl_qsscr1_fops);
	if (mtl_qsscr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qsscr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qw7 =
	    debugfs_create_file("mtl_qw7", 744, dir, &mtl_qw7_val,
				&mtl_qw7_fops);
	if (mtl_qw7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qw7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qw6 =
	    debugfs_create_file("mtl_qw6", 744, dir, &mtl_qw6_val,
				&mtl_qw6_fops);
	if (mtl_qw6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qw6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qw5 =
	    debugfs_create_file("mtl_qw5", 744, dir, &mtl_qw5_val,
				&mtl_qw5_fops);
	if (mtl_qw5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qw5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qw4 =
	    debugfs_create_file("mtl_qw4", 744, dir, &mtl_qw4_val,
				&mtl_qw4_fops);
	if (mtl_qw4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qw4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qw3 =
	    debugfs_create_file("mtl_qw3", 744, dir, &mtl_qw3_val,
				&mtl_qw3_fops);
	if (mtl_qw3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qw3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qw2 =
	    debugfs_create_file("mtl_qw2", 744, dir, &mtl_qw2_val,
				&mtl_qw2_fops);
	if (mtl_qw2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qw2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qw1 =
	    debugfs_create_file("mtl_qw1", 744, dir, &mtl_qw1_val,
				&mtl_qw1_fops);
	if (mtl_qw1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qw1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qesr7 =
	    debugfs_create_file("mtl_qesr7", 744, dir, &mtl_qesr7_val,
				&mtl_qesr7_fops);
	if (mtl_qesr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qesr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qesr6 =
	    debugfs_create_file("mtl_qesr6", 744, dir, &mtl_qesr6_val,
				&mtl_qesr6_fops);
	if (mtl_qesr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qesr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qesr5 =
	    debugfs_create_file("mtl_qesr5", 744, dir, &mtl_qesr5_val,
				&mtl_qesr5_fops);
	if (mtl_qesr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qesr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qesr4 =
	    debugfs_create_file("mtl_qesr4", 744, dir, &mtl_qesr4_val,
				&mtl_qesr4_fops);
	if (mtl_qesr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qesr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qesr3 =
	    debugfs_create_file("mtl_qesr3", 744, dir, &mtl_qesr3_val,
				&mtl_qesr3_fops);
	if (mtl_qesr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qesr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qesr2 =
	    debugfs_create_file("mtl_qesr2", 744, dir, &mtl_qesr2_val,
				&mtl_qesr2_fops);
	if (mtl_qesr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qesr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qesr1 =
	    debugfs_create_file("mtl_qesr1", 744, dir, &mtl_qesr1_val,
				&mtl_qesr1_fops);
	if (mtl_qesr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qesr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qecr7 =
	    debugfs_create_file("mtl_qecr7", 744, dir, &mtl_qecr7_val,
				&mtl_qecr7_fops);
	if (mtl_qecr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qecr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qecr6 =
	    debugfs_create_file("mtl_qecr6", 744, dir, &mtl_qecr6_val,
				&mtl_qecr6_fops);
	if (mtl_qecr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qecr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qecr5 =
	    debugfs_create_file("mtl_qecr5", 744, dir, &mtl_qecr5_val,
				&mtl_qecr5_fops);
	if (mtl_qecr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qecr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qecr4 =
	    debugfs_create_file("mtl_qecr4", 744, dir, &mtl_qecr4_val,
				&mtl_qecr4_fops);
	if (mtl_qecr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qecr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qecr3 =
	    debugfs_create_file("mtl_qecr3", 744, dir, &mtl_qecr3_val,
				&mtl_qecr3_fops);
	if (mtl_qecr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qecr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qecr2 =
	    debugfs_create_file("mtl_qecr2", 744, dir, &mtl_qecr2_val,
				&mtl_qecr2_fops);
	if (mtl_qecr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qecr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qecr1 =
	    debugfs_create_file("mtl_qecr1", 744, dir, &mtl_qecr1_val,
				&mtl_qecr1_fops);
	if (mtl_qecr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qecr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtdr7 =
	    debugfs_create_file("mtl_qtdr7", 744, dir, &mtl_qtdr7_val,
				&mtl_qtdr7_fops);
	if (mtl_qtdr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtdr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtdr6 =
	    debugfs_create_file("mtl_qtdr6", 744, dir, &mtl_qtdr6_val,
				&mtl_qtdr6_fops);
	if (mtl_qtdr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtdr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtdr5 =
	    debugfs_create_file("mtl_qtdr5", 744, dir, &mtl_qtdr5_val,
				&mtl_qtdr5_fops);
	if (mtl_qtdr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtdr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtdr4 =
	    debugfs_create_file("mtl_qtdr4", 744, dir, &mtl_qtdr4_val,
				&mtl_qtdr4_fops);
	if (mtl_qtdr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtdr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtdr3 =
	    debugfs_create_file("mtl_qtdr3", 744, dir, &mtl_qtdr3_val,
				&mtl_qtdr3_fops);
	if (mtl_qtdr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtdr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtdr2 =
	    debugfs_create_file("mtl_qtdr2", 744, dir, &mtl_qtdr2_val,
				&mtl_qtdr2_fops);
	if (mtl_qtdr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtdr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtdr1 =
	    debugfs_create_file("mtl_qtdr1", 744, dir, &mtl_qtdr1_val,
				&mtl_qtdr1_fops);
	if (mtl_qtdr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtdr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qucr7 =
	    debugfs_create_file("mtl_qucr7", 744, dir, &mtl_qucr7_val,
				&mtl_qucr7_fops);
	if (mtl_qucr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qucr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qucr6 =
	    debugfs_create_file("mtl_qucr6", 744, dir, &mtl_qucr6_val,
				&mtl_qucr6_fops);
	if (mtl_qucr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qucr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qucr5 =
	    debugfs_create_file("mtl_qucr5", 744, dir, &mtl_qucr5_val,
				&mtl_qucr5_fops);
	if (mtl_qucr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qucr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qucr4 =
	    debugfs_create_file("mtl_qucr4", 744, dir, &mtl_qucr4_val,
				&mtl_qucr4_fops);
	if (mtl_qucr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qucr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qucr3 =
	    debugfs_create_file("mtl_qucr3", 744, dir, &mtl_qucr3_val,
				&mtl_qucr3_fops);
	if (mtl_qucr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qucr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qucr2 =
	    debugfs_create_file("mtl_qucr2", 744, dir, &mtl_qucr2_val,
				&mtl_qucr2_fops);
	if (mtl_qucr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qucr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qucr1 =
	    debugfs_create_file("mtl_qucr1", 744, dir, &mtl_qucr1_val,
				&mtl_qucr1_fops);
	if (mtl_qucr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qucr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtomr7 =
	    debugfs_create_file("mtl_qtomr7", 744, dir, &mtl_qtomr7_val,
				&mtl_qtomr7_fops);
	if (mtl_qtomr7 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtomr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtomr6 =
	    debugfs_create_file("mtl_qtomr6", 744, dir, &mtl_qtomr6_val,
				&mtl_qtomr6_fops);
	if (mtl_qtomr6 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtomr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtomr5 =
	    debugfs_create_file("mtl_qtomr5", 744, dir, &mtl_qtomr5_val,
				&mtl_qtomr5_fops);
	if (mtl_qtomr5 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtomr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtomr4 =
	    debugfs_create_file("mtl_qtomr4", 744, dir, &mtl_qtomr4_val,
				&mtl_qtomr4_fops);
	if (mtl_qtomr4 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtomr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtomr3 =
	    debugfs_create_file("mtl_qtomr3", 744, dir, &mtl_qtomr3_val,
				&mtl_qtomr3_fops);
	if (mtl_qtomr3 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtomr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtomr2 =
	    debugfs_create_file("mtl_qtomr2", 744, dir, &mtl_qtomr2_val,
				&mtl_qtomr2_fops);
	if (mtl_qtomr2 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtomr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_qtomr1 =
	    debugfs_create_file("mtl_qtomr1", 744, dir, &mtl_qtomr1_val,
				&mtl_qtomr1_fops);
	if (mtl_qtomr1 == NULL) {
		printk(KERN_INFO "error creating file: mtl_qtomr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_pmtcsr =
	    debugfs_create_file("mac_pmtcsr", 744, dir, &mac_pmtcsr_val,
				&mac_pmtcsr_fops);
	if (mac_pmtcsr == NULL) {
		printk(KERN_INFO "error creating file: mac_pmtcsr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxicmp_err_octets =
	    debugfs_create_file("mmc_rxicmp_err_octets", 744, dir,
				&mmc_rxicmp_err_octets_val,
				&mmc_rxicmp_err_octets_fops);
	if (mmc_rxicmp_err_octets == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxicmp_err_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxicmp_gd_octets =
	    debugfs_create_file("mmc_rxicmp_gd_octets", 744, dir,
				&mmc_rxicmp_gd_octets_val,
				&mmc_rxicmp_gd_octets_fops);
	if (mmc_rxicmp_gd_octets == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxicmp_gd_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxtcp_err_octets =
	    debugfs_create_file("mmc_rxtcp_err_octets", 744, dir,
				&mmc_rxtcp_err_octets_val,
				&mmc_rxtcp_err_octets_fops);
	if (mmc_rxtcp_err_octets == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxtcp_err_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxtcp_gd_octets =
	    debugfs_create_file("mmc_rxtcp_gd_octets", 744, dir,
				&mmc_rxtcp_gd_octets_val,
				&mmc_rxtcp_gd_octets_fops);
	if (mmc_rxtcp_gd_octets == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxtcp_gd_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxudp_err_octets =
	    debugfs_create_file("mmc_rxudp_err_octets", 744, dir,
				&mmc_rxudp_err_octets_val,
				&mmc_rxudp_err_octets_fops);
	if (mmc_rxudp_err_octets == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxudp_err_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxudp_gd_octets =
	    debugfs_create_file("mmc_rxudp_gd_octets", 744, dir,
				&mmc_rxudp_gd_octets_val,
				&mmc_rxudp_gd_octets_fops);
	if (mmc_rxudp_gd_octets == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxudp_gd_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv6_nopay_octets =
	    debugfs_create_file("mmc_rxipv6_nopay_octets", 744, dir,
				&mmc_rxipv6_nopay_octets_val,
				&mmc_rxipv6_nopay_octets_fops);
	if (mmc_rxipv6_nopay_octets == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv6_nopay_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv6_hdrerr_octets =
	    debugfs_create_file("mmc_rxipv6_hdrerr_octets", 744, dir,
				&mmc_rxipv6_hdrerr_octets_val,
				&mmc_rxipv6_hdrerr_octets_fops);
	if (mmc_rxipv6_hdrerr_octets == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv6_hdrerr_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv6_gd_octets =
	    debugfs_create_file("mmc_rxipv6_gd_octets", 744, dir,
				&mmc_rxipv6_gd_octets_val,
				&mmc_rxipv6_gd_octets_fops);
	if (mmc_rxipv6_gd_octets == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxipv6_gd_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_udsbl_octets =
	    debugfs_create_file("mmc_rxipv4_udsbl_octets", 744, dir,
				&mmc_rxipv4_udsbl_octets_val,
				&mmc_rxipv4_udsbl_octets_fops);
	if (mmc_rxipv4_udsbl_octets == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv4_udsbl_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_frag_octets =
	    debugfs_create_file("mmc_rxipv4_frag_octets", 744, dir,
				&mmc_rxipv4_frag_octets_val,
				&mmc_rxipv4_frag_octets_fops);
	if (mmc_rxipv4_frag_octets == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv4_frag_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_nopay_octets =
	    debugfs_create_file("mmc_rxipv4_nopay_octets", 744, dir,
				&mmc_rxipv4_nopay_octets_val,
				&mmc_rxipv4_nopay_octets_fops);
	if (mmc_rxipv4_nopay_octets == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv4_nopay_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_hdrerr_octets =
	    debugfs_create_file("mmc_rxipv4_hdrerr_octets", 744, dir,
				&mmc_rxipv4_hdrerr_octets_val,
				&mmc_rxipv4_hdrerr_octets_fops);
	if (mmc_rxipv4_hdrerr_octets == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv4_hdrerr_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_gd_octets =
	    debugfs_create_file("mmc_rxipv4_gd_octets", 744, dir,
				&mmc_rxipv4_gd_octets_val,
				&mmc_rxipv4_gd_octets_fops);
	if (mmc_rxipv4_gd_octets == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxipv4_gd_octets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxicmp_err_pkts =
	    debugfs_create_file("mmc_rxicmp_err_pkts", 744, dir,
				&mmc_rxicmp_err_pkts_val,
				&mmc_rxicmp_err_pkts_fops);
	if (mmc_rxicmp_err_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxicmp_err_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxicmp_gd_pkts =
	    debugfs_create_file("mmc_rxicmp_gd_pkts", 744, dir,
				&mmc_rxicmp_gd_pkts_val,
				&mmc_rxicmp_gd_pkts_fops);
	if (mmc_rxicmp_gd_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxicmp_gd_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxtcp_err_pkts =
	    debugfs_create_file("mmc_rxtcp_err_pkts", 744, dir,
				&mmc_rxtcp_err_pkts_val,
				&mmc_rxtcp_err_pkts_fops);
	if (mmc_rxtcp_err_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxtcp_err_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxtcp_gd_pkts =
	    debugfs_create_file("mmc_rxtcp_gd_pkts", 744, dir,
				&mmc_rxtcp_gd_pkts_val,
				&mmc_rxtcp_gd_pkts_fops);
	if (mmc_rxtcp_gd_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxtcp_gd_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxudp_err_pkts =
	    debugfs_create_file("mmc_rxudp_err_pkts", 744, dir,
				&mmc_rxudp_err_pkts_val,
				&mmc_rxudp_err_pkts_fops);
	if (mmc_rxudp_err_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxudp_err_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxudp_gd_pkts =
	    debugfs_create_file("mmc_rxudp_gd_pkts", 744, dir,
				&mmc_rxudp_gd_pkts_val,
				&mmc_rxudp_gd_pkts_fops);
	if (mmc_rxudp_gd_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxudp_gd_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv6_nopay_pkts =
	    debugfs_create_file("mmc_rxipv6_nopay_pkts", 744, dir,
				&mmc_rxipv6_nopay_pkts_val,
				&mmc_rxipv6_nopay_pkts_fops);
	if (mmc_rxipv6_nopay_pkts == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv6_nopay_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv6_hdrerr_pkts =
	    debugfs_create_file("mmc_rxipv6_hdrerr_pkts", 744, dir,
				&mmc_rxipv6_hdrerr_pkts_val,
				&mmc_rxipv6_hdrerr_pkts_fops);
	if (mmc_rxipv6_hdrerr_pkts == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv6_hdrerr_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv6_gd_pkts =
	    debugfs_create_file("mmc_rxipv6_gd_pkts", 744, dir,
				&mmc_rxipv6_gd_pkts_val,
				&mmc_rxipv6_gd_pkts_fops);
	if (mmc_rxipv6_gd_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxipv6_gd_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_ubsbl_pkts =
	    debugfs_create_file("mmc_rxipv4_ubsbl_pkts", 744, dir,
				&mmc_rxipv4_ubsbl_pkts_val,
				&mmc_rxipv4_ubsbl_pkts_fops);
	if (mmc_rxipv4_ubsbl_pkts == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv4_ubsbl_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_frag_pkts =
	    debugfs_create_file("mmc_rxipv4_frag_pkts", 744, dir,
				&mmc_rxipv4_frag_pkts_val,
				&mmc_rxipv4_frag_pkts_fops);
	if (mmc_rxipv4_frag_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxipv4_frag_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_nopay_pkts =
	    debugfs_create_file("mmc_rxipv4_nopay_pkts", 744, dir,
				&mmc_rxipv4_nopay_pkts_val,
				&mmc_rxipv4_nopay_pkts_fops);
	if (mmc_rxipv4_nopay_pkts == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv4_nopay_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_hdrerr_pkts =
	    debugfs_create_file("mmc_rxipv4_hdrerr_pkts", 744, dir,
				&mmc_rxipv4_hdrerr_pkts_val,
				&mmc_rxipv4_hdrerr_pkts_fops);
	if (mmc_rxipv4_hdrerr_pkts == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxipv4_hdrerr_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxipv4_gd_pkts =
	    debugfs_create_file("mmc_rxipv4_gd_pkts", 744, dir,
				&mmc_rxipv4_gd_pkts_val,
				&mmc_rxipv4_gd_pkts_fops);
	if (mmc_rxipv4_gd_pkts == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxipv4_gd_pkts\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxctrlpackets_g =
	    debugfs_create_file("mmc_rxctrlpackets_g", 744, dir,
				&mmc_rxctrlpackets_g_val,
				&mmc_rxctrlpackets_g_fops);
	if (mmc_rxctrlpackets_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxctrlpackets_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxrcverror =
	    debugfs_create_file("mmc_rxrcverror", 744, dir, &mmc_rxrcverror_val,
				&mmc_rxrcverror_fops);
	if (mmc_rxrcverror == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxrcverror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxwatchdogerror =
	    debugfs_create_file("mmc_rxwatchdogerror", 744, dir,
				&mmc_rxwatchdogerror_val,
				&mmc_rxwatchdogerror_fops);
	if (mmc_rxwatchdogerror == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxwatchdogerror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxvlanpackets_gb =
	    debugfs_create_file("mmc_rxvlanpackets_gb", 744, dir,
				&mmc_rxvlanpackets_gb_val,
				&mmc_rxvlanpackets_gb_fops);
	if (mmc_rxvlanpackets_gb == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxvlanpackets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxfifooverflow =
	    debugfs_create_file("mmc_rxfifooverflow", 744, dir,
				&mmc_rxfifooverflow_val,
				&mmc_rxfifooverflow_fops);
	if (mmc_rxfifooverflow == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxfifooverflow\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxpausepackets =
	    debugfs_create_file("mmc_rxpausepackets", 744, dir,
				&mmc_rxpausepackets_val,
				&mmc_rxpausepackets_fops);
	if (mmc_rxpausepackets == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxpausepackets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxoutofrangetype =
	    debugfs_create_file("mmc_rxoutofrangetype", 744, dir,
				&mmc_rxoutofrangetype_val,
				&mmc_rxoutofrangetype_fops);
	if (mmc_rxoutofrangetype == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxoutofrangetype\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxlengtherror =
	    debugfs_create_file("mmc_rxlengtherror", 744, dir,
				&mmc_rxlengtherror_val,
				&mmc_rxlengtherror_fops);
	if (mmc_rxlengtherror == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxlengtherror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxunicastpackets_g =
	    debugfs_create_file("mmc_rxunicastpackets_g", 744, dir,
				&mmc_rxunicastpackets_g_val,
				&mmc_rxunicastpackets_g_fops);
	if (mmc_rxunicastpackets_g == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxunicastpackets_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rx1024tomaxoctets_gb =
	    debugfs_create_file("mmc_rx1024tomaxoctets_gb", 744, dir,
				&mmc_rx1024tomaxoctets_gb_val,
				&mmc_rx1024tomaxoctets_gb_fops);
	if (mmc_rx1024tomaxoctets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rx1024tomaxoctets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rx512to1023octets_gb =
	    debugfs_create_file("mmc_rx512to1023octets_gb", 744, dir,
				&mmc_rx512to1023octets_gb_val,
				&mmc_rx512to1023octets_gb_fops);
	if (mmc_rx512to1023octets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rx512to1023octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rx256to511octets_gb =
	    debugfs_create_file("mmc_rx256to511octets_gb", 744, dir,
				&mmc_rx256to511octets_gb_val,
				&mmc_rx256to511octets_gb_fops);
	if (mmc_rx256to511octets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rx256to511octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rx128to255octets_gb =
	    debugfs_create_file("mmc_rx128to255octets_gb", 744, dir,
				&mmc_rx128to255octets_gb_val,
				&mmc_rx128to255octets_gb_fops);
	if (mmc_rx128to255octets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rx128to255octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rx65to127octets_gb =
	    debugfs_create_file("mmc_rx65to127octets_gb", 744, dir,
				&mmc_rx65to127octets_gb_val,
				&mmc_rx65to127octets_gb_fops);
	if (mmc_rx65to127octets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rx65to127octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rx64octets_gb =
	    debugfs_create_file("mmc_rx64octets_gb", 744, dir,
				&mmc_rx64octets_gb_val,
				&mmc_rx64octets_gb_fops);
	if (mmc_rx64octets_gb == NULL) {
		printk(KERN_INFO "error creating file: mmc_rx64octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxoversize_g =
	    debugfs_create_file("mmc_rxoversize_g", 744, dir,
				&mmc_rxoversize_g_val, &mmc_rxoversize_g_fops);
	if (mmc_rxoversize_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxoversize_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxundersize_g =
	    debugfs_create_file("mmc_rxundersize_g", 744, dir,
				&mmc_rxundersize_g_val,
				&mmc_rxundersize_g_fops);
	if (mmc_rxundersize_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxundersize_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxjabbererror =
	    debugfs_create_file("mmc_rxjabbererror", 744, dir,
				&mmc_rxjabbererror_val,
				&mmc_rxjabbererror_fops);
	if (mmc_rxjabbererror == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxjabbererror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxrunterror =
	    debugfs_create_file("mmc_rxrunterror", 744, dir,
				&mmc_rxrunterror_val, &mmc_rxrunterror_fops);
	if (mmc_rxrunterror == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxrunterror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxalignmenterror =
	    debugfs_create_file("mmc_rxalignmenterror", 744, dir,
				&mmc_rxalignmenterror_val,
				&mmc_rxalignmenterror_fops);
	if (mmc_rxalignmenterror == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxalignmenterror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxcrcerror =
	    debugfs_create_file("mmc_rxcrcerror", 744, dir, &mmc_rxcrcerror_val,
				&mmc_rxcrcerror_fops);
	if (mmc_rxcrcerror == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxcrcerror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxmulticastpackets_g =
	    debugfs_create_file("mmc_rxmulticastpackets_g", 744, dir,
				&mmc_rxmulticastpackets_g_val,
				&mmc_rxmulticastpackets_g_fops);
	if (mmc_rxmulticastpackets_g == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxmulticastpackets_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxbroadcastpackets_g =
	    debugfs_create_file("mmc_rxbroadcastpackets_g", 744, dir,
				&mmc_rxbroadcastpackets_g_val,
				&mmc_rxbroadcastpackets_g_fops);
	if (mmc_rxbroadcastpackets_g == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_rxbroadcastpackets_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxoctetcount_g =
	    debugfs_create_file("mmc_rxoctetcount_g", 744, dir,
				&mmc_rxoctetcount_g_val,
				&mmc_rxoctetcount_g_fops);
	if (mmc_rxoctetcount_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxoctetcount_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxoctetcount_gb =
	    debugfs_create_file("mmc_rxoctetcount_gb", 744, dir,
				&mmc_rxoctetcount_gb_val,
				&mmc_rxoctetcount_gb_fops);
	if (mmc_rxoctetcount_gb == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxoctetcount_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_rxpacketcount_gb =
	    debugfs_create_file("mmc_rxpacketcount_gb", 744, dir,
				&mmc_rxpacketcount_gb_val,
				&mmc_rxpacketcount_gb_fops);
	if (mmc_rxpacketcount_gb == NULL) {
		printk(KERN_INFO "error creating file: mmc_rxpacketcount_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txoversize_g =
	    debugfs_create_file("mmc_txoversize_g", 744, dir,
				&mmc_txoversize_g_val, &mmc_txoversize_g_fops);
	if (mmc_txoversize_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_txoversize_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txvlanpackets_g =
	    debugfs_create_file("mmc_txvlanpackets_g", 744, dir,
				&mmc_txvlanpackets_g_val,
				&mmc_txvlanpackets_g_fops);
	if (mmc_txvlanpackets_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_txvlanpackets_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txpausepackets =
	    debugfs_create_file("mmc_txpausepackets", 744, dir,
				&mmc_txpausepackets_val,
				&mmc_txpausepackets_fops);
	if (mmc_txpausepackets == NULL) {
		printk(KERN_INFO "error creating file: mmc_txpausepackets\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txexcessdef =
	    debugfs_create_file("mmc_txexcessdef", 744, dir,
				&mmc_txexcessdef_val, &mmc_txexcessdef_fops);
	if (mmc_txexcessdef == NULL) {
		printk(KERN_INFO "error creating file: mmc_txexcessdef\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txpacketscount_g =
	    debugfs_create_file("mmc_txpacketscount_g", 744, dir,
				&mmc_txpacketscount_g_val,
				&mmc_txpacketscount_g_fops);
	if (mmc_txpacketscount_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_txpacketscount_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txoctetcount_g =
	    debugfs_create_file("mmc_txoctetcount_g", 744, dir,
				&mmc_txoctetcount_g_val,
				&mmc_txoctetcount_g_fops);
	if (mmc_txoctetcount_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_txoctetcount_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txcarriererror =
	    debugfs_create_file("mmc_txcarriererror", 744, dir,
				&mmc_txcarriererror_val,
				&mmc_txcarriererror_fops);
	if (mmc_txcarriererror == NULL) {
		printk(KERN_INFO "error creating file: mmc_txcarriererror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txexesscol =
	    debugfs_create_file("mmc_txexesscol", 744, dir, &mmc_txexesscol_val,
				&mmc_txexesscol_fops);
	if (mmc_txexesscol == NULL) {
		printk(KERN_INFO "error creating file: mmc_txexesscol\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txlatecol =
	    debugfs_create_file("mmc_txlatecol", 744, dir, &mmc_txlatecol_val,
				&mmc_txlatecol_fops);
	if (mmc_txlatecol == NULL) {
		printk(KERN_INFO "error creating file: mmc_txlatecol\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txdeferred =
	    debugfs_create_file("mmc_txdeferred", 744, dir, &mmc_txdeferred_val,
				&mmc_txdeferred_fops);
	if (mmc_txdeferred == NULL) {
		printk(KERN_INFO "error creating file: mmc_txdeferred\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txmulticol_g =
	    debugfs_create_file("mmc_txmulticol_g", 744, dir,
				&mmc_txmulticol_g_val, &mmc_txmulticol_g_fops);
	if (mmc_txmulticol_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_txmulticol_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txsinglecol_g =
	    debugfs_create_file("mmc_txsinglecol_g", 744, dir,
				&mmc_txsinglecol_g_val,
				&mmc_txsinglecol_g_fops);
	if (mmc_txsinglecol_g == NULL) {
		printk(KERN_INFO "error creating file: mmc_txsinglecol_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txunderflowerror =
	    debugfs_create_file("mmc_txunderflowerror", 744, dir,
				&mmc_txunderflowerror_val,
				&mmc_txunderflowerror_fops);
	if (mmc_txunderflowerror == NULL) {
		printk(KERN_INFO "error creating file: mmc_txunderflowerror\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txbroadcastpackets_gb =
	    debugfs_create_file("mmc_txbroadcastpackets_gb", 744, dir,
				&mmc_txbroadcastpackets_gb_val,
				&mmc_txbroadcastpackets_gb_fops);
	if (mmc_txbroadcastpackets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_txbroadcastpackets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txmulticastpackets_gb =
	    debugfs_create_file("mmc_txmulticastpackets_gb", 744, dir,
				&mmc_txmulticastpackets_gb_val,
				&mmc_txmulticastpackets_gb_fops);
	if (mmc_txmulticastpackets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_txmulticastpackets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txunicastpackets_gb =
	    debugfs_create_file("mmc_txunicastpackets_gb", 744, dir,
				&mmc_txunicastpackets_gb_val,
				&mmc_txunicastpackets_gb_fops);
	if (mmc_txunicastpackets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_txunicastpackets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_tx1024tomaxoctets_gb =
	    debugfs_create_file("mmc_tx1024tomaxoctets_gb", 744, dir,
				&mmc_tx1024tomaxoctets_gb_val,
				&mmc_tx1024tomaxoctets_gb_fops);
	if (mmc_tx1024tomaxoctets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_tx1024tomaxoctets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_tx512to1023octets_gb =
	    debugfs_create_file("mmc_tx512to1023octets_gb", 744, dir,
				&mmc_tx512to1023octets_gb_val,
				&mmc_tx512to1023octets_gb_fops);
	if (mmc_tx512to1023octets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_tx512to1023octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_tx256to511octets_gb =
	    debugfs_create_file("mmc_tx256to511octets_gb", 744, dir,
				&mmc_tx256to511octets_gb_val,
				&mmc_tx256to511octets_gb_fops);
	if (mmc_tx256to511octets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_tx256to511octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_tx128to255octets_gb =
	    debugfs_create_file("mmc_tx128to255octets_gb", 744, dir,
				&mmc_tx128to255octets_gb_val,
				&mmc_tx128to255octets_gb_fops);
	if (mmc_tx128to255octets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_tx128to255octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_tx65to127octets_gb =
	    debugfs_create_file("mmc_tx65to127octets_gb", 744, dir,
				&mmc_tx65to127octets_gb_val,
				&mmc_tx65to127octets_gb_fops);
	if (mmc_tx65to127octets_gb == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_tx65to127octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_tx64octets_gb =
	    debugfs_create_file("mmc_tx64octets_gb", 744, dir,
				&mmc_tx64octets_gb_val,
				&mmc_tx64octets_gb_fops);
	if (mmc_tx64octets_gb == NULL) {
		printk(KERN_INFO "error creating file: mmc_tx64octets_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txmulticastpackets_g =
	    debugfs_create_file("mmc_txmulticastpackets_g", 744, dir,
				&mmc_txmulticastpackets_g_val,
				&mmc_txmulticastpackets_g_fops);
	if (mmc_txmulticastpackets_g == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_txmulticastpackets_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txbroadcastpackets_g =
	    debugfs_create_file("mmc_txbroadcastpackets_g", 744, dir,
				&mmc_txbroadcastpackets_g_val,
				&mmc_txbroadcastpackets_g_fops);
	if (mmc_txbroadcastpackets_g == NULL) {
		printk(KERN_INFO
		       "error creating file: mmc_txbroadcastpackets_g\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txpacketcount_gb =
	    debugfs_create_file("mmc_txpacketcount_gb", 744, dir,
				&mmc_txpacketcount_gb_val,
				&mmc_txpacketcount_gb_fops);
	if (mmc_txpacketcount_gb == NULL) {
		printk(KERN_INFO "error creating file: mmc_txpacketcount_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_txoctetcount_gb =
	    debugfs_create_file("mmc_txoctetcount_gb", 744, dir,
				&mmc_txoctetcount_gb_val,
				&mmc_txoctetcount_gb_fops);
	if (mmc_txoctetcount_gb == NULL) {
		printk(KERN_INFO "error creating file: mmc_txoctetcount_gb\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_ipc_intr_rx =
	    debugfs_create_file("mmc_ipc_intr_rx", 744, dir,
				&mmc_ipc_intr_rx_val, &mmc_ipc_intr_rx_fops);
	if (mmc_ipc_intr_rx == NULL) {
		printk(KERN_INFO "error creating file: mmc_ipc_intr_rx\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_ipc_intr_mask_rx =
	    debugfs_create_file("mmc_ipc_intr_mask_rx", 744, dir,
				&mmc_ipc_intr_mask_rx_val,
				&mmc_ipc_intr_mask_rx_fops);
	if (mmc_ipc_intr_mask_rx == NULL) {
		printk(KERN_INFO "error creating file: mmc_ipc_intr_mask_rx\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_intr_mask_tx =
	    debugfs_create_file("mmc_intr_mask_tx", 744, dir,
				&mmc_intr_mask_tx_val, &mmc_intr_mask_tx_fops);
	if (mmc_intr_mask_tx == NULL) {
		printk(KERN_INFO "error creating file: mmc_intr_mask_tx\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_intr_mask_rx =
	    debugfs_create_file("mmc_intr_mask_rx", 744, dir,
				&mmc_intr_mask_rx_val, &mmc_intr_mask_rx_fops);
	if (mmc_intr_mask_rx == NULL) {
		printk(KERN_INFO "error creating file: mmc_intr_mask_rx\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_intr_tx =
	    debugfs_create_file("mmc_intr_tx", 744, dir, &mmc_intr_tx_val,
				&mmc_intr_tx_fops);
	if (mmc_intr_tx == NULL) {
		printk(KERN_INFO "error creating file: mmc_intr_tx\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_intr_rx =
	    debugfs_create_file("mmc_intr_rx", 744, dir, &mmc_intr_rx_val,
				&mmc_intr_rx_fops);
	if (mmc_intr_rx == NULL) {
		printk(KERN_INFO "error creating file: mmc_intr_rx\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mmc_cntrl =
	    debugfs_create_file("mmc_cntrl", 744, dir, &mmc_cntrl_val,
				&mmc_cntrl_fops);
	if (mmc_cntrl == NULL) {
		printk(KERN_INFO "error creating file: mmc_cntrl\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1lr =
	    debugfs_create_file("mac_ma1lr", 744, dir, &mac_ma1lr_val,
				&mac_ma1lr_fops);
	if (mac_ma1lr == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1lr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma1hr =
	    debugfs_create_file("mac_ma1hr", 744, dir, &mac_ma1hr_val,
				&mac_ma1hr_fops);
	if (mac_ma1hr == NULL) {
		printk(KERN_INFO "error creating file: mac_ma1hr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma0lr =
	    debugfs_create_file("mac_ma0lr", 744, dir, &mac_ma0lr_val,
				&mac_ma0lr_fops);
	if (mac_ma0lr == NULL) {
		printk(KERN_INFO "error creating file: mac_ma0lr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ma0hr =
	    debugfs_create_file("mac_ma0hr", 744, dir, &mac_ma0hr_val,
				&mac_ma0hr_fops);
	if (mac_ma0hr == NULL) {
		printk(KERN_INFO "error creating file: mac_ma0hr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_gpior =
	    debugfs_create_file("mac_gpior", 744, dir, &mac_gpior_val,
				&mac_gpior_fops);
	if (mac_gpior == NULL) {
		printk(KERN_INFO "error creating file: mac_gpior\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_gmiidr =
	    debugfs_create_file("mac_gmiidr", 744, dir, &mac_gmiidr_val,
				&mac_gmiidr_fops);
	if (mac_gmiidr == NULL) {
		printk(KERN_INFO "error creating file: mac_gmiidr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_gmiiar =
	    debugfs_create_file("mac_gmiiar", 744, dir, &mac_gmiiar_val,
				&mac_gmiiar_fops);
	if (mac_gmiiar == NULL) {
		printk(KERN_INFO "error creating file: mac_gmiiar\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_hfr2 =
	    debugfs_create_file("mac_hfr2", 744, dir, &mac_hfr2_val,
				&mac_hfr2_fops);
	if (mac_hfr2 == NULL) {
		printk(KERN_INFO "error creating file: mac_hfr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_hfr1 =
	    debugfs_create_file("mac_hfr1", 744, dir, &mac_hfr1_val,
				&mac_hfr1_fops);
	if (mac_hfr1 == NULL) {
		printk(KERN_INFO "error creating file: mac_hfr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_hfr0 =
	    debugfs_create_file("mac_hfr0", 744, dir, &mac_hfr0_val,
				&mac_hfr0_fops);
	if (mac_hfr0 == NULL) {
		printk(KERN_INFO "error creating file: mac_hfr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_mdr =
	    debugfs_create_file("mac_mdr", 744, dir, &mac_mdr_val,
				&mac_mdr_fops);
	if (mac_mdr == NULL) {
		printk(KERN_INFO "error creating file: mac_mdr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_vr =
	    debugfs_create_file("mac_vr", 744, dir, &mac_vr_val, &mac_vr_fops);
	if (mac_vr == NULL) {
		printk(KERN_INFO "error creating file: mac_vr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_htr7 =
	    debugfs_create_file("mac_htr7", 744, dir, &mac_htr7_val,
				&mac_htr7_fops);
	if (mac_htr7 == NULL) {
		printk(KERN_INFO "error creating file: mac_htr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_htr6 =
	    debugfs_create_file("mac_htr6", 744, dir, &mac_htr6_val,
				&mac_htr6_fops);
	if (mac_htr6 == NULL) {
		printk(KERN_INFO "error creating file: mac_htr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_htr5 =
	    debugfs_create_file("mac_htr5", 744, dir, &mac_htr5_val,
				&mac_htr5_fops);
	if (mac_htr5 == NULL) {
		printk(KERN_INFO "error creating file: mac_htr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_htr4 =
	    debugfs_create_file("mac_htr4", 744, dir, &mac_htr4_val,
				&mac_htr4_fops);
	if (mac_htr4 == NULL) {
		printk(KERN_INFO "error creating file: mac_htr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_htr3 =
	    debugfs_create_file("mac_htr3", 744, dir, &mac_htr3_val,
				&mac_htr3_fops);
	if (mac_htr3 == NULL) {
		printk(KERN_INFO "error creating file: mac_htr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_htr2 =
	    debugfs_create_file("mac_htr2", 744, dir, &mac_htr2_val,
				&mac_htr2_fops);
	if (mac_htr2 == NULL) {
		printk(KERN_INFO "error creating file: mac_htr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_htr1 =
	    debugfs_create_file("mac_htr1", 744, dir, &mac_htr1_val,
				&mac_htr1_fops);
	if (mac_htr1 == NULL) {
		printk(KERN_INFO "error creating file: mac_htr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_htr0 =
	    debugfs_create_file("mac_htr0", 744, dir, &mac_htr0_val,
				&mac_htr0_fops);
	if (mac_htr0 == NULL) {
		printk(KERN_INFO "error creating file: mac_htr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_riwtr7 =
	    debugfs_create_file("dma_riwtr7", 744, dir, &dma_riwtr7_val,
				&dma_riwtr7_fops);
	if (dma_riwtr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_riwtr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_riwtr6 =
	    debugfs_create_file("dma_riwtr6", 744, dir, &dma_riwtr6_val,
				&dma_riwtr6_fops);
	if (dma_riwtr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_riwtr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_riwtr5 =
	    debugfs_create_file("dma_riwtr5", 744, dir, &dma_riwtr5_val,
				&dma_riwtr5_fops);
	if (dma_riwtr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_riwtr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_riwtr4 =
	    debugfs_create_file("dma_riwtr4", 744, dir, &dma_riwtr4_val,
				&dma_riwtr4_fops);
	if (dma_riwtr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_riwtr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_riwtr3 =
	    debugfs_create_file("dma_riwtr3", 744, dir, &dma_riwtr3_val,
				&dma_riwtr3_fops);
	if (dma_riwtr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_riwtr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_riwtr2 =
	    debugfs_create_file("dma_riwtr2", 744, dir, &dma_riwtr2_val,
				&dma_riwtr2_fops);
	if (dma_riwtr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_riwtr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_riwtr1 =
	    debugfs_create_file("dma_riwtr1", 744, dir, &dma_riwtr1_val,
				&dma_riwtr1_fops);
	if (dma_riwtr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_riwtr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_riwtr0 =
	    debugfs_create_file("dma_riwtr0", 744, dir, &dma_riwtr0_val,
				&dma_riwtr0_fops);
	if (dma_riwtr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_riwtr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdrlr7 =
	    debugfs_create_file("dma_rdrlr7", 744, dir, &dma_rdrlr7_val,
				&dma_rdrlr7_fops);
	if (dma_rdrlr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdrlr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdrlr6 =
	    debugfs_create_file("dma_rdrlr6", 744, dir, &dma_rdrlr6_val,
				&dma_rdrlr6_fops);
	if (dma_rdrlr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdrlr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdrlr5 =
	    debugfs_create_file("dma_rdrlr5", 744, dir, &dma_rdrlr5_val,
				&dma_rdrlr5_fops);
	if (dma_rdrlr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdrlr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdrlr4 =
	    debugfs_create_file("dma_rdrlr4", 744, dir, &dma_rdrlr4_val,
				&dma_rdrlr4_fops);
	if (dma_rdrlr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdrlr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdrlr3 =
	    debugfs_create_file("dma_rdrlr3", 744, dir, &dma_rdrlr3_val,
				&dma_rdrlr3_fops);
	if (dma_rdrlr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdrlr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdrlr2 =
	    debugfs_create_file("dma_rdrlr2", 744, dir, &dma_rdrlr2_val,
				&dma_rdrlr2_fops);
	if (dma_rdrlr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdrlr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdrlr1 =
	    debugfs_create_file("dma_rdrlr1", 744, dir, &dma_rdrlr1_val,
				&dma_rdrlr1_fops);
	if (dma_rdrlr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdrlr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdrlr0 =
	    debugfs_create_file("dma_rdrlr0", 744, dir, &dma_rdrlr0_val,
				&dma_rdrlr0_fops);
	if (dma_rdrlr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdrlr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdrlr7 =
	    debugfs_create_file("dma_tdrlr7", 744, dir, &dma_tdrlr7_val,
				&dma_tdrlr7_fops);
	if (dma_tdrlr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdrlr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdrlr6 =
	    debugfs_create_file("dma_tdrlr6", 744, dir, &dma_tdrlr6_val,
				&dma_tdrlr6_fops);
	if (dma_tdrlr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdrlr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdrlr5 =
	    debugfs_create_file("dma_tdrlr5", 744, dir, &dma_tdrlr5_val,
				&dma_tdrlr5_fops);
	if (dma_tdrlr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdrlr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdrlr4 =
	    debugfs_create_file("dma_tdrlr4", 744, dir, &dma_tdrlr4_val,
				&dma_tdrlr4_fops);
	if (dma_tdrlr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdrlr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdrlr3 =
	    debugfs_create_file("dma_tdrlr3", 744, dir, &dma_tdrlr3_val,
				&dma_tdrlr3_fops);
	if (dma_tdrlr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdrlr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdrlr2 =
	    debugfs_create_file("dma_tdrlr2", 744, dir, &dma_tdrlr2_val,
				&dma_tdrlr2_fops);
	if (dma_tdrlr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdrlr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdrlr1 =
	    debugfs_create_file("dma_tdrlr1", 744, dir, &dma_tdrlr1_val,
				&dma_tdrlr1_fops);
	if (dma_tdrlr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdrlr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdrlr0 =
	    debugfs_create_file("dma_tdrlr0", 744, dir, &dma_tdrlr0_val,
				&dma_tdrlr0_fops);
	if (dma_tdrlr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdrlr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdtp_rpdr7 =
	    debugfs_create_file("dma_rdtp_rpdr7", 744, dir, &dma_rdtp_rpdr7_val,
				&dma_rdtp_rpdr7_fops);
	if (dma_rdtp_rpdr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdtp_rpdr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdtp_rpdr6 =
	    debugfs_create_file("dma_rdtp_rpdr6", 744, dir, &dma_rdtp_rpdr6_val,
				&dma_rdtp_rpdr6_fops);
	if (dma_rdtp_rpdr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdtp_rpdr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdtp_rpdr5 =
	    debugfs_create_file("dma_rdtp_rpdr5", 744, dir, &dma_rdtp_rpdr5_val,
				&dma_rdtp_rpdr5_fops);
	if (dma_rdtp_rpdr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdtp_rpdr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdtp_rpdr4 =
	    debugfs_create_file("dma_rdtp_rpdr4", 744, dir, &dma_rdtp_rpdr4_val,
				&dma_rdtp_rpdr4_fops);
	if (dma_rdtp_rpdr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdtp_rpdr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdtp_rpdr3 =
	    debugfs_create_file("dma_rdtp_rpdr3", 744, dir, &dma_rdtp_rpdr3_val,
				&dma_rdtp_rpdr3_fops);
	if (dma_rdtp_rpdr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdtp_rpdr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdtp_rpdr2 =
	    debugfs_create_file("dma_rdtp_rpdr2", 744, dir, &dma_rdtp_rpdr2_val,
				&dma_rdtp_rpdr2_fops);
	if (dma_rdtp_rpdr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdtp_rpdr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdtp_rpdr1 =
	    debugfs_create_file("dma_rdtp_rpdr1", 744, dir, &dma_rdtp_rpdr1_val,
				&dma_rdtp_rpdr1_fops);
	if (dma_rdtp_rpdr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdtp_rpdr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdtp_rpdr0 =
	    debugfs_create_file("dma_rdtp_rpdr0", 744, dir, &dma_rdtp_rpdr0_val,
				&dma_rdtp_rpdr0_fops);
	if (dma_rdtp_rpdr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdtp_rpdr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdtp_tpdr7 =
	    debugfs_create_file("dma_tdtp_tpdr7", 744, dir, &dma_tdtp_tpdr7_val,
				&dma_tdtp_tpdr7_fops);
	if (dma_tdtp_tpdr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdtp_tpdr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdtp_tpdr6 =
	    debugfs_create_file("dma_tdtp_tpdr6", 744, dir, &dma_tdtp_tpdr6_val,
				&dma_tdtp_tpdr6_fops);
	if (dma_tdtp_tpdr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdtp_tpdr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdtp_tpdr5 =
	    debugfs_create_file("dma_tdtp_tpdr5", 744, dir, &dma_tdtp_tpdr5_val,
				&dma_tdtp_tpdr5_fops);
	if (dma_tdtp_tpdr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdtp_tpdr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdtp_tpdr4 =
	    debugfs_create_file("dma_tdtp_tpdr4", 744, dir, &dma_tdtp_tpdr4_val,
				&dma_tdtp_tpdr4_fops);
	if (dma_tdtp_tpdr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdtp_tpdr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdtp_tpdr3 =
	    debugfs_create_file("dma_tdtp_tpdr3", 744, dir, &dma_tdtp_tpdr3_val,
				&dma_tdtp_tpdr3_fops);
	if (dma_tdtp_tpdr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdtp_tpdr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdtp_tpdr2 =
	    debugfs_create_file("dma_tdtp_tpdr2", 744, dir, &dma_tdtp_tpdr2_val,
				&dma_tdtp_tpdr2_fops);
	if (dma_tdtp_tpdr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdtp_tpdr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdtp_tpdr1 =
	    debugfs_create_file("dma_tdtp_tpdr1", 744, dir, &dma_tdtp_tpdr1_val,
				&dma_tdtp_tpdr1_fops);
	if (dma_tdtp_tpdr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdtp_tpdr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdtp_tpdr0 =
	    debugfs_create_file("dma_tdtp_tpdr0", 744, dir, &dma_tdtp_tpdr0_val,
				&dma_tdtp_tpdr0_fops);
	if (dma_tdtp_tpdr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdtp_tpdr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdlar7 =
	    debugfs_create_file("dma_rdlar7", 744, dir, &dma_rdlar7_val,
				&dma_rdlar7_fops);
	if (dma_rdlar7 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdlar7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdlar6 =
	    debugfs_create_file("dma_rdlar6", 744, dir, &dma_rdlar6_val,
				&dma_rdlar6_fops);
	if (dma_rdlar6 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdlar6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdlar5 =
	    debugfs_create_file("dma_rdlar5", 744, dir, &dma_rdlar5_val,
				&dma_rdlar5_fops);
	if (dma_rdlar5 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdlar5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdlar4 =
	    debugfs_create_file("dma_rdlar4", 744, dir, &dma_rdlar4_val,
				&dma_rdlar4_fops);
	if (dma_rdlar4 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdlar4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdlar3 =
	    debugfs_create_file("dma_rdlar3", 744, dir, &dma_rdlar3_val,
				&dma_rdlar3_fops);
	if (dma_rdlar3 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdlar3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdlar2 =
	    debugfs_create_file("dma_rdlar2", 744, dir, &dma_rdlar2_val,
				&dma_rdlar2_fops);
	if (dma_rdlar2 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdlar2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdlar1 =
	    debugfs_create_file("dma_rdlar1", 744, dir, &dma_rdlar1_val,
				&dma_rdlar1_fops);
	if (dma_rdlar1 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdlar1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rdlar0 =
	    debugfs_create_file("dma_rdlar0", 744, dir, &dma_rdlar0_val,
				&dma_rdlar0_fops);
	if (dma_rdlar0 == NULL) {
		printk(KERN_INFO "error creating file: dma_rdlar0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdlar7 =
	    debugfs_create_file("dma_tdlar7", 744, dir, &dma_tdlar7_val,
				&dma_tdlar7_fops);
	if (dma_tdlar7 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdlar7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdlar6 =
	    debugfs_create_file("dma_tdlar6", 744, dir, &dma_tdlar6_val,
				&dma_tdlar6_fops);
	if (dma_tdlar6 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdlar6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdlar5 =
	    debugfs_create_file("dma_tdlar5", 744, dir, &dma_tdlar5_val,
				&dma_tdlar5_fops);
	if (dma_tdlar5 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdlar5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdlar4 =
	    debugfs_create_file("dma_tdlar4", 744, dir, &dma_tdlar4_val,
				&dma_tdlar4_fops);
	if (dma_tdlar4 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdlar4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdlar3 =
	    debugfs_create_file("dma_tdlar3", 744, dir, &dma_tdlar3_val,
				&dma_tdlar3_fops);
	if (dma_tdlar3 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdlar3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdlar2 =
	    debugfs_create_file("dma_tdlar2", 744, dir, &dma_tdlar2_val,
				&dma_tdlar2_fops);
	if (dma_tdlar2 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdlar2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdlar1 =
	    debugfs_create_file("dma_tdlar1", 744, dir, &dma_tdlar1_val,
				&dma_tdlar1_fops);
	if (dma_tdlar1 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdlar1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tdlar0 =
	    debugfs_create_file("dma_tdlar0", 744, dir, &dma_tdlar0_val,
				&dma_tdlar0_fops);
	if (dma_tdlar0 == NULL) {
		printk(KERN_INFO "error creating file: dma_tdlar0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_ier7 =
	    debugfs_create_file("dma_ier7", 744, dir, &dma_ier7_val,
				&dma_ier7_fops);
	if (dma_ier7 == NULL) {
		printk(KERN_INFO "error creating file: dma_ier7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_ier6 =
	    debugfs_create_file("dma_ier6", 744, dir, &dma_ier6_val,
				&dma_ier6_fops);
	if (dma_ier6 == NULL) {
		printk(KERN_INFO "error creating file: dma_ier6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_ier5 =
	    debugfs_create_file("dma_ier5", 744, dir, &dma_ier5_val,
				&dma_ier5_fops);
	if (dma_ier5 == NULL) {
		printk(KERN_INFO "error creating file: dma_ier5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_ier4 =
	    debugfs_create_file("dma_ier4", 744, dir, &dma_ier4_val,
				&dma_ier4_fops);
	if (dma_ier4 == NULL) {
		printk(KERN_INFO "error creating file: dma_ier4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_ier3 =
	    debugfs_create_file("dma_ier3", 744, dir, &dma_ier3_val,
				&dma_ier3_fops);
	if (dma_ier3 == NULL) {
		printk(KERN_INFO "error creating file: dma_ier3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_ier2 =
	    debugfs_create_file("dma_ier2", 744, dir, &dma_ier2_val,
				&dma_ier2_fops);
	if (dma_ier2 == NULL) {
		printk(KERN_INFO "error creating file: dma_ier2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_ier1 =
	    debugfs_create_file("dma_ier1", 744, dir, &dma_ier1_val,
				&dma_ier1_fops);
	if (dma_ier1 == NULL) {
		printk(KERN_INFO "error creating file: dma_ier1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_ier0 =
	    debugfs_create_file("dma_ier0", 744, dir, &dma_ier0_val,
				&dma_ier0_fops);
	if (dma_ier0 == NULL) {
		printk(KERN_INFO "error creating file: dma_ier0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_imr =
	    debugfs_create_file("mac_imr", 744, dir, &mac_imr_val,
				&mac_imr_fops);
	if (mac_imr == NULL) {
		printk(KERN_INFO "error creating file: mac_imr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_isr =
	    debugfs_create_file("mac_isr", 744, dir, &mac_isr_val,
				&mac_isr_fops);
	if (mac_isr == NULL) {
		printk(KERN_INFO "error creating file: mac_isr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_isr =
	    debugfs_create_file("mtl_isr", 744, dir, &mtl_isr_val,
				&mtl_isr_fops);
	if (mtl_isr == NULL) {
		printk(KERN_INFO "error creating file: mtl_isr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sr7 =
	    debugfs_create_file("dma_sr7", 744, dir, &dma_sr7_val,
				&dma_sr7_fops);
	if (dma_sr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_sr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sr6 =
	    debugfs_create_file("dma_sr6", 744, dir, &dma_sr6_val,
				&dma_sr6_fops);
	if (dma_sr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_sr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sr5 =
	    debugfs_create_file("dma_sr5", 744, dir, &dma_sr5_val,
				&dma_sr5_fops);
	if (dma_sr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_sr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sr4 =
	    debugfs_create_file("dma_sr4", 744, dir, &dma_sr4_val,
				&dma_sr4_fops);
	if (dma_sr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_sr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sr3 =
	    debugfs_create_file("dma_sr3", 744, dir, &dma_sr3_val,
				&dma_sr3_fops);
	if (dma_sr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_sr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sr2 =
	    debugfs_create_file("dma_sr2", 744, dir, &dma_sr2_val,
				&dma_sr2_fops);
	if (dma_sr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_sr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sr1 =
	    debugfs_create_file("dma_sr1", 744, dir, &dma_sr1_val,
				&dma_sr1_fops);
	if (dma_sr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_sr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sr0 =
	    debugfs_create_file("dma_sr0", 744, dir, &dma_sr0_val,
				&dma_sr0_fops);
	if (dma_sr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_sr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_isr =
	    debugfs_create_file("dma_isr", 744, dir, &dma_isr_val,
				&dma_isr_fops);
	if (dma_isr == NULL) {
		printk(KERN_INFO "error creating file: dma_isr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_dsr2 =
	    debugfs_create_file("dma_dsr2", 744, dir, &dma_dsr2_val,
				&dma_dsr2_fops);
	if (dma_dsr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_dsr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_dsr1 =
	    debugfs_create_file("dma_dsr1", 744, dir, &dma_dsr1_val,
				&dma_dsr1_fops);
	if (dma_dsr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_dsr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_dsr0 =
	    debugfs_create_file("dma_dsr0", 744, dir, &dma_dsr0_val,
				&dma_dsr0_fops);
	if (dma_dsr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_dsr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0rdr =
	    debugfs_create_file("mtl_q0rdr", 744, dir, &mtl_q0rdr_val,
				&mtl_q0rdr_fops);
	if (mtl_q0rdr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0rdr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0esr =
	    debugfs_create_file("mtl_q0esr", 744, dir, &mtl_q0esr_val,
				&mtl_q0esr_fops);
	if (mtl_q0esr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0esr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0tdr =
	    debugfs_create_file("mtl_q0tdr", 744, dir, &mtl_q0tdr_val,
				&mtl_q0tdr_fops);
	if (mtl_q0tdr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0tdr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrbar7 =
	    debugfs_create_file("dma_chrbar7", 744, dir, &dma_chrbar7_val,
				&dma_chrbar7_fops);
	if (dma_chrbar7 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrbar7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrbar6 =
	    debugfs_create_file("dma_chrbar6", 744, dir, &dma_chrbar6_val,
				&dma_chrbar6_fops);
	if (dma_chrbar6 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrbar6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrbar5 =
	    debugfs_create_file("dma_chrbar5", 744, dir, &dma_chrbar5_val,
				&dma_chrbar5_fops);
	if (dma_chrbar5 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrbar5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrbar4 =
	    debugfs_create_file("dma_chrbar4", 744, dir, &dma_chrbar4_val,
				&dma_chrbar4_fops);
	if (dma_chrbar4 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrbar4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrbar3 =
	    debugfs_create_file("dma_chrbar3", 744, dir, &dma_chrbar3_val,
				&dma_chrbar3_fops);
	if (dma_chrbar3 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrbar3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrbar2 =
	    debugfs_create_file("dma_chrbar2", 744, dir, &dma_chrbar2_val,
				&dma_chrbar2_fops);
	if (dma_chrbar2 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrbar2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrbar1 =
	    debugfs_create_file("dma_chrbar1", 744, dir, &dma_chrbar1_val,
				&dma_chrbar1_fops);
	if (dma_chrbar1 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrbar1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrbar0 =
	    debugfs_create_file("dma_chrbar0", 744, dir, &dma_chrbar0_val,
				&dma_chrbar0_fops);
	if (dma_chrbar0 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrbar0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtbar7 =
	    debugfs_create_file("dma_chtbar7", 744, dir, &dma_chtbar7_val,
				&dma_chtbar7_fops);
	if (dma_chtbar7 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtbar7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtbar6 =
	    debugfs_create_file("dma_chtbar6", 744, dir, &dma_chtbar6_val,
				&dma_chtbar6_fops);
	if (dma_chtbar6 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtbar6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtbar5 =
	    debugfs_create_file("dma_chtbar5", 744, dir, &dma_chtbar5_val,
				&dma_chtbar5_fops);
	if (dma_chtbar5 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtbar5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtbar4 =
	    debugfs_create_file("dma_chtbar4", 744, dir, &dma_chtbar4_val,
				&dma_chtbar4_fops);
	if (dma_chtbar4 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtbar4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtbar3 =
	    debugfs_create_file("dma_chtbar3", 744, dir, &dma_chtbar3_val,
				&dma_chtbar3_fops);
	if (dma_chtbar3 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtbar3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtbar2 =
	    debugfs_create_file("dma_chtbar2", 744, dir, &dma_chtbar2_val,
				&dma_chtbar2_fops);
	if (dma_chtbar2 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtbar2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtbar1 =
	    debugfs_create_file("dma_chtbar1", 744, dir, &dma_chtbar1_val,
				&dma_chtbar1_fops);
	if (dma_chtbar1 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtbar1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtbar0 =
	    debugfs_create_file("dma_chtbar0", 744, dir, &dma_chtbar0_val,
				&dma_chtbar0_fops);
	if (dma_chtbar0 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtbar0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrdr7 =
	    debugfs_create_file("dma_chrdr7", 744, dir, &dma_chrdr7_val,
				&dma_chrdr7_fops);
	if (dma_chrdr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrdr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrdr6 =
	    debugfs_create_file("dma_chrdr6", 744, dir, &dma_chrdr6_val,
				&dma_chrdr6_fops);
	if (dma_chrdr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrdr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrdr5 =
	    debugfs_create_file("dma_chrdr5", 744, dir, &dma_chrdr5_val,
				&dma_chrdr5_fops);
	if (dma_chrdr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrdr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrdr4 =
	    debugfs_create_file("dma_chrdr4", 744, dir, &dma_chrdr4_val,
				&dma_chrdr4_fops);
	if (dma_chrdr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrdr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrdr3 =
	    debugfs_create_file("dma_chrdr3", 744, dir, &dma_chrdr3_val,
				&dma_chrdr3_fops);
	if (dma_chrdr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrdr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrdr2 =
	    debugfs_create_file("dma_chrdr2", 744, dir, &dma_chrdr2_val,
				&dma_chrdr2_fops);
	if (dma_chrdr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrdr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrdr1 =
	    debugfs_create_file("dma_chrdr1", 744, dir, &dma_chrdr1_val,
				&dma_chrdr1_fops);
	if (dma_chrdr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrdr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chrdr0 =
	    debugfs_create_file("dma_chrdr0", 744, dir, &dma_chrdr0_val,
				&dma_chrdr0_fops);
	if (dma_chrdr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_chrdr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtdr7 =
	    debugfs_create_file("dma_chtdr7", 744, dir, &dma_chtdr7_val,
				&dma_chtdr7_fops);
	if (dma_chtdr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtdr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtdr6 =
	    debugfs_create_file("dma_chtdr6", 744, dir, &dma_chtdr6_val,
				&dma_chtdr6_fops);
	if (dma_chtdr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtdr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtdr5 =
	    debugfs_create_file("dma_chtdr5", 744, dir, &dma_chtdr5_val,
				&dma_chtdr5_fops);
	if (dma_chtdr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtdr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtdr4 =
	    debugfs_create_file("dma_chtdr4", 744, dir, &dma_chtdr4_val,
				&dma_chtdr4_fops);
	if (dma_chtdr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtdr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtdr3 =
	    debugfs_create_file("dma_chtdr3", 744, dir, &dma_chtdr3_val,
				&dma_chtdr3_fops);
	if (dma_chtdr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtdr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtdr2 =
	    debugfs_create_file("dma_chtdr2", 744, dir, &dma_chtdr2_val,
				&dma_chtdr2_fops);
	if (dma_chtdr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtdr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtdr1 =
	    debugfs_create_file("dma_chtdr1", 744, dir, &dma_chtdr1_val,
				&dma_chtdr1_fops);
	if (dma_chtdr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtdr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_chtdr0 =
	    debugfs_create_file("dma_chtdr0", 744, dir, &dma_chtdr0_val,
				&dma_chtdr0_fops);
	if (dma_chtdr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_chtdr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sfcsr7 =
	    debugfs_create_file("dma_sfcsr7", 744, dir, &dma_sfcsr7_val,
				&dma_sfcsr7_fops);
	if (dma_sfcsr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_sfcsr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sfcsr6 =
	    debugfs_create_file("dma_sfcsr6", 744, dir, &dma_sfcsr6_val,
				&dma_sfcsr6_fops);
	if (dma_sfcsr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_sfcsr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sfcsr5 =
	    debugfs_create_file("dma_sfcsr5", 744, dir, &dma_sfcsr5_val,
				&dma_sfcsr5_fops);
	if (dma_sfcsr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_sfcsr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sfcsr4 =
	    debugfs_create_file("dma_sfcsr4", 744, dir, &dma_sfcsr4_val,
				&dma_sfcsr4_fops);
	if (dma_sfcsr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_sfcsr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sfcsr3 =
	    debugfs_create_file("dma_sfcsr3", 744, dir, &dma_sfcsr3_val,
				&dma_sfcsr3_fops);
	if (dma_sfcsr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_sfcsr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sfcsr2 =
	    debugfs_create_file("dma_sfcsr2", 744, dir, &dma_sfcsr2_val,
				&dma_sfcsr2_fops);
	if (dma_sfcsr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_sfcsr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sfcsr1 =
	    debugfs_create_file("dma_sfcsr1", 744, dir, &dma_sfcsr1_val,
				&dma_sfcsr1_fops);
	if (dma_sfcsr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_sfcsr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sfcsr0 =
	    debugfs_create_file("dma_sfcsr0", 744, dir, &dma_sfcsr0_val,
				&dma_sfcsr0_fops);
	if (dma_sfcsr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_sfcsr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_ivlantirr =
	    debugfs_create_file("mac_ivlantirr", 744, dir, &mac_ivlantirr_val,
				&mac_ivlantirr_fops);
	if (mac_ivlantirr == NULL) {
		printk(KERN_INFO "error creating file: mac_ivlantirr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_vlantirr =
	    debugfs_create_file("mac_vlantirr", 744, dir, &mac_vlantirr_val,
				&mac_vlantirr_fops);
	if (mac_vlantirr == NULL) {
		printk(KERN_INFO "error creating file: mac_vlantirr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_vlanhtr =
	    debugfs_create_file("mac_vlanhtr", 744, dir, &mac_vlanhtr_val,
				&mac_vlanhtr_fops);
	if (mac_vlanhtr == NULL) {
		printk(KERN_INFO "error creating file: mac_vlanhtr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_vlantr =
	    debugfs_create_file("mac_vlantr", 744, dir, &mac_vlantr_val,
				&mac_vlantr_fops);
	if (mac_vlantr == NULL) {
		printk(KERN_INFO "error creating file: mac_vlantr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_sbus =
	    debugfs_create_file("dma_sbus", 744, dir, &dma_sbus_val,
				&dma_sbus_fops);
	if (dma_sbus == NULL) {
		printk(KERN_INFO "error creating file: dma_sbus\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_bmr =
	    debugfs_create_file("dma_bmr", 744, dir, &dma_bmr_val,
				&dma_bmr_fops);
	if (dma_bmr == NULL) {
		printk(KERN_INFO "error creating file: dma_bmr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0rcr =
	    debugfs_create_file("mtl_q0rcr", 744, dir, &mtl_q0rcr_val,
				&mtl_q0rcr_fops);
	if (mtl_q0rcr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0rcr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0ocr =
	    debugfs_create_file("mtl_q0ocr", 744, dir, &mtl_q0ocr_val,
				&mtl_q0ocr_fops);
	if (mtl_q0ocr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0ocr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0romr =
	    debugfs_create_file("mtl_q0romr", 744, dir, &mtl_q0romr_val,
				&mtl_q0romr_fops);
	if (mtl_q0romr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0romr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0qr =
	    debugfs_create_file("mtl_q0qr", 744, dir, &mtl_q0qr_val,
				&mtl_q0qr_fops);
	if (mtl_q0qr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0qr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0ecr =
	    debugfs_create_file("mtl_q0ecr", 744, dir, &mtl_q0ecr_val,
				&mtl_q0ecr_fops);
	if (mtl_q0ecr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0ecr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0ucr =
	    debugfs_create_file("mtl_q0ucr", 744, dir, &mtl_q0ucr_val,
				&mtl_q0ucr_fops);
	if (mtl_q0ucr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0ucr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_q0tomr =
	    debugfs_create_file("mtl_q0tomr", 744, dir, &mtl_q0tomr_val,
				&mtl_q0tomr_fops);
	if (mtl_q0tomr == NULL) {
		printk(KERN_INFO "error creating file: mtl_q0tomr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_rqdcm1r =
	    debugfs_create_file("mtl_rqdcm1r", 744, dir, &mtl_rqdcm1r_val,
				&mtl_rqdcm1r_fops);
	if (mtl_rqdcm1r == NULL) {
		printk(KERN_INFO "error creating file: mtl_rqdcm1r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_rqdcm0r =
	    debugfs_create_file("mtl_rqdcm0r", 744, dir, &mtl_rqdcm0r_val,
				&mtl_rqdcm0r_fops);
	if (mtl_rqdcm0r == NULL) {
		printk(KERN_INFO "error creating file: mtl_rqdcm0r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_fddr =
	    debugfs_create_file("mtl_fddr", 744, dir, &mtl_fddr_val,
				&mtl_fddr_fops);
	if (mtl_fddr == NULL) {
		printk(KERN_INFO "error creating file: mtl_fddr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_fdacs =
	    debugfs_create_file("mtl_fdacs", 744, dir, &mtl_fdacs_val,
				&mtl_fdacs_fops);
	if (mtl_fdacs == NULL) {
		printk(KERN_INFO "error creating file: mtl_fdacs\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mtl_omr =
	    debugfs_create_file("mtl_omr", 744, dir, &mtl_omr_val,
				&mtl_omr_fops);
	if (mtl_omr == NULL) {
		printk(KERN_INFO "error creating file: mtl_omr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_rqc3r =
	    debugfs_create_file("mac_rqc3r", 744, dir, &mac_rqc3r_val,
				&mac_rqc3r_fops);
	if (mac_rqc3r == NULL) {
		printk(KERN_INFO "error creating file: mac_rqc3r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_rqc2r =
	    debugfs_create_file("mac_rqc2r", 744, dir, &mac_rqc2r_val,
				&mac_rqc2r_fops);
	if (mac_rqc2r == NULL) {
		printk(KERN_INFO "error creating file: mac_rqc2r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_rqc1r =
	    debugfs_create_file("mac_rqc1r", 744, dir, &mac_rqc1r_val,
				&mac_rqc1r_fops);
	if (mac_rqc1r == NULL) {
		printk(KERN_INFO "error creating file: mac_rqc1r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_rqc0r =
	    debugfs_create_file("mac_rqc0r", 744, dir, &mac_rqc0r_val,
				&mac_rqc0r_fops);
	if (mac_rqc0r == NULL) {
		printk(KERN_INFO "error creating file: mac_rqc0r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_tqpm1r =
	    debugfs_create_file("mac_tqpm1r", 744, dir, &mac_tqpm1r_val,
				&mac_tqpm1r_fops);
	if (mac_tqpm1r == NULL) {
		printk(KERN_INFO "error creating file: mac_tqpm1r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_tqpm0r =
	    debugfs_create_file("mac_tqpm0r", 744, dir, &mac_tqpm0r_val,
				&mac_tqpm0r_fops);
	if (mac_tqpm0r == NULL) {
		printk(KERN_INFO "error creating file: mac_tqpm0r\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_rfcr =
	    debugfs_create_file("mac_rfcr", 744, dir, &mac_rfcr_val,
				&mac_rfcr_fops);
	if (mac_rfcr == NULL) {
		printk(KERN_INFO "error creating file: mac_rfcr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_qtfcr7 =
	    debugfs_create_file("mac_qtfcr7", 744, dir, &mac_qtfcr7_val,
				&mac_qtfcr7_fops);
	if (mac_qtfcr7 == NULL) {
		printk(KERN_INFO "error creating file: mac_qtfcr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_qtfcr6 =
	    debugfs_create_file("mac_qtfcr6", 744, dir, &mac_qtfcr6_val,
				&mac_qtfcr6_fops);
	if (mac_qtfcr6 == NULL) {
		printk(KERN_INFO "error creating file: mac_qtfcr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_qtfcr5 =
	    debugfs_create_file("mac_qtfcr5", 744, dir, &mac_qtfcr5_val,
				&mac_qtfcr5_fops);
	if (mac_qtfcr5 == NULL) {
		printk(KERN_INFO "error creating file: mac_qtfcr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_qtfcr4 =
	    debugfs_create_file("mac_qtfcr4", 744, dir, &mac_qtfcr4_val,
				&mac_qtfcr4_fops);
	if (mac_qtfcr4 == NULL) {
		printk(KERN_INFO "error creating file: mac_qtfcr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_qtfcr3 =
	    debugfs_create_file("mac_qtfcr3", 744, dir, &mac_qtfcr3_val,
				&mac_qtfcr3_fops);
	if (mac_qtfcr3 == NULL) {
		printk(KERN_INFO "error creating file: mac_qtfcr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_qtfcr2 =
	    debugfs_create_file("mac_qtfcr2", 744, dir, &mac_qtfcr2_val,
				&mac_qtfcr2_fops);
	if (mac_qtfcr2 == NULL) {
		printk(KERN_INFO "error creating file: mac_qtfcr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_qtfcr1 =
	    debugfs_create_file("mac_qtfcr1", 744, dir, &mac_qtfcr1_val,
				&mac_qtfcr1_fops);
	if (mac_qtfcr1 == NULL) {
		printk(KERN_INFO "error creating file: mac_qtfcr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_q0tfcr =
	    debugfs_create_file("mac_q0tfcr", 744, dir, &mac_q0tfcr_val,
				&mac_q0tfcr_fops);
	if (mac_q0tfcr == NULL) {
		printk(KERN_INFO "error creating file: mac_q0tfcr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}
#if 0
	dma_axi4cr7 =
	    debugfs_create_file("dma_axi4cr7", 744, dir, &dma_axi4cr7_val,
				&dma_axi4cr7_fops);
	if (dma_axi4cr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_axi4cr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_axi4cr6 =
	    debugfs_create_file("dma_axi4cr6", 744, dir, &dma_axi4cr6_val,
				&dma_axi4cr6_fops);
	if (dma_axi4cr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_axi4cr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_axi4cr5 =
	    debugfs_create_file("dma_axi4cr5", 744, dir, &dma_axi4cr5_val,
				&dma_axi4cr5_fops);
	if (dma_axi4cr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_axi4cr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_axi4cr4 =
	    debugfs_create_file("dma_axi4cr4", 744, dir, &dma_axi4cr4_val,
				&dma_axi4cr4_fops);
	if (dma_axi4cr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_axi4cr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_axi4cr3 =
	    debugfs_create_file("dma_axi4cr3", 744, dir, &dma_axi4cr3_val,
				&dma_axi4cr3_fops);
	if (dma_axi4cr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_axi4cr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_axi4cr2 =
	    debugfs_create_file("dma_axi4cr2", 744, dir, &dma_axi4cr2_val,
				&dma_axi4cr2_fops);
	if (dma_axi4cr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_axi4cr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_axi4cr1 =
	    debugfs_create_file("dma_axi4cr1", 744, dir, &dma_axi4cr1_val,
				&dma_axi4cr1_fops);
	if (dma_axi4cr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_axi4cr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_axi4cr0 =
	    debugfs_create_file("dma_axi4cr0", 744, dir, &dma_axi4cr0_val,
				&dma_axi4cr0_fops);
	if (dma_axi4cr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_axi4cr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}
#endif

	dma_rcr7 =
	    debugfs_create_file("dma_rcr7", 744, dir, &dma_rcr7_val,
				&dma_rcr7_fops);
	if (dma_rcr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_rcr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rcr6 =
	    debugfs_create_file("dma_rcr6", 744, dir, &dma_rcr6_val,
				&dma_rcr6_fops);
	if (dma_rcr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_rcr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rcr5 =
	    debugfs_create_file("dma_rcr5", 744, dir, &dma_rcr5_val,
				&dma_rcr5_fops);
	if (dma_rcr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_rcr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rcr4 =
	    debugfs_create_file("dma_rcr4", 744, dir, &dma_rcr4_val,
				&dma_rcr4_fops);
	if (dma_rcr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_rcr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rcr3 =
	    debugfs_create_file("dma_rcr3", 744, dir, &dma_rcr3_val,
				&dma_rcr3_fops);
	if (dma_rcr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_rcr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rcr2 =
	    debugfs_create_file("dma_rcr2", 744, dir, &dma_rcr2_val,
				&dma_rcr2_fops);
	if (dma_rcr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_rcr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rcr1 =
	    debugfs_create_file("dma_rcr1", 744, dir, &dma_rcr1_val,
				&dma_rcr1_fops);
	if (dma_rcr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_rcr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_rcr0 =
	    debugfs_create_file("dma_rcr0", 744, dir, &dma_rcr0_val,
				&dma_rcr0_fops);
	if (dma_rcr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_rcr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tcr7 =
	    debugfs_create_file("dma_tcr7", 744, dir, &dma_tcr7_val,
				&dma_tcr7_fops);
	if (dma_tcr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_tcr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tcr6 =
	    debugfs_create_file("dma_tcr6", 744, dir, &dma_tcr6_val,
				&dma_tcr6_fops);
	if (dma_tcr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_tcr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tcr5 =
	    debugfs_create_file("dma_tcr5", 744, dir, &dma_tcr5_val,
				&dma_tcr5_fops);
	if (dma_tcr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_tcr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tcr4 =
	    debugfs_create_file("dma_tcr4", 744, dir, &dma_tcr4_val,
				&dma_tcr4_fops);
	if (dma_tcr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_tcr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tcr3 =
	    debugfs_create_file("dma_tcr3", 744, dir, &dma_tcr3_val,
				&dma_tcr3_fops);
	if (dma_tcr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_tcr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tcr2 =
	    debugfs_create_file("dma_tcr2", 744, dir, &dma_tcr2_val,
				&dma_tcr2_fops);
	if (dma_tcr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_tcr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tcr1 =
	    debugfs_create_file("dma_tcr1", 744, dir, &dma_tcr1_val,
				&dma_tcr1_fops);
	if (dma_tcr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_tcr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_tcr0 =
	    debugfs_create_file("dma_tcr0", 744, dir, &dma_tcr0_val,
				&dma_tcr0_fops);
	if (dma_tcr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_tcr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_cr7 =
	    debugfs_create_file("dma_cr7", 744, dir, &dma_cr7_val,
				&dma_cr7_fops);
	if (dma_cr7 == NULL) {
		printk(KERN_INFO "error creating file: dma_cr7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_cr6 =
	    debugfs_create_file("dma_cr6", 744, dir, &dma_cr6_val,
				&dma_cr6_fops);
	if (dma_cr6 == NULL) {
		printk(KERN_INFO "error creating file: dma_cr6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_cr5 =
	    debugfs_create_file("dma_cr5", 744, dir, &dma_cr5_val,
				&dma_cr5_fops);
	if (dma_cr5 == NULL) {
		printk(KERN_INFO "error creating file: dma_cr5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_cr4 =
	    debugfs_create_file("dma_cr4", 744, dir, &dma_cr4_val,
				&dma_cr4_fops);
	if (dma_cr4 == NULL) {
		printk(KERN_INFO "error creating file: dma_cr4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_cr3 =
	    debugfs_create_file("dma_cr3", 744, dir, &dma_cr3_val,
				&dma_cr3_fops);
	if (dma_cr3 == NULL) {
		printk(KERN_INFO "error creating file: dma_cr3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_cr2 =
	    debugfs_create_file("dma_cr2", 744, dir, &dma_cr2_val,
				&dma_cr2_fops);
	if (dma_cr2 == NULL) {
		printk(KERN_INFO "error creating file: dma_cr2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_cr1 =
	    debugfs_create_file("dma_cr1", 744, dir, &dma_cr1_val,
				&dma_cr1_fops);
	if (dma_cr1 == NULL) {
		printk(KERN_INFO "error creating file: dma_cr1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	dma_cr0 =
	    debugfs_create_file("dma_cr0", 744, dir, &dma_cr0_val,
				&dma_cr0_fops);
	if (dma_cr0 == NULL) {
		printk(KERN_INFO "error creating file: dma_cr0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_wtr =
	    debugfs_create_file("mac_wtr", 744, dir, &mac_wtr_val,
				&mac_wtr_fops);
	if (mac_wtr == NULL) {
		printk(KERN_INFO "error creating file: mac_wtr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_mpfr =
	    debugfs_create_file("mac_mpfr", 744, dir, &mac_mpfr_val,
				&mac_mpfr_fops);
	if (mac_mpfr == NULL) {
		printk(KERN_INFO "error creating file: mac_mpfr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_mecr =
	    debugfs_create_file("mac_mecr", 744, dir, &mac_mecr_val,
				&mac_mecr_fops);
	if (mac_mecr == NULL) {
		printk(KERN_INFO "error creating file: mac_mecr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_mcr =
	    debugfs_create_file("mac_mcr", 744, dir, &mac_mcr_val,
				&mac_mcr_fops);
	if (mac_mcr == NULL) {
		printk(KERN_INFO "error creating file: mac_mcr\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}
	/* MII/GMII registers */
	mac_bmcr_reg =
	    debugfs_create_file("mac_bmcr_reg", 744, dir, &mac_bmcr_reg_val,
				&mac_bmcr_reg_fops);
	if (mac_bmcr_reg == NULL) {
		printk(KERN_INFO "error creating file: mac_bmcr_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mac_bmsr_reg =
	    debugfs_create_file("mac_bmsr_reg", 744, dir, &mac_bmsr_reg_val,
				&mac_bmsr_reg_fops);
	if (mac_bmsr_reg == NULL) {
		printk(KERN_INFO "error creating file: mac_bmsr_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mii_physid1_reg =
	    debugfs_create_file("mii_physid1_reg", 744, dir,
				&mii_physid1_reg_val, &mii_physid1_reg_fops);
	if (mii_physid1_reg == NULL) {
		printk(KERN_INFO "error creating file: mii_physid1_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mii_physid2_reg =
	    debugfs_create_file("mii_physid2_reg", 744, dir,
				&mii_physid2_reg_val, &mii_physid2_reg_fops);
	if (mii_physid2_reg == NULL) {
		printk(KERN_INFO "error creating file: mii_physid2_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mii_advertise_reg =
	    debugfs_create_file("mii_advertise_reg", 744, dir,
				&mii_advertise_reg_val,
				&mii_advertise_reg_fops);
	if (mii_advertise_reg == NULL) {
		printk(KERN_INFO "error creating file: mii_advertise_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mii_lpa_reg =
	    debugfs_create_file("mii_lpa_reg", 744, dir, &mii_lpa_reg_val,
				&mii_lpa_reg_fops);
	if (mii_lpa_reg == NULL) {
		printk(KERN_INFO "error creating file: mii_lpa_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mii_expansion_reg =
	    debugfs_create_file("mii_expansion_reg", 744, dir,
				&mii_expansion_reg_val,
				&mii_expansion_reg_fops);
	if (mii_expansion_reg == NULL) {
		printk(KERN_INFO "error creating file: mii_expansion_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	auto_nego_np_reg =
	    debugfs_create_file("auto_nego_np_reg", 744, dir,
				&auto_nego_np_reg_val, &auto_nego_np_reg_fops);
	if (auto_nego_np_reg == NULL) {
		printk(KERN_INFO "error creating file: auto_nego_np_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mii_estatus_reg =
	    debugfs_create_file("mii_estatus_reg", 744, dir,
				&mii_estatus_reg_val, &mii_estatus_reg_fops);
	if (mii_estatus_reg == NULL) {
		printk(KERN_INFO "error creating file: mii_estatus_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mii_ctrl1000_reg =
	    debugfs_create_file("mii_ctrl1000_reg", 744, dir,
				&mii_ctrl1000_reg_val, &mii_ctrl1000_reg_fops);
	if (mii_ctrl1000_reg == NULL) {
		printk(KERN_INFO "error creating file: mii_ctrl1000_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	mii_stat1000_reg =
	    debugfs_create_file("mii_stat1000_reg", 744, dir,
				&mii_stat1000_reg_val, &mii_stat1000_reg_fops);
	if (mii_stat1000_reg == NULL) {
		printk(KERN_INFO "error creating file: mii_stat1000_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	phy_ctl_reg =
	    debugfs_create_file("phy_ctl_reg", 744, dir, &phy_ctl_reg_val,
				&phy_ctl_reg_fops);
	if (phy_ctl_reg == NULL) {
		printk(KERN_INFO "error creating file: phy_ctl_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	phy_sts_reg =
	    debugfs_create_file("phy_sts_reg", 744, dir, &phy_sts_reg_val,
				&phy_sts_reg_fops);
	if (phy_sts_reg == NULL) {
		printk(KERN_INFO "error creating file: phy_sts_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	feature_drop_tx_pktburstcnt =
	    debugfs_create_file("feature_drop_tx_pktburstcnt", 744, dir,
				&feature_drop_tx_pktburstcnt_val,
				&feature_drop_tx_pktburstcnt_fops);
	if (feature_drop_tx_pktburstcnt == NULL) {
		printk(KERN_INFO
		       "error creating file: feature_drop_tx_pktburstcnt\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	qInx = debugfs_create_file("qInx", 744, dir, &qInx_val, &qInx_fops);
	if (qInx == NULL) {
		printk(KERN_INFO "error creating file: qInx\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc0 =
	    debugfs_create_file("rx_normal_desc_descriptor0", 744, dir, NULL,
				&rx_normal_desc_desc_fops0);
	if (rx_normal_desc_desc0 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc1 =
	    debugfs_create_file("rx_normal_desc_descriptor1", 744, dir, NULL,
				&rx_normal_desc_desc_fops1);
	if (rx_normal_desc_desc1 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc2 =
	    debugfs_create_file("rx_normal_desc_descriptor2", 744, dir, NULL,
				&rx_normal_desc_desc_fops2);
	if (rx_normal_desc_desc2 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc3 =
	    debugfs_create_file("rx_normal_desc_descriptor3", 744, dir, NULL,
				&rx_normal_desc_desc_fops3);
	if (rx_normal_desc_desc3 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc4 =
	    debugfs_create_file("rx_normal_desc_descriptor4", 744, dir, NULL,
				&rx_normal_desc_desc_fops4);
	if (rx_normal_desc_desc4 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc5 =
	    debugfs_create_file("rx_normal_desc_descriptor5", 744, dir, NULL,
				&rx_normal_desc_desc_fops5);
	if (rx_normal_desc_desc5 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc6 =
	    debugfs_create_file("rx_normal_desc_descriptor6", 744, dir, NULL,
				&rx_normal_desc_desc_fops6);
	if (rx_normal_desc_desc6 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc7 =
	    debugfs_create_file("rx_normal_desc_descriptor7", 744, dir, NULL,
				&rx_normal_desc_desc_fops7);
	if (rx_normal_desc_desc7 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc8 =
	    debugfs_create_file("rx_normal_desc_descriptor8", 744, dir, NULL,
				&rx_normal_desc_desc_fops8);
	if (rx_normal_desc_desc8 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor8\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc9 =
	    debugfs_create_file("rx_normal_desc_descriptor9", 744, dir, NULL,
				&rx_normal_desc_desc_fops9);
	if (rx_normal_desc_desc9 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor9\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc10 =
	    debugfs_create_file("rx_normal_desc_descriptor10", 744, dir, NULL,
				&rx_normal_desc_desc_fops10);
	if (rx_normal_desc_desc10 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor10\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc11 =
	    debugfs_create_file("rx_normal_desc_descriptor11", 744, dir, NULL,
				&rx_normal_desc_desc_fops11);
	if (rx_normal_desc_desc11 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor11\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_desc12 =
	    debugfs_create_file("rx_normal_desc_descriptor12", 744, dir, NULL,
				&rx_normal_desc_desc_fops12);
	if (rx_normal_desc_desc12 == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_descriptor12\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc0 =
	    debugfs_create_file("tx_normal_desc_descriptor0", 744, dir, NULL,
				&tx_normal_desc_desc_fops0);
	if (tx_normal_desc_desc0 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc1 =
	    debugfs_create_file("tx_normal_desc_descriptor1", 744, dir, NULL,
				&tx_normal_desc_desc_fops1);
	if (tx_normal_desc_desc1 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc2 =
	    debugfs_create_file("tx_normal_desc_descriptor2", 744, dir, NULL,
				&tx_normal_desc_desc_fops2);
	if (tx_normal_desc_desc2 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc3 =
	    debugfs_create_file("tx_normal_desc_descriptor3", 744, dir, NULL,
				&tx_normal_desc_desc_fops3);
	if (tx_normal_desc_desc3 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc4 =
	    debugfs_create_file("tx_normal_desc_descriptor4", 744, dir, NULL,
				&tx_normal_desc_desc_fops4);
	if (tx_normal_desc_desc4 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc5 =
	    debugfs_create_file("tx_normal_desc_descriptor5", 744, dir, NULL,
				&tx_normal_desc_desc_fops5);
	if (tx_normal_desc_desc5 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc6 =
	    debugfs_create_file("tx_normal_desc_descriptor6", 744, dir, NULL,
				&tx_normal_desc_desc_fops6);
	if (tx_normal_desc_desc6 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc7 =
	    debugfs_create_file("tx_normal_desc_descriptor7", 744, dir, NULL,
				&tx_normal_desc_desc_fops7);
	if (tx_normal_desc_desc7 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc8 =
	    debugfs_create_file("tx_normal_desc_descriptor8", 744, dir, NULL,
				&tx_normal_desc_desc_fops8);
	if (tx_normal_desc_desc8 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor8\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc9 =
	    debugfs_create_file("tx_normal_desc_descriptor9", 744, dir, NULL,
				&tx_normal_desc_desc_fops9);
	if (tx_normal_desc_desc9 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor9\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc10 =
	    debugfs_create_file("tx_normal_desc_descriptor10", 744, dir, NULL,
				&tx_normal_desc_desc_fops10);
	if (tx_normal_desc_desc10 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor10\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc11 =
	    debugfs_create_file("tx_normal_desc_descriptor11", 744, dir, NULL,
				&tx_normal_desc_desc_fops11);
	if (tx_normal_desc_desc11 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor11\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_desc12 =
	    debugfs_create_file("tx_normal_desc_descriptor12", 744, dir, NULL,
				&tx_normal_desc_desc_fops12);
	if (tx_normal_desc_desc12 == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_descriptor12\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	tx_normal_desc_status =
	    debugfs_create_file("tx_normal_desc_status", 744, dir, NULL,
				&tx_normal_desc_status_fops);
	if (tx_normal_desc_status == NULL) {
		printk(KERN_INFO
		       "error while creating file: tx_normal_desc_status\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	rx_normal_desc_status =
	    debugfs_create_file("rx_normal_desc_status", 744, dir, NULL,
				&rx_normal_desc_status_fops);
	if (rx_normal_desc_status == NULL) {
		printk(KERN_INFO
		       "error while creating file: rx_normal_desc_status\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DBGPR("<-- create_debug_files\n");

	return ret;

 remove_debug_file:
	remove_debug_files();
	DBGPR("<-- create_debug_files\n");
	return ret;
}

/*! 
* \brief  API to remove debugfs files 
*
* \details This function will removes all debug files created inside 
* /sys/kernel/debug/ddgen_dwc_eqos directory and also the directory
* ddgen_dwc_eqos.
*
* \retval  0 on Success. 
* \retval  error number on Failure.
*/

void remove_debug_files()
{
	DBGPR("--> remove_debug_files\n");
	debugfs_remove_recursive(dir);
	DBGPR("<-- remove_debug_files\n");
	return;
}
