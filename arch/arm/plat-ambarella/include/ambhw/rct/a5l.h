/*
 * ambhw/rct/a5l.h
 *
 * History:
 *	2009/10/13 - [Allen Wang] created file
 *
 * Copyright (C) 2006-2009, Ambarella, Inc.
 */

#ifndef __AMBHW__RCT_A5L_H__
#define __AMBHW__RCT_A5L_H__

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/
#define RCT_DRAM_CLK_SRC_CORE		1
#define RCT_IDSP_CLK_SRC_CORE		1
#define RCT_ARM_CLK_SRC_CORE_DDR	1
#define RCT_SUPPORT_PLL_DDR		1
#define RCT_SUPPORT_PLL_IDSP		0
#define RCT_MAX_PLL_SCALER_RESOLUTION	16
#define RCT_MAX_DLL			2
#define RCT_SUPPORT_ADC16_CTRL		0
#define RCT_SUPPORT_PLL_HDMI		0
#define RCT_SUPPORT_SCALER_ARM		1
#define RCT_SUPPORT_SCALER_DRAM		1
#define RCT_SUPPORT_UNL_WDT_RST_REG	1 /* Use independent register */

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/
#define USE_PLL_CORE_FOR_DLL		1

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
#define ANA_PWR_REG			RCT_REG(0x50)
#define SOFT_RESET_REG			RCT_REG(0x68)
#define SOFT_OR_DLL_RESET_REG		RCT_REG(0x68)
#define FIO_RESET_REG			RCT_REG(0x74)
#define WDT_RST_L_REG			RCT_REG(0x78)
#define CLK_DEBOUNCE_REG		RCT_REG(0x80)

/* Chip version dependent registers */
#define PLL_CORE_CTRL_OFFSET		0x00
#define PLL_CORE_FRAC_OFFSET		0x04
#define SCALER_SD48_OFFSET		0x0c
#define PLL_VIDEO_CTRL_OFFSET		0x14
#define PLL_VIDEO_FRAC_OFFSET		0x18
#define SCALER_VIDEO_OFFSET             0x1c
#define SCALER_VIDEO_POST_OFFSET	0xa0
#define PLL_SENSOR_CTRL_OFFSET		0x24
#define PLL_SENSOR_FRAC_OFFSET		0x28
#define SCALER_SENSOR_POST_OFFSET	0x30
#define SYS_CONFIG_OFFSET		0x34
#define SCALER_SENSOR_PRE_OFFSET	0x4c
#define PLL_AUDIO_CTRL_OFFSET		0x54
#define PLL_AUDIO_FRAC_OFFSET		0x58
#define SCALER_AUDIO_OFFSET		0x5c
#define SCALER_AUDIO_PRE_OFFSET		0x60
#define SCALER_USB_OFFSET		0x7c
#define CG_PWM_OFFSET			0x84
#define USB_REFCLK_OFFSET		0x88
#define CKEN_VDSP_OFFSET		0x8c
#define DLL0_OFFSET			0x90
#define DLL1_OFFSET			0x94
#define SCALER_ADC_OFFSET		0x9c
#define CLK_REF_AU_EXTERNAL_OFFSET	0xa4
#define USE_EXTERNAL_CLK_AU_OFFSET	0xa8
#define CLK_REG_VIDEO_EXTERNAL_OFFSET	0xac
#define USE_EXTERNAL_VD_CLK_OFFSET	0xb0
#define USE_CLK_SI_4_AU_OFFSET		0xb4
#define USE_CLK_SI_4_VO_OFFSET		0xb8
#define USE_CLK_SI_INPUT_MODE_OFFSET	0xbc

