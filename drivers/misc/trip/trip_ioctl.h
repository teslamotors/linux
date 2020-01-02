#ifndef _TRIP_IOCTL_H_
#define _TRIP_IOCTL_H_

#define TRIP_IOC_MAGIC	0xf0

#define TRIP_IOCTL_VER		0x022

enum {
	TRIP_IOCTL_STATUS_OK = 0,
	TRIP_IOCTL_STATUS_ERR_TIMEOUT = 1,
	TRIP_IOCTL_STATUS_ERR_FAULT = 2,
	TRIP_IOCTL_STATUS_ERR_PARAM_ERROR = 3,
	TRIP_IOCTL_STATUS_ERR_BAD_COMMAND = 4,
	TRIP_IOCTL_STATUS_ERR_SRAM_PARITY = 5,
	TRIP_IOCTL_STATUS_ERR_DMA_CRC = 6,
	TRIP_IOCTL_STATUS_ERR_ACE_PROTO = 7,
	TRIP_IOCTL_STATUS_ERR_AXI_PROTO = 8,
	TRIP_IOCTL_STATUS_ERR_FP_INVALID_NAN = 9,
	TRIP_IOCTL_STATUS_ERR_FP_DENORM = 10,
	TRIP_IOCTL_STATUS_ERR_FP_OVERFLOW = 11,
	TRIP_IOCTL_STATUS_ERR_FP_UNDERFLOW = 12,
	TRIP_IOCTL_STATUS_ERR_HW_ASSERT = 13,
	TRIP_IOCTL_STATUS_ERR_WATCHDOG = 14,
};

#define TRIP_IOC_INFO_FW_VER_LEN     256

struct trip_ioc_info {
	uint32_t ioctl_ver;

	/* Hardware info */
	uint32_t hw_ver;
	uint32_t sram_len;
	uint32_t n_net;

	/* Info about loaded firmware */
	uint32_t fw_valid;
	uint32_t n_fw_desc;
	uint8_t fw_ver[TRIP_IOC_INFO_FW_VER_LEN];

	/* XXX - report some performance/error stats? */
};

#define TRIP_IOC_FW_TCP_VER_LEN		128

struct trip_ioc_fw_desc {
	uint32_t ioctl_ver;
	uint32_t idx;

	uint8_t tcp_ver[TRIP_IOC_FW_TCP_VER_LEN];
	uint8_t function_uuid[16];

	uint64_t data_len;
	uint64_t weight_len;
	uint64_t out_len;
};

struct trip_ioc_xfer {
	uint32_t ioctl_ver;
	uint32_t sram_addr;
	uint32_t xfer_len;
	void *buf;

	/* Spare SRAM offset used for DMA descriptors for xfer */
	uint32_t spare_net;
	uint32_t spare_sram_addr;

	uint32_t status;
};

enum trip_ioc_buffer_type {
	TRIP_IOC_BUF_NONE = 0,
	TRIP_IOC_BUF_USERPTR = 1,
	TRIP_IOC_BUF_DMABUF = 2,
};

struct trip_ioc_buffer {
	uint32_t buf_type;
	uint32_t buf_len;
	union {
		unsigned long userptr;
		int attach_fd;
	} m;
};

struct trip_ioc_start_net {
	uint32_t ioctl_ver;
	struct trip_ioc_buffer data_buf;
	struct trip_ioc_buffer weights_buf;
	struct trip_ioc_buffer output_buf;
	uint32_t sram_addr;
	uint32_t net;

	uint32_t status;
};

struct trip_ioc_dmabuf_import {
	uint32_t ioctl_ver;
	int dbuf_fd;
	uint32_t flags;

	int attach_fd;
};

/* Run this work serially with the following work. Invalid for last item. */
#define TRIP_WORK_ITEM_FLAG_SERIAL	(1 << 0)

struct trip_ioc_work_item {
	struct trip_ioc_buffer data_buf;
	struct trip_ioc_buffer weights_buf;
	struct trip_ioc_buffer output_buf;
	uint32_t sram_addr;
	uint32_t flags;
};

struct trip_ioc_work_submit {
	uint32_t ioctl_ver;
	uint32_t n_items;
	struct trip_ioc_work_item __user *items;
	uint32_t net;
	void __user *user_ctx;

	uint32_t n_submitted;
};

struct trip_ioc_work_done {
	uint32_t ioctl_ver;

	int n_items_done;
	uint32_t status;
	void __user *user_ctx;
};

enum trip_ioctl_mode_flags {
	TRIP_IOCTL_MODE_SERIALIZED = (1 << 0),
};

enum {
	TRIP_IOCTL_GET_INFO = _IOR(TRIP_IOC_MAGIC, 0, struct trip_ioc_info),
	TRIP_IOCTL_READ_SRAM = _IOWR(TRIP_IOC_MAGIC, 1, struct trip_ioc_xfer),
	TRIP_IOCTL_WRITE_SRAM = _IOWR(TRIP_IOC_MAGIC, 2, struct trip_ioc_xfer),
	TRIP_IOCTL_START_NET = _IOWR(TRIP_IOC_MAGIC, 3,
					struct trip_ioc_start_net),
	TRIP_IOCTL_DMABUF_IMPORT = _IOWR(TRIP_IOC_MAGIC, 4,
					struct trip_ioc_dmabuf_import),
	TRIP_IOCTL_GET_FW_DESC = _IOWR(TRIP_IOC_MAGIC, 5,
					struct trip_ioc_fw_desc),
	TRIP_IOCTL_WORK_SUBMIT = _IOWR(TRIP_IOC_MAGIC, 6,
					struct trip_ioc_work_submit),
	TRIP_IOCTL_WORK_DONE = _IOWR(TRIP_IOC_MAGIC, 7,
					struct trip_ioc_work_done),
	TRIP_IOCTL_ENABLE_SERIALIZED_MODE = _IO(TRIP_IOC_MAGIC, 8),
	TRIP_IOCTL_DISABLE_SERIALIZED_MODE = _IO(TRIP_IOC_MAGIC, 9),
	TRIP_IOCTL_GET_MODE_FLAGS = _IOR(TRIP_IOC_MAGIC, 10, unsigned int),
};

#endif /* _TRIP_IOCTL_H_ */

