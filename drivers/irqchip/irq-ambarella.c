/*
 * drivers/irqchip/irq-ambarella.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
 *
 * Copyright (C) 2004-2013, Ambarella, Inc.
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
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <asm/exception.h>
#include "irqchip.h"

#ifdef CONFIG_PLAT_AMBARELLA_BOSS
#include <mach/boss.h>
#endif


#define HWIRQ_TO_BANK(hwirq)	((hwirq) >> 5)
#define HWIRQ_TO_OFFSET(hwirq)	((hwirq) & 0x1f)

struct ambvic_pm_reg {
	u32 int_sel_reg;
	u32 inten_reg;
	u32 soften_reg;
	u32 proten_reg;
	u32 sense_reg;
	u32 bothedge_reg;
	u32 event_reg;
};

struct ambvic_chip_data {
	void __iomem *reg_base[VIC_INSTANCES];
	struct irq_domain *domain;
	struct ambvic_pm_reg pm_reg[VIC_INSTANCES];
};

static struct ambvic_chip_data ambvic_data __read_mostly;

/* ==========================================================================*/
#if (VIC_SUPPORT_CPU_OFFLOAD == 1)

static void ambvic_ack_irq(struct irq_data *data)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

	amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0x1 << offset);
}

static void ambvic_mask_irq(struct irq_data *data)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

	amba_writel(reg_base + VIC_INT_EN_CLR_INT_OFFSET, offset);

#ifdef CONFIG_PLAT_AMBARELLA_BOSS
	/* Disable in BOSS. */
	boss_disable_irq(data->hwirq);
#endif
}

static void ambvic_unmask_irq(struct irq_data *data)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

#ifdef CONFIG_PLAT_AMBARELLA_BOSS
	/* Enable in BOSS. */
	boss_enable_irq(data->hwirq);
#endif

	amba_writel(reg_base + VIC_INT_EN_INT_OFFSET, offset);
}

static void ambvic_mask_ack_irq(struct irq_data *data)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

	amba_writel(reg_base + VIC_INT_EN_CLR_INT_OFFSET, offset);
	amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0x1 << offset);

#ifdef CONFIG_PLAT_AMBARELLA_BOSS
	/* Disable in BOSS. */
	boss_disable_irq(data->hwirq);
#endif
}

static int ambvic_set_type_irq(struct irq_data *data, unsigned int type)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	struct irq_desc *desc = irq_to_desc(data->irq);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		amba_writel(reg_base + VIC_INT_SENSE_CLR_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_BOTHEDGE_CLR_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_EVT_INT_OFFSET, offset);
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		amba_writel(reg_base + VIC_INT_SENSE_CLR_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_BOTHEDGE_CLR_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_EVT_CLR_INT_OFFSET, offset);
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		amba_writel(reg_base + VIC_INT_SENSE_CLR_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_BOTHEDGE_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_EVT_CLR_INT_OFFSET, offset);
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		amba_writel(reg_base + VIC_INT_SENSE_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_BOTHEDGE_CLR_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_EVT_INT_OFFSET, offset);
		desc->handle_irq = handle_level_irq;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		amba_writel(reg_base + VIC_INT_SENSE_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_BOTHEDGE_CLR_INT_OFFSET, offset);
		amba_writel(reg_base + VIC_INT_EVT_CLR_INT_OFFSET, offset);
		desc->handle_irq = handle_level_irq;
		break;
	default:
		pr_err("%s: irq[%d] type[%d] fail!\n",
			__func__, data->irq, type);
		return -EINVAL;
	}

	/* clear obsolete irq */
	amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0x1 << offset);

	return 0;
}

#else
static void ambvic_ack_irq(struct irq_data *data)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

	amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0x1 << offset);
}

static void ambvic_mask_irq(struct irq_data *data)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

	amba_writel(reg_base + VIC_INTEN_CLR_OFFSET, 0x1 << offset);
}

