/*
 * linux/drivers/spi/spi_ambarella.c
 *
 * History:
 *	2008/03/03 - [Louis Sun]  created file
 *	2009/06/19 - [Zhenwu Xue] ported from 2.6.22.10

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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <asm/io.h>
#include <mach/io.h>
#include "spi_ambarella.h"

/*============================Global Variables================================*/

struct ambarella_spi {
	u32					regbase;
	int					irq;
	struct ambarella_spi_platform_info	*pinfo;

	struct tasklet_struct			tasklet;
	spinlock_t				lock;

	struct list_head			queue;
	struct spi_transfer			*c_xfer;
	u32					shutdown;

	struct spi_device			*c_dev;
	u8					rw_mode, bpw;
	u32					ridx, widx, len;
};

struct ambarella_spi_private {
	struct spi_device			*spi;
	struct mutex				mtx;
	spinlock_t				lock;
};

static struct {
	int					cs_num;
	struct ambarella_spi_private		*data;
} ambarella_spi_private_devices[SPI_INSTANCES];

const static u8 ambarella_spi_reverse_table[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

static void ambarella_spi_do_message(struct spi_master *master);
static void ambarella_spi_do_xfer(struct spi_master *master);


/*============================SPI Bus Driver==================================*/

static int ambarella_spi_disable(struct spi_master *master)
{
	struct ambarella_spi	*as = spi_master_get_devdata(master);

	amba_readl(as->regbase + SPI_ICR_OFFSET);
	amba_readl(as->regbase + SPI_ISR_OFFSET);
	amba_writel(as->regbase + SPI_SER_OFFSET, 0);
	amba_writel(as->regbase + SPI_SSIENR_OFFSET, 0);

	return 0;
}

static int ambarella_spi_setup(struct spi_device *spi)
{
	return 0;
}

static u8 ambarella_spi_reverse_8bit(u8 x)
{
	return ambarella_spi_reverse_table[x];
}

static u16 ambarella_spi_reverse_16bit(u16 x)
{
	u16 y;

	y = ambarella_spi_reverse_table[x >> 8];
	y |= (ambarella_spi_reverse_table[x & 0xff] << 8);

	return y;
}

static void ambarella_spi_tasklet(unsigned long data)
{
	struct spi_master		*master = (struct spi_master *)data;
	struct ambarella_spi		*as = spi_master_get_devdata(master);
	void 				*wbuf, *rbuf;
	u32				widx, ridx, len;
	u32				rxflr, xfer_len;
	u32				status;
	u8				cs_id, lsb;
	u16				i, tmp;

	/* Wait until SPI idle */
	status = amba_readl(as->regbase + SPI_SR_OFFSET);
	if (status & 0x1) {
		/* Transfer is still in progress */
		for (i = 0; i < MAX_QUERY_TIMES; i++) {
			status = amba_readl(as->regbase + SPI_SR_OFFSET);
			if (!(status & 0x1))
				break;
		}
		if (status & 0x1) {
			tasklet_schedule(&as->tasklet);
			return;
		}
	}

	wbuf = (void *)as->c_xfer->tx_buf;
	rbuf = (void *)as->c_xfer->rx_buf;
	len = as->len;
	cs_id = as->c_dev->chip_select;
	lsb = as->c_dev->mode & SPI_LSB_FIRST;
	widx = as->widx;
	ridx = as->ridx;

	/* Feed/Fetch data into/from FIFO */
	switch (as->rw_mode) {
	case SPI_WRITE_ONLY:
		if (widx == len)
			goto finish_transfer;

		xfer_len = len - widx;
		if (xfer_len > SPI_FIFO_LENGTH)
			xfer_len = SPI_FIFO_LENGTH;

		amba_writel(as->regbase + SPI_SER_OFFSET, 0);
		if (as->bpw <= 8) {
			for(i = 0; i < xfer_len; i++) {
				tmp = ((u8 *)wbuf)[widx++];
				if (lsb) {
					tmp = ambarella_spi_reverse_8bit(tmp);
					tmp = tmp >> (8 - as->bpw);
				}
				amba_writel(as->regbase + SPI_DR_OFFSET, tmp);
			}
		}else{
			for(i = 0; i < xfer_len; i++) {
				tmp = ((u16 *)wbuf)[widx++];
				if (lsb) {
					tmp = ambarella_spi_reverse_16bit(tmp);
					tmp = tmp >> (16 - as->bpw);
				}
				amba_writel(as->regbase + SPI_DR_OFFSET, tmp);
			}
		}
		amba_writel(as->regbase + SPI_SER_OFFSET, 1 << cs_id);
		goto continue_transfer;

		break;

	case SPI_WRITE_READ:
		/* Fetch data from Rx FIFO */
		xfer_len = len - ridx;
		rxflr = amba_readl(as->regbase + SPI_RXFLR_OFFSET);
		if (xfer_len > rxflr)
			xfer_len = rxflr;

		if (as->bpw <= 8) {
			for(i = 0; i < xfer_len; i++) {
				tmp = amba_readl(as->regbase + SPI_DR_OFFSET);
				if (lsb) {
					tmp = ambarella_spi_reverse_8bit(tmp);
					tmp = tmp >> (8 - as->bpw);
				}
				((u8 *)rbuf)[ridx++] = tmp & 0xff;
			}
		} else {
			for(i = 0; i < xfer_len; i++){
				tmp = amba_readl(as->regbase + SPI_DR_OFFSET);
				if (lsb) {
					tmp = ambarella_spi_reverse_16bit(tmp);
					tmp = tmp >> (16 - as->bpw);
				}
				((u16 *)rbuf)[ridx++] = tmp;
			}
		}

		/* Check whether finished */
		if (ridx == len && widx == len)
			goto finish_transfer;

		/* Feed data into Tx FIFO */
		xfer_len = len - widx;
		if (xfer_len > SPI_FIFO_LENGTH)
			xfer_len = SPI_FIFO_LENGTH;

		if (xfer_len) {
			amba_writel(as->regbase + SPI_SER_OFFSET, 0);
			if (as->bpw <= 8) {
				for(i = 0; i < xfer_len; i++) {
					tmp = ((u8 *)wbuf)[widx++];
					if (lsb) {
						tmp = ambarella_spi_reverse_8bit(tmp);
						tmp = tmp >> (8 - as->bpw);
					}
					amba_writel(as->regbase + SPI_DR_OFFSET, tmp);
				}
			}else{
				for(i = 0; i < xfer_len; i++) {
					tmp = ((u16 *)wbuf)[widx++];
					if (lsb) {
						tmp = ambarella_spi_reverse_16bit(tmp);
						tmp = tmp >> (16 - as->bpw);
					}
					amba_writel(as->regbase + SPI_DR_OFFSET, tmp);
				}
			}
			amba_writel(as->regbase + SPI_SER_OFFSET, 1 << cs_id);
			goto continue_transfer;
		} else {
			printk("SPI Exception!\n");
			goto finish_transfer;
		}

		break;

	case SPI_READ_ONLY:
		/* Fetch data from Rx FIFO */
		xfer_len = len - ridx;
		rxflr = amba_readl(as->regbase + SPI_RXFLR_OFFSET);
		if (xfer_len > rxflr)
			xfer_len = rxflr;

		if (as->bpw <= 8) {
			for(i = 0; i < xfer_len; i++){
				tmp = amba_readl(as->regbase + SPI_DR_OFFSET);
				if (lsb) {
					tmp = ambarella_spi_reverse_8bit(tmp);
					tmp = tmp >> (8 - as->bpw);
				}
				((u8 *)rbuf)[ridx++] = tmp & 0xff;
			}
		} else {
			for(i = 0; i < xfer_len; i++){
				tmp = amba_readl(as->regbase + SPI_DR_OFFSET);
				if (lsb) {
					tmp = ambarella_spi_reverse_16bit(tmp);
					tmp = tmp >> (16 - as->bpw);
				}
				((u16 *)rbuf)[ridx++] = tmp;
			}
		}

		/* Check whether receiving data completed */
		if (ridx == len)
			goto finish_transfer;

		/* Feed data into FIFO */
		xfer_len = len - ridx;
		if (xfer_len > SPI_FIFO_LENGTH)
			xfer_len = SPI_FIFO_LENGTH;

		if (xfer_len) {
			amba_writel(as->regbase + SPI_SER_OFFSET, 0);
			for(i = 0; i < xfer_len; i++)
				amba_writel(as->regbase + SPI_DR_OFFSET, SPI_DUMMY_DATA);
			amba_writel(as->regbase + SPI_SER_OFFSET, 1 << cs_id);
			goto continue_transfer;
		} else {
			printk("SPI Exception!\n");
			goto finish_transfer;
		}

		break;

	default:
		break;
	}

finish_transfer:
	ambarella_spi_do_xfer(master);
	if (as->pinfo->use_interrupt)
		enable_irq(as->irq);
	return;

continue_transfer:
	as->widx = widx;
	as->ridx = ridx;
	if (as->pinfo->use_interrupt) {
		enable_irq(as->irq);
	} else {
		do {
			status = amba_readl(as->regbase + SPI_SR_OFFSET) & 0x5;
		} while (status != 0x4);

		ambarella_spi_tasklet((unsigned long)master);
	}
	return;
}

static void ambarella_spi_do_xfer(struct spi_master *master)
{
	struct ambarella_spi		*as = spi_master_get_devdata(master);
	struct spi_message		*msg;
	struct spi_transfer		*xfer;
	struct ambarella_spi_cs_config  cs_config;
	u32				ctrlr0;
	void				*wbuf, *rbuf;

	msg = list_entry(as->queue.next, struct spi_message, queue);

	if (list_empty(&msg->transfers)) {

		if (as->pinfo->cs_deactivate) {
			cs_config.bus_id = master->bus_num;
			cs_config.cs_id = msg->spi->chip_select;
			cs_config.cs_num = master->num_chipselect;
			cs_config.cs_pins = as->pinfo->cs_pins;
			cs_config.cs_change = as->c_xfer->cs_change;
			as->pinfo->cs_deactivate(&cs_config);
		}
		ambarella_spi_disable(as->c_dev->master);

		msg->actual_length = as->c_xfer->len;
		msg->status = 0;
		msg->complete(msg->context);

		/* Next Message */
		spin_lock(&as->lock);
		list_del(as->queue.next);

		if (!list_empty(&as->queue)) {
			spin_unlock(&as->lock);
			ambarella_spi_do_message(master);
		} else {
			as->c_xfer = NULL;
			spin_unlock(&as->lock);
		}

		return;
	}

	/* Configure transfer */
	xfer = list_entry(msg->transfers.next, struct spi_transfer, transfer_list);
	list_del(msg->transfers.next);
	as->c_xfer = xfer;
	wbuf = (void *)xfer->tx_buf;
	rbuf = (void *)xfer->rx_buf;
	if (as->bpw <= 8)
		as->len = xfer->len;
	else
		as->len = xfer->len >> 1;
	as->widx = 0;
	as->ridx = 0;
	if (wbuf && !rbuf)
		as->rw_mode = SPI_WRITE_ONLY;
	if ( !wbuf && rbuf)
		as->rw_mode = SPI_READ_ONLY;
	if (wbuf && rbuf)
		as->rw_mode = SPI_WRITE_READ;

	ctrlr0 = amba_readl(as->regbase + SPI_CTRLR0_OFFSET);
	ctrlr0 &= 0xfffff4ff;
	ctrlr0 |= (as->rw_mode << 8);
	if (as->c_dev->mode & SPI_LOOP)
		ctrlr0 |= (0x1 << 11);

	amba_writel(as->regbase + SPI_CTRLR0_OFFSET, ctrlr0);

	if (as->pinfo->cs_activate) {
		cs_config.bus_id = master->bus_num;
		cs_config.cs_id = as->c_dev->chip_select;
		cs_config.cs_num = master->num_chipselect;
		cs_config.cs_pins = as->pinfo->cs_pins;
		cs_config.cs_change = as->c_xfer->cs_change;
		as->pinfo->cs_activate(&cs_config);
	}
	if (as->pinfo->use_interrupt) {
		disable_irq(as->irq);
		amba_writel(as->regbase + SPI_IMR_OFFSET, SPI_TXEIS_MASK);
	} else {
		amba_writel(as->regbase + SPI_IMR_OFFSET, 0);
	}

	/* Start transfer */
	amba_writel(as->regbase + SPI_SSIENR_OFFSET, 1);
	amba_writel(as->regbase + SPI_SER_OFFSET, 0);
	ambarella_spi_tasklet((unsigned long)master);
}

static void ambarella_spi_do_message(struct spi_master *master)
{
	u8				bus_id, cs_id;
	u32 				ctrlr0, ssi_clk, sckdv;
	struct ambarella_spi		*as = spi_master_get_devdata(master);
	struct spi_message		*msg;

	/* Get Message */
	msg = list_entry(as->queue.next, struct spi_message, queue);

	/* Restore setup from ambarella_spi_devdata */
	bus_id = master->bus_num;
	cs_id = msg->spi->chip_select;
	ctrlr0 = amba_readl(as->regbase + SPI_CTRLR0_OFFSET);

	if (msg->spi->bits_per_word < 4)
		msg->spi->bits_per_word = 4;
	if (msg->spi->bits_per_word > 16)
		msg->spi->bits_per_word = 16;
	as->bpw = msg->spi->bits_per_word;

	ctrlr0 &= 0xfffffff0;
	ctrlr0 |= (as->bpw - 1);

	ctrlr0 &= (~((1 << 6) | (1 << 7)));
	ctrlr0 |= ((msg->spi->mode & (SPI_CPHA | SPI_CPOL)) << 6);
	if (msg->spi->mode & SPI_LOOP) {
		ctrlr0 |= 0x00000800;
	}
	amba_writel(as->regbase + SPI_CTRLR0_OFFSET, ctrlr0);

	ssi_clk = as->pinfo->get_ssi_freq_hz();
	if(msg->spi->max_speed_hz == 0 || msg->spi->max_speed_hz > ssi_clk / 2)
	    msg->spi->max_speed_hz = ssi_clk / 2;
	sckdv = (u16)(((ssi_clk / msg->spi->max_speed_hz) + 0x01) & 0xfffe);
	amba_writel(as->regbase + SPI_BAUDR_OFFSET, sckdv);

	/* Start Xfer */
	as->c_dev = msg->spi;
	ambarella_spi_do_xfer(master);
}

static int ambarella_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct ambarella_spi		*as;
	struct spi_transfer		*xfer;

	/* Validation */
	if (unlikely(list_empty(&msg->transfers) || !spi->max_speed_hz)) {
		return -EINVAL;
	}

	as = spi_master_get_devdata(spi->master);
	if (as->shutdown)
		return -ESHUTDOWN;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (!xfer->tx_buf && !xfer->rx_buf)
			return -EINVAL;

		if (spi->bits_per_word > 8 && (xfer->len & 0x1))
			return -EINVAL;
	}

	/* Add into message list */
	msg->status = -EINPROGRESS;
	msg->actual_length = 0;
	spin_lock(&as->lock);
	list_add_tail(&msg->queue, &as->queue);

	if (!as->c_xfer) {
		as->c_xfer = (struct spi_transfer *)4;
		spin_unlock(&as->lock);
		ambarella_spi_do_message(spi->master);
	} else {
		spin_unlock(&as->lock);
	}

	return 0;
}

