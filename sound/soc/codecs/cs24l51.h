/*
 * cs24l51.h  --  CS24L51 Soc Audio driver
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2009/07/01 - [Cao Rongrong] Created file
 * 
 * Copyright (C) 2004-2009, Ambarella, Inc.
 *
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

#ifndef _CS24L51_H
#define _CS24L51_H

/* CS24L51 registers addresses */
#define CS24L51_CHIPID_REG		0x01	/* Chip ID Register, Read Only */
#define CS24L51_PWRCTL1_REG		0x02	/* Power Control 1*/
#define CS24L51_MICPWR_SPDCTL_REG	0x03	/* Mic Power and Speed Control */
#define CS24L51_INTFCTL_REG		0x04	/* Interface Control */
#define CS24L51_MICCTL_REG		0x05	/* MIC Control */
#define CS24L51_ADCCTL_REG		0x06	/* ADC Control */
#define CS24L51_ADCSEL_INVMUTE_REG	0x07	/* ADC Input Select, Invert & Mute */
#define CS24L51_DAC_OUTCTL_REG	0x08	/* DAC Output Control */
#define CS24L51_DAC_CTL_REG		0x09	/* DAC Control */
#define CS24L51_ALCA_PGAA_CTL_REG	0x0A	/* ALCA and PGAA Control */
#define CS24L51_ALCB_PGAB_CTL_REG	0x0B	/* ALCB and PGAB Control */
#define CS24L51_ADCA_ATT_REG	0x0C	/* ADCA Attenuator */
#define CS24L51_ADCB_ATT_REG	0x0D	/* ADCB Attenuator */
#define CS24L51_ADCA_MIXVOL_REG	0x0E	/* ADCA Mixer Volume Control */
#define CS24L51_ADCB_MIXVOL_REG	0x0F	/* ADCB Mixer Volume Control */
#define CS24L51_PCMA_MIXVOL_REG	0x10	/* PCMA Mixer Volume Control */
#define CS24L51_PCMB_MIXVOL_REG	0x11	/* PCMB Mixer Volume Control */
#define CS24L51_BP_FREQ_TIME_REG	0x12	/* Beep Frequency & Timing Configuration */
#define CS24L51_BP_OFFTIME_VOL_REG	0x13	/* Beep Off Time & Volume */
#define CS24L51_BP_TONE_CONF_REG	0x14	/* Beep and Tone Configuration */
#define CS24L51_TONE_CTL_REG	0x15	/* Tone Control */
#define CS24L51_AOUTA_VOLCTL_REG	0x16	/* AOUTA Volume Control */
#define CS24L51_AOUTB_VOLCTL_REG	0x17	/* AOUTB Volume Control */
#define CS24L51_ADCPCM_MIX_REG	0x18	/* ADC & PCM Channel Mixer*/
#define CS24L51_LIMIT_THD_SZC_REG	0x19	/* Limiter Threshold SZC Disable */
#define CS24L51_LIMIT_RELRATE_REG	0x1A	/* Limiter Release Rate */
#define CS24L51_LIMIT_ATKRATE_REG	0x1B	/* Limiter Attack Rate */
#define CS24L51_ALCEN_ATKRATE_REG	0x1C	/* ALC Enable and Attack Rate */
#define CS24L51_ALC_RELRATE_REG	0x1D	/* ALC Release Rate */
#define CS24L51_ALC_THD_REG		0x1E	/* ALC Threshold */
#define CS24L51_NOISEGATE_MISC_REG	0x1F	/* Noise Gate Configuration & Misc */
#define CS24L51_STATUS_REG		0x20	/* Status Register, Read Only */
#define CS24L51_CHARGE_FREQ_REG	0x21	/* Charge Pumpu Frequency */


#define CS24L51_NUMREGS	34

/* Bit masks for the CS24L51 registers */
#define CS24L51_CHIPID_ID		0xF8
#define CS24L51_CHIPID_REV		0x07

