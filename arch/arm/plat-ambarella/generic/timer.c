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
 * Timer 3 is used as clock_event_device.
 * Timer 2 is used as free-running clocksource
 *         if CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE is defined
 * Timer 1 is not used.
 */

#include <linux/interrupt.h>
#include <linux/clockchips.h>
#include <linux/delay.h>
#include <linux/clk.h>

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
static u32 ambarella_timer_get_pll(void)
{
	return clk_get_rate(clk_get(NULL, "gclk_apb"));
}

#define AMBARELLA_TIMER_FREQ			(ambarella_timer_get_pll())
#define AMBARELLA_TIMER_RATING			(300)

/* ==========================================================================*/
struct ambarella_timer_regs {
	u32 status_reg;
	u32 reload_reg;
	u32 match1_reg;
	u32 match2_reg;
	u32 irq;
	u32 ctr_en;
	u32 ctr_of;
	u32 ctr_csl;
	u32 ctr_mask;
};

static struct ambarella_timer_regs default_ce_regs = {
	.status_reg = TIMER3_STATUS_REG, /* TIMER_STATUS_REG */
	.reload_reg = TIMER3_RELOAD_REG, /* TIMER_RELOAD_REG */
	.match1_reg = TIMER3_MATCH1_REG, /* TIMER_MATCH1_REG */
	.match2_reg = TIMER3_MATCH2_REG, /* TIMER_MATCH2_REG */
	.irq        = TIMER3_IRQ,        /* TIMER_IRQ */
	.ctr_en     = TIMER_CTR_EN3,     /* TIMER_CTR_EN */
	.ctr_of     = TIMER_CTR_OF3,     /* TIMER_CTR_OF */
	.ctr_csl    = TIMER_CTR_CSL3,    /* TIMER_CTR_CSL */
	.ctr_mask   = 0x00000f00,        /* TIMER_CTR_MASK */
};

static struct ambarella_timer_regs default_cs_regs = {
	.status_reg = TIMER2_STATUS_REG, /* TIMER_STATUS_REG */
	.reload_reg = TIMER2_RELOAD_REG, /* TIMER_RELOAD_REG */
	.match1_reg = TIMER2_MATCH1_REG, /* TIMER_MATCH1_REG */
	.match2_reg = TIMER2_MATCH2_REG, /* TIMER_MATCH2_REG */
	.irq        = TIMER2_IRQ,        /* TIMER_IRQ */
	.ctr_en     = TIMER_CTR_EN2,     /* TIMER_CTR_EN */
	.ctr_of     = TIMER_CTR_OF2,     /* TIMER_CTR_OF */
	.ctr_csl    = TIMER_CTR_CSL2,    /* TIMER_CTR_CSL */
	.ctr_mask   = 0x000000f0,        /* TIMER_CTR_MASK */
};

#if defined(CONFIG_MACH_HYACINTH_0) || defined(CONFIG_MACH_HYACINTH_1)
static struct ambarella_timer_regs hyacinth0_ce_regs = {
	.status_reg = TIMER6_STATUS_REG, /* TIMER_STATUS_REG */
	.reload_reg = TIMER6_RELOAD_REG, /* TIMER_RELOAD_REG */
	.match1_reg = TIMER6_MATCH1_REG, /* TIMER_MATCH1_REG */
	.match2_reg = TIMER6_MATCH2_REG, /* TIMER_MATCH2_REG */
	.irq        = TIMER6_IRQ,        /* TIMER_IRQ */
	.ctr_en     = TIMER_CTR_EN6,     /* TIMER_CTR_EN */
	.ctr_of     = TIMER_CTR_OF6,     /* TIMER_CTR_OF */
	.ctr_csl    = TIMER_CTR_CSL6,    /* TIMER_CTR_CSL */
	.ctr_mask   = 0x00f00000,        /* TIMER_CTR_MASK */
};

static struct ambarella_timer_regs hyacinth0_cs_regs = {
	.status_reg = TIMER5_STATUS_REG, /* TIMER_STATUS_REG */
	.reload_reg = TIMER5_RELOAD_REG, /* TIMER_RELOAD_REG */
	.match1_reg = TIMER5_MATCH1_REG, /* TIMER_MATCH1_REG */
	.match2_reg = TIMER5_MATCH2_REG, /* TIMER_MATCH2_REG */
	.irq        = TIMER5_IRQ,        /* TIMER_IRQ */
	.ctr_en     = TIMER_CTR_EN5,     /* TIMER_CTR_EN */
	.ctr_of     = TIMER_CTR_OF5,     /* TIMER_CTR_OF */
	.ctr_csl    = TIMER_CTR_CSL5,    /* TIMER_CTR_CSL */
	.ctr_mask   = 0x000f0000,        /* TIMER_CTR_MASK */
};

