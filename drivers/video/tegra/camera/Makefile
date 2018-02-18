GCOV_PROFILE := y
subdir-ccflags-y += -I../nvhost/drivers/video/tegra/host
subdir-ccflags-y += -Idrivers/video/tegra/host
subdir-ccflags-y += -Idrivers/media/platform/tegra/vi
subdir-ccflags-y += -Idrivers/video/tegra/camera

# camera*.c and camera*.h are legacy code no longer in use
ifdef CONFIG_TEGRA_CAMERA_PLATFORM
obj-y += tegra_camera_platform.o
obj-y += tegra_camera_dev_mfi.o
endif
