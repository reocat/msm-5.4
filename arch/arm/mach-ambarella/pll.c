/*
 * arch/arm/mach-ambarella/pll.c
 *
 * Author: Cao Rongrong, <rrcao@ambarella.com>
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

#define MAX_CMD_LENGTH	128

static struct srcu_notifier_head pll_notifier_list;

struct proc_dir_entry *pll_dir = NULL;
struct proc_dir_entry *freq_file = NULL;
struct proc_dir_entry *mode_file = NULL;
struct proc_dir_entry *performance_file = NULL;

#if (CHIP_REV == A5S)

static char *mode_str[] = {
	"preview",
	"still_capture",
	"capture",
	"playback",
	"display_and_arm",
	"standby",
	"lcd_bypass",
};

static char *performance_str[] = {
	"480P30",
	"720P30",
	"720P60",
	"1080I60",
	"1080P30",
	"1080P60",
	"2160P60",
};

struct ambarella_pll_info {
	amb_operating_mode_t operating_mode;
};

static int ambarella_pll_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	amb_hal_success_t result;
	amb_operating_mode_t operating_mode;
	int retlen;

	if (off != 0)
		return 0;

	result = amb_get_operating_mode(HAL_BASE_VP, &operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: get operating mode failed(%d)\n",__func__, result);
		return -EPERM;
	}

	retlen = scnprintf(page, count,
			"\nPLL Information:\n"
			"\tMode: %s\n"
			"\tPerformance: %s\n"			
			"\tARM frequency: %d Hz\n"
			"\tCore frequency: %d Hz\n"
			"\tDram frequency: %d Hz\n"
			"\tiDSP frequency: %d Hz\n\n",
			mode_str[operating_mode.mode],
			performance_str[operating_mode.performance],
			get_arm_bus_freq_hz(),
			get_core_bus_freq_hz(),
			get_dram_freq_hz(),
			get_idsp_freq_hz());

	*eof = 1;

	return retlen;
}

static int ambarella_mode_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	amb_hal_success_t result;
	struct ambarella_pll_info *pll_info;
	char str[MAX_CMD_LENGTH];
	int errorCode = 0, mode;

	pll_info = (struct ambarella_pll_info *)data;

	if (copy_from_user(str, buffer, count)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto pll_mode_proc_write_exit;
	}

	errorCode = sscanf(str, "%d", &mode);
	if(errorCode != 1 || mode > AMB_OPERATING_MODE_LCD_BYPASS){
		pr_err("%s: invalid mode (%d)!\n", __func__, errorCode);
		errorCode = -EINVAL;
		goto pll_mode_proc_write_exit;
	}

	errorCode = count;

	pll_info->operating_mode.mode = mode;
	result = amb_set_operating_mode(HAL_BASE_VP, &pll_info->operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: set operating mode failed(%d)\n",__func__, result);
		errorCode = -EPERM;
		goto pll_mode_proc_write_exit;
	}

	pr_debug("%s: mode (%d)\n", __func__, mode);

pll_mode_proc_write_exit:
	return errorCode;
}

static int ambarella_performance_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	amb_hal_success_t result;
	struct ambarella_pll_info *pll_info;
	char str[MAX_CMD_LENGTH];
	int errorCode = 0, performance;

	pll_info = (struct ambarella_pll_info *)data;

	if (copy_from_user(str, buffer, count)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto pll_performance_proc_write_exit;
	}

	errorCode = sscanf(str, "%d", &performance);
	if(errorCode != 1 || performance > AMB_PERFORMANCE_2160P60){
		pr_err("%s: invalid performance (%d)!\n", __func__, errorCode);
		errorCode = -EINVAL;
		goto pll_performance_proc_write_exit;
	}

	errorCode = count;

	pll_info->operating_mode.performance = performance;
	result = amb_set_operating_mode(HAL_BASE_VP, &pll_info->operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: set operating mode failed(%d)\n",__func__, result);
		errorCode = -EPERM;
		goto pll_performance_proc_write_exit;
	}

	pr_debug("%s: performance (%d)\n", __func__, performance);

pll_performance_proc_write_exit:
	return errorCode;
}


static int ambarella_init_pll_a5s(void)
{
	amb_hal_success_t			result;
	struct ambarella_pll_info		*pll_info;
	int					errorCode = 0;

	/* initial HAL */
	result = amb_hal_init(HAL_BASE_VP, (void *)APB_BASE, (void *)AHB_BASE);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: initial HAL failed (%d)\n", __func__, result);
		errorCode = -ENOMEM;
		goto pll_a5s_exit;
	}

	pll_info = kmalloc(sizeof(struct ambarella_pll_info), GFP_KERNEL);
	if(!pll_info){
		pr_err("%s: No memory for pll_info\n", __func__);
		goto pll_a5s_exit;
	}

	/* initial pll_info */
	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info->operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: get operating mode failed(%d)\n",__func__, result);
		errorCode = -EPERM;
		goto pll_a5s_exit;
	}

#ifdef CONFIG_AMBARELLA_PLL_PROC

	pll_dir = proc_mkdir("pll", get_ambarella_proc_dir());
	if (!pll_dir){
		errorCode = -ENOMEM;
		pr_err("%s: create pll dir fail!\n", __func__);
		goto pll_a5s_exit;
	}

	mode_file = create_proc_entry("mode", S_IRUGO | S_IWUSR, pll_dir);
	if (mode_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: create proc file (mode) fail!\n", __func__);
		goto pll_a5s_exit;
	} else {
		mode_file->read_proc = ambarella_pll_proc_read;
		mode_file->write_proc = ambarella_mode_proc_write;
		mode_file->data = pll_info;
		mode_file->owner = THIS_MODULE;
	}

	performance_file = create_proc_entry("performance",
		S_IRUGO | S_IWUSR, pll_dir);
	if (performance_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: create proc file (performance) fail!\n", __func__);
		goto pll_a5s_exit;
	} else {
		performance_file->read_proc = ambarella_pll_proc_read;
		performance_file->write_proc = ambarella_performance_proc_write;
		performance_file->data = pll_info;
		performance_file->owner = THIS_MODULE;
	}

#endif

pll_a5s_exit:
	return errorCode;
}


#else

struct ambarella_pll_info {
	unsigned int armfreq;
};


#ifdef CONFIG_AMBARELLA_PLL_PROC

static inline unsigned long ambarella_cpufreq_scale(unsigned long old,
	u_int div, u_int mult)
{
#if BITS_PER_LONG == 32

	u64 result = ((u64) old) * ((u64) mult);
	do_div(result, div);
	return (unsigned long) result;

#elif BITS_PER_LONG == 64

	unsigned long result = old * ((u64) mult);
	result /= div;
	return result;

#endif
};

static unsigned long l_p_j_ref;
static unsigned int  l_p_j_ref_freq;

static void ambarella_adjust_jiffies(unsigned long val,
	unsigned int oldfreq, unsigned int newfreq)
{
	if (!l_p_j_ref_freq) {
		l_p_j_ref = loops_per_jiffy;
		l_p_j_ref_freq = oldfreq;
	}
	if ((val == AMB_CPUFREQ_PRECHANGE  && oldfreq < newfreq) ||
	    (val == AMB_CPUFREQ_POSTCHANGE && oldfreq > newfreq)) {
		loops_per_jiffy = ambarella_cpufreq_scale(l_p_j_ref,
			l_p_j_ref_freq, newfreq);
	}
}

static int ambarella_freq_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	int retlen;

	if (off != 0)
		return 0;

	retlen = scnprintf(page, count,
			"\nPLL Information:\n"
			"\tCore frequency: %d Hz\n"
			"\tDram frequency: %d Hz\n"
			"\tiDSP frequency: %d Hz\n\n",
			get_core_bus_freq_hz(),
			get_dram_freq_hz(),
			get_idsp_freq_hz());

	*eof = 1;

	return retlen;
}

static int ambarella_freq_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	struct ambarella_pll_info *pll_info;
	char str[MAX_CMD_LENGTH];
	int errorCode = 0;
	unsigned long flags;
	unsigned int new_freq_cpu, cur_freq_cpu;

	pll_info = (struct ambarella_pll_info *)data;

	if (copy_from_user(str, buffer, count)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto ambarella_pll_proc_write_exit;
	}

	errorCode = sscanf(str, "%d", &new_freq_cpu);
	if(errorCode != 1){
		pr_err("%s: convert sting fail!\n", __func__);
		errorCode = -EINVAL;
		goto ambarella_pll_proc_write_exit;
	}

	if(new_freq_cpu > 243000 || new_freq_cpu < 135000){
		pr_err("%s:\n\tinvalid frequency (%d)\n", __func__, new_freq_cpu);
		pr_info("\tfrequency should be 135000 ~ 243000 (in KHz)\n");
		errorCode = -EINVAL;
		goto ambarella_pll_proc_write_exit;
	}

	errorCode = count;

	pr_debug("%s: %ld %d\n", __func__, count, new_freq_cpu);

	if(new_freq_cpu == pll_info->armfreq)
		goto ambarella_pll_proc_write_exit;

	/* Tell everyone what we're about to do... */
	fio_select_lock(SELECT_FIO_HOLD, 1);	/* Hold the FIO bus first */
	srcu_notifier_call_chain(&pll_notifier_list,
		AMB_CPUFREQ_PRECHANGE, &new_freq_cpu);
	ambarella_adjust_jiffies(AMB_CPUFREQ_PRECHANGE,
		pll_info->armfreq, new_freq_cpu);

	local_irq_save(flags);

