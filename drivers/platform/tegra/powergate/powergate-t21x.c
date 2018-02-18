/*
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/tegra-powergate.h>
#include <linux/tegra-soc.h>
#include <linux/platform/tegra/dvfs.h>
#include <linux/tegra_soctherm.h>
#include <trace/events/power.h>

#include "powergate-priv.h"
#include "powergate-ops-t1xx.h"

#define EMULATION_MC_FLUSH_TIMEOUT 100

enum mc_client {
	MC_CLIENT_AFI		= 0,
	MC_CLIENT_AVPC		= 1,
	MC_CLIENT_DC		= 2,
	MC_CLIENT_DCB		= 3,
	MC_CLIENT_HC		= 6,
	MC_CLIENT_HDA		= 7,
	MC_CLIENT_ISP2		= 8,
	MC_CLIENT_NVENC		= 11,
	MC_CLIENT_SATA		= 15,
	MC_CLIENT_VI		= 17,
	MC_CLIENT_VIC		= 18,
	MC_CLIENT_XUSB_HOST	= 19,
	MC_CLIENT_XUSB_DEV	= 20,
	MC_CLIENT_BPMP		= 21,
	MC_CLIENT_TSEC		= 22,
	MC_CLIENT_SDMMC1	= 29,
	MC_CLIENT_SDMMC2	= 30,
	MC_CLIENT_SDMMC3	= 31,
	MC_CLIENT_SDMMC4	= 32,
	MC_CLIENT_ISP2B		= 33,
	MC_CLIENT_GPU		= 34,
	MC_CLIENT_NVDEC		= 37,
	MC_CLIENT_APE		= 38,
	MC_CLIENT_SE		= 39,
	MC_CLIENT_NVJPG		= 40,
	MC_CLIENT_TSECB		= 45,
	MC_CLIENT_LAST		= -1,
};

struct tegra210_mc_client_info {
	enum mc_client	hot_reset_clients[MAX_HOTRESET_CLIENT_NUM];
};

static struct tegra210_mc_client_info tegra210_pg_mc_info[] = {
	[TEGRA_POWERGATE_CRAIL] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_VE] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_ISP2,
			[1] = MC_CLIENT_VI,
			[2] = MC_CLIENT_LAST,
		},
	},
#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
	[TEGRA_POWERGATE_PCIE] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_AFI,
			[1] = MC_CLIENT_LAST,
		},
	},
#endif
#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
	[TEGRA_POWERGATE_SATA] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_SATA,
			[1] = MC_CLIENT_LAST,
		},
	},
#endif
	[TEGRA_POWERGATE_NVENC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_NVENC,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_SOR] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_DISA] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_DC,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_DISB] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_DCB,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_XUSBA] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_XUSBB] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_XUSB_DEV,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_XUSBC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_XUSB_HOST,
			[1] = MC_CLIENT_LAST,
		},
	},
#ifdef CONFIG_ARCH_TEGRA_VIC
	[TEGRA_POWERGATE_VIC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_VIC,
			[1] = MC_CLIENT_LAST,
		},
	},
#endif
	[TEGRA_POWERGATE_NVDEC] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_NVDEC,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_NVJPG] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_NVJPG,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_APE] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_APE,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_VE2] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_ISP2B,
			[1] = MC_CLIENT_LAST,
		},
	},
	[TEGRA_POWERGATE_GPU] = {
		.hot_reset_clients = {
			[0] = MC_CLIENT_GPU,
			[1] = MC_CLIENT_LAST,
		},
	},
};

static struct powergate_partition_info tegra210_pg_partition_info[] = {
	[TEGRA_POWERGATE_VE] = {
		.name = "ve",
		.clk_info = {
			[0] = { .clk_name = "ispa", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "vi", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "csi", .clk_type = CLK_AND_RST },
			[3] = { .clk_name = "vii2c", .clk_type = CLK_AND_RST },
			[4] = { .clk_name = "cilab", .clk_type = CLK_ONLY },
			[5] = { .clk_name = "cilcd", .clk_type = CLK_ONLY },
			[6] = { .clk_name = "cile", .clk_type = CLK_ONLY },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "host1x" },
			[5] = { .clk_name = "vi_slcg_ovr" },
			[6] = { .clk_name = "ispa_slcg_ovr" },
		},
	},
#ifdef CONFIG_ARCH_TEGRA_HAS_PCIE
	[TEGRA_POWERGATE_PCIE] = {
		.name = "pcie",
		.clk_info = {
			[0] = { .clk_name = "afi", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "pcie", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "pciex", .clk_type = RST_ONLY },
		},
		.skip_reset = true,
	},
#endif
#ifdef CONFIG_ARCH_TEGRA_HAS_SATA
	[TEGRA_POWERGATE_SATA] = {
		.name = "sata",
		.disable_after_boot = true,
		.clk_info = {
			[0] = { .clk_name = "sata_oob", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "cml1", .clk_type = CLK_ONLY },
			[2] = { .clk_name = "sata_cold", .clk_type = RST_ONLY },
			[3] = { .clk_name = "sata_aux", .clk_type = CLK_ONLY },
			[4] = { .clk_name = "sata", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "sata_slcg_ovr_fpci" },
			[5] = { .clk_name = "sata_slcg_ovr_ipfs" },
			[6] = { .clk_name = "sata_slcg_ovr" },
		},
	},
#endif
	[TEGRA_POWERGATE_NVENC] = {
		.name = "nvenc",
		.clk_info = {
			[0] = { .clk_name = "msenc.cbus", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "msenc_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_SOR] = {
		.name = "sor",
		.clk_info = {
			[0] = { .clk_name = "sor0", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "dsia", .clk_type = CLK_AND_RST },
			[2] = { .clk_name = "dsib", .clk_type = CLK_AND_RST },
			[3] = { .clk_name = "sor1", .clk_type = CLK_AND_RST },
			[4] = { .clk_name = "mipi-cal", .clk_type = CLK_AND_RST },
			[5] = { .clk_name = "dpaux", .clk_type = CLK_ONLY },
			[6] = { .clk_name = "dpaux1", .clk_type = CLK_ONLY },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "hda2hdmi" },
			[5] = { .clk_name = "hda2codec_2x" },
			[6] = { .clk_name = "disp1" },
			[7] = { .clk_name = "disp2" },
			[8] = { .clk_name = "disp1_slcg_ovr" },
			[9] = { .clk_name = "disp2_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_DISA] = {
		.name = "disa",
		.clk_info = {
			[0] = { .clk_name = "disp1", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "la" },
			[5] = { .clk_name = "host1x" },
			[6] = { .clk_name = "disp1_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_DISB] = {
		.name = "disb",
		.disable_after_boot = true,
		.clk_info = {
			[0] = { .clk_name = "disp2", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "la" },
			[5] = { .clk_name = "host1x" },
			[6] = { .clk_name = "disp2_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_XUSBA] = {
		.name = "xusba",
		.clk_info = {
			[0] = { .clk_name = "xusb_ss", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "xusb_host" },
			[5] = { .clk_name = "xusb_dev" },
			[6] = { .clk_name = "xusb_host_slcg_ovr" },
			[7] = { .clk_name = "xusb_dev_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_XUSBB] = {
		.name = "xusbb",
		.clk_info = {
			[0] = { .clk_name = "xusb_dev", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "xusb_ss" },
			[5] = { .clk_name = "xusb_host" },
			[6] = { .clk_name = "xusb_host_slcg_ovr" },
			[7] = { .clk_name = "xusb_dev_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_XUSBC] = {
		.name = "xusbc",
		.clk_info = {
			[0] = { .clk_name = "xusb_host", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "xusb_ss" },
			[5] = { .clk_name = "xusb_dev" },
			[6] = { .clk_name = "xusb_dev_slcg_ovr" },
			[7] = { .clk_name = "xusb_host_slcg_ovr" },
		},
	},
#ifdef CONFIG_ARCH_TEGRA_VIC
	[TEGRA_POWERGATE_VIC] = {
		.name = "vic",
		.clk_info = {
			[0] = { .clk_name = "vic03.cbus", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "vic03_slcg_ovr" },
			[5] = { .clk_name = "host1x" },
		},
	},
#endif
	[TEGRA_POWERGATE_NVDEC] = {
		.name = "nvdec",
		.clk_info = {
			[0] = { .clk_name = "nvdec", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "nvdec_slcg_ovr" },
			[5] = { .clk_name = "nvjpg" },
			[6] = { .clk_name = "nvjpg_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_NVJPG] = {
		.name = "nvjpg",
		.clk_info = {
			[0] = { .clk_name = "nvjpg", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "nvjpg_slcg_ovr" },
			[5] = { .clk_name = "nvdec" },
			[6] = { .clk_name = "nvdec_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_APE] = {
		.name = "ape",
		.clk_info = {
			[0] = { .clk_name = "ape", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "adsp" },
			[5] = { .clk_name = "i2s0" },
			[6] = { .clk_name = "i2s1" },
			[7] = { .clk_name = "i2s2" },
			[8] = { .clk_name = "i2s3" },
			[9] = { .clk_name = "i2s4" },
			[10] = { .clk_name = "spdif_out" },
			[11] = { .clk_name = "d_audio" },
			[12] = { .clk_name = "ape_slcg_ovr" },
			[13] = { .clk_name = "adsp_slcg_ovr" },
			[14] = { .clk_name = "d_audio_slcg_ovr" },

		},
	},
	[TEGRA_POWERGATE_VE2] = {
		.name = "ve2",
		.clk_info = {
			[0] = { .clk_name = "ispb", .clk_type = CLK_AND_RST },
		},
		.slcg_info = {
			[0] = { .clk_name = "mc_capa" },
			[1] = { .clk_name = "mc_cbpa" },
			[2] = { .clk_name = "mc_ccpa" },
			[3] = { .clk_name = "mc_cdpa" },
			[4] = { .clk_name = "ispb_slcg_ovr" },
		},
	},
	[TEGRA_POWERGATE_GPU] = {
		.name = "gpu",
		.clk_info = {
			[0] = { .clk_name = "gpu_gate", .clk_type = CLK_AND_RST },
			[1] = { .clk_name = "gpu_ref", .clk_type = CLK_ONLY },
			[2] = { .clk_name = "pll_p_out5", .clk_type = CLK_ONLY },
		},
	},
};

struct mc_client_hotreset_reg {
	u32 control_reg;
	u32 status_reg;
};

static struct mc_client_hotreset_reg tegra210_mc_reg[] = {
	[0] = { .control_reg = 0x200, .status_reg = 0x204 },
	[1] = { .control_reg = 0x970, .status_reg = 0x974 },
};

#define PMC_GPU_RG_CONTROL		0x2d4

static DEFINE_SPINLOCK(tegra210_pg_lock);

static struct dvfs_rail *gpu_rail;

#define HOTRESET_READ_COUNTS		5

static bool tegra210_pg_hotreset_check(u32 status_reg, u32 *status)
{
	int i;
	u32 curr_status;
	u32 prev_status;
	unsigned long flags;

	spin_lock_irqsave(&tegra210_pg_lock, flags);
	prev_status = mc_read(status_reg);
	for (i = 0; i < HOTRESET_READ_COUNTS; i++) {
		curr_status = mc_read(status_reg);
		if (curr_status != prev_status) {
			spin_unlock_irqrestore(&tegra210_pg_lock, flags);
			return false;
		}
	}
	*status = curr_status;
	spin_unlock_irqrestore(&tegra210_pg_lock, flags);

	return true;
}

static int tegra210_pg_mc_flush(int id)
{
	u32 idx, rst_control, rst_status;
	u32 rst_control_reg, rst_status_reg;
	enum mc_client mc_client_bit;
	unsigned long flags;
	unsigned int timeout;
	bool ret;
	int reg_idx;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mc_client_bit = tegra210_pg_mc_info[id].hot_reset_clients[idx];
		if (mc_client_bit == MC_CLIENT_LAST)
			break;

		reg_idx = mc_client_bit / 32;
		mc_client_bit %= 32;
		rst_control_reg = tegra210_mc_reg[reg_idx].control_reg;
		rst_status_reg = tegra210_mc_reg[reg_idx].status_reg;

		spin_lock_irqsave(&tegra210_pg_lock, flags);
		rst_control = mc_read(rst_control_reg);
		rst_control |= (1 << mc_client_bit);
		mc_write(rst_control, rst_control_reg);
		spin_unlock_irqrestore(&tegra210_pg_lock, flags);

		timeout = 0;
		do {
			udelay(10);
			rst_status = 0;
			ret = tegra210_pg_hotreset_check(rst_status_reg,
								&rst_status);
			if ((timeout++ > EMULATION_MC_FLUSH_TIMEOUT) &&
				(tegra_platform_is_qt() ||
				tegra_platform_is_fpga())) {
				pr_warn("%s flush %d timeout\n", __func__, id);
				break;
			}
			if (!ret)
				continue;
		} while (!(rst_status & (1 << mc_client_bit)));
	}

	return 0;
}

static int tegra210_pg_mc_flush_done(int id)
{
	u32 idx, rst_control, rst_control_reg;
	enum mc_client mc_client_bit;
	unsigned long flags;
	int reg_idx;

	for (idx = 0; idx < MAX_HOTRESET_CLIENT_NUM; idx++) {
		mc_client_bit = tegra210_pg_mc_info[id].hot_reset_clients[idx];
		if (mc_client_bit == MC_CLIENT_LAST)
			break;

		reg_idx = mc_client_bit / 32;
		mc_client_bit %= 32;
		rst_control_reg = tegra210_mc_reg[reg_idx].control_reg;

		spin_lock_irqsave(&tegra210_pg_lock, flags);
		rst_control = mc_read(rst_control_reg);
		rst_control &= ~(1 << mc_client_bit);
		mc_write(rst_control, rst_control_reg);
		spin_unlock_irqrestore(&tegra210_pg_lock, flags);

	}
	wmb();

	return 0;
}

static const char *tegra210_pg_get_name(int id)
{
	return tegra210_pg_partition_info[id].name;
}

static int tegra210_pg_powergate(int id)
{
	struct powergate_partition_info *partition =
				&tegra210_pg_partition_info[id];
	int ret = 0;

	trace_powergate(__func__, tegra210_pg_get_name(id), id, 1, 0);
	mutex_lock(&partition->pg_mutex);

	if (--partition->refcount > 0)
		goto exit_unlock;

	if ((partition->refcount < 0) || !tegra_powergate_is_powered(id)) {
		WARN(1, "Partition %s already powergated, refcount and status mismatch\n",
		     partition->name);
		goto exit_unlock;
	}

	ret = tegra1xx_powergate(id, partition);

exit_unlock:
	mutex_unlock(&partition->pg_mutex);
	trace_powergate(__func__, tegra210_pg_get_name(id), id, 0, ret);
	return ret;
}

static int tegra210_pg_unpowergate(int id)
{
	struct powergate_partition_info *partition =
				&tegra210_pg_partition_info[id];
	int ret = 0;

	trace_powergate(__func__, tegra210_pg_get_name(id), id, 1, 0);
	mutex_lock(&partition->pg_mutex);

	if (partition->refcount++ > 0)
		goto exit_unlock;

	if (tegra_powergate_is_powered(id)) {
		WARN(1, "Partition %s is already unpowergated, refcount and status mismatch\n",
		     partition->name);
		goto exit_unlock;
	}

	ret = tegra1xx_unpowergate(id, partition);

exit_unlock:
	mutex_unlock(&partition->pg_mutex);
	trace_powergate(__func__, tegra210_pg_get_name(id), id, 0, ret);
	return ret;
}

static int tegra210_pg_gpu_powergate(int id)
{
	int ret = 0;
	struct powergate_partition_info *partition =
				&tegra210_pg_partition_info[id];

	trace_powergate(__func__, tegra210_pg_get_name(id), id, 1, 0);
	mutex_lock(&partition->pg_mutex);

	if (--partition->refcount > 0)
		goto exit_unlock;

	if (!tegra_powergate_is_powered(id)) {
		WARN(1, "GPU rail is already off, refcount and status mismatch\n");
		goto exit_unlock;
	}

	if (!partition->clk_info[0].clk_ptr)
		get_clk_info(partition);

	tegra_powergate_mc_flush(id);

	udelay(10);

	powergate_partition_assert_reset(partition);

	udelay(10);

	pmc_write(0x1, PMC_GPU_RG_CONTROL);
	pmc_read(PMC_GPU_RG_CONTROL);

	udelay(10);

	partition_clk_disable(partition);

	udelay(10);

	tegra_soctherm_gpu_tsens_invalidate(1);

	if (gpu_rail) {
		ret = tegra_dvfs_rail_power_down(gpu_rail);
		if (ret) {
			WARN(1, "Could not power down GPU rail\n");
			goto exit_unlock;
		}
	} else {
		pr_info("No GPU regulator?\n");
	}

exit_unlock:
	mutex_unlock(&partition->pg_mutex);
	trace_powergate(__func__, tegra210_pg_get_name(id), id, 0, ret);
	return ret;
}

static int tegra210_pg_gpu_unpowergate(int id)
{
	int ret = 0;
	bool first = false;
	struct powergate_partition_info *partition =
				&tegra210_pg_partition_info[id];

	trace_powergate(__func__, tegra210_pg_get_name(id), id, 1, 0);
	mutex_lock(&partition->pg_mutex);

	if (partition->refcount++ > 0)
		goto exit_unlock;

	if (!gpu_rail) {
		gpu_rail = tegra_dvfs_get_rail_by_name("vdd_gpu");
		if (IS_ERR_OR_NULL(gpu_rail)) {
			WARN(1, "No GPU regulator?\n");
			goto err_power;
		}
		first = true;
	}

	if (tegra_powergate_is_powered(id)) {
		WARN(1, "GPU rail is already on, refcount and status mismatch\n");
		goto exit_unlock;
	}

	ret = tegra_dvfs_rail_power_up(gpu_rail);
	if (ret) {
		WARN(1, "Could not turn on GPU rail\n");
		goto err_power;
	}

	tegra_soctherm_gpu_tsens_invalidate(0);

	if (!partition->clk_info[0].clk_ptr)
		get_clk_info(partition);

	if (!first) {
		ret = partition_clk_enable(partition);
		if (ret) {
			WARN(1, "Could not turn on partition clocks\n");
			goto err_clk_on;
		}
	}

	udelay(10);

	powergate_partition_assert_reset(partition);

	udelay(10);

	pmc_write(0, PMC_GPU_RG_CONTROL);
	pmc_read(PMC_GPU_RG_CONTROL);

	udelay(10);

	/*
	 * Make sure all clok branches into GPU, except reference clock are
	 * gated across resert de-assertion.
	 */
	tegra_clk_disable_unprepare(partition->clk_info[0].clk_ptr);
	powergate_partition_deassert_reset(partition);
	tegra_clk_prepare_enable(partition->clk_info[0].clk_ptr);

	/* Flush MC after boot/railgate/SC7 */
	tegra_powergate_mc_flush(id);

	udelay(10);

	tegra_powergate_mc_flush_done(id);

	udelay(10);

