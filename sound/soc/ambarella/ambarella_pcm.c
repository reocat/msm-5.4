/*
 * sound/soc/ambarella_pcm.c
 *
 * History:
 *	2008/03/03 - [Eric Lee] created file
 *	2008/04/16 - [Eric Lee] Removed the compiling warning
 *	2008/08/07 - [Cao Rongrong] Fix the buffer bug,eg: size and allocation
 *	2008/11/14 - [Cao Rongrong] Support pause and resume
 *	2009/01/22 - [Anthony Ginger] Port to 2.6.28
 *	2009/03/05 - [Cao Rongrong] Update from 2.6.22.10
 *	2009/06/10 - [Cao Rongrong] Port to 2.6.29
 *	2009/06/30 - [Cao Rongrong] Fix last_desc bug
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
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/hardware.h>
#include <mach/dma.h>

#include "ambarella_pcm.h"

#define AMBA_MAX_DESC_NUM		128
#define AMBA_MIN_DESC_NUM		2
#define AMBA_PERIOD_BYTES_MAX		8192
#define AMBA_PERIOD_BYTES_MIN		32

struct scatterlist sg[AMBA_MAX_DESC_NUM];

struct ambarella_runtime_data {
	struct ambarella_pcm_dma_params *dma_data;

	struct dma_chan *dma_chan;
	struct dma_async_tx_descriptor *desc;
	struct dma_slave_config *slave_config;

	int channel;		/* Physical DMA channel */
	int ndescr;		/* Number of descriptors */
	int last_descr;		/* Record lastest DMA done descriptor number */

	u32 *dma_rpt_buf;
	dma_addr_t dma_rpt_phys;
	u32				dma_status;

	spinlock_t lock;
};

static const struct snd_pcm_hardware ambarella_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_PAUSE |
				SNDRV_PCM_INFO_RESUME |
				SNDRV_PCM_INFO_BATCH,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE,
	.rates			= SNDRV_PCM_RATE_8000_48000,
	.rate_min		= 8000,
	.rate_max		= 48000,
	.period_bytes_min	= AMBA_PERIOD_BYTES_MIN,
	.period_bytes_max	= AMBA_PERIOD_BYTES_MAX,
	.periods_min		= AMBA_MIN_DESC_NUM,
	.periods_max		= AMBA_MAX_DESC_NUM,
	.buffer_bytes_max	= 256 * 1024,
};

static bool ambpcm_play_dma_filter(struct dma_chan *chan, void *fparam)
{
	struct ambarella_runtime_data *prtd;

	prtd = (struct ambarella_runtime_data *)fparam;
	if ((chan->dev->dev_id == 0) && (chan->chan_id == I2S_TX_DMA_CHAN)) {
		chan->private = &prtd->dma_status;
		return true;
	}
	return false;
}

static bool ambpcm_record_dma_filter(struct dma_chan *chan, void *fparam)
{
	struct ambarella_runtime_data *prtd;

	prtd = (struct ambarella_runtime_data *)fparam;
	if ((chan->dev->dev_id == 0) && (chan->chan_id == I2S_RX_DMA_CHAN)) {
		chan->private = &prtd->dma_status;
		return true;
	}
	return false;
}

static void dai_dma_handler(void *dev_id)
{
	u32 *rpt;
	int cur_descr;
	struct snd_pcm_substream *substream = dev_id;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ambarella_runtime_data *prtd = runtime->private_data;

	cur_descr = prtd->last_descr;

	prtd->last_descr++;
	if (prtd->last_descr >= prtd->ndescr) {
		prtd->last_descr = 0;
	}
	snd_pcm_period_elapsed(substream);

	/* Check if stop dma chain */
	rpt = &prtd->dma_rpt_buf[cur_descr];
	if (*rpt & 0x10000000) { /* Descriptor chain done */
		*rpt &= (~0x10000000);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			/* dai_tx_disable() */
			amba_clrbitsl(I2S_INIT_REG, 0x4);
			/* dai_fifo_rst() */
			if(!amba_tstbitsl(I2S_INIT_REG, 0x6))
				amba_setbitsl(I2S_INIT_REG, 0x1);
		} else {
			/* dai_rx_disable() */
			amba_clrbitsl(I2S_INIT_REG, 0x2);
			/* dai_fifo_rst() */
			if(!amba_tstbitsl(I2S_INIT_REG, 0x6))
				amba_setbitsl(I2S_INIT_REG, 0x1);
		}
	}

	//if (*rpt & 0x08000000)  /* Descriptor DMA operation done */
	//	*rpt &= (~0x08000000);
}

