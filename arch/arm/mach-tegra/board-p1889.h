/*
 * arch/arm/mach-tegra/board-p1889.h
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _MACH_TEGRA_BOARD_P1859_H
#define _MACH_TEGRA_BOARD_P1859_H

#include <mach/irqs.h>
#include <linux/mfd/max77663-core.h>
#include "gpio-names.h"

int p1889_wifi_init(void);
int p1889_regulator_init(void);
int p1889_pca953x_init(void);
void p1889_audio_init(void);

/* External peripheral act as gpio */
#define MAX77663_IRQ_BASE		TEGRA_NR_IRQS
#define MAX77663_IRQ_END		(MAX77663_IRQ_BASE + MAX77663_IRQ_NR)
#define MAX77663_GPIO_BASE		TEGRA_NR_GPIOS
#define MAX77663_GPIO_END		(MAX77663_GPIO_BASE + MAX77663_GPIO_NR)

/* PCA953X - MISC SYSTEM IO */
#define PCA953X_MISCIO_0_GPIO_BASE	(MAX77663_GPIO_END + 1)
#define MISCIO_BT_RST_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 0)
#define MISCIO_GPS_RST_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 1)
#define MISCIO_GPS_EN_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 2)
#define MISCIO_WF_EN_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 3)
#define MISCIO_WF_RST_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 4)
#define MISCIO_BT_EN_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 5)
/* GPIO6 is not used */
#define MISCIO_NOT_USED0		(PCA953X_MISCIO_0_GPIO_BASE + 6)
#define MISCIO_BT_WAKEUP_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 7)
#define MISCIO_FAN_SEL_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 8)
#define MISCIO_EN_MISC_BUF_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 9)
#define MISCIO_EN_MSATA_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 10)
#define MISCIO_EN_SDCARD_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 11)
/* GPIO12 is not used */
#define MISCIO_NOT_USED1		(PCA953X_MISCIO_0_GPIO_BASE + 12)
#define MISCIO_ABB_RST_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 13)
#define MISCIO_USER_LED2_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 14)
#define MISCIO_USER_LED1_GPIO		(PCA953X_MISCIO_0_GPIO_BASE + 15)
#define PCA953X_MISCIO_0_GPIO_END	(PCA953X_MISCIO_0_GPIO_BASE + 15)

#define PCA953X_MISCIO_1_GPIO_BASE	(PCA953X_MISCIO_0_GPIO_END + 1)
#define MISCIO_MUX_DAP_D_SEL		(PCA953X_MISCIO_1_GPIO_BASE + 0)
#define MISCIO_MUX_DAP_D_EN		(PCA953X_MISCIO_1_GPIO_BASE + 1)
#define PCA953X_MISCIO_1_GPIO_END	(PCA953X_MISCIO_1_GPIO_BASE + 15)

#define PCA953X_MISCIO_2_GPIO_BASE	(PCA953X_MISCIO_1_GPIO_END + 1)
#define MISCIO_MDM_EN			(PCA953X_MISCIO_2_GPIO_BASE + 9)
#define MISCIO_MDM_COLDBOOT		(PCA953X_MISCIO_2_GPIO_BASE + 11)
#define MISCIO_AP_MDM_RESET		(PCA953X_MISCIO_2_GPIO_BASE + 15)
#define PCA953X_MISCIO_2_GPIO_END	(PCA953X_MISCIO_2_GPIO_BASE + 15)

/* PCA953X I2C IO expander bus addresses */
#define PCA953X_MISCIO_0_ADDR		0x75
#define PCA953X_MISCIO_1_ADDR		0x76
#define PCA953X_MISCIO_2_ADDR		0x74

#endif
