/*
 * ambhw/rct/a1.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2008/02/19 - [Allen Wang] changed to use capabilities and chip ID
 *	2008/05/07 - [Allen Wang] moved to separate files at directory 'rct' 
 *
 * Copyright (C) 2006-2008, Ambarella, Inc.
 */

#ifndef __AMBHW__RCT_A1_H__
#define __AMBHW__RCT_A1_H__

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/
#define RCT_DRAM_CLK_SRC_CORE		0
#define RCT_IDSP_CLK_SRC_CORE		0
#define RCT_SUPPORT_PLL_DDR		0
#define RCT_SUPPORT_PLL_IDSP		0
#define RCT_MAX_DLL			0
#define RCT_SUPPORT_ADC16_CTRL		0
#define RCT_SUPPORT_PLL_HDMI		0

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

/*
 * Register definitions according to chip architecture
 */

/******************************************/
/* RCT (System Boot-up and Configuration) */
/******************************************/

/* Chip version independent registers */
#define CG_UART_OFFSET			0x38
#define CG_SSI_OFFSET			0x3c
#define CG_MOTOR_OFFSET			0x40
#define CG_IR_OFFSET			0x44
#define CG_HOST_OFFSET			0x48
#define ANA_PWR_OFFSET			0x50
#define SOFT_RESET_OFFSET		0x68
#define SOFT_OR_DLL_RESET_OFFSET	0X68
#define FIO_RESET_OFFSET		0x74
#define WDT_RST_L_OFFSET		0x78
#define CLK_DEBOUNCE_OFFSET		0x80

#define CG_UART_REG			RCT_REG(0x38)
#define CG_SSI_REG			RCT_REG(0x3c)
#define CG_MOTOR_REG			RCT_REG(0x40)
#define CG_IR_REG			RCT_REG(0x44)
#define CG_HOST_REG			RCT_REG(0x48)
#define ANA_PWR_REG			RCT_REG(0x50)
#define SOFT_RESET_REG			RCT_REG(0x68)
#define SOFT_OR_DLL_RESET_REG		RCT_REG(0x68)
#define FIO_RESET_REG			RCT_REG(0x74)
#define WDT_RST_L_REG			RCT_REG(0x78)
#define CLK_DEBOUNCE_REG		RCT_REG(0x80)

/* Chip version dependent registers */
#define PLL_CORE_OFFSET			0x00
#define CG_CORE_OFFSET			0x04
#define PLL_VO_OFFSET			0x08
#define CG_VO_OFFSET			0x1c
#define PLL_SO_OFFSET			0x20
#define CG_SO_OFFSET			0x24
#define PLL_AU1_OFFSET			0x28
#define PLL_AU2_OFFSET			0x2c
#define CG_AU_OFFSET			0x30
#define CG_ADC_OFFSET			0x34
#define SYS_CONFIG_OFFSET		0x4c
#define CG_SETTING_CHANGED_OFFSET	0x60
#define SD_CLK_EN_OFFSET		0x64
#define CG_SD_OFFSET			0x70
#define CG_PWM_REG_OFFSET		0x84
#define USB_CLK_OFFSET			0x88

#define PLL_CORE_REG			RCT_REG(0x00)
#define CG_CORE_REG			RCT_REG(0x04)
#define PLL_VO_REG			RCT_REG(0x08)
#define CG_VO_REG			RCT_REG(0x1c)
#define PLL_SO_REG			RCT_REG(0x20)
#define CG_SO_REG			RCT_REG(0x24)
#define PLL_AU1_REG			RCT_REG(0x28)
#define PLL_AU2_REG			RCT_REG(0x2c)
#define CG_AU_REG			RCT_REG(0x30)
#define CG_ADC_REG			RCT_REG(0x34)
#define SYS_CONFIG_REG			RCT_REG(0x4c)
#define CG_SETTING_CHANGED_REG		RCT_REG(0x60)
#define SD_CLK_EN_REG			RCT_REG(0x64)
#define CG_SD_REG			RCT_REG(0x70)
#define CG_PWM_REG			RCT_REG(0x84)
#define USB_CLK_REG			RCT_REG(0x88)

