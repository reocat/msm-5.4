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
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/mach/irq.h>
#include <asm/mach/time.h>
#include <asm/smp_twd.h>
#include <asm/sched_clock.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include <plat/timer.h>
#include <plat/clk.h>

/* ==========================================================================*/
static struct clk pll_out_core = {
	.parent		= NULL,
	.name		= "pll_out_core",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_CORE_CTRL_REG,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= PLL_REG_UNAVAILABLE,
	.frac_reg	= PLL_CORE_FRAC_REG,
	.ctrl2_reg	= PLL_CORE_CTRL2_REG,
	.ctrl3_reg	= PLL_CORE_CTRL3_REG,
	.lock_reg	= PLL_LOCK_REG,
	.lock_bit	= 6,
	.divider	= 0,
	.max_divider	= 0,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_pll_ops,
};

static struct clk pll_out_idsp = {
	.parent		= NULL,
	.name		= "pll_out_idsp",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_IDSP_CTRL_REG,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= PLL_REG_UNAVAILABLE,
	.frac_reg	= PLL_IDSP_FRAC_REG,
	.ctrl2_reg	= PLL_IDSP_CTRL2_REG,
	.ctrl3_reg	= PLL_IDSP_CTRL3_REG,
	.lock_reg	= PLL_LOCK_REG,
	.lock_bit	= 4,
	.divider	= 0,
	.max_divider	= 0,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_pll_ops,
};

static struct clk pll_out_ddr = {
	.parent		= NULL,
	.name		= "pll_out_ddr",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_DDR_CTRL_REG,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= PLL_REG_UNAVAILABLE,
	.frac_reg	= PLL_DDR_FRAC_REG,
	.ctrl2_reg	= PLL_DDR_CTRL2_REG,
	.ctrl3_reg	= PLL_DDR_CTRL3_REG,
	.lock_reg	= PLL_LOCK_REG,
	.lock_bit	= 5,
	.divider	= 0,
	.max_divider	= 0,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_pll_ops,
};

static unsigned long ambarella_rct_core_get_rate(struct clk *c)
{
	u32 rate;
	u32 divider;

	if (!c->parent || !c->parent->ops || !c->parent->ops->get_rate) {
		goto ambarella_rct_core_get_rate_exit;
	}
	rate = c->parent->ops->get_rate(c->parent);
	if (c->post_reg != PLL_REG_UNAVAILABLE) {
		divider = amba_rct_readl(c->post_reg);
		if (divider) {
			rate /= divider;
		}
	}
	if (c->divider) {
		rate /= c->divider;
	}
#if (CHIP_REV == I1)
	if (amba_rct_readl(RCT_REG(0x24C))) {
		rate <<= 1;
	}
#endif
	c->rate = rate;

ambarella_rct_core_get_rate_exit:
	return c->rate;
}

struct clk_ops ambarella_rct_core_ops = {
	.enable		= NULL,
	.disable	= NULL,
	.get_rate	= ambarella_rct_core_get_rate,
	.round_rate	= NULL,
	.set_rate	= NULL,
	.set_parent	= NULL,
};

static struct clk gclk_core = {
	.parent		= &pll_out_core,
	.name		= "gclk_core",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
#if ((CHIP_REV == A8) || (CHIP_REV == S2L))
	.post_reg	= PLL_REG_UNAVAILABLE,
#else
	.post_reg	= SCALER_CORE_POST_REG,
#endif
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
#if ((CHIP_REV == A5S) || (CHIP_REV == S2))
	.divider	= 1,
#else
	.divider	= 2,
#endif
	.max_divider	= (1 << 4) - 1,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_core_ops,
};

static unsigned long ambarella_rct_axb_get_rate(struct clk *c)
{
	u32 rate;
	u32 divider;

	if (!c->parent || !c->parent->ops || !c->parent->ops->get_rate) {
		goto ambarella_rct_axb_get_rate_exit;
	}
	rate = c->parent->ops->get_rate(c->parent);
	if (c->post_reg != PLL_REG_UNAVAILABLE) {
		divider = amba_rct_readl(c->post_reg);
		if (divider) {
			rate /= divider;
		}
	}
	if (c->divider) {
		rate /= c->divider;
	}
#if (CHIP_REV == S2)
	if (amba_rct_readl(RCT_REG(0x24C))) {
		rate <<= 1;
	}
#endif
	c->rate = rate;

ambarella_rct_axb_get_rate_exit:
	return c->rate;
}

