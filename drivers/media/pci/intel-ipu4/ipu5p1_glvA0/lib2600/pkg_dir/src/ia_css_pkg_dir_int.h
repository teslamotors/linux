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

#ifndef _IA_CSS_PKG_DIR_INT_H_
#define _IA_CSS_PKG_DIR_INT_H_

/*
 *	Package Dir structure as specified in CSE FAS
 *
 *	PKG DIR Header
 *	Qword	63:56	55	54:48	47:32	31:24	23:0
 *	0	"_IUPKDR_"
 *	1	Rsvd	Rsvd	Type	Version	Rsvd	Size
 *
 *	Version:	Version of the Structure
 *	Size:	Size of the entire table (including header) in 16 byte chunks
 *	Type:	Must be 0 for header
 *
 *	Figure 13: PKG DIR Header
 *
 *
 *	PKG DIR Entry
 *	Qword	63:56	55	54:48	47:32	31:24	23:0
 *	N	Address/Offset
 *	N+1	Rsvd	Rsvd	Type	Version	Rsvd	Size
 *
 *	Version:	Version # of the Component
 *	Size:	Size of the component in bytes
 *	Type:	Component Identifier
 */

#define PKG_DIR_SIZE_BITS 24
#define PKG_DIR_TYPE_BITS 7

#define PKG_DIR_MAGIC_VAL_1	(('_' << 24) | ('I' << 16) | ('U' << 8) | 'P')
#define PKG_DIR_MAGIC_VAL_0	(('K' << 24) | ('D' << 16) | ('R' << 8) | '_')

#endif /* _IA_CSS_PKG_DIR_INT_H_ */

