/*
 * drivers/video/tegra/dc/hdmi2.0.c
 *
 * Copyright (c) 2014-2017, NVIDIA CORPORATION, All rights reserved.
 * Author: Animesh Kishore <ankishore@nvidia.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/clk/tegra.h>
#include <linux/nvhost.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/unistd.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#include <linux/of_address.h>
#include <linux/tegra_pm_domains.h>

#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/powergate.h>

#include "dc_reg.h"
#include "dc_priv.h"
#include "sor.h"
#include "sor_regs.h"
#include "edid.h"
#include "hdmi2.0.h"
#include "hdmihdcp.h"
#include "dpaux.h"
#ifdef CONFIG_ADF_TEGRA
#include "tegra_adf.h"
#endif
#include "hda_dc.h"
#include "hdmivrr.h"

#include <linux/tegra_prod.h>
#include "../../../../arch/arm/mach-tegra/iomap.h"

#include "hdmi2fpd_ds90uh949.h"
#include "max929x_hdmi2gmsl.h"

#if !defined(CONFIG_ARCH_TEGRA_18x_SOC)
static struct of_device_id tegra_sor_pd[] = {
	{ .compatible = "nvidia,tegra210-sor-pd", },
	{ .compatible = "nvidia,tegra132-sor-pd", },
	{},
};
#endif

static ssize_t hdmi_ddc_power_toggle(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t hdmi_ddc_power_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static struct kobj_attribute hdmi_ddc_power_config_array[TEGRA_MAX_DC] = {
	__ATTR(config, 0640, hdmi_ddc_power_show, hdmi_ddc_power_toggle),
	__ATTR(config, 0640, hdmi_ddc_power_show, hdmi_ddc_power_toggle)
};

static struct kobject *hdmi_ddc_array[TEGRA_MAX_DC];

static struct tmds_prod_pair tmds_config_modes[] = {
	{ /* 54 MHz */
	.clk = 54000000,
	.name = "prod_c_54M"
	},
	{ /* 75 MHz */
	.clk = 75000000,
	.name = "prod_c_75M"
	},
	{ /* 150 MHz */
	.clk = 150000000,
	.name = "prod_c_150M"
	},
	{ /* 200 MHz */
	.clk = 200000000,
	.name = "prod_c_200M"
	},
	{ /* 300 MHz */
	.clk = 300000000,
	.name = "prod_c_300M"
	},
	{ /* HDMI 2.0 */
	.clk = 600000000,
	.name = "prod_c_600M"
	}
};

static char *hpd_switch_name_array[TEGRA_MAX_DC] = {"hdmi0", "hdmi1"};
static char *hpd_switch_name_first_link = "hdmi";
static char *audio_switch_name_array[TEGRA_MAX_DC]
	= {"hdmi0_audio", "hdmi1_audio"};
static char *hdmi_ddc_name_array[TEGRA_MAX_DC]
	= {"hdmi0_ddc_power_toggle", "hdmi1_ddc_power_toggle"};
static char *hdmi_ddc_name_first_link = "hdmi_ddc_power_toggle";
static struct tegra_hdmi *dc_hdmi_array[TEGRA_MAX_DC];

static int tegra_hdmi_controller_enable(struct tegra_hdmi *hdmi);
static void tegra_hdmi_config_clk(struct tegra_hdmi *hdmi, u32 clk_type);
static long tegra_dc_hdmi_setup_clk(struct tegra_dc *dc, struct clk *clk);
static void tegra_hdmi_scdc_worker(struct work_struct *work);
static void tegra_hdmi_debugfs_init(struct tegra_hdmi *hdmi);
static void tegra_hdmi_hdr_worker(struct work_struct *work);


static inline bool tegra_hdmi_is_connected(struct tegra_hdmi *hdmi)
{
	return (hdmi->mon_spec.misc & FB_MISC_HDMI) ||
		(hdmi->mon_spec.misc & FB_MISC_HDMI_FORUM);
}

static inline void __maybe_unused
tegra_hdmi_irq_enable(struct tegra_hdmi *hdmi)
{
	if (tegra_platform_is_fpga())
		return;

	enable_irq(hdmi->irq);
}

static inline void __maybe_unused
tegra_hdmi_irq_disable(struct tegra_hdmi *hdmi)
{
	if (tegra_platform_is_fpga())
		return;

	disable_irq(hdmi->irq);
}

static inline bool __maybe_unused
tegra_hdmi_hpd_asserted(struct tegra_hdmi *hdmi)
{
	return tegra_dc_hpd(hdmi->dc);
}

static inline void tegra_hdmi_reset(struct tegra_hdmi *hdmi)
{
	if (tegra_platform_is_linsim())
		return;

#if defined(CONFIG_ARCH_TEGRA_18x_SOC)
	if (hdmi->sor->rst) {
		reset_control_assert(hdmi->sor->rst);
		mdelay(20);
		reset_control_deassert(hdmi->sor->rst);
		mdelay(20);
	}
#else
	tegra_periph_reset_assert(hdmi->sor->sor_clk);
	mdelay(20);
	tegra_periph_reset_deassert(hdmi->sor->sor_clk);
	mdelay(20);
#endif
}

static inline void _tegra_hdmi_ddc_enable(struct tegra_hdmi *hdmi)
{
	int sor_num = tegra_dc_which_sor(hdmi->dc);

	mutex_lock(&hdmi->ddc_refcount_lock);
	if (hdmi->ddc_refcount++)
		goto fail;
	tegra_hdmi_get(hdmi->dc);
	/*
	 * hdmi uses i2c lane muxed on dpaux1 pad.
	 * Enable dpaux1 pads and configure the mux.
	 */
	tegra_dpaux_config_pad_mode(hdmi->dc, sor_num,
					TEGRA_DPAUX_PAD_MODE_I2C);

fail:
	mutex_unlock(&hdmi->ddc_refcount_lock);
}

static inline void _tegra_hdmi_ddc_disable(struct tegra_hdmi *hdmi)
{
	int sor_num = tegra_dc_which_sor(hdmi->dc);

	mutex_lock(&hdmi->ddc_refcount_lock);

	if (WARN_ONCE(hdmi->ddc_refcount <= 0, "ddc refcount imbalance"))
		goto fail;
	if (--hdmi->ddc_refcount != 0)
		goto fail;

	/*
	 * hdmi uses i2c lane muxed on dpaux1 pad.
	 * Disable dpaux1 pads.
	 */
	tegra_dpaux_pad_power(hdmi->dc, sor_num, false);
	tegra_hdmi_put(hdmi->dc);

fail:
	mutex_unlock(&hdmi->ddc_refcount_lock);
}

static int tegra_hdmi_ddc_i2c_xfer(struct tegra_dc *dc,
					struct i2c_msg *msgs, int num)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);
	int ret;

	_tegra_hdmi_ddc_enable(hdmi);
	ret = i2c_transfer(hdmi->ddc_i2c_client->adapter, msgs, num);
	_tegra_hdmi_ddc_disable(hdmi);
	return ret;
}

static int tegra_hdmi_ddc_init(struct tegra_hdmi *hdmi, int edid_src)
{
	struct tegra_dc *dc = hdmi->dc;
	struct i2c_adapter *i2c_adap;
	int err = 0;
	struct i2c_board_info i2c_dev_info = {
		.type = "tegra_hdmi2.0",
		.addr = 0x50,
	};

	if (edid_src == EDID_SRC_PANEL)
		hdmi->edid = tegra_edid_create(dc, tegra_hdmi_ddc_i2c_xfer);
	else if (edid_src == EDID_SRC_DT)
		hdmi->edid = tegra_edid_create(dc, tegra_dc_edid_blob);
	if (IS_ERR_OR_NULL(hdmi->edid)) {
		dev_err(&dc->ndev->dev, "hdmi: can't create edid\n");
		return PTR_ERR(hdmi->edid);
	}
	tegra_dc_set_edid(dc, hdmi->edid);

	i2c_adap = i2c_get_adapter(dc->out->ddc_bus);
	if (!i2c_adap) {
		dev_err(&dc->ndev->dev,
			"hdmi: can't get adpater for ddc bus %d\n",
			dc->out->ddc_bus);
		err = -EBUSY;
		goto fail_edid_free;
	}
	hdmi->ddc_i2c_original_rate = i2c_get_adapter_bus_clk_rate(i2c_adap);

	hdmi->ddc_i2c_client = i2c_new_device(i2c_adap, &i2c_dev_info);
	i2c_put_adapter(i2c_adap);
	if (!hdmi->ddc_i2c_client) {
		dev_err(&dc->ndev->dev, "hdmi: can't create new i2c device\n");
		err = -EBUSY;
		goto fail_edid_free;
	}

	return 0;
fail_edid_free:
	tegra_edid_destroy(hdmi->edid);
	return err;
}

static int tegra_hdmi_scdc_i2c_xfer(struct tegra_dc *dc,
					struct i2c_msg *msgs, int num)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	return i2c_transfer(hdmi->scdc_i2c_client->adapter, msgs, num);
}

static int tegra_hdmi_scdc_init(struct tegra_hdmi *hdmi)
{
	struct tegra_dc *dc = hdmi->dc;
	struct i2c_adapter *i2c_adap;
	int err = 0;
	struct i2c_board_info i2c_dev_info = {
		.type = "tegra_hdmi_scdc",
		.addr = 0x54,
	};

	i2c_adap = i2c_get_adapter(dc->out->ddc_bus);
	if (!i2c_adap) {
		dev_err(&dc->ndev->dev,
			"hdmi: can't get adpater for scdc bus %d\n",
			dc->out->ddc_bus);
		err = -EBUSY;
		goto fail;
	}

	hdmi->scdc_i2c_client = i2c_new_device(i2c_adap, &i2c_dev_info);
	i2c_put_adapter(i2c_adap);
	if (!hdmi->scdc_i2c_client) {
		dev_err(&dc->ndev->dev,
			"hdmi: can't create scdc i2c device\n");
		err = -EBUSY;
		goto fail;
	}

	INIT_DELAYED_WORK(&hdmi->scdc_work, tegra_hdmi_scdc_worker);

	return 0;
fail:
	return err;
}

/*  does not return precise tmds character rate */
static u32 tegra_hdmi_mode_min_tmds_rate(const struct fb_videomode *mode)
{
	u32 tmds_csc_8bpc_khz = PICOS2KHZ(mode->pixclock);

	if (mode->vmode & (FB_VMODE_Y420 | FB_VMODE_Y420_ONLY))
		tmds_csc_8bpc_khz /= 2;

	return tmds_csc_8bpc_khz;
}

__maybe_unused
static bool tegra_hdmi_fb_mode_filter(const struct tegra_dc *dc,
					struct fb_videomode *mode)
{
	struct tegra_hdmi *hdmi = dc->out_data;

	if (!mode->pixclock)
		return false;

	if (mode->xres > 4096)
		return false;

#if defined(CONFIG_TEGRA_YUV_BYPASS_MODE_FILTER)
#if !defined(CONFIG_TEGRA_NVDISPLAY)
	/* No support for YUV modes on T210 hardware. Filter them out */
	if (mode->vmode & FB_VMODE_YUV_MASK)
		return false;
#else
	/* T186 hardware supports only YUV422. Filter out YUV420 modes */
	if ((mode->vmode & FB_VMODE_Y420_ONLY) ||
		(mode->vmode & FB_VMODE_Y420))
		return false;
#endif
#endif

	/* some non-compliant edids list 420vdb modes in vdb */
	if ((mode->vmode & FB_VMODE_Y420) &&
		!(tegra_edid_is_hfvsdb_present(hdmi->edid) &&
		tegra_edid_is_scdc_present(hdmi->edid)) &&
		tegra_edid_is_420db_present(hdmi->edid)) {
		mode->vmode &= ~FB_VMODE_Y420;
		mode->vmode |= FB_VMODE_Y420_ONLY;
	}

	if (mode->vmode & FB_VMODE_YUV_MASK &&
		tegra_edid_get_quirks(hdmi->edid) & TEGRA_EDID_QUIRK_NO_YUV)
		return false;

	/*
	 * There are currently many TVs in the market that actually do NOT support
	 * 4k@60fps 4:4:4 (594 MHz), (especially on the HDCP 2.2 ports), but
	 * advertise it in the DTD block in their EDIDs. The workaround for this port
	 * is to disable the 594 MHz mode if no HF-VSDB is present or if no SCDC
	 * support is indicated
	 */
	if (tegra_hdmi_mode_min_tmds_rate(mode) / 1000 >= 340 &&
		(!tegra_edid_is_hfvsdb_present(hdmi->edid) ||
		!tegra_edid_is_scdc_present(hdmi->edid)))
		return false;

	/*
	 * Check if mode's pixel clock requirement can be satisfied. Note that
	 * the pixclock value is in pico seconds.
	 */
	if (mode->pixclock && tegra_dc_get_out_max_pixclock(dc) &&
		mode->pixclock < tegra_dc_get_out_max_pixclock(dc))
		return false;

	/*
	 * Work around for modes that fail the constraint:
	 * V_FRONT_PORCH >= V_REF_TO_SYNC + 1
	 */
	if (mode->lower_margin == 1) {
		mode->lower_margin++;
		mode->upper_margin--;
		mode->vmode |= FB_VMODE_ADJUSTED;
	}

	if (!check_fb_videomode_timings(dc, mode)) {
		trace_printk("check_fb_videomode_timings: false\n"
			     "%u x %u @ %u Hz\n",
			     mode->xres, mode->yres, mode->pixclock);
		return false;
	}

	return true;
}

static void tegra_hdmi_ddc_power_toggle(struct tegra_hdmi *dc_hdmi, int value)
{
#if !defined(CONFIG_ARCH_TEGRA_18x_SOC)

	int partition_id = tegra_pd_get_powergate_id(tegra_sor_pd);
	if (partition_id < 0)
		return;
#endif

	if (dc_hdmi == NULL)
		return;

	if (value == 0) {
		_tegra_hdmi_ddc_disable(dc_hdmi);
#if !defined(CONFIG_ARCH_TEGRA_18x_SOC) && \
	IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS)
		tegra_powergate_partition(partition_id);
#else
		/* No specific powerdomain for SORs */
		tegra_dc_powergate_locked(dc_hdmi->dc);
#endif


	} else if (value == 1) {
#if !defined(CONFIG_ARCH_TEGRA_18x_SOC) && \
	IS_ENABLED(CONFIG_PM_GENERIC_DOMAINS)
		tegra_unpowergate_partition(partition_id);
#else
		/* No specific powerdomain for SORs */
		tegra_dc_unpowergate_locked(dc_hdmi->dc);
#endif
		_tegra_hdmi_ddc_enable(dc_hdmi);
	}

	return;
}

