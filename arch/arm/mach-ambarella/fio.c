/*
 * arch/arm/mach-ambarella/fio.c
 *
 * History:
 *	2008/03/05 - [Chien-Yang Chen] created file
 *	2008/01/09 - [Anthony Ginger] Port to 2.6.28.
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
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/interrupt.h>

#include <asm/io.h>

#include <mach/hardware.h>

/* ==========================================================================*/
static DECLARE_WAIT_QUEUE_HEAD(fio_lock);
static int fio_select_sdio_as_default = 0;
static atomic_t fio_owner = ATOMIC_INIT(SELECT_FIO_FREE);

module_param_call(fio_select_sdio_as_default, param_set_int, param_get_int,
	&fio_select_sdio_as_default, 0644);
module_param_call(fio_owner, param_set_int, param_get_int, &fio_owner, 0644);

/* ==========================================================================*/
void fio_select_lock(int module)
{
	u32 fio_ctr;
	u32 fio_dmactr;

	if (atomic_read(&fio_owner) != module) {
		wait_event(fio_lock, (atomic_cmpxchg(&fio_owner,
			SELECT_FIO_FREE, module) == SELECT_FIO_FREE));
	} else {
		pr_warning("%s: module[%d] reentry!\n", __func__, module);
	}

	fio_ctr = amba_readl(FIO_CTR_REG);
	fio_dmactr = amba_readl(FIO_DMACTR_REG);

	switch (module) {
	case SELECT_FIO_FL:
		fio_ctr &= ~FIO_CTR_XD;
		fio_dmactr = (fio_dmactr & 0xcfffffff) | FIO_DMACTR_FL;
		break;

	case SELECT_FIO_XD:
		fio_ctr |= FIO_CTR_XD;
		fio_dmactr = (fio_dmactr & 0xcfffffff) | FIO_DMACTR_XD;
		break;

	case SELECT_FIO_CF:
		fio_ctr &= ~FIO_CTR_XD;
		fio_dmactr = (fio_dmactr & 0xcfffffff) | FIO_DMACTR_CF;
#if (FIO_SUPPORT_AHB_CLK_ENA == 1)
		fio_amb_sd2_disable();
		fio_amb_cf_enable();
#endif
		break;

	case SELECT_FIO_SD:
		fio_ctr &= ~FIO_CTR_XD;
		fio_dmactr = (fio_dmactr & 0xcfffffff) | FIO_DMACTR_SD;
		break;

	case SELECT_FIO_SDIO:
		fio_ctr |= FIO_CTR_XD;
		fio_dmactr = (fio_dmactr & 0xcfffffff) | FIO_DMACTR_SD;
		break;

	case SELECT_FIO_SD2:
#if (FIO_SUPPORT_AHB_CLK_ENA == 1)
		fio_amb_cf_disable();
		fio_amb_sd2_enable();
#endif
		break;

	default:
		break;
	}

	amba_writel(FIO_CTR_REG, fio_ctr);
	amba_writel(FIO_DMACTR_REG, fio_dmactr);
}

void fio_unlock(int module)
{
#if (SD_HAS_INTERNAL_MUXER == 1)
	u32 fio_ctr;
	u32 fio_dmactr;
#endif

	if (atomic_read(&fio_owner) == module) {
#if (SD_HAS_INTERNAL_MUXER == 1)
		if (fio_select_sdio_as_default && (module != SELECT_FIO_SDIO)) {
			fio_ctr = amba_readl(FIO_CTR_REG);
			fio_dmactr = amba_readl(FIO_DMACTR_REG);

			fio_ctr |= FIO_CTR_XD;
			fio_dmactr = (fio_dmactr & 0xcfffffff) | FIO_DMACTR_SD;

			amba_writel(FIO_CTR_REG, fio_ctr);
			amba_writel(FIO_DMACTR_REG, fio_dmactr);
		}
#endif
#if (FIO_SUPPORT_AHB_CLK_ENA == 1)
		if (fio_select_sdio_as_default && (module == SELECT_FIO_CF)) {
			fio_amb_cf_disable();
			fio_amb_sd2_enable();
		}
#endif
	}

	if (atomic_cmpxchg(&fio_owner, module, SELECT_FIO_FREE) == module) {
		wake_up(&fio_lock);
	} else {
		pr_err("%s: fio_owner[%d] != module[%d]!.\n",
			__func__, atomic_read(&fio_owner), module);
	}
}

int fio_amb_sd0_is_enable(void)
{
	u32 fio_ctr;
	u32 fio_dmactr;

	fio_ctr = amba_readl(FIO_CTR_REG);
	fio_dmactr = amba_readl(FIO_DMACTR_REG);

	return (((fio_ctr & FIO_CTR_XD) == 0) && 
		((fio_dmactr & FIO_DMACTR_SD) == FIO_DMACTR_SD));
}

int fio_amb_sdio0_is_enable(void)
{
	u32 fio_ctr;
	u32 fio_dmactr;

	fio_ctr = amba_readl(FIO_CTR_REG);
	fio_dmactr = amba_readl(FIO_DMACTR_REG);

	return (((fio_ctr & FIO_CTR_XD) == FIO_CTR_XD) && 
		((fio_dmactr & FIO_DMACTR_SD) == FIO_DMACTR_SD));
}

