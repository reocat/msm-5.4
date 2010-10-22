/*
 * ambhw/vic.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *
 * Copyright (C) 2006-2008, Ambarella, Inc.
 */

#ifndef __AMBHW__VIC_H__
#define __AMBHW__VIC_H__

#include <ambhw/chip.h>
#include <ambhw/busaddr.h>

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A3) || (CHIP_REV == A5) || (CHIP_REV == A6) || \
    (CHIP_REV ==A5S) || (CHIP_REV == A7M) || (CHIP_REV == A7)
#define VIC_INSTANCES	2
#elif (CHIP_REV == I1)
#define VIC_INSTANCES	3
#else
#define VIC_INSTANCES	1
#endif

/*
 * VIC trigger types (SW definition).
 */
#define VIRQ_RISING_EDGE	0
#define VIRQ_FALLING_EDGE	1
#define VIRQ_BOTH_EDGES		2
#define VIRQ_LEVEL_LOW		3
#define VIRQ_LEVEL_HIGH		4

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define VIC_IRQ_STA_OFFSET		0x00
#define VIC_FIQ_STA_OFFSET		0x04
#define VIC_RAW_STA_OFFSET		0x08
#define VIC_INT_SEL_OFFSET		0x0c
#define VIC_INTEN_OFFSET		0x10
#define VIC_INTEN_CLR_OFFSET		0x14
#define VIC_SOFTEN_OFFSET		0x18
#define VIC_SOFTEN_CLR_OFFSET		0x1c
#define VIC_PROTEN_OFFSET		0x20
#define VIC_SENSE_OFFSET		0x24
#define VIC_BOTHEDGE_OFFSET		0x28
#define VIC_EVENT_OFFSET		0x2c
#define VIC_EDGE_CLR_OFFSET		0x38

#define VIC_IRQ_STA_REG			VIC_REG(0x00)
#define VIC_FIQ_STA_REG			VIC_REG(0x04)
#define VIC_RAW_STA_REG			VIC_REG(0x08)
#define VIC_INT_SEL_REG			VIC_REG(0x0c)
#define VIC_INTEN_REG			VIC_REG(0x10)
#define VIC_INTEN_CLR_REG		VIC_REG(0x14)
#define VIC_SOFTEN_REG			VIC_REG(0x18)
#define VIC_SOFTEN_CLR_REG		VIC_REG(0x1c)
#define VIC_PROTEN_REG			VIC_REG(0x20)
#define VIC_SENSE_REG			VIC_REG(0x24)
#define VIC_BOTHEDGE_REG		VIC_REG(0x28)
#define VIC_EVENT_REG			VIC_REG(0x2c)
#define VIC_EDGE_CLR_REG		VIC_REG(0x38)

#if (VIC_INSTANCES >= 2)

#define VIC2_IRQ_STA_REG		VIC2_REG(0x00)
#define VIC2_FIQ_STA_REG		VIC2_REG(0x04)
#define VIC2_RAW_STA_REG		VIC2_REG(0x08)
#define VIC2_INT_SEL_REG		VIC2_REG(0x0c)
#define VIC2_INTEN_REG			VIC2_REG(0x10)
#define VIC2_INTEN_CLR_REG		VIC2_REG(0x14)
#define VIC2_SOFTEN_REG			VIC2_REG(0x18)
#define VIC2_SOFTEN_CLR_REG		VIC2_REG(0x1c)
#define VIC2_PROTEN_REG			VIC2_REG(0x20)
#define VIC2_SENSE_REG			VIC2_REG(0x24)
#define VIC2_BOTHEDGE_REG		VIC2_REG(0x28)
#define VIC2_EVENT_REG			VIC2_REG(0x2c)
#define VIC2_EDGE_CLR_REG		VIC2_REG(0x38)

#endif

#if (VIC_INSTANCES >= 3)

#define VIC3_IRQ_STA_REG		VIC3_REG(0x00)
#define VIC3_FIQ_STA_REG		VIC3_REG(0x04)
#define VIC3_RAW_STA_REG		VIC3_REG(0x08)
#define VIC3_INT_SEL_REG		VIC3_REG(0x0c)
#define VIC3_INTEN_REG			VIC3_REG(0x10)
#define VIC3_INTEN_CLR_REG		VIC3_REG(0x14)
#define VIC3_SOFTEN_REG			VIC3_REG(0x18)
#define VIC3_SOFTEN_CLR_REG		VIC3_REG(0x1c)
#define VIC3_PROTEN_REG			VIC3_REG(0x20)
#define VIC3_SENSE_REG			VIC3_REG(0x24)
#define VIC3_BOTHEDGE_REG		VIC3_REG(0x28)
#define VIC3_EVENT_REG			VIC3_REG(0x2c)
#define VIC3_EDGE_CLR_REG		VIC3_REG(0x38)

#endif

#endif
