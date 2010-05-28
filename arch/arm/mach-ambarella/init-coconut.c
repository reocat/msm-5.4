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

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/board.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/i2c.h>
#include <linux/i2c/ak4183.h>
#include <linux/i2c/cy8ctmg.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "board-device.h"

/* ==========================================================================*/
#include <linux/pda_power.h>
static int ambarella_power_is_ac_online(void)
{
	return 1;
}

static struct pda_power_pdata  ambarella_power_supply_info = {
	.is_ac_online    = ambarella_power_is_ac_online,
};

struct platform_device ambarella_power_supply = {
	.name = "pda-power",
	.id   = -1,
	.dev  = {
		.platform_data = &ambarella_power_supply_info,
	},
};

/* ==========================================================================*/
static struct platform_device *ambarella_devices[] __initdata = {
	&ambarella_adc0,
	&ambarella_crypto,
	&ambarella_fb0,
	&ambarella_fb1,
	&ambarella_i2s0,
	&ambarella_idc0,
	&ambarella_idc1,
	&ambarella_ir0,
	&ambarella_rtc0,
	&ambarella_sd0,
	&ambarella_spi0,
	&ambarella_spi1,
	&ambarella_uart,
	&ambarella_uart1,
	&ambarella_udc0,
	&ambarella_wdt0,
	&ambarella_power_supply,
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
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 3,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 0,
	}
};

/* ==========================================================================*/
static void __init ambarella_init_coconut(void)
{
	int					i;

	ambarella_init_machine("Coconut");

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}

	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));

	ambarella_board_generic.power_detect.irq_gpio = GPIO(11);
	ambarella_board_generic.power_detect.irq_line = gpio_to_irq(11);
	ambarella_board_generic.power_detect.irq_type = IRQF_TRIGGER_FALLING;
	ambarella_board_generic.power_detect.irq_gpio_val = GPIO_LOW;
	ambarella_board_generic.power_detect.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.power_control.power_gpio = -1;
	ambarella_board_generic.power_control.power_level = GPIO_LOW;
	ambarella_board_generic.power_control.power_delay = 1;

	ambarella_board_generic.debug_led0.power_gpio = GPIO(83);
	ambarella_board_generic.debug_led0.power_level = GPIO_LOW;
	ambarella_board_generic.debug_led0.power_delay = 0;

	ambarella_board_generic.rs485.power_gpio = GPIO(31);
	ambarella_board_generic.rs485.power_level = GPIO_LOW;
	ambarella_board_generic.rs485.power_delay = 1;

	ambarella_board_generic.audio_codec.reset_gpio = GPIO(12);
	ambarella_board_generic.audio_codec.reset_level = GPIO_LOW;
	ambarella_board_generic.audio_codec.reset_delay = 1;
	ambarella_board_generic.audio_codec.resume_delay = 1;

	ambarella_board_generic.audio_speaker.power_gpio = -1;
	ambarella_board_generic.audio_speaker.power_level = GPIO_LOW;
	ambarella_board_generic.audio_speaker.power_delay = 1;

	ambarella_board_generic.audio_headphone.power_gpio = -1;
	ambarella_board_generic.audio_headphone.power_level = GPIO_LOW;
	ambarella_board_generic.audio_headphone.power_delay = 1;

	ambarella_board_generic.audio_microphone.power_gpio = -1;
	ambarella_board_generic.audio_microphone.power_level = GPIO_LOW;
	ambarella_board_generic.audio_microphone.power_delay = 1;

	ambarella_board_generic.tp_irq.irq_gpio = GPIO(84);
	ambarella_board_generic.tp_irq.irq_line = gpio_to_irq(84);
	ambarella_board_generic.tp_irq.irq_type = IRQF_TRIGGER_FALLING;
	ambarella_board_generic.tp_irq.irq_gpio_val = GPIO_LOW;
	ambarella_board_generic.tp_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.tp_reset.reset_gpio = -1;
	ambarella_board_generic.tp_reset.reset_level = GPIO_LOW;
	ambarella_board_generic.tp_reset.reset_delay = 1;
	ambarella_board_generic.tp_reset.resume_delay = 1;

	if (AMBARELLA_BOARD_REV(system_rev) > 10) {
		ambarella_board_generic.lcd_backlight.power_gpio = GPIO(85);
	} else {
		ambarella_board_generic.lcd_backlight.power_gpio = GPIO(45);
	}
	ambarella_board_generic.lcd_backlight.power_level = GPIO_HIGH;
	ambarella_board_generic.lcd_backlight.power_delay = 1;

	ambarella_board_generic.vin_vsync.irq_gpio = GPIO(95);
	ambarella_board_generic.vin_vsync.irq_line = gpio_to_irq(95);
	ambarella_board_generic.vin_vsync.irq_type = IRQF_TRIGGER_RISING;
	ambarella_board_generic.vin_vsync.irq_gpio_val = GPIO_HIGH;
	ambarella_board_generic.vin_vsync.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.flash_charge_ready.irq_gpio = GPIO(13);
	ambarella_board_generic.flash_charge_ready.irq_line = gpio_to_irq(13);
	ambarella_board_generic.flash_charge_ready.irq_type = IRQF_TRIGGER_RISING;
	ambarella_board_generic.flash_charge_ready.irq_gpio_val = GPIO_HIGH;
	ambarella_board_generic.flash_charge_ready.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.flash_trigger.power_gpio = GPIO(46);
	ambarella_board_generic.flash_trigger.power_level = GPIO_LOW;
	ambarella_board_generic.flash_trigger.power_delay = 1;

	ambarella_board_generic.flash_enable.power_gpio = GPIO(82);
	ambarella_board_generic.flash_enable.power_level = GPIO_LOW;
	ambarella_board_generic.flash_enable.power_delay = 1;

	ambarella_ak4183_board_info.irq =
		ambarella_board_generic.tp_irq.irq_line;
	i2c_register_board_info(0, &ambarella_ak4183_board_info, 1);

	if (AMBARELLA_BOARD_REV(system_rev) >= 17) {
		i2c_register_board_info(0, &ambarella_isl12022m_board_info, 1);
	}
}

/* ==========================================================================*/
MACHINE_START(COCONUT, "Ambarella Coconut")
	.phys_io	= AHB_START,
	.io_pg_offst	= ((AHB_BASE) >> 18) & 0xfffc,
	.map_io		= ambarella_map_io,
	.init_irq	= ambarella_init_irq,
	.timer		= &ambarella_timer,
	.init_machine	= ambarella_init_coconut,
	.boot_params	= CONFIG_AMBARELLA_PARAMS_PHYS,
MACHINE_END

