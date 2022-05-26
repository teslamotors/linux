/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AppArmor security module
 *
 * This file contains AppArmor security identifier (secid) definitions
 *
 * Copyright 2009-2018 Canonical Ltd.
 */

#ifndef __AA_SECID_H
#define __AA_SECID_H

#include <linux/types.h>

/* secid value that will not be allocated */
#define AA_SECID_INVALID 0
#define AA_SECID_ALLOC AA_SECID_INVALID

u32 aa_alloc_secid(void);
void aa_free_secid(u32 secid);

#endif /* __AA_SECID_H */
