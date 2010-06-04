/*
 * ambhw/gpio.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *
 * Copyright (C) 2006-2008, Ambarella, Inc.
 */

#ifndef __AMBHW__GPIO_H__
#define __AMBHW__GPIO_H__

#include <ambhw/chip.h>
#include <ambhw/busaddr.h>

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/
#if 	(CHIP_REV == A5) || (CHIP_REV == A6)
#define GPIO_INSTANCES	4
#elif 	(CHIP_REV == A2) || (CHIP_REV == A3) || 		\
	(CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) ||	\
	(CHIP_REV == A5S) || (CHIP_REV == A5L)
#define GPIO_INSTANCES	3
#elif 	(CHIP_REV == A7)
#define GPIO_INSTANCES	5
#else
#define GPIO_INSTANCES	2
#endif

#if 	(CHIP_REV == A1)
#define GPIO_MAX_LINES			58
#elif 	(CHIP_REV == A2) || (CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q)
#define GPIO_MAX_LINES			81
#elif 	(CHIP_REV == A3) || (CHIP_REV == A5S) || (CHIP_REV == A5L)
#define GPIO_MAX_LINES			96
#elif 	(CHIP_REV == A5) || (CHIP_REV == A6)
#define GPIO_MAX_LINES			128
#elif 	(CHIP_REV == A7)
#define GPIO_MAX_LINES			160
#else
#define GPIO_MAX_LINES			128
#endif

#if 	(CHIP_REV == A2)
#define GPIO_BWO_A2			1
#define GPIO_UNDEFINED_59_63		1
#else
#define GPIO_BWO_A2			0
#define GPIO_UNDEFINED_59_63		0
#endif

/* SW definitions */
#define GPIO_HIGH			1
#define GPIO_LOW			0

/* GPIO function selection */
/* Select SW or HW control and input/output direction of S/W function */
#define GPIO_FUNC_SW_INPUT		0
#define GPIO_FUNC_SW_OUTPUT		1
#define GPIO_FUNC_HW			2

/* GPIO 50~53 INTERNAL MUXER (A5S)*/
#define ENABLE_STEPPERC			1
#define ENABLE_SPI_SLAVE		2
#define ENABLE_UART1			3

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

#define GPIO_DATA_OFFSET		0x00
#define GPIO_DIR_OFFSET			0x04
#define GPIO_IS_OFFSET			0x08
#define GPIO_IBE_OFFSET			0x0c
#define GPIO_IEV_OFFSET			0x10
#define GPIO_IE_OFFSET			0x14
#define GPIO_AFSEL_OFFSET		0x18
#define GPIO_RIS_OFFSET			0x1c
#define GPIO_MIS_OFFSET			0x20
#define GPIO_IC_OFFSET			0x24
#define GPIO_MASK_OFFSET		0x28
#define GPIO_ENABLE_OFFSET		0x2c

#define GPIO0_DATA_REG			GPIO0_REG(0x00)
#define GPIO0_DIR_REG			GPIO0_REG(0x04)
#define GPIO0_IS_REG			GPIO0_REG(0x08)
#define GPIO0_IBE_REG			GPIO0_REG(0x0c)
#define GPIO0_IEV_REG			GPIO0_REG(0x10)
#define GPIO0_IE_REG			GPIO0_REG(0x14)
#define GPIO0_AFSEL_REG			GPIO0_REG(0x18)
#define GPIO0_RIS_REG			GPIO0_REG(0x1c)
#define GPIO0_MIS_REG			GPIO0_REG(0x20)
#define GPIO0_IC_REG			GPIO0_REG(0x24)
#define GPIO0_MASK_REG			GPIO0_REG(0x28)
#define GPIO0_ENABLE_REG		GPIO0_REG(0x2c)

