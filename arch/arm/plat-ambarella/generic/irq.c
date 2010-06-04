/*
 * arch/arm/plat-ambarella/generic/irq.c
 *
 * History:
 *	2006/12/27 - [Charles Chiou] created file
 *	2007/11/29 - [Grady Chen] added VIC2, GPIO upper level VIC control
 *	2009/01/06 - [Anthony Ginger] Port to 2.6.28
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
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/device.h>

#include <asm/io.h>

#include <mach/hardware.h>

/* ==========================================================================*/
static unsigned long ambarella_gpio_irq_bit[BITS_TO_LONGS(NR_GPIO_IRQS)];
static unsigned long ambarella_gpio_wakeup_bit[BITS_TO_LONGS(NR_GPIO_IRQS)];

/* ==========================================================================*/
#define AMBARELLA_GPIO_IRQ2GIRQ()	do { \
	irq -= GPIO_INT_VEC_OFFSET; \
	} while (0)

#if (GPIO_INSTANCES == 2)
#define AMBARELLA_GPIO_IRQ2BASE()	do { \
	gpio_base = GPIO0_BASE; \
	if (irq >= NR_VIC_IRQ_SIZE) { \
		irq -= NR_VIC_IRQ_SIZE; \
		gpio_base = GPIO1_BASE; \
	} \
	} while (0)
#elif (GPIO_INSTANCES == 3)
#define AMBARELLA_GPIO_IRQ2BASE()	do { \
	gpio_base = GPIO0_BASE; \
	if (irq >= NR_VIC_IRQ_SIZE) { \
		irq -= NR_VIC_IRQ_SIZE; \
		gpio_base = GPIO1_BASE; \
	} \
	if (irq >= NR_VIC_IRQ_SIZE) { \
		irq -= NR_VIC_IRQ_SIZE; \
		gpio_base = GPIO2_BASE; \
	} \
	} while (0)
#elif (GPIO_INSTANCES == 4)
#define AMBARELLA_GPIO_IRQ2BASE()	do { \
	gpio_base = GPIO0_BASE; \
	if (irq >= NR_VIC_IRQ_SIZE) { \
		irq -= NR_VIC_IRQ_SIZE; \
		gpio_base = GPIO1_BASE; \
	} \
	if (irq >= NR_VIC_IRQ_SIZE) { \
		irq -= NR_VIC_IRQ_SIZE; \
		gpio_base = GPIO2_BASE; \
	} \
	if (irq >= NR_VIC_IRQ_SIZE) { \
		irq -= NR_VIC_IRQ_SIZE; \
		gpio_base = GPIO3_BASE; \
	} \
	} while (0)
#endif

void ambarella_gpio_ack_irq(unsigned int irq)
{
	u32					gpio_base;

	AMBARELLA_GPIO_IRQ2GIRQ();
	AMBARELLA_GPIO_IRQ2BASE();

	amba_writel(gpio_base + GPIO_IC_OFFSET, (0x1 << irq));
}

void inline __ambarella_gpio_disable_irq(unsigned int irq)
{
	u32					gpio_base;

	AMBARELLA_GPIO_IRQ2BASE();

	amba_clrbitsl(gpio_base + GPIO_IE_OFFSET, (0x1 << irq));
}

void ambarella_gpio_disable_irq(unsigned int irq)
{
	AMBARELLA_GPIO_IRQ2GIRQ();

	__ambarella_gpio_disable_irq(irq);
	__clear_bit(irq, ambarella_gpio_irq_bit);
}

void inline __ambarella_gpio_enable_irq(unsigned int irq)
{
	u32					gpio_base;

	AMBARELLA_GPIO_IRQ2BASE();

	amba_setbitsl(gpio_base + GPIO_IE_OFFSET, (0x1 << irq));
}

void ambarella_gpio_enable_irq(unsigned int irq)
{
	AMBARELLA_GPIO_IRQ2GIRQ();

	__ambarella_gpio_enable_irq(irq);
	__set_bit(irq, ambarella_gpio_irq_bit);
}

void ambarella_gpio_mask_ack_irq(unsigned int irq)
{
	u32					gpio_base;

	AMBARELLA_GPIO_IRQ2GIRQ();
	AMBARELLA_GPIO_IRQ2BASE();

	amba_clrbitsl(gpio_base + GPIO_IE_OFFSET, (0x1 << irq));
	amba_writel(gpio_base + GPIO_IC_OFFSET, (0x1 << irq));
}