#define PLL_CORE_CTRL_REG		RCT_REG(0x00)
#define PLL_CORE_FRAC_REG		RCT_REG(0x04)
#define SCALER_SD48_REG			RCT_REG(0x0c)
#define PLL_VIDEO_CTRL_REG		RCT_REG(0x14)
#define PLL_VIDEO_FRAC_REG		RCT_REG(0x18)
#define SCALER_VIDEO_REG                RCT_REG(0x1c)
#define PLL_SENSOR_CTRL_REG		RCT_REG(0x24)
#define PLL_SENSOR_FRAC_REG		RCT_REG(0x28)
#define SCALER_SENSOR_POST_REG		RCT_REG(0x30)
#define SYS_CONFIG_REG			RCT_REG(0x34)
#define SCALER_SENSOR_PRE_REG		RCT_REG(0x4c)
#define PLL_AUDIO_CTRL_REG		RCT_REG(0x54)
#define PLL_AUDIO_FRAC_REG		RCT_REG(0x58)
#define SCALER_AUDIO_REG		RCT_REG(0x5c)
#define SCALER_AUDIO_PRE_REG		RCT_REG(0x60)
#define SCALER_USB_REG			RCT_REG(0x7c)
#define CG_PWM_REG			RCT_REG(0x84)
#define USB_REFCLK_REG			RCT_REG(0x88)
#define CKEN_VDSP_REG			RCT_REG(0x8c)
#define DLL0_REG			RCT_REG(0x90)
#define DLL1_REG			RCT_REG(0x94)
#define SCALER_ADC_REG			RCT_REG(0x9c)
#define SCALER_VIDEO_POST_REG		RCT_REG(0xa0)
#define CLK_REF_AU_EXTERNAL_REG		RCT_REG(0xa4)
#define USE_EXTERNAL_CLK_AU_REG		RCT_REG(0xa8)
#define CLK_REF_VIDEO_EXTERNAL_REG	RCT_REG(0xac)
#define USE_EXTERNAL_VD_CLK_REG		RCT_REG(0xb0)
#define USE_CLK_SI_4_AU_REG		RCT_REG(0xb4)
#define USE_CLK_SI_4_VO_REG		RCT_REG(0xb8)
#define USE_CLK_SI_INPUT_MODE_REG	RCT_REG(0xbc)

/* PLL_XXX_CTRL_REG */
#define PLL_CTRL_VAL(intprog, sout, sdiv, valwe)	\
	(((intprog & 0x7f) << 24)	|	\
	 (PLL_FORCE_LOCK << 20)         |       \
	 ((sout & 0xf) << 16)		| 	\
	 ((sdiv & 0xf) << 12)		|	\
	 (valwe & 0xfff))

#define PLL_CTRL_INTPROG(x)	((x >> 24) & 0x7f)
#define PLL_CTRL_SOUT(x)	((x >> 16) & 0xf)
#define PLL_CTRL_SDIV(x)	((x >> 12) & 0xf)
#define PLL_CTRL_VALWE(x)	(x & 0xfff)

/* PLL_XXX_FRAC_REG */
#define PLL_FRAC_VAL(frac)	(frac & 0x7fffffff)

/* SCALER_XXX_PRE_REG */
#define SCALER_PRE_VAL(dcc, p, n)		\
	(((dcc & 0x1) << 24)		|	\
	 ((p & 0x1f) << 16)		|	\
	 (n & 0xffff))

#define SCALER_PRE_P(x)		((x >> 16) & 0x1f)
#define SCALER_PRE_N(x)		(x & 0xffff)
#define SCALER_POST(x)		(x & 0xf)

#define HDMI_CTRL_OFFSET	0x08
#define HDMI_CTRL_REG		RCT_REG(0x08)

#define PLL_LOCK_OFFSET		0x2c
#define PLL_LOCK_REG		RCT_REG(0x2c)

/* PLL_LOCK_REG */
#define PLL_LOCK_VIN	        (0x1 << 9)
#define PLL_LOCK_HDMI         	(0x1 << 8)
#define PLL_LOCK_AUDIO  	(0x1 << 7)
#define PLL_LOCK_CORE	        (0x1 << 6)
#define PLL_LOCK_DDR            (0x1 << 5)
#define PLL_LOCK_IDSP           (0x1 << 4)
#define PLL_LOCK_SENSOR         (0x1 << 3)
#define PLL_LOCK_USB            (0x1 << 2)
#define PLL_LOCK_VIDEO          (0x1 << 1)
#define PLL_LOCK_VIDEO2         (0x1 << 0)

#define SENSOR_PAD_CTRL_OFFSET		0x98
#define SENSOR_PAD_CTRL_REG		RCT_REG(0x98)

/* SENSOR_PAD_CTRL_REG */
#define SENSOR_PAD_CTRL_PMEMIO_A6	0x40
#define SENSOR_PAD_CTRL_PMEMIO_A2	0x20
#define SENSOR_PAD_CTRL_PMEMIO_DS	0x10
#define SENSOR_PAD_CTRL_PMEMIO_LVCMOS	0x08
#define SENSOR_PAD_CTRL_PMEMIO_S1	0x04
#define SENSOR_PAD_CTRL_PMEMIO_S0	0x02
#define SENSOR_PAD_CTRL_PMEMIO_PWD	0x01

