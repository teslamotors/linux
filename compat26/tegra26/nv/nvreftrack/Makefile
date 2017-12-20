ccflags-y += -DNV_IS_AVP=0
ccflags-y += -DNV_OAL=0
ccflags-y += -DNV_USE_FUSE_CLOCK_ENABLE=0
ifeq ($(CONFIG_MACH_TEGRA_GENERIC_DEBUG),y)
ccflags-y += -DNV_DEBUG=1
else
ccflags-y += -DNV_DEBUG=0
endif

ccflags-y += -I$(srctree)/compat26/tegra26/nv/include/

obj-y += nvreftrack.o

ccflags-y += -Icompat26/include -include $(srctree)/compat26/include/compat26.h
