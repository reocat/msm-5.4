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
	struct gpio_chip	chip;
	u32			base_reg;
	spinlock_t		lock;
};
#define to_ambarella_gpio_chip(c) \
	container_of(c, struct ambarella_gpio_chip, chip)

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

	spin_lock_irqsave(&agchip->lock, flags);
	amba_writel(agchip->base_reg + GPIO_MASK_OFFSET,
		(0x1 << offset));
	if (value == GPIO_LOW) {
		amba_clrbitsl(agchip->base_reg + GPIO_DATA_OFFSET,
			(0x1 << offset));
	} else {
		amba_setbitsl(agchip->base_reg + GPIO_DATA_OFFSET,
			(0x1 << offset));
	}
	spin_unlock_irqrestore(&agchip->lock, flags);
}

static inline int ambarella_gpio_inline_get(
	struct ambarella_gpio_chip *agchip, u32 offset)
{
	u32					val = 0;
	unsigned long				flags;

	spin_lock_irqsave(&agchip->lock, flags);
	amba_writel(agchip->base_reg + GPIO_MASK_OFFSET,
		(0x1 << offset));
	val = amba_readl(agchip->base_reg + GPIO_DATA_OFFSET);
	spin_unlock_irqrestore(&agchip->lock, flags);

	val = (val >> offset) & 0x1;
	return (val ? GPIO_HIGH : GPIO_LOW);
}

/* ==========================================================================*/
static DEFINE_MUTEX(ambarella_gpio_mtx);
static unsigned long ambarella_gpio_valid[BITS_TO_LONGS(AMBGPIO_SIZE)];
static unsigned long ambarella_gpio_freeflag[BITS_TO_LONGS(AMBGPIO_SIZE)];

static int ambarella_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	int					retval = 0;

	mutex_lock(&ambarella_gpio_mtx);

	if (test_bit((chip->base + offset), ambarella_gpio_valid)) {
		if (test_bit((chip->base + offset), ambarella_gpio_freeflag)) {
			__clear_bit((chip->base + offset),
				ambarella_gpio_freeflag);
		} else {
			retval = -EACCES;
		}
	} else {
		retval = -EPERM;
	}

	mutex_unlock(&ambarella_gpio_mtx);

	return retval;
}

static void ambarella_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	mutex_lock(&ambarella_gpio_mtx);

	__set_bit((chip->base + offset), ambarella_gpio_freeflag);

	mutex_unlock(&ambarella_gpio_mtx);
}

static int ambarella_gpio_chip_direction_input(struct gpio_chip *chip,
	unsigned offset)
{
	int					retval = 0;
	struct ambarella_gpio_chip		*agchip;

	agchip = to_ambarella_gpio_chip(chip);
	retval = ambarella_gpio_inline_config(agchip,
		offset, GPIO_FUNC_SW_INPUT);

	return retval;
}

static int ambarella_gpio_chip_get(struct gpio_chip *chip,
	unsigned offset)
{
	int					retval = 0;
	struct ambarella_gpio_chip		*agchip;

	agchip = to_ambarella_gpio_chip(chip);
	retval = ambarella_gpio_inline_get(agchip,
		offset);

	return retval;
}

static int ambarella_gpio_chip_direction_output(struct gpio_chip *chip,
	unsigned offset, int val)
{
	int					retval = 0;
	struct ambarella_gpio_chip		*agchip;

	agchip = to_ambarella_gpio_chip(chip);
	retval = ambarella_gpio_inline_config(agchip,
		offset, GPIO_FUNC_SW_OUTPUT);
	ambarella_gpio_inline_set(agchip, offset, val);

	return retval;
}

static void ambarella_gpio_chip_set(struct gpio_chip *chip,
	unsigned offset, int val)
{
	struct ambarella_gpio_chip		*agchip;

	agchip = to_ambarella_gpio_chip(chip);
	ambarella_gpio_inline_set(agchip, offset, val);
}

static int ambarella_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	if ((chip->base + offset) < GPIO_MAX_LINES)
		return GPIO_INT_VEC(chip->base + offset);

	return -EINVAL;
}

static void ambarella_gpio_chip_dbg_show(struct seq_file *s,
	struct gpio_chip *chip)
{
	int					i;
	struct ambarella_gpio_chip		*agchip;
	u32					afsel;
	u32					mask;
	u32					data;
	u32					dir;
	unsigned long				flags;

