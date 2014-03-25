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
#include <linux/of_platform.h>
#include <linux/irqchip.h>
#include <asm/mach/arch.h>
#include <mach/init.h>

static void __init ambarella_init_ixora_dt(void)
{
	ambarella_init_machine("ixora", REF_CLK_FREQ);

	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
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

