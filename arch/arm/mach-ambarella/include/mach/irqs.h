/*
 * arch/arm/mach-ambarella/include/mach/irqs.h
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

#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

#include <mach/hardware.h>

/* The following IRQ numbers are common for all ARCH */

#if (CHIP_REV == A5) || (CHIP_REV == A6)
#define MAX_IRQ_NUMBER		192 /* VIC1:32 + VIC2:32 + GPIO: 128 = 192 */
#elif (CHIP_REV == A3) 
#define MAX_IRQ_NUMBER		160 /* VIC1:32 + VIC2:32 + GPIO: 96 = 160 */
#else
#define MAX_IRQ_NUMBER		128
#endif

#if (CHIP_REV == A1) || (CHIP_REV == A2) 
#define GPIO_VICINT_INSTANCES		2	
#elif (CHIP_REV == A3) || (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define GPIO_VICINT_INSTANCES		3
#else
#define GPIO_VICINT_INSTANCES		4
#endif

/* The following are ARCH specific IRQ numbers */

#define USBVBUS_IRQ	 	0
#define VOUT_IRQ	 	1
#define VIN_IRQ		 	2
#define VDSP_IRQ	 	3
#define USBC_IRQ	 	4
#define HOSTTX_IRQ	 	5
#define HOSTRX_IRQ	 	6
#define HIF_ARM1_IRQ		6	/* A2S, A2M */
#define I2STX_IRQ	 	7
#define I2SRX_IRQ	 	8
#define UART0_IRQ	 	9
#define GPIO0_IRQ		10
#define GPIO1_IRQ		11
#define TIMER1_IRQ		12
#define TIMER2_IRQ		13
#define TIMER3_IRQ		14
#define DMA_IRQ			15
#define FIOCMD_IRQ		16
#define FIODMA_IRQ		17
#define SD_IRQ			18
#define IDC_IRQ			19
#define SSI_IRQ			20
#define WDT_IRQ			21
#define IRIF_IRQ		22
#define CFCD1_IRQ		23
#define SD2CD_IRQ		23	/* Shared with CFCD1_IRQ */
#define XDCD_IRQ		24
#define SD1CD_IRQ		24	/* SD1 SD card card detection. */

#if (CHIP_REV == A2) || (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) || \
	(CHIP_REV == A6)  
#define SD2_IRQ			18	/* Shared with SD_IRQ */
#else
#define SD2_IRQ			25
#endif
#define ETH0_IRQ		25

#define SDCD_IRQ		26
#define GPIO3_IRQ		26	/* A5 */

#if (CHIP_REV == A2)
#define ETH_IRQ			 4	/* Shared with USBC_IRQ */
#else
#define ETH_IRQ			27
#endif

#define IDSP_ERROR_IRQ		28
#define VOUT_SYNC_MISSED_IRQ	29
#define HIF_ARM2_IRQ		29	/* A2S, A2M */

#if (CHIP_REV == A2)
#define GPIO2_IRQ		11	/* Shared with GPIO1_IRQ */
#else
#define GPIO2_IRQ		30
#endif

#define CFCD2_IRQ		31
#define CD2ND_BIT_CD_IRQ	31

#define AUDIO_ORC_IRQ 		(32 + 0)
#define DMA_FIOS_IRQ 		(32 + 1)
#define ADC_LEVEL_IRQ 		(32 + 2)
#define VOUT1_SYNC_MISSED_IRQ	(32 + 3)
#define IDC2_IRQ		(32 + 4)
#define IDSP_LAST_PIXEL_IRQ	(32 + 5)
#define IDSP_VSYNC_IRQ		(32 + 6)
#define IDSP_SENSOR_VSYNC_IRQ	(32 + 7)

#if (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define HDMI_IRQ		26
#else
#define HDMI_IRQ		(32 + 8)
#endif

