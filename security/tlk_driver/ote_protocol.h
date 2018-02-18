/*
 * Copyright (c) 2013-2016 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __OTE_PROTOCOL_H__
#define __OTE_PROTOCOL_H__

#include "ote_types.h"

#define TE_IOCTL_MAGIC_NUMBER ('t')
#define TE_IOCTL_OPEN_CLIENT_SESSION \
	_IOWR(TE_IOCTL_MAGIC_NUMBER, 0x10, union te_cmd)
#define TE_IOCTL_CLOSE_CLIENT_SESSION \
	_IOWR(TE_IOCTL_MAGIC_NUMBER, 0x11, union te_cmd)
#define TE_IOCTL_LAUNCH_OPERATION \
	_IOWR(TE_IOCTL_MAGIC_NUMBER, 0x14, union te_cmd)

/* secure storage ioctl */
#define TE_IOCTL_SS_CMD \
	_IOR(TE_IOCTL_MAGIC_NUMBER,  0x30, int)

#define TE_IOCTL_SS_CMD_GET_NEW_REQ	1
#define TE_IOCTL_SS_CMD_REQ_COMPLETE	2

#define TE_IOCTL_MIN_NR	_IOC_NR(TE_IOCTL_OPEN_CLIENT_SESSION)
#define TE_IOCTL_MAX_NR	_IOC_NR(TE_IOCTL_SS_CMD)

/* shared buffer is 2 pages: 1st are requests, 2nd are params */
#define TE_CMD_DESC_MAX	(PAGE_SIZE / sizeof(struct te_request))
#define TE_PARAM_MAX	(PAGE_SIZE / sizeof(struct te_oper_param))

#define MAX_EXT_SMC_ARGS	12

extern struct mutex smc_lock;
extern struct tlk_device tlk_dev;
extern void tlk_fiq_glue_aarch64(void);

uint32_t send_smc(uint32_t arg0, uintptr_t arg1, uintptr_t arg2);
uint32_t _tlk_generic_smc(uint32_t arg0, uintptr_t arg1, uintptr_t arg2);
void tlk_irq_handler(void);
struct te_oper_param *te_get_free_params(struct tlk_device *dev,
	unsigned int nparams);
void te_put_free_params(struct tlk_device *dev,
	struct te_oper_param *params, uint32_t nparams);
struct te_cmd_req_desc *te_get_free_cmd_desc(struct tlk_device *dev);
void te_put_used_cmd_desc(struct tlk_device *dev,
	struct te_cmd_req_desc *cmd_desc);

/* errors returned by secure world in reponse to SMC calls */
enum {
	TE_ERROR_PREEMPT_BY_IRQ = 0xFFFFFFFD,
	TE_ERROR_PREEMPT_BY_FS = 0xFFFFFFFE,
};

struct tlk_device {
	struct te_request *req_addr;
	dma_addr_t req_addr_phys;
	struct te_oper_param *param_addr;
	dma_addr_t param_addr_phys;

	char *req_param_buf;

	unsigned long *param_bitmap;

	struct list_head used_cmd_list;
	struct list_head free_cmd_list;
};

struct te_cmd_req_desc {
	struct te_request *req_addr;
	struct list_head list;
};

struct te_shmem_desc {
	struct list_head list;
	uint32_t type;
	void *buffer;
	size_t size;
	struct page **pages;
	unsigned int nr_pages;
	bool is_locked;
};

/*
 * Per-session data structure.
 *
 * Both temp (freed upon completion of the associated op) and persist
 * (freed upon session close) memory references are tracked by this
 * structure.
 *
 * Persistent memory references stay on an inactive list until the
 * associated op completes.  If it completes successfully then the
 * references are moved to the active list.
 */
struct te_session {
	struct list_head list;
	uint32_t session_id;
	struct list_head temp_shmem_list;
	struct list_head inactive_persist_shmem_list;
	struct list_head persist_shmem_list;
};

