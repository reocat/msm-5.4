/*
 * ambarella-fdma.c  --  Ambarella FIOS DMA channel driver
 *
 * History:
 *	2012/07/03 - Ken He <jianhe@ambarella.com> created file
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
#include <linux/irq.h>

#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/dma.h>
#include "ambarella-fdma.h"

/* Marco for DMA REG related */
#define DMA_CTRL_RESET_VALUE 	0x38000000

static ambarella_fdma_chan_t G_fdma;
static spinlock_t fdma_lock;

#if (DMA_SUPPORT_DMA_FIOS == 1)
static irqreturn_t ambarella_fdma_int_handler(int irq, void *dev_id)
{
	ambarella_fdma_chan_t	*dma;
	int					i;
	u32					ireg;

	dma = (ambarella_fdma_chan_t *)dev_id;

	ireg = amba_readl(DMA_FIOS_INT_REG);
	if (ireg & DMA_INT_CHAN0) {
		dma->status = amba_readl(DMA_FIOS_CHAN_STA_REG(FIO_DMA_CHAN));
		amba_writel(DMA_FIOS_CHAN_STA_REG(FIO_DMA_CHAN), 0);

		for (i = 0; i < dma->irq_count; i++) {
			if (dma->irq[i].enabled == 1) {
				dma->irq[i].handler(
					dma->irq[i].harg,
					dma->status);
			}
		}
	}

	return IRQ_HANDLED;
}
#endif

static irqreturn_t ambarella_fios_dma_int_handler(int irq, void *dev_id)
{
	ambarella_fdma_chan_t	*dma;
	int					i;
	u32					ireg;

	dma = (ambarella_fdma_chan_t *)dev_id;

	ireg = amba_readl(DMA_INT_REG);

	if (ireg & (1 << FIO_DMA_CHAN)) {
		dma->status = amba_readl(DMA_CHAN_STA_REG(FIO_DMA_CHAN));
		amba_writel(DMA_CHAN_STA_REG(FIO_DMA_CHAN), 0);

		for (i = 0; i < dma->irq_count; i++) {
			if (dma->irq[i].enabled == 1) {
				dma->irq[i].handler(
					dma->irq[i].harg,
					dma->status);
			}
		}
		return IRQ_HANDLED;
	} else
		return IRQ_NONE;
}

int ambarella_fdma_enable_irq(ambarella_fdma_handler handler)
{
	int					retval = 0;
	int					i;
	unsigned long				flags;

	if (unlikely(handler == NULL)) {
		pr_err("%s: handler is NULL!\n", __func__);
		retval = -EINVAL;
		goto ambarella_fdma_enable_irq_na;
	}

	spin_lock_irqsave(&fdma_lock, flags);

	for (i = 0; i < MAX_DMA_CHANNEL_IRQ_HANDLERS; i++) {
		if (G_fdma.irq[i].handler == NULL) {
			retval = -EINVAL;
			pr_err("%s: can't find 0x%x!\n", __func__, (u32)handler);
			break;
		}
		if (G_fdma.irq[i].handler == handler) {
			G_fdma.irq[i].enabled = 1;
			break;
		}
	}

	spin_unlock_irqrestore(&fdma_lock, flags);

ambarella_fdma_enable_irq_na:
	return retval;
}

