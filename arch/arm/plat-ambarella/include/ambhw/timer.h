/*
 * ambhw/timer.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *
 * Copyright (C) 2006-2007, Ambarella, Inc.
 */

#ifndef __AMBHW__TIMER_H__
#define __AMBHW__TIMER_H__

#include <ambhw/chip.h>
#include <ambhw/busaddr.h>

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

/* None so far */

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

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
#define TIMER_CTR_OFFSET		0x30

#define TIMER1_STATUS_REG		TIMER_REG(0x00)
#define TIMER1_RELOAD_REG		TIMER_REG(0x04)
#define TIMER1_MATCH1_REG		TIMER_REG(0x08)
#define TIMER1_MATCH2_REG		TIMER_REG(0x0c)
#define TIMER2_STATUS_REG		TIMER_REG(0x10)
#define TIMER2_RELOAD_REG		TIMER_REG(0x14)
#define TIMER2_MATCH1_REG		TIMER_REG(0x18)
#define TIMER2_MATCH2_REG		TIMER_REG(0x1c)
#define TIMER3_STATUS_REG		TIMER_REG(0x20)
#define TIMER3_RELOAD_REG		TIMER_REG(0x24)
#define TIMER3_MATCH1_REG		TIMER_REG(0x28)
#define TIMER3_MATCH2_REG		TIMER_REG(0x2c)
#define TIMER_CTR_REG			TIMER_REG(0x30)

/* Bit field definition of timer control register */
#define TIMER_CTR_OF3			0x00000400
#define TIMER_CTR_CSL3			0x00000200
#define TIMER_CTR_EN3			0x00000100
#define TIMER_CTR_OF2			0x00000040
#define TIMER_CTR_CSL2			0x00000020
#define TIMER_CTR_EN2			0x00000010
#define TIMER_CTR_OF1			0x00000004
#define TIMER_CTR_CSL1			0x00000002
#define TIMER_CTR_EN1			0x00000001

#endif
