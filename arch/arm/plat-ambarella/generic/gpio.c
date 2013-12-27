/*
 * arch/arm/plat-ambarella/generic/gpio.c
 *
 * History:
 *	2007/11/21 - [Grady Chen] created file
 *	2008/01/08 - [Anthony Ginger] Add GENERIC_GPIO support.
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
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <mach/hardware.h>
#include <plat/gpio.h>

/* ==========================================================================*/
struct ambarella_gpio_chip {
	u32			base_reg;
	spinlock_t		lock;
};

static inline int ambarella_gpio_inline_config(
	struct ambarella_gpio_chip *agchip, u32 offset, int func)
{
	int					retval = 0;
	unsigned long				flags;

	spin_lock_irqsave(&agchip->lock, flags);
	if (func == GPIO_FUNC_HW) {
		amba_setbitsl(agchip->base_reg + GPIO_AFSEL_OFFSET,
			(0x1 << offset));
		amba_clrbitsl(agchip->base_reg + GPIO_MASK_OFFSET,
			(0x1 << offset));
	} else if (func == GPIO_FUNC_SW_INPUT) {
		amba_clrbitsl(agchip->base_reg + GPIO_AFSEL_OFFSET,
			(0x1 << offset));
		amba_clrbitsl(agchip->base_reg + GPIO_DIR_OFFSET,
			(0x1 << offset));
	} else if (func == GPIO_FUNC_SW_OUTPUT) {
		amba_clrbitsl(agchip->base_reg + GPIO_AFSEL_OFFSET,
			(0x1 << offset));
		amba_setbitsl(agchip->base_reg + GPIO_DIR_OFFSET,
			(0x1 << offset));
	} else {
		pr_err("%s: invalid GPIO func %d for 0x%x:0x%x.\n",
			__func__, func, agchip->base_reg, offset);
		retval = -EINVAL;
	}
	spin_unlock_irqrestore(&agchip->lock, flags);

	return retval;
}

static inline void ambarella_gpio_inline_set(
	struct ambarella_gpio_chip *agchip, u32 offset, int value)
{
	unsigned long				flags;
	u32					mask;

	mask = (0x1 << offset);
	spin_lock_irqsave(&agchip->lock, flags);
	amba_writel(agchip->base_reg + GPIO_MASK_OFFSET, mask);
	if (value == GPIO_LOW) {
		amba_writel(agchip->base_reg + GPIO_DATA_OFFSET, 0);
	} else {
		amba_writel(agchip->base_reg + GPIO_DATA_OFFSET, mask);
	}
	spin_unlock_irqrestore(&agchip->lock, flags);
}

static inline int ambarella_gpio_inline_get(
	struct ambarella_gpio_chip *agchip, u32 offset)
{
	u32					val = 0;
	unsigned long				flags;

	spin_lock_irqsave(&agchip->lock, flags);
	amba_writel(agchip->base_reg + GPIO_MASK_OFFSET, (0x1 << offset));
	val = amba_readl(agchip->base_reg + GPIO_DATA_OFFSET);
	spin_unlock_irqrestore(&agchip->lock, flags);

	val = (val >> offset) & 0x1;
	return (val ? GPIO_HIGH : GPIO_LOW);
}

/* ==========================================================================*/
static DEFINE_MUTEX(ambarella_gpio_mtx);
static unsigned long ambarella_gpio_valid[BITS_TO_LONGS(AMBGPIO_SIZE)];
static unsigned long ambarella_gpio_freeflag[BITS_TO_LONGS(AMBGPIO_SIZE)];

static u32 gpio_base_reg[] = {
	GPIO0_BASE,
	GPIO1_BASE,
	GPIO2_BASE,
	GPIO3_BASE,
	GPIO4_BASE,
	GPIO5_BASE,
};

static struct ambarella_gpio_chip ambarella_gpio_banks[GPIO_INSTANCES];

/* ==========================================================================*/
static struct ambarella_gpio_chip *ambarella_gpio_id_to_chip(int id)
{
	return (id < 0) ? NULL : &ambarella_gpio_banks[id / GPIO_BANK_SIZE];
}

void ambarella_gpio_config(int id, int func)
{
	struct ambarella_gpio_chip		*chip;
	u32					offset;

	chip = ambarella_gpio_id_to_chip(id);
	if (chip == NULL) {
		pr_err("%s: invalid GPIO %d for func %d.\n",
			__func__, id, func);
		return;
	}
	offset = id & 0x1f;

	ambarella_gpio_inline_config(chip, offset, func);
}
EXPORT_SYMBOL(ambarella_gpio_config);

void ambarella_gpio_set(int id, int value)
{
	struct ambarella_gpio_chip		*chip;
	u32					offset;

	chip = ambarella_gpio_id_to_chip(id);
	if (chip == NULL) {
		pr_err("%s: invalid GPIO %d.\n", __func__, id);
		return;
	}
	offset = id & 0x1f;

	ambarella_gpio_inline_set(chip, offset, value);
}
EXPORT_SYMBOL(ambarella_gpio_set);