static struct ambarella_timer_regs hyacinth1_ce_regs = {
	.status_reg = TIMER8_STATUS_REG, /* TIMER_STATUS_REG */
	.reload_reg = TIMER8_RELOAD_REG, /* TIMER_RELOAD_REG */
	.match1_reg = TIMER8_MATCH1_REG, /* TIMER_MATCH1_REG */
	.match2_reg = TIMER8_MATCH2_REG, /* TIMER_MATCH2_REG */
	.irq        = TIMER8_IRQ,        /* TIMER_IRQ */
	.ctr_en     = TIMER_CTR_EN8,     /* TIMER_CTR_EN */
	.ctr_of     = TIMER_CTR_OF8,     /* TIMER_CTR_OF */
	.ctr_csl    = TIMER_CTR_CSL8,    /* TIMER_CTR_CSL */
	.ctr_mask   = 0xf0000000,        /* TIMER_CTR_MASK */
};

static struct ambarella_timer_regs hyacinth1_cs_regs = {
	.status_reg = TIMER7_STATUS_REG, /* TIMER_STATUS_REG */
	.reload_reg = TIMER7_RELOAD_REG, /* TIMER_RELOAD_REG */
	.match1_reg = TIMER7_MATCH1_REG, /* TIMER_MATCH1_REG */
	.match2_reg = TIMER7_MATCH2_REG, /* TIMER_MATCH2_REG */
	.irq        = TIMER7_IRQ,        /* TIMER_IRQ */
	.ctr_en     = TIMER_CTR_EN7,     /* TIMER_CTR_EN */
	.ctr_of     = TIMER_CTR_OF7,     /* TIMER_CTR_OF */
	.ctr_csl    = TIMER_CTR_CSL7,    /* TIMER_CTR_CSL */
	.ctr_mask   = 0x0f000000,        /* TIMER_CTR_MASK */
};
#endif

static struct ambarella_timer_regs *ambarella_timer_ce_regs = &default_ce_regs;
static struct ambarella_timer_regs *ambarella_timer_cs_regs = &default_cs_regs;

/* ==========================================================================*/

struct ambarella_timer_pm_info {
	u32 timer_clk;
	u32 timer_ctr_reg;
	u32 timer_ce_status_reg;
	u32 timer_ce_reload_reg;
	u32 timer_ce_match1_reg;
	u32 timer_ce_match2_reg;
#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)
	u32 timer_cs_status_reg;
	u32 timer_cs_reload_reg;
	u32 timer_cs_match1_reg;
	u32 timer_cs_match2_reg;
#endif
};

struct ambarella_timer_pm_info ambarella_timer_pm;
static struct clock_event_device ambarella_clkevt;
static struct irqaction ambarella_ce_timer_irq;

/* ==========================================================================*/

static inline void ambarella_ce_timer_disable(void)
{
	amba_clrbitsl(TIMER_CTR_REG, ambarella_timer_ce_regs->ctr_en);
}

static inline void ambarella_ce_timer_enable(void)
{
	amba_setbitsl(TIMER_CTR_REG, ambarella_timer_ce_regs->ctr_en);
}

static inline void ambarella_ce_timer_misc(void)
{
	amba_setbitsl(TIMER_CTR_REG, ambarella_timer_ce_regs->ctr_of);
	amba_clrbitsl(TIMER_CTR_REG, ambarella_timer_ce_regs->ctr_csl);
}

static inline void ambarella_ce_timer_set_periodic(void)
{
	u32					cnt;

	cnt = AMBARELLA_TIMER_FREQ / HZ;
	amba_writel(ambarella_timer_ce_regs->status_reg, cnt);
	amba_writel(ambarella_timer_ce_regs->reload_reg, cnt);
	amba_writel(ambarella_timer_ce_regs->match1_reg, 0x0);
	amba_writel(ambarella_timer_ce_regs->match2_reg, 0x0);
	ambarella_ce_timer_misc();
}

static inline void ambarella_ce_timer_set_oneshot(void)
{
	amba_writel(ambarella_timer_ce_regs->status_reg, 0x0);
	amba_writel(ambarella_timer_ce_regs->reload_reg, 0xffffffff);
	amba_writel(ambarella_timer_ce_regs->match1_reg, 0x0);
	amba_writel(ambarella_timer_ce_regs->match2_reg, 0x0);
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
	amba_writel(ambarella_timer_ce_regs->status_reg, delta);

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
	ambarella_clkevt.event_handler(&ambarella_clkevt);

	return IRQ_HANDLED;
}

