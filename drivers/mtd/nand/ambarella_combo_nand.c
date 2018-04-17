/*
 * drivers/mtd/ambarella_combo_nand.c
 *
 * History:
 *	2017/05/13 - [Cao Rongrong] created file
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
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/bitrev.h>
#include <linux/bch.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <plat/rct.h>
#include <plat/nand_combo.h>
#include <plat/event.h>

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
#include <linux/aipc/ipc_mutex.h>
#endif

#define AMBARELLA_NAND_DMA_BUFFER_SIZE	8192


struct ambarella_nand_host {
	struct nand_chip		chip;
	struct nand_hw_control		controller;

	struct device			*dev;
	wait_queue_head_t		wq;

	void __iomem			*regbase;
	struct regmap			*reg_rct;
	int				irq;
	u32				ecc_bits;
	/* if or not support to read id in 5 cycles */
	bool				soft_ecc;
	bool				page_4k;
	bool				enable_wp;

	/* used for software BCH */
	struct bch_control		*bch;
	u32				*errloc;
	u8				*bch_data;
	u8				read_ecc_rev[13];
	u8				calc_ecc_rev[13];
	u8				soft_bch_extra_size;

	dma_addr_t			dmaaddr;
	u8				*dmabuf;
	int				dma_bufpos;
	u32				int_sts;
	u32				ecc_rpt_sts;
	u32				ecc_rpt_sts2;

	/* saved page_addr during CMD_SEQIN */
	int				seqin_page_addr;

	/* Operation parameters for nand controller register */
	int				err_code;
	u32				control_reg;
	u32				timing[6];

	struct notifier_block		system_event;
	struct semaphore		system_event_sem;

};

/* ==========================================================================*/
#define NAND_TIMING_RSHIFT24BIT(x)	(((x) & 0xff000000) >> 24)
#define NAND_TIMING_RSHIFT16BIT(x)	(((x) & 0x00ff0000) >> 16)
#define NAND_TIMING_RSHIFT8BIT(x)	(((x) & 0x0000ff00) >> 8)
#define NAND_TIMING_RSHIFT0BIT(x)	(((x) & 0x000000ff) >> 0)

#define NAND_TIMING_LSHIFT24BIT(x)	((x) << 24)
#define NAND_TIMING_LSHIFT16BIT(x)	((x) << 16)
#define NAND_TIMING_LSHIFT8BIT(x)	((x) << 8)
#define NAND_TIMING_LSHIFT0BIT(x)	((x) << 0)

#ifndef CONFIG_ARCH_AMBARELLA_AMBALINK
static int nand_timing_calc(u32 clk, int minmax, int val)
{
	u32 x;
	int n,r;

	x = val * clk;
	n = x / 1000;
	r = x % 1000;

	if (r != 0)
		n++;

	if (minmax)
		n--;

	return n < 1 ? 1 : n;
}
#endif

