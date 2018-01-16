/*
 * CWP hypervisor support
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Jason Chen CJ <jason.cj.chen@intel.com>
 *
 */
#include <asm/hypervisor.h>
#include <linux/vhm/vhm_msi.h>

static unsigned long cpu_khz_from_cwp(void)
{
	unsigned int eax, ebx, ecx, edx;

	/* Get TSC frequency from cpuid 0x40000010 */
	eax = 0x40000010;
	ebx = ecx = edx = 0;
	__cpuid(&eax, &ebx, &ecx, &edx);

	return (unsigned long)eax;
}

static uint32_t __init cwp_detect(void)
{
	return hypervisor_cpuid_base("CWPCWPCWP\0\0", 0);
}

static void __init cwp_init_platform(void)
{
	pv_cpu_ops.cpu_khz = cpu_khz_from_cwp;
	pv_irq_ops.write_msi = cwp_write_msi_msg;
}

static void cwp_pin_vcpu(int cpu)
{
	/* do nothing here now */
}

static bool cwp_x2apic_available(void)
{
	/* do not support x2apic */
	return false;
}

static void __init cwp_init_mem_mapping(void)
{
	/* do nothing here now */
}

const struct hypervisor_x86 x86_hyper_cwp = {
	.name                   = "CWP",
	.detect                 = cwp_detect,
	.type                 	= X86_HYPER_CWP,
	.init.init_platform     = cwp_init_platform,
	.runtime.pin_vcpu       = cwp_pin_vcpu,
	.init.x2apic_available  = cwp_x2apic_available,
	.init.init_mem_mapping	= cwp_init_mem_mapping,
};
EXPORT_SYMBOL(x86_hyper_cwp);
