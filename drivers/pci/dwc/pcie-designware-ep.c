// SPDX-License-Identifier: GPL-2.0
/**
 * Synopsys DesignWare PCIe Endpoint controller driver
 *
 * Copyright (C) 2017 Texas Instruments
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
 */

#include <linux/of.h>
#include <linux/delay.h>

#include "pcie-designware.h"
#include <linux/pci-epc.h>
#include <linux/pci-epf.h>

void dw_pcie_ep_linkup(struct dw_pcie_ep *ep)
{
	struct pci_epc *epc = ep->epc;

	pci_epc_linkup(epc);
}

static void __dw_pcie_ep_reset_bar(struct dw_pcie *pci, enum pci_barno bar,
				   int flags)
{
	u32 reg;

	reg = PCI_BASE_ADDRESS_0 + (4 * bar);
	dw_pcie_dbi_ro_wr_en(pci);
	dw_pcie_writel_dbi2(pci, reg, 0x0);
	dw_pcie_writel_dbi(pci, reg, 0x0);
	if (flags & PCI_BASE_ADDRESS_MEM_TYPE_64) {
		dw_pcie_writel_dbi2(pci, reg + 4, 0x0);
		dw_pcie_writel_dbi(pci, reg + 4, 0x0);
	}
	dw_pcie_dbi_ro_wr_dis(pci);
}

void dw_pcie_ep_reset_bar(struct dw_pcie *pci, enum pci_barno bar)
{
	__dw_pcie_ep_reset_bar(pci, bar, 0);
}

static int dw_pcie_ep_write_header(struct pci_epc *epc, u8 func_no,
				   struct pci_epf_header *hdr)
{
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	dw_pcie_dbi_ro_wr_en(pci);
	dw_pcie_writew_dbi(pci, PCI_VENDOR_ID, hdr->vendorid);
	dw_pcie_writew_dbi(pci, PCI_DEVICE_ID, hdr->deviceid);
	dw_pcie_writeb_dbi(pci, PCI_REVISION_ID, hdr->revid);
	dw_pcie_writeb_dbi(pci, PCI_CLASS_PROG, hdr->progif_code);
	dw_pcie_writew_dbi(pci, PCI_CLASS_DEVICE,
			   hdr->subclass_code | hdr->baseclass_code << 8);
	dw_pcie_writeb_dbi(pci, PCI_CACHE_LINE_SIZE,
			   hdr->cache_line_size);
	dw_pcie_writew_dbi(pci, PCI_SUBSYSTEM_VENDOR_ID,
			   hdr->subsys_vendor_id);
	dw_pcie_writew_dbi(pci, PCI_SUBSYSTEM_ID, hdr->subsys_id);
	dw_pcie_writeb_dbi(pci, PCI_INTERRUPT_PIN,
			   hdr->interrupt_pin);
	dw_pcie_dbi_ro_wr_dis(pci);

	return 0;
}

static int dw_pcie_ep_inbound_atu(struct dw_pcie_ep *ep, enum pci_barno bar,
				  dma_addr_t cpu_addr,
				  enum dw_pcie_as_type as_type)
{
	int ret;
	u32 free_win;
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	free_win = find_first_zero_bit(ep->ib_window_map, ep->num_ib_windows);
	if (free_win >= ep->num_ib_windows) {
		dev_err(pci->dev, "no free inbound window\n");
		return -EINVAL;
	}

	ret = dw_pcie_prog_inbound_atu(pci, free_win, bar, cpu_addr,
				       as_type);
	if (ret < 0) {
		dev_err(pci->dev, "Failed to program IB window\n");
		return ret;
	}

	ep->bar_to_atu[bar] = free_win;
	set_bit(free_win, ep->ib_window_map);

	return 0;
}

static int dw_pcie_ep_outbound_atu(struct dw_pcie_ep *ep, phys_addr_t phys_addr,
				   u64 pci_addr, size_t size)
{
	u32 free_win;
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	free_win = find_first_zero_bit(ep->ob_window_map, ep->num_ob_windows);
	if (free_win >= ep->num_ob_windows) {
		dev_err(pci->dev, "no free outbound window\n");
		return -EINVAL;
	}

	dw_pcie_prog_outbound_atu(pci, free_win, PCIE_ATU_TYPE_MEM,
				  phys_addr, pci_addr, size);

	set_bit(free_win, ep->ob_window_map);
	ep->outbound_addr[free_win] = phys_addr;

	return 0;
}

