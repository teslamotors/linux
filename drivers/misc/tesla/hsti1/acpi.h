/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

#ifndef __HSTI1_H__
#define __HSTI1_H__

struct acpi_table_hsti1 {
	struct acpi_table_header header;	/* Common ACPI table header */
	u32 state;
} __packed;

#endif
