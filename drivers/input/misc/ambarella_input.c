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
#define CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT	(100000)
#define CONFIG_AMBARELLA_VI_BUFFER		(32)

/* ========================================================================= */
static int ir_protocol = AMBA_IR_PROTOCOL_NEC;
MODULE_PARM_DESC(ir_protocol, "Ambarella IR Protocol ID");

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

static int adc_scan_delay = 20;
MODULE_PARM_DESC(adc_scan_delay, "Ambarella ADC scan (jiffies)");

static char *keymap_name = NULL;
MODULE_PARM_DESC(keymap_name, "Ambarella Input's key map");

static int enable_input = 1;
MODULE_PARM_DESC(enable_input, "Enable Ambarella Input driver");

/* ========================================================================= */
static struct ambarella_key_table \
	default_ambarella_key_table[CONFIG_AMBARELLA_INPUT_SIZE] = {
	{AMBA_INPUT_VI_KEY,	{.vi_key	= {0,	0,	0}}},
	{AMBA_INPUT_VI_REL,	{.vi_rel	= {0,	0,	0}}},
	{AMBA_INPUT_VI_ABS,	{.vi_abs	= {0,	0,	0}}},

	{AMBA_INPUT_END}
};

/* ========================================================================= */
static void ambarella_report_key(struct ambarella_ir_info *pinfo,
	unsigned int code, int value)
{
	input_report_key(pinfo->dev, code, value);
	AMBI_CAN_SYNC();
}

static irqreturn_t ambarella_gpio_irq(int irq, void *devid)
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

static int __devinit ambarella_setup_keymap(struct ambarella_ir_info *pinfo)
{
	int				i;
	int				vi_enabled = 0;
	int				errorCode;
	const struct firmware		*ext_key_map;
	int				ret;

	if (keymap_name != NULL) {
		ret = request_firmware(&ext_key_map,
			keymap_name, pinfo->dev->dev.parent);
		if (ret) {
			ambi_err("Can't load firmware, use default\n");
			goto ambarella_setup_keymap_init;
		}
		ambi_notice("Load %s, size = %d\n",
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

		case AMBA_INPUT_SOURCE_ADC:
			if (pinfo->init_adc == 1) {
				amba_writel(ADC_RESET_REG, 0x01);
				amba_writel(ADC_ENABLE_REG, 0x01);
				msleep(3);
				amba_writel(ADC_CONTROL_REG,
					ADC_CONTROL_START | ADC_CONTROL_MODE);
				pinfo->init_adc = 0;
				queue_delayed_work(pinfo->workqueue,
					&pinfo->detect_adc, 100);
			}
			break;

		case AMBA_INPUT_SOURCE_GPIO:
			ambarella_gpio_config(pinfo->pkeymap[i].gpio_key.id,
				GPIO_FUNC_SW_INPUT);
			errorCode = request_irq(
				GPIO_INT_VEC(pinfo->pkeymap[i].gpio_key.id),
				ambarella_gpio_irq,
				pinfo->pkeymap[i].gpio_key.irq_mode,
				"ambarella_input_gpio", pinfo);
			if (errorCode)
				ambi_err("Request GPIO%d IRQ failed!\n",
					pinfo->pkeymap[i].gpio_key.id);
			break;

		case AMBA_INPUT_SOURCE_VI:
			vi_enabled = 1;
			break;

		default:
			ambi_warn("Unknown AMBA_INPUT_SOURCE %d\n",
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
			ambi_warn("Unknown AMBA_INPUT_TYPE %d\n",
				(pinfo->pkeymap[i].type &
				AMBA_INPUT_TYPE_MASK));
			break;
		}
	}

	if (vi_enabled)
		for (i = 0; i < 0x100; i++)
			set_bit(i, pinfo->dev->keybit);

	return 0;
}

static void ambarella_free_keymap(struct ambarella_ir_info *pinfo)
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
			if (pinfo->init_adc == 0) {
				pinfo->init_adc = 1;
				cancel_delayed_work(&pinfo->detect_adc);
				flush_workqueue(pinfo->workqueue);
			}
			break;

		case AMBA_INPUT_SOURCE_GPIO:
			free_irq(GPIO_INT_VEC(pinfo->pkeymap[i].gpio_key.id),
				pinfo);
			break;

		default:
			ambi_warn("Unknown AMBA_INPUT_SOURCE %d\n",
				(pinfo->pkeymap[i].type &
				AMBA_INPUT_SOURCE_MASK));
			break;
		}
	}
}

