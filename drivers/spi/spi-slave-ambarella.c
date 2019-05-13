/*
 * linux/drivers/spi/spi-slave-ambarella.c
 *
 * History:
 *	2012/02/21 - [Zhenwu Xue]  created file
 *
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

#define SPI_FIFO_DEPTH 		    (SPI_DATA_FIFO_SIZE_32)
#define SPI_FIFO_DEPTH_MSK      (SPI_FIFO_DEPTH - 1)

#define SLAVE_SPI_BUF_SIZE	PAGE_SIZE


/* export to user-space used by ioctl */
struct slavespi_transfer {
	void __user *tx_buf;
	void __user *rx_buf;
	unsigned int buf_len;

	unsigned int txd_len;
	unsigned int rxd_len;
};

#define SPI_SLAVE_MAGIC   (250)
#define SPI_SLAVE_SET_TIMEOUT    _IOW(SPI_SLAVE_MAGIC, 0, int)
#define SPI_SLAVE_IO_TRANSFER    _IOW(SPI_SLAVE_MAGIC, 1, struct slavespi_transfer)

/* slavespi controller */
struct ambarella_slavespi {

	int irq;
	void __iomem	*regbase;

	dev_t 	devno;
	struct cdev  	cdevx;
	struct device	*device;
	const char *dname;
	struct class *dclass;

	u32	mode;
	u32	bpw;
	u32 byte_wise;
	u32	fifo_depth;

	u32 buf_size;

	void	*w_buf;
	void	*r_buf;

	struct kfifo	w_fifo;
	struct kfifo	r_fifo;

	u32 want_read;
	spinlock_t w_buf_lock;
	spinlock_t r_buf_lock;

	atomic_t open_once;
	spinlock_t hw_lock;  /* access hardware */
	struct mutex um_mtx;

	int timeout;
	struct wait_queue_head wq_rhead;
	struct wait_queue_head wq_whead;
	struct wait_queue_head wq_poll;
};

static int ambarella_slavespi_setup(struct ambarella_slavespi *priv)
{
	struct clk *clk;
	spi_ctrl0_reg_t ctrl0_reg;

	clk = clk_get(NULL, "gclk_ssi2");
	if (!IS_ERR_OR_NULL(clk)) {
		clk_set_rate(clk, 196000000);
	} else {
		dev_err(priv->device, "slavespi_bus: get clock failed \n");
		return -EINVAL;
	}

	/* clear any fifo */
	writel_relaxed(1, priv->regbase + SPI_SSIENR_OFFSET);
	udelay(1);
	writel_relaxed(0, priv->regbase + SPI_SSIENR_OFFSET);
	udelay(1);

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

	ctrl0_reg.s.byte_ws = 0;  /* PLEASE DO NOT USE BYTE_WISE FIFO */
	ctrl0_reg.s.hold = 0;
	ctrl0_reg.s.fc_en = 0;
	ctrl0_reg.s.residue = 1;
	writel_relaxed(ctrl0_reg.w, priv->regbase + SPI_CTRLR0_OFFSET);

	/* reset read/write fifo */
	kfifo_reset(&priv->w_fifo);
	kfifo_reset(&priv->r_fifo);

	/* TX_RX_Threshold 8bytes */
	writel_relaxed(7, priv->regbase + SPI_TXFTLR_OFFSET);
	writel_relaxed(7, priv->regbase + SPI_RXFTLR_OFFSET);

	/* recvfifo interrupt */
	writel_relaxed(SPI_RXFIS_MASK, priv->regbase + SPI_IMR_OFFSET);
	writel_relaxed(0x0, priv->regbase + SPI_DMAC_OFFSET);

	smp_wmb();
	writel_relaxed(1, priv->regbase + SPI_SSIENR_OFFSET);

	return 0;
}

