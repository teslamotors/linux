/*
 * Copyright (c) 2015-2017, NVIDIA CORPORATION. All rights reserved.
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
#ifndef _M_TTCAN_LINUX_H
#define  _M_TTCAN_LINUX_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/can/dev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>

#include <asm/io.h>

#define MTTCAN_RX_FIFO_INTR     (0xFF)
#define MTTCAN_RX_HP_INTR       (0x1 << 8)
#define MTTCAN_TX_EV_FIFO_INTR  (0xF << 12)

#define MTTCAN_ERR_INTR       (0x1FF9 << 17)
#define MTTCAN_BUS_OFF        (1 << 25)
#define MTTCAN_ERR_WARN       (1 << 24)
#define MTTCAN_ERR_PASS       (1 << 23)

#define MTT_CAN_NAPI_WEIGHT	64
#define MTT_CAN_TX_OBJ_NUM	32
#define MTT_CAN_MAX_MRAM_ELEMS	9
#define MTT_MAX_TX_CONF		4
#define MTT_MAX_RX_CONF		3

struct can_gpio {
	int gpio;
	int active_low;
};

struct mttcan_priv {
	struct can_priv can;
	struct ttcan_controller *ttcan;
	struct delayed_work can_work;
	struct napi_struct napi;
	struct net_device *dev;
	struct device *device;
	struct clk *hclk, *cclk;
	struct can_gpio gpio_can_en;
	struct can_gpio gpio_can_stb;
	void __iomem *regs;
	void __iomem *mres;
	void *std_shadow;
	void *xtd_shadow;
	void *tmc_shadow;
	u32 gfc_reg;
	u32 xidam_reg;
	u32 irq_flags;
	u32 irq_ttflags;
	u32 irqstatus;
	u32 tt_irqstatus;
	u32 instance;
	int tt_intrs;
	int tt_param[2];
	u32 mram_param[MTT_CAN_MAX_MRAM_ELEMS];
	u32 tx_conf[MTT_MAX_TX_CONF]; /*<txb, txq, txq_mode, txb_dsize>*/
	u32 rx_conf[MTT_MAX_RX_CONF]; /*<rxb_dsize, rxq0_dsize, rxq1_dsize>*/
	bool poll;
};

int mttcan_create_sys_files(struct device *dev);
void mttcan_delete_sys_files(struct device *dev);
#endif
