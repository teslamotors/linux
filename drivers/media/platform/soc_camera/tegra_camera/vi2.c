/*
 * Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/regulator/consumer.h>
#include <linux/nvhost.h>

#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/tegra_v4l2_camera.h>

#include <mach/clk.h>
#include <mach/io_dpd.h>

#include "nvhost_syncpt.h"
#include "nvhost_acm.h"
#include "bus_client.h"
#include "t124/t124.h"
#include "t210/t210.h"
#include "common.h"

static int tpg_mode;
module_param(tpg_mode, int, 0644);

#define VI2_CAM_DRV_NAME				"vi"
#define VI2_CAM_CARD_NAME				"vi"
#define VI2_CAM_VERSION					KERNEL_VERSION(0, 0, 5)

#define TEGRA_SYNCPT_RETRY_COUNT			10

#define TEGRA_SYNCPT_CSI_WAIT_TIMEOUT                   200

/* VI registers */
#define TEGRA_VI_CFG_VI_INCR_SYNCPT			0x000
#if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) || \
	IS_ENABLED(CONFIG_ARCH_TEGRA_13x_SOC))
#define 	VI_MW_REQ_DONE				4
#define 	VI_MW_ACK_DONE				6
#define 	VI_FRAME_START				9
#define 	VI_LINE_START				11
#else
#define 	VI_LINE_START				4
#define 	VI_FRAME_START				5
#define 	VI_MW_REQ_DONE				6
#define 	VI_MW_ACK_DONE				7
#endif

#define TEGRA_VI_CFG_VI_INCR_SYNCPT_CNTRL		0x004
#define TEGRA_VI_CFG_VI_INCR_SYNCPT_ERROR		0x008
#define TEGRA_VI_CFG_CTXSW				0x020
#define TEGRA_VI_CFG_INTSTATUS				0x024
#define TEGRA_VI_CFG_PWM_CONTROL			0x038
#define TEGRA_VI_CFG_PWM_HIGH_PULSE			0x03c
#define TEGRA_VI_CFG_PWM_LOW_PULSE			0x040
#define TEGRA_VI_CFG_PWM_SELECT_PULSE_A			0x044
#define TEGRA_VI_CFG_PWM_SELECT_PULSE_B			0x048
#define TEGRA_VI_CFG_PWM_SELECT_PULSE_C			0x04c
#define TEGRA_VI_CFG_PWM_SELECT_PULSE_D			0x050
#define TEGRA_VI_CFG_VGP1				0x064
#define TEGRA_VI_CFG_VGP2				0x068
#define TEGRA_VI_CFG_VGP3				0x06c
#define TEGRA_VI_CFG_VGP4				0x070
#define TEGRA_VI_CFG_VGP5				0x074
#define TEGRA_VI_CFG_VGP6				0x078
#define TEGRA_VI_CFG_INTERRUPT_MASK			0x08c
#define TEGRA_VI_CFG_INTERRUPT_TYPE_SELECT		0x090
#define TEGRA_VI_CFG_INTERRUPT_POLARITY_SELECT		0x094
#define TEGRA_VI_CFG_INTERRUPT_STATUS			0x098
#define TEGRA_VI_CFG_VGP_SYNCPT_CONFIG			0x0ac
#define TEGRA_VI_CFG_VI_SW_RESET			0x0b4
#define TEGRA_VI_CFG_CG_CTRL				0x0b8
#define TEGRA_VI_CFG_VI_MCCIF_FIFOCTRL			0x0e4
#define TEGRA_VI_CFG_TIMEOUT_WCOAL_VI			0x0e8
#define TEGRA_VI_CFG_DVFS				0x0f0
#define TEGRA_VI_CFG_RESERVE				0x0f4
#define TEGRA_VI_CFG_RESERVE_1				0x0f8

/* CSI registers */
#define TEGRA_VI_CSI_0_BASE				0x100
#define TEGRA_VI_CSI_1_BASE				0x200
#define TEGRA_VI_CSI_2_BASE				0x300
#define TEGRA_VI_CSI_3_BASE				0x400
#define TEGRA_VI_CSI_4_BASE				0x500
#define TEGRA_VI_CSI_5_BASE				0x600

#define TEGRA_VI_CSI_SW_RESET				0x000
#define TEGRA_VI_CSI_SINGLE_SHOT			0x004
#define TEGRA_VI_CSI_SINGLE_SHOT_STATE_UPDATE		0x008
#define TEGRA_VI_CSI_IMAGE_DEF				0x00c
#define TEGRA_VI_CSI_RGB2Y_CTRL				0x010
#define TEGRA_VI_CSI_MEM_TILING				0x014
#define TEGRA_VI_CSI_IMAGE_SIZE				0x018
#define TEGRA_VI_CSI_IMAGE_SIZE_WC			0x01c
#define TEGRA_VI_CSI_IMAGE_DT				0x020
#define TEGRA_VI_CSI_SURFACE0_OFFSET_MSB		0x024
#define TEGRA_VI_CSI_SURFACE0_OFFSET_LSB		0x028
#define TEGRA_VI_CSI_SURFACE1_OFFSET_MSB		0x02c
#define TEGRA_VI_CSI_SURFACE1_OFFSET_LSB		0x030
#define TEGRA_VI_CSI_SURFACE2_OFFSET_MSB		0x034
#define TEGRA_VI_CSI_SURFACE2_OFFSET_LSB		0x038
#define TEGRA_VI_CSI_SURFACE0_BF_OFFSET_MSB		0x03c
#define TEGRA_VI_CSI_SURFACE0_BF_OFFSET_LSB		0x040
#define TEGRA_VI_CSI_SURFACE1_BF_OFFSET_MSB		0x044
#define TEGRA_VI_CSI_SURFACE1_BF_OFFSET_LSB		0x048
#define TEGRA_VI_CSI_SURFACE2_BF_OFFSET_MSB		0x04c
#define TEGRA_VI_CSI_SURFACE2_BF_OFFSET_LSB		0x050
#define TEGRA_VI_CSI_SURFACE0_STRIDE			0x054
#define TEGRA_VI_CSI_SURFACE1_STRIDE			0x058
#define TEGRA_VI_CSI_SURFACE2_STRIDE			0x05c
#define TEGRA_VI_CSI_SURFACE_HEIGHT0			0x060
#define TEGRA_VI_CSI_ISPINTF_CONFIG			0x064
#define TEGRA_VI_CSI_ERROR_STATUS			0x084
#define TEGRA_VI_CSI_ERROR_INT_MASK			0x088
#define TEGRA_VI_CSI_WD_CTRL				0x08c
#define TEGRA_VI_CSI_WD_PERIOD				0x090

#define TEGRA_CSI_CSI_CAP_CIL				0x808
#define TEGRA_CSI_CSI_CAP_CSI				0x818
#define TEGRA_CSI_CSI_CAP_PP				0x828

/* CSI Pixel Parser registers */
#define TEGRA_CSI_PIXEL_PARSER_0_BASE			0x0838
#define TEGRA_CSI_PIXEL_PARSER_1_BASE			0x086c
#define TEGRA_CSI_PIXEL_PARSER_2_BASE			0x1038
#define TEGRA_CSI_PIXEL_PARSER_3_BASE			0x106c
#define TEGRA_CSI_PIXEL_PARSER_4_BASE			0x1838
#define TEGRA_CSI_PIXEL_PARSER_5_BASE			0x186c

#define TEGRA_CSI_INPUT_STREAM_CONTROL			0x000
#define TEGRA_CSI_PIXEL_STREAM_CONTROL0			0x004
#define TEGRA_CSI_PIXEL_STREAM_CONTROL1			0x008
#define TEGRA_CSI_PIXEL_STREAM_GAP			0x00c
#define TEGRA_CSI_PIXEL_STREAM_PP_COMMAND		0x010
#define TEGRA_CSI_PIXEL_STREAM_EXPECTED_FRAME		0x014
#define TEGRA_CSI_PIXEL_PARSER_INTERRUPT_MASK		0x018
#define TEGRA_CSI_PIXEL_PARSER_STATUS			0x01c
#define TEGRA_CSI_CSI_SW_SENSOR_RESET			0x020

