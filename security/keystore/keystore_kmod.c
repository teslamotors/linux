/*
 *
 * Intel Keystore Linux driver
 * Copyright (c) 2013, Intel Corporation.
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
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/jiffies.h>
#ifdef CONFIG_DAL_KEYSTORE
#include <linux/dal.h>
#include <linux/workqueue.h>
#endif

#include <security/keystore_api_kernel.h>
#include <security/sb.h>

#include <security/keystore_api_user.h>

#include "keystore_context_safe.h"
#include "keystore_tests.h"
#include "keystore_seed.h"
#include "keystore_ioctl.h"
#include "keystore_debug.h"

/**
 * The keystore module parameters.
 */
static unsigned int version_major = KEYSTORE_VERSION_MAJOR;
static unsigned int version_minor = KEYSTORE_VERSION_MINOR;
static unsigned int version_patch = KEYSTORE_VERSION_PATCH;

#if defined(CONFIG_KEYSTORE_TESTMODE)
#pragma message "Keystore self-tests are enabled."
static bool testing = 1;  /* Testing on host is enabled */
#else
static bool testing;      /* Testing on host is disabled */
#endif

#if defined(CONFIG_KEYSTORE_HARD_CODED_SEED)
#warning Compiling keystore with a hardcoded seed!
#warning This mode should only be used for testing!
static bool hardcode_seed = 1;
#else
static bool hardcode_seed;
#endif

#if defined(CONFIG_KEYSTORE_TEST_MIGRATION)
#pragma message "Migrations functions available"
static bool test_migration = 1;
#else
static bool test_migration;
#endif

#if defined(CONFIG_KEYSTORE_DISABLE_DEVICE_SEED)
static bool device_seed_active;
#else
static bool device_seed_active = 1;
#endif

#if defined(CONFIG_KEYSTORE_DISABLE_USER_SEED)
static bool user_seed_active;
#else
static bool user_seed_active = 1;
#endif

#ifdef CONFIG_DAL_KEYSTORE
/**
 * DAL keystore client parameters
 */
#define APPLET_BLACKLIST_COUNT (1)
#define APPLET_BLACKLIST_MAX_RETRY (5)


struct applet_blacklisting_work {
	struct delayed_work dwork;
	int  retry;
	int  applet_uuid_count;
	char *applet_uuid[APPLET_BLACKLIST_COUNT];
};

typedef struct applet_blacklisting_work applet_blacklisting_work_t;

applet_blacklisting_work_t *work_;

#endif

/**
 * keystore_open - the open function
 *
 * @param inode pointer to inode structure
 * @param file pointer to file structure
 *
 * @returns 0 on success, <0 on error
 */
static int keystore_open(struct inode *inode, struct file *file)
{
	ks_debug(KBUILD_MODNAME ": keystore_open\n");
	return nonseekable_open(inode, file);
}

/**
 * keystore_release - the close function
 *
 * @param inode pointer to inode structure
 * @param file pointer to file structure
 *
 * @returns 0 on success, <0 on error
 */
static int keystore_release(struct inode *inode, struct file *file)
{
	ks_debug(KBUILD_MODNAME ": keystore_release\n");
	return 0;
}

/* The various file operations we support. */
static const struct file_operations keystore_fops = {
open:		 keystore_open,
release :	 keystore_release,
unlocked_ioctl : keystore_ioctl
};

static dev_t dev_keystore; /* Structure to set device range */
static struct class *class_keystore; /* Struct to set device class properties */
static struct cdev cdev_keystore; /* Structure to create device */

#ifdef CONFIG_DAL_KEYSTORE
int add_to_blacklist(const char *uuid_str)
{
	uuid_t uuid;
	int ret = 0;

	ret = dal_uuid_parse(uuid_str, &uuid);
	if (ret != DAL_KDI_SUCCESS) {
		pr_err(KBUILD_MODNAME ": %s dal_uuid_parse failed\n",
									__func__);
	} else {
		pr_err(KBUILD_MODNAME ": %s dal_uuid_parse succeeded\n",
									__func__);
	}

	ret = dal_set_ta_exclusive_access(&uuid);
	if (ret != DAL_KDI_SUCCESS) {
		pr_err(KBUILD_MODNAME ": %s dal_set_ta_exclusive_access failed\n",
									__func__);
	} else {
		pr_err(KBUILD_MODNAME ": %s dal_set_ta_exclusive_access succeeded\n",
									__func__);
	}
	return ret;
}

static void applet_blacklisting_worker(struct work_struct *work)
{
	struct applet_blacklisting_work *my_work = container_of(to_delayed_work(work),
						struct applet_blacklisting_work, dwork);

	int ret = DAL_KDI_SUCCESS;
	int applet = 0;

	if (my_work->retry ==  APPLET_BLACKLIST_MAX_RETRY)
		goto exit;

	for ( ; applet < my_work->applet_uuid_count; applet++) {
		ret = add_to_blacklist(my_work->applet_uuid[applet]);

		if (ret != DAL_KDI_SUCCESS) {
			my_work->retry++;
			schedule_delayed_work((struct delayed_work *)my_work, msecs_to_jiffies(200));
			ks_info(KBUILD_MODNAME ": Re-scheduling DAL Keystore blacklisting work\n");

			goto end;
		}
	}

exit:
	kfree( (void *)my_work );
end:
	return;
}