struct tlk_context {
	struct tlk_device *dev;
	struct list_head session_list;
};

enum {
	/* Trusted Application Calls */
	TE_SMC_OPEN_SESSION		= 0x70000001,
	TE_SMC_CLOSE_SESSION		= 0x70000002,
	TE_SMC_LAUNCH_OPERATION		= 0x70000003,
	TE_SMC_TA_EVENT			= 0x70000004,

	/* Trusted OS (64-bit) calls */
	TE_SMC_REGISTER_REQ_BUF		= 0x72000001,
	TE_SMC_INIT_LOGGER		= 0x72000002,
	TE_SMC_RESTART			= 0x72000100,

	/* SIP (SOC specific) calls.  */
	TE_SMC_REGISTER_FIQ_GLUE	= 0x82000005,
	TE_SMC_VRR_SET_BUF		= 0x82000011,
	TE_SMC_VRR_SEC			= 0x82000012,
};

enum {
	TE_PARAM_TYPE_NONE		= 0x0,
	TE_PARAM_TYPE_INT_RO		= 0x1,
	TE_PARAM_TYPE_INT_RW		= 0x2,
	TE_PARAM_TYPE_MEM_RO		= 0x3,
	TE_PARAM_TYPE_MEM_RW		= 0x4,
	TE_PARAM_TYPE_PERSIST_MEM_RO	= 0x100,
	TE_PARAM_TYPE_PERSIST_MEM_RW	= 0x101,
};

enum {
	TE_MEM_TYPE_NS_USER	= 0x0,
	TE_MEM_TYPE_NS_KERNEL = 0x1,
};

struct te_oper_param {
	uint32_t index;
	uint32_t type;
	union {
		struct {
			uint32_t val;
		} Int;
		struct {
			uint64_t base;
			uint32_t len;
			uint32_t type;
		} Mem;
	} u;
	uint64_t next_ptr_user;
};

struct te_service_id {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_version;
	uint8_t clock_seq_and_node[8];
};

struct te_operation {
	uint32_t	command;
	uint32_t	status;
	uint64_t	list_head;
	uint64_t	list_tail;
	uint32_t	list_count;
	uint32_t	interface_side;
};

/*
 * OpenSession
 */
struct te_opensession {
	struct te_service_id	dest_uuid;
	struct te_operation	operation;
	uint64_t		answer;
};

/*
 * CloseSession
 */
struct te_closesession {
	uint32_t	session_id;
	uint64_t	answer;
};

/*
 * LaunchOperation
 */
struct te_launchop {
	uint32_t		session_id;
	struct te_operation	operation;
	uint64_t		answer;
};

union te_cmd {
	struct te_opensession	opensession;
	struct te_closesession	closesession;
	struct te_launchop	launchop;
};

struct te_request {
	uint32_t		type;
	uint32_t		session_id;
	uint32_t		command_id;
	uint64_t		params;
	uint32_t		params_size;
	uint32_t		dest_uuid[4];
	uint32_t		result;
	uint32_t		result_origin;
};

struct te_answer {
	uint32_t	result;
	uint32_t	session_id;
	uint32_t	result_origin;
};

void te_open_session(struct te_opensession *cmd,
	struct te_request *request,
	struct tlk_context *context);

void te_close_session(struct te_closesession *cmd,
	struct te_request *request,
	struct tlk_context *context);

void te_launch_operation(struct te_launchop *cmd,
	struct te_request *request,
	struct tlk_context *context);

enum ta_event_id {
	TA_EVENT_RESTORE_KEYS = 0,

	TA_EVENT_MASK = (1 << TA_EVENT_RESTORE_KEYS),
};

int te_handle_ss_ioctl(struct file *file, unsigned int ioctl_num,
		unsigned long ioctl_param);
void ote_print_logs(void);
int tlk_ss_op(void);
int ote_property_is_disabled(const char *str);

#endif