static void ambarella_spi_cleanup(struct spi_device *spi)
{
	return;
}

static int ambarella_spi_inithw(struct spi_master *master)
{
	u16 				sckdv;
	u32 				ctrlr0, ssi_freq;
	struct ambarella_spi 	*as = spi_master_get_devdata(master);

	/* Set PLL */
	if (as->pinfo->rct_set_ssi_pll)
		as->pinfo->rct_set_ssi_pll();

	/* Disable SPI */
	ambarella_spi_disable(master);

	/* Initial Register Settings */
	ctrlr0 = ( ( SPI_CFS << 12) | (SPI_WRITE_ONLY << 8) | (SPI_SCPOL << 7) |
		(SPI_SCPH << 6)	| (SPI_FRF << 4) | (SPI_DFS)
	      );
	amba_writel(as->regbase + SPI_CTRLR0_OFFSET, ctrlr0);

	ssi_freq = 54000000;
	if (as->pinfo->get_ssi_freq_hz)
		ssi_freq = as->pinfo->get_ssi_freq_hz();
	sckdv =	(u16)(((ssi_freq / SPI_BAUD_RATE) + 0x01) & 0xfffe);
	amba_writel(as->regbase + SPI_BAUDR_OFFSET, sckdv);

	amba_writel(as->regbase + SPI_TXFTLR_OFFSET, 0);
	amba_writel(as->regbase + SPI_RXFTLR_OFFSET, SPI_FIFO_LENGTH);

	return 0;
}

