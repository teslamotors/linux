/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

#ifndef __OEM_ACPI_H__
#define __OEM_ACPI_H__

struct acpi_table_oem {
	struct acpi_table_header header;	/* Common ACPI table header */
	u8 state;
} __packed;

#endif
