/* compat26/tegra26/clock.h
 *
 * Copyright 2016 Codethink Ltd.
 *
 * Minimal clock.h for tegra26 compatibility.
*/

#include <linux/clk.h>

void clk_set_cansleep(struct clk *c);

extern struct clk *tegra_get_clock_by_name(const char *name);

struct clk_mux_sel {
	struct clk		*input;
	u32			value;
};

struct clk {
        /* node for master clocks list */
        struct list_head        node;           /* node for list of all clocks */
        struct dvfs             *dvfs;

#ifdef CONFIG_DEBUG_FS
        struct dentry           *dent;
        struct dentry           *parent_dent;
#endif
        bool                    set;
        struct clk_ops          *ops;
        unsigned long           dvfs_rate;
        unsigned long           rate;
        unsigned long           max_rate;
        unsigned long           min_rate;
        bool                    auto_dvfs;
        bool                    cansleep;
        u32                     flags;
        const char              *name;

        u32                     refcnt;
        struct clk              *parent;
        u32                     div;
        u32                     mul;

        const struct clk_mux_sel        *inputs;
        u32                             reg;
        u32                             reg_shift;

        struct list_head                shared_bus_list;

        union {
                struct {
                        unsigned int                    clk_num;
                } periph;
                struct {
                        unsigned long                   input_min;
                        unsigned long                   input_max;
                        unsigned long                   cf_min;
                        unsigned long                   cf_max;
                        unsigned long                   vco_min;
                        unsigned long                   vco_max;
                        const struct clk_pll_freq_table *freq_table;
                        int                             lock_delay;
                        unsigned long                   fixed_rate;
                } pll;
                struct {
                        u32                             sel;
                        u32                             reg_mask;
                } mux;
                struct {
                        struct clk                      *main;
                        struct clk                      *backup;
                } cpu;
                struct {
                        struct clk                      *pclk;
                        struct clk                      *hclk;
                        struct clk                      *sclk_low;
                        struct clk                      *sclk_high;
                        unsigned long                   threshold;
                } system;
                struct {
                        struct list_head                node;
                        bool                            enabled;
                        unsigned long                   rate;
                        const char                      *client_id;
                        struct clk                      *client;
                        u32                             client_div;
                } shared_bus_user;
        } u;

        struct raw_notifier_head                        *rate_change_nh;

        struct mutex mutex;
        spinlock_t spinlock;
};
