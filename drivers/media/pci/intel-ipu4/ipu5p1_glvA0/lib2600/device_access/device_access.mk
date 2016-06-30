# INTEL CONFIDENTIAL
#
# Copyright (C) 2015 - 2016 Intel Corporation.
# All Rights Reserved.
#
# The source code contained or described herein and all documents
# related to the source code ("Material") are owned by Intel Corporation
# or licensors. Title to the Material remains with Intel
# Corporation or its licensors. The Material contains trade
# secrets and proprietary and confidential information of Intel or its
# licensors. The Material is protected by worldwide copyright
# and trade secret laws and treaty provisions. No part of the Material may
# be used, copied, reproduced, modified, published, uploaded, posted,
# transmitted, distributed, or disclosed in any way without Intel's prior
# express written permission.
#
# No License under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or
# delivery of the Materials, either expressly, by implication, inducement,
# estoppel or otherwise. Any license under such intellectual property rights
# must be express and approved by Intel in writing.
#

ifndef _DEVICE_ACCESS_MK_
_DEVICE_ACCESS_MK_ = 1

DEVICE_ACCESS_DIR=$${MODULES_DIR}/device_access
DEVICE_ACCESS_INTERFACE=$(DEVICE_ACCESS_DIR)/interface
DEVICE_ACCESS_SOURCES=$(DEVICE_ACCESS_DIR)/src

DEVICE_ACCESS_HOST_FILES =

DEVICE_ACCESS_FW_FILES =

DEVICE_ACCESS_HOST_CPPFLAGS = \
	-I$(DEVICE_ACCESS_INTERFACE) \
	-I$(DEVICE_ACCESS_SOURCES)

DEVICE_ACCESS_FW_CPPFLAGS = \
	-I$(DEVICE_ACCESS_INTERFACE) \
	-I$(DEVICE_ACCESS_SOURCES)

ifeq "$(IPU_SYSVER)" "cnlA0"
	DEVICE_ACCESS_VERSION=v2
endif
ifeq "$(IPU_SYSVER)" "cnlB0"
	DEVICE_ACCESS_VERSION=v2
endif
ifeq "$(IPU_SYSVER)" "glvA0"
	DEVICE_ACCESS_VERSION=v3
endif
ifeq "$(IPU_SYSVER)" "bxtA0"
	DEVICE_ACCESS_VERSION=v1
endif
ifeq "$(IPU_SYSVER)" "bxtB0"
	DEVICE_ACCESS_VERSION=v2
endif

DEVICE_ACCESS_FW_CPPFLAGS += \
		-I$(DEVICE_ACCESS_SOURCES)/$(DEVICE_ACCESS_VERSION)
endif
