/*
 * sound/soc/ambarella_i2s.c
 *
 * History:
 *	2008/03/03 - [Eric Lee] created file
 *	2008/04/16 - [Eric Lee] Removed the compiling warning
 *	2009/01/22 - [Anthony Ginger] Port to 2.6.28
 *	2009/03/05 - [Cao Rongrong] Update from 2.6.22.10
 *	2009/06/10 - [Cao Rongrong] Port to 2.6.29
 *	2009/06/29 - [Cao Rongrong] Support more mclk and fs
 *	2010/10/25 - [Cao Rongrong] Port to 2.6.36+
 *	2011/03/20 - [Cao Rongrong] Port to 2.6.38
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
#include <plat/audio.h>
#include "ambarella_pcm.h"
#include "ambarella_i2s.h"

static unsigned int bclk_reverse = 0;
module_param(bclk_reverse, uint, 0644);
MODULE_PARM_DESC(bclk_reverse, "bclk_reverse.");

static DEFINE_MUTEX(clock_reg_mutex);
static int enable_ext_i2s = 1;

/* ==========================================================================*/

static struct srcu_notifier_head audio_notifier_list;
static struct ambarella_i2s_interface audio_i2s_intf;

struct ambarella_i2s_interface get_audio_i2s_interface(void)
{
	return audio_i2s_intf;
}
EXPORT_SYMBOL(get_audio_i2s_interface);

int ambarella_audio_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&audio_notifier_list, nb);
}
EXPORT_SYMBOL(ambarella_audio_register_notifier);

int ambarella_audio_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&audio_notifier_list, nb);
}
EXPORT_SYMBOL(ambarella_audio_unregister_notifier);

static void ambarella_audio_notify_transition(int state, void *data)
{
	memcpy(&audio_i2s_intf, data, sizeof(audio_i2s_intf));
	audio_i2s_intf.state = state;
	srcu_notifier_call_chain(&audio_notifier_list, state, &audio_i2s_intf);
}

/* ==========================================================================*/

static inline void dai_tx_enable(struct amb_i2s_priv *priv_data)
{
	u32 val;
	val = readl_relaxed(priv_data->regbase + I2S_INIT_OFFSET);
	val |= DAI_TX_EN;
	writel_relaxed(val, priv_data->regbase + I2S_INIT_OFFSET);
}

static inline void dai_rx_enable(struct amb_i2s_priv *priv_data)
{
	u32 val;
	val = readl_relaxed(priv_data->regbase + I2S_INIT_OFFSET);
	val |= DAI_RX_EN;
	writel_relaxed(val, priv_data->regbase + I2S_INIT_OFFSET);
}

static inline void dai_tx_disable(struct amb_i2s_priv *priv_data)
{
	u32 val;
	val = readl_relaxed(priv_data->regbase + I2S_INIT_OFFSET);
	val &= DAI_TX_EN;
	writel_relaxed(val, priv_data->regbase + I2S_INIT_OFFSET);
}

static inline void dai_rx_disable(struct amb_i2s_priv *priv_data)
{
	u32 val;
	val = readl_relaxed(priv_data->regbase + I2S_INIT_OFFSET);
	val &= DAI_RX_EN;
	writel_relaxed(val, priv_data->regbase + I2S_INIT_OFFSET);
}

static inline void dai_tx_fifo_rst(struct amb_i2s_priv *priv_data)
{
	u32 val;
	val = readl_relaxed(priv_data->regbase + I2S_INIT_OFFSET);
	val |= I2S_TX_FIFO_RESET_BIT;
	writel_relaxed(val, priv_data->regbase + I2S_INIT_OFFSET);
}

static inline void dai_rx_fifo_rst(struct amb_i2s_priv *priv_data)
{
	u32 val;
	val = readl_relaxed(priv_data->regbase + I2S_INIT_OFFSET);
	val |= I2S_RX_FIFO_RESET_BIT;
	writel_relaxed(val, priv_data->regbase + I2S_INIT_OFFSET);
}

static inline void dai_fifo_rst(struct amb_i2s_priv *priv_data)
{
	u32 val;
	val = readl_relaxed(priv_data->regbase + I2S_INIT_OFFSET);
	val |= DAI_FIFO_RST;
	writel_relaxed(val, priv_data->regbase + I2S_INIT_OFFSET);
}

