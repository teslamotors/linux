/*
* Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2010 - 2018, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
*/

#include "ia_css_rbm_manifest.h"
#include "ia_css_rbm.h"
#include "type_support.h"
#include "misc_support.h"
#include "assert_support.h"
#include "math_support.h"
#include "ia_css_rbm_trace.h"

#ifndef __IA_CSS_RBM_MANIFEST_INLINE__
#include "ia_css_rbm_manifest_impl.h"
#endif /* __IA_CSS_RBM_MANIFEST_INLINE__ */

STORAGE_CLASS_INLINE void
ia_css_rbm_print_with_header(
	const ia_css_rbm_t *rbm,
	const ia_css_rbm_mux_desc_t *mux,
	unsigned int mux_desc_count,
	bool print_header)
{
#ifdef __HIVECC
	ia_css_rbm_print(*rbm, NULL);
	(void)print_header;
	(void)mux_desc_count;
	(void)mux;
#else
	int i, j;

	assert(mux != NULL);
	assert(rbm != NULL);
	if (mux == NULL || rbm == NULL)
		return;

	if (print_header) {
		for (i = mux_desc_count - 1; i >= 0; i--) {
			PRINT("%*d|", mux[i].size_bits, mux[i].mux_id);
		}
		PRINT("\n");
	}
	for (i = mux_desc_count - 1; i >= 0; i--) {
		for (j = mux[i].size_bits - 1; j >= 0; j--) {
			PRINT("%d", ia_css_is_rbm_set(*rbm, j + mux[i].offset));
		}
		PRINT("|");
	}
#endif
}

STORAGE_CLASS_INLINE void
ia_css_rbm_validation_rule_print(
	ia_css_rbm_validation_rule_t *rule,
	ia_css_rbm_mux_desc_t *mux_desc,
	unsigned int mux_desc_count,
	bool print_header)
{
	ia_css_rbm_print_with_header(&rule->match, mux_desc, mux_desc_count, print_header);
#ifdef __HIVECC
	IA_CSS_TRACE_0(RBM, INFO, "Mask\n");
#else
	PRINT("\t");
#endif
	ia_css_rbm_print_with_header(&rule->mask, mux_desc, mux_desc_count, false);
#ifdef __HIVECC
	IA_CSS_TRACE_1(RBM, INFO, "Rule expected_value: %d\n", rule->expected_value);
#else
	PRINT("\t%d\n", rule->expected_value);
#endif
}

void
ia_css_rbm_pretty_print(
	const ia_css_rbm_t *rbm,
	const ia_css_rbm_mux_desc_t *mux,
	unsigned int mux_desc_count)
{
	ia_css_rbm_print_with_header(rbm, mux, mux_desc_count, false);
#ifndef __HIVECC
	PRINT("\n");
#endif
}

void
ia_css_rbm_manifest_print(
	const ia_css_rbm_manifest_t *manifest)
{
	int retval = -1;
	unsigned int i;
	bool print_header = true;
	ia_css_rbm_mux_desc_t *muxes;
	ia_css_rbm_validation_rule_t *validation_rule;
	ia_css_rbm_terminal_routing_desc_t *terminal_routing_desc;

	verifjmpexit(manifest != NULL);
	muxes = ia_css_rbm_manifest_get_muxes(manifest);
	verifjmpexit(muxes != NULL || manifest->mux_desc_count == 0);

	for (i = 0; i < manifest->mux_desc_count; i++) {
		IA_CSS_TRACE_4(RBM, INFO, "id: %d.%d offstet: %d size_bits: %d\n",
			muxes[i].gp_dev_id,
			muxes[i].mux_id,
			muxes[i].offset,
			muxes[i].size_bits);
	}
#if VIED_NCI_RBM_MAX_VALIDATION_RULE_COUNT != 0
	validation_rule = ia_css_rbm_manifest_get_validation_rules(manifest);
	verifjmpexit(validation_rule != NULL || manifest->validation_rule_count == 0);

	for (i = 0; i < manifest->validation_rule_count; i++) {
		ia_css_rbm_validation_rule_print(&validation_rule[i], muxes, manifest->mux_desc_count, print_header);
		print_header = false;
	}
#else
	(void) validation_rule;
	(void) print_header;
#endif
	terminal_routing_desc = ia_css_rbm_manifest_get_terminal_routing_desc(manifest);
	verifjmpexit(terminal_routing_desc != NULL || manifest->terminal_routing_desc_count == 0);
	for (i = 0; i < manifest->terminal_routing_desc_count; i++) {
		IA_CSS_TRACE_4(RBM, INFO, "terminal_id: %d connection_state: %d mux_id: %d state: %d\n",
			terminal_routing_desc[i].terminal_id,
			terminal_routing_desc[i].connection_state,
			terminal_routing_desc[i].mux_id,
			terminal_routing_desc[i].state);
	}

	retval = 0;
EXIT:
	if (retval != 0) {
		IA_CSS_TRACE_0(RBM, ERROR, "ia_css_rbm_manifest_print failed\n");
	}
}

