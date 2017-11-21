/*
 * Copyright (C) 2017 Ambarella Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "cpufreq: " fmt

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <plat/event.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

struct ambarella_cpufreq_info {
	unsigned int cpufreq_method;
	struct clk *cortex;
	struct clk *core;
	struct cpufreq_frequency_table *cortex_table;
	struct cpufreq_frequency_table *core_table;
	struct device *dev;
};

extern int ambarella_aarch64_cntfrq_update(void);
static struct ambarella_cpufreq_info *cpufreq_info;
static unsigned long regulator_latency = CPUFREQ_ETERNAL;

static struct cpufreq_frequency_table *ambarella_get_cpufreq_table(void)
{
	switch (cpufreq_info->cpufreq_method) {
	case 0:
	case 1:
		return cpufreq_info->cortex_table;
	case 2:
		return cpufreq_info->core_table;
	default:
		pr_err("%s can't find the cpufreq method!\n", __func__);
	}
	return NULL;
}

static struct clk *ambarella_get_cpufreq_clk(void)
{
	switch (cpufreq_info->cpufreq_method) {
	case 0:
	case 1:
		return cpufreq_info->cortex;
	case 2:
		return cpufreq_info->core;
	default:
		pr_err("%s can't find the cpufreq target clk!\n", __func__);
	}
	return NULL;
}

static int ambarella_cpufreq_set_relevance(unsigned int index)
{
	int ret;
	unsigned int relevance_freq;
	struct clk *relevance_clk;
	struct cpufreq_frequency_table *relevance_table;

	relevance_table = cpufreq_info->core_table;
	/* pll_out_core = 2 * core */
	relevance_freq = relevance_table[index].frequency * 2;
	relevance_clk = cpufreq_info->core;

	ret = clk_set_rate(relevance_clk, relevance_freq * 1000);
	if (ret < 0) {
		pr_err("%s Failed to set rate %dkHz: %d\n", __func__,  relevance_freq, ret);
		return 0;
	}

	pr_info("core -> %d KHZ(expected), %lu KHZ(actual) \n", relevance_freq / 2,
			clk_get_rate(relevance_clk) / 1000 / 2);

	return 0;
}

static int ambarella_cpufreq_set_target(struct cpufreq_policy *policy,
		unsigned int index)
{
	unsigned int old_freq, new_freq;
	struct cpufreq_frequency_table *target;
	struct cpufreq_freqs freqs;
	int ret = 0;

	target = ambarella_get_cpufreq_table();
	if (!target)
		return -EINVAL;

	old_freq = clk_get_rate(policy->clk) / 1000;
	new_freq = target[index].frequency;

	freqs.old = old_freq;
	freqs.new = new_freq;

	cpufreq_freq_transition_begin(policy, &freqs);
	ambarella_set_event(AMBA_EVENT_PRE_CPUFREQ, NULL);

	ret = clk_set_rate(policy->clk, new_freq * 1000);
	if (ret < 0) {
		pr_err("Failed to set rate %dkHz: %d\n", new_freq, ret);
		goto err_set_rate;
	}

	pr_info("cortex -> %d KHZ(expected), %lu KHZ (actual)\n", new_freq,
			clk_get_rate(policy->clk) / 1000);

	if (cpufreq_info->cpufreq_method == 1)
		ambarella_cpufreq_set_relevance(index);

#ifdef CONFIG_AMBARELLA_SCM
	ambarella_aarch64_cntfrq_update();
#endif

	pr_debug("Set actual frequency %lukHz\n", clk_get_rate(policy->clk) / 1000);

err_set_rate:
	ambarella_set_event(AMBA_EVENT_POST_CPUFREQ, NULL);
	cpufreq_freq_transition_end(policy, &freqs, 0);
	return ret;
}

