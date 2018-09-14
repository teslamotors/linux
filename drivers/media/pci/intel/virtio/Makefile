ifneq ($(EXTERNAL_BUILD), 1)
srcpath := $(srctree)
endif

IPU_STEP = bxtB0

include $(srcpath)/$(src)/Makefile.virt

ccflags-y += -I$(srcpath)/$(src)/../../../../../include/
ccflags-y += -I$(srcpath)/$(src)/../
