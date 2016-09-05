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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "keystore_seed.h"
#include "keystore_debug.h"


static int __init seed_wipe_init(void)
{
	int res;

	ks_info("--------\n" KBUILD_MODNAME ": seed_wipe_init\n");

	/* fill_seeds here will actually just wipe both of the seeds */
	/* as the module will be compiled with the config options:   */
	/* CONFIG_KEYSTORE_DISABLE_DEVICE_SEED                       */
	/* CONFIG_KEYSTORE_DISABLE_USER_SEED                         */
	res = keystore_fill_seeds();
	if (res) {
		ks_err(KBUILD_MODNAME ": keystore_fill_seeds error %d\n",
				res);
		return res;
	}

	ks_info("--------\n");
	ks_info(KBUILD_MODNAME ": seed_wipe_init completed successfully\n");

	return 0;

}

static void __exit seed_wipe_exit(void)
{
}

module_init(seed_wipe_init);
module_exit(seed_wipe_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) Keystore SEED Wiper");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_SUPPORTED_DEVICE(KBUILD_MODNAME);

/* end of file */
