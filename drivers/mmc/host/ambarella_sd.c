/*
 * drivers/mmc/host/ambarella_sd.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
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

#include <linux/blkdev.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/scatterlist.h>

#include <linux/cpufreq.h>

#include <mach/hardware.h>

#define CONFIG_SD_AMBARELLA_TIMEOUT_VAL		(0xe)
#define CONFIG_SD_AMBARELLA_WAIT_TIMEOUT	(HZ * 1)
#define CONFIG_SD_AMBARELLA_WAIT_COUNTER_LIMIT	(100000)
#define CONFIG_SD_AMBARELLA_SLEEP_COUNTER_LIMIT	(1000)

#define amb_sd_writeb(date, address)		__raw_writeb((date), (address))
#define amb_sd_writew(date, address)		__raw_writew((date), (address))
#define amb_sd_writel(date, address)		__raw_writel((date), (address))

#define amb_sd_readb(address)			__raw_readb(address)
#define amb_sd_readw(address)			__raw_readw(address)
#define amb_sd_readl(address)			__raw_readl(address)

#define CONFIG_SD_AMBARELLA_MAX_DMA_SIZE	(512 * 1024)
#define CONFIG_SD_AMBARELLA_MAX_DMA_FLAG	(SD_BLK_SZ_512KB)

enum ambarella_sd_state {
	AMBA_SD_STATE_IDLE,
	AMBA_SD_STATE_CMD,
	AMBA_SD_STATE_DATA,
	AMBA_SD_STATE_ERR
};

struct ambarella_sd_mmc_info {
	struct mmc_host			*mmc;
	struct mmc_request		*mrq;

	wait_queue_head_t		wait;

	enum ambarella_sd_state		state;

	struct scatterlist		*sg;
	u32				sg_len;
	u32				sg_index;
	u8				tmo;
	u32				blk_sz;
	u16				blk_cnt;
	u32				arg_reg;
	u16				xfr_reg;
	u16				cmd_reg;

	char				*buf_vaddress;
	dma_addr_t			buf_paddress;
	u32				dma_address;
	u32				dma_left;
	u32				dma_need_fill;
	u32				dma_size;
	u32				dma_per_size;

	u32				dma_w_fill_counter;
	u32				dma_w_counter;
	u32				dma_r_fill_counter;
	u32				dma_r_counter;

	void				(*pre_dma)(void *data);
	void				(*post_dma)(void *data);

	struct ambarella_sd_slot	slot_info;
	u32				slot_id;
	void				*pinfo;
	u32				valid;
};

struct ambarella_sd_controller_info {
	unsigned char __iomem 		*regbase;
	struct device			*dev;
	struct resource			*mem;
	unsigned int			irq;
	spinlock_t			lock;
	u16				nisen;

	struct ambarella_sd_controller	controller_info;
	struct ambarella_sd_mmc_info	*pslotinfo[SD_MAX_SLOT_NUM];
	struct mmc_ios			controller_ios;
#ifdef CONFIG_CPU_FREQ
	struct notifier_block		sd_freq_transition;
#endif
};

static void ambarella_sd_show_info(struct ambarella_sd_mmc_info *pslotinfo)
{
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	dev_dbg(pinfo->dev, "ambarella_sd_show_info enter slot%d\n",
		pslotinfo->slot_id);
	dev_dbg(pinfo->dev, "sg = 0x%x.\n", (u32)pslotinfo->sg);
	dev_dbg(pinfo->dev, "sg_len = 0x%x.\n", pslotinfo->sg_len);
	dev_dbg(pinfo->dev, "sg_index = 0x%x.\n", pslotinfo->sg_index);
	dev_dbg(pinfo->dev, "tmo = 0x%x.\n", pslotinfo->tmo);
	dev_dbg(pinfo->dev, "blk_sz = 0x%x.\n", pslotinfo->blk_sz);
	dev_dbg(pinfo->dev, "blk_cnt = 0x%x.\n", pslotinfo->blk_cnt);
	dev_dbg(pinfo->dev, "arg_reg = 0x%x.\n", pslotinfo->arg_reg);
	dev_dbg(pinfo->dev, "xfr_reg = 0x%x.\n", pslotinfo->xfr_reg);
	dev_dbg(pinfo->dev, "cmd_reg = 0x%x.\n", pslotinfo->cmd_reg);
	dev_dbg(pinfo->dev, "buf_vaddress = 0x%x.\n",
		(u32)pslotinfo->buf_vaddress);
	dev_dbg(pinfo->dev, "buf_paddress = 0x%x.\n",
		pslotinfo->buf_paddress);
	dev_dbg(pinfo->dev, "dma_address = 0x%x.\n",
		pslotinfo->dma_address);
	dev_dbg(pinfo->dev, "dma_left = 0x%x.\n", pslotinfo->dma_left);
	dev_dbg(pinfo->dev, "dma_need_fill = 0x%x.\n",
		pslotinfo->dma_need_fill);
	dev_dbg(pinfo->dev, "dma_size = 0x%x.\n",
		pslotinfo->dma_size);
	dev_dbg(pinfo->dev, "dma_per_size = 0x%x.\n",
		pslotinfo->dma_per_size);
	dev_dbg(pinfo->dev, "dma_w_fill_counter = 0x%x.\n",
		pslotinfo->dma_w_fill_counter);
	dev_dbg(pinfo->dev, "dma_w_counter = 0x%x.\n",
		pslotinfo->dma_w_counter);
	dev_dbg(pinfo->dev, "dma_r_fill_counter = 0x%x.\n",
		pslotinfo->dma_r_fill_counter);
	dev_dbg(pinfo->dev, "dma_r_counter = 0x%x.\n",
		pslotinfo->dma_r_counter);
	dev_dbg(pinfo->dev, "pre_dma = 0x%x.\n",
		(u32)pslotinfo->pre_dma);
	dev_dbg(pinfo->dev, "post_dma = 0x%x.\n",
		(u32)pslotinfo->post_dma);
	dev_dbg(pinfo->dev, "SD: state = 0x%x.\n",
		pslotinfo->state);
	dev_dbg(pinfo->dev, "ambarella_sd_show_info exit slot%d\n",
		pslotinfo->slot_id);
}

static u32 ambarella_sd_check_dma_boundary(
	struct ambarella_sd_mmc_info *pslotinfo,
	u32 address, u32 size, u32 max_size)
{
	u32					start_512kb;
	u32					end_512kb;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	start_512kb = (address) & (~(max_size - 1));
	end_512kb = (address + size - 1) & (~(max_size - 1));

	if (start_512kb != end_512kb) {
		dev_dbg(pinfo->dev, "Request crosses 512KB DMA boundary!\n");
		return 0;
	}

	return 1;
}

static u32 ambarella_sd_get_dma_size(u32 address)
{
	u32					dma_size = 0x80000;

	if ((address & 0x7FFFF) == 0) {
		dma_size = 0x80000;
	} else if ((address & 0x3FFFF) == 0) {
		dma_size = 0x40000;
	} else if ((address & 0x1FFFF) == 0) {
		dma_size = 0x20000;
	} else if ((address & 0xFFFF) == 0) {
		dma_size = 0x10000;
	} else if ((address & 0x7FFF) == 0) {
		dma_size = 0x8000;
	} else if ((address & 0x3FFF) == 0) {
		dma_size = 0x4000;
	} else if ((address & 0x1FFF) == 0) {
		dma_size = 0x2000;
	} else {
		dma_size = 0x1000;
	}

	return dma_size;
}

static u32 ambarella_sd_get_dma_size_mask(u32 size)
{
	u32					mask = SD_BLK_SZ_512KB;

	switch (size) {
	case 0x80000:
		mask = SD_BLK_SZ_512KB;
		break;
	case 0x40000:
		mask = SD_BLK_SZ_256KB;
		break;
	case 0x20000:
		mask = SD_BLK_SZ_128KB;
		break;
	case 0x10000:
		mask = SD_BLK_SZ_64KB;
		break;
	case 0x8000:
		mask = SD_BLK_SZ_32KB;
		break;
	case 0x4000:
		mask = SD_BLK_SZ_16KB;
		break;
	case 0x2000:
		mask = SD_BLK_SZ_8KB;
		break;
	case 0x1000:
		mask = SD_BLK_SZ_4KB;
		break;
	default:
		BUG_ON(1);
		break;
	}

	return mask;
}

static void ambarella_sd_pre_sg_to_dma(void *data)
{
	struct ambarella_sd_mmc_info		*pslotinfo;
	u32					i;
	struct scatterlist			*current_sg;
	char					*dmabuf;
	char					*sgbuf;
	u32					dma_start = 0xffffffff;
	u32					dma_end = 0;
	struct ambarella_sd_controller_info	*pinfo;

	pslotinfo = (struct ambarella_sd_mmc_info *)data;
	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	current_sg = pslotinfo->sg;
	dmabuf = pslotinfo->buf_vaddress;

	if (pslotinfo->sg_index == 0) {
		pslotinfo->dma_per_size = CONFIG_SD_AMBARELLA_MAX_DMA_SIZE;
		pslotinfo->dma_w_counter++;

		for (i = 0; i < pslotinfo->sg_len; i++) {
			current_sg[i].dma_address = dma_map_page(pinfo->dev,
							sg_page(&current_sg[i]),
							current_sg[i].offset,
							current_sg[i].length,
							DMA_TO_DEVICE);

			if ((current_sg[i].length & 0xFFF) &&
				(i < pslotinfo->sg_len - 1)) {
				dev_dbg(pinfo->dev,
					"Short DMA length[0x%x], %d&%d\n",
					current_sg[i].length,
					i, pslotinfo->sg_len);
				pslotinfo->dma_need_fill = 1;
			}

			dma_start = min(dma_start, current_sg[i].dma_address);
			dma_end = max(dma_end, (current_sg[i].dma_address +
					current_sg[i].length));

			pslotinfo->dma_per_size = min(pslotinfo->dma_per_size,
				ambarella_sd_get_dma_size(
					current_sg[i].length));

			pslotinfo->dma_per_size = min(pslotinfo->dma_per_size,
				ambarella_sd_get_dma_size(
					current_sg[i].dma_address));
		}
		if (ambarella_sd_check_dma_boundary(pslotinfo, dma_start,
			(dma_end - dma_start), pslotinfo->dma_per_size) == 0) {
			pslotinfo->dma_need_fill = 1;
			dev_dbg(pinfo->dev,
				"dma_start = 0x%x, dma_end = 0x%x\n",
				dma_start, dma_end);
		}

		if (pslotinfo->dma_need_fill) {
			if((pslotinfo->buf_vaddress == 0) ||
				(pslotinfo->buf_paddress == 0))
				BUG_ON(1);

			pslotinfo->dma_per_size =
				CONFIG_SD_AMBARELLA_MAX_DMA_SIZE;
			pslotinfo->dma_w_fill_counter++;

			for (i = 0; i < pslotinfo->sg_len; i++) {
				sgbuf = phys_to_virt(current_sg[i].dma_address);
				memcpy(dmabuf, sgbuf, current_sg[i].length);
				dmabuf += current_sg[i].length;
			}
			dma_sync_single_for_cpu(pinfo->dev,
				pslotinfo->buf_paddress, pslotinfo->dma_size,
				DMA_TO_DEVICE);
			pslotinfo->dma_address = pslotinfo->buf_paddress;
			pslotinfo->dma_left = pslotinfo->dma_size;
			pslotinfo->sg_index = pslotinfo->sg_len;
			pslotinfo->blk_sz &= 0xFFF;
			pslotinfo->blk_sz |= CONFIG_SD_AMBARELLA_MAX_DMA_FLAG;
		} else {
			pslotinfo->dma_address =
				current_sg[pslotinfo->sg_index].dma_address;
			pslotinfo->dma_left =
				current_sg[pslotinfo->sg_index].length;
			pslotinfo->sg_index++;
			pslotinfo->blk_sz &= 0xFFF;
			pslotinfo->blk_sz |=
				ambarella_sd_get_dma_size_mask(
					pslotinfo->dma_per_size);
		}
	} else if (pslotinfo->sg_index < pslotinfo->sg_len) {
		if (pslotinfo->dma_left) {
			pslotinfo->dma_address += pslotinfo->dma_per_size;
		} else {
			pslotinfo->dma_address =
				current_sg[pslotinfo->sg_index].dma_address;
			pslotinfo->dma_left =
				current_sg[pslotinfo->sg_index].length;
			pslotinfo->sg_index++;
		}
	} else if (pslotinfo->sg_index == pslotinfo->sg_len) {
		if (pslotinfo->dma_left) {
			pslotinfo->dma_address += pslotinfo->dma_per_size;
		} else {
			pslotinfo->dma_address = 0;
		}
	} else {
		pslotinfo->dma_address = 0;
	}

	if (pslotinfo->dma_left >= pslotinfo->dma_per_size)
		pslotinfo->dma_left -= pslotinfo->dma_per_size;
	else
		pslotinfo->dma_left = 0;

	//ambarella_sd_show_info(pslotinfo);
}

static void ambarella_sd_post_sg_to_dma(void *data)
{
	struct ambarella_sd_mmc_info		*pslotinfo;
	u32					i;
	struct scatterlist			*current_sg;
	struct ambarella_sd_controller_info	*pinfo;

	pslotinfo = (struct ambarella_sd_mmc_info *)data;
	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	current_sg = pslotinfo->sg;

	for (i = 0; i < pslotinfo->sg_len; i++) {
		dma_unmap_page(pinfo->dev, current_sg[i].dma_address,
				current_sg[i].length, DMA_TO_DEVICE);
	}
}

static void ambarella_sd_pre_dma_to_sg(void *data)
{
	struct ambarella_sd_mmc_info		*pslotinfo;
	u32					i;
	struct scatterlist			*current_sg;
	u32					dma_start = 0xffffffff;
	u32					dma_end = 0;
	struct ambarella_sd_controller_info	*pinfo;

	pslotinfo = (struct ambarella_sd_mmc_info *)data;
	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	current_sg = pslotinfo->sg;

	if (pslotinfo->sg_index == 0) {
		pslotinfo->dma_per_size = CONFIG_SD_AMBARELLA_MAX_DMA_SIZE;
		pslotinfo->dma_r_counter++;

		for (i = 0; i < pslotinfo->sg_len; i++) {
			current_sg[i].dma_address = dma_map_page(pinfo->dev,
							sg_page(&current_sg[i]),
							current_sg[i].offset,
							current_sg[i].length,
							DMA_FROM_DEVICE);

			if ((current_sg[i].length & 0xFFF) &&
				(i < pslotinfo->sg_len - 1)) {
				dev_dbg(pinfo->dev,
					"Short DMA length[0x%x], %d&%d\n",
					current_sg[i].length,
					i, pslotinfo->sg_len);
				pslotinfo->dma_need_fill = 1;
			}

			dma_start = min(dma_start, current_sg[i].dma_address);
			dma_end = max(dma_end, (current_sg[i].dma_address +
					current_sg[i].length));

			pslotinfo->dma_per_size = min(pslotinfo->dma_per_size,
				ambarella_sd_get_dma_size(
					current_sg[i].length));

			pslotinfo->dma_per_size = min(pslotinfo->dma_per_size,
				ambarella_sd_get_dma_size(
					current_sg[i].dma_address));
		}

		if (ambarella_sd_check_dma_boundary(pslotinfo, dma_start,
			(dma_end - dma_start), pslotinfo->dma_per_size) == 0) {
			pslotinfo->dma_need_fill = 1;
			dev_dbg(pinfo->dev,
				"dma_start = 0x%x, dma_end = 0x%x\n",
				dma_start, dma_end);
		}

		if (pslotinfo->dma_need_fill) {
			if((pslotinfo->buf_vaddress == 0) ||
				(pslotinfo->buf_paddress == 0))
				BUG_ON(1);

			pslotinfo->dma_per_size =
				CONFIG_SD_AMBARELLA_MAX_DMA_SIZE;
			pslotinfo->dma_r_fill_counter++;

			pslotinfo->dma_address = pslotinfo->buf_paddress;
			pslotinfo->dma_left = pslotinfo->dma_size;
			pslotinfo->sg_index = pslotinfo->sg_len;
			pslotinfo->blk_sz &= 0xFFF;
			pslotinfo->blk_sz |= CONFIG_SD_AMBARELLA_MAX_DMA_FLAG;
		} else {
			pslotinfo->dma_address =
				current_sg[pslotinfo->sg_index].dma_address;
			pslotinfo->dma_left =
				current_sg[pslotinfo->sg_index].length;
			pslotinfo->sg_index++;
			pslotinfo->blk_sz &= 0xFFF;
			pslotinfo->blk_sz |=
				ambarella_sd_get_dma_size_mask(
					pslotinfo->dma_per_size);
		}
	} else if (pslotinfo->sg_index < pslotinfo->sg_len) {
		if (pslotinfo->dma_left) {
			pslotinfo->dma_address += pslotinfo->dma_per_size;
		} else {
			pslotinfo->dma_address =
				current_sg[pslotinfo->sg_index].dma_address;
			pslotinfo->dma_left =
				current_sg[pslotinfo->sg_index].length;
			pslotinfo->sg_index++;
		}
	} else if (pslotinfo->sg_index == pslotinfo->sg_len) {
		if (pslotinfo->dma_left) {
			pslotinfo->dma_address += pslotinfo->dma_per_size;
		} else {
			pslotinfo->dma_address = 0;
		}
	} else {
		pslotinfo->dma_address = 0;
	}

	if (pslotinfo->dma_left >= pslotinfo->dma_per_size)
		pslotinfo->dma_left -= pslotinfo->dma_per_size;
	else
		pslotinfo->dma_left = 0;

	//ambarella_sd_show_info(pslotinfo);
}

static void ambarella_sd_post_dma_to_sg(void *data)
{
	struct ambarella_sd_mmc_info		*pslotinfo;
	u32					i;
	struct scatterlist			*current_sg;
	char					*dmabuf;
	char					*sgbuf;
	struct ambarella_sd_controller_info	*pinfo;

	pslotinfo = (struct ambarella_sd_mmc_info *)data;
	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	current_sg = pslotinfo->sg;
	dmabuf = pslotinfo->buf_vaddress;

	if (pslotinfo->dma_need_fill)
		dma_sync_single_for_cpu(pinfo->dev,
			pslotinfo->buf_paddress, pslotinfo->dma_size,
			DMA_FROM_DEVICE);

	for (i = 0; i < pslotinfo->sg_len; i++) {
		dma_unmap_page(pinfo->dev, current_sg[i].dma_address,
				current_sg[i].length, DMA_FROM_DEVICE);
		if (pslotinfo->dma_need_fill) {
			sgbuf = phys_to_virt(current_sg[i].dma_address);
			memcpy(sgbuf, dmabuf, current_sg[i].length);
			dmabuf += current_sg[i].length;
		}
	}
}

static void ambarella_sd_request_bus(struct mmc_host *mmc)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);

	if (pslotinfo->slot_info.request)
		pslotinfo->slot_info.request();
}

static void ambarella_sd_release_bus(struct mmc_host *mmc)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);

	if (pslotinfo->slot_info.release)
		pslotinfo->slot_info.release();
}

static void ambarella_sd_enable_normal_int(struct mmc_host *mmc, u16 ints)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	struct ambarella_sd_controller_info	*pinfo;
	unsigned long				flags;
	u16					int_flag;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	spin_lock_irqsave(&pinfo->lock, flags);

	int_flag = amb_sd_readw(pinfo->regbase + SD_NISEN_OFFSET);
	int_flag |= ints;
	amb_sd_writew(int_flag, pinfo->regbase + SD_NISEN_OFFSET);
	pinfo->nisen = int_flag;

	int_flag = amb_sd_readw(pinfo->regbase + SD_NIXEN_OFFSET);
	int_flag |= ints;
	amb_sd_writew(int_flag, pinfo->regbase + SD_NIXEN_OFFSET);

	spin_unlock_irqrestore(&pinfo->lock, flags);
}

static void ambarella_sd_disable_normal_int(struct mmc_host *mmc, u16 ints)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	struct ambarella_sd_controller_info	*pinfo;
	unsigned long				flags;
	u16					int_flag;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	spin_lock_irqsave(&pinfo->lock, flags);

	int_flag = amb_sd_readw(pinfo->regbase + SD_NISEN_OFFSET);
	int_flag &= ~ints;
	amb_sd_writew(int_flag, pinfo->regbase + SD_NISEN_OFFSET);
	pinfo->nisen = int_flag;

	int_flag = amb_sd_readw(pinfo->regbase + SD_NIXEN_OFFSET);
	int_flag &= ~ints;
	amb_sd_writew(int_flag, pinfo->regbase + SD_NIXEN_OFFSET);

	spin_unlock_irqrestore(&pinfo->lock, flags);
}

static void ambarella_sd_enable_error_int(struct mmc_host *mmc, u16 ints)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	struct ambarella_sd_controller_info	*pinfo;
	unsigned long				flags;
	u16					int_flag;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	spin_lock_irqsave(&pinfo->lock, flags);

	int_flag = amb_sd_readw(pinfo->regbase + SD_EISEN_OFFSET);
	int_flag |= ints;
	amb_sd_writew(int_flag, pinfo->regbase + SD_EISEN_OFFSET);

	int_flag = amb_sd_readw(pinfo->regbase + SD_EIXEN_OFFSET);
	int_flag |= ints;
	amb_sd_writew(int_flag, pinfo->regbase + SD_EIXEN_OFFSET);

	spin_unlock_irqrestore(&pinfo->lock, flags);
}

static void ambarella_sd_disable_error_int(struct mmc_host *mmc, u16 ints)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	struct ambarella_sd_controller_info	*pinfo;
	unsigned long				flags;
	u16					int_flag;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	spin_lock_irqsave(&pinfo->lock, flags);

	int_flag = amb_sd_readw(pinfo->regbase + SD_EISEN_OFFSET);
	int_flag &= ~ints;
	amb_sd_writew(int_flag, pinfo->regbase + SD_EISEN_OFFSET);

	int_flag = amb_sd_readw(pinfo->regbase + SD_EIXEN_OFFSET);
	int_flag &= ~ints;
	amb_sd_writew(int_flag, pinfo->regbase + SD_EIXEN_OFFSET);

	spin_unlock_irqrestore(&pinfo->lock, flags);
}

static void ambarella_sd_reset_all(struct mmc_host *mmc)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	struct ambarella_sd_controller_info	*pinfo;
	u16					int_flag = 0;
	u32					counter = 0;
	u8					reset_reg;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	dev_dbg(pinfo->dev, "Slot%d ambarella_sd_reset_all: enter %d\n",
		pslotinfo->slot_id, pslotinfo->state);

	ambarella_sd_disable_normal_int(mmc, 0xFFFF);
	ambarella_sd_disable_error_int(mmc, 0xFFFF);
	amb_sd_writew(0xFFFF, pinfo->regbase + SD_NIS_OFFSET);
	amb_sd_writew(0xFFFF, pinfo->regbase + SD_EIS_OFFSET);
	amb_sd_writeb(SD_RESET_ALL, pinfo->regbase + SD_RESET_OFFSET);
	while (1) {
		reset_reg = amb_sd_readb(pinfo->regbase + SD_RESET_OFFSET);
		if (!(reset_reg & SD_RESET_ALL))
			break;
		counter++;
		if (counter > CONFIG_SD_AMBARELLA_SLEEP_COUNTER_LIMIT) {
			dev_warn(pinfo->dev,
				"Wait SD_RESET_ALL %d.\n",
				counter);
			amb_sd_writeb(SD_RESET_ALL,
				pinfo->regbase + SD_RESET_OFFSET);
			break;
		}
		msleep(1);
	}

	pslotinfo->state = AMBA_SD_STATE_IDLE;

	amb_sd_writeb(CONFIG_SD_AMBARELLA_TIMEOUT_VAL,
		pinfo->regbase + SD_TMO_OFFSET);

	//int_flag = SD_NISEN_CARD;
	if (!ambarella_is_valid_gpio_irq(&pslotinfo->slot_info.gpio_cd)) {
		int_flag |= SD_NISEN_REMOVAL	|
			SD_NISEN_INSERT;
	}
	int_flag |= SD_NISEN_DMA	|
		SD_NISEN_BLOCK_GAP	|
		SD_NISEN_XFR_DONE	|
		SD_NISEN_CMD_DONE;
	ambarella_sd_enable_normal_int(mmc, int_flag);

	int_flag = SD_EISEN_ACMD12_ERR	|
		SD_EISEN_CURRENT_ERR	|
		SD_EISEN_DATA_BIT_ERR	|
		SD_EISEN_DATA_CRC_ERR	|
		SD_EISEN_DATA_TMOUT_ERR	|
		SD_EISEN_CMD_IDX_ERR	|
		SD_EISEN_CMD_BIT_ERR	|
		SD_EISEN_CMD_CRC_ERR	|
		SD_EISEN_CMD_TMOUT_ERR;
	ambarella_sd_enable_error_int(mmc, int_flag);

	dev_dbg(pinfo->dev, "ambarella_sd_reset_all: exit%d\n", counter);
}

static void ambarella_sd_reset_cmd_line(struct mmc_host *mmc)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	struct ambarella_sd_controller_info	*pinfo;
	u32					counter = 0;
	u8					reset_reg;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	dev_dbg(pinfo->dev, "Slot%d ambarella_sd_reset_cmd_line: enter\n",
		pslotinfo->slot_id);

	amb_sd_writeb(SD_RESET_CMD, pinfo->regbase + SD_RESET_OFFSET);
	while (1) {
		reset_reg = amb_sd_readb(pinfo->regbase + SD_RESET_OFFSET);
		if (!(reset_reg & SD_RESET_CMD))
			break;
		counter++;
		if (counter > CONFIG_SD_AMBARELLA_WAIT_COUNTER_LIMIT) {
			dev_warn(pinfo->dev,
				"Wait SD_RESET_CMD %d.\n",
				counter);
			amb_sd_writeb(SD_RESET_CMD,
				pinfo->regbase + SD_RESET_OFFSET);
			break;
		}
	}

	dev_dbg(pinfo->dev, "ambarella_sd_reset_cmd_line: exit\n");
}

static void ambarella_sd_reset_data_line(struct mmc_host *mmc)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	struct ambarella_sd_controller_info	*pinfo;
	u32					counter = 0;
	u8					reset_reg;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	dev_dbg(pinfo->dev, "Slot%d ambarella_sd_reset_data_line: enter\n",
		pslotinfo->slot_id);

	amb_sd_writeb(SD_RESET_DAT, pinfo->regbase + SD_RESET_OFFSET);
	while (1) {
		reset_reg = amb_sd_readb(pinfo->regbase + SD_RESET_OFFSET);
		if (!(reset_reg & SD_RESET_DAT))
			break;
		counter++;
		if (counter > CONFIG_SD_AMBARELLA_WAIT_COUNTER_LIMIT) {
			dev_warn(pinfo->dev,
				"Wait SD_RESET_DAT %d.\n",
				counter);
			amb_sd_writeb(SD_RESET_DAT,
				pinfo->regbase + SD_RESET_OFFSET);
			break;
		}
	}

	dev_dbg(pinfo->dev, "ambarella_sd_reset_data_line: exit\n");
}

static void ambarella_sd_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	if (enable)
		ambarella_sd_enable_normal_int(mmc, SD_NISEN_CARD);
	else
		ambarella_sd_disable_normal_int(mmc, SD_NISEN_CARD);
}

static void ambarella_sd_data_done(
	struct ambarella_sd_mmc_info *pslotinfo,
	u16 nis, 
	u16 eis)
{
	struct mmc_data				*data;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	if (pslotinfo->state == AMBA_SD_STATE_DATA) {
		if (pslotinfo->mrq)
			data = pslotinfo->mrq->data;
		else {
			dev_err(pinfo->dev,
				"ambarella_sd_data_done:"
				" pslotinfo->mrq is NULL\n");
			return;
		}

		if (!(data)) {
			dev_err(pinfo->dev,
				"ambarella_sd_cmd_done: data is NULL\n");
			return;
		}

		if (eis != 0x0) {
			if (eis & SD_EIS_CMD_BIT_ERR) {
				data->error = -EILSEQ;
			} else if (eis & SD_EIS_CMD_CRC_ERR) {
				data->error = -EILSEQ;
			} else if (eis & SD_EIS_CMD_TMOUT_ERR) {
				data->error = -ETIMEDOUT;
			} else {
				data->error = -EILSEQ;
			}
			ambarella_sd_reset_data_line(pslotinfo->mmc);
		} else {
			data->bytes_xfered = pslotinfo->dma_size;
		}

		pslotinfo->post_dma(pslotinfo);
		pslotinfo->state = AMBA_SD_STATE_IDLE;
		wake_up(&pslotinfo->wait);
	}
}

static void ambarella_sd_cmd_done(
	struct ambarella_sd_mmc_info *pslotinfo,
	u16 nis,
	u16 eis)
{
	struct mmc_command			*cmd;
	u32					rsp0, rsp1, rsp2, rsp3;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	if (pslotinfo->mrq)
		cmd = pslotinfo->mrq->cmd;
	else {
		dev_err(pinfo->dev,
			"ambarella_sd_cmd_done: pslotinfo->mrq is NULL\n");
		return;
	}

	if (!(cmd)) {
		dev_err(pinfo->dev,
			"ambarella_sd_cmd_done: cmd is NULL\n");
		return;
	}

	if (eis != 0x0) {
		if (eis & SD_EIS_CMD_BIT_ERR) {
			cmd->error = -EILSEQ;
		} else if (eis & SD_EIS_CMD_CRC_ERR) {
			cmd->error = -EILSEQ;
		} else if (eis & SD_EIS_CMD_TMOUT_ERR) {
			cmd->error = -ETIMEDOUT;
		} else {
			cmd->error = -EILSEQ;
		}
		ambarella_sd_reset_cmd_line(pslotinfo->mmc);
	}

	rsp0 = amb_sd_readl(pinfo->regbase + SD_RSP0_OFFSET);
	rsp1 = amb_sd_readl(pinfo->regbase + SD_RSP1_OFFSET);
	rsp2 = amb_sd_readl(pinfo->regbase + SD_RSP2_OFFSET);
	rsp3 = amb_sd_readl(pinfo->regbase + SD_RSP3_OFFSET);

	if (cmd->flags & MMC_RSP_136) {
		cmd->resp[0] = ((rsp3 << 8) | (rsp2 >> 24));
		cmd->resp[1] = ((rsp2 << 8) | (rsp1 >> 24));
		cmd->resp[2] = ((rsp1 << 8) | (rsp0 >> 24));
		cmd->resp[3] = (rsp0 << 8);
	} else {
		cmd->resp[0] = rsp0;
	}

	if (pslotinfo->state == AMBA_SD_STATE_CMD) {
		pslotinfo->state = AMBA_SD_STATE_IDLE;
		wake_up(&pslotinfo->wait);
	}
}

static irqreturn_t ambarella_sd_irq(int irq, void *devid)
{
	struct ambarella_sd_controller_info	*pinfo;
	struct ambarella_sd_mmc_info		*pslotinfo = NULL;
	u16					nis;
	u16					eis;
	u32					enabled = 0;
	u16 					nisen;
	u32					i;

	pinfo = (struct ambarella_sd_controller_info *)devid;

	/* Read the interrupt registers */
	nis = amb_sd_readw(pinfo->regbase + SD_NIS_OFFSET);
	eis = amb_sd_readw(pinfo->regbase + SD_EIS_OFFSET);
	nisen = amb_sd_readw(pinfo->regbase + SD_NISEN_OFFSET);

	if (!nis)
		goto ambarella_sd_irq_exit;

	for (i = 0; i < pinfo->controller_info.num_slots; i++) {
		pslotinfo = pinfo->pslotinfo[i];
		if (pslotinfo->slot_info.check_owner) {
			enabled = pslotinfo->slot_info.check_owner();
		}
		if (enabled)
			break;
	}

	if (!pslotinfo)
		goto ambarella_sd_irq_exit;

	dev_dbg(pinfo->dev,
		"ambarella_sd_irq slot%d nis = 0x%x, eis = 0x%x & %d:%d\n",
		pslotinfo->slot_id,
		//amb_sd_readw(pinfo->regbase + SD_SIST_OFFSET),
		nis, eis, pslotinfo->state, enabled);

	//Check false DAT[1] SD_NIS_CARD IRQ & card generated interrupt
	if (enabled) {
		if (pinfo->nisen != nisen)
			amb_sd_writew(pinfo->nisen,
				pinfo->regbase + SD_NISEN_OFFSET);
		if (nis & SD_NIS_CARD) {
			dev_dbg(pinfo->dev, "SD_NIS_CARD\n");
			mmc_signal_sdio_irq(pslotinfo->mmc);
		}
	} else {
		if (nis & SD_NIS_CARD) {
			nisen = (nisen & (~(SD_NISEN_CARD)));
			dev_dbg(pinfo->dev, "Disable SD_NISEN_CARD\n");
			amb_sd_writew(nisen, pinfo->regbase + SD_NISEN_OFFSET);
		}
	}

	/* Check for card detection interrupt */
	if (!ambarella_is_valid_gpio_irq(&pslotinfo->slot_info.gpio_cd)) {
		if (nis & SD_NIS_REMOVAL) {
			dev_dbg(pinfo->dev, "SD_NIS_REMOVAL\n");
			mmc_detect_change(pslotinfo->mmc, 0);
		} else if (nis & SD_NIS_INSERT) {
			if (pslotinfo->slot_info.cd_delay > 0) {
				mmc_detect_change(pslotinfo->mmc,
					pslotinfo->slot_info.cd_delay);
				dev_dbg(pinfo->dev, "SD_NIS_INSERT %d...\n",
					pslotinfo->slot_info.cd_delay);
			} else {
				mmc_detect_change(pslotinfo->mmc, 5);
				dev_dbg(pinfo->dev, "SD_NIS_INSERT...\n");
			}
		}
	}

	/* Clear interrupt */
	amb_sd_writew(nis, pinfo->regbase + SD_NIS_OFFSET);
	amb_sd_writew(eis, pinfo->regbase + SD_EIS_OFFSET);

	if (!enabled)
		goto ambarella_sd_irq_exit;

	/* Check for command normal completion */
	if (nis & SD_NIS_CMD_DONE) {
		ambarella_sd_cmd_done(pslotinfo, nis, eis);
	}

	/* Check for data normal completion */
	if (nis & SD_NIS_XFR_DONE) {
		ambarella_sd_data_done(pslotinfo, nis, eis);
	}
	else /* Check for DMA interrupt */
	if (nis & SD_NIS_DMA) {
		pslotinfo->pre_dma(pslotinfo);
		if (pslotinfo->dma_address != 0) {
			amb_sd_writel(pslotinfo->dma_address,
				pinfo->regbase + SD_DMA_ADDR_OFFSET);
		}
	}

	if (eis != 0x0) {
		if (pslotinfo->state == AMBA_SD_STATE_CMD)
			ambarella_sd_cmd_done(pslotinfo, nis, eis);
		else if (pslotinfo->state == AMBA_SD_STATE_DATA)
			ambarella_sd_data_done(pslotinfo, nis, eis);
	}

