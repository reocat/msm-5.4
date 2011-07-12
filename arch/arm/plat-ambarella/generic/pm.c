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
#include <linux/cpu.h>

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/system.h>

#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/init.h>
#include <mach/system.h>

#include <hal/hal.h>

/* ==========================================================================*/
#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX	"ambarella_config."

/* ==========================================================================*/
static int pm_debug_enable_timer_irq = 0;
module_param(pm_debug_enable_timer_irq, int, 0644);

#ifdef CONFIG_GPIO_WM831X
extern int wm831x_config_poweroff(void);
#endif

/* ==========================================================================*/
void ambarella_power_off(void)
{
	if (ambarella_board_generic.power_control.gpio_id >= 0) {
#ifdef CONFIG_GPIO_WM831X
		if (!wm831x_config_poweroff()) {
			ambarella_set_gpio_output(
				&ambarella_board_generic.power_control, 0);
		} else {
			printk("Fail to config gpio for power off, Abort\r\n");
		}
#else
		ambarella_set_gpio_output(
			&ambarella_board_generic.power_control, 0);
#endif
	} else {
		rct_power_down();
	}
}

void ambarella_machine_restart(char mode, const char *cmd)
{
	disable_nonboot_cpus();
	local_irq_disable();
	local_fiq_disable();
	flush_cache_all();
	outer_flush_all();
	outer_disable();
	outer_inv_all();
	flush_cache_all();

	arch_reset(mode, cmd);
	mdelay(1000);
	printk("Reboot failed -- System halted\n");
	while (1);
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
		ambarella_adc_suspend(0);
		ambarella_timer_suspend(tm_level);
		ambarella_irq_suspend(0);
		ambarella_gpio_suspend(0);
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
		ambarella_gpio_resume(0);
		ambarella_irq_resume(0);
		ambarella_timer_resume(tm_level);
		ambarella_adc_resume(0);
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
	struct irq_desc				*pm_desc = NULL;
	struct irq_chip				*pm_chip = NULL;
	unsigned long				flags;

	if (ambarella_pm_pre(&flags, 1, 1, 1))
		BUG();

	if (pm_debug_enable_timer_irq) {
		pm_desc = irq_to_desc(TIMER1_IRQ);
		if (pm_desc)
			pm_chip = get_irq_desc_chip(pm_desc);
		if (pm_chip && pm_chip->irq_shutdown)
			pm_chip->irq_shutdown(&pm_desc->irq_data);

		amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_EN1);
		amba_writel(TIMER1_STATUS_REG, 0x800000);
		amba_writel(TIMER1_RELOAD_REG, 0x800000);
		amba_writel(TIMER1_MATCH1_REG, 0x0);
		amba_writel(TIMER1_MATCH2_REG, 0x0);
		amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_OF1);
		amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_CSL1);
	}

	if (pm_debug_enable_timer_irq) {
		if (pm_chip && pm_chip->irq_startup)
			pm_chip->irq_startup(&pm_desc->irq_data);
		amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_EN1);
	}

	cpu_do_idle();

	if (pm_debug_enable_timer_irq) {
		amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_EN1);
		if (pm_chip && pm_chip->irq_shutdown)
			pm_chip->irq_shutdown(&pm_desc->irq_data);
	}

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
#if defined(CONFIG_PLAT_AMBARELLA_SUPPORT_HAL)
	set_ambarella_hal_invalid();
#endif
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
	arm_pm_restart = ambarella_machine_restart;

	suspend_set_ops(&ambarella_pm_suspend_ops);
	hibernation_set_ops(&ambarella_pm_hibernation_ops);

	return 0;
}

