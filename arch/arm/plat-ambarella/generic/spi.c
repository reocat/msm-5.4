/*
 * arch/arm/plat-ambarella/generic/spi.c
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
#include <plat/spi.h>

/* ==========================================================================*/
#define SPI0_CS2_CS3_EN				0x00000002
void ambarella_spi_cs_activate(struct ambarella_spi_cs_config *cs_config)
{
	u8			cs_pin;

	if (cs_config->bus_id >= SPI_INSTANCES || cs_config->cs_id >= cs_config->cs_num)
		return;

	cs_pin = cs_config->cs_pins[cs_config->cs_id];
	if (cs_config->cs_change) {
		if (cs_config->bus_id == 0 && (cs_config->cs_id == 2 || cs_config->cs_id == 3))
			amba_writel(HOST_ENABLE_REG, amba_readl(HOST_ENABLE_REG) | SPI0_CS2_CS3_EN);
		ambarella_gpio_config(cs_pin, GPIO_FUNC_HW);
	} else {
		if (cs_config->bus_id == 0 && (cs_config->cs_id == 2 || cs_config->cs_id == 3))
			amba_writel(HOST_ENABLE_REG, amba_readl(HOST_ENABLE_REG) & ~SPI0_CS2_CS3_EN);
		ambarella_gpio_config(cs_pin, GPIO_FUNC_SW_OUTPUT);
		ambarella_gpio_set(cs_pin, 0);
	}
}

void ambarella_spi_cs_deactivate(struct ambarella_spi_cs_config *cs_config)
{
	u8			cs_pin;

	if (cs_config->bus_id >= SPI_INSTANCES || cs_config->cs_id >= cs_config->cs_num)
		return;

	cs_pin = cs_config->cs_pins[cs_config->cs_id];
	if (cs_config->cs_change)
		return;
	else
		ambarella_gpio_set(cs_pin, 1);
}

struct resource ambarella_spi0_resources[] = {
	[0] = {
		.start	= SPI_BASE,
		.end	= SPI_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SSI_IRQ,
		.end	= SSI_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

int ambarella_spi0_cs_pins[] = {
	SSI0_EN0, SSI0_EN1, SSIO_EN2, SSIO_EN3,
	-1, -1, -1, -1};
struct ambarella_spi_platform_info ambarella_spi0_platform_info = {
	.use_interrupt		= 1,
	.cs_num			= ARRAY_SIZE(ambarella_spi0_cs_pins),
	.cs_pins		= ambarella_spi0_cs_pins,
	.cs_activate		= ambarella_spi_cs_activate,
	.cs_deactivate		= ambarella_spi_cs_deactivate,
	.rct_set_ssi_pll	= rct_set_ssi_pll,
	.get_ssi_freq_hz	= get_ssi_freq_hz,
};

struct platform_device ambarella_spi0 = {
	.name		= "ambarella-spi",
	.id		= 0,
	.resource	= ambarella_spi0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_spi0_resources),
	.dev		= {
		.platform_data		= &ambarella_spi0_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

#if (SPI_INSTANCES >= 2 )
struct resource ambarella_spi1_resources[] = {
	[0] = {
		.start	= SPI2_BASE,
		.end	= SPI2_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SSI2_IRQ,
		.end	= SSI2_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

int ambarella_spi1_cs_pins[] = {SSI_4_N, -1, -1, -1, -1, -1, -1, -1};
struct ambarella_spi_platform_info ambarella_spi1_platform_info = {
	.use_interrupt		= 1,
	.cs_num			= ARRAY_SIZE(ambarella_spi1_cs_pins),
	.cs_pins		= ambarella_spi1_cs_pins,
	.cs_activate		= ambarella_spi_cs_activate,
	.cs_deactivate		= ambarella_spi_cs_deactivate,
	.rct_set_ssi_pll	= rct_set_ssi2_pll,
	.get_ssi_freq_hz	= get_ssi2_freq_hz,
};

struct platform_device ambarella_spi1 = {
	.name		= "ambarella-spi",
	.id		= 1,
	.resource	= ambarella_spi1_resources,
	.num_resources	= ARRAY_SIZE(ambarella_spi1_resources),
	.dev		= {
		.platform_data		= &ambarella_spi1_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};
#endif