/* CSI PHY registers */
#define TEGRA_CSI_CIL_PHY_0_BASE			0x0908
#define TEGRA_CSI_CIL_PHY_1_BASE			0x1108
#define TEGRA_CSI_CIL_PHY_2_BASE			0x1908
#define TEGRA_CSI_PHY_CIL_COMMAND			0x0908

/* CSI CIL registers */
#define TEGRA_CSI_CIL_0_BASE				0x092c
#define TEGRA_CSI_CIL_1_BASE				0x0960
#define TEGRA_CSI_CIL_2_BASE				0x112c
#define TEGRA_CSI_CIL_3_BASE				0x1160
#define TEGRA_CSI_CIL_4_BASE				0x192c
#define TEGRA_CSI_CIL_5_BASE				0x1960

#define TEGRA_CSI_CIL_PAD_CONFIG0			0x000
#define TEGRA_CSI_CIL_PAD_CONFIG1			0x004
#define TEGRA_CSI_CIL_PHY_CONTROL			0x008
#define TEGRA_CSI_CIL_INTERRUPT_MASK			0x00c
#define TEGRA_CSI_CIL_STATUS				0x010
#define TEGRA_CSI_CILX_STATUS				0x014
#define TEGRA_CSI_CIL_ESCAPE_MODE_COMMAND		0x018
#define TEGRA_CSI_CIL_ESCAPE_MODE_DATA			0x01c
#define TEGRA_CSI_CIL_SW_SENSOR_RESET			0x020

/* CSI Pattern Generator registers */
#if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) || \
	    IS_ENABLED(CONFIG_ARCH_TEGRA_13x_SOC))
#define TEGRA_CSI_PATTERN_GENERATOR_0_BASE		0xa68
#define TEGRA_CSI_PATTERN_GENERATOR_1_BASE		0xa9c
#else
#define TEGRA_CSI_PATTERN_GENERATOR_0_BASE		0x09c4
#define TEGRA_CSI_PATTERN_GENERATOR_1_BASE		0x09f8
#define TEGRA_CSI_PATTERN_GENERATOR_2_BASE		0x11c4
#define TEGRA_CSI_PATTERN_GENERATOR_3_BASE		0x11f8
#define TEGRA_CSI_PATTERN_GENERATOR_4_BASE		0x19c4
#define TEGRA_CSI_PATTERN_GENERATOR_5_BASE		0x19f8
#endif

#define TEGRA_CSI_PATTERN_GENERATOR_CTRL		0x000
#define TEGRA_CSI_PG_BLANK				0x004
#define TEGRA_CSI_PG_PHASE				0x008
#define TEGRA_CSI_PG_RED_FREQ				0x00c
#define TEGRA_CSI_PG_RED_FREQ_RATE			0x010
#define TEGRA_CSI_PG_GREEN_FREQ				0x014
#define TEGRA_CSI_PG_GREEN_FREQ_RATE			0x018
#define TEGRA_CSI_PG_BLUE_FREQ				0x01c
#define TEGRA_CSI_PG_BLUE_FREQ_RATE			0x020
#define TEGRA_CSI_PG_AOHDR				0x024

#define TEGRA_CSI_DPCM_CTRL_A				0xad0
#define TEGRA_CSI_DPCM_CTRL_B				0xad4
#define TEGRA_CSI_STALL_COUNTER				0xae8
#define TEGRA_CSI_CSI_READONLY_STATUS			0xaec
#define TEGRA_CSI_CSI_SW_STATUS_RESET			0xaf0
#define TEGRA_CSI_CLKEN_OVERRIDE			0xaf4
#define TEGRA_CSI_DEBUG_CONTROL				0xaf8
#define TEGRA_CSI_DEBUG_COUNTER_0			0xafc
#define TEGRA_CSI_DEBUG_COUNTER_1			0xb00
#define TEGRA_CSI_DEBUG_COUNTER_2			0xb04

/* These go into the TEGRA_VI_CSI_n_IMAGE_DEF registers bits 23:16 */
#define TEGRA_IMAGE_FORMAT_T_L8				16
#define TEGRA_IMAGE_FORMAT_T_R16_I			32
#define TEGRA_IMAGE_FORMAT_T_B5G6R5			33
#define TEGRA_IMAGE_FORMAT_T_R5G6B5			34
#define TEGRA_IMAGE_FORMAT_T_A1B5G5R5			35
#define TEGRA_IMAGE_FORMAT_T_A1R5G5B5			36
#define TEGRA_IMAGE_FORMAT_T_B5G5R5A1			37
#define TEGRA_IMAGE_FORMAT_T_R5G5B5A1			38
#define TEGRA_IMAGE_FORMAT_T_A4B4G4R4			39
#define TEGRA_IMAGE_FORMAT_T_A4R4G4B4			40
#define TEGRA_IMAGE_FORMAT_T_B4G4R4A4			41
#define TEGRA_IMAGE_FORMAT_T_R4G4B4A4			42
#define TEGRA_IMAGE_FORMAT_T_A8B8G8R8			64
#define TEGRA_IMAGE_FORMAT_T_A8R8G8B8			65
#define TEGRA_IMAGE_FORMAT_T_B8G8R8A8			66
#define TEGRA_IMAGE_FORMAT_T_R8G8B8A8			67
#define TEGRA_IMAGE_FORMAT_T_A2B10G10R10		68
#define TEGRA_IMAGE_FORMAT_T_A2R10G10B10		69
#define TEGRA_IMAGE_FORMAT_T_B10G10R10A2		70
#define TEGRA_IMAGE_FORMAT_T_R10G10B10A2		71
#define TEGRA_IMAGE_FORMAT_T_A8Y8U8V8			193
#define TEGRA_IMAGE_FORMAT_T_V8U8Y8A8			194
#define TEGRA_IMAGE_FORMAT_T_A2Y10U10V10		197
#define TEGRA_IMAGE_FORMAT_T_V10U10Y10A2		198
#define TEGRA_IMAGE_FORMAT_T_Y8_U8__Y8_V8		200
#define TEGRA_IMAGE_FORMAT_T_Y8_V8__Y8_U8		201
#define TEGRA_IMAGE_FORMAT_T_U8_Y8__V8_Y8		202
#define TEGRA_IMAGE_FORMAT_T_T_V8_Y8__U8_Y8		203
#define TEGRA_IMAGE_FORMAT_T_T_Y8__U8__V8_N444		224
#define TEGRA_IMAGE_FORMAT_T_Y8__U8V8_N444		225
#define TEGRA_IMAGE_FORMAT_T_Y8__V8U8_N444		226
#define TEGRA_IMAGE_FORMAT_T_Y8__U8__V8_N422		227
#define TEGRA_IMAGE_FORMAT_T_Y8__U8V8_N422		228
#define TEGRA_IMAGE_FORMAT_T_Y8__V8U8_N422		229
#define TEGRA_IMAGE_FORMAT_T_Y8__U8__V8_N420		230
#define TEGRA_IMAGE_FORMAT_T_Y8__U8V8_N420		231
#define TEGRA_IMAGE_FORMAT_T_Y8__V8U8_N420		232
#define TEGRA_IMAGE_FORMAT_T_X2Lc10Lb10La10		233
#define TEGRA_IMAGE_FORMAT_T_A2R6R6R6R6R6		234

/* These go into the TEGRA_VI_CSI_n_CSI_IMAGE_DT registers bits 7:0 */
#define TEGRA_IMAGE_DT_YUV420_8				24
#define TEGRA_IMAGE_DT_YUV420_10			25
#define TEGRA_IMAGE_DT_YUV420CSPS_8			28
#define TEGRA_IMAGE_DT_YUV420CSPS_10			29
#define TEGRA_IMAGE_DT_YUV422_8				30
#define TEGRA_IMAGE_DT_YUV422_10			31
#define TEGRA_IMAGE_DT_RGB444				32
#define TEGRA_IMAGE_DT_RGB555				33
#define TEGRA_IMAGE_DT_RGB565				34
#define TEGRA_IMAGE_DT_RGB666				35
#define TEGRA_IMAGE_DT_RGB888				36
#define TEGRA_IMAGE_DT_RAW6				40
#define TEGRA_IMAGE_DT_RAW7				41
#define TEGRA_IMAGE_DT_RAW8				42
#define TEGRA_IMAGE_DT_RAW10				43
#define TEGRA_IMAGE_DT_RAW12				44
#define TEGRA_IMAGE_DT_RAW14				45

