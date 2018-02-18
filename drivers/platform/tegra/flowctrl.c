/*
 * drivers/platform/tegra/flowctrl.c
 *
 * functions and macros to control the flowcontroller
 *
 * Copyright (c) 2010-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>

#include <asm/barrier.h>

#include <linux/platform/tegra/flowctrl.h>
#include "iomap.h"

static u8 flowctrl_offset_halt_cpu[] = {
	FLOW_CTRL_HALT_CPU0_EVENTS,
	FLOW_CTRL_HALT_CPU1_EVENTS,
	FLOW_CTRL_HALT_CPU1_EVENTS + 8,
	FLOW_CTRL_HALT_CPU1_EVENTS + 16,
};

static u8 flowctrl_offset_cpu_csr[] = {
	FLOW_CTRL_CPU0_CSR,
	FLOW_CTRL_CPU1_CSR,
	FLOW_CTRL_CPU1_CSR + 8,
	FLOW_CTRL_CPU1_CSR + 16,
};

static u8 flowctrl_offset_cc4_ctrl[] = {
	FLOW_CTRL_CC4_CORE0_CTRL,
	FLOW_CTRL_CC4_CORE0_CTRL + 4,
	FLOW_CTRL_CC4_CORE0_CTRL + 8,
	FLOW_CTRL_CC4_CORE0_CTRL + 12,
};

void flowctrl_update(u8 offset, u32 value)
{
	void __iomem *addr = IO_ADDRESS(TEGRA_FLOW_CTRL_BASE) + offset;

	writel_relaxed(value, addr);

	/* ensure the update has reached the flow controller */
	wmb();
	readl_relaxed(addr);
}

u32 flowctrl_readl(u8 offset)
{
	void __iomem *addr = IO_ADDRESS(TEGRA_FLOW_CTRL_BASE) + offset;
	return readl_relaxed(addr);
}

void flowctrl_write_cpu_csr(unsigned int cpuid, u32 value)
{
	return flowctrl_update(flowctrl_offset_cpu_csr[cpuid], value);
}

void flowctrl_write_cpu_halt(unsigned int cpuid, u32 value)
{
	return flowctrl_update(flowctrl_offset_halt_cpu[cpuid], value);
}

void flowctrl_write_cc4_ctrl(unsigned int cpuid, u32 value)
{
	return flowctrl_update(flowctrl_offset_cc4_ctrl[cpuid], value);
}
