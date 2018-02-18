/*
 * ahub_unit_fpga_clock.c
 *
 * Author: Dara Ramesh <dramesh@nvidia.com>
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */


#define VERBOSE_DEBUG
#define DEBUG
#include <linux/module.h>
#include <asm/types.h>
#include <../arch/arm/mach-tegra/iomap.h>
#include <linux/io.h>
#include <linux/printk.h>
#include "ahub_unit_fpga_clock.h"
#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
#include <sound/tegra_audio.h>
#endif

static struct ahub_unit_fpga ahub_unit_fpga_private;

static void __iomem *pinmux_base = IO_ADDRESS(NV_ADDRESS_MAP_APB_PP_BASE);
static void __iomem *ahub_gpio_base = IO_ADDRESS(NV_ADDRESS_MAP_APE_AHUB_GPIO_BASE);

static void __iomem *rst_clk_base = IO_ADDRESS(NV_ADDRESS_MAP_PPSB_CLK_RST_BASE);
static void __iomem *i2s5_base = IO_ADDRESS(NV_ADDRESS_MAP_APE_I2S5_BASE);

static unsigned int ahub_fpga_misc_i2s_offset[5] = {
	APE_FPGA_MISC_CLK_SOURCE_I2S1_0,
	APE_FPGA_MISC_CLK_SOURCE_I2S2_0,
	APE_FPGA_MISC_CLK_SOURCE_I2S3_0,
	APE_FPGA_MISC_CLK_SOURCE_I2S4_0,
	APE_FPGA_MISC_CLK_SOURCE_I2S5_0
};

static unsigned int i2s_source_clock[5] = {
	CLK_RST_CONTROLLER_CLK_SOURCE_I2S1_0,
	CLK_RST_CONTROLLER_CLK_SOURCE_I2S2_0,
	CLK_RST_CONTROLLER_CLK_SOURCE_I2S3_0,
	CLK_RST_CONTROLLER_CLK_SOURCE_I2S4_0,
	CLK_RST_CONTROLLER_CLK_SOURCE_I2S5_0,
};

static unsigned char cdce906_setting[27] = {
	0,
	0x77,
	0x20,
	0x6d,
	1,
	1,
	0x80,
	1,
	1,
	0x20,
	0x0F,
	0x0,
	0x0,
	0x5,
	0x1,
	0x1,
	0x1,
	0x1,
	0x1,
	0x38,
	0x31,
	0x31,
	0x31,
	0x31,
	0x31,
	0x0,
	0x0,
};
void i2s_clk_divider(u32 i2s, u32 divider)
{
	void __iomem *ape_fpga_misc_i2s_offset =
		ahub_unit_fpga_private.ape_fpga_misc_i2s_clk_base[i2s];
	void __iomem *ape_fpga_misc_i2s5_cya_offset =
		ahub_unit_fpga_private.i2s5_cya_base;
	u32 write_data, read_data;

	write_data = ((divider << 1) | (1 << 28));
	writel(write_data, ape_fpga_misc_i2s_offset);
	read_data = readl(ape_fpga_misc_i2s_offset);
	if (i2s == I2S5)
		writel(1, ape_fpga_misc_i2s5_cya_offset);
#if DEBUG_FPGA
	if (read_data == write_data)
		pr_err("\n DATA MATCH I2S%d 0x%x\n", i2s+1, read_data);
	else
		pr_err("DATA MISMATCH APE_FPGA_MISC_CLK_SOURCE_I2S%d_0 Actual data: 0x%x expected data: 0x%x\n",
			i2s+1, read_data, write_data);
#endif
}
EXPORT_SYMBOL(i2s_clk_divider);

void i2s_clk_setup(u32 i2s, u32 source, u32 divider)
{
	u32 write_data;

	write_data = (source << 29) | (divider);
	writel(write_data, rst_clk_base + i2s_source_clock[i2s]);
}
EXPORT_SYMBOL(i2s_clk_setup);

void i2c_pinmux_setup(void)
{
	/* Pin mux */
	writel(0x40, pinmux_base + PINMUX_AUX_GEN1_I2C_SDA_0);
	writel(0x40, pinmux_base + PINMUX_AUX_GEN1_I2C_SCL_0);
}
EXPORT_SYMBOL(i2c_pinmux_setup);

void i2s_pinmux_setup(u32 i2s, u32 i2s_b)
{
	switch (i2s) {
	case I2S1:
		writel(0x40, pinmux_base + PINMUX_AUX_DAP1_SCLK_0);
		writel(0x40, pinmux_base + PINMUX_AUX_DAP1_FS_0);
		writel(0x40, pinmux_base + PINMUX_AUX_DAP1_DOUT_0);
		writel(0x40, pinmux_base + PINMUX_AUX_DAP1_DIN_0);
		break;
	case I2S2:
		writel(0x40, pinmux_base + PINMUX_AUX_DAP2_SCLK_0);
		writel(0x40, pinmux_base + PINMUX_AUX_DAP2_FS_0);
		writel(0x40, pinmux_base + PINMUX_AUX_DAP2_DOUT_0);
		writel(0x40, pinmux_base + PINMUX_AUX_DAP2_DIN_0);
		break;
	case I2S3:
	case I2S4:
	default:
		break;
	case I2S5:
		writel(0x41, pinmux_base + PINMUX_AUX_GPIO_PK0_0);
		writel(0x41, pinmux_base + PINMUX_AUX_GPIO_PK1_0);
		writel(0x41, pinmux_base + PINMUX_AUX_GPIO_PK2_0);
		writel(0x41, pinmux_base + PINMUX_AUX_GPIO_PK3_0);
		if (i2s_b)
			writel(1, i2s5_base + I2S5_CYA_0);
		break;
	}
}
EXPORT_SYMBOL(i2s_pinmux_setup);

void i2c_clk_setup(u32 divider)
{
	u32 reg_val;

	/* de-assert clk reset */
	reg_val = readl(rst_clk_base + CLK_RST_CONTROLLER_RST_DEVICES_L_0);
	reg_val &= ~(1 << 12);
	writel(reg_val, rst_clk_base + CLK_RST_CONTROLLER_RST_DEVICES_L_0);

	/* enable i2c1 clock */
	reg_val = readl(rst_clk_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0);
	reg_val |= (1 << 12);
	writel(reg_val, rst_clk_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0);

	reg_val = divider << 1;
	writel(reg_val, rst_clk_base + CLK_RST_CONTROLLER_CLK_SOURCE_I2C1_0);
}
EXPORT_SYMBOL(i2c_clk_setup);

