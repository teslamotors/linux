/*
 * drivers/video/tegra/dc/nvsr_regs.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION, All rights reserved.
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

#ifndef __DRIVER_VIDEO_TEGRA_DC_NVSR_REGS_H__

/* Macros and address/field defines were
 * borrowed from big GPU driver */

#define NVSR_SRC_ID0                                          0x3a0
#define NVSR_SR_CAPS0                                         0x335
#define NVSR_SR_CAPS0_SR_CAPABLE_MASK                         1
#define NVSR_SR_CAPS0_SR_CAPABLE_SHIFT                        0
#define NVSR_SR_CAPS0_SR_ENTRY_REQ_MASK                       (3<<1)
#define NVSR_SR_CAPS0_SR_ENTRY_REQ_SHIFT                      1
#define NVSR_SR_CAPS0_SR_EXIT_REQ_MASK                        (3<<3)
#define NVSR_SR_CAPS0_SR_EXIT_REQ_SHIFT                       3
#define NVSR_SR_CAPS0_RESYNC_CAP_MASK                         (7<<5)
#define NVSR_SR_CAPS0_RESYNC_CAP_SHIFT                        5
#define NVSR_SR_CAPS0_RESYNC_CAP_SS                           1
#define NVSR_SR_CAPS0_RESYNC_CAP_FL                           2
#define NVSR_SR_CAPS0_RESYNC_CAP_BS                           4
#define NVSR_SR_CAPS2                                         0x337
#define NVSR_SR_CAPS2_SEPERATE_PCLK_SUPPORTED                 1
#define NVSR_SR_CAPS2_BUFFERED_REFRESH_SUPPORTED              (1<<1)
#define NVSR_SR_CAPS2_FRAME_LOCK_MASK                         (1<<2)
#define NVSR_SR_CAPS2_FRAME_LOCK_SHIFT                        2
#define NVSR_SR_MAX_PCLK0                                     0x339
#define NVSR_SRC_CTL0                                         0x340
#define NVSR_SRC_CTL0_SR_ENABLE_CTL_ENABLED                   1
#define NVSR_SRC_CTL0_SR_ENABLE_CTL_DISABLED                  0
#define NVSR_SRC_CTL0_SR_ENABLE_MASK_ENABLE                   (1<<1)
#define NVSR_SRC_CTL0_SR_ENTRY_REQ_YES                        (1<<4)
#define NVSR_SRC_CTL0_SR_ENTRY_MASK_ENABLE                    (1<<5)
#define NVSR_SRC_CTL0_SR_EXIT_REQ_YES                         (1<<6)
#define NVSR_SRC_CTL0_SR_EXIT_MASK_ENABLE                     (1<<7)
#define NVSR_SRC_CTL1                                         0x341
#define NVSR_SRC_CTL1_SIDEBAND_ENTRY_SEL_YES                  (1<<4)
#define NVSR_SRC_CTL1_SIDEBAND_ENTRY_MASK_ENABLE              (1<<5)
#define NVSR_SRC_CTL1_SIDEBAND_EXIT_SEL_YES                   (1<<6)
#define NVSR_SRC_CTL1_SIDEBAND_EXIT_MASK_ENABLE               (1<<7)
#define NVSR_INTR0                                            0x343
#define NVSR_INTR1                                            0x344
#define NVSR_INTR1_EN_YES                                     1
#define NVSR_INTR1_EN_MASK_ENABLE                             (1<<1)
#define NVSR_RESYNC_CTL                                       0x346
#define NVSR_RESYNC_CTL_METHOD_IMM                            0
#define NVSR_RESYNC_CTL_METHOD_SS                             1
#define NVSR_RESYNC_CTL_METHOD_FL                             2
#define NVSR_RESYNC_CTL_METHOD_BS                             3
#define NVSR_RESYNC_CTL_DELAY_SHIFT                           6
#define NVSR_STATUS                                           0x348
#define NVSR_STATUS_SR_STATE_MASK                             7
#define NVSR_STATUS_SR_STATE_OFFLINE                          0
#define NVSR_STATUS_SR_STATE_IDLE                             1
#define NVSR_STATUS_SR_STATE_SR_ENTRY_TRIG                    2
#define NVSR_STATUS_SR_STATE_SR_ENTRY_CACHING                 3
#define NVSR_STATUS_SR_STATE_SR_ENTRY_RDY                     4
#define NVSR_STATUS_SR_STATE_SR_ACTIVE                        5
#define NVSR_STATUS_SR_STATE_SR_EXIT_TRIG                     6
#define NVSR_STATUS_SR_STATE_SR_EXIT_RESYNC                   7
#define NVSR_SRMODE_PIXEL_CLOCK0                              0x350
#define NVSR_SRMODE_TIMING0                                   0x352
#define NVSR_SRMODE_TIMING2                                   0x354
#define NVSR_SRMODE_TIMING4                                   0x356
#define NVSR_SRMODE_TIMING6                                   0x358
#define NVSR_SRMODE_TIMING8                                   0x35a
#define NVSR_SRMODE_TIMING9                                   0x35b
#define NVSR_SRMODE_TIMING9_HORZ_BORDER_MASK                  0x7f
#define NVSR_SRMODE_TIMING9_HSYNC_POL_MASK                    (1<<7)
#define NVSR_SRMODE_TIMING9_HSYNC_POL_SHIFT                   7
#define NVSR_SRMODE_TIMING9_HSYNC_POL_POS                     (1<<7)
#define NVSR_SRMODE_TIMING9_HSYNC_POL_NEG                     (0<<7)
#define NVSR_SRMODE_TIMING10                                  0x35c
#define NVSR_SRMODE_TIMING12                                  0x35e
#define NVSR_SRMODE_TIMING14                                  0x360
#define NVSR_SRMODE_TIMING16                                  0x362
#define NVSR_SRMODE_TIMING18                                  0x364
#define NVSR_SRMODE_TIMING19                                  0x365
#define NVSR_SRMODE_TIMING19_VERT_BORDER                      0x7f
#define NVSR_SRMODE_TIMING19_VSYNC_POL_MASK                   (1<<7)
#define NVSR_SRMODE_TIMING19_VSYNC_POL_SHIFT                  7
#define NVSR_SRMODE_TIMING19_VSYNC_POL_POS                    (1<<7)
#define NVSR_SRMODE_TIMING19_VSYNC_POL_NEG                    (0<<7)
#define NVSR_PTMODE_PIXEL_CLOCK0                              0x368
#define NVSR_PTMODE_TIMING0                                   0x36a
#define NVSR_PTMODE_TIMING2                                   0x36c
#define NVSR_PTMODE_TIMING4                                   0x36e
#define NVSR_PTMODE_TIMING6                                   0x370
#define NVSR_PTMODE_TIMING8                                   0x372
#define NVSR_PTMODE_TIMING9                                   0x373
#define NVSR_PTMODE_TIMING9_HORZ_BORDER_MASK                  0x7f
#define NVSR_PTMODE_TIMING9_HSYNC_POL_MASK                    (1<<7)
#define NVSR_PTMODE_TIMING9_HSYNC_POL_SHIFT                   7
#define NVSR_PTMODE_TIMING9_HSYNC_POL_POS                     (1<<7)
#define NVSR_PTMODE_TIMING9_HSYNC_POL_NEG                     (0<<7)
#define NVSR_PTMODE_TIMING10                                  0x374
#define NVSR_PTMODE_TIMING12                                  0x376
#define NVSR_PTMODE_TIMING14                                  0x378
#define NVSR_PTMODE_TIMING16                                  0x37a
#define NVSR_PTMODE_TIMING18                                  0x37c
#define NVSR_PTMODE_TIMING19                                  0x37d
#define NVSR_PTMODE_TIMING19_VERT_BORDER_MASK                 0x7f
#define NVSR_PTMODE_TIMING19_VSYNC_POL_MASK                   (1<<7)
#define NVSR_PTMODE_TIMING19_VSYNC_POL_SHIFT                  7
#define NVSR_PTMODE_TIMING19_VSYNC_POL_POS                    (1<<7)
#define NVSR_PTMODE_TIMING19_VSYNC_POL_NEG                    (0<<7)
#define NVSR_BLANK_TIMING0                                    0x388
#define NVSR_BLANK_TIMING2                                    0x38a
#define NVSR_BLANK_TIMING4                                    0x38c
#define NVSR_BLANK_TIMING6                                    0x38e

#endif
