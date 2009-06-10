/*
 * arch/arm/mach-ambarella/include/mach/busaddr.h
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

#ifndef __ASM_ARCH_BUSADDR_H__
#define __ASM_ARCH_BUSADDR_H__

/* AHB slave offsets */

#define FIO_FIFO_OFFSET			0x0000
#define FIO_OFFSET			0x1000
#define SD_OFFSET			0x2000
#define VIC_OFFSET			0x3000
#define DRAM_OFFSET			0x4000
#define DMA_OFFSET			0x5000
#define USBDC_OFFSET			0x6000
#define VOUT_OFFSET			0x8000
#define VIN_OFFSET			0x9000
#define I2S_OFFSET			0xa000
#define HOST_OFFSET			0xb000
#define SD2_OFFSET			0xc000
#define SDIO_OFFSET			0xc000

#if (CHIP_REV == A6)
#define MS_OFFSET			0x17000
#else
#define MS_OFFSET			0xc000
#endif

#if (CHIP_REV == A6)
#define HIF_OFFSET			0x14000
#else
#define HIF_OFFSET			0xd000
#endif

#define ETH_OFFSET			0xe000
#define ETH_DMA_OFFSET			0xf000
#define VIC2_OFFSET			0x10000
#define VOUT2_OFFSET			0x11000
#define VOUT_CLUT_OFFSET		0x11000
#define DMA_FIOS_OFFSET			0x12000

#if (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define HDMI_OFFSET			0xb000
#else
#define HDMI_OFFSET			0x13000
#endif

#define TS_OFFSET			0x15000
#define SPIB_OFFSET			0x16000



/* AHB slave base addresses */
#define FIO_FIFO_BASE			(AHB_BASE + FIO_FIFO_OFFSET)
#define FIO_BASE			(AHB_BASE + FIO_OFFSET)
#define SD_BASE				(AHB_BASE + SD_OFFSET)
#define VIC_BASE			(AHB_BASE + VIC_OFFSET)
#define DRAM_BASE			(AHB_BASE + DRAM_OFFSET)
#define DMA_BASE			(AHB_BASE + DMA_OFFSET)
#define USBDC_BASE			(AHB_BASE + USBDC_OFFSET)
#define VOUT_BASE			(AHB_BASE + VOUT_OFFSET)
#define VIN_BASE			(AHB_BASE + VIN_OFFSET)
#define I2S_BASE			(AHB_BASE + I2S_OFFSET)
#define SD2_BASE			(AHB_BASE + SD2_OFFSET)
#define MS_BASE				(AHB_BASE + MS_OFFSET)
#define HOST_BASE			(AHB_BASE + HOST_OFFSET)
#define HIF_BASE			(AHB_BASE + HIF_OFFSET)
#define ETH_BASE			(AHB_BASE + ETH_OFFSET)
#define ETH_DMA_BASE			(AHB_BASE + ETH_DMA_OFFSET)
#define VIC2_BASE			(AHB_BASE + VIC2_OFFSET)
#define VOUT2_BASE			(AHB_BASE + VOUT2_OFFSET)
#define VOUT_CLUT_BASE			(AHB_BASE + VOUT_CLUT_OFFSET)
#define DMA_FIOS_BASE			(AHB_BASE + DMA_FIOS_OFFSET)
#define HDMI_BASE			(AHB_BASE + HDMI_OFFSET)
#define TS_BASE				(AHB_BASE + TS_OFFSET)
#define SPIB_BASE			(AHB_BASE + SPIB_OFFSET)

/* AHB slave registers */

#define FIO_REG(x)			(FIO_BASE + (x))
#define XD_REG(x)			(FIO_BASE + (x))
#define CF_REG(x)			(FIO_BASE + (x))
#define SD_REG(x)			(SD_BASE + (x))
#define VIC_REG(x)			(VIC_BASE + (x))
#define DRAM_REG(x)			(DRAM_BASE + (x))
#define DMA_REG(x)			(DMA_BASE + (x))
#define USBDC_REG(x)			(USBDC_BASE + (x))
#define VOUT_REG(x)			(VOUT_BASE + (x))
#define VIN_REG(x)			(VIN_BASE + (x))
#define I2S_REG(x)			(I2S_BASE + (x))
#define SD2_REG(x)			(SD2_BASE + (x))
#define MS_REG(x)			(MS_BASE + (x))
#define HOST_REG(x)			(HOST_BASE + (x))
#define HIF_REG(x)			(HIF_BASE + (x))
#define ETH_REG(x)			(ETH_BASE + (x))
#define ETH_DMA_REG(x)			(ETH_DMA_BASE + (x))
#define VIC2_REG(x)			(VIC2_BASE + (x))
#define VOUT2_REG(x)			(VOUT2_BASE + (x))
#define VOUT_CLUT_REG(x)		(VOUT_CLUT_BASE + (x))
#define DMA_FIOS_REG(x)			(DMA_FIOS_BASE + (x))
#define HDMI_REG(x)			(HDMI_BASE + (x))
#define TS_REG(x)			(TS_BASE + (x))
#define SPIB_REG(x)			(SPIB_BASE + (x))

/*----------------------------------------------------------------------------*/

/* APB slave offsets */

