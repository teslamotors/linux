/*
 * ahub_unit_fpga_clock.h
 *
 * Copyright (c) 2013-2015, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __AHUB_UNIT_FPGA_CLOCK_H__
#define __AHUB_UNIT_FPGA_CLOCK_H__

#define SYSTEM_FPGA	0
#define DEBUG_FPGA	1

#define APE_FPGA_MISC_CLK_SOURCE_I2C1_0  0x68
#define APE_FPGA_MISC_CLK_SOURCE_I2S1_0  0x28
#define APE_FPGA_MISC_CLK_SOURCE_I2S2_0  0x20
#define APE_FPGA_MISC_CLK_SOURCE_I2S3_0  0x24
#define APE_FPGA_MISC_CLK_SOURCE_I2S4_0  0x2c
#define APE_FPGA_MISC_CLK_SOURCE_I2S5_0  0x30
#define APE_FPGA_MISC_CLK_SOURCE_DMIC1_0	0x14

#define I2C_I2C_CMD_ADDR0_0 0x4
#define I2C_I2C_CNFG_0 0x0
#define I2C_I2C_CMD_DATA1_0 0xc
#define I2C_I2C_CONFIG_LOAD_0 0x8c
#define I2C_I2C_STATUS_0 0x1c

#define I2C_I2C_CMD_ADDR1_0 0x8
#define I2C_I2C_CMD_DATA1_0 0xc
#define I2C_I2C_CMD_DATA2_0 0x10

#define CLK_RST_CONTROLLER_CLK_SOURCE_I2S1_0  0x1d8
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2S2_0  0x100
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2S3_0  0x104
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2S4_0  0x3bc
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2S5_0  0x3c0

#define CLK_RST_CONTROLLER_RST_DEVICES_L_0	0x4
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0	0x10
#define CLK_RST_CONTROLLER_CLK_SOURCE_I2C1_0	0x124

#define PINMUX_AUX_GEN1_I2C_SDA_0		0x30c0
#define PINMUX_AUX_GEN1_I2C_SCL_0		0x30bc

#define PINMUX_AUX_DAP1_SCLK_0 0x3130
#define PINMUX_AUX_DAP1_FS_0   0x3124
#define PINMUX_AUX_DAP1_DIN_0  0x3128
#define PINMUX_AUX_DAP1_DOUT_0 0x312c
#define PINMUX_AUX_DAP2_SCLK_0 0x3140
#define PINMUX_AUX_DAP2_FS_0   0x3134
#define PINMUX_AUX_DAP2_DIN_0  0x3138
#define PINMUX_AUX_DAP2_DOUT_0 0x313c

#define PINMUX_AUX_DMIC1_CLK_0 0x30a4  /* DAP3_SCLK_0 */
#define PINMUX_AUX_DMIC1_DAT_0 0x30a8  /* DAP3_FS_0 */
#define PINMUX_AUX_DMIC2_CLK_0 0x30ac  /* DAP3_DIN_0 */
#define PINMUX_AUX_DMIC2_DAT_0 0x30b0  /* DAP3_DOUT_0 */

#define PINMUX_AUX_GPIO_PK0_0  0x3254  /* DAP5 */
#define PINMUX_AUX_GPIO_PK1_0  0x3258  /* DAP5 */
#define PINMUX_AUX_GPIO_PK2_0  0x325c  /* DAP5 */
#define PINMUX_AUX_GPIO_PK3_0  0x3260  /* DAP5 */

#define NV_ADDRESS_MAP_APE_AHUB_FPGA_CAR_BASE		1882050560
#define NV_ADDRESS_MAP_APE_AHUB_FPGA_CAR_LIMIT		1882054655
#define NV_ADDRESS_MAP_APE_AHUB_FPGA_CAR_SIZE		4096
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define NV_ADDRESS_MAP_APE_AHUB_FPGA_MISC_BASE		1882048512
#define NV_ADDRESS_MAP_APE_AHUB_FPGA_MISC_LIMIT		1882048767
#endif
#define NV_ADDRESS_MAP_APE_AHUB_FPGA_MISC_SIZE		256