int fio_dma_parse_error(u32 reg)
{
	int rval = 0;

	if (reg & FIO_DMASTA_RE) {
		pr_err("%s: fio dma read error 0x%x.\n", __func__, reg);
		rval = FIO_READ_ER;
		goto done;
	}

	if (reg & FIO_DMASTA_AE) {
		pr_err("%s: fio dma address error 0x%x.\n", __func__, reg);
		rval = FIO_ADDR_ER;
		goto done;
	}

	if (!(reg & FIO_DMASTA_DN)) {
		pr_err("%s: fio dma operation not done error 0x%x.\n",
			__func__, reg);
		rval = FIO_OP_NOT_DONE_ER;
	}

done:
	return rval;
}

#if (FIO_SUPPORT_AHB_CLK_ENA == 1)
void fio_amb_fl_enable(void)
{
	amba_setbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_FLASH_CLK_ENB);
}

void fio_amb_fl_disable(void)
{
	amba_clrbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_FLASH_CLK_ENB);
}

int fio_amb_fl_is_enable(void)
{
	return amba_tstbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_FLASH_CLK_ENB);
}

void fio_amb_cf_enable(void)
{
	amba_setbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_CF_CLK_ENB);
}

void fio_amb_cf_disable(void)
{
	amba_clrbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_CF_CLK_ENB);
}

int fio_amb_cf_is_enable(void)
{
	return amba_tstbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_CF_CLK_ENB);
}

void fio_amb_sd2_enable(void)
{
	amba_setbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_SDIO_SEL);
}

void fio_amb_sd2_disable(void)
{
	amba_clrbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_SDIO_SEL);
}

int fio_amb_sd2_is_enable(void)
{
	return amba_tstbitsl(HOST_AHB_CLK_ENABLE_REG, HOST_AHB_SDIO_SEL);
}

#endif

#if (NAND_DUMMY_XFER == 1)
void nand_dummy_xfer(void)
{
	amba_writel(FIO_CTR_REG, 0x15);
	amba_writel(NAND_CTR_REG, 0x4080110);

	amba_writel(DMA_FIOS_CHAN0_STA_REG, 0x0);
	amba_writel(DMA_FIOS_CHAN0_SRC_REG, 0x60000000);
	amba_writel(DMA_FIOS_CHAN0_DST_REG, 0xc0000000);
	amba_writel(DMA_FIOS_CHAN0_CTR_REG, 0xae800020);

	amba_writel(FIO_DMAADR_REG, 0x0);
	amba_writel(FIO_DMACTR_REG, 0x86800020);

	while ((amba_readl(DMA_FIOS_INT_REG) & 0x1) != 0x1);
	amba_writel(DMA_FIOS_CHAN0_STA_REG, 0x0);

	while ((amba_readl(FIO_STA_REG) & 0x1) != 0x1);
	amba_writel(FIO_CTR_REG, 0x1);
	amba_writel(NAND_INT_REG, 0x0);

	while ((amba_readl(FIO_DMASTA_REG) & 0x1000000) != 0x1000000);
	amba_writel(FIO_DMASTA_REG, 0x0);
}
#endif

void enable_fio_dma(void)
{
#if ((HOST_MAX_AHB_CLK_EN_BITS == 10) || (I2S_24BITMUX_MODE_REG_BITS == 4))
	u32 val;
#endif

#if (HOST_MAX_AHB_CLK_EN_BITS == 10)
	/* Disable boot-select */
	val = amba_readl(HOST_AHB_CLK_ENABLE_REG);
	val &= ~(HOST_AHB_BOOT_SEL);
	val &= ~(HOST_AHB_FDMA_BURST_DIS);
	amba_writel(HOST_AHB_CLK_ENABLE_REG, val);
#endif

#if (I2S_24BITMUX_MODE_REG_BITS == 4)
	val = amba_readl(I2S_24BITMUX_MODE_REG);
	val &= ~(I2S_24BITMUX_MODE_DMA_BOOTSEL);
	val &= ~(I2S_24BITMUX_MODE_FDMA_BURST_DIS);
	amba_writel(I2S_24BITMUX_MODE_REG, val);
#endif

#if (NAND_DUMMY_XFER == 1)
	nand_dummy_xfer();
#endif
}

void fio_amb_exit_random_mode(void)
{
	amba_clrbitsl(FIO_CTR_REG, FIO_CTR_RR);
}

int __init ambarella_init_fio(void)
{
#ifndef CONFIG_AMBARELLA_QUICK_INIT
	/* Following should be handled by the bootloader... */
#if (HOST_MAX_AHB_CLK_EN_BITS == 10)
	amba_clrbitsl(HOST_AHB_CLK_ENABLE_REG,
		(HOST_AHB_BOOT_SEL | HOST_AHB_FDMA_BURST_DIS));
#endif
	rct_reset_fio();
	fio_amb_exit_random_mode();
	enable_fio_dma();
	amba_writel(FLASH_INT_REG, 0x0);
	amba_writel(XD_INT_REG, 0x0);
	amba_writel(CF_STA_REG, CF_STA_CW | CF_STA_DW);
#endif

	return 0;
}