static int amb_ecc6_ooblayout_ecc_lp(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (section >= chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = (section * 16) + 6;
	oobregion->length = chip->ecc.bytes;

	return 0;
}

static int amb_ecc6_ooblayout_free_lp(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (section >= chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = (section * 16) + 1;
	oobregion->length = 5;

	return 0;
}

static const struct mtd_ooblayout_ops amb_ecc6_lp_ooblayout_ops = {
	.ecc = amb_ecc6_ooblayout_ecc_lp,
	.free = amb_ecc6_ooblayout_free_lp,
};

static int amb_ecc8_ooblayout_ecc_lp(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (section >= chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = (section * 32) + 19;
	oobregion->length = chip->ecc.bytes;

	return 0;
}

static int amb_ecc8_ooblayout_free_lp(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *oobregion)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (section >= chip->ecc.steps)
		return -ERANGE;

	oobregion->offset = (section * 32) + 2;;
	oobregion->length = 17;

	return 0;
}

static const struct mtd_ooblayout_ops amb_ecc8_lp_ooblayout_ops = {
	.ecc = amb_ecc8_ooblayout_ecc_lp,
	.free = amb_ecc8_ooblayout_free_lp,
};

static u32 to_native_cmd(struct ambarella_nand_host *host, u32 cmd)
{
	u32 native_cmd;

	switch (cmd) {
	case NAND_CMD_RESET:
		native_cmd = NAND_AMB_CMD_RESET;
		break;
	case NAND_CMD_READID:
		native_cmd = NAND_AMB_CMD_READID;
		break;
	case NAND_CMD_STATUS:
		native_cmd = NAND_AMB_CMD_READSTATUS;
		break;
	case NAND_CMD_ERASE1:
		native_cmd = NAND_AMB_CMD_ERASE;
		break;
	case NAND_CMD_READOOB:
	case NAND_CMD_READ0:
		native_cmd = NAND_AMB_CMD_READ;
		break;
	case NAND_CMD_PAGEPROG:
		native_cmd = NAND_AMB_CMD_PROGRAM;
		break;
	default:
		dev_err(host->dev, "Unknown command: %d\n", cmd);
		BUG();
		break;
	}

	return native_cmd;
}

#ifndef CONFIG_ARCH_AMBARELLA_AMBALINK
static inline void ambarella_fio_rct_reset(struct ambarella_nand_host *host)
{
	regmap_write(host->reg_rct, FIO_RESET_OFFSET, FIO_RESET_FIO_RST);
	msleep(1);
	regmap_write(host->reg_rct, FIO_RESET_OFFSET, 0);
	msleep(1);
}
#endif

static int count_zero_bits(u8 *buf, int size, int max_bits)
{
	int i, zero_bits = 0;

	for (i = 0; i < size; i++) {
		zero_bits += hweight8(~buf[i]);
		if (zero_bits > max_bits)
			break;
	}

	return zero_bits;
}

static int nand_bch_check_blank_page(struct ambarella_nand_host *host)
{
	struct nand_chip *chip = &host->chip;
	struct mtd_info	*mtd = nand_to_mtd(chip);
	int eccsteps = chip->ecc.steps, zero_bits = 0, zeroflip = 0, oob_subset;
	u8 *bufpos, *bsp;
	u32 i;

	bufpos = host->dmabuf;
	bsp = host->dmabuf + mtd->writesize;
	oob_subset = mtd->oobsize / eccsteps;

	for (i = 0; i < eccsteps; i++) {
		zero_bits = count_zero_bits(bufpos, chip->ecc.size, chip->ecc.strength);
		if (zero_bits > chip->ecc.strength)
			return -1;

		if (zero_bits)
			zeroflip = 1;

		zero_bits += count_zero_bits(bsp, oob_subset, chip->ecc.strength);
		if (zero_bits > chip->ecc.strength)
			return -1;

		bufpos += chip->ecc.size;
		bsp += oob_subset;
	}

	if (zeroflip)
		memset(host->dmabuf, 0xff, mtd->writesize);

	return 0;
}

static void ambarella_nand_set_timing(struct ambarella_nand_host *host)
{
#ifndef CONFIG_ARCH_AMBARELLA_AMBALINK
	u8 tcls, tals, tcs, tds;
	u8 tclh, talh, tch, tdh;
	u8 twp, twh, twb, trr;
	u8 trp, treh, trb, tceh;
	u8 trdelay, tclr, twhr, tir;
	u8 tww, trhz, tar;
	u32 i, t, clk, val;

	for (i = 0; i < ARRAY_SIZE(host->timing); i++) {
		if (host->timing[i] != 0x0)
			break;
	}
	/* if the timing is not setup by Amboot, we leave the timing unchanged */
	if (i == ARRAY_SIZE(host->timing))
		return;

	clk = (clk_get_rate(clk_get(host->dev, NULL)) / 1000000);

	/* timing 0 */
	t = host->timing[0];
	tcls = NAND_TIMING_RSHIFT24BIT(t);
	tals = NAND_TIMING_RSHIFT16BIT(t);
	tcs = NAND_TIMING_RSHIFT8BIT(t);
	tds = NAND_TIMING_RSHIFT0BIT(t);

	tcls = nand_timing_calc(clk, 0, tcls);
	tals = nand_timing_calc(clk, 0, tals);
	tcs = nand_timing_calc(clk, 0, tcs);
	tds = nand_timing_calc(clk, 0, tds);

	val = NAND_TIMING_LSHIFT24BIT(tcls) |
		NAND_TIMING_LSHIFT16BIT(tals) |
		NAND_TIMING_LSHIFT8BIT(tcs) |
		NAND_TIMING_LSHIFT0BIT(tds);

	writel_relaxed(val, host->regbase + NAND_TIMING0_OFFSET);

	/* timing 1 */
	t = host->timing[1];
	tclh = NAND_TIMING_RSHIFT24BIT(t);
	talh = NAND_TIMING_RSHIFT16BIT(t);
	tch = NAND_TIMING_RSHIFT8BIT(t);
	tdh = NAND_TIMING_RSHIFT0BIT(t);

	tclh = nand_timing_calc(clk, 0, tclh);
	talh = nand_timing_calc(clk, 0, talh);
	tch = nand_timing_calc(clk, 0, tch);
	tdh = nand_timing_calc(clk, 0, tdh);

	val = NAND_TIMING_LSHIFT24BIT(tclh) |
		NAND_TIMING_LSHIFT16BIT(talh) |
		NAND_TIMING_LSHIFT8BIT(tch) |
		NAND_TIMING_LSHIFT0BIT(tdh);

	writel_relaxed(val, host->regbase + NAND_TIMING1_OFFSET);

	/* timing 2 */
	t = host->timing[2];
	twp = NAND_TIMING_RSHIFT24BIT(t);
	twh = NAND_TIMING_RSHIFT16BIT(t);
	twb = NAND_TIMING_RSHIFT8BIT(t);
	trr = NAND_TIMING_RSHIFT0BIT(t);

	twp = nand_timing_calc(clk, 0, twp);
	twh = nand_timing_calc(clk, 0, twh);
	twb = nand_timing_calc(clk, 1, twb);
	trr = nand_timing_calc(clk, 0, trr);

	val = NAND_TIMING_LSHIFT24BIT(twp) |
		NAND_TIMING_LSHIFT16BIT(twh) |
		NAND_TIMING_LSHIFT8BIT(twb) |
		NAND_TIMING_LSHIFT0BIT(trr);

	writel_relaxed(val, host->regbase + NAND_TIMING2_OFFSET);

	/* timing 3 */
	t = host->timing[3];
	trp = NAND_TIMING_RSHIFT24BIT(t);
	treh = NAND_TIMING_RSHIFT16BIT(t);
	trb = NAND_TIMING_RSHIFT8BIT(t);
	tceh = NAND_TIMING_RSHIFT0BIT(t);

	trp = nand_timing_calc(clk, 0, trp);
	treh = nand_timing_calc(clk, 0, treh);
	trb = nand_timing_calc(clk, 1, trb);
	tceh = nand_timing_calc(clk, 1, tceh);

	val = NAND_TIMING_LSHIFT24BIT(trp) |
		NAND_TIMING_LSHIFT16BIT(treh) |
		NAND_TIMING_LSHIFT8BIT(trb) |
		NAND_TIMING_LSHIFT0BIT(tceh);

	writel_relaxed(val, host->regbase + NAND_TIMING3_OFFSET);

	/* timing 4 */
	t = host->timing[4];
	trdelay = NAND_TIMING_RSHIFT24BIT(t);
	tclr = NAND_TIMING_RSHIFT16BIT(t);
	twhr = NAND_TIMING_RSHIFT8BIT(t);
	tir = NAND_TIMING_RSHIFT0BIT(t);

	trdelay = trp + treh;
	tclr = nand_timing_calc(clk, 0, tclr);
	twhr = nand_timing_calc(clk, 0, twhr);
	tir = nand_timing_calc(clk, 0, tir);

	val = NAND_TIMING_LSHIFT24BIT(trdelay) |
		NAND_TIMING_LSHIFT16BIT(tclr) |
		NAND_TIMING_LSHIFT8BIT(twhr) |
		NAND_TIMING_LSHIFT0BIT(tir);

	writel_relaxed(val, host->regbase + NAND_TIMING4_OFFSET);

	/* timing 5 */
	t = host->timing[5];
	tww = NAND_TIMING_RSHIFT16BIT(t);
	trhz = NAND_TIMING_RSHIFT8BIT(t);
	tar = NAND_TIMING_RSHIFT0BIT(t);

	tww = nand_timing_calc(clk, 0, tww);
	trhz = nand_timing_calc(clk, 1, trhz);
	tar = nand_timing_calc(clk, 0, tar);


	val = NAND_TIMING_LSHIFT16BIT(tww) |
		NAND_TIMING_LSHIFT8BIT(trhz) |
		NAND_TIMING_LSHIFT0BIT(tar);

	writel_relaxed(val, host->regbase + NAND_TIMING5_OFFSET);
#endif
}

static int ambarella_nand_system_event(struct notifier_block *nb,
	unsigned long val, void *data)
{
	struct ambarella_nand_host *host;
	int rval = NOTIFY_OK;

	host = container_of(nb, struct ambarella_nand_host, system_event);

	switch (val) {
	case AMBA_EVENT_PRE_CPUFREQ:
		pr_debug("%s: Pre Change\n", __func__);
		down(&host->system_event_sem);
		break;

	case AMBA_EVENT_POST_CPUFREQ:
		pr_debug("%s: Post Change\n", __func__);
		ambarella_nand_set_timing(host);
		up(&host->system_event_sem);
		break;

	default:
		break;
	}

	return rval;
}

static irqreturn_t ambarella_nand_isr_handler(int irq, void *dev_id)
{
	struct ambarella_nand_host *host = dev_id;
	u32 int_sts;

	int_sts = readl_relaxed(host->regbase + FIO_INT_STATUS_OFFSET);
	int_sts &= FIO_INT_OPERATION_DONE | FIO_INT_ECC_RPT_UNCORR | FIO_INT_ECC_RPT_THRESH;
	if (int_sts) {
		writel_relaxed(int_sts, host->regbase + FIO_RAW_INT_STATUS_OFFSET);
		host->int_sts = int_sts;
		host->ecc_rpt_sts =
			readl_relaxed(host->regbase + FIO_ECC_RPT_STATUS_OFFSET);
		host->ecc_rpt_sts2 =
			readl_relaxed(host->regbase + FIO_ECC_RPT_STATUS2_OFFSET);
		wake_up(&host->wq);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static void ambarella_nand_setup_dma(struct ambarella_nand_host *host, u32 cmd)
{
	struct mtd_info *mtd = nand_to_mtd(&host->chip);
	u32 dmaaddr, fdma_ctrl;

	/* Setup FDMA engine transfer */
	dmaaddr = host->dmaaddr;
	writel_relaxed(dmaaddr, host->regbase + FDMA_MN_MEM_ADDR_OFFSET);
	dmaaddr = host->dmaaddr + mtd->writesize;
	writel_relaxed(dmaaddr, host->regbase + FDMA_SP_MEM_ADDR_OFFSET);

	fdma_ctrl = cmd == NAND_AMB_CMD_READ ? FDMA_CTRL_WRITE_MEM : FDMA_CTRL_READ_MEM;
	fdma_ctrl |= FDMA_CTRL_ENABLE | FDMA_CTRL_BLK_SIZE_512B;
	fdma_ctrl |= mtd->writesize + mtd->oobsize;
	writel_relaxed(fdma_ctrl, host->regbase + FDMA_MN_CTRL_OFFSET);
}

static void ambarella_nand_setup_cc(struct ambarella_nand_host *host, u32 cmd)
{
	u32 val;

	/* currently only support ERASE custom command */
	BUG_ON(cmd != NAND_AMB_CMD_ERASE);

	val = NAND_CC_WORD_CMD1VAL0(0x60) | NAND_CC_WORD_CMD2VAL0(0xD0);
	writel_relaxed(val, host->regbase + NAND_CC_WORD_OFFSET);

	val = NAND_CC_DATA_CYCLE(5) | NAND_CC_WAIT_RB | NAND_CC_RW_NODATA |
		NAND_CC_CMD2(1) | NAND_CC_ADDR_CYCLE(3) | NAND_CC_CMD1(1) |
		NAND_CC_ADDR_SRC(1) | NAND_CC_DATA_SRC_REGISTER | NAND_CC_TERMINATE_CE;
	writel_relaxed(val, host->regbase + NAND_CC_OFFSET);
}

static int ambarella_nand_issue_cmd(struct ambarella_nand_host *host,
		u32 cmd, u32 page_addr)
{
	struct mtd_info *mtd = nand_to_mtd(&host->chip);
	u64 addr64 = (u64)(page_addr * mtd->writesize);
	u32 native_cmd = to_native_cmd(host, cmd);
	u32 nand_ctrl,  nand_cmd;
	long timeout;
	int rval = 0;

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
        aipc_mutex_lock(AMBA_IPC_MUTEX_NAND);
        enable_irq(host->irq);
#endif
	host->int_sts = 0;

	if (native_cmd == NAND_AMB_CMD_READ || native_cmd == NAND_AMB_CMD_PROGRAM)
		ambarella_nand_setup_dma(host, native_cmd);

	nand_ctrl = host->control_reg | NAND_CTRL_A33_32((u32)(addr64 >> 32));
	nand_cmd = (u32)addr64 | NAND_CMD_CMD(native_cmd);
	writel_relaxed(nand_ctrl, host->regbase + NAND_CTRL_OFFSET);
	writel_relaxed(nand_cmd, host->regbase + NAND_CMD_OFFSET);

	if (NAND_CMD_IS_CC(native_cmd))
		ambarella_nand_setup_cc(host, native_cmd);

	/* now waiting for command completed */
	timeout = wait_event_timeout(host->wq, host->int_sts, 1 * HZ);
	if (timeout <= 0) {
		rval = -EBUSY;
		dev_err(host->dev, "cmd=0x%x timeout\n", native_cmd);
	}

	/* avoid to flush previous error info */
	if (host->err_code == 0)
		host->err_code = rval;

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
        disable_irq(host->irq);
        aipc_mutex_unlock(AMBA_IPC_MUTEX_NAND);
#endif
	return rval;
}


/* ==========================================================================*/
static uint8_t ambarella_nand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct ambarella_nand_host *host = nand_get_controller_data(chip);
	uint8_t *data;

	data = host->dmabuf + host->dma_bufpos;
	host->dma_bufpos++;

	return *data;
}

static u16 ambarella_nand_read_word(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct ambarella_nand_host *host = nand_get_controller_data(chip);
	u16 *data;

	data = (u16 *)(host->dmabuf + host->dma_bufpos);
	host->dma_bufpos += 2;

	return *data;
}

static void ambarella_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct ambarella_nand_host *host = nand_get_controller_data(chip);

	BUG_ON((host->dma_bufpos + len) > AMBARELLA_NAND_DMA_BUFFER_SIZE);

	memcpy(buf, host->dmabuf + host->dma_bufpos, len);
	host->dma_bufpos += len;
}

static void ambarella_nand_write_buf(struct mtd_info *mtd,
	const uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct ambarella_nand_host *host = nand_get_controller_data(chip);

	BUG_ON((host->dma_bufpos + len) > AMBARELLA_NAND_DMA_BUFFER_SIZE);

	memcpy(host->dmabuf + host->dma_bufpos, buf, len);
	host->dma_bufpos += len;
}

static void ambarella_nand_select_chip(struct mtd_info *mtd, int chip)
{
	struct nand_chip *nand = mtd_to_nand(mtd);
	struct ambarella_nand_host *host = nand_get_controller_data(nand);

	if (chip > 0)
		dev_err(host->dev, "Multi-Chip isn't supported yet.\n");
}

static void ambarella_nand_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
}

static int ambarella_nand_dev_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);

	return (chip->read_byte(mtd) & NAND_STATUS_READY) ? 1 : 0;
}

