/*
 * arch/arm/include/asm/suspend.h
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

#ifndef _ASM_ARM_SUSPEND_H
#define _ASM_ARM_SUSPEND_H

static inline int arch_prepare_suspend(void) { return 0; }
extern int arch_pfn_is_nosave(unsigned long pfn);

struct saved_context {
	u32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12;
	u32 usr_sp, usr_lr;
	u32 svr_sp, svr_lr, svr_spsr;
	u32 abt_sp, abt_lr, abt_spsr;
	u32 irq_sp, irq_lr, irq_spsr;
	u32 und_sp, und_lr, und_spsr;
	u32 fiq_sp, fiq_lr, fiq_spsr;
	u32 fiq_r8, fiq_r9, fiq_r10, fiq_r11, fiq_r12;
	u32 cp15_control;
	u32 cp15_ttbr;
	u32 cp15_dacl;
} __attribute__((packed));

extern asmlinkage int arch_save_processor_state(struct saved_context *sc);
extern asmlinkage int arch_restore_processor_state(struct saved_context *sc);

#endif /* _ASM_ARM_SUSPEND_H */

