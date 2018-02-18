/*
 * tegra_asoc_utils_alt.c - MCLK and DAP Utility driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include <mach/clk.h>
#include <linux/reset.h>
#include <sound/soc.h>

#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-tegra.h>

#include "tegra_asoc_utils_alt.h"

#ifdef CONFIG_SWITCH
static bool is_switch_registered;
#endif

#if !defined(CONFIG_ARCH_TEGRA_21x_SOC) && !defined(CONFIG_ARCH_TEGRA_18x_SOC)
static atomic_t dap_ref_count[5];
static const char *tegra_dap_group_names[4][4] = {
	{"dap1_fs_pn0", "dap1_din_pn1", "dap1_dout_pn2", "dap1_sclk_pn3"},
	{"dap2_fs_pa2", "dap2_din_pa4", "dap2_dout_pa5", "dap2_sclk_pa3"},
	{"dap3_fs_pp0", "dap3_din_pp1", "dap3_dout_pp2", "dap3_sclk_pp3"},
	{"dap4_fs_pp4", "dap4_din_pp5", "dap4_dout_pp6", "dap4_sclk_pp7"},
};
#define tegra_pinmux_driver "nvidia,tegra124-pinmux"

static inline void tristate_dap(int dap_nr, int tristate)
{
	static struct pinctrl_dev *pinctrl_dev = NULL;
	unsigned long config;
	int i;
	int ret;

	if (!pinctrl_dev)
		pinctrl_dev = pinctrl_get_dev_from_of_compatible(
					tegra_pinmux_driver);
	if (!pinctrl_dev) {
		pr_err("%s(): Pincontrol for Tegra not found\n", __func__);
		return;
	}

	config = TEGRA_PINCONF_PACK(TEGRA_PINCONF_PARAM_TRISTATE, tristate);
	for (i = 0; i < 4; ++i) {
		ret = pinctrl_set_config_for_group_name(pinctrl_dev,
				tegra_dap_group_names[dap_nr][i], config);
		if (ret < 0)
			pr_err("%s(): Pinconfig for pin %s failed: %d\n",
				__func__, tegra_dap_group_names[dap_nr][i],
				ret);
	}
}

#define TRISTATE_DAP_PORT(n) \
static void tristate_dap_##n(bool tristate) \
{ \
	if (tristate) { \
		if (atomic_dec_return(&dap_ref_count[n-1]) == 0) \
			tristate_dap(n - 1, TEGRA_PIN_ENABLE);	\
	} else { \
		if (atomic_inc_return(&dap_ref_count[n-1]) == 1) \
			tristate_dap(n - 1, TEGRA_PIN_DISABLE);	\
	} \
}

TRISTATE_DAP_PORT(1)
TRISTATE_DAP_PORT(2)
/*I2S2 and I2S3 for other chips do not map to DAP3 and DAP4 (also
these pinmux dont exist for other chips), they map to some
other pinmux*/
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	TRISTATE_DAP_PORT(3)
	TRISTATE_DAP_PORT(4)
#endif

