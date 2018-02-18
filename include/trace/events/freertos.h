/*
 * Copyright (c) 2016 NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM freertos

#if !defined(_TRACE_FREERTOS_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_FREERTOS_H_

#include <linux/tracepoint.h>

/*
 * Classes with no argument
 */

DECLARE_EVENT_CLASS(rtos__noarg,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp),
	TP_STRUCT__entry(
		__field(u64, tstamp)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
	),
	TP_printk("tstamp:%llu", __entry->tstamp)
);

/*
 * Classes with 1 argument
 */

DECLARE_EVENT_CLASS(rtos__count,
	TP_PROTO(u64 tstamp, u32 count),
	TP_ARGS(tstamp, count),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, count)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->count = count;
	),
	TP_printk("tstamp:%llu count:%u", __entry->tstamp, __entry->count)
);

DECLARE_EVENT_CLASS(rtos__type,
	TP_PROTO(u64 tstamp, u32 type),
	TP_ARGS(tstamp, type),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, type)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->type = type;
	),
	TP_printk("tstamp:%llu type:%u", __entry->tstamp, __entry->type)
);

DECLARE_EVENT_CLASS(rtos__queue,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, queue)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->queue = queue;
	),
	TP_printk("tstamp:%llu queue:0x%08x", __entry->tstamp, __entry->queue)
);

DECLARE_EVENT_CLASS(rtos__tcb,
	TP_PROTO(u64 tstamp, u32 tcb),
	TP_ARGS(tstamp, tcb),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, tcb)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->tcb = tcb;
	),
	TP_printk("tstamp:%llu tcb:0x%08x", __entry->tstamp, __entry->tcb)
);

DECLARE_EVENT_CLASS(rtos__mutex,
	TP_PROTO(u64 tstamp, u32 mutex),
	TP_ARGS(tstamp, mutex),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, mutex)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->mutex = mutex;
	),
	TP_printk("tstamp:%llu mutex:0x%08x", __entry->tstamp, __entry->mutex)
);

DECLARE_EVENT_CLASS(rtos__timer,
	TP_PROTO(u64 tstamp, u32 timer),
	TP_ARGS(tstamp, timer),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, timer)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->timer = timer;
	),
	TP_printk("tstamp:%llu timer:0x%08x", __entry->tstamp, __entry->timer)
);

DECLARE_EVENT_CLASS(rtos__eventgroup,
	TP_PROTO(u64 tstamp, u32 eventgroup),
	TP_ARGS(tstamp, eventgroup),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, eventgroup)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->eventgroup = eventgroup;
	),
	TP_printk("tstamp:%llu eventgroup:%u", __entry->tstamp,
		__entry->eventgroup)
);

/*
 * Classes with 2 arguments
 */

DECLARE_EVENT_CLASS(rtos__tcb_priority,
	TP_PROTO(u64 tstamp, u32 tcb, u32 priority),
	TP_ARGS(tstamp, tcb, priority),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, tcb)
		__field(u32, priority)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->tcb = tcb;
		__entry->priority = priority;
	),
	TP_printk("tstamp:%llu tcb:%u priority:%u",
		__entry->tstamp, __entry->tcb, __entry->priority)
);

DECLARE_EVENT_CLASS(rtos__addr_size,
	TP_PROTO(u64 tstamp, u32 addr, u32 size),
	TP_ARGS(tstamp, addr, size),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, addr)
		__field(u32, size)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->addr = addr;
		__entry->size = size;
	),
	TP_printk("tstamp:%llu addr:%u size:%u",
		__entry->tstamp, __entry->addr, __entry->size)
);

DECLARE_EVENT_CLASS(rtos__eventgroup_wait,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 wait),
	TP_ARGS(tstamp, eventgroup, wait),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, eventgroup)
		__field(u32, wait)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->eventgroup = eventgroup;
		__entry->wait = wait;
	),
	TP_printk("tstamp:%llu eventgroup:%u wait:%u",
		__entry->tstamp, __entry->eventgroup, __entry->wait)
);

DECLARE_EVENT_CLASS(rtos__eventgroup_clear,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 clear),
	TP_ARGS(tstamp, eventgroup, clear),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, eventgroup)
		__field(u32, clear)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->eventgroup = eventgroup;
		__entry->clear = clear;
	),
	TP_printk("tstamp:%llu eventgroup:%u clear:%u",
		__entry->tstamp, __entry->eventgroup, __entry->clear)
);