ambarella_sd_irq_exit:
	return IRQ_HANDLED;
}

static irqreturn_t ambarella_sd_gpio_cd_irq(int irq, void *devid)
{
	struct ambarella_sd_mmc_info		*pslotinfo;
	u32					val = 0;
	struct ambarella_sd_controller_info	*pinfo;

	pslotinfo = (struct ambarella_sd_mmc_info *)devid;
	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	if (pslotinfo->valid &&
		ambarella_is_valid_gpio_irq(&pslotinfo->slot_info.gpio_cd)) {
		ambarella_gpio_config(pslotinfo->slot_info.gpio_cd.irq_gpio,
			GPIO_FUNC_SW_INPUT);
		val = ambarella_gpio_get(pslotinfo->slot_info.gpio_cd.irq_gpio);
		ambarella_gpio_config(pslotinfo->slot_info.gpio_cd.irq_gpio,
			pslotinfo->slot_info.gpio_cd.irq_gpio_mode);

		if (val != pslotinfo->slot_info.gpio_cd.irq_gpio_val) {
			dev_dbg(pinfo->dev, "card ejection\n");
			mmc_detect_change(pslotinfo->mmc, 0);
		} else {
			if (pslotinfo->slot_info.cd_delay > 0) {
				mmc_detect_change(pslotinfo->mmc,
					pslotinfo->slot_info.cd_delay);
				dev_dbg(pinfo->dev, "card insert %d...\n",
					pslotinfo->slot_info.cd_delay);
			} else {
				mmc_detect_change(pslotinfo->mmc, 5);
				dev_dbg(pinfo->dev, "card insert...\n");
			}
		}
	}

	return IRQ_HANDLED;
}

