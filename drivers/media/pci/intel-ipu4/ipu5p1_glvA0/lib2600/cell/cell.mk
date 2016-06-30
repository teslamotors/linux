# INTEL CONFIDENTIAL
#
# Copyright (C) 2014 - 2015 Intel Corporation.
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
ifndef _CELL_MK_
_CELL_MK_ = 1


CELL_DIR=$${MODULES_DIR}/cell
CELL_INTERFACE=$(CELL_DIR)/interface
CELL_SOURCES=$(CELL_DIR)/src

CELL_HOST_FILES =
CELL_FW_FILES =

CELL_HOST_CPPFLAGS = \
	-I$(CELL_INTERFACE) \
	-I$(CELL_SOURCES)

CELL_FW_CPPFLAGS = \
	-I$(CELL_INTERFACE) \
	-I$(CELL_SOURCES)

ifdef 0
# Disabled until it is decided to go this way or not
include $(MODULES_DIR)/device_access/device_access.mk
CELL_HOST_FILES += $(DEVICE_ACCESS_HOST_FILES)
CELL_FW_FILES += $(DEVICE_ACCESS_FW_FILES)
CELL_HOST_CPPFLAGS += $(DEVICE_ACCESS_HOST_CPPFLAGS)
CELL_FW_CPPFLAGS += $(DEVICE_ACCESS_FW_CPPFLAGS)
endif

endif
