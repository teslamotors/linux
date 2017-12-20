/* compat26/include/compat26_clk.h
 *
 * 2.6 compatbility layer includes for clock code
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *	Copyright 2016 Codethink Ltd.
 *
 * Clock code compatibility.Note, this is not included into other code
 * as it re-directs bits of the clock layer.
*/

#include "compat_clk.h"

/* the debug for this has been removed, we still need to re-direct the
 * clk_get() and clk_set_rate() calls to check they are working and to
 * deal with special cases we have yet to fix.
 */
#define COMPAT26_LOG_CLK

#ifdef COMPAT26_LOG_CLK
#define clk_get(__dev, __name) tegra26_compat_clk_get(__dev, __name)
#define clk_set_rate(__clk, __rate) tegra26_compat_clk_set_rate(__clk, __rate)
#define clk_set_parent(__clk, __par) tegra26_compat_clk_set_parent(__clk, __par)

#endif

/* redirect the clk_enable and clk_disable() calls */
#define clk_enable(__clk) tegra26_compat_clk_enable(__clk)
#define clk_disable(__clk) tegra26_compat_clk_disable(__clk)
