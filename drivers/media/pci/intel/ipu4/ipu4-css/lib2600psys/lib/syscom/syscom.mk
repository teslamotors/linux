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
# MODULE is SYSCOM

SYSCOM_DIR=$${MODULES_DIR}/syscom

SYSCOM_INTERFACE=$(SYSCOM_DIR)/interface
SYSCOM_SOURCES1=$(SYSCOM_DIR)/src

SYSCOM_HOST_FILES += $(SYSCOM_SOURCES1)/ia_css_syscom.c

SYSCOM_HOST_CPPFLAGS += -I$(SYSCOM_INTERFACE)
SYSCOM_HOST_CPPFLAGS += -I$(SYSCOM_SOURCES1)
SYSCOM_HOST_CPPFLAGS += -I$${MODULES_DIR}/devices
ifdef REGMEM_SECURE_OFFSET
SYSCOM_HOST_CPPFLAGS += -DREGMEM_SECURE_OFFSET=$(REGMEM_SECURE_OFFSET)
else
SYSCOM_HOST_CPPFLAGS += -DREGMEM_SECURE_OFFSET=0
endif

SYSCOM_FW_FILES += $(SYSCOM_SOURCES1)/ia_css_syscom_fw.c

SYSCOM_FW_CPPFLAGS += -I$(SYSCOM_INTERFACE)
SYSCOM_FW_CPPFLAGS += -I$(SYSCOM_SOURCES1)
SYSCOM_FW_CPPFLAGS += -DREGMEM_OFFSET=$(REGMEM_OFFSET)
ifdef REGMEM_SECURE_OFFSET
SYSCOM_FW_CPPFLAGS += -DREGMEM_SECURE_OFFSET=$(REGMEM_SECURE_OFFSET)
else
SYSCOM_FW_CPPFLAGS += -DREGMEM_SECURE_OFFSET=0
endif