static int ambarella_i2s_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(dai);

	if(substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dai_rx_disable(priv_data);
		dai_rx_fifo_rst(priv_data);
	} else {
		dai_tx_disable(priv_data);
		dai_tx_fifo_rst(priv_data);
	}
	return 0;
}

static int ambarella_i2s_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	/* Add a rule to enforce the DMA buffer align. */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (ret)
		goto ambarella_i2s_startup_exit;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (ret)
		goto ambarella_i2s_startup_exit;

	ret = snd_pcm_hw_constraint_integer(runtime,
		SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto ambarella_i2s_startup_exit;

	return 0;
ambarella_i2s_startup_exit:
	return ret;
}

static int ambarella_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(cpu_dai);
	u8 slots, word_len, word_pos;
	u32 clock_divider, clock_reg, channels, tx_ctrl = 0, multi24 = 0;

	/* Disable tx/rx before initializing */
	dai_tx_disable(priv_data);
	dai_rx_disable(priv_data);

	channels = params_channels(params);
	/* Set channels */
	switch (channels) {
	case 2:
		writel_relaxed(I2S_2CHANNELS_ENB,
				priv_data->regbase + I2S_CHANNEL_SELECT_OFFSET);
		break;
	case 4:
		writel_relaxed(I2S_4CHANNELS_ENB,
				priv_data->regbase + I2S_CHANNEL_SELECT_OFFSET);
		break;
	case 6:
		writel_relaxed(I2S_6CHANNELS_ENB,
				priv_data->regbase + I2S_CHANNEL_SELECT_OFFSET);
		break;
	}
	priv_data->i2s_intf.ch = channels;

	/* Set format */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		priv_data->i2s_intf.word_len = DAI_16bits;
		word_len = 0x0f;
		tx_ctrl |= 0x08; /* set unison bit (LR both in TX_LEFT_DATA) */
		if (priv_data->i2s_intf.mode == DAI_DSP_Mode) {
			slots = channels - 1;
			word_pos = 0x0f;
			priv_data->i2s_intf.slots = slots;
		} else {
			slots = 0;
			word_pos = 0;
			priv_data->i2s_intf.slots = DAI_32slots;
		}
		priv_data->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		priv_data->i2s_intf.word_len = DAI_24bits;
		word_len = 0x17;
		multi24 = 0x1; /* multi_24_en bit */
		if (priv_data->i2s_intf.mode == DAI_DSP_Mode) {
			slots = channels - 1;
			word_pos = 0x00; /* ignored, but set it to something */
			priv_data->i2s_intf.slots = slots;
		} else {
			slots = 0;
			word_pos = 0; /* ignored, but set it to something */
			priv_data->i2s_intf.slots = DAI_32slots;
		}
		priv_data->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		goto hw_params_exit;
	}

	priv_data->i2s_intf.word_pos = word_pos;
	priv_data->i2s_intf.word_order = DAI_MSB_FIRST;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (priv_data->i2s_intf.ms_mode != DAI_SLAVE)
			tx_ctrl |= 0x20;
		writel_relaxed(tx_ctrl, priv_data->regbase + I2S_TX_CTRL_OFFSET);
		writel_relaxed(0x10, priv_data->regbase + I2S_TX_FIFO_LTH_OFFSET);
	} else {
		if (priv_data->i2s_intf.ms_mode == DAI_SLAVE)
			writel_relaxed(0x00, priv_data->regbase + I2S_RX_CTRL_OFFSET);
		else
			writel_relaxed(0x02, priv_data->regbase + I2S_RX_CTRL_OFFSET);
		writel_relaxed(0x20, priv_data->regbase + I2S_RX_FIFO_GTH_OFFSET);
	}

	writel_relaxed(priv_data->i2s_intf.mode, priv_data->regbase + I2S_MODE_OFFSET);
	writel_relaxed(word_len, priv_data->regbase + I2S_WLEN_OFFSET);
	writel_relaxed(word_pos, priv_data->regbase + I2S_WPOS_OFFSET);
	writel_relaxed(slots, priv_data->regbase + I2S_SLOT_OFFSET);
	writel_relaxed(multi24, priv_data->regbase + I2S_24BITMUX_MODE_OFFSET);

	switch (params_rate(params)) {
	case 8000:
		priv_data->i2s_intf.sfreq = AUDIO_SF_8000;
		priv_data->i2s_intf.oversample = AudioCodec_1536xfs;
		break;
	case 11025:
		priv_data->i2s_intf.sfreq = AUDIO_SF_11025;
		priv_data->i2s_intf.oversample = AudioCodec_1152xfs;
		break;
	case 16000:
		priv_data->i2s_intf.sfreq = AUDIO_SF_16000;
		priv_data->i2s_intf.oversample = AudioCodec_768xfs;
		break;
	case 22050:
		priv_data->i2s_intf.sfreq = AUDIO_SF_22050;
		priv_data->i2s_intf.oversample = AudioCodec_512xfs;
		break;
	case 32000:
		priv_data->i2s_intf.sfreq = AUDIO_SF_32000;
		priv_data->i2s_intf.oversample = AudioCodec_384xfs;
		break;
	case 44100:
		priv_data->i2s_intf.sfreq = AUDIO_SF_44100;
		priv_data->i2s_intf.oversample = AudioCodec_256xfs;
		break;
	case 48000:
		priv_data->i2s_intf.sfreq = AUDIO_SF_48000;
		priv_data->i2s_intf.oversample = AudioCodec_256xfs;
		break;
	default:
		goto hw_params_exit;
	}

	/* Set clock */
	clk_set_rate(priv_data->mclk, priv_data->i2s_intf.mclk);

	mutex_lock(&clock_reg_mutex);
	clock_reg = readl_relaxed(priv_data->regbase + I2S_CLOCK_OFFSET);

	/* In 16 bit mode, the channel width is 16 bits, and in 24 bit
	 * mode then channel width is 32 bits (see Ambarella S2L Hardware
         * Programming Manual, Table 9-1); consequently we need to adjust
	 * the clock divider accordingly, by referencing 'double_rate' here.
	 */

	clock_divider = priv_data->i2s_intf.div;

	clock_reg &= ~DAI_CLOCK_MASK;
	clock_reg |= clock_divider;
	if (priv_data->i2s_intf.ms_mode == DAI_MASTER)
		clock_reg |= I2S_CLK_MASTER_MODE;
	else
		clock_reg &= (~I2S_CLK_MASTER_MODE);
	/* Disable output BCLK and LRCLK to disable external codec */
	if (enable_ext_i2s == 0)
		clock_reg &= ~(I2S_CLK_WS_OUT_EN | I2S_CLK_BCLK_OUT_EN);

	if (bclk_reverse)
		clock_reg &= ~(1<< 6);
	else
		clock_reg |= (1<< 6);

	writel_relaxed(clock_reg, priv_data->regbase + I2S_CLOCK_OFFSET);
	mutex_unlock(&clock_reg_mutex);

	dai_rx_enable(priv_data);
	dai_tx_enable(priv_data);

	msleep(1);

	/* Notify HDMI that the audio interface is changed */
	ambarella_audio_notify_transition(AUDIO_NOTIFY_SETHWPARAMS, &priv_data->i2s_intf);

	return 0;

