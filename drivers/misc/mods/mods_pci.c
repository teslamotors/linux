/*
 * mods_pci.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

#include <linux/io.h>
#include <linux/fs.h>

/************************
 * PCI HELPER FUNCTIONS *
 ************************/

static int mods_free_pci_res_map(struct file *fp,
				 struct MODS_PCI_RES_MAP_INFO *p_del_map)
{
#if defined(MODS_HAS_PCI_MAP_RESOURCE)
	struct MODS_PCI_RES_MAP_INFO *p_res_map;

	MODS_PRIVATE_DATA(private_data, fp);
	struct list_head  *head;
	struct list_head  *iter;

	mods_debug_printk(DEBUG_PCICFG,
			  "free pci resource map %p\n",
			  p_del_map);

	if (unlikely(mutex_lock_interruptible(&private_data->mtx)))
		return -EINTR;

	head = private_data->mods_pci_res_map_list;

	list_for_each(iter, head) {
		p_res_map =
			list_entry(iter, struct MODS_PCI_RES_MAP_INFO, list);

		if (p_del_map == p_res_map) {
			list_del(iter);

			mutex_unlock(&private_data->mtx);

			pci_unmap_resource(p_res_map->dev,
					   p_res_map->va,
					   p_res_map->page_count * PAGE_SIZE,
					   PCI_DMA_BIDIRECTIONAL);
			mods_debug_printk(DEBUG_PCICFG,
					  "unmapped pci resource at 0x%llx from %u:%u:%u.%u\n",
					  p_res_map->va,
					  pci_domain_nr(p_res_map->dev->bus),
					  p_res_map->dev->bus->number,
					  PCI_SLOT(p_res_map->dev->devfn),
					  PCI_FUNC(p_res_map->dev->devfn));
			kfree(p_res_map);
			return OK;
		}
	}

	mutex_unlock(&private_data->mtx);

	mods_error_printk("failed to unregister pci resource mapping %p\n",
			  p_del_map);
	return -EINVAL;
#else
	return OK;
#endif
}

int mods_unregister_all_pci_res_mappings(struct file *fp)
{
	MODS_PRIVATE_DATA(private_data, fp);
	struct list_head *head = private_data->mods_pci_res_map_list;
	struct list_head *iter;
	struct list_head *tmp;

	list_for_each_safe(iter, tmp, head) {
		struct MODS_PCI_RES_MAP_INFO *p_pci_res_map_info;
		int ret;

		p_pci_res_map_info =
			list_entry(iter, struct MODS_PCI_RES_MAP_INFO, list);
		ret = mods_free_pci_res_map(fp, p_pci_res_map_info);
		if (ret)
			return ret;
	}

	return OK;
}

/************************
 * PCI ESCAPE FUNCTIONS *
 ************************/

int esc_mods_find_pci_dev_2(struct file *pfile,
			    struct MODS_FIND_PCI_DEVICE_2 *p)
{
	struct pci_dev *dev;
	int index = 0;

	mods_debug_printk(DEBUG_PCICFG,
			  "find pci dev %04x:%04x, index %d\n",
			  (int) p->vendor_id,
			  (int) p->device_id,
			  (int) p->index);

	dev = pci_get_device(p->vendor_id, p->device_id, NULL);

	while (dev) {
		if (index == p->index) {
			p->pci_device.domain	= pci_domain_nr(dev->bus);
			p->pci_device.bus	= dev->bus->number;
			p->pci_device.device	= PCI_SLOT(dev->devfn);
			p->pci_device.function	= PCI_FUNC(dev->devfn);
			return OK;
		}
		dev = pci_get_device(p->vendor_id, p->device_id, dev);
		index++;
	}

	return -EINVAL;
}

int esc_mods_find_pci_dev(struct file *pfile,
			  struct MODS_FIND_PCI_DEVICE *p)
{
	struct pci_dev *dev;
	int index = 0;

	mods_debug_printk(DEBUG_PCICFG,
			  "find pci dev %04x:%04x, index %d\n",
			  (int) p->vendor_id,
			  (int) p->device_id,
			  (int) p->index);

	dev = pci_get_device(p->vendor_id, p->device_id, NULL);

	while (dev) {
		if (index == p->index && pci_domain_nr(dev->bus) == 0) {
			p->bus_number		= dev->bus->number;
			p->device_number	= PCI_SLOT(dev->devfn);
			p->function_number	= PCI_FUNC(dev->devfn);
			return OK;
		}
		/* Only return devices in the first domain, but don't assume
		   that they're the first devices in the list */
		if (pci_domain_nr(dev->bus) == 0)
			index++;
		dev = pci_get_device(p->vendor_id, p->device_id, dev);
	}

