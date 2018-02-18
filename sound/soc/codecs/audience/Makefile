snd-soc-es705-escore-objs := es705_escore.o escore-pm.o escore.o escore-list.o
snd-soc-es755-objs := es755.o escore.o escore-pm.o escore-list.o es-d300.o es-a300.o es-d300-route.o
snd-soc-es854-objs := es755.o escore.o escore-pm.o escore-list.o es-d300.o es-a350.o es-d300-route.o
snd-soc-es857-objs := es755.o escore.o escore-pm.o escore-list.o es-d300.o es-a375.o es-d300-route.o
snd-soc-escore-slim-objs += escore-slim.o
snd-soc-escore-i2s-objs += escore-i2s.o
snd-soc-escore-i2c-objs += escore-i2c.o
snd-soc-escore-spi-objs += escore-spi.o
snd-soc-escore-cdev-objs += escore-cdev.o
snd-soc-escore-uart-objs += escore-uart.o escore-uart-common.o
snd-soc-escore-vs-objs += escore-vs.o
snd-soc-es-spi-sensor-hub-objs := es705-sensorhub-demo.o
snd-soc-esn100-objs := es-n100.o
escore-only-objs := escore.o escore-pm.o escore-list.o

obj-$(CONFIG_SND_SOC_ES705_ESCORE)	+= snd-soc-es705-escore.o
obj-$(CONFIG_SND_SOC_ES804_ESCORE)	+= snd-soc-es705-escore.o
obj-$(CONFIG_SND_SOC_ES704_ESCORE)	+= snd-soc-es705-escore.o
obj-$(CONFIG_SND_SOC_ES325)	+= snd-soc-es325.o
obj-$(CONFIG_SND_SOC_ES515)	+= snd-soc-es515.o
obj-$(CONFIG_SND_SOC_ES755)	+= snd-soc-es755.o
obj-$(CONFIG_SND_SOC_ES854)	+= snd-soc-es854.o
obj-$(CONFIG_SND_SOC_N100)	+= snd-soc-esn100.o
obj-$(CONFIG_SND_SOC_ES857)	+= snd-soc-es857.o
obj-$(CONFIG_OSP_SENSORHUB)		+= escore-only.o

obj-$(CONFIG_SND_SOC_ES_SLIM)	+= snd-soc-escore-slim.o
ifneq ($(CONFIG_SND_SOC_ES_SLIM),y)
ifeq ($(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SLIM),y)
	obj-y += snd-soc-escore-slim.o
endif
endif

obj-$(CONFIG_SND_SOC_ES_I2S)	+= snd-soc-escore-i2s.o
obj-$(CONFIG_SND_SOC_ES_I2C)	+= snd-soc-escore-i2c.o
ifneq ($(CONFIG_SND_SOC_ES_I2C),y)
ifeq ($(CONFIG_SND_SOC_ES_HIGH_BW_BUS_I2C),y)
	obj-y += snd-soc-escore-i2c.o
endif
endif

obj-$(CONFIG_SND_SOC_ES_SPI)	+= snd-soc-escore-spi.o
ifneq ($(CONFIG_SND_SOC_ES_SPI),y)
ifeq ($(CONFIG_SND_SOC_ES_HIGH_BW_BUS_SPI),y)
	obj-y += snd-soc-escore-spi.o
endif
endif

obj-$(CONFIG_SND_SOC_ES_UART)	+= snd-soc-escore-uart.o
obj-$(CONFIG_SND_SOC_ES_WAKEUP_UART)	+= snd-soc-escore-uart.o
ifneq ($(CONFIG_SND_SOC_ES_UART),y)
ifeq ($(CONFIG_SND_SOC_ES_HIGH_BW_BUS_UART),y)
	obj-y += snd-soc-escore-uart.o
endif
endif

obj-$(CONFIG_SND_SOC_ES_CDEV)	+= snd-soc-escore-cdev.o
obj-$(CONFIG_SND_SOC_ES_VS)	+= snd-soc-escore-vs.o
obj-$(CONFIG_SND_SOC_ES_SPI_SENSOR_HUB)	+= snd-soc-es-spi-sensor-hub.o