static int tegra_hdmi_get_mon_spec(struct tegra_hdmi *hdmi)
{
#define MAX_RETRY 100
#define MIN_RETRY_DELAY_US 200
#define MAX_RETRY_DELAY_US (MIN_RETRY_DELAY_US + 200)

	size_t attempt_cnt = 0;
	int err = 0;
	struct i2c_adapter *i2c_adap = i2c_get_adapter(hdmi->dc->out->ddc_bus);

	if (IS_ERR_OR_NULL(hdmi->edid)) {
		dev_err(&hdmi->dc->ndev->dev, "hdmi: edid not initialized\n");
		return PTR_ERR(hdmi->edid);
	}

	tegra_edid_i2c_adap_change_rate(i2c_adap, hdmi->ddc_i2c_original_rate);

	hdmi->mon_spec_valid = false;
	if (hdmi->mon_spec_valid)
		fb_destroy_modedb(hdmi->mon_spec.modedb);
	memset(&hdmi->mon_spec, 0, sizeof(hdmi->mon_spec));

	do {
		err = tegra_edid_get_monspecs(hdmi->edid, &hdmi->mon_spec);
		if (err < 0)
			usleep_range(MIN_RETRY_DELAY_US, MAX_RETRY_DELAY_US);
		else
			break;
	} while (++attempt_cnt < MAX_RETRY);

	if (err < 0) {
		dev_err(&hdmi->dc->ndev->dev, "hdmi: edid read failed\n");
		/* Try to load and parse the fallback edid */
		hdmi->edid->errors = EDID_ERRORS_READ_FAILED;
		err = tegra_edid_get_monspecs(hdmi->edid, &hdmi->mon_spec);
		if (err < 0) {
			dev_err(&hdmi->dc->ndev->dev,
				"hdmi: parsing fallback edid failed\n");
			return err;
		}
		dev_info(&hdmi->dc->ndev->dev, "hdmi: using fallback edid\n");
	}

	hdmi->mon_spec_valid = true;
	return 0;

#undef MAX_RETRY_DELAY_US
#undef MIN_RETRY_DELAY_US
#undef MAX_RETRY
}

static inline int tegra_hdmi_edid_read(struct tegra_hdmi *hdmi)
{
	int err;

	err = tegra_hdmi_get_mon_spec(hdmi);

	return err;
}

static int tegra_hdmi_get_eld(struct tegra_hdmi *hdmi)
{
	int err;

	hdmi->eld_valid = false;
	memset(&hdmi->eld, 0, sizeof(hdmi->eld));

	err = tegra_edid_get_eld(hdmi->edid, &hdmi->eld);
	if (err < 0) {
		dev_err(&hdmi->dc->ndev->dev, "hdmi: eld not available\n");
		return err;
	}

	hdmi->eld_valid = true;
	return 0;
}

static inline int tegra_hdmi_eld_read(struct tegra_hdmi *hdmi)
{
	return tegra_hdmi_get_eld(hdmi);
}

static void tegra_hdmi_edid_config(struct tegra_hdmi *hdmi)
{
#define CM_TO_MM(x) (x * 10)

	struct tegra_dc *dc = hdmi->dc;

	if (!hdmi->mon_spec_valid)
		return;

	dc->out->h_size = CM_TO_MM(hdmi->mon_spec.max_x);
	dc->out->v_size = CM_TO_MM(hdmi->mon_spec.max_y);
	hdmi->dvi = !tegra_hdmi_is_connected(hdmi);

#undef CM_TO_MM
}

static void tegra_hdmi_hotplug_notify(struct tegra_hdmi *hdmi,
					bool is_asserted)
{
	struct tegra_dc *dc = hdmi->dc;
	struct fb_monspecs *mon_spec;
	int n_display_timings, idx;

	if (is_asserted)
		mon_spec = &hdmi->mon_spec;
	else
		mon_spec = NULL;

#ifdef CONFIG_ADF_TEGRA
	if (dc->adf)
		tegra_adf_process_hotplug_connected(hdmi->dc->adf, mon_spec);
#else
	n_display_timings = 0;
	/*
	 * If display timing with non-zero pclk is specified in DT,
	 * skip parsing EDID from monitor.
	 */
	for (idx = 0; idx < dc->out->n_modes; idx++) {
		if (0 != dc->out->modes->pclk) {
			n_display_timings++;
			break;
		}
	}
	if (dc->fb && 0 == n_display_timings) {
		tegra_fb_update_monspecs(hdmi->dc->fb, mon_spec,
					tegra_hdmi_fb_mode_filter);
		tegra_fb_update_fix(hdmi->dc->fb, mon_spec);
	}
#endif

	dc->connected = is_asserted;
	tegra_dc_ext_process_hotplug(dc->ndev->id);

	switch_set_state(&hdmi->hpd_switch, is_asserted ? 1 : 0);
}

static int tegra_hdmi_edid_eld_setup(struct tegra_hdmi *hdmi)
{
	int err;

	tegra_dc_unpowergate_locked(hdmi->dc);

	err = tegra_hdmi_edid_read(hdmi);
	if (err < 0)
		goto fail;

	err = tegra_hdmi_eld_read(hdmi);
	if (err < 0)
		goto fail;

	err = tegra_hdmivrr_setup(hdmi);
	if (err < 0)
		dev_info(&hdmi->dc->ndev->dev, "vrr_setup failed\n");

	tegra_dc_powergate_locked(hdmi->dc);

	tegra_hdmi_edid_config(hdmi);

	/*
	 * eld is configured when audio needs it
	 * via tegra_hdmi_edid_config()
	 */

	tegra_hdmi_hotplug_notify(hdmi, true);
	return 0;
fail:
	tegra_dc_powergate_locked(hdmi->dc);
	return err;
}

static int tegra_hdmi_controller_disable(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_sor_data *sor = hdmi->sor;
	struct tegra_dc *dc = hdmi->dc;

	tegra_dc_get(dc);
	/* disable hdcp */
#ifdef CONFIG_HDCP
	tegra_nvhdcp_set_plug(hdmi->nvhdcp, 0);
#endif
	tegra_dc_sor_detach(sor);
	tegra_hdmi_config_clk(hdmi, TEGRA_HDMI_SAFE_CLK);
	tegra_sor_power_lanes(sor, 4, false);
	tegra_sor_hdmi_pad_power_down(sor);
	tegra_hdmi_reset(hdmi);
	tegra_hdmi_put(dc);
	cancel_delayed_work(&hdmi->hdr_worker);
	tegra_dc_put(dc);

	return 0;
}

static int tegra_hdmi_disable(struct tegra_hdmi *hdmi)
{
	struct tegra_dc *dc = hdmi->dc;

	if (!hdmi->enabled) {
		dc->connected = false;
		tegra_dc_ext_process_hotplug(dc->ndev->id);
		switch_set_state(&hdmi->hpd_switch, 0);
		return 0;
	}

	hdmi->enabled = false;
	hdmi->eld_valid = false;
	hdmi->mon_spec_valid = false;

	tegra_dc_disable(hdmi->dc);

	tegra_hdmi_hotplug_notify(hdmi, false);

	return 0;
}

static int (*tegra_hdmi_state_func[])(struct tegra_hdmi *) = {
	tegra_hdmi_disable,
	tegra_hdmi_edid_eld_setup,
};

enum tegra_hdmi_plug_states {
	TEGRA_HDMI_MONITOR_DISABLE,
	TEGRA_HDMI_MONITOR_ENABLE,
};

static int hdmi_hpd_process_edid_match(struct tegra_hdmi *hdmi, int match)
{
	int ret = 0;

	if (match) {
		if (!tegra_dc_ext_is_userspace_active()) {
			/* No userspace running. Enable DC with cached mode */
			dev_info(&hdmi->dc->ndev->dev,
			"hdmi: No EDID change. No userspace active. Using "
			"cached mode to initialize dc!\n");
			hdmi->dc->use_cached_mode = true;
			hdmi->plug_state = TEGRA_HDMI_MONITOR_ENABLE;
		} else {
			/*
			 * Userspace is active. No EDID change. Userspace will
			 * issue unblank call to enable DC later.
			 */
			dev_info(&hdmi->dc->ndev->dev, "hdmi: No EDID change "
			"after HPD bounce, taking no action\n");
			ret = -EINVAL;
		}
	} else {
		dev_info(&hdmi->dc->ndev->dev,
			"hdmi: EDID change after HPD bounce, resetting\n");
		hdmi->plug_state = TEGRA_HDMI_MONITOR_DISABLE;
	}

	return ret;
}

static int read_edid_into_buffer(struct tegra_hdmi *hdmi,
				 u8 *edid_data, size_t edid_data_len)
{
	int err, i;
	int extension_blocks;
	int max_ext_blocks = (edid_data_len / 128) - 1;

	err = tegra_edid_read_block(hdmi->edid, 0, edid_data);
	if (err) {
		dev_info(&hdmi->dc->ndev->dev, "hdmi: tegra_edid_read_block(0) returned err %d\n",
			err);
		return err;
	}
	extension_blocks = edid_data[0x7e];
	dev_info(&hdmi->dc->ndev->dev, "%s: extension_blocks = %d, max_ext_blocks = %d\n",
		__func__, extension_blocks, max_ext_blocks);
	if (extension_blocks > max_ext_blocks)
		extension_blocks = max_ext_blocks;
	for (i = 1; i <= extension_blocks; i++) {
		err = tegra_edid_read_block(hdmi->edid, i, edid_data + i * 128);
		if (err) {
			dev_info(&hdmi->dc->ndev->dev, "hdmi: tegra_edid_read_block(%d) returned err %d\n",
				i, err);
			return err;
		}
	}
	return i * 128;
}

static int hdmi_recheck_edid(struct tegra_hdmi *hdmi, int *match)
{
	int ret;
	u8 tmp[HDMI_EDID_MAX_LENGTH] = {0};
	ret = read_edid_into_buffer(hdmi, tmp, sizeof(tmp));
	dev_info(&hdmi->dc->ndev->dev, "%s: read_edid_into_buffer() returned %d\n",
		__func__, ret);
	if (ret > 0) {
		struct tegra_dc_edid *data = tegra_edid_get_data(hdmi->edid);
		dev_info(&hdmi->dc->ndev->dev, "old edid len = %ld\n",
			(long int)data->len);
		*match = ((ret == data->len) &&
			  !memcmp(tmp, data->buf, data->len));
		if (*match == 0) {
			print_hex_dump(KERN_INFO, "tmp :", DUMP_PREFIX_ADDRESS,
				       16, 4, tmp, ret, true);
			print_hex_dump(KERN_INFO, "data:", DUMP_PREFIX_ADDRESS,
				       16, 4, data->buf, data->len, true);
		}
		tegra_edid_put_data(data);
		ret = 0;
	}

	return ret;
}
static void tegra_hdmi_hpd_worker(struct work_struct *work)
{
	struct tegra_hdmi *hdmi = container_of(to_delayed_work(work),
				struct tegra_hdmi, hpd_worker);
	int err;
	bool connected;
	enum tegra_hdmi_plug_states orig_state;
	int match = 0;

	mutex_lock(&hdmi->hpd_lock);

	connected = tegra_dc_hpd(hdmi->dc);
	orig_state = hdmi->plug_state;

	if (connected) {
		switch (orig_state) {
		case TEGRA_HDMI_MONITOR_ENABLE:
			if (hdmi_recheck_edid(hdmi, &match)) {
				dev_info(&hdmi->dc->ndev->dev, "hdmi: unable to read EDID\n");
				goto fail;
			} else {
				err = hdmi_hpd_process_edid_match(hdmi, match);
				if (err < 0)
					goto fail;
			}
			break;
		case TEGRA_HDMI_MONITOR_DISABLE:
			hdmi->plug_state = TEGRA_HDMI_MONITOR_ENABLE;
			break;
		default:
			break;
		};
	} else {
		switch (orig_state) {
		case TEGRA_HDMI_MONITOR_ENABLE:
			hdmi->plug_state = TEGRA_HDMI_MONITOR_DISABLE;
			break;
		case TEGRA_HDMI_MONITOR_DISABLE:
			goto fail;
		default:
			break;
		};
	}

	err = tegra_hdmi_state_func[hdmi->plug_state](hdmi);

	if (err < 0) {
			dev_info(&hdmi->dc->ndev->dev,
				"hdmi state %d failed during %splug\n",
				hdmi->plug_state, connected ? "" : "un");
			hdmi->plug_state = orig_state;
			goto fail;
		} else {
			dev_info(&hdmi->dc->ndev->dev, "hdmi: %splugged\n",
					connected ? "" : "un");
		}

	if (connected && hdmi->plug_state == TEGRA_HDMI_MONITOR_DISABLE)
		goto reschedule_worker;

fail:
	mutex_unlock(&hdmi->hpd_lock);
	return;

reschedule_worker:
	mutex_unlock(&hdmi->hpd_lock);
	cancel_delayed_work_sync(&hdmi->hpd_worker);
	schedule_delayed_work(&hdmi->hpd_worker, 0);
	return;

}

static irqreturn_t tegra_hdmi_hpd_irq_handler(int irq, void *ptr)
{
	struct tegra_dc *dc = ptr;
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	if (atomic_read(&hdmi->suspended))
		return IRQ_HANDLED;

	cancel_delayed_work(&hdmi->hpd_worker);
	schedule_delayed_work(&hdmi->hpd_worker,
				msecs_to_jiffies(HDMI_HPD_DEBOUNCE_DELAY_MS));

	return IRQ_HANDLED;
}

static int tegra_dc_hdmi_hpd_init(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);
	int hotplug_gpio = dc->out->hotplug_gpio;
	int hotplug_irq;
	int err;

	if (!gpio_is_valid(hotplug_gpio)) {
		dev_err(&dc->ndev->dev, "hdmi: invalid hotplug gpio\n");
		return -EINVAL;
	}

	hotplug_irq = gpio_to_irq(hotplug_gpio);
	if (hotplug_irq < 0) {
		dev_err(&dc->ndev->dev,
			"hdmi: hotplug gpio to irq map failed\n");
		return -EINVAL;
	}

	err = gpio_request(hotplug_gpio, "hdmi2.0_hpd");
	if (err < 0)
		dev_err(&dc->ndev->dev,
			"hdmi: hpd gpio_request failed %d\n", err);
	gpio_direction_input(hotplug_gpio);

	err = request_threaded_irq(hotplug_irq,
				NULL, tegra_hdmi_hpd_irq_handler,
				(IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT),
				dev_name(&dc->ndev->dev), dc);
	if (err) {
		dev_err(&dc->ndev->dev,
			"hdmi: request_threaded_irq failed: %d\n", err);
		goto fail;
	}
	hdmi->irq = hotplug_irq;

	INIT_DELAYED_WORK(&hdmi->hpd_worker, tegra_hdmi_hpd_worker);

	mutex_init(&hdmi->hpd_lock);

	return 0;
fail:
	gpio_free(hotplug_gpio);
	return err;
}


struct tmds_range_info  {
	struct list_head  list;
	int  lowerHz;
	int  upperHz;
	const char  *pch_prod;
};


