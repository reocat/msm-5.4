/*
 * sound/soc/a2sipcam.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2009/07/01 - [Cao Rongrong] Created file
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

#include "ambarella_pcm.h"
#include "ambarella_i2s.h"
#include "../codecs/cs42l51.h"


#define CS42L51_RESET_PIN	52
#define CS42L51_RESET_DELAY	3

static int a2sipcam_board_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static int a2sipcam_board_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int errorCode = 0, amb_mclk, mclk, oversample;

	switch (params_rate(params)) {
	case 8000:
		amb_mclk = AudioCodec_6_144;
		mclk = 6144000;
		oversample = AudioCodec_768xfs;
		break;
	case 11025:
		amb_mclk = AudioCodec_8_4672M;
		mclk = 8467200;
		oversample = AudioCodec_768xfs;
		break;
	case 12000:
		amb_mclk = AudioCodec_9_216M;
		mclk = 9216000;
		oversample = AudioCodec_768xfs;
		break;
	case 16000:
		amb_mclk = AudioCodec_8_192M;
		mclk = 8192000;
		oversample = AudioCodec_512xfs;
		break;
	case 22050:
		amb_mclk = AudioCodec_11_2896M;
		mclk = 11289600;
		oversample = AudioCodec_512xfs;
		break;
	case 24000:
		amb_mclk = AudioCodec_12_288M;
		mclk = 1228800;
		oversample = AudioCodec_512xfs;
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
	errorCode = snd_soc_dai_set_sysclk(codec_dai, CS42L51_SYSCLK, mclk, 0);
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


static struct snd_soc_ops a2sipcam_board_ops = {
	.startup = a2sipcam_board_startup,
	.hw_params = a2sipcam_board_hw_params,
};

/* a2sipcam machine dapm widgets */
static const struct snd_soc_dapm_widget a2sipcam_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
};

/* a2sipcam machine audio map (connections to cs42l51 pins) */
static const struct snd_soc_dapm_route a2sipcam_audio_map[] = {
	/* Line In is connected to LLIN1, RLIN1 */
	{"MICIN1", NULL, "Mic Jack"},
	{"LLIN1", NULL, "Line In"},
	{"RLIN1", NULL, "Line In"},

	/* Line Out is connected to LLOUT, RLOUT */
	{"Line Out", NULL, "LHPOUT"},
	{"Line Out", NULL, "RHPOUT"},
};


static int a2sipcam_cs42l51_init(struct snd_soc_codec *codec)
{
	int errorCode = 0;

	/* not connected */
	snd_soc_dapm_nc_pin(codec, "LLIN2");
	snd_soc_dapm_nc_pin(codec, "RLIN2");

	/* Add a2sipcam specific widgets */
	errorCode = snd_soc_dapm_new_controls(codec,
		a2sipcam_dapm_widgets,
		ARRAY_SIZE(a2sipcam_dapm_widgets));
	if (errorCode) {
		goto init_exit;
	}

	/* Set up a2sipcam specific audio path a2sipcam_audio_map */
	errorCode = snd_soc_dapm_add_routes(codec,
		a2sipcam_audio_map,
		ARRAY_SIZE(a2sipcam_audio_map));
	if (errorCode) {
		goto init_exit;
	}

	errorCode = snd_soc_dapm_sync(codec);

init_exit:
	return errorCode;
}

/* a2sipcam digital audio interface glue - connects codec <--> A2S */
static struct snd_soc_dai_link a2sipcam_dai_link = {
	.name = "CS42L51-DAI-LINK",
	.stream_name = "CS42L51-STREAM",
	.cpu_dai = &ambarella_i2s_dai,
	.codec_dai = &cs42l51_dai,
	.init = a2sipcam_cs42l51_init,
	.ops = &a2sipcam_board_ops,
};

/* a2sipcam audio machine driver */
static struct snd_soc_card snd_soc_card_a2sipcam = {
	.name = "A2SIPCAM",
	.platform = &ambarella_soc_platform,
	.dai_link = &a2sipcam_dai_link,
	.num_links = 1,
};

/* a2sipcam audio private data */
static struct cs42l51_setup_data a2sipcam_cs42l51_setup = {
	.i2c_bus	= 0,
	.i2c_address	= 0x4a,
	.rst_pin		= CS42L51_RESET_PIN,
	.rst_delay	= CS42L51_RESET_DELAY,
};

/* a2sipcam audio subsystem */
static struct snd_soc_device a2sipcam_snd_devdata = {
	.card = &snd_soc_card_a2sipcam,
	.codec_dev = &soc_codec_dev_cs42l51,
	.codec_data = &a2sipcam_cs42l51_setup,
};

static struct platform_device *a2sipcam_snd_device;

static int __init a2sipcam_board_init(void)
{
	int errorCode = 0;

	a2sipcam_snd_device = platform_device_alloc("soc-audio", -1);
	if (!a2sipcam_snd_device)
		return -ENOMEM;

	platform_set_drvdata(a2sipcam_snd_device, &a2sipcam_snd_devdata);
	a2sipcam_snd_devdata.dev = &a2sipcam_snd_device->dev;

	errorCode = platform_device_add(a2sipcam_snd_device);
	if (errorCode) {
		goto a2sipcam_board_init_exit;
	}

	return 0;

a2sipcam_board_init_exit:
	platform_device_put(a2sipcam_snd_device);
	return errorCode;
}

static void __exit a2sipcam_board_exit(void)
{
	platform_device_unregister(a2sipcam_snd_device);
}

module_init(a2sipcam_board_init);
module_exit(a2sipcam_board_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Amabrella A2SIPCAM Board with CS42L51 Codec for ALSA");
MODULE_LICENSE("GPL");

