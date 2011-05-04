/*
 * arch/arm/mach-ambarella/codec-wm8994.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2011/03/28 - [Cao Rongrong] Created file
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
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/regulator/fixed.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>

static int wm8994_regulator_init(void *driver_data)
{
	return 0;
}

static struct regulator_consumer_supply wm8994_avdd1_supply = {
	.dev_name	= "spi0.1",
	.supply		= "AVDD1",
};

static struct regulator_consumer_supply wm8994_dcvdd_supply = {
	.dev_name	= "spi0.1",
	.supply		= "DCVDD",
};

static struct regulator_init_data wm8994_ldo1_data = {
	.constraints	= {
		.name		= "AVDD1_3.0V",
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_avdd1_supply,
	.regulator_init		= wm8994_regulator_init,
};

static struct regulator_init_data wm8994_ldo2_data = {
	.constraints	= {
		.name		= "DCVDD_1.0V",
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8994_dcvdd_supply,
};

static struct wm8994_pdata ambarella_wm8994_pdata = {
	.ldo2_enable		= GPIO(11),
	/* configure gpio1 function: SDOUT */
	.gpio_defaults[0]	= 0x0102,
	/* configure gpio3/4/5/7 function for AIF2 BT */
	.gpio_defaults[2]	= 0x8100,
	.gpio_defaults[3]	= 0x8100,
	.gpio_defaults[4]	= 0x8100,
	.gpio_defaults[6]	= 0x0100,
	/* configure gpio8/9/10/11 function for AIF3 3G */
	.gpio_defaults[7]	= 0,	//0x8100,
	.gpio_defaults[8]	= 0,	//0x0100,
	.gpio_defaults[9]	= 0,	//0x8100,
	.gpio_defaults[10]	= 0,	//0x8100,
	.ldo[0]			= { GPIO(82), NULL, &wm8994_ldo1_data },
	.ldo[1]			= { 0, NULL, &wm8994_ldo2_data },
};


/* Fixed voltage regulators */

static struct regulator_consumer_supply wm8994_fixed_voltage0_supplies[] = {
	{
		.dev_name	= "spi0.1",
		.supply		= "DBVDD",
	}, {
		.dev_name	= "spi0.1",
		.supply		= "AVDD2",
	}, {
		.dev_name	= "spi0.1",
		.supply		= "CPVDD",
	},
};

static struct regulator_consumer_supply wm8994_fixed_voltage1_supplies[] = {
	{
		.dev_name	= "spi0.1",
		.supply		= "SPKVDD1",
	}, {
		.dev_name	= "spi0.1",
		.supply		= "SPKVDD2",
	},
};

static struct regulator_init_data wm8994_fixed_voltage0_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8994_fixed_voltage0_supplies),
	.consumer_supplies	= wm8994_fixed_voltage0_supplies,
};

static struct regulator_init_data wm8994_fixed_voltage1_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8994_fixed_voltage1_supplies),
	.consumer_supplies	= wm8994_fixed_voltage1_supplies,
};

static struct fixed_voltage_config wm8994_fixed_voltage0_config = {
	.supply_name	= "VCC_1.8V_PDA",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage0_init_data,
};

static struct fixed_voltage_config wm8994_fixed_voltage1_config = {
	.supply_name	= "V_BAT",
	.microvolts	= 3700000,
	.gpio		= -EINVAL,
	.init_data	= &wm8994_fixed_voltage1_init_data,
};

static struct platform_device wm8994_fixed_voltage0 = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage0_config,
	},
};

static struct platform_device wm8994_fixed_voltage1 = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev		= {
		.platform_data	= &wm8994_fixed_voltage1_config,
	},
};

static struct platform_device *wm8994_fixed_voltage_devices[] = {
	&wm8994_fixed_voltage0,
	&wm8994_fixed_voltage1,
};

int ambarella_init_wm8994(struct spi_board_info *board_info, u32 n,
		u16 bus_num, u16 chip_select)
{
	int i, rval = -EINVAL;

	for (i = 0; i < n; i++, board_info++) {
		if (board_info->bus_num == bus_num &&
				board_info->chip_select == chip_select) {
			memset(board_info->modalias, 0, sizeof(board_info->modalias));
			strcpy(board_info->modalias, "wm8994");
			board_info->max_speed_hz = 1000000;
			board_info->platform_data = &ambarella_wm8994_pdata;
			board_info->irq = 0;	// No interrupts from wm8994
			rval = 0;
			break;
		}
	}

	if (rval < 0) {
		pr_err("%s: failed to initial wm8994\n", __func__);
		return rval;
	}

	platform_add_devices(wm8994_fixed_voltage_devices,
			ARRAY_SIZE(wm8994_fixed_voltage_devices));

	return rval;
}

