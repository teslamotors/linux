# INTEL CONFIDENTIAL
#
# Copyright (C) 2016 - 2016 Intel Corporation.
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
# MODULE is PKG DIR

PKG_DIR_DIR  		= $${MODULES_DIR}/pkg_dir
PKG_DIR_INTERFACE	= $(PKG_DIR_DIR)/interface
PKG_DIR_SOURCES		= $(PKG_DIR_DIR)/src

PKG_DIR_FILES		= $(PKG_DIR_DIR)/src/ia_css_pkg_dir.c
PKG_DIR_CPPFLAGS	= -I$(PKG_DIR_INTERFACE)
PKG_DIR_CPPFLAGS	+= -I$(PKG_DIR_SOURCES)
PKG_DIR_CPPFLAGS	+= -I$${MODULES_DIR}/../isp/kernels/io_ls/common
PKG_DIR_CPPFLAGS	+= -I$${MODULES_DIR}/fw_abi_common_types/ipu

PKG_DIR_CREATE_FILES	= $(PKG_DIR_DIR)/src/ia_css_pkg_dir_create.c
PKG_DIR_UPDATE_FILES    = $(PKG_DIR_DIR)/src/ia_css_pkg_dir_update.c

