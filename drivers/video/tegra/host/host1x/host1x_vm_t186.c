/*
 * Tegra Graphics Host Virtual Memory Management
 *
 * Copyright (c) 2015, NVIDIA Corporation. All rights reserved.
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

#include <linux/iommu.h>
#include <linux/sizes.h>

#include <linux/platform/tegra/tegra-mc-sid.h>

#include "nvhost_vm.h"
#include "iommu_context_dev.h"

static int host1x_vm_init(struct nvhost_vm *vm)
{
	struct platform_device *pdev;

	/* wait until we have a context device */
	do {
		pdev = iommu_context_dev_allocate();
		if (!pdev)
			mdelay(1);
	} while (!pdev);

	vm->pdev = pdev;

	return 0;
}

static u32 host1x_vm_get_id_dev(struct platform_device *pdev)
{
	/* default to physical StreamID */
	int streamid = tegra_mc_get_smmu_bypass_sid();

	/* If SMMU is available for this device, query sid */
	if (pdev->dev.archdata.iommu) {
		streamid = iommu_get_hwid(pdev->dev.archdata.iommu,
					  &pdev->dev, 0);
		if (streamid < 0)
			streamid = tegra_mc_get_smmu_bypass_sid();
	}

	return streamid;
}

static int host1x_vm_get_id(struct nvhost_vm *vm)
{
	return host1x_vm_get_id_dev(vm->pdev);
}

static int host1x_vm_init_device(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	int streamid = host1x_vm_get_id_dev(pdev);
	int i;

	if (pdata->virtual_dev)
		return 0;

	for (i = 0; i < ARRAY_SIZE(pdata->vm_regs); i++) {
		if (!pdata->vm_regs[i].addr)
			return 0;

		/* use physical addressing by default */
		host1x_writel(pdev, pdata->vm_regs[i].addr, streamid);
	}

	return 0;
}

static void host1x_vm_deinit(struct nvhost_vm *vm)
{
	iommu_context_dev_release(vm->pdev);
}

static int host1x_vm_pin_static_buffer(struct platform_device *pdev,
				       void *vaddr, dma_addr_t paddr,
				       size_t size)
{
	return iommu_context_dev_map_static(vaddr, paddr, size);
}

static const struct nvhost_vm_ops host1x_vm_ops = {
	.init = host1x_vm_init,
	.deinit = host1x_vm_deinit,
	.pin_static_buffer = host1x_vm_pin_static_buffer,
	.get_id = host1x_vm_get_id,
	.init_device = host1x_vm_init_device,
};
