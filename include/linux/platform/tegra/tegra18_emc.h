/*
 * Copyright (c) 2016, NVIDIA Corporation. All rights reserved.
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

#ifndef __TEGRA18_EMC_H__
#define __TEGRA18_EMC_H__

/* HW Register offsets */

#define EMC_INTSTATUS                           0x0

#define ECC_ERR_BUF_OVF_INT_MASK                0x1
#define ECC_ERR_BUF_OVF_INT_SHIFT                12

#define ECC_CORR_ERR_INT_MASK                   0x1
#define ECC_CORR_ERR_INT_SHIFT                   11

#define ECC_UNCORR_ERR_INT_MASK                 0x1
#define ECC_UNCORR_ERR_INT_SHIFT                 10

#define EMC_INTMASK                             0x4

#define EMC_NONCRITICAL_INTMASK               0x514

#define EMC_CRITICAL_INTMASK                  0x510

#define EMC_ECC_CONTROL                       0xac0

#define ERR_BUFFER_MODE_MASK                    0x1
#define ERR_BUFFER_MODE_SHIFT                     0

#define ERR_BUFFER_RESET_MASK                   0x1
#define ERR_BUFFER_RESET_SHIFT                    1

#define ERR_BUFFER_LOAD_MASK                    0x1
#define ERR_BUFFER_LOAD_SHIFT                     2

#define ERR_BUFFER_LIMIT_MASK                  0xFF
#define ERR_BUFFER_LIMIT_SHIFT                    8


#define EMC_ECC_STATUS                        0xac4

#define ERR_BUFFER_CNT_MASK                    0xFF
#define ERR_BUFFER_CNT_SHIFT                     16

#define ERR_BUFFER_DEPTH_MASK                  0xFF
#define ERR_BUFFER_DEPTH_SHIFT                   24


#define EMC_ECC_ERR_REQ                       0xad4

#define ECC_ERR_CGID_MASK                      0xFF
#define ECC_ERR_CGID_SHIFT                       24

#define ECC_ERR_SEQ_MASK                        0x3
#define ECC_ERR_SEQ_SHIFT                        20

#define ECC_ERR_EMC_ID_MASK                     0x3
#define ECC_ERR_EMC_ID_SHIFT                      6

#define ECC_ERR_DEVICE_MASK                     0x1
#define ECC_ERR_DEVICE_SHIFT                     16

#define ECC_ERR_SIZE_MASK                       0x3
#define ECC_ERR_SIZE_SHIFT                       17

#define ECC_ERR_SWAP_MASK                       0x1
#define ECC_ERR_SWAP_SHIFT                       19

#define ECC_ERR_COL_SP0_MASK                    0x7
#define ECC_ERR_COL_SP0_SHIFT                     8

#define ECC_ERR_COL_SP1_MASK                    0x7
#define ECC_ERR_COL_SP1_SHIFT                    12


#define EMC_ECC_ERR_SP0                       0xac8
#define EMC_ECC_ERR_SP1                       0xacc

#define ECC_EERR_PAR_MASK                       0x3
#define ECC_EERR_PAR_SHIFT                       16

#define ECC_DERR_PAR_MASK                       0x3
#define ECC_DERR_PAR_SHIFT                        0

#define ECC_ERR_POISON_MASK                     0x1
#define ECC_ERR_POISON_SHIFT                      3

#define ECC_DERR_SYNDROME_MASK                0x3FF
#define ECC_DERR_SYNDROME_SHIFT                   6

#define EMC_ECC_ERR_ADDR                      0xad0

#define ECC_ERR_ROW_MASK                     0xFFFF
#define ECC_ERR_ROW_SHIFT                         8

#define ECC_ERR_BANK_MASK                       0x7
#define ECC_ERR_BANK_SHIFT                       28

#define ECC_ERR_GOB_MASK                       0x3F
#define ECC_ERR_GOB_SHIFT                         0

#define EMC_ECC_CONTROL                       0xac0

#define MC_MEM_SCRUBBER_ECC_ADDR              0xf18

#define MC_MEM_SCRUBBER_ECC_REG_CTRL          0xf20

#define SCRUB_ECC_TRIGGER_SHIFT                   0

#define SCRUB_ECC_PENDING_MASK                  0x1
#define SCRUB_ECC_PENDING_SHIFT                   1

#define MC_TIMING_CONTROL_DBG                  0xf8
#define MC_EMEM_ARB_CFG                        0x90
#define MC_EMEM_ARB_MISC1                      0xdc

#define MC_ECC_CONTROL                        0x1880

#define EMC_MCH_GLOBAL_NONCRITICAL_INTSTATUS          0x474
#define EMC_MCH_GLOBAL_CRITICAL_INTSTATUS             0x450
#define EMC_MCH_GLOBAL_INTSTATUS                      0x44c

#define EMC_BROADCAST_CHANNEL                    -1

/**
 * Read from the EMC.
 *
 * @idx The EMC channel to read from.
 * @reg The offset of the register to read.
 *
 * Read from the specified EMC channel: 0 -> EMC0, 1 -> EMC1, etc. If @idx
 * corresponds to a non-existent channel then 0 is returned.
 */
static inline u32 __emc_readl(int idx, u32 reg)
{
	if (WARN(!emc, "Read before EMC init'ed"))
		return 0;

	if ((idx != EMC_BROADCAST_CHANNEL && idx < 0) ||
		idx >= MAX_CHANNELS)
		return 0;

	if (idx == EMC_BROADCAST_CHANNEL)
		return readl(emc + reg);
	else
		return readl(emc_regs[idx] + reg);
}

/**
 * Write to the EMC.
 *
 * @idx The EMC channel to write to.
 * @val Value to write.
 * @reg The offset of the register to write.
 *
 * Write to the specified EMC channel: 0 -> EMC0, 1 -> EMC1, etc. For writes
 * there is a special channel, %EMC_BROADCAST_CHANNEL, which writes to all
 * channels. If @idx corresponds to a non-existent channel then the
 * write is dropped.
 */
static inline void __emc_writel(int idx, u32 val, u32 reg)
{
	if (WARN(!emc, "Write before EMC init'ed"))
		return;

	if ((idx != EMC_BROADCAST_CHANNEL && idx < 0) ||
	    idx >= MAX_CHANNELS)
		return;

	if (idx == EMC_BROADCAST_CHANNEL)
		writel(val, emc + reg);
	else
		writel(val, emc_regs[idx] + reg);
}

#define emc_readl(reg)       __emc_readl(EMC_BROADCAST_CHANNEL, reg)
#define emc_writel(val, reg) __emc_writel(EMC_BROADCAST_CHANNEL, val, reg)

#endif
