/*
 * arch/arm/plat-ambarella/cortex/hotplug.c
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
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/completion.h>

#include <asm/cacheflush.h>
#include <mach/hardware.h>
#include <plat/ambcache.h>

/* ==========================================================================*/
static DECLARE_COMPLETION(cpu_killed);

/* ==========================================================================*/
static inline void cpu_enter_lowpower(void)
{
	unsigned int v;

	flush_cache_all();
	asm volatile(
	"	mcr	p15, 0, %1, c7, c5, 0\n"
	"	mcr	p15, 0, %1, c7, c10, 4\n"
	/*
	* Turn off coherency
	*/
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	bic	%0, %0, #(1 << 6)\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	bic	%0, %0, #0x04\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
		: "=&r" (v)
		: "r" (0)
		: "cc");
}

static inline void cpu_leave_lowpower(void)
{
	unsigned int v;

	asm volatile(
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	orr	%0, %0, #0x04\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	orr	%0, %0, #(1 << 6)\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
		: "=&r" (v)
		:
		: "cc");
}

int platform_cpu_kill(unsigned int cpu)
{
	return wait_for_completion_timeout(&cpu_killed, 5000);
}

void platform_cpu_die(unsigned int cpu)
{
	u32 *phead_address = get_ambarella_bstmem_head();
	int i;
	u32 bstadd, bstsize;

#ifdef DEBUG
	unsigned int this_cpu = hard_smp_processor_id();

	if (cpu != this_cpu) {
		printk(KERN_CRIT
			"Eek! platform_cpu_die running on %u, should be %u\n",
			this_cpu, cpu);
		BUG();
	}
#endif
	get_ambarella_bstmem_info(&bstadd, &bstsize);
	bstadd = ambarella_phys_to_virt(bstadd);

	printk(KERN_NOTICE "CPU%u: shutdown\n", cpu);
	for (i = 0; i <= (PROCESSOR_STATUS_3 - PROCESSOR_STATUS_0); i++)
		phead_address[PROCESSOR_STATUS_0 + i] = AMB_BST_MAGIC;
	ambcache_flush_range((void *)(bstadd), bstsize);
	complete(&cpu_killed);

	for (;;) {
		cpu_enter_lowpower();
		asm volatile("wfi" : : : "memory", "cc");
		cpu_leave_lowpower();
		ambcache_inv_range((void *)(bstadd), bstsize);
		if (phead_address[PROCESSOR_STATUS_0 + cpu] != AMB_BST_MAGIC)
			break;
#ifdef DEBUG
		printk(KERN_DEBUG "CPU%u: spurious wakeup call\n", cpu);
#endif
	}
	for (i = 0; i <= (PROCESSOR_STATUS_3 - PROCESSOR_STATUS_0); i++)
		phead_address[PROCESSOR_STATUS_0 + i] = AMB_BST_MAGIC;
	ambcache_flush_range((void *)(bstadd), bstsize);
}

int platform_cpu_disable(unsigned int cpu)
{
	u32 *phead_address = get_ambarella_bstmem_head();

	if (phead_address == (u32 *)AMB_BST_INVALID)
		return -EPERM;
	if (cpu > (PROCESSOR_STATUS_3 - PROCESSOR_STATUS_0))
		return -EPERM;

	return cpu == 0 ? -EPERM : 0;
}