struct clk_ops ambarella_rct_axb_ops = {
	.enable		= NULL,
	.disable	= NULL,
	.get_rate	= ambarella_rct_axb_get_rate,
	.round_rate	= NULL,
	.set_rate	= NULL,
	.set_parent	= NULL,
};

static struct clk gclk_ahb = {
	.parent		= &pll_out_core,
	.name		= "gclk_ahb",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
#if ((CHIP_REV == A8) || (CHIP_REV == S2L))
	.post_reg	= PLL_REG_UNAVAILABLE,
#else
	.post_reg	= SCALER_CORE_POST_REG,
#endif
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
#if (CHIP_REV == A5S)
	.divider	= 1,
#elif (CHIP_REV == S2L)
	.divider	= 4,
#else
	.divider	= 2,
#endif
	.max_divider	= (1 << 4) - 1,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_core_ops,
};

static struct clk gclk_apb = {
	.parent		= &pll_out_core,
	.name		= "gclk_apb",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
#if ((CHIP_REV == A8) || (CHIP_REV == S2L))
	.post_reg	= PLL_REG_UNAVAILABLE,
#else
	.post_reg	= SCALER_CORE_POST_REG,
#endif
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
#if (CHIP_REV == A5S)
	.divider	= 2,
#elif (CHIP_REV == S2L)
	.divider	= 8,
#else
	.divider	= 4,
#endif
	.max_divider	= (1 << 4) - 1,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_core_ops,
};

/* ==========================================================================*/
#if defined(CONFIG_PLAT_AMBARELLA_HAVE_ARM11)
static struct clk gclk_arm = {
	.parent		= &pll_out_idsp,
	.name		= "gclk_arm",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= SCALER_ARM_ASYNC_REG,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
	.divider	= 0,
	.max_divider	= (1 << 3) - 1,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_scaler_ops,
};
#endif
#if defined(CONFIG_PLAT_AMBARELLA_CORTEX)
static struct clk gclk_cortex = {
	.parent		= NULL,
	.name		= "gclk_cortex",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_CORTEX_CTRL_REG,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= PLL_REG_UNAVAILABLE,
	.frac_reg	= PLL_CORTEX_FRAC_REG,
	.ctrl2_reg	= PLL_CORTEX_CTRL2_REG,
	.ctrl3_reg	= PLL_CORTEX_CTRL3_REG,
	.lock_reg	= PLL_LOCK_REG,
	.lock_bit	= 2,
	.divider	= 0,
	.max_divider	= 0,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_pll_ops,
};
static struct clk gclk_axi = {
	.parent		= &gclk_cortex,
	.name		= "gclk_axi",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= PLL_REG_UNAVAILABLE,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
	.divider	= 3,
	.max_divider	= 0,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_scaler_ops,
};
#if defined(CONFIG_HAVE_ARM_TWD)
static struct clk clk_smp_twd = {
	.parent		= &gclk_axi,
	.name		= "smp_twd",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= PLL_REG_UNAVAILABLE,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
	.divider	= 1,
	.max_divider	= 0,
	.extra_scaler	= 0,
	.ops		= &ambarella_rct_scaler_ops,
};
#endif
#endif
static struct clk gclk_idsp = {
	.parent		= &pll_out_idsp,
	.name		= "gclk_idsp",
	.rate		= 0,
	.frac_mode	= 0,
	.ctrl_reg	= PLL_REG_UNAVAILABLE,
	.pres_reg	= PLL_REG_UNAVAILABLE,
	.post_reg	= SCALER_IDSP_POST_REG,
	.frac_reg	= PLL_REG_UNAVAILABLE,
	.ctrl2_reg	= PLL_REG_UNAVAILABLE,
	.ctrl3_reg	= PLL_REG_UNAVAILABLE,
	.lock_reg	= PLL_REG_UNAVAILABLE,
	.lock_bit	= 0,
	.divider	= 0,
	.max_divider	= (1 << 3) - 1,
#if (CHIP_REV == S2L)
	.extra_scaler	= 1,
#else
	.extra_scaler	= 0,
#endif
	.ops		= &ambarella_rct_scaler_ops,
};