/*
 * Read a prod-setting list from DT
 * o Inputs:
 *  - hdmi: pointer to hdmi info
 *  - np_prod: pointer to prod-settings node
 *  - pch_prod_list: name of the DT property of prod-setting list
 *  - hd_range: pointer to linked list head for range info read
 * o Outputs:
 *  - return: pointer to a memory block allocated
 *            caller should free this memory block after use of the linked
 *            list *hd_range
 *            NULL to indicate failure
 *  - *hd_range: linked list of struct tmds_range_info read
 *               the list is sorted with lower boundary value
 */
static void  *tegra_hdmi_tmds_range_read(struct tegra_hdmi *hdmi,
		struct device_node *np_prod, char *pch_prod_list,
		struct list_head *hd_range)
{
	int  err = 0;
	int  num_str;
	int  i, j, k, n;
	int  upper, lower;
	const char  *pch_prod;
	struct device_node  *np;
	LIST_HEAD(head);
	struct tmds_range_info  *ranges = NULL;
	struct tmds_range_info  *pos, *lowest;
	LIST_HEAD(head_sorted);
#define  LEN_NAME  128
	char  buf_name[LEN_NAME];

	/* allocate single space for info array */
	num_str = of_property_count_strings(np_prod, pch_prod_list);
	if (num_str <= 0) {
		dev_warn(&hdmi->dc->ndev->dev,
			"hdmi: invalid prod list %s\n", pch_prod_list);
		goto fail_count;
	}
	ranges = kcalloc(num_str, sizeof(*ranges), GFP_KERNEL);
	if (!ranges)
		goto fail_alloc;

	/* read into the linked list */
	for (i = j = 0; i < num_str; i++) {
		err = of_property_read_string_index(np_prod,
				pch_prod_list, i, &pch_prod);
		if (err) {
			dev_warn(&hdmi->dc->ndev->dev,
				"hdmi: %s: string %d read failed, err:%d\n",
				pch_prod_list, i, err);
			continue;
		}
		if ('\0' == *pch_prod)
			continue;
		np = of_get_child_by_name(np_prod, pch_prod);
		if (np) {
			of_node_put(np);
		} else {
			dev_warn(&hdmi->dc->ndev->dev,
				"hdmi: prod-setting %s is not found\n",
				pch_prod);
			continue;
		}
		ranges[j].pch_prod = pch_prod;
		strlcpy(buf_name, pch_prod, LEN_NAME);
		for (k = 0; k < strlen(buf_name); k++) {
			if ('A' <= buf_name[k] && buf_name[k] <= 'Z')
				buf_name[k] += 'a' - 'A';
			else if ('-' == buf_name[k])
				buf_name[k] = '_';
		}
		if (2 == sscanf(buf_name, "prod_c_hdmi_%dm_%dm%n",
				&lower, &upper, &n)) {
			if (!(lower < upper) || strlen(pch_prod) != n) {
				dev_warn(&hdmi->dc->ndev->dev,
					"hdmi: %s has invalid range\n",
					pch_prod);
				continue;
			}
		} else {
			dev_warn(&hdmi->dc->ndev->dev,
				"hdmi: %s has missing boundary\n",
				pch_prod);
			continue;
		}
		ranges[j].lowerHz = lower * 1000000;
		ranges[j].upperHz = upper * 1000000;
		list_add_tail(&ranges[j].list, &head);
		j++;
	}

	/* sort ranges */
	while (!list_empty(&head)) {
		lowest = list_first_entry(&head, typeof(*lowest), list);
		list_for_each_entry(pos, &head, list) {
			if (pos->lowerHz < lowest->lowerHz)
				lowest = pos;
		}
		list_move_tail(&lowest->list, &head_sorted);
	}
	if (!list_empty(&head_sorted))
		list_replace(&head_sorted, hd_range);

fail_alloc:
fail_count:
	return ranges;
#undef  LEN_NAME
}


/*
 * Construct the HDMI TMDS range from HDMI prod-setting list in DT
 * HDMI has 2 prod-setting lists. The DT property prod_list_hdmi_soc has prod
 * list for CHIP_QUAL, and prod_list_hdmi_board has list for BOARD_QUAL.
 * Each range upper and lower boundary frequencies will be decoded from the
 * prod-setting node name in the list.
 * o inputs:
 *  - hdmi: HDMI info
 *  - np_prod: DT node for prod-setting
 * o outputs:
 *  - return: error code
 *  - hdmi->tmds_range: pointer to TMDS prod-setting range table
 */
static int  tegra_hdmi_tmds_range_init(struct tegra_hdmi *hdmi,
		struct device_node *np_prod)
{
	int  err = 0;
	int  i, l;
	int  num_list_soc = 0;
	int  num_list_bd = 0;
	LIST_HEAD(head_range_soc);
	LIST_HEAD(head_range_board);
	void  *pmem_soc = NULL,  *pmem_board = NULL;
	struct tmds_range_info  *pos_soc,  *tmp_soc;
	struct tmds_range_info  *pos_board,  *tmp_board;
	struct tmds_range_info  *spares = NULL;
	int  idx;
	struct tmds_prod_pair  *tmds_ranges = NULL;
	char  *pch_name;
#define  NAME_PROD_LIST_SOC  "prod_list_hdmi_soc"
#define  NAME_PROD_LIST_BOARD  "prod_list_hdmi_board"

	if (np_prod) {
		num_list_soc = of_property_count_strings(np_prod,
				NAME_PROD_LIST_SOC);
		if (num_list_soc < 0)
			num_list_soc = 0;
		num_list_bd = of_property_count_strings(np_prod,
				NAME_PROD_LIST_BOARD);
		if (num_list_bd < 0)
			num_list_bd = 0;
	}
	/* if prod-setting lists are not available
	 * then fall-back to default range */
	if (!np_prod || !num_list_soc || !num_list_bd)
		goto fall_back;

	/* make Chip Qual ranges */
	pmem_soc = tegra_hdmi_tmds_range_read(hdmi, np_prod,
			NAME_PROD_LIST_SOC, &head_range_soc);
	if (!pmem_soc) {
		dev_err(&hdmi->dc->ndev->dev,
			"hdmi: tegra_hdmi_tmds_range_read() for soc failed\n");
		err = EINVAL;
		goto fail_range_read_soc;
	}
	/* todo: check Chip Qual covering full spectrum
	 *       get the maximum TMDS frequency from DT */

	/* make Board Qual ranges */
	pmem_board = tegra_hdmi_tmds_range_read(hdmi, np_prod,
			NAME_PROD_LIST_BOARD, &head_range_board);
	if (!pmem_board) {
		dev_err(&hdmi->dc->ndev->dev,
			"hdmi: tegra_hdmi_tmds_range_read() for bd failed\n");
		err = EINVAL;
		goto fail_range_read_board;
	}
	/* Allocate spare range info for combining ranges */
	i = 0;
	list_for_each_entry(pos_board, &head_range_board, list)
		i++;
	spares = kcalloc(i, sizeof(*spares), GFP_KERNEL);
	if (!spares) {
		dev_err(&hdmi->dc->ndev->dev,
			"hdmi: kcalloc() for spares failed\n");
		err = ENOMEM;
		goto fall_alloc_spares;
	}
	/* Combine ranges of both Chip Qual and Board Qual
	 * Ranges for Board Qual will be inserted to Chip Qual linked list
	 * in sorted order and overlapped Chip Qual range will be removed */
	idx = 0;
	pos_soc = list_first_entry(&head_range_soc, typeof(*pos_soc), list);
	list_for_each_entry_safe(pos_board, tmp_board,
			&head_range_board, list) {
		bool  insert = false;

		list_for_each_entry_safe_from(pos_soc, tmp_soc,
				&head_range_soc, list) {
			if (pos_board->lowerHz < pos_soc->lowerHz &&
				pos_board->upperHz <= pos_soc->lowerHz) {
				insert = true;
				break;
			} else if (pos_board->lowerHz <= pos_soc->lowerHz &&
				pos_board->upperHz < pos_soc->upperHz) {
				insert = true;
				pos_soc->lowerHz = pos_board->upperHz;
				break;
			} else if (pos_board->lowerHz <= pos_soc->lowerHz &&
				pos_soc->upperHz <= pos_board->upperHz) {
				insert = true;
				list_del(&pos_soc->list);
			} else if (pos_soc->lowerHz < pos_board->lowerHz &&
				pos_board->upperHz < pos_soc->upperHz) {
				insert = true;
				spares[idx].lowerHz = pos_soc->lowerHz;
				spares[idx].upperHz = pos_board->lowerHz;
				spares[idx].pch_prod = pos_soc->pch_prod;
				list_add_tail(&spares[idx].list,
						&pos_soc->list);
				idx++;
				pos_soc->lowerHz = pos_board->upperHz;
				break;
			} else if (pos_soc->lowerHz < pos_board->lowerHz &&
				pos_board->lowerHz < pos_soc->upperHz &&
				pos_soc->upperHz <= pos_board->upperHz) {
				pos_soc->upperHz = pos_board->lowerHz;
			}
		}
		if (insert)
			list_move_tail(&pos_board->list, &pos_soc->list);
	}

	/* construct range info */
	i = l = 0;
	list_for_each_entry(pos_soc, &head_range_soc, list) {
		l += strlen(pos_soc->pch_prod) + 1;
		i++;
	}
	l += sizeof(*tmds_ranges) * (i + 1);
	tmds_ranges = kzalloc(l, GFP_KERNEL);
	if (!tmds_ranges) {
		dev_err(&hdmi->dc->ndev->dev,
			"hdmi: kzalloc() for tmds_ranges failed\n");
		err = ENOMEM;
		goto fall_alloc_tmds_ranges;
	}
	pch_name = (char *)&tmds_ranges[i + 1];
	i = 0;
	list_for_each_entry(pos_soc, &head_range_soc, list) {
		tmds_ranges[i].clk = pos_soc->upperHz;
		strcpy(pch_name, pos_soc->pch_prod);
		tmds_ranges[i].name = pch_name;
		pch_name += strlen(pos_soc->pch_prod) + 1;
		i++;
	}
	hdmi->tmds_range = tmds_ranges;

	err = 0;
	goto nofail;

	/* fall-back to the default range */
fall_back:
	dev_info(&hdmi->dc->ndev->dev,
		"hdmi: no %s, use default range\n",
		!np_prod ? "prod-settings" :
			!num_list_soc ? NAME_PROD_LIST_SOC :
				NAME_PROD_LIST_BOARD);
	hdmi->tmds_range = kcalloc(ARRAY_SIZE(tmds_config_modes) + 1,
			sizeof(tmds_config_modes[0]), GFP_KERNEL);
	if (!hdmi->tmds_range) {
		err = ENOMEM;
		goto fail_alloc_fallback;
	}
	memcpy(hdmi->tmds_range, tmds_config_modes, sizeof(tmds_config_modes));
	err = 0;

nofail:

fall_alloc_tmds_ranges:
	kfree(spares);
fall_alloc_spares:
	kfree(pmem_board);
fail_range_read_board:
	kfree(pmem_soc);
fail_range_read_soc:
fail_alloc_fallback:
	return err;
#undef  NAME_PROD_LIST_SOC
#undef  NAME_PROD_LIST_BOARD
}

static int tegra_hdmi_tmds_init(struct tegra_hdmi *hdmi)
{
	int sor_num = tegra_dc_which_sor(hdmi->dc);
	int retval = 0;
	struct device_node *np_sor = of_find_node_by_path(
			sor_num ? SOR1_NODE : SOR_NODE);
	struct device_node *np_sor_dpaux = of_find_node_by_path(
			sor_num ? DPAUX1_NODE : DPAUX_NODE);
	struct device_node *np_ps;

	if (tegra_platform_is_linsim()) {
		of_node_put(np_sor);
		of_node_put(np_sor_dpaux);
		return 0;
	}

	/* NULL check for both np_sor and np_sor_dpaux */
	if ((!np_sor) || (!np_sor_dpaux)) {
		dev_warn(&hdmi->dc->ndev->dev,
			"hdmi: find tmds prod or dpaux prod node failed\n");
		retval = -EINVAL;
		goto fail;
	}

	hdmi->prod_list = devm_tegra_prod_get_from_node(&hdmi->dc->ndev->dev,
							np_sor);
	if (IS_ERR(hdmi->prod_list)) {
		dev_warn(&hdmi->dc->ndev->dev,
			"hdmi: prod list init failed with error %ld\n",
			PTR_ERR(hdmi->prod_list));
		retval = -EINVAL;
		goto fail;
	}

	hdmi->dpaux_prod_list =
	devm_tegra_prod_get_from_node(&hdmi->dc->ndev->dev,
					np_sor_dpaux);
	if (IS_ERR(hdmi->dpaux_prod_list)) {
		dev_warn(&hdmi->dc->ndev->dev,
			"hdmi: prod list init failed for DPAUX with error %ld\n",
			PTR_ERR(hdmi->dpaux_prod_list));
		retval = -EINVAL;
		goto fail;
	}

	/* construct the HDMI TMDS range */
	np_ps = of_get_child_by_name(np_sor, "prod-settings");
	retval = tegra_hdmi_tmds_range_init(hdmi, np_ps);
	if (retval) {
		dev_warn(&hdmi->dc->ndev->dev,
			"hdmi: TMDS range init failed with error %d\n",
			retval);
		if (np_ps)
			of_node_put(np_ps);
		retval = -EINVAL;
		goto fail;
	}
	if (np_ps)
		of_node_put(np_ps);

fail:
	of_node_put(np_sor);
	of_node_put(np_sor_dpaux);
	return retval;
}

static int tegra_hdmi_config_tmds(struct tegra_hdmi *hdmi)
{
	int i;
	int err = 0;

	if (tegra_platform_is_linsim())
		return 0;

	/* Select mode with smallest clk freq > pclk */
	for (i = 0; hdmi->tmds_range[i].clk; i++) {
		if (hdmi->tmds_range[i].clk < hdmi->dc->mode.pclk)
			continue;

		dev_info(&hdmi->dc->ndev->dev,
			"hdmi: pclk:%dK, set prod-setting:%s\n",
			hdmi->dc->mode.pclk / 1000, hdmi->tmds_range[i].name);
		err = tegra_prod_set_by_name(&hdmi->sor->base,
			hdmi->tmds_range[i].name, hdmi->prod_list);
		/* Return if matching mode found */
		if (!err)
			return 0;
	}

	dev_warn(&hdmi->dc->ndev->dev,
		"hdmi: tmds prod set for pclk:%d failed\n",
		hdmi->dc->mode.pclk);
	return -EINVAL;
}

#ifdef CONFIG_TEGRA_NVDISPLAY

