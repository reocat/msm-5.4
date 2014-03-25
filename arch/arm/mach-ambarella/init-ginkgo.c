/*
 * arch/arm/mach-ambarella/init-ginkgo.c
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
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <linux/irqchip.h>
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

#include <plat/dma.h>
#include <plat/clk.h>

#include <linux/input.h>

/* ==========================================================================*/
static void __init ambarella_init_ginkgo(void)
{
#ifdef CONFIG_OUTER_CACHE
	ambcache_l2_enable();
#endif

	/* Config SD */
	fio_default_owner = SELECT_FIO_SD;

	/* Config Vin */
	ambarella_board_generic.vin1_reset.gpio_id = GPIO(49);
	ambarella_board_generic.vin1_reset.active_level = GPIO_LOW;
	ambarella_board_generic.vin1_reset.active_delay = 1;

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_IPCAM) {
		switch (AMBARELLA_BOARD_REV(system_rev)) {
		case 'A':
			break;

		default:
			pr_warn("%s: Unknown EVK Rev[%d]\n", __func__,
				AMBARELLA_BOARD_REV(system_rev));
			break;
		}
	}

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_ATB) {
			ambarella_board_generic.vin0_reset.gpio_id = GPIO(54);
			ambarella_board_generic.vin0_reset.active_level = GPIO_LOW;
			ambarella_board_generic.vin0_reset.active_delay = 500;
	}

	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);
}

/* ==========================================================================*/

static struct of_dev_auxdata ambarella_auxdata_lookup[] __initdata = {
	{}
};

static void __init ambarella_init_ginkgo_dt(void)
{
	ambarella_init_machine("ginkgo", REF_CLK_FREQ);

	ambarella_init_ginkgo();

	of_platform_populate(NULL, of_default_bus_match_table,
			ambarella_auxdata_lookup, NULL);
}


static const char * const s2_dt_board_compat[] = {
	"ambarella,s2",
	"ambarella,ginkgo",
	NULL,
};

DT_MACHINE_START(GINKGO_DT, "Ambarella S2 (Flattened Device Tree)")
	.restart_mode	=	's',
	.smp		=	smp_ops(ambarella_smp_ops),
	.map_io		=	ambarella_map_io,
	.init_early	=	ambarella_init_early,
	.init_irq	=	irqchip_init,
	.init_time	=	ambarella_timer_init,
	.init_machine	=	ambarella_init_ginkgo_dt,
	.restart	=	ambarella_restart_machine,
	.dt_compat	=	s2_dt_board_compat,
MACHINE_END

