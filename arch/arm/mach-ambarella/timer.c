/*
 * arch/arm/plat-ambarella/generic/timer.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2012, Ambarella, Inc.
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
 */

#include <linux/interrupt.h>
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/mach/time.h>
#include <asm/smp_twd.h>
#include <asm/sched_clock.h>
#include <plat/timer.h>

static void __iomem *timer_ctrl_reg = NULL;
static void __iomem *ce_base = NULL;
static u32 ce_ctrl_offset = -1;
static void __iomem *cs_base = NULL;
static u32 cs_ctrl_offset = -1;

#define AMBARELLA_TIMER_FREQ		clk_get_rate(clk_get(NULL, "gclk_apb"))
#define AMBARELLA_TIMER_RATING		(300)

static struct clock_event_device ambarella_clkevt;
static struct clocksource ambarella_clksrc;
static u32 ambarella_read_sched_clock(void);

/* ==========================================================================*/
struct amb_timer_pm_reg {
	u32 clk_rate;
	u32 ctrl_reg;
	u32 status_reg;
	u32 reload_reg;
	u32 match1_reg;
	u32 match2_reg;
};

static struct amb_timer_pm_reg amb_timer_ce_pm;
static struct amb_timer_pm_reg amb_timer_cs_pm;

static void ambarella_timer_suspend(u32 is_ce)
{
	struct amb_timer_pm_reg *amb_timer_pm;
	void __iomem *regbase;
	u32 ctrl_offset;

	amb_timer_pm = is_ce ? &amb_timer_ce_pm : &amb_timer_cs_pm;
	regbase = is_ce ? ce_base : cs_base;
	ctrl_offset = is_ce ? ce_ctrl_offset : cs_ctrl_offset;

	amb_timer_pm->clk_rate = AMBARELLA_TIMER_FREQ;
	amb_timer_pm->ctrl_reg = (amba_readl(timer_ctrl_reg) >> ctrl_offset) & 0xf;
	amb_timer_pm->status_reg = amba_readl(regbase + TIMER_STATUS_OFFSET);
	amb_timer_pm->reload_reg = amba_readl(regbase + TIMER_RELOAD_OFFSET);
	amb_timer_pm->match1_reg = amba_readl(regbase + TIMER_MATCH1_OFFSET);
	amb_timer_pm->match2_reg = amba_readl(regbase + TIMER_MATCH2_OFFSET);

	amba_clrbitsl(timer_ctrl_reg, 0xf << ctrl_offset);
}

static void ambarella_timer_resume(u32 is_ce)
{
	struct clock_event_device *clkevt = &ambarella_clkevt;
	struct clocksource *clksrc = &ambarella_clksrc;
	struct amb_timer_pm_reg *amb_timer_pm;
	void __iomem *regbase;
	u32 ctrl_offset, clk_rate;

	amb_timer_pm = is_ce ? &amb_timer_ce_pm : &amb_timer_cs_pm;
	regbase = is_ce ? ce_base : cs_base;
	ctrl_offset = is_ce ? ce_ctrl_offset : cs_ctrl_offset;

	amba_clrbitsl(timer_ctrl_reg, 0xf << ctrl_offset);

	amba_writel(regbase + TIMER_STATUS_OFFSET, amb_timer_pm->status_reg);
	amba_writel(regbase + TIMER_RELOAD_OFFSET, amb_timer_pm->reload_reg);
	amba_writel(regbase + TIMER_MATCH1_OFFSET, amb_timer_pm->match1_reg);
	amba_writel(regbase + TIMER_MATCH2_OFFSET, amb_timer_pm->match2_reg);

	clk_rate = AMBARELLA_TIMER_FREQ;
	if (amb_timer_pm->clk_rate == clk_rate)
		goto resume_exit;

	amb_timer_pm->clk_rate = clk_rate;

	if (is_ce)
		clockevents_update_freq(clkevt, clk_rate);
	else
		__clocksource_updatefreq_hz(clksrc, clk_rate);

resume_exit:
	amba_setbitsl(timer_ctrl_reg, amb_timer_pm->ctrl_reg << ctrl_offset);
}


/* ==========================================================================*/

static inline void ambarella_ce_timer_disable(void)
{
	amba_clrbitsl(timer_ctrl_reg, TIMER_CTRL_EN << ce_ctrl_offset);
}

