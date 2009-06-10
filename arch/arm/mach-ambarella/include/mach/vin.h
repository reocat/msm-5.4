/*
 * arch/arm/mach-ambarella/include/mach/vin.h
 *
 * History:
 *	2007/01/27 - [Charles Chiou] created file
 *	2008/02/19 - [Allen Wang] changed to use capabilities and chip ID
 *	2008/05/13 - [Allen Wang] added capabilities of A2S and A2M silicons 	 
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

#ifndef __ASM_ARCH_VIN_H
#define __ASM_ARCH_VIN_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A1) || (CHIP_REV == A2) || 		\
	(CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define VIN_DIRECT_DSP_INTERFACE	0
#define VIN_SUPPORT_BLC			1
#else
#define VIN_DIRECT_DSP_INTERFACE	1
#define VIN_SUPPORT_BLC			0
#endif

#if (CHIP_REV == A1)
#define VIN_SMEM_PREVIEW_INSTANCES	0
#elif (CHIP_REV == A2) || (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define VIN_SMEM_PREVIEW_INSTANCES	1
#else
#define VIN_SMEM_PREVIEW_INSTANCES	2
#endif

#if (CHIP_REV == A5) || (CHIP_REV == A6)
#define VIN_SUPPORT_SLVS_MLVS		1
#else
#define VIN_SUPPORT_SLVS_MLVS		0
#endif

#if (CHIP_REV == A3) || (CHIP_REV == A5) || (CHIP_REV == A6)
#define VIN_SUPPORT_LVDS_LVCMOS		1
#else
#define VIN_SUPPORT_LVDS_LVCMOS		0
#endif

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/
#if (VIN_SUPPORT_LVDS_LVCMOS == 0)

#define S_CTRL_OFFSET			0x00
#define S_STATUS_OFFSET			0x08
#define S_HV_WIDTH_OFFSET               0x0c
#define S_H_OFFSET_OFFSET               0x10
#define S_HV_OFFSET                     0x14
#define S_MIN_HV_OFFSET			0x18
#define S_TRIGGER0_OFFSET		0x1c
#define S_TRIGGER1_OFFSET		0x20
#define S_BLHW_AVGV_OFFSET		0x24

#if (VIN_SMEM_PREVIEW_INSTANCES == 1)
#define S_VOUT_START_OFFSET		0x28
#endif

#define S_CAP_START_OFFSET		0x2c
#define S_CAP_END_OFFSET		0x30
#define S_BLHW_CTRL_OFFSET		0x34
#define S_BLHW_AVGH_OFFSET		0x38
#define S_BLHWOO_OFFSET			0x3c
#define S_BLHWOE_OFFSET			0x40
#define S_BLHWEO_OFFSET			0x44
#define S_BLHWEE_OFFSET			0x48
#define S_BLSWOO_OFFSET			0x4c
#define S_BLSWOE_OFFSET			0x50
#define S_BLSWEO_OFFSET			0x54
#define S_BLSWEE_OFFSET			0x58
#define S_DEBUG_FIFO_COUNT_OFFSET	0x64
#define S_DEBUG_FIFO_DATA_OFFSET	0x80

#define S_CTRL_REG			VIN_REG(0x00)
#define S_STATUS_REG			VIN_REG(0x08)
#define S_HV_WIDTH                      VIN_REG(0x0c)
#define S_H_OFFSET                      VIN_REG(0x10)
#define S_HV                            VIN_REG(0x14)
#define S_MIN_HV_REG			VIN_REG(0x18)
#define S_TRIGGER0_REG			VIN_REG(0x1c)
#define S_TRIGGER1_REG			VIN_REG(0x20)
#define S_BLHW_AVGV_REG 		VIN_REG(0x24)

#if (VIN_SMEM_PREVIEW_INSTANCES == 1)
#define S_VOUT_START_REG		VIN_REG(0x28)
#endif

#define S_CAP_START_REG			VIN_REG(0x2c)
#define S_CAP_END_REG			VIN_REG(0x30)
#define S_BLHW_CTRL_REG			VIN_REG(0x34)
#define S_BLHW_AVGH_REG			VIN_REG(0x38)
#define S_BLHWOO_REG			VIN_REG(0x3c)
#define S_BLHWOE_REG			VIN_REG(0x40)
#define S_BLHWEO_REG			VIN_REG(0x44)
#define S_BLHWEE_REG			VIN_REG(0x48)
#define S_BLSWOO_REG			VIN_REG(0x4c)
#define S_BLSWOE_REG			VIN_REG(0x50)
#define S_BLSWEO_REG			VIN_REG(0x54)
#define S_BLSWEE_REG			VIN_REG(0x58)
#define S_DEBUG_FIFO_COUNT_REG		VIN_REG(0x64)
#define S_DEBUG_FIFO_DATA_REG		VIN_REG(0x80)

#endif 

#endif