static int tegra_hdmi_dpaux_init(struct tegra_hdmi *hdmi)
{
	struct resource *base_res;
	struct resource of_dpaux_res;
	int err = 0;
	struct reset_control *dpaux_rst;

	int sor_num = tegra_dc_which_sor(hdmi->dc);
	struct device_node *np_dpaux =
		sor_num ? of_find_node_by_path(DPAUX1_NODE)
		: of_find_node_by_path(DPAUX_NODE);

	if (np_dpaux && (of_device_is_available(np_dpaux))) {
		of_address_to_resource(np_dpaux, 0, &of_dpaux_res);
		hdmi->hdmi_dpaux_res[sor_num] = &of_dpaux_res;
	} else {
		dev_err(&hdmi->dc->ndev->dev, "hdmi: no dpaux node found\n");
		err = -EFAULT;
		return err;
	}

	if (!hdmi->hdmi_dpaux_res[sor_num]) {
		dev_err(&hdmi->dc->ndev->dev, "hdmi: no mem resources for dpaux\n");
		err = -EFAULT;
		goto err_free_dpaux;
	}

	base_res = devm_request_mem_region(&hdmi->dc->ndev->dev,
		hdmi->hdmi_dpaux_res[sor_num]->start,
		resource_size(hdmi->hdmi_dpaux_res[sor_num]),
		hdmi->dc->ndev->name);
	if (!base_res) {
		dev_err(&hdmi->dc->ndev->dev, "hdmi: request_mem_region failed\n");
		err = -EFAULT;
		goto err_free_dpaux;
	}

	hdmi->hdmi_dpaux_base[sor_num] = devm_ioremap(&hdmi->dc->ndev->dev,
			hdmi->hdmi_dpaux_res[sor_num]->start,
			resource_size(hdmi->hdmi_dpaux_res[sor_num]));
	if (!hdmi->hdmi_dpaux_base[sor_num]) {
		dev_err(&hdmi->dc->ndev->dev, "hdmi: registers can't be mapped\n");
		err = -EFAULT;
		goto err_release_resource_reg;
	}

	/* check for dpaux clock reset control */
	if (tegra_bpmp_running()) {
		dpaux_rst =
			of_reset_control_get(np_dpaux,
				(sor_num) ? "dpaux1" : "dpaux");
		if (IS_ERR_OR_NULL(dpaux_rst)) {
			dev_err(&hdmi->dc->ndev->dev,
				"Unable to get dpaux%u reset control\n",
				sor_num);
			err = PTR_ERR(dpaux_rst);
			goto dpaux_fail;
		}
		reset_control_deassert(dpaux_rst);
		reset_control_put(dpaux_rst);
	}

	tegra_set_dpaux_addr(hdmi->hdmi_dpaux_base[sor_num], sor_num);
	of_node_put(np_dpaux);
	return 0;

dpaux_fail:
	devm_iounmap(&hdmi->dc->ndev->dev, hdmi->hdmi_dpaux_base[sor_num]);

err_release_resource_reg:
	devm_release_mem_region(&hdmi->dc->ndev->dev,
		hdmi->hdmi_dpaux_res[sor_num]->start,
		resource_size(hdmi->hdmi_dpaux_res[sor_num]));

err_free_dpaux:
	of_node_put(np_dpaux);

	return err;
}
#endif

static int tegra_hdmi_hdr_init(struct tegra_hdmi *hdmi)
{
	INIT_DELAYED_WORK(&hdmi->hdr_worker, tegra_hdmi_hdr_worker);
	return 0;
}

static int tegra_dc_hdmi_init(struct tegra_dc *dc)
{
	static int s_hdmi_init_count;
	struct tegra_hdmi *hdmi;
	int err;
	struct device_node *np = dc->ndev->dev.of_node;
	int sor_num = tegra_dc_which_sor(dc);
#ifdef CONFIG_OF
	struct device_node *np_hdmi = of_find_node_by_path(
			sor_num ? SOR1_NODE : SOR_NODE);
#else
	struct device_node *np_hdmi = NULL;
#endif
	struct device_node *np_panel = NULL;
	int edid_src = EDID_SRC_PANEL;

	hdmi = devm_kzalloc(&dc->ndev->dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi) {
		err = -ENOMEM;
		goto fail_np_hdmi;
	}

	hdmi->dc = dc;
	dc_hdmi_array[sor_num] = hdmi;

#ifdef CONFIG_TEGRA_NVDISPLAY
	err = tegra_hdmi_dpaux_init(hdmi);
	if (err) {
		goto fail_hdmi;
	}
#endif

	hdmi->sor = tegra_dc_sor_init(dc, NULL);
	if (IS_ERR_OR_NULL(hdmi->sor)) {
		err = PTR_ERR(hdmi->sor);
		goto fail_tegra_hdmi_dpaux_init;
	}

	if (np) {
		if (np_hdmi && of_device_is_available(np_hdmi)) {
			np_panel = tegra_get_panel_node_out_type_check(dc,
				TEGRA_DC_OUT_HDMI);
			if (np_panel && of_device_is_available(np_panel)) {
				if (of_property_read_bool(np_panel,
							"nvidia,edid"))
					edid_src = EDID_SRC_DT;
				of_node_put(np_panel);
			}
		} else {
			err = -EINVAL;
			goto fail_tegra_hdmi_dpaux_init;
		}
	}

	hdmi->pdata = dc->pdata->default_out->hdmi_out;
	hdmi->ddc_refcount = 0; /* assumes this is disabled when starting */
	mutex_init(&hdmi->ddc_refcount_lock);
	hdmi->nvhdcp = NULL;
	hdmi->mon_spec_valid = false;
	hdmi->eld_valid = false;
	hdmi->device_shutdown = false;
	if (0) {
		/* TODO: seamless boot mode needs initialize the state */
	} else {
		hdmi->enabled = false;
		atomic_set(&hdmi->clock_refcount, 0);
	}
	atomic_set(&hdmi->suspended, 0);
#ifdef CONFIG_HDCP
	hdmi->nvhdcp = tegra_nvhdcp_create(hdmi, dc->ndev->id,
			dc->out->ddc_bus);
	if (IS_ERR_OR_NULL(hdmi->nvhdcp)) {
		err = PTR_ERR(hdmi->nvhdcp);
		goto fail_np_panel;
	}
	tegra_nvhdcp_debugfs_init(hdmi->nvhdcp);
#endif

	tegra_hdmi_ddc_init(hdmi, edid_src);

	tegra_hdmi_scdc_init(hdmi);

	tegra_hdmi_vrr_init(hdmi);

	tegra_hdmi_debugfs_init(hdmi);

	tegra_hdmi_tmds_init(hdmi);

	tegra_dc_set_outdata(dc, hdmi);

	tegra_hdmi_hdr_init(hdmi);

	if (hdmi->pdata->hdmi2fpd_bridge_enable) {
		hdmi2fpd_init(dc);
		hdmi2fpd_enable(dc);
	}
#ifdef CONFIG_TEGRA_HDMI2GMSL_MAX929x
	if (hdmi->pdata->hdmi2gmsl_bridge_enable) {
		hdmi->out_ops = &tegra_hdmi2gmsl_ops;
		hdmi->out_ops->init(hdmi);
	}
#endif	/* CONFIG_TEGRA_HDMI2GMSL_MAX929x */

	/* NOTE: Below code is applicable to L4T or embedded systems and is
	 * protected accordingly. This section early enables DC with first mode
	 * from the monitor specs.
	 * In case there is no hotplug we are falling back
	 * to default VGA mode.
	 */
	if (((fb_console_mapped()) ||
			((dc->pdata->flags & TEGRA_DC_FLAG_ENABLED) &&
			 (dc->pdata->flags & TEGRA_DC_FLAG_SET_EARLY_MODE))) &&
			dc->out && (dc->out->type == TEGRA_DC_OUT_HDMI)) {
		struct fb_monspecs specs;
		if (tegra_dc_hpd(dc) && (!dc->initialized)) {
			if (!tegra_edid_get_monspecs(hdmi->edid, &specs))
				tegra_dc_set_fb_mode(dc, specs.modedb, false);
			else {
			/* if for some reason there is no edid upon hotplug */
				tegra_dc_set_fb_mode(dc,
						&tegra_dc_vga_mode, false);
			}
		} else

			tegra_dc_set_fb_mode(dc, &tegra_dc_vga_mode, false);
	}

	hdmi->hpd_switch.name = hpd_switch_name_array[sor_num];
	err = switch_dev_register(&hdmi->hpd_switch);
	if (err)
		dev_err(&dc->ndev->dev,
			"hdmi: failed to register hpd switch %d, err=%d\n",
			sor_num, err);

#ifdef CONFIG_SWITCH
	if (hdmi->sor->audio_switch_name == NULL)
		hdmi->audio_switch.name = audio_switch_name_array[sor_num];
	else
		hdmi->audio_switch.name = hdmi->sor->audio_switch_name;

	err = switch_dev_register(&hdmi->audio_switch);
	if (err)
		dev_err(&dc->ndev->dev,
			"hdmi: failed to register audio switch %d, err=%d\n",
			sor_num, err);
#endif

	hdmi_ddc_array[sor_num] = kobject_create_and_add
		(hdmi_ddc_name_array[sor_num], kernel_kobj);
	if (!hdmi_ddc_array[sor_num]) {
		pr_warn("%s: kobject create_and_add \"%s\" failed\n",
			__func__, hdmi_ddc_name_array[sor_num]);
		err = -ENOMEM;
		goto fail_np_panel;
	}
	err = sysfs_create_file(hdmi_ddc_array[sor_num],
		&hdmi_ddc_power_config_array[sor_num].attr);
	if (err) {
		pr_warn("%s: sysfs create file hdmi_ddc_power_toggle failed\n",
			__func__);
		err = -ENOENT;
		goto fail_hdmi_ddc_array;
	}
	if (0 == s_hdmi_init_count) {
		err = sysfs_create_link(hdmi->hpd_switch.dev->kobj.parent,
			&(hdmi->hpd_switch.dev->kobj),
			hpd_switch_name_first_link);
		err = sysfs_create_link(kernel_kobj, hdmi_ddc_array[sor_num],
			hdmi_ddc_name_first_link);
	}
	s_hdmi_init_count++;
	of_node_put(np_hdmi);
	return 0;

fail_hdmi_ddc_array:
	kobject_put(hdmi_ddc_array[sor_num]);
fail_np_panel:
	of_node_put(np_panel);
fail_tegra_hdmi_dpaux_init:
#ifdef CONFIG_TEGRA_NVDISPLAY
	devm_iounmap(&dc->ndev->dev, hdmi->hdmi_dpaux_base[sor_num]);
	devm_release_mem_region(&dc->ndev->dev,
		hdmi->hdmi_dpaux_res[sor_num]->start,
		resource_size(hdmi->hdmi_dpaux_res[sor_num]));
fail_hdmi:
#endif
	kfree(hdmi->tmds_range);
	hdmi->tmds_range = NULL;
	devm_kfree(&dc->ndev->dev, hdmi);
fail_np_hdmi:
	of_node_put(np_hdmi);
	return err;
}

static void tegra_dc_hdmi_destroy(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	if (hdmi->pdata->hdmi2fpd_bridge_enable)
		hdmi2fpd_destroy(dc);
	if (NULL != hdmi->out_ops && NULL != hdmi->out_ops->destroy)
		hdmi->out_ops->destroy(hdmi);
	tegra_dc_sor_destroy(hdmi->sor);
	tegra_edid_destroy(hdmi->edid);
#ifdef CONFIG_HDCP
	tegra_nvhdcp_destroy(hdmi->nvhdcp);
#endif
	free_irq(gpio_to_irq(dc->out->hotplug_gpio), dc);
	gpio_free(dc->out->hotplug_gpio);
	kfree(hdmi->tmds_range);
	hdmi->tmds_range = NULL;
	hdmi->prod_list = NULL;
	hdmi->dpaux_prod_list = NULL;

	switch_dev_unregister(&hdmi->hpd_switch);
#ifdef CONFIG_SWITCH
	switch_dev_unregister(&hdmi->audio_switch);
#endif
	devm_kfree(&dc->ndev->dev, hdmi);
}

static void tegra_hdmi_config(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_sor_data *sor = hdmi->sor;
	struct tegra_dc *dc = hdmi->dc;
#ifndef CONFIG_TEGRA_NVDISPLAY
	u32 h_pulse_start, h_pulse_end;
#endif
	u32 hblank, max_ac, rekey;
	unsigned long val;
	u32 dispclk_div_8_2;

	if (tegra_platform_is_linsim())
		return;

	tegra_sor_write_field(sor, NV_SOR_INPUT_CONTROL,
			NV_SOR_INPUT_CONTROL_ARM_VIDEO_RANGE_LIMITED |
			NV_SOR_INPUT_CONTROL_HDMI_SRC_SELECT_DISPLAYB,
			NV_SOR_INPUT_CONTROL_ARM_VIDEO_RANGE_FULL |
			NV_SOR_INPUT_CONTROL_HDMI_SRC_SELECT_DISPLAYB);

	dispclk_div_8_2 = clk_get_rate(hdmi->sor->sor_clk) / 1000000 * 4;
	tegra_sor_writel(sor, NV_SOR_REFCLK,
			NV_SOR_REFCLK_DIV_INT(dispclk_div_8_2 >> 2) |
			NV_SOR_REFCLK_DIV_FRAC(dispclk_div_8_2));

	/*
	 * The rekey register and corresponding eq want to operate
	 * on "-2" of the desired rekey value
	 */
	rekey = NV_SOR_HDMI_CTRL_REKEY_DEFAULT - 2;
	hblank = dc->mode.h_sync_width + dc->mode.h_back_porch +
			dc->mode.h_front_porch;
	max_ac = (hblank - rekey - 18) / 32;

	val = 0;
	val |= hdmi->dvi ? 0x0 : NV_SOR_HDMI_CTRL_ENABLE;
	val |= NV_SOR_HDMI_CTRL_REKEY(rekey);
	val |= NV_SOR_HDMI_CTRL_MAX_AC_PACKET(max_ac);
	val |= NV_SOR_HDMI_CTRL_AUDIO_LAYOUT_SELECT;
	tegra_sor_writel(sor, NV_SOR_HDMI_CTRL, val);

#ifndef CONFIG_TEGRA_NVDISPLAY
	tegra_dc_writel(dc, 0x180, DC_DISP_H_PULSE2_CONTROL);
	h_pulse_start = dc->mode.h_ref_to_sync +
					dc->mode.h_sync_width +
					dc->mode.h_back_porch - 10;
	h_pulse_end = h_pulse_start + 8;
	tegra_dc_writel(dc, PULSE_START(h_pulse_start) | PULSE_END(h_pulse_end),
		  DC_DISP_H_PULSE2_POSITION_A);

	val = tegra_dc_readl(dc, DC_DISP_DISP_SIGNAL_OPTIONS0);
	val |= H_PULSE_2_ENABLE;
	tegra_dc_writel(dc, val, DC_DISP_DISP_SIGNAL_OPTIONS0);
#endif
}

