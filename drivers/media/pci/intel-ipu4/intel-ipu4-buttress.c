/*
 * Copyright (c) 2013--2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/elf.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/intel-ipu4-isys.h>

#include "intel-ipu4.h"
#include "intel-ipu4-bus.h"
#include "intel-ipu4-buttress.h"
#include "intel-ipu4-buttress-regs.h"
#include "intel-ipu4-cpd.h"

#define BOOTLOADER_STATUS_OFFSET	0x8000
#define BOOTLOADER_MAGIC_KEY		0xb00710ad

#define BUTTRESS_IRQS		(BUTTRESS_ISR_IPC_FROM_CSE_IS_WAITING | \
				 BUTTRESS_ISR_IPC_FROM_ISH_IS_WAITING |	\
				 BUTTRESS_ISR_IPC_EXEC_DONE_BY_CSE |	\
				 BUTTRESS_ISR_IPC_EXEC_DONE_BY_ISH |	\
				 BUTTRESS_ISR_SAI_VIOLATION |		\
				 BUTTRESS_ISR_IS_IRQ |			\
				 BUTTRESS_ISR_PS_IRQ)

#define ENTRY	BUTTRESS_IU2CSECSR_IPC_PEER_COMP_ACTIONS_RST_PHASE1
#define EXIT	BUTTRESS_IU2CSECSR_IPC_PEER_COMP_ACTIONS_RST_PHASE2
#define QUERY	BUTTRESS_IU2CSECSR_IPC_PEER_QUERIED_IP_COMP_ACTIONS_RST_PHASE

#define BUTTRESS_TSC_SYNC_RESET_TRIAL_MAX	10

#define BUTTRESS_CSE_BOOTLOAD_TIMEOUT		5000
#define BUTTRESS_CSE_AUTHENTICATE_TIMEOUT	10000

const struct intel_ipu4_buttress_sensor_clk_freq sensor_clk_freqs[] = {
	{ 6750000, BUTTRESS_SENSOR_CLK_FREQ_6P75MHZ },
	{ 8000000, BUTTRESS_SENSOR_CLK_FREQ_8MHZ },
	{ 9600000, BUTTRESS_SENSOR_CLK_FREQ_9P6MHZ },
	{ 12000000, BUTTRESS_SENSOR_CLK_FREQ_12MHZ },
	{ 13600000, BUTTRESS_SENSOR_CLK_FREQ_13P6MHZ },
	{ 14400000, BUTTRESS_SENSOR_CLK_FREQ_14P4MHZ },
	{ 15800000, BUTTRESS_SENSOR_CLK_FREQ_15P8MHZ },
	{ 16200000, BUTTRESS_SENSOR_CLK_FREQ_16P2MHZ },
	{ 17300000, BUTTRESS_SENSOR_CLK_FREQ_17P3MHZ },
	{ 18600000, BUTTRESS_SENSOR_CLK_FREQ_18P6MHZ },
	{ 19200000, BUTTRESS_SENSOR_CLK_FREQ_19P2MHZ },
	{ 24000000, BUTTRESS_SENSOR_CLK_FREQ_24MHZ },
	{ 26000000, BUTTRESS_SENSOR_CLK_FREQ_26MHZ },
	{ 27000000, BUTTRESS_SENSOR_CLK_FREQ_27MHZ }
};

static const u32 intel_ipu4_adev_irq_mask[] = {
	BUTTRESS_ISR_IS_IRQ, BUTTRESS_ISR_PS_IRQ
};

static int intel_ipu4_buttress_ipc_reset(struct intel_ipu4_device *isp,
				  enum intel_ipu4_buttress_ipc_domain ipc)
{
	unsigned long tout_jfs;
	unsigned tout = 500;
	u32 val = 0;
	u32 csr_in, csr_out, db0;

	switch (ipc) {
	case INTEL_IPU4_BUTTRESS_IPC_CSE:
		csr_in = BUTTRESS_REG_CSE2IUCSR;
		csr_out = BUTTRESS_REG_IU2CSECSR;
		db0 = BUTTRESS_REG_CSE2IUDB0;
		break;
	case INTEL_IPU4_BUTTRESS_IPC_ISH:
		csr_in = BUTTRESS_REG_ISH2IUCSR;
		csr_out = BUTTRESS_REG_IU2ISHCSR;
		db0 = BUTTRESS_REG_ISH2IUDB0;
		break;
	default:
		dev_err(&isp->pdev->dev, "%s: Invalid IPC domain\n", __func__);
		return -EINVAL;
	}

	/* Clear-by-1 CSR (all bits), corresponding internal states. */
	val = readl(isp->base + csr_in);
	writel(val, isp->base + csr_in);

	/* Set peer CSR bit IPC_PEER_COMP_ACTIONS_RST_PHASE1 */
	writel(ENTRY, isp->base + csr_out);

	/*
	* How long we should wait here?
	*/
	tout_jfs = jiffies + msecs_to_jiffies(tout);
	do {
		val = readl(isp->base + csr_in);
		dev_dbg(&isp->pdev->dev, "%s: csr_in = %x\n", __func__, val);
		if (val & ENTRY) {
			if (val & EXIT) {
				dev_dbg(&isp->pdev->dev,
					"%s: IPC_PEER_COMP_ACTIONS_RST_PHASE1 & IPC_PEER_COMP_ACTIONS_RST_PHASE2\n",
					__func__);
				/*
				 * 1) Clear-by-1 CSR bits
				 * (IPC_PEER_COMP_ACTIONS_RST_PHASE1,
				 * IPC_PEER_COMP_ACTIONS_RST_PHASE2).
				 * 2) Set peer CSR bit
				 * IPC_PEER_QUERIED_IP_COMP_ACTIONS_RST_PHASE.
				 */
				writel(ENTRY | EXIT, isp->base + csr_in);

				writel(QUERY, isp->base + csr_out);

				tout_jfs = jiffies + msecs_to_jiffies(tout);
				continue;
			} else {
				dev_dbg(&isp->pdev->dev, "%s: IPC_PEER_COMP_ACTIONS_RST_PHASE1\n",
					__func__);
				/*
				 * 1) Clear-by-1 CSR bits
				 * (IPC_PEER_COMP_ACTIONS_RST_PHASE1,
				 * IPC_PEER_QUERIED_IP_COMP_ACTIONS_RST_PHASE).
				 * 2) Set peer CSR bit
				 * IPC_PEER_COMP_ACTIONS_RST_PHASE1.
				 */
				writel(ENTRY | QUERY, isp->base + csr_in);

				writel(ENTRY, isp->base + csr_out);

				tout_jfs = jiffies + msecs_to_jiffies(tout);
				continue;
			}
		} else if (val & EXIT) {
			dev_dbg(&isp->pdev->dev, "%s: IPC_PEER_COMP_ACTIONS_RST_PHASE2\n",
				__func__);
			/*
			 * Clear-by-1 CSR bit
			 * IPC_PEER_COMP_ACTIONS_RST_PHASE2.
			 * 1) Clear incoming doorbell.
			 * 2) Clear-by-1 all CSR bits EXCEPT following
			 * bits:
			 * A. IPC_PEER_COMP_ACTIONS_RST_PHASE1.
			 * B. IPC_PEER_COMP_ACTIONS_RST_PHASE2.
			 * C. Possibly custom bits, depending on
			 * their role.
			 * 3) Set peer CSR bit
			 * IPC_PEER_COMP_ACTIONS_RST_PHASE2.
			 */
			writel(EXIT, isp->base + csr_in);

			writel(1 << BUTTRESS_IU2CSEDB0_BUSY_SHIFT,
			       isp->base + db0);

			writel(BUTTRESS_IU2CSECSR_IPC_PEER_DEASSERTED_REG_VALID_RE |
			       BUTTRESS_IU2CSECSR_IPC_PEER_ACKED_REG_VALID |
			       BUTTRESS_IU2CSECSR_IPC_PEER_ASSERTED_REG_VALID_REQ |
			       QUERY, isp->base + csr_in);

			writel(EXIT, isp->base + csr_out);

			return 0;
		} else if (val & QUERY) {
			dev_dbg(&isp->pdev->dev, "%s: IPC_PEER_QUERIED_IP_COMP_ACTIONS_RST_PHASE\n",
				__func__);
			/*
			 * 1) Clear-by-1 CSR bit
			 * IPC_PEER_QUERIED_IP_COMP_ACTIONS_RST_PHASE.
			 * 2) Set peer CSR bit
			 * IPC_PEER_COMP_ACTIONS_RST_PHASE1
			 */
			writel(QUERY, isp->base + csr_in);

			writel(ENTRY, isp->base + csr_out);

			tout_jfs = jiffies + msecs_to_jiffies(tout);
		}
		usleep_range(100, 500);
	} while (!time_after(jiffies, tout_jfs));

	dev_err(&isp->pdev->dev, "Timed out while waiting for CSE!\n");

	return -ETIMEDOUT;
}

