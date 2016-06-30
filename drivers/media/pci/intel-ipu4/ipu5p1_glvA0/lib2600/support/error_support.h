/**
* INTEL CONFIDENTIAL
*
* Copyright (C) 2013 - 2016 Intel Corporation.
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

#ifndef __ERROR_SUPPORT_H_INCLUDED__
#define __ERROR_SUPPORT_H_INCLUDED__

#if defined(__KERNEL__)
#include <linux/errno.h>
#else
#include <errno.h>
#endif

/* OS-independent definition of IA_CSS errno values */
/* #define IA_CSS_EINVAL 1 */
/* #define IA_CSS_EFAULT 2 */

#define verifret(cond, error_type)   \
do {                                \
	if (!(cond)) {                   \
		return error_type;          \
	}                               \
} while (0)

#define verifjmp(cond, error_tag)    \
do {                                \
	if (!(cond)) {                   \
		goto error_tag;             \
	}                               \
} while (0)

#define verifexit(cond, error_tag)  \
do {                               \
	if (!(cond)) {              \
		goto EXIT;         \
	}                          \
} while (0)

#define verifjmpexit(cond)         \
do {                               \
	if (!(cond)) {              \
		goto EXIT;         \
	}                          \
} while (0)

#define verifjmpexitsetretval(cond, retval)         \
do {                               \
	if (!(cond)) {              \
		retval = -1;	   \
		goto EXIT;         \
	}                          \
} while (0)

#endif /* __ERROR_SUPPORT_H_INCLUDED__ */
