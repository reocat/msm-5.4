/*
 * Ambarella SCM driver
 *
 * Copyright (C) 2017 Ambarella Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/arm-smccc.h>
#include "ambarella_scm.h"
#include <linux/of.h>

static int ambarella_scm_query(void)
{
	u32 fn;
	u32 cmd;
	struct arm_smccc_res res;

	fn = SVC_SCM_FN(AMBA_SCM_SVC_QUERY, AMBA_SCM_QUERY_VERSION);
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
			ARM_SMCCC_OWNER_SIP, fn);

	arm_smccc_smc(cmd, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

u32 ambarella_smc_readl(void __iomem *addr)
{
	struct arm_smccc_res res;
	u32 fn, cmd;

	fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_READ);
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
			ARM_SMCCC_OWNER_SIP, fn);

	arm_smccc_smc(cmd, (unsigned long)addr, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}
EXPORT_SYMBOL(ambarella_smc_readl);

void ambarella_smc_writel(u32 val, void __iomem *addr)
{
	struct arm_smccc_res res;
	u32 fn, cmd;

	fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_WRITE);
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
			ARM_SMCCC_OWNER_SIP, fn);

	arm_smccc_smc(cmd, (unsigned long)addr, val, 0, 0, 0, 0, 0, &res);
}
EXPORT_SYMBOL(ambarella_smc_writel);

int ambarella_aarch64_cntfrq_update(void)
{
	u32 fn;
	u32 cmd;
	struct arm_smccc_res res;

	fn = SVC_SCM_FN(AMBA_SCM_SVC_FREQ, AMBA_SCM_CNTFRQ_SETUP_CMD);
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
			ARM_SMCCC_OWNER_SIP, fn);

	arm_smccc_smc(cmd, 0, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}
EXPORT_SYMBOL(ambarella_aarch64_cntfrq_update);

static int ambarella_pm_smc(int gpio)
{
	u32 fn;
	u32 cmd;
	struct arm_smccc_res res;

	fn = SVC_SCM_FN(AMBA_SCM_SVC_PM, AMBA_SCM_PM_GPIO_SETUP);
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
			ARM_SMCCC_OWNER_SIP, fn);

	arm_smccc_smc(cmd, gpio, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static void ambarella_pm_sip(void)
{
	int gpio_notify_mcu = -1;

	of_property_read_u32(of_chosen, "ambarella,pm-gpio-notify-mcu",
			&gpio_notify_mcu);
	if (gpio_notify_mcu > 0) {
		pr_info("SCM: PM notification GPIO %d\n", gpio_notify_mcu);
		ambarella_pm_smc(gpio_notify_mcu);
	}
}

int __init ambarella_scm_init(void)
{
	BUG_ON(ambarella_scm_query() != ARM_SMCCC_SMC_64);

	ambarella_pm_sip();

	return 0;
}
arch_initcall(ambarella_scm_init);
