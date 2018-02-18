/*
 * tegra186_dspk_alt.h - Definitions for Tegra186 DSPK driver
 *
 * Copyright (c) 2015-2016 NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA186_DSPK_ALT_H__
#define __TEGRA186_DSPK_ALT_H__

/* Register offsets from DSPK BASE */
#define TEGRA186_DSPK_AXBAR_RX_STATUS                   0x0c
#define TEGRA186_DSPK_AXBAR_RX_INT_STATUS               0x10
#define TEGRA186_DSPK_AXBAR_RX_INT_MASK                 0x14
#define TEGRA186_DSPK_AXBAR_RX_INT_SET                  0x18
#define TEGRA186_DSPK_AXBAR_RX_INT_CLEAR                0x1c
#define TEGRA186_DSPK_AXBAR_RX_CIF_CTRL                 0x20
#define TEGRA186_DSPK_AXBAR_RX_CYA                      0x24
#define TEGRA186_DSPK_AXBAR_RX_CIF_FIFO_STATUS          0x28

#define TEGRA186_DSPK_ENABLE                            0x40
#define TEGRA186_DSPK_SOFT_RESET                        0x44
#define TEGRA186_DSPK_CG                                0x48
#define TEGRA186_DSPK_STATUS                            0x4c
#define TEGRA186_DSPK_INT_STATUS                        0x50
#define TEGRA186_DSPK_CORE_CTRL                         0x60
#define TEGRA186_DSPK_CODEC_CTRL                        0x64
#define TEGRA186_DSPK_CODEC_DATA                        0x68
#define TEGRA186_DSPK_CODEC_ENABLE                      0x6c
#define TEGRA186_DSPK_CLK_TRIM                          0x70
#define TEGRA186_DSPK_SDM_COEF_A_2                      0x74
#define TEGRA186_DSPK_SDM_COEF_A_3                      0x78
#define TEGRA186_DSPK_SDM_COEF_A_4                      0x7c
#define TEGRA186_DSPK_SDM_COEF_A_5                      0x80
#define TEGRA186_DSPK_SDM_COEF_C_1                      0x84
#define TEGRA186_DSPK_SDM_COEF_C_2                      0x88
#define TEGRA186_DSPK_SDM_COEF_C_3                      0x8c
#define TEGRA186_DSPK_SDM_COEF_C_4                      0x90
#define TEGRA186_DSPK_SDM_COEF_G_1                      0x94
#define TEGRA186_DSPK_SDM_COEF_G_2                      0x98
#define TEGRA186_DSPK_DEBUG_STATUS                      0x9c
#define TEGRA186_DSPK_DEBUG_CIF_CNTR                    0xa0
#define TEGRA186_DSPK_DEBUG_STAGE1_CNTR                 0xa4
#define TEGRA186_DSPK_DEBUG_STAGE2_CNTR                 0xa8
#define TEGRA186_DSPK_DEBUG_STAGE3_CNTR                 0xac
#define TEGRA186_DSPK_DEBUG_STAGE4_CNTR                 0xb0

/* Constants for DSPK */
#define TEGRA186_DSPK_OSR_32                            0
#define TEGRA186_DSPK_OSR_64                            1
#define TEGRA186_DSPK_OSR_128                           2
#define TEGRA186_DSPK_OSR_256                           3

/* DSPK ENABLE Register field */
#define TEGRA186_DSPK_ENABLE_EN                         BIT(0)

/* DSPK SOFT RESET field */
#define TEGRA186_DSPK_SOFT_RESET_EN                     BIT(0)

/* DSPK CG field */
#define TEGRA186_DSPK_CG_SLCG_EN                        BIT(0)

/* DSPK STATUS fields */
#define TEGRA186_DSPK_CODEC_CONFIG_DONE_SHIFT           14
#define TEGRA186_DSPK_CODEC_CONFIG_DONE_MASK            (0x1 << TEGRA186_DSPK_CODEC_CONFIG_DONE_SHIFT)

#define TEGRA186_DSPK_SLCG_CLKEN_SHIFT                  12
#define TEGRA186_DSPK_SLCG_CLKEN_MASK                   (0x1 << TEGRA186_DSPK_SLCG_CLKEN_SHIFT)