void i2c_clk_divider(u32 divider)
{
	u32 write_data, read_data;
	void __iomem *ape_fpga_misc_base =
		ahub_unit_fpga_private.ape_fpga_misc_base;

	write_data = divider << 1;
	writel(write_data,
		ape_fpga_misc_base + APE_FPGA_MISC_CLK_SOURCE_I2C1_0);
	read_data = readl(ape_fpga_misc_base + APE_FPGA_MISC_CLK_SOURCE_I2C1_0);
}
EXPORT_SYMBOL(i2c_clk_divider);

void i2c_write(u32 addr, u32 reg_addr, u32 regData, u32 no_bytes)
{
	u32 write_data, read_data;
	u32 data;
	void __iomem *ape_i2c_base = ahub_unit_fpga_private.ape_i2c_base;

	if (no_bytes == 3) {
		data = (reg_addr & 0xFF) | ((regData & 0xFF) << 16) |
			(((regData & 0xFF00) >> 8) << 8);
	} else {
		data = regData << 8 | reg_addr;
	}

	write_data = addr << 1;
	writel(write_data, ape_i2c_base + I2C_I2C_CMD_ADDR0_0);

	write_data = (no_bytes - 1) << 1;
	writel(write_data, ape_i2c_base + I2C_I2C_CNFG_0);

	write_data = data;
	writel(write_data, ape_i2c_base + I2C_I2C_CMD_DATA1_0);

	write_data = 0x01;
	writel(write_data, ape_i2c_base + I2C_I2C_CONFIG_LOAD_0);

	read_data = readl(ape_i2c_base + I2C_I2C_CONFIG_LOAD_0);

	while (read_data != 0x0) {
		read_data = readl(ape_i2c_base + I2C_I2C_CONFIG_LOAD_0);
	}

	write_data = 0x200 | (no_bytes - 1) << 1;
	writel(write_data, ape_i2c_base + I2C_I2C_CNFG_0);

	/* CHECKING STATUS for Busy status */
	read_data = readl(ape_i2c_base + I2C_I2C_STATUS_0);

	while (read_data & 0x100) { /* wait for master  busy LOW */
		pr_err("\n status before %9x\n", read_data);
		read_data = readl(ape_i2c_base + I2C_I2C_STATUS_0);
	}


	read_data = readl(ape_i2c_base + I2C_I2C_STATUS_0);
	read_data = read_data & 0xFF;
	if (read_data)
		pr_err("\n FAIL NoaCK Received for Reg_add %08x before %9x Byte\n",
			reg_addr, read_data);
}
EXPORT_SYMBOL(i2c_write);

u32 i2c_read(u32 addr, u32 reg_addr)
{
	u32 write_data, read_data;
	void __iomem *ape_i2c_base = ahub_unit_fpga_private.ape_i2c_base;

	write_data = 0x1 << 11 | (0x1 << 4) | (0 << 6) | (1 << 7);
	writel(write_data, ape_i2c_base + I2C_I2C_CNFG_0);

	write_data = addr << 1;
	writel(write_data, ape_i2c_base + I2C_I2C_CMD_ADDR0_0);

	write_data = addr << 1 | 1;
	writel(write_data, ape_i2c_base + I2C_I2C_CMD_ADDR1_0);

	write_data = reg_addr;
	writel(write_data, ape_i2c_base + I2C_I2C_CMD_DATA1_0);

	write_data = 0x1 << 11 | (0x1 << 4) | (0 << 6) | (1 << 7) | (1 << 9);
	writel(write_data, ape_i2c_base + I2C_I2C_CNFG_0); /* Go */

	/* CHECKING STATUS for Busy status */
	read_data = readl(ape_i2c_base + I2C_I2C_STATUS_0);

	while (read_data & 0x100)  { /* wait for master  busy LOW  */
		pr_err("\n status before %9x\n", read_data);
		read_data = readl(ape_i2c_base + I2C_I2C_STATUS_0);
	}

	read_data = readl(ape_i2c_base + I2C_I2C_STATUS_0);
	read_data = read_data & 0xFF;

	if (read_data)
		pr_err("\nNoaCK Received for addr %d before %9x Byte\n",
			reg_addr, read_data);

	read_data = readl(ape_i2c_base + I2C_I2C_CMD_DATA2_0);

	return read_data;
}
EXPORT_SYMBOL(i2c_read);