static irqreturn_t ambarella_spi_isr(int irq, void *dev_id)
{
	struct spi_master		*master = dev_id;
	struct ambarella_spi		*as = spi_master_get_devdata(master);

	if (amba_readl(as->regbase + SPI_ISR_OFFSET) == 0)
		return IRQ_HANDLED;

	disable_irq(as->irq);
	amba_writel(as->regbase + SPI_ISR_OFFSET, 0);
	ambarella_spi_tasklet((unsigned long)master);

	return IRQ_HANDLED;
}

static int __devinit ambarella_spi_probe(struct platform_device *pdev)
{
	struct ambarella_spi			*as;
	struct ambarella_spi_private		*ps;
	struct spi_master			*master;
	struct spi_device 			*spidev;
	struct resource 			*res;
	struct ambarella_spi_platform_info	*pinfo;
	int					i, irq, errorCode;

	/* Get IRQ NO. */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		errorCode = -EINVAL;
		goto ambarella_spi_probe_exit3;
	}

	/* Get Platform Info */
	pinfo = (struct ambarella_spi_platform_info *)pdev->dev.platform_data;
	if (!pinfo) {
		errorCode = -EINVAL;
		goto ambarella_spi_probe_exit3;
	}
	if (pinfo->cs_num && !pinfo->cs_pins) {
		errorCode = -EINVAL;
		goto ambarella_spi_probe_exit3;
	}
	if (!pinfo->cs_activate || !pinfo->cs_deactivate) {
		errorCode = -EINVAL;
		goto ambarella_spi_probe_exit3;
	}
	if (!pinfo->rct_set_ssi_pll || !pinfo->get_ssi_freq_hz) {
		errorCode = -EINVAL;
		goto ambarella_spi_probe_exit3;
	}

	/* Get Base Address */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		errorCode = -EINVAL;
		goto ambarella_spi_probe_exit3;
	}

	/* Alocate Master */
	master = spi_alloc_master(&pdev->dev, sizeof *as);
	if (!master) {
		errorCode = -ENOMEM;
		goto ambarella_spi_probe_exit3;
	}

	/* Initalize Device Data */
	master->bus_num = pdev->id;
	master->num_chipselect = pinfo->cs_num;
	master->setup = ambarella_spi_setup;
	master->transfer = ambarella_spi_transfer;
	master->cleanup = ambarella_spi_cleanup;
	platform_set_drvdata(pdev, master);
	as = spi_master_get_devdata(master);
	as->regbase = (u32)res->start;
	as->irq = irq;
	as->pinfo = pinfo;
	tasklet_init(&as->tasklet, ambarella_spi_tasklet, (unsigned long)master);
	INIT_LIST_HEAD(&as->queue);
	as->c_xfer = NULL;
	as->shutdown = 0;
	spin_lock_init(&as->lock);
	as->bpw = 16;

	/* Request IRQ */
	errorCode = request_irq(irq, ambarella_spi_isr, IRQF_TRIGGER_HIGH,
			dev_name(&pdev->dev), master);
	if (errorCode)
		goto ambarella_spi_probe_exit2;
	else
		dev_info(&pdev->dev, "ambarella SPI Controller %d created \n", pdev->id);

	/* Register Master */
	errorCode = spi_register_master(master);
	if (errorCode)
		goto ambarella_spi_probe_exit1;

	/* Allocate Private Devices */
	ps = (struct ambarella_spi_private *)kmalloc(master->num_chipselect * \
		sizeof(struct ambarella_spi_private), GFP_KERNEL);
	if (!ps) {
		errorCode = -ENOMEM;
		goto ambarella_spi_probe_exit3;
	}
	spidev = (struct spi_device *)kmalloc(master->num_chipselect * \
		sizeof(struct spi_device), GFP_KERNEL);
	if (!spidev) {
		errorCode = -ENOMEM;
		kfree(ps);
		goto ambarella_spi_probe_exit3;
	}

	for (i = 0; i < master->num_chipselect; i++) {
		ps[i].spi = spidev + i;
		ps[i].spi->master = master;
		mutex_init(&ps[i].mtx);
		spin_lock_init(&ps[i].lock);
	}
	ambarella_spi_private_devices[master->bus_num].cs_num = master->num_chipselect;
	ambarella_spi_private_devices[master->bus_num].data = ps;

	/* Inittialize Hardware*/
	ambarella_spi_inithw(master);
	goto ambarella_spi_probe_exit3;