	return -EINVAL;
}

int esc_mods_find_pci_class_code_2(struct file *pfile,
				   struct MODS_FIND_PCI_CLASS_CODE_2 *p)
{
	struct pci_dev *dev;
	int index = 0;

	mods_debug_printk(DEBUG_PCICFG, "find pci class code %04x, index %d\n",
			  (int) p->class_code, (int) p->index);

	dev = pci_get_class(p->class_code, NULL);

	while (dev) {
		if (index == p->index) {
			p->pci_device.domain	= pci_domain_nr(dev->bus);
			p->pci_device.bus	= dev->bus->number;
			p->pci_device.device	= PCI_SLOT(dev->devfn);
			p->pci_device.function	= PCI_FUNC(dev->devfn);
			return OK;
		}
		dev = pci_get_class(p->class_code, dev);
		index++;
	}

	return -EINVAL;
}

int esc_mods_find_pci_class_code(struct file *pfile,
				 struct MODS_FIND_PCI_CLASS_CODE *p)
{
	struct pci_dev *dev;
	int index = 0;

	mods_debug_printk(DEBUG_PCICFG, "find pci class code %04x, index %d\n",
			  (int) p->class_code, (int) p->index);

	dev = pci_get_class(p->class_code, NULL);

	while (dev) {
		if (index == p->index && pci_domain_nr(dev->bus) == 0) {
			p->bus_number		= dev->bus->number;
			p->device_number	= PCI_SLOT(dev->devfn);
			p->function_number	= PCI_FUNC(dev->devfn);
			return OK;
		}
		/* Only return devices in the first domain, but don't assume
		   that they're the first devices in the list */
		if (pci_domain_nr(dev->bus) == 0)
			index++;
		dev = pci_get_class(p->class_code, dev);
	}

	return -EINVAL;
}

int esc_mods_pci_get_bar_info_2(struct file *pfile,
				struct MODS_PCI_GET_BAR_INFO_2 *p)
{
	struct pci_dev *dev;
	unsigned int devfn, bar_resource_offset, i;
#if !defined(MODS_HAS_IORESOURCE_MEM_64)
	__u32 temp;
#endif

	devfn = PCI_DEVFN(p->pci_device.device, p->pci_device.function);
	dev = MODS_PCI_GET_SLOT(p->pci_device.domain, p->pci_device.bus, devfn);

	if (dev == NULL)
		return -EINVAL;

	mods_debug_printk(DEBUG_PCICFG,
			  "pci get bar info %04x:%x:%02x:%x, bar index %d\n",
			  (int) p->pci_device.domain,
			  (int) p->pci_device.bus, (int) p->pci_device.device,
			  (int) p->pci_device.function, (int) p->bar_index);

	bar_resource_offset = 0;
	for (i = 0; i < p->bar_index; i++) {
#if defined(MODS_HAS_IORESOURCE_MEM_64)
		if (pci_resource_flags(dev, bar_resource_offset)
		    & IORESOURCE_MEM_64) {
#else
		pci_read_config_dword(dev,
				      (PCI_BASE_ADDRESS_0
				       + (bar_resource_offset * 4)),
				      &temp);
		if (temp & PCI_BASE_ADDRESS_MEM_TYPE_64) {
#endif
			bar_resource_offset += 2;
		} else {
			bar_resource_offset += 1;
		}
	}
	p->base_address = pci_resource_start(dev, bar_resource_offset);
	p->bar_size	= pci_resource_len(dev, bar_resource_offset);

	return OK;
}

int esc_mods_pci_get_bar_info(struct file *pfile,
			      struct MODS_PCI_GET_BAR_INFO *p)
{
	int retval;
	struct MODS_PCI_GET_BAR_INFO_2 get_bar_info = { {0} };
	get_bar_info.pci_device.domain		= 0;
	get_bar_info.pci_device.bus		= p->pci_device.bus;
	get_bar_info.pci_device.device		= p->pci_device.device;
	get_bar_info.pci_device.function	= p->pci_device.function;
	get_bar_info.bar_index			= p->bar_index;

	retval = esc_mods_pci_get_bar_info_2(pfile, &get_bar_info);
	if (retval)
		return retval;

