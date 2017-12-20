/* compat26/tegra26/clock.c
 *
 * Copyright 2016 Codethink Ltd.
 *
 * Minimal clock.c for tegra26 compatibility.
*/
#include <linux/clk.h>
#include <asm/clkdev.h>
#include <mach/clk.h>

#include "clock.h"
#include "dvfs.h"

static DEFINE_MUTEX(clock_list_lock);
static LIST_HEAD(clocks);

static void __clk_set_cansleep(struct clk *c)
{
        struct clk *child;
        int i;
        BUG_ON(mutex_is_locked(&c->mutex));
        BUG_ON(spin_is_locked(&c->spinlock));

        /* Make sure that all possible descendants of sleeping clock are
           marked as sleeping (to eliminate "sleeping parent - non-sleeping
           child" relationship */
        list_for_each_entry(child, &clocks, node) {
                bool possible_parent = (child->parent == c);

                if (!possible_parent && child->inputs) {
                        for (i = 0; child->inputs[i].input; i++) {
                                if (child->inputs[i].input == c) {
                                        possible_parent = true;
                                        break;
                                }
                        }
                }

                if (possible_parent)
                        __clk_set_cansleep(child);
        }

        c->cansleep = true;
}

/* Must be called before any clk_get calls */
void clk_set_cansleep(struct clk *c)
{

        mutex_lock(&clock_list_lock);
        __clk_set_cansleep(c);
        mutex_unlock(&clock_list_lock);
}

