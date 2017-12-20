/* compat26/include/compat26_nvrm.h
 *
 * 2.6 compatbility layer includes
 *	Ben Dooks <ben.dooks@codethink.co.uk>
 *	Copyright 2016 Codethink Ltd.
 *
 * Bits for the compatiblity layer from arch/arm/mach-tegra/nv/
*/

/* lock entire kernel not needed any more */
#define lock_kernel() do {} while (0)
#define unlock_kernel() do {} while (0)

#if defined(CONFIG_ARCH_TEGRA_2x_SOC) && defined(CONFIG_ARCH_TEGRA_3x_SOC)
#error build cannot currently support building for both tegra2 and tegra3
#endif

#define INT_GPIO_BASE	(4*32)	/* todo - better definition for this */

#define IRQF_NOAUTOEN	(0)	/* don't have this in 4.4 */
#define IRQF_VALID	(0)	/* don't have this in 4.4 */

/* this doesn't seem to be here any more */
static inline void set_irq_flags(unsigned int irq, unsigned int flags) { }

/* cache bits (we probably shouldn't be including this) */
#include <asm/outercache.h>

static inline void outer_sync(void)
{
#ifdef CONFIG_OUTER_CACHE_SYNC
	if (outer_cache.sync)
		outer_cache.sync();
#else
	WARN_ON_ONCE(1);
#endif
};

/* reset code */

#include <mach/clk.h>
/* todo - check these map to the tegra reset calls */
#define tegra2_periph_reset_assert tegra_periph_reset_assert
#define tegra2_periph_reset_deassert tegra_periph_reset_deassert

/* things needed from elsewhere */

extern unsigned int tegra_compat26_get_irq_gnt0(void);
extern unsigned int tegra_compat26_get_irq_sem_inbox_ibf(void);

#define INT_GNT_0	tegra_compat26_get_irq_gnt0()
#define INT_SHR_SEM_INBOX_IBF	tegra_compat26_get_irq_sem_inbox_ibf()

/* todo - get an ioremap_cached variant */
#define ioremap_cached(__base, __sz) ({ void *res = ioremap(__base, __sz); pr_warn("WARN: ioremap_cached(%08x, %u) called\n", __base, (u32) __sz); res; })
