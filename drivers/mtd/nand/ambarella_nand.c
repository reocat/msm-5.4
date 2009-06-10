/*
 * drivers/mtd/ambarella_nand.c
 *
 * History:
 *	2008/04/11 - [Cao Rongrong & Chien-Yang Chen] created file
 *	2009/01/04 - [Anthony Ginger] Port to 2.6.28
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

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <linux/cpufreq.h>

#include <mach/hardware.h>
#include <mach/dma.h>

#define AMBARELLA_NAND_DMA_BUFFER_SIZE		(2048)

//nand_check_wp will be checked before write, so wait MTD fix
#undef AMBARELLA_NAND_WP

struct nxact_s {
	u32				cmd;
	struct {
		int	bank;
		int	addr_hi;
		u32	addr;
		u32	dst;
		u8	*buf;
		int	len;
		int	area;
		int	ecc;
		u8	intlve;
	}				arg;
	union {
		u32	*id;
		u32	*status;
	}				val;
	u32				fio_dma_sta;
	int				error;
};

struct nand_amb_s
{
	wait_queue_head_t		wq;
	dma_addr_t			dmaaddr;
	unsigned char			*dmabuf;
	u32				status;
	int				dma_bufpos;
	int				irq_flag;
	int				bank;
	struct cmd_arg_s {
		unsigned	command;
		int		column;
		int		page_addr;
	}				cmd_arg;

	struct nxact_s			nxact;
	u32				control;
};

struct ambarella_nand_mtd {
	struct mtd_info			mtd;
	struct nand_chip		chip;
	struct ambarella_nand_set	*set;
	void				*nand_info;	//struct ambarella_nand_info
};

struct ambarella_nand_info {
	struct nand_amb_s		amb_nand;
	struct nand_hw_control		controller;
	struct ambarella_nand_mtd	*amb_mtd;
	struct ambarella_platform_nand	*plat_nand;
	struct device			*dev;
	int				nr_mtds;
	int				suspend;
	int				dma_irq;
	int				cmd_irq;
	int				wp_gpio;
	unsigned char __iomem		*regbase;
	u32				dmabase;
#ifdef CONFIG_CPU_FREQ
	u32				origin_clk;	/* in Khz */
	struct ambarella_nand_timing	*origin_timing;
	struct notifier_block	nand_freq_transition;
	struct notifier_block	nand_freq_policy;
#endif
};

static struct nand_ecclayout amb_oobinfo_512 = {
	.eccbytes = 6,
	.eccpos = {5, 6, 7, 8, 9, 10},
	.oobfree = {{0, 5}, {11, 5}}
};

static struct nand_ecclayout amb_oobinfo_2048 = {
	.eccbytes = 24,
	.eccpos = { 0, 8, 9, 10,11, 12,
		16, 24, 25, 26, 27, 28,
		32, 40, 41, 42, 43, 44,
		48, 56, 57, 58, 59, 60},
	.oobfree = {{1, 7}, {13, 3},
		{17, 7}, {29, 3},
		{33, 7}, {45, 3},
		{49, 7}, {61, 3}}
};

static uint8_t amb_scan_ff_pattern[] = { 0xff, 0xff };

static struct nand_bbt_descr amb_512_bbt_descr = {
	.offs = 5,
	.len = 1,
	.pattern = amb_scan_ff_pattern
};

static struct nand_bbt_descr amb_2048_bbt_descr = {
	.offs = 0,
	.len = 1,
	.pattern = amb_scan_ff_pattern
};

/* ==========================================================================*/
static inline struct ambarella_nand_mtd *amb_nand_mtd_by_minfo(
	struct mtd_info *mtd)
{
	return container_of(mtd, struct ambarella_nand_mtd, mtd);
}

static inline struct ambarella_nand_info *amb_nand_info_by_minfo(
	struct mtd_info *mtd)
{
	return amb_nand_mtd_by_minfo(mtd)->nand_info;
}

static inline struct ambarella_nand_info *amb_nand_info_by_amb_nand(
	struct nand_amb_s *amb_nand)
{
	return container_of(amb_nand, struct ambarella_nand_info, amb_nand);
}

static inline struct ambarella_nand_info *amb_nand_info_by_nxact(
	struct nxact_s *nxact)
{
	struct nand_amb_s			*amb_nand;

	amb_nand = container_of(nxact, struct nand_amb_s, nxact);

	return amb_nand_info_by_amb_nand(amb_nand);
}

/* ==========================================================================*/


#ifdef CONFIG_CPU_FREQ

//#define CPUFREQ_DBG
#ifdef CPUFREQ_DBG
#define cpufreq_dbg(format, arg...)	printk(format, ##arg)
#else
#define cpufreq_dbg(format, arg...)
#endif

#define NAND_TIMING_RSHIFT24BIT(x)	(((x) & 0xff000000) >> 24)
#define NAND_TIMING_RSHIFT16BIT(x)	(((x) & 0x00ff0000) >> 16)
#define NAND_TIMING_RSHIFT8BIT(x)	(((x) & 0x0000ff00) >> 8)
#define NAND_TIMING_RSHIFT0BIT(x)	(((x) & 0x000000ff) >> 0)

#define NAND_TIMING_LSHIFT24BIT(x)	((x) << 24)
#define NAND_TIMING_LSHIFT16BIT(x)	((x) << 16)
#define NAND_TIMING_LSHIFT8BIT(x)	((x) << 8)
#define NAND_TIMING_LSHIFT0BIT(x)	((x) << 0)

#define NAND_TIMING_CALC_SPDUP(val, new_clk, origin_clk)		\
	((val) * (new_clk) / (origin_clk))
#define NAND_TIMING_CALC_SPDDOWN(val, new_clk, origin_clk)	\
	((val) * (new_clk) / (origin_clk) +			\
	 (((val) * (new_clk)) % (origin_clk) ? 1 : 0))
#define NAND_TIMING_CALC_NEW(val, new_clk, origin_clk)		\
	((new_clk) > (origin_clk) ?				\
	NAND_TIMING_CALC_SPDUP(val, new_clk, origin_clk) :	\
	NAND_TIMING_CALC_SPDDOWN(val, new_clk, origin_clk))

static void amb_nand_set_timing(struct ambarella_nand_info *nand_info,
	struct ambarella_nand_timing *timing);

