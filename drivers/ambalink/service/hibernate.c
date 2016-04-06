/*
 * Hibernation support specific for ARM
 *
 * Derived from work on ARM hibernation support by:
 *
 * Ubuntu project, hibernation support for mach-dove
 * Copyright (C) 2010 Nokia Corporation (Hiroshi Doyu)
 * Copyright (C) 2010 Texas Instruments, Inc. (Teerth Reddy et al.)
 *  https://lkml.org/lkml/2010/6/18/4
 *  https://lists.linux-foundation.org/pipermail/linux-pm/2010-June/027422.html
 *  https://patchwork.kernel.org/patch/96442/
 *
 * Copyright (C) 2006 Rafael J. Wysocki <rjw@sisk.pl>
 *
 * License terms: GNU General Public License (GPL) version 2
 */

#include <linux/mm.h>
#include <linux/suspend.h>
#include <asm/system_misc.h>
#include <asm/suspend.h>
#include <asm/memory.h>
#include <asm/sections.h>
#include <asm/cacheflush.h>

#include <plat/ambalink_cfg.h>

typedef struct ambernation_page_info {
	u64 src;
        u64 dst;
        u64 size;
} ambernation_page_info;

typedef struct ambernation_image_info {
        unsigned char	header[PAGE_SIZE];
	unsigned int	curr_page;
	unsigned int	avail_pages;
        unsigned long	page_info_phy;
} ambernation_image_info;

void *ambalink_image_info;
static u32 copied_pages = 0;


static int increase_page_info(ambernation_page_info *src_page)
{
	int retval = 0;
	ambernation_page_info *page_info;
	unsigned long next_src, next_dst;
	ambernation_image_info *image_info;
	unsigned int *curr_page;

	image_info = (ambernation_image_info *) ambalink_image_info;

	curr_page = &image_info->curr_page;

	if (copied_pages >= image_info->avail_pages) {
		pr_debug("%s: copy_pages[%d] >= total_pages[%d].\n", __func__,
			copied_pages, image_info->avail_pages);
		retval = -EPERM;
		goto exit;
	}

	page_info = (struct ambernation_page_info *)
			ambalink_phys_to_virt(image_info->page_info_phy);

	pr_debug("%s: page_info %p offset %d, cur %p \n", __func__,
			page_info, *curr_page, &page_info[*curr_page]);

	if (copied_pages == 0) {
		page_info[*curr_page].src	= src_page->src;
		page_info[*curr_page].dst	= src_page->dst;
		page_info[*curr_page].size	= src_page->size;
	} else {
		next_src = page_info[*curr_page].src + page_info[*curr_page].size;
		next_dst = page_info[*curr_page].dst + page_info[*curr_page].size;
		if ((src_page->src == next_src) && (src_page->dst == next_dst)) {
			page_info[*curr_page].size	+= src_page->size;
		} else {
			*curr_page++;

			page_info[*curr_page].src	= src_page->src;
			page_info[*curr_page].dst	= src_page->dst;
			page_info[*curr_page].size	= src_page->size;
		}
	}


	copied_pages++;

	pr_debug("%s: copy [0x%08x] to [0x%08x], size [0x%08x] %d\n", __func__,
			page_info[*curr_page].src,
			page_info[*curr_page].dst,
			page_info[*curr_page].size,
			copied_pages);
exit:

	return retval;
}

void arch_copy_data_page(unsigned long dst_pfn, unsigned long src_pfn)
{
	struct ambernation_page_info src_page;

	if (ambalink_image_info) {
		src_page.src	= ambalink_page_to_phys(pfn_to_page(src_pfn));
		src_page.dst	= ambalink_page_to_phys(pfn_to_page(dst_pfn));
		src_page.size	= PAGE_SIZE;

		increase_page_info(&src_page);
	}
}

int arch_swsusp_write(void *header)
{
	ambernation_image_info *image_info;
	extern int rpmsg_linkctrl_cmd_suspend_done(int flag);

	image_info = (ambernation_image_info *) ambalink_image_info;

	memcpy((void *) &image_info->header, header, PAGE_SIZE);

	__flush_dcache_area(&image_info, sizeof(image_info));

	return rpmsg_linkctrl_cmd_suspend_done(PM_SUSPEND_MAX);
}

