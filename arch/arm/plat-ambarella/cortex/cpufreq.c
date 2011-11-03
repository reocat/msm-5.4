/*
 * arch/arm/plat-ambarella/cortex/cpufreq.c
 *
 * Author: Cao Rongrong <rrcao@ambarella.com>
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
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

#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <mach/hardware.h>
#include <hal/hal.h>

#ifdef CONFIG_REGULATOR
static struct regulator *vdd_axi = NULL;
/* CPU power table, it correspond with the frequency table.  */
static unsigned int vol_table[] = {
	[0] = 1200000,
	[1] = 1200000,
	[2] = 1200000,
	[3] = 1200000,
	[4] = 1200000,
	[5] = 1300000,
};
#endif

/* Frequency table index must be sequential starting at 0 */
static struct cpufreq_frequency_table freq_table[] = {
	{ 0, 360000 },	/* INTPROG=14 */
	{ 1, 480000 },	/* INTPROG=19 */
	{ 2, 600000 },	/* INTPROG=24 */
	{ 3, 768000 },	/* INTPROG=31 */
	{ 4, 888000 },	/* INTPROG=36 */
	{ 5, 1008000 },	/* INTPROG=41 */
	{ 6, CPUFREQ_TABLE_END },
};

static int ambarella_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

static unsigned int ambarella_getspeed(unsigned int cpu)
{
	if (cpu >= NR_CPUS)
		return 0;

	return amb_get_cortex_clock_frequency(HAL_BASE_VP) / 1000;
}

static int ambarella_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	int idx;
	amb_hal_success_t ret = AMB_HAL_SUCCESS;
	struct cpufreq_freqs freqs;

	cpufreq_frequency_table_target(policy, freq_table, target_freq,
		relation, &idx);

	freqs.old = ambarella_getspeed(0);
	freqs.new = freq_table[idx].frequency;

	if (freqs.old == freqs.new)
		return 0;

	for_each_online_cpu(freqs.cpu)
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

#ifdef CONFIG_CPU_FREQ_DEBUG
	printk(KERN_DEBUG "cpufreq-ambarella: transition: %u --> %u\n",
	       freqs.old, freqs.new);
#endif

#ifdef CONFIG_REGULATOR
	/* if moving to higher frequency, up the voltage beforehand */
	if (vdd_axi && freqs.new > freqs.old) {
		ret = regulator_set_voltage(vdd_axi,
				vol_table[freq_table[idx].index],
				vol_table[freq_table[idx].index]);
		if (ret != 0) {
			pr_err("cpufreq: Failed to set VDD_AXI for %dkHz: %d\n",
			       freqs.new, ret);
			goto err;
		}
	}
#endif

	ret = amb_set_cortex_clock_frequency(HAL_BASE_VP, freqs.new * 1000);
	if (ret != AMB_HAL_SUCCESS) {
		pr_err("cpu-ambarella: Failed to set cpu frequency to %d kHz\n",
			freqs.new);
		return ret;
	}

#ifdef CONFIG_REGULATOR
	/* if moving to lower freq, lower the voltage after lowering freq */
	if (vdd_axi && freqs.new < freqs.old) {
		ret = regulator_set_voltage(vdd_axi,
				vol_table[freq_table[idx].index],
				vol_table[freq_table[idx].index]);
		if (ret != 0) {
			pr_err("cpufreq: Failed to set VDD_AXI for %dkHz: %d\n",
			       freqs.new, ret);
			/* Although failed to set voltage, we still stay on new frequency */
			goto err;
		}
	}
err:
#endif

	for_each_online_cpu(freqs.cpu)
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	return 0;
}

static int ambarella_cpu_init(struct cpufreq_policy *policy)
{
	if (policy->cpu >= NR_CPUS)
		return -EINVAL;

#ifdef CONFIG_REGULATOR
	/* ambarella_cpu_init() will be called per cpu,
	 * but we need only one regulator. */
	if (vdd_axi == NULL) {
		vdd_axi = regulator_get(NULL, "cpu_vcc");
		if (IS_ERR(vdd_axi)) {
			pr_err("Unable to obtain voltage regulator for VDD_AXI;"
				" voltage scaling unsupported\n");
			return PTR_ERR(vdd_axi);
		}
	}
#endif
	cpufreq_frequency_table_cpuinfo(policy, freq_table);
	cpufreq_frequency_table_get_attr(freq_table, policy->cpu);
	policy->cur = ambarella_getspeed(policy->cpu);

	/* FIXME: what's the actual transition time? */
	policy->cpuinfo.transition_latency = 300 * 1000;

	policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;
	cpumask_copy(policy->related_cpus, cpu_possible_mask);

	return 0;
}

static int ambarella_cpu_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_cpuinfo(policy, freq_table);
#ifdef CONFIG_REGULATOR
	regulator_put(vdd_axi);
#endif
	return 0;
}

static struct freq_attr *ambarella_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver ambarella_cpufreq_driver = {
	.verify		= ambarella_verify_speed,
	.target		= ambarella_target,
	.get		= ambarella_getspeed,
	.init		= ambarella_cpu_init,
	.exit		= ambarella_cpu_exit,
	.name		= "ambarella",
	.attr		= ambarella_cpufreq_attr,
};

static int __init ambarella_cpufreq_init(void)
{
	return cpufreq_register_driver(&ambarella_cpufreq_driver);
}

static void __exit ambarella_cpufreq_exit(void)
{
        cpufreq_unregister_driver(&ambarella_cpufreq_driver);
}

MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("cpufreq driver for Ambarella iONE Cortex");
MODULE_LICENSE("GPL");
module_init(ambarella_cpufreq_init);
module_exit(ambarella_cpufreq_exit);

