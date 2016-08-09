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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "manifest_cache.h"

/* The linked list of cache entries. */
struct list_head mf_cache = LIST_HEAD_INIT(mf_cache);

/**
 * Allocate a cache entry.
 *
 * @return Cache entry structure pointer or NULL if out of memory.
 */
static struct mf_cache_entry *mf_allocate_cache_entry(void)
{
	struct mf_cache_entry *item = NULL;
	struct list_head *pos, *q;
	unsigned int cnt = 0;

	/* calculate number of cache entries in use */
	list_for_each_safe(pos, q, &mf_cache) {
		item = list_entry(pos, struct mf_cache_entry, list);
		if (ktime_ms_delta(ktime_get(), item->expiry) >= 0) {
			list_del(pos);
			kzfree(item);
		} else
			cnt++;
	}

	/* check for maximum number of clients */
	if (cnt < MAX_CACHE_MANIFEST_ENTRIES) {
		/* allocate memory */
		item = kzalloc(sizeof(struct mf_cache_entry),
				GFP_KERNEL);
		if (item)
			list_add(&(item->list), &mf_cache);
	}

	return item;
}

/**
 * Find non-expired app in cache or remove the expired one.
 *
 * @param app_name Application name.
 *
 * @return remaining time if found or negative error code (see errno).
 */
int mf_find_in_cache(const char *app_name)
{
	struct list_head *pos, *q;
	struct mf_cache_entry *item;

	if (!app_name)
		return -EFAULT;

	list_for_each_safe(pos, q, &mf_cache) {
		item = list_entry(pos, struct mf_cache_entry, list);
		if (!strcmp(app_name, item->app_name)) {
			long long delta = ktime_ms_delta(ktime_get(), item->expiry);

			if (delta >= 0) {
				list_del(pos);
				kzfree(item);
				return -ESRCH;
			} else
				return (int) ((-delta + 999) / 1000);
		}
	}

	return -ESRCH;
}

/**
 * Add app to cache or update time to live if app already
 * exists in cache.
 *
 * @param app_name Application name.
 * @param time_to_live Time to live in cache (in seconds).
 *
 * @return 0 if OK or negative error code (see errno).
 */
int mf_add_to_cache(const char *app_name, int ttl)
{
	struct list_head *pos, *q;
	struct mf_cache_entry *item;

	if (!app_name)
		return -EFAULT;

	if (!*app_name || strlen(app_name) >= MAX_APP_NAME_SIZE)
		return -EINVAL;

	list_for_each_safe(pos, q, &mf_cache) {
		item = list_entry(pos, struct mf_cache_entry, list);
		if (!strcmp(app_name, item->app_name))
			goto found;
	}

	item = mf_allocate_cache_entry();
	if (!item)
		return -ENOMEM;

	strcpy(item->app_name, app_name);

found:
	item->expiry = ktime_add_ms(ktime_get(), ttl * 1000);
	return 0;
}

/**
 * Delete all cache entries.
 */
void mf_clear_cache(void)
{
	struct list_head *pos, *q;
	struct mf_cache_entry *item;

	list_for_each_safe(pos, q, &mf_cache) {
		item = list_entry(pos, struct mf_cache_entry, list);
		list_del(pos);
		kzfree(item);
	}
}

/**
 * Dump cache contents
 */
void mf_dump_cache(void)
{
	struct list_head *pos;
	struct mf_cache_entry *item;
	long long delta;

	list_for_each(pos, &mf_cache) {
		item = list_entry(pos, struct mf_cache_entry, list);
		delta = ktime_ms_delta(ktime_get(), item->expiry);
		pr_info(KBUILD_MODNAME ": cache: app=%s ttl=%d sec\n",
				item->app_name, (int) ((-delta + 999) / 1000));
	}
}

/**
 * Test cache
 */
void mf_test_cache(void)
{
	int res;

	pr_info(KBUILD_MODNAME ": mf_clear_cache()\n");
	mf_clear_cache();
	mf_dump_cache();

	res = mf_add_to_cache("/one", 60);
	pr_info(KBUILD_MODNAME ": mf_add_to_cache(\"/one\", 60) -> %d\n", res);
	mf_dump_cache();

	res = mf_add_to_cache("/two", 30);
	pr_info(KBUILD_MODNAME ": mf_add_to_cache(\"/two\", 30) -> %d\n", res);
	mf_dump_cache();

	res = mf_add_to_cache("/three", 0);
	pr_info(KBUILD_MODNAME ": mf_add_to_cache(\"/three\", 0) -> %d\n", res);
	mf_dump_cache();

	res = mf_find_in_cache("/one");
	pr_info(KBUILD_MODNAME ": mf_find_in_cache(\"/one\") -> %d\n", res);
	mf_dump_cache();

	res = mf_find_in_cache("/two");
	pr_info(KBUILD_MODNAME ": mf_find_in_cache(\"/two\") -> %d\n", res);
	mf_dump_cache();

	res = mf_find_in_cache("/three");
	pr_info(KBUILD_MODNAME ": mf_find_in_cache(\"/three\") -> %d\n", res);
	mf_dump_cache();

	res = mf_find_in_cache("/four");
	pr_info(KBUILD_MODNAME ": mf_find_in_cache(\"/four\") -> %d\n", res);
	mf_dump_cache();

	res = mf_add_to_cache("/four", 0);
	pr_info(KBUILD_MODNAME ": mf_add_to_cache(\"/four\", 0) -> %d\n", res);
	mf_dump_cache();

	res = mf_find_in_cache("/four");
	pr_info(KBUILD_MODNAME ": mf_find_in_cache(\"/four\") -> %d\n", res);
	mf_dump_cache();

	pr_info(KBUILD_MODNAME ": mf_clear_cache()\n");
	mf_clear_cache();
	mf_dump_cache();

}

/* end of file */
