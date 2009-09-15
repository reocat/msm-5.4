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

#define MAX_CMD_LENGTH				(32)

static struct srcu_notifier_head pll_notifier_list;

#if (CHIP_REV == A5S)
static struct proc_dir_entry *mode_file = NULL;
static struct proc_dir_entry *performance_file = NULL;
#else
static struct proc_dir_entry *freq_file = NULL;
#endif

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

#if (CHIP_REV == A5S)

struct ambarella_pll_info {
	amb_operating_mode_t operating_mode;
};

struct ambarella_pll_mode_info {
	char *name;
	unsigned int mode;
};

struct ambarella_pll_performance_info {
	char *name;
	unsigned int performance;
};

static struct ambarella_pll_mode_info mode_list[] = {
	{"preview", AMB_OPERATING_MODE_PREVIEW},
	{"still_capture", AMB_OPERATING_MODE_STILL_CAPTURE},
	{"capture", AMB_OPERATING_MODE_CAPTURE},
	{"playback", AMB_OPERATING_MODE_PLAYBACK},
	{"display_and_arm", AMB_OPERATING_MODE_DISPLAY_AND_ARM},
	{"standby", AMB_OPERATING_MODE_STANDBY},
//	{"lcd_bypass", AMB_OPERATING_MODE_LCD_BYPASS},
};

static struct ambarella_pll_performance_info performance_list[] = {
	{"480P30", AMB_PERFORMANCE_480P30},
	{"720P30", AMB_PERFORMANCE_720P30},
	{"720P60", AMB_PERFORMANCE_720P60},
	{"1080I60", AMB_PERFORMANCE_1080I60},
	{"1080P30", AMB_PERFORMANCE_1080P30},
	{"1080P60", AMB_PERFORMANCE_1080P60},
//	{"2160P60", AMB_PERFORMANCE_2160P60},
};

static int ambarella_pll_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	amb_hal_success_t			result;
	amb_operating_mode_t			operating_mode;
	int					retlen = 0;
	int					i;

	result = amb_get_operating_mode(HAL_BASE_VP, &operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		retlen = -EPERM;
		goto ambarella_pll_proc_read_exit;
	}

	if (off != 0) {
		retlen = 0;
		goto ambarella_pll_proc_read_exit;
	}

	retlen += scnprintf((page + retlen), (count - retlen),
			"\nPossible Mode:\n");
	for (i = 0; i < ARRAY_SIZE(mode_list); i++) {
		retlen += scnprintf((page + retlen), (count - retlen),
				"\t%s\n", mode_list[i].name);
	}

	retlen += scnprintf((page + retlen), (count - retlen),
			"\nPossible Performance:\n");
	for (i = 0; i < ARRAY_SIZE(performance_list); i++) {
		retlen += scnprintf((page + retlen), (count - retlen),
				"\t%s\n", performance_list[i].name);
	}

	retlen += scnprintf((page + retlen), (count - retlen),
			"\nPLL Information:\n"
			"\tPerformance: %s\n"
			"\tMode:\t%s\n"
			"\tARM:\t%d Hz\n"
			"\tDram:\t%d Hz\n"
			"\tiDSP:\t%d Hz\n"
			"\tCore:\t%d Hz\n"
			"\tAHB:\t%d Hz\n"
			"\tAPB:\t%d Hz\n\n",
			performance_list[operating_mode.performance].name,
			mode_list[operating_mode.mode].name,
			get_arm_bus_freq_hz(),
			get_dram_freq_hz(),
			get_idsp_freq_hz(),
			get_core_bus_freq_hz(),
			get_ahb_bus_freq_hz(),
			get_apb_bus_freq_hz());

	*eof = 1;

ambarella_pll_proc_read_exit:
	return retlen;
}

int ambarella_set_operating_mode(amb_operating_mode_t *popmode)
{
	int					errorCode = 0;
	amb_hal_success_t			result = AMB_HAL_SUCCESS;
	unsigned int				oldfreq, newfreq;
	unsigned long				flags;

	fio_select_lock(SELECT_FIO_HOLD, 1);	/* Hold the FIO bus first */

	/* Tell everyone what we're about to do... */
	srcu_notifier_call_chain(&pll_notifier_list,
		AMB_CPUFREQ_PRECHANGE, NULL);

	oldfreq = get_arm_bus_freq_hz();
	/*  FIXME: need to adjust jiffies ? */
	local_irq_save(flags);
	result = amb_set_operating_mode(HAL_BASE_VP, popmode);
	if (result != AMB_HAL_SUCCESS) {
		pr_err("%s: amb_set_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
	}
	local_irq_restore(flags);
	newfreq = get_arm_bus_freq_hz();
	ambarella_adjust_jiffies(AMB_CPUFREQ_POSTCHANGE, oldfreq, newfreq);

	/* Tell everyone what we've just done... */
	srcu_notifier_call_chain(&pll_notifier_list,
		AMB_CPUFREQ_POSTCHANGE, NULL);

	result = amb_get_operating_mode(HAL_BASE_VP, popmode);
	if (result != AMB_HAL_SUCCESS) {
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
	}

	fio_unlock(SELECT_FIO_HOLD, 1);	/* Release the FIO bus */

	return errorCode;
}
EXPORT_SYMBOL(ambarella_set_operating_mode);

static int ambarella_mode_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					errorCode = 0;
	struct ambarella_pll_info		*pll_info;
	char					str[MAX_CMD_LENGTH];
	int					mode, i;
	amb_hal_success_t			result;

	pll_info = (struct ambarella_pll_info *)data;

	i = (count < MAX_CMD_LENGTH) ? count : MAX_CMD_LENGTH;
	if (copy_from_user(str, buffer, i)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto pll_mode_proc_write_exit;
	}
	str[MAX_CMD_LENGTH - 1] = 0;

	mode = AMB_OPERATING_MODE_LCD_BYPASS + 1;
	for (i = 0; i < ARRAY_SIZE(mode_list); i++) {
		if (strstr(str, mode_list[i].name) != NULL) {
			mode = mode_list[i].mode;
		}
	}

	if (mode > AMB_OPERATING_MODE_LCD_BYPASS) {
		pr_err("%s: invalid mode (%s)!\n", __func__, str);
		errorCode = -EINVAL;
		goto pll_mode_proc_write_exit;
	}

	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info->operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
		goto pll_mode_proc_write_exit;
	}

	pll_info->operating_mode.mode = mode;
	errorCode = ambarella_set_operating_mode(&pll_info->operating_mode);
	if (!errorCode)
		errorCode = count;

pll_mode_proc_write_exit:
	return errorCode;
}

static int ambarella_performance_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	int					errorCode = 0;
	struct ambarella_pll_info		*pll_info;
	char					str[MAX_CMD_LENGTH];
	int					performance, i;
	amb_hal_success_t			result;

	pll_info = (struct ambarella_pll_info *)data;

	i = (count < MAX_CMD_LENGTH) ? count : MAX_CMD_LENGTH;
	if (copy_from_user(str, buffer, i)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto pll_performance_proc_write_exit;
	}
	str[MAX_CMD_LENGTH - 1] = 0;

	performance = AMB_PERFORMANCE_2160P60 + 1;
	for (i = 0; i < ARRAY_SIZE(performance_list); i++) {
		if (strstr(str, performance_list[i].name) != NULL) {
			performance = performance_list[i].performance;
		}
	}

	if (performance > AMB_PERFORMANCE_2160P60){
		pr_err("%s: invalid performance (%s)!\n", __func__, str);
		errorCode = -EINVAL;
		goto pll_performance_proc_write_exit;
	}

	result = amb_get_operating_mode(HAL_BASE_VP, &pll_info->operating_mode);
	if(result != AMB_HAL_SUCCESS){
		pr_err("%s: amb_get_operating_mode failed(%d)\n",
			__func__, result);
		errorCode = -EPERM;
		goto pll_performance_proc_write_exit;
	}

	pll_info->operating_mode.performance = performance;
	errorCode = ambarella_set_operating_mode(&pll_info->operating_mode);
	if (!errorCode)
		errorCode = count;

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
	if (!pll_info) {
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

	mode_file = create_proc_entry("mode", S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
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

	performance_file = create_proc_entry("performance", S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
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

pll_a5s_exit:
	return errorCode;
}

#else

struct ambarella_pll_info {
	unsigned int armfreq;
};

static int ambarella_freq_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	int					retlen = 0;

	if (off != 0)
		return 0;

	retlen = scnprintf(page, count,
			"\nPLL Information:\n"
			"\tCore:\t%d Hz\n"
			"\tDram:\t%d Hz\n"
			"\tiDSP:\t%d Hz\n"
			"\tAHB:\t%d Hz\n"
			"\tAPB:\t%d Hz\n\n",
			get_core_bus_freq_hz(),
			get_dram_freq_hz(),
			get_idsp_freq_hz(),
			get_ahb_bus_freq_hz(),
			get_apb_bus_freq_hz());

	*eof = 1;

	return retlen;
}

static int ambarella_freq_proc_write(struct file *file,
	const char __user *buffer, unsigned long count, void *data)
{
	struct ambarella_pll_info		*pll_info;
	char					str[MAX_CMD_LENGTH];
	int					errorCode = 0;
	unsigned long				flags;
	unsigned int				new_freq_cpu, cur_freq_cpu;
	unsigned int				i;

	pll_info = (struct ambarella_pll_info *)data;

	i = (count < MAX_CMD_LENGTH) ? count : MAX_CMD_LENGTH;
	if (copy_from_user(str, buffer, i)) {
		pr_err("%s: copy_from_user fail!\n", __func__);
		errorCode = -EFAULT;
		goto ambarella_pll_proc_write_exit;
	}
	str[MAX_CMD_LENGTH - 1] = 0;

	errorCode = sscanf(str, "%d", &new_freq_cpu);
	if (errorCode != 1) {
		pr_err("%s: convert sting fail %d!\n", __func__, errorCode);
		errorCode = -EINVAL;
		goto ambarella_pll_proc_write_exit;
	}

	if (new_freq_cpu > 243000000 || new_freq_cpu < 135000000) {
		pr_err("%s:\n\tinvalid frequency (%d)\n",
			__func__, new_freq_cpu);
		pr_info("\tfrequency should be 135000 ~ 243000 (in KHz)\n");
		errorCode = -EINVAL;
		goto ambarella_pll_proc_write_exit;
	}

	pr_debug("%s: %ld %d\n", __func__, count, new_freq_cpu);
	errorCode = count;

	if(new_freq_cpu == pll_info->armfreq)
		goto ambarella_pll_proc_write_exit;

	fio_select_lock(SELECT_FIO_HOLD, 1);	/* Hold the FIO bus first */

	/* Tell everyone what we're about to do... */
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
		cur_freq_cpu = get_core_bus_freq_hz();
	} while ((new_freq_cpu > pll_info->armfreq && cur_freq_cpu < new_freq_cpu) ||
		 (new_freq_cpu < pll_info->armfreq && cur_freq_cpu > new_freq_cpu));

#elif ((CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) || \
	(CHIP_REV == A5) || (CHIP_REV == A6))

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
		cur_freq_cpu = get_core_bus_freq_hz();
	} while ((new_freq_cpu > pll_info->armfreq && cur_freq_cpu < new_freq_cpu) ||
		(new_freq_cpu < pll_info->armfreq && cur_freq_cpu > new_freq_cpu));
#endif

	local_irq_restore(flags);

	ambarella_adjust_jiffies(AMB_CPUFREQ_POSTCHANGE,
		pll_info->armfreq, new_freq_cpu);
	srcu_notifier_call_chain(&pll_notifier_list,
				AMB_CPUFREQ_POSTCHANGE, &new_freq_cpu);

	pll_info->armfreq = get_core_bus_freq_hz();

	fio_unlock(SELECT_FIO_HOLD, 1);	/* Release the FIO bus */

ambarella_pll_proc_write_exit:
	return errorCode;
}

static int ambarella_init_pll_general(void)
{
	int					errorCode = 0;
	struct ambarella_pll_info		*pll_info;

	pll_info = kmalloc(sizeof(struct ambarella_pll_info), GFP_KERNEL);
	if (!pll_info) {
		pr_err("%s: No memory for pll_info\n", __func__);
		errorCode = -ENOMEM;
		goto pll_general_exit;
	}

	/* initial pll_info */
	pll_info->armfreq = get_core_bus_freq_hz();

	freq_file = create_proc_entry("corepll", S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
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

