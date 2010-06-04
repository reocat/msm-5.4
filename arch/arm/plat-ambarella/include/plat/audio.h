/*
 * arch/arm/plat-ambarella/include/plat/audio.h
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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

#ifndef __PLAT_AMBARELLA_AUDIO_H
#define __PLAT_AMBARELLA_AUDIO_H

/* ==========================================================================*/
#if (CHIP_REV == A2) || (CHIP_REV == A2S) || (CHIP_REV == A2M)
#define DAI_CLOCK_MASK		0x0400
#else
#define DAI_CLOCK_MASK		0x0000
#endif

#define MAX_MCLK_IDX_NUM		15

/* ==========================================================================*/
#ifndef __ASSEMBLER__

struct notifier_block;

struct ambarella_i2s_interface {
	u8 state;
	u8 mode;
	u8 sfreq;
	u8 mclk;
	u8 oversample;
	u8 word_order;
	u8 word_len;
	u8 word_pos;
	u8 slots;
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

enum Audio_Notify_Type
{
	AUDIO_NOTIFY_UNKNOWN,
	AUDIO_NOTIFY_INIT,
	AUDIO_NOTIFY_SETHWPARAMS,
	AUDIO_NOTIFY_REMOVE
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

enum DAI_WordOrder
{
	DAI_MSB_FIRST = 0,
	DAI_LSB_FIRST = 1
};

struct ambarella_i2s_controller {
	void					(*aucodec_digitalio_0)(void);
	void					(*aucodec_digitalio_1)(void);
	void					(*aucodec_digitalio_2)(void);
	void					(*channel_select)(u32);
	void					(*set_audio_pll)(u8);
};

/* ==========================================================================*/
extern struct platform_device			ambarella_i2s0;

extern u32					alsa_tx_enable_flag;

/* ==========================================================================*/
extern int ambarella_init_audio(void);

extern void ambarella_audio_notify_transition(struct ambarella_i2s_interface *data, unsigned int type);
extern int ambarella_audio_register_notifier(struct notifier_block *nb);
extern int ambarella_audio_unregister_notifier(struct notifier_block *nb);

extern struct ambarella_i2s_interface get_audio_i2s_interface(void);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

