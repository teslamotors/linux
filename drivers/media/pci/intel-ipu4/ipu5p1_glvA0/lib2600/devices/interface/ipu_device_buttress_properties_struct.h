/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2014 - 2016 Intel Corporation.
* All Rights Reserved.
*
* The source code contained or described herein and all documents
* related to the source code ("Material") are owned by Intel Corporation
* or licensors. Title to the Material remains with Intel
* Corporation or its licensors. The Material contains trade
* secrets and proprietary and confidential information of Intel or its
* licensors. The Material is protected by worldwide copyright
* and trade secret laws and treaty provisions. No part of the Material may
* be used, copied, reproduced, modified, published, uploaded, posted,
* transmitted, distributed, or disclosed in any way without Intel's prior
* express written permission.
*
* No License under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or
* delivery of the Materials, either expressly, by implication, inducement,
* estoppel or otherwise. Any license under such intellectual property rights
* must be express and approved by Intel in writing.
*/

#ifndef __BUTTRESS_PROPERTIES_STRUCT_H__
#define __BUTTRESS_PROPERTIES_STRUCT_H__

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

#define IA_CSS_INFO_BITS_M0_DDR (DEST_IS_DDR << IA_CSS_INFO_BITS_M0_REQUEST_DEST_POS)
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


#endif /* __BUTTRESS_PROPERTIES_STRUCT_H__ */
