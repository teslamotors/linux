obj-$(CONFIG_TESLA_INFO2)	+= tesla_info2.o
obj-$(CONFIG_TESLA_MODEL3)	+= tesla_model3.o

ifeq ($(CONFIG_TESLA_MODEL3_DISPLAY_REGULATOR),y)
CFLAGS_tesla_model3.o += -DM3_DISPLAY_REGULATOR
endif
