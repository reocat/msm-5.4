/*
 * arch/arm/plat-ambarella/generic/timer.c
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
 * Timer 3 is used as clock_event_device.
 * Timer 2 is used as free-running clocksource
 *         if CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE is defined
 * Timer 1 is not used.
 */

#include <linux/interrupt.h>
#include <linux/clockchips.h>
#include <linux/delay.h>

#include <asm/mach/irq.h>
#include <asm/mach/time.h>

#include <mach/hardware.h>
#include <plat/timer.h>

/* ==========================================================================*/
#define AMBARELLA_TIMER_FREQ	(get_apb_bus_freq_hz())
#define AMBARELLA_TIMER_RATING	(300)

struct ambarella_timer_pm_info {
	u32 timer1_status_reg;
	u32 timer1_reload_reg;
	u32 timer1_match1_reg;
	u32 timer1_match2_reg;
	u32 timer2_status_reg;
	u32 timer2_reload_reg;
	u32 timer2_match1_reg;
	u32 timer2_match2_reg;
	u32 timer3_status_reg;
	u32 timer3_reload_reg;
	u32 timer3_match1_reg;
	u32 timer3_match2_reg;
	u32 timer_ctr_reg;
	u32 timer_clk;
};
struct ambarella_timer_pm_info ambarella_timer_pm;

/* ==========================================================================*/
#define AMBARELLA_CE_TIMER_STATUS_REG	TIMER3_STATUS_REG
#define AMBARELLA_CE_TIMER_RELOAD_REG	TIMER3_RELOAD_REG
#define AMBARELLA_CE_TIMER_MATCH1_REG	TIMER3_MATCH1_REG
#define AMBARELLA_CE_TIMER_MATCH2_REG	TIMER3_MATCH2_REG
#define AMBARELLA_CE_TIMER_IRQ		TIMER3_IRQ

static inline void ambarella_ce_timer_disable(void)
{
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_EN3);
}

static inline void ambarella_ce_timer_enable(void)
{
	amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_EN3);
}

static inline void ambarella_ce_timer_misc(void)
{
	amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_OF3);
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_CSL3);
}

static inline void ambarella_ce_timer_set_periodic(void)
{
	u32					cnt;

	cnt = AMBARELLA_TIMER_FREQ / HZ;
	amba_writel(AMBARELLA_CE_TIMER_STATUS_REG, cnt);
	amba_writel(AMBARELLA_CE_TIMER_RELOAD_REG, cnt);
	amba_writel(AMBARELLA_CE_TIMER_MATCH1_REG, 0x0);
	amba_writel(AMBARELLA_CE_TIMER_MATCH2_REG, 0x0);
	ambarella_ce_timer_misc();
}

static inline void ambarella_ce_timer_set_oneshot(u32 cnt)
{
	BUG_ON(cnt == 0);
	amba_writel(AMBARELLA_CE_TIMER_STATUS_REG, cnt);
	amba_writel(AMBARELLA_CE_TIMER_RELOAD_REG, 0x0);
	amba_writel(AMBARELLA_CE_TIMER_MATCH1_REG, 0x0);
	amba_writel(AMBARELLA_CE_TIMER_MATCH2_REG, 0x0);
	ambarella_ce_timer_misc();
}

static void ambarella_ce_timer_set_mode(enum clock_event_mode mode,
	struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		ambarella_ce_timer_disable();
		ambarella_ce_timer_set_periodic();
		ambarella_ce_timer_enable();
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		ambarella_ce_timer_disable();
		ambarella_ce_timer_set_oneshot(AMBARELLA_TIMER_FREQ / HZ);
		ambarella_ce_timer_enable();
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		ambarella_ce_timer_disable();
		break;
	case CLOCK_EVT_MODE_RESUME:
		ambarella_ce_timer_enable();
		break;
	}
}

static int ambarella_ce_timer_set_next_event(unsigned long delta,
	struct clock_event_device *dev)
{
	if ((delta == 0) || (delta > 0xFFFFFFFF))
		return -ETIME;

	amba_writel(AMBARELLA_CE_TIMER_STATUS_REG, delta);

	return 0;
}

