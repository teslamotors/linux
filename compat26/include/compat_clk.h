/* compat26/include/compat_clk.h
 *
 * 2.6 compatbility layer includes for clock code
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *	Copyright 2016 Codethink Ltd.
 *
 * Clock code compatibility. For internal use.
*/

/* forward declaration, not going to pull <linux/clk.h> in here yet */
struct device;
struct clk;

/* only used for debugging. */
extern struct clk * tegra26_compat_clk_get(struct device *dev, const char *id);
extern int tegra26_compat_clk_set_rate(struct clk *clk, unsigned long rate);
extern int tegra26_compat_clk_set_parent(struct clk *clk, struct clk *parent);

/* mirrors of clk_enable and clk_disable() */
extern int tegra26_compat_clk_enable(struct clk *clk);
extern void tegra26_compat_clk_disable(struct clk *clk);
