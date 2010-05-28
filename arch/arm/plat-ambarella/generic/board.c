/*
 * arch/arm/plat-ambarella/generic/board.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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
#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mmc/host.h>
#include <linux/serial_core.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/i2c.h>

#include <asm/page.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>
#include <mach/gpio.h>

#include <linux/i2c/ak4183.h>
#include <linux/i2c/cy8ctmg.h>

#include <mach/board.h>
#include <plat/debug.h>

struct ambarella_board_info ambarella_board_generic = {
	.power_detect	= {
		.irq_gpio	= -1,
		.irq_line	= -1,
		.irq_type	= -1,
		.irq_gpio_val	= GPIO_LOW,
		.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
	},
	.power_control	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.debug_led0	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.rs485		= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.audio_codec	= {
		.reset_gpio	= -1,
		.reset_level	= GPIO_LOW,
		.reset_delay	= 1,
		.resume_delay	= 1,
	},
	.audio_speaker	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.audio_headphone	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.audio_microphone	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.tp_irq		= {
		.irq_gpio	= -1,
		.irq_line	= -1,
		.irq_type	= -1,
		.irq_gpio_val	= GPIO_LOW,
		.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
	},
	.tp_reset	= {
		.reset_gpio	= -1,
		.reset_level	= GPIO_LOW,
		.reset_delay	= 1,
		.resume_delay	= 1,
	},
	.lcd_backlight	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.vin_vsync	= {
		.irq_gpio	= -1,
		.irq_line	= -1,
		.irq_type	= -1,
		.irq_gpio_val	= GPIO_LOW,
		.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
	},
	.flash_charge_ready	= {
		.irq_gpio	= -1,
		.irq_line	= -1,
		.irq_type	= -1,
		.irq_gpio_val	= GPIO_LOW,
		.irq_gpio_mode	= GPIO_FUNC_SW_INPUT,
	},
	.flash_trigger	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
	.flash_enable	= {
		.power_gpio	= -1,
		.power_level	= GPIO_LOW,
		.power_delay	= 1,
	},
};
AMBA_BOARD_CALL(ambarella_board_generic, 0644);
EXPORT_SYMBOL(ambarella_board_generic);

