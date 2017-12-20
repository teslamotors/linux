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

#ifndef __SOC_TEGRA_TESLA_H__
#define __SOC_TEGRA_TESLA_H__

/* Used communicate Tesla boardrev-specific audio rate to multiple places */
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
extern unsigned long tegra_audio_tdm_rate;
#else
#define tegra_audio_tdm_rate (0x0)
#endif

#endif /* __SOC_TEGRA_TESLA_H__ */
