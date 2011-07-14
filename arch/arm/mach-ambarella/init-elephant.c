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

#include <linux/mmc/host.h>

#include <linux/regulator/machine.h>
#include <linux/mfd/wm831x/core.h>
#include <linux/mfd/wm831x/pdata.h>
#include "board-device.h"

#include <linux/rfkill-gpio.h>


/* ==========================================================================*/
#include <linux/pda_power.h>
/* DCDC1: iOne_VDDAX for Cortex and 3D */
static struct regulator_consumer_supply dcdc1_consumers[] = {
	{
		.supply = "cpu_vcc",
	},
};

static struct regulator_init_data elephant_wm8310_dcdc1_data = {
	.constraints = {
		.name = "VDD_AXI_1.0V_1.4V",
		.min_uV = 1000000,
		.max_uV = 1400000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		.initial_state = PM_SUSPEND_ON,
		.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(dcdc1_consumers),
	 .consumer_supplies = dcdc1_consumers,
};

static struct regulator_consumer_supply dcdc2_consumers[] = {
	{
		.supply = "ddr3_vcc",
	},
};
/* DCDC2 iOne_D1P5 for DDR3 */
static struct regulator_init_data elephant_wm8310_dcdc2_data = {
	.constraints = {
		.name = "VDDQ_i1_0.6V_1.8",
		.min_uV = 600000,
		.max_uV = 1800000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		.initial_state = PM_SUSPEND_ON,
		.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(dcdc2_consumers),
	 .consumer_supplies = dcdc2_consumers,
};
static struct regulator_consumer_supply dcdc3_consumers[] = {
	{
		.supply = "gen_vcc",
	},
	{
		.supply = "lp_vcc",
	},
};
/* DCDC3 iOne_D3P15 for A3P15 or D3P15 */
static struct regulator_init_data elephant_wm8310_dcdc3_data = {
	.constraints = {
		.name = "VDD33_LP_AND_GEN_3.15V",
		.min_uV = 2800000,
		.max_uV = 3400000,
		.apply_uV = 0,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(dcdc3_consumers),
	.consumer_supplies = dcdc3_consumers,
};

static struct regulator_consumer_supply dcdc4_consumers[] = {
	{
		.supply = "lcd_vcc",
	},
};

static struct regulator_init_data elephant_wm8310_dcdc4_data = {
	.constraints = {
		.name = "VDD_LCD_6.5V_30V",
		.min_uV = 6500000,
		.max_uV = 30000000,
		.apply_uV = 0,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(dcdc4_consumers),
	 .consumer_supplies = dcdc4_consumers,
};

static struct regulator_consumer_supply ldo1_consumers[] = {
	{
		.supply = "mipi_vcc",
	},
};
/* LDO1 MIPI_PHY */
static struct regulator_init_data elephant_wm8310_ldo1_data = {
	.constraints = {
		.name = "VDD_MIPI_PHY_0.9V_3.3V",
		.min_uV = 900000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo1_consumers),
	 .consumer_supplies = ldo1_consumers,
};
static struct regulator_consumer_supply ldo2_consumers[] = {
	{
		.supply = "sensor_vcc",
	},
};
/* LDO2 SEN_VDD */
static struct regulator_init_data elephant_wm8310_ldo2_data = {
	.constraints = {
		.name = "VDD_SEN_0.9V_3.3V",
		.min_uV = 900000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo2_consumers),
	 .consumer_supplies = ldo2_consumers,
};
static struct regulator_consumer_supply ldo3_consumers[] = {
	{
		.supply = "audio_vcc",
	},
};
/* LDO3 Audio codec power */
static struct regulator_init_data elephant_wm8310_ldo3_data = {
	.constraints = {
		.name = "VDD_AUD_0.9V_3.3V",
		.min_uV = 900000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo3_consumers),
	 .consumer_supplies = ldo3_consumers,
};

static struct regulator_consumer_supply ldo4_consumers[] = {
	{
		.supply = "gyro_vcc",
	},
};
/* LDO4 gyro sensor */
static struct regulator_init_data elephant_wm8310_ldo4_data = {
	.constraints = {
		.name = "VDD_GY_0.9V_3.3V",
		.min_uV = 900000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo4_consumers),
	 .consumer_supplies = ldo4_consumers,
};
static struct regulator_consumer_supply ldo5_consumers[] = {
	{
		.supply = "sdxc_vcc",
	},
};
/* LDO5 VDD33_SDXC */
static struct regulator_init_data elephant_wm8310_ldo5_data = {
	.constraints = {
		.name = "VDD33_SDXC_0.9V_3.3V",
		/* can not get 3.15 but 3.1 or 3.2 */
		.min_uV = 900000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo5_consumers),
	 .consumer_supplies = ldo5_consumers,
};
static struct regulator_consumer_supply ldo6_consumers[] = {
	{
		.supply = "gsensor_vcc",
	},
};
/* LDO6 Gsensor */
static struct regulator_init_data elephant_wm8310_ldo6_data = {
	.constraints = {
		.name = "VDD_G_0.9V_3.3V",
		.min_uV = 900000,
		.max_uV = 3300000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo6_consumers),
	 .consumer_supplies = ldo6_consumers,
};
static struct regulator_consumer_supply ldo7_consumers[] = {
	{
		.supply = "analog_vcc",
	},
};
/* LDO7 VDDA_25_XX */
static struct regulator_init_data elephant_wm8310_ldo7_data = {
	.constraints = {
		.name = "VDDA_25_XXX_1V_3.5V",
		.min_uV = 1000000,
		.max_uV = 3500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo7_consumers),
	 .consumer_supplies = ldo7_consumers,
};
static struct regulator_consumer_supply ldo8_consumers[] = {
	{
		.supply = "imagesen_vcc",
	},
};
/* LDO8 Image sensor */
static struct regulator_init_data elephant_wm8310_ldo8_data = {
	.constraints = {
		.name = "VDD_IMG_SEN_1V_3.5",
		.min_uV = 1000000,
		.max_uV = 3500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo8_consumers),
	 .consumer_supplies = ldo8_consumers,
};
static struct regulator_consumer_supply ldo9_consumers[] = {
	{
		.supply = "bt_vcc",
	},
};
/* LDO9 VDD_BT */
static struct regulator_init_data elephant_wm8310_ldo9_data = {
	.constraints = {
		.name = "VDD_BT_1V_3.5V",
		.min_uV = 1000000,
		.max_uV = 3500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo9_consumers),
	 .consumer_supplies = ldo9_consumers,
};
static struct regulator_consumer_supply ldo10_consumers[] = {
	{
		.supply = "lcd_ctl_vcc",
	},
};
/* LDO10 VDD_LCD */
static struct regulator_init_data elephant_wm8310_ldo10_data = {
	.constraints = {
		.name = "VDD_LCD_1V_3.5V",
		.min_uV = 1000000,
		.max_uV = 3500000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
		.valid_modes_mask = REGULATOR_MODE_STANDBY | REGULATOR_MODE_NORMAL,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(ldo10_consumers),
	 .consumer_supplies = ldo10_consumers,
};
static struct regulator_consumer_supply lsink1_consumers[] = {
	{
		.supply = "lcd_bl_sink",
	},
};
/* ISINK1 backlight */
static struct regulator_init_data elephant_wm8310_isink1_data = {
	.constraints = {
		.name = "VDD_LCD_BL_20mA_28V",
		.min_uA = 19484,
		.max_uA = 19484,
		//.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = 1,
		},
		//.initial_state = PM_SUSPEND_MAX,
		//.always_on = 1,
		.boot_on = 1,
	},
	 .num_consumer_supplies = ARRAY_SIZE(lsink1_consumers),
	 .consumer_supplies = lsink1_consumers,
};
static struct wm831x_backlight_pdata elephant_wm8310_backlight_pdata = {
	.isink = 1,
	/* 20*2^(13.25) = 19484 */
	.max_uA = 19484,
};


static struct wm831x_battery_pdata elephant_wm8310_battery_pdata = {
	.enable = 1,
	.fast_enable	 = 1,
//	.off_mask = 1,
	.trickle_ilim = 50,
	.vsel = 4200,
	.eoc_iterm = 20,
	.fast_ilim = 450,
	.timeout = 360,
	.enable_fet = 1,
};

#define WM8310_GPIO_START EXT_GPIO(0)
static struct gpio elephant_wm8310_gpios[] = {
/* pmic gpio 0..5 can be controlled with OTP data */
#if 0
		{WM8310_GPIO_START + 0, GPIOF_OUT_INIT_HIGH, "VDD_EN1"},
		{WM8310_GPIO_START + 1, GPIOF_OUT_INIT_HIGH, "VDD_EN2"},
		{WM8310_GPIO_START + 2, GPIOF_OUT_INIT_HIGH, "VDD_EN3"},
		{WM8310_GPIO_START + 3, GPIOF_OUT_INIT_HIGH, "VDD_EN4"},
		{WM8310_GPIO_START + 4, GPIOF_OUT_INIT_HIGH, "VDD_EN5"},
		{WM8310_GPIO_START + 5, GPIOF_OUT_INIT_HIGH, "VDD_EN6"},
#endif
		{WM8310_GPIO_START + 6, GPIOF_OUT_INIT_HIGH, "VDD_EN7"},
		{WM8310_GPIO_START + 7, GPIOF_OUT_INIT_HIGH, "VDD_EN8"},
		{WM8310_GPIO_START + 8, GPIOF_OUT_INIT_HIGH, "VDD_EN9"},
};

static int elephant_wm8310_pre_init(struct wm831x *wm831x)
{
	int					ret = 0;

	if (ambarella_is_valid_gpio_irq(&ambarella_board_generic.pmic_irq)) {
		wm831x->irq = ambarella_board_generic.pmic_irq.irq_line;
		set_irq_wake(wm831x->irq, 1);
	} else {
		ret = -1;
	}

	return ret;
}

/* setup the gpio 1..9 (1 is the 1st one) as power supply by hw init/OTP */
/* we only check the gpio's high value */
static int elephant_wm8310_post_init(struct wm831x *wm831x)
{
	int i, ret = 0;
	if ((ret = gpio_request_array(elephant_wm8310_gpios, ARRAY_SIZE(elephant_wm8310_gpios)))) {
		printk("Error request gpio for wm8310 on iOne\n");
		return ret;
	};
	for (i = 0 ; i < ARRAY_SIZE(elephant_wm8310_gpios); i++) {
		gpio_set_value_cansleep(elephant_wm8310_gpios[i].gpio, 1);
		if (!gpio_get_value_cansleep(elephant_wm8310_gpios[i].gpio)) {
			printk("WARNING:wm8310 gpio[%d] can not be pulled high.\n",
				elephant_wm8310_gpios[i].gpio - WM8310_GPIO_START);
			/* we hope it will work anyway */
			//ret = -EINVAL;
		};
	}

	return ret;
}

static struct wm831x_pdata elephant_wm8310_pdata = {
	.pre_init = elephant_wm8310_pre_init,
	.post_init = elephant_wm8310_post_init,
	/* CIFMODE is pulled up to enable wm8310 spi mode with GPIO */
	.irq_base = EXT_IRQ(0),