#if SYSTEM_FPGA
#define NV_ADDRESS_MAP_APE_AHUB_I2C_BASE			0x7000c000
#else
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define NV_ADDRESS_MAP_APE_AHUB_I2C_BASE			1882047744
#endif
#define NV_ADDRESS_MAP_APE_AHUB_GPIO_BASE		0x702DC700
#define APE_AHUB_GPIO_CNF_0	0x0
#define APE_AHUB_GPIO_OE_0	0x10
#define APE_AHUB_GPIO_OUT_0	0x20
#endif

#define NV_ADDRESS_MAP_APE_AHUB_I2C_LIMIT			1882048255
#define NV_ADDRESS_MAP_APE_AHUB_I2C_SIZE			512

#define NV_ADDRESS_MAP_APB_PP_BASE				1879048192
#define NV_ADDRESS_MAP_PPSB_CLK_RST_BASE			1610637312

#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
#define NV_ADDRESS_MAP_APE_I2S5_BASE                           0x702d1400
#endif
#define I2S5_CYA_0                                             0xb0

#define CDCE906_04_0960_MHz   0
#define CDCE906_06_1440_MHz   1
#define CDCE906_08_1920_MHz   2
#define CDCE906_11_2896_MHz   3
#define CDCE906_12_2880_MHz   4
#define CDCE906_16_3840_MHz   5
#define CDCE906_22_5792_MHz   6
#define CDCE906_24_5760_MHz   7
#define CDCE906_09_2160_MHz   8
#define CDCE906_16_9344_MHz   9
#define CDCE906_18_4320_MHz  10
#define CDCE906_33_8688_MHz  11
#define CDCE906_36_8640_MHz  12

#define MAX9485_DEVICE_ADDRESS 0x60

#define MAX9485_MCLK_FREQ_163840 0x31
#define MAX9485_MCLK_FREQ_112896 0x22
#define MAX9485_MCLK_FREQ_122880 0x23
#define MAX9485_MCLK_FREQ_225792 0x32
#define MAX9485_MCLK_FREQ_245760 0x33

enum AUDIO_DAC_DATAWIDTH{
	AUDIO_DAC_DATAWIDTH_16 = 3,
	AUDIO_DAC_DATAWIDTH_24 = 5
};

enum AUDIO_SAMPLE_RATE{
	AUDIO_SAMPLE_RATE_8_00 = 8000,
	AUDIO_SAMPLE_RATE_11_02 = 11020,
	AUDIO_SAMPLE_RATE_12_00 = 12000,
	AUDIO_SAMPLE_RATE_16_00 = 16000,
	AUDIO_SAMPLE_RATE_22_05 = 22050,
	AUDIO_SAMPLE_RATE_24_00 = 24000,
	AUDIO_SAMPLE_RATE_32_00 = 32000,
	AUDIO_SAMPLE_RATE_44_10 = 44100,
	AUDIO_SAMPLE_RATE_48_00 = 48000,
	AUDIO_SAMPLE_RATE_64_00 = 64000,
	AUDIO_SAMPLE_RATE_88_20 = 88200,
	AUDIO_SAMPLE_RATE_96_00 = 96000,
	AUDIO_SAMPLE_RATE_128_00 = 128000,
	AUDIO_SAMPLE_RATE_176_40 = 176000,
	AUDIO_SAMPLE_RATE_192_00 = 192000
};

enum I2S_ID{
	I2S1 = 0,
	I2S2,
	I2S3,
	I2S4,
	I2S5
};

