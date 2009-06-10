/*
 * arch/arm/mach-ambarella/include/mach/usbdc.h
 *
 * History:
 *	2007/01/27 - [Charles Chiou] created file
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

#ifndef __ASM_ARCH_USBDC_H
#define __ASM_ARCH_USBDC_H

//-------------------------------------
// USB RxFIFO and TxFIFO depth (single or multiple)
//-------------------------------------
#define USB_RXFIFO_DEPTH_CTRLOUT	(256 << 16)	// shared
#define USB_RXFIFO_DEPTH_BULKOUT	(256 << 16)	// shared
#define USB_RXFIFO_DEPTH_INTROUT	(256 << 16)	// shared
#define USB_TXFIFO_DEPTH_CTRLIN		(64 / 4)	// 16 32-bit
#define USB_TXFIFO_DEPTH_BULKIN		(1024 / 4)	// 256 32-bit
#define USB_TXFIFO_DEPTH_INTRIN		(64 / 4)	// 16 32-bit
#define USB_TXFIFO_DEPTH_ISOIN		((512 * 2) / 4)	// 128 32-bit

#define USB_TXFIFO_DEPTH		(64 / 4 + 4 * 512 / 4)	// 528 32-bit
#define USB_RXFIFO_DEPTH		(256)			// 256 32-bit

//-------------------------------------
// USB memory map
//-------------------------------------
#define USB_EP_IN_BASE			USBDC_BASE	
#define USB_EP_OUT_BASE			(USBDC_BASE + 0x0200)	
#define USB_DEV_BASE			(USBDC_BASE + 0x0400)	
#define USB_UDC_BASE			(USBDC_BASE + 0x0504)  // Todo
#define USB_RXFIFO_BASE			(USBDC_BASE + 0x0800)
#define USB_TXFIFO_BASE			(USB_RXFIFO_BASE + USB_RXFIFO_DEPTH)

/************************************************************/
/* USB Device Controller IN/OUT Endpoint-Specific Registers */
/************************************************************/

#define UDC_EP_IN_BASE			USBDC_BASE

#define UDC_EP_IN_CTRL_REG(n)		(UDC_EP_IN_BASE + 0x0000 + 0x0020*(n))
#define UDC_EP_IN_STS_REG(n)		(UDC_EP_IN_BASE + 0x0004 + 0x0020*(n))
#define UDC_EP_IN_BUF_SZ_REG(n)		(UDC_EP_IN_BASE + 0x0008 + 0x0020*(n))
#define UDC_EP_IN_MAX_PKT_SZ_REG(n)	(UDC_EP_IN_BASE + 0x000c + 0x0020*(n))
#define UDC_EP_IN_DAT_DESC_PTR_REG(n)	(UDC_EP_IN_BASE + 0x0014 + 0x0020*(n))
#define UDC_EP_IN_WR_CFM_REG(n)		(UDC_EP_IN_BASE + 0x001c + 0x0020*(n))

#define UDC_EP_OUT_BASE			(USBDC_BASE + 0x0200)

#define UDC_EP_OUT_CTRL_REG(n)		(UDC_EP_OUT_BASE + 0x0000 + 0x0020*(n))
#define UDC_EP_OUT_STS_REG(n)		(UDC_EP_OUT_BASE + 0x0004 + 0x0020*(n))
#define UDC_EP_OUT_PKT_FRM_NUM_REG(n)	(UDC_EP_OUT_BASE + 0x0008 + 0x0020*(n))
#define UDC_EP_OUT_MAX_PKT_SZ_REG(n)	(UDC_EP_OUT_BASE + 0x000c + 0x0020*(n))
#define UDC_EP_OUT_SETUP_BUF_PTR_REG(n)	(UDC_EP_OUT_BASE + 0x0010 + 0x0020*(n))
#define UDC_EP_OUT_DAT_DESC_PTR_REG(n)	(UDC_EP_OUT_BASE + 0x0014 + 0x0020*(n))
#define UDC_EP_OUT_RD_CFM_ZO_REG(n)	(UDC_EP_OUT_BASE + 0x001c + 0x0020*(n))

/* UDC_EP_IN_CTRL_REG(n) and UDC_EP_OUT_CTRL_REG(n) */

