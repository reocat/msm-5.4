/*
 * arch/arm/mach-ambarella/fio_amb.c
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
static int fio_owner = SELECT_FIO_FREE;

module_param_call(fio_select_sdio_as_default, param_set_int, param_get_int,
	&fio_select_sdio_as_default, 0644);

/* ==========================================================================*/
int fio_select_lock(int module, int lock)
{
	u32 fio_ctr;
	u32 fio_dmactr;

	if (lock) {
		wait_event_interruptible(fio_lock, ((fio_owner == module) ||
			(fio_owner == SELECT_FIO_FREE)));
		fio_owner = module;
	}

	fio_ctr = amba_readl(FIO_CTR_REG);
	fio_dmactr = amba_readl(FIO_DMACTR_REG);

	switch (module) {
	case SELECT_FIO_FL:
		/* In A2..., a proper boot config is needed for nand boot */
		/* with xD card inserted. */
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
#ifdef CONFIG_BLK_DEV_IDE_AMBARELLA_FULL_CF_BUS
		fio_amb_sd2_disable();	//Disable SD2 in CF BUS mode(PC Mem or PC IO)
#endif
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
#ifdef CONFIG_BLK_DEV_IDE_AMBARELLA_FULL_CF_BUS
		fio_amb_cf_disable();	//Disable CF in CF BUS mode(PC Mem or PC IO)
#endif
		fio_amb_sd2_enable();
#endif
		break;

	default:	// SELECT_FIO_HOLD: used to hold the fio bus
		break;
	}

	amba_writel(FIO_CTR_REG, fio_ctr);
	amba_writel(FIO_DMACTR_REG, fio_dmactr);

	return 0;
}
EXPORT_SYMBOL(fio_select_lock);

void fio_unlock(int module, int lock)
{
#if (SD_HAS_INTERNAL_MUXER == 1) && !defined(CONFIG_BLK_DEV_IDE_AMBARELLA_FULL_CF_BUS)
	u32 fio_ctr;
	u32 fio_dmactr;

	if (fio_select_sdio_as_default) {
		fio_ctr = amba_readl(FIO_CTR_REG);
		fio_dmactr = amba_readl(FIO_DMACTR_REG);

		fio_ctr |= FIO_CTR_XD;
		fio_dmactr = (fio_dmactr & 0xcfffffff) | FIO_DMACTR_SD;

		amba_writel(FIO_CTR_REG, fio_ctr);
		amba_writel(FIO_DMACTR_REG, fio_dmactr);
	}
#endif

	if (lock && (fio_owner == module)){
		fio_owner = SELECT_FIO_FREE;
		wake_up(&fio_lock);
	}
}
EXPORT_SYMBOL(fio_unlock);

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
EXPORT_SYMBOL(fio_dma_parse_error);

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

void fio_amb_exit_random_mode(void)
{
	amba_clrbitsl(FIO_CTR_REG, FIO_CTR_RR);
}

int __init ambarella_init_fio(void)
{
#if (HOST_MAX_AHB_CLK_EN_BITS == 10)
	amba_clrbitsl(HOST_AHB_CLK_ENABLE_REG,
		(HOST_AHB_BOOT_SEL | HOST_AHB_FDMA_BURST_DIS));
#endif

	/* Following should be handled by the bootloader... */
	//rct_reset_fio();
	//fio_amb_exit_random_mode();
	//amba_writel(FLASH_INT_REG, 0x0);
	//amba_writel(XD_INT_REG, 0x0);
	//amba_writel(CF_STA_REG, CF_STA_CW | CF_STA_DW);

	return 0;
}