enum AUDIO_MASTER_CLK_FREQ{
	CLK_OUT_3_0720_MHZ = 0,
	CLK_OUT_4_0960_MHZ,
	CLK_OUT_4_6080_MHZ,
	CLK_OUT_5_6448_MHZ,
	CLK_OUT_6_1440_MHZ,
	CLK_OUT_8_1920_MHZ,
	CLK_OUT_11_2896_MHZ,
	CLK_OUT_12_2888_MHZ,
	CLK_OUT_16_3840_MHZ,
	CLK_OUT_16_9344_MHZ,
	CLK_OUT_18_4320_MHZ,
	CLK_OUT_22_5792_MHZ,
	CLK_OUT_24_5760_MHZ,
	CLK_OUT_33_8688_MHZ,
	CLK_OUT_36_8640_MHZ,
	CLK_OUT_49_1520_MHZ,
	CLK_OUT_73_7280_MHZ,
	CLK_OUT_162_MHZ,
	TOTAL_CLK_OUT
};

enum AUDIO_CLOCK_GEN_SELECT{
	CLK_OUT_FROM_TEGRA,
	CLK_GEN_CDCE906,
	CLK_GEN_MAX9485
};

#define AD1937_X_ADDRESS	0x05
#define AD1937_Y_ADDRESS 	0x04
#define AD1937_Z_ADDRESS 	0x07

#define AD1937_PLL_CLK_CTRL_0                               0x00
#define AD1937_PLL_CLK_CTRL_1                               0x01
#define AD1937_DAC_CTRL_0                                   0x02
#define AD1937_DAC_CTRL_1                                   0x03
#define AD1937_DAC_CTRL_2                                   0x04
#define AD1937_DAC_MUTE_CTRL                                0x05
#define AD1937_DAC_VOL_CTRL_DAC1L                           0x06
#define AD1937_DAC_VOL_CTRL_DAC1R                           0x07
#define AD1937_DAC_VOL_CTRL_DAC2L                           0x08
#define AD1937_DAC_VOL_CTRL_DAC2R                           0x09
#define AD1937_DAC_VOL_CTRL_DAC3L                           0x0a
#define AD1937_DAC_VOL_CTRL_DAC3R                           0x0b
#define AD1937_DAC_VOL_CTRL_DAC4L                           0x0c
#define AD1937_DAC_VOL_CTRL_DAC4R                           0x0d
#define AD1937_ADC_CTRL_0                                   0x0e
#define AD1937_ADC_CTRL_1                                   0x0f
#define AD1937_ADC_CTRL_2                                   0x10

#define AD1937_PLL_CLK_CTRL_0_PWR_MASK                      (1 << 0)
#define AD1937_PLL_CLK_CTRL_0_PWR_ON                        (0 << 0)
#define AD1937_PLL_CLK_CTRL_0_PWR_OFF                       (1 << 0)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_MASK                    (3 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_256                     (0 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_384                     (1 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_512                     (2 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKI_768                     (3 << 1)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_MASK                    (3 << 3)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_XTAL                    (0 << 3)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_256                     (1 << 3)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_512                     (2 << 3)
#define AD1937_PLL_CLK_CTRL_0_MCLKO_OFF                     (3 << 3)
#define AD1937_PLL_CLK_CTRL_0_PLL_INPUT_MASK                (3 << 5)
#define AD1937_PLL_CLK_CTRL_0_PLL_INPUT_MCLKI               (0 << 5)
#define AD1937_PLL_CLK_CTRL_0_PLL_INPUT_DLRCLK              (1 << 5)
#define AD1937_PLL_CLK_CTRL_0_PLL_INPUT_ALRCLK              (2 << 5)
#define AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_MASK      (1 << 7)
#define AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_DISABLE   (0 << 7)
#define AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_ENABLE    (1 << 7)


