/*
 * mods_clock.h - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2015-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MODS_CLOCK_H_
#define __MODS_CLOCK_H_

#define MAX_NAME_LEN 32

struct clock_details {
	u32 mhandle;
	u32 phandle;
	u32 clk_uid;
	u32 rst_uid;
	char name[MAX_NAME_LEN];
};

#ifdef CONFIG_ARCH_TEGRA_18x_SOC
	#include <dt-bindings/clock/tegra186-clock.h>
	#include <dt-bindings/reset/tegra186-reset.h>

struct clock_details clock_list[] = {
	/* Reference clocks */
	{0, -1, TEGRA186_CLK_OSC, -1, "osc"},
	{0, -1, TEGRA186_CLK_CLK_M, -1, "clk_m"},
	{0, -1, TEGRA186_CLK_CLK_32K, -1, "clk_32k"},
	{0, -1, TEGRA186_CLK_PLL_REF, -1, "pll_ref"},
	/* UART clocks */
	{0, -1, TEGRA186_CLK_UARTA, TEGRA186_RESET_UARTA, "uarta"},
	{0, -1, TEGRA186_CLK_UARTB, TEGRA186_RESET_UARTB, "uartb"},
	{0, -1, TEGRA186_CLK_UARTC, TEGRA186_RESET_UARTC, "uartc"},
	{0, -1, TEGRA186_CLK_UARTD, TEGRA186_RESET_UARTD, "uartd"},
	{0, -1, TEGRA186_CLK_UARTE, TEGRA186_RESET_UARTE, "uarte"},
	{0, -1, TEGRA186_CLK_UARTF, TEGRA186_RESET_UARTF, "uartf"},
	{0, -1, TEGRA186_CLK_UARTG, TEGRA186_RESET_UARTG, "uartg"},
	/* APE clocks */
	{0, -1, TEGRA186_CLK_AHUB, -1, "ahub"},
	{0, -1, TEGRA186_CLK_APB2APE, -1, "apb2ape"},
	{0, -1, TEGRA186_CLK_APE, TEGRA186_RESET_APE, "ape"},
	{0, -1, TEGRA186_CLK_I2S1, -1, "i2s1"},
	{0, -1, TEGRA186_CLK_I2S2, -1, "i2s2"},
	{0, -1, TEGRA186_CLK_I2S3, -1, "i2s3"},
	{0, -1, TEGRA186_CLK_I2S4, -1, "i2s4"},
	{0, -1, TEGRA186_CLK_I2S5, -1, "i2s5"},
	{0, -1, TEGRA186_CLK_I2S6, -1, "i2s6"},
	{0, -1, TEGRA186_CLK_DMIC1, -1, "dmic1"},
	{0, -1, TEGRA186_CLK_DMIC2, -1, "dmic2"},
	{0, -1, TEGRA186_CLK_DMIC3, -1, "dmic3"},
	{0, -1, TEGRA186_CLK_DMIC4, -1, "dmic4"},
	{0, -1, TEGRA186_CLK_DMIC5, TEGRA186_RESET_DMIC5, "dmic5"},
	{0, -1, TEGRA186_CLK_SPDIF_IN, -1, "spdif_in"},
	{0, -1, TEGRA186_CLK_SPDIF_OUT, -1, "spdif_out"},
	{0, -1, TEGRA186_CLK_DSPK1, -1, "dspk1"},
	{0, -1, TEGRA186_CLK_DSPK2, -1, "dspk2"},
	{0, -1, TEGRA186_CLK_IQC1,  -1, "iqc1"},
	{0, -1, TEGRA186_CLK_IQC2,  -1, "iqc2"},
	{0, -1, TEGRA186_CLK_AUD_MCLK, TEGRA186_RESET_AUD_MCLK, "aud_mclk"},
	/* Camera clocks */
	{0, -1, TEGRA186_CLK_NVCSI, TEGRA186_RESET_NVCSI, "nvcsi"},
	{0, -1, TEGRA186_CLK_VI, TEGRA186_RESET_VI, "vi"},
	{0, -1, TEGRA186_CLK_ISP, TEGRA186_RESET_ISP, "isp"},
	{0, -1, TEGRA186_CLK_EXTPERIPH1, TEGRA186_RESET_EXTPERIPH1, "extperiph1"},
	{0, -1, TEGRA186_CLK_EXTPERIPH2, TEGRA186_RESET_EXTPERIPH2, "extperiph2"},
	{0, -1, TEGRA186_CLK_EXTPERIPH3, TEGRA186_RESET_EXTPERIPH3, "extperiph3"},
	{0, -1, TEGRA186_CLK_EXTPERIPH4, TEGRA186_RESET_EXTPERIPH4, "extperiph4"},
	/* AHCI clocks */
	{0, -1, TEGRA186_CLK_SATA, TEGRA186_RESET_SATA, "sata"},
	{0, -1, TEGRA186_CLK_SATA_OOB, -1, "sata_oob"},
	{0, -1, TEGRA186_CLK_SATA_IOBIST, -1, "sata_iobist"},
	/* QSPI clock */
	{0, -1, TEGRA186_CLK_QSPI, TEGRA186_RESET_QSPI, "qspi"},
	/* SPI clocks */
	{0, -1, TEGRA186_CLK_SPI1, TEGRA186_RESET_SPI1, "spi1"},
	{0, -1, TEGRA186_CLK_SPI2, TEGRA186_RESET_SPI2, "spi2"},
	{0, -1, TEGRA186_CLK_SPI3, TEGRA186_RESET_SPI3, "spi3"},
	{0, -1, TEGRA186_CLK_SPI4, TEGRA186_RESET_SPI4, "spi4"},
	/* I2C clocks */
	{0, -1, TEGRA186_CLK_I2C1, TEGRA186_RESET_I2C1, "i2c1"},
	{0, -1, TEGRA186_CLK_I2C2, TEGRA186_RESET_I2C2, "i2c2"},
	{0, -1, TEGRA186_CLK_I2C3, TEGRA186_RESET_I2C3, "i2c3"},
	{0, -1, TEGRA186_CLK_I2C4, TEGRA186_RESET_I2C4, "i2c4"},
	{0, -1, TEGRA186_CLK_I2C5, TEGRA186_RESET_I2C5, "i2c5"},
	{0, -1, TEGRA186_CLK_I2C6, TEGRA186_RESET_I2C6, "i2c6"},
	{0, -1, TEGRA186_CLK_I2C7, TEGRA186_RESET_I2C7, "i2c7"},
	{0, -1, TEGRA186_CLK_I2C8, TEGRA186_RESET_I2C8, "i2c8"},
	{0, -1, TEGRA186_CLK_I2C9, TEGRA186_RESET_I2C9, "i2c9"},
	{0, -1, TEGRA186_CLK_I2C10, TEGRA186_RESET_I2C10, "i2c10"},
	{0, -1, TEGRA186_CLK_I2C12, TEGRA186_RESET_I2C12, "i2c12"},
	{0, -1, TEGRA186_CLK_I2C13, TEGRA186_RESET_I2C13, "i2c13"},
	{0, -1, TEGRA186_CLK_I2C14, TEGRA186_RESET_I2C14, "i2c14"},
	/* SDMMC clocks */
	{0, -1, TEGRA186_CLK_SDMMC1, TEGRA186_RESET_SDMMC1, "sdmmc1"},
	{0, -1, TEGRA186_CLK_SDMMC2, TEGRA186_RESET_SDMMC2, "sdmmc2"},
	{0, -1, TEGRA186_CLK_SDMMC3, TEGRA186_RESET_SDMMC3, "sdmmc3"},
	{0, -1, TEGRA186_CLK_SDMMC4, TEGRA186_RESET_SDMMC4, "sdmmc4"},
	{0, -1, TEGRA186_CLK_SDMMC_LEGACY_TM, -1, "sdmmc_legacy_tm"},
	/* XUSB clocks */
	{0, -1, TEGRA186_CLK_XUSB, -1, "xusb"},
	{0, -1, TEGRA186_CLK_XUSB_DEV, TEGRA186_RESET_XUSB_DEV, "xusb_dev"},
	{0, -1, TEGRA186_CLK_XUSB_HOST, TEGRA186_RESET_XUSB_HOST, "xusb_host"},
	{0, -1, TEGRA186_CLK_XUSB_SS, TEGRA186_RESET_XUSB_SS, "xusb_ss"},
	{0, -1, TEGRA186_CLK_XUSB_CORE_SS, -1, "xusb_core_ss"},
	{0, -1, TEGRA186_CLK_XUSB_CORE_DEV, -1, "xusb_core_dev"},
	{0, -1, TEGRA186_CLK_XUSB_FALCON, -1, "xusb_falcon"},
	{0, -1, TEGRA186_CLK_XUSB_FS, -1, "xusb_fs"},
	/* GPCDMA clock */
	{0, -1, TEGRA186_CLK_AXI_CBB, TEGRA186_RESET_AXI_CBB, "axi_cbb"},
	/* PWM clocks */
	{0, -1, TEGRA186_CLK_PWM1, TEGRA186_RESET_PWM1, "pwm1"},
	{0, -1, TEGRA186_CLK_PWM2, TEGRA186_RESET_PWM2, "pwm2"},
	{0, -1, TEGRA186_CLK_PWM3, TEGRA186_RESET_PWM3, "pwm3"},
	{0, -1, TEGRA186_CLK_PWM4, TEGRA186_RESET_PWM4, "pwm4"},
	{0, -1, TEGRA186_CLK_PWM5, TEGRA186_RESET_PWM5, "pwm5"},
	{0, -1, TEGRA186_CLK_PWM6, TEGRA186_RESET_PWM6, "pwm6"},
	{0, -1, TEGRA186_CLK_PWM7, TEGRA186_RESET_PWM7, "pwm7"},
	{0, -1, TEGRA186_CLK_PWM8, TEGRA186_RESET_PWM8, "pwm8"},
	/* EMC clock */
	{0, -1, TEGRA186_CLK_EMC, -1, "emc"},
	/* Display clocks */
	{0, -1, TEGRA186_CLK_NVDISPLAY_P0, -1, "nvdisplay_p0"},
	{0, -1, TEGRA186_CLK_NVDISPLAY_P1, -1, "nvdisplay_p1"},
	{0, -1, TEGRA186_CLK_NVDISPLAY_P2, -1, "nvdisplay_p2"},
	/* UFS clocks */
	{0, -1, TEGRA186_CLK_UFSHC, TEGRA186_RESET_UFSHC, "ufshc"},
	{0, -1, TEGRA186_CLK_UFSDEV_REF, -1, "ufsdev_ref"},
	/* MPHY clocks */
	{0, -1, TEGRA186_CLK_MPHY_L0_RX_SYMB, -1, "mphy_l0_rx_symb"},
	{0, -1, TEGRA186_CLK_MPHY_L0_RX_LS_BIT, -1, "mphy_l0_rx_ls_bit"},
	{0, -1, TEGRA186_CLK_MPHY_L0_TX_SYMB, -1, "mphy_l0_tx_symb"},
	{0, -1, TEGRA186_CLK_MPHY_L0_TX_LS_3XBIT, -1, "mphy_l0_tx_ls_3xbit"},
	{0, -1, TEGRA186_CLK_MPHY_L0_RX_ANA, -1, "mphy_l0_rx_ana"},
	{0, -1, TEGRA186_CLK_MPHY_L1_RX_ANA, -1, "mphy_l1_rx_ana"},
	{0, -1, TEGRA186_CLK_MPHY_IOBIST, TEGRA186_RESET_MPHY_IOBIST, "mphy_iobist"},
	{0, -1, TEGRA186_CLK_MPHY_TX_1MHZ_REF, -1, "mphy_tx_1mhz_ref"},
	{0, -1, TEGRA186_CLK_MPHY_CORE_PLL_FIXED, -1, "mphy_core_pll_fixed"},
	/* UPHY clocks */
	{0, -1, TEGRA186_CLK_UPHY_PLL0_PWRSEQ, -1, "uphy_pll0_pwrseq"},
	{0, -1, TEGRA186_CLK_UPHY_PLL1_PWRSEQ, -1, "uphy_pll1_pwrseq"},
	/* PEX clocks */
	{0, -1, TEGRA186_CLK_PEX_SATA_USB_RX_BYP, -1, "pex_sata_usb_rx_byp"},
	{0, -1, TEGRA186_CLK_PEX_USB_PAD0_MGMT, -1, "pex_usb_pad0_mgmt"},
	{0, -1, TEGRA186_CLK_PEX_USB_PAD1_MGMT, -1, "pex_usb_pad1_mgmt"},
	/* Tach clock */
	{0, -1, TEGRA186_CLK_TACH, TEGRA186_RESET_TACH, "tach"},
	/* PLLP variants */
	{0, -1, TEGRA186_CLK_PLLP_OUT0, -1, "pllp_out0" },
	{0, -1, TEGRA186_CLK_PLLP_OUT5, -1, "pllp_out5"},
	{0, -1, TEGRA186_CLK_PLLP, -1, "pllp"},
	{0, -1, TEGRA186_CLK_PLLP_DIV8, -1, "pllp_div8"},
	/* PLLA variants */
	{0, -1, TEGRA186_CLK_PLLA, -1, "plla"},
	{0, -1, TEGRA186_CLK_PLLA1, -1, "plla1"},
	{0, -1, TEGRA186_CLK_PLL_A_OUT0, -1, "pll_a_out0"},
	{0, -1, TEGRA186_CLK_PLL_A_OUT1, -1, "pll_a_out1"},
	/* PLLC clocks */
	{0, -1, TEGRA186_CLK_PLLC, -1, "pllc" },
	{0, -1, TEGRA186_CLK_PLLC_OUT_ISP, -1, "pllc_out_isp"},
	{0, -1, TEGRA186_CLK_PLLC_OUT_VE, -1, "pllc_out_ve" },
	{0, -1, TEGRA186_CLK_PLLC_OUT_AON, -1, "pllc_out_aon"},
	/* PLLC2 clocks */
	{0, -1, TEGRA186_CLK_PLLC2, -1, "pllc2"},
	/* PLLC3 clocks */
	{0, -1, TEGRA186_CLK_PLLC3, -1, "pllc3"},
	/* PLLC4 clocks */
	{0, -1, TEGRA186_CLK_PLLC4_VCO, -1, "pllc4_vco"},
	{0, -1, TEGRA186_CLK_PLLC4_OUT, -1, "pllc4_out"},
	{0, -1, TEGRA186_CLK_PLLC4_OUT0, -1, "pllc4_out0"},
	{0, -1, TEGRA186_CLK_PLLC4_OUT1, -1, "pllc4_out1"},
	{0, -1, TEGRA186_CLK_PLLC4_OUT2, -1, "pllc4_out2"},
	{0, -1, TEGRA186_CLK_PLLC4_OUT_MUX, -1, "pllc4_out_mux"},
	/* PLLD clocks */
	{0, -1, TEGRA186_CLK_PLLD, -1, "pll_d"},
	{0, -1, TEGRA186_CLK_PLLD_OUT1, -1, "pll_d_out1"},
	{0, -1, TEGRA186_CLK_PLLD2, -1, "pll_d2"},
	{0, -1, TEGRA186_CLK_PLLD3, -1, "pll_d3"},
	{0, -1, TEGRA186_CLK_PLLDP, -1, "pll_dp"},
	/* PLLE clocks */
	{0, -1, TEGRA186_CLK_PLLE, -1, "plle"},
	/* PLL_REF_E variants */
	{0, -1, TEGRA186_CLK_PLLREFE_OUT, -1, "pllrefe_out"},
	{0, -1, TEGRA186_CLK_PLLREFE_PLL_REF, -1, "pllrefe_pll_ref"},
	{0, -1, TEGRA186_CLK_PLLREFE_OUT_GATED, -1, "pllrefe_out_gated"},
	{0, -1, TEGRA186_CLK_PLLREFE_OUT1, -1, "pllrefe_out1"},
	{0, -1, TEGRA186_CLK_PLLREFE_VCO, -1, "pllrefe_vco"},
	/* UTMIPLL/PLLU clocks */
	{0, -1, TEGRA186_CLK_PLLU, -1, "pllu"},
	{0, -1, TEGRA186_CLK_PLL_U_48M,  -1, "pllu_48M"},
	{0, -1, TEGRA186_CLK_PLL_U_480M, -1, "pllu_480M"},
};
#endif /* T18x SOC */

#endif /* __MODS_CLOCK_H_ */
