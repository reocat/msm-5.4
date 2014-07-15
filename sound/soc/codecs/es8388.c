/*
 * es8388.c  --  ES8388 ALSA Soc Audio driver
 *
 * Copyright 2014 Ambarella Ltd.
 *
 * Author: Ken He <jianhe@ambarella.com>
 *
 * Based on es8328.c from Everest Semiconductor
 *
 * History:
 *	2014/7/01 - [Ken He] Created file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/regmap.h>

#include "es8388.h"

#define ES8388_VERSION "v1.0"

/* codec private data */
struct es8388_priv {
	struct regmap *regmap;
	unsigned int sysclk;
};

/*
 * es8388 register
 */
static struct reg_default  es8388_reg_defaults[] = {
	{ 0,  0x06 },
	{ 1,  0x1c },
	{ 2,  0xC3 },
	{ 3,  0xFC },
	{ 4,  0xC0 },
	{ 5,  0x00 },
	{ 6,  0x00 },
	{ 7,  0x7C },
	{ 8,  0x80 },
	{ 9,  0x00 },
	{ 10, 0x00 },
	{ 11, 0x06 },
	{ 12, 0x00 },
	{ 13, 0x06 },
	{ 14, 0x30 },
	{ 15, 0x30 },
	{ 16, 0xC0 },
	{ 17, 0xC0 },
	{ 18, 0x38 },
	{ 19, 0xB0 },
	{ 20, 0x32 },
	{ 21, 0x06 },
	{ 22, 0x00 },
	{ 23, 0x00 },
	{ 24, 0x06 },
	{ 25, 0x32 },
	{ 26, 0xC0 },
	{ 27, 0xC0 },
	{ 28, 0x08 },
	{ 29, 0x06 },
	{ 30, 0x1F },
	{ 31, 0xF7 },
	{ 32, 0xFD },
	{ 33, 0xFF },
	{ 34, 0x1F },
	{ 35, 0xF7 },
	{ 36, 0xFD },
	{ 37, 0xFF },
	{ 38, 0x00 },
	{ 39, 0x38 },
	{ 40, 0x38 },
	{ 41, 0x38 },
	{ 42, 0x38 },
	{ 43, 0x38 },
	{ 44, 0x38 },
	{ 45, 0x00 },
	{ 46, 0x00 },
	{ 47, 0x00 },
	{ 48, 0x00 },
	{ 49, 0x00 },
	{ 50, 0x00 },
	{ 51, 0x00 },
	{ 52, 0x00 },
};

/* DAC/ADC Volume: min -96.0dB (0xC0) ~ max 0dB (0x00)  ( 0.5 dB step ) */
static const DECLARE_TLV_DB_SCALE(digital_tlv, -9600, 50, 0);
/* Analog Out Volume: min -30.0dB (0x00) ~ max 3dB (0x21)  ( 1 dB step ) */
static const DECLARE_TLV_DB_SCALE(out_tlv, -3000, 100, 0);
/* Analog In Volume: min 0dB (0x00) ~ max 24dB (0x08)  ( 3 dB step ) */
static const DECLARE_TLV_DB_SCALE(in_tlv, 0, 300, 0);

static const struct snd_kcontrol_new es8388_snd_controls[] = {
	SOC_DOUBLE_R_TLV("Playback Volume",
		ES8388_LDAC_VOL, ES8388_RDAC_VOL, 0, 0xC0, 1, digital_tlv),
	SOC_DOUBLE_R_TLV("Analog Out Volume",
		ES8388_LOUT1_VOL, ES8388_ROUT1_VOL, 0, 0x1D, 0, out_tlv),

	SOC_DOUBLE_R_TLV("Capture Volume",
		ES8388_LADC_VOL, ES8388_RADC_VOL, 0, 0xC0, 1, digital_tlv),
	SOC_DOUBLE_TLV("Analog In Volume",
		ES8388_ADCCONTROL1, 4, 0, 0x08, 0, in_tlv),
};

/*
 * DAPM Controls
 */

/* Channel Input Mixer */
static const char *es8388_line_texts[] = { "Line 1", "Line 2", "Differential"};
static const unsigned int es8388_line_values[] = { 0, 1, 3};

static const struct soc_enum es8388_lline_enum =
	SOC_VALUE_ENUM_SINGLE(ES8388_ADCCONTROL2, 6, 3,
		ARRAY_SIZE(es8388_line_texts), es8388_line_texts,
		es8388_line_values);
static const struct snd_kcontrol_new es8388_left_line_controls =
	SOC_DAPM_VALUE_ENUM("Route", es8388_lline_enum);

