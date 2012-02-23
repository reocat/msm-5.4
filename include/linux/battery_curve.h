/*
 * battery_curve.h
 *
 * History:
 *	2011/12/09 - [Bingliang Hu] created file
 *
 * Copyright (C) 2011-2012, Ambarella, Inc.
 *
 * All rights reserved. No Part of this file may be reproduced, stored
 * in a retrieval system, or transmitted, in any form, or by any means,
 * electronic, mechanical, photocopying, recording, or otherwise,
 * without the prior consent of Ambarella, Inc.
 *
 */

#ifndef __BATTERY_CURVE_H__
#define __BATTERY_CURVE_H__


#include <linux/mfd/wm831x/pmu.h>
#include <linux/mfd/wm831x/core.h>

struct battery_curve_parameter{
	const char *name;

	/*battery ADC value connecting with wall or usb power*/
	int voltage_with_power_min;
	int voltage_with_power_max;

	/*battery ADC value do not connecting with wall or usb power*/
	int voltage_without_power_min;
	int voltage_without_power_mid;
	int voltage_without_power_max;

	/*battery charge current*/
	int charge_current_min;
	int charge_current_max;

	/*charge mode percent. fast_mode_percent + cv_mode_percent = 100*/
	int fast_mode_percent;
	int cv_mode_percent;

	/*discharge percent. discharge_low + discharge_high = 100*/
	int discharge_low;
	int discharge_high;
};


extern struct battery_curve_parameter bc_parameter;
extern int cal_capacity(int chargecurrent, int sys_status, int charge_status, int uV, bool usbplug);

#endif


