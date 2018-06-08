// SPDX-License_Identifier: GPL-2.0
// Copyright (C) 2018 Intel Corporation

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/tps68470.h>

#define TPS68470_CLK_NAME "tps68470-clk"

#define to_tps68470_clkdata(clkd) \
	container_of(clkd, struct tps68470_clkdata, clkout_hw)

static int osc_freq_hz = 20000000;
module_param(osc_freq_hz, int, 0644);

struct tps68470_clkout_freqs {
	unsigned int freq;
	unsigned int xtaldiv;
	unsigned int plldiv;
	unsigned int postdiv;
	unsigned int buckdiv;
	unsigned int boostdiv;

} clk_freqs[] = {
/*
 *  The PLL is used to multiply the crystal oscillator
 *  frequency range of 3 MHz to 27 MHz by a programmable
 *  factor of F = (M/N)*(1/P) such that the output
 *  available at the HCLK_A or HCLK_B pins are in the range
 *  of 4 MHz to 64 MHz in increments of 0.1 MHz
 *
 * hclk_# = osc_in * (((plldiv*2)+320) / (xtaldiv+30)) * (1 / 2^postdiv)
 *
 * PLL_REF_CLK should be as close as possible to 100kHz
 * PLL_REF_CLK = input clk / XTALDIV[7:0] + 30)
 *
 * PLL_VCO_CLK = (PLL_REF_CLK * (plldiv*2 + 320))
 *
 * BOOST should be as close as possible to 2Mhz
 * BOOST = PLL_VCO_CLK / (BOOSTDIV[4:0] + 16) *
 *
 * BUCK should be as close as possible to 5.2Mhz
 * BUCK = PLL_VCO_CLK / (BUCKDIV[3:0] + 5)
 *
 * osc_in   xtaldiv  plldiv   postdiv   hclk_#
 * 20Mhz    170      32       1         19.2Mhz
 * 20Mhz    170      40       1         20Mhz
 * 20Mhz    170      80       1         24Mhz
 *
 */
	{ 19200000, 170, 32, 1, 2, 3 },
	{ 20000000, 170, 40, 1, 3, 4 },
	{ 24000000, 170, 80, 1, 4, 8 },
};

struct tps68470_clkdata {
	struct tps68470 *tps68470;
	struct clk_hw clkout_hw;
	struct clk *clk;
	int clk_cfg_idx;
};

static int tps68470_clk_is_prepared(struct clk_hw *hw)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);
	struct tps68470 *tps = clkdata->tps68470;
	int val;

	if (tps68470_reg_read(tps, TPS68470_REG_PLLCTL, &val))
		return 0;

	return val & TPS68470_PLL_EN_MASK;
}

static int tps68470_clk_prepare(struct clk_hw *hw)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);
	struct tps68470 *tps = clkdata->tps68470;
	int idx = clkdata->clk_cfg_idx;
	u8 val;

	tps68470_reg_write(tps, TPS68470_REG_BOOSTDIV, clk_freqs[idx].boostdiv);
	tps68470_reg_write(tps, TPS68470_REG_BUCKDIV, clk_freqs[idx].buckdiv);
	tps68470_reg_write(tps, TPS68470_REG_PLLSWR, TPS68470_PLLSWR_DEFAULT);
	tps68470_reg_write(tps, TPS68470_REG_XTALDIV, clk_freqs[idx].xtaldiv);
	tps68470_reg_write(tps, TPS68470_REG_PLLDIV, clk_freqs[idx].plldiv);
	tps68470_reg_write(tps, TPS68470_REG_POSTDIV, clk_freqs[idx].postdiv);
	tps68470_reg_write(tps, TPS68470_REG_POSTDIV2, clk_freqs[idx].postdiv);

	tps68470_reg_write(tps, TPS68470_REG_CLKCFG2,
			   TPS68470_DRV_STR_2MA << TPS68470_OUTPUT_A_SHIFT);
	tps68470_reg_write(tps, TPS68470_REG_CLKCFG1,
			   (TPS68470_PLL_OUTPUT_ENABLE <<
			   TPS68470_OUTPUT_A_SHIFT) |
			   (TPS68470_PLL_OUTPUT_ENABLE <<
			   TPS68470_OUTPUT_B_SHIFT));
	val = TPS68470_PLL_EN_MASK |
	      TPS68470_OSC_EXT_CAP_DEFAULT << TPS68470_OSC_EXT_CAP_SHIFT |
	      TPS68470_CLK_SRC_XTAL << TPS68470_CLK_SRC_SHIFT;

	tps68470_reg_write(tps, TPS68470_REG_PLLCTL, val);

	return 0;
}

