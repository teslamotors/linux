/*
 * drivers/platform/tegra/tegra_cl_dvfs.c
 *
 * Copyright (c) 2012-2016 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/tegra-soc.h>
#include <linux/pinctrl/pinconf-tegra.h>

#include <mach/irqs.h>

#include <linux/platform/tegra/tegra_cl_dvfs.h>
#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/dvfs.h>
#include "iomap.h"
#include "tegra_simon.h"

#define OUT_MASK			0x3f

#define CL_DVFS_CTRL			0x00
#define CL_DVFS_CONFIG			0x04
#define CL_DVFS_CONFIG_DIV_MASK		0xff

#define CL_DVFS_PARAMS			0x08
#define CL_DVFS_PARAMS_CG_SCALE		(0x1 << 24)
#define CL_DVFS_PARAMS_FORCE_MODE_SHIFT	22
#define CL_DVFS_PARAMS_FORCE_MODE_MASK	(0x3 << CL_DVFS_PARAMS_FORCE_MODE_SHIFT)
#define CL_DVFS_PARAMS_CF_PARAM_SHIFT	16
#define CL_DVFS_PARAMS_CF_PARAM_MASK	(0x3f << CL_DVFS_PARAMS_CF_PARAM_SHIFT)
#define CL_DVFS_PARAMS_CI_PARAM_SHIFT	8
#define CL_DVFS_PARAMS_CI_PARAM_MASK	(0x7 << CL_DVFS_PARAMS_CI_PARAM_SHIFT)
#define CL_DVFS_PARAMS_CG_PARAM_SHIFT	0
#define CL_DVFS_PARAMS_CG_PARAM_MASK	(0xff << CL_DVFS_PARAMS_CG_PARAM_SHIFT)

#define CL_DVFS_TUNE0			0x0c
#define CL_DVFS_TUNE1			0x10

#define CL_DVFS_FREQ_REQ		0x14
#define CL_DVFS_FREQ_REQ_FORCE_ENABLE	(0x1 << 28)
#define CL_DVFS_FREQ_REQ_FORCE_SHIFT	16
#define CL_DVFS_FREQ_REQ_FORCE_MASK     (0xfff << CL_DVFS_FREQ_REQ_FORCE_SHIFT)
#define FORCE_MAX			2047
#define FORCE_MIN			-2048
#define CL_DVFS_FREQ_REQ_SCALE_SHIFT	8
#define CL_DVFS_FREQ_REQ_SCALE_MASK     (0xff << CL_DVFS_FREQ_REQ_SCALE_SHIFT)
#define SCALE_MAX			256
#define CL_DVFS_FREQ_REQ_FREQ_VALID	(0x1 << 7)
#define CL_DVFS_FREQ_REQ_FREQ_SHIFT	0
#define CL_DVFS_FREQ_REQ_FREQ_MASK      (0x7f << CL_DVFS_FREQ_REQ_FREQ_SHIFT)
#define FREQ_MAX			127

#define CL_DVFS_SCALE_RAMP		0x18

#define CL_DVFS_DROOP_CTRL		0x1c
#define CL_DVFS_DROOP_CTRL_MIN_FREQ_SHIFT 16
#define CL_DVFS_DROOP_CTRL_MIN_FREQ_MASK  \
		(0xff << CL_DVFS_DROOP_CTRL_MIN_FREQ_SHIFT)
#define CL_DVFS_DROOP_CTRL_CUT_SHIFT	8
#define CL_DVFS_DROOP_CTRL_CUT_MASK     (0xf << CL_DVFS_DROOP_CTRL_CUT_SHIFT)
#define CL_DVFS_DROOP_CTRL_RAMP_SHIFT	0
#define CL_DVFS_DROOP_CTRL_RAMP_MASK    (0xff << CL_DVFS_DROOP_CTRL_RAMP_SHIFT)

#define CL_DVFS_OUTPUT_CFG		0x20
#define CL_DVFS_OUTPUT_CFG_I2C_ENABLE	(0x1 << 30)
#define CL_DVFS_OUTPUT_CFG_SAFE_SHIFT	24
#define CL_DVFS_OUTPUT_CFG_SAFE_MASK    \
		(OUT_MASK << CL_DVFS_OUTPUT_CFG_SAFE_SHIFT)
#define CL_DVFS_OUTPUT_CFG_MAX_SHIFT	16
#define CL_DVFS_OUTPUT_CFG_MAX_MASK     \
		(OUT_MASK << CL_DVFS_OUTPUT_CFG_MAX_SHIFT)
#define CL_DVFS_OUTPUT_CFG_MIN_SHIFT	8
#define CL_DVFS_OUTPUT_CFG_MIN_MASK     \
		(OUT_MASK << CL_DVFS_OUTPUT_CFG_MIN_SHIFT)
#define CL_DVFS_OUTPUT_CFG_PWM_DELTA	(0x1 << 7)
#define CL_DVFS_OUTPUT_CFG_PWM_ENABLE	(0x1 << 6)
#define CL_DVFS_OUTPUT_CFG_PWM_DIV_SHIFT 0
#define CL_DVFS_OUTPUT_CFG_PWM_DIV_MASK  \
		(OUT_MASK << CL_DVFS_OUTPUT_CFG_PWM_DIV_SHIFT)

#define CL_DVFS_OUTPUT_FORCE		0x24
#define CL_DVFS_OUTPUT_FORCE_ENABLE	(0x1 << 6)
#define CL_DVFS_OUTPUT_FORCE_VALUE_SHIFT 0
#define CL_DVFS_OUTPUT_FORCE_VALUE_MASK  \
		(OUT_MASK << CL_DVFS_OUTPUT_FORCE_VALUE_SHIFT)

#define CL_DVFS_MONITOR_CTRL		0x28
#define CL_DVFS_MONITOR_CTRL_DISABLE	0
#define CL_DVFS_MONITOR_CTRL_OUT	5
#define CL_DVFS_MONITOR_CTRL_FREQ	6
#define CL_DVFS_MONITOR_DATA		0x2c
#define CL_DVFS_MONITOR_DATA_NEW	(0x1 << 16)
#define CL_DVFS_MONITOR_DATA_MASK	0xFFFF

#define CL_DVFS_I2C_CFG			0x40
#define CL_DVFS_I2C_CFG_ARB_ENABLE	(0x1 << 20)
#define CL_DVFS_I2C_CFG_HS_CODE_SHIFT	16
#define CL_DVFS_I2C_CFG_HS_CODE_MASK	(0x7 << CL_DVFS_I2C_CFG_HS_CODE_SHIFT)
#define CL_DVFS_I2C_CFG_PACKET_ENABLE	(0x1 << 15)
#define CL_DVFS_I2C_CFG_SIZE_SHIFT	12
#define CL_DVFS_I2C_CFG_SIZE_MASK	(0x7 << CL_DVFS_I2C_CFG_SIZE_SHIFT)
#define CL_DVFS_I2C_CFG_SLAVE_ADDR_10	(0x1 << 10)
#define CL_DVFS_I2C_CFG_SLAVE_ADDR_SHIFT 0
#define CL_DVFS_I2C_CFG_SLAVE_ADDR_MASK	\
		(0x3ff << CL_DVFS_I2C_CFG_SLAVE_ADDR_SHIFT)

#define CL_DVFS_I2C_VDD_REG_ADDR	0x44
#define CL_DVFS_I2C_STS			0x48
#define CL_DVFS_I2C_STS_I2C_LAST_SHIFT	1
#define CL_DVFS_I2C_STS_I2C_REQ_PENDING	0x1

#define CL_DVFS_INTR_STS		0x5c
#define CL_DVFS_INTR_EN			0x60
#define CL_DVFS_INTR_MIN_MASK		0x1
#define CL_DVFS_INTR_MAX_MASK		0x2

#define CL_DVFS_CC4_HVC			0x74
#define CL_DVFS_CC4_HVC_CTRL_SHIFT	0
#define CL_DVFS_CC4_HVC_CTRL_MASK	(0x3 << CL_DVFS_CC4_HVC_CTRL_SHIFT)
#define CL_DVFS_CC4_HVC_FORCE_VAL_SHIFT	2
#define CL_DVFS_CC4_HVC_FORCE_VAL_MASK \
	(OUT_MASK << CL_DVFS_CC4_HVC_FORCE_VAL_SHIFT)
#define CL_DVFS_CC4_HVC_FORCE_EN	(0x1 << 8)

#define CL_DVFS_I2C_CNTRL		0x100
#define CL_DVFS_I2C_CLK_DIVISOR		0x16c
#define CL_DVFS_I2C_CLK_DIVISOR_MASK	0xffff
#define CL_DVFS_I2C_CLK_DIVISOR_FS_SHIFT 16
#define CL_DVFS_I2C_CLK_DIVISOR_HS_SHIFT 0

#define CL_DVFS_OUTPUT_LUT		0x200

#define CL_DVFS_APERTURE		0x400

#define IS_I2C_OFFS(offs)		\
	((((offs) >= CL_DVFS_I2C_CFG) && ((offs) <= CL_DVFS_INTR_EN)) || \
	((offs) >= CL_DVFS_I2C_CNTRL))

#define CL_DVFS_CALIBR_TIME		40000
#define CL_DVFS_OUTPUT_PENDING_TIMEOUT	1000
#define CL_DVFS_OUTPUT_RAMP_DELAY	100
#define CL_DVFS_TUNE_HIGH_DELAY		2000

#define CL_DVFS_TUNE_HIGH_MARGIN_MV	20
#define CL_DVFS_CAP_GUARD_BAND_STEPS	2

enum tegra_cl_dvfs_ctrl_mode {
	TEGRA_CL_DVFS_UNINITIALIZED = 0,
	TEGRA_CL_DVFS_DISABLED = 1,
	TEGRA_CL_DVFS_OPEN_LOOP = 2,
	TEGRA_CL_DVFS_CLOSED_LOOP = 3,
};

/**
 * enum tegra_cl_dvfs_tune_state - state of the voltage-regime switching code
 * @TEGRA_CL_DVFS_TUNE_LOW: DFLL is in the low-voltage range (or open-loop mode)
 * @TEGRA_CL_DVFS_TUNE_HIGH_REQUEST: waiting for DFLL I2C output to reach high
 * @TEGRA_CL_DVFS_TUNE_HIGH_REQUEST_2: waiting for PMIC to react to DFLL output
 * @TEGRA_CL_DVFS_TUNE_HIGH: DFLL in the high-voltage range
 *
 * These are software states, not hardware states.
 */
enum tegra_cl_dvfs_tune_state {
	TEGRA_CL_DVFS_TUNE_LOW = 0,
	TEGRA_CL_DVFS_TUNE_HIGH_REQUEST,
	TEGRA_CL_DVFS_TUNE_HIGH_REQUEST_2,
	TEGRA_CL_DVFS_TUNE_HIGH,
};

struct dfll_rate_req {
	u8	freq;
	u8	scale;
	u8	output;
	u8	cap;
	unsigned long rate;
};

struct voltage_limits {
	int		vmin;
	int		vmax;
	seqcount_t	vmin_seqcnt;
	seqcount_t	vmax_seqcnt;
	bool		clamped;
};

struct tegra_cl_dvfs {
	void					*cl_base;
	void					*cl_i2c_base;
	struct tegra_cl_dvfs_platform_data	*p_data;

	struct dvfs			*safe_dvfs;
	struct thermal_cooling_device	*vmax_cdev;
	struct thermal_cooling_device	*vmin_cdev;
	struct work_struct		init_cdev_work;

	struct clk			*soc_clk;
	struct clk			*ref_clk;
	struct clk			*i2c_clk;
	struct clk			*dfll_clk;
	unsigned long			ref_rate;
	unsigned long			i2c_rate;

	/* output voltage mapping:
	 * legacy dvfs table index -to- cl_dvfs output LUT index
	 * cl_dvfs output LUT index -to- PMU value/voltage pair ptr
	 */
	u8				clk_dvfs_map[MAX_DVFS_FREQS];
	struct voltage_reg_map		*out_map[MAX_CL_DVFS_VOLTAGES];
	u8				num_voltages;
	u8				safe_output;
	u8				rail_relations_out_min;

	u32				tune0_low;
	u32				tune0_high;

	u8				tune_high_out_start;
	u8				tune_high_out_min;
	unsigned long			tune_high_dvco_rate_min;
	unsigned long			tune_high_target_rate_min;

	u8				minimax_output;
	u8				thermal_out_caps[MAX_THERMAL_LIMITS];
	u8				thermal_out_floors[MAX_THERMAL_LIMITS+1];
	int				thermal_mv_floors[MAX_THERMAL_LIMITS];
	int				therm_caps_num;
	int				therm_floors_num;
	unsigned long			dvco_rate_floors[MAX_THERMAL_LIMITS+1];
	unsigned long			dvco_rate_min;

	struct voltage_limits		v_limits;
	u8				lut_min;
	u8				lut_max;
	u8				force_out_min;
	u32				suspended_force_out;
	int				therm_cap_idx;
	int				therm_floor_idx;
	struct dfll_rate_req		last_req;
	enum tegra_cl_dvfs_tune_state	tune_state;
	enum tegra_cl_dvfs_ctrl_mode	mode;

	struct hrtimer			tune_timer;
	ktime_t				tune_delay;
	ktime_t				tune_ramp;
	u8				tune_out_last;

	struct timer_list		calibration_timer;
	unsigned long			calibration_delay;
	ktime_t				last_calibration;
	unsigned long			calibration_range_min;
	unsigned long			calibration_range_max;

	struct notifier_block		simon_grade_nb;
};

struct tegra_cl_dvfs_soc_match_data {
	u32 flags;
};

/* Conversion macros (different scales for frequency request, and monitored
   rate is not a typo) */
#define RATE_STEP(cld)				((cld)->ref_rate / 2)
#define GET_REQUEST_FREQ(rate, ref_rate)	((rate) / ((ref_rate) / 2))
#define GET_REQUEST_RATE(freq, ref_rate)	((freq) * ((ref_rate) / 2))
#define GET_MONITORED_RATE(freq, ref_rate)	((freq) * ((ref_rate) / 4))
#define GET_DROOP_FREQ(rate, ref_rate)		((rate) / ((ref_rate) / 4))
#define ROUND_MIN_RATE(rate, ref_rate)		\
		(DIV_ROUND_UP(rate, (ref_rate) / 2) * ((ref_rate) / 2))
#define GET_DIV(ref_rate, out_rate, scale)	\
		DIV_ROUND_UP((ref_rate), (out_rate) * (scale))
#define GET_SAMPLE_PERIOD(cld)	\
		DIV_ROUND_UP(1000000, (cld)->p_data->cfg_param->sample_rate)

static const char *mode_name[] = {
	[TEGRA_CL_DVFS_UNINITIALIZED] = "uninitialized",
	[TEGRA_CL_DVFS_DISABLED] = "disabled",
	[TEGRA_CL_DVFS_OPEN_LOOP] = "open_loop",
	[TEGRA_CL_DVFS_CLOSED_LOOP] = "closed_loop",
};

/*
 * In some h/w configurations CL-DVFS module registers have two different
 * address bases: one for I2C control/status registers, and one for all other
 * registers. Registers accessors are separated below accordingly just by
 * comparing register offset with start of I2C section - CL_DVFS_I2C_CFG. One
 * special case is CL_DVFS_OUTPUT_CFG register: when I2C controls are separated
 * I2C_ENABLE bit of this register is accessed from I2C base, and all other bits
 * are accessed from the main base.
 */
static inline u32 cl_dvfs_i2c_readl(struct tegra_cl_dvfs *cld, u32 offs)
{
	return __raw_readl(cld->cl_i2c_base + offs);
}
static inline void cl_dvfs_i2c_writel(struct tegra_cl_dvfs *cld,
				      u32 val, u32 offs)
{
	__raw_writel(val, cld->cl_i2c_base + offs);
}
static inline void cl_dvfs_i2c_wmb(struct tegra_cl_dvfs *cld)
{
	cl_dvfs_i2c_readl(cld, CL_DVFS_I2C_CFG);
	dsb(sy);
}

static inline u32 cl_dvfs_readl(struct tegra_cl_dvfs *cld, u32 offs)
{
	if (IS_I2C_OFFS(offs))
		return cl_dvfs_i2c_readl(cld, offs);
	return __raw_readl((void *)cld->cl_base + offs);
}
static inline void cl_dvfs_writel(struct tegra_cl_dvfs *cld, u32 val, u32 offs)
{
	if (IS_I2C_OFFS(offs)) {
		cl_dvfs_i2c_writel(cld, val, offs);
		return;
	}
	__raw_writel(val, (void *)cld->cl_base + offs);
}
static inline void cl_dvfs_wmb(struct tegra_cl_dvfs *cld)
{
	cl_dvfs_readl(cld, CL_DVFS_CTRL);
	dsb(sy);
}

static inline void switch_monitor(struct tegra_cl_dvfs *cld, u32 selector)
{
	/* delay to make sure selector has switched */
	cl_dvfs_writel(cld, selector, CL_DVFS_MONITOR_CTRL);
	cl_dvfs_wmb(cld);
	udelay(1);
}

static inline void invalidate_request(struct tegra_cl_dvfs *cld)
{
	u32 val = cl_dvfs_readl(cld, CL_DVFS_FREQ_REQ);
	val &= ~CL_DVFS_FREQ_REQ_FREQ_VALID;
	cl_dvfs_writel(cld, val, CL_DVFS_FREQ_REQ);
	cl_dvfs_wmb(cld);
}

static inline void set_request_scale(struct tegra_cl_dvfs *cld, u8 scale)
{
	u32 val = cl_dvfs_readl(cld, CL_DVFS_FREQ_REQ);
	val &= ~CL_DVFS_FREQ_REQ_SCALE_MASK;
	val |= scale << CL_DVFS_FREQ_REQ_SCALE_SHIFT;
	cl_dvfs_writel(cld, val, CL_DVFS_FREQ_REQ);
	cl_dvfs_wmb(cld);
}

static inline u32 output_force_set_val(struct tegra_cl_dvfs *cld, u8 out_val)
{
	u32 val = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_FORCE);
	val = (val & CL_DVFS_OUTPUT_FORCE_ENABLE) | (out_val & OUT_MASK);
	cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_FORCE);
	return cl_dvfs_readl(cld, CL_DVFS_OUTPUT_FORCE);
}

static inline void output_force_enable(struct tegra_cl_dvfs *cld, u32 val)
{
	val |= CL_DVFS_OUTPUT_FORCE_ENABLE;
	cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_FORCE);
	cl_dvfs_wmb(cld);
}

static inline void output_force_disable(struct tegra_cl_dvfs *cld)
{
	u32 val = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_FORCE);
	val &= ~CL_DVFS_OUTPUT_FORCE_ENABLE;
	cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_FORCE);
	cl_dvfs_wmb(cld);
}

