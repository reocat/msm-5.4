/*
 * arch/arm/mach-ambarella/init-boss.c
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

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/board.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/i2c.h>
#include <linux/i2c/pcf857x.h>
#include <linux/i2c/pca954x.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/input.h>
#include <plat/ambinput.h>

#include <linux/mmc/host.h>

#include <linux/regulator/machine.h>
#include <linux/mfd/wm831x/core.h>
#include <linux/mfd/wm831x/pdata.h>
#include "board-device.h"

/* ==========================================================================*/

/* ==========================================================================*/
static struct platform_device *ambarella_devices[] __initdata = {
#if (UART_INSTANCES >= 2) 	
	&ambarella_uart1,
#endif
};


/* ==========================================================================*/
static void __init ambarella_init_boss(void)
{
	int					i;
	int					use_bub_default = 1;

	ambarella_init_machine("Boss");

	if (use_bub_default) {		
		platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
		for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
			device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
			device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
		}
	}
}

/* ==========================================================================*/
MACHINE_START(AMBARELLA, "Boss")
	.boot_params	= CONFIG_AMBARELLA_PARAMS_PHYS,
	.map_io		= ambarella_map_io,
	.reserve	= ambarella_memblock_reserve,
	.init_irq	= ambarella_init_irq,
	.timer		= &ambarella_timer,
	.init_machine	= ambarella_init_boss,
MACHINE_END

