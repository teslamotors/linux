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

# PSYS_PRIVATE_PG
#

PSYS_PRIVATE_PG_DIR = $${MODULES_DIR}/psys_private_pg

PSYS_PRIVATE_PG_INTERFACE = $(PSYS_PRIVATE_PG_DIR)/interface
PSYS_PRIVATE_PG_SOURCES = $(PSYS_PRIVATE_PG_DIR)/src

PSYS_PRIVATE_PG_CPPFLAGS := \
		-I$(PSYS_PRIVATE_PG_INTERFACE)

PSYS_PRIVATE_PG_HOST_FILES = \
	$(PSYS_PRIVATE_PG_SOURCES)/ia_css_psys_private_pg_data_create.c

