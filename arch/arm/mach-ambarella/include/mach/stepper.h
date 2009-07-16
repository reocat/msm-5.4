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
#if (CHIP_REV == A5 || CHIP_REV == A2S || CHIP_REV == A6 || \
     CHIP_REV == A5S)
#define	ST_SUPPORT_PRECOUNT	1
#define	ST_SUPPORT_ACC_MODE	1
#else
#define	ST_SUPPORT_PRECOUNT	0
#define	ST_SUPPORT_ACC_MODE	0
#endif

#if (CHIP_REV == A5S)
#define	ST_SUPPORT_MICROST	1
#else
#define	ST_SUPPORT_MICROST	0
#endif

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
#define ST_STATUS_A_OFFSET		0x030
#define ST_ACC_COUNT_A_OFFSET		0x034
#define ST_ACC_CLKDIV_0A_OFFSET		0x038
#define ST_ACC_CLKDIV_1A_OFFSET		0x03c
#define ST_PRE_COUNTER_A_OFFSET		0x040
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
#define ST_STATUS_B_OFFSET		0x130
#define ST_ACC_COUNT_B_OFFSET		0x134
#define ST_ACC_CLKDIV_0B_OFFSET		0x138
#define ST_ACC_CLKDIV_1B_OFFSET		0x13c
#define ST_PRE_COUNTER_B_OFFSET		0x140
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
#define ST_STATUS_C_OFFSET		0x230
#define ST_ACC_COUNT_C_OFFSET		0x234
#define ST_ACC_CLKDIV_0C_OFFSET		0x238
#define ST_ACC_CLKDIV_1C_OFFSET		0x23c
#define ST_PRE_COUNTER_C_OFFSET		0x240
#define MST_CONTROL_A_OFFSET		0x400
#define MST_PWM_A_OFFSET		0x404
#define MST_COUNT0_A_OFFSET		0x408
#define MST_COUNT1_A_OFFSET		0x40c
#define MST_STATUS_A_OFFSET		0x410
#define MST_PRE_COUNT_A_OFFSET		0x414
#define MST_CONTROL_B_OFFSET		0x500
#define MST_PWM_B_OFFSET		0x504
#define MST_COUNT0_B_OFFSET		0x508
#define MST_COUNT1_B_OFFSET		0x50c
#define MST_STATUS_B_OFFSET		0x510
#define MST_PRE_COUNT_B_OFFSET		0x514
#define MST_CONTROL_C_OFFSET		0x600
#define MST_PWM_C_OFFSET		0x604
#define MST_COUNT0_C_OFFSET		0x608
#define MST_COUNT1_C_OFFSET		0x60c
#define MST_STATUS_C_OFFSET		0x610
#define MST_PRE_COUNT_C_OFFSET		0x614
#define MST_WAVE_A1_OFFSET		0x800
#define MST_WAVE_A2_OFFSET		0x880
#define MST_WAVE_A3_OFFSET		0x900
#define MST_WAVE_A4_OFFSET		0x980
#define MST_WAVE_B1_OFFSET		0xa00
#define MST_WAVE_B2_OFFSET		0xa80
#define MST_WAVE_B3_OFFSET		0xb00
#define MST_WAVE_B4_OFFSET		0xb80
#define MST_WAVE_C1_OFFSET		0xc00
#define MST_WAVE_C2_OFFSET		0xc80
#define MST_WAVE_C3_OFFSET		0xd00
#define MST_WAVE_C4_OFFSET		0xd80

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
#define ST_STATUS_A_REG			ST_REG(0x030)
#define ST_ACC_COUNT_A_REG		ST_REG(0x034)
#define ST_ACC_CLKDIV_0A_REG		ST_REG(0x038)
#define ST_ACC_CLKDIV_1A_REG		ST_REG(0x03c)
#define ST_PRE_COUNTER_A_REG		ST_REG(0x040)
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
#define ST_STATUS_B_REG			ST_REG(0x130)
#define ST_ACC_COUNT_B_REG		ST_REG(0x134)
#define ST_ACC_CLKDIV_0B_REG		ST_REG(0x138)
#define ST_ACC_CLKDIV_1B_REG		ST_REG(0x13c)
#define ST_PRE_COUNTER_B_REG		ST_REG(0x140)
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
#define ST_STATUS_C_REG			ST_REG(0x230)
#define ST_ACC_COUNT_C_REG		ST_REG(0x234)
#define ST_ACC_CLKDIV_0C_REG		ST_REG(0x238)
#define ST_ACC_CLKDIV_1C_REG		ST_REG(0x23c)
#define ST_PRE_COUNTER_C_REG		ST_REG(0x240)
#define MST_CONTROL_A_REG		ST_REG(0x400)
#define MST_PWM_A_REG			ST_REG(0x404)
#define MST_COUNT0_A_REG		ST_REG(0x408)
#define MST_COUNT1_A_REG		ST_REG(0x40c)
#define MST_STATUS_A_REG		ST_REG(0x410)
#define MST_PRE_COUNT_A_REG		ST_REG(0x414)
#define MST_CONTROL_B_REG		ST_REG(0x500)
#define MST_PWM_B_REG			ST_REG(0x504)
#define MST_COUNT0_B_REG		ST_REG(0x508)
#define MST_COUNT1_B_REG		ST_REG(0x50c)
#define MST_STATUS_B_REG		ST_REG(0x510)
#define MST_PRE_COUNT_B_REG		ST_REG(0x514)
#define MST_CONTROL_C_REG		ST_REG(0x600)
#define MST_PWM_C_REG			ST_REG(0x604)
#define MST_COUNT0_C_REG		ST_REG(0x608)
#define MST_COUNT1_C_REG		ST_REG(0x60c)
#define MST_STATUS_C_REG		ST_REG(0x610)
#define MST_PRE_COUNT_C_REG		ST_REG(0x614)
#define MST_WAVE_A1_REG			ST_REG(0x800)
#define MST_WAVE_A2_REG			ST_REG(0x880)
#define MST_WAVE_A3_REG			ST_REG(0x900)
#define MST_WAVE_A4_REG			ST_REG(0x980)
#define MST_WAVE_B1_REG			ST_REG(0xa00)
#define MST_WAVE_B2_REG			ST_REG(0xa80)
#define MST_WAVE_B3_REG			ST_REG(0xb00)
#define MST_WAVE_B4_REG			ST_REG(0xb80)
#define MST_WAVE_C1_REG			ST_REG(0xc00)
#define MST_WAVE_C2_REG			ST_REG(0xc80)
#define MST_WAVE_C3_REG			ST_REG(0xd00)
#define MST_WAVE_C4_REG			ST_REG(0xd80)

#endif

