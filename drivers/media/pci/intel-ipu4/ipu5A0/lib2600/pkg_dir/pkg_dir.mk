# #
# Support for Intel Camera Imaging ISP subsystem.
# Copyright (c) 2010 - 2016, Intel Corporation.
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

