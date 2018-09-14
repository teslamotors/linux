/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0) */
/*
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef __IPU4_VIRTIO_COMMON_H__
#define __IPU4_VIRTIO_COMMON_H__


/*
 * CWP uses physicall addresses for memory sharing,
 * so size of one page ref will be 64-bits
 */

#define REFS_PER_PAGE (PAGE_SIZE/sizeof(u64))

/* Defines size of requests circular buffer */
#define REQ_RING_SIZE 128

#define MAX_NUMBER_OF_OPERANDS 64

#define MAX_ENTRY_FE 7

enum virio_queue_type {
      IPU_VIRTIO_QUEUE_0 = 0,
      IPU_VIRTIO_QUEUE_1,
      IPU_VIRTIO_QUEUE_MAX
};

struct ipu4_virtio_req {
	unsigned int req_id;
	unsigned int stat;
	unsigned int cmd;
	unsigned int func_ret;
	unsigned int op[MAX_NUMBER_OF_OPERANDS];
	u64 payload;
};
struct test_payload {
	unsigned int data1;
	long int data2;
	char name[256];
};
/*Not used*/
struct ipu4_virtio_resp {
	unsigned int resp_id;
	unsigned int stat;
	unsigned int cmd;
	unsigned int op[MAX_NUMBER_OF_OPERANDS];
};

/*Not used*/
struct ipu4_virtio_fe_info {
	struct ipu4_virtio_be_priv *priv;
	int client_id;
	int vmid;
	int max_vcpu;
	struct vhm_request *req_buf;
};

/*Not used*/
struct ipu4_virtio_fe_info_entry {
	struct ipu4_virtio_fe_info *info;
	struct hlist_node node;
};

struct ipu4_bknd_ops {
	/* backed initialization routine */
	int (*init)(void);

	/* backed cleanup routine */
	void (*cleanup)(void);

	/* retreiving id of current virtual machine */
	int (*get_vm_id)(void);

	int (*send_req)(int, struct ipu4_virtio_req *, int, int);
};

struct ipu4_virtio_ctx {
	/* VM(domain) id of current VM instance */
	int domid;

	/* backend ops - hypervisor specific */
	struct ipu4_bknd_ops *bknd_ops;

	/* flag that shows whether backend is initialized */
	bool initialized;

	/* device global lock */
	struct mutex lock;
};

enum intel_ipu4_virtio_command {
	IPU4_CMD_DEVICE_OPEN = 0x1,
	IPU4_CMD_DEVICE_CLOSE,
	IPU4_CMD_STREAM_ON,
	IPU4_CMD_STREAM_OFF,
	IPU4_CMD_GET_BUF,
	IPU4_CMD_PUT_BUF,
	IPU4_CMD_SET_FORMAT,
	IPU4_CMD_ENUM_NODES,
	IPU4_CMD_ENUM_LINKS,
	IPU4_CMD_SETUP_PIPE,
	IPU4_CMD_SET_FRAMEFMT,
	IPU4_CMD_GET_FRAMEFMT,
	IPU4_CMD_GET_SUPPORTED_FRAMEFMT,
	IPU4_CMD_SET_SELECTION,
	IPU4_CMD_GET_SELECTION,
	IPU4_CMD_POLL,
	IPU4_CMD_PIPELINE_OPEN,
	IPU4_CMD_PIPELINE_CLOSE,
	IPU4_CMD_GET_N
};

enum intel_ipu4_virtio_req_feedback {
	IPU4_REQ_PROCESSED,
	IPU4_REQ_NEEDS_FOLLOW_UP,
	IPU4_REQ_ERROR,
	IPU4_REQ_NOT_RESPONDED
};
extern struct ipu4_bknd_ops ipu4_virtio_bknd_ops;

void ipu4_virtio_fe_table_init(void);

int ipu4_virtio_fe_add(struct ipu4_virtio_fe_info *fe_info);

int ipu4_virtio_remove_fe(int client_id);

struct ipu4_virtio_fe_info *ipu4_virtio_fe_find(int client_id);

struct ipu4_virtio_fe_info *ipu4_virtio_fe_find_by_vmid(int vmid);

#endif
