/*
 * Driver for Intel USB mux
 *
 * Copyright (C) 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _INTEL_USB_MUX_H
#define _INTEL_USB_MUX_H

struct intel_usb_mux;

#ifdef CONFIG_EXTCON_INTEL_USB
struct intel_usb_mux *intel_usb_mux_register(struct device *dev,
					     struct resource *r);
void intel_usb_mux_unregister(struct intel_usb_mux *mux);
#else
static inline struct intel_usb_mux *intel_usb_mux_register(struct device *dev,
							   struct resource *r)
{
	return NULL;
}
static inline void intel_usb_mux_unregister(struct intel_usb_mux *mux) { }
#endif /* CONFIG_EXTCON_INTEL_USB */

#endif /* _INTEL_USB_MUX_H */