#define SSI2_IRQ 		(32 + 9)
#define VOUT_TV_SYNC_IRQ 	(32 + 10)
#define VOUT_LCD_SYNC_IRQ 	(32 + 11)
#define HIF_ARM_INT2_IRQ 	(32 + 12)
#define HIF_ARM_INT1_IRQ 	(32 + 13)
#define CODE_VOUT_B_IRQ 	(32 + 14)
#define CODE_VDSP_3_IRQ 	(32 + 15)
#define CODE_VDSP_2_IRQ 	(32 + 16)
#define CODE_VDSP_1_IRQ 	(32 + 17)
#define TS_CH0_TX_IRQ 		(32 + 18)
#define TS_CH1_TX_IRQ 		(32 + 19)
#define TS_CH0_RX_IRQ 		(32 + 20)
#define TS_CH1_RX_IRQ 		(32 + 21)

#if (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define MS_IRQ	 		5	/* A2S, A2M */
#else
#define MS_IRQ	 		(32 + 22)	
#endif

#if (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define MOTOR_IRQ		25	/* A2S, A2M */
#else
#define MOTOR_IRQ		(32 + 23)
#endif


#define GPIO_IRQ(x)		((x) + VIC_INSTANCES * 32)

/*
 * The number of IRQs depends on the number of VIC and GPIO controllers
 * on the Ambarella Processor architecture
 */
#define NR_VIC_IRQ_SIZE		(32)
#define NR_VIC_IRQS		(VIC_INSTANCES * NR_VIC_IRQ_SIZE)
#define NR_GPIO_IRQS		(GPIO_INSTANCES * NR_VIC_IRQ_SIZE)

#define VIC_INT_VEC_OFFSET	(0)
#define VIC2_INT_VEC_OFFSET	(VIC_INT_VEC_OFFSET + NR_VIC_IRQ_SIZE)
#define GPIO_INT_VEC_OFFSET	(VIC_INSTANCES * NR_VIC_IRQ_SIZE)
#define GPIO0_INT_VEC_OFFSET	(GPIO_INT_VEC_OFFSET + 0 * NR_VIC_IRQ_SIZE)
#define GPIO1_INT_VEC_OFFSET	(GPIO_INT_VEC_OFFSET + 1 * NR_VIC_IRQ_SIZE)
#define GPIO2_INT_VEC_OFFSET	(GPIO_INT_VEC_OFFSET + 2 * NR_VIC_IRQ_SIZE)
#define GPIO3_INT_VEC_OFFSET	(GPIO_INT_VEC_OFFSET + 3 * NR_VIC_IRQ_SIZE)

#define NR_IRQS			(NR_VIC_IRQS + NR_GPIO_IRQS)
#define FIQ_START		NR_IRQS

/******************************************/
/* Interrupt numbers of the uITRON kernel */
/******************************************/
#define USBVBUS_INT_VEC	 		0
#define VOUT_INT_VEC	 		1
#define VIN_INT_VEC	 		2
#define VDSP_INT_VEC	 		3
#define USBC_INT_VEC	 		4
#define HOSTTX_INT_VEC	 		5
#define MS_INT_VEC	 		5	/* A2S, A2M */
#define HOSTRX_INT_VEC	 		6
#define HIF_ARM1_INT_VEC 		6	/* A2S, A2M */
#define I2STX_INT_VEC	 		7
#define I2SRX_INT_VEC	 		8
#define UART0_INT_VEC	 		9
#define GPIO0_INT_VEC			10
#define GPIO1_INT_VEC			11
#define TIMER1_INT_VEC			12
#define TIMER2_INT_VEC			13
#define TIMER3_INT_VEC			14
#define DMA_INT_VEC			15
#define FIOCMD_INT_VEC			16
#define FIODMA_INT_VEC			17
#define SD_INT_VEC			18
#define IDC_INT_VEC			19
#define SSI_INT_VEC			20
#define WDT_INT_VEC			21
#define IRIF_INT_VEC			22
#define CFCD1_INT_VEC			23
#define SD2CD_INT_VEC			23	/* Shared with CFCD1_INT_VEC */
#define XDCD_INT_VEC			24
#define SD1CD_INT_VEC			24	/* SD1 SD card card detection. */
#define ETH0_INT_VEC			25

#if (CHIP_REV == A2) || (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) 
#define SD2_INT_VEC			18	/* Shared with SD_INT_VEC */
#else
#define SD2_INT_VEC			25
#endif