static inline void ambarella_ce_timer_enable(void)
{
	amba_setbitsl(timer_ctrl_reg, TIMER_CTRL_EN << ce_ctrl_offset);
}

static inline void ambarella_ce_timer_misc(void)
{
	amba_setbitsl(timer_ctrl_reg, TIMER_CTRL_OF << ce_ctrl_offset);
	amba_clrbitsl(timer_ctrl_reg, TIMER_CTRL_CSL << ce_ctrl_offset);
}

static inline void ambarella_ce_timer_set_periodic(void)
{
	u32 cnt = AMBARELLA_TIMER_FREQ / HZ;

	amba_writel(ce_base + TIMER_STATUS_OFFSET, cnt);
	amba_writel(ce_base + TIMER_RELOAD_OFFSET, cnt);
	amba_writel(ce_base + TIMER_MATCH1_OFFSET, 0x0);
	amba_writel(ce_base + TIMER_MATCH2_OFFSET, 0x0);

	ambarella_ce_timer_misc();
}

static inline void ambarella_ce_timer_set_oneshot(void)
{
	amba_writel(ce_base + TIMER_STATUS_OFFSET, 0x0);
	amba_writel(ce_base + TIMER_RELOAD_OFFSET, 0xffffffff);
	amba_writel(ce_base + TIMER_MATCH1_OFFSET, 0x0);
	amba_writel(ce_base + TIMER_MATCH2_OFFSET, 0x0);

	ambarella_ce_timer_misc();
}

static void ambarella_ce_timer_set_mode(enum clock_event_mode mode,
	struct clock_event_device *clkevt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		ambarella_ce_timer_disable();
		ambarella_ce_timer_set_periodic();
		ambarella_ce_timer_enable();
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		ambarella_ce_timer_disable();
		ambarella_ce_timer_set_oneshot();
		ambarella_ce_timer_enable();
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		ambarella_ce_timer_disable();
		break;
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
	pr_debug("%s:%d\n", __func__, mode);
}

static int ambarella_ce_timer_set_next_event(unsigned long delta,
	struct clock_event_device *dev)
{
	amba_writel(ce_base + TIMER_STATUS_OFFSET, delta);
	return 0;
}

static void ambarella_ce_timer_suspend(struct clock_event_device *dev)
{
	ambarella_timer_suspend(1);
}

static void ambarella_ce_timer_resume(struct clock_event_device *dev)
{
	ambarella_timer_resume(1);
}

static struct clock_event_device ambarella_clkevt = {
	.name		= "ambarella-clkevt",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.rating		= AMBARELLA_TIMER_RATING,
	.set_next_event	= ambarella_ce_timer_set_next_event,
	.set_mode	= ambarella_ce_timer_set_mode,
	.mode		= CLOCK_EVT_MODE_UNUSED,
	.suspend	= ambarella_ce_timer_suspend,
	.resume		= ambarella_ce_timer_resume,
};

static irqreturn_t ambarella_ce_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *clkevt = &ambarella_clkevt;

	clkevt->event_handler(clkevt);
	return IRQ_HANDLED;
}

static struct irqaction ambarella_ce_timer_irq = {
	.name		= "ambarella-ce-timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_TRIGGER_RISING,
	.handler	= ambarella_ce_timer_interrupt,
};

/* ==========================================================================*/
static inline void ambarella_cs_timer_init(void)
{
	amba_clrbitsl(timer_ctrl_reg, TIMER_CTRL_EN << cs_ctrl_offset);
	amba_clrbitsl(timer_ctrl_reg, TIMER_CTRL_OF << cs_ctrl_offset);
	amba_clrbitsl(timer_ctrl_reg, TIMER_CTRL_CSL << cs_ctrl_offset);
	amba_writel(cs_base + TIMER_STATUS_OFFSET, 0xffffffff);
	amba_writel(cs_base + TIMER_RELOAD_OFFSET, 0xffffffff);
	amba_writel(cs_base + TIMER_MATCH1_OFFSET, 0x0);
	amba_writel(cs_base + TIMER_MATCH2_OFFSET, 0x0);
	amba_setbitsl(timer_ctrl_reg, TIMER_CTRL_EN << cs_ctrl_offset);
}

static cycle_t ambarella_cs_timer_read(struct clocksource *cs)
{
	return (-(u32)amba_readl(cs_base + TIMER_STATUS_OFFSET));
}

