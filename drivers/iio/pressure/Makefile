#
# Makefile for industrial I/O pressure drivers
#

GCOV_PROFILE := y

ccflags-y += -Werror
# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_HID_SENSOR_PRESS)   += hid-sensor-press.o
obj-$(CONFIG_MPL115) += mpl115.o
obj-$(CONFIG_MPL3115) += mpl3115.o
obj-$(CONFIG_NVI_BMPX80) += nvi-bmpX80.o
ifdef CONFIG_NVI_MPU
	CFLAGS_nvi_bmpX80.o += -DBMP_NVI_MPU_SUPPORT=1
endif
nvi-bmpX80-objs += nvi_bmpX80.o
obj-$(CONFIG_IIO_ST_PRESS) += st_pressure.o
st_pressure-y := st_pressure_core.o
st_pressure-$(CONFIG_IIO_BUFFER) += st_pressure_buffer.o
obj-$(CONFIG_T5403) += t5403.o

obj-$(CONFIG_IIO_ST_PRESS_I2C) += st_pressure_i2c.o
obj-$(CONFIG_IIO_ST_PRESS_SPI) += st_pressure_spi.o
