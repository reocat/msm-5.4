/*
 * drivers/input/misc/ambarella_input.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
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
 * adc update: Qiao Wang 2009/10/28
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/input.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>

#include <mach/hardware.h>
#include "ambarella_input.h"


/* ========================================================================= */
#define CONFIG_AMBARELLA_VI_BUFFER		(32)

/* ========================================================================= */
static int print_keycode = 0;
MODULE_PARM_DESC(print_keycode, "Print Key Code");

static int abx_max_x = 720;
MODULE_PARM_DESC(abx_max_x, "Ambarella input max x");

static int abx_max_y = 480;
MODULE_PARM_DESC(abx_max_y, "Ambarella input max y");

static int abx_max_pressure = 512;
MODULE_PARM_DESC(abx_max_pressure, "Ambarella input max pressure");

static int abx_max_width = 16;
MODULE_PARM_DESC(abx_max_width, "Ambarella input max width");

static char *keymap_name = NULL;
MODULE_PARM_DESC(keymap_name, "Ambarella Input's key map");

/* ========================================================================= */
static struct ambarella_key_table \
	default_ambarella_key_table[CONFIG_AMBARELLA_INPUT_SIZE] = {
	{AMBA_INPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBA_INPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBA_INPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},

	{AMBA_INPUT_END}
};

/* ========================================================================= */

#include "ambarella_input_ir.c"

#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
#include "ambarella_input_adc.c"
#endif

irqreturn_t ambarella_gpio_irq(int irq, void *devid)
{
	int				i;
	int				gpio_id;
	int				level;
	struct ambarella_ir_info	*pinfo;

	pinfo = (struct ambarella_ir_info *)devid;

	gpio_id = irq - GPIO_INT_VEC_OFFSET;
	level = ambarella_gpio_get(gpio_id);

	if (!pinfo->pkeymap)
		goto ambarella_gpio_irq_exit;

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		if ((pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) !=
			AMBA_INPUT_SOURCE_GPIO)
			continue;

		if (pinfo->pkeymap[i].gpio_key.id != gpio_id)
			continue;

		if (pinfo->pkeymap[i].type == AMBA_INPUT_GPIO_KEY) {
			ambarella_report_key(pinfo,
				pinfo->pkeymap[i].gpio_key.key_code, level);
			ambi_dbg("GPIO %d is @ %d:%d\n",
				pinfo->pkeymap[i].gpio_key.key_code,
				gpio_id, level);
			break;
		}

		if (pinfo->pkeymap[i].type == AMBA_INPUT_GPIO_REL) {
			if (pinfo->pkeymap[i].gpio_rel.key_code == REL_X) {
				input_report_rel(pinfo->dev,
					REL_X,
					pinfo->pkeymap[i].gpio_rel.rel_step);
				input_report_rel(pinfo->dev,
					REL_Y, 0);
				AMBI_MUST_SYNC();
				ambi_dbg("report REL_X %d @ %d:%d\n",
					pinfo->pkeymap[i].gpio_rel.rel_step,
					gpio_id, level);
			} else
			if (pinfo->pkeymap[i].gpio_rel.key_code == REL_Y) {
				input_report_rel(pinfo->dev,
					REL_X, 0);
				input_report_rel(pinfo->dev,
					REL_Y,
					pinfo->pkeymap[i].gpio_rel.rel_step);
				AMBI_MUST_SYNC();
				ambi_dbg("report REL_Y %d @ %d:%d\n",
					pinfo->pkeymap[i].gpio_rel.rel_step,
					gpio_id, level);
			}
			break;
		}

		if (pinfo->pkeymap[i].type == AMBA_INPUT_GPIO_ABS) {
			input_report_abs(pinfo->dev,
				ABS_X, pinfo->pkeymap[i].gpio_abs.abs_x);
			input_report_abs(pinfo->dev,
				ABS_Y, pinfo->pkeymap[i].gpio_abs.abs_y);
			AMBI_MUST_SYNC();
			ambi_dbg("report ABS %d:%d @ %d:%d\n",
				pinfo->pkeymap[i].gpio_abs.abs_x,
				pinfo->pkeymap[i].gpio_abs.abs_y,
				gpio_id, level);
			break;
		}
	}

