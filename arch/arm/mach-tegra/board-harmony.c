/*
 * arch/arm/mach-tegra/board-harmony.c
 *
 * Copyright (C) 2010 Google, Inc.
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/pda_power.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/tegra_usb.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/usb_phy.h>
#include <asm/setup.h>

#include <mach/suspend.h>
#include <mach/audio.h>
#include <mach/i2s.h>
#include <mach/tegra_das.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/nand.h>
#include <mach/clk.h>
#include <mach/pci.h>

#include "clock.h"
#include "board.h"
#include "board-harmony.h"
#include "clock.h"
#include "devices.h"
#include "gpio-names.h"
#include <mach/spdif.h>

/* NVidia bootloader tags */
#define ATAG_NVIDIA		0x41000801

#define ATAG_NVIDIA_RM			0x1
#define ATAG_NVIDIA_DISPLAY		0x2
#define ATAG_NVIDIA_FRAMEBUFFER		0x3
#define ATAG_NVIDIA_CHIPSHMOO		0x4
#define ATAG_NVIDIA_CHIPSHMOOPHYS	0x5
#define ATAG_NVIDIA_PRESERVED_MEM_0	0x10000
#define ATAG_NVIDIA_PRESERVED_MEM_N	2
#define ATAG_NVIDIA_FORCE_32		0x7fffffff

#ifdef CONFIG_TEGRA_OSC_CRYSTAL_FREQ_12MHZ
#define OSC_CTL_FREQ 12000000
#else
#ifdef CONFIG_TEGRA_OSC_CRYSTAL_FREQ_13MHZ
#define OSC_CTL_FREQ 13000000
#endif
#endif

struct tag_tegra {
	__u32 bootarg_key;
	__u32 bootarg_len;
	char bootarg[1];
};

static int __init parse_tag_nvidia(const struct tag *tag)
{
	return 0;
}

__tagtable(ATAG_NVIDIA, parse_tag_nvidia);

static struct tegra_nand_chip_parms nand_chip_parms[] = {
	/* Samsung K5E2G1GACM */
	[0] = {
	       .vendor_id = 0xEC,
	       .device_id = 0xAA,
	       .read_id_fourth_byte = 0x15,
	       .capacity  = 256,
	       .timing = {
			  .trp = 21,
			  .trh = 15,
			  .twp = 21,
			  .twh = 15,
			  .tcs = 31,
			  .twhr = 60,
			  .tcr_tar_trr = 20,
			  .twb = 100,
			  .trp_resp = 30,
			  .tadl = 100,
			  },
	       },
	/* Hynix H5PS1GB3EFR */
	[1] = {
	       .vendor_id = 0xAD,
	       .device_id = 0xDC,
	       .read_id_fourth_byte = 0x95,
	       .capacity  = 512,
	       .timing = {
			  .trp = 12,
			  .trh = 10,
			  .twp = 12,
			  .twh = 10,
			  .tcs = 20,
			  .twhr = 80,
			  .tcr_tar_trr = 20,
			  .twb = 100,
			  .trp_resp = 20,
			  .tadl = 70,
			  },
	       },
};

struct tegra_nand_platform harmony_nand_data = {
	.max_chips = 8,
	.chip_parms = nand_chip_parms,
	.nr_chip_parms = ARRAY_SIZE(nand_chip_parms),
	.wp_gpio = TEGRA_GPIO_PC7,
};

static struct resource resources_nand[] = {
	[0] = {
	       .start = INT_NANDFLASH,
	       .end = INT_NANDFLASH,
	       .flags = IORESOURCE_IRQ,
	},
};

struct platform_device tegra_nand_device = {
	.name = "tegra_nand",
	.id = -1,
	.num_resources = ARRAY_SIZE(resources_nand),
	.resource = resources_nand,
	.dev = {
		.platform_data = &harmony_nand_data,
	},
};

