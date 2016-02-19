/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CRLMODULE_MSRLIST_H__
#define __CRLMODULE_MSRLIST_H__

#define TBD_CLASS_DRV_ID 2

struct i2c_client;
struct firmware;

struct tbd_header {
	/* Tag identifier, also checks endianness */
	u32 tag;
	/* Container size including this header */
	u32 size;
	/* Version, format 0xYYMMDDVV */
	u32 version;
	/* Revision, format 0xYYMMDDVV */
	u32 revision;
	/* Configuration flag bits set */
	u32 config_bits;
	/* Global checksum, header included */
	u32 checksum;
} __packed;

struct tbd_record_header {
	/* Size of record including header */
	u32 size;
	/* tbd_format_t enumeration values used */
	u8 format_id;
	/* Packing method; 0 = no packing */
	u8 packing_key;
	/* tbd_class_t enumeration values used */
	u16 class_id;
} __packed;

struct tbd_data_record_header {
	u16 next_offset;
	u16 flags;
	u16 data_offset;
	u16 data_size;
} __packed;

int crlmodule_load_msrlist(struct i2c_client *client, char *name,
		const struct firmware **fw);
int crlmodule_apply_msrlist(struct i2c_client *client,
		const struct firmware *fw);
void crlmodule_release_msrlist(const struct firmware **fw);

#endif /* ifndef __CRLMODULE_MSRLIST_H__ */
