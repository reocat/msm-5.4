/*
 * ambhw/rct/rct.h
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2008/02/19 - [Allen Wang] changed to use capabilities and chip ID
 *	2008/05/07 - [Allen Wang] moved to separate files at directory 'rct'	
 *	2008/05/13 - [Allen Wang] added capabilities of A2S and A2M silicons  
 *
 * Copyright (C) 2006-2008, Ambarella, Inc.
 */

#ifndef __AMBHW__RCT_AUDIO_H__
#define __AMBHW__RCT_AUDIO_H__

/****************************************************/
/* Capabilities based on chip revision              */
/****************************************************/

#define RCT_SUPPORT_CG_SETTING_CHANGED	1

#if (CHIP_REV == A1)
#define RCT_AUDIO_PLL_CONF_MODE		0
#define RCT_AUDIO_PLL_USE_HAL_API	0
#elif (CHIP_REV == A2) || (CHIP_REV == A3)
#define RCT_AUDIO_PLL_CONF_MODE		1
#define RCT_AUDIO_PLL_USE_HAL_API	0
#elif (CHIP_REV == A5) || (CHIP_REV == A2S) || \
	(CHIP_REV == A2M) || (CHIP_REV == A2Q) || \
	(CHIP_REV == A6)
#define RCT_AUDIO_PLL_CONF_MODE		2
#define RCT_AUDIO_PLL_USE_HAL_API	0
#else
#define RCT_AUDIO_PLL_CONF_MODE		2
#define RCT_AUDIO_PLL_USE_HAL_API	1
#endif

#define PLL_LOCK_AUDIO  	(0x1 << 7)

#if (CHIP_REV == A2) || (CHIP_REV == A2S) || (CHIP_REV == A2M)
#define DAI_CLOCK_MASK		0x0400
#else
#define DAI_CLOCK_MASK		0x0000
#endif


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

#endif /* __ASSEMBLER__ */

#endif
