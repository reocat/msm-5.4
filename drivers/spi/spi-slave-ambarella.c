/*
 * linux/drivers/spi/spi-slave-ambarella.c
 *	!!!!!!!! THIS SPISLAVE CAN only works in SPI_MODE_3 !!!!!!!
 * History:
 *	2012/02/21 - [Zhenwu Xue]  created file
 *	2019/10/25 - [xuliang Zhang] modify and fixup bugs [dma tx/rx and pio tx/rx are all ok]
 * Copyright (C) 2004-2012, Ambarella, Inc.
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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/poll.h>
#include <linux/pinctrl/consumer.h>

#include <linux/kfifo.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/cdev.h>
#include <linux/atomic.h>
#include <plat/spi.h>
#include <plat/dma.h>

#define SPI_RXFIS_MASK 		0x00000010
#define SPI_RXOIS_MASK 		0x00000008
#define SPI_RXUIS_MASK 		0x00000004
#define SPI_TXOIS_MASK 		0x00000002
#define SPI_TXEIS_MASK 		0x00000001

#define SPI_SR_BUSY_MSK (0x1)
#define SPI_SR_TXNF_MSK (0x2)
#define SPI_SR_TXEPT_MSK (0x4)
#define SPI_SR_TXE_MSK	(0x20)

#define SPI_FIFO_DEPTH		(SPI_DATA_FIFO_SIZE_32)
#define SPI_FIFO_DEPTH_MSK	(SPI_FIFO_DEPTH - 1)
#define SLAVE_SPI_BUF_SIZE	PAGE_SIZE

#define SPI_ALIGN_2_MASK	(0xFFFFFFFE)
#define FIFO_DEPTH_MULTI	(4)

#define SPI_DMA_TX	(0)
#define SPI_DMA_RX	(1)

#define SPI_TMOD_TXONLY		(1 << 8)
#define SPI_TMOD_RXONLY		(2 << 8)
#define SPI_TMODE_TXRX		(0 << 8)
#define SPI_TMODE_MSK		(3 << 8)

#define SPI_DMA_RX_EN		(1)
#define SPI_DMA_TX_EN		(2)
#define SPI_DMA_EN_NONE		(0)

#define CAN_WAKEUP_BLOCK_R (0x1)
#define CAN_WAKEUP_BLOCK_W (0x2)
#define CAN_WAKEUP_POLL (0x4)

/* export to user-space used by ioctl, not support now */
#define SPI_SLAVE_MAGIC   (250)
struct slavespi_ioctl_args {
};

/* slavespi controller */
struct ambarella_slavespi {

	int irq;
	void __iomem	*regbase;

	dev_t 	devno;
	struct cdev	cdevx;
	struct device	*device;
	const char *dname;
	struct class *dclass;

	u32	mode;
	u32	bpw;
	u32	byte_wise;
	u32	fifo_depth;

	u32	buf_size;
	void	*w_buf;
	void	*r_buf;

	u32	dma_used;
	u32	phys;
	struct	device *pltdev;
	struct dma_chan	*tx_dma_chan;
	struct dma_chan	*rx_dma_chan;
	u8	*tx_dma_buf;
	u8	*rx_dma_buf;
	dma_addr_t	tx_dma_phys;
	dma_addr_t	rx_dma_phys;

	dma_cookie_t	tx_cookie;
	dma_cookie_t	rx_cookie;

	struct dma_async_tx_descriptor	*tx_dma_desc;
	struct dma_async_tx_descriptor	*rx_dma_desc;
	u32 tx_dma_inprocess;
	u32 rx_dma_inprocess;
	u32 rx_dma_size;
	u32 tx_dma_size;

	struct kfifo	w_fifo;
	struct kfifo	r_fifo;

	u32 want_cnt;
	spinlock_t w_buf_lock;
	spinlock_t r_buf_lock;
	spinlock_t wakeup_lock;

	atomic_t open_once;
	struct mutex um_mtx;

	int timeout;
	u32 wakeup_mask;	/* bit0: read, bit1: write , bit2 poll */
	wait_queue_head_t wq_rhead;
	wait_queue_head_t wq_whead;
	wait_queue_head_t wq_poll;
	struct tasklet_struct wakeup_helper;
};

void ambarella_spislave_reset(struct ambarella_slavespi *priv)
{
	writel_relaxed(0, priv->regbase + SPI_SSIENR_OFFSET);
	udelay(1);
	writel_relaxed(1, priv->regbase + SPI_SSIENR_OFFSET);
}

static int
ambarella_spislave_dma_allocate(struct ambarella_slavespi *priv,
	bool dma_to_memory)
{
	int ret = 0;
	u8 *dma_buf;
	dma_addr_t dma_phys;
	struct dma_chan *dma_chan;

	dma_chan = dma_request_slave_channel(priv->pltdev,
					dma_to_memory ? "rx" : "tx");
	if (IS_ERR(dma_chan)) {
		ret = PTR_ERR(dma_chan);
		if (ret != -EPROBE_DEFER)
			dev_err(priv->pltdev,
				"Dma channel is not available: %d\n", ret);
		return ret;
	}

	dma_buf = dma_alloc_coherent(priv->pltdev, priv->buf_size,
				&dma_phys, GFP_KERNEL);
	if (!dma_buf) {
		dev_err(priv->pltdev, " Not able to allocate the dma buffer\n");
		dma_release_channel(dma_chan);
		return -ENOMEM;
	}

	if (dma_to_memory) {
		priv->rx_dma_chan = dma_chan;
		priv->rx_dma_buf = dma_buf;
		priv->rx_dma_phys = dma_phys;
	} else {
		priv->tx_dma_chan = dma_chan;
		priv->tx_dma_buf = dma_buf;
		priv->tx_dma_phys = dma_phys;
	}

	return ret;
}