#define UDC_EP_CTRL_RRDY		0x00000200
#define UDC_EP_CTRL_CNAK		0x00000100
#define UDC_EP_CTRL_SNAK		0x00000080
#define UDC_EP_CTRL_NAK			0x00000040
#define UDC_EP_CTRL_ET			0x00000030
	// Endpoint Type begin
#define UDC_EP_TYPE_CTRL		0x00000000		// 00 (RW)
#define UDC_EP_TYPE_ISO			0x00000010		// 00 (RW)
#define UDC_EP_TYPE_BULK		0x00000020		// 00 (RW)
#define UDC_EP_TYPE_INTR		0x00000030		// 00 (RW)
	// Endpoint Type end
#define UDC_EP_CTRL_POLL		0x00000008
#define UDC_EP_CTRL_SNOOP		0x00000004
#define UDC_EP_CTRL_FLUSH		0x00000002
#define UDC_EP_CTRL_STALL		0x00000001


/* UDC_EP_IN_STS_REG(n) and UDC_EP_OUT_STS_REG(n) */
#define UDC_EP_STS_RX_PKT_SZ		0x007ff800
#define UDC_EP_STS_TX_DMA_CMPL		0x00000400
#define UDC_EP_STS_HOST_ERR		0x00000200
#define UDC_EP_STS_BUF_NOT_AVAIL	0x00000080
#define UDC_EP_STS_IN_TOKEN		0x00000040
#define UDC_EP_STS_OUT_PKT_MSK		0x00000030
	// UDC_EP_STS_OUT_TYPE begin
#define UDC_EP_NONE_PKT			0x00000000
#define UDC_EP_DATA_PKT			0x00000010
#define UDC_EP_SETUP_PKT		0x00000020
	// UDC_EP_STS_OUT_TYPE end

/* UDC_EP_IN_BUF_SZ_REG(n) and UDC_EP_OUT_PKT_FRM_NUM_REG(n) */
#define UDC_EP_IN_BUF_SZ		0x0000ffff
#define UDC_EP_OUT_PKT_FRM_NUM		0x0000ffff

/* UDC_EP_IN_MAX_PKT_SZ_REG(n) and UDC_EP_OUT_MAX_PKT_SZ_REG(n) */
#define UDC_EP_MAX_PKT_BUF_SZ		0xffff0000
#define UDC_EP_MAX_PKT_FRM_NUM		0x0000ffff

/* UDC_EP_OUT_SETUP_BUF_PTR_REG(n) */
/* UDC_EP_IN_DAT_DESC_PTR_REG(n) and UDC_EP_OUT_DAT_DESC_PTR_REG(n) */
/* UDC_EP_IN_WR_CFM_REG(n) and UDC_EP_OUT_RD_CFM_ZO_REG(n) */


/* ---------------------------------------------------------------------- */

/******************************************/
/* USB Device Controller Global Registers */
/******************************************/
#if (CHIP_REV == 1)
#define UDC_CFG				0x400
#define UDC_CTRL			0x404
#define UDC_STS				0x408
#define UDC_INTR			0x40c
#define UDC_INTR_MSK			0x410
#define UDC_EP_INTR			0x414
#define UDC_EP_INTR_MSK			0x418
#define UDC_TEST_MODE			0x41c
#endif

#define UDC_REG(x)			(USBDC_BASE + (x))
#define UDC_CFG_REG			UDC_REG(0x400)
#define UDC_CTRL_REG			UDC_REG(0x404)
#define UDC_STS_REG			UDC_REG(0x408)
#define UDC_INTR_REG			UDC_REG(0x40c)
#define UDC_INTR_MSK_REG		UDC_REG(0x410)
#define UDC_EP_INTR_REG			UDC_REG(0x414)
#define UDC_EP_INTR_MSK_REG		UDC_REG(0x418)
#define UDC_TEST_MODE_REG		UDC_REG(0x41c)

/* UDC specific registers, 0x0500 is reserved for control endpoint (RO)*/
#define USB_UDC_REG(n)			(USB_UDC_BASE + 0x0004 * (n))

/* for UDC_CFG_REG */
#define UDC_CFG_DDR			0x00080000
#define UDC_CFG_SET_DESC		0x00040000
	// set descripter ack begin
#define UDC_CFG_SET_DESC_STALL		0x00000000
#define UDC_CFG_SET_DESC_ACK		0x00040000
	// set descripter ack end