#define GPIO1_DATA_REG			GPIO1_REG(0x00)
#define GPIO1_DIR_REG			GPIO1_REG(0x04)
#define GPIO1_IS_REG			GPIO1_REG(0x08)
#define GPIO1_IBE_REG			GPIO1_REG(0x0c)
#define GPIO1_IEV_REG			GPIO1_REG(0x10)
#define GPIO1_IE_REG			GPIO1_REG(0x14)
#define GPIO1_AFSEL_REG			GPIO1_REG(0x18)
#define GPIO1_RIS_REG			GPIO1_REG(0x1c)
#define GPIO1_MIS_REG			GPIO1_REG(0x20)
#define GPIO1_IC_REG			GPIO1_REG(0x24)
#define GPIO1_MASK_REG			GPIO1_REG(0x28)
#define GPIO1_ENABLE_REG		GPIO1_REG(0x2c)

#if	(GPIO_INSTANCES >= 3)
#define GPIO2_DATA_REG			GPIO2_REG(0x00)
#define GPIO2_DIR_REG			GPIO2_REG(0x04)
#define GPIO2_IS_REG			GPIO2_REG(0x08)
#define GPIO2_IBE_REG			GPIO2_REG(0x0c)
#define GPIO2_IEV_REG			GPIO2_REG(0x10)
#define GPIO2_IE_REG			GPIO2_REG(0x14)
#define GPIO2_AFSEL_REG			GPIO2_REG(0x18)
#define GPIO2_RIS_REG			GPIO2_REG(0x1c)
#define GPIO2_MIS_REG			GPIO2_REG(0x20)
#define GPIO2_IC_REG			GPIO2_REG(0x24)
#define GPIO2_MASK_REG			GPIO2_REG(0x28)
#define GPIO2_ENABLE_REG		GPIO2_REG(0x2c)
#endif

#if 	(GPIO_INSTANCES >= 4)
#define GPIO3_DATA_REG			GPIO3_REG(0x00)
#define GPIO3_DIR_REG			GPIO3_REG(0x04)
#define GPIO3_IS_REG			GPIO3_REG(0x08)
#define GPIO3_IBE_REG			GPIO3_REG(0x0c)
#define GPIO3_IEV_REG			GPIO3_REG(0x10)
#define GPIO3_IE_REG			GPIO3_REG(0x14)
#define GPIO3_AFSEL_REG			GPIO3_REG(0x18)
#define GPIO3_RIS_REG			GPIO3_REG(0x1c)
#define GPIO3_MIS_REG			GPIO3_REG(0x20)
#define GPIO3_IC_REG			GPIO3_REG(0x24)
#define GPIO3_MASK_REG			GPIO3_REG(0x28)
#define GPIO3_ENABLE_REG		GPIO3_REG(0x2c)
#endif

#if 	(GPIO_INSTANCES >= 5)
#define GPIO4_DATA_REG			GPIO4_REG(0x00)
#define GPIO4_DIR_REG			GPIO4_REG(0x04)
#define GPIO4_IS_REG			GPIO4_REG(0x08)
#define GPIO4_IBE_REG			GPIO4_REG(0x0c)
#define GPIO4_IEV_REG			GPIO4_REG(0x10)
#define GPIO4_IE_REG			GPIO4_REG(0x14)
#define GPIO4_AFSEL_REG			GPIO4_REG(0x18)
#define GPIO4_RIS_REG			GPIO4_REG(0x1c)
#define GPIO4_MIS_REG			GPIO4_REG(0x20)
#define GPIO4_IC_REG			GPIO4_REG(0x24)
#define GPIO4_MASK_REG			GPIO4_REG(0x28)
#define GPIO4_ENABLE_REG		GPIO4_REG(0x2c)
#endif