struct chan_regs_config {
	u32 csi_base;
	u32 csi_pp_base;
	u32 cil_base;
	u32 cil_phy_base;
	u32 tpg_base;
};

#define csi_regs_write(cam, chan, offset, val) \
		TC_VI_REG_WT(cam, chan->regs.csi_base + offset, val)
#define csi_regs_read(cam, chan, offset) \
		TC_VI_REG_RD(cam, chan->regs.csi_base + offset)
#define csi_pp_regs_write(cam, chan, offset, val) \
		TC_VI_REG_WT(cam, chan->regs.csi_pp_base + offset, val)
#define csi_pp_regs_read(cam, chan, offset) \
		TC_VI_REG_RD(cam, chan->regs.csi_pp_base + offset)
#define cil_regs_write(cam, chan, offset, val) \
		TC_VI_REG_WT(cam, chan->regs.cil_base + offset, val)
#define cil_regs_read(cam, chan, offset) \
		TC_VI_REG_RD(cam, chan->regs.cil_base + offset)
#define cil_phy_reg_write(cam, chan, val) \
		TC_VI_REG_WT(cam, chan->regs.cil_phy_base, val)
#define cil_phy_reg_read(cam, chan) TC_VI_REG_RD(cam, chan->regs.cil_phy_base)
#define tpg_regs_write(cam, chan, offset, val) \
		TC_VI_REG_WT(cam, chan->regs.tpg_base + offset, val)
#define tpg_regs_read(cam, chan, offset) \
		TC_VI_REG_RD(cam, chan->regs.tpg_base + offset)

static struct tegra_io_dpd vi2_io_dpd[] = {
	{
		.name			= "CSIA",
		.io_dpd_reg_index	= 0,
		.io_dpd_bit		= 0,
	},
	{
		.name			= "CSIB",
		.io_dpd_reg_index	= 0,
		.io_dpd_bit		= 1,
	},
	{
		.name			= "CSIC",
		.io_dpd_reg_index	= 1,
		.io_dpd_bit		= 10,
	},
	{
		.name			= "CSID",
		.io_dpd_reg_index	= 1,
		.io_dpd_bit		= 11,
	},
	{
		.name			= "CSIE",
		.io_dpd_reg_index	= 1,
		.io_dpd_bit		= 12,
	},
	{
		.name			= "CSIF",
		.io_dpd_reg_index	= 1,
		.io_dpd_bit		= 13,
	},
};

struct vi2_camera_clk {
	const char			*name;
	struct clk			*clk;
	u32				freq;
	int				use_devname;
};

#define MAX_CHAN_NUM	6

struct vi2_channel {
	struct vi2_camera		*vi2_cam;

	/* syncpt ids */
	u32				syncpt_id;
	u32				syncpt_thresh;

	struct tegra_io_dpd		*dpd;
	struct vi2_camera_clk		*clks;
	int				num_clks;

	struct chan_regs_config		regs;

	struct list_head		capture;
	spinlock_t			start_lock;
	struct list_head		done;
	spinlock_t			done_lock;

	struct task_struct		*kthread_capture_start;
	wait_queue_head_t		start_wait;
	struct task_struct		*kthread_capture_done;
	wait_queue_head_t		done_wait;

	int				port;
	int				lanes;
	s32				bytes_per_line;
	int				fourcc;
	int				code;
	int				surface;
	int				width;
	int				height;

	int				sequence;
	int				sof;
};

struct vi2_camera {
	struct tegra_camera		cam;

	void __iomem			*reg_base;

	struct nvhost_device_data	*ndata;

	struct regulator		*reg;
	const char			*regulator_name;

	struct vi2_camera_clk		*clks;
	int				num_clks;

	struct vi2_channel		channels[MAX_CHAN_NUM];

		/* Test Pattern Generator mode */
	int				tpg_mode;
	struct vi2_camera_clk		*tpg_clk;

	int				enable_refcnt;
};

/* Clock settings for camera */
static struct vi2_camera_clk vi2_common_clks[] = {
	{
		.name = "vi",
		.freq = 408000000,
		.use_devname = 1,
	},
	{
		.name = "csi",
		.freq = 408000000,
		.use_devname = 1,
	},
	{
		.name = "isp",
		.freq = 0,
	},
	{
		.name = "csus",
		.freq = 0,
		.use_devname = 1,
	},
	{
		.name = "sclk",
		.freq = 80000000,
	},
	{
		.name = "emc",
		.freq = 300000000,
		.use_devname = 1,
	},
};

static struct vi2_camera_clk vi2_clks_tpg = {
	.name = "pll_d",
	.freq = 927000000,
};

static struct vi2_camera_clk vi2_clks_ab[] = {
	{
		.name = "vi_sensor",
		.freq = 24000000,
	},
	{
		.name = "cilab",
		.freq = 102000000,
		.use_devname = 1,
	},
};

static struct vi2_camera_clk vi2_clks_cd[] = {
	{
		.name = "vi_sensor2",
		.freq = 24000000,
	},
	{
		.name = "cilcd",
		.freq = 102000000,
		.use_devname = 1,
	},
};

static struct vi2_camera_clk vi2_clks_ef[] = {
	{
		.name = "cile",
		.freq = 102000000,
		.use_devname = 1,
	},
};

static const struct soc_mbus_pixelfmt vi2_tpg_format = {
	.fourcc			= V4L2_PIX_FMT_RGB32,
	.name			= "RGBA 8-8-8-8",
	.bits_per_sample	= 32,
	.packing		= SOC_MBUS_PACKING_NONE,
	.order			= SOC_MBUS_ORDER_LE,
};

#define MAX_DEVID_LENGTH	16

static void vi2_init_syncpts(struct vi2_channel *chan)
{
	struct vi2_camera *vi2_cam = chan->vi2_cam;
	chan->syncpt_id =
		nvhost_get_syncpt_client_managed(vi2_cam->cam.pdev, "vi");
}

static void vi2_free_syncpts(struct vi2_channel *chan)
{
	struct vi2_camera *vi2_cam = chan->vi2_cam;
	nvhost_syncpt_put_ref_ext(vi2_cam->cam.pdev, chan->syncpt_id);
}

static u32 vi2_syncpt_cond(u32 cond, int port)
{
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) ||
	    IS_ENABLED(CONFIG_ARCH_TEGRA_13x_SOC))
		return (cond + port * 1) << 8;
	else
		return (cond + port * 4) << 8;
}

static int vi2_clock_start(struct vi2_camera *vi2_cam,
			   struct vi2_camera_clk *clks, int num_clks)
{
	struct tegra_camera *cam = (struct tegra_camera *)vi2_cam;
	struct platform_device *pdev = cam->pdev;
	struct vi2_camera_clk *vi2_clk;
	int i;

	for (i = 0; i < num_clks; i++) {
		vi2_clk = &clks[i];

		if (vi2_clk->use_devname) {
			char devname[MAX_DEVID_LENGTH];
			snprintf(devname, MAX_DEVID_LENGTH,
				 "tegra_%s", dev_name(&pdev->dev));
			vi2_clk->clk = clk_get_sys(devname, vi2_clk->name);
		} else
			vi2_clk->clk = clk_get(&pdev->dev, vi2_clk->name);

		if (!IS_ERR_OR_NULL(vi2_clk->clk)) {
			clk_prepare_enable(vi2_clk->clk);
			if (vi2_clk->freq > 0)
				clk_set_rate(vi2_clk->clk, vi2_clk->freq);
		} else {
			dev_err(&pdev->dev, "Failed to get clock %s.\n",
				vi2_clk->name);
			return PTR_ERR(vi2_clk->clk);
		}
	}

	return 0;
}