#define UDC_CFG_CSR_PRG			0x00020000
#define UDC_CFG_HALT			0x00010000
	// halt configure begin
#define UDC_CFG_HALT_ACK		0x00000000
#define UDC_CFG_HALT_STALL		0x00010000
	// halt configure end
#define UDC_CFG_HS_TIMEOUT		0x0000e000
#define UDC_CFG_PS_TIMEOUT		0x00001c00
#define UDC_CFG_PHY_ERROR		0x00000200
#define UDC_CFG_STS_i			0x00000100
#define UDC_CFG_STS			0x00000080
#define UDC_CFG_DIR_BI			0x00000040
#define UDC_CFG_P1_8BIT			0x00000020
#define UDC_CFG_SYNC_FRAME		0x00000010
#define UDC_CFG_SELF_POWER		0x00000008
#define UDC_CFG_RWKP			0x00000004
#define UDC_CFG_SPD			0x00000003
	// expected speed begin
#define UDC_CFG_SPD_HI			0x00000000	// 00 (RW) 30/60MHz 
#define UDC_CFG_SPD_FU			0x00000001	// 00 (RW) 
#define UDC_CFG_SPD_LO			0x00000002	// 00 (RW) 30MHz
#define UDC_CFG_SPD_FU48		0x00000003	// 00 (RW) - PHY CLK = 48 MHz
	// expected speed end

/* for UDC_CTRL_REG */
#define UDC_CTRL_THLEN			0xff000000
#define UDC_CTRL_BURST_LEN		0x00ff0000
	// default burst length begin
#define UDC_CTRL_DEFAULT_BRT_LEN	0x00070000
	// default burst length end
#define UDC_CTRL_CSR_DONE		0x00002000
#define UDC_CTRL_DEVNAK			0x00001000
#define UDC_CTRL_SCALE			0x00000800
#define UDC_CTRL_SOFTDISC		0x00000400
#define UDC_CTRL_DMA_MODE		0x00000200
#define UDC_CTRL_BURST_EN		0x00000100
#define UDC_CTRL_THE			0x00000080
#define UDC_CTRL_BF			0x00000040
#define UDC_CTRL_BE			0x00000020
	// system endian begin
#define UDC_CTRL_LITTLE_ENDN		0x00000000		// 0 (RW)(D)
#define UDC_CTRL_BIG_ENDN		0x00000020		// 0 (RW)(D)
	// system endian end
#define UDC_CTRL_DU			0x00000010
	// descripter update begin
#define UDC_CTRL_DESC_UPD_PYL		0x00000000
#define UDC_CTRL_DESC_UPD_PKT		0x00000010
	// descripter update end
#define UDC_CTRL_TRN_DMA_EN		0x00000008
#define UDC_CTRL_RCV_DMA_EN		0x00000004
#define UDC_CTRL_RES			0x00000001

/* for UDC_STS_REG */
#define UDC_STS_TS			0xfffc0000
#define UDC_STS_PHY_ERROR		0x00010000
#define UDC_STS_RXFIFO_EMPTY		0x00008000
#define UDC_STS_ENUM_SPD		0x00006000
	// for UDC_STS_ENUM_SPD begin
#define UDC_STS_ENUM_SPD_HI		0x00000000		// 00 (RO)
#define UDC_STS_ENUM_SPD_FU		0x00002000		// 00 (RO)
#define UDC_STS_ENUM_SPD_LO		0x00004000		// 00 (RO)
#define UDC_STS_ENUM_SPD_FU48		0x00006000		// 00 (RO)
	// for UDC_STS_ENUM_SPD end
#define UDC_STS_SUSP			0x00001000
#define UDC_STS_ALT			0x00000f00
#define UDC_STS_INTF			0x000000f0
#define UDC_STS_CFG			0x0000000f

/* for UDC_INTR_REG */
#define UDC_INTR_ENUM_CMPL		0x00000040
#define UDC_INTR_SOF			0x00000020
#define UDC_INTR_SUSPEND		0x00000010
#define UDC_INTR_RESET			0x00000008
#define UDC_INTR_IDLE_3MS		0x00000004
#define UDC_INTR_SET_INTF		0x00000002
#define UDC_INTR_SET_CFG		0x00000001

