/*
 * drivers/dma/ambarella-dma.c  --  Ambarella iOne DMA engine driver
 *
 * History:
 *	2012/05/10 - Ken He <jianhe@ambarella.com> created file
 *
 * Coryright (c) 2008-2012, Ambarella, Inc.
 * http://www.ambarella.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/init.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>

#include <linux/delay.h>
#include <asm/irq.h>
#include <mach/hardware.h>
#include <mach/dma.h>

/* Marco for DMA REG related */
#define DMA_CTRL_RESET_VALUE 	0x38000000

#define DMA_CTRL_REG_OFFSET	0x300
#define DMA_SRC_REG_OFFSET 	0x304
#define DMA_DST_REG_OFFSET 	0x308
#define DMA_STATUS_REG_OFFSET 0x30C
#define DMA_DA_REG_OFFSET   	0x380

/**
* struct ambarella_dma_desc
* - ambarella dma engine descriptor fields -
* @src: Source starting address
* @dst: Destination starting address
* @next_desc: Next Descriptor address
* @rpt: Report address
* @xfr_count: Transfer count
* @ctrl_info: Control Information
**/
typedef struct ambarella_dma_desc {
	dma_addr_t src;
	dma_addr_t dst;
	struct ambarella_dma_desc *next_desc;
	u32 rpt;
	u32 xfr_count;
	u32 ctrl_info;
} ambarella_dma_desc_t;

/**
* struct ambarella_dma_chan
* - ambarella dma channel fields -
* @channel: current channel Number
* @chan: interface to dma_chan struct
* @desc: ambarella_dma_desc Member for each channel
* @amb_dma: ambarella_dma_engine struct
* @dma_status: for store the dma status
* @dma_desc: dma_async_tx_descriptor
* @descriptor_mode: The channel in the descriptor mode or not
* @num_desc: The number of descriptors
* @desc_phys: the ambarella dma desc pool desc phys address
* @dma_direction: DMA data direction 0:M<->M 1:M->D 2:D->M
* @completed_cookie: last completed cookie
* @status: status
**/
struct ambarella_dma_chan {
	unsigned int channel;
	struct dma_chan chan;
	struct ambarella_dma_desc *desc;
	struct ambarella_dma_engine *amb_dma;
	enum dma_status dma_status;
	struct dma_async_tx_descriptor dma_desc;
	int descriptor_mode;
	int num_desc;
	dma_addr_t desc_phys;
	enum dma_data_direction dma_direction;
	dma_cookie_t completed_cookie;
	u32 status;
};

/**
* struct ambarella_dma_engine
* - ambarella dma engine fields -
* @regbase: DMA engine device register start address
* @dma_common_device: Interface to dma_engine struct
* @lock: spinlock
* @tasklet: tasklet_struct for engine use
* @amb_dma_chan: The NUM_DMA_CHANNELS ambarella_dma_chan
**/
struct ambarella_dma_engine {
	u32 regbase;
	struct dma_device dma_common_device;
	spinlock_t lock;
	struct tasklet_struct tasklet;
	struct ambarella_dma_chan amb_dma_chan[NUM_DMA_CHANNELS];
};

static inline struct ambarella_dma_chan *to_ambarella_dma_chan(struct dma_chan *chan)
{
	return container_of(chan, struct ambarella_dma_chan, chan);
}

/**
* dummy descriptor for disable the dma engine usage
**/
static ambarella_dma_desc_t *dummy_descriptor = NULL;
static dma_addr_t dummy_descriptor_phys;
#define AMBA_CHAN_MAX_DESC_NUM 128

/**
* ambarella_dma_cookie_status - report cookie status
* @chan: dma channel
* @cookie: cookie we are interested in
* @state: dma_tx_state structure to return last/used cookies
*
* Report the status of the cookie, filling in the state structure if
* non-NULL.  No locking is required.
* Note: In the new linux-kernel maybe 3.5 this api would be a
* 			common function in the dmaengine.h
*       Use it just for portability further linux version
*       Then this function could be remove directly from this file
*       with replacing by the common function
**/
static inline enum dma_status ambarella_dma_cookie_status(struct dma_chan *chan,
	dma_cookie_t cookie, struct dma_tx_state *state)
{
	struct ambarella_dma_chan *amb_dma_chan = to_ambarella_dma_chan(chan);
	dma_cookie_t used, complete;

	used = chan->cookie;
	complete = amb_dma_chan->completed_cookie;
	barrier();
	if (state) {
		state->last = complete;
		state->used = used;
		state->residue = 0;
	}

	return dma_async_is_complete(cookie, complete, used);
}

