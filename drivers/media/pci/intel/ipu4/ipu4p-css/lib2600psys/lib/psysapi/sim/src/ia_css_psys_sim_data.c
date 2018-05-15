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


#include <ia_css_psys_sim_data.h>

#include "ia_css_psys_sim_trace.h"

static unsigned int ia_css_psys_ran_seed;

void ia_css_psys_ran_set_seed(const unsigned int seed)
{
	ia_css_psys_ran_seed = seed;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE,
		"ia_css_psys_ran_set_seed(): enter:\n");

}

static unsigned int ia_css_psys_ran_int (void)
{
	ia_css_psys_ran_seed = 1664525UL * ia_css_psys_ran_seed + 1013904223UL;
	return ia_css_psys_ran_seed;
}

unsigned int ia_css_psys_ran_var(const unsigned int bit_depth)
{
	unsigned int	out;
	unsigned int	tmp;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "ia_css_psys_ran_var(): enter:\n");

	tmp = ia_css_psys_ran_int();

	if (bit_depth > 32)
		out = tmp;
	else if (bit_depth == 0)
		out = 0;
	else
		out = (unsigned short)(tmp >> (32 - bit_depth));

	return out;
}

unsigned int ia_css_psys_ran_val(const unsigned int range)
{
	unsigned int	out;
	unsigned int	tmp;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE, "ia_css_psys_ran_val(): enter:\n");

	tmp = ia_css_psys_ran_int();

	if (range > 1)
		out = tmp % range;
	else
		out = 0;

	return out;
}

unsigned int ia_css_psys_ran_interval(const unsigned int lo,
				const unsigned int hi)
{
	unsigned int	out;
	unsigned int	tmp;
	unsigned int	range = hi - lo;

	IA_CSS_TRACE_0(PSYSAPI_SIM, VERBOSE,
		"ia_css_psys_ran_interval(): enter:\n");

	tmp = ia_css_psys_ran_int();

	if ((range > 1) && (lo < hi))
		out = lo + (tmp % range);
	else
		out = 0;

	return out;
}
