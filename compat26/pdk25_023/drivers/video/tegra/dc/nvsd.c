/*
 * drivers/video/tegra/dc/nvsd.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <mach/dc.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/backlight.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "nvsd.h"

/* Elements for sysfs access */
#define NVSD_ATTR(__name) static struct kobj_attribute nvsd_attr_##__name = \
	__ATTR(__name, S_IRUGO|S_IWUSR, nvsd_settings_show, nvsd_settings_store)
#define NVSD_ATTRS_ENTRY(__name) (&nvsd_attr_##__name.attr)
#define IS_NVSD_ATTR(__name) (attr == &nvsd_attr_##__name)

static ssize_t nvsd_settings_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static ssize_t nvsd_settings_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t nvsd_registers_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

NVSD_ATTR(enable);
NVSD_ATTR(aggressiveness);
NVSD_ATTR(phase_in);
NVSD_ATTR(phase_in_video);
NVSD_ATTR(bin_width);
NVSD_ATTR(hw_update_delay);
NVSD_ATTR(use_vid_luma);
NVSD_ATTR(coeff);
NVSD_ATTR(blp_time_constant);
NVSD_ATTR(blp_step);
NVSD_ATTR(fc_time_limit);
NVSD_ATTR(fc_threshold);
NVSD_ATTR(lut);
NVSD_ATTR(bltf);
static struct kobj_attribute nvsd_attr_registers =
	__ATTR(registers, S_IRUGO, nvsd_registers_show, NULL);

static struct attribute *nvsd_attrs[] = {
	NVSD_ATTRS_ENTRY(enable),
	NVSD_ATTRS_ENTRY(aggressiveness),
	NVSD_ATTRS_ENTRY(phase_in),
	NVSD_ATTRS_ENTRY(phase_in_video),
	NVSD_ATTRS_ENTRY(bin_width),
	NVSD_ATTRS_ENTRY(hw_update_delay),
	NVSD_ATTRS_ENTRY(use_vid_luma),
	NVSD_ATTRS_ENTRY(coeff),
	NVSD_ATTRS_ENTRY(blp_time_constant),
	NVSD_ATTRS_ENTRY(blp_step),
	NVSD_ATTRS_ENTRY(fc_time_limit),
	NVSD_ATTRS_ENTRY(fc_threshold),
	NVSD_ATTRS_ENTRY(lut),
	NVSD_ATTRS_ENTRY(bltf),
	NVSD_ATTRS_ENTRY(registers),
	NULL,
};

static struct attribute_group nvsd_attr_group = {
	.attrs = nvsd_attrs,
};

static struct kobject *nvsd_kobj;

/* shared brightness variable */
static atomic_t *sd_brightness = NULL;
/* shared boolean for manual K workaround */
static atomic_t man_k_until_blank = ATOMIC_INIT(0);

static u8 nvsd_get_bw_idx(struct tegra_dc_sd_settings *settings)
{
	u8 bw;

	switch (settings->bin_width) {
	default:
	case -1:
	/* A -1 bin-width indicates 'automatic'
	   based upon aggressiveness. */
		settings->bin_width = -1;
		switch (settings->aggressiveness) {
		default:
		case 0:
		case 1:
			bw = SD_BIN_WIDTH_ONE;
			break;
		case 2:
		case 3:
		case 4:
			bw = SD_BIN_WIDTH_TWO;
			break;
		case 5:
			bw = SD_BIN_WIDTH_FOUR;
			break;
		}
		break;
	case 1:
		bw = SD_BIN_WIDTH_ONE;
		break;
	case 2:
		bw = SD_BIN_WIDTH_TWO;
		break;
	case 4:
		bw = SD_BIN_WIDTH_FOUR;
		break;
	case 8:
		bw = SD_BIN_WIDTH_EIGHT;
		break;
	}
	return bw >> 3;

}

