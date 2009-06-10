/*
 * arch/arm/mach-ambarella/include/mach/chip.h
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

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <mach/hardware.h> instead"
#endif

#ifndef __ASM_ARCH_CHIP_H__
#define __ASM_ARCH_CHIP_H__

#define A1	1000
#define A2	2000
#define A2S	2100
#define A2M	2200
#define A2Q	2300
#define A3	3000
#define A5	5000
#define A5M	5100
#define A6	6000

#if	defined(CONFIG_AMBARELLA_CHIP_A1)
#define CHIP_REV	A1
#elif	defined(CONFIG_AMBARELLA_CHIP_A2)
#define CHIP_REV	A2
#elif	defined(CONFIG_AMBARELLA_CHIP_A2S)
#define CHIP_REV	A2S
#elif	defined(CONFIG_AMBARELLA_CHIP_A2M)
#define CHIP_REV	A2M
#elif	defined(CONFIG_AMBARELLA_CHIP_A2Q)
#define CHIP_REV	A2Q
#elif	defined(CONFIG_AMBARELLA_CHIP_A3)
#define CHIP_REV	A3
#elif	defined(CONFIG_AMBARELLA_CHIP_A5)
#define CHIP_REV	A5
#elif	defined(CONFIG_AMBARELLA_CHIP_A5M)
#define CHIP_REV	A5M
#elif	defined(CONFIG_AMBARELLA_CHIP_A6)
#define CHIP_REV	A6
#else
#error "Undefined CHIP_REV"
#endif

#endif