#define TEGRA186_DSPK_RX_CIF_FULL_SHIFT                 10
#define TEGRA186_DSPK_RX_CIF_FULL_MASK                  (0x1 << TEGRA186_DSPK_RX_CIF_FULL_SHIFT)

#define TEGRA186_DSPK_RX_CIF_EMPTY_SHIFT                9
#define TEGRA186_DSPK_RX_CIF_EMPTY_MASK                 (0x1 << TEGRA186_DSPK_RX_CIF_EMPTY_SHIFT)

#define TEGRA186_DSPK_RX_ENABLED_SHIFT                  8
#define TEGRA186_DSPK_RX_ENABLED_MASK                   (0x1 << TEGRA186_DSPK_RX_ENABLED_SHIFT)

/* DSPK INT STATUS fields */
#define TEGRA186_DSPK_INT_CODEC_CONFIG_DONE_SHIFT       12
#define TEGRA186_DSPK_INT_CODEC_CONFIG_DONE_MASK        (0x1 << TEGRA186_DSPK_INT_CODEC_CONFIG_DONE_SHIFT)

#define TEGRA186_DSPK_RX_CIF_FIFO_UNDERRUN_SHIFT        9
#define TEGRA186_DSPK_RX_CIF_FIFO_UNDERRUN_MASK         (0x1 << TEGRA186_DSPK_RX_CIF_FIFO_UNDERRUN_SHIFT)

#define TEGRA186_DSPK_RX_DONE_SHIFT                     8
#define TEGRA186_DSPK_RX_DONE_MASK                      (0x1 << TEGRA186_DSPK_RX_DONE_SHIFT)

/* DSPK CORE CONTROL fields */
#define TEGRA186_DSPK_GAIN1_SHIFT                       28
#define TEGRA186_DSPK_GAIN1_MASK                        (0x7 << TEGRA186_DSPK_GAIN1_SHIFT)

#define TEGRA186_DSPK_GAIN2_SHIFT                       24
#define TEGRA186_DSPK_GAIN2_MASK                        (0x7 << TEGRA186_DSPK_GAIN2_SHIFT)

#define TEGRA186_DSPK_GAIN3_SHIFT                       20
#define TEGRA186_DSPK_GAIN3_MASK                        (0x7 << TEGRA186_DSPK_GAIN3_SHIFT)

#define TEGRA186_DSPK_FILTER_MODE_SHIFT                 16
#define TEGRA186_DSPK_FILTER_MODE_MASK                  (0x1 << TEGRA186_DSPK_FILTER_MODE_SHIFT)

#define TEGRA186_DSPK_CHANNEL_SELECT_SHIFT              8
#define TEGRA186_DSPK_CHANNEL_SELECT_MASK               (0x3 << TEGRA186_DSPK_CHANNEL_SELECT_SHIFT)

#define TEGRA186_DSPK_OSR_SHIFT                         4
#define TEGRA186_DSPK_OSR_MASK                          (0x3 << TEGRA186_DSPK_OSR_SHIFT)

#define TEGRA186_DSPK_LRSEL_POLARITY_SHIFT              0
#define TEGRA186_DSPK_LRSEL_POLARITY_MASK               (0x1 << TEGRA186_DSPK_LRSEL_POLARITY_SHIFT)

/* DSPK CODEC CONTROL fileds */
#define TEGRA186_DSPK_CODEC_CHANNEL_SELECT_SHIFT        24
#define TEGRA186_DSPK_CODEC_CHANNEL_SELECT_MASK         (0x3 << TEGRA186_DSPK_CODEC_CHANNEL_SELECT_SHIFT)

#define TEGRA186_DSPK_CODEC_BIT_ORDER_SHIFT             16
#define TEGRA186_DSPK_CODEC_BIT_MASK                    (0x1 << TEGRA186_DSPK_CODEC_BIT_ORDER_SHIFT)

#define TEGRA186_DSPK_CODEC_CONFIG_MODE_SHIFT           12
#define TEGRA186_DSPK_CODEC_CONFIG_MODE_MASK            (0x1 << TEGRA186_DSPK_CODEC_CONFIG_MODE_SHIFT)

#define TEGRA186_DSPK_CODEC_CONFIG_REP_NUM_SHIFT        0
#define TEGRA186_DSPK_CODEC_CONFIG_REP_NUM_MASK         (0xff << TEGRA186_DSPK_CODEC_CONFIG_REP_NUM_SHIFT)

/* DSPK CODEC ENABLE fields */
#define TEGRA186_DSPK_CODEC_ENABLE_SHIFT                0
#define TEGRA186_DSPK_CODEC_ENABLE_MASK                 (0x1 << TEGRA186_DSPK_CODEC_ENABLE_SHIFT)

/* DSPL CLK TRIM field */
#define TEGRA186_DSPK_CLK_TRIM_SHIFT                    0
#define TEGRA186_DSPK_CLK_TRIM_MASK                     (0x1f << TEGRA186_DSPK_CLK_TRIM_SHIFT)

/* DSPK DEBUG Register fields*/
#define TEGRA186_DSPK_DEBUG_STATUS_SHIFT                0
#define TEGRA186_DSPK_DEBUG_STATUS_MASK                 (0xff << TEGRA186_DSPK_DEBUG_STATUS_SHIFT)

#define TEGRA186_DSPK_DEBUG_CIF_CH0_SHIFT               16
#define TEGRA186_DSPK_DEBUG_CIF_CH0_MASK                (0xffff << TEGRA186_DSPK_DEBUG_CIF_CH0_SHIFT)
#define TEGRA186_DSPK_DEBUG_CIF_CH1_SHIFT               0
#define TEGRA186_DSPK_DEBUG_CIF_CH1_MASK                (0xffff << TEGRA186_DSPK_DEBUG_CIF_CH1_SHIFT)

#define TEGRA186_DSPK_DEBUG_STAGE1_CH0_SHIFT            16
#define TEGRA186_DSPK_DEBUG_STAGE1_CH0_MASK             (0xffff << TEGRA186_DSPK_DEBUG_STAGE1_CH0_SHIFT)
#define TEGRA186_DSPK_DEBUG_STAGE1_CH1_SHIFT            0
#define TEGRA186_DSPK_DEBUG_STAGE1_CH1_MASK             (0xffff << TEGRA186_DSPK_DEBUG_STAGE1_CH1_SHIFT)

#define TEGRA186_DSPK_DEBUG_STAGE2_CH0_SHIFT            16
#define TEGRA186_DSPK_DEBUG_STAGE2_CH0_MASK             (0xffff << TEGRA186_DSPK_DEBUG_STAGE2_CH0_SHIFT)
#define TEGRA186_DSPK_DEBUG_STAGE2_CH1_SHIFT            0
#define TEGRA186_DSPK_DEBUG_STAGE2_CH1_MASK             (0xffff << TEGRA186_DSPK_DEBUG_STAGE2_CH1_SHIFT)

#define TEGRA186_DSPK_DEBUG_STAGE3_CH0_SHIFT            16
#define TEGRA186_DSPK_DEBUG_STAGE3_CH0_MASK             (0xffff << TEGRA186_DSPK_DEBUG_STAGE3_CH0_SHIFT)
#define TEGRA186_DSPK_DEBUG_STAGE3_CH1_SHIFT            0
#define TEGRA186_DSPK_DEBUG_STAGE3_CH1_MASK             (0xffff << TEGRA186_DSPK_DEBUG_STAGE3_CH1_SHIFT)

#define TEGRA186_DSPK_DEBUG_STAGE4_CH0_SHIFT            16
#define TEGRA186_DSPK_DEBUG_STAGE4_CH0_MASK             (0xffff << TEGRA186_DSPK_DEBUG_STAGE4_CH0_SHIFT)
#define TEGRA186_DSPK_DEBUG_STAGE4_CH1_SHIFT            0
#define TEGRA186_DSPK_DEBUG_STAGE4_CH1_MASK             (0xffff << TEGRA186_DSPK_DEBUG_STAGE4_CH1_SHIFT)


struct tegra186_dspk_soc_data {
        void (*set_audio_cif) (struct regmap *map,
			unsigned int reg,
			struct tegra210_xbar_cif_conf *conf);
};

struct tegra186_dspk {
        struct clk *clk_dspk;
	struct clk *clk_pll_a_out0;
        struct regmap *regmap;
        const struct tegra186_dspk_soc_data *soc_data;
	int is_pinctrl;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_active_state;
	struct pinctrl_state *pin_idle_state;
};

#endif