static int ambarella_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct ambarella_nand_host *host = nand_get_controller_data(chip);
	int status = 0;

	/*
	 * ambarella nand controller has waited for the command completion,
	 * but still need to check the nand chip's status
	 */
	if (host->err_code)
		status = NAND_STATUS_FAIL;
	else {
		chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);
		status = chip->read_byte(mtd);
	}

	return status;
}

static void ambarella_nand_cmdfunc(struct mtd_info *mtd, unsigned cmd,
	int column, int page_addr)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct ambarella_nand_host *host = nand_get_controller_data(chip);
	u32 val, *id;

	host->err_code = 0;

	switch(cmd) {
	case NAND_CMD_ERASE2:
		break;

	case NAND_CMD_SEQIN:
		host->dma_bufpos = column;
		host->seqin_page_addr = page_addr;
		break;

	case NAND_CMD_RESET:
	case NAND_CMD_READID:
	case NAND_CMD_STATUS:
		host->dma_bufpos = 0;
		ambarella_nand_issue_cmd(host, cmd, 0);
		break;

	case NAND_CMD_READOOB:
	case NAND_CMD_READ0:
		host->dma_bufpos = (cmd == NAND_CMD_READ0) ? column : mtd->writesize;
		ambarella_nand_issue_cmd(host, cmd, page_addr);
		break;

	case NAND_CMD_PAGEPROG:
		page_addr = host->seqin_page_addr;
	case NAND_CMD_ERASE1:
		ambarella_nand_issue_cmd(host, cmd, page_addr);
		break;

	default:
		dev_err(host->dev, "%s: 0x%x, %d, %d\n",
				__func__, cmd, column, page_addr);
		BUG();
		break;
	}

	switch(cmd) {
	case NAND_CMD_READID:
		id = (u32 *)host->dmabuf;

		val = readl_relaxed(host->regbase + NAND_ID_OFFSET);
		*id = be32_to_cpu(val);

		val = readl_relaxed(host->regbase + NAND_EXT_ID_OFFSET);
		host->dmabuf[4] = (unsigned char)(val & 0xff);
		break;

	case NAND_CMD_STATUS:
		val = readl_relaxed(host->regbase + NAND_STATUS_OFFSET);
		host->dmabuf[0] = (unsigned char)val;
		break;

	case NAND_CMD_READOOB:
	case NAND_CMD_READ0:
		if (host->soft_ecc)
			break;

		if (host->int_sts & FIO_INT_ECC_RPT_UNCORR) {
			mtd->ecc_stats.failed++;
			dev_err(host->dev,
				"BCH corrected failed in block[%d]!\n",
				FIO_ECC_RPT_UNCORR_BLK_ADDR(host->ecc_rpt_sts2));
		} else if (host->int_sts & FIO_INT_ECC_RPT_THRESH) {
			val = FIO_ECC_RPT_MAX_ERR_NUM(host->ecc_rpt_sts);
			mtd->ecc_stats.corrected += val;
			dev_info(host->dev, "BCH correct [%d]bit in block[%d]\n",
				val, FIO_ECC_RPT_BLK_ADDR(host->ecc_rpt_sts));
		}
		break;
	}
}

