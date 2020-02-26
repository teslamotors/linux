/*
 * Include this header to remove any trace_printk() calls from kernels without
 * CONFIG_DEBUG_INFO enabled so we don't include unnecessary debug code in
 * the production kernel.
 *
 * We remove trace_printk() using this method rather than modifying external
 * source code directly to make future patching/merging cleaner.
 */

#ifndef __TESLA_RM_TRACE_PRINTK_H__
#define __TESLA_RM_TRACE_PRINTK_H__

#include <linux/kconfig.h>

#ifndef CONFIG_DEBUG_INFO
#ifdef trace_printk
#undef trace_printk
#define trace_printk(fmt, ...)	do { } while (0)
#endif
#endif /* CONFIG_DEBUG_INFO */

#endif /* __TESLA_RM_TRACE_PRINTK_H__ */
