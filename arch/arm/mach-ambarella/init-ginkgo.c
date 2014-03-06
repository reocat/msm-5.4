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
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <linux/irqchip.h>
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
static struct platform_device *ambarella_devices[] __initdata = {
	//&ambarella_crypto,
//	&ambarella_fb0,
//	&ambarella_fb1,
	//&ambarella_ir0,
	//&ambarella_spi0,
	&ambarella_spi_slave,
	&ambarella_wdt0,
#if defined(CONFIG_PLAT_AMBARELLA_CORTEX)
	&mpcore_wdt,
#endif
};

static struct platform_device *ambarella_pwm_devices[] __initdata = {
	&ambarella_pwm_backlight_device0,
	/*&ambarella_pwm_backlight_device1,
	&ambarella_pwm_backlight_device2,
	&ambarella_pwm_backlight_device3,
	&ambarella_pwm_backlight_device4,*/
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
		.bus_num	= 0,
		.chip_select	= 6,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 7,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 0,
	},
};

/* ==========================================================================*/
static struct ambarella_key_table generic_keymap[AMBINPUT_TABLE_SIZE] = {
	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_POWER,		1,		0x0100bcbd}}},	//POWER
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_VOLUMEUP,	1,		0x01000405}}},	//VOLUME_UP
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_VOLUMEDOWN,	1,		0x01008485}}},	//VOLUME_DOWN

	{AMBINPUT_GPIO_KEY,	{.gpio_key	= {KEY_POWER,	1,	1,	GPIO(13),	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING}}},

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
	int i, ret;
	int use_bub_default = 1;

#ifdef CONFIG_OUTER_CACHE
	ambcache_l2_enable();
#endif

	/* Config SD */
	fio_default_owner = SELECT_FIO_SD;

	/* Config Vin */
	ambarella_board_generic.vin1_reset.gpio_id = GPIO(49);
	ambarella_board_generic.vin1_reset.active_level = GPIO_LOW;
	ambarella_board_generic.vin1_reset.active_delay = 1;

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_IPCAM) {
		switch (AMBARELLA_BOARD_REV(system_rev)) {
		case 'A':
			/* Register audio codec
			 * the cs_pin of spi0.4, spi0,5, spi0.6 and spi0.7 are
			 * used as I2S signals, so we need to prevent
			 * them from be modified by SPI driver */
			ambarella_spi0_cs_pins[4] = -1;
			ambarella_spi0_cs_pins[5] = -1;
			ambarella_spi0_cs_pins[6] = -1;
			ambarella_spi0_cs_pins[7] = -1;
#if defined(CONFIG_RTC_AMBARELLA_IS112022M)
			i2c_register_board_info(2, &ambarella_isl12022m_board_info, 1);
#else
			platform_device_register(&ambarella_rtc0);
#endif
			use_bub_default = 0;
			break;

		default:
			pr_warn("%s: Unknown EVK Rev[%d]\n", __func__,
				AMBARELLA_BOARD_REV(system_rev));
			break;
		}
	}

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_ATB) {

			/* Register audio codec
			 * the cs_pin of spi0.4, spi0,5, spi0.6 and spi0.7 are
			 * used as I2S signals, so we need to prevent
			 * them from be modified by SPI driver */
			ambarella_spi0_cs_pins[1] = -1;
			ambarella_spi0_cs_pins[2] = GPIO(91);
			ambarella_spi0_cs_pins[3] = -1;
			ambarella_spi0_cs_pins[4] = -1;
			ambarella_spi0_cs_pins[5] = -1;
			ambarella_spi0_cs_pins[6] = -1;
			ambarella_spi0_cs_pins[7] = -1;

			platform_device_register(&ambarella_rtc0);

			ambarella_board_generic.vin0_reset.gpio_id = GPIO(54);
			ambarella_board_generic.vin0_reset.active_level = GPIO_LOW;
			ambarella_board_generic.vin0_reset.active_delay = 500;

			use_bub_default = 0;
	}

	if (use_bub_default) {
		platform_device_register(&ambarella_rtc0);
	}

	ambarella_platform_ir_controller0.protocol = AMBA_IR_PROTOCOL_PANASONIC;

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}

	for (i = 0; i < ARRAY_SIZE(ambarella_pwm_devices); i++) {
		ret = platform_device_register(ambarella_pwm_devices[i]);
		if (ret)
			continue;
		device_set_wakeup_capable(&ambarella_pwm_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_pwm_devices[i]->dev, 0);
	}

	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));

	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);

	platform_device_register(&ginkgo_board_input);
}

/* ==========================================================================*/

static struct of_dev_auxdata ambarella_auxdata_lookup[] __initdata = {
	{}
};

static void __init ambarella_init_ginkgo_dt(void)
{
	ambarella_init_machine("ginkgo", REF_CLK_FREQ);

	ambarella_init_ginkgo();

	of_platform_populate(NULL, of_default_bus_match_table,
			ambarella_auxdata_lookup, NULL);
}


static const char * const s2_dt_board_compat[] = {
	"ambarella,s2",
	"ambarella,ginkgo",
	NULL,
};

DT_MACHINE_START(GINKGO_DT, "Ambarella S2 (Flattened Device Tree)")
	.restart_mode	=	's',
	.smp		=	smp_ops(ambarella_smp_ops),
	.map_io		=	ambarella_map_io,
	.init_early	=	ambarella_init_early,
	.init_irq	=	irqchip_init,
	.init_time	=	ambarella_timer_init,
	.init_machine	=	ambarella_init_ginkgo_dt,
	.restart	=	ambarella_restart_machine,
	.dt_compat	=	s2_dt_board_compat,
MACHINE_END

