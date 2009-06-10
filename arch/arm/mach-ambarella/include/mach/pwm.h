/*
 * arch/arm/mach-ambarella/include/mach/pwm.h
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

#ifndef __ASM_ARCH_PWM_H
#define __ASM_ARCH_PWM_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A1 || CHIP_REV == A2S || CHIP_REV == A2M || CHIP_REV == A6)
#define	PWM_SUPPRT_2_FIELDS	0
#else
#define	PWM_SUPPRT_2_FIELDS	1
#endif

/* None so far */
#define PWM_MAX_INSTANCES	4

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define PWM_CONTROL_OFFSET		0x00
#define	PWM_ENABLE_OFFSET		0x04
#define PWM_MODE_OFFSET			0x08
#define PWM_CONTROL1_OFFSET		0x0c

#define PWM_CONTROL_REG			PWM_REG(0x00)
#define	PWM_ENABLE_REG			PWM_REG(0x04)
#define PWM_MODE_REG			PWM_REG(0x08)
#define PWM_CONTROL1_REG		PWM_REG(0x0c)

/* Pins shared with Stepper interface */
#define	PWM_ST_REG(x)			(STEPPER_BASE + (x))

#define PWM_B0_DATA_OFFSET		0x300
#define PWM_B0_ENABLE_OFFSET		0x304
#define PWM_C0_DATA_OFFSET		0x310
#define PWM_C0_ENABLE_OFFSET		0x314
#define PWM_B0_DATA1_OFFSET		0x320
#define PWM_C0_DATA1_OFFSET		0x328
#if (PWM_SUPPRT_2_FIELDS == 0)
#define PWM_B1_DATA_OFFSET		PWM_B0_DATA_OFFSET
#define PWM_B1_ENABLE_OFFSET		PWM_B0_ENABLE_OFFSET
#define PWM_C1_DATA_OFFSET		PWM_C0_DATA_OFFSET
#define PWM_C1_ENABLE_OFFSET		PWM_C0_ENABLE_OFFSET
#define PWM_B1_DATA1_OFFSET		PWM_B0_DATA1_OFFSET
#define PWM_C1_DATA1_OFFSET		PWM_C0_DATA1_OFFSET
#else
#define PWM_B1_DATA_OFFSET		0x308
#define PWM_B1_ENABLE_OFFSET		0x30c
#define PWM_C1_DATA_OFFSET		0x318
#define PWM_C1_ENABLE_OFFSET		0x31c
#define PWM_B1_DATA1_OFFSET		0x324
#define PWM_C1_DATA1_OFFSET		0x32c
#endif

#define PWM_B0_DATA_REG			PWM_ST_REG(0x300)
#define PWM_B0_ENABLE_REG		PWM_ST_REG(0x304)
#define PWM_C0_DATA_REG			PWM_ST_REG(0x310)
#define PWM_C0_ENABLE_REG		PWM_ST_REG(0x314)
#define PWM_B0_DATA1_REG		PWM_ST_REG(0x320)
#define PWM_C0_DATA1_REG		PWM_ST_REG(0x328)

#if (PWM_SUPPRT_2_FIELDS == 0)
#define PWM_B1_DATA_REG			PWM_B0_DATA_REG
#define PWM_B1_ENABLE_REG		PWM_B0_ENABLE_REG
#define PWM_C1_DATA_REG			PWM_C0_DATA_REG
#define PWM_C1_ENABLE_REG		PWM_C0_ENABLE_REG
#define PWM_B1_DATA1_REG		PWM_B0_DATA1_REG
#define PWM_C1_DATA1_REG		PWM_C0_DATA1_REG
#else /* (PWM_SUPPRT_2_FIELDS == 1) */
#define PWM_B1_DATA_REG			PWM_ST_REG(0x308)
#define PWM_B1_ENABLE_REG		PWM_ST_REG(0x30c)
#define PWM_C1_DATA_REG			PWM_ST_REG(0x318)
#define PWM_C1_ENABLE_REG		PWM_ST_REG(0x31c)
#define PWM_B1_DATA1_REG		PWM_ST_REG(0x324)
#define PWM_C1_DATA1_REG		PWM_ST_REG(0x32c) 
#endif

#endif