void program_cdc_pll(u32 pll_no, u32 freq)
{
	unsigned int p = 0, n = 0, m = 0, i = 0;
	u32 slv_add = 0;
	u32 reg_add;

	if (pll_no == 1)
		slv_add = 0x68;
	else if (pll_no == 2)
		slv_add = 0x69;
	else if (pll_no == 3)
		slv_add = 0x6a;
	else if (pll_no == 4)
		slv_add = 0x6b;

	switch (freq) {
	case CLK_OUT_3_0720_MHZ:
		m = 125;
		n = 768;
		p = 54;
		break;
	case CLK_OUT_4_0960_MHZ:
		m = 375;
		n = 2048;
		p = 36;
		break;
	case CLK_OUT_5_6448_MHZ:
		m = 75;
		n = 392;
		p = 25;
		break;
	case CLK_OUT_6_1440_MHZ:
		m = 125;
		n = 768;
		p = 27;
		break;
	case CLK_OUT_8_1920_MHZ:
		m = 375;
		n = 2048;
		p = 18;
		break;
	case CLK_OUT_11_2896_MHZ:
		m = 375;
		n = 1568;
		p = 10;
		break;
	case CLK_OUT_12_2888_MHZ:
		m = 375;
		n = 2048;
		p = 12;
		break;
	case CLK_OUT_16_3840_MHZ:
		m = 375;
		n = 2048;
		p = 9;
		break;
	case CLK_OUT_16_9344_MHZ:
		m = 125;
		n = 784;
		p = 10;
		break;
	case CLK_OUT_18_4320_MHZ:
		m = 125;
		n = 768;
		p = 9;
		break;
	case CLK_OUT_22_5792_MHZ:
		m = 375;
		n = 1568;
		p = 5;
		break;
	case CLK_OUT_24_5760_MHZ:
		m = 375;
		n = 2048;
		p = 6;
		break;
	case CLK_OUT_33_8688_MHZ:
		m = 125;
		n = 784;
		p = 5;
		break;
	case CLK_OUT_36_8640_MHZ:
		m = 375;
		n = 2048;
		p = 4;
		break;
	case CLK_OUT_49_1520_MHZ:
		m = 375;
		n = 2048;
		p = 3;
		break;
	case CLK_OUT_73_7280_MHZ:
		m = 375;
		n = 2048;
		p = 2;
		break;
	case CLK_OUT_162_MHZ:
		m = 1;
		n = 6;
		p = 1;
		break;

	default:
		m = 1;
		break;
	}

	pr_err("\n PLL Parems SLV_ADD 0x%x,  freq  %d   M %d    N  %d  P %d\n",
		slv_add, freq, m, n, p);

	i2c_write(slv_add, (10 | 1 << 7), 0xff, 2);

	cdce906_setting[1] = (unsigned char)m & 0xff;
	cdce906_setting[2] = (unsigned char)n & 0xff;
	cdce906_setting[3] &= ~(0x1f);
	cdce906_setting[3] |= (unsigned char)((((n >> 8) & 0xf) << 1) | ((m >> 8) & 1));
	cdce906_setting[12] |= (0<<6);	/* 0 : power on, 1 : power off */
	cdce906_setting[13] = p;

	for (i = 1; i < 25; i++) {
		pr_err("\n I2C_PLL_PRJ SLV_ADD 0x%x,  REG_ADD 0x%x  REG_DATA 0x%x\n",
			slv_add, i, cdce906_setting[i]);
		reg_add = i | (1 << 7);
		i2c_write(slv_add, reg_add, cdce906_setting[i], 2);
	}
}
EXPORT_SYMBOL(program_cdc_pll);

void program_io_expander(void)
{
	i2c_write(0x20, 0x2, 0x99, 2);
	i2c_write(0x20, 0x6, 0x00, 2);
	i2c_write(0x20, 0x3, 0x99, 2);
	i2c_write(0x20, 0x7, 0x00, 2);
	i2c_write(0x21, 0x2, 0x09, 2);
	i2c_write(0x21, 0x6, 0x00, 2);
}
EXPORT_SYMBOL(program_io_expander);

void program_max_codec(void)
{
	unsigned int i;
	unsigned char read_data;
	/* mclk_freq(system clock) - 0x10 : 12.288Mhz, 0x8 : 12Mhz */
	unsigned char mclk_freq = 0x10;
	/* frame_rate - 0x1 : 8khz, 0x2 : 16khz, 0x10 : 32khz,
		0x4 : 44.1khz, 0x8 : 48khz, 0x20 : 96khz */
	unsigned char frame_rate = 0x1;
	/* codec_data_format - 1 : codec slave, 2 : codec master */
	unsigned char codec_data_format = 1;
	unsigned char codec_slv_add = 0x10;
	unsigned char loopback = 0; /* DIN -> DOUT */
	unsigned int power = 1; /* 1 : on, 0 : off */

#if DEBUG_FPGA
	pr_err("Audio maxim 98090/98091 codec programming\n");
#endif
	i2c_write(codec_slv_add, 0x03, 0x04, 2);
	i2c_write(codec_slv_add, 0x04, mclk_freq, 2);
	i2c_write(codec_slv_add, 0x05, frame_rate, 2);
	i2c_write(codec_slv_add, 0x06, codec_data_format, 2);
	i2c_write(codec_slv_add, 0x07, 0x80, 2);
	i2c_write(codec_slv_add, 0x08, 0x80, 2);
	i2c_write(codec_slv_add, 0x09, 0x80, 2);
	i2c_write(codec_slv_add, 0x07, 0x80, 2);
	i2c_write(codec_slv_add, 0x08, 0x00, 2);
	i2c_write(codec_slv_add, 0x09, 0x20, 2);
	i2c_write(codec_slv_add, 0x0a, 0x00, 2);
	i2c_write(codec_slv_add, 0x0b, 0x00, 2);
	i2c_write(codec_slv_add, 0x0d, 0x03, 2);
	i2c_write(codec_slv_add, 0x0e, 0x1b, 2);
	i2c_write(codec_slv_add, 0x0f, 0x00, 2);
	i2c_write(codec_slv_add, 0x10, 0x00, 2);
	i2c_write(codec_slv_add, 0x11, 0x00, 2);
	i2c_write(codec_slv_add, 0x12, 0x00, 2);
	i2c_write(codec_slv_add, 0x13, 0x00, 2);
	i2c_write(codec_slv_add, 0x14, 0x00, 2);
	i2c_write(codec_slv_add, 0x15, 0x08, 2);
	i2c_write(codec_slv_add, 0x16, 0x10, 2);
	i2c_write(codec_slv_add, 0x17, 0x03, 2);
	i2c_write(codec_slv_add, 0x18, 0x03, 2);
	i2c_write(codec_slv_add, 0x19, 0x00, 2);
	i2c_write(codec_slv_add, 0x1a, 0x00, 2);
	i2c_write(codec_slv_add, 0x1b, 0x10, 2);
	i2c_write(codec_slv_add, 0x1c, 0x00, 2);
	i2c_write(codec_slv_add, 0x1d, 0x00, 2);
	i2c_write(codec_slv_add, 0x1e, 0x00, 2);
	i2c_write(codec_slv_add, 0x1f, 0x00, 2);
	i2c_write(codec_slv_add, 0x20, 0x00, 2);
	i2c_write(codec_slv_add, 0x21,
		((codec_data_format == 1) ? 0x03 : 0x83), 2);
	i2c_write(codec_slv_add, 0x22, 0x04, 2);
	i2c_write(codec_slv_add, 0x23, 0x00, 2);
	i2c_write(codec_slv_add, 0x24, 0x00, 2);
	i2c_write(codec_slv_add, 0x25, (loopback<<4)|0x03, 2);
	i2c_write(codec_slv_add, 0x26, 0x00, 2);
	i2c_write(codec_slv_add, 0x27, 0x00, 2);
	i2c_write(codec_slv_add, 0x28, 0x00, 2);
	i2c_write(codec_slv_add, 0x29, 0x01, 2);
	i2c_write(codec_slv_add, 0x2a, 0x02, 2);
	i2c_write(codec_slv_add, 0x2b, 0x00, 2);
	i2c_write(codec_slv_add, 0x2c, 0x1a, 2);
	i2c_write(codec_slv_add, 0x2d, 0x1a, 2);
	/* speaker settings */
	i2c_write(codec_slv_add, 0x2e, 0x01, 2);
	i2c_write(codec_slv_add, 0x2f, 0x02, 2);
	i2c_write(codec_slv_add, 0x30, 0x00, 2);
	i2c_write(codec_slv_add, 0x31, 0x2c, 2);
	i2c_write(codec_slv_add, 0x32, 0x2c, 2);

	/* head phone settings */
	/* i2c_write(codec_slv_add, 0x2e, 0x20,2, 2); */
	/* i2c_write(codec_slv_add, 0x2f, 0x50,2, 2); */
	/* i2c_write(codec_slv_add, 0x30, 0x00,2, 2); */
	/* i2c_write(codec_slv_add, 0x31, 0x2c,2, 2); */
	/* i2c_write(codec_slv_add, 0x32, 0x2c,2, 2); */

	i2c_write(codec_slv_add, 0x33, 0x00, 2);
	i2c_write(codec_slv_add, 0x34, 0x00, 2);
	i2c_write(codec_slv_add, 0x35, 0x00, 2);
	i2c_write(codec_slv_add, 0x36, 0x00, 2);
	i2c_write(codec_slv_add, 0x37, 0x00, 2);
	i2c_write(codec_slv_add, 0x38, 0x00, 2);
	i2c_write(codec_slv_add, 0x39, 0x95, 2);
	i2c_write(codec_slv_add, 0x3a, 0x00, 2);
	i2c_write(codec_slv_add, 0x3b, 0x00, 2);
	i2c_write(codec_slv_add, 0x3c, 0x15, 2);
	i2c_write(codec_slv_add, 0x3d, 0x00, 2);
	i2c_write(codec_slv_add, 0x3e, 0x0f, 2);
	i2c_write(codec_slv_add, 0x3f, 0xff, 2);
	i2c_write(codec_slv_add, 0x40, 0x00, 2);
	i2c_write(codec_slv_add, 0x41, 0x00, 2);
	i2c_write(codec_slv_add, 0x42, 0x00, 2);
	i2c_write(codec_slv_add, 0x43, 0x01, 2);
	i2c_write(codec_slv_add, 0x44, 0x01, 2);
	i2c_write(codec_slv_add, 0x45, power<<7, 2);

	for (i = 1; i < 0x50; i++) {
		read_data = i2c_read(codec_slv_add, i);
#if DEBUG_FPGA
		pr_err(
			"I2C_MAX_CODEC_READ Reg_Value 0x%x, ADD 0x%x Value 0x%x\n",
			codec_slv_add, i, read_data);
#endif
	}
}
EXPORT_SYMBOL(program_max_codec);
/* DMIC */

