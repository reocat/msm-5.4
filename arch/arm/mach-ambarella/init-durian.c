/*
 * arch/arm/mach-ambarella/init-coconut.c
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

#include <linux/input.h>
#include <plat/ambinput.h>

#include "board-device.h"

/* ==========================================================================*/
#include <linux/pda_power.h>
static int ambarella_power_is_ac_online(void)
{
	return 1;
}

static struct pda_power_pdata  ambarella_power_supply_info = {
	.is_ac_online    = ambarella_power_is_ac_online,
};

static struct platform_device ambarella_power_supply = {
	.name = "pda-power",
	.id   = -1,
	.dev  = {
		.platform_data = &ambarella_power_supply_info,
	},
};

/* ==========================================================================*/
static struct platform_device *ambarella_devices[] __initdata = {
	&ambarella_adc0,
	&ambarella_crypto,
	&ambarella_fb0,
	&ambarella_fb1,
	&ambarella_i2s0,
	&ambarella_idc0,
	&ambarella_idc1,
	&ambarella_ir0,
	&ambarella_rtc0,
	&ambarella_sd0,
	&ambarella_spi0,
	&ambarella_spi1,
	&ambarella_uart,
	&ambarella_uart1,
	&ambarella_udc0,
	&ambarella_wdt0,
	&ambarella_power_supply,
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
		.bus_num	= 1,
		.chip_select	= 0,
	}
};

/* ==========================================================================*/
static struct ambarella_key_table durian_keymap[AMBINPUT_TABLE_SIZE] = {
	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_CAMERA,	1,	2,	345,	385,}}},	//sb1: CAMERA
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_BACK,	1,	2,	607,	647,}}},	//sw3: BACK
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_ESC,	1,	2,	700,	740,}}},	//sw5: HOME
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_MENU,	1,	2,	780,	820,}}},	//sw4: MENU
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_SEARCH,	1,	2,	845,	875,}}},	//sw6: SEARCH
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_PHONE,	1,	2,	876,	910,}}},	//sw7: PHONE
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_DOWN,	1,	2,	918,	953,}}},	//sw8: VOLUME -
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_UP,	1,	2,	955,	990,}}},	//sw9: VOLUME +

	{AMBINPUT_END}
};

static struct ambarella_input_board_info durian_board_input_info = {
	.pkeymap		= durian_keymap,
	.pinput_dev		= NULL,
	.pdev			= NULL,

	.abx_max_x		= 4095,
	.abx_max_y		= 4095,
	.abx_max_pressure	= 4095,
	.abx_max_width		= 16,
};

static struct platform_device durian_board_input = {
	.name		= "ambarella-input",
	.id		= -1,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &durian_board_input_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_32BIT_MASK,
	}
};

