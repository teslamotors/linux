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

#ifndef __IA_CSS_PSYS_TERMINAL_MANIFEST_SIM_H
#define __IA_CSS_PSYS_TERMINAL_MANIFEST_SIM_H

/*! \file */

/** @file ia_css_psys_terminal_manifest.sim.h
 *
 * Define the methods on the terminal manifest object: Simulation only
 */

#include <type_support.h>					/* size_t */
#include "ia_css_terminal.h"
#include "ia_css_terminal_manifest.h"
#include "ia_css_terminal_defs.h"

/*! Create (the storage for) the terminal manifest object

 @param	terminal_type[in]	type of the terminal manifest {parameter, data}

 @return NULL on error
 */
extern ia_css_terminal_manifest_t *ia_css_terminal_manifest_alloc(
	const ia_css_terminal_type_t			terminal_type);

/*! Destroy (the storage of) the terminal manifest object

 @param	manifest[in]			terminal manifest

 @return NULL
 */
extern ia_css_terminal_manifest_t *ia_css_terminal_manifest_free(
	ia_css_terminal_manifest_t				*manifest);

#endif /* __IA_CSS_PSYS_TERMINAL_MANIFEST_SIM_H */
