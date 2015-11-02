/*
 * arch/arm/plat-ambarella/misc/ambcache.c
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
#include <linux/module.h>
#include <linux/cpu.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/io.h>

#include <mach/hardware.h>
#include <mach/init.h>
#include <plat/ambcache.h>

#define CACHE_LINE_SIZE		32
#define CACHE_LINE_MASK		~(CACHE_LINE_SIZE - 1)
#define CONFIG_CACHE_PL310_PREFETCH_OFFSET      0


void ambcache_clean_range(void *addr, unsigned int size)
{
	unsigned int vstart;
	unsigned int vend;
	unsigned int pstart;
	unsigned long flags;

	vstart = (u32)addr & CACHE_LINE_MASK;
	vend = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;
	pstart = ambarella_virt_to_phys(vstart);

	local_irq_save(flags);
	__cpuc_flush_dcache_area((void *)vstart, vend - vstart);
	outer_clean_range(pstart, (pstart + size));
	local_irq_restore(flags);
}
EXPORT_SYMBOL(ambcache_clean_range);

void ambcache_inv_range(void *addr, unsigned int size)
{
	unsigned int vstart;
	unsigned int vend;
	unsigned int pstart;
	unsigned long flags;

	vstart = (u32)addr & CACHE_LINE_MASK;
	vend = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;
	pstart = ambarella_virt_to_phys(vstart);

	local_irq_save(flags);
	outer_inv_range(pstart, (pstart + size));
	__cpuc_flush_dcache_area((void *)vstart, vend - vstart);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(ambcache_inv_range);

void ambcache_flush_range(void *addr, unsigned int size)
{
	unsigned int vstart;
	unsigned int vend;
	unsigned int pstart;
	unsigned long flags;

	vstart = (u32)addr & CACHE_LINE_MASK;
	vend = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;
	pstart = ambarella_virt_to_phys(vstart);

	local_irq_save(flags);
	__cpuc_flush_dcache_area((void *)vstart, vend - vstart);
	outer_flush_range(pstart, (pstart + size));
	local_irq_restore(flags);
}
EXPORT_SYMBOL(ambcache_flush_range);

void ambcache_pli_range(void *addr, unsigned int size)
{
	u32					vstart;
	u32					vend;
	u32					addr_tmp;

	vstart = (u32)addr & CACHE_LINE_MASK;
	vend = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;

	for (addr_tmp = vstart; addr_tmp < vend; addr_tmp += CACHE_LINE_SIZE) {
#if __LINUX_ARM_ARCH__ >= 7
		__asm__ __volatile__ (
			"pli [%0]" : : "r" (addr_tmp));
#elif __LINUX_ARM_ARCH__ >= 5
		__asm__ __volatile__ (
			"mcr p15, 0, %0, c7, c13, 1" : : "r" (addr_tmp));
#else
#error "PLI not supported"
#endif
	}
}
EXPORT_SYMBOL(ambcache_pli_range);

