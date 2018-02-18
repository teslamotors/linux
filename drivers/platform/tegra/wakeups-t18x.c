/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irqchip/tegra.h>

#include <mach/irqs.h>

#include "iomap.h"

#define TEGRA_GPIO_BANK_ID_A 0
#define TEGRA_GPIO_BANK_ID_B 1
#define TEGRA_GPIO_BANK_ID_C 2
#define TEGRA_GPIO_BANK_ID_D 3
#define TEGRA_GPIO_BANK_ID_E 4
#define TEGRA_GPIO_BANK_ID_F 5
#define TEGRA_GPIO_BANK_ID_G 6
#define TEGRA_GPIO_BANK_ID_H 7
#define TEGRA_GPIO_BANK_ID_I 8
#define TEGRA_GPIO_BANK_ID_J 9
#define TEGRA_GPIO_BANK_ID_K 10
#define TEGRA_GPIO_BANK_ID_L 11
#define TEGRA_GPIO_BANK_ID_M 12
#define TEGRA_GPIO_BANK_ID_N 13
#define TEGRA_GPIO_BANK_ID_O 14
#define TEGRA_GPIO_BANK_ID_P 15
#define TEGRA_GPIO_BANK_ID_Q 16
#define TEGRA_GPIO_BANK_ID_R 17
#define TEGRA_GPIO_BANK_ID_S 18
#define TEGRA_GPIO_BANK_ID_T 19
#define TEGRA_GPIO_BANK_ID_U 20
#define TEGRA_GPIO_BANK_ID_V 21
#define TEGRA_GPIO_BANK_ID_W 22
#define TEGRA_GPIO_BANK_ID_X 23
#define TEGRA_GPIO_BANK_ID_Y 24
#define TEGRA_GPIO_BANK_ID_Z 25
#define TEGRA_GPIO_BANK_ID_AA 26
#define TEGRA_GPIO_BANK_ID_BB 27
#define TEGRA_GPIO_BANK_ID_CC 28
#define TEGRA_GPIO_BANK_ID_DD 29
#define TEGRA_GPIO_BANK_ID_EE 30
#define TEGRA_GPIO_BANK_ID_FF 31