	p->base_address	= get_bar_info.base_address;
	p->bar_size	= get_bar_info.bar_size;
	return OK;
}

int esc_mods_pci_get_irq_2(struct file *pfile,
			   struct MODS_PCI_GET_IRQ_2 *p)
{
	struct pci_dev *dev;
	unsigned int devfn;

	devfn = PCI_DEVFN(p->pci_device.device, p->pci_device.function);
	dev = MODS_PCI_GET_SLOT(p->pci_device.domain, p->pci_device.bus, devfn);

	if (dev == NULL)
		return -EINVAL;

	mods_debug_printk(DEBUG_PCICFG,
			  "pci get irq %04x:%x:%02x:%x\n",
			  (int) p->pci_device.domain,
			  (int) p->pci_device.bus, (int) p->pci_device.device,
			  (int) p->pci_device.function);

	p->irq = dev->irq;

	return OK;
}

int esc_mods_pci_get_irq(struct file *pfile,
			 struct MODS_PCI_GET_IRQ *p)
{
	int retval;
	struct MODS_PCI_GET_IRQ_2 get_irq = { {0} };
	get_irq.pci_device.domain	= 0;
	get_irq.pci_device.bus		= p->pci_device.bus;
	get_irq.pci_device.device	= p->pci_device.device;
	get_irq.pci_device.function	= p->pci_device.function;

	retval = esc_mods_pci_get_irq_2(pfile, &get_irq);
	if (retval)
		return retval;

	p->irq = get_irq.irq;
	return OK;
}

int esc_mods_pci_read_2(struct file *pfile, struct MODS_PCI_READ_2 *p)
{
	struct pci_dev *dev;
	unsigned int devfn;

	devfn = PCI_DEVFN(p->pci_device.device, p->pci_device.function);
	dev = MODS_PCI_GET_SLOT(p->pci_device.domain, p->pci_device.bus, devfn);

	if (dev == NULL)
		return -EINVAL;

	mods_debug_printk(DEBUG_PCICFG,
			  "pci read %04x:%x:%02x.%x, addr 0x%04x, size %d\n",
			  (int) p->pci_device.domain,
			  (int) p->pci_device.bus, (int) p->pci_device.device,
			  (int) p->pci_device.function, (int) p->address,
			  (int) p->data_size);

	p->data = 0;
	switch (p->data_size) {
	case 1:
		pci_read_config_byte(dev, p->address, (u8 *) &p->data);
		break;
	case 2:
		pci_read_config_word(dev, p->address, (u16 *) &p->data);
		break;
	case 4:
		pci_read_config_dword(dev, p->address, (u32 *) &p->data);
		break;
	default:
		return -EINVAL;
	}
	return OK;
}

int esc_mods_pci_read(struct file *pfile, struct MODS_PCI_READ *p)
{
	int retval;
	struct MODS_PCI_READ_2 pci_read = { {0} };
	pci_read.pci_device.domain	= 0;
	pci_read.pci_device.bus		= p->bus_number;
	pci_read.pci_device.device	= p->device_number;
	pci_read.pci_device.function	= p->function_number;
	pci_read.address		= p->address;
	pci_read.data_size		= p->data_size;

	retval = esc_mods_pci_read_2(pfile, &pci_read);
	if (retval)
		return retval;

	p->data = pci_read.data;
	return OK;
}

int esc_mods_pci_write_2(struct file *pfile, struct MODS_PCI_WRITE_2 *p)
{
	struct pci_dev *dev;
	unsigned int devfn;

	mods_debug_printk(DEBUG_PCICFG,
			  "pci write %04x:%x:%02x.%x, addr 0x%04x, size %d, "
			  "data 0x%x\n",
			  (int) p->pci_device.domain,
			  (int) p->pci_device.bus, (int) p->pci_device.device,
			  (int) p->pci_device.function,
			  (int) p->address, (int) p->data_size, (int) p->data);

	devfn = PCI_DEVFN(p->pci_device.device, p->pci_device.function);
	dev = MODS_PCI_GET_SLOT(p->pci_device.domain, p->pci_device.bus, devfn);

	if (dev == NULL) {
		mods_error_printk(
		  "pci write to %04x:%x:%02x.%x, addr 0x%04x, size %d failed\n",
		    (unsigned)p->pci_device.domain,
		    (unsigned)p->pci_device.bus,
		    (unsigned)p->pci_device.device,
		    (unsigned)p->pci_device.function,
		    (unsigned)p->address,
		    (int)p->data_size);
		return -EINVAL;
	}

	switch (p->data_size) {
	case 1:
		pci_write_config_byte(dev, p->address, p->data);
		break;
	case 2:
		pci_write_config_word(dev, p->address, p->data);
		break;
	case 4:
		pci_write_config_dword(dev, p->address, p->data);
		break;
	default:
		return -EINVAL;
	}
	return OK;
}

int esc_mods_pci_write(struct file *pfile,
		       struct MODS_PCI_WRITE *p)
{
	struct MODS_PCI_WRITE_2 pci_write = { {0} };
	pci_write.pci_device.domain	= 0;
	pci_write.pci_device.bus	= p->bus_number;
	pci_write.pci_device.device	= p->device_number;
	pci_write.pci_device.function	= p->function_number;
	pci_write.address		= p->address;
	pci_write.data			= p->data;
	pci_write.data_size		= p->data_size;

