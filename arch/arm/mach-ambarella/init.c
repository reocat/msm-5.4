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

#include <mach/patch-ver.h>

#include "init.h"

/* ==========================================================================*/
static struct platform_device *ambarella_devices[] __initdata = {
	&ambarella_uart,
#if (UART_INSTANCE >= 2)
	&ambarella_uart1,
#endif
	&ambarella_nand,
	&ambarella_sd0,
#if (SD_INSTANCES >= 2)
	&ambarella_sd1,
#endif
	&ambarella_idc0,
#if (IDC_INSTANCES >= 2)
	&ambarella_idc1,
#endif
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
	int					i;

#if (CHIP_REV == A5S)
	char					*pname;

	errorCode = amb_get_chip_name(HAL_BASE_VP, &pname);
	BUG_ON(errorCode != AMB_HAL_SUCCESS);
#endif
	pr_info("Ambarella Patch Version: %s\n", Ambarella_Patch_Version);
	pr_info("Ambarella:\n");
	pr_info("\tchip id:\t\t%d\n", AMBARELLA_BOARD_CHIP(system_rev));
	pr_info("\tboard type:\t\t%d\n", AMBARELLA_BOARD_TYPE(system_rev));
	pr_info("\tboard revision:\t\t%d\n", AMBARELLA_BOARD_REV(system_rev));
#if (CHIP_REV == A5S)
	pr_info("\tchip name:\t\t%s\n", pname);
	pr_info("\treference clock:\t%d\n",
		amb_get_reference_clock_frequency(HAL_BASE_VP));
	pr_info("\tsystem configuration:\t0x%08x\n",
		amb_get_system_configuration(HAL_BASE_VP));
	pr_info("\tboot type:\t\t0x%08x\n",
		amb_get_boot_type(HAL_BASE_VP));
	pr_info("\thif type:\t\t0x%08x\n",
		amb_get_hif_type(HAL_BASE_VP));
#endif

	//Check chip ID
	if (AMBARELLA_BOARD_CHIP(system_rev) != AMBARELLA_BOARD_CHIP_AUTO)
		BUG_ON(AMBARELLA_BOARD_CHIP(system_rev) != CHIP_REV);

	memset((void *)ambarella_debug_info, 0, DEFAULT_DEBUG_SIZE);

	errorCode = ambarella_create_proc_dir();
	BUG_ON(errorCode != 0);

	errorCode = ambarella_init_pll();
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

	errorCode = ambarella_init_audio();
	BUG_ON(errorCode != 0);

	ambarella_register_spi_device();
	ambarella_register_i2c_device();

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++)
		device_init_wakeup(&ambarella_devices[i]->dev, 1);
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

