# INTEL CONFIDENTIAL
#
# Copyright (C) 2013 - 2016 Intel Corporation.
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
# MODULE is ISYSAPI

ISYSAPI_DIR=$${MODULES_DIR}/isysapi

ISYSAPI_INTERFACE=$(ISYSAPI_DIR)/interface
ISYSAPI_SOURCES=$(ISYSAPI_DIR)/src
ISYSAPI_EXTINCLUDE=$${MODULES_DIR}/support
ISYSAPI_EXTINTERFACE=$${MODULES_DIR}/syscom/interface

ISYSAPI_HOST_FILES += $(ISYSAPI_SOURCES)/ia_css_isys_public.c

ISYSAPI_HOST_FILES += $(ISYSAPI_SOURCES)/ia_css_isys_private.c

# ISYSAPI Trace Log Level = ISYSAPI_TRACE_LOG_LEVEL_NORMAL
# Other options are [ISYSAPI_TRACE_LOG_LEVEL_OFF, ISYSAPI_TRACE_LOG_LEVEL_DEBUG]
ifndef ISYSAPI_TRACE_CONFIG_HOST
	ISYSAPI_TRACE_CONFIG_HOST=ISYSAPI_TRACE_LOG_LEVEL_NORMAL
endif
ifndef ISYSAPI_TRACE_CONFIG_FW
	ISYSAPI_TRACE_CONFIG_FW=ISYSAPI_TRACE_LOG_LEVEL_NORMAL
endif

ISYSAPI_HOST_CPPFLAGS += -DISYSAPI_TRACE_CONFIG=$(ISYSAPI_TRACE_CONFIG_HOST)
ISYSAPI_FW_CPPFLAGS += -DISYSAPI_TRACE_CONFIG=$(ISYSAPI_TRACE_CONFIG_FW)

ISYSAPI_HOST_FILES += $(ISYSAPI_SOURCES)/ia_css_isys_public_trace.c

ISYSAPI_HOST_CPPFLAGS += -I$(ISYSAPI_INTERFACE)
ISYSAPI_HOST_CPPFLAGS += -I$(ISYSAPI_EXTINCLUDE)
ISYSAPI_HOST_CPPFLAGS += -I$(ISYSAPI_EXTINTERFACE)

ISYSAPI_FW_FILES += $(ISYSAPI_SOURCES)/isys_fw.c
ISYSAPI_FW_FILES += $(ISYSAPI_SOURCES)/isys_fw_utils.c

ISYSAPI_FW_CPPFLAGS += -I$(ISYSAPI_INTERFACE)
ISYSAPI_FW_CPPFLAGS += -I$(ISYSAPI_SOURCES)/$(IPU_SYSVER)
ISYSAPI_FW_CPPFLAGS += -I$(ISYSAPI_EXTINCLUDE)
ISYSAPI_FW_CPPFLAGS += -I$(ISYSAPI_EXTINTERFACE)