#define SCALER_VIDEO2_OFFSET		0xc8
#define SCALER_VIDEO2_POST_OFFSET	0xcc
#define USE_CLK_SI_4_VO2_OFFSET		0xd0
#define USE_EXTERNAL_VD2_CLK_OFFSET	0xd4
#define CLK_REF_VIDEO2_EXTERNAL_OFFSET	0xd8

#define SCALER_VIDEO2_REG		RCT_REG(0xc8)
#define SCALER_VIDEO2_POST_REG		RCT_REG(0xcc)
#define USE_CLK_SI_4_VO2_REG		RCT_REG(0xd0)
#define USE_EXTERNAL_VD2_CLK_REG	RCT_REG(0xd4)
#define CLK_REF_VIDEO2_EXTERNAL_REG	RCT_REG(0xd8)

#define PLL_DDR_CTRL_OFFSET		0xdc
#define PLL_DDR_FRAC_OFFSET		0xe0
#define DDRIO_CALIB_OFFSET		0x160

#define PLL_DDR_CTRL_REG		RCT_REG(0xdc)
#define PLL_DDR_FRAC_REG		RCT_REG(0xe0)
#define DDRIO_CALIB_REG			RCT_REG(0x160)


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

#define DLL2_OFFSET			0xf0
#define DLL3_OFFSET			0xf4
#define DLL2_REG			RCT_REG(0xf0)
#define DLL3_REG			RCT_REG(0xf4)

#define PLL_CORE_CTRL2_OFFSET		0x100
#define PLL_CORE_CTRL3_OFFSET		0x104
#define PLL_DDR_CTRL2_OFFSET		0x110
#define PLL_DDR_CTRL3_OFFSET		0x114
#define PLL_SENSOR_CTRL2_OFFSET		0x11c
#define PLL_SENSOR_CTRL3_OFFSET		0x120
#define PLL_AUDIO_CTRL2_OFFSET		0x124
#define PLL_AUDIO_CTRL3_OFFSET		0x12c
#define PLL_VIDEO_CTRL2_OFFSET		0x130
#define PLL_VIDEO_CTRL3_OFFSET		0x134

#define PLL_CORE_CTRL2_REG		RCT_REG(0x100)
#define PLL_CORE_CTRL3_REG		RCT_REG(0x104)
#define PLL_DDR_CTRL2_REG		RCT_REG(0x110)
#define PLL_DDR_CTRL3_REG		RCT_REG(0x114)
#define PLL_SENSOR_CTRL2_REG		RCT_REG(0x11c)
#define PLL_SENSOR_CTRL3_REG		RCT_REG(0x120)
#define PLL_AUDIO_CTRL2_REG		RCT_REG(0x124)
#define PLL_AUDIO_CTRL3_REG		RCT_REG(0x12c)
#define PLL_VIDEO_CTRL2_REG		RCT_REG(0x130)
#define PLL_VIDEO_CTRL3_REG		RCT_REG(0x134)

#define SCALER_CORE_POST_OFFSET		0x118
#define SCALER_CORE_POST_REG		RCT_REG(0x118)

#define CG_DDR_CALIB_OFFSET		0x148
#define CG_DDR_CALIB_REG		RCT_REG(0x148)

#define DLL_CTRL_SEL_OFFSET		0x158
#define DLL_CTRL_SEL_REG		RCT_REG(0x158)

#define ADC16_CTRL_OFFSET		0x198
#define ADC16_CTRL_REG			RCT_REG(0x198)

#define CLK_REF_SSI_OFFSET		0x19c
#define CLK_REF_SSI_REG			RCT_REG(0x19c)

#define USE_LVDS_CLK_4_PWM_OFFSET	0x1c4
#define USE_LVDS_CLK_4_PWM_REG		RCT_REG(0x1c4)

#define MS_DELAY_CTRL_OFFSET		0x1d0
#define MS_DELAY_CTRL_REG		RCT_REG(0x1d0)
#define MS_DELAY_CTRL_OUT_SIG		0x07000000
#define MS_DELAY_CTRL_IN_SIG		0x00070000
#define MS_DELAY_CTRL_OUT_CLK		0x00003000
#define MS_DELAY_CTRL_OUT_PATH_CLK	0x00000700
#define MS_DELAY_CTRL_IN_CLK		0x00000030

