/*
 * arch/arm/mach-tegra/board-ventana.h
 *
 * Copyright (C) 2010 Google, Inc.
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

#ifndef _MACH_TEGRA_BOARD_VENTANA_H
#define _MACH_TEGRA_BOARD_VENTANA_H

int ventana_charge_init(void);
int ventana_regulator_init(void);
int ventana_sdhci_init(void);
int ventana_pinmux_init(void);
int ventana_panel_init(void);
int ventana_sensors_init(void);
int ventana_kbc_init(void);
int ventana_emc_init(void);

/* external gpios */

/* TPS6586X gpios */
#define TPS6586X_GPIO_BASE	TEGRA_NR_GPIOS
#define AVDD_DSI_CSI_ENB_GPIO	TPS6586X_GPIO_BASE + 1 /* gpio2 */

/* TCA6416 gpios */
#define TCA6416_GPIO_BASE	TEGRA_NR_GPIOS + 4
#define CAM2_PWR_DN_GPIO	TCA6416_GPIO_BASE + 4 /* gpio4 */
#define CAM2_RST_L_GPIO		TCA6416_GPIO_BASE + 5 /* gpio5 */
#define CAM2_AF_PWR_DN_L_GPIO	TCA6416_GPIO_BASE + 6 /* gpio6 */
#define CAM2_LDO_SHUTDN_L_GPIO	TCA6416_GPIO_BASE + 7 /* gpio7 */
#define CAM2_I2C_MUX_RST_GPIO	TCA6416_GPIO_BASE + 15 /* gpio15 */

#endif
