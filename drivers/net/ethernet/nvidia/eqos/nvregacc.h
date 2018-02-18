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
 */
#ifndef __NV__REGACC__H__

#define __NV__REGACC__H__

#define CLK_CRTL0_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x8000))

#define CLK_CRTL0_WR(data) do {\
		iowrite32(data, (void *)CLK_CRTL0_OFFSET);\
} while (0)

#define CLK_CRTL0_RD(data) do {\
		(data) = ioread32((void *)CLK_CRTL0_OFFSET);\
} while (0)

/*#define  CLK_CRTL0_TX_CLK_MASK (ULONG)(~(~0<<(1))) << (30)))*/

#define CLK_CRTL0_TX_CLK_MASK (ULONG)(0x1)

/*#define CLK_CRTL0_TX_CLK_WR_MASK (unsigned long)(~((~(~0 << (1))) << (30)))*/

#define CLK_CRTL0_TX_CLK_WR_MASK (ULONG)(0xbfffffff)

#define CLK_CRTL0_TX_CLK_WR(data) do {\
		ULONG v;\
		CLK_CRTL0_RD(v);\
		v = ((v & CLK_CRTL0_TX_CLK_WR_MASK) | ((data & CLK_CRTL0_TX_CLK_MASK)<<30));\
		CLK_CRTL0_WR(v);\
} while (0)

#define CLK_CRTL0_TX_CLK_RD(data) do {\
		CLK_CRTL0_RD(data);\
		data = ((data >> 30) & CLK_CRTL0_TX_CLK_MASK);\
} while (0)

#define VIRT_INTR_CH0_CRTL0_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x8600))

#define VIRT_INTR_CH0_CRTL0_WR(data) do {\
		iowrite32(data, (void *)VIRT_INTR_CH0_CRTL0_OFFSET);\
} while (0)

#define VIRT_INTR_CH0_CRTL0_RD(data) do {\
		(data) = ioread32((void *)VIRT_INTR_CH0_CRTL0_OFFSET);\
} while (0)

/*#define  VIRT_INTR_CH0_CRTL0_TX_MASK (ULONG)(~(~0<<(1))) << (0)))*/

#define VIRT_INTR_CH0_CRTL0_TX_MASK (ULONG)(0x1)

/*#define VIRT_INTR_CH0_CRTL0_TX_WR_MASK (unsigned long)(~((~(~0 << (1))) << (0)))*/

#define VIRT_INTR_CH0_CRTL0_TX_WR_MASK (ULONG)(0xfffffffe)

#define VIRT_INTR_CH0_CRTL0_TX_WR(data) do {\
		ULONG v;\
		VIRT_INTR_CH0_CRTL0_RD(v);\
		v = ((v & VIRT_INTR_CH0_CRTL0_TX_WR_MASK) | ((data & VIRT_INTR_CH0_CRTL0_TX_MASK)<<0));\
		VIRT_INTR_CH0_CRTL0_WR(v);\
} while (0)

#define VIRT_INTR_CH0_CRTL0_TX_RD(data) do {\
		VIRT_INTR_CH0_CRTL0_RD(data);\
		data = ((data >> 0) & VIRT_INTR_CH0_CRTL0_TX_MASK);\
} while (0)

/*#define  VIRT_INTR_CH0_CRTL0_RX_MASK (ULONG)(~(~0<<(1))) << (1)))*/

#define VIRT_INTR_CH0_CRTL0_RX_MASK (ULONG)(0x1)

/*#define VIRT_INTR_CH0_CRTL0_RX_WR_MASK (unsigned long)(~((~(~0 << (1))) << (1)))*/

#define VIRT_INTR_CH0_CRTL0_RX_WR_MASK (ULONG)(0xfffffffd)

#define VIRT_INTR_CH0_CRTL0_RX_WR(data) do {\
		ULONG v;\
		VIRT_INTR_CH0_CRTL0_RD(v);\
		v = ((v & VIRT_INTR_CH0_CRTL0_RX_WR_MASK) | ((data & VIRT_INTR_CH0_CRTL0_RX_MASK)<<1));\
		VIRT_INTR_CH0_CRTL0_WR(v);\
} while (0)

#define VIRT_INTR_CH0_CRTL0_RX_RD(data) do {\
		VIRT_INTR_CH0_CRTL0_RD(data);\
		data = ((data >> 1) & VIRT_INTR_CH0_CRTL0_RX_MASK);\
} while (0)


#define VIRT_INTR_CH1_CRTL0_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x8608))

