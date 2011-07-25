/*
 * arch/arm/plat-ambarella/boss/boss.c
 *
 * Author: Henry Lin <hllin@ambarella.com>
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/aipc/ipc_slock.h>
#include <mach/boss.h>

#define BOSS_IRQ_OWNER_UITRON	0
#define BOSS_IRQ_OWNER_LINUX	1

/*
 * Get the owner of a BOSS IRQ
 */
int boss_get_irq_owner(int irq)
{
	unsigned int flags;
	int owner = BOSS_IRQ_OWNER_UITRON;

	ipc_spin_lock(boss->lock, &flags, 0);

	if (irq < 32) {
		if (boss->vic1mask & (0x1 << irq)) {
			owner = BOSS_IRQ_OWNER_LINUX;
		}
	}
	else if (irq < 64) {
		if (boss->vic2mask & (0x1 << (irq - 32))) {
			owner = BOSS_IRQ_OWNER_LINUX;
		}
	}
	else if (irq < 96) {
		if (boss->vic3mask & (0x1 << (irq - 64))) {
			owner = BOSS_IRQ_OWNER_LINUX;
		}
	}

	ipc_spin_unlock(boss->lock, flags, 0);

	return owner;
}
EXPORT_SYMBOL(boss_get_irq_owner);

/*
 * Set Linux as the owner of an BOSS IRQ
 */
void boss_enable_irq(int irq)
{
	unsigned int flags;

	ipc_spin_lock(boss->lock, &flags, 0);

	if (irq < 32) {
		boss->vic1mask |= (0x1 << irq);
	}
	else if (irq < 64) {
		boss->vic2mask |= (0x1 << (irq - 32));
	}
	else if (irq < 96) {
		boss->vic3mask |= (0x1 << (irq - 64));
	}

	ipc_spin_unlock(boss->lock, flags, 0);
}
EXPORT_SYMBOL(boss_enable_irq);

/*
 * Set uITRON as the owner of an BOSS IRQ
 */
void boss_disable_irq(int irq)
{
	unsigned int flags;

	ipc_spin_lock(boss->lock, &flags, 0);

	if (irq < 32) {
		boss->vic1mask &= ~(0x1 << irq);
	}
	else if (irq < 64) {
		boss->vic2mask &= ~(0x1 << (irq - 32));
	}
	else if (irq < 96) {
		boss->vic3mask &= ~(0x1 << (irq - 64));
	}

	ipc_spin_unlock(boss->lock, flags, 0);
}
EXPORT_SYMBOL(boss_disable_irq);