static int ambarella_input_report_ir(struct ambarella_ir_info *pinfo, u32 uid)
{
	int				i;

	if (!pinfo->pkeymap)
		return -1;

	if ((pinfo->last_ir_uid == uid) && (pinfo->last_ir_flag)) {
		pinfo->last_ir_flag--;
		return 0;
	}

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		if ((pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) !=
			AMBA_INPUT_SOURCE_IR)
			continue;

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_KEY) &&
			(pinfo->pkeymap[i].ir_key.raw_id == uid)) {
			ambarella_report_key(pinfo,
				pinfo->pkeymap[i].ir_key.key_code, 1);
			ambarella_report_key(pinfo,
				pinfo->pkeymap[i].ir_key.key_code, 0);
			ambi_dbg("IR_KEY [%d]\n",
				pinfo->pkeymap[i].ir_key.key_code);
			pinfo->last_ir_flag = pinfo->pkeymap[i].ir_key.key_flag;
			break;
		}

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_REL) &&
			(pinfo->pkeymap[i].ir_rel.raw_id == uid)) {
			if (pinfo->pkeymap[i].ir_rel.key_code == REL_X) {
				input_report_rel(pinfo->dev,
					REL_X,
					pinfo->pkeymap[i].ir_rel.rel_step);
				input_report_rel(pinfo->dev,
					REL_Y, 0);
				AMBI_MUST_SYNC();
				ambi_dbg("IR_REL X[%d]:Y[%d]\n",
					pinfo->pkeymap[i].ir_rel.rel_step, 0);
			} else
			if (pinfo->pkeymap[i].ir_rel.key_code == REL_Y) {
				input_report_rel(pinfo->dev,
					REL_X, 0);
				input_report_rel(pinfo->dev,
					REL_Y,
					pinfo->pkeymap[i].ir_rel.rel_step);
				AMBI_MUST_SYNC();
				ambi_dbg("IR_REL X[%d]:Y[%d]\n", 0,
					pinfo->pkeymap[i].ir_rel.rel_step);
			}
			pinfo->last_ir_flag = 0;
			break;
		}

		if ((pinfo->pkeymap[i].type == AMBA_INPUT_IR_ABS) &&
			(pinfo->pkeymap[i].ir_abs.raw_id == uid)) {
			input_report_abs(pinfo->dev,
				ABS_X, pinfo->pkeymap[i].ir_abs.abs_x);
			input_report_abs(pinfo->dev,
				ABS_Y, pinfo->pkeymap[i].ir_abs.abs_y);
			AMBI_MUST_SYNC();
			ambi_dbg("IR_ABS X[%d]:Y[%d]\n",
				pinfo->pkeymap[i].ir_abs.abs_x,
				pinfo->pkeymap[i].ir_abs.abs_y);
			pinfo->last_ir_flag = 0;
			break;
		}
	}

	return 0;
}

static inline int ambarella_ir_get_tick_size(struct ambarella_ir_info *pinfo)
{
	int				size = 0;

	if (pinfo->ir_pread > pinfo->ir_pwrite)
		size = MAX_IR_BUFFER - pinfo->ir_pread + pinfo->ir_pwrite;
	else
		size = pinfo->ir_pwrite - pinfo->ir_pread;

	return size;
}

int ambarella_ir_inc_read_ptr(struct ambarella_ir_info *pinfo)
{
	if (pinfo->ir_pread == pinfo->ir_pwrite)
		return -1;

	pinfo->ir_pread++;
	if (pinfo->ir_pread >= MAX_IR_BUFFER)
		pinfo->ir_pread = 0;

	return 0;
}