static struct plat_serial8250_port debug_uart_platform_data[] = {
	{
	 .membase = IO_ADDRESS(TEGRA_UARTD_BASE),
	 .mapbase = TEGRA_UARTD_BASE,
	 .irq = INT_UARTD,
	 .flags = UPF_BOOT_AUTOCONF,
	 .iotype = UPIO_MEM,
	 .regshift = 2,
	 .uartclk = 216000000,
	 }, {
	     .flags = 0
	 }
};

static struct platform_device debug_uart = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uart_platform_data,
		},
};

/* PDA power */
static struct pda_power_pdata pda_power_pdata = {
};

static struct platform_device pda_power_device = {
	.name = "pda_power",
	.id = -1,
	.dev = {
		.platform_data = &pda_power_pdata,
		},
};

static struct platform_device tegra_rfkill =
{
	.name = "tegra_rfkill",
	.id   = -1,
};

static struct i2c_board_info harmony_i2c_bus1_board_info[] = {
	{
	 I2C_BOARD_INFO("wm8903", 0x1a),
	 },
};

static struct tegra_i2c_platform_data harmony_i2c1_platform_data = {
	.adapter_nr = 0,
	.bus_count = 1,
	.bus_clk_rate = {400000, 0},
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup = TEGRA_PINGROUP_DDC,
	.func = TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup = TEGRA_PINGROUP_PTA,
	.func = TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data harmony_i2c2_platform_data = {
	.adapter_nr = 1,
	.bus_count = 2,
	.bus_clk_rate = {400000, 100000},
	.bus_mux = {&i2c2_ddc, &i2c2_gen2},
	.bus_mux_len = {1, 1},
};

static struct tegra_i2c_platform_data harmony_i2c3_platform_data = {
	.adapter_nr = 3,
	.bus_count = 1,
	.bus_clk_rate = {400000, 0},
};

static struct tegra_i2c_platform_data harmony_dvc_platform_data = {
	.adapter_nr = 4,
	.bus_count = 1,
	.bus_clk_rate = {400000, 0},
	.is_dvc = true,
};

static struct tegra_utmip_config utmi_phy_config[] = {
    [0] = {
            .hssync_start_delay = 0,
            .idle_wait_delay = 17,
            .elastic_limit = 16,
            .term_range_adj = 6,
            .xcvr_setup = 15,
            .xcvr_lsfslew = 2,
            .xcvr_lsrslew = 2,
    },
    [1] = {
            .hssync_start_delay = 0,
            .idle_wait_delay = 17,
            .elastic_limit = 16,
            .term_range_adj = 6,
            .xcvr_setup = 9,
            .xcvr_lsfslew = 2,
            .xcvr_lsrslew = 2,
    },
};

static struct tegra_ulpi_config ulpi_usb2_config = {
	.reset_gpio = TEGRA_GPIO_PV1,
	.clk = "clk_dev2",
};

static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
    [0] = {
            .phy_config = &utmi_phy_config[0],
            .operating_mode = TEGRA_USB_DEVICE,
            .power_down_on_bus_suspend = 0,
    },
    [1] = {
            .phy_config = &ulpi_usb2_config,
            .operating_mode = TEGRA_USB_HOST,
            .power_down_on_bus_suspend = 1,
    },
    [2] = {
            .phy_config = &utmi_phy_config[1],
            .operating_mode = TEGRA_USB_HOST,
            .power_down_on_bus_suspend = 0,
    },
};