/************************/
/* GPIO pins definition */
/************************/
#define GPIO(x)		(x)
#define IDC_CLK		GPIO(0)
#define IDC_DATA	GPIO(1)
#define SSI0_CLK	GPIO(2)
#define SSI0_MOSI	GPIO(3)
#define SSI0_MISO	GPIO(4)
#define SSI0_EN0	GPIO(5)
#define SSI0_EN1	GPIO(6)
#define VD_HVLD		GPIO(7)
#define VD_VVLD		GPIO(8)
#define STRIG0		GPIO(9)
#define STRIG1		GPIO(10)
#define TIMER0		GPIO(11)
#define TIMER1		GPIO(12)
#define TIMER2		GPIO(13)
#define UART0_TX	GPIO(14)
#define UART0_RX	GPIO(15)
#define VD_PWM0		GPIO(16)
/* There is no GPIO(17) */
#define VD_OUT6		GPIO(18)
#define VD_OUT7		GPIO(19)
#define VD_OUT8		GPIO(20)
#define VD_OUT9		GPIO(21)
#define VD_OUT10	GPIO(22)
#define VD_OUT11	GPIO(23)
#define VD_OUT12	GPIO(24)
#define VD_OUT13	GPIO(25)
#define VD_OUT14	GPIO(26)
#define VD_OUT15	GPIO(27)
#define SD12		GPIO(28)
#define SD13		GPIO(29)
#define SD14		GPIO(30)
#define SD15		GPIO(31)
#define CF_CD2		GPIO(32)
#define VD_SPL		GPIO(33)
#define VD_VCOMAC	GPIO(34)
#define IR_IN		GPIO(35)
#define STSCHG		GPIO(36)
#define CF_PULL_CTL	GPIO(37)
#define CF_PWRCYC	GPIO(38)
#define FL_WP		GPIO(39)
#define SC_A0		GPIO(40)
#define SC_A1		GPIO(41)
#define SC_A2		GPIO(42)
#define SC_A3		GPIO(43)
#define SC_A4		GPIO(44)
#define SC_B0		GPIO(45)
#define SC_B1		GPIO(46)
#define SC_B2		GPIO(47)
#define SC_B3		GPIO(48)
#define SC_B4		GPIO(49)
#define SC_C0		GPIO(50)
#define SC_C1		GPIO(51)
#define SC_C2		GPIO(52)
#define SC_C3		GPIO(53)
#define SC_C4		GPIO(54)
#define SD16		GPIO(55)
#define SD17		GPIO(56)
#define SD18		GPIO(57)
#define SD19		GPIO(58)

/* Tertiary function of GPIO pins */
#define A17		GPIO(18)
#define A18		GPIO(19)
#define A19		GPIO(20)
#define A20		GPIO(21)
#define A21		GPIO(22)
#define A22		GPIO(23)
#define I2S_SO_2	GPIO(40)
#define I2S_SI_2	GPIO(41)
#define I2S_WS_2	GPIO(42)
#define I2S_CLK_2	GPIO(43)
#define PWM1		GPIO(45)
#define PWM2		GPIO(46)
#define NAND_CE1	GPIO(47)
#define SSIO_EN2	GPIO(48)
#define SSIO_EN3	GPIO(49)
#if (CHIP_REV == A5L)
#define PWM3		GPIO(40)
#define PWM4		GPIO(41)
#else
#define PWM3		GPIO(50)
#define PWM4		GPIO(51)
#endif
#define NAND_CE2	GPIO(52)
#define NAND_CE3	GPIO(53)

/*------------------------------------------------------------------------*/
/* (CHIP_REV == A3) and later */
#define SD_0		GPIO(55)
#define SD_1		GPIO(56)
#define SD_2		GPIO(57)
#define SD_3		GPIO(58)
#define SD_4		GPIO(59)
#define SD_5		GPIO(60)
#define SD_6		GPIO(61)
#define SD_7		GPIO(62)

/*------------------------------------------------------------------------*/
/* (CHIP_REV == A2) and later */
#define SMIO_2		GPIO(64)
#define SMIO_3		GPIO(65)
#define SMIO_4		GPIO(66)
#define SMIO_5		GPIO(67)
#define SD1_CD		GPIO(67)
#define SMIO_6		GPIO(68)
#define SMIO_38		GPIO(69)
#define SMIO_39		GPIO(70)
#define SMIO_40		GPIO(71)
#define SMIO_41		GPIO(72)
#define SMIO_42		GPIO(73)
#define SMIO_43		GPIO(74)
#define SMIO_44		GPIO(75)
#define SMIO_45		GPIO(76)
#define I2S_WS		GPIO(77)
#define I2S_CLK		GPIO(78)
#define I2S_SO		GPIO(79)
#define I2S_SI		GPIO(80)
#define CLK_AU		GPIO(81)