static void dw_pcie_ep_clear_bar(struct pci_epc *epc, u8 func_no,
				 struct pci_epf_bar *epf_bar)
{
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	enum pci_barno bar = epf_bar->barno;
	u32 atu_index = ep->bar_to_atu[bar];

	__dw_pcie_ep_reset_bar(pci, bar, epf_bar->flags);

	dw_pcie_disable_atu(pci, atu_index, DW_PCIE_REGION_INBOUND);
	clear_bit(atu_index, ep->ib_window_map);
}

static int dw_pcie_ep_set_bar(struct pci_epc *epc, u8 func_no,
			      struct pci_epf_bar *epf_bar)
{
	int ret;
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	enum pci_barno bar = epf_bar->barno;
	size_t size = epf_bar->size;
	size_t mask = epf_bar->mask;
	int flags = epf_bar->flags;
	enum dw_pcie_as_type as_type;
	u32 reg = PCI_BASE_ADDRESS_0 + (4 * bar);

	if (ep->ops->ep_set_mask)
		ep->ops->ep_set_mask(ep, reg, mask);

	if (!(flags & PCI_BASE_ADDRESS_SPACE))
		as_type = DW_PCIE_AS_MEM;
	else
		as_type = DW_PCIE_AS_IO;

	ret = dw_pcie_ep_inbound_atu(ep, bar, epf_bar->phys_addr, as_type);
	if (ret)
		return ret;

	dw_pcie_dbi_ro_wr_en(pci);
	dw_pcie_writel_dbi2(pci, reg, size - 1);
	dw_pcie_writel_dbi(pci, reg, flags);
	dw_pcie_dbi_ro_wr_dis(pci);

	return 0;
}

static int dw_pcie_find_index(struct dw_pcie_ep *ep, phys_addr_t addr,
			      u32 *atu_index)
{
	u32 index;

	for (index = 0; index < ep->num_ob_windows; index++) {
		if (ep->outbound_addr[index] != addr)
			continue;
		*atu_index = index;
		return 0;
	}

	return -EINVAL;
}

static void dw_pcie_ep_unmap_addr(struct pci_epc *epc, u8 func_no,
				  phys_addr_t addr)
{
	int ret;
	u32 atu_index;
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	ret = dw_pcie_find_index(ep, addr, &atu_index);
	if (ret < 0)
		return;

	dw_pcie_disable_atu(pci, atu_index, DW_PCIE_REGION_OUTBOUND);
	clear_bit(atu_index, ep->ob_window_map);
}

static int dw_pcie_ep_map_addr(struct pci_epc *epc, u8 func_no,
			       phys_addr_t addr,
			       u64 pci_addr, size_t size)
{
	int ret;
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	ret = dw_pcie_ep_outbound_atu(ep, addr, pci_addr, size);
	if (ret) {
		dev_err(pci->dev, "failed to enable address\n");
		return ret;
	}

	return 0;
}

#define DW_DMA_POLL_US  100
#define DW_DMA_TIMEOUT_US 1000000 /* 1 sec */
#define DW_DMA_MAX_POLL  (DW_DMA_TIMEOUT_US / DW_DMA_POLL_US)
/*
 * DMA between local/remote memory using CH0.
 */
