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


#ifdef CONFIG_AMBARELLA_PLL_PROC

#define MAX_CMD_LENGTH	128

static const char pll_proc_name[] = "pll";

static struct srcu_notifier_head pll_notifier_list;
struct ambarella_pll_info {
	unsigned int corefreq;
};

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

static int ambarella_pll_proc_read(char *page, char **start,
	off_t off, int count, int *eof, void *data)
{
	struct ambarella_pll_info *pll_info;

	pll_info = (struct ambarella_pll_info *)data;

	printk("%s: corefreq = %d MHz\n", __func__, pll_info->corefreq / 1000);

	return 0;
}

static int ambarella_pll_proc_write(struct file *file,
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

	errorCode = count;

	printk("%s: %ld %d\n", __func__, count, new_freq_cpu);
	new_freq_cpu = new_freq_cpu * 1000;	/* in Khz */

	if(new_freq_cpu == pll_info->corefreq)
		goto ambarella_pll_proc_write_exit;

	/* Tell everyone what we're about to do... */
	fio_select_lock(SELECT_FIO_HOLD, 1);	/* Hold the FIO bus first */
	srcu_notifier_call_chain(&pll_notifier_list,
		AMB_CPUFREQ_PRECHANGE, &new_freq_cpu);
	ambarella_adjust_jiffies(AMB_CPUFREQ_PRECHANGE,
		pll_info->corefreq, new_freq_cpu);

	local_irq_save(flags);

#if ((CHIP_REV == A2) || (CHIP_REV == A3))

	do {
		cur_freq_cpu = amba_readl(PLL_CORE_CTRL_REG) & 0xfff00000;
		if (new_freq_cpu > pll_info->corefreq) {
			cur_freq_cpu += 0x01000000;
		} else {
			cur_freq_cpu -= 0x01000000;
		}
		cur_freq_cpu |= (amba_readl(PLL_CORE_CTRL_REG) & 0x000fffff);
		amba_writel(PLL_CORE_CTRL_REG, cur_freq_cpu);
		mdelay(20);
		cur_freq_cpu = get_core_bus_freq_hz() / 1000;	/* in Khz */
	} while ((new_freq_cpu > pll_info->corefreq && cur_freq_cpu < new_freq_cpu) ||
		 (new_freq_cpu < pll_info->corefreq && cur_freq_cpu > new_freq_cpu));

#elif ((CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) || (CHIP_REV == A5) || (CHIP_REV == A6))

	do {
		u32 reg, intprog, sdiv, sout, valwe;

		reg = amba_readl(PLL_CORE_CTRL_REG);
		intprog = PLL_CTRL_INTPROG(reg);
		sout = PLL_CTRL_SOUT(reg);
		sdiv = PLL_CTRL_SDIV(reg);
		valwe = PLL_CTRL_VALWE(reg);
		valwe &= 0xffe;
		if (new_freq_cpu > pll_info->corefreq)
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
	} while ((new_freq_cpu > pll_info->corefreq && cur_freq_cpu < new_freq_cpu) ||
		(new_freq_cpu < pll_info->corefreq && cur_freq_cpu > new_freq_cpu));

#endif

	local_irq_restore(flags);

	ambarella_adjust_jiffies(AMB_CPUFREQ_POSTCHANGE,
		pll_info->corefreq, new_freq_cpu);
	/* Tell everyone what we've just done...
	  * you should add a notify client with any platform specific SDRAM
	  * refresh timer adjustments. */
	srcu_notifier_call_chain(&pll_notifier_list,
				AMB_CPUFREQ_POSTCHANGE, &new_freq_cpu);
	fio_unlock(SELECT_FIO_HOLD, 1);	/* Release the FIO bus */

	pll_info->corefreq = get_core_bus_freq_hz() / 1000;

ambarella_pll_proc_write_exit:
	return errorCode;
}

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

#endif

int __init ambarella_init_pll(void)
{
	int					errorCode = 0;

#ifdef CONFIG_AMBARELLA_PLL_PROC
	static struct proc_dir_entry		*pll_file;
	struct ambarella_pll_info		*pll_info;
	pll_info = kmalloc(sizeof(struct ambarella_pll_info), GFP_KERNEL);
	if(!pll_info){
		pr_err("%s: No memory for pll_info\n", __func__);
	}

	pll_info->corefreq = get_core_bus_freq_hz() / 1000;

	pll_file = create_proc_entry(pll_proc_name, S_IRUGO | S_IWUSR,
		get_ambarella_proc_dir());
	if (pll_file == NULL) {
		errorCode = -ENOMEM;
		pr_err("%s: %s fail!\n", __func__, pll_proc_name);
	} else {
		pll_file->read_proc = ambarella_pll_proc_read;
		pll_file->write_proc = ambarella_pll_proc_write;
		pll_file->data = pll_info;
		pll_file->owner = THIS_MODULE;
	}


	srcu_init_notifier_head(&pll_notifier_list);
#endif

	return errorCode;
}