/* ==========================================================================*/
static void __iomem *timer_ctrl_reg = NULL;
static void __iomem *ce_base = NULL;
static u32 ce_ctrl_offset = -1;
static void __iomem *cs_base = NULL;
static u32 cs_ctrl_offset = -1;

static u32 ambarella_timer_get_pll(void)
{
	return clk_get_rate(clk_get(NULL, "gclk_apb"));
}

#define AMBARELLA_TIMER_FREQ			(ambarella_timer_get_pll())
#define AMBARELLA_TIMER_RATING			(300)

/* ==========================================================================*/

struct ambarella_timer_pm_info {
	u32 timer_clk;
	u32 timer_ctr_reg;
	u32 timer_ce_status_reg;
	u32 timer_ce_reload_reg;
	u32 timer_ce_match1_reg;
	u32 timer_ce_match2_reg;
	u32 timer_cs_status_reg;
	u32 timer_cs_reload_reg;
	u32 timer_cs_match1_reg;
	u32 timer_cs_match2_reg;
};

struct ambarella_timer_pm_info ambarella_timer_pm;
static struct clock_event_device ambarella_clkevt;
static struct irqaction ambarella_ce_timer_irq;

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

static struct clock_event_device ambarella_clkevt = {
	.name		= "ambarella-clkevt",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.rating		= AMBARELLA_TIMER_RATING,
	.set_next_event	= ambarella_ce_timer_set_next_event,
	.set_mode	= ambarella_ce_timer_set_mode,
	.mode		= CLOCK_EVT_MODE_UNUSED,
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
	ambarella_clk_add(&pll_out_core);
	ambarella_clk_add(&pll_out_idsp);
	ambarella_clk_add(&pll_out_ddr);
	ambarella_clk_add(&gclk_core);
	ambarella_clk_add(&gclk_ahb);
	ambarella_clk_add(&gclk_apb);
#if defined(CONFIG_PLAT_AMBARELLA_HAVE_ARM11)
	ambarella_clk_add(&gclk_arm);
#endif
#if defined(CONFIG_PLAT_AMBARELLA_CORTEX)
	ambarella_clk_add(&gclk_cortex);
	ambarella_clk_add(&gclk_axi);
#if defined(CONFIG_HAVE_ARM_TWD)
	ambarella_clk_add(&clk_smp_twd);
#endif
#endif
	ambarella_clk_add(&gclk_idsp);

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

static struct clocksource ambarella_cs_timer_clksrc = {
	.name		= "ambarella-cs-timer",
	.rating		= AMBARELLA_TIMER_RATING,
	.read		= ambarella_cs_timer_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static u32 notrace ambarella_read_sched_clock(void)
{
	return (-(u32)amba_readl(cs_base + TIMER_STATUS_OFFSET));
}

/* ==========================================================================*/

static const struct of_device_id timer_ctrl_match[] __initconst = {
	{ .compatible = "ambarella,timer-ctrl" },
	{ },
};

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

	rval = of_property_read_u32(np, "ctrl-offset", &ce_ctrl_offset);
	if (rval < 0) {
		pr_err("%s: Can't get ctrl offset\n", __func__);
		return;
	}

	of_node_put(np);

	clksrc = &ambarella_cs_timer_clksrc;

	ambarella_cs_timer_init();

	clocksource_register_hz(clksrc, AMBARELLA_TIMER_FREQ);

	pr_debug("%s: mult = %u, shift = %u\n",
		clksrc->name, clksrc->mult, clksrc->shift);