/*
 * Reading monitor data concurrently with the update may render intermediate
 * (neither "old" nor "new") values. Synchronization with the "rising edge"
 * of DATA_NEW makes it very unlikely, but still possible. Use simple filter:
 * compare 2 consecutive readings for data consistency within 2 LSb range.
 * Return error otherwise. On the platform that does not allow to use DATA_NEW
 * at all check for consistency of consecutive reads is the only protection.
 */
static int filter_monitor_data(struct tegra_cl_dvfs *cld, u32 *data)
{
	u32 val = cl_dvfs_readl(cld, CL_DVFS_MONITOR_DATA) &
		CL_DVFS_MONITOR_DATA_MASK;
	*data &= CL_DVFS_MONITOR_DATA_MASK;
	if (abs(*data - val) <= 2)
		return 0;

	*data = cl_dvfs_readl(cld, CL_DVFS_MONITOR_DATA) &
		CL_DVFS_MONITOR_DATA_MASK;
	if (abs(*data - val) <= 2)
		return 0;

	return -EINVAL;
}

static inline void wait_data_new(struct tegra_cl_dvfs *cld, u32 *data)
{
	cl_dvfs_readl(cld, CL_DVFS_MONITOR_DATA); /* clear data new */
	if (!(cld->p_data->flags & TEGRA_CL_DVFS_DATA_NEW_NO_USE)) {
		do {
			*data = cl_dvfs_readl(cld, CL_DVFS_MONITOR_DATA);
		} while (!(*data & CL_DVFS_MONITOR_DATA_NEW) &&
			 (cld->mode > TEGRA_CL_DVFS_DISABLED));
	}
}

static inline u32 get_last_output(struct tegra_cl_dvfs *cld)
{
	switch_monitor(cld, CL_DVFS_MONITOR_CTRL_OUT);
	return cl_dvfs_readl(cld, CL_DVFS_MONITOR_DATA) &
		CL_DVFS_MONITOR_DATA_MASK;
}

/* out monitored before forced value applied - return the latter if enabled */
static inline u32 cl_dvfs_get_output(struct tegra_cl_dvfs *cld)
{
	u32 val = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_FORCE);
	if (val & CL_DVFS_OUTPUT_FORCE_ENABLE)
		return val & OUT_MASK;

	switch_monitor(cld, CL_DVFS_MONITOR_CTRL_OUT);
	wait_data_new(cld, &val);
	return filter_monitor_data(cld, &val) ? : val;
}

static inline bool is_i2c(struct tegra_cl_dvfs *cld)
{
	return cld->p_data->pmu_if == TEGRA_CL_DVFS_PMU_I2C;
}

static inline u8 get_output_bottom(struct tegra_cl_dvfs *cld)
{
	return is_i2c(cld) ? 0 : cld->out_map[0]->reg_value;
}

static inline u8 get_output_top(struct tegra_cl_dvfs *cld)
{
	return is_i2c(cld) ?  cld->num_voltages - 1 :
		cld->out_map[cld->num_voltages - 1]->reg_value;
}

static inline int get_mv(struct tegra_cl_dvfs *cld, u32 out_val)
{
	return is_i2c(cld) ? cld->out_map[out_val]->reg_uV / 1000 :
		cld->p_data->vdd_map[out_val].reg_uV / 1000;
}

static inline bool is_vmin_delivered(struct tegra_cl_dvfs *cld)
{
	if (is_i2c(cld)) {
		u32 val = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
		val = (val >> CL_DVFS_I2C_STS_I2C_LAST_SHIFT) & OUT_MASK;
		return val >= cld->lut_min;
	}
	/* PWM cannot be stalled */
	return true;
}

static int tegra_pinctrl_set_tristate(struct tegra_cl_dvfs_platform_data *d,
		int group_sel, int tristate)
{
	int ret;
	unsigned long config = TEGRA_PINCONF_PACK(TEGRA_PINCONF_PARAM_TRISTATE,
					tristate);
	if (!d->u.pmu_pwm.pinctrl_dev) {
		pr_err("%s(): ERROR: No Tegra pincontrol driver\n", __func__);
		return -EINVAL;
	}

	ret = pinctrl_set_config_for_group_sel_any_context(
		d->u.pmu_pwm.pinctrl_dev, group_sel, config);
	if (ret < 0)
		pr_err("%s(): ERROR: pinconfig for pin group %d failed: %d\n",
			__func__, group_sel, ret);
	return ret;
}

static int output_enable(struct tegra_cl_dvfs *cld)
{
	if (is_i2c(cld)) {
		u32 val = cl_dvfs_i2c_readl(cld, CL_DVFS_OUTPUT_CFG);
		val |= CL_DVFS_OUTPUT_CFG_I2C_ENABLE;
		cl_dvfs_i2c_writel(cld, val, CL_DVFS_OUTPUT_CFG);
		cl_dvfs_i2c_wmb(cld);
	} else {
		u32 val = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_CFG);
		struct tegra_cl_dvfs_platform_data *d = cld->p_data;
		if (d->u.pmu_pwm.pwm_bus == TEGRA_CL_DVFS_PWM_1WIRE_BUFFER) {
			int gpio = d->u.pmu_pwm.out_gpio;
			int v = d->u.pmu_pwm.out_enable_high ? 1 : 0;
			__gpio_set_value(gpio, v);
			return 0;
		}

		if (d->u.pmu_pwm.pwm_bus == TEGRA_CL_DVFS_PWM_1WIRE_DIRECT) {
			int pg = d->u.pmu_pwm.pwm_pingroup;
			tegra_pinctrl_set_tristate(d, pg, TEGRA_PIN_DISABLE);
			return 0;
		}

		val |= CL_DVFS_OUTPUT_CFG_PWM_ENABLE;
		cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_CFG);
		cl_dvfs_wmb(cld);
	}

	return  0;
}

static int output_disable_pwm(struct tegra_cl_dvfs *cld)
{
	u32 val;
	struct tegra_cl_dvfs_platform_data *d = cld->p_data;

	if (d->u.pmu_pwm.pwm_bus == TEGRA_CL_DVFS_PWM_1WIRE_BUFFER) {
		int gpio = d->u.pmu_pwm.out_gpio;
		int v = d->u.pmu_pwm.out_enable_high ? 0 : 1;
		__gpio_set_value(gpio, v);
		return 0;
	}

	if (d->u.pmu_pwm.pwm_bus == TEGRA_CL_DVFS_PWM_1WIRE_DIRECT) {
		int pg = d->u.pmu_pwm.pwm_pingroup;
		tegra_pinctrl_set_tristate(d, pg, TEGRA_PIN_ENABLE);
		return 0;
	}

	val = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_CFG);
	val &= ~CL_DVFS_OUTPUT_CFG_PWM_ENABLE;
	cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_CFG);
	cl_dvfs_wmb(cld);
	return  0;
}

static noinline int output_flush_disable(struct tegra_cl_dvfs *cld)
{
	int i;
	u32 sts;
	u32 val = cl_dvfs_i2c_readl(cld, CL_DVFS_OUTPUT_CFG);

	/* Flush transactions in flight, and then disable */
	for (i = 0; i < CL_DVFS_OUTPUT_PENDING_TIMEOUT / 2; i++) {
		sts = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
		udelay(2);
		if (!(sts & CL_DVFS_I2C_STS_I2C_REQ_PENDING)) {
			sts = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
			if (!(sts & CL_DVFS_I2C_STS_I2C_REQ_PENDING)) {
				val &= ~CL_DVFS_OUTPUT_CFG_I2C_ENABLE;
				cl_dvfs_i2c_writel(cld, val, CL_DVFS_OUTPUT_CFG);
				wmb();
				sts = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
				if (!(sts & CL_DVFS_I2C_STS_I2C_REQ_PENDING))
					return 0; /* no pending rqst */

				/* Re-enable, continue wait */
				val |= CL_DVFS_OUTPUT_CFG_I2C_ENABLE;
				cl_dvfs_i2c_writel(cld, val, CL_DVFS_OUTPUT_CFG);
				wmb();
			}
		}
	}

	/* I2C request is still pending - disable, anyway, but report error */
	val &= ~CL_DVFS_OUTPUT_CFG_I2C_ENABLE;
	cl_dvfs_i2c_writel(cld, val, CL_DVFS_OUTPUT_CFG);
	cl_dvfs_i2c_wmb(cld);
	return -ETIMEDOUT;
}

static noinline int output_disable_flush(struct tegra_cl_dvfs *cld)
{
	int i;
	u32 sts;
	u32 val = cl_dvfs_i2c_readl(cld, CL_DVFS_OUTPUT_CFG);

	/* Disable output interface right away */
	val &= ~CL_DVFS_OUTPUT_CFG_I2C_ENABLE;
	cl_dvfs_i2c_writel(cld, val, CL_DVFS_OUTPUT_CFG);
	cl_dvfs_i2c_wmb(cld);

	/* Flush possible transaction in flight */
	for (i = 0; i < CL_DVFS_OUTPUT_PENDING_TIMEOUT / 2; i++) {
		sts = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
		udelay(2);
		if (!(sts & CL_DVFS_I2C_STS_I2C_REQ_PENDING)) {
			sts = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
			if (!(sts & CL_DVFS_I2C_STS_I2C_REQ_PENDING))
				return 0;
		}
	}

	/* I2C request is still pending - report error */
	return -ETIMEDOUT;
}

static inline int output_disable_ol_prepare(struct tegra_cl_dvfs *cld)
{
	/* PWM output control */
	if (!is_i2c(cld)) {
		/*
		 * Keep PWM running in open loop mode. External idle controller
		 * would take care of switching PWM output off/on if override
		 * is supported.
		 */
		if (cld->p_data->flags & TEGRA_CL_DVFS_HAS_IDLE_OVERRIDE)
			return 0;
		return output_disable_pwm(cld);
	}

	/*
	 * If cl-dvfs h/w does not require output to be quiet before disable,
	 * s/w can stop I2C communications at any time (including operations
	 * in closed loop mode), and I2C bus integrity is guaranteed even in
	 * case of flush timeout.
	 */
	if (!(cld->p_data->flags & TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET)) {
		int ret = output_disable_flush(cld);
		if (ret)
			pr_debug("cl_dvfs: I2C pending timeout ol_prepare\n");
		return ret;
	}
	return 0;
}

static inline int output_disable_post_ol(struct tegra_cl_dvfs *cld)
{
	/* PWM output control */
	if (!is_i2c(cld))
		return 0;

	/*
	 * If cl-dvfs h/w requires output to be quiet before disable, s/w
	 * should stop I2C communications only after the switch to open loop
	 * mode, and I2C bus integrity is not guaranteed in case of flush
	 * timeout
	*/
	if (cld->p_data->flags & TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET) {
		int ret = output_flush_disable(cld);
		if (ret)
			pr_err("cl_dvfs: I2C pending timeout post_ol\n");
		return ret;
	}
	return 0;
}

static inline void set_mode(struct tegra_cl_dvfs *cld,
			    enum tegra_cl_dvfs_ctrl_mode mode)
{
	cld->mode = mode;
	cl_dvfs_writel(cld, mode - 1, CL_DVFS_CTRL);

	if (cld->p_data->flags & TEGRA_CL_DVFS_HAS_IDLE_OVERRIDE) {
		/* Override mode follows active mode up to open loop */
		u32 val = cl_dvfs_readl(cld, CL_DVFS_CC4_HVC);
		val &= ~(CL_DVFS_CC4_HVC_CTRL_MASK | CL_DVFS_CC4_HVC_FORCE_EN);
		if (mode >= TEGRA_CL_DVFS_OPEN_LOOP) {
			val |= (TEGRA_CL_DVFS_OPEN_LOOP - 1);
			val |= CL_DVFS_CC4_HVC_FORCE_EN;
		}
		cl_dvfs_writel(cld, val, CL_DVFS_CC4_HVC);
	}
	cl_dvfs_wmb(cld);
}

static inline u8 get_output_cap(struct tegra_cl_dvfs *cld,
				struct dfll_rate_req *req)
{
	u32 thermal_cap = get_output_top(cld);

	if (cld->therm_cap_idx && (cld->therm_cap_idx <= cld->therm_caps_num))
		thermal_cap = cld->thermal_out_caps[cld->therm_cap_idx - 1];
	if (req && (req->cap < thermal_cap))
		return req->cap;
	return thermal_cap;
}

static inline u8 get_output_min(struct tegra_cl_dvfs *cld)
{
	u32 tune_min = get_output_bottom(cld);
	u32 thermal_min = tune_min;

	tune_min = cld->tune_state == TEGRA_CL_DVFS_TUNE_LOW ?
		tune_min : cld->tune_high_out_min;

	if (cld->therm_floor_idx < cld->therm_floors_num)
		thermal_min = cld->thermal_out_floors[cld->therm_floor_idx];

	/* return max of all the possible output min settings */
	return max_t(u8, max(tune_min, thermal_min),
					cld->rail_relations_out_min);
}

static inline void _load_lut(struct tegra_cl_dvfs *cld)
{
	int i;
	u32 val;

	val = cld->out_map[cld->lut_min]->reg_value;
	for (i = 0; i <= cld->lut_min; i++)
		cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_LUT + i * 4);

	for (; i < cld->lut_max; i++) {
		val = cld->out_map[i]->reg_value;
		cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_LUT + i * 4);
	}

	val = cld->out_map[cld->lut_max]->reg_value;
	for (; i < cld->num_voltages; i++)
		cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_LUT + i * 4);

	cl_dvfs_i2c_wmb(cld);
}

static void cl_dvfs_load_lut(struct tegra_cl_dvfs *cld)
{
	u32 val = cl_dvfs_i2c_readl(cld, CL_DVFS_OUTPUT_CFG);
	bool disable_out_for_load =
		!(cld->p_data->flags & TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET) &&
		(val & CL_DVFS_OUTPUT_CFG_I2C_ENABLE);

	if (disable_out_for_load) {
		val &= ~CL_DVFS_OUTPUT_CFG_I2C_ENABLE;
		cl_dvfs_i2c_writel(cld, val, CL_DVFS_OUTPUT_CFG);
		cl_dvfs_i2c_wmb(cld);
		udelay(2); /* 2us (big margin) window for disable propafation */
	}

	_load_lut(cld);

	if (disable_out_for_load) {
		val |= CL_DVFS_OUTPUT_CFG_I2C_ENABLE;
		cl_dvfs_i2c_writel(cld, val, CL_DVFS_OUTPUT_CFG);
		cl_dvfs_i2c_wmb(cld);
	}
}

#define set_tune_state(cld, state) \
	do {								\
		cld->tune_state = state;				\
		pr_debug("%s: set tune state %d\n", __func__, state);	\
	} while (0)

static inline void tune_low(struct tegra_cl_dvfs *cld)
{
	/* a must order: 1st tune dfll low, then tune trimmers low */
	cl_dvfs_writel(cld, cld->tune0_low, CL_DVFS_TUNE0);
	cl_dvfs_wmb(cld);
	if (cld->safe_dvfs->dfll_data.tune_trimmers)
		cld->safe_dvfs->dfll_data.tune_trimmers(false);
}

static inline void tune_high(struct tegra_cl_dvfs *cld)
{
	/* a must order: 1st tune trimmers high, then tune dfll high */
	if (cld->safe_dvfs->dfll_data.tune_trimmers)
		cld->safe_dvfs->dfll_data.tune_trimmers(true);
	cl_dvfs_writel(cld, cld->tune0_high, CL_DVFS_TUNE0);
	cl_dvfs_wmb(cld);
}

static inline int cl_tune_target(struct tegra_cl_dvfs *cld, unsigned long rate)
{
	bool tune_low_at_cold = cld->safe_dvfs->dfll_data.tune0_low_at_cold;

	if ((rate >= cld->tune_high_target_rate_min) &&
	    (!tune_low_at_cold || cld->therm_floor_idx))
		return TEGRA_CL_DVFS_TUNE_HIGH;
	return TEGRA_CL_DVFS_TUNE_LOW;
}

static void set_output_limits(struct tegra_cl_dvfs *cld, u8 out_min, u8 out_max)
{
	seqcount_t *vmin_seqcnt = NULL;
	seqcount_t *vmax_seqcnt = NULL;

	if (cld->v_limits.clamped)
		return;

	if ((cld->lut_min != out_min) || (cld->lut_max != out_max)) {
		/* limits update tracking start */
		if (cld->lut_min != out_min) {
			vmin_seqcnt = &cld->v_limits.vmin_seqcnt;
			write_seqcount_begin(vmin_seqcnt);
			cld->v_limits.vmin = get_mv(cld, out_min);
		}
		if (cld->lut_max != out_max) {
			vmax_seqcnt = &cld->v_limits.vmax_seqcnt;
			write_seqcount_begin(vmax_seqcnt);
			cld->v_limits.vmax = get_mv(cld, out_max);
		}

		cld->lut_min = out_min;
		cld->lut_max = out_max;
		if (cld->p_data->flags & TEGRA_CL_DVFS_DYN_OUTPUT_CFG) {
			u32 val = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_CFG);
			val &= ~(CL_DVFS_OUTPUT_CFG_MAX_MASK |
				 CL_DVFS_OUTPUT_CFG_MIN_MASK);
			val |= out_max << CL_DVFS_OUTPUT_CFG_MAX_SHIFT;
			val |= out_min << CL_DVFS_OUTPUT_CFG_MIN_SHIFT;
			cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_CFG);
		} else {
			cl_dvfs_load_lut(cld);
		}

		if (vmin_seqcnt &&
		    (cld->p_data->flags & TEGRA_CL_DVFS_HAS_IDLE_OVERRIDE)) {
			/* Override mode force value follows active mode Vmin */
			u32 val = cl_dvfs_readl(cld, CL_DVFS_CC4_HVC);
			val &= ~CL_DVFS_CC4_HVC_FORCE_VAL_MASK;
			val |= out_min << CL_DVFS_CC4_HVC_FORCE_VAL_SHIFT;
			cl_dvfs_writel(cld, val, CL_DVFS_CC4_HVC);
		}
		cl_dvfs_wmb(cld);

		/* limits update tracking end */
		if (vmin_seqcnt)
			write_seqcount_end(vmin_seqcnt);
		if (vmax_seqcnt)
			write_seqcount_end(vmax_seqcnt);

		pr_debug("cl_dvfs limits_mV [%d : %d]\n",
			 cld->v_limits.vmin, cld->v_limits.vmax);
	}
}

