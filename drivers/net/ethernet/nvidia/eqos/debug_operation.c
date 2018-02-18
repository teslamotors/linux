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
#include <linux/fs.h>
#include <linux/debugfs.h>
#include "yheader.h"

#include "yregacc.h"

#define DEBUGFS_MAX_SIZE 100
static char debugfs_buf[DEBUGFS_MAX_SIZE];

struct eqos_prv_data *pdata;

static void do_transmit_alignment_test(struct eqos_prv_data *pdata);
static void bcm_regs_clause45_write(int dev, int reg, unsigned int data);
static void bcm_regs_clause45_read(int dev, int reg, unsigned int *data);

/*
 * This structure hold information about the /debug file
 */
static struct dentry *dir;

/* Variables used to store the register value. */
static unsigned int registers_val;
static unsigned int MAC_MA32_127LR127_val;
static unsigned int MAC_MA32_127LR126_val;
static unsigned int MAC_MA32_127LR125_val;
static unsigned int MAC_MA32_127LR124_val;
static unsigned int MAC_MA32_127LR123_val;
static unsigned int MAC_MA32_127LR122_val;
static unsigned int MAC_MA32_127LR121_val;
static unsigned int MAC_MA32_127LR120_val;
static unsigned int MAC_MA32_127LR119_val;
static unsigned int MAC_MA32_127LR118_val;
static unsigned int MAC_MA32_127LR117_val;
static unsigned int MAC_MA32_127LR116_val;
static unsigned int MAC_MA32_127LR115_val;
static unsigned int MAC_MA32_127LR114_val;
static unsigned int MAC_MA32_127LR113_val;
static unsigned int MAC_MA32_127LR112_val;
static unsigned int MAC_MA32_127LR111_val;
static unsigned int MAC_MA32_127LR110_val;
static unsigned int MAC_MA32_127LR109_val;
static unsigned int MAC_MA32_127LR108_val;
static unsigned int MAC_MA32_127LR107_val;
static unsigned int MAC_MA32_127LR106_val;
static unsigned int MAC_MA32_127LR105_val;
static unsigned int MAC_MA32_127LR104_val;
static unsigned int MAC_MA32_127LR103_val;
static unsigned int MAC_MA32_127LR102_val;
static unsigned int MAC_MA32_127LR101_val;
static unsigned int MAC_MA32_127LR100_val;
static unsigned int MAC_MA32_127LR99_val;
static unsigned int MAC_MA32_127LR98_val;
static unsigned int MAC_MA32_127LR97_val;
static unsigned int MAC_MA32_127LR96_val;
static unsigned int MAC_MA32_127LR95_val;
static unsigned int MAC_MA32_127LR94_val;
static unsigned int MAC_MA32_127LR93_val;
static unsigned int MAC_MA32_127LR92_val;
static unsigned int MAC_MA32_127LR91_val;
static unsigned int MAC_MA32_127LR90_val;
static unsigned int MAC_MA32_127LR89_val;
static unsigned int MAC_MA32_127LR88_val;
static unsigned int MAC_MA32_127LR87_val;
static unsigned int MAC_MA32_127LR86_val;
static unsigned int MAC_MA32_127LR85_val;
static unsigned int MAC_MA32_127LR84_val;
static unsigned int MAC_MA32_127LR83_val;
static unsigned int MAC_MA32_127LR82_val;
static unsigned int MAC_MA32_127LR81_val;
static unsigned int MAC_MA32_127LR80_val;
static unsigned int MAC_MA32_127LR79_val;
static unsigned int MAC_MA32_127LR78_val;
static unsigned int MAC_MA32_127LR77_val;
static unsigned int MAC_MA32_127LR76_val;
static unsigned int MAC_MA32_127LR75_val;
static unsigned int MAC_MA32_127LR74_val;
static unsigned int MAC_MA32_127LR73_val;
static unsigned int MAC_MA32_127LR72_val;
static unsigned int MAC_MA32_127LR71_val;
static unsigned int MAC_MA32_127LR70_val;
static unsigned int MAC_MA32_127LR69_val;
static unsigned int MAC_MA32_127LR68_val;
static unsigned int MAC_MA32_127LR67_val;
static unsigned int MAC_MA32_127LR66_val;
static unsigned int MAC_MA32_127LR65_val;
static unsigned int MAC_MA32_127LR64_val;
static unsigned int MAC_MA32_127LR63_val;
static unsigned int MAC_MA32_127LR62_val;
static unsigned int MAC_MA32_127LR61_val;
static unsigned int MAC_MA32_127LR60_val;
static unsigned int MAC_MA32_127LR59_val;
static unsigned int MAC_MA32_127LR58_val;
static unsigned int MAC_MA32_127LR57_val;
static unsigned int MAC_MA32_127LR56_val;
static unsigned int MAC_MA32_127LR55_val;
static unsigned int MAC_MA32_127LR54_val;
static unsigned int MAC_MA32_127LR53_val;
static unsigned int MAC_MA32_127LR52_val;
static unsigned int MAC_MA32_127LR51_val;
static unsigned int MAC_MA32_127LR50_val;
static unsigned int MAC_MA32_127LR49_val;
static unsigned int MAC_MA32_127LR48_val;
static unsigned int MAC_MA32_127LR47_val;
static unsigned int MAC_MA32_127LR46_val;
static unsigned int MAC_MA32_127LR45_val;
static unsigned int MAC_MA32_127LR44_val;
static unsigned int MAC_MA32_127LR43_val;
static unsigned int MAC_MA32_127LR42_val;
static unsigned int MAC_MA32_127LR41_val;
static unsigned int MAC_MA32_127LR40_val;
static unsigned int MAC_MA32_127LR39_val;
static unsigned int MAC_MA32_127LR38_val;
static unsigned int MAC_MA32_127LR37_val;
static unsigned int MAC_MA32_127LR36_val;
static unsigned int MAC_MA32_127LR35_val;
static unsigned int MAC_MA32_127LR34_val;
static unsigned int MAC_MA32_127LR33_val;
static unsigned int MAC_MA32_127LR32_val;
static unsigned int MAC_MA32_127HR127_val;
static unsigned int MAC_MA32_127HR126_val;
static unsigned int MAC_MA32_127HR125_val;
static unsigned int MAC_MA32_127HR124_val;
static unsigned int MAC_MA32_127HR123_val;
static unsigned int MAC_MA32_127HR122_val;
static unsigned int MAC_MA32_127HR121_val;
static unsigned int MAC_MA32_127HR120_val;
static unsigned int MAC_MA32_127HR119_val;
static unsigned int MAC_MA32_127HR118_val;
static unsigned int MAC_MA32_127HR117_val;
static unsigned int MAC_MA32_127HR116_val;
static unsigned int MAC_MA32_127HR115_val;
static unsigned int MAC_MA32_127HR114_val;
static unsigned int MAC_MA32_127HR113_val;
static unsigned int MAC_MA32_127HR112_val;
static unsigned int MAC_MA32_127HR111_val;
static unsigned int MAC_MA32_127HR110_val;
static unsigned int MAC_MA32_127HR109_val;
static unsigned int MAC_MA32_127HR108_val;
static unsigned int MAC_MA32_127HR107_val;
static unsigned int MAC_MA32_127HR106_val;
static unsigned int MAC_MA32_127HR105_val;
static unsigned int MAC_MA32_127HR104_val;
static unsigned int MAC_MA32_127HR103_val;
static unsigned int MAC_MA32_127HR102_val;
static unsigned int MAC_MA32_127HR101_val;
static unsigned int MAC_MA32_127HR100_val;
static unsigned int MAC_MA32_127HR99_val;
static unsigned int MAC_MA32_127HR98_val;
static unsigned int MAC_MA32_127HR97_val;
static unsigned int MAC_MA32_127HR96_val;
static unsigned int MAC_MA32_127HR95_val;
static unsigned int MAC_MA32_127HR94_val;
static unsigned int MAC_MA32_127HR93_val;
static unsigned int MAC_MA32_127HR92_val;
static unsigned int MAC_MA32_127HR91_val;
static unsigned int MAC_MA32_127HR90_val;
static unsigned int MAC_MA32_127HR89_val;
static unsigned int MAC_MA32_127HR88_val;
static unsigned int MAC_MA32_127HR87_val;
static unsigned int MAC_MA32_127HR86_val;
static unsigned int MAC_MA32_127HR85_val;
static unsigned int MAC_MA32_127HR84_val;
static unsigned int MAC_MA32_127HR83_val;
static unsigned int MAC_MA32_127HR82_val;
static unsigned int MAC_MA32_127HR81_val;
static unsigned int MAC_MA32_127HR80_val;
static unsigned int MAC_MA32_127HR79_val;
static unsigned int MAC_MA32_127HR78_val;
static unsigned int MAC_MA32_127HR77_val;
static unsigned int MAC_MA32_127HR76_val;
static unsigned int MAC_MA32_127HR75_val;
static unsigned int MAC_MA32_127HR74_val;
static unsigned int MAC_MA32_127HR73_val;
static unsigned int MAC_MA32_127HR72_val;
static unsigned int MAC_MA32_127HR71_val;
static unsigned int MAC_MA32_127HR70_val;
static unsigned int MAC_MA32_127HR69_val;
static unsigned int MAC_MA32_127HR68_val;
static unsigned int MAC_MA32_127HR67_val;
static unsigned int MAC_MA32_127HR66_val;
static unsigned int MAC_MA32_127HR65_val;
static unsigned int MAC_MA32_127HR64_val;
static unsigned int MAC_MA32_127HR63_val;
static unsigned int MAC_MA32_127HR62_val;
static unsigned int MAC_MA32_127HR61_val;
static unsigned int MAC_MA32_127HR60_val;
static unsigned int MAC_MA32_127HR59_val;
static unsigned int MAC_MA32_127HR58_val;
static unsigned int MAC_MA32_127HR57_val;
static unsigned int MAC_MA32_127HR56_val;
static unsigned int MAC_MA32_127HR55_val;
static unsigned int MAC_MA32_127HR54_val;
static unsigned int MAC_MA32_127HR53_val;
static unsigned int MAC_MA32_127HR52_val;
static unsigned int MAC_MA32_127HR51_val;
static unsigned int MAC_MA32_127HR50_val;
static unsigned int MAC_MA32_127HR49_val;
static unsigned int MAC_MA32_127HR48_val;
static unsigned int MAC_MA32_127HR47_val;
static unsigned int MAC_MA32_127HR46_val;
static unsigned int MAC_MA32_127HR45_val;
static unsigned int MAC_MA32_127HR44_val;
static unsigned int MAC_MA32_127HR43_val;
static unsigned int MAC_MA32_127HR42_val;
static unsigned int MAC_MA32_127HR41_val;
static unsigned int MAC_MA32_127HR40_val;
static unsigned int MAC_MA32_127HR39_val;
static unsigned int MAC_MA32_127HR38_val;
static unsigned int MAC_MA32_127HR37_val;
static unsigned int MAC_MA32_127HR36_val;
static unsigned int MAC_MA32_127HR35_val;
static unsigned int MAC_MA32_127HR34_val;
static unsigned int MAC_MA32_127HR33_val;
static unsigned int MAC_MA32_127HR32_val;
static unsigned int MAC_MA1_31LR31_val;
static unsigned int MAC_MA1_31LR30_val;
static unsigned int MAC_MA1_31LR29_val;
static unsigned int MAC_MA1_31LR28_val;
static unsigned int MAC_MA1_31LR27_val;
static unsigned int MAC_MA1_31LR26_val;
static unsigned int MAC_MA1_31LR25_val;
static unsigned int MAC_MA1_31LR24_val;
static unsigned int MAC_MA1_31LR23_val;
static unsigned int MAC_MA1_31LR22_val;
static unsigned int MAC_MA1_31LR21_val;
static unsigned int MAC_MA1_31LR20_val;
static unsigned int MAC_MA1_31LR19_val;
static unsigned int MAC_MA1_31LR18_val;
static unsigned int MAC_MA1_31LR17_val;
static unsigned int MAC_MA1_31LR16_val;
static unsigned int MAC_MA1_31LR15_val;
static unsigned int MAC_MA1_31LR14_val;
static unsigned int MAC_MA1_31LR13_val;
static unsigned int MAC_MA1_31LR12_val;
static unsigned int MAC_MA1_31LR11_val;
static unsigned int MAC_MA1_31LR10_val;
static unsigned int MAC_MA1_31LR9_val;
static unsigned int MAC_MA1_31LR8_val;
static unsigned int MAC_MA1_31LR7_val;
static unsigned int MAC_MA1_31LR6_val;
static unsigned int MAC_MA1_31LR5_val;
static unsigned int MAC_MA1_31LR4_val;
static unsigned int MAC_MA1_31LR3_val;
static unsigned int MAC_MA1_31LR2_val;
static unsigned int MAC_MA1_31LR1_val;
static unsigned int MAC_MA1_31HR31_val;
static unsigned int MAC_MA1_31HR30_val;
static unsigned int MAC_MA1_31HR29_val;
static unsigned int MAC_MA1_31HR28_val;
static unsigned int MAC_MA1_31HR27_val;
static unsigned int MAC_MA1_31HR26_val;
static unsigned int MAC_MA1_31HR25_val;
static unsigned int MAC_MA1_31HR24_val;
static unsigned int MAC_MA1_31HR23_val;
static unsigned int MAC_MA1_31HR22_val;
static unsigned int MAC_MA1_31HR21_val;
static unsigned int MAC_MA1_31HR20_val;
static unsigned int MAC_MA1_31HR19_val;
static unsigned int MAC_MA1_31HR18_val;
static unsigned int MAC_MA1_31HR17_val;
static unsigned int MAC_MA1_31HR16_val;
static unsigned int MAC_MA1_31HR15_val;
static unsigned int MAC_MA1_31HR14_val;
static unsigned int MAC_MA1_31HR13_val;
static unsigned int MAC_MA1_31HR12_val;
static unsigned int MAC_MA1_31HR11_val;
static unsigned int MAC_MA1_31HR10_val;
static unsigned int MAC_MA1_31HR9_val;
static unsigned int MAC_MA1_31HR8_val;
static unsigned int MAC_MA1_31HR7_val;
static unsigned int MAC_MA1_31HR6_val;
static unsigned int MAC_MA1_31HR5_val;
static unsigned int MAC_MA1_31HR4_val;
static unsigned int MAC_MA1_31HR3_val;
static unsigned int MAC_MA1_31HR2_val;
static unsigned int MAC_MA1_31HR1_val;
static unsigned int mac_arpa_val;
static unsigned int MAC_L3A3R7_val;
static unsigned int MAC_L3A3R6_val;
static unsigned int MAC_L3A3R5_val;
static unsigned int MAC_L3A3R4_val;
static unsigned int MAC_L3A3R3_val;
static unsigned int MAC_L3A3R2_val;
static unsigned int MAC_L3A3R1_val;
static unsigned int MAC_L3A3R0_val;
static unsigned int MAC_L3A2R7_val;
static unsigned int MAC_L3A2R6_val;
static unsigned int MAC_L3A2R5_val;
static unsigned int MAC_L3A2R4_val;
static unsigned int MAC_L3A2R3_val;
static unsigned int MAC_L3A2R2_val;
static unsigned int MAC_L3A2R1_val;
static unsigned int MAC_L3A2R0_val;
static unsigned int MAC_L3A1R7_val;
static unsigned int MAC_L3A1R6_val;
static unsigned int MAC_L3A1R5_val;
static unsigned int MAC_L3A1R4_val;
static unsigned int MAC_L3A1R3_val;
static unsigned int MAC_L3A1R2_val;
static unsigned int MAC_L3A1R1_val;
static unsigned int MAC_L3A1R0_val;
static unsigned int MAC_L3A0R7_val;
static unsigned int MAC_L3A0R6_val;
static unsigned int MAC_L3A0R5_val;
static unsigned int MAC_L3A0R4_val;
static unsigned int MAC_L3A0R3_val;
static unsigned int MAC_L3A0R2_val;
static unsigned int MAC_L3A0R1_val;
static unsigned int MAC_L3A0R0_val;
static unsigned int MAC_L4AR7_val;
static unsigned int MAC_L4AR6_val;
static unsigned int MAC_L4AR5_val;
static unsigned int MAC_L4AR4_val;
static unsigned int MAC_L4AR3_val;
static unsigned int MAC_L4AR2_val;
static unsigned int MAC_L4AR1_val;
static unsigned int MAC_L4AR0_val;
static unsigned int MAC_L3L4CR7_val;
static unsigned int MAC_L3L4CR6_val;
static unsigned int MAC_L3L4CR5_val;
static unsigned int MAC_L3L4CR4_val;
static unsigned int MAC_L3L4CR3_val;
static unsigned int MAC_L3L4CR2_val;
static unsigned int MAC_L3L4CR1_val;
static unsigned int MAC_L3L4CR0_val;
static unsigned int mac_gpios_val;
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
static unsigned int MAC_SPI2r_val;
static unsigned int MAC_SPI1r_val;
static unsigned int MAC_SPI0r_val;
static unsigned int mac_pto_cr_val;
static unsigned int MAC_PPS_WIDTH3_val;
static unsigned int MAC_PPS_WIDTH2_val;
static unsigned int MAC_PPS_WIDTH1_val;
static unsigned int MAC_PPS_WIDTH0_val;
static unsigned int MAC_PPS_INTVAL3_val;
static unsigned int MAC_PPS_INTVAL2_val;
static unsigned int MAC_PPS_INTVAL1_val;
static unsigned int MAC_PPS_INTVAL0_val;
static unsigned int MAC_PPS_TTNS3_val;
static unsigned int MAC_PPS_TTNS2_val;
static unsigned int MAC_PPS_TTNS1_val;
static unsigned int MAC_PPS_TTNS0_val;
static unsigned int MAC_PPS_TTS3_val;
static unsigned int MAC_PPS_TTS2_val;
static unsigned int MAC_PPS_TTS1_val;
static unsigned int MAC_PPS_TTS0_val;
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
static unsigned int MTL_QRCR7_val;
static unsigned int MTL_QRCR6_val;
static unsigned int MTL_QRCR5_val;
static unsigned int MTL_QRCR4_val;
static unsigned int MTL_QRCR3_val;
static unsigned int MTL_QRCR2_val;
static unsigned int MTL_QRCR1_val;
static unsigned int MTL_QRDR7_val;
static unsigned int MTL_QRDR6_val;
static unsigned int MTL_QRDR5_val;
static unsigned int MTL_QRDR4_val;
static unsigned int MTL_QRDR3_val;
static unsigned int MTL_QRDR2_val;
static unsigned int MTL_QRDR1_val;
static unsigned int MTL_QOCR7_val;
static unsigned int MTL_QOCR6_val;
static unsigned int MTL_QOCR5_val;
static unsigned int MTL_QOCR4_val;
static unsigned int MTL_QOCR3_val;
static unsigned int MTL_QOCR2_val;
static unsigned int MTL_QOCR1_val;
static unsigned int MTL_QROMR7_val;
static unsigned int MTL_QROMR6_val;
static unsigned int MTL_QROMR5_val;
static unsigned int MTL_QROMR4_val;
static unsigned int MTL_QROMR3_val;
static unsigned int MTL_QROMR2_val;
static unsigned int MTL_QROMR1_val;
static unsigned int MTL_QLCR7_val;
static unsigned int MTL_QLCR6_val;
static unsigned int MTL_QLCR5_val;
static unsigned int MTL_QLCR4_val;
static unsigned int MTL_QLCR3_val;
static unsigned int MTL_QLCR2_val;
static unsigned int MTL_QLCR1_val;
static unsigned int MTL_QHCR7_val;
static unsigned int MTL_QHCR6_val;
static unsigned int MTL_QHCR5_val;
static unsigned int MTL_QHCR4_val;
static unsigned int MTL_QHCR3_val;
static unsigned int MTL_QHCR2_val;
static unsigned int MTL_QHCR1_val;
static unsigned int MTL_QSSCR7_val;
static unsigned int MTL_QSSCR6_val;
static unsigned int MTL_QSSCR5_val;
static unsigned int MTL_QSSCR4_val;
static unsigned int MTL_QSSCR3_val;
static unsigned int MTL_QSSCR2_val;
static unsigned int MTL_QSSCR1_val;
static unsigned int MTL_QW7_val;
static unsigned int MTL_QW6_val;
static unsigned int MTL_QW5_val;
static unsigned int MTL_QW4_val;
static unsigned int MTL_QW3_val;
static unsigned int MTL_QW2_val;
static unsigned int MTL_QW1_val;
static unsigned int MTL_QESR7_val;
static unsigned int MTL_QESR6_val;
static unsigned int MTL_QESR5_val;
static unsigned int MTL_QESR4_val;
static unsigned int MTL_QESR3_val;
static unsigned int MTL_QESR2_val;
static unsigned int MTL_QESR1_val;
static unsigned int MTL_QECR7_val;
static unsigned int MTL_QECR6_val;
static unsigned int MTL_QECR5_val;
static unsigned int MTL_QECR4_val;
static unsigned int MTL_QECR3_val;
static unsigned int MTL_QECR2_val;
static unsigned int MTL_QECR1_val;
static unsigned int MTL_QTDR7_val;
static unsigned int MTL_QTDR6_val;
static unsigned int MTL_QTDR5_val;
static unsigned int MTL_QTDR4_val;
static unsigned int MTL_QTDR3_val;
static unsigned int MTL_QTDR2_val;
static unsigned int MTL_QTDR1_val;
static unsigned int MTL_QUCR7_val;
static unsigned int MTL_QUCR6_val;
static unsigned int MTL_QUCR5_val;
static unsigned int MTL_QUCR4_val;
static unsigned int MTL_QUCR3_val;
static unsigned int MTL_QUCR2_val;
static unsigned int MTL_QUCR1_val;
static unsigned int MTL_QTOMR7_val;
static unsigned int MTL_QTOMR6_val;
static unsigned int MTL_QTOMR5_val;
static unsigned int MTL_QTOMR4_val;
static unsigned int MTL_QTOMR3_val;
static unsigned int MTL_QTOMR2_val;
static unsigned int MTL_QTOMR1_val;
static unsigned int mac_pmtcsr_val;
static unsigned int mmc_rxicmp_err_octets_val;
static unsigned int mmc_rxicmp_gd_octets_val;
static unsigned int mmc_rxtcp_err_octets_val;
static unsigned int mmc_rxtcp_gd_octets_val;
static unsigned int mmc_rxudp_err_octets_val;
static unsigned int mmc_rxudp_gd_octets_val;
static unsigned int MMC_RXIPV6_nopay_octets_val;
static unsigned int MMC_RXIPV6_hdrerr_octets_val;
static unsigned int MMC_RXIPV6_gd_octets_val;
static unsigned int MMC_RXIPV4_udsbl_octets_val;
static unsigned int MMC_RXIPV4_frag_octets_val;
static unsigned int MMC_RXIPV4_nopay_octets_val;
static unsigned int MMC_RXIPV4_hdrerr_octets_val;
static unsigned int MMC_RXIPV4_gd_octets_val;
static unsigned int mmc_rxicmp_err_pkts_val;
static unsigned int mmc_rxicmp_gd_pkts_val;
static unsigned int mmc_rxtcp_err_pkts_val;
static unsigned int mmc_rxtcp_gd_pkts_val;
static unsigned int mmc_rxudp_err_pkts_val;
static unsigned int mmc_rxudp_gd_pkts_val;
static unsigned int MMC_RXIPV6_nopay_pkts_val;
static unsigned int MMC_RXIPV6_hdrerr_pkts_val;
static unsigned int MMC_RXIPV6_gd_pkts_val;
static unsigned int MMC_RXIPV4_ubsbl_pkts_val;
static unsigned int MMC_RXIPV4_frag_pkts_val;
static unsigned int MMC_RXIPV4_nopay_pkts_val;
static unsigned int MMC_RXIPV4_hdrerr_pkts_val;
static unsigned int MMC_RXIPV4_gd_pkts_val;
static unsigned int mmc_rxctrlpackets_g_val;
static unsigned int mmc_rxrcverror_val;
static unsigned int mmc_rxwatchdogerror_val;
static unsigned int mmc_rxvlanpackets_gb_val;
static unsigned int mmc_rxfifooverflow_val;
static unsigned int mmc_rxpausepackets_val;
static unsigned int mmc_rxoutofrangetype_val;
static unsigned int mmc_rxlengtherror_val;
static unsigned int mmc_rxunicastpackets_g_val;
static unsigned int MMC_RX1024tomaxoctets_gb_val;
static unsigned int MMC_RX512TO1023octets_gb_val;
static unsigned int MMC_RX256TO511octets_gb_val;
static unsigned int MMC_RX128TO255octets_gb_val;
static unsigned int MMC_RX65TO127octets_gb_val;
static unsigned int MMC_RX64octets_gb_val;
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
static unsigned int MMC_TX1024tomaxoctets_gb_val;
static unsigned int MMC_TX512TO1023octets_gb_val;
static unsigned int MMC_TX256TO511octets_gb_val;
static unsigned int MMC_TX128TO255octets_gb_val;
static unsigned int MMC_TX65TO127octets_gb_val;
static unsigned int MMC_TX64octets_gb_val;
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
static unsigned int MAC_MA1lr_val;
static unsigned int MAC_MA1hr_val;
static unsigned int MAC_MA0lr_val;
static unsigned int MAC_MA0hr_val;
static unsigned int mac_gpior_val;
static unsigned int mac_gmiidr_val;
static unsigned int mac_gmiiar_val;
static unsigned int MAC_HFR2_val;
static unsigned int MAC_HFR1_val;
static unsigned int MAC_HFR0_val;
static unsigned int mac_mdr_val;
static unsigned int mac_vr_val;
static unsigned int MAC_HTR7_val;
static unsigned int MAC_HTR6_val;
static unsigned int MAC_HTR5_val;
static unsigned int MAC_HTR4_val;
static unsigned int MAC_HTR3_val;
static unsigned int MAC_HTR2_val;
static unsigned int MAC_HTR1_val;
static unsigned int MAC_HTR0_val;
static unsigned int DMA_RIWTR7_val;
static unsigned int DMA_RIWTR6_val;
static unsigned int DMA_RIWTR5_val;
static unsigned int DMA_RIWTR4_val;
static unsigned int DMA_RIWTR3_val;
static unsigned int DMA_RIWTR2_val;
static unsigned int DMA_RIWTR1_val;
static unsigned int DMA_RIWTR0_val;
static unsigned int DMA_RDRLR7_val;
static unsigned int DMA_RDRLR6_val;
static unsigned int DMA_RDRLR5_val;
static unsigned int DMA_RDRLR4_val;
static unsigned int DMA_RDRLR3_val;
static unsigned int DMA_RDRLR2_val;
static unsigned int DMA_RDRLR1_val;
static unsigned int DMA_RDRLR0_val;
static unsigned int DMA_TDRLR7_val;
static unsigned int DMA_TDRLR6_val;
static unsigned int DMA_TDRLR5_val;
static unsigned int DMA_TDRLR4_val;
static unsigned int DMA_TDRLR3_val;
static unsigned int DMA_TDRLR2_val;
static unsigned int DMA_TDRLR1_val;
static unsigned int DMA_TDRLR0_val;
static unsigned int DMA_RDTP_RPDR7_val;
static unsigned int DMA_RDTP_RPDR6_val;
static unsigned int DMA_RDTP_RPDR5_val;
static unsigned int DMA_RDTP_RPDR4_val;
static unsigned int DMA_RDTP_RPDR3_val;
static unsigned int DMA_RDTP_RPDR2_val;
static unsigned int DMA_RDTP_RPDR1_val;
static unsigned int DMA_RDTP_RPDR0_val;
static unsigned int DMA_TDTP_TPDR7_val;
static unsigned int DMA_TDTP_TPDR6_val;
static unsigned int DMA_TDTP_TPDR5_val;
static unsigned int DMA_TDTP_TPDR4_val;
static unsigned int DMA_TDTP_TPDR3_val;
static unsigned int DMA_TDTP_TPDR2_val;
static unsigned int DMA_TDTP_TPDR1_val;
static unsigned int DMA_TDTP_TPDR0_val;
static uint64_t DMA_RDLAR7_val;
static uint64_t DMA_RDLAR6_val;
static uint64_t DMA_RDLAR5_val;
static uint64_t DMA_RDLAR4_val;
static uint64_t DMA_RDLAR3_val;
static uint64_t DMA_RDLAR2_val;
static uint64_t DMA_RDLAR1_val;
static uint64_t DMA_RDLAR0_val;
static uint64_t DMA_TDLAR7_val;
static uint64_t DMA_TDLAR6_val;
static uint64_t DMA_TDLAR5_val;
static uint64_t DMA_TDLAR4_val;
static uint64_t DMA_TDLAR3_val;
static uint64_t DMA_TDLAR2_val;
static uint64_t DMA_TDLAR1_val;
static uint64_t DMA_TDLAR0_val;
static unsigned int DMA_IER7_val;
static unsigned int DMA_IER6_val;
static unsigned int DMA_IER5_val;
static unsigned int DMA_IER4_val;
static unsigned int DMA_IER3_val;
static unsigned int DMA_IER2_val;
static unsigned int DMA_IER1_val;
static unsigned int DMA_IER0_val;
static unsigned int mac_imr_val;
static unsigned int mac_isr_val;
static unsigned int mtl_isr_val;
static unsigned int DMA_SR7_val;
static unsigned int DMA_SR6_val;
static unsigned int DMA_SR5_val;
static unsigned int DMA_SR4_val;
static unsigned int DMA_SR3_val;
static unsigned int DMA_SR2_val;
static unsigned int DMA_SR1_val;
static unsigned int DMA_SR0_val;
static unsigned int dma_isr_val;
static unsigned int DMA_DSR2_val;
static unsigned int DMA_DSR1_val;
static unsigned int DMA_DSR0_val;
static unsigned int MTL_Q0rdr_val;
static unsigned int MTL_Q0esr_val;
static unsigned int MTL_Q0tdr_val;
static unsigned int DMA_CHRBAR7_val;
static unsigned int DMA_CHRBAR6_val;
static unsigned int DMA_CHRBAR5_val;
static unsigned int DMA_CHRBAR4_val;
static unsigned int DMA_CHRBAR3_val;
static unsigned int DMA_CHRBAR2_val;
static unsigned int DMA_CHRBAR1_val;
static unsigned int DMA_CHRBAR0_val;
static unsigned int DMA_CHTBAR7_val;
static unsigned int DMA_CHTBAR6_val;
static unsigned int DMA_CHTBAR5_val;
static unsigned int DMA_CHTBAR4_val;
static unsigned int DMA_CHTBAR3_val;
static unsigned int DMA_CHTBAR2_val;
static unsigned int DMA_CHTBAR1_val;
static unsigned int DMA_CHTBAR0_val;
static unsigned int DMA_CHRDR7_val;
static unsigned int DMA_CHRDR6_val;
static unsigned int DMA_CHRDR5_val;
static unsigned int DMA_CHRDR4_val;
static unsigned int DMA_CHRDR3_val;
static unsigned int DMA_CHRDR2_val;
static unsigned int DMA_CHRDR1_val;
static unsigned int DMA_CHRDR0_val;
static unsigned int DMA_CHTDR7_val;
static unsigned int DMA_CHTDR6_val;
static unsigned int DMA_CHTDR5_val;
static unsigned int DMA_CHTDR4_val;
static unsigned int DMA_CHTDR3_val;
static unsigned int DMA_CHTDR2_val;
static unsigned int DMA_CHTDR1_val;
static unsigned int DMA_CHTDR0_val;
static unsigned int DMA_SFCSR7_val;
static unsigned int DMA_SFCSR6_val;
static unsigned int DMA_SFCSR5_val;
static unsigned int DMA_SFCSR4_val;
static unsigned int DMA_SFCSR3_val;
static unsigned int DMA_SFCSR2_val;
static unsigned int DMA_SFCSR1_val;
static unsigned int DMA_SFCSR0_val;
static unsigned int mac_ivlantirr_val;
static unsigned int mac_vlantirr_val;
static unsigned int mac_vlanhtr_val;
static unsigned int mac_vlantr_val;
static unsigned int dma_sbus_val;
static unsigned int dma_bmr_val;
static unsigned int MTL_Q0rcr_val;
static unsigned int MTL_Q0ocr_val;
static unsigned int MTL_Q0romr_val;
static unsigned int MTL_Q0qr_val;
static unsigned int MTL_Q0ecr_val;
static unsigned int MTL_Q0ucr_val;
static unsigned int MTL_Q0tomr_val;
static unsigned int MTL_RQDCM1r_val;
static unsigned int MTL_RQDCM0r_val;
static unsigned int mtl_fddr_val;
static unsigned int mtl_fdacs_val;
static unsigned int mtl_omr_val;
static unsigned int MAC_RQC3r_val;
static unsigned int MAC_RQC2r_val;
static unsigned int MAC_RQC1r_val;
static unsigned int MAC_RQC0r_val;
static unsigned int MAC_TQPM1r_val;
static unsigned int MAC_TQPM0r_val;
static unsigned int mac_rfcr_val;
static unsigned int MAC_QTFCR7_val;
static unsigned int MAC_QTFCR6_val;
static unsigned int MAC_QTFCR5_val;
static unsigned int MAC_QTFCR4_val;
static unsigned int MAC_QTFCR3_val;
static unsigned int MAC_QTFCR2_val;
static unsigned int MAC_QTFCR1_val;
static unsigned int MAC_Q0tfcr_val;
static unsigned int DMA_AXI4CR7_val;
static unsigned int DMA_AXI4CR6_val;
static unsigned int DMA_AXI4CR5_val;
static unsigned int DMA_AXI4CR4_val;
static unsigned int DMA_AXI4CR3_val;
static unsigned int DMA_AXI4CR2_val;
static unsigned int DMA_AXI4CR1_val;
static unsigned int DMA_AXI4CR0_val;
static unsigned int DMA_RCR7_val;
static unsigned int DMA_RCR6_val;
static unsigned int DMA_RCR5_val;
static unsigned int DMA_RCR4_val;
static unsigned int DMA_RCR3_val;
static unsigned int DMA_RCR2_val;
static unsigned int DMA_RCR1_val;
static unsigned int DMA_RCR0_val;
static unsigned int DMA_TCR7_val;
static unsigned int DMA_TCR6_val;
static unsigned int DMA_TCR5_val;
static unsigned int DMA_TCR4_val;
static unsigned int DMA_TCR3_val;
static unsigned int DMA_TCR2_val;
static unsigned int DMA_TCR1_val;
static unsigned int DMA_TCR0_val;
static unsigned int DMA_CR7_val;
static unsigned int DMA_CR6_val;
static unsigned int DMA_CR5_val;
static unsigned int DMA_CR4_val;
static unsigned int DMA_CR3_val;
static unsigned int DMA_CR2_val;
static unsigned int DMA_CR1_val;
static unsigned int DMA_CR0_val;
static unsigned int mac_wtr_val;
static unsigned int mac_mpfr_val;
static unsigned int mac_mecr_val;
static unsigned int mac_mcr_val;

/* For MII/GMII register read/write */
static unsigned int mii_bmcr_reg_val;
static unsigned int mii_bmsr_reg_val;
static unsigned int MII_PHYSID1_reg_val;
static unsigned int MII_PHYSID2_reg_val;
static unsigned int mii_advertise_reg_val;
static unsigned int mii_lpa_reg_val;
static unsigned int mii_expansion_reg_val;
static unsigned int auto_nego_np_reg_val;
static unsigned int mii_estatus_reg_val;
static unsigned int MII_CTRL1000_reg_val;
static unsigned int MII_STAT1000_reg_val;
static unsigned int phy_ctl_reg_val;
static unsigned int phy_sts_reg_val;
static unsigned int bcm_regs_val;
static unsigned int bcm_rx_err_cnt_reg_val;
static unsigned int bcm_false_carr_cnt_reg_val;
static unsigned int bcm_rx_not_ok_cnt_reg_val;
static unsigned int bcm_int_status_reg_val;
static unsigned int bcm_int_mask_reg_val;
static unsigned int BCM_10baset_reg_val;
static unsigned int bcm_pwr_mii_ctrl_reg_val;
static unsigned int bcm_misc_test_reg_val;
static unsigned int bcm_misc_ctrl_reg_val;
static unsigned int BCM_10baset_reg_val;
static unsigned int bcm_power_mii_ctrl_reg_val;
static unsigned int bcm_misc_test_reg_val;
static unsigned int bcm_misc_ctrl_reg_val;
static unsigned int bcm_pkt_cnt_reg_val;
static unsigned int bcm_eee_adv_reg_val;
static unsigned int bcm_eee_res_sts_reg_val;
static unsigned int bcm_lpi_counter_reg_val;

/* For controlling properties/features of the device */
static unsigned int feature_drop_tx_pktburstcnt_val = 1;

/* for mq */
static unsigned int qinx_val;

static unsigned int reg_offset_val;
static unsigned int gen_reg_val;
static unsigned int do_tx_align_tst_val;

void eqos_get_pdata(struct eqos_prv_data *prv_pdata)
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
		if ((buffer[j] == '\t') || (buffer[j]) == ' ')
			break;

		regname[i] = buffer[j];
		i++;

		j++;
		cnt--;
	}
	regname[i] = '\0';

	DBGPR("<-- get_reg_name\n");
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
		if ((buffer[j] == ' ') || (buffer[j] == '\t'))
			value_present = 1;

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

static ssize_t eqos_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	int ret = 0;
	char reg_name[50];
	char reg_value[25];
	unsigned long integer_value;

	DBGPR("--> eqos_write\n");

	if (count > DEBUGFS_MAX_SIZE)
		return -EINVAL;

	if (copy_from_user(debugfs_buf, buf, count)) {
		ret = -1;
	} else {
		get_reg_name(reg_name, debugfs_buf, count);
		get_reg_value(reg_value, debugfs_buf, count);
		ret = count;

		if (kstrtoul(reg_value, 16, &integer_value)) {
			pr_err("Invalid value specified for register write");
			return -EINVAL;
		}

		if (!strcmp(reg_name, "MAC_MA32_127LR127")) {
			MAC_MA32_127LR_WR(127, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR126")) {
			MAC_MA32_127LR_WR(126, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR125")) {
			MAC_MA32_127LR_WR(125, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR124")) {
			MAC_MA32_127LR_WR(124, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR123")) {
			MAC_MA32_127LR_WR(123, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR122")) {
			MAC_MA32_127LR_WR(122, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR121")) {
			MAC_MA32_127LR_WR(121, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR120")) {
			MAC_MA32_127LR_WR(120, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR119")) {
			MAC_MA32_127LR_WR(119, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR118")) {
			MAC_MA32_127LR_WR(118, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR117")) {
			MAC_MA32_127LR_WR(117, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR116")) {
			MAC_MA32_127LR_WR(116, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR115")) {
			MAC_MA32_127LR_WR(115, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR114")) {
			MAC_MA32_127LR_WR(114, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR113")) {
			MAC_MA32_127LR_WR(113, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR112")) {
			MAC_MA32_127LR_WR(112, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR111")) {
			MAC_MA32_127LR_WR(111, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR110")) {
			MAC_MA32_127LR_WR(110, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR109")) {
			MAC_MA32_127LR_WR(109, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR108")) {
			MAC_MA32_127LR_WR(108, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR107")) {
			MAC_MA32_127LR_WR(107, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR106")) {
			MAC_MA32_127LR_WR(106, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR105")) {
			MAC_MA32_127LR_WR(105, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR104")) {
			MAC_MA32_127LR_WR(104, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR103")) {
			MAC_MA32_127LR_WR(103, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR102")) {
			MAC_MA32_127LR_WR(102, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR101")) {
			MAC_MA32_127LR_WR(101, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR100")) {
			MAC_MA32_127LR_WR(100, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR99")) {
			MAC_MA32_127LR_WR(99, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR98")) {
			MAC_MA32_127LR_WR(98, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR97")) {
			MAC_MA32_127LR_WR(97, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR96")) {
			MAC_MA32_127LR_WR(96, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR95")) {
			MAC_MA32_127LR_WR(95, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR94")) {
			MAC_MA32_127LR_WR(94, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR93")) {
			MAC_MA32_127LR_WR(93, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR92")) {
			MAC_MA32_127LR_WR(92, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR91")) {
			MAC_MA32_127LR_WR(91, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR90")) {
			MAC_MA32_127LR_WR(90, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR89")) {
			MAC_MA32_127LR_WR(89, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR88")) {
			MAC_MA32_127LR_WR(88, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR87")) {
			MAC_MA32_127LR_WR(87, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR86")) {
			MAC_MA32_127LR_WR(86, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR85")) {
			MAC_MA32_127LR_WR(85, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR84")) {
			MAC_MA32_127LR_WR(84, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR83")) {
			MAC_MA32_127LR_WR(83, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR82")) {
			MAC_MA32_127LR_WR(82, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR81")) {
			MAC_MA32_127LR_WR(81, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR80")) {
			MAC_MA32_127LR_WR(80, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR79")) {
			MAC_MA32_127LR_WR(79, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR78")) {
			MAC_MA32_127LR_WR(78, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR77")) {
			MAC_MA32_127LR_WR(77, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR76")) {
			MAC_MA32_127LR_WR(76, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR75")) {
			MAC_MA32_127LR_WR(75, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR74")) {
			MAC_MA32_127LR_WR(74, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR73")) {
			MAC_MA32_127LR_WR(73, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR72")) {
			MAC_MA32_127LR_WR(72, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR71")) {
			MAC_MA32_127LR_WR(71, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR70")) {
			MAC_MA32_127LR_WR(70, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR69")) {
			MAC_MA32_127LR_WR(69, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR68")) {
			MAC_MA32_127LR_WR(68, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR67")) {
			MAC_MA32_127LR_WR(67, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR66")) {
			MAC_MA32_127LR_WR(66, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR65")) {
			MAC_MA32_127LR_WR(65, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR64")) {
			MAC_MA32_127LR_WR(64, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR63")) {
			MAC_MA32_127LR_WR(63, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR62")) {
			MAC_MA32_127LR_WR(62, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR61")) {
			MAC_MA32_127LR_WR(61, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR60")) {
			MAC_MA32_127LR_WR(60, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR59")) {
			MAC_MA32_127LR_WR(59, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR58")) {
			MAC_MA32_127LR_WR(58, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR57")) {
			MAC_MA32_127LR_WR(57, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR56")) {
			MAC_MA32_127LR_WR(56, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR55")) {
			MAC_MA32_127LR_WR(55, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR54")) {
			MAC_MA32_127LR_WR(54, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR53")) {
			MAC_MA32_127LR_WR(53, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR52")) {
			MAC_MA32_127LR_WR(52, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR51")) {
			MAC_MA32_127LR_WR(51, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR50")) {
			MAC_MA32_127LR_WR(50, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR49")) {
			MAC_MA32_127LR_WR(49, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR48")) {
			MAC_MA32_127LR_WR(48, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR47")) {
			MAC_MA32_127LR_WR(47, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR46")) {
			MAC_MA32_127LR_WR(46, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR45")) {
			MAC_MA32_127LR_WR(45, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR44")) {
			MAC_MA32_127LR_WR(44, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR43")) {
			MAC_MA32_127LR_WR(43, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR42")) {
			MAC_MA32_127LR_WR(42, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR41")) {
			MAC_MA32_127LR_WR(41, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR40")) {
			MAC_MA32_127LR_WR(40, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR39")) {
			MAC_MA32_127LR_WR(39, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR38")) {
			MAC_MA32_127LR_WR(38, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR37")) {
			MAC_MA32_127LR_WR(37, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR36")) {
			MAC_MA32_127LR_WR(36, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR35")) {
			MAC_MA32_127LR_WR(35, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR34")) {
			MAC_MA32_127LR_WR(34, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR33")) {
			MAC_MA32_127LR_WR(33, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127LR32")) {
			MAC_MA32_127LR_WR(32, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR127")) {
			MAC_MA32_127HR_WR(127, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR126")) {
			MAC_MA32_127HR_WR(126, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR125")) {
			MAC_MA32_127HR_WR(125, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR124")) {
			MAC_MA32_127HR_WR(124, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR123")) {
			MAC_MA32_127HR_WR(123, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR122")) {
			MAC_MA32_127HR_WR(122, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR121")) {
			MAC_MA32_127HR_WR(121, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR120")) {
			MAC_MA32_127HR_WR(120, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR119")) {
			MAC_MA32_127HR_WR(119, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR118")) {
			MAC_MA32_127HR_WR(118, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR117")) {
			MAC_MA32_127HR_WR(117, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR116")) {
			MAC_MA32_127HR_WR(116, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR115")) {
			MAC_MA32_127HR_WR(115, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR114")) {
			MAC_MA32_127HR_WR(114, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR113")) {
			MAC_MA32_127HR_WR(113, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR112")) {
			MAC_MA32_127HR_WR(112, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR111")) {
			MAC_MA32_127HR_WR(111, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR110")) {
			MAC_MA32_127HR_WR(110, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR109")) {
			MAC_MA32_127HR_WR(109, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR108")) {
			MAC_MA32_127HR_WR(108, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR107")) {
			MAC_MA32_127HR_WR(107, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR106")) {
			MAC_MA32_127HR_WR(106, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR105")) {
			MAC_MA32_127HR_WR(105, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR104")) {
			MAC_MA32_127HR_WR(104, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR103")) {
			MAC_MA32_127HR_WR(103, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR102")) {
			MAC_MA32_127HR_WR(102, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR101")) {
			MAC_MA32_127HR_WR(101, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR100")) {
			MAC_MA32_127HR_WR(100, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR99")) {
			MAC_MA32_127HR_WR(99, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR98")) {
			MAC_MA32_127HR_WR(98, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR97")) {
			MAC_MA32_127HR_WR(97, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR96")) {
			MAC_MA32_127HR_WR(96, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR95")) {
			MAC_MA32_127HR_WR(95, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR94")) {
			MAC_MA32_127HR_WR(94, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR93")) {
			MAC_MA32_127HR_WR(93, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR92")) {
			MAC_MA32_127HR_WR(92, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR91")) {
			MAC_MA32_127HR_WR(91, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR90")) {
			MAC_MA32_127HR_WR(90, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR89")) {
			MAC_MA32_127HR_WR(89, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR88")) {
			MAC_MA32_127HR_WR(88, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR87")) {
			MAC_MA32_127HR_WR(87, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR86")) {
			MAC_MA32_127HR_WR(86, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR85")) {
			MAC_MA32_127HR_WR(85, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR84")) {
			MAC_MA32_127HR_WR(84, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR83")) {
			MAC_MA32_127HR_WR(83, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR82")) {
			MAC_MA32_127HR_WR(82, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR81")) {
			MAC_MA32_127HR_WR(81, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR80")) {
			MAC_MA32_127HR_WR(80, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR79")) {
			MAC_MA32_127HR_WR(79, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR78")) {
			MAC_MA32_127HR_WR(78, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR77")) {
			MAC_MA32_127HR_WR(77, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR76")) {
			MAC_MA32_127HR_WR(76, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR75")) {
			MAC_MA32_127HR_WR(75, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR74")) {
			MAC_MA32_127HR_WR(74, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR73")) {
			MAC_MA32_127HR_WR(73, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR72")) {
			MAC_MA32_127HR_WR(72, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR71")) {
			MAC_MA32_127HR_WR(71, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR70")) {
			MAC_MA32_127HR_WR(70, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR69")) {
			MAC_MA32_127HR_WR(69, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR68")) {
			MAC_MA32_127HR_WR(68, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR67")) {
			MAC_MA32_127HR_WR(67, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR66")) {
			MAC_MA32_127HR_WR(66, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR65")) {
			MAC_MA32_127HR_WR(65, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR64")) {
			MAC_MA32_127HR_WR(64, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR63")) {
			MAC_MA32_127HR_WR(63, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR62")) {
			MAC_MA32_127HR_WR(62, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR61")) {
			MAC_MA32_127HR_WR(61, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR60")) {
			MAC_MA32_127HR_WR(60, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR59")) {
			MAC_MA32_127HR_WR(59, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR58")) {
			MAC_MA32_127HR_WR(58, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR57")) {
			MAC_MA32_127HR_WR(57, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR56")) {
			MAC_MA32_127HR_WR(56, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR55")) {
			MAC_MA32_127HR_WR(55, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR54")) {
			MAC_MA32_127HR_WR(54, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR53")) {
			MAC_MA32_127HR_WR(53, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR52")) {
			MAC_MA32_127HR_WR(52, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR51")) {
			MAC_MA32_127HR_WR(51, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR50")) {
			MAC_MA32_127HR_WR(50, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR49")) {
			MAC_MA32_127HR_WR(49, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR48")) {
			MAC_MA32_127HR_WR(48, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR47")) {
			MAC_MA32_127HR_WR(47, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR46")) {
			MAC_MA32_127HR_WR(46, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR45")) {
			MAC_MA32_127HR_WR(45, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR44")) {
			MAC_MA32_127HR_WR(44, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR43")) {
			MAC_MA32_127HR_WR(43, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR42")) {
			MAC_MA32_127HR_WR(42, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR41")) {
			MAC_MA32_127HR_WR(41, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR40")) {
			MAC_MA32_127HR_WR(40, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR39")) {
			MAC_MA32_127HR_WR(39, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR38")) {
			MAC_MA32_127HR_WR(38, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR37")) {
			MAC_MA32_127HR_WR(37, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR36")) {
			MAC_MA32_127HR_WR(36, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR35")) {
			MAC_MA32_127HR_WR(35, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR34")) {
			MAC_MA32_127HR_WR(34, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR33")) {
			MAC_MA32_127HR_WR(33, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA32_127HR32")) {
			MAC_MA32_127HR_WR(32, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR31")) {
			MAC_MA1_31LR_WR(31, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR30")) {
			MAC_MA1_31LR_WR(30, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR29")) {
			MAC_MA1_31LR_WR(29, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR28")) {
			MAC_MA1_31LR_WR(28, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR27")) {
			MAC_MA1_31LR_WR(27, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR26")) {
			MAC_MA1_31LR_WR(26, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR25")) {
			MAC_MA1_31LR_WR(25, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR24")) {
			MAC_MA1_31LR_WR(24, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR23")) {
			MAC_MA1_31LR_WR(23, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR22")) {
			MAC_MA1_31LR_WR(22, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR21")) {
			MAC_MA1_31LR_WR(21, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR20")) {
			MAC_MA1_31LR_WR(20, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR19")) {
			MAC_MA1_31LR_WR(19, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR18")) {
			MAC_MA1_31LR_WR(18, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR17")) {
			MAC_MA1_31LR_WR(17, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR16")) {
			MAC_MA1_31LR_WR(16, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR15")) {
			MAC_MA1_31LR_WR(15, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR14")) {
			MAC_MA1_31LR_WR(14, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR13")) {
			MAC_MA1_31LR_WR(13, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR12")) {
			MAC_MA1_31LR_WR(12, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR11")) {
			MAC_MA1_31LR_WR(11, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR10")) {
			MAC_MA1_31LR_WR(10, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR9")) {
			MAC_MA1_31LR_WR(9, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR8")) {
			MAC_MA1_31LR_WR(8, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR7")) {
			MAC_MA1_31LR_WR(7, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR6")) {
			MAC_MA1_31LR_WR(6, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR5")) {
			MAC_MA1_31LR_WR(5, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR4")) {
			MAC_MA1_31LR_WR(4, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR3")) {
			MAC_MA1_31LR_WR(3, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR2")) {
			MAC_MA1_31LR_WR(2, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31LR1")) {
			MAC_MA1_31LR_WR(1, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR31")) {
			MAC_MA1_31HR_WR(31, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR30")) {
			MAC_MA1_31HR_WR(30, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR29")) {
			MAC_MA1_31HR_WR(29, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR28")) {
			MAC_MA1_31HR_WR(28, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR27")) {
			MAC_MA1_31HR_WR(27, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR26")) {
			MAC_MA1_31HR_WR(26, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR25")) {
			MAC_MA1_31HR_WR(25, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR24")) {
			MAC_MA1_31HR_WR(24, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR23")) {
			MAC_MA1_31HR_WR(23, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR22")) {
			MAC_MA1_31HR_WR(22, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR21")) {
			MAC_MA1_31HR_WR(21, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR20")) {
			MAC_MA1_31HR_WR(20, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR19")) {
			MAC_MA1_31HR_WR(19, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR18")) {
			MAC_MA1_31HR_WR(18, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR17")) {
			MAC_MA1_31HR_WR(17, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR16")) {
			MAC_MA1_31HR_WR(16, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR15")) {
			MAC_MA1_31HR_WR(15, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR14")) {
			MAC_MA1_31HR_WR(14, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR13")) {
			MAC_MA1_31HR_WR(13, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR12")) {
			MAC_MA1_31HR_WR(12, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR11")) {
			MAC_MA1_31HR_WR(11, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR10")) {
			MAC_MA1_31HR_WR(10, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR9")) {
			MAC_MA1_31HR_WR(9, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR8")) {
			MAC_MA1_31HR_WR(8, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR7")) {
			MAC_MA1_31HR_WR(7, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR6")) {
			MAC_MA1_31HR_WR(6, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR5")) {
			MAC_MA1_31HR_WR(5, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR4")) {
			MAC_MA1_31HR_WR(4, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR3")) {
			MAC_MA1_31HR_WR(3, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR2")) {
			MAC_MA1_31HR_WR(2, integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1_31HR1")) {
			MAC_MA1_31HR_WR(1, integer_value);
		} else if (!strcmp(reg_name, "MAC_ARPA")) {
			MAC_ARPA_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A3R7")) {
			MAC_L3A3R7_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A3R6")) {
			MAC_L3A3R6_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A3R5")) {
			MAC_L3A3R5_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A3R4")) {
			MAC_L3A3R4_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A3R3")) {
			MAC_L3A3R3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A3R2")) {
			MAC_L3A3R2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A3R1")) {
			MAC_L3A3R1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A3R0")) {
			MAC_L3A3R0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A2R7")) {
			MAC_L3A2R7_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A2R6")) {
			MAC_L3A2R6_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A2R5")) {
			MAC_L3A2R5_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A2R4")) {
			MAC_L3A2R4_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A2R3")) {
			MAC_L3A2R3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A2R2")) {
			MAC_L3A2R2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A2R1")) {
			MAC_L3A2R1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A2R0")) {
			MAC_L3A2R0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A1R7")) {
			MAC_L3A1R7_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A1R6")) {
			MAC_L3A1R6_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A1R5")) {
			MAC_L3A1R5_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A1R4")) {
			MAC_L3A1R4_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A1R3")) {
			MAC_L3A1R3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A1R2")) {
			MAC_L3A1R2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A1R1")) {
			MAC_L3A1R1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A1R0")) {
			MAC_L3A1R0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A0R7")) {
			MAC_L3A0R7_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A0R6")) {
			MAC_L3A0R6_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A0R5")) {
			MAC_L3A0R5_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A0R4")) {
			MAC_L3A0R4_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A0R3")) {
			MAC_L3A0R3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A0R2")) {
			MAC_L3A0R2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A0R1")) {
			MAC_L3A0R1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3A0R0")) {
			MAC_L3A0R0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L4AR7")) {
			MAC_L4AR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L4AR6")) {
			MAC_L4AR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L4AR5")) {
			MAC_L4AR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L4AR4")) {
			MAC_L4AR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L4AR3")) {
			MAC_L4AR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L4AR2")) {
			MAC_L4AR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L4AR1")) {
			MAC_L4AR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L4AR0")) {
			MAC_L4AR0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3L4CR7")) {
			MAC_L3L4CR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3L4CR6")) {
			MAC_L3L4CR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3L4CR5")) {
			MAC_L3L4CR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3L4CR4")) {
			MAC_L3L4CR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3L4CR3")) {
			MAC_L3L4CR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3L4CR2")) {
			MAC_L3L4CR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3L4CR1")) {
			MAC_L3L4CR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_L3L4CR0")) {
			MAC_L3L4CR0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_GPIOS")) {
			MAC_GPIOS_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PCS")) {
			MAC_PCS_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_TES")) {
			pr_err("MAC_TES is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_AE")) {
			pr_err("MAC_AE is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_ALPA")) {
			pr_err("MAC_ALPA is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_AAD")) {
			MAC_AAD_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_ANS")) {
			pr_err("MAC_ANS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_ANC")) {
			MAC_ANC_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_LPC")) {
			MAC_LPC_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_LPS")) {
			MAC_LPS_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_LMIR")) {
			MAC_LMIR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_SPI2R")) {
			MAC_SPI2R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_SPI1R")) {
			MAC_SPI1R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_SPI0R")) {
			MAC_SPI0R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PTO_CR")) {
			MAC_PTO_CR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_WIDTH3")) {
			MAC_PPS_WIDTH3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_WIDTH2")) {
			MAC_PPS_WIDTH2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_WIDTH1")) {
			MAC_PPS_WIDTH1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_WIDTH0")) {
			MAC_PPS_WIDTH0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_INTVAL3")) {
			MAC_PPS_INTVAL3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_INTVAL2")) {
			MAC_PPS_INTVAL2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_INTVAL1")) {
			MAC_PPS_INTVAL1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_INTVAL0")) {
			MAC_PPS_INTVAL0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_TTNS3")) {
			MAC_PPS_TTNS3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_TTNS2")) {
			MAC_PPS_TTNS2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_TTNS1")) {
			MAC_PPS_TTNS1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_TTNS0")) {
			MAC_PPS_TTNS0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_TTS3")) {
			MAC_PPS_TTS3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_TTS2")) {
			MAC_PPS_TTS2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_TTS1")) {
			MAC_PPS_TTS1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPS_TTS0")) {
			MAC_PPS_TTS0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PPSC")) {
			MAC_PPSC_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_TEAC")) {
			MAC_TEAC_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_TIAC")) {
			MAC_TIAC_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_ATS")) {
			pr_err("MAC_ATS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_ATN")) {
			pr_err("MAC_ATN is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_AC")) {
			MAC_AC_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_TTN")) {
			pr_err("MAC_TTN is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_TTSN")) {
			pr_err("MAC_TTSN is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_TSR")) {
			pr_err("MAC_TSR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_STHWR")) {
			MAC_STHWR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_TAR")) {
			MAC_TAR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_STNSUR")) {
			MAC_STNSUR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_STSUR")) {
			MAC_STSUR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_STNSR")) {
			pr_err("MAC_STNSR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_STSR")) {
			pr_err("MAC_STSR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_SSIR")) {
			MAC_SSIR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_TCR")) {
			MAC_TCR_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_DSR")) {
			MTL_DSR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_RWPFFR")) {
			MAC_RWPFFR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_RTSR")) {
			pr_err("MAC_RTSR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_IER")) {
			MTL_IER_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QRCR7")) {
			MTL_QRCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QRCR6")) {
			MTL_QRCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QRCR5")) {
			MTL_QRCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QRCR4")) {
			MTL_QRCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QRCR3")) {
			MTL_QRCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QRCR2")) {
			MTL_QRCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QRCR1")) {
			MTL_QRCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QRDR7")) {
			pr_err("MTL_QRDR7 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QRDR6")) {
			pr_err("MTL_QRDR6 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QRDR5")) {
			pr_err("MTL_QRDR5 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QRDR4")) {
			pr_err("MTL_QRDR4 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QRDR3")) {
			pr_err("MTL_QRDR3 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QRDR2")) {
			pr_err("MTL_QRDR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QRDR1")) {
			pr_err("MTL_QRDR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QOCR7")) {
			MTL_QOCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QOCR6")) {
			MTL_QOCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QOCR5")) {
			MTL_QOCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QOCR4")) {
			MTL_QOCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QOCR3")) {
			MTL_QOCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QOCR2")) {
			MTL_QOCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QOCR1")) {
			MTL_QOCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QROMR7")) {
			MTL_QROMR_WR(7, integer_value);
		} else if (!strcmp(reg_name, "MTL_QROMR6")) {
			MTL_QROMR_WR(6, integer_value);
		} else if (!strcmp(reg_name, "MTL_QROMR5")) {
			MTL_QROMR_WR(5, integer_value);
		} else if (!strcmp(reg_name, "MTL_QROMR4")) {
			MTL_QROMR_WR(4, integer_value);
		} else if (!strcmp(reg_name, "MTL_QROMR3")) {
			MTL_QROMR_WR(3, integer_value);
		} else if (!strcmp(reg_name, "MTL_QROMR2")) {
			MTL_QROMR_WR(2, integer_value);
		} else if (!strcmp(reg_name, "MTL_QROMR1")) {
			MTL_QROMR_WR(1, integer_value);
		} else if (!strcmp(reg_name, "MTL_QLCR7")) {
			MTL_QLCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QLCR6")) {
			MTL_QLCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QLCR5")) {
			MTL_QLCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QLCR4")) {
			MTL_QLCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QLCR3")) {
			MTL_QLCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QLCR2")) {
			MTL_QLCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QLCR1")) {
			MTL_QLCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QHCR7")) {
			MTL_QHCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QHCR6")) {
			MTL_QHCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QHCR5")) {
			MTL_QHCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QHCR4")) {
			MTL_QHCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QHCR3")) {
			MTL_QHCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QHCR2")) {
			MTL_QHCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QHCR1")) {
			MTL_QHCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QSSCR7")) {
			MTL_QSSCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QSSCR6")) {
			MTL_QSSCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QSSCR5")) {
			MTL_QSSCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QSSCR4")) {
			MTL_QSSCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QSSCR3")) {
			MTL_QSSCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QSSCR2")) {
			MTL_QSSCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QSSCR1")) {
			MTL_QSSCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QW7")) {
			MTL_QW7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QW6")) {
			MTL_QW6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QW5")) {
			MTL_QW5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QW4")) {
			MTL_QW4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QW3")) {
			MTL_QW3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QW2")) {
			MTL_QW2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QW1")) {
			MTL_QW1_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QESR7")) {
			pr_err("MTL_QESR7 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QESR6")) {
			pr_err("MTL_QESR6 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QESR5")) {
			pr_err("MTL_QESR5 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QESR4")) {
			pr_err("MTL_QESR4 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QESR3")) {
			pr_err("MTL_QESR3 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QESR2")) {
			pr_err("MTL_QESR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QESR1")) {
			pr_err("MTL_QESR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QECR7")) {
			MTL_QECR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QECR6")) {
			MTL_QECR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QECR5")) {
			MTL_QECR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QECR4")) {
			MTL_QECR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QECR3")) {
			MTL_QECR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QECR2")) {
			MTL_QECR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QECR1")) {
			MTL_QECR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QTDR7")) {
			pr_err("MTL_QTDR7 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QTDR6")) {
			pr_err("MTL_QTDR6 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QTDR5")) {
			pr_err("MTL_QTDR5 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QTDR4")) {
			pr_err("MTL_QTDR4 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QTDR3")) {
			pr_err("MTL_QTDR3 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QTDR2")) {
			pr_err("MTL_QTDR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QTDR1")) {
			pr_err("MTL_QTDR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_QUCR7")) {
			MTL_QUCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QUCR6")) {
			MTL_QUCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QUCR5")) {
			MTL_QUCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QUCR4")) {
			MTL_QUCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QUCR3")) {
			MTL_QUCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QUCR2")) {
			MTL_QUCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QUCR1")) {
			MTL_QUCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QTOMR7")) {
			MTL_QTOMR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QTOMR6")) {
			MTL_QTOMR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QTOMR5")) {
			MTL_QTOMR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QTOMR4")) {
			MTL_QTOMR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QTOMR3")) {
			MTL_QTOMR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QTOMR2")) {
			MTL_QTOMR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_QTOMR1")) {
			MTL_QTOMR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_PMTCSR")) {
			MAC_PMTCSR_WR(integer_value);
		} else if (!strcmp(reg_name, "MMC_RXICMP_ERR_OCTETS")) {
			pr_err("MMC_RXICMP_ERR_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXICMP_GD_OCTETS")) {
			pr_err("MMC_RXICMP_GD_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXTCP_ERR_OCTETS")) {
			pr_err("MMC_RXTCP_ERR_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXTCP_GD_OCTETS")) {
			pr_err("MMC_RXTCP_GD_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXUDP_ERR_OCTETS")) {
			pr_err("MMC_RXUDP_ERR_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXUDP_GD_OCTETS")) {
			pr_err("MMC_RXUDP_GD_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV6_NOPAY_OCTETS")) {
			pr_err("MMC_RXIPV6_NOPAY_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV6_HDRERR_OCTETS")) {
			pr_err("MMC_RXIPV6_HDRERR_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV6_GD_OCTETS")) {
			pr_err("MMC_RXIPV6_GD_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_UDSBL_OCTETS")) {
			pr_err("MMC_RXIPV4_UDSBL_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_FRAG_OCTETS")) {
			pr_err("MMC_RXIPV4_FRAG_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_NOPAY_OCTETS")) {
			pr_err("MMC_RXIPV4_NOPAY_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_HDRERR_OCTETS")) {
			pr_err("MMC_RXIPV4_HDRERR_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_GD_OCTETS")) {
			pr_err("MMC_RXIPV4_GD_OCTETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXICMP_ERR_PKTS")) {
			pr_err("MMC_RXICMP_ERR_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXICMP_GD_PKTS")) {
			pr_err("MMC_RXICMP_GD_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXTCP_ERR_PKTS")) {
			pr_err("MMC_RXTCP_ERR_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXTCP_GD_PKTS")) {
			pr_err("MMC_RXTCP_GD_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXUDP_ERR_PKTS")) {
			pr_err("MMC_RXUDP_ERR_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXUDP_GD_PKTS")) {
			pr_err("MMC_RXUDP_GD_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV6_NOPAY_PKTS")) {
			pr_err("MMC_RXIPV6_NOPAY_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV6_HDRERR_PKTS")) {
			pr_err("MMC_RXIPV6_HDRERR_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV6_GD_PKTS")) {
			pr_err("MMC_RXIPV6_GD_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_UBSBL_PKTS")) {
			pr_err("MMC_RXIPV4_UBSBL_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_FRAG_PKTS")) {
			pr_err("MMC_RXIPV4_FRAG_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_NOPAY_PKTS")) {
			pr_err("MMC_RXIPV4_NOPAY_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_HDRERR_PKTS")) {
			pr_err("MMC_RXIPV4_HDRERR_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXIPV4_GD_PKTS")) {
			pr_err("MMC_RXIPV4_GD_PKTS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXCTRLPACKETS_G")) {
			pr_err("MMC_RXCTRLPACKETS_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXRCVERROR")) {
			pr_err("MMC_RXRCVERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXWATCHDOGERROR")) {
			pr_err("MMC_RXWATCHDOGERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXVLANPACKETS_GB")) {
			pr_err("MMC_RXVLANPACKETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXFIFOOVERFLOW")) {
			pr_err("MMC_RXFIFOOVERFLOW is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXPAUSEPACKETS")) {
			pr_err("MMC_RXPAUSEPACKETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXOUTOFRANGETYPE")) {
			pr_err("MMC_RXOUTOFRANGETYPE is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXLENGTHERROR")) {
			pr_err("MMC_RXLENGTHERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXUNICASTPACKETS_G")) {
			pr_err("MMC_RXUNICASTPACKETS_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RX1024TOMAXOCTETS_GB")) {
			pr_err("MMC_RX1024TOMAXOCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RX512TO1023OCTETS_GB")) {
			pr_err("MMC_RX512TO1023OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RX256TO511OCTETS_GB")) {
			pr_err("MMC_RX256TO511OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RX128TO255OCTETS_GB")) {
			pr_err("MMC_RX128TO255OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RX65TO127OCTETS_GB")) {
			pr_err("MMC_RX65TO127OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RX64OCTETS_GB")) {
			pr_err("MMC_RX64OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXOVERSIZE_G")) {
			pr_err("MMC_RXOVERSIZE_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXUNDERSIZE_G")) {
			pr_err("MMC_RXUNDERSIZE_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXJABBERERROR")) {
			pr_err("MMC_RXJABBERERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXRUNTERROR")) {
			pr_err("MMC_RXRUNTERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXALIGNMENTERROR")) {
			pr_err("MMC_RXALIGNMENTERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXCRCERROR")) {
			pr_err("MMC_RXCRCERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXMULTICASTPACKETS_G")) {
			pr_err("MMC_RXMULTICASTPACKETS_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXBROADCASTPACKETS_G")) {
			pr_err("MMC_RXBROADCASTPACKETS_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXOCTETCOUNT_G")) {
			pr_err("MMC_RXOCTETCOUNT_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXOCTETCOUNT_GB")) {
			pr_err("MMC_RXOCTETCOUNT_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_RXPACKETCOUNT_GB")) {
			pr_err("MMC_RXPACKETCOUNT_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXOVERSIZE_G")) {
			pr_err("MMC_TXOVERSIZE_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXVLANPACKETS_G")) {
			pr_err("MMC_TXVLANPACKETS_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXPAUSEPACKETS")) {
			pr_err("MMC_TXPAUSEPACKETS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXEXCESSDEF")) {
			pr_err("MMC_TXEXCESSDEF is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXPACKETSCOUNT_G")) {
			pr_err("MMC_TXPACKETSCOUNT_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXOCTETCOUNT_G")) {
			pr_err("MMC_TXOCTETCOUNT_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXCARRIERERROR")) {
			pr_err("MMC_TXCARRIERERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXEXESSCOL")) {
			pr_err("MMC_TXEXESSCOL is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXLATECOL")) {
			pr_err("MMC_TXLATECOL is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXDEFERRED")) {
			pr_err("MMC_TXDEFERRED is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXMULTICOL_G")) {
			pr_err("MMC_TXMULTICOL_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXSINGLECOL_G")) {
			pr_err("MMC_TXSINGLECOL_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXUNDERFLOWERROR")) {
			pr_err("MMC_TXUNDERFLOWERROR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXBROADCASTPACKETS_GB")) {
			pr_err("MMC_TXBROADCASTPACKETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXMULTICASTPACKETS_GB")) {
			pr_err("MMC_TXMULTICASTPACKETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXUNICASTPACKETS_GB")) {
			pr_err("MMC_TXUNICASTPACKETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TX1024TOMAXOCTETS_GB")) {
			pr_err("MMC_TX1024TOMAXOCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TX512TO1023OCTETS_GB")) {
			pr_err("MMC_TX512TO1023OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TX256TO511OCTETS_GB")) {
			pr_err("MMC_TX256TO511OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TX128TO255OCTETS_GB")) {
			pr_err("MMC_TX128TO255OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TX65TO127OCTETS_GB")) {
			pr_err("MMC_TX65TO127OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TX64OCTETS_GB")) {
			pr_err("MMC_TX64OCTETS_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXMULTICASTPACKETS_G")) {
			pr_err("MMC_TXMULTICASTPACKETS_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXBROADCASTPACKETS_G")) {
			pr_err("MMC_TXBROADCASTPACKETS_G is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXPACKETCOUNT_GB")) {
			pr_err("MMC_TXPACKETCOUNT_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_TXOCTETCOUNT_GB")) {
			pr_err("MMC_TXOCTETCOUNT_GB is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_IPC_INTR_RX")) {
			pr_err("MMC_IPC_INTR_RX is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MMC_IPC_INTR_MASK_RX")) {
			MMC_IPC_INTR_MASK_RX_WR(integer_value);
		} else if (!strcmp(reg_name, "MMC_INTR_MASK_TX")) {
			MMC_INTR_MASK_TX_WR(integer_value);
		} else if (!strcmp(reg_name, "MMC_INTR_MASK_RX")) {
			MMC_INTR_MASK_RX_WR(integer_value);
		} else if (!strcmp(reg_name, "MMC_INTR_TX")) {
			MMC_INTR_TX_WR(integer_value);
		} else if (!strcmp(reg_name, "MMC_INTR_RX")) {
			MMC_INTR_RX_WR(integer_value);
		} else if (!strcmp(reg_name, "MMC_CNTRL")) {
			MMC_CNTRL_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1LR")) {
			MAC_MA1LR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_MA1HR")) {
			MAC_MA1HR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_MA0LR")) {
			MAC_MA0LR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_MA0HR")) {
			MAC_MA0HR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_GPIOR")) {
			MAC_GPIOR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_GMIIDR")) {
			MAC_GMIIDR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_GMIIAR")) {
			MAC_GMIIAR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_HFR2")) {
			pr_err("MAC_HFR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_HFR1")) {
			pr_err("MAC_HFR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_HFR0")) {
			pr_err("MAC_HFR0 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_MDR")) {
			pr_err("MAC_MDR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_VR")) {
			pr_err("MAC_VR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MAC_HTR7")) {
			MAC_HTR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_HTR6")) {
			MAC_HTR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_HTR5")) {
			MAC_HTR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_HTR4")) {
			MAC_HTR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_HTR3")) {
			MAC_HTR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_HTR2")) {
			MAC_HTR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_HTR1")) {
			MAC_HTR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_HTR0")) {
			MAC_HTR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RIWTR7")) {
			DMA_RIWTR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RIWTR6")) {
			DMA_RIWTR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RIWTR5")) {
			DMA_RIWTR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RIWTR4")) {
			DMA_RIWTR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RIWTR3")) {
			DMA_RIWTR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RIWTR2")) {
			DMA_RIWTR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RIWTR1")) {
			DMA_RIWTR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RIWTR0")) {
			DMA_RIWTR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDRLR7")) {
			DMA_RDRLR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDRLR6")) {
			DMA_RDRLR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDRLR5")) {
			DMA_RDRLR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDRLR4")) {
			DMA_RDRLR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDRLR3")) {
			DMA_RDRLR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDRLR2")) {
			DMA_RDRLR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDRLR1")) {
			DMA_RDRLR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDRLR0")) {
			DMA_RDRLR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDRLR7")) {
			DMA_TDRLR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDRLR6")) {
			DMA_TDRLR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDRLR5")) {
			DMA_TDRLR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDRLR4")) {
			DMA_TDRLR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDRLR3")) {
			DMA_TDRLR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDRLR2")) {
			DMA_TDRLR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDRLR1")) {
			DMA_TDRLR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDRLR0")) {
			DMA_TDRLR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDTP_RPDR7")) {
			DMA_RDTP_RPDR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDTP_RPDR6")) {
			DMA_RDTP_RPDR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDTP_RPDR5")) {
			DMA_RDTP_RPDR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDTP_RPDR4")) {
			DMA_RDTP_RPDR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDTP_RPDR3")) {
			DMA_RDTP_RPDR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDTP_RPDR2")) {
			DMA_RDTP_RPDR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDTP_RPDR1")) {
			DMA_RDTP_RPDR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDTP_RPDR0")) {
			DMA_RDTP_RPDR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDTP_TPDR7")) {
			DMA_TDTP_TPDR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDTP_TPDR6")) {
			DMA_TDTP_TPDR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDTP_TPDR5")) {
			DMA_TDTP_TPDR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDTP_TPDR4")) {
			DMA_TDTP_TPDR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDTP_TPDR3")) {
			DMA_TDTP_TPDR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDTP_TPDR2")) {
			DMA_TDTP_TPDR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDTP_TPDR1")) {
			DMA_TDTP_TPDR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDTP_TPDR0")) {
			DMA_TDTP_TPDR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDLAR7")) {
			DMA_RDLAR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDLAR6")) {
			DMA_RDLAR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDLAR5")) {
			DMA_RDLAR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDLAR4")) {
			DMA_RDLAR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDLAR3")) {
			DMA_RDLAR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDLAR2")) {
			DMA_RDLAR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDLAR1")) {
			DMA_RDLAR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RDLAR0")) {
			DMA_RDLAR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDLAR7")) {
			DMA_TDLAR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDLAR6")) {
			DMA_TDLAR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDLAR5")) {
			DMA_TDLAR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDLAR4")) {
			DMA_TDLAR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDLAR3")) {
			DMA_TDLAR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDLAR2")) {
			DMA_TDLAR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDLAR1")) {
			DMA_TDLAR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TDLAR0")) {
			DMA_TDLAR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_IER7")) {
			DMA_IER_WR(7, integer_value);
		} else if (!strcmp(reg_name, "DMA_IER6")) {
			DMA_IER_WR(6, integer_value);
		} else if (!strcmp(reg_name, "DMA_IER5")) {
			DMA_IER_WR(5, integer_value);
		} else if (!strcmp(reg_name, "DMA_IER4")) {
			DMA_IER_WR(4, integer_value);
		} else if (!strcmp(reg_name, "DMA_IER3")) {
			DMA_IER_WR(3, integer_value);
		} else if (!strcmp(reg_name, "DMA_IER2")) {
			DMA_IER_WR(2, integer_value);
		} else if (!strcmp(reg_name, "DMA_IER1")) {
			DMA_IER_WR(1, integer_value);
		} else if (!strcmp(reg_name, "DMA_IER0")) {
			DMA_IER_WR(0, integer_value);
		} else if (!strcmp(reg_name, "MAC_IMR")) {
			MAC_IMR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_ISR")) {
			pr_err("MAC_ISR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_ISR")) {
			pr_err("MTL_ISR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_SR7")) {
			DMA_SR_WR(7, integer_value);
		} else if (!strcmp(reg_name, "DMA_SR6")) {
			DMA_SR_WR(6, integer_value);
		} else if (!strcmp(reg_name, "DMA_SR5")) {
			DMA_SR_WR(5, integer_value);
		} else if (!strcmp(reg_name, "DMA_SR4")) {
			DMA_SR_WR(4, integer_value);
		} else if (!strcmp(reg_name, "DMA_SR3")) {
			DMA_SR_WR(3, integer_value);
		} else if (!strcmp(reg_name, "DMA_SR2")) {
			DMA_SR_WR(2, integer_value);
		} else if (!strcmp(reg_name, "DMA_SR1")) {
			DMA_SR_WR(1, integer_value);
		} else if (!strcmp(reg_name, "DMA_SR0")) {
			DMA_SR_WR(0, integer_value);
		} else if (!strcmp(reg_name, "DMA_ISR")) {
			pr_err("DMA_ISR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_DSR2")) {
			pr_err("DMA_DSR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_DSR1")) {
			pr_err("DMA_DSR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_DSR0")) {
			pr_err("DMA_DSR0 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_Q0RDR")) {
			pr_err("MTL_Q0RDR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_Q0ESR")) {
			pr_err("MTL_Q0ESR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_Q0TDR")) {
			pr_err("MTL_Q0TDR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRBAR7")) {
			pr_err("DMA_CHRBAR7 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRBAR6")) {
			pr_err("DMA_CHRBAR6 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRBAR5")) {
			pr_err("DMA_CHRBAR5 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRBAR4")) {
			pr_err("DMA_CHRBAR4 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRBAR3")) {
			pr_err("DMA_CHRBAR3 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRBAR2")) {
			pr_err("DMA_CHRBAR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRBAR1")) {
			pr_err("DMA_CHRBAR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRBAR0")) {
			pr_err("DMA_CHRBAR0 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTBAR7")) {
			pr_err("DMA_CHTBAR7 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTBAR6")) {
			pr_err("DMA_CHTBAR6 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTBAR5")) {
			pr_err("DMA_CHTBAR5 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTBAR4")) {
			pr_err("DMA_CHTBAR4 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTBAR3")) {
			pr_err("DMA_CHTBAR3 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTBAR2")) {
			pr_err("DMA_CHTBAR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTBAR1")) {
			pr_err("DMA_CHTBAR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTBAR0")) {
			pr_err("DMA_CHTBAR0 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRDR7")) {
			pr_err("DMA_CHRDR7 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRDR6")) {
			pr_err("DMA_CHRDR6 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRDR5")) {
			pr_err(": is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRDR4")) {
			pr_err(": is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRDR3")) {
			pr_err(": is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRDR2")) {
			pr_err("DMA_CHRDR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRDR1")) {
			pr_err("DMA_CHRDR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHRDR0")) {
			pr_err("DMA_CHRDR0 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTDR7")) {
			pr_err("DMA_CHTDR7 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTDR6")) {
			pr_err("DMA_CHTDR6 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTDR5")) {
			pr_err("DMA_CHTDR5 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTDR4")) {
			pr_err("DMA_CHTDR4 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTDR3")) {
			pr_err("DMA_CHTDR3 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTDR2")) {
			pr_err("DMA_CHTDR2 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTDR1")) {
			pr_err("DMA_CHTDR1 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_CHTDR0")) {
			pr_err("DMA_CHTDR0 is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "DMA_SFCSR7")) {
			DMA_SFCSR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_SFCSR6")) {
			DMA_SFCSR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_SFCSR5")) {
			DMA_SFCSR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_SFCSR4")) {
			DMA_SFCSR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_SFCSR3")) {
			DMA_SFCSR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_SFCSR2")) {
			DMA_SFCSR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_SFCSR1")) {
			DMA_SFCSR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_SFCSR0")) {
			DMA_SFCSR0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_IVLANTIRR")) {
			MAC_IVLANTIRR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_VLANTIRR")) {
			MAC_VLANTIRR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_VLANHTR")) {
			MAC_VLANHTR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_VLANTR")) {
			MAC_VLANTR_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_SBUS")) {
			DMA_SBUS_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_BMR")) {
			DMA_BMR_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_Q0RCR")) {
			MTL_Q0RCR_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_Q0OCR")) {
			pr_err("MTL_Q0OCR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_Q0ROMR")) {
			MTL_Q0ROMR_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_Q0QR")) {
			MTL_Q0QR_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_Q0ECR")) {
			MTL_Q0ECR_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_Q0UCR")) {
			MTL_Q0UCR_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_Q0TOMR")) {
			MTL_Q0TOMR_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_RQDCM1R")) {
			MTL_RQDCM1R_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_RQDCM0R")) {
			MTL_RQDCM0R_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_FDDR")) {
			pr_err("MTL_FDDR is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MTL_FDACS")) {
			MTL_FDACS_WR(integer_value);
		} else if (!strcmp(reg_name, "MTL_OMR")) {
			MTL_OMR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_RQC3R")) {
			MAC_RQC3R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_RQC2R")) {
			MAC_RQC2R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_RQC1R")) {
			MAC_RQC1R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_RQC0R")) {
			MAC_RQC0R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_TQPM1R")) {
			MAC_TQPM1R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_TQPM0R")) {
			MAC_TQPM0R_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_RFCR")) {
			MAC_RFCR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_QTFCR7")) {
			MAC_QTFCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_QTFCR6")) {
			MAC_QTFCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_QTFCR5")) {
			MAC_QTFCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_QTFCR4")) {
			MAC_QTFCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_QTFCR3")) {
			MAC_QTFCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_QTFCR2")) {
			MAC_QTFCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_QTFCR1")) {
			MAC_QTFCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_Q0TFCR")) {
			MAC_Q0TFCR_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_AXI4CR7")) {
			DMA_AXI4CR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_AXI4CR6")) {
			DMA_AXI4CR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_AXI4CR5")) {
			DMA_AXI4CR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_AXI4CR4")) {
			DMA_AXI4CR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_AXI4CR3")) {
			DMA_AXI4CR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_AXI4CR2")) {
			DMA_AXI4CR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_AXI4CR1")) {
			DMA_AXI4CR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_AXI4CR0")) {
			DMA_AXI4CR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RCR7")) {
			DMA_RCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RCR6")) {
			DMA_RCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RCR5")) {
			DMA_RCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RCR4")) {
			DMA_RCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RCR3")) {
			DMA_RCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RCR2")) {
			DMA_RCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RCR1")) {
			DMA_RCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_RCR0")) {
			DMA_RCR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TCR7")) {
			DMA_TCR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TCR6")) {
			DMA_TCR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TCR5")) {
			DMA_TCR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TCR4")) {
			DMA_TCR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TCR3")) {
			DMA_TCR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TCR2")) {
			DMA_TCR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TCR1")) {
			DMA_TCR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_TCR0")) {
			DMA_TCR0_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_CR7")) {
			DMA_CR7_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_CR6")) {
			DMA_CR6_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_CR5")) {
			DMA_CR5_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_CR4")) {
			DMA_CR4_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_CR3")) {
			DMA_CR3_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_CR2")) {
			DMA_CR2_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_CR1")) {
			DMA_CR1_WR(integer_value);
		} else if (!strcmp(reg_name, "DMA_CR0")) {
			DMA_CR0_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_WTR")) {
			MAC_WTR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_MPFR")) {
			MAC_MPFR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_MECR")) {
			MAC_MECR_WR(integer_value);
		} else if (!strcmp(reg_name, "MAC_MCR")) {
			MAC_MCR_WR(integer_value);
		}
		/* For MII/GMII register read */
		else if (!strcmp(reg_name, "MII_BMCR_REG")) {
			eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      MII_BMCR,
						      (int)integer_value);
		} else if (!strcmp(reg_name, "MII_BMSR_REG")) {
			pr_err("MII_BMSR_REG is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MII_PHYSID1_REG")) {
			pr_err("MII_PHYSID1_REG is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MII_PHYSID2_REG")) {
			pr_err("MII_PHYSID2_REG is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MII_ADVERTISE_REG")) {
			eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      MII_ADVERTISE,
						      (int)integer_value);
		} else if (!strcmp(reg_name, "MII_LPA_REG")) {
			pr_err("MII_LPA_REG is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MII_EXPANSION_REG")) {
			pr_err("MII_EXPANSION_REG is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "AUTO_NEGO_NP_REG")) {
			eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      EQOS_AUTO_NEGO_NP,
						      (int)integer_value);
		} else if (!strcmp(reg_name, "MII_ESTATUS_REG")) {
			pr_err("MII_ESTATUS_REG is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "MII_CTRL1000_REG")) {
			eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      MII_CTRL1000,
						      (int)integer_value);
		} else if (!strcmp(reg_name, "MII_STAT1000_REG")) {
			pr_err("MII_STAT1000_REG is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "PHY_CTL_REG")) {
			eqos_mdio_write_direct(pdata,
						      pdata->phyaddr,
						      EQOS_PHY_CTL,
						      (int)integer_value);
		} else if (!strcmp(reg_name, "PHY_STS_REG")) {
			pr_err("PHY_STS_REG is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "feature_drop_tx_pktburstcnt")) {
			feature_drop_tx_pktburstcnt_val = (int)integer_value;
			if (feature_drop_tx_pktburstcnt_val == 0) {
				feature_drop_tx_pktburstcnt_val++;
				pr_err("drop_tx_pktburstcnt is negative\n");
			}
			pdata->drop_tx_pktburstcnt =
			    feature_drop_tx_pktburstcnt_val;
		} else if (!strcmp(reg_name, "qinx")) {
			qinx_val = (int)integer_value;
			if (qinx_val > (EQOS_QUEUE_CNT - 1)) {
				qinx_val = 0;
				pr_err("Invalid queue number\n");
				ret = -EFAULT;
			}
		} else if (!strcmp(reg_name, "reg_offset")) {
			reg_offset_val = (int)integer_value;
		} else if (!strcmp(reg_name, "gen_reg")) {
			if (reg_offset_val & 0x10000) {
				/* write clause45 phy reg */
				gen_reg_val = (int)integer_value;
				bcm_regs_clause45_write(2,
				(reg_offset_val & 0xffff), gen_reg_val);
			} else if (reg_offset_val & 0x20000) {
				/* write phy reg */
				gen_reg_val = (int)integer_value;
				eqos_mdio_write_direct(pdata,
				pdata->phyaddr, (reg_offset_val & 0xffff),
				gen_reg_val);
			} else  {
				/* write mac reg */
				gen_reg_val = (int)integer_value;
				iowrite32(gen_reg_val, (void *)(ULONG *)
				(BASE_ADDRESS + reg_offset_val));
			}
		} else if (!strcmp(reg_name, "do_tx_align_tst")) {
			do_tx_align_tst_val = (int)integer_value;
			do_transmit_alignment_test(pdata);
		} else if (!strcmp(reg_name, "BCM_REGS")) {
			pr_err("BCM_REGS is readonly\n");
			ret = -EFAULT;
		} else if (!strcmp(reg_name, "pre_padcal_err_counters")) {
			pr_err("pre_padcal_err_counters is readonly\n");
			ret = -EFAULT;
		} else {
			pr_err("%s not found\n", reg_name);
			ret = -EFAULT;
		}
	}

	DBGPR("<-- eqos_write\n");

	return ret;
}

static ssize_t registers_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	DBGPR("--> registers_write\n");
	pr_info("Error: Invalid file name\n");
	DBGPR("<-- registers_write\n");

	return -1;
}

static ssize_t registers_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	char *debug_buf = NULL;

	DBGPR("--> registers_read\n");

	MAC_MA32_127LR_RD(127, MAC_MA32_127LR127_val);
	MAC_MA32_127LR_RD(126, MAC_MA32_127LR126_val);
	MAC_MA32_127LR_RD(125, MAC_MA32_127LR125_val);
	MAC_MA32_127LR_RD(124, MAC_MA32_127LR124_val);
	MAC_MA32_127LR_RD(123, MAC_MA32_127LR123_val);
	MAC_MA32_127LR_RD(122, MAC_MA32_127LR122_val);
	MAC_MA32_127LR_RD(121, MAC_MA32_127LR121_val);
	MAC_MA32_127LR_RD(120, MAC_MA32_127LR120_val);
	MAC_MA32_127LR_RD(119, MAC_MA32_127LR119_val);
	MAC_MA32_127LR_RD(118, MAC_MA32_127LR118_val);
	MAC_MA32_127LR_RD(117, MAC_MA32_127LR117_val);
	MAC_MA32_127LR_RD(116, MAC_MA32_127LR116_val);
	MAC_MA32_127LR_RD(115, MAC_MA32_127LR115_val);
	MAC_MA32_127LR_RD(114, MAC_MA32_127LR114_val);
	MAC_MA32_127LR_RD(113, MAC_MA32_127LR113_val);
	MAC_MA32_127LR_RD(112, MAC_MA32_127LR112_val);
	MAC_MA32_127LR_RD(111, MAC_MA32_127LR111_val);
	MAC_MA32_127LR_RD(110, MAC_MA32_127LR110_val);
	MAC_MA32_127LR_RD(109, MAC_MA32_127LR109_val);
	MAC_MA32_127LR_RD(108, MAC_MA32_127LR108_val);
	MAC_MA32_127LR_RD(107, MAC_MA32_127LR107_val);
	MAC_MA32_127LR_RD(106, MAC_MA32_127LR106_val);
	MAC_MA32_127LR_RD(105, MAC_MA32_127LR105_val);
	MAC_MA32_127LR_RD(104, MAC_MA32_127LR104_val);
	MAC_MA32_127LR_RD(103, MAC_MA32_127LR103_val);
	MAC_MA32_127LR_RD(102, MAC_MA32_127LR102_val);
	MAC_MA32_127LR_RD(101, MAC_MA32_127LR101_val);
	MAC_MA32_127LR_RD(100, MAC_MA32_127LR100_val);
	MAC_MA32_127LR_RD(99, MAC_MA32_127LR99_val);
	MAC_MA32_127LR_RD(98, MAC_MA32_127LR98_val);
	MAC_MA32_127LR_RD(97, MAC_MA32_127LR97_val);
	MAC_MA32_127LR_RD(96, MAC_MA32_127LR96_val);
	MAC_MA32_127LR_RD(95, MAC_MA32_127LR95_val);
	MAC_MA32_127LR_RD(94, MAC_MA32_127LR94_val);
	MAC_MA32_127LR_RD(93, MAC_MA32_127LR93_val);
	MAC_MA32_127LR_RD(92, MAC_MA32_127LR92_val);
	MAC_MA32_127LR_RD(91, MAC_MA32_127LR91_val);
	MAC_MA32_127LR_RD(90, MAC_MA32_127LR90_val);
	MAC_MA32_127LR_RD(89, MAC_MA32_127LR89_val);
	MAC_MA32_127LR_RD(88, MAC_MA32_127LR88_val);
	MAC_MA32_127LR_RD(87, MAC_MA32_127LR87_val);
	MAC_MA32_127LR_RD(86, MAC_MA32_127LR86_val);
	MAC_MA32_127LR_RD(85, MAC_MA32_127LR85_val);
	MAC_MA32_127LR_RD(84, MAC_MA32_127LR84_val);
	MAC_MA32_127LR_RD(83, MAC_MA32_127LR83_val);
	MAC_MA32_127LR_RD(82, MAC_MA32_127LR82_val);
	MAC_MA32_127LR_RD(81, MAC_MA32_127LR81_val);
	MAC_MA32_127LR_RD(80, MAC_MA32_127LR80_val);
	MAC_MA32_127LR_RD(79, MAC_MA32_127LR79_val);
	MAC_MA32_127LR_RD(78, MAC_MA32_127LR78_val);
	MAC_MA32_127LR_RD(77, MAC_MA32_127LR77_val);
	MAC_MA32_127LR_RD(76, MAC_MA32_127LR76_val);
	MAC_MA32_127LR_RD(75, MAC_MA32_127LR75_val);
	MAC_MA32_127LR_RD(74, MAC_MA32_127LR74_val);
	MAC_MA32_127LR_RD(73, MAC_MA32_127LR73_val);
	MAC_MA32_127LR_RD(72, MAC_MA32_127LR72_val);
	MAC_MA32_127LR_RD(71, MAC_MA32_127LR71_val);
	MAC_MA32_127LR_RD(70, MAC_MA32_127LR70_val);
	MAC_MA32_127LR_RD(69, MAC_MA32_127LR69_val);
	MAC_MA32_127LR_RD(68, MAC_MA32_127LR68_val);
	MAC_MA32_127LR_RD(67, MAC_MA32_127LR67_val);
	MAC_MA32_127LR_RD(66, MAC_MA32_127LR66_val);
	MAC_MA32_127LR_RD(65, MAC_MA32_127LR65_val);
	MAC_MA32_127LR_RD(64, MAC_MA32_127LR64_val);
	MAC_MA32_127LR_RD(63, MAC_MA32_127LR63_val);
	MAC_MA32_127LR_RD(62, MAC_MA32_127LR62_val);
	MAC_MA32_127LR_RD(61, MAC_MA32_127LR61_val);
	MAC_MA32_127LR_RD(60, MAC_MA32_127LR60_val);
	MAC_MA32_127LR_RD(59, MAC_MA32_127LR59_val);
	MAC_MA32_127LR_RD(58, MAC_MA32_127LR58_val);
	MAC_MA32_127LR_RD(57, MAC_MA32_127LR57_val);
	MAC_MA32_127LR_RD(56, MAC_MA32_127LR56_val);
	MAC_MA32_127LR_RD(55, MAC_MA32_127LR55_val);
	MAC_MA32_127LR_RD(54, MAC_MA32_127LR54_val);
	MAC_MA32_127LR_RD(53, MAC_MA32_127LR53_val);
	MAC_MA32_127LR_RD(52, MAC_MA32_127LR52_val);
	MAC_MA32_127LR_RD(51, MAC_MA32_127LR51_val);
	MAC_MA32_127LR_RD(50, MAC_MA32_127LR50_val);
	MAC_MA32_127LR_RD(49, MAC_MA32_127LR49_val);
	MAC_MA32_127LR_RD(48, MAC_MA32_127LR48_val);
	MAC_MA32_127LR_RD(47, MAC_MA32_127LR47_val);
	MAC_MA32_127LR_RD(46, MAC_MA32_127LR46_val);
	MAC_MA32_127LR_RD(45, MAC_MA32_127LR45_val);
	MAC_MA32_127LR_RD(44, MAC_MA32_127LR44_val);
	MAC_MA32_127LR_RD(43, MAC_MA32_127LR43_val);
	MAC_MA32_127LR_RD(42, MAC_MA32_127LR42_val);
	MAC_MA32_127LR_RD(41, MAC_MA32_127LR41_val);
	MAC_MA32_127LR_RD(40, MAC_MA32_127LR40_val);
	MAC_MA32_127LR_RD(39, MAC_MA32_127LR39_val);
	MAC_MA32_127LR_RD(38, MAC_MA32_127LR38_val);
	MAC_MA32_127LR_RD(37, MAC_MA32_127LR37_val);
	MAC_MA32_127LR_RD(36, MAC_MA32_127LR36_val);
	MAC_MA32_127LR_RD(35, MAC_MA32_127LR35_val);
	MAC_MA32_127LR_RD(34, MAC_MA32_127LR34_val);
	MAC_MA32_127LR_RD(33, MAC_MA32_127LR33_val);
	MAC_MA32_127LR_RD(32, MAC_MA32_127LR32_val);
	MAC_MA32_127HR_RD(127, MAC_MA32_127HR127_val);
	MAC_MA32_127HR_RD(126, MAC_MA32_127HR126_val);
	MAC_MA32_127HR_RD(125, MAC_MA32_127HR125_val);
	MAC_MA32_127HR_RD(124, MAC_MA32_127HR124_val);
	MAC_MA32_127HR_RD(123, MAC_MA32_127HR123_val);
	MAC_MA32_127HR_RD(122, MAC_MA32_127HR122_val);
	MAC_MA32_127HR_RD(121, MAC_MA32_127HR121_val);
	MAC_MA32_127HR_RD(120, MAC_MA32_127HR120_val);
	MAC_MA32_127HR_RD(119, MAC_MA32_127HR119_val);
	MAC_MA32_127HR_RD(118, MAC_MA32_127HR118_val);
	MAC_MA32_127HR_RD(117, MAC_MA32_127HR117_val);
	MAC_MA32_127HR_RD(116, MAC_MA32_127HR116_val);
	MAC_MA32_127HR_RD(115, MAC_MA32_127HR115_val);
	MAC_MA32_127HR_RD(114, MAC_MA32_127HR114_val);
	MAC_MA32_127HR_RD(113, MAC_MA32_127HR113_val);
	MAC_MA32_127HR_RD(112, MAC_MA32_127HR112_val);
	MAC_MA32_127HR_RD(111, MAC_MA32_127HR111_val);
	MAC_MA32_127HR_RD(110, MAC_MA32_127HR110_val);
	MAC_MA32_127HR_RD(109, MAC_MA32_127HR109_val);
	MAC_MA32_127HR_RD(108, MAC_MA32_127HR108_val);
	MAC_MA32_127HR_RD(107, MAC_MA32_127HR107_val);
	MAC_MA32_127HR_RD(106, MAC_MA32_127HR106_val);
	MAC_MA32_127HR_RD(105, MAC_MA32_127HR105_val);
	MAC_MA32_127HR_RD(104, MAC_MA32_127HR104_val);
	MAC_MA32_127HR_RD(103, MAC_MA32_127HR103_val);
	MAC_MA32_127HR_RD(102, MAC_MA32_127HR102_val);
	MAC_MA32_127HR_RD(101, MAC_MA32_127HR101_val);
	MAC_MA32_127HR_RD(100, MAC_MA32_127HR100_val);
	MAC_MA32_127HR_RD(99, MAC_MA32_127HR99_val);
	MAC_MA32_127HR_RD(98, MAC_MA32_127HR98_val);
	MAC_MA32_127HR_RD(97, MAC_MA32_127HR97_val);
	MAC_MA32_127HR_RD(96, MAC_MA32_127HR96_val);
	MAC_MA32_127HR_RD(95, MAC_MA32_127HR95_val);
	MAC_MA32_127HR_RD(94, MAC_MA32_127HR94_val);
	MAC_MA32_127HR_RD(93, MAC_MA32_127HR93_val);
	MAC_MA32_127HR_RD(92, MAC_MA32_127HR92_val);
	MAC_MA32_127HR_RD(91, MAC_MA32_127HR91_val);
	MAC_MA32_127HR_RD(90, MAC_MA32_127HR90_val);
	MAC_MA32_127HR_RD(89, MAC_MA32_127HR89_val);
	MAC_MA32_127HR_RD(88, MAC_MA32_127HR88_val);
	MAC_MA32_127HR_RD(87, MAC_MA32_127HR87_val);
	MAC_MA32_127HR_RD(86, MAC_MA32_127HR86_val);
	MAC_MA32_127HR_RD(85, MAC_MA32_127HR85_val);
	MAC_MA32_127HR_RD(84, MAC_MA32_127HR84_val);
	MAC_MA32_127HR_RD(83, MAC_MA32_127HR83_val);
	MAC_MA32_127HR_RD(82, MAC_MA32_127HR82_val);
	MAC_MA32_127HR_RD(81, MAC_MA32_127HR81_val);
	MAC_MA32_127HR_RD(80, MAC_MA32_127HR80_val);
	MAC_MA32_127HR_RD(79, MAC_MA32_127HR79_val);
	MAC_MA32_127HR_RD(78, MAC_MA32_127HR78_val);
	MAC_MA32_127HR_RD(77, MAC_MA32_127HR77_val);
	MAC_MA32_127HR_RD(76, MAC_MA32_127HR76_val);
	MAC_MA32_127HR_RD(75, MAC_MA32_127HR75_val);
	MAC_MA32_127HR_RD(74, MAC_MA32_127HR74_val);
	MAC_MA32_127HR_RD(73, MAC_MA32_127HR73_val);
	MAC_MA32_127HR_RD(72, MAC_MA32_127HR72_val);
	MAC_MA32_127HR_RD(71, MAC_MA32_127HR71_val);
	MAC_MA32_127HR_RD(70, MAC_MA32_127HR70_val);
	MAC_MA32_127HR_RD(69, MAC_MA32_127HR69_val);
	MAC_MA32_127HR_RD(68, MAC_MA32_127HR68_val);
	MAC_MA32_127HR_RD(67, MAC_MA32_127HR67_val);
	MAC_MA32_127HR_RD(66, MAC_MA32_127HR66_val);
	MAC_MA32_127HR_RD(65, MAC_MA32_127HR65_val);
	MAC_MA32_127HR_RD(64, MAC_MA32_127HR64_val);
	MAC_MA32_127HR_RD(63, MAC_MA32_127HR63_val);
	MAC_MA32_127HR_RD(62, MAC_MA32_127HR62_val);
	MAC_MA32_127HR_RD(61, MAC_MA32_127HR61_val);
	MAC_MA32_127HR_RD(60, MAC_MA32_127HR60_val);
	MAC_MA32_127HR_RD(59, MAC_MA32_127HR59_val);
	MAC_MA32_127HR_RD(58, MAC_MA32_127HR58_val);
	MAC_MA32_127HR_RD(57, MAC_MA32_127HR57_val);
	MAC_MA32_127HR_RD(56, MAC_MA32_127HR56_val);
	MAC_MA32_127HR_RD(55, MAC_MA32_127HR55_val);
	MAC_MA32_127HR_RD(54, MAC_MA32_127HR54_val);
	MAC_MA32_127HR_RD(53, MAC_MA32_127HR53_val);
	MAC_MA32_127HR_RD(52, MAC_MA32_127HR52_val);
	MAC_MA32_127HR_RD(51, MAC_MA32_127HR51_val);
	MAC_MA32_127HR_RD(50, MAC_MA32_127HR50_val);
	MAC_MA32_127HR_RD(49, MAC_MA32_127HR49_val);
	MAC_MA32_127HR_RD(48, MAC_MA32_127HR48_val);
	MAC_MA32_127HR_RD(47, MAC_MA32_127HR47_val);
	MAC_MA32_127HR_RD(46, MAC_MA32_127HR46_val);
	MAC_MA32_127HR_RD(45, MAC_MA32_127HR45_val);
	MAC_MA32_127HR_RD(44, MAC_MA32_127HR44_val);
	MAC_MA32_127HR_RD(43, MAC_MA32_127HR43_val);
	MAC_MA32_127HR_RD(42, MAC_MA32_127HR42_val);
	MAC_MA32_127HR_RD(41, MAC_MA32_127HR41_val);
	MAC_MA32_127HR_RD(40, MAC_MA32_127HR40_val);
	MAC_MA32_127HR_RD(39, MAC_MA32_127HR39_val);
	MAC_MA32_127HR_RD(38, MAC_MA32_127HR38_val);
	MAC_MA32_127HR_RD(37, MAC_MA32_127HR37_val);
	MAC_MA32_127HR_RD(36, MAC_MA32_127HR36_val);
	MAC_MA32_127HR_RD(35, MAC_MA32_127HR35_val);
	MAC_MA32_127HR_RD(34, MAC_MA32_127HR34_val);
	MAC_MA32_127HR_RD(33, MAC_MA32_127HR33_val);
	MAC_MA32_127HR_RD(32, MAC_MA32_127HR32_val);
	MAC_MA1_31LR_RD(31, MAC_MA1_31LR31_val);
	MAC_MA1_31LR_RD(30, MAC_MA1_31LR30_val);
	MAC_MA1_31LR_RD(29, MAC_MA1_31LR29_val);
	MAC_MA1_31LR_RD(28, MAC_MA1_31LR28_val);
	MAC_MA1_31LR_RD(27, MAC_MA1_31LR27_val);
	MAC_MA1_31LR_RD(26, MAC_MA1_31LR26_val);
	MAC_MA1_31LR_RD(25, MAC_MA1_31LR25_val);
	MAC_MA1_31LR_RD(24, MAC_MA1_31LR24_val);
	MAC_MA1_31LR_RD(23, MAC_MA1_31LR23_val);
	MAC_MA1_31LR_RD(22, MAC_MA1_31LR22_val);
	MAC_MA1_31LR_RD(21, MAC_MA1_31LR21_val);
	MAC_MA1_31LR_RD(20, MAC_MA1_31LR20_val);
	MAC_MA1_31LR_RD(19, MAC_MA1_31LR19_val);
	MAC_MA1_31LR_RD(18, MAC_MA1_31LR18_val);
	MAC_MA1_31LR_RD(17, MAC_MA1_31LR17_val);
	MAC_MA1_31LR_RD(16, MAC_MA1_31LR16_val);
	MAC_MA1_31LR_RD(15, MAC_MA1_31LR15_val);
	MAC_MA1_31LR_RD(14, MAC_MA1_31LR14_val);
	MAC_MA1_31LR_RD(13, MAC_MA1_31LR13_val);
	MAC_MA1_31LR_RD(12, MAC_MA1_31LR12_val);
	MAC_MA1_31LR_RD(11, MAC_MA1_31LR11_val);
	MAC_MA1_31LR_RD(10, MAC_MA1_31LR10_val);
	MAC_MA1_31LR_RD(9, MAC_MA1_31LR9_val);
	MAC_MA1_31LR_RD(8, MAC_MA1_31LR8_val);
	MAC_MA1_31LR_RD(7, MAC_MA1_31LR7_val);
	MAC_MA1_31LR_RD(6, MAC_MA1_31LR6_val);
	MAC_MA1_31LR_RD(5, MAC_MA1_31LR5_val);
	MAC_MA1_31LR_RD(4, MAC_MA1_31LR4_val);
	MAC_MA1_31LR_RD(3, MAC_MA1_31LR3_val);
	MAC_MA1_31LR_RD(2, MAC_MA1_31LR2_val);
	MAC_MA1_31LR_RD(1, MAC_MA1_31LR1_val);
	MAC_MA1_31HR_RD(31, MAC_MA1_31HR31_val);
	MAC_MA1_31HR_RD(30, MAC_MA1_31HR30_val);
	MAC_MA1_31HR_RD(29, MAC_MA1_31HR29_val);
	MAC_MA1_31HR_RD(28, MAC_MA1_31HR28_val);
	MAC_MA1_31HR_RD(27, MAC_MA1_31HR27_val);
	MAC_MA1_31HR_RD(26, MAC_MA1_31HR26_val);
	MAC_MA1_31HR_RD(25, MAC_MA1_31HR25_val);
	MAC_MA1_31HR_RD(24, MAC_MA1_31HR24_val);
	MAC_MA1_31HR_RD(23, MAC_MA1_31HR23_val);
	MAC_MA1_31HR_RD(22, MAC_MA1_31HR22_val);
	MAC_MA1_31HR_RD(21, MAC_MA1_31HR21_val);
	MAC_MA1_31HR_RD(20, MAC_MA1_31HR20_val);
	MAC_MA1_31HR_RD(19, MAC_MA1_31HR19_val);
	MAC_MA1_31HR_RD(18, MAC_MA1_31HR18_val);
	MAC_MA1_31HR_RD(17, MAC_MA1_31HR17_val);
	MAC_MA1_31HR_RD(16, MAC_MA1_31HR16_val);
	MAC_MA1_31HR_RD(15, MAC_MA1_31HR15_val);
	MAC_MA1_31HR_RD(14, MAC_MA1_31HR14_val);
	MAC_MA1_31HR_RD(13, MAC_MA1_31HR13_val);
	MAC_MA1_31HR_RD(12, MAC_MA1_31HR12_val);
	MAC_MA1_31HR_RD(11, MAC_MA1_31HR11_val);
	MAC_MA1_31HR_RD(10, MAC_MA1_31HR10_val);
	MAC_MA1_31HR_RD(9, MAC_MA1_31HR9_val);
	MAC_MA1_31HR_RD(8, MAC_MA1_31HR8_val);
	MAC_MA1_31HR_RD(7, MAC_MA1_31HR7_val);
	MAC_MA1_31HR_RD(6, MAC_MA1_31HR6_val);
	MAC_MA1_31HR_RD(5, MAC_MA1_31HR5_val);
	MAC_MA1_31HR_RD(4, MAC_MA1_31HR4_val);
	MAC_MA1_31HR_RD(3, MAC_MA1_31HR3_val);
	MAC_MA1_31HR_RD(2, MAC_MA1_31HR2_val);
	MAC_MA1_31HR_RD(1, MAC_MA1_31HR1_val);
	MAC_ARPA_RD(mac_arpa_val);
	MAC_L3A3R7_RD(MAC_L3A3R7_val);
	MAC_L3A3R6_RD(MAC_L3A3R6_val);
	MAC_L3A3R5_RD(MAC_L3A3R5_val);
	MAC_L3A3R4_RD(MAC_L3A3R4_val);
	MAC_L3A3R3_RD(MAC_L3A3R3_val);
	MAC_L3A3R2_RD(MAC_L3A3R2_val);
	MAC_L3A3R1_RD(MAC_L3A3R1_val);
	MAC_L3A3R0_RD(MAC_L3A3R0_val);
	MAC_L3A2R7_RD(MAC_L3A2R7_val);
	MAC_L3A2R6_RD(MAC_L3A2R6_val);
	MAC_L3A2R5_RD(MAC_L3A2R5_val);
	MAC_L3A2R4_RD(MAC_L3A2R4_val);
	MAC_L3A2R3_RD(MAC_L3A2R3_val);
	MAC_L3A2R2_RD(MAC_L3A2R2_val);
	MAC_L3A2R1_RD(MAC_L3A2R1_val);
	MAC_L3A2R0_RD(MAC_L3A2R0_val);
	MAC_L3A1R7_RD(MAC_L3A1R7_val);
	MAC_L3A1R6_RD(MAC_L3A1R6_val);
	MAC_L3A1R5_RD(MAC_L3A1R5_val);
	MAC_L3A1R4_RD(MAC_L3A1R4_val);
	MAC_L3A1R3_RD(MAC_L3A1R3_val);
	MAC_L3A1R2_RD(MAC_L3A1R2_val);
	MAC_L3A1R1_RD(MAC_L3A1R1_val);
	MAC_L3A1R0_RD(MAC_L3A1R0_val);
	MAC_L3A0R7_RD(MAC_L3A0R7_val);
	MAC_L3A0R6_RD(MAC_L3A0R6_val);
	MAC_L3A0R5_RD(MAC_L3A0R5_val);
	MAC_L3A0R4_RD(MAC_L3A0R4_val);
	MAC_L3A0R3_RD(MAC_L3A0R3_val);
	MAC_L3A0R2_RD(MAC_L3A0R2_val);
	MAC_L3A0R1_RD(MAC_L3A0R1_val);
	MAC_L3A0R0_RD(MAC_L3A0R0_val);
	MAC_L4AR7_RD(MAC_L4AR7_val);
	MAC_L4AR6_RD(MAC_L4AR6_val);
	MAC_L4AR5_RD(MAC_L4AR5_val);
	MAC_L4AR4_RD(MAC_L4AR4_val);
	MAC_L4AR3_RD(MAC_L4AR3_val);
	MAC_L4AR2_RD(MAC_L4AR2_val);
	MAC_L4AR1_RD(MAC_L4AR1_val);
	MAC_L4AR0_RD(MAC_L4AR0_val);
	MAC_L3L4CR7_RD(MAC_L3L4CR7_val);
	MAC_L3L4CR6_RD(MAC_L3L4CR6_val);
	MAC_L3L4CR5_RD(MAC_L3L4CR5_val);
	MAC_L3L4CR4_RD(MAC_L3L4CR4_val);
	MAC_L3L4CR3_RD(MAC_L3L4CR3_val);
	MAC_L3L4CR2_RD(MAC_L3L4CR2_val);
	MAC_L3L4CR1_RD(MAC_L3L4CR1_val);
	MAC_L3L4CR0_RD(MAC_L3L4CR0_val);
	MAC_GPIOS_RD(mac_gpios_val);
	MAC_PCS_RD(mac_pcs_val);
	MAC_TES_RD(mac_tes_val);
	MAC_AE_RD(mac_ae_val);
	MAC_ALPA_RD(mac_alpa_val);
	MAC_AAD_RD(mac_aad_val);
	MAC_ANS_RD(mac_ans_val);
	MAC_ANC_RD(mac_anc_val);
	MAC_LPC_RD(mac_lpc_val);
	MAC_LPS_RD(mac_lps_val);
	MAC_SPI2R_RD(mac_lmir_val);
	MAC_SPI2R_RD(MAC_SPI2r_val);
	MAC_SPI1R_RD(MAC_SPI1r_val);
	MAC_SPI0R_RD(MAC_SPI0r_val);
	MAC_PTO_CR_RD(mac_pto_cr_val);
	MAC_PPS_WIDTH3_RD(MAC_PPS_WIDTH3_val);
	MAC_PPS_WIDTH2_RD(MAC_PPS_WIDTH2_val);
	MAC_PPS_WIDTH1_RD(MAC_PPS_WIDTH1_val);
	MAC_PPS_WIDTH0_RD(MAC_PPS_WIDTH0_val);
	MAC_PPS_INTVAL3_RD(MAC_PPS_INTVAL3_val);
	MAC_PPS_INTVAL2_RD(MAC_PPS_INTVAL2_val);
	MAC_PPS_INTVAL1_RD(MAC_PPS_INTVAL1_val);
	MAC_PPS_INTVAL0_RD(MAC_PPS_INTVAL0_val);
	MAC_PPS_TTNS3_RD(MAC_PPS_TTNS3_val);
	MAC_PPS_TTNS2_RD(MAC_PPS_TTNS2_val);
	MAC_PPS_TTNS1_RD(MAC_PPS_TTNS1_val);
	MAC_PPS_TTNS0_RD(MAC_PPS_TTNS0_val);
	MAC_PPS_TTS3_RD(MAC_PPS_TTS3_val);
	MAC_PPS_TTS2_RD(MAC_PPS_TTS2_val);
	MAC_PPS_TTS1_RD(MAC_PPS_TTS1_val);
	MAC_PPS_TTS0_RD(MAC_PPS_TTS0_val);
	MAC_PPSC_RD(mac_ppsc_val);
	MAC_TEAC_RD(mac_teac_val);
	MAC_TIAC_RD(mac_tiac_val);
	MAC_ATS_RD(mac_ats_val);
	MAC_ATN_RD(mac_atn_val);
	MAC_AC_RD(mac_ac_val);
	MAC_TTN_RD(mac_ttn_val);
	MAC_TTSN_RD(mac_ttsn_val);
	MAC_TSR_RD(mac_tsr_val);
	MAC_STHWR_RD(mac_sthwr_val);
	MAC_TAR_RD(mac_tar_val);
	MAC_STNSUR_RD(mac_stnsur_val);
	MAC_STSUR_RD(mac_stsur_val);
	MAC_STNSR_RD(mac_stnsr_val);
	MAC_STSR_RD(mac_stsr_val);
	MAC_SSIR_RD(mac_ssir_val);
	MAC_TCR_RD(mac_tcr_val);
	MTL_DSR_RD(mtl_dsr_val);
	MAC_RWPFFR_RD(mac_rwpffr_val);
	MAC_RTSR_RD(mac_rtsr_val);
	MTL_IER_RD(mtl_ier_val);
	MTL_QRCR7_RD(MTL_QRCR7_val);
	MTL_QRCR6_RD(MTL_QRCR6_val);
	MTL_QRCR5_RD(MTL_QRCR5_val);
	MTL_QRCR4_RD(MTL_QRCR4_val);
	MTL_QRCR3_RD(MTL_QRCR3_val);
	MTL_QRCR2_RD(MTL_QRCR2_val);
	MTL_QRCR1_RD(MTL_QRCR1_val);
	MTL_QRDR7_RD(MTL_QRDR7_val);
	MTL_QRDR6_RD(MTL_QRDR6_val);
	MTL_QRDR5_RD(MTL_QRDR5_val);
	MTL_QRDR4_RD(MTL_QRDR4_val);
	MTL_QRDR3_RD(MTL_QRDR3_val);
	MTL_QRDR2_RD(MTL_QRDR2_val);
	MTL_QRDR1_RD(MTL_QRDR1_val);
	MTL_QOCR7_RD(MTL_QOCR7_val);
	MTL_QOCR6_RD(MTL_QOCR6_val);
	MTL_QOCR5_RD(MTL_QOCR5_val);
	MTL_QOCR4_RD(MTL_QOCR4_val);
	MTL_QOCR3_RD(MTL_QOCR3_val);
	MTL_QOCR2_RD(MTL_QOCR2_val);
	MTL_QOCR1_RD(MTL_QOCR1_val);
	MTL_QROMR_RD(7, MTL_QROMR7_val);
	MTL_QROMR_RD(6, MTL_QROMR6_val);
	MTL_QROMR_RD(5, MTL_QROMR5_val);
	MTL_QROMR_RD(4, MTL_QROMR4_val);
	MTL_QROMR_RD(3, MTL_QROMR3_val);
	MTL_QROMR_RD(2, MTL_QROMR2_val);
	MTL_QROMR_RD(1, MTL_QROMR1_val);
	MTL_QLCR7_RD(MTL_QLCR7_val);
	MTL_QLCR6_RD(MTL_QLCR6_val);
	MTL_QLCR5_RD(MTL_QLCR5_val);
	MTL_QLCR4_RD(MTL_QLCR4_val);
	MTL_QLCR3_RD(MTL_QLCR3_val);
	MTL_QLCR2_RD(MTL_QLCR2_val);
	MTL_QLCR1_RD(MTL_QLCR1_val);
	MTL_QHCR7_RD(MTL_QHCR7_val);
	MTL_QHCR6_RD(MTL_QHCR6_val);
	MTL_QHCR5_RD(MTL_QHCR5_val);
	MTL_QHCR4_RD(MTL_QHCR4_val);
	MTL_QHCR3_RD(MTL_QHCR3_val);
	MTL_QHCR2_RD(MTL_QHCR2_val);
	MTL_QHCR1_RD(MTL_QHCR1_val);
	MTL_QSSCR7_RD(MTL_QSSCR7_val);
	MTL_QSSCR6_RD(MTL_QSSCR6_val);
	MTL_QSSCR5_RD(MTL_QSSCR5_val);
	MTL_QSSCR4_RD(MTL_QSSCR4_val);
	MTL_QSSCR3_RD(MTL_QSSCR3_val);
	MTL_QSSCR2_RD(MTL_QSSCR2_val);
	MTL_QSSCR1_RD(MTL_QSSCR1_val);
	MTL_QW7_RD(MTL_QW7_val);
	MTL_QW6_RD(MTL_QW6_val);
	MTL_QW5_RD(MTL_QW5_val);
	MTL_QW4_RD(MTL_QW4_val);
	MTL_QW3_RD(MTL_QW3_val);
	MTL_QW2_RD(MTL_QW2_val);
	MTL_QW1_RD(MTL_QW1_val);
	MTL_QESR7_RD(MTL_QESR7_val);
	MTL_QESR6_RD(MTL_QESR6_val);
	MTL_QESR5_RD(MTL_QESR5_val);
	MTL_QESR4_RD(MTL_QESR4_val);
	MTL_QESR3_RD(MTL_QESR3_val);
	MTL_QESR2_RD(MTL_QESR2_val);
	MTL_QESR1_RD(MTL_QESR1_val);
	MTL_QECR7_RD(MTL_QECR7_val);
	MTL_QECR6_RD(MTL_QECR6_val);
	MTL_QECR5_RD(MTL_QECR5_val);
	MTL_QECR4_RD(MTL_QECR4_val);
	MTL_QECR3_RD(MTL_QECR3_val);
	MTL_QECR2_RD(MTL_QECR2_val);
	MTL_QECR1_RD(MTL_QECR1_val);
	MTL_QTDR7_RD(MTL_QTDR7_val);
	MTL_QTDR6_RD(MTL_QTDR6_val);
	MTL_QTDR5_RD(MTL_QTDR5_val);
	MTL_QTDR4_RD(MTL_QTDR4_val);
	MTL_QTDR3_RD(MTL_QTDR3_val);
	MTL_QTDR2_RD(MTL_QTDR2_val);
	MTL_QTDR1_RD(MTL_QTDR1_val);
	MTL_QUCR7_RD(MTL_QUCR7_val);
	MTL_QUCR6_RD(MTL_QUCR6_val);
	MTL_QUCR5_RD(MTL_QUCR5_val);
	MTL_QUCR4_RD(MTL_QUCR4_val);
	MTL_QUCR3_RD(MTL_QUCR3_val);
	MTL_QUCR2_RD(MTL_QUCR2_val);
	MTL_QUCR1_RD(MTL_QUCR1_val);
	MTL_QTOMR7_RD(MTL_QTOMR7_val);
	MTL_QTOMR6_RD(MTL_QTOMR6_val);
	MTL_QTOMR5_RD(MTL_QTOMR5_val);
	MTL_QTOMR4_RD(MTL_QTOMR4_val);
	MTL_QTOMR3_RD(MTL_QTOMR3_val);
	MTL_QTOMR2_RD(MTL_QTOMR2_val);
	MTL_QTOMR1_RD(MTL_QTOMR1_val);
	MAC_PMTCSR_RD(mac_pmtcsr_val);
	if (pdata->hw_feat.mmc_sel) {
		MMC_RXICMP_ERR_OCTETS_RD(mmc_rxicmp_err_octets_val);
		MMC_RXICMP_GD_OCTETS_RD(mmc_rxicmp_gd_octets_val);
		MMC_RXTCP_ERR_OCTETS_RD(mmc_rxtcp_err_octets_val);
		MMC_RXTCP_GD_OCTETS_RD(mmc_rxtcp_gd_octets_val);
		MMC_RXUDP_ERR_OCTETS_RD(mmc_rxudp_err_octets_val);
		MMC_RXUDP_GD_OCTETS_RD(mmc_rxudp_gd_octets_val);
		MMC_RXIPV6_NOPAY_OCTETS_RD(MMC_RXIPV6_nopay_octets_val);
		MMC_RXIPV6_HDRERR_OCTETS_RD(MMC_RXIPV6_hdrerr_octets_val);
		MMC_RXIPV6_GD_OCTETS_RD(MMC_RXIPV6_gd_octets_val);
		MMC_RXIPV4_UDSBL_OCTETS_RD(MMC_RXIPV4_udsbl_octets_val);
		MMC_RXIPV4_FRAG_OCTETS_RD(MMC_RXIPV4_frag_octets_val);
		MMC_RXIPV4_NOPAY_OCTETS_RD(MMC_RXIPV4_nopay_octets_val);
		MMC_RXIPV4_HDRERR_OCTETS_RD(MMC_RXIPV4_hdrerr_octets_val);
		MMC_RXIPV4_GD_OCTETS_RD(MMC_RXIPV4_gd_octets_val);
		MMC_RXICMP_ERR_PKTS_RD(mmc_rxicmp_err_pkts_val);
		MMC_RXICMP_GD_PKTS_RD(mmc_rxicmp_gd_pkts_val);
		MMC_RXTCP_ERR_PKTS_RD(mmc_rxtcp_err_pkts_val);
		MMC_RXTCP_GD_PKTS_RD(mmc_rxtcp_gd_pkts_val);
		MMC_RXUDP_ERR_PKTS_RD(mmc_rxudp_err_pkts_val);
		MMC_RXUDP_GD_PKTS_RD(mmc_rxudp_gd_pkts_val);
		MMC_RXIPV6_NOPAY_PKTS_RD(MMC_RXIPV6_nopay_pkts_val);
		MMC_RXIPV6_HDRERR_PKTS_RD(MMC_RXIPV6_hdrerr_pkts_val);
		MMC_RXIPV6_GD_PKTS_RD(MMC_RXIPV6_gd_pkts_val);
		MMC_RXIPV4_UBSBL_PKTS_RD(MMC_RXIPV4_ubsbl_pkts_val);
		MMC_RXIPV4_FRAG_PKTS_RD(MMC_RXIPV4_frag_pkts_val);
		MMC_RXIPV4_NOPAY_PKTS_RD(MMC_RXIPV4_nopay_pkts_val);
		MMC_RXIPV4_HDRERR_PKTS_RD(MMC_RXIPV4_hdrerr_pkts_val);
		MMC_RXIPV4_GD_PKTS_RD(MMC_RXIPV4_gd_pkts_val);
		MMC_RXCTRLPACKETS_G_RD(mmc_rxctrlpackets_g_val);
		MMC_RXRCVERROR_RD(mmc_rxrcverror_val);
		MMC_RXWATCHDOGERROR_RD(mmc_rxwatchdogerror_val);
		MMC_RXVLANPACKETS_GB_RD(mmc_rxvlanpackets_gb_val);
		MMC_RXFIFOOVERFLOW_RD(mmc_rxfifooverflow_val);
		MMC_RXPAUSEPACKETS_RD(mmc_rxpausepackets_val);
		MMC_RXOUTOFRANGETYPE_RD(mmc_rxoutofrangetype_val);
		MMC_RXLENGTHERROR_RD(mmc_rxlengtherror_val);
		MMC_RXUNICASTPACKETS_G_RD(mmc_rxunicastpackets_g_val);
		MMC_RX1024TOMAXOCTETS_GB_RD(MMC_RX1024tomaxoctets_gb_val);
		MMC_RX512TO1023OCTETS_GB_RD(MMC_RX512TO1023octets_gb_val);
		MMC_RX256TO511OCTETS_GB_RD(MMC_RX256TO511octets_gb_val);
		MMC_RX128TO255OCTETS_GB_RD(MMC_RX128TO255octets_gb_val);
		MMC_RX65TO127OCTETS_GB_RD(MMC_RX65TO127octets_gb_val);
		MMC_RX64OCTETS_GB_RD(MMC_RX64octets_gb_val);
		MMC_RXOVERSIZE_G_RD(mmc_rxoversize_g_val);
		MMC_RXUNDERSIZE_G_RD(mmc_rxundersize_g_val);
		MMC_RXJABBERERROR_RD(mmc_rxjabbererror_val);
		MMC_RXRUNTERROR_RD(mmc_rxrunterror_val);
		MMC_RXALIGNMENTERROR_RD(mmc_rxalignmenterror_val);
		MMC_RXCRCERROR_RD(mmc_rxcrcerror_val);
		MMC_RXMULTICASTPACKETS_G_RD(mmc_rxmulticastpackets_g_val);
		MMC_RXBROADCASTPACKETS_G_RD(mmc_rxbroadcastpackets_g_val);
		MMC_RXOCTETCOUNT_G_RD(mmc_rxoctetcount_g_val);
		MMC_RXOCTETCOUNT_GB_RD(mmc_rxoctetcount_gb_val);
		MMC_RXPACKETCOUNT_GB_RD(mmc_rxpacketcount_gb_val);
		MMC_TXOVERSIZE_G_RD(mmc_txoversize_g_val);
		MMC_TXVLANPACKETS_G_RD(mmc_txvlanpackets_g_val);
		MMC_TXPAUSEPACKETS_RD(mmc_txpausepackets_val);
		MMC_TXEXCESSDEF_RD(mmc_txexcessdef_val);
		MMC_TXPACKETSCOUNT_G_RD(mmc_txpacketscount_g_val);
		MMC_TXOCTETCOUNT_G_RD(mmc_txoctetcount_g_val);
		MMC_TXCARRIERERROR_RD(mmc_txcarriererror_val);
		MMC_TXEXESSCOL_RD(mmc_txexesscol_val);
		MMC_TXLATECOL_RD(mmc_txlatecol_val);
		MMC_TXDEFERRED_RD(mmc_txdeferred_val);
		MMC_TXMULTICOL_G_RD(mmc_txmulticol_g_val);
		MMC_TXSINGLECOL_G_RD(mmc_txsinglecol_g_val);
		MMC_TXUNDERFLOWERROR_RD(mmc_txunderflowerror_val);
		MMC_TXBROADCASTPACKETS_GB_RD(mmc_txbroadcastpackets_gb_val);
		MMC_TXMULTICASTPACKETS_GB_RD(mmc_txmulticastpackets_gb_val);
		MMC_TXUNICASTPACKETS_GB_RD(mmc_txunicastpackets_gb_val);
		MMC_TX1024TOMAXOCTETS_GB_RD(MMC_TX1024tomaxoctets_gb_val);
		MMC_TX512TO1023OCTETS_GB_RD(MMC_TX512TO1023octets_gb_val);
		MMC_TX256TO511OCTETS_GB_RD(MMC_TX256TO511octets_gb_val);
		MMC_TX128TO255OCTETS_GB_RD(MMC_TX128TO255octets_gb_val);
		MMC_TX65TO127OCTETS_GB_RD(MMC_TX65TO127octets_gb_val);
		MMC_TX64OCTETS_GB_RD(MMC_TX64octets_gb_val);
		MMC_TXMULTICASTPACKETS_G_RD(mmc_txmulticastpackets_g_val);
		MMC_TXBROADCASTPACKETS_G_RD(mmc_txbroadcastpackets_g_val);
		MMC_TXPACKETCOUNT_GB_RD(mmc_txpacketcount_gb_val);
		MMC_TXOCTETCOUNT_GB_RD(mmc_txoctetcount_gb_val);
		MMC_IPC_INTR_RX_RD(mmc_ipc_intr_rx_val);
		MMC_IPC_INTR_MASK_RX_RD(mmc_ipc_intr_mask_rx_val);
		MMC_INTR_MASK_TX_RD(mmc_intr_mask_tx_val);
		MMC_INTR_MASK_RX_RD(mmc_intr_mask_rx_val);
		MMC_INTR_TX_RD(mmc_intr_tx_val);
		MMC_INTR_RX_RD(mmc_intr_rx_val);
		MMC_CNTRL_RD(mmc_cntrl_val);
	}
	MAC_MA1LR_RD(MAC_MA1lr_val);
	MAC_MA1HR_RD(MAC_MA1hr_val);
	MAC_MA0LR_RD(MAC_MA0lr_val);
	MAC_MA0HR_RD(MAC_MA0hr_val);
	MAC_GPIOR_RD(mac_gpior_val);
	MAC_GMIIDR_RD(mac_gmiidr_val);
	MAC_GMIIAR_RD(mac_gmiiar_val);
	MAC_HFR2_RD(MAC_HFR2_val);
	MAC_HFR1_RD(MAC_HFR1_val);
	MAC_HFR0_RD(MAC_HFR0_val);
	MAC_MDR_RD(mac_mdr_val);
	MAC_VR_RD(mac_vr_val);
	MAC_HTR7_RD(MAC_HTR7_val);
	MAC_HTR6_RD(MAC_HTR6_val);
	MAC_HTR5_RD(MAC_HTR5_val);
	MAC_HTR4_RD(MAC_HTR4_val);
	MAC_HTR3_RD(MAC_HTR3_val);
	MAC_HTR2_RD(MAC_HTR2_val);
	MAC_HTR1_RD(MAC_HTR1_val);
	MAC_HTR0_RD(MAC_HTR0_val);
	DMA_RIWTR7_RD(DMA_RIWTR7_val);
	DMA_RIWTR6_RD(DMA_RIWTR6_val);
	DMA_RIWTR5_RD(DMA_RIWTR5_val);
	DMA_RIWTR4_RD(DMA_RIWTR4_val);
	DMA_RIWTR3_RD(DMA_RIWTR3_val);
	DMA_RIWTR2_RD(DMA_RIWTR2_val);
	DMA_RIWTR1_RD(DMA_RIWTR1_val);
	DMA_RIWTR0_RD(DMA_RIWTR0_val);
	DMA_RDRLR7_RD(DMA_RDRLR7_val);
	DMA_RDRLR6_RD(DMA_RDRLR6_val);
	DMA_RDRLR5_RD(DMA_RDRLR5_val);
	DMA_RDRLR4_RD(DMA_RDRLR4_val);
	DMA_RDRLR3_RD(DMA_RDRLR3_val);
	DMA_RDRLR2_RD(DMA_RDRLR2_val);
	DMA_RDRLR1_RD(DMA_RDRLR1_val);
	DMA_RDRLR0_RD(DMA_RDRLR0_val);
	DMA_TDRLR7_RD(DMA_TDRLR7_val);
	DMA_TDRLR6_RD(DMA_TDRLR6_val);
	DMA_TDRLR5_RD(DMA_TDRLR5_val);
	DMA_TDRLR4_RD(DMA_TDRLR4_val);
	DMA_TDRLR3_RD(DMA_TDRLR3_val);
	DMA_TDRLR2_RD(DMA_TDRLR2_val);
	DMA_TDRLR1_RD(DMA_TDRLR1_val);
	DMA_TDRLR0_RD(DMA_TDRLR0_val);
	DMA_RDTP_RPDR7_RD(DMA_RDTP_RPDR7_val);
	DMA_RDTP_RPDR6_RD(DMA_RDTP_RPDR6_val);
	DMA_RDTP_RPDR5_RD(DMA_RDTP_RPDR5_val);
	DMA_RDTP_RPDR4_RD(DMA_RDTP_RPDR4_val);
	DMA_RDTP_RPDR3_RD(DMA_RDTP_RPDR3_val);
	DMA_RDTP_RPDR2_RD(DMA_RDTP_RPDR2_val);
	DMA_RDTP_RPDR1_RD(DMA_RDTP_RPDR1_val);
	DMA_RDTP_RPDR0_RD(DMA_RDTP_RPDR0_val);
	DMA_TDTP_TPDR7_RD(DMA_TDTP_TPDR7_val);
	DMA_TDTP_TPDR6_RD(DMA_TDTP_TPDR6_val);
	DMA_TDTP_TPDR5_RD(DMA_TDTP_TPDR5_val);
	DMA_TDTP_TPDR4_RD(DMA_TDTP_TPDR4_val);
	DMA_TDTP_TPDR3_RD(DMA_TDTP_TPDR3_val);
	DMA_TDTP_TPDR2_RD(DMA_TDTP_TPDR2_val);
	DMA_TDTP_TPDR1_RD(DMA_TDTP_TPDR1_val);
	DMA_TDTP_TPDR0_RD(DMA_TDTP_TPDR0_val);
	DMA_RDLAR7_RD(DMA_RDLAR7_val);
	DMA_RDLAR6_RD(DMA_RDLAR6_val);
	DMA_RDLAR5_RD(DMA_RDLAR5_val);
	DMA_RDLAR4_RD(DMA_RDLAR4_val);
	DMA_RDLAR3_RD(DMA_RDLAR3_val);
	DMA_RDLAR2_RD(DMA_RDLAR2_val);
	DMA_RDLAR1_RD(DMA_RDLAR1_val);
	DMA_RDLAR0_RD(DMA_RDLAR0_val);
	DMA_TDLAR7_RD(DMA_TDLAR7_val);
	DMA_TDLAR6_RD(DMA_TDLAR6_val);
	DMA_TDLAR5_RD(DMA_TDLAR5_val);
	DMA_TDLAR4_RD(DMA_TDLAR4_val);
	DMA_TDLAR3_RD(DMA_TDLAR3_val);
	DMA_TDLAR2_RD(DMA_TDLAR2_val);
	DMA_TDLAR1_RD(DMA_TDLAR1_val);
	DMA_TDLAR0_RD(DMA_TDLAR0_val);
	DMA_IER_RD(7, DMA_IER7_val);
	DMA_IER_RD(6, DMA_IER6_val);
	DMA_IER_RD(5, DMA_IER5_val);
	DMA_IER_RD(4, DMA_IER4_val);
	DMA_IER_RD(3, DMA_IER3_val);
	DMA_IER_RD(2, DMA_IER2_val);
	DMA_IER_RD(1, DMA_IER1_val);
	DMA_IER_RD(0, DMA_IER0_val);
	MAC_IMR_RD(mac_imr_val);
	MAC_ISR_RD(mac_isr_val);
	MTL_ISR_RD(mtl_isr_val);
	DMA_SR_RD(7, DMA_SR7_val);
	DMA_SR_RD(6, DMA_SR6_val);
	DMA_SR_RD(5, DMA_SR5_val);
	DMA_SR_RD(4, DMA_SR4_val);
	DMA_SR_RD(3, DMA_SR3_val);
	DMA_SR_RD(2, DMA_SR2_val);
	DMA_SR_RD(1, DMA_SR1_val);
	DMA_SR_RD(0, DMA_SR0_val);
	DMA_ISR_RD(dma_isr_val);
	DMA_DSR2_RD(DMA_DSR2_val);
	DMA_DSR1_RD(DMA_DSR1_val);
	DMA_DSR0_RD(DMA_DSR0_val);
	MTL_Q0RDR_RD(MTL_Q0rdr_val);
	MTL_Q0ESR_RD(MTL_Q0esr_val);
	MTL_Q0TDR_RD(MTL_Q0tdr_val);
	DMA_CHRBAR7_RD(DMA_CHRBAR7_val);
	DMA_CHRBAR6_RD(DMA_CHRBAR6_val);
	DMA_CHRBAR5_RD(DMA_CHRBAR5_val);
	DMA_CHRBAR4_RD(DMA_CHRBAR4_val);
	DMA_CHRBAR3_RD(DMA_CHRBAR3_val);
	DMA_CHRBAR2_RD(DMA_CHRBAR2_val);
	DMA_CHRBAR1_RD(DMA_CHRBAR1_val);
	DMA_CHRBAR0_RD(DMA_CHRBAR0_val);
	DMA_CHTBAR7_RD(DMA_CHTBAR7_val);
	DMA_CHTBAR6_RD(DMA_CHTBAR6_val);
	DMA_CHTBAR5_RD(DMA_CHTBAR5_val);
	DMA_CHTBAR4_RD(DMA_CHTBAR4_val);
	DMA_CHTBAR3_RD(DMA_CHTBAR3_val);
	DMA_CHTBAR2_RD(DMA_CHTBAR2_val);
	DMA_CHTBAR1_RD(DMA_CHTBAR1_val);
	DMA_CHTBAR0_RD(DMA_CHTBAR0_val);
	DMA_CHRDR7_RD(DMA_CHRDR7_val);
	DMA_CHRDR6_RD(DMA_CHRDR6_val);
	DMA_CHRDR5_RD(DMA_CHRDR5_val);
	DMA_CHRDR4_RD(DMA_CHRDR4_val);
	DMA_CHRDR3_RD(DMA_CHRDR3_val);
	DMA_CHRDR2_RD(DMA_CHRDR2_val);
	DMA_CHRDR1_RD(DMA_CHRDR1_val);
	DMA_CHRDR0_RD(DMA_CHRDR0_val);
	DMA_CHTDR7_RD(DMA_CHTDR7_val);
	DMA_CHTDR6_RD(DMA_CHTDR6_val);
	DMA_CHTDR5_RD(DMA_CHTDR5_val);
	DMA_CHTDR4_RD(DMA_CHTDR4_val);
	DMA_CHTDR3_RD(DMA_CHTDR3_val);
	DMA_CHTDR2_RD(DMA_CHTDR2_val);
	DMA_CHTDR1_RD(DMA_CHTDR1_val);
	DMA_CHTDR0_RD(DMA_CHTDR0_val);
	DMA_SFCSR7_RD(DMA_SFCSR7_val);
	DMA_SFCSR6_RD(DMA_SFCSR6_val);
	DMA_SFCSR5_RD(DMA_SFCSR5_val);
	DMA_SFCSR4_RD(DMA_SFCSR4_val);
	DMA_SFCSR3_RD(DMA_SFCSR3_val);
	DMA_SFCSR2_RD(DMA_SFCSR2_val);
	DMA_SFCSR1_RD(DMA_SFCSR1_val);
	DMA_SFCSR0_RD(DMA_SFCSR0_val);
	MAC_IVLANTIRR_RD(mac_ivlantirr_val);
	MAC_VLANTIRR_RD(mac_vlantirr_val);
	MAC_VLANHTR_RD(mac_vlanhtr_val);
	MAC_VLANTR_RD(mac_vlantr_val);
	DMA_SBUS_RD(dma_sbus_val);
	DMA_BMR_RD(dma_bmr_val);
	MTL_Q0RCR_RD(MTL_Q0rcr_val);
	MTL_Q0OCR_RD(MTL_Q0ocr_val);
	MTL_Q0ROMR_RD(MTL_Q0romr_val);
	MTL_Q0QR_RD(MTL_Q0qr_val);
	MTL_Q0ECR_RD(MTL_Q0ecr_val);
	MTL_Q0UCR_RD(MTL_Q0ucr_val);
	MTL_Q0TOMR_RD(MTL_Q0tomr_val);
	MTL_RQDCM1R_RD(MTL_RQDCM1r_val);
	MTL_RQDCM0R_RD(MTL_RQDCM0r_val);
	MTL_FDDR_RD(mtl_fddr_val);
	MTL_FDACS_RD(mtl_fdacs_val);
	MTL_OMR_RD(mtl_omr_val);
	MAC_RQC3R_RD(MAC_RQC3r_val);
	MAC_RQC2R_RD(MAC_RQC2r_val);
	MAC_RQC1R_RD(MAC_RQC1r_val);
	MAC_RQC0R_RD(MAC_RQC0r_val);
	MAC_TQPM1R_RD(MAC_TQPM1r_val);
	MAC_TQPM0R_RD(MAC_TQPM0r_val);
	MAC_RFCR_RD(mac_rfcr_val);
	MAC_QTFCR7_RD(MAC_QTFCR7_val);
	MAC_QTFCR6_RD(MAC_QTFCR6_val);
	MAC_QTFCR5_RD(MAC_QTFCR5_val);
	MAC_QTFCR4_RD(MAC_QTFCR4_val);
	MAC_QTFCR3_RD(MAC_QTFCR3_val);
	MAC_QTFCR2_RD(MAC_QTFCR2_val);
	MAC_QTFCR1_RD(MAC_QTFCR1_val);
	MAC_Q0TFCR_RD(MAC_Q0tfcr_val);
	DMA_AXI4CR7_RD(DMA_AXI4CR7_val);
	DMA_AXI4CR6_RD(DMA_AXI4CR6_val);
	DMA_AXI4CR5_RD(DMA_AXI4CR5_val);
	DMA_AXI4CR4_RD(DMA_AXI4CR4_val);
	DMA_AXI4CR3_RD(DMA_AXI4CR3_val);
	DMA_AXI4CR2_RD(DMA_AXI4CR2_val);
	DMA_AXI4CR1_RD(DMA_AXI4CR1_val);
	DMA_AXI4CR0_RD(DMA_AXI4CR0_val);
	DMA_RCR7_RD(DMA_RCR7_val);
	DMA_RCR6_RD(DMA_RCR6_val);
	DMA_RCR5_RD(DMA_RCR5_val);
	DMA_RCR4_RD(DMA_RCR4_val);
	DMA_RCR3_RD(DMA_RCR3_val);
	DMA_RCR2_RD(DMA_RCR2_val);
	DMA_RCR1_RD(DMA_RCR1_val);
	DMA_RCR0_RD(DMA_RCR0_val);
	DMA_TCR7_RD(DMA_TCR7_val);
	DMA_TCR6_RD(DMA_TCR6_val);
	DMA_TCR5_RD(DMA_TCR5_val);
	DMA_TCR4_RD(DMA_TCR4_val);
	DMA_TCR3_RD(DMA_TCR3_val);
	DMA_TCR2_RD(DMA_TCR2_val);
	DMA_TCR1_RD(DMA_TCR1_val);
	DMA_TCR0_RD(DMA_TCR0_val);
	DMA_CR7_RD(DMA_CR7_val);
	DMA_CR6_RD(DMA_CR6_val);
	DMA_CR5_RD(DMA_CR5_val);
	DMA_CR4_RD(DMA_CR4_val);
	DMA_CR3_RD(DMA_CR3_val);
	DMA_CR2_RD(DMA_CR2_val);
	DMA_CR1_RD(DMA_CR1_val);
	DMA_CR0_RD(DMA_CR0_val);
	MAC_WTR_RD(mac_wtr_val);
	MAC_MPFR_RD(mac_mpfr_val);
	MAC_MECR_RD(mac_mecr_val);
	MAC_MCR_RD(mac_mcr_val);
	/* For MII/GMII register read */
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR,
				     &mii_bmcr_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMSR,
				     &mii_bmsr_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID1,
				     &MII_PHYSID1_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID2,
				     &MII_PHYSID2_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ADVERTISE,
				     &mii_advertise_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_LPA,
				     &mii_lpa_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_EXPANSION,
				     &mii_expansion_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr,
				     EQOS_AUTO_NEGO_NP,
				     &auto_nego_np_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ESTATUS,
				     &mii_estatus_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_CTRL1000,
				     &MII_CTRL1000_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_STAT1000,
				     &MII_STAT1000_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, EQOS_PHY_CTL,
				     &phy_ctl_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, EQOS_PHY_STS,
				     &phy_sts_reg_val);

	debug_buf = kmalloc(26820, GFP_KERNEL);

	sprintf(debug_buf,
		"MAC_MA32_127LR127            :%#x\n"
		"MAC_MA32_127LR126            :%#x\n"
		"MAC_MA32_127LR125            :%#x\n"
		"MAC_MA32_127LR124            :%#x\n"
		"MAC_MA32_127LR123            :%#x\n"
		"MAC_MA32_127LR122            :%#x\n"
		"MAC_MA32_127LR121            :%#x\n"
		"MAC_MA32_127LR120            :%#x\n"
		"MAC_MA32_127LR119            :%#x\n"
		"MAC_MA32_127LR118            :%#x\n"
		"MAC_MA32_127LR117            :%#x\n"
		"MAC_MA32_127LR116            :%#x\n"
		"MAC_MA32_127LR115            :%#x\n"
		"MAC_MA32_127LR114            :%#x\n"
		"MAC_MA32_127LR113            :%#x\n"
		"MAC_MA32_127LR112            :%#x\n"
		"MAC_MA32_127LR111            :%#x\n"
		"MAC_MA32_127LR110            :%#x\n"
		"MAC_MA32_127LR109            :%#x\n"
		"MAC_MA32_127LR108            :%#x\n"
		"MAC_MA32_127LR107            :%#x\n"
		"MAC_MA32_127LR106            :%#x\n"
		"MAC_MA32_127LR105            :%#x\n"
		"MAC_MA32_127LR104            :%#x\n"
		"MAC_MA32_127LR103            :%#x\n"
		"MAC_MA32_127LR102            :%#x\n"
		"MAC_MA32_127LR101            :%#x\n"
		"MAC_MA32_127LR100            :%#x\n"
		"MAC_MA32_127LR99            :%#x\n"
		"MAC_MA32_127LR98            :%#x\n"
		"MAC_MA32_127LR97            :%#x\n"
		"MAC_MA32_127LR96            :%#x\n"
		"MAC_MA32_127LR95            :%#x\n"
		"MAC_MA32_127LR94            :%#x\n"
		"MAC_MA32_127LR93            :%#x\n"
		"MAC_MA32_127LR92            :%#x\n"
		"MAC_MA32_127LR91            :%#x\n"
		"MAC_MA32_127LR90            :%#x\n"
		"MAC_MA32_127LR89            :%#x\n"
		"MAC_MA32_127LR88            :%#x\n"
		"MAC_MA32_127LR87            :%#x\n"
		"MAC_MA32_127LR86            :%#x\n"
		"MAC_MA32_127LR85            :%#x\n"
		"MAC_MA32_127LR84            :%#x\n"
		"MAC_MA32_127LR83            :%#x\n"
		"MAC_MA32_127LR82            :%#x\n"
		"MAC_MA32_127LR81            :%#x\n"
		"MAC_MA32_127LR80            :%#x\n"
		"MAC_MA32_127LR79            :%#x\n"
		"MAC_MA32_127LR78            :%#x\n"
		"MAC_MA32_127LR77            :%#x\n"
		"MAC_MA32_127LR76            :%#x\n"
		"MAC_MA32_127LR75            :%#x\n"
		"MAC_MA32_127LR74            :%#x\n"
		"MAC_MA32_127LR73            :%#x\n"
		"MAC_MA32_127LR72            :%#x\n"
		"MAC_MA32_127LR71            :%#x\n"
		"MAC_MA32_127LR70            :%#x\n"
		"MAC_MA32_127LR69            :%#x\n"
		"MAC_MA32_127LR68            :%#x\n"
		"MAC_MA32_127LR67            :%#x\n"
		"MAC_MA32_127LR66            :%#x\n"
		"MAC_MA32_127LR65            :%#x\n"
		"MAC_MA32_127LR64            :%#x\n"
		"MAC_MA32_127LR63            :%#x\n"
		"MAC_MA32_127LR62            :%#x\n"
		"MAC_MA32_127LR61            :%#x\n"
		"MAC_MA32_127LR60            :%#x\n"
		"MAC_MA32_127LR59            :%#x\n"
		"MAC_MA32_127LR58            :%#x\n"
		"MAC_MA32_127LR57            :%#x\n"
		"MAC_MA32_127LR56            :%#x\n"
		"MAC_MA32_127LR55            :%#x\n"
		"MAC_MA32_127LR54            :%#x\n"
		"MAC_MA32_127LR53            :%#x\n"
		"MAC_MA32_127LR52            :%#x\n"
		"MAC_MA32_127LR51            :%#x\n"
		"MAC_MA32_127LR50            :%#x\n"
		"MAC_MA32_127LR49            :%#x\n"
		"MAC_MA32_127LR48            :%#x\n"
		"MAC_MA32_127LR47            :%#x\n"
		"MAC_MA32_127LR46            :%#x\n"
		"MAC_MA32_127LR45            :%#x\n"
		"MAC_MA32_127LR44            :%#x\n"
		"MAC_MA32_127LR43            :%#x\n"
		"MAC_MA32_127LR42            :%#x\n"
		"MAC_MA32_127LR41            :%#x\n"
		"MAC_MA32_127LR40            :%#x\n"
		"MAC_MA32_127LR39            :%#x\n"
		"MAC_MA32_127LR38            :%#x\n"
		"MAC_MA32_127LR37            :%#x\n"
		"MAC_MA32_127LR36            :%#x\n"
		"MAC_MA32_127LR35            :%#x\n"
		"MAC_MA32_127LR34            :%#x\n"
		"MAC_MA32_127LR33            :%#x\n"
		"MAC_MA32_127LR32            :%#x\n"
		"MAC_MA32_127HR127            :%#x\n"
		"MAC_MA32_127HR126            :%#x\n"
		"MAC_MA32_127HR125            :%#x\n"
		"MAC_MA32_127HR124            :%#x\n"
		"MAC_MA32_127HR123            :%#x\n"
		"MAC_MA32_127HR122            :%#x\n"
		"MAC_MA32_127HR121            :%#x\n"
		"MAC_MA32_127HR120            :%#x\n"
		"MAC_MA32_127HR119            :%#x\n"
		"MAC_MA32_127HR118            :%#x\n"
		"MAC_MA32_127HR117            :%#x\n"
		"MAC_MA32_127HR116            :%#x\n"
		"MAC_MA32_127HR115            :%#x\n"
		"MAC_MA32_127HR114            :%#x\n"
		"MAC_MA32_127HR113            :%#x\n"
		"MAC_MA32_127HR112            :%#x\n"
		"MAC_MA32_127HR111            :%#x\n"
		"MAC_MA32_127HR110            :%#x\n"
		"MAC_MA32_127HR109            :%#x\n"
		"MAC_MA32_127HR108            :%#x\n"
		"MAC_MA32_127HR107            :%#x\n"
		"MAC_MA32_127HR106            :%#x\n"
		"MAC_MA32_127HR105            :%#x\n"
		"MAC_MA32_127HR104            :%#x\n"
		"MAC_MA32_127HR103            :%#x\n"
		"MAC_MA32_127HR102            :%#x\n"
		"MAC_MA32_127HR101            :%#x\n"
		"MAC_MA32_127HR100            :%#x\n"
		"MAC_MA32_127HR99            :%#x\n"
		"MAC_MA32_127HR98            :%#x\n"
		"MAC_MA32_127HR97            :%#x\n"
		"MAC_MA32_127HR96            :%#x\n"
		"MAC_MA32_127HR95            :%#x\n"
		"MAC_MA32_127HR94            :%#x\n"
		"MAC_MA32_127HR93            :%#x\n"
		"MAC_MA32_127HR92            :%#x\n"
		"MAC_MA32_127HR91            :%#x\n"
		"MAC_MA32_127HR90            :%#x\n"
		"MAC_MA32_127HR89            :%#x\n"
		"MAC_MA32_127HR88            :%#x\n"
		"MAC_MA32_127HR87            :%#x\n"
		"MAC_MA32_127HR86            :%#x\n"
		"MAC_MA32_127HR85            :%#x\n"
		"MAC_MA32_127HR84            :%#x\n"
		"MAC_MA32_127HR83            :%#x\n"
		"MAC_MA32_127HR82            :%#x\n"
		"MAC_MA32_127HR81            :%#x\n"
		"MAC_MA32_127HR80            :%#x\n"
		"MAC_MA32_127HR79            :%#x\n"
		"MAC_MA32_127HR78            :%#x\n"
		"MAC_MA32_127HR77            :%#x\n"
		"MAC_MA32_127HR76            :%#x\n"
		"MAC_MA32_127HR75            :%#x\n"
		"MAC_MA32_127HR74            :%#x\n"
		"MAC_MA32_127HR73            :%#x\n"
		"MAC_MA32_127HR72            :%#x\n"
		"MAC_MA32_127HR71            :%#x\n"
		"MAC_MA32_127HR70            :%#x\n"
		"MAC_MA32_127HR69            :%#x\n"
		"MAC_MA32_127HR68            :%#x\n"
		"MAC_MA32_127HR67            :%#x\n"
		"MAC_MA32_127HR66            :%#x\n"
		"MAC_MA32_127HR65            :%#x\n"
		"MAC_MA32_127HR64            :%#x\n"
		"MAC_MA32_127HR63            :%#x\n"
		"MAC_MA32_127HR62            :%#x\n"
		"MAC_MA32_127HR61            :%#x\n"
		"MAC_MA32_127HR60            :%#x\n"
		"MAC_MA32_127HR59            :%#x\n"
		"MAC_MA32_127HR58            :%#x\n"
		"MAC_MA32_127HR57            :%#x\n"
		"MAC_MA32_127HR56            :%#x\n"
		"MAC_MA32_127HR55            :%#x\n"
		"MAC_MA32_127HR54            :%#x\n"
		"MAC_MA32_127HR53            :%#x\n"
		"MAC_MA32_127HR52            :%#x\n"
		"MAC_MA32_127HR51            :%#x\n"
		"MAC_MA32_127HR50            :%#x\n"
		"MAC_MA32_127HR49            :%#x\n"
		"MAC_MA32_127HR48            :%#x\n"
		"MAC_MA32_127HR47            :%#x\n"
		"MAC_MA32_127HR46            :%#x\n"
		"MAC_MA32_127HR45            :%#x\n"
		"MAC_MA32_127HR44            :%#x\n"
		"MAC_MA32_127HR43            :%#x\n"
		"MAC_MA32_127HR42            :%#x\n"
		"MAC_MA32_127HR41            :%#x\n"
		"MAC_MA32_127HR40            :%#x\n"
		"MAC_MA32_127HR39            :%#x\n"
		"MAC_MA32_127HR38            :%#x\n"
		"MAC_MA32_127HR37            :%#x\n"
		"MAC_MA32_127HR36            :%#x\n"
		"MAC_MA32_127HR35            :%#x\n"
		"MAC_MA32_127HR34            :%#x\n"
		"MAC_MA32_127HR33            :%#x\n"
		"MAC_MA32_127HR32            :%#x\n"
		"MAC_MA1_31LR31              :%#x\n"
		"MAC_MA1_31LR30              :%#x\n"
		"MAC_MA1_31LR29              :%#x\n"
		"MAC_MA1_31LR28              :%#x\n"
		"MAC_MA1_31LR27              :%#x\n"
		"MAC_MA1_31LR26              :%#x\n"
		"MAC_MA1_31LR25              :%#x\n"
		"MAC_MA1_31LR24              :%#x\n"
		"MAC_MA1_31LR23              :%#x\n"
		"MAC_MA1_31LR22              :%#x\n"
		"MAC_MA1_31LR21              :%#x\n"
		"MAC_MA1_31LR20              :%#x\n"
		"MAC_MA1_31LR19              :%#x\n"
		"MAC_MA1_31LR18              :%#x\n"
		"MAC_MA1_31LR17              :%#x\n"
		"MAC_MA1_31LR16              :%#x\n"
		"MAC_MA1_31LR15              :%#x\n"
		"MAC_MA1_31LR14              :%#x\n"
		"MAC_MA1_31LR13              :%#x\n"
		"MAC_MA1_31LR12              :%#x\n"
		"MAC_MA1_31LR11              :%#x\n"
		"MAC_MA1_31LR10              :%#x\n"
		"MAC_MA1_31LR9              :%#x\n"
		"MAC_MA1_31LR8              :%#x\n"
		"MAC_MA1_31LR7              :%#x\n"
		"MAC_MA1_31LR6              :%#x\n"
		"MAC_MA1_31LR5              :%#x\n"
		"MAC_MA1_31LR4              :%#x\n"
		"MAC_MA1_31LR3              :%#x\n"
		"MAC_MA1_31LR2              :%#x\n"
		"MAC_MA1_31LR1              :%#x\n"
		"MAC_MA1_31HR31              :%#x\n"
		"MAC_MA1_31HR30              :%#x\n"
		"MAC_MA1_31HR29              :%#x\n"
		"MAC_MA1_31HR28              :%#x\n"
		"MAC_MA1_31HR27              :%#x\n"
		"MAC_MA1_31HR26              :%#x\n"
		"MAC_MA1_31HR25              :%#x\n"
		"MAC_MA1_31HR24              :%#x\n"
		"MAC_MA1_31HR23              :%#x\n"
		"MAC_MA1_31HR22              :%#x\n"
		"MAC_MA1_31HR21              :%#x\n"
		"MAC_MA1_31HR20              :%#x\n"
		"MAC_MA1_31HR19              :%#x\n"
		"MAC_MA1_31HR18              :%#x\n"
		"MAC_MA1_31HR17              :%#x\n"
		"MAC_MA1_31HR16              :%#x\n"
		"MAC_MA1_31HR15              :%#x\n"
		"MAC_MA1_31HR14              :%#x\n"
		"MAC_MA1_31HR13              :%#x\n"
		"MAC_MA1_31HR12              :%#x\n"
		"MAC_MA1_31HR11              :%#x\n"
		"MAC_MA1_31HR10              :%#x\n"
		"MAC_MA1_31HR9              :%#x\n"
		"MAC_MA1_31HR8              :%#x\n"
		"MAC_MA1_31HR7              :%#x\n"
		"MAC_MA1_31HR6              :%#x\n"
		"MAC_MA1_31HR5              :%#x\n"
		"MAC_MA1_31HR4              :%#x\n"
		"MAC_MA1_31HR3              :%#x\n"
		"MAC_MA1_31HR2              :%#x\n"
		"MAC_MA1_31HR1              :%#x\n"
		"MAC_ARPA                   :%#x\n"
		"MAC_L3A3R7                 :%#x\n"
		"MAC_L3A3R6                 :%#x\n"
		"MAC_L3A3R5                 :%#x\n"
		"MAC_L3A3R4                 :%#x\n"
		"MAC_L3A3R3                 :%#x\n"
		"MAC_L3A3R2                 :%#x\n"
		"MAC_L3A3R1                 :%#x\n"
		"MAC_L3A3R0                 :%#x\n"
		"MAC_L3A2R7                 :%#x\n"
		"MAC_L3A2R6                 :%#x\n"
		"MAC_L3A2R5                 :%#x\n"
		"MAC_L3A2R4                 :%#x\n"
		"MAC_L3A2R3                 :%#x\n"
		"MAC_L3A2R2                 :%#x\n"
		"MAC_L3A2R1                 :%#x\n"
		"MAC_L3A2R0                 :%#x\n"
		"MAC_L3A1R7                 :%#x\n"
		"MAC_L3A1R6                 :%#x\n"
		"MAC_L3A1R5                 :%#x\n"
		"MAC_L3A1R4                 :%#x\n"
		"MAC_L3A1R3                 :%#x\n"
		"MAC_L3A1R2                 :%#x\n"
		"MAC_L3A1R1                 :%#x\n"
		"MAC_L3A1R0                 :%#x\n"
		"MAC_L3A0R7                 :%#x\n"
		"MAC_L3A0R6                 :%#x\n"
		"MAC_L3A0R5                 :%#x\n"
		"MAC_L3A0R4                 :%#x\n"
		"MAC_L3A0R3                 :%#x\n"
		"MAC_L3A0R2                 :%#x\n"
		"MAC_L3A0R1                 :%#x\n"
		"MAC_L3A0R0                 :%#x\n"
		"MAC_L4AR7                 :%#x\n"
		"MAC_L4AR6                 :%#x\n"
		"MAC_L4AR5                 :%#x\n"
		"MAC_L4AR4                 :%#x\n"
		"MAC_L4AR3                 :%#x\n"
		"MAC_L4AR2                 :%#x\n"
		"MAC_L4AR1                 :%#x\n"
		"MAC_L4AR0                 :%#x\n"
		"MAC_L3L4CR7                :%#x\n"
		"MAC_L3L4CR6                :%#x\n"
		"MAC_L3L4CR5                :%#x\n"
		"MAC_L3L4CR4                :%#x\n"
		"MAC_L3L4CR3                :%#x\n"
		"MAC_L3L4CR2                :%#x\n"
		"MAC_L3L4CR1                :%#x\n"
		"MAC_L3L4CR0                :%#x\n"
		"MAC_GPIOS                  :%#x\n"
		"MAC_PCS                    :%#x\n"
		"MAC_TES                    :%#x\n"
		"MAC_AE                     :%#x\n"
		"MAC_ALPA                   :%#x\n"
		"MAC_AAD                    :%#x\n"
		"MAC_ANS                    :%#x\n"
		"MAC_ANC                    :%#x\n"
		"MAC_LPC                    :%#x\n"
		"MAC_LPS                    :%#x\n"
		"MAC_LMIR                   :%#x\n"
		"MAC_SPI2R                  :%#x\n"
		"MAC_SPI1R                  :%#x\n"
		"MAC_SPI0R                  :%#x\n"
		"MAC_PTO_CR                 :%#x\n"
		"MAC_PPS_WIDTH3             :%#x\n"
		"MAC_PPS_WIDTH2             :%#x\n"
		"MAC_PPS_WIDTH1             :%#x\n"
		"MAC_PPS_WIDTH0             :%#x\n"
		"MAC_PPS_INTVAL3            :%#x\n"
		"MAC_PPS_INTVAL2            :%#x\n"
		"MAC_PPS_INTVAL1            :%#x\n"
		"MAC_PPS_INTVAL0            :%#x\n"
		"MAC_PPS_TTNS3              :%#x\n"
		"MAC_PPS_TTNS2              :%#x\n"
		"MAC_PPS_TTNS1              :%#x\n"
		"MAC_PPS_TTNS0              :%#x\n"
		"MAC_PPS_TTS3               :%#x\n"
		"MAC_PPS_TTS2               :%#x\n"
		"MAC_PPS_TTS1               :%#x\n"
		"MAC_PPS_TTS0               :%#x\n"
		"MAC_PPSC                   :%#x\n"
		"MAC_TEAC                   :%#x\n"
		"MAC_TIAC                   :%#x\n"
		"MAC_ATS                    :%#x\n"
		"MAC_ATN                    :%#x\n"
		"MAC_AC                     :%#x\n"
		"MAC_TTN                    :%#x\n"
		"MAC_TTSN                   :%#x\n"
		"MAC_TSR                    :%#x\n"
		"MAC_STHWR                  :%#x\n"
		"MAC_TAR                    :%#x\n"
		"MAC_STNSUR                 :%#x\n"
		"MAC_STSUR                  :%#x\n"
		"MAC_STNSR                  :%#x\n"
		"MAC_STSR                   :%#x\n"
		"MAC_SSIR                   :%#x\n"
		"MAC_TCR                    :%#x\n"
		"MTL_DSR                    :%#x\n"
		"MAC_RWPFFR                 :%#x\n"
		"MAC_RTSR                   :%#x\n"
		"MTL_IER                    :%#x\n"
		"MTL_QRCR7                  :%#x\n"
		"MTL_QRCR6                  :%#x\n"
		"MTL_QRCR5                  :%#x\n"
		"MTL_QRCR4                  :%#x\n"
		"MTL_QRCR3                  :%#x\n"
		"MTL_QRCR2                  :%#x\n"
		"MTL_QRCR1                  :%#x\n"
		"MTL_QRDR7                  :%#x\n"
		"MTL_QRDR6                  :%#x\n"
		"MTL_QRDR5                  :%#x\n"
		"MTL_QRDR4                  :%#x\n"
		"MTL_QRDR3                  :%#x\n"
		"MTL_QRDR2                  :%#x\n"
		"MTL_QRDR1                  :%#x\n"
		"MTL_QOCR7                  :%#x\n"
		"MTL_QOCR6                  :%#x\n"
		"MTL_QOCR5                  :%#x\n"
		"MTL_QOCR4                  :%#x\n"
		"MTL_QOCR3                  :%#x\n"
		"MTL_QOCR2                  :%#x\n"
		"MTL_QOCR1                  :%#x\n"
		"MTL_QROMR7                 :%#x\n"
		"MTL_QROMR6                 :%#x\n"
		"MTL_QROMR5                 :%#x\n"
		"MTL_QROMR4                 :%#x\n"
		"MTL_QROMR3                 :%#x\n"
		"MTL_QROMR2                 :%#x\n"
		"MTL_QROMR1                 :%#x\n"
		"MTL_QLCR7                  :%#x\n"
		"MTL_QLCR6                  :%#x\n"
		"MTL_QLCR5                  :%#x\n"
		"MTL_QLCR4                  :%#x\n"
		"MTL_QLCR3                  :%#x\n"
		"MTL_QLCR2                  :%#x\n"
		"MTL_QLCR1                  :%#x\n"
		"MTL_QHCR7                  :%#x\n"
		"MTL_QHCR6                  :%#x\n"
		"MTL_QHCR5                  :%#x\n"
		"MTL_QHCR4                  :%#x\n"
		"MTL_QHCR3                  :%#x\n"
		"MTL_QHCR2                  :%#x\n"
		"MTL_QHCR1                  :%#x\n"
		"MTL_QSSCR7                 :%#x\n"
		"MTL_QSSCR6                 :%#x\n"
		"MTL_QSSCR5                 :%#x\n"
		"MTL_QSSCR4                 :%#x\n"
		"MTL_QSSCR3                 :%#x\n"
		"MTL_QSSCR2                 :%#x\n"
		"MTL_QSSCR1                 :%#x\n"
		"MTL_QW7                    :%#x\n"
		"MTL_QW6                    :%#x\n"
		"MTL_QW5                    :%#x\n"
		"MTL_QW4                    :%#x\n"
		"MTL_QW3                    :%#x\n"
		"MTL_QW2                    :%#x\n"
		"MTL_QW1                    :%#x\n"
		"MTL_QESR7                  :%#x\n"
		"MTL_QESR6                  :%#x\n"
		"MTL_QESR5                  :%#x\n"
		"MTL_QESR4                  :%#x\n"
		"MTL_QESR3                  :%#x\n"
		"MTL_QESR2                  :%#x\n"
		"MTL_QESR1                  :%#x\n"
		"MTL_QECR7                  :%#x\n"
		"MTL_QECR6                  :%#x\n"
		"MTL_QECR5                  :%#x\n"
		"MTL_QECR4                  :%#x\n"
		"MTL_QECR3                  :%#x\n"
		"MTL_QECR2                  :%#x\n"
		"MTL_QECR1                  :%#x\n"
		"MTL_QTDR7                  :%#x\n"
		"MTL_QTDR6                  :%#x\n"
		"MTL_QTDR5                  :%#x\n"
		"MTL_QTDR4                  :%#x\n"
		"MTL_QTDR3                  :%#x\n"
		"MTL_QTDR2                  :%#x\n"
		"MTL_QTDR1                  :%#x\n"
		"MTL_QUCR7                  :%#x\n"
		"MTL_QUCR6                  :%#x\n"
		"MTL_QUCR5                  :%#x\n"
		"MTL_QUCR4                  :%#x\n"
		"MTL_QUCR3                  :%#x\n"
		"MTL_QUCR2                  :%#x\n"
		"MTL_QUCR1                  :%#x\n"
		"MTL_QTOMR7                 :%#x\n"
		"MTL_QTOMR6                 :%#x\n"
		"MTL_QTOMR5                 :%#x\n"
		"MTL_QTOMR4                 :%#x\n"
		"MTL_QTOMR3                 :%#x\n"
		"MTL_QTOMR2                 :%#x\n"
		"MTL_QTOMR1                 :%#x\n"
		"MAC_PMTCSR                 :%#x\n"
		"MMC_RXICMP_ERR_OCTETS      :%#x\n"
		"MMC_RXICMP_GD_OCTETS       :%#x\n"
		"MMC_RXTCP_ERR_OCTETS       :%#x\n"
		"MMC_RXTCP_GD_OCTETS        :%#x\n"
		"MMC_RXUDP_ERR_OCTETS       :%#x\n"
		"MMC_RXUDP_GD_OCTETS        :%#x\n"
		"MMC_RXIPV6_NOPAY_OCTETS    :%#x\n"
		"MMC_RXIPV6_HDRERR_OCTETS   :%#x\n"
		"MMC_RXIPV6_GD_OCTETS       :%#x\n"
		"MMC_RXIPV4_UDSBL_OCTETS    :%#x\n"
		"MMC_RXIPV4_FRAG_OCTETS     :%#x\n"
		"MMC_RXIPV4_NOPAY_OCTETS    :%#x\n"
		"MMC_RXIPV4_HDRERR_OCTETS   :%#x\n"
		"MMC_RXIPV4_GD_OCTETS       :%#x\n"
		"MMC_RXICMP_ERR_PKTS        :%#x\n"
		"MMC_RXICMP_GD_PKTS         :%#x\n"
		"MMC_RXTCP_ERR_PKTS         :%#x\n"
		"MMC_RXTCP_GD_PKTS          :%#x\n"
		"MMC_RXUDP_ERR_PKTS         :%#x\n"
		"MMC_RXUDP_GD_PKTS          :%#x\n"
		"MMC_RXIPV6_NOPAY_PKTS      :%#x\n"
		"MMC_RXIPV6_HDRERR_PKTS     :%#x\n"
		"MMC_RXIPV6_GD_PKTS         :%#x\n"
		"MMC_RXIPV4_UBSBL_PKTS      :%#x\n"
		"MMC_RXIPV4_FRAG_PKTS       :%#x\n"
		"MMC_RXIPV4_NOPAY_PKTS      :%#x\n"
		"MMC_RXIPV4_HDRERR_PKTS     :%#x\n"
		"MMC_RXIPV4_GD_PKTS         :%#x\n"
		"MMC_RXCTRLPACKETS_G        :%#x\n"
		"MMC_RXRCVERROR             :%#x\n"
		"MMC_RXWATCHDOGERROR        :%#x\n"
		"MMC_RXVLANPACKETS_GB       :%#x\n"
		"MMC_RXFIFOOVERFLOW         :%#x\n"
		"MMC_RXPAUSEPACKETS         :%#x\n"
		"MMC_RXOUTOFRANGETYPE       :%#x\n"
		"MMC_RXLENGTHERROR          :%#x\n"
		"MMC_RXUNICASTPACKETS_G     :%#x\n"
		"MMC_RX1024TOMAXOCTETS_GB   :%#x\n"
		"MMC_RX512TO1023OCTETS_GB   :%#x\n"
		"MMC_RX256TO511OCTETS_GB    :%#x\n"
		"MMC_RX128TO255OCTETS_GB    :%#x\n"
		"MMC_RX65TO127OCTETS_GB     :%#x\n"
		"MMC_RX64OCTETS_GB          :%#x\n"
		"MMC_RXOVERSIZE_G           :%#x\n"
		"MMC_RXUNDERSIZE_G          :%#x\n"
		"MMC_RXJABBERERROR          :%#x\n"
		"MMC_RXRUNTERROR            :%#x\n"
		"MMC_RXALIGNMENTERROR       :%#x\n"
		"MMC_RXCRCERROR             :%#x\n"
		"MMC_RXMULTICASTPACKETS_G   :%#x\n"
		"MMC_RXBROADCASTPACKETS_G   :%#x\n"
		"MMC_RXOCTETCOUNT_G         :%#x\n"
		"MMC_RXOCTETCOUNT_GB        :%#x\n"
		"MMC_RXPACKETCOUNT_GB       :%#x\n"
		"MMC_TXOVERSIZE_G           :%#x\n"
		"MMC_TXVLANPACKETS_G        :%#x\n"
		"MMC_TXPAUSEPACKETS         :%#x\n"
		"MMC_TXEXCESSDEF            :%#x\n"
		"MMC_TXPACKETSCOUNT_G       :%#x\n"
		"MMC_TXOCTETCOUNT_G         :%#x\n"
		"MMC_TXCARRIERERROR         :%#x\n"
		"MMC_TXEXESSCOL             :%#x\n"
		"MMC_TXLATECOL              :%#x\n"
		"MMC_TXDEFERRED             :%#x\n"
		"MMC_TXMULTICOL_G           :%#x\n"
		"MMC_TXSINGLECOL_G          :%#x\n"
		"MMC_TXUNDERFLOWERROR       :%#x\n"
		"MMC_TXBROADCASTPACKETS_GB  :%#x\n"
		"MMC_TXMULTICASTPACKETS_GB  :%#x\n"
		"MMC_TXUNICASTPACKETS_GB    :%#x\n"
		"MMC_TX1024TOMAXOCTETS_GB   :%#x\n"
		"MMC_TX512TO1023OCTETS_GB   :%#x\n"
		"MMC_TX256TO511OCTETS_GB    :%#x\n"
		"MMC_TX128TO255OCTETS_GB    :%#x\n"
		"MMC_TX65TO127OCTETS_GB     :%#x\n"
		"MMC_TX64OCTETS_GB          :%#x\n"
		"MMC_TXMULTICASTPACKETS_G   :%#x\n"
		"MMC_TXBROADCASTPACKETS_G   :%#x\n"
		"MMC_TXPACKETCOUNT_GB       :%#x\n"
		"MMC_TXOCTETCOUNT_GB        :%#x\n"
		"MMC_IPC_INTR_RX            :%#x\n"
		"MMC_IPC_INTR_MASK_RX       :%#x\n"
		"MMC_INTR_MASK_TX           :%#x\n"
		"MMC_INTR_MASK_RX           :%#x\n"
		"MMC_INTR_TX                :%#x\n"
		"MMC_INTR_RX                :%#x\n"
		"MMC_CNTRL                  :%#x\n"
		"MAC_MA1LR                  :%#x\n"
		"MAC_MA1HR                  :%#x\n"
		"MAC_MA0LR                  :%#x\n"
		"MAC_MA0HR                  :%#x\n"
		"MAC_GPIOR                  :%#x\n"
		"MAC_GMIIDR                 :%#x\n"
		"MAC_GMIIAR                 :%#x\n"
		"MAC_HFR2                   :%#x\n"
		"MAC_HFR1                   :%#x\n"
		"MAC_HFR0                   :%#x\n"
		"MAC_MDR                    :%#x\n"
		"MAC_VR                     :%#x\n"
		"MAC_HTR7                   :%#x\n"
		"MAC_HTR6                   :%#x\n"
		"MAC_HTR5                   :%#x\n"
		"MAC_HTR4                   :%#x\n"
		"MAC_HTR3                   :%#x\n"
		"MAC_HTR2                   :%#x\n"
		"MAC_HTR1                   :%#x\n"
		"MAC_HTR0                   :%#x\n"
		"DMA_RIWTR7                 :%#x\n"
		"DMA_RIWTR6                 :%#x\n"
		"DMA_RIWTR5                 :%#x\n"
		"DMA_RIWTR4                 :%#x\n"
		"DMA_RIWTR3                 :%#x\n"
		"DMA_RIWTR2                 :%#x\n"
		"DMA_RIWTR1                 :%#x\n"
		"DMA_RIWTR0                 :%#x\n"
		"DMA_RDRLR7                 :%#x\n"
		"DMA_RDRLR6                 :%#x\n"
		"DMA_RDRLR5                 :%#x\n"
		"DMA_RDRLR4                 :%#x\n"
		"DMA_RDRLR3                 :%#x\n"
		"DMA_RDRLR2                 :%#x\n"
		"DMA_RDRLR1                 :%#x\n"
		"DMA_RDRLR0                 :%#x\n"
		"DMA_TDRLR7                 :%#x\n"
		"DMA_TDRLR6                 :%#x\n"
		"DMA_TDRLR5                 :%#x\n"
		"DMA_TDRLR4                 :%#x\n"
		"DMA_TDRLR3                 :%#x\n"
		"DMA_TDRLR2                 :%#x\n"
		"DMA_TDRLR1                 :%#x\n"
		"DMA_TDRLR0                 :%#x\n"
		"DMA_RDTP_RPDR7             :%#x\n"
		"DMA_RDTP_RPDR6             :%#x\n"
		"DMA_RDTP_RPDR5             :%#x\n"
		"DMA_RDTP_RPDR4             :%#x\n"
		"DMA_RDTP_RPDR3             :%#x\n"
		"DMA_RDTP_RPDR2             :%#x\n"
		"DMA_RDTP_RPDR1             :%#x\n"
		"DMA_RDTP_RPDR0             :%#x\n"
		"DMA_TDTP_TPDR7             :%#x\n"
		"DMA_TDTP_TPDR6             :%#x\n"
		"DMA_TDTP_TPDR5             :%#x\n"
		"DMA_TDTP_TPDR4             :%#x\n"
		"DMA_TDTP_TPDR3             :%#x\n"
		"DMA_TDTP_TPDR2             :%#x\n"
		"DMA_TDTP_TPDR1             :%#x\n"
		"DMA_TDTP_TPDR0             :%#x\n"
		"DMA_RDLAR7                 :%#llx\n"
		"DMA_RDLAR6                 :%#llx\n"
		"DMA_RDLAR5                 :%#llx\n"
		"DMA_RDLAR4                 :%#llx\n"
		"DMA_RDLAR3                 :%#llx\n"
		"DMA_RDLAR2                 :%#llx\n"
		"DMA_RDLAR1                 :%#llx\n"
		"DMA_RDLAR0                 :%#llx\n"
		"DMA_TDLAR7                 :%#llx\n"
		"DMA_TDLAR6                 :%#llx\n"
		"DMA_TDLAR5                 :%#llx\n"
		"DMA_TDLAR4                 :%#llx\n"
		"DMA_TDLAR3                 :%#llx\n"
		"DMA_TDLAR2                 :%#llx\n"
		"DMA_TDLAR1                 :%#llx\n"
		"DMA_TDLAR0                 :%#llx\n"
		"DMA_IER7                   :%#x\n"
		"DMA_IER6                   :%#x\n"
		"DMA_IER5                   :%#x\n"
		"DMA_IER4                   :%#x\n"
		"DMA_IER3                   :%#x\n"
		"DMA_IER2                   :%#x\n"
		"DMA_IER1                   :%#x\n"
		"DMA_IER0                   :%#x\n"
		"MAC_IMR                    :%#x\n"
		"MAC_ISR                    :%#x\n"
		"MTL_ISR                    :%#x\n"
		"DMA_SR7                    :%#x\n"
		"DMA_SR6                    :%#x\n"
		"DMA_SR5                    :%#x\n"
		"DMA_SR4                    :%#x\n"
		"DMA_SR3                    :%#x\n"
		"DMA_SR2                    :%#x\n"
		"DMA_SR1                    :%#x\n"
		"DMA_SR0                    :%#x\n"
		"DMA_ISR                    :%#x\n"
		"DMA_DSR2                   :%#x\n"
		"DMA_DSR1                   :%#x\n"
		"DMA_DSR0                   :%#x\n"
		"MTL_Q0RDR                  :%#x\n"
		"MTL_Q0ESR                  :%#x\n"
		"MTL_Q0TDR                  :%#x\n"
		"DMA_CHRBAR7                :%#x\n"
		"DMA_CHRBAR6                :%#x\n"
		"DMA_CHRBAR5                :%#x\n"
		"DMA_CHRBAR4                :%#x\n"
		"DMA_CHRBAR3                :%#x\n"
		"DMA_CHRBAR2                :%#x\n"
		"DMA_CHRBAR1                :%#x\n"
		"DMA_CHRBAR0                :%#x\n"
		"DMA_CHTBAR7                :%#x\n"
		"DMA_CHTBAR6                :%#x\n"
		"DMA_CHTBAR5                :%#x\n"
		"DMA_CHTBAR4                :%#x\n"
		"DMA_CHTBAR3                :%#x\n"
		"DMA_CHTBAR2                :%#x\n"
		"DMA_CHTBAR1                :%#x\n"
		"DMA_CHTBAR0                :%#x\n"
		"DMA_CHRDR7                 :%#x\n"
		"DMA_CHRDR6                 :%#x\n"
		"DMA_CHRDR5                 :%#x\n"
		"DMA_CHRDR4                 :%#x\n"
		"DMA_CHRDR3                 :%#x\n"
		"DMA_CHRDR2                 :%#x\n"
		"DMA_CHRDR1                 :%#x\n"
		"DMA_CHRDR0                 :%#x\n"
		"DMA_CHTDR7                 :%#x\n"
		"DMA_CHTDR6                 :%#x\n"
		"DMA_CHTDR5                 :%#x\n"
		"DMA_CHTDR4                 :%#x\n"
		"DMA_CHTDR3                 :%#x\n"
		"DMA_CHTDR2                 :%#x\n"
		"DMA_CHTDR1                 :%#x\n"
		"DMA_CHTDR0                 :%#x\n"
		"DMA_SFCSR7                 :%#x\n"
		"DMA_SFCSR6                 :%#x\n"
		"DMA_SFCSR5                 :%#x\n"
		"DMA_SFCSR4                 :%#x\n"
		"DMA_SFCSR3                 :%#x\n"
		"DMA_SFCSR2                 :%#x\n"
		"DMA_SFCSR1                 :%#x\n"
		"DMA_SFCSR0                 :%#x\n"
		"MAC_IVLANTIRR              :%#x\n"
		"MAC_VLANTIRR               :%#x\n"
		"MAC_VLANHTR                :%#x\n"
		"MAC_VLANTR                 :%#x\n"
		"DMA_SBUS                   :%#x\n"
		"DMA_BMR                    :%#x\n"
		"MTL_Q0RCR                  :%#x\n"
		"MTL_Q0OCR                  :%#x\n"
		"MTL_Q0ROMR                 :%#x\n"
		"MTL_Q0QR                   :%#x\n"
		"MTL_Q0ECR                  :%#x\n"
		"MTL_Q0UCR                  :%#x\n"
		"MTL_Q0TOMR                 :%#x\n"
		"MTL_RQDCM1R                :%#x\n"
		"MTL_RQDCM0R                :%#x\n"
		"MTL_FDDR                   :%#x\n"
		"MTL_FDACS                  :%#x\n"
		"MTL_OMR                    :%#x\n"
		"MAC_RQC3R                  :%#x\n"
		"MAC_RQC2R                  :%#x\n"
		"MAC_RQC1R                  :%#x\n"
		"MAC_RQC0R                  :%#x\n"
		"MAC_TQPM1R                 :%#x\n"
		"MAC_TQPM0R                 :%#x\n"
		"MAC_RFCR                   :%#x\n"
		"MAC_QTFCR7                 :%#x\n"
		"MAC_QTFCR6                 :%#x\n"
		"MAC_QTFCR5                 :%#x\n"
		"MAC_QTFCR4                 :%#x\n"
		"MAC_QTFCR3                 :%#x\n"
		"MAC_QTFCR2                 :%#x\n"
		"MAC_QTFCR1                 :%#x\n"
		"MAC_Q0TFCR                 :%#x\n"
		"DMA_AXI4CR7                :%#x\n"
		"DMA_AXI4CR6                :%#x\n"
		"DMA_AXI4CR5                :%#x\n"
		"DMA_AXI4CR4                :%#x\n"
		"DMA_AXI4CR3                :%#x\n"
		"DMA_AXI4CR2                :%#x\n"
		"DMA_AXI4CR1                :%#x\n"
		"DMA_AXI4CR0                :%#x\n"
		"DMA_RCR7                   :%#x\n"
		"DMA_RCR6                   :%#x\n"
		"DMA_RCR5                   :%#x\n"
		"DMA_RCR4                   :%#x\n"
		"DMA_RCR3                   :%#x\n"
		"DMA_RCR2                   :%#x\n"
		"DMA_RCR1                   :%#x\n"
		"DMA_RCR0                   :%#x\n"
		"DMA_TCR7                   :%#x\n"
		"DMA_TCR6                   :%#x\n"
		"DMA_TCR5                   :%#x\n"
		"DMA_TCR4                   :%#x\n"
		"DMA_TCR3                   :%#x\n"
		"DMA_TCR2                   :%#x\n"
		"DMA_TCR1                   :%#x\n"
		"DMA_TCR0                   :%#x\n"
		"DMA_CR7                    :%#x\n"
		"DMA_CR6                    :%#x\n"
		"DMA_CR5                    :%#x\n"
		"DMA_CR4                    :%#x\n"
		"DMA_CR3                    :%#x\n"
		"DMA_CR2                    :%#x\n"
		"DMA_CR1                    :%#x\n"
		"DMA_CR0                    :%#x\n"
		"MAC_WTR                    :%#x\n"
		"MAC_MPFR                   :%#x\n"
		"MAC_MECR                   :%#x\n"
		"MAC_MCR                    :%#x\n"
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
		MAC_MA32_127LR127_val,
		MAC_MA32_127LR126_val,
		MAC_MA32_127LR125_val,
		MAC_MA32_127LR124_val,
		MAC_MA32_127LR123_val,
		MAC_MA32_127LR122_val,
		MAC_MA32_127LR121_val,
		MAC_MA32_127LR120_val,
		MAC_MA32_127LR119_val,
		MAC_MA32_127LR118_val,
		MAC_MA32_127LR117_val,
		MAC_MA32_127LR116_val,
		MAC_MA32_127LR115_val,
		MAC_MA32_127LR114_val,
		MAC_MA32_127LR113_val,
		MAC_MA32_127LR112_val,
		MAC_MA32_127LR111_val,
		MAC_MA32_127LR110_val,
		MAC_MA32_127LR109_val,
		MAC_MA32_127LR108_val,
		MAC_MA32_127LR107_val,
		MAC_MA32_127LR106_val,
		MAC_MA32_127LR105_val,
		MAC_MA32_127LR104_val,
		MAC_MA32_127LR103_val,
		MAC_MA32_127LR102_val,
		MAC_MA32_127LR101_val,
		MAC_MA32_127LR100_val,
		MAC_MA32_127LR99_val,
		MAC_MA32_127LR98_val,
		MAC_MA32_127LR97_val,
		MAC_MA32_127LR96_val,
		MAC_MA32_127LR95_val,
		MAC_MA32_127LR94_val,
		MAC_MA32_127LR93_val,
		MAC_MA32_127LR92_val,
		MAC_MA32_127LR91_val,
		MAC_MA32_127LR90_val,
		MAC_MA32_127LR89_val,
		MAC_MA32_127LR88_val,
		MAC_MA32_127LR87_val,
		MAC_MA32_127LR86_val,
		MAC_MA32_127LR85_val,
		MAC_MA32_127LR84_val,
		MAC_MA32_127LR83_val,
		MAC_MA32_127LR82_val,
		MAC_MA32_127LR81_val,
		MAC_MA32_127LR80_val,
		MAC_MA32_127LR79_val,
		MAC_MA32_127LR78_val,
		MAC_MA32_127LR77_val,
		MAC_MA32_127LR76_val,
		MAC_MA32_127LR75_val,
		MAC_MA32_127LR74_val,
		MAC_MA32_127LR73_val,
		MAC_MA32_127LR72_val,
		MAC_MA32_127LR71_val,
		MAC_MA32_127LR70_val,
		MAC_MA32_127LR69_val,
		MAC_MA32_127LR68_val,
		MAC_MA32_127LR67_val,
		MAC_MA32_127LR66_val,
		MAC_MA32_127LR65_val,
		MAC_MA32_127LR64_val,
		MAC_MA32_127LR63_val,
		MAC_MA32_127LR62_val,
		MAC_MA32_127LR61_val,
		MAC_MA32_127LR60_val,
		MAC_MA32_127LR59_val,
		MAC_MA32_127LR58_val,
		MAC_MA32_127LR57_val,
		MAC_MA32_127LR56_val,
		MAC_MA32_127LR55_val,
		MAC_MA32_127LR54_val,
		MAC_MA32_127LR53_val,
		MAC_MA32_127LR52_val,
		MAC_MA32_127LR51_val,
		MAC_MA32_127LR50_val,
		MAC_MA32_127LR49_val,
		MAC_MA32_127LR48_val,
		MAC_MA32_127LR47_val,
		MAC_MA32_127LR46_val,
		MAC_MA32_127LR45_val,
		MAC_MA32_127LR44_val,
		MAC_MA32_127LR43_val,
		MAC_MA32_127LR42_val,
		MAC_MA32_127LR41_val,
		MAC_MA32_127LR40_val,
		MAC_MA32_127LR39_val,
		MAC_MA32_127LR38_val,
		MAC_MA32_127LR37_val,
		MAC_MA32_127LR36_val,
		MAC_MA32_127LR35_val,
		MAC_MA32_127LR34_val,
		MAC_MA32_127LR33_val,
		MAC_MA32_127LR32_val,
		MAC_MA32_127HR127_val,
		MAC_MA32_127HR126_val,
		MAC_MA32_127HR125_val,
		MAC_MA32_127HR124_val,
		MAC_MA32_127HR123_val,
		MAC_MA32_127HR122_val,
		MAC_MA32_127HR121_val,
		MAC_MA32_127HR120_val,
		MAC_MA32_127HR119_val,
		MAC_MA32_127HR118_val,
		MAC_MA32_127HR117_val,
		MAC_MA32_127HR116_val,
		MAC_MA32_127HR115_val,
		MAC_MA32_127HR114_val,
		MAC_MA32_127HR113_val,
		MAC_MA32_127HR112_val,
		MAC_MA32_127HR111_val,
		MAC_MA32_127HR110_val,
		MAC_MA32_127HR109_val,
		MAC_MA32_127HR108_val,
		MAC_MA32_127HR107_val,
		MAC_MA32_127HR106_val,
		MAC_MA32_127HR105_val,
		MAC_MA32_127HR104_val,
		MAC_MA32_127HR103_val,
		MAC_MA32_127HR102_val,
		MAC_MA32_127HR101_val,
		MAC_MA32_127HR100_val,
		MAC_MA32_127HR99_val,
		MAC_MA32_127HR98_val,
		MAC_MA32_127HR97_val,
		MAC_MA32_127HR96_val,
		MAC_MA32_127HR95_val,
		MAC_MA32_127HR94_val,
		MAC_MA32_127HR93_val,
		MAC_MA32_127HR92_val,
		MAC_MA32_127HR91_val,
		MAC_MA32_127HR90_val,
		MAC_MA32_127HR89_val,
		MAC_MA32_127HR88_val,
		MAC_MA32_127HR87_val,
		MAC_MA32_127HR86_val,
		MAC_MA32_127HR85_val,
		MAC_MA32_127HR84_val,
		MAC_MA32_127HR83_val,
		MAC_MA32_127HR82_val,
		MAC_MA32_127HR81_val,
		MAC_MA32_127HR80_val,
		MAC_MA32_127HR79_val,
		MAC_MA32_127HR78_val,
		MAC_MA32_127HR77_val,
		MAC_MA32_127HR76_val,
		MAC_MA32_127HR75_val,
		MAC_MA32_127HR74_val,
		MAC_MA32_127HR73_val,
		MAC_MA32_127HR72_val,
		MAC_MA32_127HR71_val,
		MAC_MA32_127HR70_val,
		MAC_MA32_127HR69_val,
		MAC_MA32_127HR68_val,
		MAC_MA32_127HR67_val,
		MAC_MA32_127HR66_val,
		MAC_MA32_127HR65_val,
		MAC_MA32_127HR64_val,
		MAC_MA32_127HR63_val,
		MAC_MA32_127HR62_val,
		MAC_MA32_127HR61_val,
		MAC_MA32_127HR60_val,
		MAC_MA32_127HR59_val,
		MAC_MA32_127HR58_val,
		MAC_MA32_127HR57_val,
		MAC_MA32_127HR56_val,
		MAC_MA32_127HR55_val,
		MAC_MA32_127HR54_val,
		MAC_MA32_127HR53_val,
		MAC_MA32_127HR52_val,
		MAC_MA32_127HR51_val,
		MAC_MA32_127HR50_val,
		MAC_MA32_127HR49_val,
		MAC_MA32_127HR48_val,
		MAC_MA32_127HR47_val,
		MAC_MA32_127HR46_val,
		MAC_MA32_127HR45_val,
		MAC_MA32_127HR44_val,
		MAC_MA32_127HR43_val,
		MAC_MA32_127HR42_val,
		MAC_MA32_127HR41_val,
		MAC_MA32_127HR40_val,
		MAC_MA32_127HR39_val,
		MAC_MA32_127HR38_val,
		MAC_MA32_127HR37_val,
		MAC_MA32_127HR36_val,
		MAC_MA32_127HR35_val,
		MAC_MA32_127HR34_val,
		MAC_MA32_127HR33_val,
		MAC_MA32_127HR32_val,
		MAC_MA1_31LR31_val,
		MAC_MA1_31LR30_val,
		MAC_MA1_31LR29_val,
		MAC_MA1_31LR28_val,
		MAC_MA1_31LR27_val,
		MAC_MA1_31LR26_val,
		MAC_MA1_31LR25_val,
		MAC_MA1_31LR24_val,
		MAC_MA1_31LR23_val,
		MAC_MA1_31LR22_val,
		MAC_MA1_31LR21_val,
		MAC_MA1_31LR20_val,
		MAC_MA1_31LR19_val,
		MAC_MA1_31LR18_val,
		MAC_MA1_31LR17_val,
		MAC_MA1_31LR16_val,
		MAC_MA1_31LR15_val,
		MAC_MA1_31LR14_val,
		MAC_MA1_31LR13_val,
		MAC_MA1_31LR12_val,
		MAC_MA1_31LR11_val,
		MAC_MA1_31LR10_val,
		MAC_MA1_31LR9_val,
		MAC_MA1_31LR8_val,
		MAC_MA1_31LR7_val,
		MAC_MA1_31LR6_val,
		MAC_MA1_31LR5_val,
		MAC_MA1_31LR4_val,
		MAC_MA1_31LR3_val,
		MAC_MA1_31LR2_val,
		MAC_MA1_31LR1_val,
		MAC_MA1_31HR31_val,
		MAC_MA1_31HR30_val,
		MAC_MA1_31HR29_val,
		MAC_MA1_31HR28_val,
		MAC_MA1_31HR27_val,
		MAC_MA1_31HR26_val,
		MAC_MA1_31HR25_val,
		MAC_MA1_31HR24_val,
		MAC_MA1_31HR23_val,
		MAC_MA1_31HR22_val,
		MAC_MA1_31HR21_val,
		MAC_MA1_31HR20_val,
		MAC_MA1_31HR19_val,
		MAC_MA1_31HR18_val,
		MAC_MA1_31HR17_val,
		MAC_MA1_31HR16_val,
		MAC_MA1_31HR15_val,
		MAC_MA1_31HR14_val,
		MAC_MA1_31HR13_val,
		MAC_MA1_31HR12_val,
		MAC_MA1_31HR11_val,
		MAC_MA1_31HR10_val,
		MAC_MA1_31HR9_val,
		MAC_MA1_31HR8_val,
		MAC_MA1_31HR7_val,
		MAC_MA1_31HR6_val,
		MAC_MA1_31HR5_val,
		MAC_MA1_31HR4_val,
		MAC_MA1_31HR3_val,
		MAC_MA1_31HR2_val,
		MAC_MA1_31HR1_val,
		mac_arpa_val,
		MAC_L3A3R7_val,
		MAC_L3A3R6_val,
		MAC_L3A3R5_val,
		MAC_L3A3R4_val,
		MAC_L3A3R3_val,
		MAC_L3A3R2_val,
		MAC_L3A3R1_val,
		MAC_L3A3R0_val,
		MAC_L3A2R7_val,
		MAC_L3A2R6_val,
		MAC_L3A2R5_val,
		MAC_L3A2R4_val,
		MAC_L3A2R3_val,
		MAC_L3A2R2_val,
		MAC_L3A2R1_val,
		MAC_L3A2R0_val,
		MAC_L3A1R7_val,
		MAC_L3A1R6_val,
		MAC_L3A1R5_val,
		MAC_L3A1R4_val,
		MAC_L3A1R3_val,
		MAC_L3A1R2_val,
		MAC_L3A1R1_val,
		MAC_L3A1R0_val,
		MAC_L3A0R7_val,
		MAC_L3A0R6_val,
		MAC_L3A0R5_val,
		MAC_L3A0R4_val,
		MAC_L3A0R3_val,
		MAC_L3A0R2_val,
		MAC_L3A0R1_val,
		MAC_L3A0R0_val,
		MAC_L4AR7_val,
		MAC_L4AR6_val,
		MAC_L4AR5_val,
		MAC_L4AR4_val,
		MAC_L4AR3_val,
		MAC_L4AR2_val,
		MAC_L4AR1_val,
		MAC_L4AR0_val,
		MAC_L3L4CR7_val,
		MAC_L3L4CR6_val,
		MAC_L3L4CR5_val,
		MAC_L3L4CR4_val,
		MAC_L3L4CR3_val,
		MAC_L3L4CR2_val,
		MAC_L3L4CR1_val,
		MAC_L3L4CR0_val,
		mac_gpios_val,
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
		MAC_SPI2r_val,
		MAC_SPI1r_val,
		MAC_SPI0r_val,
		mac_pto_cr_val,
		MAC_PPS_WIDTH3_val,
		MAC_PPS_WIDTH2_val,
		MAC_PPS_WIDTH1_val,
		MAC_PPS_WIDTH0_val,
		MAC_PPS_INTVAL3_val,
		MAC_PPS_INTVAL2_val,
		MAC_PPS_INTVAL1_val,
		MAC_PPS_INTVAL0_val,
		MAC_PPS_TTNS3_val,
		MAC_PPS_TTNS2_val,
		MAC_PPS_TTNS1_val,
		MAC_PPS_TTNS0_val,
		MAC_PPS_TTS3_val,
		MAC_PPS_TTS2_val,
		MAC_PPS_TTS1_val,
		MAC_PPS_TTS0_val,
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
		MTL_QRCR7_val,
		MTL_QRCR6_val,
		MTL_QRCR5_val,
		MTL_QRCR4_val,
		MTL_QRCR3_val,
		MTL_QRCR2_val,
		MTL_QRCR1_val,
		MTL_QRDR7_val,
		MTL_QRDR6_val,
		MTL_QRDR5_val,
		MTL_QRDR4_val,
		MTL_QRDR3_val,
		MTL_QRDR2_val,
		MTL_QRDR1_val,
		MTL_QOCR7_val,
		MTL_QOCR6_val,
		MTL_QOCR5_val,
		MTL_QOCR4_val,
		MTL_QOCR3_val,
		MTL_QOCR2_val,
		MTL_QOCR1_val,
		MTL_QROMR7_val,
		MTL_QROMR6_val,
		MTL_QROMR5_val,
		MTL_QROMR4_val,
		MTL_QROMR3_val,
		MTL_QROMR2_val,
		MTL_QROMR1_val,
		MTL_QLCR7_val,
		MTL_QLCR6_val,
		MTL_QLCR5_val,
		MTL_QLCR4_val,
		MTL_QLCR3_val,
		MTL_QLCR2_val,
		MTL_QLCR1_val,
		MTL_QHCR7_val,
		MTL_QHCR6_val,
		MTL_QHCR5_val,
		MTL_QHCR4_val,
		MTL_QHCR3_val,
		MTL_QHCR2_val,
		MTL_QHCR1_val,
		MTL_QSSCR7_val,
		MTL_QSSCR6_val,
		MTL_QSSCR5_val,
		MTL_QSSCR4_val,
		MTL_QSSCR3_val,
		MTL_QSSCR2_val,
		MTL_QSSCR1_val,
		MTL_QW7_val,
		MTL_QW6_val,
		MTL_QW5_val,
		MTL_QW4_val,
		MTL_QW3_val,
		MTL_QW2_val,
		MTL_QW1_val,
		MTL_QESR7_val,
		MTL_QESR6_val,
		MTL_QESR5_val,
		MTL_QESR4_val,
		MTL_QESR3_val,
		MTL_QESR2_val,
		MTL_QESR1_val,
		MTL_QECR7_val,
		MTL_QECR6_val,
		MTL_QECR5_val,
		MTL_QECR4_val,
		MTL_QECR3_val,
		MTL_QECR2_val,
		MTL_QECR1_val,
		MTL_QTDR7_val,
		MTL_QTDR6_val,
		MTL_QTDR5_val,
		MTL_QTDR4_val,
		MTL_QTDR3_val,
		MTL_QTDR2_val,
		MTL_QTDR1_val,
		MTL_QUCR7_val,
		MTL_QUCR6_val,
		MTL_QUCR5_val,
		MTL_QUCR4_val,
		MTL_QUCR3_val,
		MTL_QUCR2_val,
		MTL_QUCR1_val,
		MTL_QTOMR7_val,
		MTL_QTOMR6_val,
		MTL_QTOMR5_val,
		MTL_QTOMR4_val,
		MTL_QTOMR3_val,
		MTL_QTOMR2_val,
		MTL_QTOMR1_val,
		mac_pmtcsr_val,
		mmc_rxicmp_err_octets_val,
		mmc_rxicmp_gd_octets_val,
		mmc_rxtcp_err_octets_val,
		mmc_rxtcp_gd_octets_val,
		mmc_rxudp_err_octets_val,
		mmc_rxudp_gd_octets_val,
		MMC_RXIPV6_nopay_octets_val,
		MMC_RXIPV6_hdrerr_octets_val,
		MMC_RXIPV6_gd_octets_val,
		MMC_RXIPV4_udsbl_octets_val,
		MMC_RXIPV4_frag_octets_val,
		MMC_RXIPV4_nopay_octets_val,
		MMC_RXIPV4_hdrerr_octets_val,
		MMC_RXIPV4_gd_octets_val,
		mmc_rxicmp_err_pkts_val,
		mmc_rxicmp_gd_pkts_val,
		mmc_rxtcp_err_pkts_val,
		mmc_rxtcp_gd_pkts_val,
		mmc_rxudp_err_pkts_val,
		mmc_rxudp_gd_pkts_val,
		MMC_RXIPV6_nopay_pkts_val,
		MMC_RXIPV6_hdrerr_pkts_val,
		MMC_RXIPV6_gd_pkts_val,
		MMC_RXIPV4_ubsbl_pkts_val,
		MMC_RXIPV4_frag_pkts_val,
		MMC_RXIPV4_nopay_pkts_val,
		MMC_RXIPV4_hdrerr_pkts_val,
		MMC_RXIPV4_gd_pkts_val,
		mmc_rxctrlpackets_g_val,
		mmc_rxrcverror_val,
		mmc_rxwatchdogerror_val,
		mmc_rxvlanpackets_gb_val,
		mmc_rxfifooverflow_val,
		mmc_rxpausepackets_val,
		mmc_rxoutofrangetype_val,
		mmc_rxlengtherror_val,
		mmc_rxunicastpackets_g_val,
		MMC_RX1024tomaxoctets_gb_val,
		MMC_RX512TO1023octets_gb_val,
		MMC_RX256TO511octets_gb_val,
		MMC_RX128TO255octets_gb_val,
		MMC_RX65TO127octets_gb_val,
		MMC_RX64octets_gb_val,
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
		MMC_TX1024tomaxoctets_gb_val,
		MMC_TX512TO1023octets_gb_val,
		MMC_TX256TO511octets_gb_val,
		MMC_TX128TO255octets_gb_val,
		MMC_TX65TO127octets_gb_val,
		MMC_TX64octets_gb_val,
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
		MAC_MA1lr_val,
		MAC_MA1hr_val,
		MAC_MA0lr_val,
		MAC_MA0hr_val,
		mac_gpior_val,
		mac_gmiidr_val,
		mac_gmiiar_val,
		MAC_HFR2_val,
		MAC_HFR1_val,
		MAC_HFR0_val,
		mac_mdr_val,
		mac_vr_val,
		MAC_HTR7_val,
		MAC_HTR6_val,
		MAC_HTR5_val,
		MAC_HTR4_val,
		MAC_HTR3_val,
		MAC_HTR2_val,
		MAC_HTR1_val,
		MAC_HTR0_val,
		DMA_RIWTR7_val,
		DMA_RIWTR6_val,
		DMA_RIWTR5_val,
		DMA_RIWTR4_val,
		DMA_RIWTR3_val,
		DMA_RIWTR2_val,
		DMA_RIWTR1_val,
		DMA_RIWTR0_val,
		DMA_RDRLR7_val,
		DMA_RDRLR6_val,
		DMA_RDRLR5_val,
		DMA_RDRLR4_val,
		DMA_RDRLR3_val,
		DMA_RDRLR2_val,
		DMA_RDRLR1_val,
		DMA_RDRLR0_val,
		DMA_TDRLR7_val,
		DMA_TDRLR6_val,
		DMA_TDRLR5_val,
		DMA_TDRLR4_val,
		DMA_TDRLR3_val,
		DMA_TDRLR2_val,
		DMA_TDRLR1_val,
		DMA_TDRLR0_val,
		DMA_RDTP_RPDR7_val,
		DMA_RDTP_RPDR6_val,
		DMA_RDTP_RPDR5_val,
		DMA_RDTP_RPDR4_val,
		DMA_RDTP_RPDR3_val,
		DMA_RDTP_RPDR2_val,
		DMA_RDTP_RPDR1_val,
		DMA_RDTP_RPDR0_val,
		DMA_TDTP_TPDR7_val,
		DMA_TDTP_TPDR6_val,
		DMA_TDTP_TPDR5_val,
		DMA_TDTP_TPDR4_val,
		DMA_TDTP_TPDR3_val,
		DMA_TDTP_TPDR2_val,
		DMA_TDTP_TPDR1_val,
		DMA_TDTP_TPDR0_val,
		DMA_RDLAR7_val,
		DMA_RDLAR6_val,
		DMA_RDLAR5_val,
		DMA_RDLAR4_val,
		DMA_RDLAR3_val,
		DMA_RDLAR2_val,
		DMA_RDLAR1_val,
		DMA_RDLAR0_val,
		DMA_TDLAR7_val,
		DMA_TDLAR6_val,
		DMA_TDLAR5_val,
		DMA_TDLAR4_val,
		DMA_TDLAR3_val,
		DMA_TDLAR2_val,
		DMA_TDLAR1_val,
		DMA_TDLAR0_val,
		DMA_IER7_val,
		DMA_IER6_val,
		DMA_IER5_val,
		DMA_IER4_val,
		DMA_IER3_val,
		DMA_IER2_val,
		DMA_IER1_val,
		DMA_IER0_val,
		mac_imr_val,
		mac_isr_val,
		mtl_isr_val,
		DMA_SR7_val,
		DMA_SR6_val,
		DMA_SR5_val,
		DMA_SR4_val,
		DMA_SR3_val,
		DMA_SR2_val,
		DMA_SR1_val,
		DMA_SR0_val,
		dma_isr_val,
		DMA_DSR2_val,
		DMA_DSR1_val,
		DMA_DSR0_val,
		MTL_Q0rdr_val,
		MTL_Q0esr_val,
		MTL_Q0tdr_val,
		DMA_CHRBAR7_val,
		DMA_CHRBAR6_val,
		DMA_CHRBAR5_val,
		DMA_CHRBAR4_val,
		DMA_CHRBAR3_val,
		DMA_CHRBAR2_val,
		DMA_CHRBAR1_val,
		DMA_CHRBAR0_val,
		DMA_CHTBAR7_val,
		DMA_CHTBAR6_val,
		DMA_CHTBAR5_val,
		DMA_CHTBAR4_val,
		DMA_CHTBAR3_val,
		DMA_CHTBAR2_val,
		DMA_CHTBAR1_val,
		DMA_CHTBAR0_val,
		DMA_CHRDR7_val,
		DMA_CHRDR6_val,
		DMA_CHRDR5_val,
		DMA_CHRDR4_val,
		DMA_CHRDR3_val,
		DMA_CHRDR2_val,
		DMA_CHRDR1_val,
		DMA_CHRDR0_val,
		DMA_CHTDR7_val,
		DMA_CHTDR6_val,
		DMA_CHTDR5_val,
		DMA_CHTDR4_val,
		DMA_CHTDR3_val,
		DMA_CHTDR2_val,
		DMA_CHTDR1_val,
		DMA_CHTDR0_val,
		DMA_SFCSR7_val,
		DMA_SFCSR6_val,
		DMA_SFCSR5_val,
		DMA_SFCSR4_val,
		DMA_SFCSR3_val,
		DMA_SFCSR2_val,
		DMA_SFCSR1_val,
		DMA_SFCSR0_val,
		mac_ivlantirr_val,
		mac_vlantirr_val,
		mac_vlanhtr_val,
		mac_vlantr_val,
		dma_sbus_val,
		dma_bmr_val,
		MTL_Q0rcr_val,
		MTL_Q0ocr_val,
		MTL_Q0romr_val,
		MTL_Q0qr_val,
		MTL_Q0ecr_val,
		MTL_Q0ucr_val,
		MTL_Q0tomr_val,
		MTL_RQDCM1r_val,
		MTL_RQDCM0r_val,
		mtl_fddr_val,
		mtl_fdacs_val,
		mtl_omr_val,
		MAC_RQC3r_val,
		MAC_RQC2r_val,
		MAC_RQC1r_val,
		MAC_RQC0r_val,
		MAC_TQPM1r_val,
		MAC_TQPM0r_val,
		mac_rfcr_val,
		MAC_QTFCR7_val,
		MAC_QTFCR6_val,
		MAC_QTFCR5_val,
		MAC_QTFCR4_val,
		MAC_QTFCR3_val,
		MAC_QTFCR2_val,
		MAC_QTFCR1_val,
		MAC_Q0tfcr_val,
		DMA_AXI4CR7_val,
		DMA_AXI4CR6_val,
		DMA_AXI4CR5_val,
		DMA_AXI4CR4_val,
		DMA_AXI4CR3_val,
		DMA_AXI4CR2_val,
		DMA_AXI4CR1_val,
		DMA_AXI4CR0_val,
		DMA_RCR7_val,
		DMA_RCR6_val,
		DMA_RCR5_val,
		DMA_RCR4_val,
		DMA_RCR3_val,
		DMA_RCR2_val,
		DMA_RCR1_val,
		DMA_RCR0_val,
		DMA_TCR7_val,
		DMA_TCR6_val,
		DMA_TCR5_val,
		DMA_TCR4_val,
		DMA_TCR3_val,
		DMA_TCR2_val,
		DMA_TCR1_val,
		DMA_TCR0_val,
		DMA_CR7_val,
		DMA_CR6_val,
		DMA_CR5_val,
		DMA_CR4_val,
		DMA_CR3_val,
		DMA_CR2_val,
		DMA_CR1_val,
		DMA_CR0_val,
		mac_wtr_val,
		mac_mpfr_val,
		mac_mecr_val,
		mac_mcr_val,
		mii_bmcr_reg_val,
		mii_bmsr_reg_val,
		MII_PHYSID1_reg_val,
		MII_PHYSID2_reg_val,
		mii_advertise_reg_val,
		mii_lpa_reg_val,
		mii_expansion_reg_val,
		auto_nego_np_reg_val,
		mii_estatus_reg_val,
		MII_CTRL1000_reg_val,
		MII_STAT1000_reg_val,
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

static ssize_t descriptor_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	DBGPR("--> registers_write\n");
	pr_info(
	       "write operation not supported for descrptors: write error\n");
	DBGPR("<-- registers_write\n");

	return -1;
}

static ssize_t MAC_MA32_127LR127_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(127, MAC_MA32_127LR127_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR127            :%#x\n",
		MAC_MA32_127LR127_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR127_fops = {
	.read = MAC_MA32_127LR127_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR126_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(126, MAC_MA32_127LR126_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR126            :%#x\n",
		MAC_MA32_127LR126_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR126_fops = {
	.read = MAC_MA32_127LR126_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR125_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(125, MAC_MA32_127LR125_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR125            :%#x\n",
		MAC_MA32_127LR125_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR125_fops = {
	.read = MAC_MA32_127LR125_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR124_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(124, MAC_MA32_127LR124_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR124            :%#x\n",
		MAC_MA32_127LR124_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR124_fops = {
	.read = MAC_MA32_127LR124_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR123_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(123, MAC_MA32_127LR123_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR123            :%#x\n",
		MAC_MA32_127LR123_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR123_fops = {
	.read = MAC_MA32_127LR123_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR122_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(122, MAC_MA32_127LR122_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR122            :%#x\n",
		MAC_MA32_127LR122_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR122_fops = {
	.read = MAC_MA32_127LR122_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR121_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(121, MAC_MA32_127LR121_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR121            :%#x\n",
		MAC_MA32_127LR121_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR121_fops = {
	.read = MAC_MA32_127LR121_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR120_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(120, MAC_MA32_127LR120_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR120            :%#x\n",
		MAC_MA32_127LR120_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR120_fops = {
	.read = MAC_MA32_127LR120_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR119_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(119, MAC_MA32_127LR119_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR119            :%#x\n",
		MAC_MA32_127LR119_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR119_fops = {
	.read = MAC_MA32_127LR119_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR118_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(118, MAC_MA32_127LR118_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR118            :%#x\n",
		MAC_MA32_127LR118_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR118_fops = {
	.read = MAC_MA32_127LR118_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR117_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(117, MAC_MA32_127LR117_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR117            :%#x\n",
		MAC_MA32_127LR117_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR117_fops = {
	.read = MAC_MA32_127LR117_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR116_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(116, MAC_MA32_127LR116_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR116            :%#x\n",
		MAC_MA32_127LR116_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR116_fops = {
	.read = MAC_MA32_127LR116_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR115_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(115, MAC_MA32_127LR115_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR115            :%#x\n",
		MAC_MA32_127LR115_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR115_fops = {
	.read = MAC_MA32_127LR115_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR114_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(114, MAC_MA32_127LR114_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR114            :%#x\n",
		MAC_MA32_127LR114_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR114_fops = {
	.read = MAC_MA32_127LR114_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR113_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(113, MAC_MA32_127LR113_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR113            :%#x\n",
		MAC_MA32_127LR113_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR113_fops = {
	.read = MAC_MA32_127LR113_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR112_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(112, MAC_MA32_127LR112_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR112            :%#x\n",
		MAC_MA32_127LR112_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR112_fops = {
	.read = MAC_MA32_127LR112_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR111_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(111, MAC_MA32_127LR111_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR111            :%#x\n",
		MAC_MA32_127LR111_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR111_fops = {
	.read = MAC_MA32_127LR111_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR110_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(110, MAC_MA32_127LR110_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR110            :%#x\n",
		MAC_MA32_127LR110_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR110_fops = {
	.read = MAC_MA32_127LR110_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR109_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(109, MAC_MA32_127LR109_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR109            :%#x\n",
		MAC_MA32_127LR109_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR109_fops = {
	.read = MAC_MA32_127LR109_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR108_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(108, MAC_MA32_127LR108_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR108            :%#x\n",
		MAC_MA32_127LR108_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR108_fops = {
	.read = MAC_MA32_127LR108_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR107_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(107, MAC_MA32_127LR107_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR107            :%#x\n",
		MAC_MA32_127LR107_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR107_fops = {
	.read = MAC_MA32_127LR107_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR106_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(106, MAC_MA32_127LR106_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR106            :%#x\n",
		MAC_MA32_127LR106_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR106_fops = {
	.read = MAC_MA32_127LR106_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR105_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(105, MAC_MA32_127LR105_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR105            :%#x\n",
		MAC_MA32_127LR105_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR105_fops = {
	.read = MAC_MA32_127LR105_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR104_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(104, MAC_MA32_127LR104_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR104            :%#x\n",
		MAC_MA32_127LR104_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR104_fops = {
	.read = MAC_MA32_127LR104_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR103_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(103, MAC_MA32_127LR103_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR103            :%#x\n",
		MAC_MA32_127LR103_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR103_fops = {
	.read = MAC_MA32_127LR103_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR102_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(102, MAC_MA32_127LR102_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR102            :%#x\n",
		MAC_MA32_127LR102_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR102_fops = {
	.read = MAC_MA32_127LR102_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR101_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(101, MAC_MA32_127LR101_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR101            :%#x\n",
		MAC_MA32_127LR101_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR101_fops = {
	.read = MAC_MA32_127LR101_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR100_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(100, MAC_MA32_127LR100_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR100            :%#x\n",
		MAC_MA32_127LR100_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR100_fops = {
	.read = MAC_MA32_127LR100_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR99_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(99, MAC_MA32_127LR99_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR99            :%#x\n",
		MAC_MA32_127LR99_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR99_fops = {
	.read = MAC_MA32_127LR99_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR98_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(98, MAC_MA32_127LR98_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR98            :%#x\n",
		MAC_MA32_127LR98_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR98_fops = {
	.read = MAC_MA32_127LR98_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR97_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(97, MAC_MA32_127LR97_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR97            :%#x\n",
		MAC_MA32_127LR97_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR97_fops = {
	.read = MAC_MA32_127LR97_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR96_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(96, MAC_MA32_127LR96_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR96            :%#x\n",
		MAC_MA32_127LR96_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR96_fops = {
	.read = MAC_MA32_127LR96_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR95_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(95, MAC_MA32_127LR95_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR95            :%#x\n",
		MAC_MA32_127LR95_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR95_fops = {
	.read = MAC_MA32_127LR95_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR94_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(94, MAC_MA32_127LR94_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR94            :%#x\n",
		MAC_MA32_127LR94_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR94_fops = {
	.read = MAC_MA32_127LR94_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR93_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(93, MAC_MA32_127LR93_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR93            :%#x\n",
		MAC_MA32_127LR93_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR93_fops = {
	.read = MAC_MA32_127LR93_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR92_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(92, MAC_MA32_127LR92_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR92            :%#x\n",
		MAC_MA32_127LR92_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR92_fops = {
	.read = MAC_MA32_127LR92_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR91_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(91, MAC_MA32_127LR91_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR91            :%#x\n",
		MAC_MA32_127LR91_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR91_fops = {
	.read = MAC_MA32_127LR91_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR90_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(90, MAC_MA32_127LR90_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR90            :%#x\n",
		MAC_MA32_127LR90_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR90_fops = {
	.read = MAC_MA32_127LR90_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR89_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(89, MAC_MA32_127LR89_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR89            :%#x\n",
		MAC_MA32_127LR89_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR89_fops = {
	.read = MAC_MA32_127LR89_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR88_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(88, MAC_MA32_127LR88_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR88            :%#x\n",
		MAC_MA32_127LR88_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR88_fops = {
	.read = MAC_MA32_127LR88_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR87_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(87, MAC_MA32_127LR87_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR87            :%#x\n",
		MAC_MA32_127LR87_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR87_fops = {
	.read = MAC_MA32_127LR87_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR86_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(86, MAC_MA32_127LR86_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR86            :%#x\n",
		MAC_MA32_127LR86_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR86_fops = {
	.read = MAC_MA32_127LR86_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR85_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(85, MAC_MA32_127LR85_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR85            :%#x\n",
		MAC_MA32_127LR85_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR85_fops = {
	.read = MAC_MA32_127LR85_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR84_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(84, MAC_MA32_127LR84_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR84            :%#x\n",
		MAC_MA32_127LR84_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR84_fops = {
	.read = MAC_MA32_127LR84_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR83_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(83, MAC_MA32_127LR83_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR83            :%#x\n",
		MAC_MA32_127LR83_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR83_fops = {
	.read = MAC_MA32_127LR83_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR82_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(82, MAC_MA32_127LR82_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR82            :%#x\n",
		MAC_MA32_127LR82_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR82_fops = {
	.read = MAC_MA32_127LR82_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR81_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(81, MAC_MA32_127LR81_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR81            :%#x\n",
		MAC_MA32_127LR81_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR81_fops = {
	.read = MAC_MA32_127LR81_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR80_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(80, MAC_MA32_127LR80_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR80            :%#x\n",
		MAC_MA32_127LR80_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR80_fops = {
	.read = MAC_MA32_127LR80_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR79_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(79, MAC_MA32_127LR79_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR79            :%#x\n",
		MAC_MA32_127LR79_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR79_fops = {
	.read = MAC_MA32_127LR79_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR78_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(78, MAC_MA32_127LR78_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR78            :%#x\n",
		MAC_MA32_127LR78_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR78_fops = {
	.read = MAC_MA32_127LR78_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR77_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(77, MAC_MA32_127LR77_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR77            :%#x\n",
		MAC_MA32_127LR77_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR77_fops = {
	.read = MAC_MA32_127LR77_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR76_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(76, MAC_MA32_127LR76_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR76            :%#x\n",
		MAC_MA32_127LR76_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR76_fops = {
	.read = MAC_MA32_127LR76_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR75_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(75, MAC_MA32_127LR75_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR75            :%#x\n",
		MAC_MA32_127LR75_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR75_fops = {
	.read = MAC_MA32_127LR75_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR74_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(74, MAC_MA32_127LR74_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR74            :%#x\n",
		MAC_MA32_127LR74_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR74_fops = {
	.read = MAC_MA32_127LR74_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR73_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(73, MAC_MA32_127LR73_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR73            :%#x\n",
		MAC_MA32_127LR73_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR73_fops = {
	.read = MAC_MA32_127LR73_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR72_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(72, MAC_MA32_127LR72_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR72            :%#x\n",
		MAC_MA32_127LR72_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR72_fops = {
	.read = MAC_MA32_127LR72_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR71_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(71, MAC_MA32_127LR71_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR71            :%#x\n",
		MAC_MA32_127LR71_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR71_fops = {
	.read = MAC_MA32_127LR71_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR70_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(70, MAC_MA32_127LR70_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR70            :%#x\n",
		MAC_MA32_127LR70_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR70_fops = {
	.read = MAC_MA32_127LR70_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR69_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(69, MAC_MA32_127LR69_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR69            :%#x\n",
		MAC_MA32_127LR69_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR69_fops = {
	.read = MAC_MA32_127LR69_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR68_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(68, MAC_MA32_127LR68_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR68            :%#x\n",
		MAC_MA32_127LR68_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR68_fops = {
	.read = MAC_MA32_127LR68_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR67_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(67, MAC_MA32_127LR67_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR67            :%#x\n",
		MAC_MA32_127LR67_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR67_fops = {
	.read = MAC_MA32_127LR67_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR66_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(66, MAC_MA32_127LR66_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR66            :%#x\n",
		MAC_MA32_127LR66_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR66_fops = {
	.read = MAC_MA32_127LR66_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR65_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(65, MAC_MA32_127LR65_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR65            :%#x\n",
		MAC_MA32_127LR65_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR65_fops = {
	.read = MAC_MA32_127LR65_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR64_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(64, MAC_MA32_127LR64_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR64            :%#x\n",
		MAC_MA32_127LR64_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR64_fops = {
	.read = MAC_MA32_127LR64_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR63_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(63, MAC_MA32_127LR63_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR63            :%#x\n",
		MAC_MA32_127LR63_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR63_fops = {
	.read = MAC_MA32_127LR63_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR62_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(62, MAC_MA32_127LR62_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR62            :%#x\n",
		MAC_MA32_127LR62_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR62_fops = {
	.read = MAC_MA32_127LR62_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR61_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(61, MAC_MA32_127LR61_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR61            :%#x\n",
		MAC_MA32_127LR61_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR61_fops = {
	.read = MAC_MA32_127LR61_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR60_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(60, MAC_MA32_127LR60_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR60            :%#x\n",
		MAC_MA32_127LR60_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR60_fops = {
	.read = MAC_MA32_127LR60_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR59_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(59, MAC_MA32_127LR59_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR59            :%#x\n",
		MAC_MA32_127LR59_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR59_fops = {
	.read = MAC_MA32_127LR59_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR58_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(58, MAC_MA32_127LR58_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR58            :%#x\n",
		MAC_MA32_127LR58_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR58_fops = {
	.read = MAC_MA32_127LR58_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR57_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(57, MAC_MA32_127LR57_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR57            :%#x\n",
		MAC_MA32_127LR57_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR57_fops = {
	.read = MAC_MA32_127LR57_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR56_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(56, MAC_MA32_127LR56_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR56            :%#x\n",
		MAC_MA32_127LR56_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR56_fops = {
	.read = MAC_MA32_127LR56_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR55_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(55, MAC_MA32_127LR55_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR55            :%#x\n",
		MAC_MA32_127LR55_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR55_fops = {
	.read = MAC_MA32_127LR55_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR54_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(54, MAC_MA32_127LR54_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR54            :%#x\n",
		MAC_MA32_127LR54_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR54_fops = {
	.read = MAC_MA32_127LR54_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR53_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(53, MAC_MA32_127LR53_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR53            :%#x\n",
		MAC_MA32_127LR53_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR53_fops = {
	.read = MAC_MA32_127LR53_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR52_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(52, MAC_MA32_127LR52_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR52            :%#x\n",
		MAC_MA32_127LR52_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR52_fops = {
	.read = MAC_MA32_127LR52_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR51_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(51, MAC_MA32_127LR51_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR51            :%#x\n",
		MAC_MA32_127LR51_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR51_fops = {
	.read = MAC_MA32_127LR51_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR50_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(50, MAC_MA32_127LR50_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR50            :%#x\n",
		MAC_MA32_127LR50_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR50_fops = {
	.read = MAC_MA32_127LR50_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR49_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(49, MAC_MA32_127LR49_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR49            :%#x\n",
		MAC_MA32_127LR49_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR49_fops = {
	.read = MAC_MA32_127LR49_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR48_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(48, MAC_MA32_127LR48_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR48            :%#x\n",
		MAC_MA32_127LR48_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR48_fops = {
	.read = MAC_MA32_127LR48_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR47_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(47, MAC_MA32_127LR47_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR47            :%#x\n",
		MAC_MA32_127LR47_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR47_fops = {
	.read = MAC_MA32_127LR47_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR46_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(46, MAC_MA32_127LR46_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR46            :%#x\n",
		MAC_MA32_127LR46_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR46_fops = {
	.read = MAC_MA32_127LR46_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR45_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(45, MAC_MA32_127LR45_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR45            :%#x\n",
		MAC_MA32_127LR45_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR45_fops = {
	.read = MAC_MA32_127LR45_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR44_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(44, MAC_MA32_127LR44_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR44            :%#x\n",
		MAC_MA32_127LR44_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR44_fops = {
	.read = MAC_MA32_127LR44_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR43_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(43, MAC_MA32_127LR43_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR43            :%#x\n",
		MAC_MA32_127LR43_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR43_fops = {
	.read = MAC_MA32_127LR43_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR42_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(42, MAC_MA32_127LR42_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR42            :%#x\n",
		MAC_MA32_127LR42_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR42_fops = {
	.read = MAC_MA32_127LR42_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR41_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(41, MAC_MA32_127LR41_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR41            :%#x\n",
		MAC_MA32_127LR41_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR41_fops = {
	.read = MAC_MA32_127LR41_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR40_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(40, MAC_MA32_127LR40_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR40            :%#x\n",
		MAC_MA32_127LR40_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR40_fops = {
	.read = MAC_MA32_127LR40_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR39_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(39, MAC_MA32_127LR39_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR39            :%#x\n",
		MAC_MA32_127LR39_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR39_fops = {
	.read = MAC_MA32_127LR39_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR38_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(38, MAC_MA32_127LR38_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR38            :%#x\n",
		MAC_MA32_127LR38_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR38_fops = {
	.read = MAC_MA32_127LR38_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR37_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(37, MAC_MA32_127LR37_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR37            :%#x\n",
		MAC_MA32_127LR37_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR37_fops = {
	.read = MAC_MA32_127LR37_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR36_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(36, MAC_MA32_127LR36_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR36            :%#x\n",
		MAC_MA32_127LR36_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR36_fops = {
	.read = MAC_MA32_127LR36_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR35_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(35, MAC_MA32_127LR35_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR35            :%#x\n",
		MAC_MA32_127LR35_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR35_fops = {
	.read = MAC_MA32_127LR35_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR34_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(34, MAC_MA32_127LR34_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR34            :%#x\n",
		MAC_MA32_127LR34_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR34_fops = {
	.read = MAC_MA32_127LR34_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR33_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(33, MAC_MA32_127LR33_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR33            :%#x\n",
		MAC_MA32_127LR33_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR33_fops = {
	.read = MAC_MA32_127LR33_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127LR32_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127LR_RD(32, MAC_MA32_127LR32_val);
	sprintf(debugfs_buf, "MAC_MA32_127LR32            :%#x\n",
		MAC_MA32_127LR32_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127LR32_fops = {
	.read = MAC_MA32_127LR32_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR127_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(127, MAC_MA32_127HR127_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR127            :%#x\n",
		MAC_MA32_127HR127_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR127_fops = {
	.read = MAC_MA32_127HR127_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR126_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(126, MAC_MA32_127HR126_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR126            :%#x\n",
		MAC_MA32_127HR126_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR126_fops = {
	.read = MAC_MA32_127HR126_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR125_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(125, MAC_MA32_127HR125_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR125            :%#x\n",
		MAC_MA32_127HR125_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR125_fops = {
	.read = MAC_MA32_127HR125_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR124_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(124, MAC_MA32_127HR124_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR124            :%#x\n",
		MAC_MA32_127HR124_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR124_fops = {
	.read = MAC_MA32_127HR124_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR123_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(123, MAC_MA32_127HR123_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR123            :%#x\n",
		MAC_MA32_127HR123_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR123_fops = {
	.read = MAC_MA32_127HR123_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR122_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(122, MAC_MA32_127HR122_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR122            :%#x\n",
		MAC_MA32_127HR122_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR122_fops = {
	.read = MAC_MA32_127HR122_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR121_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(121, MAC_MA32_127HR121_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR121            :%#x\n",
		MAC_MA32_127HR121_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR121_fops = {
	.read = MAC_MA32_127HR121_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR120_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(120, MAC_MA32_127HR120_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR120            :%#x\n",
		MAC_MA32_127HR120_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR120_fops = {
	.read = MAC_MA32_127HR120_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR119_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(119, MAC_MA32_127HR119_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR119            :%#x\n",
		MAC_MA32_127HR119_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR119_fops = {
	.read = MAC_MA32_127HR119_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR118_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(118, MAC_MA32_127HR118_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR118            :%#x\n",
		MAC_MA32_127HR118_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR118_fops = {
	.read = MAC_MA32_127HR118_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR117_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(117, MAC_MA32_127HR117_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR117            :%#x\n",
		MAC_MA32_127HR117_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR117_fops = {
	.read = MAC_MA32_127HR117_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR116_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(116, MAC_MA32_127HR116_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR116            :%#x\n",
		MAC_MA32_127HR116_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR116_fops = {
	.read = MAC_MA32_127HR116_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR115_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(115, MAC_MA32_127HR115_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR115            :%#x\n",
		MAC_MA32_127HR115_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR115_fops = {
	.read = MAC_MA32_127HR115_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR114_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(114, MAC_MA32_127HR114_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR114            :%#x\n",
		MAC_MA32_127HR114_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR114_fops = {
	.read = MAC_MA32_127HR114_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR113_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(113, MAC_MA32_127HR113_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR113            :%#x\n",
		MAC_MA32_127HR113_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR113_fops = {
	.read = MAC_MA32_127HR113_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR112_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(112, MAC_MA32_127HR112_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR112            :%#x\n",
		MAC_MA32_127HR112_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR112_fops = {
	.read = MAC_MA32_127HR112_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR111_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(111, MAC_MA32_127HR111_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR111            :%#x\n",
		MAC_MA32_127HR111_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR111_fops = {
	.read = MAC_MA32_127HR111_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR110_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(110, MAC_MA32_127HR110_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR110            :%#x\n",
		MAC_MA32_127HR110_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR110_fops = {
	.read = MAC_MA32_127HR110_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR109_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(109, MAC_MA32_127HR109_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR109            :%#x\n",
		MAC_MA32_127HR109_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR109_fops = {
	.read = MAC_MA32_127HR109_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR108_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(108, MAC_MA32_127HR108_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR108            :%#x\n",
		MAC_MA32_127HR108_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR108_fops = {
	.read = MAC_MA32_127HR108_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR107_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(107, MAC_MA32_127HR107_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR107            :%#x\n",
		MAC_MA32_127HR107_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR107_fops = {
	.read = MAC_MA32_127HR107_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR106_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(106, MAC_MA32_127HR106_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR106            :%#x\n",
		MAC_MA32_127HR106_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR106_fops = {
	.read = MAC_MA32_127HR106_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR105_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(105, MAC_MA32_127HR105_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR105            :%#x\n",
		MAC_MA32_127HR105_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR105_fops = {
	.read = MAC_MA32_127HR105_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR104_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(104, MAC_MA32_127HR104_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR104            :%#x\n",
		MAC_MA32_127HR104_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR104_fops = {
	.read = MAC_MA32_127HR104_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR103_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(103, MAC_MA32_127HR103_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR103            :%#x\n",
		MAC_MA32_127HR103_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR103_fops = {
	.read = MAC_MA32_127HR103_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR102_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(102, MAC_MA32_127HR102_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR102            :%#x\n",
		MAC_MA32_127HR102_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR102_fops = {
	.read = MAC_MA32_127HR102_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR101_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(101, MAC_MA32_127HR101_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR101            :%#x\n",
		MAC_MA32_127HR101_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR101_fops = {
	.read = MAC_MA32_127HR101_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR100_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(100, MAC_MA32_127HR100_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR100            :%#x\n",
		MAC_MA32_127HR100_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR100_fops = {
	.read = MAC_MA32_127HR100_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR99_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(99, MAC_MA32_127HR99_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR99            :%#x\n",
		MAC_MA32_127HR99_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR99_fops = {
	.read = MAC_MA32_127HR99_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR98_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(98, MAC_MA32_127HR98_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR98            :%#x\n",
		MAC_MA32_127HR98_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR98_fops = {
	.read = MAC_MA32_127HR98_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR97_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(97, MAC_MA32_127HR97_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR97            :%#x\n",
		MAC_MA32_127HR97_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR97_fops = {
	.read = MAC_MA32_127HR97_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR96_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(96, MAC_MA32_127HR96_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR96            :%#x\n",
		MAC_MA32_127HR96_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR96_fops = {
	.read = MAC_MA32_127HR96_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR95_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(95, MAC_MA32_127HR95_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR95            :%#x\n",
		MAC_MA32_127HR95_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR95_fops = {
	.read = MAC_MA32_127HR95_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR94_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(94, MAC_MA32_127HR94_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR94            :%#x\n",
		MAC_MA32_127HR94_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR94_fops = {
	.read = MAC_MA32_127HR94_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR93_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(93, MAC_MA32_127HR93_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR93            :%#x\n",
		MAC_MA32_127HR93_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR93_fops = {
	.read = MAC_MA32_127HR93_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR92_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(92, MAC_MA32_127HR92_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR92            :%#x\n",
		MAC_MA32_127HR92_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR92_fops = {
	.read = MAC_MA32_127HR92_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR91_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(91, MAC_MA32_127HR91_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR91            :%#x\n",
		MAC_MA32_127HR91_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR91_fops = {
	.read = MAC_MA32_127HR91_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR90_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(90, MAC_MA32_127HR90_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR90            :%#x\n",
		MAC_MA32_127HR90_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR90_fops = {
	.read = MAC_MA32_127HR90_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR89_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(89, MAC_MA32_127HR89_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR89            :%#x\n",
		MAC_MA32_127HR89_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR89_fops = {
	.read = MAC_MA32_127HR89_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR88_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(88, MAC_MA32_127HR88_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR88            :%#x\n",
		MAC_MA32_127HR88_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR88_fops = {
	.read = MAC_MA32_127HR88_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR87_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(87, MAC_MA32_127HR87_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR87            :%#x\n",
		MAC_MA32_127HR87_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR87_fops = {
	.read = MAC_MA32_127HR87_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR86_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(86, MAC_MA32_127HR86_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR86            :%#x\n",
		MAC_MA32_127HR86_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR86_fops = {
	.read = MAC_MA32_127HR86_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR85_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(85, MAC_MA32_127HR85_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR85            :%#x\n",
		MAC_MA32_127HR85_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR85_fops = {
	.read = MAC_MA32_127HR85_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR84_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(84, MAC_MA32_127HR84_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR84            :%#x\n",
		MAC_MA32_127HR84_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR84_fops = {
	.read = MAC_MA32_127HR84_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR83_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(83, MAC_MA32_127HR83_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR83            :%#x\n",
		MAC_MA32_127HR83_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR83_fops = {
	.read = MAC_MA32_127HR83_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR82_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(82, MAC_MA32_127HR82_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR82            :%#x\n",
		MAC_MA32_127HR82_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR82_fops = {
	.read = MAC_MA32_127HR82_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR81_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(81, MAC_MA32_127HR81_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR81            :%#x\n",
		MAC_MA32_127HR81_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR81_fops = {
	.read = MAC_MA32_127HR81_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR80_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(80, MAC_MA32_127HR80_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR80            :%#x\n",
		MAC_MA32_127HR80_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR80_fops = {
	.read = MAC_MA32_127HR80_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR79_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(79, MAC_MA32_127HR79_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR79            :%#x\n",
		MAC_MA32_127HR79_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR79_fops = {
	.read = MAC_MA32_127HR79_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR78_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(78, MAC_MA32_127HR78_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR78            :%#x\n",
		MAC_MA32_127HR78_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR78_fops = {
	.read = MAC_MA32_127HR78_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR77_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(77, MAC_MA32_127HR77_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR77            :%#x\n",
		MAC_MA32_127HR77_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR77_fops = {
	.read = MAC_MA32_127HR77_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR76_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(76, MAC_MA32_127HR76_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR76            :%#x\n",
		MAC_MA32_127HR76_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR76_fops = {
	.read = MAC_MA32_127HR76_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR75_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(75, MAC_MA32_127HR75_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR75            :%#x\n",
		MAC_MA32_127HR75_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR75_fops = {
	.read = MAC_MA32_127HR75_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR74_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(74, MAC_MA32_127HR74_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR74            :%#x\n",
		MAC_MA32_127HR74_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR74_fops = {
	.read = MAC_MA32_127HR74_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR73_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(73, MAC_MA32_127HR73_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR73            :%#x\n",
		MAC_MA32_127HR73_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR73_fops = {
	.read = MAC_MA32_127HR73_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR72_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(72, MAC_MA32_127HR72_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR72            :%#x\n",
		MAC_MA32_127HR72_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR72_fops = {
	.read = MAC_MA32_127HR72_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR71_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(71, MAC_MA32_127HR71_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR71            :%#x\n",
		MAC_MA32_127HR71_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR71_fops = {
	.read = MAC_MA32_127HR71_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR70_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(70, MAC_MA32_127HR70_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR70            :%#x\n",
		MAC_MA32_127HR70_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR70_fops = {
	.read = MAC_MA32_127HR70_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR69_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(69, MAC_MA32_127HR69_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR69            :%#x\n",
		MAC_MA32_127HR69_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR69_fops = {
	.read = MAC_MA32_127HR69_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR68_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(68, MAC_MA32_127HR68_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR68            :%#x\n",
		MAC_MA32_127HR68_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR68_fops = {
	.read = MAC_MA32_127HR68_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR67_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(67, MAC_MA32_127HR67_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR67            :%#x\n",
		MAC_MA32_127HR67_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR67_fops = {
	.read = MAC_MA32_127HR67_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR66_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(66, MAC_MA32_127HR66_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR66            :%#x\n",
		MAC_MA32_127HR66_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR66_fops = {
	.read = MAC_MA32_127HR66_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR65_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(65, MAC_MA32_127HR65_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR65            :%#x\n",
		MAC_MA32_127HR65_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR65_fops = {
	.read = MAC_MA32_127HR65_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR64_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(64, MAC_MA32_127HR64_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR64            :%#x\n",
		MAC_MA32_127HR64_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR64_fops = {
	.read = MAC_MA32_127HR64_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR63_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(63, MAC_MA32_127HR63_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR63            :%#x\n",
		MAC_MA32_127HR63_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR63_fops = {
	.read = MAC_MA32_127HR63_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR62_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(62, MAC_MA32_127HR62_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR62            :%#x\n",
		MAC_MA32_127HR62_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR62_fops = {
	.read = MAC_MA32_127HR62_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR61_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(61, MAC_MA32_127HR61_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR61            :%#x\n",
		MAC_MA32_127HR61_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR61_fops = {
	.read = MAC_MA32_127HR61_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR60_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(60, MAC_MA32_127HR60_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR60            :%#x\n",
		MAC_MA32_127HR60_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR60_fops = {
	.read = MAC_MA32_127HR60_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR59_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(59, MAC_MA32_127HR59_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR59            :%#x\n",
		MAC_MA32_127HR59_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR59_fops = {
	.read = MAC_MA32_127HR59_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR58_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(58, MAC_MA32_127HR58_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR58            :%#x\n",
		MAC_MA32_127HR58_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR58_fops = {
	.read = MAC_MA32_127HR58_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR57_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(57, MAC_MA32_127HR57_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR57            :%#x\n",
		MAC_MA32_127HR57_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR57_fops = {
	.read = MAC_MA32_127HR57_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR56_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(56, MAC_MA32_127HR56_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR56            :%#x\n",
		MAC_MA32_127HR56_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR56_fops = {
	.read = MAC_MA32_127HR56_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR55_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(55, MAC_MA32_127HR55_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR55            :%#x\n",
		MAC_MA32_127HR55_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR55_fops = {
	.read = MAC_MA32_127HR55_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR54_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(54, MAC_MA32_127HR54_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR54            :%#x\n",
		MAC_MA32_127HR54_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR54_fops = {
	.read = MAC_MA32_127HR54_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR53_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(53, MAC_MA32_127HR53_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR53            :%#x\n",
		MAC_MA32_127HR53_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR53_fops = {
	.read = MAC_MA32_127HR53_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR52_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(52, MAC_MA32_127HR52_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR52            :%#x\n",
		MAC_MA32_127HR52_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR52_fops = {
	.read = MAC_MA32_127HR52_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR51_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(51, MAC_MA32_127HR51_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR51            :%#x\n",
		MAC_MA32_127HR51_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR51_fops = {
	.read = MAC_MA32_127HR51_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR50_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(50, MAC_MA32_127HR50_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR50            :%#x\n",
		MAC_MA32_127HR50_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR50_fops = {
	.read = MAC_MA32_127HR50_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR49_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(49, MAC_MA32_127HR49_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR49            :%#x\n",
		MAC_MA32_127HR49_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR49_fops = {
	.read = MAC_MA32_127HR49_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR48_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(48, MAC_MA32_127HR48_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR48            :%#x\n",
		MAC_MA32_127HR48_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR48_fops = {
	.read = MAC_MA32_127HR48_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR47_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(47, MAC_MA32_127HR47_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR47            :%#x\n",
		MAC_MA32_127HR47_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR47_fops = {
	.read = MAC_MA32_127HR47_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR46_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(46, MAC_MA32_127HR46_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR46            :%#x\n",
		MAC_MA32_127HR46_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR46_fops = {
	.read = MAC_MA32_127HR46_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR45_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(45, MAC_MA32_127HR45_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR45            :%#x\n",
		MAC_MA32_127HR45_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR45_fops = {
	.read = MAC_MA32_127HR45_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR44_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(44, MAC_MA32_127HR44_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR44            :%#x\n",
		MAC_MA32_127HR44_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR44_fops = {
	.read = MAC_MA32_127HR44_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR43_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(43, MAC_MA32_127HR43_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR43            :%#x\n",
		MAC_MA32_127HR43_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR43_fops = {
	.read = MAC_MA32_127HR43_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR42_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(42, MAC_MA32_127HR42_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR42            :%#x\n",
		MAC_MA32_127HR42_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR42_fops = {
	.read = MAC_MA32_127HR42_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR41_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(41, MAC_MA32_127HR41_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR41            :%#x\n",
		MAC_MA32_127HR41_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR41_fops = {
	.read = MAC_MA32_127HR41_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR40_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(40, MAC_MA32_127HR40_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR40            :%#x\n",
		MAC_MA32_127HR40_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR40_fops = {
	.read = MAC_MA32_127HR40_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR39_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(39, MAC_MA32_127HR39_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR39            :%#x\n",
		MAC_MA32_127HR39_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR39_fops = {
	.read = MAC_MA32_127HR39_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR38_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(38, MAC_MA32_127HR38_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR38            :%#x\n",
		MAC_MA32_127HR38_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR38_fops = {
	.read = MAC_MA32_127HR38_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR37_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(37, MAC_MA32_127HR37_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR37            :%#x\n",
		MAC_MA32_127HR37_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR37_fops = {
	.read = MAC_MA32_127HR37_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR36_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(36, MAC_MA32_127HR36_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR36            :%#x\n",
		MAC_MA32_127HR36_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR36_fops = {
	.read = MAC_MA32_127HR36_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR35_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(35, MAC_MA32_127HR35_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR35            :%#x\n",
		MAC_MA32_127HR35_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR35_fops = {
	.read = MAC_MA32_127HR35_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR34_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(34, MAC_MA32_127HR34_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR34            :%#x\n",
		MAC_MA32_127HR34_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR34_fops = {
	.read = MAC_MA32_127HR34_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR33_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(33, MAC_MA32_127HR33_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR33            :%#x\n",
		MAC_MA32_127HR33_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR33_fops = {
	.read = MAC_MA32_127HR33_read,
	.write = eqos_write,
};

static ssize_t MAC_MA32_127HR32_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA32_127HR_RD(32, MAC_MA32_127HR32_val);
	sprintf(debugfs_buf, "MAC_MA32_127HR32            :%#x\n",
		MAC_MA32_127HR32_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA32_127HR32_fops = {
	.read = MAC_MA32_127HR32_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR31_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(31, MAC_MA1_31LR31_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR31              :%#x\n",
		MAC_MA1_31LR31_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR31_fops = {
	.read = MAC_MA1_31LR31_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR30_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(30, MAC_MA1_31LR30_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR30              :%#x\n",
		MAC_MA1_31LR30_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR30_fops = {
	.read = MAC_MA1_31LR30_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR29_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(29, MAC_MA1_31LR29_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR29              :%#x\n",
		MAC_MA1_31LR29_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR29_fops = {
	.read = MAC_MA1_31LR29_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR28_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(28, MAC_MA1_31LR28_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR28              :%#x\n",
		MAC_MA1_31LR28_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR28_fops = {
	.read = MAC_MA1_31LR28_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR27_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(27, MAC_MA1_31LR27_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR27              :%#x\n",
		MAC_MA1_31LR27_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR27_fops = {
	.read = MAC_MA1_31LR27_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR26_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(26, MAC_MA1_31LR26_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR26              :%#x\n",
		MAC_MA1_31LR26_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR26_fops = {
	.read = MAC_MA1_31LR26_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR25_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(25, MAC_MA1_31LR25_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR25              :%#x\n",
		MAC_MA1_31LR25_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR25_fops = {
	.read = MAC_MA1_31LR25_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR24_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(24, MAC_MA1_31LR24_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR24              :%#x\n",
		MAC_MA1_31LR24_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR24_fops = {
	.read = MAC_MA1_31LR24_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR23_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(23, MAC_MA1_31LR23_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR23              :%#x\n",
		MAC_MA1_31LR23_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR23_fops = {
	.read = MAC_MA1_31LR23_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR22_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(22, MAC_MA1_31LR22_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR22              :%#x\n",
		MAC_MA1_31LR22_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR22_fops = {
	.read = MAC_MA1_31LR22_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR21_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(21, MAC_MA1_31LR21_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR21              :%#x\n",
		MAC_MA1_31LR21_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR21_fops = {
	.read = MAC_MA1_31LR21_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR20_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(20, MAC_MA1_31LR20_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR20              :%#x\n",
		MAC_MA1_31LR20_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR20_fops = {
	.read = MAC_MA1_31LR20_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR19_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(19, MAC_MA1_31LR19_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR19              :%#x\n",
		MAC_MA1_31LR19_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR19_fops = {
	.read = MAC_MA1_31LR19_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR18_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(18, MAC_MA1_31LR18_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR18              :%#x\n",
		MAC_MA1_31LR18_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR18_fops = {
	.read = MAC_MA1_31LR18_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR17_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(17, MAC_MA1_31LR17_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR17              :%#x\n",
		MAC_MA1_31LR17_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR17_fops = {
	.read = MAC_MA1_31LR17_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR16_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(16, MAC_MA1_31LR16_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR16              :%#x\n",
		MAC_MA1_31LR16_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR16_fops = {
	.read = MAC_MA1_31LR16_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR15_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(15, MAC_MA1_31LR15_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR15              :%#x\n",
		MAC_MA1_31LR15_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR15_fops = {
	.read = MAC_MA1_31LR15_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR14_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(14, MAC_MA1_31LR14_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR14              :%#x\n",
		MAC_MA1_31LR14_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR14_fops = {
	.read = MAC_MA1_31LR14_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR13_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(13, MAC_MA1_31LR13_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR13              :%#x\n",
		MAC_MA1_31LR13_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR13_fops = {
	.read = MAC_MA1_31LR13_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR12_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(12, MAC_MA1_31LR12_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR12              :%#x\n",
		MAC_MA1_31LR12_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR12_fops = {
	.read = MAC_MA1_31LR12_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR11_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(11, MAC_MA1_31LR11_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR11              :%#x\n",
		MAC_MA1_31LR11_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR11_fops = {
	.read = MAC_MA1_31LR11_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR10_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(10, MAC_MA1_31LR10_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR10              :%#x\n",
		MAC_MA1_31LR10_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR10_fops = {
	.read = MAC_MA1_31LR10_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR9_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(9, MAC_MA1_31LR9_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR9              :%#x\n",
		MAC_MA1_31LR9_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR9_fops = {
	.read = MAC_MA1_31LR9_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR8_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(8, MAC_MA1_31LR8_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR8              :%#x\n",
		MAC_MA1_31LR8_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR8_fops = {
	.read = MAC_MA1_31LR8_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR7_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(7, MAC_MA1_31LR7_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR7              :%#x\n",
		MAC_MA1_31LR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR7_fops = {
	.read = MAC_MA1_31LR7_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR6_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(6, MAC_MA1_31LR6_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR6              :%#x\n",
		MAC_MA1_31LR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR6_fops = {
	.read = MAC_MA1_31LR6_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR5_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(5, MAC_MA1_31LR5_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR5              :%#x\n",
		MAC_MA1_31LR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR5_fops = {
	.read = MAC_MA1_31LR5_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR4_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(4, MAC_MA1_31LR4_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR4              :%#x\n",
		MAC_MA1_31LR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR4_fops = {
	.read = MAC_MA1_31LR4_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR3_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(3, MAC_MA1_31LR3_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR3              :%#x\n",
		MAC_MA1_31LR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR3_fops = {
	.read = MAC_MA1_31LR3_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR2_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(2, MAC_MA1_31LR2_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR2              :%#x\n",
		MAC_MA1_31LR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR2_fops = {
	.read = MAC_MA1_31LR2_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31LR1_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31LR_RD(1, MAC_MA1_31LR1_val);
	sprintf(debugfs_buf, "MAC_MA1_31LR1              :%#x\n",
		MAC_MA1_31LR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31LR1_fops = {
	.read = MAC_MA1_31LR1_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR31_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(31, MAC_MA1_31HR31_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR31              :%#x\n",
		MAC_MA1_31HR31_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR31_fops = {
	.read = MAC_MA1_31HR31_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR30_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(30, MAC_MA1_31HR30_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR30              :%#x\n",
		MAC_MA1_31HR30_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR30_fops = {
	.read = MAC_MA1_31HR30_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR29_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(29, MAC_MA1_31HR29_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR29              :%#x\n",
		MAC_MA1_31HR29_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR29_fops = {
	.read = MAC_MA1_31HR29_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR28_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(28, MAC_MA1_31HR28_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR28              :%#x\n",
		MAC_MA1_31HR28_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR28_fops = {
	.read = MAC_MA1_31HR28_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR27_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(27, MAC_MA1_31HR27_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR27              :%#x\n",
		MAC_MA1_31HR27_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR27_fops = {
	.read = MAC_MA1_31HR27_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR26_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(26, MAC_MA1_31HR26_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR26              :%#x\n",
		MAC_MA1_31HR26_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR26_fops = {
	.read = MAC_MA1_31HR26_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR25_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(25, MAC_MA1_31HR25_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR25              :%#x\n",
		MAC_MA1_31HR25_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR25_fops = {
	.read = MAC_MA1_31HR25_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR24_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(24, MAC_MA1_31HR24_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR24              :%#x\n",
		MAC_MA1_31HR24_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR24_fops = {
	.read = MAC_MA1_31HR24_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR23_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(23, MAC_MA1_31HR23_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR23              :%#x\n",
		MAC_MA1_31HR23_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR23_fops = {
	.read = MAC_MA1_31HR23_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR22_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(22, MAC_MA1_31HR22_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR22              :%#x\n",
		MAC_MA1_31HR22_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR22_fops = {
	.read = MAC_MA1_31HR22_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR21_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(21, MAC_MA1_31HR21_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR21              :%#x\n",
		MAC_MA1_31HR21_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR21_fops = {
	.read = MAC_MA1_31HR21_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR20_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(20, MAC_MA1_31HR20_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR20              :%#x\n",
		MAC_MA1_31HR20_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR20_fops = {
	.read = MAC_MA1_31HR20_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR19_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(19, MAC_MA1_31HR19_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR19              :%#x\n",
		MAC_MA1_31HR19_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR19_fops = {
	.read = MAC_MA1_31HR19_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR18_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(18, MAC_MA1_31HR18_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR18              :%#x\n",
		MAC_MA1_31HR18_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR18_fops = {
	.read = MAC_MA1_31HR18_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR17_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(17, MAC_MA1_31HR17_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR17              :%#x\n",
		MAC_MA1_31HR17_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR17_fops = {
	.read = MAC_MA1_31HR17_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR16_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(16, MAC_MA1_31HR16_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR16              :%#x\n",
		MAC_MA1_31HR16_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR16_fops = {
	.read = MAC_MA1_31HR16_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR15_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(15, MAC_MA1_31HR15_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR15              :%#x\n",
		MAC_MA1_31HR15_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR15_fops = {
	.read = MAC_MA1_31HR15_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR14_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(14, MAC_MA1_31HR14_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR14              :%#x\n",
		MAC_MA1_31HR14_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR14_fops = {
	.read = MAC_MA1_31HR14_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR13_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(13, MAC_MA1_31HR13_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR13              :%#x\n",
		MAC_MA1_31HR13_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR13_fops = {
	.read = MAC_MA1_31HR13_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR12_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(12, MAC_MA1_31HR12_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR12              :%#x\n",
		MAC_MA1_31HR12_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR12_fops = {
	.read = MAC_MA1_31HR12_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR11_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(11, MAC_MA1_31HR11_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR11              :%#x\n",
		MAC_MA1_31HR11_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR11_fops = {
	.read = MAC_MA1_31HR11_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR10_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(10, MAC_MA1_31HR10_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR10              :%#x\n",
		MAC_MA1_31HR10_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR10_fops = {
	.read = MAC_MA1_31HR10_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR9_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(9, MAC_MA1_31HR9_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR9              :%#x\n",
		MAC_MA1_31HR9_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR9_fops = {
	.read = MAC_MA1_31HR9_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR8_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(8, MAC_MA1_31HR8_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR8              :%#x\n",
		MAC_MA1_31HR8_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR8_fops = {
	.read = MAC_MA1_31HR8_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR7_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(7, MAC_MA1_31HR7_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR7              :%#x\n",
		MAC_MA1_31HR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR7_fops = {
	.read = MAC_MA1_31HR7_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR6_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(6, MAC_MA1_31HR6_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR6              :%#x\n",
		MAC_MA1_31HR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR6_fops = {
	.read = MAC_MA1_31HR6_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR5_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(5, MAC_MA1_31HR5_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR5              :%#x\n",
		MAC_MA1_31HR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR5_fops = {
	.read = MAC_MA1_31HR5_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR4_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(4, MAC_MA1_31HR4_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR4              :%#x\n",
		MAC_MA1_31HR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR4_fops = {
	.read = MAC_MA1_31HR4_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR3_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(3, MAC_MA1_31HR3_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR3              :%#x\n",
		MAC_MA1_31HR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR3_fops = {
	.read = MAC_MA1_31HR3_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR2_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(2, MAC_MA1_31HR2_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR2              :%#x\n",
		MAC_MA1_31HR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR2_fops = {
	.read = MAC_MA1_31HR2_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1_31HR1_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1_31HR_RD(1, MAC_MA1_31HR1_val);
	sprintf(debugfs_buf, "MAC_MA1_31HR1              :%#x\n",
		MAC_MA1_31HR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1_31HR1_fops = {
	.read = MAC_MA1_31HR1_read,
	.write = eqos_write,
};

static ssize_t mac_arpa_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_ARPA_RD(mac_arpa_val);
	sprintf(debugfs_buf, "MAC_ARPA                  :%#x\n", mac_arpa_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_arpa_fops = {
	.read = mac_arpa_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A3R7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A3R7_RD(MAC_L3A3R7_val);
	sprintf(debugfs_buf, "MAC_L3A3R7                 :%#x\n",
		MAC_L3A3R7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A3R7_fops = {
	.read = MAC_L3A3R7_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A3R6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A3R6_RD(MAC_L3A3R6_val);
	sprintf(debugfs_buf, "MAC_L3A3R6                 :%#x\n",
		MAC_L3A3R6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A3R6_fops = {
	.read = MAC_L3A3R6_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A3R5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A3R5_RD(MAC_L3A3R5_val);
	sprintf(debugfs_buf, "MAC_L3A3R5                 :%#x\n",
		MAC_L3A3R5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A3R5_fops = {
	.read = MAC_L3A3R5_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A3R4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A3R4_RD(MAC_L3A3R4_val);
	sprintf(debugfs_buf, "MAC_L3A3R4                 :%#x\n",
		MAC_L3A3R4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A3R4_fops = {
	.read = MAC_L3A3R4_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A3R3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A3R3_RD(MAC_L3A3R3_val);
	sprintf(debugfs_buf, "MAC_L3A3R3                 :%#x\n",
		MAC_L3A3R3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A3R3_fops = {
	.read = MAC_L3A3R3_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A3R2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A3R2_RD(MAC_L3A3R2_val);
	sprintf(debugfs_buf, "MAC_L3A3R2                 :%#x\n",
		MAC_L3A3R2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A3R2_fops = {
	.read = MAC_L3A3R2_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A3R1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A3R1_RD(MAC_L3A3R1_val);
	sprintf(debugfs_buf, "MAC_L3A3R1                 :%#x\n",
		MAC_L3A3R1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A3R1_fops = {
	.read = MAC_L3A3R1_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A3R0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A3R0_RD(MAC_L3A3R0_val);
	sprintf(debugfs_buf, "MAC_L3A3R0                 :%#x\n",
		MAC_L3A3R0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A3R0_fops = {
	.read = MAC_L3A3R0_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A2R7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A2R7_RD(MAC_L3A2R7_val);
	sprintf(debugfs_buf, "MAC_L3A2R7                 :%#x\n",
		MAC_L3A2R7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A2R7_fops = {
	.read = MAC_L3A2R7_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A2R6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A2R6_RD(MAC_L3A2R6_val);
	sprintf(debugfs_buf, "MAC_L3A2R6                 :%#x\n",
		MAC_L3A2R6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A2R6_fops = {
	.read = MAC_L3A2R6_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A2R5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A2R5_RD(MAC_L3A2R5_val);
	sprintf(debugfs_buf, "MAC_L3A2R5                 :%#x\n",
		MAC_L3A2R5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A2R5_fops = {
	.read = MAC_L3A2R5_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A2R4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A2R4_RD(MAC_L3A2R4_val);
	sprintf(debugfs_buf, "MAC_L3A2R4                 :%#x\n",
		MAC_L3A2R4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A2R4_fops = {
	.read = MAC_L3A2R4_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A2R3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A2R3_RD(MAC_L3A2R3_val);
	sprintf(debugfs_buf, "MAC_L3A2R3                 :%#x\n",
		MAC_L3A2R3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A2R3_fops = {
	.read = MAC_L3A2R3_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A2R2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A2R2_RD(MAC_L3A2R2_val);
	sprintf(debugfs_buf, "MAC_L3A2R2                 :%#x\n",
		MAC_L3A2R2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A2R2_fops = {
	.read = MAC_L3A2R2_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A2R1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A2R1_RD(MAC_L3A2R1_val);
	sprintf(debugfs_buf, "MAC_L3A2R1                 :%#x\n",
		MAC_L3A2R1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A2R1_fops = {
	.read = MAC_L3A2R1_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A2R0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A2R0_RD(MAC_L3A2R0_val);
	sprintf(debugfs_buf, "MAC_L3A2R0                 :%#x\n",
		MAC_L3A2R0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A2R0_fops = {
	.read = MAC_L3A2R0_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A1R7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A1R7_RD(MAC_L3A1R7_val);
	sprintf(debugfs_buf, "MAC_L3A1R7                 :%#x\n",
		MAC_L3A1R7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A1R7_fops = {
	.read = MAC_L3A1R7_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A1R6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A1R6_RD(MAC_L3A1R6_val);
	sprintf(debugfs_buf, "MAC_L3A1R6                 :%#x\n",
		MAC_L3A1R6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A1R6_fops = {
	.read = MAC_L3A1R6_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A1R5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A1R5_RD(MAC_L3A1R5_val);
	sprintf(debugfs_buf, "MAC_L3A1R5                 :%#x\n",
		MAC_L3A1R5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A1R5_fops = {
	.read = MAC_L3A1R5_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A1R4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A1R4_RD(MAC_L3A1R4_val);
	sprintf(debugfs_buf, "MAC_L3A1R4                 :%#x\n",
		MAC_L3A1R4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A1R4_fops = {
	.read = MAC_L3A1R4_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A1R3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A1R3_RD(MAC_L3A1R3_val);
	sprintf(debugfs_buf, "MAC_L3A1R3                 :%#x\n",
		MAC_L3A1R3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A1R3_fops = {
	.read = MAC_L3A1R3_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A1R2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A1R2_RD(MAC_L3A1R2_val);
	sprintf(debugfs_buf, "MAC_L3A1R2                 :%#x\n",
		MAC_L3A1R2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A1R2_fops = {
	.read = MAC_L3A1R2_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A1R1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A1R1_RD(MAC_L3A1R1_val);
	sprintf(debugfs_buf, "MAC_L3A1R1                 :%#x\n",
		MAC_L3A1R1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A1R1_fops = {
	.read = MAC_L3A1R1_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A1R0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A1R0_RD(MAC_L3A1R0_val);
	sprintf(debugfs_buf, "MAC_L3A1R0                 :%#x\n",
		MAC_L3A1R0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A1R0_fops = {
	.read = MAC_L3A1R0_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A0R7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A0R7_RD(MAC_L3A0R7_val);
	sprintf(debugfs_buf, "MAC_L3A0R7                 :%#x\n",
		MAC_L3A0R7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A0R7_fops = {
	.read = MAC_L3A0R7_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A0R6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A0R6_RD(MAC_L3A0R6_val);
	sprintf(debugfs_buf, "MAC_L3A0R6                 :%#x\n",
		MAC_L3A0R6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A0R6_fops = {
	.read = MAC_L3A0R6_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A0R5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A0R5_RD(MAC_L3A0R5_val);
	sprintf(debugfs_buf, "MAC_L3A0R5                 :%#x\n",
		MAC_L3A0R5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A0R5_fops = {
	.read = MAC_L3A0R5_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A0R4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A0R4_RD(MAC_L3A0R4_val);
	sprintf(debugfs_buf, "MAC_L3A0R4                 :%#x\n",
		MAC_L3A0R4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A0R4_fops = {
	.read = MAC_L3A0R4_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A0R3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A0R3_RD(MAC_L3A0R3_val);
	sprintf(debugfs_buf, "MAC_L3A0R3                 :%#x\n",
		MAC_L3A0R3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A0R3_fops = {
	.read = MAC_L3A0R3_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A0R2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A0R2_RD(MAC_L3A0R2_val);
	sprintf(debugfs_buf, "MAC_L3A0R2                 :%#x\n",
		MAC_L3A0R2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A0R2_fops = {
	.read = MAC_L3A0R2_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A0R1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A0R1_RD(MAC_L3A0R1_val);
	sprintf(debugfs_buf, "MAC_L3A0R1                 :%#x\n",
		MAC_L3A0R1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A0R1_fops = {
	.read = MAC_L3A0R1_read,
	.write = eqos_write,
};

static ssize_t MAC_L3A0R0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3A0R0_RD(MAC_L3A0R0_val);
	sprintf(debugfs_buf, "MAC_L3A0R0                 :%#x\n",
		MAC_L3A0R0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3A0R0_fops = {
	.read = MAC_L3A0R0_read,
	.write = eqos_write,
};

static ssize_t MAC_L4AR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L4AR7_RD(MAC_L4AR7_val);
	sprintf(debugfs_buf, "MAC_L4AR7                :%#x\n", MAC_L4AR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L4AR7_fops = {
	.read = MAC_L4AR7_read,
	.write = eqos_write,
};

static ssize_t MAC_L4AR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L4AR6_RD(MAC_L4AR6_val);
	sprintf(debugfs_buf, "MAC_L4AR6                :%#x\n", MAC_L4AR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L4AR6_fops = {
	.read = MAC_L4AR6_read,
	.write = eqos_write,
};

static ssize_t MAC_L4AR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L4AR5_RD(MAC_L4AR5_val);
	sprintf(debugfs_buf, "MAC_L4AR5                :%#x\n", MAC_L4AR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L4AR5_fops = {
	.read = MAC_L4AR5_read,
	.write = eqos_write,
};

static ssize_t MAC_L4AR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L4AR4_RD(MAC_L4AR4_val);
	sprintf(debugfs_buf, "MAC_L4AR4                :%#x\n", MAC_L4AR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L4AR4_fops = {
	.read = MAC_L4AR4_read,
	.write = eqos_write,
};

static ssize_t MAC_L4AR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L4AR3_RD(MAC_L4AR3_val);
	sprintf(debugfs_buf, "MAC_L4AR3                :%#x\n", MAC_L4AR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L4AR3_fops = {
	.read = MAC_L4AR3_read,
	.write = eqos_write,
};

static ssize_t MAC_L4AR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L4AR2_RD(MAC_L4AR2_val);
	sprintf(debugfs_buf, "MAC_L4AR2                :%#x\n", MAC_L4AR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L4AR2_fops = {
	.read = MAC_L4AR2_read,
	.write = eqos_write,
};

static ssize_t MAC_L4AR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L4AR1_RD(MAC_L4AR1_val);
	sprintf(debugfs_buf, "MAC_L4AR1                :%#x\n", MAC_L4AR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L4AR1_fops = {
	.read = MAC_L4AR1_read,
	.write = eqos_write,
};

static ssize_t MAC_L4AR0_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L4AR0_RD(MAC_L4AR0_val);
	sprintf(debugfs_buf, "MAC_L4AR0                :%#x\n", MAC_L4AR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L4AR0_fops = {
	.read = MAC_L4AR0_read,
	.write = eqos_write,
};

static ssize_t MAC_L3L4CR7_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3L4CR7_RD(MAC_L3L4CR7_val);
	sprintf(debugfs_buf, "MAC_L3L4CR7                :%#x\n",
		MAC_L3L4CR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3L4CR7_fops = {
	.read = MAC_L3L4CR7_read,
	.write = eqos_write,
};

static ssize_t MAC_L3L4CR6_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3L4CR6_RD(MAC_L3L4CR6_val);
	sprintf(debugfs_buf, "MAC_L3L4CR6                :%#x\n",
		MAC_L3L4CR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3L4CR6_fops = {
	.read = MAC_L3L4CR6_read,
	.write = eqos_write,
};

static ssize_t MAC_L3L4CR5_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3L4CR5_RD(MAC_L3L4CR5_val);
	sprintf(debugfs_buf, "MAC_L3L4CR5                :%#x\n",
		MAC_L3L4CR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3L4CR5_fops = {
	.read = MAC_L3L4CR5_read,
	.write = eqos_write,
};

static ssize_t MAC_L3L4CR4_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3L4CR4_RD(MAC_L3L4CR4_val);
	sprintf(debugfs_buf, "MAC_L3L4CR4                :%#x\n",
		MAC_L3L4CR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3L4CR4_fops = {
	.read = MAC_L3L4CR4_read,
	.write = eqos_write,
};

static ssize_t MAC_L3L4CR3_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3L4CR3_RD(MAC_L3L4CR3_val);
	sprintf(debugfs_buf, "MAC_L3L4CR3                :%#x\n",
		MAC_L3L4CR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3L4CR3_fops = {
	.read = MAC_L3L4CR3_read,
	.write = eqos_write,
};

static ssize_t MAC_L3L4CR2_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3L4CR2_RD(MAC_L3L4CR2_val);
	sprintf(debugfs_buf, "MAC_L3L4CR2                :%#x\n",
		MAC_L3L4CR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3L4CR2_fops = {
	.read = MAC_L3L4CR2_read,
	.write = eqos_write,
};

static ssize_t MAC_L3L4CR1_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3L4CR1_RD(MAC_L3L4CR1_val);
	sprintf(debugfs_buf, "MAC_L3L4CR1                :%#x\n",
		MAC_L3L4CR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3L4CR1_fops = {
	.read = MAC_L3L4CR1_read,
	.write = eqos_write,
};

static ssize_t MAC_L3L4CR0_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_L3L4CR0_RD(MAC_L3L4CR0_val);
	sprintf(debugfs_buf, "MAC_L3L4CR0                :%#x\n",
		MAC_L3L4CR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_L3L4CR0_fops = {
	.read = MAC_L3L4CR0_read,
	.write = eqos_write,
};

static ssize_t mac_gpios_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_GPIOS_RD(mac_gpios_val);
	sprintf(debugfs_buf, "MAC_GPIOS                  :%#x\n",
		mac_gpios_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_gpios_fops = {
	.read = mac_gpios_read,
	.write = eqos_write,
};

static ssize_t mac_pcs_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PCS_RD(mac_pcs_val);
	sprintf(debugfs_buf, "MAC_PCS                    :%#x\n", mac_pcs_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pcs_fops = {
	.read = mac_pcs_read,
	.write = eqos_write,
};

static ssize_t mac_tes_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TES_RD(mac_tes_val);
	sprintf(debugfs_buf, "MAC_TES                    :%#x\n", mac_tes_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tes_fops = {
	.read = mac_tes_read,
	.write = eqos_write,
};

static ssize_t mac_ae_read(struct file *file, char __user *userbuf,
			   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_AE_RD(mac_ae_val);
	sprintf(debugfs_buf, "MAC_AE                     :%#x\n", mac_ae_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ae_fops = {
	.read = mac_ae_read,
	.write = eqos_write,
};

static ssize_t mac_alpa_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_ALPA_RD(mac_alpa_val);
	sprintf(debugfs_buf, "MAC_ALPA                   :%#x\n", mac_alpa_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_alpa_fops = {
	.read = mac_alpa_read,
	.write = eqos_write,
};

static ssize_t mac_aad_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_AAD_RD(mac_aad_val);
	sprintf(debugfs_buf, "MAC_AAD                    :%#x\n", mac_aad_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_aad_fops = {
	.read = mac_aad_read,
	.write = eqos_write,
};

static ssize_t mac_ans_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_ANS_RD(mac_ans_val);
	sprintf(debugfs_buf, "MAC_ANS                    :%#x\n", mac_ans_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ans_fops = {
	.read = mac_ans_read,
	.write = eqos_write,
};

static ssize_t mac_anc_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_ANC_RD(mac_anc_val);
	sprintf(debugfs_buf, "MAC_ANC                    :%#x\n", mac_anc_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_anc_fops = {
	.read = mac_anc_read,
	.write = eqos_write,
};

static ssize_t mac_lpc_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_LPC_RD(mac_lpc_val);
	sprintf(debugfs_buf, "MAC_LPC                    :%#x\n", mac_lpc_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_lpc_fops = {
	.read = mac_lpc_read,
	.write = eqos_write,
};

static ssize_t mac_lps_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_LPS_RD(mac_lps_val);
	sprintf(debugfs_buf, "MAC_LPS                    :%#x\n", mac_lps_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_lps_fops = {
	.read = mac_lps_read,
	.write = eqos_write,
};


static ssize_t mac_lmir_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_LMIR_RD(mac_lmir_val);
	sprintf(debugfs_buf, "MAC_LMIR                  :%#x\n", mac_lmir_val);
	ret = simple_read_from_buffer(userbuf,
				count,
				ppos,
				debugfs_buf,
				strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_lmir_fops = {
	.read = mac_lmir_read,
	.write = eqos_write,
};

static ssize_t MAC_SPI2r_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_SPI2R_RD(MAC_SPI2r_val);
	sprintf(debugfs_buf, "MAC_SPI2R                :%#x\n", MAC_SPI2r_val);
	ret = simple_read_from_buffer(userbuf,
				count,
				ppos,
				debugfs_buf,
				strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_SPI2r_fops = {
	.read = MAC_SPI2r_read,
	.write = eqos_write,
};

static ssize_t MAC_SPI1r_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_SPI1R_RD(MAC_SPI1r_val);
	sprintf(debugfs_buf, "MAC_SPI1R                :%#x\n", MAC_SPI1r_val);
	ret = simple_read_from_buffer(userbuf,
				count,
				ppos,
				debugfs_buf,
				strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_SPI1r_fops = {
	.read = MAC_SPI1r_read,
	.write = eqos_write,
};

static ssize_t MAC_SPI0r_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_SPI0R_RD(MAC_SPI0r_val);
	sprintf(debugfs_buf, "MAC_SPI0R                :%#x\n", MAC_SPI0r_val);
	ret = simple_read_from_buffer(userbuf,
				count,
				ppos,
				debugfs_buf,
				strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_SPI0r_fops = {
	.read = MAC_SPI0r_read,
	.write = eqos_write,
};

static ssize_t mac_pto_cr_read(struct file *file, char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PTO_CR_RD(mac_pto_cr_val);
	sprintf(debugfs_buf, "MAC_PTO_CR              :%#x\n", mac_pto_cr_val);
	ret = simple_read_from_buffer(userbuf,
				count,
				ppos,
				debugfs_buf,
				strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pto_cr_fops = {
	.read = mac_pto_cr_read,
	.write = eqos_write,
};



static ssize_t MAC_PPS_WIDTH3_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_WIDTH3_RD(MAC_PPS_WIDTH3_val);
	sprintf(debugfs_buf, "MAC_PPS_WIDTH3             :%#x\n",
		MAC_PPS_WIDTH3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_WIDTH3_fops = {
	.read = MAC_PPS_WIDTH3_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_WIDTH2_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_WIDTH2_RD(MAC_PPS_WIDTH2_val);
	sprintf(debugfs_buf, "MAC_PPS_WIDTH2             :%#x\n",
		MAC_PPS_WIDTH2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_WIDTH2_fops = {
	.read = MAC_PPS_WIDTH2_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_WIDTH1_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_WIDTH1_RD(MAC_PPS_WIDTH1_val);
	sprintf(debugfs_buf, "MAC_PPS_WIDTH1             :%#x\n",
		MAC_PPS_WIDTH1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_WIDTH1_fops = {
	.read = MAC_PPS_WIDTH1_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_WIDTH0_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_WIDTH0_RD(MAC_PPS_WIDTH0_val);
	sprintf(debugfs_buf, "MAC_PPS_WIDTH0             :%#x\n",
		MAC_PPS_WIDTH0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_WIDTH0_fops = {
	.read = MAC_PPS_WIDTH0_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_INTVAL3_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_INTVAL3_RD(MAC_PPS_INTVAL3_val);
	sprintf(debugfs_buf, "MAC_PPS_INTVAL3            :%#x\n",
		MAC_PPS_INTVAL3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_INTVAL3_fops = {
	.read = MAC_PPS_INTVAL3_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_INTVAL2_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_INTVAL2_RD(MAC_PPS_INTVAL2_val);
	sprintf(debugfs_buf, "MAC_PPS_INTVAL2            :%#x\n",
		MAC_PPS_INTVAL2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_INTVAL2_fops = {
	.read = MAC_PPS_INTVAL2_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_INTVAL1_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_INTVAL1_RD(MAC_PPS_INTVAL1_val);
	sprintf(debugfs_buf, "MAC_PPS_INTVAL1            :%#x\n",
		MAC_PPS_INTVAL1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_INTVAL1_fops = {
	.read = MAC_PPS_INTVAL1_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_INTVAL0_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_INTVAL0_RD(MAC_PPS_INTVAL0_val);
	sprintf(debugfs_buf, "MAC_PPS_INTVAL0            :%#x\n",
		MAC_PPS_INTVAL0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_INTVAL0_fops = {
	.read = MAC_PPS_INTVAL0_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_TTNS3_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_TTNS3_RD(MAC_PPS_TTNS3_val);
	sprintf(debugfs_buf, "MAC_PPS_TTNS3              :%#x\n",
		MAC_PPS_TTNS3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_TTNS3_fops = {
	.read = MAC_PPS_TTNS3_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_TTNS2_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_TTNS2_RD(MAC_PPS_TTNS2_val);
	sprintf(debugfs_buf, "MAC_PPS_TTNS2              :%#x\n",
		MAC_PPS_TTNS2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_TTNS2_fops = {
	.read = MAC_PPS_TTNS2_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_TTNS1_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_TTNS1_RD(MAC_PPS_TTNS1_val);
	sprintf(debugfs_buf, "MAC_PPS_TTNS1              :%#x\n",
		MAC_PPS_TTNS1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_TTNS1_fops = {
	.read = MAC_PPS_TTNS1_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_TTNS0_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_TTNS0_RD(MAC_PPS_TTNS0_val);
	sprintf(debugfs_buf, "MAC_PPS_TTNS0              :%#x\n",
		MAC_PPS_TTNS0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_TTNS0_fops = {
	.read = MAC_PPS_TTNS0_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_TTS3_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_TTS3_RD(MAC_PPS_TTS3_val);
	sprintf(debugfs_buf, "MAC_PPS_TTS3               :%#x\n",
		MAC_PPS_TTS3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_TTS3_fops = {
	.read = MAC_PPS_TTS3_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_TTS2_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_TTS2_RD(MAC_PPS_TTS2_val);
	sprintf(debugfs_buf, "MAC_PPS_TTS2               :%#x\n",
		MAC_PPS_TTS2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_TTS2_fops = {
	.read = MAC_PPS_TTS2_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_TTS1_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_TTS1_RD(MAC_PPS_TTS1_val);
	sprintf(debugfs_buf, "MAC_PPS_TTS1               :%#x\n",
		MAC_PPS_TTS1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_TTS1_fops = {
	.read = MAC_PPS_TTS1_read,
	.write = eqos_write,
};

static ssize_t MAC_PPS_TTS0_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPS_TTS0_RD(MAC_PPS_TTS0_val);
	sprintf(debugfs_buf, "MAC_PPS_TTS0               :%#x\n",
		MAC_PPS_TTS0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_PPS_TTS0_fops = {
	.read = MAC_PPS_TTS0_read,
	.write = eqos_write,
};

static ssize_t mac_ppsc_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PPSC_RD(mac_ppsc_val);
	sprintf(debugfs_buf, "MAC_PPSC                  :%#x\n", mac_ppsc_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ppsc_fops = {
	.read = mac_ppsc_read,
	.write = eqos_write,
};

static ssize_t mac_teac_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TEAC_RD(mac_teac_val);
	sprintf(debugfs_buf, "MAC_TEAC                  :%#x\n", mac_teac_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_teac_fops = {
	.read = mac_teac_read,
	.write = eqos_write,
};

static ssize_t mac_tiac_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TIAC_RD(mac_tiac_val);
	sprintf(debugfs_buf, "MAC_TIAC                  :%#x\n", mac_tiac_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tiac_fops = {
	.read = mac_tiac_read,
	.write = eqos_write,
};

static ssize_t mac_ats_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_ATS_RD(mac_ats_val);
	sprintf(debugfs_buf, "MAC_ATS                    :%#x\n", mac_ats_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ats_fops = {
	.read = mac_ats_read,
	.write = eqos_write,
};

static ssize_t mac_atn_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_ATN_RD(mac_atn_val);
	sprintf(debugfs_buf, "MAC_ATN                    :%#x\n", mac_atn_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_atn_fops = {
	.read = mac_atn_read,
	.write = eqos_write,
};

static ssize_t mac_ac_read(struct file *file, char __user *userbuf,
			   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_AC_RD(mac_ac_val);
	sprintf(debugfs_buf, "MAC_AC                     :%#x\n", mac_ac_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ac_fops = {
	.read = mac_ac_read,
	.write = eqos_write,
};

static ssize_t mac_ttn_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TTN_RD(mac_ttn_val);
	sprintf(debugfs_buf, "MAC_TTN                    :%#x\n", mac_ttn_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ttn_fops = {
	.read = mac_ttn_read,
	.write = eqos_write,
};

static ssize_t mac_ttsn_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TTSN_RD(mac_ttsn_val);
	sprintf(debugfs_buf, "MAC_TTSN                  :%#x\n", mac_ttsn_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ttsn_fops = {
	.read = mac_ttsn_read,
	.write = eqos_write,
};

static ssize_t mac_tsr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TSR_RD(mac_tsr_val);
	sprintf(debugfs_buf, "MAC_TSR                    :%#x\n", mac_tsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tsr_fops = {
	.read = mac_tsr_read,
	.write = eqos_write,
};

static ssize_t mac_sthwr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_STHWR_RD(mac_sthwr_val);
	sprintf(debugfs_buf, "MAC_STHWR                  :%#x\n",
		mac_sthwr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_sthwr_fops = {
	.read = mac_sthwr_read,
	.write = eqos_write,
};

static ssize_t mac_tar_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TAR_RD(mac_tar_val);
	sprintf(debugfs_buf, "MAC_TAR                    :%#x\n", mac_tar_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tar_fops = {
	.read = mac_tar_read,
	.write = eqos_write,
};

static ssize_t mac_stnsur_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_STNSUR_RD(mac_stnsur_val);
	sprintf(debugfs_buf, "MAC_STNSUR                 :%#x\n",
		mac_stnsur_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_stnsur_fops = {
	.read = mac_stnsur_read,
	.write = eqos_write,
};

static ssize_t mac_stsur_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_STSUR_RD(mac_stsur_val);
	sprintf(debugfs_buf, "MAC_STSUR                  :%#x\n",
		mac_stsur_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_stsur_fops = {
	.read = mac_stsur_read,
	.write = eqos_write,
};

static ssize_t mac_stnsr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_STNSR_RD(mac_stnsr_val);
	sprintf(debugfs_buf, "MAC_STNSR                  :%#x\n",
		mac_stnsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_stnsr_fops = {
	.read = mac_stnsr_read,
	.write = eqos_write,
};

static ssize_t mac_stsr_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_STSR_RD(mac_stsr_val);
	sprintf(debugfs_buf, "MAC_STSR                  :%#x\n", mac_stsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_stsr_fops = {
	.read = mac_stsr_read,
	.write = eqos_write,
};

static ssize_t mac_ssir_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_SSIR_RD(mac_ssir_val);
	sprintf(debugfs_buf, "MAC_SSIR                  :%#x\n", mac_ssir_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ssir_fops = {
	.read = mac_ssir_read,
	.write = eqos_write,
};

static ssize_t mac_tcr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TCR_RD(mac_tcr_val);
	sprintf(debugfs_buf, "MAC_TCR                    :%#x\n", mac_tcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_tcr_fops = {
	.read = mac_tcr_read,
	.write = eqos_write,
};

static ssize_t mtl_dsr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_DSR_RD(mtl_dsr_val);
	sprintf(debugfs_buf, "MTL_DSR                    :%#x\n", mtl_dsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_dsr_fops = {
	.read = mtl_dsr_read,
	.write = eqos_write,
};

static ssize_t mac_rwpffr_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_RWPFFR_RD(mac_rwpffr_val);
	sprintf(debugfs_buf, "MAC_RWPFFR                 :%#x\n",
		mac_rwpffr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rwpffr_fops = {
	.read = mac_rwpffr_read,
	.write = eqos_write,
};

static ssize_t mac_rtsr_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_RTSR_RD(mac_rtsr_val);
	sprintf(debugfs_buf, "MAC_RTSR                  :%#x\n", mac_rtsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rtsr_fops = {
	.read = mac_rtsr_read,
	.write = eqos_write,
};

static ssize_t mtl_ier_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_IER_RD(mtl_ier_val);
	sprintf(debugfs_buf, "MTL_IER                    :%#x\n", mtl_ier_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_ier_fops = {
	.read = mtl_ier_read,
	.write = eqos_write,
};

static ssize_t MTL_QRCR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRCR7_RD(MTL_QRCR7_val);
	sprintf(debugfs_buf, "MTL_QRCR7                  :%#x\n",
		MTL_QRCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRCR7_fops = {
	.read = MTL_QRCR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QRCR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRCR6_RD(MTL_QRCR6_val);
	sprintf(debugfs_buf, "MTL_QRCR6                  :%#x\n",
		MTL_QRCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRCR6_fops = {
	.read = MTL_QRCR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QRCR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRCR5_RD(MTL_QRCR5_val);
	sprintf(debugfs_buf, "MTL_QRCR5                  :%#x\n",
		MTL_QRCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRCR5_fops = {
	.read = MTL_QRCR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QRCR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRCR4_RD(MTL_QRCR4_val);
	sprintf(debugfs_buf, "MTL_QRCR4                  :%#x\n",
		MTL_QRCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRCR4_fops = {
	.read = MTL_QRCR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QRCR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRCR3_RD(MTL_QRCR3_val);
	sprintf(debugfs_buf, "MTL_QRCR3                  :%#x\n",
		MTL_QRCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRCR3_fops = {
	.read = MTL_QRCR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QRCR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRCR2_RD(MTL_QRCR2_val);
	sprintf(debugfs_buf, "MTL_QRCR2                  :%#x\n",
		MTL_QRCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRCR2_fops = {
	.read = MTL_QRCR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QRCR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRCR1_RD(MTL_QRCR1_val);
	sprintf(debugfs_buf, "MTL_QRCR1                  :%#x\n",
		MTL_QRCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRCR1_fops = {
	.read = MTL_QRCR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QRDR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRDR7_RD(MTL_QRDR7_val);
	sprintf(debugfs_buf, "MTL_QRDR7                  :%#x\n",
		MTL_QRDR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRDR7_fops = {
	.read = MTL_QRDR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QRDR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRDR6_RD(MTL_QRDR6_val);
	sprintf(debugfs_buf, "MTL_QRDR6                  :%#x\n",
		MTL_QRDR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRDR6_fops = {
	.read = MTL_QRDR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QRDR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRDR5_RD(MTL_QRDR5_val);
	sprintf(debugfs_buf, "MTL_QRDR5                  :%#x\n",
		MTL_QRDR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRDR5_fops = {
	.read = MTL_QRDR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QRDR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRDR4_RD(MTL_QRDR4_val);
	sprintf(debugfs_buf, "MTL_QRDR4                  :%#x\n",
		MTL_QRDR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRDR4_fops = {
	.read = MTL_QRDR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QRDR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRDR3_RD(MTL_QRDR3_val);
	sprintf(debugfs_buf, "MTL_QRDR3                  :%#x\n",
		MTL_QRDR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRDR3_fops = {
	.read = MTL_QRDR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QRDR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRDR2_RD(MTL_QRDR2_val);
	sprintf(debugfs_buf, "MTL_QRDR2                  :%#x\n",
		MTL_QRDR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRDR2_fops = {
	.read = MTL_QRDR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QRDR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QRDR1_RD(MTL_QRDR1_val);
	sprintf(debugfs_buf, "MTL_QRDR1                  :%#x\n",
		MTL_QRDR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QRDR1_fops = {
	.read = MTL_QRDR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QOCR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QOCR7_RD(MTL_QOCR7_val);
	sprintf(debugfs_buf, "MTL_QOCR7                  :%#x\n",
		MTL_QOCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QOCR7_fops = {
	.read = MTL_QOCR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QOCR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QOCR6_RD(MTL_QOCR6_val);
	sprintf(debugfs_buf, "MTL_QOCR6                  :%#x\n",
		MTL_QOCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QOCR6_fops = {
	.read = MTL_QOCR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QOCR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QOCR5_RD(MTL_QOCR5_val);
	sprintf(debugfs_buf, "MTL_QOCR5                  :%#x\n",
		MTL_QOCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QOCR5_fops = {
	.read = MTL_QOCR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QOCR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QOCR4_RD(MTL_QOCR4_val);
	sprintf(debugfs_buf, "MTL_QOCR4                  :%#x\n",
		MTL_QOCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QOCR4_fops = {
	.read = MTL_QOCR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QOCR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QOCR3_RD(MTL_QOCR3_val);
	sprintf(debugfs_buf, "MTL_QOCR3                  :%#x\n",
		MTL_QOCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QOCR3_fops = {
	.read = MTL_QOCR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QOCR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QOCR2_RD(MTL_QOCR2_val);
	sprintf(debugfs_buf, "MTL_QOCR2                  :%#x\n",
		MTL_QOCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QOCR2_fops = {
	.read = MTL_QOCR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QOCR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QOCR1_RD(MTL_QOCR1_val);
	sprintf(debugfs_buf, "MTL_QOCR1                  :%#x\n",
		MTL_QOCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QOCR1_fops = {
	.read = MTL_QOCR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QROMR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QROMR_RD(7, MTL_QROMR7_val);
	sprintf(debugfs_buf, "MTL_QROMR7                 :%#x\n",
		MTL_QROMR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QROMR7_fops = {
	.read = MTL_QROMR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QROMR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QROMR_RD(6, MTL_QROMR6_val);
	sprintf(debugfs_buf, "MTL_QROMR6                 :%#x\n",
		MTL_QROMR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QROMR6_fops = {
	.read = MTL_QROMR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QROMR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QROMR_RD(5, MTL_QROMR5_val);
	sprintf(debugfs_buf, "MTL_QROMR5                 :%#x\n",
		MTL_QROMR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QROMR5_fops = {
	.read = MTL_QROMR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QROMR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QROMR_RD(4, MTL_QROMR4_val);
	sprintf(debugfs_buf, "MTL_QROMR4                 :%#x\n",
		MTL_QROMR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QROMR4_fops = {
	.read = MTL_QROMR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QROMR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QROMR_RD(3, MTL_QROMR3_val);
	sprintf(debugfs_buf, "MTL_QROMR3                 :%#x\n",
		MTL_QROMR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QROMR3_fops = {
	.read = MTL_QROMR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QROMR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QROMR_RD(2, MTL_QROMR2_val);
	sprintf(debugfs_buf, "MTL_QROMR2                 :%#x\n",
		MTL_QROMR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QROMR2_fops = {
	.read = MTL_QROMR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QROMR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QROMR_RD(1, MTL_QROMR1_val);
	sprintf(debugfs_buf, "MTL_QROMR1                 :%#x\n",
		MTL_QROMR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QROMR1_fops = {
	.read = MTL_QROMR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QLCR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QLCR7_RD(MTL_QLCR7_val);
	sprintf(debugfs_buf, "MTL_QLCR7                  :%#x\n",
		MTL_QLCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QLCR7_fops = {
	.read = MTL_QLCR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QLCR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QLCR6_RD(MTL_QLCR6_val);
	sprintf(debugfs_buf, "MTL_QLCR6                  :%#x\n",
		MTL_QLCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QLCR6_fops = {
	.read = MTL_QLCR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QLCR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QLCR5_RD(MTL_QLCR5_val);
	sprintf(debugfs_buf, "MTL_QLCR5                  :%#x\n",
		MTL_QLCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QLCR5_fops = {
	.read = MTL_QLCR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QLCR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QLCR4_RD(MTL_QLCR4_val);
	sprintf(debugfs_buf, "MTL_QLCR4                  :%#x\n",
		MTL_QLCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QLCR4_fops = {
	.read = MTL_QLCR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QLCR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QLCR3_RD(MTL_QLCR3_val);
	sprintf(debugfs_buf, "MTL_QLCR3                  :%#x\n",
		MTL_QLCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QLCR3_fops = {
	.read = MTL_QLCR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QLCR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QLCR2_RD(MTL_QLCR2_val);
	sprintf(debugfs_buf, "MTL_QLCR2                  :%#x\n",
		MTL_QLCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QLCR2_fops = {
	.read = MTL_QLCR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QLCR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QLCR1_RD(MTL_QLCR1_val);
	sprintf(debugfs_buf, "MTL_QLCR1                  :%#x\n",
		MTL_QLCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QLCR1_fops = {
	.read = MTL_QLCR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QHCR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QHCR7_RD(MTL_QHCR7_val);
	sprintf(debugfs_buf, "MTL_QHCR7                  :%#x\n",
		MTL_QHCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QHCR7_fops = {
	.read = MTL_QHCR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QHCR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QHCR6_RD(MTL_QHCR6_val);
	sprintf(debugfs_buf, "MTL_QHCR6                  :%#x\n",
		MTL_QHCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QHCR6_fops = {
	.read = MTL_QHCR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QHCR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QHCR5_RD(MTL_QHCR5_val);
	sprintf(debugfs_buf, "MTL_QHCR5                  :%#x\n",
		MTL_QHCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QHCR5_fops = {
	.read = MTL_QHCR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QHCR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QHCR4_RD(MTL_QHCR4_val);
	sprintf(debugfs_buf, "MTL_QHCR4                  :%#x\n",
		MTL_QHCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QHCR4_fops = {
	.read = MTL_QHCR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QHCR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QHCR3_RD(MTL_QHCR3_val);
	sprintf(debugfs_buf, "MTL_QHCR3                  :%#x\n",
		MTL_QHCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QHCR3_fops = {
	.read = MTL_QHCR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QHCR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QHCR2_RD(MTL_QHCR2_val);
	sprintf(debugfs_buf, "MTL_QHCR2                  :%#x\n",
		MTL_QHCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QHCR2_fops = {
	.read = MTL_QHCR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QHCR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QHCR1_RD(MTL_QHCR1_val);
	sprintf(debugfs_buf, "MTL_QHCR1                  :%#x\n",
		MTL_QHCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QHCR1_fops = {
	.read = MTL_QHCR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QSSCR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QSSCR7_RD(MTL_QSSCR7_val);
	sprintf(debugfs_buf, "MTL_QSSCR7                 :%#x\n",
		MTL_QSSCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QSSCR7_fops = {
	.read = MTL_QSSCR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QSSCR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QSSCR6_RD(MTL_QSSCR6_val);
	sprintf(debugfs_buf, "MTL_QSSCR6                 :%#x\n",
		MTL_QSSCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QSSCR6_fops = {
	.read = MTL_QSSCR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QSSCR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QSSCR5_RD(MTL_QSSCR5_val);
	sprintf(debugfs_buf, "MTL_QSSCR5                 :%#x\n",
		MTL_QSSCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QSSCR5_fops = {
	.read = MTL_QSSCR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QSSCR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QSSCR4_RD(MTL_QSSCR4_val);
	sprintf(debugfs_buf, "MTL_QSSCR4                 :%#x\n",
		MTL_QSSCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QSSCR4_fops = {
	.read = MTL_QSSCR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QSSCR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QSSCR3_RD(MTL_QSSCR3_val);
	sprintf(debugfs_buf, "MTL_QSSCR3                 :%#x\n",
		MTL_QSSCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QSSCR3_fops = {
	.read = MTL_QSSCR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QSSCR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QSSCR2_RD(MTL_QSSCR2_val);
	sprintf(debugfs_buf, "MTL_QSSCR2                 :%#x\n",
		MTL_QSSCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QSSCR2_fops = {
	.read = MTL_QSSCR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QSSCR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QSSCR1_RD(MTL_QSSCR1_val);
	sprintf(debugfs_buf, "MTL_QSSCR1                 :%#x\n",
		MTL_QSSCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QSSCR1_fops = {
	.read = MTL_QSSCR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QW7_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QW7_RD(MTL_QW7_val);
	sprintf(debugfs_buf, "MTL_QW7                    :%#x\n", MTL_QW7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QW7_fops = {
	.read = MTL_QW7_read,
	.write = eqos_write,
};

static ssize_t MTL_QW6_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QW6_RD(MTL_QW6_val);
	sprintf(debugfs_buf, "MTL_QW6                    :%#x\n", MTL_QW6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QW6_fops = {
	.read = MTL_QW6_read,
	.write = eqos_write,
};

static ssize_t MTL_QW5_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QW5_RD(MTL_QW5_val);
	sprintf(debugfs_buf, "MTL_QW5                    :%#x\n", MTL_QW5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QW5_fops = {
	.read = MTL_QW5_read,
	.write = eqos_write,
};

static ssize_t MTL_QW4_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QW4_RD(MTL_QW4_val);
	sprintf(debugfs_buf, "MTL_QW4                    :%#x\n", MTL_QW4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QW4_fops = {
	.read = MTL_QW4_read,
	.write = eqos_write,
};

static ssize_t MTL_QW3_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QW3_RD(MTL_QW3_val);
	sprintf(debugfs_buf, "MTL_QW3                    :%#x\n", MTL_QW3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QW3_fops = {
	.read = MTL_QW3_read,
	.write = eqos_write,
};

static ssize_t MTL_QW2_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QW2_RD(MTL_QW2_val);
	sprintf(debugfs_buf, "MTL_QW2                    :%#x\n", MTL_QW2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QW2_fops = {
	.read = MTL_QW2_read,
	.write = eqos_write,
};

static ssize_t MTL_QW1_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QW1_RD(MTL_QW1_val);
	sprintf(debugfs_buf, "MTL_QW1                    :%#x\n", MTL_QW1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QW1_fops = {
	.read = MTL_QW1_read,
	.write = eqos_write,
};

static ssize_t MTL_QESR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QESR7_RD(MTL_QESR7_val);
	sprintf(debugfs_buf, "MTL_QESR7                  :%#x\n",
		MTL_QESR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QESR7_fops = {
	.read = MTL_QESR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QESR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QESR6_RD(MTL_QESR6_val);
	sprintf(debugfs_buf, "MTL_QESR6                  :%#x\n",
		MTL_QESR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QESR6_fops = {
	.read = MTL_QESR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QESR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QESR5_RD(MTL_QESR5_val);
	sprintf(debugfs_buf, "MTL_QESR5                  :%#x\n",
		MTL_QESR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QESR5_fops = {
	.read = MTL_QESR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QESR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QESR4_RD(MTL_QESR4_val);
	sprintf(debugfs_buf, "MTL_QESR4                  :%#x\n",
		MTL_QESR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QESR4_fops = {
	.read = MTL_QESR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QESR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QESR3_RD(MTL_QESR3_val);
	sprintf(debugfs_buf, "MTL_QESR3                  :%#x\n",
		MTL_QESR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QESR3_fops = {
	.read = MTL_QESR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QESR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QESR2_RD(MTL_QESR2_val);
	sprintf(debugfs_buf, "MTL_QESR2                  :%#x\n",
		MTL_QESR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QESR2_fops = {
	.read = MTL_QESR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QESR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QESR1_RD(MTL_QESR1_val);
	sprintf(debugfs_buf, "MTL_QESR1                  :%#x\n",
		MTL_QESR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QESR1_fops = {
	.read = MTL_QESR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QECR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QECR7_RD(MTL_QECR7_val);
	sprintf(debugfs_buf, "MTL_QECR7                  :%#x\n",
		MTL_QECR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QECR7_fops = {
	.read = MTL_QECR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QECR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QECR6_RD(MTL_QECR6_val);
	sprintf(debugfs_buf, "MTL_QECR6                  :%#x\n",
		MTL_QECR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QECR6_fops = {
	.read = MTL_QECR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QECR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QECR5_RD(MTL_QECR5_val);
	sprintf(debugfs_buf, "MTL_QECR5                  :%#x\n",
		MTL_QECR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QECR5_fops = {
	.read = MTL_QECR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QECR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QECR4_RD(MTL_QECR4_val);
	sprintf(debugfs_buf, "MTL_QECR4                  :%#x\n",
		MTL_QECR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QECR4_fops = {
	.read = MTL_QECR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QECR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QECR3_RD(MTL_QECR3_val);
	sprintf(debugfs_buf, "MTL_QECR3                  :%#x\n",
		MTL_QECR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QECR3_fops = {
	.read = MTL_QECR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QECR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QECR2_RD(MTL_QECR2_val);
	sprintf(debugfs_buf, "MTL_QECR2                  :%#x\n",
		MTL_QECR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QECR2_fops = {
	.read = MTL_QECR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QECR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QECR1_RD(MTL_QECR1_val);
	sprintf(debugfs_buf, "MTL_QECR1                  :%#x\n",
		MTL_QECR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QECR1_fops = {
	.read = MTL_QECR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QTDR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTDR7_RD(MTL_QTDR7_val);
	sprintf(debugfs_buf, "MTL_QTDR7                  :%#x\n",
		MTL_QTDR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTDR7_fops = {
	.read = MTL_QTDR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QTDR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTDR6_RD(MTL_QTDR6_val);
	sprintf(debugfs_buf, "MTL_QTDR6                  :%#x\n",
		MTL_QTDR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTDR6_fops = {
	.read = MTL_QTDR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QTDR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTDR5_RD(MTL_QTDR5_val);
	sprintf(debugfs_buf, "MTL_QTDR5                  :%#x\n",
		MTL_QTDR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTDR5_fops = {
	.read = MTL_QTDR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QTDR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTDR4_RD(MTL_QTDR4_val);
	sprintf(debugfs_buf, "MTL_QTDR4                  :%#x\n",
		MTL_QTDR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTDR4_fops = {
	.read = MTL_QTDR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QTDR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTDR3_RD(MTL_QTDR3_val);
	sprintf(debugfs_buf, "MTL_QTDR3                  :%#x\n",
		MTL_QTDR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTDR3_fops = {
	.read = MTL_QTDR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QTDR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTDR2_RD(MTL_QTDR2_val);
	sprintf(debugfs_buf, "MTL_QTDR2                  :%#x\n",
		MTL_QTDR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTDR2_fops = {
	.read = MTL_QTDR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QTDR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTDR1_RD(MTL_QTDR1_val);
	sprintf(debugfs_buf, "MTL_QTDR1                  :%#x\n",
		MTL_QTDR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTDR1_fops = {
	.read = MTL_QTDR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QUCR7_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QUCR7_RD(MTL_QUCR7_val);
	sprintf(debugfs_buf, "MTL_QUCR7                  :%#x\n",
		MTL_QUCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QUCR7_fops = {
	.read = MTL_QUCR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QUCR6_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QUCR6_RD(MTL_QUCR6_val);
	sprintf(debugfs_buf, "MTL_QUCR6                  :%#x\n",
		MTL_QUCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QUCR6_fops = {
	.read = MTL_QUCR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QUCR5_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QUCR5_RD(MTL_QUCR5_val);
	sprintf(debugfs_buf, "MTL_QUCR5                  :%#x\n",
		MTL_QUCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QUCR5_fops = {
	.read = MTL_QUCR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QUCR4_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QUCR4_RD(MTL_QUCR4_val);
	sprintf(debugfs_buf, "MTL_QUCR4                  :%#x\n",
		MTL_QUCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QUCR4_fops = {
	.read = MTL_QUCR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QUCR3_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QUCR3_RD(MTL_QUCR3_val);
	sprintf(debugfs_buf, "MTL_QUCR3                  :%#x\n",
		MTL_QUCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QUCR3_fops = {
	.read = MTL_QUCR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QUCR2_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QUCR2_RD(MTL_QUCR2_val);
	sprintf(debugfs_buf, "MTL_QUCR2                  :%#x\n",
		MTL_QUCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QUCR2_fops = {
	.read = MTL_QUCR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QUCR1_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QUCR1_RD(MTL_QUCR1_val);
	sprintf(debugfs_buf, "MTL_QUCR1                  :%#x\n",
		MTL_QUCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QUCR1_fops = {
	.read = MTL_QUCR1_read,
	.write = eqos_write,
};

static ssize_t MTL_QTOMR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTOMR7_RD(MTL_QTOMR7_val);
	sprintf(debugfs_buf, "MTL_QTOMR7                 :%#x\n",
		MTL_QTOMR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTOMR7_fops = {
	.read = MTL_QTOMR7_read,
	.write = eqos_write,
};

static ssize_t MTL_QTOMR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTOMR6_RD(MTL_QTOMR6_val);
	sprintf(debugfs_buf, "MTL_QTOMR6                 :%#x\n",
		MTL_QTOMR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTOMR6_fops = {
	.read = MTL_QTOMR6_read,
	.write = eqos_write,
};

static ssize_t MTL_QTOMR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTOMR5_RD(MTL_QTOMR5_val);
	sprintf(debugfs_buf, "MTL_QTOMR5                 :%#x\n",
		MTL_QTOMR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTOMR5_fops = {
	.read = MTL_QTOMR5_read,
	.write = eqos_write,
};

static ssize_t MTL_QTOMR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTOMR4_RD(MTL_QTOMR4_val);
	sprintf(debugfs_buf, "MTL_QTOMR4                 :%#x\n",
		MTL_QTOMR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTOMR4_fops = {
	.read = MTL_QTOMR4_read,
	.write = eqos_write,
};

static ssize_t MTL_QTOMR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTOMR3_RD(MTL_QTOMR3_val);
	sprintf(debugfs_buf, "MTL_QTOMR3                 :%#x\n",
		MTL_QTOMR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTOMR3_fops = {
	.read = MTL_QTOMR3_read,
	.write = eqos_write,
};

static ssize_t MTL_QTOMR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTOMR2_RD(MTL_QTOMR2_val);
	sprintf(debugfs_buf, "MTL_QTOMR2                 :%#x\n",
		MTL_QTOMR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTOMR2_fops = {
	.read = MTL_QTOMR2_read,
	.write = eqos_write,
};

static ssize_t MTL_QTOMR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_QTOMR1_RD(MTL_QTOMR1_val);
	sprintf(debugfs_buf, "MTL_QTOMR1                 :%#x\n",
		MTL_QTOMR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_QTOMR1_fops = {
	.read = MTL_QTOMR1_read,
	.write = eqos_write,
};

static ssize_t mac_pmtcsr_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_PMTCSR_RD(mac_pmtcsr_val);
	sprintf(debugfs_buf, "MAC_PMTCSR                 :%#x\n",
		mac_pmtcsr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_pmtcsr_fops = {
	.read = mac_pmtcsr_read,
	.write = eqos_write,
};

static ssize_t mmc_rxicmp_err_octets_read(struct file *file,
					  char __user *userbuf, size_t count,
					  loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXICMP_ERR_OCTETS_RD(mmc_rxicmp_err_octets_val);
	sprintf(debugfs_buf, "MMC_RXICMP_ERR_OCTETS      :%#x\n",
		mmc_rxicmp_err_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxicmp_err_octets_fops = {
	.read = mmc_rxicmp_err_octets_read,
	.write = eqos_write,
};

static ssize_t mmc_rxicmp_gd_octets_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXICMP_GD_OCTETS_RD(mmc_rxicmp_gd_octets_val);
	sprintf(debugfs_buf, "MMC_RXICMP_GD_OCTETS       :%#x\n",
		mmc_rxicmp_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxicmp_gd_octets_fops = {
	.read = mmc_rxicmp_gd_octets_read,
	.write = eqos_write,
};

static ssize_t mmc_rxtcp_err_octets_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXTCP_ERR_OCTETS_RD(mmc_rxtcp_err_octets_val);
	sprintf(debugfs_buf, "MMC_RXTCP_ERR_OCTETS       :%#x\n",
		mmc_rxtcp_err_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxtcp_err_octets_fops = {
	.read = mmc_rxtcp_err_octets_read,
	.write = eqos_write,
};

static ssize_t mmc_rxtcp_gd_octets_read(struct file *file,
					char __user *userbuf, size_t count,
					loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXTCP_GD_OCTETS_RD(mmc_rxtcp_gd_octets_val);
	sprintf(debugfs_buf, "MMC_RXTCP_GD_OCTETS        :%#x\n",
		mmc_rxtcp_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxtcp_gd_octets_fops = {
	.read = mmc_rxtcp_gd_octets_read,
	.write = eqos_write,
};

static ssize_t mmc_rxudp_err_octets_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXUDP_ERR_OCTETS_RD(mmc_rxudp_err_octets_val);
	sprintf(debugfs_buf, "MMC_RXUDP_ERR_OCTETS       :%#x\n",
		mmc_rxudp_err_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxudp_err_octets_fops = {
	.read = mmc_rxudp_err_octets_read,
	.write = eqos_write,
};

static ssize_t mmc_rxudp_gd_octets_read(struct file *file,
					char __user *userbuf, size_t count,
					loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXUDP_GD_OCTETS_RD(mmc_rxudp_gd_octets_val);
	sprintf(debugfs_buf, "MMC_RXUDP_GD_OCTETS        :%#x\n",
		mmc_rxudp_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxudp_gd_octets_fops = {
	.read = mmc_rxudp_gd_octets_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV6_nopay_octets_read(struct file *file,
					    char __user *userbuf, size_t count,
					    loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV6_NOPAY_OCTETS_RD(MMC_RXIPV6_nopay_octets_val);
	sprintf(debugfs_buf, "MMC_RXIPV6_NOPAY_OCTETS    :%#x\n",
		MMC_RXIPV6_nopay_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV6_nopay_octets_fops = {
	.read = MMC_RXIPV6_nopay_octets_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV6_hdrerr_octets_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV6_HDRERR_OCTETS_RD(MMC_RXIPV6_hdrerr_octets_val);
	sprintf(debugfs_buf, "MMC_RXIPV6_HDRERR_OCTETS   :%#x\n",
		MMC_RXIPV6_hdrerr_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV6_hdrerr_octets_fops = {
	.read = MMC_RXIPV6_hdrerr_octets_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV6_gd_octets_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV6_GD_OCTETS_RD(MMC_RXIPV6_gd_octets_val);
	sprintf(debugfs_buf, "MMC_RXIPV6_GD_OCTETS       :%#x\n",
		MMC_RXIPV6_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV6_gd_octets_fops = {
	.read = MMC_RXIPV6_gd_octets_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_udsbl_octets_read(struct file *file,
					    char __user *userbuf, size_t count,
					    loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_UDSBL_OCTETS_RD(MMC_RXIPV4_udsbl_octets_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_UDSBL_OCTETS    :%#x\n",
		MMC_RXIPV4_udsbl_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_udsbl_octets_fops = {
	.read = MMC_RXIPV4_udsbl_octets_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_frag_octets_read(struct file *file,
					   char __user *userbuf, size_t count,
					   loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_FRAG_OCTETS_RD(MMC_RXIPV4_frag_octets_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_FRAG_OCTETS     :%#x\n",
		MMC_RXIPV4_frag_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_frag_octets_fops = {
	.read = MMC_RXIPV4_frag_octets_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_nopay_octets_read(struct file *file,
					    char __user *userbuf, size_t count,
					    loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_NOPAY_OCTETS_RD(MMC_RXIPV4_nopay_octets_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_NOPAY_OCTETS    :%#x\n",
		MMC_RXIPV4_nopay_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_nopay_octets_fops = {
	.read = MMC_RXIPV4_nopay_octets_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_hdrerr_octets_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_HDRERR_OCTETS_RD(MMC_RXIPV4_hdrerr_octets_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_HDRERR_OCTETS   :%#x\n",
		MMC_RXIPV4_hdrerr_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_hdrerr_octets_fops = {
	.read = MMC_RXIPV4_hdrerr_octets_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_gd_octets_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_GD_OCTETS_RD(MMC_RXIPV4_gd_octets_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_GD_OCTETS       :%#x\n",
		MMC_RXIPV4_gd_octets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_gd_octets_fops = {
	.read = MMC_RXIPV4_gd_octets_read,
	.write = eqos_write,
};

static ssize_t mmc_rxicmp_err_pkts_read(struct file *file,
					char __user *userbuf, size_t count,
					loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXICMP_ERR_PKTS_RD(mmc_rxicmp_err_pkts_val);
	sprintf(debugfs_buf, "MMC_RXICMP_ERR_PKTS        :%#x\n",
		mmc_rxicmp_err_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxicmp_err_pkts_fops = {
	.read = mmc_rxicmp_err_pkts_read,
	.write = eqos_write,
};

static ssize_t mmc_rxicmp_gd_pkts_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXICMP_GD_PKTS_RD(mmc_rxicmp_gd_pkts_val);
	sprintf(debugfs_buf, "MMC_RXICMP_GD_PKTS         :%#x\n",
		mmc_rxicmp_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxicmp_gd_pkts_fops = {
	.read = mmc_rxicmp_gd_pkts_read,
	.write = eqos_write,
};

static ssize_t mmc_rxtcp_err_pkts_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXTCP_ERR_PKTS_RD(mmc_rxtcp_err_pkts_val);
	sprintf(debugfs_buf, "MMC_RXTCP_ERR_PKTS         :%#x\n",
		mmc_rxtcp_err_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxtcp_err_pkts_fops = {
	.read = mmc_rxtcp_err_pkts_read,
	.write = eqos_write,
};

static ssize_t mmc_rxtcp_gd_pkts_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXTCP_GD_PKTS_RD(mmc_rxtcp_gd_pkts_val);
	sprintf(debugfs_buf, "MMC_RXTCP_GD_PKTS          :%#x\n",
		mmc_rxtcp_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxtcp_gd_pkts_fops = {
	.read = mmc_rxtcp_gd_pkts_read,
	.write = eqos_write,
};

static ssize_t mmc_rxudp_err_pkts_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXUDP_ERR_PKTS_RD(mmc_rxudp_err_pkts_val);
	sprintf(debugfs_buf, "MMC_RXUDP_ERR_PKTS         :%#x\n",
		mmc_rxudp_err_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxudp_err_pkts_fops = {
	.read = mmc_rxudp_err_pkts_read,
	.write = eqos_write,
};

static ssize_t mmc_rxudp_gd_pkts_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXUDP_GD_PKTS_RD(mmc_rxudp_gd_pkts_val);
	sprintf(debugfs_buf, "MMC_RXUDP_GD_PKTS          :%#x\n",
		mmc_rxudp_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxudp_gd_pkts_fops = {
	.read = mmc_rxudp_gd_pkts_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV6_nopay_pkts_read(struct file *file,
					  char __user *userbuf, size_t count,
					  loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV6_NOPAY_PKTS_RD(MMC_RXIPV6_nopay_pkts_val);
	sprintf(debugfs_buf, "MMC_RXIPV6_NOPAY_PKTS      :%#x\n",
		MMC_RXIPV6_nopay_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV6_nopay_pkts_fops = {
	.read = MMC_RXIPV6_nopay_pkts_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV6_hdrerr_pkts_read(struct file *file,
					   char __user *userbuf, size_t count,
					   loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV6_HDRERR_PKTS_RD(MMC_RXIPV6_hdrerr_pkts_val);
	sprintf(debugfs_buf, "MMC_RXIPV6_HDRERR_PKTS     :%#x\n",
		MMC_RXIPV6_hdrerr_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV6_hdrerr_pkts_fops = {
	.read = MMC_RXIPV6_hdrerr_pkts_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV6_gd_pkts_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV6_GD_PKTS_RD(MMC_RXIPV6_gd_pkts_val);
	sprintf(debugfs_buf, "MMC_RXIPV6_GD_PKTS         :%#x\n",
		MMC_RXIPV6_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV6_gd_pkts_fops = {
	.read = MMC_RXIPV6_gd_pkts_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_ubsbl_pkts_read(struct file *file,
					  char __user *userbuf, size_t count,
					  loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_UBSBL_PKTS_RD(MMC_RXIPV4_ubsbl_pkts_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_UBSBL_PKTS      :%#x\n",
		MMC_RXIPV4_ubsbl_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_ubsbl_pkts_fops = {
	.read = MMC_RXIPV4_ubsbl_pkts_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_frag_pkts_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_FRAG_PKTS_RD(MMC_RXIPV4_frag_pkts_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_FRAG_PKTS       :%#x\n",
		MMC_RXIPV4_frag_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_frag_pkts_fops = {
	.read = MMC_RXIPV4_frag_pkts_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_nopay_pkts_read(struct file *file,
					  char __user *userbuf, size_t count,
					  loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_NOPAY_PKTS_RD(MMC_RXIPV4_nopay_pkts_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_NOPAY_PKTS      :%#x\n",
		MMC_RXIPV4_nopay_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_nopay_pkts_fops = {
	.read = MMC_RXIPV4_nopay_pkts_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_hdrerr_pkts_read(struct file *file,
					   char __user *userbuf, size_t count,
					   loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_HDRERR_PKTS_RD(MMC_RXIPV4_hdrerr_pkts_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_HDRERR_PKTS     :%#x\n",
		MMC_RXIPV4_hdrerr_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_hdrerr_pkts_fops = {
	.read = MMC_RXIPV4_hdrerr_pkts_read,
	.write = eqos_write,
};

static ssize_t MMC_RXIPV4_gd_pkts_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXIPV4_GD_PKTS_RD(MMC_RXIPV4_gd_pkts_val);
	sprintf(debugfs_buf, "MMC_RXIPV4_GD_PKTS         :%#x\n",
		MMC_RXIPV4_gd_pkts_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RXIPV4_gd_pkts_fops = {
	.read = MMC_RXIPV4_gd_pkts_read,
	.write = eqos_write,
};

static ssize_t mmc_rxctrlpackets_g_read(struct file *file,
					char __user *userbuf, size_t count,
					loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXCTRLPACKETS_G_RD(mmc_rxctrlpackets_g_val);
	sprintf(debugfs_buf, "MMC_RXCTRLPACKETS_G        :%#x\n",
		mmc_rxctrlpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxctrlpackets_g_fops = {
	.read = mmc_rxctrlpackets_g_read,
	.write = eqos_write,
};

static ssize_t mmc_rxrcverror_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXRCVERROR_RD(mmc_rxrcverror_val);
	sprintf(debugfs_buf, "MMC_RXRCVERROR             :%#x\n",
		mmc_rxrcverror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxrcverror_fops = {
	.read = mmc_rxrcverror_read,
	.write = eqos_write,
};

static ssize_t mmc_rxwatchdogerror_read(struct file *file,
					char __user *userbuf, size_t count,
					loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXWATCHDOGERROR_RD(mmc_rxwatchdogerror_val);
	sprintf(debugfs_buf, "MMC_RXWATCHDOGERROR        :%#x\n",
		mmc_rxwatchdogerror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxwatchdogerror_fops = {
	.read = mmc_rxwatchdogerror_read,
	.write = eqos_write,
};

static ssize_t mmc_rxvlanpackets_gb_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXVLANPACKETS_GB_RD(mmc_rxvlanpackets_gb_val);
	sprintf(debugfs_buf, "MMC_RXVLANPACKETS_GB       :%#x\n",
		mmc_rxvlanpackets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxvlanpackets_gb_fops = {
	.read = mmc_rxvlanpackets_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_rxfifooverflow_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXFIFOOVERFLOW_RD(mmc_rxfifooverflow_val);
	sprintf(debugfs_buf, "MMC_RXFIFOOVERFLOW         :%#x\n",
		mmc_rxfifooverflow_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxfifooverflow_fops = {
	.read = mmc_rxfifooverflow_read,
	.write = eqos_write,
};

static ssize_t mmc_rxpausepackets_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXPAUSEPACKETS_RD(mmc_rxpausepackets_val);
	sprintf(debugfs_buf, "MMC_RXPAUSEPACKETS         :%#x\n",
		mmc_rxpausepackets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxpausepackets_fops = {
	.read = mmc_rxpausepackets_read,
	.write = eqos_write,
};

static ssize_t mmc_rxoutofrangetype_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXOUTOFRANGETYPE_RD(mmc_rxoutofrangetype_val);
	sprintf(debugfs_buf, "MMC_RXOUTOFRANGETYPE       :%#x\n",
		mmc_rxoutofrangetype_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxoutofrangetype_fops = {
	.read = mmc_rxoutofrangetype_read,
	.write = eqos_write,
};

static ssize_t mmc_rxlengtherror_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXLENGTHERROR_RD(mmc_rxlengtherror_val);
	sprintf(debugfs_buf, "MMC_RXLENGTHERROR          :%#x\n",
		mmc_rxlengtherror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxlengtherror_fops = {
	.read = mmc_rxlengtherror_read,
	.write = eqos_write,
};

static ssize_t mmc_rxunicastpackets_g_read(struct file *file,
					   char __user *userbuf, size_t count,
					   loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXUNICASTPACKETS_G_RD(mmc_rxunicastpackets_g_val);
	sprintf(debugfs_buf, "MMC_RXUNICASTPACKETS_G     :%#x\n",
		mmc_rxunicastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxunicastpackets_g_fops = {
	.read = mmc_rxunicastpackets_g_read,
	.write = eqos_write,
};

static ssize_t MMC_RX1024tomaxoctets_gb_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RX1024TOMAXOCTETS_GB_RD(MMC_RX1024tomaxoctets_gb_val);
	sprintf(debugfs_buf, "MMC_RX1024TOMAXOCTETS_GB   :%#x\n",
		MMC_RX1024tomaxoctets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RX1024tomaxoctets_gb_fops = {
	.read = MMC_RX1024tomaxoctets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_RX512TO1023octets_gb_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RX512TO1023OCTETS_GB_RD(MMC_RX512TO1023octets_gb_val);
	sprintf(debugfs_buf, "MMC_RX512TO1023OCTETS_GB   :%#x\n",
		MMC_RX512TO1023octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RX512TO1023octets_gb_fops = {
	.read = MMC_RX512TO1023octets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_RX256TO511octets_gb_read(struct file *file,
					    char __user *userbuf, size_t count,
					    loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RX256TO511OCTETS_GB_RD(MMC_RX256TO511octets_gb_val);
	sprintf(debugfs_buf, "MMC_RX256TO511OCTETS_GB    :%#x\n",
		MMC_RX256TO511octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RX256TO511octets_gb_fops = {
	.read = MMC_RX256TO511octets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_RX128TO255octets_gb_read(struct file *file,
					    char __user *userbuf, size_t count,
					    loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RX128TO255OCTETS_GB_RD(MMC_RX128TO255octets_gb_val);
	sprintf(debugfs_buf, "MMC_RX128TO255OCTETS_GB    :%#x\n",
		MMC_RX128TO255octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RX128TO255octets_gb_fops = {
	.read = MMC_RX128TO255octets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_RX65TO127octets_gb_read(struct file *file,
					   char __user *userbuf, size_t count,
					   loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RX65TO127OCTETS_GB_RD(MMC_RX65TO127octets_gb_val);
	sprintf(debugfs_buf, "MMC_RX65TO127OCTETS_GB     :%#x\n",
		MMC_RX65TO127octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RX65TO127octets_gb_fops = {
	.read = MMC_RX65TO127octets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_RX64octets_gb_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RX64OCTETS_GB_RD(MMC_RX64octets_gb_val);
	sprintf(debugfs_buf, "MMC_RX64OCTETS_GB          :%#x\n",
		MMC_RX64octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_RX64octets_gb_fops = {
	.read = MMC_RX64octets_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_rxoversize_g_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXOVERSIZE_G_RD(mmc_rxoversize_g_val);
	sprintf(debugfs_buf, "MMC_RXOVERSIZE_G           :%#x\n",
		mmc_rxoversize_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxoversize_g_fops = {
	.read = mmc_rxoversize_g_read,
	.write = eqos_write,
};

static ssize_t mmc_rxundersize_g_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXUNDERSIZE_G_RD(mmc_rxundersize_g_val);
	sprintf(debugfs_buf, "MMC_RXUNDERSIZE_G          :%#x\n",
		mmc_rxundersize_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxundersize_g_fops = {
	.read = mmc_rxundersize_g_read,
	.write = eqos_write,
};

static ssize_t mmc_rxjabbererror_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXJABBERERROR_RD(mmc_rxjabbererror_val);
	sprintf(debugfs_buf, "MMC_RXJABBERERROR          :%#x\n",
		mmc_rxjabbererror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxjabbererror_fops = {
	.read = mmc_rxjabbererror_read,
	.write = eqos_write,
};

static ssize_t mmc_rxrunterror_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXRUNTERROR_RD(mmc_rxrunterror_val);
	sprintf(debugfs_buf, "MMC_RXRUNTERROR            :%#x\n",
		mmc_rxrunterror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxrunterror_fops = {
	.read = mmc_rxrunterror_read,
	.write = eqos_write,
};

static ssize_t mmc_rxalignmenterror_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXALIGNMENTERROR_RD(mmc_rxalignmenterror_val);
	sprintf(debugfs_buf, "MMC_RXALIGNMENTERROR       :%#x\n",
		mmc_rxalignmenterror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxalignmenterror_fops = {
	.read = mmc_rxalignmenterror_read,
	.write = eqos_write,
};

static ssize_t mmc_rxcrcerror_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXCRCERROR_RD(mmc_rxcrcerror_val);
	sprintf(debugfs_buf, "MMC_RXCRCERROR             :%#x\n",
		mmc_rxcrcerror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxcrcerror_fops = {
	.read = mmc_rxcrcerror_read,
	.write = eqos_write,
};

static ssize_t mmc_rxmulticastpackets_g_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXMULTICASTPACKETS_G_RD(mmc_rxmulticastpackets_g_val);
	sprintf(debugfs_buf, "MMC_RXMULTICASTPACKETS_G   :%#x\n",
		mmc_rxmulticastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxmulticastpackets_g_fops = {
	.read = mmc_rxmulticastpackets_g_read,
	.write = eqos_write,
};

static ssize_t mmc_rxbroadcastpackets_g_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXBROADCASTPACKETS_G_RD(mmc_rxbroadcastpackets_g_val);
	sprintf(debugfs_buf, "MMC_RXBROADCASTPACKETS_G   :%#x\n",
		mmc_rxbroadcastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxbroadcastpackets_g_fops = {
	.read = mmc_rxbroadcastpackets_g_read,
	.write = eqos_write,
};

static ssize_t mmc_rxoctetcount_g_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXOCTETCOUNT_G_RD(mmc_rxoctetcount_g_val);
	sprintf(debugfs_buf, "MMC_RXOCTETCOUNT_G         :%#x\n",
		mmc_rxoctetcount_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxoctetcount_g_fops = {
	.read = mmc_rxoctetcount_g_read,
	.write = eqos_write,
};

static ssize_t mmc_rxoctetcount_gb_read(struct file *file,
					char __user *userbuf, size_t count,
					loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXOCTETCOUNT_GB_RD(mmc_rxoctetcount_gb_val);
	sprintf(debugfs_buf, "MMC_RXOCTETCOUNT_GB        :%#x\n",
		mmc_rxoctetcount_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxoctetcount_gb_fops = {
	.read = mmc_rxoctetcount_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_rxpacketcount_gb_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_RXPACKETCOUNT_GB_RD(mmc_rxpacketcount_gb_val);
	sprintf(debugfs_buf, "MMC_RXPACKETCOUNT_GB       :%#x\n",
		mmc_rxpacketcount_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_rxpacketcount_gb_fops = {
	.read = mmc_rxpacketcount_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_txoversize_g_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXOVERSIZE_G_RD(mmc_txoversize_g_val);
	sprintf(debugfs_buf, "MMC_TXOVERSIZE_G           :%#x\n",
		mmc_txoversize_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txoversize_g_fops = {
	.read = mmc_txoversize_g_read,
	.write = eqos_write,
};

static ssize_t mmc_txvlanpackets_g_read(struct file *file,
					char __user *userbuf, size_t count,
					loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXVLANPACKETS_G_RD(mmc_txvlanpackets_g_val);
	sprintf(debugfs_buf, "MMC_TXVLANPACKETS_G        :%#x\n",
		mmc_txvlanpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txvlanpackets_g_fops = {
	.read = mmc_txvlanpackets_g_read,
	.write = eqos_write,
};

static ssize_t mmc_txpausepackets_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXPAUSEPACKETS_RD(mmc_txpausepackets_val);
	sprintf(debugfs_buf, "MMC_TXPAUSEPACKETS         :%#x\n",
		mmc_txpausepackets_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txpausepackets_fops = {
	.read = mmc_txpausepackets_read,
	.write = eqos_write,
};

static ssize_t mmc_txexcessdef_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXEXCESSDEF_RD(mmc_txexcessdef_val);
	sprintf(debugfs_buf, "MMC_TXEXCESSDEF            :%#x\n",
		mmc_txexcessdef_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txexcessdef_fops = {
	.read = mmc_txexcessdef_read,
	.write = eqos_write,
};

static ssize_t mmc_txpacketscount_g_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXPACKETSCOUNT_G_RD(mmc_txpacketscount_g_val);
	sprintf(debugfs_buf, "MMC_TXPACKETSCOUNT_G       :%#x\n",
		mmc_txpacketscount_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txpacketscount_g_fops = {
	.read = mmc_txpacketscount_g_read,
	.write = eqos_write,
};

static ssize_t mmc_txoctetcount_g_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXOCTETCOUNT_G_RD(mmc_txoctetcount_g_val);
	sprintf(debugfs_buf, "MMC_TXOCTETCOUNT_G         :%#x\n",
		mmc_txoctetcount_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txoctetcount_g_fops = {
	.read = mmc_txoctetcount_g_read,
	.write = eqos_write,
};

static ssize_t mmc_txcarriererror_read(struct file *file, char __user *userbuf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXCARRIERERROR_RD(mmc_txcarriererror_val);
	sprintf(debugfs_buf, "MMC_TXCARRIERERROR         :%#x\n",
		mmc_txcarriererror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txcarriererror_fops = {
	.read = mmc_txcarriererror_read,
	.write = eqos_write,
};

static ssize_t mmc_txexesscol_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXEXESSCOL_RD(mmc_txexesscol_val);
	sprintf(debugfs_buf, "MMC_TXEXESSCOL             :%#x\n",
		mmc_txexesscol_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txexesscol_fops = {
	.read = mmc_txexesscol_read,
	.write = eqos_write,
};

static ssize_t mmc_txlatecol_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXLATECOL_RD(mmc_txlatecol_val);
	sprintf(debugfs_buf, "MMC_TXLATECOL              :%#x\n",
		mmc_txlatecol_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txlatecol_fops = {
	.read = mmc_txlatecol_read,
	.write = eqos_write,
};

static ssize_t mmc_txdeferred_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXDEFERRED_RD(mmc_txdeferred_val);
	sprintf(debugfs_buf, "MMC_TXDEFERRED             :%#x\n",
		mmc_txdeferred_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txdeferred_fops = {
	.read = mmc_txdeferred_read,
	.write = eqos_write,
};

static ssize_t mmc_txmulticol_g_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXMULTICOL_G_RD(mmc_txmulticol_g_val);
	sprintf(debugfs_buf, "MMC_TXMULTICOL_G           :%#x\n",
		mmc_txmulticol_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txmulticol_g_fops = {
	.read = mmc_txmulticol_g_read,
	.write = eqos_write,
};

static ssize_t mmc_txsinglecol_g_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXSINGLECOL_G_RD(mmc_txsinglecol_g_val);
	sprintf(debugfs_buf, "MMC_TXSINGLECOL_G          :%#x\n",
		mmc_txsinglecol_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txsinglecol_g_fops = {
	.read = mmc_txsinglecol_g_read,
	.write = eqos_write,
};

static ssize_t mmc_txunderflowerror_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXUNDERFLOWERROR_RD(mmc_txunderflowerror_val);
	sprintf(debugfs_buf, "MMC_TXUNDERFLOWERROR       :%#x\n",
		mmc_txunderflowerror_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txunderflowerror_fops = {
	.read = mmc_txunderflowerror_read,
	.write = eqos_write,
};

static ssize_t mmc_txbroadcastpackets_gb_read(struct file *file,
					      char __user *userbuf,
					      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXBROADCASTPACKETS_GB_RD(mmc_txbroadcastpackets_gb_val);
	sprintf(debugfs_buf, "MMC_TXBROADCASTPACKETS_GB  :%#x\n",
		mmc_txbroadcastpackets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txbroadcastpackets_gb_fops = {
	.read = mmc_txbroadcastpackets_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_txmulticastpackets_gb_read(struct file *file,
					      char __user *userbuf,
					      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXMULTICASTPACKETS_GB_RD(mmc_txmulticastpackets_gb_val);
	sprintf(debugfs_buf, "MMC_TXMULTICASTPACKETS_GB  :%#x\n",
		mmc_txmulticastpackets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txmulticastpackets_gb_fops = {
	.read = mmc_txmulticastpackets_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_txunicastpackets_gb_read(struct file *file,
					    char __user *userbuf, size_t count,
					    loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXUNICASTPACKETS_GB_RD(mmc_txunicastpackets_gb_val);
	sprintf(debugfs_buf, "MMC_TXUNICASTPACKETS_GB    :%#x\n",
		mmc_txunicastpackets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txunicastpackets_gb_fops = {
	.read = mmc_txunicastpackets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_TX1024tomaxoctets_gb_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TX1024TOMAXOCTETS_GB_RD(MMC_TX1024tomaxoctets_gb_val);
	sprintf(debugfs_buf, "MMC_TX1024TOMAXOCTETS_GB   :%#x\n",
		MMC_TX1024tomaxoctets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_TX1024tomaxoctets_gb_fops = {
	.read = MMC_TX1024tomaxoctets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_TX512TO1023octets_gb_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TX512TO1023OCTETS_GB_RD(MMC_TX512TO1023octets_gb_val);
	sprintf(debugfs_buf, "MMC_TX512TO1023OCTETS_GB   :%#x\n",
		MMC_TX512TO1023octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_TX512TO1023octets_gb_fops = {
	.read = MMC_TX512TO1023octets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_TX256TO511octets_gb_read(struct file *file,
					    char __user *userbuf, size_t count,
					    loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TX256TO511OCTETS_GB_RD(MMC_TX256TO511octets_gb_val);
	sprintf(debugfs_buf, "MMC_TX256TO511OCTETS_GB    :%#x\n",
		MMC_TX256TO511octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_TX256TO511octets_gb_fops = {
	.read = MMC_TX256TO511octets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_TX128TO255octets_gb_read(struct file *file,
					    char __user *userbuf, size_t count,
					    loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TX128TO255OCTETS_GB_RD(MMC_TX128TO255octets_gb_val);
	sprintf(debugfs_buf, "MMC_TX128TO255OCTETS_GB    :%#x\n",
		MMC_TX128TO255octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_TX128TO255octets_gb_fops = {
	.read = MMC_TX128TO255octets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_TX65TO127octets_gb_read(struct file *file,
					   char __user *userbuf, size_t count,
					   loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TX65TO127OCTETS_GB_RD(MMC_TX65TO127octets_gb_val);
	sprintf(debugfs_buf, "MMC_TX65TO127OCTETS_GB     :%#x\n",
		MMC_TX65TO127octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_TX65TO127octets_gb_fops = {
	.read = MMC_TX65TO127octets_gb_read,
	.write = eqos_write,
};

static ssize_t MMC_TX64octets_gb_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TX64OCTETS_GB_RD(MMC_TX64octets_gb_val);
	sprintf(debugfs_buf, "MMC_TX64OCTETS_GB          :%#x\n",
		MMC_TX64octets_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MMC_TX64octets_gb_fops = {
	.read = MMC_TX64octets_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_txmulticastpackets_g_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXMULTICASTPACKETS_G_RD(mmc_txmulticastpackets_g_val);
	sprintf(debugfs_buf, "MMC_TXMULTICASTPACKETS_G   :%#x\n",
		mmc_txmulticastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txmulticastpackets_g_fops = {
	.read = mmc_txmulticastpackets_g_read,
	.write = eqos_write,
};

static ssize_t mmc_txbroadcastpackets_g_read(struct file *file,
					     char __user *userbuf,
					     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXBROADCASTPACKETS_G_RD(mmc_txbroadcastpackets_g_val);
	sprintf(debugfs_buf, "MMC_TXBROADCASTPACKETS_G   :%#x\n",
		mmc_txbroadcastpackets_g_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txbroadcastpackets_g_fops = {
	.read = mmc_txbroadcastpackets_g_read,
	.write = eqos_write,
};

static ssize_t mmc_txpacketcount_gb_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXPACKETCOUNT_GB_RD(mmc_txpacketcount_gb_val);
	sprintf(debugfs_buf, "MMC_TXPACKETCOUNT_GB       :%#x\n",
		mmc_txpacketcount_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txpacketcount_gb_fops = {
	.read = mmc_txpacketcount_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_txoctetcount_gb_read(struct file *file,
					char __user *userbuf, size_t count,
					loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_TXOCTETCOUNT_GB_RD(mmc_txoctetcount_gb_val);
	sprintf(debugfs_buf, "MMC_TXOCTETCOUNT_GB        :%#x\n",
		mmc_txoctetcount_gb_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_txoctetcount_gb_fops = {
	.read = mmc_txoctetcount_gb_read,
	.write = eqos_write,
};

static ssize_t mmc_ipc_intr_rx_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_IPC_INTR_RX_RD(mmc_ipc_intr_rx_val);
	sprintf(debugfs_buf, "MMC_IPC_INTR_RX            :%#x\n",
		mmc_ipc_intr_rx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_ipc_intr_rx_fops = {
	.read = mmc_ipc_intr_rx_read,
	.write = eqos_write,
};

static ssize_t mmc_ipc_intr_mask_rx_read(struct file *file,
					 char __user *userbuf, size_t count,
					 loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_IPC_INTR_MASK_RX_RD(mmc_ipc_intr_mask_rx_val);
	sprintf(debugfs_buf, "MMC_IPC_INTR_MASK_RX       :%#x\n",
		mmc_ipc_intr_mask_rx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_ipc_intr_mask_rx_fops = {
	.read = mmc_ipc_intr_mask_rx_read,
	.write = eqos_write,
};

static ssize_t mmc_intr_mask_tx_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_INTR_MASK_TX_RD(mmc_intr_mask_tx_val);
	sprintf(debugfs_buf, "MMC_INTR_MASK_TX           :%#x\n",
		mmc_intr_mask_tx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_intr_mask_tx_fops = {
	.read = mmc_intr_mask_tx_read,
	.write = eqos_write,
};

static ssize_t mmc_intr_mask_rx_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_INTR_MASK_RX_RD(mmc_intr_mask_rx_val);
	sprintf(debugfs_buf, "MMC_INTR_MASK_RX           :%#x\n",
		mmc_intr_mask_rx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_intr_mask_rx_fops = {
	.read = mmc_intr_mask_rx_read,
	.write = eqos_write,
};

static ssize_t mmc_intr_tx_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_INTR_TX_RD(mmc_intr_tx_val);
	sprintf(debugfs_buf, "MMC_INTR_TX                :%#x\n",
		mmc_intr_tx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_intr_tx_fops = {
	.read = mmc_intr_tx_read,
	.write = eqos_write,
};

static ssize_t mmc_intr_rx_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_INTR_RX_RD(mmc_intr_rx_val);
	sprintf(debugfs_buf, "MMC_INTR_RX                :%#x\n",
		mmc_intr_rx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_intr_rx_fops = {
	.read = mmc_intr_rx_read,
	.write = eqos_write,
};

static ssize_t mmc_cntrl_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;


	if (!pdata->hw_feat.mmc_sel) {
		pr_err(
		       "MMC Module not selected. Register cannot be read\n");
		return -EINVAL;
	}
	MMC_CNTRL_RD(mmc_cntrl_val);
	sprintf(debugfs_buf, "MMC_CNTRL                  :%#x\n",
		mmc_cntrl_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mmc_cntrl_fops = {
	.read = mmc_cntrl_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1lr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1LR_RD(MAC_MA1lr_val);
	sprintf(debugfs_buf, "MAC_MA1LR                  :%#x\n",
		MAC_MA1lr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1lr_fops = {
	.read = MAC_MA1lr_read,
	.write = eqos_write,
};

static ssize_t MAC_MA1hr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA1HR_RD(MAC_MA1hr_val);
	sprintf(debugfs_buf, "MAC_MA1HR                  :%#x\n",
		MAC_MA1hr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA1hr_fops = {
	.read = MAC_MA1hr_read,
	.write = eqos_write,
};

static ssize_t MAC_MA0lr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA0LR_RD(MAC_MA0lr_val);
	sprintf(debugfs_buf, "MAC_MA0LR                  :%#x\n",
		MAC_MA0lr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA0lr_fops = {
	.read = MAC_MA0lr_read,
	.write = eqos_write,
};

static ssize_t MAC_MA0hr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MA0HR_RD(MAC_MA0hr_val);
	sprintf(debugfs_buf, "MAC_MA0HR       :%#x\n", MAC_MA0hr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_MA0hr_fops = {
	.read = MAC_MA0hr_read,
	.write = eqos_write,
};

static ssize_t mac_gpior_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_GPIOR_RD(mac_gpior_val);
	sprintf(debugfs_buf, "MAC_GPIOR       :%#x\n", mac_gpior_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_gpior_fops = {
	.read = mac_gpior_read,
	.write = eqos_write,
};

static ssize_t mac_gmiidr_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_GMIIDR_RD(mac_gmiidr_val);
	sprintf(debugfs_buf, "MAC_GMIIDR      :%#x\n", mac_gmiidr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_gmiidr_fops = {
	.read = mac_gmiidr_read,
	.write = eqos_write,
};

static ssize_t mac_gmiiar_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_GMIIAR_RD(mac_gmiiar_val);
	sprintf(debugfs_buf, "MAC_GMIIAR      :%#x\n", mac_gmiiar_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_gmiiar_fops = {
	.read = mac_gmiiar_read,
	.write = eqos_write,
};

static ssize_t MAC_HFR2_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HFR2_RD(MAC_HFR2_val);
	sprintf(debugfs_buf, "MAC_HFR2        :%#x\n", MAC_HFR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HFR2_fops = {
	.read = MAC_HFR2_read,
	.write = eqos_write,
};

static ssize_t MAC_HFR1_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HFR1_RD(MAC_HFR1_val);
	sprintf(debugfs_buf, "MAC_HFR1        :%#x\n", MAC_HFR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HFR1_fops = {
	.read = MAC_HFR1_read,
	.write = eqos_write,
};

static ssize_t MAC_HFR0_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HFR0_RD(MAC_HFR0_val);
	sprintf(debugfs_buf, "MAC_HFR0        :%#x\n", MAC_HFR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HFR0_fops = {
	.read = MAC_HFR0_read,
	.write = eqos_write,
};

static ssize_t mac_mdr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MDR_RD(mac_mdr_val);
	sprintf(debugfs_buf, "MAC_MDR         :%#x\n", mac_mdr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_mdr_fops = {
	.read = mac_mdr_read,
	.write = eqos_write,
};

static ssize_t mac_vr_read(struct file *file, char __user *userbuf,
			   size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_VR_RD(mac_vr_val);
	sprintf(debugfs_buf, "MAC_VR          :%#x\n", mac_vr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_vr_fops = {
	.read = mac_vr_read,
	.write = eqos_write,
};

static ssize_t MAC_HTR7_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HTR7_RD(MAC_HTR7_val);
	sprintf(debugfs_buf, "MAC_HTR7        :%#x\n", MAC_HTR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HTR7_fops = {
	.read = MAC_HTR7_read,
	.write = eqos_write,
};

static ssize_t MAC_HTR6_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HTR6_RD(MAC_HTR6_val);
	sprintf(debugfs_buf, "MAC_HTR6        :%#x\n", MAC_HTR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HTR6_fops = {
	.read = MAC_HTR6_read,
	.write = eqos_write,
};

static ssize_t MAC_HTR5_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HTR5_RD(MAC_HTR5_val);
	sprintf(debugfs_buf, "MAC_HTR5        :%#x\n", MAC_HTR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HTR5_fops = {
	.read = MAC_HTR5_read,
	.write = eqos_write,
};

static ssize_t MAC_HTR4_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HTR4_RD(MAC_HTR4_val);
	sprintf(debugfs_buf, "MAC_HTR4        :%#x\n", MAC_HTR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HTR4_fops = {
	.read = MAC_HTR4_read,
	.write = eqos_write,
};

static ssize_t MAC_HTR3_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HTR3_RD(MAC_HTR3_val);
	sprintf(debugfs_buf, "MAC_HTR3        :%#x\n", MAC_HTR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HTR3_fops = {
	.read = MAC_HTR3_read,
	.write = eqos_write,
};

static ssize_t MAC_HTR2_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HTR2_RD(MAC_HTR2_val);
	sprintf(debugfs_buf, "MAC_HTR2        :%#x\n", MAC_HTR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HTR2_fops = {
	.read = MAC_HTR2_read,
	.write = eqos_write,
};

static ssize_t MAC_HTR1_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HTR1_RD(MAC_HTR1_val);
	sprintf(debugfs_buf, "MAC_HTR1        :%#x\n", MAC_HTR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HTR1_fops = {
	.read = MAC_HTR1_read,
	.write = eqos_write,
};

static ssize_t MAC_HTR0_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_HTR0_RD(MAC_HTR0_val);
	sprintf(debugfs_buf, "MAC_HTR0        :%#x\n", MAC_HTR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_HTR0_fops = {
	.read = MAC_HTR0_read,
	.write = eqos_write,
};

static ssize_t DMA_RIWTR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RIWTR7_RD(DMA_RIWTR7_val);
	sprintf(debugfs_buf, "DMA_RIWTR7      :%#x\n", DMA_RIWTR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RIWTR7_fops = {
	.read = DMA_RIWTR7_read,
	.write = eqos_write,
};

static ssize_t DMA_RIWTR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RIWTR6_RD(DMA_RIWTR6_val);
	sprintf(debugfs_buf, "DMA_RIWTR6      :%#x\n", DMA_RIWTR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RIWTR6_fops = {
	.read = DMA_RIWTR6_read,
	.write = eqos_write,
};

static ssize_t DMA_RIWTR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RIWTR5_RD(DMA_RIWTR5_val);
	sprintf(debugfs_buf, "DMA_RIWTR5      :%#x\n", DMA_RIWTR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RIWTR5_fops = {
	.read = DMA_RIWTR5_read,
	.write = eqos_write,
};

static ssize_t DMA_RIWTR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RIWTR4_RD(DMA_RIWTR4_val);
	sprintf(debugfs_buf, "DMA_RIWTR4      :%#x\n", DMA_RIWTR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RIWTR4_fops = {
	.read = DMA_RIWTR4_read,
	.write = eqos_write,
};

static ssize_t DMA_RIWTR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RIWTR3_RD(DMA_RIWTR3_val);
	sprintf(debugfs_buf, "DMA_RIWTR3      :%#x\n", DMA_RIWTR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RIWTR3_fops = {
	.read = DMA_RIWTR3_read,
	.write = eqos_write,
};

static ssize_t DMA_RIWTR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RIWTR2_RD(DMA_RIWTR2_val);
	sprintf(debugfs_buf, "DMA_RIWTR2      :%#x\n", DMA_RIWTR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RIWTR2_fops = {
	.read = DMA_RIWTR2_read,
	.write = eqos_write,
};

static ssize_t DMA_RIWTR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RIWTR1_RD(DMA_RIWTR1_val);
	sprintf(debugfs_buf, "DMA_RIWTR1      :%#x\n", DMA_RIWTR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RIWTR1_fops = {
	.read = DMA_RIWTR1_read,
	.write = eqos_write,
};

static ssize_t DMA_RIWTR0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RIWTR0_RD(DMA_RIWTR0_val);
	sprintf(debugfs_buf, "DMA_RIWTR0      :%#x\n", DMA_RIWTR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RIWTR0_fops = {
	.read = DMA_RIWTR0_read,
	.write = eqos_write,
};

static ssize_t DMA_RDRLR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDRLR7_RD(DMA_RDRLR7_val);
	sprintf(debugfs_buf, "DMA_RDRLR7      :%#x\n", DMA_RDRLR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDRLR7_fops = {
	.read = DMA_RDRLR7_read,
	.write = eqos_write,
};

static ssize_t DMA_RDRLR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDRLR6_RD(DMA_RDRLR6_val);
	sprintf(debugfs_buf, "DMA_RDRLR6      :%#x\n", DMA_RDRLR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDRLR6_fops = {
	.read = DMA_RDRLR6_read,
	.write = eqos_write,
};

static ssize_t DMA_RDRLR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDRLR5_RD(DMA_RDRLR5_val);
	sprintf(debugfs_buf, "DMA_RDRLR5      :%#x\n", DMA_RDRLR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDRLR5_fops = {
	.read = DMA_RDRLR5_read,
	.write = eqos_write,
};

static ssize_t DMA_RDRLR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDRLR4_RD(DMA_RDRLR4_val);
	sprintf(debugfs_buf, "DMA_RDRLR4      :%#x\n", DMA_RDRLR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDRLR4_fops = {
	.read = DMA_RDRLR4_read,
	.write = eqos_write,
};

static ssize_t DMA_RDRLR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDRLR3_RD(DMA_RDRLR3_val);
	sprintf(debugfs_buf, "DMA_RDRLR3      :%#x\n", DMA_RDRLR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDRLR3_fops = {
	.read = DMA_RDRLR3_read,
	.write = eqos_write,
};

static ssize_t DMA_RDRLR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDRLR2_RD(DMA_RDRLR2_val);
	sprintf(debugfs_buf, "DMA_RDRLR2      :%#x\n", DMA_RDRLR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDRLR2_fops = {
	.read = DMA_RDRLR2_read,
	.write = eqos_write,
};

static ssize_t DMA_RDRLR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDRLR1_RD(DMA_RDRLR1_val);
	sprintf(debugfs_buf, "DMA_RDRLR1      :%#x\n", DMA_RDRLR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDRLR1_fops = {
	.read = DMA_RDRLR1_read,
	.write = eqos_write,
};

static ssize_t DMA_RDRLR0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDRLR0_RD(DMA_RDRLR0_val);
	sprintf(debugfs_buf, "DMA_RDRLR0      :%#x\n", DMA_RDRLR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDRLR0_fops = {
	.read = DMA_RDRLR0_read,
	.write = eqos_write,
};

static ssize_t DMA_TDRLR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDRLR7_RD(DMA_TDRLR7_val);
	sprintf(debugfs_buf, "DMA_TDRLR7      :%#x\n", DMA_TDRLR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDRLR7_fops = {
	.read = DMA_TDRLR7_read,
	.write = eqos_write,
};

static ssize_t DMA_TDRLR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDRLR6_RD(DMA_TDRLR6_val);
	sprintf(debugfs_buf, "DMA_TDRLR6      :%#x\n", DMA_TDRLR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDRLR6_fops = {
	.read = DMA_TDRLR6_read,
	.write = eqos_write,
};

static ssize_t DMA_TDRLR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDRLR5_RD(DMA_TDRLR5_val);
	sprintf(debugfs_buf, "DMA_TDRLR5      :%#x\n", DMA_TDRLR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDRLR5_fops = {
	.read = DMA_TDRLR5_read,
	.write = eqos_write,
};

static ssize_t DMA_TDRLR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDRLR4_RD(DMA_TDRLR4_val);
	sprintf(debugfs_buf, "DMA_TDRLR4      :%#x\n", DMA_TDRLR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDRLR4_fops = {
	.read = DMA_TDRLR4_read,
	.write = eqos_write,
};

static ssize_t DMA_TDRLR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDRLR3_RD(DMA_TDRLR3_val);
	sprintf(debugfs_buf, "DMA_TDRLR3      :%#x\n", DMA_TDRLR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDRLR3_fops = {
	.read = DMA_TDRLR3_read,
	.write = eqos_write,
};

static ssize_t DMA_TDRLR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDRLR2_RD(DMA_TDRLR2_val);
	sprintf(debugfs_buf, "DMA_TDRLR2      :%#x\n", DMA_TDRLR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDRLR2_fops = {
	.read = DMA_TDRLR2_read,
	.write = eqos_write,
};

static ssize_t DMA_TDRLR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDRLR1_RD(DMA_TDRLR1_val);
	sprintf(debugfs_buf, "DMA_TDRLR1      :%#x\n", DMA_TDRLR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDRLR1_fops = {
	.read = DMA_TDRLR1_read,
	.write = eqos_write,
};

static ssize_t DMA_TDRLR0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDRLR0_RD(DMA_TDRLR0_val);
	sprintf(debugfs_buf, "DMA_TDRLR0      :%#x\n", DMA_TDRLR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDRLR0_fops = {
	.read = DMA_TDRLR0_read,
	.write = eqos_write,
};

static ssize_t DMA_RDTP_RPDR7_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDTP_RPDR7_RD(DMA_RDTP_RPDR7_val);
	sprintf(debugfs_buf, "DMA_RDTP_RPDR7  :%#x\n", DMA_RDTP_RPDR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDTP_RPDR7_fops = {
	.read = DMA_RDTP_RPDR7_read,
	.write = eqos_write,
};

static ssize_t DMA_RDTP_RPDR6_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDTP_RPDR6_RD(DMA_RDTP_RPDR6_val);
	sprintf(debugfs_buf, "DMA_RDTP_RPDR6  :%#x\n", DMA_RDTP_RPDR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDTP_RPDR6_fops = {
	.read = DMA_RDTP_RPDR6_read,
	.write = eqos_write,
};

static ssize_t DMA_RDTP_RPDR5_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDTP_RPDR5_RD(DMA_RDTP_RPDR5_val);
	sprintf(debugfs_buf, "DMA_RDTP_RPDR5  :%#x\n", DMA_RDTP_RPDR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDTP_RPDR5_fops = {
	.read = DMA_RDTP_RPDR5_read,
	.write = eqos_write,
};

static ssize_t DMA_RDTP_RPDR4_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDTP_RPDR4_RD(DMA_RDTP_RPDR4_val);
	sprintf(debugfs_buf, "DMA_RDTP_RPDR4  :%#x\n", DMA_RDTP_RPDR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDTP_RPDR4_fops = {
	.read = DMA_RDTP_RPDR4_read,
	.write = eqos_write,
};

static ssize_t DMA_RDTP_RPDR3_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDTP_RPDR3_RD(DMA_RDTP_RPDR3_val);
	sprintf(debugfs_buf, "DMA_RDTP_RPDR3  :%#x\n", DMA_RDTP_RPDR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDTP_RPDR3_fops = {
	.read = DMA_RDTP_RPDR3_read,
	.write = eqos_write,
};

static ssize_t DMA_RDTP_RPDR2_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDTP_RPDR2_RD(DMA_RDTP_RPDR2_val);
	sprintf(debugfs_buf, "DMA_RDTP_RPDR2  :%#x\n", DMA_RDTP_RPDR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDTP_RPDR2_fops = {
	.read = DMA_RDTP_RPDR2_read,
	.write = eqos_write,
};

static ssize_t DMA_RDTP_RPDR1_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDTP_RPDR1_RD(DMA_RDTP_RPDR1_val);
	sprintf(debugfs_buf, "DMA_RDTP_RPDR1  :%#x\n", DMA_RDTP_RPDR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDTP_RPDR1_fops = {
	.read = DMA_RDTP_RPDR1_read,
	.write = eqos_write,
};

static ssize_t DMA_RDTP_RPDR0_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDTP_RPDR0_RD(DMA_RDTP_RPDR0_val);
	sprintf(debugfs_buf, "DMA_RDTP_RPDR0  :%#x\n", DMA_RDTP_RPDR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDTP_RPDR0_fops = {
	.read = DMA_RDTP_RPDR0_read,
	.write = eqos_write,
};

static ssize_t DMA_TDTP_TPDR7_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDTP_TPDR7_RD(DMA_TDTP_TPDR7_val);
	sprintf(debugfs_buf, "DMA_TDTP_TPDR7  :%#x\n", DMA_TDTP_TPDR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDTP_TPDR7_fops = {
	.read = DMA_TDTP_TPDR7_read,
	.write = eqos_write,
};

static ssize_t DMA_TDTP_TPDR6_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDTP_TPDR6_RD(DMA_TDTP_TPDR6_val);
	sprintf(debugfs_buf, "DMA_TDTP_TPDR6  :%#x\n", DMA_TDTP_TPDR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDTP_TPDR6_fops = {
	.read = DMA_TDTP_TPDR6_read,
	.write = eqos_write,
};

static ssize_t DMA_TDTP_TPDR5_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDTP_TPDR5_RD(DMA_TDTP_TPDR5_val);
	sprintf(debugfs_buf, "DMA_TDTP_TPDR5  :%#x\n", DMA_TDTP_TPDR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDTP_TPDR5_fops = {
	.read = DMA_TDTP_TPDR5_read,
	.write = eqos_write,
};

static ssize_t DMA_TDTP_TPDR4_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDTP_TPDR4_RD(DMA_TDTP_TPDR4_val);
	sprintf(debugfs_buf, "DMA_TDTP_TPDR4  :%#x\n", DMA_TDTP_TPDR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDTP_TPDR4_fops = {
	.read = DMA_TDTP_TPDR4_read,
	.write = eqos_write,
};

static ssize_t DMA_TDTP_TPDR3_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDTP_TPDR3_RD(DMA_TDTP_TPDR3_val);
	sprintf(debugfs_buf, "DMA_TDTP_TPDR3  :%#x\n", DMA_TDTP_TPDR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDTP_TPDR3_fops = {
	.read = DMA_TDTP_TPDR3_read,
	.write = eqos_write,
};

static ssize_t DMA_TDTP_TPDR2_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDTP_TPDR2_RD(DMA_TDTP_TPDR2_val);
	sprintf(debugfs_buf, "DMA_TDTP_TPDR2  :%#x\n", DMA_TDTP_TPDR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDTP_TPDR2_fops = {
	.read = DMA_TDTP_TPDR2_read,
	.write = eqos_write,
};

static ssize_t DMA_TDTP_TPDR1_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDTP_TPDR1_RD(DMA_TDTP_TPDR1_val);
	sprintf(debugfs_buf, "DMA_TDTP_TPDR1  :%#x\n", DMA_TDTP_TPDR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDTP_TPDR1_fops = {
	.read = DMA_TDTP_TPDR1_read,
	.write = eqos_write,
};

static ssize_t DMA_TDTP_TPDR0_read(struct file *file, char __user *userbuf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDTP_TPDR0_RD(DMA_TDTP_TPDR0_val);
	sprintf(debugfs_buf, "DMA_TDTP_TPDR0  :%#x\n", DMA_TDTP_TPDR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDTP_TPDR0_fops = {
	.read = DMA_TDTP_TPDR0_read,
	.write = eqos_write,
};

static ssize_t DMA_RDLAR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDLAR7_RD(DMA_RDLAR7_val);
	sprintf(debugfs_buf, "DMA_RDLAR7      :%#llx\n", DMA_RDLAR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDLAR7_fops = {
	.read = DMA_RDLAR7_read,
	.write = eqos_write,
};

static ssize_t DMA_RDLAR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDLAR6_RD(DMA_RDLAR6_val);
	sprintf(debugfs_buf, "DMA_RDLAR6      :%#llx\n", DMA_RDLAR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDLAR6_fops = {
	.read = DMA_RDLAR6_read,
	.write = eqos_write,
};

static ssize_t DMA_RDLAR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDLAR5_RD(DMA_RDLAR5_val);
	sprintf(debugfs_buf, "DMA_RDLAR5      :%#llx\n", DMA_RDLAR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDLAR5_fops = {
	.read = DMA_RDLAR5_read,
	.write = eqos_write,
};

static ssize_t DMA_RDLAR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDLAR4_RD(DMA_RDLAR4_val);
	sprintf(debugfs_buf, "DMA_RDLAR4      :%#llx\n", DMA_RDLAR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDLAR4_fops = {
	.read = DMA_RDLAR4_read,
	.write = eqos_write,
};

static ssize_t DMA_RDLAR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDLAR3_RD(DMA_RDLAR3_val);
	sprintf(debugfs_buf, "DMA_RDLAR3      :%#llx\n", DMA_RDLAR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDLAR3_fops = {
	.read = DMA_RDLAR3_read,
	.write = eqos_write,
};

static ssize_t DMA_RDLAR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDLAR2_RD(DMA_RDLAR2_val);
	sprintf(debugfs_buf, "DMA_RDLAR2      :%#llx\n", DMA_RDLAR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDLAR2_fops = {
	.read = DMA_RDLAR2_read,
	.write = eqos_write,
};

static ssize_t DMA_RDLAR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDLAR1_RD(DMA_RDLAR1_val);
	sprintf(debugfs_buf, "DMA_RDLAR1      :%#llx\n", DMA_RDLAR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDLAR1_fops = {
	.read = DMA_RDLAR1_read,
	.write = eqos_write,
};

static ssize_t DMA_RDLAR0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RDLAR0_RD(DMA_RDLAR0_val);
	sprintf(debugfs_buf, "DMA_RDLAR0      :%#llx\n", DMA_RDLAR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RDLAR0_fops = {
	.read = DMA_RDLAR0_read,
	.write = eqos_write,
};

static ssize_t DMA_TDLAR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDLAR7_RD(DMA_TDLAR7_val);
	sprintf(debugfs_buf, "DMA_TDLAR7      :%#llx\n", DMA_TDLAR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDLAR7_fops = {
	.read = DMA_TDLAR7_read,
	.write = eqos_write,
};

static ssize_t DMA_TDLAR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDLAR6_RD(DMA_TDLAR6_val);
	sprintf(debugfs_buf, "DMA_TDLAR6      :%#llx\n", DMA_TDLAR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDLAR6_fops = {
	.read = DMA_TDLAR6_read,
	.write = eqos_write,
};

static ssize_t DMA_TDLAR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDLAR5_RD(DMA_TDLAR5_val);
	sprintf(debugfs_buf, "DMA_TDLAR5      :%#llx\n", DMA_TDLAR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDLAR5_fops = {
	.read = DMA_TDLAR5_read,
	.write = eqos_write,
};

static ssize_t DMA_TDLAR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDLAR4_RD(DMA_TDLAR4_val);
	sprintf(debugfs_buf, "DMA_TDLAR4      :%#llx\n", DMA_TDLAR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDLAR4_fops = {
	.read = DMA_TDLAR4_read,
	.write = eqos_write,
};

static ssize_t DMA_TDLAR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDLAR3_RD(DMA_TDLAR3_val);
	sprintf(debugfs_buf, "DMA_TDLAR3      :%#llx\n", DMA_TDLAR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDLAR3_fops = {
	.read = DMA_TDLAR3_read,
	.write = eqos_write,
};

static ssize_t DMA_TDLAR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDLAR2_RD(DMA_TDLAR2_val);
	sprintf(debugfs_buf, "DMA_TDLAR2      :%#llx\n", DMA_TDLAR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDLAR2_fops = {
	.read = DMA_TDLAR2_read,
	.write = eqos_write,
};

static ssize_t DMA_TDLAR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDLAR1_RD(DMA_TDLAR1_val);
	sprintf(debugfs_buf, "DMA_TDLAR1      :%#llx\n", DMA_TDLAR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDLAR1_fops = {
	.read = DMA_TDLAR1_read,
	.write = eqos_write,
};

static ssize_t DMA_TDLAR0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TDLAR0_RD(DMA_TDLAR0_val);
	sprintf(debugfs_buf, "DMA_TDLAR0      :%#llx\n", DMA_TDLAR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TDLAR0_fops = {
	.read = DMA_TDLAR0_read,
	.write = eqos_write,
};

static ssize_t DMA_IER7_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_IER_RD(7, DMA_IER7_val);
	sprintf(debugfs_buf, "DMA_IER7        :%#x\n", DMA_IER7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_IER7_fops = {
	.read = DMA_IER7_read,
	.write = eqos_write,
};

static ssize_t DMA_IER6_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_IER_RD(6, DMA_IER6_val);
	sprintf(debugfs_buf, "DMA_IER6        :%#x\n", DMA_IER6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_IER6_fops = {
	.read = DMA_IER6_read,
	.write = eqos_write,
};

static ssize_t DMA_IER5_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_IER_RD(5, DMA_IER5_val);
	sprintf(debugfs_buf, "DMA_IER5        :%#x\n", DMA_IER5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_IER5_fops = {
	.read = DMA_IER5_read,
	.write = eqos_write,
};

static ssize_t DMA_IER4_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_IER_RD(4, DMA_IER4_val);
	sprintf(debugfs_buf, "DMA_IER4        :%#x\n", DMA_IER4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_IER4_fops = {
	.read = DMA_IER4_read,
	.write = eqos_write,
};

static ssize_t DMA_IER3_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_IER_RD(3, DMA_IER3_val);
	sprintf(debugfs_buf, "DMA_IER3        :%#x\n", DMA_IER3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_IER3_fops = {
	.read = DMA_IER3_read,
	.write = eqos_write,
};

static ssize_t DMA_IER2_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_IER_RD(2, DMA_IER2_val);
	sprintf(debugfs_buf, "DMA_IER2        :%#x\n", DMA_IER2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_IER2_fops = {
	.read = DMA_IER2_read,
	.write = eqos_write,
};

static ssize_t DMA_IER1_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_IER_RD(1, DMA_IER1_val);
	sprintf(debugfs_buf, "DMA_IER1        :%#x\n", DMA_IER1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_IER1_fops = {
	.read = DMA_IER1_read,
	.write = eqos_write,
};

static ssize_t DMA_IER0_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_IER_RD(0, DMA_IER0_val);
	sprintf(debugfs_buf, "DMA_IER0        :%#x\n", DMA_IER0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_IER0_fops = {
	.read = DMA_IER0_read,
	.write = eqos_write,
};

static ssize_t mac_imr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_IMR_RD(mac_imr_val);
	sprintf(debugfs_buf, "MAC_IMR         :%#x\n", mac_imr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_imr_fops = {
	.read = mac_imr_read,
	.write = eqos_write,
};

static ssize_t mac_isr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_ISR_RD(mac_isr_val);
	sprintf(debugfs_buf, "MAC_ISR         :%#x\n", mac_isr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_isr_fops = {
	.read = mac_isr_read,
	.write = eqos_write,
};

static ssize_t mtl_isr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_ISR_RD(mtl_isr_val);
	sprintf(debugfs_buf, "MTL_ISR         :%#x\n", mtl_isr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_isr_fops = {
	.read = mtl_isr_read,
	.write = eqos_write,
};

static ssize_t DMA_SR7_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SR_RD(7, DMA_SR7_val);
	sprintf(debugfs_buf, "DMA_SR7         :%#x\n", DMA_SR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SR7_fops = {
	.read = DMA_SR7_read,
	.write = eqos_write,
};

static ssize_t DMA_SR6_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SR_RD(6, DMA_SR6_val);
	sprintf(debugfs_buf, "DMA_SR6         :%#x\n", DMA_SR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SR6_fops = {
	.read = DMA_SR6_read,
	.write = eqos_write,
};

static ssize_t DMA_SR5_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SR_RD(5, DMA_SR5_val);
	sprintf(debugfs_buf, "DMA_SR5         :%#x\n", DMA_SR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SR5_fops = {
	.read = DMA_SR5_read,
	.write = eqos_write,
};

static ssize_t DMA_SR4_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SR_RD(4, DMA_SR4_val);
	sprintf(debugfs_buf, "DMA_SR4         :%#x\n", DMA_SR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SR4_fops = {
	.read = DMA_SR4_read,
	.write = eqos_write,
};

static ssize_t DMA_SR3_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SR_RD(3, DMA_SR3_val);
	sprintf(debugfs_buf, "DMA_SR3         :%#x\n", DMA_SR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SR3_fops = {
	.read = DMA_SR3_read,
	.write = eqos_write,
};

static ssize_t DMA_SR2_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SR_RD(2, DMA_SR2_val);
	sprintf(debugfs_buf, "DMA_SR2         :%#x\n", DMA_SR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SR2_fops = {
	.read = DMA_SR2_read,
	.write = eqos_write,
};

static ssize_t DMA_SR1_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SR_RD(1, DMA_SR1_val);
	sprintf(debugfs_buf, "DMA_SR1         :%#x\n", DMA_SR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SR1_fops = {
	.read = DMA_SR1_read,
	.write = eqos_write,
};

static ssize_t DMA_SR0_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SR_RD(0, DMA_SR0_val);
	sprintf(debugfs_buf, "DMA_SR0         :%#x\n", DMA_SR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SR0_fops = {
	.read = DMA_SR0_read,
	.write = eqos_write,
};

static ssize_t dma_isr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_ISR_RD(dma_isr_val);
	sprintf(debugfs_buf, "DMA_ISR         :%#x\n", dma_isr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_isr_fops = {
	.read = dma_isr_read,
	.write = eqos_write,
};

static ssize_t DMA_DSR2_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_DSR2_RD(DMA_DSR2_val);
	sprintf(debugfs_buf, "DMA_DSR2                  :%#x\n", DMA_DSR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_DSR2_fops = {
	.read = DMA_DSR2_read,
	.write = eqos_write,
};

static ssize_t DMA_DSR1_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_DSR1_RD(DMA_DSR1_val);
	sprintf(debugfs_buf, "DMA_DSR1                  :%#x\n", DMA_DSR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_DSR1_fops = {
	.read = DMA_DSR1_read,
	.write = eqos_write,
};

static ssize_t DMA_DSR0_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_DSR0_RD(DMA_DSR0_val);
	sprintf(debugfs_buf, "DMA_DSR0                  :%#x\n", DMA_DSR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_DSR0_fops = {
	.read = DMA_DSR0_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0rdr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0RDR_RD(MTL_Q0rdr_val);
	sprintf(debugfs_buf, "MTL_Q0RDR       :%#x\n", MTL_Q0rdr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0rdr_fops = {
	.read = MTL_Q0rdr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0esr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0ESR_RD(MTL_Q0esr_val);
	sprintf(debugfs_buf, "MTL_Q0ESR       :%#x\n", MTL_Q0esr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0esr_fops = {
	.read = MTL_Q0esr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0tdr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0TDR_RD(MTL_Q0tdr_val);
	sprintf(debugfs_buf, "MTL_Q0TDR       :%#x\n", MTL_Q0tdr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0tdr_fops = {
	.read = MTL_Q0tdr_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRBAR7_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRBAR7_RD(DMA_CHRBAR7_val);
	sprintf(debugfs_buf, "DMA_CHRBAR7     :%#x\n", DMA_CHRBAR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRBAR7_fops = {
	.read = DMA_CHRBAR7_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRBAR6_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRBAR6_RD(DMA_CHRBAR6_val);
	sprintf(debugfs_buf, "DMA_CHRBAR6     :%#x\n", DMA_CHRBAR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRBAR6_fops = {
	.read = DMA_CHRBAR6_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRBAR5_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRBAR5_RD(DMA_CHRBAR5_val);
	sprintf(debugfs_buf, "DMA_CHRBAR5     :%#x\n", DMA_CHRBAR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRBAR5_fops = {
	.read = DMA_CHRBAR5_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRBAR4_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRBAR4_RD(DMA_CHRBAR4_val);
	sprintf(debugfs_buf, "DMA_CHRBAR4     :%#x\n", DMA_CHRBAR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRBAR4_fops = {
	.read = DMA_CHRBAR4_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRBAR3_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRBAR3_RD(DMA_CHRBAR3_val);
	sprintf(debugfs_buf, "DMA_CHRBAR3     :%#x\n", DMA_CHRBAR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRBAR3_fops = {
	.read = DMA_CHRBAR3_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRBAR2_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRBAR2_RD(DMA_CHRBAR2_val);
	sprintf(debugfs_buf, "DMA_CHRBAR2     :%#x\n", DMA_CHRBAR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRBAR2_fops = {
	.read = DMA_CHRBAR2_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRBAR1_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRBAR1_RD(DMA_CHRBAR1_val);
	sprintf(debugfs_buf, "DMA_CHRBAR1     :%#x\n", DMA_CHRBAR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRBAR1_fops = {
	.read = DMA_CHRBAR1_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRBAR0_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRBAR0_RD(DMA_CHRBAR0_val);
	sprintf(debugfs_buf, "DMA_CHRBAR0     :%#x\n", DMA_CHRBAR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRBAR0_fops = {
	.read = DMA_CHRBAR0_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTBAR7_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTBAR7_RD(DMA_CHTBAR7_val);
	sprintf(debugfs_buf, "DMA_CHTBAR7     :%#x\n", DMA_CHTBAR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTBAR7_fops = {
	.read = DMA_CHTBAR7_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTBAR6_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTBAR6_RD(DMA_CHTBAR6_val);
	sprintf(debugfs_buf, "DMA_CHTBAR6     :%#x\n", DMA_CHTBAR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTBAR6_fops = {
	.read = DMA_CHTBAR6_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTBAR5_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTBAR5_RD(DMA_CHTBAR5_val);
	sprintf(debugfs_buf, "DMA_CHTBAR5     :%#x\n", DMA_CHTBAR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTBAR5_fops = {
	.read = DMA_CHTBAR5_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTBAR4_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTBAR4_RD(DMA_CHTBAR4_val);
	sprintf(debugfs_buf, "DMA_CHTBAR4     :%#x\n", DMA_CHTBAR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTBAR4_fops = {
	.read = DMA_CHTBAR4_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTBAR3_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTBAR3_RD(DMA_CHTBAR3_val);
	sprintf(debugfs_buf, "DMA_CHTBAR3     :%#x\n", DMA_CHTBAR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTBAR3_fops = {
	.read = DMA_CHTBAR3_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTBAR2_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTBAR2_RD(DMA_CHTBAR2_val);
	sprintf(debugfs_buf, "DMA_CHTBAR2     :%#x\n", DMA_CHTBAR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTBAR2_fops = {
	.read = DMA_CHTBAR2_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTBAR1_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTBAR1_RD(DMA_CHTBAR1_val);
	sprintf(debugfs_buf, "DMA_CHTBAR1     :%#x\n", DMA_CHTBAR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTBAR1_fops = {
	.read = DMA_CHTBAR1_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTBAR0_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTBAR0_RD(DMA_CHTBAR0_val);
	sprintf(debugfs_buf, "DMA_CHTBAR0     :%#x\n", DMA_CHTBAR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTBAR0_fops = {
	.read = DMA_CHTBAR0_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRDR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRDR7_RD(DMA_CHRDR7_val);
	sprintf(debugfs_buf, "DMA_CHRDR7      :%#x\n", DMA_CHRDR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRDR7_fops = {
	.read = DMA_CHRDR7_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRDR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRDR6_RD(DMA_CHRDR6_val);
	sprintf(debugfs_buf, "DMA_CHRDR6      :%#x\n", DMA_CHRDR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRDR6_fops = {
	.read = DMA_CHRDR6_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRDR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRDR5_RD(DMA_CHRDR5_val);
	sprintf(debugfs_buf, "DMA_CHRDR5      :%#x\n", DMA_CHRDR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRDR5_fops = {
	.read = DMA_CHRDR5_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRDR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRDR4_RD(DMA_CHRDR4_val);
	sprintf(debugfs_buf, "DMA_CHRDR4      :%#x\n", DMA_CHRDR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRDR4_fops = {
	.read = DMA_CHRDR4_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRDR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRDR3_RD(DMA_CHRDR3_val);
	sprintf(debugfs_buf, "DMA_CHRDR3      :%#x\n", DMA_CHRDR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRDR3_fops = {
	.read = DMA_CHRDR3_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRDR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRDR2_RD(DMA_CHRDR2_val);
	sprintf(debugfs_buf, "DMA_CHRDR2      :%#x\n", DMA_CHRDR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRDR2_fops = {
	.read = DMA_CHRDR2_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRDR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRDR1_RD(DMA_CHRDR1_val);
	sprintf(debugfs_buf, "DMA_CHRDR1      :%#x\n", DMA_CHRDR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRDR1_fops = {
	.read = DMA_CHRDR1_read,
	.write = eqos_write,
};

static ssize_t DMA_CHRDR0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHRDR0_RD(DMA_CHRDR0_val);
	sprintf(debugfs_buf, "DMA_CHRDR0      :%#x\n", DMA_CHRDR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHRDR0_fops = {
	.read = DMA_CHRDR0_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTDR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTDR7_RD(DMA_CHTDR7_val);
	sprintf(debugfs_buf, "DMA_CHTDR7      :%#x\n", DMA_CHTDR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTDR7_fops = {
	.read = DMA_CHTDR7_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTDR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTDR6_RD(DMA_CHTDR6_val);
	sprintf(debugfs_buf, "DMA_CHTDR6      :%#x\n", DMA_CHTDR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTDR6_fops = {
	.read = DMA_CHTDR6_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTDR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTDR5_RD(DMA_CHTDR5_val);
	sprintf(debugfs_buf, "DMA_CHTDR5      :%#x\n", DMA_CHTDR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTDR5_fops = {
	.read = DMA_CHTDR5_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTDR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTDR4_RD(DMA_CHTDR4_val);
	sprintf(debugfs_buf, "DMA_CHTDR4      :%#x\n", DMA_CHTDR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTDR4_fops = {
	.read = DMA_CHTDR4_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTDR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTDR3_RD(DMA_CHTDR3_val);
	sprintf(debugfs_buf, "DMA_CHTDR3      :%#x\n", DMA_CHTDR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTDR3_fops = {
	.read = DMA_CHTDR3_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTDR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTDR2_RD(DMA_CHTDR2_val);
	sprintf(debugfs_buf, "DMA_CHTDR2      :%#x\n", DMA_CHTDR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTDR2_fops = {
	.read = DMA_CHTDR2_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTDR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTDR1_RD(DMA_CHTDR1_val);
	sprintf(debugfs_buf, "DMA_CHTDR1      :%#x\n", DMA_CHTDR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTDR1_fops = {
	.read = DMA_CHTDR1_read,
	.write = eqos_write,
};

static ssize_t DMA_CHTDR0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CHTDR0_RD(DMA_CHTDR0_val);
	sprintf(debugfs_buf, "DMA_CHTDR0      :%#x\n", DMA_CHTDR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CHTDR0_fops = {
	.read = DMA_CHTDR0_read,
	.write = eqos_write,
};

static ssize_t DMA_SFCSR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SFCSR7_RD(DMA_SFCSR7_val);
	sprintf(debugfs_buf, "DMA_SFCSR7      :%#x\n", DMA_SFCSR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SFCSR7_fops = {
	.read = DMA_SFCSR7_read,
	.write = eqos_write,
};

static ssize_t DMA_SFCSR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SFCSR6_RD(DMA_SFCSR6_val);
	sprintf(debugfs_buf, "DMA_SFCSR6      :%#x\n", DMA_SFCSR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SFCSR6_fops = {
	.read = DMA_SFCSR6_read,
	.write = eqos_write,
};

static ssize_t DMA_SFCSR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SFCSR5_RD(DMA_SFCSR5_val);
	sprintf(debugfs_buf, "DMA_SFCSR5      :%#x\n", DMA_SFCSR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SFCSR5_fops = {
	.read = DMA_SFCSR5_read,
	.write = eqos_write,
};

static ssize_t DMA_SFCSR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SFCSR4_RD(DMA_SFCSR4_val);
	sprintf(debugfs_buf, "DMA_SFCSR4      :%#x\n", DMA_SFCSR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SFCSR4_fops = {
	.read = DMA_SFCSR4_read,
	.write = eqos_write,
};

static ssize_t DMA_SFCSR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SFCSR3_RD(DMA_SFCSR3_val);
	sprintf(debugfs_buf, "DMA_SFCSR3      :%#x\n", DMA_SFCSR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SFCSR3_fops = {
	.read = DMA_SFCSR3_read,
	.write = eqos_write,
};

static ssize_t DMA_SFCSR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SFCSR2_RD(DMA_SFCSR2_val);
	sprintf(debugfs_buf, "DMA_SFCSR2      :%#x\n", DMA_SFCSR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SFCSR2_fops = {
	.read = DMA_SFCSR2_read,
	.write = eqos_write,
};

static ssize_t DMA_SFCSR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SFCSR1_RD(DMA_SFCSR1_val);
	sprintf(debugfs_buf, "DMA_SFCSR1      :%#x\n", DMA_SFCSR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SFCSR1_fops = {
	.read = DMA_SFCSR1_read,
	.write = eqos_write,
};

static ssize_t DMA_SFCSR0_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SFCSR0_RD(DMA_SFCSR0_val);
	sprintf(debugfs_buf, "DMA_SFCSR0      :%#x\n", DMA_SFCSR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_SFCSR0_fops = {
	.read = DMA_SFCSR0_read,
	.write = eqos_write,
};

static ssize_t mac_ivlantirr_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_IVLANTIRR_RD(mac_ivlantirr_val);
	sprintf(debugfs_buf, "MAC_IVLANTIRR              :%#x\n",
		mac_ivlantirr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_ivlantirr_fops = {
	.read = mac_ivlantirr_read,
	.write = eqos_write,
};

static ssize_t mac_vlantirr_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_VLANTIRR_RD(mac_vlantirr_val);
	sprintf(debugfs_buf, "MAC_VLANTIRR               :%#x\n",
		mac_vlantirr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_vlantirr_fops = {
	.read = mac_vlantirr_read,
	.write = eqos_write,
};

static ssize_t mac_vlanhtr_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_VLANHTR_RD(mac_vlanhtr_val);
	sprintf(debugfs_buf, "MAC_VLANHTR                :%#x\n",
		mac_vlanhtr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_vlanhtr_fops = {
	.read = mac_vlanhtr_read,
	.write = eqos_write,
};

static ssize_t mac_vlantr_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_VLANTR_RD(mac_vlantr_val);
	sprintf(debugfs_buf, "MAC_VLANTR                 :%#x\n",
		mac_vlantr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_vlantr_fops = {
	.read = mac_vlantr_read,
	.write = eqos_write,
};

static ssize_t dma_sbus_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_SBUS_RD(dma_sbus_val);
	sprintf(debugfs_buf, "DMA_SBUS                  :%#x\n", dma_sbus_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_sbus_fops = {
	.read = dma_sbus_read,
	.write = eqos_write,
};

static ssize_t dma_bmr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_BMR_RD(dma_bmr_val);
	sprintf(debugfs_buf, "DMA_BMR                    :%#x\n", dma_bmr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations dma_bmr_fops = {
	.read = dma_bmr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0rcr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0RCR_RD(MTL_Q0rcr_val);
	sprintf(debugfs_buf, "MTL_Q0RCR       :%#x\n", MTL_Q0rcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0rcr_fops = {
	.read = MTL_Q0rcr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0ocr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0OCR_RD(MTL_Q0ocr_val);
	sprintf(debugfs_buf, "MTL_Q0OCR       :%#x\n", MTL_Q0ocr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0ocr_fops = {
	.read = MTL_Q0ocr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0romr_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0ROMR_RD(MTL_Q0romr_val);
	sprintf(debugfs_buf, "MTL_Q0ROMR      :%#x\n", MTL_Q0romr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0romr_fops = {
	.read = MTL_Q0romr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0qr_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0QR_RD(MTL_Q0qr_val);
	sprintf(debugfs_buf, "MTL_Q0QR        :%#x\n", MTL_Q0qr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0qr_fops = {
	.read = MTL_Q0qr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0ecr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0ECR_RD(MTL_Q0ecr_val);
	sprintf(debugfs_buf, "MTL_Q0ECR       :%#x\n", MTL_Q0ecr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0ecr_fops = {
	.read = MTL_Q0ecr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0ucr_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0UCR_RD(MTL_Q0ucr_val);
	sprintf(debugfs_buf, "MTL_Q0UCR       :%#x\n", MTL_Q0ucr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0ucr_fops = {
	.read = MTL_Q0ucr_read,
	.write = eqos_write,
};

static ssize_t MTL_Q0tomr_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_Q0TOMR_RD(MTL_Q0tomr_val);
	sprintf(debugfs_buf, "MTL_Q0TOMR      :%#x\n", MTL_Q0tomr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_Q0tomr_fops = {
	.read = MTL_Q0tomr_read,
	.write = eqos_write,
};

static ssize_t MTL_RQDCM1r_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_RQDCM1R_RD(MTL_RQDCM1r_val);
	sprintf(debugfs_buf, "MTL_RQDCM1R     :%#x\n", MTL_RQDCM1r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_RQDCM1r_fops = {
	.read = MTL_RQDCM1r_read,
	.write = eqos_write,
};

static ssize_t MTL_RQDCM0r_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_RQDCM0R_RD(MTL_RQDCM0r_val);
	sprintf(debugfs_buf, "MTL_RQDCM0R     :%#x\n", MTL_RQDCM0r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MTL_RQDCM0r_fops = {
	.read = MTL_RQDCM0r_read,
	.write = eqos_write,
};

static ssize_t mtl_fddr_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_FDDR_RD(mtl_fddr_val);
	sprintf(debugfs_buf, "MTL_FDDR        :%#x\n", mtl_fddr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_fddr_fops = {
	.read = mtl_fddr_read,
	.write = eqos_write,
};

static ssize_t mtl_fdacs_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_FDACS_RD(mtl_fdacs_val);
	sprintf(debugfs_buf, "MTL_FDACS       :%#x\n", mtl_fdacs_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_fdacs_fops = {
	.read = mtl_fdacs_read,
	.write = eqos_write,
};

static ssize_t mtl_omr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MTL_OMR_RD(mtl_omr_val);
	sprintf(debugfs_buf, "MTL_OMR         :%#x\n", mtl_omr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mtl_omr_fops = {
	.read = mtl_omr_read,
	.write = eqos_write,
};

static ssize_t MAC_RQC3r_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_RQC3R_RD(MAC_RQC3r_val);
	sprintf(debugfs_buf, "MAC_RQC3R       :%#x\n", MAC_RQC3r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_RQC3r_fops = {
	.read = MAC_RQC3r_read,
	.write = eqos_write,
};

static ssize_t MAC_RQC2r_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_RQC2R_RD(MAC_RQC2r_val);
	sprintf(debugfs_buf, "MAC_RQC2R       :%#x\n", MAC_RQC2r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_RQC2r_fops = {
	.read = MAC_RQC2r_read,
	.write = eqos_write,
};

static ssize_t MAC_RQC1r_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_RQC1R_RD(MAC_RQC1r_val);
	sprintf(debugfs_buf, "MAC_RQC1R       :%#x\n", MAC_RQC1r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_RQC1r_fops = {
	.read = MAC_RQC1r_read,
	.write = eqos_write,
};

static ssize_t MAC_RQC0r_read(struct file *file, char __user *userbuf,
			      size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_RQC0R_RD(MAC_RQC0r_val);
	sprintf(debugfs_buf, "MAC_RQC0R       :%#x\n", MAC_RQC0r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_RQC0r_fops = {
	.read = MAC_RQC0r_read,
	.write = eqos_write,
};

static ssize_t MAC_TQPM1r_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TQPM1R_RD(MAC_TQPM1r_val);
	sprintf(debugfs_buf, "MAC_TQPM1R      :%#x\n", MAC_TQPM1r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_TQPM1r_fops = {
	.read = MAC_TQPM1r_read,
	.write = eqos_write,
};

static ssize_t MAC_TQPM0r_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_TQPM0R_RD(MAC_TQPM0r_val);
	sprintf(debugfs_buf, "MAC_TQPM0R      :%#x\n", MAC_TQPM0r_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_TQPM0r_fops = {
	.read = MAC_TQPM0r_read,
	.write = eqos_write,
};

static ssize_t mac_rfcr_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_RFCR_RD(mac_rfcr_val);
	sprintf(debugfs_buf, "MAC_RFCR        :%#x\n", mac_rfcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_rfcr_fops = {
	.read = mac_rfcr_read,
	.write = eqos_write,
};

static ssize_t MAC_QTFCR7_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_QTFCR7_RD(MAC_QTFCR7_val);
	sprintf(debugfs_buf, "MAC_QTFCR7      :%#x\n", MAC_QTFCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_QTFCR7_fops = {
	.read = MAC_QTFCR7_read,
	.write = eqos_write,
};

static ssize_t MAC_QTFCR6_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_QTFCR6_RD(MAC_QTFCR6_val);
	sprintf(debugfs_buf, "MAC_QTFCR6      :%#x\n", MAC_QTFCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_QTFCR6_fops = {
	.read = MAC_QTFCR6_read,
	.write = eqos_write,
};

static ssize_t MAC_QTFCR5_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_QTFCR5_RD(MAC_QTFCR5_val);
	sprintf(debugfs_buf, "MAC_QTFCR5      :%#x\n", MAC_QTFCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_QTFCR5_fops = {
	.read = MAC_QTFCR5_read,
	.write = eqos_write,
};

static ssize_t MAC_QTFCR4_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_QTFCR4_RD(MAC_QTFCR4_val);
	sprintf(debugfs_buf, "MAC_QTFCR4      :%#x\n", MAC_QTFCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_QTFCR4_fops = {
	.read = MAC_QTFCR4_read,
	.write = eqos_write,
};

static ssize_t MAC_QTFCR3_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_QTFCR3_RD(MAC_QTFCR3_val);
	sprintf(debugfs_buf, "MAC_QTFCR3      :%#x\n", MAC_QTFCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_QTFCR3_fops = {
	.read = MAC_QTFCR3_read,
	.write = eqos_write,
};

static ssize_t MAC_QTFCR2_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_QTFCR2_RD(MAC_QTFCR2_val);
	sprintf(debugfs_buf, "MAC_QTFCR2      :%#x\n", MAC_QTFCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_QTFCR2_fops = {
	.read = MAC_QTFCR2_read,
	.write = eqos_write,
};

static ssize_t MAC_QTFCR1_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_QTFCR1_RD(MAC_QTFCR1_val);
	sprintf(debugfs_buf, "MAC_QTFCR1      :%#x\n", MAC_QTFCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_QTFCR1_fops = {
	.read = MAC_QTFCR1_read,
	.write = eqos_write,
};

static ssize_t MAC_Q0tfcr_read(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_Q0TFCR_RD(MAC_Q0tfcr_val);
	sprintf(debugfs_buf, "MAC_Q0TFCR      :%#x\n", MAC_Q0tfcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MAC_Q0tfcr_fops = {
	.read = MAC_Q0tfcr_read,
	.write = eqos_write,
};

static ssize_t DMA_AXI4CR7_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_AXI4CR7_RD(DMA_AXI4CR7_val);
	sprintf(debugfs_buf, "DMA_AXI4CR7     :%#x\n", DMA_AXI4CR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_AXI4CR7_fops = {
	.read = DMA_AXI4CR7_read,
	.write = eqos_write,
};

static ssize_t DMA_AXI4CR6_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_AXI4CR6_RD(DMA_AXI4CR6_val);
	sprintf(debugfs_buf, "DMA_AXI4CR6     :%#x\n", DMA_AXI4CR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_AXI4CR6_fops = {
	.read = DMA_AXI4CR6_read,
	.write = eqos_write,
};

static ssize_t DMA_AXI4CR5_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_AXI4CR5_RD(DMA_AXI4CR5_val);
	sprintf(debugfs_buf, "DMA_AXI4CR5     :%#x\n", DMA_AXI4CR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_AXI4CR5_fops = {
	.read = DMA_AXI4CR5_read,
	.write = eqos_write,
};

static ssize_t DMA_AXI4CR4_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_AXI4CR4_RD(DMA_AXI4CR4_val);
	sprintf(debugfs_buf, "DMA_AXI4CR4     :%#x\n", DMA_AXI4CR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_AXI4CR4_fops = {
	.read = DMA_AXI4CR4_read,
	.write = eqos_write,
};

static ssize_t DMA_AXI4CR3_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_AXI4CR3_RD(DMA_AXI4CR3_val);
	sprintf(debugfs_buf, "DMA_AXI4CR3     :%#x\n", DMA_AXI4CR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_AXI4CR3_fops = {
	.read = DMA_AXI4CR3_read,
	.write = eqos_write,
};

static ssize_t DMA_AXI4CR2_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_AXI4CR2_RD(DMA_AXI4CR2_val);
	sprintf(debugfs_buf, "DMA_AXI4CR2     :%#x\n", DMA_AXI4CR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_AXI4CR2_fops = {
	.read = DMA_AXI4CR2_read,
	.write = eqos_write,
};

static ssize_t DMA_AXI4CR1_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_AXI4CR1_RD(DMA_AXI4CR1_val);
	sprintf(debugfs_buf, "DMA_AXI4CR1     :%#x\n", DMA_AXI4CR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_AXI4CR1_fops = {
	.read = DMA_AXI4CR1_read,
	.write = eqos_write,
};

static ssize_t DMA_AXI4CR0_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_AXI4CR0_RD(DMA_AXI4CR0_val);
	sprintf(debugfs_buf, "DMA_AXI4CR0     :%#x\n", DMA_AXI4CR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_AXI4CR0_fops = {
	.read = DMA_AXI4CR0_read,
	.write = eqos_write,
};

static ssize_t DMA_RCR7_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RCR7_RD(DMA_RCR7_val);
	sprintf(debugfs_buf, "DMA_RCR7        :%#x\n", DMA_RCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RCR7_fops = {
	.read = DMA_RCR7_read,
	.write = eqos_write,
};

static ssize_t DMA_RCR6_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RCR6_RD(DMA_RCR6_val);
	sprintf(debugfs_buf, "DMA_RCR6        :%#x\n", DMA_RCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RCR6_fops = {
	.read = DMA_RCR6_read,
	.write = eqos_write,
};

static ssize_t DMA_RCR5_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RCR5_RD(DMA_RCR5_val);
	sprintf(debugfs_buf, "DMA_RCR5        :%#x\n", DMA_RCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RCR5_fops = {
	.read = DMA_RCR5_read,
	.write = eqos_write,
};

static ssize_t DMA_RCR4_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RCR4_RD(DMA_RCR4_val);
	sprintf(debugfs_buf, "DMA_RCR4        :%#x\n", DMA_RCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RCR4_fops = {
	.read = DMA_RCR4_read,
	.write = eqos_write,
};

static ssize_t DMA_RCR3_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RCR3_RD(DMA_RCR3_val);
	sprintf(debugfs_buf, "DMA_RCR3        :%#x\n", DMA_RCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RCR3_fops = {
	.read = DMA_RCR3_read,
	.write = eqos_write,
};

static ssize_t DMA_RCR2_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RCR2_RD(DMA_RCR2_val);
	sprintf(debugfs_buf, "DMA_RCR2        :%#x\n", DMA_RCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RCR2_fops = {
	.read = DMA_RCR2_read,
	.write = eqos_write,
};

static ssize_t DMA_RCR1_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RCR1_RD(DMA_RCR1_val);
	sprintf(debugfs_buf, "DMA_RCR1        :%#x\n", DMA_RCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RCR1_fops = {
	.read = DMA_RCR1_read,
	.write = eqos_write,
};

static ssize_t DMA_RCR0_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_RCR0_RD(DMA_RCR0_val);
	sprintf(debugfs_buf, "DMA_RCR0        :%#x\n", DMA_RCR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_RCR0_fops = {
	.read = DMA_RCR0_read,
	.write = eqos_write,
};

static ssize_t DMA_TCR7_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TCR7_RD(DMA_TCR7_val);
	sprintf(debugfs_buf, "DMA_TCR7        :%#x\n", DMA_TCR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TCR7_fops = {
	.read = DMA_TCR7_read,
	.write = eqos_write,
};

static ssize_t DMA_TCR6_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TCR6_RD(DMA_TCR6_val);
	sprintf(debugfs_buf, "DMA_TCR6        :%#x\n", DMA_TCR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TCR6_fops = {
	.read = DMA_TCR6_read,
	.write = eqos_write,
};

static ssize_t DMA_TCR5_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TCR5_RD(DMA_TCR5_val);
	sprintf(debugfs_buf, "DMA_TCR5        :%#x\n", DMA_TCR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TCR5_fops = {
	.read = DMA_TCR5_read,
	.write = eqos_write,
};

static ssize_t DMA_TCR4_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TCR4_RD(DMA_TCR4_val);
	sprintf(debugfs_buf, "DMA_TCR4        :%#x\n", DMA_TCR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TCR4_fops = {
	.read = DMA_TCR4_read,
	.write = eqos_write,
};

static ssize_t DMA_TCR3_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TCR3_RD(DMA_TCR3_val);
	sprintf(debugfs_buf, "DMA_TCR3        :%#x\n", DMA_TCR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TCR3_fops = {
	.read = DMA_TCR3_read,
	.write = eqos_write,
};

static ssize_t DMA_TCR2_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TCR2_RD(DMA_TCR2_val);
	sprintf(debugfs_buf, "DMA_TCR2        :%#x\n", DMA_TCR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TCR2_fops = {
	.read = DMA_TCR2_read,
	.write = eqos_write,
};

static ssize_t DMA_TCR1_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TCR1_RD(DMA_TCR1_val);
	sprintf(debugfs_buf, "DMA_TCR1        :%#x\n", DMA_TCR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TCR1_fops = {
	.read = DMA_TCR1_read,
	.write = eqos_write,
};

static ssize_t DMA_TCR0_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_TCR0_RD(DMA_TCR0_val);
	sprintf(debugfs_buf, "DMA_TCR0        :%#x\n", DMA_TCR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_TCR0_fops = {
	.read = DMA_TCR0_read,
	.write = eqos_write,
};

static ssize_t DMA_CR7_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CR7_RD(DMA_CR7_val);
	sprintf(debugfs_buf, "DMA_CR7         :%#x\n", DMA_CR7_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CR7_fops = {
	.read = DMA_CR7_read,
	.write = eqos_write,
};

static ssize_t DMA_CR6_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CR6_RD(DMA_CR6_val);
	sprintf(debugfs_buf, "DMA_CR6         :%#x\n", DMA_CR6_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CR6_fops = {
	.read = DMA_CR6_read,
	.write = eqos_write,
};

static ssize_t DMA_CR5_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CR5_RD(DMA_CR5_val);
	sprintf(debugfs_buf, "DMA_CR5         :%#x\n", DMA_CR5_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CR5_fops = {
	.read = DMA_CR5_read,
	.write = eqos_write,
};

static ssize_t DMA_CR4_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CR4_RD(DMA_CR4_val);
	sprintf(debugfs_buf, "DMA_CR4         :%#x\n", DMA_CR4_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CR4_fops = {
	.read = DMA_CR4_read,
	.write = eqos_write,
};

static ssize_t DMA_CR3_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CR3_RD(DMA_CR3_val);
	sprintf(debugfs_buf, "DMA_CR3         :%#x\n", DMA_CR3_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CR3_fops = {
	.read = DMA_CR3_read,
	.write = eqos_write,
};

static ssize_t DMA_CR2_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CR2_RD(DMA_CR2_val);
	sprintf(debugfs_buf, "DMA_CR2         :%#x\n", DMA_CR2_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CR2_fops = {
	.read = DMA_CR2_read,
	.write = eqos_write,
};

static ssize_t DMA_CR1_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CR1_RD(DMA_CR1_val);
	sprintf(debugfs_buf, "DMA_CR1         :%#x\n", DMA_CR1_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CR1_fops = {
	.read = DMA_CR1_read,
	.write = eqos_write,
};

static ssize_t DMA_CR0_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	DMA_CR0_RD(DMA_CR0_val);
	sprintf(debugfs_buf, "DMA_CR0         :%#x\n", DMA_CR0_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations DMA_CR0_fops = {
	.read = DMA_CR0_read,
	.write = eqos_write,
};

static ssize_t mac_wtr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_WTR_RD(mac_wtr_val);
	sprintf(debugfs_buf, "MAC_WTR         :%#x\n", mac_wtr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_wtr_fops = {
	.read = mac_wtr_read,
	.write = eqos_write,
};

static ssize_t mac_mpfr_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MPFR_RD(mac_mpfr_val);
	sprintf(debugfs_buf, "MAC_MPFR        :%#x\n", mac_mpfr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_mpfr_fops = {
	.read = mac_mpfr_read,
	.write = eqos_write,
};

static ssize_t mac_mecr_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MECR_RD(mac_mecr_val);
	sprintf(debugfs_buf, "MAC_MECR        :%#x\n", mac_mecr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_mecr_fops = {
	.read = mac_mecr_read,
	.write = eqos_write,
};

static ssize_t mac_mcr_read(struct file *file, char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	ssize_t ret;

	MAC_MCR_RD(mac_mcr_val);
	sprintf(debugfs_buf, "MAC_MCR         :%#x\n", mac_mcr_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mac_mcr_fops = {
	.read = mac_mcr_read,
	.write = eqos_write,
};

/* For MII/GMII registers */
static ssize_t mii_bmcr_reg_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR,
				     &mii_bmcr_reg_val);
	sprintf(debugfs_buf,
		"Phy Control Reg(Basic Mode Control Reg)      :%#x\n",
		mii_bmcr_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_bmcr_reg_fops = {
	.read = mii_bmcr_reg_read,
	.write = eqos_write,
};

static ssize_t mii_bmsr_reg_read(struct file *file, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMSR,
				     &mii_bmsr_reg_val);
	sprintf(debugfs_buf,
		"Phy Status Reg(Basic Mode Status Reg)        :%#x\n",
		mii_bmsr_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations mii_bmsr_reg_fops = {
	.read = mii_bmsr_reg_read,
};

static ssize_t MII_PHYSID1_reg_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID1,
				     &MII_PHYSID1_reg_val);
	sprintf(debugfs_buf,
		"Phy Id (PHYS ID 1)                           :%#x\n",
		MII_PHYSID1_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MII_PHYSID1_reg_fops = {
	.read = MII_PHYSID1_reg_read,
};

static ssize_t MII_PHYSID2_reg_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID2,
				     &MII_PHYSID2_reg_val);
	sprintf(debugfs_buf,
		"Phy Id (PHYS ID 2)                           :%#x\n",
		MII_PHYSID2_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MII_PHYSID2_reg_fops = {
	.read = MII_PHYSID2_reg_read,
};

static ssize_t mii_advertise_reg_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ADVERTISE,
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
	.write = eqos_write,
};

static ssize_t mii_lpa_reg_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_LPA,
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

static ssize_t mii_expansion_reg_read(struct file *file, char __user *userbuf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_EXPANSION,
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

static ssize_t auto_nego_np_reg_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr,
				     EQOS_AUTO_NEGO_NP,
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
	.write = eqos_write,
};

static ssize_t mii_estatus_reg_read(struct file *file, char __user *userbuf,
				    size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ESTATUS,
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

static ssize_t MII_CTRL1000_reg_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_CTRL1000,
				     &MII_CTRL1000_reg_val);
	sprintf(debugfs_buf,
		"1000 Ctl Reg (1000BASE-T Control Reg)        :%#x\n",
		MII_CTRL1000_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MII_CTRL1000_reg_fops = {
	.read = MII_CTRL1000_reg_read,
	.write = eqos_write,
};

static ssize_t MII_STAT1000_reg_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_STAT1000,
				     &MII_STAT1000_reg_val);
	sprintf(debugfs_buf,
		"1000 Sts Reg (1000BASE-T Status)             :%#x\n",
		MII_STAT1000_reg_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations MII_STAT1000_reg_fops = {
	.read = MII_STAT1000_reg_read,
};

static ssize_t phy_ctl_reg_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, EQOS_PHY_CTL,
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
	.write = eqos_write,
};

static ssize_t phy_sts_reg_read(struct file *file, char __user *userbuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;

	eqos_mdio_read_direct(pdata, pdata->phyaddr, EQOS_PHY_STS,
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
						char __user *userbuf,
						size_t count, loff_t *ppos)
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
	.write = eqos_write,
};

static ssize_t qinx_read(struct file *file,
			 char __user *userbuf, size_t count, loff_t *ppos)
{
	ssize_t ret;

	sprintf(debugfs_buf, "qinx             :%#x\n", qinx_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations qinx_fops = {
	.read = qinx_read,
	.write = eqos_write,
};

static ssize_t reg_offset_read(struct file *file,
			 char __user *userbuf, size_t count, loff_t *ppos)
{
	ssize_t ret;

	sprintf(debugfs_buf, "reg_offset             :%#x\n", reg_offset_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations reg_offset_fops = {
	.read = reg_offset_read,
	.write = eqos_write,
};

static ssize_t gen_reg_read(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	ssize_t ret;

	unsigned int data_val;

	if (reg_offset_val & 0x10000) {
		/* read clause45 phy reg */
		bcm_regs_clause45_read(2,
				(reg_offset_val & 0xffff),
				&data_val);
		sprintf(debugfs_buf, "reg(%#x) = %#x\n",
			reg_offset_val, data_val);
		ret = simple_read_from_buffer(userbuf,
					count,
					ppos,
					debugfs_buf,
					strlen(debugfs_buf));
	} else if (reg_offset_val & 0x20000) {
		/* read phy reg */
		eqos_mdio_read_direct(pdata, pdata->phyaddr,
			(reg_offset_val & 0xffff), &data_val);
		sprintf(debugfs_buf, "reg(%#x) = %#x\n",
			reg_offset_val, data_val);
		ret = simple_read_from_buffer(userbuf,
					count,
					ppos,
					debugfs_buf,
					strlen(debugfs_buf));
	} else  {
		/* read mac reg */
		gen_reg_val = ioread32((void *)(ULONG *)
			(BASE_ADDRESS + reg_offset_val));
		sprintf(debugfs_buf, "reg(%#x) = %#x\n", reg_offset_val,
			gen_reg_val);
		ret = simple_read_from_buffer(userbuf,
					count,
					ppos,
					debugfs_buf,
					strlen(debugfs_buf));
	}
	return ret;
}

static const struct file_operations gen_reg_fops = {
	.read = gen_reg_read,
	.write = eqos_write,
};

static ssize_t do_tx_align_tst_read(struct file *file,
			 char __user *userbuf, size_t count, loff_t *ppos)
{
	ssize_t ret;

	sprintf(debugfs_buf, "do_tx_align_tst        :%#x\n",
		do_tx_align_tst_val);
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debugfs_buf,
				    strlen(debugfs_buf));
	return ret;
}

static const struct file_operations do_tx_align_tst_fops = {
	.read = do_tx_align_tst_read,
	.write = eqos_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read0(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read0\n");
	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val, desc_num)
			, prx_desc[desc_num].rdes3,
			prx_desc[desc_num].rdes2,
			prx_desc[desc_num].rdes1,
			prx_desc[desc_num].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read0\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops0 = {
	.read = RX_NORMAL_DESC_descriptor_read0,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read1(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
						GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read1\n");
	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 20)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val, desc_num + 20)
			, prx_desc[desc_num + 20].rdes3,
			prx_desc[desc_num + 20].rdes2,
			prx_desc[desc_num + 20].rdes1,
			prx_desc[desc_num + 20].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read1\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops1 = {
	.read = RX_NORMAL_DESC_descriptor_read1,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read2(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read2\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 40)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val, desc_num + 40)
			, prx_desc[desc_num + 40].rdes3,
			prx_desc[desc_num + 40].rdes2,
			prx_desc[desc_num + 40].rdes1,
			prx_desc[desc_num + 40].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read2\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops2 = {
	.read = RX_NORMAL_DESC_descriptor_read2,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read3(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read3\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 60)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val, desc_num + 60)
			, prx_desc[desc_num + 60].rdes3,
			prx_desc[desc_num + 60].rdes2,
			prx_desc[desc_num + 60].rdes1,
			prx_desc[desc_num + 60].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read3\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops3 = {
	.read = RX_NORMAL_DESC_descriptor_read3,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read4(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read4\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 80)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val, desc_num + 80)
			, prx_desc[desc_num + 80].rdes3,
			prx_desc[desc_num + 80].rdes2,
			prx_desc[desc_num + 80].rdes1,
			prx_desc[desc_num + 80].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read4\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops4 = {
	.read = RX_NORMAL_DESC_descriptor_read4,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read5(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read5\n");
	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 100)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val,
						desc_num + 100)
			, prx_desc[desc_num + 100].rdes3,
			prx_desc[desc_num + 100].rdes2,
			prx_desc[desc_num + 100].rdes1,
			prx_desc[desc_num + 100].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read5\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops5 = {
	.read = RX_NORMAL_DESC_descriptor_read5,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read6(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read6\n");
	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 120)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val,
						desc_num + 120)
			, prx_desc[desc_num + 120].rdes3,
			prx_desc[desc_num + 120].rdes2,
			prx_desc[desc_num + 120].rdes1,
			prx_desc[desc_num + 120].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read6\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops6 = {
	.read = RX_NORMAL_DESC_descriptor_read6,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read7(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read7\n");
	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 140)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val,
						desc_num + 140)
			, prx_desc[desc_num + 140].rdes3,
			prx_desc[desc_num + 140].rdes2,
			prx_desc[desc_num + 140].rdes1,
			prx_desc[desc_num + 140].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read7\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops7 = {
	.read = RX_NORMAL_DESC_descriptor_read7,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read8(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read8\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 160)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val,
						desc_num + 160)
			, prx_desc[desc_num + 160].rdes3,
			prx_desc[desc_num + 160].rdes2,
			prx_desc[desc_num + 160].rdes1,
			prx_desc[desc_num + 160].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read8\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops8 = {
	.read = RX_NORMAL_DESC_descriptor_read8,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read9(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read9\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 180)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val,
						desc_num + 180)
			, prx_desc[desc_num + 180].rdes3,
			prx_desc[desc_num + 180].rdes2,
			prx_desc[desc_num + 180].rdes1,
			prx_desc[desc_num + 180].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read9\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops9 = {
	.read = RX_NORMAL_DESC_descriptor_read9,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read10(struct file *file,
						char __user *userbuf,
						size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read10\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 200)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val,
						desc_num + 200)
			, prx_desc[desc_num + 200].rdes3,
			prx_desc[desc_num + 200].rdes2,
			prx_desc[desc_num + 200].rdes1,
			prx_desc[desc_num + 200].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read10\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops10 = {
	.read = RX_NORMAL_DESC_descriptor_read10,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read11(struct file *file,
						char __user *userbuf,
						size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read11\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 220)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val,
						desc_num + 220)
			, prx_desc[desc_num + 220].rdes3,
			prx_desc[desc_num + 220].rdes2,
			prx_desc[desc_num + 220].rdes1,
			prx_desc[desc_num + 220].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read11\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops11 = {
	.read = RX_NORMAL_DESC_descriptor_read11,
	.write = descriptor_write,
};

static ssize_t RX_NORMAL_DESC_descriptor_read12(struct file *file,
						char __user *userbuf,
						size_t count, loff_t *ppos)
{
	struct s_rx_desc *prx_desc =
					GET_RX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> RX_NORMAL_DESC_descriptor_read12\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(9952, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 16; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"rx_desc.rdes3             :%x\n"
			"rx_desc.rdes2             :%x\n"
			"rx_desc.rdes1             :%x\n"
			"rx_desc.rdes0             :%x\n",
			qinx_val, (int)(desc_num + 240)
			, (ULONG) GET_RX_DESC_DMA_ADDR(qinx_val,
						desc_num + 240)
			, prx_desc[desc_num + 240].rdes3,
			prx_desc[desc_num + 240].rdes2,
			prx_desc[desc_num + 240].rdes1,
			prx_desc[desc_num + 240].rdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- RX_NORMAL_DESC_descriptor_read12\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_desc_fops12 = {
	.read = RX_NORMAL_DESC_descriptor_read12,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read0(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read0\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num)
			, ptx_desc[desc_num].tdes3,
			ptx_desc[desc_num].tdes2,
			ptx_desc[desc_num].tdes1,
			ptx_desc[desc_num].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read0\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops0 = {
	.read = TX_NORMAL_DESC_descriptor_read0,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read1(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read1\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 20)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 20)
			, ptx_desc[desc_num + 20].tdes3,
			ptx_desc[desc_num + 20].tdes2,
			ptx_desc[desc_num + 20].tdes1,
			ptx_desc[desc_num + 20].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read1\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops1 = {
	.read = TX_NORMAL_DESC_descriptor_read1,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read2(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read2\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 40)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 40)
			, ptx_desc[desc_num + 40].tdes3,
			ptx_desc[desc_num + 40].tdes2,
			ptx_desc[desc_num + 40].tdes1,
			ptx_desc[desc_num + 40].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read2\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops2 = {
	.read = TX_NORMAL_DESC_descriptor_read2,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read3(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read3\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 60)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 60)
			, ptx_desc[desc_num + 60].tdes3,
			ptx_desc[desc_num + 60].tdes2,
			ptx_desc[desc_num + 60].tdes1,
			ptx_desc[desc_num + 60].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read3\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops3 = {
	.read = TX_NORMAL_DESC_descriptor_read3,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read4(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read4\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	};

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 80)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 80)
			, ptx_desc[desc_num + 80].tdes3,
			ptx_desc[desc_num + 80].tdes2,
			ptx_desc[desc_num + 80].tdes1,
			ptx_desc[desc_num + 80].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read4\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops4 = {
	.read = TX_NORMAL_DESC_descriptor_read4,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read5(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read5\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 100)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 100)
			, ptx_desc[desc_num + 100].tdes3,
			ptx_desc[desc_num + 100].tdes2,
			ptx_desc[desc_num + 100].tdes1,
			ptx_desc[desc_num + 100].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read5\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops5 = {
	.read = TX_NORMAL_DESC_descriptor_read5,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read6(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read6\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 120)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 120)
			, ptx_desc[desc_num + 120].tdes3,
			ptx_desc[desc_num + 120].tdes2,
			ptx_desc[desc_num + 120].tdes1,
			ptx_desc[desc_num + 120].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read6\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops6 = {
	.read = TX_NORMAL_DESC_descriptor_read6,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read7(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read7\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 140)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 140)
			, ptx_desc[desc_num + 140].tdes3,
			ptx_desc[desc_num + 140].tdes2,
			ptx_desc[desc_num + 140].tdes1,
			ptx_desc[desc_num + 140].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read7\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops7 = {
	.read = TX_NORMAL_DESC_descriptor_read7,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read8(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read8\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 160)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 160)
			, ptx_desc[desc_num + 160].tdes3,
			ptx_desc[desc_num + 160].tdes2,
			ptx_desc[desc_num + 160].tdes1,
			ptx_desc[desc_num + 160].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read8\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops8 = {
	.read = TX_NORMAL_DESC_descriptor_read8,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read9(struct file *file,
					       char __user *userbuf,
					       size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read9\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 180)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 180)
			, ptx_desc[desc_num + 180].tdes3,
			ptx_desc[desc_num + 180].tdes2,
			ptx_desc[desc_num + 180].tdes1,
			ptx_desc[desc_num + 180].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read9\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops9 = {
	.read = TX_NORMAL_DESC_descriptor_read9,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read10(struct file *file,
						char __user *userbuf,
						size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read10\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 200)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 200)
			, ptx_desc[desc_num + 200].tdes3,
			ptx_desc[desc_num + 200].tdes2,
			ptx_desc[desc_num + 200].tdes1,
			ptx_desc[desc_num + 200].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read10\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops10 = {
	.read = TX_NORMAL_DESC_descriptor_read10,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read11(struct file *file,
						char __user *userbuf,
						size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc = GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read11\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(12440, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 20; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 220)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 220)
			, ptx_desc[desc_num + 220].tdes3,
			ptx_desc[desc_num + 220].tdes2,
			ptx_desc[desc_num + 220].tdes1,
			ptx_desc[desc_num + 220].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read11\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops11 = {
	.read = TX_NORMAL_DESC_descriptor_read11,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_descriptor_read12(struct file *file,
						char __user *userbuf,
						size_t count, loff_t *ppos)
{
	struct s_tx_desc *ptx_desc =  GET_TX_DESC_PTR(qinx_val, 0);
	ssize_t ret = 0, desc_num = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;

	DBGPR("--> TX_NORMAL_DESC_descriptor_read12\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(9952, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';
	for (desc_num = 0; desc_num < 16; desc_num++) {
		sprintf(tmp_buf,
			"Channel %d Descriptor %d's [%#lx] contents are:\n"
			"tx_desc.tdes3             :%x\n"
			"tx_desc.tdes2             :%x\n"
			"tx_desc.tdes1             :%x\n"
			"tx_desc.tdes0             :%x\n",
			qinx_val, (int)(desc_num + 240)
			, (ULONG) GET_TX_DESC_DMA_ADDR(qinx_val, desc_num + 240)
			, ptx_desc[desc_num + 240].tdes3,
			ptx_desc[desc_num + 240].tdes2,
			ptx_desc[desc_num + 240].tdes1,
			ptx_desc[desc_num + 240].tdes0);
		strcat(debug_buf, tmp_buf);
	}
	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_descriptor_read12\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_desc_fops12 = {
	.read = TX_NORMAL_DESC_descriptor_read12,
	.write = descriptor_write,
};

static ssize_t TX_NORMAL_DESC_status_read(struct file *file,
					  char __user *userbuf, size_t count,
					  loff_t *ppos)
{
	struct tx_ring *ptx_ring =
	    GET_TX_WRAPPER_DESC(qinx_val);
	ssize_t ret = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;
	int i;
	/* shadow variables */
	unsigned int cur_tx = ptx_ring->cur_tx;
	unsigned int dirty_tx = ptx_ring->dirty_tx;
	unsigned int free_desc_cnt = ptx_ring->free_desc_cnt;
	unsigned int tx_pkt_queued = ptx_ring->tx_pkt_queued;

	unsigned int tmp_cur_tx = 0;
	unsigned int tmp_dirty_tx = 0;

	DBGPR("--> TX_NORMAL_DESC_status_read\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(9952, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';

	sprintf(debug_buf,
		"\nCHANNEL %d TX DESCRIPTORS STATUS......................\n\n",
		qinx_val);

	sprintf(tmp_buf, "TOTAL DESCRIPTOR COUNT                   : %d\n\n"
		"TOTAL FREE DESCRIPTOR COUNT              : %d\n\n"
		"TOTAL DESCRIPTOR QUEUED FOR TRANSMISSION : %d\n\n"
		"NEXT DESCRIPTOR TO BE USED BY DRIVER     : %d\n\n"
		"NEXT DESCRIPTOR TO BE USED BY DEVICE     : %d\n\n",
		TX_DESC_CNT, free_desc_cnt, tx_pkt_queued, cur_tx, dirty_tx);
	strcat(debug_buf, tmp_buf);

	/* Free tx descriptor index */
	if ((free_desc_cnt == TX_DESC_CNT) && (tx_pkt_queued == 0)) {
		sprintf(tmp_buf,
			"ALL %d DESCRIPTORS ARE FREE, HENCE NO PACKETS ARE QUEUED FOR TRANSMISSION\n",
			TX_DESC_CNT);
		strcat(debug_buf, tmp_buf);
	} else if ((free_desc_cnt == 0) && (tx_pkt_queued == TX_DESC_CNT)) {
		sprintf(tmp_buf,
			"ALL %d DESCRIPTORS ARE USED FOR TRANSMISSION, HENCE NO FREE DESCRIPTORS\n",
			TX_DESC_CNT);
		strcat(debug_buf, tmp_buf);
	} else {
		if (free_desc_cnt > 0) {
			tmp_cur_tx = cur_tx;
			tmp_dirty_tx = dirty_tx;
			i = 1;
			sprintf(tmp_buf,
				"FREE DESCRIPTORS INDEX(es) are           :\n\n");
			strcat(debug_buf, tmp_buf);
			if (tmp_cur_tx > tmp_dirty_tx) {
				for (; tmp_cur_tx < TX_DESC_CNT; tmp_cur_tx++) {
					sprintf(tmp_buf, "%d ", tmp_cur_tx);
					strcat(debug_buf, tmp_buf);
					if ((i % 16) == 0) {
						sprintf(tmp_buf, "\n");
						strcat(debug_buf, tmp_buf);
					}
					i++;
				}
				for (tmp_cur_tx = 0; tmp_cur_tx < tmp_dirty_tx;
				     tmp_cur_tx++) {
					sprintf(tmp_buf, "%d ", tmp_cur_tx);
					strcat(debug_buf, tmp_buf);
					if ((i % 16) == 0) {
						sprintf(tmp_buf, "\n");
						strcat(debug_buf, tmp_buf);
					}
					i++;
				}
			} else {	/* (tmp_cur_tx < tmp_dirty_tx) */
				for (; tmp_cur_tx > tmp_dirty_tx; ) {
					sprintf(tmp_buf, "%d ", tmp_cur_tx);
					strcat(debug_buf, tmp_buf);
					if ((i % 16) == 0) {
						sprintf(tmp_buf, "\n");
						strcat(debug_buf, tmp_buf);
					}
					i++;
					tmp_cur_tx++;
				}
			}
			sprintf(tmp_buf, "\n");
			strcat(debug_buf, tmp_buf);
		}

		if (tx_pkt_queued > 0) {
			tmp_cur_tx = cur_tx;
			tmp_dirty_tx = dirty_tx;
			i = 1;
			sprintf(tmp_buf,
				"\nUSED DESCRIPTORS INDEX(es) are           :\n\n");
			strcat(debug_buf, tmp_buf);
			if (tmp_dirty_tx < tmp_cur_tx) {
				for (; tmp_dirty_tx < tmp_cur_tx;
				     tmp_dirty_tx++) {
					sprintf(tmp_buf, "%d ", tmp_dirty_tx);
					strcat(debug_buf, tmp_buf);
					if ((i % 16) == 0) {
						sprintf(tmp_buf, "\n");
						strcat(debug_buf, tmp_buf);
					}
					i++;
				}
			} else {	/* (tmp_dirty_tx < tmp_cur_tx) */
				for (; tmp_dirty_tx > TX_DESC_CNT;
				     tmp_dirty_tx++) {
					sprintf(tmp_buf, "%d ", tmp_dirty_tx);
					strcat(debug_buf, tmp_buf);
					if ((i % 16) == 0) {
						sprintf(tmp_buf, "\n");
						strcat(debug_buf, tmp_buf);
					}
					i++;
				}
				for (tmp_dirty_tx = 0;
				     tmp_dirty_tx < tmp_cur_tx;
				     tmp_dirty_tx++) {
					sprintf(tmp_buf, "%d ", tmp_dirty_tx);
					strcat(debug_buf, tmp_buf);
					if ((i % 16) == 0) {
						sprintf(tmp_buf, "\n");
						strcat(debug_buf, tmp_buf);
					}
					i++;
				}
			}
			sprintf(tmp_buf, "\n");
			strcat(debug_buf, tmp_buf);
		}
	}
	sprintf(tmp_buf, "\n.........................................DONE\n\n");
	strcat(debug_buf, tmp_buf);

	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<-- TX_NORMAL_DESC_status_read\n");
	return ret;
}

static const struct file_operations TX_NORMAL_DESC_status_fops = {
	.read = TX_NORMAL_DESC_status_read,
};

static ssize_t RX_NORMAL_DESC_status_read(struct file *file,
					  char __user *userbuf, size_t count,
					  loff_t *ppos)
{
	struct rx_ring *prx_ring =
	    GET_RX_WRAPPER_DESC(qinx_val);
	ssize_t ret = 0;
	char *tmp_buf = NULL;
	char *debug_buf = NULL;
	uint64_t dma_chrdr;	/* head ptr */
	uint64_t dma_rdtp_rpdr;	/* tail ptr */
	uint64_t dma_rdlar = 0;	/* tail ptr */
	UINT tail_idx;
	UINT head_idx;
	UINT drv_desc_cnt = 0;
	UINT dev_desc_cnt = 0;
	unsigned int cur_rx = prx_ring->cur_rx;

	DBGPR("-->RX_NORMAL_DESC_status_read\n");

	tmp_buf = kmalloc(622, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	debug_buf = kmalloc(9952, GFP_KERNEL);
	if (!debug_buf) {
		kfree(tmp_buf);
		return -ENOMEM;
	}

	debug_buf[0] = '\0';

	cur_rx = prx_ring->cur_rx;
	DMA_CHRDR_RD(qinx_val, dma_chrdr);
	DMA_RDTP_RPDR_RD(qinx_val, dma_rdtp_rpdr);
	DMA_RDLAR_RD(qinx_val, dma_rdlar);

	tail_idx =
	    (dma_rdtp_rpdr - dma_rdlar) / sizeof(struct s_rx_desc);

	head_idx =
	    (dma_chrdr - dma_rdlar) / sizeof(struct s_rx_desc);

	if (tail_idx == head_idx) {
		dev_desc_cnt = 0;
		drv_desc_cnt = RX_DESC_CNT;
		pr_err("\nhead:[%d]%#llx tail:[%d]%#llx cur-rx:%d\n",
		       head_idx, dma_chrdr, tail_idx,
		       dma_rdtp_rpdr, cur_rx);
	} else if (head_idx > tail_idx) {	/* tail ptr is above head ptr */
		dev_desc_cnt = RX_DESC_CNT - (head_idx - tail_idx) + 1;
		drv_desc_cnt = RX_DESC_CNT - dev_desc_cnt;
	} else {		/* tail ptr is below head ptr */
		dev_desc_cnt = (tail_idx - head_idx + 1);
		drv_desc_cnt = RX_DESC_CNT - dev_desc_cnt;
	}

	sprintf(debug_buf,
		"\nCHANNEL %d : RX DESCRIPTORS STATUS....................\n\n",
		qinx_val);

	sprintf(tmp_buf, "TOTAL DESCRIPTOR COUNT                   : %d\n\n"
		"TOTAL DESCRIPTORS OWNED BY DEVICE        : %d\n\n"
		"TOTAL DESCRIPTOR OWNED BY DRIVER         : %d\n\n"
		"NEXT DESCRIPTOR TO BE USED BY DEVICE     : %d\n\n"
		"NEXT DESCRIPTOR TO BE USED BY DRIVER     : %d\n\n",
		RX_DESC_CNT, dev_desc_cnt, drv_desc_cnt, head_idx, cur_rx);
	strcat(debug_buf, tmp_buf);
	sprintf(tmp_buf, "\n.........................................DONE\n\n");
	strcat(debug_buf, tmp_buf);

	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(tmp_buf);
	kfree(debug_buf);

	DBGPR("<--RX_NORMAL_DESC_status_read\n");
	return ret;
}

static const struct file_operations RX_NORMAL_DESC_status_fops = {
	.read = RX_NORMAL_DESC_status_read,
};

static void bcm_regs_clause45_read(int dev, int reg, unsigned int *data)
{
	/* Write the desired MMD dev_addr */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, MMD_CTRL_REG, dev);

	/* Write the desired MMD reg_addr */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, MMD_ADDR_DATA_REG,
		reg);

	/* Select the Function : DATA with no post increment */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, MMD_CTRL_REG,
		dev | MMD_CTRL_FUNC_DATA_NOINCR);

	/* read the content of the MMD's selected register */
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MMD_ADDR_DATA_REG,
		data);
}


static void bcm_regs_clause45_write(int dev, int reg, unsigned int data)
{
	/* Write the desired MMD dev_addr */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, MMD_CTRL_REG, dev);

	/* Write the desired MMD reg_addr */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, MMD_ADDR_DATA_REG,
		reg);

	/* Select the Function : DATA with no post increment */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, MMD_CTRL_REG,
		dev | MMD_CTRL_FUNC_DATA_NOINCR);

	/* read the content of the MMD's selected register */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, MMD_ADDR_DATA_REG,
		data);
}

static ssize_t bcm_regs_read(struct file *file, char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	ssize_t ret;

	char *debug_buf = NULL;

	debug_buf = kmalloc(26820, GFP_KERNEL);
	if (!debug_buf)
		return -ENOMEM;


	/* For MII/GMII register read */
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR,
				     &mii_bmcr_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_BMSR,
				     &mii_bmsr_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID1,
				     &MII_PHYSID1_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID2,
				     &MII_PHYSID2_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ADVERTISE,
				     &mii_advertise_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_LPA,
				     &mii_lpa_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_EXPANSION,
				     &mii_expansion_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr,
				     EQOS_AUTO_NEGO_NP,
				     &auto_nego_np_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_CTRL1000,
				     &MII_CTRL1000_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_STAT1000,
				     &MII_STAT1000_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, MII_ESTATUS,
				     &mii_estatus_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, EQOS_PHY_CTL,
				     &phy_ctl_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, EQOS_PHY_STS,
				     &phy_sts_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_RX_ERR_CNT_REG,
					&bcm_rx_err_cnt_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_FALSE_CARR_CNT_REG,
					&bcm_false_carr_cnt_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_RX_NOT_OK_CNT_REG,
					&bcm_rx_not_ok_cnt_reg_val);

	/* read shadow regs */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, BCM_AUX_CTRL_SHADOW_REG,
			       ((BCM_10BASET_SHADOW_REG << 12) | 0x7));
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_AUX_CTRL_SHADOW_REG,
				&BCM_10baset_reg_val);
	eqos_mdio_write_direct(pdata, pdata->phyaddr, BCM_AUX_CTRL_SHADOW_REG,
			       ((BCM_POWER_MII_CTRL_SHADOW_REG << 12) | 0x7));
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_AUX_CTRL_SHADOW_REG,
				&bcm_power_mii_ctrl_reg_val);
	eqos_mdio_write_direct(pdata, pdata->phyaddr, BCM_AUX_CTRL_SHADOW_REG,
			       ((BCM_MISC_TEST_SHADOW_REG << 12) | 0x7));
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_AUX_CTRL_SHADOW_REG,
				&bcm_misc_test_reg_val);
	eqos_mdio_write_direct(pdata, pdata->phyaddr, BCM_AUX_CTRL_SHADOW_REG,
			       ((BCM_MISC_CTRL_SHADOW_REG << 12) | 0x7));
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_AUX_CTRL_SHADOW_REG,
				&bcm_misc_ctrl_reg_val);

	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_INT_STATUS_REG,
					&bcm_int_status_reg_val);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_INT_MASK_REG,
					&bcm_int_mask_reg_val);

	/* Read expansion regs */
	eqos_mdio_write_direct(pdata, pdata->phyaddr, BCM_EXPANSION_CTRL_REG,
					BCM_PKT_CNT_EXP_REG);
	eqos_mdio_read_direct(pdata, pdata->phyaddr, BCM_EXPANSION_REG,
				     &bcm_pkt_cnt_reg_val);
	eqos_mdio_write_direct(pdata, pdata->phyaddr, BCM_EXPANSION_CTRL_REG,
			       0x000);

	/* EEE advertisement regs */
	bcm_regs_clause45_read(MDIO_MMD_AN, CL45_ADV_EEE_REG,
		&bcm_eee_adv_reg_val);

	/* EEE resolution status regs */
	bcm_regs_clause45_read(MDIO_MMD_AN, CL45_RES_EEE_REG,
		&bcm_eee_res_sts_reg_val);

	/* EEE resolution status regs */
	bcm_regs_clause45_read(MDIO_MMD_AN, CL45_LPI_COUNTER_REG,
		&bcm_lpi_counter_reg_val);

	sprintf(debug_buf,
		"\nBroadCom Phy Regs\n"
		"Control(0x0)                        : %#x\n"
		"Status(0x1)                         : %#x\n"
		"Id1 (0x2)                           : %#x\n"
		"Id2 (0x3)                           : %#x\n"
		"Auto Neg Advertisement(0x4)         : %#x\n"
		"Auto Neg Link Partner Ability(0x5)  : %#x\n"
		"Auto Neg Expansion(0x6)             : %#x\n"
		"Auto Neg Next Page(0x7)             : %#x\n"
		"1000 Base-T Control(0x9)            : %#x\n"
		"1000 Base-T Status(0xa)             : %#x\n"
		"IEEE Extended Status(0xf)           : %#x\n"
		"Extended Control(0x10)              : %#x\n"
		"Extended Status(0x11)               : %#x\n"
		"Rx Error Count(0x12)                : %#x\n"
		"False Carrier Sense Count(0x13)     : %#x\n"
		"Rx Not Ok Count(0x14)               : %#x\n"
		"10BASE-T(0x18, Shadow 001)          : %#x\n"
		"Power/Mii Ctrl(0x18, Shadow 010)    : %#x\n"
		"Misc Test (0x18, Shadow 100)        : %#x\n"
		"Misc Ctrl(0x18, Shadow 111)         : %#x\n"
		"Int Status(0x1a)                    : %#x\n"
		"Int Mask(0x1b)                      : %#x\n"
		"Pkt Count(expansion reg 0xf00)      : %#x\n"
		"EEE advertisement(C45, Dev7, 0x3C)  : %#x\n"
		"EEE resolution (C45, Dev7, 0x803E)  : %#x\n"
		"LPI Mode Counter(C45, Dev7, 0x803F) : %#x\n",
		mii_bmcr_reg_val,
		mii_bmsr_reg_val,
		MII_PHYSID1_reg_val,
		MII_PHYSID2_reg_val,
		mii_advertise_reg_val,
		mii_lpa_reg_val,
		mii_expansion_reg_val,
		auto_nego_np_reg_val,
		MII_CTRL1000_reg_val,
		MII_STAT1000_reg_val,
		mii_estatus_reg_val,
		phy_ctl_reg_val,
		phy_sts_reg_val,
		bcm_rx_err_cnt_reg_val,
		bcm_false_carr_cnt_reg_val,
		bcm_rx_not_ok_cnt_reg_val,
		BCM_10baset_reg_val,
		bcm_pwr_mii_ctrl_reg_val,
		bcm_misc_test_reg_val,
		bcm_misc_ctrl_reg_val,
		bcm_int_status_reg_val,
		bcm_int_mask_reg_val,
		bcm_pkt_cnt_reg_val,
		bcm_eee_adv_reg_val,
		bcm_eee_res_sts_reg_val,
		bcm_lpi_counter_reg_val
		);

	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(debug_buf);
	return ret;
}

static const struct file_operations bcm_regs_fops = {
	.read = bcm_regs_read,
};

static ssize_t pre_padcal_err_regs_read(struct file *file,
		char __user *userbuf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;

	char *debug_buf = NULL;

	debug_buf = kmalloc(1024, GFP_KERNEL);
	if (!debug_buf)
		return -ENOMEM;

	sprintf(debug_buf,
		"\nError counter reg values before temp based recalibration\n"
		"total number of temp based recalibration %#lx\n"
		"tx_underflow_error(0x748): %#lx\n"
		"tx_carrier_error(0x760): %#lx\n"
		"tx_excessdef(0x76c): %#lx\n"
		"rx_crc_errror(0x794): %#lx\n"
		"rx_align_error(0x798): %#lx\n"
		"rx_run_error(0x79c): %#lx\n"
		"rx_jabber_error(0x7a0): %#lx\n"
		"rx_length_error(0x7c8): %#lx\n"
		"rx_outofrangetype(0x7cc): %#lx\n"
		"rx_fifo_overflow(0x7d4): %#lx\n"
		"rx_watchdog_error(0x7dc): %#lx\n"
		"rx_receive_error(0x7e0): %#lx\n"
		"rx_ipv4_hderr(0x814): %#lx\n"
		"rx_ipv6_hderr(0x828): %#lx\n"
		"rx_udp_err(0x834): %#lx\n"
		"rx_tcp_err(0x83c): %#lx\n"
		"rx_icmp_err(0x844): %#lx\n"
		"rx_ipv4_hderr(0x854): %#lx\n"
		"rx_ipv6_hderr(0x868): %#lx\n"
		"rx_udp_err(0x874): %#lx\n"
		"rx_tcp_err(0x87c): %#lx\n"
		"rx_icmp_err(0x884): %#lx\n",
		pdata->xstats.temp_pad_recalib_count,
		pdata->mmc.mmc_tx_underflow_error_pre_recalib,
		pdata->mmc.mmc_tx_carrier_error_pre_recalib,
		pdata->mmc.mmc_tx_excessdef_pre_recalib,
		pdata->mmc.mmc_rx_crc_errror_pre_recalib,
		pdata->mmc.mmc_rx_align_error_pre_recalib,
		pdata->mmc.mmc_rx_run_error_pre_recalib,
		pdata->mmc.mmc_rx_jabber_error_pre_recalib,
		pdata->mmc.mmc_rx_length_error_pre_recalib,
		pdata->mmc.mmc_rx_outofrangetype_pre_recalib,
		pdata->mmc.mmc_rx_fifo_overflow_pre_recalib,
		pdata->mmc.mmc_rx_watchdog_error_pre_recalib,
		pdata->mmc.mmc_rx_receive_error_pre_recalib,
		pdata->mmc.mmc_rx_ipv4_hderr_pre_recalib,
		pdata->mmc.mmc_rx_ipv6_hderr_octets_pre_recalib,
		pdata->mmc.mmc_rx_udp_err_pre_recalib,
		pdata->mmc.mmc_rx_tcp_err_pre_recalib,
		pdata->mmc.mmc_rx_icmp_err_pre_recalib,
		pdata->mmc.mmc_rx_ipv4_hderr_octets_pre_recalib,
		pdata->mmc.mmc_rx_ipv6_hderr_pre_recalib,
		pdata->mmc.mmc_rx_udp_err_octets_pre_recalib,
		pdata->mmc.mmc_rx_tcp_err_octets_pre_recalib,
		pdata->mmc.mmc_rx_icmp_err_octets_pre_recalib
		);

	/* reset the counters */
	pdata->mmc.mmc_tx_underflow_error_pre_recalib = 0;
	pdata->mmc.mmc_tx_carrier_error_pre_recalib = 0;
	pdata->mmc.mmc_tx_excessdef_pre_recalib = 0;
	pdata->mmc.mmc_rx_crc_errror_pre_recalib = 0;
	pdata->mmc.mmc_rx_align_error_pre_recalib = 0;
	pdata->mmc.mmc_rx_run_error_pre_recalib = 0;
	pdata->mmc.mmc_rx_jabber_error_pre_recalib = 0;
	pdata->mmc.mmc_rx_length_error_pre_recalib = 0;
	pdata->mmc.mmc_rx_outofrangetype_pre_recalib = 0;
	pdata->mmc.mmc_rx_fifo_overflow_pre_recalib = 0;
	pdata->mmc.mmc_rx_watchdog_error_pre_recalib = 0;
	pdata->mmc.mmc_rx_receive_error_pre_recalib = 0;
	pdata->mmc.mmc_rx_ipv4_hderr_pre_recalib = 0;
	pdata->mmc.mmc_rx_ipv6_hderr_octets_pre_recalib = 0;
	pdata->mmc.mmc_rx_udp_err_pre_recalib = 0;
	pdata->mmc.mmc_rx_tcp_err_pre_recalib = 0;
	pdata->mmc.mmc_rx_icmp_err_pre_recalib = 0;
	pdata->mmc.mmc_rx_ipv4_hderr_octets_pre_recalib = 0;
	pdata->mmc.mmc_rx_ipv6_hderr_pre_recalib = 0;
	pdata->mmc.mmc_rx_udp_err_octets_pre_recalib = 0;
	pdata->mmc.mmc_rx_tcp_err_octets_pre_recalib = 0;
	pdata->mmc.mmc_rx_icmp_err_octets_pre_recalib = 0;

	ret =
	    simple_read_from_buffer(userbuf, count, ppos, debug_buf,
				    strlen(debug_buf));
	kfree(debug_buf);
	return ret;
}

static const struct file_operations pre_padcal_err_regs_fops = {
	.read = pre_padcal_err_regs_read,
};

/*!
*  \brief  API to create debugfs files
*
* \details This function will creates debug files required for debugging.
* All debug files are created inside a directory named as 2490000.eqos
* (debugfs directory /sys/kernel/debug/2490000.eqos directory).
* Note: Before doing any read or write operation, debugfs has to be mounted
* on system.
* \retval  0 on Success.
* \retval  error number on Failure.
*/

int create_debug_files(void)
{
	int ret = 0;
	struct dentry *registers;
	struct dentry *MAC_MA32_127LR127;
	struct dentry *MAC_MA32_127LR126;
	struct dentry *MAC_MA32_127LR125;
	struct dentry *MAC_MA32_127LR124;
	struct dentry *MAC_MA32_127LR123;
	struct dentry *MAC_MA32_127LR122;
	struct dentry *MAC_MA32_127LR121;
	struct dentry *MAC_MA32_127LR120;
	struct dentry *MAC_MA32_127LR119;
	struct dentry *MAC_MA32_127LR118;
	struct dentry *MAC_MA32_127LR117;
	struct dentry *MAC_MA32_127LR116;
	struct dentry *MAC_MA32_127LR115;
	struct dentry *MAC_MA32_127LR114;
	struct dentry *MAC_MA32_127LR113;
	struct dentry *MAC_MA32_127LR112;
	struct dentry *MAC_MA32_127LR111;
	struct dentry *MAC_MA32_127LR110;
	struct dentry *MAC_MA32_127LR109;
	struct dentry *MAC_MA32_127LR108;
	struct dentry *MAC_MA32_127LR107;
	struct dentry *MAC_MA32_127LR106;
	struct dentry *MAC_MA32_127LR105;
	struct dentry *MAC_MA32_127LR104;
	struct dentry *MAC_MA32_127LR103;
	struct dentry *MAC_MA32_127LR102;
	struct dentry *MAC_MA32_127LR101;
	struct dentry *MAC_MA32_127LR100;
	struct dentry *MAC_MA32_127LR99;
	struct dentry *MAC_MA32_127LR98;
	struct dentry *MAC_MA32_127LR97;
	struct dentry *MAC_MA32_127LR96;
	struct dentry *MAC_MA32_127LR95;
	struct dentry *MAC_MA32_127LR94;
	struct dentry *MAC_MA32_127LR93;
	struct dentry *MAC_MA32_127LR92;
	struct dentry *MAC_MA32_127LR91;
	struct dentry *MAC_MA32_127LR90;
	struct dentry *MAC_MA32_127LR89;
	struct dentry *MAC_MA32_127LR88;
	struct dentry *MAC_MA32_127LR87;
	struct dentry *MAC_MA32_127LR86;
	struct dentry *MAC_MA32_127LR85;
	struct dentry *MAC_MA32_127LR84;
	struct dentry *MAC_MA32_127LR83;
	struct dentry *MAC_MA32_127LR82;
	struct dentry *MAC_MA32_127LR81;
	struct dentry *MAC_MA32_127LR80;
	struct dentry *MAC_MA32_127LR79;
	struct dentry *MAC_MA32_127LR78;
	struct dentry *MAC_MA32_127LR77;
	struct dentry *MAC_MA32_127LR76;
	struct dentry *MAC_MA32_127LR75;
	struct dentry *MAC_MA32_127LR74;
	struct dentry *MAC_MA32_127LR73;
	struct dentry *MAC_MA32_127LR72;
	struct dentry *MAC_MA32_127LR71;
	struct dentry *MAC_MA32_127LR70;
	struct dentry *MAC_MA32_127LR69;
	struct dentry *MAC_MA32_127LR68;
	struct dentry *MAC_MA32_127LR67;
	struct dentry *MAC_MA32_127LR66;
	struct dentry *MAC_MA32_127LR65;
	struct dentry *MAC_MA32_127LR64;
	struct dentry *MAC_MA32_127LR63;
	struct dentry *MAC_MA32_127LR62;
	struct dentry *MAC_MA32_127LR61;
	struct dentry *MAC_MA32_127LR60;
	struct dentry *MAC_MA32_127LR59;
	struct dentry *MAC_MA32_127LR58;
	struct dentry *MAC_MA32_127LR57;
	struct dentry *MAC_MA32_127LR56;
	struct dentry *MAC_MA32_127LR55;
	struct dentry *MAC_MA32_127LR54;
	struct dentry *MAC_MA32_127LR53;
	struct dentry *MAC_MA32_127LR52;
	struct dentry *MAC_MA32_127LR51;
	struct dentry *MAC_MA32_127LR50;
	struct dentry *MAC_MA32_127LR49;
	struct dentry *MAC_MA32_127LR48;
	struct dentry *MAC_MA32_127LR47;
	struct dentry *MAC_MA32_127LR46;
	struct dentry *MAC_MA32_127LR45;
	struct dentry *MAC_MA32_127LR44;
	struct dentry *MAC_MA32_127LR43;
	struct dentry *MAC_MA32_127LR42;
	struct dentry *MAC_MA32_127LR41;
	struct dentry *MAC_MA32_127LR40;
	struct dentry *MAC_MA32_127LR39;
	struct dentry *MAC_MA32_127LR38;
	struct dentry *MAC_MA32_127LR37;
	struct dentry *MAC_MA32_127LR36;
	struct dentry *MAC_MA32_127LR35;
	struct dentry *MAC_MA32_127LR34;
	struct dentry *MAC_MA32_127LR33;
	struct dentry *MAC_MA32_127LR32;
	struct dentry *MAC_MA32_127HR127;
	struct dentry *MAC_MA32_127HR126;
	struct dentry *MAC_MA32_127HR125;
	struct dentry *MAC_MA32_127HR124;
	struct dentry *MAC_MA32_127HR123;
	struct dentry *MAC_MA32_127HR122;
	struct dentry *MAC_MA32_127HR121;
	struct dentry *MAC_MA32_127HR120;
	struct dentry *MAC_MA32_127HR119;
	struct dentry *MAC_MA32_127HR118;
	struct dentry *MAC_MA32_127HR117;
	struct dentry *MAC_MA32_127HR116;
	struct dentry *MAC_MA32_127HR115;
	struct dentry *MAC_MA32_127HR114;
	struct dentry *MAC_MA32_127HR113;
	struct dentry *MAC_MA32_127HR112;
	struct dentry *MAC_MA32_127HR111;
	struct dentry *MAC_MA32_127HR110;
	struct dentry *MAC_MA32_127HR109;
	struct dentry *MAC_MA32_127HR108;
	struct dentry *MAC_MA32_127HR107;
	struct dentry *MAC_MA32_127HR106;
	struct dentry *MAC_MA32_127HR105;
	struct dentry *MAC_MA32_127HR104;
	struct dentry *MAC_MA32_127HR103;
	struct dentry *MAC_MA32_127HR102;
	struct dentry *MAC_MA32_127HR101;
	struct dentry *MAC_MA32_127HR100;
	struct dentry *MAC_MA32_127HR99;
	struct dentry *MAC_MA32_127HR98;
	struct dentry *MAC_MA32_127HR97;
	struct dentry *MAC_MA32_127HR96;
	struct dentry *MAC_MA32_127HR95;
	struct dentry *MAC_MA32_127HR94;
	struct dentry *MAC_MA32_127HR93;
	struct dentry *MAC_MA32_127HR92;
	struct dentry *MAC_MA32_127HR91;
	struct dentry *MAC_MA32_127HR90;
	struct dentry *MAC_MA32_127HR89;
	struct dentry *MAC_MA32_127HR88;
	struct dentry *MAC_MA32_127HR87;
	struct dentry *MAC_MA32_127HR86;
	struct dentry *MAC_MA32_127HR85;
	struct dentry *MAC_MA32_127HR84;
	struct dentry *MAC_MA32_127HR83;
	struct dentry *MAC_MA32_127HR82;
	struct dentry *MAC_MA32_127HR81;
	struct dentry *MAC_MA32_127HR80;
	struct dentry *MAC_MA32_127HR79;
	struct dentry *MAC_MA32_127HR78;
	struct dentry *MAC_MA32_127HR77;
	struct dentry *MAC_MA32_127HR76;
	struct dentry *MAC_MA32_127HR75;
	struct dentry *MAC_MA32_127HR74;
	struct dentry *MAC_MA32_127HR73;
	struct dentry *MAC_MA32_127HR72;
	struct dentry *MAC_MA32_127HR71;
	struct dentry *MAC_MA32_127HR70;
	struct dentry *MAC_MA32_127HR69;
	struct dentry *MAC_MA32_127HR68;
	struct dentry *MAC_MA32_127HR67;
	struct dentry *MAC_MA32_127HR66;
	struct dentry *MAC_MA32_127HR65;
	struct dentry *MAC_MA32_127HR64;
	struct dentry *MAC_MA32_127HR63;
	struct dentry *MAC_MA32_127HR62;
	struct dentry *MAC_MA32_127HR61;
	struct dentry *MAC_MA32_127HR60;
	struct dentry *MAC_MA32_127HR59;
	struct dentry *MAC_MA32_127HR58;
	struct dentry *MAC_MA32_127HR57;
	struct dentry *MAC_MA32_127HR56;
	struct dentry *MAC_MA32_127HR55;
	struct dentry *MAC_MA32_127HR54;
	struct dentry *MAC_MA32_127HR53;
	struct dentry *MAC_MA32_127HR52;
	struct dentry *MAC_MA32_127HR51;
	struct dentry *MAC_MA32_127HR50;
	struct dentry *MAC_MA32_127HR49;
	struct dentry *MAC_MA32_127HR48;
	struct dentry *MAC_MA32_127HR47;
	struct dentry *MAC_MA32_127HR46;
	struct dentry *MAC_MA32_127HR45;
	struct dentry *MAC_MA32_127HR44;
	struct dentry *MAC_MA32_127HR43;
	struct dentry *MAC_MA32_127HR42;
	struct dentry *MAC_MA32_127HR41;
	struct dentry *MAC_MA32_127HR40;
	struct dentry *MAC_MA32_127HR39;
	struct dentry *MAC_MA32_127HR38;
	struct dentry *MAC_MA32_127HR37;
	struct dentry *MAC_MA32_127HR36;
	struct dentry *MAC_MA32_127HR35;
	struct dentry *MAC_MA32_127HR34;
	struct dentry *MAC_MA32_127HR33;
	struct dentry *MAC_MA32_127HR32;
	struct dentry *MAC_MA1_31LR31;
	struct dentry *MAC_MA1_31LR30;
	struct dentry *MAC_MA1_31LR29;
	struct dentry *MAC_MA1_31LR28;
	struct dentry *MAC_MA1_31LR27;
	struct dentry *MAC_MA1_31LR26;
	struct dentry *MAC_MA1_31LR25;
	struct dentry *MAC_MA1_31LR24;
	struct dentry *MAC_MA1_31LR23;
	struct dentry *MAC_MA1_31LR22;
	struct dentry *MAC_MA1_31LR21;
	struct dentry *MAC_MA1_31LR20;
	struct dentry *MAC_MA1_31LR19;
	struct dentry *MAC_MA1_31LR18;
	struct dentry *MAC_MA1_31LR17;
	struct dentry *MAC_MA1_31LR16;
	struct dentry *MAC_MA1_31LR15;
	struct dentry *MAC_MA1_31LR14;
	struct dentry *MAC_MA1_31LR13;
	struct dentry *MAC_MA1_31LR12;
	struct dentry *MAC_MA1_31LR11;
	struct dentry *MAC_MA1_31LR10;
	struct dentry *MAC_MA1_31LR9;
	struct dentry *MAC_MA1_31LR8;
	struct dentry *MAC_MA1_31LR7;
	struct dentry *MAC_MA1_31LR6;
	struct dentry *MAC_MA1_31LR5;
	struct dentry *MAC_MA1_31LR4;
	struct dentry *MAC_MA1_31LR3;
	struct dentry *MAC_MA1_31LR2;
	struct dentry *MAC_MA1_31LR1;
	struct dentry *MAC_MA1_31HR31;
	struct dentry *MAC_MA1_31HR30;
	struct dentry *MAC_MA1_31HR29;
	struct dentry *MAC_MA1_31HR28;
	struct dentry *MAC_MA1_31HR27;
	struct dentry *MAC_MA1_31HR26;
	struct dentry *MAC_MA1_31HR25;
	struct dentry *MAC_MA1_31HR24;
	struct dentry *MAC_MA1_31HR23;
	struct dentry *MAC_MA1_31HR22;
	struct dentry *MAC_MA1_31HR21;
	struct dentry *MAC_MA1_31HR20;
	struct dentry *MAC_MA1_31HR19;
	struct dentry *MAC_MA1_31HR18;
	struct dentry *MAC_MA1_31HR17;
	struct dentry *MAC_MA1_31HR16;
	struct dentry *MAC_MA1_31HR15;
	struct dentry *MAC_MA1_31HR14;
	struct dentry *MAC_MA1_31HR13;
	struct dentry *MAC_MA1_31HR12;
	struct dentry *MAC_MA1_31HR11;
	struct dentry *MAC_MA1_31HR10;
	struct dentry *MAC_MA1_31HR9;
	struct dentry *MAC_MA1_31HR8;
	struct dentry *MAC_MA1_31HR7;
	struct dentry *MAC_MA1_31HR6;
	struct dentry *MAC_MA1_31HR5;
	struct dentry *MAC_MA1_31HR4;
	struct dentry *MAC_MA1_31HR3;
	struct dentry *MAC_MA1_31HR2;
	struct dentry *MAC_MA1_31HR1;
	struct dentry *MAC_ARPA;
	struct dentry *MAC_L3A3R7;
	struct dentry *MAC_L3A3R6;
	struct dentry *MAC_L3A3R5;
	struct dentry *MAC_L3A3R4;
	struct dentry *MAC_L3A3R3;
	struct dentry *MAC_L3A3R2;
	struct dentry *MAC_L3A3R1;
	struct dentry *MAC_L3A3R0;
	struct dentry *MAC_L3A2R7;
	struct dentry *MAC_L3A2R6;
	struct dentry *MAC_L3A2R5;
	struct dentry *MAC_L3A2R4;
	struct dentry *MAC_L3A2R3;
	struct dentry *MAC_L3A2R2;
	struct dentry *MAC_L3A2R1;
	struct dentry *MAC_L3A2R0;
	struct dentry *MAC_L3A1R7;
	struct dentry *MAC_L3A1R6;
	struct dentry *MAC_L3A1R5;
	struct dentry *MAC_L3A1R4;
	struct dentry *MAC_L3A1R3;
	struct dentry *MAC_L3A1R2;
	struct dentry *MAC_L3A1R1;
	struct dentry *MAC_L3A1R0;
	struct dentry *MAC_L3A0R7;
	struct dentry *MAC_L3A0R6;
	struct dentry *MAC_L3A0R5;
	struct dentry *MAC_L3A0R4;
	struct dentry *MAC_L3A0R3;
	struct dentry *MAC_L3A0R2;
	struct dentry *MAC_L3A0R1;
	struct dentry *MAC_L3A0R0;
	struct dentry *MAC_L4AR7;
	struct dentry *MAC_L4AR6;
	struct dentry *MAC_L4AR5;
	struct dentry *MAC_L4AR4;
	struct dentry *MAC_L4AR3;
	struct dentry *MAC_L4AR2;
	struct dentry *MAC_L4AR1;
	struct dentry *MAC_L4AR0;
	struct dentry *MAC_L3L4CR7;
	struct dentry *MAC_L3L4CR6;
	struct dentry *MAC_L3L4CR5;
	struct dentry *MAC_L3L4CR4;
	struct dentry *MAC_L3L4CR3;
	struct dentry *MAC_L3L4CR2;
	struct dentry *MAC_L3L4CR1;
	struct dentry *MAC_L3L4CR0;
	struct dentry *MAC_GPIOS;
	struct dentry *MAC_PCS;
	struct dentry *MAC_TES;
	struct dentry *MAC_AE;
	struct dentry *MAC_ALPA;
	struct dentry *MAC_AAD;
	struct dentry *MAC_ANS;
	struct dentry *MAC_ANC;
	struct dentry *MAC_LPC;
	struct dentry *MAC_LPS;
	struct dentry *MAC_LMIR;
	struct dentry *MAC_SPI2R;
	struct dentry *MAC_SPI1R;
	struct dentry *MAC_SPI0R;
	struct dentry *MAC_PTO_CR;
	struct dentry *MAC_PPS_WIDTH3;
	struct dentry *MAC_PPS_WIDTH2;
	struct dentry *MAC_PPS_WIDTH1;
	struct dentry *MAC_PPS_WIDTH0;
	struct dentry *MAC_PPS_INTVAL3;
	struct dentry *MAC_PPS_INTVAL2;
	struct dentry *MAC_PPS_INTVAL1;
	struct dentry *MAC_PPS_INTVAL0;
	struct dentry *MAC_PPS_TTNS3;
	struct dentry *MAC_PPS_TTNS2;
	struct dentry *MAC_PPS_TTNS1;
	struct dentry *MAC_PPS_TTNS0;
	struct dentry *MAC_PPS_TTS3;
	struct dentry *MAC_PPS_TTS2;
	struct dentry *MAC_PPS_TTS1;
	struct dentry *MAC_PPS_TTS0;
	struct dentry *MAC_PPSC;
	struct dentry *MAC_TEAC;
	struct dentry *MAC_TIAC;
	struct dentry *MAC_ATS;
	struct dentry *MAC_ATN;
	struct dentry *MAC_AC;
	struct dentry *MAC_TTN;
	struct dentry *MAC_TTSN;
	struct dentry *MAC_TSR;
	struct dentry *MAC_STHWR;
	struct dentry *MAC_TAR;
	struct dentry *MAC_STNSUR;
	struct dentry *MAC_STSUR;
	struct dentry *MAC_STNSR;
	struct dentry *MAC_STSR;
	struct dentry *MAC_SSIR;
	struct dentry *MAC_TCR;
	struct dentry *MTL_DSR;
	struct dentry *MAC_RWPFFR;
	struct dentry *MAC_RTSR;
	struct dentry *MTL_IER;
	struct dentry *MTL_QRCR7;
	struct dentry *MTL_QRCR6;
	struct dentry *MTL_QRCR5;
	struct dentry *MTL_QRCR4;
	struct dentry *MTL_QRCR3;
	struct dentry *MTL_QRCR2;
	struct dentry *MTL_QRCR1;
	struct dentry *MTL_QRDR7;
	struct dentry *MTL_QRDR6;
	struct dentry *MTL_QRDR5;
	struct dentry *MTL_QRDR4;
	struct dentry *MTL_QRDR3;
	struct dentry *MTL_QRDR2;
	struct dentry *MTL_QRDR1;
	struct dentry *MTL_QOCR7;
	struct dentry *MTL_QOCR6;
	struct dentry *MTL_QOCR5;
	struct dentry *MTL_QOCR4;
	struct dentry *MTL_QOCR3;
	struct dentry *MTL_QOCR2;
	struct dentry *MTL_QOCR1;
	struct dentry *MTL_QROMR7;
	struct dentry *MTL_QROMR6;
	struct dentry *MTL_QROMR5;
	struct dentry *MTL_QROMR4;
	struct dentry *MTL_QROMR3;
	struct dentry *MTL_QROMR2;
	struct dentry *MTL_QROMR1;
	struct dentry *MTL_QLCR7;
	struct dentry *MTL_QLCR6;
	struct dentry *MTL_QLCR5;
	struct dentry *MTL_QLCR4;
	struct dentry *MTL_QLCR3;
	struct dentry *MTL_QLCR2;
	struct dentry *MTL_QLCR1;
	struct dentry *MTL_QHCR7;
	struct dentry *MTL_QHCR6;
	struct dentry *MTL_QHCR5;
	struct dentry *MTL_QHCR4;
	struct dentry *MTL_QHCR3;
	struct dentry *MTL_QHCR2;
	struct dentry *MTL_QHCR1;
	struct dentry *MTL_QSSCR7;
	struct dentry *MTL_QSSCR6;
	struct dentry *MTL_QSSCR5;
	struct dentry *MTL_QSSCR4;
	struct dentry *MTL_QSSCR3;
	struct dentry *MTL_QSSCR2;
	struct dentry *MTL_QSSCR1;
	struct dentry *MTL_QW7;
	struct dentry *MTL_QW6;
	struct dentry *MTL_QW5;
	struct dentry *MTL_QW4;
	struct dentry *MTL_QW3;
	struct dentry *MTL_QW2;
	struct dentry *MTL_QW1;
	struct dentry *MTL_QESR7;
	struct dentry *MTL_QESR6;
	struct dentry *MTL_QESR5;
	struct dentry *MTL_QESR4;
	struct dentry *MTL_QESR3;
	struct dentry *MTL_QESR2;
	struct dentry *MTL_QESR1;
	struct dentry *MTL_QECR7;
	struct dentry *MTL_QECR6;
	struct dentry *MTL_QECR5;
	struct dentry *MTL_QECR4;
	struct dentry *MTL_QECR3;
	struct dentry *MTL_QECR2;
	struct dentry *MTL_QECR1;
	struct dentry *MTL_QTDR7;
	struct dentry *MTL_QTDR6;
	struct dentry *MTL_QTDR5;
	struct dentry *MTL_QTDR4;
	struct dentry *MTL_QTDR3;
	struct dentry *MTL_QTDR2;
	struct dentry *MTL_QTDR1;
	struct dentry *MTL_QUCR7;
	struct dentry *MTL_QUCR6;
	struct dentry *MTL_QUCR5;
	struct dentry *MTL_QUCR4;
	struct dentry *MTL_QUCR3;
	struct dentry *MTL_QUCR2;
	struct dentry *MTL_QUCR1;
	struct dentry *MTL_QTOMR7;
	struct dentry *MTL_QTOMR6;
	struct dentry *MTL_QTOMR5;
	struct dentry *MTL_QTOMR4;
	struct dentry *MTL_QTOMR3;
	struct dentry *MTL_QTOMR2;
	struct dentry *MTL_QTOMR1;
	struct dentry *MAC_PMTCSR;
	struct dentry *MMC_RXICMP_ERR_OCTETS;
	struct dentry *MMC_RXICMP_GD_OCTETS;
	struct dentry *MMC_RXTCP_ERR_OCTETS;
	struct dentry *MMC_RXTCP_GD_OCTETS;
	struct dentry *MMC_RXUDP_ERR_OCTETS;
	struct dentry *MMC_RXUDP_GD_OCTETS;
	struct dentry *MMC_RXIPV6_NOPAY_OCTETS;
	struct dentry *MMC_RXIPV6_HDRERR_OCTETS;
	struct dentry *MMC_RXIPV6_GD_OCTETS;
	struct dentry *MMC_RXIPV4_UDSBL_OCTETS;
	struct dentry *MMC_RXIPV4_FRAG_OCTETS;
	struct dentry *MMC_RXIPV4_NOPAY_OCTETS;
	struct dentry *MMC_RXIPV4_HDRERR_OCTETS;
	struct dentry *MMC_RXIPV4_GD_OCTETS;
	struct dentry *MMC_RXICMP_ERR_PKTS;
	struct dentry *MMC_RXICMP_GD_PKTS;
	struct dentry *MMC_RXTCP_ERR_PKTS;
	struct dentry *MMC_RXTCP_GD_PKTS;
	struct dentry *MMC_RXUDP_ERR_PKTS;
	struct dentry *MMC_RXUDP_GD_PKTS;
	struct dentry *MMC_RXIPV6_NOPAY_PKTS;
	struct dentry *MMC_RXIPV6_HDRERR_PKTS;
	struct dentry *MMC_RXIPV6_GD_PKTS;
	struct dentry *MMC_RXIPV4_UBSBL_PKTS;
	struct dentry *MMC_RXIPV4_FRAG_PKTS;
	struct dentry *MMC_RXIPV4_NOPAY_PKTS;
	struct dentry *MMC_RXIPV4_HDRERR_PKTS;
	struct dentry *MMC_RXIPV4_GD_PKTS;
	struct dentry *MMC_RXCTRLPACKETS_G;
	struct dentry *MMC_RXRCVERROR;
	struct dentry *MMC_RXWATCHDOGERROR;
	struct dentry *MMC_RXVLANPACKETS_GB;
	struct dentry *MMC_RXFIFOOVERFLOW;
	struct dentry *MMC_RXPAUSEPACKETS;
	struct dentry *MMC_RXOUTOFRANGETYPE;
	struct dentry *MMC_RXLENGTHERROR;
	struct dentry *MMC_RXUNICASTPACKETS_G;
	struct dentry *MMC_RX1024TOMAXOCTETS_GB;
	struct dentry *MMC_RX512TO1023OCTETS_GB;
	struct dentry *MMC_RX256TO511OCTETS_GB;
	struct dentry *MMC_RX128TO255OCTETS_GB;
	struct dentry *MMC_RX65TO127OCTETS_GB;
	struct dentry *MMC_RX64OCTETS_GB;
	struct dentry *MMC_RXOVERSIZE_G;
	struct dentry *MMC_RXUNDERSIZE_G;
	struct dentry *MMC_RXJABBERERROR;
	struct dentry *MMC_RXRUNTERROR;
	struct dentry *MMC_RXALIGNMENTERROR;
	struct dentry *MMC_RXCRCERROR;
	struct dentry *MMC_RXMULTICASTPACKETS_G;
	struct dentry *MMC_RXBROADCASTPACKETS_G;
	struct dentry *MMC_RXOCTETCOUNT_G;
	struct dentry *MMC_RXOCTETCOUNT_GB;
	struct dentry *MMC_RXPACKETCOUNT_GB;
	struct dentry *MMC_TXOVERSIZE_G;
	struct dentry *MMC_TXVLANPACKETS_G;
	struct dentry *MMC_TXPAUSEPACKETS;
	struct dentry *MMC_TXEXCESSDEF;
	struct dentry *MMC_TXPACKETSCOUNT_G;
	struct dentry *MMC_TXOCTETCOUNT_G;
	struct dentry *MMC_TXCARRIERERROR;
	struct dentry *MMC_TXEXESSCOL;
	struct dentry *MMC_TXLATECOL;
	struct dentry *MMC_TXDEFERRED;
	struct dentry *MMC_TXMULTICOL_G;
	struct dentry *MMC_TXSINGLECOL_G;
	struct dentry *MMC_TXUNDERFLOWERROR;
	struct dentry *MMC_TXBROADCASTPACKETS_GB;
	struct dentry *MMC_TXMULTICASTPACKETS_GB;
	struct dentry *MMC_TXUNICASTPACKETS_GB;
	struct dentry *MMC_TX1024TOMAXOCTETS_GB;
	struct dentry *MMC_TX512TO1023OCTETS_GB;
	struct dentry *MMC_TX256TO511OCTETS_GB;
	struct dentry *MMC_TX128TO255OCTETS_GB;
	struct dentry *MMC_TX65TO127OCTETS_GB;
	struct dentry *MMC_TX64OCTETS_GB;
	struct dentry *MMC_TXMULTICASTPACKETS_G;
	struct dentry *MMC_TXBROADCASTPACKETS_G;
	struct dentry *MMC_TXPACKETCOUNT_GB;
	struct dentry *MMC_TXOCTETCOUNT_GB;
	struct dentry *MMC_IPC_INTR_RX;
	struct dentry *MMC_IPC_INTR_MASK_RX;
	struct dentry *MMC_INTR_MASK_TX;
	struct dentry *MMC_INTR_MASK_RX;
	struct dentry *MMC_INTR_TX;
	struct dentry *MMC_INTR_RX;
	struct dentry *MMC_CNTRL;
	struct dentry *MAC_MA1LR;
	struct dentry *MAC_MA1HR;
	struct dentry *MAC_MA0LR;
	struct dentry *MAC_MA0HR;
	struct dentry *MAC_GPIOR;
	struct dentry *MAC_GMIIDR;
	struct dentry *MAC_GMIIAR;
	struct dentry *MAC_HFR2;
	struct dentry *MAC_HFR1;
	struct dentry *MAC_HFR0;
	struct dentry *MAC_MDR;
	struct dentry *MAC_VR;
	struct dentry *MAC_HTR7;
	struct dentry *MAC_HTR6;
	struct dentry *MAC_HTR5;
	struct dentry *MAC_HTR4;
	struct dentry *MAC_HTR3;
	struct dentry *MAC_HTR2;
	struct dentry *MAC_HTR1;
	struct dentry *MAC_HTR0;
	struct dentry *DMA_RIWTR7;
	struct dentry *DMA_RIWTR6;
	struct dentry *DMA_RIWTR5;
	struct dentry *DMA_RIWTR4;
	struct dentry *DMA_RIWTR3;
	struct dentry *DMA_RIWTR2;
	struct dentry *DMA_RIWTR1;
	struct dentry *DMA_RIWTR0;
	struct dentry *DMA_RDRLR7;
	struct dentry *DMA_RDRLR6;
	struct dentry *DMA_RDRLR5;
	struct dentry *DMA_RDRLR4;
	struct dentry *DMA_RDRLR3;
	struct dentry *DMA_RDRLR2;
	struct dentry *DMA_RDRLR1;
	struct dentry *DMA_RDRLR0;
	struct dentry *DMA_TDRLR7;
	struct dentry *DMA_TDRLR6;
	struct dentry *DMA_TDRLR5;
	struct dentry *DMA_TDRLR4;
	struct dentry *DMA_TDRLR3;
	struct dentry *DMA_TDRLR2;
	struct dentry *DMA_TDRLR1;
	struct dentry *DMA_TDRLR0;
	struct dentry *DMA_RDTP_RPDR7;
	struct dentry *DMA_RDTP_RPDR6;
	struct dentry *DMA_RDTP_RPDR5;
	struct dentry *DMA_RDTP_RPDR4;
	struct dentry *DMA_RDTP_RPDR3;
	struct dentry *DMA_RDTP_RPDR2;
	struct dentry *DMA_RDTP_RPDR1;
	struct dentry *DMA_RDTP_RPDR0;
	struct dentry *DMA_TDTP_TPDR7;
	struct dentry *DMA_TDTP_TPDR6;
	struct dentry *DMA_TDTP_TPDR5;
	struct dentry *DMA_TDTP_TPDR4;
	struct dentry *DMA_TDTP_TPDR3;
	struct dentry *DMA_TDTP_TPDR2;
	struct dentry *DMA_TDTP_TPDR1;
	struct dentry *DMA_TDTP_TPDR0;
	struct dentry *DMA_RDLAR7;
	struct dentry *DMA_RDLAR6;
	struct dentry *DMA_RDLAR5;
	struct dentry *DMA_RDLAR4;
	struct dentry *DMA_RDLAR3;
	struct dentry *DMA_RDLAR2;
	struct dentry *DMA_RDLAR1;
	struct dentry *DMA_RDLAR0;
	struct dentry *DMA_TDLAR7;
	struct dentry *DMA_TDLAR6;
	struct dentry *DMA_TDLAR5;
	struct dentry *DMA_TDLAR4;
	struct dentry *DMA_TDLAR3;
	struct dentry *DMA_TDLAR2;
	struct dentry *DMA_TDLAR1;
	struct dentry *DMA_TDLAR0;
	struct dentry *DMA_IER7;
	struct dentry *DMA_IER6;
	struct dentry *DMA_IER5;
	struct dentry *DMA_IER4;
	struct dentry *DMA_IER3;
	struct dentry *DMA_IER2;
	struct dentry *DMA_IER1;
	struct dentry *DMA_IER0;
	struct dentry *MAC_IMR;
	struct dentry *MAC_ISR;
	struct dentry *MTL_ISR;
	struct dentry *DMA_SR7;
	struct dentry *DMA_SR6;
	struct dentry *DMA_SR5;
	struct dentry *DMA_SR4;
	struct dentry *DMA_SR3;
	struct dentry *DMA_SR2;
	struct dentry *DMA_SR1;
	struct dentry *DMA_SR0;
	struct dentry *DMA_ISR;
	struct dentry *DMA_DSR2;
	struct dentry *DMA_DSR1;
	struct dentry *DMA_DSR0;
	struct dentry *MTL_Q0RDR;
	struct dentry *MTL_Q0ESR;
	struct dentry *MTL_Q0TDR;
	struct dentry *DMA_CHRBAR7;
	struct dentry *DMA_CHRBAR6;
	struct dentry *DMA_CHRBAR5;
	struct dentry *DMA_CHRBAR4;
	struct dentry *DMA_CHRBAR3;
	struct dentry *DMA_CHRBAR2;
	struct dentry *DMA_CHRBAR1;
	struct dentry *DMA_CHRBAR0;
	struct dentry *DMA_CHTBAR7;
	struct dentry *DMA_CHTBAR6;
	struct dentry *DMA_CHTBAR5;
	struct dentry *DMA_CHTBAR4;
	struct dentry *DMA_CHTBAR3;
	struct dentry *DMA_CHTBAR2;
	struct dentry *DMA_CHTBAR1;
	struct dentry *DMA_CHTBAR0;
	struct dentry *DMA_CHRDR7;
	struct dentry *DMA_CHRDR6;
	struct dentry *DMA_CHRDR5;
	struct dentry *DMA_CHRDR4;
	struct dentry *DMA_CHRDR3;
	struct dentry *DMA_CHRDR2;
	struct dentry *DMA_CHRDR1;
	struct dentry *DMA_CHRDR0;
	struct dentry *DMA_CHTDR7;
	struct dentry *DMA_CHTDR6;
	struct dentry *DMA_CHTDR5;
	struct dentry *DMA_CHTDR4;
	struct dentry *DMA_CHTDR3;
	struct dentry *DMA_CHTDR2;
	struct dentry *DMA_CHTDR1;
	struct dentry *DMA_CHTDR0;
	struct dentry *DMA_SFCSR7;
	struct dentry *DMA_SFCSR6;
	struct dentry *DMA_SFCSR5;
	struct dentry *DMA_SFCSR4;
	struct dentry *DMA_SFCSR3;
	struct dentry *DMA_SFCSR2;
	struct dentry *DMA_SFCSR1;
	struct dentry *DMA_SFCSR0;
	struct dentry *MAC_IVLANTIRR;
	struct dentry *MAC_VLANTIRR;
	struct dentry *MAC_VLANHTR;
	struct dentry *MAC_VLANTR;
	struct dentry *DMA_SBUS;
	struct dentry *DMA_BMR;
	struct dentry *MTL_Q0RCR;
	struct dentry *MTL_Q0OCR;
	struct dentry *MTL_Q0ROMR;
	struct dentry *MTL_Q0QR;
	struct dentry *MTL_Q0ECR;
	struct dentry *MTL_Q0UCR;
	struct dentry *MTL_Q0TOMR;
	struct dentry *MTL_RQDCM1R;
	struct dentry *MTL_RQDCM0R;
	struct dentry *MTL_FDDR;
	struct dentry *MTL_FDACS;
	struct dentry *MTL_OMR;
	struct dentry *MAC_RQC3R;
	struct dentry *MAC_RQC2R;
	struct dentry *MAC_RQC1R;
	struct dentry *MAC_RQC0R;
	struct dentry *MAC_TQPM1R;
	struct dentry *MAC_TQPM0R;
	struct dentry *MAC_RFCR;
	struct dentry *MAC_QTFCR7;
	struct dentry *MAC_QTFCR6;
	struct dentry *MAC_QTFCR5;
	struct dentry *MAC_QTFCR4;
	struct dentry *MAC_QTFCR3;
	struct dentry *MAC_QTFCR2;
	struct dentry *MAC_QTFCR1;
	struct dentry *MAC_Q0TFCR;
	struct dentry *DMA_AXI4CR7;
	struct dentry *DMA_AXI4CR6;
	struct dentry *DMA_AXI4CR5;
	struct dentry *DMA_AXI4CR4;
	struct dentry *DMA_AXI4CR3;
	struct dentry *DMA_AXI4CR2;
	struct dentry *DMA_AXI4CR1;
	struct dentry *DMA_AXI4CR0;
	struct dentry *DMA_RCR7;
	struct dentry *DMA_RCR6;
	struct dentry *DMA_RCR5;
	struct dentry *DMA_RCR4;
	struct dentry *DMA_RCR3;
	struct dentry *DMA_RCR2;
	struct dentry *DMA_RCR1;
	struct dentry *DMA_RCR0;
	struct dentry *DMA_TCR7;
	struct dentry *DMA_TCR6;
	struct dentry *DMA_TCR5;
	struct dentry *DMA_TCR4;
	struct dentry *DMA_TCR3;
	struct dentry *DMA_TCR2;
	struct dentry *DMA_TCR1;
	struct dentry *DMA_TCR0;
	struct dentry *DMA_CR7;
	struct dentry *DMA_CR6;
	struct dentry *DMA_CR5;
	struct dentry *DMA_CR4;
	struct dentry *DMA_CR3;
	struct dentry *DMA_CR2;
	struct dentry *DMA_CR1;
	struct dentry *DMA_CR0;
	struct dentry *MAC_WTR;
	struct dentry *MAC_MPFR;
	struct dentry *MAC_MECR;
	struct dentry *MAC_MCR;
	/* MII/GMII registers */
	struct dentry *MII_BMCR_REG;
	struct dentry *MII_BMSR_REG;
	struct dentry *MII_PHYSID1_REG;
	struct dentry *MII_PHYSID2_REG;
	struct dentry *MII_ADVERTISE_REG;
	struct dentry *MII_LPA_REG;
	struct dentry *MII_EXPANSION_REG;
	struct dentry *AUTO_NEGO_NP_REG;
	struct dentry *MII_ESTATUS_REG;
	struct dentry *MII_CTRL1000_REG;
	struct dentry *MII_STAT1000_REG;
	struct dentry *PHY_CTL_REG;
	struct dentry *PHY_STS_REG;
	struct dentry *feature_drop_tx_pktburstcnt;
	struct dentry *qinx;

	struct dentry *reg_offset;
	struct dentry *gen_reg;
	struct dentry *do_tx_align_tst;

	struct dentry *RX_NORMAL_DESC_desc0;
	struct dentry *RX_NORMAL_DESC_desc1;
	struct dentry *RX_NORMAL_DESC_desc2;
	struct dentry *RX_NORMAL_DESC_desc3;
	struct dentry *RX_NORMAL_DESC_desc4;
	struct dentry *RX_NORMAL_DESC_desc5;
	struct dentry *RX_NORMAL_DESC_desc6;
	struct dentry *RX_NORMAL_DESC_desc7;
	struct dentry *RX_NORMAL_DESC_desc8;
	struct dentry *RX_NORMAL_DESC_desc9;
	struct dentry *RX_NORMAL_DESC_desc10;
	struct dentry *RX_NORMAL_DESC_desc11;
	struct dentry *RX_NORMAL_DESC_desc12;
	struct dentry *TX_NORMAL_DESC_desc0;
	struct dentry *TX_NORMAL_DESC_desc1;
	struct dentry *TX_NORMAL_DESC_desc2;
	struct dentry *TX_NORMAL_DESC_desc3;
	struct dentry *TX_NORMAL_DESC_desc4;
	struct dentry *TX_NORMAL_DESC_desc5;
	struct dentry *TX_NORMAL_DESC_desc6;
	struct dentry *TX_NORMAL_DESC_desc7;
	struct dentry *TX_NORMAL_DESC_desc8;
	struct dentry *TX_NORMAL_DESC_desc9;
	struct dentry *TX_NORMAL_DESC_desc10;
	struct dentry *TX_NORMAL_DESC_desc11;
	struct dentry *TX_NORMAL_DESC_desc12;
	struct dentry *TX_NORMAL_DESC_STATUS;
	struct dentry *RX_NORMAL_DESC_STATUS;
	struct dentry *BCM_REGS;
	struct dentry *pre_padcal_err_counters;

	DBGPR("--> create_debug_files\n");

	dir = debugfs_create_dir("2490000.eqos", NULL);
	if (dir == NULL) {
		pr_info(
		       "error creating directory: eqos_debug\n");
		return -ENODEV;
	}

	registers =
	    debugfs_create_file("registers", 744, dir, &registers_val,
				&registers_fops);
	if (registers == NULL) {
		pr_info("error creating file: registers\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR127 =
	    debugfs_create_file("MAC_MA32_127LR127", 744, dir,
				&MAC_MA32_127LR127_val,
				&MAC_MA32_127LR127_fops);
	if (MAC_MA32_127LR127 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR127\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR126 =
	    debugfs_create_file("MAC_MA32_127LR126", 744, dir,
				&MAC_MA32_127LR126_val,
				&MAC_MA32_127LR126_fops);
	if (MAC_MA32_127LR126 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR126\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR125 =
	    debugfs_create_file("MAC_MA32_127LR125", 744, dir,
				&MAC_MA32_127LR125_val,
				&MAC_MA32_127LR125_fops);
	if (MAC_MA32_127LR125 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR125\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR124 =
	    debugfs_create_file("MAC_MA32_127LR124", 744, dir,
				&MAC_MA32_127LR124_val,
				&MAC_MA32_127LR124_fops);
	if (MAC_MA32_127LR124 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR124\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR123 =
	    debugfs_create_file("MAC_MA32_127LR123", 744, dir,
				&MAC_MA32_127LR123_val,
				&MAC_MA32_127LR123_fops);
	if (MAC_MA32_127LR123 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR123\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR122 =
	    debugfs_create_file("MAC_MA32_127LR122", 744, dir,
				&MAC_MA32_127LR122_val,
				&MAC_MA32_127LR122_fops);
	if (MAC_MA32_127LR122 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR122\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR121 =
	    debugfs_create_file("MAC_MA32_127LR121", 744, dir,
				&MAC_MA32_127LR121_val,
				&MAC_MA32_127LR121_fops);
	if (MAC_MA32_127LR121 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR121\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR120 =
	    debugfs_create_file("MAC_MA32_127LR120", 744, dir,
				&MAC_MA32_127LR120_val,
				&MAC_MA32_127LR120_fops);
	if (MAC_MA32_127LR120 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR120\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR119 =
	    debugfs_create_file("MAC_MA32_127LR119", 744, dir,
				&MAC_MA32_127LR119_val,
				&MAC_MA32_127LR119_fops);
	if (MAC_MA32_127LR119 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR119\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR118 =
	    debugfs_create_file("MAC_MA32_127LR118", 744, dir,
				&MAC_MA32_127LR118_val,
				&MAC_MA32_127LR118_fops);
	if (MAC_MA32_127LR118 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR118\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR117 =
	    debugfs_create_file("MAC_MA32_127LR117", 744, dir,
				&MAC_MA32_127LR117_val,
				&MAC_MA32_127LR117_fops);
	if (MAC_MA32_127LR117 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR117\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR116 =
	    debugfs_create_file("MAC_MA32_127LR116", 744, dir,
				&MAC_MA32_127LR116_val,
				&MAC_MA32_127LR116_fops);
	if (MAC_MA32_127LR116 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR116\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR115 =
	    debugfs_create_file("MAC_MA32_127LR115", 744, dir,
				&MAC_MA32_127LR115_val,
				&MAC_MA32_127LR115_fops);
	if (MAC_MA32_127LR115 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR115\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR114 =
	    debugfs_create_file("MAC_MA32_127LR114", 744, dir,
				&MAC_MA32_127LR114_val,
				&MAC_MA32_127LR114_fops);
	if (MAC_MA32_127LR114 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR114\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR113 =
	    debugfs_create_file("MAC_MA32_127LR113", 744, dir,
				&MAC_MA32_127LR113_val,
				&MAC_MA32_127LR113_fops);
	if (MAC_MA32_127LR113 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR113\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR112 =
	    debugfs_create_file("MAC_MA32_127LR112", 744, dir,
				&MAC_MA32_127LR112_val,
				&MAC_MA32_127LR112_fops);
	if (MAC_MA32_127LR112 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR112\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR111 =
	    debugfs_create_file("MAC_MA32_127LR111", 744, dir,
				&MAC_MA32_127LR111_val,
				&MAC_MA32_127LR111_fops);
	if (MAC_MA32_127LR111 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR111\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR110 =
	    debugfs_create_file("MAC_MA32_127LR110", 744, dir,
				&MAC_MA32_127LR110_val,
				&MAC_MA32_127LR110_fops);
	if (MAC_MA32_127LR110 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR110\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR109 =
	    debugfs_create_file("MAC_MA32_127LR109", 744, dir,
				&MAC_MA32_127LR109_val,
				&MAC_MA32_127LR109_fops);
	if (MAC_MA32_127LR109 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR109\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR108 =
	    debugfs_create_file("MAC_MA32_127LR108", 744, dir,
				&MAC_MA32_127LR108_val,
				&MAC_MA32_127LR108_fops);
	if (MAC_MA32_127LR108 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR108\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR107 =
	    debugfs_create_file("MAC_MA32_127LR107", 744, dir,
				&MAC_MA32_127LR107_val,
				&MAC_MA32_127LR107_fops);
	if (MAC_MA32_127LR107 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR107\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR106 =
	    debugfs_create_file("MAC_MA32_127LR106", 744, dir,
				&MAC_MA32_127LR106_val,
				&MAC_MA32_127LR106_fops);
	if (MAC_MA32_127LR106 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR106\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR105 =
	    debugfs_create_file("MAC_MA32_127LR105", 744, dir,
				&MAC_MA32_127LR105_val,
				&MAC_MA32_127LR105_fops);
	if (MAC_MA32_127LR105 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR105\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR104 =
	    debugfs_create_file("MAC_MA32_127LR104", 744, dir,
				&MAC_MA32_127LR104_val,
				&MAC_MA32_127LR104_fops);
	if (MAC_MA32_127LR104 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR104\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR103 =
	    debugfs_create_file("MAC_MA32_127LR103", 744, dir,
				&MAC_MA32_127LR103_val,
				&MAC_MA32_127LR103_fops);
	if (MAC_MA32_127LR103 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR103\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR102 =
	    debugfs_create_file("MAC_MA32_127LR102", 744, dir,
				&MAC_MA32_127LR102_val,
				&MAC_MA32_127LR102_fops);
	if (MAC_MA32_127LR102 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR102\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR101 =
	    debugfs_create_file("MAC_MA32_127LR101", 744, dir,
				&MAC_MA32_127LR101_val,
				&MAC_MA32_127LR101_fops);
	if (MAC_MA32_127LR101 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR101\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR100 =
	    debugfs_create_file("MAC_MA32_127LR100", 744, dir,
				&MAC_MA32_127LR100_val,
				&MAC_MA32_127LR100_fops);
	if (MAC_MA32_127LR100 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR100\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR99 =
	    debugfs_create_file("MAC_MA32_127LR99", 744, dir,
				&MAC_MA32_127LR99_val, &MAC_MA32_127LR99_fops);
	if (MAC_MA32_127LR99 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR99\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR98 =
	    debugfs_create_file("MAC_MA32_127LR98", 744, dir,
				&MAC_MA32_127LR98_val, &MAC_MA32_127LR98_fops);
	if (MAC_MA32_127LR98 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR98\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR97 =
	    debugfs_create_file("MAC_MA32_127LR97", 744, dir,
				&MAC_MA32_127LR97_val, &MAC_MA32_127LR97_fops);
	if (MAC_MA32_127LR97 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR97\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR96 =
	    debugfs_create_file("MAC_MA32_127LR96", 744, dir,
				&MAC_MA32_127LR96_val, &MAC_MA32_127LR96_fops);
	if (MAC_MA32_127LR96 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR96\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR95 =
	    debugfs_create_file("MAC_MA32_127LR95", 744, dir,
				&MAC_MA32_127LR95_val, &MAC_MA32_127LR95_fops);
	if (MAC_MA32_127LR95 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR95\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR94 =
	    debugfs_create_file("MAC_MA32_127LR94", 744, dir,
				&MAC_MA32_127LR94_val, &MAC_MA32_127LR94_fops);
	if (MAC_MA32_127LR94 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR94\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR93 =
	    debugfs_create_file("MAC_MA32_127LR93", 744, dir,
				&MAC_MA32_127LR93_val, &MAC_MA32_127LR93_fops);
	if (MAC_MA32_127LR93 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR93\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR92 =
	    debugfs_create_file("MAC_MA32_127LR92", 744, dir,
				&MAC_MA32_127LR92_val, &MAC_MA32_127LR92_fops);
	if (MAC_MA32_127LR92 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR92\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR91 =
	    debugfs_create_file("MAC_MA32_127LR91", 744, dir,
				&MAC_MA32_127LR91_val, &MAC_MA32_127LR91_fops);
	if (MAC_MA32_127LR91 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR91\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR90 =
	    debugfs_create_file("MAC_MA32_127LR90", 744, dir,
				&MAC_MA32_127LR90_val, &MAC_MA32_127LR90_fops);
	if (MAC_MA32_127LR90 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR90\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR89 =
	    debugfs_create_file("MAC_MA32_127LR89", 744, dir,
				&MAC_MA32_127LR89_val, &MAC_MA32_127LR89_fops);
	if (MAC_MA32_127LR89 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR89\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR88 =
	    debugfs_create_file("MAC_MA32_127LR88", 744, dir,
				&MAC_MA32_127LR88_val, &MAC_MA32_127LR88_fops);
	if (MAC_MA32_127LR88 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR88\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR87 =
	    debugfs_create_file("MAC_MA32_127LR87", 744, dir,
				&MAC_MA32_127LR87_val, &MAC_MA32_127LR87_fops);
	if (MAC_MA32_127LR87 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR87\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR86 =
	    debugfs_create_file("MAC_MA32_127LR86", 744, dir,
				&MAC_MA32_127LR86_val, &MAC_MA32_127LR86_fops);
	if (MAC_MA32_127LR86 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR86\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR85 =
	    debugfs_create_file("MAC_MA32_127LR85", 744, dir,
				&MAC_MA32_127LR85_val, &MAC_MA32_127LR85_fops);
	if (MAC_MA32_127LR85 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR85\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR84 =
	    debugfs_create_file("MAC_MA32_127LR84", 744, dir,
				&MAC_MA32_127LR84_val, &MAC_MA32_127LR84_fops);
	if (MAC_MA32_127LR84 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR84\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR83 =
	    debugfs_create_file("MAC_MA32_127LR83", 744, dir,
				&MAC_MA32_127LR83_val, &MAC_MA32_127LR83_fops);
	if (MAC_MA32_127LR83 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR83\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR82 =
	    debugfs_create_file("MAC_MA32_127LR82", 744, dir,
				&MAC_MA32_127LR82_val, &MAC_MA32_127LR82_fops);
	if (MAC_MA32_127LR82 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR82\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR81 =
	    debugfs_create_file("MAC_MA32_127LR81", 744, dir,
				&MAC_MA32_127LR81_val, &MAC_MA32_127LR81_fops);
	if (MAC_MA32_127LR81 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR81\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR80 =
	    debugfs_create_file("MAC_MA32_127LR80", 744, dir,
				&MAC_MA32_127LR80_val, &MAC_MA32_127LR80_fops);
	if (MAC_MA32_127LR80 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR80\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR79 =
	    debugfs_create_file("MAC_MA32_127LR79", 744, dir,
				&MAC_MA32_127LR79_val, &MAC_MA32_127LR79_fops);
	if (MAC_MA32_127LR79 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR79\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR78 =
	    debugfs_create_file("MAC_MA32_127LR78", 744, dir,
				&MAC_MA32_127LR78_val, &MAC_MA32_127LR78_fops);
	if (MAC_MA32_127LR78 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR78\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR77 =
	    debugfs_create_file("MAC_MA32_127LR77", 744, dir,
				&MAC_MA32_127LR77_val, &MAC_MA32_127LR77_fops);
	if (MAC_MA32_127LR77 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR77\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR76 =
	    debugfs_create_file("MAC_MA32_127LR76", 744, dir,
				&MAC_MA32_127LR76_val, &MAC_MA32_127LR76_fops);
	if (MAC_MA32_127LR76 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR76\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR75 =
	    debugfs_create_file("MAC_MA32_127LR75", 744, dir,
				&MAC_MA32_127LR75_val, &MAC_MA32_127LR75_fops);
	if (MAC_MA32_127LR75 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR75\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR74 =
	    debugfs_create_file("MAC_MA32_127LR74", 744, dir,
				&MAC_MA32_127LR74_val, &MAC_MA32_127LR74_fops);
	if (MAC_MA32_127LR74 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR74\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR73 =
	    debugfs_create_file("MAC_MA32_127LR73", 744, dir,
				&MAC_MA32_127LR73_val, &MAC_MA32_127LR73_fops);
	if (MAC_MA32_127LR73 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR73\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR72 =
	    debugfs_create_file("MAC_MA32_127LR72", 744, dir,
				&MAC_MA32_127LR72_val, &MAC_MA32_127LR72_fops);
	if (MAC_MA32_127LR72 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR72\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR71 =
	    debugfs_create_file("MAC_MA32_127LR71", 744, dir,
				&MAC_MA32_127LR71_val, &MAC_MA32_127LR71_fops);
	if (MAC_MA32_127LR71 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR71\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR70 =
	    debugfs_create_file("MAC_MA32_127LR70", 744, dir,
				&MAC_MA32_127LR70_val, &MAC_MA32_127LR70_fops);
	if (MAC_MA32_127LR70 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR70\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR69 =
	    debugfs_create_file("MAC_MA32_127LR69", 744, dir,
				&MAC_MA32_127LR69_val, &MAC_MA32_127LR69_fops);
	if (MAC_MA32_127LR69 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR69\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR68 =
	    debugfs_create_file("MAC_MA32_127LR68", 744, dir,
				&MAC_MA32_127LR68_val, &MAC_MA32_127LR68_fops);
	if (MAC_MA32_127LR68 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR68\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR67 =
	    debugfs_create_file("MAC_MA32_127LR67", 744, dir,
				&MAC_MA32_127LR67_val, &MAC_MA32_127LR67_fops);
	if (MAC_MA32_127LR67 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR67\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR66 =
	    debugfs_create_file("MAC_MA32_127LR66", 744, dir,
				&MAC_MA32_127LR66_val, &MAC_MA32_127LR66_fops);
	if (MAC_MA32_127LR66 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR66\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR65 =
	    debugfs_create_file("MAC_MA32_127LR65", 744, dir,
				&MAC_MA32_127LR65_val, &MAC_MA32_127LR65_fops);
	if (MAC_MA32_127LR65 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR65\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR64 =
	    debugfs_create_file("MAC_MA32_127LR64", 744, dir,
				&MAC_MA32_127LR64_val, &MAC_MA32_127LR64_fops);
	if (MAC_MA32_127LR64 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR64\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR63 =
	    debugfs_create_file("MAC_MA32_127LR63", 744, dir,
				&MAC_MA32_127LR63_val, &MAC_MA32_127LR63_fops);
	if (MAC_MA32_127LR63 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR63\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR62 =
	    debugfs_create_file("MAC_MA32_127LR62", 744, dir,
				&MAC_MA32_127LR62_val, &MAC_MA32_127LR62_fops);
	if (MAC_MA32_127LR62 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR62\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR61 =
	    debugfs_create_file("MAC_MA32_127LR61", 744, dir,
				&MAC_MA32_127LR61_val, &MAC_MA32_127LR61_fops);
	if (MAC_MA32_127LR61 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR61\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR60 =
	    debugfs_create_file("MAC_MA32_127LR60", 744, dir,
				&MAC_MA32_127LR60_val, &MAC_MA32_127LR60_fops);
	if (MAC_MA32_127LR60 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR60\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR59 =
	    debugfs_create_file("MAC_MA32_127LR59", 744, dir,
				&MAC_MA32_127LR59_val, &MAC_MA32_127LR59_fops);
	if (MAC_MA32_127LR59 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR59\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR58 =
	    debugfs_create_file("MAC_MA32_127LR58", 744, dir,
				&MAC_MA32_127LR58_val, &MAC_MA32_127LR58_fops);
	if (MAC_MA32_127LR58 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR58\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR57 =
	    debugfs_create_file("MAC_MA32_127LR57", 744, dir,
				&MAC_MA32_127LR57_val, &MAC_MA32_127LR57_fops);
	if (MAC_MA32_127LR57 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR57\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR56 =
	    debugfs_create_file("MAC_MA32_127LR56", 744, dir,
				&MAC_MA32_127LR56_val, &MAC_MA32_127LR56_fops);
	if (MAC_MA32_127LR56 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR56\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR55 =
	    debugfs_create_file("MAC_MA32_127LR55", 744, dir,
				&MAC_MA32_127LR55_val, &MAC_MA32_127LR55_fops);
	if (MAC_MA32_127LR55 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR55\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR54 =
	    debugfs_create_file("MAC_MA32_127LR54", 744, dir,
				&MAC_MA32_127LR54_val, &MAC_MA32_127LR54_fops);
	if (MAC_MA32_127LR54 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR54\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR53 =
	    debugfs_create_file("MAC_MA32_127LR53", 744, dir,
				&MAC_MA32_127LR53_val, &MAC_MA32_127LR53_fops);
	if (MAC_MA32_127LR53 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR53\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR52 =
	    debugfs_create_file("MAC_MA32_127LR52", 744, dir,
				&MAC_MA32_127LR52_val, &MAC_MA32_127LR52_fops);
	if (MAC_MA32_127LR52 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR52\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR51 =
	    debugfs_create_file("MAC_MA32_127LR51", 744, dir,
				&MAC_MA32_127LR51_val, &MAC_MA32_127LR51_fops);
	if (MAC_MA32_127LR51 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR51\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR50 =
	    debugfs_create_file("MAC_MA32_127LR50", 744, dir,
				&MAC_MA32_127LR50_val, &MAC_MA32_127LR50_fops);
	if (MAC_MA32_127LR50 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR50\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR49 =
	    debugfs_create_file("MAC_MA32_127LR49", 744, dir,
				&MAC_MA32_127LR49_val, &MAC_MA32_127LR49_fops);
	if (MAC_MA32_127LR49 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR49\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR48 =
	    debugfs_create_file("MAC_MA32_127LR48", 744, dir,
				&MAC_MA32_127LR48_val, &MAC_MA32_127LR48_fops);
	if (MAC_MA32_127LR48 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR48\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR47 =
	    debugfs_create_file("MAC_MA32_127LR47", 744, dir,
				&MAC_MA32_127LR47_val, &MAC_MA32_127LR47_fops);
	if (MAC_MA32_127LR47 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR47\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR46 =
	    debugfs_create_file("MAC_MA32_127LR46", 744, dir,
				&MAC_MA32_127LR46_val, &MAC_MA32_127LR46_fops);
	if (MAC_MA32_127LR46 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR46\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR45 =
	    debugfs_create_file("MAC_MA32_127LR45", 744, dir,
				&MAC_MA32_127LR45_val, &MAC_MA32_127LR45_fops);
	if (MAC_MA32_127LR45 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR45\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR44 =
	    debugfs_create_file("MAC_MA32_127LR44", 744, dir,
				&MAC_MA32_127LR44_val, &MAC_MA32_127LR44_fops);
	if (MAC_MA32_127LR44 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR44\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR43 =
	    debugfs_create_file("MAC_MA32_127LR43", 744, dir,
				&MAC_MA32_127LR43_val, &MAC_MA32_127LR43_fops);
	if (MAC_MA32_127LR43 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR43\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR42 =
	    debugfs_create_file("MAC_MA32_127LR42", 744, dir,
				&MAC_MA32_127LR42_val, &MAC_MA32_127LR42_fops);
	if (MAC_MA32_127LR42 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR42\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR41 =
	    debugfs_create_file("MAC_MA32_127LR41", 744, dir,
				&MAC_MA32_127LR41_val, &MAC_MA32_127LR41_fops);
	if (MAC_MA32_127LR41 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR41\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR40 =
	    debugfs_create_file("MAC_MA32_127LR40", 744, dir,
				&MAC_MA32_127LR40_val, &MAC_MA32_127LR40_fops);
	if (MAC_MA32_127LR40 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR40\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR39 =
	    debugfs_create_file("MAC_MA32_127LR39", 744, dir,
				&MAC_MA32_127LR39_val, &MAC_MA32_127LR39_fops);
	if (MAC_MA32_127LR39 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR39\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR38 =
	    debugfs_create_file("MAC_MA32_127LR38", 744, dir,
				&MAC_MA32_127LR38_val, &MAC_MA32_127LR38_fops);
	if (MAC_MA32_127LR38 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR38\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR37 =
	    debugfs_create_file("MAC_MA32_127LR37", 744, dir,
				&MAC_MA32_127LR37_val, &MAC_MA32_127LR37_fops);
	if (MAC_MA32_127LR37 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR37\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR36 =
	    debugfs_create_file("MAC_MA32_127LR36", 744, dir,
				&MAC_MA32_127LR36_val, &MAC_MA32_127LR36_fops);
	if (MAC_MA32_127LR36 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR36\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR35 =
	    debugfs_create_file("MAC_MA32_127LR35", 744, dir,
				&MAC_MA32_127LR35_val, &MAC_MA32_127LR35_fops);
	if (MAC_MA32_127LR35 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR35\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR34 =
	    debugfs_create_file("MAC_MA32_127LR34", 744, dir,
				&MAC_MA32_127LR34_val, &MAC_MA32_127LR34_fops);
	if (MAC_MA32_127LR34 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR34\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR33 =
	    debugfs_create_file("MAC_MA32_127LR33", 744, dir,
				&MAC_MA32_127LR33_val, &MAC_MA32_127LR33_fops);
	if (MAC_MA32_127LR33 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR33\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127LR32 =
	    debugfs_create_file("MAC_MA32_127LR32", 744, dir,
				&MAC_MA32_127LR32_val, &MAC_MA32_127LR32_fops);
	if (MAC_MA32_127LR32 == NULL) {
		pr_info("error creating file: MAC_MA32_127LR32\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR127 =
	    debugfs_create_file("MAC_MA32_127HR127", 744, dir,
				&MAC_MA32_127HR127_val,
				&MAC_MA32_127HR127_fops);
	if (MAC_MA32_127HR127 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR127\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR126 =
	    debugfs_create_file("MAC_MA32_127HR126", 744, dir,
				&MAC_MA32_127HR126_val,
				&MAC_MA32_127HR126_fops);
	if (MAC_MA32_127HR126 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR126\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR125 =
	    debugfs_create_file("MAC_MA32_127HR125", 744, dir,
				&MAC_MA32_127HR125_val,
				&MAC_MA32_127HR125_fops);
	if (MAC_MA32_127HR125 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR125\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR124 =
	    debugfs_create_file("MAC_MA32_127HR124", 744, dir,
				&MAC_MA32_127HR124_val,
				&MAC_MA32_127HR124_fops);
	if (MAC_MA32_127HR124 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR124\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR123 =
	    debugfs_create_file("MAC_MA32_127HR123", 744, dir,
				&MAC_MA32_127HR123_val,
				&MAC_MA32_127HR123_fops);
	if (MAC_MA32_127HR123 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR123\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR122 =
	    debugfs_create_file("MAC_MA32_127HR122", 744, dir,
				&MAC_MA32_127HR122_val,
				&MAC_MA32_127HR122_fops);
	if (MAC_MA32_127HR122 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR122\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR121 =
	    debugfs_create_file("MAC_MA32_127HR121", 744, dir,
				&MAC_MA32_127HR121_val,
				&MAC_MA32_127HR121_fops);
	if (MAC_MA32_127HR121 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR121\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR120 =
	    debugfs_create_file("MAC_MA32_127HR120", 744, dir,
				&MAC_MA32_127HR120_val,
				&MAC_MA32_127HR120_fops);
	if (MAC_MA32_127HR120 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR120\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR119 =
	    debugfs_create_file("MAC_MA32_127HR119", 744, dir,
				&MAC_MA32_127HR119_val,
				&MAC_MA32_127HR119_fops);
	if (MAC_MA32_127HR119 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR119\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR118 =
	    debugfs_create_file("MAC_MA32_127HR118", 744, dir,
				&MAC_MA32_127HR118_val,
				&MAC_MA32_127HR118_fops);
	if (MAC_MA32_127HR118 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR118\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR117 =
	    debugfs_create_file("MAC_MA32_127HR117", 744, dir,
				&MAC_MA32_127HR117_val,
				&MAC_MA32_127HR117_fops);
	if (MAC_MA32_127HR117 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR117\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR116 =
	    debugfs_create_file("MAC_MA32_127HR116", 744, dir,
				&MAC_MA32_127HR116_val,
				&MAC_MA32_127HR116_fops);
	if (MAC_MA32_127HR116 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR116\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR115 =
	    debugfs_create_file("MAC_MA32_127HR115", 744, dir,
				&MAC_MA32_127HR115_val,
				&MAC_MA32_127HR115_fops);
	if (MAC_MA32_127HR115 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR115\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR114 =
	    debugfs_create_file("MAC_MA32_127HR114", 744, dir,
				&MAC_MA32_127HR114_val,
				&MAC_MA32_127HR114_fops);
	if (MAC_MA32_127HR114 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR114\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR113 =
	    debugfs_create_file("MAC_MA32_127HR113", 744, dir,
				&MAC_MA32_127HR113_val,
				&MAC_MA32_127HR113_fops);
	if (MAC_MA32_127HR113 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR113\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR112 =
	    debugfs_create_file("MAC_MA32_127HR112", 744, dir,
				&MAC_MA32_127HR112_val,
				&MAC_MA32_127HR112_fops);
	if (MAC_MA32_127HR112 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR112\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR111 =
	    debugfs_create_file("MAC_MA32_127HR111", 744, dir,
				&MAC_MA32_127HR111_val,
				&MAC_MA32_127HR111_fops);
	if (MAC_MA32_127HR111 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR111\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR110 =
	    debugfs_create_file("MAC_MA32_127HR110", 744, dir,
				&MAC_MA32_127HR110_val,
				&MAC_MA32_127HR110_fops);
	if (MAC_MA32_127HR110 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR110\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR109 =
	    debugfs_create_file("MAC_MA32_127HR109", 744, dir,
				&MAC_MA32_127HR109_val,
				&MAC_MA32_127HR109_fops);
	if (MAC_MA32_127HR109 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR109\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR108 =
	    debugfs_create_file("MAC_MA32_127HR108", 744, dir,
				&MAC_MA32_127HR108_val,
				&MAC_MA32_127HR108_fops);
	if (MAC_MA32_127HR108 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR108\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR107 =
	    debugfs_create_file("MAC_MA32_127HR107", 744, dir,
				&MAC_MA32_127HR107_val,
				&MAC_MA32_127HR107_fops);
	if (MAC_MA32_127HR107 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR107\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR106 =
	    debugfs_create_file("MAC_MA32_127HR106", 744, dir,
				&MAC_MA32_127HR106_val,
				&MAC_MA32_127HR106_fops);
	if (MAC_MA32_127HR106 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR106\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR105 =
	    debugfs_create_file("MAC_MA32_127HR105", 744, dir,
				&MAC_MA32_127HR105_val,
				&MAC_MA32_127HR105_fops);
	if (MAC_MA32_127HR105 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR105\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR104 =
	    debugfs_create_file("MAC_MA32_127HR104", 744, dir,
				&MAC_MA32_127HR104_val,
				&MAC_MA32_127HR104_fops);
	if (MAC_MA32_127HR104 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR104\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR103 =
	    debugfs_create_file("MAC_MA32_127HR103", 744, dir,
				&MAC_MA32_127HR103_val,
				&MAC_MA32_127HR103_fops);
	if (MAC_MA32_127HR103 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR103\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR102 =
	    debugfs_create_file("MAC_MA32_127HR102", 744, dir,
				&MAC_MA32_127HR102_val,
				&MAC_MA32_127HR102_fops);
	if (MAC_MA32_127HR102 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR102\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR101 =
	    debugfs_create_file("MAC_MA32_127HR101", 744, dir,
				&MAC_MA32_127HR101_val,
				&MAC_MA32_127HR101_fops);
	if (MAC_MA32_127HR101 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR101\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR100 =
	    debugfs_create_file("MAC_MA32_127HR100", 744, dir,
				&MAC_MA32_127HR100_val,
				&MAC_MA32_127HR100_fops);
	if (MAC_MA32_127HR100 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR100\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR99 =
	    debugfs_create_file("MAC_MA32_127HR99", 744, dir,
				&MAC_MA32_127HR99_val, &MAC_MA32_127HR99_fops);
	if (MAC_MA32_127HR99 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR99\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR98 =
	    debugfs_create_file("MAC_MA32_127HR98", 744, dir,
				&MAC_MA32_127HR98_val, &MAC_MA32_127HR98_fops);
	if (MAC_MA32_127HR98 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR98\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR97 =
	    debugfs_create_file("MAC_MA32_127HR97", 744, dir,
				&MAC_MA32_127HR97_val, &MAC_MA32_127HR97_fops);
	if (MAC_MA32_127HR97 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR97\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR96 =
	    debugfs_create_file("MAC_MA32_127HR96", 744, dir,
				&MAC_MA32_127HR96_val, &MAC_MA32_127HR96_fops);
	if (MAC_MA32_127HR96 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR96\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR95 =
	    debugfs_create_file("MAC_MA32_127HR95", 744, dir,
				&MAC_MA32_127HR95_val, &MAC_MA32_127HR95_fops);
	if (MAC_MA32_127HR95 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR95\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR94 =
	    debugfs_create_file("MAC_MA32_127HR94", 744, dir,
				&MAC_MA32_127HR94_val, &MAC_MA32_127HR94_fops);
	if (MAC_MA32_127HR94 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR94\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR93 =
	    debugfs_create_file("MAC_MA32_127HR93", 744, dir,
				&MAC_MA32_127HR93_val, &MAC_MA32_127HR93_fops);
	if (MAC_MA32_127HR93 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR93\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR92 =
	    debugfs_create_file("MAC_MA32_127HR92", 744, dir,
				&MAC_MA32_127HR92_val, &MAC_MA32_127HR92_fops);
	if (MAC_MA32_127HR92 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR92\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR91 =
	    debugfs_create_file("MAC_MA32_127HR91", 744, dir,
				&MAC_MA32_127HR91_val, &MAC_MA32_127HR91_fops);
	if (MAC_MA32_127HR91 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR91\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR90 =
	    debugfs_create_file("MAC_MA32_127HR90", 744, dir,
				&MAC_MA32_127HR90_val, &MAC_MA32_127HR90_fops);
	if (MAC_MA32_127HR90 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR90\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR89 =
	    debugfs_create_file("MAC_MA32_127HR89", 744, dir,
				&MAC_MA32_127HR89_val, &MAC_MA32_127HR89_fops);
	if (MAC_MA32_127HR89 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR89\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR88 =
	    debugfs_create_file("MAC_MA32_127HR88", 744, dir,
				&MAC_MA32_127HR88_val, &MAC_MA32_127HR88_fops);
	if (MAC_MA32_127HR88 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR88\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR87 =
	    debugfs_create_file("MAC_MA32_127HR87", 744, dir,
				&MAC_MA32_127HR87_val, &MAC_MA32_127HR87_fops);
	if (MAC_MA32_127HR87 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR87\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR86 =
	    debugfs_create_file("MAC_MA32_127HR86", 744, dir,
				&MAC_MA32_127HR86_val, &MAC_MA32_127HR86_fops);
	if (MAC_MA32_127HR86 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR86\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR85 =
	    debugfs_create_file("MAC_MA32_127HR85", 744, dir,
				&MAC_MA32_127HR85_val, &MAC_MA32_127HR85_fops);
	if (MAC_MA32_127HR85 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR85\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR84 =
	    debugfs_create_file("MAC_MA32_127HR84", 744, dir,
				&MAC_MA32_127HR84_val, &MAC_MA32_127HR84_fops);
	if (MAC_MA32_127HR84 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR84\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR83 =
	    debugfs_create_file("MAC_MA32_127HR83", 744, dir,
				&MAC_MA32_127HR83_val, &MAC_MA32_127HR83_fops);
	if (MAC_MA32_127HR83 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR83\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR82 =
	    debugfs_create_file("MAC_MA32_127HR82", 744, dir,
				&MAC_MA32_127HR82_val, &MAC_MA32_127HR82_fops);
	if (MAC_MA32_127HR82 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR82\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR81 =
	    debugfs_create_file("MAC_MA32_127HR81", 744, dir,
				&MAC_MA32_127HR81_val, &MAC_MA32_127HR81_fops);
	if (MAC_MA32_127HR81 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR81\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR80 =
	    debugfs_create_file("MAC_MA32_127HR80", 744, dir,
				&MAC_MA32_127HR80_val, &MAC_MA32_127HR80_fops);
	if (MAC_MA32_127HR80 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR80\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR79 =
	    debugfs_create_file("MAC_MA32_127HR79", 744, dir,
				&MAC_MA32_127HR79_val, &MAC_MA32_127HR79_fops);
	if (MAC_MA32_127HR79 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR79\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR78 =
	    debugfs_create_file("MAC_MA32_127HR78", 744, dir,
				&MAC_MA32_127HR78_val, &MAC_MA32_127HR78_fops);
	if (MAC_MA32_127HR78 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR78\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR77 =
	    debugfs_create_file("MAC_MA32_127HR77", 744, dir,
				&MAC_MA32_127HR77_val, &MAC_MA32_127HR77_fops);
	if (MAC_MA32_127HR77 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR77\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR76 =
	    debugfs_create_file("MAC_MA32_127HR76", 744, dir,
				&MAC_MA32_127HR76_val, &MAC_MA32_127HR76_fops);
	if (MAC_MA32_127HR76 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR76\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR75 =
	    debugfs_create_file("MAC_MA32_127HR75", 744, dir,
				&MAC_MA32_127HR75_val, &MAC_MA32_127HR75_fops);
	if (MAC_MA32_127HR75 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR75\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR74 =
	    debugfs_create_file("MAC_MA32_127HR74", 744, dir,
				&MAC_MA32_127HR74_val, &MAC_MA32_127HR74_fops);
	if (MAC_MA32_127HR74 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR74\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR73 =
	    debugfs_create_file("MAC_MA32_127HR73", 744, dir,
				&MAC_MA32_127HR73_val, &MAC_MA32_127HR73_fops);
	if (MAC_MA32_127HR73 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR73\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR72 =
	    debugfs_create_file("MAC_MA32_127HR72", 744, dir,
				&MAC_MA32_127HR72_val, &MAC_MA32_127HR72_fops);
	if (MAC_MA32_127HR72 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR72\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR71 =
	    debugfs_create_file("MAC_MA32_127HR71", 744, dir,
				&MAC_MA32_127HR71_val, &MAC_MA32_127HR71_fops);
	if (MAC_MA32_127HR71 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR71\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR70 =
	    debugfs_create_file("MAC_MA32_127HR70", 744, dir,
				&MAC_MA32_127HR70_val, &MAC_MA32_127HR70_fops);
	if (MAC_MA32_127HR70 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR70\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR69 =
	    debugfs_create_file("MAC_MA32_127HR69", 744, dir,
				&MAC_MA32_127HR69_val, &MAC_MA32_127HR69_fops);
	if (MAC_MA32_127HR69 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR69\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR68 =
	    debugfs_create_file("MAC_MA32_127HR68", 744, dir,
				&MAC_MA32_127HR68_val, &MAC_MA32_127HR68_fops);
	if (MAC_MA32_127HR68 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR68\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR67 =
	    debugfs_create_file("MAC_MA32_127HR67", 744, dir,
				&MAC_MA32_127HR67_val, &MAC_MA32_127HR67_fops);
	if (MAC_MA32_127HR67 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR67\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR66 =
	    debugfs_create_file("MAC_MA32_127HR66", 744, dir,
				&MAC_MA32_127HR66_val, &MAC_MA32_127HR66_fops);
	if (MAC_MA32_127HR66 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR66\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR65 =
	    debugfs_create_file("MAC_MA32_127HR65", 744, dir,
				&MAC_MA32_127HR65_val, &MAC_MA32_127HR65_fops);
	if (MAC_MA32_127HR65 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR65\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR64 =
	    debugfs_create_file("MAC_MA32_127HR64", 744, dir,
				&MAC_MA32_127HR64_val, &MAC_MA32_127HR64_fops);
	if (MAC_MA32_127HR64 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR64\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR63 =
	    debugfs_create_file("MAC_MA32_127HR63", 744, dir,
				&MAC_MA32_127HR63_val, &MAC_MA32_127HR63_fops);
	if (MAC_MA32_127HR63 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR63\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR62 =
	    debugfs_create_file("MAC_MA32_127HR62", 744, dir,
				&MAC_MA32_127HR62_val, &MAC_MA32_127HR62_fops);
	if (MAC_MA32_127HR62 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR62\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR61 =
	    debugfs_create_file("MAC_MA32_127HR61", 744, dir,
				&MAC_MA32_127HR61_val, &MAC_MA32_127HR61_fops);
	if (MAC_MA32_127HR61 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR61\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR60 =
	    debugfs_create_file("MAC_MA32_127HR60", 744, dir,
				&MAC_MA32_127HR60_val, &MAC_MA32_127HR60_fops);
	if (MAC_MA32_127HR60 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR60\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR59 =
	    debugfs_create_file("MAC_MA32_127HR59", 744, dir,
				&MAC_MA32_127HR59_val, &MAC_MA32_127HR59_fops);
	if (MAC_MA32_127HR59 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR59\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR58 =
	    debugfs_create_file("MAC_MA32_127HR58", 744, dir,
				&MAC_MA32_127HR58_val, &MAC_MA32_127HR58_fops);
	if (MAC_MA32_127HR58 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR58\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR57 =
	    debugfs_create_file("MAC_MA32_127HR57", 744, dir,
				&MAC_MA32_127HR57_val, &MAC_MA32_127HR57_fops);
	if (MAC_MA32_127HR57 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR57\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR56 =
	    debugfs_create_file("MAC_MA32_127HR56", 744, dir,
				&MAC_MA32_127HR56_val, &MAC_MA32_127HR56_fops);
	if (MAC_MA32_127HR56 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR56\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR55 =
	    debugfs_create_file("MAC_MA32_127HR55", 744, dir,
				&MAC_MA32_127HR55_val, &MAC_MA32_127HR55_fops);
	if (MAC_MA32_127HR55 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR55\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR54 =
	    debugfs_create_file("MAC_MA32_127HR54", 744, dir,
				&MAC_MA32_127HR54_val, &MAC_MA32_127HR54_fops);
	if (MAC_MA32_127HR54 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR54\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR53 =
	    debugfs_create_file("MAC_MA32_127HR53", 744, dir,
				&MAC_MA32_127HR53_val, &MAC_MA32_127HR53_fops);
	if (MAC_MA32_127HR53 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR53\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR52 =
	    debugfs_create_file("MAC_MA32_127HR52", 744, dir,
				&MAC_MA32_127HR52_val, &MAC_MA32_127HR52_fops);
	if (MAC_MA32_127HR52 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR52\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR51 =
	    debugfs_create_file("MAC_MA32_127HR51", 744, dir,
				&MAC_MA32_127HR51_val, &MAC_MA32_127HR51_fops);
	if (MAC_MA32_127HR51 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR51\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR50 =
	    debugfs_create_file("MAC_MA32_127HR50", 744, dir,
				&MAC_MA32_127HR50_val, &MAC_MA32_127HR50_fops);
	if (MAC_MA32_127HR50 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR50\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR49 =
	    debugfs_create_file("MAC_MA32_127HR49", 744, dir,
				&MAC_MA32_127HR49_val, &MAC_MA32_127HR49_fops);
	if (MAC_MA32_127HR49 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR49\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR48 =
	    debugfs_create_file("MAC_MA32_127HR48", 744, dir,
				&MAC_MA32_127HR48_val, &MAC_MA32_127HR48_fops);
	if (MAC_MA32_127HR48 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR48\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR47 =
	    debugfs_create_file("MAC_MA32_127HR47", 744, dir,
				&MAC_MA32_127HR47_val, &MAC_MA32_127HR47_fops);
	if (MAC_MA32_127HR47 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR47\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR46 =
	    debugfs_create_file("MAC_MA32_127HR46", 744, dir,
				&MAC_MA32_127HR46_val, &MAC_MA32_127HR46_fops);
	if (MAC_MA32_127HR46 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR46\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR45 =
	    debugfs_create_file("MAC_MA32_127HR45", 744, dir,
				&MAC_MA32_127HR45_val, &MAC_MA32_127HR45_fops);
	if (MAC_MA32_127HR45 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR45\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR44 =
	    debugfs_create_file("MAC_MA32_127HR44", 744, dir,
				&MAC_MA32_127HR44_val, &MAC_MA32_127HR44_fops);
	if (MAC_MA32_127HR44 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR44\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR43 =
	    debugfs_create_file("MAC_MA32_127HR43", 744, dir,
				&MAC_MA32_127HR43_val, &MAC_MA32_127HR43_fops);
	if (MAC_MA32_127HR43 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR43\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR42 =
	    debugfs_create_file("MAC_MA32_127HR42", 744, dir,
				&MAC_MA32_127HR42_val, &MAC_MA32_127HR42_fops);
	if (MAC_MA32_127HR42 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR42\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR41 =
	    debugfs_create_file("MAC_MA32_127HR41", 744, dir,
				&MAC_MA32_127HR41_val, &MAC_MA32_127HR41_fops);
	if (MAC_MA32_127HR41 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR41\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR40 =
	    debugfs_create_file("MAC_MA32_127HR40", 744, dir,
				&MAC_MA32_127HR40_val, &MAC_MA32_127HR40_fops);
	if (MAC_MA32_127HR40 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR40\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR39 =
	    debugfs_create_file("MAC_MA32_127HR39", 744, dir,
				&MAC_MA32_127HR39_val, &MAC_MA32_127HR39_fops);
	if (MAC_MA32_127HR39 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR39\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR38 =
	    debugfs_create_file("MAC_MA32_127HR38", 744, dir,
				&MAC_MA32_127HR38_val, &MAC_MA32_127HR38_fops);
	if (MAC_MA32_127HR38 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR38\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR37 =
	    debugfs_create_file("MAC_MA32_127HR37", 744, dir,
				&MAC_MA32_127HR37_val, &MAC_MA32_127HR37_fops);
	if (MAC_MA32_127HR37 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR37\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR36 =
	    debugfs_create_file("MAC_MA32_127HR36", 744, dir,
				&MAC_MA32_127HR36_val, &MAC_MA32_127HR36_fops);
	if (MAC_MA32_127HR36 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR36\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR35 =
	    debugfs_create_file("MAC_MA32_127HR35", 744, dir,
				&MAC_MA32_127HR35_val, &MAC_MA32_127HR35_fops);
	if (MAC_MA32_127HR35 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR35\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR34 =
	    debugfs_create_file("MAC_MA32_127HR34", 744, dir,
				&MAC_MA32_127HR34_val, &MAC_MA32_127HR34_fops);
	if (MAC_MA32_127HR34 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR34\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR33 =
	    debugfs_create_file("MAC_MA32_127HR33", 744, dir,
				&MAC_MA32_127HR33_val, &MAC_MA32_127HR33_fops);
	if (MAC_MA32_127HR33 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR33\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA32_127HR32 =
	    debugfs_create_file("MAC_MA32_127HR32", 744, dir,
				&MAC_MA32_127HR32_val, &MAC_MA32_127HR32_fops);
	if (MAC_MA32_127HR32 == NULL) {
		pr_info("error creating file: MAC_MA32_127HR32\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR31 =
	    debugfs_create_file("MAC_MA1_31LR31", 744, dir, &MAC_MA1_31LR31_val,
				&MAC_MA1_31LR31_fops);
	if (MAC_MA1_31LR31 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR31\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR30 =
	    debugfs_create_file("MAC_MA1_31LR30", 744, dir, &MAC_MA1_31LR30_val,
				&MAC_MA1_31LR30_fops);
	if (MAC_MA1_31LR30 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR30\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR29 =
	    debugfs_create_file("MAC_MA1_31LR29", 744, dir, &MAC_MA1_31LR29_val,
				&MAC_MA1_31LR29_fops);
	if (MAC_MA1_31LR29 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR29\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR28 =
	    debugfs_create_file("MAC_MA1_31LR28", 744, dir, &MAC_MA1_31LR28_val,
				&MAC_MA1_31LR28_fops);
	if (MAC_MA1_31LR28 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR28\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR27 =
	    debugfs_create_file("MAC_MA1_31LR27", 744, dir, &MAC_MA1_31LR27_val,
				&MAC_MA1_31LR27_fops);
	if (MAC_MA1_31LR27 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR27\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR26 =
	    debugfs_create_file("MAC_MA1_31LR26", 744, dir, &MAC_MA1_31LR26_val,
				&MAC_MA1_31LR26_fops);
	if (MAC_MA1_31LR26 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR26\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR25 =
	    debugfs_create_file("MAC_MA1_31LR25", 744, dir, &MAC_MA1_31LR25_val,
				&MAC_MA1_31LR25_fops);
	if (MAC_MA1_31LR25 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR25\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR24 =
	    debugfs_create_file("MAC_MA1_31LR24", 744, dir, &MAC_MA1_31LR24_val,
				&MAC_MA1_31LR24_fops);
	if (MAC_MA1_31LR24 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR24\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR23 =
	    debugfs_create_file("MAC_MA1_31LR23", 744, dir, &MAC_MA1_31LR23_val,
				&MAC_MA1_31LR23_fops);
	if (MAC_MA1_31LR23 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR23\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR22 =
	    debugfs_create_file("MAC_MA1_31LR22", 744, dir, &MAC_MA1_31LR22_val,
				&MAC_MA1_31LR22_fops);
	if (MAC_MA1_31LR22 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR22\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR21 =
	    debugfs_create_file("MAC_MA1_31LR21", 744, dir, &MAC_MA1_31LR21_val,
				&MAC_MA1_31LR21_fops);
	if (MAC_MA1_31LR21 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR21\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR20 =
	    debugfs_create_file("MAC_MA1_31LR20", 744, dir, &MAC_MA1_31LR20_val,
				&MAC_MA1_31LR20_fops);
	if (MAC_MA1_31LR20 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR20\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR19 =
	    debugfs_create_file("MAC_MA1_31LR19", 744, dir, &MAC_MA1_31LR19_val,
				&MAC_MA1_31LR19_fops);
	if (MAC_MA1_31LR19 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR19\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR18 =
	    debugfs_create_file("MAC_MA1_31LR18", 744, dir, &MAC_MA1_31LR18_val,
				&MAC_MA1_31LR18_fops);
	if (MAC_MA1_31LR18 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR18\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR17 =
	    debugfs_create_file("MAC_MA1_31LR17", 744, dir, &MAC_MA1_31LR17_val,
				&MAC_MA1_31LR17_fops);
	if (MAC_MA1_31LR17 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR17\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR16 =
	    debugfs_create_file("MAC_MA1_31LR16", 744, dir, &MAC_MA1_31LR16_val,
				&MAC_MA1_31LR16_fops);
	if (MAC_MA1_31LR16 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR16\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR15 =
	    debugfs_create_file("MAC_MA1_31LR15", 744, dir, &MAC_MA1_31LR15_val,
				&MAC_MA1_31LR15_fops);
	if (MAC_MA1_31LR15 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR15\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR14 =
	    debugfs_create_file("MAC_MA1_31LR14", 744, dir, &MAC_MA1_31LR14_val,
				&MAC_MA1_31LR14_fops);
	if (MAC_MA1_31LR14 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR14\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR13 =
	    debugfs_create_file("MAC_MA1_31LR13", 744, dir, &MAC_MA1_31LR13_val,
				&MAC_MA1_31LR13_fops);
	if (MAC_MA1_31LR13 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR13\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR12 =
	    debugfs_create_file("MAC_MA1_31LR12", 744, dir, &MAC_MA1_31LR12_val,
				&MAC_MA1_31LR12_fops);
	if (MAC_MA1_31LR12 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR12\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR11 =
	    debugfs_create_file("MAC_MA1_31LR11", 744, dir, &MAC_MA1_31LR11_val,
				&MAC_MA1_31LR11_fops);
	if (MAC_MA1_31LR11 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR11\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR10 =
	    debugfs_create_file("MAC_MA1_31LR10", 744, dir, &MAC_MA1_31LR10_val,
				&MAC_MA1_31LR10_fops);
	if (MAC_MA1_31LR10 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR10\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR9 =
	    debugfs_create_file("MAC_MA1_31LR9", 744, dir, &MAC_MA1_31LR9_val,
				&MAC_MA1_31LR9_fops);
	if (MAC_MA1_31LR9 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR9\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR8 =
	    debugfs_create_file("MAC_MA1_31LR8", 744, dir, &MAC_MA1_31LR8_val,
				&MAC_MA1_31LR8_fops);
	if (MAC_MA1_31LR8 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR8\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR7 =
	    debugfs_create_file("MAC_MA1_31LR7", 744, dir, &MAC_MA1_31LR7_val,
				&MAC_MA1_31LR7_fops);
	if (MAC_MA1_31LR7 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR6 =
	    debugfs_create_file("MAC_MA1_31LR6", 744, dir, &MAC_MA1_31LR6_val,
				&MAC_MA1_31LR6_fops);
	if (MAC_MA1_31LR6 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR5 =
	    debugfs_create_file("MAC_MA1_31LR5", 744, dir, &MAC_MA1_31LR5_val,
				&MAC_MA1_31LR5_fops);
	if (MAC_MA1_31LR5 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR4 =
	    debugfs_create_file("MAC_MA1_31LR4", 744, dir, &MAC_MA1_31LR4_val,
				&MAC_MA1_31LR4_fops);
	if (MAC_MA1_31LR4 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR3 =
	    debugfs_create_file("MAC_MA1_31LR3", 744, dir, &MAC_MA1_31LR3_val,
				&MAC_MA1_31LR3_fops);
	if (MAC_MA1_31LR3 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR2 =
	    debugfs_create_file("MAC_MA1_31LR2", 744, dir, &MAC_MA1_31LR2_val,
				&MAC_MA1_31LR2_fops);
	if (MAC_MA1_31LR2 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31LR1 =
	    debugfs_create_file("MAC_MA1_31LR1", 744, dir, &MAC_MA1_31LR1_val,
				&MAC_MA1_31LR1_fops);
	if (MAC_MA1_31LR1 == NULL) {
		pr_info("error creating file: MAC_MA1_31LR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR31 =
	    debugfs_create_file("MAC_MA1_31HR31", 744, dir, &MAC_MA1_31HR31_val,
				&MAC_MA1_31HR31_fops);
	if (MAC_MA1_31HR31 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR31\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR30 =
	    debugfs_create_file("MAC_MA1_31HR30", 744, dir, &MAC_MA1_31HR30_val,
				&MAC_MA1_31HR30_fops);
	if (MAC_MA1_31HR30 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR30\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR29 =
	    debugfs_create_file("MAC_MA1_31HR29", 744, dir, &MAC_MA1_31HR29_val,
				&MAC_MA1_31HR29_fops);
	if (MAC_MA1_31HR29 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR29\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR28 =
	    debugfs_create_file("MAC_MA1_31HR28", 744, dir, &MAC_MA1_31HR28_val,
				&MAC_MA1_31HR28_fops);
	if (MAC_MA1_31HR28 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR28\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR27 =
	    debugfs_create_file("MAC_MA1_31HR27", 744, dir, &MAC_MA1_31HR27_val,
				&MAC_MA1_31HR27_fops);
	if (MAC_MA1_31HR27 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR27\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR26 =
	    debugfs_create_file("MAC_MA1_31HR26", 744, dir, &MAC_MA1_31HR26_val,
				&MAC_MA1_31HR26_fops);
	if (MAC_MA1_31HR26 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR26\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR25 =
	    debugfs_create_file("MAC_MA1_31HR25", 744, dir, &MAC_MA1_31HR25_val,
				&MAC_MA1_31HR25_fops);
	if (MAC_MA1_31HR25 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR25\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR24 =
	    debugfs_create_file("MAC_MA1_31HR24", 744, dir, &MAC_MA1_31HR24_val,
				&MAC_MA1_31HR24_fops);
	if (MAC_MA1_31HR24 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR24\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR23 =
	    debugfs_create_file("MAC_MA1_31HR23", 744, dir, &MAC_MA1_31HR23_val,
				&MAC_MA1_31HR23_fops);
	if (MAC_MA1_31HR23 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR23\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR22 =
	    debugfs_create_file("MAC_MA1_31HR22", 744, dir, &MAC_MA1_31HR22_val,
				&MAC_MA1_31HR22_fops);
	if (MAC_MA1_31HR22 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR22\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR21 =
	    debugfs_create_file("MAC_MA1_31HR21", 744, dir, &MAC_MA1_31HR21_val,
				&MAC_MA1_31HR21_fops);
	if (MAC_MA1_31HR21 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR21\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR20 =
	    debugfs_create_file("MAC_MA1_31HR20", 744, dir, &MAC_MA1_31HR20_val,
				&MAC_MA1_31HR20_fops);
	if (MAC_MA1_31HR20 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR20\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR19 =
	    debugfs_create_file("MAC_MA1_31HR19", 744, dir, &MAC_MA1_31HR19_val,
				&MAC_MA1_31HR19_fops);
	if (MAC_MA1_31HR19 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR19\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR18 =
	    debugfs_create_file("MAC_MA1_31HR18", 744, dir, &MAC_MA1_31HR18_val,
				&MAC_MA1_31HR18_fops);
	if (MAC_MA1_31HR18 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR18\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR17 =
	    debugfs_create_file("MAC_MA1_31HR17", 744, dir, &MAC_MA1_31HR17_val,
				&MAC_MA1_31HR17_fops);
	if (MAC_MA1_31HR17 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR17\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR16 =
	    debugfs_create_file("MAC_MA1_31HR16", 744, dir, &MAC_MA1_31HR16_val,
				&MAC_MA1_31HR16_fops);
	if (MAC_MA1_31HR16 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR16\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR15 =
	    debugfs_create_file("MAC_MA1_31HR15", 744, dir, &MAC_MA1_31HR15_val,
				&MAC_MA1_31HR15_fops);
	if (MAC_MA1_31HR15 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR15\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR14 =
	    debugfs_create_file("MAC_MA1_31HR14", 744, dir, &MAC_MA1_31HR14_val,
				&MAC_MA1_31HR14_fops);
	if (MAC_MA1_31HR14 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR14\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR13 =
	    debugfs_create_file("MAC_MA1_31HR13", 744, dir, &MAC_MA1_31HR13_val,
				&MAC_MA1_31HR13_fops);
	if (MAC_MA1_31HR13 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR13\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR12 =
	    debugfs_create_file("MAC_MA1_31HR12", 744, dir, &MAC_MA1_31HR12_val,
				&MAC_MA1_31HR12_fops);
	if (MAC_MA1_31HR12 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR12\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR11 =
	    debugfs_create_file("MAC_MA1_31HR11", 744, dir, &MAC_MA1_31HR11_val,
				&MAC_MA1_31HR11_fops);
	if (MAC_MA1_31HR11 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR11\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR10 =
	    debugfs_create_file("MAC_MA1_31HR10", 744, dir, &MAC_MA1_31HR10_val,
				&MAC_MA1_31HR10_fops);
	if (MAC_MA1_31HR10 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR10\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR9 =
	    debugfs_create_file("MAC_MA1_31HR9", 744, dir, &MAC_MA1_31HR9_val,
				&MAC_MA1_31HR9_fops);
	if (MAC_MA1_31HR9 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR9\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR8 =
	    debugfs_create_file("MAC_MA1_31HR8", 744, dir, &MAC_MA1_31HR8_val,
				&MAC_MA1_31HR8_fops);
	if (MAC_MA1_31HR8 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR8\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR7 =
	    debugfs_create_file("MAC_MA1_31HR7", 744, dir, &MAC_MA1_31HR7_val,
				&MAC_MA1_31HR7_fops);
	if (MAC_MA1_31HR7 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR6 =
	    debugfs_create_file("MAC_MA1_31HR6", 744, dir, &MAC_MA1_31HR6_val,
				&MAC_MA1_31HR6_fops);
	if (MAC_MA1_31HR6 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR5 =
	    debugfs_create_file("MAC_MA1_31HR5", 744, dir, &MAC_MA1_31HR5_val,
				&MAC_MA1_31HR5_fops);
	if (MAC_MA1_31HR5 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR4 =
	    debugfs_create_file("MAC_MA1_31HR4", 744, dir, &MAC_MA1_31HR4_val,
				&MAC_MA1_31HR4_fops);
	if (MAC_MA1_31HR4 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR3 =
	    debugfs_create_file("MAC_MA1_31HR3", 744, dir, &MAC_MA1_31HR3_val,
				&MAC_MA1_31HR3_fops);
	if (MAC_MA1_31HR3 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR2 =
	    debugfs_create_file("MAC_MA1_31HR2", 744, dir, &MAC_MA1_31HR2_val,
				&MAC_MA1_31HR2_fops);
	if (MAC_MA1_31HR2 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1_31HR1 =
	    debugfs_create_file("MAC_MA1_31HR1", 744, dir, &MAC_MA1_31HR1_val,
				&MAC_MA1_31HR1_fops);
	if (MAC_MA1_31HR1 == NULL) {
		pr_info("error creating file: MAC_MA1_31HR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_ARPA =
	    debugfs_create_file("MAC_ARPA", 744, dir, &mac_arpa_val,
				&mac_arpa_fops);
	if (MAC_ARPA == NULL) {
		pr_info("error creating file: MAC_ARPA\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A3R7 =
	    debugfs_create_file("MAC_L3A3R7", 744, dir, &MAC_L3A3R7_val,
				&MAC_L3A3R7_fops);
	if (MAC_L3A3R7 == NULL) {
		pr_info("error creating file: MAC_L3A3R7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A3R6 =
	    debugfs_create_file("MAC_L3A3R6", 744, dir, &MAC_L3A3R6_val,
				&MAC_L3A3R6_fops);
	if (MAC_L3A3R6 == NULL) {
		pr_info("error creating file: MAC_L3A3R6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A3R5 =
	    debugfs_create_file("MAC_L3A3R5", 744, dir, &MAC_L3A3R5_val,
				&MAC_L3A3R5_fops);
	if (MAC_L3A3R5 == NULL) {
		pr_info("error creating file: MAC_L3A3R5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A3R4 =
	    debugfs_create_file("MAC_L3A3R4", 744, dir, &MAC_L3A3R4_val,
				&MAC_L3A3R4_fops);
	if (MAC_L3A3R4 == NULL) {
		pr_info("error creating file: MAC_L3A3R4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A3R3 =
	    debugfs_create_file("MAC_L3A3R3", 744, dir, &MAC_L3A3R3_val,
				&MAC_L3A3R3_fops);
	if (MAC_L3A3R3 == NULL) {
		pr_info("error creating file: MAC_L3A3R3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A3R2 =
	    debugfs_create_file("MAC_L3A3R2", 744, dir, &MAC_L3A3R2_val,
				&MAC_L3A3R2_fops);
	if (MAC_L3A3R2 == NULL) {
		pr_info("error creating file: MAC_L3A3R2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A3R1 =
	    debugfs_create_file("MAC_L3A3R1", 744, dir, &MAC_L3A3R1_val,
				&MAC_L3A3R1_fops);
	if (MAC_L3A3R1 == NULL) {
		pr_info("error creating file: MAC_L3A3R1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A3R0 =
	    debugfs_create_file("MAC_L3A3R0", 744, dir, &MAC_L3A3R0_val,
				&MAC_L3A3R0_fops);
	if (MAC_L3A3R0 == NULL) {
		pr_info("error creating file: MAC_L3A3R0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A2R7 =
	    debugfs_create_file("MAC_L3A2R7", 744, dir, &MAC_L3A2R7_val,
				&MAC_L3A2R7_fops);
	if (MAC_L3A2R7 == NULL) {
		pr_info("error creating file: MAC_L3A2R7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A2R6 =
	    debugfs_create_file("MAC_L3A2R6", 744, dir, &MAC_L3A2R6_val,
				&MAC_L3A2R6_fops);
	if (MAC_L3A2R6 == NULL) {
		pr_info("error creating file: MAC_L3A2R6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A2R5 =
	    debugfs_create_file("MAC_L3A2R5", 744, dir, &MAC_L3A2R5_val,
				&MAC_L3A2R5_fops);
	if (MAC_L3A2R5 == NULL) {
		pr_info("error creating file: MAC_L3A2R5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A2R4 =
	    debugfs_create_file("MAC_L3A2R4", 744, dir, &MAC_L3A2R4_val,
				&MAC_L3A2R4_fops);
	if (MAC_L3A2R4 == NULL) {
		pr_info("error creating file: MAC_L3A2R4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A2R3 =
	    debugfs_create_file("MAC_L3A2R3", 744, dir, &MAC_L3A2R3_val,
				&MAC_L3A2R3_fops);
	if (MAC_L3A2R3 == NULL) {
		pr_info("error creating file: MAC_L3A2R3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A2R2 =
	    debugfs_create_file("MAC_L3A2R2", 744, dir, &MAC_L3A2R2_val,
				&MAC_L3A2R2_fops);
	if (MAC_L3A2R2 == NULL) {
		pr_info("error creating file: MAC_L3A2R2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A2R1 =
	    debugfs_create_file("MAC_L3A2R1", 744, dir, &MAC_L3A2R1_val,
				&MAC_L3A2R1_fops);
	if (MAC_L3A2R1 == NULL) {
		pr_info("error creating file: MAC_L3A2R1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A2R0 =
	    debugfs_create_file("MAC_L3A2R0", 744, dir, &MAC_L3A2R0_val,
				&MAC_L3A2R0_fops);
	if (MAC_L3A2R0 == NULL) {
		pr_info("error creating file: MAC_L3A2R0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A1R7 =
	    debugfs_create_file("MAC_L3A1R7", 744, dir, &MAC_L3A1R7_val,
				&MAC_L3A1R7_fops);
	if (MAC_L3A1R7 == NULL) {
		pr_info("error creating file: MAC_L3A1R7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A1R6 =
	    debugfs_create_file("MAC_L3A1R6", 744, dir, &MAC_L3A1R6_val,
				&MAC_L3A1R6_fops);
	if (MAC_L3A1R6 == NULL) {
		pr_info("error creating file: MAC_L3A1R6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A1R5 =
	    debugfs_create_file("MAC_L3A1R5", 744, dir, &MAC_L3A1R5_val,
				&MAC_L3A1R5_fops);
	if (MAC_L3A1R5 == NULL) {
		pr_info("error creating file: MAC_L3A1R5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A1R4 =
	    debugfs_create_file("MAC_L3A1R4", 744, dir, &MAC_L3A1R4_val,
				&MAC_L3A1R4_fops);
	if (MAC_L3A1R4 == NULL) {
		pr_info("error creating file: MAC_L3A1R4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A1R3 =
	    debugfs_create_file("MAC_L3A1R3", 744, dir, &MAC_L3A1R3_val,
				&MAC_L3A1R3_fops);
	if (MAC_L3A1R3 == NULL) {
		pr_info("error creating file: MAC_L3A1R3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A1R2 =
	    debugfs_create_file("MAC_L3A1R2", 744, dir, &MAC_L3A1R2_val,
				&MAC_L3A1R2_fops);
	if (MAC_L3A1R2 == NULL) {
		pr_info("error creating file: MAC_L3A1R2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A1R1 =
	    debugfs_create_file("MAC_L3A1R1", 744, dir, &MAC_L3A1R1_val,
				&MAC_L3A1R1_fops);
	if (MAC_L3A1R1 == NULL) {
		pr_info("error creating file: MAC_L3A1R1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A1R0 =
	    debugfs_create_file("MAC_L3A1R0", 744, dir, &MAC_L3A1R0_val,
				&MAC_L3A1R0_fops);
	if (MAC_L3A1R0 == NULL) {
		pr_info("error creating file: MAC_L3A1R0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A0R7 =
	    debugfs_create_file("MAC_L3A0R7", 744, dir, &MAC_L3A0R7_val,
				&MAC_L3A0R7_fops);
	if (MAC_L3A0R7 == NULL) {
		pr_info("error creating file: MAC_L3A0R7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A0R6 =
	    debugfs_create_file("MAC_L3A0R6", 744, dir, &MAC_L3A0R6_val,
				&MAC_L3A0R6_fops);
	if (MAC_L3A0R6 == NULL) {
		pr_info("error creating file: MAC_L3A0R6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A0R5 =
	    debugfs_create_file("MAC_L3A0R5", 744, dir, &MAC_L3A0R5_val,
				&MAC_L3A0R5_fops);
	if (MAC_L3A0R5 == NULL) {
		pr_info("error creating file: MAC_L3A0R5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A0R4 =
	    debugfs_create_file("MAC_L3A0R4", 744, dir, &MAC_L3A0R4_val,
				&MAC_L3A0R4_fops);
	if (MAC_L3A0R4 == NULL) {
		pr_info("error creating file: MAC_L3A0R4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A0R3 =
	    debugfs_create_file("MAC_L3A0R3", 744, dir, &MAC_L3A0R3_val,
				&MAC_L3A0R3_fops);
	if (MAC_L3A0R3 == NULL) {
		pr_info("error creating file: MAC_L3A0R3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A0R2 =
	    debugfs_create_file("MAC_L3A0R2", 744, dir, &MAC_L3A0R2_val,
				&MAC_L3A0R2_fops);
	if (MAC_L3A0R2 == NULL) {
		pr_info("error creating file: MAC_L3A0R2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A0R1 =
	    debugfs_create_file("MAC_L3A0R1", 744, dir, &MAC_L3A0R1_val,
				&MAC_L3A0R1_fops);
	if (MAC_L3A0R1 == NULL) {
		pr_info("error creating file: MAC_L3A0R1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3A0R0 =
	    debugfs_create_file("MAC_L3A0R0", 744, dir, &MAC_L3A0R0_val,
				&MAC_L3A0R0_fops);
	if (MAC_L3A0R0 == NULL) {
		pr_info("error creating file: MAC_L3A0R0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L4AR7 =
	    debugfs_create_file("MAC_L4AR7", 744, dir, &MAC_L4AR7_val,
				&MAC_L4AR7_fops);
	if (MAC_L4AR7 == NULL) {
		pr_info("error creating file: MAC_L4AR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L4AR6 =
	    debugfs_create_file("MAC_L4AR6", 744, dir, &MAC_L4AR6_val,
				&MAC_L4AR6_fops);
	if (MAC_L4AR6 == NULL) {
		pr_info("error creating file: MAC_L4AR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L4AR5 =
	    debugfs_create_file("MAC_L4AR5", 744, dir, &MAC_L4AR5_val,
				&MAC_L4AR5_fops);
	if (MAC_L4AR5 == NULL) {
		pr_info("error creating file: MAC_L4AR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L4AR4 =
	    debugfs_create_file("MAC_L4AR4", 744, dir, &MAC_L4AR4_val,
				&MAC_L4AR4_fops);
	if (MAC_L4AR4 == NULL) {
		pr_info("error creating file: MAC_L4AR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L4AR3 =
	    debugfs_create_file("MAC_L4AR3", 744, dir, &MAC_L4AR3_val,
				&MAC_L4AR3_fops);
	if (MAC_L4AR3 == NULL) {
		pr_info("error creating file: MAC_L4AR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L4AR2 =
	    debugfs_create_file("MAC_L4AR2", 744, dir, &MAC_L4AR2_val,
				&MAC_L4AR2_fops);
	if (MAC_L4AR2 == NULL) {
		pr_info("error creating file: MAC_L4AR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L4AR1 =
	    debugfs_create_file("MAC_L4AR1", 744, dir, &MAC_L4AR1_val,
				&MAC_L4AR1_fops);
	if (MAC_L4AR1 == NULL) {
		pr_info("error creating file: MAC_L4AR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L4AR0 =
	    debugfs_create_file("MAC_L4AR0", 744, dir, &MAC_L4AR0_val,
				&MAC_L4AR0_fops);
	if (MAC_L4AR0 == NULL) {
		pr_info("error creating file: MAC_L4AR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3L4CR7 =
	    debugfs_create_file("MAC_L3L4CR7", 744, dir, &MAC_L3L4CR7_val,
				&MAC_L3L4CR7_fops);
	if (MAC_L3L4CR7 == NULL) {
		pr_info("error creating file: MAC_L3L4CR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3L4CR6 =
	    debugfs_create_file("MAC_L3L4CR6", 744, dir, &MAC_L3L4CR6_val,
				&MAC_L3L4CR6_fops);
	if (MAC_L3L4CR6 == NULL) {
		pr_info("error creating file: MAC_L3L4CR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3L4CR5 =
	    debugfs_create_file("MAC_L3L4CR5", 744, dir, &MAC_L3L4CR5_val,
				&MAC_L3L4CR5_fops);
	if (MAC_L3L4CR5 == NULL) {
		pr_info("error creating file: MAC_L3L4CR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3L4CR4 =
	    debugfs_create_file("MAC_L3L4CR4", 744, dir, &MAC_L3L4CR4_val,
				&MAC_L3L4CR4_fops);
	if (MAC_L3L4CR4 == NULL) {
		pr_info("error creating file: MAC_L3L4CR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3L4CR3 =
	    debugfs_create_file("MAC_L3L4CR3", 744, dir, &MAC_L3L4CR3_val,
				&MAC_L3L4CR3_fops);
	if (MAC_L3L4CR3 == NULL) {
		pr_info("error creating file: MAC_L3L4CR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3L4CR2 =
	    debugfs_create_file("MAC_L3L4CR2", 744, dir, &MAC_L3L4CR2_val,
				&MAC_L3L4CR2_fops);
	if (MAC_L3L4CR2 == NULL) {
		pr_info("error creating file: MAC_L3L4CR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3L4CR1 =
	    debugfs_create_file("MAC_L3L4CR1", 744, dir, &MAC_L3L4CR1_val,
				&MAC_L3L4CR1_fops);
	if (MAC_L3L4CR1 == NULL) {
		pr_info("error creating file: MAC_L3L4CR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_L3L4CR0 =
	    debugfs_create_file("MAC_L3L4CR0", 744, dir, &MAC_L3L4CR0_val,
				&MAC_L3L4CR0_fops);
	if (MAC_L3L4CR0 == NULL) {
		pr_info("error creating file: MAC_L3L4CR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_GPIOS =
	    debugfs_create_file("MAC_GPIOS", 744, dir, &mac_gpios_val,
				&mac_gpios_fops);
	if (MAC_GPIOS == NULL) {
		pr_info("error creating file: MAC_GPIOS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PCS =
	    debugfs_create_file("MAC_PCS", 744, dir, &mac_pcs_val,
				&mac_pcs_fops);
	if (MAC_PCS == NULL) {
		pr_info("error creating file: MAC_PCS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TES =
	    debugfs_create_file("MAC_TES", 744, dir, &mac_tes_val,
				&mac_tes_fops);
	if (MAC_TES == NULL) {
		pr_info("error creating file: MAC_TES\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_AE =
	    debugfs_create_file("MAC_AE", 744, dir, &mac_ae_val, &mac_ae_fops);
	if (MAC_AE == NULL) {
		pr_info("error creating file: MAC_AE\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_ALPA =
	    debugfs_create_file("MAC_ALPA", 744, dir, &mac_alpa_val,
				&mac_alpa_fops);
	if (MAC_ALPA == NULL) {
		pr_info("error creating file: MAC_ALPA\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_AAD =
	    debugfs_create_file("MAC_AAD", 744, dir, &mac_aad_val,
				&mac_aad_fops);
	if (MAC_AAD == NULL) {
		pr_info("error creating file: MAC_AAD\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_ANS =
	    debugfs_create_file("MAC_ANS", 744, dir, &mac_ans_val,
				&mac_ans_fops);
	if (MAC_ANS == NULL) {
		pr_info("error creating file: MAC_ANS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_ANC =
	    debugfs_create_file("MAC_ANC", 744, dir, &mac_anc_val,
				&mac_anc_fops);
	if (MAC_ANC == NULL) {
		pr_info("error creating file: MAC_ANC\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_LPC =
	    debugfs_create_file("MAC_LPC", 744, dir, &mac_lpc_val,
				&mac_lpc_fops);
	if (MAC_LPC == NULL) {
		pr_info("error creating file: MAC_LPC\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_LPS =
	    debugfs_create_file("MAC_LPS", 744, dir, &mac_lps_val,
				&mac_lps_fops);
	if (MAC_LPS == NULL) {
		pr_info("error creating file: MAC_LPS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_LMIR = debugfs_create_file("MAC_LMIR", 744, dir, &mac_lmir_val,
				       &mac_lmir_fops);
	if (MAC_LMIR == NULL) {
		pr_info("error creating file: MAC_LMIR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_SPI2R = debugfs_create_file("MAC_SPI2R", 744, dir, &MAC_SPI2r_val,
					&MAC_SPI2r_fops);
	if (MAC_SPI2R == NULL) {
		pr_info("error creating file: MAC_SPI2R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_SPI1R = debugfs_create_file("MAC_SPI1R", 744, dir, &MAC_SPI1r_val,
					&MAC_SPI1r_fops);
	if (MAC_SPI1R == NULL) {
		pr_info("error creating file: MAC_SPI1R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_SPI0R = debugfs_create_file("MAC_SPI0R", 744, dir, &MAC_SPI0r_val,
					&MAC_SPI0r_fops);
	if (MAC_SPI0R == NULL) {
		pr_info("error creating file: MAC_SPI0R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PTO_CR = debugfs_create_file("MAC_PTO_CR", 744, dir,
					 &mac_pto_cr_val,
					 &mac_pto_cr_fops);
	if (MAC_PTO_CR == NULL) {
		pr_info("error creating file: MAC_PTO_CR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}




	MAC_PPS_WIDTH3 =
	    debugfs_create_file("MAC_PPS_WIDTH3", 744, dir, &MAC_PPS_WIDTH3_val,
				&MAC_PPS_WIDTH3_fops);
	if (MAC_PPS_WIDTH3 == NULL) {
		pr_info("error creating file: MAC_PPS_WIDTH3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_WIDTH2 =
	    debugfs_create_file("MAC_PPS_WIDTH2", 744, dir, &MAC_PPS_WIDTH2_val,
				&MAC_PPS_WIDTH2_fops);
	if (MAC_PPS_WIDTH2 == NULL) {
		pr_info("error creating file: MAC_PPS_WIDTH2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_WIDTH1 =
	    debugfs_create_file("MAC_PPS_WIDTH1", 744, dir, &MAC_PPS_WIDTH1_val,
				&MAC_PPS_WIDTH1_fops);
	if (MAC_PPS_WIDTH1 == NULL) {
		pr_info("error creating file: MAC_PPS_WIDTH1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_WIDTH0 =
	    debugfs_create_file("MAC_PPS_WIDTH0", 744, dir, &MAC_PPS_WIDTH0_val,
				&MAC_PPS_WIDTH0_fops);
	if (MAC_PPS_WIDTH0 == NULL) {
		pr_info("error creating file: MAC_PPS_WIDTH0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_INTVAL3 =
	    debugfs_create_file("MAC_PPS_INTVAL3", 744, dir,
				&MAC_PPS_INTVAL3_val, &MAC_PPS_INTVAL3_fops);
	if (MAC_PPS_INTVAL3 == NULL) {
		pr_info("error creating file: MAC_PPS_INTVAL3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_INTVAL2 =
	    debugfs_create_file("MAC_PPS_INTVAL2", 744, dir,
				&MAC_PPS_INTVAL2_val, &MAC_PPS_INTVAL2_fops);
	if (MAC_PPS_INTVAL2 == NULL) {
		pr_info("error creating file: MAC_PPS_INTVAL2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_INTVAL1 =
	    debugfs_create_file("MAC_PPS_INTVAL1", 744, dir,
				&MAC_PPS_INTVAL1_val, &MAC_PPS_INTVAL1_fops);
	if (MAC_PPS_INTVAL1 == NULL) {
		pr_info("error creating file: MAC_PPS_INTVAL1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_INTVAL0 =
	    debugfs_create_file("MAC_PPS_INTVAL0", 744, dir,
				&MAC_PPS_INTVAL0_val, &MAC_PPS_INTVAL0_fops);
	if (MAC_PPS_INTVAL0 == NULL) {
		pr_info("error creating file: MAC_PPS_INTVAL0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_TTNS3 =
	    debugfs_create_file("MAC_PPS_TTNS3", 744, dir, &MAC_PPS_TTNS3_val,
				&MAC_PPS_TTNS3_fops);
	if (MAC_PPS_TTNS3 == NULL) {
		pr_info("error creating file: MAC_PPS_TTNS3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_TTNS2 =
	    debugfs_create_file("MAC_PPS_TTNS2", 744, dir, &MAC_PPS_TTNS2_val,
				&MAC_PPS_TTNS2_fops);
	if (MAC_PPS_TTNS2 == NULL) {
		pr_info("error creating file: MAC_PPS_TTNS2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_TTNS1 =
	    debugfs_create_file("MAC_PPS_TTNS1", 744, dir, &MAC_PPS_TTNS1_val,
				&MAC_PPS_TTNS1_fops);
	if (MAC_PPS_TTNS1 == NULL) {
		pr_info("error creating file: MAC_PPS_TTNS1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_TTNS0 =
	    debugfs_create_file("MAC_PPS_TTNS0", 744, dir, &MAC_PPS_TTNS0_val,
				&MAC_PPS_TTNS0_fops);
	if (MAC_PPS_TTNS0 == NULL) {
		pr_info("error creating file: MAC_PPS_TTNS0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_TTS3 =
	    debugfs_create_file("MAC_PPS_TTS3", 744, dir, &MAC_PPS_TTS3_val,
				&MAC_PPS_TTS3_fops);
	if (MAC_PPS_TTS3 == NULL) {
		pr_info("error creating file: MAC_PPS_TTS3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_TTS2 =
	    debugfs_create_file("MAC_PPS_TTS2", 744, dir, &MAC_PPS_TTS2_val,
				&MAC_PPS_TTS2_fops);
	if (MAC_PPS_TTS2 == NULL) {
		pr_info("error creating file: MAC_PPS_TTS2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_TTS1 =
	    debugfs_create_file("MAC_PPS_TTS1", 744, dir, &MAC_PPS_TTS1_val,
				&MAC_PPS_TTS1_fops);
	if (MAC_PPS_TTS1 == NULL) {
		pr_info("error creating file: MAC_PPS_TTS1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPS_TTS0 =
	    debugfs_create_file("MAC_PPS_TTS0", 744, dir, &MAC_PPS_TTS0_val,
				&MAC_PPS_TTS0_fops);
	if (MAC_PPS_TTS0 == NULL) {
		pr_info("error creating file: MAC_PPS_TTS0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PPSC =
	    debugfs_create_file("MAC_PPSC", 744, dir, &mac_ppsc_val,
				&mac_ppsc_fops);
	if (MAC_PPSC == NULL) {
		pr_info("error creating file: MAC_PPSC\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TEAC =
	    debugfs_create_file("MAC_TEAC", 744, dir, &mac_teac_val,
				&mac_teac_fops);
	if (MAC_TEAC == NULL) {
		pr_info("error creating file: MAC_TEAC\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TIAC =
	    debugfs_create_file("MAC_TIAC", 744, dir, &mac_tiac_val,
				&mac_tiac_fops);
	if (MAC_TIAC == NULL) {
		pr_info("error creating file: MAC_TIAC\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_ATS =
	    debugfs_create_file("MAC_ATS", 744, dir, &mac_ats_val,
				&mac_ats_fops);
	if (MAC_ATS == NULL) {
		pr_info("error creating file: MAC_ATS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_ATN =
	    debugfs_create_file("MAC_ATN", 744, dir, &mac_atn_val,
				&mac_atn_fops);
	if (MAC_ATN == NULL) {
		pr_info("error creating file: MAC_ATN\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_AC =
	    debugfs_create_file("MAC_AC", 744, dir, &mac_ac_val, &mac_ac_fops);
	if (MAC_AC == NULL) {
		pr_info("error creating file: MAC_AC\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TTN =
	    debugfs_create_file("MAC_TTN", 744, dir, &mac_ttn_val,
				&mac_ttn_fops);
	if (MAC_TTN == NULL) {
		pr_info("error creating file: MAC_TTN\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TTSN =
	    debugfs_create_file("MAC_TTSN", 744, dir, &mac_ttsn_val,
				&mac_ttsn_fops);
	if (MAC_TTSN == NULL) {
		pr_info("error creating file: MAC_TTSN\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TSR =
	    debugfs_create_file("MAC_TSR", 744, dir, &mac_tsr_val,
				&mac_tsr_fops);
	if (MAC_TSR == NULL) {
		pr_info("error creating file: MAC_TSR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_STHWR =
	    debugfs_create_file("MAC_STHWR", 744, dir, &mac_sthwr_val,
				&mac_sthwr_fops);
	if (MAC_STHWR == NULL) {
		pr_info("error creating file: MAC_STHWR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TAR =
	    debugfs_create_file("MAC_TAR", 744, dir, &mac_tar_val,
				&mac_tar_fops);
	if (MAC_TAR == NULL) {
		pr_info("error creating file: MAC_TAR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_STNSUR =
	    debugfs_create_file("MAC_STNSUR", 744, dir, &mac_stnsur_val,
				&mac_stnsur_fops);
	if (MAC_STNSUR == NULL) {
		pr_info("error creating file: MAC_STNSUR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_STSUR =
	    debugfs_create_file("MAC_STSUR", 744, dir, &mac_stsur_val,
				&mac_stsur_fops);
	if (MAC_STSUR == NULL) {
		pr_info("error creating file: MAC_STSUR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_STNSR =
	    debugfs_create_file("MAC_STNSR", 744, dir, &mac_stnsr_val,
				&mac_stnsr_fops);
	if (MAC_STNSR == NULL) {
		pr_info("error creating file: MAC_STNSR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_STSR =
	    debugfs_create_file("MAC_STSR", 744, dir, &mac_stsr_val,
				&mac_stsr_fops);
	if (MAC_STSR == NULL) {
		pr_info("error creating file: MAC_STSR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_SSIR =
	    debugfs_create_file("MAC_SSIR", 744, dir, &mac_ssir_val,
				&mac_ssir_fops);
	if (MAC_SSIR == NULL) {
		pr_info("error creating file: MAC_SSIR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TCR =
	    debugfs_create_file("MAC_TCR", 744, dir, &mac_tcr_val,
				&mac_tcr_fops);
	if (MAC_TCR == NULL) {
		pr_info("error creating file: MAC_TCR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_DSR =
	    debugfs_create_file("MTL_DSR", 744, dir, &mtl_dsr_val,
				&mtl_dsr_fops);
	if (MTL_DSR == NULL) {
		pr_info("error creating file: MTL_DSR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_RWPFFR =
	    debugfs_create_file("MAC_RWPFFR", 744, dir, &mac_rwpffr_val,
				&mac_rwpffr_fops);
	if (MAC_RWPFFR == NULL) {
		pr_info("error creating file: MAC_RWPFFR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_RTSR =
	    debugfs_create_file("MAC_RTSR", 744, dir, &mac_rtsr_val,
				&mac_rtsr_fops);
	if (MAC_RTSR == NULL) {
		pr_info("error creating file: MAC_RTSR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_IER =
	    debugfs_create_file("MTL_IER", 744, dir, &mtl_ier_val,
				&mtl_ier_fops);
	if (MTL_IER == NULL) {
		pr_info("error creating file: MTL_IER\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRCR7 =
	    debugfs_create_file("MTL_QRCR7", 744, dir, &MTL_QRCR7_val,
				&MTL_QRCR7_fops);
	if (MTL_QRCR7 == NULL) {
		pr_info("error creating file: MTL_QRCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRCR6 =
	    debugfs_create_file("MTL_QRCR6", 744, dir, &MTL_QRCR6_val,
				&MTL_QRCR6_fops);
	if (MTL_QRCR6 == NULL) {
		pr_info("error creating file: MTL_QRCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRCR5 =
	    debugfs_create_file("MTL_QRCR5", 744, dir, &MTL_QRCR5_val,
				&MTL_QRCR5_fops);
	if (MTL_QRCR5 == NULL) {
		pr_info("error creating file: MTL_QRCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRCR4 =
	    debugfs_create_file("MTL_QRCR4", 744, dir, &MTL_QRCR4_val,
				&MTL_QRCR4_fops);
	if (MTL_QRCR4 == NULL) {
		pr_info("error creating file: MTL_QRCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRCR3 =
	    debugfs_create_file("MTL_QRCR3", 744, dir, &MTL_QRCR3_val,
				&MTL_QRCR3_fops);
	if (MTL_QRCR3 == NULL) {
		pr_info("error creating file: MTL_QRCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRCR2 =
	    debugfs_create_file("MTL_QRCR2", 744, dir, &MTL_QRCR2_val,
				&MTL_QRCR2_fops);
	if (MTL_QRCR2 == NULL) {
		pr_info("error creating file: MTL_QRCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRCR1 =
	    debugfs_create_file("MTL_QRCR1", 744, dir, &MTL_QRCR1_val,
				&MTL_QRCR1_fops);
	if (MTL_QRCR1 == NULL) {
		pr_info("error creating file: MTL_QRCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRDR7 =
	    debugfs_create_file("MTL_QRDR7", 744, dir, &MTL_QRDR7_val,
				&MTL_QRDR7_fops);
	if (MTL_QRDR7 == NULL) {
		pr_info("error creating file: MTL_QRDR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRDR6 =
	    debugfs_create_file("MTL_QRDR6", 744, dir, &MTL_QRDR6_val,
				&MTL_QRDR6_fops);
	if (MTL_QRDR6 == NULL) {
		pr_info("error creating file: MTL_QRDR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRDR5 =
	    debugfs_create_file("MTL_QRDR5", 744, dir, &MTL_QRDR5_val,
				&MTL_QRDR5_fops);
	if (MTL_QRDR5 == NULL) {
		pr_info("error creating file: MTL_QRDR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRDR4 =
	    debugfs_create_file("MTL_QRDR4", 744, dir, &MTL_QRDR4_val,
				&MTL_QRDR4_fops);
	if (MTL_QRDR4 == NULL) {
		pr_info("error creating file: MTL_QRDR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRDR3 =
	    debugfs_create_file("MTL_QRDR3", 744, dir, &MTL_QRDR3_val,
				&MTL_QRDR3_fops);
	if (MTL_QRDR3 == NULL) {
		pr_info("error creating file: MTL_QRDR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRDR2 =
	    debugfs_create_file("MTL_QRDR2", 744, dir, &MTL_QRDR2_val,
				&MTL_QRDR2_fops);
	if (MTL_QRDR2 == NULL) {
		pr_info("error creating file: MTL_QRDR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QRDR1 =
	    debugfs_create_file("MTL_QRDR1", 744, dir, &MTL_QRDR1_val,
				&MTL_QRDR1_fops);
	if (MTL_QRDR1 == NULL) {
		pr_info("error creating file: MTL_QRDR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QOCR7 =
	    debugfs_create_file("MTL_QOCR7", 744, dir, &MTL_QOCR7_val,
				&MTL_QOCR7_fops);
	if (MTL_QOCR7 == NULL) {
		pr_info("error creating file: MTL_QOCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QOCR6 =
	    debugfs_create_file("MTL_QOCR6", 744, dir, &MTL_QOCR6_val,
				&MTL_QOCR6_fops);
	if (MTL_QOCR6 == NULL) {
		pr_info("error creating file: MTL_QOCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QOCR5 =
	    debugfs_create_file("MTL_QOCR5", 744, dir, &MTL_QOCR5_val,
				&MTL_QOCR5_fops);
	if (MTL_QOCR5 == NULL) {
		pr_info("error creating file: MTL_QOCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QOCR4 =
	    debugfs_create_file("MTL_QOCR4", 744, dir, &MTL_QOCR4_val,
				&MTL_QOCR4_fops);
	if (MTL_QOCR4 == NULL) {
		pr_info("error creating file: MTL_QOCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QOCR3 =
	    debugfs_create_file("MTL_QOCR3", 744, dir, &MTL_QOCR3_val,
				&MTL_QOCR3_fops);
	if (MTL_QOCR3 == NULL) {
		pr_info("error creating file: MTL_QOCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QOCR2 =
	    debugfs_create_file("MTL_QOCR2", 744, dir, &MTL_QOCR2_val,
				&MTL_QOCR2_fops);
	if (MTL_QOCR2 == NULL) {
		pr_info("error creating file: MTL_QOCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QOCR1 =
	    debugfs_create_file("MTL_QOCR1", 744, dir, &MTL_QOCR1_val,
				&MTL_QOCR1_fops);
	if (MTL_QOCR1 == NULL) {
		pr_info("error creating file: MTL_QOCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QROMR7 =
	    debugfs_create_file("MTL_QROMR7", 744, dir, &MTL_QROMR7_val,
				&MTL_QROMR7_fops);
	if (MTL_QROMR7 == NULL) {
		pr_info("error creating file: MTL_QROMR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QROMR6 =
	    debugfs_create_file("MTL_QROMR6", 744, dir, &MTL_QROMR6_val,
				&MTL_QROMR6_fops);
	if (MTL_QROMR6 == NULL) {
		pr_info("error creating file: MTL_QROMR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QROMR5 =
	    debugfs_create_file("MTL_QROMR5", 744, dir, &MTL_QROMR5_val,
				&MTL_QROMR5_fops);
	if (MTL_QROMR5 == NULL) {
		pr_info("error creating file: MTL_QROMR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QROMR4 =
	    debugfs_create_file("MTL_QROMR4", 744, dir, &MTL_QROMR4_val,
				&MTL_QROMR4_fops);
	if (MTL_QROMR4 == NULL) {
		pr_info("error creating file: MTL_QROMR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QROMR3 =
	    debugfs_create_file("MTL_QROMR3", 744, dir, &MTL_QROMR3_val,
				&MTL_QROMR3_fops);
	if (MTL_QROMR3 == NULL) {
		pr_info("error creating file: MTL_QROMR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QROMR2 =
	    debugfs_create_file("MTL_QROMR2", 744, dir, &MTL_QROMR2_val,
				&MTL_QROMR2_fops);
	if (MTL_QROMR2 == NULL) {
		pr_info("error creating file: MTL_QROMR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QROMR1 =
	    debugfs_create_file("MTL_QROMR1", 744, dir, &MTL_QROMR1_val,
				&MTL_QROMR1_fops);
	if (MTL_QROMR1 == NULL) {
		pr_info("error creating file: MTL_QROMR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QLCR7 =
	    debugfs_create_file("MTL_QLCR7", 744, dir, &MTL_QLCR7_val,
				&MTL_QLCR7_fops);
	if (MTL_QLCR7 == NULL) {
		pr_info("error creating file: MTL_QLCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QLCR6 =
	    debugfs_create_file("MTL_QLCR6", 744, dir, &MTL_QLCR6_val,
				&MTL_QLCR6_fops);
	if (MTL_QLCR6 == NULL) {
		pr_info("error creating file: MTL_QLCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QLCR5 =
	    debugfs_create_file("MTL_QLCR5", 744, dir, &MTL_QLCR5_val,
				&MTL_QLCR5_fops);
	if (MTL_QLCR5 == NULL) {
		pr_info("error creating file: MTL_QLCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QLCR4 =
	    debugfs_create_file("MTL_QLCR4", 744, dir, &MTL_QLCR4_val,
				&MTL_QLCR4_fops);
	if (MTL_QLCR4 == NULL) {
		pr_info("error creating file: MTL_QLCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QLCR3 =
	    debugfs_create_file("MTL_QLCR3", 744, dir, &MTL_QLCR3_val,
				&MTL_QLCR3_fops);
	if (MTL_QLCR3 == NULL) {
		pr_info("error creating file: MTL_QLCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QLCR2 =
	    debugfs_create_file("MTL_QLCR2", 744, dir, &MTL_QLCR2_val,
				&MTL_QLCR2_fops);
	if (MTL_QLCR2 == NULL) {
		pr_info("error creating file: MTL_QLCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QLCR1 =
	    debugfs_create_file("MTL_QLCR1", 744, dir, &MTL_QLCR1_val,
				&MTL_QLCR1_fops);
	if (MTL_QLCR1 == NULL) {
		pr_info("error creating file: MTL_QLCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QHCR7 =
	    debugfs_create_file("MTL_QHCR7", 744, dir, &MTL_QHCR7_val,
				&MTL_QHCR7_fops);
	if (MTL_QHCR7 == NULL) {
		pr_info("error creating file: MTL_QHCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QHCR6 =
	    debugfs_create_file("MTL_QHCR6", 744, dir, &MTL_QHCR6_val,
				&MTL_QHCR6_fops);
	if (MTL_QHCR6 == NULL) {
		pr_info("error creating file: MTL_QHCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QHCR5 =
	    debugfs_create_file("MTL_QHCR5", 744, dir, &MTL_QHCR5_val,
				&MTL_QHCR5_fops);
	if (MTL_QHCR5 == NULL) {
		pr_info("error creating file: MTL_QHCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QHCR4 =
	    debugfs_create_file("MTL_QHCR4", 744, dir, &MTL_QHCR4_val,
				&MTL_QHCR4_fops);
	if (MTL_QHCR4 == NULL) {
		pr_info("error creating file: MTL_QHCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QHCR3 =
	    debugfs_create_file("MTL_QHCR3", 744, dir, &MTL_QHCR3_val,
				&MTL_QHCR3_fops);
	if (MTL_QHCR3 == NULL) {
		pr_info("error creating file: MTL_QHCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QHCR2 =
	    debugfs_create_file("MTL_QHCR2", 744, dir, &MTL_QHCR2_val,
				&MTL_QHCR2_fops);
	if (MTL_QHCR2 == NULL) {
		pr_info("error creating file: MTL_QHCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QHCR1 =
	    debugfs_create_file("MTL_QHCR1", 744, dir, &MTL_QHCR1_val,
				&MTL_QHCR1_fops);
	if (MTL_QHCR1 == NULL) {
		pr_info("error creating file: MTL_QHCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QSSCR7 =
	    debugfs_create_file("MTL_QSSCR7", 744, dir, &MTL_QSSCR7_val,
				&MTL_QSSCR7_fops);
	if (MTL_QSSCR7 == NULL) {
		pr_info("error creating file: MTL_QSSCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QSSCR6 =
	    debugfs_create_file("MTL_QSSCR6", 744, dir, &MTL_QSSCR6_val,
				&MTL_QSSCR6_fops);
	if (MTL_QSSCR6 == NULL) {
		pr_info("error creating file: MTL_QSSCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QSSCR5 =
	    debugfs_create_file("MTL_QSSCR5", 744, dir, &MTL_QSSCR5_val,
				&MTL_QSSCR5_fops);
	if (MTL_QSSCR5 == NULL) {
		pr_info("error creating file: MTL_QSSCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QSSCR4 =
	    debugfs_create_file("MTL_QSSCR4", 744, dir, &MTL_QSSCR4_val,
				&MTL_QSSCR4_fops);
	if (MTL_QSSCR4 == NULL) {
		pr_info("error creating file: MTL_QSSCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QSSCR3 =
	    debugfs_create_file("MTL_QSSCR3", 744, dir, &MTL_QSSCR3_val,
				&MTL_QSSCR3_fops);
	if (MTL_QSSCR3 == NULL) {
		pr_info("error creating file: MTL_QSSCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QSSCR2 =
	    debugfs_create_file("MTL_QSSCR2", 744, dir, &MTL_QSSCR2_val,
				&MTL_QSSCR2_fops);
	if (MTL_QSSCR2 == NULL) {
		pr_info("error creating file: MTL_QSSCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QSSCR1 =
	    debugfs_create_file("MTL_QSSCR1", 744, dir, &MTL_QSSCR1_val,
				&MTL_QSSCR1_fops);
	if (MTL_QSSCR1 == NULL) {
		pr_info("error creating file: MTL_QSSCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QW7 =
	    debugfs_create_file("MTL_QW7", 744, dir, &MTL_QW7_val,
				&MTL_QW7_fops);
	if (MTL_QW7 == NULL) {
		pr_info("error creating file: MTL_QW7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QW6 =
	    debugfs_create_file("MTL_QW6", 744, dir, &MTL_QW6_val,
				&MTL_QW6_fops);
	if (MTL_QW6 == NULL) {
		pr_info("error creating file: MTL_QW6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QW5 =
	    debugfs_create_file("MTL_QW5", 744, dir, &MTL_QW5_val,
				&MTL_QW5_fops);
	if (MTL_QW5 == NULL) {
		pr_info("error creating file: MTL_QW5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QW4 =
	    debugfs_create_file("MTL_QW4", 744, dir, &MTL_QW4_val,
				&MTL_QW4_fops);
	if (MTL_QW4 == NULL) {
		pr_info("error creating file: MTL_QW4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QW3 =
	    debugfs_create_file("MTL_QW3", 744, dir, &MTL_QW3_val,
				&MTL_QW3_fops);
	if (MTL_QW3 == NULL) {
		pr_info("error creating file: MTL_QW3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QW2 =
	    debugfs_create_file("MTL_QW2", 744, dir, &MTL_QW2_val,
				&MTL_QW2_fops);
	if (MTL_QW2 == NULL) {
		pr_info("error creating file: MTL_QW2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QW1 =
	    debugfs_create_file("MTL_QW1", 744, dir, &MTL_QW1_val,
				&MTL_QW1_fops);
	if (MTL_QW1 == NULL) {
		pr_info("error creating file: MTL_QW1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QESR7 =
	    debugfs_create_file("MTL_QESR7", 744, dir, &MTL_QESR7_val,
				&MTL_QESR7_fops);
	if (MTL_QESR7 == NULL) {
		pr_info("error creating file: MTL_QESR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QESR6 =
	    debugfs_create_file("MTL_QESR6", 744, dir, &MTL_QESR6_val,
				&MTL_QESR6_fops);
	if (MTL_QESR6 == NULL) {
		pr_info("error creating file: MTL_QESR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QESR5 =
	    debugfs_create_file("MTL_QESR5", 744, dir, &MTL_QESR5_val,
				&MTL_QESR5_fops);
	if (MTL_QESR5 == NULL) {
		pr_info("error creating file: MTL_QESR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QESR4 =
	    debugfs_create_file("MTL_QESR4", 744, dir, &MTL_QESR4_val,
				&MTL_QESR4_fops);
	if (MTL_QESR4 == NULL) {
		pr_info("error creating file: MTL_QESR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QESR3 =
	    debugfs_create_file("MTL_QESR3", 744, dir, &MTL_QESR3_val,
				&MTL_QESR3_fops);
	if (MTL_QESR3 == NULL) {
		pr_info("error creating file: MTL_QESR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QESR2 =
	    debugfs_create_file("MTL_QESR2", 744, dir, &MTL_QESR2_val,
				&MTL_QESR2_fops);
	if (MTL_QESR2 == NULL) {
		pr_info("error creating file: MTL_QESR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QESR1 =
	    debugfs_create_file("MTL_QESR1", 744, dir, &MTL_QESR1_val,
				&MTL_QESR1_fops);
	if (MTL_QESR1 == NULL) {
		pr_info("error creating file: MTL_QESR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QECR7 =
	    debugfs_create_file("MTL_QECR7", 744, dir, &MTL_QECR7_val,
				&MTL_QECR7_fops);
	if (MTL_QECR7 == NULL) {
		pr_info("error creating file: MTL_QECR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QECR6 =
	    debugfs_create_file("MTL_QECR6", 744, dir, &MTL_QECR6_val,
				&MTL_QECR6_fops);
	if (MTL_QECR6 == NULL) {
		pr_info("error creating file: MTL_QECR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QECR5 =
	    debugfs_create_file("MTL_QECR5", 744, dir, &MTL_QECR5_val,
				&MTL_QECR5_fops);
	if (MTL_QECR5 == NULL) {
		pr_info("error creating file: MTL_QECR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QECR4 =
	    debugfs_create_file("MTL_QECR4", 744, dir, &MTL_QECR4_val,
				&MTL_QECR4_fops);
	if (MTL_QECR4 == NULL) {
		pr_info("error creating file: MTL_QECR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QECR3 =
	    debugfs_create_file("MTL_QECR3", 744, dir, &MTL_QECR3_val,
				&MTL_QECR3_fops);
	if (MTL_QECR3 == NULL) {
		pr_info("error creating file: MTL_QECR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QECR2 =
	    debugfs_create_file("MTL_QECR2", 744, dir, &MTL_QECR2_val,
				&MTL_QECR2_fops);
	if (MTL_QECR2 == NULL) {
		pr_info("error creating file: MTL_QECR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QECR1 =
	    debugfs_create_file("MTL_QECR1", 744, dir, &MTL_QECR1_val,
				&MTL_QECR1_fops);
	if (MTL_QECR1 == NULL) {
		pr_info("error creating file: MTL_QECR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTDR7 =
	    debugfs_create_file("MTL_QTDR7", 744, dir, &MTL_QTDR7_val,
				&MTL_QTDR7_fops);
	if (MTL_QTDR7 == NULL) {
		pr_info("error creating file: MTL_QTDR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTDR6 =
	    debugfs_create_file("MTL_QTDR6", 744, dir, &MTL_QTDR6_val,
				&MTL_QTDR6_fops);
	if (MTL_QTDR6 == NULL) {
		pr_info("error creating file: MTL_QTDR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTDR5 =
	    debugfs_create_file("MTL_QTDR5", 744, dir, &MTL_QTDR5_val,
				&MTL_QTDR5_fops);
	if (MTL_QTDR5 == NULL) {
		pr_info("error creating file: MTL_QTDR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTDR4 =
	    debugfs_create_file("MTL_QTDR4", 744, dir, &MTL_QTDR4_val,
				&MTL_QTDR4_fops);
	if (MTL_QTDR4 == NULL) {
		pr_info("error creating file: MTL_QTDR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTDR3 =
	    debugfs_create_file("MTL_QTDR3", 744, dir, &MTL_QTDR3_val,
				&MTL_QTDR3_fops);
	if (MTL_QTDR3 == NULL) {
		pr_info("error creating file: MTL_QTDR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTDR2 =
	    debugfs_create_file("MTL_QTDR2", 744, dir, &MTL_QTDR2_val,
				&MTL_QTDR2_fops);
	if (MTL_QTDR2 == NULL) {
		pr_info("error creating file: MTL_QTDR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTDR1 =
	    debugfs_create_file("MTL_QTDR1", 744, dir, &MTL_QTDR1_val,
				&MTL_QTDR1_fops);
	if (MTL_QTDR1 == NULL) {
		pr_info("error creating file: MTL_QTDR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QUCR7 =
	    debugfs_create_file("MTL_QUCR7", 744, dir, &MTL_QUCR7_val,
				&MTL_QUCR7_fops);
	if (MTL_QUCR7 == NULL) {
		pr_info("error creating file: MTL_QUCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QUCR6 =
	    debugfs_create_file("MTL_QUCR6", 744, dir, &MTL_QUCR6_val,
				&MTL_QUCR6_fops);
	if (MTL_QUCR6 == NULL) {
		pr_info("error creating file: MTL_QUCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QUCR5 =
	    debugfs_create_file("MTL_QUCR5", 744, dir, &MTL_QUCR5_val,
				&MTL_QUCR5_fops);
	if (MTL_QUCR5 == NULL) {
		pr_info("error creating file: MTL_QUCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QUCR4 =
	    debugfs_create_file("MTL_QUCR4", 744, dir, &MTL_QUCR4_val,
				&MTL_QUCR4_fops);
	if (MTL_QUCR4 == NULL) {
		pr_info("error creating file: MTL_QUCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QUCR3 =
	    debugfs_create_file("MTL_QUCR3", 744, dir, &MTL_QUCR3_val,
				&MTL_QUCR3_fops);
	if (MTL_QUCR3 == NULL) {
		pr_info("error creating file: MTL_QUCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QUCR2 =
	    debugfs_create_file("MTL_QUCR2", 744, dir, &MTL_QUCR2_val,
				&MTL_QUCR2_fops);
	if (MTL_QUCR2 == NULL) {
		pr_info("error creating file: MTL_QUCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QUCR1 =
	    debugfs_create_file("MTL_QUCR1", 744, dir, &MTL_QUCR1_val,
				&MTL_QUCR1_fops);
	if (MTL_QUCR1 == NULL) {
		pr_info("error creating file: MTL_QUCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTOMR7 =
	    debugfs_create_file("MTL_QTOMR7", 744, dir, &MTL_QTOMR7_val,
				&MTL_QTOMR7_fops);
	if (MTL_QTOMR7 == NULL) {
		pr_info("error creating file: MTL_QTOMR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTOMR6 =
	    debugfs_create_file("MTL_QTOMR6", 744, dir, &MTL_QTOMR6_val,
				&MTL_QTOMR6_fops);
	if (MTL_QTOMR6 == NULL) {
		pr_info("error creating file: MTL_QTOMR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTOMR5 =
	    debugfs_create_file("MTL_QTOMR5", 744, dir, &MTL_QTOMR5_val,
				&MTL_QTOMR5_fops);
	if (MTL_QTOMR5 == NULL) {
		pr_info("error creating file: MTL_QTOMR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTOMR4 =
	    debugfs_create_file("MTL_QTOMR4", 744, dir, &MTL_QTOMR4_val,
				&MTL_QTOMR4_fops);
	if (MTL_QTOMR4 == NULL) {
		pr_info("error creating file: MTL_QTOMR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTOMR3 =
	    debugfs_create_file("MTL_QTOMR3", 744, dir, &MTL_QTOMR3_val,
				&MTL_QTOMR3_fops);
	if (MTL_QTOMR3 == NULL) {
		pr_info("error creating file: MTL_QTOMR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTOMR2 =
	    debugfs_create_file("MTL_QTOMR2", 744, dir, &MTL_QTOMR2_val,
				&MTL_QTOMR2_fops);
	if (MTL_QTOMR2 == NULL) {
		pr_info("error creating file: MTL_QTOMR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_QTOMR1 =
	    debugfs_create_file("MTL_QTOMR1", 744, dir, &MTL_QTOMR1_val,
				&MTL_QTOMR1_fops);
	if (MTL_QTOMR1 == NULL) {
		pr_info("error creating file: MTL_QTOMR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_PMTCSR =
	    debugfs_create_file("MAC_PMTCSR", 744, dir, &mac_pmtcsr_val,
				&mac_pmtcsr_fops);
	if (MAC_PMTCSR == NULL) {
		pr_info("error creating file: MAC_PMTCSR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXICMP_ERR_OCTETS =
	    debugfs_create_file("MMC_RXICMP_ERR_OCTETS", 744, dir,
				&mmc_rxicmp_err_octets_val,
				&mmc_rxicmp_err_octets_fops);
	if (MMC_RXICMP_ERR_OCTETS == NULL) {
		pr_info(
		       "error creating file: MMC_RXICMP_ERR_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXICMP_GD_OCTETS =
	    debugfs_create_file("MMC_RXICMP_GD_OCTETS", 744, dir,
				&mmc_rxicmp_gd_octets_val,
				&mmc_rxicmp_gd_octets_fops);
	if (MMC_RXICMP_GD_OCTETS == NULL) {
		pr_info("error creating file: MMC_RXICMP_GD_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXTCP_ERR_OCTETS =
	    debugfs_create_file("MMC_RXTCP_ERR_OCTETS", 744, dir,
				&mmc_rxtcp_err_octets_val,
				&mmc_rxtcp_err_octets_fops);
	if (MMC_RXTCP_ERR_OCTETS == NULL) {
		pr_info("error creating file: MMC_RXTCP_ERR_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXTCP_GD_OCTETS =
	    debugfs_create_file("MMC_RXTCP_GD_OCTETS", 744, dir,
				&mmc_rxtcp_gd_octets_val,
				&mmc_rxtcp_gd_octets_fops);
	if (MMC_RXTCP_GD_OCTETS == NULL) {
		pr_info("error creating file: MMC_RXTCP_GD_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXUDP_ERR_OCTETS =
	    debugfs_create_file("MMC_RXUDP_ERR_OCTETS", 744, dir,
				&mmc_rxudp_err_octets_val,
				&mmc_rxudp_err_octets_fops);
	if (MMC_RXUDP_ERR_OCTETS == NULL) {
		pr_info("error creating file: MMC_RXUDP_ERR_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXUDP_GD_OCTETS =
	    debugfs_create_file("MMC_RXUDP_GD_OCTETS", 744, dir,
				&mmc_rxudp_gd_octets_val,
				&mmc_rxudp_gd_octets_fops);
	if (MMC_RXUDP_GD_OCTETS == NULL) {
		pr_info("error creating file: MMC_RXUDP_GD_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV6_NOPAY_OCTETS =
	    debugfs_create_file("MMC_RXIPV6_NOPAY_OCTETS", 744, dir,
				&MMC_RXIPV6_nopay_octets_val,
				&MMC_RXIPV6_nopay_octets_fops);
	if (MMC_RXIPV6_NOPAY_OCTETS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV6_NOPAY_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV6_HDRERR_OCTETS =
	    debugfs_create_file("MMC_RXIPV6_HDRERR_OCTETS", 744, dir,
				&MMC_RXIPV6_hdrerr_octets_val,
				&MMC_RXIPV6_hdrerr_octets_fops);
	if (MMC_RXIPV6_HDRERR_OCTETS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV6_HDRERR_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV6_GD_OCTETS =
	    debugfs_create_file("MMC_RXIPV6_GD_OCTETS", 744, dir,
				&MMC_RXIPV6_gd_octets_val,
				&MMC_RXIPV6_gd_octets_fops);
	if (MMC_RXIPV6_GD_OCTETS == NULL) {
		pr_info("error creating file: MMC_RXIPV6_GD_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_UDSBL_OCTETS =
	    debugfs_create_file("MMC_RXIPV4_UDSBL_OCTETS", 744, dir,
				&MMC_RXIPV4_udsbl_octets_val,
				&MMC_RXIPV4_udsbl_octets_fops);
	if (MMC_RXIPV4_UDSBL_OCTETS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV4_UDSBL_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_FRAG_OCTETS =
	    debugfs_create_file("MMC_RXIPV4_FRAG_OCTETS", 744, dir,
				&MMC_RXIPV4_frag_octets_val,
				&MMC_RXIPV4_frag_octets_fops);
	if (MMC_RXIPV4_FRAG_OCTETS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV4_FRAG_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_NOPAY_OCTETS =
	    debugfs_create_file("MMC_RXIPV4_NOPAY_OCTETS", 744, dir,
				&MMC_RXIPV4_nopay_octets_val,
				&MMC_RXIPV4_nopay_octets_fops);
	if (MMC_RXIPV4_NOPAY_OCTETS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV4_NOPAY_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_HDRERR_OCTETS =
	    debugfs_create_file("MMC_RXIPV4_HDRERR_OCTETS", 744, dir,
				&MMC_RXIPV4_hdrerr_octets_val,
				&MMC_RXIPV4_hdrerr_octets_fops);
	if (MMC_RXIPV4_HDRERR_OCTETS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV4_HDRERR_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_GD_OCTETS =
	    debugfs_create_file("MMC_RXIPV4_GD_OCTETS", 744, dir,
				&MMC_RXIPV4_gd_octets_val,
				&MMC_RXIPV4_gd_octets_fops);
	if (MMC_RXIPV4_GD_OCTETS == NULL) {
		pr_info("error creating file: MMC_RXIPV4_GD_OCTETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXICMP_ERR_PKTS =
	    debugfs_create_file("MMC_RXICMP_ERR_PKTS", 744, dir,
				&mmc_rxicmp_err_pkts_val,
				&mmc_rxicmp_err_pkts_fops);
	if (MMC_RXICMP_ERR_PKTS == NULL) {
		pr_info("error creating file: MMC_RXICMP_ERR_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXICMP_GD_PKTS =
	    debugfs_create_file("MMC_RXICMP_GD_PKTS", 744, dir,
				&mmc_rxicmp_gd_pkts_val,
				&mmc_rxicmp_gd_pkts_fops);
	if (MMC_RXICMP_GD_PKTS == NULL) {
		pr_info("error creating file: MMC_RXICMP_GD_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXTCP_ERR_PKTS =
	    debugfs_create_file("MMC_RXTCP_ERR_PKTS", 744, dir,
				&mmc_rxtcp_err_pkts_val,
				&mmc_rxtcp_err_pkts_fops);
	if (MMC_RXTCP_ERR_PKTS == NULL) {
		pr_info("error creating file: MMC_RXTCP_ERR_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXTCP_GD_PKTS =
	    debugfs_create_file("MMC_RXTCP_GD_PKTS", 744, dir,
				&mmc_rxtcp_gd_pkts_val,
				&mmc_rxtcp_gd_pkts_fops);
	if (MMC_RXTCP_GD_PKTS == NULL) {
		pr_info("error creating file: MMC_RXTCP_GD_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXUDP_ERR_PKTS =
	    debugfs_create_file("MMC_RXUDP_ERR_PKTS", 744, dir,
				&mmc_rxudp_err_pkts_val,
				&mmc_rxudp_err_pkts_fops);
	if (MMC_RXUDP_ERR_PKTS == NULL) {
		pr_info("error creating file: MMC_RXUDP_ERR_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXUDP_GD_PKTS =
	    debugfs_create_file("MMC_RXUDP_GD_PKTS", 744, dir,
				&mmc_rxudp_gd_pkts_val,
				&mmc_rxudp_gd_pkts_fops);
	if (MMC_RXUDP_GD_PKTS == NULL) {
		pr_info("error creating file: MMC_RXUDP_GD_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV6_NOPAY_PKTS =
	    debugfs_create_file("MMC_RXIPV6_NOPAY_PKTS", 744, dir,
				&MMC_RXIPV6_nopay_pkts_val,
				&MMC_RXIPV6_nopay_pkts_fops);
	if (MMC_RXIPV6_NOPAY_PKTS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV6_NOPAY_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV6_HDRERR_PKTS =
	    debugfs_create_file("MMC_RXIPV6_HDRERR_PKTS", 744, dir,
				&MMC_RXIPV6_hdrerr_pkts_val,
				&MMC_RXIPV6_hdrerr_pkts_fops);
	if (MMC_RXIPV6_HDRERR_PKTS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV6_HDRERR_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV6_GD_PKTS =
	    debugfs_create_file("MMC_RXIPV6_GD_PKTS", 744, dir,
				&MMC_RXIPV6_gd_pkts_val,
				&MMC_RXIPV6_gd_pkts_fops);
	if (MMC_RXIPV6_GD_PKTS == NULL) {
		pr_info("error creating file: MMC_RXIPV6_GD_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_UBSBL_PKTS =
	    debugfs_create_file("MMC_RXIPV4_UBSBL_PKTS", 744, dir,
				&MMC_RXIPV4_ubsbl_pkts_val,
				&MMC_RXIPV4_ubsbl_pkts_fops);
	if (MMC_RXIPV4_UBSBL_PKTS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV4_UBSBL_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_FRAG_PKTS =
	    debugfs_create_file("MMC_RXIPV4_FRAG_PKTS", 744, dir,
				&MMC_RXIPV4_frag_pkts_val,
				&MMC_RXIPV4_frag_pkts_fops);
	if (MMC_RXIPV4_FRAG_PKTS == NULL) {
		pr_info("error creating file: MMC_RXIPV4_FRAG_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_NOPAY_PKTS =
	    debugfs_create_file("MMC_RXIPV4_NOPAY_PKTS", 744, dir,
				&MMC_RXIPV4_nopay_pkts_val,
				&MMC_RXIPV4_nopay_pkts_fops);
	if (MMC_RXIPV4_NOPAY_PKTS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV4_NOPAY_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_HDRERR_PKTS =
	    debugfs_create_file("MMC_RXIPV4_HDRERR_PKTS", 744, dir,
				&MMC_RXIPV4_hdrerr_pkts_val,
				&MMC_RXIPV4_hdrerr_pkts_fops);
	if (MMC_RXIPV4_HDRERR_PKTS == NULL) {
		pr_info(
		       "error creating file: MMC_RXIPV4_HDRERR_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXIPV4_GD_PKTS =
	    debugfs_create_file("MMC_RXIPV4_GD_PKTS", 744, dir,
				&MMC_RXIPV4_gd_pkts_val,
				&MMC_RXIPV4_gd_pkts_fops);
	if (MMC_RXIPV4_GD_PKTS == NULL) {
		pr_info("error creating file: MMC_RXIPV4_GD_PKTS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXCTRLPACKETS_G =
	    debugfs_create_file("MMC_RXCTRLPACKETS_G", 744, dir,
				&mmc_rxctrlpackets_g_val,
				&mmc_rxctrlpackets_g_fops);
	if (MMC_RXCTRLPACKETS_G == NULL) {
		pr_info("error creating file: MMC_RXCTRLPACKETS_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXRCVERROR =
	    debugfs_create_file("MMC_RXRCVERROR", 744, dir, &mmc_rxrcverror_val,
				&mmc_rxrcverror_fops);
	if (MMC_RXRCVERROR == NULL) {
		pr_info("error creating file: MMC_RXRCVERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXWATCHDOGERROR =
	    debugfs_create_file("MMC_RXWATCHDOGERROR", 744, dir,
				&mmc_rxwatchdogerror_val,
				&mmc_rxwatchdogerror_fops);
	if (MMC_RXWATCHDOGERROR == NULL) {
		pr_info("error creating file: MMC_RXWATCHDOGERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXVLANPACKETS_GB =
	    debugfs_create_file("MMC_RXVLANPACKETS_GB", 744, dir,
				&mmc_rxvlanpackets_gb_val,
				&mmc_rxvlanpackets_gb_fops);
	if (MMC_RXVLANPACKETS_GB == NULL) {
		pr_info("error creating file: MMC_RXVLANPACKETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXFIFOOVERFLOW =
	    debugfs_create_file("MMC_RXFIFOOVERFLOW", 744, dir,
				&mmc_rxfifooverflow_val,
				&mmc_rxfifooverflow_fops);
	if (MMC_RXFIFOOVERFLOW == NULL) {
		pr_info("error creating file: MMC_RXFIFOOVERFLOW\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXPAUSEPACKETS =
	    debugfs_create_file("MMC_RXPAUSEPACKETS", 744, dir,
				&mmc_rxpausepackets_val,
				&mmc_rxpausepackets_fops);
	if (MMC_RXPAUSEPACKETS == NULL) {
		pr_info("error creating file: MMC_RXPAUSEPACKETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXOUTOFRANGETYPE =
	    debugfs_create_file("MMC_RXOUTOFRANGETYPE", 744, dir,
				&mmc_rxoutofrangetype_val,
				&mmc_rxoutofrangetype_fops);
	if (MMC_RXOUTOFRANGETYPE == NULL) {
		pr_info("error creating file: MMC_RXOUTOFRANGETYPE\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXLENGTHERROR =
	    debugfs_create_file("MMC_RXLENGTHERROR", 744, dir,
				&mmc_rxlengtherror_val,
				&mmc_rxlengtherror_fops);
	if (MMC_RXLENGTHERROR == NULL) {
		pr_info("error creating file: MMC_RXLENGTHERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXUNICASTPACKETS_G =
	    debugfs_create_file("MMC_RXUNICASTPACKETS_G", 744, dir,
				&mmc_rxunicastpackets_g_val,
				&mmc_rxunicastpackets_g_fops);
	if (MMC_RXUNICASTPACKETS_G == NULL) {
		pr_info(
		       "error creating file: MMC_RXUNICASTPACKETS_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RX1024TOMAXOCTETS_GB =
	    debugfs_create_file("MMC_RX1024TOMAXOCTETS_GB", 744, dir,
				&MMC_RX1024tomaxoctets_gb_val,
				&MMC_RX1024tomaxoctets_gb_fops);
	if (MMC_RX1024TOMAXOCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_RX1024TOMAXOCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RX512TO1023OCTETS_GB =
	    debugfs_create_file("MMC_RX512TO1023OCTETS_GB", 744, dir,
				&MMC_RX512TO1023octets_gb_val,
				&MMC_RX512TO1023octets_gb_fops);
	if (MMC_RX512TO1023OCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_RX512TO1023OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RX256TO511OCTETS_GB =
	    debugfs_create_file("MMC_RX256TO511OCTETS_GB", 744, dir,
				&MMC_RX256TO511octets_gb_val,
				&MMC_RX256TO511octets_gb_fops);
	if (MMC_RX256TO511OCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_RX256TO511OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RX128TO255OCTETS_GB =
	    debugfs_create_file("MMC_RX128TO255OCTETS_GB", 744, dir,
				&MMC_RX128TO255octets_gb_val,
				&MMC_RX128TO255octets_gb_fops);
	if (MMC_RX128TO255OCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_RX128TO255OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RX65TO127OCTETS_GB =
	    debugfs_create_file("MMC_RX65TO127OCTETS_GB", 744, dir,
				&MMC_RX65TO127octets_gb_val,
				&MMC_RX65TO127octets_gb_fops);
	if (MMC_RX65TO127OCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_RX65TO127OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RX64OCTETS_GB =
	    debugfs_create_file("MMC_RX64OCTETS_GB", 744, dir,
				&MMC_RX64octets_gb_val,
				&MMC_RX64octets_gb_fops);
	if (MMC_RX64OCTETS_GB == NULL) {
		pr_info("error creating file: MMC_RX64OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXOVERSIZE_G =
	    debugfs_create_file("MMC_RXOVERSIZE_G", 744, dir,
				&mmc_rxoversize_g_val, &mmc_rxoversize_g_fops);
	if (MMC_RXOVERSIZE_G == NULL) {
		pr_info("error creating file: MMC_RXOVERSIZE_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXUNDERSIZE_G =
	    debugfs_create_file("MMC_RXUNDERSIZE_G", 744, dir,
				&mmc_rxundersize_g_val,
				&mmc_rxundersize_g_fops);
	if (MMC_RXUNDERSIZE_G == NULL) {
		pr_info("error creating file: MMC_RXUNDERSIZE_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXJABBERERROR =
	    debugfs_create_file("MMC_RXJABBERERROR", 744, dir,
				&mmc_rxjabbererror_val,
				&mmc_rxjabbererror_fops);
	if (MMC_RXJABBERERROR == NULL) {
		pr_info("error creating file: MMC_RXJABBERERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXRUNTERROR =
	    debugfs_create_file("MMC_RXRUNTERROR", 744, dir,
				&mmc_rxrunterror_val, &mmc_rxrunterror_fops);
	if (MMC_RXRUNTERROR == NULL) {
		pr_info("error creating file: MMC_RXRUNTERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXALIGNMENTERROR =
	    debugfs_create_file("MMC_RXALIGNMENTERROR", 744, dir,
				&mmc_rxalignmenterror_val,
				&mmc_rxalignmenterror_fops);
	if (MMC_RXALIGNMENTERROR == NULL) {
		pr_info("error creating file: MMC_RXALIGNMENTERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXCRCERROR =
	    debugfs_create_file("MMC_RXCRCERROR", 744, dir, &mmc_rxcrcerror_val,
				&mmc_rxcrcerror_fops);
	if (MMC_RXCRCERROR == NULL) {
		pr_info("error creating file: MMC_RXCRCERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXMULTICASTPACKETS_G =
	    debugfs_create_file("MMC_RXMULTICASTPACKETS_G", 744, dir,
				&mmc_rxmulticastpackets_g_val,
				&mmc_rxmulticastpackets_g_fops);
	if (MMC_RXMULTICASTPACKETS_G == NULL) {
		pr_info(
		       "error creating file: MMC_RXMULTICASTPACKETS_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXBROADCASTPACKETS_G =
	    debugfs_create_file("MMC_RXBROADCASTPACKETS_G", 744, dir,
				&mmc_rxbroadcastpackets_g_val,
				&mmc_rxbroadcastpackets_g_fops);
	if (MMC_RXBROADCASTPACKETS_G == NULL) {
		pr_info(
		       "error creating file: MMC_RXBROADCASTPACKETS_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXOCTETCOUNT_G =
	    debugfs_create_file("MMC_RXOCTETCOUNT_G", 744, dir,
				&mmc_rxoctetcount_g_val,
				&mmc_rxoctetcount_g_fops);
	if (MMC_RXOCTETCOUNT_G == NULL) {
		pr_info("error creating file: MMC_RXOCTETCOUNT_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXOCTETCOUNT_GB =
	    debugfs_create_file("MMC_RXOCTETCOUNT_GB", 744, dir,
				&mmc_rxoctetcount_gb_val,
				&mmc_rxoctetcount_gb_fops);
	if (MMC_RXOCTETCOUNT_GB == NULL) {
		pr_info("error creating file: MMC_RXOCTETCOUNT_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_RXPACKETCOUNT_GB =
	    debugfs_create_file("MMC_RXPACKETCOUNT_GB", 744, dir,
				&mmc_rxpacketcount_gb_val,
				&mmc_rxpacketcount_gb_fops);
	if (MMC_RXPACKETCOUNT_GB == NULL) {
		pr_info("error creating file: MMC_RXPACKETCOUNT_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXOVERSIZE_G =
	    debugfs_create_file("MMC_TXOVERSIZE_G", 744, dir,
				&mmc_txoversize_g_val, &mmc_txoversize_g_fops);
	if (MMC_TXOVERSIZE_G == NULL) {
		pr_info("error creating file: MMC_TXOVERSIZE_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXVLANPACKETS_G =
	    debugfs_create_file("MMC_TXVLANPACKETS_G", 744, dir,
				&mmc_txvlanpackets_g_val,
				&mmc_txvlanpackets_g_fops);
	if (MMC_TXVLANPACKETS_G == NULL) {
		pr_info("error creating file: MMC_TXVLANPACKETS_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXPAUSEPACKETS =
	    debugfs_create_file("MMC_TXPAUSEPACKETS", 744, dir,
				&mmc_txpausepackets_val,
				&mmc_txpausepackets_fops);
	if (MMC_TXPAUSEPACKETS == NULL) {
		pr_info("error creating file: MMC_TXPAUSEPACKETS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXEXCESSDEF =
	    debugfs_create_file("MMC_TXEXCESSDEF", 744, dir,
				&mmc_txexcessdef_val, &mmc_txexcessdef_fops);
	if (MMC_TXEXCESSDEF == NULL) {
		pr_info("error creating file: MMC_TXEXCESSDEF\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXPACKETSCOUNT_G =
	    debugfs_create_file("MMC_TXPACKETSCOUNT_G", 744, dir,
				&mmc_txpacketscount_g_val,
				&mmc_txpacketscount_g_fops);
	if (MMC_TXPACKETSCOUNT_G == NULL) {
		pr_info("error creating file: MMC_TXPACKETSCOUNT_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXOCTETCOUNT_G =
	    debugfs_create_file("MMC_TXOCTETCOUNT_G", 744, dir,
				&mmc_txoctetcount_g_val,
				&mmc_txoctetcount_g_fops);
	if (MMC_TXOCTETCOUNT_G == NULL) {
		pr_info("error creating file: MMC_TXOCTETCOUNT_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXCARRIERERROR =
	    debugfs_create_file("MMC_TXCARRIERERROR", 744, dir,
				&mmc_txcarriererror_val,
				&mmc_txcarriererror_fops);
	if (MMC_TXCARRIERERROR == NULL) {
		pr_info("error creating file: MMC_TXCARRIERERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXEXESSCOL =
	    debugfs_create_file("MMC_TXEXESSCOL", 744, dir, &mmc_txexesscol_val,
				&mmc_txexesscol_fops);
	if (MMC_TXEXESSCOL == NULL) {
		pr_info("error creating file: MMC_TXEXESSCOL\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXLATECOL =
	    debugfs_create_file("MMC_TXLATECOL", 744, dir, &mmc_txlatecol_val,
				&mmc_txlatecol_fops);
	if (MMC_TXLATECOL == NULL) {
		pr_info("error creating file: MMC_TXLATECOL\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXDEFERRED =
	    debugfs_create_file("MMC_TXDEFERRED", 744, dir, &mmc_txdeferred_val,
				&mmc_txdeferred_fops);
	if (MMC_TXDEFERRED == NULL) {
		pr_info("error creating file: MMC_TXDEFERRED\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXMULTICOL_G =
	    debugfs_create_file("MMC_TXMULTICOL_G", 744, dir,
				&mmc_txmulticol_g_val, &mmc_txmulticol_g_fops);
	if (MMC_TXMULTICOL_G == NULL) {
		pr_info("error creating file: MMC_TXMULTICOL_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXSINGLECOL_G =
	    debugfs_create_file("MMC_TXSINGLECOL_G", 744, dir,
				&mmc_txsinglecol_g_val,
				&mmc_txsinglecol_g_fops);
	if (MMC_TXSINGLECOL_G == NULL) {
		pr_info("error creating file: MMC_TXSINGLECOL_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXUNDERFLOWERROR =
	    debugfs_create_file("MMC_TXUNDERFLOWERROR", 744, dir,
				&mmc_txunderflowerror_val,
				&mmc_txunderflowerror_fops);
	if (MMC_TXUNDERFLOWERROR == NULL) {
		pr_info("error creating file: MMC_TXUNDERFLOWERROR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXBROADCASTPACKETS_GB =
	    debugfs_create_file("MMC_TXBROADCASTPACKETS_GB", 744, dir,
				&mmc_txbroadcastpackets_gb_val,
				&mmc_txbroadcastpackets_gb_fops);
	if (MMC_TXBROADCASTPACKETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_TXBROADCASTPACKETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXMULTICASTPACKETS_GB =
	    debugfs_create_file("MMC_TXMULTICASTPACKETS_GB", 744, dir,
				&mmc_txmulticastpackets_gb_val,
				&mmc_txmulticastpackets_gb_fops);
	if (MMC_TXMULTICASTPACKETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_TXMULTICASTPACKETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXUNICASTPACKETS_GB =
	    debugfs_create_file("MMC_TXUNICASTPACKETS_GB", 744, dir,
				&mmc_txunicastpackets_gb_val,
				&mmc_txunicastpackets_gb_fops);
	if (MMC_TXUNICASTPACKETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_TXUNICASTPACKETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TX1024TOMAXOCTETS_GB =
	    debugfs_create_file("MMC_TX1024TOMAXOCTETS_GB", 744, dir,
				&MMC_TX1024tomaxoctets_gb_val,
				&MMC_TX1024tomaxoctets_gb_fops);
	if (MMC_TX1024TOMAXOCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_TX1024TOMAXOCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TX512TO1023OCTETS_GB =
	    debugfs_create_file("MMC_TX512TO1023OCTETS_GB", 744, dir,
				&MMC_TX512TO1023octets_gb_val,
				&MMC_TX512TO1023octets_gb_fops);
	if (MMC_TX512TO1023OCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_TX512TO1023OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TX256TO511OCTETS_GB =
	    debugfs_create_file("MMC_TX256TO511OCTETS_GB", 744, dir,
				&MMC_TX256TO511octets_gb_val,
				&MMC_TX256TO511octets_gb_fops);
	if (MMC_TX256TO511OCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_TX256TO511OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TX128TO255OCTETS_GB =
	    debugfs_create_file("MMC_TX128TO255OCTETS_GB", 744, dir,
				&MMC_TX128TO255octets_gb_val,
				&MMC_TX128TO255octets_gb_fops);
	if (MMC_TX128TO255OCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_TX128TO255OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TX65TO127OCTETS_GB =
	    debugfs_create_file("MMC_TX65TO127OCTETS_GB", 744, dir,
				&MMC_TX65TO127octets_gb_val,
				&MMC_TX65TO127octets_gb_fops);
	if (MMC_TX65TO127OCTETS_GB == NULL) {
		pr_info(
		       "error creating file: MMC_TX65TO127OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TX64OCTETS_GB =
	    debugfs_create_file("MMC_TX64OCTETS_GB", 744, dir,
				&MMC_TX64octets_gb_val,
				&MMC_TX64octets_gb_fops);
	if (MMC_TX64OCTETS_GB == NULL) {
		pr_info("error creating file: MMC_TX64OCTETS_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXMULTICASTPACKETS_G =
	    debugfs_create_file("MMC_TXMULTICASTPACKETS_G", 744, dir,
				&mmc_txmulticastpackets_g_val,
				&mmc_txmulticastpackets_g_fops);
	if (MMC_TXMULTICASTPACKETS_G == NULL) {
		pr_info(
		       "error creating file: MMC_TXMULTICASTPACKETS_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXBROADCASTPACKETS_G =
	    debugfs_create_file("MMC_TXBROADCASTPACKETS_G", 744, dir,
				&mmc_txbroadcastpackets_g_val,
				&mmc_txbroadcastpackets_g_fops);
	if (MMC_TXBROADCASTPACKETS_G == NULL) {
		pr_info(
		       "error creating file: MMC_TXBROADCASTPACKETS_G\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXPACKETCOUNT_GB =
	    debugfs_create_file("MMC_TXPACKETCOUNT_GB", 744, dir,
				&mmc_txpacketcount_gb_val,
				&mmc_txpacketcount_gb_fops);
	if (MMC_TXPACKETCOUNT_GB == NULL) {
		pr_info("error creating file: MMC_TXPACKETCOUNT_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_TXOCTETCOUNT_GB =
	    debugfs_create_file("MMC_TXOCTETCOUNT_GB", 744, dir,
				&mmc_txoctetcount_gb_val,
				&mmc_txoctetcount_gb_fops);
	if (MMC_TXOCTETCOUNT_GB == NULL) {
		pr_info("error creating file: MMC_TXOCTETCOUNT_GB\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_IPC_INTR_RX =
	    debugfs_create_file("MMC_IPC_INTR_RX", 744, dir,
				&mmc_ipc_intr_rx_val, &mmc_ipc_intr_rx_fops);
	if (MMC_IPC_INTR_RX == NULL) {
		pr_info("error creating file: MMC_IPC_INTR_RX\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_IPC_INTR_MASK_RX =
	    debugfs_create_file("MMC_IPC_INTR_MASK_RX", 744, dir,
				&mmc_ipc_intr_mask_rx_val,
				&mmc_ipc_intr_mask_rx_fops);
	if (MMC_IPC_INTR_MASK_RX == NULL) {
		pr_info("error creating file: MMC_IPC_INTR_MASK_RX\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_INTR_MASK_TX =
	    debugfs_create_file("MMC_INTR_MASK_TX", 744, dir,
				&mmc_intr_mask_tx_val, &mmc_intr_mask_tx_fops);
	if (MMC_INTR_MASK_TX == NULL) {
		pr_info("error creating file: MMC_INTR_MASK_TX\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_INTR_MASK_RX =
	    debugfs_create_file("MMC_INTR_MASK_RX", 744, dir,
				&mmc_intr_mask_rx_val, &mmc_intr_mask_rx_fops);
	if (MMC_INTR_MASK_RX == NULL) {
		pr_info("error creating file: MMC_INTR_MASK_RX\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_INTR_TX =
	    debugfs_create_file("MMC_INTR_TX", 744, dir, &mmc_intr_tx_val,
				&mmc_intr_tx_fops);
	if (MMC_INTR_TX == NULL) {
		pr_info("error creating file: MMC_INTR_TX\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_INTR_RX =
	    debugfs_create_file("MMC_INTR_RX", 744, dir, &mmc_intr_rx_val,
				&mmc_intr_rx_fops);
	if (MMC_INTR_RX == NULL) {
		pr_info("error creating file: MMC_INTR_RX\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MMC_CNTRL =
	    debugfs_create_file("MMC_CNTRL", 744, dir, &mmc_cntrl_val,
				&mmc_cntrl_fops);
	if (MMC_CNTRL == NULL) {
		pr_info("error creating file: MMC_CNTRL\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1LR =
	    debugfs_create_file("MAC_MA1LR", 744, dir, &MAC_MA1lr_val,
				&MAC_MA1lr_fops);
	if (MAC_MA1LR == NULL) {
		pr_info("error creating file: MAC_MA1LR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA1HR =
	    debugfs_create_file("MAC_MA1HR", 744, dir, &MAC_MA1hr_val,
				&MAC_MA1hr_fops);
	if (MAC_MA1HR == NULL) {
		pr_info("error creating file: MAC_MA1HR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA0LR =
	    debugfs_create_file("MAC_MA0LR", 744, dir, &MAC_MA0lr_val,
				&MAC_MA0lr_fops);
	if (MAC_MA0LR == NULL) {
		pr_info("error creating file: MAC_MA0LR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MA0HR =
	    debugfs_create_file("MAC_MA0HR", 744, dir, &MAC_MA0hr_val,
				&MAC_MA0hr_fops);
	if (MAC_MA0HR == NULL) {
		pr_info("error creating file: MAC_MA0HR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_GPIOR =
	    debugfs_create_file("MAC_GPIOR", 744, dir, &mac_gpior_val,
				&mac_gpior_fops);
	if (MAC_GPIOR == NULL) {
		pr_info("error creating file: MAC_GPIOR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_GMIIDR =
	    debugfs_create_file("MAC_GMIIDR", 744, dir, &mac_gmiidr_val,
				&mac_gmiidr_fops);
	if (MAC_GMIIDR == NULL) {
		pr_info("error creating file: MAC_GMIIDR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_GMIIAR =
	    debugfs_create_file("MAC_GMIIAR", 744, dir, &mac_gmiiar_val,
				&mac_gmiiar_fops);
	if (MAC_GMIIAR == NULL) {
		pr_info("error creating file: MAC_GMIIAR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HFR2 =
	    debugfs_create_file("MAC_HFR2", 744, dir, &MAC_HFR2_val,
				&MAC_HFR2_fops);
	if (MAC_HFR2 == NULL) {
		pr_info("error creating file: MAC_HFR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HFR1 =
	    debugfs_create_file("MAC_HFR1", 744, dir, &MAC_HFR1_val,
				&MAC_HFR1_fops);
	if (MAC_HFR1 == NULL) {
		pr_info("error creating file: MAC_HFR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HFR0 =
	    debugfs_create_file("MAC_HFR0", 744, dir, &MAC_HFR0_val,
				&MAC_HFR0_fops);
	if (MAC_HFR0 == NULL) {
		pr_info("error creating file: MAC_HFR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MDR =
	    debugfs_create_file("MAC_MDR", 744, dir, &mac_mdr_val,
				&mac_mdr_fops);
	if (MAC_MDR == NULL) {
		pr_info("error creating file: MAC_MDR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_VR =
	    debugfs_create_file("MAC_VR", 744, dir, &mac_vr_val, &mac_vr_fops);
	if (MAC_VR == NULL) {
		pr_info("error creating file: MAC_VR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HTR7 =
	    debugfs_create_file("MAC_HTR7", 744, dir, &MAC_HTR7_val,
				&MAC_HTR7_fops);
	if (MAC_HTR7 == NULL) {
		pr_info("error creating file: MAC_HTR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HTR6 =
	    debugfs_create_file("MAC_HTR6", 744, dir, &MAC_HTR6_val,
				&MAC_HTR6_fops);
	if (MAC_HTR6 == NULL) {
		pr_info("error creating file: MAC_HTR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HTR5 =
	    debugfs_create_file("MAC_HTR5", 744, dir, &MAC_HTR5_val,
				&MAC_HTR5_fops);
	if (MAC_HTR5 == NULL) {
		pr_info("error creating file: MAC_HTR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HTR4 =
	    debugfs_create_file("MAC_HTR4", 744, dir, &MAC_HTR4_val,
				&MAC_HTR4_fops);
	if (MAC_HTR4 == NULL) {
		pr_info("error creating file: MAC_HTR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HTR3 =
	    debugfs_create_file("MAC_HTR3", 744, dir, &MAC_HTR3_val,
				&MAC_HTR3_fops);
	if (MAC_HTR3 == NULL) {
		pr_info("error creating file: MAC_HTR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HTR2 =
	    debugfs_create_file("MAC_HTR2", 744, dir, &MAC_HTR2_val,
				&MAC_HTR2_fops);
	if (MAC_HTR2 == NULL) {
		pr_info("error creating file: MAC_HTR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HTR1 =
	    debugfs_create_file("MAC_HTR1", 744, dir, &MAC_HTR1_val,
				&MAC_HTR1_fops);
	if (MAC_HTR1 == NULL) {
		pr_info("error creating file: MAC_HTR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_HTR0 =
	    debugfs_create_file("MAC_HTR0", 744, dir, &MAC_HTR0_val,
				&MAC_HTR0_fops);
	if (MAC_HTR0 == NULL) {
		pr_info("error creating file: MAC_HTR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RIWTR7 =
	    debugfs_create_file("DMA_RIWTR7", 744, dir, &DMA_RIWTR7_val,
				&DMA_RIWTR7_fops);
	if (DMA_RIWTR7 == NULL) {
		pr_info("error creating file: DMA_RIWTR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RIWTR6 =
	    debugfs_create_file("DMA_RIWTR6", 744, dir, &DMA_RIWTR6_val,
				&DMA_RIWTR6_fops);
	if (DMA_RIWTR6 == NULL) {
		pr_info("error creating file: DMA_RIWTR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RIWTR5 =
	    debugfs_create_file("DMA_RIWTR5", 744, dir, &DMA_RIWTR5_val,
				&DMA_RIWTR5_fops);
	if (DMA_RIWTR5 == NULL) {
		pr_info("error creating file: DMA_RIWTR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RIWTR4 =
	    debugfs_create_file("DMA_RIWTR4", 744, dir, &DMA_RIWTR4_val,
				&DMA_RIWTR4_fops);
	if (DMA_RIWTR4 == NULL) {
		pr_info("error creating file: DMA_RIWTR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RIWTR3 =
	    debugfs_create_file("DMA_RIWTR3", 744, dir, &DMA_RIWTR3_val,
				&DMA_RIWTR3_fops);
	if (DMA_RIWTR3 == NULL) {
		pr_info("error creating file: DMA_RIWTR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RIWTR2 =
	    debugfs_create_file("DMA_RIWTR2", 744, dir, &DMA_RIWTR2_val,
				&DMA_RIWTR2_fops);
	if (DMA_RIWTR2 == NULL) {
		pr_info("error creating file: DMA_RIWTR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RIWTR1 =
	    debugfs_create_file("DMA_RIWTR1", 744, dir, &DMA_RIWTR1_val,
				&DMA_RIWTR1_fops);
	if (DMA_RIWTR1 == NULL) {
		pr_info("error creating file: DMA_RIWTR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RIWTR0 =
	    debugfs_create_file("DMA_RIWTR0", 744, dir, &DMA_RIWTR0_val,
				&DMA_RIWTR0_fops);
	if (DMA_RIWTR0 == NULL) {
		pr_info("error creating file: DMA_RIWTR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDRLR7 =
	    debugfs_create_file("DMA_RDRLR7", 744, dir, &DMA_RDRLR7_val,
				&DMA_RDRLR7_fops);
	if (DMA_RDRLR7 == NULL) {
		pr_info("error creating file: DMA_RDRLR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDRLR6 =
	    debugfs_create_file("DMA_RDRLR6", 744, dir, &DMA_RDRLR6_val,
				&DMA_RDRLR6_fops);
	if (DMA_RDRLR6 == NULL) {
		pr_info("error creating file: DMA_RDRLR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDRLR5 =
	    debugfs_create_file("DMA_RDRLR5", 744, dir, &DMA_RDRLR5_val,
				&DMA_RDRLR5_fops);
	if (DMA_RDRLR5 == NULL) {
		pr_info("error creating file: DMA_RDRLR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDRLR4 =
	    debugfs_create_file("DMA_RDRLR4", 744, dir, &DMA_RDRLR4_val,
				&DMA_RDRLR4_fops);
	if (DMA_RDRLR4 == NULL) {
		pr_info("error creating file: DMA_RDRLR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDRLR3 =
	    debugfs_create_file("DMA_RDRLR3", 744, dir, &DMA_RDRLR3_val,
				&DMA_RDRLR3_fops);
	if (DMA_RDRLR3 == NULL) {
		pr_info("error creating file: DMA_RDRLR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDRLR2 =
	    debugfs_create_file("DMA_RDRLR2", 744, dir, &DMA_RDRLR2_val,
				&DMA_RDRLR2_fops);
	if (DMA_RDRLR2 == NULL) {
		pr_info("error creating file: DMA_RDRLR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDRLR1 =
	    debugfs_create_file("DMA_RDRLR1", 744, dir, &DMA_RDRLR1_val,
				&DMA_RDRLR1_fops);
	if (DMA_RDRLR1 == NULL) {
		pr_info("error creating file: DMA_RDRLR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDRLR0 =
	    debugfs_create_file("DMA_RDRLR0", 744, dir, &DMA_RDRLR0_val,
				&DMA_RDRLR0_fops);
	if (DMA_RDRLR0 == NULL) {
		pr_info("error creating file: DMA_RDRLR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDRLR7 =
	    debugfs_create_file("DMA_TDRLR7", 744, dir, &DMA_TDRLR7_val,
				&DMA_TDRLR7_fops);
	if (DMA_TDRLR7 == NULL) {
		pr_info("error creating file: DMA_TDRLR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDRLR6 =
	    debugfs_create_file("DMA_TDRLR6", 744, dir, &DMA_TDRLR6_val,
				&DMA_TDRLR6_fops);
	if (DMA_TDRLR6 == NULL) {
		pr_info("error creating file: DMA_TDRLR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDRLR5 =
	    debugfs_create_file("DMA_TDRLR5", 744, dir, &DMA_TDRLR5_val,
				&DMA_TDRLR5_fops);
	if (DMA_TDRLR5 == NULL) {
		pr_info("error creating file: DMA_TDRLR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDRLR4 =
	    debugfs_create_file("DMA_TDRLR4", 744, dir, &DMA_TDRLR4_val,
				&DMA_TDRLR4_fops);
	if (DMA_TDRLR4 == NULL) {
		pr_info("error creating file: DMA_TDRLR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDRLR3 =
	    debugfs_create_file("DMA_TDRLR3", 744, dir, &DMA_TDRLR3_val,
				&DMA_TDRLR3_fops);
	if (DMA_TDRLR3 == NULL) {
		pr_info("error creating file: DMA_TDRLR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDRLR2 =
	    debugfs_create_file("DMA_TDRLR2", 744, dir, &DMA_TDRLR2_val,
				&DMA_TDRLR2_fops);
	if (DMA_TDRLR2 == NULL) {
		pr_info("error creating file: DMA_TDRLR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDRLR1 =
	    debugfs_create_file("DMA_TDRLR1", 744, dir, &DMA_TDRLR1_val,
				&DMA_TDRLR1_fops);
	if (DMA_TDRLR1 == NULL) {
		pr_info("error creating file: DMA_TDRLR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDRLR0 =
	    debugfs_create_file("DMA_TDRLR0", 744, dir, &DMA_TDRLR0_val,
				&DMA_TDRLR0_fops);
	if (DMA_TDRLR0 == NULL) {
		pr_info("error creating file: DMA_TDRLR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDTP_RPDR7 =
	    debugfs_create_file("DMA_RDTP_RPDR7", 744, dir, &DMA_RDTP_RPDR7_val,
				&DMA_RDTP_RPDR7_fops);
	if (DMA_RDTP_RPDR7 == NULL) {
		pr_info("error creating file: DMA_RDTP_RPDR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDTP_RPDR6 =
	    debugfs_create_file("DMA_RDTP_RPDR6", 744, dir, &DMA_RDTP_RPDR6_val,
				&DMA_RDTP_RPDR6_fops);
	if (DMA_RDTP_RPDR6 == NULL) {
		pr_info("error creating file: DMA_RDTP_RPDR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDTP_RPDR5 =
	    debugfs_create_file("DMA_RDTP_RPDR5", 744, dir, &DMA_RDTP_RPDR5_val,
				&DMA_RDTP_RPDR5_fops);
	if (DMA_RDTP_RPDR5 == NULL) {
		pr_info("error creating file: DMA_RDTP_RPDR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDTP_RPDR4 =
	    debugfs_create_file("DMA_RDTP_RPDR4", 744, dir, &DMA_RDTP_RPDR4_val,
				&DMA_RDTP_RPDR4_fops);
	if (DMA_RDTP_RPDR4 == NULL) {
		pr_info("error creating file: DMA_RDTP_RPDR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDTP_RPDR3 =
	    debugfs_create_file("DMA_RDTP_RPDR3", 744, dir, &DMA_RDTP_RPDR3_val,
				&DMA_RDTP_RPDR3_fops);
	if (DMA_RDTP_RPDR3 == NULL) {
		pr_info("error creating file: DMA_RDTP_RPDR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDTP_RPDR2 =
	    debugfs_create_file("DMA_RDTP_RPDR2", 744, dir, &DMA_RDTP_RPDR2_val,
				&DMA_RDTP_RPDR2_fops);
	if (DMA_RDTP_RPDR2 == NULL) {
		pr_info("error creating file: DMA_RDTP_RPDR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDTP_RPDR1 =
	    debugfs_create_file("DMA_RDTP_RPDR1", 744, dir, &DMA_RDTP_RPDR1_val,
				&DMA_RDTP_RPDR1_fops);
	if (DMA_RDTP_RPDR1 == NULL) {
		pr_info("error creating file: DMA_RDTP_RPDR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDTP_RPDR0 =
	    debugfs_create_file("DMA_RDTP_RPDR0", 744, dir, &DMA_RDTP_RPDR0_val,
				&DMA_RDTP_RPDR0_fops);
	if (DMA_RDTP_RPDR0 == NULL) {
		pr_info("error creating file: DMA_RDTP_RPDR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDTP_TPDR7 =
	    debugfs_create_file("DMA_TDTP_TPDR7", 744, dir, &DMA_TDTP_TPDR7_val,
				&DMA_TDTP_TPDR7_fops);
	if (DMA_TDTP_TPDR7 == NULL) {
		pr_info("error creating file: DMA_TDTP_TPDR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDTP_TPDR6 =
	    debugfs_create_file("DMA_TDTP_TPDR6", 744, dir, &DMA_TDTP_TPDR6_val,
				&DMA_TDTP_TPDR6_fops);
	if (DMA_TDTP_TPDR6 == NULL) {
		pr_info("error creating file: DMA_TDTP_TPDR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDTP_TPDR5 =
	    debugfs_create_file("DMA_TDTP_TPDR5", 744, dir, &DMA_TDTP_TPDR5_val,
				&DMA_TDTP_TPDR5_fops);
	if (DMA_TDTP_TPDR5 == NULL) {
		pr_info("error creating file: DMA_TDTP_TPDR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDTP_TPDR4 =
	    debugfs_create_file("DMA_TDTP_TPDR4", 744, dir, &DMA_TDTP_TPDR4_val,
				&DMA_TDTP_TPDR4_fops);
	if (DMA_TDTP_TPDR4 == NULL) {
		pr_info("error creating file: DMA_TDTP_TPDR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDTP_TPDR3 =
	    debugfs_create_file("DMA_TDTP_TPDR3", 744, dir, &DMA_TDTP_TPDR3_val,
				&DMA_TDTP_TPDR3_fops);
	if (DMA_TDTP_TPDR3 == NULL) {
		pr_info("error creating file: DMA_TDTP_TPDR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDTP_TPDR2 =
	    debugfs_create_file("DMA_TDTP_TPDR2", 744, dir, &DMA_TDTP_TPDR2_val,
				&DMA_TDTP_TPDR2_fops);
	if (DMA_TDTP_TPDR2 == NULL) {
		pr_info("error creating file: DMA_TDTP_TPDR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDTP_TPDR1 =
	    debugfs_create_file("DMA_TDTP_TPDR1", 744, dir, &DMA_TDTP_TPDR1_val,
				&DMA_TDTP_TPDR1_fops);
	if (DMA_TDTP_TPDR1 == NULL) {
		pr_info("error creating file: DMA_TDTP_TPDR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDTP_TPDR0 =
	    debugfs_create_file("DMA_TDTP_TPDR0", 744, dir, &DMA_TDTP_TPDR0_val,
				&DMA_TDTP_TPDR0_fops);
	if (DMA_TDTP_TPDR0 == NULL) {
		pr_info("error creating file: DMA_TDTP_TPDR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDLAR7 =
	    debugfs_create_file("DMA_RDLAR7", 744, dir, &DMA_RDLAR7_val,
				&DMA_RDLAR7_fops);
	if (DMA_RDLAR7 == NULL) {
		pr_info("error creating file: DMA_RDLAR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDLAR6 =
	    debugfs_create_file("DMA_RDLAR6", 744, dir, &DMA_RDLAR6_val,
				&DMA_RDLAR6_fops);
	if (DMA_RDLAR6 == NULL) {
		pr_info("error creating file: DMA_RDLAR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDLAR5 =
	    debugfs_create_file("DMA_RDLAR5", 744, dir, &DMA_RDLAR5_val,
				&DMA_RDLAR5_fops);
	if (DMA_RDLAR5 == NULL) {
		pr_info("error creating file: DMA_RDLAR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDLAR4 =
	    debugfs_create_file("DMA_RDLAR4", 744, dir, &DMA_RDLAR4_val,
				&DMA_RDLAR4_fops);
	if (DMA_RDLAR4 == NULL) {
		pr_info("error creating file: DMA_RDLAR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDLAR3 =
	    debugfs_create_file("DMA_RDLAR3", 744, dir, &DMA_RDLAR3_val,
				&DMA_RDLAR3_fops);
	if (DMA_RDLAR3 == NULL) {
		pr_info("error creating file: DMA_RDLAR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDLAR2 =
	    debugfs_create_file("DMA_RDLAR2", 744, dir, &DMA_RDLAR2_val,
				&DMA_RDLAR2_fops);
	if (DMA_RDLAR2 == NULL) {
		pr_info("error creating file: DMA_RDLAR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDLAR1 =
	    debugfs_create_file("DMA_RDLAR1", 744, dir, &DMA_RDLAR1_val,
				&DMA_RDLAR1_fops);
	if (DMA_RDLAR1 == NULL) {
		pr_info("error creating file: DMA_RDLAR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RDLAR0 =
	    debugfs_create_file("DMA_RDLAR0", 744, dir, &DMA_RDLAR0_val,
				&DMA_RDLAR0_fops);
	if (DMA_RDLAR0 == NULL) {
		pr_info("error creating file: DMA_RDLAR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDLAR7 =
	    debugfs_create_file("DMA_TDLAR7", 744, dir, &DMA_TDLAR7_val,
				&DMA_TDLAR7_fops);
	if (DMA_TDLAR7 == NULL) {
		pr_info("error creating file: DMA_TDLAR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDLAR6 =
	    debugfs_create_file("DMA_TDLAR6", 744, dir, &DMA_TDLAR6_val,
				&DMA_TDLAR6_fops);
	if (DMA_TDLAR6 == NULL) {
		pr_info("error creating file: DMA_TDLAR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDLAR5 =
	    debugfs_create_file("DMA_TDLAR5", 744, dir, &DMA_TDLAR5_val,
				&DMA_TDLAR5_fops);
	if (DMA_TDLAR5 == NULL) {
		pr_info("error creating file: DMA_TDLAR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDLAR4 =
	    debugfs_create_file("DMA_TDLAR4", 744, dir, &DMA_TDLAR4_val,
				&DMA_TDLAR4_fops);
	if (DMA_TDLAR4 == NULL) {
		pr_info("error creating file: DMA_TDLAR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDLAR3 =
	    debugfs_create_file("DMA_TDLAR3", 744, dir, &DMA_TDLAR3_val,
				&DMA_TDLAR3_fops);
	if (DMA_TDLAR3 == NULL) {
		pr_info("error creating file: DMA_TDLAR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDLAR2 =
	    debugfs_create_file("DMA_TDLAR2", 744, dir, &DMA_TDLAR2_val,
				&DMA_TDLAR2_fops);
	if (DMA_TDLAR2 == NULL) {
		pr_info("error creating file: DMA_TDLAR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDLAR1 =
	    debugfs_create_file("DMA_TDLAR1", 744, dir, &DMA_TDLAR1_val,
				&DMA_TDLAR1_fops);
	if (DMA_TDLAR1 == NULL) {
		pr_info("error creating file: DMA_TDLAR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TDLAR0 =
	    debugfs_create_file("DMA_TDLAR0", 744, dir, &DMA_TDLAR0_val,
				&DMA_TDLAR0_fops);
	if (DMA_TDLAR0 == NULL) {
		pr_info("error creating file: DMA_TDLAR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_IER7 =
	    debugfs_create_file("DMA_IER7", 744, dir, &DMA_IER7_val,
				&DMA_IER7_fops);
	if (DMA_IER7 == NULL) {
		pr_info("error creating file: DMA_IER7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_IER6 =
	    debugfs_create_file("DMA_IER6", 744, dir, &DMA_IER6_val,
				&DMA_IER6_fops);
	if (DMA_IER6 == NULL) {
		pr_info("error creating file: DMA_IER6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_IER5 =
	    debugfs_create_file("DMA_IER5", 744, dir, &DMA_IER5_val,
				&DMA_IER5_fops);
	if (DMA_IER5 == NULL) {
		pr_info("error creating file: DMA_IER5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_IER4 =
	    debugfs_create_file("DMA_IER4", 744, dir, &DMA_IER4_val,
				&DMA_IER4_fops);
	if (DMA_IER4 == NULL) {
		pr_info("error creating file: DMA_IER4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_IER3 =
	    debugfs_create_file("DMA_IER3", 744, dir, &DMA_IER3_val,
				&DMA_IER3_fops);
	if (DMA_IER3 == NULL) {
		pr_info("error creating file: DMA_IER3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_IER2 =
	    debugfs_create_file("DMA_IER2", 744, dir, &DMA_IER2_val,
				&DMA_IER2_fops);
	if (DMA_IER2 == NULL) {
		pr_info("error creating file: DMA_IER2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_IER1 =
	    debugfs_create_file("DMA_IER1", 744, dir, &DMA_IER1_val,
				&DMA_IER1_fops);
	if (DMA_IER1 == NULL) {
		pr_info("error creating file: DMA_IER1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_IER0 =
	    debugfs_create_file("DMA_IER0", 744, dir, &DMA_IER0_val,
				&DMA_IER0_fops);
	if (DMA_IER0 == NULL) {
		pr_info("error creating file: DMA_IER0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_IMR =
	    debugfs_create_file("MAC_IMR", 744, dir, &mac_imr_val,
				&mac_imr_fops);
	if (MAC_IMR == NULL) {
		pr_info("error creating file: MAC_IMR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_ISR =
	    debugfs_create_file("MAC_ISR", 744, dir, &mac_isr_val,
				&mac_isr_fops);
	if (MAC_ISR == NULL) {
		pr_info("error creating file: MAC_ISR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_ISR =
	    debugfs_create_file("MTL_ISR", 744, dir, &mtl_isr_val,
				&mtl_isr_fops);
	if (MTL_ISR == NULL) {
		pr_info("error creating file: MTL_ISR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SR7 =
	    debugfs_create_file("DMA_SR7", 744, dir, &DMA_SR7_val,
				&DMA_SR7_fops);
	if (DMA_SR7 == NULL) {
		pr_info("error creating file: DMA_SR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SR6 =
	    debugfs_create_file("DMA_SR6", 744, dir, &DMA_SR6_val,
				&DMA_SR6_fops);
	if (DMA_SR6 == NULL) {
		pr_info("error creating file: DMA_SR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SR5 =
	    debugfs_create_file("DMA_SR5", 744, dir, &DMA_SR5_val,
				&DMA_SR5_fops);
	if (DMA_SR5 == NULL) {
		pr_info("error creating file: DMA_SR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SR4 =
	    debugfs_create_file("DMA_SR4", 744, dir, &DMA_SR4_val,
				&DMA_SR4_fops);
	if (DMA_SR4 == NULL) {
		pr_info("error creating file: DMA_SR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SR3 =
	    debugfs_create_file("DMA_SR3", 744, dir, &DMA_SR3_val,
				&DMA_SR3_fops);
	if (DMA_SR3 == NULL) {
		pr_info("error creating file: DMA_SR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SR2 =
	    debugfs_create_file("DMA_SR2", 744, dir, &DMA_SR2_val,
				&DMA_SR2_fops);
	if (DMA_SR2 == NULL) {
		pr_info("error creating file: DMA_SR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SR1 =
	    debugfs_create_file("DMA_SR1", 744, dir, &DMA_SR1_val,
				&DMA_SR1_fops);
	if (DMA_SR1 == NULL) {
		pr_info("error creating file: DMA_SR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SR0 =
	    debugfs_create_file("DMA_SR0", 744, dir, &DMA_SR0_val,
				&DMA_SR0_fops);
	if (DMA_SR0 == NULL) {
		pr_info("error creating file: DMA_SR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_ISR =
	    debugfs_create_file("DMA_ISR", 744, dir, &dma_isr_val,
				&dma_isr_fops);
	if (DMA_ISR == NULL) {
		pr_info("error creating file: DMA_ISR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_DSR2 =
	    debugfs_create_file("DMA_DSR2", 744, dir, &DMA_DSR2_val,
				&DMA_DSR2_fops);
	if (DMA_DSR2 == NULL) {
		pr_info("error creating file: DMA_DSR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_DSR1 =
	    debugfs_create_file("DMA_DSR1", 744, dir, &DMA_DSR1_val,
				&DMA_DSR1_fops);
	if (DMA_DSR1 == NULL) {
		pr_info("error creating file: DMA_DSR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_DSR0 =
	    debugfs_create_file("DMA_DSR0", 744, dir, &DMA_DSR0_val,
				&DMA_DSR0_fops);
	if (DMA_DSR0 == NULL) {
		pr_info("error creating file: DMA_DSR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0RDR =
	    debugfs_create_file("MTL_Q0RDR", 744, dir, &MTL_Q0rdr_val,
				&MTL_Q0rdr_fops);
	if (MTL_Q0RDR == NULL) {
		pr_info("error creating file: MTL_Q0RDR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0ESR =
	    debugfs_create_file("MTL_Q0ESR", 744, dir, &MTL_Q0esr_val,
				&MTL_Q0esr_fops);
	if (MTL_Q0ESR == NULL) {
		pr_info("error creating file: MTL_Q0ESR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0TDR =
	    debugfs_create_file("MTL_Q0TDR", 744, dir, &MTL_Q0tdr_val,
				&MTL_Q0tdr_fops);
	if (MTL_Q0TDR == NULL) {
		pr_info("error creating file: MTL_Q0TDR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRBAR7 =
	    debugfs_create_file("DMA_CHRBAR7", 744, dir, &DMA_CHRBAR7_val,
				&DMA_CHRBAR7_fops);
	if (DMA_CHRBAR7 == NULL) {
		pr_info("error creating file: DMA_CHRBAR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRBAR6 =
	    debugfs_create_file("DMA_CHRBAR6", 744, dir, &DMA_CHRBAR6_val,
				&DMA_CHRBAR6_fops);
	if (DMA_CHRBAR6 == NULL) {
		pr_info("error creating file: DMA_CHRBAR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRBAR5 =
	    debugfs_create_file("DMA_CHRBAR5", 744, dir, &DMA_CHRBAR5_val,
				&DMA_CHRBAR5_fops);
	if (DMA_CHRBAR5 == NULL) {
		pr_info("error creating file: DMA_CHRBAR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRBAR4 =
	    debugfs_create_file("DMA_CHRBAR4", 744, dir, &DMA_CHRBAR4_val,
				&DMA_CHRBAR4_fops);
	if (DMA_CHRBAR4 == NULL) {
		pr_info("error creating file: DMA_CHRBAR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRBAR3 =
	    debugfs_create_file("DMA_CHRBAR3", 744, dir, &DMA_CHRBAR3_val,
				&DMA_CHRBAR3_fops);
	if (DMA_CHRBAR3 == NULL) {
		pr_info("error creating file: DMA_CHRBAR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRBAR2 =
	    debugfs_create_file("DMA_CHRBAR2", 744, dir, &DMA_CHRBAR2_val,
				&DMA_CHRBAR2_fops);
	if (DMA_CHRBAR2 == NULL) {
		pr_info("error creating file: DMA_CHRBAR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRBAR1 =
	    debugfs_create_file("DMA_CHRBAR1", 744, dir, &DMA_CHRBAR1_val,
				&DMA_CHRBAR1_fops);
	if (DMA_CHRBAR1 == NULL) {
		pr_info("error creating file: DMA_CHRBAR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRBAR0 =
	    debugfs_create_file("DMA_CHRBAR0", 744, dir, &DMA_CHRBAR0_val,
				&DMA_CHRBAR0_fops);
	if (DMA_CHRBAR0 == NULL) {
		pr_info("error creating file: DMA_CHRBAR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTBAR7 =
	    debugfs_create_file("DMA_CHTBAR7", 744, dir, &DMA_CHTBAR7_val,
				&DMA_CHTBAR7_fops);
	if (DMA_CHTBAR7 == NULL) {
		pr_info("error creating file: DMA_CHTBAR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTBAR6 =
	    debugfs_create_file("DMA_CHTBAR6", 744, dir, &DMA_CHTBAR6_val,
				&DMA_CHTBAR6_fops);
	if (DMA_CHTBAR6 == NULL) {
		pr_info("error creating file: DMA_CHTBAR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTBAR5 =
	    debugfs_create_file("DMA_CHTBAR5", 744, dir, &DMA_CHTBAR5_val,
				&DMA_CHTBAR5_fops);
	if (DMA_CHTBAR5 == NULL) {
		pr_info("error creating file: DMA_CHTBAR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTBAR4 =
	    debugfs_create_file("DMA_CHTBAR4", 744, dir, &DMA_CHTBAR4_val,
				&DMA_CHTBAR4_fops);
	if (DMA_CHTBAR4 == NULL) {
		pr_info("error creating file: DMA_CHTBAR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTBAR3 =
	    debugfs_create_file("DMA_CHTBAR3", 744, dir, &DMA_CHTBAR3_val,
				&DMA_CHTBAR3_fops);
	if (DMA_CHTBAR3 == NULL) {
		pr_info("error creating file: DMA_CHTBAR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTBAR2 =
	    debugfs_create_file("DMA_CHTBAR2", 744, dir, &DMA_CHTBAR2_val,
				&DMA_CHTBAR2_fops);
	if (DMA_CHTBAR2 == NULL) {
		pr_info("error creating file: DMA_CHTBAR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTBAR1 =
	    debugfs_create_file("DMA_CHTBAR1", 744, dir, &DMA_CHTBAR1_val,
				&DMA_CHTBAR1_fops);
	if (DMA_CHTBAR1 == NULL) {
		pr_info("error creating file: DMA_CHTBAR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTBAR0 =
	    debugfs_create_file("DMA_CHTBAR0", 744, dir, &DMA_CHTBAR0_val,
				&DMA_CHTBAR0_fops);
	if (DMA_CHTBAR0 == NULL) {
		pr_info("error creating file: DMA_CHTBAR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRDR7 =
	    debugfs_create_file("DMA_CHRDR7", 744, dir, &DMA_CHRDR7_val,
				&DMA_CHRDR7_fops);
	if (DMA_CHRDR7 == NULL) {
		pr_info("error creating file: DMA_CHRDR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRDR6 =
	    debugfs_create_file("DMA_CHRDR6", 744, dir, &DMA_CHRDR6_val,
				&DMA_CHRDR6_fops);
	if (DMA_CHRDR6 == NULL) {
		pr_info("error creating file: DMA_CHRDR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRDR5 =
	    debugfs_create_file("DMA_CHRDR5", 744, dir, &DMA_CHRDR5_val,
				&DMA_CHRDR5_fops);
	if (DMA_CHRDR5 == NULL) {
		pr_info("error creating file: DMA_CHRDR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRDR4 =
	    debugfs_create_file("DMA_CHRDR4", 744, dir, &DMA_CHRDR4_val,
				&DMA_CHRDR4_fops);
	if (DMA_CHRDR4 == NULL) {
		pr_info("error creating file: DMA_CHRDR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRDR3 =
	    debugfs_create_file("DMA_CHRDR3", 744, dir, &DMA_CHRDR3_val,
				&DMA_CHRDR3_fops);
	if (DMA_CHRDR3 == NULL) {
		pr_info("error creating file: DMA_CHRDR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRDR2 =
	    debugfs_create_file("DMA_CHRDR2", 744, dir, &DMA_CHRDR2_val,
				&DMA_CHRDR2_fops);
	if (DMA_CHRDR2 == NULL) {
		pr_info("error creating file: DMA_CHRDR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRDR1 =
	    debugfs_create_file("DMA_CHRDR1", 744, dir, &DMA_CHRDR1_val,
				&DMA_CHRDR1_fops);
	if (DMA_CHRDR1 == NULL) {
		pr_info("error creating file: DMA_CHRDR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHRDR0 =
	    debugfs_create_file("DMA_CHRDR0", 744, dir, &DMA_CHRDR0_val,
				&DMA_CHRDR0_fops);
	if (DMA_CHRDR0 == NULL) {
		pr_info("error creating file: DMA_CHRDR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTDR7 =
	    debugfs_create_file("DMA_CHTDR7", 744, dir, &DMA_CHTDR7_val,
				&DMA_CHTDR7_fops);
	if (DMA_CHTDR7 == NULL) {
		pr_info("error creating file: DMA_CHTDR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTDR6 =
	    debugfs_create_file("DMA_CHTDR6", 744, dir, &DMA_CHTDR6_val,
				&DMA_CHTDR6_fops);
	if (DMA_CHTDR6 == NULL) {
		pr_info("error creating file: DMA_CHTDR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTDR5 =
	    debugfs_create_file("DMA_CHTDR5", 744, dir, &DMA_CHTDR5_val,
				&DMA_CHTDR5_fops);
	if (DMA_CHTDR5 == NULL) {
		pr_info("error creating file: DMA_CHTDR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTDR4 =
	    debugfs_create_file("DMA_CHTDR4", 744, dir, &DMA_CHTDR4_val,
				&DMA_CHTDR4_fops);
	if (DMA_CHTDR4 == NULL) {
		pr_info("error creating file: DMA_CHTDR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTDR3 =
	    debugfs_create_file("DMA_CHTDR3", 744, dir, &DMA_CHTDR3_val,
				&DMA_CHTDR3_fops);
	if (DMA_CHTDR3 == NULL) {
		pr_info("error creating file: DMA_CHTDR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTDR2 =
	    debugfs_create_file("DMA_CHTDR2", 744, dir, &DMA_CHTDR2_val,
				&DMA_CHTDR2_fops);
	if (DMA_CHTDR2 == NULL) {
		pr_info("error creating file: DMA_CHTDR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTDR1 =
	    debugfs_create_file("DMA_CHTDR1", 744, dir, &DMA_CHTDR1_val,
				&DMA_CHTDR1_fops);
	if (DMA_CHTDR1 == NULL) {
		pr_info("error creating file: DMA_CHTDR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CHTDR0 =
	    debugfs_create_file("DMA_CHTDR0", 744, dir, &DMA_CHTDR0_val,
				&DMA_CHTDR0_fops);
	if (DMA_CHTDR0 == NULL) {
		pr_info("error creating file: DMA_CHTDR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SFCSR7 =
	    debugfs_create_file("DMA_SFCSR7", 744, dir, &DMA_SFCSR7_val,
				&DMA_SFCSR7_fops);
	if (DMA_SFCSR7 == NULL) {
		pr_info("error creating file: DMA_SFCSR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SFCSR6 =
	    debugfs_create_file("DMA_SFCSR6", 744, dir, &DMA_SFCSR6_val,
				&DMA_SFCSR6_fops);
	if (DMA_SFCSR6 == NULL) {
		pr_info("error creating file: DMA_SFCSR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SFCSR5 =
	    debugfs_create_file("DMA_SFCSR5", 744, dir, &DMA_SFCSR5_val,
				&DMA_SFCSR5_fops);
	if (DMA_SFCSR5 == NULL) {
		pr_info("error creating file: DMA_SFCSR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SFCSR4 =
	    debugfs_create_file("DMA_SFCSR4", 744, dir, &DMA_SFCSR4_val,
				&DMA_SFCSR4_fops);
	if (DMA_SFCSR4 == NULL) {
		pr_info("error creating file: DMA_SFCSR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SFCSR3 =
	    debugfs_create_file("DMA_SFCSR3", 744, dir, &DMA_SFCSR3_val,
				&DMA_SFCSR3_fops);
	if (DMA_SFCSR3 == NULL) {
		pr_info("error creating file: DMA_SFCSR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SFCSR2 =
	    debugfs_create_file("DMA_SFCSR2", 744, dir, &DMA_SFCSR2_val,
				&DMA_SFCSR2_fops);
	if (DMA_SFCSR2 == NULL) {
		pr_info("error creating file: DMA_SFCSR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SFCSR1 =
	    debugfs_create_file("DMA_SFCSR1", 744, dir, &DMA_SFCSR1_val,
				&DMA_SFCSR1_fops);
	if (DMA_SFCSR1 == NULL) {
		pr_info("error creating file: DMA_SFCSR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SFCSR0 =
	    debugfs_create_file("DMA_SFCSR0", 744, dir, &DMA_SFCSR0_val,
				&DMA_SFCSR0_fops);
	if (DMA_SFCSR0 == NULL) {
		pr_info("error creating file: DMA_SFCSR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_IVLANTIRR =
	    debugfs_create_file("MAC_IVLANTIRR", 744, dir, &mac_ivlantirr_val,
				&mac_ivlantirr_fops);
	if (MAC_IVLANTIRR == NULL) {
		pr_info("error creating file: MAC_IVLANTIRR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_VLANTIRR =
	    debugfs_create_file("MAC_VLANTIRR", 744, dir, &mac_vlantirr_val,
				&mac_vlantirr_fops);
	if (MAC_VLANTIRR == NULL) {
		pr_info("error creating file: MAC_VLANTIRR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_VLANHTR =
	    debugfs_create_file("MAC_VLANHTR", 744, dir, &mac_vlanhtr_val,
				&mac_vlanhtr_fops);
	if (MAC_VLANHTR == NULL) {
		pr_info("error creating file: MAC_VLANHTR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_VLANTR =
	    debugfs_create_file("MAC_VLANTR", 744, dir, &mac_vlantr_val,
				&mac_vlantr_fops);
	if (MAC_VLANTR == NULL) {
		pr_info("error creating file: MAC_VLANTR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_SBUS =
	    debugfs_create_file("DMA_SBUS", 744, dir, &dma_sbus_val,
				&dma_sbus_fops);
	if (DMA_SBUS == NULL) {
		pr_info("error creating file: DMA_SBUS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_BMR =
	    debugfs_create_file("DMA_BMR", 744, dir, &dma_bmr_val,
				&dma_bmr_fops);
	if (DMA_BMR == NULL) {
		pr_info("error creating file: DMA_BMR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0RCR =
	    debugfs_create_file("MTL_Q0RCR", 744, dir, &MTL_Q0rcr_val,
				&MTL_Q0rcr_fops);
	if (MTL_Q0RCR == NULL) {
		pr_info("error creating file: MTL_Q0RCR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0OCR =
	    debugfs_create_file("MTL_Q0OCR", 744, dir, &MTL_Q0ocr_val,
				&MTL_Q0ocr_fops);
	if (MTL_Q0OCR == NULL) {
		pr_info("error creating file: MTL_Q0OCR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0ROMR =
	    debugfs_create_file("MTL_Q0ROMR", 744, dir, &MTL_Q0romr_val,
				&MTL_Q0romr_fops);
	if (MTL_Q0ROMR == NULL) {
		pr_info("error creating file: MTL_Q0ROMR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0QR =
	    debugfs_create_file("MTL_Q0QR", 744, dir, &MTL_Q0qr_val,
				&MTL_Q0qr_fops);
	if (MTL_Q0QR == NULL) {
		pr_info("error creating file: MTL_Q0QR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0ECR =
	    debugfs_create_file("MTL_Q0ECR", 744, dir, &MTL_Q0ecr_val,
				&MTL_Q0ecr_fops);
	if (MTL_Q0ECR == NULL) {
		pr_info("error creating file: MTL_Q0ECR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0UCR =
	    debugfs_create_file("MTL_Q0UCR", 744, dir, &MTL_Q0ucr_val,
				&MTL_Q0ucr_fops);
	if (MTL_Q0UCR == NULL) {
		pr_info("error creating file: MTL_Q0UCR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_Q0TOMR =
	    debugfs_create_file("MTL_Q0TOMR", 744, dir, &MTL_Q0tomr_val,
				&MTL_Q0tomr_fops);
	if (MTL_Q0TOMR == NULL) {
		pr_info("error creating file: MTL_Q0TOMR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_RQDCM1R =
	    debugfs_create_file("MTL_RQDCM1R", 744, dir, &MTL_RQDCM1r_val,
				&MTL_RQDCM1r_fops);
	if (MTL_RQDCM1R == NULL) {
		pr_info("error creating file: MTL_RQDCM1R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_RQDCM0R =
	    debugfs_create_file("MTL_RQDCM0R", 744, dir, &MTL_RQDCM0r_val,
				&MTL_RQDCM0r_fops);
	if (MTL_RQDCM0R == NULL) {
		pr_info("error creating file: MTL_RQDCM0R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_FDDR =
	    debugfs_create_file("MTL_FDDR", 744, dir, &mtl_fddr_val,
				&mtl_fddr_fops);
	if (MTL_FDDR == NULL) {
		pr_info("error creating file: MTL_FDDR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_FDACS =
	    debugfs_create_file("MTL_FDACS", 744, dir, &mtl_fdacs_val,
				&mtl_fdacs_fops);
	if (MTL_FDACS == NULL) {
		pr_info("error creating file: MTL_FDACS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MTL_OMR =
	    debugfs_create_file("MTL_OMR", 744, dir, &mtl_omr_val,
				&mtl_omr_fops);
	if (MTL_OMR == NULL) {
		pr_info("error creating file: MTL_OMR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_RQC3R =
	    debugfs_create_file("MAC_RQC3R", 744, dir, &MAC_RQC3r_val,
				&MAC_RQC3r_fops);
	if (MAC_RQC3R == NULL) {
		pr_info("error creating file: MAC_RQC3R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_RQC2R =
	    debugfs_create_file("MAC_RQC2R", 744, dir, &MAC_RQC2r_val,
				&MAC_RQC2r_fops);
	if (MAC_RQC2R == NULL) {
		pr_info("error creating file: MAC_RQC2R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_RQC1R =
	    debugfs_create_file("MAC_RQC1R", 744, dir, &MAC_RQC1r_val,
				&MAC_RQC1r_fops);
	if (MAC_RQC1R == NULL) {
		pr_info("error creating file: MAC_RQC1R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_RQC0R =
	    debugfs_create_file("MAC_RQC0R", 744, dir, &MAC_RQC0r_val,
				&MAC_RQC0r_fops);
	if (MAC_RQC0R == NULL) {
		pr_info("error creating file: MAC_RQC0R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TQPM1R =
	    debugfs_create_file("MAC_TQPM1R", 744, dir, &MAC_TQPM1r_val,
				&MAC_TQPM1r_fops);
	if (MAC_TQPM1R == NULL) {
		pr_info("error creating file: MAC_TQPM1R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_TQPM0R =
	    debugfs_create_file("MAC_TQPM0R", 744, dir, &MAC_TQPM0r_val,
				&MAC_TQPM0r_fops);
	if (MAC_TQPM0R == NULL) {
		pr_info("error creating file: MAC_TQPM0R\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_RFCR =
	    debugfs_create_file("MAC_RFCR", 744, dir, &mac_rfcr_val,
				&mac_rfcr_fops);
	if (MAC_RFCR == NULL) {
		pr_info("error creating file: MAC_RFCR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_QTFCR7 =
	    debugfs_create_file("MAC_QTFCR7", 744, dir, &MAC_QTFCR7_val,
				&MAC_QTFCR7_fops);
	if (MAC_QTFCR7 == NULL) {
		pr_info("error creating file: MAC_QTFCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_QTFCR6 =
	    debugfs_create_file("MAC_QTFCR6", 744, dir, &MAC_QTFCR6_val,
				&MAC_QTFCR6_fops);
	if (MAC_QTFCR6 == NULL) {
		pr_info("error creating file: MAC_QTFCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_QTFCR5 =
	    debugfs_create_file("MAC_QTFCR5", 744, dir, &MAC_QTFCR5_val,
				&MAC_QTFCR5_fops);
	if (MAC_QTFCR5 == NULL) {
		pr_info("error creating file: MAC_QTFCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_QTFCR4 =
	    debugfs_create_file("MAC_QTFCR4", 744, dir, &MAC_QTFCR4_val,
				&MAC_QTFCR4_fops);
	if (MAC_QTFCR4 == NULL) {
		pr_info("error creating file: MAC_QTFCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_QTFCR3 =
	    debugfs_create_file("MAC_QTFCR3", 744, dir, &MAC_QTFCR3_val,
				&MAC_QTFCR3_fops);
	if (MAC_QTFCR3 == NULL) {
		pr_info("error creating file: MAC_QTFCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_QTFCR2 =
	    debugfs_create_file("MAC_QTFCR2", 744, dir, &MAC_QTFCR2_val,
				&MAC_QTFCR2_fops);
	if (MAC_QTFCR2 == NULL) {
		pr_info("error creating file: MAC_QTFCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_QTFCR1 =
	    debugfs_create_file("MAC_QTFCR1", 744, dir, &MAC_QTFCR1_val,
				&MAC_QTFCR1_fops);
	if (MAC_QTFCR1 == NULL) {
		pr_info("error creating file: MAC_QTFCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_Q0TFCR =
	    debugfs_create_file("MAC_Q0TFCR", 744, dir, &MAC_Q0tfcr_val,
				&MAC_Q0tfcr_fops);
	if (MAC_Q0TFCR == NULL) {
		pr_info("error creating file: MAC_Q0TFCR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_AXI4CR7 =
	    debugfs_create_file("DMA_AXI4CR7", 744, dir, &DMA_AXI4CR7_val,
				&DMA_AXI4CR7_fops);
	if (DMA_AXI4CR7 == NULL) {
		pr_info("error creating file: DMA_AXI4CR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_AXI4CR6 =
	    debugfs_create_file("DMA_AXI4CR6", 744, dir, &DMA_AXI4CR6_val,
				&DMA_AXI4CR6_fops);
	if (DMA_AXI4CR6 == NULL) {
		pr_info("error creating file: DMA_AXI4CR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_AXI4CR5 =
	    debugfs_create_file("DMA_AXI4CR5", 744, dir, &DMA_AXI4CR5_val,
				&DMA_AXI4CR5_fops);
	if (DMA_AXI4CR5 == NULL) {
		pr_info("error creating file: DMA_AXI4CR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_AXI4CR4 =
	    debugfs_create_file("DMA_AXI4CR4", 744, dir, &DMA_AXI4CR4_val,
				&DMA_AXI4CR4_fops);
	if (DMA_AXI4CR4 == NULL) {
		pr_info("error creating file: DMA_AXI4CR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_AXI4CR3 =
	    debugfs_create_file("DMA_AXI4CR3", 744, dir, &DMA_AXI4CR3_val,
				&DMA_AXI4CR3_fops);
	if (DMA_AXI4CR3 == NULL) {
		pr_info("error creating file: DMA_AXI4CR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_AXI4CR2 =
	    debugfs_create_file("DMA_AXI4CR2", 744, dir, &DMA_AXI4CR2_val,
				&DMA_AXI4CR2_fops);
	if (DMA_AXI4CR2 == NULL) {
		pr_info("error creating file: DMA_AXI4CR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_AXI4CR1 =
	    debugfs_create_file("DMA_AXI4CR1", 744, dir, &DMA_AXI4CR1_val,
				&DMA_AXI4CR1_fops);
	if (DMA_AXI4CR1 == NULL) {
		pr_info("error creating file: DMA_AXI4CR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_AXI4CR0 =
	    debugfs_create_file("DMA_AXI4CR0", 744, dir, &DMA_AXI4CR0_val,
				&DMA_AXI4CR0_fops);
	if (DMA_AXI4CR0 == NULL) {
		pr_info("error creating file: DMA_AXI4CR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RCR7 =
	    debugfs_create_file("DMA_RCR7", 744, dir, &DMA_RCR7_val,
				&DMA_RCR7_fops);
	if (DMA_RCR7 == NULL) {
		pr_info("error creating file: DMA_RCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RCR6 =
	    debugfs_create_file("DMA_RCR6", 744, dir, &DMA_RCR6_val,
				&DMA_RCR6_fops);
	if (DMA_RCR6 == NULL) {
		pr_info("error creating file: DMA_RCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RCR5 =
	    debugfs_create_file("DMA_RCR5", 744, dir, &DMA_RCR5_val,
				&DMA_RCR5_fops);
	if (DMA_RCR5 == NULL) {
		pr_info("error creating file: DMA_RCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RCR4 =
	    debugfs_create_file("DMA_RCR4", 744, dir, &DMA_RCR4_val,
				&DMA_RCR4_fops);
	if (DMA_RCR4 == NULL) {
		pr_info("error creating file: DMA_RCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RCR3 =
	    debugfs_create_file("DMA_RCR3", 744, dir, &DMA_RCR3_val,
				&DMA_RCR3_fops);
	if (DMA_RCR3 == NULL) {
		pr_info("error creating file: DMA_RCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RCR2 =
	    debugfs_create_file("DMA_RCR2", 744, dir, &DMA_RCR2_val,
				&DMA_RCR2_fops);
	if (DMA_RCR2 == NULL) {
		pr_info("error creating file: DMA_RCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RCR1 =
	    debugfs_create_file("DMA_RCR1", 744, dir, &DMA_RCR1_val,
				&DMA_RCR1_fops);
	if (DMA_RCR1 == NULL) {
		pr_info("error creating file: DMA_RCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_RCR0 =
	    debugfs_create_file("DMA_RCR0", 744, dir, &DMA_RCR0_val,
				&DMA_RCR0_fops);
	if (DMA_RCR0 == NULL) {
		pr_info("error creating file: DMA_RCR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TCR7 =
	    debugfs_create_file("DMA_TCR7", 744, dir, &DMA_TCR7_val,
				&DMA_TCR7_fops);
	if (DMA_TCR7 == NULL) {
		pr_info("error creating file: DMA_TCR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TCR6 =
	    debugfs_create_file("DMA_TCR6", 744, dir, &DMA_TCR6_val,
				&DMA_TCR6_fops);
	if (DMA_TCR6 == NULL) {
		pr_info("error creating file: DMA_TCR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TCR5 =
	    debugfs_create_file("DMA_TCR5", 744, dir, &DMA_TCR5_val,
				&DMA_TCR5_fops);
	if (DMA_TCR5 == NULL) {
		pr_info("error creating file: DMA_TCR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TCR4 =
	    debugfs_create_file("DMA_TCR4", 744, dir, &DMA_TCR4_val,
				&DMA_TCR4_fops);
	if (DMA_TCR4 == NULL) {
		pr_info("error creating file: DMA_TCR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TCR3 =
	    debugfs_create_file("DMA_TCR3", 744, dir, &DMA_TCR3_val,
				&DMA_TCR3_fops);
	if (DMA_TCR3 == NULL) {
		pr_info("error creating file: DMA_TCR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TCR2 =
	    debugfs_create_file("DMA_TCR2", 744, dir, &DMA_TCR2_val,
				&DMA_TCR2_fops);
	if (DMA_TCR2 == NULL) {
		pr_info("error creating file: DMA_TCR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TCR1 =
	    debugfs_create_file("DMA_TCR1", 744, dir, &DMA_TCR1_val,
				&DMA_TCR1_fops);
	if (DMA_TCR1 == NULL) {
		pr_info("error creating file: DMA_TCR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_TCR0 =
	    debugfs_create_file("DMA_TCR0", 744, dir, &DMA_TCR0_val,
				&DMA_TCR0_fops);
	if (DMA_TCR0 == NULL) {
		pr_info("error creating file: DMA_TCR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CR7 =
	    debugfs_create_file("DMA_CR7", 744, dir, &DMA_CR7_val,
				&DMA_CR7_fops);
	if (DMA_CR7 == NULL) {
		pr_info("error creating file: DMA_CR7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CR6 =
	    debugfs_create_file("DMA_CR6", 744, dir, &DMA_CR6_val,
				&DMA_CR6_fops);
	if (DMA_CR6 == NULL) {
		pr_info("error creating file: DMA_CR6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CR5 =
	    debugfs_create_file("DMA_CR5", 744, dir, &DMA_CR5_val,
				&DMA_CR5_fops);
	if (DMA_CR5 == NULL) {
		pr_info("error creating file: DMA_CR5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CR4 =
	    debugfs_create_file("DMA_CR4", 744, dir, &DMA_CR4_val,
				&DMA_CR4_fops);
	if (DMA_CR4 == NULL) {
		pr_info("error creating file: DMA_CR4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CR3 =
	    debugfs_create_file("DMA_CR3", 744, dir, &DMA_CR3_val,
				&DMA_CR3_fops);
	if (DMA_CR3 == NULL) {
		pr_info("error creating file: DMA_CR3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CR2 =
	    debugfs_create_file("DMA_CR2", 744, dir, &DMA_CR2_val,
				&DMA_CR2_fops);
	if (DMA_CR2 == NULL) {
		pr_info("error creating file: DMA_CR2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CR1 =
	    debugfs_create_file("DMA_CR1", 744, dir, &DMA_CR1_val,
				&DMA_CR1_fops);
	if (DMA_CR1 == NULL) {
		pr_info("error creating file: DMA_CR1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	DMA_CR0 =
	    debugfs_create_file("DMA_CR0", 744, dir, &DMA_CR0_val,
				&DMA_CR0_fops);
	if (DMA_CR0 == NULL) {
		pr_info("error creating file: DMA_CR0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_WTR =
	    debugfs_create_file("MAC_WTR", 744, dir, &mac_wtr_val,
				&mac_wtr_fops);
	if (MAC_WTR == NULL) {
		pr_info("error creating file: MAC_WTR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MPFR =
	    debugfs_create_file("MAC_MPFR", 744, dir, &mac_mpfr_val,
				&mac_mpfr_fops);
	if (MAC_MPFR == NULL) {
		pr_info("error creating file: MAC_MPFR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MECR =
	    debugfs_create_file("MAC_MECR", 744, dir, &mac_mecr_val,
				&mac_mecr_fops);
	if (MAC_MECR == NULL) {
		pr_info("error creating file: MAC_MECR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MAC_MCR =
	    debugfs_create_file("MAC_MCR", 744, dir, &mac_mcr_val,
				&mac_mcr_fops);
	if (MAC_MCR == NULL) {
		pr_info("error creating file: MAC_MCR\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}
	/* MII/GMII registers */
	MII_BMCR_REG =
	    debugfs_create_file("MII_BMCR_REG", 744, dir, &mii_bmcr_reg_val,
				&mii_bmcr_reg_fops);
	if (MII_BMCR_REG == NULL) {
		pr_info("error creating file: MII_BMCR_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_BMSR_REG =
	    debugfs_create_file("MII_BMSR_REG", 744, dir, &mii_bmsr_reg_val,
				&mii_bmsr_reg_fops);
	if (MII_BMSR_REG == NULL) {
		pr_info("error creating file: MII_BMSR_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_PHYSID1_REG =
	    debugfs_create_file("MII_PHYSID1_REG", 744, dir,
				&MII_PHYSID1_reg_val, &MII_PHYSID1_reg_fops);
	if (MII_PHYSID1_REG == NULL) {
		pr_info("error creating file: MII_PHYSID1_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_PHYSID2_REG =
	    debugfs_create_file("MII_PHYSID2_REG", 744, dir,
				&MII_PHYSID2_reg_val, &MII_PHYSID2_reg_fops);
	if (MII_PHYSID2_REG == NULL) {
		pr_info("error creating file: MII_PHYSID2_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_ADVERTISE_REG =
	    debugfs_create_file("MII_ADVERTISE_REG", 744, dir,
				&mii_advertise_reg_val,
				&mii_advertise_reg_fops);
	if (MII_ADVERTISE_REG == NULL) {
		pr_info("error creating file: MII_ADVERTISE_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_LPA_REG =
	    debugfs_create_file("MII_LPA_REG", 744, dir, &mii_lpa_reg_val,
				&mii_lpa_reg_fops);
	if (MII_LPA_REG == NULL) {
		pr_info("error creating file: MII_LPA_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_EXPANSION_REG =
	    debugfs_create_file("MII_EXPANSION_REG", 744, dir,
				&mii_expansion_reg_val,
				&mii_expansion_reg_fops);
	if (MII_EXPANSION_REG == NULL) {
		pr_info("error creating file: MII_EXPANSION_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	AUTO_NEGO_NP_REG =
	    debugfs_create_file("AUTO_NEGO_NP_REG", 744, dir,
				&auto_nego_np_reg_val, &auto_nego_np_reg_fops);
	if (AUTO_NEGO_NP_REG == NULL) {
		pr_info("error creating file: AUTO_NEGO_NP_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_ESTATUS_REG =
	    debugfs_create_file("MII_ESTATUS_REG", 744, dir,
				&mii_estatus_reg_val, &mii_estatus_reg_fops);
	if (MII_ESTATUS_REG == NULL) {
		pr_info("error creating file: MII_ESTATUS_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_CTRL1000_REG =
	    debugfs_create_file("MII_CTRL1000_REG", 744, dir,
				&MII_CTRL1000_reg_val, &MII_CTRL1000_reg_fops);
	if (MII_CTRL1000_REG == NULL) {
		pr_info("error creating file: MII_CTRL1000_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	MII_STAT1000_REG =
	    debugfs_create_file("MII_STAT1000_REG", 744, dir,
				&MII_STAT1000_reg_val, &MII_STAT1000_reg_fops);
	if (MII_STAT1000_REG == NULL) {
		pr_info("error creating file: MII_STAT1000_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	PHY_CTL_REG =
	    debugfs_create_file("PHY_CTL_REG", 744, dir, &phy_ctl_reg_val,
				&phy_ctl_reg_fops);
	if (PHY_CTL_REG == NULL) {
		pr_info("error creating file: PHY_CTL_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	PHY_STS_REG =
	    debugfs_create_file("PHY_STS_REG", 744, dir, &phy_sts_reg_val,
				&phy_sts_reg_fops);
	if (PHY_STS_REG == NULL) {
		pr_info("error creating file: PHY_STS_REG\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	feature_drop_tx_pktburstcnt =
	    debugfs_create_file("feature_drop_tx_pktburstcnt", 744, dir,
				&feature_drop_tx_pktburstcnt_val,
				&feature_drop_tx_pktburstcnt_fops);
	if (feature_drop_tx_pktburstcnt == NULL) {
		pr_info(
		       "error creating file: feature_drop_tx_pktburstcnt\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	qinx = debugfs_create_file("qinx", 744, dir, &qinx_val, &qinx_fops);
	if (qinx == NULL) {
		pr_info("error creating file: qinx\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	reg_offset = debugfs_create_file("reg_offset", 744, dir,
					 &reg_offset_val, &reg_offset_fops);
	if (reg_offset == NULL) {
		pr_info("error creating file: reg_offset\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	gen_reg =
	    debugfs_create_file("gen_reg", 744, dir, &gen_reg_val,
				&gen_reg_fops);
	if (gen_reg == NULL) {
		pr_info("error creating file: gen_reg\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	do_tx_align_tst = debugfs_create_file("do_tx_align_tst", 744, dir,
		&do_tx_align_tst_val, &do_tx_align_tst_fops);
	if (do_tx_align_tst == NULL) {
		pr_info("error creating file: do_tx_align_tst\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc0 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor0", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops0);
	if (RX_NORMAL_DESC_desc0 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc1 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor1", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops1);
	if (RX_NORMAL_DESC_desc1 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc2 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor2", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops2);
	if (RX_NORMAL_DESC_desc2 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc3 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor3", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops3);
	if (RX_NORMAL_DESC_desc3 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc4 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor4", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops4);
	if (RX_NORMAL_DESC_desc4 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc5 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor5", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops5);
	if (RX_NORMAL_DESC_desc5 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc6 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor6", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops6);
	if (RX_NORMAL_DESC_desc6 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc7 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor7", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops7);
	if (RX_NORMAL_DESC_desc7 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc8 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor8", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops8);
	if (RX_NORMAL_DESC_desc8 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor8\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc9 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor9", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops9);
	if (RX_NORMAL_DESC_desc9 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor9\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc10 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor10", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops10);
	if (RX_NORMAL_DESC_desc10 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor10\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc11 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor11", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops11);
	if (RX_NORMAL_DESC_desc11 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor11\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_desc12 =
	    debugfs_create_file("RX_NORMAL_DESC_descriptor12", 744, dir, NULL,
				&RX_NORMAL_DESC_desc_fops12);
	if (RX_NORMAL_DESC_desc12 == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_descriptor12\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc0 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor0", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops0);
	if (TX_NORMAL_DESC_desc0 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor0\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc1 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor1", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops1);
	if (TX_NORMAL_DESC_desc1 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor1\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc2 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor2", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops2);
	if (TX_NORMAL_DESC_desc2 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor2\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc3 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor3", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops3);
	if (TX_NORMAL_DESC_desc3 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor3\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc4 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor4", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops4);
	if (TX_NORMAL_DESC_desc4 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor4\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc5 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor5", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops5);
	if (TX_NORMAL_DESC_desc5 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor5\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc6 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor6", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops6);
	if (TX_NORMAL_DESC_desc6 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor6\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc7 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor7", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops7);
	if (TX_NORMAL_DESC_desc7 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor7\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc8 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor8", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops8);
	if (TX_NORMAL_DESC_desc8 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor8\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc9 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor9", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops9);
	if (TX_NORMAL_DESC_desc9 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor9\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc10 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor10", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops10);
	if (TX_NORMAL_DESC_desc10 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor10\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc11 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor11", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops11);
	if (TX_NORMAL_DESC_desc11 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor11\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_desc12 =
	    debugfs_create_file("TX_NORMAL_DESC_descriptor12", 744, dir, NULL,
				&TX_NORMAL_DESC_desc_fops12);
	if (TX_NORMAL_DESC_desc12 == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_descriptor12\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	TX_NORMAL_DESC_STATUS =
	    debugfs_create_file("TX_NORMAL_DESC_STATUS", 744, dir, NULL,
				&TX_NORMAL_DESC_status_fops);
	if (TX_NORMAL_DESC_STATUS == NULL) {
		pr_info(
		       "error while creating file: TX_NORMAL_DESC_STATUS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	RX_NORMAL_DESC_STATUS =
	    debugfs_create_file("RX_NORMAL_DESC_STATUS", 744, dir, NULL,
				&RX_NORMAL_DESC_status_fops);
	if (RX_NORMAL_DESC_STATUS == NULL) {
		pr_info(
		       "error while creating file: RX_NORMAL_DESC_STATUS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}
	BCM_REGS =
	    debugfs_create_file("BCM_REGS", 744, dir,
				&bcm_regs_val, &bcm_regs_fops);
	if (BCM_REGS == NULL) {
		pr_info("error creating file: BCM_REGS\n");
		ret = -ENODEV;
		goto remove_debug_file;
	}

	pre_padcal_err_counters =
	    debugfs_create_file("pre_padcal_err_counters", 744, dir,
				NULL, &pre_padcal_err_regs_fops);
	if (pre_padcal_err_counters == NULL) {
		pr_info("error creating file: pre_padcal_err_counters\n");
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
* /sys/kernel/debug/2490000.eqos directory and also the directory
* 2490000.eqos.
*
* \retval  0 on Success.
* \retval  error number on Failure.
*/

void remove_debug_files(void)
{
	DBGPR("--> remove_debug_files\n");
	debugfs_remove_recursive(dir);
	DBGPR("<-- remove_debug_files\n");
}

/* test function to send a packet with buffer aligned to from 0-63.
 * To run the test do 1) uncomment DO_TX_ALIGN_TST in yheader.h
 * 2) start sniffer to capture packets from DUT.
 * 3) after driver is loaded do "echo do_tx_align_tst 1 > do_tx_align_tst".
 * 3) sniffer should have 64 packets with pattern.
 * 4) TX desc is dumped showing buffer address and it's alignment.
 */
static void do_transmit_alignment_test(struct eqos_prv_data *pdata)
{
#ifdef DO_TX_ALIGN_TST
	uint q_idx = 0;
	struct tx_ring *ptx_wr =
	    GET_TX_WRAPPER_DESC(q_idx);

	struct s_tx_desc *ptxd = GET_TX_DESC_PTR(q_idx, ptx_wr->cur_tx);

	unsigned long flags;
	uint i;
	uint start_index = ptx_wr->cur_tx;
	uint last_index;
	u32 *psrc, *pdst;
	uint pkt_size = 0x40;

	DBGPR("-->%s()\n", __func__);
	pr_err("-->%s(): ptxd=%4p\n", __func__, ptxd);

	for (i = 0; i < 2048; i++)
		pdata->ptst_buf[i] = i;

	if (pdata->dt_cfg.intr_mode == MODE_MULTI_IRQ)
		spin_lock_irqsave(&pdata->chinfo[q_idx].chan_tx_lock, flags);
	else
		spin_lock_irqsave(&pdata->tx_lock, flags);


	psrc = (u32 *)ptxd;
	psrc[0] = pdata->tst_buf_dma_addr;
	psrc[1] = 0;
	psrc[2] = 0x80000000 | pkt_size;
	psrc[3] = pkt_size;

	pdst = (u32 *)(ptxd + 1);
	for (i = 0; i < 64; i++) {
		pdst[0] = psrc[0];
		pdst[1] = psrc[1];
		pdst[2] = psrc[2];
		pdst[3] = psrc[3] | 0xb0000000;

		pdst[0] += i; /* cycle addr to different boundary */

		pdst += 4;
	}
	pdst = (u32 *)(ptxd + 2);
	for (i = 0; i < 64; i++) {
		pr_err(
		"%s():[%d] Dw0=0x%.8x, Dw1=0x%.8x, Dw2=0x%.8x, Dw3=0x%.8x\n",
			__func__, i, pdst[0], pdst[1], pdst[2], pdst[3]);

		pdst += 4;
	}
	last_index = start_index + 64;

	/* set OWN bit on first td */
	psrc[3] |= 0xb0000000;

	DMA_TDTP_TPDR_WR(q_idx,
			GET_TX_DESC_DMA_ADDR(q_idx, last_index));


	if (pdata->dt_cfg.intr_mode == MODE_MULTI_IRQ)
		spin_unlock_irqrestore(&pdata->chinfo[q_idx].chan_tx_lock,
					flags);
	else
		spin_unlock_irqrestore(&pdata->tx_lock, flags);

	DBGPR("<--%s()\n", __func__);
#endif
}
