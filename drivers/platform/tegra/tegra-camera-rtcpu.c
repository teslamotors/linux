/*
 * Copyright (c) 2015-2016 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/tegra-camera-rtcpu.h>

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/iommu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/sched.h>
#include <linux/seq_buf.h>
#include <linux/slab.h>
#include <linux/tegra_ast.h>
#include <linux/tegra-firmwares.h>
#include <linux/tegra-hsp.h>
#include <linux/tegra-ivc-bus.h>
#include <linux/tegra_pm_domains.h>
#include <linux/tegra-rtcpu-monitor.h>
#include <linux/tegra-rtcpu-trace.h>
#include <linux/wait.h>
#include <linux/tegra_pm_domains.h>

#include <dt-bindings/memory/tegra-swgroup.h>
#include <dt-bindings/memory/tegra186-swgroup.h>

#include "rtcpu/camrtc-commands.h"
#include "rtcpu/camrtc-ctrl-commands.h"

/* Register specifics */
#define TEGRA_APS_FRSC_SC_CTL_0			0x0
#define TEGRA_APS_FRSC_SC_MODEIN_0		0x14
#define TEGRA_PM_R5_CTRL_0			0x40
#define TEGRA_PM_PWR_STATUS_0			0x20

#define TEGRA_R5R_SC_DISABLE			0x5
#define TEGRA_FN_MODEIN				0x29527
#define TEGRA_PM_FWLOADDONE			0x2
#define TEGRA_PM_WFIPIPESTOPPED			0x200000

static const char * const sce_clock_names[] = {
	"sce-apb",
	"sce-cpu-nic",
};

static const struct rtcpu_clock_rates {
	unsigned slow, fast;
} sce_clock_rates[] = {
	/* Slow rate (RTPM),  fast rate */
	{ .slow = 102000000, .fast = 102000000, },
	{ .slow = 115200000, .fast = 473600000, },
};

static inline void bugme(void)
{
BUILD_BUG_ON(ARRAY_SIZE(sce_clock_names) != ARRAY_SIZE(sce_clock_rates));
}

static const char * const sce_reset_names[] = {
	/* Group 1 */
	"tsctnsce",
	"sce-pm",
	"sce-dbgresetn",
	"sce-presetdbgn",
	"sce-actmon",
	"sce-dma",
	"sce-tke",
	"sce-gte",
	"sce-cfg",
	/* Group 2: nSYSPORRESET, nRESET */
	"sce-nreset",
	"sce-nsysporeset",
};

struct sce_resets {
	struct reset_control *tsctnsce;
	struct reset_control *sce_pm;
	struct reset_control *sce_dbgresetn;
	struct reset_control *sce_presetdbgn;
	struct reset_control *sce_actmon;
	struct reset_control *sce_dma;
	struct reset_control *sce_tke;
	struct reset_control *sce_gte;
	struct reset_control *sce_cfg;
	/* Group 2: nSYSPORRESET, nRESET */
	struct reset_control *sce_nreset;
	struct reset_control *sce_nsysporeset;
};

static const char * const sce_reg_names[] = {
	"sce-cfg",
	"sce-pm",
};

static const char * const ape_clock_names[] = {
	"ahub",
	"apb2ape",
	"ape",
	"adsp",
	"adspneon",
};

static const char * const ape_reset_names[] = {
	"adsp-all"
};

static const char * const ape_reg_names[] = {
};

enum tegra_cam_rtcpu_id {
	TEGRA_CAM_RTCPU_SCE,
	TEGRA_CAM_RTCPU_APE,
};

struct tegra_cam_rtcpu_pdata {
	const char *name;
	const char * const *clock_names;
	const struct rtcpu_clock_rates *clock_rates;
	const char * const *reset_names;
	const char * const *reg_names;
	enum tegra_cam_rtcpu_id id;
	u32 num_clocks;
	u32 num_resets;
	u32 num_regs;
};

