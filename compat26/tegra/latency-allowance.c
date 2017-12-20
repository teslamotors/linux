/* compat26/tegra/latency-allowance.c
 *
 * Copyright 2016 Codethink Ltd.
 */

#include <soc/tegra/mc.h>

#include "../include/compat26.h"
#include <mach/latency_allowance.h>


/* Map from 2.6 tegra_la_id to 4.4 client id
 *
 * 2.6 - arch/arm/mach-tegra/include/mach/latency_allowance.h
 * 4.4 - drivers/memory/tegra/tegra30.c
 *
 * Example mapping for display controller clients:
 *	TEGRA_LA_DISPLAY_0A	0x01
 *	TEGRA_LA_DISPLAY_0AB	0x02
 *	TEGRA_LA_DISPLAY_0B	0x03
 *	TEGRA_LA_DISPLAY_0BB	0x04
 *	TEGRA_LA_DISPLAY_0C	0x05
 *	TEGRA_LA_DISPLAY_0CB	0x06
 *	TEGRA_LA_DISPLAY_1B	0x07
 *	TEGRA_LA_DISPLAY_1BB	0x08
 *	TEGRA_LA_DISPLAY_HC	0x10
 *	TEGRA_LA_DISPLAY_HCB	0x11
 */
static const int client_map[66] = {
	0x0e,	/* 0 */
	0x31,	/* 1 */
	0x0f,	/* 2 */
	0x32,	/* 3 */
	0x01,	/* 4 */
	0x03,	/* 5 */
	0x05,	/* 6 */
	0x07,	/* 7 */
	0x10,	/* 8 */
	0x02,	/* 9 */
	0x04,	/* 10 */
	0x06,	/* 11 */
	0x08,	/* 12 */
	0x11,	/* 13 */
	0x09,	/* 14 */
	0x28,	/* 15 */
	0x29,	/* 16 */
	0x2a,	/* 17 */
	0x0a,	/* 18 */
	0x0b,	/* 19 */
	0x14,	/* 20 */
	0x30,	/* 21 */
	0x16,	/* 22 */
	0x17,	/* 23 */
	0x36,	/* 24 */
	0x15,	/* 25 */
	0x35,	/* 26 */
	0x37,	/* 27 */
	0x27,	/* 28 */
	0x39,	/* 29 */
	0x26,	/* 30 */
	0x38,	/* 31 */
	0x0c,	/* 32 */
	0x1a,	/* 33 */
	0x1b,	/* 34 */
	0x1c,	/* 35 */
	0x2b,	/* 36 */
	0x3a,	/* 37 */
	0x12,	/* 38 */
	0x18,	/* 39 */
	0x20,	/* 40 */
	0x33,	/* 41 */
	0x13,	/* 42 */
	0x19,	/* 43 */
	0x21,	/* 44 */
	0x34,	/* 45 */
	0x1d,	/* 46 */
	0x1e,	/* 47 */
	0x3b,	/* 48 */
	0x3c,	/* 49 */
	0x00,	/* 50 */
	0x1f,	/* 51 */
	0x3d,	/* 52 */
	0x22,	/* 53 */
	0x23,	/* 54 */
	0x24,	/* 55 */
	0x25,	/* 56 */
	0x3e,	/* 57 */
	0x3f,	/* 58 */
	0x40,	/* 59 */
	0x41,	/* 60 */
	0x0d,	/* 61 */
	0x2c,	/* 62 */
	0x2d,	/* 63 */
	0x2e,	/* 64 */
	0x2f,	/* 65 */
};

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_TEGRA_FPGA_PLATFORM)
#else
int tegra_set_latency_allowance(enum tegra_la_id id,
				unsigned int bandwidth_in_mbps)
{
	int id_int = (int)id;

	if (id == TEGRA_LA_MAX_ID)
		return -EINVAL;

	tegra_mc_set_latency_allowance(client_map[id_int], bandwidth_in_mbps);

	return 0;
}
#endif	