void program_dmic_gpio(void)
{
#if !SYSTEM_FPGA
	writel(0x1, ahub_gpio_base + APE_AHUB_GPIO_CNF_0);
	writel(0x1, ahub_gpio_base + APE_AHUB_GPIO_OE_0);
	writel(0x1, ahub_gpio_base + APE_AHUB_GPIO_OUT_0);
#endif
}
EXPORT_SYMBOL(program_dmic_gpio);
void program_dmic_clk(int dmic_clk)
{
	void __iomem *misc_base = ahub_unit_fpga_private.ape_fpga_misc_base;
#if !SYSTEM_FPGA
	writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_I2C1_0);

	switch (dmic_clk) {
	case 256000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0xf, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 512000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0x7, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 1024000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0x3, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 1411200:
		program_cdc_pll(2, CLK_OUT_5_6448_MHZ);
		writel(0x3, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 1536000:
		program_cdc_pll(2, CLK_OUT_6_1440_MHZ);
		writel(0x3, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 2048000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 2822400:
		program_cdc_pll(2, CLK_OUT_5_6448_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 3072000:
		program_cdc_pll(2, CLK_OUT_6_1440_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 4096000:
		program_cdc_pll(2, CLK_OUT_4_0960_MHZ);
		writel(0x0, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 5644800:
		program_cdc_pll(2, CLK_OUT_11_2896_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 6144000:
		program_cdc_pll(2, CLK_OUT_12_2888_MHZ);
		writel(0x1, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 11289600:
		program_cdc_pll(2, CLK_OUT_11_2896_MHZ);
		writel(0x0, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	case 12288000:
		program_cdc_pll(2, CLK_OUT_12_2888_MHZ);
		writel(0x0, misc_base + APE_FPGA_MISC_CLK_SOURCE_DMIC1_0);
		break;
	default:
		pr_err("Unsupported sample rate and OSR combination\n");
	}
	#endif
}
EXPORT_SYMBOL(program_dmic_clk);
/**
 * AD1937
 */
static unsigned char ad1937x_enable[17] = {
	/* AD1937_PLL_CLK_CTRL_0 */
	AD1937_PLL_CLK_CTRL_0_PWR_OFF |
	AD1937_PLL_CLK_CTRL_0_MCLKI_512 |
	AD1937_PLL_CLK_CTRL_0_MCLKO_OFF |
	AD1937_PLL_CLK_CTRL_0_PLL_INPUT_MCLKI |
	AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_ENABLE,

	/* AD1937_PLL_CLK_CTRL_1 */
	AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL |
	AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL |
	AD1937_PLL_CLK_CTRL_1_VREF_ENABLE,

	/* AD1937_DAC_CTRL_0 */
	AD1937_DAC_CTRL_0_PWR_ON |
	AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ |
	AD1937_DAC_CTRL_0_DSDATA_DELAY_1 |
	AD1937_DAC_CTRL_0_FMT_STEREO,

	/* AD1937_DAC_CTRL_1 */
	AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MIDCYCLE |
	AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_256 |
	AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_LOW |
	AD1937_DAC_CTRL_1_DLRCLK_SLAVE |
	AD1937_DAC_CTRL_1_DBCLK_SLAVE |
	AD1937_DAC_CTRL_1_DBCLK_SOURCE_DBCLK_PIN |
	AD1937_DAC_CTRL_1_DBCLK_POLARITY_NORMAL,

	/* AD1937_DAC_CTRL_2 */
	AD1937_DAC_CTRL_2_MASTER_UNMUTE |
	AD1937_DAC_CTRL_2_DEEMPHASIS_FLAT |
	AD1937_DAC_CTRL_2_WORD_WIDTH_16_BITS |          /* sample size */
	AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_NORMAL,

	/* AD1937_DAC_MUTE_CTRL */
	AD1937_DAC_MUTE_CTRL_DAC1L_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC1R_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC2L_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC2R_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC3L_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC3R_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC4L_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC4R_UNMUTE,

	/* AD1937_DAC_VOL_CTRL_DAC1L */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC1R */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC2L */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC2R */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC3L */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC3R */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC4L */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC4R */
	0,

	/* AD1937_ADC_CTRL_0 */
	AD1937_ADC_CTRL_0_PWR_OFF |
	AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_ON |
	AD1937_ADC_CTRL_0_ADC1L_MUTE |
	AD1937_ADC_CTRL_0_ADC1R_MUTE |
	AD1937_ADC_CTRL_0_ADC2L_MUTE |
	AD1937_ADC_CTRL_0_ADC2R_MUTE |
	AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ,

	/* AD1937_ADC_CTRL_1 */
	AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS |          /* sample size */
	AD1937_ADC_CTRL_1_ASDATA_DELAY_1 |
	AD1937_ADC_CTRL_1_FMT_STEREO |
	AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_PIPELINE,

	/* AD1937_ADC_CTRL_2 */
	AD1937_ADC_CTRL_2_ALRCLK_FMT_50_50 |
	AD1937_ADC_CTRL_2_ABCLK_POLARITY_RISING_EDGE |
	AD1937_ADC_CTRL_2_ALRCLK_POLARITY_LEFT_LOW |
	AD1937_ADC_CTRL_2_ALRCLK_SLAVE |
	AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_64 |
	AD1937_ADC_CTRL_2_ABCLK_SLAVE |
	AD1937_ADC_CTRL_2_ABCLK_SOURCE_ABCLK_PIN
};

/**
 * AD1937
 */
static unsigned char ad1937y_enable[17] = {
	/* AD1937_PLL_CLK_CTRL_0 */
	AD1937_PLL_CLK_CTRL_0_PWR_OFF |
	AD1937_PLL_CLK_CTRL_0_MCLKI_512 |
	AD1937_PLL_CLK_CTRL_0_MCLKO_OFF |
	AD1937_PLL_CLK_CTRL_0_PLL_INPUT_MCLKI |
	AD1937_PLL_CLK_CTRL_0_INTERNAL_MASTER_CLK_ENABLE,

	/* AD1937_PLL_CLK_CTRL_1 */
	AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL |
	AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL |
	AD1937_PLL_CLK_CTRL_1_VREF_ENABLE,

	/* AD1937_DAC_CTRL_0 */
	AD1937_DAC_CTRL_0_PWR_ON |
	AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ |
	AD1937_DAC_CTRL_0_DSDATA_DELAY_1 |
	AD1937_DAC_CTRL_0_FMT_STEREO,

	/* AD1937_DAC_CTRL_1 */
	AD1937_DAC_CTRL_1_DBCLK_ACTIVE_EDGE_MIDCYCLE |
	AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_256 |
	AD1937_DAC_CTRL_1_DLRCLK_POLARITY_LEFT_LOW |
	AD1937_DAC_CTRL_1_DLRCLK_SLAVE |
	AD1937_DAC_CTRL_1_DBCLK_SLAVE |
	AD1937_DAC_CTRL_1_DBCLK_SOURCE_DBCLK_PIN |
	AD1937_DAC_CTRL_1_DBCLK_POLARITY_NORMAL,

	/* AD1937_DAC_CTRL_2 */
	AD1937_DAC_CTRL_2_MASTER_UNMUTE |
	AD1937_DAC_CTRL_2_DEEMPHASIS_FLAT |
	AD1937_DAC_CTRL_2_WORD_WIDTH_16_BITS |          /* sample size */
	AD1937_DAC_CTRL_2_DAC_OUTPUT_POLARITY_NORMAL,

	/* AD1937_DAC_MUTE_CTRL */
	AD1937_DAC_MUTE_CTRL_DAC1L_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC1R_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC2L_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC2R_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC3L_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC3R_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC4L_UNMUTE |
	AD1937_DAC_MUTE_CTRL_DAC4R_UNMUTE,

	/* AD1937_DAC_VOL_CTRL_DAC1L */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC1R */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC2L */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC2R */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC3L */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC3R */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC4L */
	0,

	/* AD1937_DAC_VOL_CTRL_DAC4R */
	0,

	/* AD1937_ADC_CTRL_0 */
	AD1937_ADC_CTRL_0_PWR_OFF |
	AD1937_ADC_CTRL_0_HIGH_PASS_FILTER_OFF |
	AD1937_ADC_CTRL_0_ADC1L_MUTE |
	AD1937_ADC_CTRL_0_ADC1R_MUTE |
	AD1937_ADC_CTRL_0_ADC2L_MUTE |
	AD1937_ADC_CTRL_0_ADC2R_MUTE |
	AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ,

	/* AD1937_ADC_CTRL_1 */
	AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS |          /* sample size */
	AD1937_ADC_CTRL_1_ASDATA_DELAY_1 |
	AD1937_ADC_CTRL_1_FMT_STEREO |
	AD1937_ADC_CTRL_1_ABCLK_ACTIVE_EDGE_MIDCYCLE,

	/* AD1937_ADC_CTRL_2 */
	AD1937_ADC_CTRL_2_ALRCLK_FMT_50_50 |
	AD1937_ADC_CTRL_2_ABCLK_POLARITY_RISING_EDGE |
	AD1937_ADC_CTRL_2_ALRCLK_POLARITY_LEFT_LOW |
	AD1937_ADC_CTRL_2_ALRCLK_SLAVE |
	AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_64 |
	AD1937_ADC_CTRL_2_ABCLK_SLAVE |
	AD1937_ADC_CTRL_2_ABCLK_SOURCE_ABCLK_PIN
};

static unsigned char *ad1937_enable[2] = {ad1937x_enable, ad1937y_enable};

void SetMax9485(int freq)
{
	unsigned char max9485_value;

	switch (freq) {
	default:
	case CLK_OUT_12_2888_MHZ:
		max9485_value = MAX9485_MCLK_FREQ_122880;
		break;
	case CLK_OUT_11_2896_MHZ:
		max9485_value = MAX9485_MCLK_FREQ_112896;
		break;
	case CLK_OUT_24_5760_MHZ:
		max9485_value = MAX9485_MCLK_FREQ_245760;
		break;
	case CLK_OUT_22_5792_MHZ:
		max9485_value = MAX9485_MCLK_FREQ_225792;
		break;
	}

	i2c_write(MAX9485_DEVICE_ADDRESS, max9485_value, 0, 1);
}
EXPORT_SYMBOL(SetMax9485);

void OnAD1937CaptureAndPlayback(int mode,
	int codec_data_format,
	int codec_data_width,
	int bit_size,
	int polarity,
	int bitclk_inv,
	int frame_rate,
	AD1937_EXTRA_INFO *pExtraInfo)
{
	int i;
	int target_freq = CLK_OUT_24_5760_MHZ; /* default */
	int codec_idx;

	codec_idx = (pExtraInfo->codecId == AD1937_X_ADDRESS) ? 0 : 1;

	ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] &= ~(AD1937_PLL_CLK_CTRL_0_MCLKI_MASK);
	ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] &= ~(AD1937_PLL_CLK_CTRL_0_PWR_MASK);
	ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] &= ~(AD1937_PLL_CLK_CTRL_1_DAC_CLK_MASK);
	ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] &= ~(AD1937_PLL_CLK_CTRL_1_ADC_CLK_MASK);
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] &= ~(AD1937_DAC_CTRL_0_SAMPLE_RATE_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] &= ~(AD1937_ADC_CTRL_0_SAMPLE_RATE_MASK);

	switch (frame_rate) {
	case AUDIO_SAMPLE_RATE_8_00:
		target_freq = CLK_OUT_4_0960_MHZ; /* direct MCLK mode */
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_PWR_OFF;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK;

		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_16_00:
		target_freq = CLK_OUT_8_1920_MHZ; /* direct MCLK mode */
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_PWR_OFF;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK;

		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_32_00:
		if (pExtraInfo->mclk_mode == AD1937_MCLK_DIRECT_MODE) {
			/* direct MCLK mode */
			target_freq = CLK_OUT_16_3840_MHZ;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_PWR_OFF;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK;
		} else {
			/* internal PLL mode */
			target_freq = CLK_OUT_8_1920_MHZ;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_PWR_ON;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_MCLKI_256;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;
		}
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_44_10:
		if (pExtraInfo->mclk_mode == AD1937_MCLK_DIRECT_MODE) {
			/* direct MCLK mode */
			target_freq = CLK_OUT_22_5792_MHZ;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_PWR_OFF;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK;
		} else {
			/* internal PLL mode */
			target_freq = CLK_OUT_11_2896_MHZ;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_PWR_ON;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_MCLKI_256;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;
		}
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_48_00:
		if (pExtraInfo->mclk_mode == AD1937_MCLK_DIRECT_MODE) {
			/* direct MCLK mode */
			target_freq = CLK_OUT_24_5760_MHZ;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_PWR_OFF;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				(bit_size != 512) ? 0 :
					AD1937_PLL_CLK_CTRL_0_MCLKI_512;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_DAC_CLK_MCLK;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_ADC_CLK_MCLK;
		} else {
			/* internal PLL mode */
			target_freq = CLK_OUT_12_2888_MHZ;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_PWR_ON;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
				AD1937_PLL_CLK_CTRL_0_PLL_INPUT_DLRCLK;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
			ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
				AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;
		}
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_32_44_1_48_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_64_00:
		/* internal PLL mode */
		target_freq = CLK_OUT_16_3840_MHZ;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_PWR_ON;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_MCLKI_512;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;

		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_88_20:
		/* internal PLL mode */
		target_freq = CLK_OUT_22_5792_MHZ;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_PWR_ON;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_MCLKI_512;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;

		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_96_00:
		/* internal PLL mode */
		target_freq = CLK_OUT_24_5760_MHZ;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_PWR_ON;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_MCLKI_512;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;

		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_64_88_2_96_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_128_00:
		/* internal PLL mode */
		target_freq = CLK_OUT_16_3840_MHZ;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_PWR_ON;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_MCLKI_512;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;

		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_176_40:
		/* internal PLL mode */
		target_freq = CLK_OUT_22_5792_MHZ;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_PWR_ON;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_MCLKI_512;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;

		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ;
		break;
	case AUDIO_SAMPLE_RATE_192_00:
		/* internal PLL mode */
		target_freq = CLK_OUT_24_5760_MHZ;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_PWR_ON;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |=
			AD1937_PLL_CLK_CTRL_0_MCLKI_512;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_DAC_CLK_PLL;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_1] |=
			AD1937_PLL_CLK_CTRL_1_ADC_CLK_PLL;

		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] |=
			AD1937_DAC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |=
			AD1937_ADC_CTRL_0_SAMPLE_RATE_128_176_4_192_KHZ;
		break;
	default:
		break;
	}

	{
	/* slave mode clock setting */
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] &=
			~(AD1937_DAC_CTRL_1_DLRCLK_MASK |
			AD1937_DAC_CTRL_1_DBCLK_MASK |
			AD1937_DAC_CTRL_1_DBCLK_SOURCE_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] &=
			~(AD1937_ADC_CTRL_2_ALRCLK_MASK |
			AD1937_ADC_CTRL_2_ABCLK_MASK |
			AD1937_ADC_CTRL_2_ABCLK_SOURCE_MASK);

	ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] |=
			(AD1937_DAC_CTRL_1_DLRCLK_SLAVE |
			AD1937_DAC_CTRL_1_DBCLK_SLAVE |
			AD1937_DAC_CTRL_1_DBCLK_SOURCE_DBCLK_PIN);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |=
			(AD1937_ADC_CTRL_2_ALRCLK_SLAVE |
			AD1937_ADC_CTRL_2_ABCLK_SLAVE |
			AD1937_ADC_CTRL_2_ABCLK_SOURCE_ABCLK_PIN);
	}

	if (pExtraInfo->daisyEn) {
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] &= ~(AD1937_PLL_CLK_CTRL_0_PWR_MASK);
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |= AD1937_PLL_CLK_CTRL_0_PWR_ON;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |= AD1937_PLL_CLK_CTRL_0_MCLKI_512;
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] &= ~(AD1937_PLL_CLK_CTRL_0_MCLKO_MASK);
		ad1937_enable[codec_idx][AD1937_PLL_CLK_CTRL_0] |= AD1937_PLL_CLK_CTRL_0_MCLKO_512;
	}

	ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] &= ~(AD1937_DAC_CTRL_1_DLRCLK_MASK |
						AD1937_DAC_CTRL_1_DBCLK_MASK |
						AD1937_DAC_CTRL_1_DBCLK_SOURCE_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] &= ~(AD1937_ADC_CTRL_2_ALRCLK_MASK |
						AD1937_ADC_CTRL_2_ABCLK_MASK |
						AD1937_ADC_CTRL_2_ABCLK_SOURCE_MASK);

	if (mode == AUDIO_CODEC_MASTER_MODE) {
		if (pExtraInfo->dacMasterEn) { /* DAC is master */
			ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] |= (AD1937_DAC_CTRL_1_DLRCLK_MASTER |
								AD1937_DAC_CTRL_1_DBCLK_MASTER |
								AD1937_DAC_CTRL_1_DBCLK_SOURCE_INTERNAL);
			ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= (AD1937_ADC_CTRL_2_ALRCLK_SLAVE |
								AD1937_ADC_CTRL_2_ABCLK_SLAVE |
								AD1937_ADC_CTRL_2_ABCLK_SOURCE_ABCLK_PIN);
		} else { /* ADC is master */
			ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] |= (AD1937_DAC_CTRL_1_DLRCLK_SLAVE |
								AD1937_DAC_CTRL_1_DBCLK_SLAVE |
								AD1937_DAC_CTRL_1_DBCLK_SOURCE_DBCLK_PIN);
			ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= (AD1937_ADC_CTRL_2_ALRCLK_MASTER |
								AD1937_ADC_CTRL_2_ABCLK_MASTER |
								AD1937_ADC_CTRL_2_ABCLK_SOURCE_INTERNAL);
		}
	} else {
		pr_err("++AD1937 Slave\n");
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] |= (AD1937_DAC_CTRL_1_DLRCLK_SLAVE |
								AD1937_DAC_CTRL_1_DBCLK_SLAVE |
								AD1937_DAC_CTRL_1_DBCLK_SOURCE_INTERNAL);
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= (AD1937_ADC_CTRL_2_ALRCLK_SLAVE |
								AD1937_ADC_CTRL_2_ABCLK_SLAVE |
								AD1937_ADC_CTRL_2_ABCLK_SOURCE_INTERNAL);
	}

	/* Bit width */
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_2] &= ~(AD1937_DAC_CTRL_2_WORD_WIDTH_MASK);
	if (codec_data_width == I2S_DATAWIDTH_16)
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_2] |= AD1937_DAC_CTRL_2_WORD_WIDTH_16_BITS;
	else if (codec_data_width == I2S_DATAWIDTH_20)
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_2] |= AD1937_DAC_CTRL_2_WORD_WIDTH_20_BITS;
	else if (codec_data_width == I2S_DATAWIDTH_24)
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_2] |= AD1937_DAC_CTRL_2_WORD_WIDTH_24_BITS;
	else
	pr_err("Not support this codec data width\r\n");

	/* Delay & Serial Format */
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_0] &= ~(AD1937_DAC_CTRL_0_DSDATA_DELAY_MASK |
							AD1937_DAC_CTRL_0_FMT_MASK);
	switch (codec_data_format) {
	case AUDIO_INTERFACE_LJM_FORMAT:
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_FMT_STEREO;
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_DSDATA_DELAY_0;
		break;
	case AUDIO_INTERFACE_RJM_FORMAT:
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_FMT_STEREO;
		if (codec_data_width == I2S_DATAWIDTH_16)
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_DSDATA_DELAY_16;
		else if (codec_data_width == I2S_DATAWIDTH_20)
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_DSDATA_DELAY_12;
		else if (codec_data_width == I2S_DATAWIDTH_24)
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_DSDATA_DELAY_8;
		break;
	case AUDIO_INTERFACE_TDM_FORMAT:
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_FMT_TDM_SINGLE_LINE;
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_DSDATA_DELAY_1;
		break;
	case AUDIO_INTERFACE_I2S_FORMAT:
	default:
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_FMT_STEREO;
		ad1937_enable[codec_idx][AD1937_DAC_CTRL_0]  |= AD1937_DAC_CTRL_0_DSDATA_DELAY_1;
		break;
	break;
	}

	/* BCLKs per frame */
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] &= ~(AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_MASK);
	if (bit_size == 64)
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] |= AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_64;
	else if (bit_size == 128)
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] |= AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_128;
	else if (bit_size == 256)
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] |= AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_256;
	else
	ad1937_enable[codec_idx][AD1937_DAC_CTRL_1] |= AD1937_DAC_CTRL_1_DBCLK_PER_FRAME_512;

	/* Bit width */
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] &= ~(AD1937_ADC_CTRL_1_WORD_WIDTH_MASK);

	if (codec_data_width == I2S_DATAWIDTH_16)
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_WORD_WIDTH_16_BITS;
	else if (codec_data_width == I2S_DATAWIDTH_20)
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_WORD_WIDTH_20_BITS;
	else if (codec_data_width == I2S_DATAWIDTH_24)
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_WORD_WIDTH_24_BITS;
	else
	pr_err("Not support this codec data width\r\n");

	/* Delay & Serial Format */
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] &= ~(AD1937_ADC_CTRL_1_ASDATA_DELAY_MASK |
		AD1937_ADC_CTRL_1_FMT_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] &= ~(AD1937_ADC_CTRL_2_ALRCLK_FMT_MASK);
	switch (codec_data_format) {
	case AUDIO_INTERFACE_LJM_FORMAT:
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_FMT_STEREO;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_ASDATA_DELAY_0;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ALRCLK_FMT_50_50;
		break;
	case AUDIO_INTERFACE_RJM_FORMAT:
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_DAC_CTRL_0_FMT_STEREO;
		if (codec_data_width == I2S_DATAWIDTH_16)
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_ASDATA_DELAY_16;
		else if (codec_data_width == I2S_DATAWIDTH_20)
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_ASDATA_DELAY_12;
		else if (codec_data_width == I2S_DATAWIDTH_24)
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_ASDATA_DELAY_8;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ALRCLK_FMT_50_50;
		break;
	case AUDIO_INTERFACE_TDM_FORMAT:
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_FMT_TDM_SINGLE_LINE;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_ASDATA_DELAY_1;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ALRCLK_FMT_50_50;

		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] &= ~(AD1937_ADC_CTRL_2_ABCLK_POLARITY_MASK);
		if (mode == AUDIO_CODEC_MASTER_MODE) {
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ABCLK_POLARITY_FALLING_EDGE;
		} else {
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ABCLK_POLARITY_RISING_EDGE;
		}
		break;
	case AUDIO_INTERFACE_I2S_FORMAT:
	default:
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_FMT_STEREO;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_1] |= AD1937_ADC_CTRL_1_ASDATA_DELAY_1;
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ALRCLK_FMT_50_50;

		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] &= ~(AD1937_ADC_CTRL_2_ABCLK_POLARITY_MASK);
		if (mode == AUDIO_CODEC_MASTER_MODE) {
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ABCLK_POLARITY_FALLING_EDGE;
		} else {
		ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ABCLK_POLARITY_RISING_EDGE;
		}
		break;
	}

	/* BCLKs per frame */
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] &= ~(AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_MASK);
	if (bit_size == 64)
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_64;
	else if (bit_size == 128)
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_128;
	else if (bit_size == 256)
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_256;
	else
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_2] |= AD1937_ADC_CTRL_2_ABCLK_PER_FRAME_512;

	/* Power On ADC */
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] &= (~AD1937_ADC_CTRL_0_PWR_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |= AD1937_ADC_CTRL_0_PWR_ON;

	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] &= (~AD1937_ADC_CTRL_0_ADC1L_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |= AD1937_ADC_CTRL_0_ADC1L_UNMUTE;
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] &= (~AD1937_ADC_CTRL_0_ADC1R_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |= AD1937_ADC_CTRL_0_ADC1R_UNMUTE;


	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] &= (~AD1937_ADC_CTRL_0_ADC2L_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |= AD1937_ADC_CTRL_0_ADC2L_UNMUTE;
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] &= (~AD1937_ADC_CTRL_0_ADC2R_MASK);
	ad1937_enable[codec_idx][AD1937_ADC_CTRL_0] |= AD1937_ADC_CTRL_0_ADC2R_UNMUTE;

	for (i = 0; i < 17; i++) {
		unsigned char val[4];
		val[0] = i;
		val[1] = ad1937_enable[codec_idx][i];
		val[2] = i;

		i2c_write(pExtraInfo->codecId, val[0], val[1], 2); /*wri2c 0x20 0x00 0x00 */
		val[3] = i2c_read(pExtraInfo->codecId, val[2]);
		pr_err("::Reg Addr 0x%02x 0x%02x : 0x%02x \r\n", i, ad1937_enable[codec_idx][i], val[3]);
	}
}
EXPORT_SYMBOL(OnAD1937CaptureAndPlayback);

