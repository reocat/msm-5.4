/*
 * arch/arm/mach-ambarella/gpio.c
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

#include <linux/proc_fs.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <mach/hardware.h>

/* ==========================================================================*/
static inline int ambarella_gpio_inline_config(u32 base, u32 offset, int func)
{
	int					errorCode = 0;

	if (func == GPIO_FUNC_HW) {
		amba_setbits(base + GPIO_AFSEL_OFFSET, (0x1 << offset));
		amba_clrbits(base + GPIO_MASK_OFFSET, (0x1 << offset));
	} else if (func == GPIO_FUNC_SW_INPUT) {
		amba_clrbits(base + GPIO_AFSEL_OFFSET, (0x1 << offset));
		amba_setbits(base + GPIO_MASK_OFFSET, (0x1 << offset));
		amba_clrbits(base + GPIO_DIR_OFFSET, (0x1 << offset));
	} else if (func == GPIO_FUNC_SW_OUTPUT) {
		amba_clrbits(base + GPIO_AFSEL_OFFSET, (0x1 << offset));
		amba_setbits(base + GPIO_MASK_OFFSET, (0x1 << offset));
		amba_setbits(base + GPIO_DIR_OFFSET, (0x1 << offset));
	} else {
		pr_err("%s: invalid GPIO func %d for 0x%x:0x%x.\n",
			__func__, func, base, offset);
		errorCode = -EINVAL;
	}

	return errorCode;
}

static inline void ambarella_gpio_inline_set(u32 base, u32 offset, int value)
{
	if (value == GPIO_LOW) {
		amba_clrbits(base + GPIO_DATA_OFFSET, (0x1 << offset));
	} else {
		amba_setbits(base + GPIO_DATA_OFFSET, (0x1 << offset));
	}
}

static inline int ambarella_gpio_inline_get(u32 base, u32 offset)
{
	u32					val = 0;

	val = amba_readl(base + GPIO_DATA_OFFSET);
	val = (val >> offset) & 0x1;

	return (val ? GPIO_HIGH: GPIO_LOW);
}

/* ==========================================================================*/
void ambarella_gpio_config(int id, int func)
{
	u32					base = 0;
	u32					offset;

	if (id < (1 * GPIO_BANK_SIZE)) {
		base = GPIO0_BASE;
	} else if (id < (2 * GPIO_BANK_SIZE)) {
		base = GPIO1_BASE;
#if (GPIO_INSTANCES >= 3)
	} else if (id < (3 * GPIO_BANK_SIZE)) {
		base = GPIO2_BASE;
#endif
#if (GPIO_INSTANCES >= 4)
	} else if (id < (4 * GPIO_BANK_SIZE)) {
		base = GPIO3_BASE;
#endif
	}
	if (base == 0) {
		pr_err("%s: invalid GPIO %d for func %d.\n",
			__func__, id, func);
		return;
	}
	offset = id & 0x1f;

	ambarella_gpio_inline_config(base, offset, func);
}
EXPORT_SYMBOL(ambarella_gpio_config);

void ambarella_gpio_set(int id, int value)
{
	u32					base = 0;
	u32					offset;

	if (id < (1 * GPIO_BANK_SIZE)) {
		base = GPIO0_BASE;
	} else if (id < (2 * GPIO_BANK_SIZE)) {
		base = GPIO1_BASE;
#if (GPIO_INSTANCES >= 3)
	} else if (id < (3 * GPIO_BANK_SIZE)) {
		base = GPIO2_BASE;
#endif
#if (GPIO_INSTANCES >= 4)
	} else if (id < (4 * GPIO_BANK_SIZE)) {
		base = GPIO3_BASE;
#endif
	}
	if (base == 0) {
		pr_err("%s: invalid GPIO %d for value %d.\n",
			__func__, id, value);
		return;
	}
	offset = id & 0x1f;

	ambarella_gpio_inline_set(base, offset, value);
}
EXPORT_SYMBOL(ambarella_gpio_set);

int ambarella_gpio_get(int id)
{
	u32					base = 0;
	u32					offset;

	if (id < (1 * GPIO_BANK_SIZE)) {
		base = GPIO0_BASE;
	} else if (id < (2 * GPIO_BANK_SIZE)) {
		base = GPIO1_BASE;
#if (GPIO_INSTANCES >= 3)
	} else if (id < (3 * GPIO_BANK_SIZE)) {
		base = GPIO2_BASE;
#endif
#if (GPIO_INSTANCES >= 4)
	} else if (id < (4 * GPIO_BANK_SIZE)) {
		base = GPIO3_BASE;
#endif
	}
	if (base == 0) {
		pr_err("%s: invalid GPIO %d.\n",
			__func__, id);
		return 0;
	}
	offset = id & 0x1f;

	return ambarella_gpio_inline_get(base, offset);
}
EXPORT_SYMBOL(ambarella_gpio_get);

