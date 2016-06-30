/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2015 - 2015 Intel Corporation.
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

#ifndef _IA_CSS_PKG_DIR_TYPES_H_
#define _IA_CSS_PKG_DIR_TYPES_H_

#include "type_support.h"

struct ia_css_pkg_dir_entry {
	uint32_t address[2];
	uint32_t size;
	uint16_t version;
	uint8_t  type;
	uint8_t  unused;
};

typedef void ia_css_pkg_dir_t;
typedef struct ia_css_pkg_dir_entry ia_css_pkg_dir_entry_t;

/* The version field of the pkg_dir header defines if entries contain offsets or pointers */
/* This is temporary, until all pkg_dirs use pointers */
enum ia_css_pkg_dir_version {
	IA_CSS_PKG_DIR_POINTER,
	IA_CSS_PKG_DIR_OFFSET
};


#endif /* _IA_CSS_PKG_DIR_TYPES_H_ */

