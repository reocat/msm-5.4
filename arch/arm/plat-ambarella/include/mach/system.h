/*
 * arch/arm/plat-ambarella/include/mach/system.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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

#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <plat/reboot.h>
#include <plat/ambcache.h>

/* ==========================================================================*/

/* ==========================================================================*/
#ifndef __ASSEMBLER__

/* ==========================================================================*/
static inline void arch_idle(void)
{
	cpu_do_idle();
}

static inline void arch_reset(char mode, const char *cmd)
{
	reboot_info_t *info = (reboot_info_t *)(ambarella_reboot_info);
	if(info != NULL) {
		info->magic = REBOOT_MAGIC;
		if(cmd == NULL) {
			info->mode = NORMAL;
		}
		else if(strcmp(cmd, "recovery") == 0) {
			info->mode = RECOVERY;
		}
		else if(strcmp(cmd, "fastboot") == 0) {
			info->mode = FASTBOOT;
		}
		else {
			info->mode = NORMAL;
		}
		ambcache_clean_range(info,sizeof(reboot_info_t));
	}

	rct_reset_chip();
	cpu_reset(CONFIG_AMBARELLA_ZRELADDR);
}

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