int ambarella_fdma_disable_irq(ambarella_fdma_handler handler)
{
	int					retval = 0;
	int					i;
	unsigned long				flags;

	if (unlikely(handler == NULL)) {
		pr_err("%s: handler is NULL!\n", __func__);
		retval = -EINVAL;
		goto ambarella_fdma_disable_irq_na;
	}

	spin_lock_irqsave(&fdma_lock, flags);
	for (i = 0; i < MAX_DMA_CHANNEL_IRQ_HANDLERS; i++) {
		if (G_fdma.irq[i].handler == NULL) {
			retval = -EINVAL;
			pr_err("%s: can't find 0x%x!\n",
				__func__, (u32)handler);
			break;
		}

		if (G_fdma.irq[i].handler == handler) {
			G_fdma.irq[i].enabled = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&fdma_lock, flags);

ambarella_fdma_disable_irq_na:
	return retval;
}

/**
* ambarella_fdma_init
* @amb_dma_dev: ambarella dma engine
* @dma_chan: dma channel num
**/
int ambarella_fdma_init(void)
{
	int ret = 0;
	ambarella_fdma_chan_t	 *fdma = &G_fdma;
#if (DMA_SUPPORT_DMA_FIOS == 1)
	ret = request_irq(DMA_FIOS_IRQ, ambarella_fdma_int_handler,
										IRQ_TYPE_LEVEL_HIGH, "ambarella-fdma", fdma);
	if (ret) {
		pr_err("%s: request_irq %d fail %d!\n", __func__, DMA_FIOS_IRQ, ret);
		return ret;
	}
	amba_writel(DMA_FIOS_CHAN_STA_REG(FIO_DMA_CHAN), 0);
	amba_writel(DMA_FIOS_CHAN_CTR_REG(FIO_DMA_CHAN), DMA_CTRL_RESET_VALUE);
#else
	ret = request_irq(DMA_IRQ, ambarella_fios_dma_int_handler,
										IRQF_SHARED | IRQ_TYPE_LEVEL_HIGH, "ambarella-fdma", fdma);
	if (ret) {
		pr_err("%s: request_irq %d fail %d!\n", __func__, DMA_IRQ, ret);
		return ret;
	}
	amba_writel(DMA_CHAN_STA_REG(FIO_DMA_CHAN), 0);
	amba_writel(DMA_CHAN_CTR_REG(FIO_DMA_CHAN), DMA_CTRL_RESET_VALUE);
#endif

	return ret;
}

int ambarella_fdma_request_irq(ambarella_fdma_handler handler, void *harg)
{
	int ret = 0;
	int					i;
	unsigned long				flags;

	if (unlikely(handler == NULL)) {
		pr_err("%s: handler is NULL!\n", __func__);
		ret = -EINVAL;
		goto ambarella_fdma_request_irq_exit_na;
	}

	spin_lock_irqsave(&fdma_lock, flags);
	if (unlikely(G_fdma.irq_count > MAX_DMA_CHANNEL_IRQ_HANDLERS)) {
		pr_err("%s: fios irq_count[%d] > "
			"MAX_DMA_CHANNEL_IRQ_HANDLERS[%d]!\n",
			__func__, G_fdma.irq_count, MAX_DMA_CHANNEL_IRQ_HANDLERS);
		ret = -EINVAL;
		goto ambarella_fdma_request_irq_exit;
	}

	for (i = 0; i < MAX_DMA_CHANNEL_IRQ_HANDLERS; i++) {
		if (G_fdma.irq[i].handler == NULL) {
			G_fdma.irq[i].enabled = 0;
			G_fdma.irq[i].handler = handler;
			G_fdma.irq[i].harg = harg;
			G_fdma.irq_count++;
			break;
		}
	}

ambarella_fdma_request_irq_exit:
	spin_unlock_irqrestore(&fdma_lock, flags);

ambarella_fdma_request_irq_exit_na:
	return ret;
}

void ambarella_fdma_free_irq(ambarella_fdma_handler handler)
{
	int					i;
	unsigned long				flags;

	if (unlikely(handler == NULL)) {
		pr_err("%s: handler is NULL!\n", __func__);
		return;
	}

	spin_lock_irqsave(&fdma_lock, flags);
	if (unlikely(G_fdma.irq_count >
		MAX_DMA_CHANNEL_IRQ_HANDLERS)) {
		pr_err("%s: fdma irq_count[%d] > "
			"MAX_DMA_CHANNEL_IRQ_HANDLERS[%d]!\n",
			__func__, G_fdma.irq_count,
			MAX_DMA_CHANNEL_IRQ_HANDLERS);
		goto ambarella_fdma_free_irq_exit;
	}

	for (i = 0; i < MAX_DMA_CHANNEL_IRQ_HANDLERS; i++) {
		if (G_fdma.irq[i].handler == handler) {
			G_fdma.irq[i].enabled = 0;
			G_fdma.irq[i].handler = NULL;
			G_fdma.irq[i].harg = NULL;
			G_fdma.irq_count--;
			break;
		}
	}

	for (i = i + 1; i < MAX_DMA_CHANNEL_IRQ_HANDLERS; i++) {
		if (G_fdma.irq[i].handler != NULL) {
			G_fdma.irq[i - 1].enabled = G_fdma.irq[i].enabled;
			G_fdma.irq[i - 1].handler = G_fdma.irq[i].handler;
			G_fdma.irq[i - 1].harg = G_fdma.irq[i].harg;
			G_fdma.irq[i].handler = NULL;
			G_fdma.irq[i].harg = NULL;
			G_fdma.irq[i].enabled = 0;
		}
	}

ambarella_fdma_free_irq_exit:
	spin_unlock_irqrestore(&fdma_lock, flags);
}


#if (DMA_SUPPORT_DMA_FIOS == 1)
static inline int ambarella_req_fdma_fios(ambarella_fdma_desc_t *req, int chan)
{
	int					retval = 0;
	u32					ctr = 0;

	if (unlikely(req->xfr_count > 0x003fffff)) {
		pr_err("%s: xfr_count[0x%x] out of range!\n",
			__func__, req->xfr_count);
		retval = -EINVAL;
		goto amb_req_fdma_fios_exit;
	}

	if (unlikely((req->dst & 0x7)||(req->src & 0x7))) {
		pr_err("%s: src [0x%08x] or dst [0x%08x] is not 8 bytes aligned!\n",
					__func__, req->src, req->dst);
		retval = -EINVAL;
		goto amb_req_fdma_fios_exit;
	}

	amba_writel(DMA_FIOS_CHAN_STA_REG(chan), 0);
	if (req->next_desc == NULL) {
		amba_writel(DMA_FIOS_CHAN_SRC_REG(chan), req->src);
		amba_writel(DMA_FIOS_CHAN_DST_REG(chan), req->dst);

		ctr |= req->ctrl_info | req->xfr_count;
		ctr &= ~DMA_CHANX_CTR_D;
		ctr |= DMA_CHANX_CTR_EN;
	} else {/* Descriptor mode */
		amba_writel(DMA_FIOS_CHAN_DA_REG(chan), (u32)req);

		ctr |= DMA_CHANX_CTR_D;
		ctr |= DMA_CHANX_CTR_EN;
	}
	amba_writel(DMA_FIOS_CHAN_CTR_REG(chan), ctr);

amb_req_fdma_fios_exit:
	return retval;
}
#endif

static inline int ambarella_req_fdma(ambarella_fdma_desc_t * req, int chan)
{
	int					retval = 0;
	u32					ctr = 0;

	if (unlikely(req->xfr_count > 0x003fffff)) {
		pr_err("%s: xfr_count[0x%x] out of range!\n",
			__func__, req->xfr_count);
		retval = -EINVAL;
		goto amb_req_fdma_exit;
	}

	if (unlikely((req->dst & 0x7)||(req->src & 0x7))) {
		pr_err("%s: src [0x%08x] or dst [0x%08x] is not 8 bytes aligned!\n",
					__func__, req->src, req->dst);
		retval = -EINVAL;
		goto amb_req_fdma_exit;
	}

	amba_writel(DMA_CHAN_STA_REG(chan), 0);
	if (req->next_desc == NULL) {
		amba_writel(DMA_CHAN_SRC_REG(chan), req->src);
		amba_writel(DMA_CHAN_DST_REG(chan), req->dst);

		ctr |= (req->ctrl_info | req->xfr_count);
		ctr &= ~DMA_CHANX_CTR_D;
		ctr |= DMA_CHANX_CTR_EN;

	} else {/* Descriptor mode */
		amba_writel(DMA_CHAN_DA_REG(chan), (u32) req);

		ctr |= DMA_CHANX_CTR_D;
		ctr |= DMA_CHANX_CTR_EN;
	}
	amba_writel(DMA_CHAN_CTR_REG(chan), ctr);

amb_req_fdma_exit:
	return retval;
}

int ambarella_fdma_xfr(ambarella_fdma_desc_t *req, int chan)
{
	int					retval = 0;

	if (unlikely(chan < 0 || chan >= NUM_DMA_CHANNELS)) {
		pr_err("%s: chan[%d] < NUM_DMA_CHANNELS[%d]!\n",
			__func__, chan, NUM_DMA_CHANNELS);
		retval = -EINVAL;
		goto ambarella_fdma_xfr_exit;
	}

	if (unlikely(req == NULL)) {
		pr_err("%s: req is NULL!\n", __func__);
		retval = -EINVAL;
		goto ambarella_fdma_xfr_exit;
	}

#if (DMA_SUPPORT_DMA_FIOS == 1)
	retval = ambarella_req_fdma_fios(req, chan);
#else
	retval = ambarella_req_fdma(req, chan);
#endif

ambarella_fdma_xfr_exit:
	return retval;
}

int ambarella_fdma_desc_xfr(dma_addr_t desc_addr, int chan)
{
	if (unlikely(desc_addr == 0)) {
		pr_err("%s: desc_addr is NULL!\n", __func__);
		return -EINVAL;
	}

	if (unlikely((desc_addr & 0x7) != 0)) {
		pr_err("%s: desc_addr isn't aligned!\n", __func__);
		return -EINVAL;
	}

	if (unlikely(chan < 0 || chan >= NUM_DMA_CHANNELS)) {
		pr_err("%s: chan[%d] < NUM_DMA_CHANNELS[%d]!\n",
			__func__, chan, NUM_DMA_CHANNELS);
		return -EINVAL;
	}

	if (amba_readl(DMA_CHAN_CTR_REG(chan)) & DMA_CHANX_CTR_EN)
		pr_err("%s: dma (channel = %d) is still enabled!\n", __func__, chan);

	amba_writel(DMA_CHAN_DA_REG(chan), desc_addr);
	amba_writel(DMA_CHAN_CTR_REG(chan), DMA_CHANX_CTR_EN | DMA_CHANX_CTR_D);

	return 0;
}

MODULE_DESCRIPTION("Ambarella FDMA Driver");
MODULE_AUTHOR("Ken He <jianhe@ambarella.com>");
MODULE_LICENSE("GPL");