int intel_ipu4_buttress_ipc_validity_protocol(struct intel_ipu4_device *isp)
{
	unsigned long tout_jfs;
	unsigned int tout = 1000;
	u32 val;

	/* Set bit 3 in CSE CSR */
	writel(BUTTRESS_IU2CSECSR_IPC_PEER_ASSERTED_REG_VALID_REQ,
	       isp->base + BUTTRESS_REG_CSE2IUCSR);

	/*
	* How long we should wait here?
	*/
	tout_jfs = jiffies + msecs_to_jiffies(tout);
	do {
		val = readl(isp->base + BUTTRESS_REG_CSE2IUCSR);
		dev_dbg(&isp->pdev->dev, "%s: CSE2IUCSR = %x\n", __func__, val);

		if (val & BUTTRESS_IU2CSECSR_IPC_PEER_ACKED_REG_VALID) {
			dev_dbg(&isp->pdev->dev, "%s: Validity ack received from CSE\n",
				__func__);
			goto out;
		}
		usleep_range(100, 1000);
	} while (!time_after(jiffies, tout_jfs));

	return -ETIMEDOUT;

out:
	/* Set bit 5 in CSE CSR */
	writel(BUTTRESS_IU2CSECSR_IPC_PEER_DEASSERTED_REG_VALID_RE,
	       isp->base + BUTTRESS_REG_CSE2IUCSR);

	return 0;
}

static void intel_ipu4_buttress_ipc_recv(
	struct intel_ipu4_device *isp,
	enum intel_ipu4_buttress_ipc_domain ipc,
	u32 *ipc_msg)
{
	u32 data0_offs, db0_offs, val;

	switch (ipc) {
	case INTEL_IPU4_BUTTRESS_IPC_CSE:
		data0_offs = BUTTRESS_REG_CSE2IUDATA0;
		db0_offs = BUTTRESS_REG_CSE2IUDB0;
		break;
	case INTEL_IPU4_BUTTRESS_IPC_ISH:
		data0_offs = BUTTRESS_REG_ISH2IUDATA0;
		db0_offs = BUTTRESS_REG_ISH2IUDB0;
		break;
	default:
		dev_err(&isp->pdev->dev, "%s: Invalid IPC domain\n", __func__);
		WARN_ON(1);
		return;
	}

	val = readl(isp->base + data0_offs);
	writel(0, isp->base + db0_offs);

	if (ipc_msg)
		*ipc_msg = val;
}

static int intel_ipu4_buttress_ipc_send(
	struct intel_ipu4_device *isp,
	enum intel_ipu4_buttress_ipc_domain ipc,
	u32 ipc_msg)
{
	struct intel_ipu4_buttress *b = &isp->buttress;
	struct completion *ipc_complete;
	u32 val, data0_offs, db0_offs;
	int ret;

	switch (ipc) {
	case INTEL_IPU4_BUTTRESS_IPC_CSE:
		data0_offs = BUTTRESS_REG_IU2CSEDATA0;
		db0_offs = BUTTRESS_REG_IU2CSEDB0;
		ipc_complete = &b->cse_ipc_complete;
		break;
	case INTEL_IPU4_BUTTRESS_IPC_ISH:
		data0_offs = BUTTRESS_REG_IU2CSEDATA0;
		db0_offs = BUTTRESS_REG_IU2CSEDB0;
		ipc_complete = &b->ish_ipc_complete;
		break;
	default:
		dev_err(&isp->pdev->dev, "%s: Invalid IPC domain\n", __func__);
		return -EINVAL;
	}

	init_completion(ipc_complete);

	writel(ipc_msg, isp->base + data0_offs);

	val = 1 << BUTTRESS_IU2CSEDB0_BUSY_SHIFT | 1;

	writel(val, isp->base + db0_offs);

	/*
	 * TODO: How long we should wait?
	 */
	ret = wait_for_completion_timeout(ipc_complete,
					  msecs_to_jiffies(10));
	if (ret) {
		dev_dbg(&isp->pdev->dev, "IPC response received\n");
		return 0;
	}

	dev_err(&isp->pdev->dev, "IPC timeout\n");

	return -ETIMEDOUT;
}

static irqreturn_t intel_ipu4_buttress_call_isr(
					struct intel_ipu4_bus_device *adev)
{
	irqreturn_t ret = IRQ_WAKE_THREAD;

	if (!adev || !adev->adrv)
		return IRQ_NONE;