#define VIRT_INTR_CH1_CRTL0_WR(data) do {\
		iowrite32(data, (void *)VIRT_INTR_CH1_CRTL0_OFFSET);\
} while (0)

#define VIRT_INTR_CH1_CRTL0_RD(data) do {\
		(data) = ioread32((void *)VIRT_INTR_CH1_CRTL0_OFFSET);\
} while (0)

/*#define  VIRT_INTR_CH1_CRTL0_TX_MASK (ULONG)(~(~0<<(1))) << (0)))*/

#define VIRT_INTR_CH1_CRTL0_TX_MASK (ULONG)(0x1)

/*#define VIRT_INTR_CH1_CRTL0_TX_WR_MASK (unsigned long)(~((~(~0 << (1))) << (0)))*/

#define VIRT_INTR_CH1_CRTL0_TX_WR_MASK (ULONG)(0xfffffffe)

#define VIRT_INTR_CH1_CRTL0_TX_WR(data) do {\
		ULONG v;\
		VIRT_INTR_CH1_CRTL0_RD(v);\
		v = ((v & VIRT_INTR_CH1_CRTL0_TX_WR_MASK) | ((data & VIRT_INTR_CH1_CRTL0_TX_MASK)<<0));\
		VIRT_INTR_CH1_CRTL0_WR(v);\
} while (0)

#define VIRT_INTR_CH1_CRTL0_TX_RD(data) do {\
		VIRT_INTR_CH1_CRTL0_RD(data);\
		data = ((data >> 0) & VIRT_INTR_CH1_CRTL0_TX_MASK);\
} while (0)

/*#define  VIRT_INTR_CH1_CRTL0_RX_MASK (ULONG)(~(~0<<(1))) << (1)))*/

#define VIRT_INTR_CH1_CRTL0_RX_MASK (ULONG)(0x1)

/*#define VIRT_INTR_CH1_CRTL0_RX_WR_MASK (unsigned long)(~((~(~0 << (1))) << (1)))*/

#define VIRT_INTR_CH1_CRTL0_RX_WR_MASK (ULONG)(0xfffffffd)

#define VIRT_INTR_CH1_CRTL0_RX_WR(data) do {\
		ULONG v;\
		VIRT_INTR_CH1_CRTL0_RD(v);\
		v = ((v & VIRT_INTR_CH1_CRTL0_RX_WR_MASK) | ((data & VIRT_INTR_CH1_CRTL0_RX_MASK)<<1));\
		VIRT_INTR_CH1_CRTL0_WR(v);\
} while (0)

#define VIRT_INTR_CH1_CRTL0_RX_RD(data) do {\
		VIRT_INTR_CH1_CRTL0_RD(data);\
		data = ((data >> 1) & VIRT_INTR_CH1_CRTL0_RX_MASK);\
} while (0)


#define VIRT_INTR_CH2_CRTL0_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x8610))

#define VIRT_INTR_CH2_CRTL0_WR(data) do {\
		iowrite32(data, (void *)VIRT_INTR_CH2_CRTL0_OFFSET);\
} while (0)

#define VIRT_INTR_CH2_CRTL0_RD(data) do {\
		(data) = ioread32((void *)VIRT_INTR_CH2_CRTL0_OFFSET);\
} while (0)

/*#define  VIRT_INTR_CH2_CRTL0_TX_MASK (ULONG)(~(~0<<(1))) << (0)))*/

#define VIRT_INTR_CH2_CRTL0_TX_MASK (ULONG)(0x1)

/*#define VIRT_INTR_CH2_CRTL0_TX_WR_MASK (unsigned long)(~((~(~0 << (1))) << (0)))*/

#define VIRT_INTR_CH2_CRTL0_TX_WR_MASK (ULONG)(0xfffffffe)

#define VIRT_INTR_CH2_CRTL0_TX_WR(data) do {\
		ULONG v;\
		VIRT_INTR_CH2_CRTL0_RD(v);\
		v = ((v & VIRT_INTR_CH2_CRTL0_TX_WR_MASK) | ((data & VIRT_INTR_CH2_CRTL0_TX_MASK)<<0));\
		VIRT_INTR_CH2_CRTL0_WR(v);\
} while (0)

#define VIRT_INTR_CH2_CRTL0_TX_RD(data) do {\
		VIRT_INTR_CH2_CRTL0_RD(data);\
		data = ((data >> 0) & VIRT_INTR_CH2_CRTL0_TX_MASK);\
} while (0)

