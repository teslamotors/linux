/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2019, Intel, co. */
#ifndef MANUF_MODE
#define MANUF_MODE

/**
 * @MANUF_MODE_SEALED: platform is sealed
 * @MANUF_MODE_ENABLED: manufacturing mode is enabled
 * @MANUF_MODE_UNDEF: manufacturing mode cannot be determined
 */
enum {
	MANUF_MODE_SEALED = 0,
	MANUF_MODE_ENABLED = 1,
	MANUF_MODE_UNDEF = 2,
};

#ifdef CONFIG_MANUF_MODE
unsigned int manuf_mode(void);
#else
static inline unsigned int manuf_mode(void)
{
	return MANUF_MODE_UNDEF;
}
#endif /* CONFIG_MANUF_MODE */

#endif /* MANUF_MODE */