static void tps68470_clk_unprepare(struct clk_hw *hw)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);
	struct tps68470 *tps = clkdata->tps68470;

	/* disable clock first*/
	tps68470_clear_bits(tps, TPS68470_REG_PLLCTL, TPS68470_PLL_EN_MASK);

	/* write hw defaults */
	tps68470_reg_write(tps, TPS68470_REG_BOOSTDIV, 0);
	tps68470_reg_write(tps, TPS68470_REG_BUCKDIV, 0);
	tps68470_reg_write(tps, TPS68470_REG_PLLSWR, 0);
	tps68470_reg_write(tps, TPS68470_REG_XTALDIV, 0);
	tps68470_reg_write(tps, TPS68470_REG_PLLDIV, 0);
	tps68470_reg_write(tps, TPS68470_REG_POSTDIV, 0);
	tps68470_reg_write(tps, TPS68470_REG_CLKCFG2, 0);
	tps68470_reg_write(tps, TPS68470_REG_CLKCFG1, 0);
}

static int tps68470_clk_enable(struct clk_hw *hw)
{
	/*
	 * FIXME: enabled in prepare because need of
	 *	  i2c write and this should not sleep
	 */
	return 0;
}

static void tps68470_clk_disable(struct clk_hw *hw)
{
	/*
	 * FIXME: disabled in unprepare because need of
	 *	  i2c write and this should not sleep
	 */
}

static unsigned long tps68470_clk_recalc_rate(struct clk_hw *hw,
				    unsigned long parent_rate)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);

	return clk_freqs[clkdata->clk_cfg_idx].freq;
}

static int tps68470_clk_cfg_lookup(unsigned long rate)
{
	unsigned long best = ULONG_MAX;
	int i = 0, best_idx;

	for (i = 0; i < ARRAY_SIZE(clk_freqs); i++) {
		long diff = clk_freqs[i].freq - rate;

		if (0 == diff)
			return i;

		diff = abs(diff);
		if (diff < best) {
			best = diff;
			best_idx = i;
		}
	}

	return i;
}

static long tps68470_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long *parent_rate)
{
	int idx = tps68470_clk_cfg_lookup(rate);

	return clk_freqs[idx].freq;
}

static int tps68470_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
	struct tps68470_clkdata *clkdata = to_tps68470_clkdata(hw);
	int idx = tps68470_clk_cfg_lookup(rate);

	if (rate != clk_freqs[idx].freq)
		return -EINVAL;

	clkdata->clk_cfg_idx = idx;

	return 0;
}

static const struct clk_ops tps68470_clk_ops = {
	.is_prepared = tps68470_clk_is_prepared,
	.prepare = tps68470_clk_prepare,
	.unprepare = tps68470_clk_unprepare,
	.enable = tps68470_clk_enable,
	.disable = tps68470_clk_disable,
	.recalc_rate = tps68470_clk_recalc_rate,
	.round_rate = tps68470_clk_round_rate,
	.set_rate = tps68470_clk_set_rate,
};

static struct clk_init_data tps68470_clk_initdata = {
	.name = TPS68470_CLK_NAME,
	.ops = &tps68470_clk_ops,
	.flags = CLK_IS_ROOT,
};

static int tps68470_clk_probe(struct platform_device *pdev)
{
	struct tps68470 *tps68470 = dev_get_drvdata(pdev->dev.parent);
	struct tps68470_clkdata *tps68470_clkdata;
	int ret;

	tps68470_clkdata = devm_kzalloc(&pdev->dev, sizeof(*tps68470_clkdata),
					GFP_KERNEL);
	if (!tps68470_clkdata)
		return -ENOMEM;

	tps68470_clkdata->tps68470 = tps68470;
	tps68470_clkdata->clkout_hw.init = &tps68470_clk_initdata;
	tps68470_clkdata->clk =
		devm_clk_register(&pdev->dev, &tps68470_clkdata->clkout_hw);
	if (IS_ERR(tps68470_clkdata->clk))
		return PTR_ERR(tps68470_clkdata->clk);

	/* FIXME: Cannot remove clkdev so block module removal */
	ret = try_module_get(THIS_MODULE);
	if (!ret)
		goto error;

	ret = clk_register_clkdev(tps68470_clkdata->clk,
				  TPS68470_CLK_NAME, NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to register clkdev:%d\n", ret);
		goto error;
	}

	platform_set_drvdata(pdev, tps68470_clkdata);

	dev_info(tps68470->dev, "Registered %s clk\n", pdev->name);

	return 0;
error:
	clk_unregister(tps68470_clkdata->clk);

	return ret;
}

static struct platform_driver tps68470_clk = {
	.driver = {
		.name = TPS68470_CLK_NAME,
	},
	.probe = tps68470_clk_probe,
};

static int __init tps68470_clk_init(void)
{
	return platform_driver_register(&tps68470_clk);
}
subsys_initcall(tps68470_clk_init);

static void __exit tps68470_clk_exit(void)
{
	platform_driver_unregister(&tps68470_clk);
}
module_exit(tps68470_clk_exit);

MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_AUTHOR("Jian Xu Zheng <jian.xu.zheng@intel.com>");
MODULE_AUTHOR("Yuning Pu <yuning.pu@intel.com>");
MODULE_AUTHOR("Antti Laakso <antti.laakso@intel.com>");
MODULE_DESCRIPTION("clock driver for TPS68470 pmic");
MODULE_ALIAS("platform:tps68470-clk");
MODULE_LICENSE("GPL");
