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

#ifndef __IA_CSS_RETURN_TOKEN__
#define __IA_CSS_RETURN_TOKEN__

#include "storage_class.h"
#include "assert_support.h"

/* ia_css_return_token: data item of exacly 8 bytes (64 bits)
 * which can be used to pass a return token back to the host
*/
typedef unsigned long long ia_css_return_token;

STORAGE_CLASS_INLINE void
ia_css_return_token_copy(ia_css_return_token *to, const ia_css_return_token *from)
{
	/* copy a return token on VIED processor */
	int *dst = (int *)to;
	int *src = (int *)from;

	dst[0] = src[0];
	dst[1] = src[1];
}

STORAGE_CLASS_INLINE void
ia_css_return_token_zero(ia_css_return_token *to)
{
	/* zero return token on VIED processor */
	int *dst = (int *)to;

	dst[0] = 0;
	dst[1] = 0;
}

STORAGE_CLASS_INLINE void _check_return_token_size(void)
{
	CT_ASSERT(sizeof(int) == 4);
	CT_ASSERT(sizeof(ia_css_return_token) == 8);
}

#endif /* __IA_CSS_RETURN_TOKEN__ */
