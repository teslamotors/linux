/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_CAMRTC_TRACE_H
#define INCLUDE_CAMRTC_TRACE_H

#include "camrtc-common.h"

/*
 * Trace memory consists of three part.
 *
 * 1. Trace memory header: This describes the layout of trace memory,
 * and latest activities.
 *
 * 2. Exception memory: This is an array of exception entries. Each
 * entry describes an exception occurred in the firmware.
 *
 * 3. Event memory: This is an array of event entries. This is implemented
 * as a ring buffer.
 *
 * The next index gets updated when new messages are committed to the
 * trace memory. The next index points to the entry to be written to
 * at next occurrence of the exception or event.
 *
 * Trace memory layout
 *
 * 0x00000 +-------------------------------+
 *         |      Trace Memory Header      |
 * 0x01000 +-------------------------------+
 *         |                               |
 *         |        Exception Memory       | <- exception_next_idx
 *         |                               |
 * 0x10000 +-------------------------------+
 *         |                               |
 *         |                               |
 *         |          Event Memory         |
 *         |                               | <- event_next_idx
 *         |                               |
 *         +-------------------------------+
 */

/* Offset of each memory */
#define CAMRTC_TRACE_NEXT_IDX_SIZE	64U
#define CAMRTC_TRACE_EXCEPTION_OFFSET	0x01000U
#define CAMRTC_TRACE_EVENT_OFFSET	0x10000U

/* Size of each entry */
#define CAMRTC_TRACE_EXCEPTION_SIZE	1024U
#define CAMRTC_TRACE_EVENT_SIZE		64U

/* Depth of call stack */
#define CAMRTC_TRACE_CALLSTACK_MAX	32U

/*
 * Trace memory header
 */

#define CAMRTC_TRACE_SIGNATURE_1	0x5420564eU
#define CAMRTC_TRACE_SIGNATURE_2	0x45434152U

struct camrtc_trace_memory_header {
	/* layout: offset 0 */
	uint32_t signature[2];
	uint32_t revision;
	uint32_t reserved1;
	uint32_t exception_offset;
	uint32_t exception_size;
	uint32_t exception_entries;
	uint32_t reserved2;
	uint32_t event_offset;
	uint32_t event_size;
	uint32_t event_entries;
	uint32_t reserved3;
	uint32_t reserved4[0xd0 / 4];

	/* pointer: offset 0x100 */
	uint32_t exception_next_idx;
	uint32_t event_next_idx;
	uint32_t reserved_ptrs[0x38 / 4];
} __packed;

/*
 * Exception entry
 */

enum camrtc_trace_armv7_exception_type {
	/* Reset = 0 */
	CAMRTC_ARMV7_EXCEPTION_UNDEFINED_INSTRUCTION = 1,
	/* SWI = 2 */
	CAMRTC_ARMV7_EXCEPTION_PREFETCH_ABORT = 3,
	CAMRTC_ARMV7_EXCEPTION_DATA_ABORT,
	CAMRTC_ARMV7_EXCEPTION_RSVD,	/* Should never happen */
	CAMRTC_ARMV7_EXCEPTION_IRQ,	/* Unhandled IRQ */
	CAMRTC_ARMV7_EXCEPTION_FIQ,	/* Unhandled FIQ */
};

struct camrtc_trace_callstack {
	uint32_t lr_stack_addr;		/* address in stack where lr is saved */
	uint32_t lr;			/* value of saved lr */
} __packed;

struct camrtc_trace_armv7_exception {
	uint32_t len;		/* length in byte including this */
	uint32_t type;		/* enum camrtc_trace_armv7_exception_type */
	union {
		uint32_t data[24];
		struct {
			uint32_t r0, r1, r2, r3;
			uint32_t r4, r5, r6, r7;
			uint32_t r8, r9, r10, r11;
			uint32_t r12, sp, lr, pc;
			uint32_t r8_prev, r9_prev, r10_prev, r11_prev, r12_prev;
			uint32_t sp_prev, lr_prev;
			uint32_t reserved;
		};
	} gpr;
	/* program status registers */
	uint32_t cpsr, spsr;
	/* data fault status/address register */
	uint32_t dfsr, dfar, adfsr;
	/* instruction fault status/address register */
	uint32_t ifsr, ifar, aifsr;
	struct camrtc_trace_callstack callstack[CAMRTC_TRACE_CALLSTACK_MAX];
} __packed;

/*
 * Each trace event shares the header.
 * The format of event data is determined by event type.
 */

