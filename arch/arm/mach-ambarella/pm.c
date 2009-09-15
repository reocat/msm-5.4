/*
 * arch/arm/mach-ambarella/pm.c
 * Power Management Routines
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/pda_power.h>

#include <asm/io.h>

#include <mach/hardware.h>

/* ==========================================================================*/
struct ambarella_gpio_power_info system_power_gpio_info = {
	.power_gpio	= -1,
	.power_level	= GPIO_HIGH,
	.power_delay	= 0,
};
module_param_call(system_power_gpio, param_set_int, param_get_int,
	&system_power_gpio_info.power_gpio, 0644);
MODULE_PARM_DESC(system_power_gpio,
	"The power on gpio id of system, if you have.");

module_param_call(system_power_level, param_set_int, param_get_int,
	&system_power_gpio_info.power_level, 0644);
MODULE_PARM_DESC(system_power_level,
	"The gpio power on level, if you set power on gpio id.");

module_param_call(system_power_delay, param_set_int, param_get_int,
	&system_power_gpio_info.power_delay, 0644);
MODULE_PARM_DESC(system_power_delay,
	"The gpio power on delay(ms), if you set power on gpio id.");

void ambarella_power_off(void)
{
	if (system_power_gpio_info.power_gpio >= 0) {
		ambarella_set_gpio_power(&system_power_gpio_info, 0);
	} else {
		rct_power_down();
	}
}

/* ==========================================================================*/
static int ambarella_power_is_ac_online(void)
{
	return 1;
}

static struct pda_power_pdata  ambarella_power_supply_info = {
	.is_ac_online    = ambarella_power_is_ac_online,
};

struct platform_device ambarella_power_supply = {
	.name = "pda-power",
	.id   = -1,
	.dev  = {
		.platform_data = &ambarella_power_supply_info,
	},
};

/* ==========================================================================*/
static int ambarella_pm_enter(suspend_state_t state)
{
	int					errorCode = 0;
#if (CHIP_REV == A5S)
	amb_hal_success_t			result;
	amb_operating_mode_t			operating_mode;

	result = amb_get_operating_mode(HAL_BASE_VP, &operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
	}

	operating_mode.mode = AMB_OPERATING_MODE_STANDBY;
	errorCode = ambarella_set_operating_mode(&operating_mode);
	if (errorCode) {
		pr_err("%s: amb_set_operating_mode failed(%d)\n",
			__func__, errorCode);
	}
#else
	pr_info("%s: enter with state[%d]\n", __func__, state);
	mdelay(10000);
	pr_info("%s: exit with state[%d]\n", __func__, state);
#endif

	return errorCode;
}

static int ambarella_pm_valid(suspend_state_t state)
{
	pr_info("%s: called with state[%d]\n", __func__, state);

	return 1;
}

static struct platform_suspend_ops ambarella_pm_ops = {
	.valid		= ambarella_pm_valid,
	.enter		= ambarella_pm_enter,
};

int __init ambarella_init_pm(void)
{
	/* power on sequence */
	amba_writel(RTC_POS0_REG, 0x50);
	amba_writel(RTC_POS1_REG, 0x50);
	amba_writel(RTC_POS2_REG, 0x50);

	pm_power_off = ambarella_power_off;

	suspend_set_ops(&ambarella_pm_ops);

	return 0;
}