/* Tertiary function of GPIO pins */
#define SSI_4_N		GPIO(77)
#define SSI_5_N		GPIO(78)
#define SSI_6_N		GPIO(79)
#define SSI_7_N		GPIO(80)

/*------------------------------------------------------------------------*/
/* (CHIP_REV == A3) and later */
#define I2S1_SO		GPIO(82)
#define I2S1_SI		GPIO(83)
#define I2S2_SO		GPIO(84)
#define I2S2_SI		GPIO(85)
#define IDC2CLK		GPIO(86)
#define IDC2DATA	GPIO(87)
#define SSI2CLK		GPIO(88)
#define SSI2MOSI	GPIO(89)
#define SSI2MISO	GPIO(90)
#define SSI2_0EN	GPIO(91)
#define SD_8		GPIO(92)
#define SD_9		GPIO(93)
#define SD_10		GPIO(94)
#define SD_11		GPIO(95)

/*------------------------------------------------------------------------*/
/* (CHIP_REV == A5) and later */
#if (CHIP_REV == A5) || (CHIP_REV == A5S) || (CHIP_REV == A6)
#define GMII_MDC_O	GPIO(96)
#define GMII_MDIO	GPIO(97)
#define PHY_TXEN_O 	GPIO(98)
#define PJY_TXER_O  	GPIO(99)
#define PHY_TXD_O_0	GPIO(100)
#define PHY_TXD_O_1   	GPIO(101)
#define CLK_TX_I	GPIO(104)
#define CLK_RX_I	GPIO(105)
#define NAND_FLASH_CE1 	GPIO(114)
#define NAND_FLASH_CE2	GPIO(115)
#define NAND_FLASH_CE3	GPIO(116)
#endif


/*------------------------------------------------------------------------*/
#if (CHIP_REV == A5L)
#define IDC_BUS_HDMI	GPIO(95)
#else
/* (CHIP_REV == A2S/A2M) */
#define IDC_BUS_HDMI	GPIO(87)
#endif
/*------------------------------------------------------------------------*/
#if (CHIP_REV == A5S)
/* (CHIP_REV == A5S)*/
#define IDC3_BUS_MUX	GPIO(36)

#define IDC3_DATA	GPIO(36)
#define IDC3_CLK	GPIO(37)

#define PHY_TXD_O_2	GPIO(18)
#define PHY_TXD_O_3	GPIO(19)
#define PHY_RXDV_I	GPIO(40)
#define PHY_RXER_I	GPIO(41)
#define PHY_CRS_I	GPIO(42)
#define PHY_COL_I	GPIO(43)
#define PHY_RXD_I_0	GPIO(44)
#define PHY_RXD_I_1	GPIO(47)
#define PHY_RXD_I_2	GPIO(48)
#define PHY_RXD_I_3	GPIO(49)
#define UART1_TX	GPIO(50)
#define UART1_RTS	GPIO(51)
#define UART1_RX	GPIO(52)
#define UART1_CTS	GPIO(53)
#define SSI_SLAVE_MISO	GPIO(50)
#define SSI_SLAVE_EN	GPIO(51)
#define SSI_SLAVE_MOSI	GPIO(52)
#define SSI_SLAVE_CLK	GPIO(53)

#elif (CHIP_REV == A7)
/* (CHIP_REV == A7)*/

#define VD1_OUT20	GPIO(28)
#define VD1_OUT21	GPIO(29)
#define VD1_OUT22	GPIO(30)
#define VD1_OUT23	GPIO(31)

#define IDC3_BUS_MUX	GPIO(17)
#define IDC3_DATA	GPIO(36)
#define IDC3_CLK	GPIO(37)

#define UART1_TX	GPIO(50)
#define UART1_RTS	GPIO(51)
#define UART1_RX	GPIO(52)
#define UART1_CTS	GPIO(53)
#define SSI_SLAVE_MISO	GPIO(50)
#define SSI_SLAVE_EN	GPIO(51)
#define SSI_SLAVE_MOSI	GPIO(52)
#define SSI_SLAVE_CLK	GPIO(53)

