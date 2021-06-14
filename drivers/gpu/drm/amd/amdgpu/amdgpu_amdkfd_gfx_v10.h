/*
 * Copyright 2020 Advanced Micro Devices, Inc.
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

void kgd_gfx_v10_enable_debug_trap(struct kgd_dev *kgd,
				      uint32_t vmid);
void kgd_gfx_v10_disable_debug_trap(struct kgd_dev *kgd, uint32_t vmid);
int kgd_gfx_v10_set_wave_launch_trap_override(struct kgd_dev *kgd,
					     uint32_t vmid,
					     uint32_t trap_override,
					     uint32_t trap_mask_bits,
					     uint32_t trap_mask_request,
					     uint32_t *trap_mask_prev,
					     uint32_t *trap_mask_supported);
void kgd_gfx_v10_set_wave_launch_mode(struct kgd_dev *kgd,
					 uint8_t wave_launch_mode,
					 uint32_t vmid);
void kgd_gfx_v10_set_address_watch(struct kgd_dev *kgd,
					uint64_t watch_address,
					uint32_t watch_address_mask,
					uint32_t watch_id,
					uint32_t watch_mode,
					uint32_t debug_vmid);
void kgd_gfx_v10_clear_address_watch(struct kgd_dev *kgd,
					uint32_t watch_id);
void kgd_gfx_v10_get_iq_wait_times(struct kgd_dev *kgd,
				uint32_t *wait_times);
void kgd_gfx_v10_build_grace_period_packet_info(struct kgd_dev *kgd,
					uint32_t wait_times,
					uint32_t grace_period,
					uint32_t *reg_offset,
					uint32_t *reg_data);
