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

#ifndef _KEYSTORE_DEBUG_H_
#define _KEYSTORE_DEBUG_H_

#ifdef CONFIG_KEYSTORE_DEBUG

/**
 * keystore_hexdump() - Display a block of data hexadecimally.
 *
 * @txt: Prefix string to display.
 * @ptr: Pointer to the block of data.
 * @size: Size of the block of data in bytes.
 *
 * Will only be enabled if the %KEYSTORE_DEBUG config parameter
 * is activated.
 */
void keystore_hexdump(const char *txt, const void *ptr, unsigned int size);


/**
 * show_and_compare() - Compare two blocks of data. Show the first block.
 * Show error message and the second block if different.
 *
 * @name1: First data block name.
 * @ptr1: First data block pointer.
 * @name2: Second data block name.
 * @ptr2: Second data block pointer.
 * @size: Size of data blocks in bytes.
 *
 * Returns: 0 on success or negative error number.
 */
int show_and_compare(const char *name1, const void *ptr1,
		     const char *name2, const void *ptr2,
		     unsigned int size);

/* Dummy function if debugging is not configured */
/* Static inline so will be optimised away by compiler */
#else /* CONFIG_KEYSTORE_DEBUG */
static inline void keystore_hexdump(const char *txt, const void *ptr,
				    unsigned int size)
{
}

static inline int show_and_compare(const char *name1, const void *ptr1,
				   const char *name2, const void *ptr2,
				   unsigned int size)
{
	return 0;
}
#endif

#ifdef CONFIG_KEYSTORE_DEBUG

/* Enable output of pr_debug */
#define DEBUG

#define ks_info(...)  pr_info(__VA_ARGS__)
#define ks_debug(...) pr_debug(__VA_ARGS__)
#define ks_warn(...)  pr_warn(__VA_ARGS__)
#define ks_err(...)   pr_err(__VA_ARGS__)

#define FUNC_BEGIN   ks_info(KBUILD_MODNAME ": %s() BEGIN\n", __func__)
#define FUNC_END     ks_info(KBUILD_MODNAME ": %s() END\n", __func__)
#define FUNC_RES(x)  ks_info(KBUILD_MODNAME ": %s() END, res=%ld\n", __func__, \
			     (long)(x))
#define SHOW_LINE    ks_info(KBUILD_MODNAME ": %s:%u\n", __func__, __LINE__)

#else

#define ks_info(...)
#define ks_debug(...)
#define ks_warn(...)
#define ks_err(...)

#define FUNC_BEGIN
#define FUNC_END
#define FUNC_RES(x)
#define SHOW_LINE

#endif


#endif /* _KEYSTORE_DEBUG_H_ */