	return esc_mods_pci_write_2(pfile, &pci_write);
}

int esc_mods_pci_bus_add_dev(struct file *pfile,
			     struct MODS_PCI_BUS_ADD_DEVICES *scan)
{
#if defined(CONFIG_PCI)
	mods_info_printk("scanning pci bus %x\n", scan->bus);

	/* initiate a PCI bus scan to find hotplugged PCI devices in domain 0 */
	pci_scan_child_bus(pci_find_bus(0, scan->bus));

	/* add newly found devices */
	pci_bus_add_devices(pci_find_bus(0, scan->bus));

	return OK;
#else
	return -EINVAL;
#endif
}

int esc_mods_pci_hot_reset(struct file *pfile,
			   struct MODS_PCI_HOT_RESET *p)
{
#if defined(CONFIG_PPC64)
	struct pci_dev *dev;
	unsigned int devfn;
	int retval;

	mods_debug_printk(DEBUG_PCICFG,
			  "pci_hot_reset %04x:%x:%02x.%x\n",
			  (int) p->pci_device.domain,
			  (int) p->pci_device.bus,
			  (int) p->pci_device.device,
			  (int) p->pci_device.function);

	devfn = PCI_DEVFN(p->pci_device.device, p->pci_device.function);
	dev = MODS_PCI_GET_SLOT(p->pci_device.domain, p->pci_device.bus, devfn);

	if (dev == NULL) {
		mods_error_printk(
		    "pci_hot_reset cannot find pci device %04x:%x:%02x.%x\n",
		    (unsigned)p->pci_device.domain,
		    (unsigned)p->pci_device.bus,
		    (unsigned)p->pci_device.device,
		    (unsigned)p->pci_device.function);
		return -EINVAL;
	}

	retval = pci_set_pcie_reset_state(dev, pcie_hot_reset);
	if (retval) {
		mods_error_printk(
		    "pci_hot_reset failed on %04x:%x:%02x.%x\n",
		    (unsigned)p->pci_device.domain,
		    (unsigned)p->pci_device.bus,
		    (unsigned)p->pci_device.device,
		    (unsigned)p->pci_device.function);
		return retval;
	}

	retval = pci_set_pcie_reset_state(dev, pcie_deassert_reset);
	if (retval) {
		mods_error_printk(
		    "pci_hot_reset deassert failed on %04x:%x:%02x.%x\n",
		    (unsigned)p->pci_device.domain,
		    (unsigned)p->pci_device.bus,
		    (unsigned)p->pci_device.device,
		    (unsigned)p->pci_device.function);
		return retval;
	}

	return OK;
#else
	return -EINVAL;
#endif
}

/************************
 * PIO ESCAPE FUNCTIONS *
 ************************/

int esc_mods_pio_read(struct file *pfile, struct MODS_PIO_READ *p)
{
	LOG_ENT();
	switch (p->data_size) {
	case 1:
		p->data = inb(p->port);
		break;
	case 2:
		p->data = inw(p->port);
		break;
	case 4:
		p->data = inl(p->port);
		break;
	default:
		return -EINVAL;
	}
	LOG_EXT();
	return OK;
}

int esc_mods_pio_write(struct file *pfile, struct MODS_PIO_WRITE  *p)
{
	LOG_ENT();
	switch (p->data_size) {
	case 1:
		outb(p->data, p->port);
		break;
	case 2:
		outw(p->data, p->port);
		break;
	case 4:
		outl(p->data, p->port);
		break;
	default:
		return -EINVAL;
	}
	LOG_EXT();
	return OK;
}

int esc_mods_device_numa_info_2(struct file *fp,
				struct MODS_DEVICE_NUMA_INFO_2 *p)
{
#ifdef MODS_HAS_DEV_TO_NUMA_NODE
	unsigned int devfn = PCI_DEVFN(p->pci_device.device,
				       p->pci_device.function);
	struct pci_dev *dev = MODS_PCI_GET_SLOT(p->pci_device.domain,
						p->pci_device.bus, devfn);

