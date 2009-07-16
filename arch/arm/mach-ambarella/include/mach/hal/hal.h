/*
 * arch/arm/mach-ambarella/include/mach/hal/hal.h
 *
 * History:
 *    2007/11/29 - [Charles Chiou] created file
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

#ifndef __HAL_H__
#define __HAL_H__

#ifndef	__ASM__

#include <mach/hal/header.h>

#if	(CHIP_REV == A5S)
#include <mach/hal/ambhal.h>
#include <mach/hal/ambhalmini.h>
#endif

extern int hal_init(void);
extern int hal_call_name(int, char **);
extern int hal_call_fid(u32, u32, u32, u32, u32);
extern u32 g_haladdr;

#define IS_HAL_INIT()	(g_haladdr == HAL_BASE)

#endif	/* __ASM__ */

#define HAL_BASE	0xc00a0000
#define HAL_BASE_VP	((void *) 0xc00a0000)

#if	(CHIP_REV == A5S)
#define	USE_HAL		1
#endif

#endif
