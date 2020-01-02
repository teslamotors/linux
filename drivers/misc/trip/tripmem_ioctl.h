#ifndef _TRIPMEM_IOCTL_H_
#define _TRIPMEM_IOCTL_H_

#define TRIPMEM_IOC_MAGIC	0xf1

#define TRIPMEM_IOCTL_VER		0x022

struct tripmem_ioc_resv_mem {
	uint32_t ioctl_ver;
	uint64_t resv_size;
};

struct tripmem_ioc_dmabuf_export {
	uint32_t ioctl_ver;
	uint64_t buf_ofs;
	uint64_t buf_len;
	uint32_t flags;	/* valid mask: O_CLOEXEC | O_ACCMODE */

	int fd;
};

struct tripmem_ioc_get_info {
	uint32_t ioctl_ver;
	uint64_t resv_size;
};

struct tripmem_ioc_get_flags {
	uint32_t ioctl_ver;
	uint32_t flags;
};

#define TRIPMEM_FLAG_HAS_ECC	(1 << 0)

enum {
	TRIPMEM_IOCTL_RESERVE_MEM = _IOW(TRIPMEM_IOC_MAGIC, 4,
					struct tripmem_ioc_resv_mem),
	TRIPMEM_IOCTL_DMABUF_EXPORT = _IOWR(TRIPMEM_IOC_MAGIC, 5,
					struct tripmem_ioc_dmabuf_export),
	TRIPMEM_IOCTL_GET_INFO = _IOR(TRIPMEM_IOC_MAGIC, 6,
					struct tripmem_ioc_get_info),
	TRIPMEM_IOCTL_GET_FLAGS = _IOR(TRIPMEM_IOC_MAGIC, 7,
					struct tripmem_ioc_get_flags),
};

#endif /* _TRIPMEM_IOCTL_H_ */