static int dw_pcie_ep_dma(struct pci_epc *epc, u8 func_no,
						  dma_addr_t local_addr, phys_addr_t remote_addr,
						  size_t size, int direction)
{
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	dma_addr_t dst_addr_p, src_addr_p;
	struct device *dev = pci->dev;
	unsigned int status, iter = 0;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&epc->lock, flags);

	if (direction == DMA_FROM_DEVICE) {
		if (ep->dma_read_active) {
			ret = -EINPROGRESS;
			goto end;
		}

		ep->dma_read_active = true;
		src_addr_p = remote_addr;
		dst_addr_p = local_addr;

		/* enable the DMA read engine */
		dw_pcie_writel_dbi(pci, PF0_DMA_READ_ENGINE_EN_OFF, 0x1);

		/* clear the current set DMA completed/error status bits */
		dw_pcie_writel_dbi(pci, PF0_DMA_READ_INT_CLEAR_OFF,
						   DMA_CH0_COMPLETED_STATUS);
		/* unmask the dma completion interrupt bits */
		dw_pcie_writel_dbi(pci, PF0_DMA_READ_INT_MASK_OFF, 0x0);

		/* set the DMA control register to enable local interrupt enable */
		dw_pcie_writel_dbi(pci, PF0_DMA_CH_CONTROL1_OFF_RDCH_0, 0x04000008);
		/* set the DMA transfer size */
		dw_pcie_writel_dbi(pci, PF0_DMA_XFER_SIZE_OFF_RDCH_0, size);

		/* set the DMA source address bits */
		dw_pcie_writel_dbi(pci, PF0_DMA_SAR_LOW_OFF_RDCH_0,
						   src_addr_p & 0xffffffff);
		dw_pcie_writel_dbi(pci, PF0_DMA_SAR_HIGH_OFF_RDCH_0, src_addr_p >> 32);

		/* set the DMA dest address bits */
		dw_pcie_writel_dbi(pci, PF0_DMA_DAR_LOW_OFF_RDCH_0,
						   dst_addr_p & 0xffffffff);
		dw_pcie_writel_dbi(pci, PF0_DMA_DAR_HIGH_OFF_RDCH_0, dst_addr_p >> 32);

		/* kick the DMA on channel 0 */
		dw_pcie_writel_dbi(pci, PF0_DMA_READ_DOORBELL_OFF, 0);

		dev_dbg(dev, "Waiting for DMA to complete");
		/* wait for DMA completion success/error interrupt.
		 * TODO: block for a DMA completion interrupt instead of polling.
		 */
		do {
			spin_unlock_irqrestore(&epc->lock, flags);
			usleep_range(DW_DMA_POLL_US, DW_DMA_POLL_US + 10);
			spin_lock_irqsave(&epc->lock, flags);
			status = dw_pcie_readl_dbi(pci, PF0_DMA_READ_INT_STATUS_OFF);
		} while (!(status & DMA_CH0_COMPLETED_STATUS) &&
				 (++iter < DW_DMA_MAX_POLL));

		ep->dma_read_active = false;
		if (!(status & DMA_CH0_COMPLETED_STATUS) && (iter == DW_DMA_MAX_POLL)) {
			dw_pcie_writel_dbi(pci, PF0_DMA_READ_DOORBELL_OFF,
							   DMA_DOORBELL_STOP);
			ret = -ETIMEDOUT;
		} else if (status & DMA_CH0_ABORT) {
			ret = -EIO;
		}
	} else if (direction == DMA_TO_DEVICE) {
		src_addr_p = local_addr;
		dst_addr_p = remote_addr;
		if (ep->dma_write_active) {
			ret = -EINPROGRESS;
			goto end;
		}

		ep->dma_write_active = true;
		/* enable the DMA write engine */
		dw_pcie_writel_dbi(pci, PF0_DMA_WRITE_ENGINE_EN_OFF, 0x1);

		/* clear the current set DMA completed/error status bits */
		dw_pcie_writel_dbi(pci, PF0_DMA_WRITE_INT_CLEAR_OFF,
						   DMA_CH0_COMPLETED_STATUS);
		/* unmask the dma completion interrupt bits */
		dw_pcie_writel_dbi(pci, PF0_DMA_WRITE_INT_MASK_OFF, 0x0);

		/* set the DMA control register to enable local interrupt enable */
		dw_pcie_writel_dbi(pci, PF0_DMA_CH_CONTROL1_OFF_WRCH_0, 0x04000008);
		/* set the DMA transfer size */
		dw_pcie_writel_dbi(pci, PF0_DMA_XFER_SIZE_OFF_WRCH_0, size);

		/* set the DMA source address bits */
		dw_pcie_writel_dbi(pci, PF0_DMA_SAR_LOW_OFF_WRCH_0,
						   src_addr_p & 0xffffffff);
		dw_pcie_writel_dbi(pci, PF0_DMA_SAR_HIGH_OFF_WRCH_0, src_addr_p >> 32);

		/* set the DMA dst address bits */
		dw_pcie_writel_dbi(pci, PF0_DMA_DAR_LOW_OFF_WRCH_0,
						   dst_addr_p & 0xffffffff);
		dw_pcie_writel_dbi(pci, PF0_DMA_DAR_HIGH_OFF_WRCH_0, dst_addr_p >> 32);

		/* kick the DMA on channel 0 */
		dw_pcie_writel_dbi(pci, PF0_DMA_WRITE_DOORBELL_OFF, 0x0);

		dev_dbg(dev, "Waiting for DMA to complete");
		/* wait for DMA completion success/error interrupt.
		 * TODO: block for a DMA completion interrupt instead of polling.
		 */
		do {
			spin_unlock_irqrestore(&epc->lock, flags);
			usleep_range(DW_DMA_POLL_US, DW_DMA_POLL_US + 10);
			spin_lock_irqsave(&epc->lock, flags);
			status = dw_pcie_readl_dbi(pci, PF0_DMA_WRITE_INT_STATUS_OFF);
		} while (!(status & DMA_CH0_COMPLETED_STATUS) &&
				 (++iter < DW_DMA_MAX_POLL));

		ep->dma_write_active = false;
		if (iter == DW_DMA_MAX_POLL) {
			dw_pcie_writel_dbi(pci, PF0_DMA_WRITE_DOORBELL_OFF,
							   DMA_DOORBELL_STOP);
			ret = -ETIMEDOUT;
		} else if (status & DMA_CH0_ABORT) {
			ret = -EIO;
		}
	}

