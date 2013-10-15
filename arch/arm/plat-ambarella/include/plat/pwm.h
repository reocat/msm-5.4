/*
 * arch/arm/plat-ambarella/include/plat/pwm.h
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

#ifndef __PLAT_AMBARELLA_PWM_H__
#define __PLAT_AMBARELLA_PWM_H__

/* ==========================================================================*/

/* ==========================================================================*/
#define PWM_CONTROL_OFFSET		0x00
#define	PWM_ENABLE_OFFSET		0x04
#define PWM_MODE_OFFSET			0x08
#define PWM_CONTROL1_OFFSET		0x0c

#define PWM_B0_DATA_OFFSET		0x300
#define PWM_B0_ENABLE_OFFSET		0x304
#define PWM_C0_DATA_OFFSET		0x310
#define PWM_C0_ENABLE_OFFSET		0x314
#define PWM_B0_DATA1_OFFSET		0x320
#define PWM_C0_DATA1_OFFSET		0x328
#define PWM_B1_DATA_OFFSET		0x308
#define PWM_B1_ENABLE_OFFSET		0x30c
#define PWM_C1_DATA_OFFSET		0x318
#define PWM_C1_ENABLE_OFFSET		0x31c
#define PWM_B1_DATA1_OFFSET		0x324
#define PWM_C1_DATA1_OFFSET		0x32c

#define	PWM_ST_REG(x)			(STEPPER_BASE + (x))

/* ==========================================================================*/
#ifndef __ASSEMBLER__

/* ==========================================================================*/
struct ambarella_pwm_info {
	unsigned int period_ns;
	unsigned int max_duty;
	unsigned int default_duty;
	unsigned int active_level;
};

#define AMBA_PWM_MODULE_PARAM_CALL(name_prefix, arg, perm) \
	module_param_cb(name_prefix##period_ns, &param_ops_int, &(arg.period_ns), perm); \
	module_param_cb(name_prefix##max_duty, &param_ops_int, &(arg.max_duty), perm); \
	module_param_cb(name_prefix##default_duty, &param_ops_int, &(arg.default_duty), perm); \
	module_param_cb(name_prefix##active_level, &param_ops_int, &(arg.active_level), perm)

extern struct platform_device ambarella_pwm_platform_device0;
extern struct platform_device ambarella_pwm_platform_device1;
extern struct platform_device ambarella_pwm_platform_device2;
extern struct platform_device ambarella_pwm_platform_device3;
extern struct platform_device ambarella_pwm_platform_device4;

/* ==========================================================================*/
extern int ambarella_init_pwm(void);

#endif /* __ASSEMBLER__ */
/* ==========================================================================*/

#endif /* __PLAT_AMBARELLA_PWM_H__ */