	.gpio_base = EXT_GPIO(0),
	.backlight = &elephant_wm8310_backlight_pdata,

	.battery = &elephant_wm8310_battery_pdata,

	.dcdc = {
		&elephant_wm8310_dcdc1_data, /* DCDC1 */
		&elephant_wm8310_dcdc2_data, /* DCDC2 */
		&elephant_wm8310_dcdc3_data, /* DCDC3 */
		&elephant_wm8310_dcdc4_data, /* DCDC4 */
	},
	.ldo = {
		&elephant_wm8310_ldo1_data,
		&elephant_wm8310_ldo2_data,
		&elephant_wm8310_ldo3_data,
		&elephant_wm8310_ldo4_data,
		&elephant_wm8310_ldo5_data,
		&elephant_wm8310_ldo6_data,
		&elephant_wm8310_ldo7_data,
		&elephant_wm8310_ldo8_data,
		&elephant_wm8310_ldo9_data,
		&elephant_wm8310_ldo10_data,
	},

	.isink = {
		&elephant_wm8310_isink1_data, /* ISINK1 */
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
	&ambarella_pcm0,
	&ambarella_dummy_codec0,
	&ambarella_idc0,
	&ambarella_idc1,
	&ambarella_i2cmux,
	&ambarella_ir0,
	&ambarella_ohci0,
	&ambarella_ahci0,
	&ambarella_rtc0,
	&ambarella_sd0,
	&ambarella_sd1,
	&ambarella_spi0,
	&ambarella_spi1,
	&ambarella_spi2,
	&ambarella_spi3,
	&ambarella_spi4,
	&ambarella_uart,
	&ambarella_uart1,
	&ambarella_uart2,
	&ambarella_uart3,
	&ambarella_udc0,
	&ambarella_wdt0,
};

static struct platform_device *ambarella_pwm_devices[] __initdata = {
	&ambarella_pwm_platform_device0,
	&ambarella_pwm_platform_device1,
	&ambarella_pwm_platform_device2,
	&ambarella_pwm_platform_device3,
	&ambarella_pwm_platform_device4,
};

/* ==========================================================================*/
static struct spi_board_info ambarella_spi_devices[] = {
	[0] = {
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 0,
	},
	[1] = {
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 1,
	},
	[2] = {
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 2,
	},
	[3] = {
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 3,
	},
	[4] = {
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 4,
	},
	[5] = {
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 5,
	},
	[6] = {
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 6,
	},
	[7] = {
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 7,
	},
	[8] = {
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 0,
	},
	[9] = {
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 0,
	},
	[10] = {
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 1,
	},
	[11] = {
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 2,
	},
	[12] = {
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 3,
	},
	[13] = {
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 4,
	},
	[14] = {
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 5,
	},
	[15] = {
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 6,
	},
	[16] = {
		.modalias	= "spidev",
		.bus_num	= 2,
		.chip_select	= 7,
	},
	[17] = {
		.modalias	= "spidev",
		.bus_num	= 3,
		.chip_select	= 0,
	},
	[18] = {
		.modalias	= "spidev",
		.bus_num	= 4,
		.chip_select	= 0,
	}
};

/* ==========================================================================*/
static struct ambarella_key_table elephant_keymap[AMBINPUT_TABLE_SIZE] = {
	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_POWER,	3,	0x001a0015}}},	//POWER
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_HOME,	3,	0x001a0061}}},	//HOME
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_MENU,	3,	0x001a004d}}},	//MENU
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_ESC,	3,	0x001a0063}}},	//BACK
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_SEND,	3,	0x001a0010}}},	//CALL
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_END,	3,	0x001a0020}}},	//ENDCALL
	{AMBINPUT_IR_SW,	{.ir_key	= {0,		0,	0x001a003d}}},	//SW0 OFF
	{AMBINPUT_IR_SW,	{.ir_key	= {0,		1,	0x001a0064}}},	//SW0 ON
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_VOLUMEUP,3,	0x001a0077}}},	//VOLUME_UP
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_VOLUMEDOWN,3,	0x001a0078}}},	//VOLUME_DOWN
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_OK,	3,	0x001a0076}}},	//DPAD_CENTER
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_S,	3,	0x001a0073}}},	//S
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_W,	3,	0x001a0072}}},	//W
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_A,	3,	0x001a0075}}},	//A
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_D,	3,	0x001a0074}}},	//D
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_SEARCH,	3,	0x001a007e}}},	//SEARCH

	{AMBINPUT_GPIO_KEY,	{.gpio_key	= {KEY_HOME,	1,	1,	GPIO(120),	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING}}},
	{AMBINPUT_GPIO_KEY,	{.gpio_key	= {KEY_MENU,	1,	1,	GPIO(121),	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING}}},
	{AMBINPUT_GPIO_KEY,	{.gpio_key	= {KEY_ESC,	1,	1,	GPIO(122),	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING}}},
	{AMBINPUT_GPIO_KEY,	{.gpio_key	= {KEY_POWER,	1,	1,	GPIO(123),	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING}}},

	{AMBINPUT_END},
};