#define AD1937_PLL_CLK_CTRL_1_DAC_CLK_MASK                  (1 << 0)
#define AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL                   (0 << 0)
#define AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK                  (1 << 0)
#define AD1937_PLL_CLK_CTRL_1_ADC_CLK_MASK                  (1 << 1)
#define AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL                   (0 << 1)
#define AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK                  (1 << 1)
#define AD1937_PLL_CLK_CTRL_1_VREF_MASK                     (1 << 2)
#define AD1937_PLL_CLK_CTRL_1_VREF_ENABLE                   (0 << 2)
#define AD1937_PLL_CLK_CTRL_1_VREF_DISABLE                  (1 << 2)
#define AD1937_PLL_CLK_CTRL_1_PLL_MASK                      (1 << 3)
#define AD1937_PLL_CLK_CTRL_1_PLL_NOT_LOCKED                (0 << 3)
#define AD1937_PLL_CLK_CTRL_1_PLL_LOCKED                    (1 << 3)

#define AD1937_DAC_CTRL_0_PWR_MASK                          (1 << 0)
#define AD1937_DAC_CTRL_0_PWR_ON                            (0 << 0)
#define AD1937_DAC_CTRL_0_PWR_OFF                           (1 << 0)
#define AD1937_DAC_CTRL_0_SAMPLE_RATE_MASK                  (3 << 1)
#define AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ        (0 << 1)
#define AD1937_DAC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ        (1 << 1)
#define AD1937_DAC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ     (2 << 1)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_MASK                 (7 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_1                    (0 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_0                    (1 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_8                    (2 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_12                   (3 << 3)
#define AD1937_DAC_CTRL_0_DSDATA_DELAY_16                   (4 << 3)
#define AD1937_DAC_CTRL_0_FMT_MASK                          (3 << 6)
#define AD1937_DAC_CTRL_0_FMT_STEREO                        (0 << 6)
#define AD1937_DAC_CTRL_0_FMT_TDM_SINGLE_LINE               (1 << 6)
#define AD1937_DAC_CTRL_0_FMT_TDM_AUX                       (2 << 6)
#define AD1937_DAC_CTRL_0_FMT_TDM_DUAL_LINE                 (3 << 6)

#define AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MASK            (1 << 0)
#define AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MIDCYCLE        (0 << 0)
#define AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_PIPELINE        (1 << 0)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_MASK              (3 << 1)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_64                (0 << 1)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_128               (1 << 1)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_256               (2 << 1)
#define AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_512               (3 << 1)
#define AD1937_DAC_CTRL_1_DLRCLK_POLARITY_MASK              (1 << 3)
#define AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_LOW          (0 << 3)
#define AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_HIGH         (1 << 3)
#define AD1937_DAC_CTRL_1_DLRCLK_MASK                       (1 << 4)
#define AD1937_DAC_CTRL_1_DLRCLK_SLAVE                      (0 << 4)
#define AD1937_DAC_CTRL_1_DLRCLK_MASTER                     (1 << 4)
#define AD1937_DAC_CTRL_1_DBCLK_MASK                        (1 << 5)
#define AD1937_DAC_CTRL_1_DBCLK_SLAVE                       (0 << 5)
#define AD1937_DAC_CTRL_1_DBCLK_MASTER                      (1 << 5)
#define AD1937_DAC_CTRL_1_DBCLK_SOURCE_MASK                 (1 << 6)
#define AD1937_DAC_CTRL_1_DBCLK_SOURCE_DBCLK_PIN            (0 << 6)
#define AD1937_DAC_CTRL_1_DBCLK_SOURCE_INTERNAL             (1 << 6)
#define AD1937_DAC_CTRL_1_DBCLK_POLARITY_MASK               (1 << 7)
#define AD1937_DAC_CTRL_1_DBCLK_POLARITY_NORMAL             (0 << 7)
#define AD1937_DAC_CTRL_1_DBCLK_POLARITY_INVERTED           (1 << 7)

