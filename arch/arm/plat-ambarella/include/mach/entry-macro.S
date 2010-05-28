/*
 * arch/arm/plat-ambarella/include/mach/entry-macro.S
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2007/12/27 - [Grady Chen] Added VIC2, GPIO0, GPIO1, GPIO2
 *	2009/06/15 - [Anthony Ginger] Optimize with clz.
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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

#include <mach/hardware.h>
#include <asm/irq.h>

		.macro	stat2nr, stat, nr, tmp
		rsbs	\tmp, \stat, #0
		and	\nr, \tmp, \stat
		clzcc	\nr, \nr
		rsc	\nr, \nr, #32
		teq	\nr, #32
		.endm

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_preamble, base, tmp
		.endm

		.macro	arch_ret_to_user, tmp1, tmp2
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		ldr	\base, =(VIC_BASE)
		ldr	\tmp, =(GPIO_IRQ_MASK)
		ldr	\irqstat, [\base, #VIC_IRQ_STA_OFFSET]
		and	\base, \irqstat, \tmp
		stat2nr	\base, \irqnr, \tmp
		bne	1000f
#if (VIC_INSTANCES >= 2)
		ldr	\base, =(VIC2_BASE)
		ldr	\irqnr, [\base, #VIC_IRQ_STA_OFFSET]
		stat2nr	\irqnr, \irqnr, \tmp
		addne	\irqnr, \irqnr, #VIC2_INT_VEC_OFFSET
		bne	1000f
#endif
		stat2nr	\irqstat, \irqnr, \tmp
1000:
		.endm

