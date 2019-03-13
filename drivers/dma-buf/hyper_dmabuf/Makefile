subdir-ccflags-$(CONFIG_HYPER_DMABUF) := -Werror
TARGET_MODULE:=hyper_dmabuf

# If we running by kernel building system
ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := hyper_dmabuf_drv.o \
                                 hyper_dmabuf_ioctl.o \
                                 hyper_dmabuf_list.o \
				 hyper_dmabuf_sgl_proc.o \
				 hyper_dmabuf_ops.o \
				 hyper_dmabuf_msg.o \
				 hyper_dmabuf_id.o \
				 hyper_dmabuf_remote_sync.o \
				 hyper_dmabuf_query.o \

ifeq ($(CONFIG_HYPER_DMABUF_EVENT_GEN), y)
	$(TARGET_MODULE)-objs += hyper_dmabuf_event.o
endif

ifeq ($(CONFIG_HYPER_DMABUF_XEN), y)
	$(TARGET_MODULE)-objs += xen/hyper_dmabuf_xen_comm.o \
				 xen/hyper_dmabuf_xen_comm_list.o \
				 xen/hyper_dmabuf_xen_shm.o \
				 xen/hyper_dmabuf_xen_drv.o
else ifeq ($(CONFIG_HYPER_DMABUF_ACRN), y)
	ifeq ($(CONFIG_HYPER_DMABUF_VIRTIO_BE), y)
		$(TARGET_MODULE)-objs += virtio/hyper_dmabuf_virtio_be_drv.o \
					 virtio/hyper_dmabuf_virtio_fe_list.o
	else
		$(TARGET_MODULE)-objs += virtio/hyper_dmabuf_virtio_fe_drv.o
	endif
	$(TARGET_MODULE)-objs += virtio/hyper_dmabuf_virtio_common.o \
				 virtio/hyper_dmabuf_virtio_shm.o \
				 virtio/hyper_dmabuf_virtio_comm_ring.o
endif

obj-$(CONFIG_HYPER_DMABUF) := $(TARGET_MODULE).o

# If we are running without kernel build system
else
BUILDSYSTEM_DIR?=../../../
PWD:=$(shell pwd)

all :
# run kernel build system to make module
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
# run kernel build system to cleanup in current directory
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean

load:
	insmod ./$(TARGET_MODULE).ko

unload:
	rmmod ./$(TARGET_MODULE).ko

endif