/* phase in the luts based on the current and max step */
static void nvsd_phase_in_luts(struct tegra_dc_sd_settings *settings,
	struct tegra_dc *dc)
{
	u32 val;
	u8 bw_idx;
	int i;
	u16 cur_phase_step = settings->cur_phase_step;
	u16 phase_in_steps = settings->phase_in_steps;

	bw_idx = nvsd_get_bw_idx(settings);

	/* Phase in Final LUT */
	for (i = 0; i < DC_DISP_SD_LUT_NUM; i++) {
		val = SD_LUT_R((settings->lut[bw_idx][i].r *
				cur_phase_step)/phase_in_steps) |
			SD_LUT_G((settings->lut[bw_idx][i].g *
				cur_phase_step)/phase_in_steps) |
			SD_LUT_B((settings->lut[bw_idx][i].b *
				cur_phase_step)/phase_in_steps);

		tegra_dc_writel(dc, val, DC_DISP_SD_LUT(i));
	}
			/* Phase in Final BLTF */
	for (i = 0; i < DC_DISP_SD_BL_TF_NUM; i++) {
		val = SD_BL_TF_POINT_0(255-((255-settings->bltf[bw_idx][i][0])
				* cur_phase_step)/phase_in_steps) |
			SD_BL_TF_POINT_1(255-((255-settings->bltf[bw_idx][i][1])
				* cur_phase_step)/phase_in_steps) |
			SD_BL_TF_POINT_2(255-((255-settings->bltf[bw_idx][i][2])
				* cur_phase_step)/phase_in_steps) |
			SD_BL_TF_POINT_3(255-((255-settings->bltf[bw_idx][i][3])
				* cur_phase_step)/phase_in_steps);

		tegra_dc_writel(dc, val, DC_DISP_SD_BL_TF(i));
	}
}

/* handle the commands that may be invoked for phase_in */
static void nvsd_cmd_handler(struct tegra_dc_sd_settings *settings,
	struct tegra_dc *dc)
{
	u32 val;
	u8 bw_idx, bw;

	if (settings->cmd & ENABLE) {
		settings->cur_phase_step++;
		if (settings->cur_phase_step >= settings->phase_in_steps)
			settings->cmd &= ~ENABLE;

		nvsd_phase_in_luts(settings, dc);
	}
	if (settings->cmd & DISABLE) {
		settings->cur_phase_step--;
		nvsd_phase_in_luts(settings, dc);
		if (settings->cur_phase_step == 0) {
			/* finish up aggressiveness phase in */
			if (settings->cmd & AGG_CHG)
				settings->aggressiveness = settings->final_agg;
			settings->cmd = NO_CMD;
			settings->enable = 0;
			nvsd_init(dc, settings);
		}
	}
	if (settings->cmd & AGG_CHG) {
		if (settings->aggressiveness == settings->final_agg)
			settings->cmd &= ~AGG_CHG;
		if ((settings->cur_agg_step++ & (STEPS_PER_AGG_CHG - 1)) == 0) {
			settings->final_agg > settings->aggressiveness ?
				settings->aggressiveness++ :
				settings->aggressiveness--;

			/* Update aggressiveness value in HW */
			val = tegra_dc_readl(dc, DC_DISP_SD_CONTROL);
			val &= ~SD_AGGRESSIVENESS(0x7);
			val |= SD_AGGRESSIVENESS(settings->aggressiveness);

			/* Adjust bin_width for automatic setting */
			if (settings->bin_width == -1) {
				bw_idx = nvsd_get_bw_idx(settings);

				bw = bw_idx << 3;

				val &= ~SD_BIN_WIDTH_MASK;
				val |= bw;
			}
			tegra_dc_writel(dc, val, DC_DISP_SD_CONTROL);

			nvsd_phase_in_luts(settings, dc);
		}
	}
}

