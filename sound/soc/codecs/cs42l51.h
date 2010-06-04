/*
 * cs42l51.h  --  CS42L51 Soc Audio driver
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

#ifndef _CS42L51_H
#define _CS42L51_H

/* CS42L51 registers addresses */
#define CS42L51_CHIPID_REG		0x01	/* Chip ID Register, Read Only */
#define CS42L51_PWRCTL1_REG		0x02	/* Power Control 1*/
#define CS42L51_MICPWR_SPDCTL_REG	0x03	/* Mic Power and Speed Control */
#define CS42L51_INTFCTL_REG		0x04	/* Interface Control */
#define CS42L51_MICCTL_REG		0x05	/* MIC Control */
#define CS42L51_ADCCTL_REG		0x06	/* ADC Control */
#define CS42L51_ADCSEL_INVMUTE_REG	0x07	/* ADC Input Select, Invert & Mute */
#define CS42L51_DAC_OUTCTL_REG	0x08	/* DAC Output Control */
#define CS42L51_DAC_CTL_REG		0x09	/* DAC Control */
#define CS42L51_ALCA_PGAA_CTL_REG	0x0A	/* ALCA and PGAA Control */
#define CS42L51_ALCB_PGAB_CTL_REG	0x0B	/* ALCB and PGAB Control */
#define CS42L51_ADCA_ATT_REG	0x0C	/* ADCA Attenuator */
#define CS42L51_ADCB_ATT_REG	0x0D	/* ADCB Attenuator */
#define CS42L51_ADCA_MIXVOL_REG	0x0E	/* ADCA Mixer Volume Control */
#define CS42L51_ADCB_MIXVOL_REG	0x0F	/* ADCB Mixer Volume Control */
#define CS42L51_PCMA_MIXVOL_REG	0x10	/* PCMA Mixer Volume Control */
#define CS42L51_PCMB_MIXVOL_REG	0x11	/* PCMB Mixer Volume Control */
#define CS42L51_BP_FREQ_TIME_REG	0x12	/* Beep Frequency & Timing Configuration */
#define CS42L51_BP_OFFTIME_VOL_REG	0x13	/* Beep Off Time & Volume */
#define CS42L51_BP_TONE_CONF_REG	0x14	/* Beep and Tone Configuration */
#define CS42L51_TONE_CTL_REG	0x15	/* Tone Control */
#define CS42L51_AOUTA_VOLCTL_REG	0x16	/* AOUTA Volume Control */
#define CS42L51_AOUTB_VOLCTL_REG	0x17	/* AOUTB Volume Control */
#define CS42L51_ADCPCM_MIX_REG	0x18	/* ADC & PCM Channel Mixer*/
#define CS42L51_LIMIT_THD_SZC_REG	0x19	/* Limiter Threshold SZC Disable */
#define CS42L51_LIMIT_RELRATE_REG	0x1A	/* Limiter Release Rate */
#define CS42L51_LIMIT_ATKRATE_REG	0x1B	/* Limiter Attack Rate */
#define CS42L51_ALCEN_ATKRATE_REG	0x1C	/* ALC Enable and Attack Rate */
#define CS42L51_ALC_RELRATE_REG	0x1D	/* ALC Release Rate */
#define CS42L51_ALC_THD_REG		0x1E	/* ALC Threshold */
#define CS42L51_NOISEGATE_MISC_REG	0x1F	/* Noise Gate Configuration & Misc */
#define CS42L51_STATUS_REG		0x20	/* Status Register, Read Only */
#define CS42L51_CHARGE_FREQ_REG	0x21	/* Charge Pumpu Frequency */

#define CS42L51_NUMREGS	34


/* Bit masks for the CS42L51 registers */

/* 01h */
#define CS42L51_CHIPID_ID		0xF8
#define CS42L51_CHIPID_REV		0x07
/* 02h */
#define CS42L51_PWRCTL_PDN_DACB	0x40
#define CS42L51_PWRCTL_PDN_DACA	0x20
#define CS42L51_PWRCTL_PDN_PGAB	0x10
#define CS42L51_PWRCTL_PDN_PGAA	0x08
#define CS42L51_PWRCTL_PDN_ADCB	0x04
#define CS42L51_PWRCTL_PDN_ADCA	0x02
#define CS42L51_PWRCTL_PDN		0x01
/* 03h */
#define CS42L51_AUTO_ENABLE		0x80
#define CS42L51_SPEED_MASK		0x60
#define CS42L51_SPEED_QSM		0x60
#define CS42L51_SPEED_HSM		0x40
#define CS42L51_SPEED_SSM		0x20
#define CS42L51_SPEED_DSM		0x00
#define CS42L51_PDN_MICB		0x08
#define CS42L51_PDN_MICA		0x04
#define CS42L51_PDN_MICBIAS		0x02
#define CS42L51_MCLKDIV2		0x01
/* 04h */
#define CS42L51_INTF_MASTER		0x40
#define CS42L51_INTF_DACDIF_MASK	0x38
#define CS42L51_INTF_DACDIF_LJ24	0x00
#define CS42L51_INTF_DACDIF_I2S	0x08
#define CS42L51_INTF_DACDIF_RJ24	0x10
#define CS42L51_INTF_DACDIF_RJ20	0x18
#define CS42L51_INTF_DACDIF_RJ18	0x20
#define CS42L51_INTF_DACDIF_RJ16	0x28
#define CS42L51_INTF_ADCDIF_I2S	0x04

/* 07h */
#define CS42L51_MUTE_ADC_A	0x01
#define CS42L51_MUTE_ADC_B	0x02
/* 08h */
#define CS42L51_MUTE_DAC_A	0x01
#define CS42L51_MUTE_DAC_B	0x02
/* 09h */
#define CS42L51_FREEZE_ENABLE	0x20


struct cs42l51_setup_data {
	int i2c_bus;
	unsigned short	i2c_address;
	unsigned int	rst_pin;
	unsigned int 	rst_delay;
};

#define CS42L51_SYSCLK	0
#define CS42L51_CLKDIV_LRCLK	0

extern struct snd_soc_dai cs42l51_dai;
extern struct snd_soc_codec_device soc_codec_dev_cs42l51;

#endif

