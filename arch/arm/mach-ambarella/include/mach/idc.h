/*
 * arch/arm/mach-ambarella/include/mach/idc.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
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

#ifndef __ASM_ARCH_IDC_H
#define __ASM_ARCH_IDC_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if 	(CHIP_REV == A1) || (CHIP_REV == A2) || 		\
	(CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define IDC_INSTANCES		1	
#else
#define IDC_INSTANCES		2
#endif

#if (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define IDC_SUPPORT_PIN_MUXING_FOR_HDMI         1
#else
#define IDC_SUPPORT_PIN_MUXING_FOR_HDMI         0
#endif

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define IDC_ENR_OFFSET			0x00
#define IDC_CTRL_OFFSET			0x04
#define IDC_DATA_OFFSET			0x08
#define IDC_STS_OFFSET			0x0c
#define IDC_PSLL_OFFSET			0x10
#define IDC_PSLH_OFFSET			0x14
#define IDC_FMCTRL_OFFSET		0x18
#define IDC_FMDATA_OFFSET		0x1c

#define IDC_ENR_REG			IDC_REG(0x00)
#define IDC_CTRL_REG			IDC_REG(0x04)
#define IDC_DATA_REG			IDC_REG(0x08)
#define IDC_STS_REG			IDC_REG(0x0c)
#define IDC_PSLL_REG			IDC_REG(0x10)
#define IDC_PSLH_REG			IDC_REG(0x14)
#define IDC_FMCTRL_REG			IDC_REG(0x18)
#define IDC_FMDATA_REG			IDC_REG(0x1c)

#if 	(IDC_INSTANCES == 2)
#define IDC2_ENR_REG			IDC2_REG(0x00)
#define IDC2_CTRL_REG			IDC2_REG(0x04)
#define IDC2_DATA_REG			IDC2_REG(0x08)
#define IDC2_STS_REG			IDC2_REG(0x0c)
#define IDC2_PSLL_REG			IDC2_REG(0x10)
#define IDC2_PSLH_REG			IDC2_REG(0x14)
#define IDC2_FMCTRL_REG			IDC2_REG(0x18)
#define IDC2_FMDATA_REG			IDC2_REG(0x1c)
#endif

#define IDC_MEM_REGION_SIZE	(0x20)

#define IDC_ENR_REG_ENABLE	(0x01)
#define IDC_ENR_REG_DISABLE	(0x00)

#define IDC_CTRL_STOP		(0x08)
#define IDC_CTRL_START		(0x04)
#define IDC_CTRL_IF		(0x02)
#define IDC_CTRL_ACK		(0x01)
#define IDC_CTRL_CLS		(0x00)

#define IDC_FIFO_BUF_SIZE	(63)

#define IDC_FMCTRL_STOP		(0x08)
#define IDC_FMCTRL_START	(0x04)
#define IDC_FMCTRL_IF		(0x02)

#define I2C_M_PIN_MUXING	(0x8000)

#endif

