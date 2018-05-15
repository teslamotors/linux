# # #
# Support for Intel Camera Imaging ISP subsystem.
# Copyright (c) 2010 - 2018, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details
#
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
