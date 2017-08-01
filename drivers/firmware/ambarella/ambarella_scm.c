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

static int ambarella_scm_call(void)
{
	u32 fn;
	u32 cmd;
	struct arm_smccc_res res;

	fn = SVC_SCM_FN(AMBA_SCM_SVC_QUERY, AMBA_SCM_QUERY_VERSION);
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
			ARM_SMCCC_OWNER_SIP, fn);

	arm_smccc_smc(cmd, 0, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

int ambarella_aarch64_cntfrq_update(void)
{
	u32 fn;
	u32 cmd;
	struct arm_smccc_res res;

	fn = SVC_SCM_FN(AMBA_SCM_SVC_FREQ, AMBA_SCM_CNTFRQ_SETUP_CMD);
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
			ARM_SMCCC_OWNER_SIP, fn);

	arm_smccc_smc(cmd, 0, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}
EXPORT_SYMBOL(ambarella_aarch64_cntfrq_update);

int ambarella_hibernate_gpio_pre(int gpio)
{
	u32 fn;
	u32 cmd;
	struct arm_smccc_res res;

	fn = SVC_SCM_FN(AMBA_SCM_SVC_HIBERNATE, AMBA_SCM_HIBERNATE_GPIO_SETUP);
	cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
			ARM_SMCCC_OWNER_SIP, fn);

	arm_smccc_smc(cmd, gpio, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}
EXPORT_SYMBOL(ambarella_hibernate_gpio_pre);

int __init ambarella_scm_init(void)
{
	/* FIXME: test */
	ambarella_scm_call();
	return 0;
}
arch_initcall(ambarella_scm_init);
