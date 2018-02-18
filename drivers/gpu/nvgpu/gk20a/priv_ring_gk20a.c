/*
 * GK20A priv ring
 *
 * Copyright (c) 2011-2016, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/delay.h>	/* for mdelay */

#include "gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_pri_ringmaster_gk20a.h"
#include "hw_pri_ringstation_sys_gk20a.h"

void gk20a_reset_priv_ring(struct gk20a *g)
{
	if (tegra_platform_is_linsim())
		return;

	if (g->ops.clock_gating.slcg_priring_load_gating_prod)
		g->ops.clock_gating.slcg_priring_load_gating_prod(g,
				g->slcg_enabled);

	gk20a_writel(g,pri_ringmaster_command_r(),
			0x4);

	gk20a_writel(g, pri_ringstation_sys_decode_config_r(),
			0x2);

	gk20a_readl(g, pri_ringstation_sys_decode_config_r());

}

void gk20a_priv_ring_isr(struct gk20a *g)
{
	u32 status0, status1;
	u32 cmd;
	s32 retry = 100;

	if (tegra_platform_is_linsim())
		return;

	status0 = gk20a_readl(g, pri_ringmaster_intr_status0_r());
	status1 = gk20a_readl(g, pri_ringmaster_intr_status1_r());

	gk20a_dbg(gpu_dbg_intr, "ringmaster intr status0: 0x%08x,"
		"status1: 0x%08x", status0, status1);

	if (status0 & (0x1 | 0x2 | 0x4)) {
		gk20a_reset_priv_ring(g);
	}

	if (status0 & 0x100) {
		gk20a_dbg(gpu_dbg_intr, "SYS write error. ADR %08x WRDAT %08x INFO %08x, CODE %08x",
			gk20a_readl(g, 0x122120), gk20a_readl(g, 0x122124), gk20a_readl(g, 0x122128),
			gk20a_readl(g, 0x12212c));
	}

	if (status1 & 0x1) {
		gk20a_dbg(gpu_dbg_intr, "GPC write error. ADR %08x WRDAT %08x INFO %08x, CODE %08x",
			gk20a_readl(g, 0x128120), gk20a_readl(g, 0x128124), gk20a_readl(g, 0x128128),
			gk20a_readl(g, 0x12812c));
	}

	cmd = gk20a_readl(g, pri_ringmaster_command_r());
	cmd = set_field(cmd, pri_ringmaster_command_cmd_m(),
		pri_ringmaster_command_cmd_ack_interrupt_f());
	gk20a_writel(g, pri_ringmaster_command_r(), cmd);

	do {
		cmd = pri_ringmaster_command_cmd_v(
			gk20a_readl(g, pri_ringmaster_command_r()));
		usleep_range(20, 40);
	} while (cmd != pri_ringmaster_command_cmd_no_cmd_v() && --retry);

	if (retry <= 0)
		gk20a_warn(dev_from_gk20a(g),
			"priv ringmaster cmd ack too many retries");

	status0 = gk20a_readl(g, pri_ringmaster_intr_status0_r());
	status1 = gk20a_readl(g, pri_ringmaster_intr_status1_r());

	gk20a_dbg_info("ringmaster intr status0: 0x%08x,"
		" status1: 0x%08x", status0, status1);
}

