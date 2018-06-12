##
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
##

# MODULE is cpd/cpd_component

CPD_DIR				= $${MODULES_DIR}/cpd
CPD_COMPONENT_DIR		= $${MODULES_DIR}/cpd/cpd_component
CPD_COMPONENT_INTERFACE		= $(CPD_COMPONENT_DIR)/interface
CPD_COMPONENT_SOURCES		= $(CPD_COMPONENT_DIR)/src

CPD_COMPONENT_FILES		= $(CPD_COMPONENT_SOURCES)/ia_css_cpd_component_create.c
CPD_COMPONENT_FILES		+= $(CPD_COMPONENT_SOURCES)/ia_css_cpd_component.c
CPD_COMPONENT_CPPFLAGS		= -I$(CPD_COMPONENT_INTERFACE)
CPD_COMPONENT_CPPFLAGS		+= -I$(CPD_COMPONENT_SOURCES)
CPD_COMPONENT_CPPFLAGS		+= -I$(CPD_DIR)
