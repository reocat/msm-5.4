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
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
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
	&ambarella_crypto,
	&ambarella_ehci0,
	&ambarella_ohci0,
	&ambarella_eth0,
	&ambarella_fb0,
	&ambarella_fb1,
//	&ambarella_i2s0,
//	&ambarella_pcm0,
//	&ambarella_dummy_codec0,
//	&ambarella_dummy_audio_device,
//	&ambarella_ambevk_audio_device,
	&ambarella_ir0,
	&ambarella_sd0,
	&ambarella_sd1,
	&ambarella_sd2,
	&ambarella_udc0,
	&ambarella_wdt0,
	&ambarella_dma,
	&ambarella_rtc0,
	&ambarella_spi0,
	&ambarella_spi1,
	&ambarella_pwm_backlight_device0,
	&ambarella_pwm_backlight_device1,
	&ambarella_pwm_backlight_device2,
	&ambarella_pwm_backlight_device3,
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
#if 0
	{
		.modalias	= "spidev",
		.bus_num	= 0,
		.chip_select	= 3,
	},

	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 0,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 1,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 2,
	},
	{
		.modalias	= "spidev",
		.bus_num	= 1,
		.chip_select	= 3,
	},
#endif
};


/* ==========================================================================*/
static struct ambarella_key_table generic_keymap[AMBINPUT_TABLE_SIZE] = {

	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_POWER,		1,		0x0100bcbd}}},	//POWER
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_VOLUMEUP,	1,		0x01000405}}},	//VOLUME_UP
	{AMBINPUT_IR_KEY,	{.ir_key	= {KEY_VOLUMEDOWN,	1,		0x01008485}}},	//VOLUME_DOWN

	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_RESERVED,	0,	1,	3100,	4095}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_MODE,		0,	1,	2700,	3100}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_DELETE,		0,	1,	1800,	2400}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_RECORD,		0,	1,	1000,	1350}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_HP,			0,	1,	300,		800}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_CAMERA,		0,	1,	0,		200}}},

	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_RESERVED,	0,	3,	3100,	4095}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_MENU,		0,	3,	2800,	3100}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_RIGHT,		0,	3,	2000,	2800}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_LEFT,			0,	3,	1000,	2000}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_DOWN,		0,	3,	400,		1000}}},
	{AMBINPUT_ADC_KEY,	{.adc_key	= {KEY_UP,			0,	3,	0,		400}}},

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

static void ixora_bub_sd_set_vdd(u32 vdd)
{
	pr_debug("%s = %dmV\n", __func__, vdd);
	ambarella_gpio_config(GPIO(0), GPIO_FUNC_SW_OUTPUT);
	if (vdd == 1800) {
		ambarella_gpio_set(GPIO(0), 1);
	} else {
		ambarella_gpio_set(GPIO(0), 0);
	}
	msleep(100);
}

static void ixora_bub_sd_set_bus_timing(u32 timing)
{
	u32 sd_phy_ctrl_0_reg;
	u32 sd_phy_ctrl_1_reg;

	pr_debug("%s = %d\n", __func__, timing);
	sd_phy_ctrl_0_reg = amba_rct_readl(RCT_REG(0x4C0));
	sd_phy_ctrl_1_reg = amba_rct_readl(RCT_REG(0x4C4));
	pr_debug("sd_phy_ctrl_0_reg = 0x%08X\n", sd_phy_ctrl_0_reg);
	pr_debug("sd_phy_ctrl_1_reg = 0x%08X\n", sd_phy_ctrl_1_reg);
	switch (timing) {
	case MMC_TIMING_LEGACY:
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
	case MMC_TIMING_UHS_SDR12:
	case MMC_TIMING_UHS_SDR25:
	case MMC_TIMING_UHS_SDR50:
		break;
	case MMC_TIMING_UHS_SDR104:
		break;
	case MMC_TIMING_UHS_DDR50:
		break;
	default:
		break;
	}
	pr_debug("sd_phy_ctrl_0_reg = 0x%08X\n", sd_phy_ctrl_0_reg);
	pr_debug("sd_phy_ctrl_1_reg = 0x%08X\n", sd_phy_ctrl_1_reg);
}

static void ixora_bub_sdio_set_vdd(u32 vdd)
{
	pr_debug("%s = %dmV\n", __func__, vdd);
	ambarella_gpio_config(GPIO(11), GPIO_FUNC_SW_OUTPUT);
	if (vdd == 1800) {
		ambarella_gpio_set(GPIO(11), 1);
	} else {
		ambarella_gpio_set(GPIO(11), 0);
	}
	msleep(100);
}

/* ==========================================================================*/
static void ixora_ipcam_sd_set_vdd(u32 vdd)
{
	pr_debug("%s = %dmV\n", __func__, vdd);
	ambarella_gpio_config(GPIO(0), GPIO_FUNC_SW_OUTPUT);
	if (vdd == 1800) {
		ambarella_gpio_set(GPIO(0), 0);
	} else {
		ambarella_gpio_set(GPIO(0), 1);
	}
	msleep(100);
}

