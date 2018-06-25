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

#include <asm/page.h>
#include <linux/errno.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/sort.h>

#include <security/abl_cmdline.h>
#if !defined(CONFIG_KEYSTORE_HARD_CODED_SEED)
#include <soc/apl/abl.h>
#endif
#include <security/keystore_api_user.h>
#include "keystore_debug.h"
#include "keystore_seed.h"

#define ABL_SEED_MAX_ENTRIES 4
#define ABL_IVI_SEED_LEN 64

struct abl_seed_entry_t {
	uint8_t          cse_svn;
	uint8_t          bios_svn;
	uint8_t          Reserved[2];
	uint8_t          seed[SEC_SEED_SIZE];
}  __attribute__((packed));

struct abl_seed_list_t {
	uint32_t                number_of_seeds;
	struct abl_seed_entry_t list[ABL_SEED_MAX_ENTRIES];
}  __attribute__((packed));

/* Device and user Seed lists */
struct abl_seed_list_t sec_seed_list[MAX_SEED_TYPES];

/* Device and User seed - derived from platform seed and nonces from PDR */
uint8_t sec_seed[MAX_SEED_TYPES][SEC_SEED_SIZE];
bool seed_available[MAX_SEED_TYPES];

#if !defined(CONFIG_KEYSTORE_HARD_CODED_SEED)

static int keystore_copy_seeds(struct seed_offset *offset)
{
	int res = 0;

	/* Set default availability */
	seed_available[SEED_TYPE_DEVICE] = false;
	seed_available[SEED_TYPE_USER] = false;

	/* First zero the contents */
	memset(sec_seed, 0, sizeof(sec_seed));

	if (!offset) {
		ks_err(KBUILD_MODNAME ": keystore_get_seed: ksoffset NULL!\n");
		return -EFAULT;
	}

	if (offset->device_seed) {
#if !defined(CONFIG_KEYSTORE_DISABLE_DEVICE_SEED)
		res = memcpy_from_ph(sec_seed[SEED_TYPE_DEVICE],
				     (phys_addr_t)offset->device_seed,
				     (size_t)SEC_SEED_SIZE);

		if (res) {
			ks_err(KBUILD_MODNAME ": Error retrieving Device SEED\n");
			return res;
		}

		seed_available[SEED_TYPE_DEVICE] = true;
#endif
		memset_ph((phys_addr_t)offset->device_seed, 0,
			  (size_t)SEC_SEED_SIZE);
	}

	if (offset->user_seed) {
#if !defined(CONFIG_KEYSTORE_DISABLE_USER_SEED)
		res = memcpy_from_ph(sec_seed[SEED_TYPE_USER],
				     (phys_addr_t)offset->user_seed,
				     (size_t)SEC_SEED_SIZE);
		if (res) {
			ks_err(KBUILD_MODNAME ": Error retrieving User SEED\n");
			return res;
		}

		seed_available[SEED_TYPE_USER] = true;

#endif
		memset_ph((phys_addr_t)offset->user_seed, 0,
			  (size_t)SEC_SEED_SIZE);
	}

	return res;
}

static int keystore_copy_seed_list(struct seed_offset *offset)
{
	int res = 0;

	/* Set default availability */
	memset(sec_seed_list, 0, sizeof(sec_seed_list));

	if (!offset) {
		ks_err(KBUILD_MODNAME ": keystore_get_seed: ksoffset NULL!\n");
		return -EFAULT;
	}

	if (offset->device_seed) {
#if !defined(CONFIG_KEYSTORE_DISABLE_DEVICE_SEED)
		res = memcpy_from_ph(&sec_seed_list[SEED_TYPE_DEVICE],
				     (phys_addr_t)offset->device_seed,
				     sizeof(struct abl_seed_list_t));

		if (res) {
			ks_err(KBUILD_MODNAME ": Error retrieving Device SEED list!\n");
			return res;
		}
#endif
		memset_ph((phys_addr_t)offset->device_seed, 0,
			  sizeof(struct abl_seed_list_t));
	}

	if (offset->user_seed) {
#if !defined(CONFIG_KEYSTORE_DISABLE_USER_SEED)
		res = memcpy_from_ph(&sec_seed_list[SEED_TYPE_USER],
				     (phys_addr_t)offset->user_seed,
				     sizeof(struct abl_seed_list_t));
		if (res) {
			ks_err(KBUILD_MODNAME ": Error retrieving User SEED list!\n");
			return res;
		}
#endif
		memset_ph((phys_addr_t)offset->user_seed, 0,
			  sizeof(struct abl_seed_list_t));
	}

	return res;
}
#endif

static int seed_list_compare(const void *left, const void *right)
{
	struct abl_seed_entry_t *left_entry = (struct abl_seed_entry_t *)left;
	struct abl_seed_entry_t *right_entry = (struct abl_seed_entry_t *)right;

	if (!left_entry || !right_entry)
		return 0;

	return right_entry->cse_svn - left_entry->cse_svn;
}