	if (adev->adrv->isr)
		ret = adev->adrv->isr(adev);

	if (ret == IRQ_WAKE_THREAD && !adev->adrv->isr_threaded)
		ret = IRQ_NONE;

	adev->adrv->wake_isr_thread = (ret == IRQ_WAKE_THREAD);

	return ret;
}

irqreturn_t intel_ipu4_buttress_isr(int irq, void *isp_ptr)
{
	struct intel_ipu4_device *isp = isp_ptr;
	struct intel_ipu4_bus_device *adev[] = { isp->isys, isp->psys };
	struct intel_ipu4_buttress *b = &isp->buttress;
	irqreturn_t ret = IRQ_NONE;
	u32 disable_irqs = 0;
	u32 irq_status;
	unsigned int i;

	dev_dbg(&isp->pdev->dev, "Buttress interrupt handler\n");

	irq_status = readl(isp->base + BUTTRESS_REG_ISR_ENABLED_STATUS);
	if (!irq_status)
		return IRQ_NONE;

	do {
		writel(irq_status, isp->base + BUTTRESS_REG_ISR_CLEAR);

		for (i = 0; i < ARRAY_SIZE(intel_ipu4_adev_irq_mask); i++) {
			if (irq_status & intel_ipu4_adev_irq_mask[i]) {
				irqreturn_t r =
					intel_ipu4_buttress_call_isr(adev[i]);
				if (r == IRQ_WAKE_THREAD) {
					ret = IRQ_WAKE_THREAD;
					disable_irqs |=
						intel_ipu4_adev_irq_mask[i];
				} else if (ret == IRQ_NONE && r == IRQ_HANDLED)
					ret = IRQ_HANDLED;
			}
		}

		if (irq_status & (BUTTRESS_ISR_IPC_FROM_CSE_IS_WAITING |
				  BUTTRESS_ISR_IPC_FROM_ISH_IS_WAITING |
				  BUTTRESS_ISR_IPC_EXEC_DONE_BY_CSE |
				  BUTTRESS_ISR_IPC_EXEC_DONE_BY_ISH |
				  BUTTRESS_ISR_SAI_VIOLATION) &&
		    ret == IRQ_NONE)
			ret = IRQ_HANDLED;

		if (irq_status & BUTTRESS_ISR_IPC_FROM_CSE_IS_WAITING) {
			dev_dbg(&isp->pdev->dev,
				"BUTTRESS_ISR_IPC_FROM_CSE_IS_WAITING\n");
			intel_ipu4_buttress_ipc_recv(isp,
					INTEL_IPU4_BUTTRESS_IPC_CSE, NULL);
		}

		if (irq_status & BUTTRESS_ISR_IPC_FROM_ISH_IS_WAITING) {
			dev_dbg(&isp->pdev->dev,
				"BUTTRESS_ISR_IPC_FROM_CSE_IS_WAITING\n");
			intel_ipu4_buttress_ipc_recv(isp,
					INTEL_IPU4_BUTTRESS_IPC_ISH, NULL);
		}

		if (irq_status & BUTTRESS_ISR_IPC_EXEC_DONE_BY_CSE) {
			dev_dbg(&isp->pdev->dev,
				"BUTTRESS_ISR_IPC_EXEC_DONE_BY_CSE\n");
			complete(&b->cse_ipc_complete);
		}

		if (irq_status & BUTTRESS_ISR_IPC_EXEC_DONE_BY_ISH) {
			dev_dbg(&isp->pdev->dev,
				"BUTTRESS_ISR_IPC_EXEC_DONE_BY_CSE\n");
			complete(&b->ish_ipc_complete);
		}

		if (irq_status & BUTTRESS_ISR_SAI_VIOLATION) {
			dev_err(&isp->pdev->dev,
				"BUTTRESS_ISR_SAI_VIOLATION\n");
			WARN_ON(1);
		}

		irq_status = readl(isp->base + BUTTRESS_REG_ISR_ENABLED_STATUS);
	} while (irq_status);

	if (disable_irqs)
		writel(BUTTRESS_IRQS & ~disable_irqs,
		       isp->base + BUTTRESS_REG_ISR_ENABLE);

	return ret;
}

irqreturn_t intel_ipu4_buttress_isr_threaded(int irq, void *isp_ptr)
{
	struct intel_ipu4_device *isp = isp_ptr;
	struct intel_ipu4_bus_device *adev[] = { isp->isys, isp->psys };
	irqreturn_t ret = IRQ_NONE;
	unsigned int i;

	dev_dbg(&isp->pdev->dev, "Buttress threaded interrupt handler\n");

	for (i = 0; i < ARRAY_SIZE(intel_ipu4_adev_irq_mask); i++) {
		if (adev[i] && adev[i]->adrv &&
		    adev[i]->adrv->wake_isr_thread &&
		    adev[i]->adrv->isr_threaded(adev[i]) == IRQ_HANDLED)
				ret = IRQ_HANDLED;
	}

	writel(BUTTRESS_IRQS, isp->base + BUTTRESS_REG_ISR_ENABLE);

	return ret;
}

void intel_ipu4_buttress_disable_secure_touch(struct intel_ipu4_device *isp)
{
	u32 val;

	val = readl(isp->base + BUTTRESS_REG_SECURE_TOUCH);
	val &= ~(1 << BUTTRESS_SECURE_TOUCH_SECURE_TOUCH_SHIFT);
	writel(val, isp->base + BUTTRESS_REG_SECURE_TOUCH);
}

int intel_ipu4_buttress_power(
	struct device *dev, struct intel_ipu4_buttress_ctrl *ctrl, bool on)
{
	struct intel_ipu4_device *isp = to_intel_ipu4_bus_device(dev)->isp;
	u32 pwr_sts, val, retry = 300;
	int ret = 0;

	if (!ctrl)
		return 0;

	mutex_lock(&isp->buttress.power_mutex);

	if (!on) {
		val = 0;
		pwr_sts = ctrl->pwr_sts_off << ctrl->pwr_sts_shift;
	} else {
		val = 1 << BUTTRESS_FREQ_CTL_START_SHIFT
			| ctrl->divisor
			| ctrl->qos_floor << BUTTRESS_FREQ_CTL_QOS_FLOOR_SHIFT;

		pwr_sts = ctrl->pwr_sts_on << ctrl->pwr_sts_shift;
	}

	writel(val, isp->base + ctrl->freq_ctl);

	/* TODO: How long we should wait? */
	do {
		usleep_range(100, 1000);
		val = readl(isp->base + BUTTRESS_REG_PWR_STATE);
		if ((val & ctrl->pwr_sts_mask) == pwr_sts) {
			dev_dbg(&isp->pdev->dev, "Rail state successfully changed\n");
			goto out;
		}

	} while (retry--);

