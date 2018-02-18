/*
################################################################################
#
# r8168 is the Linux device driver released for Realtek Gigabit Ethernet
# controllers with PCI-Express interface.
#
# Copyright(c) 2014 Realtek Semiconductor Corp. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#
# Author:
# Realtek NIC software team <nicfae@realtek.com>
# No. 2, Innovation Road II, Hsinchu Science Park, Hsinchu 300, Taiwan
#
################################################################################
*/

/************************************************************************************
 *  This product is covered by one or more of the following patents:
 *  US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/fs.h>		// struct file_operations
#include <linux/mm.h>		// mmap
#include <linux/slab.h>		// kmalloc
#include <linux/pci-aspm.h>
#include <asm/io.h>
#include <asm/uaccess.h>		// copy_to_user
#include <asm/byteorder.h>

#include <linux/if_vlan.h>
#include "r8168.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
#warning KERNEL_VERSION < 2.6.11
#endif

static DEV_INFO		dev_info[MAX_DEV_NUM];
static atomic_t		dev_num;
static spinlock_t	module_lock;
static int		major = 0;

module_param(major, int, S_IRUGO|S_IWUSR);

//static int dev_IoctlFun(struct inode *inode, struct file *pFile,unsigned int cmd, unsigned long arg)
static long dev_IoctlFun(struct file *pFile,unsigned int cmd, unsigned long arg)
{
	PPGDEV	mydev;
	int	result;

	if (_IOC_TYPE(cmd) != RTL_IOC_MAGIC)
	{
		DbgFunPrint("Invalid command!!!");
		return -ENOTTY;
	}

	mydev = (PPGDEV)pFile->private_data;
	result = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	if (down_interruptible(&mydev->dev_sem))
	{
		DbgFunPrint("lock fail");
		return -ERESTARTSYS;
	}
#else
	if (mutex_lock_interruptible(&mydev->dev_mutex))
	{
		DbgFunPrint("lock fail");
		return -ERESTARTSYS;
	}
#endif

	switch(cmd)
	{
		case IOC_PCI_CONFIG:
		{
			PCI_CONFIG_RW	pci_config;

			if (copy_from_user(&pci_config, (void __user *)arg, sizeof(PCI_CONFIG_RW)))
			{
				DbgFunPrint("copy_from_user fail");
				result = -EFAULT;
				break;
			}


			if (pci_config.bRead==TRUE)
			{
				if (pci_config.size==1)
				{
					result = pci_read_config_byte(mydev->pdev, pci_config.addr, &pci_config.byte);
				}
				else if (pci_config.size==2)
				{
					result = pci_read_config_word(mydev->pdev, pci_config.addr, &pci_config.word);
				}
				else if (pci_config.size==4)
				{
					result = pci_read_config_dword(mydev->pdev, pci_config.addr, &pci_config.dword);
				}
				else
				{
					result = -EINVAL;
				}

				if (result)
				{
					DbgFunPrint("Read PCI fail:%d",result);
					break;
				}

				if (copy_to_user((void __user *)arg , &pci_config, sizeof(PCI_CONFIG_RW)))
				{
					DbgFunPrint("copy_from_user fail");
					result = -EFAULT;
					break;
				}
			}
			else // write
			{
				if (pci_config.size==1)
				{
					result = pci_write_config_byte(mydev->pdev, pci_config.addr, pci_config.byte);
				}
				else if (pci_config.size==2)
				{
					result = pci_write_config_word(mydev->pdev, pci_config.addr, pci_config.word);
				}
				else if (pci_config.size==4)
				{
					result = pci_write_config_dword(mydev->pdev, pci_config.addr, pci_config.dword);
				}
				else
				{
					result = -EINVAL;
				}

				if (result)
				{
					DbgFunPrint("Write PCI fail:%d",result);
					break;
				}
			}
#if 0
			DebugPrint("bRead=%u, size=%d, addr=0x%02x , data=0x%08x",
				pci_config.bRead, pci_config.size,
				pci_config.addr, pci_config.dword);
#endif
			break;
		}

		case IOC_IOMEM_OFFSET:
			if ( put_user(mydev->offset,(unsigned int __user *)arg) )
			{
				DbgFunPrint("put_user fail");
				result = -EFAULT;
				break;
			}
			break;

		case IOC_DEV_FUN:
			if ( put_user(mydev->pdev->devfn,(unsigned int __user *)arg) )
			{
				DbgFunPrint("put_user fail");
				result = -EFAULT;
				break;
			}
			break;

		default:
			DbgFunPrint("Command not support!!!");
			result = -ENOTTY;
			break;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	up(&mydev->dev_sem);
#else
	mutex_unlock(&mydev->dev_mutex);
#endif
	return result;
}

ssize_t dev_read(struct file *filp, char __user *buffer, size_t count, loff_t *f_pos)
{
	PPGDEV	mydev;
	BYTE	*buf;
	ssize_t	readcount;

	DbgFunPrint("count=%zd",count);
	mydev = (PPGDEV)filp->private_data;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	if (down_interruptible(&mydev->dev_sem))
	{
		DbgFunPrint("lock fail");
		return 0;
	}
#else
	if (mutex_lock_interruptible(&mydev->dev_mutex))
	{
		DbgFunPrint("lock fail");
		return 0;
	}
#endif

	// To Do:
	if (count > 0x100)
	{
		count = 0x100;
//		return 0;
	}
	else
	{
		count &= ~0x3;
	}
	buf = kmalloc(count,GFP_KERNEL);
	if (!buf)
	{
		return -ENOMEM;
	}

	for (readcount=0;readcount<count;readcount+=4)
	{
		if (pci_read_config_dword(mydev->pdev,readcount,(u32 *)&buf[readcount]))
		{
			break;
		}
	}

	if (copy_to_user(buffer,buf,readcount))
	{
		readcount = 0;
	}
	*f_pos += readcount;

	kfree(buf);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	up(&mydev->dev_sem);
#else
	mutex_unlock(&mydev->dev_mutex);
#endif
	return readcount;
}

static int dev_Open(struct inode *inode, struct file *pFile)
{
	struct pci_dev *pdev;
	PPGDEV	mydev;

	mydev = container_of(inode->i_cdev, PGDEV, cdev);
	DbgFunPrint("mydev=%p",mydev);

	pdev = mydev->pdev;
	rtl8168_remove_one(pdev);
	pci_set_drvdata(pdev, mydev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	atomic_inc(&mydev->count);
	if (atomic_read(&mydev->count) > 1)
	{
		atomic_dec(&mydev->count);
		DbgFunPrint("Busy");
		return -EMFILE;
	}
#else
	if (atomic_inc_return(&mydev->count) > 1)
	{
		atomic_dec(&mydev->count);
		DbgFunPrint("Busy");
		return -EMFILE;
	}
#endif

	pFile->private_data = mydev;

	return 0;
}

static int dev_Close(struct inode *inode, struct file *pFile)
{
	struct pci_dev *pdev;
	PPGDEV	mydev;
	int ret;

	mydev = container_of(inode->i_cdev, PGDEV, cdev);
	DbgFunPrint("mydev=%p",mydev);
	pFile->private_data = NULL;

	atomic_dec(&mydev->count);

	pdev = mydev->pdev;
	pci_set_drvdata(pdev, NULL);

	ret = rtl8168_init_one(pdev, mydev->id);
	if (!ret) {
	        struct rtl8168_private *tp;
	        struct net_device *dev;

		dev = pci_get_drvdata(pdev);
		tp = netdev_priv(dev);
		tp->pgdev = mydev;
	}

	return ret;
}

static void dev_vma_open(struct vm_area_struct *vma)
{
	DebugPrint("dev_vma_open");
//	DbgFunPrint("vma=0x%08x, vm_start=0x%08x, vm_end=0x%08x, vm_pgoff=%d\n",
//			(DWORD)vma, (DWORD)vma->vm_start, (DWORD)vma->vm_end,
//			(DWORD)vma->vm_pgoff);
}

static void dev_vma_close(struct vm_area_struct *vma)
{
	DebugPrint("dev_vma_close");
//	DbgFunPrint("vma=0x%08x, vm_start=0x%08x, vm_end=0x%08x, vm_pgoff=%d\n",
//			(DWORD)vma, (DWORD)vma->vm_start, (DWORD)vma->vm_end,
//			(DWORD)vma->vm_pgoff);
}

static struct vm_operations_struct dev_vma_ops = {
	.open =		dev_vma_open,
	.close = 	dev_vma_close,
};

static int dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	PPGDEV		mydev;

	mydev = (PPGDEV)filp->private_data;

	if (!mydev)
	{
		DbgFunPrint("NULL pointer");
		return -EFAULT;
	}
	DbgFunPrint("\nvma=%p, vm_start=0x%08x, vm_end=0x%08x\nvm_pgoff=%d, vm_flags=0x%x, mydev=%p\n",
			vma, (DWORD)vma->vm_start, (DWORD)vma->vm_end,
			(DWORD)vma->vm_pgoff,(DWORD)vma->vm_flags,
			mydev);

	if (!mydev || !mydev->base_phyaddr)
	{
		DbgFunPrint("Invalid base_phyaddr=0x%08x",(DWORD)mydev->base_phyaddr);
		return -ENXIO;
	}

	switch(mydev->deviceID)
	{
		case 0x8167:
		case 0x8169:
		{
			unsigned int iomem;
			pci_read_config_dword(mydev->pdev, 0x14, &iomem);
			if (!iomem)
			{
				DbgFunPrint("Invalid iomem=0x%08x, base_phyaddr=0x%08x",iomem,(DWORD)mydev->base_phyaddr);
				pci_write_config_dword(mydev->pdev, 0x14, (DWORD)mydev->base_phyaddr);
			}
			break;
		}

		default:
			break;
	}

	vma->vm_flags |= VM_IO;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	if (remap_page_range(vma, vma->vm_start, mydev->base_phyaddr, vma->vm_end-vma->vm_start, vma->vm_page_prot))
	{
		DbgFunPrint("remap_page_range fail!!!");
		return -EAGAIN;
	}
#else
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, mydev->base_phyaddr>>PAGE_SHIFT, vma->vm_end-vma->vm_start, vma->vm_page_prot))
	{
		DbgFunPrint("remap_pfn_range fail!!!");
		return -EAGAIN;
	}
#endif
	vma->vm_ops = &dev_vma_ops;
	dev_vma_open(vma);
	return 0;
}

static struct file_operations pg_fops = {
	.owner			= THIS_MODULE,
	.read			= dev_read,
//	.write			= NsmWriteFun,
	.mmap			= dev_mmap,
	.compat_ioctl		= dev_IoctlFun,
	.open			= dev_Open,
	.release		= dev_Close,
};

int __devinit pgdrv_prob(struct pci_dev *pdev, const struct pci_device_id *id)
{
	PPGDEV		mydev;
	dev_t		devno;
	int		pg_minor, result, i;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
        pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 |
                               PCIE_LINK_STATE_CLKPM);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
	atomic_inc(&dev_num);
	if (atomic_read(&dev_num) > MAX_DEV_NUM)
	{
		atomic_dec(&dev_num);
		DbgFunPrint("Too Many Device");
		return -EFAULT;
	}
#else
	if (atomic_inc_return(&dev_num) > MAX_DEV_NUM)
	{
		atomic_dec(&dev_num);
		DbgFunPrint("Too Many Device");
		return -EFAULT;
	}
#endif

	mydev = kmalloc(sizeof(PGDEV),GFP_KERNEL);
	if (!mydev)
	{
		DebugPrint("Allocate dev fail");
		return -ENOMEM;
	}
	memset(mydev, 0, sizeof(PGDEV));

	pg_minor = -1;
	devno = 0;
	spin_lock(&module_lock);
	for (i = 0; i < MAX_DEV_NUM; i++)
	{
		if (dev_info[i].bUsed == FALSE)
		{
			dev_info[i].bUsed = TRUE;
			devno = dev_info[i].devno;
			mydev->index = i;
			break;
		}
	}
	spin_unlock(&module_lock);

	mydev->pdev = pdev;
	mydev->id = id;
	atomic_set(&mydev->count, 0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
	init_MUTEX(&mydev->dev_sem);
#else
	mutex_init(&mydev->dev_mutex);
#endif

	pg_minor = MINOR(devno);
	DbgFunPrint("mydev=%p, major=%u, minor=%u",mydev,major,pg_minor);

	cdev_init(&mydev->cdev, &pg_fops);
	mydev->cdev.owner = THIS_MODULE;
	result = cdev_add(&mydev->cdev, devno, 1);
	if (result)
	{
		DebugPrint("cdev_add fault");
		spin_lock(&module_lock);
		dev_info[mydev->index].bUsed = FALSE;
		spin_unlock(&module_lock);
		kfree(mydev);
		return result;
	}

	pci_set_drvdata(pdev, mydev);

	result = pci_enable_device(mydev->pdev);
	if (result<0)
	{
		DbgFunPrint("pci_enable_device fail");
		return result;
	}

	result = pci_request_regions(mydev->pdev, MODULENAME);
	if (result<0)
	{
		DbgFunPrint("pci_request_regions fail");
		goto error_2;
	}

	pci_set_master(mydev->pdev);

	switch(id->device)
	{
		case 0x8167:
		case 0x8169:
			mydev->base_phyaddr = pci_resource_start(mydev->pdev, 1);
			break;
		default:
			mydev->base_phyaddr = pci_resource_start(mydev->pdev, 2);
			break;
	}
	if (!mydev->base_phyaddr)
	{
		DbgFunPrint("Invalid phyaddress");
		result = -EFAULT;
		goto error_1;
	}
	mydev->deviceID = id->device;
	mydev->offset = mydev->base_phyaddr & ((1 << PAGE_SHIFT) - 1);
	DbgFunPrint("ID=0x%04x base_phyaddr=0x%08x, offset=0x%08x",
		    mydev->deviceID , (DWORD)mydev->base_phyaddr,
		    mydev->offset);

	return 0;

error_1:
	pci_release_regions(mydev->pdev);
error_2:
	pci_disable_device(mydev->pdev);
	cdev_del(&mydev->cdev);
	kfree(mydev);
	return result;
}

void __devexit pgdrv_remove(struct pci_dev *pdev)
{
	PPGDEV	mydev;

	mydev = pci_get_drvdata(pdev);
	DbgFunPrint("mydev=%p",mydev);

	mydev->base_phyaddr = 0;
	pci_release_regions(pdev);
	pci_disable_device(pdev);

	cdev_del(&mydev->cdev);
	spin_lock(&module_lock);
	dev_info[mydev->index].bUsed = FALSE;
	spin_unlock(&module_lock);
	kfree(mydev);
	pci_set_drvdata(pdev, NULL);
	atomic_dec(&dev_num);
}

int __init pgdrv_init(void)
{
	int	result, i;
	dev_t	devno;

	memset(dev_info, 0, sizeof(dev_info));
	atomic_set(&dev_num,0);
	spin_lock_init(&module_lock);

	if (!major)
	{
		result = alloc_chrdev_region(&devno, 0, MAX_DEV_NUM, PGNAME);
		major = MAJOR(devno);
	}
	else
	{
		devno = MKDEV(major, 0);
		result = register_chrdev_region(devno, MAX_DEV_NUM, PGNAME);
	}

	if (result < 0)
	{
		DebugPrint("Can't get major %d", major);
		return result;
	}
	DbgFunPrint("Major : %d",major);

	for (i=0; i<MAX_DEV_NUM; i++)
	{
		dev_info[i].devno = MKDEV(major, i);
	}

	return 0;
}

void __exit pgdrv_exit(void)
{
	DbgFunPrint();
	unregister_chrdev_region(MKDEV(major, 0), MAX_DEV_NUM);
}
