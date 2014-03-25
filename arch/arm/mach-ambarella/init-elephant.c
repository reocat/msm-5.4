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
#include <linux/irqchip.h>

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
#include <linux/i2c/pcf857x.h>
#include <linux/i2c/pca954x.h>

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/input.h>
#include <linux/mmc/host.h>
#include <plat/ambcache.h>

#include "board-device.h"

#include <plat/dma.h>
#include <linux/rfkill-gpio.h>

#include <linux/mfd/wm831x/pdata.h>
/* ==========================================================================*/
#include <linux/pda_power.h>

static struct platform_device *ambarella_devices[] __initdata = {
	&ambarella_idc0_mux,
	&ambarella_ahci0,
};

/* ==========================================================================*/

static struct rfkill_gpio_platform_data elephant_board_bt_info = {
	.name		= "bt-gpio",
	.reset_gpio	= GPIO(190),
	.shutdown_gpio	= -1,
	.type		= RFKILL_TYPE_BLUETOOTH,
};

static struct platform_device elephant_bt_rfkill = {
	.name		= "rfkill_gpio",
	.id		= -1,
	.resource	= NULL,
	.num_resources	= 0,
	.dev		= {
		.platform_data		= &elephant_board_bt_info,
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

/*==========================================================================*/
static void elephant_ipcam_nvr_sdxc_set_vdd(u32 vdd)
{
	pr_debug("%s = %dmV\n", __func__, vdd);
	ambarella_gpio_config(GPIO(23), GPIO_FUNC_SW_OUTPUT);
	if (vdd == 1800) {
		ambarella_gpio_set(GPIO(23), 0);
	} else {
		ambarella_gpio_set(GPIO(23), 1);
	}
	msleep(10);
}

/* ==========================================================================*/
static void __init ambarella_init_elephant(void)
{
	int					i, ret;
	int					use_bub_default = 1;
	int					use_ambarella_rtc0 = 1;
	int					use_ambarella_wdt0 = 1;

	ambarella_init_machine("Elephant", REF_CLK_FREQ);

	/* Config SD */
	fio_default_owner = SELECT_FIO_SDIO;

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_EVK) {
		switch (AMBARELLA_BOARD_REV(system_rev)) {
		case 'C':
		case 'B':
#if defined(CONFIG_CODEC_AMBARELLA_WM8994)
			ambarella_init_wm8994();
#endif
		case 'A':
			ambarella_board_generic.lcd_reset.gpio_id = GPIO(105);
			ambarella_board_generic.lcd_reset.active_level = GPIO_LOW;
			ambarella_board_generic.lcd_reset.active_delay = 10;

			ambarella_board_generic.lcd_backlight.gpio_id = GPIO(16);
			ambarella_board_generic.lcd_backlight.active_level = GPIO_HIGH;
			ambarella_board_generic.lcd_backlight.active_delay = 1;

			ambarella_board_generic.lcd_spi_hw.bus_id = 2;
			ambarella_board_generic.lcd_spi_hw.cs_id = 0;

			ambarella_board_generic.lcd_spi_cfg.spi_mode = 0;
			ambarella_board_generic.lcd_spi_cfg.cfs_dfs = 16;
			ambarella_board_generic.lcd_spi_cfg.baud_rate = 5000000;
			ambarella_board_generic.lcd_spi_cfg.cs_change = 0;

			ambarella_board_generic.touch_panel_irq.irq_gpio = GPIO(44);
			ambarella_board_generic.touch_panel_irq.irq_line = gpio_to_irq(44);
			ambarella_board_generic.touch_panel_irq.irq_type = IRQF_TRIGGER_FALLING;
			ambarella_board_generic.touch_panel_irq.irq_gpio_val = GPIO_LOW;
			ambarella_board_generic.touch_panel_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

			ambarella_board_generic.hdmi_extpower.gpio_id = GPIO(104);
			ambarella_board_generic.hdmi_extpower.active_level = GPIO_HIGH;
			ambarella_board_generic.hdmi_extpower.active_delay = 1;

			ambarella_board_generic.wifi_power.gpio_id = GPIO(109);
			ambarella_board_generic.wifi_power.active_level = GPIO_HIGH;
			ambarella_board_generic.wifi_power.active_delay = 300;
			ambarella_board_generic.wifi_sd_bus = 0;
			ambarella_board_generic.wifi_sd_slot = 1;

			ambarella_board_generic.pmic_irq.irq_gpio = GPIO(54);
			ambarella_board_generic.pmic_irq.irq_line = gpio_to_irq(54);
			ambarella_board_generic.pmic_irq.irq_type = IRQF_TRIGGER_FALLING;
			ambarella_board_generic.pmic_irq.irq_gpio_val = GPIO_LOW;
			ambarella_board_generic.pmic_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

			ambarella_board_generic.sata_power.gpio_id = GPIO(101);
			ambarella_board_generic.sata_power.active_level = GPIO_HIGH;
			ambarella_board_generic.sata_power.active_delay = 1;

			ambarella_board_generic.power_control.gpio_id = GPIO(120);
			ambarella_board_generic.power_control.active_level = GPIO_LOW;
#if defined(CONFIG_PMIC_AMBARELLA_WM831X)
			use_ambarella_rtc0 = 0;
			use_ambarella_wdt0= 0;
#endif
			ambarella_board_generic.gsensor_power.gpio_id = GPIO(151);
			ambarella_board_generic.gsensor_power.active_level = GPIO_HIGH;
			ambarella_board_generic.gsensor_power.active_delay = 10;

			ambarella_board_generic.gsensor_irq.irq_gpio = GPIO(49);
			ambarella_board_generic.gsensor_irq.irq_line = gpio_to_irq(GPIO(49));
			ambarella_board_generic.gsensor_irq.irq_type = IRQF_TRIGGER_RISING;
			ambarella_board_generic.gsensor_irq.irq_gpio_val = GPIO_LOW;
			ambarella_board_generic.gsensor_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

#if defined(CONFIG_TOUCH_AMBARELLA_TM1726)
			ambarella_tm1726_board_info.irq = ambarella_board_generic.touch_panel_irq.irq_line;
			i2c_register_board_info(2, &ambarella_tm1726_board_info, 1);
#endif
			//vin0: Rear Sensor vin1: Front Sensor
			ambarella_board_generic.vin0_power.gpio_id = GPIO(122);
			ambarella_board_generic.vin0_power.active_level = GPIO_HIGH;
			ambarella_board_generic.vin0_power.active_delay = 1;

			ambarella_board_generic.vin1_power.gpio_id = GPIO(121);
			ambarella_board_generic.vin1_power.active_level = GPIO_HIGH;
			ambarella_board_generic.vin1_power.active_delay = 1;

			ambarella_board_generic.vin1_reset.gpio_id = GPIO(123);
			ambarella_board_generic.vin1_reset.active_level = GPIO_LOW;
			ambarella_board_generic.vin1_reset.active_delay = 1;

			ambarella_board_generic.vin1_af_power.gpio_id = GPIO(124);
			ambarella_board_generic.vin1_af_power.active_level = GPIO_HIGH;
			ambarella_board_generic.vin1_af_power.active_delay = 1;

			platform_device_register(&elephant_bt_rfkill);
			platform_device_register(&i1evk_cpufreq_device);

			ambarella_board_generic.pwm0_config.max_duty = 255;
			ambarella_board_generic.pwm0_config.default_duty = 255;

			use_bub_default = 0;
			break;

		default:
			pr_warn("%s: Unknown EVK Rev[%d]\n", __func__, AMBARELLA_BOARD_REV(system_rev));
			break;
		}
	} else if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_IPCAM) {
		ambarella_board_generic.sata_power.gpio_id = GPIO(122);
		ambarella_board_generic.sata_power.active_level = GPIO_HIGH;
		ambarella_board_generic.sata_power.active_delay = 1;

		ambarella_board_generic.hdmi_extpower.gpio_id = GPIO(104);
		ambarella_board_generic.hdmi_extpower.active_level = GPIO_HIGH;
		ambarella_board_generic.hdmi_extpower.active_delay = 1;

		use_bub_default = 0;
	}

