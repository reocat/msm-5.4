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

#include <asm/cacheflush.h>

#include <plat/ambcache.h>

/* ==========================================================================*/

/* ==========================================================================*/
void ambcache_clean_range(void *addr, unsigned int size)
{
	u32					addr_start, addr_end;

	addr_start = (u32)addr;
	addr_end = addr_start + size;
	dmac_clean_range((void *)addr_start, (void *)addr_end);
#ifdef CONFIG_OUTER_CACHE
	addr_start = ambarella_virt_to_phys(addr_start);
	addr_end = addr_start + size;
	outer_clean_range(addr_start, addr_end);
#endif
}
EXPORT_SYMBOL(ambcache_clean_range);

void ambcache_inv_range(void *addr, unsigned int size)
{
	u32					addr_start, addr_end;

	addr_start = (u32)addr;
	addr_end = addr_start + size;
	dmac_inv_range((void *)addr_start, (void *)addr_end);
#ifdef CONFIG_OUTER_CACHE
	addr_start = ambarella_virt_to_phys(addr_start);
	addr_end = addr_start + size;
	outer_inv_range(addr_start, addr_end);
#endif
}
EXPORT_SYMBOL(ambcache_inv_range);

void ambcache_flush_range(void *addr, unsigned int size)
{
	u32					addr_start, addr_end;

	addr_start = (u32)addr;
	addr_end = addr_start + size;
	dmac_flush_range((void *)addr_start, (void *)addr_end);
#ifdef CONFIG_OUTER_CACHE
	addr_start = ambarella_virt_to_phys(addr_start);
	addr_end = addr_start + size;
	outer_flush_range(addr_start, addr_end);
#endif
}
EXPORT_SYMBOL(ambcache_flush_range);