int ambarella_ir_move_read_ptr(struct ambarella_ir_info *pinfo, int offset)
{
	int				rval = 0;

	for (; offset > 0; offset--) {
		rval = ambarella_ir_inc_read_ptr(pinfo);
		if (rval < 0)
			return rval;
	}
	return rval;
}

static inline void ambarella_ir_write_data(struct ambarella_ir_info *pinfo,
	u16 val)
{
	K_ASSERT(pinfo->ir_pwrite >= 0);
	K_ASSERT(pinfo->ir_pwrite < MAX_IR_BUFFER);

	pinfo->tick_buf[pinfo->ir_pwrite] = val;

	pinfo->ir_pwrite++;

	if (pinfo->ir_pwrite >= MAX_IR_BUFFER)
		pinfo->ir_pwrite = 0;
}

u16 ambarella_ir_read_data(struct ambarella_ir_info *pinfo, int pointer)
{
	K_ASSERT(pointer >= 0);
	K_ASSERT(pointer < MAX_IR_BUFFER);

	if (pointer == pinfo->ir_pwrite)
		return 0;

	return pinfo->tick_buf[pointer];
}

static irqreturn_t ambarella_ir_irq(int irq, void *devid)
{
	struct ambarella_ir_info	*pinfo;
	int				count;
	int				size;
	int				cur_ptr;
	int				temp_ptr;
	int				rval;
	u32				uid;

	pinfo = (struct ambarella_ir_info *)devid;

	K_ASSERT(pinfo->ir_pread >= 0);
	K_ASSERT(pinfo->ir_pread < MAX_IR_BUFFER);

	if (amba_readl(pinfo->regbase + IR_CONTROL_OFFSET) &
		IR_CONTROL_FIFO_OV) {
		while (amba_readl(pinfo->regbase + IR_STATUS_OFFSET) > 0) {
			amba_readl(pinfo->regbase + IR_DATA_OFFSET);
		}

		goto ambarella_ir_irq_exit;
	}

	count = amba_readl(pinfo->regbase + IR_STATUS_OFFSET);
	if (count == 0)
		goto ambarella_ir_irq_exit;

	for (; count > 0; count--) {
		ambarella_ir_write_data(pinfo, 
			amba_readl(pinfo->regbase + IR_DATA_OFFSET));
	}

	size = ambarella_ir_get_tick_size(pinfo);

	cur_ptr = pinfo->ir_pread;

	for (; size > 0; size--) {
		temp_ptr = pinfo->ir_pread;

		rval = pinfo->ir_parse(pinfo, &uid);
		if (rval < 0) {
			pinfo->ir_pread = temp_ptr;
			rval = ambarella_ir_inc_read_ptr(pinfo);
			if (rval < 0)
				goto ambarella_ir_irq_exit;

			continue;
		}

		if (print_keycode)
			printk("uid = 0x%08x\n", uid);
		ambarella_input_report_ir(pinfo, uid);

		temp_ptr = pinfo->ir_pread;
		cur_ptr  = pinfo->ir_pread;

		size = ambarella_ir_get_tick_size(pinfo);
	}

	pinfo->ir_pread = cur_ptr;

ambarella_ir_irq_exit:

	amba_writel(pinfo->regbase + IR_CONTROL_OFFSET,
		(amba_readl(pinfo->regbase + IR_CONTROL_OFFSET) |
		IR_CONTROL_LEVINT));

	return IRQ_HANDLED;
}

void ambarella_ir_enable(struct ambarella_ir_info *pinfo)
{
	amba_writel(pinfo->regbase + IR_CONTROL_OFFSET, IR_CONTROL_RESET);
	amba_writel(pinfo->regbase + IR_CONTROL_OFFSET,
		(amba_readl((pinfo->regbase + IR_CONTROL_OFFSET))	|
		IR_CONTROL_ENB		|
		IR_CONTROL_INTLEV(24)	|
		IR_CONTROL_INTENB));

	enable_irq(pinfo->irq);
}

