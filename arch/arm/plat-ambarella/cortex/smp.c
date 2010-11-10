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
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/bootmem.h>

#include <asm/cacheflush.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/localtimer.h>
#include <asm/unified.h>
#include <asm/smp_scu.h>

/* ==========================================================================*/
extern void ambarella_secondary_startup(void);

static void __iomem *scu_base = __io(AMBARELLA_VA_SCU_BASE);
static DEFINE_SPINLOCK(boot_lock);

static unsigned int smp_max_cpus = 0;

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

	gic_cpu_init(0, __io(AMBARELLA_VA_GIC_CPU_BASE));

	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;
	int i;
	u32 *phead_address;
	u32 bstadd, bstsize;
	int retval = 0;

	spin_lock(&boot_lock);

	if (get_ambarella_bstmem_info(&bstadd, &bstsize) != AMB_BST_MAGIC) {
		pr_err("Can't find SMP BST!\n");
		retval = -EPERM;
		goto boot_secondary_exit;
	}

	phead_address = get_ambarella_bstmem_head();
	if (phead_address == (u32 *)AMB_BST_INVALID) {
		pr_err("Can't find SMP BST Header!\n");
		retval = -EPERM;
		goto boot_secondary_exit;
	}

	smp_cross_call(cpumask_of(cpu));
	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (phead_address[PROCESSOR_STATUS_0 + cpu] != AMB_BST_INVALID)
			break;
		udelay(10);
	}

	smp_rmb();
	for (i = 0; i < smp_max_cpus; i++) {
		if (phead_address[PROCESSOR_STATUS_0 + i] == AMB_BST_INVALID) {
			pr_err("CPU[%d] is still dead!\n", i);
			retval = -EAGAIN;
		}
	}
	if (retval == 0) {
		pr_info("Free BST Memory: 0x%08x[0x%08x]\n", bstadd, bstsize);
		free_bootmem(bstadd, bstsize);
	}

boot_secondary_exit:
	spin_unlock(&boot_lock);

	return retval;
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
	u32 *phead_address = get_ambarella_bstmem_head();

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
		if (phead_address != (u32 *)AMB_BST_INVALID) {
			for (i = 1; i < max_cpus; i++) {
				phead_address[PROCESSOR_START_0 + i] = BSYM(
					virt_to_phys(ambarella_secondary_startup));
			}
			flush_cache_all();
		}
	}
	smp_max_cpus = max_cpus;
}