ambarella_gpio_irq_exit:
	return IRQ_HANDLED;
}

int ambarella_vi_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	struct ambarella_ir_info	*pinfo = data;
	u32				key_num;
	char				key_buffer[CONFIG_AMBARELLA_VI_BUFFER];
	u32				value1;
	u32				value2;

	memset(key_buffer, 0, CONFIG_AMBARELLA_VI_BUFFER);
	if (count < CONFIG_AMBARELLA_VI_BUFFER) {
		if (copy_from_user(key_buffer, buffer, count))
			return -EINVAL;

		key_num = sscanf(key_buffer, "%*s %d:%d", &value1, &value2);

		if (print_keycode)
			printk("Get %d data[%s : %d:%d]\n",
				key_num, key_buffer, value1, value2);
#if 0
		if (key_num < 2)
			return -EINVAL;
#endif
		if (memcmp(key_buffer, "key", 3) == 0) 	{
			ambarella_report_key(pinfo, value1, value2);
		}else
		if (memcmp(key_buffer, "rel", 3) == 0) 	{
			input_report_rel(pinfo->dev, REL_X, value1);
			input_report_rel(pinfo->dev, REL_Y, value2);
			AMBI_MUST_SYNC();
		}else
		if (memcmp(key_buffer, "abs", 3) == 0) 	{
			input_report_abs(pinfo->dev, ABS_X, value1);
			input_report_abs(pinfo->dev, ABS_Y, value2);
			AMBI_MUST_SYNC();
		}else
		if (memcmp(key_buffer, "ton", 3) == 0) 	{
			input_report_key(pinfo->dev, BTN_TOUCH, 1);
			input_report_abs(pinfo->dev, ABS_X, value1);
			input_report_abs(pinfo->dev, ABS_Y, value2);
			AMBI_MUST_SYNC();
		}else
		if (memcmp(key_buffer, "tof", 3) == 0) 	{
			input_report_key(pinfo->dev, BTN_TOUCH, 0);
			AMBI_MUST_SYNC();
		}else
		if (memcmp(key_buffer, "pre", 3) == 0) 	{
			input_report_key(pinfo->dev, ABS_PRESSURE, value1);
			AMBI_MUST_SYNC();
		}else
		if (memcmp(key_buffer, "wid", 3) == 0) 	{
			input_report_key(pinfo->dev, ABS_TOOL_WIDTH, value1);
			AMBI_MUST_SYNC();
		}
	}

	return count;
}

static int __init ambarella_input_init(void)
{
	int				errorCode = 0;

	errorCode = platform_driver_register(&ambarella_ir_driver);
	if (errorCode)
		printk(KERN_ERR "Register ambarella_ir_driver failed!\n");

#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
	errorCode = platform_driver_register(&ambarella_adc_driver);
	if (errorCode) {
		printk(KERN_ERR "Register ambarella_adc_driver failed!\n");
		platform_driver_unregister(&ambarella_ir_driver);
	}
#endif
	return errorCode;
}

static void __exit ambarella_input_exit(void)
{
	platform_driver_unregister(&ambarella_ir_driver);
#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
	platform_driver_unregister(&ambarella_adc_driver);
#endif
}

module_init(ambarella_input_init);
module_exit(ambarella_input_exit);

MODULE_DESCRIPTION("Ambarella Media Processor input driver");
MODULE_AUTHOR("Anthony Ginger, <hfjiang@ambarella.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ambai");

module_param(print_keycode, int, 0644);
module_param(abx_max_x, int, 0644);
module_param(abx_max_y, int, 0644);
module_param(abx_max_pressure, int, 0644);
module_param(abx_max_width, int, 0644);
module_param(keymap_name, charp, 0644);

