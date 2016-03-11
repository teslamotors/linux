/*
 *
 * Intel Secure Boot driver
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

#ifndef _SB_H_
#define _SB_H_

/**
 * DOC: Secure Boot
 *
 * The sb module obtains the secure boot status from the
 * Converged Security Engine (CSE) and caches it locally.
 *
 * The status is available via the get_sb_stat() function.
 *
 */

/**
 * get_sb_stat() - Function to check secure boot status.
 *
 * Returns: 1 on SB enabled systems else returns 0
 */
unsigned char get_sb_stat(void);

#endif /* _SB_H_ */