	if (use_bub_default) {
		pr_info("%s: Default BUB config:\n", __func__);
		ambarella_board_generic.lcd_reset.gpio_id = GPIO(46);
		ambarella_board_generic.lcd_reset.active_level = GPIO_LOW;
		ambarella_board_generic.lcd_reset.active_delay = 1;

		ambarella_board_generic.lcd_spi_hw.bus_id = 3;
		ambarella_board_generic.lcd_spi_hw.cs_id = 0;

		ambarella_board_generic.touch_panel_irq.irq_gpio = GPIO(45);
		ambarella_board_generic.touch_panel_irq.irq_line = gpio_to_irq(45);
		ambarella_board_generic.touch_panel_irq.irq_type = IRQF_TRIGGER_FALLING;
		ambarella_board_generic.touch_panel_irq.irq_gpio_val = GPIO_LOW;
		ambarella_board_generic.touch_panel_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

		ambarella_board_generic.vin0_reset.gpio_id = GPIO(12);
		ambarella_board_generic.vin0_reset.active_level = GPIO_LOW;
		ambarella_board_generic.vin0_reset.active_delay = 100;

#if defined(CONFIG_TOUCH_AMBARELLA_TM1510)
		ambarella_tm1510_board_info.irq = ambarella_board_generic.touch_panel_irq.irq_line;
		i2c_register_board_info(2, &ambarella_tm1510_board_info, 1);
#endif

		i2c_register_board_info(0, &elephant_board_ext_gpio_info, 1);
		i2c_register_board_info(0, &elephant_board_ext_i2c_info, 1);
	}

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}
}

/* ==========================================================================*/
MACHINE_START(ELEPHANT, "Elephant")
	.atag_offset	= 0x100,
	.restart_mode	= 's',
	.smp		= smp_ops(ambarella_smp_ops),
	.map_io		= ambarella_map_io,
	.init_irq	= irqchip_init,
	.init_time	= ambarella_timer_init,
	.init_machine	= ambarella_init_elephant,
	.restart	= ambarella_restart_machine,
MACHINE_END

