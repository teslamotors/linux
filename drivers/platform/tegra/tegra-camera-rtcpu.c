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
#include <linux/delay.h>
#include <linux/tegra-ivc-bus.h>
#include <linux/tegra-rtcpu-trace.h>
#include <linux/wait.h>
#include <linux/tegra_pm_domains.h>

#include <dt-bindings/memory/tegra-swgroup.h>
#include <dt-bindings/memory/tegra186-swgroup.h>

#include "rtcpu/camrtc-commands.h"
#include "rtcpu/camrtc-ctrl-commands.h"

/* Register specifics */
#define TEGRA_APS_FRSC_SC_CTL_0			0x0
#define TEGRA_SCE_APS_FRSC_SC_MODEIN_0		0x14
#define TEGRA_SCE_EVP_RESET_ADDR_0		0x20
#define TEGRA_SCEPM_R5_CTRL_0			0x40
#define TEGRA_SCEPM_PWR_STATUS_0		0x20

#define TEGRA_SCE_R5R_SC_DISABLE		0x5
#define TEGRA_SCE_FN_MODEIN			0x29527
#define TEGRA_SCE_FWLOADDONE			0x2
#define TEGRA_SCEPM_WFIPIPESTOPPED		0x200000

static const char * const sce_clock_names[] = {
	"sce-apb",
	"sce-cpu-nic",
};

static const unsigned sce_clock_rates[][2] = {
	/* Slow rate (RTPM),	fast rate */
	{ 102000000,		102000000, },
	{ 115200000,		473600000, },
};

static const char * const sce_reset_names[] = {
	"sce-pm",
	"sce-nsysporeset",
	"sce-nreset",
	"sce-dbgresetn",
	"sce-presetdbgn",
	"sce-actmon",
	"sce-dma",
	"tsctnsce",
	"sce-tke",
	"sce-gte",
	"sce-cfg",
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

enum tegra_cam_rtcpu_id {
	TEGRA_CAM_RTCPU_SCE,
	TEGRA_CAM_RTCPU_APE,
};

struct tegra_cam_rtcpu_pdata {
	const char * const *clock_names;
	const unsigned (*clock_rates)[2];
	const char * const *reset_names;
	enum tegra_cam_rtcpu_id id;
	u32 sid;
	u32 num_clocks;
	u32 num_resets;
};

static const struct tegra_cam_rtcpu_pdata sce_pdata = {
	.clock_names = sce_clock_names,
	.clock_rates = sce_clock_rates,
	.reset_names = sce_reset_names,
	.id = TEGRA_CAM_RTCPU_SCE,
	.sid = TEGRA_SID_SCE,
	.num_clocks = ARRAY_SIZE(sce_clock_names),
	.num_resets = ARRAY_SIZE(sce_reset_names),
};

static const struct tegra_cam_rtcpu_pdata ape_pdata = {
	.clock_names = ape_clock_names,
	.reset_names = ape_reset_names,
	.id = TEGRA_CAM_RTCPU_APE,
	.sid = TEGRA_SID_APE,
	.num_clocks = ARRAY_SIZE(ape_clock_names),
	.num_resets = ARRAY_SIZE(ape_reset_names),
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

struct tegra_cam_rtcpu {
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
		struct {
			void __iomem *sce_cfg_base;
			void __iomem *sce_pm_base;
		} __packed rtcpu_sce;
	};

	struct clk **clocks;
	struct reset_control **resets;
	const struct tegra_cam_rtcpu_pdata *rtcpu_pdata;
};

static int tegra_cam_rtcpu_get_clks_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->rtcpu_pdata;
	int i;

	rtcpu->clocks = devm_kmalloc(dev,
		pdata->num_clocks * sizeof(rtcpu->clocks[0]), GFP_KERNEL);
	rtcpu->resets = devm_kmalloc(dev,
		pdata->num_resets * sizeof(rtcpu->resets[0]), GFP_KERNEL);
	if (unlikely(rtcpu->clocks == NULL || rtcpu->resets == NULL))
		return -ENOMEM;

	/* Get clocks and resets */
	for (i = 0; i < pdata->num_clocks; i++) {
		rtcpu->clocks[i] = devm_clk_get(dev, pdata->clock_names[i]);
		if (IS_ERR(rtcpu->clocks[i])) {
			if (rtcpu->clocks[i] == ERR_PTR(-EPROBE_DEFER))
				return -EPROBE_DEFER;

			dev_err(dev, "clock %s not found: %ld\n",
				pdata->clock_names[i],
				PTR_ERR(rtcpu->clocks[i]));
		}
	}

	for (i = 0; i < pdata->num_resets; i++) {
		rtcpu->resets[i] =
			devm_reset_control_get(dev, pdata->reset_names[i]);
		if (IS_ERR(rtcpu->resets[i])) {
			if (rtcpu->resets[i] == ERR_PTR(-EPROBE_DEFER))
				return -EPROBE_DEFER;

			dev_err(dev, "reset %s not found: %ld\n",
				pdata->reset_names[i],
				PTR_ERR(rtcpu->resets[i]));
		}
	}

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
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->rtcpu_pdata;
	int i, ret;

	for (i = 0; i < pdata->num_clocks; i++) {
		if (IS_ERR(rtcpu->clocks[i]))
			continue;

		ret = (*func)(rtcpu->clocks[i]);
		if (ret) {
			dev_err(dev, "clock %s failed: %d\n",
				pdata->clock_names[i], ret);
		}
	}

	return 0;
}