static int ambarella_cpufreq_driver_init(struct cpufreq_policy *policy)
{
	int ret;
	struct cpufreq_frequency_table *target;

	if (policy->cpu != 0)
		return -EINVAL;

	target = ambarella_get_cpufreq_table();
	if (!target)
		return -EINVAL;

	policy->clk = ambarella_get_cpufreq_clk();
	if (!policy->clk)
		return -ENODEV;

	ret = cpufreq_generic_init(policy, target, regulator_latency);
	if (ret != 0) {
		pr_err("Failed to configure frequency table: %d\n",	ret);
	}

	return ret;
}

static struct freq_attr *ambarella_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver ambarella_cpufreq_driver = {
	.flags		= CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK |
				CPUFREQ_ASYNC_NOTIFICATION,
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

static int of_add_cpufreq_table(struct device *dev)
{
	struct cpufreq_frequency_table *target, *relevance;
	const struct property *prop;
	const __be32 *val;
	int nr, i;

	prop = of_find_property(dev->of_node, "cpufreq-table", NULL);
	if (!prop)
		return -ENODEV;
	if (!prop->value)
		return -ENODATA;

	nr = prop->length / sizeof(u32);
	if (nr % 2)
		return -EINVAL;

	target = kcalloc((nr / 2 + 1),
			sizeof(struct cpufreq_frequency_table), GFP_KERNEL);

	relevance = kcalloc((nr / 2 + 1),
			sizeof(struct cpufreq_frequency_table), GFP_KERNEL);

	if (!target || !relevance)
		return -ENOMEM;

	cpufreq_info->cortex_table = target;
	cpufreq_info->core_table = relevance;

	val = prop->value;

	for (i = 0 ; i < nr / 2; i++) {
		target[i].driver_data = i;
		target[i].frequency = be32_to_cpup(val++);

		relevance[i].driver_data = i;
		relevance[i].frequency = be32_to_cpup(val++);
	}

	target[i].driver_data = i;
	target[i].frequency = CPUFREQ_TABLE_END;

	relevance[i].driver_data = i;
	relevance[i].frequency = CPUFREQ_TABLE_END;

	return 0;
}

static int ambarella_cpufreq_probe(struct platform_device *pdev)
{
	int ret = 0;
	const char *cpufreq_method_str;
	struct device_node *np;

	np = pdev->dev.of_node;
	if (!np)
		return -ENODEV;

	cpufreq_info = devm_kzalloc(&pdev->dev, sizeof(*cpufreq_info), GFP_KERNEL);
	if (!cpufreq_info) {
		ret = -ENOMEM;
		goto err_put_node;
	}

	ret = of_property_read_string(np, "cpufreq-method", &cpufreq_method_str);
	if (ret < 0) {
		ret = -ENODEV;
		goto err_put_node;
	}

	if (!strcasecmp(cpufreq_method_str, "cortex"))
		cpufreq_info->cpufreq_method = 0;
	else if (!strcasecmp(cpufreq_method_str, "relevance"))
		cpufreq_info->cpufreq_method = 1;
	else if (!strcasecmp(cpufreq_method_str, "core"))
		cpufreq_info->cpufreq_method = 2;
	else {
		ret = -EINVAL;
		goto err_put_node;
	}

	ret = of_add_cpufreq_table(&pdev->dev);
	if (ret < 0)
		goto err_put_node;

	cpufreq_info->cortex = clk_get_sys(NULL, "gclk_cortex");
	if (IS_ERR(cpufreq_info->cortex)) {
		pr_err("Failed to get gclk_cortex: %ld\n", PTR_ERR(cpufreq_info->cortex));
		ret = PTR_ERR(cpufreq_info->cortex);
		goto err_put_node;
	}

	cpufreq_info->core = clk_get_sys(NULL, "pll_out_core");
	if (IS_ERR(cpufreq_info->core)) {
		pr_err("Failed to get pll_out_core: %ld\n", PTR_ERR(cpufreq_info->core));
		ret = PTR_ERR(cpufreq_info->core);
		goto err_put_node;
	}

	cpufreq_register_driver(&ambarella_cpufreq_driver);
	of_node_put(np);
	return 0;

err_put_node:
	of_node_put(np);
	dev_err(&pdev->dev, "%s: failed initialization\n", __func__);
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