static void
ambarella_spi_slave_dma_free(struct ambarella_slavespi *priv,
	bool dma_to_memory)
{
	u8 *dma_buf;
	dma_addr_t dma_phys;
	struct dma_chan *dma_chan;

	if (dma_to_memory) {
		dma_buf = priv->rx_dma_buf;
		dma_chan = priv->rx_dma_chan;
		dma_phys = priv->rx_dma_phys;
		priv->rx_dma_chan = NULL;
		priv->rx_dma_buf = NULL;
	} else {
		dma_buf = priv->tx_dma_buf;
		dma_chan = priv->tx_dma_chan;
		dma_phys = priv->tx_dma_phys;
		priv->tx_dma_buf = NULL;
		priv->tx_dma_chan = NULL;
	}

	if (!dma_chan) {
		return;
	}

	dma_free_coherent(priv->pltdev, priv->buf_size, dma_buf, dma_phys);
	dma_release_channel(dma_chan);
}

static void
ambarella_slavespi_start_tx_dma(struct ambarella_slavespi *priv);

static int
ambarella_slavespi_start_rx_dma(struct ambarella_slavespi *priv);

static void slavespi_drain_rxfifo(struct ambarella_slavespi *priv);


static void ambarella_rx_dma_callback(void *dma_param)
{
	u32 copied = 0;
	u32 avail_size = 0;
	unsigned long flags;

	struct dma_tx_state state;
	enum dma_status status;
	struct ambarella_slavespi *priv = (struct ambarella_slavespi *)dma_param;

	spin_lock_irqsave(&priv->r_buf_lock, flags);

	status = dmaengine_tx_status(priv->rx_dma_chan, priv->rx_cookie, &state);
	if (status == DMA_IN_PROGRESS) {
		pr_info("slavespi: RX DMA is in progress\n");
		spin_unlock_irqrestore(&priv->r_buf_lock, flags);
		return ;
	}

	if (status == DMA_COMPLETE) {
		/* dma is complete , get data into r_fifo */
		avail_size = priv->rx_dma_size - state.residue;
		/* let cpu own this buf to read data */
		dma_sync_single_for_cpu(priv->pltdev, priv->rx_dma_phys, priv->rx_dma_size, DMA_FROM_DEVICE);
		copied = kfifo_in(&priv->r_fifo, priv->rx_dma_buf, avail_size);
		ambarella_slavespi_start_rx_dma(priv);		/* re-start rx-dma again */
	}
	spin_unlock_irqrestore(&priv->r_buf_lock, flags);

	if (avail_size > copied) {
		pr_err("slavespi: rx dma drop %u bytes \n", avail_size - copied);
	}

	wake_up_interruptible(&priv->wq_poll);
	wake_up_interruptible(&priv->wq_rhead);
}

