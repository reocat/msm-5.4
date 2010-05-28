/*
 * arch/arm/plat-ambarella/include/plat/pll.h
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

#ifndef __PLAT_AMBARELLA_PLL_H
#define __PLAT_AMBARELLA_PLL_H

/* ==========================================================================*/

/* ==========================================================================*/
#ifndef __ASSEMBLER__

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
#include <hal/ambhal.h>
#include <hal/ambhalmini.h>
#include <hal/header.h>

extern void *get_ambarella_hal_vp(void);
#endif /* CONFIG_PLAT_AMBARELLA_SUPPORT_HAL */

/* ==========================================================================*/
struct ambarella_mem_hal_desc {
	u32 physaddr;
	u32 size;
	u32 virtual;
	u32 remapped;
	u32 inited;
};

/* ==========================================================================*/
extern int ambarella_init_pll(void);

extern u32 ambarella_pll_suspend(u32 level);
extern u32 ambarella_pll_resume(u32 level);

extern void set_ambarella_hal_invalid(void);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