exit_unlock:
	mutex_unlock(&partition->pg_mutex);
	trace_powergate(__func__, tegra210_pg_get_name(id), id, 0, ret);
	return ret;

err_clk_on:
	powergate_module(id);
err_power:
	mutex_unlock(&partition->pg_mutex);

	trace_powergate(__func__, tegra210_pg_get_name(id), id, 0, ret);
	return ret;
}

static int tegra210_pg_powergate_sor(int id)
{
	int ret;

	ret = tegra210_pg_powergate(id);
	if (ret)
		return ret;

	ret = tegra_powergate_partition(TEGRA_POWERGATE_SOR);
	if (ret)
		return ret;

	return 0;
}

static int tegra210_pg_unpowergate_sor(int id)
{
	int ret;

	ret = tegra_unpowergate_partition(TEGRA_POWERGATE_SOR);
	if (ret)
		return ret;

	ret = tegra210_pg_unpowergate(id);
	if (ret) {
		tegra_powergate_partition(TEGRA_POWERGATE_SOR);
		return ret;
	}

	return 0;
}

static int tegra210_pg_nvdec_powergate(int id)
{
	tegra210_pg_powergate(TEGRA_POWERGATE_NVDEC);
	tegra_powergate_partition(TEGRA_POWERGATE_NVJPG);

	return 0;
}

