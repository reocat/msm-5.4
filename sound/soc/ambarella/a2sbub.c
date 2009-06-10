/*
 * sound/soc/a2sbub.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * History:
 *	2009/03/05 - [Cao Rongrong] Created file
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

#include "ambarella_pcm.h"
#include "ambarella_i2s.h"
#include "../codecs/adav803.h"


static int a2sbub_board_startup(struct snd_pcm_substream *substream)
{
//	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct snd_soc_codec *codec = rtd->socdev->codec;

	return 0;
}

static int a2sbub_board_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int rval = 0;

	/* set codec DAI configuration */
	rval = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (rval < 0) {
		printk(KERN_ERR "can't set codec DAI configuration\n");
		goto hw_params_exit;
	}

	/* set cpu DAI configuration */
	rval = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (rval < 0) {
		printk(KERN_ERR "can't set cpu DAI configuration\n");
		goto hw_params_exit;
	}

	/* set the codec system clock for DAC and ADC */
	rval = snd_soc_dai_set_sysclk(codec_dai, 0, 0, SND_SOC_CLOCK_IN);
	if (rval < 0){
		printk(KERN_ERR "can't set codec DAI clock\n");
		goto hw_params_exit;
	}

hw_params_exit:
	return rval;
}


static struct snd_soc_ops a2sbub_board_ops = {
	.startup = a2sbub_board_startup,
	.hw_params = a2sbub_board_hw_params,
};

/* a2sbub machine dapm widgets */
static const struct snd_soc_dapm_widget a2sbub_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("Line In", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
};

/* a2sbub machine audio map (connections to adav803 pins) */
static const struct snd_soc_dapm_route a2sbub_audio_map[] = {
	/* Line Out is connected to LLOUT, RLOUT */
	{"Line Out", NULL, "LLOUT"},
	{"Line Out", NULL, "RLOUT"},

	/* Line In is connected to LLIN, RLIN */
	{"LLIN", NULL, "Line In"},
	{"RLIN", NULL, "Line In"},
};


static int a2sbub_adav803_init(struct snd_soc_codec *codec)
{
	int rval = 0;

	/* Add a2sbub specific widgets */
	rval = snd_soc_dapm_new_controls(codec,
		a2sbub_dapm_widgets,
		ARRAY_SIZE(a2sbub_dapm_widgets));
	if (rval) {
		goto init_exit;
	}

	/* Set up a2sbub specific audio path a2sbub_audio_map */
	rval = snd_soc_dapm_add_routes(codec,
		a2sbub_audio_map,
		ARRAY_SIZE(a2sbub_audio_map));
	if (rval) {
		goto init_exit;
	}

	rval = snd_soc_dapm_sync(codec);

init_exit:
	return rval;
}

/* a2sbub digital audio interface glue - connects codec <--> A2S */
static struct snd_soc_dai_link a2sbub_dai_link = {
	.name = "ADAV803-DAI-LINK",
	.stream_name = "ADAV803-STREAM",
	.cpu_dai = &ambarella_i2s_dai,
	.codec_dai = &adav803_dai,
	.init = a2sbub_adav803_init,
	.ops = &a2sbub_board_ops,
};

/* a2sbub audio machine driver */
static struct snd_soc_machine snd_soc_machine_a2sbub = {
	.name = "A2SBUB",
	.dai_link = &a2sbub_dai_link,
	.num_links = 1,
};

/* a2sbub audio private data */
static struct adav803_setup_data a2sbub_adav803_setup = {
	.i2c_bus = 0,
	.i2c_address = 0x10,
};

/* a2sbub audio subsystem */
static struct snd_soc_device a2sbub_snd_devdata = {
	.machine = &snd_soc_machine_a2sbub,
	.platform = &ambarella_soc_platform,
	.codec_dev = &soc_codec_dev_adav803,
	.codec_data = &a2sbub_adav803_setup,
};

static struct platform_device *a2sbub_snd_device;

static int __init a2sbub_board_init(void)
{
	int rval = 0;

	a2sbub_snd_device = platform_device_alloc("soc-audio", -1);
	if (!a2sbub_snd_device)
		return -ENOMEM;

	platform_set_drvdata(a2sbub_snd_device, &a2sbub_snd_devdata);
	a2sbub_snd_devdata.dev = &a2sbub_snd_device->dev;

	rval = platform_device_add(a2sbub_snd_device);
	if (rval) {
		platform_device_del(a2sbub_snd_device);
		platform_device_put(a2sbub_snd_device);
		goto a2sbub_board_init_exit;
	}

a2sbub_board_init_exit:
	return rval;
}

static void __exit a2sbub_board_exit(void)
{
	platform_device_unregister(a2sbub_snd_device);
}

module_init(a2sbub_board_init);
module_exit(a2sbub_board_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Amabrella A2SBUB Board with ADAV803 Codec for ALSA");
MODULE_LICENSE("GPL");