/* CG_CORE_REG */
#define CG_CORE_ED_108M			0x04
#define CG_CORE_ED_SMEM			0x02
#define CG_CORE_CG_BYP			0x01

/* CG_SETTING_CHANGED_REG */
#define CG_SETTING_CHANGED_AUDIO_PLL2	0x1000
#define CG_SETTING_CHANGED_AUDIO_PLL1	0x0800
#define CG_SETTING_CHANGED_SENSOR	0x0400
#define CG_SETTING_CHANGED_VIDEO	0x0200
#define CG_SETTING_CHANGED_CORE_PLL	0x0100
#define CG_SETTING_CHANGED_IR		0x0080
#define CG_SETTING_CHANGED_MOTOR	0x0040
#define CG_SETTING_CHANGED_SSI_SD	0x0020
#define CG_SETTING_CHANGED_UART		0x0010
#define CG_SETTING_CHANGED_AUDIO	0x0008
#define CG_SETTING_CHANGED_SENSOR0	0x0004
#define CG_SETTING_CHANGED_VIDEO0	0x0002
#define CG_SETTING_CHANGED_CORE_CLK	0x0001

/* DLL0_REG */
#define DLL0_VAL(madj_0,adj_0_0,adj_0_1)		\
	(((madj_0) & 0xff)           |			\
	 (((adj_0_0) & 0xff) << 8)   |			\
	 (((adj_0_1) & 0xff) << 16)) |

/* DLL1_REG */
#define DLL1_VAL(madj_1,adj_1_0,adj_1_1)		\
	(((madj_1) & 0xff)           |			\
	 (((adj_1_0) & 0xff) << 8)   |			\
	 (((adj_1_1) & 0xff) << 16)) |

#define PLL_CORE_CTRL3_OFFSET		0x104
#define PLL_IDSP_CTRL3_OFFSET		0x10c
#define PLL_DDR_CTRL3_OFFSET		0x114

#define PLL_CORE_CTRL3_REG		RCT_REG(0x104)
#define PLL_IDSP_CTRL3_REG		RCT_REG(0x10c)
#define PLL_DDR_CTRL3_REG		RCT_REG(0x114)

/* ANA_PWR_REG */
#define USB_ON_BIT			0x02

/* FIO_RESET_REG */
#define FIO_RESET_FIO_RST		0x00000008
#define FIO_RESET_CF_RST		0x00000004
#define FIO_RESET_XD_RST		0x00000002
#define FIO_RESET_FLASH_RST		0x00000001

/* USBP_REFCLK_REG */
#define USBP_REFCLK_TX_TUNE_11		0x0c00
#define USBP_REFCLK_TX_TUNE_10		0x0800
#define USBP_REFCLK_TX_TUNE_01		0x0400
#define USBP_REFCLK_TX_TUNE_00		0x0000
#define USBP_REFCLK_WAK_PULLDOWN_EN	0x0200
#define USBP_REFCLK_DP_PULLUP_ESD	0x0100
#define USBP_REFCLK_ESD_TEST_MODE	0x0080
#define USBP_REFCLK_COMMON_ON_N		0x0040
#define USBP_REFCLK_CLK48M_SEL		0x0020
#define USBP_REFCLK_CLK_SEL_11		0x000c
#define USBP_REFCLK_CLK_SEL_10		0x0008
#define USBP_REFCLK_CLK_SEL_01		0x0004
#define USBP_REFCLK_CLK_SEL_00		0x0000
#define USBP_REFCLK_XO_EXT_CLK_ENBN	0x0002
#define USBP_REFCLK_XO_REFCLK_ENB	0x0001


/*
 * SYS_CONFIG_REG
 */
#define SYS_CONFIG_JTAG_SEL		0x00000400
#define SYS_CONFIG_FAST_BOOT		0x00000200
#define SYS_CONFIG_BOOT_BYPASS		0x00000100
	/*
	 * 0: ARM waits for FIO 2KB code fetch before execution.
	 * 1: ARM does not wait for FIO to fetch 2KB code before execution.
	 */