static struct tegra_audio_platform_data tegra_audio_pdata[] = {
        /* For I2S1 */
        [0] = {
                .dma_on         = true,  /* use dma by default */
                .i2s_clk_rate   = 240000000,
                .dap_clk        = "clk_dev1",
                .audio_sync_clk = "audio_2x",
                .mode           = I2S_BIT_FORMAT_I2S,
                .fifo_fmt       = I2S_FIFO_16_LSB,
                .bit_size       = I2S_BIT_SIZE_16,
        },
        /* For I2S2 */
        [1] = {
                .dma_on         = true,  /* use dma by default */
                .i2s_clk_rate   = 240000000,
                .dap_clk        = "clk_dev1",
                .audio_sync_clk = "audio_2x",
                .mode           = I2S_BIT_FORMAT_I2S,
                .fifo_fmt       = I2S_FIFO_16_LSB,
                .bit_size       = I2S_BIT_SIZE_16,
        }
};
static struct tegra_das_platform_data tegra_das_pdata = {
        .tegra_dap_port_info_table = {
                [0] = {
                        .dac_port = tegra_das_port_none,
                        .codec_type = tegra_audio_codec_type_none,
                        .device_property = {
                                .num_channels = 0,
                                .bits_per_sample = 0,
                                .rate = 0,
                                .dac_dap_data_comm_format = 0,
                        },
                },
                /* I2S1 <--> DAC1 <--> DAP1 <--> Hifi Codec */
                [1] = {
                        .dac_port = tegra_das_port_i2s1,
                        .codec_type = tegra_audio_codec_type_hifi,
                        .device_property = {
                                .num_channels = 2,
                                .bits_per_sample = 16,
                                .rate = 44100,
                                .dac_dap_data_comm_format = dac_dap_data_format_i2s,
                        },
                },
                [2] = {
                        .dac_port = tegra_das_port_none,
                        .codec_type = tegra_audio_codec_type_none,
                        .device_property = {
                                .num_channels = 0,
                                .bits_per_sample = 0,
                                .rate = 0,
                                .dac_dap_data_comm_format = 0,
                        },
                },
                [3] = {
                        .dac_port = tegra_das_port_none,
                        .codec_type = tegra_audio_codec_type_none,
                        .device_property = {
                                .num_channels = 0,
                                .bits_per_sample = 0,
                                .rate = 0,
                                .dac_dap_data_comm_format = 0,
                        },
                },
                [4] = {
                        .dac_port = tegra_das_port_none,
                        .codec_type = tegra_audio_codec_type_none,
                        .device_property = {
                                .num_channels = 0,
                                .bits_per_sample = 0,
                                .rate = 0,
                                .dac_dap_data_comm_format = 0,
                        },
                },
        },
  .tegra_das_con_table = {
                [0] = {
                        .con_id = tegra_das_port_con_id_hifi,
                        .num_entries = 2,
                        .con_line = {
                                [0] = {tegra_das_port_i2s1, tegra_das_port_dap1, true},
                                [1] = {tegra_das_port_dap1, tegra_das_port_i2s1, false},
                        },
                },
        }
};

static struct tegra_audio_platform_data tegra_spdif_audio_pdata = {
	.tdm_enable = true,
	.num_slots = 4,
	.dma_on = 0,
	.fifo_fmt = 0,
	.mode = SPDIF_BIT_MODE_MODE16BIT,
};

static void harmony_usb_init(void)
{
    tegra_ehci2_device.dev.platform_data=&tegra_ehci_pdata[1];
    tegra_ehci3_device.dev.platform_data=&tegra_ehci_pdata[2];
    platform_device_register(&tegra_ehci3_device);
}


static struct platform_device *harmony_devices[] __initdata = {
	&debug_uart,
	&tegra_uartc_device,
	&tegra_rfkill,
	&tegra_nand_device,
	&tegra_udc_device,
	&pda_power_device,
	&pmu_device,
	&tegra_spi_device1,
	&tegra_spi_device2,
	&tegra_spi_device3,
	&tegra_spi_device4,
	&tegra_gart_device,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_das_device,
	&tegra_avp_device,
#ifdef CONFIG_TEGRA_ALSA_SPDIF
	&tegra_spdif_input_device,
#endif
};

static void harmony_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &harmony_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &harmony_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &harmony_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &harmony_dvc_platform_data;

	i2c_register_board_info(0, harmony_i2c_bus1_board_info, 1);

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);
}

static void __init tegra_harmony_fixup(struct machine_desc *desc,
				       struct tag *tags, char **cmdline,
				       struct meminfo *mi)
{
	mi->nr_banks = 2;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = 448 * SZ_1M;
	mi->bank[1].start = SZ_512M;
	mi->bank[1].size = SZ_512M;
}