static const struct soc_enum es8388_rline_enum =
	SOC_VALUE_ENUM_SINGLE(ES8388_ADCCONTROL2, 4, 3,
		ARRAY_SIZE(es8388_line_texts), es8388_line_texts,
		es8388_line_values);
static const struct snd_kcontrol_new es8388_right_line_controls =
	SOC_DAPM_VALUE_ENUM("Route", es8388_rline_enum);


/* Left Mixer */
static const struct snd_kcontrol_new es8388_left_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left Playback Switch", ES8388_DACCONTROL17, 7, 1, 0),
	SOC_DAPM_SINGLE("Left Bypass Switch"  , ES8388_DACCONTROL17, 6, 1, 0),
};

/* Right Mixer */
static const struct snd_kcontrol_new es8388_right_mixer_controls[] = {
	SOC_DAPM_SINGLE("Right Playback Switch", ES8388_DACCONTROL20, 7, 1, 0),
	SOC_DAPM_SINGLE("Right Bypass Switch"  , ES8388_DACCONTROL20, 6, 1, 0),
};
#if 0
/* Differential Mux */
static const char *es8388_diff_sel[] = {"Line 1", "Line 2"};
static const struct soc_enum diffmux =
	SOC_ENUM_SINGLE(ES8388_ADCCONTROL3, 7, 2, es8388_diff_sel);
static const struct snd_kcontrol_new es8388_diffmux_controls =
	SOC_DAPM_ENUM("Route", diffmux);
#endif
/* Mono ADC Mux */
static const char *es8388_mono_mux[] = {"Stereo", "Mono (Left)", "Mono (Right)", "NONE"};
static const struct soc_enum monomux =
	SOC_ENUM_SINGLE(ES8388_ADCCONTROL3, 3, 4, es8388_mono_mux);
static const struct snd_kcontrol_new es8388_monomux_controls =
	SOC_DAPM_ENUM("Route", monomux);