static void __init ambarella_init_ixora_ipcam(void)
{
	ambarella_eth0_platform_info.mii_id = 3;
	ambarella_eth0_platform_info.phy_id = 0x00221560;
	ambarella_eth0_platform_info.phy_irq.irq_gpio = GPIO(10);
	ambarella_eth0_platform_info.phy_irq.irq_type = IRQF_TRIGGER_LOW;
	ambarella_eth0_platform_info.phy_irq.irq_gpio_val = GPIO_LOW;
	ambarella_eth0_platform_info.phy_irq.irq_gpio_mode = GPIO_FUNC_SW_INPUT;

	ambarella_platform_sd0_controller.max_clock = 48000000;
	ambarella_platform_sd0_controller.slot[0].private_caps |=
		(AMBA_SD_PRIVATE_CAPS_VDD_18 |
		AMBA_SD_PRIVATE_CAPS_ADMA);
	ambarella_platform_sd0_controller.slot[0].set_vdd =
		ixora_ipcam_sd_set_vdd;
	ambarella_platform_sd0_controller.slot[0].set_bus_timing =
		ixora_bub_sd_set_bus_timing;
	ambarella_platform_sd0_controller.slot[0].ext_power.gpio_id = EXT_GPIO(0);
	ambarella_platform_sd0_controller.slot[0].ext_power.active_level = GPIO_HIGH;
	ambarella_platform_sd0_controller.slot[0].ext_power.active_delay = 300;

	ambarella_platform_sd1_controller.max_clock = 24000000;
	ambarella_platform_sd1_controller.slot[0].private_caps |=
		(AMBA_SD_PRIVATE_CAPS_ADMA);
	ambarella_platform_sd1_controller.slot[0].ext_power.gpio_id = EXT_GPIO(1);
	ambarella_platform_sd1_controller.slot[0].ext_power.active_level = GPIO_HIGH;
	ambarella_platform_sd1_controller.slot[0].ext_power.active_delay = 300;
}

/* ==========================================================================*/
static void __init ambarella_init_ixora(void)
{
	int use_bub_default = 1;

	ambarella_init_machine("ixora", REF_CLK_FREQ);
#ifdef CONFIG_OUTER_CACHE
	ambcache_l2_enable();
#endif

	if (AMBARELLA_BOARD_TYPE(system_rev) == AMBARELLA_BOARD_TYPE_IPCAM) {
		switch (AMBARELLA_BOARD_REV(system_rev)) {
		case 'A':
			ambarella_init_ixora_ipcam();
			use_bub_default = 0;
			break;

		default:
			pr_warn("%s: Unknown IPCAM Rev[%d]\n", __func__,
				AMBARELLA_BOARD_REV(system_rev));
			break;
		}
	}

	if (use_bub_default) {
		/* Config USB over-curent protection */
		ambarella_board_generic.uhc_use_ocp = (0x1 << 16) | 0x3;

		ambarella_platform_sd0_controller.max_clock = 48000000;
		ambarella_platform_sd0_controller.slot[0].private_caps |=
			(AMBA_SD_PRIVATE_CAPS_VDD_18 |
			AMBA_SD_PRIVATE_CAPS_ADMA);
		ambarella_platform_sd0_controller.slot[0].set_vdd =
			ixora_bub_sd_set_vdd;
		ambarella_platform_sd0_controller.slot[0].set_bus_timing =
			ixora_bub_sd_set_bus_timing;

		ambarella_platform_sd1_controller.max_clock = 48000000;
		ambarella_platform_sd1_controller.slot[0].private_caps |=
			(AMBA_SD_PRIVATE_CAPS_VDD_18 |
			AMBA_SD_PRIVATE_CAPS_ADMA);
		ambarella_platform_sd1_controller.slot[0].set_vdd =
			ixora_bub_sdio_set_vdd;

		ambarella_platform_sd2_controller.max_clock = 12000000;
		ambarella_platform_sd2_controller.slot[0].default_caps |=
			(MMC_CAP_8_BIT_DATA | MMC_CAP_BUS_WIDTH_TEST);
		ambarella_platform_sd2_controller.slot[0].private_caps |=
			(AMBA_SD_PRIVATE_CAPS_ADMA);
	}

	ambarella_platform_ir_controller0.protocol = AMBA_IR_PROTOCOL_PANASONIC;
}

/* ==========================================================================*/
static void __init ambarella_init_ixora_dt(void)
{
	int i;

	ambarella_init_ixora();

	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);

	platform_add_devices(ixora_devices, ARRAY_SIZE(ixora_devices));
	for (i = 0; i < ARRAY_SIZE(ixora_devices); i++) {
		device_set_wakeup_capable(&ixora_devices[i]->dev, 1);
		device_set_wakeup_enable(&ixora_devices[i]->dev, 0);
	}

	i2c_register_board_info(0, ambarella_board_vin_infos,
		ARRAY_SIZE(ambarella_board_vin_infos));
	i2c_register_board_info(1, &ambarella_board_hdmi_info, 1);

#if defined(CONFIG_VOUT_CONVERTER_IT66121)
	ambarella_init_it66121(0, 0x98>>1);
#endif

	spi_register_board_info(ambarella_spi_devices,
		ARRAY_SIZE(ambarella_spi_devices));

	platform_device_register(&ixora_board_input);
}


static const char * const s2l_dt_board_compat[] = {
	"ambarella,s2l",
	"ambarella,hawthorn",
	"ambarella,ixora",
	NULL,
};

DT_MACHINE_START(IXORA_DT, "Ambarella S2L (Flattened Device Tree)")
	.restart_mode	=	's',
	.map_io		=	ambarella_map_io,
	.init_early	=	ambarella_init_early,
	.init_irq	=	irqchip_init,
	.init_time	=	ambarella_timer_init,
	.init_machine	=	ambarella_init_ixora_dt,
	.restart	=	ambarella_restart_machine,
	.dt_compat	=	s2l_dt_board_compat,
MACHINE_END

