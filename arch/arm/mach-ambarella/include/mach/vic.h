/*
 * arch/arm/mach-ambarella/include/mach/vic.h
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

#ifndef __ASM_ARCH_VIC_H
#define __ASM_ARCH_VIC_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A3) || (CHIP_REV == A5) || (CHIP_REV == A6)
#define VIC_INSTANCES	2
#else
#define VIC_INSTANCES	1
#endif

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define VIC_IRQ_STA_OFFSET		0x00
#define VIC_FIQ_STA_OFFSET		0x04
#define VIC_RAW_STA_OFFSET		0x08
#define VIC_INT_SEL_OFFSET		0x0c
#define VIC_INTEN_OFFSET		0x10
#define VIC_INTEN_CLR_OFFSET		0x14
#define VIC_SOFTEN_OFFSET		0x18
#define VIC_SOFTEN_CLR_OFFSET		0x1c
#define VIC_PROTEN_OFFSET		0x20
#define VIC_SENSE_OFFSET		0x24
#define VIC_BOTHEDGE_OFFSET		0x28
#define VIC_EVENT_OFFSET		0x2c
#define VIC_EDGE_CLR_OFFSET		0x38

#define VIC_IRQ_STA_REG			VIC_REG(0x00)
#define VIC_FIQ_STA_REG			VIC_REG(0x04)
#define VIC_RAW_STA_REG			VIC_REG(0x08)
#define VIC_INT_SEL_REG			VIC_REG(0x0c)
#define VIC_INTEN_REG			VIC_REG(0x10)
#define VIC_INTEN_CLR_REG		VIC_REG(0x14)
#define VIC_SOFTEN_REG			VIC_REG(0x18)
#define VIC_SOFTEN_CLR_REG		VIC_REG(0x1c)
#define VIC_PROTEN_REG			VIC_REG(0x20)
#define VIC_SENSE_REG			VIC_REG(0x24)
#define VIC_BOTHEDGE_REG		VIC_REG(0x28)
#define VIC_EVENT_REG			VIC_REG(0x2c)
#define VIC_EDGE_CLR_REG		VIC_REG(0x38)

#if (VIC_INSTANCES == 2)

#define VIC2_IRQ_STA_REG		VIC2_REG(0x00)
#define VIC2_FIQ_STA_REG		VIC2_REG(0x04)
#define VIC2_RAW_STA_REG		VIC2_REG(0x08)
#define VIC2_INT_SEL_REG		VIC2_REG(0x0c)
#define VIC2_INTEN_REG			VIC2_REG(0x10)
#define VIC2_INTEN_CLR_REG		VIC2_REG(0x14)
#define VIC2_SOFTEN_REG			VIC2_REG(0x18)
#define VIC2_SOFTEN_CLR_REG		VIC2_REG(0x1c)
#define VIC2_PROTEN_REG			VIC2_REG(0x20)
#define VIC2_SENSE_REG			VIC2_REG(0x24)
#define VIC2_BOTHEDGE_REG		VIC2_REG(0x28)
#define VIC2_EVENT_REG			VIC2_REG(0x2c)
#define VIC2_EDGE_CLR_REG		VIC2_REG(0x38)

#endif

#endif