/* ==========================================================================*/
struct ambarella_gpio_chip {
	struct gpio_chip	chip;
	u32			mem_base;
};
#define to_ambarella_gpio_chip(c) container_of(c, struct ambarella_gpio_chip, chip)

static DEFINE_SPINLOCK(ambarella_gpio_lock);
static unsigned long ambarella_gpio_valid[BITS_TO_LONGS(ARCH_NR_GPIOS)];

static int ambarella_gpio_chip_direction_input(struct gpio_chip *chip,
	unsigned offset)
{
	int					errorCode = 0;
	unsigned long				flags;
	struct ambarella_gpio_chip		*ambarella_chip;

	spin_lock_irqsave(&ambarella_gpio_lock, flags);

	ambarella_chip = to_ambarella_gpio_chip(chip);

	errorCode = ambarella_gpio_inline_config(
		ambarella_chip->mem_base,
		offset,
		GPIO_FUNC_SW_INPUT);

	spin_unlock_irqrestore(&ambarella_gpio_lock, flags);

	return errorCode;
}

static int ambarella_gpio_chip_direction_output(struct gpio_chip *chip,
	unsigned offset, int val)
{
	int					errorCode = 0;
	unsigned long				flags;
	struct ambarella_gpio_chip		*ambarella_chip;

	spin_lock_irqsave(&ambarella_gpio_lock, flags);

	ambarella_chip = to_ambarella_gpio_chip(chip);

	errorCode = ambarella_gpio_inline_config(
		ambarella_chip->mem_base,
		offset,
		GPIO_FUNC_SW_OUTPUT);
	ambarella_gpio_inline_set(ambarella_chip->mem_base, offset, val);	

	spin_unlock_irqrestore(&ambarella_gpio_lock, flags);

	return errorCode;
}

static int ambarella_gpio_chip_get(struct gpio_chip *chip,
	unsigned offset)
{
	struct ambarella_gpio_chip		*ambarella_chip;

	ambarella_chip = to_ambarella_gpio_chip(chip);

	return ambarella_gpio_inline_get(ambarella_chip->mem_base, offset);
}

static void ambarella_gpio_chip_set(struct gpio_chip *chip,
	unsigned offset, int val)
{
	struct ambarella_gpio_chip		*ambarella_chip;

	ambarella_chip = to_ambarella_gpio_chip(chip);

	return ambarella_gpio_inline_set(ambarella_chip->mem_base, offset, val);
}

static void ambarella_gpio_chip_dbg_show(struct seq_file *s,
	struct gpio_chip *chip)
{
	int					i;
	struct ambarella_gpio_chip		*ambarella_chip;
	u32					afsel;
	u32					mask;
	u32					data;
	u32					dir;

	ambarella_chip = to_ambarella_gpio_chip(chip);

	afsel = amba_readl(ambarella_chip->mem_base + GPIO_AFSEL_OFFSET);
	mask = amba_readl(ambarella_chip->mem_base + GPIO_MASK_OFFSET);
	data = amba_readl(ambarella_chip->mem_base + GPIO_DATA_OFFSET);
	dir = amba_readl(ambarella_chip->mem_base + GPIO_DIR_OFFSET);

	for (i = 0; i < chip->ngpio; i++) {
		if ((afsel & (1 << i)) && !(mask & (1 << i)))
			seq_printf(s, "GPIO %s%d: HW\n", chip->label, i);
		else if (!(afsel & (1 << i)) && (mask & (1 << i)))
			seq_printf(s, "GPIO %s%d:\t%s\t%s\n", chip->label, i,
				   (data & (1 << i)) ? "set" : "clear",
				   (dir & (1 << i)) ? "out" : "in");
		else
			seq_printf(s, "GPIO %s%d: unknown!\n", chip->label, i);
	}
}

#define AMBARELLA_GPIO_BANK(name, reg_base, base_gpio)			\
{									\
	.chip = {							\
		.label			= name,				\
		.direction_input	= ambarella_gpio_chip_direction_input, \
		.direction_output	= ambarella_gpio_chip_direction_output, \
		.get			= ambarella_gpio_chip_get,	\
		.set			= ambarella_gpio_chip_set,	\
		.dbg_show		= ambarella_gpio_chip_dbg_show,	\
		.base			= base_gpio,			\
		.ngpio			= GPIO_BANK_SIZE,		\
	},								\
	.mem_base			= reg_base,			\
}

