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
#include <linux/irqchip/irq-ambarella-vic.h>
#include <linux/irqchip/irq-ambarella-gpio.h>

/* ==========================================================================*/
void __init ambarella_init_irq(void)
{
#if defined(CONFIG_ARM_GIC)
	gic_init(0, LOCAL_TIMER_IRQ, __io(AMBARELLA_VA_GIC_DIST_BASE),
		__io(AMBARELLA_VA_GIC_CPU_BASE));
#endif /* CONFIG_ARM_GIC */
#if defined(CONFIG_AMBARELLA_VIC)
	ambarella_vic_init_irq((void __iomem *)VIC_REG(0), VIC_INT_VEC(0));
#if (VIC_INSTANCES >= 2)
	ambarella_vic_add_irq((void __iomem *)VIC2_REG(0), VIC2_INT_VEC(0));
#endif
#if (VIC_INSTANCES >= 3)
	ambarella_vic_add_irq((void __iomem *)VIC3_REG(0), VIC3_INT_VEC(0));
#endif
#endif /* CONFIG_AMBARELLA_VIC */
#if defined(CONFIG_AMBARELLA_GVIC)
	ambarella_gvic_init_irq((void __iomem *)GPIO0_REG(0),
		GPIO_INT_VEC(32 * 0), GPIO0_IRQ, IRQ_TYPE_LEVEL_HIGH);
#if (GPIO_INSTANCES >= 2)
	ambarella_gvic_add_irq((void __iomem *)GPIO1_REG(0),
		GPIO_INT_VEC(32 * 1), GPIO1_IRQ, IRQ_TYPE_LEVEL_HIGH);
#endif
#if (GPIO_INSTANCES >= 3)
	ambarella_gvic_add_irq((void __iomem *)GPIO2_REG(0),
		GPIO_INT_VEC(32 * 2), GPIO2_IRQ, IRQ_TYPE_LEVEL_HIGH);
#endif
#if (GPIO_INSTANCES >= 4)
	ambarella_gvic_add_irq((void __iomem *)GPIO3_REG(0),
		GPIO_INT_VEC(32 * 3), GPIO3_IRQ, IRQ_TYPE_LEVEL_HIGH);
#endif
#if (GPIO_INSTANCES >= 5)
	ambarella_gvic_add_irq((void __iomem *)GPIO4_REG(0),
		GPIO_INT_VEC(32 * 4), GPIO4_IRQ, IRQ_TYPE_LEVEL_HIGH);
#endif
#if (GPIO_INSTANCES >= 6)
	ambarella_gvic_add_irq((void __iomem *)GPIO5_REG(0),
		GPIO_INT_VEC(32 * 5), GPIO5_IRQ, IRQ_TYPE_LEVEL_HIGH);
#endif
#endif /* CONFIG_AMBARELLA_GVIC */
}

