/*
 * arch/arm/mach-ambarella/init-ginkgo.c
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

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/board.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/i2c.h>
#include <linux/i2c/ak4183.h>
#include <linux/i2c/cy8ctmg.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <plat/ambinput.h>
#include <plat/ambcache.h>

#include "board-device.h"

#if (AUDIO_CODEC_INSTANCES == 1)
static struct platform_device ambarella_auc_codec0 = {
	.name		= "a2auc-codec",
	.id		= -1,
};
#endif

/* ==========================================================================*/
static struct platform_device *ambarella_devices[] __initdata = {
	&ambarella_adc0,
//	&ambarella_crypto,
	&ambarella_dummy_codec0,
	&ambarella_ehci0,
	&ambarella_ohci0,
	&ambarella_eth0,
	&ambarella_fb0,
	&ambarella_fb1,
	&ambarella_i2s0,
	&ambarella_idc0,
	&ambarella_idc1,
	&ambarella_ir0,
	&ambarella_rtc0,
	&ambarella_sd0,
	&ambarella_sd1,
	&ambarella_spi0,
	&ambarella_uart,
	&ambarella_uart1,
	&ambarella_uart2,
	&ambarella_uart3,
	&ambarella_udc0,
	&ambarella_wdt0,
};

/* ==========================================================================*/
static struct spi_board_info ambarella_spi_devices[] = {
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 1,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 2,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 3,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 4,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 5,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 1,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 2,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 3,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 4,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 5,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 6,
	},
};

/* ==========================================================================*/
static struct ambarella_key_table generic_keymap[AMBINPUT_TABLE_SIZE] = {
	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_END},
};

static struct ambarella_input_board_info ginkgo_board_input_info = {
	.pkeymap		= generic_keymap,
	.pinput_dev		= NULL,
	.pdev			= NULL,

	.abx_max_x		= 4095,
	.abx_max_y		= 4095,
	.abx_max_pressure	= 4095,
	.abx_max_width		= 16,
};

struct platform_device ginkgo_board_input = {
	.name		= "ambarella-input",
	.id		= -1,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &ginkgo_board_input_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

/* ==========================================================================*/
static void __init ambarella_init_ginkgo(void)
{
	int					i;

	ambarella_init_machine("ginkgo");

#ifdef CONFIG_OUTER_CACHE
	ambcache_l2_enable();
#endif

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}

	/* Config SD */
	fio_default_owner = SELECT_FIO_FREE;
	ambarella_platform_sd_controller0.slot[0].max_blk_sz = SD_BLK_SZ_512KB;
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio = SMIO_5;
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_line = gpio_to_irq(SMIO_5);
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_type = IRQ_TYPE_EDGE_BOTH;
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio_mode = GPIO_FUNC_SW_INPUT;
	ambarella_platform_sd_controller1.slot[0].max_blk_sz = SD_BLK_SZ_512KB;
	ambarella_platform_sd_controller1.slot[0].gpio_cd.irq_gpio = SMIO_44;
	ambarella_platform_sd_controller1.slot[0].gpio_cd.irq_line = gpio_to_irq(SMIO_44);
	ambarella_platform_sd_controller1.slot[0].gpio_cd.irq_type = IRQ_TYPE_EDGE_BOTH;
	ambarella_platform_sd_controller1.slot[0].gpio_wp.gpio_id = SMIO_45;

	ambarella_board_generic.uhc_use_ocp = (0x1 << 16) | 0x1;

	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));

	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);

	platform_device_register(&ginkgo_board_input);
}

/* ==========================================================================*/
MACHINE_START(GINKGO, "Ginkgo")
	.boot_params	= CONFIG_AMBARELLA_PARAMS_PHYS,
	.map_io		= ambarella_map_io,
	.reserve	= ambarella_memblock_reserve,
	.init_irq	= ambarella_init_irq,
	.timer		= &ambarella_timer,
	.init_machine	= ambarella_init_ginkgo,
MACHINE_END