#define AD1937_DAC_CTRL_2_MASTER_MASK                       (1 << 0)
#define AD1937_DAC_CTRL_2_MASTER_UNMUTE                     (0 << 0)
#define AD1937_DAC_CTRL_2_MASTER_MUTE                       (1 << 0)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_MASK                   (3 << 1)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_FLAT                   (0 << 1)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_48_KHZ                 (1 << 1)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_44_1_KHZ               (2 << 1)
#define AD1937_DAC_CTRL_2_DEEMPHASIS_32_KHZ                 (3 << 1)
#define AD1937_DAC_CTRL_2_WORD_WIDTH_MASK                   (3 << 3)
#define AD1937_DAC_CTRL_2_WORD_WIDTH_24_BITS                (0 << 3)
#define AD1937_DAC_CTRL_2_WORD_WIDTH_20_BITS                (1 << 3)
#define AD1937_DAC_CTRL_2_WORD_WIDTH_16_BITS                (3 << 3)
#define AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_MASK          (1 << 5)
#define AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_NORMAL        (0 << 5)
#define AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_INVERTED      (1 << 5)

#define AD1937_DAC_MUTE_CTRL_DAC1L_MASK                     (1 << 0)
#define AD1937_DAC_MUTE_CTRL_DAC1L_UNMUTE                   (0 << 0)
#define AD1937_DAC_MUTE_CTRL_DAC1L_MUTE                     (1 << 0)
#define AD1937_DAC_MUTE_CTRL_DAC1R_MASK                     (1 << 1)
#define AD1937_DAC_MUTE_CTRL_DAC1R_UNMUTE                   (0 << 1)
#define AD1937_DAC_MUTE_CTRL_DAC1R_MUTE                     (1 << 1)
#define AD1937_DAC_MUTE_CTRL_DAC2L_MASK                     (1 << 2)
#define AD1937_DAC_MUTE_CTRL_DAC2L_UNMUTE                   (0 << 2)
#define AD1937_DAC_MUTE_CTRL_DAC2L_MUTE                     (1 << 2)
#define AD1937_DAC_MUTE_CTRL_DAC2R_MASK                     (1 << 3)
#define AD1937_DAC_MUTE_CTRL_DAC2R_UNMUTE                   (0 << 3)
#define AD1937_DAC_MUTE_CTRL_DAC2R_MUTE                     (1 << 3)
#define AD1937_DAC_MUTE_CTRL_DAC3L_MASK                     (1 << 4)
#define AD1937_DAC_MUTE_CTRL_DAC3L_UNMUTE                   (0 << 4)
#define AD1937_DAC_MUTE_CTRL_DAC3L_MUTE                     (1 << 4)
#define AD1937_DAC_MUTE_CTRL_DAC3R_MASK                     (1 << 5)
#define AD1937_DAC_MUTE_CTRL_DAC3R_UNMUTE                   (0 << 5)
#define AD1937_DAC_MUTE_CTRL_DAC3R_MUTE                     (1 << 5)
#define AD1937_DAC_MUTE_CTRL_DAC4L_MASK                     (1 << 6)
#define AD1937_DAC_MUTE_CTRL_DAC4L_UNMUTE                   (0 << 6)
#define AD1937_DAC_MUTE_CTRL_DAC4L_MUTE                     (1 << 6)
#define AD1937_DAC_MUTE_CTRL_DAC4R_MASK                     (1 << 7)
#define AD1937_DAC_MUTE_CTRL_DAC4R_UNMUTE                   (0 << 7)
#define AD1937_DAC_MUTE_CTRL_DAC4R_MUTE                     (1 << 7)

#define AD1937_DAC_VOL_CTRL_DAC1L_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC1R_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC2L_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC2R_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC3L_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC3R_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC4L_MASK                      (0xff << 0)
#define AD1937_DAC_VOL_CTRL_DAC4R_MASK                      (0xff << 0)