static int ambarella_slavespi_start_rx_dma(struct ambarella_slavespi *priv)
{
	struct dma_slave_config rx_cfg;
	memset(&rx_cfg, 0, sizeof(rx_cfg));

	priv->rx_dma_size = priv->fifo_depth * FIFO_DEPTH_MULTI;

	rx_cfg.src_addr	 = priv->phys + SPI_DR_OFFSET;

	/* Attention: src_maxburst should not big
	 * or get duplicated data for the last valid byte;
	 * ref data-sheet for more info
	*/
	if (priv->bpw <= 8) {
		rx_cfg.src_addr_width	= DMA_SLAVE_BUSWIDTH_1_BYTE;
		rx_cfg.src_maxburst	= 8;
	} else {
		rx_cfg.src_addr_width	= DMA_SLAVE_BUSWIDTH_2_BYTES;
		rx_cfg.src_maxburst	= 4;
	}
	rx_cfg.direction = DMA_DEV_TO_MEM;
	BUG_ON(dmaengine_slave_config(priv->rx_dma_chan, &rx_cfg) < 0);

	priv->rx_dma_desc = dmaengine_prep_slave_single(priv->rx_dma_chan,
		priv->rx_dma_phys, priv->rx_dma_size, DMA_DEV_TO_MEM,  DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
	if (!priv->rx_dma_desc) {
		dev_err(priv->pltdev, "slavespi: Not able to get desc for Rx dma \n");
		return -EIO;
	}

	priv->rx_dma_desc->callback = ambarella_rx_dma_callback;
	priv->rx_dma_desc->callback_param = priv;

	/* let device own this buf to write received data from outside */
	dma_sync_single_for_device(priv->pltdev, priv->rx_dma_phys, priv->rx_dma_size, DMA_FROM_DEVICE);
	priv->rx_cookie = dmaengine_submit(priv->rx_dma_desc);
	dma_async_issue_pending(priv->rx_dma_chan);

	return 0;
}

static void ambarella_tx_dma_callback(void *dma_param)
{
	u32 sr = 0;
	u32 copied = 0;
	unsigned long flags;
	u32 w_fifo_size = 0;
	u32 w_fifo_len = 0;

	struct dma_tx_state state;
	enum dma_status status;
	struct ambarella_slavespi *priv = NULL;

	priv = (struct ambarella_slavespi *)dma_param;

	sr = readl_relaxed(priv->regbase + SPI_SR_OFFSET);
	if (sr & SPI_SR_TXE_MSK) {
		//pr_err("slavespi: txe \n");
	}

	spin_lock_irqsave(&priv->w_buf_lock, flags);

	status = dmaengine_tx_status(priv->tx_dma_chan, priv->tx_cookie, &state);
	if (status == DMA_IN_PROGRESS) {
		pr_info("slavespi: TX DMA is in progress\n");
		spin_unlock_irqrestore(&priv->w_buf_lock, flags);
		return ;
	}
	if (status == DMA_COMPLETE) {
		w_fifo_len = kfifo_len(&priv->w_fifo);
		if (priv->bpw > 8) {
			w_fifo_len &= SPI_ALIGN_2_MASK;
		}
		if (w_fifo_len > 0) {
			w_fifo_size = min_t(u32, w_fifo_len, priv->buf_size);
			copied = kfifo_out(&priv->w_fifo, priv->tx_dma_buf, w_fifo_size);
			if (copied != w_fifo_size) {
				pr_err("spislave: wfifo output bytes mismatch \n");
			}
			priv->tx_dma_size = copied;
		} else { /* tranfer dummy byte */
			memset(priv->tx_dma_buf, 0xFF, 64);
			priv->tx_dma_size = 64;
			priv->tx_dma_inprocess = 0;	/* you can over write txdma buf */
		}
		spin_unlock_irqrestore(&priv->w_buf_lock, flags);
		ambarella_slavespi_start_tx_dma(priv);
	}

	wake_up_interruptible(&priv->wq_poll);
	wake_up_interruptible(&priv->wq_whead);
}

void ambarella_slavespi_start_tx_dma(struct ambarella_slavespi *priv)
{
	struct dma_slave_config tx_cfg;

	/* TX DMA : mem->dev.txfifo */
	tx_cfg.dst_addr	 = priv->phys + SPI_DR_OFFSET;
	if (priv->bpw <= 8) {
		tx_cfg.dst_addr_width	= DMA_SLAVE_BUSWIDTH_1_BYTE;
		tx_cfg.dst_maxburst	= 8;
	} else {
		tx_cfg.dst_addr_width	= DMA_SLAVE_BUSWIDTH_2_BYTES;
		tx_cfg.dst_maxburst	= 4;
	}

	tx_cfg.direction = DMA_MEM_TO_DEV;

	BUG_ON(dmaengine_slave_config(priv->tx_dma_chan, &tx_cfg) < 0);

	/* now let device own this buf to tranfer data to outside */
	dma_sync_single_for_device(priv->pltdev, priv->tx_dma_phys, priv->tx_dma_size, DMA_TO_DEVICE);

	priv->tx_dma_desc = dmaengine_prep_slave_single(priv->tx_dma_chan,
		priv->tx_dma_phys, priv->tx_dma_size, DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);

	BUG_ON (!priv->tx_dma_desc);

	priv->tx_dma_desc->callback	= ambarella_tx_dma_callback;
	priv->tx_dma_desc->callback_param = priv;
	priv->tx_cookie = dmaengine_submit(priv->tx_dma_desc);
	dma_async_issue_pending(priv->tx_dma_chan);
}

static void wakeup_tasklet(unsigned long data)
{
	u32 mask = 0;
	unsigned long flags;
	struct ambarella_slavespi *priv	= (struct ambarella_slavespi *)data;

	spin_lock_irqsave(&priv->wakeup_lock, flags);
	mask = priv->wakeup_mask;
	priv->wakeup_mask = 0;
	spin_unlock_irqrestore(&priv->wakeup_lock, flags);

	if (mask & CAN_WAKEUP_BLOCK_W) {
		wake_up_interruptible(&priv->wq_whead);
	}

	if (mask & CAN_WAKEUP_BLOCK_R) {
		wake_up_interruptible(&priv->wq_rhead);
	}

	if (mask & CAN_WAKEUP_POLL) {
		wake_up_interruptible(&priv->wq_poll);
	}
}

static int ambarella_slavespi_tx_dummy(struct ambarella_slavespi *priv)
{
	u32 i = 0;

	/* fill hardware-txfifo with dummy data */
	for (i = 0; i < priv->fifo_depth; i++) {
		writel_relaxed(0xFF, priv->regbase + SPI_DR_OFFSET);
	}

	/* fill tx-dma buffer with dummy data */
	memset(priv->tx_dma_buf, 0xFF, priv->fifo_depth);
	priv->tx_dma_size = priv->fifo_depth;

	return 0;
}

static int ambarella_slavespi_setup(struct ambarella_slavespi *priv)
{
	int err = 0;
	struct clk *clk;
	spi_ctrl0_reg_t ctrl0_reg;

	writel_relaxed(0, priv->regbase + SPI_SSIENR_OFFSET);

	clk = clk_get(NULL, "gclk_ssi2");
	if (!IS_ERR_OR_NULL(clk)) {
		clk_set_rate(clk, 192000000);
	} else {
		dev_err(priv->device, "slavespi_bus: get clock failed \n");
		return -EINVAL;
	}

	ctrl0_reg.w = 0;
	ctrl0_reg.s.dfs = (priv->bpw - 1);
	ctrl0_reg.s.frf = 0;

	if (priv->mode & SPI_CPHA) {
		ctrl0_reg.s.scph = 1;
	} else {
		ctrl0_reg.s.scph = 0;
	}
	if (priv->mode & SPI_CPOL) {
		ctrl0_reg.s.scpol = 1;
	} else {
		ctrl0_reg.s.scpol = 0;
	}

	ctrl0_reg.s.tmod = SPI_WRITE_READ;
	if (priv->mode & SPI_LSB_FIRST) {
		ctrl0_reg.s.tx_lsb = 1;
		ctrl0_reg.s.rx_lsb = 1;
	} else {
		ctrl0_reg.s.tx_lsb = 0;
		ctrl0_reg.s.rx_lsb = 0;
	}

	if (priv->bpw > 8) {
		ctrl0_reg.s.byte_ws = 0; /* bit-per-word >8 bits, no byte_wise */
	}

	ctrl0_reg.s.hold = 0;
	ctrl0_reg.s.fc_en = 0;
	ctrl0_reg.s.residue = 1;
	writel_relaxed(ctrl0_reg.w, priv->regbase + SPI_CTRLR0_OFFSET);

	/* reset read/write fifo */
	kfifo_reset(&priv->w_fifo);
	kfifo_reset(&priv->r_fifo);

	/* clear any error */
	readl_relaxed(priv->regbase + SPI_TXOICR_OFFSET);
	readl_relaxed(priv->regbase + SPI_RXOICR_OFFSET);
	readl_relaxed(priv->regbase + SPI_RXUICR_OFFSET);
	readl_relaxed(priv->regbase + SPI_ICR_OFFSET);

	/* disable dma & interrupt */
	writel_relaxed(SPI_DMA_EN_NONE, priv->regbase + SPI_DMAC_OFFSET);
	writel_relaxed(0, priv->regbase + SPI_IMR_OFFSET);
	priv->tx_dma_inprocess = 0;
	priv->rx_dma_inprocess = 0;

	/* TX_Threshold <=16bytes, >=RX_Threadhold 16bytes */
	writel_relaxed(15, priv->regbase + SPI_TXFTLR_OFFSET);
	writel_relaxed(15, priv->regbase + SPI_RXFTLR_OFFSET);

	/* recvfifo interrupt */
	if (priv->dma_used) {
		dmaengine_terminate_sync(priv->tx_dma_chan);
		dmaengine_terminate_sync(priv->rx_dma_chan);
	} else {
		writel_relaxed(0x0, priv->regbase + SPI_DMAC_OFFSET);
		writel_relaxed(SPI_RXFIS_MASK, priv->regbase + SPI_IMR_OFFSET);
	}
	smp_wmb();
	writel_relaxed(1, priv->regbase + SPI_SSIENR_OFFSET);

	if (priv->dma_used) {
		priv->rx_dma_inprocess = 1;
		err = ambarella_slavespi_start_rx_dma(priv);

		ambarella_slavespi_tx_dummy(priv);
		priv->tx_dma_inprocess = 0;	/* can over write this dma buf */
		ambarella_slavespi_start_tx_dma(priv);

		/* finally start hardware */
		writel_relaxed(SPI_DMA_TX_EN|SPI_DMA_RX_EN, priv->regbase + SPI_DMAC_OFFSET);
	}

	/* pinctrl */

	return err;
}

/* drain any data in hardware rxfifo , used with spinlock */
void slavespi_drain_rxfifo(struct ambarella_slavespi *priv)
{
	u32 i = 0;
	u32 len = 0;
	u32 rxflr = 0;

	rxflr = readl_relaxed(priv->regbase + SPI_RXFLR_OFFSET);
	if (0 == rxflr) {	/* data-fifo-entry = 0, no data avail */
		return ;
	}

	for (i = 0; i < rxflr; i++) {
		if (priv->bpw > 8) {
			((u16 *)priv->r_buf)[i] = readl_relaxed(priv->regbase + SPI_DR_OFFSET);
		} else {
			((u8 *)priv->r_buf)[i] = readl_relaxed(priv->regbase + SPI_DR_OFFSET);
		}
	}
	if (priv->bpw > 8) {
		rxflr <<= 1;
	}

	len = kfifo_in(&priv->r_fifo, priv->r_buf, rxflr);
	if (rxflr > len) {
		pr_warn("spislave: recv drop %u bytes \n", (rxflr - len));
	}

	return ;
}

static irqreturn_t ambarella_slavespi_isr(int irq, void *dev_data)
{
	int i = 0;
	u32 isr = 0;
	u32 imr = 0;
	u32 risr = 0;
	u32 data_len = 0;
	u32 hw_free_len = 0;

	struct ambarella_slavespi *priv	= dev_data;

	isr = readl_relaxed(priv->regbase + SPI_ISR_OFFSET);
	risr = readl_relaxed(priv->regbase + SPI_RISR_OFFSET);

	if ((risr & SPI_RXOIS_MASK) || (risr & SPI_TXOIS_MASK)) {
		pr_err("slavespi: in isr speed too fast \n");
		/* reset fifo to clear error */
		writel_relaxed(0, priv->regbase + SPI_SSIENR_OFFSET);
		udelay(1);
		writel_relaxed(1, priv->regbase + SPI_SSIENR_OFFSET);
	}

	if (isr & SPI_TXEIS_MASK) {
		spin_lock(&priv->w_buf_lock);
		if (kfifo_is_empty(&priv->w_fifo)) {
			/* w_fifo is empty then disable txfifo empty interrupt */
			imr = readl_relaxed(priv->regbase + SPI_IMR_OFFSET);
			imr &= (~SPI_TXEIS_MASK);
			writel_relaxed(imr, priv->regbase + SPI_IMR_OFFSET);
			priv->wakeup_mask |= CAN_WAKEUP_BLOCK_W | CAN_WAKEUP_POLL;
		} else {
			hw_free_len = priv->fifo_depth - readl_relaxed(priv->regbase + SPI_TXFLR_OFFSET);
			if (priv->bpw > 8) {
				hw_free_len *= 2; /* must fill 2*n bytes */
			}
			data_len = kfifo_out(&priv->w_fifo, priv->w_buf, hw_free_len);
			if (priv->bpw > 8) {
				data_len = data_len >> 1;
			}
			for (i = 0; i < data_len; i++) {
				if (priv->bpw > 8) {
					writel_relaxed(((u16 *)priv->w_buf)[i], \
						priv->regbase + SPI_DR_OFFSET);
				} else {
					writel_relaxed(((u8 *)priv->w_buf)[i], \
						priv->regbase + SPI_DR_OFFSET);
				}
			}
			priv->wakeup_mask |= CAN_WAKEUP_POLL;
			if (kfifo_len(&priv->w_fifo) < (FIFO_DEPTH_MULTI * priv->fifo_depth)) {
				priv->wakeup_mask |= CAN_WAKEUP_BLOCK_W;
			}

			imr = readl_relaxed(priv->regbase + SPI_IMR_OFFSET);
			writel_relaxed(imr | SPI_TXEIS_MASK, priv->regbase + SPI_IMR_OFFSET);
		}
		spin_unlock(&priv->w_buf_lock);
	}

	if ((!priv->dma_used) && (isr & SPI_RXFIS_MASK)) {
		spin_lock(&priv->r_buf_lock);
		slavespi_drain_rxfifo(priv);
		if (!kfifo_is_empty(&priv->r_fifo)) {
			priv->wakeup_mask |= CAN_WAKEUP_POLL;		/* wakeup poll for read */
		}
		if (kfifo_len(&priv->r_fifo) >= priv->want_cnt) {
			priv->wakeup_mask |= CAN_WAKEUP_BLOCK_R;	/* wakeup timeout block read */
		}
		spin_unlock(&priv->r_buf_lock);
	}

	writel_relaxed(0, priv->regbase + SPI_ISR_OFFSET);
	writel_relaxed(0, priv->regbase + SPI_RISR_OFFSET);

	tasklet_schedule(&priv->wakeup_helper);
	return IRQ_HANDLED;
}

static int slavespi_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct ambarella_slavespi *priv = NULL;

	priv = container_of(inode->i_cdev, struct ambarella_slavespi, cdevx);
	if (!priv) {
		dev_info(priv->device, "slavespi: no such device \n");
		return -EINVAL;
	}
	filp->private_data = priv;

	if (!atomic_dec_and_test(&priv->open_once)) {
		atomic_inc(&priv->open_once);
		dev_info(priv->device, "slavespi: device is busy \n");
		return -EBUSY;
	}

	err = ambarella_slavespi_setup(priv);
	return err;
}