int ambarella_gpio_irq_set_type(unsigned int irq, unsigned int type)
{
	u32					gpio_base;
	u32					mask;
	u32					bit;
	u32					sense;
	u32					bothedges;
	u32					event;

	pr_debug("%s: irq[%d] type[%d]\n", __func__, irq, type);

	AMBARELLA_GPIO_IRQ2GIRQ();
	AMBARELLA_GPIO_IRQ2BASE();

	mask = ~(0x1 << irq);
	bit = (0x1 << irq);
	sense = amba_readl(gpio_base + GPIO_IS_OFFSET);
	bothedges = amba_readl(gpio_base + GPIO_IBE_OFFSET);
	event = amba_readl(gpio_base + GPIO_IEV_OFFSET);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		sense &= mask;
		bothedges &= mask;
		event |= bit;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		sense &= mask;
		bothedges &= mask;
		event &= mask;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		sense &= mask;
		bothedges |= bit;
		event &= mask;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		sense |= bit;
		bothedges &= mask;
		event |= bit;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		sense |= bit;
		bothedges &= mask;
		event &= mask;
		break;
	default:
		pr_err("%s: can't set irq type %d for irq 0x%08x@%d\n",
			__func__, type, gpio_base, irq);
		return -EINVAL;
	}

	amba_writel(gpio_base + GPIO_IS_OFFSET, sense);
	amba_writel(gpio_base + GPIO_IBE_OFFSET, bothedges);
	amba_writel(gpio_base + GPIO_IEV_OFFSET, event);

	return 0;
}

static int ambarella_gpio_irq_set_wake(unsigned int irq, unsigned int state)
{
	pr_info("%s: set irq %d = %d\n", __func__, irq, state);

	AMBARELLA_GPIO_IRQ2GIRQ();

	if (state)
		__set_bit(irq, ambarella_gpio_wakeup_bit);
	else
		__clear_bit(irq, ambarella_gpio_wakeup_bit);

	return 0;
}

static struct irq_chip ambarella_gpio_irq_chip = {
	.name		= "ambarella gpio irq",
	.enable		= ambarella_gpio_enable_irq,
	.disable	= ambarella_gpio_disable_irq,
	.ack		= ambarella_gpio_ack_irq,
	.mask_ack	= ambarella_gpio_mask_ack_irq,
	.mask		= ambarella_gpio_disable_irq,
	.unmask		= ambarella_gpio_enable_irq,
	.set_type	= ambarella_gpio_irq_set_type,
	.set_wake	= ambarella_gpio_irq_set_wake,
};

/* ==========================================================================*/
#if (VIC_INSTANCES == 1)
#define AMBARELLA_VIC_IRQ2BASE()	do { } while (0)
#elif (VIC_INSTANCES == 2)
#define AMBARELLA_VIC_IRQ2BASE()	do { \
	if (irq >= VIC2_INT_VEC_OFFSET) { \
		irq -= VIC2_INT_VEC_OFFSET; \
		vic_base = VIC2_BASE; \
	} \
	} while (0)
#endif

static void ambarella_ack_irq(unsigned int irq)
{
	u32					vic_base = VIC_BASE;

	AMBARELLA_VIC_IRQ2BASE();

	amba_writel(vic_base + VIC_EDGE_CLR_OFFSET, 0x1 << irq);
}

static void ambarella_disable_irq(unsigned int irq)
{
	u32					vic_base = VIC_BASE;

	AMBARELLA_VIC_IRQ2BASE();

	amba_writel(vic_base + VIC_INTEN_CLR_OFFSET, 0x1 << irq);
}

static void ambarella_enable_irq(unsigned int irq)
{
	u32					vic_base = VIC_BASE;

	AMBARELLA_VIC_IRQ2BASE();

	amba_writel(vic_base + VIC_INTEN_OFFSET, 0x1 << irq);
}

static void ambarella_mask_ack_irq(unsigned int irq)
{
	u32					vic_base = VIC_BASE;

	AMBARELLA_VIC_IRQ2BASE();

	amba_writel(vic_base + VIC_INTEN_CLR_OFFSET, 0x1 << irq);
	amba_writel(vic_base + VIC_EDGE_CLR_OFFSET, 0x1 << irq);
}