/**
* ambarella_dma_init_channel
* @amb_dma_dev: ambarella dma engine
* @dma_chan: dma channel num
**/
static int ambarella_dma_init_channel(struct ambarella_dma_engine *amb_dma_dev, int dma_chan)
{
	struct ambarella_dma_chan *amb_dma_c = &amb_dma_dev->amb_dma_chan[dma_chan];

#if (DMA_SUPPORT_DMA_FIOS == 1)
	if (FIO_DMA_CHAN == dma_chan) {
		amba_writel(DMA_FIOS_CHAN_STA_REG(dma_chan), 0);
		amba_writel(DMA_FIOS_CHAN_CTR_REG(dma_chan), DMA_CTRL_RESET_VALUE);
	} else
#endif
	{
		amba_writel(DMA_CHAN_STA_REG(dma_chan), 0);
		amba_writel(DMA_CHAN_CTR_REG(dma_chan), DMA_CTRL_RESET_VALUE);
	}

	amb_dma_dev->dma_common_device.chancnt++;
	amb_dma_c->channel = dma_chan;
	amb_dma_c->amb_dma = amb_dma_dev;
	amb_dma_c->descriptor_mode = 0;
	amb_dma_c->dma_direction = DMA_NONE;

	/* init the each channel's dma_chan struct member */
	amb_dma_c->chan.chan_id = dma_chan;
	amb_dma_c->chan.device = &amb_dma_dev->dma_common_device;
	amb_dma_c->chan.cookie = amb_dma_c->completed_cookie = 1;

	/* Add the channel to the DMAC list */
	list_add_tail(&amb_dma_c->chan.device_node,
			&amb_dma_dev->dma_common_device.channels);

	return 0;
}

static void ambarella_dma_irq_handler_chan(struct ambarella_dma_chan *amb_dma_chan)
{

	if (amb_dma_chan->dma_desc.callback)
		amb_dma_chan->dma_desc.callback(amb_dma_chan->dma_desc.callback_param);

	amb_dma_chan->completed_cookie = amb_dma_chan->dma_desc.cookie;

}

static irqreturn_t ambarella_dma_irq_handler(int irq, void *dev_data)
{
	struct ambarella_dma_engine *amb_dma = dev_data;
	u32 ireg, i, dma_reg;
	u32 *dma_status;

	ireg = amba_readl(amb_dma->regbase + DMA_INT_OFFSET);

	for (i = 0; i < NUM_DMA_CHANNELS; i++) {
		if (ireg & (1 << i)) {
			dma_reg = amb_dma->regbase + DMA_STATUS_REG_OFFSET + (i << 4);
			amb_dma->amb_dma_chan[i].status = amba_readl(dma_reg);
			if(amb_dma->amb_dma_chan[i].chan.private) {
				dma_status = amb_dma->amb_dma_chan[i].chan.private;
				*dma_status = amb_dma->amb_dma_chan[i].status;
			}

			if(((amb_dma->amb_dma_chan[i].status) & 0xE7800000) != 0) {
				amb_dma->amb_dma_chan[i].dma_status = DMA_ERROR;
				pr_err("dma channel[%d] status is error0x[%08x]!\n",i,
					amb_dma->amb_dma_chan[i].status);
			}
			amba_writel(dma_reg, 0);
			ambarella_dma_irq_handler_chan(&amb_dma->amb_dma_chan[i]);
		}
	}

	return IRQ_HANDLED;
}

