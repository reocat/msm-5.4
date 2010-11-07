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
struct resource ambarella_ehci_resources[] = {
	[0] = {
		.start	= USB_HOST_CTRL_EHCI_BASE,
		.end	= USB_HOST_CTRL_EHCI_BASE + 0xFFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= USB_EHCI_IRQ,
		.end	= USB_EHCI_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct ambarella_uhc_controller ambarella_platform_ehci_data = {
	.init_pll	= NULL,
};

struct platform_device ambarella_ehci0 = {
	.name		= "ambarella-ehci",
	.id		= -1,
	.resource	= ambarella_ehci_resources,
	.num_resources	= ARRAY_SIZE(ambarella_ehci_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_ehci_data,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

struct resource ambarella_ohci_resources[] = {
	[0] = {
		.start	= USB_HOST_CTRL_OHCI_BASE,
		.end	= USB_HOST_CTRL_OHCI_BASE + 0xFFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= USB_OHCI_IRQ,
		.end	= USB_OHCI_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct ambarella_uhc_controller ambarella_platform_ohci_data = {
	.init_pll	= NULL,
};

struct platform_device ambarella_ohci0 = {
	.name		= "ambarella-ohci",
	.id		= -1,
	.resource	= ambarella_ohci_resources,
	.num_resources	= ARRAY_SIZE(ambarella_ohci_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_ohci_data,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