static int tegra210_pg_nvdec_unpowergate(int id)
{
	tegra_unpowergate_partition(TEGRA_POWERGATE_NVJPG);
	tegra210_pg_unpowergate(TEGRA_POWERGATE_NVDEC);

	return 0;
}

static int tegra210_pg_powergate_partition(int id)
{
	int ret;

	switch (id) {
		case TEGRA_POWERGATE_GPU:
			ret = tegra210_pg_gpu_powergate(id);
			break;
		case TEGRA_POWERGATE_DISA:
		case TEGRA_POWERGATE_DISB:
		case TEGRA_POWERGATE_VE:
			ret = tegra210_pg_powergate_sor(id);
			break;
		case TEGRA_POWERGATE_NVDEC:
			ret = tegra210_pg_nvdec_powergate(id);
			break;
		default:
			ret = tegra210_pg_powergate(id);
	}

	return ret;
}

static int tegra210_pg_unpowergate_partition(int id)
{
	int ret;

	switch (id) {
		case TEGRA_POWERGATE_GPU:
			ret = tegra210_pg_gpu_unpowergate(id);
			break;
		case TEGRA_POWERGATE_DISA:
		case TEGRA_POWERGATE_DISB:
		case TEGRA_POWERGATE_VE:
			ret = tegra210_pg_unpowergate_sor(id);
			break;
		case TEGRA_POWERGATE_NVDEC:
			ret = tegra210_pg_nvdec_unpowergate(id);
			break;
		default:
			ret = tegra210_pg_unpowergate(id);
	}

	return ret;
}

