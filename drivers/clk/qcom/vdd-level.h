/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019, The Linux Foundation. All rights reserved. */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_H

#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>

enum vdd_levels {
	VDD_NONE,
	VDD_MIN,		/* MIN_SVS */
	VDD_LOWER,		/* LOW_SVS / SVS2 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L1,		/* SVS_L1 */
	VDD_NOMINAL,		/* NOM */
	VDD_HIGH,		/* TURBO */
	VDD_NUM,
};

#endif