void ambarella_ir_disable(struct ambarella_ir_info *pinfo)
{
	disable_irq(pinfo->irq);
}

void ambarella_ir_set_protocol(struct ambarella_ir_info *pinfo,
	enum ambarella_ir_protocol protocol_id)
{
	memset(pinfo->tick_buf, 0x0, sizeof(pinfo->tick_buf));
	pinfo->ir_pread  = 0;
	pinfo->ir_pwrite = 0;

	switch (protocol_id) {
	case AMBA_IR_PROTOCOL_NEC:
		pinfo->ir_parse = ambarella_ir_nec_parse;
		break;
	case AMBA_IR_PROTOCOL_PANASONIC:
		pinfo->ir_parse = ambarella_ir_panasonic_parse;
		break;
	case AMBA_IR_PROTOCOL_SONY:
		pinfo->ir_parse = ambarella_ir_sony_parse;
		break;
	case AMBA_IR_PROTOCOL_PHILIPS:
		pinfo->ir_parse = ambarella_ir_philips_parse;
		break;
	default:
		pinfo->ir_parse = ambarella_ir_nec_parse;
		break;
	}
}

void ambarella_ir_init(struct ambarella_ir_info *pinfo)
{
	ambarella_ir_disable(pinfo);

	rct_set_ir_pll();

	ambarella_gpio_config(pinfo->gpio_id, GPIO_FUNC_HW);

	ambarella_ir_set_protocol(pinfo, ir_protocol);

	ambarella_ir_enable(pinfo);
}