static void schedule_applet_blacklisting(void)
{
	work_ = (applet_blacklisting_work_t *)kmalloc(sizeof(applet_blacklisting_work_t), GFP_KERNEL);
	if (work_) {
		ks_info(KBUILD_MODNAME ": Scheduling DAL Keystore blacklisting work\n");

		work_->applet_uuid_count = APPLET_BLACKLIST_COUNT;
		work_->retry = 0;
		work_->applet_uuid[0] = CONFIG_DAL_KEYSTORE_APPLET_ID;

		INIT_DELAYED_WORK( (struct delayed_work *)work_, applet_blacklisting_worker );
		schedule_delayed_work((struct delayed_work *)work_, msecs_to_jiffies(17000));
	}
}
#endif

static int __init keystore_init(void)
{
	int res;

	ks_info(KBUILD_MODNAME ": keystore_init\n");

#ifdef CONFIG_KEYSTORE_SECURE_BOOT_IGNORE
	ks_info(KBUILD_MODNAME ": Not checking secure boot status!\n");
#else
	/*
	 * Check if secure boot status. Keystore should not start on
	 * non-secure boot enabled systems.
	 */
	if (!get_sb_stat()) {
		ks_err(KBUILD_MODNAME ": Exiting keystore - secure boot not enabled\n");
		return -1;
	}
#endif /* CONFIG_KEYSTORE_SECURE_BOOT_IGNORE */

	/* get seed from cmdline */
	res = keystore_fill_seeds();
	if (res) {
		ks_err(KBUILD_MODNAME ": keystore_fill_seeds error %d\n",
				res);
		return res;
	}

	/* create dynamic node device */
	res = alloc_chrdev_region(&dev_keystore, 0, 1, "keystore");
	if (res != 0) {
		ks_err(KBUILD_MODNAME ": Unable to set range of devices: %d\n",
		       res);
		res =  -ENODEV;
		goto err1;
	}

	class_keystore = class_create(THIS_MODULE, "keystore");
	if (class_keystore == NULL) {
		ks_err(KBUILD_MODNAME ": Unable to create device class\n");
		res =  -ENODEV;
		goto err2;
	}

	if (device_create(class_keystore, NULL, dev_keystore,
				NULL, "keystore") == NULL) {
		res =  -ENODEV;
		goto err3;
	}

	cdev_init(&cdev_keystore, &keystore_fops);
	res = cdev_add(&cdev_keystore, dev_keystore, 1);
	if (res != 0) {
		ks_err(KBUILD_MODNAME ": Unable create device: %d\n", res);
		res =  -ENODEV;
		goto err4;
	}

	ks_info(KBUILD_MODNAME ": keystore_init completed successfully\n");
#ifdef CONFIG_DAL_KEYSTORE
	schedule_applet_blacklisting();
#endif
	return 0;

	/* error */
err4:
	cdev_del(&cdev_keystore);
	device_destroy(class_keystore, dev_keystore);
err3:
	class_destroy(class_keystore);
err2:
	unregister_chrdev_region(dev_keystore, 1);
err1:
	return res;
}

static void __exit keystore_exit(void)
{
	ks_info(KBUILD_MODNAME ": keystore_exit\n");
	/*unset device hook */
	cdev_del(&cdev_keystore);
	/*destroy device */
	device_destroy(class_keystore, dev_keystore);
	/*unset device class */
	class_destroy(class_keystore);
	/*unregister device range */
	unregister_chrdev_region(dev_keystore, 1);

	ctx_free();
}

module_init(keystore_init);
module_exit(keystore_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) Keystore");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_SUPPORTED_DEVICE(KBUILD_MODNAME);

module_param(version_major, uint, S_IRUSR);
MODULE_PARM_DESC(version_major, "Keystore major version number");
module_param(version_minor, uint, S_IRUSR);
MODULE_PARM_DESC(version_minor, "Keystore minor version number");
module_param(version_patch, uint, S_IRUSR);
MODULE_PARM_DESC(version_patch, "Keystore patch version number");

module_param(testing, bool, S_IRUSR);
MODULE_PARM_DESC(testing, "Run diagnostics on start");

module_param(hardcode_seed, bool, S_IRUSR);
MODULE_PARM_DESC(hardcode_seed, "Use hardcoded SEED and key values");

module_param(test_migration, bool, S_IRUSR);
MODULE_PARM_DESC(test_migration, "Migration test functions available");

module_param(device_seed_active, bool, S_IRUSR);
MODULE_PARM_DESC(device_seed_active, "Device SEED active");
module_param(user_seed_active, bool, S_IRUSR);
MODULE_PARM_DESC(user_seed_active, "User SEED active");


/* end of file */