#define CAMRTC_TRACE_EVENT_HEADER_SIZE		16U
#define CAMRTC_TRACE_EVENT_PAYLOAD_SIZE		\
	(CAMRTC_TRACE_EVENT_SIZE - CAMRTC_TRACE_EVENT_HEADER_SIZE)

#define CAMRTC_EVENT_TYPE_OFFSET		24U
#define CAMRTC_EVENT_TYPE_MASK			\
	(0xffU << CAMRTC_EVENT_TYPE_OFFSET)
#define CAMRTC_EVENT_TYPE_FROM_ID(id)		\
	(((id) & CAMRTC_EVENT_TYPE_MASK) >> CAMRTC_EVENT_TYPE_OFFSET)

#define CAMRTC_EVENT_MODULE_OFFSET		16U
#define CAMRTC_EVENT_MODULE_MASK		\
	(0xff << CAMRTC_EVENT_MODULE_OFFSET)
#define CAMRTC_EVENT_MODULE_FROM_ID(id)		\
	(((id) & CAMRTC_EVENT_MODULE_MASK) >> CAMRTC_EVENT_MODULE_OFFSET)

#define CAMRTC_EVENT_SUBID_OFFSET		0
#define CAMRTC_EVENT_SUBID_MASK			\
	(0xffff << CAMRTC_EVENT_SUBID_OFFSET)
#define CAMRTC_EVENT_SUBID_FROM_ID(id)		\
	(((id) & CAMRTC_EVENT_SUBID_MASK) >> CAMRTC_EVENT_SUBID_OFFSET)

#define CAMRTC_EVENT_MAKE_ID(type, module, subid) \
	(((type) << CAMRTC_EVENT_TYPE_OFFSET) | \
	((module) << CAMRTC_EVENT_MODULE_OFFSET) | (subid))

struct camrtc_event_header {
	uint32_t len;		/* Size in bytes including this field */
	uint32_t id;		/* Event ID */
	uint64_t tstamp;	/* Timestamp from TKE TSC */
} __packed;

struct camrtc_event_struct {
	struct camrtc_event_header header;
	union {
		uint8_t data8[CAMRTC_TRACE_EVENT_PAYLOAD_SIZE];
		uint32_t data32[CAMRTC_TRACE_EVENT_PAYLOAD_SIZE / 4];
	} data;
} __packed;

enum camrtc_event_type {
	CAMRTC_EVENT_TYPE_ARRAY,
	CAMRTC_EVENT_TYPE_ARMV7_EXCEPTION,
	CAMRTC_EVENT_TYPE_PAD,
	CAMRTC_EVENT_TYPE_START,
	CAMRTC_EVENT_TYPE_STRING,
	CAMRTC_EVENT_TYPE_BULK,
};

enum camrtc_event_module {
	CAMRTC_EVENT_MODULE_UNKNOWN,
	CAMRTC_EVENT_MODULE_BASE,
	CAMRTC_EVENT_MODULE_RTOS,
	CAMRTC_EVENT_MODULE_HEARTBEAT,
	CAMRTC_EVENT_MODULE_DBG,
	CAMRTC_EVENT_MODULE_MODS,
	CAMRTC_EVENT_MODULE_VINOTIFY,
	CAMRTC_EVENT_MODULE_I2C,
};

enum camrtc_trace_event_type_ids {
	camrtc_trace_type_exception =
		CAMRTC_EVENT_MAKE_ID(CAMRTC_EVENT_TYPE_ARMV7_EXCEPTION,
		CAMRTC_EVENT_MODULE_BASE, 0),
	camrtc_trace_type_pad =
		CAMRTC_EVENT_MAKE_ID(CAMRTC_EVENT_TYPE_PAD,
		CAMRTC_EVENT_MODULE_BASE, 0),
	camrtc_trace_type_start =
		CAMRTC_EVENT_MAKE_ID(CAMRTC_EVENT_TYPE_START,
		CAMRTC_EVENT_MODULE_BASE, 0),
	camrtc_trace_type_string =
		CAMRTC_EVENT_MAKE_ID(CAMRTC_EVENT_TYPE_STRING,
		CAMRTC_EVENT_MODULE_BASE, 0),
};

enum camrtc_trace_base_ids {
	camrtc_trace_base_ids_begin =
		CAMRTC_EVENT_MAKE_ID(CAMRTC_EVENT_TYPE_ARRAY,
			CAMRTC_EVENT_MODULE_BASE, 0),
	camrtc_trace_base_target_init,
	camrtc_trace_base_start_scheduler,
};