#define CS24L51_PWRCTL_PDN_DACB	0x40
#define CS24L51_PWRCTL_PDN_DACA	0x20
#define CS24L51_PWRCTL_PDN_PGAB	0x10
#define CS24L51_PWRCTL_PDN_PGAA	0x08
#define CS24L51_PWRCTL_PDN_ADCB	0x04
#define CS24L51_PWRCTL_PDN_ADCA	0x02
#define CS24L51_PWRCTL_PDN		0x01

#define CS24L51_AUTO_ENABLE		0x80
#define CS24L51_SPEED_MASK		0x60
#define CS24L51_SPEED_QSM		0x60
#define CS24L51_SPEED_HSM		0x40
#define CS24L51_SPEED_SSM		0x20
#define CS24L51_SPEED_DSM		0x00
#define CS24L51_PDN_MICB		0x08
#define CS24L51_PDN_MICA		0x04
#define CS24L51_PDN_MICBIAS		0x02
#define CS24L51_MCLKDIV2		0x01

#define CS24L51_INTF_MASTER		0x40
#define CS24L51_INTF_DACDIF_MASK	0x38
#define CS24L51_INTF_DACDIF_LJ24	0x00
#define CS24L51_INTF_DACDIF_I2S	0x08
#define CS24L51_INTF_DACDIF_RJ24	0x10
#define CS24L51_INTF_DACDIF_RJ20	0x18
#define CS24L51_INTF_DACDIF_RJ18	0x20
#define CS24L51_INTF_DACDIF_RJ16	0x28
#define CS24L51_INTF_ADCDIF_I2S	0x04

#define CS24L51_FREEZE_ENABLE	0x20

#define CS24L51_MODE_DIV_MASK	0x0E
#define CS24L51_MODE_DIV1	0x00
#define CS24L51_MODE_DIV15	0x02
#define CS24L51_MODE_DIV2	0x04
#define CS24L51_MODE_DIV3	0x06
#define CS24L51_MODE_DIV4	0x08
#define CS24L51_MODE_POPGUARD	0x01
#define CS24L51_FORMAT_FREEZE_A	0x80
#define CS24L51_FORMAT_FREEZE_B	0x40
#define CS24L51_FORMAT_LOOPBACK	0x20
#define CS24L51_FORMAT_DAC_MASK	0x18
#define CS24L51_FORMAT_DAC_LJ	0x00
#define CS24L51_FORMAT_DAC_I2S	0x08
#define CS24L51_FORMAT_DAC_RJ16	0x18
#define CS24L51_FORMAT_DAC_RJ24	0x10
#define CS24L51_FORMAT_ADC_MASK	0x01
#define CS24L51_FORMAT_ADC_LJ	0x00
#define CS24L51_FORMAT_ADC_I2S	0x01
#define CS24L51_TRANS_ONE_VOL	0x80
#define CS24L51_TRANS_SOFT	0x40
#define CS24L51_TRANS_ZERO	0x20
#define CS24L51_TRANS_INV_ADC_A	0x08
#define CS24L51_TRANS_INV_ADC_B	0x10
#define CS24L51_TRANS_INV_DAC_A	0x02
#define CS24L51_TRANS_INV_DAC_B	0x04
#define CS24L51_TRANS_DEEMPH	0x01
#define CS24L51_MUTE_AUTO	0x20
#define CS24L51_MUTE_ADC_A	0x08
#define CS24L51_MUTE_ADC_B	0x10
#define CS24L51_MUTE_POLARITY	0x04
#define CS24L51_MUTE_DAC_A	0x01
#define CS24L51_MUTE_DAC_B	0x02

struct cs24l51_setup_data {
	int i2c_bus;
	unsigned short	i2c_address;
	unsigned int	rst_pin;
	unsigned int 	rst_delay;
};

#define CS24L51_SYSCLK	0
#define CS24L51_CLKDIV_LRCLK	0

extern struct snd_soc_dai cs24l51_dai;
extern struct snd_soc_codec_device soc_codec_dev_cs24l51;

#endif

