/* compat26/tegra26/tegra3_dvfs.c
 *
 * Copyright 2016 Codethink Ltd.
 *
 * Minimal tegra3_dvfs.c for tegra26 compatibility.
*/

#include <linux/clk.h>

#include "clock.h"
#include "dvfs.h"
#include "tegra3_emc.h"

#define VDD_SAFE_STEP                 100

static struct dvfs_rail tegra3_dvfs_rail_vdd_cpu = {
        .reg_id = "vdd_cpu",
        .max_millivolts = 1250,
        .min_millivolts = 850,
        .step = VDD_SAFE_STEP,
        .jmp_to_zero = true,
};

static struct dvfs_rail tegra3_dvfs_rail_vdd_core = {
        .reg_id = "vdd_core",
        .max_millivolts = 1300,
        .min_millivolts = 1000,
        .step = VDD_SAFE_STEP,
};

static int tegra3_get_core_floor_mv(int cpu_mv)
{
        if (cpu_mv <= 825)
                return 1000;
        if (cpu_mv <=  975)
                return 1100;
	/* TODO: find out what does tegra_cpu_speedo_id do
	 * as we commented following 3 statements out at moment
	 */
//        if ((tegra_cpu_speedo_id() < 2) ||
//            (tegra_cpu_speedo_id() == 4))
//                return 1200;
        if (cpu_mv <= 1075)
                return 1200;
        if (cpu_mv <= 1250)
                return 1300;
        BUG();
}

int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{
        int ret = 0;

        if (tegra_emc_get_dram_type() != DRAM_TYPE_DDR3)
                return ret;

        if (((&tegra3_dvfs_rail_vdd_core == rail) &&
             (rail->nominal_millivolts > TEGRA_EMC_BRIDGE_MVOLTS_MIN)) ||
            ((&tegra3_dvfs_rail_vdd_cpu == rail) &&
             (tegra3_get_core_floor_mv(rail->nominal_millivolts) >
              TEGRA_EMC_BRIDGE_MVOLTS_MIN))) {
                struct clk *bridge = tegra_get_clock_by_name("bridge.emc");
                BUG_ON(!bridge);

                ret = clk_enable(bridge);
                pr_info("%s: %s: %s bridge.emc\n", __func__,
                        rail->reg_id, ret ? "failed to enable" : "enabled");
        }
        return ret;
}

int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{
        if (tegra_emc_get_dram_type() != DRAM_TYPE_DDR3)
                return 0;

        if (((&tegra3_dvfs_rail_vdd_core == rail) &&
             (rail->nominal_millivolts > TEGRA_EMC_BRIDGE_MVOLTS_MIN)) ||
            ((&tegra3_dvfs_rail_vdd_cpu == rail) &&
             (tegra3_get_core_floor_mv(rail->nominal_millivolts) >
              TEGRA_EMC_BRIDGE_MVOLTS_MIN))) {
                struct clk *bridge = tegra_get_clock_by_name("bridge.emc");
                BUG_ON(!bridge);

                clk_disable(bridge);
                pr_info("%s: %s: disabled bridge.emc\n",
                        __func__, rail->reg_id);
        }
        return 0;
}


