/*
 * arch/arm/mach-ambarella/init-elephant.c
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
#include <linux/i2c/pcf857x.h>
#include <linux/i2c/pca954x.h>

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
	&ambarella_ehci0,
	&ambarella_fb0,
	&ambarella_fb1,
	&ambarella_i2s0,
	&ambarella_idc0,
	&ambarella_idc1,
	&ambarella_ir0,
	&ambarella_ohci0,
	&ambarella_ahci0,
	&ambarella_rtc0,
	&ambarella_sd0,
	&ambarella_sd1,
	&ambarella_spi0,
	&ambarella_spi1,
	&ambarella_uart,
	&ambarella_uart1,
	&ambarella_udc0,
	&ambarella_wdt0,
	&ambarella_pwm_platform_device0,
	&ambarella_pwm_platform_device1,
	&ambarella_pwm_platform_device2,
	&ambarella_pwm_platform_device3,
	&ambarella_pwm_platform_device4,
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
static struct ambarella_key_table elephant_keymap[AMBINPUT_TABLE_SIZE] = {
	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_END},
};

static struct ambarella_input_board_info elephant_board_input_info = {
	.pkeymap		= elephant_keymap,
	.pinput_dev		= NULL,
	.pdev			= NULL,

	.abx_max_x		= 4095,
	.abx_max_y		= 4095,
	.abx_max_pressure	= 4095,
	.abx_max_width		= 16,
};

struct platform_device elephant_board_input = {
	.name		= "ambarella-input",
	.id		= -1,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &elephant_board_input_info,
		.dma_mask		= &ambarella_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	}
};

/* ==========================================================================*/
static struct pcf857x_platform_data elephant_board_ext_gpio0_pdata = {
	.gpio_base	= EXT_GPIO(0),
};

static struct i2c_board_info elephant_board_ext_gpio_info = {
	.type			= "pcf8574a",
	.addr			= 0x3e,
	.flags			= 0,
	.platform_data		= &elephant_board_ext_gpio0_pdata,
};

static struct pca954x_platform_mode elephant_board_ext_i2c_mode = {
	.adap_id		= 0,
	.deselect_on_exit	= 1,
};

static struct pca954x_platform_data elephant_board_ext_i2c_pdata = {
	.modes			= &elephant_board_ext_i2c_mode,
	.num_modes		= 0,
};

static struct i2c_board_info elephant_board_ext_i2c_info = {
	.type			= "pca9542",
	.addr			= 0x70,
	.flags			= 0,
	.platform_data		= &elephant_board_ext_i2c_pdata,
};

/* ==========================================================================*/
static void __init ambarella_init_elephant(void)
{
	int					i;

	ambarella_init_machine("Elephant");

	ambarella_board_generic.lcd_reset.gpio_id = GPIO(46);
	ambarella_board_generic.lcd_reset.active_level = GPIO_LOW;
	ambarella_board_generic.lcd_reset.active_delay = 1;

	ambarella_board_generic.touch_panel_irq.irq_gpio = GPIO(45);
	ambarella_board_generic.touch_panel_irq.irq_line = gpio_to_irq(45);
	ambarella_board_generic.touch_panel_irq.irq_type = IRQF_TRIGGER_FALLING;
	ambarella_board_generic.touch_panel_irq.irq_gpio_val = GPIO_LOW;
	ambarella_board_generic.touch_panel_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	/* Config SD*/
	fio_select_sdio_as_default = 0;
	ambarella_platform_sd_controller0.clk_limit = 12500000;
	ambarella_platform_sd_controller0.slot[0].use_bounce_buffer = 1;
	ambarella_platform_sd_controller0.slot[0].max_blk_sz = SD_BLK_SZ_512KB;
	ambarella_platform_sd_controller0.slot[0].cd_delay = 1000;
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio = GPIO(67);
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_line = gpio_to_irq(67);
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_type = IRQ_TYPE_EDGE_BOTH;
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio_val	= GPIO_LOW,
	ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
	ambarella_platform_sd_controller0.slot[0].gpio_wp.gpio_id = GPIO(68);
	ambarella_platform_sd_controller0.slot[1].cd_delay = 1000;
	ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_gpio = GPIO(75);
	ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_line = gpio_to_irq(75);
	ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_type = IRQ_TYPE_EDGE_BOTH;
	ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_gpio_val	= GPIO_LOW,
	ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
	ambarella_platform_sd_controller0.slot[1].gpio_wp.gpio_id = GPIO(76);
	ambarella_platform_sd_controller1.clk_limit = 25000000;
	ambarella_platform_sd_controller1.slot[0].cd_delay = 100;
	ambarella_platform_sd_controller1.slot[0].use_bounce_buffer = 1;
	ambarella_platform_sd_controller1.slot[0].max_blk_sz = SD_BLK_SZ_512KB;

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}

	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));

	ambarella_tm1510_board_info.irq =
		ambarella_board_generic.touch_panel_irq.irq_line;
	ambarella_tm1510_board_info.flags = I2C_M_PIN_MUXING;
	i2c_register_board_info(0, &ambarella_tm1510_board_info, 1);

	ambarella_board_vin_infos[0].flags = I2C_M_PIN_MUXING;
	ambarella_board_vin_infos[1].flags = I2C_M_PIN_MUXING;
	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);

	i2c_register_board_info(0, &elephant_board_ext_gpio_info, 1);
	i2c_register_board_info(0, &elephant_board_ext_i2c_info, 1);

	platform_device_register(&elephant_board_input);
}

/* ==========================================================================*/
MACHINE_START(ELEPHANT, "Elephant")
	.phys_io	= AHB_PHYS_BASE,
	.io_pg_offst	= ((AHB_BASE) >> 18) & 0xfffc,
	.boot_params	= CONFIG_AMBARELLA_PARAMS_PHYS,
	.map_io		= ambarella_map_io,
	.reserve	= ambarella_memblock_reserve,
	.init_irq	= ambarella_init_irq,
	.timer		= &ambarella_timer,
	.init_machine	= ambarella_init_elephant,
MACHINE_END