int tegra_alt_asoc_utils_tristate_dap(int id, bool tristate)
{
	switch (id) {
	case 0:
		tristate_dap_1(tristate);
		break;
	case 1:
		tristate_dap_2(tristate);
		break;
/*I2S2 and I2S3 for other chips do not map to DAP3 and DAP4 (also
these pinmux dont exist for other chips), they map to some
other pinmux*/
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	case 2:
		tristate_dap_3(tristate);
		break;
	case 3:
		tristate_dap_4(tristate);
		break;
#endif
	default:
		pr_warn("Invalid DAP port\n");
		break;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_tristate_dap);
#endif

struct clk *tegra_alt_asoc_utils_get_clk(struct device *dev,
					 bool dev_id,
					 const char *clk_name)
{
	struct clk *clk;
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	if (dev_id)
		clk = clk_get_sys(clk_name, NULL);
	else
		clk = clk_get_sys(NULL, clk_name);
#else
	clk = devm_clk_get(dev, clk_name);
#endif
	return clk;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_get_clk);

void tegra_alt_asoc_utils_clk_put(struct device *dev, struct clk *clk)
{
#if defined(CONFIG_ARCH_TEGRA_21x_SOC)
	clk_put(clk);
#else
	devm_clk_put(dev, clk);
#endif
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_clk_put);

int tegra_alt_asoc_utils_set_rate(struct tegra_asoc_audio_clock_info *data,
				int srate,
				int mclk,
				int clk_out_rate)
{
	int new_baseclock;
	int ahub_rate = 0;
	bool clk_change;
	int err;

	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
		if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
			new_baseclock = 56448000;
		else if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA30)
			new_baseclock = 564480000;
		else if ((data->soc > TEGRA_ASOC_UTILS_SOC_TEGRA30) &&
			 (data->soc < TEGRA_ASOC_UTILS_SOC_TEGRA186))
			new_baseclock = 282240000;
		else {
			new_baseclock = data->clk_rates[PLLA_x11025_RATE];
			mclk = data->clk_rates[PLLA_OUT0_x11025_RATE];
			ahub_rate = data->clk_rates[AHUB_x11025_RATE];

			clk_out_rate = srate << 8;
			data->clk_out_rate = clk_out_rate;
		}
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
	case 192000:
		if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
			new_baseclock = 73728000;
		else if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA30)
			new_baseclock = 552960000;
		else if ((data->soc > TEGRA_ASOC_UTILS_SOC_TEGRA30) &&
			 (data->soc < TEGRA_ASOC_UTILS_SOC_TEGRA186))
			new_baseclock = 368640000;
		else {
			new_baseclock = data->clk_rates[PLLA_x8000_RATE];
			mclk = data->clk_rates[PLLA_OUT0_x8000_RATE];
			ahub_rate = data->clk_rates[AHUB_x8000_RATE];

			/* aud_mclk should be 256 times the playback rate*/
			clk_out_rate = srate << 8;
			data->clk_out_rate = clk_out_rate;
		}
		break;
	default:
		return -EINVAL;
	}

	clk_change = ((new_baseclock != data->set_baseclock) ||
			(mclk != data->set_mclk) ||
			(clk_out_rate != data->set_clk_out_rate));

	if (!clk_change)
		return 0;

	/* Don't change rate if already one dai-link is using it */
	if (data->lock_count)
		return -EINVAL;

	data->set_baseclock = 0;
	data->set_mclk = 0;

	err = clk_set_rate(data->clk_pll_a, new_baseclock);
	if (err) {
		dev_err(data->dev, "Can't set pll_a rate: %d\n", err);
		return err;
	}

	err = clk_set_rate(data->clk_pll_a_out0, mclk);
	if (err) {
		dev_err(data->dev, "Can't set clk_pll_a_out0 rate: %d\n", err);
		return err;
	}

	if (data->soc > TEGRA_ASOC_UTILS_SOC_TEGRA210) {
		err = clk_set_rate(data->clk_ahub, ahub_rate);
		if (err) {
			dev_err(data->dev, "Can't set clk_cdev1 rate: %d\n",
				err);
			return err;
		}
	}

	err = clk_set_rate(data->clk_cdev1, clk_out_rate);
	if (err) {
		dev_err(data->dev, "Can't set clk_cdev1 rate: %d\n", err);
		return err;
	}

	data->set_baseclock = new_baseclock;
	data->set_mclk = mclk;
	data->set_clk_out_rate = clk_out_rate;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_set_rate);

void tegra_alt_asoc_utils_lock_clk_rate(struct tegra_asoc_audio_clock_info *data,
				    int lock)
{
	if (lock)
		data->lock_count++;
	else if (data->lock_count)
		data->lock_count--;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_lock_clk_rate);

int tegra_alt_asoc_utils_clk_enable(struct tegra_asoc_audio_clock_info *data)
{
	int err;

#if defined(CONFIG_COMMON_CLK)
	reset_control_reset(data->clk_cdev1_rst);
#endif
	err = clk_prepare_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", err);
		return err;
	}
	data->clk_cdev1_state = 1;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_clk_enable);

int tegra_alt_asoc_utils_clk_disable(struct tegra_asoc_audio_clock_info *data)
{
	clk_disable_unprepare(data->clk_cdev1);
	data->clk_cdev1_state = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_clk_disable);

int tegra_alt_asoc_utils_init(struct tegra_asoc_audio_clock_info *data,
			  struct device *dev, struct snd_soc_card *card)
{
	int ret;