#define AD1937_ADC_CTRL_0_PWR_MASK                          (1 << 0)
#define AD1937_ADC_CTRL_0_PWR_ON                            (0 << 0)
#define AD1937_ADC_CTRL_0_PWR_OFF                           (1 << 0)
#define AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_MASK             (1 << 1)
#define AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_OFF              (0 << 1)
#define AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_ON               (1 << 1)
#define AD1937_ADC_CTRL_0_ADC1L_MASK                        (1 << 2)
#define AD1937_ADC_CTRL_0_ADC1L_UNMUTE                      (0 << 2)
#define AD1937_ADC_CTRL_0_ADC1L_MUTE                        (1 << 2)
#define AD1937_ADC_CTRL_0_ADC1R_MASK                        (1 << 3)
#define AD1937_ADC_CTRL_0_ADC1R_UNMUTE                      (0 << 3)
#define AD1937_ADC_CTRL_0_ADC1R_MUTE                        (1 << 3)
#define AD1937_ADC_CTRL_0_ADC2L_MASK                        (1 << 4)
#define AD1937_ADC_CTRL_0_ADC2L_UNMUTE                      (0 << 4)
#define AD1937_ADC_CTRL_0_ADC2L_MUTE                        (1 << 4)
#define AD1937_ADC_CTRL_0_ADC2R_MASK                        (1 << 5)
#define AD1937_ADC_CTRL_0_ADC2R_UNMUTE                      (0 << 5)
#define AD1937_ADC_CTRL_0_ADC2R_MUTE                        (1 << 5)
#define AD1937_ADC_CTRL_0_SAMPLE_RATE_MASK                  (3 << 6)
#define AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ        (0 << 6)
#define AD1937_ADC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ        (1 << 6)
#define AD1937_ADC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ     (2 << 6)

#define AD1937_ADC_CTRL_1_WORD_WIDTH_MASK                   (3 << 0)
#define AD1937_ADC_CTRL_1_WORD_WIDTH_24_BITS                (0 << 0)
#define AD1937_ADC_CTRL_1_WORD_WIDTH_20_BITS                (1 << 0)
#define AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS                (3 << 0)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_MASK                 (7 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_1                    (0 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_0                    (1 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_8                    (2 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_12                   (3 << 2)
#define AD1937_ADC_CTRL_1_ASDATA_DELAY_16                   (4 << 2)
#define AD1937_ADC_CTRL_1_FMT_MASK                          (3 << 5)
#define AD1937_ADC_CTRL_1_FMT_STEREO                        (0 << 5)
#define AD1937_ADC_CTRL_1_FMT_TDM_SINGLE_LINE               (1 << 5)
#define AD1937_ADC_CTRL_1_FMT_TDM_AUX                       (2 << 5)
#define AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MASK            (1 << 7)
#define AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MIDCYCLE        (0 << 7)
#define AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_PIPELINE        (1 << 7)

#define AD1937_ADC_CTRL_2_ALRCLK_FMT_MASK                   (1 << 0)
#define AD1937_ADC_CTRL_2_ALRCLK_FMT_50_50                  (0 << 0)
#define AD1937_ADC_CTRL_2_ALRCLK_FMT_PULSE                  (1 << 0)
#define AD1937_ADC_CTRL_2_ABCLK_POLARITY_MASK               (1 << 1)
#define AD1937_ADC_CTRL_2_ABCLK_POLARITY_FALLING_EDGE       (0 << 1)
#define AD1937_ADC_CTRL_2_ABCLK_POLARITY_RISING_EDGE        (1 << 1)
#define AD1937_ADC_CTRL_2_ALRCLK_POLARITY_MASK              (1 << 2)
#define AD1937_ADC_CTRL_2_ALRCLK_POLARITY_LEFT_LOW          (0 << 2)
#define AD1937_ADC_CTRL_2_ALRCLK_POLARITY_LEFT_HIGH         (1 << 2)
#define AD1937_ADC_CTRL_2_ALRCLK_MASK                       (1 << 3)
#define AD1937_ADC_CTRL_2_ALRCLK_SLAVE                      (0 << 3)
#define AD1937_ADC_CTRL_2_ALRCLK_MASTER                     (1 << 3)
#define AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_MASK              (3 << 4)
#define AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_64                (0 << 4)
#define AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_128               (1 << 4)
#define AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_256               (2 << 4)
#define AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_512               (3 << 4)
#define AD1937_ADC_CTRL_2_ABCLK_MASK                        (1 << 6)
#define AD1937_ADC_CTRL_2_ABCLK_SLAVE                       (0 << 6)
#define AD1937_ADC_CTRL_2_ABCLK_MASTER                      (1 << 6)
#define AD1937_ADC_CTRL_2_ABCLK_SOURCE_MASK                 (1 << 7)
#define AD1937_ADC_CTRL_2_ABCLK_SOURCE_ABCLK_PIN            (0 << 7)
#define AD1937_ADC_CTRL_2_ABCLK_SOURCE_INTERNAL             (1 << 7)