/* for UDC_INTR_MSK_REG */
#define UDC_INTR_MSK_ENUM		0x00000040
#define UDC_INTR_MSK_SOF		0x00000020
#define UDC_INTR_MSK_US			0x00000010
#define UDC_INTR_MSK_UR			0x00000008
#define UDC_INTR_MSK_ES			0x00000004
#define UDC_INTR_MSK_SI			0x00000002
#define UDC_INTR_MSK_SC			0x00000001

/* for UDC_EP_INTR_REG */
#define UDC_EP_INTR_EP0_IN		0x00000001		// 0 (R/WC)
#define UDC_EP_INTR_EP1_IN		0x00000002		// 0 (R/WC)
#define UDC_EP_INTR_EP2_IN		0x00000004		// 0 (R/WC)
#define UDC_EP_INTR_EP3_IN		0x00000008		// 0 (R/WC)
#define UDC_EP_INTR_EP4_IN		0x00000010		// 0 (R/WC)

#define UDC_EP_INTR_EP0_OUT		0x00010000		// 0 (R/WC)
#define UDC_EP_INTR_EP1_OUT		0x00020000		// 0 (R/WC)
#define UDC_EP_INTR_EP2_OUT		0x00040000		// 0 (R/WC)
#define UDC_EP_INTR_EP3_OUT		0x00080000		// 0 (R/WC)
#define UDC_EP_INTR_EP4_OUT		0x00100000		// 0 (R/WC)


/* for UDC_EP_INTR_MSK_REG */
#define UDC_EP_INTR_MSK_EP0_IN		0x00000001		// 0 (R/WC)
#define UDC_EP_INTR_MSK_EP1_IN		0x00000002		// 0 (R/WC)
#define UDC_EP_INTR_MSK_EP2_IN		0x00000004		// 0 (R/WC)
#define UDC_EP_INTR_MSK_EP3_IN		0x00000008		// 0 (R/WC)
#define UDC_EP_INTR_MSK_EP4_IN		0x00000010		// 0 (R/WC)

#define UDC_EP_INTR_MSK_EP0_OUT		0x00010000		// 0 (R/WC)
#define UDC_EP_INTR_MSK_EP1_OUT		0x00020000		// 0 (R/WC)
#define UDC_EP_INTR_MSK_EP2_OUT		0x00040000		// 0 (R/WC)
#define UDC_EP_INTR_MSK_EP3_OUT		0x00080000		// 0 (R/WC)
#define UDC_EP_INTR_MSK_EP4_OUT		0x00100000		// 0 (R/WC)


#define UDC_EP_INTR_EP_OUT		(UDC_EP_INTR_EP0_OUT | \
					 UDC_EP_INTR_EP1_OUT | \
					 UDC_EP_INTR_EP2_OUT)

/*// | \
					 //UDC_EP_INTR_EP3_OUT | \
					 //UDC_EP_INTR_EP4_OUT )
					 */
#define UDC_EP_INTR_EP_IN		(UDC_EP_INTR_EP0_IN | \
					 UDC_EP_INTR_EP1_IN | \
					 UDC_EP_INTR_EP2_IN | \
					 UDC_EP_INTR_EP3_IN)
					 /*| \
					 //UDC_EP_INTR_EP4_IN ) */

/* for UDC_TEST_MODE_REG */
#define UDC_TEST_MODE_TSTMODE		0x00000001


/* From  SDK * * * By Caorr */


// for USB_UDC_REG 
#define USB_UDC_EP0_NUM			0x00000000
#define USB_UDC_EP1_NUM			0x00000001
#define USB_UDC_EP2_NUM			0x00000002
#define USB_UDC_EP3_NUM			0x00000003
#define USB_UDC_EP4_NUM			0x00000004
#define USB_UDC_EP5_NUM			0x00000005
#define USB_UDC_EP6_NUM			0x00000006
#define USB_UDC_EP7_NUM			0x00000007
#define USB_UDC_EP8_NUM			0x00000008
#define USB_UDC_EP9_NUM			0x00000009
#define USB_UDC_EP10_NUM		0x0000000a
#define USB_UDC_EP11_NUM		0x0000000b
#define USB_UDC_EP12_NUM		0x0000000c
#define USB_UDC_EP13_NUM		0x0000000d
#define USB_UDC_EP14_NUM		0x0000000e
#define USB_UDC_EP15_NUM		0x0000000f