static int tegra210_pg_powergate_clk_off(int id)
{
	int ret = 0;
	struct powergate_partition_info *partition =
				&tegra210_pg_partition_info[id];

	BUG_ON(TEGRA_IS_GPU_POWERGATE_ID(id));

	trace_powergate(__func__, tegra210_pg_get_name(id), id, 1, 0);
	mutex_lock(&partition->pg_mutex);

	if (--partition->refcount > 0)
		goto exit_unlock;

	if ((partition->refcount < 0) || !tegra_powergate_is_powered(id)) {
		WARN(1, "Partition %s already powergated, refcount and status mismatch\n",
		     partition->name);
		goto exit_unlock;
	}

	ret = tegra1xx_powergate_partition_with_clk_off(id,
			&tegra210_pg_partition_info[id]);
exit_unlock:
	mutex_unlock(&partition->pg_mutex);
	trace_powergate(__func__, tegra210_pg_get_name(id), id, 0, ret);

	return ret;
}

static int tegra210_pg_unpowergate_clk_on(int id)
{
	int ret = 0;
	struct powergate_partition_info *partition =
				&tegra210_pg_partition_info[id];

	BUG_ON(TEGRA_IS_GPU_POWERGATE_ID(id));
	trace_powergate(__func__, tegra210_pg_get_name(id), id, 1, 0);
	mutex_lock(&partition->pg_mutex);

	if (partition->refcount++ > 0)
		goto exit_unlock;

	ret = tegra1xx_unpowergate_partition_with_clk_on(id,
			&tegra210_pg_partition_info[id]);
exit_unlock:
	mutex_unlock(&partition->pg_mutex);
	trace_powergate(__func__, tegra210_pg_get_name(id), id, 0, ret);

	return ret;
}

