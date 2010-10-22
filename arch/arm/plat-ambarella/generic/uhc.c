/*
 * arch/arm/plat-ambarella/generic/uhc.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
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

#include <mach/hardware.h>
#include <plat/uhc.h>

/* ==========================================================================*/
struct resource ambarella_uhc_resources[] = {
#if 0
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
#endif
};

static struct ambarella_uhc_controller ambarella_platform_uhc_controller0 = {
	.init_pll	= NULL,
};

struct platform_device ambarella_uhc0 = {
	.name		= "ambarella-uhc",
	.id		= -1,
	.resource	= ambarella_uhc_resources,
	.num_resources	= ARRAY_SIZE(ambarella_uhc_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_uhc_controller0,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

