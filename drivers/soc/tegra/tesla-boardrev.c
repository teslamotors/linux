/*
 * Copyright (C) 2017 Tesla Motors, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>

#include <soc/tegra/tesla.h>

/* Default to 48 KHz for Rev B, D MCUs */
unsigned long tegra_audio_tdm_rate = 6144000;

static int __init boardrev_setup(char *str)
{
	if (!str)
		return -EINVAL;

	if (strcmp(str, "x07") == 0) {
		/* We're a Rev. E MCU, use 44.1 Khz */
		tegra_audio_tdm_rate = 5644800; /* 44100*128 */
	}
	return 0;
}

early_param("boardrev", boardrev_setup);

