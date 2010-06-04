/*
 * sound/soc/coconut.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2009/08/20 - [Cao Rongrong] Created file
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

#include <linux/platform_device.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <mach/hardware.h>
#include <plat/audio.h>

#include "ambarella_pcm.h"
#include "ambarella_i2s.h"
#include "../codecs/ak4642.h"


#define AK4642_RESET_PIN	12
#define AK4642_RESET_DELAY	1

static int coconut_board_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static int coconut_board_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int errorCode = 0, amb_mclk, mclk, oversample;

	switch (params_rate(params)) {
	case 8000:
		amb_mclk = AudioCodec_2_048M;
		mclk = 2048000;
		oversample = AudioCodec_256xfs;
		break;
	case 11025:
		amb_mclk = AudioCodec_2_8224M;
		mclk = 2822400;
		oversample = AudioCodec_256xfs;
		break;
	case 12000:
		amb_mclk = AudioCodec_3_072M;
		mclk = 3072000;
		oversample = AudioCodec_256xfs;
		break;
	case 16000:
		amb_mclk = AudioCodec_4_096M;
		mclk = 4096000;
		oversample = AudioCodec_256xfs;
		break;
	case 22050:
		amb_mclk = AudioCodec_5_6448M;
		mclk = 5644800;
		oversample = AudioCodec_256xfs;
		break;
	case 24000:
		amb_mclk = AudioCodec_6_144;
		mclk = 6144000;
		oversample = AudioCodec_256xfs;
		break;
	case 32000:
		amb_mclk = AudioCodec_8_192M;
		mclk = 8192000;
		oversample = AudioCodec_256xfs;
		break;
	case 44100:
		amb_mclk = AudioCodec_11_2896M;
		mclk = 11289600;
		oversample = AudioCodec_256xfs;
		break;
	case 48000:
		amb_mclk = AudioCodec_12_288M;
		mclk = 12288000;
		oversample = AudioCodec_256xfs;
		break;
	default:
		errorCode = -EINVAL;
		goto hw_params_exit;
	}

	/* set the I2S system data format*/
	errorCode = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (errorCode < 0) {
		pr_err("can't set codec DAI configuration\n");
		goto hw_params_exit;
	}

	errorCode = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (errorCode < 0) {
		pr_err("can't set cpu DAI configuration\n");
		goto hw_params_exit;
	}

	/* set the I2S system clock*/
	errorCode = snd_soc_dai_set_sysclk(codec_dai, AK4642_SYSCLK, mclk, 0);
	if (errorCode < 0) {
		pr_err("can't set cpu MCLK configuration\n");
		goto hw_params_exit;
	}

	errorCode = snd_soc_dai_set_sysclk(cpu_dai, AMBARELLA_CLKSRC_ONCHIP, amb_mclk, 0);
	if (errorCode < 0) {
		pr_err("can't set cpu MCLK configuration\n");
		goto hw_params_exit;
	}

	errorCode = snd_soc_dai_set_clkdiv(cpu_dai, AMBARELLA_CLKDIV_LRCLK, oversample);
	if (errorCode < 0) {
		pr_err("can't set cpu MCLK/SF ratio\n");
		goto hw_params_exit;
	}

hw_params_exit:
	return errorCode;
}


static struct snd_soc_ops coconut_board_ops = {
	.startup = coconut_board_startup,
	.hw_params = coconut_board_hw_params,
};

/* coconut machine dapm widgets */
static const struct snd_soc_dapm_widget coconut_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
	SND_SOC_DAPM_HP("HP Jack", NULL),
};

/* coconut machine audio map (connections to ak4642 pins) */
static const struct snd_soc_dapm_route coconut_audio_map[] = {
	/* Line In is connected to LLIN1, RLIN1 */
	{"LIN1", NULL, "Mic Jack"},
	{"RIN1", NULL, "Mic Jack"},
	{"LIN2", NULL, "Line In"},
	{"RIN2", NULL, "Line In"},

	/* Line Out is connected to LLOUT, RLOUT */
	{"Line Out", NULL, "LOUT"},
	{"Line Out", NULL, "ROUT"},
	{"HP Jack", NULL, "HPL"},
	{"HP Jack", NULL, "HPR"},
};


static int coconut_ak4642_init(struct snd_soc_codec *codec)
{
	int errorCode = 0;

	/* not connected */
	snd_soc_dapm_nc_pin(codec, "SPP");
	snd_soc_dapm_nc_pin(codec, "SPN");
	snd_soc_dapm_nc_pin(codec, "MIN");

	/* Add coconut specific widgets */
	errorCode = snd_soc_dapm_new_controls(codec,
		coconut_dapm_widgets,
		ARRAY_SIZE(coconut_dapm_widgets));
	if (errorCode) {
		goto init_exit;
	}

	/* Set up coconut specific audio path coconut_audio_map */
	errorCode = snd_soc_dapm_add_routes(codec,
		coconut_audio_map,
		ARRAY_SIZE(coconut_audio_map));
	if (errorCode) {
		goto init_exit;
	}

	errorCode = snd_soc_dapm_sync(codec);

init_exit:
	return errorCode;
}

/* coconut digital audio interface glue - connects codec <--> A2S */
static struct snd_soc_dai_link coconut_dai_link = {
	.name = "AK4642-DAI-LINK",
	.stream_name = "AK4642-STREAM",
	.cpu_dai = &ambarella_i2s_dai,
	.codec_dai = &ak4642_dai,
	.init = coconut_ak4642_init,
	.ops = &coconut_board_ops,
};

/* coconut audio machine driver */
static struct snd_soc_card snd_soc_card_coconut = {
	.name = "COCONUT",
	.platform = &ambarella_soc_platform,
	.dai_link = &coconut_dai_link,
	.num_links = 1,
};

/* coconut audio private data */
static struct ak4642_setup_data coconut_ak4642_setup = {
	.i2c_bus	= 0,
	.i2c_address	= 0x12,
	.rst_pin		= AK4642_RESET_PIN,
	.rst_delay	= AK4642_RESET_DELAY,
};

/* coconut audio subsystem */
static struct snd_soc_device coconut_snd_devdata = {
	.card = &snd_soc_card_coconut,
	.codec_dev = &soc_codec_dev_ak4642,
	.codec_data = &coconut_ak4642_setup,
};

static struct platform_device *coconut_snd_device;

static int __init coconut_board_init(void)
{
	int errorCode = 0;

	coconut_snd_device = platform_device_alloc("soc-audio", -1);
	if (!coconut_snd_device)
		return -ENOMEM;

	platform_set_drvdata(coconut_snd_device, &coconut_snd_devdata);
	coconut_snd_devdata.dev = &coconut_snd_device->dev;

	errorCode = platform_device_add(coconut_snd_device);
	if (errorCode) {
		goto coconut_board_init_exit;
	}

	return 0;

coconut_board_init_exit:
	platform_device_put(coconut_snd_device);
	return errorCode;
}

static void __exit coconut_board_exit(void)
{
	platform_device_unregister(coconut_snd_device);
}

module_init(coconut_board_init);
module_exit(coconut_board_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Amabrella Coconut Board with AK4642 Codec for ALSA");
MODULE_LICENSE("GPL");