void ambarella_scan_adc(struct work_struct *work)
{
	int				i;
	struct ambarella_ir_info	*pinfo;
	int				adc_index;
	u32				adc_data[ADC_MAX_INSTANCES];
	u32				counter = 0;

	pinfo = container_of(work, struct ambarella_ir_info, detect_adc.work);

	if (!pinfo->pkeymap)
		return;

	memset(adc_data, 0x00, sizeof(adc_data));
	while ((__raw_readl(ADC_CONTROL_REG) & ADC_CONTROL_STATUS) == 0) {
		counter++;
		if (counter > CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT) {
			ambi_warn("ambarella_scan_adc "
				"ADC_CONTROL_STATUS Timeout!\n");
			goto ambarella_scan_adc_exit;
		}
	};
	adc_data[0] = amba_readl(ADC_DATA0_REG);
	adc_data[1] = amba_readl(ADC_DATA1_REG);
	adc_data[2] = amba_readl(ADC_DATA2_REG);
	adc_data[3] = amba_readl(ADC_DATA3_REG);
	ambi_dbg("adc0 = 0x%03x\t""adc1 = 0x%03x\t"
			"adc2 = 0x%03x\t""adc3 = 0x%03x\n",
			adc_data[0], adc_data[1], adc_data[2], adc_data[3]);
	if (ADC_MAX_INSTANCES == 8) {
		adc_data[4] = amba_readl(ADC_DATA4_REG);
		adc_data[5] = amba_readl(ADC_DATA5_REG);
		adc_data[6] = amba_readl(ADC_DATA6_REG);
		adc_data[7] = amba_readl(ADC_DATA7_REG);
		ambi_dbg("adc4 = 0x%03x\t""adc5 = 0x%03x\t"
				"adc6 = 0x%03x\t""adc7 = 0x%03x\n",
				adc_data[4], adc_data[5],
				adc_data[6], adc_data[7]);
	}

	for (i = 0; i < CONFIG_AMBARELLA_INPUT_SIZE; i++) {
		if (pinfo->pkeymap[i].type == AMBA_INPUT_END)
			break;

		if ((pinfo->pkeymap[i].type & AMBA_INPUT_SOURCE_MASK) !=
			AMBA_INPUT_SOURCE_ADC)
			continue;

		adc_index = pinfo->pkeymap[i].adc_key.chan;
		if (adc_index >= ADC_MAX_INSTANCES)
			break;

		if ((pinfo->pkeymap[i].adc_key.low_level >
			adc_data[adc_index]) ||
			(pinfo->pkeymap[i].adc_key.high_level <
			adc_data[adc_index]))
			continue;

		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_KEY) {
			if (pinfo->last_adc_key[adc_index] != KEY_RESERVED) {
				ambarella_report_key(pinfo,
					pinfo->last_adc_key[adc_index], 0);
				ambi_dbg("Key %d release & %d\n",
					pinfo->last_adc_key[adc_index],
					adc_data[adc_index]);
			}
			pinfo->last_adc_key[adc_index] = 
				pinfo->pkeymap[i].adc_key.key_code;
			if (pinfo->last_adc_key[adc_index] != KEY_RESERVED) {
				ambarella_report_key(pinfo,
					pinfo->last_adc_key[adc_index], 1);
				ambi_dbg("Key %d press & %d\n",
					pinfo->last_adc_key[adc_index],
					adc_data[adc_index]);
				break;
			}
		}

		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_REL) {
			if (pinfo->pkeymap[i].adc_rel.key_code == REL_X) {
				input_report_rel(pinfo->dev,
					REL_X,
					pinfo->pkeymap[i].adc_rel.rel_step);
				input_report_rel(pinfo->dev,
					REL_Y, 0);
				AMBI_MUST_SYNC();
				ambi_dbg("report REL_X %d & %d\n",
					pinfo->pkeymap[i].adc_rel.rel_step,
					adc_data[adc_index]);
				break;
			} else
			if (pinfo->pkeymap[i].adc_rel.key_code == REL_Y) {
				input_report_rel(pinfo->dev,
					REL_X, 0);
				input_report_rel(pinfo->dev,
					REL_Y,
					pinfo->pkeymap[i].adc_rel.rel_step);
				AMBI_MUST_SYNC();
				ambi_dbg("report REL_Y %d & %d\n",
					pinfo->pkeymap[i].adc_rel.rel_step,
					adc_data[adc_index]);
				break;
			}
		}

		if (pinfo->pkeymap[i].type == AMBA_INPUT_ADC_REL) {
			input_report_abs(pinfo->dev,
				ABS_X, pinfo->pkeymap[i].adc_abs.abs_x);
			input_report_abs(pinfo->dev,
				ABS_Y, pinfo->pkeymap[i].adc_abs.abs_y);
			AMBI_MUST_SYNC();
			ambi_dbg("report ABS %d:%d & %d\n",
				pinfo->pkeymap[i].adc_abs.abs_x,
				pinfo->pkeymap[i].adc_abs.abs_y,
				adc_data[adc_index]);
			break;
		}
	}

ambarella_scan_adc_exit:
	if (pinfo->init_adc == 0) {
		queue_delayed_work(pinfo->workqueue,
			&pinfo->detect_adc, adc_scan_delay);
	} else {
		amba_writel(ADC_RESET_REG, 0x01);
		amba_writel(ADC_ENABLE_REG, 0x00);
	}
}

