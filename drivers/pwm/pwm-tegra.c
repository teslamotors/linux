/*
 * drivers/pwm/pwm-tegra.c
 *
 * Tegra pulse-width-modulation controller driver
 *
 * Copyright (c) 2010-2016, NVIDIA CORPORATION. All rights reserved.
 * Based on arch/arm/plat-mxc/pwm.c by Sascha Hauer <s.hauer@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/slab.h>
#include <linux/reset.h>

#define PWM_ENABLE	(1 << 31)
#define PWM_DUTY_WIDTH	8
#define PWM_DUTY_SHIFT	16
#define PWM_SCALE_WIDTH	13
#define PWM_SCALE_SHIFT	0

struct tegra_pwm_chipdata {
	int num_pwm;
};

struct tegra_pwm_chip {
	struct pwm_chip		chip;
	struct device		*dev;

	struct clk		*clk;

	void __iomem		*mmio_base;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*suspend_state;
	struct pinctrl_state	*resume_state;
	bool			pretty_good_algo;
	int			num_user;
	int			clk_init_rate;
	int			clk_curr_rate;
	const struct tegra_pwm_chipdata	*chip_data;
	struct reset_control	*rstc;
};

static inline struct tegra_pwm_chip *to_tegra_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct tegra_pwm_chip, chip);
}

static inline u32 pwm_readl(struct tegra_pwm_chip *chip, unsigned int num)
{
	return readl(chip->mmio_base + (num << 4));
}

static inline void pwm_writel(struct tegra_pwm_chip *chip, unsigned int num,
			     unsigned long val)
{
	writel(val, chip->mmio_base + (num << 4));
}

static int tegra_get_optimal_rate(struct tegra_pwm_chip *pc,
		int duty_ns, int period_ns)
{
	unsigned int due_dp, dn;
	unsigned int due_dm;
	unsigned long p_rate, in_rate, rate, hz;
	int ret;

	p_rate = clk_get_rate(clk_get_parent(pc->clk));

	/* Round rate/128 to nearest integer */
	rate = DIV_ROUND_CLOSEST(p_rate, 128);

	/* Round (10^9 ns)/period_ns to nearest integer */
	hz = DIV_ROUND_CLOSEST(1000000000ul, period_ns);

	/* Round rate/(128*hz) to nearest integer; we assume hz >= 49Hz */
	due_dp = DIV_ROUND_CLOSEST(rate, hz);

	/* Round due_dp/257 up to next largest integer */
	dn = DIV_ROUND_UP(due_dp, 257);

	/* Round due_dp/dn to nearest integer */
	due_dm = DIV_ROUND_CLOSEST(due_dp, dn);

	/*
	 * Make sure that the freq division will fit in the register's
	 * frequency divider field.
	 */
	if ((dn - 1) >> PWM_SCALE_WIDTH)
		return -EINVAL;

	in_rate = (2 * p_rate) / (due_dm - 1);
	ret = clk_set_rate(pc->clk, in_rate);
	if (ret < 0) {
		dev_err(pc->dev, "Not able to set proper rate: %d\n", ret);
		return ret;
	}
	pc->clk_curr_rate = clk_get_rate(pc->clk);
	return dn - 1;
}

static int tegra_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			    int duty_ns, int period_ns)
{
	struct tegra_pwm_chip *pc = to_tegra_pwm_chip(chip);
	unsigned long long c;
	unsigned long hz;
	int rate;
	u32 val = 0;
	int err;

	/*
	 * Convert from duty_ns / period_ns to a fixed number of duty ticks
	 * per (1 << PWM_DUTY_WIDTH) cycles and make sure to round to the
	 * nearest integer during division.
	 */
	c = duty_ns;
	c *= (1 << PWM_DUTY_WIDTH);
	c += period_ns / 2;
	do_div(c, period_ns);

	val = (u32)c << PWM_DUTY_SHIFT;

	if (pc->pretty_good_algo) {
		rate = tegra_get_optimal_rate(pc, duty_ns, period_ns);
		if (rate >= 0)
			goto timing_done;
	} else {
		if (pc->clk_init_rate != pc->clk_curr_rate) {
			err = clk_set_rate(pc->clk, pc->clk_init_rate);
			if (err < 0) {
				dev_err(pc->dev,
					"Not able to set proper rate: %d\n",
					err);
				return err;
			}
			pc->clk_curr_rate = pc->clk_init_rate;
		}
	}

	/*
	 * Compute the prescaler value for which (1 << PWM_DUTY_WIDTH)
	 * cycles at the PWM clock rate will take period_ns nanoseconds.
	 */
	rate = clk_get_rate(pc->clk) >> PWM_DUTY_WIDTH;
	hz = 1000000000ul / period_ns;

	rate = (rate + (hz / 2)) / hz;

	/*
	 * Since the actual PWM divider is the register's frequency divider
	 * field minus 1, we need to decrement to get the correct value to
	 * write to the register.
	 */
	if (rate > 0)
		rate--;

	/*
	 * Make sure that the rate will fit in the register's frequency
	 * divider field.
	 */
	if (rate >> PWM_SCALE_WIDTH)
		return -EINVAL;

timing_done:
	val |= rate << PWM_SCALE_SHIFT;

	/*
	 * If the PWM channel is disabled, make sure to turn on the clock
	 * before writing the register. Otherwise, keep it enabled.
	 */
	if (!test_bit(PWMF_ENABLED, &pwm->flags)) {
		err = clk_prepare_enable(pc->clk);
		if (err < 0)
			return err;
	} else
		val |= PWM_ENABLE;

	pwm_writel(pc, pwm->hwpwm, val);
	/*
	 * If the PWM is not enabled, turn the clock off again to save power.
	 */
	if (!test_bit(PWMF_ENABLED, &pwm->flags))
		clk_disable_unprepare(pc->clk);

	return 0;
}

