/*
 * arch/arm/mach-ambarella/init-ixora.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2013, Ambarella, Inc.
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
#include <linux/clk.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/gpio.h>
#include <asm/system_info.h>

#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/board.h>
#include <mach/common.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/mmc/host.h>

#include <plat/ambinput.h>
#include <plat/ambcache.h>

#include "board-device.h"

#include <plat/dma.h>
#include <plat/clk.h>

#include <linux/input.h>
/* ==========================================================================*/
static struct platform_device *ixora_devices[] __initdata = {
	&ambarella_adc0,
//	&ambarella_crypto,
	&ambarella_ehci0,
	&ambarella_ohci0,
	&ambarella_eth0,
	&ambarella_fb0,
	&ambarella_fb1,
	&ambarella_i2s0,
	&ambarella_pcm0,
	&ambarella_dummy_codec0,
	&ambarella_idc0,
	&ambarella_idc1,
	&ambarella_idc2,
	&ambarella_ir0,
	&ambarella_sd0,
	&ambarella_sd1,
//	&ambarella_sd2,
	&ambarella_uart,
	&ambarella_uart1,
	&ambarella_udc0,
	&ambarella_wdt0,
	&ambarella_dma,
	&ambarella_nand,
	&ambarella_rtc0,
};

/* ==========================================================================*/
static struct ambarella_key_table generic_keymap[AMBINPUT_TABLE_SIZE] = {
	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_END},
};

static struct ambarella_input_board_info ixora_board_input_info = {
	.pkeymap		= generic_keymap,
	.pinput_dev		= NULL,
	.pdev			= NULL,

	.abx_max_x		= 4095,
	.abx_max_y		= 4095,
	.abx_max_pressure	= 4095,
	.abx_max_width		= 16,
};

struct platform_device ixora_board_input = {
	.name		= "ambarella-input",
	.id		= -1,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &ixora_board_input_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

/* ==========================================================================*/
static void __init ambarella_init_ixora(void)
{
	int i;
	int use_bub_default = 1;

#if defined(CONFIG_AMBARELLA_RAW_BOOT)
	system_rev = AMBARELLA_BOARD_VERSION(S2L,
		AMBARELLA_BOARD_TYPE_BUB, 'A');

	amba_writel(GPIO0_REG(GPIO_AFSEL_OFFSET), 0x00000000);
	amba_writel(GPIO0_REG(GPIO_DIR_OFFSET), 0x00000000);
	amba_writel(GPIO0_REG(GPIO_MASK_OFFSET), 0xFFFFFFFF);
	amba_writel(GPIO0_REG(GPIO_DATA_OFFSET), 0x00C00000);
	amba_writel(GPIO0_REG(GPIO_ENABLE_OFFSET), 0xFFFFFFFF);

	amba_writel(GPIO1_REG(GPIO_AFSEL_OFFSET), 0x00000000);
	amba_writel(GPIO1_REG(GPIO_DIR_OFFSET), 0x00000000);
	amba_writel(GPIO1_REG(GPIO_MASK_OFFSET), 0xFFFFFFFF);
	amba_writel(GPIO1_REG(GPIO_DATA_OFFSET), 0x00000000);
	amba_writel(GPIO1_REG(GPIO_ENABLE_OFFSET), 0xFFFFFFFF);

	amba_writel(GPIO2_REG(GPIO_AFSEL_OFFSET), 0x00000000);
	amba_writel(GPIO2_REG(GPIO_DIR_OFFSET), 0x00000000);
	amba_writel(GPIO2_REG(GPIO_MASK_OFFSET), 0xFFFFFFFF);
	amba_writel(GPIO2_REG(GPIO_DATA_OFFSET), 0x00000000);
	amba_writel(GPIO2_REG(GPIO_ENABLE_OFFSET), 0xFFFFFFFF);

	amba_writel(GPIO3_REG(GPIO_AFSEL_OFFSET), 0x00000000);
	amba_writel(GPIO3_REG(GPIO_DIR_OFFSET), 0x00000000);
	amba_writel(GPIO3_REG(GPIO_MASK_OFFSET), 0xFFFFFFFF);
	amba_writel(GPIO3_REG(GPIO_DATA_OFFSET), 0x00000000);
	amba_writel(GPIO3_REG(GPIO_ENABLE_OFFSET), 0xFFFFFFFF);

	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(0, 0)), 0xF800001F);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(0, 1)), 0x00878000);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(0, 2)), 0x07000000);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(1, 0)), 0x003FFFFF);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(1, 1)), 0xFFC00000);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(1, 2)), 0x00000000);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(2, 0)), 0xFE000000);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(2, 1)), 0x01FFFFFF);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(2, 2)), 0x00000000);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(3, 0)), 0x0003FFFF);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(3, 1)), 0x00000000);
	amba_writel(IOMUX_REG(IOMUX_REG_OFFSET(3, 2)), 0x00000000);
	amba_writel(IOMUX_REG(IOMUX_CTRL_SET_OFFSET), 0x00000001);
	amba_writel(IOMUX_REG(IOMUX_CTRL_SET_OFFSET), 0x00000000);

/*	amba_writel(GPIO_PAD_PULL_REG(GPIO_PAD_PULL_EN_0_OFFSET), 0x00000000);
	amba_writel(GPIO_PAD_PULL_REG(GPIO_PAD_PULL_DIR_0_OFFSET), 0x00000000);
	amba_writel(GPIO_PAD_PULL_REG(GPIO_PAD_PULL_EN_1_OFFSET), 0x00000000);
	amba_writel(GPIO_PAD_PULL_REG(GPIO_PAD_PULL_DIR_1_OFFSET), 0x00000000);
	amba_writel(GPIO_PAD_PULL_REG(GPIO_PAD_PULL_EN_2_OFFSET), 0x00000000);
	amba_writel(GPIO_PAD_PULL_REG(GPIO_PAD_PULL_DIR_2_OFFSET), 0x00000000);
	amba_writel(GPIO_PAD_PULL_REG(GPIO_PAD_PULL_EN_3_OFFSET), 0x00000000);
	amba_writel(GPIO_PAD_PULL_REG(GPIO_PAD_PULL_DIR_3_OFFSET), 0x00000000);*/


#endif
	ambarella_init_machine("ixora", REF_CLK_FREQ);

#ifdef CONFIG_OUTER_CACHE
	ambcache_l2_enable();
#endif

	if (use_bub_default) {
		/* Config USB over-curent protection */
		ambarella_board_generic.uhc_use_ocp = (0x1 << 16) | 0x3;
	}

	platform_add_devices(ixora_devices, ARRAY_SIZE(ixora_devices));
	for (i = 0; i < ARRAY_SIZE(ixora_devices); i++) {
		device_set_wakeup_capable(&ixora_devices[i]->dev, 1);
		device_set_wakeup_enable(&ixora_devices[i]->dev, 0);
	}

	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);

	platform_device_register(&ixora_board_input);
}

/* ==========================================================================*/
MACHINE_START(IXORA, "Ixora")
	.atag_offset	= 0x100,
	.restart_mode	= 's',
	.smp		= smp_ops(ambarella_smp_ops),
	.reserve	= ambarella_memblock_reserve,
	.map_io		= ambarella_map_io,
	.init_irq	= ambarella_init_irq,
	.init_time	= ambarella_timer_init,
	.init_machine	= ambarella_init_ixora,
	.restart	= ambarella_restart_machine,
MACHINE_END