static void ambarella_sd_send_cmd(
	struct ambarella_sd_mmc_info *pslotinfo,
	struct mmc_command *cmd)
{
	struct mmc_data				*data = cmd->data;
	u32					counter = 0;
	u32					tmpreg;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	pslotinfo->state = AMBA_SD_STATE_CMD;
	pslotinfo->sg_len = 0;
	pslotinfo->sg_index = 0;
	pslotinfo->sg = NULL;
	pslotinfo->blk_sz = 0;
	pslotinfo->blk_cnt = 0;
	pslotinfo->arg_reg = 0;
	pslotinfo->xfr_reg = 0;
	pslotinfo->cmd_reg = 0;
	pslotinfo->tmo = 0;

	pslotinfo->dma_address = 0;
	pslotinfo->dma_left = 0;
	pslotinfo->dma_need_fill = 0;
	pslotinfo->dma_size = 0;
	pslotinfo->dma_per_size = 0;

	if (!(cmd->flags & MMC_RSP_PRESENT))
		pslotinfo->cmd_reg = SD_CMD_RSP_NONE;
	else if (cmd->flags & MMC_RSP_136)
		pslotinfo->cmd_reg = SD_CMD_RSP_136;
	else if (cmd->flags & MMC_RSP_BUSY)
		pslotinfo->cmd_reg = SD_CMD_RSP_48BUSY;
	else
		pslotinfo->cmd_reg = SD_CMD_RSP_48;

