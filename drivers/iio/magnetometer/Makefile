#
# Makefile for industrial I/O Magnetometer sensor drivers
#

GCOV_PROFILE := y

ccflags-y += -Werror
# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_AK09911)	+= ak09911.o
obj-$(CONFIG_AK8975)	+= ak8975.o
obj-$(CONFIG_MAG3110)	+= mag3110.o
obj-$(CONFIG_NVI_AK89XX) += nvi-ak89xx.o
ifdef CONFIG_NVI_MPU
	CFLAGS_nvi_ak89xx.o += -DAKM_NVI_MPU_SUPPORT=1
endif
nvi-ak89xx-objs += nvi_ak89xx.o
obj-$(CONFIG_HID_SENSOR_MAGNETOMETER_3D) += hid-sensor-magn-3d.o

obj-$(CONFIG_IIO_ST_MAGN_3AXIS) += st_magn.o
st_magn-y := st_magn_core.o
st_magn-$(CONFIG_IIO_BUFFER) += st_magn_buffer.o

obj-$(CONFIG_IIO_ST_MAGN_I2C_3AXIS) += st_magn_i2c.o
obj-$(CONFIG_IIO_ST_MAGN_SPI_3AXIS) += st_magn_spi.o
