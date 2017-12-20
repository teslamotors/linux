ccflags-y += -DNV_IS_AVP=0
ccflags-y += -DNV_OAL=0
ccflags-y += -DNV_USE_FUSE_CLOCK_ENABLE=0
ifeq ($(CONFIG_MACH_TEGRA_GENERIC_DEBUG),y)
ccflags-y += -DNV_DEBUG=1
else
ccflags-y += -DNV_DEBUG=0
endif
ccflags-y += -I$(srctree)/compat26/tegra26/nv/include/
ccflags-y += -I$(srctree)/compat26/tegra26/nv/nvrm/core/common
ccflags-y += -I$(srctree)/compat26/tegra26/nv/nvrm/core

#obj-y += ap15rm_init.o
obj-y += ap15rm_avp_service.o
obj-y += ap15rm_xpc.o
obj-y += ap15rm_xpc_hw_private.o

include $(srctree)/compat26/Makefile.inc
ccflags-y += -include $(srctree)/compat26/include/compat26_nvrm.h
ccflags-y += -I$(srctree)/compat26/$(compat-dir-y)/drivers/video/tegra/
