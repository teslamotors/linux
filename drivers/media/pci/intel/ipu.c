// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2013 - 2018 Intel Corporation

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/timer.h>
#include <linux/sched.h>

#include "ipu.h"
#include "ipu-buttress.h"
#include "ipu-platform.h"
#include "ipu-platform-buttress-regs.h"
#include "ipu-cpd.h"
#include "ipu-pdata.h"
#include "ipu-bus.h"
#include "ipu-mmu.h"
#include "ipu-platform-regs.h"
#include "ipu-platform-isys-csi2-reg.h"
#include "ipu-trace.h"

#define IPU_PCI_BAR		0

static struct ipu_bus_device *ipu_mmu_init(struct pci_dev *pdev,
					   struct device *parent,
					   struct ipu_buttress_ctrl *ctrl,
					   void __iomem *base,
					   const struct ipu_hw_variants *hw,
					   unsigned int nr, int mmid)
{
	struct ipu_mmu_pdata *pdata =
	    devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	unsigned int i;

	if (!pdata)
		return ERR_PTR(-ENOMEM);

	if (hw->nr_mmus > IPU_MMU_MAX_DEVICES)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < hw->nr_mmus; i++) {
		struct ipu_mmu_hw *pdata_mmu = &pdata->mmu_hw[i];
		const struct ipu_mmu_hw *src_mmu = &hw->mmu_hw[i];

		if (src_mmu->nr_l1streams > IPU_MMU_MAX_TLB_L1_STREAMS ||
		    src_mmu->nr_l2streams > IPU_MMU_MAX_TLB_L2_STREAMS)
			return ERR_PTR(-EINVAL);

		*pdata_mmu = *src_mmu;
		pdata_mmu->base = base + src_mmu->offset;
	}

	pdata->nr_mmus = hw->nr_mmus;
	pdata->mmid = mmid;

	return ipu_bus_add_device(pdev, parent, pdata, NULL, ctrl,
				  IPU_MMU_NAME, nr);
}

static struct ipu_bus_device *ipu_isys_init(struct pci_dev *pdev,
					    struct device *parent,
					    struct device *iommu,
					    void __iomem *base,
					    const struct ipu_isys_internal_pdata
					    *ipdata,
					    struct ipu_isys_subdev_pdata
					    *spdata, unsigned int nr)
{
	struct ipu_isys_pdata *pdata =
	    devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);

	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;
	pdata->ipdata = ipdata;
	pdata->spdata = spdata;

	return ipu_bus_add_device(pdev, parent, pdata, iommu, NULL,
				  IPU_ISYS_NAME, nr);
}

static struct ipu_bus_device *ipu_psys_init(struct pci_dev *pdev,
					    struct device *parent,
					    struct device *iommu,
					    void __iomem *base,
					    const struct ipu_psys_internal_pdata
					    *ipdata, unsigned int nr)
{
	struct ipu_psys_pdata *pdata =
	    devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);

	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->base = base;
	pdata->ipdata = ipdata;
	return ipu_bus_add_device(pdev, parent, pdata, iommu, NULL,
				  IPU_PSYS_NAME, nr);
}

int ipu_fw_authenticate(void *data, u64 val)
{
	struct ipu_device *isp = data;
	int ret;

	if (!isp->secure_mode)
		return -EINVAL;

	ret = ipu_buttress_reset_authentication(isp);
	if (ret) {
		dev_err(&isp->pdev->dev, "Failed to reset authentication!\n");
		return ret;
	}

	return ipu_buttress_authenticate(isp);
}
EXPORT_SYMBOL(ipu_fw_authenticate);
DEFINE_SIMPLE_ATTRIBUTE(authenticate_fops, NULL, ipu_fw_authenticate, "%llu\n");

#ifdef CONFIG_DEBUG_FS
static int resume_ipu_bus_device(struct ipu_bus_device *adev)
{
	struct device *dev = &adev->dev;
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (!pm || !pm->resume)
		return -EIO;

	return pm->resume(dev);
}

static int suspend_ipu_bus_device(struct ipu_bus_device *adev)
{
	struct device *dev = &adev->dev;
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (!pm || !pm->suspend)
		return -EIO;

	return pm->suspend(dev);
}

static int force_suspend_get(void *data, u64 *val)
{
	struct ipu_device *isp = data;
	struct ipu_buttress *b = &isp->buttress;

	*val = b->force_suspend;
	return 0;
}