	if (cmd->flags & MMC_RSP_CRC)
		pslotinfo->cmd_reg |= SD_CMD_CHKCRC;

	if (cmd->flags & MMC_RSP_OPCODE)
		pslotinfo->cmd_reg |= SD_CMD_CHKIDX;

	pslotinfo->cmd_reg |= SD_CMD_IDX(cmd->opcode);
	pslotinfo->arg_reg = cmd->arg;

	if (data != NULL) {
#ifdef CONFIG_MMC_AMBARELLA_CAL_TMO
		u32 sd_clk;
		u32 clk_divisor;
		u32 tmoclk;
		u32 desired_tmo;
		u32 actual_tmo;

		sd_clk = get_sd_freq_hz();
		clk_divisor = amb_sd_readw(pinfo->regbase + SD_CLK_OFFSET);
		clk_divisor >>= 8;
		tmoclk = sd_clk / (0x1 << clk_divisor) / 1000;
		desired_tmo = (data->timeout_ns / 1000000) * tmoclk;
		desired_tmo += data->timeout_clks / 1000;
		for ((pslotinfo->tmo) = 0;
			(pslotinfo->tmo) < 0xe; (pslotinfo->tmo)++) {
			actual_tmo =
				(2 << (13 + (pslotinfo->tmo))) * tmoclk / 1000;
			if (actual_tmo >= desired_tmo)
				break;
		}
#else
		pslotinfo->tmo = CONFIG_SD_AMBARELLA_TIMEOUT_VAL;
#endif
		pslotinfo->state = AMBA_SD_STATE_DATA;

		pslotinfo->blk_sz = data->blksz;
		pslotinfo->dma_size = data->blksz * data->blocks;

		pslotinfo->sg_len = data->sg_len;
		pslotinfo->sg = data->sg;

		pslotinfo->xfr_reg |= SD_XFR_DMA_EN;
		pslotinfo->cmd_reg |= SD_CMD_DATA;

		pslotinfo->blk_cnt = data->blocks;
		if (pslotinfo->blk_cnt > 1) {
			pslotinfo->xfr_reg |= SD_XFR_MUL_SEL;
			pslotinfo->xfr_reg |= SD_XFR_BLKCNT_EN;
		}

		if (data->flags & MMC_DATA_STREAM) {
			pslotinfo->xfr_reg |= SD_XFR_MUL_SEL;
			pslotinfo->xfr_reg &= ~SD_XFR_BLKCNT_EN;
		}

		if (data->flags & MMC_DATA_WRITE) {
			pslotinfo->xfr_reg &= ~SD_XFR_CTH_SEL;
			pslotinfo->pre_dma = &ambarella_sd_pre_sg_to_dma;
			pslotinfo->post_dma = &ambarella_sd_post_sg_to_dma;
		} else {
			pslotinfo->xfr_reg |= SD_XFR_CTH_SEL;
			pslotinfo->pre_dma = &ambarella_sd_pre_dma_to_sg;
			pslotinfo->post_dma = &ambarella_sd_post_dma_to_sg;
		}

		pslotinfo->pre_dma(pslotinfo);

		while (1) {
			tmpreg = amb_sd_readl(pinfo->regbase + SD_STA_OFFSET);
			if ((tmpreg & SD_STA_CMD_INHIBIT_DAT) == 0) {
				break;
			}
			counter++;
			if (counter > CONFIG_SD_AMBARELLA_WAIT_COUNTER_LIMIT) {
				dev_warn(pinfo->dev,
					"Wait SD_STA_CMD_INHIBIT_DAT %d.\n",
					counter);
				pslotinfo->state = AMBA_SD_STATE_ERR;
				return;
			}
		}

		amb_sd_writeb(pslotinfo->tmo, pinfo->regbase + SD_TMO_OFFSET);
		if (pslotinfo->dma_address == 0) {
			dev_warn(pinfo->dev, "Wrong dma_address.\n");
			pslotinfo->state = AMBA_SD_STATE_ERR;
			return;
		}
		amb_sd_writel(pslotinfo->dma_address,
			pinfo->regbase + SD_DMA_ADDR_OFFSET);
		amb_sd_writew(pslotinfo->blk_sz,
			pinfo->regbase + SD_BLK_SZ_OFFSET);
		amb_sd_writew(pslotinfo->blk_cnt,
			pinfo->regbase + SD_BLK_CNT_OFFSET);
		amb_sd_writel(pslotinfo->arg_reg,
			pinfo->regbase + SD_ARG_OFFSET);
		amb_sd_writew(pslotinfo->xfr_reg,
			pinfo->regbase + SD_XFR_OFFSET);
		amb_sd_writew(pslotinfo->cmd_reg,
			pinfo->regbase + SD_CMD_OFFSET);
	} else {
		while (1) {
			tmpreg = amb_sd_readl(pinfo->regbase + SD_STA_OFFSET);
			if ((tmpreg & SD_STA_CMD_INHIBIT_CMD) == 0) {
				break;
			}
			counter++;
			if (counter > CONFIG_SD_AMBARELLA_WAIT_COUNTER_LIMIT) {
				dev_warn(pinfo->dev,
					"Wait SD_STA_CMD_INHIBIT_CMD %d.\n",
					counter);
				pslotinfo->state = AMBA_SD_STATE_ERR;
				return;
			}
		}

		amb_sd_writel(pslotinfo->arg_reg,
			pinfo->regbase + SD_ARG_OFFSET);
		amb_sd_writew(pslotinfo->cmd_reg,
			pinfo->regbase + SD_CMD_OFFSET);
	}
	ambarella_sd_show_info(pslotinfo);
}

