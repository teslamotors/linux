/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2015 - 2016 Intel Corporation.
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

#ifndef _IA_CSS_PKG_DIR_IUNIT_H_
#define _IA_CSS_PKG_DIR_IUNIT_H_

/* In bootflow, pkg_dir only supports upto 16 entries in pkg_dir
 * pkg_dir_header + Psys_server pg + Isys_server pg + 13 Client pg
 */

enum  {
	IA_CSS_PKG_DIR_SIZE    = 16,
	IA_CSS_PKG_DIR_ENTRIES = IA_CSS_PKG_DIR_SIZE - 1
};

#define IUNIT_MAX_CLIENT_PKG_ENTRIES	13

/* Example assignment of unique identifiers for the FW components
 * This should match the identifiers in the manifest
 */
enum ia_css_pkg_dir_entry_type {
	IA_CSS_PKG_DIR_HEADER = 0,
	IA_CSS_PKG_DIR_PSYS_SERVER_PG,
	IA_CSS_PKG_DIR_ISYS_SERVER_PG,
	IA_CSS_PKG_DIR_CLIENT_PG
};

/* Fixed entries in the package directory */
enum ia_css_pkg_dir_index {
	IA_CSS_PKG_DIR_PSYS_INDEX = 0,
	IA_CSS_PKG_DIR_ISYS_INDEX = 1,
	IA_CSS_PKG_DIR_CLIENT_0   = 2
};

#endif /* _IA_CSS_PKG_DIR_IUNIT_H_ */