#define PHY_RXDV_I	GPIO(40)
#define PHY_RXER_I	GPIO(41)
#define PHY_CRS_I	GPIO(42)
#define PHY_COL_I	GPIO(43)
#define PHY_RXD_I_0	GPIO(44)
#define PHY_RXD_I_1	GPIO(47)
#define PHY_RXD_I_2	GPIO(48)
#define PHY_RXD_I_3	GPIO(49)
#define PHY_CLK_TX_I	GPIO(96)
#define PHY_CLK_RX_I	GPIO(97)
#define PHY_TXEN_O 	GPIO(98)
#define PHY_TXER_O  	GPIO(99)
#define PHY_GMII_MDO_O	GPIO(100)
#define PHY_GMII_MDC_O	GPIO(101)
#define PHY_TXD_O_0	GPIO(102)
#define PHY_TXD_O_1   	GPIO(103)
#define PHY_TXD_O_2	GPIO(104)
#define PHY_TXD_O_3	GPIO(105)
#define PHY2_CLK_TX_I	GPIO(106)
#define PHY2_CLK_RX_I	GPIO(107)
#define PHY2_TXEN_O 	GPIO(108)
#define PHY2_TXER_O  	GPIO(109)
#define PHY2_GMII_MDO_O	GPIO(110)
#define PHY2_GMII_MDC_O	GPIO(111)
#define PHY2_TXD_O_0	GPIO(112)
#define PHY2_TXD_O_1   	GPIO(113)
#define PHY2_TXD_O_2	GPIO(114)
#define PHY2_TXD_O_3	GPIO(115)
#define PHY2_RXD_I_0	GPIO(116)
#define PHY2_RXD_I_1	GPIO(117)
#define PHY2_RXD_I_2	GPIO(118)
#define PHY2_RXD_I_3	GPIO(119)
#define PHY2_COL_I	GPIO(120)
#define PHY2_CRS_I	GPIO(121)
#define PHY2_RXER_I	GPIO(122)
#define PHY2_RXDV_I	GPIO(123)

#define SSI3_EN0	GPIO(128)
#define SSI3_EN1	GPIO(129)
#define SSI3_EN2	GPIO(130)
#define SSI3_EN3	GPIO(131)
#define SSI3_EN4	GPIO(132)
#define SSI3_EN5	GPIO(133)
#define SSI3_EN6	GPIO(134)
#define SSI3_EN7	GPIO(135)
#define SSI3_MOSI	GPIO(136)
#define SSI3_CLK	GPIO(137)
#define SSI3_MISO	GPIO(138)

#define SC_D0		GPIO(139)
#define SC_D1		GPIO(140)
#define SC_D2		GPIO(141)
#define SC_D3		GPIO(142)
#define SC_D4		GPIO(143)

#elif (CHIP_REV == A5L)
#define IDC3_BUS_MUX	GPIO(63)
#else

/* (CHIP_REV == A5) and later */
#define PHY_TXD_O_2	GPIO(102)
#define PHY_TXD_O_3	GPIO(103)
#define PHY_RXDV_I	GPIO(106)
#define PHY_RXER_I	GPIO(107)
#define PHY_CRS_I	GPIO(108)
#define PHY_COL_I	GPIO(109)
#define PHY_RXD_I_0	GPIO(110)
#define PHY_RXD_I_1	GPIO(111)
#define PHY_RXD_I_2	GPIO(112)
#define PHY_RXD_I_3	GPIO(113)
#endif

#define VD1_OUT20	GPIO(28)
#define VD1_OUT21	GPIO(29)
#define VD1_OUT22	GPIO(30)
#define VD1_OUT23	GPIO(31)
#define VD1_OUT15	GPIO(92)
#define VD1_OUT16	GPIO(93)
#define VD1_OUT17	GPIO(94)
#define VD1_OUT18	GPIO(95)

#endif