static const struct snd_soc_dapm_widget es8388_dapm_widgets[] = {
	/* DAC Part */
	SND_SOC_DAPM_MIXER("Left Mixer", SND_SOC_NOPM, 0, 0,
		&es8388_left_mixer_controls[0], ARRAY_SIZE(es8388_left_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Mixer", SND_SOC_NOPM, 0, 0,
		&es8388_right_mixer_controls[0], ARRAY_SIZE(es8388_right_mixer_controls)),

	SND_SOC_DAPM_MUX("Left Line Mux", SND_SOC_NOPM, 0, 0, &es8388_left_line_controls),
	SND_SOC_DAPM_MUX("Right Line Mux", SND_SOC_NOPM, 0, 0, &es8388_right_line_controls),

	SND_SOC_DAPM_DAC("Left DAC"  , "Left Playback" , ES8388_DACPOWER, 7, 1),
	SND_SOC_DAPM_DAC("Right DAC" , "Right Playback", ES8388_DACPOWER, 6, 1),
	SND_SOC_DAPM_PGA("Left Out 1" , ES8388_DACPOWER, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right Out 1", ES8388_DACPOWER, 4, 0, NULL, 0),
	/* SND_SOC_DAPM_PGA("Left Out 2" , ES8388_DACPOWER, 3, 0, NULL, 0), */
	/* SND_SOC_DAPM_PGA("Right Out 2", ES8388_DACPOWER, 2, 0, NULL, 0), */

	SND_SOC_DAPM_OUTPUT("LOUT1"),
	SND_SOC_DAPM_OUTPUT("ROUT1"),
	SND_SOC_DAPM_OUTPUT("LOUT2"),
	SND_SOC_DAPM_OUTPUT("ROUT2"),

	/* ADC Part */
//	SND_SOC_DAPM_MUX("Differential Mux", SND_SOC_NOPM, 0, 0,
//		&es8388_diffmux_controls),

//	SND_SOC_DAPM_MUX("Left Line Mux", ES8388_ADCPOWER, 7, 1, &es8388_left_line_controls),
//	SND_SOC_DAPM_MUX("Right Line Mux", ES8388_ADCPOWER, 6, 1, &es8388_right_line_controls),

	SND_SOC_DAPM_MUX("Left ADC Mux", SND_SOC_NOPM, 0, 0, &es8388_monomux_controls),
	SND_SOC_DAPM_MUX("Right ADC Mux", SND_SOC_NOPM, 0, 0, &es8388_monomux_controls),

	SND_SOC_DAPM_PGA("Left Analog Input" , ES8388_ADCPOWER, 7, 1, NULL, 0),
	SND_SOC_DAPM_PGA("Right Analog Input", ES8388_ADCPOWER, 6, 1, NULL, 0),
	SND_SOC_DAPM_ADC("Left ADC" , "Left Capture" , ES8388_ADCPOWER, 5, 1),
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture", ES8388_ADCPOWER, 4, 1),

	SND_SOC_DAPM_MICBIAS("Mic Bias", ES8388_ADCPOWER, 3, 1),

	SND_SOC_DAPM_INPUT("MICIN"),
	SND_SOC_DAPM_INPUT("LINPUT1"),
	SND_SOC_DAPM_INPUT("LINPUT2"),
	SND_SOC_DAPM_INPUT("RINPUT1"),
	SND_SOC_DAPM_INPUT("RINPUT2"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* left mixer */
	{"Left Mixer", "Left Playback Switch", "Left DAC"},

	/* right mixer */
	{"Right Mixer", "Right Playback Switch", "Right DAC"},

	/* left out 1 */
	{"Left Out 1", NULL, "Left Mixer"},
	{"LOUT1", NULL, "Left Out 1"},

	/* right out 1 */
	{"Right Out 1", NULL, "Right Mixer"},
	{"ROUT1", NULL, "Right Out 1"},

	/* Left Line Mux */
	{"Left Line Mux", "Line 1", "LINPUT1"},
	{"Left Line Mux", "Line 2", "LINPUT2"},
	{"Left Line Mux", NULL, "MICIN"},

	/* Right Line Mux */
	{"Right Line Mux", "Line 1", "RINPUT1"},
	{"Right Line Mux", "Line 2", "RINPUT2"},
	{"Right Line Mux", NULL, "MICIN"},

	/* Left ADC Mux */
	{"Left ADC Mux", "Stereo", "Left Line Mux"},
//	{"Left ADC Mux", "Mono (Left)" , "Left Line Mux"},

	/* Right ADC Mux */
	{"Right ADC Mux", "Stereo", "Right Line Mux"},
//	{"Right ADC Mux", "Mono (Right)", "Right Line Mux"},

	/* ADC */
	{"Left ADC" , NULL, "Left ADC Mux"},
	{"Right ADC", NULL, "Right ADC Mux"},
};

static int es8388_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct es8388_priv *es8388 = snd_soc_codec_get_drvdata(codec);

	es8388->sysclk = freq;
	return 0;
}

static int es8388_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 iface = 0;
	u8 adciface = 0;
	u8 daciface = 0;

	iface = snd_soc_read(codec, ES8388_IFACE);
	adciface = snd_soc_read(codec, ES8388_ADC_IFACE);
	daciface = snd_soc_read(codec, ES8388_DAC_IFACE);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:    // MASTER MODE
		iface |= 0x80;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:    // SLAVE MODE
		iface &= 0x7F;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		adciface &= 0xFC;
		daciface &= 0xF9;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		iface &= 0xDF;
		adciface &= 0xDF;
		daciface &= 0xBF;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x20;
		adciface |= 0x20;
		daciface |= 0x40;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x20;
		adciface &= 0xDF;
		daciface &= 0xBF;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface &= 0xDF;
		adciface |= 0x20;
		daciface |= 0x40;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, ES8388_IFACE    , iface);
	snd_soc_write(codec, ES8388_ADC_IFACE, adciface);
	snd_soc_write(codec, ES8388_DAC_IFACE, daciface);

	return 0;
}

static int es8388_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 iface;

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		iface = snd_soc_read(codec, ES8388_DAC_IFACE) & 0xC7;
		/* bit size */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x0018;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			iface |= 0x0008;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x0020;
			break;
		}
		/* set iface & srate */
		snd_soc_write(codec, ES8388_DAC_IFACE, iface);
	} else {
		iface = snd_soc_read(codec, ES8388_ADC_IFACE) & 0xE3;
		/* bit size */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			iface |= 0x000C;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			iface |= 0x0004;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			iface |= 0x0010;
			break;
		}
		/* set iface */
		snd_soc_write(codec, ES8388_ADC_IFACE, iface);
	}

	return 0;
}

static int es8388_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned char val = 0;

	val = snd_soc_read(codec, ES8388_DAC_MUTE);
	if (mute){
		val |= 0x04;
	} else {
		val &= ~0x04;
	}

	snd_soc_write(codec, ES8388_DAC_MUTE, val);

	return 0;
}