#define USE_COMMON_VO_CLOCK_OFFSET	0x1d4
#define USE_COMMON_VO_CLOCK_REG		RCT_REG(0x1d4)

#define CLOCK_OBSV_OFFSET		0x1e0
#define CLOCK_OBSV_REG			RCT_REG(0x1e0)

/* CLOCK_OBSV_REG */
#define CLK_OBSV_ENABLE(x)		(4L << (x))
#define CLK_OBSV_PLL_OUT(x)		((x) & 0xf)
#define CLK_OBSV_PLL_OUT_CORE		0x0
#define CLK_OBSV_PLL_OUT_VIDEO		0x1
#define CLK_OBSV_PLL_OUT_SENSOR		0x2
#define CLK_OBSV_PLL_OUT_AUDIO		0x3
#define CLK_OBSV_PLL_OUT_DDR		0x6

#define DISABLE_EXT_BYPASS_OFFSET	0x1e4
#define DISABLE_EXT_BYPASS_REG		RCT_REG(0x1e4)

#define SCALER_ARM_ASYNC_OFFSET		0x1f0
#define SCALER_ARM_ASYNC_REG		RCT_REG(0x1f0)

/* IOCTRL */
#define IOCTRL_MISC1_OFFSET		0x1fc
#define IOCTRL_MISC2_OFFSET		0x200
#define IOCTRL_SD_OFFSET		0x204
#define IOCTRL_SMIO_OFFSET		0x208
#define IOCTRL_VD0_OFFSET		0x20c
#define IOCTRL_SENSOR_OFFSET		0x214
#define IOCTRL_MISC1_REG		RCT_REG(0x1fc)
#define IOCTRL_MISC2_REG		RCT_REG(0x200)
#define IOCTRL_SD_REG			RCT_REG(0x204)
#define IOCTRL_SMIO_REG			RCT_REG(0x208)
#define IOCTRL_VD0_REG			RCT_REG(0x20c)
#define IOCTRL_SENSOR_REG		RCT_REG(0x214)

#define AHB_MISC_EN_OFFSET		0x21c
#define AHB_MISC_EN_REG			RCT_REG(0x21c)

#define CG_DDR_INIT_OFFSET		0x220
#define DDR_DIV_RST_OFFSET		0x224
#define DDRC_IDSP_RESET_OFFSET		0x228
#define CG_DDR_INIT_REG			RCT_REG(0x220)
#define DDR_DIV_RST_REG			RCT_REG(0x224)
#define DDRC_IDSP_RESET_REG		RCT_REG(0x228)

#define USE_PLL_DDR_FOR_ARM_OFFSET		0x230
#define USE_PLL_CORE_FOR_DDR_OFFSET		0x234
#define USE_PLL_DDR_FOR_VO2_OFFSET		0x238
#define USE_PLL_CORE_VCO_FOR_CLK_CORE_OFFSET	0x248

#define USE_PLL_DDR_FOR_ARM_REG			RCT_REG(0x230)
#define USE_PLL_CORE_FOR_DDR_REG		RCT_REG(0x234)
#define USE_PLL_DDR_FOR_VO2_REG			RCT_REG(0x238)
#define USE_PLL_CORE_VCO_FOR_CLK_CORE_REG	RCT_REG(0x248)

#define SCALER_DDRIO_POST_OFFSET	0x23c
#define SCALER_DDRIO_POST_REG		RCT_REG(0x23c)

#define UNLOCK_WDT_RST_L_OFFSET		0x240
#define UNLOCK_WDT_RST_L_REG		RCT_REG(0x240)

#define HDMI_PHY_DIVIDER_SEL_OFFSET	0x244
#define HDMI_PHY_DIVIDER_SEL_REG	RCT_REG(0x244)

/* HDMI_PHY_DIVIDER_SEL_REG */
#define HDMI_PHY_DIV_SEL_1X		0x0
#define HDMI_PHY_DIV_SEL_2X		0x1
#define HDMI_PHY_DIV_SEL_4X		0x2

