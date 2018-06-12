// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <sound/pcm.h>
#include <sound/sof.h>
#include <linux/pci.h>
#include <linux/acpi.h>
#include "sof-priv.h"

#if IS_ENABLED(CONFIG_SND_SOC_SOF_APOLLOLAKE)
static struct snd_soc_acpi_mach sof_bxt_machines[] = {
	{
		.id = "DLGS7219",
		.drv_name = "bxt_da7219_max98357a_i2s",
		.sof_fw_filename = "intel/reef-apl.ri",
		.sof_tplg_filename = "intel/reef-apl.tplg",
		.asoc_plat_name = "0000:00:0e.0",
	},
	{
		.id = "INT34C3",
		.drv_name = "bxt_tdf8532",
		.sof_fw_filename = "intel/reef-apl.ri",
		.sof_tplg_filename = "intel/reef-apl.tplg",
		.asoc_plat_name = "0000:00:0e.0",
	},
};

static struct sof_dev_desc bxt_desc = {
	.machines		= sof_bxt_machines,
	.resindex_lpe_base	= 0,
	.resindex_pcicfg_base	= -1,
	.resindex_imr_base	= -1,
	.irqindex_host_ipc	= -1,
	.resindex_dma_base	= -1,
	.nocodec_fw_filename = "intel/reef-apl.ri",
	.nocodec_tplg_filename = "intel/reef-apl-nocodec.tplg"
};
#endif

#if IS_ENABLED(CONFIG_SND_SOC_SOF_BAYTRAIL)
static struct snd_soc_acpi_mach sof_byt_machines[] = {
	{
		.id = "INT343A",
		.drv_name = "edison",
		.sof_fw_filename = "intel/reef-byt.ri",
		.sof_tplg_filename = "intel/reef-byt.tplg",
		.asoc_plat_name = "baytrail-pcm-audio",
	},
	{}
};

static const struct sof_dev_desc byt_desc = {
	.machines		= sof_byt_machines,
	.resindex_lpe_base	= 3,	/* IRAM, but subtract IRAM offset */
	.resindex_pcicfg_base	= -1,
	.resindex_imr_base	= 0,
	.irqindex_host_ipc	= -1,
	.resindex_dma_base	= -1,
	.nocodec_fw_filename = "intel/reef-byt.ri",
	.nocodec_tplg_filename = "intel/reef-byt.tplg"
};
#endif

#if IS_ENABLED(CONFIG_SND_SOC_SOF_CANNONLAKE)
static struct snd_soc_acpi_mach sof_cnl_machines[] = {
	{
		.id = "INT34C2",
		.drv_name = "cnl_alc700_i2s",
		.sof_fw_filename = "intel/reef-cnl.ri",
		.sof_tplg_filename = "intel/reef-cnl.tplg",
		.asoc_plat_name = "0000:00:0e.0",
	},
	{}
};

static const struct sof_dev_desc cnl_desc = {
	.machines		= sof_cnl_machines,
	.resindex_lpe_base	= 0,
	.resindex_pcicfg_base	= -1,
	.resindex_imr_base	= -1,
	.irqindex_host_ipc	= -1,
	.resindex_dma_base	= -1,
	.nocodec_fw_filename = "intel/reef-cnl.ri",
	.nocodec_tplg_filename = "intel/reef-cnl.tplg"
};
#endif

struct sof_pci_priv {
	struct snd_sof_pdata *sof_pdata;
	struct platform_device *pdev_pcm;
};

static void sof_pci_fw_cb(const struct firmware *fw, void *context)
{
	struct sof_pci_priv *priv = context;
	struct snd_sof_pdata *sof_pdata = priv->sof_pdata;
	const struct snd_soc_acpi_mach *mach = sof_pdata->machine;
	struct device *dev = sof_pdata->dev;

	sof_pdata->fw = fw;
	if (!fw) {
		dev_err(dev, "Cannot load firmware %s\n",
			mach->sof_fw_filename);
		return;
	}

	/* register PCM and DAI driver */
	priv->pdev_pcm =
		platform_device_register_data(dev, "sof-audio", -1,
					      sof_pdata, sizeof(*sof_pdata));
	if (IS_ERR(priv->pdev_pcm)) {
		dev_err(dev, "Cannot register device sof-audio. Error %d\n",
			(int)PTR_ERR(priv->pdev_pcm));
	}
}

static const struct dev_pm_ops sof_pci_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(snd_sof_suspend, snd_sof_resume)
	SET_RUNTIME_PM_OPS(snd_sof_runtime_suspend, snd_sof_runtime_resume,
			   NULL)
	.suspend_late = snd_sof_suspend_late,
};

static const struct sof_ops_table mach_ops[] = {
#if IS_ENABLED(CONFIG_SND_SOC_SOF_APOLLOLAKE)
	{&bxt_desc, &snd_sof_apl_ops},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SOF_BAYTRAIL)
	{&byt_desc, &snd_sof_byt_ops},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SOF_CANNONLAKE)
	{&cnl_desc, &snd_sof_cnl_ops},
#endif
};

static struct snd_sof_dsp_ops *sof_pci_get_ops(const struct sof_dev_desc *d)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mach_ops); i++) {
		if (d == mach_ops[i].desc)
			return mach_ops[i].ops;
	}

	/* not found */
	return NULL;
}