DECLARE_EVENT_CLASS(rtos__eventgroup_set,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 set),
	TP_ARGS(tstamp, eventgroup, set),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, eventgroup)
		__field(u32, set)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->eventgroup = eventgroup;
		__entry->set = set;
	),
	TP_printk("tstamp:%llu eventgroup:%u set:%u",
		__entry->tstamp, __entry->eventgroup, __entry->set)
);

DECLARE_EVENT_CLASS(rtos__queue_name,
	TP_PROTO(u64 tstamp, u32 queue, u32 name),
	TP_ARGS(tstamp, queue, name),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, queue)
		__field(u32, name)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->queue = queue;
		__entry->name = name;
	),
	TP_printk("tstamp:%llu queue:%u name:0x%08x",
		__entry->tstamp, __entry->queue, __entry->name)
);

/*
 * Classes with 3 arguments
 */

DECLARE_EVENT_CLASS(rtos__ptimer_msgid_value,
	TP_PROTO(u64 tstamp, u32 ptimer, u32 msgid, u32 value),
	TP_ARGS(tstamp, ptimer, msgid, value),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, ptimer)
		__field(u32, msgid)
		__field(u32, value)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->ptimer = ptimer;
		__entry->msgid = msgid;
		__entry->value = value;
	),
	TP_printk("tstamp:%llu timer:0x%08x msgid:%u value:%u",
		__entry->tstamp, __entry->ptimer, __entry->msgid,
		__entry->value)
);

DECLARE_EVENT_CLASS(rtos__eventgroup_set_wait,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 set, u32 wait),
	TP_ARGS(tstamp, eventgroup, set, wait),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, eventgroup)
		__field(u32, set)
		__field(u32, wait)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->eventgroup = eventgroup;
		__entry->set = set;
		__entry->wait = wait;
	),
	TP_printk("tstamp:%llu eventgroup:%u set:%u wait:%u",
		__entry->tstamp, __entry->eventgroup, __entry->set,
		__entry->wait)
);

DECLARE_EVENT_CLASS(rtos__eventgroup_wait_timeout,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 wait, u32 timeout),
	TP_ARGS(tstamp, eventgroup, wait, timeout),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, eventgroup)
		__field(u32, wait)
		__field(u32, timeout)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->eventgroup = eventgroup;
		__entry->timeout = timeout;
	),
	TP_printk("tstamp:%llu eventgroup:%u wait:%u timeout:%u",
		__entry->tstamp, __entry->eventgroup,
		__entry->wait, __entry->timeout)
);

/*
 * Classes with 4 arguments
 */

DECLARE_EVENT_CLASS(rtos__timer_msgid_value_return,
	TP_PROTO(u64 tstamp, u32 timer, u32 msgid, u32 value, u32 ret),
	TP_ARGS(tstamp, timer, msgid, value, ret),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, timer)
		__field(u32, msgid)
		__field(u32, value)
		__field(u32, ret)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->timer = timer;
		__entry->msgid = msgid;
		__entry->value = value;
		__entry->ret = ret;
	),
	TP_printk("tstamp:%llu timer:%u msgid:%u value:%u return:%u",
		__entry->tstamp, __entry->timer, __entry->msgid,
		__entry->value, __entry->ret)
);

DECLARE_EVENT_CLASS(rtos__eventgroup_set_wait_timeout,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 set, u32 wait, u32 timeout),
	TP_ARGS(tstamp, eventgroup, set, wait, timeout),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, eventgroup)
		__field(u32, set)
		__field(u32, wait)
		__field(u32, timeout)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->eventgroup = eventgroup;
		__entry->set = set;
		__entry->timeout = timeout;
	),
	TP_printk("tstamp:%llu eventgroup:%u set:%u wait:%u timeout:%u",
		__entry->tstamp, __entry->eventgroup, __entry->set,
		__entry->wait, __entry->timeout)
);

DECLARE_EVENT_CLASS(rtos__function_param1_param2_ret,
	TP_PROTO(u64 tstamp, u32 function, u32 param1, u32 param2, u32 ret),
	TP_ARGS(tstamp, function, param1, param2, ret),
	TP_STRUCT__entry(
		__field(u64, tstamp)
		__field(u32, function)
		__field(u32, param1)
		__field(u32, param2)
		__field(u32, ret)
	),
	TP_fast_assign(
		__entry->tstamp = tstamp;
		__entry->function = function;
		__entry->param1 = param1;
		__entry->ret = ret;
	),
	TP_printk(
	    "tstamp:%llu function:0x%08x param1:0x%08x param2:0x%08x ret:%u",
	    __entry->tstamp, __entry->function, __entry->param1,
	    __entry->param2, __entry->ret)
);