static struct irqaction ambarella_ce_timer_irq = {
	.name		= "ambarella-ce-timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_TRIGGER_RISING,
	.handler	= ambarella_ce_timer_interrupt,
};

/* ==========================================================================*/
#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)

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
	amba_clrbitsl(TIMER_CTR_REG, ambarella_timer_cs_regs->ctr_en);
	amba_clrbitsl(TIMER_CTR_REG, ambarella_timer_cs_regs->ctr_of);
	amba_clrbitsl(TIMER_CTR_REG, ambarella_timer_cs_regs->ctr_csl);
	amba_writel(ambarella_timer_cs_regs->status_reg, 0xffffffff);
	amba_writel(ambarella_timer_cs_regs->reload_reg, 0xffffffff);
	amba_writel(ambarella_timer_cs_regs->match1_reg, 0x0);
	amba_writel(ambarella_timer_cs_regs->match2_reg, 0x0);
	amba_setbitsl(TIMER_CTR_REG, ambarella_timer_cs_regs->ctr_en);
}

static cycle_t ambarella_cs_timer_read(struct clocksource *cs)
{
	return (-(u32)amba_readl(ambarella_timer_cs_regs->status_reg));
}

static struct clocksource ambarella_cs_timer_clksrc = {
	.name		= "ambarella-cs-timer",
	.rating		= AMBARELLA_TIMER_RATING,
	.read		= ambarella_cs_timer_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.mult		= 2236962133u,
	.shift		= 27,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static u32 notrace ambarella_read_sched_clock(void)
{
	return (-(u32)amba_readl(ambarella_timer_cs_regs->status_reg));
}
#endif /* defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE) */

#if defined(CONFIG_HAVE_ARM_TWD)
static DEFINE_TWD_LOCAL_TIMER(twd_local_timer,
	AMBARELLA_PA_PT_WD_BASE, IRQ_LOCALTIMER);

static void __init ambarella_twd_init(void)
{
       int err = twd_local_timer_register(&twd_local_timer);
       if (err)
               pr_err("twd_local_timer_register failed %d\n", err);
}
#else
#define ambarella_twd_init()       do {} while(0)
#endif
/* ==========================================================================*/
void __init ambarella_timer_init(void)
{
	int ret = 0;

#if defined(CONFIG_MACH_HYACINTH_0) || defined(CONFIG_MACH_HYACINTH_1)
	if (machine_is_hyacinth_0()) {
		ambarella_timer_ce_regs = &hyacinth0_ce_regs;
		ambarella_timer_cs_regs = &hyacinth0_cs_regs;
	}
	else if (machine_is_hyacinth_1()) {
		ambarella_timer_ce_regs = &hyacinth1_ce_regs;
		ambarella_timer_cs_regs = &hyacinth1_cs_regs;
	}
#endif

#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)
	ambarella_cs_timer_init();
	ambarella_cs_timer_clksrc.mult = clocksource_hz2mult(
		AMBARELLA_TIMER_FREQ, ambarella_cs_timer_clksrc.shift);
	pr_debug("%s: mult = %u, shift = %u\n",
		ambarella_cs_timer_clksrc.name,
		ambarella_cs_timer_clksrc.mult,
		ambarella_cs_timer_clksrc.shift);
	clocksource_register(&ambarella_cs_timer_clksrc);
	setup_sched_clock(ambarella_read_sched_clock, 32, AMBARELLA_TIMER_FREQ);
#endif

	ambarella_clkevt.cpumask = cpumask_of(0);
	ambarella_clkevt.irq = ambarella_timer_ce_regs->irq;
	ret = setup_irq(ambarella_clkevt.irq, &ambarella_ce_timer_irq);
	if (ret) {
		printk(KERN_ERR "Failed to register timer IRQ: %d\n", ret);
		BUG();
	}
	clockevents_calc_mult_shift(&ambarella_clkevt, AMBARELLA_TIMER_FREQ, 5);
	ambarella_clkevt.max_delta_ns =
		clockevent_delta2ns(0xffffffff, &ambarella_clkevt);
	ambarella_clkevt.min_delta_ns =
		clockevent_delta2ns(1, &ambarella_clkevt);
	clockevents_register_device(&ambarella_clkevt);
	ambarella_twd_init();
}

u32 ambarella_timer_suspend(u32 level)
{
	u32 timer_ctr_mask;

	ambarella_timer_pm.timer_ctr_reg = amba_readl(TIMER_CTR_REG);
	ambarella_timer_pm.timer_clk = AMBARELLA_TIMER_FREQ;

	ambarella_timer_pm.timer_ce_status_reg =
		amba_readl(ambarella_timer_ce_regs->status_reg);
	ambarella_timer_pm.timer_ce_reload_reg =
		amba_readl(ambarella_timer_ce_regs->reload_reg);
	ambarella_timer_pm.timer_ce_match1_reg =
		amba_readl(ambarella_timer_ce_regs->match1_reg);
	ambarella_timer_pm.timer_ce_match2_reg =
		amba_readl(ambarella_timer_ce_regs->match2_reg);
#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)
	ambarella_timer_pm.timer_cs_status_reg =
		amba_readl(ambarella_timer_ce_regs->status_reg);
	ambarella_timer_pm.timer_cs_reload_reg =
		amba_readl(ambarella_timer_ce_regs->reload_reg);
	ambarella_timer_pm.timer_cs_match1_reg =
		amba_readl(ambarella_timer_ce_regs->match1_reg);
	ambarella_timer_pm.timer_cs_match2_reg =
		amba_readl(ambarella_timer_ce_regs->match2_reg);
#endif

	if (level) {
		disable_irq(ambarella_timer_ce_regs->irq);
		timer_ctr_mask = ambarella_timer_ce_regs->ctr_mask;
#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)
		timer_ctr_mask |= ambarella_timer_cs_regs->ctr_mask;
#endif
		amba_clrbitsl(TIMER_CTR_REG, timer_ctr_mask);
	}

	return 0;
}

u32 ambarella_timer_resume(u32 level)
{
	u32 timer_ctr_mask;

	timer_ctr_mask = ambarella_timer_ce_regs->ctr_mask;
#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)
	timer_ctr_mask |= ambarella_timer_cs_regs->ctr_mask;
#endif
	amba_clrbitsl(TIMER_CTR_REG, timer_ctr_mask);
#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)
	amba_writel(ambarella_timer_cs_regs->status_reg,
		ambarella_timer_pm.timer_cs_status_reg);
	amba_writel(ambarella_timer_cs_regs->reload_reg,
		ambarella_timer_pm.timer_cs_reload_reg);
	amba_writel(ambarella_timer_cs_regs->match1_reg,
		ambarella_timer_pm.timer_cs_match1_reg);
	amba_writel(ambarella_timer_cs_regs->match2_reg,
		ambarella_timer_pm.timer_cs_match2_reg);
#endif
	if ((ambarella_timer_pm.timer_ce_status_reg == 0) &&
		(ambarella_timer_pm.timer_ce_reload_reg == 0)){
		amba_writel(ambarella_timer_ce_regs->status_reg,
			AMBARELLA_TIMER_FREQ / HZ);
	} else {
		amba_writel(ambarella_timer_ce_regs->status_reg,
			ambarella_timer_pm.timer_ce_status_reg);
	}
	amba_writel(ambarella_timer_ce_regs->reload_reg,
		ambarella_timer_pm.timer_ce_reload_reg);
	amba_writel(ambarella_timer_ce_regs->match1_reg,
		ambarella_timer_pm.timer_ce_match1_reg);
	amba_writel(ambarella_timer_ce_regs->match2_reg,
		ambarella_timer_pm.timer_ce_match2_reg);

	if (ambarella_timer_pm.timer_clk != AMBARELLA_TIMER_FREQ) {
		clockevents_calc_mult_shift(&ambarella_clkevt,
				AMBARELLA_TIMER_FREQ, 5);
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

#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)
		clocksource_change_rating(&ambarella_cs_timer_clksrc, 0);
		ambarella_cs_timer_clksrc.mult = clocksource_hz2mult(
			AMBARELLA_TIMER_FREQ, ambarella_cs_timer_clksrc.shift);
		pr_debug("%s: mult = %u, shift = %u\n",
			ambarella_cs_timer_clksrc.name,
			ambarella_cs_timer_clksrc.mult,
			ambarella_cs_timer_clksrc.shift);
		clocksource_change_rating(&ambarella_cs_timer_clksrc,
			AMBARELLA_TIMER_RATING);
#if defined(CONFIG_HAVE_SCHED_CLOCK)
		setup_sched_clock(ambarella_read_sched_clock,
			32, AMBARELLA_TIMER_FREQ);
#endif
#endif
	}

	timer_ctr_mask = ambarella_timer_ce_regs->ctr_mask;
#if defined(CONFIG_AMBARELLA_SUPPORT_CLOCKSOURCE)
	timer_ctr_mask |= ambarella_timer_cs_regs->ctr_mask;
#endif
	amba_setbitsl(TIMER_CTR_REG,
		(ambarella_timer_pm.timer_ctr_reg & timer_ctr_mask));
	if (level) {
		enable_irq(ambarella_timer_ce_regs->irq);
	}

	return 0;
}
