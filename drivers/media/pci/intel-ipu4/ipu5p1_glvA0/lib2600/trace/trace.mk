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
# MODULE Trace

# Dependencies
IA_CSS_TRACE_SUPPORT = $${MODULES_DIR}/support

# API
IA_CSS_TRACE = $${MODULES_DIR}/trace
IA_CSS_TRACE_INTERFACE = $(IA_CSS_TRACE)/interface

#
# Host
#

# Host CPP Flags
IA_CSS_TRACE_HOST_CPPFLAGS += -I$(IA_CSS_TRACE_SUPPORT)
IA_CSS_TRACE_HOST_CPPFLAGS += -I$(IA_CSS_TRACE_INTERFACE)
IA_CSS_TRACE_HOST_CPPFLAGS += -I$(IA_CSS_TRACE)/trace_modules

#
# Firmware
#

# Firmware CPP Flags
IA_CSS_TRACE_FW_CPPFLAGS += -I$(IA_CSS_TRACE_SUPPORT)
IA_CSS_TRACE_FW_CPPFLAGS += -I$(IA_CSS_TRACE_INTERFACE)
IA_CSS_TRACE_FW_CPPFLAGS += -I$(IA_CSS_TRACE)/trace_modules