	LOG_ENT();

	if (dev == NULL) {
		mods_error_printk("PCI device %u:%u:%u.%u not found\n",
				  p->pci_device.domain,
				  p->pci_device.bus, p->pci_device.device,
				  p->pci_device.function);
		LOG_EXT();
		return -EINVAL;
	}

	p->node = dev_to_node(&dev->dev);
	if (-1 != p->node) {
		const unsigned long *maskp
			= cpumask_bits(cpumask_of_node(p->node));
		unsigned int i, word, bit, maskidx;

		if (((nr_cpumask_bits + 31) / 32) > MAX_CPU_MASKS) {
			mods_error_printk("too many CPUs (%d) for mask bits\n",
					  nr_cpumask_bits);
			LOG_EXT();
			return -EINVAL;
		}

		for (i = 0, maskidx = 0;
		     i < nr_cpumask_bits;
		     i += 32, maskidx++) {
			word = i / BITS_PER_LONG;
			bit = i % BITS_PER_LONG;
			p->node_cpu_mask[maskidx]
				= (maskp[word] >> bit) & 0xFFFFFFFFUL;
		}
	}
	p->node_count = num_possible_nodes();
	p->cpu_count = num_possible_cpus();

	LOG_EXT();
	return OK;
#else
	return -EINVAL;
#endif
}

int esc_mods_device_numa_info(struct file *fp,
			      struct MODS_DEVICE_NUMA_INFO *p)
{
	int retval, i;
	struct MODS_DEVICE_NUMA_INFO_2 numa_info = { {0} };
	numa_info.pci_device.domain	= 0;
	numa_info.pci_device.bus	= p->pci_device.bus;
	numa_info.pci_device.device	= p->pci_device.device;
	numa_info.pci_device.function	= p->pci_device.function;

	retval = esc_mods_device_numa_info_2(fp, &numa_info);
	if (retval)
		return retval;

	p->node				= numa_info.node;
	p->node_count			= numa_info.node_count;
	for (i = 0; i < MAX_CPU_MASKS; i++)
		p->node_cpu_mask[i]	= numa_info.node_cpu_mask[i];
	p->cpu_count			= numa_info.cpu_count;
	return OK;
}

int esc_mods_pci_map_resource(struct file *fp,
			      struct MODS_PCI_MAP_RESOURCE  *p)
{
#if defined(MODS_HAS_PCI_MAP_RESOURCE)
	MODS_PRIVATE_DATA(private_data, fp);
	unsigned int devfn;
	struct pci_dev *rem_dev;
	struct pci_dev *loc_dev;
	struct MODS_PCI_RES_MAP_INFO *p_res_map;

	LOG_ENT();

	devfn = PCI_DEVFN(p->local_pci_device.device,
			  p->local_pci_device.function);
	loc_dev = MODS_PCI_GET_SLOT(p->local_pci_device.domain,
				    p->local_pci_device.bus, devfn);
	if (!loc_dev) {
		mods_error_printk("Local PCI device %u:%u:%u.%u not found\n",
				  p->local_pci_device.domain,
				  p->local_pci_device.bus,
				  p->local_pci_device.device,
				  p->local_pci_device.function);
		LOG_EXT();
		return -EINVAL;
	}

	devfn = PCI_DEVFN(p->remote_pci_device.device,
			  p->remote_pci_device.function);
	rem_dev = MODS_PCI_GET_SLOT(p->remote_pci_device.domain,
				    p->remote_pci_device.bus, devfn);
	if (!rem_dev) {
		mods_error_printk("Remote PCI device %u:%u:%u.%u not found\n",
				  p->remote_pci_device.domain,
				  p->remote_pci_device.bus,
				  p->remote_pci_device.device,
				  p->remote_pci_device.function);
		LOG_EXT();
		return -EINVAL;
	}

	if ((p->resource_index >= DEVICE_COUNT_RESOURCE) ||
	    !pci_resource_len(rem_dev, p->resource_index)) {
		mods_error_printk(
			"Resource %u on device %u:%u:%u.%u not found\n",
			p->resource_index,
			p->remote_pci_device.domain,
			p->remote_pci_device.bus,
			p->remote_pci_device.device,
			p->remote_pci_device.function);
		LOG_EXT();
		return -EINVAL;
	}