ambarella_spi_probe_exit1:
	free_irq(irq, master);

ambarella_spi_probe_exit2:
	tasklet_kill(&as->tasklet);
	spi_master_put(master);

ambarella_spi_probe_exit3:
	return errorCode;
}

static int __devexit ambarella_spi_remove(struct platform_device *pdev)
{

	struct spi_master		*master = platform_get_drvdata(pdev);
	struct ambarella_spi		*as = spi_master_get_devdata(master);
	struct spi_message		*msg;

	as->shutdown = 1;
	ambarella_spi_disable(master);

	list_for_each_entry(msg, &as->queue, queue) {
		msg->status = -ESHUTDOWN;
		msg->complete(msg->context);
	}

	tasklet_kill(&as->tasklet);
	free_irq(as->irq, master);
	spi_unregister_master(master);

	return 0;
}

#ifdef CONFIG_PM
static int ambarella_spi_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int				errorCode = 0;
	struct spi_master		*master;
	struct ambarella_spi		*pinfo;

	master = platform_get_drvdata(pdev);
	pinfo = spi_master_get_devdata(master);

	if (pinfo) {
		if (!device_may_wakeup(&pdev->dev))
			disable_irq(pinfo->irq);
	} else {
		dev_err(&pdev->dev, "Cannot find valid pinfo\n");
		errorCode = -ENXIO;
	}

	dev_dbg(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);

	return errorCode;
}

