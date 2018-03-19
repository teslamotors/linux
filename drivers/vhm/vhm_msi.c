/*
 * virtio and hyperviosr service module (VHM): msi paravirt
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Jason Chen CJ <jason.cj.chen@intel.com>
 *
 */

#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/vhm/acrn_hv_defs.h>
#include <linux/vhm/vhm_hypercall.h>

#include "../pci/pci.h"

static struct msi_msg acrn_notify_msix_remap(struct msi_desc *entry,
				struct msi_msg *msg)
{
	volatile struct acrn_vm_pci_msix_remap notify;
	struct pci_dev *dev = msi_desc_to_pci_dev(entry);
	struct msi_msg remapped_msg = *msg;
	u16 msgctl;
	int ret;

	pci_read_config_word(dev, dev->msi_cap + PCI_MSI_FLAGS, &msgctl);

	notify.msi_ctl = msgctl;
	notify.virt_bdf = (dev->bus->number << 8) | dev->devfn;
	notify.msi_addr = msg->address_hi;
	notify.msi_addr <<= 32;
	notify.msi_addr |= msg->address_lo;
	notify.msi_data = msg->data;
	notify.msix = !!entry->msi_attrib.is_msix;

	if (notify.msix)
		notify.msix_entry_index = entry->msi_attrib.entry_nr;
	else
		notify.msix_entry_index = 0;

	ret = hcall_remap_pci_msix(0, virt_to_phys(&notify));
	if (ret < 0)
		dev_err(&dev->dev, "Failed to notify MSI/x change to HV\n");
	else {
		remapped_msg.address_hi = (unsigned int)(notify.msi_addr >> 32);
		remapped_msg.address_lo = (unsigned int)notify.msi_addr;
		remapped_msg.data = notify.msi_data;
	}
	return remapped_msg;
}

void acrn_write_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	struct pci_dev *dev = msi_desc_to_pci_dev(entry);
	struct msi_msg fmsg;

	if (dev->current_state != PCI_D0 || pci_dev_is_disconnected(dev)) {
		/* Don't touch the hardware now */
	} else if (entry->msi_attrib.is_msix) {
		void __iomem *base = pci_msix_desc_addr(entry);

		fmsg = acrn_notify_msix_remap(entry, msg);

		writel(fmsg.address_lo, base + PCI_MSIX_ENTRY_LOWER_ADDR);
		writel(fmsg.address_hi, base + PCI_MSIX_ENTRY_UPPER_ADDR);
		writel(fmsg.data, base + PCI_MSIX_ENTRY_DATA);
	} else {
		int pos = dev->msi_cap;
		u16 msgctl;

		fmsg = acrn_notify_msix_remap(entry, msg);

		pci_read_config_word(dev, pos + PCI_MSI_FLAGS, &msgctl);
		msgctl &= ~PCI_MSI_FLAGS_QSIZE;
		msgctl |= entry->msi_attrib.multiple << 4;
		pci_write_config_word(dev, pos + PCI_MSI_FLAGS, msgctl);

		pci_write_config_dword(dev, pos + PCI_MSI_ADDRESS_LO,
				       fmsg.address_lo);
		if (entry->msi_attrib.is_64) {
			pci_write_config_dword(dev, pos + PCI_MSI_ADDRESS_HI,
					       fmsg.address_hi);
			pci_write_config_word(dev, pos + PCI_MSI_DATA_64,
					      fmsg.data);
		} else {
			pci_write_config_word(dev, pos + PCI_MSI_DATA_32,
					      fmsg.data);
		}
	}
	entry->msg = *msg;
}