static u32 ambnand_calc_timing(struct ambarella_nand_info *nand_info, u32 idx)
{ 
	u32 origin_clk = nand_info->origin_clk;
	u32 new_clk = get_ahb_bus_freq_hz() / 1000;
	u32 origin_timing, timing_reg = 0;
	u8 tcls, tals, tcs, tds;
	u8 tclh, talh, tch, tdh;
	u8 twp, twh, twb, trr;
	u8 trp, treh, trb, tceh;
	u8 trdelay, tclr, twhr, tir;
	u8 tww, trhz, tar;

	switch(idx)
	{
	case 0: /* Calculate timing 0 */
		origin_timing = nand_info->origin_timing->timing0;
		if (origin_clk == new_clk)
			return origin_timing;

		tcls = NAND_TIMING_RSHIFT24BIT(origin_timing);
		tals = NAND_TIMING_RSHIFT16BIT(origin_timing);
		tcs = NAND_TIMING_RSHIFT8BIT(origin_timing);
		tds = NAND_TIMING_RSHIFT0BIT(origin_timing);

		tcls = NAND_TIMING_CALC_NEW(tcls, new_clk, origin_clk);
		tals = NAND_TIMING_CALC_NEW(tals, new_clk, origin_clk);
		tcs = NAND_TIMING_CALC_NEW(tcs, new_clk, origin_clk);
		tds = NAND_TIMING_CALC_NEW(tds, new_clk, origin_clk);

		timing_reg = NAND_TIMING_LSHIFT24BIT(tcls) | 
			NAND_TIMING_LSHIFT16BIT(tals) |
			NAND_TIMING_LSHIFT8BIT(tcs) |
			NAND_TIMING_LSHIFT0BIT(tds);
		break;
	case 1: /* Calculate timing 1*/
		origin_timing = nand_info->origin_timing->timing1;
		if (origin_clk == new_clk)
			return origin_timing;

		tclh = NAND_TIMING_RSHIFT24BIT(origin_timing);
		talh = NAND_TIMING_RSHIFT16BIT(origin_timing);
		tch = NAND_TIMING_RSHIFT8BIT(origin_timing);
		tdh = NAND_TIMING_RSHIFT0BIT(origin_timing);

		tclh = NAND_TIMING_CALC_NEW(tclh, new_clk, origin_clk);
		talh = NAND_TIMING_CALC_NEW(talh, new_clk, origin_clk);
		tch = NAND_TIMING_CALC_NEW(tch, new_clk, origin_clk);
		tdh = NAND_TIMING_CALC_NEW(tdh, new_clk, origin_clk);

		timing_reg = NAND_TIMING_LSHIFT24BIT(tclh) | 
			NAND_TIMING_LSHIFT16BIT(talh) |
			NAND_TIMING_LSHIFT8BIT(tch) |
			NAND_TIMING_LSHIFT0BIT(tdh);
		break;
	case 2: /* Calculate timing 2*/
		origin_timing = nand_info->origin_timing->timing2;
		if (origin_clk == new_clk)
			return origin_timing;

		twp = NAND_TIMING_RSHIFT24BIT(origin_timing);
		twh = NAND_TIMING_RSHIFT16BIT(origin_timing);
		twb = NAND_TIMING_RSHIFT8BIT(origin_timing) + 1;
		trr = NAND_TIMING_RSHIFT0BIT(origin_timing);

		twp = NAND_TIMING_CALC_NEW(twp, new_clk, origin_clk);
		twh = NAND_TIMING_CALC_NEW(twh, new_clk, origin_clk);
		twb = NAND_TIMING_CALC_NEW(twb, new_clk, origin_clk);
		twb = (twb - 1) ? (twb - 1) : 1;
		trr = NAND_TIMING_CALC_NEW(trr, new_clk, origin_clk);

		timing_reg = NAND_TIMING_LSHIFT24BIT(twp) | 
			NAND_TIMING_LSHIFT16BIT(twh) |
			NAND_TIMING_LSHIFT8BIT(twb) |
			NAND_TIMING_LSHIFT0BIT(trr);
		break;
	case 3: /* Calculate timing 3*/
		origin_timing = nand_info->origin_timing->timing3;
		if (origin_clk == new_clk)
			return origin_timing;

		trp = NAND_TIMING_RSHIFT24BIT(origin_timing);
		treh = NAND_TIMING_RSHIFT16BIT(origin_timing);
		trb = NAND_TIMING_RSHIFT8BIT(origin_timing) + 1;
		tceh = NAND_TIMING_RSHIFT0BIT(origin_timing) + 1;

		trp = NAND_TIMING_CALC_NEW(trp, new_clk, origin_clk);
		treh = NAND_TIMING_CALC_NEW(treh, new_clk, origin_clk);
		trb = NAND_TIMING_CALC_NEW(trb, new_clk, origin_clk);
		trb = (trb - 1) ? (trb - 1) : 1;
		tceh = NAND_TIMING_CALC_NEW(tceh, new_clk, origin_clk);
		tceh = (tceh - 1) ? (tceh - 1) : 1;

		timing_reg = NAND_TIMING_LSHIFT24BIT(trp) | 
			NAND_TIMING_LSHIFT16BIT(treh) |
			NAND_TIMING_LSHIFT8BIT(trb) |
			NAND_TIMING_LSHIFT0BIT(tceh);
		break;
	case 4: /* Calculate timing 4*/
		origin_timing = nand_info->origin_timing->timing4;
		if (origin_clk == new_clk)
			return origin_timing;

		trdelay = NAND_TIMING_RSHIFT24BIT(origin_timing) + 1;
		tclr = NAND_TIMING_RSHIFT16BIT(origin_timing);
		twhr = NAND_TIMING_RSHIFT8BIT(origin_timing);
		tir = NAND_TIMING_RSHIFT0BIT(origin_timing);

		trdelay = NAND_TIMING_CALC_NEW(trdelay, new_clk, origin_clk);
		trdelay = (trdelay - 1) ? (trdelay - 1) : 1;
		tclr = NAND_TIMING_CALC_NEW(tclr, new_clk, origin_clk);
		twhr = NAND_TIMING_CALC_NEW(twhr, new_clk, origin_clk);
		tir = NAND_TIMING_CALC_NEW(tir, new_clk, origin_clk);

		timing_reg = NAND_TIMING_LSHIFT24BIT(trdelay) | 
			NAND_TIMING_LSHIFT16BIT(tclr) |
			NAND_TIMING_LSHIFT8BIT(twhr) |
			NAND_TIMING_LSHIFT0BIT(tir);
		break;
	case 5: /* Calculate timing 5*/
		origin_timing = nand_info->origin_timing->timing5;
		if (origin_clk == new_clk)
			return origin_timing;

		tww = NAND_TIMING_RSHIFT16BIT(origin_timing);
		trhz = NAND_TIMING_RSHIFT8BIT(origin_timing) + 1;
		tar = NAND_TIMING_RSHIFT0BIT(origin_timing) + 1;

		tww = NAND_TIMING_CALC_NEW(tww, new_clk, origin_clk);
		trhz = NAND_TIMING_CALC_NEW(trhz, new_clk, origin_clk);
		trhz = (trhz - 1) ? (trhz - 1) : 1;
		tar = NAND_TIMING_CALC_NEW(tar, new_clk, origin_clk);
		tar = (tar - 1) ? (tar - 1) : 1;

		timing_reg = NAND_TIMING_LSHIFT16BIT(tww) |
			NAND_TIMING_LSHIFT8BIT(trhz) |
			NAND_TIMING_LSHIFT0BIT(tar);
		break;
	}	

	return timing_reg;
}

static int ambnand_freq_transition(struct notifier_block *nb,
	unsigned long val, void *data)
{
	struct ambarella_nand_timing timing;
	struct ambarella_nand_info *nand_info = container_of(nb,
		struct ambarella_nand_info, nand_freq_transition);
	unsigned long flags;

	/* TODO struct cpufreq_freqs *f = data; */

	local_irq_save(flags);

	switch (val) {
	case CPUFREQ_PRECHANGE:
		printk("%s: Pre Change\n", __func__);
		break;

	case CPUFREQ_POSTCHANGE:
		printk("%s: Post Change\n", __func__);
		timing.timing0 = ambnand_calc_timing(nand_info, 0);
		timing.timing1 = ambnand_calc_timing(nand_info, 1);
		timing.timing2 = ambnand_calc_timing(nand_info, 2);
		timing.timing3 = ambnand_calc_timing(nand_info, 3);
		timing.timing4 = ambnand_calc_timing(nand_info, 4);
		timing.timing5 = ambnand_calc_timing(nand_info, 5);
		amb_nand_set_timing(nand_info, &timing);

		cpufreq_dbg("origin reg:\t0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", 
			nand_info->origin_timing->timing0, 
			nand_info->origin_timing->timing1, 
			nand_info->origin_timing->timing2,
			nand_info->origin_timing->timing3, 
			nand_info->origin_timing->timing4, 
			nand_info->origin_timing->timing5);

		cpufreq_dbg("new reg:\t0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", 
			timing.timing0, timing.timing1, timing.timing2,
			timing.timing3, timing.timing4, timing.timing5);
		
		break;
	}

	local_irq_restore(flags);

	return 0;
}

