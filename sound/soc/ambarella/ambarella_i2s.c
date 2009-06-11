/*
 * sound/soc/ambarella_i2s.c
 *
 * History:
 *	2008/03/03 - [Eric Lee] created file
 *	2008/04/16 - [Eric Lee] Removed the compiling warning
 *	2009/01/22 - [Anthony Ginger] Port to 2.6.28
 *	2009/03/05 - [Cao Rongrong] Update from 2.6.22.10
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
#include <sound/soc.h>

#include <mach/hardware.h>

#include "ambarella_pcm.h"
#include "ambarella_i2s.h"

#define AMBARELLA_I2S_DEBUG 0
#if AMBARELLA_I2S_DEBUG
#define DBG(x...) printk(x) //printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

static u32 DAI_Clock_Divide_Table[MAX_OVERSAMPLE_IDX_NUM][2] = { 
	{ 1, 0 }, // 128xfs 
	{ 3, 1 }, // 256xfs
	{ 5, 2 }, // 384xfs
	{ 7, 3 }, // 512xfs
	{ 11, 5 }, // 768xfs
	{ 15, 7 }, // 1024xfs
	{ 17, 8 }, // 1152xfs
	{ 23, 11 }, // 1536xfs
	{ 35, 17 } // 2304xfs
};

struct amb_i2s_priv {
	u32 mode;
	u16 clock;
	u8 mclk;
};


/* FIXME HERE for PCM interface */
static struct ambarella_pcm_dma_params ambarella_i2s_pcm_stereo_out = {
	.name			= "I2S PCM Stereo out",
	.dev_addr		= io_v2p(I2S_TX_LEFT_DATA_DMA_REG),
};

static struct ambarella_pcm_dma_params ambarella_i2s_pcm_stereo_in = {
	.name			= "I2S PCM Stereo in",
	.dev_addr		= io_v2p(I2S_RX_DATA_DMA_REG),
};

void set_audio_pll(u8 mclk)
{
	rct_set_aud_ctrl2_reg();
	rct_set_pll_frac_mode();
	rct_set_audio_pll_fs(AUC_CLK_ONCHIP_PLL_27MHZ, mclk);
}

static inline void dai_tx_enable(void)
{
	amba_setbits(I2S_INIT_REG, DAI_TX_EN);
}

static inline void dai_rx_enable(void)
{
	amba_setbits(I2S_INIT_REG, DAI_RX_EN);
}

static inline void dai_tx_disable(void)
{
	amba_clrbits(I2S_INIT_REG, DAI_TX_EN);
}

static inline void dai_rx_disable(void)
{
	amba_clrbits(I2S_INIT_REG, DAI_RX_EN);
}

static inline void dai_fifo_rst(void)
{
	amba_setbits(I2S_INIT_REG, DAI_FIFO_RST);
	msleep(1);
	if (amba_tstbits(I2S_INIT_REG, DAI_FIFO_RST)) {
		printk("DAI_FIFO_RST fail!\n");
	}
}

static int ambarella_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct amb_i2s_priv *priv_data = cpu_dai->private_data;
	u8 oversample, slots;
	u32 clock_divider;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		cpu_dai->dma_data = &ambarella_i2s_pcm_stereo_out;
	else
		cpu_dai->dma_data = &ambarella_i2s_pcm_stereo_in;

	/* Set format */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		slots = DAI_64slots;	
		amba_writel(I2S_MODE_REG, priv_data->mode);
		amba_writel(I2S_WLEN_REG, 0x0F);
		amba_writel(I2S_WPOS_REG, 0xf);
		amba_writel(I2S_SLOT_REG, 0);
		amba_writel(I2S_24BITMUX_MODE_REG, 0);
		break;
	default:
		return -EINVAL;
	}
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		/* Disable tx/rx before initializing */
		dai_tx_disable();
		amba_writel(I2S_TX_CTRL_REG, 0x28);
		amba_writel(I2S_TX_FIFO_LTH_REG, 0x10);
	} else {
		/* Disable tx/rx before initializing */
		dai_rx_disable();
		amba_writel(I2S_RX_CTRL_REG, 0x02);
		amba_writel(I2S_RX_FIFO_GTH_REG, 0x40);
	}

	set_audio_sfreq(params_rate(params));
	/* Set clock */
	switch (params_rate(params)) {
	case 8000:
		priv_data->mclk = AudioCodec_4_096M;
		oversample = AudioCodec_512xfs;
		/* FIXME, A2AUC only, will be replced by other way */
		break;
	case 11025:
		priv_data->mclk = AudioCodec_5_6448M;
		oversample = AudioCodec_512xfs;
		/* FIXME, A2AUC only, will be replced by other way */
		break;
	case 16000:
		priv_data->mclk = AudioCodec_4_096M;
		oversample = AudioCodec_256xfs;
		break;
	case 22050:
		priv_data->mclk = AudioCodec_5_6448M;
		oversample = AudioCodec_256xfs;
		break;
	case 32000:
		priv_data->mclk = AudioCodec_8_192M;
		oversample = AudioCodec_256xfs;
		break;
	case 44100:
		priv_data->mclk = AudioCodec_11_2896M;
		oversample = AudioCodec_256xfs;
		break;
	case 48000:
		priv_data->mclk = AudioCodec_12_288M;
		oversample = AudioCodec_256xfs;
		break;
	default:
		return -EINVAL;
	}

	set_audio_pll(priv_data->mclk);
	/* Dai smapling rate, polarity configuration*/
	clock_divider = DAI_Clock_Divide_Table[oversample][slots >> 6];
	clock_divider |= 0x3C0 ;
	priv_data->clock &= (u16)DAI_CLOCK_MASK;
	priv_data->clock |= clock_divider;
	amba_writel(I2S_CLOCK_REG, priv_data->clock);

	if(!amba_tstbits(I2S_INIT_REG, 0x6))
		dai_fifo_rst();

	msleep(1);

	return 0;
}