static ssize_t
slavespi_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int i = 0;
	int err = 0;
	u32 imr = 0;

	u32 copied = 0;
	u32 dma_copied = 0;

	u32 txfifo_free = 0;
	u32 txfifo_len = 0;

	u32 w_fifo_size = 0;
	u32 w_fifo_len = 0;

	unsigned long flags;
	struct ambarella_slavespi *priv = NULL;

	priv = (struct ambarella_slavespi *)filp->private_data;
	if (!buf || 0 == count) {
		return 0;
	}

	spin_lock_irqsave(&priv->w_buf_lock, flags);
	if (kfifo_is_full(&priv->w_fifo)) {
		spin_unlock_irqrestore(&priv->w_buf_lock, flags);
		copied = 0;
		return copied;
	} else {
		err = kfifo_from_user(&priv->w_fifo, buf, count, &copied);
		spin_unlock_irqrestore(&priv->w_buf_lock, flags);
		if (err) {
			pr_err("slavespi: get data from user failed, err = %d \n", err);
			return err;
		}
	}

	if (priv->dma_used) {
		spin_lock_irqsave(&priv->w_buf_lock, flags);
		if (priv->tx_dma_inprocess) {
			spin_unlock_irqrestore(&priv->w_buf_lock, flags);
			return copied;
		}
		priv->tx_dma_inprocess = 1;

		w_fifo_len = kfifo_len(&priv->w_fifo);
		if (priv->bpw > 8) {
			w_fifo_len &= SPI_ALIGN_2_MASK;
		}
		w_fifo_size = min_t(u32, w_fifo_len, priv->buf_size);
		dma_copied = kfifo_out(&priv->w_fifo, priv->tx_dma_buf, w_fifo_size);
		if (dma_copied != w_fifo_size) {
			pr_err("spislave: wfifo output bytes mismatch \n");
		}
		spin_unlock_irqrestore(&priv->w_buf_lock, flags);

		if (dma_copied > 0) {
			priv->tx_dma_size = dma_copied;
			ambarella_slavespi_start_tx_dma(priv);
		} else {
			priv->tx_dma_inprocess = 0;	/* can over write dma buf */
		}

		return copied;
	}

	/* no dma avail, using interrupt pio to transfer */
	spin_lock_irqsave(&priv->w_buf_lock, flags);
	txfifo_free = priv->fifo_depth - readl_relaxed(priv->regbase + SPI_TXFLR_OFFSET);
	if (priv->bpw > 8) {
		txfifo_free *= 2;
	}
	txfifo_len = kfifo_out(&priv->w_fifo, priv->w_buf, txfifo_free);
	if (priv->bpw > 8) {
		txfifo_len >>= 1;
	}
	for (i = 0; i < txfifo_len; i++) {
		if (priv->bpw > 8) {
			writel_relaxed(((u16 *)priv->w_buf)[i], \
				priv->regbase + SPI_DR_OFFSET);
		} else {
			writel_relaxed(((u8 *)priv->w_buf)[i], \
				priv->regbase + SPI_DR_OFFSET);
		}
	}

	imr = readl_relaxed(priv->regbase + SPI_IMR_OFFSET);
	writel_relaxed((imr | SPI_TXEIS_MASK), priv->regbase + SPI_IMR_OFFSET);
	spin_unlock_irqrestore(&priv->w_buf_lock, flags);

	wait_event_interruptible(priv->wq_whead, !kfifo_is_full(&priv->w_fifo));

	return copied;
}

