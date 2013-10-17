/*
 * arch/arm/plat-ambarella/include/plat/timer.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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

#ifndef __PLAT_AMBARELLA_TIMER_H__
#define __PLAT_AMBARELLA_TIMER_H__

/* ==========================================================================*/
#define TIMER_OFFSET			0xB000
#define TIMER_BASE			(APB_BASE + TIMER_OFFSET)
#define TIMER_REG(x)			(TIMER_BASE + (x))

/* ==========================================================================*/
#if (CHIP_REV == I1) || (CHIP_REV == A7L) || (CHIP_REV == S2) || \
    (CHIP_REV == A8) || (CHIP_REV == S2L)
#define INTERVAL_TIMER_INSTANCES		8
#else
#define INTERVAL_TIMER_INSTANCES		3
#endif

/* ==========================================================================*/
#define TIMER1_STATUS_OFFSET		0x00
#define TIMER1_RELOAD_OFFSET		0x04
#define TIMER1_MATCH1_OFFSET		0x08
#define TIMER1_MATCH2_OFFSET		0x0c
#define TIMER2_STATUS_OFFSET		0x10
#define TIMER2_RELOAD_OFFSET		0x14
#define TIMER2_MATCH1_OFFSET		0x18
#define TIMER2_MATCH2_OFFSET		0x1c
#define TIMER3_STATUS_OFFSET		0x20
#define TIMER3_RELOAD_OFFSET		0x24
#define TIMER3_MATCH1_OFFSET		0x28
#define TIMER3_MATCH2_OFFSET		0x2c

#if (INTERVAL_TIMER_INSTANCES == 8)
#define TIMER4_STATUS_OFFSET		0x34
#define TIMER4_RELOAD_OFFSET		0x38
#define TIMER4_MATCH1_OFFSET		0x3c
#define TIMER4_MATCH2_OFFSET		0x40
#define TIMER5_STATUS_OFFSET		0x44
#define TIMER5_RELOAD_OFFSET		0x48
#define TIMER5_MATCH1_OFFSET		0x4c
#define TIMER5_MATCH2_OFFSET		0x50
#define TIMER6_STATUS_OFFSET		0x54
#define TIMER6_RELOAD_OFFSET		0x58
#define TIMER6_MATCH1_OFFSET		0x5c
#define TIMER6_MATCH2_OFFSET		0x60
#define TIMER7_STATUS_OFFSET		0x64
#define TIMER7_RELOAD_OFFSET		0x68
#define TIMER7_MATCH1_OFFSET		0x6c
#define TIMER7_MATCH2_OFFSET		0x70
#define TIMER8_STATUS_OFFSET		0x74
#define TIMER8_RELOAD_OFFSET		0x78
#define TIMER8_MATCH1_OFFSET		0x7c
#define TIMER8_MATCH2_OFFSET		0x80
#endif

#define TIMER_CTR_OFFSET		0x30

