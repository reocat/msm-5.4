/*
 * arch/arm/mach-ambarella/adc.c
 *
 * Author: Anthony Ginger <hfjiang@ambarella.com>
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
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>

#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include <mach/hardware.h>

#ifndef CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT
#define CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT	(100000)
#endif

u32 ambarella_adc_get_instances(void)
{
	return ADC_NUM_CHANNELS;
}
EXPORT_SYMBOL(ambarella_adc_get_instances);

static inline u32 ambarella_adc_get_channel_inline(u32 channel_id)
{
	u32					adc_data = 0;

	switch(channel_id) {
	case 0:
		adc_data = amba_readl(ADC_DATA0_REG);
		break;
	case 1:
		adc_data = amba_readl(ADC_DATA1_REG);
		break;
	case 2:
		adc_data = amba_readl(ADC_DATA2_REG);
		break;
	case 3:
		adc_data = amba_readl(ADC_DATA3_REG);
		break;
#if (ADC_NUM_CHANNELS == 8)
	case 4:
		adc_data = amba_readl(ADC_DATA4_REG);
		break;
	case 5:
		adc_data = amba_readl(ADC_DATA5_REG);
		break;
	case 6:
		adc_data = amba_readl(ADC_DATA6_REG);
		break;
	case 7:
		adc_data = amba_readl(ADC_DATA7_REG);
		break;
#endif
	default:
		pr_warning("%s: invalid adc channel id %d!\n",
			__func__, channel_id);
		break;
	}

	return adc_data;
}

void ambarella_adc_get_array(u32 *adc_data, u32 *array_size)
{
	int					i;
	u32					counter = 0;

	if (unlikely(*array_size > ADC_NUM_CHANNELS)) {
		pr_warning("%s: Limit array_size form %d to %d!\n",
			__func__, *array_size, ADC_NUM_CHANNELS);
		*array_size = ADC_NUM_CHANNELS;
	}
	while ((amba_readl(ADC_CONTROL_REG) & ADC_CONTROL_STATUS) == 0) {
		counter++;
		if (counter > CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT) {
			pr_warning("%s: Wait ADC_CONTROL_STATUS timeout!\n"
				"Try: echo 1 > /proc/ambarella/adc\n",
				__func__);
			return;
		}
	};
	for (i = 0; i < *array_size; i++)
		adc_data[i] = ambarella_adc_get_channel_inline(i);
}

u32 ambarella_adc_get_channel(u32 channel_id)
{
	u32					adc_data = 0;
	u32					counter = 0;

	while ((amba_readl(ADC_CONTROL_REG) & ADC_CONTROL_STATUS) == 0) {
		counter++;
		if (counter > CONFIG_AMBARELLA_ADC_WAIT_COUNTER_LIMIT) {
			pr_warning("%s: Wait ADC_CONTROL_STATUS timeout!\n",
				__func__);
			goto ambarella_adc_get_channel_exit;
		}
	};
	adc_data = ambarella_adc_get_channel_inline(channel_id);

ambarella_adc_get_channel_exit:
	return adc_data;
}

void ambarella_adc_start(void)
{
	u32				counter;

#if (CHIP_REV != A5S)
	amba_writel(ADC_RESET_REG, 0x01);
#endif
	amba_writel(ADC_ENABLE_REG, 0x01);
	msleep(3);

	amba_writel(ADC_CONTROL_REG, 0);
	counter = 0;
	while (amba_tstbitsl(ADC_CONTROL_REG, ADC_CONTROL_STATUS) == 0x0) {
		counter++;
		if (counter > 10000)
			break;
	}

	amba_writel(ADC_CONTROL_REG, ADC_CONTROL_START | ADC_CONTROL_MODE);
	counter = 0;
	while (amba_tstbitsl(ADC_CONTROL_REG, ADC_CONTROL_STATUS) == 0x0) {
		counter++;
		if (counter > 10000)
			break;
	}
}

void ambarella_adc_stop(void)
{
#if (CHIP_REV != A5S)
	amba_writel(ADC_RESET_REG, 0x01);
#endif
	amba_writel(ADC_ENABLE_REG, 0x00);
}

#ifdef CONFIG_AMBARELLA_ADC_PROC
#define AMBARELLA_ADC_PROC_READ_SIZE		(13)
static const char adc_proc_name[] = "adc";
static struct proc_dir_entry *adc_file;

static int ambarella_adc_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					read_counter = 0;
	char					cmd;

	if (copy_from_user(&cmd, buffer, 1)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		read_counter = -EFAULT;
		goto ambarella_adc_proc_write_exit;
	}
	read_counter = count;

	if (cmd == '1') {
		ambarella_adc_start();
	} else {
		ambarella_adc_stop();
	}

ambarella_adc_proc_write_exit:
	return read_counter;
}

static int ambarella_adc_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	int					len = 0;
	int					i;
	u32					adc_data[ADC_NUM_CHANNELS];
	int					adc_size;

	adc_size = ambarella_adc_get_instances();

	if (off > (adc_size * AMBARELLA_ADC_PROC_READ_SIZE)) {
		*eof = 1;
		return 0;
	}

	*start = page + off;

	if (count > (adc_size * AMBARELLA_ADC_PROC_READ_SIZE)) {
		count = (adc_size * AMBARELLA_ADC_PROC_READ_SIZE);
		*eof = 1;
	}

	if ((off + count) > (adc_size * AMBARELLA_ADC_PROC_READ_SIZE)) {
		count = (adc_size * AMBARELLA_ADC_PROC_READ_SIZE) - off;
		*eof = 1;
	}

	adc_size = count / AMBARELLA_ADC_PROC_READ_SIZE;
	ambarella_adc_get_array(adc_data, &adc_size);
	for (i = off / AMBARELLA_ADC_PROC_READ_SIZE; i < adc_size; i++)
		len += sprintf(*start + len, "adc%d = 0x%03x\n",
			i, adc_data[i]);

	return len;
}
#endif

int __init ambarella_init_adc(void)
{
	int					errorCode = 0;

#ifdef CONFIG_AMBARELLA_ADC_PROC
	adc_file = create_proc_entry(adc_proc_name, S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (adc_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: %s fail!\n", __func__, adc_proc_name);
	} else {
		adc_file->read_proc = ambarella_adc_proc_read;
		adc_file->write_proc = ambarella_adc_proc_write;
		adc_file->owner = THIS_MODULE;
	}
#endif

	return errorCode;
}

u32 adc_is_irq_supported(void)
{
	#if (CHIP_REV == A5S)
		return 1;
	#else
		return 0;
	#endif
}


/*
 * set the related channal's interrupt trigger and threshold
 */
void adc_set_irq_threshold(u32 ch, u32 h_level,u32 l_level)
{
#if (CHIP_REV == A5S)
	u32 irq_control_address = 0;
	u32 value = ((!!h_level)<<31) | ((!!l_level)<<30)
		| ((h_level&0x3ff)<<15) | (l_level&0x3ff);

	switch (ch) {
	case 0:
		irq_control_address = ADC_CHAN3_INTR_REG;
		break;
	case 1:
		irq_control_address = ADC_CHAN0_INTR_REG;
		break;
	case 2:
		irq_control_address = ADC_CHAN1_INTR_REG;
		break;
	case 3:
		irq_control_address = ADC_CHAN2_INTR_REG;
		break;
	default:
		printk("Don't support %d channels\n", ch);
		break;
	}
	amba_writel(irq_control_address, value);
#else
#endif
}

