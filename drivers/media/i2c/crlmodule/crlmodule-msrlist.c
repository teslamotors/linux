// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2016 - 2018 Intel Corporation

#include <linux/i2c.h>
#include <linux/firmware.h>
#include "crlmodule-msrlist.h"
#include "crlmodule.h"

/*
 *
 * DRVB file is part of the old structure of tagged
 * binary container, which is used as such in crlmodule.
 * Changes needs to be done in cameralibs to remove the
 * tagged structure and convert to untagged drvb format.
 * Below are the tagged binary data container structure
 * definitions. Most of it is copied from libmsrlisthelper.c
 * and some changes done for crlmodule.
 *
 */

static int crlmodule_write_msrlist(struct i2c_client *client, u8 *bufptr,
					unsigned int size)
{
	/*
	 *
	 * The configuration data contains any number of sequences where
	 * the first byte (that is, u8) that marks the number of bytes
	 * in the sequence to follow, is indeed followed by the indicated
	 * number of bytes of actual data to be written to sensor.
	 * By convention, the first two bytes of actual data should be
	 * understood as an address in the sensor address space (hibyte
	 * followed by lobyte) where the remaining data in the sequence
	 * will be written.
	 *
	 */

	u8 *ptr = bufptr;
	int ret;

	while (ptr < bufptr + size) {
		struct i2c_msg msg = {
			.addr = client->addr,
			.flags = 0,
		};

		msg.len = *ptr++;
		msg.buf = ptr;
		ptr += msg.len;

		if (ptr > bufptr + size)
			return -EINVAL;

		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret < 0) {
			dev_err(&client->dev, "i2c write error: %d", ret);
			return ret;
		}
	}
	return 0;
}

static int crlmodule_parse_msrlist(struct i2c_client *client, u8 *buffer,
					unsigned int size)
{
	u8 *endptr8 = buffer + size;
	int ret;
	unsigned int dataset = 0;
	struct tbd_data_record_header *header =
		(struct tbd_data_record_header *)buffer;

	do {

		if ((u8 *)header + sizeof(*header) > endptr8)
			return -EINVAL;

		if ((u8 *)header + header->data_offset +
				header->data_size > endptr8)
			return -EINVAL;

		dataset++;

		if (header->data_size && (header->flags & 1)) {

			ret = crlmodule_write_msrlist(client,
					buffer + header->data_offset,
					header->data_size);
			if (ret)
				return ret;
		}
		header = (struct tbd_data_record_header *)(buffer +
			header->next_offset);
	} while (header->next_offset);

	return 0;
}


int crlmodule_apply_msrlist(struct i2c_client *client,
			const struct firmware *fw)
{
	struct tbd_header *header;
	struct tbd_record_header *record;

	header = (struct tbd_header *)fw->data;
	record = (struct tbd_record_header *)(header + 1);

	if (record->size && record->class_id != TBD_CLASS_DRV_ID)
		return -EINVAL;

	return crlmodule_parse_msrlist(client, (u8 *)(record + 1),
			record->size);
}


int crlmodule_load_msrlist(struct i2c_client *client, char *name,
		const struct firmware **fw)
{

	struct tbd_header *header;
	struct tbd_record_header *record;
	int ret = -ENOENT;

	ret = request_firmware(fw, name, &client->dev);
	if (ret) {
		dev_err(&client->dev,
			"Error %d while requesting firmware %s\n",
			ret, name);
		return ret;
	}
	header = (struct tbd_header *)(*fw)->data;

	if (sizeof(*header) > (*fw)->size)
		goto out;

	/* Check that we have drvb block. */
	if (memcmp(&header->tag, "DRVB", 4))
		goto out;

	if (header->size != (*fw)->size)
		goto out;

	if (sizeof(*header) + sizeof(*record) > (*fw)->size)
		goto out;


	return 0;

out:
		crlmodule_release_msrlist(fw);
		return ret;
}


void crlmodule_release_msrlist(const struct firmware **fw)
{
	release_firmware(*fw);
	*fw = NULL;
}
