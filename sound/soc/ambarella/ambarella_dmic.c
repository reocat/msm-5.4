/*
 * sound/soc/ambarella_i2s.c
 *
 * History:
 *	2016/07/13 - [XianqingZheng] created file
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
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/io.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>
#include <plat/dma.h>
#include <plat/dmic.h>
#include "ambarella_pcm.h"
#include "ambarella_dmic.h"
#include <linux/mfd/syscon.h>

static const struct snd_soc_dapm_widget dmic_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("DMIC AIF", "Capture", 0,
			     SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_INPUT("DMic"),
};

static const struct snd_soc_dapm_route intercon[] = {
	{"DMIC AIF", NULL, "DMic"},
};

static int ambarella_dmic_enable(struct amb_dmic_priv *priv_data)
{
	writel_relaxed(1, priv_data->regbase + DMIC_ENABLE_OFFSET);
	return 0;
}

static int ambarella_dmic_disbale(struct amb_dmic_priv *priv_data)
{
	writel_relaxed(0, priv_data->regbase + DMIC_ENABLE_OFFSET);
	return 0;
}

static int ambarella_dmic_clock(struct amb_dmic_priv *priv_data, u32 mclk, u32 rate)
{
	u32 dec_factor0 = 31, dec_factor1 = 1, dec_fs, fdiv;
	u32 val;

	dec_fs = priv_data->mclk/rate -1;
	fdiv = (dec_fs + 1) / ((dec_factor0 + 1) * (dec_factor1 + 1));
	dec_fs = (dec_fs << 16) | (dec_factor0 | (dec_factor1 << 8));

	writel_relaxed(dec_fs, priv_data->regbase + DECIMATION_FACTOR_OFFSET);
	writel_relaxed(fdiv, priv_data->regbase + DMIC_CLK_DIV_OFFSET);

	val = (1 << 16) | 0x1;
	/*set the phase for left and right channel*/
	writel_relaxed(val, priv_data->regbase + DMIC_DATA_PHASE_OFFSET);

	return 0;
}

static int ambarella_dmic_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct amb_dmic_priv *priv_data = snd_soc_dai_get_drvdata(dai);
	u32 rate;

	/*Reset the ADC Data Path*/
	writel_relaxed(0, priv_data->regbase + AUDIO_CODEC_DP_RESET_OFFSET);
	writel_relaxed(1, priv_data->regbase + AUDIO_CODEC_DP_RESET_OFFSET);
	writel_relaxed(0, priv_data->regbase + AUDIO_CODEC_DP_RESET_OFFSET);


	rate = params_rate(params);
	ambarella_dmic_clock(priv_data, priv_data->mclk, rate);

	return 0;
}

static int ambarella_dmic_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct amb_dmic_priv *priv_data = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		regmap_update_bits(priv_data->reg_scr, 0xc, 0x01000000, 0x01000000);
		ambarella_dmic_enable(priv_data);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		regmap_update_bits(priv_data->reg_scr, 0xc, 0x01000000, 0x00000000);
		ambarella_dmic_disbale(priv_data);
		break;
	default:
		break;
	}

	return 0;
}

/*
 * Set Ambarella I2S DAI format
 */
static int ambarella_dmic_set_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	return 0;
}

static int ambarella_dmic_set_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct amb_dmic_priv *priv_data = snd_soc_dai_get_drvdata(dai);

	priv_data->mclk = freq;
	return 0;
}

static const struct snd_soc_dai_ops ambarella_dmic_dai_ops = {
	.hw_params = ambarella_dmic_hw_params,
	.trigger = ambarella_dmic_trigger,
	.set_fmt = ambarella_dmic_set_fmt,
	.set_sysclk = ambarella_dmic_set_sysclk,
};

static struct snd_soc_dai_driver ambarella_dmic_dai = {
	.name = "dmic-hifi",
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE),
	},
	.ops = &ambarella_dmic_dai_ops,
	.symmetric_rates = 1,
};

static struct snd_soc_codec_driver ambarella_dmic = {
	.dapm_widgets = dmic_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(dmic_dapm_widgets),
	.dapm_routes = intercon,
	.num_dapm_routes = ARRAY_SIZE(intercon),
};

static int ambarella_dmic_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct amb_dmic_priv *priv_data;
	struct resource *res;

	priv_data = devm_kzalloc(&pdev->dev, sizeof(*priv_data), GFP_KERNEL);
	if (priv_data == NULL)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No mem resource for DICI!\n");
		return -ENXIO;
	}

	priv_data->regbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!priv_data->regbase) {
		dev_err(&pdev->dev, "devm_ioremap() failed\n");
		return -ENOMEM;
	}

	priv_data->reg_scr = syscon_regmap_lookup_by_phandle(np, "amb,scr-regmap");
	if (IS_ERR(priv_data->reg_scr)) {
		dev_err(&pdev->dev, "no scr regmap!\n");
		return -ENXIO;
	}

	dev_set_drvdata(&pdev->dev, priv_data);

	snd_soc_register_codec(&pdev->dev,
			&ambarella_dmic, &ambarella_dmic_dai, 1);

	return 0;
}

static int ambarella_dmic_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static const struct of_device_id ambarella_dmic_dt_ids[] = {
	{ .compatible = "ambarella,dmic", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ambarella_dmic_dt_ids);

static struct platform_driver ambarella_dmic_driver = {
	.probe = ambarella_dmic_probe,
	.remove = ambarella_dmic_remove,

	.driver = {
		.name = "ambarella-dmic",
		.of_match_table = ambarella_dmic_dt_ids,
	},
};

module_platform_driver(ambarella_dmic_driver);

MODULE_AUTHOR("XianqingZheng <xqzheng@ambarella.com>");
MODULE_DESCRIPTION("Ambarella Soc DMIC Interface");

MODULE_LICENSE("GPL");