end:
	spin_unlock_irqrestore(&epc->lock, flags);

	if (ret)
		dev_err(dev, "DMA completed with error: %d", ret);
	else
		dev_dbg(dev, "DMA complete");
	return ret;
}

static int dw_pcie_ep_get_msi(struct pci_epc *epc, u8 func_no)
{
	int val;
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	val = dw_pcie_readw_dbi(pci, MSI_MESSAGE_CONTROL);
	if (!(val & MSI_CAP_MSI_EN_MASK))
		return -EINVAL;

	val = (val & MSI_CAP_MME_MASK) >> MSI_CAP_MME_SHIFT;
	return val;
}

static int dw_pcie_ep_set_msi(struct pci_epc *epc, u8 func_no, u8 encode_int)
{
	int val;
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	val = dw_pcie_readw_dbi(pci, MSI_MESSAGE_CONTROL);
	val &= ~MSI_CAP_MMC_MASK;
	val |= (encode_int << MSI_CAP_MMC_SHIFT) & MSI_CAP_MMC_MASK;
	dw_pcie_dbi_ro_wr_en(pci);
	dw_pcie_writew_dbi(pci, MSI_MESSAGE_CONTROL, val);
	dw_pcie_dbi_ro_wr_dis(pci);

	return 0;
}

static int dw_pcie_ep_raise_irq(struct pci_epc *epc, u8 func_no,
				enum pci_epc_irq_type type, u8 interrupt_num)
{
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);

	if (!ep->ops->raise_irq)
		return -EINVAL;

	return ep->ops->raise_irq(ep, func_no, type, interrupt_num);
}

static void dw_pcie_ep_stop(struct pci_epc *epc)
{
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	if (!pci->ops->stop_link)
		return;

	pci->ops->stop_link(pci);
}

static int dw_pcie_ep_start(struct pci_epc *epc)
{
	struct dw_pcie_ep *ep = epc_get_drvdata(epc);
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	if (!pci->ops->start_link)
		return -EINVAL;

	return pci->ops->start_link(pci);
}

static const struct pci_epc_ops epc_ops = {
	.write_header		= dw_pcie_ep_write_header,
	.set_bar		= dw_pcie_ep_set_bar,
	.clear_bar		= dw_pcie_ep_clear_bar,
	.map_addr		= dw_pcie_ep_map_addr,
	.unmap_addr		= dw_pcie_ep_unmap_addr,
	.set_msi		= dw_pcie_ep_set_msi,
	.get_msi		= dw_pcie_ep_get_msi,
	.raise_irq		= dw_pcie_ep_raise_irq,
	.start			= dw_pcie_ep_start,
	.stop			= dw_pcie_ep_stop,
	.dma			= dw_pcie_ep_dma,
};