static void ambarella_cs_timer_suspend(struct clocksource *dev)
{
	ambarella_timer_suspend(0);
}

static void ambarella_cs_timer_resume(struct clocksource *dev)
{
	ambarella_timer_resume(0);
}

static struct clocksource ambarella_clksrc = {
	.name		= "ambarella-cs-timer",
	.rating		= AMBARELLA_TIMER_RATING,
	.read		= ambarella_cs_timer_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
	.suspend	= ambarella_cs_timer_suspend,
	.resume		= ambarella_cs_timer_resume,
};

static u32 notrace ambarella_read_sched_clock(void)
{
	return (-(u32)amba_readl(cs_base + TIMER_STATUS_OFFSET));
}

/* ==========================================================================*/
static const struct of_device_id clock_event_match[] __initconst = {
	{ .compatible = "ambarella,clock-event" },
	{ },
};

static const struct of_device_id clock_source_match[] __initconst = {
	{ .compatible = "ambarella,clock-source" },
	{ },
};

static void __init ambarella_clockevent_init(void)
{
	struct device_node *np;
	struct clock_event_device *clkevt;
	int rval, irq;

	np = of_find_matching_node(NULL, clock_event_match);
	if (!np) {
		pr_err("Can't find clock event node\n");
		return;
	}

	ce_base = of_iomap(np, 0);
	if (!ce_base) {
		pr_err("%s: Failed to map event base\n", __func__);
		return;
	}

	timer_ctrl_reg = of_iomap(np, 1);
	if (!timer_ctrl_reg) {
		pr_err("%s: Failed to map timer-ctrl base\n", __func__);
		return;
	}

	irq = irq_of_parse_and_map(np, 0);
	if (irq <= 0) {
		pr_err("%s: Can't get irq\n", __func__);
		return;
	}

	rval = of_property_read_u32(np, "ctrl-offset", &ce_ctrl_offset);
	if (rval < 0) {
		pr_err("%s: Can't get ctrl offset\n", __func__);
		return;
	}

	of_node_put(np);

	clkevt = &ambarella_clkevt;
	clkevt->cpumask = cpumask_of(0);
	clkevt->irq = irq;

	rval = setup_irq(clkevt->irq, &ambarella_ce_timer_irq);
	if (rval) {
		printk(KERN_ERR "Failed to register timer IRQ: %d\n", rval);
		BUG();
	}

	clockevents_config_and_register(clkevt, AMBARELLA_TIMER_FREQ, 1, 0xffffffff);
}

static void __init ambarella_clocksource_init(void)
{
	struct device_node *np;
	struct clocksource *clksrc;
	int rval;

	np = of_find_matching_node(NULL, clock_source_match);
	if (!np) {
		pr_err("Can't find clock source node\n");
		return;
	}

	cs_base = of_iomap(np, 0);
	if (!cs_base) {
		pr_err("%s: Failed to map source base\n", __func__);
		return;
	}

	timer_ctrl_reg = of_iomap(np, 1);
	if (!timer_ctrl_reg) {
		pr_err("%s: Failed to map timer-ctrl base\n", __func__);
		return;
	}

	rval = of_property_read_u32(np, "ctrl-offset", &cs_ctrl_offset);
	if (rval < 0) {
		pr_err("%s: Can't get ctrl offset\n", __func__);
		return;
	}

	of_node_put(np);

	clksrc = &ambarella_clksrc;

	ambarella_cs_timer_init();

	clocksource_register_hz(clksrc, AMBARELLA_TIMER_FREQ);

	pr_debug("%s: mult = %u, shift = %u\n",
		clksrc->name, clksrc->mult, clksrc->shift);

	setup_sched_clock(ambarella_read_sched_clock, 32, AMBARELLA_TIMER_FREQ);
}

#ifdef CONFIG_HAVE_ARM_TWD
static DEFINE_TWD_LOCAL_TIMER(twd_local_timer, AMBARELLA_VA_PT_WD_BASE, IRQ_LOCALTIMER);

static void __init ambarella_smp_twd_init(void)
{
	int err = twd_local_timer_register(&twd_local_timer);
	if (err)
		pr_err("twd_local_timer_register failed %d\n", err);
}
#else
#define ambarella_smp_twd_init()	do {} while(0)
#endif

void __init ambarella_timer_init(void)
{
	ambarella_clockevent_init();
	ambarella_clocksource_init();

	ambarella_smp_twd_init();
}