hw_params_exit:
	dai_rx_enable(priv_data);
	dai_tx_enable(priv_data);
	return -EINVAL;
}

static int ambarella_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *cpu_dai)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			dai_tx_enable(priv_data);
		else
			dai_rx_enable(priv_data);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK){
			dai_tx_disable(priv_data);
			dai_tx_fifo_rst(priv_data);
		}else{
			dai_rx_disable(priv_data);
			dai_rx_fifo_rst(priv_data);
		}
		break;
	default:
		break;
	}

	return 0;
}

/*
 * Set Ambarella I2S DAI format
 */
static int ambarella_i2s_set_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(cpu_dai);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
		priv_data->i2s_intf.mode = DAI_leftJustified_Mode;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		priv_data->i2s_intf.mode = DAI_rightJustified_Mode;
		break;
	case SND_SOC_DAIFMT_I2S:
		priv_data->i2s_intf.mode = DAI_I2S_Mode;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		priv_data->i2s_intf.mode = DAI_DSP_Mode;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		priv_data->i2s_intf.ms_mode = DAI_MASTER;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		if (priv_data->i2s_intf.mode != DAI_I2S_Mode) {
			printk("DAI can't work in slave mode without standard I2S format!\n");
			return -EINVAL;
		}
		priv_data->i2s_intf.ms_mode = DAI_SLAVE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ambarella_i2s_set_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(cpu_dai);

	switch (clk_id) {
	case AMBARELLA_CLKSRC_ONCHIP:
		priv_data->i2s_intf.clksrc = clk_id;
		priv_data->i2s_intf.mclk = freq;
		break;

	case AMBARELLA_CLKSRC_EXTERNAL:
	default:
		printk("CLK SOURCE (%d) is not supported yet\n", clk_id);
		return -EINVAL;
	}

	return 0;
}

