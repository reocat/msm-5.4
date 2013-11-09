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
#include <linux/clk.h>

#include <mach/hardware.h>
#include <plat/fio.h>
#include <plat/sd.h>
#include <plat/clk.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/
#if (CHIP_REV == S2L)
static struct clk pll_out_sd = {
	.parent		= NULL,
	.name		= "pll_out_sd",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_SD_CTRL_REG,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= PLL_REG_UNAVAILABLE,
	.frac_reg	= PLL_SD_FRAC_REG,
	.ctrl2_reg	= PLL_SD_CTRL2_REG,
	.ctrl3_reg	= PLL_SD_CTRL3_REG,
	.lock_reg	= PLL_LOCK_REG,
	.lock_bit	= 12,
	.divider	= 0,
	.max_divider	= 0,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_pll_ops,
};
#endif

static struct clk gclk_sd = {
	.parent		= NULL,
	.name		= "gclk_sd",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= SCALER_SD48_REG,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
	.divider	= 0,
	.max_divider	= (1 << 16) - 1,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_scaler_ops,
};

static struct clk *ambarella_sd_register_clk(void)
{
	struct clk *pgclk_sd = NULL;
	struct clk *ppll_parent = NULL;

	pgclk_sd = clk_get(NULL, "gclk_sd");
	if (IS_ERR(pgclk_sd)) {
#if (CHIP_REV == S2L)
		ppll_parent = clk_get(NULL, "pll_out_sd");
#else
		ppll_parent = clk_get(NULL, "pll_out_core");
#endif
		if (IS_ERR(ppll_parent)) {
			BUG();
		}
		gclk_sd.parent = ppll_parent;
		ambarella_clk_add(&gclk_sd);
		pgclk_sd = &gclk_sd;
		pr_info("SYSCLK:SD[%lu]\n", clk_get_rate(pgclk_sd));
	}

	return pgclk_sd;
}

#if (CHIP_REV == I1) || (CHIP_REV == S2L)
static struct clk gclk_sdxc = {
	.parent		= NULL,
	.name		= "gclk_sdxc",
	.rate		= 0,
	.frac_mode	= 0,
#if (CHIP_REV == I1)
	.ctrl_reg	= PLL_SDXC_CTRL_REG,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= SCALER_SDXC_REG,
	.frac_reg	= PLL_SDXC_FRAC_REG,
	.ctrl2_reg	= PLL_SDXC_CTRL2_REG,
	.ctrl3_reg	= PLL_SDXC_CTRL3_REG,
	.lock_reg	= PLL_LOCK_REG,
	.lock_bit	= 10,
#elif (CHIP_REV == S2L)
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= SCALER_SDXC_REG,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
#endif
	.divider	= 0,
#if (CHIP_REV == I1)
	.max_divider	= 0,
#elif (CHIP_REV == S2L)
	.max_divider	= (1 << 16) - 1,
#endif
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_pll_ops,
};

static struct clk *ambarella_sdxc_register_clk(void)
{
	struct clk *pgclk_sdxc = NULL;
#if (CHIP_REV == S2L)
	struct clk *ppll_parent = NULL;
#endif

	pgclk_sdxc = clk_get(NULL, "gclk_sdxc");
	if (IS_ERR(pgclk_sdxc)) {
#if (CHIP_REV == S2L)
		ppll_parent = clk_get(NULL, "pll_out_sd");
		if (IS_ERR(ppll_parent)) {
			BUG();
		}
		gclk_sdxc.parent = ppll_parent;
#endif
		ambarella_clk_add(&gclk_sdxc);
		pgclk_sdxc = &gclk_sdxc;
		pr_info("SYSCLK:SDXC[%lu]\n", clk_get_rate(pgclk_sdxc));
	}

	return pgclk_sdxc;
}
#endif

#if (CHIP_REV == A7L) || (CHIP_REV == S2) || (CHIP_REV == S2L)
static struct clk gclk_sdio = {
	.parent		= NULL,
	.name		= "gclk_sdio",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= SCALER_SDIO_REG,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
	.divider	= 0,
	.max_divider	= (1 << 16) - 1,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_scaler_ops,
};

static struct clk *ambarella_sdio_register_clk(void)
{
	struct clk *pgclk_sdio = NULL;
	struct clk *ppll_parent = NULL;