static ssize_t
slavespi_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int err = 0;
	int copied = 0;
	unsigned long flags;
	struct ambarella_slavespi *priv = NULL;

	if (!buf || 0 == count) {
		return 0;
	}

	priv = (struct ambarella_slavespi *)filp->private_data;
	if (priv->dma_used) {
		if (priv->rx_dma_inprocess) {
			spin_lock_irqsave(&priv->r_buf_lock, flags);
			err = kfifo_to_user(&priv->r_fifo, buf, count, &copied);
			spin_unlock_irqrestore(&priv->r_buf_lock, flags);
			if (err) {
				pr_err("spislave: copy to user failed \n");
				return err;
			} else {
				return copied;
			}
		} else {
			priv->rx_dma_inprocess = 1;
			err = ambarella_slavespi_start_rx_dma(priv);
			if (err) {
				pr_err("spislave: start rx dma failed \n");
				priv->rx_dma_inprocess = 0;
			}
			return err;
		}
	}

	if (filp->f_flags & O_NONBLOCK) {
		spin_lock_irqsave(&priv->r_buf_lock, flags);
		slavespi_drain_rxfifo(priv);
		err = kfifo_to_user(&priv->r_fifo, buf, count, &copied);
		spin_unlock_irqrestore(&priv->r_buf_lock, flags);
		if (err) {
			return err;
		} else {
			return copied;
		}
	}

	/* following is block read */
	priv->want_cnt = count;
	spin_lock_irqsave(&priv->r_buf_lock, flags);
	slavespi_drain_rxfifo(priv);
	if (kfifo_len(&priv->r_fifo) >= priv->want_cnt) {
		err = kfifo_to_user(&priv->r_fifo, buf, priv->want_cnt, &copied);
		spin_unlock_irqrestore(&priv->r_buf_lock, flags);
		if (err) {
			return err;
		} else {
			return copied;
		}
	}
	spin_unlock_irqrestore(&priv->r_buf_lock, flags);

	wait_event_interruptible(priv->wq_rhead, \
			(kfifo_len(&priv->r_fifo) >= priv->want_cnt));

	spin_lock_irqsave(&priv->r_buf_lock, flags);
	slavespi_drain_rxfifo(priv);
	err = kfifo_to_user(&priv->r_fifo, buf, count, &copied);
	spin_unlock_irqrestore(&priv->r_buf_lock, flags);

	if (err) {
		return err;
	} else {
		return copied;
	}
}