static void ambarella_sd_set_clk(struct mmc_host *mmc, u32 clk)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	u16					clk_div;
	u16					clkreg;
	u32					sd_clk;
	u32					desired_clk;
	u32					actual_clk;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	pinfo->controller_info.clk_limit = min(get_sd_freq_hz(),
		pinfo->controller_info.clk_limit);
	mmc->f_max = pinfo->controller_info.clk_limit;
	mmc->f_min = get_sd_freq_hz() >> 8;

	if (clk == 0) {
		amb_sd_writew(0, pinfo->regbase + SD_CLK_OFFSET);
#if 0
		dev_dbg(pinfo->dev, "dma_w_fill_counter = 0x%x.\n",
			pslotinfo->dma_w_fill_counter);
		dev_dbg(pinfo->dev, "dma_w_counter = 0x%x.\n",
			pslotinfo->dma_w_counter);
		dev_dbg(pinfo->dev, "dma_r_fill_counter = 0x%x.\n",
			pslotinfo->dma_r_fill_counter);
		dev_dbg(pinfo->dev, "dma_r_counter = 0x%x.\n",
			pslotinfo->dma_r_counter);
		pslotinfo->dma_w_fill_counter = 0;
		pslotinfo->dma_w_counter = 0;
		pslotinfo->dma_r_fill_counter = 0;
		pslotinfo->dma_r_counter = 0;
#endif
	} else {
		sd_clk = get_sd_freq_hz();
		dev_dbg(pinfo->dev, "sd_clk = %d.\n", sd_clk);

		desired_clk = clk;
		for (clk_div = 0x0; clk_div <= 0x80;) {
			if (clk_div == 0)
				actual_clk = sd_clk;
			else
				actual_clk = sd_clk / (clk_div << 1);

			if (actual_clk <= desired_clk)
				break;

			if (clk_div >= 0x80)
				break;

			if (clk_div == 0x0)
				clk_div = 0x1;
			else
				clk_div <<= 1;
		}
		dev_dbg(pinfo->dev, "desired_clk = %d.\n", desired_clk);
		dev_dbg(pinfo->dev, "actual_clk = %d.\n", actual_clk);
		dev_dbg(pinfo->dev, "clk_div = %d.\n", clk_div);

		clk_div <<= 8;
		clk_div |= SD_CLK_ICLK_EN;
		amb_sd_writew(clk_div, pinfo->regbase + SD_CLK_OFFSET);
		while (1) {
			clkreg = amb_sd_readw(pinfo->regbase + SD_CLK_OFFSET);
			if (clkreg & SD_CLK_ICLK_STABLE)
				break;
		}
		clk_div |= SD_CLK_EN;
		amb_sd_writew(clk_div, pinfo->regbase + SD_CLK_OFFSET);
		// Wait stable
		msleep(5);
	}
}