static int ambarella_i2s_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	if(substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		dai_fifo_rst();
	return 0;
}

static int ambarella_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			struct snd_soc_dai *dai)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			dai_rx_enable();
		} else {
			if(alsa_tx_enable_flag == 0)
				dai_tx_enable();
		}	
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			//dai_rx_disable();
			//Stop by DMA EOC
		} else {
			//dai_tx_disable();	
			//Stop by DMA EOC
		}	
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * Set Ambarella I2S DAI format
 */
static int ambarella_i2s_set_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	struct amb_i2s_priv *priv_data = cpu_dai->private_data;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {	
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
		priv_data->mode = DAI_leftJustified_Mode;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		priv_data->mode = DAI_rightJustified_Mode;
		break;	
	case SND_SOC_DAIFMT_I2S:
		priv_data->mode = DAI_I2S_Mode;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ambarella_i2s_probe(struct platform_device *pdev,
	struct snd_soc_dai *dai)
{
	struct amb_i2s_priv *priv_data;
	u32 clock_divider;

	printk("%s 1\n", __func__);

	DBG("DBG: I2S probe, set PLL, DAI init here ! \n");

#if (CHIP_REV == A2S) || (CHIP_REV == A2M)
	/* aucodec_digitalio_on */
	amba_setbits(GPIO2_AFSEL_REG, (0xf << 18) | (0xf << 13));
#endif

#if (CHIP_REV == A2)
	/* aucodec_digitalio_on */
	amba_setbits(GPIO2_AFSEL_REG, (0x3 << 15) | (0x3 << 20));
#endif

	priv_data = kzalloc(sizeof(struct amb_i2s_priv), GFP_KERNEL);
	if (priv_data == NULL)
		return -ENOMEM;

	dai->private_data = priv_data;
	
#if !(defined(CONFIG_SND_SOC_AMBARELLA_A2AUC) || defined(CONFIG_SND_SOC_AMBARELLA_A2AUC_MODULE))
	/*Switch to external I2S Input */
	priv_data->clock = 0x1<<10;
#endif
	set_audio_pll(AudioCodec_12_288M);

	/* Dai default smapling rate, polarity configuration*/
	clock_divider = DAI_Clock_Divide_Table[AudioCodec_256xfs][DAI_64slots >> 6];
	clock_divider |= 0x3C0 ;
	priv_data->clock &= (u16)DAI_CLOCK_MASK;
	priv_data->clock |= clock_divider;
	amba_writel(I2S_CLOCK_REG, priv_data->clock);

	set_audio_sfreq(48000);

	printk("%s 2\n", __func__);

	return 0;
}

struct snd_soc_dai ambarella_i2s_dai = {
	.name = "ambarella-i2s",
	.id = 0,
	.probe = ambarella_i2s_probe,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = {
		.prepare = ambarella_i2s_prepare,
		.trigger = ambarella_i2s_trigger,
		.hw_params = ambarella_i2s_hw_params,
		.set_fmt = ambarella_i2s_set_fmt,
	},
};
EXPORT_SYMBOL(ambarella_i2s_dai);

static int __init ambarella_i2s_init(void)
{
	printk("%s\n", __func__);
	return snd_soc_register_dai(&ambarella_i2s_dai);
}
module_init(ambarella_i2s_init);

static void __exit ambarella_i2s_exit(void)
{
	snd_soc_unregister_dai(&ambarella_i2s_dai);
}
module_exit(ambarella_i2s_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella Soc I2S Interface");

MODULE_LICENSE("GPL");

