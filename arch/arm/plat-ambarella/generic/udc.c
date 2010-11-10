/*
 * arch/arm/plat-ambarella/generic/udc.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include <mach/hardware.h>
#include <plat/udc.h>

/* ==========================================================================*/
struct resource ambarella_udc_resources[] = {
	[0] = {
		.start	= USBDC_BASE,
		.end	= USBDC_BASE + 0x1FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= USBC_IRQ,
		.end	= USBC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static void init_usb(void)
{
	struct irq_desc *desc = irq_to_desc(TIMER1_IRQ);

	if (desc && desc->chip)
		desc->chip->set_type(USBVBUS_IRQ, IRQ_TYPE_LEVEL_HIGH);

	_init_usb_pll();
}

static void reset_usb(void)
{
#if (CHIP_REV == A5S)||(CHIP_REV == A7)||(CHIP_REV == I1)
	rct_usb_reset();
#endif
}

static struct ambarella_udc_controller ambarella_platform_udc_controller0 = {
	.init_pll	= init_usb,
	.reset_usb	= reset_usb,
};

struct platform_device ambarella_udc0 = {
	.name		= "ambarella-udc",
	.id		= -1,
	.resource	= ambarella_udc_resources,
	.num_resources	= ARRAY_SIZE(ambarella_udc_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_udc_controller0,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

