/*
 * arch/arm/mach-ambarella/init-durian.c
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
#include <asm/system_info.h>

#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/board.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/input.h>
#include <plat/dma.h>
#include <linux/mmc/host.h>

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
		.bus_num	= 0,
		.chip_select	= 6,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 7,
	}
};

/* ==========================================================================*/
static void __init ambarella_init_durian(void)
{
	int					i;

	ambarella_init_machine("Durian", REF_CLK_FREQ);

	/* Config Board */
	ambarella_board_generic.lcd_reset.gpio_id = GPIO(113);
	ambarella_board_generic.lcd_reset.active_level = GPIO_LOW;
	ambarella_board_generic.lcd_reset.active_delay = 1;

	ambarella_board_generic.lcd_backlight.gpio_id = GPIO(47);
	ambarella_board_generic.lcd_backlight.active_level = GPIO_HIGH;
	ambarella_board_generic.lcd_backlight.active_delay = 1;

	ambarella_board_generic.lcd_spi_hw.bus_id = 0;
	ambarella_board_generic.lcd_spi_hw.cs_id = 5;

	ambarella_board_generic.lcd_spi_cfg.spi_mode = 0;
	ambarella_board_generic.lcd_spi_cfg.cfs_dfs = 16;
	ambarella_board_generic.lcd_spi_cfg.baud_rate = 5000000;
	ambarella_board_generic.lcd_spi_cfg.cs_change = 0;

	ambarella_board_generic.vin0_reset.gpio_id = GPIO(118);
	ambarella_board_generic.vin0_reset.active_level = GPIO_LOW;
	ambarella_board_generic.vin0_reset.active_delay = 100;

	ambarella_board_generic.flash_charge_ready.irq_gpio = GPIO(51);
	ambarella_board_generic.flash_charge_ready.irq_line = gpio_to_irq(51);
	ambarella_board_generic.flash_charge_ready.irq_type = IRQF_TRIGGER_RISING;
	ambarella_board_generic.flash_charge_ready.irq_gpio_val = GPIO_HIGH;
	ambarella_board_generic.flash_charge_ready.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.flash_trigger.gpio_id = GPIO(54);
	ambarella_board_generic.flash_trigger.active_level = GPIO_LOW;
	ambarella_board_generic.flash_trigger.active_delay = 1;

	ambarella_board_generic.flash_enable.gpio_id = GPIO(11);
	ambarella_board_generic.flash_enable.active_level = GPIO_LOW;
	ambarella_board_generic.flash_enable.active_delay = 1;

	ambarella_board_generic.gyro_irq.irq_gpio = GPIO(53);
	ambarella_board_generic.gyro_irq.irq_line = gpio_to_irq(53);
	ambarella_board_generic.gyro_irq.irq_type = IRQF_TRIGGER_RISING;
	ambarella_board_generic.gyro_irq.irq_gpio_val = GPIO_HIGH;
	ambarella_board_generic.gyro_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.gyro_power.gpio_id = EXT_GPIO(46);
	ambarella_board_generic.gyro_power.active_level = GPIO_HIGH;
	ambarella_board_generic.gyro_power.active_delay = 1;

	/* Config SD */
	fio_default_owner = SELECT_FIO_SD;


	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));
}

/* ==========================================================================*/
MACHINE_START(DURIAN, "Durian")
	.atag_offset	= 0x100,
	.restart_mode	= 's',
	.map_io		= ambarella_map_io,
	.init_irq	= irqchip_init,
	.init_time	= ambarella_timer_init,
	.init_machine	= ambarella_init_durian,
	.restart	= ambarella_restart_machine,
MACHINE_END

