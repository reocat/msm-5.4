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
#include <linux/cpufreq.h>

#include <mach/hardware.h>

#define AMBARELLA_TIMER_FREQ	(get_apb_bus_freq_hz())
#define AMBARELLA_TIMER_RATING	(300)

static enum clock_event_mode ambarella_timer1_mode = CLOCK_EVT_MODE_UNUSED;
static struct notifier_block tm_freq_transition;

static inline void ambarella_timer1_disable(void)
{
	amba_clrbits(TIMER_CTR_REG, TIMER_CTR_EN1);
}

static inline void ambarella_timer1_enable(void)
{
	amba_setbits(TIMER_CTR_REG, TIMER_CTR_EN1);
}

static inline void ambarella_timer1_set_periodic(void)
{
	u32					cnt;

	cnt = AMBARELLA_TIMER_FREQ / HZ;
	amba_writel(TIMER1_STATUS_REG, cnt);
	amba_writel(TIMER1_RELOAD_REG, cnt);
	amba_writel(TIMER1_MATCH1_REG, 0x0);
	amba_writel(TIMER1_MATCH2_REG, 0x0);
	amba_setbits(TIMER_CTR_REG, TIMER_CTR_OF1);
	amba_clrbits(TIMER_CTR_REG, TIMER_CTR_CSL1);
}

static inline void ambarella_timer1_set_oneshot(u32 cnt)
{
	if (!cnt)
		cnt = AMBARELLA_TIMER_FREQ / HZ;
	amba_writel(TIMER1_STATUS_REG, cnt);
	amba_writel(TIMER1_RELOAD_REG, 0x0);
	amba_writel(TIMER1_MATCH1_REG, 0x0);
	amba_writel(TIMER1_MATCH2_REG, 0x0);
	amba_setbits(TIMER_CTR_REG, TIMER_CTR_OF1);
	amba_clrbits(TIMER_CTR_REG, TIMER_CTR_CSL1);
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
	ambarella_timer1_mode = mode;
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
	amba_clrbits(TIMER_CTR_REG, TIMER_CTR_EN2);
	amba_writel(TIMER2_STATUS_REG, 0xffffffff);
	amba_writel(TIMER2_RELOAD_REG, 0xffffffff);
	amba_writel(TIMER2_MATCH1_REG, 0x0);
	amba_writel(TIMER2_MATCH2_REG, 0x0);
	amba_clrbits(TIMER_CTR_REG, TIMER_CTR_OF2);
	amba_clrbits(TIMER_CTR_REG, TIMER_CTR_CSL2);
	amba_setbits(TIMER_CTR_REG, TIMER_CTR_EN2);
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

int ambtm_freq_transition(struct notifier_block *nb,
	unsigned long val, void *data)
{
	unsigned long flags;
	int cnt, save;
	/* TODO struct cpufreq_freqs *f = data; */

	local_irq_save(flags);

	switch (val) {
	case AMB_CPUFREQ_PRECHANGE:
		pr_info("%s: Pre Change\n", __func__);
		disable_irq(TIMER1_IRQ);
		break;

	case AMB_CPUFREQ_POSTCHANGE:
		pr_info("%s: Post Change\n", __func__);
		/* Reset timer */
		save = amba_readl(TIMER_CTR_REG);
		amba_writel(TIMER_CTR_REG, 0x0);

		ambarella_clkevt.mult = div_sc(AMBARELLA_TIMER_FREQ,
			NSEC_PER_SEC, ambarella_clkevt.shift);
		ambarella_clkevt.max_delta_ns =
			clockevent_delta2ns(0xffffffff, &ambarella_clkevt);
		ambarella_clkevt.min_delta_ns =
			clockevent_delta2ns(1, &ambarella_clkevt);

		ambarella_timer2_clksrc.mult = clocksource_hz2mult(
			AMBARELLA_TIMER_FREQ, ambarella_timer2_clksrc.shift);

		/* Program the timer to start ticking */
		cnt = AMBARELLA_TIMER_FREQ / CLOCK_TICK_RATE;
		switch (ambarella_timer1_mode) {
		case CLOCK_EVT_MODE_PERIODIC:
			amba_writel(TIMER1_STATUS_REG, cnt);
			amba_writel(TIMER1_RELOAD_REG, cnt);
			break;
		case CLOCK_EVT_MODE_ONESHOT:
			amba_writel(TIMER1_STATUS_REG, cnt);
			break;
		case CLOCK_EVT_MODE_UNUSED:
		case CLOCK_EVT_MODE_SHUTDOWN:
		case CLOCK_EVT_MODE_RESUME:
			break;
		}

		amba_writel(TIMER_CTR_REG, save);
		enable_irq(TIMER1_IRQ);
		break;

	default:
		pr_err("%s: %ld\n", __func__, val);
		break;
	}

	local_irq_restore(flags);

	return 0;
} 

int __init ambarella_init_tm(void)
{
	int					errCode = 0;

	tm_freq_transition.notifier_call = ambtm_freq_transition;
	errCode = ambarella_register_freqnotifier(&tm_freq_transition); 

	return errCode;
}