bool
ia_css_rbm_manifest_check_rbm_validity(
	const ia_css_rbm_manifest_t *manifest,
	const ia_css_rbm_t *rbm)
{
	unsigned int i;
	ia_css_rbm_t res;
	ia_css_rbm_t final_rbm = ia_css_rbm_clear();
	ia_css_rbm_validation_rule_t *rules;
	bool matches_rules;

	verifjmpexit(manifest != NULL);
	verifjmpexit(rbm != NULL);

	if (ia_css_is_rbm_empty(*rbm)) {
		IA_CSS_TRACE_0(RBM, ERROR, "ia_css_rbm_manifest_check_rbm_validity failes: RBM is empty.\n");
		return false;
	}

#if VIED_NCI_RBM_MAX_VALIDATION_RULE_COUNT != 0
	rules = ia_css_rbm_manifest_get_validation_rules(manifest);
	verifjmpexit(rules != NULL || manifest->validation_rule_count == 0);

	for (i = 0; i < manifest->validation_rule_count; i++) {
		res = ia_css_rbm_intersection(*rbm, rules[i].mask);
		matches_rules = ia_css_is_rbm_equal(res, rules[i].match);

		if (!matches_rules)
			continue;

		if (rules[i].expected_value == 1) {
			final_rbm = ia_css_rbm_union(final_rbm, res);
		} else {
			IA_CSS_TRACE_1(RBM, INFO, "ia_css_rbm_manifest_check_rbm_validity failes on rule %d\n", 1);
			return false;
		}
	}
#else
	(void)matches_rules;
	(void)i;
	(void)rules;
	(void)res;
#endif
	return ia_css_is_rbm_equal(final_rbm, *rbm);
EXIT:
	return false;
}

ia_css_rbm_t
ia_css_rbm_set_mux(
	ia_css_rbm_t rbm,
	ia_css_rbm_mux_desc_t *mux,
	unsigned int mux_count,
	unsigned int gp_dev_id,
	unsigned int mux_id,
	unsigned int value)
{
	unsigned int i;

	verifjmpexit(mux != NULL);

	for (i = 0; i < mux_count; i++) {
		if (mux[i].gp_dev_id == gp_dev_id && mux[i].mux_id == mux_id)
			break;
	}
	if (i >= mux_count) {
		IA_CSS_TRACE_2(RBM, ERROR,
			"ia_css_rbm_set_mux mux with mux_id %d.%d not found\n", gp_dev_id, mux_id);
		return rbm;
	}
	if (value >= mux[i].size_bits) {
		IA_CSS_TRACE_3(RBM, ERROR,
			"ia_css_rbm_set_mux mux mux_id %d.%d, value %d illegal\n", gp_dev_id, mux_id, value);
		return rbm;
	}
	rbm = ia_css_rbm_set(rbm, mux[i].offset + value);
EXIT:
	return rbm;
}
