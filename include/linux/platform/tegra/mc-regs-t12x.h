/*
 * Copyright (c) 2014, NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MACH_TEGRA_MC_REGS_T12X_H__
#define __MACH_TEGRA_MC_REGS_T12X_H__

/* Auto generated. Do not edit. */
#define MC_INTSTATUS                                            0x0
#define MC_INTMASK                                              0x4
#define MC_ERR_STATUS                                           0x8
#define MC_ERR_ADR                                              0xc
#define MC_EMEM_CFG                                             0x50
#define MC_EMEM_ADR_CFG                                         0x54
#define MC_EMEM_ADR_CFG_DEV0                                    0x58
#define MC_EMEM_ADR_CFG_DEV1                                    0x5c
#define MC_SECURITY_CFG0                                        0x70
#define MC_SECURITY_CFG1                                        0x74
#define MC_SECURITY_CFG3                                        0x9bc
#define MC_SECURITY_RSV                                         0x7c
#define MC_EMEM_ARB_CFG                                         0x90
#define MC_EMEM_ARB_OUTSTANDING_REQ                             0x94
#define MC_EMEM_ARB_TIMING_RCD                                  0x98
#define MC_EMEM_ARB_TIMING_RP                                   0x9c
#define MC_EMEM_ARB_TIMING_RC                                   0xa0
#define MC_EMEM_ARB_TIMING_RAS                                  0xa4
#define MC_EMEM_ARB_TIMING_FAW                                  0xa8
#define MC_EMEM_ARB_TIMING_RRD                                  0xac
#define MC_EMEM_ARB_TIMING_RAP2PRE                              0xb0
#define MC_EMEM_ARB_TIMING_WAP2PRE                              0xb4
#define MC_EMEM_ARB_TIMING_R2R                                  0xb8
#define MC_EMEM_ARB_TIMING_W2W                                  0xbc
#define MC_EMEM_ARB_TIMING_R2W                                  0xc0
#define MC_EMEM_ARB_TIMING_W2R                                  0xc4
#define MC_EMEM_ARB_DA_TURNS                                    0xd0
#define MC_EMEM_ARB_DA_COVERS                                   0xd4
#define MC_EMEM_ARB_MISC0                                       0xd8
#define MC_EMEM_ARB_MISC1                                       0xdc
#define MC_EMEM_ARB_RING1_THROTTLE                              0xe0
#define MC_EMEM_ARB_RING3_THROTTLE                              0xe4
#define MC_EMEM_ARB_NISO_THROTTLE                               0x6b0
#define MC_EMEM_ARB_OVERRIDE                                    0xe8
#define MC_EMEM_ARB_RSV                                         0xec
#define MC_CLKEN_OVERRIDE                                       0xf4
#define MC_TIMING_CONTROL_DBG                                   0xf8
#define MC_TIMING_CONTROL                                       0xfc
#define MC_STAT_CONTROL                                         0x100
#define MC_STAT_STATUS                                          0x104
#define MC_STAT_EMC_CLOCK_LIMIT                                 0x108
#define MC_STAT_EMC_CLOCK_LIMIT_MSBS                            0x10c
#define MC_STAT_EMC_CLOCKS                                      0x110
#define MC_STAT_EMC_CLOCKS_MSBS                                 0x114
#define MC_STAT_EMC_FILTER_SET0_ADR_LIMIT_LO                    0x118
#define MC_STAT_EMC_FILTER_SET1_ADR_LIMIT_LO                    0x158
#define MC_STAT_EMC_FILTER_SET0_ADR_LIMIT_HI                    0x11c
#define MC_STAT_EMC_FILTER_SET1_ADR_LIMIT_HI                    0x15c
#define MC_STAT_EMC_FILTER_SET0_ADR_LIMIT_UPPER                 0xa20
#define MC_STAT_EMC_FILTER_SET1_ADR_LIMIT_UPPER                 0xa24
#define MC_STAT_EMC_FILTER_SET0_VIRTUAL_ADR_LIMIT_LO            0x198
#define MC_STAT_EMC_FILTER_SET1_VIRTUAL_ADR_LIMIT_LO            0x1a8
#define MC_STAT_EMC_FILTER_SET0_VIRTUAL_ADR_LIMIT_HI            0x19c
#define MC_STAT_EMC_FILTER_SET1_VIRTUAL_ADR_LIMIT_HI            0x1ac
#define MC_STAT_EMC_FILTER_SET0_VIRTUAL_ADR_LIMIT_UPPER         0xa28
#define MC_STAT_EMC_FILTER_SET1_VIRTUAL_ADR_LIMIT_UPPER         0xa2c
#define MC_STAT_EMC_FILTER_SET0_ASID                            0x1a0
#define MC_STAT_EMC_FILTER_SET1_ASID                            0x1b0
#define MC_STAT_EMC_FILTER_SET0_SLACK_LIMIT                     0x120
#define MC_STAT_EMC_FILTER_SET1_SLACK_LIMIT                     0x160
#define MC_STAT_EMC_FILTER_SET0_CLIENT_0                        0x128
#define MC_STAT_EMC_FILTER_SET1_CLIENT_0                        0x168
#define MC_STAT_EMC_FILTER_SET0_CLIENT_1                        0x12c
#define MC_STAT_EMC_FILTER_SET1_CLIENT_1                        0x16c
#define MC_STAT_EMC_FILTER_SET0_CLIENT_2                        0x130
#define MC_STAT_EMC_FILTER_SET1_CLIENT_2                        0x170
#define MC_STAT_EMC_FILTER_SET0_CLIENT_3                        0x134
#define MC_STAT_EMC_FILTER_SET1_CLIENT_3                        0x174
#define MC_STAT_EMC_SET0_COUNT                                  0x138
#define MC_STAT_EMC_SET0_COUNT_MSBS                             0x13c
#define MC_STAT_EMC_SET1_COUNT                                  0x178
#define MC_STAT_EMC_SET1_COUNT_MSBS                             0x17c
#define MC_STAT_EMC_SET0_SLACK_ACCUM                            0x140
#define MC_STAT_EMC_SET0_SLACK_ACCUM_MSBS                       0x144
#define MC_STAT_EMC_SET1_SLACK_ACCUM                            0x180
#define MC_STAT_EMC_SET1_SLACK_ACCUM_MSBS                       0x184
#define MC_STAT_EMC_SET0_HISTO_COUNT                            0x148
#define MC_STAT_EMC_SET0_HISTO_COUNT_MSBS                       0x14c
#define MC_STAT_EMC_SET1_HISTO_COUNT                            0x188
#define MC_STAT_EMC_SET1_HISTO_COUNT_MSBS                       0x18c
#define MC_STAT_EMC_SET0_MINIMUM_SLACK_OBSERVED                 0x150
#define MC_STAT_EMC_SET1_MINIMUM_SLACK_OBSERVED                 0x190
#define MC_STAT_EMC_SET0_IDLE_CYCLE_COUNT                       0x1b8
#define MC_STAT_EMC_SET0_IDLE_CYCL_COUNT_MSBS                   0x1bc
#define MC_STAT_EMC_SET1_IDLE_CYCLE_COUNT                       0x1c8
#define MC_STAT_EMC_SET1_IDLE_CYCL_COUNT_MSBS                   0x1cc
#define MC_STAT_EMC_SET0_IDLE_CYCLE_PARTITION_SELECT            0x1c0
#define MC_STAT_EMC_SET1_IDLE_CYCLE_PARTITION_SELECT            0x1d0
#define MC_CLIENT_HOTRESET_CTRL                                 0x200
#define MC_CLIENT_HOTRESET_CTRL_1                               0x970
#define MC_CLIENT_HOTRESET_STATUS                               0x204
#define MC_CLIENT_HOTRESET_STATUS_1                             0x974
#define MC_EMEM_ARB_ISOCHRONOUS_0                               0x208
#define MC_EMEM_ARB_ISOCHRONOUS_1                               0x20c
#define MC_EMEM_ARB_ISOCHRONOUS_2                               0x210
#define MC_EMEM_ARB_ISOCHRONOUS_3                               0x214
#define MC_EMEM_ARB_HYSTERESIS_0                                0x218
#define MC_EMEM_ARB_HYSTERESIS_1                                0x21c
#define MC_EMEM_ARB_HYSTERESIS_2                                0x220
#define MC_EMEM_ARB_HYSTERESIS_3                                0x224
#define MC_RESERVED_RSV                                         0x3fc
#define MC_DISB_EXTRA_SNAP_LEVELS                               0x408
#define MC_APB_EXTRA_SNAP_LEVELS                                0x2a4
#define MC_AHB_EXTRA_SNAP_LEVELS                                0x2a0
#define MC_USBD_EXTRA_SNAP_LEVELS                               0xa18
#define MC_ISP_EXTRA_SNAP_LEVELS                                0xa08
#define MC_AUD_EXTRA_SNAP_LEVELS                                0xa10
#define MC_MSE_EXTRA_SNAP_LEVELS                                0x40c
#define MC_A9AVPPC_EXTRA_SNAP_LEVELS                            0x414
#define MC_FTOP_EXTRA_SNAP_LEVELS                               0x2bc
#define MC_HOST_EXTRA_SNAP_LEVELS                               0xa14
#define MC_SAX_EXTRA_SNAP_LEVELS                                0x2c0
#define MC_STOP_EXTRA_SNAP_LEVELS                               0x29c
#define MC_DIS_EXTRA_SNAP_LEVELS                                0x2ac
#define MC_VICPC_EXTRA_SNAP_LEVELS                              0xa1c
#define MC_AVP_EXTRA_SNAP_LEVELS                                0x2a8
#define MC_USBX_EXTRA_SNAP_LEVELS                               0x404
#define MC_PCX_EXTRA_SNAP_LEVELS                                0x2b8
#define MC_VD_EXTRA_SNAP_LEVELS                                 0x2d4
#define MC_SD_EXTRA_SNAP_LEVELS                                 0xa04
#define MC_VE_EXTRA_SNAP_LEVELS                                 0x2d8
#define MC_GK_EXTRA_SNAP_LEVELS                                 0xa00
#define MC_VE2_EXTRA_SNAP_LEVELS                                0x410
#define MC_DISPLAY_SNAP_RING                                    0x608
#define MC_VIDEO_PROTECT_BOM                                    0x648
#define MC_VIDEO_PROTECT_SIZE_MB                                0x64c
#define MC_VIDEO_PROTECT_BOM_ADR_HI                             0x978
#define MC_VIDEO_PROTECT_REG_CTRL                               0x650
#define MC_ERR_VPR_STATUS                                       0x654
#define MC_ERR_VPR_ADR                                          0x658
#define MC_VIDEO_PROTECT_VPR_OVERRIDE                           0x418
#define MC_VIDEO_PROTECT_VPR_OVERRIDE1                          0x590
#define MC_IRAM_BOM                                             0x65c
#define MC_IRAM_TOM                                             0x660
#define MC_IRAM_ADR_HI                                          0x980
#define MC_IRAM_REG_CTRL                                        0x964
#define MC_EMEM_CFG_ACCESS_CTRL                                 0x664
#define MC_TZ_SECURITY_CTRL                                     0x668
#define MC_EMEM_ARB_OUTSTANDING_REQ_RING3                       0x66c
#define MC_EMEM_ARB_OUTSTANDING_REQ_NISO                        0x6b4
#define MC_EMEM_ARB_RING0_THROTTLE_MASK                         0x6bc
#define MC_EMEM_ARB_NISO_THROTTLE_MASK                          0x6b8
#define MC_SEC_CARVEOUT_BOM                                     0x670
#define MC_SEC_CARVEOUT_SIZE_MB                                 0x674
#define MC_SEC_CARVEOUT_ADR_HI                                  0x9d4
#define MC_SEC_CARVEOUT_REG_CTRL                                0x678
#define MC_ERR_SEC_STATUS                                       0x67c
#define MC_ERR_SEC_ADR                                          0x680
#define MC_PC_IDLE_CLOCK_GATE_CONFIG                            0x684
#define MC_PC_IDLE_CLOCK_GATE                                   0x954
#define MC_STUTTER_CONTROL                                      0x688
#define MC_RESERVED_RSV_1                                       0x958
#define MC_DVFS_PIPE_SELECT                                     0x95c
#define MC_AUD_PTSA_MIN                                         0x54c
#define MC_AHB_PTSA_MIN                                         0x4e0
#define MC_MLL_MPCORER_PTSA_RATE                                0x44c
#define MC_RING2_PTSA_RATE                                      0x440
#define MC_USBD_PTSA_RATE                                       0x530
#define MC_USBX_PTSA_MIN                                        0x528
#define MC_USBD_PTSA_MIN                                        0x534
#define MC_APB_PTSA_MAX                                         0x4f0
#define MC_R0_DIS_PTSA_MAX                                      0x46c
#define MC_R0_DISB_PTSA_MAX                                     0x478
#define MC_DIS_PTSA_MIN                                         0x420
#define MC_AVP_PTSA_MAX                                         0x4fc
#define MC_AVP_PTSA_RATE                                        0x4f4
#define MC_RING1_PTSA_MIN                                       0x480
#define MC_DIS_PTSA_MAX                                         0x424
#define MC_SD_PTSA_MAX                                          0x4d8
#define MC_MSE_PTSA_RATE                                        0x4c4
#define MC_VICPC_PTSA_MIN                                       0x558
#define MC_PCX_PTSA_MAX                                         0x4b4
#define MC_ISP_PTSA_RATE                                        0x4a0
#define MC_A9AVPPC_PTSA_MIN                                     0x48c
#define MC_RING2_PTSA_MAX                                       0x448
#define MC_AUD_PTSA_RATE                                        0x548
#define MC_HOST_PTSA_MIN                                        0x51c
#define MC_MLL_MPCORER_PTSA_MAX                                 0x454
#define MC_SD_PTSA_MIN                                          0x4d4
#define MC_RING1_PTSA_RATE                                      0x47c
#define MC_R0_DIS_PTSA_MIN                                      0x468
#define MC_VD_PTSA_RATE                                         0x500
#define MC_AVP_PTSA_MIN                                         0x4f8
#define MC_VE_PTSA_MAX                                          0x43c
#define MC_VICPC_PTSA_RATE                                      0x554
#define MC_GK_PTSA_MAX                                          0x544
#define MC_R0_DISB_PTSA_MIN                                     0x474
#define MC_VICPC_PTSA_MAX                                       0x55c
#define MC_SAX_PTSA_RATE                                        0x4b8
#define MC_PCX_PTSA_MIN                                         0x4b0
#define MC_APB_PTSA_MIN                                         0x4ec
#define MC_PCX_PTSA_RATE                                        0x4ac
#define MC_RING1_PTSA_MAX                                       0x484
#define MC_MLL_MPCORER_PTSA_MIN                                 0x450
#define MC_AUD_PTSA_MAX                                         0x550
#define MC_ISP_PTSA_MAX                                         0x4a8
#define MC_DISB_PTSA_RATE                                       0x428
#define MC_VE2_PTSA_MAX                                         0x49c
#define MC_FTOP_PTSA_RATE                                       0x50c
#define MC_R0_DIS_PTSA_RATE                                     0x464
#define MC_A9AVPPC_PTSA_RATE                                    0x488
#define MC_VE2_PTSA_MIN                                         0x498
#define MC_USBX_PTSA_MAX                                        0x52c
#define MC_DIS_PTSA_RATE                                        0x41c
#define MC_USBD_PTSA_MAX                                        0x538
#define MC_A9AVPPC_PTSA_MAX                                     0x490
#define MC_USBX_PTSA_RATE                                       0x524
#define MC_FTOP_PTSA_MAX                                        0x514
#define MC_SD_PTSA_RATE                                         0x4d0
#define MC_AHB_PTSA_RATE                                        0x4dc
#define MC_FTOP_PTSA_MIN                                        0x510
#define MC_SMMU_SMMU_PTSA_MAX                                   0x460
#define MC_RING2_PTSA_MIN                                       0x444
#define MC_APB_PTSA_RATE                                        0x4e8
#define MC_MSE_PTSA_MIN                                         0x4c8
#define MC_HOST_PTSA_RATE                                       0x518
#define MC_VE_PTSA_RATE                                         0x434
#define MC_AHB_PTSA_MAX                                         0x4e4
#define MC_SAX_PTSA_MIN                                         0x4bc
#define MC_SMMU_SMMU_PTSA_MIN                                   0x45c
#define MC_ISP_PTSA_MIN                                         0x4a4
#define MC_HOST_PTSA_MAX                                        0x520
#define MC_R0_DISB_PTSA_RATE                                    0x470
#define MC_SAX_PTSA_MAX                                         0x4c0
#define MC_VE_PTSA_MIN                                          0x438
#define MC_GK_PTSA_MIN                                          0x540
#define MC_MSE_PTSA_MAX                                         0x4cc
#define MC_DISB_PTSA_MAX                                        0x430
#define MC_DISB_PTSA_MIN                                        0x42c
#define MC_SMMU_SMMU_PTSA_RATE                                  0x458
#define MC_VE2_PTSA_RATE                                        0x494
#define MC_VD_PTSA_MIN                                          0x504
#define MC_VD_PTSA_MAX                                          0x508
#define MC_GK_PTSA_RATE                                         0x53c
#define MC_PTSA_GRANT_DECREMENT                                 0x960
#define MC_LATENCY_ALLOWANCE_AVPC_0                             0x2e4
#define MC_LATENCY_ALLOWANCE_XUSB_1                             0x380
#define MC_LATENCY_ALLOWANCE_ISP2B_0                            0x384
#define MC_LATENCY_ALLOWANCE_SDMMCAA_0                          0x3bc
#define MC_LATENCY_ALLOWANCE_MPCORELP_0                         0x324
#define MC_LATENCY_ALLOWANCE_SDMMCA_0                           0x3b8
#define MC_LATENCY_ALLOWANCE_ISP2_0                             0x370
#define MC_LATENCY_ALLOWANCE_ISP2_1                             0x374
#define MC_LATENCY_ALLOWANCE_DC_0                               0x2e8
#define MC_LATENCY_ALLOWANCE_VIC_0                              0x394
#define MC_LATENCY_ALLOWANCE_VDE_3                              0x360
#define MC_LATENCY_ALLOWANCE_DCB_1                              0x2f8
#define MC_LATENCY_ALLOWANCE_DCB_2                              0x2fc
#define MC_LATENCY_ALLOWANCE_TSEC_0                             0x390
#define MC_LATENCY_ALLOWANCE_DC_2                               0x2f0
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0AB                  0x694
#define MC_LATENCY_ALLOWANCE_VDE_2                              0x35c
#define MC_LATENCY_ALLOWANCE_PPCS_1                             0x348
#define MC_LATENCY_ALLOWANCE_XUSB_0                             0x37c
#define MC_LATENCY_ALLOWANCE_MSENC_0                            0x328
#define MC_LATENCY_ALLOWANCE_PPCS_0                             0x344
#define MC_LATENCY_ALLOWANCE_AFI_0                              0x2e0
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0B                   0x698
#define MC_LATENCY_ALLOWANCE_VDE_0                              0x354
#define MC_LATENCY_ALLOWANCE_DC_1                               0x2ec
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0C                   0x6a0
#define MC_LATENCY_ALLOWANCE_VDE_1                              0x358
#define MC_LATENCY_ALLOWANCE_A9AVP_0                            0x3a4
#define MC_LATENCY_ALLOWANCE_SDMMC_0                            0x3c0
#define MC_LATENCY_ALLOWANCE_DCB_0                              0x2f4
#define MC_LATENCY_ALLOWANCE_HC_1                               0x314
#define MC_LATENCY_ALLOWANCE_PTC_0                              0x34c
#define MC_LATENCY_ALLOWANCE_MPCORE_0                           0x320
#define MC_LATENCY_ALLOWANCE_VI2_0                              0x398
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0BB                  0x69c
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0CB                  0x6a4
#define MC_LATENCY_ALLOWANCE_SATA_0                             0x350
#define MC_SCALED_LATENCY_ALLOWANCE_DISPLAY0A                   0x690
#define MC_LATENCY_ALLOWANCE_HC_0                               0x310
#define MC_LATENCY_ALLOWANCE_DC_3                               0x3c8
#define MC_LATENCY_ALLOWANCE_GPU_0                              0x3ac
#define MC_LATENCY_ALLOWANCE_SDMMCAB_0                          0x3c4
#define MC_LATENCY_ALLOWANCE_ISP2B_1                            0x388
#define MC_LATENCY_ALLOWANCE_HDA_0                              0x318
#define MC_MIN_LENGTH_DCB_2                                     0x8a8
#define MC_MIN_LENGTH_A9AVP_0                                   0x950
#define MC_MIN_LENGTH_TSEC_0                                    0x93c
#define MC_MIN_LENGTH_DC_1                                      0x898
#define MC_MIN_LENGTH_ISP2B_0                                   0x930
#define MC_MIN_LENGTH_VI2_0                                     0x944
#define MC_MIN_LENGTH_VDE_3                                     0x90c
#define MC_MIN_LENGTH_DCB_0                                     0x8a0
#define MC_MIN_LENGTH_DCB_1                                     0x8a4
#define MC_MIN_LENGTH_PPCS_1                                    0x8f4
#define MC_MIN_LENGTH_MPCORELP_0                                0x8d0
#define MC_MIN_LENGTH_HDA_0                                     0x8c4
#define MC_MIN_LENGTH_SDMMC_0                                   0xb18
#define MC_MIN_LENGTH_ISP2B_1                                   0x934
#define MC_MIN_LENGTH_HC_1                                      0x8c0
#define MC_MIN_LENGTH_DC_3                                      0xb20
#define MC_MIN_LENGTH_AVPC_0                                    0x890
#define MC_MIN_LENGTH_VIC_0                                     0x940
#define MC_MIN_LENGTH_ISP2_0                                    0x91c
#define MC_MIN_LENGTH_HC_0                                      0x8bc
#define MC_MIN_LENGTH_SATA_0                                    0x8fc
#define MC_MIN_LENGTH_VDE_2                                     0x908
#define MC_MIN_LENGTH_DC_0                                      0x894
#define MC_MIN_LENGTH_XUSB_1                                    0x92c
#define MC_MIN_LENGTH_DC_2                                      0x89c
#define MC_MIN_LENGTH_SDMMCAA_0                                 0xb14
#define MC_MIN_LENGTH_GPU_0                                     0xb04
#define MC_MIN_LENGTH_AFI_0                                     0x88c
#define MC_MIN_LENGTH_PPCS_0                                    0x8f0
#define MC_MIN_LENGTH_ISP2_1                                    0x920
#define MC_MIN_LENGTH_XUSB_0                                    0x928
#define MC_MIN_LENGTH_MPCORE_0                                  0x8cc
#define MC_MIN_LENGTH_VDE_1                                     0x904
#define MC_MIN_LENGTH_SDMMCA_0                                  0xb10
#define MC_MIN_LENGTH_MSENC_0                                   0x8d4
#define MC_MIN_LENGTH_SDMMCAB_0                                 0xb1c
#define MC_MIN_LENGTH_PTC_0                                     0x8f8
#define MC_MIN_LENGTH_VDE_0                                     0x900
#define MC_EMEM_ARB_OVERRIDE_1                                  0x968
#define MC_VIDEO_PROTECT_GPU_OVERRIDE_0                         0x984
#define MC_VIDEO_PROTECT_GPU_OVERRIDE_1                         0x988
#define MC_EMEM_ARB_STATS_0                                     0x990
#define MC_EMEM_ARB_STATS_1                                     0x994
#define MC_MTS_CARVEOUT_BOM                                     0x9a0
#define MC_MTS_CARVEOUT_SIZE_MB                                 0x9a4
#define MC_MTS_CARVEOUT_ADR_HI                                  0x9a8
#define MC_MTS_CARVEOUT_REG_CTRL                                0x9ac
#define MC_ERR_MTS_STATUS                                       0x9b0
#define MC_ERR_MTS_ADR                                          0x9b4
#define MC_EMEM_BANK_SWIZZLE_CFG0                               0x9c0
#define MC_EMEM_BANK_SWIZZLE_CFG1                               0x9c4
#define MC_EMEM_BANK_SWIZZLE_CFG2                               0x9c8
#define MC_EMEM_BANK_SWIZZLE_CFG3                               0x9cc
#define MC_ERR_APB_ASID_UPDATE_STATUS                           0x9d0