static bool nvsd_update_enable(struct tegra_dc_sd_settings *settings,
	int enable_val)
{

	if (enable_val != 1 && enable_val != 0)
		return false;

	if (!settings->cmd && settings->enable != enable_val) {
		settings->phase_in_steps =
			STEPS_PER_AGG_LVL*settings->aggressiveness;
		settings->cur_phase_step = enable_val ?
			0 : settings->phase_in_steps;
	}

	if (settings->enable != enable_val || settings->cmd & DISABLE) {
		settings->cmd &= ~(ENABLE | DISABLE);
		if (!settings->enable && enable_val)
			settings->cmd |= PHASE_IN;
		settings->cmd |= enable_val ? ENABLE : DISABLE;
		return true;
	}

	return false;
}

static bool nvsd_update_agg(struct tegra_dc_sd_settings *settings, int agg_val)
{
	int i;
	int pri_lvl = SD_AGG_PRI_LVL(agg_val);
	int agg_lvl = SD_GET_AGG(agg_val);
	struct tegra_dc_sd_agg_priorities *sd_agg_priorities =
		&settings->agg_priorities;

	if (agg_lvl > 5 || agg_lvl < 0)
		return false;
	else if (agg_lvl == 0 && pri_lvl == 0)
		return false;

	if (pri_lvl >= 0 && pri_lvl < 4)
		sd_agg_priorities->agg[pri_lvl] = agg_lvl;

	for (i = NUM_AGG_PRI_LVLS - 1; i >= 0; i--) {
		if (sd_agg_priorities->agg[i])
			break;
	}

	sd_agg_priorities->pri_lvl = i;
	pri_lvl = i;
	agg_lvl = sd_agg_priorities->agg[i];

	if (settings->phase_in && settings->enable &&
		settings->aggressiveness != agg_lvl) {

		settings->final_agg = agg_lvl;
		settings->cmd |= AGG_CHG;
		settings->cur_agg_step = 0;
		return true;
	} else if (settings->aggressiveness != agg_lvl) {
		settings->aggressiveness = agg_lvl;
		return true;
	}

	return false;
}

