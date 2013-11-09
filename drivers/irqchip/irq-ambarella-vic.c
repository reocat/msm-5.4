/*
 *  drivers/irqchip/irq-ambarella-vic.c
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

#include <linux/export.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>

#include <linux/irqchip/irq-ambarella-gpio.h>
#include <linux/irqchip/chained_irq.h>

#include <asm/exception.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>

/* ==========================================================================*/
struct ambarella_vic_device_s {
	void __iomem *vic_base;
#if defined(CONFIG_AMBARELLA_VIC_DOMAIN)
	struct irq_domain *domain;
#else
	unsigned int irq_start;
#endif
};
struct ambarella_vic_pm_s {
	u32 int_sel_reg;
	u32 inten_reg;
	u32 soften_reg;
	u32 proten_reg;
	u32 sense_reg;
	u32 bothedge_reg;
	u32 event_reg;
};
struct ambarella_vic_info_s {
	struct ambarella_vic_device_s device_info[VIC_INSTANCES];
	unsigned int device_num;
	struct ambarella_vic_pm_s device_pm[VIC_INSTANCES];
};

static struct ambarella_vic_info_s amba_vic_info
	__attribute__ ((aligned(32))) =
{
	.device_num = 0,
};

/* ==========================================================================*/
#if (VIC_SUPPORT_CPU_OFFLOAD == 1)
static void ambarella_vic_ack_irq(struct irq_data *data)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;

	amba_writel((vic_base + VIC_EDGE_CLR_OFFSET), (0x1 << vic_irq));
}

static void ambarella_vic_mask_irq(struct irq_data *data)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;

	amba_writel((vic_base + VIC_INT_EN_CLR_INT_OFFSET), vic_irq);
}

static void ambarella_vic_unmask_irq(struct irq_data *data)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;

	amba_writel((vic_base + VIC_INT_EN_INT_OFFSET), vic_irq);
}

static void ambarella_vic_mask_ack_irq(struct irq_data *data)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;

	amba_writel((vic_base + VIC_INT_EN_CLR_INT_OFFSET), vic_irq);
	amba_writel((vic_base + VIC_EDGE_CLR_OFFSET), (0x1 << vic_irq));
}

static int ambarella_vic_set_type_irq(struct irq_data *data, unsigned int type)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;
	struct irq_desc *desc = irq_to_desc(data->irq);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		amba_writel((vic_base + VIC_INT_SENSE_CLR_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_BOTHEDGE_CLR_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_EVT_INT_OFFSET),
			vic_irq);
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		amba_writel((vic_base + VIC_INT_SENSE_CLR_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_BOTHEDGE_CLR_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_EVT_CLR_INT_OFFSET),
			vic_irq);
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		amba_writel((vic_base + VIC_INT_SENSE_CLR_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_BOTHEDGE_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_EVT_CLR_INT_OFFSET),
			vic_irq);
		desc->handle_irq = handle_edge_irq;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		amba_writel((vic_base + VIC_INT_SENSE_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_BOTHEDGE_CLR_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_EVT_INT_OFFSET),
			vic_irq);
		desc->handle_irq = handle_level_irq;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		amba_writel((vic_base + VIC_INT_SENSE_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_BOTHEDGE_CLR_INT_OFFSET),
			vic_irq);
		amba_writel((vic_base + VIC_INT_EVT_CLR_INT_OFFSET),
			vic_irq);
		desc->handle_irq = handle_level_irq;
		break;
	default:
		pr_err("%s: irq[%d] type[%d] fail!\n",
			__func__, data->irq, type);
		return -EINVAL;
	}

	ambarella_vic_ack_irq(data);

	return 0;
}
#else
static void ambarella_vic_ack_irq(struct irq_data *data)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;

	amba_writel((vic_base + VIC_EDGE_CLR_OFFSET), (0x1 << vic_irq));
}

static void ambarella_vic_mask_irq(struct irq_data *data)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;

	amba_writel((vic_base + VIC_INTEN_CLR_OFFSET), (0x1 << vic_irq));
}

