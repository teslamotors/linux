// SPDX-License-Identifier: GPL-2.0+
//
// AMD ACP PCI Driver
//
//Copyright 2016 Advanced Micro Devices, Inc.

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include "acp3x.h"

struct acp3x_dev_data {
	void __iomem *acp3x_base;
	bool acp3x_audio_mode;
	struct resource *res;
	struct platform_device *pdev[3];
};

static int snd_acp3x_probe(struct pci_dev *pci,
			   const struct pci_device_id *pci_id)
{
	int ret;
	u32 addr, val;
	struct acp3x_dev_data *adata;
	struct platform_device_info pdevinfo;
	unsigned int irqflags;

	if (pci_enable_device(pci)) {
		dev_err(&pci->dev, "pci_enable_device failed\n");
		return -ENODEV;
	}

	ret = pci_request_regions(pci, "AMD ACP3x audio");
	if (ret < 0) {
		dev_err(&pci->dev, "pci_request_regions failed\n");
		goto disable_pci;
	}

	adata = devm_kzalloc(&pci->dev, sizeof(struct acp3x_dev_data),
			     GFP_KERNEL);
	if (!adata) {
		ret = -ENOMEM;
		goto release_regions;
	}

	irqflags = IRQF_SHARED;

	addr = pci_resource_start(pci, 0);
	adata->acp3x_base = ioremap(addr, pci_resource_len(pci, 0));
	if (!adata->acp3x_base) {
		ret = -ENOMEM;
		goto release_regions;
	}

	/* set pci bus-mastering */
	pci_set_master(pci);

	pci_set_drvdata(pci, adata);

	val = rv_readl(adata->acp3x_base + mmACP_I2S_PIN_CONFIG);
	switch (val) {
	case I2S_MODE:
		adata->res = devm_kzalloc(&pci->dev,
					  sizeof(struct resource) * 2,
					  GFP_KERNEL);
		if (!adata->res) {
			ret = -ENOMEM;
			goto unmap_mmio;
		}

		adata->res[0].name = "acp3x_i2s_iomem";
		adata->res[0].flags = IORESOURCE_MEM;
		adata->res[0].start = addr;
		adata->res[0].end = addr + (ACP3x_REG_END - ACP3x_REG_START);

		adata->res[1].name = "acp3x_i2s_irq";
		adata->res[1].flags = IORESOURCE_IRQ;
		adata->res[1].start = pci->irq;
		adata->res[1].end = pci->irq;

		adata->acp3x_audio_mode = ACP3x_I2S_MODE;

		memset(&pdevinfo, 0, sizeof(pdevinfo));
		pdevinfo.name = "acp3x_rv_i2s";
		pdevinfo.id = 0;
		pdevinfo.parent = &pci->dev;
		pdevinfo.num_res = 2;
		pdevinfo.res = adata->res;
		pdevinfo.data = &irqflags;
		pdevinfo.size_data = sizeof(irqflags);

		adata->pdev[0] = platform_device_register_full(&pdevinfo);
		if (IS_ERR(adata->pdev[0])) {
			dev_err(&pci->dev, "cannot register %s device\n",
				pdevinfo.name);
			ret = PTR_ERR(adata->pdev[0]);
			goto unmap_mmio;
		}

		/* create dummy codec device */
		adata->pdev[1] = platform_device_register_simple("dummy_w5102",
								 0, NULL, 0);
		if (IS_ERR(adata->pdev[1])) {
			dev_err(&pci->dev, "Cannot register dummy_w5102\n");
			ret = PTR_ERR(adata->pdev[1]);
			goto unregister_pdev0;
		}
		/* create dummy mach device */
		adata->pdev[2] =
			platform_device_register_simple("acp3x_w5102_mach", 0,
							NULL, 0);
		if (IS_ERR(adata->pdev[2])) {
			dev_err(&pci->dev, "Can't register acp3x_w5102_mach\n");
			ret = PTR_ERR(adata->pdev[2]);
			goto unregister_pdev1;
		}
		break;
	default:
		dev_err(&pci->dev, "Invalid ACP audio mode : %d\n", val);
		ret = -ENODEV;
		goto unmap_mmio;
	}
	return 0;

unregister_pdev1:
	platform_device_unregister(adata->pdev[1]);
unregister_pdev0:
	platform_device_unregister(adata->pdev[0]);
unmap_mmio:
	iounmap(adata->acp3x_base);
release_regions:
	pci_release_regions(pci);
disable_pci:
	pci_disable_device(pci);

	return ret;
}

static void snd_acp3x_remove(struct pci_dev *pci)
{
	struct acp3x_dev_data *adata = pci_get_drvdata(pci);
	int i;

	if (adata->acp3x_audio_mode == ACP3x_I2S_MODE) {
		for (i = 2; i >= 0; i--)
			platform_device_unregister(adata->pdev[i]);
	}
	iounmap(adata->acp3x_base);

	pci_release_regions(pci);
	pci_disable_device(pci);
}

static const struct pci_device_id snd_acp3x_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, 0x15e2),
	.class = PCI_CLASS_MULTIMEDIA_OTHER << 8,
	.class_mask = 0xffffff },
	{ 0, },
};
MODULE_DEVICE_TABLE(pci, snd_acp3x_ids);

static struct pci_driver acp3x_driver  = {
	.name = KBUILD_MODNAME,
	.id_table = snd_acp3x_ids,
	.probe = snd_acp3x_probe,
	.remove = snd_acp3x_remove,
};

module_pci_driver(acp3x_driver);

MODULE_AUTHOR("Maruthi.Bayyavarapu@amd.com");
MODULE_DESCRIPTION("AMD ACP3x PCI driver");
MODULE_LICENSE("GPL v2");
