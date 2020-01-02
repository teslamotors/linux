/*
 *
 * Intel Manifest Cache Linux driver
 * Copyright (c) 2016, Intel Corporation.
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

#ifndef _MANIFEST_CACHE_H_
#define _MANIFEST_CACHE_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/ktime.h>

#define MAX_APP_NAME_SIZE             255
#define MAX_CACHE_MANIFEST_ENTRIES    1000

/**
 * struct mf_cache_entry - The manifest cache entry structure.
 *
 * @list: Kernel list head for linked list.
 * @app_name: Application name.
 * @expiry: Expiration time.
 */
struct mf_cache_entry {
	struct list_head list;                               /* kernel list */
	char app_name[MAX_APP_NAME_SIZE];                    /* app name */
	ktime_t expiry;                                      /* expiration time */
};

/**
 * Find non-expired app in cache or remove the expired one.
 *
 * @param app_name Application name.
 *
 * @return remaining time if found or negative error code (see errno).
 */
int mf_find_in_cache(const char *app_name);

/**
 * Add app to cache or update time to live if app already
 * exists in cache.
 *
 * @param app_name Application name.
 * @param time_to_live Time to live in cache (in seconds).
 *
 * @return 0 if OK or negative error code (see errno).
 */
int mf_add_to_cache(const char *app_name, int ttl);

/**
 * Delete all cache entries.
 */
void mf_clear_cache(void);

/**
 * Dump cache contents
 */
void mf_dump_cache(void);

/**
 * Test cache
 */
void mf_test_cache(void);

#endif /* _MANIFEST_CACHE_H_ */