static int vi2_common_clock_start(struct soc_camera_host *ici)
{
	struct tegra_camera *cam = ici->priv;
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;
	int ret;

	vi2_cam->num_clks = ARRAY_SIZE(vi2_common_clks);
	vi2_cam->clks = vi2_common_clks;
	ret = vi2_clock_start(vi2_cam, vi2_cam->clks, vi2_cam->num_clks);
	if (ret)
		return ret;

	if (vi2_cam->tpg_mode) {
		vi2_cam->tpg_clk = &vi2_clks_tpg;
		ret = vi2_clock_start(vi2_cam, vi2_cam->tpg_clk, 1);
		if (!ret) {
			tegra_clk_cfg_ex(vi2_cam->tpg_clk->clk,
					TEGRA_CLK_PLLD_CSI_OUT_ENB, 1);
			tegra_clk_cfg_ex(vi2_cam->tpg_clk->clk,
					TEGRA_CLK_PLLD_DSI_OUT_ENB, 1);
			tegra_clk_cfg_ex(vi2_cam->tpg_clk->clk,
					TEGRA_CLK_MIPI_CSI_OUT_ENB, 0);
		}
	}

	return ret;
}

static void vi2_clock_stop(struct vi2_camera_clk *clks, int num_clks)
{
	struct vi2_camera_clk *vi2_clk;
	int i;

	for (i = 0; i < num_clks; i++) {
		vi2_clk = &clks[i];
		if (!IS_ERR_OR_NULL(vi2_clk->clk)) {
			clk_disable_unprepare(vi2_clk->clk);
			clk_put(vi2_clk->clk);
		}
		vi2_clk->clk = NULL;
	}
}

static void vi2_common_clock_stop(struct soc_camera_host *ici)
{
	struct tegra_camera *cam = ici->priv;
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;

	vi2_clock_stop(vi2_cam->clks, vi2_cam->num_clks);

	if (vi2_cam->tpg_mode) {
		if (!IS_ERR(vi2_cam->tpg_clk->clk)) {
			tegra_clk_cfg_ex(vi2_cam->tpg_clk->clk,
					 TEGRA_CLK_MIPI_CSI_OUT_ENB, 1);
			tegra_clk_cfg_ex(vi2_cam->tpg_clk->clk,
					 TEGRA_CLK_PLLD_CSI_OUT_ENB, 0);
			tegra_clk_cfg_ex(vi2_cam->tpg_clk->clk,
					 TEGRA_CLK_PLLD_DSI_OUT_ENB, 0);
			clk_disable_unprepare(vi2_cam->tpg_clk->clk);
			clk_put(vi2_cam->tpg_clk->clk);
			vi2_cam->tpg_clk->clk = NULL;
		}
	}
}

static u32 vi2_cal_regs_base(u32 regs_base, int port)
{
	return regs_base + (port / 2 * 0x800) + (port & 1) * 0x34;
}

static int vi2_channel_capture_frame(struct vi2_channel *chan,
				     struct tegra_camera_buffer *buf);

static int vi2_channel_kthread_capture_start(void *data)
{
	struct vi2_channel *chan = data;
	struct tegra_camera_buffer *buf;

	set_freezable();

	while (1) {
		try_to_freeze();
		wait_event_interruptible(chan->start_wait,
				!list_empty(&chan->capture) ||
				kthread_should_stop());
		if (kthread_should_stop())
			break;

		spin_lock(&chan->start_lock);
		if (list_empty(&chan->capture)) {
			spin_unlock(&chan->start_lock);
			continue;
		}

		buf = list_entry(chan->capture.next, struct tegra_camera_buffer,
				 queue);
		list_del_init(&buf->queue);
		spin_unlock(&chan->start_lock);

		vi2_channel_capture_frame(chan, buf);
	}

	return 0;
}

static int vi2_channel_capture_done(struct vi2_channel *chan,
				    struct tegra_camera_buffer *buf);

static int vi2_channel_kthread_capture_done(void *data)
{
	struct vi2_channel *chan = data;
	struct tegra_camera_buffer *buf;

	set_freezable();

	while (1) {
		try_to_freeze();
		wait_event_interruptible(chan->done_wait,
				!list_empty(&chan->done) ||
				kthread_should_stop());
		if (kthread_should_stop() && list_empty(&chan->done))
			break;

		spin_lock(&chan->done_lock);
		if (list_empty(&chan->done)) {
			spin_unlock(&chan->done_lock);
			continue;
		}

		buf = list_entry(chan->done.next, struct tegra_camera_buffer,
				queue);
		list_del_init(&buf->queue);
		spin_unlock(&chan->done_lock);

		vi2_channel_capture_done(chan, buf);
	}

	return 0;
}

static s32 vi2_bytes_per_line(u32 width, const struct soc_mbus_pixelfmt *mf);

static int vi2_channel_init(struct vi2_camera *vi2_cam,
			    struct soc_camera_device *icd)
{
	int port = icd_to_port(icd);
	struct vi2_channel *chan = &vi2_cam->channels[port];
	struct chan_regs_config *regs = &chan->regs;

	INIT_LIST_HEAD(&chan->capture);
	INIT_LIST_HEAD(&chan->done);
	spin_lock_init(&chan->start_lock);
	spin_lock_init(&chan->done_lock);
	init_waitqueue_head(&chan->start_wait);
	init_waitqueue_head(&chan->done_wait);

	chan->dpd = &vi2_io_dpd[port];
	chan->vi2_cam = vi2_cam;
	chan->sequence = 0;
	chan->surface = 0;
	chan->sof = 1;
	chan->port = port;
	chan->lanes = icd_to_lanes(icd);

	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) ||
	    IS_ENABLED(CONFIG_ARCH_TEGRA_13x_SOC))
		/* Init and start channel related clocks */
		if (port == TEGRA_CAMERA_PORT_CSI_A) {
			chan->clks = vi2_clks_ab;
			chan->num_clks = ARRAY_SIZE(vi2_clks_ab);
		} else {
			chan->clks = vi2_clks_cd;
			chan->num_clks = ARRAY_SIZE(vi2_clks_cd);
		}
	else {
		/* Init and start channel related clocks */
		switch (port) {
		case TEGRA_CAMERA_PORT_CSI_A:
		case TEGRA_CAMERA_PORT_CSI_B:
			chan->clks = vi2_clks_ab;
			chan->num_clks = ARRAY_SIZE(vi2_clks_ab);
			break;
		case TEGRA_CAMERA_PORT_CSI_C:
		case TEGRA_CAMERA_PORT_CSI_D:
			chan->clks = vi2_clks_cd;
			chan->num_clks = ARRAY_SIZE(vi2_clks_cd);
			break;
		case TEGRA_CAMERA_PORT_CSI_E:
		case TEGRA_CAMERA_PORT_CSI_F:
			chan->clks = vi2_clks_ef;
			chan->num_clks = ARRAY_SIZE(vi2_clks_ef);
			break;
		default:
			return -EINVAL;
		}
	}

	vi2_clock_start(vi2_cam, chan->clks, chan->num_clks);

	tegra_io_dpd_disable(chan->dpd);

	/* Init syncpts */
	vi2_init_syncpts(chan);

	/* Init channel register base */
	regs->csi_base = TEGRA_VI_CSI_0_BASE + port * 0x100;
	regs->csi_pp_base = vi2_cal_regs_base(TEGRA_CSI_PIXEL_PARSER_0_BASE,
			    port);
	regs->cil_base = vi2_cal_regs_base(TEGRA_CSI_CIL_0_BASE, port);
	regs->cil_phy_base = TEGRA_CSI_CIL_PHY_0_BASE + port / 2 * 0x800;
	regs->tpg_base = vi2_cal_regs_base(TEGRA_CSI_PATTERN_GENERATOR_0_BASE,
			 port);

	/* Clean up status */
	cil_regs_write(vi2_cam, chan, TEGRA_CSI_CIL_STATUS, 0xFFFFFFFF);
	cil_regs_write(vi2_cam, chan, TEGRA_CSI_CILX_STATUS, 0xFFFFFFFF);
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_PARSER_STATUS,
			  0xFFFFFFFF);
	csi_regs_write(vi2_cam, chan, TEGRA_VI_CSI_ERROR_STATUS, 0xFFFFFFFF);

	return 0;
}

static void vi2_channel_deinit(struct vi2_camera *vi2_cam,
			       struct soc_camera_device *icd)
{
	int port = icd_to_port(icd);
	struct vi2_channel *chan = &vi2_cam->channels[port];

	/* free syncpts */
	vi2_free_syncpts(chan);
	chan->sequence = 0;
	chan->sof = 0;
	tegra_io_dpd_enable(chan->dpd);
	vi2_clock_stop(chan->clks, chan->num_clks);
}