static int ambarella_spi_resume(struct platform_device *pdev)
{
	int				errorCode = 0;
	struct spi_master		*master;
	struct ambarella_spi		*pinfo;

	master = platform_get_drvdata(pdev);
	pinfo = spi_master_get_devdata(master);

	if (pinfo) {
		if (!device_may_wakeup(&pdev->dev))
			enable_irq(pinfo->irq);
	} else {
		dev_err(&pdev->dev, "Cannot find valid pinfo\n");
		errorCode = -ENXIO;
	}

	dev_dbg(&pdev->dev, "%s exit with %d\n", __func__, errorCode);

	return errorCode;
}
#endif

static struct platform_driver ambarella_spi_driver = {
	.probe		= ambarella_spi_probe,
	.remove		= __devexit_p(ambarella_spi_remove),
#ifdef CONFIG_PM
	.suspend	= ambarella_spi_suspend,
	.resume		= ambarella_spi_resume,
#endif
	.driver		= {
		.name	= "ambarella-spi",
		.owner	= THIS_MODULE,
	},
};

static int __init ambarella_spi_init(void)
{
	return platform_driver_register(&ambarella_spi_driver);
}

static void __exit ambarella_spi_exit(void)
{
	platform_driver_unregister(&ambarella_spi_driver);
}

