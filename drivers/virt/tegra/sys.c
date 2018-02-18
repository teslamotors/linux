/*
 * Copyright (C) 2014-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * System calls to hypervisor
 *
 * This file is BSD licensed so anyone can use the definitions to implement
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

#include "syscalls.h"

/*
 * NOTE:
 * Sysregs struct is for reads from HV only.
 *
 * To pass data to HV, use real registers [r0-rX] to HVC call.
 */

int hyp_read_gid(unsigned int *gid)
{
	int sysregs[12];
	int ret;

	ret = hvc_read_gid(sysregs);
	*gid = *((unsigned int *)&sysregs[0]);
	return ret;
}

int hyp_read_nguests(unsigned int *nguests)
{
	int sysregs[12];
	int ret;

	ret = hvc_read_nguests(sysregs);
	*nguests = *((unsigned int *)&sysregs[0]);
	return ret;
}

int hyp_read_ivc_info(uint64_t *ivc_info_page_pa)
{
	int sysregs[12];
	int ret;

	ret = hvc_read_ivc_info(sysregs);
	*ivc_info_page_pa = *((uint64_t *)&sysregs[0]);
	return ret;
}

int hyp_read_ipa_pa_info(struct hyp_ipa_pa_info *info, int guestid, u64 ipa)
{
	int sysregs[12];
	int ret;

	ret = hvc_read_ipa_pa_info(sysregs, guestid, ipa);
	*info = *((struct hyp_ipa_pa_info *)&sysregs[0]);
	return ret;
}

int hyp_raise_irq(unsigned int irq, unsigned int vmid)
{
	return hvc_raise_irq(irq, vmid);
}

uint64_t hyp_sysinfo_ipa(void)
{
	return hvc_sysinfo_ipa();
}
