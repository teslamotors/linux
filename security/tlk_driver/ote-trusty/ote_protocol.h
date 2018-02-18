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
 */

#ifndef __OTE_PROTOCOL_TRUSTY_H__
#define __OTE_PROTOCOL_TRUSTY_H__

#include <linux/trusty/smcall.h>
#include "../ote_types.h"

#define TE_IOCTL_MAGIC_NUMBER ('t')
#define TE_IOCTL_OPEN_CLIENT_SESSION \
	_IOWR(TE_IOCTL_MAGIC_NUMBER, 0x10, union te_cmd)
#define TE_IOCTL_CLOSE_CLIENT_SESSION \
	_IOWR(TE_IOCTL_MAGIC_NUMBER, 0x11, union te_cmd)
#define TE_IOCTL_LAUNCH_OPERATION \
	_IOWR(TE_IOCTL_MAGIC_NUMBER, 0x14, union te_cmd)

#define TE_IOCTL_SS_NEW_REQ_LEGACY \
	_IOR(TE_IOCTL_MAGIC_NUMBER,  0x20, struct te_ss_op_legacy)
#define TE_IOCTL_SS_REQ_COMPLETE_LEGACY \
	_IOWR(TE_IOCTL_MAGIC_NUMBER, 0x21, struct te_ss_op_legacy)

/* ioctls using new SS structs (eventually to replace current SS ioctls) */
#define TE_IOCTL_SS_NEW_REQ \
	_IOR(TE_IOCTL_MAGIC_NUMBER,  0x20, struct te_ss_op)
#define TE_IOCTL_SS_REQ_COMPLETE \
	_IOWR(TE_IOCTL_MAGIC_NUMBER, 0x21, struct te_ss_op)

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

struct tlk_info;

extern struct mutex smc_lock;
extern struct tlk_device tlk_dev;
extern void tlk_fiq_glue_aarch64(void);

uint32_t send_smc(uint32_t arg0, uintptr_t arg1, uintptr_t arg2);
uint32_t _tlk_generic_smc(uint32_t arg0, uintptr_t arg1, uintptr_t arg2);
uint32_t _tlk_extended_smc(uintptr_t *args);
uint32_t tlk_generic_smc(struct tlk_info *info, uint32_t arg0,
			 uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);
uint32_t tlk_extended_smc(struct tlk_info *info, uintptr_t *args);
int tlk_device_init(struct tlk_info *info);
void tlk_irq_handler(void);
#ifdef CONFIG_TEGRA_VIRTUALIZATION
int hyp_ipa_translate(uint64_t *ipa);
#endif

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
	struct te_oper_param_page_info *param_pages;
	dma_addr_t param_pages_phys;
	size_t param_pages_size;
	size_t param_pages_tail;
	struct te_request_compat *req_addr_compat;
	struct te_oper_param_compat *param_addr_compat;
	char *req_param_buf;
	unsigned long *param_bitmap;
	struct list_head used_cmd_list;
	struct list_head free_cmd_list;
	struct tlk_info *tlk_info;
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
	unsigned long nrpages;
	struct page *pages[];
};

struct te_session {
	struct rb_node	node;
	uint32_t	session_id;
	struct list_head persist_shmem_list;
};

struct tlk_context {
	struct tlk_device *dev;
	struct rb_root sessions;
	struct list_head temp_shmem_list;
	struct list_head temp_persist_shmem_list;
	bool is_ss_daemon;
};

enum {
	TE_SMC_OPEN_SESSION		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_APP, 1),
	TE_SMC_CLOSE_SESSION		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_APP, 2),
	TE_SMC_LAUNCH_OPERATION		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_APP, 3),
	TE_SMC_NS_CB_COMPLETE		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_APP, 4),
	TE_SMC_TA_EVENT			= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_APP, 5),
	TE_SMC_ADDR_SHARE		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_APP, 6),

	TE_SMC_FC_HAS_NS_WORK		= SMC_FASTCALL_NR(SMC_ENTITY_TRUSTED_APP, 1),

	/* Trusted OS calls */
	TE_SMC_REGISTER_REQ_BUF		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 2),
	TE_SMC_REGISTER_IRQ_HANDLER	= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 4),
	TE_SMC_NS_IRQ_DONE		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 5),
	TE_SMC_INIT_LOGGER		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 7),
	TE_SMC_SS_REGISTER_HANDLER_LEGACY = SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 8),
	TE_SMC_SS_REQ_COMPLETE		= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 9),
	TE_SMC_SS_REGISTER_HANDLER	= SMC_STDCALL_NR(SMC_ENTITY_TRUSTED_OS, 10),

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
		} Mem;
	} u;
	uint64_t next_ptr_user;
};

struct te_oper_param_page_info {
	uint64_t attr;
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

#define SS_OP_MAX_DATA_SIZE	0x1000
struct te_ss_op {
	uint32_t	req_size;
	uint8_t		data[SS_OP_MAX_DATA_SIZE];
};

struct te_ss_op_legacy {
	uint8_t		data[SS_OP_MAX_DATA_SIZE];
};

enum ta_event_id {
	TA_EVENT_RESTORE_KEYS = 0,

	TA_EVENT_MASK = (1 << TA_EVENT_RESTORE_KEYS),
};

int tlk_ss_init(struct tlk_info *info);
int tlk_ss_init_legacy(struct tlk_info *info);
int te_handle_ss_ioctl_legacy(struct file *file, unsigned int ioctl_num,
		unsigned long ioctl_param);
int ote_logger_init(struct tlk_info *tlk_info);
int te_handle_ss_ioctl(struct file *file, unsigned int ioctl_num,
		unsigned long ioctl_param);
void ote_print_logs(void);
void tlk_ss_close(void);
uint32_t tlk_ss_op(void);
int ote_property_is_disabled(const char *str);

int te_fill_page_info(struct te_oper_param_page_info *pg_inf,
		unsigned long start, struct page *page,
		struct vm_area_struct *vma);

#endif //__OTE_PROTOCOL_TRUSTY_H__
