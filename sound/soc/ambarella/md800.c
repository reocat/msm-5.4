/*
 * sound/soc/md800.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2011/10/19 - [Cao Rongrong] Created file
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <mach/hardware.h>
#include <plat/audio.h>

#include "../codecs/es8328.h"


#define MD800_SPK_ON    0
#define MD800_SPK_OFF   1

/* default: Speaker output */
static int md800_spk_func;
static int md800_spk_gpio = GPIO(139);
static int md800_spk_level = GPIO_HIGH;

static void md800_ext_control(struct snd_soc_codec *codec)
{
	int errorCode = 0;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	if (md800_spk_func == MD800_SPK_ON){
		errorCode = gpio_direction_output(md800_spk_gpio, !!md800_spk_level);
		if (errorCode < 0) {
			printk(KERN_ERR "Could not Set Spk-Ctrl GPIO high\n");
			goto err_exit;
		}
		mdelay(1);
		snd_soc_dapm_enable_pin(dapm, "Ext Spk");
	} else {
		errorCode = gpio_direction_output(md800_spk_gpio, !md800_spk_level);
		if (errorCode < 0) {
			printk(KERN_ERR "Could not Set Spk-Ctrl GPIO low\n");
			goto err_exit;
		}
		mdelay(1);
		snd_soc_dapm_disable_pin(dapm, "Ext Spk");
	}

	/* signal a DAPM event */
	snd_soc_dapm_sync(dapm);

err_exit:
	return;
}

static int md800_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	/* check the jack status at stream startup */
	md800_ext_control(codec);
	return 0;
}

static int md800_board_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
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
	errorCode = snd_soc_dai_set_sysclk(codec_dai, ES8328_SYSCLK, mclk, 0);
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



static struct snd_soc_ops md800_board_ops = {
	.startup = md800_startup,
	.hw_params = md800_board_hw_params,
};

static int md800_get_spk(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = md800_spk_func;
	return 0;
}

static int md800_set_spk(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);

	if (md800_spk_func == ucontrol->value.integer.value[0])
		return 0;

	md800_spk_func = ucontrol->value.integer.value[0];
	md800_ext_control(codec);
	return 1;
}

static int md800_hp_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int event)
{
	/* when hp enabled, do we need to disable speaker? */
	return 0;
}

static int md800_spk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int event)
{
	int errorCode = 0;

	if (SND_SOC_DAPM_EVENT_ON(event)){
		errorCode = gpio_direction_output(md800_spk_gpio, !!md800_spk_level);
		if (errorCode < 0) {
			printk(KERN_ERR "Could not Set Spk-Ctrl GPIO high\n");
			goto err_exit;
		}
	} else {
		errorCode = gpio_direction_output(md800_spk_gpio, !md800_spk_level);
		if (errorCode < 0) {
			printk(KERN_ERR "Could not Set Spk-Ctrl GPIO low\n");
			goto err_exit;
		}
	}
	mdelay(1);

err_exit:
	return errorCode;
}


/* tcc machine dapm widgets */
static const struct snd_soc_dapm_widget md800_dapm_widgets[] = {
    SND_SOC_DAPM_HP("Headphone Jack", md800_hp_event),
    SND_SOC_DAPM_MIC("Mic Jack", NULL),
    SND_SOC_DAPM_SPK("Ext Spk", md800_spk_event),
    SND_SOC_DAPM_LINE("Line Jack", NULL),
    SND_SOC_DAPM_HP("Headset Jack", NULL),
};

/* tcc machine audio map (connections to the codec pins) */
static const struct snd_soc_dapm_route md800_audio_map[] = {
	/* mic is connected to MICIN (via right channel of headphone jack) */
	{"MICIN", NULL, "Mic Jack"},

	/* headset Jack  - in = micin, out = LHPOUT*/
	{"Headset Jack", NULL, "LOUT1"},
	{"Headset Jack", NULL, "ROUT1"},

	/* headphone connected to LHPOUT1, RHPOUT1 */
	{"Headphone Jack", NULL, "LOUT1"},
	{"Headphone Jack", NULL, "ROUT1"},

	/* speaker connected to LOUT, ROUT */
	{"Ext Spk", NULL, "ROUT1"},
	{"Ext Spk", NULL, "LOUT1"},
};

static const char *spk_function[]  = {"On", "Off"};

static const struct soc_enum md800_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_function), spk_function),
};

static const struct snd_kcontrol_new es8328_md800_controls[] = {
	SOC_ENUM_EXT("Speaker Function", md800_enum[0], md800_get_spk , md800_set_spk),
};

static int md800_es8328_init(struct snd_soc_pcm_runtime *rtd)
{
	int errorCode = 0;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_enable_pin(dapm, "MICIN");

	/* Add md800 specific controls */
	snd_soc_add_controls(codec, es8328_md800_controls,
			ARRAY_SIZE(es8328_md800_controls));

	/* Add md800 specific widgets */
	errorCode = snd_soc_dapm_new_controls(dapm,
			md800_dapm_widgets,
			ARRAY_SIZE(md800_dapm_widgets));
	if (errorCode) {
		goto init_exit;
	}

	/* Set up md800 specific audio path md800_audio_map */
	errorCode = snd_soc_dapm_add_routes(dapm,
		md800_audio_map,
		ARRAY_SIZE(md800_audio_map));
	if (errorCode) {
		goto init_exit;
	}

	errorCode = snd_soc_dapm_sync(dapm);

init_exit:
	return errorCode;
}


/* md800 digital audio interface glue - connects codec <--> A2S */
static struct snd_soc_dai_link md800_dai_link = {
	.name = "ES8328",
	.stream_name = "ES8328-STREAM",
	.cpu_dai_name = "ambarella-i2s.0",
	.platform_name = "ambarella-pcm-audio",
	.codec_dai_name = "ES8328",
	.codec_name = "es8328-codec.0-0010",
	.init = md800_es8328_init,
	.ops = &md800_board_ops,
};


/* md800 audio machine driver */
static struct snd_soc_card snd_soc_card_md800 = {
	.name = "MD800",
	.dai_link = &md800_dai_link,
	.num_links = 1,
};

static struct platform_device *md800_snd_device;

static int __init md800_board_init(void)
{
	int errorCode = 0;

	errorCode = gpio_request(md800_spk_gpio, "Spk-Ctrl");
	if (errorCode < 0) {
		printk(KERN_ERR "Could not get Spk-Ctrl GPIO %d\n", md800_spk_gpio);
		goto md800_board_init_exit1;
	}

	md800_snd_device = platform_device_alloc("soc-audio", -1);
	if (!md800_snd_device)
		return -ENOMEM;

	platform_set_drvdata(md800_snd_device, &snd_soc_card_md800);

	errorCode = platform_device_add(md800_snd_device);
	if (errorCode) {
		platform_device_put(md800_snd_device);
		goto md800_board_init_exit0;
	}

	return 0;

md800_board_init_exit0:
	gpio_free(md800_spk_gpio);
md800_board_init_exit1:
	return errorCode;
}

static void __exit md800_board_exit(void)
{
	platform_device_unregister(md800_snd_device);
	gpio_free(md800_spk_gpio);
}


module_init(md800_board_init);
module_exit(md800_board_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("MD800 Board with ES8328 Codec for ALSA");
MODULE_LICENSE("GPL");