struct ahub_unit_fpga *get_ahub_unit_fpga_private(void)
{
	return &ahub_unit_fpga_private;
}
EXPORT_SYMBOL(get_ahub_unit_fpga_private);

void ahub_unit_fpga_init(void)
{
	unsigned int i;

	ahub_unit_fpga_private.ape_fpga_misc_base =
		ioremap(NV_ADDRESS_MAP_APE_AHUB_FPGA_MISC_BASE, 256);
	for (i = 0; i < 5; i++)
		ahub_unit_fpga_private.ape_fpga_misc_i2s_clk_base[i] =
			ioremap(NV_ADDRESS_MAP_APE_AHUB_FPGA_MISC_BASE +
				ahub_fpga_misc_i2s_offset[i], 0x4);
	ahub_unit_fpga_private.ape_i2c_base =
		ioremap(NV_ADDRESS_MAP_APE_AHUB_I2C_BASE, 512);
	ahub_unit_fpga_private.pinmux_base =
		ioremap(NV_ADDRESS_MAP_APB_PP_BASE, 512);
	ahub_unit_fpga_private.ape_gpio_base =
		ioremap(NV_ADDRESS_MAP_APE_AHUB_GPIO_BASE, 256);
	ahub_unit_fpga_private.rst_clk_base =
		ioremap(NV_ADDRESS_MAP_PPSB_CLK_RST_BASE, 512);
	ahub_unit_fpga_private.i2s5_cya_base =
		ioremap(NV_ADDRESS_MAP_APE_I2S5_BASE + I2S5_CYA_0, 0x10);
}
EXPORT_SYMBOL(ahub_unit_fpga_init);

void ahub_unit_fpga_deinit(void)
{
	unsigned int i;

	iounmap(ahub_unit_fpga_private.ape_fpga_misc_base);
	for (i = 0; i < 5; i++)
		iounmap(ahub_unit_fpga_private.ape_fpga_misc_i2s_clk_base[i]);
	iounmap(ahub_unit_fpga_private.ape_i2c_base);
	iounmap(ahub_unit_fpga_private.pinmux_base);
	iounmap(ahub_unit_fpga_private.ape_gpio_base);
	iounmap(ahub_unit_fpga_private.rst_clk_base);
	iounmap(ahub_unit_fpga_private.i2s5_cya_base);
}
EXPORT_SYMBOL(ahub_unit_fpga_deinit);
MODULE_LICENSE("GPL");
