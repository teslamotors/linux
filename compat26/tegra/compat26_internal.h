/* compat26/tegra/compat26_internal.h
 *
 * Copyright 2016 Codethink Ltd.
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *
 * Internal functions used by the tegra26 compatibility layer.
*/

#include <linux/clk.h>
#include <linux/reset.h>

/* device defines for internal use */

#define __DEV_HOST1X	(0)
#define __DEV_MPE	(1)
#define __DEV_VI	(2)
#define __DEV_ISP	(3)
#define __DEV_DC0	(4)
#define __DEV_DC1	(5)
#define __DEV_DSI	(6)
#define __DEV_HDMI	(7)
#define __DEV_GR2D	(8)
#define __DEV_GR3D	(9)
#define __DEV_EPP	(10)
#define __DEV_AVP	(11)
#define __DEV_EMC	(12)
#define __DEV_DSIB	(13)
#define __DEV_HDA	(14)

#define __DEV_FLG_MULTI		(1 << 0)	/* >1 of these devices */
#define __DEV_FLG_NOCLK		(1 << 1)	/* device has no clocks */
#define __DEV_FLG_NOIO		(1 << 2)	/* device has no io area */

#define COMPAT_MAX_RESOURCES	(8)

struct tegra26_compat_dev {
	unsigned int		flags;
	struct platform_device	*dev;
	void __iomem		*iomap;
	const char		*compat_clkname;
	const char		*compat_clkname2;
	const char		**reset_names;

	/* resources to fill in. the grhost device needs to see both the
	 * grhost and some of the dc, so we split the dc device's resources
	 * in to the resource_dc array.
	 */
	struct resource		*resources[COMPAT_MAX_RESOURCES];
	struct resource		*resources_dc[COMPAT_MAX_RESOURCES];
};

extern void tegra26_fb_set_memory(unsigned long base, unsigned long size);
extern void tegra26_fb2_set_memory(unsigned long base, unsigned long size);

extern int tegra26_compat_avp_clk(struct device *dev);

extern void tegra_compat_add_clk_reset(struct clk *c,
				       struct reset_control *reset);

extern struct reset_control *tegra_clk_to_reset(struct clk *);

extern struct device *tegra26_get_dev(unsigned int dev);
extern void __iomem *tegra26_get_iomap(unsigned int dev);

/* call to prepare devices for use (pre calling board code) */
extern void compat_dev_prepare(void);

/* internal debugging */
extern void compat_dev_debug_show(void);

#ifdef CONFIG_DEBUG_FS
extern struct dentry *tegra26_debug_root;

extern int tegra26_debugfs_add_ro(const char *name, int (*print)(struct seq_file *s));

#else
static inline int tegra26_debugfs_add_ro(const char *name, int (*print)(struct seq_file *s)) { return 0; }

#endif

