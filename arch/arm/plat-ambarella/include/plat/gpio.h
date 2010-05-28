/*
 * arch/arm/plat-ambarella/include/plat/gpio.h
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

#ifndef __PLAT_AMBARELLA_GPIO_H
#define __PLAT_AMBARELLA_GPIO_H

/* ==========================================================================*/
#define GPIO_BANK_SIZE			32
#define ARCH_NR_GPIOS			(GPIO_INSTANCES * GPIO_BANK_SIZE)

/* ==========================================================================*/
#ifndef __ASSEMBLER__
#include <asm-generic/gpio.h>

#define gpio_get_value	__gpio_get_value
#define gpio_set_value	__gpio_set_value
#define gpio_cansleep	__gpio_cansleep

/* ==========================================================================*/
struct ambarella_gpio_power_info {
	int	power_gpio;
	int	power_level;
	int	power_delay;		//ms
};
#define AMBA_GPIO_POWER_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_call(name_prefix##power_gpio, param_set_int, param_get_int, &(arg.power_gpio), perm); \
	module_param_call(name_prefix##power_level, param_set_int, param_get_int, &(arg.power_level), perm); \
	module_param_call(name_prefix##power_delay, param_set_int, param_get_int, &(arg.power_delay), perm)
extern void ambarella_set_gpio_power(struct ambarella_gpio_power_info *pinfo, u32 poweron);

struct ambarella_gpio_input_info {
	int	input_gpio;
	int	input_level;
	int	input_delay;		//ms
};
#define AMBA_GPIO_INPUT_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_call(name_prefix##input_gpio, param_set_int, param_get_int, &(arg.input_gpio), perm); \
	module_param_call(name_prefix##input_level, param_set_int, param_get_int, &(arg.input_level), perm); \
	module_param_call(name_prefix##input_delay, param_set_int, param_get_int, &(arg.input_delay), perm)
extern u32 ambarella_get_gpio_input(struct ambarella_gpio_input_info *pinfo);

struct ambarella_gpio_reset_info {
	int	reset_gpio;
	int	reset_level;
	int	reset_delay;		//ms
	int	resume_delay;		//ms
};
#define AMBA_GPIO_RESET_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_call(name_prefix##reset_gpio, param_set_int, param_get_int, &(arg.reset_gpio), perm); \
	module_param_call(name_prefix##reset_level, param_set_int, param_get_int, &(arg.reset_level), perm); \
	module_param_call(name_prefix##reset_delay, param_set_int, param_get_int, &(arg.reset_delay), perm); \
	module_param_call(name_prefix##resume_delay, param_set_int, param_get_int, &(arg.resume_delay), perm)
extern void ambarella_set_gpio_reset(struct ambarella_gpio_reset_info *pinfo);

struct ambarella_gpio_irq_info {
	int	irq_gpio;
	int	irq_line;
	int	irq_type;
	int	irq_gpio_val;
	int	irq_gpio_mode;
};
#define AMBA_GPIO_IRQ_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_call(name_prefix##irq_gpio, param_set_int, param_get_int, &(arg.irq_gpio), perm); \
	module_param_call(name_prefix##irq_line, param_set_int, param_get_int, &(arg.irq_line), perm); \
	module_param_call(name_prefix##irq_type, param_set_int, param_get_int, &(arg.irq_type), perm); \
	module_param_call(name_prefix##irq_gpio_val, param_set_int, param_get_int, &(arg.irq_gpio_val), perm); \
	module_param_call(name_prefix##irq_gpio_mode, param_set_int, param_get_int, &(arg.irq_gpio_mode), perm)
extern int ambarella_is_valid_gpio_irq(struct ambarella_gpio_irq_info *pgpio_irq);

/* ==========================================================================*/
extern int ambarella_init_gpio(void);
extern void ambarella_gpio_set_valid(unsigned pin, int valid);

extern void ambarella_gpio_config(int id, int func);
extern void ambarella_gpio_set(int id, int value);
extern int ambarella_gpio_get(int id);

extern u32 ambarella_gpio_suspend(u32 level);
extern u32 ambarella_gpio_resume(u32 level);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif

