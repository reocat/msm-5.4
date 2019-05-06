#ifdef CONFIG_IMX6_GW_GOOD_BOARD

/*
 * Copyright (C) 2015-2016 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/control.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mfd/syscon.h>
#include "../codecs/nau8822.h"
#include "fsl_sai.h"

//add hui
#define DAI_NAME_SIZE	32
//
struct imx_nau8822_data {
//add hui
	struct snd_soc_dai_link dai;
//
	struct snd_soc_card card;
//add hui
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
//
	struct clk *codec_clk;
	unsigned int clk_frequency;
	bool is_codec_master;
	bool is_stream_in_use[2];
	bool is_stream_opened[2];
	struct regmap *gpr;
	unsigned int hp_det[2];
	u32 asrc_rate;
	u32 asrc_format;
};

struct imx_priv {
	enum of_gpio_flags hp_active_low;
	enum of_gpio_flags mic_active_low;
	bool is_headset_jack;
	struct snd_kcontrol *headphone_kctl;
	struct platform_device *pdev;
	struct platform_device *asrc_pdev;
	struct snd_card *snd_card;
};

static struct imx_priv card_priv;

static struct snd_soc_jack imx_hp_jack;
static struct snd_soc_jack_pin imx_hp_jack_pin = {
	.pin = "Headphone Jack",
	.mask = SND_JACK_HEADPHONE,
};
static struct snd_soc_jack_gpio imx_hp_jack_gpio = {
	.name = "headphone detect",
	.report = SND_JACK_HEADPHONE,
	.debounce_time = 250,
	.invert = 0,
};

static struct snd_soc_jack imx_mic_jack;
static struct snd_soc_jack_pin imx_mic_jack_pins = {
	.pin = "Mic Jack",
	.mask = SND_JACK_MICROPHONE,
};
static struct snd_soc_jack_gpio imx_mic_jack_gpio = {
	.name = "mic detect",
	.report = SND_JACK_MICROPHONE,
	.debounce_time = 250,
	.invert = 0,
};

static int hp_jack_status_check(void *data)
{
	struct imx_priv *priv = &card_priv;
	struct snd_soc_jack *jack = data;
	struct snd_soc_dapm_context *dapm = &jack->card->dapm;
	int hp_status, ret;

	hp_status = gpio_get_value(imx_hp_jack_gpio.gpio);

	if (hp_status != priv->hp_active_low) {
		snd_soc_dapm_disable_pin(dapm, "Ext Spk");
		if (priv->is_headset_jack) {
			snd_soc_dapm_enable_pin(dapm, "Mic Jack");
			snd_soc_dapm_disable_pin(dapm, "Main MIC");
		}
		ret = imx_hp_jack_gpio.report;
		snd_kctl_jack_report(priv->snd_card, priv->headphone_kctl, 1);
	} else {
		snd_soc_dapm_enable_pin(dapm, "Ext Spk");
		if (priv->is_headset_jack) {
			snd_soc_dapm_disable_pin(dapm, "Mic Jack");
			snd_soc_dapm_enable_pin(dapm, "Main MIC");
		}
		ret = 0;
		snd_kctl_jack_report(priv->snd_card, priv->headphone_kctl, 0);
	}

	return ret;
}

static int mic_jack_status_check(void *data)
{
	struct imx_priv *priv = &card_priv;
	struct snd_soc_jack *jack = data;
	struct snd_soc_dapm_context *dapm = &jack->card->dapm;
	int mic_status, ret;

	mic_status = gpio_get_value(imx_mic_jack_gpio.gpio);

	if (mic_status != priv->mic_active_low) {
		snd_soc_dapm_disable_pin(dapm, "Main MIC");
		ret = imx_mic_jack_gpio.report;
	} else {
		snd_soc_dapm_enable_pin(dapm, "Main MIC");
		ret = 0;
	}

	return ret;
}

static const struct snd_soc_dapm_widget imx_nau8822_dapm_widgets[] = {
	//SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_MIC("AMIC", NULL),
	SND_SOC_DAPM_MIC("DMIC", NULL),
};
//add hui
static const struct snd_soc_dapm_route imx_nau8822_dapm_route[] = {
	//{"Headphone Jack", NULL, "HPOUTL"},
	//{"Headphone Jack", NULL, "HPOUTR"},
	{"Ext Spk", NULL, "LSPK"},
	{"Ext Spk", NULL, "RSPK"},
	//{"AMIC", NULL, "MICBIAS"},
	//{"Left Input Mixer", "Mic Switch", "Mic Bias"},Left Input Mixer
	{"Mic Bias", NULL, "LMICN"},
	{"Mic Bias", NULL, "LMICP"},
};
//
//add hui
static int sample_rate = 44100;
static snd_pcm_format_t sample_format = SNDRV_PCM_FORMAT_S16_LE;
//
static int imx_nau8822_jack_init(struct snd_soc_card *card,
		struct snd_soc_jack *jack, struct snd_soc_jack_pin *pin,
		struct snd_soc_jack_gpio *gpio)
{
	int ret;

	ret = snd_soc_card_jack_new(card, pin->pin, pin->mask, jack, pin, 1);
	if (ret) {
		return ret;
	}

	ret = snd_soc_jack_add_gpios(jack, 1, gpio);
	if (ret)
		return ret;

	return 0;
}

static ssize_t headphone_show(struct device_driver *dev, char *buf)
{
	struct imx_priv *priv = &card_priv;
	int hp_status;

	/* Check if headphone is plugged in */
	hp_status = gpio_get_value(imx_hp_jack_gpio.gpio);

	if (hp_status != priv->hp_active_low)
		strcpy(buf, "Headphone\n");
	else
		strcpy(buf, "Speaker\n");

	return strlen(buf);
}