static int keystore_sort_seed_list(struct abl_seed_list_t *list)
{
	if (!list)
		return -EFAULT;

	if (list->number_of_seeds > ABL_SEED_MAX_ENTRIES)
		list->number_of_seeds = ABL_SEED_MAX_ENTRIES;

	sort(list->list, list->number_of_seeds, sizeof(struct abl_seed_entry_t),
	     seed_list_compare, NULL);

	return 0;
}

int keystore_fill_seeds(void)
{
	int res = 0;
#ifdef CONFIG_KEYSTORE_HARD_CODED_SEED
	unsigned int sl_index = 0;

	ks_warn(KBUILD_MODNAME ": Using hardcoded SEEDs for key generation!\n");
	ks_warn(KBUILD_MODNAME ": Keys wrapped with keystore will not be secure!\n");

	memset(sec_seed_list, 0, sizeof(sec_seed_list));
	sec_seed_list[SEED_TYPE_DEVICE].number_of_seeds = 3;
	for (sl_index = 0; sl_index < sec_seed_list[SEED_TYPE_DEVICE].number_of_seeds; sl_index++) {
		sec_seed_list[SEED_TYPE_DEVICE].list[sl_index].cse_svn = sl_index;
		memset(sec_seed_list[SEED_TYPE_DEVICE].list[sl_index].seed,
				((sl_index + 0xa5) & 0xFF), SEC_SEED_SIZE);
	}

	sec_seed_list[SEED_TYPE_USER].number_of_seeds = 4;
	for (sl_index = 0; sl_index < sec_seed_list[SEED_TYPE_USER].number_of_seeds; sl_index++) {

		sec_seed_list[SEED_TYPE_USER].list[sl_index].cse_svn = sl_index;
		memset(sec_seed_list[SEED_TYPE_USER].list[sl_index].seed,
				((sl_index + 0xb6) & 0xFF), SEC_SEED_SIZE);
	}

	keystore_sort_seed_list(&sec_seed_list[SEED_TYPE_USER]);
	keystore_sort_seed_list(&sec_seed_list[SEED_TYPE_DEVICE]);

#if defined(CONFIG_KEYSTORE_DISABLE_DEVICE_SEED)
	seed_available[SEED_TYPE_DEVICE] = false;
#elif defined(CONFIG_KEYSTORE_DISABLE_USER_SEED)
	seed_available[SEED_TYPE_USER] = false;
#endif

#else
	struct seed_offset s_off;
	/* Get keys and seeds offsets from cmdline */
	res = get_apl_seed_offsets(&s_off);
	if (res) {
		ks_info(KBUILD_MODNAME ": d/u-seed info missing in cmdline\n");
	} else {
		/* Copy single SEED values */
		keystore_copy_seeds(&s_off);
	}

	/* Get SEED list from command line */
	res = get_apl_seed_list_offsets(&s_off);
	if (res) {
		ks_info(KBUILD_MODNAME ": d/u-seed list info missing in cmdline\n");
	} else {
		/* Copy SEED lists */
		keystore_copy_seed_list(&s_off);

		/* Sort the list so that highest SVN is at position 0 */
		keystore_sort_seed_list(&sec_seed_list[SEED_TYPE_USER]);
		keystore_sort_seed_list(&sec_seed_list[SEED_TYPE_DEVICE]);
	}

	/* Check that at least one device SEED available */
	if (!seed_available[SEED_TYPE_DEVICE] &&
	    sec_seed_list[SEED_TYPE_DEVICE].number_of_seeds == 0)
		return -EPERM;
#endif
	return res;
}

const uint8_t *keystore_get_seed_list_entry(enum keystore_seed_type type, unsigned int i)
{
	if ((type != SEED_TYPE_DEVICE) && (type != SEED_TYPE_USER))
		return NULL;

	if (i >= sec_seed_list[type].number_of_seeds)
		return NULL;

	ks_debug(KBUILD_MODNAME ": SEED-SVN = %d\n",
					sec_seed_list[type].list[i].cse_svn);
	keystore_hexdump("SEED Value",
			sec_seed_list[type].list[i].seed, SEC_SEED_SIZE);

	return sec_seed_list[type].list[i].seed;
}

unsigned int keystore_get_seed_list_size(enum keystore_seed_type type)
{
	if ((type != SEED_TYPE_DEVICE) && (type != SEED_TYPE_USER))
		return 0;

	return sec_seed_list[type].number_of_seeds;
}

const uint8_t *keystore_get_seed(enum keystore_seed_type type)
{
	if ((type != SEED_TYPE_DEVICE) && (type != SEED_TYPE_USER))
		return NULL;

	/* First check if the SEED is in the SEED list */
	if (keystore_get_seed_list_size(type) > 0) {
		ks_debug(KBUILD_MODNAME ": Seed list available for type %u\n");
		return keystore_get_seed_list_entry(type, 0);
	}

	/* No SEED available in list, check single value */
	ks_debug(KBUILD_MODNAME ": Seed %u: %s\n",
		 type, seed_available[type] ? "Enabled" : "Disabled");

	if (!seed_available[type])
		return NULL;

	keystore_hexdump("SEED Value", sec_seed[type], SEC_SEED_SIZE);

	return sec_seed[type];
}



/* end of file */
