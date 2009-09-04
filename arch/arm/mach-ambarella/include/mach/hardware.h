/*
 * arch/arm/mach-ambarella/include/mach/hardware.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
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

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#ifndef __ASSEMBLER__
#include <linux/types.h>
#endif  /* __ASSEMBLER__ */

#include <mach/chip.h>

/*
 *			Phisical			Virtual
 * --------------------------------------------------------------------------
 * AHB(IO)	0x60000000 - 0x60FFFFFF		0xD8000000 - 0xD8FFFFFF
 * APB(IO)	0x70000000 - 0x70FFFFFF		0xD9000000 - 0xD9FFFFFF
 * RESERVE MEM	0xC0000000 - 0xC00FFFFF		0xC0000000 - 0xC00FFFFF
 * Linux MEM	0xC0100000 - 0xC13FFFFF		0xC0100000 - 0xC13FFFFF
 * BSB MEM	0xC1400000 - 0xC17FFFFF		0xE1400000 - 0xE17FFFFF
 * DSP MEM	0xC1800000 - 0xC3FFFFFF		0xE1800000 - 0xE3FFFFFF
 */

#define AHB_START		(0x60000000)
#define AHB_SIZE		(0x01000000)
#define AHB_BASE		(0xD8000000)

#define APB_START		(0x70000000)
#define APB_SIZE		(0x01000000)
#define APB_BASE		(0xD9000000)

#define NOLINUX_MEM_V_START	(0xE0000000)
#define DEFAULT_BSB_MEM_OFFSET	(0x01400000)
#define DEFAULT_DSP_MEM_OFFSET	(0x01800000)
#define DEFAULT_DSP_MEM_SIZE	(0x02800000)

#define RESERVE_MEM_P_START	(0xC0000000)
#define RESERVE_MEM_SIZE	(0x00100000)

#define DEFAULT_ATAG_START	(PHYS_OFFSET + 0x000C0000)
#define DEFAULT_HAL_BASE	(PHYS_OFFSET + 0x000D0000)
#define DEFAULT_HAL_SIZE	(0x00020000)
#define DEFAULT_DEBUG_BASE	(PHYS_OFFSET + 0x000F8000)
#define DEFAULT_DEBUG_SIZE	(0x00008000)
#define HAL_BASE_VP		(get_ambarella_hal_vp())

#include <mach/busaddr.h>

#include <mach/ahb.h>
#include <mach/apb.h>

#include <mach/config.h>

#ifndef __ASSEMBLER__

#if	(CHIP_REV == A5S)
#include <mach/hal/ambhal.h>
#include <mach/hal/ambhalmini.h>
#include <mach/hal/header.h>
#endif

#endif  /* __ASSEMBLER__ */

#endif