#define HYST_SATAR				(0x1 << 31)
#define HYST_PPCSAHBSLVR			(0x1 << 30)
#define HYST_PPCSAHBDMAR			(0x1 << 29)
#define HYST_MSENCSRD				(0x1 << 28)
#define HYST_HOST1XR				(0x1 << 23)
#define HYST_HOST1XDMAR				(0x1 << 22)
#define HYST_HDAR				(0x1 << 21)
#define HYST_DISPLAYHCB				(0x1 << 17)
#define HYST_DISPLAYHC				(0x1 << 16)
#define HYST_AVPCARM7R				(0x1 << 15)
#define HYST_AFIR				(0x1 << 14)
#define HYST_DISPLAY0CB				(0x1 << 6)
#define HYST_DISPLAY0C				(0x1 << 5)
#define YST_DISPLAY0BB				(0x1 << 4)
#define YST_DISPLAY0B				(0x1 << 3)
#define YST_DISPLAY0AB				(0x1 << 2)
#define YST_DISPLAY0A				(0x1 << 1)
#define HYST_PTCR				(0x1 << 0)

#define HYST_VDEDBGW				(0x1 << 31)
#define HYST_VDEBSEVW				(0x1 << 30)
#define HYST_SATAW				(0x1 << 29)
#define HYST_PPCSAHBSLVW			(0x1 << 28)
#define HYST_PPCSAHBDMAW			(0x1 << 27)
#define HYST_MPCOREW				(0x1 << 25)
#define HYST_MPCORELPW				(0x1 << 24)
#define HYST_HOST1XW				(0x1 << 22)
#define HYST_HDAW				(0x1 << 21)
#define HYST_AVPCARM7W				(0x1 << 18)
#define HYST_AFIW				(0x1 << 17)
#define HYST_MSENCSWR				(0x1 << 11)
#define YST_MPCORER				(0x1 << 7)
#define YST_MPCORELPR				(0x1 << 6)
#define YST_VDETPER				(0x1 << 5)
#define YST_VDEMCER				(0x1 << 4)
#define YST_VDEMBER				(0x1 << 3)
#define YST_VDEBSEVR				(0x1 << 2)

