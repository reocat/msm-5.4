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
static struct platform_device *ambarella_devices[] __initdata = {
//	&ambarella_fb0,
//	&ambarella_fb1,
};

static struct platform_device *ambarella_pwm_devices[] __initdata = {
	//&ambarella_pwm_backlight_device0,
	/*&ambarella_pwm_backlight_device1,
	&ambarella_pwm_backlight_device2,
	&ambarella_pwm_backlight_device3,
	&ambarella_pwm_backlight_device4,*/
};

/* ==========================================================================*/
static void __init ambarella_init_ginkgo(void)
{
	int i, ret;
	int use_bub_default = 1;

#ifdef CONFIG_OUTER_CACHE
	ambcache_l2_enable();
#endif

	/* Config SD */
	fio_default_owner = SELECT_FIO_SD;

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_IPCAM) {
		switch (AMBARELLA_BOARD_REV(system_rev)) {
		case 'A':
#if defined(CONFIG_RTC_AMBARELLA_IS112022M)
			i2c_register_board_info(2, &ambarella_isl12022m_board_info, 1);
#endif
			use_bub_default = 0;
			break;

		default:
			pr_warn("%s: Unknown EVK Rev[%d]\n", __func__,
				AMBARELLA_BOARD_REV(system_rev));
			break;
		}
	}

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}
}

#ifdef CONFIG_ARM_GIC
static void ginkgo_amp_mask(struct irq_data *d)
{
	void __iomem *addr;
	u32 line = d->hwirq;
	u32 base = (u32)__io(AMBARELLA_VA_GIC_DIST_BASE);
	u32 mask;

	// set distribution to core0
	//printk("{{{{ gic mask line %d }}}}\n", line);
	addr = (void __iomem *) (base + GIC_DIST_TARGET + (line >> 2) * 4);
	mask = readl_relaxed(addr);
	mask &= ~(0xFF << ((line % 4) * 8));
	mask |= (0x1 << ((line % 4) * 8));
	writel_relaxed(mask, addr);
}

static void ginkgo_amp_unmask(struct irq_data *d)
{
	void __iomem *addr;
	u32 line = d->hwirq;
	u32 base = (u32)__io(AMBARELLA_VA_GIC_DIST_BASE);
	u32 mask;

	// set distribution to core1
	//printk("{{{{ gic unmask line %d }}}}\n", line);
	addr = (void __iomem *) (base + GIC_DIST_TARGET + (line >> 2) * 4);
	mask = readl_relaxed(addr);
	mask &= ~(0xFF << ((line % 4) * 8));
	mask |= (0x2 << ((line % 4) * 8));
	writel_relaxed(mask, addr);
}
#endif

static void __init ginkgo_init_irq(void)
{
#ifdef CONFIG_ARM_GIC
	// In case of AMP, we disable general gic_dist_init in gic.c
	// Instead, we distribute irq to core-1 on the fly when an irq
	// is unmasked in ginkgo_amp_unmask.
	gic_arch_extn.irq_mask = ginkgo_amp_mask;
	gic_arch_extn.irq_unmask = ginkgo_amp_unmask;
#endif
	irqchip_init();
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
	.init_irq	=	ginkgo_init_irq,
	.init_time	=	ambarella_timer_init,
	.init_machine	=	ambarella_init_ginkgo_dt,
	.restart	=	ambarella_restart_machine,
	.dt_compat	=	s2_dt_board_compat,
MACHINE_END

