
# Kernel makefile for motions sensors
#
# 

# MPU
obj-$(CONFIG_SENSORS_MPU3050)	+= mpu3050.o
mpu3050-objs += mpuirq.o \
	mpu-dev.o \
	mpu-i2c.o \
	mlsl-kernel.o \
	mlos-kernel.o \
	$(MLLITE_DIR)mldl_cfg.o

#
# Accel options
#
ifdef CONFIG_SENSORS_KXTF9_MPU
mpu3050-objs += $(MLLITE_DIR)accel/kxtf9.o
endif

#
# Compass options
#
ifdef CONFIG_SENSORS_AK8975_MPU
mpu3050-objs += $(MLLITE_DIR)compass/ak8975.o
endif

EXTRA_CFLAGS += -I$(M)/$(MLLITE_DIR) \
                -I$(M)/../../include \
				-Idrivers/misc/mpu3050 \
                -Iinclude/linux

ifdef CONFIG_SENSORS_MPU3050_DEBUG
EXTRA_CFLAGS += -DDEBUG
endif

