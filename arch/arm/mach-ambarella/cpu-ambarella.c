/*
 *  linux/arch/arm/mach-ambarella/cpu-ambarella.c
 *
 * Author: Cao Rong rong <rrcao@ambarella.com>
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
 * History:
 *	2009/04/14 - [Cao Rongrong] Created file
 *
 * Note:
 *   This driver may change the memory bus clock rate, but will not do any
 *   platform specific access timing changes... for example if you have flash
 *   memory which need to be changed according to the bus clock rate,
 *   you will need to register a platform specific notifier which will adjust the
 *   memory access strobes to maintain a minimum strobe width.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/cpufreq.h>

#include <mach/hardware.h>

//#define DEBUG

#ifdef DEBUG
#define ambfreq_debug(fmt, arg...)	printk(fmt, ##arg)
#else
#define ambfreq_debug(fmt, arg...)
#endif

static unsigned int ambarella_maxfreq = 283000;

typedef struct {
	unsigned int khz;
	unsigned int membus;
	unsigned int ref;
} ambarella_freqs_t;

static ambarella_freqs_t ambarella_freqs[] = {
	{135000, 135000, 0},
	{162000, 162000, 0},
	{189000, 189000, 0},
	{216000, 216000, 0},
	{243000, 243000, 0},
//	{256000, 256000, 0},
//	{270000, 270000, 0},
//	{283000, 283000, 0}
};


#define NUM_AMBARELLA_FREQS ARRAY_SIZE(ambarella_freqs)

static struct cpufreq_frequency_table ambarella_freq_table[NUM_AMBARELLA_FREQS+1];


/* find a valid frequency point */
static int ambarella_verify_policy(struct cpufreq_policy *policy)
{
	int ret;

	ret = cpufreq_frequency_table_verify(policy, ambarella_freq_table);

	ambfreq_debug("Verified CPU policy: %dKhz min to %dKhz max\n",
		 policy->min, policy->max);

	return ret;
}

static unsigned int ambarella_cpufreq_getspeed(unsigned int cpu)
{
	if (cpu)
		return 0;

	return get_core_bus_freq_hz() / 1000;
}

static int ambarella_set_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_frequency_table *freqs_table = ambarella_freq_table;
	ambarella_freqs_t *freq_settings = ambarella_freqs;
	struct cpufreq_freqs freqs;
	unsigned int idx;
	unsigned long flags;
	unsigned int new_freq_cpu, new_freq_mem, cur_freq_cpu;
	//unsigned int unused, preset_mdrefr, postset_mdrefr, cclkcfg;

	/* Lookup the next frequency index */
	if (cpufreq_frequency_table_target(policy, freqs_table,
					   target_freq, relation, &idx)) {
		return -EINVAL;
	}

	new_freq_cpu = freq_settings[idx].khz;
	new_freq_mem = freq_settings[idx].membus;
	if(new_freq_cpu == policy->cur)
		goto done;

	freqs.old = policy->cur;
	freqs.new = new_freq_cpu;
	freqs.cpu = policy->cpu;

	ambfreq_debug(KERN_INFO "Changing CPU frequency from %d Mhz to %d Mhz, "
		 "(SDRAM %d Mhz)\n",
		 freqs.old / 1000, freqs.new / 1000, new_freq_mem / 1000);

	/* Tell everyone what we're about to do...
	  * you should add a notify client with any platform specific
	  * Vcc changing capability */
	fio_select_lock(SELECT_FIO_HOLD, 1);	/* Hold the FIO bus first */
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	local_irq_save(flags);

#if ((CHIP_REV == A2) || (CHIP_REV == A3))

	do {
		cur_freq_cpu = amba_readl(PLL_CORE_CTRL_REG) & 0xfff00000;
		if (freqs.new > freqs.old) {
			cur_freq_cpu += 0x01000000;
		} else {
			cur_freq_cpu -= 0x01000000;
		}
		cur_freq_cpu |= (amba_readl(PLL_CORE_CTRL_REG) & 0x000fffff);
		amba_writel(PLL_CORE_CTRL_REG, cur_freq_cpu);
		mdelay(20);
		cur_freq_cpu = get_core_bus_freq_hz() / 1000;	/* in Khz */
	} while ((freqs.new > freqs.old && cur_freq_cpu < freqs.new) ||
		 (freqs.new < freqs.old && cur_freq_cpu > freqs.new));

#elif ((CHIP_REV == A2S) || (CHIP_REV == A2M) || (CHIP_REV == A2Q) || (CHIP_REV == A5) || (CHIP_REV == A6))

	do {
		u32 reg, intprog, sdiv, sout, valwe;

		reg = amba_readl(PLL_CORE_CTRL_REG);
		intprog = PLL_CTRL_INTPROG(reg);
		sout = PLL_CTRL_SOUT(reg);
		sdiv = PLL_CTRL_SDIV(reg);
		valwe = PLL_CTRL_VALWE(reg);
		valwe &= 0xffe;
		if (freqs.new > freqs.old)
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
	} while ((freqs.new > freqs.old && cur_freq_cpu < freqs.new) ||
		(freqs.new < freqs.old && cur_freq_cpu > freqs.new));

#endif

	local_irq_restore(flags);

	/* Tell everyone what we've just done...
	  * you should add a notify client with any platform specific SDRAM
	  * refresh timer adjustments. */
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	fio_unlock(SELECT_FIO_HOLD, 1);	/* Release the FIO bus */

done:
	return 0;
}

static __init int ambarella_cpufreq_init(struct cpufreq_policy *policy)
{
	int i;
	unsigned int freq;

	/* set default policy and cpuinfo */
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	policy->cpuinfo.transition_latency = 1000; /* FIXME: 1 ms, assumed */
	policy->cur = get_core_bus_freq_hz() / 1000;	   /* current freq */
	policy->min = policy->max = policy->cur;

	/* Generate the Ambarella chip cpufreq_frequency_table struct */
	for (i = 0; i < NUM_AMBARELLA_FREQS; i++) {
		freq = ambarella_freqs[i].khz;
		if (freq > ambarella_maxfreq)
			break;
		ambarella_freq_table[i].frequency = freq;
		ambarella_freq_table[i].index = i;
	}
	ambarella_freq_table[i].frequency = CPUFREQ_TABLE_END;

	/* Set the policy's minimum and maximum frequencies from the tables
	  * constructed just now. This sets cpuinfo.mxx_freq, min and max. */
	cpufreq_frequency_table_cpuinfo(policy, ambarella_freq_table);

	printk(KERN_INFO "Ambarella CPU frequency change support initialized\n");

	return 0;
}

static struct cpufreq_driver ambarella_cpufreq_driver = {
	.verify	= ambarella_verify_policy,
	.target	= ambarella_set_target,
	.init	= ambarella_cpufreq_init,
	.get	= ambarella_cpufreq_getspeed,
	.name	= "ambarella-freq",
};

static int __init ambarella_cpu_init(void)
{
	return cpufreq_register_driver(&ambarella_cpufreq_driver);
}

static void __exit ambarella_cpu_exit(void)
{
	cpufreq_unregister_driver(&ambarella_cpufreq_driver);
}


MODULE_AUTHOR("Cao Rongrong");
MODULE_DESCRIPTION("CPU frequency changing driver for Ambarella CPUs");
MODULE_LICENSE("GPL");
module_init(ambarella_cpu_init);
module_exit(ambarella_cpu_exit);

