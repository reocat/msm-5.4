/*
 * ak4642.h  --  AK4642 Soc Audio driver
 *
 * Copyright 2009 Ambarella Ltd.
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * Based on ak4535.c by Richard Purdie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _AK4642_H
#define _AK4642_H

/* AK4642 register space */

#define AK4642_PM1		0x00
#define AK4642_PM2		0x01
#define AK4642_SIG1		0x02
#define AK4642_SIG2		0x03
#define AK4642_MODE1		0x04
#define AK4642_MODE2		0x05
#define AK4642_TMSEL		0x06
#define AK4642_ALC1		0x07
#define AK4642_ALC2		0x08
#define AK4642_LIVOL		0x09
#define AK4642_LDVOL		0x0a
#define AK4642_ALC3		0x0b
#define AK4642_RIVOL		0x0c
#define AK4642_RDVOL		0x0d
#define AK4642_MODE3		0x0e
#define AK4642_MODE4		0x0f
#define AK4642_PM3		0x10
#define AK4642_FSEL		0x11
#define AK4642_F3EF0		0x12
#define AK4642_F3EF1		0x13
#define AK4642_F3EF2		0x14
#define AK4642_F3EF3		0x15
#define AK4642_EQEF0		0x16
#define AK4642_EQEF1		0x17
#define AK4642_EQEF2		0x18
#define AK4642_EQEF3		0x19
#define AK4642_EQEF4		0x1a
#define AK4642_EQEF5		0x1b
#define AK4642_F1EF0		0x1c
#define AK4642_E1EF1		0x1d
#define AK4642_E1EF2		0x1e
#define AK4642_E1EF3		0x1f

#if 1

#define AK4642_DAC		0x6
#define AK4642_MIC		0x7
#define AK4642_TIMER		0x8
#define AK4642_PGA		0xb
#define AK4642_LATT		0xc
#define AK4642_RATT		0xd
#define AK4642_VOL		0xe
#define AK4642_STATUS		0xf

#endif

#define AK4642_CACHEREGNUM 	0x20

struct ak4642_setup_data {
	int		i2c_bus;
	unsigned short	i2c_address;
	unsigned int	rst_pin;
	unsigned int 	rst_delay;
};

#define AK4642_SYSCLK	0

extern struct snd_soc_dai ak4642_dai;
extern struct snd_soc_codec_device soc_codec_dev_ak4642;

#endif