/*#define  VIRT_INTR_CH2_CRTL0_RX_MASK (ULONG)(~(~0<<(1))) << (1)))*/

#define VIRT_INTR_CH2_CRTL0_RX_MASK (ULONG)(0x1)

/*#define VIRT_INTR_CH2_CRTL0_RX_WR_MASK (unsigned long)(~((~(~0 << (1))) << (1)))*/

#define VIRT_INTR_CH2_CRTL0_RX_WR_MASK (ULONG)(0xfffffffd)

#define VIRT_INTR_CH2_CRTL0_RX_WR(data) do {\
		ULONG v;\
		VIRT_INTR_CH2_CRTL0_RD(v);\
		v = ((v & VIRT_INTR_CH2_CRTL0_RX_WR_MASK) | ((data & VIRT_INTR_CH2_CRTL0_RX_MASK)<<1));\
		VIRT_INTR_CH2_CRTL0_WR(v);\
} while (0)

#define VIRT_INTR_CH2_CRTL0_RX_RD(data) do {\
		VIRT_INTR_CH2_CRTL0_RD(data);\
		data = ((data >> 1) & VIRT_INTR_CH2_CRTL0_RX_MASK);\
} while (0)


#define VIRT_INTR_CH3_CRTL0_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x8618))

#define VIRT_INTR_CH3_CRTL0_WR(data) do {\
		iowrite32(data, (void *)VIRT_INTR_CH3_CRTL0_OFFSET);\
} while (0)

#define VIRT_INTR_CH3_CRTL0_RD(data) do {\
		(data) = ioread32((void *)VIRT_INTR_CH3_CRTL0_OFFSET);\
} while (0)

/*#define  VIRT_INTR_CH3_CRTL0_TX_MASK (ULONG)(~(~0<<(1))) << (0)))*/

#define VIRT_INTR_CH3_CRTL0_TX_MASK (ULONG)(0x1)

/*#define VIRT_INTR_CH3_CRTL0_TX_WR_MASK (unsigned long)(~((~(~0 << (1))) << (0)))*/

#define VIRT_INTR_CH3_CRTL0_TX_WR_MASK (ULONG)(0xfffffffe)

#define VIRT_INTR_CH3_CRTL0_TX_WR(data) do {\
		ULONG v;\
		VIRT_INTR_CH3_CRTL0_RD(v);\
		v = ((v & VIRT_INTR_CH3_CRTL0_TX_WR_MASK) | ((data & VIRT_INTR_CH3_CRTL0_TX_MASK)<<0));\
		VIRT_INTR_CH3_CRTL0_WR(v);\
} while (0)

#define VIRT_INTR_CH3_CRTL0_TX_RD(data) do {\
		VIRT_INTR_CH3_CRTL0_RD(data);\
		data = ((data >> 0) & VIRT_INTR_CH3_CRTL0_TX_MASK);\
} while (0)

/*#define  VIRT_INTR_CH3_CRTL0_RX_MASK (ULONG)(~(~0<<(1))) << (1)))*/

#define VIRT_INTR_CH3_CRTL0_RX_MASK (ULONG)(0x1)

/*#define VIRT_INTR_CH3_CRTL0_RX_WR_MASK (unsigned long)(~((~(~0 << (1))) << (1)))*/

#define VIRT_INTR_CH3_CRTL0_RX_WR_MASK (ULONG)(0xfffffffd)

#define VIRT_INTR_CH3_CRTL0_RX_WR(data) do {\
		ULONG v;\
		VIRT_INTR_CH3_CRTL0_RD(v);\
		v = ((v & VIRT_INTR_CH3_CRTL0_RX_WR_MASK) | ((data & VIRT_INTR_CH3_CRTL0_RX_MASK)<<1));\
		VIRT_INTR_CH3_CRTL0_WR(v);\
} while (0)

#define VIRT_INTR_CH3_CRTL0_RX_RD(data) do {\
		VIRT_INTR_CH3_CRTL0_RD(data);\
		data = ((data >> 1) & VIRT_INTR_CH3_CRTL0_RX_MASK);\
} while (0)


#define VIRT_INTR_CH0_STAT_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x8604))

#define VIRT_INTR_CH0_STAT_WR(data) do {\
		iowrite32(data, (void *)VIRT_INTR_CH0_STAT_OFFSET);\
} while (0)

#define VIRT_INTR_CH0_STAT_RD(data) do {\
		(data) = ioread32((void *)VIRT_INTR_CH0_STAT_OFFSET);\
} while (0)