static int ambarella_irq_set_type(unsigned int irq, unsigned int type)
{
	u32					vic_base = VIC_BASE;
	u32					mask;
	u32					bit;
	u32					sense;
	u32					bothedges;
	u32					event;

	pr_debug("%s: irq[%d] type[%d]\n", __func__, irq, type);

	AMBARELLA_VIC_IRQ2BASE();

	mask = ~(0x1 << irq);
	bit = (0x1 << irq);
	sense = amba_readl(vic_base + VIC_SENSE_OFFSET);
	bothedges = amba_readl(vic_base + VIC_BOTHEDGE_OFFSET);
	event = amba_readl(vic_base + VIC_EVENT_OFFSET);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		sense &= mask;
		bothedges &= mask;
		event |= bit;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		sense &= mask;
		bothedges &= mask;
		event &= mask;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		sense &= mask;
		bothedges |= bit;
		event &= mask;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		sense |= bit;
		bothedges &= mask;
		event |= bit;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		sense |= bit;
		bothedges &= mask;
		event &= mask;
		break;
	default:
		pr_err("%s: can't set irq type %d for irq 0x%08x@%d\n",
			__func__, type, vic_base, irq);
		return -EINVAL;
	}

	amba_writel(vic_base + VIC_SENSE_OFFSET, sense);
	amba_writel(vic_base + VIC_BOTHEDGE_OFFSET, bothedges);
	amba_writel(vic_base + VIC_EVENT_OFFSET, event);

	return 0;
}

static int ambarella_irq_set_wake(unsigned int irq, unsigned int state)
{
	pr_info("%s: set irq %d = %d\n", __func__, irq, state);

	return 0;
}

static struct irq_chip ambarella_irq_chip = {
	.name		= "ambarella onchip irq",
	.enable		= ambarella_enable_irq,
	.disable	= ambarella_disable_irq,
	.ack		= ambarella_ack_irq,
	.mask_ack	= ambarella_mask_ack_irq,
	.mask		= ambarella_disable_irq,
	.unmask		= ambarella_enable_irq,
	.set_type	= ambarella_irq_set_type,
	.set_wake	= ambarella_irq_set_wake,
};

/* ==========================================================================*/
static inline u32 ambarella_irq_stat2nr(u32 stat)
{
	u32					tmp;
	u32					nr;

	__asm__ __volatile__
		("rsbs	%[tmp], %[stat], #0" :
		[tmp] "=r" (tmp) : [stat] "r" (stat));
	__asm__ __volatile__
		("and	%[nr], %[tmp], %[stat]" :
		[nr] "=r" (nr) : [tmp] "r" (tmp), [stat] "r" (stat));
	__asm__ __volatile__
		("clzcc	%[nr], %[nr]" :
		[nr] "+r" (nr));
	__asm__ __volatile__
		("rsc	%[nr], %[nr], #32" :
		[nr] "+r" (nr));

	return nr;
}

static void ambarella_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	u32					nr;

	switch (irq) {
	case GPIO0_IRQ:
		nr = ambarella_irq_stat2nr(amba_readl(GPIO0_MIS_REG));
		if (nr < 32)
			generic_handle_irq(GPIO0_INT_VEC_OFFSET + nr);
		break;

	case GPIO1_IRQ:
		nr = ambarella_irq_stat2nr(amba_readl(GPIO1_MIS_REG));
		if (nr < 32) {
			generic_handle_irq(GPIO1_INT_VEC_OFFSET + nr);
			break;
		}

#if (GPIO_INSTANCES >= 3)
#if (GPIO2_IRQ != GPIO1_IRQ)
		break;
	case GPIO2_IRQ:
#endif
		nr = ambarella_irq_stat2nr(amba_readl(GPIO2_MIS_REG));
		if (nr < 32)
			generic_handle_irq(GPIO2_INT_VEC_OFFSET + nr);
#endif
		break;

#if (GPIO_INSTANCES >= 4)
	case GPIO3_IRQ:
		nr = ambarella_irq_stat2nr(amba_readl(GPIO3_MIS_REG));
		if (nr < 32)
			generic_handle_irq(GPIO3_INT_VEC_OFFSET + nr);
		break;
#endif

	default:
		break;
	}
}