	dev_err(&isp->pdev->dev,
		"Timeout when trying to change state of the rail 0x%x\n", val);

	/*
	 * Return success always as psys power up is not working
	 * currently. This should be -ETIMEDOUT always when psys power
	 * up is fixed
	 * Also in FPGA case don't report time out. Depending on FPGA version
	 * the PM state transition may or may not work.
	 */
	if (!is_intel_ipu4_hw_bxt_fpga(isp))
		ret = -ETIMEDOUT;

out:
	if (is_intel_ipu4_hw_bxt_fpga(isp))
		intel_ipu4_buttress_disable_secure_touch(isp);

	ctrl->started = !ret && on;

	mutex_unlock(&isp->buttress.power_mutex);

	return ret;
}

static bool secure_mode_enable;
module_param(secure_mode_enable, bool, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(secure_mode, "IPU4 secure mode enable");

void intel_ipu4_buttress_set_secure_mode(struct intel_ipu4_device *isp)
{
	uint8_t retry = 100;
	u32 val, read;

	/*
	 * HACK to disable possible secure mode. This can be
	 * reverted when CSE is disabling the secure mode
	 */
	val = readl(isp->base + BUTTRESS_REG_SECURITY_CTL);
	if (secure_mode_enable)
		val |= 1 << BUTTRESS_SECURITY_CTL_FW_SECURE_MODE_SHIFT;
	else
		val &= ~(1 << BUTTRESS_SECURITY_CTL_FW_SECURE_MODE_SHIFT);

	writel(val, isp->base + BUTTRESS_REG_SECURITY_CTL);

	/* In B0, for some registers in buttress, because of a hw bug, write
	 * might not succeed at first attempt. Write twice untill the
	 * write is successful
	 */
	if (is_intel_ipu4_hw_bxt_b0(isp)) {
		writel(val, isp->base + BUTTRESS_REG_SECURITY_CTL);

		while (retry--) {
			read = readl(isp->base + BUTTRESS_REG_SECURITY_CTL);
			if (read == val)
				break;

			writel(val, isp->base + BUTTRESS_REG_SECURITY_CTL);
		}
		if (retry == 0)
			dev_err(&isp->pdev->dev,
				"update security control register failed\n");
	}
}

static bool intel_ipu4_buttress_get_secure_mode(struct intel_ipu4_device *isp)
{
	u32 val;

	if (is_intel_ipu4_hw_bxt_fpga(isp))
		return 0;

	val = readl(isp->base + BUTTRESS_REG_SECURITY_CTL);

	return val & (1 << BUTTRESS_SECURITY_CTL_FW_SECURE_MODE_SHIFT);
}

static bool intel_ipu4_buttress_auth_done(struct intel_ipu4_device *isp)
{
	u32 val;

	if (is_intel_ipu4_hw_bxt_fpga(isp))
		return 0;

	val = readl(isp->base + BUTTRESS_REG_SECURITY_CTL);

	return (val & BUTTRESS_SECURITY_CTL_FW_SETUP_MASK) ==
		BUTTRESS_SECURITY_CTL_AUTH_DONE;
}

void intel_ipu4_buttress_set_psys_ratio(struct intel_ipu4_device *isp,
					unsigned int psys_divisor,
					unsigned int psys_qos_floor)
{
	struct intel_ipu4_buttress_ctrl *ctrl = isp->psys_iommu->ctrl;

	mutex_lock(&isp->buttress.power_mutex);
	ctrl->divisor = psys_divisor;
	ctrl->qos_floor = psys_qos_floor;

	if (ctrl->started) {
		/*
		 * According to documentation driver initiates DVFS
		 * transition by writing wanted ratio, floor ratio and start
		 * bit. No need to stop PS first
		 */
		writel(1 << BUTTRESS_FREQ_CTL_START_SHIFT |
		       ctrl->qos_floor << BUTTRESS_FREQ_CTL_QOS_FLOOR_SHIFT |
		       psys_divisor, isp->base + BUTTRESS_REG_PS_FREQ_CTL);
	}

	mutex_unlock(&isp->buttress.power_mutex);
}

int intel_ipu4_buttress_map_fw_image(struct intel_ipu4_bus_device *sys,
				     const struct firmware *fw,
					 struct sg_table *sgt)
{
	struct page **pages;
	const void *addr;
	unsigned long n_pages, i;
	int rval;

	n_pages = PAGE_ALIGN(fw->size) >> PAGE_SHIFT;

	pages = kmalloc_array(n_pages, sizeof(*pages), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	addr = fw->data;
	for (i = 0; i < n_pages; i++) {
		struct page *p = vmalloc_to_page(addr);

		if (!p) {
			rval = -ENODEV;
			goto out;
		}
		pages[i] = p;
		addr += PAGE_SIZE;
	}

	rval = sg_alloc_table_from_pages(sgt, pages, n_pages, 0, fw->size,
					 GFP_KERNEL);
	if (rval) {
		rval = -ENOMEM;
		goto out;
	}

	n_pages = dma_map_sg(&sys->dev, sgt->sgl, sgt->nents, DMA_TO_DEVICE);
	if (n_pages != sgt->nents) {
		rval = -ENOMEM;
		sg_free_table(sgt);
		goto out;
	}

	dma_sync_sg_for_device(&sys->dev, sgt->sgl, sgt->nents, DMA_TO_DEVICE);

out:
	kfree(pages);

	return rval;
}
EXPORT_SYMBOL_GPL(intel_ipu4_buttress_map_fw_image);

int intel_ipu4_buttress_unmap_fw_image(struct intel_ipu4_bus_device *sys,
				    struct sg_table *sgt)
{
	dma_unmap_sg(&sys->dev, sgt->sgl, sgt->nents, DMA_TO_DEVICE);
	sg_free_table(sgt);

	return 0;
}
EXPORT_SYMBOL_GPL(intel_ipu4_buttress_unmap_fw_image);

int intel_ipu4_buttress_authenticate(struct intel_ipu4_device *isp)
{
	struct intel_ipu4_psys_pdata *psys_pdata;
	struct intel_ipu4_buttress *b = &isp->buttress;
	u32 data;
	int rval;
	unsigned long tout_jfs;

	if (!isp->secure_mode) {
		dev_dbg(&isp->pdev->dev, "Non-secure mode -> skip authentication\n",
			rval);
		return 0;
	}

	psys_pdata = isp->psys->pdata;

	mutex_lock(&b->auth_mutex);

	rval = pm_runtime_get_sync(&isp->psys_iommu->dev);
	if (rval < 0) {
		dev_err(&isp->pdev->dev, "Runtime PM failed (%d)\n", rval);
		goto unlock_mutex;
	}

	if (intel_ipu4_buttress_auth_done(isp)) {
		rval = 0;
		goto iunit_power_off;
	}

	/*
	 * Write address of FIT table to FW_SOURCE register
	 * Let's use fw address. I.e. not using FIT table yet
	 */
	data = (u32)isp->pkg_dir_dma_addr;
	writel(data, isp->base + BUTTRESS_REG_FW_SOURCE_BASE_LO);

	data = (u32)(isp->pkg_dir_dma_addr >> 32);
	writel(data, isp->base + BUTTRESS_REG_FW_SOURCE_BASE_HI);

	/*
	 * Write boot_load into IU2CSEDATA0
	 * Write sizeof(boot_load) | 0x2 << CLIENT_ID to
	 * IU2CSEDB.IU2CSECMD and set IU2CSEDB.IU2CSEBUSY as
	 */
	dev_info(&isp->pdev->dev, "Sending BOOT_LOAD to CSE\n");
	rval = intel_ipu4_buttress_ipc_send(
		isp, INTEL_IPU4_BUTTRESS_IPC_CSE,
		BUTTRESS_IU2CSEDATA0_IPC_BOOT_LOAD);
	if (rval) {
		dev_err(&isp->pdev->dev, "CSE boot_load failed\n");
		goto iunit_power_off;
	}

	tout_jfs = jiffies + msecs_to_jiffies(BUTTRESS_CSE_BOOTLOAD_TIMEOUT);
	do {
		data = readl(isp->base + BUTTRESS_REG_CSE_ACTION);
		data &= BUTTRESS_CSE_ACTION_MASK;
		if (data == BUTTRESS_CSE_ACTION_SETUP_DONE) {
			dev_dbg(&isp->pdev->dev, "CSE boot_load done\n");
			break;
		} else if (data == BUTTRESS_CSE_ACTION_BASE_AUTH_FAILED) {
			dev_err(&isp->pdev->dev, "CSE boot_load failed\n");
			rval = -EINVAL;
			goto iunit_power_off;
		}
		usleep_range(500, 1000);
	} while (!time_after(jiffies, tout_jfs));

	if (data != BUTTRESS_CSE_ACTION_SETUP_DONE) {
		dev_err(&isp->pdev->dev, "CSE boot_load timed out\n");
		rval = -ETIMEDOUT;
		goto iunit_power_off;
	}

	tout_jfs = jiffies + msecs_to_jiffies(BUTTRESS_CSE_BOOTLOAD_TIMEOUT);
	do {
		data = readl(psys_pdata->base + BOOTLOADER_STATUS_OFFSET);
		dev_dbg(&isp->pdev->dev, "%s: BOOTLOADER_STATUS 0x%x",
			__func__, data);
		if (data == BOOTLOADER_MAGIC_KEY) {
			dev_dbg(&isp->pdev->dev,
				"%s: Expected magic number found, breaking...",
				__func__);
			break;
		}
		usleep_range(500, 1000);
	} while (!time_after(jiffies, tout_jfs));

	if (data != BOOTLOADER_MAGIC_KEY) {
		dev_dbg(&isp->pdev->dev,
			"%s: CSE boot_load timed out...\n", __func__);
		rval = -ETIMEDOUT;
		goto iunit_power_off;
	}

	/*
	 * Write authenticate_run into IU2CSEDATA0
	 * Write sizeof(boot_load) | 0x2 << CLIENT_ID to
	 * IU2CSEDB.IU2CSECMD and set IU2CSEDB.IU2CSEBUSY as
	 */
	dev_info(&isp->pdev->dev, "Sending AUTHENTICATE_RUN to CSE\n");
	rval = intel_ipu4_buttress_ipc_send(
		isp, INTEL_IPU4_BUTTRESS_IPC_CSE,
		BUTTRESS_IU2CSEDATA0_IPC_AUTHENTICATE_RUN);
	if (rval) {
		dev_err(&isp->pdev->dev, "CSE authenticate_run failed\n");
		goto iunit_power_off;
	}

	tout_jfs = jiffies + msecs_to_jiffies(
		BUTTRESS_CSE_AUTHENTICATE_TIMEOUT);
	do {
		data = readl(isp->base + BUTTRESS_REG_CSE_ACTION);
		data &= BUTTRESS_CSE_ACTION_MASK;
		if (data == BUTTRESS_CSE_ACTION_BASE_AUTH_DONE) {
			dev_dbg(&isp->pdev->dev, "CSE authenticate_run done\n");
			break;
		} else if (data == BUTTRESS_CSE_ACTION_BASE_AUTH_FAILED) {
			dev_err(&isp->pdev->dev, "CSE authenticate_run failed\n");
			rval = -EINVAL;
			goto iunit_power_off;
		}
		usleep_range(500, 1000);
	} while (!time_after(jiffies, tout_jfs));

	if (data != BUTTRESS_CSE_ACTION_BASE_AUTH_DONE) {
		dev_err(&isp->pdev->dev, "CSE authenticate_run timed out\n");
		rval = -ETIMEDOUT;
		goto iunit_power_off;
	}

iunit_power_off:
	pm_runtime_put(&isp->psys_iommu->dev);

unlock_mutex:
	mutex_unlock(&b->auth_mutex);

	return rval;
}
EXPORT_SYMBOL(intel_ipu4_buttress_authenticate);

static int intel_ipu4_buttress_send_tsc_request(struct intel_ipu4_device *isp)
{
	unsigned long tout_jfs = msecs_to_jiffies(5);

	writel(BUTTRESS_FABRIC_CMD_START_TSC_SYNC,
	       isp->base + BUTTRESS_REG_FABRIC_CMD);

	tout_jfs += jiffies;
	do {
		u32 val;

		val = readl(isp->base + BUTTRESS_REG_PWR_STATE);
		val = (val & BUTTRESS_PWR_STATE_HH_STATUS_MASK) >>
			BUTTRESS_PWR_STATE_HH_STATUS_SHIFT;

		switch (val) {
		case BUTTRESS_PWR_STATE_HH_STATE_DONE:
			dev_dbg(&isp->pdev->dev,
				 "Start tsc sync completed!\n");
			return 0;
		case BUTTRESS_PWR_STATE_HH_STATE_ERR:
			dev_err(&isp->pdev->dev,
				 "Start tsc sync failed!\n");
			return -EINVAL;
		default:
			usleep_range(500, 1000);
			break;
		}
	} while (!time_after(jiffies, tout_jfs));

	return -ETIMEDOUT;
}

int intel_ipu4_buttress_start_tsc_sync(struct intel_ipu4_device *isp)
{
	unsigned int i;

	if (is_intel_ipu4_hw_bxt_fpga(isp))
		return 0;

	for (i = 0; i < BUTTRESS_TSC_SYNC_RESET_TRIAL_MAX; i++) {
		int ret;

		ret = intel_ipu4_buttress_send_tsc_request(isp);
		if (ret == -ETIMEDOUT) {
			u32 val;
			/* set tsw soft reset */
			val = readl(isp->base + BUTTRESS_REG_TSW_CTL);
			val = val | BUTTRESS_TSW_CTL_SOFT_RESET;
			writel(val, isp->base + BUTTRESS_REG_TSW_CTL);
			/* clear tsw soft reset */
			val = val & (~BUTTRESS_TSW_CTL_SOFT_RESET);
			writel(val, isp->base + BUTTRESS_REG_TSW_CTL);

			continue;
		}
		return ret;
	}

	dev_err(&isp->pdev->dev, "TSC sync failed(timeout).\n");

	return -ETIMEDOUT;
}
EXPORT_SYMBOL(intel_ipu4_buttress_start_tsc_sync);

struct clk_intel_ipu4_sensor {
	struct intel_ipu4_device *isp;
	struct clk_hw hw;
	unsigned int id;
	unsigned long rate;
};

#define to_clk_intel_ipu4_sensor(_hw) \
	container_of(_hw, struct clk_intel_ipu4_sensor, hw)

static int intel_ipu4_buttress_clk_pll_prepare(struct clk_hw *hw)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);
	int ret;

	/* Workaround needed to get sensor clock running in some cases */
	ret = pm_runtime_get_sync(&ck->isp->isys->dev);
	return ret >=  0 ? 0 : ret;
}

