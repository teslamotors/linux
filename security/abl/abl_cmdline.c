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

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <asm/setup.h>  /* COMMAND_LINE_SIZE */

#include <security/abl_cmdline.h>

/* tag format from ABL */
#define TAG_ABL_KEYS		"ABL.keys"
#define TAG_ABL_SEED		"ABL.seed"

/* below macros are used to check return stat of sscanf() */
#define ABL_RET_KSIZE_IKEY		2
#define ABL_RET_KSIZE_IKEY_OKEY		3
#define ABL_RET_DSEED_USEED		2

/* offset format from ABL */

/* ikey/okey available - highly likely*/
#define ABL_KEYS_FORMAT0	"ABL.keys=%u@%x,%x"
/* ikey available, okey hash na - likely */
#define ABL_KEYS_FORMAT1	"ABL.keys=%u@%x,"

/* below cases are unlikely as we have intel key as part of kpimage */
/* okey available, ikey hash na - unlikely */
#define ABL_KEYS_FORMAT2	"ABL.keys=%u@,%x"
/* ikey/okey hash na - unlikely */
#define ABL_KEYS_FORMAT3	"ABL.keys=%u@,"

/* device and user seed */
#define ABL_SEED_FORMAT		"ABL.seed=%x,%x"

/**
 * Functions to read and copy the cmdline string
 *
 * @param cmdline - pointer to output cmdline string
 * @param size - size of passed memory
 *
 * @return 0 if OK or negative error code (see errno).
 */
static int read_cmdline(char *cmdline, int size)
{
	if (cmdline && saved_command_line) {
		/* copy the saved cmdline */
		strncpy(cmdline, saved_command_line,
			strnlen(saved_command_line, size));
	} else {
		pr_err(KBUILD_MODNAME ": cmdline is NULL\n");
		return -EINVAL;
	}
	return 0;
}

/**
 * Parse kernel cmdline string and returns the pointer to a substing
 * with complete values.
 *
 * @param tag - cmdline tag to be searched for
 * @param cmdline - pointer to cmdline string
 *
 * @return NULL on error, if OK returns the pointer to substring
 * remember to free the substring.
 */

static char *get_strtag(char *tag, const char *cmdline)
{
	char *p = NULL;
	char *q = NULL;
	char *rptr = NULL;
	char ch = '\0';

	if (!tag || !cmdline) {
		pr_err(KBUILD_MODNAME ": null ptr - tag: %p cmdline:%p\n",
				tag, cmdline);
		return NULL;
	}

	/* get the tag substring */
	p = strstr(cmdline, tag);
	if (p) {
		q = strstr(p, " ");
		if (q && *q == ' ') {
			ch = *q;
			*q = '\0';
		}

		/* allocate memory for substring, sizeof substring + 1 bytes */
		rptr = kzalloc(strnlen(p, COMMAND_LINE_SIZE) + 1, GFP_KERNEL);
		if (!rptr)
			return NULL;

		/* copy the substring to allocated mem */
		strncpy(rptr, p, strnlen(p, COMMAND_LINE_SIZE)+1);
		/* restore the char if it was altered */
		if (q && ch != '\0')
			*q = ch;
	} else {
		pr_err(KBUILD_MODNAME ": %s not found\n", tag);
	}

	return rptr;
}

int get_seed_offsets(struct seed_offset *ksoff)
{
	static char cmdline[COMMAND_LINE_SIZE];
	int res;
	char *s = NULL;

	if (!ksoff) {
		pr_err(KBUILD_MODNAME ": null ptr - ksoff: %p\n", ksoff);
		return -EFAULT;
	}

	memset(ksoff, 0, sizeof(struct seed_offset));
	memset(cmdline, 0, sizeof(cmdline));
	res = read_cmdline(cmdline, COMMAND_LINE_SIZE);
	if (res < 0)
		return res;

	/* get the substrings for kestore required tags */
	s = get_strtag(TAG_ABL_SEED, cmdline);

	/* return error if keys or seed params are missing in cmdline */
	if (!s) {
		pr_err(KBUILD_MODNAME ": seedtag: missing.\n");
		res = -ENODATA;
		goto err;
	}

	/* Scan ABL.seed tag */
	res = sscanf(s, ABL_SEED_FORMAT,
		     &ksoff->device_seed,
		     &ksoff->user_seed);
	pr_info(KBUILD_MODNAME ": %s - tag scan: got %d values[%u, %u]\n",
		s, res,
		ksoff->device_seed,
		ksoff->user_seed);
	/* check if we got both the seed offsets */
	if (res < ABL_RET_DSEED_USEED) {
		pr_err(KBUILD_MODNAME ": %s - key missing/wrong format (res: %d)\n",
		       s, res);
		res = -ENODATA;
		goto err;
	}

	/* all good, success */
	res = 0;
err:
	/* free strdup mem, kfree have its own NULL check */
	kfree(s);

	return res;
}
EXPORT_SYMBOL(get_seed_offsets);