static int ambarella_vi_proc_write(struct file *file,
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

static int __devinit ambarella_ir_probe(struct platform_device *pdev)
{
	int				errorCode;
	struct ambarella_ir_info	*pinfo;
	struct resource 		*irq;
	struct resource 		*mem;
	struct resource 		*io;
	struct resource 		*ioarea;
	struct input_dev		*input_dev;
	struct proc_dir_entry		*input_file;

	pinfo = kmalloc(sizeof(struct ambarella_ir_info), GFP_KERNEL);
	if (!pinfo) {
		dev_err(&pdev->dev, "Failed to allocate pinfo!\n");
		errorCode = -ENOMEM;
		goto ir_errorCode_na;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&pdev->dev, "Failed to allocate input_dev!\n");
		errorCode = -ENOMEM;
		goto ir_errorCode_pinfo;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "Get mem resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_free_input_dev;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq == NULL) {
		dev_err(&pdev->dev, "Get irq resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_free_input_dev;
	}

	io = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (io == NULL) {
		dev_err(&pdev->dev, "Get GPIO resource failed!\n");
		errorCode = -ENXIO;
		goto ir_errorCode_free_input_dev;
	}

	ioarea = request_mem_region(mem->start,
			(mem->end - mem->start) + 1, pdev->name);
	if (ioarea == NULL) {
		dev_err(&pdev->dev, "Request ioarea failed!\n");
		errorCode = -EBUSY;
		goto ir_errorCode_free_input_dev;
	}

	snprintf(pinfo->name, CONFIG_AMBARELLA_INPUT_STR_SIZE,
		"%sd", pdev->name);
	input_dev->name = pinfo->name;
	snprintf(pinfo->phys, CONFIG_AMBARELLA_INPUT_STR_SIZE,
		"ambarella/input%d", pdev->id);
	input_dev->phys = pinfo->phys;
	snprintf(pinfo->uniq, CONFIG_AMBARELLA_INPUT_STR_SIZE,
		"ambarella input input%d", pdev->id);
	input_dev->uniq = pinfo->uniq;

	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = MACH_TYPE_AMBARELLA;
	input_dev->id.product = AMBARELLA_BOARD_CHIP(system_rev);
	input_dev->id.version = AMBARELLA_BOARD_REV(system_rev);
	input_dev->dev.parent = &pdev->dev;

	pinfo->regbase = (unsigned char __iomem *)mem->start;
	pinfo->id = pdev->id;
	pinfo->dev = input_dev;
	pinfo->mem = mem;
	pinfo->irq = irq->start;
	pinfo->gpio_id = io->start;
	pinfo->init_adc = 1;
	memset(pinfo->last_adc_key, KEY_RESERVED, sizeof(pinfo->last_adc_key));
	pinfo->last_ir_uid = 0;
	pinfo->last_ir_flag = 0;

	pinfo->workqueue = create_singlethread_workqueue(pinfo->name);
	if (!pinfo->workqueue) {
		errorCode = -ENOMEM;
		dev_err(&pdev->dev, "Setup key map failed!\n");
		goto ir_errorCode_free_input_dev;
	}
	INIT_DELAYED_WORK(&pinfo->detect_adc, ambarella_scan_adc);

	platform_set_drvdata(pdev, pinfo);

	errorCode = ambarella_setup_keymap(pinfo);
	if (errorCode) {
		dev_err(&pdev->dev, "Setup key map failed!\n");
		goto ir_errorCode_free_platform;
	}

	errorCode = request_irq(pinfo->irq,
		ambarella_ir_irq, IRQF_DISABLED | IRQF_TRIGGER_HIGH,
		pdev->name, pinfo);
	if (errorCode) {
		dev_err(&pdev->dev, "Request IRQ failed!\n");
		goto ir_errorCode_free_keymap;
	}

	ambarella_ir_init(pinfo);

	errorCode = input_register_device(input_dev);
	if (errorCode) {
		dev_err(&pdev->dev, "Register input_dev failed!\n");
		goto ir_errorCode_free_irq;
	}

	input_file = create_proc_entry(dev_name(&pdev->dev),
		S_IRUGO | S_IWUSR, get_ambarella_proc_dir());
	if (input_file == NULL) {
		dev_err(&pdev->dev, "Register %s failed!\n",
			dev_name(&pdev->dev));
		errorCode = -ENOMEM;
		goto ir_errorCode_unregister_device;
	} else {
		input_file->write_proc = ambarella_vi_proc_write;
		input_file->owner = THIS_MODULE;
		input_file->data = pinfo;
	}

	dev_notice(&pdev->dev,
		"Ambarella Media Processor IR Host Controller %d probed!\n",
		pdev->id);

	goto ir_errorCode_na;

ir_errorCode_unregister_device:
	input_unregister_device(pinfo->dev);

ir_errorCode_free_irq:
	free_irq(pinfo->irq, pinfo);

ir_errorCode_free_keymap:
	ambarella_free_keymap(pinfo);

ir_errorCode_free_platform:
	platform_set_drvdata(pdev, NULL);

ir_errorCode_free_input_dev:
	input_free_device(input_dev);
	destroy_workqueue(pinfo->workqueue);
	release_mem_region(mem->start, (mem->end - mem->start) + 1);

ir_errorCode_pinfo:
	kfree(pinfo);

ir_errorCode_na:
	return errorCode;
}

static int __devexit ambarella_ir_remove(struct platform_device *pdev)
{
	struct ambarella_ir_info	*pinfo;
	int				errorCode = 0;

	pinfo = platform_get_drvdata(pdev);

	if (pinfo) {
		remove_proc_entry(dev_name(&pdev->dev),
			get_ambarella_proc_dir());
		input_unregister_device(pinfo->dev);
		free_irq(pinfo->irq, pinfo);
		ambarella_free_keymap(pinfo);
		platform_set_drvdata(pdev, NULL);
		input_free_device(pinfo->dev);
		destroy_workqueue(pinfo->workqueue);
		release_mem_region(pinfo->mem->start,
			(pinfo->mem->end - pinfo->mem->start) + 1);
		kfree(pinfo);
	}

	dev_notice(&pdev->dev,
		"Remove Ambarella Media Processor IR Host Controller.\n");

	return errorCode;
}

#ifdef CONFIG_PM
static int ambarella_ir_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	int					errorCode = 0;

	dev_info(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);
	return errorCode;
}