static const struct tegra_cam_rtcpu_pdata sce_pdata = {
	.name = "sce",
	.clock_names = sce_clock_names,
	.clock_rates = sce_clock_rates,
	.reset_names = sce_reset_names,
	.id = TEGRA_CAM_RTCPU_SCE,
	.num_clocks = ARRAY_SIZE(sce_clock_names),
	.num_resets = ARRAY_SIZE(sce_reset_names),
	.reg_names = sce_reg_names,
	.num_regs = ARRAY_SIZE(sce_reg_names),
};

static const struct tegra_cam_rtcpu_pdata ape_pdata = {
	.name = "ape",
	.clock_names = ape_clock_names,
	.reset_names = ape_reset_names,
	.id = TEGRA_CAM_RTCPU_APE,
	.num_clocks = ARRAY_SIZE(ape_clock_names),
	.num_resets = ARRAY_SIZE(ape_reset_names),
	.reg_names = ape_reg_names,
	.num_regs = ARRAY_SIZE(ape_reg_names),
};

#define NV(p) "nvidia," #p

static const struct of_device_id tegra_cam_rtcpu_of_match[] = {
	{
		.compatible = NV(tegra186-sce-ivc), .data = &sce_pdata
	},
	{
		.compatible = NV(tegra186-ape-ivc), .data = &ape_pdata
	},
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_cam_rtcpu_of_match);

enum rtcpu_state {
	RTCPU_DOWN,
	RTCPU_UP,
};

#define MAX(x, y) (x > y ? x : y)
#define NUM(names) MAX(ARRAY_SIZE(sce_##names), ARRAY_SIZE(ape_##names))

struct tegra_cam_rtcpu {
	const char *name;
	struct tegra_ivc_bus *ivc;
	struct tegra_ast *ast;
	struct tegra_hsp_sm_pair *sm_pair;
	struct tegra_rtcpu_trace *tracer;
	struct {
		struct mutex mutex;
		wait_queue_head_t response_waitq;
		wait_queue_head_t empty_waitq;
		atomic_t response;
		atomic_t emptied;
	} cmd;
	u32 fw_version;
	u32 ivc_version;
	u8 fw_hash[RTCPU_FW_HASH_SIZE];
	union {
		void __iomem *regs[NUM(reg_names)];
		struct {
			void __iomem *cfg_base;
			void __iomem *pm_base;
		};
	};
	struct clk *clocks[NUM(clock_names)];
	union {
		struct sce_resets sce_resets;
		struct reset_control *resets[NUM(reset_names)];
	};
	const struct tegra_cam_rtcpu_pdata *pdata;
	struct tegra_camrtc_mon *monitor;
	bool power_domain;
	enum rtcpu_state state;
};

struct reset_control *tegra_camrtc_reset_control_get(struct device *dev,
						const char *id)
{
	/*
	 * Depending on firmware, not all resets are available.
	 * Unavailable resets are removed from device tree but
	 * reset_control_get() returns -EPROBE_DEFER for them.
	 */
	if (of_property_match_string(dev->of_node, "reset-names", id) < 0)
		return ERR_PTR(-ENOENT);
	return devm_reset_control_get(dev, id);
}

