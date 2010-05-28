/*
 * arch/arm/plat-ambarella/include/mach/board.h
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

#ifndef __PLAT_AMBARELLA_BOARD_H
#define __PLAT_AMBARELLA_BOARD_H

/* ==========================================================================*/

/* ==========================================================================*/
#ifndef __ASSEMBLER__

struct ambarella_board_info {
	struct ambarella_gpio_irq_info		power_detect;
	struct ambarella_gpio_power_info	power_control;
	struct ambarella_gpio_power_info	debug_led0;
	struct ambarella_gpio_power_info	rs485;

	struct ambarella_gpio_reset_info	audio_codec;
	struct ambarella_gpio_power_info	audio_speaker;
	struct ambarella_gpio_power_info	audio_headphone;
	struct ambarella_gpio_power_info	audio_microphone;

	struct ambarella_gpio_irq_info		tp_irq;
	struct ambarella_gpio_reset_info	tp_reset;
	struct ambarella_gpio_power_info	lcd_backlight;

	struct ambarella_gpio_irq_info		vin_vsync;

	struct ambarella_gpio_irq_info		flash_charge_ready;
	struct ambarella_gpio_power_info	flash_trigger;
	struct ambarella_gpio_power_info	flash_enable;
};
#define AMBA_BOARD_CALL(arg, perm) \
	AMBA_GPIO_IRQ_MODULE_PARAM_CALL(board_##power_detect_, arg.power_detect, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##power_control_, arg.power_control, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##debug_led0_, arg.debug_led0, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##rs485_, arg.rs485, perm); \
	AMBA_GPIO_RESET_MODULE_PARAM_CALL(board_##audio_codec_, arg.audio_codec, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##audio_speaker_, arg.audio_speaker, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##audio_headphone_, arg.audio_headphone, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##audio_microphone_, arg.audio_microphone, perm); \
	AMBA_GPIO_IRQ_MODULE_PARAM_CALL(board_##touch_panel_, arg.tp_irq, perm); \
	AMBA_GPIO_RESET_MODULE_PARAM_CALL(board_##touch_panel_, arg.tp_reset, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##lcd_backlight_, arg.lcd_backlight, perm); \
	AMBA_GPIO_IRQ_MODULE_PARAM_CALL(board_##vin_vsync_, arg.vin_vsync, perm); \
	AMBA_GPIO_IRQ_MODULE_PARAM_CALL(board_##flash_charge_ready_, arg.flash_charge_ready, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##flash_trigger_, arg.flash_trigger, perm); \
	AMBA_GPIO_POWER_MODULE_PARAM_CALL(board_##flash_enable_, arg.flash_enable, perm)

/* ==========================================================================*/
extern struct ambarella_board_info ambarella_board_generic;

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

