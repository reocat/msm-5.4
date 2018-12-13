/*
 * Copyright (C) 2017 Ambarella Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <plat/event.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/clocksource.h>
#include <clocksource/arm_arch_timer.h>
#include "../../kernel/time/tick-internal.h"

struct ambarella_cpufreq_s {
	u32 effective_clk;
	struct clk *cortex_clk;
	struct clk *core_clk;
	struct cpufreq_frequency_table *cortex_freq_table;
	struct cpufreq_frequency_table *core_freq_table;
	struct device *dev;
};

extern int ambarella_aarch64_cntfrq_update(void);
extern struct clocksource *arch_clocksource_default_clock(void);
static struct ambarella_cpufreq_s *cpufreq;
static struct clocksource *copy_clocksource;

/*-----------------------------------------------------------------------------*/

static int ambarella_unregister_local_clocksource(void)
{
	struct clocksource *curr_clocksource =
		arch_clocksource_default_clock();

	__clocksource_register(copy_clocksource);
	clocksource_unregister(curr_clocksource);

	return 0;
}

static int ambarella_register_local_clocksource(void)
{
	int rate;
	struct clocksource *curr_clocksource =
		arch_clocksource_default_clock();

	local_irq_disable();
	local_fiq_disable();

	ambarella_aarch64_cntfrq_update();
	rate = arch_timer_get_cntfrq();

	local_fiq_enable();
	local_irq_enable();

	clocksource_register_hz(curr_clocksource, rate);
	clocksource_unregister(copy_clocksource);

	return 0;
}

static void ambarella_update_arm_cntfrq(void *data)
{

	struct clock_event_device *dev =
		this_cpu_ptr(&tick_cpu_device)->evtdev;

	ambarella_aarch64_cntfrq_update();

	/* Need to update clockevent frequency */
	clockevents_update_freq(dev, arch_timer_get_cntfrq());
}

static int ambarella_clocksrc_nc(struct notifier_block *nb,
				unsigned long event, void *data)
{
	switch(event) {
	case PRE_RATE_CHANGE:
		ambarella_unregister_local_clocksource();
		break;
	case POST_RATE_CHANGE:
		ambarella_register_local_clocksource();
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static int ambarella_copy_clocksource(void)
{
	struct clocksource *curr_clocksource = arch_clocksource_default_clock();

	copy_clocksource = kmalloc(sizeof(struct clocksource), GFP_KERNEL);
	if (!copy_clocksource)
		return -ENOMEM;

	memcpy(copy_clocksource, curr_clocksource, sizeof(struct clocksource));

	copy_clocksource->name = "__arch_sys_counter";

	return 0;
}

static int ambarella_cpufreq_nc(struct notifier_block *nb,
	unsigned long state, void *data)
{
	struct cpufreq_freqs *freqs = data;

	if (state == CPUFREQ_POSTCHANGE)
		smp_call_function_single(freqs->cpu, ambarella_update_arm_cntfrq,
			NULL, 1);

	return NOTIFY_OK;
}

static struct notifier_block ambarella_cpufreq_change = {
	.notifier_call = ambarella_cpufreq_nc,
};
static struct notifier_block ambarella_cortex_change = {
	.notifier_call = ambarella_clocksrc_nc,
};

static struct cpufreq_frequency_table *ambarella_get_cpufreq_table(void)
{
	struct cpufreq_frequency_table *target = NULL;

	switch (cpufreq->effective_clk) {
	case 0:
		target = cpufreq->cortex_freq_table;
		break;
	case 1:
		target = cpufreq->core_freq_table;
		break;
	case 2:
		target = cpufreq->cortex_freq_table;
		break;
	default:
		pr_err("get frequency table error. Effective clock is invalid.\n");
	}

	return target;
}

static struct clk *ambarella_get_cpufreq_clk(void)
{
	struct clk *clk = NULL;

	switch (cpufreq->effective_clk) {
	case 0:
		clk = cpufreq->cortex_clk;
		break;
	case 1:
		clk = cpufreq->core_clk;
		break;
	case 2:
		clk = cpufreq->cortex_clk;
		break;
	default:
		pr_err("get frequency target clock error. Effective clock is invalid.\n");
	}

	return clk;
}

static int ambarella_cpufreq_set_related(struct clk *clk,
		struct cpufreq_frequency_table *clk_table,
		unsigned int index)
{
	int rval;
	unsigned int prev_freq;

	prev_freq = clk_get_rate(clk) / 1000;

	rval = clk_set_rate(clk, clk_table[index].frequency * 1000);
	if (rval < 0) {
		pr_err("set '%s' frequency to %u HZ error.\n",
				__clk_get_name(clk), clk_table[index].frequency);
		return rval;
	}

	pr_info("cpufreq: %s, %u KHZ -> %lu KHZ\n",
			__clk_get_name(clk),
			prev_freq,
			clk_get_rate(clk) / 1000);
	return rval;
}

static int ambarella_cpufreq_set_target(struct cpufreq_policy *policy,
		unsigned int index)
{
	unsigned int old_freq, new_freq;
	struct cpufreq_frequency_table *freqs_table;
	struct cpufreq_freqs freqs;
	int rval;

	freqs_table = ambarella_get_cpufreq_table();
	if (!freqs_table)
		return -EINVAL;

	old_freq = clk_get_rate(policy->clk) / 1000;
	new_freq = freqs_table[index].frequency;

	freqs.old = old_freq;
	freqs.new = new_freq;

	/* CPUFREQ system notification */
	cpufreq_freq_transition_begin(policy, &freqs);

	/* Ambarella private system notification */
	ambarella_set_event(AMBA_EVENT_PRE_CPUFREQ, NULL);

	rval = clk_set_rate(policy->clk, new_freq * 1000);
	if (rval < 0) {
		pr_err("set %s frequency to %u KHZ error.\n",
				__clk_get_name(policy->clk), new_freq);
		goto err_set_rate;
	}

	pr_info("cpufreq: %s, %u KHZ -> %lu KHZ\n",
			__clk_get_name(policy->clk),
			old_freq,
			clk_get_rate(policy->clk) / 1000);

	if (cpufreq->effective_clk == 2)
		ambarella_cpufreq_set_related(cpufreq->core_clk,
				cpufreq->core_freq_table,
				index);
err_set_rate:

	ambarella_set_event(AMBA_EVENT_POST_CPUFREQ, NULL);
	cpufreq_freq_transition_end(policy, &freqs, 0);
	return rval;
}

static int ambarella_cpufreq_driver_init(struct cpufreq_policy *policy)
{
	int ret;
	struct cpufreq_frequency_table *target;

	/* only master CPU handler CPU frequency. */
	if (policy->cpu)
		return -EINVAL;

	target = ambarella_get_cpufreq_table();
	if (!target)
		return -EINVAL;

	policy->clk = ambarella_get_cpufreq_clk();
	if (!policy->clk)
		return -ENODEV;

	ret = cpufreq_generic_init(policy, target, CPUFREQ_ETERNAL);
	if (ret != 0) {
		pr_err("Failed to configure frequency table: %d\n", ret);
	}

	return ret;
}

static struct freq_attr *ambarella_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver ambarella_cpufreq_driver = {
	.flags		= CPUFREQ_STICKY | CPUFREQ_ASYNC_NOTIFICATION,
	.verify		= cpufreq_generic_frequency_table_verify,
	.target_index	= ambarella_cpufreq_set_target,
	.get		= cpufreq_generic_get,
	.init		= ambarella_cpufreq_driver_init,
	.attr		= ambarella_cpufreq_attr,
	.name		= "ambarella",
};

static const struct of_device_id ambarella_cpufreq_match[] = {
	{
		.compatible = "ambarella,cpufreq",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ambarella_cpufreq_match);

static int of_cpufreq_table_parse(struct device *dev)
{
	struct cpufreq_frequency_table *cortex_freq_table, *core_freq_table;
	const struct property *prop;
	const __be32 *val;
	int nr, i;

	prop = of_find_property(dev->of_node,
			"clocks-frequency-cortex-core", NULL);
	if (!prop)
		return -ENODEV;
	if (!prop->value)
		return -ENODATA;

	nr = prop->length / sizeof(u32);
	if (nr % 2)
		return -EINVAL;

	cortex_freq_table = kcalloc((nr / 2 + 1),
			sizeof(struct cpufreq_frequency_table), GFP_KERNEL);

	core_freq_table = kcalloc((nr / 2 + 1),
			sizeof(struct cpufreq_frequency_table), GFP_KERNEL);

	if (!cortex_freq_table || !core_freq_table)
		return -ENOMEM;

	cpufreq->cortex_freq_table = cortex_freq_table;
	cpufreq->core_freq_table = core_freq_table;

	val = prop->value;

	for (i = 0 ; i < nr / 2; i++) {
		cortex_freq_table[i].driver_data = i;
		cortex_freq_table[i].frequency = be32_to_cpup(val++);

		core_freq_table[i].driver_data = i;
		core_freq_table[i].frequency = be32_to_cpup(val++);
	}

	cortex_freq_table[i].driver_data =
		core_freq_table[i].driver_data = i;
	cortex_freq_table[i].frequency =
		core_freq_table[i].frequency = CPUFREQ_TABLE_END;

	return 0;
}

static int ambarella_cpufreq_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;

	np = pdev->dev.of_node;
	if (!np)
		return -ENODEV;

	cpufreq = devm_kzalloc(&pdev->dev, sizeof(*cpufreq), GFP_KERNEL);
	if (!cpufreq) {
		ret = -ENOMEM;
		goto err_put_node;
	}

	ret = of_property_read_u32(np, "cpufreq-effective-clock", &cpufreq->effective_clk);
	if (ret < 0 || cpufreq->effective_clk > 2) {
		ret = -ENODEV;
		goto err_put_node;
	}

	ret = of_cpufreq_table_parse(&pdev->dev);
	if (ret < 0)
		goto err_put_node;

	cpufreq->cortex_clk = of_clk_get_by_name(np, "cortex_clk");
	if (IS_ERR(cpufreq->cortex_clk)) {
		dev_err(&pdev->dev, "get 'cortex_clk' error.\n");
		ret = -ENODEV;
		goto err_put_node;
	}

	ret = clk_notifier_register(cpufreq->cortex_clk, &ambarella_cortex_change);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register clock notifier.\n");
		goto err_put_node;
	}

	cpufreq->core_clk = of_clk_get_by_name(np, "core_clk");
	if (IS_ERR(cpufreq->core_clk)) {
		dev_err(&pdev->dev, "get 'core_clk' error.\n");
		ret = -ENODEV;
		goto err_put_node;
	}

	ret = ambarella_copy_clocksource();
	if (ret)
		goto err_put_node;


	cpufreq_register_notifier(&ambarella_cpufreq_change,
			CPUFREQ_TRANSITION_NOTIFIER);

	cpufreq_register_driver(&ambarella_cpufreq_driver);
	of_node_put(np);
	return ret;

err_put_node:
	of_node_put(np);
	dev_err(&pdev->dev, "cpufreq probe error.\n");
	return ret;
}

static int ambarella_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&ambarella_cpufreq_driver);
	return 0;
}

static struct platform_driver ambarella_cpufreq_platdrv = {
	.driver = {
		.name	= "ambarella-cpufreq",
		.of_match_table = ambarella_cpufreq_match,
	},
	.probe		= ambarella_cpufreq_probe,
	.remove		= ambarella_cpufreq_remove,
};
module_platform_driver(ambarella_cpufreq_platdrv);

MODULE_AUTHOR("Jorney <qtu@ambarella.com>");
MODULE_DESCRIPTION("Ambarella cpufreq driver");
MODULE_LICENSE("GPL");