/*
 * Events
 */

DEFINE_EVENT(rtos__noarg, rtos_task_switched_in,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__count, rtos_increase_tick_count,
	TP_PROTO(u64 tstamp, u32 count),
	TP_ARGS(tstamp, count)
);

DEFINE_EVENT(rtos__noarg, rtos_low_power_idle_begin,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__noarg, rtos_low_power_idle_end,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__noarg, rtos_task_switched_out,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__tcb_priority, rtos_task_priority_inherit,
	TP_PROTO(u64 tstamp, u32 tcb, u32 priority),
	TP_ARGS(tstamp, tcb, priority)
);

DEFINE_EVENT(rtos__tcb_priority, rtos_task_priority_disinherit,
	TP_PROTO(u64 tstamp, u32 tcb, u32 priority),
	TP_ARGS(tstamp, tcb, priority)
);

DEFINE_EVENT(rtos__queue, rtos_blocking_on_queue_receive,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_blocking_on_queue_send,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__tcb, rtos_moved_task_to_ready_state,
	TP_PROTO(u64 tstamp, u32 tcb),
	TP_ARGS(tstamp, tcb)
);

DEFINE_EVENT(rtos__queue, rtos_queue_create,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__type, rtos_queue_create_failed,
	TP_PROTO(u64 tstamp, u32 type),
	TP_ARGS(tstamp, type)
);

DEFINE_EVENT(rtos__queue, rtos_create_mutex,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__noarg, rtos_create_mutex_failed,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__mutex, rtos_give_mutex_recursive,
	TP_PROTO(u64 tstamp, u32 mutex),
	TP_ARGS(tstamp, mutex)
);

DEFINE_EVENT(rtos__mutex, rtos_give_mutex_recursive_failed,
	TP_PROTO(u64 tstamp, u32 mutex),
	TP_ARGS(tstamp, mutex)
);

DEFINE_EVENT(rtos__mutex, rtos_take_mutex_recursive,
	TP_PROTO(u64 tstamp, u32 mutex),
	TP_ARGS(tstamp, mutex)
);

DEFINE_EVENT(rtos__mutex, rtos_take_mutex_recursive_failed,
	TP_PROTO(u64 tstamp, u32 mutex),
	TP_ARGS(tstamp, mutex)
);

DEFINE_EVENT(rtos__noarg, rtos_create_counting_semaphore,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__noarg, rtos_create_counting_semaphore_failed,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__queue, rtos_queue_send,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_send_failed,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_receive,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_peek,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_peek_from_isr,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_receive_failed,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_send_from_isr,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_send_from_isr_failed,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_receive_from_isr,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_receive_from_isr_failed,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_peek_from_isr_failed,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__queue, rtos_queue_delete,
	TP_PROTO(u64 tstamp, u32 queue),
	TP_ARGS(tstamp, queue)
);

DEFINE_EVENT(rtos__tcb, rtos_task_create,
	TP_PROTO(u64 tstamp, u32 tcb),
	TP_ARGS(tstamp, tcb)
);

DEFINE_EVENT(rtos__noarg, rtos_task_create_failed,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__tcb, rtos_task_delete,
	TP_PROTO(u64 tstamp, u32 tcb),
	TP_ARGS(tstamp, tcb)
);

DEFINE_EVENT(rtos__noarg, rtos_task_delay_until,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__noarg, rtos_task_delay,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__tcb_priority, rtos_task_priority_set,
	TP_PROTO(u64 tstamp, u32 tcb, u32 priority),
	TP_ARGS(tstamp, tcb, priority)
);

DEFINE_EVENT(rtos__tcb, rtos_task_suspend,
	TP_PROTO(u64 tstamp, u32 tcb),
	TP_ARGS(tstamp, tcb)
);

DEFINE_EVENT(rtos__tcb, rtos_task_resume,
	TP_PROTO(u64 tstamp, u32 tcb),
	TP_ARGS(tstamp, tcb)
);

DEFINE_EVENT(rtos__tcb, rtos_task_resume_from_isr,
	TP_PROTO(u64 tstamp, u32 tcb),
	TP_ARGS(tstamp, tcb)
);

DEFINE_EVENT(rtos__count, rtos_task_increment_tick,
	TP_PROTO(u64 tstamp, u32 count),
	TP_ARGS(tstamp, count)
);