/**
* ambarella_dma_desc_set_configuration function
* set the configuration for the Descriptor mode
**/
static int ambarella_dma_desc_set_configuration(struct ambarella_dma_chan *amb_dma_chan)
{
	int retval = 0;
	u32 dma_reg, ctrl = 0;

	struct ambarella_dma_engine *amb_dma = amb_dma_chan->amb_dma;

	if (unlikely(amb_dma_chan->desc_phys == 0)) {
		pr_err("%s: amb_desc is NULL!\n",__func__);
		retval = -EINVAL;
		goto amb_dma_desc_exit;
	}

	if (unlikely((amb_dma_chan->desc_phys & 0x07) != 0)) {
		pr_err("%s: amb_desc is not 8 bytes aligned!\n",__func__);
		retval = -EINVAL;
		goto amb_dma_desc_exit;
	}

	dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	if(amba_readl(dma_reg) & DMA_CHANX_CTR_EN)
		pr_err("%s: dma (channel = %d) is still enabled!\n", __func__, amb_dma_chan->channel);

	dma_reg = amb_dma->regbase + DMA_STATUS_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, 0);
	dma_reg = amb_dma->regbase + DMA_DA_REG_OFFSET + ((amb_dma_chan->channel) << 2);
	amba_writel(dma_reg, (u32)(amb_dma_chan->desc_phys));

	ctrl |= DMA_CHANX_CTR_D;
//	ctrl |= DMA_CHANX_CTR_EN;

	dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, ctrl);

amb_dma_desc_exit:
	return retval;
}

/**
* ambarella_dma_set_configuration function
* set the configuration for the Non-Descriptor mode
**/
static int ambarella_dma_set_configuration(struct ambarella_dma_chan *amb_dma_chan)
{
	int retval = 0;
	u32 dma_reg, ctrl = 0;

	struct ambarella_dma_desc *amb_desc = amb_dma_chan->desc;
	struct ambarella_dma_engine *amb_dma = amb_dma_chan->amb_dma;

	if (unlikely(amb_desc->xfr_count > 0x003fffff)) {
		pr_err("%s: xfr_count[0x%x] out of range!\n",
			__func__, amb_desc->xfr_count);
		retval = -EINVAL;
		goto amb_dma_set_exit;
	}

	if (unlikely((amb_desc->src & 0x7)||(amb_desc->dst & 0x7))) {
		pr_err("%s: src [0x%08x] or dst [0x%08x] is not 8 bytes aligned!\n",
					__func__, amb_desc->src, amb_desc->dst);
		retval = -EINVAL;
		goto amb_dma_set_exit;
	}
	dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	if(amba_readl(dma_reg) & DMA_CHANX_CTR_EN)
		pr_err("%s: dma (channel = %d) is still enabled!\n", __func__, amb_dma_chan->channel);

	dma_reg = amb_dma->regbase + DMA_STATUS_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, 0);
	dma_reg = amb_dma->regbase + DMA_SRC_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, amb_desc->src);
	dma_reg = amb_dma->regbase + DMA_DST_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, amb_desc->dst);

	ctrl |= (amb_desc->ctrl_info | amb_desc->xfr_count);
	ctrl &= ~DMA_CHANX_CTR_D;
	//ctrl |= DMA_CHANX_CTR_EN;

	dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, ctrl);

amb_dma_set_exit:
	return retval;
}

static int ambarella_dma_terminate_all(struct ambarella_dma_chan *amb_dma_chan)
{
	u32 dma_reg;

	struct ambarella_dma_engine *amb_dma = amb_dma_chan->amb_dma;

	/* Disable DMA: following sequence is not mentioned at APM.*/
	dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	if (amba_readl(dma_reg) & DMA_CHANX_CTR_EN) {
		dma_reg = amb_dma->regbase + DMA_STATUS_REG_OFFSET + ((amb_dma_chan->channel) << 4);
		amba_writel(dma_reg, DMA_CHANX_STA_DD);
		dma_reg = amb_dma->regbase + DMA_DA_REG_OFFSET + ((amb_dma_chan->channel) << 2);
		amba_writel(dma_reg, dummy_descriptor_phys);
		dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
		amba_writel(dma_reg, 0x28000000);
		udelay(10);
	}

	return 0;
}

