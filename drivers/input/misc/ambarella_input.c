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

static struct ambarella_input_info *amba_input_dev = NULL;

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

#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
#include "ambarella_input_ir.c"
#endif
#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
#include "ambarella_input_adc.c"
#endif

irqreturn_t ambarella_gpio_irq(int irq, void *devid)
{
	int				i;
	int				gpio_id;
	int				level;
	struct ambarella_input_info	*pinfo;

	pinfo = (struct ambarella_input_info *)devid;

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
			input_report_key(pinfo->dev,
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
	struct ambarella_input_info	*pinfo = data;
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
			input_report_key(pinfo->dev, value1, value2);
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

static int __devinit ambarella_setup_keymap(struct ambarella_input_info *pinfo)
{
	int				i;
	int				vi_enabled = 0;
	int				errorCode;
	const struct firmware		*ext_key_map;

	if (keymap_name != NULL) {
		errorCode = request_firmware(&ext_key_map,
			keymap_name, &pinfo->dev->dev);
		if (errorCode) {
			printk(KERN_ERR"Can't load firmware, use default\n");
			goto ambarella_setup_keymap_init;
		}
		printk(KERN_NOTICE"Load %s, size = %d\n",
			keymap_name, ext_key_map->size);
		memset(default_ambarella_key_table, AMBA_INPUT_END,
			sizeof(default_ambarella_key_table));
		memcpy(default_ambarella_key_table,
			ext_key_map->data, ext_key_map->size);
	}

ambarella_setup_keymap_init:
	pinfo->pkeymap = default_ambarella_key_table;

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		switch (pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) {
		case AMBA_INPUT_SOURCE_IR:
			break;

#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
		case AMBA_INPUT_SOURCE_ADC:
			/* here, we record the adc channels that we will use */
			if (pinfo->pkeymap[i].adc_key.chan < pinfo->adc_channel_num) {
				pinfo->channel[pinfo->pkeymap[i].adc_key.chan].adc_channel_used = 1;
			} else {
				printk(KERN_ERR"The adc channel [%d] is not exist\n",
					pinfo->pkeymap[i].adc_key.chan);
				return errorCode;
			}

			if (pinfo->pkeymap[i].adc_key.key_code == KEY_RESERVED) {
				if (pinfo->pkeymap[i].adc_key.irq_trig) {
					pinfo->channel[pinfo->pkeymap[i].adc_key.chan].adc_high_trig
						= pinfo->pkeymap[i].adc_key.low_level;
					pinfo->channel[pinfo->pkeymap[i].adc_key.chan].adc_low_trig
						= 0;
				} else {
					pinfo->channel[pinfo->pkeymap[i].adc_key.chan].adc_low_trig
						= pinfo->pkeymap[i].adc_key.high_level;
					pinfo->channel[pinfo->pkeymap[i].adc_key.chan].adc_high_trig
						= 0;
					ambi_dbg(" adc_high_trig [%d], adc_low_trig [%d]\n",
					pinfo->channel[pinfo->pkeymap[i].adc_key.chan].adc_high_trig,
					pinfo->channel[pinfo->pkeymap[i].adc_key.chan].adc_low_trig);
				}
			}
			break;
#endif
		case AMBA_INPUT_SOURCE_GPIO:
			ambarella_gpio_config(pinfo->pkeymap[i].gpio_key.id, GPIO_FUNC_SW_INPUT);
			errorCode = request_irq(
				GPIO_INT_VEC(pinfo->pkeymap[i].gpio_key.id),
				ambarella_gpio_irq,
				pinfo->pkeymap[i].gpio_key.irq_mode,
				"ambarella_input_gpio", pinfo);
			if (errorCode)
				printk(KERN_ERR"Request GPIO%d IRQ failed!\n",
					pinfo->pkeymap[i].gpio_key.id);
			if (pinfo->pkeymap[i].gpio_key.can_wakeup) {
				errorCode = set_irq_wake(GPIO_INT_VEC(
					pinfo->pkeymap[i].gpio_key.id), 1);
				if (errorCode)
					printk(KERN_ERR"set_irq_wake %d failed!\n",
						pinfo->pkeymap[i].gpio_key.id);
			}
			errorCode = 0;	//Continue with error...
			break;

		case AMBA_INPUT_SOURCE_VI:
			vi_enabled = 1;
			break;

		default:
			printk(KERN_WARNING"Unknown AMBA_INPUT_SOURCE %d\n",
				(pinfo->pkeymap[i].type &
				AMBA_INPUT_SOURCE_MASK));
			break;
		}

		switch (pinfo->pkeymap[i].type & AMBA_INPUT_TYPE_MASK) {
		case AMBA_INPUT_TYPE_KEY:
			set_bit(EV_KEY, pinfo->dev->evbit);
			set_bit(pinfo->pkeymap[i].ir_key.key_code,
				pinfo->dev->keybit);
			break;

		case AMBA_INPUT_TYPE_REL:
			set_bit(EV_KEY, pinfo->dev->evbit);
			set_bit(EV_REL, pinfo->dev->evbit);
			set_bit(BTN_LEFT, pinfo->dev->keybit);
			set_bit(BTN_RIGHT, pinfo->dev->keybit);
			set_bit(REL_X, pinfo->dev->relbit);
			set_bit(REL_Y, pinfo->dev->relbit);
			break;

		case AMBA_INPUT_TYPE_ABS:
			set_bit(EV_KEY, pinfo->dev->evbit);
			set_bit(EV_ABS, pinfo->dev->evbit);
			set_bit(BTN_LEFT, pinfo->dev->keybit);
			set_bit(BTN_RIGHT, pinfo->dev->keybit);
			set_bit(BTN_TOUCH, pinfo->dev->keybit);
			set_bit(ABS_X, pinfo->dev->absbit);
			set_bit(ABS_Y, pinfo->dev->absbit);
			set_bit(ABS_PRESSURE, pinfo->dev->absbit);
			set_bit(ABS_TOOL_WIDTH, pinfo->dev->absbit);
			input_set_abs_params(pinfo->dev, ABS_X,
				0, abx_max_x, 0, 0);
			input_set_abs_params(pinfo->dev, ABS_Y,
				0, abx_max_y, 0, 0);
			input_set_abs_params(pinfo->dev, ABS_PRESSURE,
				0, abx_max_pressure, 0, 0);
			input_set_abs_params(pinfo->dev, ABS_TOOL_WIDTH,
				0, abx_max_width, 0, 0);
			break;

		default:
			printk(KERN_WARNING"Unknown AMBA_INPUT_TYPE %d\n",
				(pinfo->pkeymap[i].type &
				AMBA_INPUT_TYPE_MASK));
			break;
		}
	}

	if (vi_enabled)
		for (i = 0; i < 0x100; i++)
			set_bit(i, pinfo->dev->keybit);

	return errorCode;
}

static void ambarella_free_keymap(struct ambarella_input_info *pinfo)
{
	int				i;

	if (!pinfo->pkeymap)
		return;

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		switch (pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) {
		case AMBA_INPUT_SOURCE_IR:
			break;

		case AMBA_INPUT_SOURCE_ADC:
			break;

		case AMBA_INPUT_SOURCE_GPIO:
			if (pinfo->pkeymap[i].gpio_key.can_wakeup)
				set_irq_wake(GPIO_INT_VEC( pinfo->pkeymap[i].gpio_key.id), 0);
			free_irq(GPIO_INT_VEC(pinfo->pkeymap[i].gpio_key.id), pinfo);
			break;

		default:
			printk(KERN_WARNING"Unknown AMBA_INPUT_SOURCE %d\n",
				(pinfo->pkeymap[i].type &
				AMBA_INPUT_SOURCE_MASK));
			break;
		}
	}
}

