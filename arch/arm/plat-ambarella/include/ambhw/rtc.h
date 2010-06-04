/*
 * ambhw/rtc.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *
 * Copyright (C) 2006-2007, Ambarella, Inc.
 */

#ifndef __AMBHW__RTC_H__
#define __AMBHW__RTC_H__

#include <ambhw/chip.h>
#include <ambhw/busaddr.h>

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A1)
#define RTC_SUPPORT_ONCHIP_INSTANCES		0 
#else
#define RTC_SUPPORT_ONCHIP_INSTANCES		1
#endif

#if (CHIP_REV == A5S) || (CHIP_REV == A7) || (CHIP_REV == A5L)
#define RTC_SUPPORT_EXTRA_WAKEUP_PINS		1
#define RTC_SUPPORT_30BITS_PASSED_SECONDS	1
#else
#define RTC_SUPPORT_EXTRA_WAKEUP_PINS		0
#define RTC_SUPPORT_30BITS_PASSED_SECONDS	0
#endif

#if (CHIP_REV == A7) 
#define RTC_SUPPORT_GPIO_PAD_PULL_CTRL		1
#else
#define RTC_SUPPORT_GPIO_PAD_PULL_CTRL		0
#endif

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/
#if (RTC_SUPPORT_ONCHIP_INSTANCES == 1)

#define RTC_POS0_OFFSET			0x20
#define RTC_POS1_OFFSET			0x24
#define RTC_POS2_OFFSET			0x28
#define RTC_PWC_ALAT_OFFSET		0x2c
#define RTC_PWC_CURT_OFFSET		0x30
#define RTC_CURT_OFFSET			0x34
#define RTC_ALAT_OFFSET			0x38
#define RTC_STATUS_OFFSET		0x3c
#define RTC_RESET_OFFSET		0x40

#if (RTC_SUPPORT_EXTRA_WAKEUP_PINS == 1)
#define RTC_WO_OFFSET			0x7c
#endif

#if (RTC_SUPPORT_GPIO_PAD_PULL_CTRL == 1)
#define RTC_GPIO_PULL_EN_0_OFFSET	0x80
#define RTC_GPIO_PULL_EN_1_OFFSET	0x84
#define RTC_GPIO_PULL_EN_2_OFFSET	0x88
#define RTC_GPIO_PULL_EN_3_OFFSET	0x8c
#define RTC_GPIO_PULL_EN_4_OFFSET	0x90
#define RTC_GPIO_PULL_DIR_0_OFFSET	0x94
#define RTC_GPIO_PULL_DIR_1_OFFSET	0x98
#define RTC_GPIO_PULL_DIR_2_OFFSET	0x9c
#define RTC_GPIO_PULL_DIR_3_OFFSET	0xa0
#define RTC_GPIO_PULL_DIR_4_OFFSET	0xa4
#define RTC_RTC_CTL_OFFSET		0xfc
#endif


#define RTC_POS0_REG			RTC_REG(0x20)
#define RTC_POS1_REG			RTC_REG(0x24)
#define RTC_POS2_REG			RTC_REG(0x28)
#define RTC_PWC_ALAT_REG		RTC_REG(0x2c)
#define RTC_PWC_CURT_REG		RTC_REG(0x30)
#define RTC_CURT_REG			RTC_REG(0x34)
#define RTC_ALAT_REG			RTC_REG(0x38)
#define RTC_STATUS_REG			RTC_REG(0x3c)
#define RTC_RESET_REG			RTC_REG(0x40)

#if (RTC_SUPPORT_EXTRA_WAKEUP_PINS == 1)
#define RTC_WO_REG			RTC_REG(0x7c)
#endif
 
#if (RTC_SUPPORT_GPIO_PAD_PULL_CTRL == 1) 
#define RTC_GPIO_PULL_EN_0_REG		RTC_REG(0x80)
#define RTC_GPIO_PULL_EN_1_REG		RTC_REG(0x84)
#define RTC_GPIO_PULL_EN_2_REG		RTC_REG(0x88)
#define RTC_GPIO_PULL_EN_3_REG		RTC_REG(0x8c)
#define RTC_GPIO_PULL_EN_4_REG		RTC_REG(0x90)
#define RTC_GPIO_PULL_DIR_0_REG		RTC_REG(0x94)
#define RTC_GPIO_PULL_DIR_1_REG		RTC_REG(0x98)
#define RTC_GPIO_PULL_DIR_2_REG		RTC_REG(0x9c)
#define RTC_GPIO_PULL_DIR_3_REG		RTC_REG(0xa0)
#define RTC_GPIO_PULL_DIR_4_REG		RTC_REG(0xa4)
#define RTC_RTC_CTL_REG			RTC_REG(0xfc)
#endif

/* RTC_STATUS_REG */
#define RTC_STATUS_WKUP			0x8
#define RTC_STATUS_ALA_WK		0x4
#define RTC_STATUS_PC_RST		0x2
#define RTC_STATUS_RTC_CLK		0x1

#endif 

#endif

