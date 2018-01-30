/**
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

#ifndef __IPU_DEVICE_BUTTRESS_PROPERTIES_STRUCT_H
#define __IPU_DEVICE_BUTTRESS_PROPERTIES_STRUCT_H

/* Destination values for master port 0 and bitfield "request_dest" */
enum cio_M0_btrs_dest {
	DEST_IS_BUT_REGS = 0,
	DEST_IS_DDR,
	RESERVED,
	DEST_IS_SUBSYSTEM,
	N_BTRS_DEST
};

/* Bit-field positions for M0 info bits */
enum ia_css_info_bits_m0_pos {
	IA_CSS_INFO_BITS_M0_SNOOPABLE_POS	= 0,
	IA_CSS_INFO_BITS_M0_IMR_DESTINED_POS	= 1,
	IA_CSS_INFO_BITS_M0_REQUEST_DEST_POS	= 4
};

#define IA_CSS_INFO_BITS_M0_DDR \
	(DEST_IS_DDR << IA_CSS_INFO_BITS_M0_REQUEST_DEST_POS)
#define IA_CSS_INFO_BITS_M0_SNOOPABLE (1 << IA_CSS_INFO_BITS_M0_SNOOPABLE_POS)

/* Info bits as expected by the buttress */
/* Deprecated because bit fields are not portable */

/* For master port 0*/
union cio_M0_t {
	struct {
		unsigned int snoopable		: 1;
		unsigned int imr_destined	: 1;
		unsigned int spare0		: 2;
		unsigned int request_dest	: 2;
		unsigned int spare1		: 26;
	} as_bitfield;
	unsigned int as_word;
};

/* For master port 1*/
union cio_M1_t {
	struct {
		unsigned int spare0		: 1;
		unsigned int deadline_pointer	: 1;
		unsigned int reserved		: 1;
		unsigned int zlw		: 1;
		unsigned int stream_id		: 4;
		unsigned int address_swizzling	: 1;
		unsigned int spare1		: 23;
	} as_bitfield;
	unsigned int as_word;
};


#endif /* __IPU_DEVICE_BUTTRESS_PROPERTIES_STRUCT_H */
