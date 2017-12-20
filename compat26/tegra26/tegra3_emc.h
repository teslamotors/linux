/* compat26/tegra26/tegra3_emc.h
 *
 * Copyright 2016 Codethink Ltd.
 *
 * Minimal tegra3_emc.h for tegra26 compatibility.
*/

#define TEGRA_EMC_BRIDGE_MVOLTS_MIN     1200

enum {
        DRAM_TYPE_DDR3   = 0,
        DRAM_TYPE_LPDDR2 = 2,
};

extern int tegra_emc_get_dram_type(void);