void tegra_hdmi_infoframe_pkt_write(struct tegra_hdmi *hdmi,
						u32 header_reg, u8 pkt_type,
						u8 pkt_vs, u8 pkt_len,
						void *reg_payload,
						u32 reg_payload_len,
						bool sw_checksum)
{
	struct tegra_dc_sor_data *sor = hdmi->sor;
	u32 val;
	u32 *data = reg_payload;
	u32 data_reg = header_reg + 1;

	val = NV_SOR_HDMI_INFOFRAME_HEADER_TYPE(pkt_type) |
		NV_SOR_HDMI_INFOFRAME_HEADER_VERSION(pkt_vs) |
		NV_SOR_HDMI_INFOFRAME_HEADER_LEN(pkt_len);
	tegra_sor_writel(sor, header_reg, val);

	if (sw_checksum) {
		u8 checksum = pkt_type + pkt_vs + pkt_len;

		for (val = 1; val <= pkt_len; val++)
			checksum += ((u8 *)reg_payload)[val];

		/* The first byte of the payload must always be the checksum
		 * that we are going to calculate in SW */
		((u8 *)reg_payload)[0] = (256 - checksum);
	}

	for (val = 0; val < reg_payload_len; val += 4, data_reg++, data++)
		tegra_sor_writel(sor, data_reg, *data);
}

u32 tegra_hdmi_get_cea_modedb_size(struct tegra_hdmi *hdmi)
{
	if (!tegra_hdmi_is_connected(hdmi) || !hdmi->mon_spec_valid)
		return 0;

	return (hdmi->mon_spec.misc & FB_MISC_HDMI_FORUM) ?
		CEA_861_F_MODEDB_SIZE : CEA_861_D_MODEDB_SIZE;
}

static void tegra_hdmi_get_cea_fb_videomode(struct fb_videomode *m,
						struct tegra_hdmi *hdmi)
{
	struct tegra_dc *dc = hdmi->dc;
	struct tegra_dc_mode dc_mode;
	int yuv_flag;

	memcpy(&dc_mode, &dc->mode, sizeof(dc->mode));

	/* get CEA video timings */
	yuv_flag = dc_mode.vmode & FB_VMODE_YUV_MASK;
	if (yuv_flag == (FB_VMODE_Y420      | FB_VMODE_Y24) ||
	    yuv_flag == (FB_VMODE_Y420_ONLY | FB_VMODE_Y24)) {
		dc_mode.h_back_porch *= 2;
		dc_mode.h_front_porch *= 2;
		dc_mode.h_sync_width *= 2;
		dc_mode.h_active *= 2;
		dc_mode.pclk *= 2;
	} else if (yuv_flag == (FB_VMODE_Y420 | FB_VMODE_Y30)) {
		dc_mode.h_back_porch = (dc_mode.h_back_porch * 8) / 5;
		dc_mode.h_front_porch = (dc_mode.h_front_porch * 8) / 5;
		dc_mode.h_sync_width = (dc_mode.h_sync_width * 8) / 5;
		dc_mode.h_active = (dc_mode.h_active * 8) / 5;
		dc_mode.pclk = (dc_mode.pclk / 5) * 8;
	}

	tegra_dc_to_fb_videomode(m, &dc_mode);

	/* only interlaced required for VIC identification */
	m->vmode &= FB_VMODE_INTERLACED;
}

__maybe_unused
static int tegra_hdmi_find_cea_vic(struct tegra_hdmi *hdmi)
{
	struct fb_videomode m;
	struct tegra_dc *dc = hdmi->dc;
	unsigned i;
	unsigned best = 0;
	u32 modedb_size = tegra_hdmi_get_cea_modedb_size(hdmi);

	if (dc->initialized) {
		u32 vic = tegra_sor_readl(hdmi->sor,
			NV_SOR_HDMI_AVI_INFOFRAME_SUBPACK0_HIGH) & 0xff;
		if (!vic)
			dev_warn(&dc->ndev->dev, "hdmi: BL set VIC 0\n");
		return vic;
	}

	tegra_hdmi_get_cea_fb_videomode(&m, hdmi);

	m.flag &= ~FB_FLAG_RATIO_MASK;
	m.flag |= tegra_dc_get_aspect_ratio(dc);

	for (i = 1; i < modedb_size; i++) {
		const struct fb_videomode *curr = &cea_modes[i];

		if (!fb_mode_is_equal_tolerance(curr, &m,
			FB_MODE_TOLERANCE_DEFAULT))
			continue;

		if (!best)
			best = i;
		/* if either flag is set, then match is required */
		if (curr->flag &
			(FB_FLAG_RATIO_4_3 | FB_FLAG_RATIO_16_9 |
			FB_FLAG_RATIO_64_27 | FB_FLAG_RATIO_256_135)) {
			if (m.flag & curr->flag & FB_FLAG_RATIO_4_3)
				best = i;
			else if (m.flag & curr->flag & FB_FLAG_RATIO_16_9)
				best = i;
			else if (m.flag & curr->flag & FB_FLAG_RATIO_64_27)
				best = i;
			else if (m.flag & curr->flag & FB_FLAG_RATIO_256_135)
				best = i;
		} else {
			best = i;
		}
	}
	return best;
}

static u32 tegra_hdmi_get_aspect_ratio(struct tegra_hdmi *hdmi)
{
	u32 aspect_ratio;

	switch (hdmi->dc->mode.avi_m) {
	case TEGRA_DC_MODE_AVI_M_4_3:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_4_3;
		break;
	case TEGRA_DC_MODE_AVI_M_16_9:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_16_9;
		break;
	/*
	 * no avi_m field for picture aspect ratio 64:27 and 256:135.
	 * sink detects via VIC, avi_m is 0.
	 */
	case TEGRA_DC_MODE_AVI_M_64_27: /* fall through */
	case TEGRA_DC_MODE_AVI_M_256_135: /* fall through */
	default:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_NO_DATA;
	}

	/* For seamless HDMI, read aspect ratio parameters from bootloader
	 * set AVI Infoframe parameters
	 */
	if ((aspect_ratio == HDMI_AVI_ASPECT_RATIO_NO_DATA) &&
					(hdmi->dc->initialized)) {
		u32 temp = 0;
		temp = tegra_sor_readl(hdmi->sor,
			NV_SOR_HDMI_AVI_INFOFRAME_SUBPACK0_LOW);
		temp = (temp >> 20) & 0x3;
		switch (temp) {
		case 1:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_4_3;
		break;
		case 2:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_16_9;
		break;
		default:
		aspect_ratio = HDMI_AVI_ASPECT_RATIO_NO_DATA;
		}
	}
	return aspect_ratio;
}

static u32 tegra_hdmi_get_rgb_ycc(struct tegra_hdmi *hdmi)
{
	int yuv_flag = hdmi->dc->mode.vmode & FB_VMODE_YUV_MASK;

	/*
	 * For seamless HDMI, read YUV flag parameters from bootloader
	 * set AVI Infoframe parameters
	 */
	if (hdmi->dc->initialized) {
		u32 temp = 0;
		temp = tegra_sor_readl(hdmi->sor,
			NV_SOR_HDMI_AVI_INFOFRAME_SUBPACK0_LOW);
		temp = (temp >> 12) & 0x3;
		switch (temp) {
		case HDMI_AVI_RGB:
			return HDMI_AVI_RGB;
		case HDMI_AVI_YCC_420:
			return HDMI_AVI_YCC_420;
		default:
			dev_warn(&hdmi->dc->ndev->dev, "hdmi: BL didn't set RGB/YUV indicator flag\n");
			break;
		}
	}

	if (yuv_flag & (FB_VMODE_Y420 | FB_VMODE_Y420_ONLY))
		return HDMI_AVI_YCC_420;
	else if (yuv_flag & FB_VMODE_Y422)
		return HDMI_AVI_YCC_422;
	else if (yuv_flag & FB_VMODE_Y444)
		return HDMI_AVI_YCC_444;

	return HDMI_AVI_RGB;
}

static bool tegra_hdmi_is_ex_colorimetry(struct tegra_hdmi *hdmi)
{
	return !!(hdmi->dc->mode.vmode & FB_VMODE_EC_ENABLE);
}

static u32 tegra_hdmi_get_ex_colorimetry(struct tegra_hdmi *hdmi)
{
	u32 vmode = hdmi->dc->mode.vmode;

	return tegra_hdmi_is_ex_colorimetry(hdmi) ?
		((vmode & FB_VMODE_EC_MASK) >> FB_VMODE_EC_SHIFT) :
		HDMI_AVI_EXT_COLORIMETRY_INVALID;
}

static u32 tegra_hdmi_get_rgb_quant(struct tegra_hdmi *hdmi)
{
	u32 vmode = hdmi->dc->mode.vmode;

	if (tegra_edid_get_quant_cap(hdmi->edid) & FB_CAP_RGB_QUANT_SELECTABLE)
		return vmode & FB_VMODE_LIMITED_RANGE ?
			HDMI_AVI_RGB_QUANT_LIMITED : HDMI_AVI_RGB_QUANT_FULL;
	else
		/*
		 * The safest way to break the HDMI spec when forcing full range
		 * on a limited system: send full data with the QUANT_DEFAULT
		 * */
		return HDMI_AVI_RGB_QUANT_DEFAULT;
}

static u32 tegra_hdmi_get_ycc_quant(struct tegra_hdmi *hdmi)
{
	u32 vmode = hdmi->dc->mode.vmode;

	if (tegra_edid_get_quant_cap(hdmi->edid) & FB_CAP_YUV_QUANT_SELECTABLE)
		return vmode & FB_VMODE_LIMITED_RANGE ?
			HDMI_AVI_YCC_QUANT_LIMITED : HDMI_AVI_YCC_QUANT_FULL;
	else
		/*
		 * The safest way to break the HDMI spec when forcing full range
		 * on a limited system: send full data with the QUANT_DEFAULT
		 * */
		return HDMI_AVI_YCC_QUANT_NONE;
}

static void tegra_hdmi_avi_infoframe_update(struct tegra_hdmi *hdmi)
{
	struct hdmi_avi_infoframe *avi = &hdmi->avi;

	memset(&hdmi->avi, 0, sizeof(hdmi->avi));

	if (tegra_platform_is_linsim())
		return;

	avi->scan = HDMI_AVI_UNDERSCAN;
	avi->bar_valid = HDMI_AVI_BAR_INVALID;
	avi->act_fmt_valid = HDMI_AVI_ACTIVE_FORMAT_INVALID;
	avi->rgb_ycc = tegra_hdmi_get_rgb_ycc(hdmi);

	avi->act_format = HDMI_AVI_ACTIVE_FORMAT_SAME;
	avi->aspect_ratio = tegra_hdmi_get_aspect_ratio(hdmi);
	avi->colorimetry = tegra_hdmi_is_ex_colorimetry(hdmi) ?
			HDMI_AVI_COLORIMETRY_EXTENDED_VALID :
			HDMI_AVI_COLORIMETRY_DEFAULT;

	avi->scaling = HDMI_AVI_SCALING_UNKNOWN;
	avi->rgb_quant = tegra_hdmi_get_rgb_quant(hdmi);
	avi->ext_colorimetry = tegra_hdmi_get_ex_colorimetry(hdmi);
	avi->it_content = HDMI_AVI_IT_CONTENT_FALSE;

	/* set correct vic if video format is cea defined else set 0 */
	avi->video_format = tegra_hdmi_find_cea_vic(hdmi);

	avi->pix_rep = HDMI_AVI_NO_PIX_REPEAT;
	avi->it_content_type = HDMI_AVI_IT_CONTENT_NONE;
	avi->ycc_quant = tegra_hdmi_get_ycc_quant(hdmi);

	avi->top_bar_end_line_low_byte = 0;
	avi->top_bar_end_line_high_byte = 0;

	avi->bot_bar_start_line_low_byte = 0;
	avi->bot_bar_start_line_high_byte = 0;

	avi->left_bar_end_pixel_low_byte = 0;
	avi->left_bar_end_pixel_high_byte = 0;

	avi->right_bar_start_pixel_low_byte = 0;
	avi->right_bar_start_pixel_high_byte = 0;
}

static void tegra_hdmi_avi_infoframe(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_sor_data *sor = hdmi->sor;

	if (hdmi->dvi)
		return;

	if (tegra_platform_is_linsim())
		return;

	/* disable avi infoframe before configuring except for seamless case */
	if (!hdmi->dc->initialized)
		tegra_sor_writel(sor, NV_SOR_HDMI_AVI_INFOFRAME_CTRL, 0);

	tegra_hdmi_avi_infoframe_update(hdmi);

	tegra_hdmi_infoframe_pkt_write(hdmi, NV_SOR_HDMI_AVI_INFOFRAME_HEADER,
					HDMI_INFOFRAME_TYPE_AVI,
					HDMI_INFOFRAME_VS_AVI,
					HDMI_INFOFRAME_LEN_AVI,
					&hdmi->avi, sizeof(hdmi->avi),
					false);

	/* Send infoframe every frame, checksum hw generated */
	tegra_sor_writel(sor, NV_SOR_HDMI_AVI_INFOFRAME_CTRL,
		NV_SOR_HDMI_AVI_INFOFRAME_CTRL_ENABLE_YES |
		NV_SOR_HDMI_AVI_INFOFRAME_CTRL_OTHER_DISABLE |
		NV_SOR_HDMI_AVI_INFOFRAME_CTRL_SINGLE_DISABLE |
		NV_SOR_HDMI_AVI_INFOFRAME_CTRL_CHECKSUM_ENABLE);
}

static int tegra_hdmi_get_extended_vic(const struct tegra_dc_mode *mode)
{
	struct fb_videomode m;
	struct tegra_dc_mode mode_fixed;
	unsigned i;

	mode_fixed = *mode;

	if (mode_fixed.vmode & FB_VMODE_1000DIV1001) {
		mode_fixed.pclk = (u64)mode_fixed.pclk * 1001 / 1000;
		mode_fixed.vmode &= ~FB_VMODE_1000DIV1001;
	}

	tegra_dc_to_fb_videomode(&m, &mode_fixed);

	/* only interlaced required for VIC identification */
	m.vmode &= FB_VMODE_INTERLACED;

	for (i = 1; i < HDMI_EXT_MODEDB_SIZE; i++) {
		const struct fb_videomode *curr = &hdmi_ext_modes[i];

		if (fb_mode_is_equal_tolerance(curr, &m,
			FB_MODE_TOLERANCE_DEFAULT))
			return i;
	}
	return 0;
}