#define USB_UDC_OUT			0x00000000
#define USB_UDC_IN			0x00000010

#define USB_UDC_CFG_NUM			0x00000080 //0x00000100 //
#define USB_UDC_INTF_NUM		0x00000000 //0x00001000 //
#define USB_UDC_ALT_SET			0x00000000 //0x00010000 //

#define USB_UDC_CTRL			0x00000000
#define USB_UDC_ISO			0x00000020
#define USB_UDC_BULK			0x00000040
#define USB_UDC_INTR			0x00000060

#define USB_EP_CTRL_MAX_PKT_SZ		64
#define USB_EP_BULK_MAX_PKT_SZ_HI	512
#define USB_EP_BULK_MAX_PKT_SZ_FU	64
#define USB_EP_INTR_MAX_PKT_SZ		64
#define USB_EP_ISO_MAX_PKT_SZ		512

#define USB_EP_CTRLIN_MAX_PKT_SZ	USB_EP_CTRL_MAX_PKT_SZ
#define USB_EP_CTRLOUT_MAX_PKT_SZ	USB_EP_CTRL_MAX_PKT_SZ
#define USB_EP_BULKIN_MAX_PKT_SZ_HI	USB_EP_BULK_MAX_PKT_SZ_HI
#define USB_EP_BULKOUT_MAX_PKT_SZ_HI	USB_EP_BULK_MAX_PKT_SZ_HI
#define USB_EP_BULKIN_MAX_PKT_SZ_FU	USB_EP_BULK_MAX_PKT_SZ_FU
#define USB_EP_BULKOUT_MAX_PKT_SZ_FU	USB_EP_BULK_MAX_PKT_SZ_FU
#define USB_EP_INTRIN_MAX_PKT_SZ	USB_EP_INTR_MAX_PKT_SZ
#define USB_EP_INTROUT_MAX_PKT_SZ	USB_EP_INTR_MAX_PKT_SZ
#define USB_EP_ISOIN_MAX_PKT_SZ		USB_EP_ISO_MAX_PKT_SZ
#define USB_EP_ISOOUT_MAX_PKT_SZ	USB_EP_ISO_MAX_PKT_SZ
#define USB_EP_ISOIN_TRANSACTIONS	1
#define USB_EP_ISOOUT_TRANSACTIONS	1

#define USB_UDC_MAX_PKT_SZ		(0x1fff << 19)

#define USB_UDC_CTRL_MAX_PKT_SZ		(USB_EP_CTRL_MAX_PKT_SZ << 19)
#define USB_UDC_BULK_MAX_PKT_SZ_HI	(USB_EP_BULK_MAX_PKT_SZ_HI << 19)
#define USB_UDC_BULK_MAX_PKT_SZ_FU	(USB_EP_BULK_MAX_PKT_SZ_FU << 19)
#define USB_UDC_INTR_MAX_PKT_SZ		(USB_EP_INTR_MAX_PKT_SZ << 19)
#define USB_UDC_ISO_MAX_PKT_SZ		(USB_EP_ISO_MAX_PKT_SZ << 19)

#define USB_UDC_CTRLIN_MAX_PKT_SZ	(USB_EP_CTRLIN_MAX_PKT_SZ << 19)
#define USB_UDC_CTRLOUT_MAX_PKT_SZ	(USB_EP_CTRLOUT_MAX_PKT_SZ << 19)
#define USB_UDC_BULKIN_MAX_PKT_SZ_HI	(USB_EP_BULKIN_MAX_PKT_SZ_HI << 19)
#define USB_UDC_BULKOUT_MAX_PKT_SZ_HI	(USB_EP_BULKOUT_MAX_PKT_SZ_HI << 19)
#define USB_UDC_BULKIN_MAX_PKT_SZ_FU	(USB_EP_BULKIN_MAX_PKT_SZ_FU << 19)
#define USB_UDC_BULKOUT_MAX_PKT_SZ_FU	(USB_EP_BULKOUT_MAX_PKT_SZ_FU << 19)
#define USB_UDC_INTRIN_MAX_PKT_SZ	(USB_EP_INTRIN_MAX_PKT_SZ << 19)
#define USB_UDC_INTROUT_MAX_PKT_SZ	(USB_EP_INTROUT_MAX_PKT_SZ << 19)
#define USB_UDC_ISOIN_MAX_PKT_SZ	(USB_EP_ISOIN_MAX_PKT_SZ << 19)
#define USB_UDC_ISOOUT_MAX_PKT_SZ	(USB_EP_ISOOUT_MAX_PKT_SZ << 19)