static int vi2_camera_activate(struct vi2_camera *vi2_cam)
{
	int ret = 0;

	ret = nvhost_module_busy_ext(vi2_cam->cam.pdev);
	if (ret) {
		dev_err(&vi2_cam->cam.pdev->dev, "nvhost module is busy\n");
		goto exit;
	}

	/* Enable external power */
	if (vi2_cam->reg) {
		ret = regulator_enable(vi2_cam->reg);
		if (ret)
			dev_err(&vi2_cam->cam.pdev->dev,
				"enabling regulator failed\n");
	}

	/* Reset VI2/CSI2 when activating, no sepecial ops for deactiving  */
	/* T12_CG_2ND_LEVEL_EN */
	TC_VI_REG_WT(vi2_cam, TEGRA_VI_CFG_CG_CTRL, 1);
	TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CLKEN_OVERRIDE, 0x0);
	udelay(10);

	/* Unpowergate VE */
	tegra_unpowergate_partition(TEGRA_POWERGATE_VENC);

	return 0;

exit:
	return ret;
}

static int vi2_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;
	int ret;

	if (!vi2_cam->enable_refcnt) {
		ret = vi2_camera_activate(vi2_cam);
		if (ret)
			return ret;
	}
	vi2_channel_init(vi2_cam, icd);
	vi2_cam->enable_refcnt++;

	return 0;
}

static void vi2_camera_deactivate(struct vi2_camera *vi2_cam)
{
	/* Powergate VE */
	tegra_powergate_partition(TEGRA_POWERGATE_VENC);

	/* Disable external power */
	if (vi2_cam->reg)
		regulator_disable(vi2_cam->reg);

	nvhost_module_idle_ext(vi2_cam->cam.pdev);
}

static void vi2_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct tegra_camera *cam = ici->priv;
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;

	vi2_cam->enable_refcnt--;
	vi2_channel_deinit(vi2_cam, icd);
	if (!vi2_cam->enable_refcnt)
		vi2_camera_deactivate(vi2_cam);
}

static bool vi2_port_is_valid(int port)
{
	return (((port) >= TEGRA_CAMERA_PORT_CSI_A) &&
		((port) <= TEGRA_CAMERA_PORT_CSI_F));
}

static s32 vi2_bytes_per_line(u32 width, const struct soc_mbus_pixelfmt *mf)
{
	s32 bytes_per_line = soc_mbus_bytes_per_line(width, mf);

	if (bytes_per_line % 64)
		bytes_per_line = bytes_per_line + (64 - (bytes_per_line % 64));

	return bytes_per_line;
}

static bool vi2_ignore_subdev_fmt(struct tegra_camera *cam)
{
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;

	/* If we are in TPG mode, we ignore the subdev's supported formats. */
	return vi2_cam->tpg_mode;
}

static int vi2_get_formats(struct soc_camera_device *icd,
			   unsigned int idx,
			   struct soc_camera_format_xlate *xlate)
{
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct vi2_camera *vi2_cam = ici->priv;

	/*
	 * If we are not in TPG mode, then let the regular get_formats handler
	 * handle this.
	 */
	if (!vi2_cam->tpg_mode)
		return -EAGAIN;

	if (xlate) {
		xlate->host_fmt = &vi2_tpg_format;
		xlate->code = V4L2_MBUS_FMT_RGBA8888_4X8_LE;
	}

	return 1;
}

/*
 * vi2_try_mbus_fmt() and vi2_s_mbus_fmt(): Tegra's VI2 camera host controller
 * can only support widths of multiples of 4.  In addition, though it doesn't
 * affect these functions but is just an interesting point, Tegra's VI2 camera
 * host controller will only output lines to memory in multiples of 64 bytes.
 * an image width that results in a line stride that is not a multiple of 64
 * bytes will result in the line being padded in order to satisfy the multiple
 * of 64 line stride (see vi2_bytes_per_line()).
 */
static int vi2_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *mf)
{
	if (mf->width % 4) {
		int width;
		int ret;

		/* First, clamp our width to a multiple of 4. */
		mf->width = ALIGN(mf->width, 4);

		/*
		 * Remember this new width, and ask the subdev if this width
		 * is okay.
		 */
		width = mf->width;
		ret = v4l2_subdev_call(sd, video, try_mbus_fmt, mf);
		if (IS_ERR_VALUE(ret))
			return ret;

		/* If this new width is not okay with the subdev, we fail. */
		if (mf->width != width)
			return -EINVAL;
	}

	return 0;
}

static int vi2_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	if (mf->width % 4)
		return -EINVAL;

	return 0;
}

static void vi2_videobuf_queue(struct tegra_camera *cam,
			       struct soc_camera_device *icd,
			       struct tegra_camera_buffer *buf)
{
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;
	int port = icd_to_port(icd);
	struct vi2_channel *chan = &vi2_cam->channels[port];

	if (icd->current_fmt) {
		chan->fourcc = icd->current_fmt->host_fmt->fourcc;
		chan->code = icd->current_fmt->code;
		chan->bytes_per_line = vi2_bytes_per_line(icd->user_width,
				icd->current_fmt->host_fmt);
		chan->width = icd->user_width;
		chan->height = icd->user_height;
	}

	spin_lock(&chan->start_lock);
	list_add_tail(&buf->queue, &chan->capture);
	spin_unlock(&chan->start_lock);

	/* Wait up kthread for capture */
	wake_up_interruptible(&chan->start_wait);
}

static int vi2_start_streaming(struct tegra_camera *cam,
			       struct soc_camera_device *icd,
			       unsigned int count)
{
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;
	int port = icd_to_port(icd);
	struct vi2_channel *chan = &vi2_cam->channels[port];

	/* Start kthread to capture data to buffer */
	chan->kthread_capture_start = kthread_run(
					vi2_channel_kthread_capture_start, chan,
					"vi2_channel:%d_0", port);
	if (IS_ERR(chan->kthread_capture_start)) {
		dev_err(&vi2_cam->cam.pdev->dev,
			"failed to run kthread for capture start!\n");
		return PTR_ERR(chan->kthread_capture_start);
	}

	chan->kthread_capture_done = kthread_run(
					vi2_channel_kthread_capture_done, chan,
					"vi2_channel:%d_1", port);
	if (IS_ERR(chan->kthread_capture_done)) {
		dev_err(&vi2_cam->cam.pdev->dev,
			"failed to run kthread for capture done!\n");
		return PTR_ERR(chan->kthread_capture_done);
	}

	return 0;
}

static int vi2_stop_streaming(struct tegra_camera *cam,
			      struct soc_camera_device *icd)
{
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;
	int port = icd_to_port(icd);
	struct vi2_channel *chan = &vi2_cam->channels[port];

	/* Stop the kthread for capture */
	kthread_stop(chan->kthread_capture_start);
	chan->kthread_capture_start = NULL;
	kthread_stop(chan->kthread_capture_done);
	chan->kthread_capture_done = NULL;
 
	return 0;
}

#define TEGRA_CSI_CILA_PAD_CONFIG0	0x92c
#define TEGRA_CSI_CILB_PAD_CONFIG0	0x960
#define TEGRA_CSI_CILC_PAD_CONFIG0	0x994
#define TEGRA_CSI_CILD_PAD_CONFIG0	0x9c8
#define TEGRA_CSI_CILE_PAD_CONFIG0	0xa08

#define TEGRA_CSI_CSI_CIL_A_INTERRUPT_MASK	0x938
#define TEGRA_CSI_CSI_CIL_B_INTERRUPT_MASK	0x96c
#define TEGRA_CSI_CSI_CIL_C_INTERRUPT_MASK	0x9a0
#define TEGRA_CSI_CSI_CIL_D_INTERRUPT_MASK	0x9d4
#define TEGRA_CSI_CSI_CIL_E_INTERRUPT_MASK	0xa14

#define TEGRA_CSI_PHY_CILA_CONTROL0	0x934
#define TEGRA_CSI_PHY_CILB_CONTROL0	0x968
#define TEGRA_CSI_PHY_CILC_CONTROL0	0x99c
#define TEGRA_CSI_PHY_CILD_CONTROL0	0x9d0
#define TEGRA_CSI_PHY_CILE_CONTROL0	0xa10