static void tegra_hdmi_vendor_infoframe_update(struct tegra_hdmi *hdmi)
{
	struct hdmi_vendor_infoframe *vsi = &hdmi->vsi;
	u8 extended_vic;

	memset(&hdmi->vsi, 0, sizeof(hdmi->vsi));

	vsi->oui = HDMI_LICENSING_LLC_OUI;

	extended_vic = tegra_hdmi_get_extended_vic(&hdmi->dc->mode);
	if (extended_vic) {
		vsi->video_format =
			HDMI_VENDOR_VIDEO_FORMAT_EXTENDED;
		vsi->extended_vic = extended_vic;
	}
}

static void tegra_hdmi_vendor_infoframe(struct tegra_hdmi *hdmi)
{
/* hdmi licensing, LLC vsi playload len as per hdmi1.4b  */
#define HDMI_INFOFRAME_LEN_VENDOR_LLC	(6)

	struct tegra_dc_sor_data *sor = hdmi->sor;

	if (hdmi->dvi)
		return;

	/* disable vsi infoframe before configuring */
	tegra_sor_writel(sor, NV_SOR_HDMI_VSI_INFOFRAME_CTRL, 0);

	tegra_hdmi_vendor_infoframe_update(hdmi);

	tegra_hdmi_infoframe_pkt_write(hdmi, NV_SOR_HDMI_VSI_INFOFRAME_HEADER,
					HDMI_INFOFRAME_TYPE_VENDOR,
					HDMI_INFOFRAME_VS_VENDOR,
					HDMI_INFOFRAME_LEN_VENDOR_LLC,
					&hdmi->vsi, sizeof(hdmi->vsi),
					false);

	/* Send infoframe every frame, checksum hw generated */
	tegra_sor_writel(sor, NV_SOR_HDMI_VSI_INFOFRAME_CTRL,
		NV_SOR_HDMI_VSI_INFOFRAME_CTRL_ENABLE_YES |
		NV_SOR_HDMI_VSI_INFOFRAME_CTRL_OTHER_DISABLE |
		NV_SOR_HDMI_VSI_INFOFRAME_CTRL_SINGLE_DISABLE |
		NV_SOR_HDMI_VSI_INFOFRAME_CTRL_CHECKSUM_ENABLE);

#undef HDMI_INFOFRAME_LEN_VENDOR_LLC
}

static void tegra_hdmi_hdr_infoframe_update(struct tegra_hdmi *hdmi)
{
	struct hdmi_hdr_infoframe *hdr = &hdmi->hdr;

	memset(&hdmi->hdr, 0, sizeof(hdmi->hdr));

	if (tegra_platform_is_linsim())
		return;

	hdr->eotf = hdmi->dc->hdr.eotf;
	hdr->static_metadata_id = hdmi->dc->hdr.static_metadata_id;

	/* PB3-14 : Group 1 : Static Metadata*/
	hdr->display_primaries_x_0_lsb = hdmi->dc->hdr.static_metadata[0];
	hdr->display_primaries_x_0_msb = hdmi->dc->hdr.static_metadata[1];
	hdr->display_primaries_y_0_lsb = hdmi->dc->hdr.static_metadata[2];
	hdr->display_primaries_y_0_msb = hdmi->dc->hdr.static_metadata[3];
	hdr->display_primaries_x_1_lsb = hdmi->dc->hdr.static_metadata[4];
	hdr->display_primaries_x_1_msb = hdmi->dc->hdr.static_metadata[5];
	hdr->display_primaries_y_1_lsb = hdmi->dc->hdr.static_metadata[6];
	hdr->display_primaries_y_1_msb = hdmi->dc->hdr.static_metadata[7];
	hdr->display_primaries_x_2_lsb = hdmi->dc->hdr.static_metadata[8];
	hdr->display_primaries_x_2_msb = hdmi->dc->hdr.static_metadata[9];
	hdr->display_primaries_y_2_lsb = hdmi->dc->hdr.static_metadata[10];
	hdr->display_primaries_y_2_msb = hdmi->dc->hdr.static_metadata[11];

	/* PB15-18 : Group 2 : Static Metadata*/
	hdr->white_point_x_lsb = hdmi->dc->hdr.static_metadata[12];
	hdr->white_point_x_msb = hdmi->dc->hdr.static_metadata[13];
	hdr->white_point_y_lsb = hdmi->dc->hdr.static_metadata[14];
	hdr->white_point_y_msb = hdmi->dc->hdr.static_metadata[15];

	/* PB19-20 : Group 3 : Static Metadata*/
	hdr->max_display_mastering_luminance_lsb =
					hdmi->dc->hdr.static_metadata[16];
	hdr->max_display_mastering_luminance_msb =
					hdmi->dc->hdr.static_metadata[17];

	/* PB21-22 : Group 4 : Static Metadata*/
	hdr->min_display_mastering_luminance_lsb =
					hdmi->dc->hdr.static_metadata[18];
	hdr->min_display_mastering_luminance_msb =
					hdmi->dc->hdr.static_metadata[19];

	/* PB23-24 : Group 5 : Static Metadata*/
	hdr->max_content_light_level_lsb = hdmi->dc->hdr.static_metadata[20];
	hdr->max_content_light_level_msb = hdmi->dc->hdr.static_metadata[21];

	/* PB25-26 : Group 6 : Static Metadata*/
	hdr->max_frame_avg_light_level_lsb = hdmi->dc->hdr.static_metadata[22];
	hdr->min_frame_avg_light_level_msb = hdmi->dc->hdr.static_metadata[23];

	return;
}

static void tegra_hdmi_hdr_infoframe(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_sor_data *sor = hdmi->sor;

	/* set_bits = contains all the bits to be set
	 * for NV_SOR_HDMI_GENERIC_CTRL reg */
	u32 set_bits = (NV_SOR_HDMI_GENERIC_CTRL_ENABLE_YES |
			NV_SOR_HDMI_GENERIC_CTRL_AUDIO_ENABLE);

	/* disable generic infoframe before configuring */
	tegra_sor_writel(sor, NV_SOR_HDMI_GENERIC_CTRL, 0);

	tegra_hdmi_hdr_infoframe_update(hdmi);

	tegra_hdmi_infoframe_pkt_write(hdmi, NV_SOR_HDMI_GENERIC_HEADER,
					HDMI_INFOFRAME_TYPE_HDR,
					HDMI_INFOFRAME_VS_HDR,
					HDMI_INFOFRAME_LEN_HDR,
					&hdmi->hdr, sizeof(hdmi->hdr),
					true);

	/* set the required bits in NV_SOR_HDMI_GENERIC_CTRL*/
	tegra_sor_writel(sor, NV_SOR_HDMI_GENERIC_CTRL, set_bits);

	return;
}

__maybe_unused
static int tegra_hdmi_scdc_read(struct tegra_hdmi *hdmi,
					u8 offset_data[][2], u32 n_entries)
{
	u32 i;
	struct i2c_msg msg[] = {
		{
			.addr = 0x54,
			.len = 1,
			.buf = NULL,
		},
		{
			.addr = 0x54,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = NULL,
		},
	};

	_tegra_hdmi_ddc_enable(hdmi);

	for (i = 0; i < n_entries; i++) {
		msg[0].buf = offset_data[i];
		msg[1].buf = &offset_data[i][1];
		tegra_hdmi_scdc_i2c_xfer(hdmi->dc, msg, ARRAY_SIZE(msg));
	}

	_tegra_hdmi_ddc_disable(hdmi);

	return 0;
}

static int tegra_hdmi_scdc_write(struct tegra_hdmi *hdmi,
					u8 offset_data[][2], u32 n_entries)
{
	u32 i;
	struct i2c_msg msg[] = {
		{
			.addr = 0x54,
			.len = 2,
			.buf = NULL,
		},
	};

	_tegra_hdmi_ddc_enable(hdmi);

	for (i = 0; i < n_entries; i++) {
		msg[0].buf = offset_data[i];
		tegra_hdmi_scdc_i2c_xfer(hdmi->dc, msg, ARRAY_SIZE(msg));
	}

	_tegra_hdmi_ddc_disable(hdmi);

	return 0;
}

static int tegra_hdmi_v2_x_mon_config(struct tegra_hdmi *hdmi, bool enable)
{
	u8 tmds_config_en[][2] = {
		{
			HDMI_SCDC_TMDS_CONFIG_OFFSET,
			(HDMI_SCDC_TMDS_CONFIG_BIT_CLK_RATIO_40 |
			HDMI_SCDC_TMDS_CONFIG_SCRAMBLING_EN)
		},
	};
	u8 tmds_config_dis[][2] = {
		{
			HDMI_SCDC_TMDS_CONFIG_OFFSET,
			0
		},
	};

	if (hdmi->dc->vedid)
		goto skip_scdc_i2c;

	tegra_hdmi_scdc_write(hdmi,
			enable ? tmds_config_en : tmds_config_dis,
			ARRAY_SIZE(tmds_config_en));

skip_scdc_i2c:
	return 0;
}

static void tegra_hdmi_v2_x_host_config(struct tegra_hdmi *hdmi, bool enable)
{
	u32 val = NV_SOR_HDMI2_CTRL_SCRAMBLE_ENABLE |
		NV_SOR_HDMI2_CTRL_CLK_MODE_DIV_BY_4;

	tegra_sor_write_field(hdmi->sor, NV_SOR_HDMI2_CTRL,
			NV_SOR_HDMI2_CTRL_SCRAMBLE_ENABLE |
			NV_SOR_HDMI2_CTRL_CLK_MODE_DIV_BY_4,
			enable ? val : 0);
}

static int _tegra_hdmi_v2_x_config(struct tegra_hdmi *hdmi)
{
#define SCDC_STABILIZATION_DELAY_MS (20)

	/* disable hdmi2.x config on host and monitor only
	 * if bootloader didn't initialize hdmi
	 */
	if (!hdmi->dc->initialized) {
		tegra_hdmi_v2_x_mon_config(hdmi, false);
		tegra_hdmi_v2_x_host_config(hdmi, false);
	}

	/* enable hdmi2.x config on host and monitor */
	tegra_hdmi_v2_x_mon_config(hdmi, true);
	msleep(SCDC_STABILIZATION_DELAY_MS);

	tegra_hdmi_v2_x_host_config(hdmi, true);

	return 0;
#undef SCDC_STABILIZATION_DELAY_MS
}

static int tegra_hdmi_v2_x_config(struct tegra_hdmi *hdmi)
{
	_tegra_hdmi_v2_x_config(hdmi);

	return 0;
}

static void tegra_hdmi_scdc_worker(struct work_struct *work)
{
	struct tegra_hdmi *hdmi = container_of(to_delayed_work(work),
				struct tegra_hdmi, scdc_work);
	u8 rd_status_flags[][2] = {
		{HDMI_SCDC_STATUS_FLAGS, 0x0}
	};

	if (!hdmi->enabled || hdmi->dc->mode.pclk <= 340000000)
		return;

	if (hdmi->dc->vedid)
		goto skip_scdc_i2c;

	if (!tegra_edid_is_scdc_present(hdmi->dc->edid))
		return;

	tegra_hdmi_scdc_read(hdmi, rd_status_flags, ARRAY_SIZE(rd_status_flags));
	if (!rd_status_flags[0][1]  && (hdmi->dc->mode.pclk > 340000000)) {
		dev_info(&hdmi->dc->ndev->dev, "hdmi: scdc scrambling status is reset, "
						"trying to reconfigure.\n");
		_tegra_hdmi_v2_x_config(hdmi);
	}

skip_scdc_i2c:
	/* reschedule the worker */
	cancel_delayed_work(&hdmi->scdc_work);
	schedule_delayed_work(&hdmi->scdc_work,
			msecs_to_jiffies(HDMI_SCDC_MONITOR_TIMEOUT_MS));
}

static void _tegra_hdmi_clock_enable(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_sor_data *sor = hdmi->sor;
	tegra_disp_clk_prepare_enable(sor->safe_clk);

	tegra_hdmi_config_clk(hdmi, TEGRA_HDMI_SAFE_CLK);

	/* Setting various clock to figure out whether bpmp is able to
	 * handle this
	 */
	/* Set PLLD2 to preset clock value*/
	/* Set PLLD2 to SOR_REF_CLK*/
	tegra_sor_clk_enable(sor);
}

static void _tegra_hdmi_clock_disable(struct tegra_hdmi *hdmi)
{
	struct tegra_dc_sor_data *sor = hdmi->sor;
	tegra_sor_clk_disable(sor);
}

void tegra_hdmi_get(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	if (atomic_inc_return(&hdmi->clock_refcount) == 1)
		_tegra_hdmi_clock_enable(hdmi);
}

void tegra_hdmi_put(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	if (WARN_ONCE(atomic_read(&hdmi->clock_refcount) <= 0,
		"hdmi: clock refcount imbalance"))
		return;
	if (atomic_dec_return(&hdmi->clock_refcount) == 0)
		_tegra_hdmi_clock_disable(hdmi);
}

static inline u32 tegra_hdmi_get_bpp(struct tegra_hdmi *hdmi)
{
	int yuv_flag = hdmi->dc->mode.vmode & FB_VMODE_YUV_MASK;

	/* According to HDMI 1.4b spec:
	 *
	 * YCbCr 4:2:2 is also 36-bit mode but does not require the
	 * further use of the Deep Color modes described in ...
	 *
	 * Setting this to zero prevents sending phase info */
	if (yuv_flag == (FB_VMODE_Y422 | FB_VMODE_Y36))
		return 0;
	else if (yuv_flag & FB_VMODE_Y24)
		return 24;
	else if (yuv_flag & FB_VMODE_Y30)
		return 30;
	else if (yuv_flag & FB_VMODE_Y36)
		return 36;
	else if (yuv_flag & FB_VMODE_Y48)
		return 48;
	else
		return 0;
}

static u32 tegra_hdmi_gcp_color_depth(struct tegra_hdmi *hdmi)
{
	u32 gcp_cd = 0;

	switch (tegra_hdmi_get_bpp(hdmi)) {
	case 0:
		gcp_cd = TEGRA_HDMI_BPP_UNKNOWN;
		break;
	case 24:
		gcp_cd = TEGRA_HDMI_BPP_24;
		break;
	case 30:
		gcp_cd = TEGRA_HDMI_BPP_30;
		break;
	case 36:
		gcp_cd = TEGRA_HDMI_BPP_36;
		break;
	case 48:
		gcp_cd = TEGRA_HDMI_BPP_48;
		break;
	default:
		dev_WARN(&hdmi->dc->ndev->dev,
			"hdmi: unknown gcp color depth\n");
	};

	return gcp_cd;
}

/* return packing phase of last pixel in preceding video data period */
static u32 tegra_hdmi_gcp_packing_phase(struct tegra_hdmi *hdmi)
{
	int yuv_flag = hdmi->dc->mode.vmode & FB_VMODE_YUV_MASK;

	if (!tegra_hdmi_gcp_color_depth(hdmi))
		return 0;

	if ((IS_RGB(yuv_flag) && (yuv_flag == FB_VMODE_Y36)) ||
			(yuv_flag == (FB_VMODE_Y444 | FB_VMODE_Y36)))
		return 2;
	else
		return 0;
}

