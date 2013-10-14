/*
 *  drivers/irqchip/irq-ambarella-gpio.c
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
struct ambarella_gvic_device_s {
	void __iomem *gpio_base;
	struct irq_domain *domain;
	unsigned int irq_start;
	unsigned int irq_wake;
};
struct ambarella_gvic_pm_s {
	u32 data_reg;
	u32 dir_reg;
	u32 is_reg;
	u32 ibe_reg;
	u32 iev_reg;
	u32 ie_reg;
	u32 afsel_reg;
	u32 mask_reg;
};
struct ambarella_gvic_info_s {
	struct ambarella_gvic_device_s device_info[GPIO_INSTANCES];
	unsigned int device_num;
	struct ambarella_gvic_pm_s device_pm[GPIO_INSTANCES];
};

static struct ambarella_gvic_info_s amba_gvic_info
	__attribute__ ((aligned(32))) =
{
	.device_num = 0,
};

/* ==========================================================================*/
static void ambarella_gvic_enable_irq(struct irq_data *data)
{
	struct ambarella_gvic_device_s *pgvic_device;
	void __iomem *gpio_base;
	unsigned int gpio_irq;

	pgvic_device = irq_data_get_irq_chip_data(data);
	gpio_base = pgvic_device->gpio_base;
	gpio_irq = data->hwirq;

	amba_writel((gpio_base + GPIO_IC_OFFSET), (0x1 << gpio_irq));
	amba_setbitsl((gpio_base + GPIO_IE_OFFSET), (0x1 << gpio_irq));
}

static void ambarella_gvic_disable_irq(struct irq_data *data)
{
	struct ambarella_gvic_device_s *pgvic_device;
	void __iomem *gpio_base;
	unsigned int gpio_irq;

	pgvic_device = irq_data_get_irq_chip_data(data);
	gpio_base = pgvic_device->gpio_base;
	gpio_irq = data->hwirq;

	amba_clrbitsl((gpio_base + GPIO_IE_OFFSET), (0x1 << gpio_irq));
	amba_writel((gpio_base + GPIO_IC_OFFSET), (0x1 << gpio_irq));
}

static void ambarella_gvic_ack_irq(struct irq_data *data)
{
	struct ambarella_gvic_device_s *pgvic_device;
	void __iomem *gpio_base;
	unsigned int gpio_irq;

	pgvic_device = irq_data_get_irq_chip_data(data);
	gpio_base = pgvic_device->gpio_base;
	gpio_irq = data->hwirq;

	amba_writel((gpio_base + GPIO_IC_OFFSET), (0x1 << gpio_irq));
}

static void ambarella_gvic_mask_irq(struct irq_data *data)
{
	struct ambarella_gvic_device_s *pgvic_device;
	void __iomem *gpio_base;
	unsigned int gpio_irq;

	pgvic_device = irq_data_get_irq_chip_data(data);
	gpio_base = pgvic_device->gpio_base;
	gpio_irq = data->hwirq;

	amba_clrbitsl((gpio_base + GPIO_IE_OFFSET), (0x1 << gpio_irq));
}

static void ambarella_gvic_mask_ack_irq(struct irq_data *data)
{
	struct ambarella_gvic_device_s *pgvic_device;
	void __iomem *gpio_base;
	unsigned int gpio_irq;

	pgvic_device = irq_data_get_irq_chip_data(data);
	gpio_base = pgvic_device->gpio_base;
	gpio_irq = data->hwirq;

	amba_clrbitsl((gpio_base + GPIO_IE_OFFSET), (0x1 << gpio_irq));
	amba_writel((gpio_base + GPIO_IC_OFFSET), (0x1 << gpio_irq));
}

static void ambarella_gvic_unmask_irq(struct irq_data *data)
{
	struct ambarella_gvic_device_s *pgvic_device;
	void __iomem *gpio_base;
	unsigned int gpio_irq;

	pgvic_device = irq_data_get_irq_chip_data(data);
	gpio_base = pgvic_device->gpio_base;
	gpio_irq = data->hwirq;

	amba_setbitsl((gpio_base + GPIO_IE_OFFSET), (0x1 << gpio_irq));
}

static int ambarella_gvic_set_type_irq(struct irq_data *data,
	unsigned int type)
{
	struct ambarella_gvic_device_s *pgvic_device;
	void __iomem *gpio_base;
	unsigned int gpio_irq;
	struct irq_desc *desc;
	u32 mask;
	u32 bit;
	u32 sense;
	u32 bothedges;
	u32 event;

	pgvic_device = irq_data_get_irq_chip_data(data);
	gpio_base = pgvic_device->gpio_base;
	gpio_irq = data->hwirq;
	desc = irq_to_desc(data->irq);

	mask = ~(0x1 << gpio_irq);
	bit = (0x1 << gpio_irq);
	sense = amba_readl(gpio_base + GPIO_IS_OFFSET);
	bothedges = amba_readl(gpio_base + GPIO_IBE_OFFSET);
	event = amba_readl(gpio_base + GPIO_IEV_OFFSET);

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

	amba_writel((gpio_base + GPIO_IS_OFFSET), sense);
	amba_writel((gpio_base + GPIO_IBE_OFFSET), bothedges);
	amba_writel((gpio_base + GPIO_IEV_OFFSET), event);

	ambarella_gvic_ack_irq(data);

	return 0;
}