static int tegra_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct tegra_pwm_chip *pc = to_tegra_pwm_chip(chip);
	int rc = 0;
	u32 val;

	rc = clk_prepare_enable(pc->clk);
	if (rc < 0)
		return rc;

	val = pwm_readl(pc, pwm->hwpwm);
	val |= PWM_ENABLE;
	pwm_writel(pc, pwm->hwpwm, val);

	return 0;
}

static void tegra_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct tegra_pwm_chip *pc = to_tegra_pwm_chip(chip);
	u32 val;

	val = pwm_readl(pc, pwm->hwpwm);
	val &= ~PWM_ENABLE;
	pwm_writel(pc, pwm->hwpwm, val);

	clk_disable_unprepare(pc->clk);
}

static int tegra_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct tegra_pwm_chip *pc = to_tegra_pwm_chip(chip);

	pc->num_user++;
	if ((pc->num_user > 1) && pc->pretty_good_algo) {
		dev_err(pc->dev, "Multiple user is not possible\n");
		WARN_ON(1);
		pc->num_user--;
		return -EBUSY;
	}
	return 0;
}

static void tegra_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct tegra_pwm_chip *pc = to_tegra_pwm_chip(chip);

	if (pc->num_user)
		pc->num_user--;
};

static const struct pwm_ops tegra_pwm_ops = {
	.request = tegra_pwm_request,
	.free = tegra_pwm_free,
	.config = tegra_pwm_config,
	.enable = tegra_pwm_enable,
	.disable = tegra_pwm_disable,
	.owner = THIS_MODULE,
};

static const struct tegra_pwm_chipdata t124_pwm_cdata = {
	.num_pwm = 4,
};

static const struct tegra_pwm_chipdata t186_pwm_cdata = {
	.num_pwm = 1,
};

static const struct of_device_id tegra_pwm_of_match[] = {
	{ .compatible = "nvidia,tegra20-pwm", .data = &t124_pwm_cdata, },
	{ .compatible = "nvidia,tegra30-pwm", .data = &t124_pwm_cdata, },
	{ .compatible = "nvidia,tegra114-pwm", .data = &t124_pwm_cdata, },
	{ .compatible = "nvidia,tegra124-pwm", .data = &t124_pwm_cdata, },
	{ .compatible = "nvidia,tegra186-pwm", .data = &t186_pwm_cdata, },
	{ }
};
MODULE_DEVICE_TABLE(of, tegra_pwm_of_match);

