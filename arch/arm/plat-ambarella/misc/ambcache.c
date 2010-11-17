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
#ifdef CONFIG_CACHE_L2X0
#include <asm/hardware/cache-l2x0.h>
#endif
#include <asm/io.h>

#include <plat/ambcache.h>
#include <plat/atag.h>
#include <plat/cortex.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/
#define CACHE_LINE_SIZE		32
#define CACHE_LINE_MASK		~(CACHE_LINE_SIZE - 1)

/* ==========================================================================*/
#ifdef CONFIG_OUTER_CACHE
static void __iomem *ambcache_l2_base = __io(AMBARELLA_VA_L2CC_BASE);
static u32 cortex_l2cache_status = 0;
#endif

/* ==========================================================================*/
void ambcache_clean_range(void *addr, unsigned int size)
{
	u32					addr_start;
	u32					addr_end;
	u32					addr_tmp;

	addr_start = (u32)addr & CACHE_LINE_MASK;
	addr_end = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;

	for (addr_tmp = addr_start; addr_tmp < addr_end;
		addr_tmp += CACHE_LINE_SIZE) {
		__asm__ __volatile__ (
			"mcr p15, 0, %0, c7, c10, 1" : : "r" (addr_tmp));
	}
	dsb();

#ifdef CONFIG_OUTER_CACHE
	addr_start = ambarella_virt_to_phys(addr_start);
	addr_end = addr_start + size;
	outer_clean_range(addr_start, addr_end);
#endif
}
EXPORT_SYMBOL(ambcache_clean_range);

void ambcache_inv_range(void *addr, unsigned int size)
{
	u32					addr_start;
	u32					addr_end;
	u32					addr_tmp;

	addr_start = (u32)addr & CACHE_LINE_MASK;
	addr_end = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;

	for (addr_tmp = addr_start; addr_tmp < addr_end;
		addr_tmp += CACHE_LINE_SIZE) {
		__asm__ __volatile__ (
			"mcr p15, 0, %0, c7, c6, 1" : : "r" (addr_tmp));
	}
	dsb();

#ifdef CONFIG_OUTER_CACHE
	addr_start = ambarella_virt_to_phys(addr_start);
	addr_end = addr_start + size;
	outer_inv_range(addr_start, addr_end);
#endif
}
EXPORT_SYMBOL(ambcache_inv_range);

void ambcache_flush_range(void *addr, unsigned int size)
{
	u32					addr_start;
	u32					addr_end;
	u32					addr_tmp;

	addr_start = (u32)addr & CACHE_LINE_MASK;
	addr_end = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;

	for (addr_tmp = addr_start; addr_tmp < addr_end;
		addr_tmp += CACHE_LINE_SIZE) {
		__asm__ __volatile__ (
			"mcr p15, 0, %0, c7, c14, 1" : : "r" (addr_tmp));
	}
	dsb();

#ifdef CONFIG_OUTER_CACHE
	addr_start = ambarella_virt_to_phys(addr_start);
	addr_end = addr_start + size;
	outer_flush_range(addr_start, addr_end);
#endif
}
EXPORT_SYMBOL(ambcache_flush_range);

int ambcache_l2_enable()
{
#ifdef CONFIG_CACHE_L2X0
	if (readl(ambcache_l2_base + L2X0_DATA_LATENCY_CTRL) !=
		0x00000120) {
		writel(0x00000120, (ambcache_l2_base +
			L2X0_DATA_LATENCY_CTRL));
		pr_info("DATA_LATENCY[0x%08x]\n", readl(
			ambcache_l2_base + L2X0_DATA_LATENCY_CTRL));
	}
	l2x0_init(ambcache_l2_base, 0x00000000, 0xffffffff);
#endif
	return 0;
}
EXPORT_SYMBOL(ambcache_l2_enable);

int ambcache_l2_disable()
{
#ifdef CONFIG_CACHE_L2X0
	l2x0_exit();
#endif
	return 0;
}
EXPORT_SYMBOL(ambcache_l2_disable);

#ifdef CONFIG_OUTER_CACHE
/* =========================Debug Only========================================*/
int cortex_l2cache_set_status(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_uint(val, kp);
	if (!ret) {
		if (cortex_l2cache_status) {
			ret = ambcache_l2_enable();
		} else {
			ret = ambcache_l2_disable();
		}
	}

	return ret;
}

static struct kernel_param_ops param_ops_cortex_l2cache = {
	.set = cortex_l2cache_set_status,
	.get = param_get_uint,
};
module_param_cb(cortex_l2_cache, &param_ops_cortex_l2cache,
	&cortex_l2cache_status, 0644);
#endif