#endif

static irqreturn_t nand_fiocmd_isr_handler(int irq, void *dev_id)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u32					val;

	nand_info = (struct ambarella_nand_info *)dev_id;
	amb_nand = &nand_info->amb_nand;

	val = amba_readl(nand_info->regbase + FIO_STA_OFFSET);

	if (val & FIO_STA_FI) {
		amba_clrbits(nand_info->regbase + FIO_CTR_OFFSET,
			(FIO_CTR_RS | FIO_CTR_SE | FIO_CTR_CO));

		amba_writel(nand_info->regbase + FLASH_INT_OFFSET, 0x0);

		amb_nand->irq_flag |= 0x1;
		wake_up_interruptible(&amb_nand->wq);
	}

	return IRQ_HANDLED;
}

static irqreturn_t nand_fiodma_isr_handler(int irq, void *dev_id)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	struct nxact_s				*nxact;
	u32					val;

	nand_info = (struct ambarella_nand_info *)dev_id;
	amb_nand = &nand_info->amb_nand;

	val = amba_readl(nand_info->regbase + FIO_DMACTR_OFFSET);

	if ((val & (FIO_DMACTR_SD | FIO_DMACTR_CF |
		FIO_DMACTR_XD | FIO_DMACTR_FL)) ==  FIO_DMACTR_FL) {
		nxact = &amb_nand->nxact;
		nxact->fio_dma_sta =
			amba_readl(nand_info->regbase + FIO_DMASTA_OFFSET);

		amba_writel(nand_info->regbase + FIO_DMASTA_OFFSET, 0x0);

		amb_nand->irq_flag |= 0x2;
		wake_up_interruptible(&amb_nand->wq);
	}

	return IRQ_HANDLED;
}

static void nand_dma_isr_handler(void *dev_id, u32 status)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;

	nand_info = (struct ambarella_nand_info *)dev_id;
	amb_nand = &nand_info->amb_nand;

	amb_nand->status = status;
	amb_nand->irq_flag |= 0x4;
	wake_up_interruptible(&amb_nand->wq);
}

static void nand_amb_setup_dma_devmem(struct nxact_s *nxact)
{
	u32					val;
	u8					*dst;
	int					len;
	ambarella_dma_req_t			dma_req;
	struct ambarella_nand_info		*nand_info;

	nand_info = amb_nand_info_by_nxact(nxact);
	dst = nxact->arg.buf;
	len = nxact->arg.len;

	dma_req.src = io_v2p(nand_info->dmabase);
	dma_req.dst = (u32) dst;
	dma_req.next = NULL;
	dma_req.rpt = (u32) NULL;
	dma_req.xfr_count = len;
	if (len > 16) {
		dma_req.attr = DMA_CHANX_CTR_WM |
			DMA_CHANX_CTR_NI |
			DMA_NODC_MN_BURST_SIZE;
	} else {
		dma_req.attr = DMA_CHANX_CTR_WM |
			DMA_CHANX_CTR_NI |
			DMA_NODC_SP_BURST_SIZE;
	}

	ambarella_dma_xfr(&dma_req, FIO_DMA_CHAN);

	amba_writel(nand_info->regbase + FIO_DMAADR_OFFSET, nxact->arg.addr);
	if (len > 16) {
		val = FIO_DMACTR_EN |
			FIO_DMACTR_FL |
			FIO_MN_BURST_SIZE |
			len;
	} else {
		val = FIO_DMACTR_EN |
			FIO_DMACTR_FL |
			FIO_SP_BURST_SIZE |
			len;
	}
	amba_writel(nand_info->regbase + FIO_DMACTR_OFFSET, val);
}

static void nand_amb_setup_dma_memdev(struct nxact_s *nxact)
{
	u32					val;
	u8					*src;
	int					len;
	ambarella_dma_req_t			dma_req;
	struct ambarella_nand_info		*nand_info;

	nand_info = amb_nand_info_by_nxact(nxact);
	src = nxact->arg.buf;
	len = nxact->arg.len;

	dma_req.src = (u32) src;
	dma_req.dst = io_v2p(nand_info->dmabase);
	dma_req.next = NULL;
	dma_req.rpt = (u32) NULL;
	dma_req.xfr_count = len;
	if (len > 16) {
		dma_req.attr = DMA_CHANX_CTR_RM |
			DMA_CHANX_CTR_NI |
			DMA_NODC_MN_BURST_SIZE;
	} else {
		dma_req.attr = DMA_CHANX_CTR_RM |
			DMA_CHANX_CTR_NI |
			DMA_NODC_SP_BURST_SIZE;
	}

	ambarella_dma_xfr(&dma_req, FIO_DMA_CHAN);

	amba_writel(nand_info->regbase + FIO_DMAADR_OFFSET, nxact->arg.addr);

	if (len > 16) {
		val = FIO_DMACTR_EN |
			FIO_DMACTR_FL |
			FIO_MN_BURST_SIZE |
			FIO_DMACTR_RM |
			len;
	} else {
		val = FIO_DMACTR_EN |
			FIO_DMACTR_FL |
			FIO_SP_BURST_SIZE |
			FIO_DMACTR_RM |
			len;
	}

	amba_writel(nand_info->regbase + FIO_DMACTR_OFFSET, val);
}