module_init(ambarella_spi_init);
module_exit(ambarella_spi_exit);

MODULE_DESCRIPTION("Ambarella Media Processor SPI Bus Controller");
MODULE_AUTHOR("Louis Sun, <louis.sun@ambarella.com>");
MODULE_LICENSE("GPL");


/*=================Utilities for Non-GPL Use==================================*/
static void ambarella_spi_complete(void *arg)
{
	complete(arg);
}

int ambarella_spi_write(amba_spi_cfg_t *spi_cfg, amba_spi_write_t *spi_write)
{
	u8				bus_id, cs_id, cs_num;
	int				errorCode;
	struct ambarella_spi_private	*ps;
	struct spi_device		*spi;
	struct spi_message		msg;
	struct spi_transfer		xfer;

	DECLARE_COMPLETION_ONSTACK(done);

	/* Validate Input Args */
	if (!spi_cfg || !spi_write)
		return -EINVAL;

	bus_id = spi_write->bus_id;
	cs_id = spi_write->cs_id;
	cs_num = ambarella_spi_private_devices[bus_id].cs_num;
	ps = ambarella_spi_private_devices[bus_id].data;

	if (bus_id >= SPI_INSTANCES	|| cs_id >= cs_num
		|| !spi_write->buffer	|| !spi_write->n_size)
		return -EINVAL;

	/* Transfer */
	memset(&xfer, 0, sizeof(struct spi_transfer));
	xfer.tx_buf = spi_write->buffer;
	xfer.len = spi_write->n_size;
	xfer.cs_change = spi_cfg->cs_change;

	/* Message */
	memset(&msg, 0, sizeof(struct spi_message));
	INIT_LIST_HEAD(&msg.transfers);
	list_add_tail(&xfer.transfer_list, &msg.transfers);
	msg.complete = ambarella_spi_complete;
	msg.context = &done;
	spi = ps[cs_id].spi;
	msg.spi = spi;

	mutex_lock(&ps[cs_id].mtx);

	/* Config */
	spi->chip_select = cs_id;
	spi->mode = spi_cfg->spi_mode;
	if (spi_cfg->lsb_first)
		spi->mode |= SPI_LSB_FIRST;
	else
		spi->mode &= ~SPI_LSB_FIRST;
	spi->mode &= ~SPI_LOOP;
	spi->bits_per_word = spi_cfg->cfs_dfs;
	spi->max_speed_hz = spi_cfg->baud_rate;

	/* Wait */
	spin_lock_irq(&ps[cs_id].lock);
	errorCode = spi->master->transfer(spi, &msg);
	spin_unlock_irq(&ps[cs_id].lock);
	if (!errorCode)
		wait_for_completion(&done);

	mutex_unlock(&ps[cs_id].mtx);

	return errorCode;
}
EXPORT_SYMBOL(ambarella_spi_write);

