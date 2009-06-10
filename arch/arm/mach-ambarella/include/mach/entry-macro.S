/*
 * arch/arm/mach-ambarella/include/mach/entry-macro.S
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2007/12/27 - [Grady Chen] Added VIC2, GPIO0, GPIO1, GPIO2
 *	2009/01/07 - [Anthony Ginger] Move GPIO support to irq.c.
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

		.macro  disable_fiq
		.endm

		.macro	get_irqnr_preamble, base, tmp
		.endm

		.macro	arch_ret_to_user, tmp1, tmp2
		.endm

		.macro  get_irqnr_and_base, irqnr, irqstat, base, tmp
		mov     \base, #AHB_BASE
		orr	\tmp, \base, #VIC_OFFSET
		ldr     \irqstat, [\tmp, #VIC_IRQ_STA_OFFSET]
		mov     \irqnr, #VIC_INT_VEC_OFFSET
1001:
		tst     \irqstat, #1
		addeq   \irqnr, \irqnr, #1
		moveq   \irqstat, \irqstat, lsr #1
		tsteq   \irqnr, #NR_VIC_IRQ_SIZE
		beq     1001b
		teq     \irqnr, #NR_VIC_IRQ_SIZE
#if (VIC_INSTANCES >= 2)
		bne	1003f
		orr	\tmp, \base, #VIC2_OFFSET
		ldr	\irqstat, [\tmp, #VIC_IRQ_STA_OFFSET]
/*		mov	\irqnr, #VIC2_INT_VEC_OFFSET	//Omit, irqnr is continus */
1002:
		tst     \irqstat, #1
		addeq   \irqnr, \irqnr, #1
		moveq   \irqstat, \irqstat, lsr #1
		tsteq   \irqnr, #GPIO_INT_VEC_OFFSET
		beq     1002b
		teq     \irqnr, #GPIO_INT_VEC_OFFSET
#endif
1003:
		.endm