static int nand_amb_request(struct nand_amb_s *amb_nand)
{
	int					errorCode = 0;
	struct nxact_s				*nxact;
	u32					cmd;
	u32					nand_ctr_reg = 0;
	u32					nand_cmd_reg = 0;
	u32					fio_ctr_reg = 0;
	struct ambarella_nand_info		*nand_info;

	nand_info = amb_nand_info_by_amb_nand(amb_nand);
	nxact = &amb_nand->nxact;
	cmd = nxact->cmd;

	nand_ctr_reg = amb_nand->control | NAND_CTR_WAS; 

	fio_select_lock(SELECT_FIO_FL, 1);

#ifdef AMBARELLA_NAND_WP
	if ((cmd == NAND_AMB_CMD_ERASE ||
		cmd == NAND_AMB_CMD_COPYBACK ||
		cmd == NAND_AMB_CMD_PROGRAM) &&
		nand_info->wp_gpio >= 0)
		gpio_direction_output(nand_info->wp_gpio, GPIO_HIGH);
#endif

	switch (cmd) {
	case NAND_AMB_CMD_RESET:
		nand_cmd_reg = NAND_AMB_CMD_RESET;
		amba_writel(nand_info->regbase + FLASH_CMD_OFFSET,
			nand_cmd_reg);
		break;

	case NAND_AMB_CMD_READID:
		nand_ctr_reg |= NAND_CTR_A(nxact->arg.addr_hi);
		nand_cmd_reg = nxact->arg.addr | NAND_AMB_CMD_READID;
		amba_writel(nand_info->regbase + FLASH_CTR_OFFSET,
			nand_ctr_reg);
		amba_writel(nand_info->regbase + FLASH_CMD_OFFSET,
			nand_cmd_reg);
		break;

	case NAND_AMB_CMD_READSTATUS:
		nand_ctr_reg |= NAND_CTR_A(nxact->arg.addr_hi);
		nand_cmd_reg = nxact->arg.addr | NAND_AMB_CMD_READSTATUS;
		amba_writel(nand_info->regbase + FLASH_CTR_OFFSET,
			nand_ctr_reg);
		amba_writel(nand_info->regbase + FLASH_CMD_OFFSET,
			nand_cmd_reg);
		break;

	case NAND_AMB_CMD_ERASE:
		nand_ctr_reg |= NAND_CTR_A(nxact->arg.addr_hi);
		nand_cmd_reg = nxact->arg.addr | NAND_AMB_CMD_ERASE;
		amba_writel(nand_info->regbase + FLASH_CTR_OFFSET,
			nand_ctr_reg);
		amba_writel(nand_info->regbase + FLASH_CMD_OFFSET,
			nand_cmd_reg);
		break;

	case NAND_AMB_CMD_COPYBACK:
		nand_ctr_reg |= NAND_CTR_A(nxact->arg.addr_hi);
		nand_ctr_reg |= NAND_CTR_CE;
		nand_cmd_reg = nxact->arg.addr | NAND_AMB_CMD_COPYBACK;
		amba_writel(nand_info->regbase + FLASH_CFI_OFFSET,
			nxact->arg.dst);
		amba_writel(nand_info->regbase + FLASH_CTR_OFFSET,
			nand_ctr_reg);
		amba_writel(nand_info->regbase + FLASH_CMD_OFFSET,
			nand_cmd_reg);
		break;

	case NAND_AMB_CMD_READ:
		nand_ctr_reg |= NAND_CTR_A(nxact->arg.addr_hi);

		if (nxact->arg.area == MAIN_ECC)
			nand_ctr_reg |= (NAND_CTR_SE);
		else if (nxact->arg.area == SPARE_ONLY ||
			nxact->arg.area == SPARE_ECC)
			nand_ctr_reg |= (NAND_CTR_SE | NAND_CTR_SA);

		fio_ctr_reg = amba_readl(nand_info->regbase + FIO_CTR_OFFSET);

		if (nxact->arg.area == SPARE_ONLY ||
			nxact->arg.area == SPARE_ECC  ||
			nxact->arg.area == MAIN_ECC)
			fio_ctr_reg |= (FIO_CTR_RS);

		switch (nxact->arg.ecc) {
		case EC_MDSE:
			nand_ctr_reg |= NAND_CTR_EC_SPARE;
			fio_ctr_reg |= FIO_CTR_CO;
			break;
		case EC_MESD:
			nand_ctr_reg |= NAND_CTR_EC_MAIN;
			fio_ctr_reg |= FIO_CTR_CO;
			break;
		case EC_MESE:
			nand_ctr_reg |=	(NAND_CTR_EC_MAIN | NAND_CTR_EC_SPARE);
			fio_ctr_reg |= FIO_CTR_CO;
			break;
		case EC_MDSD:
		default:
			break;
		}

		amba_writel(nand_info->regbase + FIO_CTR_OFFSET,
			fio_ctr_reg);
		amba_writel(nand_info->regbase + FLASH_CTR_OFFSET,
			nand_ctr_reg);
		nand_amb_setup_dma_devmem(nxact);

		break;

	case NAND_AMB_CMD_PROGRAM:
		nand_ctr_reg |= NAND_CTR_A(nxact->arg.addr_hi);

		if (nxact->arg.area == MAIN_ECC)
			nand_ctr_reg |= (NAND_CTR_SE);
		else if (nxact->arg.area == SPARE_ONLY ||
			nxact->arg.area == SPARE_ECC)
			nand_ctr_reg |= (NAND_CTR_SE | NAND_CTR_SA);

		fio_ctr_reg = amba_readl(nand_info->regbase + FIO_CTR_OFFSET);

		if (nxact->arg.area == SPARE_ONLY ||
			nxact->arg.area == SPARE_ECC  ||
			nxact->arg.area == MAIN_ECC)
			fio_ctr_reg |= (FIO_CTR_RS);

		switch (nxact->arg.ecc) {
		case EG_MDSE :
			nand_ctr_reg |= NAND_CTR_EG_SPARE;
			break;
		case EG_MESD :
			nand_ctr_reg |= NAND_CTR_EG_MAIN;
			break;
		case EG_MESE :
			nand_ctr_reg |= (NAND_CTR_EG_MAIN | NAND_CTR_EG_SPARE);
			break;
		case EG_MDSD:
		default:
			break;
		}

		amba_writel(nand_info->regbase + FIO_CTR_OFFSET,
			fio_ctr_reg);
		amba_writel(nand_info->regbase + FLASH_CTR_OFFSET,
			nand_ctr_reg);
		nand_amb_setup_dma_memdev(nxact);

		break;

	default:
		dev_warn(nand_info->dev,
			"%s: wrong command %d!\n", __func__, cmd);
		errorCode = -EINVAL;
		goto nand_amb_request_exit;
		break;
	}

	if (cmd == NAND_AMB_CMD_READ || cmd == NAND_AMB_CMD_PROGRAM) {
		wait_event_interruptible_timeout(amb_nand->wq,
			(amb_nand->irq_flag == 0x7), 5 * HZ);

		if (amb_nand->status & (DMA_CHANX_STA_OE | DMA_CHANX_STA_ME |
			DMA_CHANX_STA_BE | DMA_CHANX_STA_RWE |
			DMA_CHANX_STA_AE)) {
			dev_err(nand_info->dev,
				"%s: Errors happend in DMA transaction %d!\n",
				__func__, amb_nand->status);
			errorCode = -1;
		}

		if(errorCode != -1) {
			errorCode = fio_dma_parse_error(nxact->fio_dma_sta);
			if (errorCode) {
				dev_err(nand_info->dev,
					"%s: cmd=%d, bank=0x%x, addr_hi=0x%x, "
					"addr=0x%x, dst=0x%x, buf=0x%x, "
					"len=0x%x, area=0x%x, ecc=0x%x, "
					"intlve=0x%x, control=0x%x:0x%x!\n",
					__func__, cmd,
					nxact->arg.bank,
					nxact->arg.addr_hi,
					nxact->arg.addr,
					nxact->arg.dst,
					(u32)nxact->arg.buf,
					nxact->arg.len,
					nxact->arg.area,
					nxact->arg.ecc,
					nxact->arg.intlve,
					amb_nand->control,
					nand_ctr_reg);
			}
		}
	} else {
		wait_event_interruptible_timeout(amb_nand->wq,
			(amb_nand->irq_flag == 0x1), 5 * HZ);
		if (cmd == NAND_AMB_CMD_READID) {	
			*(nxact->val.id) =
				amba_readl(nand_info->regbase +
				FLASH_ID_OFFSET);
		} else if (cmd == NAND_AMB_CMD_READSTATUS) {
			*(nxact->val.status) =
				amba_readl(nand_info->regbase +
				FLASH_STA_OFFSET);
		}
	}

	amb_nand->irq_flag = 0x0;

nand_amb_request_exit:
#ifdef AMBARELLA_NAND_WP
	if ((cmd == NAND_AMB_CMD_ERASE ||
		cmd == NAND_AMB_CMD_COPYBACK ||
		cmd == NAND_AMB_CMD_PROGRAM) &&
		nand_info->wp_gpio >= 0)
		gpio_direction_output(nand_info->wp_gpio, GPIO_LOW);
#endif

	fio_unlock(SELECT_FIO_FL, 1);

	return errorCode;
}

int nand_amb_read_id(struct ambarella_nand_info *nand_info,
	u32 bank, u32 *id)
{
	int					errorCode = 0;
	struct nand_amb_s			*amb_nand;

	amb_nand = &nand_info->amb_nand;

	if(bank > 0){
		dev_err(nand_info->dev,
			"%s: Not supported yet (bank > 0).\n", __func__);
		errorCode = -EINVAL;
		goto nand_amb_read_id_exit;
	}

	amb_nand->nxact.cmd = NAND_AMB_CMD_READID;
	amb_nand->nxact.arg.addr_hi = 0;
	amb_nand->nxact.arg.addr = 0;
	amb_nand->nxact.val.id = id;

	errorCode = nand_amb_request(amb_nand);

nand_amb_read_id_exit:
	return errorCode;
}