#define MOTOR_INT_VEC			25	/* A2S, A2M */
#define SDCD_INT_VEC			26	
#define GPIO3_INT_VEC			26	/* A5 */

#if (CHIP_REV == A2)
#define ETH_INT_VEC			4	/* Shared with USBC_INT_VEC */
#else
#define ETH_INT_VEC			27
#endif

#define IDSP_ERROR_INT_VEC		28
#define VOUT_SYNC_MISSED_INT_VEC 	29
#define HIF_ARM2_INT_VEC		29	/* A2S, A2M */

#if (CHIP_REV == A2)
#define GPIO2_INT_VEC			11	/* Shared with GPIO1_INT_VEC */
#else
#define GPIO2_INT_VEC			30
#endif
 
#define CD2ND_BIT_CD			31
#define CFCD2_INT_VEC			31

#define VIC2_INT_VEC(x)			((x) + VIC2_INT_VEC_OFFSET)
#define AUDIO_ORC_INT_VEC 		VIC2_INT_VEC(0)
#define DMA_FIOS_INT_VEC 		VIC2_INT_VEC(1)
#define ADC_LEVEL_INT_VEC 		VIC2_INT_VEC(2)
#define VOUT1_SYNC_MISSED_INT_VEC 	VIC2_INT_VEC(3)
#define IDC2_INT_VEC			VIC2_INT_VEC(4)
#define IDSP_LAST_PIXEL_INT_VEC 	VIC2_INT_VEC(5)
#define IDSP_VSYNC_INT_VEC 		VIC2_INT_VEC(6)
#define IDSP_SENSOR_VSYNC_INT_VEC 	VIC2_INT_VEC(7)

#if (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define HDMI_INT_VEC			26
#else
#define HDMI_INT_VEC 			VIC2_INT_VEC(8)
#endif

#define SSI2_INT_VEC 			VIC2_INT_VEC(9)
#define VOUT_TV_SYNC_INT_VEC 		VIC2_INT_VEC(10)
#define VOUT_LCD_SYNC_INT_VEC 		VIC2_INT_VEC(11)