static int tegra_cam_rtcpu_get_resources(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->pdata;
	int i, err;

#define GET_RESOURCES(_res_, _get_, _warn_) \
	for (i = 0; i < pdata->num_##_res_##s; i++) { \
		rtcpu->_res_##s[i] = _get_(dev, pdata->_res_##_names[i]); \
		if (!IS_ERR(rtcpu->_res_##s[i])) \
			continue; \
		err = PTR_ERR(rtcpu->_res_##s[i]); \
		if (err == -EPROBE_DEFER) { \
			dev_info(dev, "defer %s probe because %s %s\n", \
				rtcpu->name, #_res_, pdata->_res_##_names[i]); \
			return err; \
		} \
		if (_warn_ && err != -ENODATA) \
			dev_err(dev, "%s %s not available: %d\n", #_res_, \
				pdata->_res_##_names[i], err); \
	}

	GET_RESOURCES(clock, devm_clk_get, true);
	GET_RESOURCES(reset, tegra_camrtc_reset_control_get, true);
	GET_RESOURCES(reg, tegra_ioremap_byname, true);

	return 0;
}

static int tegra_cam_rtcpu_clk_disable_unprepare(struct clk *clk)
{
	clk_disable_unprepare(clk);

	return 0;
}

static int tegra_cam_rtcpu_apply_clks(struct device *dev,
					int (*func)(struct clk *clk))
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(rtcpu->clocks); i++) {
		if (IS_ERR_OR_NULL(rtcpu->clocks[i]))
			continue;

		ret = (*func)(rtcpu->clocks[i]);
		if (ret) {
			dev_err(dev, "clock %s failed: %d\n",
				rtcpu->pdata->clock_names[i], ret);
		}
	}

	return 0;
}

static int tegra_cam_rtcpu_apply_resets(struct device *dev,
					int (*func)(struct reset_control *))
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(rtcpu->resets); i++) {
		if (IS_ERR_OR_NULL(rtcpu->resets[i]))
			continue;

		ret = (*func)(rtcpu->resets[i]);
		if (ret) {
			dev_err(dev, "reset %s failed: %d\n",
				rtcpu->pdata->reset_names[i], ret);
		}
	}

	return 0;
}

static int tegra_camrtc_wait_for_wfi(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	long timeout = HZ, delay_stride = HZ / 50;

	if (rtcpu->pm_base == NULL)
		return 0;

	/* Poll for WFI assert.*/
	for (;;) {
		u32 val = readl(rtcpu->pm_base + TEGRA_PM_PWR_STATUS_0);

		if ((val & TEGRA_PM_WFIPIPESTOPPED) == 0)
			break;

		if (timeout < 0)
			return -EBUSY;

		msleep(delay_stride);
		timeout -= delay_stride;
	}

	return 0;
}

/*
 * Send the PM_SUSPEND command to remote core FW and
 * perform necessary checks to confirm if RTCPU is
 * indeed suspended.
 */
static int tegra_cam_rtcpu_cmd_remote_suspend(struct device *dev)
{
	int err;
	int expect = RTCPU_COMMAND(PM_SUSPEND,
				(CAMRTC_PM_CTRL_STATE_SUSPEND << 8) |
				CAMRTC_PM_CTRL_STATUS_OK);

	/* Suspend the RTCPU */
	err = tegra_camrtc_command(dev, RTCPU_COMMAND(PM_SUSPEND, 0), 0);
	if (err != expect) {
		dev_err(dev, "suspend failed: 0x%08x\n", (unsigned)err);
		if (err >= 0)
			return -EIO;
		return err;
	}

	err = tegra_camrtc_wait_for_wfi(dev);
	if (err) {
		dev_err(dev, "failed to go into WFI\n");
		return err;
	}

	return 0;
}

int tegra_camrtc_restore(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return tegra_camrtc_mon_restore_rtcpu(rtcpu->monitor);
}
EXPORT_SYMBOL(tegra_camrtc_restore);

int tegra_camrtc_start(struct device *dev)
{
	int ret;
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	ret = tegra_cam_rtcpu_apply_clks(dev, clk_prepare_enable);
	if (ret) {
		dev_err(dev, "failed to turn on clocks: %d\n", ret);
		return ret;
	}

	ret = tegra_cam_rtcpu_apply_resets(dev, reset_control_deassert);
	if (ret) {
		dev_err(dev, "failed to deassert the resets while starting camrtc: %d",
			ret);
		return ret;
	}

	if (rtcpu->ivc == NULL)
		/* probe is not yet complete */
		return 0;

	tegra_camrtc_boot(dev);

	ret = tegra_camrtc_ivc_setup_ready(dev);
	if (ret) {
		dev_err(dev, "failed to setup ivc: %d\n", ret);
		return ret;
	}

	rtcpu->state = RTCPU_UP;

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_start);

bool tegra_camrtc_is_rtcpu_alive(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return rtcpu->state == RTCPU_UP;
}
EXPORT_SYMBOL(tegra_camrtc_is_rtcpu_alive);

int tegra_camrtc_stop(struct device *dev)
{
	int ret;

	ret = tegra_cam_rtcpu_cmd_remote_suspend(dev);
	if (ret) {
		dev_err(dev, "failed to suspend camera rtcpu: %d\n", ret);
		return ret;
	}

	tegra_camrtc_set_halt(dev, true);

	ret = tegra_cam_rtcpu_apply_resets(dev, reset_control_reset);
	if (ret) {
		dev_err(dev, "failed to assert the resets: %d\n", ret);
		return ret;
	}

	ret = tegra_cam_rtcpu_apply_clks(dev,
				tegra_cam_rtcpu_clk_disable_unprepare);
	if (ret) {
		dev_err(dev, "failed to turn off clocks: %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_stop);

int tegra_camrtc_reset(struct device *dev)
{
	return tegra_cam_rtcpu_apply_resets(dev, reset_control_reset);
}
EXPORT_SYMBOL(tegra_camrtc_reset);

int tegra_camrtc_set_halt(struct device *dev, bool halt)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	u32 reg_val;

	if (rtcpu->pm_base) {
		reg_val = readl(rtcpu->pm_base + TEGRA_PM_R5_CTRL_0);

		if (halt) {
			reg_val &= ~TEGRA_PM_FWLOADDONE;
		} else {
			/* Set FW load done bit to unhalt */
			reg_val |= TEGRA_PM_FWLOADDONE;
		}

		dev_info(dev, "%s gets %s\n", rtcpu->name,
			halt ? "halted" : "unhalted");

		writel(reg_val, rtcpu->pm_base + TEGRA_PM_R5_CTRL_0);
	}

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_set_halt);

int tegra_camrtc_get_halt(struct device *dev, bool *halt)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	u32 reg_val;

	if (dev == NULL)
		return -EINVAL;

	if (rtcpu->pm_base == NULL) {
		*halt = 0;
		return 0;
	}

	reg_val = readl(rtcpu->pm_base + TEGRA_PM_R5_CTRL_0);

	*halt = (reg_val & TEGRA_PM_FWLOADDONE) == 0;

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_get_halt);

void tegra_camrtc_ready(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (rtcpu->pm_base) {
		u32 val = readl(rtcpu->pm_base + TEGRA_PM_R5_CTRL_0);

		/* Skip configuring core if it is already unhalted */
		if ((val & TEGRA_PM_FWLOADDONE) != 0)
			return;
	}

	if (rtcpu->cfg_base) {
		/* Disable R5R and smartcomp in camera mode */
		writel(TEGRA_R5R_SC_DISABLE,
			rtcpu->cfg_base + TEGRA_APS_FRSC_SC_CTL_0);

		/* Enable JTAG/Coresight */
		writel(TEGRA_FN_MODEIN,
			rtcpu->cfg_base + TEGRA_APS_FRSC_SC_MODEIN_0);

		/* Reset R5 */
		if (!IS_ERR_OR_NULL(rtcpu->sce_resets.sce_nsysporeset))
			reset_control_reset(rtcpu->sce_resets.sce_nsysporeset);
	}
}
EXPORT_SYMBOL(tegra_camrtc_ready);

static u32 tegra_camrtc_full_notify(void *data, u32 response)
{
	struct tegra_cam_rtcpu *rtcpu = data;

	atomic_set(&rtcpu->cmd.response, response);
	wake_up(&rtcpu->cmd.response_waitq);
	return 0;
}

static void tegra_camrtc_empty_notify(void *data, u32 empty_value)
{
	struct tegra_cam_rtcpu *rtcpu = data;

	atomic_set(&rtcpu->cmd.emptied, 1);
	wake_up(&rtcpu->cmd.empty_waitq);
}

static long tegra_camrtc_wait_for_empty(struct device *dev,
				long timeout)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (timeout == 0)
		timeout = 2 * HZ;

	timeout = wait_event_interruptible_timeout(
		rtcpu->cmd.empty_waitq,
		/* Make sure IRQ has been handled */
		atomic_read(&rtcpu->cmd.emptied) != 0 &&
		tegra_hsp_sm_pair_is_empty(rtcpu->sm_pair),
		timeout);

	if (timeout > 0)
		atomic_set(&rtcpu->cmd.emptied, 0);

	return timeout;
}

int tegra_camrtc_command(struct device *dev, u32 command, long timeout)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int response;

#define INVALID_RESPONSE (0x80000000U)

	if (timeout == 0)
		timeout = 2 * HZ;

	mutex_lock(&rtcpu->cmd.mutex);

	timeout = tegra_camrtc_wait_for_empty(dev, timeout);
	if (timeout <= 0) {
		dev_err(dev, "Timed out waiting for empty mailbox");
		response = -ETIMEDOUT;
		goto done;
	}

	atomic_set(&rtcpu->cmd.response, INVALID_RESPONSE);

	tegra_hsp_sm_pair_write(rtcpu->sm_pair, command);

	timeout = wait_event_interruptible_timeout(
		rtcpu->cmd.response_waitq,
		atomic_read(&rtcpu->cmd.response) != INVALID_RESPONSE,
		timeout);
	if (timeout <= 0) {
		dev_err(dev, "Timed out waiting for response");
		response = -ETIMEDOUT;
		goto done;
	}

	response = (int)atomic_read(&rtcpu->cmd.response);

done:
	mutex_unlock(&rtcpu->cmd.mutex);

	return response;
}
EXPORT_SYMBOL(tegra_camrtc_command);