static bool tegra_hdmi_gcp_default_phase_en(struct tegra_hdmi *hdmi)
{
	int yuv_flag = hdmi->dc->mode.vmode & FB_VMODE_YUV_MASK;

	if (!tegra_hdmi_gcp_color_depth(hdmi))
		return false;

	if ((yuv_flag == (FB_VMODE_Y420 | FB_VMODE_Y30)) ||
			(yuv_flag == (FB_VMODE_Y444 | FB_VMODE_Y36)) ||
			(IS_RGB(yuv_flag) && (yuv_flag == FB_VMODE_Y36)))
		return true;
	else
		return false;
}

/* general control packet */
static void tegra_hdmi_gcp(struct tegra_hdmi *hdmi)
{
#define GCP_SB1_PP_SHIFT 4

	struct tegra_dc_sor_data *sor = hdmi->sor;
	u8 sb1, sb2;

	/* disable gcp before configuring */
	tegra_sor_writel(sor, NV_SOR_HDMI_GCP_CTRL, 0);

	sb1 = tegra_hdmi_gcp_packing_phase(hdmi) << GCP_SB1_PP_SHIFT |
		tegra_hdmi_gcp_color_depth(hdmi);
	sb2 = !!tegra_hdmi_gcp_default_phase_en(hdmi);
	tegra_sor_writel(sor, NV_SOR_HDMI_GCP_SUBPACK(0),
			sb1 << NV_SOR_HDMI_GCP_SUBPACK_SB1_SHIFT |
			sb2 << NV_SOR_HDMI_GCP_SUBPACK_SB2_SHIFT);

	/* Send gcp every frame */
	tegra_sor_writel(sor, NV_SOR_HDMI_GCP_CTRL,
			NV_SOR_HDMI_GCP_CTRL_ENABLE |
			NV_SOR_HDMI_GCP_CTRL_OTHER_DIS |
			NV_SOR_HDMI_GCP_CTRL_SINGLE_DIS);

#undef GCP_SB1_PP_SHIFT
}

static int tegra_hdmi_controller_enable(struct tegra_hdmi *hdmi)
{
	struct tegra_dc *dc = hdmi->dc;
	struct tegra_dc_sor_data *sor = hdmi->sor;

	tegra_dc_get(dc);
	tegra_hdmi_get(dc);

	tegra_sor_hdmi_pad_power_up(sor);

	tegra_sor_power_lanes(sor, 4, true);

	tegra_dc_sor_set_internal_panel(sor, false);
	tegra_hdmi_config(hdmi);
	tegra_hdmi_avi_infoframe(hdmi);
	tegra_hdmi_vendor_infoframe(hdmi);

	tegra_sor_pad_cal_power(sor, true);
	tegra_hdmi_config_tmds(hdmi);
	tegra_sor_pad_cal_power(sor, false);
	tegra_hdmi_config_clk(hdmi, TEGRA_HDMI_BRICK_CLK);
	tegra_dc_sor_attach(sor);
	/* enable hdcp */
#ifdef CONFIG_HDCP
	tegra_nvhdcp_set_plug(hdmi->nvhdcp, true);
#endif

	tegra_dpaux_prod_set_for_hdmi(hdmi->dc);

#ifndef CONFIG_TEGRA_NVDISPLAY
	tegra_dc_setup_clk(dc, dc->clk);
	tegra_dc_hdmi_setup_clk(dc, hdmi->sor->sor_clk);
#endif
	tegra_hdmi_config(hdmi);

	tegra_sor_config_xbar(hdmi->sor);

	/* IS THE POWER ENABLE AFTER ATTACH IS VALID*/
	/* TODO: Confirm sequence with HW */
	tegra_sor_writel(sor,  NV_SOR_SEQ_INST(0), 0x8080);
	tegra_sor_writel(sor,  NV_SOR_PWR, 0x80000001);

	if (hdmi->dc->mode.pclk > 340000000) {
		tegra_hdmi_v2_x_config(hdmi);
		schedule_delayed_work(&hdmi->scdc_work,
			msecs_to_jiffies(HDMI_SCDC_MONITOR_TIMEOUT_MS));
	}

	tegra_hdmi_gcp(hdmi);

	tegra_dc_put(dc);
	return 0;
}

static void tegra_dc_hdmi_enable(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	if (hdmi->enabled)
		return;
	if (NULL != hdmi->out_ops && NULL != hdmi->out_ops->enable)
		hdmi->out_ops->enable(hdmi);
	tegra_hdmi_controller_enable(hdmi);

	hdmi->enabled = true;

	tegra_hda_set_data(dc, hdmi, SINK_HDMI);
#ifdef CONFIG_SWITCH
	if (!hdmi->dvi)
		switch_set_state(&hdmi->audio_switch, 1);
#endif
}

static inline u32 __maybe_unused
tegra_hdmi_get_shift_clk_div(struct tegra_hdmi *hdmi)
{
	/*
	 * HW does not support deep color yet
	 * always return 0
	 */

	return 0;
}

#ifdef CONFIG_TEGRA_NVDISPLAY
static void tegra_hdmi_config_clk(struct tegra_hdmi *hdmi, u32 clk_type)
{
	if (clk_type == hdmi->clk_type)
		return;

	if (tegra_platform_is_linsim())
		return;

	if (clk_type == TEGRA_HDMI_BRICK_CLK) {

		struct tegra_dc_sor_data *sor = hdmi->sor;
		int val = NV_SOR_CLK_CNTRL_DP_LINK_SPEED_G2_7;

		if (hdmi->dc->mode.pclk > 340000000) {
			long rate = clk_get_rate(sor->sor_clk);

			/*half rate and double vco*/
			val = NV_SOR_CLK_CNTRL_DP_LINK_SPEED_G5_4;
			/* Set Rate to SOR_CLK*/
			clk_set_rate(sor->sor_clk, (rate >> 1));
		}

		val |= NV_SOR_CLK_CNTRL_DP_CLK_SEL_SINGLE_PCLK;
		tegra_sor_writel(hdmi->sor, NV_SOR_CLK_CNTRL, val);
		usleep_range(250, 300); /* sor brick pll stabilization delay */

		clk_set_parent(sor->src_switch_clk, sor->brick_clk);

		hdmi->clk_type = TEGRA_HDMI_BRICK_CLK;
	} else if (clk_type == TEGRA_HDMI_SAFE_CLK) {
		if (!hdmi->dc->initialized) {
			clk_set_parent(hdmi->sor->src_switch_clk,
					hdmi->sor->safe_clk);
			hdmi->clk_type = TEGRA_HDMI_SAFE_CLK;
		}
	} else {
		dev_err(&hdmi->dc->ndev->dev,
				"hdmi: incorrect clk type configured\n");
	}
}
#else
static void tegra_hdmi_config_clk(struct tegra_hdmi *hdmi, u32 clk_type)
{
	if (clk_type == hdmi->clk_type)
		return;

	if (tegra_platform_is_linsim())
		return;

	if (clk_type == TEGRA_HDMI_BRICK_CLK) {
		u32 val;
		struct tegra_dc_sor_data *sor = hdmi->sor;
		int div = hdmi->dc->mode.pclk < 340000000 ? 1 : 2;
		unsigned long rate = clk_get_rate(sor->src_switch_clk);
		unsigned long parent_rate =
			clk_get_rate(clk_get_parent(sor->src_switch_clk));

		/* Set sor divider */
		if (rate != DIV_ROUND_UP(parent_rate, div)) {
			rate = DIV_ROUND_UP(parent_rate, div);
			clk_set_rate(sor->src_switch_clk, rate);
		}

		/* Select brick muxes */
		val = (hdmi->dc->mode.pclk < 340000000) ?
			NV_SOR_CLK_CNTRL_DP_LINK_SPEED_G2_7 :
			NV_SOR_CLK_CNTRL_DP_LINK_SPEED_G5_4;

		val |= NV_SOR_CLK_CNTRL_DP_CLK_SEL_SINGLE_PCLK;
		tegra_sor_writel(hdmi->sor, NV_SOR_CLK_CNTRL, val);
		usleep_range(250, 300); /* sor brick pll stabilization delay */

		/*
		 * Report brick configuration and rate, so that SOR clock tree
		 * is properly updated. No h/w changes by clock api calls below,
		 * just sync s/w state with brick h/w.
		 */
		rate = rate/NV_SOR_HDMI_BRICK_DIV*NV_SOR_HDMI_BRICK_MUL(val);
		if (clk_get_parent(sor->brick_clk) != sor->src_switch_clk)
			clk_set_parent(sor->brick_clk, sor->src_switch_clk);
		clk_set_rate(sor->brick_clk, rate);

#ifdef CONFIG_TEGRA_CORE_DVFS
		/*
		 * Select primary -- HDMI -- DVFS table for SOR clock (if SOR
		 * clock has single DVFS table for all modes, nothing changes).
		 */
		tegra_dvfs_use_alt_freqs_on_clk(sor->sor_clk, false);
#endif

		/* Select sor clock muxes */
		tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 3);

		tegra_dc_writel(hdmi->dc, PIXEL_CLK_DIVIDER_PCD1 |
			SHIFT_CLK_DIVIDER(tegra_hdmi_get_shift_clk_div(hdmi)),
			DC_DISP_DISP_CLOCK_CONTROL);

		hdmi->clk_type = TEGRA_HDMI_BRICK_CLK;
	} else if (clk_type == TEGRA_HDMI_SAFE_CLK) {
		if (!hdmi->dc->initialized) {
			/* Select sor clock muxes */
			tegra_clk_cfg_ex(hdmi->sor->sor_clk,
				TEGRA_CLK_SOR_CLK_SEL, 0);
			hdmi->clk_type = TEGRA_HDMI_SAFE_CLK;
		}
	} else {
		dev_err(&hdmi->dc->ndev->dev, "hdmi: incorrect clk type configured\n");
	}
}
#endif

/* returns exact pixel clock in Hz */
static long tegra_hdmi_get_pclk(struct tegra_dc_mode *mode)
{
	long h_total, v_total;
	long refresh, pclk;
	h_total = mode->h_active + mode->h_front_porch + mode->h_back_porch +
		mode->h_sync_width;
	v_total = mode->v_active + mode->v_front_porch + mode->v_back_porch +
		mode->v_sync_width;
	refresh = tegra_dc_calc_refresh(mode);
	refresh = DIV_ROUND_CLOSEST(refresh, 1000);

	pclk = h_total * v_total * refresh;

	if (mode->vmode & FB_VMODE_1000DIV1001)
		pclk = pclk * 1000 / 1001;

	return pclk;
}

static long tegra_dc_hdmi_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	struct clk *parent_clk;

#ifdef CONFIG_TEGRA_NVDISPLAY
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);
	struct tegra_dc_sor_data *sor = hdmi->sor;
	long parent_clk_rate;
	long rate;
	int yuv_flag = hdmi->dc->mode.vmode & FB_VMODE_YUV_MASK;

	if (!dc->out->parent_clk) {
		dev_err(&dc->ndev->dev,
				"hdmi: failed, no clock name for this node.\n");
		return -EINVAL;
	}

	/* GET THE PARENT  */
	parent_clk = tegra_disp_clk_get(&dc->ndev->dev, dc->out->parent_clk);
	if (IS_ERR_OR_NULL(parent_clk)) {
		dev_err(&dc->ndev->dev, "hdmi: failed to get clock %s\n",
				dc->out->parent_clk);
		return -EINVAL;
	}

	dc->mode.pclk = tegra_hdmi_get_pclk(&dc->mode);

	/* Set rate on PARENT */
	if ((IS_RGB(yuv_flag) && (yuv_flag == FB_VMODE_Y36)) ||
			(yuv_flag == (FB_VMODE_Y444 | FB_VMODE_Y36))) {
		parent_clk_rate = 6 * dc->mode.pclk;
		clk_set_rate(parent_clk, parent_clk_rate);
	} else {
		clk_set_rate(parent_clk, dc->mode.pclk);
	}

	if (clk == dc->clk)
		clk_set_rate(clk, dc->mode.pclk);

	/* Enable safe sor clock */
	tegra_sor_safe_clk_enable(sor);
	/* Set Parent to SOR_CLK*/
	clk_set_parent(sor->sor_clk, parent_clk);

	/* Set Rate to SOR_CLK*/
	if ((IS_RGB(yuv_flag) && (yuv_flag == FB_VMODE_Y36)) ||
			(yuv_flag == (FB_VMODE_Y444 | FB_VMODE_Y36))) {
		rate = dc->mode.pclk;
		rate = rate >> 1;
		rate = rate * 3;
		clk_set_rate(sor->sor_clk, rate);
	} else {
		clk_set_rate(sor->sor_clk, dc->mode.pclk);
	}

	/* Enable SOR_CLK*/
	tegra_sor_clk_enable(sor);

	/* Select the sor_out parent as SAFE_CLK*/
	clk_set_parent(sor->src_switch_clk, sor->safe_clk);
	/* Enable sor_out Clock */
	tegra_disp_clk_prepare_enable(sor->src_switch_clk);
	/* Enable brick Clock */
	tegra_disp_clk_prepare_enable(sor->brick_clk);

	return tegra_dc_pclk_round_rate(dc, dc->mode.pclk);
#else
	parent_clk = clk_get(NULL,
				dc->out->parent_clk ? : "pll_d2");

	dc->mode.pclk = tegra_hdmi_get_pclk(&dc->mode);

	if (IS_ERR_OR_NULL(parent_clk)) {
		dev_err(&dc->ndev->dev, "hdmi: parent clk get failed\n");
		return 0;
	}

	if (!tegra_platform_is_silicon())
		return dc->mode.pclk;

	if (clk == dc->clk) {
		if (clk_get_parent(clk) != parent_clk) {
			if (clk_set_parent(clk, parent_clk)) {
				dev_err(&dc->ndev->dev,
					"hdmi: set dc parent failed\n");
				return 0;
			}
		}
	} else {
		struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);
		struct tegra_dc_sor_data *sor = hdmi->sor;

		if (clk_get_parent(sor->src_switch_clk) != parent_clk) {
			if (clk_set_parent(sor->src_switch_clk, parent_clk)) {
				dev_err(&dc->ndev->dev,
					"hdmi: set src switch parent failed\n");
				return 0;
			}
		}
	}
	if (dc->initialized)
		goto skip_setup;
	if (clk_get_rate(parent_clk) != dc->mode.pclk)
		clk_set_rate(parent_clk, dc->mode.pclk);