int nand_amb_read_status(struct ambarella_nand_info *nand_info,
	u32 bank, u32 *status)
{
	int					errorCode = 0;
	struct nand_amb_s			*amb_nand;

	amb_nand = &nand_info->amb_nand;

	if(bank > 0){
		dev_err(nand_info->dev,
			"%s: Not supported yet (bank > 0).\n", __func__);
		errorCode = -EINVAL;
		goto nand_amb_read_status_exit;
	}

	amb_nand->nxact.cmd = NAND_AMB_CMD_READSTATUS;
	amb_nand->nxact.arg.addr_hi = 0;
	amb_nand->nxact.arg.addr = 0;
	amb_nand->nxact.val.status = status;

	errorCode = nand_amb_request(amb_nand);

nand_amb_read_status_exit:
	return errorCode;
}

int nand_amb_erase(struct ambarella_nand_info *nand_info, u32 page_addr)
{
	int					errorCode = 0;
	struct nand_amb_s			*amb_nand;
	struct ambarella_nand_mtd		*amb_mtd;
	u32					addr_hi;
	u32					addr;
	u64					addr64;

	amb_nand = &nand_info->amb_nand;
	amb_mtd = nand_info->amb_mtd;
	addr64 = (u64)(page_addr * amb_mtd->mtd.writesize);
	addr_hi = (u32)(addr64 >> 32);
	addr = (u32)addr64;

	amb_nand->nxact.cmd = NAND_AMB_CMD_ERASE;
	amb_nand->nxact.arg.addr_hi = addr_hi;
	amb_nand->nxact.arg.addr = addr;
	amb_nand->nxact.arg.intlve = 1;

	errorCode = nand_amb_request(amb_nand);

	return errorCode;
}

int nand_amb_write_page(struct ambarella_nand_info *nand_info,
	u32 page_addr, u8 *buf, u8 area)
{
	int					errorCode = 0;
	struct nand_amb_s			*amb_nand;
	struct ambarella_nand_mtd		*amb_mtd;
	u32					addr_hi;
	u32					addr;
	u32					len;
	u64					addr64;
	u8					ecc;

	amb_nand = &nand_info->amb_nand;
	amb_mtd = nand_info->amb_mtd;
	addr64 = (u64)(page_addr * amb_mtd->mtd.writesize);
	addr_hi = (u32)(addr64 >> 32);
	addr = (u32)addr64;

	switch (area) {
	case MAIN_ONLY:
		ecc = EG_MDSD;
		len = amb_mtd->mtd.writesize;
		break;
	case MAIN_ECC:
		ecc = EG_MESD;
		len = amb_mtd->mtd.writesize;
		break;
	case SPARE_ONLY:
		ecc = EG_MDSD;
		len = amb_mtd->mtd.oobsize;
		break;
	case SPARE_ECC:
		ecc = EG_MDSE;
		len = amb_mtd->mtd.oobsize;
		break;
	default:
		dev_err(nand_info->dev, "%s: Wrong area.\n", __func__);
		errorCode = -EINVAL;
		goto nand_amb_write_page_exit;
		break;
	}

	amb_nand->nxact.cmd = NAND_AMB_CMD_PROGRAM;
	amb_nand->nxact.arg.addr_hi = addr_hi;
	amb_nand->nxact.arg.addr = addr;
	amb_nand->nxact.arg.buf = buf;
	amb_nand->nxact.arg.len = len;
	amb_nand->nxact.arg.area = area;
	amb_nand->nxact.arg.ecc = ecc;
	amb_nand->nxact.arg.intlve = 1;

	errorCode = nand_amb_request(amb_nand);

nand_amb_write_page_exit:
	return errorCode;
}

int nand_amb_read_page(struct ambarella_nand_info *nand_info,
	u32 page_addr, u8 *buf, u8 area)
{
	int					errorCode = 0;
	struct nand_amb_s			*amb_nand;
	struct ambarella_nand_mtd		*amb_mtd;
	u32					addr_hi;
	u32					addr;
	u32					len;
	u64					addr64;
	u8					ecc;

	amb_nand = &nand_info->amb_nand;
	amb_mtd = nand_info->amb_mtd;
	addr64 = (u64)(page_addr * amb_mtd->mtd.writesize);
	addr_hi = (u32)(addr64 >> 32);
	addr = (u32)addr64;

	switch (area) {
	case MAIN_ONLY:
		ecc = EC_MDSD;
		len = amb_mtd->mtd.writesize;
		break;
	case MAIN_ECC:
		ecc = EC_MESD;
		len = amb_mtd->mtd.writesize;
		break;
	case SPARE_ONLY:
		ecc = EC_MDSD;
		len = amb_mtd->mtd.oobsize;
		break;
	case SPARE_ECC:
		ecc = EC_MDSE;
		len = amb_mtd->mtd.oobsize;
		break;
	default:
		dev_err(nand_info->dev, "%s: Wrong area.\n", __func__);
		errorCode = -EINVAL;
		goto nand_amb_read_page_exit;
		break;
	}

	amb_nand->nxact.cmd = NAND_AMB_CMD_READ;
	amb_nand->nxact.arg.addr_hi = addr_hi;
	amb_nand->nxact.arg.addr = addr;
	amb_nand->nxact.arg.buf = buf;
	amb_nand->nxact.arg.len = len;
	amb_nand->nxact.arg.area = area;
	amb_nand->nxact.arg.ecc = ecc;
	amb_nand->nxact.arg.intlve = 1;

	errorCode = nand_amb_request(amb_nand);

nand_amb_read_page_exit:
	return errorCode;
}

/* ==========================================================================*/
static uint8_t amb_nand_read_byte(struct mtd_info *mtd)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	uint8_t					*data;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;

	data = amb_nand->dmabuf + amb_nand->dma_bufpos;
	amb_nand->dma_bufpos++;

	return *data;
}

static u16 amb_nand_read_word(struct mtd_info *mtd)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u16					*data;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;

	data = (u16 *)(amb_nand->dmabuf + amb_nand->dma_bufpos);
	amb_nand->dma_bufpos += 2;

	return *data;
}

static void amb_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;

	if ((amb_nand->dma_bufpos >= AMBARELLA_NAND_DMA_BUFFER_SIZE) ||
		((amb_nand->dma_bufpos + len) > AMBARELLA_NAND_DMA_BUFFER_SIZE))
		BUG();

	memcpy(buf, amb_nand->dmabuf + amb_nand->dma_bufpos, len);
	amb_nand->dma_bufpos += len;
}

static void amb_nand_write_buf(struct mtd_info *mtd,
	const uint8_t *buf, int len)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;

	if ((amb_nand->dma_bufpos >= AMBARELLA_NAND_DMA_BUFFER_SIZE) ||
		((amb_nand->dma_bufpos + len) > AMBARELLA_NAND_DMA_BUFFER_SIZE))
		BUG();

	memcpy(amb_nand->dmabuf + amb_nand->dma_bufpos, buf, len);
	amb_nand->dma_bufpos += len;
}

static int amb_nand_verify_buf(struct mtd_info *mtd,
	const uint8_t *buf, int len)
{
	BUG();

	return 0;
}

static void amb_nand_select_chip(struct mtd_info *mtd, int chip)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;
	amb_nand->bank = chip;
}

static void amb_nand_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	BUG();
}

static int amb_nand_dev_ready(struct mtd_info *mtd)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u32					status;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;

	errorCode = nand_amb_read_status(nand_info, amb_nand->bank, &status);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: failed %d.\n",
			__func__, errorCode);
		return 0;
	}

	return (status & NAND_STATUS_READY) ? 1 : 0;
}

