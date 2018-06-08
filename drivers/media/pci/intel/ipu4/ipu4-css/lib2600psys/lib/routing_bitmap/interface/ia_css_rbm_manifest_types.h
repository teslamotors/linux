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

#ifndef __IA_CSS_RBM_MANIFEST_TYPES_H
#define __IA_CSS_RBM_MANIFEST_TYPES_H

#include "ia_css_rbm.h"
#include "vied_nci_psys_resource_model.h"

#ifndef VIED_NCI_RBM_MAX_MUX_COUNT
#error Please define VIED_NCI_RBM_MAX_MUX_COUNT
#endif
#ifndef VIED_NCI_RBM_MAX_VALIDATION_RULE_COUNT
#error Please define VIED_NCI_RBM_MAX_VALIDATION_RULE_COUNT
#endif
#ifndef VIED_NCI_RBM_MAX_TERMINAL_DESC_COUNT
#error Please define VIED_NCI_RBM_MAX_TERMINAL_DESC_COUNT
#endif
#ifndef N_PADDING_UINT8_IN_RBM_MANIFEST
#error Please define N_PADDING_UINT8_IN_RBM_MANIFEST
#endif

#define SIZE_OF_RBM_MUX_DESC_S ( \
	(4 * IA_CSS_UINT8_T_BITS))

typedef struct ia_css_rbm_mux_desc_s {
	uint8_t gp_dev_id;
	uint8_t mux_id;
	uint8_t offset;
	uint8_t size_bits;
} ia_css_rbm_mux_desc_t;

#define SIZE_OF_RBM_VALIDATION_RULE_DESC_S ( \
	  (2 * IA_CSS_RBM_BITS) \
	+ (1 * IA_CSS_UINT32_T_BITS))

typedef struct ia_css_rbm_validation_rule_s {
	ia_css_rbm_t match; /* RBM is an array of 32 bit elements */
	ia_css_rbm_t mask;
	uint32_t expected_value;
} ia_css_rbm_validation_rule_t;

#define SIZE_OF_RBM_TERMINAL_ROUTING_DESC_S ( \
	(4 * IA_CSS_UINT8_T_BITS))

typedef struct ia_css_rbm_terminal_routing_desc_s {
	uint8_t terminal_id;
	uint8_t connection_state;
	uint8_t mux_id;
	uint8_t state;
} ia_css_rbm_terminal_routing_desc_t;

#define SIZE_OF_RBM_MANIFEST_S ( \
	  (VIED_NCI_RBM_MAX_MUX_COUNT * SIZE_OF_RBM_MUX_DESC_S) \
	+ (VIED_NCI_RBM_MAX_VALIDATION_RULE_COUNT * SIZE_OF_RBM_VALIDATION_RULE_DESC_S) \
	+ (VIED_NCI_RBM_MAX_TERMINAL_DESC_COUNT * SIZE_OF_RBM_TERMINAL_ROUTING_DESC_S) \
	+ (3 * IA_CSS_UINT16_T_BITS) \
	+ (N_PADDING_UINT8_IN_RBM_MANIFEST * IA_CSS_UINT8_T_BITS))

typedef struct ia_css_rbm_manifest_s {
#if VIED_NCI_RBM_MAX_VALIDATION_RULE_COUNT > 0
	ia_css_rbm_validation_rule_t
		validation_rules[VIED_NCI_RBM_MAX_VALIDATION_RULE_COUNT];
#endif
	uint16_t mux_desc_count;
	uint16_t validation_rule_count;
	uint16_t terminal_routing_desc_count;

#if VIED_NCI_RBM_MAX_MUX_COUNT > 0
	ia_css_rbm_mux_desc_t
		mux_desc[VIED_NCI_RBM_MAX_MUX_COUNT];
#endif

#if VIED_NCI_RBM_MAX_TERMINAL_DESC_COUNT > 0
	ia_css_rbm_terminal_routing_desc_t
		terminal_routing_desc[VIED_NCI_RBM_MAX_TERMINAL_DESC_COUNT];
#endif

#if N_PADDING_UINT8_IN_RBM_MANIFEST > 0
	uint8_t padding[N_PADDING_UINT8_IN_RBM_MANIFEST];
#endif
} ia_css_rbm_manifest_t;

#endif /* __IA_CSS_RBM_MANIFEST_TYPES_H */
