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

#ifndef __RECV_PORT_STRUCT_H__
#define __RECV_PORT_STRUCT_H__

#include "buffer_type.h"

struct recv_port {
	buffer_address buffer;	/* address of buffer in DDR */
	unsigned int size;
	unsigned int token_size;
	unsigned int wr_reg;	/* index of write pointer located in regmem */
	unsigned int rd_reg;	/* index read pointer located in regmem */

	unsigned int mmid;
	unsigned int ssid;
	unsigned int mem_addr;	/* address of memory containing regmem */
};

#endif /*__RECV_PORT_STRUCT_H__*/