#define TEGRA_GPIO(bank, offset) \
	((TEGRA_GPIO_BANK_ID_##bank * 8) + offset)

#define INT_OFFSET	32

#define INT_AOTAG2PMC  279 + INT_OFFSET
#define INT_RTC        10 + INT_OFFSET
#define INT_AOVC       215 + INT_OFFSET
#define INT_AOWDT      18 + INT_OFFSET
#define INT_XUSB       167 + INT_OFFSET
#define INT_SW_WAKE_TRIGGER 19 + INT_OFFSET
#define INT_AOVIC_FIQ	21 + INT_OFFSET
#define INT_AOVIC_IRQ   22 + INT_OFFSET
#define INT_AON_GPIO_0 60 + INT_OFFSET
#define INT_AON_GPIO_1 61 + INT_OFFSET
#define INT_VFMON      280 + INT_OFFSET
#define INT_AOPM       26 + INT_OFFSET
#define INT_PMC2LIC    211 + INT_OFFSET
#define INT_AO_DEBUG_WAKE 29 + INT_OFFSET
#define INT_AOPM2LIC   30 + INT_OFFSET
#define INT_AON_CAR    226 + INT_OFFSET
#define INT_SPE_WDT_EXPIRY 15 + INT_OFFSET
#define INT_EXTERNAL_PMU 209 + INT_OFFSET

static int tegra_gpio_wakes[] = {
	TEGRA_GPIO(A, 6),		/* wake0 */
	TEGRA_GPIO(A, 2),		/* wake1 */
	TEGRA_GPIO(A, 5),		/* wake2 */
	TEGRA_GPIO(D, 3),		/* wake3 */
	TEGRA_GPIO(E, 3),		/* wake4 */
	TEGRA_GPIO(G, 3),		/* wake5 */
	-EINVAL,			/* wake6 */
	TEGRA_GPIO(B, 3),		/* wake7 */
	TEGRA_GPIO(B, 5),		/* wake8 */
	TEGRA_GPIO(C, 0),		/* wake9 */
	TEGRA_GPIO(S, 2),		/* wake10 */
	TEGRA_GPIO(H, 2),		/* wake11 */
	TEGRA_GPIO(J, 5),		/* wake12 */
	TEGRA_GPIO(J, 6),		/* wake13 */
	TEGRA_GPIO(J, 7),		/* wake14 */
	TEGRA_GPIO(K, 0),		/* wake15 */
	TEGRA_GPIO(Q, 1),		/* wake16 */
	TEGRA_GPIO(F, 4),		/* wake17 */
	TEGRA_GPIO(M, 5),		/* wake18 */
	TEGRA_GPIO(P, 0),		/* wake19 */
	TEGRA_GPIO(P, 2),		/* wake20 */
	TEGRA_GPIO(P, 1),		/* wake21 */
	TEGRA_GPIO(O, 3),		/* wake22 */
	TEGRA_GPIO(R, 5),		/* wake23 */
	-EINVAL,			/* wake24 */
	TEGRA_GPIO(S, 3),		/* wake25 */
	TEGRA_GPIO(S, 4),		/* wake26 */
	TEGRA_GPIO(S, 1),		/* wake27 */
	TEGRA_GPIO(F, 2),		/* wake28 */
	TEGRA_GPIO(FF, 0),		/* wake29 */
	TEGRA_GPIO(FF, 4),		/* wake30 */
	TEGRA_GPIO(C, 6),		/* wake31 */
	TEGRA_GPIO(W, 2),		/* wake32 */
	TEGRA_GPIO(W, 5),		/* wake33 */
	TEGRA_GPIO(W, 1),		/* wake34 */
	TEGRA_GPIO(V, 0),		/* wake35 */
	TEGRA_GPIO(V, 1),		/* wake36 */
	TEGRA_GPIO(V, 2),		/* wake37 */
	TEGRA_GPIO(V, 3),		/* wake38 */
	TEGRA_GPIO(V, 4),		/* wake39 */
	TEGRA_GPIO(V, 5),		/* wake40 */
	TEGRA_GPIO(EE, 0),		/* wake41 */
	TEGRA_GPIO(Z, 1),		/* wake42 */
	TEGRA_GPIO(Z, 3),		/* wake43 */
	TEGRA_GPIO(AA, 0),		/* wake44 */
	TEGRA_GPIO(AA, 1),		/* wake45 */
	TEGRA_GPIO(AA, 2),		/* wake46 */
	TEGRA_GPIO(AA, 3),		/* wake47 */
	TEGRA_GPIO(AA, 4),		/* wake48 */
	TEGRA_GPIO(AA, 5),		/* wake49 */
	TEGRA_GPIO(AA, 6),		/* wake50 */
	TEGRA_GPIO(AA, 7),		/* wake51 */
	TEGRA_GPIO(X, 3),		/* wake52 */
	TEGRA_GPIO(X, 7),		/* wake53 */
	TEGRA_GPIO(Y, 0),		/* wake54 */
	TEGRA_GPIO(Y, 1),		/* wake55 */
	TEGRA_GPIO(Y, 2),		/* wake56 */
	TEGRA_GPIO(Y, 5),		/* wake57 */
	TEGRA_GPIO(Y, 6),		/* wake58 */
	TEGRA_GPIO(L, 1),		/* wake59 */
	TEGRA_GPIO(L, 3),		/* wake60 */
	TEGRA_GPIO(L, 4),		/* wake61 */
	TEGRA_GPIO(L, 5),		/* wake62 */
	TEGRA_GPIO(I, 4),		/* wake63 */
	TEGRA_GPIO(I, 6),		/* wake64 */
	TEGRA_GPIO(Z, 0),		/* wake65 */
	TEGRA_GPIO(Z, 2),		/* wake66 */
	TEGRA_GPIO(FF, 1),		/* wake67 */
	TEGRA_GPIO(FF, 2),		/* wake68 */
	TEGRA_GPIO(FF, 3),		/* wake69 */
	TEGRA_GPIO(H, 3),		/* wake70 */
	TEGRA_GPIO(P, 5),		/* wake71 */
	-EINVAL,		/* wake72 */
	-EINVAL,		/* wake73 */
	-EINVAL,		/* wake74 */
	-EINVAL,		/* wake75 */
	-EINVAL,		/* wake76 */
	-EINVAL,		/* wake77 */
	-EINVAL,		/* wake78 */
	-EINVAL,		/* wake79 */
	-EINVAL,		/* wake80 */
	-EINVAL,		/* wake81 */
	-EINVAL,		/* wake82 */
	-EINVAL,		/* wake83 */
	-EINVAL,		/* wake84 */
	-EINVAL,		/* wake85 */
	-EINVAL,		/* wake86 */
	-EINVAL,		/* wake87 */
	-EINVAL,		/* wake88 */
	-EINVAL,		/* wake89 */
	-EINVAL,		/* wake90 */
	-EINVAL,		/* wake91 */
	-EINVAL,		/* wake92 */
	-EINVAL,		/* wake93 */
	-EINVAL,		/* wake94 */
	-EINVAL,		/* wake95 */
};

static int tegra_wake_event_irq[] = {
	-EAGAIN,		/* wake0 */
	-EAGAIN,		/* wake1 */
	-EAGAIN,		/* wake2 */
	-EAGAIN,		/* wake3 */
	-EAGAIN,		/* wake4 */
	-EAGAIN,		/* wake5 */
	-EINVAL,		/* wake6 */
	-EAGAIN,		/* wake7 */
	-EAGAIN,		/* wake8 */
	-EAGAIN,		/* wake9 */
	-EAGAIN,		/* wake10 */
	-EAGAIN,		/* wake11 */
	-EAGAIN,		/* wake12 */
	-EAGAIN,		/* wake13 */
	-EAGAIN,		/* wake14 */
	-EAGAIN,		/* wake15 */
	-EAGAIN,		/* wake16 */
	-EAGAIN,		/* wake17 */
	-EAGAIN,		/* wake18 */
	-EAGAIN,		/* wake19 */
	-EAGAIN,		/* wake20 */
	-EAGAIN,		/* wake21 */
	-EAGAIN,		/* wake22 */
	-EAGAIN,		/* wake23 */
	INT_EXTERNAL_PMU,		/* wake24 */
	-EAGAIN,		/* wake25 */
	-EAGAIN,		/* wake26 */
	-EAGAIN,		/* wake27 */
	-EAGAIN,		/* wake28 */
	-EAGAIN,		/* wake29 */
	-EAGAIN,		/* wake30 */
	-EAGAIN,		/* wake31 */
	-EAGAIN,		/* wake32 */
	-EAGAIN,		/* wake33 */
	-EAGAIN,		/* wake34 */
	-EAGAIN,		/* wake35 */
	-EAGAIN,		/* wake36 */
	-EAGAIN,		/* wake37 */
	-EAGAIN,		/* wake38 */
	-EAGAIN,		/* wake39 */
	-EAGAIN,		/* wake40 */
	-EAGAIN,		/* wake41 */
	-EAGAIN,		/* wake42 */
	-EAGAIN,		/* wake43 */
	-EAGAIN,		/* wake44 */
	-EAGAIN,		/* wake45 */
	-EAGAIN,		/* wake46 */
	-EAGAIN,		/* wake47 */
	-EAGAIN,		/* wake48 */
	-EAGAIN,		/* wake49 */
	-EAGAIN,		/* wake50 */
	-EAGAIN,		/* wake51 */
	-EAGAIN,		/* wake52 */
	-EAGAIN,		/* wake53 */
	-EAGAIN,		/* wake54 */
	-EAGAIN,		/* wake55 */
	-EAGAIN,		/* wake56 */
	-EAGAIN,		/* wake57 */
	-EAGAIN,		/* wake58 */
	-EAGAIN,		/* wake59 */
	-EAGAIN,		/* wake60 */
	-EAGAIN,		/* wake61 */
	-EAGAIN,		/* wake62 */
	-EAGAIN,		/* wake63 */
	-EAGAIN,		/* wake64 */
	-EAGAIN,		/* wake65 */
	-EAGAIN,		/* wake66 */
	-EAGAIN,		/* wake67 */
	-EAGAIN,		/* wake68 */
	-EAGAIN,		/* wake69 */
	-EAGAIN,		/* wake70 */
	-EAGAIN,		/* wake71 */
	INT_AOTAG2PMC,		/* wake72 */
	INT_RTC,		/* wake73 */
	INT_AOVC,		/* wake74 */
	INT_AOWDT,		/* wake75 */
	INT_XUSB,		/* wake76 */
	INT_XUSB,		/* wake77 */
	INT_XUSB,		/* wake78 */
	INT_XUSB,		/* wake79 */
	INT_XUSB,		/* wake80 */
	INT_XUSB,		/* wake81 */
	INT_XUSB,		/* wake82 */
	INT_SW_WAKE_TRIGGER,	/* wake83 */
	INT_SPE_WDT_EXPIRY,	/* wake84 */
	INT_AOVIC_FIQ,		/* wake85 */
	INT_AOVIC_IRQ,		/* wake86 */
	INT_AON_GPIO_0,		/* wake87 */
	INT_AON_GPIO_1,		/* wake88 */
	INT_VFMON,		/* wake89 */
	INT_AOPM,		/* wake90 */
	-EINVAL,		/* wake91 */
	INT_PMC2LIC,		/* wake92 */
	INT_AO_DEBUG_WAKE,	/* wake93 */
	INT_AOPM2LIC,		/* wake94 */
	INT_AON_CAR,		/* wake95 */
};

static int __init tegra18x_wakeup_table_init(void)
{
	tegra_gpio_wake_table = tegra_gpio_wakes;
	tegra_irq_wake_table = tegra_wake_event_irq;
	tegra_wake_table_len = ARRAY_SIZE(tegra_gpio_wakes);
	return 0;
}

int __init tegra_wakeup_table_init(void)
{
	return tegra18x_wakeup_table_init();
}