int ambarella_gpio_get(int id)
{
	struct ambarella_gpio_chip		*chip;
	u32					offset;

	chip = ambarella_gpio_id_to_chip(id);
	if (chip == NULL) {
		pr_err("%s: invalid GPIO %d.\n", __func__, id);
		return 0;
	}
	offset = id & 0x1f;

	return ambarella_gpio_inline_get(chip, offset);
}
EXPORT_SYMBOL(ambarella_gpio_get);

/* ==========================================================================*/
void ambarella_gpio_raw_lock(u32 id, unsigned long *pflags)
{
	spin_lock_irqsave(&ambarella_gpio_banks[id].lock, *pflags);
}

void ambarella_gpio_raw_unlock(u32 id, unsigned long *pflags)
{
	spin_unlock_irqrestore(&ambarella_gpio_banks[id].lock, *pflags);
}

/* ==========================================================================*/
void __init ambarella_gpio_set_valid(unsigned pin, int valid)
{
	if (likely(pin >=0 && pin < AMBGPIO_SIZE)) {
		if (valid)
			__set_bit(pin, ambarella_gpio_valid);
		else
			__clear_bit(pin, ambarella_gpio_valid);
	}
}

int __init ambarella_init_gpio(void)
{
	int					retval = 0;
	int					i;

	mutex_lock(&ambarella_gpio_mtx);
	memset(ambarella_gpio_valid, 0xff, sizeof(ambarella_gpio_valid));
	memset(ambarella_gpio_freeflag, 0xff, sizeof(ambarella_gpio_freeflag));
	for (i = GPIO_MAX_LINES; i < AMBGPIO_SIZE; i++) {
		ambarella_gpio_set_valid(i, 0);
		__clear_bit(i, ambarella_gpio_freeflag);
	}
	mutex_unlock(&ambarella_gpio_mtx);

	for (i = 0; i < GPIO_INSTANCES; i++) {
		spin_lock_init(&ambarella_gpio_banks[i].lock);
		ambarella_gpio_banks[i].base_reg = gpio_base_reg[i];
	}

	return retval;
}

/* ==========================================================================*/
int ambarella_set_gpio_output_can_sleep(
	struct ambarella_gpio_io_info *pinfo, u32 on, int can_sleep)
{
	int					retval = 0;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		retval = -1;
		goto ambarella_set_gpio_output_can_sleep_exit;
	}
	if (pinfo->gpio_id < 0 ) {
		pr_debug("%s: wrong gpio id %d.\n", __func__, pinfo->gpio_id);
		retval = -1;
		goto ambarella_set_gpio_output_can_sleep_exit;
	}

	pr_debug("%s: Gpio[%d] %s, level[%s], delay[%dms].\n", __func__,
		pinfo->gpio_id, on ? "ON" : "OFF",
		pinfo->active_level ? "HIGH" : "LOW",
		pinfo->active_delay);
	if (pinfo->gpio_id >= EXT_GPIO(0)) {
		pr_debug("%s: try expander gpio id %d.\n",
			__func__, pinfo->gpio_id);
		if (on) {
			gpio_direction_output(pinfo->gpio_id,
				pinfo->active_level);
		} else {
			gpio_direction_output(pinfo->gpio_id,
				!pinfo->active_level);
		}
	} else {
		ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_SW_OUTPUT);
		if (on) {
			ambarella_gpio_set(pinfo->gpio_id,
				pinfo->active_level);
		} else {
			ambarella_gpio_set(pinfo->gpio_id,
				!pinfo->active_level);
		}
	}
	if (can_sleep) {
		msleep(pinfo->active_delay);
	} else {
		mdelay(pinfo->active_delay);
	}

ambarella_set_gpio_output_can_sleep_exit:
	return retval;
}
EXPORT_SYMBOL(ambarella_set_gpio_output_can_sleep);

u32 ambarella_get_gpio_input_can_sleep(
	struct ambarella_gpio_io_info *pinfo, int can_sleep)
{
	u32					gpio_value = 0;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		goto ambarella_get_gpio_input_can_sleep_exit;
	}
	if (pinfo->gpio_id < 0 ) {
		pr_debug("%s: wrong gpio id %d.\n", __func__, pinfo->gpio_id);
		goto ambarella_get_gpio_input_can_sleep_exit;
	}

	if (pinfo->gpio_id >= EXT_GPIO(0)) {
		pr_debug("%s: try expander gpio id %d.\n",
			__func__, pinfo->gpio_id);
		gpio_direction_input(pinfo->gpio_id);
		if (can_sleep) {
			msleep(pinfo->active_delay);
		} else {
			mdelay(pinfo->active_delay);
		}
		gpio_value = gpio_get_value(pinfo->gpio_id);
	} else {
		ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_SW_INPUT);
		if (can_sleep) {
			msleep(pinfo->active_delay);
		} else {
			mdelay(pinfo->active_delay);
		}
		gpio_value = ambarella_gpio_get(pinfo->gpio_id);
	}

	pr_debug("%s: {gpio[%d], level[%s], delay[%dms]} get[%d].\n",
		__func__, pinfo->gpio_id,
		pinfo->active_level ? "HIGH" : "LOW",
		pinfo->active_delay, gpio_value);

