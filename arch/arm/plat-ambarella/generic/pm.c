/*
 * arch/arm/plat-ambarella/generic/pm.c
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
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/io.h>

#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/init.h>
#include <hal/hal.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/
static int pm_debug_sss_counter = 1;
module_param(pm_debug_sss_counter, int, 0644);

static int pm_debug_enable_timer_irq = 0;
module_param(pm_debug_enable_timer_irq, int, 0644);

static int pm_debug_hal_standby = 1;
module_param(pm_debug_hal_standby, int, 0644);

/* ==========================================================================*/
void ambarella_power_off(void)
{
	if (ambarella_board_generic.power_control.gpio_id >= 0) {
		ambarella_set_gpio_output(
			&ambarella_board_generic.power_control, 0);
	} else {
		rct_power_down();
	}
}

/* ==========================================================================*/
u32 get_ambarella_sss_virt(void)
{
	u32					vsss_start;
	u32					*tmp_sss;

	vsss_start = ambarella_phys_to_virt(DEFAULT_SSS_START);
	tmp_sss = (u32 *)vsss_start;
	if ((tmp_sss[1] != DEFAULT_SSS_MAGIC0) ||
		(tmp_sss[2] != DEFAULT_SSS_MAGIC1) ||
		(tmp_sss[3] != DEFAULT_SSS_MAGIC2))
		vsss_start = 0;

	return vsss_start;
}

u32 get_ambarella_sss_entry_virt(void)
{
	u32					vsss_start;

	vsss_start = get_ambarella_sss_virt();
	if (vsss_start == 0)
		return 0;

	return ambarella_phys_to_virt(*(u32 *)vsss_start);
}

/* ==========================================================================*/
static int ambarella_pm_pre(unsigned long *irqflag)
{
	int					retval = 0;

	retval = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_PRE_PM, NULL));
	if (retval) {
		pr_err("%s@%d: AMBA_EVENT_PRE_PM failed(%d)\n",
			__func__, __LINE__, retval);
	}

	local_irq_save(*irqflag);

	ambarella_adc_suspend(1);
	ambarella_timer_suspend(1);
	ambarella_irq_suspend(1);
	ambarella_gpio_suspend(1);
	ambarella_pll_suspend(1);

	retval = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_PRE_PM, NULL));
	if (retval) {
		pr_err("%s@%d: AMBA_EVENT_PRE_PM failed(%d)\n",
			__func__, __LINE__, retval);
	}

	return retval;
}

static int ambarella_pm_post(unsigned long *irqflag)
{
	int					retval = 0;

	retval = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_POST_PM, NULL));
	if (retval) {
		pr_err("%s: AMBA_EVENT_PRE_PM failed(%d)\n",
			__func__, retval);
	}

	ambarella_pll_resume(1);
	ambarella_gpio_resume(1);
	ambarella_irq_resume(1);
	ambarella_timer_resume(1);
	ambarella_adc_resume(1);

	local_irq_restore(*irqflag);

	retval = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_POST_PM, NULL));
	if (retval) {
		pr_err("%s: AMBA_EVENT_PRE_PM failed(%d)\n",
			__func__, retval);
	}

	return retval;
}

static int ambarella_pm_check(suspend_state_t state)
{
	int					retval = 0;

	retval = notifier_to_errno(
		ambarella_set_raw_event(AMBA_EVENT_CHECK_PM, &state));
	if (retval) {
		pr_err("%s: AMBA_EVENT_CHECK_PM failed(%d)\n",
			__func__, retval);
		goto ambarella_pm_check_exit;
	}

	retval = notifier_to_errno(
		ambarella_set_event(AMBA_EVENT_CHECK_PM, &state));
	if (retval) {
		pr_err("%s: AMBA_EVENT_CHECK_PM failed(%d)\n",
			__func__, retval);
	}

ambarella_pm_check_exit:
	return retval;
}