static struct clock_event_device ambarella_clkevt = {
	.name		= "ambarella-clkevt",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 32,
	.rating		= AMBARELLA_TIMER_RATING,
	.cpumask	= cpu_all_mask,
	.set_next_event	= ambarella_ce_timer_set_next_event,
	.set_mode	= ambarella_ce_timer_set_mode,
	.mode		= CLOCK_EVT_MODE_UNUSED,
	.irq		= AMBARELLA_CE_TIMER_IRQ,
};

static irqreturn_t ambarella_ce_timer_interrupt(int irq, void *dev_id)
{
	ambarella_clkevt.event_handler(&ambarella_clkevt);

	return IRQ_HANDLED;
}

static struct irqaction ambarella_ce_timer_irq = {
	.name		= "ambarella-ce-timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_TRIGGER_RISING,
	.handler	= ambarella_ce_timer_interrupt,
};

/* ==========================================================================*/
#ifdef CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE
#define AMBARELLA_CS_TIMER_STATUS_REG	TIMER2_STATUS_REG
#define AMBARELLA_CS_TIMER_RELOAD_REG	TIMER2_RELOAD_REG
#define AMBARELLA_CS_TIMER_MATCH1_REG	TIMER2_MATCH1_REG
#define AMBARELLA_CS_TIMER_MATCH2_REG	TIMER2_MATCH2_REG

static inline void ambarella_cs_timer_init(void)
{
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_EN2);
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_OF2);
	amba_clrbitsl(TIMER_CTR_REG, TIMER_CTR_CSL2);
	amba_writel(AMBARELLA_CS_TIMER_STATUS_REG, 0xffffffff);
	amba_writel(AMBARELLA_CS_TIMER_RELOAD_REG, 0xffffffff);
	amba_writel(AMBARELLA_CS_TIMER_MATCH1_REG, 0x0);
	amba_writel(AMBARELLA_CS_TIMER_MATCH2_REG, 0x0);
	amba_setbitsl(TIMER_CTR_REG, TIMER_CTR_EN2);
}

static cycle_t ambarella_cs_timer_read(struct clocksource *cs)
{
	return 0xffffffff - amba_readl(AMBARELLA_CS_TIMER_STATUS_REG);
}

static struct clocksource ambarella_cs_timer_clksrc = {
	.name		= "ambarella-cs-timer",
	.shift		= 21,
	.rating		= AMBARELLA_TIMER_RATING,
	.read		= ambarella_cs_timer_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};
#endif
/* ==========================================================================*/
static void ambarella_timer_init(void)
{
#ifdef CONFIG_LOCAL_TIMERS
	twd_base = __io(AMBARELLA_VA_PT_WD_BASE);
#endif

#ifdef CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE
	ambarella_cs_timer_init();
	ambarella_cs_timer_clksrc.mult = clocksource_hz2mult(
		AMBARELLA_TIMER_FREQ, ambarella_cs_timer_clksrc.shift);
	clocksource_register(&ambarella_cs_timer_clksrc);
#endif

	setup_irq(ambarella_clkevt.irq, &ambarella_ce_timer_irq);
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
	disable_irq(AMBARELLA_CE_TIMER_IRQ);
	ambarella_timer_pm.timer_ctr_reg = amba_readl(TIMER_CTR_REG);
#ifdef CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE
	amba_writel(TIMER_CTR_REG,
		(ambarella_timer_pm.timer_ctr_reg & 0xFFFFF00F));
#else
	amba_writel(TIMER_CTR_REG,
		(ambarella_timer_pm.timer_ctr_reg & 0xFFFFF0FF));
#endif

	ambarella_timer_pm.timer1_status_reg = amba_readl(TIMER1_STATUS_REG);
	ambarella_timer_pm.timer1_reload_reg = amba_readl(TIMER1_RELOAD_REG);
	ambarella_timer_pm.timer1_match1_reg = amba_readl(TIMER1_MATCH1_REG);
	ambarella_timer_pm.timer1_match2_reg = amba_readl(TIMER1_MATCH2_REG);
	ambarella_timer_pm.timer2_status_reg = amba_readl(TIMER2_STATUS_REG);
	ambarella_timer_pm.timer2_reload_reg = amba_readl(TIMER2_RELOAD_REG);
	ambarella_timer_pm.timer2_match1_reg = amba_readl(TIMER2_MATCH1_REG);
	ambarella_timer_pm.timer2_match2_reg = amba_readl(TIMER2_MATCH2_REG);
	ambarella_timer_pm.timer3_status_reg = amba_readl(TIMER3_STATUS_REG);
	ambarella_timer_pm.timer3_reload_reg = amba_readl(TIMER3_RELOAD_REG);
	ambarella_timer_pm.timer3_match1_reg = amba_readl(TIMER3_MATCH1_REG);
	ambarella_timer_pm.timer3_match2_reg = amba_readl(TIMER3_MATCH2_REG);
	ambarella_timer_pm.timer_clk = AMBARELLA_TIMER_FREQ;

	return 0;
}

