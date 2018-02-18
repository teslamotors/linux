/*
 * Copyright (C) 2014-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Hypervisor interfaces
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of NVIDIA CORPORATION nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NVIDIA CORPORATION OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __VMM_SYSCALLS_H__
#define __VMM_SYSCALLS_H__

#define HVC_NR_READ_STAT		1
#define HVC_NR_READ_IVC			2
#define HVC_NR_READ_GID			3
#define HVC_NR_RAISE_IRQ		4
#define HVC_NR_READ_NGUESTS		5
#define HVC_NR_READ_IPA_PA		6
#define HVC_NR_READ_GUEST_RESET_MASK	7
#define HVC_NR_ACK_GUEST_CLEANUP	8
#define HVC_NR_READ_HYP_INFO		9
#define HVC_NR_GUEST_RESET		10
#define HVC_NR_SYSINFO_IPA		13

#define GUEST_PRIMARY		0
#define GUEST_IVC_SERVER	0

#ifndef __ASSEMBLY__

#if defined(__KERNEL__)
#include <linux/types.h>
#endif

struct tegra_hv_queue_data {
	uint32_t	id;	/* IVC id */
	uint32_t	peers[2];
	uint32_t	size;
	uint32_t	nframes;
	uint32_t	frame_size;
	uint32_t	offset;
	uint16_t	irq, raise_irq;
};

struct ivc_mempool {
	uint64_t pa;
	uint64_t size;
	uint32_t id;
	uint32_t peer_vmid;
};

struct ivc_shared_area {
	uint64_t pa;
	uint64_t size;
	uint32_t guest;
	uint16_t free_irq_start;
	uint16_t free_irq_count;
};

struct ivc_info_page {
	uint32_t nr_queues;
	uint32_t nr_areas;
	uint32_t nr_mempools;

	/* The actual length of this array is nr_areas. */
	struct ivc_shared_area areas[];

	/*
	 * Following this array is an array of queue data structures with an
	 * entry per queue that is accessible in the current guest. This array
	 * is terminated by an entry with no frames.
	 *
	 * struct tegra_hv_queue_data queue_data[nr_queues];
	 */
};

static inline const struct tegra_hv_queue_data *ivc_info_queue_array(
		const struct ivc_info_page *info)
{
	return (struct tegra_hv_queue_data *)&info->areas[info->nr_areas];
}

static inline const struct ivc_mempool *ivc_info_mempool_array(
		const struct ivc_info_page *info)
{
	return (struct ivc_mempool *)
			&ivc_info_queue_array(info)[info->nr_queues];
}

struct hyp_ipa_pa_info {
	uint64_t base;       /* base of contiguous pa region */
	uint64_t offset;     /* offset for requested ipa address */
	uint64_t size;       /* size of pa region */
};

#ifdef CONFIG_ARM64

#define _X4_X17 "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", \
"x13", "x14", "x15", "x16", "x17"

static inline int hyp_read_gid(unsigned int *gid)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_GID)
		: "x2", "x3", _X4_X17);

	*gid = r1;
	return (int)r0;
}

static inline int hyp_read_nguests(unsigned int *nguests)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_NGUESTS)
		: "x2", "x3", _X4_X17);

	*nguests = r1;
	return (int)r0;
}

static inline int hyp_read_ivc_info(uint64_t *ivc_info_page_pa)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_IVC)
		: "x2", "x3", _X4_X17);

	*ivc_info_page_pa = r1;
	return (int)r0;
}

static inline int hyp_read_ipa_pa_info(struct hyp_ipa_pa_info *info,
		unsigned int guestid, uint64_t ipa)
{
	register uint64_t r0 asm("x0") = guestid;
	register uint64_t r1 asm("x1") = ipa;
	register uint64_t r2 asm("x2");
	register uint64_t r3 asm("x3");


	asm("hvc %4"
		: "+r"(r0), "+r"(r1), "=r"(r2), "=r"(r3)
		: "i"(HVC_NR_READ_IPA_PA)
		: _X4_X17);

	info->base = r1;
	info->offset = r2;
	info->size = r3;

	return (int)r0;
}

static inline int hyp_raise_irq(unsigned int irq, unsigned int vmid)
{
	register uint64_t r0 asm("x0") = irq;
	register uint64_t r1 asm("x1") = vmid;

	asm volatile("hvc %1"
		: "+r"(r0)
		: "i"(HVC_NR_RAISE_IRQ), "r"(r1)
		: "x2", "x3", _X4_X17);

	return (int)r0;
}

static inline int hyp_read_guest_reset_mask(unsigned int *guest_reset_mask)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_GUEST_RESET_MASK)
		: "x2", "x3", _X4_X17);

	*guest_reset_mask = r1;
	return (int)r0;
}

static inline int hyp_ack_guest_cleanup(unsigned int vmid)
{
	register uint64_t r0 asm("x0") = vmid;

	asm volatile("hvc %1"
		: "+r"(r0)
		: "i"(HVC_NR_ACK_GUEST_CLEANUP)
		: "x1", "x2", "x3", _X4_X17);

	return (int)r0;
}

static inline int hyp_read_hyp_info(uint64_t *hyp_info_page_pa)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_HYP_INFO)
		: "x2", "x3", _X4_X17);

	*hyp_info_page_pa = r1;
	return (int)r0;
}

static inline int hyp_guest_reset(unsigned int vmid)
{
	register uint64_t r0 asm("x0") = vmid;

	asm volatile("hvc %1"
		: "+r"(r0)
		: "i"(HVC_NR_GUEST_RESET)
		: "x1", "x2", "x3", _X4_X17);

	return (int)r0;
}

static inline uint64_t hyp_sysinfo_ipa(void)
{
	register uint64_t r0 asm("x0");

	asm("hvc %1"
		: "=r"(r0)
		: "i"(HVC_NR_SYSINFO_IPA)
		: "x1", "x2", "x3", _X4_X17);

	return r0;
}

#undef _X4_X17

#else

int hyp_read_gid(unsigned int *gid);
int hyp_read_nguests(unsigned int *nguests);
int hyp_read_ivc_info(uint64_t *ivc_info_page_pa);
int hyp_read_ipa_pa_info(struct hyp_ipa_pa_info *info, int guestid, u64 ipa);
int hyp_raise_irq(unsigned int irq, unsigned int vmid);
uint64_t hyp_sysinfo_ipa(void);

/* ASM prototypes */
extern int hvc_read_gid(void *);
extern int hvc_read_ivc_info(int *);
extern int hvc_read_ipa_pa_info(void *, int guestid, uint64_t ipa);
extern int hvc_read_nguests(void *);
extern int hvc_raise_irq(unsigned int irq, unsigned int vmid);

#endif /* CONFIG_ARCH_ARM64 */

#endif /* !__ASSEMBLY__ */

#endif /* __VMM_SYSCALLS_H__ */