#define TSSI_OFFSET			0x1000
#define SSI_OFFSET			0x2000
#define SPI_OFFSET			0x2000
#define IDC_OFFSET			0x3000
#define STEPPER_OFFSET			0x4000
#define UART_OFFSET			0x5000
#define IR_OFFSET			0x6000
#define IDC2_OFFSET			0x7000
#define PWM_OFFSET			0x8000
#define GPIO0_OFFSET			0x9000
#define GPIO1_OFFSET			0xa000
#define TIMER_OFFSET			0xb000
#define WDOG_OFFSET			0xc000
#define ADC_OFFSET			0xd000
#define RTC_OFFSET			0xd000
#define GPIO2_OFFSET			0xe000
#define SPI2_OFFSET			0xf000
#define GPIO3_OFFSET			0x1f000
#if	defined(__A1_FPGA__)
#define RCT_OFFSET			0xf000
#else
#define RCT_OFFSET			0x170000
#endif
#define AUC_OFFSET			0x180000

/* APB slave base addresses */

#define TSSI_BASE			(APB_BASE + TSSI_OFFSET)
#define SSI_BASE			(APB_BASE + SPI_OFFSET)
#define SPI_BASE			(APB_BASE + SPI_OFFSET)
#define IDC_BASE			(APB_BASE + IDC_OFFSET)
#define STEPPER_BASE			(APB_BASE + STEPPER_OFFSET)
#define UART_BASE			(APB_BASE + UART_OFFSET)
#define IR_BASE				(APB_BASE + IR_OFFSET)
#define IDC2_BASE			(APB_BASE + IDC2_OFFSET)
#define PWM_BASE			(APB_BASE + PWM_OFFSET)
#define GPIO0_BASE			(APB_BASE + GPIO0_OFFSET)
#define GPIO1_BASE			(APB_BASE + GPIO1_OFFSET)
#define TIMER_BASE			(APB_BASE + TIMER_OFFSET)
#define WDOG_BASE			(APB_BASE + WDOG_OFFSET)
#define ADC_BASE			(APB_BASE + ADC_OFFSET)
#define RTC_BASE			(APB_BASE + RTC_OFFSET)
#define GPIO2_BASE			(APB_BASE + GPIO2_OFFSET)
#define SPI2_BASE			(APB_BASE + SPI2_OFFSET)
#define GPIO3_BASE			(APB_BASE + GPIO3_OFFSET)
#define RCT_BASE			(APB_BASE + RCT_OFFSET)
#define AUC_BASE			(APB_BASE + AUC_OFFSET)

/* APB slave registers */

#define TSSI_REG(x)			(TSSI_BASE + (x))
#define SPI_REG(x)			(SPI_BASE + (x))
#define SSI_REG(x)			(SPI_BASE + (x))
#define IDC_REG(x)			(IDC_BASE + (x))
#define ST_REG(x)			(STEPPER_BASE + (x))
#define UART_REG(x)			(UART_BASE + (x))
#define UART0_REG(x)			(UART_BASE + (x))
#define UART1_REG(x)			(UART_BASE + 0x00010000 + (x))
#define IR_REG(x)			(IR_BASE + (x))
#define IDC2_REG(x)			(IDC2_BASE + (x))
#define PWM_REG(x)			(PWM_BASE + (x))
#define GPIO0_REG(x)			(GPIO0_BASE + (x))
#define GPIO1_REG(x)			(GPIO1_BASE + (x))
#define TIMER_REG(x)			(TIMER_BASE + (x))
#define WDOG_REG(x)			(WDOG_BASE + (x))
#define ADC_REG(x)			(ADC_BASE + (x))
#define RTC_REG(x)			(RTC_BASE + (x))
#define GPIO2_REG(x)			(GPIO2_BASE + (x))
#define SPI2_REG(x)			(SPI2_BASE + (x))
#define GPIO3_REG(x)			(GPIO3_BASE + (x))
#define RCT_REG(x)			(RCT_BASE + (x))
#define AUC_REG(x)			(AUC_BASE + (x))

/* DSP Debug ports */

#define DSP_DEBUG0_OFFSET               0x100000
#define DSP_DEBUG1_OFFSET               0x110000
#define DSP_DEBUG2_OFFSET               0x120000
#define DSP_DEBUG3_OFFSET               0x130000
#define DSP_DEBUG4_OFFSET               0x140000
#define DSP_DEBUG5_OFFSET               0x150000
#define DSP_DEBUG6_OFFSET               0x160000
#define DSP_DEBUG7_OFFSET               0x170000

#define DSP_DEBUG0_BASE                 (APB_BASE + DSP_DEBUG0_OFFSET)
#define DSP_DEBUG1_BASE                 (APB_BASE + DSP_DEBUG1_OFFSET)
#define DSP_DEBUG2_BASE                 (APB_BASE + DSP_DEBUG2_OFFSET)
#define DSP_DEBUG3_BASE                 (APB_BASE + DSP_DEBUG3_OFFSET)
#define DSP_DEBUG4_BASE                 (APB_BASE + DSP_DEBUG4_OFFSET)
#define DSP_DEBUG5_BASE                 (APB_BASE + DSP_DEBUG5_OFFSET)
#define DSP_DEBUG6_BASE                 (APB_BASE + DSP_DEBUG6_OFFSET)
#define DSP_DEBUG7_BASE                 (APB_BASE + DSP_DEBUG7_OFFSET)

/*----------------------------------------------------------------------------*/

#endif