static long
slavespi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static int slavespi_release(struct inode *inode, struct file *filp)
{
	struct ambarella_slavespi *priv = NULL;
	priv = (struct ambarella_slavespi *)filp->private_data;

	writel_relaxed(0, priv->regbase + SPI_SSIENR_OFFSET);
	writel_relaxed(0, priv->regbase + SPI_ISR_OFFSET);
	writel_relaxed(0, priv->regbase + SPI_RISR_OFFSET);

	readl_relaxed(priv->regbase + SPI_TXOICR_OFFSET);
	readl_relaxed(priv->regbase + SPI_RXOICR_OFFSET);
	readl_relaxed(priv->regbase + SPI_RXUICR_OFFSET);
	readl_relaxed(priv->regbase + SPI_ICR_OFFSET);

	if (priv->dma_used) {
		writel_relaxed(0, priv->regbase + SPI_DMAC_OFFSET);
		writel_relaxed(0, priv->regbase + SPI_IMR_OFFSET);

		dmaengine_terminate_sync(priv->tx_dma_chan);
		dmaengine_terminate_sync(priv->rx_dma_chan);

		priv->tx_dma_inprocess = 0;
		priv->rx_dma_inprocess = 0;
	}

	kfifo_reset(&priv->w_fifo);
	kfifo_reset(&priv->r_fifo);

	atomic_inc(&priv->open_once);

	pr_info("close slavespi \n");

	return 0;
}