static int ambarella_gvic_set_wake_irq(struct irq_data *data, unsigned int on)
{
	struct ambarella_gvic_device_s *pgvic_device;
	unsigned int irq_wake_bit;

	pgvic_device = irq_data_get_irq_chip_data(data);
	irq_wake_bit = (1 << data->hwirq);
	if (on) {
		pgvic_device->irq_wake |= irq_wake_bit;
	} else {
		pgvic_device->irq_wake &= ~irq_wake_bit;
	}
	pr_debug("%s: irq[%d] on[%d], wake[0x%08X]\n",
		__func__, data->irq, on, pgvic_device->irq_wake);

	return 0;
}

static struct irq_chip ambarella_gvic_chip = {
	.name		= "GPIO",
	.irq_enable	= ambarella_gvic_enable_irq,
	.irq_disable	= ambarella_gvic_disable_irq,
	.irq_ack	= ambarella_gvic_ack_irq,
	.irq_mask	= ambarella_gvic_mask_irq,
	.irq_mask_ack	= ambarella_gvic_mask_ack_irq,
	.irq_unmask	= ambarella_gvic_unmask_irq,
	.irq_set_type	= ambarella_gvic_set_type_irq,
	.irq_set_wake	= ambarella_gvic_set_wake_irq,
	.flags		= (IRQCHIP_SET_TYPE_MASKED | IRQCHIP_MASK_ON_SUSPEND),
};

/* ==========================================================================*/
static int ambarella_gvic_irqdomain_map(struct irq_domain *d,
	unsigned int irq, irq_hw_number_t hwirq)
{
	struct ambarella_gvic_device_s *pgvic_device;

	pgvic_device = (struct ambarella_gvic_device_s *)d->host_data;
	irq_set_chip_and_handler(irq, &ambarella_gvic_chip, handle_level_irq);
	irq_set_chip_data(irq, pgvic_device);
	set_irq_flags(irq, (IRQF_VALID | IRQF_PROBE));

	return 0;
}

static void ambarella_gvic_handle_irq(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *gpio_chip;
	struct ambarella_gvic_device_s *pgvic_device;
	unsigned int gpio_mis;
	unsigned int gpio_hwirq;
	unsigned int gpio_irq;

	gpio_chip = irq_desc_get_chip(desc);
	chained_irq_enter(gpio_chip, desc);

	pgvic_device = irq_get_handler_data(irq);
	gpio_mis = amba_readl(pgvic_device->gpio_base + GPIO_MIS_OFFSET);
	if (gpio_mis) {
		gpio_hwirq = ffs(gpio_mis) - 1;
#if 0
		gpio_irq = irq_find_mapping(pgvic_device->domain, gpio_hwirq);
#else
		gpio_irq = (pgvic_device->irq_start + gpio_hwirq);
#endif
		generic_handle_irq(gpio_irq);
	}

	chained_irq_exit(gpio_chip, desc);
}

static struct irq_domain_ops ambarella_gvic_irqdomain_ops = {
	.map = ambarella_gvic_irqdomain_map,
};

void __init __ambarella_gvic_init(struct ambarella_gvic_device_s *pgvic_device,
	void __iomem *gpio_base, int irq_start,
	unsigned int irq_gpio, unsigned int irq_gpio_type)
{
	struct irq_domain *gvic_domain;

	irq_set_irq_type(irq_gpio, irq_gpio_type);
	pgvic_device->gpio_base = gpio_base;
	pgvic_device->irq_start = irq_start;
	gvic_domain = irq_domain_add_legacy(NULL, 32, irq_start, 0,
		&ambarella_gvic_irqdomain_ops, pgvic_device);
	irq_create_strict_mappings(gvic_domain, irq_start, 0, 32);
	irq_set_handler_data(irq_gpio, pgvic_device);
	irq_set_chained_handler(irq_gpio, ambarella_gvic_handle_irq);
	pgvic_device->domain = gvic_domain;
	pgvic_device->irq_start = irq_start;
	pgvic_device->irq_wake = 0;
}

void __init ambarella_gvic_init_irq(void __iomem *gpio_base,
	unsigned int irq_start,
	unsigned int irq_gpio,
	unsigned int irq_gpio_type)
{
	if (amba_gvic_info.device_num) {
		pr_err("%s: already inited!\n", __func__);
		return;
	}

	memset(&amba_gvic_info, 0, sizeof(amba_gvic_info));
	__ambarella_gvic_init(&amba_gvic_info.device_info[
		amba_gvic_info.device_num], gpio_base,
		irq_start, irq_gpio, irq_gpio_type);
	amba_gvic_info.device_num++;
}

void __init ambarella_gvic_add_irq(void __iomem *gpio_base,
	unsigned int irq_start,
	unsigned int irq_gpio,
	unsigned int irq_gpio_type)
{
	if (amba_gvic_info.device_num >= GPIO_INSTANCES) {
		pr_err("%s: No more space for new GVIC!\n", __func__);
		return;
	}

	__ambarella_gvic_init(&amba_gvic_info.device_info[
		amba_gvic_info.device_num], gpio_base,
		irq_start, irq_gpio, irq_gpio_type);
	amba_gvic_info.device_num++;
}