	if ((p->va < pci_resource_start(rem_dev, p->resource_index)) ||
	    (p->va > pci_resource_end(rem_dev, p->resource_index)) ||
	    (p->va + p->page_count * PAGE_SIZE >
			pci_resource_end(rem_dev, p->resource_index))) {
		mods_error_printk(
			"Invalid resource address 0x%llx on device %u:%u:%u.%u "
			"not found\n",
			(unsigned long long)p->va,
			p->remote_pci_device.domain,
			p->remote_pci_device.bus,
			p->remote_pci_device.device,
			p->remote_pci_device.function);
		LOG_EXT();
		return -EINVAL;
	}

	p_res_map = kmalloc(sizeof(struct MODS_PCI_RES_MAP_INFO), GFP_KERNEL);
	if (unlikely(!p_res_map)) {
		mods_error_printk("failed to allocate pci res map struct\n");
		LOG_EXT();
		return -ENOMEM;
	}

	p_res_map->dev = loc_dev;
	p_res_map->page_count = p->page_count;
	p_res_map->va = pci_map_resource(loc_dev,
		&rem_dev->resource[resource_index],
		p->va - pci_resource_start(rem_dev, p->resource_index),
		p->page_count * PAGE_SIZE,
		PCI_DMA_BIDIRECTIONAL);
	p_res_map->va = p->va;
	if (pci_dma_mapping_error(loc_dev, p_res_map->va)) {
		kfree(p_res_map);
		LOG_EXT();
		return -ENOMEM;
	}

	if (unlikely(mutex_lock_interruptible(&private_data->mtx))) {
		kfree(p_res_map);
		LOG_EXT();
		return -EINTR;
	}
	list_add(&p_res_map->list, private_data->mods_pci_res_map_list);
	mutex_unlock(&private_data->mtx);

	p->va = p_res_map->va;

	mods_debug_printk(DEBUG_PCICFG,
			  "mapped pci resource %u from %u:%u:%u.%u to %u:%u:%u.%u at 0x%llx\n",
			  p->resource_index,
			  p->remote_pci_device.domain,
			  p->remote_pci_device.bus,
			  p->remote_pci_device.device,
			  p->remote_pci_device.function,
			  p->local_pci_device.domain,
			  p->local_pci_device.bus,
			  p->local_pci_device.device,
			  p->local_pci_device.function,
			  p->va);
#else
	/*
	 * We still return OK, in case the system is running an older kernel
	 * with the IOMMU disabled. The va parameter will still contain the
	 * input physical address, which is what the device should use in this
	 * fallback case.
	 */
#endif
	return OK;
}

int esc_mods_pci_unmap_resource(struct file *fp,
				struct MODS_PCI_UNMAP_RESOURCE  *p)
{
#if defined(MODS_HAS_PCI_MAP_RESOURCE)
	MODS_PRIVATE_DATA(private_data, fp);
	unsigned int devfn = PCI_DEVFN(p->pci_device.device,
				       p->pci_device.function);
	struct pci_dev *dev = MODS_PCI_GET_SLOT(p->pci_device.domain,
						p->pci_device.bus, devfn);
	struct list_head *head = private_data->mods_pci_res_map_list;
	struct list_head *iter;
	struct list_head *tmp;

	LOG_ENT();

	if (!dev) {
		mods_error_printk("PCI device %u:%u:%u.%u not found\n",
				  p->pci_device.domain,
				  p->pci_device.bus,
				  p->pci_device.device,
				  p->pci_device.function);
		LOG_EXT();
		return -EINVAL;
	}

	list_for_each_safe(iter, tmp, head) {
		struct MODS_PCI_RES_MAP_INFO *p_pci_res_map_info;

		p_pci_res_map_info =
		    list_entry(iter, struct MODS_PCI_RES_MAP_INFO, list);

		if ((p_pci_res_map_info->dev == dev) &&
		    (p_pci_res_map_info->va == p->va)) {
			int ret = mods_free_pci_res_map(fp, p_pci_res_map_info);
			LOG_EXT();
			return ret;
		}
	}

	mods_error_printk(
		"PCI mapping 0x%llx on device %u:%u:%u.%u not found\n",
		p->va,
		p->pci_device.domain,
		p->pci_device.bus,
		p->pci_device.device,
		p->pci_device.function);
	return -EINVAL;
#else
	return OK;
#endif
}
