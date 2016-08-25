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
CLIENT_PKG_DIR  	= $${MODULES_DIR}/client_pkg
CLIENT_PKG_INTERFACE = $(CLIENT_PKG_DIR)/interface
CLIENT_PKG_SOURCES	= $(CLIENT_PKG_DIR)/src

CLIENT_PKG_FILES	= $(CLIENT_PKG_DIR)/src/ia_css_client_pkg.c
CLIENT_PKG_CPPFLAGS	= -I$(CLIENT_PKG_INTERFACE)
CLIENT_PKG_CPPFLAGS	+= -I$(CLIENT_PKG_SOURCES)

CLIENT_PKG_CREATE_FILES = $(CLIENT_PKG_DIR)/src/ia_css_client_pkg_create.c

CLIENT_PKG_GEN_FILES = $(CLIENT_PKG_DIR)/src/ia_css_client_pkg_gen.c
CLIENT_PKG_GEN_FILES += $(CLIENT_PKG_DIR)/src/ia_css_client_pkg_create.c