static void vi2_capture_setup_cil_t124(struct vi2_camera *vi2_cam, int port)
{
	if (port == TEGRA_CAMERA_PORT_CSI_A) {
		/*
		 * PAD_CILA_PDVCLAMP 0, PAD_CILA_PDIO_CLK 0,
		 * PAD_CILA_PDIO 0, PAD_AB_BK_MODE 1
		 */
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CILA_PAD_CONFIG0, 0x10000);

		/* PAD_CILB_PDVCLAMP 0, PAD_CILB_PDIO_CLK 0, PAD_CILB_PDIO 0 */
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CILB_PAD_CONFIG0, 0x0);

		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CSI_CIL_A_INTERRUPT_MASK, 0x0);
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CSI_CIL_B_INTERRUPT_MASK, 0x0);

#ifdef DEBUG
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_DEBUG_CONTROL,
			     0x3 | (0x1 << 5) | (0x40 << 8));
#endif
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CILA_CONTROL0, 0x9);
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CILB_CONTROL0, 0x9);
	} else {
		/*
		 * PAD_CILC_PDVCLAMP 0, PAD_CILC_PDIO_CLK 0,
		 * PAD_CILC_PDIO 0, PAD_CD_BK_MODE 1
		 */
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CILC_PAD_CONFIG0, 0x10000);

		/* PAD_CILD_PDVCLAMP 0, PAD_CILD_PDIO_CLK 0, PAD_CILD_PDIO 0 */
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CILD_PAD_CONFIG0, 0x0);

		/* PAD_CILE_PDVCLAMP 0, PAD_CILE_PDIO_CLK 0, PAD_CILE_PDIO 0 */
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CILE_PAD_CONFIG0, 0x0);

		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CSI_CIL_C_INTERRUPT_MASK, 0x0);
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CSI_CIL_D_INTERRUPT_MASK, 0x0);
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_CSI_CIL_E_INTERRUPT_MASK, 0x0);
#ifdef DEBUG
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_DEBUG_CONTROL,
				0x5 | (0x1 << 5) | (0x50 << 8));
#endif
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CILC_CONTROL0, 0x9);
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CILD_CONTROL0, 0x9);
		TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CILE_CONTROL0, 0x9);
	}
}

static void vi2_capture_setup_cil_phy_t124(struct vi2_camera *vi2_cam,
					   int lanes, int port)
{
	u32 val;

	/* Shared register */
	val = TC_VI_REG_RD(vi2_cam, TEGRA_CSI_PHY_CIL_COMMAND);
	if (port == TEGRA_CAMERA_PORT_CSI_A) {
		if (lanes == 4)
			TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CIL_COMMAND,
					(val & 0xFFFF0000) | 0x0101);
		else
			TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CIL_COMMAND,
					(val & 0xFFFF0000) | 0x0201);
	} else {
		if (lanes == 4)
			TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CIL_COMMAND,
					(val & 0x0000FFFF) | 0x21010000);
		else if (lanes == 1)
			TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CIL_COMMAND,
					(val & 0x0000FFFF) | 0x12020000);
		else
			TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CIL_COMMAND,
					(val & 0x0000FFFF) | 0x22010000);
	}
}

static int vi2_channel_capture_setup(struct vi2_channel *chan)
{
	struct vi2_camera *vi2_cam = chan->vi2_cam;
	int port = chan->port;
	int lanes = chan->lanes;
	int width = chan->width;
	int height = chan->height;
	struct chan_regs_config *regs = &chan->regs;
	int format = 0, data_type = 0, image_size = 0;

	/* Skip VI2/CSI2 setup for second and later frame capture */
	if (!chan->sof)
		return 0;

	/* CIL PHY register setup */
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) ||
			IS_ENABLED(CONFIG_ARCH_TEGRA_13x_SOC))
		vi2_capture_setup_cil_t124(vi2_cam, port);
	else {
		if (port & 0x1) {
			cil_regs_write(vi2_cam, chan,
				       TEGRA_CSI_CIL_PAD_CONFIG0 - 0x34, 0x0);
			cil_regs_write(vi2_cam, chan, TEGRA_CSI_CIL_PAD_CONFIG0,
				       0x0);
		} else {
			cil_regs_write(vi2_cam, chan, TEGRA_CSI_CIL_PAD_CONFIG0,
				       0x10000);
			cil_regs_write(vi2_cam, chan,
				       TEGRA_CSI_CIL_PAD_CONFIG0 + 0x34,
				       0x0);
		}

		cil_regs_write(vi2_cam, chan, TEGRA_CSI_CIL_INTERRUPT_MASK,
			       0x0);
		cil_regs_write(vi2_cam, chan, TEGRA_CSI_CIL_PHY_CONTROL, 0xA);
		if (lanes == 4) {
			regs->cil_base = vi2_cal_regs_base(TEGRA_CSI_CIL_0_BASE,
							   port + 1);
			cil_regs_write(vi2_cam, chan, TEGRA_CSI_CIL_PAD_CONFIG0,
				       0x0);
			cil_regs_write(vi2_cam, chan,
				       TEGRA_CSI_CIL_INTERRUPT_MASK, 0x0);
			cil_regs_write(vi2_cam, chan, TEGRA_CSI_CIL_PHY_CONTROL,
				       0xA);
			regs->cil_base = vi2_cal_regs_base(TEGRA_CSI_CIL_0_BASE,
							   port);
		}
	}

	/* CSI pixel parser registers setup */
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_STREAM_PP_COMMAND,
			  0xf007);
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_PARSER_INTERRUPT_MASK,
			  0x0);
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_STREAM_CONTROL0,
			  0x280301f0 | (port & 0x1));
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_STREAM_PP_COMMAND,
			  0xf007);
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_STREAM_CONTROL1, 0x11);
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_STREAM_GAP, 0x140000);
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_STREAM_EXPECTED_FRAME,
			  0x0);
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_INPUT_STREAM_CONTROL,
			  0x3f0000 | (lanes - 1));

	/* CIL PHY register setup */
	if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) ||
	    IS_ENABLED(CONFIG_ARCH_TEGRA_13x_SOC))
		vi2_capture_setup_cil_phy_t124(vi2_cam, lanes, port);
	else {
		if (lanes == 4)
			cil_phy_reg_write(vi2_cam, chan, 0x0101);
		else {
			u32 val = cil_phy_reg_read(vi2_cam, chan);
			if (port & 0x1)
				val = (val & ~(0x100)) | (0x100);
			else
				val = (val & ~(0x1)) | (0x1);
			cil_phy_reg_write(vi2_cam, chan, val);
		}
	}

	if (vi2_cam->tpg_mode) {
		tpg_regs_write(vi2_cam, chan, TEGRA_CSI_PATTERN_GENERATOR_CTRL,
				((vi2_cam->tpg_mode - 1) << 2) | 0x1);
		tpg_regs_write(vi2_cam, chan, TEGRA_CSI_PG_PHASE, 0x0);
		tpg_regs_write(vi2_cam, chan, TEGRA_CSI_PG_RED_FREQ, 0x100010);
		tpg_regs_write(vi2_cam, chan, TEGRA_CSI_PG_RED_FREQ_RATE, 0x0);
		tpg_regs_write(vi2_cam, chan, TEGRA_CSI_PG_GREEN_FREQ,
			       0x100010);
		tpg_regs_write(vi2_cam, chan, TEGRA_CSI_PG_GREEN_FREQ_RATE,
			       0x0);
		tpg_regs_write(vi2_cam, chan, TEGRA_CSI_PG_BLUE_FREQ, 0x100010);
		tpg_regs_write(vi2_cam, chan, TEGRA_CSI_PG_BLUE_FREQ_RATE, 0x0);
		if (IS_ENABLED(CONFIG_ARCH_TEGRA_12x_SOC) ||
		    IS_ENABLED(CONFIG_ARCH_TEGRA_13x_SOC))
			TC_VI_REG_WT(vi2_cam, TEGRA_CSI_PHY_CIL_COMMAND,
				     0x22020202);
		else
			cil_phy_reg_write(vi2_cam, chan, 0x0202);

		format = TEGRA_IMAGE_FORMAT_T_A8B8G8R8;
		data_type = TEGRA_IMAGE_DT_RGB888;
		image_size = width * 3;
	} else if ((chan->code == V4L2_MBUS_FMT_UYVY8_2X8) ||
		   (chan->code == V4L2_MBUS_FMT_VYUY8_2X8) ||
		   (chan->code == V4L2_MBUS_FMT_YUYV8_2X8) ||
		   (chan->code == V4L2_MBUS_FMT_YVYU8_2X8)) {
		/* TBD */
	} else if ((chan->code == V4L2_MBUS_FMT_SBGGR8_1X8) ||
		   (chan->code == V4L2_MBUS_FMT_SGBRG8_1X8)) {
		format = TEGRA_IMAGE_FORMAT_T_L8;
		data_type = TEGRA_IMAGE_DT_RAW8;
		image_size = width;
	} else if ((chan->code == V4L2_MBUS_FMT_SBGGR10_1X10) ||
		   (chan->code == V4L2_MBUS_FMT_SRGGB10_1X10)) {
		format = TEGRA_IMAGE_FORMAT_T_R16_I;
		data_type = TEGRA_IMAGE_DT_RAW10;
		image_size = width * 10 / 8;
	} else if (chan->code == V4L2_MBUS_FMT_SRGGB12_1X12) {
		format = TEGRA_IMAGE_FORMAT_T_R16_I;
		data_type = TEGRA_IMAGE_DT_RAW12;
		image_size = width * 12 / 8;
	}

	csi_regs_write(vi2_cam, chan, TEGRA_VI_CSI_IMAGE_DEF,
			(vi2_cam->tpg_mode ? 0 : 1 << 24) | (format << 16) |
			0x1);
	csi_regs_write(vi2_cam, chan, TEGRA_VI_CSI_IMAGE_DT, data_type);
	csi_regs_write(vi2_cam, chan, TEGRA_VI_CSI_IMAGE_SIZE_WC, image_size);
	csi_regs_write(vi2_cam, chan, TEGRA_VI_CSI_IMAGE_SIZE,
			(height << 16) | width);

	/* Start pixel parser in single shot mode at beginning */
	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_STREAM_PP_COMMAND,
			  0xf005);

	return 0;
}

