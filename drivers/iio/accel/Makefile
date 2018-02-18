#
# Makefile for industrial I/O accelerometer drivers
#

# When adding new entries keep the list in alphabetical order
obj-$(CONFIG_BMA180) += bma180.o
obj-$(CONFIG_BMC150_ACCEL) += bmc150-accel-core.o
obj-$(CONFIG_BMC150_ACCEL_I2C) += bmc150-accel-i2c.o
obj-$(CONFIG_BMC150_ACCEL_SPI) += bmc150-accel-spi.o
obj-$(CONFIG_HID_SENSOR_ACCEL_3D) += hid-sensor-accel-3d.o
obj-$(CONFIG_SMI130_ACCEL) += smi130-accel-core.o
obj-$(CONFIG_SMI130_ACCEL_SPI) += smi130-accel-spi.o
obj-$(CONFIG_KXCJK1013) += kxcjk-1013.o
obj-$(CONFIG_KXSD9)	+= kxsd9.o
obj-$(CONFIG_MMA8452)	+= mma8452.o

obj-$(CONFIG_IIO_ST_ACCEL_3AXIS) += st_accel.o
st_accel-y := st_accel_core.o
st_accel-$(CONFIG_IIO_BUFFER) += st_accel_buffer.o

obj-$(CONFIG_IIO_ST_ACCEL_I2C_3AXIS) += st_accel_i2c.o
obj-$(CONFIG_IIO_ST_ACCEL_SPI_3AXIS) += st_accel_spi.o

obj-$(CONFIG_NVS_AIS328DQ) += nvs_ais328dq.o