static void ambarella_nand_hwctl(struct mtd_info *mtd, int mode)
{
}

static int ambarella_nand_calculate_ecc(struct mtd_info *mtd,
		const u_char *buf, u_char *code)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct ambarella_nand_host *host = nand_get_controller_data(chip);
	u32 i, amb_eccsize;

	if (!host->soft_ecc) {
		memset(code, 0xff, host->chip.ecc.bytes);
	} else {
		/* make it be compatible with hw bch */
		for (i = 0; i < host->chip.ecc.size; i++)
			host->bch_data[i] = bitrev8(buf[i]);

		memset(code, 0, host->chip.ecc.bytes);

		amb_eccsize = host->chip.ecc.size + host->soft_bch_extra_size;
		encode_bch(host->bch, host->bch_data, amb_eccsize, code);

		/* make it be compatible with hw bch */
		for (i = 0; i < host->chip.ecc.bytes; i++)
			code[i] = bitrev8(code[i]);
	}

	return 0;
}

static int ambarella_nand_correct_data(struct mtd_info *mtd, u_char *buf,
		u_char *read_ecc, u_char *calc_ecc)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct ambarella_nand_host *host = nand_get_controller_data(chip);
	u32 *errloc = host->errloc;
	int amb_eccsize, i, count;
	int rval = 0;

	/*
	 * if we use hardware ecc, any errors include DMA error and FIO DMA
	 * error, we consider it as a ecc error which will tell the caller the
	 * read is failed. We have distinguish all the errors, but the
	 * nand_read_ecc only check the return value by this function.
	 */
	if (!host->soft_ecc) {
		rval = host->err_code;
	} else {
		for (i = 0; i < chip->ecc.bytes; i++) {
			host->read_ecc_rev[i] = bitrev8(read_ecc[i]);
			host->calc_ecc_rev[i] = bitrev8(calc_ecc[i]);
		}

		amb_eccsize = chip->ecc.size + host->soft_bch_extra_size;
		count = decode_bch(host->bch, NULL,
					amb_eccsize,
					host->read_ecc_rev,
					host->calc_ecc_rev,
					NULL, errloc);
		if (count > 0) {
			for (i = 0; i < count; i++) {
				if (errloc[i] < (amb_eccsize * 8)) {
					/* error is located in data, correct it */
					buf[errloc[i] >> 3] ^= (128 >> (errloc[i] & 7));
				}
				/* else error in ecc, no action needed */

				dev_dbg(host->dev,
					"corrected bitflip %u\n", errloc[i]);
			}
		} else if (count < 0) {
			count = nand_bch_check_blank_page(host);
			if (count < 0)
				dev_err(host->dev, "ecc unrecoverable error\n");
		}

		rval = count;
	}

	return rval;
}

