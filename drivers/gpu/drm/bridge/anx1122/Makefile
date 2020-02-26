ccflags-y := -Iinclude/drm

anx1122-y := anx1122_drv.o
anx1122-$(CONFIG_DEBUG_FS) += debugfs.o

obj-$(CONFIG_DRM_ANX1122) := anx1122.o
