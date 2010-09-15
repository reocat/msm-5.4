/*
 * sound/soc/dummy.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 * History:
 *	2009/03/12 - [Cao Rongrong] Created file
 *	2009/06/10 - [Cao Rongrong] Port to 2.6.29
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
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <mach/hardware.h>
#include <plat/audio.h>

#include "ambarella_pcm.h"
#include "ambarella_i2s.h"
#include "../codecs/ambarella_dummy.h"

static unsigned int dummy_dai_fmt = 0;
module_param(dummy_dai_fmt, uint, 0644);
MODULE_PARM_DESC(dummy_dai_fmt, "DAI format.");

static unsigned int dummy_disable_codec = 0;
module_param(dummy_disable_codec, uint, 0644);
MODULE_PARM_DESC(dummy_disable_codec, "Disable External Codec.");

static unsigned int dummy_pwr_pin = 12;
module_param(dummy_pwr_pin, uint, 0644);
MODULE_PARM_DESC(dummy_pwr_pin, "External Codec Power Pin.");


static int ambarella_dummy_board_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int errorCode = 0, mclk, oversample, i2s_mode;

	switch (params_rate(params)) {
	case 8000:
		mclk = AudioCodec_4_096M;
		oversample = AudioCodec_512xfs;
		break;
	case 11025:
		mclk = AudioCodec_5_6448M;
		oversample = AudioCodec_512xfs;
		break;
	case 16000:
		mclk = AudioCodec_4_096M;
		oversample = AudioCodec_256xfs;
		break;
	case 22050:
		mclk = AudioCodec_5_6448M;
		oversample = AudioCodec_256xfs;
		break;
	case 32000:
		mclk = AudioCodec_8_192M;
		oversample = AudioCodec_256xfs;
		break;
	case 44100:
		mclk = AudioCodec_11_2896M;
		oversample = AudioCodec_256xfs;
		break;
	case 48000:
		mclk = AudioCodec_12_288M;
		oversample = AudioCodec_256xfs;
		break;
	default:
		errorCode = -EINVAL;
		goto hw_params_exit;
	}

	if (dummy_dai_fmt == 0)
		i2s_mode = SND_SOC_DAIFMT_I2S;
	else
		i2s_mode = SND_SOC_DAIFMT_DSP_A;

	/* set the I2S system data format*/
	errorCode = snd_soc_dai_set_fmt(cpu_dai,
		i2s_mode | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (errorCode < 0) {
		printk(KERN_ERR "can't set cpu DAI configuration\n");
		goto hw_params_exit;
	}

	/* set the I2S system clock*/
	errorCode = snd_soc_dai_set_sysclk(cpu_dai, AMBARELLA_CLKSRC_ONCHIP, mclk, 0);
	if (errorCode < 0) {
		printk(KERN_ERR "can't set cpu MCLK configuration\n");
		goto hw_params_exit;
	}

	errorCode = snd_soc_dai_set_clkdiv(cpu_dai, AMBARELLA_CLKDIV_LRCLK, oversample);
	if (errorCode < 0) {
		printk(KERN_ERR "can't set cpu MCLK/SF ratio\n");
		goto hw_params_exit;
	}

hw_params_exit:
	return errorCode;
}


static struct snd_soc_ops ambarella_dummy_board_ops = {
	.hw_params = ambarella_dummy_board_hw_params,
};


/* ambarella_dummy board machine dapm widgets */
static const struct snd_soc_dapm_widget ambarella_dummy_dapm_widgets[] = {

};

/* ambarella_dummy board machine audio map (connections to the a2auc pins) */
static const struct snd_soc_dapm_route ambarella_dummy_audio_map[] = {

};

static const struct snd_kcontrol_new ambarella_dummy_controls[] = {

};

static int ambarella_dummy_init(struct snd_soc_codec *codec)
{
	int errorCode = 0, i;

	/* Add ambarella_dummy board specific controls */
	for (i = 0; i < ARRAY_SIZE(ambarella_dummy_controls); i++) {
		errorCode = snd_ctl_add(codec->card,
			snd_soc_cnew(&ambarella_dummy_controls[i],codec, NULL));
		if (errorCode < 0)
			goto init_exit;
	}

	/* Add ambarella_dummy board specific widgets */
	errorCode = snd_soc_dapm_new_controls(codec,
		ambarella_dummy_dapm_widgets,
		ARRAY_SIZE(ambarella_dummy_dapm_widgets));
	if (errorCode) {
		goto init_exit;
	}

	/* Set up ambarella_dummy board specific audio path ambarella_dummy_audio_map */
	errorCode = snd_soc_dapm_add_routes(codec,
		ambarella_dummy_audio_map,
		ARRAY_SIZE(ambarella_dummy_audio_map));
	if (errorCode) {
		goto init_exit;
	}

	errorCode = snd_soc_dapm_sync(codec);

init_exit:
	return errorCode;
}

static struct snd_soc_dai_link ambarella_dummy_dai_link = {
	.name = "AMB-DUMMY-DAI-LINK",
	.stream_name = "AMB-DUMMY-STREAM",
	.cpu_dai = &ambarella_i2s_dai,
	.codec_dai = &ambdummy_dai,
	.init = ambarella_dummy_init,
	.ops = &ambarella_dummy_board_ops,
};

static struct snd_soc_card snd_soc_card_ambarella_dummy = {
	.name = "ambarella_dummy",
	.platform = &ambarella_soc_platform,
	.dai_link = &ambarella_dummy_dai_link,
	.num_links = 1,
};

static struct snd_soc_device ambarella_dummy_snd_devdata = {
	.card = &snd_soc_card_ambarella_dummy,
	.codec_dev = &soc_codec_dev_ambdummy,
};

static struct platform_device *ambarella_dummy_snd_device;

static int __init ambarella_dummy_board_init(void)
{
	int errorCode = 0;

	ambarella_dummy_snd_device =
		platform_device_alloc("soc-audio", -1);
	if (!ambarella_dummy_snd_device) {
		errorCode = -ENOMEM;
		goto ambarella_dummy_board_init_exit;
	}

	platform_set_drvdata(ambarella_dummy_snd_device,
		&ambarella_dummy_snd_devdata);
	ambarella_dummy_snd_devdata.dev =
		&ambarella_dummy_snd_device->dev;

	errorCode = platform_device_add(ambarella_dummy_snd_device);
	if (errorCode)
		goto ambarella_dummy_board_init_exit;

	if (dummy_disable_codec) {
		errorCode = gpio_request(dummy_pwr_pin, "dummy_disable_codec");
		if (errorCode < 0) {
			pr_err("Request dummy_disable_codec GPIO(%d) failed\n",
				dummy_pwr_pin);
			goto ambarella_dummy_board_init_exit;
		}

		gpio_direction_output(dummy_pwr_pin, GPIO_LOW);
	}

	return 0;

ambarella_dummy_board_init_exit:
	platform_device_del(ambarella_dummy_snd_device);
	platform_device_put(ambarella_dummy_snd_device);

	return errorCode;
}

static void __exit ambarella_dummy_board_exit(void)
{
	if (dummy_disable_codec)
		gpio_free(dummy_pwr_pin);

	platform_device_unregister(ambarella_dummy_snd_device);
}

module_init(ambarella_dummy_board_init);
module_exit(ambarella_dummy_board_exit);

MODULE_AUTHOR("Anthony Ginger <hfjiang@ambarella.com>");
MODULE_DESCRIPTION("Amabrella A2 Board with internal Codec for ALSA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("snd-soc-a2bub");

