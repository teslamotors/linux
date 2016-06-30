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

#ifndef __MISC_SUPPORT_H_INCLUDED__
#define __MISC_SUPPORT_H_INCLUDED__

/* suppress compiler warnings on unused variables */
#ifndef NOT_USED
#define NOT_USED(a) ((void)(a))
#endif

/* Calculate the  total bytes for pow(2) byte alignment */
#define tot_bytes_for_pow2_align(pow2, cur_bytes)	((cur_bytes + (pow2 - 1)) & ~(pow2 - 1))

/* Display the macro value given a string */
#define _STR(x) #x
#define STR(x) _STR(x)

/* Concatenate */
#ifndef CAT /* also defined in <hive/attributes.h> */
#define _CAT(a, b)	a ## b
#define CAT(a, b)	_CAT(a, b)
#endif

#define _CAT3(a, b, c)	a ## b ## c
#define CAT3(a, b, c)	_CAT3(a, b, c)

/* NO_HOIST, NO_CSE attributes must be ignored for host code */
#ifndef __HIVECC
#ifndef NO_HOIST
#define NO_HOIST
#endif
#ifndef NO_CSE
#define NO_CSE
#endif
#endif

/* Derive METHOD */
#if defined(C_RUN)
	#define HIVE_METHOD "crun"
#elif defined(HRT_UNSCHED)
	#define HIVE_METHOD "unsched"
#elif defined(HRT_SCHED)
	#define HIVE_METHOD "sched"
#else
	#define HIVE_METHOD "target"
	#define HRT_TARGET 1
#endif

#endif /* __MISC_SUPPORT_H_INCLUDED__ */