/* ==========================================================================*/
void __init ambarella_init_irq(void)
{
	u32					i;

	/* Set VIC sense and event type for each entry */
	amba_writel(VIC_SENSE_REG, 0x00000000);
	amba_writel(VIC_BOTHEDGE_REG, 0x00000000);
	amba_writel(VIC_EVENT_REG, 0x00000000);

#if (VIC_INSTANCES >= 2)
	amba_writel(VIC2_SENSE_REG, 0x00000000);
	amba_writel(VIC2_BOTHEDGE_REG, 0x00000000);
	amba_writel(VIC2_EVENT_REG, 0x00000000);
#endif

	/* Disable all IRQ */
	amba_writel(VIC_INT_SEL_REG, 0x00000000);
	amba_writel(VIC_INTEN_REG, 0x00000000);
	amba_writel(VIC_INTEN_CLR_REG, 0xffffffff);
	amba_writel(VIC_EDGE_CLR_REG, 0xffffffff);

#if (VIC_INSTANCES >= 2)
	amba_writel(VIC2_INT_SEL_REG, 0x00000000);
	amba_writel(VIC2_INTEN_REG, 0x00000000);
	amba_writel(VIC2_INTEN_CLR_REG, 0xffffffff);
	amba_writel(VIC2_EDGE_CLR_REG, 0xffffffff);
#endif

	ambarella_irq_set_type(USBVBUS_IRQ, IRQ_TYPE_LEVEL_HIGH);

	/* Setup VIC IRQs */
	for (i = VIC_INT_VEC_OFFSET; i < NR_VIC_IRQS; i++) {
		set_irq_chip(i, &ambarella_irq_chip);

		if (i == VOUT_IRQ ||
		    i == VIN_IRQ ||
		    i == VDSP_IRQ ||
		    i == TIMER1_IRQ ||
		    i == TIMER2_IRQ ||
		    i == TIMER3_IRQ ||
		    i == WDT_IRQ ||
		    i == CFCD1_IRQ ||
		    i == XDCD_IRQ)
			set_irq_handler(i, handle_edge_irq);
#if (VIC_INSTANCES >= 2)
		else if (i == SDCD_IRQ ||
			 i == IDSP_ERROR_IRQ ||
			 i == VOUT_SYNC_MISSED_IRQ ||
			 i == CD2ND_BIT_CD_IRQ ||
			 i == AUDIO_ORC_IRQ ||
			 i == VOUT1_SYNC_MISSED_IRQ ||
			 i == IDSP_LAST_PIXEL_IRQ ||
			 i == IDSP_VSYNC_IRQ ||
			 i == IDSP_SENSOR_VSYNC_IRQ ||
			 i == VOUT_TV_SYNC_IRQ ||
			 i == VOUT_LCD_SYNC_IRQ)
			set_irq_handler(i, handle_edge_irq);
#endif
		else
			set_irq_handler(i, handle_level_irq);

		set_irq_flags(i, IRQF_VALID);
	}

	/* Setup GPIO IRQs */
	for (i = GPIO_INT_VEC_OFFSET; i < NR_IRQS; i++) {
		set_irq_chip(i, &ambarella_gpio_irq_chip);
		set_irq_handler(i, handle_level_irq);
		set_irq_flags(i, IRQF_VALID);
		__clear_bit((i - GPIO_INT_VEC_OFFSET),
			ambarella_gpio_wakeup_bit);
	}
	set_irq_chained_handler(GPIO0_IRQ, ambarella_gpio_irq_handler);
	set_irq_chained_handler(GPIO1_IRQ, ambarella_gpio_irq_handler);
#if (GPIO_INSTANCES >= 3)
#if (GPIO2_IRQ != GPIO1_IRQ)
	set_irq_chained_handler(GPIO2_IRQ, ambarella_gpio_irq_handler);
#endif
#endif
#if (GPIO_INSTANCES >= 4)
	set_irq_chained_handler(GPIO3_IRQ, ambarella_gpio_irq_handler);
#endif
}

/* ==========================================================================*/
struct ambarella_vic_pm_info {
	u32 vic_int_sel_reg;
	u32 vic_inten_reg;
	u32 vic_soften_reg;
	u32 vic_proten_reg;
	u32 vic_sense_reg;
	u32 vic_bothedge_reg;
	u32 vic_event_reg;
};

struct ambarella_vic_pm_info ambarella_vic_pm;
#if (VIC_INSTANCES >= 2)
struct ambarella_vic_pm_info ambarella_vic2_pm;
#endif

