EXTRA_CFLAGS += -I$(srctree)/../display
EXTRA_CFLAGS += -I$(srctree)

obj-$(CONFIG_VGASTATE)            += vgastate.o
obj-$(CONFIG_HDMI)                += hdmi.o

obj-$(CONFIG_ADF)		  += adf/
obj-$(CONFIG_VT)		  += console/
obj-$(CONFIG_LOGO)		  += logo/
obj-y				  += backlight/

obj-y				  += fbdev/
obj-$(CONFIG_DS90UH925Q_SER)      += ds90uh925q_ser.o
obj-$(CONFIG_DS90UB947)           += ds90ub947-q1.o
obj-y                             += tegra/

obj-$(CONFIG_VIDEOMODE_HELPERS) += display_timing.o videomode.o
ifeq ($(CONFIG_OF),y)
obj-$(CONFIG_VIDEOMODE_HELPERS) += of_display_timing.o of_videomode.o
endif
