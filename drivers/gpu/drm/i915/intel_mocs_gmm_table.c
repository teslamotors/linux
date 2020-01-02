/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#include "i915_drv.h"
#include "intel_mocs.h"

static const struct drm_i915_mocs_entry gen_9_mocs_table[] = {
	{0x00000009, 0x0010}, /* ED_UC LLC/eLLC EDSCC:0 L3SCC:0 L3_UC */
	{0x00000038, 0x0030}, /* ED_PTE LLC/eLLC LRU_L EDSCC:0 L3SCC:0 L3_WB */
	{0x0000003b, 0x0030}, /* ED_WB LLC/eLLC LRU_L EDSCC:0 L3SCC:0 L3_WB */
	{0x00000039, 0x0030}, /* ED_UC LLC/eLLC LRU_L EDSCC:0 L3SCC:0 L3_WB */
	{0x0000003b, 0x0010}, /* ED_WB LLC/eLLC LRU_L EDSCC:0 L3SCC:0 L3_UC */
	{0x00000039, 0x0010}, /* ED_UC LLC/eLLC LRU_L EDSCC:0 L3SCC:0 L3_UC */
	{0x0000001b, 0x0030}, /* ED_WB LLC/eLLC LRU_S EDSCC:0 L3SCC:0 L3_WB */
	{0x00000037, 0x0010}, /* ED_WB LLC LRU_L EDSCC:0 L3SCC:0 L3_UC */
	{0x00000037, 0x0030}, /* ED_WB LLC LRU_L EDSCC:0 L3SCC:0 L3_WB */
	{0x00000017, 0x0010}, /* ED_WB LLC LRU_S EDSCC:0 L3SCC:0 L3_UC */
	{0x00000033, 0x0030}, /* ED_WB eLLC LRU_L EDSCC:0 L3SCC:0 L3_WB */
	{0x00000033, 0x0010}, /* ED_WB eLLC LRU_L EDSCC:0 L3SCC:0 L3_UC */
	{0x00000017, 0x0030}, /* ED_WB LLC LRU_S EDSCC:0 L3SCC:0 L3_WB */
	{0x00000019, 0x0010}, /* ED_UC LLC/eLLC LRU_S EDSCC:0 L3SCC:0 L3_UC */
};

static const struct drm_i915_mocs_entry broxton_mocs_table[] = {
	{0x00000009, 0x0010}, /* ED_UC LLC/eLLC EDSCC:0 L3SCC:0 L3_UC */
	{0x00000038, 0x0030}, /* ED_PTE LLC/eLLC LRU_L EDSCC:0 L3SCC:0 L3_WB */
	{0x00000039, 0x0030}, /* ED_UC LLC/eLLC LRU_L EDSCC:0 L3SCC:0 L3_WB */
	{0x00000005, 0x0010}, /* ED_UC LLC EDSCC:0 L3SCC:0 L3_UC */
	{0x00000005, 0x0030}, /* ED_UC LLC EDSCC:0 L3SCC:0 L3_WB */
};

/**
 * get_mocs_settings
 *
 * This function will return the values of the MOCS table that needs to
 * be programmed for the platform. It will return the values that need
 * to be programmed and if they need to be programmed.
 *
 * If the return values is false then the registers do not need programming.
 */
bool get_mocs_settings(struct drm_i915_private *dev_priv,
		       struct drm_i915_mocs_table *table) {
	bool result = false;

	if (IS_SKYLAKE(dev_priv) || IS_KABYLAKE(dev_priv)) {
		table->size  = ARRAY_SIZE(gen_9_mocs_table);
		table->table = gen_9_mocs_table;
		result = true;
	} else if (IS_BROXTON(dev_priv)) {
		table->size  = ARRAY_SIZE(broxton_mocs_table);
		table->table = broxton_mocs_table;
		result = true;
	} else {
		WARN_ONCE(INTEL_INFO(dev_priv)->gen >= 9,
			  "Platform that should have a MOCS table does not.\n");
	}

#define L3_ESC(value)		((value) << 0)
#define L3_SCC(value)		((value) << 1)

	/* WaDisableSkipCaching:skl,bxt,kbl */
	if (IS_GEN9(dev_priv)) {
		int i;

		for (i = 0; i < table->size; i++)
			if (WARN_ON(table->table[i].l3cc_value &
				    (L3_ESC(1) | L3_SCC(0x7))))
				return false;
	}

	return result;
}