	agchip = to_ambarella_gpio_chip(chip);

	spin_lock_irqsave(&agchip->lock, flags);
	afsel = amba_readl(agchip->base_reg + GPIO_AFSEL_OFFSET);
	dir = amba_readl(agchip->base_reg + GPIO_DIR_OFFSET);
	mask = amba_readl(agchip->base_reg + GPIO_MASK_OFFSET);
	amba_writel(agchip->base_reg + GPIO_MASK_OFFSET, ~afsel);
	data = amba_readl(agchip->base_reg + GPIO_DATA_OFFSET);
	amba_writel(agchip->base_reg + GPIO_MASK_OFFSET, mask);
	spin_unlock_irqrestore(&agchip->lock, flags);

	seq_printf(s, "GPIO_BASE:\t0x%08X\n", agchip->base_reg);
	seq_printf(s, "GPIO_AFSEL:\t0x%08X\n", afsel);
	seq_printf(s, "GPIO_DIR:\t0x%08X\n", dir);
	seq_printf(s, "GPIO_MASK:\t0x%08X:0x%08X\n", mask, ~afsel);
	seq_printf(s, "GPIO_DATA:\t0x%08X\n", data);
	for (i = 0; i < chip->ngpio; i++) {
		seq_printf(s, "GPIO %d: ", (chip->base + i));
		if (afsel & (1 << i)) {
			seq_printf(s, "HW\n");
		} else {
			seq_printf(s, "%s\t%s\n",
				(dir & (1 << i)) ? "out" : "in",
				(data & (1 << i)) ? "set" : "clear");
		}
	}
}

#define AMBARELLA_GPIO_BANK(name, reg_base, base_gpio)			\
{									\
	.chip = {							\
		.label			= name,				\
		.owner			= THIS_MODULE,			\
		.request		= ambarella_gpio_request,	\
		.free			= ambarella_gpio_free,		\
		.direction_input	= ambarella_gpio_chip_direction_input,	\
		.get			= ambarella_gpio_chip_get,	\
		.direction_output	= ambarella_gpio_chip_direction_output, \
		.set			= ambarella_gpio_chip_set,	\
		.to_irq			= ambarella_gpio_to_irq,	\
		.dbg_show		= ambarella_gpio_chip_dbg_show,	\
		.base			= base_gpio,			\
		.ngpio			= GPIO_BANK_SIZE,		\
		.can_sleep		= 0,				\
		.exported		= 0,				\
	},								\
	.base_reg			= reg_base,			\
}

static struct ambarella_gpio_chip ambarella_gpio_banks[] = {
	AMBARELLA_GPIO_BANK("ambarella-gpio0",
		GPIO0_BASE, GPIO(0 * GPIO_BANK_SIZE)),
	AMBARELLA_GPIO_BANK("ambarella-gpio1",
		GPIO1_BASE, GPIO(1 * GPIO_BANK_SIZE)),
#if (GPIO_INSTANCES >= 3)
	AMBARELLA_GPIO_BANK("ambarella-gpio2",
		GPIO2_BASE, GPIO(2 * GPIO_BANK_SIZE)),
#endif
#if (GPIO_INSTANCES >= 4)
	AMBARELLA_GPIO_BANK("ambarella-gpio3",
		GPIO3_BASE, GPIO(3 * GPIO_BANK_SIZE)),
#endif
#if (GPIO_INSTANCES >= 5)
	AMBARELLA_GPIO_BANK("ambarella-gpio4",
		GPIO4_BASE, GPIO(4 * GPIO_BANK_SIZE)),
#endif
#if (GPIO_INSTANCES >= 6)
	AMBARELLA_GPIO_BANK("ambarella-gpio5",
		GPIO5_BASE, GPIO(5 * GPIO_BANK_SIZE)),
#endif
};

/* ==========================================================================*/
static struct ambarella_gpio_chip *ambarella_gpio_id_to_chip(int id)
{
	struct ambarella_gpio_chip		*chip = NULL;

