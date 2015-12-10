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

#ifndef _KEYSTORE_RAND_H_
#define _KEYSTORE_RAND_H_

#include <linux/types.h>

/**
 * Function gets CPU information then checks if CPU support RDRAND instruction and
 * finally get data as int from RDRAND register
 * If CPU does not support RDRAND ENODATA will be return
 * @param random is pointer to simple data type in case NULL EINVAL will be returned
 */
int keystore_get_rdrand(uint8_t *buf, int size);

#endif /* _KEYSTORE_RAND_H_ */
