/*
 * arch/arm/plat-ambarella/include/plat/syscfg.h
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

#ifndef __PLAT_AMBARELLA_SYSCFG_H
#define __PLAT_AMBARELLA_SYSCFG_H

/* ==========================================================================*/
#ifdef CONFIG_VMSPLIT_3G
#define NOLINUX_MEM_V_START		(0xE0000000)
#else
#define NOLINUX_MEM_V_START		(0xC0000000)
#endif

/*
 * 64MB DDR Sample	Phisical			Virtual
 * --------------------------------------------------------------------------
 * AHB(IO)	0x60000000 - 0x60FFFFFF		0xD8000000 - 0xD8FFFFFF
 * APB(IO)	0x70000000 - 0x70FFFFFF		0xD9000000 - 0xD9FFFFFF
 * PPM		0xC0000000 - 0xC01FFFFF		0xE0000000 - 0xE01FFFFF
 * Linux MEM	0xC0200000 - 0xC15FFFFF		0xC0000000 - 0xC13FFFFF
 * BSB		0xC1600000 - 0xC17FFFFF		(NOLINUX_MEM_V_START + 0x01600000) - (NOLINUX_MEM_V_START + 0x017FFFFF)
 * DSP		0xC1800000 - 0xC3FFFFFF		(NOLINUX_MEM_V_START + 0x01800000) - (NOLINUX_MEM_V_START + 0x03FFFFFF)
 */

#define AHB_START			(0x60000000)
#define AHB_SIZE			(0x01000000)
#define AHB_BASE			(0xD8000000)

#define APB_START			(0x70000000)
#define APB_SIZE			(0x01000000)
#define APB_BASE			(0xD9000000)

#define DEFAULT_BSB_START		(0x00000000)
#define DEFAULT_BSB_BASE		(0x00000000)
#define DEFAULT_BSB_SIZE		(0x00000000)

#define DEFAULT_DSP_START		(0x00000000)
#define DEFAULT_DSP_BASE		(0x00000000)
#define DEFAULT_DSP_SIZE		(0x00000000)

#define DEFAULT_DEBUG_START		(0xC00F8000)
#define DEFAULT_DEBUG_SIZE		(0x00008000)

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
#define DEFAULT_HAL_START		(0xC00A0000)
#define DEFAULT_HAL_BASE		(0xFEE00000)
#define DEFAULT_HAL_SIZE		(0x00030000)

#define HAL_BASE_VP			(get_ambarella_hal_vp())
#endif

#define DEFAULT_SSS_START		(0xC00F7000)
#define DEFAULT_SSS_MAGIC0		(0x19790110)
#define DEFAULT_SSS_MAGIC1		(0x19450107)
#define DEFAULT_SSS_MAGIC2		(0x19531110)

/* ==========================================================================*/

#endif