static void dai_rx_dma_handler(void *dev_id)
{
	dai_dma_handler(dev_id);
}

static void dai_tx_dma_handler(void *dev_id)
{
	dai_dma_handler(dev_id);
}

/* this may get called several times by oss emulation */
static int ambarella_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ambarella_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct ambarella_pcm_dma_params *dma_data;
	size_t totsize = params_buffer_bytes(params);
	size_t period = params_period_bytes(params);

	dma_addr_t dma_buff_phys, next_rpt;
	unsigned long flags;
	int i;

 	dma_cap_mask_t mask;
	struct dma_slave_config *amb_slave_config;
	enum dma_data_direction direction;
	unsigned sg_len;

	dma_data = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	if (!dma_data)
		return -ENODEV;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totsize;

	if (prtd->dma_data)
		return 0;

	prtd->dma_data = dma_data;

	sg_len = 0;
	sg_init_table(&sg[0], AMBA_MAX_DESC_NUM);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd->channel = I2S_TX_DMA_CHAN;
		direction = DMA_TO_DEVICE;
		/* Try to grab a DMA channel */
		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		prtd->dma_chan = dma_request_channel(mask, ambpcm_play_dma_filter, prtd);
		if (!prtd->dma_chan) {
			return -EINVAL;
		}
	} else {
		prtd->channel = I2S_RX_DMA_CHAN;
		direction = DMA_FROM_DEVICE;
		/* Try to grab a DMA channel */
		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);
		prtd->dma_chan = dma_request_channel(mask, ambpcm_record_dma_filter, prtd);
		if (!prtd->dma_chan) {
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&prtd->lock, flags);

	amb_slave_config = prtd->slave_config;
	next_rpt = prtd->dma_rpt_phys;
	dma_buff_phys = runtime->dma_addr;

	prtd->ndescr = 0;
	prtd->last_descr = 0;
	do {
		if (period > totsize)
			period = totsize;

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			amb_slave_config->direction = DMA_TO_DEVICE;
			amb_slave_config->src_addr = dma_buff_phys;
			amb_slave_config->dst_addr = prtd->dma_data->dev_addr;
			amb_slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			amb_slave_config->dst_maxburst = 32;
		} else {
			amb_slave_config->direction = DMA_FROM_DEVICE;
			amb_slave_config->src_addr = prtd->dma_data->dev_addr;
			amb_slave_config->dst_addr = dma_buff_phys;
			amb_slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			amb_slave_config->src_maxburst = 32;
		}
		sg[sg_len].dma_address = next_rpt;
		sg_dma_len(&sg[sg_len]) = period;

		dma_buff_phys += period;
		next_rpt += sizeof(dma_addr_t);
		prtd->ndescr++;
		sg_len++;
		amb_slave_config++;
	} while (totsize -= period);

	prtd->desc = prtd->dma_chan->device->device_prep_slave_sg(prtd->dma_chan,
		 &sg[0], sg_len, direction, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		prtd->desc->callback = dai_tx_dma_handler;
	else
		prtd->desc->callback = dai_rx_dma_handler;

	prtd->desc->callback_param = substream;

	for (i = 0; i < prtd->ndescr; i++)
		prtd->dma_rpt_buf[i] = 0;

	spin_unlock_irqrestore(&prtd->lock, flags);

	return 0;
}

static int ambarella_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ambarella_runtime_data *prtd = runtime->private_data;

	if (prtd->dma_data) {
		if (prtd->dma_chan) {
			dma_release_channel(prtd->dma_chan);
			prtd->dma_chan = NULL;
		}
		prtd->dma_data = NULL;
		prtd->ndescr = 0;
		prtd->last_descr = 0;
	}

	/* TODO - do we need to ensure DMA flushed */
	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

static int ambarella_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct ambarella_runtime_data *prtd = substream->runtime->private_data;

	/* Ensure dma is stopped */
		dmaengine_terminate_all(prtd->dma_chan);

	return 0;
}

static int ambarella_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ambarella_runtime_data *prtd = runtime->private_data;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&prtd->lock, flags);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			dmaengine_slave_config(prtd->dma_chan, prtd->slave_config);
			dmaengine_submit(prtd->desc);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			dmaengine_terminate_all(prtd->dma_chan);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&prtd->lock, flags);

	return ret;
}

static snd_pcm_uframes_t ambarella_pcm_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ambarella_runtime_data *prtd = runtime->private_data;

	return prtd->last_descr * runtime->period_size;
}