static __initdata struct tegra_clk_init_table harmony_clk_init_table[] = {
	/* name         parent          rate            enabled */
	{ "clk_dev1",   NULL,           26000000,       true},
	{ "clk_m",      NULL,           OSC_CTL_FREQ,   true},
	{ "3d",         "pll_m",        266400000,      true},
	{ "2d",         "pll_m",        266400000,      true},
	{ "vi",         "pll_m",        50000000,       true},
	{ "vi_sensor",  "pll_m",        111000000,      false},
	{ "epp",        "pll_m",        266400000,      true},
	{ "mpe",        "pll_m",        111000000,      false},
	{ "pll_c",      "clk_m",        600000000,      true},
	{ "pll_c_out1", "pll_c",        240000000,      true},
	{ "vde",        "pll_m",        240000000,      false},
	{ "pll_p",      "clk_m",        216000000,      true},
	{ "pll_p_out1", "pll_p",        28800000,       true},
	{ "pll_a",      "pll_p_out1",   56448000,       true},
	{ "pll_a_out0", "pll_a",        11289600,       true},
	{ "i2s1",       "pll_a_out0",   11289600,       true},
	{ "audio",      "pll_a_out0",   11289600,       true},
	{ "audio_2x",   "audio",        22579200,       false},
	{ "pll_p_out2", "pll_p",        48000000,       true},
	{ "pll_p_out3", "pll_p",        72000000,       true},
	{ "i2c1_i2c",   "pll_p_out3",   72000000,       true},
	{ "i2c2_i2c",   "pll_p_out3",   72000000,       true},
	{ "i2c3_i2c",   "pll_p_out3",   72000000,       true},
	{ "dvc_i2c",    "pll_p_out3",   72000000,       true},
	{ "csi",        "pll_p_out3",   72000000,       false},
	{ "pll_p_out4", "pll_p",        108000000,      true},
	{ "sclk",       "pll_p_out4",   108000000,      true},
	{ "hclk",       "sclk",         108000000,      true},
	{ "pclk",       "hclk",         54000000,       true},
	{ "spdif_in",   "pll_p",        36000000,       false},
	{ "csite",      "pll_p",        144000000,      true},
	{ "uartd",      "pll_p",        216000000,      true},
	{ "host1x",     "pll_p",        144000000,      true},
	{ "disp1",      "pll_p",        216000000,      true},
	{ "pll_d",      "clk_m",        594000000,      false},
	{ "pll_d_out0", "pll_d",        297000000,      false},
	{ "dsi",        "pll_d",        594000000,      false},
	{ "pll_u",      "clk_m",        OSC_CTL_FREQ*40,true},
	{ "clk_d",      "clk_m",        OSC_CTL_FREQ*2, true},
	{ "timer",      "clk_m",        OSC_CTL_FREQ,   true},
	{ "i2s2",       "clk_m",        11289600,       false},
	{ "spdif_out",  "clk_m",        OSC_CTL_FREQ,   false},
	{ "spi",        "clk_m",        OSC_CTL_FREQ,   false},
	{ "xio",        "clk_m",        OSC_CTL_FREQ,   false},
	{ "twc",        "clk_m",        OSC_CTL_FREQ,   false},
	{ "sbc1",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "sbc2",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "sbc3",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "sbc4",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "ide",        "clk_m",        OSC_CTL_FREQ,   false},
	{ "ndflash",    "clk_m",        108000000,      true},
	{ "vfir",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "sdmmc1",     "pll_p",        24000000,       true},
	{ "sdmmc2",     "pll_p",        48000000,       true},
	{ "sdmmc3",     "pll_p",        48000000,       false},
	{ "sdmmc4",     "pll_p",        48000000,       true},
	{ "la",         "clk_m",        OSC_CTL_FREQ,   false},
	{ "owr",        "clk_m",        OSC_CTL_FREQ,   false},
	{ "nor",        "clk_m",        12000000,       false},
	{ "mipi",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "i2c1",       "clk_m",        3000000,        false},
	{ "i2c2",       "clk_m",        3000000,        false},
	{ "i2c3",       "clk_m",        3000000,        false},
	{ "dvc",        "clk_m",        3000000,        false},
	{ "uarta",      "clk_m",        OSC_CTL_FREQ,   false},
	{ "uartb",      "clk_m",        OSC_CTL_FREQ,   false},
	{ "uartc",      "pll_p",        216000000,      true},
	{ "uarte",      "clk_m",        OSC_CTL_FREQ,   false},
	{ "cve",        "clk_m",        OSC_CTL_FREQ,   false},
	{ "tvo",        "clk_m",        OSC_CTL_FREQ,   false},
	{ "hdmi",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "tvdac",      "clk_m",        OSC_CTL_FREQ,   true},
	{ "disp2",      "clk_m",        OSC_CTL_FREQ,   true},
	{ "usbd",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "usb2",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "usb3",       "clk_m",        OSC_CTL_FREQ,   true},
	{ "isp",        "clk_m",        OSC_CTL_FREQ,   false},
	{ "csus",       "clk_m",        OSC_CTL_FREQ,   false},
	{ "pwm",        "clk_32k",      32768,          false},
	{ "clk_32k",    NULL,           32768,          true},
	{ "pll_s",      "clk_32k",      32768,          false},
	{ "rtc",        "clk_32k",      32768,          true},
	{ "kbc",        "clk_32k",      32768,          true},
	{ "spdif_in",	"pll_m",		22579000,		true},
	{ "spdif_out",	"pll_a_out0",	5644800,		true},
	{ NULL,         NULL,           0,              0},
};