static void intel_ipu4_buttress_clk_pll_unprepare(struct clk_hw *hw)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);

	/* Workaround needed to get sensor clock stopped in some cases */
	pm_runtime_put(&ck->isp->isys->dev);
}

static int intel_ipu4_buttress_clk_pll_enable(struct clk_hw *hw)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);
	u32 val;

	/*
	 * Start bit behaves like master clock request towards ICLK.
	 * It is needed regardless of the 24 MHz or per clock out pll
	 * setting.
	 */
	val = readl(ck->isp->base + BUTTRESS_REG_SENSOR_FREQ_CTL);
	val |= 1 << BUTTRESS_FREQ_CTL_START_SHIFT;

	if (is_intel_ipu4_hw_bxt_b0(ck->isp)) {
		unsigned int i;

		val &= ~BUTTRESS_SENSOR_FREQ_CTL_OSC_OUT_FREQ_MASK_B0(ck->id);
		for (i = 0; i < ARRAY_SIZE(sensor_clk_freqs); i++)
			if (sensor_clk_freqs[i].rate == ck->rate)
				break;

		if (i < ARRAY_SIZE(sensor_clk_freqs))
			val |= sensor_clk_freqs[i].val <<
			    BUTTRESS_SENSOR_FREQ_CTL_OSC_OUT_FREQ_SHIFT_B0(ck->id);
		else
			val |= BUTTRESS_SENSOR_FREQ_CTL_OSC_OUT_FREQ_DEFAULT_B0(ck->id);
	}

	writel(val, ck->isp->base + BUTTRESS_REG_SENSOR_FREQ_CTL);

	return 0;
}