static int ambarella_nand_write_oob_std(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	struct ambarella_nand_host *host = nand_get_controller_data(chip);
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	int i, status, eccsteps;

	/*
	 * Our nand controller will write the generated ECC code into spare
	 * area automatically, so we should mark the ECC code which located
	 * in the eccpos.
	 */
	if (!host->soft_ecc) {
		eccsteps = chip->ecc.steps;
		for (i = 0; eccsteps; eccsteps--, i += chip->ecc.bytes) {
			chip->ecc.calculate(mtd, NULL, &ecc_calc[i]);
			status = mtd_ooblayout_set_eccbytes(mtd, ecc_calc,
					chip->oob_poi, 0, chip->ecc.total);
			if (status)
				return status;
		}
	}

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize, page);
	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);
	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

/*
 * The encoding sequence in a byte is "LSB first".
 *
 * For each 2K page, there will be 2048 byte main data (B0 ~ B2047) and 64 byte
 * spare data (B2048 ~ B2111). Thus, each page is divided into 4 BCH blocks.
 * For example, B0~B511 and B2048~B2063 are grouped as the first BCH block.
 * B0 will be encoded first and B2053 will be encoded last.
 *
 * B2054 ~B2063 are used to store 10B parity data (precisely to say, 78 bits)
 * The 2 dummy bits are filled as 0 and located at the msb of B2063.
*/
static int ambarella_nand_init_soft_bch(struct ambarella_nand_host *host)
{
	struct nand_chip *chip = &host->chip;
	u32 amb_eccsize, eccbytes, m, t;

	amb_eccsize = chip->ecc.size + host->soft_bch_extra_size;
	eccbytes = chip->ecc.bytes;

	m = fls(1 + 8 * amb_eccsize);
	t = (eccbytes * 8) / m;

	host->bch = init_bch(m, t, 0);
	if (!host->bch)
		return -EINVAL;

	host->errloc = devm_kzalloc(host->dev,
				t * sizeof(*host->errloc), GFP_KERNEL);
	if (!host->errloc)
		return -ENOMEM;

	host->bch_data = devm_kzalloc(host->dev,
					amb_eccsize, GFP_KERNEL);
	if (host->bch_data == NULL)
		return -ENOMEM;

	/*
	 * asumming the 6 bytes data in spare area are all 0xff, in other
	 * words, we don't support to write anything except for ECC code
	 * into spare are.
	 */
	memset(host->bch_data + chip->ecc.size,
				0xff, host->soft_bch_extra_size);

	return 0;
}