static dma_cookie_t ambarella_assign_cookie(struct ambarella_dma_chan *amb_dma_chan)
{
	dma_cookie_t cookie = amb_dma_chan->chan.cookie;

	if (++cookie < 0)
		cookie = 1;

	amb_dma_chan->chan.cookie = cookie;
	amb_dma_chan->dma_desc.cookie = cookie;

	return cookie;
}

static dma_cookie_t ambarella_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct dma_chan *chan = tx->chan;
	struct ambarella_dma_chan *amb_dma_chan = to_ambarella_dma_chan(chan);
	struct ambarella_dma_engine *amb_dma = amb_dma_chan->amb_dma;
	u32 dma_reg, ctrl = 0;
	dma_cookie_t cookie = 0;
	unsigned long flags;

	spin_lock_irqsave(&amb_dma->lock, flags);

	cookie = ambarella_assign_cookie(amb_dma_chan);
	dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	ctrl = amba_readl(dma_reg);
	ctrl |= DMA_CHANX_CTR_EN;
	amba_writel(dma_reg, ctrl);

	spin_unlock_irqrestore(&amb_dma->lock, flags);
	return cookie;
}

/* DMA Engine API begin */
static int ambarella_dma_alloc_chan_resources(struct dma_chan *chan)
{
	struct ambarella_dma_chan *amb_dma_chan = to_ambarella_dma_chan(chan);
	dma_addr_t amba_desc_phys;

	__memzero(&amb_dma_chan->dma_desc, sizeof(struct dma_async_tx_descriptor));
	dma_async_tx_descriptor_init(&amb_dma_chan->dma_desc, chan);
	amb_dma_chan->dma_desc.tx_submit = ambarella_tx_submit;

	/* dma_desc.flags will be overwritten in prep funcs */
	amb_dma_chan->dma_desc.flags = DMA_CTRL_ACK;
	//amb_dma_chan->dma_status = DMA_SUCCESS;

	/* Now allocate and setup the descriptor. */
	amb_dma_chan->desc = dma_alloc_coherent(NULL,
			sizeof(ambarella_dma_desc_t) * AMBA_CHAN_MAX_DESC_NUM,
			&amba_desc_phys, GFP_KERNEL);

	if (!amb_dma_chan->desc) {
		return -ENOMEM;
	}

	amb_dma_chan->desc_phys = amba_desc_phys;
	return 0;
}

static void ambarella_dma_free_chan_resources(struct dma_chan *chan)
{
	struct ambarella_dma_chan *amb_dma_chan = to_ambarella_dma_chan(chan);

	dma_free_coherent(NULL, sizeof(ambarella_dma_desc_t) * AMBA_CHAN_MAX_DESC_NUM,
				amb_dma_chan->desc, amb_dma_chan->desc_phys);
}

static enum dma_status ambarella_dma_tx_status(struct dma_chan *chan,
				dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	return ambarella_dma_cookie_status(chan, cookie, txstate);
}

static void ambarella_dma_issue_pending(struct dma_chan *chan)
{
	/*
	 * Nothing to do. We only have a single descriptor
	 * Maybe something to add
	 */
}