#define GPIO_INT_VEC(x) 		((x) + GPIO_INT_VEC_OFFSET)
#define GPIO00_INT_VEC			GPIO_INT_VEC(0)
#define GPIO01_INT_VEC			GPIO_INT_VEC(1)
#define GPIO02_INT_VEC			GPIO_INT_VEC(2)
#define GPIO03_INT_VEC			GPIO_INT_VEC(3)
#define GPIO04_INT_VEC			GPIO_INT_VEC(4)
#define GPIO05_INT_VEC			GPIO_INT_VEC(5)
#define GPIO06_INT_VEC			GPIO_INT_VEC(6)
#define GPIO07_INT_VEC			GPIO_INT_VEC(7)
#define GPIO08_INT_VEC			GPIO_INT_VEC(8)
#define GPIO09_INT_VEC			GPIO_INT_VEC(9)
#define GPIO10_INT_VEC			GPIO_INT_VEC(10)
#define GPIO11_INT_VEC			GPIO_INT_VEC(11)
#define GPIO12_INT_VEC			GPIO_INT_VEC(12)
#define GPIO13_INT_VEC			GPIO_INT_VEC(13)
#define GPIO14_INT_VEC			GPIO_INT_VEC(14)
#define GPIO15_INT_VEC			GPIO_INT_VEC(15)
#define GPIO16_INT_VEC			GPIO_INT_VEC(16)
#define GPIO17_INT_VEC			GPIO_INT_VEC(17)
#define GPIO18_INT_VEC			GPIO_INT_VEC(18)
#define GPIO19_INT_VEC			GPIO_INT_VEC(19)
#define GPIO20_INT_VEC			GPIO_INT_VEC(20)
#define GPIO21_INT_VEC			GPIO_INT_VEC(21)
#define GPIO22_INT_VEC			GPIO_INT_VEC(22)
#define GPIO23_INT_VEC			GPIO_INT_VEC(23)
#define GPIO24_INT_VEC			GPIO_INT_VEC(24)
#define GPIO25_INT_VEC			GPIO_INT_VEC(25)
#define GPIO26_INT_VEC			GPIO_INT_VEC(26)
#define GPIO27_INT_VEC			GPIO_INT_VEC(27)
#define GPIO28_INT_VEC			GPIO_INT_VEC(28)
#define GPIO29_INT_VEC			GPIO_INT_VEC(29)
#define GPIO30_INT_VEC			GPIO_INT_VEC(30)
#define GPIO31_INT_VEC			GPIO_INT_VEC(31)
#define GPIO32_INT_VEC			GPIO_INT_VEC(32)
#define GPIO33_INT_VEC			GPIO_INT_VEC(33)
#define GPIO34_INT_VEC			GPIO_INT_VEC(34)
#define GPIO35_INT_VEC			GPIO_INT_VEC(35)
#define GPIO36_INT_VEC			GPIO_INT_VEC(36)
#define GPIO37_INT_VEC			GPIO_INT_VEC(37)
#define GPIO38_INT_VEC			GPIO_INT_VEC(38)
#define GPIO39_INT_VEC			GPIO_INT_VEC(39)
#define GPIO40_INT_VEC			GPIO_INT_VEC(40)
#define GPIO41_INT_VEC			GPIO_INT_VEC(41)
#define GPIO42_INT_VEC			GPIO_INT_VEC(42)
#define GPIO43_INT_VEC			GPIO_INT_VEC(43)
#define GPIO44_INT_VEC			GPIO_INT_VEC(44)
#define GPIO45_INT_VEC			GPIO_INT_VEC(45)
#define GPIO46_INT_VEC			GPIO_INT_VEC(46)
#define GPIO47_INT_VEC			GPIO_INT_VEC(47)
#define GPIO48_INT_VEC			GPIO_INT_VEC(48)
#define GPIO49_INT_VEC			GPIO_INT_VEC(49)
#define GPIO50_INT_VEC			GPIO_INT_VEC(50)
#define GPIO51_INT_VEC			GPIO_INT_VEC(51)
#define GPIO52_INT_VEC			GPIO_INT_VEC(52)
#define GPIO53_INT_VEC			GPIO_INT_VEC(53)
#define GPIO54_INT_VEC			GPIO_INT_VEC(54)
#define GPIO55_INT_VEC			GPIO_INT_VEC(55)
#define GPIO56_INT_VEC			GPIO_INT_VEC(56)
#define GPIO57_INT_VEC			GPIO_INT_VEC(57)
#define GPIO58_INT_VEC			GPIO_INT_VEC(58)
#define GPIO59_INT_VEC			GPIO_INT_VEC(59)
#define GPIO60_INT_VEC			GPIO_INT_VEC(60)
#define GPIO61_INT_VEC			GPIO_INT_VEC(61)
#define GPIO62_INT_VEC			GPIO_INT_VEC(62)
#define GPIO63_INT_VEC			GPIO_INT_VEC(63)

#if (MAX_IRQ_NUMBER >= 128)
#define GPIO64_INT_VEC			GPIO_INT_VEC(64)
#define GPIO65_INT_VEC			GPIO_INT_VEC(65)
#define GPIO66_INT_VEC			GPIO_INT_VEC(66)
#define GPIO67_INT_VEC			GPIO_INT_VEC(67)
#define GPIO68_INT_VEC			GPIO_INT_VEC(68)
#define GPIO69_INT_VEC			GPIO_INT_VEC(69)
#define GPIO70_INT_VEC			GPIO_INT_VEC(70)
#define GPIO71_INT_VEC			GPIO_INT_VEC(71)
#define GPIO72_INT_VEC			GPIO_INT_VEC(72)
#define GPIO73_INT_VEC			GPIO_INT_VEC(73)
#define GPIO74_INT_VEC			GPIO_INT_VEC(74)
#define GPIO75_INT_VEC			GPIO_INT_VEC(75)
#define GPIO76_INT_VEC			GPIO_INT_VEC(76)
#define GPIO77_INT_VEC			GPIO_INT_VEC(77)
#define GPIO78_INT_VEC			GPIO_INT_VEC(78)
#define GPIO79_INT_VEC			GPIO_INT_VEC(79)
#define GPIO80_INT_VEC			GPIO_INT_VEC(80)
#define GPIO81_INT_VEC			GPIO_INT_VEC(81)
#define GPIO82_INT_VEC			GPIO_INT_VEC(82)
#define GPIO83_INT_VEC			GPIO_INT_VEC(83)
#define GPIO84_INT_VEC			GPIO_INT_VEC(84)
#define GPIO85_INT_VEC			GPIO_INT_VEC(85)
#define GPIO86_INT_VEC			GPIO_INT_VEC(86)
#define GPIO87_INT_VEC			GPIO_INT_VEC(87)
#define GPIO88_INT_VEC			GPIO_INT_VEC(88)
#define GPIO89_INT_VEC			GPIO_INT_VEC(89)
#define GPIO90_INT_VEC			GPIO_INT_VEC(90)
#define GPIO91_INT_VEC			GPIO_INT_VEC(91)
#define GPIO92_INT_VEC			GPIO_INT_VEC(92)
#define GPIO93_INT_VEC			GPIO_INT_VEC(93)
#define GPIO94_INT_VEC			GPIO_INT_VEC(94)
#define GPIO95_INT_VEC			GPIO_INT_VEC(95)
#endif

