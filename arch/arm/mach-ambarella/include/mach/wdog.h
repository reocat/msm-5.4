/*
 * arch/arm/mach-ambarella/include/mach/wdog.h
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

#ifndef __ASM_ARCH_WDOG_H
#define __ASM_ARCH_WDOG_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

/* None so far */

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define WDOG_STATUS_OFFSET		0x00
#define WDOG_RELOAD_OFFSET		0x04
#define WDOG_RESTART_OFFSET		0x08
#define WDOG_CONTROL_OFFSET		0x0c
#define WDOG_TIMEOUT_OFFSET		0x10
#define WDOG_CLR_TMO_OFFSET		0x14
#define WDOG_RESET_OFFSET		0x18

#define WDOG_STATUS_REG			WDOG_REG(0x00)
#define WDOG_RELOAD_REG			WDOG_REG(0x04)
#define WDOG_RESTART_REG		WDOG_REG(0x08)
#define WDOG_CONTROL_REG		WDOG_REG(0x0c)
#define WDOG_TIMEOUT_REG		WDOG_REG(0x10)
#define WDOG_CLR_TMO_REG		WDOG_REG(0x14)
#define WDOG_RESET_REG			WDOG_REG(0x18)

/* Bit field definition of watch dog timer control register */
#define WDOG_CTR_INT_EN			0x00000004
#define WDOG_CTR_RST_EN			0x00000002
#define WDOG_CTR_EN			0x00000001

#endif