static void ambarella_nand_init_hw(struct ambarella_nand_host *host)
{
	u32 val;

	/* reset FIO by RCT */
#ifndef CONFIG_ARCH_AMBARELLA_AMBALINK
	ambarella_fio_rct_reset(host);

	/* Reset FIO FIFO and then exit random read mode */
	val = readl_relaxed(host->regbase + FIO_CTRL_OFFSET);
	val |= FIO_CTRL_RANDOM_READ;
	writel_relaxed(val, host->regbase + FIO_CTRL_OFFSET);
	/* wait for some time to make sure FIO FIFO reset is done */
	msleep(3);
	val &= ~FIO_CTRL_RANDOM_READ;
	writel_relaxed(val, host->regbase + FIO_CTRL_OFFSET);
#endif

	/* always use 5 cycles to read ID */
	val = readl_relaxed(host->regbase + NAND_EXT_CTRL_OFFSET);
	val |= NAND_EXT_CTRL_I5;
	if (host->page_4k)
		val |= NAND_EXT_CTRL_4K_PAGE;
	else
		val &= ~NAND_EXT_CTRL_4K_PAGE;
	writel_relaxed(val, host->regbase + NAND_EXT_CTRL_OFFSET);

	/* always enable dual-space mode */
#ifndef CONFIG_ARCH_AMBARELLA_AMBALINK
	if (host->ecc_bits == 6)
		val = FDMA_DSM_MAIN_JP_SIZE_512B | FDMA_DSM_SPARE_JP_SIZE_16B;
	else
		val = FDMA_DSM_MAIN_JP_SIZE_512B | FDMA_DSM_SPARE_JP_SIZE_32B;
	writel_relaxed(val, host->regbase + FDMA_DSM_CTRL_OFFSET);

	/* disable BCH if using soft ecc */
	val = readl_relaxed(host->regbase + FIO_CTRL_OFFSET);
	val |= FIO_CTRL_RDERR_STOP | FIO_CTRL_SKIP_BLANK_ECC;
	if (host->soft_ecc)
		val &= ~FIO_CTRL_ECC_BCH_ENABLE;
	else
		val |= FIO_CTRL_ECC_BCH_ENABLE;
	writel_relaxed(val, host->regbase + FIO_CTRL_OFFSET);

	ambarella_nand_set_timing(host);

	/* setup min number of correctable bits that not trigger irq. */
	val = FIO_ECC_RPT_ERR_NUM_TH(host->ecc_bits);
	writel_relaxed(val, host->regbase + FIO_ECC_RPT_CFG_OFFSET);

	/* clear and enable nand irq */
	val = readl_relaxed(host->regbase + FIO_RAW_INT_STATUS_OFFSET);
	writel_relaxed(val, host->regbase + FIO_RAW_INT_STATUS_OFFSET);
	val = FIO_INT_OPERATION_DONE | FIO_INT_ECC_RPT_UNCORR | FIO_INT_ECC_RPT_THRESH;
	writel_relaxed(val, host->regbase + FIO_INT_ENABLE_OFFSET);
#endif
}

static int ambarella_nand_config_flash(struct ambarella_nand_host *host)
{
	int					rval = 0;

	/*
	 * Calculate row address cycyle according to whether the page number
	 * of the nand is greater than 65536.
	 */
	if ((host->chip.chip_shift - host->chip.page_shift) > 16)
		host->control_reg |= NAND_CTRL_P3;
	else
		host->control_reg &= ~NAND_CTRL_P3;

	host->control_reg &= ~NAND_CTRL_SIZE_8G;
	switch (host->chip.chipsize) {
	case 8 * 1024 * 1024:
		host->control_reg |= NAND_CTRL_SIZE_64M;
		break;
	case 16 * 1024 * 1024:
		host->control_reg |= NAND_CTRL_SIZE_128M;
		break;
	case 32 * 1024 * 1024:
		host->control_reg |= NAND_CTRL_SIZE_256M;
		break;
	case 64 * 1024 * 1024:
		host->control_reg |= NAND_CTRL_SIZE_512M;
		break;
	case 128 * 1024 * 1024:
		host->control_reg |= NAND_CTRL_SIZE_1G;
		break;
	case 256 * 1024 * 1024:
		host->control_reg |= NAND_CTRL_SIZE_2G;
		break;
	case 512 * 1024 * 1024:
		host->control_reg |= NAND_CTRL_SIZE_4G;
		break;
	case 1024 * 1024 * 1024:
		host->control_reg |= NAND_CTRL_SIZE_8G;
		break;
	default:
		dev_err(host->dev,
			"Unexpected NAND flash chipsize %lld. Aborting\n",
			host->chip.chipsize);
		rval = -ENXIO;
		break;
	}

	return rval;
}

