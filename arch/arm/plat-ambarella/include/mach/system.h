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

/* ==========================================================================*/
#include <plat/bapi.h>

#if defined(CONFIG_BOSS_SINGLE_CORE)
#include <mach/boss.h>
#endif

/* ==========================================================================*/
#ifndef __ASSEMBLER__

/* ==========================================================================*/
static inline void arch_idle(void)
{
	cpu_do_idle();

#if defined(CONFIG_BOSS_SINGLE_CORE)
	if (boss) {
		*boss->gidle = 1;
	}
#endif
}

static inline void arch_reset(char mode, const char *cmd)
{
#if defined(CONFIG_AMBARELLA_SUPPORT_BAPI)
	struct ambarella_bapi_reboot_info_s	reboot_info;

	reboot_info.magic = DEFAULT_BAPI_REBOOT_MAGIC;
	reboot_info.mode = DEFAULT_BAPI_REBOOT_NORMAL;
	if (cmd) {
		if(strcmp(cmd, "recovery") == 0) {
			reboot_info.mode = DEFAULT_BAPI_REBOOT_RECOVERY;
		} else if(strcmp(cmd, "fastboot") == 0) {
			reboot_info.mode = DEFAULT_BAPI_REBOOT_FASTBOOT;
		}
	}
	ambarella_bapi_cmd(AMBARELLA_BAPI_CMD_SET_REBOOT_INFO, &reboot_info);
#endif

	amba_writel(SOFT_RESET_REG, 0x0);
	amba_writel(SOFT_RESET_REG, 0x1);
	cpu_reset(CONFIG_AMBARELLA_ZRELADDR);
}

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

