/*
 * battery_curve.c
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>

#include <linux/delay.h>
#include <linux/battery_curve.h>
#include <linux/mfd/wm831x/auxadc.h>
#include <linux/mfd/wm831x/pdata.h>

#include <linux/battery_curve.h>

struct battery_curve_parameter bc_parameter;

static int cal_by_voltage(int sys_status,
	int charge_status,
	int uV)
{
	int capc=0;
	static int pre_cap = 60,timecounter=0;

//printk("by_voltage ");
	if(uV < bc_parameter.voltage_without_power_min){
		return 0;
	}
	if(!(sys_status & WM831X_PWR_WALL)){
		if(uV <= bc_parameter.voltage_without_power_mid){
			capc = bc_parameter.discharge_low *
				(uV-bc_parameter.voltage_without_power_min) /
				(bc_parameter.voltage_without_power_mid - bc_parameter.voltage_without_power_min);
		}
		else{
			capc = bc_parameter.discharge_low +
				bc_parameter.discharge_high *
				(uV - bc_parameter.voltage_without_power_mid) /
				(bc_parameter.voltage_without_power_max - bc_parameter.voltage_without_power_mid);
		}
	}else{
		if(charge_status & WM831X_CHG_TOPOFF){//constant voltage mode
				capc = pre_cap;
			if(timecounter== 72){//every 3 min to increase 1 :3*60*2 / (BAT_UPDATE_DELAY_MSEC/1000)
				capc = pre_cap + 1;
				timecounter = 0;
				pre_cap = capc;
			}else
				timecounter++;
		}else{
			capc = bc_parameter.discharge_low *
				(uV - bc_parameter.voltage_with_power_min) /
				(bc_parameter.voltage_with_power_max - bc_parameter.voltage_with_power_min);
		}
	}

	return capc;
}




static int cal_by_current(int chargecurrent,
	int sys_status,
	int charge_status,
	int uV)
{
	int capc=0;

//printk("by_current ");
	if(charge_status & WM831X_CHG_ACTIVE){
		if(sys_status & WM831X_PWR_WALL){
			if((charge_status & WM831X_CHG_STATE_MASK) == WM831X_CHG_STATE_FAST)
				capc = (bc_parameter.fast_mode_percent *
					(uV - bc_parameter.voltage_with_power_min)) /
					(bc_parameter.voltage_with_power_max - bc_parameter.voltage_with_power_min);

			if(charge_status & WM831X_CHG_TOPOFF)
				capc = bc_parameter.fast_mode_percent +
					((bc_parameter.cv_mode_percent *
					(bc_parameter.charge_current_max - chargecurrent)) /
					(bc_parameter.charge_current_max - bc_parameter.charge_current_min));
		}
	}else if(!(sys_status & WM831X_PWR_WALL)){
		if(uV < bc_parameter.voltage_without_power_mid){
			capc = bc_parameter.discharge_low *
				(uV-bc_parameter.voltage_without_power_min) /
				(bc_parameter.voltage_without_power_mid - bc_parameter.voltage_without_power_min);
		}
		else{
			capc = bc_parameter.discharge_low +
				(bc_parameter.discharge_high * (uV - bc_parameter.voltage_without_power_mid) /
				(bc_parameter.voltage_without_power_max - bc_parameter.voltage_without_power_mid));
		}
	}

	return capc;
}

static int stabilize_capacity(int capacity,
	int sys_status,
	int charge_status,
	int chargecurrent,
	int uV,
	bool usbplug)
{
	static int pre_capc,first_time=2;	
	int capc = capacity;

	if(capc < 0)
		capc = 0;

	if(capc >= 100){
		capc = 99;
	}
	//printk("*uV=%d, current=%d,400d=0x%x,404a=0x%x,cap=%d,pre=%d\n",uV, chargecurrent, sys_status, charge_status, capc, pre_capc);

	if(first_time == 2){
		if(sys_status & WM831X_PWR_WALL){
			if(charge_status & WM831X_CHG_ACTIVE){
				first_time = 1;
				pre_capc = capc;
			}
		}else{
			first_time = 1;
			pre_capc = capc;
		}
	}

	if(capc == pre_capc){}
	else if(capc > pre_capc){
		if(!(sys_status & WM831X_PWR_WALL)){
			if(pre_capc == 0){
				capc = 1;
				pre_capc = capc;
			}
			capc = pre_capc;
		}else{
			capc = pre_capc + 1;
			pre_capc = capc;
		}
	}else{
		if(!(sys_status & WM831X_PWR_WALL)){
			if((pre_capc - capc) <= 20){
				capc = pre_capc-1;
				pre_capc=capc;
			}else{
				capc = pre_capc - 2;
				pre_capc = capc;
			}
		}else{
			capc = pre_capc;
		}
	}

	if(capc <= 0){
		capc = 0;
		pre_capc = capc;
	}

	if(capc >= 100){
		capc = 100;
		pre_capc = capc;
	}

	if(sys_status & WM831X_PWR_WALL){
		if((charge_status & WM831X_CHG_STATE_MASK) == 0){
			capc = 100;
		}
	}

	if(usbplug)
		return pre_capc;

	return capc;
}

int cal_capacity(int chargecurrent,
	int sys_status,
	int charge_status,
	int uV,
	bool usbplug)
{
	int capc = 0;

	if(sys_status & (WM831X_PWR_WALL + WM831X_PWR_USB)){
		if(((chargecurrent < bc_parameter.charge_current_min)
			|| (chargecurrent > 2 * bc_parameter.charge_current_max))
			&& (charge_status & WM831X_CHG_ACTIVE)){
			capc = cal_by_voltage(sys_status,
				charge_status,
				uV);
		}else{
			capc = cal_by_current(chargecurrent,
				sys_status,
				charge_status,
				uV);
		}
	}else{
		capc = cal_by_voltage(sys_status,
			charge_status,
			uV);
	}

	capc = stabilize_capacity(capc,
		sys_status,
		charge_status,
		chargecurrent,
		uV,
		usbplug);

	return capc;
}