static ssize_t micphone_show(struct device_driver *dev, char *buf)
{
	struct imx_priv *priv = &card_priv;
	int mic_status;

	/* Check if headphone is plugged in */
	mic_status = gpio_get_value(imx_mic_jack_gpio.gpio);

	if (mic_status != priv->mic_active_low)
		strcpy(buf, "Mic Jack\n");
	else
		strcpy(buf, "Main MIC\n");

	return strlen(buf);
}

static DRIVER_ATTR_RO(headphone);
static DRIVER_ATTR_RO(micphone);

static int imx_nau8822_set_bias_level(struct snd_soc_card *card,
					struct snd_soc_dapm_context *dapm,
					enum snd_soc_bias_level level)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;
	struct imx_priv *priv = &card_priv;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	struct device *dev = &priv->pdev->dev;
	unsigned int pll_out;
	int ret;

	rtd = snd_soc_get_pcm_runtime(card, card->dai_link[0].name);
	codec_dai = rtd->codec_dai;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		if (dapm->bias_level == SND_SOC_BIAS_STANDBY) {
			if (sample_format == SNDRV_PCM_FORMAT_S24_LE)
				pll_out = sample_rate * 384;
			else
				pll_out = sample_rate * 256;

			ret = snd_soc_dai_set_pll(codec_dai, NAU8822_FLL,
					NAU8822_FLL_MCLK, data->clk_frequency,
					pll_out);
			if (ret < 0) {
				dev_err(dev, "failed to start FLL: %d\n", ret);
				return ret;
			}
			
			ret = snd_soc_dai_set_sysclk(codec_dai,
					NAU8822_SYSCLK_FLL, pll_out,
					SND_SOC_CLOCK_IN);
			if (ret < 0) {
				dev_err(dev, "failed to set SYSCLK: %d\n", ret);
				return ret;
			}
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level == SND_SOC_BIAS_PREPARE) {
			ret = snd_soc_dai_set_sysclk(codec_dai,
					NAU8822_SYSCLK_MCLK, data->clk_frequency,
					SND_SOC_CLOCK_IN);
			if (ret < 0) {
				dev_err(dev,
					"failed to switch away from FLL: %d\n",
					ret);
				return ret;
			}

			ret = snd_soc_dai_set_pll(codec_dai, NAU8822_FLL,
					0, 0, 0);
			if (ret < 0) {
				dev_err(dev, "failed to stop FLL: %d\n", ret);
				return ret;
			
			}
		}
		break;

	default:
		break;
	}
	return 0;
}
static int imx_hifi_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	unsigned int sample_rate = params_rate(params);
	unsigned int pll_out;

	pll_out = sample_rate * 256;
	sample_rate = params_rate(params);
	sample_format = params_format(params);

	snd_soc_dai_set_pll(codec_dai, NAU8822_FLL,
					NAU8822_FLL_MCLK, data->clk_frequency,
					pll_out);
	snd_soc_dai_set_sysclk(cpu_dai, 0, 0, SND_SOC_CLOCK_IN);
	snd_soc_dai_set_sysclk(codec_dai,
					NAU8822_SYSCLK_FLL, pll_out,
					SND_SOC_CLOCK_IN);
					

	return 0;
}