/* ANA_PWR_REG */
#define ANA_PWR_DLL_POWER_DOWN		0x80
#define ANA_PWR_PMEMIO_S1		0x40
#define ANA_PWR_POWER_DOWN		0x20
#define ANA_PWR_NEVER_SUSPEND_USBP	0x04
#define ANA_PWR_USB_SUSPEND		0x02

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

/* CKEN_VDSP_REG */
#define CKEN_VDSP_SMEM			0x08
#define CKEN_VDSP_CODE			0x04
#define CKEN_VDSP_TSFM			0x02
#define CKEN_VDSP_MEMD			0x01

/*
 * SYS_CONFIG_REG
 */
#define SYS_CONFIG_SELECT_ENET		0x00008000
#define SYS_CONFIG_EMA_SEL		0x00001000
#define SYS_CONFIG_SD_BOOT		0x00000800
#define SYS_CONFIG_RDY_PL		0x00002000
	/*
	 * sets polarity of hif_rdy signal
	 * 0 -> hif_rdy active low
	 * 1 -> hif_rdy active high
	 */
#define SYS_CONFIG_HIF_TYPE		0x00001000
#define SYS_CONFIG_SPI_BOOT		0x00010000
#define SYS_CONFIG_FLASH_BOOT		0x00000400

	/*
	 * 0 : Boot with USB (ARM code on internal ROM).
	 * 1 : Boot using FIO controller (NAND or NOR flash).
	 */
#define SYS_CONFIG_FAST_BOOT		0x00000200
#define SYS_CONFIG_BOOT_BYPASS		0x00000100
	/*
	 * 0: ARM waits for FIO 2KB code fetch before execution.
	 * 1: ARM does not wait for FIO to fetch 2KB code before execution.
	 */
#define SYS_CONFIG_NAND_BOOT_BYPASS	0x00000100
#define SYS_CONFIG_RMII_SEL		0x00000080
#define SYS_CONFIG_NAND_READ_CONFIRM	0x00000040
#define SYS_CONFIG_NAND_FLASH_PAGE	0x00000020	/* 0 for 512 bytes */
#define SYS_CONFIG_CORE_283MHZ		0x0000000e
#define SYS_CONFIG_CORE_270MHZ		0x0000000c
#define SYS_CONFIG_CORE_256MHZ		0x0000000a
#define SYS_CONFIG_CORE_243MHZ		0x00000008
#define SYS_CONFIG_CORE_230MHZ		0x00000006
#define SYS_CONFIG_CORE_182MHZ		0x00000004
#define SYS_CONFIG_CORE_135MHZ		0x00000002
#define SYS_CONFIG_CORE_216MHZ		0x00000000
#define SYS_CONFIG_FLASH_NOR		0x00000001
#define SYS_CONFIG_FLASH_NAND		0x00000000
#define SYS_CONFIG_BOOTMEDIA		0x00000001 /* 0: NAND, 1: NOR */

/*
 * Core values
 */
#define PLL_CORE_VCO_FREQ_720MHZ	720000000 	/* VCO freq = 720 MHz */
#define PLL_CORE_VCO_FREQ_960MHZ	960000000 	/* VCO freq = 960 MHz */
#define PLL_CORE_VCO_FREQ_1200MHZ	1200000000 	/* VCO freq = 1200 MHz */

#if	defined(FIX_CORE_180MHZ)

#define PLL_CORE_VCO_OUT_FREQ_HZ	PLL_CORE_VCO_FREQ_720MHZ

#if (PLL_CORE_VCO_OUT_FREQ_HZ == PLL_CORE_VCO_FREQ_720MHZ)

/* VCO freq = 720 MHz */
#define PLL_CORE_180MHZ_VAL		0x09132100
#define PLL_CORE_240MHZ_VAL		0x09122100
#define PLL_CORE_SCALER_VAL_180MHZ	0x4
#define PLL_CORE_SCALER_VAL_240MHZ	0x3
#define PLL_ARM_SCALER_VAL		0x2 /* 360 MHz*/  

/* DDRIO post scaler */
#if	defined(FIX_DRAM_324MHZ)
#define RCT_DDRIO_POST_SCALER_VAL	0x1
#elif	defined(FIX_DRAM_162MHZ)
#define RCT_DDRIO_POST_SCALER_VAL	0x2
#endif

#endif
#endif

#if	defined(FIX_CORE_240MHZ)

#define PLL_CORE_VCO_OUT_FREQ_HZ	PLL_CORE_VCO_FREQ_960MHZ