int tegra_camrtc_boot(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	u32 command;
	int ret;

	dev_info(dev, "booting %s with Camera RTCPU FW\n", rtcpu->name);

	tegra_camrtc_ready(dev);

	tegra_camrtc_set_halt(dev, false);

	/*
	 * Handshake FW version before continueing with the boot
	 */
	command = RTCPU_COMMAND(INIT, 0);
	ret = tegra_camrtc_command(dev, command, 0);
	if (ret < 0)
		return ret;
	if (ret != command) {
		dev_err(dev, "RTCPU sync problem (response=0x%08x)\n", ret);
		return -EIO;
	}

	command = RTCPU_COMMAND(FW_VERSION, RTCPU_FW_SM2_VERSION);
	ret = tegra_camrtc_command(dev, command, 0);
	if (ret < 0)
		return ret;
	if (RTCPU_GET_COMMAND_ID(ret) != RTCPU_CMD_FW_VERSION ||
		RTCPU_GET_COMMAND_VALUE(ret) < RTCPU_FW_VERSION) {
		dev_err(dev, "RTCPU version mismatch (response=0x%08x)\n", ret);
		return -EIO;
	}

	rtcpu->fw_version = RTCPU_GET_COMMAND_VALUE(ret);

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_boot);

int tegra_camrtc_ivc_setup_ready(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	u32 command;
	int ret;

	if (rtcpu->tracer) {
		dev_info(dev, "enabling tracing\n");
		command = RTCPU_COMMAND(IVC_READY, RTCPU_IVC_WITH_TRACE);
	} else {
		dev_warn(dev, "disabling tracing\n");
		command = RTCPU_COMMAND(IVC_READY, RTCPU_IVC_SANS_TRACE);
	}

	ret = tegra_camrtc_command(dev, command, 0);
	if (ret < 0)
		return ret;

	if (RTCPU_GET_COMMAND_ID(ret) != RTCPU_CMD_IVC_READY) {
		dev_err(dev, "IVC setup problem (response=0x%08x)\n", ret);
		return -EIO;
	}

	rtcpu->ivc_version = RTCPU_GET_COMMAND_VALUE(ret);

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_ivc_setup_ready);

