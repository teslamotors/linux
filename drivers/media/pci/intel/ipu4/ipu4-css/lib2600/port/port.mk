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
# MODULE is PORT

PORT_DIR=$${MODULES_DIR}/port

PORT_INTERFACE=$(PORT_DIR)/interface
PORT_SOURCES1=$(PORT_DIR)/src

PORT_HOST_FILES += $(PORT_SOURCES1)/send_port.c
PORT_HOST_FILES += $(PORT_SOURCES1)/recv_port.c
PORT_HOST_FILES += $(PORT_SOURCES1)/queue.c

PORT_HOST_CPPFLAGS += -I$(PORT_INTERFACE)

PORT_FW_FILES += $(PORT_SOURCES1)/send_port.c
PORT_FW_FILES += $(PORT_SOURCES1)/recv_port.c

PORT_FW_CPPFLAGS += -I$(PORT_INTERFACE)