static int tegra_cam_rtcpu_apply_resets(struct device *dev,
					int (*func)(struct reset_control *))
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->rtcpu_pdata;
	int i, ret;

	for (i = 0; i < pdata->num_resets; i++) {
		if (IS_ERR(rtcpu->resets[i]))
			continue;

		ret = (*func)(rtcpu->resets[i]);
		if (ret) {
			dev_err(dev, "reset %s failed: %d\n",
				pdata->reset_names[i], ret);
		}
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
	u32 reg_val;
	int err;
	long timeout = HZ, delay_stride = HZ/50;

	struct tegra_cam_rtcpu *cam_rtcpu = dev_get_drvdata(dev);

	/* Suspend the RTCPU */
	err = tegra_camrtc_command(dev, RTCPU_COMMAND(PM_SUSPEND, 0), 0);
	if (err < 0) {
		dev_err(dev, "tegra_camrtc_command failed: 0x%08x", err);
		return err;
	}

	if (err != RTCPU_COMMAND(PM_SUSPEND,
			(CAMRTC_PM_CTRL_STATE_SUSPEND << 8) |
				CAMRTC_PM_CTRL_STATUS_OK)) {
		dev_err(dev, "Suspend failed: RTCPU sync problem 0x%08x", err);
		return -EIO;
	}

	/* Check if standbyWFI is asserted, to proceed for SC7 */
	if (cam_rtcpu->rtcpu_pdata->id == TEGRA_CAM_RTCPU_SCE) {
		/* Poll for WFI assert.*/
		while (1) {
			reg_val = readl(cam_rtcpu->rtcpu_sce.sce_pm_base +
					TEGRA_SCEPM_PWR_STATUS_0);

			if ((reg_val & TEGRA_SCEPM_WFIPIPESTOPPED) == 0)
				break;

			if (timeout <= 0) {
				dev_err(dev, "SCE failed to go into WFI\n");
				return -EBUSY;
			}

			msleep(delay_stride);
			timeout -= delay_stride;
		}
	}

	return 0;
}

int tegra_camrtc_start(struct device *dev)
{
	int ret;
	struct tegra_cam_rtcpu *cam_rtcpu = dev_get_drvdata(dev);

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

	if (cam_rtcpu->ivc == NULL)
		/* probe is not yet complete */
		return 0;

	tegra_camrtc_boot(dev);
	return tegra_camrtc_ivc_setup_ready(dev);
}
EXPORT_SYMBOL(tegra_camrtc_start);

