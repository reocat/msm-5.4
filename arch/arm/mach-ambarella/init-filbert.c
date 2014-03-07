/*
 * arch/arm/mach-ambarella/init-filbert.c
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
#include <plat/dma.h>
#include "board-device.h"

/* ==========================================================================*/
static struct platform_device *ambarella_devices[] __initdata = {
	&ambarella_fb0,
	&ambarella_fb1,
	&ambarella_pwm_backlight_device0,
	&ambarella_pwm_backlight_device1,
	&ambarella_pwm_backlight_device2,
	&ambarella_pwm_backlight_device3,
	&ambarella_pwm_backlight_device4,
	&ambarella_rtc0,
	&ambarella_spi0,
	&ambarella_spi1,
	&ambarella_spi2,
	&ambarella_wdt0,
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
		.bus_num	= 0,
		.chip_select	= 4,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 5,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 1,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 2,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 3,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 4,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 5,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 6,
	},
};

/* ==========================================================================*/
static void __init ambarella_init_filbert(void)
{
	int					i;
	struct ambarella_spi_platform_info	*spi2_pdata;

	ambarella_init_machine("Filbert", REF_CLK_FREQ);

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}

	/* config LCD */
	ambarella_board_generic.lcd_backlight.gpio_id = GPIO(85);
	ambarella_board_generic.lcd_backlight.active_level = GPIO_HIGH;
	ambarella_board_generic.lcd_backlight.active_delay = 1;

	ambarella_board_generic.lcd_spi_hw.bus_id = 2;
	ambarella_board_generic.lcd_spi_hw.cs_id = 4;

	/* config audio */
	ambarella_board_generic.audio_reset.gpio_id = GPIO(12);
	ambarella_board_generic.audio_reset.active_level = GPIO_LOW;

	/* Config Vin */
	ambarella_board_generic.vin0_reset.gpio_id = GPIO(127);
	ambarella_board_generic.vin0_reset.active_level = GPIO_LOW;
	ambarella_board_generic.vin0_reset.active_delay = 200;

	/* Config SD */
	fio_default_owner = SELECT_FIO_SDIO;

	spi2_pdata = (struct ambarella_spi_platform_info *)ambarella_spi2.dev.platform_data;
	spi2_pdata->cs_num = 7;
	spi2_pdata->cs_pins[7] = -1;
	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));

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
MACHINE_START(FILBERT, "Filbert")
	.atag_offset	= 0x100,
	.restart_mode	= 's',
	.map_io		= ambarella_map_io,
	.init_irq	= irqchip_init,
	.init_time	= ambarella_timer_init,
	.init_machine	= ambarella_init_filbert,
	.restart	= ambarella_restart_machine,
MACHINE_END