static int tegra_camrtc_get_fw_hash(struct device *dev,
				u8 hash[RTCPU_FW_HASH_SIZE])
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int ret, i;
	u32 value;

	if (rtcpu->fw_version < RTCPU_FW_SM2_VERSION) {
		dev_info(dev, "fw version %u has no sha1\n", rtcpu->fw_version);
		return -EIO;
	}

	for (i = 0; i < RTCPU_FW_HASH_SIZE; i++) {
		ret = tegra_camrtc_command(dev, RTCPU_COMMAND(FW_HASH, i), 0);
		value = RTCPU_GET_COMMAND_VALUE(ret);

		if (ret < 0 ||
			RTCPU_GET_COMMAND_ID(ret) != RTCPU_CMD_FW_HASH ||
			value > (u8)~0) {
			dev_warn(dev, "FW_HASH problem (0x%08x)\n", ret);
			return -EIO;
		}

		hash[i] = value;
	}

	return 0;
}

ssize_t tegra_camrtc_print_version(struct device *dev,
					char *buf, size_t size)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	struct seq_buf s;
	int i;

	seq_buf_init(&s, buf, size);
	seq_buf_printf(&s, "version cpu=%s cmd=%u ivc=%u sha1=",
		rtcpu->name, rtcpu->fw_version, rtcpu->ivc_version);

	for (i = 0; i < RTCPU_FW_HASH_SIZE; i++)
		seq_buf_printf(&s, "%02x", rtcpu->fw_hash[i]);

	return seq_buf_used(&s);
}
EXPORT_SYMBOL(tegra_camrtc_print_version);