static void ambarella_vic_unmask_irq(struct irq_data *data)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;

	amba_writel((vic_base + VIC_INTEN_OFFSET), (0x1 << vic_irq));
}

static void ambarella_vic_mask_ack_irq(struct irq_data *data)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;

	amba_writel((vic_base + VIC_INTEN_CLR_OFFSET), (0x1 << vic_irq));
	amba_writel((vic_base + VIC_EDGE_CLR_OFFSET), (0x1 << vic_irq));
}

static int ambarella_vic_set_type_irq(struct irq_data *data, unsigned int type)
{
	void __iomem *vic_base = irq_data_get_irq_chip_data(data);
	unsigned int vic_irq = data->hwirq;
	struct irq_desc *desc = irq_to_desc(data->irq);
	u32 mask;
	u32 bit;
	u32 sense;
	u32 bothedges;
	u32 event;

	mask = ~(0x1 << vic_irq);
	bit = (0x1 << vic_irq);
	sense = amba_readl(vic_base + VIC_SENSE_OFFSET);
	bothedges = amba_readl(vic_base + VIC_BOTHEDGE_OFFSET);
	event = amba_readl(vic_base + VIC_EVENT_OFFSET);
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

	amba_writel((vic_base + VIC_SENSE_OFFSET), sense);
	amba_writel((vic_base + VIC_BOTHEDGE_OFFSET), bothedges);
	amba_writel((vic_base + VIC_EVENT_OFFSET), event);

	ambarella_vic_ack_irq(data);

	return 0;
}
#endif

static struct irq_chip ambarella_vic_chip = {
	.name		= "VIC",
	.irq_ack	= ambarella_vic_ack_irq,
	.irq_mask	= ambarella_vic_mask_irq,
	.irq_mask_ack	= ambarella_vic_mask_ack_irq,
	.irq_unmask	= ambarella_vic_unmask_irq,
	.irq_set_type	= ambarella_vic_set_type_irq,
	.flags		= (IRQCHIP_SET_TYPE_MASKED | IRQCHIP_MASK_ON_SUSPEND |
			IRQCHIP_SKIP_SET_WAKE),
};

/* ==========================================================================*/
static int ambarella_vic_irqdomain_map(struct irq_domain *d,
	unsigned int irq, irq_hw_number_t hwirq)
{
	struct ambarella_vic_device_s *pvic_device;

	pvic_device = (struct ambarella_vic_device_s *)d->host_data;
	irq_set_chip_and_handler(irq, &ambarella_vic_chip, handle_level_irq);
	irq_set_chip_data(irq, pvic_device->vic_base);
	set_irq_flags(irq, (IRQF_VALID | IRQF_PROBE));

	return 0;
}

static int ambarella_vic_handle_one(
	struct ambarella_vic_device_s *pvic_device,
	struct pt_regs *regs)
{
#if (VIC_SUPPORT_CPU_OFFLOAD == 1)
	unsigned int hwirq;
#else
	unsigned int vic_irq;
	unsigned int hwirq;
#endif
	unsigned int irq;
	int handled = 0;

#if (VIC_SUPPORT_CPU_OFFLOAD == 1)
	while ((hwirq = amba_readl(pvic_device->vic_base +
		VIC_INT_PENDING_OFFSET))) {
#else
	while ((vic_irq = amba_readl(pvic_device->vic_base +
		VIC_IRQ_STA_OFFSET))) {
		hwirq = (ffs(vic_irq) - 1);
#endif
#if defined(CONFIG_AMBARELLA_VIC_DOMAIN)
		irq = irq_find_mapping(pvic_device->domain, hwirq);
#else
		irq = (pvic_device->irq_start + hwirq);
#endif
		handle_IRQ(irq, regs);
		handled = 1;
	}

	return handled;
}

static asmlinkage void __exception_irq_entry ambarella_vic_handle_irq(
	struct pt_regs *regs)
{
	int i;
	int handled = 0;