static void cl_dvfs_set_force_out_min(struct tegra_cl_dvfs *cld);
static void set_cl_config(struct tegra_cl_dvfs *cld, struct dfll_rate_req *req)
{
	bool sample_tune_out_last = false;
	u8 cap_gb = CL_DVFS_CAP_GUARD_BAND_STEPS;
	u8 out_max, out_min;
	u8 out_cap = get_output_cap(cld, req);
	struct dvfs_rail *rail = cld->safe_dvfs->dvfs_rail;

	switch (cld->tune_state) {
	case TEGRA_CL_DVFS_TUNE_LOW:
		if (cl_tune_target(cld, req->rate) > TEGRA_CL_DVFS_TUNE_LOW) {
			set_tune_state(cld, TEGRA_CL_DVFS_TUNE_HIGH_REQUEST);
			hrtimer_start(&cld->tune_timer, cld->tune_delay,
				      HRTIMER_MODE_REL);
			cl_dvfs_set_force_out_min(cld);
			sample_tune_out_last = true;
		}
		break;

	case TEGRA_CL_DVFS_TUNE_HIGH:
	case TEGRA_CL_DVFS_TUNE_HIGH_REQUEST:
	case TEGRA_CL_DVFS_TUNE_HIGH_REQUEST_2:
		if (cl_tune_target(cld, req->rate) == TEGRA_CL_DVFS_TUNE_LOW) {
			set_tune_state(cld, TEGRA_CL_DVFS_TUNE_LOW);
			tune_low(cld);
			cl_dvfs_set_force_out_min(cld);
		}
		break;
	default:
		BUG();
	}

	/*
	 * Criteria to select new request and output boundaries. Listed in
	 * the order of priorities to resolve conflicts (if any).
	 *
	 * 1) out_min is at/above minimum voltage level for current temperature
	 *    and tuning ranges
	 * 2) out_max is at/above PMIC guard-band forced minimum
	 * 3) new request has at least on step room for regulation: request +/-1
	 *    within [out_min, out_max] interval
	 * 4) new request is at least CL_DVFS_CAP_GUARD_BAND_STEPS below out_max
	 * 5) - if no other rail depends on DFLL rail, out_max is at/above
	 *    minimax level to provide better convergence accuracy for rates
	 *    close to tuning range boundaries
	 *    - if some other rail depends on DFLL rail, out_max should match
	 *    voltage from safe dvfs table used by s/w DVFS on other rails to
	 *    resolve dependencies
	 */
	out_min = get_output_min(cld);
	if (out_cap > (out_min + cap_gb)) {
		req->output = out_cap - cap_gb;
		out_max = out_cap;
	} else {
		req->output = out_min + 1;
		out_max = req->output + 1;
	}

	if (req->output == cld->safe_output) {
		req->output++;
		out_max = max(out_max, (u8)(req->output + 1));
	}

	if (list_empty(&rail->relationships_to))
		out_max = max(out_max, cld->minimax_output);

	out_max = max(out_max, cld->force_out_min);

	set_output_limits(cld, out_min, out_max);

	/* Must be sampled after new out_min is set */
	if (sample_tune_out_last && is_i2c(cld)) {
		u32 val = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
		cld->tune_out_last =
			(val >> CL_DVFS_I2C_STS_I2C_LAST_SHIFT) & OUT_MASK;
	}
}

static void set_ol_config(struct tegra_cl_dvfs *cld)
{
	u32 val, out_min, out_max;

	/* always unclamp and restore limits before open loop */
	if (cld->v_limits.clamped) {
		cld->v_limits.clamped = false;
		set_cl_config(cld, &cld->last_req);
	}
	out_min = cld->lut_min;
	out_max = cld->lut_max;

	/* always tune low (safe) in open loop */
	if (cld->tune_state != TEGRA_CL_DVFS_TUNE_LOW) {
		set_tune_state(cld, TEGRA_CL_DVFS_TUNE_LOW);
		tune_low(cld);

		out_min = get_output_min(cld);
	}
	set_output_limits(cld, out_min, out_max);

	/* 1:1 scaling in open loop */
	val = cl_dvfs_readl(cld, CL_DVFS_FREQ_REQ);
	if (!(cld->p_data->flags & TEGRA_CL_DVFS_SCALE_IN_OPEN_LOOP))
		val |= (SCALE_MAX - 1) << CL_DVFS_FREQ_REQ_SCALE_SHIFT;
	val &= ~CL_DVFS_FREQ_REQ_FORCE_ENABLE;
	cl_dvfs_writel(cld, val, CL_DVFS_FREQ_REQ);
}

static enum hrtimer_restart tune_timer_cb(struct hrtimer *timer)
{
	unsigned long flags;
	u32 val, out_min, out_last;
	struct tegra_cl_dvfs *cld =
		container_of(timer, struct tegra_cl_dvfs, tune_timer);

	clk_lock_save(cld->dfll_clk, &flags);

	if (cld->tune_state == TEGRA_CL_DVFS_TUNE_HIGH_REQUEST) {
		out_min = cld->lut_min;
		val = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
		out_last = is_i2c(cld) ?
			(val >> CL_DVFS_I2C_STS_I2C_LAST_SHIFT) & OUT_MASK :
			out_min; /* no way to stall PWM: out_last >= out_min */

		/*
		 * Update high tune settings if both last I2C value and minimum
		 * output are above high range output threshold, provided I2C
		 * transaction that might be in flight when minimum output was
		 * set has been completed. The latter condition is true if no
		 * transaction is pending or I2C last value has changed since
		 * minimum limit was set.
		 *
		 * Since PWM mode never has pending indicator set, high tune
		 * settings are updated always.
		 */
		if (!(val & CL_DVFS_I2C_STS_I2C_REQ_PENDING) ||
		    (cld->tune_out_last != out_last)) {
			cld->tune_out_last = cld->num_voltages;
		}

		if ((cld->tune_out_last == cld->num_voltages) &&
		    (out_last >= cld->tune_high_out_min)  &&
		    (out_min >= cld->tune_high_out_min)) {
			set_tune_state(cld, TEGRA_CL_DVFS_TUNE_HIGH_REQUEST_2);
			hrtimer_start(&cld->tune_timer, cld->tune_ramp,
				      HRTIMER_MODE_REL);
		} else {
			hrtimer_start(&cld->tune_timer, cld->tune_delay,
				      HRTIMER_MODE_REL);
		}
	} else if (cld->tune_state == TEGRA_CL_DVFS_TUNE_HIGH_REQUEST_2) {
		set_tune_state(cld, TEGRA_CL_DVFS_TUNE_HIGH);
		tune_high(cld);
	}
	clk_unlock_restore(cld->dfll_clk, &flags);

	return HRTIMER_NORESTART;
}

static inline void calibration_timer_update(struct tegra_cl_dvfs *cld)
{
	/*
	 * Forced output must be disabled in closed loop mode outside of
	 * calibration. It may be temporarily enabled during calibration;
	 * use timer update to clean up.
	 */
	output_force_disable(cld);

	if (cld->calibration_delay)
		mod_timer(&cld->calibration_timer,
			  jiffies + cld->calibration_delay + 1);
}

static void cl_dvfs_calibrate(struct tegra_cl_dvfs *cld)
{
	u32 val, data;
	ktime_t now;
	unsigned long rate;
	unsigned long step = RATE_STEP(cld);
	unsigned long rate_min = cld->dvco_rate_min;
	u8 out_min = get_output_min(cld);

	if (!cld->calibration_delay)
		return;
	/*
	 *  Enter calibration procedure only if
	 *  - closed loop operations
	 *  - last request engaged clock skipper
	 *  - at least specified time after the last calibration attempt
	 */
	if ((cld->mode != TEGRA_CL_DVFS_CLOSED_LOOP) ||
	    (cld->last_req.rate > rate_min))
		return;

	now = ktime_get();
	if (ktime_us_delta(now, cld->last_calibration) <
	    jiffies_to_usecs(cld->calibration_delay))
		return;
	cld->last_calibration = now;

	/* Defer calibration if in the middle of tuning transition */
	if ((cld->tune_state > TEGRA_CL_DVFS_TUNE_LOW) &&
	    (cld->tune_state < TEGRA_CL_DVFS_TUNE_HIGH)) {
		calibration_timer_update(cld);
		return;
	}

	/* Defer calibration if forced output was left enabled */
	val = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_FORCE);
	if (val & CL_DVFS_OUTPUT_FORCE_ENABLE) {
		calibration_timer_update(cld);
		return;
	}

	/*
	 * Check if we need to force minimum output during calibration.
	 *
	 * Considerations for selecting TEGRA_CL_DVFS_CALIBRATE_FORCE_VMIN.
	 * - if there is no voltage enforcement underneath this driver, no need
	 * to select defer option.
	 *
	 *  - if SoC has internal pm controller that controls voltage while CPU
	 * cluster is idle, and restores force_val on idle exit, the following
	 * trade-offs applied:
	 *
	 * a) force: DVCO calibration is accurate, but calibration time is
	 * increased by 2 sample periods and target module maybe under-clocked
	 * during that time,
	 * b) don't force: calibration results depend on whether flag
	 * TEGRA_CL_DVFS_DEFER_FORCE_CALIBRATE is set -- see description below.
	 */
	if (cld->p_data->flags & TEGRA_CL_DVFS_CALIBRATE_FORCE_VMIN) {
		int delay = 2 * GET_SAMPLE_PERIOD(cld);
		val = output_force_set_val(cld, out_min);
		output_force_enable(cld, val);
		udelay(delay);
	}

	/* Synchronize with sample period, and get rate measurements */
	switch_monitor(cld, CL_DVFS_MONITOR_CTRL_FREQ);

	if (cld->p_data->flags & TEGRA_CL_DVFS_DATA_NEW_NO_USE) {
		/* Cannot use DATA_NEW synch - get data after one full sample
		   period (with 10us margin) */
		int delay = GET_SAMPLE_PERIOD(cld) + 10;
		udelay(delay);
	}
	wait_data_new(cld, &data);
	wait_data_new(cld, &data);

	/* Defer calibration if data reading is not consistent */
	if (filter_monitor_data(cld, &data)) {
		calibration_timer_update(cld);
		return;
	}

	/* Get output (voltage) measurements */
	if (is_i2c(cld)) {
		/* Defer calibration if I2C transaction is pending */
		val = cl_dvfs_readl(cld, CL_DVFS_I2C_STS);
		if (val & CL_DVFS_I2C_STS_I2C_REQ_PENDING) {
			calibration_timer_update(cld);
			return;
		}
		val = (val >> CL_DVFS_I2C_STS_I2C_LAST_SHIFT) & OUT_MASK;
	} else if (cld->p_data->flags & TEGRA_CL_DVFS_CALIBRATE_FORCE_VMIN) {
		/* Use forced value (cannot read it back from PWM interface) */
		val = out_min;
	} else {
		/* Get last output (there is no such thing as pending PWM) */
		val = get_last_output(cld);

		/* Defer calibration if data reading is not consistent */
		if (filter_monitor_data(cld, &val)) {
			calibration_timer_update(cld);
			return;
		}
	}

	if (cld->p_data->flags & TEGRA_CL_DVFS_CALIBRATE_FORCE_VMIN) {
		/* Defer calibration if forced and read outputs do not match */
		if (val != out_min) {
			calibration_timer_update(cld);
			return;
		}
		output_force_disable(cld);
	}

	/*
	 * Check if we need to defer calibration when voltage is matching
	 * request force_val.
	 *
	 * Considerations for selecting TEGRA_CL_DVFS_DEFER_FORCE_CALIBRATE.
	 * - if there is no voltage enforcement underneath this driver, no need
	 * to select defer option.
	 *
	 *  - if SoC has internal pm controller that controls voltage while CPU
	 * cluster is idle, and restores force_val on idle exit, the following
	 * trade-offs applied:
	 *
	 * a) defer: DVCO minimum maybe slightly over-estimated, all frequencies
	 * below DVCO minimum are skipped-to accurately, but voltage at low
	 * frequencies would fluctuate between Vmin and Vmin + 1 LUT/PWM step.
	 * b) don't defer: DVCO minimum rate is underestimated, maybe down to
	 * calibration_range_min, respectively actual frequencies below DVCO
	 * minimum are configured higher than requested, but voltage at low
	 * frequencies is saturated at Vmin.
	 */
	if ((val == cld->last_req.output) &&
	    (cld->p_data->flags & TEGRA_CL_DVFS_DEFER_FORCE_CALIBRATE)) {
		calibration_timer_update(cld);
		return;
	}

	/* Adjust minimum rate */
	rate = GET_MONITORED_RATE(data, cld->ref_rate);
	if ((val > out_min) || (rate < (rate_min - step)))
		rate_min -= step;
	else if (rate > (rate_min + step))
		rate_min += step;
	else {
		if ((cld->tune_state == TEGRA_CL_DVFS_TUNE_HIGH) &&
		    (cld->tune_high_out_min == out_min)) {
			cld->tune_high_dvco_rate_min = rate_min;
			return;
		}
		if (cld->thermal_out_floors[cld->therm_floor_idx] == out_min) {
			cld->dvco_rate_floors[cld->therm_floor_idx] = rate_min;
			return;
		}
		calibration_timer_update(cld);
		return;
	}

	cld->dvco_rate_min = clamp(rate_min,
			cld->calibration_range_min, cld->calibration_range_max);
	calibration_timer_update(cld);
	pr_debug("%s: calibrated dvco_rate_min %lu (%lu), measured %lu\n",
		 __func__, cld->dvco_rate_min, rate_min, rate);
}

static void calibration_timer_cb(unsigned long data)
{
	unsigned long flags, rate_min;
	struct tegra_cl_dvfs *cld = (struct tegra_cl_dvfs *)data;

	pr_debug("%s\n", __func__);

	clk_lock_save(cld->dfll_clk, &flags);
	rate_min = cld->dvco_rate_min;
	cl_dvfs_calibrate(cld);
	if (rate_min != cld->dvco_rate_min) {
		tegra_cl_dvfs_request_rate(cld,
			tegra_cl_dvfs_request_get(cld));
	}
	clk_unlock_restore(cld->dfll_clk, &flags);
}

static void set_request(struct tegra_cl_dvfs *cld, struct dfll_rate_req *req)
{
	u32 val, f;
	int force_val = req->output - cld->safe_output;
	int coef = 128; /* FIXME: cld->p_data->cfg_param->cg_scale? */;

	/* If going down apply force output floor */
	val = cl_dvfs_readl(cld, CL_DVFS_FREQ_REQ);
	f = (val & CL_DVFS_FREQ_REQ_FREQ_MASK) >> CL_DVFS_FREQ_REQ_FREQ_SHIFT;
	if ((!(val & CL_DVFS_FREQ_REQ_FREQ_VALID) || (f > req->freq)) &&
	    (cld->force_out_min > req->output))
		force_val = cld->force_out_min - cld->safe_output;

	force_val = force_val * coef / cld->p_data->cfg_param->cg;
	force_val = clamp(force_val, FORCE_MIN, FORCE_MAX);

	/*
	 * 1st set new frequency request and force values, then set force enable
	 * bit (if not set already). Use same CL_DVFS_FREQ_REQ register read
	 * (not other cl_dvfs register) plus explicit delay as a fence.
	 */
	val &= CL_DVFS_FREQ_REQ_FORCE_ENABLE;
	val |= req->freq << CL_DVFS_FREQ_REQ_FREQ_SHIFT;
	val |= req->scale << CL_DVFS_FREQ_REQ_SCALE_SHIFT;
	val |= ((u32)force_val << CL_DVFS_FREQ_REQ_FORCE_SHIFT) &
		CL_DVFS_FREQ_REQ_FORCE_MASK;
	val |= CL_DVFS_FREQ_REQ_FREQ_VALID;
	cl_dvfs_writel(cld, val, CL_DVFS_FREQ_REQ);
	wmb();
	val = cl_dvfs_readl(cld, CL_DVFS_FREQ_REQ);

	if (!(val & CL_DVFS_FREQ_REQ_FORCE_ENABLE)) {
		udelay(1);  /* 1us (big margin) window for force value settle */
		val |= CL_DVFS_FREQ_REQ_FORCE_ENABLE;
		cl_dvfs_writel(cld, val, CL_DVFS_FREQ_REQ);
		cl_dvfs_wmb(cld);
	}
}

static u8 find_mv_out_cap(struct tegra_cl_dvfs *cld, int mv)
{
	u8 cap;
	int uv;

	for (cap = 0; cap < cld->num_voltages; cap++) {
		uv = cld->out_map[cap]->reg_uV;
		if (uv / 1000 >= mv)
			return is_i2c(cld) ? cap : cld->out_map[cap]->reg_value;
	}
	return get_output_top(cld);	/* maximum possible output */
}

static u8 find_mv_out_floor(struct tegra_cl_dvfs *cld, int mv)
{
	u8 floor;
	int uv;

	for (floor = 0; floor < cld->num_voltages; floor++) {
		uv = cld->out_map[floor]->reg_uV;
		if (uv / 1000 > mv) {
			if (!floor)	/* minimum possible output */
				return get_output_bottom(cld);
			break;
		}
	}
	return is_i2c(cld) ? floor - 1 : cld->out_map[floor - 1]->reg_value;
}

static int find_safe_output(
	struct tegra_cl_dvfs *cld, unsigned long rate, u8 *safe_output)
{
	int i;
	int n = cld->safe_dvfs->num_freqs;
	unsigned long *freqs = cld->safe_dvfs->freqs;

	for (i = 0; i < n; i++) {
		if (freqs[i] >= rate) {
			*safe_output = cld->clk_dvfs_map[i];
			return 0;
		}
	}
	return -EINVAL;
}

/* Return rate with predicted voltage closest/below or equal out_min */
static unsigned long get_dvco_rate_below(struct tegra_cl_dvfs *cld, u8 out_min)
{
	int i;

	for (i = 0; i < cld->safe_dvfs->num_freqs; i++) {
		if (cld->clk_dvfs_map[i] > out_min)
			break;
	}
	i = i ? i-1 : 0;
	return cld->safe_dvfs->freqs[i];
}

/* Return rate with predicted voltage closest/above out_min */
static unsigned long get_dvco_rate_above(struct tegra_cl_dvfs *cld, u8 out_min)
{
	int i;

	for (i = 0; i < cld->safe_dvfs->num_freqs; i++) {
		if (cld->clk_dvfs_map[i] > out_min)
			return cld->safe_dvfs->freqs[i];
	}
	return cld->safe_dvfs->freqs[i-1];
}

static void cl_dvfs_set_dvco_rate_min(struct tegra_cl_dvfs *cld,
				      struct dfll_rate_req *req)
{
	unsigned long range = 32 * RATE_STEP(cld);	/* 800 MHz */
	unsigned long tune_high_range_min = 0;
	unsigned long rate = cld->dvco_rate_floors[cld->therm_floor_idx];
	if (!rate) {
		rate = cld->safe_dvfs->dfll_data.out_rate_min;
		if (cld->therm_floor_idx < cld->therm_floors_num)
			rate = get_dvco_rate_below(cld,
				cld->thermal_out_floors[cld->therm_floor_idx]);
	}

	if (cl_tune_target(cld, req->rate) > TEGRA_CL_DVFS_TUNE_LOW) {
		rate = max(rate, cld->tune_high_dvco_rate_min);
		tune_high_range_min = cld->tune_high_target_rate_min;
	}

	/* round minimum rate to request unit (ref_rate/2) boundary */
	cld->dvco_rate_min = ROUND_MIN_RATE(rate, cld->ref_rate);
	pr_debug("%s: calibrated dvco_rate_min %lu\n",
		 __func__, cld->dvco_rate_min);