static int imx_hifi_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct device *dev = card->dev;
	int ret;

	data->is_stream_in_use[tx] = false;

	if (data->is_codec_master && !data->is_stream_in_use[!tx]) {
		ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS |
			SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF);
		if (ret)
			dev_warn(dev, "failed to set codec dai fmt: %d\n", ret);
	}

	return 0;
}

static u32 imx_nau8822_rates[] = { 8000, 16000, 32000, 48000 };
static struct snd_pcm_hw_constraint_list imx_nau8822_rate_constraints = {
	.count = ARRAY_SIZE(imx_nau8822_rates),
	.list = imx_nau8822_rates,
};

static int imx_hifi_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct fsl_sai *sai = dev_get_drvdata(cpu_dai->dev);
	int ret = 0;

	data->is_stream_opened[tx] = true;
	if (data->is_stream_opened[tx] != sai->is_stream_opened[tx] ||
	    data->is_stream_opened[!tx] != sai->is_stream_opened[!tx]) {
		data->is_stream_opened[tx] = false;
		return -EBUSY;
	}

	if (!data->is_codec_master) {
		ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE, &imx_nau8822_rate_constraints);
		if (ret)
			return ret;
	}

	ret = clk_prepare_enable(data->codec_clk);
	if (ret) {
		dev_err(card->dev, "Failed to enable MCLK: %d\n", ret);
		return ret;
	}

	return ret;
}

static void imx_hifi_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	clk_disable_unprepare(data->codec_clk);

	data->is_stream_opened[tx] = false;
}

static struct snd_soc_ops imx_hifi_ops = {
	.hw_params = imx_hifi_hw_params,
};

static int imx_nau8822_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;
	struct imx_priv *priv = &card_priv;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	struct device *dev = &priv->pdev->dev;
	int ret;

	rtd = snd_soc_get_pcm_runtime(card, card->dai_link[0].name);
	codec_dai = rtd->codec_dai;

	ret = snd_soc_dai_set_sysclk(codec_dai, NAU8822_SYSCLK_MCLK,
			data->clk_frequency, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(dev, "failed to set sysclk in %s\n", __func__);

	return ret;
}

static int be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			struct snd_pcm_hw_params *params)
{
	struct snd_soc_card *card = rtd->card;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	struct imx_priv *priv = &card_priv;
	struct snd_interval *rate;
	struct snd_mask *mask;

	if (!priv->asrc_pdev)
		return -EINVAL;

	rate = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	rate->max = rate->min = data->asrc_rate;

	mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	snd_mask_none(mask);
	snd_mask_set(mask, data->asrc_format);

	return 0;
}

static struct snd_soc_dai_link imx_nau8822_dai[] = {
	{
		.name = "HiFi",
		.stream_name = "HiFi",
		.codec_dai_name = "nau8822-hifi",
		.ops = &imx_hifi_ops,
	},
	{
		.name = "HiFi-ASRC-FE",
		.stream_name = "HiFi-ASRC-FE",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "HiFi-ASRC-BE",
		.stream_name = "HiFi-ASRC-BE",
		.codec_dai_name = "nau8822-hifi",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &imx_hifi_ops,
		.be_hw_params_fixup = be_hw_params_fixup,
	},
};

