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
static void __init ambarella_init_durian(void)
{
	int					i;

	ambarella_init_machine("Durian", REF_CLK_FREQ);

	/* Config SD */
	fio_default_owner = SELECT_FIO_SD;
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

