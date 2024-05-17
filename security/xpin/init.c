// SPDX-License-Identifier: GPL-2.0-only
/*
 * init.c: Handles boot-time initialization for XPin.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "-init: " fmt

#include "xpin_internal.h"
#include <linux/kconfig.h>
#include <linux/kernel.h>
#include <linux/printk.h>


bool xpin_enforce = IS_ENABLED(CONFIG_SECURITY_XPIN_ENFORCE);
unsigned int xpin_logging = IS_ENABLED(CONFIG_SECURITY_XPIN_LOGGING)
	? XPIN_LOGGING_NORMAL
	: XPIN_LOGGING_NONE;
bool xpin_lockdown = IS_ENABLED(CONFIG_SECURITY_XPIN_LOCKDOWN);


static int __init xpin_enforce_setup(char *str)
{
	unsigned long enforce;
	int err = kstrtoul(str, 0, &enforce);

	if (!err)
		xpin_enforce = enforce ? 1 : 0;
	return 1;
}
__setup("xpin.enforce=", xpin_enforce_setup);


static int __init xpin_logging_setup(char *str)
{
	unsigned long logging;
	int err = kstrtoul(str, 0, &logging);

	if (!err)
		xpin_logging = (unsigned int)logging;
	return 1;
}
__setup("xpin.logging=", xpin_logging_setup);


static int __init xpin_lockdown_setup(char *str)
{
	unsigned long lockdown;
	int err = kstrtoul(str, 0, &lockdown);

	if (!err)
		xpin_lockdown = lockdown ? 1 : 0;
	return 1;
}
__setup("xpin.lockdown=", xpin_lockdown_setup);


int __init xpin_init(void)
{
	pr_info("Initializing XPin\n");
	xpin_add_hooks();
	pr_info("Added LSM hooks\n");
	return 0;
}

fs_initcall(xpin_init_securityfs);