static void ambarella_sd_set_pwr(struct mmc_host *mmc, u32 pwr_mode, u32 vdd)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	u8					pwr = SD_PWR_OFF;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	if (pwr_mode == MMC_POWER_OFF) {
		amb_sd_writeb(pwr, pinfo->regbase + SD_PWR_OFFSET);
		ambarella_set_gpio_power(&pslotinfo->slot_info.ext_power, 0);
	} else if (pwr_mode == MMC_POWER_UP) {
		ambarella_set_gpio_power(&pslotinfo->slot_info.ext_power, 1);
	}

	if ((pwr_mode == MMC_POWER_ON) || (pwr_mode == MMC_POWER_UP)) {
		pwr = SD_PWR_ON;
		switch (1 << vdd) {
		case MMC_VDD_165_195:
			pwr |= SD_PWR_1_8V;
			break;
		case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr |= SD_PWR_3_0V;
			break;
		case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr |= SD_PWR_3_3V;
			break;
		default:
			pwr = SD_PWR_OFF;
			dev_err(pinfo->dev, "Wrong voltage[%d]!\n", vdd);
			break;
		}
		amb_sd_writeb(pwr, pinfo->regbase + SD_PWR_OFFSET);
	}

	dev_dbg(pinfo->dev, "pwr = 0x%x.\n", pwr);
}

static void ambarella_sd_set_bus(struct mmc_host *mmc,
	u32 bus_width, u32 timing)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	u8					hostr = 0;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	hostr = amb_sd_readb(pinfo->regbase + SD_HOST_OFFSET);
	if (bus_width == MMC_BUS_WIDTH_4) {
		hostr &= ~(SD_HOST_8BIT);
		hostr |= SD_HOST_4BIT;
	} else if (bus_width == MMC_BUS_WIDTH_1) {
		hostr &= ~(SD_HOST_8BIT);
		hostr &= ~(SD_HOST_4BIT);
	}
	hostr &= ~SD_HOST_HIGH_SPEED;
	if (timing == MMC_TIMING_SD_HS) {
		dev_dbg(pinfo->dev, "MMC_TIMING_SD_HS!\n");
		//hostr |= SD_HOST_HIGH_SPEED;
	}
	amb_sd_writeb(hostr, pinfo->regbase + SD_HOST_OFFSET);

	dev_dbg(pinfo->dev, "hostr = 0x%x.\n", hostr);
}

static void ambarella_sd_check_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	if (pinfo->controller_ios.clock != ios->clock) {
		ambarella_sd_set_clk(mmc, ios->clock);
		pinfo->controller_ios.clock = ios->clock;
	}

	if ((pinfo->controller_ios.power_mode != ios->power_mode) ||
		(pinfo->controller_ios.vdd != ios->vdd)){
		ambarella_sd_set_pwr(mmc, ios->power_mode, ios->vdd);
		pinfo->controller_ios.power_mode = ios->power_mode;
		pinfo->controller_ios.vdd = ios->vdd;
	}

	if ((pinfo->controller_ios.bus_width != ios->bus_width) ||
		(pinfo->controller_ios.timing != ios->timing)){
		ambarella_sd_set_bus(mmc, ios->bus_width, ios->timing);
		pinfo->controller_ios.bus_width = ios->bus_width;
		pinfo->controller_ios.timing = ios->timing;
	}
}

static void ambarella_sd_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	ambarella_sd_request_bus(mmc);
	ambarella_sd_check_ios(mmc, ios);
	ambarella_sd_release_bus(mmc);
}

static int ambarella_sd_get_ro(struct mmc_host *mmc)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	u32					wpspl;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	ambarella_sd_request_bus(mmc);

	wpspl = amb_sd_readl(pinfo->regbase + SD_STA_OFFSET);
	dev_dbg(pinfo->dev, "SD/MMC RD[0x%x].\n", wpspl);
	wpspl &= SD_STA_WPS_PL;

	ambarella_sd_release_bus(mmc);

	return wpspl ? 1 : 0;
}

static void ambarella_sd_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct ambarella_sd_mmc_info		*pslotinfo = mmc_priv(mmc);
	long					timeout;
	u32					card_sta;
	u32					need_reset = 0;
	u32					error_id = -ETIMEDOUT;
	struct ambarella_sd_controller_info	*pinfo;

	pinfo = (struct ambarella_sd_controller_info *)pslotinfo->pinfo;

	ambarella_sd_request_bus(mmc);
	ambarella_sd_check_ios(mmc, &mmc->ios);

	pslotinfo->mrq = mrq;

	card_sta = amb_sd_readl(pinfo->regbase + SD_STA_OFFSET);

	if ((card_sta & SD_STA_CARD_INSERTED) ||
		ambarella_is_valid_gpio_irq(&pslotinfo->slot_info.gpio_cd)) {
		ambarella_sd_send_cmd(pslotinfo, mrq->cmd);
		if (pslotinfo->state != AMBA_SD_STATE_ERR) {
			timeout = wait_event_timeout(pslotinfo->wait,
					(pslotinfo->state ==
						AMBA_SD_STATE_IDLE),
					pinfo->controller_info.wait_tmo);
			if (timeout <= 0) {
				dev_err(pinfo->dev, "cmd timeout %d %d!\n",
						pslotinfo->state,
						mrq->cmd->opcode);
				need_reset = 1;
				goto ambarella_sd_request_need_reset;
			}
			dev_dbg(pinfo->dev, "%ld jiffies left.\n", timeout);
			if (mrq->cmd->error) {
				need_reset = 1;
				goto ambarella_sd_request_need_reset;
			}
			if ((mrq->cmd->data) &&
				(mrq->cmd->data->error)) {
				need_reset = 1;
				goto ambarella_sd_request_need_reset;
			}
		} else {
			need_reset = 1;
			goto ambarella_sd_request_need_reset;
		}

		if (mrq->stop) {
			ambarella_sd_send_cmd(pslotinfo, mrq->stop);
			if (pslotinfo->state != AMBA_SD_STATE_ERR) {
				timeout = wait_event_timeout(pslotinfo->wait,
					(pslotinfo->state ==
						AMBA_SD_STATE_IDLE),
					pinfo->controller_info.wait_tmo);
				if (timeout <= 0) {
					dev_err(pinfo->dev,
						"stop timeout %d %d!\n",
						pslotinfo->state,
						mrq->stop->opcode);
					need_reset = 1;
					goto ambarella_sd_request_need_reset;
				}
				dev_dbg(pinfo->dev, "%ld jiffies left.\n",
					timeout);
				if (mrq->stop->error) {
					need_reset = 1;
					goto ambarella_sd_request_need_reset;
				}
			} else {
				need_reset = 1;
				goto ambarella_sd_request_need_reset;
			}
		}
	} else {
		dev_dbg(pinfo->dev, "card_sta 0x%x.\n", card_sta);
		mrq->cmd->error = error_id;
	}
	if (pslotinfo->state != AMBA_SD_STATE_IDLE)
		need_reset = 1;

ambarella_sd_request_need_reset:
	if (need_reset) {
		if (mrq->stop) {
			if (mrq->stop->error) {
				dev_dbg(pinfo->dev, "stop->error = %d\n",
					mrq->stop->error);
				error_id = mrq->stop->error;
			}
			else
				mrq->stop->error = error_id;
		}

		if (mrq->cmd->data) {
			if (mrq->cmd->data->error) {
				dev_dbg(pinfo->dev, "data->error = %d\n",
					mrq->cmd->data->error);
				error_id = mrq->cmd->data->error;
			}
			else
				mrq->cmd->data->error = error_id;
		}

		if (mrq->cmd->error) {
			dev_dbg(pinfo->dev, "cmd->error = %d\n",
				mrq->cmd->error);
			error_id = mrq->cmd->error;
		}
		else
			mrq->cmd->error = error_id;

		dev_dbg(pinfo->dev, "need_reset %d %d 0x%x %d!\n",
				pslotinfo->state, mrq->cmd->opcode,
				card_sta, error_id);

		ambarella_sd_reset_all(pslotinfo->mmc);
	}

	pslotinfo->mrq = NULL;

	ambarella_sd_release_bus(mmc);

	mmc_request_done(mmc, mrq);
}

static const struct mmc_host_ops ambarella_sd_host_ops = {
	.request	= ambarella_sd_request,
	.set_ios	= ambarella_sd_ios,
	.get_ro		= ambarella_sd_get_ro,
	.enable_sdio_irq= ambarella_sd_enable_sdio_irq,
};