/* Functional initialization */
void nvsd_init(struct tegra_dc *dc, struct tegra_dc_sd_settings *settings)
{
	u32 i = 0;
	u32 val = 0;
	u32 bw_idx = 0;
	/* TODO: check if HW says SD's available */

	/* If SD's not present or disabled, clear the register and return. */
	if (!settings || settings->enable == 0) {
		/* clear the brightness val, too. */
		if (sd_brightness)
			atomic_set(sd_brightness, 255);

		sd_brightness = NULL;

		if (settings)
			settings->cur_phase_step = 0;
		tegra_dc_writel(dc, 0, DC_DISP_SD_CONTROL);
		return;
	}

	dev_dbg(&dc->ndev->dev, "NVSD Init:\n");

	/* init agg_priorities */
	if (!settings->agg_priorities.agg[0])
		settings->agg_priorities.agg[0] = settings->aggressiveness;

	/* WAR: Settings will not be valid until the next flip.
	 * Thus, set manual K to either HW's current value (if
	 * we're already enabled) or a non-effective value (if
	 * we're about to enable). */
	val = tegra_dc_readl(dc, DC_DISP_SD_CONTROL);

	if (val & SD_ENABLE_NORMAL)
		i = tegra_dc_readl(dc, DC_DISP_SD_HW_K_VALUES);
	else
		i = 0; /* 0 values for RGB = 1.0, i.e. non-affected */

	tegra_dc_writel(dc, i, DC_DISP_SD_MAN_K_VALUES);
	/* Enable manual correction mode here so that changing the
	 * settings won't immediately impact display dehavior. */
	val |= SD_CORRECTION_MODE_MAN;
	tegra_dc_writel(dc, val, DC_DISP_SD_CONTROL);

	bw_idx = nvsd_get_bw_idx(settings);

	/* Write LUT */
	if (!settings->cmd) {
		dev_dbg(&dc->ndev->dev, "  LUT:\n");

		for (i = 0; i < DC_DISP_SD_LUT_NUM; i++) {
			val = SD_LUT_R(settings->lut[bw_idx][i].r) |
				SD_LUT_G(settings->lut[bw_idx][i].g) |
				SD_LUT_B(settings->lut[bw_idx][i].b);
			tegra_dc_writel(dc, val, DC_DISP_SD_LUT(i));

			dev_dbg(&dc->ndev->dev, "    %d: 0x%08x\n", i, val);
		}
	}

	/* Write BL TF */
	if (!settings->cmd) {
		dev_dbg(&dc->ndev->dev, "  BL_TF:\n");

		for (i = 0; i < DC_DISP_SD_BL_TF_NUM; i++) {
			val = SD_BL_TF_POINT_0(settings->bltf[bw_idx][i][0]) |
				SD_BL_TF_POINT_1(settings->bltf[bw_idx][i][1]) |
				SD_BL_TF_POINT_2(settings->bltf[bw_idx][i][2]) |
				SD_BL_TF_POINT_3(settings->bltf[bw_idx][i][3]);

			tegra_dc_writel(dc, val, DC_DISP_SD_BL_TF(i));

			dev_dbg(&dc->ndev->dev, "    %d: 0x%08x\n", i, val);
		}
	} else if ((settings->cmd & PHASE_IN)) {
		settings->cmd &= ~PHASE_IN;
		/* Write NO_OP values for BLTF */
		for (i = 0; i < DC_DISP_SD_BL_TF_NUM; i++) {
			val = SD_BL_TF_POINT_0(0xFF) |
				SD_BL_TF_POINT_1(0xFF) |
				SD_BL_TF_POINT_2(0xFF) |
				SD_BL_TF_POINT_3(0xFF);

			tegra_dc_writel(dc, val, DC_DISP_SD_BL_TF(i));

			dev_dbg(&dc->ndev->dev, "    %d: 0x%08x\n", i, val);
		}
	}

	/* Set step correctly on init */
	if (!settings->cmd && settings->phase_in) {
		settings->phase_in_steps = STEPS_PER_AGG_LVL *
			settings->aggressiveness;
		settings->cur_phase_step = settings->enable ?
			settings->phase_in_steps : 0;
	}

	/* Write Coeff */
	val = SD_CSC_COEFF_R(settings->coeff.r) |
		SD_CSC_COEFF_G(settings->coeff.g) |
		SD_CSC_COEFF_B(settings->coeff.b);
	tegra_dc_writel(dc, val, DC_DISP_SD_CSC_COEFF);
	dev_dbg(&dc->ndev->dev, "  COEFF: 0x%08x\n", val);

	/* Write BL Params */
	val = SD_BLP_TIME_CONSTANT(settings->blp.time_constant) |
		SD_BLP_STEP(settings->blp.step);
	tegra_dc_writel(dc, val, DC_DISP_SD_BL_PARAMETERS);
	dev_dbg(&dc->ndev->dev, "  BLP: 0x%08x\n", val);

	/* Write Auto/Manual PWM */
	val = (settings->use_auto_pwm) ? SD_BLC_MODE_AUTO : SD_BLC_MODE_MAN;
	tegra_dc_writel(dc, val, DC_DISP_SD_BL_CONTROL);
	dev_dbg(&dc->ndev->dev, "  BL_CONTROL: 0x%08x\n", val);

	/* Write Flicker Control */
	val = SD_FC_TIME_LIMIT(settings->fc.time_limit) |
		SD_FC_THRESHOLD(settings->fc.threshold);
	tegra_dc_writel(dc, val, DC_DISP_SD_FLICKER_CONTROL);
	dev_dbg(&dc->ndev->dev, "  FLICKER_CONTROL: 0x%08x\n", val);

	/* Manage SD Control */
	val = 0;
	/* Stay in manual correction mode until the next flip. */
	val |= SD_CORRECTION_MODE_MAN;
	/* Enable / One-Shot */
	val |= (settings->enable == 2) ?
		(SD_ENABLE_ONESHOT | SD_ONESHOT_ENABLE) :
		SD_ENABLE_NORMAL;
	/* HW Update Delay */
	val |= SD_HW_UPDATE_DLY(settings->hw_update_delay);
	/* Video Luma */
	val |= (settings->use_vid_luma) ? SD_USE_VID_LUMA : 0;
	/* Aggressiveness */
	val |= SD_AGGRESSIVENESS(settings->aggressiveness);
	/* Bin Width (value derived from bw_idx) */
	val |= bw_idx << 3;
	/* Finally, Write SD Control */
	tegra_dc_writel(dc, val, DC_DISP_SD_CONTROL);
	dev_dbg(&dc->ndev->dev, "  SD_CONTROL: 0x%08x\n", val);

	/* set the brightness pointer */
	sd_brightness = settings->sd_brightness;

	/* note that we're in manual K until the next flip */
	atomic_set(&man_k_until_blank, 1);
}

