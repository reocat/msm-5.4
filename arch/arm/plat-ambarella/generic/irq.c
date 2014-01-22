/*
 * arch/arm/plat-ambarella/generic/irq.c
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2007/11/29 - [Grady Chen] added VIC2, GPIO upper level VIC control
 *	2009/01/06 - [Anthony Ginger] Port to 2.6.28
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/module.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>

#include <linux/irqchip/arm-gic.h>

/* ==========================================================================*/
void __init ambarella_init_irq(void)
{

}