	do {
		handled = 0;
		for (i = 0; i < amba_vic_info.device_num; i++) {
			handled |= ambarella_vic_handle_one(
				&amba_vic_info.device_info[i], regs);
		}
	} while (handled);
}

static struct irq_domain_ops ambarella_vic_irqdomain_ops = {
	.map = ambarella_vic_irqdomain_map,
};

void __init __ambarella_vic_init(struct ambarella_vic_device_s *pvic_device,
	void __iomem *vic_base, unsigned int irq_start)
{
	struct irq_domain *vic_domain;

	amba_writel((vic_base + VIC_INT_SEL_OFFSET), 0x00000000);
	amba_writel((vic_base + VIC_INTEN_OFFSET), 0x00000000);
	amba_writel((vic_base + VIC_INTEN_CLR_OFFSET), 0xffffffff);
	amba_writel((vic_base + VIC_EDGE_CLR_OFFSET), 0xffffffff);
	amba_writel((vic_base + VIC_INT_PTR0_OFFSET), 0xffffffff);

	pvic_device->vic_base = vic_base;
	vic_domain = irq_domain_add_legacy(NULL, 32, irq_start, 0,
		&ambarella_vic_irqdomain_ops, pvic_device);
	irq_create_strict_mappings(vic_domain, irq_start, 0, 32);
#if defined(CONFIG_AMBARELLA_VIC_DOMAIN)
	pvic_device->domain = vic_domain;
#else
	pvic_device->irq_start = irq_start;
#endif
}

void __init ambarella_vic_init_irq(void __iomem *vic_base,
	unsigned int irq_start)
{
	if (amba_vic_info.device_num) {
		pr_err("%s: already inited!\n", __func__);
		return;
	}

	memset(&amba_vic_info, 0, sizeof(amba_vic_info));
	__ambarella_vic_init(&amba_vic_info.device_info[
		amba_vic_info.device_num], vic_base, irq_start);
	amba_vic_info.device_num++;
	set_handle_irq(ambarella_vic_handle_irq);
}

void __init ambarella_vic_add_irq(void __iomem *vic_base,
	unsigned int irq_start)
{
	if (amba_vic_info.device_num >= VIC_INSTANCES) {
		pr_err("%s: No more space for new VIC!\n", __func__);
		return;
	}

	__ambarella_vic_init(&amba_vic_info.device_info[
		amba_vic_info.device_num], vic_base, irq_start);
	amba_vic_info.device_num++;
}

/* ==========================================================================*/
#if defined(CONFIG_PM)
static void ambarella_vic_suspend_one(
	struct ambarella_vic_device_s *pvic_device,
	struct ambarella_vic_pm_s *pvic_pm)
{
	pvic_pm->int_sel_reg =
		amba_readl(pvic_device->vic_base + VIC_INT_SEL_OFFSET);
	pvic_pm->inten_reg =
		amba_readl(pvic_device->vic_base + VIC_INTEN_OFFSET);
	pvic_pm->soften_reg =
		amba_readl(pvic_device->vic_base + VIC_SOFTEN_OFFSET);
	pvic_pm->proten_reg =
		amba_readl(pvic_device->vic_base + VIC_PROTEN_OFFSET);
	pvic_pm->sense_reg =
		amba_readl(pvic_device->vic_base + VIC_SENSE_OFFSET);
	pvic_pm->bothedge_reg =
		amba_readl(pvic_device->vic_base + VIC_BOTHEDGE_OFFSET);
	pvic_pm->event_reg =
		amba_readl(pvic_device->vic_base + VIC_EVENT_OFFSET);