static int ambarella_dma_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
		unsigned long arg)
{
	struct ambarella_dma_chan *amb_dma_chan = to_ambarella_dma_chan(chan);
	struct dma_slave_config *dmaengine_cfg = (void *)arg;
	struct ambarella_dma_desc *amba_dma_desc = amb_dma_chan->desc;
	int i, ret = 0;
	enum dma_slave_buswidth		dev_width = 0;
	u32 burst_level = 0;

	switch(cmd) {
	case DMA_TERMINATE_ALL:
		ret = ambarella_dma_terminate_all(amb_dma_chan);
		break;
	case DMA_PAUSE:
		break;
	case DMA_RESUME:
		break;
	case DMA_SLAVE_CONFIG:
		if (amb_dma_chan->descriptor_mode == 1) {
			for( i = 0; i < amb_dma_chan->num_desc; i++) {
				amba_dma_desc->src = dmaengine_cfg->src_addr;
				amba_dma_desc->dst = dmaengine_cfg->dst_addr;

				/* Use the dma_slave_config to transfer param, maybe need optimize */
				if (dmaengine_cfg->direction == DMA_TO_DEVICE) {
					amba_dma_desc->ctrl_info = DMA_DESC_RM | DMA_DESC_NI | DMA_DESC_ID |
								   DMA_DESC_IE | DMA_DESC_ST;
					dev_width = dmaengine_cfg->dst_addr_width;
					burst_level = dmaengine_cfg->dst_maxburst;
				} else if (dmaengine_cfg->direction == DMA_FROM_DEVICE) {
					amba_dma_desc->ctrl_info = DMA_DESC_WM | DMA_DESC_NI | DMA_DESC_ID |
								   DMA_DESC_IE | DMA_DESC_ST;
					dev_width = dmaengine_cfg->src_addr_width;
					burst_level = dmaengine_cfg->src_maxburst;
				} else if (dmaengine_cfg->direction == DMA_BIDIRECTIONAL) {
					amba_dma_desc->ctrl_info = DMA_DESC_WM | DMA_DESC_RM | DMA_DESC_ID |
								   DMA_DESC_IE | DMA_DESC_ST;
					dev_width = dmaengine_cfg->dst_addr_width ?
								dmaengine_cfg->dst_addr_width : dmaengine_cfg->src_addr_width;
					burst_level = dmaengine_cfg->dst_maxburst ?
								dmaengine_cfg->dst_maxburst : dmaengine_cfg->src_maxburst;
					if (i == (amb_dma_chan->num_desc -1) )
						amba_dma_desc->ctrl_info |= DMA_DESC_EOC;
				} else {
					ret = -EPERM;
					break;
				}

				/* bus width for descriptor mode control_info [ts fileds] */
				switch (dev_width) {
					case DMA_SLAVE_BUSWIDTH_8_BYTES:
						amba_dma_desc->ctrl_info |= DMA_DESC_TS_8B;
						break;
					case DMA_SLAVE_BUSWIDTH_4_BYTES:
						amba_dma_desc->ctrl_info |= DMA_DESC_TS_4B;
						break;
					case DMA_SLAVE_BUSWIDTH_2_BYTES:
						amba_dma_desc->ctrl_info |= DMA_DESC_TS_2B;
						break;
					case DMA_SLAVE_BUSWIDTH_1_BYTE:
						amba_dma_desc->ctrl_info |= DMA_DESC_TS_1B;
						break;
					default:
						break;
				}

				/* burst for descriptor mode control_info [blk fileds] */
				switch (burst_level) {
				case 1024:
					amb_dma_chan->desc->ctrl_info |= DMA_DESC_BLK_1024B;
					break;
				case 512:
					amb_dma_chan->desc->ctrl_info |= DMA_DESC_BLK_512B;
					break;
				case 256:
					amb_dma_chan->desc->ctrl_info |= DMA_DESC_BLK_256B;
					break;
				case 128:
					amb_dma_chan->desc->ctrl_info |= DMA_DESC_BLK_128B;
					break;
				case 64:
					amb_dma_chan->desc->ctrl_info |= DMA_DESC_BLK_64B;
					break;
				case 32:
					amb_dma_chan->desc->ctrl_info |= DMA_DESC_BLK_32B;
					break;
				case 16:
					amb_dma_chan->desc->ctrl_info |= DMA_DESC_BLK_16B;
					break;
				case 8:
				default:
					break;
			}
				amba_dma_desc++;
				dmaengine_cfg++;
			}

			ret = ambarella_dma_desc_set_configuration(amb_dma_chan);
		} else {
			if(dmaengine_cfg->direction == DMA_FROM_DEVICE) {
				amba_dma_desc->ctrl_info = DMA_CHANX_CTR_WM | DMA_CHANX_CTR_NI;
				dev_width = dmaengine_cfg->src_addr_width;
				burst_level = dmaengine_cfg->src_maxburst;
			} else if (dmaengine_cfg->direction == DMA_TO_DEVICE) {
				amba_dma_desc->ctrl_info = DMA_CHANX_CTR_RM | DMA_CHANX_CTR_NI;
				dev_width = dmaengine_cfg->dst_addr_width;
				burst_level = dmaengine_cfg->dst_maxburst;
			} else if (dmaengine_cfg->direction == DMA_BIDIRECTIONAL) {
				amba_dma_desc->ctrl_info = DMA_CHANX_CTR_RM | DMA_CHANX_CTR_WM;
				dev_width = dmaengine_cfg->dst_addr_width ?
						dmaengine_cfg->dst_addr_width : dmaengine_cfg->src_addr_width;
				burst_level = dmaengine_cfg->dst_maxburst ?
						dmaengine_cfg->dst_maxburst : dmaengine_cfg->src_maxburst;
			} else {
				ret = -EPERM;
				break;
			}

			amba_dma_desc->src = dmaengine_cfg->src_addr;
			amba_dma_desc->dst = dmaengine_cfg->dst_addr;

			/* bus width for non-descriptor mode control_reg [ts fileds] */
			switch (dev_width) {
				case DMA_SLAVE_BUSWIDTH_8_BYTES:
					amba_dma_desc->ctrl_info |= DMA_CHANX_CTR_TS_8B;
					break;
				case DMA_SLAVE_BUSWIDTH_4_BYTES:
					amba_dma_desc->ctrl_info |= DMA_CHANX_CTR_TS_4B;
					break;
				case DMA_SLAVE_BUSWIDTH_2_BYTES:
					amba_dma_desc->ctrl_info |= DMA_CHANX_CTR_TS_2B;
					break;
				case DMA_SLAVE_BUSWIDTH_1_BYTE:
					amba_dma_desc->ctrl_info |= DMA_CHANX_CTR_TS_1B;
					break;
				default:
					break;
			}

			switch (burst_level) {
				case 1024:
					amb_dma_chan->desc->ctrl_info |= DMA_CHANX_CTR_BLK_1024B;
					break;
				case 512:
					amb_dma_chan->desc->ctrl_info |= DMA_CHANX_CTR_BLK_512B;
					break;
				case 256:
					amb_dma_chan->desc->ctrl_info |= DMA_CHANX_CTR_BLK_256B;
					break;
				case 128:
					amb_dma_chan->desc->ctrl_info |= DMA_CHANX_CTR_BLK_128B;
					break;
				case 64:
					amb_dma_chan->desc->ctrl_info |= DMA_CHANX_CTR_BLK_64B;
					break;
				case 32:
					amb_dma_chan->desc->ctrl_info |= DMA_CHANX_CTR_BLK_32B;
					break;
				case 16:
					amb_dma_chan->desc->ctrl_info |= DMA_CHANX_CTR_BLK_16B;
					break;
				case 8:
				default:
					break;
			}

			ret = ambarella_dma_set_configuration(amb_dma_chan);
		}
		break;
	default:
		ret = -EPERM;
	}

	return ret;
}

