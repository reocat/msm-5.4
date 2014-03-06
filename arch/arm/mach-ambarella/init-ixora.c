/*
 * arch/arm/mach-ambarella/init-ixora.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2013, Ambarella, Inc.
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
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/irqchip.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/gpio.h>
#include <asm/system_info.h>

#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/board.h>
#include <mach/common.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/mmc/host.h>

#include <plat/ambinput.h>
#include <plat/ambcache.h>

#include "board-device.h"

#include <plat/dma.h>
#include <plat/clk.h>

#include <linux/input.h>
/* ==========================================================================*/
static struct platform_device *ixora_devices[] __initdata = {
	&ambarella_crypto,
	&ambarella_fb0,
	&ambarella_fb1,
	&ambarella_ir0,
	&ambarella_wdt0,
	&ambarella_rtc0,
	&ambarella_spi0,
	&ambarella_spi1,
	&ambarella_pwm_backlight_device0,
	&ambarella_pwm_backlight_device1,
	&ambarella_pwm_backlight_device2,
	&ambarella_pwm_backlight_device3,
};

/* ==========================================================================*/
static struct spi_board_info ambarella_spi_devices[] = {
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 1,
	},

	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 2,
	},
#if 0
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 3,
	},

	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 1,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 2,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 3,
	},
#endif
};


/* ==========================================================================*/
static struct ambarella_key_table generic_keymap[AMBINPUT_TABLE_SIZE] = {

	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_POWER,		1,		0x0100bcbd}}},	//POWER
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_VOLUMEUP,	1,		0x01000405}}},	//VOLUME_UP
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_VOLUMEDOWN,	1,		0x01008485}}},	//VOLUME_DOWN

	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_END},
};

static struct ambarella_input_board_info ixora_board_input_info = {
	.pkeymap		= generic_keymap,
	.pinput_dev		= NULL,
	.pdev			= NULL,

	.abx_max_x		= 4095,
	.abx_max_y		= 4095,
	.abx_max_pressure	= 4095,
	.abx_max_width		= 16,
};

struct platform_device ixora_board_input = {
	.name		= "ambarella-input",
	.id		= -1,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &ixora_board_input_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

/* ==========================================================================*/
static void __init ambarella_init_ixora(void)
{
	int use_bub_default = 1;

	ambarella_init_machine("ixora", REF_CLK_FREQ);
#ifdef CONFIG_OUTER_CACHE
	ambcache_l2_enable();
#endif

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_IPCAM) {
		switch (AMBARELLA_BOARD_REV(system_rev)) {
		case 'A':
			use_bub_default = 0;
			break;

		default:
			pr_warn("%s: Unknown IPCAM Rev[%d]\n", __func__,
				AMBARELLA_BOARD_REV(system_rev));
			break;
		}
	}

	ambarella_platform_ir_controller0.protocol = AMBA_IR_PROTOCOL_PANASONIC;
}

/* ==========================================================================*/

static struct of_dev_auxdata ambarella_auxdata_lookup[] __initdata = {
	{}
};

static void __init ambarella_init_ixora_dt(void)
{
	int i;

	ambarella_init_ixora();

	of_platform_populate(NULL, of_default_bus_match_table, ambarella_auxdata_lookup, NULL);

	platform_add_devices(ixora_devices, ARRAY_SIZE(ixora_devices));
	for (i = 0; i < ARRAY_SIZE(ixora_devices); i++) {
		device_set_wakeup_capable(&ixora_devices[i]->dev, 1);
		device_set_wakeup_enable(&ixora_devices[i]->dev, 0);
	}

	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);

#if defined(CONFIG_VOUT_CONVERTER_IT66121)
	ambarella_init_it66121(0, 0x98>>1);
#endif

	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));

	platform_device_register(&ixora_board_input);
}


static const char * const s2l_dt_board_compat[] = {
	"ambarella,s2l",
	"ambarella,hawthorn",
	"ambarella,ixora",
	NULL,
};

DT_MACHINE_START(IXORA_DT, "Ambarella S2L (Flattened Device Tree)")
	.restart_mode	=	's',
	.map_io		=	ambarella_map_io,
	.init_early	=	ambarella_init_early,
	.init_irq	=	irqchip_init,
	.init_time	=	ambarella_timer_init,
	.init_machine	=	ambarella_init_ixora_dt,
	.restart	=	ambarella_restart_machine,
	.dt_compat	=	s2l_dt_board_compat,
MACHINE_END

