/*
 * arch/arm/plat-ambarella/generic/eth.c
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
#include <linux/ethtool.h>

#include <mach/hardware.h>
#include <plat/eth.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/
static int ambarella_eth0_is_supportclk(void)
{
#if (SUPPORT_GMII == 1)
	return 1;
#else
	return 0;
#endif
}

static void ambarella_eth0_setclk(u32 freq)
{
#if (SUPPORT_GMII == 1)
	amba_writel(RCT_REG(0x2ac), 0x01);
#endif
}

static u32 ambarella_eth0_getclk(void)
{
	return 125000000;
}

static u32 ambarella_eth0_get_supported(void)
{
	u32			supported;

	supported = (SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full | \
		SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full | \
		SUPPORTED_Autoneg | SUPPORTED_MII);
#if (SUPPORT_GMII == 1)
	supported |= (SUPPORTED_1000baseT_Half | SUPPORTED_1000baseT_Full);
#endif

	return supported;
}

static struct resource ambarella_eth0_resources[] = {
	[0] = {
		.start	= ETH_BASE,
		.end	= ETH_BASE + 0x1FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= ETH_IRQ,
		.end	= ETH_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct ambarella_eth_platform_info ambarella_eth0_platform_info = {
	.mac_addr		= {0, 0, 0, 0, 0, 0},
	.napi_weight		= 64,
	.max_work_count		= 5,
	.mii_id			= -1,
	.phy_id			= 0x00008201,
	.mii_power	= {
		.gpio_id	= -1,
		.active_level	= GPIO_LOW,
		.active_delay	= 1,
	},
	.mii_reset	= {
		.gpio_id	= -1,
		.active_level	= GPIO_LOW,
		.active_delay	= 1,
	},
	.default_clk		= 125000000,
	.default_1000_clk	= 125000000,
	.default_100_clk	= 125000000,
	.default_10_clk		= 125000000,
	.is_enabled		= rct_is_eth_enabled,
	.is_supportclk		= ambarella_eth0_is_supportclk,
	.setclk			= ambarella_eth0_setclk,
	.getclk			= ambarella_eth0_getclk,
	.get_supported		= ambarella_eth0_get_supported,
};
AMBA_ETH_PARAM_CALL(0, ambarella_eth0_platform_info, 0644);

/* ==========================================================================*/
int __init ambarella_init_eth0(unsigned int high, unsigned int low)
{
	int					errCode = 0;

	ambarella_eth0_platform_info.mac_addr[0] = (low >> 0);
	ambarella_eth0_platform_info.mac_addr[1] = (low >> 8);
	ambarella_eth0_platform_info.mac_addr[2] = (low >> 16);
	ambarella_eth0_platform_info.mac_addr[3] = (low >> 24);
	ambarella_eth0_platform_info.mac_addr[4] = (high >> 0);
	ambarella_eth0_platform_info.mac_addr[5] = (high >> 8);

	return errCode;
}

/* ==========================================================================*/
struct platform_device ambarella_eth0 = {
	.name		= "ambarella-eth",
	.id		= 0,
	.resource	= ambarella_eth0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_eth0_resources),
	.dev		= {
		.platform_data		= &ambarella_eth0_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