/* Periodic update */
bool nvsd_update_brightness(struct tegra_dc *dc)
{
	u32 val = 0;
	int cur_sd_brightness;
	struct tegra_dc_sd_settings *settings = dc->out->sd_settings;
	int new_k, step;

	if (sd_brightness) {
		if (atomic_read(&man_k_until_blank)) {
			val = tegra_dc_readl(dc, DC_DISP_SD_CONTROL);
			val &= ~SD_CORRECTION_MODE_MAN;
			tegra_dc_writel(dc, val, DC_DISP_SD_CONTROL);
			atomic_set(&man_k_until_blank, 0);
		}

		if (settings->cmd)
			nvsd_cmd_handler(settings, dc);

		/* nvsd_cmd_handler may turn off didim */
		if (!settings->enable)
			return true;

		cur_sd_brightness = atomic_read(sd_brightness);

		/* read brightness value */
		val = tegra_dc_readl(dc, DC_DISP_SD_BL_CONTROL);
		val = SD_BLC_BRIGHTNESS(val);
		new_k = tegra_dc_readl(dc, DC_DISP_SD_HW_K_VALUES);
		new_k = SD_HW_K_R(new_k);

		if (settings->phase_in_video) {
			step = settings->phase_vid_step;
			/* check if pixel modification value has changed */
			if (new_k != settings->prev_k) {
				/* Compute how much to shift brightness by */
				step = ((int)val - cur_sd_brightness)*8/256;
				if (step <= 0)
					step = step ? 1 : ~step + 1;
				settings->prev_k = new_k;
			} else if (cur_sd_brightness != val) {
				/* Phase in Brightness */
				if (step--)
					val > cur_sd_brightness ?
						(cur_sd_brightness++)
						: (cur_sd_brightness--);
				atomic_set(sd_brightness, cur_sd_brightness);
				return true;
			}
			settings->phase_vid_step = step;
		} else if (val != (u32)cur_sd_brightness) {
			/* set brightness value and note the update */
			atomic_set(sd_brightness, (int)val);
			return true;
		}
	}

	/* No update needed. */
	return false;
}

static ssize_t nvsd_lut_show(struct tegra_dc_sd_settings *sd_settings,
	char *buf, ssize_t res)
{
	u32 i;
	u32 j;

	for (i = 0; i < NUM_BIN_WIDTHS; i++) {
		res += snprintf(buf + res, PAGE_SIZE - res,
			"Bin Width: %d\n", 1 << i);

		for (j = 0; j < DC_DISP_SD_LUT_NUM; j++) {
			res += snprintf(buf + res,
				PAGE_SIZE - res,
				"%d: R: %3d / G: %3d / B: %3d\n",
				j,
				sd_settings->lut[i][j].r,
				sd_settings->lut[i][j].g,
				sd_settings->lut[i][j].b);
		}
	}
	return res;
}

static ssize_t nvsd_bltf_show(struct tegra_dc_sd_settings *sd_settings,
	char *buf, ssize_t res)
{
	u32 i;
	u32 j;

	for (i = 0; i < NUM_BIN_WIDTHS; i++) {
		res += snprintf(buf + res, PAGE_SIZE - res,
			"Bin Width: %d\n", 1 << i);

		for (j = 0; j < DC_DISP_SD_BL_TF_NUM; j++) {
			res += snprintf(buf + res,
				PAGE_SIZE - res,
				"%d: 0: %3d / 1: %3d / 2: %3d / 3: %3d\n",
				j,
				sd_settings->bltf[i][j][0],
				sd_settings->bltf[i][j][1],
				sd_settings->bltf[i][j][2],
				sd_settings->bltf[i][j][3]);
		}
	}
	return res;
}