static int sof_pci_probe(struct pci_dev *pci,
			 const struct pci_device_id *pci_id)
{
	struct device *dev = &pci->dev;
	const struct sof_dev_desc *desc =
		(const struct sof_dev_desc *)pci_id->driver_data;
	struct snd_soc_acpi_mach *mach;
	struct snd_sof_pdata *sof_pdata;
	struct sof_pci_priv *priv;
	struct snd_sof_dsp_ops *ops;
	int ret = 0;

	dev_dbg(&pci->dev, "PCI DSP detected");

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	pci_set_drvdata(pci, priv);

	sof_pdata = devm_kzalloc(dev, sizeof(*sof_pdata), GFP_KERNEL);
	if (!sof_pdata)
		return -ENOMEM;

	ret = pci_enable_device(pci);
	if (ret < 0)
		return ret;

	ret = pci_request_regions(pci, "Audio DSP");
	if (ret < 0)
		return ret;

	/* get ops for platform */
	ops = sof_pci_get_ops(desc);
	if (!ops) {
		dev_err(dev, "error: no matching PCI descriptor ops\n");
		return -ENODEV;
	}

#if IS_ENABLED(CONFIG_SND_SOC_SOF_FORCE_NOCODEC_MODE)
	/* force nocodec mode */
	dev_warn(dev, "Force to use nocodec mode\n");
	mach = devm_kzalloc(dev, sizeof(*mach), GFP_KERNEL);
	ret = sof_nocodec_setup(dev, sof_pdata, mach, desc);
	if (ret < 0)
		return ret;
#else
	/* find machine */
	mach = snd_soc_acpi_find_machine(desc->machines);
	if (!mach) {
#if IS_ENABLED(CONFIG_SND_SOC_SOF_NOCODEC)
		/* fallback to nocodec mode */
		dev_warn(dev, "No matching ASoC machine driver found - using nocodec\n");
		mach = devm_kzalloc(dev, sizeof(*mach), GFP_KERNEL);
		ret = sof_nocodec_setup(dev, sof_pdata, mach, desc);
		if (ret < 0)
			return ret;
#else
		dev_err(dev, "No matching ASoC machine driver found - aborting probe\n");
		return -ENODEV;
#endif
	}
#endif

	mach->pdata = ops;

	sof_pdata->id = pci_id->device;
	sof_pdata->name = pci_name(pci);
	sof_pdata->machine = mach;
	sof_pdata->desc = (struct sof_dev_desc *)pci_id->driver_data;
	priv->sof_pdata = sof_pdata;
	sof_pdata->pci = pci;
	sof_pdata->dev = &pci->dev;

	/* register machine driver */
	sof_pdata->pdev_mach =
		platform_device_register_data(dev, mach->drv_name, -1,
					      sof_pdata, sizeof(*sof_pdata));
	if (IS_ERR(sof_pdata->pdev_mach))
		return PTR_ERR(sof_pdata->pdev_mach);
	dev_dbg(dev, "created machine %s\n",
		dev_name(&sof_pdata->pdev_mach->dev));

	/* continue probing after firmware is loaded */
	dev_info(dev, "info: loading firmware %s\n", mach->sof_fw_filename);
	ret = request_firmware_nowait(THIS_MODULE, true, mach->sof_fw_filename,
				      dev, GFP_KERNEL, priv, sof_pci_fw_cb);
	if (ret) {
		platform_device_unregister(sof_pdata->pdev_mach);
		dev_err(dev, "error: failed to load firmware %s\n",
			mach->sof_fw_filename);
	}

	return ret;
}

static void sof_pci_shutdown(struct pci_dev *pci)
{
	snd_sof_shutdown(&pci->dev);
}

static void sof_pci_remove(struct pci_dev *pci)
{
	struct sof_pci_priv *priv = pci_get_drvdata(pci);
	struct snd_sof_pdata *sof_pdata = priv->sof_pdata;

	platform_device_unregister(sof_pdata->pdev_mach);
	if (!IS_ERR_OR_NULL(priv->pdev_pcm))
		platform_device_unregister(priv->pdev_pcm);
	release_firmware(sof_pdata->fw);
	pci_release_regions(pci);
}

/* PCI IDs */
static const struct pci_device_id sof_pci_ids[] = {
#if IS_ENABLED(CONFIG_SND_SOC_SOF_APOLLOLAKE)
	/* BXT-P & Apollolake */
	{ PCI_DEVICE(0x8086, 0x5a98),
		.driver_data = (unsigned long)&bxt_desc},
	{ PCI_DEVICE(0x8086, 0x1a98),
		.driver_data = (unsigned long)&bxt_desc},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SOF_BAYTRAIL)
	{ PCI_DEVICE(0x8086, 0x119a),
		.driver_data = (unsigned long)&byt_desc},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SOF_CANNONLAKE)
	{ PCI_DEVICE(0x8086, 0x9dc8),
		.driver_data = (unsigned long)&cnl_desc},
#endif
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, sof_pci_ids);

/* pci_driver definition */
static struct pci_driver snd_sof_pci_driver = {
	.name = "sof-audio-pci",
	.id_table = sof_pci_ids,
	.probe = sof_pci_probe,
	.remove = sof_pci_remove,
	.shutdown = sof_pci_shutdown,
	.driver = {
		.pm = &sof_pci_pm,
	},
};
module_pci_driver(snd_sof_pci_driver);

MODULE_LICENSE("Dual BSD/GPL");