//-------------------------------------
// DMA status quadlet fields
//-------------------------------------
// IN / OUT descriptor specific
#define USB_DMA_RXTX_BYTES		0x0000ffff	// bit mask

// SETUP descriptor specific
#define USB_DMA_CFG_STS			0x0fff0000	// bit mask
#define USB_DMA_CFG_NUM			0x0f000000	// bit mask
#define USB_DMA_INTF_NUM		0x00f00000	// bit mask
#define USB_DMA_ALT_SET			0x000f0000	// bitmask
// ISO OUT descriptor only
#define USB_DMA_FRM_NUM			0x07ff0000	// bit mask
// IN / OUT descriptor specific
#define USB_DMA_LAST			0x08000000	// bit mask

#define USB_DMA_RXTX_STS		0x30000000	// bit mask
#define USB_DMA_RXTX_SUCC		0x00000000
#define USB_DMA_RXTX_DES_ERR		0x10000000
#define USB_DMA_RXTX_BUF_ERR		0x30000000

#define USB_DMA_BUF_STS 		0xc0000000	// bit mask
#define USB_DMA_BUF_HOST_RDY		0x00000000
#define USB_DMA_BUF_DMA_BUSY		0x40000000
#define USB_DMA_BUF_DMA_DONE		0x80000000
#define	USB_DMA_BUF_HOST_BUSY		0xc0000000


//-------------------------------------
// DMA related fields
//-------------------------------------

// USB data buffer size
#define USB_CTRLIN_BUF_SZ		(64 * 8)
#define USB_CTRLOUT_BUF_SZ		(64 * 8)
#define USB_BULKIN_BUF_SZ		(512 * 96) /* modified from 8 to 96 */
#define USB_BULKOUT_BUF_SZ		(512 * 96)  /* to be multiple of 512 */
#define USB_INTRIN_BUF_SZ		512
#define USB_INTROUT_BUF_SZ		512
#define USB_ISOIN_BUF_SZ		(32 * 2)
#define USB_ISOOUT_BUF_SZ		(32 * 2)

// USB data descriptor size
#define USB_CTRLIN_DESC_SZ		8
#define USB_CTRLOUT_DESC_SZ		8
#define USB_BULKOUT_DESC_SZ		96
#define USB_BULKIN_DESC_SZ		96
#define USB_INTRIN_DESC_SZ		1
#define USB_INTROUT_DESC_SZ		1
#define USB_ISOOUT_DESC_SZ		1
#define USB_ISOIN_DESC_SZ		16


#define UDC_HW_DISABLE   		0
#define UDC_HW_ENABLE_DEV_INTR		1
#define UDC_HW_ENABLE_EPx		2
#define UDC_HW_ENABLE_EP0		3
#define UDC_HW_CLEAR    		4


/*====================================================
    desciptor relation definition
====================================================*/
/*---- number of configration ----*/
#define USB_NUM_CONFIG			1

/*---- number of interface ----*/
#define USB_NUM_INTERFACE		3 // Temporary hardcode. USB_CFG_NUMBER_OF_IF

/*---- number of string descriptor ----*/
#define USB_NUM_STRING_DESC		4

/*---- default configuration number ----*/
#define USB_DEFAULT_CONFIG		1

/* TOKEN */
#define COMMAND_TOKEN			0x55434D44
#define STATUS_TOKEN			0x55525350

#define USB_CMD_RDY_TO_RCV		0
#define USB_CMD_RCV_DATA		1
#define USB_CMD_RDY_TO_SND		2
#define USB_CMD_SND_DATA		3
#define USB_CMD_INQUIRY_STATUS		4

/* Ready-to-Recv commands */
#define USB_CMD_SET_MEMORY_READ		1
#define USB_CMD_SET_MEMORY_WRITE	2

#define USB_LEP_CTRL			0	// logical endpoint 0 - control
#define USB_LEP_BULKOUT			1	// logical endpoint 1 - bulk out
#define USB_LEP_BULKIN			1	// logical endpoint 1 - bulk in

#endif