u32 ambarella_irq_suspend(u32 level)
{
	u32					i;

	ambarella_vic_pm.vic_int_sel_reg = amba_readl(VIC_INT_SEL_REG);
	ambarella_vic_pm.vic_inten_reg = amba_readl(VIC_INTEN_REG);
	ambarella_vic_pm.vic_soften_reg = amba_readl(VIC_SOFTEN_REG);
	ambarella_vic_pm.vic_proten_reg = amba_readl(VIC_PROTEN_REG);
	ambarella_vic_pm.vic_sense_reg = amba_readl(VIC_SENSE_REG);
	ambarella_vic_pm.vic_bothedge_reg = amba_readl(VIC_BOTHEDGE_REG);
	ambarella_vic_pm.vic_event_reg = amba_readl(VIC_EVENT_REG);
#if (VIC_INSTANCES >= 2)
	ambarella_vic2_pm.vic_int_sel_reg = amba_readl(VIC2_INT_SEL_REG);
	ambarella_vic2_pm.vic_inten_reg = amba_readl(VIC2_INTEN_REG);
	ambarella_vic2_pm.vic_soften_reg = amba_readl(VIC2_SOFTEN_REG);
	ambarella_vic2_pm.vic_proten_reg = amba_readl(VIC2_PROTEN_REG);
	ambarella_vic2_pm.vic_sense_reg = amba_readl(VIC2_SENSE_REG);
	ambarella_vic2_pm.vic_bothedge_reg = amba_readl(VIC2_BOTHEDGE_REG);
	ambarella_vic2_pm.vic_event_reg = amba_readl(VIC2_EVENT_REG);
#endif

	for (i = 0; i < NR_GPIO_IRQS; i++) {
		if (test_bit(i, ambarella_gpio_wakeup_bit)) {
			__ambarella_gpio_enable_irq(i);
		} else {
			__ambarella_gpio_disable_irq(i);
		}
	}
	pr_debug("%s: GPIO0_IE_REG = 0x%08X\n",
		__func__, amba_readl(GPIO0_IE_REG));
	pr_debug("%s: GPIO1_IE_REG = 0x%08X\n",
		__func__, amba_readl(GPIO1_IE_REG));
#if (GPIO_INSTANCES >= 3)
	pr_debug("%s: GPIO2_IE_REG = 0x%08X\n",
		__func__, amba_readl(GPIO2_IE_REG));
#endif
#if (GPIO_INSTANCES >= 4)
	pr_debug("%s: GPIO3_IE_REG = 0x%08X\n",
		__func__, amba_readl(GPIO3_IE_REG));
#endif

	return 0;
}

u32 ambarella_irq_resume(u32 level)
{
	u32					i;

	amba_writel(VIC_INT_SEL_REG, ambarella_vic_pm.vic_int_sel_reg);
	amba_writel(VIC_INTEN_CLR_REG, 0xffffffff);
	amba_writel(VIC_INTEN_REG, ambarella_vic_pm.vic_inten_reg);
	amba_writel(VIC_SOFTEN_CLR_REG, 0xffffffff);
	amba_writel(VIC_SOFTEN_REG, ambarella_vic_pm.vic_soften_reg);
	amba_writel(VIC_PROTEN_REG, ambarella_vic_pm.vic_proten_reg);
	amba_writel(VIC_SENSE_REG, ambarella_vic_pm.vic_sense_reg);
	amba_writel(VIC_BOTHEDGE_REG, ambarella_vic_pm.vic_bothedge_reg);
	amba_writel(VIC_EVENT_REG, ambarella_vic_pm.vic_event_reg);
#if (VIC_INSTANCES >= 2)
	amba_writel(VIC2_INT_SEL_REG, ambarella_vic2_pm.vic_int_sel_reg);
	amba_writel(VIC2_INTEN_CLR_REG, 0xffffffff);
	amba_writel(VIC2_INTEN_REG, ambarella_vic2_pm.vic_inten_reg);
	amba_writel(VIC2_SOFTEN_CLR_REG, 0xffffffff);
	amba_writel(VIC2_SOFTEN_REG, ambarella_vic2_pm.vic_soften_reg);
	amba_writel(VIC2_PROTEN_REG, ambarella_vic2_pm.vic_proten_reg);
	amba_writel(VIC2_SENSE_REG, ambarella_vic2_pm.vic_sense_reg);
	amba_writel(VIC2_BOTHEDGE_REG, ambarella_vic2_pm.vic_bothedge_reg);
	amba_writel(VIC2_EVENT_REG, ambarella_vic2_pm.vic_event_reg);
#endif

	for (i = 0; i < NR_GPIO_IRQS; i++) {
		if (test_bit(i, ambarella_gpio_irq_bit)) {
			__ambarella_gpio_enable_irq(i);
		} else {
			__ambarella_gpio_disable_irq(i);
		}
	}
	pr_debug("%s: GPIO0_IE_REG = 0x%08X\n",
		__func__, amba_readl(GPIO0_IE_REG));
	pr_debug("%s: GPIO1_IE_REG = 0x%08X\n",
		__func__, amba_readl(GPIO1_IE_REG));
#if (GPIO_INSTANCES >= 3)
	pr_debug("%s: GPIO2_IE_REG = 0x%08X\n",
		__func__, amba_readl(GPIO2_IE_REG));
#endif
#if (GPIO_INSTANCES >= 4)
	pr_debug("%s: GPIO3_IE_REG = 0x%08X\n",
		__func__, amba_readl(GPIO3_IE_REG));
#endif

	return 0;
}

