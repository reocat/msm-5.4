/*
 * arch/arm/plat-ambarella/cortex/smp.c
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

#include <linux/init.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/localtimer.h>
#include <asm/smp_scu.h>
#include <mach/hardware.h>

/* ==========================================================================*/
static void __iomem *scu_base = __io(AMBARELLA_VA_SCU_BASE);
static DEFINE_SPINLOCK(boot_lock);

/* ==========================================================================*/
static inline unsigned int get_core_count(void)
{
	if (scu_base)
		return scu_get_core_count(scu_base);

	return 1;
}

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	trace_hardirqs_off();

	gic_cpu_init(0, __io(AMBARELLA_VA_IC_BASE));

	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	spin_lock(&boot_lock);

	//TBD...
	smp_wmb();

	spin_unlock(&boot_lock);

	return 0;
}

void __init smp_init_cpus(void)
{
	unsigned int i, ncores = get_core_count();

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	unsigned int ncores = get_core_count();
	unsigned int cpu = smp_processor_id();
	int i;

	if (ncores == 0) {
		pr_err("%s: strange core count of 0? Default to 1\n", __func__);
		ncores = 1;
	}

	if (ncores > NR_CPUS) {
		pr_warning("%s: no. of cores (%d) greater than configured\n"
			"maximum of %d - clipping\n",
			__func__, ncores, NR_CPUS);
		ncores = NR_CPUS;
	}
	smp_store_cpu_info(cpu);

	if (max_cpus > ncores)
		max_cpus = ncores;

	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	if (max_cpus > 1) {
		percpu_timer_setup();
		scu_enable(scu_base);
		//TBD...
	}
}

