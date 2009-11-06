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
 * 64MB DDR Sample	Phisical			Virtual
 * --------------------------------------------------------------------------
 * AHB(IO)	0x60000000 - 0x60FFFFFF		0xD8000000 - 0xD8FFFFFF
 * APB(IO)	0x70000000 - 0x70FFFFFF		0xD9000000 - 0xD9FFFFFF
 * PPM		0xC0000000 - 0xC01FFFFF		0xE0000000 - 0xE01FFFFF
 * Linux MEM	0xC0200000 - 0xC15FFFFF		0xC0000000 - 0xC13FFFFF
 * BSB		0xC1600000 - 0xC17FFFFF		0xE1600000 - 0xE17FFFFF
 * DSP		0xC1800000 - 0xC3FFFFFF		0xE1800000 - 0xE3FFFFFF
 */

#define AHB_START		(0x60000000)
#define AHB_SIZE		(0x01000000)
#define AHB_BASE		(0xD8000000)

#define APB_START		(0x70000000)
#define APB_SIZE		(0x01000000)
#define APB_BASE		(0xD9000000)

#include <mach/busaddr.h>

#include <mach/ahb.h>
#include <mach/apb.h>

#include <mach/config.h>

#endif