static int force_suspend_set(void *data, u64 val)
{
	struct ipu_device *isp = data;
	struct ipu_buttress *b = &isp->buttress;
	int ret = 0;

	if (val == b->force_suspend)
		return 0;

	if (val) {
		b->force_suspend = 1;
		ret = suspend_ipu_bus_device(isp->psys_iommu);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to suspend psys\n");
			return ret;
		}
		ret = suspend_ipu_bus_device(isp->isys_iommu);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to suspend isys\n");
			return ret;
		}
		ret = pci_set_power_state(isp->pdev, PCI_D3hot);
		if (ret) {
			dev_err(&isp->pdev->dev,
				"Failed to suspend IUnit PCI device\n");
			return ret;
		}
	} else {
		ret = pci_set_power_state(isp->pdev, PCI_D0);
		if (ret) {
			dev_err(&isp->pdev->dev,
				"Failed to suspend IUnit PCI device\n");
			return ret;
		}
		ret = resume_ipu_bus_device(isp->isys_iommu);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to resume isys\n");
			return ret;
		}
		ret = resume_ipu_bus_device(isp->psys_iommu);
		if (ret) {
			dev_err(&isp->pdev->dev, "Failed to resume psys\n");
			return ret;
		}
		b->force_suspend = 0;
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(force_suspend_fops, force_suspend_get,
			force_suspend_set, "%llu\n");
/*
 * The sysfs interface for reloading cpd fw is there only for debug purpose,
 * and it must not be used when either isys or psys is in use.
 */
static int cpd_fw_reload(void *data, u64 val)
{
	struct ipu_device *isp = data;
	int rval = -EINVAL;

	if (isp->cpd_fw_reload)
		rval = isp->cpd_fw_reload(isp);
	if (!rval && isp->isys_fw_reload)
		rval = isp->isys_fw_reload(isp);

	return rval;
}

DEFINE_SIMPLE_ATTRIBUTE(cpd_fw_fops, NULL, cpd_fw_reload, "%llu\n");

#endif /* CONFIG_DEBUG_FS */

static int ipu_init_debugfs(struct ipu_device *isp)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *file;
	struct dentry *dir;

	dir = debugfs_create_dir(pci_name(isp->pdev), NULL);
	if (!dir)
		return -ENOMEM;

	file = debugfs_create_file("force_suspend", 0700, dir, isp,
				   &force_suspend_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("authenticate", 0700, dir, isp,
				   &authenticate_fops);
	if (!file)
		goto err;

	file = debugfs_create_file("cpd_fw_reload", 0700, dir, isp,
				   &cpd_fw_fops);
	if (!file)
		goto err;

	if (ipu_trace_debugfs_add(isp, dir))
		goto err;

	isp->ipu_dir = dir;

	if (ipu_buttress_debugfs_init(isp))
		goto err;

	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
#else
	return 0;
#endif /* CONFIG_DEBUG_FS */
}

static void ipu_remove_debugfs(struct ipu_device *isp)
{
	/*
	 * Since isys and psys debugfs dir will be created under ipu root dir,
	 * mark its dentry to NULL to avoid duplicate removal.
	 */
	debugfs_remove_recursive(isp->ipu_dir);
	isp->ipu_dir = NULL;
}

static int ipu_pci_config_setup(struct pci_dev *dev)
{
	u16 pci_command;
	int rval = pci_enable_msi(dev);

	if (rval) {
		dev_err(&dev->dev, "Failed to enable msi (%d)\n", rval);
		return rval;
	}

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	pci_command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER |
	    PCI_COMMAND_INTX_DISABLE;
	pci_write_config_word(dev, PCI_COMMAND, pci_command);

	return 0;
}

static void ipu_configure_vc_mechanism(struct ipu_device *isp)
{
	u32 val = readl(isp->base + BUTTRESS_REG_BTRS_CTRL);

	if (IPU_BTRS_ARB_STALL_MODE_VC0 == IPU_BTRS_ARB_MODE_TYPE_STALL)
		val |= BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC0;
	else
		val &= ~BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC0;

	if (IPU_BTRS_ARB_STALL_MODE_VC1 == IPU_BTRS_ARB_MODE_TYPE_STALL)
		val |= BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC1;
	else
		val &= ~BUTTRESS_REG_BTRS_CTRL_STALL_MODE_VC1;

	writel(val, isp->base + BUTTRESS_REG_BTRS_CTRL);
}

