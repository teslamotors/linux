/*
 * cyttsp6_acpi.h
 * Support for loading platform data from ACPI for the cyttsp6 driver.
 * For use with Cypress touchscreen controllers.
 * Supported parts include:
 * CY8CTMA46X
 * CY8CTMA1036/768
 * CYTMA445
 *
 * Copyright (C) 2019 Codethink
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
 */

#ifndef _LINUX_CYTTSP6_ACPI_H
#define _LINUX_CYTTSP6_ACPI_H

#ifdef CONFIG_ACPI

/**
 * cyttsp6_acpi_get_pdata - get platform data from ACPI
 * @dev: The device to search in for ACPI properties
 *
 * Platform data is saved to dev->platform_data.
 *
 * Return: 0 on success,
 *         %-ENOMEM if memory allocation fails.
 *	   %-EINVAL if given arguments are not valid,
 *	   %-ENODATA if a property does not have a value,
 *	   %-EPROTO or %-EILSEQ if a property has the wrong type,
 *	   %-EOVERFLOW if the size of a property is not as expected,
 *	   %-ENXIO if no suitable firmware interface is present,
 *         %-ENODEV if a child device is missing.
 */
int cyttsp6_acpi_get_pdata(struct device *dev);

#else /* CONFIG_ACPI */
static inline int cyttsp6_acpi_get_pdata(struct device *dev)
{
	return -ENODEV;
}
#endif /* CONFIG_ACPI */

#endif /* _LINUX_CYTTSP6_ACPI_H */
