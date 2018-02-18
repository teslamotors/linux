 /*
 * arch/arm64/mach-tegra/board-t210ref.c
 *
 * Copyright (c) 2013-2016, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/i2c/i2c-hid.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/spi/spi.h>
#include <linux/spi/rm31080a_ts.h>
#include <linux/maxim_sti.h>
#include <linux/memblock.h>
#include <linux/spi/spi-tegra.h>
#include <linux/nfc/pn544.h>
#include <linux/rfkill-gpio.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/regulator/consumer.h>
#include <linux/smb349-charger.h>
#include <linux/max17048_battery.h>
#include <linux/leds.h>
#include <linux/platform_data/at24.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/platform_data/serial-tegra.h>
#include <linux/edp.h>
#include <linux/usb/tegra_usb_phy.h>
#include <linux/mfd/palmas.h>
#include <linux/clk/tegra.h>
#include <media/tegra_dtv.h>
#include <linux/clocksource.h>
#include <linux/irqchip.h>
#include <linux/irqchip/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/platform_data/tegra_usb_modem_power.h>
#include <linux/platform_data/tegra_ahci.h>
#include <linux/irqchip/tegra.h>
#include <sound/max98090.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-tegra.h>
#include <linux/tegra_nvadsp.h>
#include <linux/tegra-pm.h>
#include <linux/regulator/machine.h>
#include <linux/of_fdt.h>

#include <mach/irqs.h>
#include <mach/io_dpd.h>
#include <linux/platform/tegra/isomgr.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/dc.h>
#include <mach/tegra_usb_pad_ctrl.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/xusb.h>

#include "board.h"
#include "board-panel.h"
#include "board-common.h"
#include "board-touch-raydium.h"
#include "board-touch-maxim_sti.h"
#include <linux/platform/tegra/clock.h>
#include <linux/platform/tegra/common.h>
#include "devices.h"
#include "gpio-names.h"
#include "iomap.h"
#include "pm.h"
#include <mach/tegra-board-id.h>
#include "tegra-of-dev-auxdata.h"
#include <linux/platform/tegra/dvfs.h>
#include <linux/platform/tegra/tegra12_emc.h>
#include "board-t210.h"

#define UTMI1_PORT_OWNER_XUSB 0x1

static struct tegra_usb_platform_data tegra_udc_pdata;
static struct tegra_usb_otg_data tegra_otg_pdata;

static bool disable_fb;

static int parse_disable_fb(char *buf)
{
	disable_fb = true;
	return 0;
}
early_param("t210_nofb", parse_disable_fb);

static void t210ref_usb_init(void)
{
	int usb_port_owner_info = tegra_get_usb_port_owner_info();

	if (!(usb_port_owner_info & UTMI1_PORT_OWNER_XUSB)) {
		tegra_otg_pdata.is_xhci = false;
		tegra_udc_pdata.u_data.dev.is_xhci = false;
	} else {
		tegra_otg_pdata.is_xhci = true;
		tegra_udc_pdata.u_data.dev.is_xhci = true;
	}
}

static struct of_dev_auxdata t210ref_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("nvidia,tegra210-sdhci", TEGRA_SDMMC1_BASE,
			"sdhci-tegra.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-sdhci", TEGRA_SDMMC2_BASE,
			"sdhci-tegra.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-sdhci", TEGRA_SDMMC3_BASE,
			"sdhci-tegra.2", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-sdhci", TEGRA_SDMMC4_BASE,
			"sdhci-tegra.3", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-udc", TEGRA_USB_BASE,
			"tegra-udc.0", &tegra_udc_pdata.u_data.dev),
	OF_DEV_AUXDATA("nvidia,tegra132-otg", TEGRA_USB_BASE,
			"tegra-otg", &tegra_otg_pdata),
	OF_DEV_AUXDATA("nvidia,tegra132-ehci", TEGRA_USB_BASE,
			"tegra-ehci.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-usb-cd", TEGRA_XUSB_PADCTL_BASE,
			"tegra-usb-cd", NULL),
	OF_DEV_AUXDATA("nvidia,tegra132-ehci", 0x7d004000, "tegra-ehci.1",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-host1x", TEGRA_HOST1X_BASE, "host1x",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-gm20b", TEGRA_GK20A_BAR0_BASE,
			"gpu.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-nvenc", TEGRA_NVENC_BASE, "msenc",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-vi", TEGRA_VI_BASE, "vi",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-isp", TEGRA_ISP_BASE, "isp.0",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-isp", TEGRA_ISPB_BASE, "isp.1",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-tsec", TEGRA_TSEC_BASE, "tsec",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-tsec", TEGRA_TSECB_BASE, "tsecb",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-vic", TEGRA_VIC_BASE, "vic03",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-se", 0x70012000, "tegra21-se",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-dtv", 0x7000c300, "dtv", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-nvdec", TEGRA_NVDEC_BASE, "nvdec",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-nvjpg", TEGRA_NVJPG_BASE, "nvjpg",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-virt-pcm", 0,
			"tegra210-virt-pcm", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-axbar",  TEGRA_AXBAR_BASE,
			"tegra210-axbar", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-adma",  TEGRA_ADMA_BASE,
			"tegra210-adma", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-qspi", 0x70410000, "qspi",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-ahci-sata", 0x70021000, "tegra-sata.0",
		NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-adsp-audio", 0, "adsp_audio.3", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-efuse", TEGRA_FUSE_BASE, "tegra-fuse",
			NULL),
	OF_DEV_AUXDATA("linux,spdif-dit", 0, "spdif-dit.0", NULL),
	OF_DEV_AUXDATA("linux,spdif-dit", 1, "spdif-dit.1", NULL),
	OF_DEV_AUXDATA("linux,spdif-dit", 2, "spdif-dit.2", NULL),
	OF_DEV_AUXDATA("linux,spdif-dit", 3, "spdif-dit.3", NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-xhci", 0x70090000, "tegra-xhci",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-xudc", 0x700D0000, "tegra-xudc",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-camera", 0, "pcl-generic",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-dc", TEGRA_DISPLAY_BASE, "tegradc.0",
			NULL),
	OF_DEV_AUXDATA("nvidia,tegra210-dc", TEGRA_DISPLAY2_BASE, "tegradc.1",
			NULL),
	OF_DEV_AUXDATA("pwm-backlight", 0, "pwm-backlight", NULL),
#ifdef CONFIG_TEGRA_CEC_SUPPORT
	OF_DEV_AUXDATA("nvidia,tegra210-cec", 0x70015000, "tegra_cec", NULL),
#endif
	OF_DEV_AUXDATA("nvidia,tegra-audio-rt5639", 0x0, "tegra-snd-rt5639",
			NULL),
	OF_DEV_AUXDATA("nvidia,icera-i500", 0, "tegra_usb_modem_power", NULL),
	OF_DEV_AUXDATA("raydium,rm_ts_spidev", 0, "rm_ts_spidev", NULL),
	OF_DEV_AUXDATA("nvidia,tegra30-hda", 0x70030000, "tegra30-hda", NULL),
	OF_DEV_AUXDATA("nvidia,ptm", 0x72010000, "ptm", NULL),
	{}
};

static void __init tegra_t210ref_early_init(void)
{
	if (!tegra_platform_is_fpga()) {
		tegra_clk_init_from_dt("t210-clk-init-table");
		tegra_clk_verify_parents();
	}
	if (of_machine_is_compatible("nvidia,e2141"))
		tegra_soc_device_init("e2141");
	else if (of_machine_is_compatible("nvidia,e2220"))
		tegra_soc_device_init("e2220");
	else if (of_machine_is_compatible("nvidia,e2190"))
		tegra_soc_device_init("e2190");
	else if (of_machine_is_compatible("nvidia,grenada"))
		tegra_soc_device_init("grenada");
	else if (of_machine_is_compatible("nvidia,loki-e"))
		tegra_soc_device_init("loki_e");
	else if (of_machine_is_compatible("nvidia,foster-e"))
		tegra_soc_device_init("foster_e");
	else if (of_machine_is_compatible("nvidia,jetson-e"))
		tegra_soc_device_init("jetson_e");
	else if (of_machine_is_compatible("nvidia,jetson-cv"))
		tegra_soc_device_init("jetson_cv");
	else if (of_machine_is_compatible("nvidia,vcm31-t210"))
		tegra_soc_device_init("vcm31-t210");
	else if (of_machine_is_compatible("nvidia,p2382_t210"))
		tegra_soc_device_init("p2382_t210");
	else if (of_machine_is_compatible("nvidia,drive-px"))
		tegra_soc_device_init("drive-px");
	else if (of_machine_is_compatible("nvidia,t18x-interposer"))
		tegra_soc_device_init("t18x-interposer");
	else if (of_machine_is_compatible("nvidia,e2580"))
		tegra_soc_device_init("e2580");
	else if (of_machine_is_compatible("nvidia,he2290"))
		tegra_soc_device_init("he2290");
	else if (of_machine_is_compatible("nvidia,maui"))
		tegra_soc_device_init("maui");
	else if (of_machine_is_compatible("nvidia,m3402"))
		tegra_soc_device_init("m3402");
}

static struct tegra_suspend_platform_data t210ref_suspend_data = {
	.cpu_timer      = 1700,
	.cpu_off_timer  = 300,
	.suspend_mode   = TEGRA_SUSPEND_LP0,
	.core_timer     = 0x257e,
	.core_off_timer = 640,
	.cpu_suspend_freq = 204000,
	.corereq_high   = true,
	.sysclkreq_high = true,
};

static int __init t210ref_suspend_init(void)
{
	tegra_init_suspend(&t210ref_suspend_data);
	return 0;
}

static void __init tegra_t210ref_late_init(void)
{
	struct board_info board_info;
	struct device_node *np = NULL;

	tegra_get_board_info(&board_info);
	pr_info("board_info: id:sku:fab:major:minor = 0x%04x:0x%04x:0x%02x:0x%02x:0x%02x\n",
		board_info.board_id, board_info.sku,
		board_info.fab, board_info.major_revision,
		board_info.minor_revision);

	t210ref_usb_init();
	tegra_io_dpd_init();
	/* FIXME: Assumed all t210ref platforms have sdhci DT support */
	t210ref_suspend_init();

	tegra21_emc_init();
	isomgr_init();

	np = of_find_node_by_path("/host1x");
	if (!disable_fb && np && of_device_is_available(np))
		tegra_fb_copy_or_clear();

	/* put PEX pads into DPD mode to save additional power */
	t210ref_camera_init();
}