static void ambvic_unmask_irq(struct irq_data *data)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

	amba_writel(reg_base + VIC_INTEN_OFFSET, 0x1 << offset);
}

static void ambvic_mask_ack_irq(struct irq_data *data)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);

	amba_writel(reg_base + VIC_INTEN_CLR_OFFSET, 0x1 << offset);
	amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0x1 << offset);
}

static int ambvic_set_type_irq(struct irq_data *data, unsigned int type)
{
	void __iomem *reg_base = irq_data_get_irq_chip_data(data);
	struct irq_desc *desc = irq_to_desc(data->irq);
	u32 offset = HWIRQ_TO_OFFSET(data->hwirq);
	u32 mask, bit, sense, bothedges, event;

	mask = ~(0x1 << offset);
	bit = (0x1 << offset);
	sense = amba_readl(reg_base + VIC_SENSE_OFFSET);
	bothedges = amba_readl(reg_base + VIC_BOTHEDGE_OFFSET);
	event = amba_readl(reg_base + VIC_EVENT_OFFSET);
	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		sense &= mask;
		bothedges &= mask;
		event |= bit;
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		sense &= mask;
		bothedges &= mask;
		event &= mask;
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		sense &= mask;
		bothedges |= bit;
		event &= mask;
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		sense |= bit;
		bothedges &= mask;
		event |= bit;
		desc->handle_irq = handle_level_irq;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		sense |= bit;
		bothedges &= mask;
		event &= mask;
		desc->handle_irq = handle_level_irq;
		break;
	default:
		pr_err("%s: irq[%d] type[%d] fail!\n",
			__func__, data->irq, type);
		return -EINVAL;
	}

	amba_writel(reg_base + VIC_SENSE_OFFSET, sense);
	amba_writel(reg_base + VIC_BOTHEDGE_OFFSET, bothedges);
	amba_writel(reg_base + VIC_EVENT_OFFSET, event);
	/* clear obsolete irq */
	amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0x1 << offset);

	return 0;
}

#endif

static struct irq_chip ambvic_chip = {
	.name		= "VIC",
	.irq_ack	= ambvic_ack_irq,
	.irq_mask	= ambvic_mask_irq,
	.irq_mask_ack	= ambvic_mask_ack_irq,
	.irq_unmask	= ambvic_unmask_irq,
	.irq_set_type	= ambvic_set_type_irq,
	.flags		= (IRQCHIP_SET_TYPE_MASKED | IRQCHIP_MASK_ON_SUSPEND |
			IRQCHIP_SKIP_SET_WAKE),
};

/* ==========================================================================*/

static int ambvic_handle_one(struct pt_regs *regs,
		struct irq_domain *domain, u32 bank)
{
	void __iomem *reg_base = ambvic_data.reg_base[bank];
	u32 irq, hwirq;
	int handled = 0;

	do {
#if (VIC_SUPPORT_CPU_OFFLOAD == 0)
		u32 irq_sta;

		irq_sta = amba_readl(reg_base + VIC_IRQ_STA_OFFSET);
		if (irq_sta == 0)
			break;
		hwirq = ffs(irq_sta) - 1;
#else
#ifdef CONFIG_PLAT_AMBARELLA_BOSS
		hwirq = *boss->irqno;
#else
		hwirq = amba_readl(reg_base + VIC_INT_PENDING_OFFSET);
#endif
		if (hwirq == 0)
			break;
#endif
		hwirq += bank * NR_VIC_IRQ_SIZE;
		irq = irq_find_mapping(domain, hwirq);
		handle_IRQ(irq, regs);
		handled = 1;
#ifdef CONFIG_PLAT_AMBARELLA_BOSS
		/* Linux will continue to process interrupt, */
		/* but we must enter IRQ from AmbaBoss_Irq in BOSS system. */
		break;
#endif
	} while (1);

	return handled;
}

