/*
 *  linux/irqchip/irq-ambarella-vic.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2013, Ambarella, Inc.
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

#ifndef __IRQ_AMBARELLA_VIC_H__
#define __IRQ_AMBARELLA_VIC_H__

void ambarella_vic_init_irq(void __iomem *vic_base, unsigned int irq_start);
void ambarella_vic_add_irq(void __iomem *vic_base, unsigned int irq_start);

void ambarella_vic_sw_set(void __iomem *vic_base, unsigned int vic_irq);
void ambarella_vic_sw_clr(void __iomem *vic_base, unsigned int vic_irq);

#endif // __IRQ_AMBARELLA_VIC_H__

