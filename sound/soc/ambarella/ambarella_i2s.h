/*
 * sound/soc/ambarella_i2s.h
 *
 * History:
 *	2008/03/03 - [Eric Lee] created file
 *	2008/04/16 - [Eric Lee] Removed the compiling warning
 *	2009/01/22 - [Anthony Ginger] Port to 2.6.28
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

#ifndef AMBARELLA_I2S_H_
#define AMBARELLA_I2S_H_

typedef struct audiopll_s {
	u8 op_mode;
	u8 fs_grp;
	u8 fs_idx;
	u8 mclk_idx;
} audiopll_t;

typedef struct dai_reg_set_s {
	u8	mode;		/* DAI mode select */
	u8	tx_ctrl;	/* Transmitter control register setting */

	u16	clock;		/* Clock and word select control */
	u8	tx_fifo_th;	/* Transmitter FIFO threshold register */
	u8	rx_fifo_th;	/* Receiver FIFO threshold register */
}dai_reg_set_t;

typedef struct dai_set_s{
	u32	sysclk;
	u32	sfreq;
	u8	fs_ratio;
	u8	slots;
	u8	mode;
	u8	resolution;
	u8	uniononoff;
	u8	oversample;
	dai_reg_set_t *dai_base;
	audiopll_t *audiopll;
}dai_set_t;

enum AudioCodec_MCLK {
	AudioCodec_18_432M = 0,
	AudioCodec_16_9344M = 1,
	AudioCodec_12_288M = 2,
	AudioCodec_11_2896M = 3,
	AudioCodec_9_216M = 4,
	AudioCodec_8_4672M = 5,
	AudioCodec_8_192M = 6,
	AudioCodec_6_144 = 7,
	AudioCodec_5_6448M = 8,
	AudioCodec_4_608M = 9,
	AudioCodec_4_2336M = 10,
	AudioCodec_4_096M = 11,
	AudioCodec_3_072M = 12,
	AudioCodec_2_8224M = 13,	
	AudioCodec_2_048M = 14
};

#define MAX_OVERSAMPLE_IDX_NUM	9
enum AudioCodec_OverSample {
	AudioCodec_128xfs = 0,
	AudioCodec_256xfs = 1,
	AudioCodec_384xfs = 2,
	AudioCodec_512xfs = 3,
	AudioCodec_768xfs = 4,
	AudioCodec_1024xfs = 5,
	AudioCodec_1152xfs = 6,
	AudioCodec_1536xfs = 7,
	AudioCodec_2304xfs = 8
};

#define DAI_CLOCK_MASK 0x0400

enum DAI_INIT_CTL
{
	DAI_FIFO_RST = 1,
	DAI_RX_EN = 2,
	DAI_TX_EN = 4
};

enum DAI_Mode
{
	DAI_leftJustified_Mode = 0,
	DAI_rightJustified_Mode = 1,
	DAI_MSBExtend_Mode = 2,
	DAI_I2S_Mode = 4,
	DAI_DSP_Mode = 6
};

enum DAI_resolution
{
	DAI_16bits = 0,
	DAI_18bits = 1,
	DAI_20bits = 2,
	DAI_24bits = 3,
	DAI_32bits = 4

};

enum DAI_ifunion
{
	DAI_union = 0,
	DAI_nonunion = 1
};

#define DAI_32slots	32
#define DAI_64slots	64
#define DAI_48slots	48

extern struct snd_soc_dai ambarella_i2s_dai;

#endif /*AMBARELLA_I2S_H_*/