static struct ambarella_key_table elephant_keymap_vendor1[AMBINPUT_TABLE_SIZE] = {
	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_GPIO_KEY,	{.gpio_key	= {KEY_ESC,	0,	1,	GPIO(186),	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING}}},

	{AMBINPUT_END},
};

static struct ambarella_key_table elephant_keymap_evk[AMBINPUT_TABLE_SIZE] = {
	{AMBINPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBINPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBINPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},
	{AMBINPUT_VI_SW,	{.vi_sw		= {0,	0,	0}}},

	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_RESERVED,	0,	2,	2500,	4095}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_HOME,		0,	2,	1750,	2050}}},	//HOME
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_MENU,		0,	2,	1350,	1650}}},	//MENU
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_ESC,		0,	2,	980,	1280}}},	//ESC
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_SEARCH,		0,	2,	600,	900}}},		//SEARCH

	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_RESERVED,	0,	4,	2500,	4095}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_VOLUMEDOWN,	0,	4,	1800,	2100}}},	//VOLUME DOWN
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_VOLUMEUP,	0,	4,	1320,	1620}}},	//VOLUME UP
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_STOP,		0,	4,	820,	1120}}},	//STOP
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_RECORD,		0,	4,	310,	610}}},		//RECORD

	//{AMBINPUT_GPIO_KEY,	{.gpio_key	= {KEY_UNKNOWN,		1,	1,	GPIO(11),	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING}}},

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