/* Sysfs accessors */
static ssize_t nvsd_settings_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct nvhost_device *ndev = to_nvhost_device(dev);
	struct tegra_dc *dc = nvhost_get_drvdata(ndev);
	struct tegra_dc_sd_settings *sd_settings = dc->out->sd_settings;
	ssize_t res = 0;

	if (sd_settings) {
		if (IS_NVSD_ATTR(enable))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->enable);
		else if (IS_NVSD_ATTR(aggressiveness))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->aggressiveness);
		else if (IS_NVSD_ATTR(phase_in))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->phase_in);
		else if (IS_NVSD_ATTR(phase_in_video))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->phase_in_video);
		else if (IS_NVSD_ATTR(bin_width))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->bin_width);
		else if (IS_NVSD_ATTR(hw_update_delay))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->hw_update_delay);
		else if (IS_NVSD_ATTR(use_vid_luma))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->use_vid_luma);
		else if (IS_NVSD_ATTR(coeff))
			res = snprintf(buf, PAGE_SIZE,
				"R: %d / G: %d / B: %d\n",
				sd_settings->coeff.r,
				sd_settings->coeff.g,
				sd_settings->coeff.b);
		else if (IS_NVSD_ATTR(blp_time_constant))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->blp.time_constant);
		else if (IS_NVSD_ATTR(blp_step))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->blp.step);
		else if (IS_NVSD_ATTR(fc_time_limit))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->fc.time_limit);
		else if (IS_NVSD_ATTR(fc_threshold))
			res = snprintf(buf, PAGE_SIZE, "%d\n",
				sd_settings->fc.threshold);
		else if (IS_NVSD_ATTR(lut))
			res = nvsd_lut_show(sd_settings, buf, res);
		else if (IS_NVSD_ATTR(bltf))
			res = nvsd_bltf_show(sd_settings, buf, res);
		else
			res = -EINVAL;
	} else {
		/* This shouldn't be reachable. But just in case... */
		res = -EINVAL;
	}

	return res;
}

#define nvsd_check_and_update(_min, _max, _varname) { \
	int val = simple_strtol(buf, NULL, 10); \
	if (val >= _min && val <= _max) { \
		sd_settings->_varname = val; \
		settings_updated = true; \
	} }

#define nvsd_get_multi(_ele, _num, _act, _min, _max) { \
	char *b, *c, *orig_b; \
	b = orig_b = kstrdup(buf, GFP_KERNEL); \
	for (_act = 0; _act < _num; _act++) { \
		if (!b) \
			break; \
		b = strim(b); \
		c = strsep(&b, " "); \
		if (!strlen(c)) \
			break; \
		_ele[_act] = simple_strtol(c, NULL, 10); \
		if (_ele[_act] < _min || _ele[_act] > _max) \
			break; \
	} \
	if (orig_b) \
		kfree(orig_b); \
}

static int nvsd_lut_store(struct tegra_dc_sd_settings *sd_settings,
	const char *buf)
{
	int ele[3 * DC_DISP_SD_LUT_NUM * NUM_BIN_WIDTHS];
	int i = 0;
	int j = 0;
	int num = 3 * DC_DISP_SD_LUT_NUM * NUM_BIN_WIDTHS;

	nvsd_get_multi(ele, num, i, 0, 255);

	if (i != num)
		return -EINVAL;

	for (i = 0; i < NUM_BIN_WIDTHS; i++) {
		for (j = 0; j < DC_DISP_SD_LUT_NUM; j++) {
			sd_settings->lut[i][j].r =
				ele[i * NUM_BIN_WIDTHS + j * 3 + 0];
			sd_settings->lut[i][j].g =
				ele[i * NUM_BIN_WIDTHS + j * 3 + 1];
			sd_settings->lut[i][j].b =
				ele[i * NUM_BIN_WIDTHS + j * 3 + 2];
		}
	}
	return 0;
}

