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
#if (CHIP_REV == S2L)
#else
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
#endif

/* ==========================================================================*/
#if (CHIP_REV == S2L)
#else
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

/* ==========================================================================*/
static struct ambarella_nand_set ambarella_nand_default_set = {
	.name		= "ambarella_nand_set",
	.nr_chips	= 1,
	.nr_partitions	= 0,
	.ecc_bits	= 0,
};

static struct ambarella_nand_timing ambarella_nand_default_timing = {
	.control	= 0,
	.size		= 0,
	.timing0	= 0xFFFFFFFF,
	.timing1	= 0xFFFFFFFF,
	.timing2	= 0xFFFFFFFF,
	.timing3	= 0xFFFFFFFF,
	.timing4	= 0xFFFFFFFF,
	.timing5	= 0xFFFFFFFF,
};

static void fio_amb_nand_request(void)
{
#if (CHIP_REV == S2L)
#else
	fio_select_lock(SELECT_FIO_FL);
#endif
}

static void fio_amb_nand_release(void)
{
#if (CHIP_REV == S2L)
#else
	fio_unlock(SELECT_FIO_FL);
#endif
}

static int fio_amb_nand_parse_error(u32 reg)
{
	return fio_dma_parse_error(reg);
}

static u32 ambarella_nand_get_pll(void)
{
	u32 nand_pll;

	nand_pll = (clk_get_rate(clk_get(NULL, "gclk_ahb")) / 1000);
#if (FIO_USE_2X_FREQ == 1)
	nand_pll <<= 1;
#endif
	return nand_pll;
}

static void ambarella_nand_get_cfg(u32 *ppage_size, u32 *pread_confirm)
{
	u32 sys_config;

	sys_config = amba_rct_readl(SYS_CONFIG_REG);
	*ppage_size = (sys_config & SYS_CONFIG_NAND_PAGE_SIZE) ? 2048 : 512;
	*pread_confirm = (sys_config & SYS_CONFIG_NAND_READ_CONFIRM) ? 1 : 0;
}

static struct ambarella_platform_nand ambarella_platform_default_nand = {
	.sets		= &ambarella_nand_default_set,
	.timing		= &ambarella_nand_default_timing,
	.flash_bbt	= 1,
	.id_cycles	= NAND_READ_ID_CYCLES,
	.parse_error	= fio_amb_nand_parse_error,
	.request	= fio_amb_nand_request,
	.release	= fio_amb_nand_release,
	.get_pll	= ambarella_nand_get_pll,
	.get_cfg	= ambarella_nand_get_cfg,
};

static int __init parse_nand_tag_cs(const struct tag *tag)
{
	ambarella_nand_default_timing.control = tag->u.serialnr.low;
	ambarella_nand_default_timing.size = tag->u.serialnr.high;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_CS, parse_nand_tag_cs);

static int __init parse_nand_tag_t0(const struct tag *tag)
{
	ambarella_nand_default_timing.timing0 = tag->u.serialnr.low;
	ambarella_nand_default_timing.timing1 = tag->u.serialnr.high;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_T0, parse_nand_tag_t0);

static int __init parse_nand_tag_t1(const struct tag *tag)
{
	ambarella_nand_default_timing.timing2 = tag->u.serialnr.low;
	ambarella_nand_default_timing.timing3 = tag->u.serialnr.high;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_T1, parse_nand_tag_t1);

static int __init parse_nand_tag_t2(const struct tag *tag)
{
	ambarella_nand_default_timing.timing4 = tag->u.serialnr.low;
	ambarella_nand_default_timing.timing5 = tag->u.serialnr.high;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_T2, parse_nand_tag_t2);

static int __init parse_nand_tag_ecc(const struct tag *tag)
{
	ambarella_nand_default_set.ecc_bits = tag->u.serialnr.low;

	return 0;
}
__tagtable(ATAG_AMBARELLA_NAND_ECC, parse_nand_tag_ecc);

static struct resource ambarella_fio_resources[] = {
	[0] = {
		.start	= FIO_BASE,
		.end	= FIO_BASE + 0x0FFF,
		.name	= "fio_reg",
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= FIOCMD_IRQ,
		.end	= FIOCMD_IRQ,
		.name	= "fio_cmd_irq",
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= FIODMA_IRQ,
		.end	= FIODMA_IRQ,
		.name	= "fio_dma_irq",
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= GPIO(39),
		.end	= GPIO(39),
		.name	= "wp_gpio",
		.flags	= IORESOURCE_IO,
	},
	[4] = {
		.start	= FIO_FIFO_BASE,
		.end	= FIO_FIFO_BASE + 0x0FFF,
		.name	= "fio_fifo",
		.flags	= IORESOURCE_MEM,
	},
	[5] = {
		.start	= DMA_FIOS_BASE,
		.end	= DMA_FIOS_BASE + 0x0FFF,
		.name	= "fdma_reg",
		.flags	= IORESOURCE_MEM,
	},
	[6] = {
		.start	= DMA_FIOS_IRQ,
		.end	= DMA_FIOS_IRQ,
		.name	= "fdma_irq",
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ambarella_nand = {
	.name		= "ambarella-nand",
	.id		= -1,
	.resource	= ambarella_fio_resources,
	.num_resources	= ARRAY_SIZE(ambarella_fio_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_default_nand,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

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