static int ambarella_pm_enter_standby(void)
{
	int					retval = 0;
	unsigned long				flags;
	struct irq_desc				*desc;
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
	amb_hal_success_t			result;
	amb_operating_mode_t			operating_mode;
#endif

	if (ambarella_pm_pre(&flags))
		BUG();

	if (pm_debug_enable_timer_irq) {
		desc = irq_to_desc(TIMER1_IRQ);
		if (desc && desc->chip)
			desc->chip->disable(TIMER1_IRQ);

		amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_EN1);
		amba_writel(TIMER1_STATUS_REG, 0x800000);
		amba_writel(TIMER1_RELOAD_REG, 0x800000);
		amba_writel(TIMER1_MATCH1_REG, 0x0);
		amba_writel(TIMER1_MATCH2_REG, 0x0);
		amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_OF1);
		amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_CSL1);
	}

#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
	result = amb_get_operating_mode(HAL_BASE_VP, &operating_mode);
	BUG_ON(result != AMB_HAL_SUCCESS);
	operating_mode.mode = AMB_OPERATING_MODE_STANDBY;

	if (pm_debug_enable_timer_irq) {
		if (desc && desc->chip)
			desc->chip->enable(TIMER1_IRQ);
		amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_EN1);
	}

	if (pm_debug_hal_standby) {
		result = amb_set_operating_mode(HAL_BASE_VP, &operating_mode);
		if (result != AMB_HAL_SUCCESS) {
			pr_err("%s: amb_set_operating_mode failed(%d)\n",
				__func__, result);
			retval = -EPERM;
		}
	} else {
		__asm__ __volatile__ (
			"mcr	p15, 0, %[result], c7, c0, 4" :
			[result] "+r" (result));
	}

	if (pm_debug_enable_timer_irq) {
		amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_EN1);
		if (desc && desc->chip)
			desc->chip->disable(TIMER1_IRQ);
	}

#endif

	if (ambarella_pm_post(&flags))
		BUG();

	return retval;
}

typedef unsigned int (*amba_snapshot_suspend_t)(u32);

static int ambarella_pm_enter_sss(void)
{
	int					retval = 0;
	amba_snapshot_suspend_t			sss_entry;
	unsigned long				flags;

	sss_entry = (amba_snapshot_suspend_t)get_ambarella_sss_entry_virt();

	if (ambarella_pm_pre(&flags))
		BUG();

	retval = sss_entry(get_ambarella_sss_virt());
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
	set_ambarella_hal_invalid();
#endif

	if (ambarella_pm_post(&flags))
		BUG();

	return retval;
}

static int ambarella_pm_enter(suspend_state_t state)
{
	int					retval = 0;

	pr_debug("%s: enter with state[%d]\n", __func__, state);

	switch (state) {
	case PM_SUSPEND_ON:
		break;

	case PM_SUSPEND_STANDBY:
		retval = ambarella_pm_enter_standby();
		break;

	case PM_SUSPEND_MEM:
		if ((get_ambarella_sss_entry_virt() != 0) &&
			(pm_debug_sss_counter > 0)) {
			retval = ambarella_pm_enter_sss();
			pm_debug_sss_counter--;
		}
		break;

	default:
		break;
	}

	pr_debug("%s: exit state[%d] with %d\n", __func__, state, retval);

	return retval;
}

static int ambarella_pm_valid(suspend_state_t state)
{
	int					retval = 0;
	int					valid = 0;

	retval = ambarella_pm_check(state);
	if (retval)
		goto ambarella_pm_valid_exit;

	switch (state) {
	case PM_SUSPEND_ON:
		valid = 1;
		break;

	case PM_SUSPEND_STANDBY:
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
		valid = 1;
#endif
		break;

	case PM_SUSPEND_MEM:
		if ((get_ambarella_sss_entry_virt() != 0) &&
			(pm_debug_sss_counter > 0))
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