static void ambarella_nand_init_chip(struct ambarella_nand_host *host,
		struct device_node *np)
{
	struct nand_chip *chip = &host->chip;
	u32 poc;

	regmap_read(host->reg_rct, SYS_CONFIG_OFFSET, &poc);
	BUG_ON(!(poc & SYS_CONFIG_NAND_ECC_BCH_EN));

	/*
	 * if ecc is generated by software, the ecc bits num will
	 * be defined in FDT.
	 */
	if (!host->soft_ecc)
		host->ecc_bits = (poc & SYS_CONFIG_NAND_ECC_SPARE_2X) ? 8 : 6;

	host->page_4k = (poc & SYS_CONFIG_NAND_PAGE_SIZE) ? false : true;

	dev_info(host->dev, "in %secc-[%d]bit mode\n",
		host->soft_ecc ? "soft " : "", host->ecc_bits);

	ambarella_nand_init_hw(host);

	/*
	 * Always use P3 and I5 to support all NAND,
	 * but we will adjust page cycles after read ID from NAND.
	 */
	host->control_reg = NAND_CTRL_P3 | NAND_CTRL_SIZE_8G;
	host->control_reg |= host->enable_wp ? NAND_CTRL_WP : 0;

	chip->chip_delay = 0;
	chip->controller = &host->controller;
	chip->read_byte = ambarella_nand_read_byte;
	chip->read_word = ambarella_nand_read_word;
	chip->write_buf = ambarella_nand_write_buf;
	chip->read_buf = ambarella_nand_read_buf;
	chip->select_chip = ambarella_nand_select_chip;
	chip->cmd_ctrl = ambarella_nand_cmd_ctrl;
	chip->dev_ready = ambarella_nand_dev_ready;
	chip->waitfunc = ambarella_nand_waitfunc;
	chip->cmdfunc = ambarella_nand_cmdfunc;
	chip->options |= NAND_NO_SUBPAGE_WRITE;

	nand_set_flash_node(chip, np);
}

static int ambarella_nand_init_chipecc(struct ambarella_nand_host *host)
{
	struct nand_chip *chip = &host->chip;
	struct mtd_info	*mtd = nand_to_mtd(chip);
	int rval = 0;

	/* sanity check */
	BUG_ON(mtd->writesize != 2048 && mtd->writesize != 4096);
	BUG_ON(host->ecc_bits != 6 && host->ecc_bits != 8);
	BUG_ON(host->ecc_bits == 8 && mtd->oobsize < 128);

	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.strength = host->ecc_bits;

	switch (host->ecc_bits) {
	case 8:
		chip->ecc.size = 512;
		chip->ecc.bytes = 13;
		host->soft_bch_extra_size = 19;
		mtd_set_ooblayout(mtd, &amb_ecc8_lp_ooblayout_ops);
		break;
	case 6:
		chip->ecc.size = 512;
		chip->ecc.bytes = 10;
		host->soft_bch_extra_size = 6;
		mtd_set_ooblayout(mtd, &amb_ecc6_lp_ooblayout_ops);
		break;
	}

	if (host->soft_ecc) {
		rval = ambarella_nand_init_soft_bch(host);
		if (rval < 0)
			return rval;
	}

	chip->ecc.hwctl = ambarella_nand_hwctl;
	chip->ecc.calculate = ambarella_nand_calculate_ecc;
	chip->ecc.correct = ambarella_nand_correct_data;
	chip->ecc.write_oob = ambarella_nand_write_oob_std;

	return 0;
}

static int ambarella_nand_get_resource(
	struct ambarella_nand_host *host, struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	int rval = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No mem resource for fio_reg!\n");
		return -ENXIO;
	}

	host->regbase =
		devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!host->regbase) {
		dev_err(&pdev->dev, "devm_ioremap() failed\n");
		return -ENOMEM;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq < 0) {
		dev_err(&pdev->dev, "no irq found!\n");
		return -ENODEV;
	}

	host->reg_rct = syscon_regmap_lookup_by_phandle(np, "amb,rct-regmap");
	if (IS_ERR(host->reg_rct)) {
		dev_err(&pdev->dev, "no rct regmap!\n");
		return PTR_ERR(host->reg_rct);
	}

	host->enable_wp = !!of_find_property(np, "amb,enable-wp", NULL);

	rval = of_property_read_u32_array(np, "amb,timing",
			host->timing, 6);
	if (rval < 0) {
		dev_dbg(&pdev->dev, "No timing defined!\n");
		memset(host->timing, 0x0, sizeof(host->timing));
	}

	rval = of_property_read_u32(np, "amb,soft-ecc", &host->ecc_bits);
	if (rval == 0)
		host->soft_ecc = true;

	rval = request_irq(host->irq, ambarella_nand_isr_handler,
			IRQF_SHARED | IRQF_TRIGGER_HIGH,
			"nand_irq", host);
	if (rval < 0) {
		dev_err(&pdev->dev, "Could not register irq %d!\n", host->irq);
		return rval;
	}

	return 0;
}

