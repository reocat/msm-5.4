/*
 * ambarella_dummy.c  --  A2SAUC ALSA SoC Audio driver
 *
 * History:
 *	2008/10/17 - [Andrew Lu] created file
 *	2009/03/12 - [Cao Rongrong] Port to 2.6.27
 *	2009/06/10 - [Cao Rongrong] Port to 2.6.29
 *
 * Coryright (c) 2008-2009, Ambarella, Inc.
 * http://www.ambarella.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "ambarella_dummy.h"

static inline unsigned int ambdummy_codec_read(struct snd_soc_codec *codec,
	unsigned int reg)
{
	return 0;
}

static inline int ambdummy_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	return 0;
}

static int ambdummy_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int ambdummy_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	codec->bias_level = level;
	return 0;
}

#define AMBDUMMY_RATES SNDRV_PCM_RATE_8000_48000

#define AMBDUMMY_FORMATS SNDRV_PCM_FMTBIT_S16_LE

static struct snd_soc_dai_ops ambdummy_dai_ops = {
	.digital_mute = ambdummy_mute,
};

struct snd_soc_dai ambdummy_dai = {
	.name = "AMBARELLA_DUMMY_CODEC",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 6,
		.rates = AMBDUMMY_RATES,
		.formats = AMBDUMMY_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 6,
		.rates = AMBDUMMY_RATES,
		.formats = AMBDUMMY_FORMATS,},
	.ops = &ambdummy_dai_ops,
};
EXPORT_SYMBOL(ambdummy_dai);

static int ambdummy_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	ambdummy_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int ambdummy_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	ambdummy_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	ambdummy_set_bias_level(codec, codec->suspend_bias_level);

	return 0;
}
/*
 * initialize the A2AUC driver
 * register the mixer and dsp interfaces with the kernel
 */
static int ambdummy_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret = 0;

	codec->name = "AMBARELLA_DUMMY_CODEC";
	codec->owner = THIS_MODULE;
	codec->read = ambdummy_codec_read;
	codec->write = ambdummy_codec_write;
	codec->set_bias_level = ambdummy_set_bias_level;
	codec->dai = &ambdummy_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = 0;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "A2AUC: failed to create pcms\n");
		kfree(codec->reg_cache);
		return ret;
	}

	/* power on device */
	ambdummy_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return ret;

}

static struct snd_soc_device *ambdummy_socdev;

static int ambdummy_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	socdev->card->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	ambdummy_socdev = socdev;

	printk("DBG: AMBARELLA DUMMY CODEC init here!! \n");

	ret = ambdummy_init(socdev);

	return ret;
}

/* power down chip */
static int ambdummy_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	if (codec->control_data)
		ambdummy_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_ambdummy = {
	.probe = 	ambdummy_probe,
	.remove = 	ambdummy_remove,
	.suspend = 	ambdummy_suspend,
	.resume =	ambdummy_resume,
};

EXPORT_SYMBOL(soc_codec_dev_ambdummy);

static int __init ambarella_dummy_codec_init(void)
{
	return snd_soc_register_dai(&ambdummy_dai);
}
module_init(ambarella_dummy_codec_init);

static void __exit ambarella_dummy_codec_exit(void)
{
	snd_soc_unregister_dai(&ambdummy_dai);
}
module_exit(ambarella_dummy_codec_exit);

MODULE_LICENSE("GPL");

