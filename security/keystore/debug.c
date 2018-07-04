/*
 *
 * Intel Keystore Linux driver
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "keystore_debug.h"

/**
 * Display a block of data hexadecimally.
 *
 * @param txt Prefix string to display.
 * @param ptr Pointer to the block of data.
 * @param size Size of the block of data in bytes.
 */
void keystore_hexdump(const char *txt, const void *ptr, unsigned int size)
{
	ks_debug("%s: size: %u (0x%lx)\n", txt, size, (unsigned long)ptr);

	if (ptr && size)
		print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, ptr, size);
}

/**
 * Compare two blocks of data. Show the first block. Show error message and the second block if different.
 *
 * @param name1 First data block name.
 * @param ptr1 First data block pointer.
 * @param name2 Second data block name.
 * @param ptr2 Second data block pointer.
 * @param size Size of data blocks in bytes.
 */
int show_and_compare(const char *name1, const void *ptr1, const char *name2,
		const void *ptr2, unsigned int size)
{
	int res = 0;

	if (!name1 || !ptr1 || !name2 || !ptr2)
		return -EFAULT;

	res = memcmp(ptr1, ptr2, size);

	ks_debug(KBUILD_MODNAME " Comparing buffers %s and %s\n", name1, name2);
	keystore_hexdump(name1, ptr1, size);
	if (res) {
		ks_debug(KBUILD_MODNAME " Buffers differ!\n");
		keystore_hexdump(name2, ptr2, size);
	} else {
		ks_debug(KBUILD_MODNAME " Buffers are identical.\n");
	}

	return res ? -1 : 0;
}