static spinlock_t *tegra210_pg_get_lock(void)
{
	return &tegra210_pg_lock;
}

static bool tegra210_pg_skip(int id)
{
	switch (id) {
	case TEGRA_POWERGATE_GPU:
		return true;
	default:
		return false;
	}
}

static bool tegra210_pg_is_powered(int id)
{
	u32 status = 0;

	if (TEGRA_IS_GPU_POWERGATE_ID(id)) {
		if (gpu_rail)
			status = tegra_dvfs_is_rail_up(gpu_rail);
	} else {
		status = pmc_read(PWRGATE_STATUS) & (1 << id);
	}

	return !!status;
}

static int tegra210_pg_init_refcount(void)
{
	int i;

	for (i = 0; i < TEGRA_NUM_POWERGATE; i++) {
		if (tegra_powergate_is_powered(i))
			tegra210_pg_partition_info[i].refcount = 1;
		else
			tegra210_pg_partition_info[i].disable_after_boot = 0;

		mutex_init(&tegra210_pg_partition_info[i].pg_mutex);
	}

	/* SOR refcount depends on other units */
	tegra210_pg_partition_info[TEGRA_POWERGATE_SOR].refcount =
		(tegra_powergate_is_powered(TEGRA_POWERGATE_DISA) ? 1 : 0) +
		(tegra_powergate_is_powered(TEGRA_POWERGATE_DISB) ? 1 : 0) +
		(tegra_powergate_is_powered(TEGRA_POWERGATE_VE) ? 1 : 0);

	tegra_powergate_partition(TEGRA_POWERGATE_XUSBA);
	tegra_powergate_partition(TEGRA_POWERGATE_XUSBB);
	tegra_powergate_partition(TEGRA_POWERGATE_XUSBC);

	return 0;
}

