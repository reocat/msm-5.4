/*
 * arch/arm/mach-ambarella/include/mach/uart.h
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

#ifndef __ASM_ARCH_UART_H
#define __ASM_ARCH_UART_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

/* None so far */

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/
#if (CHIP_REV == A5M)
#define UART_PORT_MAX			2
#else
#define UART_PORT_MAX			1
#endif

#define UART_FIFO_SIZE			(16)
#define UART_REG_MEM_SIZE		(0x20)

#define UART0_BASE			UART0_REG(0)
#define UART1_BASE			UART1_REG(0)

#define UART0_RB_OFFSET			0x00
#define UART0_TH_OFFSET			0x00
#define UART0_DLL_OFFSET		0x00
#define UART0_IE_OFFSET			0x04
#define UART0_DLH_OFFSET		0x04
#define UART0_II_OFFSET			0x08
#define UART0_FC_OFFSET			0x08
#define UART0_LC_OFFSET			0x0c
#define UART0_MC_OFFSET			0x10
#define UART0_LS_OFFSET			0x14
#define UART0_MS_OFFSET			0x18
#define UART0_SC_OFFSET			0x1c

#define UART0_RB_REG			UART0_REG(0x00)
#define UART0_TH_REG			UART0_REG(0x00)
#define UART0_DLL_REG			UART0_REG(0x00)
#define UART0_IE_REG			UART0_REG(0x04)
#define UART0_DLH_REG			UART0_REG(0x04)
#define UART0_II_REG			UART0_REG(0x08)
#define UART0_FC_REG			UART0_REG(0x08)
#define UART0_LC_REG			UART0_REG(0x0c)
#define UART0_MC_REG			UART0_REG(0x10)
#define UART0_LS_REG			UART0_REG(0x14)
#define UART0_MS_REG			UART0_REG(0x18)
#define UART0_SC_REG			UART0_REG(0x1c)

#define UARTX_RB_REG(x)			(UART_BASE + ((x) << 16))
#define UARTX_TH_REG(x)			(UART_BASE + ((x) << 16))
#define UARTX_DLL_REG(x)		(UART_BASE + ((x) << 16))
#define UARTX_IE_REG(x)			(UART_BASE + ((x) << 16) + 0x04)
#define UARTX_DLH_REG(x)		(UART_BASE + ((x) << 16) + 0x04)
#define UARTX_II_REG(x)			(UART_BASE + ((x) << 16) + 0x08)
#define UARTX_FC_REG(x)			(UART_BASE + ((x) << 16) + 0x08)
#define UARTX_LC_REG(x)			(UART_BASE + ((x) << 16) + 0x0c)
#define UARTX_MC_REG(x)			(UART_BASE + ((x) << 16) + 0x10)
#define UARTX_LS_REG(x)			(UART_BASE + ((x) << 16) + 0x14)
#define UARTX_MS_REG(x)			(UART_BASE + ((x) << 16) + 0x18)
#define UARTX_SC_REG(x)			(UART_BASE + ((x) << 16) + 0x1c)

/* UART[x]_IE_REG */
#define UART_IE_PTIME			0x80
#define UART_IE_EDSSI			0x08
#define UART_IE_ELSI			0x04
#define UART_IE_ETBEI			0x02
#define UART_IE_ERBFI			0x01

/* UART[x]_II_REG */
#define UART_II_MODEM_STATUS_CHANGED	0x00
#define UART_II_NO_INT_PENDING		0x01
#define UART_II_THR_EMPTY		0x02
#define UART_II_RCV_DATA_AVAIL		0x04
#define UART_II_RCV_STATUS		0x06
#define UART_II_CHAR_TIMEOUT		0x0c

/* UART[x]_FC_REG */
#define UART_FC_RX_ONECHAR		0x00
#define UART_FC_RX_QUARTER_FULL		0x40
#define UART_FC_RX_HALF_FULL		0x80
#define UART_FC_RX_2_TO_FULL		0xc0
#define UART_FC_TX_EMPTY		0x00
#define UART_FC_TX_2_IN_FIFO		0x10
#define UART_FC_TX_QUATER_IN_FIFO	0x20
#define UART_FC_TX_HALF_IN_FIFO		0x30
#define UART_FC_XMITR			0x04
#define UART_FC_RCVRR			0x02
#define UART_FC_FIFOE			0x01

/* UART[x]_LC_REG */
#define UART_LC_DLAB			0x80
#define UART_LC_BRK			0x40
#define UART_LC_EPS			0x10
#define UART_LC_EVEN_PARITY		0x08
#define UART_LC_ODD_PARITY		0x00
#define UART_LC_STOP_2BIT		0x04
#define UART_LC_STOP_1BIT		0x00
#define UART_LC_CLS_8_BITS		0x03
#define UART_LC_CLS_7_BITS		0x02
#define UART_LC_CLS_6_BITS		0x01
#define UART_LC_CLS_5_BITS		0x00
/*	quick defs */
#	define	UART_LC_8N1		0x03
#	define	UART_LC_7E1		0x0a

/* UART[x]_LS_REG */
#define UART_LS_FERR			0x80
#define UART_LS_TEMT			0x40
#define UART_LS_THRE			0x20
#define UART_LS_BI			0x10
#define UART_LS_FE			0x08
#define UART_LS_PE			0x04
#define UART_LS_OE			0x02
#define UART_LS_DR			0x01

/* Other defs for UART */
#define RECV_BUF_SIZ			1500
#define SEND_BUF_SIZ			1500

#endif