static void tegra_camrtc_log_fw_version(struct device *dev)
{
	char version[TEGRA_CAMRTC_VERSION_LEN];

	tegra_camrtc_print_version(dev, version, sizeof(version));

	dev_info(dev, "firmware %s\n", version);
}

static int tegra_cam_rtcpu_runtime_suspend(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->pdata;
	int i;

	if (pdata->clock_rates == NULL)
		return 0;

	for (i = 0; i < ARRAY_SIZE(rtcpu->clocks); i++) {
		if (IS_ERR_OR_NULL(rtcpu->clocks[i]))
			continue;
		clk_set_rate(rtcpu->clocks[i], pdata->clock_rates[i].slow);
	}

	return 0;
}

static int tegra_cam_rtcpu_runtime_resume(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->pdata;
	int i;

	if (pdata->clock_rates == NULL)
		return 0;

	for (i = 0; i < ARRAY_SIZE(rtcpu->clocks); i++) {
		if (IS_ERR_OR_NULL(rtcpu->clocks[i]))
			continue;
		clk_set_rate(rtcpu->clocks[i], pdata->clock_rates[i].fast);
	}

	return 0;
}

static int tegra_cam_rtcpu_remove(struct platform_device *pdev)
{
	struct tegra_cam_rtcpu *rtcpu = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);

	if (rtcpu->power_domain)
		tegra_pd_remove_device(&pdev->dev);

	tegra_cam_rtcpu_mon_destroy(rtcpu->monitor);
	tegra_ivc_bus_destroy(rtcpu->ivc);
	tegra_ast_destroy(rtcpu->ast);
	tegra_hsp_sm_pair_free(rtcpu->sm_pair);

	return 0;
}

