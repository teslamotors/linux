/*
 * include/linux/tegra-timer.h
 *
 * Copyright (c) 2016 NVIDIA Corporation. All rights reserved.
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

#ifndef _RTC_TEGRA_H_
#define _RTC_TEGRA_H_

#define RTC_SECONDS		0x08
#define RTC_SHADOW_SECONDS	0x0c
#define RTC_MILLISECONDS	0x10

#ifdef CONFIG_RTC_DRV_TEGRA
void tegra_rtc_set_trigger(unsigned long cycles);
u64 tegra_rtc_read_ms(void);
#else
void tegra_rtc_set_trigger(unsigned long cycles) { return; };
u64 tegra_rtc_read_ms(void) { return 0; };
#endif

#endif /* _RTC_TEGRA_H_ */