#define TIMER1_STATUS_REG		TIMER_REG(TIMER1_STATUS_OFFSET)
#define TIMER1_RELOAD_REG		TIMER_REG(TIMER1_RELOAD_OFFSET)
#define TIMER1_MATCH1_REG		TIMER_REG(TIMER1_MATCH1_OFFSET)
#define TIMER1_MATCH2_REG		TIMER_REG(TIMER1_MATCH2_OFFSET)
#define TIMER2_STATUS_REG		TIMER_REG(TIMER2_STATUS_OFFSET)
#define TIMER2_RELOAD_REG		TIMER_REG(TIMER2_RELOAD_OFFSET)
#define TIMER2_MATCH1_REG		TIMER_REG(TIMER2_MATCH1_OFFSET)
#define TIMER2_MATCH2_REG		TIMER_REG(TIMER2_MATCH2_OFFSET)
#define TIMER3_STATUS_REG		TIMER_REG(TIMER3_STATUS_OFFSET)
#define TIMER3_RELOAD_REG		TIMER_REG(TIMER3_RELOAD_OFFSET)
#define TIMER3_MATCH1_REG		TIMER_REG(TIMER3_MATCH1_OFFSET)
#define TIMER3_MATCH2_REG		TIMER_REG(TIMER3_MATCH2_OFFSET)
#if (INTERVAL_TIMER_INSTANCES == 8)
#define TIMER4_STATUS_REG		TIMER_REG(TIMER4_STATUS_OFFSET)
#define TIMER4_RELOAD_REG		TIMER_REG(TIMER4_RELOAD_OFFSET)
#define TIMER4_MATCH1_REG		TIMER_REG(TIMER4_MATCH1_OFFSET)
#define TIMER4_MATCH2_REG		TIMER_REG(TIMER4_MATCH2_OFFSET)
#define TIMER5_STATUS_REG		TIMER_REG(TIMER5_STATUS_OFFSET)
#define TIMER5_RELOAD_REG		TIMER_REG(TIMER5_RELOAD_OFFSET)
#define TIMER5_MATCH1_REG		TIMER_REG(TIMER5_MATCH1_OFFSET)
#define TIMER5_MATCH2_REG		TIMER_REG(TIMER5_MATCH2_OFFSET)
#define TIMER6_STATUS_REG		TIMER_REG(TIMER6_STATUS_OFFSET)
#define TIMER6_RELOAD_REG		TIMER_REG(TIMER6_RELOAD_OFFSET)
#define TIMER6_MATCH1_REG		TIMER_REG(TIMER6_MATCH1_OFFSET)
#define TIMER6_MATCH2_REG		TIMER_REG(TIMER6_MATCH2_OFFSET)
#define TIMER7_STATUS_REG		TIMER_REG(TIMER7_STATUS_OFFSET)
#define TIMER7_RELOAD_REG		TIMER_REG(TIMER7_RELOAD_OFFSET)
#define TIMER7_MATCH1_REG		TIMER_REG(TIMER7_MATCH1_OFFSET)
#define TIMER7_MATCH2_REG		TIMER_REG(TIMER7_MATCH2_OFFSET)
#define TIMER8_STATUS_REG		TIMER_REG(TIMER8_STATUS_OFFSET)
#define TIMER8_RELOAD_REG		TIMER_REG(TIMER8_RELOAD_OFFSET)
#define TIMER8_MATCH1_REG		TIMER_REG(TIMER8_MATCH1_OFFSET)
#define TIMER8_MATCH2_REG		TIMER_REG(TIMER8_MATCH2_OFFSET)
#endif

#define TIMER_CTR_REG			TIMER_REG(TIMER_CTR_OFFSET)

#if (INTERVAL_TIMER_INSTANCES == 8)
#define TIMER_CTR_OF8			0x40000000
#define TIMER_CTR_CSL8			0x20000000
#define TIMER_CTR_EN8			0x10000000
#define TIMER_CTR_OF7			0x04000000
#define TIMER_CTR_CSL7			0x02000000
#define TIMER_CTR_EN7			0x01000000
#define TIMER_CTR_OF6			0x00400000
#define TIMER_CTR_CSL6			0x00200000
#define TIMER_CTR_EN6			0x00100000
#define TIMER_CTR_OF5			0x00040000
#define TIMER_CTR_CSL5			0x00020000
#define TIMER_CTR_EN5			0x00010000
#define TIMER_CTR_OF4			0x00004000
#define TIMER_CTR_CSL4			0x00002000
#define TIMER_CTR_EN4			0x00001000
#endif
#define TIMER_CTR_OF3			0x00000400
#define TIMER_CTR_CSL3			0x00000200
#define TIMER_CTR_EN3			0x00000100
#define TIMER_CTR_OF2			0x00000040
#define TIMER_CTR_CSL2			0x00000020
#define TIMER_CTR_EN2			0x00000010
#define TIMER_CTR_OF1			0x00000004
#define TIMER_CTR_CSL1			0x00000002
#define TIMER_CTR_EN1			0x00000001

/* ==========================================================================*/
#ifndef __ASSEMBLER__

/* ==========================================================================*/
extern struct sys_timer ambarella_timer;

/* ==========================================================================*/
extern void ambarella_timer_init(void);

extern u32 ambarella_timer_suspend(u32 level);
extern u32 ambarella_timer_resume(u32 level);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif /* __PLAT_AMBARELLA_TIMER_H__ */

