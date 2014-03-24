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
#include <plat/ambcache.h>

#include "board-device.h"

/* ==========================================================================*/
static void __init ambarella_init_ixora(void)
{
	ambarella_init_machine("ixora", REF_CLK_FREQ);
#ifdef CONFIG_OUTER_CACHE
	ambcache_l2_enable();
#endif
}

/* ==========================================================================*/
static struct of_dev_auxdata ambarella_auxdata_lookup[] __initdata = {
	{}
};

static void __init ambarella_init_ixora_dt(void)
{
	ambarella_init_ixora();

	of_platform_populate(NULL, of_default_bus_match_table, ambarella_auxdata_lookup, NULL);

	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);

#if defined(CONFIG_VOUT_CONVERTER_IT66121)
	ambarella_init_it66121(0, 0x98>>1);
#endif
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

