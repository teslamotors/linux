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

# MODULE is FW ABI COMMON TYPES

FW_ABI_COMMON_TYPES_DIRS = -I$${MODULES_DIR}/fw_abi_common_types
FW_ABI_COMMON_TYPES_DIRS += -I$${MODULES_DIR}/fw_abi_common_types/cpu

FW_ABI_COMMON_TYPES_HOST_FILES =
FW_ABI_COMMON_TYPES_HOST_CPPFLAGS = $(FW_ABI_COMMON_TYPES_DIRS)

FW_ABI_COMMON_TYPES_FW_FILES =
FW_ABI_COMMON_TYPES_FW_CPPFLAGS = $(FW_ABI_COMMON_TYPES_DIRS)
