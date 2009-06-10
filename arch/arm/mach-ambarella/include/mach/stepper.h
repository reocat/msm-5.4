/*
 * arch/arm/mach-ambarella/include/mach/stepper.h
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

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <mach/hardware.h> instead"
#endif

#ifndef __ASM_ARCH_STEPPER_H
#define __ASM_ARCH_STEPPER_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define	ST_CONTROL_A_OFFSET		0x000
#define ST_PATTERN0_0A_OFFSET		0x004
#define ST_PATTERN1_0A_OFFSET		0x008
#define ST_PATTERN0_1A_OFFSET		0x00c
#define ST_PATTERN1_1A_OFFSET		0x010
#define ST_PATTERN0_2A_OFFSET		0x014
#define ST_PATTERN1_2A_OFFSET		0x018
#define ST_PATTERN0_3A_OFFSET		0x01c
#define ST_PATTERN1_3A_OFFSET		0x020
#define ST_PATTERN0_4A_OFFSET		0x024
#define ST_PATTERN1_4A_OFFSET		0x028
#define ST_COUNT_A_OFFSET		0x02c
#define ST_CONTROL_B_OFFSET		0x100
#define ST_PATTERN0_0B_OFFSET		0x104
#define ST_PATTERN1_0B_OFFSET		0x108
#define ST_PATTERN0_1B_OFFSET		0x10c
#define ST_PATTERN1_1B_OFFSET		0x110
#define ST_PATTERN0_2B_OFFSET		0x114
#define ST_PATTERN1_2B_OFFSET		0x118
#define ST_PATTERN0_3B_OFFSET		0x11c
#define ST_PATTERN1_3B_OFFSET		0x120
#define ST_PATTERN0_4B_OFFSET		0x124
#define ST_PATTERN1_4B_OFFSET		0x128
#define ST_COUNT_B_OFFSET		0x12c
#define ST_CONTROL_C_OFFSET		0x200
#define ST_PATTERN0_0C_OFFSET		0x204
#define ST_PATTERN1_0C_OFFSET		0x208
#define ST_PATTERN0_1C_OFFSET		0x20c
#define ST_PATTERN1_1C_OFFSET		0x210
#define ST_PATTERN0_2C_OFFSET		0x214
#define ST_PATTERN1_2C_OFFSET		0x218
#define ST_PATTERN0_3C_OFFSET		0x21c
#define ST_PATTERN1_3C_OFFSET		0x220
#define ST_PATTERN0_4C_OFFSET		0x224
#define ST_PATTERN1_4C_OFFSET		0x228
#define ST_COUNT_C_OFFSET		0x22c

#define	ST_CONTROL_A_REG		ST_REG(0x000)
#define ST_PATTERN0_0A_REG		ST_REG(0x004)
#define ST_PATTERN1_0A_REG		ST_REG(0x008)
#define ST_PATTERN0_1A_REG		ST_REG(0x00c)
#define ST_PATTERN1_1A_REG		ST_REG(0x010)
#define ST_PATTERN0_2A_REG		ST_REG(0x014)
#define ST_PATTERN1_2A_REG		ST_REG(0x018)
#define ST_PATTERN0_3A_REG		ST_REG(0x01c)
#define ST_PATTERN1_3A_REG		ST_REG(0x020)
#define ST_PATTERN0_4A_REG		ST_REG(0x024)
#define ST_PATTERN1_4A_REG		ST_REG(0x028)
#define ST_COUNT_A_REG			ST_REG(0x02c)
#define ST_CONTROL_B_REG		ST_REG(0x100)
#define ST_PATTERN0_0B_REG		ST_REG(0x104)
#define ST_PATTERN1_0B_REG		ST_REG(0x108)
#define ST_PATTERN0_1B_REG		ST_REG(0x10c)
#define ST_PATTERN1_1B_REG		ST_REG(0x110)
#define ST_PATTERN0_2B_REG		ST_REG(0x114)
#define ST_PATTERN1_2B_REG		ST_REG(0x118)
#define ST_PATTERN0_3B_REG		ST_REG(0x11c)
#define ST_PATTERN1_3B_REG		ST_REG(0x120)
#define ST_PATTERN0_4B_REG		ST_REG(0x124)
#define ST_PATTERN1_4B_REG		ST_REG(0x128)
#define ST_COUNT_B_REG			ST_REG(0x12c)
#define ST_CONTROL_C_REG		ST_REG(0x200)
#define ST_PATTERN0_0C_REG		ST_REG(0x204)
#define ST_PATTERN1_0C_REG		ST_REG(0x208)
#define ST_PATTERN0_1C_REG		ST_REG(0x20c)
#define ST_PATTERN1_1C_REG		ST_REG(0x210)
#define ST_PATTERN0_2C_REG		ST_REG(0x214)
#define ST_PATTERN1_2C_REG		ST_REG(0x218)
#define ST_PATTERN0_3C_REG		ST_REG(0x21c)
#define ST_PATTERN1_3C_REG		ST_REG(0x220)
#define ST_PATTERN0_4C_REG		ST_REG(0x224)
#define ST_PATTERN1_4C_REG		ST_REG(0x228)
#define ST_COUNT_C_REG			ST_REG(0x22c)

#endif

