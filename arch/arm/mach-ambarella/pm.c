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

#include "init.h"

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
static int ambarella_pm_pre(unsigned long *irqflag)
{
	int					errorCode = 0;

	errorCode = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_PRE_PM, NULL));
	if (errorCode) {
		pr_err("%s@%d: AMBA_EVENT_PRE_PM failed(%d)\n",
			__func__, __LINE__, errorCode);
	}

	local_irq_save(*irqflag);

	ambarella_adc_suspend(1);
	ambarella_timer_suspend(1);
	ambarella_irq_suspend(1);
	ambarella_gpio_suspend(1);
	ambarella_pll_suspend(1);

	errorCode = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_PRE_PM, NULL));
	if (errorCode) {
		pr_err("%s@%d: AMBA_EVENT_PRE_PM failed(%d)\n",
			__func__, __LINE__, errorCode);
	}

	return errorCode;
}

static int ambarella_pm_post(unsigned long *irqflag)
{
	int					errorCode = 0;

	errorCode = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_POST_PM, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_PRE_PM failed(%d)\n",
			__func__, errorCode);
	}

	ambarella_pll_resume(1);
	ambarella_gpio_resume(1);
	ambarella_irq_resume(1);
	ambarella_timer_resume(1);
	ambarella_adc_resume(1);

	local_irq_restore(*irqflag);

	errorCode = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_POST_PM, NULL));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_PRE_PM failed(%d)\n",
			__func__, errorCode);
	}

	return errorCode;
}

static int ambarella_pm_check(suspend_state_t state)
{
	int					errorCode = 0;

	errorCode = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_CHECK_PM, &state));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_CHECK_PM failed(%d)\n",
			__func__, errorCode);
		goto ambarella_pm_check_exit;
	}

	errorCode = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_CHECK_PM, &state));
	if (errorCode) {
		pr_err("%s: AMBA_EVENT_CHECK_PM failed(%d)\n",
			__func__, errorCode);
	}

ambarella_pm_check_exit:
	return errorCode;
}

static int ambarella_pm_enter_standby(void)
{
	int					errorCode = 0;
	unsigned long				flags;
#if (SYSTEM_SUPPORT_HAL == 1)
	amb_hal_success_t			result;
	amb_operating_mode_t			operating_mode;
#endif

	if (ambarella_pm_pre(&flags))
		BUG();

#if (SYSTEM_SUPPORT_HAL == 1)
	result = amb_get_operating_mode(HAL_BASE_VP, &operating_mode);
	BUG_ON(result != AMB_HAL_SUCCESS);
	operating_mode.mode = AMB_OPERATING_MODE_STANDBY;

#if 1
	result = amb_set_operating_mode(HAL_BASE_VP, &operating_mode);
	if (result != AMB_HAL_SUCCESS) {
		pr_err("%s: amb_set_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
	}
#else
	__asm__ __volatile__ (
		"mcr	p15, 0, %[result], c7, c0, 4" :
		[result] "+r" (result));
#endif
#endif

	if (ambarella_pm_post(&flags))
		BUG();

	return errorCode;
}

typedef unsigned int (*amba_snapshot_suspend_t)(u32);

static int ambarella_pm_enter_sss(void)
{
	int					errorCode = 0;
	amba_snapshot_suspend_t			sss_entry;
	unsigned long				flags;

	sss_entry = (amba_snapshot_suspend_t)get_ambarella_sss_entry_virt();

	if (ambarella_pm_pre(&flags))
		BUG();

	errorCode = sss_entry(get_ambarella_sss_virt());
#if (SYSTEM_SUPPORT_HAL == 1)
	set_ambarella_hal_invalid();
#endif

	if (ambarella_pm_post(&flags))
		BUG();

	return errorCode;
}

static int ambarella_pm_enter(suspend_state_t state)
{
	int					errorCode = 0;

	pr_debug("%s: enter with state[%d]\n", __func__, state);

	switch (state) {
	case PM_SUSPEND_ON:
		break;

	case PM_SUSPEND_STANDBY:
		errorCode = ambarella_pm_enter_standby();
		break;

	case PM_SUSPEND_MEM:
		if (toss != NULL)
			errorCode = toss_switch(0);
		else if (get_ambarella_sss_entry_virt())
			errorCode = ambarella_pm_enter_sss();
		break;

	default:
		break;
	}

	pr_debug("%s: exit state[%d] with %d\n", __func__, state, errorCode);

	return errorCode;
}

static int ambarella_pm_valid(suspend_state_t state)
{
	int					errorCode = 0;
	int					valid = 0;

	errorCode = ambarella_pm_check(state);
	if (errorCode)
		goto ambarella_pm_valid_exit;

	switch (state) {
	case PM_SUSPEND_ON:
		valid = 1;
		break;

	case PM_SUSPEND_STANDBY:
#if (SYSTEM_SUPPORT_HAL == 1)
		valid = 1;
#endif
		break;

	case PM_SUSPEND_MEM:
		if (toss != NULL)
			valid = 1;
		else if (get_ambarella_sss_entry_virt())
			valid = 1;
		break;

	default:
		break;
	}

ambarella_pm_valid_exit:
	pr_debug("%s: state[%d]=%d\n", __func__, state, valid);

	return valid;
}

static struct platform_suspend_ops ambarella_pm_ops = {
	.valid		= ambarella_pm_valid,
	.enter		= ambarella_pm_enter,
};

int __init ambarella_init_pm(void)
{
	pm_power_off = ambarella_power_off;

	suspend_set_ops(&ambarella_pm_ops);

	return 0;
}