static struct dma_async_tx_descriptor *ambarella_prep_dma_memcpy(
	struct dma_chan *chan, dma_addr_t dest,
	dma_addr_t src, size_t len, unsigned long flags)
{
	struct ambarella_dma_chan *amb_dma_chan = to_ambarella_dma_chan(chan);
	struct ambarella_dma_engine *amb_dma = amb_dma_chan->amb_dma;
	u32 dma_reg;
	u32 ctrl = 0;

	if (unlikely(len > 0x003fffff)) {
		pr_err("%s: len[0x%x] out of range!\n",
			__func__, len);
		return NULL;
	}

	if (unlikely((dest & 0x7)||(src & 0x7))) {
		pr_err("%s: src [0x%08x] or dst [0x%08x] is not 8 bytes aligned!\n",
					__func__, src, dest);
		return NULL;
	}

	dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	if(amba_readl(dma_reg) & DMA_CHANX_CTR_EN)
		pr_err("%s: dma (channel = %d) is still enabled!\n", __func__, amb_dma_chan->channel);

	dma_reg = amb_dma->regbase + DMA_STATUS_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, 0);
	dma_reg = amb_dma->regbase + DMA_SRC_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, src);
	dma_reg = amb_dma->regbase + DMA_DST_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	amba_writel(dma_reg, dest);

	dma_reg = amb_dma->regbase + DMA_CTRL_REG_OFFSET + ((amb_dma_chan->channel) << 4);
	ctrl = ( DMA_CHANX_CTR_WM | DMA_CHANX_CTR_RM | DMA_CHANX_CTR_BLK_512B | DMA_CHANX_CTR_TS_4B |(u32)len );
	ctrl &= ~DMA_CHANX_CTR_D;
	amba_writel(dma_reg, ctrl);

	amb_dma_chan->dma_desc.callback = NULL;
	amb_dma_chan->dma_desc.callback_param = NULL;

	return &amb_dma_chan->dma_desc;
}

