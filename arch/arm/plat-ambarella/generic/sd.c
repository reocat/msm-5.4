/*
 * arch/arm/plat-ambarella/generic/sd.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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
#include <linux/irq.h>
#include <linux/moduleparam.h>
#include <linux/mmc/host.h>
#include <linux/module.h>

#include <mach/hardware.h>
#include <plat/fio.h>
#include <plat/sd.h>
#include <hal/hal.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/
typedef void (*mmc_dc_fn)(struct mmc_host *host, unsigned long delay);

int ambarella_sd_set_fixed_cd(const char *val, const struct kernel_param *kp)
{
	struct ambarella_sd_slot		*pslotinfo;
	mmc_dc_fn				mmc_dc = NULL;
	int					retval;

	retval = param_set_int(val, kp);
	pslotinfo = container_of(kp->arg, struct ambarella_sd_slot, fixed_cd);
#if defined(CONFIG_MMC)
	mmc_dc = mmc_detect_change;
#else
#if defined(CONFIG_KALLSYMS)
	mmc_dc = (mmc_dc_fn)module_kallsyms_lookup_name("mmc_detect_change");
#endif
#endif
	if (!retval && pslotinfo && mmc_dc && pslotinfo->pmmc_host) {
		mmc_dc(pslotinfo->pmmc_host, pslotinfo->cd_delay);
	}

	return retval;
}

static struct kernel_param_ops param_ops_ambarella_sd_cdpos = {
	.set = ambarella_sd_set_fixed_cd,
	.get = param_get_int,
};

/* ==========================================================================*/
struct resource ambarella_sd0_resources[] = {
	[0] = {
		.start	= SD_BASE,
		.end	= SD_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SD_IRQ,
		.end	= SD_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

void fio_amb_sd0_slot1_request(void)
{
	fio_select_lock(SELECT_FIO_SD);
}

void fio_amb_sd0_slot1_release(void)
{
	fio_unlock(SELECT_FIO_SD);
}

#if (SD_HAS_INTERNAL_MUXER == 1)
void fio_amb_sd0_slot2_request(void)
{
	fio_select_lock(SELECT_FIO_SDIO);
}

void fio_amb_sd0_slot2_release(void)
{
	fio_unlock(SELECT_FIO_SDIO);
}
#endif

struct ambarella_sd_controller ambarella_platform_sd_controller0 = {
#if (SD_HAS_INTERNAL_MUXER == 1)
	.num_slots		= 2,
#else
	.num_slots		= 1,
#endif
	.slot[0] = {
		.pmmc_host	= NULL,

		.default_caps	= MMC_CAP_4_BIT_DATA |
				MMC_CAP_SDIO_IRQ |
				MMC_CAP_ERASE |
				MMC_CAP_BUS_WIDTH_TEST,
		.active_caps	= 0,
		.default_caps2	= 0,
		.active_caps2	= 0,
		.private_caps	= AMBA_SD_PRIVATE_CAPS_DTO_BY_SDCLK,

		.ext_power	= {
			.gpio_id	= -1,
			.active_level	= GPIO_LOW,
			.active_delay	= 1,
		},
		.ext_reset	= {
			.gpio_id	= -1,
			.active_level	= GPIO_LOW,
			.active_delay	= 1,
		},
		.fixed_cd	= -1,
#if (SD_HAS_INTERNAL_MUXER == 1)
		.gpio_cd	= {
			.irq_gpio	= SD1_CD,
			.irq_line	= SD1CD_IRQ,
			.irq_type	= IRQ_TYPE_EDGE_BOTH,
			.irq_gpio_val	= GPIO_LOW,
			.irq_gpio_mode	= GPIO_FUNC_HW,
		},
#else
		.gpio_cd	= {
			.irq_gpio	= -1,
			.irq_line	= -1,
			.irq_type	= -1,
			.irq_gpio_val	= GPIO_LOW,
			.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
		},
#endif
		.cd_delay	= 100,
		.fixed_wp	= -1,
		.gpio_wp	= {
			.gpio_id	= -1,
			.active_level	= GPIO_HIGH,
			.active_delay	= 1,
		},

		.check_owner	= fio_amb_sd0_is_enable,
		.request	= fio_amb_sd0_slot1_request,
		.release	= fio_amb_sd0_slot1_release,
		.set_int	= fio_amb_sd0_set_int,
		.set_vdd	= NULL,
		.set_bus_timing	= NULL,
	},
#if (SD_HAS_INTERNAL_MUXER == 1)
	.slot[1] = {
		.pmmc_host	= NULL,

		.default_caps	= MMC_CAP_4_BIT_DATA |
				MMC_CAP_SDIO_IRQ |
				MMC_CAP_ERASE |
				MMC_CAP_BUS_WIDTH_TEST,
		.active_caps	= 0,
		.default_caps2	= 0,
		.active_caps2	= 0,
		.private_caps	= AMBA_SD_PRIVATE_CAPS_DTO_BY_SDCLK,

		.ext_power	= {
			.gpio_id	= -1,
			.active_level	= GPIO_LOW,
			.active_delay	= 1,
		},
		.ext_reset	= {
			.gpio_id	= -1,
			.active_level	= GPIO_LOW,
			.active_delay	= 1,
		},
		.fixed_cd	= -1,
		.gpio_cd	= {
			.irq_gpio	= -1,
			.irq_line	= -1,
			.irq_type	= -1,
			.irq_gpio_val	= GPIO_LOW,
			.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
		},
		.cd_delay	= 100,
		.fixed_wp	= -1,
		.gpio_wp	= {
			.gpio_id	= -1,
			.active_level	= GPIO_HIGH,
			.active_delay	= 1,
		},

		.check_owner	= fio_amb_sdio0_is_enable,
		.request	= fio_amb_sd0_slot2_request,
		.release	= fio_amb_sd0_slot2_release,
		.set_int	= fio_amb_sdio0_set_int,
		.set_vdd	= NULL,
		.set_bus_timing	= NULL,
	},
#endif
	.set_pll		= rct_set_sd_pll,
	.get_pll		= get_sd_freq_hz,

	.max_blk_mask		= SD_BLK_SZ_128KB,
	.max_clock		= 24000000,
	.active_clock		= 0,
	.wait_tmo		= ((5 * HZ) / 2),
	.pwr_delay		= 1,

	.dma_fix		= 0,
#if (SD_SUPPORT_PLL_SCALER == 1)
	.support_pll_scaler	= 1,
#else
	.support_pll_scaler	= 0,
#endif
};
module_param_cb(sd0_max_block, &param_ops_uint,
	&(ambarella_platform_sd_controller0.max_blk_mask), 0644);
module_param_cb(sd0_max_clock, &param_ops_uint,
	&(ambarella_platform_sd_controller0.max_clock), 0644);
module_param_cb(sd0_active_clock, &param_ops_uint,
	&(ambarella_platform_sd_controller0.active_clock), 0444);
module_param_cb(sd0_wait_timeout, &param_ops_uint,
	&(ambarella_platform_sd_controller0.wait_tmo), 0644);
module_param_cb(sd0_pwr_delay, &param_ops_uint,
	&(ambarella_platform_sd_controller0.pwr_delay), 0644);
AMBA_SD_PARAM_CALL(0, 0, ambarella_platform_sd_controller0,
	&param_ops_ambarella_sd_cdpos, 0644);
#if (SD_HAS_INTERNAL_MUXER == 1)
AMBA_SD_PARAM_CALL(0, 1, ambarella_platform_sd_controller0,
	&param_ops_ambarella_sd_cdpos, 0644);
#endif

struct platform_device ambarella_sd0 = {
	.name		= "ambarella-sd",
	.id		= 0,
	.resource	= ambarella_sd0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_sd0_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_sd_controller0,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

#if (SD_INSTANCES >= 2)
struct resource ambarella_sd1_resources[] = {
	[0] = {
		.start	= SD2_BASE,
		.end	= SD2_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SD2_IRQ,
		.end	= SD2_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

int fio_amb_sd2_check_owner(void)
{
#if (FIO_SUPPORT_AHB_CLK_ENA == 1)
	return fio_amb_sd2_is_enable();
#endif
#if (SD_HOST1_HOST2_HAS_MUX == 1)
	return fio_amb_sd0_is_enable();
#endif
	return 1;
}

void fio_amb_sd2_request(void)
{
#if (FIO_SUPPORT_AHB_CLK_ENA == 1)
	fio_select_lock(SELECT_FIO_SD2);
#endif
#if (SD_HOST1_HOST2_HAS_MUX == 1)
	fio_select_lock(SELECT_FIO_SD2);
#endif
}

void fio_amb_sd2_release(void)
{
#if (FIO_SUPPORT_AHB_CLK_ENA == 1)
	fio_unlock(SELECT_FIO_SD2);
#endif
#if (SD_HOST1_HOST2_HAS_MUX == 1)
	fio_unlock(SELECT_FIO_SD2);
#endif
}

static DEFINE_SPINLOCK(fio_amb_sd2_int_lock);
void fio_amb_sd2_set_int(u32 mask, u32 on)
{
	unsigned long				flags;
	u32					int_flag;

	spin_lock_irqsave(&fio_amb_sd2_int_lock, flags);
	int_flag = amba_readl(SD2_NISEN_REG);
	if (on)
		int_flag |= mask;
	else
		int_flag &= ~mask;
	amba_writel(SD2_NISEN_REG, int_flag);
	amba_writel(SD2_NIXEN_REG, int_flag);
	spin_unlock_irqrestore(&fio_amb_sd2_int_lock, flags);
}

struct ambarella_sd_controller ambarella_platform_sd_controller1 = {
	.num_slots		= 1,
	.slot[0] = {
		.pmmc_host	= NULL,

		.default_caps	= MMC_CAP_4_BIT_DATA |
				MMC_CAP_SDIO_IRQ |
				MMC_CAP_ERASE |
				MMC_CAP_BUS_WIDTH_TEST,
		.active_caps	= 0,
		.default_caps2	= 0,
		.active_caps2	= 0,
		.private_caps	= AMBA_SD_PRIVATE_CAPS_DTO_BY_SDCLK,

		.ext_power	= {
			.gpio_id	= -1,
			.active_level	= GPIO_LOW,
			.active_delay	= 1,
		},
		.ext_reset	= {
			.gpio_id	= -1,
			.active_level	= GPIO_LOW,
			.active_delay	= 1,
		},
		.fixed_cd	= -1,
		.gpio_cd	= {
			.irq_gpio	= -1,
			.irq_line	= -1,
			.irq_type	= -1,
			.irq_gpio_val	= GPIO_LOW,
			.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
		},
		.cd_delay	= 100,
		.fixed_wp	= -1,
		.gpio_wp	= {
			.gpio_id	= -1,
			.active_level	= GPIO_HIGH,
			.active_delay	= 1,
		},

		.check_owner	= fio_amb_sd2_check_owner,
		.request	= fio_amb_sd2_request,
		.release	= fio_amb_sd2_release,
		.set_int	= fio_amb_sd2_set_int,
		.set_vdd	= NULL,
		.set_bus_timing	= NULL,
	},
#if (SD_HAS_SDXC_CLOCK == 1)
	.set_pll		= rct_set_sdxc_pll,
	.get_pll		= get_sdxc_freq_hz,
#else
	.set_pll		= rct_set_sd_pll,
	.get_pll		= get_sd_freq_hz,
#endif

	.max_blk_mask		= SD_BLK_SZ_128KB,
	.max_clock		= 24000000,
	.active_clock		= 0,
	.wait_tmo		= ((5 * HZ) / 2),
	.pwr_delay		= 1,

#if (CHIP_REV == S2)
	.dma_fix		= 0xC0000000,
#else
	.dma_fix		= 0,
#endif
#if (SD_SUPPORT_PLL_SCALER == 1)
	.support_pll_scaler	= 1,
#else
	.support_pll_scaler	= 0,
#endif
};
module_param_cb(sd1_max_block, &param_ops_uint,
	&(ambarella_platform_sd_controller1.max_blk_mask), 0644);
module_param_cb(sd1_max_clock, &param_ops_uint,
	&(ambarella_platform_sd_controller1.max_clock), 0644);
module_param_cb(sd1_active_clock, &param_ops_uint,
	&(ambarella_platform_sd_controller1.active_clock), 0444);
module_param_cb(sd1_wait_timeout, &param_ops_uint,
	&(ambarella_platform_sd_controller1.wait_tmo), 0644);
module_param_cb(sd1_pwr_delay, &param_ops_uint,
	&(ambarella_platform_sd_controller1.pwr_delay), 0644);
AMBA_SD_PARAM_CALL(1, 0, ambarella_platform_sd_controller1,
	&param_ops_ambarella_sd_cdpos, 0644);

struct platform_device ambarella_sd1 = {
	.name		= "ambarella-sd",
	.id		= 1,
	.resource	= ambarella_sd1_resources,
	.num_resources	= ARRAY_SIZE(ambarella_sd1_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_sd_controller1,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif

int __init ambarella_init_sd(void)
{
	int retval = 0;

	return retval;
}

void ambarella_detect_sd_slot(int bus, int slot, int fixed_cd)
{
	struct ambarella_sd_slot		*pslotinfo = NULL;
	mmc_dc_fn				mmc_dc = NULL;

	pr_debug("%s:%d[%d:%d:%d]\n", __func__, __LINE__, bus, slot, fixed_cd);
	if ((bus == 0) && (slot == 0)) {
		pslotinfo = &ambarella_platform_sd_controller0.slot[0];
	}
#if (SD_HAS_INTERNAL_MUXER == 1)
	if ((bus == 0) && (slot == 1)) {
		pslotinfo = &ambarella_platform_sd_controller0.slot[1];
	}
#endif
#if (SD_INSTANCES >= 2)
	if ((bus == 1) && (slot == 0)) {
		pslotinfo = &ambarella_platform_sd_controller1.slot[0];
	}
#endif

#if defined(CONFIG_MMC)
	mmc_dc = mmc_detect_change;
#else
#if defined(CONFIG_KALLSYMS)
	mmc_dc = (mmc_dc_fn)module_kallsyms_lookup_name("mmc_detect_change");
#endif
#endif
	if (pslotinfo && mmc_dc && pslotinfo->pmmc_host) {
		pslotinfo->fixed_cd = fixed_cd;
		mmc_dc(pslotinfo->pmmc_host, pslotinfo->cd_delay);
		pr_debug("%s:%d[%p:%p:%p:%d]\n", __func__, __LINE__, pslotinfo,
			mmc_dc, pslotinfo->pmmc_host, pslotinfo->cd_delay);
	}
}
EXPORT_SYMBOL(ambarella_detect_sd_slot);