	if (id < 0) {
		chip = NULL;
	} else if (id < (1 * GPIO_BANK_SIZE)) {
		chip = &ambarella_gpio_banks[0];
	} else if (id < (2 * GPIO_BANK_SIZE)) {
		chip = &ambarella_gpio_banks[1];
#if (GPIO_INSTANCES >= 3)
	} else if (id < (3 * GPIO_BANK_SIZE)) {
		chip = &ambarella_gpio_banks[2];
#endif
#if (GPIO_INSTANCES >= 4)
	} else if (id < (4 * GPIO_BANK_SIZE)) {
		chip = &ambarella_gpio_banks[3];
#endif
#if (GPIO_INSTANCES >= 5)
	} else if (id < (5 * GPIO_BANK_SIZE)) {
		chip = &ambarella_gpio_banks[4];
#endif
#if (GPIO_INSTANCES >= 6)
	} else if (id < (6 * GPIO_BANK_SIZE)) {
		chip = &ambarella_gpio_banks[5];
#endif
	}

	return chip;
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
void ambarella_gpio_raw_setbitsl(u32 id, u32 reg, u32 value)
{
	unsigned long				flags;

	spin_lock_irqsave(&ambarella_gpio_banks[id].lock, flags);
	amba_setbitsl(ambarella_gpio_banks[id].base_reg + reg, value);
	spin_unlock_irqrestore(&ambarella_gpio_banks[id].lock, flags);
}

void ambarella_gpio_raw_clrbitsl(u32 id, u32 reg, u32 value)
{
	unsigned long				flags;

	spin_lock_irqsave(&ambarella_gpio_banks[id].lock, flags);
	amba_clrbitsl(ambarella_gpio_banks[id].base_reg + reg, value);
	spin_unlock_irqrestore(&ambarella_gpio_banks[id].lock, flags);
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
	for (i = GPIO_MAX_LINES + 1; i < AMBGPIO_SIZE; i++) {
		ambarella_gpio_set_valid(i, 0);
		__clear_bit(i, ambarella_gpio_freeflag);
	}
	mutex_unlock(&ambarella_gpio_mtx);

	for (i = 0; i < ARRAY_SIZE(ambarella_gpio_banks); i++) {
		spin_lock_init(&ambarella_gpio_banks[i].lock);
		retval = gpiochip_add(&ambarella_gpio_banks[i].chip);
		if (retval) {
			pr_err("%s: gpiochip_add %s fail %d.\n", __func__,
				ambarella_gpio_banks[i].chip.label, retval);
			break;
		}
	}

	return retval;
}

/* ==========================================================================*/
struct ambarella_gpio_pm_info {
	u32 data_reg;
	u32 dir_reg;
	u32 is_reg;
	u32 ibe_reg;
	u32 iev_reg;
	u32 ie_reg;
	u32 afsel_reg;
	u32 mask_reg;
};

struct ambarella_gpio_pm_info ambarella_gpio0_pm;
struct ambarella_gpio_pm_info ambarella_gpio1_pm;
#if (GPIO_INSTANCES >= 3)
struct ambarella_gpio_pm_info ambarella_gpio2_pm;
#endif
#if (GPIO_INSTANCES >= 4)
struct ambarella_gpio_pm_info ambarella_gpio3_pm;
#endif
#if (GPIO_INSTANCES >= 5)
struct ambarella_gpio_pm_info ambarella_gpio4_pm;
#endif
#if (GPIO_INSTANCES >= 6)
struct ambarella_gpio_pm_info ambarella_gpio5_pm;
#endif

u32 ambarella_gpio_suspend(u32 level)
{
	ambarella_gpio0_pm.data_reg   = amba_readl(GPIO0_DATA_REG);
	ambarella_gpio0_pm.dir_reg    = amba_readl(GPIO0_DIR_REG);
	ambarella_gpio0_pm.is_reg     = amba_readl(GPIO0_IS_REG);
	ambarella_gpio0_pm.ibe_reg    = amba_readl(GPIO0_IBE_REG);
	ambarella_gpio0_pm.iev_reg    = amba_readl(GPIO0_IEV_REG);
	ambarella_gpio0_pm.ie_reg     = amba_readl(GPIO0_IE_REG);
	ambarella_gpio0_pm.afsel_reg  = amba_readl(GPIO0_AFSEL_REG);
	ambarella_gpio0_pm.mask_reg   = amba_readl(GPIO0_MASK_REG);

	ambarella_gpio1_pm.data_reg   = amba_readl(GPIO1_DATA_REG);
	ambarella_gpio1_pm.dir_reg    = amba_readl(GPIO1_DIR_REG);
	ambarella_gpio1_pm.is_reg     = amba_readl(GPIO1_IS_REG);
	ambarella_gpio1_pm.ibe_reg    = amba_readl(GPIO1_IBE_REG);
	ambarella_gpio1_pm.iev_reg    = amba_readl(GPIO1_IEV_REG);
	ambarella_gpio1_pm.ie_reg     = amba_readl(GPIO1_IE_REG);
	ambarella_gpio1_pm.afsel_reg  = amba_readl(GPIO1_AFSEL_REG);
	ambarella_gpio1_pm.mask_reg   = amba_readl(GPIO1_MASK_REG);

#if (GPIO_INSTANCES >= 3)
	ambarella_gpio2_pm.data_reg   = amba_readl(GPIO2_DATA_REG);
	ambarella_gpio2_pm.dir_reg    = amba_readl(GPIO2_DIR_REG);
	ambarella_gpio2_pm.is_reg     = amba_readl(GPIO2_IS_REG);
	ambarella_gpio2_pm.ibe_reg    = amba_readl(GPIO2_IBE_REG);
	ambarella_gpio2_pm.iev_reg    = amba_readl(GPIO2_IEV_REG);
	ambarella_gpio2_pm.ie_reg     = amba_readl(GPIO2_IE_REG);
	ambarella_gpio2_pm.afsel_reg  = amba_readl(GPIO2_AFSEL_REG);
	ambarella_gpio2_pm.mask_reg   = amba_readl(GPIO2_MASK_REG);
#endif

#if (GPIO_INSTANCES >= 4)
	ambarella_gpio3_pm.data_reg   = amba_readl(GPIO3_DATA_REG);
	ambarella_gpio3_pm.dir_reg    = amba_readl(GPIO3_DIR_REG);
	ambarella_gpio3_pm.is_reg     = amba_readl(GPIO3_IS_REG);
	ambarella_gpio3_pm.ibe_reg    = amba_readl(GPIO3_IBE_REG);
	ambarella_gpio3_pm.iev_reg    = amba_readl(GPIO3_IEV_REG);
	ambarella_gpio3_pm.ie_reg     = amba_readl(GPIO3_IE_REG);
	ambarella_gpio3_pm.afsel_reg  = amba_readl(GPIO3_AFSEL_REG);
	ambarella_gpio3_pm.mask_reg   = amba_readl(GPIO3_MASK_REG);
#endif

#if (GPIO_INSTANCES >= 5)
	ambarella_gpio4_pm.data_reg   = amba_readl(GPIO4_DATA_REG);
	ambarella_gpio4_pm.dir_reg    = amba_readl(GPIO4_DIR_REG);
	ambarella_gpio4_pm.is_reg     = amba_readl(GPIO4_IS_REG);
	ambarella_gpio4_pm.ibe_reg    = amba_readl(GPIO4_IBE_REG);
	ambarella_gpio4_pm.iev_reg    = amba_readl(GPIO4_IEV_REG);
	ambarella_gpio4_pm.ie_reg     = amba_readl(GPIO4_IE_REG);
	ambarella_gpio4_pm.afsel_reg  = amba_readl(GPIO4_AFSEL_REG);
	ambarella_gpio4_pm.mask_reg   = amba_readl(GPIO4_MASK_REG);
#endif

#if (GPIO_INSTANCES >= 6)
	ambarella_gpio5_pm.data_reg   = amba_readl(GPIO5_DATA_REG);
	ambarella_gpio5_pm.dir_reg    = amba_readl(GPIO5_DIR_REG);
	ambarella_gpio5_pm.is_reg     = amba_readl(GPIO5_IS_REG);
	ambarella_gpio5_pm.ibe_reg    = amba_readl(GPIO5_IBE_REG);
	ambarella_gpio5_pm.iev_reg    = amba_readl(GPIO5_IEV_REG);
	ambarella_gpio5_pm.ie_reg     = amba_readl(GPIO5_IE_REG);
	ambarella_gpio5_pm.afsel_reg  = amba_readl(GPIO5_AFSEL_REG);
	ambarella_gpio5_pm.mask_reg   = amba_readl(GPIO5_MASK_REG);
#endif

	return 0;
}

u32 ambarella_gpio_resume(u32 level)
{
	amba_writel(GPIO0_AFSEL_REG,  ambarella_gpio0_pm.afsel_reg);
	amba_writel(GPIO0_MASK_REG,   ambarella_gpio0_pm.mask_reg);
	amba_writel(GPIO0_DIR_REG,    ambarella_gpio0_pm.dir_reg);
	amba_writel(GPIO0_DATA_REG,   ambarella_gpio0_pm.data_reg);
	amba_writel(GPIO0_IS_REG,     ambarella_gpio0_pm.is_reg);
	amba_writel(GPIO0_IBE_REG,    ambarella_gpio0_pm.ibe_reg);
	amba_writel(GPIO0_IEV_REG,    ambarella_gpio0_pm.iev_reg);
	amba_writel(GPIO0_IE_REG,     ambarella_gpio0_pm.ie_reg);

	amba_writel(GPIO1_AFSEL_REG,  ambarella_gpio1_pm.afsel_reg);
	amba_writel(GPIO1_MASK_REG,   ambarella_gpio1_pm.mask_reg);
	amba_writel(GPIO1_DIR_REG,    ambarella_gpio1_pm.dir_reg);
	amba_writel(GPIO1_DATA_REG,   ambarella_gpio1_pm.data_reg);
	amba_writel(GPIO1_IS_REG,     ambarella_gpio1_pm.is_reg);
	amba_writel(GPIO1_IBE_REG,    ambarella_gpio1_pm.ibe_reg);
	amba_writel(GPIO1_IEV_REG,    ambarella_gpio1_pm.iev_reg);
	amba_writel(GPIO1_IE_REG,     ambarella_gpio1_pm.ie_reg);

#if (GPIO_INSTANCES >= 3)
	amba_writel(GPIO2_AFSEL_REG,  ambarella_gpio2_pm.afsel_reg);
	amba_writel(GPIO2_MASK_REG,   ambarella_gpio2_pm.mask_reg);
	amba_writel(GPIO2_DIR_REG,    ambarella_gpio2_pm.dir_reg);
	amba_writel(GPIO2_DATA_REG,   ambarella_gpio2_pm.data_reg);
	amba_writel(GPIO2_IS_REG,     ambarella_gpio2_pm.is_reg);
	amba_writel(GPIO2_IBE_REG,    ambarella_gpio2_pm.ibe_reg);
	amba_writel(GPIO2_IEV_REG,    ambarella_gpio2_pm.iev_reg);
	amba_writel(GPIO2_IE_REG,     ambarella_gpio2_pm.ie_reg);
#endif

#if (GPIO_INSTANCES >= 4)
	amba_writel(GPIO3_AFSEL_REG,  ambarella_gpio3_pm.afsel_reg);
	amba_writel(GPIO3_MASK_REG,   ambarella_gpio3_pm.mask_reg);
	amba_writel(GPIO3_DIR_REG,    ambarella_gpio3_pm.dir_reg);
	amba_writel(GPIO3_DATA_REG,   ambarella_gpio3_pm.data_reg);
	amba_writel(GPIO3_IS_REG,     ambarella_gpio3_pm.is_reg);
	amba_writel(GPIO3_IBE_REG,    ambarella_gpio3_pm.ibe_reg);
	amba_writel(GPIO3_IEV_REG,    ambarella_gpio3_pm.iev_reg);
	amba_writel(GPIO3_IE_REG,     ambarella_gpio3_pm.ie_reg);
#endif

#if (GPIO_INSTANCES >= 5)
	amba_writel(GPIO4_AFSEL_REG,  ambarella_gpio4_pm.afsel_reg);
	amba_writel(GPIO4_MASK_REG,   ambarella_gpio4_pm.mask_reg);
	amba_writel(GPIO4_DIR_REG,    ambarella_gpio4_pm.dir_reg);
	amba_writel(GPIO4_DATA_REG,   ambarella_gpio4_pm.data_reg);
	amba_writel(GPIO4_IS_REG,     ambarella_gpio4_pm.is_reg);
	amba_writel(GPIO4_IBE_REG,    ambarella_gpio4_pm.ibe_reg);
	amba_writel(GPIO4_IEV_REG,    ambarella_gpio4_pm.iev_reg);
	amba_writel(GPIO4_IE_REG,     ambarella_gpio4_pm.ie_reg);
#endif

#if (GPIO_INSTANCES >= 6)
	amba_writel(GPIO5_AFSEL_REG,  ambarella_gpio5_pm.afsel_reg);
	amba_writel(GPIO5_MASK_REG,   ambarella_gpio5_pm.mask_reg);
	amba_writel(GPIO5_DIR_REG,    ambarella_gpio5_pm.dir_reg);
	amba_writel(GPIO5_DATA_REG,   ambarella_gpio5_pm.data_reg);
	amba_writel(GPIO5_IS_REG,     ambarella_gpio5_pm.is_reg);
	amba_writel(GPIO5_IBE_REG,    ambarella_gpio5_pm.ibe_reg);
	amba_writel(GPIO5_IEV_REG,    ambarella_gpio5_pm.iev_reg);
	amba_writel(GPIO5_IE_REG,     ambarella_gpio5_pm.ie_reg);
#endif

	return 0;
}

/* ==========================================================================*/
static inline int ambarella_gpio_check(struct ambarella_gpio_io_info *pinfo)
{
	int					retval = 0;

	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL.\n", __func__);
		retval = -1;
		goto ambarella_gpio_check_exit;
	}