static void intel_ipu4_buttress_clk_pll_disable(struct clk_hw *hw)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);
	u32 val;

	/* See enable control above */
	val = readl(ck->isp->base + BUTTRESS_REG_SENSOR_FREQ_CTL);
	val &= ~(1 << BUTTRESS_FREQ_CTL_START_SHIFT);
	writel(val, ck->isp->base + BUTTRESS_REG_SENSOR_FREQ_CTL);
}

static int intel_ipu4_buttress_clk_enable(struct clk_hw *hw)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);
	u32 val;

	val = readl(ck->isp->base + BUTTRESS_REG_SENSOR_CLK_CTL);
	val |= 1 << BUTTRESS_SENSOR_CLK_CTL_OSC_CLK_OUT_EN_SHIFT(ck->id);

	/* Enable dynamic sensor clock */
	val |= 1 << BUTTRESS_SENSOR_CLK_CTL_OSC_CLK_OUT_SEL_SHIFT(ck->id);
	writel(val, ck->isp->base + BUTTRESS_REG_SENSOR_CLK_CTL);

	return 0;
}

static void intel_ipu4_buttress_clk_disable(struct clk_hw *hw)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);
	u32 val;

	val = readl(ck->isp->base + BUTTRESS_REG_SENSOR_CLK_CTL);
	val &= ~(1 << BUTTRESS_SENSOR_CLK_CTL_OSC_CLK_OUT_EN_SHIFT(ck->id));
	writel(val, ck->isp->base + BUTTRESS_REG_SENSOR_CLK_CTL);
}

static long intel_ipu4_buttress_clk_round_rate(
	struct clk_hw *hw, unsigned long rate, unsigned long *parent_rate)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);
	unsigned long best = ULONG_MAX;
	unsigned long round_rate;
	int i;

	if (is_intel_ipu4_hw_bxt_a0(ck->isp))
		return 24000000;

	for (i = 0; i < ARRAY_SIZE(sensor_clk_freqs); i++) {
		long diff = sensor_clk_freqs[i].rate - rate;
		if (0 == diff)
			return rate;

		diff = abs(diff);
		if (diff < best) {
			best = diff;
			round_rate = sensor_clk_freqs[i].rate;
		}
	}

	return round_rate;
}

static unsigned long intel_ipu4_buttress_clk_recalc_rate(
	struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);

	return ck->rate;
}

static int intel_ipu4_buttress_clk_set_rate(
	struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct clk_intel_ipu4_sensor *ck = to_clk_intel_ipu4_sensor(hw);

	/*
	 * R	N	P	PVD	PLLout
	 * 1	45	128	2	6.75
	 * 1	40	96	2	8
	 * 1	40	80	2	9.6
	 * 1	15	20	4	14.4
	 * 1	40	32	2	24
	 * 1	65	48	1	26
	 *
	 * FIXME: Configure 24 MHz for now.
	 */
	ck->rate = rate;

	return 0;
}

static struct clk_ops intel_ipu4_buttress_clk_sensor_ops = {
	.enable = intel_ipu4_buttress_clk_enable,
	.disable = intel_ipu4_buttress_clk_disable,
};