enum camrtc_trace_event_rtos_ids {
	camrtc_trace_rtos_ids_begin =
		CAMRTC_EVENT_MAKE_ID(CAMRTC_EVENT_TYPE_ARRAY,
			CAMRTC_EVENT_MODULE_RTOS, 0),
	camrtc_trace_rtos_task_switched_in,
	camrtc_trace_rtos_increase_tick_count,
	camrtc_trace_rtos_low_power_idle_begin,
	camrtc_trace_rtos_low_power_idle_end,
	camrtc_trace_rtos_task_switched_out,
	camrtc_trace_rtos_task_priority_inherit,
	camrtc_trace_rtos_task_priority_disinherit,
	camrtc_trace_rtos_blocking_on_queue_receive,
	camrtc_trace_rtos_blocking_on_queue_send,
	camrtc_trace_rtos_moved_task_to_ready_state,
	camrtc_trace_rtos_queue_create,
	camrtc_trace_rtos_queue_create_failed,
	camrtc_trace_rtos_create_mutex,
	camrtc_trace_rtos_create_mutex_failed,
	camrtc_trace_rtos_give_mutex_recursive,
	camrtc_trace_rtos_give_mutex_recursive_failed,
	camrtc_trace_rtos_take_mutex_recursive,
	camrtc_trace_rtos_take_mutex_recursive_failed,
	camrtc_trace_rtos_create_counting_semaphore,
	camrtc_trace_rtos_create_counting_semaphore_failed,
	camrtc_trace_rtos_queue_send,
	camrtc_trace_rtos_queue_send_failed,
	camrtc_trace_rtos_queue_receive,
	camrtc_trace_rtos_queue_peek,
	camrtc_trace_rtos_queue_peek_from_isr,
	camrtc_trace_rtos_queue_receive_failed,
	camrtc_trace_rtos_queue_send_from_isr,
	camrtc_trace_rtos_queue_send_from_isr_failed,
	camrtc_trace_rtos_queue_receive_from_isr,
	camrtc_trace_rtos_queue_receive_from_isr_failed,
	camrtc_trace_rtos_queue_peek_from_isr_failed,
	camrtc_trace_rtos_queue_delete,
	camrtc_trace_rtos_task_create,
	camrtc_trace_rtos_task_create_failed,
	camrtc_trace_rtos_task_delete,
	camrtc_trace_rtos_task_delay_until,
	camrtc_trace_rtos_task_delay,
	camrtc_trace_rtos_task_priority_set,
	camrtc_trace_rtos_task_suspend,
	camrtc_trace_rtos_task_resume,
	camrtc_trace_rtos_task_resume_from_isr,
	camrtc_trace_rtos_task_increment_tick,
	camrtc_trace_rtos_timer_create,
	camrtc_trace_rtos_timer_create_failed,
	camrtc_trace_rtos_timer_command_send,
	camrtc_trace_rtos_timer_expired,
	camrtc_trace_rtos_timer_command_received,
	camrtc_trace_rtos_malloc,
	camrtc_trace_rtos_free,
	camrtc_trace_rtos_event_group_create,
	camrtc_trace_rtos_event_group_create_failed,
	camrtc_trace_rtos_event_group_sync_block,
	camrtc_trace_rtos_event_group_sync_end,
	camrtc_trace_rtos_event_group_wait_bits_block,
	camrtc_trace_rtos_event_group_wait_bits_end,
	camrtc_trace_rtos_event_group_clear_bits,
	camrtc_trace_rtos_event_group_clear_bits_from_isr,
	camrtc_trace_rtos_event_group_set_bits,
	camrtc_trace_rtos_event_group_set_bits_from_isr,
	camrtc_trace_rtos_event_group_delete,
	camrtc_trace_rtos_pend_func_call,
	camrtc_trace_rtos_pend_func_call_from_isr,
	camrtc_trace_rtos_queue_registry_add,
};

enum camrtc_trace_dbg_ids {
	camrtc_trace_dbg_ids_begin =
		CAMRTC_EVENT_MAKE_ID(CAMRTC_EVENT_TYPE_ARRAY,
			CAMRTC_EVENT_MODULE_DBG, 0),
	camrtc_trace_dbg_unknown,
	camrtc_trace_dbg_enter,
	camrtc_trace_dbg_exit,
	camrtc_trace_dbg_set_loglevel,
};

enum camrtc_trace_vinotify_ids {
	camrtc_trace_vinotify_ids_begin =
		CAMRTC_EVENT_MAKE_ID(CAMRTC_EVENT_TYPE_ARRAY,
			CAMRTC_EVENT_MODULE_VINOTIFY, 0),
	camrtc_trace_vinotify_handle_msg,
};

#endif /* INCLUDE_CAMRTC_TRACE_H */
