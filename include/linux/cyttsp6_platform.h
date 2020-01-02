/*
 * cyttsp6_platform.h
 * Cypress TrueTouch(TM) Standard Product V4 Platform Module.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2013-2015 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP6_PLATFORM_H
#define _LINUX_CYTTSP6_PLATFORM_H

#include <linux/cyttsp6_core.h>

#if defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6) \
	|| defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6_MODULE)
extern struct cyttsp6_loader_platform_data _cyttsp6_loader_platform_data;

int cyttsp6_xres(struct cyttsp6_core_platform_data *pdata, struct device *dev);
int cyttsp6_init(struct cyttsp6_core_platform_data *pdata,
		int on, struct device *dev);
int cyttsp6_power(struct cyttsp6_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq);
#ifdef CYTTSP6_DETECT_HW
int cyttsp6_detect(struct cyttsp6_core_platform_data *pdata,
		struct device *dev, cyttsp6_platform_read read);
#endif
int cyttsp6_irq_stat(struct cyttsp6_core_platform_data *pdata,
		struct device *dev);
int cyttsp6_error_stat(struct cyttsp6_core_platform_data *pdata,
		struct device *dev);
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP6 */

#endif /* _LINUX_CYTTSP6_PLATFORM_H */