/* ==========================================================================*/
static void __init ambarella_init_elephant(void)
{
	int					i, ret;
	int					use_bub_default = 1;

	ambarella_init_machine("Elephant");

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_EVK) {
		switch (AMBARELLA_BOARD_REV(system_rev)) {
		case 'B':
			ambarella_platform_sd_controller1.slot[0].ext_power.gpio_id = GPIO(111);
			ambarella_platform_sd_controller1.slot[0].ext_power.active_level = GPIO_HIGH;
			ambarella_platform_sd_controller1.slot[0].ext_power.active_delay = 300;
		 	/* the cs_pin of spi0.1 is used to determine wm8994's
		 	 * I2C address, and the cs_pin of spi0.4, spi0,5, spi0.6
		 	 * spi0.7 are used as I2S signals, so we need to prevent
		 	 * them from be modified by SPI driver */
			ambarella_spi0_cs_pins[1] = -1;
			ambarella_spi0_cs_pins[4] = -1;
			ambarella_spi0_cs_pins[5] = -1;
			ambarella_spi0_cs_pins[6] = -1;
			ambarella_spi0_cs_pins[7] = -1;
			ambarella_init_wm8994();
		case 'A':
			ambarella_board_generic.lcd_reset.gpio_id = GPIO(105);
			ambarella_board_generic.lcd_reset.active_level = GPIO_LOW;
			ambarella_board_generic.lcd_reset.active_delay = 1;

			ambarella_board_generic.lcd_backlight.gpio_id = GPIO(16);
			ambarella_board_generic.lcd_backlight.active_level = GPIO_HIGH;
			ambarella_board_generic.lcd_backlight.active_delay = 1;

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

			ambarella_board_generic.power_control.gpio_id = GPIO(120);
			ambarella_board_generic.power_control.active_level = GPIO_LOW;

			memcpy(ambarella_spi_devices[12].modalias, "wm8310", 6);
			ambarella_spi_devices[12].max_speed_hz = 500000;
			ambarella_spi_devices[12].platform_data = &elephant_wm8310_pdata;

			ambarella_board_generic.gsensor_power.gpio_id = GPIO(151);
			ambarella_board_generic.gsensor_power.active_level = GPIO_HIGH;
			ambarella_board_generic.gsensor_power.active_delay = 10;

			ambarella_board_generic.gsensor_irq.irq_gpio = GPIO(49);
			ambarella_board_generic.gsensor_irq.irq_line = gpio_to_irq(GPIO(49));
			ambarella_board_generic.gsensor_irq.irq_type = IRQF_TRIGGER_RISING;
			ambarella_board_generic.gsensor_irq.irq_gpio_val = GPIO_LOW;
			ambarella_board_generic.gsensor_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

			ambarella_eth0_platform_info.mii_id = 0;
			ambarella_eth0_platform_info.phy_id = 0x001cc912;
			ambarella_eth0_platform_info.phy_irq.irq_gpio = GPIO(21);
			ambarella_eth0_platform_info.phy_irq.irq_line = gpio_to_irq(GPIO(21));
			ambarella_eth0_platform_info.phy_irq.irq_type = IRQF_TRIGGER_LOW;
			ambarella_eth0_platform_info.phy_irq.irq_gpio_val = GPIO_LOW;
			ambarella_eth0_platform_info.phy_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;
			ambarella_eth0_platform_info.mii_power.gpio_id = GPIO(97);
			ambarella_eth0_platform_info.mii_power.active_level = GPIO_HIGH;
			ambarella_eth0_platform_info.mii_power.active_delay = 20;
			ambarella_eth0_platform_info.mii_reset.gpio_id = GPIO(98);
			ambarella_eth0_platform_info.mii_reset.active_level = GPIO_LOW;
			ambarella_eth0_platform_info.mii_reset.active_delay = 20;

			fio_default_owner = SELECT_FIO_SDIO;
			ambarella_platform_sd_controller0.clk_limit = 24000000;
			ambarella_platform_sd_controller0.slot[0].use_bounce_buffer = 1;
			ambarella_platform_sd_controller0.slot[0].caps |= (MMC_CAP_8_BIT_DATA | MMC_CAP_NONREMOVABLE);
			ambarella_platform_sd_controller0.slot[0].max_blk_sz = SD_BLK_SZ_128KB;
			ambarella_platform_sd_controller0.slot[0].cd_delay = 100;
			ambarella_platform_sd_controller0.slot[0].fixed_cd = 1;
			ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio = -1;
			ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_line = -1;
			ambarella_platform_sd_controller0.slot[0].fixed_wp = 0;
			ambarella_platform_sd_controller0.slot[0].gpio_wp.gpio_id = -1;
			ambarella_platform_sd_controller0.slot[0].ext_power.gpio_id = GPIO(157);
			ambarella_platform_sd_controller0.slot[0].ext_power.active_level = GPIO_HIGH;
			ambarella_platform_sd_controller0.slot[0].ext_power.active_delay = 300;
			ambarella_platform_sd_controller0.slot[1].use_bounce_buffer = 1;
			ambarella_platform_sd_controller0.slot[1].caps |= (MMC_CAP_NONREMOVABLE);
			ambarella_platform_sd_controller0.slot[1].max_blk_sz = SD_BLK_SZ_128KB;
			ambarella_platform_sd_controller0.slot[1].cd_delay = 100;
			ambarella_platform_sd_controller0.slot[1].fixed_cd = 0;
			ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_gpio = -1;
			ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_line = -1;
			ambarella_platform_sd_controller0.slot[1].fixed_wp = 0;
			ambarella_platform_sd_controller0.slot[1].gpio_wp.gpio_id = -1;
			ambarella_platform_sd_controller1.clk_limit = 24000000;
			ambarella_platform_sd_controller1.slot[0].use_bounce_buffer = 1;
			ambarella_platform_sd_controller1.slot[0].caps |= (MMC_CAP_8_BIT_DATA | MMC_CAP_BUS_WIDTH_TEST);
			ambarella_platform_sd_controller1.slot[0].max_blk_sz = SD_BLK_SZ_128KB;
			ambarella_platform_sd_controller1.slot[0].cd_delay = 100;

			ambarella_tm1726_board_info.irq = ambarella_board_generic.touch_panel_irq.irq_line;
			i2c_register_board_info(2, &ambarella_tm1726_board_info, 1);

			ambarella_board_generic.vin_power.gpio_id = GPIO(122);
			ambarella_board_generic.vin_power.active_level = GPIO_HIGH;
			ambarella_board_generic.vin_power.active_delay = 1;
			i2c_register_board_info(0, ambarella_board_vin_infos, ARRAY_SIZE(ambarella_board_vin_infos));

			platform_device_register(&elephant_bt_rfkill);

			elephant_board_input_info.pkeymap = elephant_keymap_evk;

			use_bub_default = 0;
			break;

		default:
			pr_warn("%s: Unknown EVK Rev[%d]\n", __func__, AMBARELLA_BOARD_REV(system_rev));
			break;
		}
	}

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_VENDOR) {
		switch (AMBARELLA_BOARD_REV(system_rev)) {
		case 2:
			ambarella_board_generic.wifi_power.gpio_id = GPIO(128);
			ambarella_board_generic.wifi_power.active_level = GPIO_HIGH;
			ambarella_board_generic.wifi_power.active_delay = 300;
			/* sdio slot */
			ambarella_board_generic.wifi_sd_bus = 0;
			ambarella_board_generic.wifi_sd_slot = 1;
		case 1:
			ambarella_board_generic.lcd_power.gpio_id = GPIO(41);
			ambarella_board_generic.lcd_power.active_level = GPIO_HIGH;
			ambarella_board_generic.lcd_power.active_delay = 1;

			ambarella_board_generic.lcd_reset.gpio_id = GPIO(32);
			ambarella_board_generic.lcd_reset.active_level = GPIO_LOW;
			ambarella_board_generic.lcd_reset.active_delay = 1;

			ambarella_board_generic.lcd_backlight.gpio_id = GPIO(16);
			ambarella_board_generic.lcd_backlight.active_level = GPIO_HIGH;
			ambarella_board_generic.lcd_backlight.active_delay = 1;

			ambarella_board_generic.touch_panel_reset.gpio_id = GPIO(29);
			ambarella_board_generic.touch_panel_reset.active_level = GPIO_LOW;
			ambarella_board_generic.touch_panel_reset.active_delay = 1;

			ambarella_board_generic.touch_panel_irq.irq_gpio = GPIO(51);
			ambarella_board_generic.touch_panel_irq.irq_line = gpio_to_irq(51);
			ambarella_board_generic.touch_panel_irq.irq_type = IRQF_TRIGGER_FALLING;
			ambarella_board_generic.touch_panel_irq.irq_gpio_val = GPIO_LOW;
			ambarella_board_generic.touch_panel_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

			fio_default_owner = SELECT_FIO_SDIO;
			ambarella_platform_sd_controller0.clk_limit = 24000000;
			ambarella_platform_sd_controller0.slot[0].use_bounce_buffer = 1;
			ambarella_platform_sd_controller0.slot[0].max_blk_sz = SD_BLK_SZ_128KB;
			ambarella_platform_sd_controller0.slot[0].cd_delay = 100;
			ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio = GPIO(67);
			ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_line = gpio_to_irq(67);
			ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_type = IRQ_TYPE_EDGE_BOTH;
			ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio_val = GPIO_LOW;
			ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio_mode = GPIO_FUNC_SW_INPUT;
			ambarella_platform_sd_controller0.slot[0].gpio_wp.gpio_id = GPIO(68);
			ambarella_platform_sd_controller0.slot[1].use_bounce_buffer = 1;
			ambarella_platform_sd_controller0.slot[1].max_blk_sz = SD_BLK_SZ_128KB;
			ambarella_platform_sd_controller0.slot[1].cd_delay = 100;
			ambarella_platform_sd_controller0.slot[1].fixed_cd = 1;
			ambarella_platform_sd_controller0.slot[1].fixed_wp = 0;
			ambarella_platform_sd_controller1.clk_limit = 24000000;
			ambarella_platform_sd_controller1.slot[0].cd_delay = 100;
			ambarella_platform_sd_controller1.slot[0].use_bounce_buffer = 1;
			ambarella_platform_sd_controller1.slot[0].max_blk_sz = SD_BLK_SZ_128KB;

			ambarella_nt11001_board_info.irq = ambarella_board_generic.touch_panel_irq.irq_line;
			ambarella_nt11001_board_info.flags = 0;
			i2c_register_board_info(0, &ambarella_nt11001_board_info, 1);

			elephant_board_input_info.pkeymap = elephant_keymap_vendor1;

			use_bub_default = 0;
			break;

		default:
			pr_warn("%s: Unknown VENDOR Rev[%d]\n", __func__, AMBARELLA_BOARD_REV(system_rev));
			break;
		}
	}

	if (use_bub_default) {
		pr_info("%s: Default BUB config:\n", __func__);
		ambarella_board_generic.lcd_reset.gpio_id = GPIO(46);
		ambarella_board_generic.lcd_reset.active_level = GPIO_LOW;
		ambarella_board_generic.lcd_reset.active_delay = 1;

		ambarella_board_generic.touch_panel_irq.irq_gpio = GPIO(45);
		ambarella_board_generic.touch_panel_irq.irq_line = gpio_to_irq(45);
		ambarella_board_generic.touch_panel_irq.irq_type = IRQF_TRIGGER_FALLING;
		ambarella_board_generic.touch_panel_irq.irq_gpio_val = GPIO_LOW;
		ambarella_board_generic.touch_panel_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

		ambarella_eth0_platform_info.mii_id = 0;
		ambarella_eth0_platform_info.phy_id = 0x001cc912;

		fio_default_owner = SELECT_FIO_SDIO;
		ambarella_platform_sd_controller0.clk_limit = 24000000;
		ambarella_platform_sd_controller0.slot[0].use_bounce_buffer = 1;
		ambarella_platform_sd_controller0.slot[0].max_blk_sz = SD_BLK_SZ_128KB;
		ambarella_platform_sd_controller0.slot[0].cd_delay = 100;
		ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio = GPIO(67);
		ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_line = gpio_to_irq(67);
		ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_type = IRQ_TYPE_EDGE_BOTH;
		ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio_val = GPIO_LOW;
		ambarella_platform_sd_controller0.slot[0].gpio_cd.irq_gpio_mode = GPIO_FUNC_SW_INPUT;
		ambarella_platform_sd_controller0.slot[0].gpio_wp.gpio_id = GPIO(68);
		ambarella_platform_sd_controller0.slot[1].use_bounce_buffer = 1;
		ambarella_platform_sd_controller0.slot[1].max_blk_sz = SD_BLK_SZ_128KB;
		ambarella_platform_sd_controller0.slot[1].cd_delay = 100;
		ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_gpio = GPIO(75);
		ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_line = gpio_to_irq(75);
		ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_type = IRQ_TYPE_EDGE_BOTH;
		ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_gpio_val = GPIO_LOW;
		ambarella_platform_sd_controller0.slot[1].gpio_cd.irq_gpio_mode = GPIO_FUNC_SW_INPUT;
		ambarella_platform_sd_controller0.slot[1].gpio_wp.gpio_id = GPIO(76);
		ambarella_platform_sd_controller1.clk_limit = 24000000;
		ambarella_platform_sd_controller1.slot[0].cd_delay = 100;
		ambarella_platform_sd_controller1.slot[0].use_bounce_buffer = 1;
		ambarella_platform_sd_controller1.slot[0].max_blk_sz = SD_BLK_SZ_128KB;
		ambarella_platform_sd_controller1.slot[0].ext_power.gpio_id = GPIO(106);
		ambarella_platform_sd_controller1.slot[0].ext_power.active_level = GPIO_HIGH;
		ambarella_platform_sd_controller1.slot[0].ext_power.active_delay = 300;

		ambarella_tm1510_board_info.irq = ambarella_board_generic.touch_panel_irq.irq_line;
		i2c_register_board_info(2, &ambarella_tm1510_board_info, 1);

		i2c_register_board_info(0, &elephant_board_ext_gpio_info, 1);
		i2c_register_board_info(0, &elephant_board_ext_i2c_info, 1);

		i2c_register_board_info(2, ambarella_board_vin_infos, ARRAY_SIZE(ambarella_board_vin_infos));
	}

	ambarella_platform_ir_controller0.protocol = AMBA_IR_PROTOCOL_SONY;

	platform_add_devices(ambarella_devices, ARRAY_SIZE(ambarella_devices));
	for (i = 0; i < ARRAY_SIZE(ambarella_devices); i++) {
		device_set_wakeup_capable(&ambarella_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_devices[i]->dev, 0);
	}

	for (i = 0; i < ARRAY_SIZE(ambarella_pwm_devices); i++) {
		if (i == 1
			&& AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_BUB)
			continue;
		if (i == 2
			&& AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_VENDOR
			&& AMBARELLA_BOARD_REV(system_rev) == 2)
			continue;
		if (i == 2
			&& AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_BUB)
			continue;
		if (i == 4
			&& AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_EVK)
			continue;
		ret = platform_device_register(ambarella_pwm_devices[i]);
		if (ret)
			continue;
		device_set_wakeup_capable(&ambarella_pwm_devices[i]->dev, 1);
		device_set_wakeup_enable(&ambarella_pwm_devices[i]->dev, 0);
	}

	spi_register_board_info(ambarella_spi_devices, ARRAY_SIZE(ambarella_spi_devices));

	platform_device_register(&elephant_board_input);

	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);
}

/* ==========================================================================*/
MACHINE_START(ELEPHANT, "Elephant")
	.boot_params	= CONFIG_AMBARELLA_PARAMS_PHYS,
	.map_io		= ambarella_map_io,
	.reserve	= ambarella_memblock_reserve,
	.init_irq	= ambarella_init_irq,
	.timer		= &ambarella_timer,
	.init_machine	= ambarella_init_elephant,
MACHINE_END