#define AUDIO_CODEC_SLAVE_MODE 1
#define AUDIO_CODEC_MASTER_MODE 0

#define AUDIO_DAC_MASTER_MODE 1
#define AUDIO_DAC_SLAVE_MODE 0

#define I2S_DATAWIDTH_04 0
#define I2S_DATAWIDTH_08 1
#define I2S_DATAWIDTH_12 2
#define I2S_DATAWIDTH_16 3
#define I2S_DATAWIDTH_20 4
#define I2S_DATAWIDTH_24 5
#define I2S_DATAWIDTH_28 6
#define I2S_DATAWIDTH_32 7

#define AD1937_MCLK_PLL_INTERNAL_MODE 0
#define AD1937_MCLK_DIRECT_MODE 1

enum AUDIO_INTERFACE_FORMAT{
	AUDIO_INTERFACE_I2S_FORMAT,
	AUDIO_INTERFACE_LJM_FORMAT,
	AUDIO_INTERFACE_RJM_FORMAT,
	AUDIO_INTERFACE_DSP_FORMAT,
	AUDIO_INTERFACE_PCM_FORMAT,
	AUDIO_INTERFACE_NW_FORMAT,
	AUDIO_INTERFACE_TDM_FORMAT,
	AUDIO_INTERFACE_TOTAL_FORMAT
};

typedef struct AD1937_EXTRA_INFO{
	unsigned int codecId;
	unsigned int clkgenId;
	unsigned int dacMasterEn;
	unsigned int daisyEn;
	unsigned int mclk_mode;
} AD1937_EXTRA_INFO;

struct ahub_unit_fpga {
	unsigned int configured;
	void __iomem *ape_fpga_misc_base;
	void __iomem *ape_fpga_misc_i2s_clk_base[5];
	void __iomem *ape_i2c_base;
	void __iomem *pinmux_base;
	void __iomem *ape_gpio_base;
	void __iomem *rst_clk_base;
	void __iomem *i2s5_cya_base;
};

void i2c_write(u32 addr, u32 regAddrr, u32 regData, u32 NoBytes);
u32 i2c_read(u32 addr, u32 regAddrr);
void i2s_clk_divider(u32 i2s, u32 Divider);
void i2c_clk_divider(u32 Divider);
void program_max_codec(void);
void program_cdc_pll(u32 PLLno, u32 Freq);
void i2s_clk_setup(u32 i2s, u32 source, u32 divider);
void i2c_pinmux_setup(void);
void i2c_clk_setup(u32 divider);
void i2s_pinmux_setup(u32 i2s, u32 i2s_b);
void program_io_expander(void);
void program_dmic_gpio(void);
void program_dmic_clk(int dmic_clk);
void SetMax9485(int freq);
void ahub_unit_fpga_init(void);
void ahub_unit_fpga_deinit(void);
struct ahub_unit_fpga *get_ahub_unit_fpga_private(void);

void OnAD1937CaptureAndPlayback(int mode,
	int codec_data_format,
	int codec_data_width,
	int bitSize,
	int polarity,
	int bitclkInv,
	int frameRate,
	AD1937_EXTRA_INFO * extra_info);
#endif