static int tegra_pwm_probe(struct platform_device *pdev)
{
	struct tegra_pwm_chip *pwm;
	const struct tegra_pwm_chipdata *cdata = &t124_pwm_cdata;
	struct resource *r;
	int ret;

	if (pdev->dev.of_node) {
		cdata = of_get_match_device_data(
				of_match_ptr(tegra_pwm_of_match), &pdev->dev);
		if (!cdata) {
			dev_err(&pdev->dev, "Chip data not found\n");
			return -ENODEV;
		}
	}

	pwm = devm_kzalloc(&pdev->dev, sizeof(*pwm), GFP_KERNEL);
	if (!pwm)
		return -ENOMEM;

	pwm->dev = &pdev->dev;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pwm->mmio_base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(pwm->mmio_base)) {
		dev_err(&pdev->dev, "PWM io mapping failed\n");
		return PTR_ERR(pwm->mmio_base);
	}

	platform_set_drvdata(pdev, pwm);

	if (pdev->dev.of_node)
		pwm->pretty_good_algo = of_property_read_bool(pdev->dev.of_node,
						"pwm,use-pretty-good-alogorithm");

	pwm->clk = devm_clk_get(&pdev->dev, "pwm");
	if (IS_ERR(pwm->clk)) {
		dev_err(&pdev->dev, "PWM clock get failed\n");
		return PTR_ERR(pwm->clk);
	}

	pwm->rstc = devm_reset_control_get(&pdev->dev, "pwm");
	if (IS_ERR(pwm->rstc)) {
		ret = PTR_ERR(pwm->rstc);
		dev_err(&pdev->dev, "Reset control is not found: %d\n", ret);
		return ret;
	}
	reset_control_reset(pwm->rstc);

	pwm->chip_data = cdata;
	pwm->clk_init_rate = clk_get_rate(pwm->clk);
	pwm->clk_curr_rate = pwm->clk_init_rate;
	pwm->chip.dev = &pdev->dev;
	pwm->chip.ops = &tegra_pwm_ops;
	pwm->chip.base = -1;
	pwm->chip.npwm = pwm->chip_data->num_pwm;

	ret = pwmchip_add(&pwm->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		return ret;
	}

	pwm->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pwm->pinctrl))
		pwm->pinctrl = NULL;

	if (pwm->pinctrl) {
		pwm->suspend_state = pinctrl_lookup_state(pwm->pinctrl,
						"suspend");
		if (IS_ERR(pwm->suspend_state))
			pwm->suspend_state = NULL;

		pwm->resume_state = pinctrl_lookup_state(pwm->pinctrl,
						"resume");
		if (IS_ERR(pwm->resume_state))
			pwm->resume_state = NULL;

		if (pwm->suspend_state && pwm->resume_state)
			dev_info(&pdev->dev,
				"Pinconfig found for suspend/resume\n");
	}
	return 0;
}

static int tegra_pwm_remove(struct platform_device *pdev)
{
	struct tegra_pwm_chip *pc = platform_get_drvdata(pdev);
	int i;

	if (WARN_ON(!pc))
		return -ENODEV;

	for (i = 0; i < pc->chip_data->num_pwm; i++) {
		struct pwm_device *pwm = &pc->chip.pwms[i];

		if (!test_bit(PWMF_ENABLED, &pwm->flags))
			if (clk_prepare_enable(pc->clk) < 0)
				continue;

		pwm_writel(pc, i, 0);

		clk_disable_unprepare(pc->clk);
	}

	return pwmchip_remove(&pc->chip);
}

#ifdef CONFIG_PM_SLEEP
static int tegra_pwm_suspend(struct device *dev)
{
	struct tegra_pwm_chip *pc = dev_get_drvdata(dev);
	int ret;

	if (pc->pinctrl && pc->suspend_state) {
		ret = pinctrl_select_state(pc->pinctrl, pc->suspend_state);
		if (ret < 0) {
			dev_err(dev, "setting pin suspend state failed :%d\n",
				ret);
			return ret;
		}
	}
	return 0;
}

static int tegra_pwm_resume(struct device *dev)
{
	struct tegra_pwm_chip *pc = dev_get_drvdata(dev);
	int ret;

	if (pc->pinctrl && pc->resume_state) {
		ret = pinctrl_select_state(pc->pinctrl, pc->resume_state);
		if (ret < 0) {
			dev_err(dev, "setting pin resume state failed :%d\n",
				ret);
			return ret;
		}
	}
	return 0;
}
#endif

static const struct dev_pm_ops tegra_pwm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tegra_pwm_suspend, tegra_pwm_resume)
};

static struct platform_driver tegra_pwm_driver = {
	.driver = {
		.name = "tegra-pwm",
		.owner = THIS_MODULE,
		.of_match_table = tegra_pwm_of_match,
		.pm = &tegra_pwm_pm_ops,
	},
	.probe = tegra_pwm_probe,
	.remove = tegra_pwm_remove,
};

static int __init tegra_pwm_init_driver(void)
{
	return platform_driver_register(&tegra_pwm_driver);
}

static void __exit tegra_pwm_exit_driver(void)
{
	platform_driver_unregister(&tegra_pwm_driver);
}

subsys_initcall(tegra_pwm_init_driver);
module_exit(tegra_pwm_exit_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_ALIAS("platform:tegra-pwm");