	/* set symmetrical calibration boundaries */
	cld->calibration_range_min = cld->dvco_rate_min > range ?
		cld->dvco_rate_min - range : 0;
	if (cld->calibration_range_min < tune_high_range_min)
		cld->calibration_range_min = tune_high_range_min;
	if (cld->calibration_range_min < cld->safe_dvfs->freqs[0])
		cld->calibration_range_min = cld->safe_dvfs->freqs[0];
	cld->calibration_range_max = cld->dvco_rate_min + range;
	rate = cld->safe_dvfs->freqs[cld->safe_dvfs->num_freqs - 1];
	if (cld->calibration_range_max > rate)
		cld->calibration_range_max = rate;
}

static void cl_dvfs_set_force_out_min(struct tegra_cl_dvfs *cld)
{
	u8 force_out_min;
	int force_mv_min = cld->p_data->pmu_undershoot_gb;

	if (!force_mv_min) {
		cld->force_out_min = get_output_bottom(cld);
		return;
	}

	WARN_ONCE(!list_empty(&cld->safe_dvfs->dvfs_rail->relationships_to),
		  "%s: PMIC undershoot must fit DFLL rail dependency-to slack",
		  __func__);

	force_out_min = get_output_min(cld);
	force_mv_min += get_mv(cld, force_out_min);
	force_out_min = find_mv_out_cap(cld, force_mv_min);
	if (force_out_min == cld->safe_output)
		force_out_min++;
	cld->force_out_min = force_out_min;
}

static struct voltage_reg_map *find_vdd_map_entry(
	struct tegra_cl_dvfs *cld, int mV, bool exact)
{
	int i, uninitialized_var(reg_mV);

	for (i = 0; i < cld->p_data->vdd_map_size; i++) {
		/* round down to 1mV */
		reg_mV = cld->p_data->vdd_map[i].reg_uV / 1000;
		if (mV <= reg_mV)
			break;
	}

	if (i < cld->p_data->vdd_map_size) {
		if (!exact || (mV == reg_mV))
			return &cld->p_data->vdd_map[i];
	}
	return NULL;
}

static void cl_dvfs_init_maps(struct tegra_cl_dvfs *cld)
{
	int i, j, v, v_max, n;
	const int *millivolts;
	struct voltage_reg_map *m;

	BUILD_BUG_ON(MAX_CL_DVFS_VOLTAGES > OUT_MASK + 1);

	n = cld->safe_dvfs->num_freqs;
	BUG_ON(n >= MAX_CL_DVFS_VOLTAGES);

	millivolts = cld->safe_dvfs->dfll_millivolts;
	v_max = millivolts[n - 1];

	v = cld->safe_dvfs->dfll_data.min_millivolts;
	BUG_ON(v > millivolts[0]);

	cld->out_map[0] = find_vdd_map_entry(cld, v, true);
	BUG_ON(!cld->out_map[0]);

	for (i = 0, j = 1; i < n; i++) {
		for (;;) {
			v += max(1, (v_max - v) / (MAX_CL_DVFS_VOLTAGES - j));
			if (v >= millivolts[i])
				break;

			m = find_vdd_map_entry(cld, v, false);
			BUG_ON(!m);
			if (m != cld->out_map[j - 1])
				cld->out_map[j++] = m;
		}

		v = (j == MAX_CL_DVFS_VOLTAGES - 1) ? v_max : millivolts[i];
		m = find_vdd_map_entry(cld, v, true);
		BUG_ON(!m);
		if (m != cld->out_map[j - 1])
			cld->out_map[j++] = m;
		if (is_i2c(cld)) {
			cld->clk_dvfs_map[i] = j - 1;
		} else {
			cld->clk_dvfs_map[i] = cld->out_map[j - 1]->reg_value;
			BUG_ON(cld->clk_dvfs_map[i] > OUT_MASK + 1);
		}

		if (v >= v_max)
			break;
	}
	cld->num_voltages = j;

	/* hit Vmax before last freq was mapped: map the rest to max output */
	for (j = i++; i < n; i++)
		cld->clk_dvfs_map[i] = cld->clk_dvfs_map[j];
}

static void cl_dvfs_init_tuning_thresholds(struct tegra_cl_dvfs *cld)
{
	int mv;

	/*
	 * Convert high tuning voltage threshold into output LUT index, and
	 * add necessary margin.  If voltage threshold is outside operating
	 * range set it at maximum output level to effectively disable tuning
	 * parameters adjustment.
	 */
	cld->tune_high_out_min = get_output_top(cld);
	cld->tune_high_out_start = cld->tune_high_out_min;
	cld->tune_high_dvco_rate_min = ULONG_MAX;
	cld->tune_high_target_rate_min = ULONG_MAX;

	mv = cld->safe_dvfs->dfll_data.tune_high_min_millivolts;
	if (mv >= cld->safe_dvfs->dfll_data.min_millivolts) {
		int margin = cld->safe_dvfs->dfll_data.tune_high_margin_mv ? :
				CL_DVFS_TUNE_HIGH_MARGIN_MV;
		u8 out_min = find_mv_out_cap(cld, mv);
		u8 out_start = find_mv_out_cap(cld, mv + margin);
		out_start = max(out_start, (u8)(out_min + 1));
		if (out_start < get_output_top(cld)) {
			cld->tune_high_out_min = out_min;
			cld->tune_high_out_start = out_start;
			if (cld->minimax_output <= out_start)
				cld->minimax_output = out_start + 1;
			cld->tune_high_dvco_rate_min =
				get_dvco_rate_above(cld, out_start + 1);
			cld->tune_high_target_rate_min =
				get_dvco_rate_above(cld, out_min);
		}
	}
}

static void cl_dvfs_init_hot_output_cap(struct tegra_cl_dvfs *cld)
{
	int i;
	if (!cld->safe_dvfs->dvfs_rail->therm_mv_caps ||
	    !cld->safe_dvfs->dvfs_rail->therm_mv_caps_num)
		return;

	if (!cld->safe_dvfs->dvfs_rail->vmax_cdev)
		WARN(1, "%s: missing dfll cap cooling device\n",
		     cld->safe_dvfs->dvfs_rail->reg_id);
	/*
	 * Convert monotonically decreasing thermal caps at high temperature
	 * into output LUT indexes; make sure there is a room for regulation
	 * below minimum thermal cap.
	 */
	cld->therm_caps_num = cld->safe_dvfs->dvfs_rail->therm_mv_caps_num;
	for (i = 0; i < cld->therm_caps_num; i++) {
		cld->thermal_out_caps[i] = find_mv_out_floor(
			cld, cld->safe_dvfs->dvfs_rail->therm_mv_caps[i]);
	}
	BUG_ON(cld->thermal_out_caps[cld->therm_caps_num - 1] <
	       cld->minimax_output);
}

static void cl_dvfs_convert_cold_output_floor(struct tegra_cl_dvfs *cld,
					      int offset)
{
	int i;
	struct dvfs_rail *rail = cld->safe_dvfs->dvfs_rail;

	/*
	 * Convert monotonically decreasing thermal floors at low temperature
	 * into output LUT indexes; make sure there is a room for regulation
	 * above maximum thermal floor. The latter is also exempt from offset
	 * application.
	 */
	cld->therm_floors_num = rail->therm_mv_floors_num;
	for (i = 0; i < cld->therm_floors_num; i++) {
		int mv = rail->therm_mv_floors[i] + (i ? offset : 0);
		u8 out = cld->thermal_out_floors[i] = find_mv_out_cap(cld, mv);
		cld->thermal_mv_floors[i] = get_mv(cld, out);
	}
	BUG_ON(cld->thermal_out_floors[0] + 1 >= get_output_top(cld));
	if (!rail->therm_mv_dfll_floors) {
		wmb();
		rail->therm_mv_dfll_floors = cld->thermal_mv_floors;
	}
}

static void cl_dvfs_init_cold_output_floor(struct tegra_cl_dvfs *cld)
{
	if (!cld->safe_dvfs->dvfs_rail->therm_mv_floors ||
	    !cld->safe_dvfs->dvfs_rail->therm_mv_floors_num)
		return;

	if (!cld->safe_dvfs->dvfs_rail->vmin_cdev)
		WARN(1, "%s: missing dfll floor cooling device\n",
		     cld->safe_dvfs->dvfs_rail->reg_id);

	/* Most conservative offset 0 always safe */
	cl_dvfs_convert_cold_output_floor(cld, 0);

	if (cld->minimax_output <= cld->thermal_out_floors[0])
		cld->minimax_output = cld->thermal_out_floors[0] + 1;
}

static void cl_dvfs_init_output_thresholds(struct tegra_cl_dvfs *cld)
{
	cld->minimax_output = 0;
	cl_dvfs_init_tuning_thresholds(cld);
	cl_dvfs_init_cold_output_floor(cld);

	/* Append minimum output to thermal floors */
	cld->thermal_out_floors[cld->therm_floors_num] = get_output_bottom(cld);

	/*
	 * make sure safe output is safe at any temperature, and there is a room
	 * for regulation below safe output if force mode is disabled (in force
	 * mode room for regulation is provided by force request that is always
	 * above safe output)
	 */
	cld->safe_output = cld->p_data->cfg_param->force_mode ?
		cld->thermal_out_floors[0] : cld->thermal_out_floors[0] + 1;
	cld->safe_output = max(cld->safe_output,
		(u8)(get_output_bottom(cld) + 1));


	if (cld->minimax_output <= cld->safe_output)
		cld->minimax_output = cld->safe_output + 1;

	/* init caps after minimax output is determined */
	cl_dvfs_init_hot_output_cap(cld);
}

static void cl_dvfs_init_pwm_if(struct tegra_cl_dvfs *cld)
{
	u32 val, div;
	struct tegra_cl_dvfs_platform_data *p_data = cld->p_data;
	bool delta_mode = p_data->u.pmu_pwm.delta_mode;
	int pg = p_data->u.pmu_pwm.pwm_pingroup;
	int pcg = p_data->u.pmu_pwm.pwm_clk_pingroup;

	div = GET_DIV(cld->ref_rate, p_data->u.pmu_pwm.pwm_rate, 1);

	val = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_CFG);
	val |= delta_mode ? CL_DVFS_OUTPUT_CFG_PWM_DELTA : 0;
	val |= (div << CL_DVFS_OUTPUT_CFG_PWM_DIV_SHIFT) &
		CL_DVFS_OUTPUT_CFG_PWM_DIV_MASK;

	/*
	 * Different ways to enable/disable PWM depending on board design:
	 * a) Use native CL-DVFS output PWM_ENABLE control (2WIRE bus)
	 * b) Use gpio control of external buffer (1WIRE bus with buffer)
	 * c) Use tristate PWM pingroup control (1WIRE bus with direct connect)
	 * in cases (b) and (c) keep CL-DVFS native control always enabled
	 */

	switch (p_data->u.pmu_pwm.pwm_bus) {
	case TEGRA_CL_DVFS_PWM_1WIRE_BUFFER:
		tegra_pinctrl_set_tristate(p_data, pg, TEGRA_PIN_DISABLE);
		val |= CL_DVFS_OUTPUT_CFG_PWM_ENABLE;
		break;

	case TEGRA_CL_DVFS_PWM_1WIRE_DIRECT:
		tegra_pinctrl_set_tristate(p_data, pg, TEGRA_PIN_ENABLE);
		val |= CL_DVFS_OUTPUT_CFG_PWM_ENABLE;
		break;

	case TEGRA_CL_DVFS_PWM_2WIRE:
		tegra_pinctrl_set_tristate(p_data, pg, TEGRA_PIN_DISABLE);
		tegra_pinctrl_set_tristate(p_data, pcg, TEGRA_PIN_DISABLE);
		break;

	default:
		BUG();
	}

	cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_CFG);
	cl_dvfs_wmb(cld);
}

static void cl_dvfs_init_i2c_if(struct tegra_cl_dvfs *cld)
{
	u32 val, div;
	struct tegra_cl_dvfs_platform_data *p_data = cld->p_data;
	bool hs_mode = p_data->u.pmu_i2c.hs_rate;

	/* PMU slave address, vdd register offset, and transfer mode */
	val = p_data->u.pmu_i2c.slave_addr << CL_DVFS_I2C_CFG_SLAVE_ADDR_SHIFT;
	if (p_data->u.pmu_i2c.addr_10)
		val |= CL_DVFS_I2C_CFG_SLAVE_ADDR_10;
	if (hs_mode) {
		val |= p_data->u.pmu_i2c.hs_master_code <<
			CL_DVFS_I2C_CFG_HS_CODE_SHIFT;
		val |= CL_DVFS_I2C_CFG_PACKET_ENABLE;
	}
	val |= CL_DVFS_I2C_CFG_SIZE_MASK;
	val |= CL_DVFS_I2C_CFG_ARB_ENABLE;
	cl_dvfs_writel(cld, val, CL_DVFS_I2C_CFG);
	cl_dvfs_writel(cld, p_data->u.pmu_i2c.reg, CL_DVFS_I2C_VDD_REG_ADDR);


	val = GET_DIV(cld->i2c_rate, p_data->u.pmu_i2c.fs_rate, 8);
	BUG_ON(!val || (val > CL_DVFS_I2C_CLK_DIVISOR_MASK));
	val = (val - 1) << CL_DVFS_I2C_CLK_DIVISOR_FS_SHIFT;
	if (hs_mode) {
		div = GET_DIV(cld->i2c_rate, p_data->u.pmu_i2c.hs_rate, 12);
		BUG_ON(!div || (div > CL_DVFS_I2C_CLK_DIVISOR_MASK));
	} else {
		div = 2;	/* default hs divisor just in case */
	}
	val |= (div - 1) << CL_DVFS_I2C_CLK_DIVISOR_HS_SHIFT;
	cl_dvfs_writel(cld, val, CL_DVFS_I2C_CLK_DIVISOR);
	cl_dvfs_i2c_wmb(cld);
}

static void cl_dvfs_init_out_if(struct tegra_cl_dvfs *cld)
{
	u32 val, out_min, out_max;

	/*
	 * Disable output, and set safe voltage and output limits;
	 * disable and clear limit interrupts.
	 */
	cld->tune_state = TEGRA_CL_DVFS_TUNE_LOW;
	cld->therm_cap_idx = cld->therm_caps_num;
	cld->therm_floor_idx = 0;
	cl_dvfs_set_dvco_rate_min(cld, &cld->last_req);
	cl_dvfs_set_force_out_min(cld);

	if (cld->p_data->flags & TEGRA_CL_DVFS_DYN_OUTPUT_CFG) {
		/*
		 * If h/w supports dynamic chanage of output register, limit
		 * LUT * index range using cl_dvfs h/w controls, and load full
		 * range LUT table once.
		 */
		out_min = get_output_min(cld);
		out_max = get_output_cap(cld, NULL);
		cld->lut_min = get_output_bottom(cld);
		cld->lut_max = get_output_top(cld);
	} else {
		/* LUT available only for I2C, no dynamic config WAR for PWM */
		BUG_ON(!is_i2c(cld));

		/*
		 * Allow the entire range of LUT indexes, but limit output
		 * voltage in LUT mapping (this "indirect" application of limits
		 * is used, because h/w does not support dynamic change of index
		 * limits, but dynamic reload of LUT is fine).
		 */
		out_min = get_output_bottom(cld);
		out_max = get_output_top(cld);
		cld->lut_min = get_output_min(cld);
		cld->lut_max = get_output_cap(cld, NULL);
	}

	/*
	 * Disable output interface. If configuration and I2C address spaces
	 * are separated, output enable/disable control and output limits are
	 * in different apertures and output must be disabled 1st to avoid
	 * spurious I2C transaction. If configuration and I2C address spaces
	 * are combined output enable/disable control and output limits are
	 * in the same register, and it is safe to just clear it.
	 */
	cl_dvfs_i2c_writel(cld, 0, CL_DVFS_OUTPUT_CFG);
	cl_dvfs_i2c_wmb(cld);

	val = (cld->safe_output << CL_DVFS_OUTPUT_CFG_SAFE_SHIFT) |
		(out_max << CL_DVFS_OUTPUT_CFG_MAX_SHIFT) |
		(out_min << CL_DVFS_OUTPUT_CFG_MIN_SHIFT);
	cl_dvfs_writel(cld, val, CL_DVFS_OUTPUT_CFG);
	if (cld->p_data->flags & TEGRA_CL_DVFS_HAS_IDLE_OVERRIDE) {
		val = out_min << CL_DVFS_CC4_HVC_FORCE_VAL_SHIFT;
		cl_dvfs_writel(cld, val, CL_DVFS_CC4_HVC);
	}
	cl_dvfs_wmb(cld);

	cl_dvfs_writel(cld, 0, CL_DVFS_OUTPUT_FORCE);
	cl_dvfs_writel(cld, 0, CL_DVFS_INTR_EN);
	cl_dvfs_writel(cld, CL_DVFS_INTR_MAX_MASK | CL_DVFS_INTR_MIN_MASK,
		       CL_DVFS_INTR_STS);

	/* fill in LUT table */
	if (is_i2c(cld))
		cl_dvfs_load_lut(cld);

	if (cld->p_data->flags & TEGRA_CL_DVFS_DYN_OUTPUT_CFG) {
		/* dynamic update of output register allowed - no need to reload
		   lut - use lut limits as output register setting shadow */
		cld->lut_min = out_min;
		cld->lut_max = out_max;
	}
	cld->v_limits.vmin = get_mv(cld, cld->lut_min);
	cld->v_limits.vmax = get_mv(cld, cld->lut_max);

	/* configure transport */
	if (is_i2c(cld))
		cl_dvfs_init_i2c_if(cld);
	else
		cl_dvfs_init_pwm_if(cld);
}