static int nvsd_bltf_store(struct tegra_dc_sd_settings *sd_settings,
	const char *buf)
{
	int ele[4 * DC_DISP_SD_BL_TF_NUM];
	int i = 0, j = 0, num = 4 * DC_DISP_SD_BL_TF_NUM;

	nvsd_get_multi(ele, num, i, 0, 255);

	if (i != num)
		return -EINVAL;

	for (i = 0; i < NUM_BIN_WIDTHS; i++) {
		for (j = 0; j < DC_DISP_SD_BL_TF_NUM; j++) {
			sd_settings->bltf[i][j][0] =
				ele[i * NUM_BIN_WIDTHS + j * 4 + 0];
			sd_settings->bltf[i][j][1] =
				ele[i * NUM_BIN_WIDTHS + j * 4 + 1];
			sd_settings->bltf[i][j][2] =
				ele[i * NUM_BIN_WIDTHS + j * 4 + 2];
			sd_settings->bltf[i][j][3] =
				ele[i * NUM_BIN_WIDTHS + j * 4 + 3];
		}
	}

	return 0;
}

static ssize_t nvsd_settings_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct nvhost_device *ndev = to_nvhost_device(dev);
	struct tegra_dc *dc = nvhost_get_drvdata(ndev);
	struct tegra_dc_sd_settings *sd_settings = dc->out->sd_settings;
	ssize_t res = count;
	bool settings_updated = false;
	long int result;
	int err;

	if (sd_settings) {
		if (IS_NVSD_ATTR(enable)) {
			if (sd_settings->phase_in) {
				err = strict_strtol(buf, 10, &result);
				if (err)
					return err;

				if (nvsd_update_enable(sd_settings, result))
					nvsd_check_and_update(1, 1, enable);

			} else {
				nvsd_check_and_update(0, 1, enable);
			}
		} else if (IS_NVSD_ATTR(aggressiveness)) {
			err = strict_strtol(buf, 10, &result);
			if (err)
				return err;

			if (nvsd_update_agg(sd_settings, result)
					&& !sd_settings->phase_in)
				settings_updated = true;

		} else if (IS_NVSD_ATTR(phase_in)) {
			nvsd_check_and_update(0, 1, phase_in);
		} else if (IS_NVSD_ATTR(phase_in_video)) {
			nvsd_check_and_update(0, 1, phase_in_video);
			if (sd_settings->phase_in_video) {
				/* Phase in adjustments to pixels */
				sd_settings->blp.time_constant = 4;
				sd_settings->blp.step = 0;
			} else {
				/* Instant Adjustments to pixels */
				sd_settings->blp.time_constant = 1024;
				sd_settings->blp.step = 255;
			}
		} else if (IS_NVSD_ATTR(bin_width)) {
			nvsd_check_and_update(0, 8, bin_width);
		} else if (IS_NVSD_ATTR(hw_update_delay)) {
			nvsd_check_and_update(0, 2, hw_update_delay);
		} else if (IS_NVSD_ATTR(use_vid_luma)) {
			nvsd_check_and_update(0, 1, use_vid_luma);
		} else if (IS_NVSD_ATTR(coeff)) {
			int ele[3], i = 0, num = 3;
			nvsd_get_multi(ele, num, i, 0, 15);

			if (i == num) {
				sd_settings->coeff.r = ele[0];
				sd_settings->coeff.g = ele[1];
				sd_settings->coeff.b = ele[2];
				settings_updated = true;
			} else {
				res = -EINVAL;
			}
		} else if (IS_NVSD_ATTR(blp_time_constant)) {
			nvsd_check_and_update(0, 1024, blp.time_constant);
		} else if (IS_NVSD_ATTR(blp_step)) {
			nvsd_check_and_update(0, 255, blp.step);
		} else if (IS_NVSD_ATTR(fc_time_limit)) {
			nvsd_check_and_update(0, 255, fc.time_limit);
		} else if (IS_NVSD_ATTR(fc_threshold)) {
			nvsd_check_and_update(0, 255, fc.threshold);
		} else if (IS_NVSD_ATTR(lut)) {
			if (nvsd_lut_store(sd_settings, buf))
				res = -EINVAL;
			else
				settings_updated = true;
		} else if (IS_NVSD_ATTR(bltf)) {
			if (nvsd_bltf_store(sd_settings, buf))
				res = -EINVAL;
			else
				settings_updated = true;
		} else {
			res = -EINVAL;
		}

		/* Re-init if our settings were updated. */
		if (settings_updated) {
			nvsd_init(dc, sd_settings);

			/* Update backlight state IFF we're disabling! */
			if (!sd_settings->enable && sd_settings->bl_device) {
				/* Do the actual brightness update outside of
				 * the mutex */
				struct platform_device *pdev =
					sd_settings->bl_device;
				struct backlight_device *bl =
					platform_get_drvdata(pdev);

				if (bl)
					backlight_update_status(bl);
			}
		}
	} else {
		/* This shouldn't be reachable. But just in case... */
		res = -EINVAL;
	}

	return res;
}

