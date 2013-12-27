/*
 * arch/arm/plat-ambarella/generic/pinctrl.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
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
#include <linux/pinctrl/machine.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <plat/gpio.h>
#include <plat/pinctrl.h>

/* Pin control settings */
static struct pinctrl_map __initdata amb_pinmux_map[] = {
	PIN_MAP_MUX_GROUP_DEFAULT("ambarella-i2s.0", "amb-pinctrl", NULL, "i2s0"),
};

int __init ambarella_init_pinctrl(void)
{
	int					retval = 0;

	/* Initialize pinmuxing */
	pinctrl_register_mappings(amb_pinmux_map,
			ARRAY_SIZE(amb_pinmux_map));

	return retval;
}

static int ambarella_pinctrl_set_altfunc(int pin,
			enum amb_pin_altfunc altfunc, u32 multi_alt)
{
#ifdef CONFIG_PLAT_AMBARELLA_SUPPORT_IOMUX
	u32 bank, offset, i, reg, data;

	bank = PINID_TO_BANK(pin);
	offset = PINID_TO_OFFSET(pin);

	for (i = 0; i < 3; i++) {
		reg = IOMUX_REG(IOMUX_REG_OFFSET(bank, i));
		data = amba_readl(reg) & (~(0x1 << offset));
		amba_writel(reg, data | (((altfunc >> i) & 0x1) << offset));
	}
#else
	if (multi_alt) {
		switch(pin) {
		case GPIO(79):
		case GPIO(80):
			if (altfunc == AMB_ALTFUNC_HW_1)
				amba_clrbitsl(GPIO3_REG(GPIO_AFSEL_OFFSET), 0x00000030);
			else
				amba_setbitsl(GPIO3_REG(GPIO_AFSEL_OFFSET), 0x00000030);
			break;
		}
	}
#endif
	return 0;
}

static struct ambarella_platform_pinctrl amb_pinctrl_pdata = {
	.set_pin_altfunc = ambarella_pinctrl_set_altfunc,
};

static struct resource ambarella_pinctrl0_resources[] = {
	[0] = {
		.start	= GPIO0_BASE,
		.end	= GPIO0_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= GPIO1_BASE,
		.end	= GPIO1_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= GPIO2_BASE,
		.end	= GPIO2_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= GPIO3_BASE,
		.end	= GPIO3_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[4] = {
		.start	= GPIO4_BASE,
		.end	= GPIO4_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[5] = {
		.start	= GPIO5_BASE,
		.end	= GPIO5_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device ambarella_pinctrl0 = {
	.name		= "amb-pinctrl",
	.id		= -1,
	.resource	= ambarella_pinctrl0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_pinctrl0_resources),
	.dev		= {
		.platform_data		= &amb_pinctrl_pdata,
	}
};