#if ((CHIP_REV == A2) || (CHIP_REV == A3))

	do {
		cur_freq_cpu = amba_readl(PLL_CORE_CTRL_REG) & 0xfff00000;
		if (new_freq_cpu > pll_info->armfreq) {
			cur_freq_cpu += 0x01000000;
		} else {
			cur_freq_cpu -= 0x01000000;
		}
		cur_freq_cpu |= (amba_readl(PLL_CORE_CTRL_REG) & 0x000fffff);
		amba_writel(PLL_CORE_CTRL_REG, cur_freq_cpu);
		mdelay(20);
		cur_freq_cpu = get_core_bus_freq_hz() / 1000;	/* in Khz */
	} while ((new_freq_cpu > pll_info->armfreq && cur_freq_cpu < new_freq_cpu) ||
		 (new_freq_cpu < pll_info->armfreq && cur_freq_cpu > new_freq_cpu));

#elif ((CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) || (CHIP_REV == A5) || (CHIP_REV == A6))

	do {
		u32 reg, intprog, sdiv, sout, valwe;

		reg = amba_readl(PLL_CORE_CTRL_REG);
		intprog = PLL_CTRL_INTPROG(reg);
		sout = PLL_CTRL_SOUT(reg);
		sdiv = PLL_CTRL_SDIV(reg);
		valwe = PLL_CTRL_VALWE(reg);
		valwe &= 0xffe;
		if (new_freq_cpu > pll_info->armfreq)
			intprog++;
		else
			intprog--;

		reg = PLL_CTRL_VAL(intprog, sout, sdiv, valwe);
		amba_writel(PLL_CORE_CTRL_REG, reg);

		/* PLL write enable */
		reg |= 0x1;
		amba_writel(PLL_CORE_CTRL_REG, reg);

		/* FIXME: wait a while */
		mdelay(20);
		cur_freq_cpu = get_core_bus_freq_hz() / 1000;	/* in Khz */
	} while ((new_freq_cpu > pll_info->armfreq && cur_freq_cpu < new_freq_cpu) ||
		(new_freq_cpu < pll_info->armfreq && cur_freq_cpu > new_freq_cpu));
#endif

	local_irq_restore(flags);

	ambarella_adjust_jiffies(AMB_CPUFREQ_POSTCHANGE,
		pll_info->armfreq, new_freq_cpu);
	/* Tell everyone what we've just done...
	  * you should add a notify client with any platform specific SDRAM
	  * refresh timer adjustments. */
	srcu_notifier_call_chain(&pll_notifier_list,
				AMB_CPUFREQ_POSTCHANGE, &new_freq_cpu);
	fio_unlock(SELECT_FIO_HOLD, 1);	/* Release the FIO bus */

	pll_info->armfreq = get_core_bus_freq_hz() / 1000;

ambarella_pll_proc_write_exit:
	return errorCode;
}

#endif  /* End for #ifdef CONFIG_AMBARELLA_PLL_PROC */

static int ambarella_init_pll_general(void)
{
	int					errorCode = 0;
	struct ambarella_pll_info		*pll_info;

	pll_info = kmalloc(sizeof(struct ambarella_pll_info), GFP_KERNEL);
	if(!pll_info){
		pr_err("%s: No memory for pll_info\n", __func__);
		errorCode = -ENOMEM;
		goto pll_general_exit;
	}

	/* initial pll_info */
	pll_info->armfreq = get_core_bus_freq_hz() / 1000;

#ifdef CONFIG_AMBARELLA_PLL_PROC
	pll_dir = proc_mkdir("pll", get_ambarella_proc_dir());
	if (!pll_dir){
		errorCode = -ENOMEM;
		pr_err("%s: create pll dir fail!\n", __func__);
		goto pll_general_exit;
	}

	freq_file = create_proc_entry("frequency", S_IRUGO | S_IWUSR, pll_dir);
	if (freq_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: create proc file (freq) fail!\n", __func__);
		goto pll_general_exit;
	} else {
		freq_file->read_proc = ambarella_freq_proc_read;
		freq_file->write_proc = ambarella_freq_proc_write;
		freq_file->data = pll_info;
		freq_file->owner = THIS_MODULE;
	}
#endif

pll_general_exit:
	return errorCode;
}

#endif	/* End for #if (CHIP_REV == A5S) */


int ambarella_register_freqnotifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&pll_notifier_list, nb);
}
EXPORT_SYMBOL(ambarella_register_freqnotifier);

int ambarella_unregister_freqnotifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&pll_notifier_list, nb);
}
EXPORT_SYMBOL(ambarella_unregister_freqnotifier);


int __init ambarella_init_pll(void)
{
	int					errorCode = 0;

	srcu_init_notifier_head(&pll_notifier_list);

#if (CHIP_REV == A5S)
	errorCode = ambarella_init_pll_a5s();
#else
	errorCode = ambarella_init_pll_general();
#endif

	return errorCode;
}


