/*
 * arch/arm/plat-ambarella/generic/wdt.c
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

#include <mach/hardware.h>
#include <plat/wdt.h>

/* ==========================================================================*/
static struct resource ambarella_wdt0_resources[] = {
	[0] = {
		.start	= WDOG_BASE,
		.end	= WDOG_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= WDT_IRQ,
		.end	= WDT_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static void ambarella_wdt0_start(u32 mode)
{
	if ((mode & WDOG_CTR_RST_EN) == WDOG_CTR_RST_EN) {
	
		/* Allow the change of WDT_RST_L_REG via APB */  
#if (RCT_SUPPORT_UNL_WDT_RST_ANAPWR == 1)
		amba_writel(ANA_PWR_REG, amba_readl(ANA_PWR_REG) | 0x80);
#endif	

		/* Clear the WDT_RST_L_REG to zero */
		amba_writel(WDT_RST_L_REG, RCT_WDT_RESET_VAL);

		/* Not allow the change of WDT_RST_L_REG via APB */
#if (RCT_SUPPORT_UNL_WDT_RST_ANAPWR == 1)
		amba_writel(ANA_PWR_REG, amba_readl(ANA_PWR_REG) & (~0x80));
#endif

		/* Clear software reset bit. */
#if defined(RCT_SOFT_OR_DLLRESET_PATTERN)
		amba_writel(SOFT_RESET_REG, RCT_SOFT_OR_DLLRESET_PATTERN);
#else
		amba_writel(SOFT_RESET_REG, 0x2);
#endif
	}
	amba_writel(WDOG_CONTROL_REG, mode);
	while(amba_tstbitsl(WDOG_CONTROL_REG, mode) != mode);
}

static struct ambarella_wdt_controller ambarella_platform_wdt_controller0 = {
	.get_pll		= get_apb_bus_freq_hz,
	.start			= ambarella_wdt0_start,
};

struct platform_device ambarella_wdt0 = {
	.name		= "ambarella-wdt",
	.id		= -1,
	.resource	= ambarella_wdt0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_wdt0_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_wdt_controller0,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

#if defined(CONFIG_PLAT_AMBARELLA_CORTEX)
//----------------mpcore watchdog------------------------------------
static struct resource mpcore_wdt_resources[] = {
	[0] = {
		.start	= MPCORE_WDT_REG,
		.end	= MPCORE_WDT_REG + 0x00FF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= LOCAL_WDOG_IRQ,
		.end	= LOCAL_WDOG_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device mpcore_wdt = {
	.name		= "mpcore_wdt",
	.id		= -1,
	.resource	= mpcore_wdt_resources,
	.num_resources	= ARRAY_SIZE(mpcore_wdt_resources),
	.dev		= {
	}
};
#endif