static int es8388_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	switch(level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		if(codec->dapm.bias_level != SND_SOC_BIAS_ON) {
			/* updated by David-everest,5-25
			// Chip Power on
			snd_soc_write(codec, ES8388_CHIPPOWER, 0xF3);
			// VMID control
			snd_soc_write(codec, ES8388_CONTROL1 , 0x06);
			// ADC/DAC DLL power on
			snd_soc_write(codec, ES8388_CONTROL2 , 0xF3);
			*/
			snd_soc_write(codec, ES8388_ADCPOWER, 0x00);
			snd_soc_write(codec, ES8388_DACPOWER , 0x30);
			snd_soc_write(codec, ES8388_CHIPPOWER , 0x00);
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		/*
		// ADC/DAC DLL power on
		snd_soc_write(codec, ES8388_CONTROL2 , 0xFF);
		// Chip Power off
		snd_soc_write(codec, ES8388_CHIPPOWER, 0xF3);
		*/
		snd_soc_write(codec, ES8388_ADCPOWER, 0x00);
		snd_soc_write(codec, ES8388_DACPOWER , 0x30);
		snd_soc_write(codec, ES8388_CHIPPOWER , 0x00);
		break;

	case SND_SOC_BIAS_OFF:
		/*
		// ADC/DAC DLL power off
		snd_soc_write(codec, ES8388_CONTROL2 , 0xFF);
		// Chip Control
		snd_soc_write(codec, ES8388_CONTROL1 , 0x00);
		// Chip Power off
		snd_soc_write(codec, ES8388_CHIPPOWER, 0xFF);
		*/
		snd_soc_write(codec, ES8388_ADCPOWER, 0xFF);
		snd_soc_write(codec, ES8388_DACPOWER , 0xC0);
		snd_soc_write(codec, ES8388_CHIPPOWER , 0xC3);
		break;
	}

	codec->dapm.bias_level = level;

	return 0;
}


#define ES8388_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
                    SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
                    SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

#define ES8388_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
                    SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_soc_dai_ops es8388_dai_ops = {
	.hw_params    = es8388_pcm_hw_params,
	.set_fmt      = es8388_set_dai_fmt,
	.set_sysclk   = es8388_set_dai_sysclk,
	.digital_mute = es8388_mute,
};


struct snd_soc_dai_driver es8388_dai = {
	.name = "es8388-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = ES8388_RATES,
		.formats = ES8388_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = ES8388_RATES,
		.formats = ES8388_FORMATS,},
	.ops = &es8388_dai_ops,
	.symmetric_rates = 1,
};