/* ==========================================================================*/
#if defined(CONFIG_PM)
static void ambarella_gvic_suspend_one(
	struct ambarella_gvic_device_s *pgvic_device,
	struct ambarella_gvic_pm_s *pgvic_pm)
{
	pgvic_pm->afsel_reg =
		amba_readl(pgvic_device->gpio_base + GPIO_AFSEL_OFFSET);
	pgvic_pm->dir_reg =
		amba_readl(pgvic_device->gpio_base + GPIO_DIR_OFFSET);
	pgvic_pm->is_reg =
		amba_readl(pgvic_device->gpio_base + GPIO_IS_OFFSET);
	pgvic_pm->ibe_reg =
		amba_readl(pgvic_device->gpio_base + GPIO_IBE_OFFSET);
	pgvic_pm->iev_reg =
		amba_readl(pgvic_device->gpio_base + GPIO_IEV_OFFSET);
	pgvic_pm->ie_reg =
		amba_readl(pgvic_device->gpio_base + GPIO_IE_OFFSET);
	pgvic_pm->mask_reg = ~pgvic_pm->afsel_reg;
	amba_writel((pgvic_device->gpio_base + GPIO_MASK_OFFSET),
		pgvic_pm->mask_reg);
	pgvic_pm->data_reg =
		amba_readl(pgvic_device->gpio_base + GPIO_DATA_OFFSET);

	pr_debug("%s: suspending GVIC[%p]\n",
		__func__, pgvic_device->gpio_base);
	pr_debug("afsel_reg[0x%08X]\n", pgvic_pm->afsel_reg);
	pr_debug("dir_reg[0x%08X]\n", pgvic_pm->dir_reg);
	pr_debug("is_reg[0x%08X]\n", pgvic_pm->is_reg);
	pr_debug("ibe_reg[0x%08X]\n", pgvic_pm->ibe_reg);
	pr_debug("iev_reg[0x%08X]\n", pgvic_pm->iev_reg);
	pr_debug("ie_reg[0x%08X]\n", pgvic_pm->ie_reg);
	pr_debug("mask_reg[0x%08X]\n", pgvic_pm->mask_reg);

	if (pgvic_device->irq_wake) {
		amba_writel((pgvic_device->gpio_base + GPIO_IE_OFFSET),
			pgvic_device->irq_wake);
		pr_info("GVIC[%p]: irq_wake[0x%08X]\n",
			pgvic_device->gpio_base, pgvic_device->irq_wake);
	}
}

static int ambarella_gvic_suspend(void)
{
	int i;

	for (i = 0; i < amba_gvic_info.device_num; i++) {
		ambarella_gvic_suspend_one(&amba_gvic_info.device_info[i],
			&amba_gvic_info.device_pm[i]);
	}

	return 0;
}

static void ambarella_gvic_resume_one(
	struct ambarella_gvic_device_s *pgvic_device,
	struct ambarella_gvic_pm_s *pgvic_pm)
{
	pr_debug("%s: resuming GVIC[%p]\n",
		__func__, pgvic_device->gpio_base);

	amba_writel((pgvic_device->gpio_base + GPIO_AFSEL_OFFSET),
		pgvic_pm->afsel_reg);
	amba_writel((pgvic_device->gpio_base + GPIO_DIR_OFFSET),
		pgvic_pm->dir_reg);
	amba_writel((pgvic_device->gpio_base + GPIO_MASK_OFFSET),
		pgvic_pm->mask_reg);
	amba_writel((pgvic_device->gpio_base + GPIO_DATA_OFFSET),
		pgvic_pm->data_reg);
	amba_writel((pgvic_device->gpio_base + GPIO_IS_OFFSET),
		pgvic_pm->is_reg);
	amba_writel((pgvic_device->gpio_base + GPIO_IBE_OFFSET),
		pgvic_pm->ibe_reg);
	amba_writel((pgvic_device->gpio_base + GPIO_IEV_OFFSET),
		pgvic_pm->iev_reg);
	amba_writel((pgvic_device->gpio_base + GPIO_IE_OFFSET),
		pgvic_pm->ie_reg);
}

static void ambarella_gvic_resume(void)
{
	int i;

	for (i = (amba_gvic_info.device_num - 1); i >= 0; i--) {
		ambarella_gvic_resume_one(&amba_gvic_info.device_info[i],
			&amba_gvic_info.device_pm[i]);
	}
}

struct syscore_ops ambarella_gvic_syscore_ops = {
	.suspend	= ambarella_gvic_suspend,
	.resume		= ambarella_gvic_resume,
};

static int __init ambarella_gvic_pm_init(void)
{
	if (amba_gvic_info.device_num > 0) {
		register_syscore_ops(&ambarella_gvic_syscore_ops);
	}

	return 0;
}
late_initcall(ambarella_gvic_pm_init);
#endif /* CONFIG_PM */

