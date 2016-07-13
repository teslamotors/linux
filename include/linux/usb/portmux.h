/**
 * intel_mux.h - USB Port Mux definitions
 *
 * Copyright (C) 2016 Intel Corporation
 *
 * Author: Lu Baolu <baolu.lu@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_USB_PORTMUX_H
#define __LINUX_USB_PORTMUX_H

/**
 * struct portmux_ops - ops two switch the port
 *
 * @cable_set_cb: function to switch port to host
 * @cable_unset_cb: function to switch port to device
 */
struct portmux_ops {
	int (*cable_set_cb)(struct device *dev);
	int (*cable_unset_cb)(struct device *dev);
};

/**
 * struct portmux_desc - port mux device descriptor
 *
 * @name: the name of the mux device
 * @extcon_name: the name of extcon device, set to NULL if the mux
 *               is not connected to any extcon cable  and control
 *               purely by user through sysfs.
 * @dev: the parent of the mux device
 * @ops: ops to switch the port role
 * @initial_state: the initial state of the mux, set to -1 if the
 *                 initial state is unknown, set to 0 for device
 *                 and 1 for host.
 */
struct portmux_desc {
	const char *name;
	const char *extcon_name;
	struct device *dev;
	const struct portmux_ops *ops;
	int initial_state;
};

/**
 * struct portmux_dev - A mux device
 *
 * @desc: the descriptor of the mux
 * @dev: device of this mux
 * @edev: the extcon device bound to this mux
 * @nb: notifier of extcon state change
 * @mux_mutex: lock to serialize port switch operation
 * @mux_state: state of the mux, could be set to below values
 *             -1: before initialization
 *              0: port switched to device
 *              1: port switched to host
 */
struct portmux_dev {
	const struct portmux_desc *desc;
	struct device dev;
	struct extcon_dev *edev;
	struct notifier_block nb;

	 /* lock for mux_state */
	struct mutex mux_mutex;
	int mux_state;
};

struct portmux_dev *portmux_register(struct portmux_desc *desc);
void portmux_unregister(struct portmux_dev *pdev);
#ifdef CONFIG_PM_SLEEP
void portmux_complete(struct portmux_dev *pdev);
#endif

#endif /* __LINUX_USB_PORTMUX_H */