int tegra_camrtc_stop(struct device *dev)
{
	int ret;

	ret = tegra_cam_rtcpu_cmd_remote_suspend(dev);
	if (ret) {
		dev_err(dev, "failed to suspend camera rtcpu: %d", ret);
		return ret;
	}

	tegra_camrtc_set_halt(dev, true);

	ret = tegra_cam_rtcpu_apply_resets(dev, reset_control_reset);
	if (ret) {
		dev_err(dev, "failed to assert the resets: %d", ret);
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
	struct tegra_cam_rtcpu *cam_rtcpu = dev_get_drvdata(dev);
	u32 reg_val;

	if (cam_rtcpu->rtcpu_pdata->id == TEGRA_CAM_RTCPU_SCE) {
		reg_val = readl(cam_rtcpu->rtcpu_sce.sce_pm_base +
				TEGRA_SCEPM_R5_CTRL_0);

		if (halt) {
			reg_val &= ~TEGRA_SCE_FWLOADDONE;
		} else {
			/* Set FW load done bit to unhalt */
			reg_val |= TEGRA_SCE_FWLOADDONE;
		}

		dev_info(dev, "sce gets %s", halt ? "halted" : "unhalted");

		writel(reg_val, cam_rtcpu->rtcpu_sce.sce_pm_base +
		       TEGRA_SCEPM_R5_CTRL_0);
	}

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_set_halt);

int tegra_camrtc_get_halt(struct device *dev, bool *halt)
{
	struct tegra_cam_rtcpu *cam_rtcpu = dev_get_drvdata(dev);
	u32 reg_val;

	if (dev == NULL)
		return -EINVAL;

	if (cam_rtcpu->rtcpu_pdata->id == TEGRA_CAM_RTCPU_SCE) {
		reg_val = readl(cam_rtcpu->rtcpu_sce.sce_pm_base +
				TEGRA_SCEPM_R5_CTRL_0);

		*halt = (reg_val & TEGRA_SCE_FWLOADDONE) == 0;

		dev_info(dev, "sce is %s", *halt ? "halted" : "unhalted");
	}

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_get_halt);

void tegra_camrtc_ready(struct device *dev)
{
	struct tegra_cam_rtcpu *cam_rtcpu = dev_get_drvdata(dev);

	if (cam_rtcpu->rtcpu_pdata->id == TEGRA_CAM_RTCPU_SCE) {
		/* Disable SCE R5R and smartcomp in camera mode */
		writel(TEGRA_SCE_R5R_SC_DISABLE,
			cam_rtcpu->rtcpu_sce.sce_cfg_base +
					TEGRA_APS_FRSC_SC_CTL_0);

		/* Enable JTAG/Coresight */
		writel(TEGRA_SCE_FN_MODEIN,
			cam_rtcpu->rtcpu_sce.sce_cfg_base +
					TEGRA_SCE_APS_FRSC_SC_MODEIN_0);
	}
}
EXPORT_SYMBOL(tegra_camrtc_ready);

static u32 tegra_camrtc_full_notify(void *data, u32 response)
{
	struct tegra_cam_rtcpu *cam_rtcpu = data;

	atomic_set(&cam_rtcpu->cmd.response, response);
	wake_up(&cam_rtcpu->cmd.response_waitq);
	return 0;
}

static void tegra_camrtc_empty_notify(void *data, u32 empty_value)
{
	struct tegra_cam_rtcpu *cam_rtcpu = data;

	atomic_set(&cam_rtcpu->cmd.emptied, 1);
	wake_up(&cam_rtcpu->cmd.empty_waitq);
}

static long tegra_camrtc_wait_for_empty(struct device *dev,
				long timeout)
{
	struct tegra_cam_rtcpu *cam_rtcpu = dev_get_drvdata(dev);

	if (timeout == 0)
		timeout = 2 * HZ;

	timeout = wait_event_interruptible_timeout(
		cam_rtcpu->cmd.empty_waitq,
		/* Make sure IRQ has been handled */
		atomic_read(&cam_rtcpu->cmd.emptied) != 0 &&
		tegra_hsp_sm_pair_is_empty(cam_rtcpu->sm_pair),
		timeout);

	if (timeout > 0)
		atomic_set(&cam_rtcpu->cmd.emptied, 0);

	return timeout;
}

int tegra_camrtc_command(struct device *dev, u32 command, long timeout)
{
	struct tegra_cam_rtcpu *cam_rtcpu = dev_get_drvdata(dev);
	int response;

#define INVALID_RESPONSE (0x80000000U)

	if (timeout == 0)
		timeout = 2 * HZ;

	mutex_lock(&cam_rtcpu->cmd.mutex);

	timeout = tegra_camrtc_wait_for_empty(dev, timeout);
	if (timeout <= 0) {
		dev_err(dev, "Timed out waiting for empty mailbox");
		response = -ETIMEDOUT;
		goto done;
	}

	atomic_set(&cam_rtcpu->cmd.response, INVALID_RESPONSE);

	tegra_hsp_sm_pair_write(cam_rtcpu->sm_pair, command);

	timeout = wait_event_interruptible_timeout(
		cam_rtcpu->cmd.response_waitq,
		atomic_read(&cam_rtcpu->cmd.response) != INVALID_RESPONSE,
		timeout);
	if (timeout <= 0) {
		dev_err(dev, "Timed out waiting for response");
		response = -ETIMEDOUT;
		goto done;
	}

	response = (int)atomic_read(&cam_rtcpu->cmd.response);

done:
	mutex_unlock(&cam_rtcpu->cmd.mutex);

	return response;
}
EXPORT_SYMBOL(tegra_camrtc_command);

int tegra_camrtc_boot(struct device *dev)
{
	struct tegra_cam_rtcpu *cam_rtcpu = dev_get_drvdata(dev);
	int ret;

	if (tegra_camrtc_wait_for_empty(dev, 0) <= 0) {
		dev_err(dev, "Timeout for HSP SM empty interrupt");
		return -ETIMEDOUT;
	}

	/*
	 * Intermediate firmware skips negotiation unless there is a
	 * command in mailbox when RTCPU gets unhalted.
	 *
	 * Insert a random command (INIT 16) to mailbox before
	 * unhalting.
	 */
	tegra_hsp_sm_pair_write(cam_rtcpu->sm_pair, RTCPU_COMMAND(INIT, 16));

	tegra_camrtc_ready(dev);

	if (cam_rtcpu->rtcpu_pdata->id == TEGRA_CAM_RTCPU_SCE) {
		/* Unhalt SCE */
		tegra_camrtc_set_halt(dev, false);

		dev_info(dev, "booting SCE with Camera RTCPU FW");
	} else {
		dev_info(dev, "booting APE with Camera RTCPU FW");
	}

	/*
	 * Handshake FW version before continueing with the boot
	 */
	ret = tegra_camrtc_command(dev,
		RTCPU_COMMAND(INIT, 0), 0);
	if (ret < 0)
		return ret;

	if (ret != RTCPU_COMMAND(INIT, 0)) {
		dev_err(dev, "RTCPU sync problem (response=0x%08x)", ret);
		return -EIO;
	}

	ret = tegra_camrtc_command(dev,
		RTCPU_COMMAND(FW_VERSION, RTCPU_FW_SM2_VERSION), 0);
	if (ret < 0)
		return ret;

	if (RTCPU_GET_COMMAND_ID(ret) != RTCPU_CMD_FW_VERSION ||
		RTCPU_GET_COMMAND_VALUE(ret) < RTCPU_FW_VERSION) {
		dev_err(dev, "RTCPU version mismatch (response=0x%08x)", ret);
		return -EIO;
	}

	cam_rtcpu->fw_version = RTCPU_GET_COMMAND_VALUE(ret);

	return 0;
}
EXPORT_SYMBOL(tegra_camrtc_boot);

int tegra_camrtc_ivc_setup_ready(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	u32 command;
	int ret;

	if (rtcpu->tracer) {
		dev_info(dev, "enabling tracing");
		command = RTCPU_COMMAND(IVC_READY, RTCPU_IVC_WITH_TRACE);
	} else {
		dev_warn(dev, "disabling tracing");
		command = RTCPU_COMMAND(IVC_READY, RTCPU_IVC_SANS_TRACE);
	}

	ret = tegra_camrtc_command(dev, command, 0);
	if (ret < 0)
		return ret;

	if (RTCPU_GET_COMMAND_ID(ret) != RTCPU_CMD_IVC_READY) {
		dev_err(dev, "IVC setup problem (response=0x%08x)", ret);
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
		dev_info(dev, "fw version %u has no sha1", rtcpu->fw_version);
		return -EIO;
	}

	for (i = 0; i < RTCPU_FW_HASH_SIZE; i++) {
		ret = tegra_camrtc_command(dev, RTCPU_COMMAND(FW_HASH, i), 0);
		value = RTCPU_GET_COMMAND_VALUE(ret);

		if (ret < 0 ||
			RTCPU_GET_COMMAND_ID(ret) != RTCPU_CMD_FW_HASH ||
			value > (u8)~0) {
			dev_warn(dev, "FW_HASH problem (response=0x%08x)", ret);
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
	seq_buf_printf(&s, "version cmd=%u ivc=%u sha1 ",
		rtcpu->fw_version, rtcpu->ivc_version);

	for (i = 0; i < RTCPU_FW_HASH_SIZE; i++)
		seq_buf_printf(&s, "%02x", rtcpu->fw_hash[i]);

	return seq_buf_used(&s);
}
EXPORT_SYMBOL(tegra_camrtc_print_version);

static void tegra_camrtc_log_fw_version(struct device *dev)
{
	char version[TEGRA_CAMRTC_VERSION_LEN];

	tegra_camrtc_print_version(dev, version, sizeof(version));

	dev_info(dev, "firmware %s", version);
}

static int tegra_cam_rtcpu_runtime_suspend(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->rtcpu_pdata;
	int i;

	if (pdata->clock_rates == NULL)
		return 0;

	for (i = 0; i < pdata->num_clocks; i++)
		clk_set_rate(rtcpu->clocks[i], pdata->clock_rates[i][0]);

	return 0;
}

static int tegra_cam_rtcpu_runtime_resume(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->rtcpu_pdata;
	int i;

	if (pdata->clock_rates == NULL)
		return 0;

	for (i = 0; i < pdata->num_clocks; i++)
		clk_set_rate(rtcpu->clocks[i], pdata->clock_rates[i][1]);

	return 0;
}

static int tegra_cam_rtcpu_remove(struct platform_device *pdev)
{
	struct tegra_cam_rtcpu *cam_rtcpu = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	if (cam_rtcpu->rtcpu_pdata->id == TEGRA_CAM_RTCPU_APE)
		tegra_pd_remove_device(&pdev->dev);

	tegra_ivc_bus_destroy(cam_rtcpu->ivc);
	tegra_ast_destroy(cam_rtcpu->ast);
	tegra_hsp_sm_pair_free(cam_rtcpu->sm_pair);
	return 0;
}

static int tegra_cam_rtcpu_probe(struct platform_device *pdev)
{
	struct tegra_cam_rtcpu *cam_rtcpu;
	struct device *dev = &pdev->dev;
	struct device_node *hsp_node;
	int ret;
	const struct of_device_id *match;

	dev_dbg(dev, "probing\n");

	cam_rtcpu = devm_kzalloc(dev, sizeof(*cam_rtcpu), GFP_KERNEL);
	if (!cam_rtcpu)
		return -ENOMEM;
	platform_set_drvdata(pdev, cam_rtcpu);

	match = of_match_device(tegra_cam_rtcpu_of_match, dev);
	if (match == NULL) {
		dev_err(dev, "Device match not found\n");
		return -ENODEV;
	}

	cam_rtcpu->rtcpu_pdata = match->data;

	ret = tegra_cam_rtcpu_get_clks_resets(dev);
	if (ret) {
		dev_err(dev, "failed to get clocks/resets: %d\n", ret);
		return ret;
	}

	if (cam_rtcpu->rtcpu_pdata->id == TEGRA_CAM_RTCPU_SCE) {
		/* EVP addresses are needed to set up the reset vector */
		/* EVP should be mapped if the (APE) FW is reloaded by kernel */

		/* SCE PM addr */
		cam_rtcpu->rtcpu_sce.sce_pm_base =
			tegra_ioremap_byname(dev, "sce-pm");
		if (!cam_rtcpu->rtcpu_sce.sce_pm_base) {
			dev_err(dev, "failed to map SCE PM space.\n");
			return -EINVAL;
		}

		/* SCE CFG addr */
		cam_rtcpu->rtcpu_sce.sce_cfg_base =
			tegra_ioremap_byname(dev, "sce-cfg");
		if (!cam_rtcpu->rtcpu_sce.sce_cfg_base) {
			dev_err(dev, "failed to map SCE CFG space.\n");
			return -EINVAL;
		}
	} else {
		/* APE power device add */
		tegra_pd_add_device(dev);
	}

	/* enable power */
	pm_runtime_enable(dev);

	ret = tegra_cam_rtcpu_apply_clks(dev, clk_prepare_enable);
	if (ret) {
		dev_err(dev, "failed to turn on clocks: %d\n", ret);
		return ret;
	}

	tegra_cam_rtcpu_apply_resets(dev, reset_control_deassert);

	/* set resume state */
	pm_runtime_get_sync(dev);

	mutex_init(&cam_rtcpu->cmd.mutex);
	init_waitqueue_head(&cam_rtcpu->cmd.response_waitq);
	init_waitqueue_head(&cam_rtcpu->cmd.empty_waitq);
	hsp_node = of_get_child_by_name(dev->of_node, "hsp");
	cam_rtcpu->sm_pair = of_tegra_hsp_sm_pair_by_name(hsp_node,
					"cmd-pair", tegra_camrtc_full_notify,
					tegra_camrtc_empty_notify, cam_rtcpu);
	of_node_put(hsp_node);
	if (IS_ERR(cam_rtcpu->sm_pair)) {
		ret = PTR_ERR(cam_rtcpu->sm_pair);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to obtain mbox pair: %d\n", ret);
		goto fail;
	}

	cam_rtcpu->ast = tegra_ast_create(dev);
	if (IS_ERR(cam_rtcpu->ast)) {
		ret = PTR_ERR(cam_rtcpu->ast);
		goto fail;
	}

	cam_rtcpu->tracer = tegra_rtcpu_trace_create(dev, cam_rtcpu->ast,
						cam_rtcpu->rtcpu_pdata->sid);

	ret = tegra_camrtc_boot(dev);
	if (ret)
		goto fail;

	ret = tegra_camrtc_get_fw_hash(dev, cam_rtcpu->fw_hash);
	if (ret == 0)
		devm_tegrafw_register(dev, "camrtc",
			TFW_NORMAL, tegra_camrtc_print_version, NULL);

	cam_rtcpu->ivc = tegra_ivc_bus_create(dev, cam_rtcpu->ast,
						cam_rtcpu->rtcpu_pdata->sid);
	if (IS_ERR(cam_rtcpu->ivc)) {
		ret = PTR_ERR(cam_rtcpu->ivc);
		goto fail;
	}

	ret = tegra_camrtc_ivc_setup_ready(dev);
	if (ret)
		goto fail;

	tegra_camrtc_log_fw_version(dev);

	/* set idle to slow down clock while idle mode */
	pm_runtime_idle(dev);

	dev_dbg(dev, "probe successful\n");

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
		dev_err(&pdev->dev, "failed to shutdown cam-rtcpu: %d\n", ret);
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