static struct dma_async_tx_descriptor *ambarella_prep_slave_sg(
	struct dma_chan *chan, struct scatterlist *sgl,
	unsigned int sg_len, enum dma_data_direction direction,
	unsigned long flags)
{
	struct ambarella_dma_chan *amb_dma_chan = to_ambarella_dma_chan(chan);
	struct ambarella_dma_desc *amba_dma_desc;
	struct scatterlist *sgent;
	dma_addr_t next_desc_phys;
	unsigned j = 0;

	amb_dma_chan->num_desc = sg_len;
	amba_dma_desc = amb_dma_chan->desc;
	next_desc_phys = amb_dma_chan->desc_phys;

	if ( sg_len > 1 ) {
		amb_dma_chan->descriptor_mode = 1;

		for_each_sg(sgl, sgent, sg_len, j) {
			next_desc_phys += sizeof(ambarella_dma_desc_t);
			amba_dma_desc->next_desc = (struct ambarella_dma_desc *) next_desc_phys;
			amba_dma_desc->xfr_count = sg_dma_len(sgent);		/* For xfr count length */
			amba_dma_desc->rpt = sg_dma_address(sgent);		/* For report address */
			amba_dma_desc++;
		}

		amba_dma_desc[-1].next_desc = (struct ambarella_dma_desc *) amb_dma_chan->desc_phys;
	} else {
		amba_dma_desc->xfr_count = sg_dma_len(sgl);		/* For xfr count length */
	}
	amb_dma_chan->dma_desc.callback = NULL;
	amb_dma_chan->dma_desc.callback_param = NULL;

	return &amb_dma_chan->dma_desc;
}

/* DMA Engine API end */
static void ambarella_dma_task(unsigned long data)
{
//	struct ambarella_dma_engine *amb_dma = (void*)data;
}

