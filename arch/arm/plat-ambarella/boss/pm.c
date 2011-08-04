/*
 * arch/arm/plat-ambarella/boss/pm.c
 * Power Management Routines
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2011, Ambarella, Inc.
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
#include <linux/cpu.h>

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/system.h>

#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/system.h>

#include <hal/hal.h>
#include <linux/aipc/i_util.h>
#include <linux/aipc/lk_util.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/

/* ==========================================================================*/
void ambarella_power_off(void)
{
	pr_err("%s@%d: TBD\n", __func__, __LINE__);
}

void ambarella_power_off_prepare(void)
{
}

/* ==========================================================================*/
static int ambarella_pm_pre(unsigned long *irqflag, u32 bsuspend,
	u32 tm_level, u32 bnotifier)
{
	int					retval = 0;

	if (bnotifier) {
		retval = notifier_to_errno(
			ambarella_set_event(AMBA_EVENT_PRE_PM, NULL));
		if (retval) {
			pr_err("%s@%d: AMBA_EVENT_PRE_PM failed(%d)\n",
				__func__, __LINE__, retval);
		}
	}
	if (irqflag)
		local_irq_save(*irqflag);

	if (bsuspend) {
		ambarella_timer_suspend(tm_level);
		ambarella_irq_suspend(0);
		ambarella_pll_suspend(0);
	}

	if (bnotifier && irqflag) {
		retval = notifier_to_errno(
			ambarella_set_raw_event(AMBA_EVENT_PRE_PM, NULL));
		if (retval) {
			pr_err("%s@%d: AMBA_EVENT_PRE_PM failed(%d)\n",
				__func__, __LINE__, retval);
		}
	}

	return retval;
}

static int ambarella_pm_post(unsigned long *irqflag, u32 bresume,
	u32 tm_level, u32 bnotifier)
{
	int					retval = 0;

	if (bnotifier && irqflag) {
		retval = notifier_to_errno(
			ambarella_set_raw_event(AMBA_EVENT_POST_PM, NULL));
		if (retval) {
			pr_err("%s: AMBA_EVENT_PRE_PM failed(%d)\n",
				__func__, retval);
		}
	}

	if (bresume) {
		ambarella_pll_resume(0);
		ambarella_irq_resume(0);
		ambarella_timer_resume(tm_level);
	}

	if (irqflag)
		local_irq_restore(*irqflag);

	if (bnotifier) {
		retval = notifier_to_errno(
			ambarella_set_event(AMBA_EVENT_POST_PM, NULL));
		if (retval) {
			pr_err("%s: AMBA_EVENT_PRE_PM failed(%d)\n",
				__func__, retval);
		}
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

	if (ambarella_pm_pre(&flags, 1, 1, 1))
		BUG();

	pr_err("%s@%d: TBD\n", __func__, __LINE__);

	if (ambarella_pm_post(&flags, 1, 1, 1))
		BUG();

	return retval;
}

static int ambarella_pm_enter_mem(void)
{
	int					retval = 0;
	unsigned long				flags;

	if (ambarella_pm_pre(&flags, 1, 1, 1))
		BUG();

	pr_err("%s@%d: TBD\n", __func__, __LINE__);

	if (ambarella_pm_post(&flags, 1, 1, 1))
		BUG();

	return retval;
}

static int ambarella_pm_suspend_enter(suspend_state_t state)
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
		retval = ambarella_pm_enter_mem();
		break;

	default:
		break;
	}

	pr_debug("%s: exit state[%d] with %d\n", __func__, state, retval);

	return retval;
}

static int ambarella_pm_suspend_valid(suspend_state_t state)
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
		valid = 1;
		break;

	case PM_SUSPEND_MEM:
		//valid = 1;
		break;

	default:
		break;
	}

ambarella_pm_valid_exit:
	pr_debug("%s: state[%d]=%d\n", __func__, state, valid);

	return valid;
}

static struct platform_suspend_ops ambarella_pm_suspend_ops = {
	.valid		= ambarella_pm_suspend_valid,
	.enter		= ambarella_pm_suspend_enter,
};

static int ambarella_pm_hibernation_begin(void)
{
	int					retval = 0;

	return retval;
}

static void ambarella_pm_hibernation_end(void)
{
	int					retval = 0;

	retval = ambarella_pm_post(NULL, 0, 0, 1);

	ipc_report_abresume();
}

static int ambarella_pm_hibernation_pre_snapshot(void)
{
	int					retval = 0;

	retval = ambarella_pm_pre(NULL, 1, 0, 1);

	return retval;
}

static void ambarella_pm_hibernation_finish(void)
{
}

static int ambarella_pm_hibernation_prepare(void)
{
	int					retval = 0;

	return retval;
}

static int ambarella_pm_hibernation_enter(void)
{
	int					retval = 0;

	ambarella_power_off();

	return retval;
}

static void ambarella_pm_hibernation_leave(void)
{
	ambarella_pm_post(NULL, 1, 0, 0);
}

static int ambarella_pm_hibernation_pre_restore(void)
{
	int					retval = 0;

	return retval;
}

static void ambarella_pm_hibernation_restore_cleanup(void)
{
}

static void ambarella_pm_hibernation_restore_recover(void)
{
	ambarella_pm_post(NULL, 0, 0, 1);
}

static struct platform_hibernation_ops ambarella_pm_hibernation_ops = {
	.begin = ambarella_pm_hibernation_begin,
	.end = ambarella_pm_hibernation_end,
	.pre_snapshot = ambarella_pm_hibernation_pre_snapshot,
	.finish = ambarella_pm_hibernation_finish,
	.prepare = ambarella_pm_hibernation_prepare,
	.enter = ambarella_pm_hibernation_enter,
	.leave = ambarella_pm_hibernation_leave,
	.pre_restore = ambarella_pm_hibernation_pre_restore,
	.restore_cleanup = ambarella_pm_hibernation_restore_cleanup,
	.recover = ambarella_pm_hibernation_restore_recover,
};

int __init ambarella_init_pm(void)
{
	pm_power_off = ambarella_power_off;
	pm_power_off_prepare = ambarella_power_off_prepare;

	suspend_set_ops(&ambarella_pm_suspend_ops);
	hibernation_set_ops(&ambarella_pm_hibernation_ops);

	return 0;
}

/* ==========================================================================*/
int arch_swsusp_write(unsigned int flags)
{
	int					retval = 0;

	ipc_report_absuspend();

	return retval;
}

void arch_copy_data_page(unsigned long dst_pfn, unsigned long src_pfn)
{
	pr_debug("Copy 0x%08lx to 0x%08lx\n", __pfn_to_phys(src_pfn),
		__pfn_to_phys(dst_pfn));
}