#ifdef CONFIG_CPU_FREQ
static int ambsd_freq_transition(struct notifier_block *nb,
	unsigned long val, void *data)
{
	struct platform_device			*pdev;
	struct ambarella_sd_controller_info	*pinfo;
	struct mmc_host				*mmc;
	unsigned long				flags;
	u32					i;

	pinfo = container_of(nb,struct ambarella_sd_controller_info,
			sd_freq_transition);
	pdev = to_platform_device(pinfo->dev);

	local_irq_save(flags);

	switch (val) {
	case CPUFREQ_PRECHANGE:
		printk("%s[%d]: Pre Change\n", __func__, pdev->id);
		break;

	case CPUFREQ_POSTCHANGE:
		printk("%s[%d]: Post Change\n", __func__, pdev->id);
		memset(&pinfo->controller_ios, 0, sizeof(struct mmc_ios));
		for (i = 0; i < pinfo->controller_info.num_slots; i++) {
			mmc = pinfo->pslotinfo[i]->mmc;
			ambarella_sd_check_ios(mmc, &pinfo->controller_ios);
		}
		break;
	}

	local_irq_restore(flags);

	return 0;
}
#endif

static int __devinit ambarella_sd_probe(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct ambarella_sd_controller_info	*pinfo;
	struct ambarella_sd_mmc_info		*pslotinfo = NULL;
	struct resource 			*irq;
	struct resource 			*mem;
	struct resource 			*ioarea;
	struct mmc_host				*mmc;
	u32					hc_cap;
	u32					i;
	u32					clock_min;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "Get SD/MMC mem resource failed!\n");
		errorCode = -ENXIO;
		goto sd_errorCode_na;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq == NULL) {
		dev_err(&pdev->dev, "Get SD/MMC irq resource failed!\n");
		errorCode = -ENXIO;
		goto sd_errorCode_na;
	}

	ioarea = request_mem_region(mem->start,
			(mem->end - mem->start) + 1, pdev->name);
	if (ioarea == NULL) {
		dev_err(&pdev->dev, "Request SD/MMC ioarea failed!\n");
		errorCode = -EBUSY;
		goto sd_errorCode_na;
	}

	pinfo = kzalloc(sizeof(struct ambarella_sd_controller_info),
		GFP_KERNEL);
	if (pinfo == NULL) {
		dev_err(&pdev->dev, "Out of memory!\n");
		errorCode = -ENOMEM;
		goto sd_errorCode_ioarea;
	}
	pinfo->regbase = (unsigned char __iomem *)mem->start;
	pinfo->dev = &pdev->dev;
	pinfo->mem = mem;
	pinfo->irq = irq->start;
	spin_lock_init(&pinfo->lock);
	pinfo->nisen = 0;
	if (pdev->dev.platform_data == NULL) {
		dev_err(&pdev->dev, "Need SD/MMC controller info!\n");
		errorCode = -EPERM;
		goto sd_errorCode_free_pinfo;
	}
	memcpy(&pinfo->controller_info, pdev->dev.platform_data,
		sizeof(struct ambarella_sd_controller));

	if (pinfo->controller_info.wait_tmo <
		CONFIG_SD_AMBARELLA_WAIT_TIMEOUT) {
		dev_dbg(&pdev->dev,
			"Change wait timeout from %d to %d.\n",
			pinfo->controller_info.wait_tmo,
			CONFIG_SD_AMBARELLA_WAIT_TIMEOUT);
		pinfo->controller_info.wait_tmo =
			CONFIG_SD_AMBARELLA_WAIT_TIMEOUT;
	}

	clock_min = get_sd_freq_hz() >> 8;
	if (pinfo->controller_info.clk_limit < clock_min) {
		dev_dbg(&pdev->dev,
			"Change clock limit from %dKHz to %dKHz.\n",
			pinfo->controller_info.clk_limit,
			clock_min);
		pinfo->controller_info.clk_limit = clock_min;
	}

	for (i = 0; i < pinfo->controller_info.num_slots; i++) {
		mmc = mmc_alloc_host(sizeof(struct ambarella_sd_mmc_info),
			&pdev->dev);
		if (!mmc) {
			dev_err(&pdev->dev, "Failed to allocate mmc host!\n");
			errorCode = -ENOMEM;
			goto sd_errorCode_free_host;
		}
		mmc->ops = &ambarella_sd_host_ops;

		pinfo->pslotinfo[i] = pslotinfo = mmc_priv(mmc);
		pslotinfo->mmc = mmc;
		init_waitqueue_head(&pslotinfo->wait);
		pslotinfo->state = AMBA_SD_STATE_ERR;
		pslotinfo->dma_w_fill_counter = 0;
		pslotinfo->dma_w_counter = 0;
		pslotinfo->dma_r_fill_counter = 0;
		pslotinfo->dma_r_counter = 0;
		memcpy(&pslotinfo->slot_info, &pinfo->controller_info.slot[i],
			sizeof(struct ambarella_sd_slot));
		pslotinfo->slot_id = i;
		pslotinfo->pinfo = pinfo;

		ambarella_sd_request_bus(mmc);

		ambarella_sd_reset_all(mmc);

		hc_cap = amb_sd_readl(pinfo->regbase + SD_CAP_OFFSET);
		if (hc_cap & SD_CAP_TOCLK_MHZ)
			dev_dbg(&pdev->dev,
				"Timeout Clock Frequency: %dMHz.\n",
				SD_CAP_TOCLK_FREQ(hc_cap));
		else
			dev_dbg(&pdev->dev,
				"Timeout Clock Frequency: %dKHz.\n",
				SD_CAP_TOCLK_FREQ(hc_cap));

		mmc->f_min = clock_min;
		mmc->f_max = pinfo->controller_info.clk_limit;
		dev_dbg(&pdev->dev,
			"SD Clock: base[%dMHz], min[%dHz], max[%dHz].\n",
			SD_CAP_BASE_FREQ(hc_cap),
			mmc->f_min,
			mmc->f_max);

		if (hc_cap & SD_CAP_MAX_2KB_BLK)
			mmc->max_blk_size = 2048;
		else if (hc_cap & SD_CAP_MAX_1KB_BLK)
			mmc->max_blk_size = 1024;
		else if (hc_cap & SD_CAP_MAX_512B_BLK)
			mmc->max_blk_size = 512;
		dev_dbg(&pdev->dev,
			"SD max_blk_size: %d.\n",
			mmc->max_blk_size);

		mmc->caps = MMC_CAP_4_BIT_DATA | MMC_CAP_SDIO_IRQ;
		if (hc_cap & SD_CAP_HIGH_SPEED) {
			mmc->caps |= MMC_CAP_SD_HIGHSPEED;
			mmc->caps |= MMC_CAP_MMC_HIGHSPEED;
		}
		dev_dbg(&pdev->dev, "SD caps: 0x%lx.\n", mmc->caps);

		if (hc_cap & SD_CAP_DMA) {
			dev_dbg(&pdev->dev, "HW support DMA!\n");
		} else {
			dev_err(&pdev->dev, "HW do not support DMA!\n");
			errorCode = -ENODEV;
			goto sd_errorCode_free_host;
		}

		if (hc_cap & SD_CAP_SUS_RES)
			dev_dbg(&pdev->dev, "HW support Suspend/Resume!\n");
		else
			dev_dbg(&pdev->dev,
				"HW do not support Suspend/Resume!\n");

		mmc->ocr_avail = 0;
		if (hc_cap & SD_CAP_VOL_1_8V)
			mmc->ocr_avail |= MMC_VDD_165_195;
		if (hc_cap & SD_CAP_VOL_3_0V)
			mmc->ocr_avail |= MMC_VDD_29_30 | MMC_VDD_30_31;
		if (hc_cap & SD_CAP_VOL_3_3V)
			mmc->ocr_avail |= MMC_VDD_32_33 | MMC_VDD_33_34;
		if (mmc->ocr_avail == 0) {
			dev_err(&pdev->dev,
				"Hardware report wrong voltages[0x%x].!\n",
				hc_cap);
			errorCode = -ENODEV;
			goto sd_errorCode_free_host;
		}

		if (hc_cap & SD_CAP_INTMODE) {
			dev_dbg(&pdev->dev, "HW support Interrupt mode!\n");
		} else {
			dev_err(&pdev->dev,
				"HW do not support Interrupt mode!\n");
			errorCode = -ENODEV;
			goto sd_errorCode_free_host;
		}

		if (pslotinfo->slot_info.bounce_buffer) {
			mmc->max_hw_segs = 128;
			mmc->max_phys_segs = 128;
			mmc->max_seg_size = CONFIG_SD_AMBARELLA_MAX_DMA_SIZE;
			mmc->max_req_size = mmc->max_seg_size;
			mmc->max_blk_count = 0xFFFF;

			pslotinfo->buf_vaddress = kmalloc(mmc->max_seg_size,
					GFP_NOIO | GFP_DMA |
					__GFP_REPEAT | __GFP_NOWARN);
			if (!pslotinfo->buf_vaddress) {
				dev_err(&pdev->dev,
					"Can't malloc dma memory!\n");
				errorCode = -ENOMEM;
				goto sd_errorCode_free_host;
			}

			pslotinfo->buf_paddress = dma_map_single(pinfo->dev,
				pslotinfo->buf_vaddress,
				mmc->max_seg_size,
				DMA_BIDIRECTIONAL);

			if (ambarella_sd_check_dma_boundary(pslotinfo,
				pslotinfo->buf_paddress,
				mmc->max_seg_size,
				CONFIG_SD_AMBARELLA_MAX_DMA_SIZE) == 0) {
				dev_err(&pdev->dev,
					"Dma memory out of boundary!\n");
				errorCode = -ENOMEM;
				goto sd_errorCode_free_host;
			}
		} else {
			mmc->max_hw_segs = 1;
			mmc->max_phys_segs = 1;
			mmc->max_seg_size = CONFIG_SD_AMBARELLA_MAX_DMA_SIZE;
			mmc->max_req_size = mmc->max_seg_size;
			mmc->max_blk_count = 0xFFFF;

			pslotinfo->buf_paddress = (dma_addr_t)NULL;
			pslotinfo->buf_vaddress = NULL;
		}

		if (pslotinfo->slot_info.ext_power.power_gpio != -1) {
			errorCode = gpio_request(
				pslotinfo->slot_info.ext_power.power_gpio,
				pdev->name);
			if (errorCode < 0) {
				dev_err(&pdev->dev,
				"Could not get Power GPIO %d\n",
				pslotinfo->slot_info.ext_power.power_gpio);
				goto sd_errorCode_free_host;
			}
		}

		if (ambarella_is_valid_gpio_irq(
			&pslotinfo->slot_info.gpio_cd)) {
			errorCode = gpio_request(
				pslotinfo->slot_info.gpio_cd.irq_gpio,
				pdev->name);
			if (errorCode < 0) {
				dev_err(&pdev->dev,
					"Could not get CD GPIO %d\n",
					pslotinfo->slot_info.gpio_cd.irq_gpio);
				goto sd_errorCode_free_host;
			}

			ambarella_gpio_config(
				pslotinfo->slot_info.gpio_cd.irq_gpio,
				pslotinfo->slot_info.gpio_cd.irq_gpio_mode);
			errorCode = request_irq(
				pslotinfo->slot_info.gpio_cd.irq_line,
				ambarella_sd_gpio_cd_irq,
				pslotinfo->slot_info.gpio_cd.irq_type,
				pdev->name, pslotinfo);
			if (errorCode) {
				dev_err(&pdev->dev,
					"Request GPIO(%d) CD IRQ(%d) FAIL!\n",
					pslotinfo->slot_info.gpio_cd.irq_gpio,
					pslotinfo->slot_info.gpio_cd.irq_line);
				goto sd_errorCode_free_host;
			}
		}

		ambarella_sd_release_bus(mmc);
	}

	errorCode = request_irq(pinfo->irq,
		ambarella_sd_irq,
		IRQF_SHARED | IRQF_TRIGGER_HIGH,
		pdev->name, pinfo);
	if (errorCode) {
		dev_err(&pdev->dev, "Request IRQ failed!\n");
		goto sd_errorCode_free_host;
	}

	for (i = 0; i < pinfo->controller_info.num_slots; i++) {
		pslotinfo = pinfo->pslotinfo[i];
		errorCode = mmc_add_host(pslotinfo->mmc);
		if (errorCode) {
			dev_err(&pdev->dev, "Add mmc host failed!\n");
			goto sd_errorCode_remove_host;
		}
		pslotinfo->valid = 1;
		msleep(100);
	}

	platform_set_drvdata(pdev, pinfo);