static void cl_dvfs_init_cntrl_logic(struct tegra_cl_dvfs *cld)
{
	u32 val;
	struct tegra_cl_dvfs_cfg_param *param = cld->p_data->cfg_param;

	/* configure mode, control loop parameters, DFLL tuning */
	set_mode(cld, TEGRA_CL_DVFS_DISABLED);

	val = GET_DIV(cld->ref_rate, param->sample_rate, 32);
	BUG_ON(val > CL_DVFS_CONFIG_DIV_MASK);
	cl_dvfs_writel(cld, val, CL_DVFS_CONFIG);

	val = (param->force_mode << CL_DVFS_PARAMS_FORCE_MODE_SHIFT) |
		(param->cf << CL_DVFS_PARAMS_CF_PARAM_SHIFT) |
		(param->ci << CL_DVFS_PARAMS_CI_PARAM_SHIFT) |
		((u8)param->cg << CL_DVFS_PARAMS_CG_PARAM_SHIFT) |
		(param->cg_scale ? CL_DVFS_PARAMS_CG_SCALE : 0);
	cl_dvfs_writel(cld, val, CL_DVFS_PARAMS);

	cl_dvfs_writel(cld, cld->tune0_low, CL_DVFS_TUNE0);
	cl_dvfs_writel(cld, cld->safe_dvfs->dfll_data.tune1, CL_DVFS_TUNE1);
	cl_dvfs_wmb(cld);
	if (cld->safe_dvfs->dfll_data.tune_trimmers)
		cld->safe_dvfs->dfll_data.tune_trimmers(false);

	/* configure droop (skipper 1) and scale (skipper 2) */
	val = GET_DROOP_FREQ(cld->safe_dvfs->dfll_data.droop_rate_min,
			cld->ref_rate) << CL_DVFS_DROOP_CTRL_MIN_FREQ_SHIFT;
	BUG_ON(val > CL_DVFS_DROOP_CTRL_MIN_FREQ_MASK);
	val |= (param->droop_cut_value << CL_DVFS_DROOP_CTRL_CUT_SHIFT);
	val |= (param->droop_restore_ramp << CL_DVFS_DROOP_CTRL_RAMP_SHIFT);
	cl_dvfs_writel(cld, val, CL_DVFS_DROOP_CTRL);

	val = cl_dvfs_readl(cld, CL_DVFS_FREQ_REQ) &
		CL_DVFS_FREQ_REQ_SCALE_MASK;
	cld->last_req.scale = val >> CL_DVFS_FREQ_REQ_SCALE_SHIFT;
	cld->last_req.cap = 0;
	cld->last_req.freq = 0;
	cld->last_req.output = 0;
	cl_dvfs_writel(cld, val, CL_DVFS_FREQ_REQ);
	cl_dvfs_writel(cld, param->scale_out_ramp, CL_DVFS_SCALE_RAMP);

	/* select frequency for monitoring */
	cl_dvfs_writel(cld, CL_DVFS_MONITOR_CTRL_FREQ, CL_DVFS_MONITOR_CTRL);
	cl_dvfs_wmb(cld);
}

static int cl_dvfs_enable_clocks(struct tegra_cl_dvfs *cld)
{
	if (is_i2c(cld))
		clk_enable(cld->i2c_clk);

	clk_enable(cld->ref_clk);
	clk_enable(cld->soc_clk);
	return 0;
}

static void cl_dvfs_disable_clocks(struct tegra_cl_dvfs *cld)
{
	if (is_i2c(cld))
		clk_disable(cld->i2c_clk);

	clk_disable(cld->ref_clk);
	clk_disable(cld->soc_clk);
}

static int sync_tune_state(struct tegra_cl_dvfs *cld)
{
	u32 val = cl_dvfs_readl(cld, CL_DVFS_TUNE0);
	if (cld->tune0_low == val)
		set_tune_state(cld, TEGRA_CL_DVFS_TUNE_LOW);
	else if (cld->tune0_high == val)
		set_tune_state(cld, TEGRA_CL_DVFS_TUNE_HIGH);
	else {
		pr_err("\n %s: Failed to sync cl_dvfs tune state\n", __func__);
		return -EINVAL;
	}
	return 0;
}

/*
 * When bootloader enables cl_dvfs, then this function
 * can be used to set cl_dvfs sw sate to be in sync with
 * cl_dvfs HW sate.
 */
static int cl_dvfs_sync(struct tegra_cl_dvfs *cld)
{
	u32 val;
	int status;
	unsigned long int rate;
	unsigned long int dfll_boot_req_khz =
		cld->safe_dvfs->dfll_data.dfll_boot_khz;

	if (!dfll_boot_req_khz) {
		pr_err("%s: Failed to sync DFLL boot rate\n", __func__);
		return -EINVAL;
	}

	output_enable(cld);

	val = cl_dvfs_readl(cld, CL_DVFS_FREQ_REQ) &
		CL_DVFS_FREQ_REQ_SCALE_MASK;
	cld->last_req.scale = val >> CL_DVFS_FREQ_REQ_SCALE_SHIFT;
	cld->last_req.rate = dfll_boot_req_khz * 1000;
	cld->last_req.freq = GET_REQUEST_FREQ(cld->last_req.rate,
						cld->ref_rate);
	val = cld->last_req.freq;
	rate = GET_REQUEST_RATE(val, cld->ref_rate);
	if (find_safe_output(cld, rate, &(cld->last_req.output))) {
		pr_err("%s: Failed to find safe output for rate %lu\n",
			__func__, rate);
		return -EINVAL;
	}
	cld->last_req.cap = cld->last_req.output;
	cld->mode = TEGRA_CL_DVFS_CLOSED_LOOP;
	status = sync_tune_state(cld);
	if (status)
		return status;
	return 0;
}

static bool is_cl_dvfs_closed_loop(struct tegra_cl_dvfs *cld)
{
	u32 mode;
	mode = cl_dvfs_readl(cld, CL_DVFS_CTRL) + 1;
	if (mode == TEGRA_CL_DVFS_CLOSED_LOOP)
		return true;
	return false;
}

static int cl_dvfs_init(struct tegra_cl_dvfs *cld)
{
	int ret, gpio, flags;

	/* Enable output inerface clock */
	if (cld->p_data->pmu_if == TEGRA_CL_DVFS_PMU_I2C) {
		ret = clk_enable(cld->i2c_clk);
		if (ret) {
			pr_err("%s: Failed to enable %s\n",
			       __func__, cld->i2c_clk->name);
			return ret;
		}
		cld->i2c_rate = clk_get_rate(cld->i2c_clk);
	} else if (cld->p_data->pmu_if == TEGRA_CL_DVFS_PMU_PWM) {
		int pwm_bus = cld->p_data->u.pmu_pwm.pwm_bus;
		if (pwm_bus > TEGRA_CL_DVFS_PWM_1WIRE_DIRECT) {
			/* FIXME: PWM 2-wire support */
			pr_err("%s: not supported PWM 2-wire bus\n", __func__);
			return -ENOSYS;
		} else if (pwm_bus == TEGRA_CL_DVFS_PWM_1WIRE_BUFFER) {
			gpio = cld->p_data->u.pmu_pwm.out_gpio;
			flags = cld->p_data->u.pmu_pwm.out_enable_high ?
				GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH;
			if (gpio_request_one(gpio, flags, "cl_dvfs_pwm")) {
				pr_err("%s: Failed to request pwm gpio %d\n",
				       __func__, gpio);
				return -EPERM;
			}
		}
	} else {
		pr_err("%s: unknown PMU interface\n", __func__);
		return -EINVAL;
	}

	/* Enable module clocks, release control logic reset */
	ret = clk_enable(cld->ref_clk);
	if (ret) {
		pr_err("%s: Failed to enable %s\n",
		       __func__, cld->ref_clk->name);
		return ret;
	}
	ret = clk_enable(cld->soc_clk);
	if (ret) {
		pr_err("%s: Failed to enable %s\n",
		       __func__, cld->ref_clk->name);
		return ret;
	}
	cld->ref_rate = clk_get_rate(cld->ref_clk);
	BUG_ON(!cld->ref_rate);

	/* init tuning timer */
	hrtimer_init(&cld->tune_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cld->tune_timer.function = tune_timer_cb;
	cld->tune_delay = ktime_set(0, CL_DVFS_TUNE_HIGH_DELAY * 1000);
	if (!cld->p_data->tune_ramp_delay)
		cld->p_data->tune_ramp_delay = CL_DVFS_OUTPUT_RAMP_DELAY;
	cld->tune_ramp = ktime_set(0, cld->p_data->tune_ramp_delay * 1000);

	/* init forced output resume delay */
	if (!cld->p_data->resume_ramp_delay)
		cld->p_data->resume_ramp_delay = CL_DVFS_OUTPUT_RAMP_DELAY;

	/* init calibration timer */
	init_timer_deferrable(&cld->calibration_timer);
	cld->calibration_timer.function = calibration_timer_cb;
	cld->calibration_timer.data = (unsigned long)cld;
	cld->calibration_delay = usecs_to_jiffies(CL_DVFS_CALIBR_TIME);

	/* Init tune0 settings */
	cld->tune0_low = cld->safe_dvfs->dfll_data.tune0;
	cld->tune0_high = cld->safe_dvfs->dfll_data.tune0_high_mv;

	/* Get ready ouput voltage mapping*/
	cl_dvfs_init_maps(cld);

	/* Setup output range thresholds */
	cl_dvfs_init_output_thresholds(cld);

	/* Setup PMU interface */
	cl_dvfs_init_out_if(cld);

	if (is_cl_dvfs_closed_loop(cld)) {
		ret = cl_dvfs_sync(cld);
		if (ret)
			return ret;
	} else {
		/*
		 * Configure control registers in disabled mode
		 * and disable clocks
		 */
		cl_dvfs_init_cntrl_logic(cld);
		cl_dvfs_disable_clocks(cld);
	}

	/* Set target clock cl_dvfs data */
	tegra_dfll_set_cl_dvfs_data(cld->dfll_clk, cld);
	return 0;
}

/*
 * Re-initialize and enable target device clock in open loop mode. Called
 * directly from SoC clock resume syscore operation. Closed loop will be
 * re-entered in platform syscore ops as well.
 */
void tegra_cl_dvfs_resume(struct tegra_cl_dvfs *cld)
{
	enum tegra_cl_dvfs_ctrl_mode mode = cld->mode;
	struct dfll_rate_req req = cld->last_req;

	cl_dvfs_enable_clocks(cld);

	/* Setup PMU interface, and configure controls in disabled mode */
	cl_dvfs_init_out_if(cld);
	cl_dvfs_init_cntrl_logic(cld);

	/* Restore force output */
	cl_dvfs_writel(cld, cld->suspended_force_out, CL_DVFS_OUTPUT_FORCE);

	cl_dvfs_disable_clocks(cld);

	/* Restore last request and mode */
	cld->last_req = req;
	if (mode != TEGRA_CL_DVFS_DISABLED) {
		set_mode(cld, TEGRA_CL_DVFS_OPEN_LOOP);
		if (WARN(mode > TEGRA_CL_DVFS_OPEN_LOOP,
			 "DFLL was left locked in suspend\n"))
			return;
	}

	/* Re-enable bypass output if it was forced before suspend */
	if ((cld->p_data->u.pmu_pwm.dfll_bypass_dev) &&
	    (cld->suspended_force_out & CL_DVFS_OUTPUT_FORCE_ENABLE)) {
		if (!cld->safe_dvfs->dfll_data.is_bypass_down ||
		    !cld->safe_dvfs->dfll_data.is_bypass_down()) {
			cl_dvfs_wmb(cld);
			output_enable(cld);
			udelay(cld->p_data->resume_ramp_delay);
		}
	}
}

#ifdef CONFIG_THERMAL
/* cl_dvfs cap cooling device */
static int tegra_cl_dvfs_get_vmax_cdev_max_state(
	struct thermal_cooling_device *cdev, unsigned long *max_state)
{
	struct tegra_cl_dvfs *cld = (struct tegra_cl_dvfs *)cdev->devdata;
	*max_state = cld->therm_caps_num;
	return 0;
}

static int tegra_cl_dvfs_get_vmax_cdev_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *cur_state)
{
	struct tegra_cl_dvfs *cld = (struct tegra_cl_dvfs *)cdev->devdata;
	*cur_state = cld->therm_cap_idx;
	return 0;
}

static int tegra_cl_dvfs_set_vmax_cdev_state(
	struct thermal_cooling_device *cdev, unsigned long cur_state)
{
	unsigned long flags;
	struct tegra_cl_dvfs *cld = (struct tegra_cl_dvfs *)cdev->devdata;

	clk_lock_save(cld->dfll_clk, &flags);

	if (cld->therm_cap_idx != cur_state) {
		cld->therm_cap_idx = cur_state;
		if (cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP) {
			tegra_cl_dvfs_request_rate(cld,
				tegra_cl_dvfs_request_get(cld));
		}
	}
	clk_unlock_restore(cld->dfll_clk, &flags);
	return 0;
}

static struct thermal_cooling_device_ops tegra_cl_dvfs_vmax_cool_ops = {
	.get_max_state = tegra_cl_dvfs_get_vmax_cdev_max_state,
	.get_cur_state = tegra_cl_dvfs_get_vmax_cdev_cur_state,
	.set_cur_state = tegra_cl_dvfs_set_vmax_cdev_state,
};

/* cl_dvfs vmin cooling device */
static int tegra_cl_dvfs_get_vmin_cdev_max_state(
	struct thermal_cooling_device *cdev, unsigned long *max_state)
{
	struct tegra_cl_dvfs *cld = (struct tegra_cl_dvfs *)cdev->devdata;
	*max_state = cld->therm_floors_num;
	return 0;
}

static int tegra_cl_dvfs_get_vmin_cdev_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *cur_state)
{
	struct tegra_cl_dvfs *cld = (struct tegra_cl_dvfs *)cdev->devdata;
	*cur_state = cld->therm_floor_idx;
	return 0;
}

static int tegra_cl_dvfs_set_vmin_cdev_state(
	struct thermal_cooling_device *cdev, unsigned long cur_state)
{
	unsigned long flags;
	struct tegra_cl_dvfs *cld = (struct tegra_cl_dvfs *)cdev->devdata;

	clk_lock_save(cld->dfll_clk, &flags);

	if (cld->therm_floor_idx != cur_state) {
		cld->therm_floor_idx = cur_state;
		cl_dvfs_set_dvco_rate_min(cld, &cld->last_req);
		cl_dvfs_set_force_out_min(cld);
		if (cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP) {
			tegra_cl_dvfs_request_rate(cld,
				tegra_cl_dvfs_request_get(cld));
			/* Delay to make sure new Vmin delivery started */
			udelay(2 * GET_SAMPLE_PERIOD(cld));
		}
	}
	clk_unlock_restore(cld->dfll_clk, &flags);
	return 0;
}

static struct thermal_cooling_device_ops tegra_cl_dvfs_vmin_cool_ops = {
	.get_max_state = tegra_cl_dvfs_get_vmin_cdev_max_state,
	.get_cur_state = tegra_cl_dvfs_get_vmin_cdev_cur_state,
	.set_cur_state = tegra_cl_dvfs_set_vmin_cdev_state,
};

static void tegra_cl_dvfs_init_cdev(struct work_struct *work)
{
	char *type;
	char dt_type[THERMAL_NAME_LENGTH];
	struct device_node *dn;
	struct tegra_cl_dvfs *cld = container_of(
		work, struct tegra_cl_dvfs, init_cdev_work);

	/* just report error - initialized at WC temperature, anyway */
	if (cld->safe_dvfs->dvfs_rail->vmin_cdev) {
		type = cld->safe_dvfs->dvfs_rail->vmin_cdev->cdev_type;
		snprintf(dt_type, sizeof(dt_type), "%s_dfll", type);
		dn = cld->safe_dvfs->dvfs_rail->vmin_cdev->cdev_dn;
		cld->vmin_cdev = dn ?
			thermal_of_cooling_device_register(dn, dt_type,
				(void *)cld, &tegra_cl_dvfs_vmin_cool_ops) :
			thermal_cooling_device_register(type,
				(void *)cld, &tegra_cl_dvfs_vmin_cool_ops);

		if (IS_ERR_OR_NULL(cld->vmin_cdev)  ||
		    list_empty(&cld->vmin_cdev->thermal_instances)) {
			cld->vmin_cdev = NULL;
			pr_err("%s: tegra cooling device %s failed to register\n",
			       __func__, type);
			return;
		}
		pr_info("%s: %s cooling device registered\n", __func__, type);
	}

	if (cld->safe_dvfs->dvfs_rail->vmax_cdev) {
		type = cld->safe_dvfs->dvfs_rail->vmax_cdev->cdev_type;
		snprintf(dt_type, sizeof(dt_type), "%s_dfll", type);
		dn = cld->safe_dvfs->dvfs_rail->vmax_cdev->cdev_dn;
		cld->vmax_cdev = dn ?
			thermal_of_cooling_device_register(dn, dt_type,
				(void *)cld, &tegra_cl_dvfs_vmax_cool_ops) :
			thermal_cooling_device_register(type,
				(void *)cld, &tegra_cl_dvfs_vmax_cool_ops);

		if (IS_ERR_OR_NULL(cld->vmax_cdev) ||
		    list_empty(&cld->vmax_cdev->thermal_instances)) {
			cld->vmax_cdev = NULL;
			pr_err("%s: tegra cooling device %s failed to register\n",
			       __func__, type);
			return;
		}
		pr_info("%s: %s cooling device registered\n", __func__, type);
	}
}
#endif

#ifdef CONFIG_PM_SLEEP
/*
 * cl_dvfs controls clock/voltage to other devices, including CPU. Therefore,
 * cl_dvfs driver pm suspend callback does not stop cl-dvfs operations. It is
 * only used to enforce cold/hot volatge limit, since temperature may change in
 * suspend without waking up. The correct temperature zone after supend will
 * be updated via cl_dvfs cooling device interface during resume of temperature
 * sensor.
 */
static int tegra_cl_dvfs_suspend_cl(struct device *dev)
{
	unsigned long flags;
	struct tegra_cl_dvfs *cld = dev_get_drvdata(dev);

	clk_lock_save(cld->dfll_clk, &flags);
	if (cld->vmax_cdev)
		cld->vmax_cdev->updated = false;
	cld->therm_cap_idx = cld->therm_caps_num;
	if (cld->vmin_cdev)
		cld->vmin_cdev->updated = false;
	cld->therm_floor_idx = 0;
	cl_dvfs_set_dvco_rate_min(cld, &cld->last_req);
	cl_dvfs_set_force_out_min(cld);
	if (cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP) {
		set_cl_config(cld, &cld->last_req);
		set_request(cld, &cld->last_req);
		/* Delay to make sure new Vmin delivery started */
		udelay(2 * GET_SAMPLE_PERIOD(cld));
	}
	cld->suspended_force_out = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_FORCE);
	clk_unlock_restore(cld->dfll_clk, &flags);

	pr_debug("%s: closed loop thermal control suspended\n", __func__);

	return 0;
}

static const struct dev_pm_ops tegra_cl_dvfs_pm_ops = {
	.suspend_noirq = tegra_cl_dvfs_suspend_cl,
};
#endif

/*
 * These dfll bypass APIs provide direct access to force output register.
 * Set operation always updates force value, but applies it only in open loop,
 * or disabled mode. Get operation returns force value back if it is applied,
 * and return monitored output, otherwise. Hence, get value matches real output
 * in any mode.
 */
static int tegra_cl_dvfs_force_output(void *data, unsigned int out_sel)
{
	u32 val;
	unsigned long flags;
	struct tegra_cl_dvfs *cld = data;

	if (out_sel > OUT_MASK)
		return -EINVAL;

	clk_lock_save(cld->dfll_clk, &flags);

	val = output_force_set_val(cld, out_sel);
	if ((cld->mode < TEGRA_CL_DVFS_CLOSED_LOOP) &&
	    !(val & CL_DVFS_OUTPUT_FORCE_ENABLE)) {
		output_force_enable(cld, val);
		/* enable output only if bypass h/w is alive */
		if (!cld->safe_dvfs->dfll_data.is_bypass_down ||
		    !cld->safe_dvfs->dfll_data.is_bypass_down())
			output_enable(cld);
	}

	clk_unlock_restore(cld->dfll_clk, &flags);
	return 0;
}

