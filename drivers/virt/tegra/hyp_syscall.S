/*
 * Copyright (C) 2014-2016, NVIDIA Corporation. All rights reserved.
 *
 * Hypervisor related routines
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
#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/hwcap.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>

#include "syscalls.h"

#.arch_extension virt

/*
 * Old toolchains cannot assemble this so we use a macro.
 */
.macro hvc nr
.if \nr > 15
.error "Unsupported hvc call number, too high for this macro"
.endif
.word 0xe140007\nr
.endm
/*
 * Declares a system call entry exit function with
 * HVC value, a pointer and number of words sent/received.
 */
.macro declare_syscall name, syscall_nr
	ENTRY(hvc_\name)
		stmfd	sp!, {r0-r12}	@ sp->|r0-r12|
		hvc	\syscall_nr	@ HVC Call
		stmfd	sp!, {r0}	@ Store result |->res|r0-r12|
		ldr	r0, [sp, #4]	@ Load struct ptr from stack
		stmia	r0, {r1-r12}	@ Fill structure
		ldmfd	sp!, {r0}	@ Restore result in r0 |->r0-r12|
		add	sp, sp, #16 	@ Unwind until r4
		ldmfd	sp!, {r4-r12}	@ Restore regs to preserve.
		mov	pc, lr
	ENDPROC(hvc_\name)
.endm

/* Those who need to read data use this */
declare_syscall read_gid HVC_NR_READ_GID
declare_syscall read_nguests HVC_NR_READ_NGUESTS
declare_syscall read_ivc_info HVC_NR_READ_IVC
declare_syscall read_ipa_pa_info HVC_NR_READ_IPA_PA
declare_syscall sysinfo_ipa HVC_NR_SYSINFO_IPA

/* TODO: Define calls with no read in a way that does less reg. read/writes */
/*
 * r0 = irqnr
 * r1 = vmid
 *
 * Return:
 * r0 = return value
 */
ENTRY(hvc_raise_irq)
	hvc	HVC_NR_RAISE_IRQ	@ HVC Call
	mov	pc, lr
ENDPROC(hvc_raise_irq)