	if ((pinfo->gpio_id < 0 ) || (pinfo->gpio_id >= GPIO_MAX_LINES)) {
		pr_debug("%s: wrong gpio id %d.\n", __func__, pinfo->gpio_id);
		retval = -1;
		goto ambarella_gpio_check_exit;
	}

ambarella_gpio_check_exit:
	return retval;
}

int ambarella_set_gpio_output(struct ambarella_gpio_io_info *pinfo, u32 on)
{
	int					retval = 0;

	retval = ambarella_gpio_check(pinfo);
	if (retval)
		goto ambarella_set_gpio_output_exit;

	pr_debug("%s: Gpio[%d] %s, level[%s], delay[%dms].\n", __func__,
		pinfo->gpio_id, on ? "ON" : "OFF",
		pinfo->active_level ? "HIGH" : "LOW",
		pinfo->active_delay);

	ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_SW_OUTPUT);
	if (on)
		ambarella_gpio_set(pinfo->gpio_id, pinfo->active_level);
	else
		ambarella_gpio_set(pinfo->gpio_id, !pinfo->active_level);
	mdelay(pinfo->active_delay);

ambarella_set_gpio_output_exit:
	return retval;
}
EXPORT_SYMBOL(ambarella_set_gpio_output);

u32 ambarella_get_gpio_input(struct ambarella_gpio_io_info *pinfo)
{
	int					retval = 0;
	u32					gpio_value = 0;

	retval = ambarella_gpio_check(pinfo);
	if (retval)
		goto ambarella_get_gpio_input_exit;

	ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_SW_INPUT);
	mdelay(pinfo->active_delay);
	gpio_value = ambarella_gpio_get(pinfo->gpio_id);

	pr_debug("%s: {gpio[%d], level[%s], delay[%dms]} get[%d].\n",
		__func__, pinfo->gpio_id,
		pinfo->active_level ? "HIGH" : "LOW",
		pinfo->active_delay, gpio_value);