u32 ambarella_timer_resume(u32 level)
{
#if 0
	amba_writel(TIMER1_STATUS_REG, ambarella_timer_pm.timer1_status_reg);
	amba_writel(TIMER1_RELOAD_REG, ambarella_timer_pm.timer1_reload_reg);
	amba_writel(TIMER1_MATCH1_REG, ambarella_timer_pm.timer1_match1_reg);
	amba_writel(TIMER1_MATCH2_REG, ambarella_timer_pm.timer1_match2_reg);
#endif
#ifdef CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE
	amba_writel(TIMER2_STATUS_REG, ambarella_timer_pm.timer2_status_reg);
	amba_writel(TIMER2_RELOAD_REG, ambarella_timer_pm.timer2_reload_reg);
	amba_writel(TIMER2_MATCH1_REG, ambarella_timer_pm.timer2_match1_reg);
	amba_writel(TIMER2_MATCH2_REG, ambarella_timer_pm.timer2_match2_reg);
#endif
	if ((ambarella_timer_pm.timer3_status_reg == 0) &&
		(ambarella_timer_pm.timer3_reload_reg == 0)){
		amba_writel(TIMER3_STATUS_REG, AMBARELLA_TIMER_FREQ / HZ);
	} else {
		amba_writel(TIMER3_STATUS_REG,
			ambarella_timer_pm.timer3_status_reg);
	}
	amba_writel(TIMER3_RELOAD_REG, ambarella_timer_pm.timer3_reload_reg);
	amba_writel(TIMER3_MATCH1_REG, ambarella_timer_pm.timer3_match1_reg);
	amba_writel(TIMER3_MATCH2_REG, ambarella_timer_pm.timer3_match2_reg);

	if (ambarella_timer_pm.timer_clk != AMBARELLA_TIMER_FREQ) {
		ambarella_clkevt.mult = div_sc(AMBARELLA_TIMER_FREQ,
			NSEC_PER_SEC, ambarella_clkevt.shift);
		ambarella_clkevt.max_delta_ns =
			clockevent_delta2ns(0xffffffff, &ambarella_clkevt);
		ambarella_clkevt.min_delta_ns =
			clockevent_delta2ns(1, &ambarella_clkevt);
		switch (ambarella_clkevt.mode) {
		case CLOCK_EVT_MODE_PERIODIC:
			ambarella_ce_timer_set_periodic();
			break;
		case CLOCK_EVT_MODE_ONESHOT:
		case CLOCK_EVT_MODE_UNUSED:
		case CLOCK_EVT_MODE_SHUTDOWN:
		case CLOCK_EVT_MODE_RESUME:
			break;
		}

#ifdef CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE
		/* Since AMBARELLA_TIMER_FREQ may be changed during suspend,
		   we'll switch to a safe clocksource and update it! */
		clocksource_change_rating(&ambarella_cs_timer_clksrc, 0);
		ambarella_cs_timer_clksrc.mult = clocksource_hz2mult(
			AMBARELLA_TIMER_FREQ, ambarella_cs_timer_clksrc.shift);
		clocksource_change_rating(&ambarella_cs_timer_clksrc,
			AMBARELLA_TIMER_RATING);
#endif
	}

#ifdef CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE
	amba_writel(TIMER_CTR_REG,
		((ambarella_timer_pm.timer_ctr_reg & 0x00000FF0) |
		(amba_readl(TIMER_CTR_REG) & 0xFFFFF00F)));
#else
	amba_writel(TIMER_CTR_REG,
		((ambarella_timer_pm.timer_ctr_reg & 0x00000F00) |
		(amba_readl(TIMER_CTR_REG) & 0xFFFFF0FF)));
#endif
	enable_irq(AMBARELLA_CE_TIMER_IRQ);

	return 0;
}

int __init ambarella_init_tm(void)
{
	int					errCode = 0;

	return errCode;
}