static int tegra_cl_dvfs_get_output(void *data)
{
	u32 val;
	unsigned long flags;
	struct tegra_cl_dvfs *cld = data;

	clk_lock_save(cld->dfll_clk, &flags);
	val = cl_dvfs_get_output(cld);
	clk_unlock_restore(cld->dfll_clk, &flags);
	return val;
}

static void cl_dvfs_init_pwm_bypass(struct tegra_cl_dvfs *cld,
					   struct platform_device *byp_dev)
{
	struct tegra_dfll_bypass_platform_data *p_data =
		byp_dev->dev.platform_data;

	int vinit = cld->p_data->u.pmu_pwm.init_uV;
	int vmin = cld->p_data->u.pmu_pwm.min_uV;
	int vstep = cld->p_data->u.pmu_pwm.step_uV;

	/* Sync initial voltage and setup bypass callbacks */
	if ((vinit >= vmin) && vstep) {
		unsigned int vsel = DIV_ROUND_UP((vinit - vmin), vstep);
		tegra_cl_dvfs_force_output(cld, vsel);
	}

	p_data->set_bypass_sel = tegra_cl_dvfs_force_output;
	p_data->get_bypass_sel = tegra_cl_dvfs_get_output;
	p_data->dfll_data = cld;
	wmb();
}

/*
 * The Silicon Monitor (SiMon) notification provides grade information on
 * the DFLL controlled rail. The resepctive minimum voltage offset is applied
 * to thermal floors profile. SiMon offsets are negative, the higher the grade
 * the lower the floor. In addition SiMon grade may affect tuning settings: more
 * aggressive settings may be used at grades above zero.
 */
static void update_simon_tuning(struct tegra_cl_dvfs *cld, unsigned long grade)
{

	struct dvfs_dfll_data *dfll_data = &cld->safe_dvfs->dfll_data;
	u32 mask = dfll_data->tune0_simon_mask;

	if (!mask)
		return;

	/*
	 * Safe order:
	 * - switch to settings for low voltage tuning range at current grade
	 * - update both low/high voltage range settings to match new grade
	 *   notification (note that same toggle mask is applied to settings
	 *   in both low and high voltage ranges).
	 * - switch to settings for low voltage tuning range at new grade
	 * - switch to settings for high voltage range at new grade if tuning
	 *   state was high
	 */
	tune_low(cld);
	udelay(1);
	pr_debug("tune0: 0x%x\n", cl_dvfs_readl(cld, CL_DVFS_TUNE0));

	cld->tune0_low = dfll_data->tune0 ^ (grade ? mask : 0);
	cld->tune0_high = dfll_data->tune0_high_mv ^ (grade ? mask : 0);

	tune_low(cld);
	udelay(1);
	pr_debug("tune0: 0x%x\n", cl_dvfs_readl(cld, CL_DVFS_TUNE0));

	if (cld->tune_state == TEGRA_CL_DVFS_TUNE_HIGH) {
		tune_high(cld);
		pr_debug("tune0: 0x%x\n", cl_dvfs_readl(cld, CL_DVFS_TUNE0));
	}
}

static int cl_dvfs_simon_grade_notify_cb(struct notifier_block *nb,
					 unsigned long grade, void *v)
{
	unsigned long flags;
	int i, simon_offset;
	int curr_domain = (int)((long)v);
	struct tegra_cl_dvfs *cld = container_of(
		nb, struct tegra_cl_dvfs, simon_grade_nb);
	struct dvfs_rail *rail = cld->safe_dvfs->dvfs_rail;

	if (!cld->therm_floors_num || (curr_domain != rail->simon_domain))
		return NOTIFY_DONE;

	if (grade >= rail->simon_vmin_offs_num)
		grade = rail->simon_vmin_offs_num - 1;
	simon_offset = rail->simon_vmin_offsets[grade];
	BUG_ON(simon_offset > 0);

	clk_lock_save(cld->dfll_clk, &flags);

	/* Update tuning based on SiMon grade */
	update_simon_tuning(cld, grade);

	/* Convert new floors and invalidate minimum rates */
	cl_dvfs_convert_cold_output_floor(cld, simon_offset);
	for (i = 0; i < cld->therm_floors_num; i++)
		cld->dvco_rate_floors[i] = 0;

	cl_dvfs_set_dvco_rate_min(cld, &cld->last_req);
	cl_dvfs_set_force_out_min(cld);
	if (cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP) {
		tegra_cl_dvfs_request_rate(cld,
			tegra_cl_dvfs_request_get(cld));
	}

	clk_unlock_restore(cld->dfll_clk, &flags);

	pr_info("tegra_dvfs: set %s simon grade %lu\n", rail->reg_id, grade);

	return NOTIFY_OK;
};

static void tegra_cl_dvfs_register_simon_notifier(struct tegra_cl_dvfs *cld)
{
	struct dvfs_rail *rail = cld->safe_dvfs->dvfs_rail;

	/* Stay at default if no simon offsets */
	if (!rail->simon_vmin_offsets)
		return;

	cld->simon_grade_nb.notifier_call = cl_dvfs_simon_grade_notify_cb;

	if (tegra_register_simon_notifier(&cld->simon_grade_nb)) {
		pr_err("tegra_dvfs: failed to register %s simon notifier\n",
		       rail->reg_id);
		return;
	}

	pr_info("tegra_dvfs: registered %s simon notifier\n", rail->reg_id);
	return;
}

/*
 * Two mechanisms to build vdd_map dynamically:
 *
 * 1. Use regulator interface to match voltage selector to voltage level,
 * and platform data coefficients to convert selector to register values.
 * Applied when vdd supply with I2C inteface and internal voltage selection
 * register is connected.
 *
 * 2. Directly map PWM duty cycle selector to voltage level using platform data
 * coefficients. Applied when vdd supply driven by PWM data output is connected.
 */
static int build_regulator_vdd_map(struct tegra_cl_dvfs_platform_data *p_data,
	struct regulator *reg, struct voltage_reg_map **p_vdd_map)
{
	int n;
	u32 sel, i;
	struct voltage_reg_map *vdd_map;

	if (!reg)
		return -ENOSYS;

	n = regulator_count_voltages(reg);
	if (n <= 0)
		return -ENODATA;

	vdd_map = kzalloc(sizeof(*vdd_map) * n, GFP_KERNEL);
	if (!vdd_map)
		return -ENOMEM;

	for (i = 0, sel = 0; sel < n; sel++) {
		int v = regulator_list_voltage(reg, sel);
		if (v > 0) {
			vdd_map[i].reg_uV = v;
			vdd_map[i].reg_value = sel * p_data->u.pmu_i2c.sel_mul +
				p_data->u.pmu_i2c.sel_offs;
			i++;
		}
	}

	p_data->vdd_map_size = i;
	p_data->vdd_map = vdd_map;
	*p_vdd_map = vdd_map;
	return i ? 0 : -EINVAL;
}

static int build_direct_vdd_map(struct tegra_cl_dvfs_platform_data *p_data,
				struct voltage_reg_map **p_vdd_map)
{
	int i;
	struct voltage_reg_map *vdd_map =
		kzalloc(sizeof(*vdd_map) * MAX_CL_DVFS_VOLTAGES, GFP_KERNEL);

	if (!vdd_map)
		return -ENOMEM;

	for (i = 0; i < MAX_CL_DVFS_VOLTAGES; i++) {
		vdd_map[i].reg_uV = i * p_data->u.pmu_pwm.step_uV +
			p_data->u.pmu_pwm.min_uV;
		vdd_map[i].reg_value = i;
	}

	p_data->vdd_map_size = i;
	p_data->vdd_map = vdd_map;
	*p_vdd_map = vdd_map;
	return 0;
}

/* cl_dvfs comaptibility tables */
static struct tegra_cl_dvfs_soc_match_data t132_data = {
	.flags = TEGRA_CL_DVFS_DEFER_FORCE_CALIBRATE,
};

static struct tegra_cl_dvfs_soc_match_data t210_data = {
	.flags = TEGRA_CL_DVFS_HAS_IDLE_OVERRIDE |
			TEGRA_CL_DVFS_SCALE_IN_OPEN_LOOP,
};

static struct of_device_id tegra_cl_dvfs_of_match[] = {
	{ .compatible = "nvidia,tegra114-dfll", },
	{ .compatible = "nvidia,tegra124-dfll", },
	{ .compatible = "nvidia,tegra132-dfll", .data = &t132_data, },
	{ .compatible = "nvidia,tegra148-dfll", },
	{ .compatible = "nvidia,tegra210-dfll", .data = &t210_data, },
	{ },
};

/* cl_dvfs dt parsing */
#ifdef CONFIG_OF