int dw_pcie_ep_raise_msi_irq(struct dw_pcie_ep *ep, u8 func_no,
			     u8 interrupt_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct pci_epc *epc = ep->epc;
	u16 msg_ctrl, msg_data;
	u32 msg_addr_lower, msg_addr_upper;
	u64 msg_addr;
	bool has_upper;
	int ret;

	/* Raise MSI per the PCI Local Bus Specification Revision 3.0, 6.8.1. */
	msg_ctrl = dw_pcie_readw_dbi(pci, MSI_MESSAGE_CONTROL);
	has_upper = !!(msg_ctrl & PCI_MSI_FLAGS_64BIT);
	msg_addr_lower = dw_pcie_readl_dbi(pci, MSI_MESSAGE_ADDR_L32);
	if (has_upper) {
		msg_addr_upper = dw_pcie_readl_dbi(pci, MSI_MESSAGE_ADDR_U32);
		msg_data = dw_pcie_readw_dbi(pci, MSI_MESSAGE_DATA_64);
	} else {
		msg_addr_upper = 0;
		msg_data = dw_pcie_readw_dbi(pci, MSI_MESSAGE_DATA_32);
	}
	msg_addr = ((u64) msg_addr_upper) << 32 | msg_addr_lower;
	ret = dw_pcie_ep_map_addr(epc, func_no, ep->msi_mem_phys, msg_addr,
				  epc->mem->page_size);
	if (ret)
		return ret;

	writel(msg_data | (interrupt_num - 1), ep->msi_mem);

	dw_pcie_ep_unmap_addr(epc, func_no, ep->msi_mem_phys);

	return 0;
}

void dw_pcie_ep_exit(struct dw_pcie_ep *ep)
{
	struct pci_epc *epc = ep->epc;

	pci_epc_mem_free_addr(epc, ep->msi_mem_phys, ep->msi_mem,
			      epc->mem->page_size);

	pci_epc_mem_exit(epc);
}

int dw_pcie_ep_init(struct dw_pcie_ep *ep)
{
	int ret;
	void *addr;
	struct pci_epc *epc;
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct device *dev = pci->dev;
	struct device_node *np = dev->of_node;

	if (!pci->dbi_base || !pci->dbi_base2) {
		dev_err(dev, "dbi_base/dbi_base2 is not populated\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "num-ib-windows", &ep->num_ib_windows);
	if (ret < 0) {
		dev_err(dev, "unable to read *num-ib-windows* property\n");
		return ret;
	}
	if (ep->num_ib_windows > MAX_IATU_IN) {
		dev_err(dev, "invalid *num-ib-windows*\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "num-ob-windows", &ep->num_ob_windows);
	if (ret < 0) {
		dev_err(dev, "unable to read *num-ob-windows* property\n");
		return ret;
	}
	if (ep->num_ob_windows > MAX_IATU_OUT) {
		dev_err(dev, "invalid *num-ob-windows*\n");
		return -EINVAL;
	}

	ep->ib_window_map = devm_kzalloc(dev, sizeof(long) *
					 BITS_TO_LONGS(ep->num_ib_windows),
					 GFP_KERNEL);
	if (!ep->ib_window_map)
		return -ENOMEM;

	ep->ob_window_map = devm_kzalloc(dev, sizeof(long) *
					 BITS_TO_LONGS(ep->num_ob_windows),
					 GFP_KERNEL);
	if (!ep->ob_window_map)
		return -ENOMEM;

	addr = devm_kzalloc(dev, sizeof(phys_addr_t) * ep->num_ob_windows,
			    GFP_KERNEL);
	if (!addr)
		return -ENOMEM;
	ep->outbound_addr = addr;

	if (ep->ops->ep_init)
		ep->ops->ep_init(ep);

	epc = devm_pci_epc_create(dev, &epc_ops);
	if (IS_ERR(epc)) {
		dev_err(dev, "failed to create epc device\n");
		return PTR_ERR(epc);
	}

	ret = of_property_read_u8(np, "max-functions", &epc->max_functions);
	if (ret < 0)
		epc->max_functions = 1;

	ret = __pci_epc_mem_init(epc, ep->phys_base, ep->addr_size,
				 ep->page_size);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize address space\n");
		return ret;
	}

	ep->msi_mem = pci_epc_mem_alloc_addr(epc, &ep->msi_mem_phys,
					     epc->mem->page_size);
	if (!ep->msi_mem) {
		dev_err(dev, "Failed to reserve memory for MSI\n");
		return -ENOMEM;
	}

	ep->epc = epc;
	epc_set_drvdata(epc, ep);
	dw_pcie_setup(pci);

	return 0;
}