static int ipu_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct ipu_device *isp;
	phys_addr_t phys;
	void __iomem *const *iomap;
	void __iomem *isys_base = NULL;
	void __iomem *psys_base = NULL;
	struct ipu_buttress_ctrl *isys_ctrl, *psys_ctrl;
	unsigned int dma_mask = IPU_DMA_MASK;
	int rval;

	trace_printk("B|%d|TMWK\n", current->pid);

	isp = devm_kzalloc(&pdev->dev, sizeof(*isp), GFP_KERNEL);
	if (!isp)
		return -ENOMEM;

	isp->pdev = pdev;
	INIT_LIST_HEAD(&isp->devices);

	rval = pcim_enable_device(pdev);
	if (rval) {
		dev_err(&pdev->dev, "Failed to enable CI ISP device (%d)\n",
			rval);
		trace_printk("E|TMWK\n");
		return rval;
	}

	dev_info(&pdev->dev, "Device 0x%x (rev: 0x%x)\n",
		 pdev->device, pdev->revision);

	phys = pci_resource_start(pdev, IPU_PCI_BAR);

	rval = pcim_iomap_regions(pdev,
				  1 << IPU_PCI_BAR,
				  pci_name(pdev));
	if (rval) {
		dev_err(&pdev->dev, "Failed to I/O memory remapping (%d)\n",
			rval);
		trace_printk("E|TMWK\n");
		return rval;
	}
	dev_info(&pdev->dev, "physical base address 0x%llx\n", phys);

	iomap = pcim_iomap_table(pdev);
	if (!iomap) {
		dev_err(&pdev->dev, "Failed to iomap table (%d)\n", rval);
		trace_printk("E|TMWK\n");
		return -ENODEV;
	}

	isp->base = iomap[IPU_PCI_BAR];
	dev_info(&pdev->dev, "mapped as: 0x%p\n", isp->base);

	pci_set_drvdata(pdev, isp);
	pci_set_master(pdev);

	isp->cpd_fw_name = IPU_CPD_FIRMWARE_NAME;

	isys_base = isp->base + isys_ipdata.hw_variant.offset;
	psys_base = isp->base + psys_ipdata.hw_variant.offset;

	rval = pci_set_dma_mask(pdev, DMA_BIT_MASK(dma_mask));
	if (!rval)
		rval = pci_set_consistent_dma_mask(pdev,
						   DMA_BIT_MASK(dma_mask));
	if (rval) {
		dev_err(&pdev->dev, "Failed to set DMA mask (%d)\n", rval);
		trace_printk("E|TMWK\n");
		return rval;
	}

	rval = ipu_pci_config_setup(pdev);
	if (rval) {
		trace_printk("E|TMWK\n");
		return rval;
	}

	rval = devm_request_threaded_irq(&pdev->dev, pdev->irq,
					 ipu_buttress_isr,
					 ipu_buttress_isr_threaded,
					 IRQF_SHARED, IPU_NAME, isp);
	if (rval) {
		dev_err(&pdev->dev, "Requesting irq failed(%d)\n", rval);
		trace_printk("E|TMWK\n");
		return rval;
	}

	rval = ipu_buttress_init(isp);
	if (rval) {
		trace_printk("E|TMWK\n");
		return rval;
	}

	dev_info(&pdev->dev, "cpd file name: %s\n", isp->cpd_fw_name);

	rval = request_firmware(&isp->cpd_fw, isp->cpd_fw_name, &pdev->dev);
	if (rval) {
		dev_err(&isp->pdev->dev, "Requesting signed firmware failed\n");
		trace_printk("E|TMWK\n");
		return rval;
	}

	rval = ipu_cpd_validate_cpd_file(isp, isp->cpd_fw->data,
					 isp->cpd_fw->size);
	if (rval) {
		dev_err(&isp->pdev->dev, "Failed to validate cpd\n");
		goto out_ipu_bus_del_devices;
	}

	rval = ipu_trace_add(isp);
	if (rval)
		dev_err(&pdev->dev, "Trace support not available\n");

	/*
	 * NOTE Device hierarchy below is important to ensure proper
	 * runtime suspend and resume order.
	 * Also registration order is important to ensure proper
	 * suspend and resume order during system
	 * suspend. Registration order is as follows:
	 * isys_iommu->isys->psys_iommu->psys
	 */
	isys_ctrl = devm_kzalloc(&pdev->dev, sizeof(*isys_ctrl), GFP_KERNEL);
	if (!isys_ctrl) {
		rval = -ENOMEM;
		goto out_ipu_bus_del_devices;
	}

	/* Init butress control with default values based on the HW */
	memcpy(isys_ctrl, &isys_buttress_ctrl, sizeof(*isys_ctrl));

	isp->isys_iommu = ipu_mmu_init(pdev, &pdev->dev, isys_ctrl,
				       isys_base,
				       &isys_ipdata.hw_variant, 0, ISYS_MMID);
	rval = PTR_ERR(isp->isys_iommu);
	if (IS_ERR(isp->isys_iommu)) {
		dev_err(&pdev->dev, "can't create isys iommu device\n");
		rval = -ENOMEM;
		goto out_ipu_bus_del_devices;
	}

	isp->isys = ipu_isys_init(pdev, &isp->isys_iommu->dev,
				  &isp->isys_iommu->dev, isys_base,
				  &isys_ipdata, pdev->dev.platform_data, 0);
	rval = PTR_ERR(isp->isys);
	if (IS_ERR(isp->isys))
		goto out_ipu_bus_del_devices;

	psys_ctrl = devm_kzalloc(&pdev->dev, sizeof(*psys_ctrl), GFP_KERNEL);
	if (!psys_ctrl) {
		rval = -ENOMEM;
		goto out_ipu_bus_del_devices;
	}

	/* Init butress control with default values based on the HW */
	memcpy(psys_ctrl, &psys_buttress_ctrl, sizeof(*psys_ctrl));

	isp->psys_iommu = ipu_mmu_init(pdev,
				       isp->isys_iommu ?
				       &isp->isys_iommu->dev :
				       &pdev->dev, psys_ctrl, psys_base,
				       &psys_ipdata.hw_variant, 1, PSYS_MMID);
	rval = PTR_ERR(isp->psys_iommu);
	if (IS_ERR(isp->psys_iommu)) {
		dev_err(&pdev->dev, "can't create psys iommu device\n");
		goto out_ipu_bus_del_devices;
	}

	isp->psys = ipu_psys_init(pdev, &isp->psys_iommu->dev,
				  &isp->psys_iommu->dev, psys_base,
				  &psys_ipdata, 0);
	rval = PTR_ERR(isp->psys);
	if (IS_ERR(isp->psys))
		goto out_ipu_bus_del_devices;

	rval = ipu_init_debugfs(isp);
	if (rval) {
		dev_err(&pdev->dev, "Failed to initialize debugfs");
		goto out_ipu_bus_del_devices;
	}

	/* Configure the arbitration mechanisms for VC requests */
	ipu_configure_vc_mechanism(isp);

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	trace_printk("E|TMWK\n");
	return 0;