static struct ambarella_gpio_chip ambarella_gpio_banks[] = {
	AMBARELLA_GPIO_BANK("ambarella-gpio0", GPIO0_BASE, GPIO(0 * GPIO_BANK_SIZE)),
	AMBARELLA_GPIO_BANK("ambarella-gpio1", GPIO1_BASE, GPIO(1 * GPIO_BANK_SIZE)),
#if (GPIO_INSTANCES >= 3)
	AMBARELLA_GPIO_BANK("ambarella-gpio2", GPIO2_BASE, GPIO(2 * GPIO_BANK_SIZE)),
#endif
#if (GPIO_INSTANCES >= 4)
	AMBARELLA_GPIO_BANK("ambarella-gpio3", GPIO3_BASE, GPIO(3 * GPIO_BANK_SIZE)),
#endif
};

/* ==========================================================================*/
#ifdef CONFIG_AMBARELLA_GPIO_PROC
static unsigned char gpio_array[ARCH_NR_GPIOS * 3];
static const char gpio_proc_name[] = "gpio";
static struct proc_dir_entry *gpio_file;

static int ambarella_gpio_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	unsigned short				i;

	if (count > GPIO_MAX_LINES) {
		count = GPIO_MAX_LINES;
	}

	if (off + count > GPIO_MAX_LINES) {
		*eof = 1;
		return 0;
	}

	*start = page + off;
	for (i = off; i < (off + count); i++) {
		if (ambarella_gpio_get(i)) {
			page[i] = '1';
		} else {
			page[i] = '0';
		}
	}

	return count;
}

static int ambarella_gpio_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					errorCode = 0;
	int					cmd_cnt;
	int					i;
	int					gpio_id;
	int					func;

	if (count > sizeof(gpio_array)) {
		pr_err("%s: count %d out of size!\n", __func__, (u32)count);
		errorCode = -ENOSPC;
		goto ambarella_gpio_write_exit;
	}

	if (copy_from_user(gpio_array, buffer, count)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto ambarella_gpio_write_exit;
	}

	cmd_cnt = count / 3;
	for (i = 0; i < cmd_cnt; i++) {
		gpio_id = gpio_array[i * 3 + 1];
		func = gpio_array[i * 3 + 2];
		if ((gpio_array[i * 3] == 'C') || (gpio_array[i * 3] == 'c')) {
			ambarella_gpio_config(gpio_id, func);
			continue;
		}
		if ((gpio_array[i * 3] == 'W') || (gpio_array[i * 3] == 'w')) {
			ambarella_gpio_set(gpio_id, func);
			continue;
		}
	}

	errorCode = count;

ambarella_gpio_write_exit:
	return errorCode;
}
#endif

/* ==========================================================================*/
void __init ambarella_gpio_set_valid(unsigned pin, int valid)
{
	if (likely(pin >=0 && pin < ARCH_NR_GPIOS)) {
		if (valid)
			__set_bit(pin, ambarella_gpio_valid);
		else
			__clear_bit(pin, ambarella_gpio_valid);
	}
}

int __init ambarella_init_gpio(void)
{
	int					errorCode = 0;
	int					i;

	set_irq_type(GPIO0_IRQ, IRQ_TYPE_LEVEL_HIGH);
	set_irq_type(GPIO1_IRQ, IRQ_TYPE_LEVEL_HIGH);
#if (GPIO_INSTANCES >= 3)
#if (GPIO2_IRQ != GPIO1_IRQ)
	set_irq_type(GPIO2_IRQ, IRQ_TYPE_LEVEL_HIGH);
#endif
#endif
#if (GPIO_INSTANCES >= 4)
	set_irq_type(GPIO3_IRQ, IRQ_TYPE_LEVEL_HIGH);
#endif

	for (i = 0; i < ARCH_NR_GPIOS; i++) {
		ambarella_gpio_set_valid(i, 1);
	}

	for (i = 0; i < ARRAY_SIZE(ambarella_gpio_banks); i++) {
		errorCode = gpiochip_add(&ambarella_gpio_banks[i].chip);
		if (errorCode) {
			pr_err("%s: gpiochip_add %s fail %d.\n",
				__func__,
				ambarella_gpio_banks[i].chip.label,
				errorCode);
			errorCode = -EINVAL;
			break;
		}
	}

#ifdef CONFIG_AMBARELLA_GPIO_PROC
	gpio_file = create_proc_entry(gpio_proc_name, S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (gpio_file == NULL) {
		pr_err("%s: %s fail!\n", __func__, gpio_proc_name);
		errorCode = -ENOMEM;
	} else {
		gpio_file->read_proc = ambarella_gpio_proc_read;
		gpio_file->write_proc = ambarella_gpio_proc_write;
		gpio_file->owner = THIS_MODULE;
	}
#endif

	return errorCode;
}

