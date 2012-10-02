/*
 * arch/arm/mach-ambarella/init-hyacinth_1.c
 *
 * Author: Tzu-Jung Lee <tjlee@ambarella.com>
 *
 * Copyright (C) 2012-2012, Ambarella, Inc.
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
#include <linux/platform_device.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>

#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/board.h>

#ifdef CONFIG_RPROC_CA9_B
extern struct platform_device ambarella_rproc_ca9_b_and_arm11_dev;
#endif /* CONFIG_RPROC_CA9_B */

static struct platform_device *ambarella_devices[] __initdata = {
	&ambarella_rtc0,
	&ambarella_uart,
	&ambarella_uart1,
#ifdef CONFIG_RPROC_CA9_B
	&ambarella_rproc_ca9_b_and_arm11_dev,
#endif /* CONFIG_RPROC_CA9_B */
};

static void __init ambarella_init_hyacinth(void)
{
	int i;

	ambarella_init_machine("Hyacinth_1");

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}
}

MACHINE_START(HYACINTH_1, "Hyacinth_1")
	.boot_params	= CONFIG_AMBARELLA_PARAMS_PHYS,
	.map_io		= ambarella_map_io,
	.reserve	= ambarella_memblock_reserve,
	.init_irq	= ambarella_init_irq,
	.timer		= &ambarella_timer,
	.init_machine	= ambarella_init_hyacinth,
MACHINE_END