out_ipu_bus_del_devices:
	ipu_bus_del_devices(pdev);
	ipu_buttress_exit(isp);
	release_firmware(isp->cpd_fw);

	trace_printk("E|TMWK\n");
	return rval;
}

static void ipu_pci_remove(struct pci_dev *pdev)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);

	ipu_remove_debugfs(isp);
	ipu_trace_release(isp);

	ipu_bus_del_devices(pdev);

	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);

	pci_release_regions(pdev);
	pci_disable_device(pdev);

	ipu_buttress_exit(isp);

	release_firmware(isp->cpd_fw);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
static void ipu_pci_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);

	if (prepare) {
		dev_err(&pdev->dev, "FLR prepare\n");
		pm_runtime_forbid(&isp->pdev->dev);
		isp->flr_done = true;
		return;
	}

	ipu_buttress_restore(isp);
	if (isp->secure_mode)
		ipu_buttress_reset_authentication(isp);

	ipu_bus_flr_recovery();
	isp->ipc_reinit = true;
	pm_runtime_allow(&isp->pdev->dev);

	dev_err(&pdev->dev, "FLR completed\n");
}
#else
static void ipu_pci_reset_prepare(struct pci_dev *pdev)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);

	dev_warn(&pdev->dev, "FLR prepare\n");
	pm_runtime_forbid(&isp->pdev->dev);
	isp->flr_done = true;
}

static void ipu_pci_reset_done(struct pci_dev *pdev)
{
	struct ipu_device *isp = pci_get_drvdata(pdev);

	ipu_buttress_restore(isp);
	if (isp->secure_mode)
		ipu_buttress_reset_authentication(isp);

	ipu_bus_flr_recovery();
	isp->ipc_reinit = true;
	pm_runtime_allow(&isp->pdev->dev);

	dev_warn(&pdev->dev, "FLR completed\n");
}
#endif