static int es8388_suspend(struct snd_soc_codec *codec)
{
	es8388_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int es8388_resume(struct snd_soc_codec *codec)
{
	es8388_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static int es8388_remove(struct snd_soc_codec *codec)
{
	es8388_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static inline int es8388_reset(struct regmap *map)
{
	int ret = 0;
	regmap_write(map, 0x00, 0x80);
	ret = regmap_write(map, 0x00, 0x00);
	return ret;
}

static int es8388_probe(struct snd_soc_codec *codec)
{
	struct es8388_priv *es8388 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int i = 0;

	dev_info(codec->dev, "ES8388 Audio Codec %s", ES8388_VERSION);
	codec->control_data = es8388->regmap;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	es8388_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	snd_soc_write(codec, ES8388_MASTERMODE,0x00);
	snd_soc_write(codec, ES8388_CHIPPOWER, 0xF3);
	snd_soc_write(codec, ES8388_DACCONTROL23, 0x00);
//	snd_soc_write(codec, ES8388_ANAVOLMANAG, 0x74);
	snd_soc_write(codec, ES8388_DACCONTROL21, 0x80);
	snd_soc_write(codec, ES8388_CONTROL1, 0x30);
	snd_soc_write(codec, ES8388_CHIPLOPOW2, 0xFF);
	snd_soc_write(codec, ES8388_CHIPLOPOW1, 0x00);
	//-------------ADC---------------------------//
	snd_soc_write(codec, ES8388_ADCCONTROL1, 0x88);
	snd_soc_write(codec, ES8388_ADCCONTROL2, 0xF0);
	snd_soc_write(codec, ES8388_ADCCONTROL3, 0x02);
	snd_soc_write(codec, ES8388_ADCCONTROL4, 0x0C);
	snd_soc_write(codec, ES8388_ADCCONTROL5, 0x02);
	snd_soc_write(codec, ES8388_ADCCONTROL7, 0xF0);
	snd_soc_write(codec, ES8388_ADCCONTROL8, 0x00);
	snd_soc_write(codec, ES8388_ADCCONTROL9, 0x00);
	snd_soc_write(codec, ES8388_ADCCONTROL10, 0xDA);
	snd_soc_write(codec, ES8388_ADCCONTROL11, 0xB0);
	snd_soc_write(codec, ES8388_ADCCONTROL12, 0x12);
	snd_soc_write(codec, ES8388_ADCCONTROL13, 0x06);
	snd_soc_write(codec, ES8388_ADCCONTROL14, 0x11);
	//-------------DAC-----------------------------//
	snd_soc_write(codec, ES8388_DACCONTROL1, 0x18);
	snd_soc_write(codec, ES8388_DACCONTROL2, 0x02);
	snd_soc_write(codec, ES8388_DACCONTROL3, 0x76);
	snd_soc_write(codec, ES8388_DACCONTROL4, 0x00);
	snd_soc_write(codec, ES8388_DACCONTROL5, 0x00);
	snd_soc_write(codec, ES8388_DACCONTROL17, 0xB8);
	snd_soc_write(codec, ES8388_DACCONTROL20, 0xB8);

	snd_soc_write(codec, ES8388_CHIPPOWER, 0x00);
	snd_soc_write(codec, ES8388_CONTROL1, 0x37);
	snd_soc_write(codec, ES8388_CONTROL2, 0x72);
	snd_soc_write(codec, ES8388_DACPOWER, 0x3C);
	snd_soc_write(codec, ES8388_ADCPOWER, 0xF9);
	snd_soc_write(codec, ES8388_CONTROL1, 0x32);
	snd_soc_write(codec, ES8388_DACCONTROL3, 0x72);

	snd_soc_write(codec, ES8388_CHIPPOWER, 0xF3);
	for( i = 0; i < 0x1d; i++)
	{
		snd_soc_write(codec, ES8388_DACCONTROL24, i);    //LOUT1/ROUT1 VOLUME
		snd_soc_write(codec, ES8388_DACCONTROL25, i);
		snd_soc_write(codec, ES8388_DACCONTROL26, i);    //LOUT2/ROUT2 VOLUME
		snd_soc_write(codec, ES8388_DACCONTROL27, i);
		msleep(5);
	}

	return ret;
}

static bool es8388_volatile_register(struct device *dev,
							unsigned int reg)
{
	switch (reg) {
		case 0:
		return true;

	default:
		break;
	}

	return false;
}


static struct snd_soc_codec_driver soc_codec_dev_es8388 = {
	.probe =	es8388_probe,
	.remove =	es8388_remove,
	.suspend =	es8388_suspend,
	.resume =	es8388_resume,
	.set_bias_level = es8388_set_bias_level,

	.dapm_widgets = es8388_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(es8388_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),

	.controls = es8388_snd_controls,
	.num_controls = ARRAY_SIZE(es8388_snd_controls),
};

static struct regmap_config es8388_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = ES8388_MAX_REGISTER,
	.reg_defaults = es8388_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(es8388_reg_defaults),
	.volatile_reg = es8388_volatile_register,
	.cache_type = REGCACHE_RBTREE,
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static int es8388_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct es8388_priv *es8388;
	int ret = 0;

	es8388 = devm_kzalloc(&i2c->dev, sizeof(struct es8388_priv), GFP_KERNEL);
	if (es8388 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, es8388);
	es8388->regmap = devm_regmap_init_i2c(i2c, &es8388_regmap);
	if (IS_ERR(es8388->regmap)) {
		ret = PTR_ERR(es8388->regmap);
		dev_err(&i2c->dev, "regmap_init() failed: %d\n", ret);
		return ret;
	}

	ret =  snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_es8388, &es8388_dai, 1);

	return ret;
}

static int es8388_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	return 0;
}

static struct of_device_id es8388_of_match[] = {
	{ .compatible = "ambarella,es8388",},
	{},
};
MODULE_DEVICE_TABLE(of, es8388_of_match);
static const struct i2c_device_id es8388_i2c_id[] = {
	{ "es8388", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es8388_i2c_id);

static struct i2c_driver es8388_i2c_driver = {
	.driver = {
		.name = "es8388-codec",
		.owner = THIS_MODULE,
		.of_match_table = es8388_of_match,
	},
	.probe    = es8388_i2c_probe,
	.remove   = es8388_i2c_remove,
	.id_table = es8388_i2c_id,
};
#endif

static int __init es8388_modinit(void)
{
	int ret = 0;

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&es8388_i2c_driver);
	if (ret != 0) {
		pr_err("Failed to register ES8388 I2C driver: %d\n", ret);
	}
#endif
	return ret;
}
module_init(es8388_modinit);

static void __exit es8388_exit(void)
{
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	i2c_del_driver(&es8388_i2c_driver);
#endif
}
module_exit(es8388_exit);

MODULE_DESCRIPTION("ASoC ES8388 driver");
MODULE_AUTHOR("Ken He <jianhe@ambarella.com>");
MODULE_LICENSE("GPL");