static int ambarella_i2s_set_clkdiv(struct snd_soc_dai *cpu_dai,
		int div_id, int div)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(cpu_dai);

	switch (div_id) {
	case AMBARELLA_CLKDIV_LRCLK:
		priv_data->i2s_intf.div = div;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* ==========================================================================*/


#ifdef CONFIG_PM
static int ambarella_i2s_dai_suspend(struct snd_soc_dai *dai)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(dai);

	priv_data->clock_reg = readl_relaxed(priv_data->regbase + I2S_CLOCK_OFFSET);

	return 0;
}

static int ambarella_i2s_dai_resume(struct snd_soc_dai *dai)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(dai);

	writel_relaxed(priv_data->clock_reg, priv_data->regbase + I2S_CLOCK_OFFSET);

	return 0;
}
#else /* CONFIG_PM */
#define ambarella_i2s_dai_suspend	NULL
#define ambarella_i2s_dai_resume	NULL
#endif /* CONFIG_PM */

static int ambarella_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(dai);
	u32 sfreq, clock_divider;

	dai->capture_dma_data = &priv_data->capture_dma_data;
	dai->playback_dma_data = &priv_data->playback_dma_data;

	if (priv_data->default_mclk == 12288000) {
		sfreq = AUDIO_SF_48000;
	} else if (priv_data->default_mclk == 11289600){
		sfreq = AUDIO_SF_44100;
	} else {
		dev_warn(dai->dev, "Please sepcify the default mclk\n");
		priv_data->default_mclk = 12288000;
		sfreq = AUDIO_SF_48000;
	}

	clk_set_rate(priv_data->mclk, priv_data->default_mclk);

	/* Dai default smapling rate, polarity configuration.
	 * Note: Just be configured, actually BCLK and LRCLK will not
	 * output to outside at this time. */
	clock_divider = 3;
	writel_relaxed(clock_divider | I2S_CLK_TX_PO_FALL,
			priv_data->regbase + I2S_CLOCK_OFFSET);

	priv_data->i2s_intf.mode = DAI_I2S_Mode;
	priv_data->i2s_intf.clksrc = AMBARELLA_CLKSRC_ONCHIP;
	priv_data->i2s_intf.ms_mode = DAI_MASTER;
	priv_data->i2s_intf.mclk = priv_data->default_mclk;
	priv_data->i2s_intf.oversample = AudioCodec_256xfs;
	priv_data->i2s_intf.div = clock_divider;
	priv_data->i2s_intf.word_order = DAI_MSB_FIRST;
	priv_data->i2s_intf.sfreq = sfreq;
	priv_data->i2s_intf.word_len = DAI_16bits;
	priv_data->i2s_intf.word_pos = 0;
	priv_data->i2s_intf.slots = DAI_32slots;
	priv_data->i2s_intf.ch = 2;

	/* reset fifo */
	dai_tx_enable(priv_data);
	dai_rx_enable(priv_data);
	dai_fifo_rst(priv_data);

	/* Notify HDMI that the audio interface is initialized */
	ambarella_audio_notify_transition(AUDIO_NOTIFY_INIT, &priv_data->i2s_intf);

	return 0;
}

static int ambarella_i2s_dai_remove(struct snd_soc_dai *dai)
{
	struct amb_i2s_priv *priv_data = snd_soc_dai_get_drvdata(dai);

	/* Disable I2S clock output */
	writel_relaxed(0x0, priv_data->regbase + I2S_CLOCK_OFFSET);

	/* Notify that the audio interface is removed */
	ambarella_audio_notify_transition(AUDIO_NOTIFY_REMOVE, &priv_data->i2s_intf);

	return 0;
}

