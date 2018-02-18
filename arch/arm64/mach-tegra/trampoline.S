	.align 5
	.globl __aarch64_tramp

__aarch64_tramp:
	.inst	0xe59f0014	//	ldr r0, TEGRA_PMC_BASE
	.inst	0xe5901140	//	ldr r1, [r0, #PMC_SCRATCH41]
	.inst	0xe5801368	//	str r1, [r0, #PMC_SECURE_SCRATCH34]
	.inst	0xe3a01000	//	mov r1, #0
	.inst	0xe580136c	//	str r1, [r0, #PMC_SECURE_SCRATCH35]
	.inst	0xe3a00003	//	mov r0, #3
	.inst	0xee0c0f50	//	mcr 15, 0, r0, cr12, cr0, {2}
	.inst	0x7000e400	//	.word   PMC_BASE
