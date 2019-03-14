// SPDX-License-Identifier: GPL-2.0
/*
 * soc-apci-intel-bxt-match.c - tables and support for BXT ACPI enumeration.
 *
 * Copyright (c) 2018, Intel Corporation.
 *
 */

#include <sound/soc-acpi.h>
#include <sound/soc-acpi-intel-match.h>

static struct snd_soc_acpi_codecs bxt_codecs = {
	.num_codecs = 1,
	.codecs = {"MX98357A"}
};

struct snd_soc_acpi_mach snd_soc_acpi_intel_bxt_machines[] = {
	{
		.id = "INT343A",
#if IS_ENABLED(CONFIG_SND_SOC_INTEL_BXTP_IVI_RSE_MACH)
		.drv_name = "bxt_ivi_rse_i2s",
		.fw_filename = "dsp_fw_release.bin",
#else
		.drv_name = "bxt_alc298s_i2s",
		.fw_filename = "intel/dsp_fw_bxtn.bin",
#endif
	},
	{
		.id = "DLGS7219",
		.drv_name = "bxt_da7219_max98357a",
		.fw_filename = "intel/dsp_fw_bxtn.bin",
		.machine_quirk = snd_soc_acpi_codec_list,
		.quirk_data = &bxt_codecs,
		.sof_fw_filename = "intel/sof-apl.ri",
		.sof_tplg_filename = "intel/sof-apl-da7219.tplg",
		.asoc_plat_name = "0000:00:0e.0",
	},
	{
		.id = "104C5122",
		.drv_name = "bxt-pcm512x",
		.sof_fw_filename = "intel/sof-apl.ri",
		.sof_tplg_filename = "intel/sof-apl-pcm512x.tplg",
		.asoc_plat_name = "0000:00:0e.0",
	},
	{
		.id = "1AEC8804",
		.drv_name = "bxt-wm8804",
		.sof_fw_filename = "intel/sof-apl.ri",
		.sof_tplg_filename = "intel/sof-apl-wm8804.tplg",
		.asoc_plat_name = "0000:00:0e.0",
	},
	{
		.id = "INT34C3",
#if IS_ENABLED(CONFIG_SND_SOC_INTEL_BXT_GPMRB_MACH)
		.drv_name = "gpmrb_machine",
		.fw_filename = "ADSPFW.bin",
#else
		.drv_name = "bxt_tdf8532",
		.fw_filename = "intel/dsp_fw_bxtn.bin",
		.sof_fw_filename = "intel/sof-apl.ri",
		.sof_tplg_filename = "intel/sof-apl-tdf8532.tplg",
		.asoc_plat_name = "0000:00:0e.0",
#endif
	},
	{
		.id = "INT344A",
		.drv_name = "bxt_ivi_hu_i2s",
		.fw_filename = "dsp_fw_release.bin",
	},
	{
		.id = "INT344B",
		.drv_name = "bxt_ivi_generic_i2s",
		.fw_filename = "dsp_fw_release.bin",
	},
	{
		.id = "INT345A",
		.drv_name = "lhcrb_wm8731_i2s",
		.fw_filename = "dsp_fw_release.bin",
	},
	{
		.id = "INT345B",
		.drv_name = "lhcrb_aic3107M_i2s",
		.fw_filename = "dsp_fw_release.bin",
	},
	{
		.id = "INT345C",
		.drv_name = "lhcrb_aic3107S_i2s",
		.fw_filename = "dsp_fw_release.bin",
	},
	{},
};
EXPORT_SYMBOL_GPL(snd_soc_acpi_intel_bxt_machines);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel Common ACPI Match module");