static int __devinit ambarella_dma_probe(struct platform_device *pdev)
{
	struct ambarella_dma_engine *amb_dma;
	struct resource *res;
	unsigned i;
	int irq, ret = 0;
	ambarella_dma_desc_t	*dma_desc_array;

	/* Get the device resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res)
		return -ENXIO;

	/* alloc the amba dma engine struct */
	amb_dma = kzalloc(sizeof(*amb_dma), GFP_KERNEL);
	if (!amb_dma)
		return -ENOMEM;

	/* Get the irq */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = -EINVAL;
		goto ambarella_dma_probe_exit;
	}

	/* Alloc the dummy_descriptor for terminate usage */
	dummy_descriptor = dma_alloc_coherent(NULL,
			sizeof(ambarella_dma_desc_t) * 4,
			&dummy_descriptor_phys, GFP_KERNEL);
	if (!dummy_descriptor) {
		ret = -ENOMEM;
		goto ambarella_dma_probe_exit;
	}

	dma_desc_array = (ambarella_dma_desc_t *)dummy_descriptor;
	dma_desc_array->ctrl_info = DMA_DESC_EOC | DMA_DESC_WM | DMA_DESC_NI |
				DMA_DESC_IE | DMA_DESC_ST | DMA_DESC_ID;
	dma_desc_array->src = 0;
	dma_desc_array->next_desc = (ambarella_dma_desc_t *)dummy_descriptor_phys;
	dma_desc_array->rpt = dummy_descriptor_phys + sizeof(ambarella_dma_desc_t);
	dma_desc_array->dst = dummy_descriptor_phys + sizeof(ambarella_dma_desc_t) * 2;
	dma_desc_array->xfr_count = 0;

	/* Init the amba dma engine struct */
	INIT_LIST_HEAD(&amb_dma->dma_common_device.channels);
	INIT_LIST_HEAD(&amb_dma->dma_common_device.global_node);

	dma_cap_set(DMA_SLAVE, amb_dma->dma_common_device.cap_mask);
	dma_cap_set(DMA_MEMCPY, amb_dma->dma_common_device.cap_mask);
	amb_dma->dma_common_device.device_alloc_chan_resources = ambarella_dma_alloc_chan_resources;
	amb_dma->dma_common_device.device_free_chan_resources = ambarella_dma_free_chan_resources;
	amb_dma->dma_common_device.device_tx_status = ambarella_dma_tx_status;
	amb_dma->dma_common_device.device_issue_pending = ambarella_dma_issue_pending;
	amb_dma->dma_common_device.device_control = ambarella_dma_control;
	amb_dma->dma_common_device.device_prep_dma_memcpy = ambarella_prep_dma_memcpy;
	amb_dma->dma_common_device.device_prep_slave_sg = ambarella_prep_slave_sg;
	amb_dma->dma_common_device.dev = &pdev->dev;

	amb_dma->dma_common_device.copy_align = 3; /* 2^3 = 8 bytes for copy alignment */

	amb_dma->regbase = (u32)res->start;
	spin_lock_init(&amb_dma->lock);
	tasklet_init(&amb_dma->tasklet, ambarella_dma_task, (unsigned long)amb_dma);

	ret = request_irq(irq, ambarella_dma_irq_handler, IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), amb_dma);
	if (ret)
		goto ambarella_dma_probe_exit2;

	if (pdev->id == 0) {
		for (i = 0; i < NUM_DMA_CHANNELS; i++) {
			ambarella_dma_init_channel(amb_dma, i);
		}
	}
#if (DMA_SUPPORT_DMA_FIOS == 1)
	else {
		ambarella_dma_init_channel(amb_dma, FIO_DMA_CHAN);
	}
#endif

	ret = dma_async_device_register(&amb_dma->dma_common_device);
	if (ret) {
		pr_warn("ambarella DMA: failed to register DMA engine device: %d\n", ret);
		goto ambarella_dma_probe_exit1;
	} else {
		platform_set_drvdata(pdev, amb_dma);
	}

	dev_info(&pdev->dev, "ambarella DMA engine \n");

	return 0;

ambarella_dma_probe_exit1:
	free_irq(irq, amb_dma);

ambarella_dma_probe_exit2:
	tasklet_kill(&amb_dma->tasklet);

ambarella_dma_probe_exit:
	kfree(amb_dma);
	return ret;
}

static int __devexit ambarella_dma_remove(struct platform_device *pdev)
{
	struct ambarella_dma_engine *amb_dma = platform_get_drvdata(pdev);

	dma_async_device_unregister(&amb_dma->dma_common_device);

	kfree(amb_dma);

	return 0;
}

#ifdef CONFIG_PM
static int ambarella_dma_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int	errorCode = 0;

	return errorCode;
}

static int ambarella_dma_resume(struct platform_device *pdev)
{
	int	errorCode = 0;

	return errorCode;
}
#endif

static struct platform_driver ambarella_dma_driver = {
	.probe	= ambarella_dma_probe,
	.remove	= __devexit_p(ambarella_dma_remove),

#ifdef CONFIG_PM
	.suspend	= ambarella_dma_suspend,
	.resume	= ambarella_dma_resume,
#endif

	.driver	= {
		.name	= "ambarella-dma",
		.owner	= THIS_MODULE,
	},
};

static int __init ambarella_dma_init(void)
{
	return platform_driver_register(&ambarella_dma_driver);
}

static void __exit ambarella_dma_exit(void)
{
	platform_driver_unregister(&ambarella_dma_driver);
}

subsys_initcall(ambarella_dma_init);
module_exit(ambarella_dma_exit);

MODULE_DESCRIPTION("Ambarella DMA Engine System Driver");
MODULE_AUTHOR("Jian He <jianhe@ambarella.com>");
MODULE_LICENSE("GPL");