#if (PLL_CORE_VCO_OUT_FREQ_HZ == PLL_CORE_VCO_FREQ_960MHZ)

/* VCO freq = 960 MHz */
#define PLL_CORE_240MHZ_VAL		0x09133100
#define PLL_CORE_SCALER_VAL_240MHZ	0x4
#define PLL_ARM_SCALER_VAL		0x2 /* 480 MHZ */

/* DDRIO post scaler */
#if	defined(FIX_DRAM_240MHZ)
#define RCT_DDRIO_POST_SCALER_VAL	0x2
#else
#define RCT_DDRIO_POST_SCALER_VAL	0x1
#endif

#endif

#if (PLL_CORE_VCO_OUT_FREQ_HZ == PLL_CORE_VCO_FREQ_1200MHZ)

/* VCO freq = 1200 MHz */
#define PLL_CORE_240MHZ_VAL		0x09144100
#define PLL_CORE_SCALER_VAL_240MHZ	0x5
#define PLL_ARM_SCALER_VAL		0x3

/* DDRIO post scaler */
#if	defined(FIX_DRAM_300MHZ)
#define RCT_DDRIO_POST_SCALER_VAL	0x2
#endif

#endif

#endif

#if	defined(FIX_CORE_108MHZ)
#define	EXPECT_PLL_CORE_VAL		PLL_CORE_108MHZ_VAL
#define PLL_CORE_SCALER_VAL		PLL_CORE_SCALER_VAL_108MHZ
#define USE_PLL_CORE_FOR_DDR 		1 
#elif	defined(FIX_CORE_180MHZ)
#define	EXPECT_PLL_CORE_VAL		PLL_CORE_180MHZ_VAL
#define PLL_CORE_SCALER_VAL		PLL_CORE_SCALER_VAL_180MHZ
#define USE_PLL_CORE_FOR_DDR 		0 
#elif	defined(FIX_CORE_216MHZ)
#define EXPECT_PLL_CORE_VAL		PLL_CORE_216MHZ_VAL
#define PLL_CORE_SCALER_VAL		PLL_CORE_SCALER_VAL_216MHZ
#define USE_PLL_CORE_FOR_DDR 		1
#elif	defined(FIX_CORE_240MHZ)
#define EXPECT_PLL_CORE_VAL		PLL_CORE_240MHZ_VAL
#define PLL_CORE_SCALER_VAL		PLL_CORE_SCALER_VAL_240MHZ
#define USE_PLL_CORE_FOR_DDR 		0 
#else
#define EXPECT_PLL_CORE_VAL		0xffffffff
#define PLL_CORE_SCALER_VAL 		0x2	
#define PLL_ARM_SCALER_VAL		0x2
#define RCT_DDRIO_POST_SCALER_VAL	0x2	
#define USE_PLL_CORE_FOR_DDR 		1 
#endif

/* DRAM control */
/* cg_ddr needed only for lpddr2*/
#define RCT_CG_DDR_INIT			0x0000010a	/* FIXME */
#define RCT_CG_DDR_NORMAL		0x0000000a	/* FIXME */
#define DRAM_DQS_SYNC_VAL		0x00000040	/* FIXME */
#define DRAM_ZQ_CALIB			0x00000061

/* DRAM control */

#if	defined(FIX_DRAM_162MHZ)
#define DRAM_DDRC_REQ_CREDIT_VAL	0x04
//#define DRAM_DDRC_REQ_CREDIT_VAL	0x0
#elif	defined(FIX_DRAM_240MHZ)
#define DRAM_DDRC_REQ_CREDIT_VAL	0x04
//#define DRAM_DDRC_REQ_CREDIT_VAL	0x0
#else
#define DRAM_DDRC_REQ_CREDIT_VAL	0x100
#endif

/* SCALER_SD48_REG */
#define SD48_DUTY_CYCLE_CTRL	0x1000000
#define SD48_DELAY_MUX		0x0c00000
#define SD48_PRIMARY_DIV	0x01f0000
#define SD48_INTEGER_DIV	0x000ffff

//#ifndef __ASSEMBLER__

/*
 * RCT/PLL functions
 */
//extern u32 get_apb_bus_freq_hz(void);
//extern u32 get_ahb_bus_freq_hz(void);
//extern u32 get_core_bus_freq_hz(void);

//#endif  /* __ASSEMBLER__ */

#endif