	data->dev = dev;
	data->card = card;

	if (of_machine_is_compatible("nvidia,tegra20"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA20;
	else if (of_machine_is_compatible("nvidia,tegra30"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA30;
	else if (of_machine_is_compatible("nvidia,tegra114"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA114;
	else if (of_machine_is_compatible("nvidia,tegra148"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA148;
	else if (of_machine_is_compatible("nvidia,tegra124"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA124;
	else if (of_machine_is_compatible("nvidia,tegra210"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA210;
	else if (of_machine_is_compatible("nvidia,tegra186"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA186;
	else if (!dev->of_node) {
		/* non-DT is always Tegra20 */
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA20;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA30;
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA114;
#elif defined(CONFIG_ARCH_TEGRA_14x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA148;
#elif defined(CONFIG_ARCH_TEGRA_12x_SOC)
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA124;
#endif
	} else
		/* DT boot, but unknown SoC */
		return -EINVAL;

	/* pll_p_out1 is not used for ahub for T210,T186 */
	if (data->soc < TEGRA_ASOC_UTILS_SOC_TEGRA210) {
		data->clk_pll_p_out1 = clk_get_sys(NULL, "pll_p_out1");
		if (IS_ERR(data->clk_pll_p_out1)) {
			dev_err(data->dev, "Can't retrieve clk pll_p_out1\n");
			ret = PTR_ERR(data->clk_pll_p_out1);
			goto err;
		}
	}

	data->clk_m = tegra_alt_asoc_utils_get_clk(dev, false, "clk_m");
	if (IS_ERR(data->clk_m)) {
		dev_err(data->dev, "Can't retrieve clk clk_m\n");
		ret = PTR_ERR(data->clk_m);
		goto err;
	}

	data->clk_pll_a = tegra_alt_asoc_utils_get_clk(dev, false, "pll_a");
	if (IS_ERR(data->clk_pll_a)) {
		dev_err(data->dev, "Can't retrieve clk pll_a\n");
		ret = PTR_ERR(data->clk_pll_a);
		goto err_put_pll_p_out1;
	}

	data->clk_pll_a_out0 = tegra_alt_asoc_utils_get_clk(dev, false,
							"pll_a_out0");
	if (IS_ERR(data->clk_pll_a_out0)) {
		dev_err(data->dev, "Can't retrieve clk pll_a_out0\n");
		ret = PTR_ERR(data->clk_pll_a_out0);
		goto err_put_pll_a;
	}

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		data->clk_cdev1 = clk_get_sys(NULL, "cdev1");
	else
		data->clk_cdev1 = tegra_alt_asoc_utils_get_clk(dev, true,
					"extern1");

	if (IS_ERR(data->clk_cdev1)) {
		dev_err(data->dev, "Can't retrieve clk cdev1\n");
		ret = PTR_ERR(data->clk_cdev1);
		goto err_put_pll_a_out0;
	}

	if (data->soc > TEGRA_ASOC_UTILS_SOC_TEGRA210) {
		data->clk_ahub = tegra_alt_asoc_utils_get_clk(dev, false,
								"ahub");
		if (IS_ERR(data->clk_ahub)) {
			dev_err(data->dev, "Can't retrieve clk ahub\n");
			ret = PTR_ERR(data->clk_ahub);
			goto err_put_cdev1;
		}

#if defined(CONFIG_COMMON_CLK)
		data->clk_cdev1_rst = devm_reset_control_get(dev,
						"extern1_rst");
		if (IS_ERR(data->clk_cdev1_rst)) {
			dev_err(dev, "Reset control is not found, err: %ld\n",
					PTR_ERR(data->clk_cdev1_rst));
			return PTR_ERR(data->clk_cdev1_rst);
		}

		reset_control_reset(data->clk_cdev1_rst);
#endif
	}

	if (data->soc < TEGRA_ASOC_UTILS_SOC_TEGRA210) {
		ret = tegra_alt_asoc_utils_set_rate(data, 48000,
					256 * 48000, 256 * 48000);
		if (ret)
			goto err_put_ahub;
	}

#if !defined(CONFIG_ARCH_TEGRA_18x_SOC)
	ret = clk_prepare_enable(data->clk_cdev1);
	if (ret) {
		dev_err(data->dev, "Can't enable clk cdev1/extern1");
		goto err_put_ahub;
	}
	data->clk_cdev1_state = 1;
#endif
	return 0;

err_put_ahub:
	if (data->soc > TEGRA_ASOC_UTILS_SOC_TEGRA210)
		tegra_alt_asoc_utils_clk_put(dev, data->clk_ahub);
err_put_cdev1:
	tegra_alt_asoc_utils_clk_put(dev, data->clk_cdev1);
err_put_pll_a_out0:
	tegra_alt_asoc_utils_clk_put(dev, data->clk_pll_a_out0);
err_put_pll_a:
	tegra_alt_asoc_utils_clk_put(dev, data->clk_pll_a);
err_put_pll_p_out1:
	if (data->soc < TEGRA_ASOC_UTILS_SOC_TEGRA210)
		clk_put(data->clk_pll_p_out1);
err:
	return ret;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_init);

int tegra_alt_asoc_utils_set_parent(struct tegra_asoc_audio_clock_info *data,
			int is_i2s_master)
{
	int ret = -ENODEV;

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		return ret;

	if (is_i2s_master) {
		ret = clk_set_parent(data->clk_cdev1, data->clk_pll_a_out0);
		if (ret) {
			dev_err(data->dev, "Can't set clk cdev1/extern1 parent");
			return ret;
		}
	} else {
		ret = clk_set_parent(data->clk_cdev1, data->clk_m);
		if (ret) {
			dev_err(data->dev, "Can't set clk cdev1/extern1 parent");
			return ret;
		}

		ret = clk_set_rate(data->clk_cdev1, 13000000);
		if (ret) {
			dev_err(data->dev, "Can't set clk rate");
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_set_parent);

int tegra_alt_asoc_utils_set_extern_parent(
	struct tegra_asoc_audio_clock_info *data, const char *parent)
{
	unsigned long rate;
	int err;

	if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
		return -ENODEV;

	rate = clk_get_rate(data->clk_cdev1);
	if (!strcmp(parent, "clk_m")) {
		err = clk_set_parent(data->clk_cdev1, data->clk_m);
		if (err) {
			dev_err(data->dev, "Can't set clk extern1 parent");
			return err;
		}
	} else if (!strcmp(parent, "pll_a_out0")) {
		err = clk_set_parent(data->clk_cdev1, data->clk_pll_a_out0);
		if (err) {
			dev_err(data->dev, "Can't set clk cdev1/extern1 parent");
			return err;
		}
	}

	err = clk_set_rate(data->clk_cdev1, rate);
	if (err) {
		dev_err(data->dev, "Can't set clk rate");
		return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_set_extern_parent);

void tegra_alt_asoc_utils_fini(struct tegra_asoc_audio_clock_info *data)
{
	if (data->clk_cdev1_state)
		clk_disable_unprepare(data->clk_cdev1);

	if (data->soc > TEGRA_ASOC_UTILS_SOC_TEGRA210)
		if (!IS_ERR(data->clk_ahub))
			tegra_alt_asoc_utils_clk_put(data->dev, data->clk_ahub);

	if (!IS_ERR(data->clk_pll_a_out0))
		tegra_alt_asoc_utils_clk_put(data->dev, data->clk_pll_a_out0);

	if (!IS_ERR(data->clk_pll_a))
		tegra_alt_asoc_utils_clk_put(data->dev, data->clk_pll_a);

	if (data->soc < TEGRA_ASOC_UTILS_SOC_TEGRA210)
		if (!IS_ERR(data->clk_pll_p_out1))
			clk_put(data->clk_pll_p_out1);
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_utils_fini);

#ifdef CONFIG_SWITCH
int tegra_alt_asoc_switch_register(struct switch_dev *sdev)
{
	int ret;

	if (is_switch_registered)
		return -EBUSY;

	ret = switch_dev_register(sdev);

	if (ret >= 0)
		is_switch_registered = true;

	return ret;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_switch_register);

void tegra_alt_asoc_switch_unregister(struct switch_dev *sdev)
{
	if (!is_switch_registered)
		return;

	switch_dev_unregister(sdev);
	is_switch_registered = false;
}
EXPORT_SYMBOL_GPL(tegra_alt_asoc_switch_unregister);
#endif

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra ASoC utility code");
MODULE_LICENSE("GPL");
