/*
 * arch/arm/plat-ambarella/include/plat/reboot.h
 *
 * Author: Eric Chen <pzchen@ambarella.com>
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

#ifndef __PLAT_AMBARELLA_REBOOT_H
#define __PLAT_AMBARELLA_REBOOT_H

/* ==========================================================================*/
#ifndef __ASSEMBLER__

/* ==========================================================================*/
extern u32				ambarella_reboot_info;

#define REBOOT_MAGIC    0x4a32e9b0
#define NORMAL      0
#define RECOVERY    1
#define FASTBOOT    2

#define REBOOT_INFO_SIZE	sizeof(reboot_info_t)
typedef struct _reboot_info
{
    int magic;
    int mode;
	int reserve[2];
}reboot_info_t;

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

