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

/* implementation of buffer access from the CPU */
/* using shared_memory interface */

#include "buffer_access.h"
#include "vied/shared_memory_access.h"
/* #include <stdio.h> */

void
buffer_load(buffer_address address, void *data, unsigned int bytes, unsigned int mm_id)
{
	shared_memory_load(mm_id, address, data, bytes);
}

void
buffer_store(buffer_address address, const void *data, unsigned int bytes, unsigned int mm_id)
{
	shared_memory_store(mm_id, address, data, bytes);
}