static int ambarella_ir_resume(struct platform_device *pdev)
{
	int					errorCode = 0;

	dev_info(&pdev->dev, "%s exit with %d\n", __func__, errorCode);

	return errorCode;
}
#endif

static struct platform_driver ambarella_ir_driver = {
	.probe		= ambarella_ir_probe,
	.remove		= __devexit_p(ambarella_ir_remove),
#ifdef CONFIG_PM
	.suspend	= ambarella_ir_suspend,
	.resume		= ambarella_ir_resume,
#endif
	.driver		= {
		.name	= "ambarella-ir",
		.owner	= THIS_MODULE,
	},
};

static int ambarella_input_register(void)
{
	int				errorCode = 0;

	errorCode = platform_driver_register(&ambarella_ir_driver);
	if (errorCode)
		printk(KERN_ERR "Register ambarella_ir_driver failed!\n");

	return errorCode;
}

static void ambarella_input_unregister(void)
{
	platform_driver_unregister(&ambarella_ir_driver);
}

static int ambarella_input_set_enable(const char *val, struct kernel_param *kp)
{
	int				errorCode = 0;
	int				tmp = enable_input;

	errorCode = param_set_int(val, kp);
	if (enable_input)
		enable_input = 1;
	if ((tmp == 0) && (enable_input == 1)) {
		errorCode = ambarella_input_register();
	} else
	if ((tmp == 1) && (enable_input == 0)) {
		ambarella_input_unregister();
	}

	return errorCode;
}

static int __init ambarella_input_init(void)
{
	int				errorCode = 0;

	if (enable_input) {
		enable_input = 1;
		errorCode = ambarella_input_register();
	}

	return errorCode;
}

static void __exit ambarella_input_exit(void)
{
	if (enable_input)
		ambarella_input_unregister();
}

module_init(ambarella_input_init);
module_exit(ambarella_input_exit);

MODULE_DESCRIPTION("Ambarella Media Processor input driver");
MODULE_AUTHOR("Anthony Ginger, <hfjiang@ambarella.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ambai");

module_param(ir_protocol, int, 0644);
module_param(print_keycode, int, 0644);
module_param(abx_max_x, int, 0644);
module_param(abx_max_y, int, 0644);
module_param(abx_max_pressure, int, 0644);
module_param(abx_max_width, int, 0644);
module_param(adc_scan_delay, int, 0644);
module_param(keymap_name, charp, 0644);
module_param_call(enable_input, ambarella_input_set_enable,
	param_get_int, &enable_input, 0644);