/* drain any data in hardware rxfifo , used with spinlock */
static void slavespi_drain_rxfifo(struct ambarella_slavespi *priv)
{
	u32 i = 0;
	u32 len = 0;
	u32 rxflr = 0;

	rxflr = readl_relaxed(priv->regbase + SPI_RXFLR_OFFSET);
	for (i = 0; i < rxflr; i++) {
		if (!priv->byte_wise) {
			((u16 *)priv->r_buf)[i] = readl_relaxed(priv->regbase + SPI_DR_OFFSET);
		} else {
			((u8 *)priv->r_buf)[i] = readl_relaxed(priv->regbase + SPI_DR_OFFSET);
		}
	}
	if (!priv->byte_wise) {
		rxflr <<= 1;
	}
	len = kfifo_in(&priv->r_fifo, priv->r_buf, rxflr);
	if (len < rxflr) {
		i = rxflr - len;
		while (i--) { /* drop oldest */
			kfifo_skip(&priv->r_fifo);
		}

		/* over write oldest */
		len = kfifo_in(&priv->r_fifo, priv->r_buf + len, (rxflr - len));
		if (len) {
			pr_warn("spislave: recv drop %u bytes \n", len);
		}
	}
}

static void ambarella_tx_isr_func(unsigned int spisr, void *dev_data)
{
	u32 i = 0;
	u32 imr = 0;
	u32 risr = 0;
	u32 ept_fifo = 0;
	u32 ept_bytes = 0;
	u32 wfifo_len = 0;

	struct ambarella_slavespi *priv	= dev_data;
	risr = readl_relaxed(priv->regbase + SPI_RISR_OFFSET);

	if ((!(spisr & SPI_TXEIS_MASK)) && (!(risr & SPI_TXEIS_MASK))) {
		return ;
	}

	spin_lock(&priv->w_buf_lock);

	ept_fifo = priv->fifo_depth - readl_relaxed(priv->regbase + SPI_TXFLR_OFFSET);
	if (kfifo_is_empty(&priv->w_fifo)) {
		/* w_fifo is empty then disable txfifo empty interrupt */
		imr = readl_relaxed(priv->regbase + SPI_IMR_OFFSET);
		writel_relaxed(imr & ~SPI_TXEIS_MASK, priv->regbase + SPI_IMR_OFFSET);
		spin_unlock(&priv->w_buf_lock);
		wake_up_interruptible(&priv->wq_whead);
		return;
	}

	ept_bytes = ept_fifo;
	if (!priv->byte_wise) {
		ept_bytes <<= 1;
	}
	wfifo_len = kfifo_len(&priv->w_fifo);
	ept_bytes = min_t(u32, ept_bytes, wfifo_len);

	if (!priv->byte_wise) {
		ept_bytes &= ~1;
	}

	ept_bytes = kfifo_out(&priv->w_fifo, priv->w_buf, ept_bytes);
	if (!priv->byte_wise) {
		ept_bytes >>= 1;
	}

	for (i = 0; i < ept_bytes; i++) {
		if (!priv->byte_wise) {
			writel_relaxed(((u16 *)priv->w_buf)[i], \
				priv->regbase + SPI_DR_OFFSET);
		} else {
			writel_relaxed(((u8 *)priv->w_buf)[i], \
				priv->regbase + SPI_DR_OFFSET);
		}
	}
	spin_unlock(&priv->w_buf_lock);

	/* always enable TXEIS interrupt at here */
	spin_lock(&priv->hw_lock);
	imr = readl_relaxed(priv->regbase + SPI_IMR_OFFSET);
	writel_relaxed((imr | SPI_TXEIS_MASK), priv->regbase + SPI_IMR_OFFSET);
	spin_unlock(&priv->hw_lock);
}

static void ambarella_rx_isr_func(unsigned int spisr, void *dev_data)
{
	u32 risr = 0;

	struct ambarella_slavespi *priv = dev_data;
	risr = readl_relaxed(priv->regbase + SPI_RISR_OFFSET);

	if (!(spisr & SPI_RXFIS_MASK) && !(risr & SPI_RXFIS_MASK)) {
		return ;
	}

	spin_lock(&priv->r_buf_lock);

	slavespi_drain_rxfifo(priv);

	if (kfifo_len(&priv->r_fifo) >= priv->want_read) {
		wake_up_interruptible(&priv->wq_rhead);
	}

	spin_unlock(&priv->r_buf_lock);
}