static unsigned int
slavespi_poll(struct file *filp, struct poll_table_struct *wait)
{
	u32 mask = 0;
	struct ambarella_slavespi *priv = NULL;

	priv = (struct ambarella_slavespi *)filp->private_data;
	poll_wait(filp, &priv->wq_poll, wait);

	if (kfifo_len(&priv->r_fifo) > 0) {
		mask |= POLLIN | POLLRDNORM;
	}

	if (!kfifo_is_full(&priv->w_fifo)) {
		mask |= POLLOUT;
	}

	return mask;
}

static struct file_operations slavespi_fops = {
	.open		= slavespi_open,
	.read		= slavespi_read,
	.write		= slavespi_write,
	.unlocked_ioctl	= slavespi_ioctl,
	.release	= slavespi_release,
	.poll		= slavespi_poll,
};

static int ambarella_slavespi_probe(struct platform_device *pdev)
{
	int err = 0;
	int irq = 0;
	struct pinctrl *pins = NULL;
	void __iomem	*reg = NULL;
	struct resource *res = NULL;
	struct ambarella_slavespi *priv = NULL;

	/* Get Base Address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "get mem resource failed\n");
		err = -ENOENT;
		goto err_out;
	}

	reg = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!reg) {
		dev_err(&pdev->dev, "devm_ioremap() failed\n");
		err = -ENOMEM;
		goto err_out;
	}

	/* Get IRQ NO. */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "get irq failed\n");
		err = -ENOENT;
		goto err_out;
	}

	pins = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(pins)) {
		dev_err(&pdev->dev, "Failed to request pinctrl\n");
		goto err_out;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "no memory\n");
		err = -ENOMEM;
		goto err_out;
	}

	priv->phys = res->start;
	priv->wakeup_mask = 0;
	priv->regbase = reg;
	priv->mode = 3;			/* Attention [only support mode 3!!!], force!!! */
	priv->bpw = 8;			/* use dts to determinate ? */
	priv->irq = irq;
	priv->byte_wise = 0;
	priv->fifo_depth = SPI_FIFO_DEPTH;
	priv->buf_size = SLAVE_SPI_BUF_SIZE;
	priv->timeout = 0;		/* no timeout any more; use poll instead */
	priv->pltdev = &pdev->dev;
	priv->dma_used = 0;

	atomic_set(&priv->open_once, 1);
	init_waitqueue_head(&priv->wq_rhead);
	init_waitqueue_head(&priv->wq_whead);
	init_waitqueue_head(&priv->wq_poll);

	mutex_init(&priv->um_mtx);
	spin_lock_init(&priv->r_buf_lock);
	spin_lock_init(&priv->w_buf_lock);
	spin_lock_init(&priv->wakeup_lock);
	tasklet_init(&priv->wakeup_helper, wakeup_tasklet, (unsigned long)priv);

	if (of_find_property(pdev->dev.of_node, "amb,dma-used", NULL)) {
		priv->dma_used = 1;
		err = ambarella_spislave_dma_allocate(priv, SPI_DMA_TX);
		if (err) {
			goto err_out;
		}
		err = ambarella_spislave_dma_allocate(priv, SPI_DMA_RX);
		if (err) {
			ambarella_spi_slave_dma_free(priv, SPI_DMA_TX);
			goto err_out;
		}
	}

	priv->dname = "slavespi";
	err = alloc_chrdev_region(&priv->devno, 0, 1, priv->dname);
	if (err) {
		dev_err(&pdev->dev, "alloc chrdevno failed \n");
		err = -ENODEV;
		goto err_out;
	}
	cdev_init(&priv->cdevx, &slavespi_fops);
	priv->cdevx.owner = THIS_MODULE;
	cdev_add(&priv->cdevx, priv->devno, 1);
	priv->dclass = class_create(THIS_MODULE, priv->dname);
	if (IS_ERR(priv->dclass)) {
		err = -EIO;
		dev_err(&pdev->dev, "Error: %s: Unable to create class\n", __FILE__);
		goto ambarella_slavespi_probe_exit;
	}
	priv->device = device_create(priv->dclass, NULL, priv->devno, priv, priv->dname);
	if (IS_ERR(priv->device)) {
		err = -EIO;
		dev_err(&pdev->dev, "Error: %s: Unable to create device\n", __FILE__);
		goto ambarella_slavespi_probe_exit;
	}

	if (!kfifo_initialized(&priv->w_fifo)) { /* wfifo = 256PAGE */
		err = kfifo_alloc(&priv->w_fifo, priv->buf_size << 8, GFP_KERNEL);
		if (err) {
			err = -ENOMEM;
			dev_err(&pdev->dev, "Alloc w_kfifo failed \n");
			goto ambarella_slavespi_probe_exit;
		}
	}

	priv->w_buf = kmalloc(priv->buf_size, GFP_KERNEL);
	if (!priv->w_buf) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "Not able to allocate the write buffer\n");
		goto ambarella_slavespi_probe_exit;
	}

	if (!kfifo_initialized(&priv->r_fifo)) { /* rfifo = 256PAGE */
		err = kfifo_alloc(&priv->r_fifo, priv->buf_size << 8, GFP_KERNEL);
		if (err) {
			err = -ENOMEM;
			dev_err(&pdev->dev, "Alloc r_kfifo failed \n");
			goto ambarella_slavespi_probe_exit;
		}
	}

	priv->r_buf = kmalloc(priv->buf_size, GFP_KERNEL);
	if (!priv->r_buf) {
		err = -ENOMEM;
		dev_err(&pdev->dev, "Not able to allocate the read buffer\n");
		goto ambarella_slavespi_probe_exit;
	}

	if (0 == priv->dma_used) {
		err = devm_request_irq(&pdev->dev, priv->irq, ambarella_slavespi_isr,
			IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), priv);
		if (err) {
			err = -EIO;
			dev_err(&pdev->dev, "Request spi interrupt error \n");
			goto ambarella_slavespi_probe_exit;
		}
	}

	platform_set_drvdata(pdev, priv);
	dev_info(&pdev->dev, "slavespi: probe ok using %s \n", priv->dma_used ? "dma-mode":"irq-mode");

	return 0;