#define SYS_CONFIG_SMEM_IMEM_HALF	0x00000080
#define SYS_CONFIG_NAND_READ_CONFIRM	0x00000040 /* 0: use, 1: doesn't use */
#define SYS_CONFIG_NAND_FLASH_PAGE	0x00000020 /* 0: 512Byte, 1: 2K Byte */
#define SYS_CONFIG_NAND_CF_TRUE_IDE	0x00000010
#define SYS_CONFIG_POWER_UP_BIST	0x00000008
#define SYS_CONFIG_CORE_135MHZ		0x00000006
#define SYS_CONFIG_CORE_162MHZ		0x00000004
#define SYS_CONFIG_CORE_182MHZ		0x00000002
#define SYS_CONFIG_CORE_216MHZ		0x00000000
#define SYS_CONFIG_BOOTMEDIA		0x00000001 /* 0: NAND, 1: NOR */


/*
 * Core values
 */
#define PLL_CORE_135MHZ_VAL	0x0022
#define PLL_CORE_162MHZ_VAL	0x002a
#define PLL_CORE_182MHZ_VAL	0x0166
#define PLL_CORE_202MHZ_VAL	0x0036
#define PLL_CORE_216MHZ_VAL	0x003a
#define PLL_CORE_229MHZ_VAL	0x003e
#define PLL_CORE_243MHZ_VAL	0x0042
#define PLL_CORE_250MHZ_VAL	0x018e
#define PLL_CORE_256MHZ_VAL	0x0046
#define PLL_CORE_263MHZ_VAL	0xdead  /* FIXME */
#define PLL_CORE_270MHZ_VAL	0x004a
#define PLL_CORE_283MHZ_VAL	0x004e
#define PLL_CORE_290MHZ_VAL	0x0052

#if defined(CONFIG_REF_CLK_48MHZ)
/* Based on 48MHz reference clock */
#define PLL_CORE_132MHZ_VAL	0x0b02fb0a
#define PLL_CORE_144MHZ_VAL	0x0c02fb0a
#define PLL_CORE_156MHZ_VAL	0x0d02fb0a
#define PLL_CORE_168MHZ_VAL	0x0e02fb0a
#define PLL_CORE_180MHZ_VAL	0x0f02fb0a
#define PLL_CORE_192MHZ_VAL	0x1002fb0a
#define PLL_CORE_204MHZ_VAL	0x1102fb0a
#define PLL_CORE_216MHZ_VAL	0x1202fb0a
#define PLL_CORE_228MHZ_VAL	0x1302fb0a
#define PLL_CORE_240MHZ_VAL	0x1402fa0a
#endif

#if	defined(FIX_CORE_135MHZ)
#define	EXPECT_PLL_CORE_VAL	PLL_CORE_135MHZ_VAL
#elif	defined(FIX_CORE_162MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_162MHZ_VAL
#elif	defined(FIX_CORE_176MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_176MHZ_VAL
#elif	defined(FIX_CORE_182MHZ)
#define	EXPECT_PLL_CORE_VAL	PLL_CORE_182MHZ_VAL
#elif	defined(FIX_CORE_189MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_189MHZ_VAL
#elif	defined(FIX_CORE_202MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_202MHZ_VAL
#elif	defined(FIX_CORE_216MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_216MHZ_VAL
#elif	defined(FIX_CORE_229MHZ)
#define	EXPECT_PLL_CORE_VAL	PLL_CORE_229MHZ_VAL
#elif	defined(FIX_CORE_243MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_243MHZ_VAL
#elif	defined(FIX_CORE_250MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_250MHZ_VAL
#elif	defined(FIX_CORE_256MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_256MHZ_VAL
#elif	defined(FIX_CORE_263MHZ)
#define	EXPECT_PLL_CORE_VAL	PLL_CORE_263MHZ_VAL
#elif	defined(FIX_CORE_270MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_270MHZ_VAL
#elif	defined(FIX_CORE_283MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_283MHZ_VAL
#elif	defined(FIX_CORE_290MHZ)
#define EXPECT_PLL_CORE_VAL	PLL_CORE_290MHZ_VAL
#else
#define EXPECT_PLL_CORE_VAL	0xffff
#endif

#define EXPECT_PLL_IDSP_VAL	0xffff

/* SCALER_SD48_REG */
#define SD48_DUTY_CYCLE_CTRL	0x10000
#define SD48_PRIMARY_DIV	0x0ff00
#define SD48_INTEGER_DIV	0x000ff

#endif