static irqreturn_t ambarella_slavespi_isr(int irq, void *dev_data)
{
	u32 status = 0;
	struct ambarella_slavespi *priv	= dev_data;

	status = readl_relaxed(priv->regbase + SPI_ISR_OFFSET);

	ambarella_tx_isr_func(status, dev_data);
	ambarella_rx_isr_func(status, dev_data);

	writel_relaxed(0, priv->regbase + SPI_ISR_OFFSET);
	writel_relaxed(0, priv->regbase + SPI_RISR_OFFSET);

	return IRQ_HANDLED;
}

static int slavespi_open(struct inode *inode, struct file *filp)
{
	struct ambarella_slavespi *priv = NULL;

	priv = container_of(inode->i_cdev, struct ambarella_slavespi, cdevx);
	if (!priv) {
		pr_info("slavespi: no such dev info \n");
		return -EINVAL;
	}
	filp->private_data = priv;

	if (!atomic_dec_and_test(&priv->open_once)) {
    		atomic_inc(&priv->open_once);
    		dev_info(priv->device, "slavespi: device is busy \n");
    		return -EBUSY;
	}

	ambarella_slavespi_setup(priv);
	dev_info(priv->device, "slavespi: open ok \n");
	return 0;
}

static ssize_t
slavespi_unlock_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos)
{
	int err = 0;
	int ok_wait = 0;
	unsigned int sr = 0;
	unsigned int imr = 0;
	unsigned long flags;

	unsigned int txflr = 0;
	unsigned int copied = 0;
	unsigned int copied_saved = 0;

	unsigned int try_times = 3;
	struct ambarella_slavespi *priv = NULL;

	priv = (struct ambarella_slavespi *)filp->private_data;
	if (!priv->byte_wise) {
		count &= ~1;
	}

	if (!buf) {
		return 0;
	}

	spin_lock_irqsave(&priv->w_buf_lock, flags);
	if (!(filp->f_flags & O_NONBLOCK)) {
		kfifo_reset_out(&priv->w_fifo); /* will block */
	}
	err = kfifo_from_user(&priv->w_fifo, buf, count, &copied);
	spin_unlock_irqrestore(&priv->w_buf_lock, flags);

	if (err || (copied != count)) {
		pr_info("slavespi: block_write partitional copied \n");
	}

	spin_lock_irqsave(&priv->hw_lock, flags);
	imr = readl_relaxed(priv->regbase + SPI_IMR_OFFSET);
	writel_relaxed((imr | SPI_TXEIS_MASK), priv->regbase + SPI_IMR_OFFSET);
	spin_unlock_irqrestore(&priv->hw_lock, flags);

	if (filp->f_flags & O_NONBLOCK) {
		return copied;
	}

	/* flowing is block with timeout support */
	if (priv->timeout > 0) {
		ok_wait = wait_event_interruptible_timeout(priv->wq_whead, \
				kfifo_is_empty(&priv->w_fifo), priv->timeout);
		if (ok_wait > 0) {
			return copied;
		}
	} else {
		ok_wait = wait_event_interruptible(priv->wq_whead, \
				kfifo_is_empty(&priv->w_fifo));
		if (0 == ok_wait) {
			return copied;
		}
	}

retry:
	/* wait until spi idle */
	do {
		sr = readl_relaxed(priv->regbase + SPI_SR_OFFSET);
		if (sr & SPI_SR_BUSY_MSK) {
			msleep_interruptible(2);
		} else {
			break;
		}
	} while (1);

	/* drain rxfifo */
	spin_lock_irqsave(&priv->r_buf_lock, flags);
	slavespi_drain_rxfifo(priv);
	spin_unlock_irqrestore(&priv->r_buf_lock, flags);

	/* caculate txd bytes */
	spin_lock_irqsave(&priv->w_buf_lock, flags);
	copied -= kfifo_len(&priv->w_fifo);
	txflr = readl_relaxed(priv->regbase + SPI_TXFLR_OFFSET);
	if (!priv->byte_wise) {
		txflr <<= 1;
	}
	copied -= txflr;
	spin_unlock_irqrestore(&priv->w_buf_lock, flags);

	/* check spi is still idle or not */
	sr = readl_relaxed(priv->regbase + SPI_SR_OFFSET);
	if (sr & SPI_SR_BUSY_MSK) {
		if (try_times) {
			try_times--;
			copied = copied_saved;
			goto retry;
		}
	}

	/* clean all w_fifo  && reset hardware txfifo/rxfifo */
	kfifo_reset_out(&priv->w_fifo);
	writel_relaxed(0, priv->regbase + SPI_SSIENR_OFFSET);
	smp_wmb();
	writel_relaxed(1, priv->regbase + SPI_SSIENR_OFFSET);

	return copied;
}

