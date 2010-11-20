/*
 * arch/arm/plat-ambarella/include/hal/hal.h
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

#ifndef __PLAT_AMBARELLA_HAL_H
#define __PLAT_AMBARELLA_HAL_H

/* ==========================================================================*/
#ifndef __ASM_ARCH_HARDWARE_H
#error "include <mach/hardware.h> first"
#endif

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_MMAP_NEW)
#define DEFAULT_HAL_START		(0x000a0000)
#else
#define DEFAULT_HAL_START		(0xc00a0000)
#endif
#define DEFAULT_HAL_BASE		(0xfee00000)
#define DEFAULT_HAL_SIZE		(0x00030000)

#define HAL_BASE_VP			(get_ambarella_hal_vp())

#include <hal/header.h>
#if (CHIP_REV == A7)
#include <hal/a7/ambhal.h>
#include <hal/a7/ambhalmini.h>
#elif (CHIP_REV == A5S)
#include <hal/a5s/ambhal.h>
#include <hal/a5s/ambhalmini.h>
#elif (CHIP_REV == I1)
#include <hal/i1/ambhal.h>
#include <hal/i1/ambhalmini.h>
#else
#error "Undefined CHIP_REV, Can't support HAL!"
#endif

#endif /* CONFIG_PLAT_AMBARELLA_SUPPORT_HAL */

/* ==========================================================================*/
#ifndef __ASSEMBLER__

/* ==========================================================================*/

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

