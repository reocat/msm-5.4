/*
 * arch/arm/mach-ambarella/include/mach/ir.h
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

#ifndef __ASM_ARCH_IR_H
#define __ASM_ARCH_IR_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

/* None so far */

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define IR_CONTROL_OFFSET		0x00
#define IR_STATUS_OFFSET		0x04
#define IR_DATA_OFFSET			0x08

#define IR_CONTROL_REG			IR_REG(0x00)
#define IR_STATUS_REG			IR_REG(0x04)
#define IR_DATA_REG			IR_REG(0x08)

/* IR_CONTROL_REG */
#define IR_CONTROL_RESET		0x00004000
#define IR_CONTROL_ENB			0x00002000
#define IR_CONTROL_LEVINT		0x00001000
#define IR_CONTROL_INTLEV(x)		(((x) & 0x3f)  << 4)
#define IR_CONTROL_FIFO_OV		0x00000008
#define IR_CONTROL_INTENB		0x00000004

#if (CHIP_REV == 2)
#define IR_STATUS_COUNT(x)		((x) & 0x3f)
#define IR_DATA_DATA(x)			((x) & 0xffff)
#endif

#endif