static struct tegra_suspend_platform_data harmony_suspend = {
	.cpu_timer = 5000,
	.cpu_off_timer = 5000,
	.core_timer = 0x7e7e,
	.core_off_timer = 0x7f,
	.separate_req = true,
	.corereq_high = false,
	.sysclkreq_high = true,
	.suspend_mode = TEGRA_SUSPEND_LP0,
};

#if defined(CONFIG_TEGRA_PCI)
static struct tegra_pci_platform_data harmony_pci_platform_data = {
	.port_status[0] = 1,
	.port_status[1] = 1,
	.use_dock_detect = 0,
	.gpio           = TEGRA_NR_GPIOS+2,
};

static void harmony_pcie_init(void)
{
	tegra_pci_device.dev.platform_data = &harmony_pci_platform_data;
	platform_device_register(&tegra_pci_device);
}
#endif

static void __init tegra_harmony_init(void)
{
	tegra_common_init();

	tegra_init_suspend(&harmony_suspend);

	tegra_clk_init_from_table(harmony_clk_init_table);

	harmony_pinmux_init();
	harmony_regulator_init();

	tegra_i2s_device1.dev.platform_data = &tegra_audio_pdata[0];
	tegra_i2s_device2.dev.platform_data = &tegra_audio_pdata[1];
	tegra_das_device.dev.platform_data = &tegra_das_pdata;
	tegra_spdif_input_device.name = "spdif";
	tegra_spdif_input_device.dev.platform_data = &tegra_spdif_audio_pdata;

	platform_add_devices(harmony_devices, ARRAY_SIZE(harmony_devices));

	harmony_usb_init();
	harmony_panel_init();
	harmony_sdhci_init();
	harmony_i2c_init();
	harmony_power_off_init();
#if defined(CONFIG_TEGRA_PCI)
	harmony_pcie_init();
#endif
}

MACHINE_START(HARMONY, "harmony")
    .boot_params  = 0x00000100,
    .phys_io      = IO_APB_PHYS,
    .io_pg_offst  = ((IO_APB_VIRT) >> 18) & 0xfffc,
    .fixup        = tegra_harmony_fixup,
    .init_irq     = tegra_init_irq,
    .init_machine = tegra_harmony_init,
    .map_io       = tegra_map_common_io,
    .timer        = &tegra_timer,
MACHINE_END
