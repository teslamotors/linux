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

#ifndef __STORAGE_CLASS_H_INCLUDED__
#define __STORAGE_CLASS_H_INCLUDED__

#define STORAGE_CLASS_EXTERN \
extern

#if defined(_MSC_VER)
#define STORAGE_CLASS_INLINE \
static __inline
#elif defined(__HIVECC)
#define STORAGE_CLASS_INLINE \
static inline
#else
#define STORAGE_CLASS_INLINE \
static inline
#endif

/* Register struct */
#ifndef __register
#if defined(__HIVECC) && !defined(PIPE_GENERATION)
#define __register register
#else
#define __register
#endif
#endif

/* Memory attribute */
#ifndef MEM
#ifdef PIPE_GENERATION
#elif defined(__HIVECC)
#include <hive/attributes.h>
#else
#define MEM(any_mem)
#endif
#endif

#endif /* __STORAGE_CLASS_H_INCLUDED__ */