#ifdef CONFIG_CPU_FREQ
	pinfo->sd_freq_transition.notifier_call = ambsd_freq_transition;
	cpufreq_register_notifier(&pinfo->sd_freq_transition,
		CPUFREQ_TRANSITION_NOTIFIER);
#endif

	dev_notice(&pdev->dev,
		"Ambarella Media Processor SD/MMC[%d] probed %d slots!\n",
		pdev->id, pinfo->controller_info.num_slots);

	goto sd_errorCode_na;

sd_errorCode_remove_host:
	for (i = 0; i < pinfo->controller_info.num_slots; i++) {
		pslotinfo = pinfo->pslotinfo[i];
		if ((pslotinfo->mmc) && (pslotinfo->valid == 1))
			mmc_remove_host(pslotinfo->mmc);
	}

	free_irq(pinfo->irq, pinfo);

sd_errorCode_free_host:
	for (i = 0; i < pinfo->controller_info.num_slots; i++) {
		pslotinfo = pinfo->pslotinfo[i];

		if (ambarella_is_valid_gpio_irq(
			&pslotinfo->slot_info.gpio_cd)) {
			free_irq(pslotinfo->slot_info.gpio_cd.irq_line,
				pslotinfo);
			gpio_free(pslotinfo->slot_info.gpio_cd.irq_gpio);
		}

		if (pslotinfo->slot_info.ext_power.power_gpio != -1) {
			gpio_free(pslotinfo->slot_info.ext_power.power_gpio);
		}

		if (pslotinfo->buf_paddress) {
			dma_unmap_single(pinfo->dev,
				pslotinfo->buf_paddress,
				pslotinfo->mmc->max_seg_size,
				DMA_BIDIRECTIONAL);
			pslotinfo->buf_paddress = (dma_addr_t)NULL;
		}

		if (pslotinfo->buf_vaddress) {
			kfree(pslotinfo->buf_vaddress);
			pslotinfo->buf_vaddress = NULL;
		}

		if (pslotinfo->mmc);
			mmc_free_host(pslotinfo->mmc);
	}

sd_errorCode_free_pinfo:
	kfree(pinfo);

sd_errorCode_ioarea:
	release_mem_region(mem->start, (mem->end - mem->start) + 1);

sd_errorCode_na:
	return errorCode;
}

static int __devexit ambarella_sd_remove(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct ambarella_sd_controller_info	*pinfo;
	struct ambarella_sd_mmc_info		*pslotinfo;
	u32					i;

	pinfo = platform_get_drvdata(pdev);

	if (pinfo) {
		platform_set_drvdata(pdev, NULL);

		free_irq(pinfo->irq, pinfo);

		for (i = 0; i < pinfo->controller_info.num_slots; i++) {
			pslotinfo = pinfo->pslotinfo[i];

			if (ambarella_is_valid_gpio_irq(
				&pslotinfo->slot_info.gpio_cd)) {
				free_irq(pslotinfo->slot_info.gpio_cd.irq_line,
					pslotinfo);
				gpio_free(
					pslotinfo->slot_info.gpio_cd.irq_gpio);
			}

			if (pslotinfo->mmc)
				mmc_remove_host(pslotinfo->mmc);
		}

		for (i = 0; i < pinfo->controller_info.num_slots; i++) {
			pslotinfo = pinfo->pslotinfo[i];

			if (pslotinfo->slot_info.ext_power.power_gpio != -1) {
				gpio_free(
				pslotinfo->slot_info.ext_power.power_gpio);
			}

			if (pslotinfo->buf_paddress) {
				dma_unmap_single(pinfo->dev,
					pslotinfo->buf_paddress,
					pslotinfo->mmc->max_seg_size,
					DMA_BIDIRECTIONAL);
				pslotinfo->buf_paddress = (dma_addr_t)NULL;
			}

			if (pslotinfo->buf_vaddress) {
				kfree(pslotinfo->buf_vaddress);
				pslotinfo->buf_vaddress = NULL;
			}

			if (pslotinfo->mmc);
				mmc_free_host(pslotinfo->mmc);
		}

		release_mem_region(pinfo->mem->start,
			(pinfo->mem->end - pinfo->mem->start) + 1);

		kfree(pinfo);
	}

	dev_notice(&pdev->dev,
		"Remove Ambarella Media Processor SD/MMC Host Controller.\n");

	return errorCode;
}

#ifdef CONFIG_PM
static int ambarella_sd_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int					errorCode = 0;

	dev_info(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);
	return errorCode;
}

static int ambarella_sd_resume(struct platform_device *pdev)
{
	int					errorCode = 0;

	dev_info(&pdev->dev, "%s exit with %d\n", __func__, errorCode);

	return errorCode;
}
#endif

static struct platform_driver ambarella_sd_driver = {
	.probe		= ambarella_sd_probe,
	.remove		= __devexit_p(ambarella_sd_remove),
#ifdef CONFIG_PM
	.suspend	= ambarella_sd_suspend,
	.resume		= ambarella_sd_resume,
#endif
	.driver		= {
		.name	= "ambarella-sd",
		.owner	= THIS_MODULE,
	},
};

static int __init ambarella_sd_init(void)
{
	int				errorCode = 0;

	rct_set_sd_pll();

	errorCode = platform_driver_register(&ambarella_sd_driver);
	if (errorCode)
		printk(KERN_ERR "Register ambarella_sd_driver failed %d!\n",
			errorCode);

	return errorCode;
}

static void __exit ambarella_sd_exit(void)
{
	platform_driver_unregister(&ambarella_sd_driver);
}

module_init(ambarella_sd_init);
module_exit(ambarella_sd_exit);

MODULE_DESCRIPTION("Ambarella Media Processor SD/MMC Host Controller");
MODULE_AUTHOR("Anthony Ginger, <hfjiang@ambarella.com>");
MODULE_LICENSE("GPL");

