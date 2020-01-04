/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
*/

/* #define DEBUG */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/highmem.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <mach/io.h>
#include <asm/io.h>

#include "tegrawatch_mod.h"

MODULE_LICENSE("GPL");

#ifdef TEGRAWATCH_MOD_VER
/* custom module version */
MODULE_INFO(vermagic, TEGRAWATCH_MOD_VER);
#endif

struct iomem_data_t {
    struct cdev *cdev;
    struct class *class;
    struct device *dev;
    int minor;
    int major;
    struct semaphore sem;
};

static struct iomem_data_t iomem;
long ioctl_iomem(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = -EINVAL;
    void __iomem *vaddr;

    /* Check type and command number */
    if (_IOC_TYPE(cmd) != TEGRA_IOC_MAGIC)
        return -ENOTTY;

    /* Check access direction */
    if (_IOC_DIR(cmd) & _IOC_READ)
        ret = !access_ok(VERIFY_WRITE,
                (void __user *)arg, _IOC_SIZE(cmd));
    if (ret == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
        ret = !access_ok(VERIFY_READ,
                (void __user *)arg, _IOC_SIZE(cmd));
    if (ret)
        return -EFAULT;

    if (down_interruptible(&iomem.sem))
        return -ERESTARTSYS;

    switch(cmd)
    {
        case IOCTL_TEGRA_IOREAD:
        {
            struct iord_buffer *iobuff = (struct iord_buffer *)arg;
            dev_dbg(iomem.dev, "ioread: 0x%08lx, %dbytes\n", iobuff->startmem, iobuff->nbytes);
            vaddr = ioremap(iobuff->startmem, iobuff->nbytes);
            if(vaddr)
            {
                ret = copy_to_user(iobuff->buffer, (const void *)vaddr, iobuff->nbytes);
                if(ret)
                {
                    ret  = -EFAULT;
                    break;
                }
                ret = iobuff->nbytes;
                iounmap(vaddr);
            }
            else
            {
                printk("Address (0x%08lx) xlate returned NULL (not mapped ?)\n", iobuff->startmem);
            }
            break;
        }
        case IOCTL_TEGRA_IOWRITE:
        {
            struct iowr_buffer *iobuff = (struct iowr_buffer *)arg;
            dev_dbg(iomem.dev, "iowrite: 0x%08lx, %dbytes\n", iobuff->startmem, iobuff->nbytes);

            vaddr = ioremap(iobuff->startmem, iobuff->nbytes);
            if(vaddr)
            {
                ret = copy_from_user((void *)vaddr, iobuff->buffer, iobuff->nbytes);
                if(ret)
                {
                    ret  = -EFAULT;
                    break;
                }
                ret = iobuff->nbytes;
                iounmap(vaddr);
            }
            else
            {
                printk("Address (0x%08lx) xlate returned NULL (not mapped ?)\n", iobuff->startmem);
            }
            break;
        }
        default:
            printk("Unknown tegra iomem ioctl code\n");
            ret = -EINVAL;
            break;
    }
    up(&iomem.sem);
    return ret;
}


static const struct file_operations iomem_fops = {
    .unlocked_ioctl      = ioctl_iomem,
};

static int __init iomem_init(void)
{
    dev_t devno;
    int err;

    iomem.minor = 0;
    err = alloc_chrdev_region(&devno, iomem.minor, 1, "iomem");
    if(err)
    {
        printk(KERN_ERR "%s: error allocating chrdev \n", __func__);
        return -ENOMEM;
    }

    iomem.major = MAJOR(devno);

    iomem.cdev = cdev_alloc();
    if (iomem.cdev == NULL) {
        printk(KERN_ERR "%s: error allocating cdev \n", __func__);
        err = -ENOMEM;
        goto cleanup;
    }

    sema_init(&iomem.sem, 1);
    cdev_init(iomem.cdev, &iomem_fops);
    iomem.cdev->owner = THIS_MODULE;
    err = cdev_add(iomem.cdev, MKDEV(iomem.major, iomem.minor), 1);
    if(err < 0)
    {
        printk(KERN_ERR "%s: cdev_add failed\n", __func__);
        kfree(iomem.cdev);
        goto cleanup;
    }

    iomem.class = class_create(THIS_MODULE, "iomem");
    if (IS_ERR(iomem.class))
        return PTR_ERR(iomem.class);

    iomem.dev = device_create(iomem.class, NULL, MKDEV(iomem.major, iomem.minor), NULL, "iomem");

    return 0;

cleanup:
    return err;
}

static void __exit iomem_exit(void)
{
    if(iomem.cdev)
    {
        device_del(iomem.dev);
        class_destroy(iomem.class);
        unregister_chrdev_region(MKDEV(iomem.major, iomem.minor), 1);
        cdev_del(iomem.cdev);
        kfree(iomem.cdev);
    }
}

module_init(iomem_init);
module_exit(iomem_exit);