static void amb_nand_cmdfunc(struct mtd_info *mtd, unsigned command,
	int column, int page_addr)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u32					status;
	u32					id;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;

	/* Emulate NAND_CMD_READOOB on large-page chips */
	if (mtd->writesize > 512 && command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		command = NAND_CMD_READ0;
	}

	if (command == NAND_CMD_STATUS) {
		nand_amb_read_status(nand_info, amb_nand->bank, &status);
		amb_nand->dmabuf[0] = status;
		amb_nand->dma_bufpos = 0;
	} else if (command == NAND_CMD_READID) {
		nand_amb_read_id(nand_info, amb_nand->bank, &id);
		amb_nand->dmabuf[0] = (unsigned char) (id >> 24);
		amb_nand->dmabuf[1] = (unsigned char) (id >> 16);
		amb_nand->dmabuf[2] = (unsigned char) (id >> 8);
		amb_nand->dmabuf[3] = (unsigned char) id;
		amb_nand->dma_bufpos = 0;
	} else if (command == NAND_CMD_ERASE1) {
		nand_amb_erase(nand_info, page_addr);
	} else {
		amb_nand->cmd_arg.command = command;
		amb_nand->cmd_arg.column = column;
		amb_nand->cmd_arg.page_addr = page_addr;
	}
}

static int amb_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
	const uint8_t *buf, int page, int cached, int raw)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u32					status;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, 0x00, page);

	if (raw)
		chip->ecc.write_page_raw(mtd, chip, buf);
	else
		chip->ecc.write_page(mtd, chip, buf);

	errorCode = nand_amb_read_status(nand_info, amb_nand->bank, &status);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: failed %d.\n",
			__func__, errorCode);
		goto amb_nand_write_page_exit;
	}

	if (status & NAND_STATUS_FAIL) {
		dev_err(nand_info->dev, "%s: status failed %d.\n",
			__func__, status);
		errorCode = -EIO;
	}

amb_nand_write_page_exit:
	return errorCode;
}

static void amb_nand_hwctl(struct mtd_info *mtd, int mode)
{
	BUG();
}

static int amb_nand_caculate_ecc(struct mtd_info *mtd,
	const u_char *dat, u_char *ecc_code)
{
	BUG();

	return 0;
}

static int amb_nand_correct_data(struct mtd_info *mtd, u_char *dat,
	u_char *read_ecc, u_char *calc_ecc)
{
	BUG();

	return 0;
}

static void amb_nand_write_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u8					*dma_buf;
	u32					page_addr;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;
	dma_buf = (u8 *)amb_nand->dmaaddr;
	page_addr = amb_nand->cmd_arg.page_addr;

	amb_nand->dma_bufpos = 0;
	chip->write_buf(mtd, buf, mtd->writesize);
	errorCode = nand_amb_write_page(nand_info, page_addr,
		dma_buf, MAIN_ONLY);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: failed %d.\n",
			__func__, errorCode);
		return;
	}

	amb_nand->dma_bufpos = 0;
	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
	errorCode = nand_amb_write_page(nand_info, page_addr,
		dma_buf, SPARE_ONLY);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: failed %d.\n",
			__func__, errorCode);
	}
}

static void amb_nand_write_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u8					*dma_buf;
	u32					page_addr;
	int					i;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;
	dma_buf = (u8 *)amb_nand->dmaaddr;
	page_addr = amb_nand->cmd_arg.page_addr;

	amb_nand->dma_bufpos = 0;
	chip->write_buf(mtd, buf, mtd->writesize);
	errorCode = nand_amb_write_page(nand_info, page_addr,
		dma_buf, MAIN_ECC);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: MAIN_ECC failed %d.\n",
			__func__, errorCode);
		return;
	}

	amb_nand->dma_bufpos = 0;
	for (i = 0; i < chip->ecc.layout->eccbytes; i++)
		chip->oob_poi[chip->ecc.layout->eccpos[i]] = 0xFF;
	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
	errorCode = nand_amb_write_page(nand_info, page_addr,
		dma_buf, SPARE_ECC);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: SPARE_ECC failed %d.\n",
			__func__, errorCode);
	}
}

static int amb_nand_write_oob_std(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u8					*dma_buf;
	int					i;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;
	dma_buf = (u8 *)amb_nand->dmaaddr;

	amb_nand->dma_bufpos = 0;
	for (i = 0; i < chip->ecc.layout->eccbytes; i++)
		chip->oob_poi[chip->ecc.layout->eccpos[i]] = 0xFF;
	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);

	errorCode = nand_amb_write_page(nand_info, page,
		dma_buf, SPARE_ECC);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: failed %d.\n",
			__func__, errorCode);
	}

	return errorCode;
}

static int amb_nand_read_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u8					*dma_buf;
	u32					page_addr;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;
	dma_buf = (u8 *)amb_nand->dmaaddr;
	page_addr = amb_nand->cmd_arg.page_addr;

	errorCode = nand_amb_read_page(nand_info, page_addr,
		dma_buf, MAIN_ONLY);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: failed %d.\n",
			__func__, errorCode);
		goto amb_nand_read_page_raw_exit;
	} else {
		amb_nand->dma_bufpos = 0;
		chip->read_buf(mtd, buf, mtd->writesize);
	}

	errorCode = nand_amb_read_page(nand_info, page_addr,
		dma_buf, SPARE_ONLY);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: failed %d.\n",
			__func__, errorCode);
	} else {
		amb_nand->dma_bufpos = 0;
		chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);
	}

amb_nand_read_page_raw_exit:
	return errorCode;
}

static int amb_nand_read_page_hwecc(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u8					*dma_buf;
	u32					page_addr;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;
	dma_buf = (u8 *)amb_nand->dmaaddr;
	page_addr = amb_nand->cmd_arg.page_addr;

	errorCode = nand_amb_read_page(nand_info, page_addr,
		dma_buf, MAIN_ECC);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: MAIN_ECC failed %d.\n",
			__func__, errorCode);
		mtd->ecc_stats.failed++;
		goto amb_nand_read_page_hwecc_exit;
	} else {
		amb_nand->dma_bufpos = 0;
		chip->read_buf(mtd, buf, mtd->writesize);
	}

	errorCode = nand_amb_read_page(nand_info, page_addr,
		dma_buf, SPARE_ECC);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: SPARE_ECC failed %d.\n",
			__func__, errorCode);
	} else {
		amb_nand->dma_bufpos = 0;
		chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);
	}

amb_nand_read_page_hwecc_exit:
	return errorCode;
}

static int amb_nand_read_oob_std(struct mtd_info *mtd,
	struct nand_chip *chip, int page, int sndcmd)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	u8					*dma_buf;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;
	dma_buf = (u8 *)amb_nand->dmaaddr;

	errorCode = nand_amb_read_page(nand_info, page,
		dma_buf, SPARE_ECC);
	if (errorCode) {
		dev_err(nand_info->dev, "%s: page %d failed %d.\n",
			__func__, page, errorCode);
	} else {
		amb_nand->dma_bufpos = 0;
		chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);
	}

	return errorCode;
}

static int amb_nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct ambarella_nand_info		*nand_info;
	struct nand_amb_s			*amb_nand;
	struct nand_chip			*chip;
	u8					*dma_buf;
	int					page;

	nand_info = amb_nand_info_by_minfo(mtd);
	amb_nand = &nand_info->amb_nand;
	chip = &nand_info->amb_mtd->chip;
	dma_buf = (u8 *)amb_nand->dmaaddr;

	page = (int)(ofs >> chip->page_shift);
	dev_info(nand_info->dev, "%s: page = 0x%x\n", __func__, page);
	page &= chip->pagemask;
	dev_info(nand_info->dev, "%s: real page = 0x%x\n", __func__, page);

	memset(dma_buf, 0, mtd->oobsize);

	return nand_amb_write_page(nand_info, page, dma_buf, SPARE_ONLY);
}

