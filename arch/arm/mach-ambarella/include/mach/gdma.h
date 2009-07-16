/*
 * arch/arm/mach-ambarella/include/mach/gdma.h
 *
 * History:
 *	2009/03/12 - [Jack Huang] created file
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

#ifndef __ASM_ARCH_GDMA_H
#define __ASM_ARCH_GDMA_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A3) || (CHIP_REV == A5) || (CHIP_REV == A6) || \
    (CHIP_REV == A5S)
#define NUMBERS_GDMA_INSTANCES	8
#else
#define NUMBERS_GDMA_INSTANCES	0
#endif

#if (CHIP_REV == A5S)
#define GDMA_ON_AHB		1
#else
#define GDMA_ON_AHB		0
#endif


/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#if (GDMA_ON_AHB == 1)
#define GDMA_SRC_1_BASE_OFFSET		0x00
#define GDMA_SRC_1_PITCH_OFFSET		0x04
#define GDMA_SRC_2_BASE_OFFSET		0x08
#define GDMA_SRC_2_PITCH_OFFSET		0x0c
#define GDMA_DST_BASE_OFFSET		0x10
#define GDMA_DST_PITCH_OFFSET		0x14
#define GDMA_WIDTH_OFFSET		0x18
#define GDMA_HIGHT_OFFSET		0x1c
#define GDMA_TRANSPARENT_OFFSET		0x20
#define GDMA_OPCODE_OFFSET		0x24
#define GDMA_PENDING_OPS_OFFSET		0x28

#define GDMA_SRC_1_BASE_REG		GRAPHICS_DMA_REG(0x00)
#define GDMA_SRC_1_PITCH_REG		GRAPHICS_DMA_REG(0x04)
#define GDMA_SRC_2_BASE_REG		GRAPHICS_DMA_REG(0x08)
#define GDMA_SRC_2_PITCH_REG		GRAPHICS_DMA_REG(0x0c)
#define GDMA_DST_BASE_REG		GRAPHICS_DMA_REG(0x10)
#define GDMA_DST_PITCH_REG		GRAPHICS_DMA_REG(0x14)
#define GDMA_WIDTH_REG			GRAPHICS_DMA_REG(0x18)
#define GDMA_HEIGHT_REG			GRAPHICS_DMA_REG(0x1c)
#define GDMA_TRANSPARENT_REG		GRAPHICS_DMA_REG(0x20)
#define GDMA_OPCODE_REG			GRAPHICS_DMA_REG(0x24)
#define GDMA_PENDING_OPS_REG		GRAPHICS_DMA_REG(0x28)
#endif /*	GDMA_ON_AHB == 1	*/

#endif