	setup_sched_clock(ambarella_read_sched_clock, 32, AMBARELLA_TIMER_FREQ);
}

void __init ambarella_timer_init(void)
{
	struct device_node *np;

	np = of_find_matching_node(NULL, timer_ctrl_match);
	if (!np) {
		pr_err("Can't find timer ctrl node\n");
		return;
	}

	timer_ctrl_reg = of_iomap(np, 0);
	if (!timer_ctrl_reg) {
		pr_err("%s: Failed to map timer ctrl reg\n", __func__);
		return;
	}

	of_node_put(np);

	ambarella_clockevent_init();
	ambarella_clocksource_init();
}

u32 ambarella_timer_suspend(u32 level)
{
	struct clock_event_device *clkevt = &ambarella_clkevt;
	u32 timer_ctr_mask;

	ambarella_timer_pm.timer_ctr_reg = amba_readl(timer_ctrl_reg);
	ambarella_timer_pm.timer_clk = AMBARELLA_TIMER_FREQ;

	ambarella_timer_pm.timer_ce_status_reg =
		amba_readl(ce_base + TIMER_STATUS_OFFSET);
	ambarella_timer_pm.timer_ce_reload_reg =
		amba_readl(ce_base + TIMER_RELOAD_OFFSET);
	ambarella_timer_pm.timer_ce_match1_reg =
		amba_readl(ce_base + TIMER_MATCH1_OFFSET);
	ambarella_timer_pm.timer_ce_match2_reg =
		amba_readl(ce_base + TIMER_MATCH2_OFFSET);

	ambarella_timer_pm.timer_cs_status_reg =
		amba_readl(cs_base + TIMER_STATUS_OFFSET);
	ambarella_timer_pm.timer_cs_reload_reg =
		amba_readl(cs_base + TIMER_RELOAD_OFFSET);
	ambarella_timer_pm.timer_cs_match1_reg =
		amba_readl(cs_base + TIMER_MATCH1_OFFSET);
	ambarella_timer_pm.timer_cs_match2_reg =
		amba_readl(cs_base + TIMER_MATCH2_OFFSET);

	if (level) {
		disable_irq(clkevt->irq);
		timer_ctr_mask = 0xf << ce_ctrl_offset | 0xf << cs_ctrl_offset;
		amba_clrbitsl(timer_ctrl_reg, timer_ctr_mask);
	}

	return 0;
}

u32 ambarella_timer_resume(u32 level)
{
	struct clock_event_device *clkevt = &ambarella_clkevt;
	struct clocksource *clksrc = &ambarella_cs_timer_clksrc;
	u32 timer_ctr_mask;

	timer_ctr_mask = 0xf << ce_ctrl_offset | 0xf << cs_ctrl_offset;
	amba_clrbitsl(timer_ctrl_reg, timer_ctr_mask);

	amba_writel(cs_base + TIMER_STATUS_OFFSET,
			ambarella_timer_pm.timer_cs_status_reg);
	amba_writel(cs_base + TIMER_RELOAD_OFFSET,
			ambarella_timer_pm.timer_cs_reload_reg);
	amba_writel(cs_base + TIMER_MATCH1_OFFSET,
			ambarella_timer_pm.timer_cs_match1_reg);
	amba_writel(cs_base + TIMER_MATCH2_OFFSET,
			ambarella_timer_pm.timer_cs_match2_reg);

	if ((ambarella_timer_pm.timer_ce_status_reg == 0) &&
		(ambarella_timer_pm.timer_ce_reload_reg == 0)){
		amba_writel(ce_base + TIMER_STATUS_OFFSET,
				AMBARELLA_TIMER_FREQ / HZ);
	} else {
		amba_writel(ce_base + TIMER_STATUS_OFFSET,
				ambarella_timer_pm.timer_ce_status_reg);
	}
	amba_writel(ce_base + TIMER_RELOAD_OFFSET,
			ambarella_timer_pm.timer_ce_reload_reg);
	amba_writel(ce_base + TIMER_MATCH1_OFFSET,
			ambarella_timer_pm.timer_ce_match1_reg);
	amba_writel(ce_base + TIMER_MATCH2_OFFSET,
			ambarella_timer_pm.timer_ce_match2_reg);

	if (ambarella_timer_pm.timer_clk != AMBARELLA_TIMER_FREQ) {
		clockevents_config(clkevt, AMBARELLA_TIMER_FREQ);

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

		clocksource_change_rating(clksrc, 0);
		clksrc->mult = clocksource_hz2mult(
			AMBARELLA_TIMER_FREQ, clksrc->shift);
		pr_debug("%s: mult = %u, shift = %u\n",
			clksrc->name, clksrc->mult, clksrc->shift);
		clocksource_change_rating(clksrc, AMBARELLA_TIMER_RATING);
#if defined(CONFIG_HAVE_SCHED_CLOCK)
		setup_sched_clock(ambarella_read_sched_clock,
			32, AMBARELLA_TIMER_FREQ);
#endif
	}

	amba_setbitsl(timer_ctrl_reg,
			(ambarella_timer_pm.timer_ctr_reg & timer_ctr_mask));
	if (level)
		enable_irq(clkevt->irq);

	return 0;
}
