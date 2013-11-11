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
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/export.h>

#include <mach/hardware.h>
#include <plat/uport.h>
#include <plat/udc.h>
#include <plat/rct.h>

/* ==========================================================================*/
/* amb_udc_status will be modified in ambarella_udc.c */
enum ambarella_udc_status amb_udc_status = AMBARELLA_UDC_STATUS_UNKNOWN;
EXPORT_SYMBOL(amb_udc_status);

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

static void enable_phy(void)
{
	ambarella_enable_usb_port(UDC_OWN_PORT);
}

static void disable_phy(void)
{
	ambarella_disable_usb_port(UDC_OWN_PORT);
}

static void reset_usb(void)
{
	u32 val;

	val = amba_rct_readl(ANA_PWR_REG);
	/* force usbphy on */
	amba_rct_writel(ANA_PWR_REG, val | 0x4);
	udelay(1);
	/* UDC soft reset */
	amba_rct_setbitsl(USBP1_CTRL_REG, 0x20000000);
	udelay(1);
	amba_rct_clrbitsl(USBP1_CTRL_REG, 0x20000000);
	udelay(1);
	/* restore ana_pwr_reg */
	amba_rct_writel(ANA_PWR_REG, val);
	udelay(1);
}

static int flush_rxfifo(void)
{
	int retry_count = 1000;
	int rval = 0;

	amba_setbitsl(USB_DEV_CTRL_REG, USB_DEV_NAK);
	amba_setbitsl(USB_DEV_CTRL_REG, USB_DEV_FLUSH_RXFIFO);
	while(!(amba_readl(USB_DEV_STS_REG) & USB_DEV_RXFIFO_EMPTY_STS)) {
		if (retry_count-- < 0) {
			printk (KERN_ERR "%s: failed", __func__);
			rval = -1;
			break;
		}
		udelay(5);
	}
	amba_clrbitsl(USB_DEV_CTRL_REG, USB_DEV_NAK);

	return rval;
}

static struct ambarella_udc_controller ambarella_platform_udc_controller0 = {
	.vbus_polled	= 1,	/* need to poll vbus(usb cable) status */
	.irqflags	= IRQF_DISABLED | IRQF_TRIGGER_HIGH,
#if defined(CONFIG_PLAT_AMBARELLA_S2_CORTEX)
	.dma_fix	= 0xC0000000,
#else
	.dma_fix	= 0,
#endif
	.enable_phy	= enable_phy,
	.disable_phy	= disable_phy,
	.reset_usb	= reset_usb,
	.flush_rxfifo	= flush_rxfifo,
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