/* ==========================================================================*/
static void __init ambarella_init_durian(void)
{
	int					i;

	ambarella_init_machine("Durian");

	/* Config Board*/
	ambarella_board_generic.power_detect.irq_gpio = GPIO(11);
	ambarella_board_generic.power_detect.irq_line = gpio_to_irq(11);
	ambarella_board_generic.power_detect.irq_type = IRQF_TRIGGER_FALLING;
	ambarella_board_generic.power_detect.irq_gpio_val = GPIO_LOW;
	ambarella_board_generic.power_detect.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.power_control.gpio_id = -1;
	ambarella_board_generic.power_control.active_level = GPIO_LOW;
	ambarella_board_generic.power_control.active_delay = 1;

	ambarella_board_generic.debug_led0.gpio_id = GPIO(83);
	ambarella_board_generic.debug_led0.active_level = GPIO_LOW;
	ambarella_board_generic.debug_led0.active_delay = 0;

	ambarella_board_generic.rs485.gpio_id = GPIO(31);
	ambarella_board_generic.rs485.active_level = GPIO_LOW;
	ambarella_board_generic.rs485.active_delay = 1;

	ambarella_board_generic.audio_reset.gpio_id = -1;
	ambarella_board_generic.audio_reset.active_level = GPIO_LOW;
	ambarella_board_generic.audio_reset.active_delay = 1;

	ambarella_board_generic.audio_speaker.gpio_id = -1;
	ambarella_board_generic.audio_speaker.active_level = GPIO_LOW;
	ambarella_board_generic.audio_speaker.active_delay = 1;

	ambarella_board_generic.audio_headphone.gpio_id = -1;
	ambarella_board_generic.audio_headphone.active_level = GPIO_LOW;
	ambarella_board_generic.audio_headphone.active_delay = 1;

	ambarella_board_generic.audio_microphone.gpio_id = -1;
	ambarella_board_generic.audio_microphone.active_level = GPIO_LOW;
	ambarella_board_generic.audio_microphone.active_delay = 1;

	ambarella_board_generic.touch_panel_irq.irq_gpio = GPIO(12);
	ambarella_board_generic.touch_panel_irq.irq_line = gpio_to_irq(12);
	ambarella_board_generic.touch_panel_irq.irq_type = IRQF_TRIGGER_FALLING;
	ambarella_board_generic.touch_panel_irq.irq_gpio_val = GPIO_LOW;
	ambarella_board_generic.touch_panel_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.touch_panel_reset.gpio_id = -1;
	ambarella_board_generic.touch_panel_reset.active_level = GPIO_LOW;
	ambarella_board_generic.touch_panel_reset.active_delay = 1;

	ambarella_board_generic.lcd_reset.gpio_id = GPIO(28);
	ambarella_board_generic.lcd_reset.active_level = GPIO_LOW;
	ambarella_board_generic.lcd_reset.active_delay = 1;

	ambarella_board_generic.lcd_power.gpio_id = GPIO(38);
	ambarella_board_generic.lcd_power.active_level = GPIO_HIGH;
	ambarella_board_generic.lcd_power.active_delay = 1;

	ambarella_board_generic.lcd_backlight.gpio_id = GPIO(16);
	ambarella_board_generic.lcd_backlight.active_level = GPIO_HIGH;
	ambarella_board_generic.lcd_backlight.active_delay = 1;

	ambarella_board_generic.vin_vsync.irq_gpio = GPIO(95);
	ambarella_board_generic.vin_vsync.irq_line = gpio_to_irq(95);
	ambarella_board_generic.vin_vsync.irq_type = IRQF_TRIGGER_RISING;
	ambarella_board_generic.vin_vsync.irq_gpio_val = GPIO_HIGH;
	ambarella_board_generic.vin_vsync.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.flash_charge_ready.irq_gpio = GPIO(13);
	ambarella_board_generic.flash_charge_ready.irq_line = gpio_to_irq(13);
	ambarella_board_generic.flash_charge_ready.irq_type = IRQF_TRIGGER_RISING;
	ambarella_board_generic.flash_charge_ready.irq_gpio_val = GPIO_HIGH;
	ambarella_board_generic.flash_charge_ready.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_board_generic.flash_trigger.gpio_id = GPIO(46);
	ambarella_board_generic.flash_trigger.active_level = GPIO_LOW;
	ambarella_board_generic.flash_trigger.active_delay = 1;

	ambarella_board_generic.flash_enable.gpio_id = GPIO(82);
	ambarella_board_generic.flash_enable.active_level = GPIO_LOW;
	ambarella_board_generic.flash_enable.active_delay = 1;

	/* Config ETH*/
	ambarella_eth0_platform_info.mii_reset.gpio_id = GPIO(7);

	/* Config SD*/
	fio_select_sdio_as_default = 1;
	ambarella_platform_sd_controller0.clk_limit = 25000000;
	ambarella_platform_sd_controller0.slot[0].cd_delay = 100;
	ambarella_platform_sd_controller0.slot[0].bounce_buffer = 1;
	ambarella_platform_sd_controller0.slot[1].ext_power.gpio_id = GPIO(54);
	ambarella_platform_sd_controller0.slot[1].ext_power.active_level = GPIO_HIGH;
	ambarella_platform_sd_controller0.slot[1].ext_power.active_delay = 300;
	ambarella_platform_sd_controller0.slot[1].cd_delay = 100;
	ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_gpio = GPIO(75);
	ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_line = gpio_to_irq(75);
	ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_type = IRQ_TYPE_EDGE_BOTH;
	ambarella_platform_sd_controller0.slot[1].gpio_wp.gpio_id = GPIO(76);

	/* Register devices*/
	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}

	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));

	ambarella_chacha_mt4d_board_info.irq =
		ambarella_board_generic.touch_panel_irq.irq_line;
	ambarella_chacha_mt4d_board_info.flags = I2C_M_PIN_MUXING;
	i2c_register_board_info(0, &ambarella_chacha_mt4d_board_info, 1);

	platform_device_register(&durian_board_input);
}

/* ==========================================================================*/
MACHINE_START(DURIAN, "Ambarella Durian")
	.phys_io	= AHB_START,
	.io_pg_offst	= ((AHB_BASE) >> 18) & 0xfffc,
	.map_io		= ambarella_map_io,
	.init_irq	= ambarella_init_irq,
	.timer		= &ambarella_timer,
	.init_machine	= ambarella_init_durian,
	.boot_params	= CONFIG_AMBARELLA_PARAMS_PHYS,
MACHINE_END