#define VIRT_INTR_CH1_STAT_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x860c))

#define VIRT_INTR_CH1_STAT_WR(data) do {\
		iowrite32(data, (void *)VIRT_INTR_CH1_STAT_OFFSET);\
} while (0)

#define VIRT_INTR_CH1_STAT_RD(data) do {\
		(data) = ioread32((void *)VIRT_INTR_CH1_STAT_OFFSET);\
} while (0)

#define VIRT_INTR_CH2_STAT_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x8614))

#define VIRT_INTR_CH2_STAT_WR(data) do {\
		iowrite32(data, (void *)VIRT_INTR_CH2_STAT_OFFSET);\
} while (0)

#define VIRT_INTR_CH2_STAT_RD(data) do {\
		(data) = ioread32((void *)VIRT_INTR_CH2_STAT_OFFSET);\
} while (0)

#define VIRT_INTR_CH3_STAT_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x861c))

#define VIRT_INTR_CH3_STAT_WR(data) do {\
		iowrite32(data, (void *)VIRT_INTR_CH3_STAT_OFFSET);\
} while (0)

#define VIRT_INTR_CH3_STAT_RD(data) do {\
		(data) = ioread32((void *)VIRT_INTR_CH3_STAT_OFFSET);\
} while (0)


#define VIRT_INTR_CH_CRTL_TX_WR_MASK (ULONG)(1 << 0)
#define VIRT_INTR_CH_CRTL_RX_WR_MASK (ULONG)(1 << 1)

#define VIRT_INTR_CH_CRTL_WR(chan, data) do {\
		iowrite32(data, (void *)(volatile ULONG*)(BASE_ADDRESS + 0x8600 + (chan * 8)));\
} while (0)

#define VIRT_INTR_CH_CRTL_RD(chan, data) do {\
		(data) = ioread32((void *)(volatile ULONG*)(BASE_ADDRESS+0x8600 + (chan * 8)));\
} while (0)

#define VIRT_INTR_CH_STAT_WR(chan, data) do {\
		iowrite32(data, (void *)(volatile ULONG*)(BASE_ADDRESS+0x8604 + (chan * 8)));\
} while (0)

#define VIRT_INTR_CH_STAT_RD(chan, data) do {\
		(data) = ioread32((void *)(volatile ULONG*)(BASE_ADDRESS+0x8604 + (chan * 8)));\
} while (0)


/* pad related regs */
#define PAD_CRTL_OFFSET ((volatile ULONG*)(BASE_ADDRESS + 0x8800))
#define PAD_CRTL_WR(data) do {\
		iowrite32(data, (void *)PAD_CRTL_OFFSET);\
} while (0)

#define PAD_CRTL_RD(data) do {\
		(data) = ioread32((void *)PAD_CRTL_OFFSET);\
} while (0)

#define PAD_CRTL_E_INPUT_OR_E_PWRD_MASK BIT(31)

#define PAD_CRTL_E_INPUT_OR_E_PWRD_WR(data) do {\
		ULONG v;\
		PAD_CRTL_RD(v);\
		v = ((v & ~PAD_CRTL_E_INPUT_OR_E_PWRD_MASK) | \
			((data & 1) << 31));\
		PAD_CRTL_WR(v);\
} while (0)

#define PAD_AUTO_CAL_CFG_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x8804))

#define PAD_AUTO_CAL_CFG_WR(data) do {\
		iowrite32(data, (void *)PAD_AUTO_CAL_CFG_OFFSET);\
} while (0)

#define PAD_AUTO_CAL_CFG_RD(data) do {\
		(data) = ioread32((void *)PAD_AUTO_CAL_CFG_OFFSET);\
} while (0)

#define PAD_AUTO_CAL_CFG_START_MASK BIT(31)
#define PAD_AUTO_CAL_CFG_ENABLE_MASK BIT(29)

#define PAD_AUTO_CAL_STAT_OFFSET ((volatile ULONG *)(BASE_ADDRESS + 0x880c))

#define PAD_AUTO_CAL_STAT_WR(data) do {\
		iowrite32(data, (void *)PAD_AUTO_CAL_STAT_OFFSET);\
} while (0)

#define PAD_AUTO_CAL_STAT_RD(data) do {\
		(data) = ioread32((void *)PAD_AUTO_CAL_STAT_OFFSET);\
} while (0)

#define PAD_AUTO_CAL_STAT_ACTIVE_MASK BIT(31)

#endif
