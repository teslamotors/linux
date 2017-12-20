/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __LINUX_TRUSTY_TRUSTY_H
#define __LINUX_TRUSTY_TRUSTY_H

#include <linux/kernel.h>
#include <linux/trusty/sm_err.h>
#include <linux/device.h>
#include <linux/pagemap.h>


#if IS_ENABLED(CONFIG_TRUSTY)
s32 trusty_std_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2);
s32 trusty_fast_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2);
#ifdef CONFIG_64BIT
s64 trusty_fast_call64(struct device *dev, u64 smcnr, u64 a0, u64 a1, u64 a2);
#endif
#else
static inline s32 trusty_std_call32(struct device *dev, u32 smcnr,
				    u32 a0, u32 a1, u32 a2)
{
	return SM_ERR_UNDEFINED_SMC;
}
static inline s32 trusty_fast_call32(struct device *dev, u32 smcnr,
				     u32 a0, u32 a1, u32 a2)
{
	return SM_ERR_UNDEFINED_SMC;
}
#ifdef CONFIG_64BIT
static inline s64 trusty_fast_call64(struct device *dev,
				     u64 smcnr, u64 a0, u64 a1, u64 a2)
{
	return SM_ERR_UNDEFINED_SMC;
}
#endif
#endif

struct notifier_block;
enum {
	TRUSTY_CALL_PREPARE,
	TRUSTY_CALL_RETURNED,
};
int trusty_call_notifier_register(struct device *dev,
				  struct notifier_block *n);
int trusty_call_notifier_unregister(struct device *dev,
				    struct notifier_block *n);
const char *trusty_version_str_get(struct device *dev);
u32 trusty_get_api_version(struct device *dev);

struct ns_mem_page_info {
	uint64_t attr;
};

int trusty_encode_page_info(struct ns_mem_page_info *inf,
			    struct page *page, pgprot_t pgprot);

int trusty_call32_mem_buf(struct device *dev, u32 smcnr,
			  struct page *page,  u32 size,
			  pgprot_t pgprot);

struct trusty_nop {
	struct list_head node;
	u32 args[3];
};

static inline void trusty_nop_init(struct trusty_nop *nop,
				   u32 arg0, u32 arg1, u32 arg2) {
	INIT_LIST_HEAD(&nop->node);
	nop->args[0] = arg0;
	nop->args[1] = arg1;
	nop->args[2] = arg2;
}

void trusty_enqueue_nop(struct device *dev, struct trusty_nop *nop);
void trusty_dequeue_nop(struct device *dev, struct trusty_nop *nop);

#define TRUSTY_VMCALL_PENDING_INTR 0x74727505
static inline void set_pending_intr_to_lk(uint8_t vector)
{
#ifdef CONFIG_X86
	__asm__ __volatile__(
		"vmcall"
		::"a"(TRUSTY_VMCALL_PENDING_INTR), "b"(vector)
	);
#endif
}

void trusty_update_wall_info(struct device *dev, void *va, size_t sz);
void *trusty_wall_base(struct device *dev);
void *trusty_wall_per_cpu_item_ptr(struct device *dev, unsigned int cpu,
				   u32 item_id, size_t exp_sz);

/* CPUID leaf 0x3 is used because eVMM will trap this leaf.*/
#define EVMM_SIGNATURE_CORP 0x43544E49  /* "INTC", edx */
#define EVMM_SIGNATURE_VMM  0x4D4D5645  /* "EVMM", ecx */

static inline int trusty_check_cpuid(u32 *vmm_signature)
{
	u32 eax, ebx, ecx, edx;

	cpuid(3, &eax, &ebx, &ecx, &edx);
	if ((ecx != EVMM_SIGNATURE_VMM) ||
	    (edx != EVMM_SIGNATURE_CORP)) {
		return -EINVAL;
	}

	if(vmm_signature) {
		*vmm_signature = ecx;
	}

	return 0;
}

/* High 32 bits of unsigned 64-bit integer*/
#ifdef CONFIG_64BIT
#define HIULINT(x) ((x) >> 32)
#else
#define HIULINT(x) 0
#endif
#endif
