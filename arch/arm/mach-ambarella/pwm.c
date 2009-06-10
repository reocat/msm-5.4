/*
 * arch/arm/mach-ambarella/adc.c.c
 *
 * Author: Jay Zhang, <jzhang@ambarella.com>
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

#define PWM_CMD_SIZE		(5)
#define PWM_ARRAY_SIZE		(PWM_MAX_INSTANCES * PWM_CMD_SIZE)

#ifdef CONFIG_AMBARELLA_PWM_PROC
static u32 pwm_array[PWM_ARRAY_SIZE];
static const char pwm_proc_name[] = "pwm";
static struct proc_dir_entry *pwm_file;

static int ambarella_pwm_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					errorCode = 0;
	int					cmd_cnt;
	int					i;
	u32					pwm_ch;
	u32					enable;
	u32					xon;
	u32					xoff;
	u32					div;
	u32					data_reg;
	
	if (count > sizeof(pwm_array)) {
		pr_err("%s: count %d out of size!\n", __func__, (u32)count);
		errorCode = -ENOSPC;
		goto ambarella_pwm_proc_write_exit;
	}

	if (copy_from_user(pwm_array, buffer, count)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto ambarella_pwm_proc_write_exit;
	}

	cmd_cnt = count / (PWM_CMD_SIZE * sizeof(pwm_array[0]));
	for (i = 0; i < cmd_cnt; i++) {
		pwm_ch = pwm_array[i * 3];
		enable = pwm_array[i * 3 + 1];
		xon = pwm_array[i * 3 + 2];
		xoff = pwm_array[i * 3 + 3];
		div = pwm_array[i * 3 + 4];

		if (pwm_ch == 0)
			data_reg = ((xon - 1) << 16) + (xoff - 1);
		else
			data_reg = ((xon - 1) << 10) + (xoff - 1);

		switch (pwm_ch) {
		case 0:
			amba_writel(PWM_CONTROL_REG, data_reg);
			amba_writel(PWM_ENABLE_REG, enable);
			amba_writel(PWM_MODE_REG, div << 1);
			break;

		case 1:
			amba_writel(PWM_B0_DATA1_REG, data_reg);
			amba_writel(PWM_B0_ENABLE_REG, enable + (div << 1));
			break;

		case 2:
			amba_writel(PWM_B1_DATA1_REG, data_reg);
			amba_writel(PWM_B1_ENABLE_REG, enable + (div << 1));
			break;

		case 3:
			amba_writel(PWM_C0_DATA1_REG, data_reg);
			amba_writel(PWM_C0_ENABLE_REG, enable + (div << 1));
			break;

		case 4:
			amba_writel(PWM_C1_DATA1_REG, data_reg);
			amba_writel(PWM_C1_ENABLE_REG, enable + (div << 1));
			break;

		default:
			pr_warning("%s: invalid pwm channel id %d!\n",
				__func__, pwm_ch);
			break;
		}
	}

	errorCode = count;

ambarella_pwm_proc_write_exit:
	return errorCode;
}
#endif

int __init ambarella_init_pwm(void)
{
	int					errorCode = 0;

#ifdef CONFIG_AMBARELLA_PWM_PROC
	pwm_file = create_proc_entry(pwm_proc_name, S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (pwm_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: %s fail!\n", __func__, pwm_proc_name);
	} else {
		pwm_file->read_proc = NULL;
		pwm_file->write_proc = ambarella_pwm_proc_write;
		pwm_file->owner = THIS_MODULE;
	}
#endif

	return errorCode;
}