#if (MAX_IRQ_NUMBER >= 192)
#define GPIO96_INT_VEC			GPIO_INT_VEC(96)
#define GPIO97_INT_VEC			GPIO_INT_VEC(97)
#define GPIO98_INT_VEC			GPIO_INT_VEC(98)
#define GPIO99_INT_VEC			GPIO_INT_VEC(99)
#define GPIO100_INT_VEC			GPIO_INT_VEC(100)
#define GPIO101_INT_VEC			GPIO_INT_VEC(101)
#define GPIO102_INT_VEC			GPIO_INT_VEC(102)
#define GPIO103_INT_VEC			GPIO_INT_VEC(103)
#define GPIO104_INT_VEC			GPIO_INT_VEC(104)
#define GPIO105_INT_VEC			GPIO_INT_VEC(105)
#define GPIO106_INT_VEC			GPIO_INT_VEC(106)
#define GPIO107_INT_VEC			GPIO_INT_VEC(107)
#define GPIO108_INT_VEC			GPIO_INT_VEC(108)
#define GPIO109_INT_VEC			GPIO_INT_VEC(109)
#define GPIO110_INT_VEC			GPIO_INT_VEC(110)
#define GPIO111_INT_VEC			GPIO_INT_VEC(111)
#define GPIO112_INT_VEC			GPIO_INT_VEC(112)
#define GPIO113_INT_VEC			GPIO_INT_VEC(113)
#define GPIO114_INT_VEC			GPIO_INT_VEC(114)
#define GPIO115_INT_VEC			GPIO_INT_VEC(115)
#define GPIO116_INT_VEC			GPIO_INT_VEC(116)
#define GPIO117_INT_VEC			GPIO_INT_VEC(117)
#define GPIO118_INT_VEC			GPIO_INT_VEC(118)
#define GPIO119_INT_VEC			GPIO_INT_VEC(119)
#define GPIO120_INT_VEC			GPIO_INT_VEC(120)
#define GPIO121_INT_VEC			GPIO_INT_VEC(121)
#define GPIO122_INT_VEC			GPIO_INT_VEC(122)
#define GPIO123_INT_VEC			GPIO_INT_VEC(123)
#define GPIO124_INT_VEC			GPIO_INT_VEC(124)
#define GPIO125_INT_VEC			GPIO_INT_VEC(125)
#define GPIO126_INT_VEC			GPIO_INT_VEC(126)
#define GPIO127_INT_VEC			GPIO_INT_VEC(127)
#endif

#ifndef __ASSEMBLER__

static inline int gpio_to_irq(unsigned gpio)
{
	if (gpio < GPIO_MAX_LINES)
		return GPIO_INT_VEC(gpio);

	return -EINVAL;
}

static inline int irq_to_gpio(unsigned irq)
{
	if ((irq > GPIO_INT_VEC(0)) && (irq < MAX_IRQ_NUMBER))
		return irq - GPIO_INT_VEC(0);

	return -EINVAL;
}

#endif /* __ASSEMBLER__ */

#endif /* __ASM_ARCH_IRQS_H */

