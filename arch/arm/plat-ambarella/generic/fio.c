/*
 * arch/arm/plat-ambarella/generic/fio.c
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/setup.h>

#include <mach/hardware.h>
#include <plat/dma.h>
#include <plat/fio.h>
#include <plat/sd.h>
#include <plat/nand.h>
#include <plat/audio.h>
#include <plat/clk.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/
#if (CHIP_REV != S2L)

static DECLARE_WAIT_QUEUE_HEAD(fio_wait);
static DEFINE_SPINLOCK(fio_lock);

static u32 fio_owner = SELECT_FIO_FREE;
int fio_default_owner = SELECT_FIO_FREE;
#if defined(CONFIG_AMBARELLA_SYS_FIO_CALL)
module_param_cb(fio_owner, &param_ops_int, &fio_owner, 0644);
module_param_cb(fio_default_owner, &param_ops_int, &fio_default_owner, 0644);
#endif

static DEFINE_SPINLOCK(fio_sd0_int_lock);
static u32 fio_sd_int = 0;
static u32 fio_sdio_int = 0;

void __fio_select_lock(int module)
{
	u32					fio_ctr;
	u32					fio_dmactr;
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
	unsigned long				flags;
#endif

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
#if (CHIP_REV == A7L) || (CHIP_REV == S2)
		fio_ctr &= ~FIO_CTR_XD;
		fio_dmactr = (fio_dmactr & 0xcfffffff) | FIO_DMACTR_SD;
#endif
		break;

	default:
		break;
	}

#if (CHIP_REV == A5S) || (CHIP_REV == I1)
	spin_lock_irqsave(&fio_sd0_int_lock, flags);
	amba_clrbitsl(SD_REG(SD_NISEN_OFFSET), SD_NISEN_CARD);
	spin_unlock_irqrestore(&fio_sd0_int_lock, flags);
#if defined(CONFIG_AMBARELLA_FIO_FORCE_SDIO_GPIO)
	if (module != SELECT_FIO_SDIO) {
		ambarella_gpio_raw_lock(2, &flags);
		amba_clrbitsl(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x000007e0);
		ambarella_gpio_raw_unlock(2, &flags);
	}
#endif
#endif
	amba_writel(FIO_CTR_REG, fio_ctr);
	amba_writel(FIO_DMACTR_REG, fio_dmactr);
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
	if (module == SELECT_FIO_SD) {
		spin_lock_irqsave(&fio_sd0_int_lock, flags);
		amba_writel(SD_REG(SD_NISEN_OFFSET), fio_sd_int);
		amba_writel(SD_REG(SD_NIXEN_OFFSET), fio_sd_int);
		spin_unlock_irqrestore(&fio_sd0_int_lock, flags);
	} else
	if (module == SELECT_FIO_SDIO) {
#if defined(CONFIG_AMBARELLA_FIO_FORCE_SDIO_GPIO)
		ambarella_gpio_raw_lock(2, &flags);
		amba_setbitsl(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x000007e0);
		ambarella_gpio_raw_unlock(2, &flags);
#endif
		spin_lock_irqsave(&fio_sd0_int_lock, flags);
		amba_writel(SD_REG(SD_NISEN_OFFSET), fio_sdio_int);
		amba_writel(SD_REG(SD_NIXEN_OFFSET), fio_sdio_int);
		spin_unlock_irqrestore(&fio_sd0_int_lock, flags);
	}
#endif
}

static bool fio_check_free(u32 module)
{
	unsigned long flags;
	bool is_free = 0;

	spin_lock_irqsave(&fio_lock, flags);

	if (fio_owner & module) {
		pr_warning("%s: module[%d] reentry!\n", __func__, module);
		is_free = 1;
		goto fio_exit;
	}

#if (CHIP_REV == A7L) || (CHIP_REV == S2)
	if (module & (SELECT_FIO_SD | SELECT_FIO_SD2)) {
		if (!(fio_owner & (~(SELECT_FIO_SD | SELECT_FIO_SD2)))) {
			fio_owner |= module;
			is_free = 1;
			goto fio_exit;
		}
	} else
#endif
	if (fio_owner == SELECT_FIO_FREE) {
		is_free = 1;
		fio_owner = module;
		goto fio_exit;
	}

fio_exit:
	spin_unlock_irqrestore(&fio_lock, flags);

	return is_free;
}

void fio_select_lock(int module)
{
	wait_event(fio_wait, fio_check_free(module));
	__fio_select_lock(module);
}
EXPORT_SYMBOL(fio_select_lock);

void fio_unlock(int module)
{
	unsigned long flags;

	if (!(fio_owner & (~module)) &&
		(fio_default_owner != SELECT_FIO_FREE) &&
		(fio_default_owner != module)) {
		__fio_select_lock(fio_default_owner);
	}

	spin_lock_irqsave(&fio_lock, flags);
#if (CHIP_REV == A7L) || (CHIP_REV == S2)
	if (module & (SELECT_FIO_SD | SELECT_FIO_SD2)) {
		if (fio_owner & module) {
			fio_owner &= (~module);
			wake_up(&fio_wait);
		} else {
			pr_err("%s: fio_owner(0x%x) != module(0x%x)!.\n",
				__func__, fio_owner, module);
		}
	} else
#endif
	if (fio_owner == module) {
		fio_owner = SELECT_FIO_FREE;
		wake_up(&fio_wait);
	} else {
		pr_err("%s: fio_owner(%d) != module(%d)!.\n",
			__func__, fio_owner, module);
	}

	spin_unlock_irqrestore(&fio_lock, flags);
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

void fio_amb_sd0_set_int(u32 mask, u32 on)
{
	unsigned long				flags;
	u32					int_flag;

	spin_lock_irqsave(&fio_sd0_int_lock, flags);
	int_flag = amba_readl(SD_REG(SD_NISEN_OFFSET));
	if (on)
		int_flag |= mask;
	else
		int_flag &= ~mask;
	fio_sd_int = int_flag;
	if (fio_amb_sd0_is_enable()) {
		amba_writel(SD_REG(SD_NISEN_OFFSET), int_flag);
		amba_writel(SD_REG(SD_NIXEN_OFFSET), int_flag);
	}
	spin_unlock_irqrestore(&fio_sd0_int_lock, flags);
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

void fio_amb_sdio0_set_int(u32 mask, u32 on)
{
	unsigned long				flags;
	u32					int_flag;

	spin_lock_irqsave(&fio_sd0_int_lock, flags);
	int_flag = amba_readl(SD_REG(SD_NISEN_OFFSET));
	if (on)
		int_flag |= mask;
	else
		int_flag &= ~mask;
	fio_sdio_int = int_flag;
	if (fio_amb_sdio0_is_enable()) {
		amba_writel(SD_REG(SD_NISEN_OFFSET), int_flag);
		amba_writel(SD_REG(SD_NIXEN_OFFSET), int_flag);
	}
	spin_unlock_irqrestore(&fio_sd0_int_lock, flags);
}
#endif

/* ==========================================================================*/
int __init ambarella_init_fio(void)
{
#if defined(CONFIG_AMBARELLA_FIO_FORCE_SDIO_GPIO)
	unsigned long flags;

	//SMIO_38 ~ SMIO_43
	ambarella_gpio_raw_lock(2, &flags);
	amba_clrbitsl(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x000007e0);
	amba_clrbitsl(GPIO2_REG(GPIO_DIR_OFFSET), 0x00000780);
	amba_setbitsl(GPIO2_REG(GPIO_DIR_OFFSET), 0x00000060);
	amba_writel(GPIO2_REG(GPIO_MASK_OFFSET), 0x00000060);
	amba_writel(GPIO2_REG(GPIO_DATA_OFFSET), 0x00000040);
	ambarella_gpio_raw_unlock(2, &flags);
#endif
#if defined(CONFIG_AMBARELLA_RAW_BOOT)
#if (NAND_DUAL_SPACE_MODE == 1)
	u32 sys_config;

	sys_config = amba_rct_readl(SYS_CONFIG_REG);
	if ((sys_config & SYS_CONFIG_NAND_ECC_BCH_EN) ==
		SYS_CONFIG_NAND_ECC_BCH_EN) {
		if ((sys_config & SYS_CONFIG_NAND_ECC_SPARE_2X) ==
			SYS_CONFIG_NAND_ECC_SPARE_2X) {
			ambarella_nand_default_set.ecc_bits = 8;
		} else {
			ambarella_nand_default_set.ecc_bits = 6;
		}
	}
#endif
	amba_rct_writel(FIO_RESET_REG, (FIO_RESET_FIO_RST | FIO_RESET_CF_RST |
		FIO_RESET_XD_RST | FIO_RESET_FLASH_RST));
	mdelay(100);
	amba_rct_writel(FIO_RESET_REG, 0);
	mdelay(100);
	amba_clrbitsl(FIO_CTR_REG, FIO_CTR_RR);

	amba_setbitsl(NAND_CTR_REG, 0);
	amba_writel(NAND_CMD_REG, 0xff);
	mdelay(100);
	amba_writel(NAND_INT_REG, 0x0);
	amba_writel(NAND_CMD_REG, 0x90);
	mdelay(100);
	amba_writel(NAND_INT_REG, 0x0);
#endif
	return 0;
}

