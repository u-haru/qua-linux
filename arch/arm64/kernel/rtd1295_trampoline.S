/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Realtek RTD1295 secondary CPU entry trampoline.
 *
 * The RTD1295 boot ROM/BL31 parks the secondary CPUs at EL3, MMU and caches
 * off, polling the SB2 register cpu-release-addr. Once the kernel writes the
 * physical address of this code there and executes SEV, all secondaries jump
 * here at EL3. Drop to EL2 (non-secure, AArch64) like the vendor
 * armv8_switch_to_el2 does, then join the generic spin-table holding pen,
 * which waits for this CPU's MPIDR in secondary_holding_pen_release.
 *
 * Runs at its physical address with the MMU off: everything is PC-relative.
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>

#define CurrentEL_EL3		(3 << 2)

/* NS | RES1(5:4) | SMD | HCE | RW: EL2 is AArch64, non-secure, no SMC */
#define RTD1295_SCR_EL3		0x5b1
/* RES1 bits only: no FP/SIMD traps to EL2 */
#define RTD1295_CPTR_EL2	0x33ff
/* RES1 bits (29,28,23,22,18,16,11,5,4), MMU/caches off, little endian */
#define RTD1295_SCTLR_EL2	0x30c50830
/* A53 CPUECTLR_EL1, SMPEN bit */
#define A53_CPUECTLR_EL1	S3_1_C15_C2_1
#define A53_CPUECTLR_SMPEN	(1 << 6)

	.section ".idmap.text","a"

SYM_FUNC_START(rtd1295_secondary_trampoline)
	mrs	x0, CurrentEL
	cmp	x0, #CurrentEL_EL3
	b.ne	1f

	/* Same frequency as the boot CPU (CNTFRQ_EL0 is only writable at EL3) */
	adr_l	x1, rtd1295_secondary_cntfrq
	ldr	x1, [x1]
	cbz	x1, 0f
	msr	cntfrq_el0, x1
0:
	/* Take part in coherency before the kernel enables the D-cache */
	mrs	x1, A53_CPUECTLR_EL1
	orr	x1, x1, #A53_CPUECTLR_SMPEN
	msr	A53_CPUECTLR_EL1, x1

	mov	x1, #RTD1295_SCR_EL3
	msr	scr_el3, x1
	msr	cptr_el3, xzr
	mov	x1, #RTD1295_CPTR_EL2
	msr	cptr_el2, x1
	msr	cntvoff_el2, xzr
	mov_q	x1, RTD1295_SCTLR_EL2
	msr	sctlr_el2, x1
	mov	x1, sp
	msr	sp_el2, x1
	/* EL2h, DAIF masked */
	mov	x1, #(PSR_MODE_EL2h | PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | PSR_F_BIT)
	msr	spsr_el3, x1
	adr_l	x1, secondary_holding_pen
	msr	elr_el3, x1
	isb
	eret
1:
	b	secondary_holding_pen
SYM_FUNC_END(rtd1295_secondary_trampoline)