static ssize_t
slavespi_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	u32 copied = 0;
	struct ambarella_slavespi *priv = NULL;
	priv = (struct ambarella_slavespi *)filp->private_data;

	if (!buf) {
		return 0;
	}

	mutex_lock(&priv->um_mtx);
	copied = slavespi_unlock_write(filp, (const char __user *)buf, count, f_pos);
	mutex_unlock(&priv->um_mtx);

	return copied;
}

static ssize_t
slavespi_unblk_read(struct ambarella_slavespi *priv, char __user *buf, size_t count)
{
	int err = 0;
	u32 copied = 0;
	unsigned long flags;

	if (!priv->byte_wise) {
		count &= ~1;
	}

	if (!buf) {
		return 0;
	}

	spin_lock_irqsave(&priv->r_buf_lock, flags);
	slavespi_drain_rxfifo(priv);
	err = kfifo_to_user(&priv->r_fifo, buf, count, &copied);
	if (err) {
		pr_warn("<1> read partitional transfer \n");
	}
	spin_unlock_irqrestore(&priv->r_buf_lock, flags);

	return copied;
}

static ssize_t
slavespi_unlock_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int err = 0;
	u32 copied = 0;
	unsigned long flags;
	struct ambarella_slavespi *priv = NULL;

	priv = (struct ambarella_slavespi *)filp->private_data;
	if (!priv->byte_wise) {
		count &= ~1;
	}

	if (filp->f_flags & O_NONBLOCK) {
		return slavespi_unblk_read(priv, (char __user *)buf, count);
	}

	priv->want_read = count;
	if (priv->timeout > 0) {
		wait_event_interruptible_timeout(priv->wq_rhead, \
			(kfifo_len(&priv->r_fifo) >= count), priv->timeout);
	} else {
		wait_event_interruptible(priv->wq_rhead, \
			(kfifo_len(&priv->r_fifo) >= count));
	}

	spin_lock_irqsave(&priv->r_buf_lock, flags);
	slavespi_drain_rxfifo(priv);
	err = kfifo_to_user(&priv->r_fifo, buf, count, &copied);
	if (err) {
		pr_warn("<2> read partitional transfer \n");
	}
	spin_unlock_irqrestore(&priv->r_buf_lock, flags);

	return copied;
}

static ssize_t
slavespi_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	u32 copied = 0;
	struct ambarella_slavespi *priv = NULL;
	priv = (struct ambarella_slavespi *)filp->private_data;

	if (!buf) {
		return 0;
	}

	mutex_lock(&priv->um_mtx);
	copied = slavespi_unlock_read(filp, (char __user *)buf, count, f_pos);
	mutex_unlock(&priv->um_mtx);

	return copied;
}