#define OF_READ_U32_OPT(node, name, var)				       \
do {									       \
	u32 val;							       \
	if (!of_property_read_u32((node), #name, &val)) {		       \
		(var) = val;						       \
		dev_dbg(&pdev->dev, "DT: " #name " = %u\n", val);	       \
	}								       \
} while (0)

#define OF_READ_U32(node, name, var)					       \
do {									       \
	u32 val;							       \
	if (of_property_read_u32((node), #name, &val)) {		       \
		dev_err(&pdev->dev, "missing " #name " in DT data\n");	       \
		goto err_out;						       \
	}								       \
	(var) = val;							       \
	dev_dbg(&pdev->dev, "DT: " #name " = %u\n", val);		       \
} while (0)

#define OF_GET_GPIO(node, name, pin, flags)				       \
do {									       \
	(pin) = of_get_named_gpio_flags((node), #name, 0, &(flags));	       \
	if ((pin) < 0) {						       \
		dev_err(&pdev->dev, "missing " #name " in DT data\n");	       \
		goto err_out;						       \
	}								       \
	dev_dbg(&pdev->dev, "DT: " #name " = %u\n", (pin));		       \
} while (0)

#define OF_READ_BOOL(node, name, var)					       \
do {									       \
	(var) = of_property_read_bool((node), #name);			       \
	dev_dbg(&pdev->dev, "DT: " #name " = %s\n", (var) ? "true" : "false"); \
} while (0)

#define TEGRA_DFLL_OF_PWM_PERIOD_CELL 1

static int dt_parse_pwm_regulator(struct platform_device *pdev,
	struct device_node *r_dn, struct tegra_cl_dvfs_platform_data *p_data)
{
	unsigned long val;
	int min_uV, max_uV, step_uV, init_uV;
	struct of_phandle_args args;
	struct platform_device *rdev = of_find_device_by_node(r_dn);

	if (of_parse_phandle_with_args(r_dn, "pwms", "#pwm-cells", 0, &args)) {
		dev_err(&pdev->dev, "DT: failed to parse pwms property\n");
		goto err_out;
	}
	of_node_put(args.np);

	if (args.args_count <= TEGRA_DFLL_OF_PWM_PERIOD_CELL) {
		dev_err(&pdev->dev, "DT: low #pwm-cells %d\n", args.args_count);
		goto err_out;
	}

	/* convert pwm period in ns to cl_dvfs pwm clock rate in Hz */
	val = args.args[TEGRA_DFLL_OF_PWM_PERIOD_CELL];
	val = (NSEC_PER_SEC / val) * (MAX_CL_DVFS_VOLTAGES - 1);
	p_data->u.pmu_pwm.pwm_rate = val;
	dev_dbg(&pdev->dev, "DT: pwm-rate: %lu\n", val);

	/* voltage boundaries and step */
	OF_READ_U32(r_dn, regulator-min-microvolt, min_uV);
	OF_READ_U32(r_dn, regulator-max-microvolt, max_uV);
	OF_READ_U32(r_dn, regulator-init-microvolt, init_uV);

	step_uV = (max_uV - min_uV) / (MAX_CL_DVFS_VOLTAGES - 1);
	if (step_uV <= 0) {
		dev_err(&pdev->dev, "DT: invalid pwm step %d\n", step_uV);
		goto err_out;
	}
	if ((max_uV - min_uV) % (MAX_CL_DVFS_VOLTAGES - 1))
		dev_warn(&pdev->dev,
			 "DT: pwm range [%d...%d] is not aligned on %d steps\n",
			 min_uV, max_uV, MAX_CL_DVFS_VOLTAGES - 1);

	p_data->u.pmu_pwm.min_uV = min_uV;
	p_data->u.pmu_pwm.step_uV = step_uV;
	p_data->u.pmu_pwm.init_uV = init_uV;

	/*
	 * For pwm regulator access from the regulator driver, without
	 * interference with closed loop operations, cl_dvfs provides
	 * dfll bypass callbacks in device platform data
	 */
	if (rdev && rdev->dev.platform_data)
		p_data->u.pmu_pwm.dfll_bypass_dev = rdev;

	of_node_put(r_dn);
	return 0;

err_out:
	of_node_put(r_dn);
	return -EINVAL;
}

static int dt_parse_pwm_pmic_params(struct platform_device *pdev,
	struct device_node *pmic_dn, struct tegra_cl_dvfs_platform_data *p_data)
{
	int pin, i = 0;
	enum of_gpio_flags f;
	bool pwm_1wire_buffer, pwm_1wire_direct, pwm_2wire;
	struct device_node *r_dn =
		 of_parse_phandle(pmic_dn, "pwm-regulator", 0);

	/* pwm regulator device */
	if (!r_dn) {
		dev_err(&pdev->dev, "missing DT pwm regulator data\n");
		goto err_out;
	}

	if (dt_parse_pwm_regulator(pdev, r_dn, p_data)) {
		dev_err(&pdev->dev, "failed to parse DT pwm regulator\n");
		goto err_out;
	}

	/* pwm config data */
	OF_READ_BOOL(pmic_dn, pwm-1wire-buffer, pwm_1wire_buffer);
	OF_READ_BOOL(pmic_dn, pwm-1wire-direct, pwm_1wire_direct);
	OF_READ_BOOL(pmic_dn, pwm-2wire, pwm_2wire);
	if (pwm_1wire_buffer) {
		i++;
		p_data->u.pmu_pwm.pwm_bus = TEGRA_CL_DVFS_PWM_1WIRE_BUFFER;
	}
	if (pwm_1wire_direct) {
		i++;
		p_data->u.pmu_pwm.pwm_bus = TEGRA_CL_DVFS_PWM_1WIRE_DIRECT;
	}
	if (pwm_2wire) {
		i++;
		p_data->u.pmu_pwm.pwm_bus = TEGRA_CL_DVFS_PWM_2WIRE;
	}
	if (i != 1) {
		dev_err(&pdev->dev, "%s pwm_bus in DT board data\n",
			i ? "inconsistent" : "missing");
		goto err_out;
	}

	/* pwm pins data */
	OF_GET_GPIO(pmic_dn, pwm-data-gpio, pin, f);
	p_data->u.pmu_pwm.pinctrl_dev = pinctrl_get_dev_from_gpio(pin);
	if (!p_data->u.pmu_pwm.pinctrl_dev) {
		dev_err(&pdev->dev, "No tegra pincontrol driver\n");
		goto err_out;
	}
	p_data->u.pmu_pwm.pwm_pingroup = pinctrl_get_selector_from_gpio(
					p_data->u.pmu_pwm.pinctrl_dev, pin);
	if (p_data->u.pmu_pwm.pwm_pingroup < 0) {
		dev_err(&pdev->dev, "invalid gpio %d\n", pin);
		goto err_out;
	}

	if (pwm_1wire_buffer) {
		OF_GET_GPIO(pmic_dn, pwm-buffer-ctrl-gpio, pin, f);
		p_data->u.pmu_pwm.out_enable_high = !(f & OF_GPIO_ACTIVE_LOW);
		p_data->u.pmu_pwm.out_gpio = pin;
	} else if (pwm_2wire) {
		OF_GET_GPIO(pmic_dn, pwm-clk-gpio, pin, f);
		p_data->u.pmu_pwm.pwm_clk_pingroup =
			pinctrl_get_selector_from_gpio(
				p_data->u.pmu_pwm.pinctrl_dev, pin);
		if (p_data->u.pmu_pwm.pwm_pingroup < 0) {
			dev_err(&pdev->dev, "invalid gpio %d\n", pin);
			goto err_out;
		}
		OF_READ_BOOL(pmic_dn, pwm-delta-mode,
			     p_data->u.pmu_pwm.delta_mode);
	}

	of_node_put(pmic_dn);
	return 0;

err_out:
	of_node_put(pmic_dn);
	return -EINVAL;
}

static int dt_parse_i2c_pmic_params(struct platform_device *pdev,
	struct device_node *pmic_dn, struct tegra_cl_dvfs_platform_data *p_data)
{
	OF_READ_U32(pmic_dn, pmic-i2c-address, p_data->u.pmu_i2c.slave_addr);
	OF_READ_U32(pmic_dn, pmic-i2c-voltage-register, p_data->u.pmu_i2c.reg);

	OF_READ_BOOL(pmic_dn, i2c-10-bit-addresses, p_data->u.pmu_i2c.addr_10);

	OF_READ_U32(pmic_dn, sel-conversion-slope, p_data->u.pmu_i2c.sel_mul);
	OF_READ_U32_OPT(pmic_dn, sel-conversion-offset,
			p_data->u.pmu_i2c.sel_offs);
	OF_READ_U32_OPT(pmic_dn, pmic-undershoot-gb, p_data->pmu_undershoot_gb);

	OF_READ_U32(pmic_dn, i2c-fs-rate, p_data->u.pmu_i2c.fs_rate);
	OF_READ_U32_OPT(pmic_dn, i2c-hs-rate, p_data->u.pmu_i2c.hs_rate);
	if (p_data->u.pmu_i2c.hs_rate)
		OF_READ_U32(pmic_dn, i2c-hs-master-code,
			    p_data->u.pmu_i2c.hs_master_code);

	of_node_put(pmic_dn);
	return 0;

err_out:
	of_node_put(pmic_dn);
	return -EINVAL;
}

static int dt_parse_board_params(struct platform_device *pdev,
	struct device_node *b_dn, struct tegra_cl_dvfs_cfg_param *p_cfg)
{
	int i = 0;
	bool fixed_forcing, auto_forcing, no_forcing;

	OF_READ_U32(b_dn, sample-rate, p_cfg->sample_rate);
	OF_READ_U32(b_dn, cf, p_cfg->cf);
	OF_READ_U32(b_dn, ci, p_cfg->ci);
	OF_READ_U32(b_dn, cg, p_cfg->cg);
	OF_READ_U32(b_dn, droop-cut-value, p_cfg->droop_cut_value);
	OF_READ_U32(b_dn, droop-restore-ramp, p_cfg->droop_restore_ramp);
	OF_READ_U32(b_dn, scale-out-ramp, p_cfg->scale_out_ramp);

	OF_READ_BOOL(b_dn, cg-scale, p_cfg->cg_scale);

	OF_READ_BOOL(b_dn, fixed-output-forcing, fixed_forcing);
	OF_READ_BOOL(b_dn, auto-output-forcing, auto_forcing);
	OF_READ_BOOL(b_dn, no-output-forcing, no_forcing);
	if (fixed_forcing) {
		i++;
		p_cfg->force_mode = TEGRA_CL_DVFS_FORCE_FIXED;
	}
	if (auto_forcing) {
		i++;
		p_cfg->force_mode = TEGRA_CL_DVFS_FORCE_AUTO;
	}
	if (no_forcing) {
		i++;
		p_cfg->force_mode = TEGRA_CL_DVFS_FORCE_NONE;
	}
	if (i != 1) {
		dev_err(&pdev->dev, "%s force_mode in DT board data\n",
			i ? "inconsistent" : "missing");
		goto err_out;
	}

	of_node_put(b_dn);
	return 0;

err_out:
	of_node_put(b_dn);
	return -EINVAL;
}

static int cl_dvfs_dt_parse_pdata(struct platform_device *pdev,
				  struct tegra_cl_dvfs_platform_data *p_data)
{
	int ret;
	u32 flags = 0;
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *i2c_dn, *pwm_dn, *b_dn;
	const struct of_device_id *match;

	ret = of_property_read_string(dn, "out-clock-name",
				      &p_data->dfll_clk_name);
	if (ret) {
		dev_err(&pdev->dev, "missing target clock name in DT data\n");
		return ret;
	}
	dev_dbg(&pdev->dev, "DT: target clock: %s\n", p_data->dfll_clk_name);

	match = of_match_node(tegra_cl_dvfs_of_match, dn);
	if (match && match->data) {
		const struct tegra_cl_dvfs_soc_match_data *data = match->data;
		flags |= data->flags;
	}

	if (of_find_property(dn, "i2c-quiet-output-workaround", NULL))
		flags |= TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET;
	if (of_find_property(dn, "monitor-data-new-workaround", NULL))
		flags |= TEGRA_CL_DVFS_DATA_NEW_NO_USE;
	if (!of_find_property(dn, "dynamic-output-lut-workaround", NULL))
		flags |= TEGRA_CL_DVFS_DYN_OUTPUT_CFG;	/* inverse polarity */
	if (flags & TEGRA_CL_DVFS_HAS_IDLE_OVERRIDE) {
		/* Properties below are accepted only with idle override */
		if (of_find_property(dn, "defer-force-calibrate", NULL))
			flags |= TEGRA_CL_DVFS_DEFER_FORCE_CALIBRATE;
		if (of_find_property(dn, "calibrate-force-vmin", NULL))
			flags |= TEGRA_CL_DVFS_CALIBRATE_FORCE_VMIN;
	}
	p_data->flags = flags;
	dev_dbg(&pdev->dev, "DT: flags: 0x%x\n", p_data->flags);

	OF_READ_U32_OPT(dn, tune-ramp-delay, p_data->tune_ramp_delay);
	OF_READ_U32_OPT(dn, resume-ramp-delay, p_data->resume_ramp_delay);

	/* pmic integration */
	i2c_dn = of_parse_phandle(dn, "i2c-pmic-integration", 0);
	pwm_dn = of_get_child_by_name(dn, "pwm-pmic-integration");
	if (!i2c_dn == !pwm_dn) {
		of_node_put(i2c_dn);
		of_node_put(pwm_dn);
		dev_err(&pdev->dev, "%s DT pmic data\n",
			i2c_dn ? "inconsistent" : "missing");
		return -ENODATA;
	}

	ret = i2c_dn ? dt_parse_i2c_pmic_params(pdev, i2c_dn, p_data) :
			dt_parse_pwm_pmic_params(pdev, pwm_dn, p_data);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse DT pmic data\n");
		return ret;
	}
	p_data->pmu_if = i2c_dn ? TEGRA_CL_DVFS_PMU_I2C : TEGRA_CL_DVFS_PMU_PWM;

	/* board configuration parameters */
	b_dn = of_parse_phandle(dn, "board-params", 0);
	if (!b_dn) {
		dev_err(&pdev->dev, "missing DT board data\n");
		return -ENODATA;
	}

	ret = dt_parse_board_params(pdev, b_dn, p_data->cfg_param);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse DT board data\n");
		return ret;
	}

	dev_info(&pdev->dev, "DT data retrieved successfully\n");
	return 0;
}
#else
static void *tegra_cl_dvfs_dt_parse_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

static int __init tegra_cl_dvfs_probe(struct platform_device *pdev)
{
	int ret;
	struct tegra_cl_dvfs_platform_data *p_data;
	struct resource *res, *res_i2c = NULL;
	struct tegra_cl_dvfs_cfg_param *p_cfg = NULL;
	struct voltage_reg_map *p_vdd_map = NULL;
	struct tegra_cl_dvfs *cld = NULL;
	struct clk *ref_clk, *soc_clk, *i2c_clk, *safe_dvfs_clk, *dfll_clk;

	/* Get resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing register base\n");
		return -ENOMEM;
	}
	dev_dbg(&pdev->dev, "DFLL MMIO [0x%lx ... 0x%lx]\n",
		(unsigned long)res->start, (unsigned long)res->end);

	if (pdev->num_resources > 1) {
		res_i2c = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!res_i2c) {
			dev_err(&pdev->dev, "missing i2c register base\n");
			return -ENOMEM;
		}
		dev_dbg(&pdev->dev, "DFLL I2C MMIO [0x%lx ... 0x%lx]\n",
			(unsigned long)res_i2c->start,
			(unsigned long)res_i2c->end);
	}

	p_data = pdev->dev.platform_data;
	if (!p_data) {
		p_data = kzalloc(sizeof(*p_data), GFP_KERNEL);
		if (!p_data) {
			dev_err(&pdev->dev, "failed to allocate p_data\n");
			ret = -ENOMEM;
			goto err_out;
		}
		p_cfg = kzalloc(sizeof(*p_cfg), GFP_KERNEL);
		if (!p_cfg) {
			dev_err(&pdev->dev, "failed to allocate p_cfg\n");
			ret = -ENOMEM;
			goto err_out;
		}

		p_data->cfg_param = p_cfg;
		ret = cl_dvfs_dt_parse_pdata(pdev, p_data);
		if (ret) {
			dev_err(&pdev->dev, "failed to parse DT p_data\n");
			goto err_out;
		}
	} else if (!p_data->cfg_param) {
		dev_err(&pdev->dev, "missing platform data\n");
		ret = -ENODATA;
		goto err_out;
	}

	ref_clk = clk_get(&pdev->dev, "ref");
	soc_clk = clk_get(&pdev->dev, "soc");
	i2c_clk = clk_get(&pdev->dev, "i2c");
	safe_dvfs_clk = clk_get(&pdev->dev, "safe_dvfs");
	dfll_clk = clk_get(&pdev->dev, p_data->dfll_clk_name);
	if (IS_ERR(ref_clk) || IS_ERR(soc_clk) || IS_ERR(i2c_clk)) {
		dev_err(&pdev->dev, "missing control clock\n");
		ret = -ENOENT;
		goto err_out;
	}
	if (IS_ERR(safe_dvfs_clk)) {
		dev_err(&pdev->dev, "missing safe dvfs source clock\n");
		ret = PTR_ERR(safe_dvfs_clk);
		goto err_out;
	}
	if (IS_ERR(dfll_clk)) {
		dev_err(&pdev->dev, "missing target dfll clock\n");
		ret = PTR_ERR(dfll_clk);
		goto err_out;
	}
	if (!safe_dvfs_clk->dvfs || !safe_dvfs_clk->dvfs->dvfs_rail) {
		dev_err(&pdev->dev, "invalid safe dvfs source\n");
		ret = -EINVAL;
		goto err_out;
	}

	/* Build vdd_map if not specified by platform data */
	if (!p_data->vdd_map || !p_data->vdd_map_size) {
		struct regulator *reg = safe_dvfs_clk->dvfs->dvfs_rail->reg;
		if (p_data->pmu_if == TEGRA_CL_DVFS_PMU_PWM)
			ret = build_direct_vdd_map(p_data, &p_vdd_map);
		else
			ret = build_regulator_vdd_map(p_data, reg, &p_vdd_map);

		if (ret) {
			dev_err(&pdev->dev, "missing vdd_map (%d)\n", ret);
			goto err_out;
		}
	}

	/* Allocate cl_dvfs object and populate resource accessors */
	cld = kzalloc(sizeof(*cld), GFP_KERNEL);
	if (!cld) {
		dev_err(&pdev->dev, "failed to allocate cl_dvfs object\n");
		ret = -ENOMEM;
		goto err_out;
	}

	cld->cl_base = IO_ADDRESS(res->start);
	cld->cl_i2c_base = res_i2c ? IO_ADDRESS(res_i2c->start) : cld->cl_base;
	cld->p_data = p_data;
	cld->ref_clk = ref_clk;
	cld->soc_clk = soc_clk;
	cld->i2c_clk = i2c_clk;
	cld->dfll_clk = dfll_clk;
	cld->safe_dvfs = safe_dvfs_clk->dvfs;
#ifdef CONFIG_THERMAL
	INIT_WORK(&cld->init_cdev_work, tegra_cl_dvfs_init_cdev);
#endif
	/* Initialize cl_dvfs */
	ret = cl_dvfs_init(cld);
	if (ret)
		goto err_out;

	/* From now on probe would not fail */
	platform_set_drvdata(pdev, cld);

	/*
	 *  I2C interface mux is embedded into cl_dvfs h/w, so the attached
	 *  regulator can be accessed by s/w independently. PWM interface,
	 *  on the other hand, is accessible solely through cl_dvfs registers.
	 *  Hence, bypass device is supported in PWM mode only.
	 */
	if ((p_data->pmu_if == TEGRA_CL_DVFS_PMU_PWM) &&
	    p_data->u.pmu_pwm.dfll_bypass_dev) {
		clk_enable(cld->soc_clk);
		cl_dvfs_init_pwm_bypass(cld, p_data->u.pmu_pwm.dfll_bypass_dev);
	}

	/* Register SiMon notifier */
	tegra_cl_dvfs_register_simon_notifier(cld);

	/*
	 * Schedule cooling device registration as a separate work to address
	 * the following race: when cl_dvfs is probed the DFLL child clock
	 * (e.g., CPU) cannot be changed; on the other hand cooling device
	 * registration will update the entire thermal zone, and may trigger
	 * rate change of the target clock
	 */
	if (cld->safe_dvfs->dvfs_rail->vmin_cdev ||
	    cld->safe_dvfs->dvfs_rail->vmax_cdev)
		schedule_work(&cld->init_cdev_work);
	return 0;

err_out:
	if (p_data && p_vdd_map)
		p_data->vdd_map = NULL;
	kfree(p_vdd_map);
	kfree(cld);
	if (!pdev->dev.platform_data) {
		kfree(p_cfg);
		kfree(p_data);
	}
	return ret;
}

static struct platform_driver tegra_cl_dvfs_driver = {
	.driver         = {
		.name   = "tegra_cl_dvfs",
		.owner  = THIS_MODULE,
		.of_match_table = tegra_cl_dvfs_of_match,
#ifdef CONFIG_PM_SLEEP
		.pm = &tegra_cl_dvfs_pm_ops,
#endif
	},
};

int __init tegra_init_cl_dvfs(void)
{
	return platform_driver_probe(&tegra_cl_dvfs_driver,
				     tegra_cl_dvfs_probe);
}

/*
 * CL_DVFS states:
 *
 * - DISABLED: control logic mode - DISABLED, output interface disabled,
 *   dfll in reset
 * - OPEN_LOOP: control logic mode - OPEN_LOOP, output interface disabled,
 *   dfll is running "unlocked"
 * - CLOSED_LOOP: control logic mode - CLOSED_LOOP, output interface enabled,
 *   dfll is running "locked"
 */

/* Switch from any other state to DISABLED state */
void tegra_cl_dvfs_disable(struct tegra_cl_dvfs *cld)
{
	switch (cld->mode) {
	case TEGRA_CL_DVFS_CLOSED_LOOP:
		WARN(1, "DFLL is disabled directly from closed loop mode\n");
		set_ol_config(cld);
		output_disable_ol_prepare(cld);
		set_mode(cld, TEGRA_CL_DVFS_DISABLED);
		output_disable_post_ol(cld);
		invalidate_request(cld);
		cl_dvfs_disable_clocks(cld);
		return;

	case TEGRA_CL_DVFS_OPEN_LOOP:
		set_mode(cld, TEGRA_CL_DVFS_DISABLED);
		invalidate_request(cld);
		cl_dvfs_disable_clocks(cld);
		return;

	default:
		BUG_ON(cld->mode > TEGRA_CL_DVFS_CLOSED_LOOP);
		return;
	}
}

/* Switch from DISABLE state to OPEN_LOOP state */
int tegra_cl_dvfs_enable(struct tegra_cl_dvfs *cld)
{
	if (cld->mode == TEGRA_CL_DVFS_UNINITIALIZED) {
		pr_err("%s: Cannot enable DFLL in %s mode\n",
		       __func__, mode_name[cld->mode]);
		return -EPERM;
	}

	if (cld->mode != TEGRA_CL_DVFS_DISABLED)
		return 0;

	cl_dvfs_enable_clocks(cld);
	if (cld->p_data->flags & TEGRA_CL_DVFS_SCALE_IN_OPEN_LOOP)
		set_request_scale(cld, cld->last_req.scale);
	set_mode(cld, TEGRA_CL_DVFS_OPEN_LOOP);
	udelay(1);
	return 0;
}

/* Switch from OPEN_LOOP state to CLOSED_LOOP state */
int tegra_cl_dvfs_lock(struct tegra_cl_dvfs *cld)
{
	struct dfll_rate_req *req = &cld->last_req;

	switch (cld->mode) {
	case TEGRA_CL_DVFS_CLOSED_LOOP:
		return 0;

	case TEGRA_CL_DVFS_OPEN_LOOP:
		if (req->freq == 0) {
			pr_err("%s: Cannot lock DFLL at rate 0\n", __func__);
			return -EINVAL;
		}

		/*
		 * Update control logic setting with last rate request;
		 * sync output limits with current tuning and thermal state,
		 * enable output and switch to closed loop mode. Make sure
		 * forced output does not interfere with closed loop.
		 */
		set_cl_config(cld, req);
		output_enable(cld);
		set_mode(cld, TEGRA_CL_DVFS_CLOSED_LOOP);
		set_request(cld, req);
		calibration_timer_update(cld);
		return 0;

	default:
		BUG_ON(cld->mode > TEGRA_CL_DVFS_CLOSED_LOOP);
		pr_err("%s: Cannot lock DFLL in %s mode\n",
		       __func__, mode_name[cld->mode]);
		return -EPERM;
	}
}

/* Switch from CLOSED_LOOP state to OPEN_LOOP state */
int tegra_cl_dvfs_unlock(struct tegra_cl_dvfs *cld)
{
	int ret;
	bool in_range;

	switch (cld->mode) {
	case TEGRA_CL_DVFS_CLOSED_LOOP:
		set_ol_config(cld);
		in_range = is_vmin_delivered(cld);

		/* allow grace 2 sample periods to get in range */
		if (!in_range)
			udelay(2 * GET_SAMPLE_PERIOD(cld));

		ret = output_disable_ol_prepare(cld);
		set_mode(cld, TEGRA_CL_DVFS_OPEN_LOOP);
		if (!ret)
			ret = output_disable_post_ol(cld);

		if (!ret && !in_range && !is_vmin_delivered(cld)) {
			pr_err("cl_dvfs: exiting closed loop out of range\n");
			return -EINVAL;
		}
		return ret;

	case TEGRA_CL_DVFS_OPEN_LOOP:
		return 0;

	default:
		BUG_ON(cld->mode > TEGRA_CL_DVFS_CLOSED_LOOP);
		pr_err("%s: Cannot unlock DFLL in %s mode\n",
		       __func__, mode_name[cld->mode]);
		return -EPERM;
	}
}

/*
 * Convert requested rate into the control logic settings. In CLOSED_LOOP mode,
 * update new settings immediately to adjust DFLL output rate accordingly.
 * Otherwise, just save them until next switch to closed loop.
 */
int tegra_cl_dvfs_request_rate(struct tegra_cl_dvfs *cld, unsigned long rate)
{
	u32 val;
	bool dvco_min_crossed, dvco_min_updated;
	struct dfll_rate_req req;
	req.rate = rate;

	if (cld->mode == TEGRA_CL_DVFS_UNINITIALIZED) {
		pr_err("%s: Cannot set DFLL rate in %s mode\n",
		       __func__, mode_name[cld->mode]);
		return -EPERM;
	}

	/* Calibrate dfll minimum rate */
	cl_dvfs_calibrate(cld);

	/* Update minimum dvco rate if we are crossing tuning threshold */
	dvco_min_updated = cl_tune_target(cld, rate) !=
		cl_tune_target(cld, cld->last_req.rate);
	if (dvco_min_updated)
		cl_dvfs_set_dvco_rate_min(cld, &req);

	/* Determine DFLL output scale */
	req.scale = SCALE_MAX - 1;
	if (rate < cld->dvco_rate_min) {
		int scale = DIV_ROUND_CLOSEST((rate / 1000 * SCALE_MAX),
			(cld->dvco_rate_min / 1000));
		if (!scale) {
			pr_err("%s: Rate %lu is below scalable range\n",
			       __func__, rate);
			goto req_err;
		}
		req.scale = scale - 1;
		rate = cld->dvco_rate_min;
	}
	dvco_min_crossed = (rate == cld->dvco_rate_min) &&
		(cld->last_req.rate > cld->dvco_rate_min);

	/* Convert requested rate into frequency request and scale settings */
	val = GET_REQUEST_FREQ(rate, cld->ref_rate);
	if (val > FREQ_MAX) {
		pr_err("%s: Rate %lu is above dfll range\n", __func__, rate);
		goto req_err;
	}
	req.freq = val;
	rate = GET_REQUEST_RATE(val, cld->ref_rate);

	/* Find safe voltage for requested rate */
	if (find_safe_output(cld, rate, &req.output)) {
		pr_err("%s: Failed to find safe output for rate %lu\n",
		       __func__, rate);
		goto req_err;
	}
	req.cap = req.output;

	/*
	 * Save validated request, and in CLOSED_LOOP mode actually update
	 * control logic settings; use request output to set maximum voltage
	 * limit, but keep one LUT step room above safe voltage
	 */
	cld->last_req = req;

	if (cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP) {
		set_cl_config(cld, &cld->last_req);
		set_request(cld, &cld->last_req);
		if (dvco_min_crossed || dvco_min_updated)
			calibration_timer_update(cld);
	} else if ((cld->mode == TEGRA_CL_DVFS_OPEN_LOOP) &&
		   (cld->p_data->flags & TEGRA_CL_DVFS_SCALE_IN_OPEN_LOOP)) {
		set_request_scale(cld, req.scale);
	}
	return 0;

req_err:
	/* Restore dvco rate minimum */
	if (dvco_min_updated)
		cl_dvfs_set_dvco_rate_min(cld, &cld->last_req);
	return -EINVAL;

}

unsigned long tegra_cl_dvfs_request_get(struct tegra_cl_dvfs *cld)
{
	struct dfll_rate_req *req = &cld->last_req;

	/*
	 * If running below dvco minimum rate with skipper resolution:
	 * dvco min rate / 256 - return last requested rate rounded to 1kHz.
	 * If running above dvco minimum, with closed loop resolution:
	 * ref rate / 2 - return cl_dvfs target rate.
	 */
	if ((req->scale + 1) < SCALE_MAX)
		return req->rate / 1000 * 1000;

	return GET_REQUEST_RATE(req->freq, cld->ref_rate);
}

/*
 * Compare actually set (last delivered) and required Vmin. These levels may
 * be different if temperature or SiMon grade changes while cl-dvfs output
 * interface is disabled, and new required setting is not delivered to PMIC.
 * It actually may happen while cl_dvfs is disabled, or during transition
 * to/from disabled state.
 *
 * Return:
 * 0 if levels are equal,
 * +1 if last Vmin is above required,
 * -1 if last Vmin is below required.
 */
int tegra_dvfs_cmp_dfll_vmin_tfloor(struct clk *dfll_clk, int *tfloor)
{
	int ret = 0;
	unsigned long flags;
	u8 needed_out_min, last_out_min;
	struct tegra_cl_dvfs *cld;

	if (!dfll_clk)
		return -EINVAL;

	cld = tegra_dfll_get_cl_dvfs_data(dfll_clk);
	if (IS_ERR(cld))
		return PTR_ERR(cld);

	clk_lock_save(dfll_clk, &flags);
	needed_out_min = get_output_min(cld);
	last_out_min = cld->lut_min;

	if (last_out_min > needed_out_min)
		ret = 1;
	else if (last_out_min < needed_out_min)
		ret = -1;

	if (tfloor)
		*tfloor = get_mv(cld, needed_out_min);

	clk_unlock_restore(dfll_clk, &flags);
	return ret;
}

/*
 * Voltage clamping interface: set maximum and minimum voltage limits at the
 * same lowest safe (for current temperature and tuning range) level. Allows
 * temporary fix output voltage in closed loop mode. Clock rate target in this
 * state is ignored, DFLL rate is just determined by the fixed limits. Clamping
 * request is rejected if limits are already clamped, or DFLL is not in closed
 * loop mode.
 *
 * This interface is tailored for fixing voltage during SiMon grading; no other
 * s/w should use it.
 *
 * Return: fixed positive voltage if clamping request was successful, or
 * 0 if un-clamping request was successful, or -EPERM if request is rejected.
 *
 */
int tegra_dvfs_clamp_dfll_at_vmin(struct clk *dfll_clk, bool clamp)
{
	struct tegra_cl_dvfs *cld;
	unsigned long flags;
	int ret = -EPERM;

	if (!dfll_clk)
		return -EINVAL;

	cld = tegra_dfll_get_cl_dvfs_data(dfll_clk);
	if (IS_ERR(cld))
		return PTR_ERR(cld);

	clk_lock_save(dfll_clk, &flags);
	if (cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP) {
		if (clamp && !cld->v_limits.clamped) {
			u8 out_min = max(cld->lut_min, cld->force_out_min);
			set_output_limits(cld, out_min, out_min);
			cld->v_limits.clamped = true;
			ret = cld->v_limits.vmin;
		} else if (!clamp) {
			if (cld->v_limits.clamped) {
				cld->v_limits.clamped = false;
				set_cl_config(cld, &cld->last_req);
				set_request(cld, &cld->last_req);
			}
			ret = 0;
		}
	}
	clk_unlock_restore(dfll_clk, &flags);
	return ret;
}
EXPORT_SYMBOL(tegra_dvfs_clamp_dfll_at_vmin);

/*
 * Get the new Vmin setting from external rail that is connected to same CPU
 * regulator.
 */
int tegra_dvfs_set_rail_relations_dfll_vmin(struct clk *dfll_clk,
						int rail_relations_vmin)
{
	struct tegra_cl_dvfs *cld;
	unsigned long flags;
	u8 rail_relations_out_min;

	if (!dfll_clk)
		return -EINVAL;

	/* get handle to cl_dvfs from dfll_clk */
	cld = tegra_dfll_get_cl_dvfs_data(dfll_clk);
	if (IS_ERR(cld))
		return PTR_ERR(cld);

	clk_lock_save(cld->dfll_clk, &flags);

	/* convert mv to output value of cl_dvfs */
	rail_relations_out_min = find_mv_out_cap(cld, rail_relations_vmin);

	if (cld->rail_relations_out_min != rail_relations_out_min) {
		cld->rail_relations_out_min = rail_relations_out_min;
		if (cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP) {
			tegra_cl_dvfs_request_rate(cld,
				tegra_cl_dvfs_request_get(cld));
			/* Delay to make sure new Vmin delivery started */
			udelay(2 * GET_SAMPLE_PERIOD(cld));
		}
	}
	clk_unlock_restore(cld->dfll_clk, &flags);
	return 0;
}

#ifdef CONFIG_DEBUG_FS

static int lock_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP;
	return 0;
}
static int lock_set(void *data, u64 val)
{
	struct clk *c = (struct clk *)data;
	return tegra_clk_cfg_ex(c, TEGRA_CLK_DFLL_LOCK, val);
}
DEFINE_SIMPLE_ATTRIBUTE(lock_fops, lock_get, lock_set, "%llu\n");

static int flags_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = cld->p_data->flags;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(flags_fops, flags_get, NULL, "0x%llx\n");

static int monitor_get(void *data, u64 *val)
{
	u32 v, s;
	unsigned long flags;
	struct clk *c = (struct clk *)data;
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;

	clk_enable(cld->soc_clk);
	clk_lock_save(c, &flags);

	switch_monitor(cld, CL_DVFS_MONITOR_CTRL_FREQ);
	wait_data_new(cld, &v);
	filter_monitor_data(cld, &v); /* ignore error, use "some value" */

	v = GET_MONITORED_RATE(v, cld->ref_rate);
	s = cl_dvfs_readl(cld, CL_DVFS_FREQ_REQ);
	s = (s & CL_DVFS_FREQ_REQ_SCALE_MASK) >> CL_DVFS_FREQ_REQ_SCALE_SHIFT;
	*val = (u64)v * (s + 1) / 256;

	clk_unlock_restore(c, &flags);
	clk_disable(cld->soc_clk);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(monitor_fops, monitor_get, NULL, "%llu\n");

static int output_get(void *data, u64 *val)
{
	u32 v;
	unsigned long flags;
	struct clk *c = (struct clk *)data;
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;

	clk_enable(cld->soc_clk);
	clk_lock_save(c, &flags);

	v = cl_dvfs_get_output(cld);
	if (IS_ERR_VALUE(v))
		v = get_last_output(cld); /* ignore error, use "some value" */
	*val = get_mv(cld, v);

	clk_unlock_restore(c, &flags);
	clk_disable(cld->soc_clk);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(output_fops, output_get, NULL, "%llu\n");

static int vmax_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = cld->v_limits.vmax;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(vmax_fops, vmax_get, NULL, "%llu\n");

static int vmin_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = cld->v_limits.vmin;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(vmin_fops, vmin_get, NULL, "%llu\n");

static int tune_high_mv_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = cld->safe_dvfs->dfll_data.tune_high_min_millivolts;
	return 0;
}
static int tune_high_mv_set(void *data, u64 val)
{
	unsigned long flags;
	struct clk *c = (struct clk *)data;
	struct tegra_cl_dvfs *cld = c->u.dfll.cl_dvfs;

	clk_lock_save(c, &flags);

	cld->safe_dvfs->dfll_data.tune_high_min_millivolts = val;
	cl_dvfs_init_output_thresholds(cld);
	if (cld->mode == TEGRA_CL_DVFS_CLOSED_LOOP) {
		tegra_cl_dvfs_request_rate(cld,
			tegra_cl_dvfs_request_get(cld));
	}

	clk_unlock_restore(c, &flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(tune_high_mv_fops, tune_high_mv_get, tune_high_mv_set,
			"%llu\n");

static int fout_mv_get(void *data, u64 *val)
{
	u32 v;
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	v = cl_dvfs_readl(cld, CL_DVFS_OUTPUT_FORCE) & OUT_MASK;
	*val = cld->p_data->vdd_map[v].reg_uV / 1000;
	return 0;
}
static int fout_mv_set(void *data, u64 val)
{
	u32 v;
	unsigned long flags;
	struct clk *c = (struct clk *)data;
	struct tegra_cl_dvfs *cld = c->u.dfll.cl_dvfs;

	clk_enable(cld->soc_clk);
	clk_lock_save(c, &flags);

	if (val) {
		u8 out_v = is_i2c(cld) ? find_mv_out_cap(cld, (int)val) :
			find_vdd_map_entry(cld, (int)val, false)->reg_value;
		v = output_force_set_val(cld, out_v);
		if (!(v & CL_DVFS_OUTPUT_FORCE_ENABLE))
			output_force_enable(cld, v);
	} else {
		output_force_disable(cld);
	}

	clk_unlock_restore(c, &flags);
	clk_disable(cld->soc_clk);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fout_mv_fops, fout_mv_get, fout_mv_set, "%llu\n");

static int fmin_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = cld->dvco_rate_min;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(dvco_rate_min_fops, fmin_get, NULL, "%llu\n");

static int calibr_delay_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = jiffies_to_msecs(cld->calibration_delay);
	return 0;
}
static int calibr_delay_set(void *data, u64 val)
{
	unsigned long flags;
	struct clk *c = (struct clk *)data;
	struct tegra_cl_dvfs *cld = c->u.dfll.cl_dvfs;

	clk_lock_save(c, &flags);
	cld->calibration_delay = msecs_to_jiffies(val);
	clk_unlock_restore(c, &flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(calibr_delay_fops, calibr_delay_get, calibr_delay_set,
			"%llu\n");

static int undershoot_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = cld->p_data->pmu_undershoot_gb;
	return 0;
}
static int undershoot_set(void *data, u64 val)
{
	unsigned long flags;
	struct clk *c = (struct clk *)data;
	struct tegra_cl_dvfs *cld = c->u.dfll.cl_dvfs;

	clk_lock_save(c, &flags);
	cld->p_data->pmu_undershoot_gb = val;
	cl_dvfs_set_force_out_min(cld);
	clk_unlock_restore(c, &flags);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(undershoot_fops, undershoot_get, undershoot_set,
			"%llu\n");

static int clamp_get(void *data, u64 *val)
{
	struct tegra_cl_dvfs *cld = ((struct clk *)data)->u.dfll.cl_dvfs;
	*val = cld->v_limits.clamped ? cld->v_limits.vmin : 0;
	return 0;
}
static int clamp_set(void *data, u64 val)
{
	struct clk *dfll_clk = data;
	int ret = tegra_dvfs_clamp_dfll_at_vmin(dfll_clk, val);
	return ret < 0 ? ret : 0;
}
DEFINE_SIMPLE_ATTRIBUTE(clamp_fops, clamp_get, clamp_set, "%llu\n");

static int cl_profiles_show(struct seq_file *s, void *data)
{
	u8 v;
	int i, *trips;
	unsigned long r;
	struct clk *c = s->private;
	struct tegra_cl_dvfs *cld = c->u.dfll.cl_dvfs;

	seq_printf(s, "THERM CAPS:%s\n", cld->therm_caps_num ? "" : " NONE");
	for (i = 0; i < cld->therm_caps_num; i++) {
		v = cld->thermal_out_caps[i];
		trips = cld->safe_dvfs->dvfs_rail->vmax_cdev->trip_temperatures;
		seq_printf(s, "%3dC.. %5dmV\n", trips[i], get_mv(cld, v));
	}

	if (cld->tune_high_target_rate_min == ULONG_MAX) {
		seq_puts(s, "TUNE HIGH: NONE\n");
	} else {
		seq_puts(s, "TUNE HIGH:\n");
		seq_printf(s, "min    %5dmV%9lukHz\n",
			   get_mv(cld, cld->tune_high_out_min),
			   cld->tune_high_dvco_rate_min / 1000);
		seq_printf(s, "%-14s%9lukHz\n", "rate threshold",
			   cld->tune_high_target_rate_min / 1000);
	}

	seq_printf(s, "THERM FLOORS:%s\n", cld->therm_floors_num ? "" : " NONE");
	for (i = 0; i < cld->therm_floors_num; i++) {
		v = cld->thermal_out_floors[i];
		r = cld->dvco_rate_floors[i];
		trips = cld->safe_dvfs->dvfs_rail->vmin_cdev->trip_temperatures;
		seq_printf(s, " ..%3dC%5dmV%9lukHz%s\n",
			   trips[i], get_mv(cld, v),
			   (r ? : get_dvco_rate_below(cld, v)) / 1000,
			   r ? " (calibrated)"  : "");
	}
	r = cld->dvco_rate_floors[i];
	seq_printf(s, "  vmin:%5dmV%9lukHz%s\n", cld->out_map[0]->reg_uV / 1000,
		   (r ? : cld->safe_dvfs->dfll_data.out_rate_min) / 1000,
		   r ? " (calibrated)"  : "");

	return 0;
}

static int cl_profiles_open(struct inode *inode, struct file *file)
{
	return single_open(file, cl_profiles_show, inode->i_private);
}

static const struct file_operations cl_profiles_fops = {
	.open		= cl_profiles_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int cl_register_show(struct seq_file *s, void *data)
{
	u32 offs;
	struct clk *c = s->private;
	struct tegra_cl_dvfs *cld = c->u.dfll.cl_dvfs;

	clk_enable(cld->soc_clk);

	seq_printf(s, "CONTROL REGISTERS:\n");
	for (offs = 0; offs <= CL_DVFS_MONITOR_DATA; offs += 4)
		seq_printf(s, "[0x%02x] = 0x%08x\n",
			   offs, cl_dvfs_readl(cld, offs));

	seq_printf(s, "\nI2C and INTR REGISTERS:\n");
	for (offs = CL_DVFS_I2C_CFG; offs <= CL_DVFS_I2C_STS; offs += 4)
		seq_printf(s, "[0x%02x] = 0x%08x\n",
			   offs, cl_dvfs_readl(cld, offs));

	offs = CL_DVFS_INTR_STS;
	seq_printf(s, "[0x%02x] = 0x%08x\n", offs, cl_dvfs_readl(cld, offs));
	offs = CL_DVFS_INTR_EN;
	seq_printf(s, "[0x%02x] = 0x%08x\n", offs, cl_dvfs_readl(cld, offs));

	if (cld->p_data->flags & TEGRA_CL_DVFS_HAS_IDLE_OVERRIDE) {
		seq_printf(s, "\nOVERRIDE REGISTERS:\n");
		offs = CL_DVFS_CC4_HVC;
		seq_printf(s, "[0x%02x] = 0x%08x\n", offs,
			   cl_dvfs_readl(cld, offs));
	}

	seq_printf(s, "\nLUT:\n");
	for (offs = CL_DVFS_OUTPUT_LUT;
	     offs < CL_DVFS_OUTPUT_LUT + 4 * MAX_CL_DVFS_VOLTAGES;
	     offs += 4)
		seq_printf(s, "[0x%02x] = 0x%08x\n",
			   offs, cl_dvfs_readl(cld, offs));

	clk_disable(cld->soc_clk);
	return 0;
}

static int cl_register_open(struct inode *inode, struct file *file)
{
	return single_open(file, cl_register_show, inode->i_private);
}

static ssize_t cl_register_write(struct file *file,
	const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[80];
	u32 offs;
	u32 val;
	struct clk *c = file->f_path.dentry->d_inode->i_private;
	struct tegra_cl_dvfs *cld = c->u.dfll.cl_dvfs;

	if (sizeof(buf) <= count)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	/* terminate buffer and trim - white spaces may be appended
	 *  at the end when invoked from shell command line */
	buf[count] = '\0';
	strim(buf);

	if (sscanf(buf, "[0x%x] = 0x%x", &offs, &val) != 2)
		return -1;

	if (offs >= CL_DVFS_APERTURE)
		return -1;

	clk_enable(cld->soc_clk);
	cl_dvfs_writel(cld, val, offs & (~0x3));
	clk_disable(cld->soc_clk);
	return count;
}

static const struct file_operations cl_register_fops = {
	.open		= cl_register_open,
	.read		= seq_read,
	.write		= cl_register_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int __init tegra_cl_dvfs_debug_init(struct clk *dfll_clk)
{
	struct dentry *cl_dvfs_dentry;

	if (!dfll_clk || !dfll_clk->dent || (dfll_clk->state == UNINITIALIZED))
		return 0;

	if (!debugfs_create_file("lock", S_IRUGO | S_IWUSR,
		dfll_clk->dent, dfll_clk, &lock_fops))
		goto err_out;

	cl_dvfs_dentry = debugfs_create_dir("cl_dvfs", dfll_clk->dent);
	if (!cl_dvfs_dentry)
		goto err_out;

	if (!debugfs_create_file("flags", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &flags_fops))
		goto err_out;

	if (!debugfs_create_file("monitor", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &monitor_fops))
		goto err_out;

	if (!debugfs_create_file("output_mv", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &output_fops))
		goto err_out;

	if (!debugfs_create_file("vmax_mv", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &vmax_fops))
		goto err_out;

	if (!debugfs_create_file("vmin_mv", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &vmin_fops))
		goto err_out;

	if (!debugfs_create_file("tune_high_mv", S_IRUGO | S_IWUSR,
		cl_dvfs_dentry, dfll_clk, &tune_high_mv_fops))
		goto err_out;

	if (!debugfs_create_file("force_out_mv", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &fout_mv_fops))
		goto err_out;

	if (!debugfs_create_file("dvco_min", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &dvco_rate_min_fops))
		goto err_out;

	if (!debugfs_create_file("calibr_delay", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &calibr_delay_fops))
		goto err_out;

	if (!debugfs_create_file("pmu_undershoot_gb", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &undershoot_fops))
		goto err_out;

	if (!debugfs_create_file("clamp_at_min", S_IRUGO | S_IWUSR,
		cl_dvfs_dentry, dfll_clk, &clamp_fops))
		goto err_out;

	if (!debugfs_create_file("profiles", S_IRUGO,
		cl_dvfs_dentry, dfll_clk, &cl_profiles_fops))
		goto err_out;

	if (!debugfs_create_file("registers", S_IRUGO | S_IWUSR,
		cl_dvfs_dentry, dfll_clk, &cl_register_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(dfll_clk->dent);
	return -ENOMEM;
}
#endif
