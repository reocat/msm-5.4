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
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include "ambarella_scm.h"

struct smc_dev_s {
	u64 addr;
	u32 length;
	struct list_head list;
};

static LIST_HEAD(smc_virt_list);

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
static int of_secure_bus(phys_addr_t phys_addr)
{
	struct device_node *node, *np;
	u32 val[2];
	int index, rval;

	node = of_find_node_by_name(NULL, "secure-world");
	if (!node) {
		pr_err("missing 'secure' DT node\n");
		return 0;
	}

	for (index = 0;; index ++) {

		np = of_parse_phandle(node, "device", index);
		if (!np)
			break;

		rval = of_property_read_u32_array(np, "reg", val, 2);
		if (rval)
			break;

		if ((u32)phys_addr == clamp((u32)phys_addr, val[0], val[0] + val[1]))
			return 1;
	}
	return 0;
}

void __smccc_virt_hacker(phys_addr_t phys_addr, void __iomem *virt_addr,
		size_t length)
{
	struct smc_dev_s *new;

	if (of_secure_bus(phys_addr)) {
		new = vmalloc(sizeof(struct smc_dev_s));
		if (!new)
			return;

		new->addr = (u64)virt_addr;
		new->length = length;

		list_add_tail(&new->list, &smc_virt_list);
	}
}

int __smccc_read(const volatile void __iomem *addr, u64 *val, u8 width)
{
	struct smc_dev_s *smc_dev;
	struct arm_smccc_res res;
	struct vm_struct *vm;
	u64 offset;
	u32 fn, cmd;

	list_for_each_entry(smc_dev, &smc_virt_list, list) {
		if ((u64)addr == clamp((u64)addr, smc_dev->addr,
					smc_dev->addr + smc_dev->length)) {

			vm = find_vm_area((const void *)addr);
			if (!vm)
				return 1;

			offset = (u64)addr & (vm->size - PAGE_SIZE - 1);
			switch (width) {
			case 64:
				fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_READ64);
				break;
			case 32:
				fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_READ32);
				break;
			case 16:
				fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_READ16);
				break;
			case 8:
				fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_READ8);
				break;
			default:
				return 1;
			}

			cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
					ARM_SMCCC_OWNER_SIP, fn);
			arm_smccc_smc(cmd, (unsigned long)vm->phys_addr + offset,
					0, 0, 0, 0, 0, 0, &res);
			*val = res.a0;
			return 0;
		}
	}
	return 1;
}
EXPORT_SYMBOL(__smccc_read);

int __smccc_write(volatile void __iomem *addr, u64 val, u8 width)
{
	struct smc_dev_s *smc_dev;
	struct arm_smccc_res res;
	struct vm_struct *vm;
	u32 fn, cmd;
	u64 offset;

	list_for_each_entry(smc_dev, &smc_virt_list, list) {
		if ((u64)addr == clamp((u64)addr, smc_dev->addr,
					smc_dev->addr + smc_dev->length)) {

			vm = find_vm_area((const void *)addr);
			if (!vm)
				return 1;

			offset = (u64)addr & (vm->size - PAGE_SIZE - 1);
			switch (width) {
			case 64:
				fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_WRITE64);
				break;
			case 32:
				fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_WRITE32);
				break;
			case 16:
				fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_WRITE16);
				break;
			case 8:
				fn = SVC_SCM_FN(AMBA_SIP_ACCESS_REG, AMBA_SIP_ACCESS_REG_WRITE8);
				break;
			default:
				return 1;
			}

			cmd = ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_64,
					ARM_SMCCC_OWNER_SIP, fn);
			arm_smccc_smc(cmd, (unsigned long)vm->phys_addr + offset,
					val, 0, 0, 0, 0, 0, &res);
			return 0;
		}
	}
	return 1;
}
EXPORT_SYMBOL(__smccc_write);

int __init ambarella_scm_init(void)
{
	BUG_ON(ambarella_scm_query() != ARM_SMCCC_SMC_64);
	ambarella_pm_sip();
	return 0;
}
arch_initcall(ambarella_scm_init);
