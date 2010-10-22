/*
 * arch/arm/plat-ambarella/generic/idc.c
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
#include <linux/i2c.h>

#include <mach/hardware.h>
#include <plat/idc.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/
#define DEFAULT_I2C_CLASS	(I2C_CLASS_HWMON | I2C_CLASS_TV_ANALOG | I2C_CLASS_TV_DIGITAL | I2C_CLASS_SPD)

struct resource ambarella_idc0_resources[] = {
	[0] = {
		.start	= IDC_BASE,
		.end	= IDC_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IDC_IRQ,
		.end	= IDC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

#if (IDC_SUPPORT_PIN_MUXING_FOR_HDMI == 1)
void ambarella_idc_set_pin_muxing(u32 on)
{
	if (on)
		ambarella_gpio_config(IDC_BUS_HDMI, GPIO_FUNC_HW);
	else
		ambarella_gpio_config(IDC_BUS_HDMI, GPIO_FUNC_SW_OUTPUT);
}
#elif (IDC_SUPPORT_INTERNAL_MUX == 1)
void ambarella_idc_set_pin_muxing(u32 on)
{
	if (on)
		ambarella_gpio_config(IDC3_BUS_MUX, GPIO_FUNC_HW);
	else
		ambarella_gpio_config(IDC3_BUS_MUX, GPIO_FUNC_SW_INPUT);
}
#endif

struct ambarella_idc_platform_info ambarella_idc0_platform_info = {
	.clk_limit	= 100000,
	.bulk_write_num	= 60,
#if (IDC_SUPPORT_PIN_MUXING_FOR_HDMI == 1)
	.i2c_class	= DEFAULT_I2C_CLASS | I2C_CLASS_DDC,
	.set_pin_muxing	= ambarella_idc_set_pin_muxing,
#elif (IDC_SUPPORT_INTERNAL_MUX == 1)
	.i2c_class	= DEFAULT_I2C_CLASS,
	.set_pin_muxing	= ambarella_idc_set_pin_muxing,
#else
	.i2c_class	= DEFAULT_I2C_CLASS,
	.set_pin_muxing	= NULL,
#endif
	.get_clock	= get_apb_bus_freq_hz,
};
AMBA_IDC_PARAM_CALL(0, ambarella_idc0_platform_info, 0644);

struct platform_device ambarella_idc0 = {
	.name		= "ambarella-i2c",
	.id		= 0,
	.resource	= ambarella_idc0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_idc0_resources),
	.dev		= {
		.platform_data		= &ambarella_idc0_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

#if (IDC_INSTANCES >= 2)
struct resource ambarella_idc1_resources[] = {
	[0] = {
		.start	= IDC2_BASE,
		.end	= IDC2_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IDC2_IRQ,
		.end	= IDC2_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct ambarella_idc_platform_info ambarella_idc1_platform_info = {
	.clk_limit	= 100000,
	.bulk_write_num	= 60,
	.i2c_class	= I2C_CLASS_DDC,
	.set_pin_muxing	= NULL,
	.get_clock	= get_apb_bus_freq_hz,
};
AMBA_IDC_PARAM_CALL(1, ambarella_idc1_platform_info, 0644);

struct platform_device ambarella_idc1 = {
	.name		= "ambarella-i2c",
	.id		= 1,
	.resource	= ambarella_idc1_resources,
	.num_resources	= ARRAY_SIZE(ambarella_idc1_resources),
	.dev		= {
		.platform_data		= &ambarella_idc1_platform_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif

