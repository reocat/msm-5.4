/*
 * arch/arm/power/sleep.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2011, Ambarella, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/suspend.h>
#include <linux/stddef.h>

#include <asm/suspend.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

struct saved_context arch_arm_context;
extern int in_suspend;

void save_processor_state(void)
{
	__memzero(&arch_arm_context, sizeof(arch_arm_context));

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("orr	r0, r0, #0x1f" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.usr_sp) : "cc", "r0");
	asm("mov	r1, sp" : : : "cc", "r1");
	asm("mov	r2, lr" : : : "cc", "r2");
	asm("stmea	r0, {r1-r2}" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x17" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.abt_sp) : "cc", "r0");
	asm("mov	r1, sp" : : : "cc", "r1");
	asm("mov	r2, lr" : : : "cc", "r2");
	asm("mrs	r3, spsr" : : : "cc", "r2");
	asm("stmea	r0, {r1-r3}" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x12" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.irq_sp) : "cc", "r0");
	asm("mov	r1, sp" : : : "cc", "r1");
	asm("mov	r2, lr" : : : "cc", "r2");
	asm("mrs	r3, spsr" : : : "cc", "r2");
	asm("stmea	r0, {r1-r3}" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x1b" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.und_sp) : "cc", "r0");
	asm("mov	r1, sp" : : : "cc", "r1");
	asm("mov	r2, lr" : : : "cc", "r2");
	asm("mrs	r3, spsr" : : : "cc", "r2");
	asm("stmea	r0, {r1-r3}" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x11" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r8", "r9", "r10", "r11", "r12", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.fiq_sp) : "cc", "r0");
	asm("mov	r1, sp" : : : "cc", "r1");
	asm("mov	r2, lr" : : : "cc", "r2");
	asm("mrs	r3, spsr" : : : "cc", "r2");
	asm("stmea	r0, {r1-r3,r8-r12}" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x13" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r8", "r9", "r10", "r11", "r12", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.svr_sp) : "cc", "r0");
	asm("mov	r1, sp" : : : "cc", "r1");
	asm("mov	r2, lr" : : : "cc", "r2");
	asm("mrs	r3, spsr" : : : "cc", "r2");
	asm("stmea	r0, {r1-r3}" : : : "cc");

	asm("mov	r0, %0" : : "r" (&arch_arm_context.cp15_control) : "cc", "r0");
	asm("mrc	p15, 0, r1, c1, c0, 0" : : : "cc", "r1");
	asm("mrc	p15, 0, r2, c2, c0, 0" : : : "cc", "r2");
	asm("mrc	p15, 0, r3, c3, c0, 0" : : : "cc", "r3");
	asm("stmea	r0, {r1-r3}" : : : "cc");
}
EXPORT_SYMBOL(save_processor_state);

void restore_processor_state(void)
{
	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("orr	r0, r0, #0x1f" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.usr_sp) : "cc", "r0");
	asm("ldmfd	r0, {r1-r2}" : : : "cc", "r0", "r1", "r2");
	asm("mov	sp, r1" : : : "cc", "sp");
	asm("mov	lr, r2" : : : "cc", "lr");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x17" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.abt_sp) : "cc", "r0");
	asm("ldmfd	r0, {r1-r3}" : : : "cc", "r0", "r1", "r2", "r3");
	asm("mov	sp, r1" : : : "cc", "sp");
	asm("mov	lr, r2" : : : "cc", "lr");
	asm("msr	spsr_cxsf, r3" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x12" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.irq_sp) : "cc", "r0");
	asm("ldmfd	r0, {r1-r3}" : : : "cc", "r0", "r1", "r2", "r3");
	asm("mov	sp, r1" : : : "cc", "sp");
	asm("mov	lr, r2" : : : "cc", "lr");
	asm("msr	spsr_cxsf, r3" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x1b" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.und_sp) : "cc", "r0");
	asm("ldmfd	r0, {r1-r3}" : : : "cc", "r0", "r1", "r2", "r3");
	asm("mov	sp, r1" : : : "cc", "sp");
	asm("mov	lr, r2" : : : "cc", "lr");
	asm("msr	spsr_cxsf, r3" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x11" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r8", "r9", "r10", "r11", "r12", "r13", "r14");
	asm("mov	r0, %0" : : "r" (&arch_arm_context.fiq_sp) : "cc", "r0");
	asm("ldmfd	r0, {r1-r3,r8-r12}" : : : "cc", "r0", "r1", "r2", "r3", "r8", "r9", "r10", "r11", "r12");
	asm("mov	sp, r1" : : : "cc", "sp");
	asm("mov	lr, r2" : : : "cc", "lr");
	asm("msr	spsr_cxsf, r3" : : : "cc");

	asm("mrs	r0, cpsr" : : : "cc", "r0");
	asm("bic	r0, r0, #0x1f" : : : "cc", "r0");
	asm("orr	r0, r0, #0x13" : : : "cc", "r0");
	asm("msr	cpsr_cxsf, r0" : : : "cc", "r0", "r8", "r9", "r10", "r11", "r12", "r13", "r14");

	asm("mov	r0, %0" : : "r" (&arch_arm_context.cp15_control) : "cc", "r0");
	asm("ldmfd	r0, {r1-r3}" : : : "cc", "r0", "r1", "r2", "r3");
	asm("mcr	p15, 0, r1, c1, c0, 0" : : : "cc");
	asm("mcr	p15, 0, r2, c2, c0, 0" : : : "cc");
	asm("mcr	p15, 0, r3, c3, c0, 0" : : : "cc");

	flush_tlb_all();
	flush_cache_all();
}
EXPORT_SYMBOL(restore_processor_state);