int ambarella_spi_read(amba_spi_cfg_t *spi_cfg, amba_spi_read_t *spi_read)
{
	u8				bus_id, cs_id, cs_num;
	int				errorCode;
	struct ambarella_spi_private	*ps;
	struct spi_device		*spi;
	struct spi_message		msg;
	struct spi_transfer		xfer;

	DECLARE_COMPLETION_ONSTACK(done);

	/* Validate Input Args */
	if (!spi_cfg || !spi_read)
		return -EINVAL;

	bus_id = spi_read->bus_id;
	cs_id = spi_read->cs_id;
	cs_num = ambarella_spi_private_devices[bus_id].cs_num;
	ps = ambarella_spi_private_devices[bus_id].data;

	if (bus_id >= SPI_INSTANCES	|| cs_id >= cs_num
		|| !spi_read->buffer	|| !spi_read->n_size)
		return -EINVAL;

	/* Transfer */
	memset(&xfer, 0, sizeof(struct spi_transfer));
	xfer.rx_buf = spi_read->buffer;
	xfer.len = spi_read->n_size;
	xfer.cs_change = spi_cfg->cs_change;

	/* Message */
	memset(&msg, 0, sizeof(struct spi_message));
	INIT_LIST_HEAD(&msg.transfers);
	list_add_tail(&xfer.transfer_list, &msg.transfers);
	msg.complete = ambarella_spi_complete;
	msg.context = &done;
	spi = ps[cs_id].spi;
	msg.spi = spi;

	mutex_lock(&ps[cs_id].mtx);

	/* Config */
	spi->chip_select = cs_id;
	spi->mode = spi_cfg->spi_mode;
	if (spi_cfg->lsb_first)
		spi->mode |= SPI_LSB_FIRST;
	else
		spi->mode &= ~SPI_LSB_FIRST;
	spi->mode &= ~SPI_LOOP;
	spi->bits_per_word = spi_cfg->cfs_dfs;
	spi->max_speed_hz = spi_cfg->baud_rate;

	/* Wait */
	spin_lock_irq(&ps[cs_id].lock);
	errorCode = spi->master->transfer(spi, &msg);
	spin_unlock_irq(&ps[cs_id].lock);
	if (!errorCode)
		wait_for_completion(&done);

	mutex_unlock(&ps[cs_id].mtx);

	return errorCode;
}
EXPORT_SYMBOL(ambarella_spi_read);

