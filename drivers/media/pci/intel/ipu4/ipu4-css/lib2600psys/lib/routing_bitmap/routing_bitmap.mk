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

ifdef _H_ROUTING_BITMAP_MK
$(error ERROR: routing_bitmap.mk included multiple times, please check makefile)
else
_H_ROUTING_BITMAP_MK=1
endif

ROUTING_BITMAP_FILES += $(ROUTING_BITMAP_DIR)/src/ia_css_rbm_manifest.c

ROUTING_BITMAP_DIR = $(MODULES_DIR)/routing_bitmap
ROUTING_BITMAP_INTERFACE = $(ROUTING_BITMAP_DIR)/interface
ROUTING_BITMAP_SOURCES   = $(ROUTING_BITMAP_DIR)/src

ROUTING_BITMAP_CPPFLAGS    = -I$(ROUTING_BITMAP_INTERFACE)
ROUTING_BITMAP_CPPFLAGS   += -I$(ROUTING_BITMAP_SOURCES)

ifeq ($(ROUTING_BITMAP_INLINE),1)
ROUTING_BITMAP_CPPFLAGS   += -D__IA_CSS_RBM_INLINE__
else
ROUTING_BITMAP_FILES += $(ROUTING_BITMAP_DIR)/src/ia_css_rbm.c
endif

ifeq ($(ROUTING_BITMAP_MANIFEST_INLINE),1)
ROUTING_BITMAP_CPPFLAGS   += -D__IA_CSS_RBM_MANIFEST_INLINE__
endif