static struct clk_ops intel_ipu4_buttress_clk_sensor_ops_parent = {
	.enable = intel_ipu4_buttress_clk_pll_enable,
	.disable = intel_ipu4_buttress_clk_pll_disable,
	.prepare = intel_ipu4_buttress_clk_pll_prepare,
	.unprepare = intel_ipu4_buttress_clk_pll_unprepare,
	.round_rate = intel_ipu4_buttress_clk_round_rate,
	.recalc_rate = intel_ipu4_buttress_clk_recalc_rate,
	.set_rate = intel_ipu4_buttress_clk_set_rate,
};

static struct clk_init_data intel_ipu4_buttress_sensor_clk_data_a0[] = {
	{
		.name = "OSC_CLK_OUT0",
		.ops = &intel_ipu4_buttress_clk_sensor_ops,
		.parent_names = (const char *[]){
			"intel_ipu4_sensor_pll"
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
	{
		.name = "OSC_CLK_OUT1",
		.ops = &intel_ipu4_buttress_clk_sensor_ops,
		.parent_names = (const char *[]){
			"intel_ipu4_sensor_pll"
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
	{
		.name = "OSC_CLK_OUT2",
		.ops = &intel_ipu4_buttress_clk_sensor_ops,
		.parent_names = (const char *[]){
			"OSC_CLK_OUT1"
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_init_data intel_ipu4_buttress_sensor_clk_data_b0[] = {
	{
		.name = "OSC_CLK_OUT0",
		.ops = &intel_ipu4_buttress_clk_sensor_ops,
		.parent_names = (const char *[]){
			"intel_ipu4_sensor_pll"
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
	{
		.name = "OSC_CLK_OUT1",
		.ops = &intel_ipu4_buttress_clk_sensor_ops,
		.parent_names = (const char *[]){
			"intel_ipu4_sensor_pll"
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
	{
		.name = "OSC_CLK_OUT2",
		.ops = &intel_ipu4_buttress_clk_sensor_ops,
		.parent_names = (const char *[]){
			"intel_ipu4_sensor_pll"
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_init_data intel_ipu4_buttress_sensor_pll_data = {
	.name = "intel_ipu4_sensor_pll",
	.ops = &intel_ipu4_buttress_clk_sensor_ops_parent,

};

static int intel_ipu4_buttress_clk_init(struct intel_ipu4_device *isp)
{
	struct intel_ipu4_buttress *b = &isp->buttress;
	struct intel_ipu4_isys_subdev_pdata *pdata =
		isp->pdev->dev.platform_data;
	struct intel_ipu4_isys_clk_mapping *clkmap =
		pdata ? pdata->clk_map : NULL;
	struct clk_intel_ipu4_sensor *parent_clk =
		devm_kzalloc(&isp->pdev->dev, sizeof(*parent_clk), GFP_KERNEL);
	struct clk_init_data *clk_data;
	int i, rval;

	if (!parent_clk)
		return -ENOMEM;

	parent_clk->hw.init = &intel_ipu4_buttress_sensor_pll_data;
	parent_clk->isp = isp;

	b->pll_sensor = clk_register(NULL, &parent_clk->hw);
	if (IS_ERR(b->pll_sensor))
		return PTR_ERR(b->pll_sensor);

	clk_data = is_intel_ipu4_hw_bxt_a0(isp) ?
		intel_ipu4_buttress_sensor_clk_data_a0 :
		intel_ipu4_buttress_sensor_clk_data_b0;

	for (i = 0; i < INTEL_IPU4_BUTTRESS_NUM_OF_SENS_CKS; i++) {
		char buffer[16]; /* max for clk_register_clkdev */
		struct clk_intel_ipu4_sensor *my_clk =
			devm_kzalloc(&isp->pdev->dev, sizeof(*my_clk),
				     GFP_KERNEL);

		if (!my_clk) {
			rval = -ENOMEM;
			goto err;
		}

		my_clk->hw.init = &clk_data[i];

		my_clk->id = i;
		my_clk->isp = isp;

		b->clk_sensor[i] = clk_register(NULL, &my_clk->hw);
		if (IS_ERR(b->clk_sensor[i])) {
			rval = PTR_ERR(b->clk_sensor[i]);
			goto err;
		}

		/*
		 * Workaround for hsd 120947163. I.e. osc_clk1 and
		 * osc_clk2 share en control in A0
		 */

		if ((is_intel_ipu4_hw_bxt_a0(isp)) && (i == 2))
			rval = clk_set_parent(b->clk_sensor[i],
					      b->clk_sensor[1]);
		else
			rval = clk_set_parent(b->clk_sensor[i], b->pll_sensor);
		if (rval)
			goto err;

		/* Register generic clocks for sensor driver */
		snprintf(buffer, sizeof(buffer), "ipu4_cam_clk%d", i);
		rval = clk_register_clkdev(b->clk_sensor[i], buffer, NULL);
		if (rval)
			goto err;
	}

	/* Now map sensor clocks */
	if (!clkmap)
		return 0;

	while (clkmap->clkdev_data.dev_id) {
		/*
		 * Lookup table must be NULL terminated
		 * CLKDEV_INIT(NULL, NULL, NULL)
		 */
		for (i = 0; i < INTEL_IPU4_BUTTRESS_NUM_OF_SENS_CKS; i++) {
			if (!strcmp(clkmap->platform_clock_name,
				clk_data[i].name)) {
				clkmap->clkdev_data.clk = b->clk_sensor[i];
				clkdev_add(&clkmap->clkdev_data);
				break;
			}
		}
		clkmap++;
	}

	return 0;

err:
	for (; i >= 0; i--)
		clk_unregister(b->clk_sensor[i]);

	clk_unregister(b->pll_sensor);

	return rval;
}

#ifdef CONFIG_DEBUG_FS

static int intel_ipu4_buttress_reg_open(struct inode *inode, struct file *file)
{
	if (!inode->i_private)
		return -EACCES;

	file->private_data = inode->i_private;
	return 0;
}

static ssize_t intel_ipu4_buttress_reg_read(struct file *file, char __user *buf,
		   size_t count, loff_t *ppos)
{
	struct debugfs_reg32 *reg = file->private_data;
	u8 tmp[11];
	u32 val = readl((void __iomem *)reg->offset);
	int len = scnprintf(tmp, sizeof(tmp), "0x%08x", val);

	return simple_read_from_buffer(buf, len, ppos, &tmp, len);
}

static ssize_t intel_ipu4_buttress_reg_write(struct file *file,
					     const char __user *buf,
					     size_t count, loff_t *ppos)
{
	struct debugfs_reg32 *reg = file->private_data;
	u32 val;
	int rval;

	rval = kstrtou32_from_user(buf, count, 0, &val);
	if (rval)
		return rval;

	writel(val, (void __iomem *)reg->offset);

	return count;
}

static struct debugfs_reg32 buttress_regs[] = {
	{ "IU2CSEDB0", BUTTRESS_REG_IU2CSEDB0 },
	{ "IU2CSEDATA0", BUTTRESS_REG_IU2CSEDATA0 },
	{ "CSE2IUDB0", BUTTRESS_REG_CSE2IUDB0 },
	{ "CSE2IUDATA0", BUTTRESS_REG_CSE2IUDATA0 },
	{ "CSE2IUCSR", BUTTRESS_REG_CSE2IUCSR },
	{ "IU2CSECSR", BUTTRESS_REG_IU2CSECSR },
};

static const struct file_operations intel_ipu4_buttress_reg_fops = {
	.owner = THIS_MODULE,
	.open = intel_ipu4_buttress_reg_open,
	.read = intel_ipu4_buttress_reg_read,
	.write = intel_ipu4_buttress_reg_write,
};

static int intel_ipu4_buttress_start_tsc_sync_set(void *data, u64 val)
{
	struct intel_ipu4_device *isp = data;

	return intel_ipu4_buttress_start_tsc_sync(isp);
}

DEFINE_SIMPLE_ATTRIBUTE(intel_ipu4_buttress_start_tsc_sync_fops,
			NULL, /* intel_ipu4_buttress_start_tsc_sync_get, */
			intel_ipu4_buttress_start_tsc_sync_set, "%llu\n");

static int intel_ipu4_buttress_tsc_get(void *data, u64 *val)
{
	struct intel_ipu4_device *isp = data;
	u64 tsc_hi, tsc_chk = 0;
	short retry = 1;

	do {
		tsc_hi = readl(isp->base + BUTTRESS_REG_TSC_HI);
		*val = readl(isp->base + BUTTRESS_REG_TSC_LO);
		tsc_chk = readl(isp->base + BUTTRESS_REG_TSC_HI);
		if (tsc_chk == tsc_hi) {
			*val |= (tsc_hi << 32);
			return 0;
		}
	} while (retry--);

	WARN_ON_ONCE(1);

	return -EINVAL;
}

DEFINE_SIMPLE_ATTRIBUTE(intel_ipu4_buttress_tsc_fops,
			intel_ipu4_buttress_tsc_get,
			NULL, /* intel_ipu4_buttress_tsc_set,  */"%llu\n");

int intel_ipu4_buttress_debugfs_init(struct intel_ipu4_device *isp)
{
	struct debugfs_reg32 *reg =
		devm_kcalloc(&isp->pdev->dev, ARRAY_SIZE(buttress_regs),
			     sizeof(*reg), GFP_KERNEL);
	struct dentry *dir, *file;
	int i;

	if (!reg)
		return -ENOMEM;

	dir = debugfs_create_dir(pci_name(isp->pdev), isp->intel_ipu4_dir);
	if (!dir)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(buttress_regs); i++, reg++) {
		reg->offset = (unsigned long)isp->base +
			buttress_regs[i].offset;
		reg->name = buttress_regs[i].name;
		file = debugfs_create_file(reg->name, S_IRWXU,
					   dir, reg,
					   &intel_ipu4_buttress_reg_fops);
		if (!file)
			goto err;
	}

	file = debugfs_create_file("start_tsc_sync", S_IWUSR, dir, isp,
				   &intel_ipu4_buttress_start_tsc_sync_fops);
	if (!file)
		goto err;
	file = debugfs_create_file("tsc", S_IRUSR, dir, isp,
				   &intel_ipu4_buttress_tsc_fops);
	if (!file)
		goto err;
	return 0;
err:
	debugfs_remove_recursive(dir);
	return -ENOMEM;
}

#endif /* CONFIG_DEBUG_FS */

void intel_ipu4_buttress_csi_port_config(struct intel_ipu4_device *isp,
					 u32 legacy, u32 combo)
{
	int combo_shift;

	if (is_intel_ipu4_hw_bxt_a0(isp))
		combo_shift = BUTTRESS_CSI2_PORT_CONFIG_AB_COMBO_SHIFT_A0;
	else
		combo_shift = BUTTRESS_CSI2_PORT_CONFIG_AB_COMBO_SHIFT_B0;

	writel((legacy & BUTTRESS_CSI2_PORT_CONFIG_AB_MUX_MASK) |
	       ((combo & BUTTRESS_CSI2_PORT_CONFIG_AB_MUX_MASK) << combo_shift),
	       isp->base + BUTTRESS_REG_CSI2_PORT_CONFIG_AB);
}
EXPORT_SYMBOL(intel_ipu4_buttress_csi_port_config);

int intel_ipu4_buttress_init(struct intel_ipu4_device *isp)
{
	struct intel_ipu4_buttress *b = &isp->buttress;
	int rval;

	mutex_init(&b->power_mutex);
	mutex_init(&b->auth_mutex);
	init_completion(&b->ish_ipc_complete);
	init_completion(&b->cse_ipc_complete);

	rval = intel_ipu4_buttress_clk_init(isp);
	if (rval) {
		mutex_destroy(&b->power_mutex);
		mutex_destroy(&b->auth_mutex);
		return rval;
	}

	intel_ipu4_buttress_set_secure_mode(isp);

	isp->secure_mode = intel_ipu4_buttress_get_secure_mode(isp);

	if (isp->secure_mode != secure_mode_enable)
		dev_warn(&isp->pdev->dev, "Unable to set secure mode!\n");

	if (isp->secure_mode) {
		dev_info(&isp->pdev->dev, "IPU4 in secure mode\n");
		rval = intel_ipu4_buttress_ipc_reset(
			isp, INTEL_IPU4_BUTTRESS_IPC_CSE);
		if (rval) {
			dev_err(&isp->pdev->dev, "IPC reset protocol failed!\n");
			return rval;
		}
	} else {
		dev_info(&isp->pdev->dev, "IPU4 in non-secure mode\n");
	}

	writel(BUTTRESS_IRQS, isp->base + BUTTRESS_REG_ISR_CLEAR);
	writel(BUTTRESS_IRQS, isp->base + BUTTRESS_REG_ISR_ENABLE);

	return 0;
}

void intel_ipu4_buttress_exit(struct intel_ipu4_device *isp)
{
	struct intel_ipu4_buttress *b = &isp->buttress;
	int i;

	writel(0, isp->base + BUTTRESS_REG_ISR_ENABLE);

	for (i = 0; i < INTEL_IPU4_BUTTRESS_NUM_OF_SENS_CKS; i++)
		clk_unregister(b->clk_sensor[i]);
	if (b->pll_sensor)
		clk_unregister(b->pll_sensor);

	mutex_destroy(&b->power_mutex);
	mutex_destroy(&b->auth_mutex);
}
