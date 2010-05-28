/*
 * arch/arm/plat-ambarella/include/plat/irq.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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

#ifndef __PLAT_AMBARELLA_IRQ_H
#define __PLAT_AMBARELLA_IRQ_H

/* ==========================================================================*/
#define NR_VIC_IRQ_SIZE			(32)

#if (GPIO_INSTANCES >= 4)
#define GPIO_IRQ_MASK			(0xbbfff3ff)
#elif (GPIO_INSTANCES >= 3)
#define GPIO_IRQ_MASK			(0xbffff3ff)
#else
#define GPIO_IRQ_MASK			(0xfffff3ff)
#endif

/* ==========================================================================*/
#ifndef __ASSEMBLER__

/* ==========================================================================*/

/* ==========================================================================*/
static inline int gpio_to_irq(unsigned gpio)
{
	if (gpio < GPIO_MAX_LINES)
		return GPIO_INT_VEC(gpio);

	return -EINVAL;
}

static inline int irq_to_gpio(unsigned irq)
{
	if ((irq > GPIO_INT_VEC(0)) && (irq < MAX_IRQ_NUMBER))
		return irq - GPIO_INT_VEC(0);

	return -EINVAL;
}

extern void ambarella_init_irq(void);
extern void ambarella_gpio_ack_irq(unsigned int irq);

extern u32 ambarella_irq_suspend(u32 level);
extern u32 ambarella_irq_resume(u32 level);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

