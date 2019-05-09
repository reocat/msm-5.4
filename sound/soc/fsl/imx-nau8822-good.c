/*
 * Copyright (C) 2015-2016 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Angelo Dureghello <angelo.dureghello@timesys.com>
 *
 * Differentiating Alsa platform driver,
 * module imx-nau8822-good.c    variant for the good board
 * module imx-nau8822.c         variant for best and better
 *
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
	{ .compatible = "fsl,imx-audio-nau8822-good", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_nau8822_dt_ids);

static struct platform_driver imx_nau8822_driver = {
	.driver = {
		.name = "imx-nau8822-good",
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
MODULE_ALIAS("platform:imx-nau8822-good");
