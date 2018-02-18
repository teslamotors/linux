/*
 * drivers/net/wireless/bcmdhd/dhd_custom_sysfs_tegra_rf_test.c
 *
 * NVIDIA Tegra Sysfs for BCMDHD driver
 *
 * Copyright (C) 2015 NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "dhd_custom_sysfs_tegra.h"
#include "bcmutils.h"
#include "wlioctl.h"
#include "wldev_common.h"

atomic_t rf_test = ATOMIC_INIT(0);
atomic_t cur_power_mode = ATOMIC_INIT(0);
extern int tegra_sysfs_wifi_on;

//NUM_RF_TEST_PARAMS needs to be updated when modifying this array
char *rf_test_vars[] = {"roam_off", "txchain", "rxchain"};

rf_test_params_t rf_test_params[NUM_RF_TEST_PARAMS];

void rf_test_params_init() {
	int i;
	for (i = 0; i < NUM_RF_TEST_PARAMS; i++) {
		rf_test_params[i].var = rf_test_vars[i];
		atomic_set(&rf_test_params[i].cur_val, 0);
	}
}

void
tegra_sysfs_rf_test_enable()
{
	extern struct net_device *dhd_custom_sysfs_tegra_histogram_stat_netdev;
	struct net_device *net = dhd_custom_sysfs_tegra_histogram_stat_netdev;
	int power_mode_off = 0;
	int i;

	pr_info("%s\n", __func__);

	/* Get current roam_off, txchain, rxchain and power mode state */
	for (i = 0; i < NUM_RF_TEST_PARAMS; i++) {
		if (wldev_iovar_getint(net, rf_test_params[i].var,
		    (s32 *)&rf_test_params[i].cur_val) != BCME_OK) {
			pr_err("%s: Failed to get current %s val\n", __func__, rf_test_params[i].var);
			return;
		}
	}
	if(wldev_ioctl(net, WLC_GET_PM, &cur_power_mode, sizeof(cur_power_mode), false)) {
		pr_err("%s: Failed to get current power mode state\n", __func__);
	}

	/* set roam_off/power mode and block setting it */
	if (wldev_iovar_setint(net, "roam_off", 1) != BCME_OK) {
		pr_err("%s: Failed to set roam off state\n", __func__);
		return;
	}
	if(wldev_ioctl(net, WLC_SET_PM, &power_mode_off, sizeof(power_mode_off), true)) {
		pr_err("%s: Failed to set power mode off state\n", __func__);
	}
	atomic_set(&rf_test, 1);
}


void
tegra_sysfs_rf_test_disable()
{
	extern struct net_device *dhd_custom_sysfs_tegra_histogram_stat_netdev;
	struct net_device *net = dhd_custom_sysfs_tegra_histogram_stat_netdev;
	int i;

	pr_info("%s\n", __func__);

	if (atomic_read(&rf_test)) {
		atomic_set(&rf_test, 0);
		/* Restore saved roam_off and power mode state */
		for (i = 0; i < NUM_RF_TEST_PARAMS; i++) {
			if (wldev_iovar_setint(net, rf_test_params[i].var,
			atomic_read(&rf_test_params[i].cur_val)) != BCME_OK) {
				pr_err("%s: Failed to restore %s val\n", __func__, rf_test_params[i].var);
			}
		}
		if (wldev_ioctl(net, WLC_SET_PM,
			(void *)&atomic_read(&cur_power_mode),
			sizeof(cur_power_mode), true)) {
			pr_err("%s: Failed to restore power mode state\n", __func__);
		}
	}
}

ssize_t
tegra_sysfs_rf_test_state_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	if (atomic_read(&rf_test)) {
		strcpy(buf, "enabled\n");
		return strlen(buf);
	} else {
		strcpy(buf, "disabled\n");
		return strlen(buf);
	}
}

ssize_t
tegra_sysfs_rf_test_state_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	if (strncmp(buf, "enable", 6) == 0) {
		if (!atomic_read(&rf_test) && tegra_sysfs_wifi_on) {
			tegra_sysfs_rf_test_enable();
		} else {
			pr_info("%s: operation not allowed\n", __func__);
		}
	} else if (strncmp(buf, "disable", 7) == 0) {
		if (atomic_read(&rf_test) && tegra_sysfs_wifi_on) {
			tegra_sysfs_rf_test_disable();
		} else {
			pr_info("%s: operation not allowed\n", __func__);
		}
	} else {
		pr_err("%s: unknown command\n", __func__);
	}

	return count;
}
