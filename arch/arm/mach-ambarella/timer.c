/*
 * arch/arm/mach-ambarella/timer.c
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2008/01/08 - [Anthony Ginger] Rewrite for 2.6.28
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
 * Default clock is from APB.
 * Timer 1 is used as clock_event_device, while Timer 2 is
 * used as free-running clocksource.
 * Timer 3 is unused.
 */

#include <linux/interrupt.h>
#include <linux/clockchips.h>
#include <linux/delay.h>

#include <asm/mach/irq.h>
#include <asm/mach/time.h>

#include <mach/hardware.h>

#define AMBARELLA_TIMER_FREQ	(get_apb_bus_freq_hz())
#define AMBARELLA_TIMER_RATING	(300)

static struct notifier_block ambarella_timer1_clockevents_notifier;

static inline void ambarella_timer1_disable(void)
{
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_EN1);
}

static inline void ambarella_timer1_enable(void)
{
	amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_EN1);
}

static inline void ambarella_timer1_set_periodic(void)
{
	u32					cnt;

	cnt = AMBARELLA_TIMER_FREQ / HZ;
	amba_writel(TIMER1_STATUS_REG, cnt);
	amba_writel(TIMER1_RELOAD_REG, cnt);
	amba_writel(TIMER1_MATCH1_REG, 0x0);
	amba_writel(TIMER1_MATCH2_REG, 0x0);
	amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_OF1);
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_CSL1);
}

static inline void ambarella_timer1_set_oneshot(u32 cnt)
{
	if (!cnt)
		cnt = AMBARELLA_TIMER_FREQ / HZ;
	amba_writel(TIMER1_STATUS_REG, cnt);
	amba_writel(TIMER1_RELOAD_REG, 0x0);
	amba_writel(TIMER1_MATCH1_REG, 0x0);
	amba_writel(TIMER1_MATCH2_REG, 0x0);
	amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_OF1);
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_CSL1);
}

static void ambarella_timer1_set_mode(enum clock_event_mode mode,
	struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		ambarella_timer1_disable();
		ambarella_timer1_set_periodic();
		ambarella_timer1_enable();
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		ambarella_timer1_disable();
		ambarella_timer1_set_oneshot(0);
		ambarella_timer1_enable();
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		ambarella_timer1_disable();
		break;
	case CLOCK_EVT_MODE_RESUME:
		ambarella_timer1_enable();
		break;
	}
}

static int ambarella_timer1_set_next_event(unsigned long delta,
	struct clock_event_device *dev)
{
	if ((delta == 0) || (delta > 0xFFFFFFFF))
		return -ETIME;

	amba_writel(TIMER1_STATUS_REG, delta);

	return 0;
}

static struct clock_event_device ambarella_clkevt = {
	.name		= "ambarella-clkevt",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 32,
	.rating		= AMBARELLA_TIMER_RATING,
	.cpumask	= cpu_all_mask,
	.set_next_event	= ambarella_timer1_set_next_event,
	.set_mode	= ambarella_timer1_set_mode,
	.mode		= CLOCK_EVT_MODE_UNUSED,
};

static irqreturn_t ambarella_timer1_interrupt(int irq, void *dev_id)
{
	ambarella_clkevt.event_handler(&ambarella_clkevt);

	return IRQ_HANDLED;
}

static struct irqaction ambarella_timer1_irq = {
	.name		= "ambarella-timer1",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_TRIGGER_RISING,
	.handler	= ambarella_timer1_interrupt,
};

static inline void ambarella_timer2_init_for_clocksource(void)
{
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_EN2);
	amba_writel(TIMER2_STATUS_REG, 0xffffffff);
	amba_writel(TIMER2_RELOAD_REG, 0xffffffff);
	amba_writel(TIMER2_MATCH1_REG, 0x0);
	amba_writel(TIMER2_MATCH2_REG, 0x0);
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_OF2);
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_CSL2);
	amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_EN2);
}

static cycle_t ambarella_timer2_read(void)
{
	return 0xffffffff - amba_readl(TIMER2_STATUS_REG);
}

static struct clocksource ambarella_timer2_clksrc = {
	.name		= "ambarella-timer2",
	.shift		= 21,
	.rating		= AMBARELLA_TIMER_RATING,
	.read		= ambarella_timer2_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void ambarella_timer_init(void)
{
	ambarella_timer2_init_for_clocksource();
	ambarella_timer2_clksrc.mult = clocksource_hz2mult(
		AMBARELLA_TIMER_FREQ, ambarella_timer2_clksrc.shift);
	clocksource_register(&ambarella_timer2_clksrc);

	setup_irq(TIMER1_IRQ, &ambarella_timer1_irq);
	ambarella_clkevt.mult = div_sc(AMBARELLA_TIMER_FREQ,
		NSEC_PER_SEC, ambarella_clkevt.shift);
	ambarella_clkevt.max_delta_ns =
		clockevent_delta2ns(0xffffffff, &ambarella_clkevt);
	ambarella_clkevt.min_delta_ns =
		clockevent_delta2ns(1, &ambarella_clkevt);
	clockevents_register_device(&ambarella_clkevt);
}

struct sys_timer ambarella_timer = {
	.init		= ambarella_timer_init,
};

u32 ambarella_timer_suspend(u32 level)
{
	disable_irq(TIMER1_IRQ);

	return 0;
}

u32 ambarella_timer_resume(u32 level)
{
	amba_writel(TIMER_CTR_REG, 0x0);

	ambarella_clkevt.mult = div_sc(AMBARELLA_TIMER_FREQ,
		NSEC_PER_SEC, ambarella_clkevt.shift);
	ambarella_clkevt.max_delta_ns =
		clockevent_delta2ns(0xffffffff, &ambarella_clkevt);
	ambarella_clkevt.min_delta_ns =
		clockevent_delta2ns(1, &ambarella_clkevt);

	ambarella_timer2_init_for_clocksource();
	ambarella_timer2_clksrc.mult = clocksource_hz2mult(
		AMBARELLA_TIMER_FREQ, ambarella_timer2_clksrc.shift);
	ambarella_timer2_clksrc.mult_orig = ambarella_timer2_clksrc.mult;
	clocksource_calculate_interval(&ambarella_timer2_clksrc,
		NTP_INTERVAL_LENGTH);

	ambarella_timer1_set_mode(ambarella_clkevt.mode,
		&ambarella_clkevt);
	enable_irq(TIMER1_IRQ);

	return 0;
}

static int ambarella_timer1_clockevents(struct notifier_block *nb,
	unsigned long reason, void *dev)
{
	switch (reason) {
	case CLOCK_EVT_NOTIFY_SUSPEND:
		pr_info("%s: CLOCK_EVT_NOTIFY_SUSPEND\n", __func__);
		disable_irq(TIMER1_IRQ);
		break;

	case CLOCK_EVT_NOTIFY_RESUME:
		pr_info("%s: CLOCK_EVT_NOTIFY_RESUME\n", __func__);
		enable_irq(TIMER1_IRQ);
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

int __init ambarella_init_tm(void)
{
	int					errCode = 0;

	ambarella_timer1_clockevents_notifier.notifier_call =
		ambarella_timer1_clockevents;
	errCode = clockevents_register_notifier(
		&ambarella_timer1_clockevents_notifier);

	return errCode;
}