static int ambarella_nand_probe(struct platform_device *pdev)
{
	struct ambarella_nand_host *host;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	int rval = 0;

	host = devm_kzalloc(&pdev->dev,
			sizeof(struct ambarella_nand_host), GFP_KERNEL);
	if (host == NULL) {
		dev_err(&pdev->dev, "kzalloc for nand host failed!\n");
		return -ENOMEM;
	}

	host->dev = &pdev->dev;
	spin_lock_init(&host->controller.lock);
	init_waitqueue_head(&host->controller.wq);
	init_waitqueue_head(&host->wq);
	sema_init(&host->system_event_sem, 1);

	host->dmabuf = dma_alloc_coherent(host->dev,
					AMBARELLA_NAND_DMA_BUFFER_SIZE,
					&host->dmaaddr, GFP_KERNEL);
	if (host->dmabuf == NULL) {
		dev_err(&pdev->dev, "dma_alloc_coherent failed!\n");
		rval = -ENOMEM;
		goto err_exit0;
	}
	BUG_ON(host->dmaaddr & 0x7);

#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
        aipc_mutex_lock(AMBA_IPC_MUTEX_NAND);
#endif
	rval = ambarella_nand_get_resource(host, pdev);
	if (rval < 0)
		goto err_exit1;

	ambarella_nand_init_chip(host, pdev->dev.of_node);
#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
        disable_irq(host->irq);
        aipc_mutex_unlock(AMBA_IPC_MUTEX_NAND);
#endif

	chip = &host->chip;
	nand_set_controller_data(chip, host);

	mtd = nand_to_mtd(chip);

	rval = nand_scan_ident(mtd, 1, NULL);
	if (rval)
		goto err_exit2;

	if (chip->bbt_options & NAND_BBT_USE_FLASH)
		chip->bbt_options |= NAND_BBT_NO_OOB;

	rval = ambarella_nand_init_chipecc(host);
	if (rval)
		goto err_exit2;

	rval = ambarella_nand_config_flash(host);
	if (rval)
		goto err_exit2;

	rval = nand_scan_tail(mtd);
	if (rval)
		goto err_exit2;

	mtd->name = "amba_nand";

	rval = mtd_device_parse_register(mtd, NULL, NULL, NULL, 0);
	if (rval < 0)
		goto err_exit2;

	platform_set_drvdata(pdev, host);

	host->system_event.notifier_call = ambarella_nand_system_event;
	ambarella_register_event_notifier(&host->system_event);

	return 0;

err_exit2:
	free_irq(host->irq, host);
err_exit1:
	dma_free_coherent(host->dev,
		AMBARELLA_NAND_DMA_BUFFER_SIZE,
		host->dmabuf, host->dmaaddr);
err_exit0:
#ifdef CONFIG_ARCH_AMBARELLA_AMBALINK
        disable_irq(host->irq);
        aipc_mutex_unlock(AMBA_IPC_MUTEX_NAND);
#endif
	return rval;
}

static int ambarella_nand_remove(struct platform_device *pdev)
{
	struct ambarella_nand_host *host = platform_get_drvdata(pdev);
	int rval = 0;

	if (host) {
		ambarella_unregister_event_notifier(&host->system_event);

		nand_release(nand_to_mtd(&host->chip));

		free_irq(host->irq, host);

		dma_free_coherent(host->dev,
			AMBARELLA_NAND_DMA_BUFFER_SIZE,
			host->dmabuf, host->dmaaddr);
	}

	return rval;
}

#ifdef CONFIG_PM
static int ambarella_nand_suspend(struct device *dev)
{
	struct ambarella_nand_host *host;
	struct platform_device *pdev = to_platform_device(dev);

	host = platform_get_drvdata(pdev);
	disable_irq(host->irq);

	return 0;
}

static int ambarella_nand_restore(struct device *dev)
{
	struct ambarella_nand_host *host;
	struct platform_device *pdev = to_platform_device(dev);
	int rval = 0;

#ifndef CONFIG_ARCH_AMBARELLA_AMBALINK
	host = platform_get_drvdata(pdev);
	ambarella_nand_init_hw(host);
#endif
	enable_irq(host->irq);
	rval = nand_scan_tail(nand_to_mtd(&host->chip));

	return rval;
}

static int ambarella_nand_resume(struct device *dev)
{
	struct ambarella_nand_host *host;
	struct platform_device *pdev = to_platform_device(dev);

#ifndef CONFIG_ARCH_AMBARELLA_AMBALINK
	host = platform_get_drvdata(pdev);
	ambarella_nand_init_hw(host);
#endif
	enable_irq(host->irq);

	return 0;
}

static const struct dev_pm_ops ambarella_nand_pm_ops = {
	/* suspend to memory */
	.suspend = ambarella_nand_suspend,
	.resume = ambarella_nand_resume,

	/* suspend to disk */
	.freeze = ambarella_nand_suspend,
	.thaw = ambarella_nand_resume,

	/* restore from suspend to disk */
	.restore = ambarella_nand_restore,
};
#endif

static const struct of_device_id ambarella_nand_of_match[] = {
	{.compatible = "ambarella,nand", },
	{},
};
MODULE_DEVICE_TABLE(of, ambarella_nand_of_match);

static struct platform_driver ambarella_nand_driver = {
	.probe		= ambarella_nand_probe,
	.remove		= ambarella_nand_remove,
	.driver = {
		.name	= "ambarella-nand",
		.of_match_table = ambarella_nand_of_match,
#ifdef CONFIG_PM
		.pm	= &ambarella_nand_pm_ops,
#endif
	},
};
module_platform_driver(ambarella_nand_driver);

MODULE_AUTHOR("Cao Rongrong");
MODULE_DESCRIPTION("Ambarella Combo NAND Controller Driver");
MODULE_LICENSE("GPL");

