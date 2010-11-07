/*
 * ambhw/wdog.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *
 * Copyright (C) 2006-2007, Ambarella, Inc.
 */

#ifndef __AMBHW__WDOG_H__
#define __AMBHW__WDOG_H__

#include <ambhw/chip.h>
#include <ambhw/busaddr.h>

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
#define WDOG_RST_WD_OFFSET		0x18

#define WDOG_STATUS_REG			WDOG_REG(WDOG_STATUS_OFFSET)
#define WDOG_RELOAD_REG			WDOG_REG(WDOG_RELOAD_OFFSET)
#define WDOG_RESTART_REG		WDOG_REG(WDOG_RESTART_OFFSET)
#define WDOG_CONTROL_REG		WDOG_REG(WDOG_CONTROL_OFFSET)
#define WDOG_TIMEOUT_REG		WDOG_REG(WDOG_TIMEOUT_OFFSET)
#define WDOG_CLR_TMO_REG		WDOG_REG(WDOG_CLR_TMO_OFFSET)
#define WDOG_RST_WD_REG			WDOG_REG(WDOG_RST_WD_OFFSET)

/* Bit field definition of watch dog timer control register */
#define WDOG_CTR_INT_EN			0x00000004
#define WDOG_CTR_RST_EN			0x00000002
#define WDOG_CTR_EN			0x00000001

#endif