int ambarella_spi_write_then_read(amba_spi_cfg_t *spi_cfg,
	amba_spi_write_then_read_t *spi_write_then_read)
{
	u8				bus_id, cs_id, cs_num, *buf;
	u16				size;
	int				errorCode;
	struct ambarella_spi_private	*ps;
	struct spi_device		*spi;
	struct spi_message		msg;
	struct spi_transfer		xfer;

	DECLARE_COMPLETION_ONSTACK(done);

	/* Validate Input Args */
	if (!spi_cfg || !spi_write_then_read)
		return -EINVAL;

	bus_id = spi_write_then_read->bus_id;
	cs_id = spi_write_then_read->cs_id;
	cs_num = ambarella_spi_private_devices[bus_id].cs_num;
	ps = ambarella_spi_private_devices[bus_id].data;

	if (bus_id >= SPI_INSTANCES		  || cs_id >= cs_num
		|| !spi_write_then_read->w_buffer || !spi_write_then_read->w_size
		|| !spi_write_then_read->r_buffer || !spi_write_then_read->r_size)
		return -EINVAL;

	/* Prepare Buffer */
	size = spi_write_then_read->w_size + spi_write_then_read->r_size;
	buf = (u8 *)kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, spi_write_then_read->w_buffer, spi_write_then_read->w_size);
	memset(buf + spi_write_then_read->w_size, SPI_DUMMY_DATA,
		spi_write_then_read->r_size);

	/* Transfer */
	memset(&xfer, 0, sizeof(struct spi_transfer));
	xfer.tx_buf = buf;
	xfer.rx_buf = buf;
	xfer.len = size;
	xfer.cs_change = spi_cfg->cs_change;

	/* Message */
	memset(&msg, 0, sizeof(struct spi_message));
	INIT_LIST_HEAD(&msg.transfers);
	list_add_tail(&xfer.transfer_list, &msg.transfers);
	msg.complete = ambarella_spi_complete;
	msg.context = &done;
	spi = ps[cs_id].spi;
	msg.spi = spi;

	mutex_lock(&ps[cs_id].mtx);

	/* Config */
	spi->chip_select = cs_id;
	spi->mode = spi_cfg->spi_mode;
	if (spi_cfg->lsb_first)
		spi->mode |= SPI_LSB_FIRST;
	else
		spi->mode &= ~SPI_LSB_FIRST;
	spi->mode &= ~SPI_LOOP;
	spi->bits_per_word = spi_cfg->cfs_dfs;
	spi->max_speed_hz = spi_cfg->baud_rate;

	/* Wait */
	spin_lock_irq(&ps[cs_id].lock);
	errorCode = spi->master->transfer(spi, &msg);
	spin_unlock_irq(&ps[cs_id].lock);
	if (!errorCode)
		wait_for_completion(&done);

	mutex_unlock(&ps[cs_id].mtx);

	/* Free Buffer */
	memcpy(spi_write_then_read->r_buffer, buf + spi_write_then_read->w_size,
		spi_write_then_read->r_size);
	kfree(buf);

	return errorCode;
}
EXPORT_SYMBOL(ambarella_spi_write_then_read);

int ambarella_spi_write_and_read(amba_spi_cfg_t *spi_cfg,
	amba_spi_write_and_read_t *spi_write_and_read)
{
	u8				bus_id, cs_id, cs_num;
	int				errorCode;
	struct ambarella_spi_private	*ps;
	struct spi_device		*spi;
	struct spi_message		msg;
	struct spi_transfer		xfer;

	DECLARE_COMPLETION_ONSTACK(done);

	/* Validate Input Args */
	if (!spi_cfg || !spi_write_and_read)
		return -EINVAL;

	bus_id = spi_write_and_read->bus_id;
	cs_id = spi_write_and_read->cs_id;
	cs_num = ambarella_spi_private_devices[bus_id].cs_num;
	ps = ambarella_spi_private_devices[bus_id].data;

	if (bus_id >= SPI_INSTANCES		|| cs_id >= cs_num
		|| !spi_write_and_read->w_buffer|| !spi_write_and_read->r_buffer
		|| !spi_write_and_read->n_size)
		return -EINVAL;

	/* Transfer */
	memset(&xfer, 0, sizeof(struct spi_transfer));
	xfer.tx_buf = spi_write_and_read->w_buffer;
	xfer.rx_buf = spi_write_and_read->r_buffer;
	xfer.len = spi_write_and_read->n_size;
	xfer.cs_change = spi_cfg->cs_change;

	/* Message */
	memset(&msg, 0, sizeof(struct spi_message));
	INIT_LIST_HEAD(&msg.transfers);
	list_add_tail(&xfer.transfer_list, &msg.transfers);
	msg.complete = ambarella_spi_complete;
	msg.context = &done;
	spi = ps[cs_id].spi;
	msg.spi = spi;

	mutex_lock(&ps[cs_id].mtx);

	/* Config */
	spi->chip_select = cs_id;
	spi->mode = spi_cfg->spi_mode;
	if (spi_cfg->lsb_first)
		spi->mode |= SPI_LSB_FIRST;
	else
		spi->mode &= ~SPI_LSB_FIRST;
	spi->mode &= ~SPI_LOOP;
	spi->bits_per_word = spi_cfg->cfs_dfs;
	spi->max_speed_hz = spi_cfg->baud_rate;

	/* Wait */
	spin_lock_irq(&ps[cs_id].lock);
	errorCode = spi->master->transfer(spi, &msg);
	spin_unlock_irq(&ps[cs_id].lock);
	if (!errorCode)
		wait_for_completion(&done);

	mutex_unlock(&ps[cs_id].mtx);

	return errorCode;
}
EXPORT_SYMBOL(ambarella_spi_write_and_read);