static asmlinkage void __exception_irq_entry ambvic_handle_irq(struct pt_regs *regs)
{
#ifdef CONFIG_PLAT_AMBARELLA_BOSS
	ambvic_handle_one(regs, ambvic_data.domain, 0);
#else
	int i, handled;

	do {
		handled = 0;
		for (i = 0; i < VIC_INSTANCES; i++) {
			handled |= ambvic_handle_one(regs, ambvic_data.domain, i);
		}
	} while (handled);
#endif
}

static int ambvic_irq_domain_map(struct irq_domain *d,
		unsigned int irq, irq_hw_number_t hwirq)
{
	if (hwirq > NR_VIC_IRQS)
		return -EPERM;

	irq_set_chip_and_handler(irq, &ambvic_chip, handle_level_irq);
	irq_set_chip_data(irq, ambvic_data.reg_base[HWIRQ_TO_BANK(hwirq)]);
	set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);

	return 0;
}

static struct irq_domain_ops amb_irq_domain_ops = {
	.map = ambvic_irq_domain_map,
	.xlate = irq_domain_xlate_twocell,
};

int __init ambvic_of_init(struct device_node *np, struct device_node *parent)
{
	void __iomem *reg_base;
	int i, irq;

	memset(&ambvic_data, 0, sizeof(struct ambvic_chip_data));

	for (i = 0; i < VIC_INSTANCES; i++) {
		reg_base = of_iomap(np, i);
		BUG_ON(!reg_base);
#ifndef CONFIG_PLAT_AMBARELLA_BOSS
		amba_writel(reg_base + VIC_INT_SEL_OFFSET, 0x00000000);
		amba_writel(reg_base + VIC_INTEN_OFFSET, 0x00000000);
		amba_writel(reg_base + VIC_INTEN_CLR_OFFSET, 0xffffffff);
		amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0xffffffff);
		amba_writel(reg_base + VIC_INT_PTR0_OFFSET, 0xffffffff);
#endif
		ambvic_data.reg_base[i] = reg_base;
	}

	set_handle_irq(ambvic_handle_irq);

	ambvic_data.domain = irq_domain_add_linear(np,
			NR_VIC_IRQS, &amb_irq_domain_ops, NULL);
	BUG_ON(!ambvic_data.domain);

	// WORKAROUND only, will be removed finally
	for (i = 1; i < NR_VIC_IRQS; i++) {
		irq = irq_create_mapping(ambvic_data.domain, i);
		irq_set_chip_and_handler(irq, &ambvic_chip, handle_level_irq);
		irq_set_chip_data(irq, ambvic_data.reg_base[HWIRQ_TO_BANK(i)]);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	return 0;
}

IRQCHIP_DECLARE(ambvic, "ambarella,vic", ambvic_of_init);

/* ==========================================================================*/
#if defined(CONFIG_PM)

static int ambvic_suspend(void)
{
	struct ambvic_pm_reg *pm_reg;
	void __iomem *reg_base;
	int i;

	for (i = 0; i < VIC_INSTANCES; i++) {
		reg_base = ambvic_data.reg_base[i];
		pm_reg = &ambvic_data.pm_reg[i];

		pm_reg->int_sel_reg = amba_readl(reg_base + VIC_INT_SEL_OFFSET);
		pm_reg->inten_reg = amba_readl(reg_base + VIC_INTEN_OFFSET);
		pm_reg->soften_reg = amba_readl(reg_base + VIC_SOFTEN_OFFSET);
		pm_reg->proten_reg = amba_readl(reg_base + VIC_PROTEN_OFFSET);
		pm_reg->sense_reg = amba_readl(reg_base + VIC_SENSE_OFFSET);
		pm_reg->bothedge_reg = amba_readl(reg_base + VIC_BOTHEDGE_OFFSET);
		pm_reg->event_reg = amba_readl(reg_base + VIC_EVENT_OFFSET);
	}

	return 0;
}