static struct powergate_ops tegra210_pg_ops = {
	.soc_name = "tegra210",

	.num_powerdomains = TEGRA_NUM_POWERGATE,

	.get_powergate_lock = tegra210_pg_get_lock,
	.get_powergate_domain_name = tegra210_pg_get_name,

	.powergate_partition = tegra210_pg_powergate_partition,
	.unpowergate_partition = tegra210_pg_unpowergate_partition,

	.powergate_partition_with_clk_off = tegra210_pg_powergate_clk_off,
	.unpowergate_partition_with_clk_on = tegra210_pg_unpowergate_clk_on,

	.powergate_mc_flush = tegra210_pg_mc_flush,
	.powergate_mc_flush_done = tegra210_pg_mc_flush_done,

	.powergate_skip = tegra210_pg_skip,

	.powergate_is_powered = tegra210_pg_is_powered,

	.powergate_init_refcount = tegra210_pg_init_refcount,
};

struct powergate_ops *tegra210_powergate_init_chip_support(void)
{
	if (tegra_platform_is_linsim())
		return NULL;

	return &tegra210_pg_ops;
}

int slcg_register_notifier(int id, struct notifier_block *nb)
{
	struct powergate_partition_info *pg_info = &tegra210_pg_partition_info[id];

	if (!pg_info || !nb)
		return -EINVAL;

	return raw_notifier_chain_register(&pg_info->slcg_notifier, nb);
}
EXPORT_SYMBOL(slcg_register_notifier);

int slcg_unregister_notifier(int id, struct notifier_block *nb)
{
	struct powergate_partition_info *pg_info =
			&tegra210_pg_partition_info[id];

	if (!pg_info || !nb)
		return -EINVAL;

	return raw_notifier_chain_unregister(&pg_info->slcg_notifier, nb);
}
EXPORT_SYMBOL(slcg_unregister_notifier);

static int __init tegra210_disable_boot_partitions(void)
{
	int i;

	pr_info("Disable partitions left on by BL\n");
	for (i = 0; i < TEGRA_NUM_POWERGATE; i++)
		if (tegra210_pg_partition_info[i].disable_after_boot &&
			(i != TEGRA_POWERGATE_GPU)) {
			pr_info("    %s\n", tegra210_pg_partition_info[i].name);
			tegra_powergate_partition(i);
		}

	return 0;
}
late_initcall(tegra210_disable_boot_partitions);
