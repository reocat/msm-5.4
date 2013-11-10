/*
 * arch/arm/plat-ambarella/include/plat/rct.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2013, Ambarella, Inc.
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

#ifndef __PLAT_AMBARELLA_RCT_H__
#define __PLAT_AMBARELLA_RCT_H__

/* ==========================================================================*/
#define RCT_OFFSET			0x170000
#if (CHIP_REV == A8) || (CHIP_REV == S2L)
#define RCT_BASE			(DBGBUS_BASE + RCT_OFFSET)
#else
#define RCT_BASE			(APB_BASE + RCT_OFFSET)
#endif
#define RCT_REG(x)			(RCT_BASE + (x))

#if (CHIP_REV == S2) || (CHIP_REV == S2L)
#define AHB_SCRATCHPAD_OFFSET		0x1B000
#define AHB_SECURE_OFFSET		0x1D000
#else
#define AHB_SCRATCHPAD_OFFSET		0x19000
#define AHB_SECURE_OFFSET		0x1F000
#endif
#define AHB_SCRATCHPAD_BASE		(AHB_BASE + AHB_SCRATCHPAD_OFFSET)
#define AHB_SECURE_BASE			(AHB_BASE + AHB_SECURE_OFFSET)
#define AHB_SCRATCHPAD_REG(x)		(AHB_SCRATCHPAD_BASE + (x))
#define AHB_SECURE_REG(x)		(AHB_SECURE_BASE + (x))

/* ==========================================================================*/
#define PLL_LOCK_OFFSET			0x2C
#define SOFT_OR_DLL_RESET_OFFSET	0x68

#define PLL_LOCK_REG			RCT_REG(PLL_LOCK_OFFSET)
#define SOFT_OR_DLL_RESET_REG		RCT_REG(SOFT_OR_DLL_RESET_OFFSET)

/* ==========================================================================*/
#define FIO_RESET_OFFSET		0x74
#define FIO_RESET_REG			RCT_REG(FIO_RESET_OFFSET)
#define FIO_RESET_FIO_RST		0x00000008
#define FIO_RESET_CF_RST		0x00000004
#define FIO_RESET_XD_RST		0x00000002
#define FIO_RESET_FLASH_RST		0x00000001

/* ==========================================================================*/
#define USB_REFCLK_OFFSET		0x88

#define USB_REFCLK_REG			RCT_REG(USB_REFCLK_OFFSET)

/* ==========================================================================*/
#define PLL_AUDIO_CTRL_OFFSET		0x54
#define PLL_AUDIO_FRAC_OFFSET		0x58
#define SCALER_AUDIO_POST_OFFSET	0x5C
#define SCALER_AUDIO_PRE_OFFSET		0x60
#define PLL_AUDIO_CTRL2_OFFSET		0x124
#define PLL_AUDIO_CTRL3_OFFSET		0x12c

#define PLL_AUDIO_CTRL_REG		RCT_REG(PLL_AUDIO_CTRL_OFFSET)
#define PLL_AUDIO_FRAC_REG		RCT_REG(PLL_AUDIO_FRAC_OFFSET)
#define SCALER_AUDIO_POST_REG		RCT_REG(SCALER_AUDIO_POST_OFFSET)
#define SCALER_AUDIO_PRE_REG		RCT_REG(SCALER_AUDIO_PRE_OFFSET)
#define PLL_AUDIO_CTRL2_REG		RCT_REG(PLL_AUDIO_CTRL2_OFFSET)
#define PLL_AUDIO_CTRL3_REG		RCT_REG(PLL_AUDIO_CTRL3_OFFSET)

/* ==========================================================================*/
#define ANA_PWR_OFFSET			0x50
#define ANA_PWR_REG			RCT_REG(ANA_PWR_OFFSET)
#define ANA_PWR_POWER_DOWN		0x0020

#define SYS_CONFIG_OFFSET		0x34
#define SYS_CONFIG_REG			RCT_REG(SYS_CONFIG_OFFSET)