int get_seckey_offsets(struct key_offset *ksoff)
{
	static char cmdline[COMMAND_LINE_SIZE];
	int res;
	char *k = NULL;

	if (!ksoff) {
		pr_err(KBUILD_MODNAME ": null ptr - ksoff: %p\n", ksoff);
		return -EINVAL;
	}

	memset(ksoff, 0, sizeof(struct key_offset));
	memset(cmdline, 0, sizeof(cmdline));
	res = read_cmdline(cmdline, COMMAND_LINE_SIZE);
	if (res < 0)
		return res;

	/* get the substrings for kestore required tags */
	k = get_strtag(TAG_ABL_KEYS, cmdline);

	/* return error if keys or seed params are missing in cmdline */
	if (!k) {
		pr_err(KBUILD_MODNAME ": keytag: missing\n");
		res = -ENODATA;
		goto err;
	}

	/* Scan ABL.keys tag */
	res = sscanf(k, ABL_KEYS_FORMAT0, &ksoff->ksize,
		     (unsigned int *)&ksoff->ikey,
		     (unsigned int *)&ksoff->okey);
	pr_info(KBUILD_MODNAME ": %s - tag scan(fmt0): got %d values[%d, %ld, %ld]\n",
			k, res, ksoff->ksize, ksoff->ikey, ksoff->okey);
	/* check if we get all required offsets */
	if (res != ABL_RET_KSIZE_IKEY_OKEY) {
		/* check if we atleast have ksize and ikey */
		res = sscanf(k, ABL_KEYS_FORMAT1, &ksoff->ksize,
			     (unsigned int *)&ksoff->ikey);
		pr_info(KBUILD_MODNAME ": %s - tag scan(fmt1): got %d values [%d, %ld, %ld]\n",
			k, res, ksoff->ksize, ksoff->ikey, ksoff->okey);
		if (res != ABL_RET_KSIZE_IKEY) {
			pr_err(KBUILD_MODNAME ": %s - key missing/wrong format (res: %d)\n",
			       k, res);
			res = -ENODATA;
			goto err;
		}
	}

	/* all good, success */
	res = 0;
err:
	/* free strdup mem, kfree have its own NULL check */
	kfree(k);

	return res;
}
EXPORT_SYMBOL(get_seckey_offsets);

#ifndef ARCH_HAS_VALID_PHYS_ADDR_RANGE
static inline int valid_phys_addr_range(phys_addr_t addr, size_t count)
{
	return addr + count <= __pa(high_memory);
}
#endif

static inline unsigned long size_inside_page(unsigned long start,
					     unsigned long size)
{
	unsigned long sz = PAGE_SIZE - (start & (PAGE_SIZE - 1));

	return min(sz, size);
}

int memcpy_from_ph(void *dest, phys_addr_t p, size_t count)
{
	if (!valid_phys_addr_range(p, count))
		return -EFAULT;

	while (count > 0) {
		ssize_t sz = size_inside_page(p, count);

		memcpy(dest, __va(p), sz);
		dest += sz;
		p += sz;
		count -= sz;
	}

	return 0;
}
EXPORT_SYMBOL(memcpy_from_ph);

int memset_ph(phys_addr_t p, int val, size_t count)
{
	if (!valid_phys_addr_range(p, count))
		return -EFAULT;

	while (count > 0) {
		ssize_t sz = size_inside_page(p, count);

		memset(__va(p), val, sz);
		p += sz;
		count -= sz;
	}

	return 0;
}
EXPORT_SYMBOL(memset_ph);

/* end of file */