#ifdef CONFIG_PM

/*
 * PCI base driver code requires driver to provide these to enable
 * PCI device level PM state transitions (D0<->D3)
 */
static int ipu_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ipu_device *isp = pci_get_drvdata(pdev);

	isp->flr_done = false;

	return 0;
}

static int ipu_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ipu_device *isp = pci_get_drvdata(pdev);
	struct ipu_buttress *b = &isp->buttress;
	int rval;

	/* Configure the arbitration mechanisms for VC requests */
	ipu_configure_vc_mechanism(isp);

	ipu_buttress_set_secure_mode(isp);
	isp->secure_mode = ipu_buttress_get_secure_mode(isp);
	dev_info(dev, "IPU in %s mode\n",
		 isp->secure_mode ? "secure" : "non-secure");

	ipu_buttress_restore(isp);

	rval = ipu_buttress_ipc_reset(isp, &b->cse);
	if (rval)
		dev_err(&isp->pdev->dev, "IPC reset protocol failed!\n");

	return 0;
}

static int ipu_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ipu_device *isp = pci_get_drvdata(pdev);
	int rval;

	ipu_configure_vc_mechanism(isp);
	ipu_buttress_restore(isp);

	if (isp->ipc_reinit) {
		struct ipu_buttress *b = &isp->buttress;

		isp->ipc_reinit = false;
		rval = ipu_buttress_ipc_reset(isp, &b->cse);
		if (rval)
			dev_err(&isp->pdev->dev,
				"IPC reset protocol failed!\n");
	}

	return 0;
}

static const struct dev_pm_ops ipu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(&ipu_suspend, &ipu_resume)
	    SET_RUNTIME_PM_OPS(&ipu_suspend,	/* Same as in suspend flow */
			       &ipu_runtime_resume,
			       NULL)
};

#define IPU_PM (&ipu_pm_ops)
#else
#define IPU_PM NULL
#endif

static const struct pci_device_id ipu_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, IPU_PCI_ID)},
	{0,}
};
MODULE_DEVICE_TABLE(pci, ipu_pci_tbl);

static const struct pci_error_handlers pci_err_handlers = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
	.reset_notify = ipu_pci_reset_notify,
#else
	.reset_prepare = ipu_pci_reset_prepare,
	.reset_done = ipu_pci_reset_done,
#endif
};

static struct pci_driver ipu_pci_driver = {
	.name = IPU_NAME,
	.id_table = ipu_pci_tbl,
	.probe = ipu_pci_probe,
	.remove = ipu_pci_remove,
	.driver = {
		   .pm = IPU_PM,
		   },
	.err_handler = &pci_err_handlers,
};

static int __init ipu_init(void)
{
	int rval = ipu_bus_register();

	if (rval) {
		pr_warn("can't register ipu bus (%d)\n", rval);
		return rval;
	}

	rval = pci_register_driver(&ipu_pci_driver);
	if (rval) {
		pr_warn("can't register pci driver (%d)\n", rval);
		goto out_pci_register_driver;
	}

	return 0;

out_pci_register_driver:
	ipu_bus_unregister();

	return rval;
}

static void __exit ipu_exit(void)
{
	pci_unregister_driver(&ipu_pci_driver);
	ipu_bus_unregister();
}

module_init(ipu_init);
module_exit(ipu_exit);

MODULE_AUTHOR("Sakari Ailus <sakari.ailus@linux.intel.com>");
MODULE_AUTHOR("Jouni Högander <jouni.hogander@intel.com>");
MODULE_AUTHOR("Antti Laakso <antti.laakso@intel.com>");
MODULE_AUTHOR("Samu Onkalo <samu.onkalo@intel.com>");
MODULE_AUTHOR("Jianxu Zheng <jian.xu.zheng@intel.com>");
MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_AUTHOR("Renwei Wu <renwei.wu@intel.com>");
MODULE_AUTHOR("Bingbu Cao <bingbu.cao@intel.com>");
MODULE_AUTHOR("Yunliang Ding <yunliang.ding@intel.com>");
MODULE_AUTHOR("Zaikuo Wang <zaikuo.wang@intel.com>");
MODULE_AUTHOR("Leifu Zhao <leifu.zhao@intel.com>");
MODULE_AUTHOR("Xia Wu <xia.wu@intel.com>");
MODULE_AUTHOR("Kun Jiang <kun.jiang@intel.com>");
MODULE_AUTHOR("Intel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel ipu pci driver");