static int vi2_capture_buffer_setup(struct vi2_channel *chan,
				    struct tegra_camera_buffer *buf)
{
	struct vi2_camera *vi2_cam = chan->vi2_cam;

	switch (chan->fourcc) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		/* FIXME: Setup YUV buffer */

	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_RGB32:
		csi_regs_write(vi2_cam, chan,
			       TEGRA_VI_CSI_SURFACE0_OFFSET_MSB +
			       chan->surface * 8,
			       0x0);
		csi_regs_write(vi2_cam, chan,
			       TEGRA_VI_CSI_SURFACE0_OFFSET_LSB +
			       chan->surface * 8,
			       buf->buffer_addr);
		csi_regs_write(vi2_cam, chan,
			       TEGRA_VI_CSI_SURFACE0_STRIDE +
			       chan->surface * 4,
			       chan->bytes_per_line);
		break;
	default:
		dev_err(&vi2_cam->cam.pdev->dev, "Wrong host format %d\n",
			chan->fourcc);
		return -EINVAL;
	}

	return 0;
}

static void vi2_capture_error_status(struct vi2_channel *chan, int err)
{
	struct vi2_camera *vi2_cam = chan->vi2_cam;
	u32 val;

	dev_err(&vi2_cam->cam.pdev->dev,
		"CSI %d syncpt timeout, syncpt = %d, err = %d\n",
		chan->port, chan->syncpt_id, err);

#ifdef DEBUG
	val = TC_VI_REG_RD(vi2_cam, TEGRA_CSI_DEBUG_COUNTER_0);
	dev_err(&vi2_cam->cam.pdev->dev,
		"TEGRA_CSI_DEBUG_COUNTER_0 0x%08x\n", val);
#endif
	val = cil_regs_read(vi2_cam, chan, TEGRA_CSI_CIL_STATUS);
	dev_err(&vi2_cam->cam.pdev->dev,
		"TEGRA_CSI_CSI_CIL_STATUS 0x%08x\n", val);
	val = cil_regs_read(vi2_cam, chan, TEGRA_CSI_CILX_STATUS);
	dev_err(&vi2_cam->cam.pdev->dev,
		"TEGRA_CSI_CSI_CILX_STATUS 0x%08x\n", val);
	val = csi_pp_regs_read(vi2_cam, chan, TEGRA_CSI_PIXEL_PARSER_STATUS);
	dev_err(&vi2_cam->cam.pdev->dev,
		"TEGRA_CSI_PIXEL_PARSER_STATUS 0x%08x\n", val);
	val = csi_regs_read(vi2_cam, chan, TEGRA_VI_CSI_ERROR_STATUS);
	dev_err(&vi2_cam->cam.pdev->dev,
		"TEGRA_VI_CSI_ERROR_STATUS 0x%08x\n", val);
}

static int vi2_channel_capture_frame(struct vi2_channel *chan,
				     struct tegra_camera_buffer *buf)
{
	struct vi2_camera *vi2_cam = chan->vi2_cam;
	struct tegra_camera *cam = &vi2_cam->cam;
	u32 val;
	int err;

	/* Setup capture registers */
	vi2_channel_capture_setup(chan);

	/* Start capture */
	err = vi2_capture_buffer_setup(chan, buf);
	if (err < 0)
		return err;

	chan->syncpt_thresh = nvhost_syncpt_incr_max_ext(cam->pdev,
			chan->syncpt_id, 1);

	TC_VI_REG_WT(vi2_cam, TEGRA_VI_CFG_VI_INCR_SYNCPT,
		     vi2_syncpt_cond(VI_FRAME_START,  chan->port) |
		     chan->syncpt_id);

	csi_regs_write(vi2_cam, chan, TEGRA_VI_CSI_SINGLE_SHOT, 0x1);

	/* Move buffer to capture done queue */
	spin_lock(&chan->done_lock);
	list_add_tail(&buf->queue, &chan->done);
	spin_unlock(&chan->done_lock);

	/* Wait up kthread for capture done */
	wake_up_interruptible(&chan->done_wait);

	err = nvhost_syncpt_wait_timeout_ext(cam->pdev,
			chan->syncpt_id, chan->syncpt_thresh,
			TEGRA_SYNCPT_CSI_WAIT_TIMEOUT,
			NULL,
			NULL);

	/* Mark SOF flag to Zero after we captured the FIRST frame */
	if (chan->sof)
		chan->sof = 0;

	/* Capture syncpt timeout err, then dump error status */
	if (err)
		vi2_capture_error_status(chan, err);

	return err;
}


static int vi2_channel_capture_done(struct vi2_channel *chan,
				    struct tegra_camera_buffer *buf)
{
	struct vi2_camera *vi2_cam = chan->vi2_cam;
	struct tegra_camera *cam = &vi2_cam->cam;
	struct vb2_buffer *vb = &buf->vb;
	int err = 0;

	chan->syncpt_thresh = nvhost_syncpt_incr_max_ext(cam->pdev,
				chan->syncpt_id, 1);

	/*
	 * Make sure recieve VI_MW_ACK_DONE of the last frame before
	 * stop and dequeue buffer, otherwise MC error will shows up
	 * for the last frame.
	 */
	TC_VI_REG_WT(vi2_cam, TEGRA_VI_CFG_VI_INCR_SYNCPT,
		     vi2_syncpt_cond(VI_MW_ACK_DONE,  chan->port) |
		     chan->syncpt_id);

	/*
	 * Ignore error here and just stop pixel parser after waiting,
	 * even if it's timeout
	 */
	err = nvhost_syncpt_wait_timeout_ext(cam->pdev,
			chan->syncpt_id, chan->syncpt_thresh,
			TEGRA_SYNCPT_CSI_WAIT_TIMEOUT,
			NULL,
			NULL);
	if (err)
		dev_err(&vi2_cam->cam.pdev->dev,
			"MW_ACK_DONE syncpoint time out!\n");

	csi_pp_regs_write(vi2_cam, chan, TEGRA_CSI_PIXEL_STREAM_PP_COMMAND,
			  0xf002);