ambarella_get_gpio_input_can_sleep_exit:
	return (gpio_value == pinfo->active_level) ? 1 : 0;
}
EXPORT_SYMBOL(ambarella_get_gpio_input_can_sleep);

int ambarella_set_gpio_reset_can_sleep(
	struct ambarella_gpio_io_info *pinfo, int can_sleep)
{
	int					retval = 0;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		retval = -1;
		goto ambarella_set_gpio_reset_can_sleep_exit;
	}
	if (pinfo->gpio_id < 0 ) {
		pr_debug("%s: wrong gpio id %d.\n", __func__, pinfo->gpio_id);
		retval = -1;
		goto ambarella_set_gpio_reset_can_sleep_exit;
	}

	pr_debug("%s: Reset gpio[%d], level[%s], delay[%dms].\n",
		__func__, pinfo->gpio_id,
		pinfo->active_level ? "HIGH" : "LOW",
		pinfo->active_delay);
	if (pinfo->gpio_id >= EXT_GPIO(0)) {
		pr_debug("%s: try expander gpio id %d.\n",
			__func__, pinfo->gpio_id);
		gpio_direction_output(pinfo->gpio_id, pinfo->active_level);
		if (can_sleep) {
			msleep(pinfo->active_delay);
		} else {
			mdelay(pinfo->active_delay);
		}
		gpio_direction_output(pinfo->gpio_id, !pinfo->active_level);
		if (can_sleep) {
			msleep(pinfo->active_delay);
		} else {
			mdelay(pinfo->active_delay);
		}
	} else {
		ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_SW_OUTPUT);
		ambarella_gpio_set(pinfo->gpio_id, pinfo->active_level);
		if (can_sleep) {
			msleep(pinfo->active_delay);
		} else {
			mdelay(pinfo->active_delay);
		}
		ambarella_gpio_set(pinfo->gpio_id, !pinfo->active_level);
		if (can_sleep) {
			msleep(pinfo->active_delay);
		} else {
			mdelay(pinfo->active_delay);
		}
	}

ambarella_set_gpio_reset_can_sleep_exit:
	return retval;
}
EXPORT_SYMBOL(ambarella_set_gpio_reset_can_sleep);

/* ==========================================================================*/
int ambarella_set_gpio_output(
	struct ambarella_gpio_io_info *pinfo, u32 on)
{
	return ambarella_set_gpio_output_can_sleep(pinfo, on, 0);
}
EXPORT_SYMBOL(ambarella_set_gpio_output);

u32 ambarella_get_gpio_input(
	struct ambarella_gpio_io_info *pinfo)
{
	return ambarella_get_gpio_input_can_sleep(pinfo, 0);
}
EXPORT_SYMBOL(ambarella_get_gpio_input);

int ambarella_set_gpio_reset(
	struct ambarella_gpio_io_info *pinfo)
{
	return ambarella_set_gpio_reset_can_sleep(pinfo, 0);
}
EXPORT_SYMBOL(ambarella_set_gpio_reset);

int ambarella_is_valid_gpio_irq(struct ambarella_gpio_irq_info *pinfo)
{
	int					bvalid = 0;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		goto ambarella_is_valid_gpio_irq_exit;
	}

	if ((pinfo->irq_gpio < 0 ) || (pinfo->irq_gpio >= ARCH_NR_GPIOS))
		goto ambarella_is_valid_gpio_irq_exit;

	if ((pinfo->irq_type != IRQ_TYPE_EDGE_RISING) &&
		(pinfo->irq_type != IRQ_TYPE_EDGE_FALLING) &&
		(pinfo->irq_type != IRQ_TYPE_EDGE_BOTH) &&
		(pinfo->irq_type != IRQ_TYPE_LEVEL_HIGH) &&
		(pinfo->irq_type != IRQ_TYPE_LEVEL_LOW))
		goto ambarella_is_valid_gpio_irq_exit;

	if ((pinfo->irq_gpio_val != GPIO_HIGH) &&
		(pinfo->irq_gpio_val != GPIO_LOW))
		goto ambarella_is_valid_gpio_irq_exit;

	if ((pinfo->irq_gpio_mode != GPIO_FUNC_SW_INPUT) &&
		(pinfo->irq_gpio_mode != GPIO_FUNC_SW_OUTPUT) &&
		(pinfo->irq_gpio_mode != GPIO_FUNC_HW))
		goto ambarella_is_valid_gpio_irq_exit;

	if ((pinfo->irq_gpio_mode != GPIO_FUNC_HW) &&
		((gpio_to_irq(pinfo->irq_gpio) < GPIO_INT_VEC(0)) ||
		(gpio_to_irq(pinfo->irq_gpio) >= NR_IRQS)))
		goto ambarella_is_valid_gpio_irq_exit;

	bvalid = 1;

ambarella_is_valid_gpio_irq_exit:
	return bvalid;
}
EXPORT_SYMBOL(ambarella_is_valid_gpio_irq);

