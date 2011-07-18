/*
 * arch/arm/mach-boss/timer.c
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2008/01/08 - [Anthony Ginger] Rewrite for 2.6.28
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

#include <linux/interrupt.h>
#include <linux/clockchips.h>
#include <linux/delay.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>
#include <mach/boss.h>

/* ==========================================================================*/

static irqreturn_t ambarella_timer_interrupt(int irq, void *dev_id)
{
	timer_tick();

	return IRQ_HANDLED;
}

static struct irqaction ambarella_timer_irq = {
	.name		= "BOSS Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_TRIGGER_RISING,
	.handler	= ambarella_timer_interrupt,
};

static void ambarella_timer_init(void)
{
	setup_irq(BOSS_VIRT_TIMER_INT_VEC, &ambarella_timer_irq);
}

u32 ambarella_timer_suspend(u32 level)
{
	return 0;
}

u32 ambarella_timer_resume(u32 level)
{
	return 0;
}


struct sys_timer ambarella_timer = {
	.init		= ambarella_timer_init,
};