/* ==========================================================================*/
static void amb_nand_set_timing(struct ambarella_nand_info *nand_info,
	struct ambarella_nand_timing *timing)
{
	amba_writel(nand_info->regbase + FLASH_TIM0_OFFSET, timing->timing0);
	amba_writel(nand_info->regbase + FLASH_TIM1_OFFSET, timing->timing1);
	amba_writel(nand_info->regbase + FLASH_TIM2_OFFSET, timing->timing2);
	amba_writel(nand_info->regbase + FLASH_TIM3_OFFSET, timing->timing3);
	amba_writel(nand_info->regbase + FLASH_TIM4_OFFSET, timing->timing4);
	amba_writel(nand_info->regbase + FLASH_TIM5_OFFSET, timing->timing5);
}

static int __devinit ambarella_nand_init_chip(
	struct ambarella_nand_info *nand_info,
	struct ambarella_nand_mtd *amb_mtd,
	struct ambarella_nand_set *set)
{
	struct nand_chip *chip = &amb_mtd->chip;

	chip->read_byte = amb_nand_read_byte;
	chip->read_word = amb_nand_read_word;
	chip->write_buf = amb_nand_write_buf;
	chip->read_buf = amb_nand_read_buf;
	chip->verify_buf = amb_nand_verify_buf;
	chip->select_chip = amb_nand_select_chip;
	chip->cmd_ctrl = amb_nand_cmd_ctrl;
	chip->dev_ready = amb_nand_dev_ready;
	chip->cmdfunc = amb_nand_cmdfunc;
	chip->write_page = amb_nand_write_page;

	chip->priv = amb_mtd;
	chip->options = NAND_NO_AUTOINCR; 
	chip->block_markbad = amb_nand_block_markbad; 
	chip->controller = &nand_info->controller;

	amb_mtd->nand_info = nand_info;
	amb_mtd->mtd.priv = chip;
	amb_mtd->mtd.owner = THIS_MODULE;
	amb_mtd->set = set;

	return 0;
}

static int __devinit ambarella_nand_init_chipecc(
	struct ambarella_nand_info *nand_info,
	struct ambarella_nand_mtd *amb_mtd)
{
	struct nand_chip *chip = &amb_mtd->chip;
	struct mtd_info	*mtd = &amb_mtd->mtd;

	if (mtd->writesize == 2048) {
		chip->ecc.layout = &amb_oobinfo_2048;
		chip->badblock_pattern = &amb_2048_bbt_descr;
	} else if (mtd->writesize == 512) {
		chip->ecc.layout = &amb_oobinfo_512;
		chip->badblock_pattern = &amb_512_bbt_descr;
	} else {
		dev_err(nand_info->dev,
			"Unexpected NAND flash writesize %d. Aborting\n",
			mtd->writesize);
		return -1;
	}

	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.size = mtd->writesize;
	chip->ecc.bytes = 14;
	chip->ecc.hwctl = amb_nand_hwctl;
	chip->ecc.calculate = amb_nand_caculate_ecc;
	chip->ecc.correct = amb_nand_correct_data;
	chip->ecc.write_page = amb_nand_write_page_hwecc;
	chip->ecc.write_page_raw = amb_nand_write_page_raw;
	chip->ecc.write_oob = amb_nand_write_oob_std;
	chip->ecc.read_page = amb_nand_read_page_hwecc;
	chip->ecc.read_page_raw = amb_nand_read_page_raw;
	chip->ecc.read_oob = amb_nand_read_oob_std;

	return 0;
}

#ifdef CONFIG_MTD_PARTITIONS
static int __devinit ambarella_nand_add_partition(struct ambarella_nand_info *nand_info,
	struct ambarella_nand_mtd *amb_mtd, struct ambarella_nand_set *set)
{
	if (set == NULL)
		return add_mtd_device(&amb_mtd->mtd);

	if (set->nr_partitions > 0 && set->partitions != NULL)
		return add_mtd_partitions(&amb_mtd->mtd,
			set->partitions, set->nr_partitions);

	return add_mtd_device(&amb_mtd->mtd);
}
#else
static int __devinit ambarella_nand_add_partition(struct ambarella_nand_info *nand_info,
	struct ambarella_nand_mtd *amb_mtd, struct ambarella_nand_set *set)
{
	return add_mtd_device(&amb_mtd->mtd);
}
#endif

