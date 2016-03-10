/*
 * arch/arm/plat-ambarella/misc/service.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * Copyright (C) 2012-2016, Ambarella, Inc.
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
#include <plat/iav_helper.h>
#include <mach/init.h>

/*===========================================================================*/

static LIST_HEAD(ambarella_svc_list);

int ambarella_register_service(struct ambarella_service *amb_svc)
{
	struct ambarella_service *svc;

	if (!amb_svc || !amb_svc->func)
		return -EINVAL;

	list_for_each_entry(svc, &ambarella_svc_list, node) {
		if (svc->service == amb_svc->service) {
			pr_err("%s: service (%d) is already existed\n",
					__func__, amb_svc->service);
			return -EEXIST;
		}
	}

	list_add_tail(&amb_svc->node, &ambarella_svc_list);

	return 0;
}
EXPORT_SYMBOL(ambarella_register_service);

int ambarella_unregister_service(struct ambarella_service *amb_svc)
{
	struct ambarella_service *svc;

	if (!amb_svc)
		return -EINVAL;

	list_for_each_entry(svc, &ambarella_svc_list, node) {
		if (svc->service == amb_svc->service) {
			list_del(&svc->node);
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ambarella_unregister_service);

int ambarella_request_service(int service, void *arg, void *result)
{
	struct ambarella_service *svc;
	int found = 0;

	list_for_each_entry(svc, &ambarella_svc_list, node) {
		if (svc->service == service) {
			found = 1;
			break;
		}
	}

	if (found == 0) {
		pr_err("%s: no such service (%d)\n", __func__, service);
		return -ENODEV;
	}

	return svc->func(arg, result);
}
EXPORT_SYMBOL(ambarella_request_service);

/*===========================================================================*/

#define CACHE_LINE_SIZE		32
#define CACHE_LINE_MASK		~(CACHE_LINE_SIZE - 1)

void ambcache_clean_range(void *addr, unsigned int size)
{
	u32					vstart;
	u32					vend;
#ifdef CONFIG_OUTER_CACHE
	u32					pstart;
#endif
	u32					addr_tmp;

	vstart = (u32)addr & CACHE_LINE_MASK;
	vend = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;
#if defined(CONFIG_AMBARELLA_SYS_CACHE_CALL)
	if (cache_check_start && (vstart != (u32)addr)) {
		if (cache_check_fail_halt) {
			panic("%s start:0x%08x vs 0x%08x\n",
				__func__, vstart, (u32)addr);
		} else {
			pr_warn("%s start:0x%08x vs 0x%08x\n",
				__func__, vstart, (u32)addr);
		}
	}
	if (cache_check_end && (vend != ((u32)addr + size))) {
		if (cache_check_fail_halt) {
			panic("%s end:0x%08x vs 0x%08x\n",
				__func__, vend, ((u32)addr + size));
		} else {
			pr_warn("%s end:0x%08x vs 0x%08x\n",
				__func__, vend, ((u32)addr + size));
		}
	}
#endif
#ifdef CONFIG_OUTER_CACHE
	pstart = ambarella_virt_to_phys(vstart);
#endif

	for (addr_tmp = vstart; addr_tmp < vend; addr_tmp += CACHE_LINE_SIZE) {
		__asm__ __volatile__ (
			"mcr p15, 0, %0, c7, c10, 1" : : "r" (addr_tmp));
	}
	dsb();

#ifdef CONFIG_OUTER_CACHE
	outer_clean_range(pstart, (pstart + size));
#endif
}
EXPORT_SYMBOL(ambcache_clean_range);

void ambcache_inv_range(void *addr, unsigned int size)
{
	u32					vstart;
	u32					vend;
#ifdef CONFIG_OUTER_CACHE
	u32					pstart;
#endif
	u32					addr_tmp;

	vstart = (u32)addr & CACHE_LINE_MASK;
	vend = ((u32)addr + size + CACHE_LINE_SIZE - 1) & CACHE_LINE_MASK;
#if defined(CONFIG_AMBARELLA_SYS_CACHE_CALL)
	if (cache_check_start && (vstart != (u32)addr)) {
		if (cache_check_fail_halt) {
			panic("%s start:0x%08x vs 0x%08x\n",
				__func__, vstart, (u32)addr);
		} else {
			pr_warn("%s start:0x%08x vs 0x%08x\n",
				__func__, vstart, (u32)addr);
		}
	}
	if (cache_check_end && (vend != ((u32)addr + size))) {
		if (cache_check_fail_halt) {
			panic("%s end:0x%08x vs 0x%08x\n",
				__func__, vend, ((u32)addr + size));
		} else {
			pr_warn("%s end:0x%08x vs 0x%08x\n",
				__func__, vend, ((u32)addr + size));
		}
	}
#endif
#ifdef CONFIG_OUTER_CACHE
	pstart = ambarella_virt_to_phys(vstart);
	outer_inv_range(pstart, (pstart + size));
#endif

	for (addr_tmp = vstart; addr_tmp < vend; addr_tmp += CACHE_LINE_SIZE) {
		__asm__ __volatile__ (
			"mcr p15, 0, %0, c7, c6, 1" : : "r" (addr_tmp));
	}
	dsb();

#ifdef CONFIG_OUTER_CACHE
	outer_inv_range(pstart, (pstart + size));

	for (addr_tmp = vstart; addr_tmp < vend; addr_tmp += CACHE_LINE_SIZE) {
		__asm__ __volatile__ (
			"mcr p15, 0, %0, c7, c6, 1" : : "r" (addr_tmp));
	}
	dsb();
#endif
}
EXPORT_SYMBOL(ambcache_inv_range);