static int imx_nau8822_probe(struct platform_device *pdev)
{
	struct device_node *cpu_np, *codec_np = NULL;
	struct device_node *gpr_np;
	struct platform_device *cpu_pdev;
	struct imx_priv *priv = &card_priv;
	struct i2c_client *codec_dev;
	struct imx_nau8822_data *data;
	struct platform_device *asrc_pdev = NULL;
	struct device_node *asrc_np;
	int ret;

	printk("imx_nau8822_probe() entering");

	gpio_request(465, NULL);
	gpio_direction_output(465, 0);
	gpio_free(465);

	priv->pdev = pdev;

	cpu_np = of_parse_phandle(pdev->dev.of_node, "cpu-dai", 0);
	if (!cpu_np) {
		dev_err(&pdev->dev, "cpu dai phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	cpu_pdev = of_find_device_by_node(cpu_np);
	if (!cpu_pdev) {
		dev_err(&pdev->dev, "failed to find SAI platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev || !codec_dev->dev.driver) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	if (of_property_read_bool(pdev->dev.of_node, "codec-master"))
		data->is_codec_master = true;
//add hui
	data->codec_clk = devm_clk_get(&codec_dev->dev, "mclk");
	//data->codec_clk = devm_clk_get(&codec_dev->dev, NULL);
//
	if (IS_ERR(data->codec_clk)) {
		ret = PTR_ERR(data->codec_clk);
		dev_err(&pdev->dev, "failed to get codec clk: %d\n", ret);
		goto fail;
	}
//add hui
	data->clk_frequency = clk_get_rate(data->codec_clk);
	ret = clk_prepare_enable(data->codec_clk);
//

	gpr_np = of_parse_phandle(pdev->dev.of_node, "gpr", 0);
        if (gpr_np) {
		data->gpr = syscon_node_to_regmap(gpr_np);
		if (IS_ERR(data->gpr)) {
			ret = PTR_ERR(data->gpr);
			dev_err(&pdev->dev, "failed to get gpr regmap\n");
			goto fail;
		}

		/* set SAI2_MCLK_DIR to enable codec MCLK for imx7d */
		regmap_update_bits(data->gpr, 4, 1<<20, 1<<20);
	}

	of_property_read_u32_array(pdev->dev.of_node, "hp-det", data->hp_det, 2);

	asrc_np = of_parse_phandle(pdev->dev.of_node, "asrc-controller", 0);
	if (asrc_np) {
		asrc_pdev = of_find_device_by_node(asrc_np);
		priv->asrc_pdev = asrc_pdev;
	}

	data->dai.name = "HiFi";
	data->dai.stream_name = "HiFi";
	data->dai.codec_dai_name = "nau8822-hifi";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_dai_name = dev_name(&cpu_pdev->dev);
	data->dai.platform_of_node = cpu_np;
	data->dai.ops = &imx_hifi_ops;
	data->dai.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBM_CFM; 
//
	data->card.dev = &pdev->dev;
	
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto fail;
	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto fail;
//add hui
	data->card.num_links = 1;
//
	data->card.owner = THIS_MODULE;
//add hui
	data->card.dai_link = &data->dai;
//
	data->card.dapm_widgets = imx_nau8822_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_nau8822_dapm_widgets);
//add hui
	//add for dapm_route
	data->card.dapm_routes = imx_nau8822_dapm_route;
	data->card.num_dapm_routes = ARRAY_SIZE(imx_nau8822_dapm_route);
	//snd_soc_dapm_enable_pin(&priv->codec->dapm, "Ext Spk");
//
	data->card.late_probe = imx_nau8822_late_probe;
//add hui
	data->card.set_bias_level = imx_nau8822_set_bias_level;
//end
	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);
	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto fail;
	}
//addhui
	snd_soc_dapm_enable_pin(&data->card.dapm, "Ext Spk");
	of_node_put(cpu_np);
	of_node_put(codec_np);

	printk("imx_nau8822_probe() ok");

	return 0;

fail:
	if (cpu_np)
		of_node_put(cpu_np);
	if (codec_np)
		of_node_put(codec_np);

	return ret;
}

static int imx_nau8822_remove(struct platform_device *pdev)
{
//add hui
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);

	if (!IS_ERR(data->codec_clk))
		clk_disable_unprepare(data->codec_clk);

	return 0;
}

static const struct of_device_id imx_nau8822_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-nau8822", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_nau8822_dt_ids);

