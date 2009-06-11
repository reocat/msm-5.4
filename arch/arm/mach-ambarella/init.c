/*
 * arch/arm/mach-ambarella/init.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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

#include "init.h"

/* ==========================================================================*/
static struct platform_device *devices[] __initdata = {
	&ambarella_uart,
	&ambarella_nand,
	&ambarella_sd0,
#if (SD_INSTANCES >= 2)
	&ambarella_sd1,
#endif
	&ambarella_idc0,
	&ambarella_spi0,
#if (SPI_INSTANCES >= 2)	
	&ambarella_spi1,
#endif
	&ambarella_i2s0,
	&ambarella_rtc0,
	&ambarella_wdt0,
	&ambarella_eth0,
	&ambarella_udc0,
#ifdef CONFIG_AMBARELLA_FB0
	&ambarella_fb0,
#endif
#ifdef CONFIG_AMBARELLA_FB1
	&ambarella_fb1,
#endif
	&ambarella_ir0,
	&ambarella_ide0,
	&ambarella_power_supply,
};

/* ==========================================================================*/
static void __init ambarella_init(void)
{
	int					errorCode = 0;

	pr_info("ambarella_init:\n");
	pr_info("\t\tchip id: %d\n", AMBARELLA_BOARD_CHIP(system_rev));
	pr_info("\t\tboard type: %d\n", AMBARELLA_BOARD_TYPE(system_rev));
	pr_info("\t\tboard revision: %d\n", AMBARELLA_BOARD_REV(system_rev));

	//Check chip ID
	if (AMBARELLA_BOARD_CHIP(system_rev) != AMBARELLA_BOARD_CHIP_AUTO)
		BUG_ON(AMBARELLA_BOARD_CHIP(system_rev) != CHIP_REV);

	errorCode = ambarella_create_proc_dir();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_tm();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_gpio();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_fio();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_dma();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_adc();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_pwm();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_fb();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_pm();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_eth0(system_serial_high,
		system_serial_low);
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_nand(NULL);
	BUG_ON(errorCode != 0);

	ambarella_register_spi_device();

	platform_add_devices(devices, ARRAY_SIZE(devices));
}

/* ==========================================================================*/
MACHINE_START(AMBARELLA, "Ambarella Media SoC")
	.phys_io	= AHB_START,
	.io_pg_offst	= ((AHB_BASE) >> 18) & 0xfffc,
	.map_io		= ambarella_map_io,
	.init_irq	= ambarella_init_irq,
	.timer		= &ambarella_timer,
	.init_machine	= ambarella_init,
	.boot_params	= DEFAULT_ATAG_START,
MACHINE_END