static int ambarella_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ambarella_runtime_data *prtd;
	int ret = 0;

	snd_soc_set_runtime_hwparams(substream, &ambarella_pcm_hardware);

	/* Add a rule to enforce the DMA buffer align. */
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (ret)
		goto ambarella_pcm_open_exit;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (ret)
		goto ambarella_pcm_open_exit;

	ret = snd_pcm_hw_constraint_integer(runtime,
		SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		goto ambarella_pcm_open_exit;

	prtd = kzalloc(sizeof(struct ambarella_runtime_data), GFP_KERNEL);
	if (prtd == NULL) {
		ret = -ENOMEM;
		goto ambarella_pcm_open_exit;
	}

	prtd->slave_config = kzalloc(sizeof(struct dma_slave_config) * AMBA_MAX_DESC_NUM,
				 			GFP_KERNEL);
	if (prtd->slave_config == NULL) {
		ret = -ENOMEM;
		goto ambarella_pcm_open_free_prtd;
	}

	prtd->dma_rpt_buf = dma_alloc_coherent(substream->pcm->card->dev,
					AMBA_MAX_DESC_NUM * sizeof(prtd->dma_rpt_buf),
					&prtd->dma_rpt_phys, GFP_KERNEL);
	if(prtd->dma_rpt_buf == NULL){
		dev_err(substream->pcm->card->dev,
			"No memory for dma_rpt_buf\n");
		goto ambarella_pcm_open_free_prtd;
	}

	spin_lock_init(&prtd->lock);

	runtime->private_data = prtd;
	goto ambarella_pcm_open_exit;

ambarella_pcm_open_free_prtd:
	kfree(prtd);

ambarella_pcm_open_exit:
	return ret;
}

static int ambarella_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct ambarella_runtime_data *prtd = runtime->private_data;

	if(prtd){
		if (prtd->slave_config != NULL )
			kfree(prtd->slave_config);
		prtd->slave_config = NULL;

		if(prtd->dma_rpt_buf != NULL) {
			dma_free_coherent(substream->pcm->card->dev,
				AMBA_MAX_DESC_NUM * sizeof(prtd->dma_rpt_buf),
				prtd->dma_rpt_buf, prtd->dma_rpt_phys);
			prtd->dma_rpt_buf = NULL;
		}

		kfree(prtd);
		runtime->private_data = NULL;
	}

	return 0;
}

static int ambarella_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_coherent(substream->pcm->card->dev, vma,
		runtime->dma_area, runtime->dma_addr, runtime->dma_bytes);
}

static struct snd_pcm_ops ambarella_pcm_ops = {
	.open		= ambarella_pcm_open,
	.close		= ambarella_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= ambarella_pcm_hw_params,
	.hw_free	= ambarella_pcm_hw_free,
	.prepare	= ambarella_pcm_prepare,
	.trigger	= ambarella_pcm_trigger,
	.pointer	= ambarella_pcm_pointer,
	.mmap		= ambarella_pcm_mmap,
};

static int ambarella_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = ambarella_pcm_hardware.buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static void ambarella_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	for (stream = 0; stream <= SNDRV_PCM_STREAM_LAST; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
			buf->area, buf->addr);
		buf->area = NULL;
		buf->addr = (dma_addr_t)NULL;
	}
}

static int ambarella_pcm_new(struct snd_card *card,
	struct snd_soc_dai *dai, struct snd_pcm *pcm)
{
	int ret = 0;

	card->dev->dma_mask = &ambarella_dmamask;
	card->dev->coherent_dma_mask = ambarella_dmamask;

	if (dai->driver->playback.channels_min) {
		ret = ambarella_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (dai->driver->capture.channels_min) {
		ret = ambarella_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
out:
	return ret;
}

static struct snd_soc_platform_driver ambarella_soc_platform = {
	.pcm_new	= ambarella_pcm_new,
	.pcm_free	= ambarella_pcm_free_dma_buffers,
	.ops		= &ambarella_pcm_ops,
};

static int __devinit ambarella_soc_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &ambarella_soc_platform);
}

static int __devexit ambarella_soc_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver ambarella_pcm_driver = {
	.driver = {
			.name = "ambarella-pcm-audio",
			.owner = THIS_MODULE,
	},

	.probe = ambarella_soc_platform_probe,
	.remove = __devexit_p(ambarella_soc_platform_remove),
};

static int __init snd_ambarella_pcm_init(void)
{
	return platform_driver_register(&ambarella_pcm_driver);
}
module_init(snd_ambarella_pcm_init);

static void __exit snd_ambarella_pcm_exit(void)
{
	platform_driver_unregister(&ambarella_pcm_driver);
}
module_exit(snd_ambarella_pcm_exit);

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella Soc PCM DMA module");
MODULE_LICENSE("GPL");

