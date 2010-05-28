/*
 * ambhw/idc.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *
 * Copyright (C) 2006-2008, Ambarella, Inc.
 */

#ifndef __AMBHW__IDC_H__
#define __AMBHW__IDC_H__

#include <ambhw/chip.h>
#include <ambhw/busaddr.h>

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A1) || (CHIP_REV == A2) || 		\
    (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) || \
    (CHIP_REV == A5L)  
#define IDC_INSTANCES		1
#else
#define IDC_INSTANCES		2
#endif

#if (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) || (CHIP_REV == A5L)
#define IDC_SUPPORT_PIN_MUXING_FOR_HDMI         1
#else
#define IDC_SUPPORT_PIN_MUXING_FOR_HDMI         0
#endif

#if (CHIP_REV == A5S) || (CHIP_REV == A7)
#define IDC_SUPPORT_INTERNAL_MUX	1
#else
#define IDC_SUPPORT_INTERNAL_MUX	0
#endif

#if (CHIP_REV == A5S) || (CHIP_REV == A2S) || (CHIP_REV == A2M)
#define IDC_PORTS_USING_INTERNAL_MUX 	1
#elif (CHIP_REV == A5L)
#define IDC_PORTS_USING_INTERNAL_MUX	2
#else
#define IDC_PORTS_USING_INTERNAL_MUX 	0
#endif

#if (CHIP_REV == A5L)
#define IDCS_INSTANCES			1
#else
#define IDCS_INSTANCES			0
#endif

#if (CHIP_REV == A5S) || (CHIP_REV == A7) || (CHIP_REV == A5L)
#define IDC_INTERNAL_DELAY_CLK		2
#else
#define IDC_INTERNAL_DELAY_CLK		0
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

#if	(IDCS_INSTANCES >= 1)
#define IDCS_ENR_OFFSET			0x00
#define IDCS_CTRL_OFFSET		0x04
#define IDCS_DATA_OFFSET		0x08
#define IDCS_STS_OFFSET			0x0c
#define IDCS_FIFO_CNT_OFFSET		0x10
#define IDCS_RX_CNT_OFFSET		0x14
#define IDCS_TX_CNT_OFFSET		0x18
#define IDCS_HOLD_TIME_OFFSET		0x1c
#define IDCS_SLAVE_ADDR_OFFSET		0x20
#define	IDCS_SCL_TIMER_OFFSET		0x24
#define IDCS_TIMEOUT_STS_OFFSET		0x28
#endif

#define IDC_ENR_REG			IDC_REG(0x00)
#define IDC_CTRL_REG			IDC_REG(0x04)
#define IDC_DATA_REG			IDC_REG(0x08)
#define IDC_STS_REG			IDC_REG(0x0c)
#define IDC_PSLL_REG			IDC_REG(0x10)
#define IDC_PSLH_REG			IDC_REG(0x14)
#define IDC_FMCTRL_REG			IDC_REG(0x18)
#define IDC_FMDATA_REG			IDC_REG(0x1c)

#if 	(IDC_INSTANCES >= 2)
#define IDC2_ENR_REG			IDC2_REG(0x00)
#define IDC2_CTRL_REG			IDC2_REG(0x04)
#define IDC2_DATA_REG			IDC2_REG(0x08)
#define IDC2_STS_REG			IDC2_REG(0x0c)
#define IDC2_PSLL_REG			IDC2_REG(0x10)
#define IDC2_PSLH_REG			IDC2_REG(0x14)
#define IDC2_FMCTRL_REG			IDC2_REG(0x18)
#define IDC2_FMDATA_REG			IDC2_REG(0x1c)
#endif

#if	(IDCS_INSTANCES >= 1)
#define IDCS_ENR_REG			IDCS_REG(0x00)
#define IDCS_CTRL_REG			IDCS_REG(0x04)
#define IDCS_DATA_REG			IDCS_REG(0x08)
#define IDCS_STS_REG			IDCS_REG(0x0c)
#define IDCS_FIFO_CNT_REG		IDCS_REG(0x10)
#define IDCS_RX_CNT_REG			IDCS_REG(0x14)
#define IDCS_TX_CNT_REG			IDCS_REG(0x18)
#define IDCS_HOLD_TIME_REG		IDCS_REG(0x1c)
#define IDCS_SLAVE_ADDR_REG		IDCS_REG(0x20)
#define	IDCS_SCL_TIMER_REG		IDCS_REG(0x24)
#define IDCS_TIMEOUT_STS_REG		IDCS_REG(0x28)
#endif

#define IDC_ENR_REG_ENABLE		(0x01)
#define IDC_ENR_REG_DISABLE		(0x00)

#define IDC_CTRL_STOP			(0x08)
#define IDC_CTRL_START			(0x04)
#define IDC_CTRL_IF			(0x02)
#define IDC_CTRL_ACK			(0x01)
#define IDC_CTRL_CLS			(0x00)

#define IDC_FIFO_BUF_SIZE		(63)

#define IDC_FMCTRL_STOP			(0x08)
#define IDC_FMCTRL_START		(0x04)
#define IDC_FMCTRL_IF			(0x02)

#define I2C_M_PIN_MUXING		(0x8000)

#endif
