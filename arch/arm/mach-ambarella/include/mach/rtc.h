/*
 * arch/arm/mach-ambarella/include/mach/rtc.h
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

#ifndef __ASM_ARCH_RTC_H
#define __ASM_ARCH_RTC_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A1)
#define RTC_SUPPORT_ONCHIP_INSTANCE	0 
#else
#define RTC_SUPPORT_ONCHIP_INSTANCE	1
#endif

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/
#if (RTC_SUPPORT_ONCHIP_INSTANCE == 1)

#define RTC_POS0_OFFSET			0x20
#define RTC_POS1_OFFSET			0x24
#define RTC_POS2_OFFSET			0x28
#define RTC_PWC_ALAT_OFFSET		0x2c
#define RTC_PWC_CURT_OFFSET		0x30
#define RTC_CURT_OFFSET			0x34
#define RTC_ALAT_OFFSET			0x38
#define RTC_STATUS_OFFSET		0x3c
#define RTC_RESET_OFFSET		0x40

#define RTC_POS0_REG			RTC_REG(0x20)
#define RTC_POS1_REG			RTC_REG(0x24)
#define RTC_POS2_REG			RTC_REG(0x28)
#define RTC_PWC_ALAT_REG		RTC_REG(0x2c)
#define RTC_PWC_CURT_REG		RTC_REG(0x30)
#define RTC_CURT_REG			RTC_REG(0x34)
#define RTC_ALAT_REG			RTC_REG(0x38)
#define RTC_STATUS_REG			RTC_REG(0x3c)
#define RTC_RESET_REG			RTC_REG(0x40)

/* RTC_STATUS_REG */
#define RTC_STATUS_WKUP			0x8
#define RTC_STATUS_ALA_WK		0x4
#define RTC_STATUS_PC_RST		0x2
#define RTC_STATUS_RTC_CLK		0x1

#endif 

#endif

