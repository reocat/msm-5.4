/*
 * arch/arm/mach-ambarella/init-coconut.c
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
#include <linux/irqchip.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/pda_power.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/system_info.h>
#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/board.h>

#include <plat/dma.h>

#include "board-device.h"

/* ==========================================================================*/
static struct platform_device *ambarella_devices[] __initdata = {
	&ambarella_idc0_mux,
};


/* ==========================================================================*/
static void __init ambarella_init_coconut(void)
{
	int					i;

	/* Config Board */
	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_EVK) {
		//vin0: Sensor
		ambarella_board_generic.vin0_power.gpio_id = GPIO(82);
		ambarella_board_generic.vin0_power.active_level = GPIO_HIGH;
		ambarella_board_generic.vin0_power.active_delay = 1;

		ambarella_board_generic.vin0_reset.gpio_id = GPIO(83);
		ambarella_board_generic.vin0_reset.active_level = GPIO_LOW;
		ambarella_board_generic.vin0_reset.active_delay = 1;

		ambarella_board_generic.vin0_vsync.irq_gpio = GPIO(95);
		ambarella_board_generic.vin0_vsync.irq_type = IRQF_TRIGGER_RISING;
		ambarella_board_generic.vin0_vsync.irq_gpio_val = GPIO_HIGH;
		ambarella_board_generic.vin0_vsync.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

		/* Config SD */
		fio_default_owner = SELECT_FIO_SDIO;

		ambarella_board_generic.wifi_power.gpio_id = GPIO(84);
		ambarella_board_generic.wifi_power.active_level = GPIO_HIGH;
		ambarella_board_generic.wifi_power.active_delay = 300;
		ambarella_board_generic.wifi_sd_bus = 0;
		ambarella_board_generic.wifi_sd_slot = 1;
	} else {
		ambarella_board_generic.debug_led0.gpio_id = GPIO(83);
		ambarella_board_generic.debug_led0.active_level = GPIO_LOW;
		ambarella_board_generic.debug_led0.active_delay = 0;

		ambarella_board_generic.rs485.gpio_id = GPIO(31);
		ambarella_board_generic.rs485.active_level = GPIO_LOW;
		ambarella_board_generic.rs485.active_delay = 1;

		ambarella_board_generic.touch_panel_irq.irq_gpio = GPIO(84);
		ambarella_board_generic.touch_panel_irq.irq_type = IRQF_TRIGGER_FALLING;
		ambarella_board_generic.touch_panel_irq.irq_gpio_val = GPIO_LOW;
		ambarella_board_generic.touch_panel_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

		if (AMBARELLA_BOARD_REV(system_rev) > 10) {
			ambarella_board_generic.lcd_backlight.gpio_id = GPIO(85);
		} else {
			ambarella_board_generic.lcd_backlight.gpio_id = GPIO(45);
		}
		ambarella_board_generic.lcd_backlight.active_level = GPIO_HIGH;
		ambarella_board_generic.lcd_backlight.active_delay = 1;

		ambarella_board_generic.lcd_spi_hw.bus_id = 0;
		ambarella_board_generic.lcd_spi_hw.cs_id = 1;

		ambarella_board_generic.vin0_vsync.irq_gpio = GPIO(95);
		ambarella_board_generic.vin0_vsync.irq_type = IRQF_TRIGGER_RISING;
		ambarella_board_generic.vin0_vsync.irq_gpio_val = GPIO_HIGH;
		ambarella_board_generic.vin0_vsync.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

		ambarella_board_generic.vin0_reset.gpio_id = GPIO(93);
		ambarella_board_generic.vin0_reset.active_level = GPIO_LOW;
		ambarella_board_generic.vin0_reset.active_delay = 200;

		ambarella_board_generic.flash_charge_ready.irq_gpio = GPIO(13);
		ambarella_board_generic.flash_charge_ready.irq_type = IRQF_TRIGGER_RISING;
		ambarella_board_generic.flash_charge_ready.irq_gpio_val = GPIO_HIGH;
		ambarella_board_generic.flash_charge_ready.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

		ambarella_board_generic.flash_trigger.gpio_id = GPIO(46);
		ambarella_board_generic.flash_trigger.active_level = GPIO_LOW;
		ambarella_board_generic.flash_trigger.active_delay = 1;

		ambarella_board_generic.flash_enable.gpio_id = GPIO(82);
		ambarella_board_generic.flash_enable.active_level = GPIO_LOW;
		ambarella_board_generic.flash_enable.active_delay = 1;

		/* Config SD */
		fio_default_owner = SELECT_FIO_SDIO;
	}

	/* Register devices */
	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}

#if defined(CONFIG_TOUCH_AMBARELLA_AK4183)
	ambarella_ak4183_board_info.irq =
		ambarella_board_generic.touch_panel_irq.irq_line;
	i2c_register_board_info(0, &ambarella_ak4183_board_info, 1);
#endif
#if defined(CONFIG_TOUCH_AMBARELLA_CHACHA_MT4D)
	ambarella_chacha_mt4d_board_info.irq =
		ambarella_board_generic.touch_panel_irq.irq_line;
	ambarella_chacha_mt4d_board_info.flags = 0;
	i2c_register_board_info(0, &ambarella_chacha_mt4d_board_info, 1);
#endif
#if defined(CONFIG_TOUCH_AMBARELLA_TM1510)
	ambarella_tm1510_board_info.irq =
		ambarella_board_generic.touch_panel_irq.irq_line;
	ambarella_tm1510_board_info.flags = 0;
	i2c_register_board_info(0, &ambarella_tm1510_board_info, 1);
#endif
	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);
}

/* ==========================================================================*/
static struct of_dev_auxdata ambarella_auxdata_lookup[] __initdata = {
	{}
};

static void __init ambarella_init_coconut_dt(void)
{
	ambarella_init_machine("coconut", REF_CLK_FREQ);

	ambarella_init_coconut();

	of_platform_populate(NULL, of_default_bus_match_table,
			ambarella_auxdata_lookup, NULL);
}


static const char * const a5s_dt_board_compat[] = {
	"ambarella,a5s",
	"ambarella,coconut",
	NULL,
};

DT_MACHINE_START(COCONUT_DT, "Ambarella A5S (Flattened Device Tree)")
	.restart_mode	=	's',
	.map_io		=	ambarella_map_io,
	.init_early	=	ambarella_init_early,
	.init_irq	=	irqchip_init,
	.init_time	=	ambarella_timer_init,
	.init_machine	=	ambarella_init_coconut_dt,
	.restart	=	ambarella_restart_machine,
	.dt_compat	=	a5s_dt_board_compat,
MACHINE_END

