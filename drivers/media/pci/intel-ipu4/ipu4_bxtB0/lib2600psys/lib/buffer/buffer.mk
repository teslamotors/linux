# 
#
# MODULE is BUFFER

ifdef _H_BUFFER_MK
$(error ERROR: buffer.mk included multiple times, please check makefile)
else
_H_BUFFER_MK=1
endif

BUFFER_DIR=$${MODULES_DIR}/buffer

BUFFER_INTERFACE=$(BUFFER_DIR)/interface
BUFFER_SOURCES_CPU=$(BUFFER_DIR)/src/cpu
BUFFER_SOURCES_CSS=$(BUFFER_DIR)/src/css

BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/ia_css_buffer.c
BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/ia_css_output_buffer.c
BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/ia_css_input_buffer.c
BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/ia_css_shared_buffer.c
BUFFER_HOST_FILES += $(BUFFER_SOURCES_CPU)/buffer_access.c
BUFFER_HOST_CPPFLAGS += -I$(BUFFER_INTERFACE)
BUFFER_HOST_CPPFLAGS += -I$${MODULES_DIR}/support

BUFFER_FW_FILES += $(BUFFER_SOURCES_CSS)/ia_css_input_buffer.c
BUFFER_FW_FILES += $(BUFFER_SOURCES_CSS)/ia_css_output_buffer.c
BUFFER_FW_FILES += $(BUFFER_SOURCES_CSS)/ia_css_shared_buffer.c
BUFFER_FW_FILES += $(BUFFER_SOURCES_CSS)/buffer_access.c

BUFFER_FW_CPPFLAGS += -I$(BUFFER_INTERFACE)
BUFFER_FW_CPPFLAGS += -I$${MODULES_DIR}/support