static long
slavespi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	u32 left = 0;
	int tmout = 0;
	loff_t f_pos = 0;
	struct slavespi_transfer trans;
	struct ambarella_slavespi *priv = NULL;

	memset(&trans, 0, sizeof(trans));
	priv = (struct ambarella_slavespi *)file->private_data;


	switch (cmd) {
		case SPI_SLAVE_SET_TIMEOUT:
			get_user(tmout, (__s32 *)arg);
			pr_info("--timeout is %d \n", tmout);
			priv->timeout = tmout * HZ;
			return 0;

		case SPI_SLAVE_IO_TRANSFER:
			left = copy_from_user(&trans, (struct slavespi_transfer * __user)arg,
				sizeof(trans));
			if (left) {
				return -EAGAIN;
			}
			break;

		default:
			return 0;
	}

	mutex_lock(&priv->um_mtx);

	trans.txd_len = slavespi_unlock_write(file, trans.tx_buf, trans.buf_len, &f_pos);
	/* always use unblk for read, read latest data */
	trans.rxd_len = slavespi_unblk_read(priv, trans.rx_buf, trans.buf_len);

	mutex_unlock(&priv->um_mtx);

	left = copy_to_user((struct slavespi_transfer * __user)arg, &trans, sizeof(trans));
	if (left) {
		return -EAGAIN;
	}

	return 0;
}

static int slavespi_release(struct inode *inode, struct file *filp)
{
	struct ambarella_slavespi *priv = NULL;
	priv = (struct ambarella_slavespi *)filp->private_data;

	writel_relaxed(0, priv->regbase + SPI_SSIENR_OFFSET);

	kfifo_reset(&priv->w_fifo);
	kfifo_reset(&priv->r_fifo);

	atomic_inc(&priv->open_once);

	return 0;
}

static unsigned int
slavespi_poll(struct file *filp, struct poll_table_struct *wait)
{
	unsigned long flags;
	unsigned int mask = 0;
	struct ambarella_slavespi *priv = NULL;
	priv = (struct ambarella_slavespi *)filp->private_data;

	poll_wait(filp, &priv->wq_poll, wait);

	spin_lock_irqsave(&priv->r_buf_lock, flags);
	if (kfifo_len(&priv->r_fifo) > 0) { /* allow read and can read some data */
		mask |= POLLIN | POLLRDNORM;
	}
	spin_unlock_irqrestore(&priv->r_buf_lock, flags);

	spin_lock_irqsave(&priv->w_buf_lock, flags);
	if (kfifo_avail(&priv->w_fifo) > 0) { /* allow write but may write nothing */
		mask |= POLLOUT;
	}
	spin_unlock_irqrestore(&priv->w_buf_lock, flags);

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

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "no memory\n");
		err = -ENOMEM;
		goto err_out;
	}

	priv->regbase = reg;
	priv->mode = 0;
	priv->bpw = 8;
	priv->irq = irq;
	priv->byte_wise = 1;
	priv->fifo_depth = SPI_FIFO_DEPTH;
	priv->buf_size = SLAVE_SPI_BUF_SIZE;
	priv->timeout = HZ;
	atomic_set(&priv->open_once, 1);
	init_waitqueue_head(&priv->wq_rhead);
	init_waitqueue_head(&priv->wq_whead);
	init_waitqueue_head(&priv->wq_poll);

	mutex_init(&priv->um_mtx);
	spin_lock_init(&priv->r_buf_lock);
	spin_lock_init(&priv->w_buf_lock);
	spin_lock_init(&priv->hw_lock);

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

	if (!kfifo_initialized(&priv->w_fifo)) { /* wfifo = 2PAGE */
		err = kfifo_alloc(&priv->w_fifo, priv->buf_size << 1, GFP_KERNEL);
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

	err = devm_request_irq(&pdev->dev, priv->irq, ambarella_slavespi_isr,
			IRQF_TRIGGER_HIGH, dev_name(&pdev->dev), priv);
	if (err) {
		err = -EIO;
		dev_err(&pdev->dev, "Request spi interrupt error \n");
		goto ambarella_slavespi_probe_exit;
	}

	platform_set_drvdata(pdev, priv);

	dev_info(&pdev->dev, "--probe--ok-- \n");

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
	return err;
}

static int ambarella_slavespi_remove(struct platform_device *pdev)
{
	struct ambarella_slavespi *priv = NULL;

	priv = platform_get_drvdata(pdev);

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

	/* shutdown clock power reset controller */
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