static void ambvic_resume(void)
{
	struct ambvic_pm_reg *pm_reg;
	void __iomem *reg_base;
	int i;

#ifndef CONFIG_PLAT_AMBARELLA_BOSS
	for (i = VIC_INSTANCES - 1; i >= 0; i--) {
		reg_base = ambvic_data.reg_base[i];
		pm_reg = &ambvic_data.pm_reg[i];

		amba_writel(reg_base + VIC_INT_SEL_OFFSET, pm_reg->int_sel_reg);
		amba_writel(reg_base + VIC_INTEN_CLR_OFFSET, 0xffffffff);
		amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0xffffffff);
		amba_writel(reg_base + VIC_INTEN_OFFSET, pm_reg->inten_reg);
		amba_writel(reg_base + VIC_SOFTEN_CLR_OFFSET, 0xffffffff);
		amba_writel(reg_base + VIC_SOFTEN_OFFSET, pm_reg->soften_reg);
		amba_writel(reg_base + VIC_PROTEN_OFFSET, pm_reg->proten_reg);
		amba_writel(reg_base + VIC_SENSE_OFFSET, pm_reg->sense_reg);
		amba_writel(reg_base + VIC_BOTHEDGE_OFFSET, pm_reg->bothedge_reg);
		amba_writel(reg_base + VIC_EVENT_OFFSET, pm_reg->event_reg);
	}
#else
	/* Should we only restore the setting of Linux's driver?  */
	for (i = VIC_INSTANCES - 1; i >= 0; i--) {
		reg_base = ambvic_data.reg_base[i];
		pm_reg = &ambvic_data.pm_reg[i];

		amba_writel(reg_base + VIC_INT_SEL_OFFSET, pm_reg->int_sel_reg);
		//amba_writel(reg_base + VIC_INTEN_CLR_OFFSET, 0xffffffff);
		//amba_writel(reg_base + VIC_EDGE_CLR_OFFSET, 0xffffffff);
		//amba_writel(reg_base + VIC_INTEN_OFFSET, pm_reg->inten_reg);
		//amba_writel(reg_base + VIC_SOFTEN_CLR_OFFSET, 0xffffffff);
		amba_writel(reg_base + VIC_SOFTEN_OFFSET, pm_reg->soften_reg);
		//amba_writel(reg_base + VIC_PROTEN_OFFSET, pm_reg->proten_reg);
		amba_writel(reg_base + VIC_SENSE_OFFSET, pm_reg->sense_reg);
		amba_writel(reg_base + VIC_BOTHEDGE_OFFSET, pm_reg->bothedge_reg);
		amba_writel(reg_base + VIC_EVENT_OFFSET, pm_reg->event_reg);
	}
#endif
}

struct syscore_ops ambvic_syscore_ops = {
	.suspend	= ambvic_suspend,
	.resume		= ambvic_resume,
};

static int __init ambvic_pm_init(void)
{
	register_syscore_ops(&ambvic_syscore_ops);
	return 0;
}
late_initcall(ambvic_pm_init);
#endif /* CONFIG_PM */


/* ==========================================================================*/
#if (VIC_SUPPORT_CPU_OFFLOAD == 1)
void ambvic_sw_set(void __iomem *reg_base, u32 offset)
{
	amba_writel(reg_base + VIC_SOFT_INT_INT_OFFSET, offset);
}

void ambvic_sw_clr(void __iomem *reg_base, u32 offset)
{
	amba_writel(reg_base + VIC_SOFT_INT_CLR_INT_OFFSET, offset);
}
#else
void ambvic_sw_set(void __iomem *reg_base, u32 offset)
{
	amba_writel(reg_base + VIC_SOFTEN_OFFSET, 0x1 << offset);
}

void ambvic_sw_clr(void __iomem *reg_base, u32 offset)
{
	amba_writel(reg_base + VIC_SOFTEN_CLR_OFFSET, 0x1 << offset);
}
#endif