ambarella_get_gpio_input_exit:
	return (gpio_value == pinfo->active_level) ? 1 : 0;
}
EXPORT_SYMBOL(ambarella_get_gpio_input);

int ambarella_set_gpio_reset(struct ambarella_gpio_io_info *pinfo)
{
	int					retval = 0;

	retval = ambarella_gpio_check(pinfo);
	if (retval)
		goto ambarella_set_gpio_reset_exit;

	pr_debug("%s: Reset gpio[%d], level[%s], delay[%dms].\n",
		__func__, pinfo->gpio_id,
		pinfo->active_level ? "HIGH" : "LOW",
		pinfo->active_delay);

	ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_SW_OUTPUT);
	ambarella_gpio_set(pinfo->gpio_id, pinfo->active_level);
	mdelay(pinfo->active_delay);
	ambarella_gpio_set(pinfo->gpio_id, !pinfo->active_level);
	mdelay(pinfo->active_delay);

ambarella_set_gpio_reset_exit:
	return retval;
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
		((pinfo->irq_line < GPIO_INT_VEC(0)) ||
		(pinfo->irq_line >= NR_IRQS)))
		goto ambarella_is_valid_gpio_irq_exit;

	bvalid = 1;

ambarella_is_valid_gpio_irq_exit:
	return bvalid;
}
EXPORT_SYMBOL(ambarella_is_valid_gpio_irq);