	pgclk_sdio = clk_get(NULL, "gclk_sdio");
	if (IS_ERR(pgclk_sdio)) {
#if (CHIP_REV == S2L)
		ppll_parent = clk_get(NULL, "pll_out_sd");
#else
		ppll_parent = clk_get(NULL, "pll_out_core");
#endif
		if (IS_ERR(ppll_parent)) {
			BUG();
		}
		gclk_sdio.parent = ppll_parent;
		ambarella_clk_add(&gclk_sdio);
		pgclk_sdio = &gclk_sdio;
		pr_info("SYSCLK:SDIO[%lu]\n", clk_get_rate(pgclk_sdio));
	}

	return pgclk_sdio;
}
#endif

static void ambarella_sd_set_pll(u32 freq_hz)
{
	clk_set_rate(ambarella_sd_register_clk(), freq_hz);
}

static u32 ambarella_sd_get_pll(void)
{
	return clk_get_rate(ambarella_sd_register_clk());
}

#if (CHIP_REV == I1) || (CHIP_REV == S2L)
static void ambarella_sdxc_set_pll(u32 freq_hz)
{
	clk_set_rate(ambarella_sdxc_register_clk(), freq_hz);
}

static u32 ambarella_sdxc_get_pll(void)
{
	return clk_get_rate(ambarella_sdxc_register_clk());
}
#endif

#if (CHIP_REV == A7L) || (CHIP_REV == S2) || (CHIP_REV == S2L)
static void ambarella_sdio_set_pll(u32 freq_hz)
{
	clk_set_rate(ambarella_sdio_register_clk(), freq_hz);
}

static u32 ambarella_sdio_get_pll(void)
{
	return clk_get_rate(ambarella_sdio_register_clk());
}
#endif

/* ==========================================================================*/
typedef void (*mmc_dc_fn)(struct mmc_host *host, unsigned long delay);

#if defined(CONFIG_AMBARELLA_SYS_SD_CALL)
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
#endif

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

static void fio_amb_sd0_slot1_request(void)
{
#if (CHIP_REV == S2L)
	return;
#else
	fio_select_lock(SELECT_FIO_SD);
#endif
}

static void fio_amb_sd0_slot1_release(void)
{
#if (CHIP_REV == S2L)
	return;
#else
	fio_unlock(SELECT_FIO_SD);
#endif
}

#if (CHIP_REV == S2L)
static void ambarella_sd0_set_int(u32 mask, u32 on)
{
	u32 int_flag;

	int_flag = amba_readl(SD_REG(SD_NISEN_OFFSET));
	if (on)
		int_flag |= mask;
	else
		int_flag &= ~mask;
	amba_writel(SD_REG(SD_NISEN_OFFSET), int_flag);
	amba_writel(SD_REG(SD_NIXEN_OFFSET), int_flag);
}
#endif

#if (CHIP_REV == A5S) || (CHIP_REV == I1)
static void fio_amb_sd0_slot2_request(void)
{
	fio_select_lock(SELECT_FIO_SDIO);
}

static void fio_amb_sd0_slot2_release(void)
{
	fio_unlock(SELECT_FIO_SDIO);
}
#endif

static int fio_amb_sd_check_owner(void)
{
#if (CHIP_REV == A7L) || (CHIP_REV == S2) || (CHIP_REV == S2L)
	return 1;
#else
	return fio_amb_sd0_is_enable();
#endif
}

struct ambarella_sd_controller ambarella_platform_sd0_controller = {
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
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
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
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
		.check_owner	= fio_amb_sd_check_owner,
		.request	= fio_amb_sd0_slot1_request,
		.release	= fio_amb_sd0_slot1_release,
#if (CHIP_REV == S2L)
		.set_int	= ambarella_sd0_set_int,
#else
		.set_int	= fio_amb_sd0_set_int,
#endif
		.set_vdd	= NULL,
		.set_bus_timing	= NULL,
	},
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
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
	.set_pll		= ambarella_sd_set_pll,
	.get_pll		= ambarella_sd_get_pll,
	.max_blk_mask		= SD_BLK_SZ_128KB,
	.max_clock		= 24000000,
	.active_clock		= 0,
	.wait_tmo		= ((5 * HZ) / 2),
	.pwr_delay		= 1,

	.dma_fix		= 0,
	.support_pll_scaler	= 1,
};
#if defined(CONFIG_AMBARELLA_SYS_SD_CALL)
module_param_cb(sd0_max_block, &param_ops_uint,
	&(ambarella_platform_sd0_controller.max_blk_mask), 0644);