#define NVSD_PRINT_REG(__name) { \
	u32 val = tegra_dc_readl(dc, __name); \
	res += snprintf(buf + res, PAGE_SIZE - res, #__name ": 0x%08x\n", \
		val); \
}

#define NVSD_PRINT_REG_ARRAY(__name) { \
	u32 val = 0, i = 0; \
	res += snprintf(buf + res, PAGE_SIZE - res, #__name ":\n"); \
	for (i = 0; i < __name##_NUM; i++) { \
		val = tegra_dc_readl(dc, __name(i)); \
		res += snprintf(buf + res, PAGE_SIZE - res, "  %d: 0x%08x\n", \
			i, val); \
	} \
}

static ssize_t nvsd_registers_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	struct device *dev = container_of((kobj->parent), struct device, kobj);
	struct nvhost_device *ndev = to_nvhost_device(dev);
	struct tegra_dc *dc = nvhost_get_drvdata(ndev);
	ssize_t res = 0;

	NVSD_PRINT_REG(DC_DISP_SD_CONTROL);
	NVSD_PRINT_REG(DC_DISP_SD_CSC_COEFF);
	NVSD_PRINT_REG_ARRAY(DC_DISP_SD_LUT);
	NVSD_PRINT_REG(DC_DISP_SD_FLICKER_CONTROL);
	NVSD_PRINT_REG(DC_DISP_SD_PIXEL_COUNT);
	NVSD_PRINT_REG_ARRAY(DC_DISP_SD_HISTOGRAM);
	NVSD_PRINT_REG(DC_DISP_SD_BL_PARAMETERS);
	NVSD_PRINT_REG_ARRAY(DC_DISP_SD_BL_TF);
	NVSD_PRINT_REG(DC_DISP_SD_BL_CONTROL);
	NVSD_PRINT_REG(DC_DISP_SD_HW_K_VALUES);
	NVSD_PRINT_REG(DC_DISP_SD_MAN_K_VALUES);

	return res;
}

/* Sysfs initializer */
int nvsd_create_sysfs(struct device *dev)
{
	int retval = 0;

	nvsd_kobj = kobject_create_and_add("smartdimmer", &dev->kobj);

	if (!nvsd_kobj)
		return -ENOMEM;

	retval = sysfs_create_group(nvsd_kobj, &nvsd_attr_group);

	if (retval) {
		kobject_put(nvsd_kobj);
		dev_err(dev, "%s: failed to create attributes\n", __func__);
	}

	return retval;
}

/* Sysfs destructor */
void __devexit nvsd_remove_sysfs(struct device *dev)
{
	if (nvsd_kobj) {
		sysfs_remove_group(nvsd_kobj, &nvsd_attr_group);
		kobject_put(nvsd_kobj);
	}
}