static int __devinit ambarella_input_center_init(void)
{
	int				errorCode;
	struct proc_dir_entry		*input_file;
	struct input_dev		*pdev;
	struct ambarella_input_info 	*pinfo;

	pinfo = kmalloc(sizeof(struct ambarella_input_info), GFP_KERNEL);
	if (!pinfo) {
		printk(KERN_ERR "Amba: unable to allocate pinfo\n");
		errorCode = -ENOMEM;
		goto input_errorCode_na;
	}
	amba_input_dev = pinfo;

	pdev = input_allocate_device();
	if (!pdev) {
		dev_err(&pdev->dev, "Amba: unable to allocate input device\n");
		errorCode = -ENOMEM;
		goto input_errorCode_pinfo;
	}

	pinfo->dev = pdev;
	pdev->name = "Amba input center";
	pdev->phys = "ambarella/input0";
	pdev->id.bustype = BUS_HOST;

	errorCode = input_register_device(pdev);
	if (errorCode) {
		printk(KERN_ERR"Register input_dev failed!\n");
		goto input_errorCode_free_input_dev;
	}

	input_file = create_proc_entry(dev_name(&pdev->dev),
		S_IRUGO | S_IWUSR, get_ambarella_proc_dir());
	if (input_file == NULL) {
		printk(KERN_ERR"Register %s failed!\n",
			dev_name(&pdev->dev));
		errorCode = -ENOMEM;
		goto input_errorCode_unregister_device;
	} else {
		input_file->write_proc = ambarella_vi_proc_write;
		input_file->owner = THIS_MODULE;
		input_file->data = pinfo;
	}

#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
/* here, we get the ADC channel number, and allocate memory, which setup_keymap will use */
	pinfo->adc_channel_num = ambarella_adc_get_instances();
	ambi_dbg("adc has %d channels\n", pinfo->adc_channel_num);
	if (pinfo->adc_channel_num > 32)
		goto input_errorCode_unregister_device;

	amba_input_dev->channel = kzalloc(sizeof(struct ambarella_adc_channel_info)
			* amba_input_dev->adc_channel_num, GFP_KERNEL);
	if (!amba_input_dev->channel) {
		printk(KERN_ERR"Fail to malloc channel info\n");
		goto input_errorCode_unregister_device;
	}
#endif
	errorCode = ambarella_setup_keymap(pinfo);
	if (errorCode)
		goto input_errorCode_setup_keymap;

	printk(KERN_NOTICE "Ambarella Media Processor Input Center probed!\n" );

	goto input_errorCode_na;

input_errorCode_setup_keymap:
#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
	kfree(amba_input_dev->channel);
#endif
input_errorCode_unregister_device:
	input_unregister_device(pinfo->dev);

 input_errorCode_free_input_dev:
	input_free_device(pdev);

input_errorCode_pinfo:
	kfree(pinfo);

input_errorCode_na:
	return errorCode;
}