module_param_cb(sd0_max_clock, &param_ops_uint,
	&(ambarella_platform_sd0_controller.max_clock), 0644);
module_param_cb(sd0_active_clock, &param_ops_uint,
	&(ambarella_platform_sd0_controller.active_clock), 0444);
module_param_cb(sd0_wait_timeout, &param_ops_uint,
	&(ambarella_platform_sd0_controller.wait_tmo), 0644);
module_param_cb(sd0_pwr_delay, &param_ops_uint,
	&(ambarella_platform_sd0_controller.pwr_delay), 0644);
AMBA_SD_PARAM_CALL(0, 0, ambarella_platform_sd0_controller,
	&param_ops_ambarella_sd_cdpos, 0644);
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
AMBA_SD_PARAM_CALL(0, 1, ambarella_platform_sd0_controller,
	&param_ops_ambarella_sd_cdpos, 0644);
#endif
#endif

struct platform_device ambarella_sd0 = {
	.name		= "ambarella-sd",
	.id		= 0,
	.resource	= ambarella_sd0_resources,
	.num_resources	= ARRAY_SIZE(ambarella_sd0_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_sd0_controller,
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

static int ambarella_sd2_check_owner(void)
{
	return 1;
}

static void ambarella_sd2_request(void)
{
#if (CHIP_REV == A7L) || (CHIP_REV == S2)
	fio_select_lock(SELECT_FIO_SD2);
#endif
}

static void ambarella_sd2_release(void)
{
#if (CHIP_REV == A7L) || (CHIP_REV == S2)
	fio_unlock(SELECT_FIO_SD2);
#endif
}

static void ambarella_sd2_set_int(u32 mask, u32 on)
{
	u32 int_flag;

	int_flag = amba_readl(SD2_REG(SD_NISEN_OFFSET));
	if (on)
		int_flag |= mask;
	else
		int_flag &= ~mask;
	amba_writel(SD2_REG(SD_NISEN_OFFSET), int_flag);
	amba_writel(SD2_REG(SD_NIXEN_OFFSET), int_flag);
}

struct ambarella_sd_controller ambarella_platform_sd1_controller = {
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
		.check_owner	= ambarella_sd2_check_owner,
		.request	= ambarella_sd2_request,
		.release	= ambarella_sd2_release,
		.set_int	= ambarella_sd2_set_int,
		.set_vdd	= NULL,
		.set_bus_timing	= NULL,
	},
#if (CHIP_REV == I1)
	.set_pll		= ambarella_sdxc_set_pll,
	.get_pll		= ambarella_sdxc_get_pll,
#elif (CHIP_REV == A7L) || (CHIP_REV == S2) || (CHIP_REV == S2L)
	.set_pll		= ambarella_sdio_set_pll,
	.get_pll		= ambarella_sdio_get_pll,
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
	.support_pll_scaler	= 1,
};
#if defined(CONFIG_AMBARELLA_SYS_SD_CALL)
module_param_cb(sd1_max_block, &param_ops_uint,
	&(ambarella_platform_sd1_controller.max_blk_mask), 0644);
module_param_cb(sd1_max_clock, &param_ops_uint,
	&(ambarella_platform_sd1_controller.max_clock), 0644);
module_param_cb(sd1_active_clock, &param_ops_uint,
	&(ambarella_platform_sd1_controller.active_clock), 0444);
module_param_cb(sd1_wait_timeout, &param_ops_uint,
	&(ambarella_platform_sd1_controller.wait_tmo), 0644);
module_param_cb(sd1_pwr_delay, &param_ops_uint,
	&(ambarella_platform_sd1_controller.pwr_delay), 0644);
AMBA_SD_PARAM_CALL(1, 0, ambarella_platform_sd1_controller,
	&param_ops_ambarella_sd_cdpos, 0644);
#endif

struct platform_device ambarella_sd1 = {
	.name		= "ambarella-sd",
	.id		= 1,
	.resource	= ambarella_sd1_resources,
	.num_resources	= ARRAY_SIZE(ambarella_sd1_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_sd1_controller,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif

#if (SD_INSTANCES >= 3)
struct resource ambarella_sd2_resources[] = {
	[0] = {
		.start	= SD3_BASE,
		.end	= SD3_BASE + 0x0FFF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= SD3_IRQ,
		.end	= SD3_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static int ambarella_sd3_check_owner(void)
{
	return 1;
}

static void ambarella_sd3_request(void)
{
}

static void ambarella_sd3_release(void)
{
}

static void ambarella_sd3_set_int(u32 mask, u32 on)
{
	u32 int_flag;

	int_flag = amba_readl(SD3_REG(SD_NISEN_OFFSET));
	if (on)
		int_flag |= mask;
	else
		int_flag &= ~mask;
	amba_writel(SD3_REG(SD_NISEN_OFFSET), int_flag);
	amba_writel(SD3_REG(SD_NIXEN_OFFSET), int_flag);
}

struct ambarella_sd_controller ambarella_platform_sd2_controller = {
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
		.check_owner	= ambarella_sd3_check_owner,
		.request	= ambarella_sd3_request,
		.release	= ambarella_sd3_release,
		.set_int	= ambarella_sd3_set_int,
		.set_vdd	= NULL,
		.set_bus_timing	= NULL,
	},
	.set_pll		= ambarella_sdxc_set_pll,
	.get_pll		= ambarella_sdxc_get_pll,
	.max_blk_mask		= SD_BLK_SZ_128KB,
	.max_clock		= 24000000,
	.active_clock		= 0,
	.wait_tmo		= ((5 * HZ) / 2),
	.pwr_delay		= 1,
	.dma_fix		= 0,
	.support_pll_scaler	= 1,
};
#if defined(CONFIG_AMBARELLA_SYS_SD_CALL)
module_param_cb(sd2_max_block, &param_ops_uint,
	&(ambarella_platform_sd2_controller.max_blk_mask), 0644);
module_param_cb(sd2_max_clock, &param_ops_uint,
	&(ambarella_platform_sd2_controller.max_clock), 0644);
module_param_cb(sd2_active_clock, &param_ops_uint,
	&(ambarella_platform_sd2_controller.active_clock), 0444);
module_param_cb(sd2_wait_timeout, &param_ops_uint,
	&(ambarella_platform_sd2_controller.wait_tmo), 0644);
module_param_cb(sd2_pwr_delay, &param_ops_uint,
	&(ambarella_platform_sd2_controller.pwr_delay), 0644);
AMBA_SD_PARAM_CALL(1, 0, ambarella_platform_sd2_controller,
	&param_ops_ambarella_sd_cdpos, 0644);
#endif

struct platform_device ambarella_sd2 = {
	.name		= "ambarella-sd",
	.id		= 1,
	.resource	= ambarella_sd2_resources,
	.num_resources	= ARRAY_SIZE(ambarella_sd2_resources),
	.dev		= {
		.platform_data		= &ambarella_platform_sd2_controller,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};
#endif

/* ==========================================================================*/
int __init ambarella_init_sd(void)
{
	int retval = 0;

#if (CHIP_REV == S2L)
	ambarella_clk_add(&pll_out_sd);
#endif

	return retval;
}

void ambarella_detect_sd_slot(int bus, int slot, int fixed_cd)
{
	struct ambarella_sd_slot		*pslotinfo = NULL;
	mmc_dc_fn				mmc_dc = NULL;

	pr_debug("%s:%d[%d:%d:%d]\n", __func__, __LINE__, bus, slot, fixed_cd);
	if ((bus == 0) && (slot == 0)) {
		pslotinfo = &ambarella_platform_sd0_controller.slot[0];
	}
#if (CHIP_REV == A5S) || (CHIP_REV == I1)
	if ((bus == 0) && (slot == 1)) {
		pslotinfo = &ambarella_platform_sd0_controller.slot[1];
	}
#endif
#if (SD_INSTANCES >= 2)
	if ((bus == 1) && (slot == 0)) {
		pslotinfo = &ambarella_platform_sd1_controller.slot[0];
	}
#endif
#if (SD_INSTANCES >= 3)
	if ((bus == 2) && (slot == 0)) {
		pslotinfo = &ambarella_platform_sd2_controller.slot[0];
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