static struct platform_driver imx_nau8822_driver = {
	.driver = {
		.name = "imx-nau8822",
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_nau8822_dt_ids,
	},
	.probe = imx_nau8822_probe,
	.remove = imx_nau8822_remove,
};
module_platform_driver(imx_nau8822_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale i.MX nau8822 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-nau8822");

#else
/*
 * Copyright 2013 Freescale Semiconductor, Inc.
 *
 * Based on imx-sgtl5000.c
 * Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2012 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/consumer.h>

#include "../codecs/nau8822.h"
#include "imx-audmux.h"

#define DAI_NAME_SIZE	32

struct imx_nau8822_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	struct clk *codec_clk;
	unsigned int clk_frequency;
};

struct imx_priv {
	struct platform_device *pdev;
};
static struct imx_priv card_priv;

static const struct snd_soc_dapm_widget imx_nau8822_dapm_widgets[] = {
	//SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_MIC("AMIC", NULL),
	SND_SOC_DAPM_MIC("DMIC", NULL),
};
//add for dapm_route
static const struct snd_soc_dapm_route imx_nau8822_dapm_route[] = {
	//{"Headphone Jack", NULL, "HPOUTL"},
	//{"Headphone Jack", NULL, "HPOUTR"},
	{"Ext Spk", NULL, "LSPK"},
	{"Ext Spk", NULL, "RSPK"},
	//{"AMIC", NULL, "MICBIAS"},
	//{"Left Input Mixer", "Mic Switch", "Mic Bias"},Left Input Mixer
	{"Mic Bias", NULL, "LMICN"},
	{"Mic Bias", NULL, "LMICP"},
};

static int sample_rate = 44100;
static snd_pcm_format_t sample_format = SNDRV_PCM_FORMAT_S16_LE;

static int imx_hifi_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	sample_rate = params_rate(params);
	sample_format = params_format(params);

	return 0;
}

static struct snd_soc_ops imx_hifi_ops = {
	.hw_params = imx_hifi_hw_params,
};

static int imx_nau8822_set_bias_level(struct snd_soc_card *card,
					struct snd_soc_dapm_context *dapm,
					enum snd_soc_bias_level level)
{
    struct snd_soc_pcm_runtime *rtd = list_first_entry(
		&card->rtd_list, struct snd_soc_pcm_runtime, list);
        
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct imx_priv *priv = &card_priv;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	struct device *dev = &priv->pdev->dev;
	unsigned int pll_out;
	int ret;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		if (dapm->bias_level == SND_SOC_BIAS_STANDBY) {
			if (sample_format == SNDRV_PCM_FORMAT_S24_LE)
				pll_out = sample_rate * 384;
			else
				pll_out = sample_rate * 256;

			ret = snd_soc_dai_set_pll(codec_dai, NAU8822_FLL,
					NAU8822_FLL_MCLK, data->clk_frequency,
					pll_out);
			if (ret < 0) {
				dev_err(dev, "failed to start FLL: %d\n", ret);
				return ret;
			}
			
			ret = snd_soc_dai_set_sysclk(codec_dai,
					NAU8822_SYSCLK_FLL, pll_out,
					SND_SOC_CLOCK_IN);
			if (ret < 0) {
				dev_err(dev, "failed to set SYSCLK: %d\n", ret);
				return ret;
			}
		}
		break;

	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level == SND_SOC_BIAS_PREPARE) {
			ret = snd_soc_dai_set_sysclk(codec_dai,
					NAU8822_SYSCLK_MCLK, data->clk_frequency,
					SND_SOC_CLOCK_IN);
			if (ret < 0) {
				dev_err(dev,
					"failed to switch away from FLL: %d\n",
					ret);
				return ret;
			}

			ret = snd_soc_dai_set_pll(codec_dai, NAU8822_FLL,
					0, 0, 0);
			if (ret < 0) {
				dev_err(dev, "failed to stop FLL: %d\n", ret);
				return ret;
			
			}
		}
		break;

	default:
		break;
	}

	return 0;
}

static int imx_nau8822_late_probe(struct snd_soc_card *card)
{
    struct snd_soc_pcm_runtime *rtd = list_first_entry(
		&card->rtd_list, struct snd_soc_pcm_runtime, list);
    struct snd_soc_dai *codec_dai = rtd->codec_dai;
    
	struct imx_priv *priv = &card_priv;
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);
	struct device *dev = &priv->pdev->dev;
	int ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, NAU8822_SYSCLK_MCLK,
			data->clk_frequency, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(dev, "failed to set sysclk in %s\n", __func__);

	return ret;
}
#include <linux/gpio.h>
static int imx_nau8822_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np, *codec_np;
	struct platform_device *ssi_pdev;
	struct imx_priv *priv = &card_priv;
	struct i2c_client *codec_dev;
	struct imx_nau8822_data *data;
	int int_port, ext_port;
	int ret;

	gpio_request(465, NULL);
	gpio_direction_output(465, 0);
	gpio_free(465);
	
	priv->pdev = pdev;

	ret = of_property_read_u32(np, "mux-int-port", &int_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-int-port missing or invalid\n");
		return ret;
	}
	ret = of_property_read_u32(np, "mux-ext-port", &ext_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-ext-port missing or invalid\n");
		return ret;
	}

	/*
	 * The port numbering in the hardware manual starts at 1, while
	 * the audmux API expects it starts at 0.
	 */
	int_port--;
	ext_port--;
	ret = imx_audmux_v2_configure_port(int_port,
			IMX_AUDMUX_V2_PTCR_SYN |
			IMX_AUDMUX_V2_PTCR_TFSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TCSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TFSDIR |
			IMX_AUDMUX_V2_PTCR_TCLKDIR,
			IMX_AUDMUX_V2_PDCR_RXDSEL(ext_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux internal port setup failed\n");
		return ret;
	}
	ret = imx_audmux_v2_configure_port(ext_port,
			IMX_AUDMUX_V2_PTCR_SYN,
			IMX_AUDMUX_V2_PDCR_RXDSEL(int_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux external port setup failed\n");
		return ret;
	}

	ssi_np = of_parse_phandle(pdev->dev.of_node, "ssi-controller", 0);
	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!ssi_np || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	ssi_pdev = of_find_device_by_node(ssi_np);
	if (!ssi_pdev) {
		dev_err(&pdev->dev, "failed to find SSI platform device\n");
		ret = -EINVAL;
		goto fail;
	}
	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev || !codec_dev->dev.driver) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->codec_clk = devm_clk_get(&codec_dev->dev, NULL);
	if (IS_ERR(data->codec_clk)) {
		ret = PTR_ERR(data->codec_clk);
		dev_err(&codec_dev->dev, "failed to get codec clk: %d\n", ret);
		goto fail;
	}

	data->clk_frequency = clk_get_rate(data->codec_clk);
	ret = clk_prepare_enable(data->codec_clk);
	if (ret) {
		dev_err(&codec_dev->dev, "failed to enable codec clk: %d\n", ret);
		goto fail;
	}

	data->dai.name = "HiFi";
	data->dai.stream_name = "HiFi";
	data->dai.codec_dai_name = "nau8822-hifi";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_dai_name = dev_name(&ssi_pdev->dev);
	data->dai.platform_of_node = ssi_np;
	data->dai.ops = &imx_hifi_ops;
	data->dai.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			    SND_SOC_DAIFMT_CBM_CFM;

	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto clk_fail;
	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto clk_fail;
	data->card.num_links = 1;
	data->card.owner = THIS_MODULE;
	data->card.dai_link = &data->dai;
	data->card.dapm_widgets = imx_nau8822_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_nau8822_dapm_widgets);
	
	//add for dapm_route
	data->card.dapm_routes = imx_nau8822_dapm_route;
	data->card.num_dapm_routes = ARRAY_SIZE(imx_nau8822_dapm_route);
	//snd_soc_dapm_enable_pin(&priv->codec->dapm, "Ext Spk");

	data->card.late_probe = imx_nau8822_late_probe;
	data->card.set_bias_level = imx_nau8822_set_bias_level;

	platform_set_drvdata(pdev, &data->card);
	snd_soc_card_set_drvdata(&data->card, data);

	ret = devm_snd_soc_register_card(&pdev->dev, &data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto clk_fail;
	}
	snd_soc_dapm_enable_pin(&data->card.dapm, "Ext Spk");
	of_node_put(ssi_np);
	of_node_put(codec_np);

	return 0;

clk_fail:
	clk_disable_unprepare(data->codec_clk);
fail:
	of_node_put(ssi_np);
	of_node_put(codec_np);

	return ret;
}

static int imx_nau8822_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct imx_nau8822_data *data = snd_soc_card_get_drvdata(card);

	if (!IS_ERR(data->codec_clk))
		clk_disable_unprepare(data->codec_clk);

	return 0;
}

static const struct of_device_id imx_nau8822_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-nau8822", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_nau8822_dt_ids);

static struct platform_driver imx_nau8822_driver = {
	.driver = {
		.name = "imx-nau8822",
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_nau8822_dt_ids,
	},
	.probe = imx_nau8822_probe,
	.remove = imx_nau8822_remove,
};
module_platform_driver(imx_nau8822_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("Freescale i.MX nau8822 ASoC machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-nau8822");
#endif
