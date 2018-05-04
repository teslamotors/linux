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


# MODULE is CPD UTL (Metadata File Extension)

CPD_DIR  		= $${MODULES_DIR}/cpd/
CPD_METADATA_DIR	= $${MODULES_DIR}/cpd/cpd_metadata
CPD_METADATA_INTERFACE	= $(CPD_METADATA_DIR)/interface
CPD_METADATA_SOURCES	= $(CPD_METADATA_DIR)/src

CPD_METADATA_FILES	= $(CPD_METADATA_SOURCES)/ia_css_cpd_metadata_create.c
CPD_METADATA_FILES	+= $(CPD_METADATA_SOURCES)/ia_css_cpd_metadata.c
CPD_METADATA_CPPFLAGS	= -I$(CPD_METADATA_INTERFACE) \
			  -I$(CPD_METADATA_SOURCES) \
			  -I$(CPD_DIR)