#define HYST_DISPLAYT				(0x1 << 26)
#define HYST_GPUSWR				(0x1 << 25)
#define HYST_GPUSRD				(0x1 << 24)
#define HYST_A9AVPSCW				(0x1 << 23)
#define HYST_A9AVPSCR				(0x1 << 22)
#define HYST_TSECSWR				(0x1 << 21)
#define HYST_TSECSRD				(0x1 << 20)
#define HYST_ISPWBB				(0x1 << 17)
#define HYST_ISPWAB				(0x1 << 16)
#define HYST_ISPRAB				(0x1 << 14)
#define HYST_XUSB_DEVW				(0x1 << 13)
#define HYST_XUSB_DEVR				(0x1 << 12)
#define HYST_XUSB_HOSTW				(0x1 << 11)
#define HYST_XUSB_HOSTR				(0x1 << 10)
#define YST_ISPWB				(0x1 << 7)
#define YST_ISPWA				(0x1 << 6)
#define YST_ISPRA				(0x1 << 4)
#define YST_VDETPMW				(0x1 << 1)
#define YST_VDEMBEW				(0x1 << 0)

#define HYST_DISPLAYD				(0x1 << 19)
#define HYST_VIW				(0x1 << 18)
#define HYST_VICSWR				(0x1 << 13)
#define HYST_VICSRD				(0x1 << 12)
#define YST_SDMMCWAB				(0x1 << 7)
#define YST_SDMMCW				(0x1 << 6)
#define YST_SDMMCWAA				(0x1 << 5)
#define YST_SDMMCWA				(0x1 << 4)
#define YST_SDMMCRAB				(0x1 << 3)
#define YST_SDMMCR				(0x1 << 2)
#define YST_SDMMCRAA				(0x1 << 1)
#define YST_SDMMCRA				(0x1 << 0)

/* NOTE: this must match the # of MC_LATENCY_XXX above */
#define T12X_MC_LATENCY_ALLOWANCE_NUM_REGS 38

#endif