#define WDT_RST_L_OFFSET		0x78
#define UNLOCK_WDT_RST_L_OFFSET		0x260
#define WDT_RST_L_REG			RCT_REG(WDT_RST_L_OFFSET)
#define UNLOCK_WDT_RST_L_REG		RCT_REG(UNLOCK_WDT_RST_L_OFFSET)

/* ==========================================================================*/
#define CG_UART_OFFSET			0x38
#define CG_SSI_OFFSET			0x3C
#define CG_MOTOR_OFFSET			0x40
#define CG_IR_OFFSET			0x44
#define CG_HOST_OFFSET			0x48
#define CG_PWM_OFFSET			0x84
#define CG_SSI2_OFFSET			0xEC
#define CG_SSI_AHB_OFFSET		0x2CC

#define CG_UART_REG			RCT_REG(CG_UART_OFFSET)
#define CG_SSI_REG			RCT_REG(CG_SSI_OFFSET)
#define CG_MOTOR_REG			RCT_REG(CG_MOTOR_OFFSET)
#define CG_IR_REG			RCT_REG(CG_IR_OFFSET)
#define CG_HOST_REG			RCT_REG(CG_HOST_OFFSET)
#define CG_PWM_REG			RCT_REG(CG_PWM_OFFSET)
#define CG_SSI2_REG			RCT_REG(CG_SSI2_OFFSET)
#define CG_SSI_AHB_REG			RCT_REG(CG_SSI_AHB_OFFSET)

/* ==========================================================================*/
#define PLL_CORE_CTRL_OFFSET		0x00
#define PLL_CORE_FRAC_OFFSET		0x04
#define PLL_CORE_CTRL2_OFFSET		0x100
#define PLL_CORE_CTRL3_OFFSET		0x104
#define SCALER_CORE_POST_OFFSET		0x118

#define PLL_CORE_CTRL_REG		RCT_REG(PLL_CORE_CTRL_OFFSET)
#define PLL_CORE_FRAC_REG		RCT_REG(PLL_CORE_FRAC_OFFSET)
#define PLL_CORE_CTRL2_REG		RCT_REG(PLL_CORE_CTRL2_OFFSET)
#define PLL_CORE_CTRL3_REG		RCT_REG(PLL_CORE_CTRL3_OFFSET)
#define SCALER_CORE_POST_REG		RCT_REG(SCALER_CORE_POST_OFFSET)

/* ==========================================================================*/
#define PLL_IDSP_CTRL_OFFSET		0xE4
#define PLL_IDSP_FRAC_OFFSET		0xE8
#define PLL_IDSP_CTRL2_OFFSET		0x108
#define PLL_IDSP_CTRL3_OFFSET		0x10C

#define PLL_IDSP_CTRL_REG		RCT_REG(PLL_IDSP_CTRL_OFFSET)
#define PLL_IDSP_FRAC_REG		RCT_REG(PLL_IDSP_FRAC_OFFSET)
#define PLL_IDSP_CTRL2_REG		RCT_REG(PLL_IDSP_CTRL2_OFFSET)
#define PLL_IDSP_CTRL3_REG		RCT_REG(PLL_IDSP_CTRL3_OFFSET)

/* ==========================================================================*/
#define PLL_DDR_CTRL_OFFSET		0xDC
#define PLL_DDR_FRAC_OFFSET		0xE0
#define PLL_DDR_CTRL2_OFFSET		0x110
#define PLL_DDR_CTRL3_OFFSET		0x114

#define PLL_DDR_CTRL_REG		RCT_REG(PLL_DDR_CTRL_OFFSET)
#define PLL_DDR_FRAC_REG		RCT_REG(PLL_DDR_FRAC_OFFSET)
#define PLL_DDR_CTRL2_REG		RCT_REG(PLL_DDR_CTRL2_OFFSET)
#define PLL_DDR_CTRL3_REG		RCT_REG(PLL_DDR_CTRL3_OFFSET)