	/* Captured one frame */
	do_gettimeofday(&vb->v4l2_buf.timestamp);
	vb->v4l2_buf.sequence = chan->sequence++;
	vb->v4l2_buf.field = V4L2_FIELD_NONE;
	vb2_buffer_done(vb, err < 0 ? VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);

	return err;
}

struct tegra_camera_ops vi2_ops = {
	.add_device		= vi2_add_device,
	.remove_device		= vi2_remove_device,
	.clock_start		= vi2_common_clock_start,
	.clock_stop		=  vi2_common_clock_stop,
	.port_is_valid		= vi2_port_is_valid,
	.bytes_per_line		= vi2_bytes_per_line,
	.ignore_subdev_fmt	= vi2_ignore_subdev_fmt,
	.get_formats		= vi2_get_formats,
	.try_mbus_fmt		= vi2_try_mbus_fmt,
	.s_mbus_fmt		= vi2_s_mbus_fmt,
	.videobuf_queue		= vi2_videobuf_queue,
	.start_streaming	= vi2_start_streaming,
	.stop_streaming		= vi2_stop_streaming,
};

static struct of_device_id vi2_of_match[] = {
#ifdef TEGRA_12X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra124-vi",
		.data = (struct nvhost_device_data *)&t124_vi_info },
#endif
#ifdef TEGRA_21X_OR_HIGHER_CONFIG
	{ .compatible = "nvidia,tegra210-vi",
		.data = (struct nvhost_device_data *)&t21_vi_info },
#endif
#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	{ .compatible = "nvidia,tegra186-vi",
		.data = (struct nvhost_device_data *)&t18_vi_info },
#endif
	{ },
};

static int tegra_camera_slcg_handler(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct clk *clk;
	int ret = 0;

	struct nvhost_device_data *pdata =
		container_of(nb, struct nvhost_device_data,
			toggle_slcg_notifier);

	/* Skip this operation for TPG mode */
	if (tpg_mode)
		return NOTIFY_OK;

	clk = clk_get(&pdata->pdev->dev, "pll_d");
	if (IS_ERR(clk))
		return -EINVAL;

	/* Make CSI sourced from PLL_D */
	ret = tegra_clk_cfg_ex(clk, TEGRA_CLK_PLLD_CSI_OUT_ENB, 1);
	if (ret) {
		dev_err(&pdata->pdev->dev,
		"%s: failed to select CSI source pll_d: %d\n",
		__func__, ret);
		return ret;
	}

	/* Enable PLL_D */
	ret = clk_prepare_enable(clk);
	if (ret) {
		dev_err(&pdata->pdev->dev, "Can't enable pll_d: %d\n", ret);
		return ret;
	}

	udelay(1);

	/* Disable PLL_D */
	clk_disable_unprepare(clk);

	/* Restore CSI source */
	ret = tegra_clk_cfg_ex(clk, TEGRA_CLK_MIPI_CSI_OUT_ENB, 1);
	if (ret) {
		dev_err(&pdata->pdev->dev,
		"%s: failed to restore csi source: %d\n",
		__func__, ret);
		return ret;
	}

	clk_put(clk);

	return NOTIFY_OK;
}

static int vi2_probe(struct platform_device *pdev)
{
	struct vi2_camera *vi2_cam;
	struct nvhost_device_data *ndata = NULL;
	int err = 0;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(vi2_of_match, &pdev->dev);
		if (match) {
			ndata = (struct nvhost_device_data *)match->data;
			pdev->dev.platform_data = ndata;
		}

		/*
		 * Device Tree will initialize this ID as -1
		 * Set it to the right value for future usage
		 */
		pdev->id = pdev->dev.id;
	} else
		ndata = pdev->dev.platform_data;

	if (!ndata) {
		dev_err(&pdev->dev, "No nvhost device data!\n");
		return -EINVAL;
	}

	vi2_cam = devm_kzalloc(&pdev->dev, sizeof(struct vi2_camera),
			       GFP_KERNEL);
	if (!vi2_cam) {
		dev_err(&pdev->dev, "couldn't allocate cam\n");
		return -ENOMEM;
	}

	vi2_cam->ndata = ndata;
	ndata->pdev = pdev;
	vi2_cam->cam.pdev = pdev;

	/* Init Regulator */
	vi2_cam->regulator_name = "avdd_dsi_csi";
	vi2_cam->reg = devm_regulator_get(&pdev->dev, vi2_cam->regulator_name);
	if (IS_ERR_OR_NULL(vi2_cam->reg)) {
		dev_err(&pdev->dev, "%s: couldn't get regulator %s, err %ld\n",
			__func__, vi2_cam->regulator_name,
			PTR_ERR(vi2_cam->reg));
		return PTR_ERR(vi2_cam->reg);
	}

	/* Initialize our nvhost client */
	mutex_init(&ndata->lock);
	platform_set_drvdata(pdev, ndata);
	err = nvhost_client_device_get_resources(pdev);
	if (err) {
		dev_err(&pdev->dev, "%s: nvhost get resources failed %d\n",
				__func__, err);
		return err;
	}

	if (!ndata->aperture[0]) {
		dev_err(&pdev->dev, "%s: failed to map register base\n",
				__func__);
		return -ENXIO;
	}

	/* Match the nvhost_module_init VENC powergating */
	tegra_unpowergate_partition(TEGRA_POWERGATE_VENC);
	nvhost_module_init(pdev);

	err = nvhost_client_device_init(pdev);
	if (err) {
		dev_err(&pdev->dev, "%s: nvhost init failed %d\n",
			__func__, err);
		return err;
	}

	/* Get the VI register base */
	vi2_cam->reg_base = ndata->aperture[0];

	/* Match the nvhost_module_init VENC powergating */
	if (ndata->slcg_notifier_enable &&
			(ndata->powergate_id != -1)) {
		ndata->toggle_slcg_notifier.notifier_call =
		&tegra_camera_slcg_handler;

		slcg_register_notifier(ndata->powergate_id,
			&ndata->toggle_slcg_notifier);
	}

	platform_set_drvdata(pdev, vi2_cam);

	vi2_cam->tpg_mode = tpg_mode;

	/* Init VI2/CSI2 ops */
	strlcpy(vi2_cam->cam.card, VI2_CAM_CARD_NAME,
		sizeof(vi2_cam->cam.card));
	vi2_cam->cam.version = VI2_CAM_VERSION;
	vi2_cam->cam.ops = &vi2_ops;

	err = tegra_camera_init(pdev, &vi2_cam->cam);
	if (err) {
		platform_set_drvdata(pdev, vi2_cam->ndata);
		nvhost_client_device_release(pdev);
		vi2_cam->ndata->aperture[0] = NULL;
		return err;
	}

	dev_notice(&pdev->dev, "Tegra camera driver loaded.\n");

	return 0;
}

static int vi2_remove(struct platform_device *pdev)
{
	struct soc_camera_host *ici = to_soc_camera_host(&pdev->dev);
	struct tegra_camera *cam = container_of(ici,
					struct tegra_camera, ici);
	struct vi2_camera *vi2_cam = (struct vi2_camera *)cam;
	struct nvhost_device_data *ndata = vi2_cam->ndata;

	tegra_camera_deinit(pdev, &vi2_cam->cam);

	platform_set_drvdata(pdev, vi2_cam->ndata);

	if (ndata->slcg_notifier_enable &&
	    (ndata->powergate_id != -1))
		slcg_unregister_notifier(ndata->powergate_id,
					 &ndata->toggle_slcg_notifier);

	nvhost_client_device_release(pdev);
	vi2_cam->ndata->aperture[0] = NULL;

	dev_notice(&pdev->dev, "Tegra camera host driver unloaded\n");

	return 0;
}

static struct platform_driver vi2_driver = {
	.driver	= {
		.name	= VI2_CAM_DRV_NAME,
		.of_match_table = of_match_ptr(vi2_of_match),
	},
	.probe		= vi2_probe,
	.remove		= vi2_remove,
};

module_platform_driver(vi2_driver);

MODULE_DESCRIPTION("TEGRA SoC Camera Host driver");
MODULE_AUTHOR("Bryan Wu <pengw@nvidia.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("nvhost:" VI2_CAM_DRV_NAME);