ambarella_slavespi_probe_exit:
	if (priv->r_buf) {
		kfree(priv->r_buf);
		priv->r_buf = NULL;
	}

	if (kfifo_initialized(&priv->r_fifo)) {
		kfifo_free(&priv->r_fifo);
	}

	if (priv->w_buf) {
		kfree(priv->w_buf);
		priv->w_buf = NULL;
	}

	if (kfifo_initialized(&priv->w_fifo)) {
		kfifo_free(&priv->w_fifo);
	}

	if (priv->device) {
		device_destroy(priv->dclass, priv->devno);
		priv->device = NULL;
	}

	if (priv->dclass) {
		class_destroy(priv->dclass);
		priv->dclass = NULL;
	}

	if (priv->devno) {
		cdev_del(&priv->cdevx);
		unregister_chrdev_region(priv->devno, 1);
		priv->devno = 0;
	}

err_out:
	printk("spislave probe failed \n");
	return err;
}

static int ambarella_slavespi_remove(struct platform_device *pdev)
{
	struct ambarella_slavespi *priv = NULL;

	priv = platform_get_drvdata(pdev);

	if (priv->dma_used) {
		writel_relaxed(SPI_DMA_EN_NONE, priv->regbase + SPI_DMAC_OFFSET);
		writel_relaxed(0, priv->regbase + SPI_IMR_OFFSET);

		dmaengine_terminate_sync(priv->tx_dma_chan);
		dmaengine_terminate_sync(priv->rx_dma_chan);

		ambarella_spi_slave_dma_free(priv, SPI_DMA_TX);
		ambarella_spi_slave_dma_free(priv, SPI_DMA_RX);
	}

	if (priv->r_buf) {
		kfree(priv->r_buf);
		priv->r_buf = NULL;
	}

	if (kfifo_initialized(&priv->r_fifo)) {
		kfifo_free(&priv->r_fifo);
	}

	if (priv->w_buf) {
		kfree(priv->w_buf);
		priv->w_buf = NULL;
	}

	if (kfifo_initialized(&priv->w_fifo)) {
		kfifo_free(&priv->w_fifo);
	}

	if (priv->device) {
		device_destroy(priv->dclass, priv->devno);
		priv->device = NULL;
	}

	if (priv->dclass) {
		class_destroy(priv->dclass);
		priv->dclass = NULL;
	}

	if (priv->devno) {
		cdev_del(&priv->cdevx);
		unregister_chrdev_region(priv->devno, 1);
		priv->devno = 0;
	}

	/* shutdown controller */
	writel_relaxed(0, priv->regbase + SPI_SSIENR_OFFSET);
	return 0;
}


#ifdef CONFIG_PM
static int ambarella_slavespi_suspend_noirq(struct device *dev)
{
	return 0;
}

static int ambarella_slavespi_resume_noirq(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops ambarella_slavespi_dev_pm_ops = {
	.suspend_noirq = ambarella_slavespi_suspend_noirq,
	.resume_noirq = ambarella_slavespi_resume_noirq,
};
#endif

static const struct of_device_id ambarella_slavespi_dt_ids[] = {
	{.compatible = "ambarella,spi-slave", },
	{},
};
MODULE_DEVICE_TABLE(of, ambarella_slavespi_dt_ids);

static struct platform_driver ambarella_slavespi_driver = {
	.probe		= ambarella_slavespi_probe,
	.remove		= ambarella_slavespi_remove,
	.driver		= {
		.name	= "ambarella-spi-slave",
		.owner	= THIS_MODULE,
		.of_match_table = ambarella_slavespi_dt_ids,
#ifdef CONFIG_PM
		.pm	= &ambarella_slavespi_dev_pm_ops,
#endif
	},
};

static int __init ambarella_slavespi_init(void)
{
	return platform_driver_register(&ambarella_slavespi_driver);
}

static void __exit ambarella_slavespi_exit(void)
{
	platform_driver_unregister(&ambarella_slavespi_driver);
}

subsys_initcall(ambarella_slavespi_init);
module_exit(ambarella_slavespi_exit);

MODULE_DESCRIPTION("Ambarella Media Processor SPI Slave Controller");
MODULE_AUTHOR("Zhenwu Xue, <zwxue@ambarella.com>");
MODULE_LICENSE("GPL");
