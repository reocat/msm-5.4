/*
 * arch/arm/mach-ambarella/include/mach/audio.h
 *
 * History:
 *	2008/02/18 - [Allen Wang] created file
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

#ifndef __ASM_ARCH_AUDIO_H__
#define __ASM_ARCH_AUDIO_H__

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#if (CHIP_REV == A2)
#define AUDIO_CODEC_INSTANCES	1	/* Onchip audio codec */
#else
#define AUDIO_CODEC_INSTANCES	0
#endif

/****************************************************/
/* Controller registers definitions                 */
/****************************************************/

/**************************************/
/* On-chip Audio Codec */
/**************************************/

#if (AUDIO_CODEC_INSTANCES == 1)

#define AUC_ENABLE_OFFSET		0x000
#define AUC_DP_RESET_OFFSET		0x004
#define AUC_OSR_OFFSET                  0x008
#define AUC_CONTROL_OFFSET		0x00c
#define AUC_STATUS_OFFSET		0x010
#define AUC_CIC_OFFSET                  0x014
#define AUC_I2S_OFFSET                  0x018
#define AUC_DSM_GAIN_OFFSET		0x020
#define AUC_EMA_OFFSET                  0x024
#define AUC_ADC_CONTROL_OFFSET          0x034
#define AUC_ADC_VGA_GAIN_OFFSET		0x038
#define AUC_DAC_CONTROL_OFFSET          0x03c
#define AUC_DAC_GAIN_OFFSET		0x040
#define AUC_COEFF_START_DC_OFFSET	0x100
#define AUC_COEFF_START_IS_OFFSET	0x200
#define AUC_COEFF_START_PD_OFFSET	0x300
#define AUC_COEFF_START_HP_OFFSET	0x400
#define AUC_COEFF_START_WN_OFFSET	0x400
#define AUC_COEFF_START_DE_OFFSET	0x450

#define AUC_ENABLE_REG                  AUC_REG(0x000)
#define AUC_DP_RESET_REG		AUC_REG(0x004)
#define AUC_OSR_REG                     AUC_REG(0x008)
#define AUC_CONTROL_REG                 AUC_REG(0x00c)
#define AUC_STATUS_REG                  AUC_REG(0x010)
#define AUC_CIC_REG			AUC_REG(0x014)
#define AUC_I2S_REG			AUC_REG(0x018)
#define AUC_DSM_GAIN_REG		AUC_REG(0x020)
#define AUC_EMA_REG			AUC_REG(0x024)
#define AUC_ADC_CONTROL_REG		AUC_REG(0x034)
#define AUC_ADC_VGA_GAIN_REG		AUC_REG(0x038)
#define AUC_DAC_CONTROL_REG		AUC_REG(0x03c)
#define AUC_DAC_GAIN_REG		AUC_REG(0x040)
#define AUC_COEFF_START_DC_REG          AUC_REG(0x100)
#define AUC_COEFF_START_IS_REG		AUC_REG(0x200)
#define AUC_COEFF_START_PD_REG		AUC_REG(0x300)
#define AUC_COEFF_START_HP_REG		AUC_REG(0x400)
#define AUC_COEFF_START_WN_REG		AUC_REG(0x400)
#define AUC_COEFF_START_DE_REG		AUC_REG(0x450)
#endif

#define MAX_MCLK_IDX_NUM		15

#endif

