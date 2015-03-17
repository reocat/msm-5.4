/*
 * sound/soc/s2lmelektra.c
 *
 * Author:
 *
 * History:
 *
 * Copyright (C) 2004-2016, Ambarella, Inc.
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
#include <linux/module.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <plat/audio.h>
#include "../codecs/wm8974_amb.h"

static unsigned int dai_fmt = 0;
module_param(dai_fmt, uint, 0644);
MODULE_PARM_DESC(dai_fmt, "DAI format.");

static int s2lmelektra_board_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static int s2lmelektra_board_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int errorCode = 0, mclk, mclk_divider, oversample, i2s_mode;

	switch (params_rate(params)) {
	case 8000:
		mclk = 12288000;
		mclk_divider = WM8974_MCLKDIV_6;
		oversample = AudioCodec_1536xfs;
		break;
	case 11025:
		mclk = 11289600;
		mclk_divider = WM8974_MCLKDIV_4;
		oversample = AudioCodec_1024xfs;
		break;
	case 12000:
		mclk = 12288000;
		mclk_divider = WM8974_MCLKDIV_4;
		oversample = AudioCodec_1024xfs;
		break;
	case 16000:
		mclk = 12288000;
		mclk_divider = WM8974_MCLKDIV_3;
		oversample = AudioCodec_768xfs;
		break;
	case 22050:
		mclk = 11289600;
		mclk_divider = WM8974_MCLKDIV_2;
		oversample = AudioCodec_512xfs;
		break;
	case 24000:
		mclk = 12288000;
		mclk_divider = WM8974_MCLKDIV_2;
		oversample = AudioCodec_512xfs;
		break;
	case 32000:
		mclk = 12288000;
		mclk_divider = WM8974_MCLKDIV_1_5;
		oversample = AudioCodec_384xfs;
		break;
	case 44100:
		mclk = 11289600;
		mclk_divider = WM8974_MCLKDIV_1;
		oversample = AudioCodec_256xfs;
		break;
	case 48000:
		mclk = 12288000;
		mclk_divider = WM8974_MCLKDIV_1;
		oversample = AudioCodec_256xfs;
		break;
	default:
		errorCode = -EINVAL;
		goto hw_params_exit;
	}

	if (dai_fmt == 0)
		i2s_mode = SND_SOC_DAIFMT_I2S;
	else
		i2s_mode = SND_SOC_DAIFMT_DSP_A;

	/* set the I2S system data format*/
	errorCode = snd_soc_dai_set_fmt(codec_dai,
		i2s_mode | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (errorCode < 0) {
		pr_err("can't set codec DAI configuration\n");
		goto hw_params_exit;
	}

	errorCode = snd_soc_dai_set_fmt(cpu_dai,
		i2s_mode | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (errorCode < 0) {
		pr_err("can't set cpu DAI configuration\n");
		goto hw_params_exit;
	}

	/* set the I2S system clock*/
	errorCode = snd_soc_dai_set_sysclk(codec_dai, WM8974_SYSCLK, mclk, 0);
	if (errorCode < 0) {
		pr_err("can't set codec MCLK configuration\n");
		goto hw_params_exit;
	}

	errorCode = snd_soc_dai_set_sysclk(cpu_dai, AMBARELLA_CLKSRC_ONCHIP, mclk, 0);
	if (errorCode < 0) {
		pr_err("can't set cpu MCLK configuration\n");
		goto hw_params_exit;
	}

	errorCode = snd_soc_dai_set_clkdiv(codec_dai, WM8974_MCLKDIV, mclk_divider);
	if (errorCode < 0) {
		pr_err("can't set codec MCLK/SF ratio\n");
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

static struct snd_soc_ops s2lmelektra_board_ops = {
	.startup =  s2lmelektra_board_startup,
	.hw_params =  s2lmelektra_board_hw_params,
};

/* s2lmelektra machine dapm widgets */
static const struct snd_soc_dapm_widget s2lmelektra_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic External", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};

/* s2lmelektra machine audio map (connections to wm8974 pins) */
static const struct snd_soc_dapm_route s2lmelektra_audio_map[] = {
	/* Mic External is connected to MICN, MICNP, AUX*/
	{"MICN", NULL, "Mic External"},
	{"MICP", NULL, "Mic External"},

	/* Speaker is connected to SPKOUTP, SPKOUTN */
	{"Speaker", NULL, "SPKOUTP"},
	{"Speaker", NULL, "SPKOUTN"},
};

static int s2lmelektra_wm8974_init(struct snd_soc_pcm_runtime *rtd)
{
	int errorCode = 0;
	return errorCode;
}

/* s2lmelektra digital audio interface glue - connects codec <--> A2S */
static struct snd_soc_dai_link s2lmelektra_dai_link = {
	.name = "WM8974",
	.stream_name = "WM8974-STREAM",
	.codec_dai_name = "wm8974-hifi",
	.init = s2lmelektra_wm8974_init,
	.ops = &s2lmelektra_board_ops,
};


/* s2lmelektra audio machine driver */
static struct snd_soc_card snd_soc_card_s2lmelektra = {
	.owner = THIS_MODULE,
	.dai_link = &s2lmelektra_dai_link,
	.num_links = 1,

	.dapm_widgets = s2lmelektra_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(s2lmelektra_dapm_widgets),
	.dapm_routes = s2lmelektra_audio_map,
	.num_dapm_routes = ARRAY_SIZE(s2lmelektra_audio_map),
};

static int s2lmelektra_soc_snd_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cpup_np, *codec_np;
	struct snd_soc_card *card = &snd_soc_card_s2lmelektra;
	int rval = 0;

	card->dev = &pdev->dev;
	if (snd_soc_of_parse_card_name(card, "amb,model")) {
		dev_err(&pdev->dev, "Card name is not provided\n");
		return -ENODEV;
	}

	cpup_np = of_parse_phandle(np, "amb,i2s-controllers", 0);
	codec_np = of_parse_phandle(np, "amb,audio-codec", 0);
	if (!cpup_np || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		return -EINVAL;
	}

	s2lmelektra_dai_link.codec_of_node = codec_np;
	s2lmelektra_dai_link.cpu_of_node = cpup_np;
	s2lmelektra_dai_link.platform_of_node = cpup_np;

	of_node_put(codec_np);
	of_node_put(cpup_np);

	rval = snd_soc_register_card(card);
	if (rval)
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", rval);

	return rval;
}

static int s2lmelektra_soc_snd_remove(struct platform_device *pdev)
{
	snd_soc_unregister_card(&snd_soc_card_s2lmelektra);
	return 0;
}

static const struct of_device_id s2lmelektra_dt_ids[] = {
	{ .compatible = "ambarella,s2lmelektra-wm8974", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s2lmelektra_dt_ids);

static struct platform_driver s2lmelektra_soc_snd_driver = {
	.driver = {
		.name = "snd_soc_card_s2lmelektra",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = s2lmelektra_dt_ids,
	},
	.probe = s2lmelektra_soc_snd_probe,
	.remove = s2lmelektra_soc_snd_remove,

};

module_platform_driver(s2lmelektra_soc_snd_driver);

MODULE_DESCRIPTION("Amabrella Board with WM8974 Codec for ALSA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("snd-soc-s2lmelektra");


