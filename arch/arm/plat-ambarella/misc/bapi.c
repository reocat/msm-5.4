/*
 * arch/arm/plat-ambarella/misc/bapi.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2011, Ambarella, Inc.
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
#include <linux/suspend.h>

#include <asm/setup.h>
#include <asm/suspend.h>

#include <mach/hardware.h>
#include <plat/bapi.h>
#include <plat/ambcache.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/

/* ==========================================================================*/
static struct ambarella_bapi_tag_s bapi_tag;
static struct ambarella_bapi_s *bapi_info = NULL;
extern int in_suspend;

/* ==========================================================================*/
static int __init parse_mem_tag_bapi(const struct tag *tag)
{
	bapi_tag.magic = tag->u.mem.size;
	bapi_tag.pbapi_info = tag->u.mem.start;

	pr_debug("%s: magic=0x%08x pbapi_info=0x%08x\n", __func__,
		bapi_tag.magic, bapi_tag.pbapi_info);
	return 0;
}
__tagtable(ATAG_AMBARELLA_BAPI, parse_mem_tag_bapi);

/* ==========================================================================*/
int ambarella_bapi_check_bapi_info(enum ambarella_bapi_cmd_e cmd)
{
	int				retval = 0;

	if (bapi_info == NULL) {
		pr_debug("CMD[%d]: bapi_info is NULL.\n", cmd);
		retval = -EPERM;
		goto ambarella_bapi_check_bapi_info_exit;
	}

	if (bapi_info->magic != DEFAULT_BAPI_MAGIC) {
		pr_debug("CMD[%d]: Wrong magic %d!\n", cmd, bapi_info->magic);
		retval = -EPERM;
		goto ambarella_bapi_check_bapi_info_exit;
	}

	if (bapi_info->version != DEFAULT_BAPI_VERSION) {
		pr_debug("CMD[%d]: Wrong version %d!\n", cmd, bapi_info->version);
		retval = -EPERM;
		goto ambarella_bapi_check_bapi_info_exit;
	}

ambarella_bapi_check_bapi_info_exit:
	return retval;
}

int ambarella_bapi_cmd(enum ambarella_bapi_cmd_e cmd, void *args)
{
	int					retval = 0;
	struct ambarella_bapi_aoss_s		*tmp_aoss_info = NULL;
	struct ambarella_bapi_aoss_page_info_s	*tmp_page_info = NULL;
	struct ambarella_bapi_aoss_page_info_s	*src_page_info = NULL;
	ambarella_bapi_aoss_t			aoss_entry = NULL;
	unsigned long                           flags;

	switch(cmd) {
	case AMBARELLA_BAPI_CMD_INIT:
		if ((bapi_tag.magic == DEFAULT_BAPI_TAG_MAGIC) &&
			(bapi_tag.pbapi_info != 0)) {
			bapi_info = (struct ambarella_bapi_s *)
				ambarella_phys_to_virt(bapi_tag.pbapi_info);
			pr_debug("%s: bapi_info & 0x%p\n", __func__, bapi_info);
		} else {
			bapi_info = NULL;
			retval = -ENXIO;
		}
		retval = ambarella_bapi_check_bapi_info(cmd);
		break;

	case AMBARELLA_BAPI_CMD_AOSS_INIT:
		retval = ambarella_bapi_check_bapi_info(cmd);
		if (retval == 0) {
			bapi_info->aoss_info.magic = DEFAULT_BAPI_AOSS_MAGIC;
			bapi_info->aoss_info.copy_pages = 0;
		}
		break;

	case AMBARELLA_BAPI_CMD_AOSS_COPY_PAGE:
		retval = ambarella_bapi_check_bapi_info(cmd);
		if ((retval != 0) || (args == NULL))
			break;

		tmp_aoss_info = &bapi_info->aoss_info;
		src_page_info = (struct ambarella_bapi_aoss_page_info_s *)args;
		if (tmp_aoss_info->copy_pages < tmp_aoss_info->total_pages) {
			tmp_page_info = (struct ambarella_bapi_aoss_page_info_s *)
				ambarella_phys_to_virt(tmp_aoss_info->page_info);
			pr_debug("%s: tmp_page_info %p offset %d, cur %p \n", __func__,
				tmp_page_info, tmp_aoss_info->copy_pages,
				&tmp_page_info[tmp_aoss_info->copy_pages]);
			tmp_page_info[tmp_aoss_info->copy_pages].src =
				src_page_info->src;
			tmp_page_info[tmp_aoss_info->copy_pages].dst =
				src_page_info->dst;
			pr_debug("%s: copy [0x%08x] to [0x%08x]\n", __func__,
				tmp_page_info[tmp_aoss_info->copy_pages].src,
				tmp_page_info[tmp_aoss_info->copy_pages].dst);
			tmp_aoss_info->copy_pages++;
		} else {
			pr_err("%s: copy_pages[%d] >= total_pages[%d].\n", __func__,
				tmp_aoss_info->copy_pages, tmp_aoss_info->total_pages);
			retval = -EPERM;
		}
		break;

	case AMBARELLA_BAPI_CMD_AOSS_SAVE:
		retval = ambarella_bapi_check_bapi_info(cmd);
		if (retval == 0) {
			aoss_entry = (ambarella_bapi_aoss_t)ambarella_phys_to_virt(
				bapi_info->aoss_info.fn_pri[0]);
			pr_info("%s: %p[%p].\n", __func__, aoss_entry,
				bapi_info->aoss_info.fn_pri);
			local_irq_save(flags);
			ambcache_clean_range(bapi_info, bapi_info->size);
			retval = aoss_entry((u32)bapi_info->aoss_info.fn_pri);
			bapi_info->aoss_info.copy_pages = 0;
			in_suspend = 0;
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
			set_ambarella_hal_invalid();
#endif
			swsusp_arch_restore_cpu();
			local_irq_restore(flags);
		}
		break;

	default:
		pr_err("%s: Unknown CMD[%d].\n", __func__, cmd);
		break;
	}

	return retval;
}

/* ==========================================================================*/
void arch_copy_data_page(unsigned long dst_pfn, unsigned long src_pfn)
{
	struct ambarella_bapi_aoss_page_info_s	page_info;

	page_info.src = __pfn_to_phys(src_pfn);
	page_info.dst = __pfn_to_phys(dst_pfn);

	ambarella_bapi_cmd(AMBARELLA_BAPI_CMD_AOSS_COPY_PAGE, &page_info);
}

int arch_swsusp_write(unsigned int flags)
{
	return ambarella_bapi_cmd(AMBARELLA_BAPI_CMD_AOSS_SAVE, NULL);
}

/* ==========================================================================*/
static int __init ambarella_bapi_init(void)
{
	int					retval = 0;

	ambarella_bapi_cmd(AMBARELLA_BAPI_CMD_INIT, NULL);
	ambarella_bapi_cmd(AMBARELLA_BAPI_CMD_AOSS_INIT, NULL);

	return retval;
}
arch_initcall(ambarella_bapi_init);