static int __devexit ambarella_input_center_remove(struct ambarella_input_info *pinfo)
{
	if (pinfo && pinfo->dev) {
		ambarella_free_keymap(pinfo);
		remove_proc_entry(dev_name(&pinfo->dev->dev), get_ambarella_proc_dir());
		input_unregister_device(pinfo->dev);
		input_free_device(pinfo->dev);
		kfree(pinfo);
	}

	printk(KERN_NOTICE "Ambarella Media Processor Input Center removed!\n" );

	return 0;
}

static int __init ambarella_input_init(void)
{
	int				errorCode = 0;

	errorCode = ambarella_input_center_init();
	if (errorCode) {
		printk(KERN_ERR "Register ambarella_input_driver failed!\n");
		return errorCode;
	}

#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
	errorCode = platform_driver_register(&ambarella_ir_driver);
	if (errorCode) {
		printk(KERN_ERR "Register ambarella_ir_driver failed!\n");
		ambarella_input_center_remove(amba_input_dev);
		return errorCode;
	}
#endif

#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
	errorCode = platform_driver_register(&ambarella_adc_driver);
	if (errorCode) {
		printk(KERN_ERR "Register ambarella_adc_driver failed!\n");
#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
		platform_driver_unregister(&ambarella_ir_driver);
#endif
		ambarella_input_center_remove(amba_input_dev);
		return errorCode;
	}
#endif

	return errorCode;
}

static void __exit ambarella_input_exit(void)
{

#if (defined CONFIG_INPUT_AMBARELLA_ADC_MODULE) || (defined CONFIG_INPUT_AMBARELLA_ADC)
	platform_driver_unregister(&ambarella_adc_driver);
#endif

#if (defined CONFIG_INPUT_AMBARELLA_IR_MODULE) || (defined CONFIG_INPUT_AMBARELLA_IR)
	platform_driver_unregister(&ambarella_ir_driver);
#endif
	ambarella_input_center_remove(amba_input_dev);

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