DEFINE_EVENT(rtos__timer, rtos_timer_create,
	TP_PROTO(u64 tstamp, u32 timer),
	TP_ARGS(tstamp, timer)
);

DEFINE_EVENT(rtos__noarg, rtos_timer_create_failed,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__timer_msgid_value_return, rtos_timer_command_send,
	TP_PROTO(u64 tstamp, u32 timer, u32 msgid, u32 value, u32 ret),
	TP_ARGS(tstamp, timer, msgid, value, ret)
);

DEFINE_EVENT(rtos__timer, rtos_timer_expired,
	TP_PROTO(u64 tstamp, u32 timer),
	TP_ARGS(tstamp, timer)
);

DEFINE_EVENT(rtos__ptimer_msgid_value, rtos_timer_command_received,
	TP_PROTO(u64 tstamp, u32 ptimer, u32 msgid, u32 value),
	TP_ARGS(tstamp, ptimer, msgid, value)
);

DEFINE_EVENT(rtos__addr_size, rtos_malloc,
	TP_PROTO(u64 tstamp, u32 addr, u32 size),
	TP_ARGS(tstamp, addr, size)
);

DEFINE_EVENT(rtos__addr_size, rtos_free,
	TP_PROTO(u64 tstamp, u32 addr, u32 size),
	TP_ARGS(tstamp, addr, size)
);

DEFINE_EVENT(rtos__eventgroup, rtos_event_group_create,
	TP_PROTO(u64 tstamp, u32 eventgroup),
	TP_ARGS(tstamp, eventgroup)
);

DEFINE_EVENT(rtos__noarg, rtos_event_group_create_failed,
	TP_PROTO(u64 tstamp),
	TP_ARGS(tstamp)
);

DEFINE_EVENT(rtos__eventgroup_set_wait, rtos_event_group_sync_block,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 set, u32 wait),
	TP_ARGS(tstamp, eventgroup, set, wait)
);

DEFINE_EVENT(rtos__eventgroup_set_wait_timeout, rtos_event_group_sync_end,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 set, u32 wait, u32 timeout),
	TP_ARGS(tstamp, eventgroup, set, wait, timeout)
);

DEFINE_EVENT(rtos__eventgroup_wait, rtos_event_group_wait_bits_block,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 wait),
	TP_ARGS(tstamp, eventgroup, wait)
);

DEFINE_EVENT(rtos__eventgroup_wait_timeout, rtos_event_group_wait_bits_end,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 wait, u32 timeout),
	TP_ARGS(tstamp, eventgroup, wait, timeout)
);

DEFINE_EVENT(rtos__eventgroup_clear, rtos_event_group_clear_bits,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 clear),
	TP_ARGS(tstamp, eventgroup, clear)
);

DEFINE_EVENT(rtos__eventgroup_clear, rtos_event_group_clear_bits_from_isr,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 clear),
	TP_ARGS(tstamp, eventgroup, clear)
);

DEFINE_EVENT(rtos__eventgroup_set, rtos_event_group_set_bits,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 set),
	TP_ARGS(tstamp, eventgroup, set)
);

DEFINE_EVENT(rtos__eventgroup_set, rtos_event_group_set_bits_from_isr,
	TP_PROTO(u64 tstamp, u32 eventgroup, u32 set),
	TP_ARGS(tstamp, eventgroup, set)
);

DEFINE_EVENT(rtos__eventgroup, rtos_event_group_delete,
	TP_PROTO(u64 tstamp, u32 eventgroup),
	TP_ARGS(tstamp, eventgroup)
);

DEFINE_EVENT(rtos__function_param1_param2_ret, rtos_pend_func_call,
	TP_PROTO(u64 tstamp, u32 function, u32 param1, u32 param2, u32 ret),
	TP_ARGS(tstamp, function, param1, param2, ret)
);

DEFINE_EVENT(rtos__function_param1_param2_ret, rtos_pend_func_call_from_isr,
	TP_PROTO(u64 tstamp, u32 function, u32 param1, u32 param2, u32 ret),
	TP_ARGS(tstamp, function, param1, param2, ret)
);

DEFINE_EVENT(rtos__queue_name, rtos_queue_registry_add,
	TP_PROTO(u64 tstamp, u32 queue, u32 name),
	TP_ARGS(tstamp, queue, name)
);

#endif /* _TRACE_FREERTOS_H_ */

#include <trace/define_trace.h>
