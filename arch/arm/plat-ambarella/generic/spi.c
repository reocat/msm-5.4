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
#include <linux/moduleparam.h>
#include <linux/clk.h>

#include <mach/hardware.h>
#include <plat/spi.h>
#include <plat/clk.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

#if ( SPI_SLAVE_INSTANCES >= 1 )
struct resource ambarella_spi_slave_resources[] = {
	[0] = {
		.start	= SPI_SLAVE_BASE,
		.end	= SPI_SLAVE_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SSI_SLAVE_IRQ,
		.end	= SSI_SLAVE_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ambarella_spi_slave = {
	.name		= "ambarella-spi-slave",
	.id		= 0,
	.resource	= ambarella_spi_slave_resources,
	.num_resources	= ARRAY_SIZE(ambarella_spi_slave_resources),
	.dev		= {
		.platform_data		= NULL,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif

#if ( SPI_AHB_SLAVE_INSTANCES >= 1 )

static struct clk gclk_ssi3_ahb = {
	.parent		= NULL,
	.name		= "gclk_ssi3_ahb",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= CG_SSI3_REG,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
	.divider	= 0,
	.max_divider	= (1 << 24) - 1,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_scaler_ops,
};

static struct clk *ambarella_ssi3_ahb_register_clk(void)
{
	struct clk *pgclk_ssi_ahb = NULL;
	struct clk *pgclk_ahb = NULL;

	pgclk_ssi_ahb = clk_get(NULL, "gclk_ssi3_ahb");
	if (IS_ERR(pgclk_ssi_ahb)) {
		pgclk_ahb = clk_get(NULL, "pll_out_core");
		if (IS_ERR(pgclk_ahb)) {
			BUG();
		}
		gclk_ssi3_ahb.parent = pgclk_ahb;
		ambarella_clk_add(&gclk_ssi3_ahb);
		pgclk_ssi_ahb = &gclk_ssi3_ahb;
		pr_info("SYSCLK:PWM[%lu]\n", clk_get_rate(pgclk_ssi_ahb));
	}

	return pgclk_ssi_ahb;
}

static void ambarella_ssi3_ahb_set_pll(void)
{
	u32 freq_hz = 13500000;

	clk_set_rate(ambarella_ssi3_ahb_register_clk(), freq_hz);
}

static u32 ambarella_ssi3_ahb_get_pll(void)
{
	return clk_get_rate(ambarella_ssi3_ahb_register_clk());
}

struct ambarella_spi_platform_info ambarella_spi_ahb_slave_platform_info = {
	.fifo_entries		= 16,
	.rct_set_ssi_pll	= ambarella_ssi3_ahb_set_pll,
	.get_ssi_freq_hz	= ambarella_ssi3_ahb_get_pll,
};



struct resource ambarella_spi_slave_resources[] = {
	[0] = {
		.start	= SPI_AHB_SLAVE_BASE,
		.end	= SPI_AHB_SLAVE_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SSI_SLAVE_IRQ,
		.end	= SSI_SLAVE_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ambarella_spi_slave = {
	.name		= "ambarella-spi-slave",
	.id		= 0,
	.resource	= ambarella_spi_slave_resources,
	.num_resources	= ARRAY_SIZE(ambarella_spi_slave_resources),
	.dev		= {
		.platform_data		= &ambarella_spi_ahb_slave_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif


