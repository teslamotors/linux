/*
 * Copyright 2019 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef KFD_DEBUG_EVENTS_H_INCLUDED
#define KFD_DEBUG_EVENTS_H_INCLUDED

#include "kfd_priv.h"

uint32_t kfd_dbg_get_queue_status_word(struct queue *q, int flags);

int kfd_dbg_ev_query_debug_event(struct kfd_process_device *pdd,
		      unsigned int *queue_id,
		      unsigned int flags,
		      uint32_t *event_status);

void kfd_set_dbg_ev_from_interrupt(struct kfd_dev *dev,
				   unsigned int pasid,
				   uint32_t doorbell_id,
				   bool is_vmfault);

int kfd_dbg_ev_enable(struct kfd_process_device *pdd);

int kfd_dbg_trap_disable(struct kfd_process_device *pdd);
int kfd_dbg_trap_enable(struct kfd_process_device *pdd, uint32_t *fd);
int kfd_dbg_trap_set_wave_launch_override(struct kfd_dev *dev,
		uint32_t vmid,
		uint32_t trap_override,
		uint32_t trap_mask_bits,
		uint32_t trap_mask_request,
		uint32_t *trap_mask_prev,
		uint32_t *trap_mask_supported);
int kfd_dbg_trap_set_wave_launch_mode(struct kfd_process_device *pdd,
		uint8_t wave_launch_mode);
int kfd_dbg_trap_clear_address_watch(struct kfd_process_device *pdd,
		uint32_t watch_id);
int kfd_dbg_trap_set_address_watch(struct kfd_process_device *pdd,
		uint64_t watch_address,
		uint32_t watch_address_mask,
		uint32_t *watch_id,
		uint32_t watch_mode);
int kfd_dbg_trap_set_precise_mem_ops(struct kfd_dev *dev, uint32_t enable);

#endif