static int tegra_cam_rtcpu_probe(struct platform_device *pdev)
{
	struct tegra_cam_rtcpu *rtcpu;
	const struct tegra_cam_rtcpu_pdata *pdata;
	struct device *dev = &pdev->dev;
	struct device_node *hsp_node;
	int ret;
	const struct of_device_id *match;
	const char *name;
	u32 stream_id;

	match = of_match_device(tegra_cam_rtcpu_of_match, dev);
	if (match == NULL) {
		dev_err(dev, "Device match not found\n");
		return -ENODEV;
	}
	pdata = match->data;
	name = pdata->name;
	of_property_read_string(dev->of_node, NV(cpu-name), &name);

	dev_dbg(dev, "probing RTCPU on %s\n", name);

	rtcpu = devm_kzalloc(dev, sizeof(*rtcpu), GFP_KERNEL);
	if (!rtcpu)
		return -ENOMEM;

	rtcpu->pdata = pdata;
	rtcpu->name = name;
	platform_set_drvdata(pdev, rtcpu);

	ret = of_property_read_u32(dev->of_node, NV(stream-id), &stream_id);
	if (ret) {
		stream_id = iommu_get_hwid(dev->archdata.iommu, dev, 0);
		if ((int)stream_id < 0) {
			stream_id = TEGRA_SID_RCE;
			dev_info(dev, "%s defaults to stream-id %u\n",
				rtcpu->name, stream_id);
		}
	} else {
		/* APE power device add */
		tegra_pd_add_device(dev);
	}

	ret = tegra_cam_rtcpu_get_resources(dev);
	if (ret)
		return ret;

	if (of_property_read_bool(dev->of_node, "power-domains")) {
		rtcpu->power_domain = true;
		tegra_pd_add_device(dev);
	}

	/* enable power management */
	pm_runtime_enable(dev);

	ret = tegra_cam_rtcpu_apply_clks(dev, clk_prepare_enable);
	if (ret) {
		dev_err(dev, "failed to turn on %s clocks: %d\n", name, ret);
		return ret;
	}

	tegra_cam_rtcpu_apply_resets(dev, reset_control_deassert);

	/* set resume state */
	pm_runtime_get_sync(dev);

	mutex_init(&rtcpu->cmd.mutex);
	init_waitqueue_head(&rtcpu->cmd.response_waitq);
	init_waitqueue_head(&rtcpu->cmd.empty_waitq);
	hsp_node = of_get_child_by_name(dev->of_node, "hsp");
	rtcpu->sm_pair = of_tegra_hsp_sm_pair_by_name(hsp_node,
					"cmd-pair", tegra_camrtc_full_notify,
					tegra_camrtc_empty_notify, rtcpu);
	of_node_put(hsp_node);
	if (IS_ERR(rtcpu->sm_pair)) {
		ret = PTR_ERR(rtcpu->sm_pair);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to obtain %s mbox pair: %d\n",
				name, ret);
		goto fail;
	}

	rtcpu->ast = tegra_ast_create(dev);
	if (IS_ERR(rtcpu->ast)) {
		ret = PTR_ERR(rtcpu->ast);
		goto fail;
	}

	rtcpu->tracer = tegra_rtcpu_trace_create(dev, rtcpu->ast, stream_id);

	ret = tegra_camrtc_boot(dev);
	if (ret)
		goto fail;

	ret = tegra_camrtc_get_fw_hash(dev, rtcpu->fw_hash);
	if (ret == 0)
		devm_tegrafw_register(dev, "camrtc",
			TFW_NORMAL, tegra_camrtc_print_version, NULL);

	rtcpu->ivc = tegra_ivc_bus_create(dev, rtcpu->ast, stream_id);
	if (IS_ERR(rtcpu->ivc)) {
		ret = PTR_ERR(rtcpu->ivc);
		goto fail;
	}

	ret = tegra_camrtc_ivc_setup_ready(dev);
	if (ret)
		goto fail;

	rtcpu->monitor = tegra_camrtc_mon_create(dev);
	/* set idle to slow down clock while idle mode */
	pm_runtime_idle(dev);
	rtcpu->state = RTCPU_UP;

	tegra_camrtc_log_fw_version(dev);

	dev_dbg(dev, "successfully probed RTCPU on %s\n", name);

	return 0;

fail:
	pm_runtime_put_sync(dev);
	tegra_cam_rtcpu_remove(pdev);
	return ret;
}

static int tegra_cam_rtcpu_resume(struct device *dev)
{
	return tegra_camrtc_start(dev);
}

static int tegra_cam_rtcpu_suspend(struct device *dev)
{
	return tegra_camrtc_stop(dev);
}

static void tegra_cam_rtcpu_shutdown(struct platform_device *pdev)
{
	int ret;

	ret = tegra_camrtc_stop(&pdev->dev);
	if (ret)
		dev_err(&pdev->dev, "failed to shutdown rtcpu: %d\n", ret);
}

static const struct dev_pm_ops tegra_cam_rtcpu_pm_ops = {
	.suspend = tegra_cam_rtcpu_suspend,
	.resume = tegra_cam_rtcpu_resume,
	.runtime_suspend = tegra_cam_rtcpu_runtime_suspend,
	.runtime_resume = tegra_cam_rtcpu_runtime_resume,
	.runtime_idle = tegra_cam_rtcpu_runtime_suspend,
};

static struct platform_driver tegra_cam_rtcpu_driver = {
	.driver = {
		.name	= "tegra186-cam-rtcpu",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_cam_rtcpu_of_match),
#ifdef CONFIG_PM
		.pm = &tegra_cam_rtcpu_pm_ops,
#endif
	},
	.probe = tegra_cam_rtcpu_probe,
	.remove = tegra_cam_rtcpu_remove,
	.shutdown = tegra_cam_rtcpu_shutdown,
};
module_platform_driver(tegra_cam_rtcpu_driver);

MODULE_DESCRIPTION("CAMERA RTCPU driver");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL v2");