skip_setup:
#ifdef CONFIG_TEGRA_CORE_DVFS
	/*
	 * DC clock divider is controlled by DC driver transparently to clock
	 * framework -- hence, direct call to DVFS with target mode rate. SOR
	 * clock rate in clock tree is properly updated, and can be used for
	 * DVFS update.
	 *
	 * TODO: tegra_hdmi_controller_enable() procedure 1st configures SOR
	 * clock via tegra_hdmi_config_clk(), and then calls this function
	 * that may re-lock parent PLL. That needs to be double-checked:
	 * in general re-locking PLL while the downstream module is already
	 * sourced from it is not recommended. If/when the order of enabling
	 * HDMI controller is changed, we can remove direct DVFS call for SOR
	 * (but for DC it should be kept, anyway).
	 */
	if (clk == dc->clk)
		tegra_dvfs_set_rate(clk, dc->mode.pclk);
	else
		tegra_dvfs_set_rate(clk, clk_get_rate(clk));
#endif

	return tegra_dc_pclk_round_rate(dc, dc->mode.pclk);
#endif
}

static void tegra_dc_hdmi_shutdown(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	_tegra_hdmivrr_activate(hdmi, false);
	hdmi->device_shutdown = true;
#ifdef CONFIG_HDCP
	tegra_nvhdcp_shutdown(hdmi->nvhdcp);
#endif

	return;
}

static void tegra_dc_hdmi_disable(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	_tegra_hdmivrr_activate(hdmi, false);

	if (NULL != hdmi->out_ops && NULL != hdmi->out_ops->disable)
		hdmi->out_ops->disable(hdmi);

	hdmi->enabled = false;

#ifdef CONFIG_SWITCH
	switch_set_state(&hdmi->audio_switch, 0);
#endif

	tegra_hdmi_controller_disable(hdmi);
	tegra_hda_reset_data(dc);

	return;
}

static bool tegra_dc_hdmi_detect(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);
	unsigned long delay = msecs_to_jiffies(HDMI_HPD_DEBOUNCE_DELAY_MS);

	if (tegra_platform_is_linsim())
		return true;

	if (dc->out->hotplug_state != TEGRA_HPD_STATE_NORMAL)
		delay = 0;

	cancel_delayed_work(&hdmi->hpd_worker);
	schedule_delayed_work(&hdmi->hpd_worker, delay);
	return tegra_dc_hpd(dc);
}

static void tegra_dc_hdmi_suspend(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	_tegra_hdmivrr_activate(hdmi, false);

	if (hdmi->pdata->hdmi2fpd_bridge_enable)
		hdmi2fpd_suspend(dc);

	if (dc->out->flags & TEGRA_DC_OUT_HOTPLUG_WAKE_LP0) {
		int wake_irq = gpio_to_irq(dc->out->hotplug_gpio);
		int ret;

		ret = enable_irq_wake(wake_irq);
		if (ret < 0) {
			dev_err(&dc->ndev->dev,
			"%s: Couldn't enable HDMI wakeup, irq=%d, error=%d\n",
			__func__, wake_irq, ret);
		}
	}

	atomic_set(&hdmi->suspended, 1);
}

static void tegra_dc_hdmi_resume(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	atomic_set(&hdmi->suspended, 0);

	if (dc->out->flags & TEGRA_DC_OUT_HOTPLUG_WAKE_LP0)
		disable_irq_wake(gpio_to_irq(dc->out->hotplug_gpio));

	if (hdmi->pdata->hdmi2fpd_bridge_enable)
		hdmi2fpd_resume(dc);

	cancel_delayed_work(&hdmi->hpd_worker);

	/* If resume happens with a non-VRR monitor, the HPD
	   worker will correct the mode based on the new EDID */
	_tegra_hdmivrr_activate(hdmi, true);

	schedule_delayed_work(&hdmi->hpd_worker,
				msecs_to_jiffies(HDMI_HPD_DEBOUNCE_DELAY_MS + HDMI_HPD_DROP_TIMEOUT_MS));
}

static int tegra_dc_hdmi_set_hdr(struct tegra_dc *dc)
{
	u16 ret = 0;
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	/* If the sink isn't HDR capable, return */
	ret = tegra_edid_get_ex_hdr_cap(hdmi->edid);
	if (!(ret & FB_CAP_HDR))
		return 0;
	tegra_hdmi_hdr_infoframe(hdmi);

	/*
	 *If hdr is disabled then send HDR infoframe for
	 *2 secs with SDR eotf and then stop
	 */
	if (dc->hdr.enabled)
		return 0;

	schedule_delayed_work(&hdmi->hdr_worker,
		msecs_to_jiffies(HDMI_HDR_INFOFRAME_STOP_TIMEOUT_MS));

	return 0;
}

static void tegra_hdmi_hdr_worker(struct work_struct *work)
{
	struct tegra_hdmi *hdmi = container_of(to_delayed_work(work),
				struct tegra_hdmi, hdr_worker);
	/* TODO:Add null check */

	/* Read the current regsiter value to restore the bits */
	u32 val = tegra_sor_readl(hdmi->sor, NV_SOR_HDMI_GENERIC_CTRL);

	/* Set val to disable generic infoframe */
	val &= ~NV_SOR_HDMI_GENERIC_CTRL_ENABLE_YES;

	tegra_sor_writel(hdmi->sor, NV_SOR_HDMI_GENERIC_CTRL, val);

	return;
}

static int tegra_dc_hdmi_ddc_enable(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);
	_tegra_hdmi_ddc_enable(hdmi);
	return 0;
}

static int tegra_dc_hdmi_ddc_disable(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);
	_tegra_hdmi_ddc_disable(hdmi);
	return 0;
}

static void tegra_dc_hdmi_modeset_notifier(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	tegra_hdmi_get(dc);
	tegra_dc_io_start(dc);

	/* disable hdmi2.x config on host and monitor */
	if (dc->mode.pclk > 340000000) {
		if (tegra_edid_is_scdc_present(dc->edid))
			tegra_hdmi_v2_x_mon_config(hdmi, true);
		tegra_hdmi_v2_x_host_config(hdmi, true);
	} else {
		if (tegra_edid_is_scdc_present(dc->edid))
			tegra_hdmi_v2_x_mon_config(hdmi, false);
		tegra_hdmi_v2_x_host_config(hdmi, false);
	}
#ifdef CONFIG_TEGRA_DC_CMU
	if (!(dc->mode.vmode & FB_VMODE_SET_YUV_MASK))
		_tegra_dc_cmu_enable(dc,
			dc->mode.vmode & FB_VMODE_LIMITED_RANGE);
#endif
	tegra_dc_io_end(dc);
	tegra_hdmi_put(dc);
}

#ifdef CONFIG_DEBUG_FS
/* show current hpd state */
static int tegra_hdmi_hotplug_dbg_show(struct seq_file *m, void *unused)
{
	struct tegra_hdmi *hdmi = m->private;
	struct tegra_dc *dc = hdmi->dc;

	if (WARN_ON(!hdmi || !dc || !dc->out))
		return -EINVAL;

	rmb();
	seq_printf(m, "hdmi hpd state: %d\n", dc->out->hotplug_state);

	return 0;
}

int tegra_hdmi_get_hotplug_state(struct tegra_hdmi *hdmi)
{
	rmb();
	return hdmi->dc->out->hotplug_state;
}

void tegra_hdmi_set_hotplug_state(struct tegra_hdmi *hdmi, int new_hpd_state)
{
	struct tegra_dc *dc = hdmi->dc;
	int hotplug_state;

	rmb();
	hotplug_state = dc->out->hotplug_state;

	if (hotplug_state == TEGRA_HPD_STATE_NORMAL &&
			new_hpd_state != TEGRA_HPD_STATE_NORMAL &&
			dc->hotplug_supported) {
		disable_irq(gpio_to_irq(dc->out->hotplug_gpio));
	} else if (hotplug_state != TEGRA_HPD_STATE_NORMAL &&
			new_hpd_state == TEGRA_HPD_STATE_NORMAL &&
			dc->hotplug_supported) {
		enable_irq(gpio_to_irq(dc->out->hotplug_gpio));
	}

	dc->out->hotplug_state = new_hpd_state;
	wmb();

	/*
	 * sw controlled plug/unplug.
	 * wait for any already executing hpd worker thread.
	 * No debounce delay, schedule immedately
	 */
	cancel_delayed_work_sync(&hdmi->hpd_worker);
	schedule_delayed_work(&hdmi->hpd_worker, 0);
}

/*
 * sw control for hpd.
 * 0 is normal state, hw drives hpd.
 * -1 is force deassert, sw drives hpd.
 * 1 is force assert, sw drives hpd.
 * before releasing to hw, sw must ensure hpd state is normal i.e. 0
 */
static ssize_t tegra_hdmi_hotplug_dbg_write(struct file *file,
					const char __user *addr,
					size_t len, loff_t *pos)
{
	struct seq_file *m = file->private_data;
	struct tegra_hdmi *hdmi = m->private;
	struct tegra_dc *dc = hdmi->dc;
	long new_hpd_state;
	int ret;

	if (WARN_ON(!hdmi || !dc || !dc->out))
		return -EINVAL;

	ret = kstrtol_from_user(addr, len, 10, &new_hpd_state);
	if (ret < 0)
		return ret;

	tegra_hdmi_set_hotplug_state(hdmi, new_hpd_state);

	return len;
}

static int tegra_hdmi_hotplug_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_hdmi_hotplug_dbg_show, inode->i_private);
}

static const struct file_operations tegra_hdmi_hotplug_dbg_ops = {
	.open = tegra_hdmi_hotplug_dbg_open,
	.read = seq_read,
	.write = tegra_hdmi_hotplug_dbg_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int tegra_hdmi_status_dbg_show(struct seq_file *m, void *unused)
{
	struct tegra_hdmi *hdmi = m->private;
	struct tegra_dc *dc;

	if (WARN_ON(!hdmi))
		return -EINVAL;

	dc = hdmi->dc;

	if (WARN_ON(!dc || !dc->out))
		return -EINVAL;

	seq_printf(m, "hotplug state: %d\n", tegra_dc_hpd(dc));
	seq_printf(m, "SCDC present: %d\n",
		tegra_edid_is_scdc_present(hdmi->edid));
	seq_printf(m, "Forum VSDB present: %d\n",
		tegra_edid_is_hfvsdb_present(hdmi->edid));
	seq_printf(m, "YCbCr4:2:0 VDB present: %d\n",
		tegra_edid_is_420db_present(hdmi->edid));

	return 0;
}

static int tegra_hdmi_status_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_hdmi_status_dbg_show,
		inode->i_private);
}

static const struct file_operations tegra_hdmi_status_dbg_ops = {
	.open = tegra_hdmi_status_dbg_open,
	.read = seq_read,
	.release = single_release,
};

static void tegra_hdmi_debugfs_init(struct tegra_hdmi *hdmi)
{
	struct dentry *dir, *ret;

	dir = debugfs_create_dir("tegra_hdmi",  NULL);
	if (IS_ERR_OR_NULL(dir))
		return;

	ret = debugfs_create_file("hotplug", S_IRUGO, dir,
				hdmi, &tegra_hdmi_hotplug_dbg_ops);
	if (IS_ERR_OR_NULL(ret))
		goto fail;
	ret = debugfs_create_file("hdmi_status", S_IRUGO, dir,
				hdmi, &tegra_hdmi_status_dbg_ops);
	if (IS_ERR_OR_NULL(ret))
		goto fail;
	return;
fail:
	debugfs_remove_recursive(dir);
	return;
}
#else
static void tegra_hdmi_debugfs_init(struct tegra_hdmi *hdmi)
{
	return;
}
#endif

static int hdmi_get_index_for_ddc_kobj(struct kobject *kobj)
{
	int result;

	for (result = 0; result < TEGRA_MAX_DC; result++) {
		if (kobj == hdmi_ddc_array[result])
			return result;
	}
	return -1;
}

static ssize_t hdmi_ddc_power_toggle(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int value, index;

	index = hdmi_get_index_for_ddc_kobj(kobj);
	if (0 > index)
		return 0;
	sscanf(buf, "%du", &value);
	tegra_hdmi_ddc_power_toggle(dc_hdmi_array[index], value);
	return count;
}

static ssize_t hdmi_ddc_power_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int index;

	index = hdmi_get_index_for_ddc_kobj(kobj);
	if (0 > index)
		return 0;
	return sprintf(buf, "%d\n", dc_hdmi_array[index]->ddc_refcount);
}

static bool tegra_dc_hdmi_hpd_state(struct tegra_dc *dc)
{
	int sense;
	int level;
	bool hpd;

	if (WARN_ON(!dc || !dc->out))
		return false;

	level = gpio_get_value_cansleep(dc->out->hotplug_gpio);

	sense = dc->out->flags & TEGRA_DC_OUT_HOTPLUG_MASK;

	hpd = (sense == TEGRA_DC_OUT_HOTPLUG_HIGH && level) ||
		(sense == TEGRA_DC_OUT_HOTPLUG_LOW && !level);

	return hpd;
}

static void tegra_dc_hdmi_vrr_enable(struct tegra_dc *dc, bool enable)
{
	struct tegra_vrr *vrr  = dc->out->vrr;

	if (!vrr)
		return;

	if (!(dc->mode.vmode & FB_VMODE_VRR)) {
		WARN(enable, "VRR enable request in non-VRR mode\n");
		return;
	}

	vrr->enable = enable;
}

static void tegra_dc_hdmi_postpoweron(struct tegra_dc *dc)
{
	_tegra_hdmivrr_activate(tegra_dc_get_outdata(dc), true);
}

static void tegra_dc_hdmi_sor_sleep(struct tegra_dc *dc)
{
	struct tegra_hdmi *hdmi = tegra_dc_get_outdata(dc);

	if (hdmi->sor->sor_state == SOR_ATTACHED)
		tegra_dc_sor_sleep(hdmi->sor);
}

struct tegra_dc_out_ops tegra_dc_hdmi2_0_ops = {
	.init = tegra_dc_hdmi_init,
	.hotplug_init = tegra_dc_hdmi_hpd_init,
	.destroy = tegra_dc_hdmi_destroy,
	.enable = tegra_dc_hdmi_enable,
	.disable = tegra_dc_hdmi_disable,
	.setup_clk = tegra_dc_hdmi_setup_clk,
	.detect = tegra_dc_hdmi_detect,
	.shutdown = tegra_dc_hdmi_shutdown,
	.suspend = tegra_dc_hdmi_suspend,
	.resume = tegra_dc_hdmi_resume,
	.ddc_enable = tegra_dc_hdmi_ddc_enable,
	.ddc_disable = tegra_dc_hdmi_ddc_disable,
	.modeset_notifier = tegra_dc_hdmi_modeset_notifier,
	.mode_filter = tegra_hdmi_fb_mode_filter,
	.hpd_state = tegra_dc_hdmi_hpd_state,
	.vrr_enable = tegra_dc_hdmi_vrr_enable,
	.vrr_update_monspecs = tegra_hdmivrr_update_monspecs,
	.set_hdr = tegra_dc_hdmi_set_hdr,
	.postpoweron = tegra_dc_hdmi_postpoweron,
	.shutdown_interface = tegra_dc_hdmi_sor_sleep,
};