static int __devinit ambarella_nand_probe(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct ambarella_platform_nand		*plat_nand;
	struct ambarella_nand_info		*nand_info;
	struct ambarella_nand_mtd		*amb_mtd;
	struct ambarella_nand_set		*set;
	struct nand_amb_s			*amb_nand;
	int					i;
	struct resource				*wp_res;
	struct resource				*reg_res;
	struct resource				*dma_res;

	plat_nand = (struct ambarella_platform_nand *)pdev->dev.platform_data;
	if (plat_nand == NULL) {
		dev_err(&pdev->dev, "Can't get platform_data!\n");
		errorCode = - EPERM;
		goto ambarella_nand_probe_exit;
	}

	nand_info = kzalloc(sizeof(struct ambarella_nand_info), GFP_KERNEL);
	if (nand_info == NULL) {
		dev_err(&pdev->dev, "kzalloc for nand nand_info failed!\n");
		errorCode = - ENOMEM;
		goto ambarella_nand_probe_exit;
	}

	reg_res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "registers");
	if (reg_res == NULL) {
		dev_err(&pdev->dev, "Get reg_res failed!\n");
		errorCode = -ENXIO;
		goto ambarella_nand_probe_free_info;
	}
	nand_info->regbase = (unsigned char __iomem *)reg_res->start;

	dma_res = platform_get_resource_byname(pdev,
		IORESOURCE_MEM, "dma");
	if (dma_res == NULL) {
		dev_err(&pdev->dev, "Get dma_res failed!\n");
		errorCode = -ENXIO;
		goto ambarella_nand_probe_free_info;
	}
	nand_info->dmabase = dma_res->start;

	wp_res = platform_get_resource_byname(pdev,
		IORESOURCE_IO, "wp_gpio");
	if (wp_res == NULL) {
		nand_info->wp_gpio = -1;
	} else {
		nand_info->wp_gpio = wp_res->start;

		errorCode = gpio_request(nand_info->wp_gpio, pdev->name);
		if (errorCode < 0) {
			dev_err(&pdev->dev, "Could not get WP GPIO %d\n",
				nand_info->wp_gpio);
			goto ambarella_nand_probe_free_info;
		}

		errorCode = gpio_direction_output(nand_info->wp_gpio,
			GPIO_HIGH);
		if (errorCode < 0) {
			dev_err(&pdev->dev, "Could not Set WP GPIO %d\n",
				nand_info->wp_gpio);
			goto ambarella_nand_probe_free_wp_gpio;
		}
	}

	nand_info->cmd_irq = platform_get_irq_byname(pdev, "ambarella-fio-cmd");
	if (nand_info->cmd_irq <= 0) {
		dev_err(&pdev->dev, "Get cmd_irq failed!\n");
		errorCode = -ENXIO;
		goto ambarella_nand_probe_free_wp_gpio;
	}

	nand_info->dma_irq = platform_get_irq_byname(pdev, "ambarella-fio-dma");
	if (nand_info->dma_irq <= 0) {
		dev_err(&pdev->dev, "Get dma_irq failed!\n");
		errorCode = -ENXIO;
		goto ambarella_nand_probe_free_wp_gpio;
	}

	amb_nand_set_timing(nand_info, plat_nand->timing);

	nand_info->plat_nand = plat_nand;
	nand_info->dev = &pdev->dev;
	nand_info->nr_mtds = plat_nand->nr_sets;
	spin_lock_init(&nand_info->controller.lock);
	init_waitqueue_head(&nand_info->controller.wq);

	nand_info->amb_mtd =
		kzalloc(nand_info->nr_mtds * sizeof(struct ambarella_nand_mtd),
		GFP_KERNEL);
	if (nand_info->amb_mtd == NULL) {
		dev_err(&pdev->dev, "kzalloc for nand_mtd failed!\n");
		errorCode = -ENOMEM;
		goto ambarella_nand_probe_free_wp_gpio;
	}
	amb_mtd = nand_info->amb_mtd;

	amb_nand = &nand_info->amb_nand; 
	amb_nand->control = plat_nand->timing->control;
	init_waitqueue_head(&amb_nand->wq);

	amb_nand->dmabuf = dma_alloc_coherent(nand_info->dev,
		AMBARELLA_NAND_DMA_BUFFER_SIZE,
		&amb_nand->dmaaddr, GFP_KERNEL);
	if (amb_nand->dmabuf == NULL) {
		dev_err(&pdev->dev, "dma_alloc_coherent failed!\n");
		errorCode = -ENOMEM;
		goto ambarella_nand_probe_free_amb_mtd;
	}

	errorCode = request_irq(nand_info->cmd_irq, nand_fiocmd_isr_handler,
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			pdev->name, nand_info);
	if (errorCode) {
		dev_err(&pdev->dev, "Could not register IRQ %d!\n",
			nand_info->cmd_irq);
		goto ambarella_nand_probe_free_dma;
	}

	errorCode = request_irq(nand_info->dma_irq, nand_fiodma_isr_handler,
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			pdev->name, nand_info);
	if (errorCode) {
		dev_err(&pdev->dev, "Could not register IRQ %d!\n",
			nand_info->dma_irq);
		goto ambarella_nand_probe_free_cmd_irq;
	}

	errorCode = ambarella_dma_request_irq(FIO_DMA_CHAN,
		nand_dma_isr_handler, nand_info);
	if (errorCode) {
		dev_err(&pdev->dev, "Could not request DMA channel %d\n",
			FIO_DMA_CHAN);
		goto ambarella_nand_probe_free_dma_irq;
	}

	errorCode = ambarella_dma_enable_irq(FIO_DMA_CHAN,
		nand_dma_isr_handler);
	if (errorCode) {
		dev_err(&pdev->dev, "Could not enable DMA channel %d\n",
			FIO_DMA_CHAN);
		goto ambarella_nand_probe_free_dma_chan;
	}

	set = plat_nand->sets;
	for (i = 0; i < nand_info->nr_mtds; i++) {
		ambarella_nand_init_chip(nand_info, amb_mtd, set);

		errorCode = nand_scan_ident(&amb_mtd->mtd, set->nr_chips);
		if (errorCode)
			continue;

		errorCode = ambarella_nand_init_chipecc(nand_info, amb_mtd);
		if (errorCode)
			goto ambarella_nand_probe_mtd_error;

		errorCode = nand_scan_tail(&amb_mtd->mtd);
		if (errorCode)
			goto ambarella_nand_probe_mtd_error;

		ambarella_nand_add_partition(nand_info, amb_mtd, set);

		set++;
		amb_mtd++;
	}
	platform_set_drvdata(pdev, nand_info);

#ifdef CONFIG_CPU_FREQ
	nand_info->origin_clk = get_ahb_bus_freq_hz() / 1000; /* in Khz */
	nand_info->origin_timing = plat_nand->timing;
	nand_info->nand_freq_transition.notifier_call = ambnand_freq_transition;
	cpufreq_register_notifier(&nand_info->nand_freq_transition,
		CPUFREQ_TRANSITION_NOTIFIER);
#endif

	goto ambarella_nand_probe_exit;

ambarella_nand_probe_mtd_error:
	ambarella_dma_disable_irq(FIO_DMA_CHAN, nand_dma_isr_handler);

ambarella_nand_probe_free_dma_chan:
	ambarella_dma_free_irq(FIO_DMA_CHAN, nand_dma_isr_handler);

ambarella_nand_probe_free_dma_irq:
	free_irq(nand_info->dma_irq, nand_info);

ambarella_nand_probe_free_cmd_irq:
	free_irq(nand_info->cmd_irq, nand_info);

ambarella_nand_probe_free_dma:
	dma_free_coherent(nand_info->dev,
		AMBARELLA_NAND_DMA_BUFFER_SIZE,
		amb_nand->dmabuf, amb_nand->dmaaddr);

ambarella_nand_probe_free_amb_mtd:
	kfree(nand_info->amb_mtd);

ambarella_nand_probe_free_wp_gpio:
	if (nand_info->wp_gpio >= 0)
		gpio_free(nand_info->wp_gpio);

ambarella_nand_probe_free_info:
	kfree(nand_info);

ambarella_nand_probe_exit:

	return errorCode;
}

static int __devexit ambarella_nand_remove(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;
	struct ambarella_nand_mtd		*amb_mtd;
	struct nand_amb_s			*amb_nand;
	int					i;

	nand_info = (struct ambarella_nand_info *)platform_get_drvdata(pdev);

	if (nand_info) {
		platform_set_drvdata(pdev, NULL);

		errorCode = ambarella_dma_disable_irq(FIO_DMA_CHAN,
			nand_dma_isr_handler);
		ambarella_dma_free_irq(FIO_DMA_CHAN, nand_dma_isr_handler);
		free_irq(nand_info->dma_irq, nand_info);
		free_irq(nand_info->cmd_irq, nand_info);

		amb_nand = &nand_info->amb_nand; 
		dma_free_coherent(nand_info->dev,
			AMBARELLA_NAND_DMA_BUFFER_SIZE,
			amb_nand->dmabuf, amb_nand->dmaaddr);

		amb_mtd = nand_info->amb_mtd;
		for (i = 0; i < nand_info->nr_mtds; i ++) {
			nand_release(&amb_mtd->mtd);
			amb_mtd++;
		}
		kfree(nand_info->amb_mtd);
		if (nand_info->wp_gpio >= 0) {
			gpio_direction_output(nand_info->wp_gpio, GPIO_LOW);
			gpio_free(nand_info->wp_gpio);
		}
		kfree(nand_info);
	}

	return errorCode;
}

#ifdef CONFIG_PM
static int ambarella_nand_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;

	nand_info = platform_get_drvdata(pdev);
	nand_info->suspend = 1;

	dev_info(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);

	return errorCode;
}

static int ambarella_nand_resume(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct ambarella_nand_info		*nand_info;

	nand_info = platform_get_drvdata(pdev);
	nand_info->suspend = 0;

	dev_info(&pdev->dev, "%s exit with %d\n", __func__, errorCode);

	return errorCode;
}
#endif

static struct platform_driver amb_nand_driver = {
	.probe		= ambarella_nand_probe,
	.remove		= ambarella_nand_remove,
#ifdef CONFIG_PM
	.suspend	= ambarella_nand_suspend,
	.resume		= ambarella_nand_resume,
#endif
	.driver = {
		.name	= "ambarella-nand",
		.owner	= THIS_MODULE,
	},
};

static int __init ambarella_nand_init(void)
{
	return platform_driver_register(&amb_nand_driver);
}

static void __exit ambarella_nand_exit(void)
{
	platform_driver_unregister(&amb_nand_driver);
}

module_init(ambarella_nand_init);
module_exit(ambarella_nand_exit);

MODULE_AUTHOR("Cao Rongrong & Chien-Yang Chen");
MODULE_DESCRIPTION("Ambarella Media processor NAND Controller Driver");
MODULE_LICENSE("GPL");