/* ==========================================================================*/
#define SCALER_ARM_ASYNC_OFFSET		0x1F0

#define SCALER_ARM_ASYNC_REG		RCT_REG(SCALER_ARM_ASYNC_OFFSET)

/* ==========================================================================*/
#if (CHIP_REV == S2L)
#define PLL_CORTEX_CTRL_OFFSET		0x264
#define PLL_CORTEX_FRAC_OFFSET		0x268
#define PLL_CORTEX_CTRL2_OFFSET		0x26C
#define PLL_CORTEX_CTRL3_OFFSET		0x270
#else
#define PLL_CORTEX_CTRL_OFFSET		0x2B0
#define PLL_CORTEX_FRAC_OFFSET		0x2B4
#define PLL_CORTEX_CTRL2_OFFSET		0x2B8
#define PLL_CORTEX_CTRL3_OFFSET		0x2BC
#endif

#define PLL_CORTEX_CTRL_REG		RCT_REG(PLL_CORTEX_CTRL_OFFSET)
#define PLL_CORTEX_FRAC_REG		RCT_REG(PLL_CORTEX_FRAC_OFFSET)
#define PLL_CORTEX_CTRL2_REG		RCT_REG(PLL_CORTEX_CTRL2_OFFSET)
#define PLL_CORTEX_CTRL3_REG		RCT_REG(PLL_CORTEX_CTRL3_OFFSET)

/* ==========================================================================*/
#define SCALER_SD48_OFFSET		0x0C
#define SCALER_SD48_REG			RCT_REG(SCALER_SD48_OFFSET)

#if (CHIP_REV == A7L)
#define SCALER_SDIO_OFFSET		0x334
#elif (CHIP_REV == S2L)
#define SCALER_SDIO_OFFSET		0x430
#elif (CHIP_REV == S2)
#define SCALER_SDIO_OFFSET		0x10
#endif
#define SCALER_SDIO_REG			RCT_REG(SCALER_SDIO_OFFSET)

#if (CHIP_REV == I1)
#define SCALER_SDXC_OFFSET		0x2A4
#elif (CHIP_REV == S2L)
#define SCALER_SDXC_OFFSET		0x434
#endif
#define SCALER_SDXC_REG			RCT_REG(SCALER_SDXC_OFFSET)

#if (CHIP_REV == I1)
#define PLL_SDXC_CTRL_OFFSET		0x290
#define PLL_SDXC_FRAC_OFFSET		0x294
#define PLL_SDXC_CTRL2_OFFSET		0x298
#define PLL_SDXC_CTRL3_OFFSET		0x29C
#define PLL_SDXC_CTRL_REG		RCT_REG(PLL_SDXC_CTRL_OFFSET)
#define PLL_SDXC_FRAC_REG		RCT_REG(PLL_SDXC_FRAC_OFFSET)
#define PLL_SDXC_CTRL2_REG		RCT_REG(PLL_SDXC_CTRL2_OFFSET)
#define PLL_SDXC_CTRL3_REG		RCT_REG(PLL_SDXC_CTRL3_OFFSET)
#endif

#if (CHIP_REV == S2L)
#define PLL_SD_CTRL_OFFSET		0x4AC
#define PLL_SD_FRAC_OFFSET		0x4B0
#define PLL_SD_CTRL2_OFFSET		0x4B4
#define PLL_SD_CTRL3_OFFSET		0x4B8
#define PLL_SD_CTRL_REG			RCT_REG(PLL_SD_CTRL_OFFSET)
#define PLL_SD_FRAC_REG			RCT_REG(PLL_SD_FRAC_OFFSET)
#define PLL_SD_CTRL2_REG		RCT_REG(PLL_SD_CTRL2_OFFSET)
#define PLL_SD_CTRL3_REG		RCT_REG(PLL_SD_CTRL3_OFFSET)
#endif

/* ==========================================================================*/

#endif /* __PLAT_AMBARELLA_RCT_H__ */

