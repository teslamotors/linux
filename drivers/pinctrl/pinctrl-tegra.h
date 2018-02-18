/*
 * Driver for the NVIDIA Tegra pinmux
 *
 * Copyright (c) 2011-2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __PINMUX_TEGRA_H__
#define __PINMUX_TEGRA_H__

/**
 * struct tegra_function - Tegra pinctrl mux function
 * @name: The name of the function, exported to pinctrl core.
 * @groups: An array of pin groups that may select this function.
 * @ngroups: The number of entries in @groups.
 */
struct tegra_function {
	const char *name;
	const char * const *groups;
	unsigned ngroups;
};

/**
 * struct tegra_pingroup - Tegra pin group
 * @mux_reg:		Mux register offset. -1 if unsupported.
 * @mux_bank:		Mux register bank. 0 if unsupported.
 * @mux_bit:		Mux register bit. 0 if unsupported.
 * @pupd_reg:		Pull-up/down register offset. -1 if unsupported.
 * @pupd_bank:		Pull-up/down register bank. 0 if unsupported.
 * @pupd_bit:		Pull-up/down register bit. 0 if unsupported.
 * @tri_reg:		Tri-state register offset. -1 if unsupported.
 * @tri_bank:		Tri-state register bank. 0 if unsupported.
 * @tri_bit:		Tri-state register bit. 0 if unsupported.
 * @parked_bit:		Parked register bit. -1 if unsupported.
 * @einput_bit:		Enable-input register bit. 0 if unsupported.
 * @odrain_bit:		Open-drain register bit. 0 if unsupported.
 * @lock_bit:		Lock register bit. 0 if unsupported.
 * @ioreset_bit:	IO reset register bit. 0 if unsupported.
 * @rcv_sel_bit:	Receiver select bit. 0 if unsupported.
 * @e_io_hv_bit:	E_IO_HV register bit. 0 if unsupported.
 * @drv_reg:		Drive fields register offset. -1 if unsupported.
 *			This register contains the hsm, schmitt, lpmd, drvdn,
 *			drvup, slwr, and slwf parameters.
 * @drv_bank:		Drive fields register bank. 0 if unsupported.
 * @hsm_bit:		High Speed Mode register bit. 0 if unsupported.
 * @schmitt_bit:	Scmitt register bit. 0 if unsupported.
 * @lpmd_bit:		Low Power Mode register bit. 0 if unsupported.
 * @drvdn_bit:		Drive Down register bit. 0 if unsupported.
 * @drvdn_width:	Drive Down field width. 0 if unsupported.
 * @drvup_bit:		Drive Up register bit. 0 if unsupported.
 * @drvup_width:	Drive Up field width. 0 if unsupported.
 * @slwr_bit:		Slew Rising register bit. 0 if unsupported.
 * @slwr_width:		Slew Rising field width. 0 if unsupported.
 * @slwf_bit:		Slew Falling register bit. 0 if unsupported.
 * @slwf_width:		Slew Falling field width. 0 if unsupported.
 * @drvtype_bit:	Drive type register bit. 0 if unsupported.
 * @drvtype_width:	Drive type field width. 0 if unsupported.
 *
 * A representation of a group of pins (possibly just one pin) in the Tegra
 * pin controller. Each group allows some parameter or parameters to be
 * configured. The most common is mux function selection. Many others exist
 * such as pull-up/down, tri-state, etc. Tegra's pin controller is complex;
 * certain groups may only support configuring certain parameters, hence
 * each parameter is optional, represented by a -1 "reg" value.
 */
struct tegra_pingroup {
	const char *name;
	const unsigned *pins;
	unsigned npins;
	unsigned funcs[4];
	s32 mux_reg;
	s32 pupd_reg;
	s32 tri_reg;
	s32 drv_reg;
	int mux_bank;
	int pupd_bank;
	int tri_bank;
	int drv_bank;
	int mux_bit;
	int pupd_bit;
	int tri_bit;
	int einput_bit;
	int odrain_bit;
	int lock_bit;
	int parked_bit;
	int ioreset_bit;
	int rcv_sel_bit;
	int e_io_hv_bit;
	int hsm_bit;
	int schmitt_bit;
	int lpmd_bit;
	int drvdn_bit;
	int drvup_bit;
	int slwr_bit;
	int slwf_bit;
	int gpio_bit;
	int lpdr_bit;
	int pbias_buf_bit;
	int preemp_bit;
	int drvtype_bit;
	int rfu_in_bit;
	int drvdn_width;
	int drvup_width;
	int slwr_width;
	int slwf_width;
};

/**
 * struct tegra_pinctrl_soc_data - Tegra pin controller driver configuration
 * @ngpios:	The number of GPIO pins the pin controller HW affects.
 * @pins:	An array describing all pins the pin controller affects.
 *		All pins which are also GPIOs must be listed first within the
 *		array, and be numbered identically to the GPIO controller's
 *		numbering.
 * @npins:	The numbmer of entries in @pins.
 * @functions:	An array describing all mux functions the SoC supports.
 * @nfunctions:	The numbmer of entries in @functions.
 * @groups:	An array describing all pin groups the pin SoC supports.
 * @ngroups:	The numbmer of entries in @groups.
 * @is_gpio_reg_support: GPIO/SFIO selection support in pinmux register.
 * @config_data: List of configuration data which is SoC specific.
 * @nconfig_data: Number of config data.
 */
struct tegra_pinctrl_soc_data {
	unsigned ngpios;
	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	const struct tegra_function *functions;
	unsigned nfunctions;
	const struct tegra_pingroup *groups;
	unsigned ngroups;
	bool hsm_in_mux;
	bool schmitt_in_mux;
	bool drvtype_in_mux;
	bool is_gpio_reg_support;
	int (*suspend)(u32 *pg_data);
	void (*resume)(u32 *pg_data);
	int (*gpio_request_enable)(unsigned pin);
};

int tegra_pinctrl_probe(struct platform_device *pdev,
			const struct tegra_pinctrl_soc_data *soc_data);
int tegra_pinctrl_remove(struct platform_device *pdev);

u32 tegra_pinctrl_readl(u32 bank, u32 reg);
void tegra_pinctrl_writel(u32 val, u32 bank, u32 reg);

/* Special pinmux options */
#define TEGRA_PINMUX_SPECIAL_GPIO		0
#define TEGRA_PINMUX_SPECIAL_UNUSED		1
#define TEGRA_PINMUX_SPECIAL_MAX		2

#endif
