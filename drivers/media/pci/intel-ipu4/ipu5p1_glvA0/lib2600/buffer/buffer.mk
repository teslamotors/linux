# INTEL CONFIDENTIAL
#
# Copyright (C) 2013 - 2015 Intel Corporation.
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
# MODULE is BUFFER

ifdef _H_BUFFER_MK
$(error ERROR: buffer.mk included multiple times, please check makefile)
else
_H_BUFFER_MK=1
endif

BUFFER_DIR=$${MODULES_DIR}/buffer

BUFFER_INTERFACE=$(BUFFER_DIR)/interface
BUFFER_SOURCES_CPU=$(BUFFER_DIR)/src/cpu
BUFFER_SOURCES_CSS=$(BUFFER_DIR)/src/css

BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/ia_css_buffer.c
BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/ia_css_output_buffer.c
BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/ia_css_input_buffer.c
BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/ia_css_shared_buffer.c
BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/buffer_access.c
BUFFER_HOST_CPPFLAGS += -I$(BUFFER_INTERFACE)
BUFFER_HOST_CPPFLAGS += -I$${MODULES_DIR}/support

BUFFER_FW_FILES += $(BUFFER_SOURCES_CSS)/ia_css_input_buffer.c
BUFFER_FW_FILES += $(BUFFER_SOURCES_CSS)/ia_css_output_buffer.c
BUFFER_FW_FILES += $(BUFFER_SOURCES_CSS)/ia_css_shared_buffer.c
BUFFER_FW_FILES += $(BUFFER_SOURCES_CSS)/buffer_access.c

BUFFER_FW_CPPFLAGS += -I$(BUFFER_INTERFACE)
BUFFER_FW_CPPFLAGS += -I$${MODULES_DIR}/support