static void __init tegra_t210ref_init_early(void)
{
	tegra21x_init_early();
}

static int tegra_t210ref_notifier_call(struct notifier_block *nb,
				    unsigned long event, void *data)
{
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	struct device *dev = data;
#endif

	switch (event) {
	case BUS_NOTIFY_BIND_DRIVER:
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		if (dev->of_node) {
			if (of_device_is_compatible(dev->of_node,
				"pwm-backlight")) {
				tegra_pwm_bl_ops_register(dev);
			}
		}
#endif
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static int tegra_t210ref_i2c_notifier_call(struct notifier_block *nb,
			unsigned long event, void *data)
{
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	struct device *dev = data;
#endif

	switch (event) {
	case BUS_NOTIFY_BIND_DRIVER:
#ifndef CONFIG_TEGRA_HDMI_PRIMARY
		if (dev->of_node) {
			if (of_device_is_compatible(dev->of_node,
					"ti,lp8550") ||
				of_device_is_compatible(dev->of_node,
					"ti,lp8551") ||
				of_device_is_compatible(dev->of_node,
					"ti,lp8552") ||
				of_device_is_compatible(dev->of_node,
					"ti,lp8553") ||
				of_device_is_compatible(dev->of_node,
					"ti,lp8554") ||
				of_device_is_compatible(dev->of_node,
					"ti,lp8555") ||
				of_device_is_compatible(dev->of_node,
					"ti,lp8556") ||
				of_device_is_compatible(dev->of_node,
					"ti,lp8557")) {
					ti_lp855x_bl_ops_register(dev);
			}
		}
#endif
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}


static struct notifier_block platform_nb = {
	.notifier_call = tegra_t210ref_notifier_call,
};

static struct notifier_block i2c_nb = {
	.notifier_call = tegra_t210ref_i2c_notifier_call,
};

static void __init tegra_t210ref_dt_init(void)
{
	unsigned long pinmux_clamp = readl(IO_ADDRESS(0x70000040));

	/* Check for the PINMUX CLAMP Settings */
	if (pinmux_clamp & 1)
		pr_warn("WARN: pinmux CLAMP_INPUTS_WHEN_TRISTATED: enabled\n");
	else
		pr_warn("INFO: pinmux CLAMP_INPUTS_WHEN_TRISTATED: disabled\n");

	regulator_has_full_constraints();

#ifndef CONFIG_TEGRA_HDMI_PRIMARY
	/*
	 * In t210ref, zero display_board_id is considered to
	 * jdi 1440x810 5.8" one.
	 */
	tegra_set_fixed_panel_ops(true, &dsi_j_1440_810_5_8_ops,
		"j,1440-810-5-8");
	tegra_set_fixed_pwm_bl_ops(dsi_j_1440_810_5_8_ops.pwm_bl_ops);
#endif
	bus_register_notifier(&platform_bus_type, &platform_nb);
	bus_register_notifier(&i2c_bus_type, &i2c_nb);
	tegra_t210ref_early_init();
	of_platform_populate(NULL,
		of_default_bus_match_table, t210ref_auxdata_lookup,
		&platform_bus);

	tegra_t210ref_late_init();
}

static void __init tegra_t210ref_reserve(void)
{
#ifdef CONFIG_TEGRA_HDMI_PRIMARY
	ulong tmp;
#endif /* CONFIG_TEGRA_HDMI_PRIMARY */

#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM) || \
		defined(CONFIG_TEGRA_NO_CARVEOUT)
	ulong carveout_size = 0;
	ulong fb2_size = SZ_64M + SZ_8M;
#else
	ulong carveout_size = SZ_1G;
	ulong fb2_size = SZ_4M;
#endif
	ulong fb1_size = SZ_64M + SZ_8M;
	ulong vpr_size = 364 * SZ_1M;
	if (of_flat_dt_is_compatible(of_get_flat_dt_root(), "nvidia,foster-e"))
		vpr_size = 672 * SZ_1M;
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
	/* support FBcon on 4K monitors */
	fb2_size = SZ_64M + SZ_8M;	/* 4096*2160*4*2 = 70778880 bytes */
#endif /* CONFIG_FRAMEBUFFER_CONSOLE */

#ifdef CONFIG_TEGRA_HDMI_PRIMARY
	tmp = fb1_size;
	fb1_size = fb2_size;
	fb2_size = tmp;
#endif /* CONFIG_TEGRA_HDMI_PRIMARY */

	tegra_reserve4(carveout_size, fb1_size, fb2_size, vpr_size);
}

static const char * const t210ref_dt_board_compat[] = {
	"nvidia,e2220",
	"nvidia,e2190",
	"nvidia,e2141",
	"nvidia,grenada",
	"nvidia,loki-e",
	"nvidia,foster-e",
	"nvidia,jetson-e",
	"nvidia,jetson-cv",
	"nvidia,vcm31-t210",
	"nvidia,p2382_t210",
	"nvidia,drive-px",
	"nvidia,t18x-interposer",
	"nvidia,e2580",
	"nvidia,he2290",
	"nvidia,maui",
	"nvidia,m3402",
	NULL
};

DT_MACHINE_START(T210REF, "t210ref")
	.atag_offset	= 0x100,
	.map_io		= tegra_map_common_io,
	.reserve	= tegra_t210ref_reserve,
	.init_early	= tegra_t210ref_init_early,
	.init_time	= clocksource_of_init,
	.init_machine	= tegra_t210ref_dt_init,
	.dt_compat	= t210ref_dt_board_compat,
	.init_late      = tegra_init_late,
MACHINE_END