	pr_debug("%s: suspending VIC[%p]\n",
		__func__, pvic_device->vic_base);
	pr_debug("int_sel_reg[0x%08X]\n", pvic_pm->int_sel_reg);
	pr_debug("inten_reg[0x%08X]\n", pvic_pm->inten_reg);
	pr_debug("soften_reg[0x%08X]\n", pvic_pm->soften_reg);
	pr_debug("proten_reg[0x%08X]\n", pvic_pm->proten_reg);
	pr_debug("sense_reg[0x%08X]\n", pvic_pm->sense_reg);
	pr_debug("bothedge_reg[0x%08X]\n", pvic_pm->bothedge_reg);
	pr_debug("event_reg[0x%08X]\n", pvic_pm->event_reg);
}

static int ambarella_vic_suspend(void)
{
	int i;

	for (i = 0; i < amba_vic_info.device_num; i++) {
		ambarella_vic_suspend_one(&amba_vic_info.device_info[i],
			&amba_vic_info.device_pm[i]);
	}

	return 0;
}

static void ambarella_vic_resume_one(
	struct ambarella_vic_device_s *pvic_device,
	struct ambarella_vic_pm_s *pvic_pm)
{
	pr_debug("%s: resuming VIC[%p]\n",
		__func__, pvic_device->vic_base);

	amba_writel((pvic_device->vic_base + VIC_INT_SEL_OFFSET),
		pvic_pm->int_sel_reg);
	amba_writel((pvic_device->vic_base + VIC_INTEN_CLR_OFFSET),
		0xffffffff);
	amba_writel((pvic_device->vic_base + VIC_EDGE_CLR_OFFSET),
		0xffffffff);
	amba_writel((pvic_device->vic_base + VIC_INTEN_OFFSET),
		pvic_pm->inten_reg);
	amba_writel((pvic_device->vic_base + VIC_SOFTEN_CLR_OFFSET),
		0xffffffff);
	amba_writel((pvic_device->vic_base + VIC_SOFTEN_OFFSET),
		pvic_pm->soften_reg);
	amba_writel((pvic_device->vic_base + VIC_PROTEN_OFFSET),
		pvic_pm->proten_reg);
	amba_writel((pvic_device->vic_base + VIC_SENSE_OFFSET),
		pvic_pm->sense_reg);
	amba_writel((pvic_device->vic_base + VIC_BOTHEDGE_OFFSET),
		pvic_pm->bothedge_reg);
	amba_writel((pvic_device->vic_base + VIC_EVENT_OFFSET),
		pvic_pm->event_reg);
}

static void ambarella_vic_resume(void)
{
	int i;

	for (i = (amba_vic_info.device_num - 1); i >= 0; i--) {
		ambarella_vic_resume_one(&amba_vic_info.device_info[i],
			&amba_vic_info.device_pm[i]);
	}
}

struct syscore_ops ambarella_vic_syscore_ops = {
	.suspend	= ambarella_vic_suspend,
	.resume		= ambarella_vic_resume,
};

static int __init ambarella_vic_pm_init(void)
{
	if (amba_vic_info.device_num > 0) {
		register_syscore_ops(&ambarella_vic_syscore_ops);
	}

	return 0;
}
late_initcall(ambarella_vic_pm_init);
#endif /* CONFIG_PM */

/* ==========================================================================*/
#if (VIC_SUPPORT_CPU_OFFLOAD == 1)
void ambarella_vic_sw_set(void __iomem *vic_base, unsigned int vic_irq)
{
	amba_writel((vic_base + VIC_SOFT_INT_INT_OFFSET), vic_irq);
}

void ambarella_vic_sw_clr(void __iomem *vic_base, unsigned int vic_irq)
{
	amba_writel((vic_base + VIC_SOFT_INT_CLR_INT_OFFSET), vic_irq);
}
#else
void ambarella_vic_sw_set(void __iomem *vic_base, unsigned int vic_irq)
{
	amba_writel((vic_base + VIC_SOFTEN_OFFSET), (0x1 << vic_irq));
}

void ambarella_vic_sw_clr(void __iomem *vic_base, unsigned int vic_irq)
{
	amba_writel((vic_base + VIC_SOFTEN_CLR_OFFSET), (0x1 << vic_irq));
}
#endif

