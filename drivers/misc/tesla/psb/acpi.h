/* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0 */

#ifndef __PSB_ACPI_H__
#define __PSB_ACPI_H__

struct acpi_table_psb {
	struct acpi_table_header header;	/* Common ACPI table header */
	u32 config;
	u16 bios_key_rev_id;
	u8 root_key_hash[32];
} __packed;

#endif
