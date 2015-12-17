#
#  Copyright (c) 2010 - 2015, Intel Corporation.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms and conditions of the GNU General Public License,
#  version 2, as published by the Free Software Foundation.
#
#  This program is distributed in the hope it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
#  more details.
#

##
# 
##

# MODULE is SERVER_INIT

SERVER_INIT_DIR=$${MODULES_DIR}/server_init
SERVER_INIT_INTERFACE=$(SERVER_INIT_DIR)/interface
SERVER_INIT_SOURCES=$(SERVER_INIT_DIR)/src

SERVER_INIT_FW_FILES  = $(SERVER_INIT_SOURCES)/ia_css_server_init_cell.c
SERVER_INIT_FW_FILES += $(SERVER_INIT_SOURCES)/ia_css_server_offset.c
SERVER_INIT_FW_CPPFLAGS = -I$(SERVER_INIT_INTERFACE)

SERVER_INIT_HOST_FILES  = $(SERVER_INIT_SOURCES)/ia_css_server_init_host.c
SERVER_INIT_HOST_FILES += $(SERVER_INIT_SOURCES)/ia_css_server_offset.c
SERVER_INIT_HOST_CPPFLAGS = -I$(SERVER_INIT_INTERFACE)

