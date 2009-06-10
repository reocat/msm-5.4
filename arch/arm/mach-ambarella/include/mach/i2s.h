/*
 * arch/arm/mach-ambarella/include/mach/i2s.h
 *
 * History:
 *	2007/01/27 - [Charles Chiou] created file
 *	2008/02/19 - [Allen Wang] changed to use capabilities and chip ID 
 *	2008/05/13 - [Allen Wang] added capabilities of A2S and A2M silicons	 	 
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

#ifndef __ASM_ARCH_I2S_H
#define __ASM_ARCH_I2S_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if 	(CHIP_REV == A1) || (CHIP_REV == A2) ||		\
	(CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define	I2S_SUPPORT_CATE_SHIFT		0
#define	I2S_MAX_CHANNELS		2
#else
#define	I2S_SUPPORT_CATE_SHIFT		1
#define	I2S_MAX_CHANNELS		6
#endif


/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define I2S_MODE_OFFSET			0x00
#define I2S_RX_CTRL_OFFSET		0x04
#define I2S_TX_CTRL_OFFSET		0x08
#define I2S_WLEN_OFFSET			0x0c
#define I2S_WPOS_OFFSET			0x10
#define I2S_SLOT_OFFSET			0x14
#define I2S_TX_FIFO_LTH_OFFSET		0x18
#define I2S_RX_FIFO_GTH_OFFSET		0x1c
#define I2S_CLOCK_OFFSET		0x20
#define I2S_INIT_OFFSET			0x24
#define I2S_TX_STATUS_OFFSET		0x28
#define I2S_TX_LEFT_DATA_OFFSET		0x2c
#define I2S_TX_RIGHT_DATA_OFFSET	0x30
#define I2S_RX_STATUS_OFFSET		0x34
#define I2S_RX_DATA_OFFSET		0x38
#define I2S_TX_FIFO_CNTR_OFFSET		0x3c
#define I2S_RX_FIFO_CNTR_OFFSET		0x40
#define I2S_TX_INT_ENABLE_OFFSET	0x44
#define I2S_RX_INT_ENABLE_OFFSET	0x48
#define I2S_RX_ECHO_OFFSET		0x4c
#define I2S_24BITMUX_MODE_OFFSET	0x50
#define I2S_RX_DATA_DMA_OFFSET		0x80
#define I2S_TX_LEFT_DATA_DMA_OFFSET	0xc0

#define I2S_MODE_REG			I2S_REG(0x00)
#define I2S_RX_CTRL_REG			I2S_REG(0x04)
#define I2S_TX_CTRL_REG			I2S_REG(0x08)
#define I2S_WLEN_REG			I2S_REG(0x0c)
#define I2S_WPOS_REG			I2S_REG(0x10)
#define I2S_SLOT_REG			I2S_REG(0x14)
#define I2S_TX_FIFO_LTH_REG		I2S_REG(0x18)
#define I2S_RX_FIFO_GTH_REG		I2S_REG(0x1c)
#define I2S_CLOCK_REG			I2S_REG(0x20)
#define I2S_INIT_REG			I2S_REG(0x24)
#define I2S_TX_STATUS_REG		I2S_REG(0x28)
#define I2S_TX_LEFT_DATA_REG		I2S_REG(0x2c)
#define I2S_TX_RIGHT_DATA_REG		I2S_REG(0x30)
#define I2S_RX_STATUS_REG		I2S_REG(0x34)
#define I2S_RX_DATA_REG			I2S_REG(0x38)
#define I2S_TX_FIFO_CNTR_REG		I2S_REG(0x3c)
#define I2S_RX_FIFO_CNTR_REG		I2S_REG(0x40)
#define I2S_TX_INT_ENABLE_REG		I2S_REG(0x44)
#define I2S_RX_INT_ENABLE_REG		I2S_REG(0x48)
#define I2S_RX_ECHO_REG			I2S_REG(0x4c)
#define I2S_24BITMUX_MODE_REG		I2S_REG(0x50)
#if 	(I2S_SUPPORT_CATE_SHIFT == 1)
#define I2S_GATEOFF_REG			I2S_REG(0x54)
#endif
#if 	(I2S_MAX_CHANNELS == 6)
#define I2S_CHANNEL_SELECT_REG		I2S_REG(0x58)
#endif
#define I2S_RX_DATA_DMA_REG		I2S_REG(0x80)
#define I2S_TX_LEFT_DATA_DMA_REG	I2S_REG(0xc0)

#define I2S_TX_ENABLE_BIT		(1 << 2)
#define I2S_RX_ENABLE_BIT		(1 << 1)
#define I2S_FIFO_RESET_BIT		(1 << 0)

#define I2S_RX_LOOPBACK_BIT		(1 << 3)
#define I2S_RX_ORDER_BIT		(1 << 2)
#define I2S_RX_WS_MST_BIT		(1 << 1)
#define I2S_RX_WS_INV_BIT		(1 << 0)

#define I2S_TX_LOOPBACK_BIT		(1 << 7)
#define I2S_TX_ORDER_BIT		(1 << 6)
#define I2S_TX_WS_MST_BIT		(1 << 5)
#define I2S_TX_WS_INV_BIT		(1 << 4)
#define I2S_TX_TX_MONO_BITS		(3 << 0)

#if 	(I2S_SUPPORT_CATE_SHIFT == 1)
#define I2S_RX_SHIFT_ENB		(1 << 2)
#define I2S_TX_SHIFT_ENB		(1 << 1)
#endif

#if 	(I2S_MAX_CHANNELS == 6)
#define I2S_2CHANNELS_ENB		0x00
#define I2S_4CHANNELS_ENB		0x01
#define I2S_6CHANNELS_ENB		0x02
#endif

#define I2S_FIFO_THRESHOLD_INTRPT	0x08
#define I2S_FIFO_FULL_INTRPT		0x02
#define I2S_FIFO_EMPTY_INTRPT		0x01

#endif