static const struct snd_soc_dai_ops ambarella_i2s_dai_ops = {
	.prepare = ambarella_i2s_prepare,
	.startup = ambarella_i2s_startup,
	.hw_params = ambarella_i2s_hw_params,
	.trigger = ambarella_i2s_trigger,
	.set_fmt = ambarella_i2s_set_fmt,
	.set_sysclk = ambarella_i2s_set_sysclk,
	.set_clkdiv = ambarella_i2s_set_clkdiv,
};

static struct snd_soc_dai_driver ambarella_i2s_dai = {
	.probe = ambarella_i2s_dai_probe,
	.remove = ambarella_i2s_dai_remove,
	.suspend = ambarella_i2s_dai_suspend,
	.resume = ambarella_i2s_dai_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 0, // initialized in ambarella_i2s_probe function
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE),
	},
	.capture = {
		.channels_min = 2,
		.channels_max = 0, // initialized in ambarella_i2s_probe function
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE),
	},
	.ops = &ambarella_i2s_dai_ops,
	.symmetric_rates = 1,
};

static const struct snd_soc_component_driver ambarella_i2s_component = {
	.name		= "ambarella-i2s",
};

static int ambarella_i2s_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct amb_i2s_priv *priv_data;
	struct resource *res;
	int channels, rval;

	priv_data = devm_kzalloc(&pdev->dev, sizeof(*priv_data), GFP_KERNEL);
	if (priv_data == NULL)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No mem resource for i2s!\n");
		return -ENXIO;
	}

	priv_data->regbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!priv_data->regbase) {
		dev_err(&pdev->dev, "devm_ioremap() failed\n");
		return -ENOMEM;
	}

	priv_data->playback_dma_data.addr = I2S_TX_LEFT_DATA_DMA_REG;
	priv_data->playback_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	priv_data->playback_dma_data.maxburst = 32;

	priv_data->capture_dma_data.addr = I2S_RX_DATA_DMA_REG;
	priv_data->capture_dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	priv_data->capture_dma_data.maxburst = 32;
	priv_data->mclk = clk_get(&pdev->dev, "gclk_audio");
	if (IS_ERR(priv_data->mclk)) {
		dev_err(&pdev->dev, "Get audio clk failed!\n");
		return PTR_ERR(priv_data->mclk);
	}

	dev_set_drvdata(&pdev->dev, priv_data);

	rval = of_property_read_u32(np, "amb,i2s-channels", &channels);
	if (rval < 0) {
		dev_err(&pdev->dev, "Get channels failed! %d\n", rval);
		return -ENXIO;
	}

	of_property_read_u32(np, "amb,default-mclk", &priv_data->default_mclk);

	ambarella_i2s_dai.playback.channels_max = channels;
	ambarella_i2s_dai.capture.channels_max = channels;

	srcu_init_notifier_head(&audio_notifier_list);

	rval = snd_soc_register_component(&pdev->dev,
			&ambarella_i2s_component,  &ambarella_i2s_dai, 1);
	if (rval < 0){
		dev_err(&pdev->dev, "register DAI failed\n");
		return rval;
	}

	rval = ambarella_pcm_platform_register(&pdev->dev);
	if (rval) {
		dev_err(&pdev->dev, "register PCM failed: %d\n", rval);
		snd_soc_unregister_component(&pdev->dev);
		return rval;
	}

	return 0;
}

static int ambarella_i2s_remove(struct platform_device *pdev)
{
	ambarella_pcm_platform_unregister(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static const struct of_device_id ambarella_i2s_dt_ids[] = {
	{ .compatible = "ambarella,i2s", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ambarella_i2s_dt_ids);

static struct platform_driver ambarella_i2s_driver = {
	.probe = ambarella_i2s_probe,
	.remove = ambarella_i2s_remove,

	.driver = {
		.name = "ambarella-i2s",
		.of_match_table = ambarella_i2s_dt_ids,
	},
};

module_platform_driver(ambarella_i2s_driver);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella Soc I2S Interface");

MODULE_LICENSE("GPL");

