/*
 * arch/arm/mach-ambarella/include/mach/spi.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2008/02/19 - [Allen Wang] changed to use capabilities and chip ID	 
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

#ifndef __ASM_ARCH_SPI_H
#define __ASM_ARCH_SPI_H

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/
#if (CHIP_REV == A1)
#define SPI_MAX_SLAVE_ID 			3
#else
#define SPI_MAX_SLAVE_ID 			7
#endif

#if (CHIP_REV == A1)
#define SPI_SUPPORT_TISSP_NSM			0
#else     	
#define SPI_SUPPORT_TISSP_NSM			1
#endif

#if ((CHIP_REV == A3) || (CHIP_REV == A5) || (CHIP_REV == A6))
#define SPI_INSTANCES				2
#else
#define SPI_INSTANCES				1
#endif

#if ((CHIP_REV == A1) || (CHIP_REV == A2) || 		\
     (CHIP_REV == A3) || (CHIP_REV == A5) || (CHIP_REV == A6) ) 
#define SPI_EN2_EN3_ENABLED_BY_HOST_ENA_REG	1
#else
#define SPI_EN2_EN3_ENABLED_BY_HOST_ENA_REG	0
#endif

#if ((CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q))
#define SPI_EN2_ENABLED_BY_GPIO2_AFSEL_REG	1
#else
#define SPI_EN2_ENABLED_BY_GPIO2_AFSEL_REG	0
#endif

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define SPI_CTRLR0_OFFSET		0x00
#define SPI_CTRLR1_OFFSET		0x04
#define SPI_SSIENR_OFFSET		0x08
#define SPI_SER_OFFSET			0x10
#define SPI_BAUDR_OFFSET		0x14
#define SPI_TXFTLR_OFFSET		0x18
#define SPI_RXFTLR_OFFSET		0x1c
#define SPI_TXFLR_OFFSET		0x20
#define SPI_RXFLR_OFFSET		0x24
#define SPI_SR_OFFSET			0x28
#define SPI_IMR_OFFSET			0x2c
#define SPI_ISR_OFFSET			0x30
#define SPI_RISR_OFFSET			0x34
#define SPI_TXOICR_OFFSET		0x38
#define SPI_RXOICR_OFFSET		0x3c
#define SPI_RXUICR_OFFSET		0x40
#define SPI_MSTICR_OFFSET		0x44
#define SPI_ICR_OFFSET			0x48
#define SPI_IDR_OFFSET			0x58
#define SPI_VERSION_ID_OFFSET		0x5c
#define SPI_DR_OFFSET			0x60

#define SPI_CTRLR0_REG			SPI_REG(0x00)
#define SPI_CTRLR1_REG			SPI_REG(0x04)
#define SPI_SSIENR_REG			SPI_REG(0x08)
#define SPI_MWCR_REG			SPI_REG(0x0c)
#define SPI_SER_REG			SPI_REG(0x10)
#define SPI_BAUDR_REG			SPI_REG(0x14)
#define SPI_TXFTLR_REG			SPI_REG(0x18)
#define SPI_RXFTLR_REG			SPI_REG(0x1c)
#define SPI_TXFLR_REG			SPI_REG(0x20)
#define SPI_RXFLR_REG			SPI_REG(0x24)
#define SPI_SR_REG			SPI_REG(0x28)
#define SPI_IMR_REG			SPI_REG(0x2c)
#define SPI_ISR_REG			SPI_REG(0x30)
#define SPI_RISR_REG			SPI_REG(0x34)
#define SPI_TXOICR_REG			SPI_REG(0x38)
#define SPI_RXOICR_REG			SPI_REG(0x3c)
#define SPI_RXUICR_REG			SPI_REG(0x40)
#define SPI_MSTICR_REG			SPI_REG(0x44)
#define SPI_ICR_REG			SPI_REG(0x48)
#define SPI_IDR_REG			SPI_REG(0x58)
#define SPI_VERSION_ID_REG		SPI_REG(0x5c)
#define SPI_DR_REG			SPI_REG(0x60)

#if (SPI_INSTANCES == 2)
#define SPI2_CTRLR0_REG			SPI2_REG(0x00)
#define SPI2_CTRLR1_REG			SPI2_REG(0x04)
#define SPI2_SSIENR_REG			SPI2_REG(0x08)
#define SPI2_MWCR_REG			SPI2_REG(0x0c)
#define SPI2_SER_REG			SPI2_REG(0x10)
#define SPI2_BAUDR_REG			SPI2_REG(0x14)
#define SPI2_TXFTLR_REG			SPI2_REG(0x18)
#define SPI2_RXFTLR_REG			SPI2_REG(0x1c)
#define SPI2_TXFLR_REG			SPI2_REG(0x20)
#define SPI2_RXFLR_REG			SPI2_REG(0x24)
#define SPI2_SR_REG			SPI2_REG(0x28)
#define SPI2_IMR_REG			SPI2_REG(0x2c)
#define SPI2_ISR_REG			SPI2_REG(0x30)
#define SPI2_RISR_REG			SPI2_REG(0x34)
#define SPI2_TXOICR_REG			SPI2_REG(0x38)
#define SPI2_RXOICR_REG			SPI2_REG(0x3c)
#define SPI2_RXUICR_REG			SPI2_REG(0x40)
#define SPI2_MSTICR_REG			SPI2_REG(0x44)
#define SPI2_ICR_REG			SPI2_REG(0x48)
#define SPI2_IDR_REG			SPI2_REG(0x58)
#define SPI2_VERSION_ID_REG		SPI2_REG(0x5c)
#define SPI2_DR_REG			SPI2_REG(0x60)
#endif

#endif

