/*
 * PMU driver for Wolfson Microelectronics wm831x PMICs
 *
 * Copyright 2009 Wolfson Microelectronics PLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include <linux/mfd/wm831x/core.h>
#include <linux/mfd/wm831x/auxadc.h>
#include <linux/mfd/wm831x/pmu.h>
#include <linux/mfd/wm831x/pdata.h>

#include <plat/udc.h>
#include <plat/gpio.h>

#define BAT_UPDATE_DELAY_MSEC   5000
#define ADC_GAP        500000
#define BAT_ADC_NOAC_MIN   3300000
#define BAT_ADC_NOAC_MID   3600000
#define BAT_ADC_NOAC_MAX   3800000
#define BAT_ADC_MIN   3950000
#define BAT_ADC_MAX   4200000
#define fast_percent   10
#define cv_percent    (100-fast_percent)
#define BAT_CC_MIN   90					//BAT charge current
#define BAT_CC_MAX   1050

struct wm831x_power {
	struct wm831x *wm831x;
	struct power_supply wall;
	struct power_supply usb;
	struct power_supply battery;
	char wall_name[20];
	char usb_name[20];
	char battery_name[20];
	struct timer_list battery_update_timer;
};

static void enable_battery_charge(struct wm831x *wm831x)
{
	int ret= 0,value=0;

	value =  wm831x_reg_read(wm831x,WM831X_CHARGER_CONTROL_1);
	if(value < 0){
		dev_err(wm831x->dev, "Failed to read charger control 1: %d\n",
				value);
		return;
	}

	ret = wm831x_reg_unlock(wm831x);
	if (ret != 0) {
		dev_err(wm831x->dev, "Failed to unlock registers: %d\n", ret);
		return;
	}

	if(!(value & WM831X_CHG_ENA_MASK)){
		ret = wm831x_set_bits(wm831x, WM831X_CHARGER_CONTROL_1,
				      WM831X_CHG_ENA_MASK,
				      WM831X_CHG_ENA);
		if (ret != 0)
			dev_err(wm831x->dev, "Failed to set charger control 1: %d\n",
				ret);
	}
	wm831x_reg_lock(wm831x);
}

static void disable_battery_charge(struct wm831x *wm831x)
{
	int ret= 0,value=0;

	value =  wm831x_reg_read(wm831x,WM831X_CHARGER_CONTROL_1);
	if(value < 0){
		dev_err(wm831x->dev, "Failed to read charger control 1: %d\n",
				value);
		return;
	}

	ret = wm831x_reg_unlock(wm831x);
	if (ret != 0) {
		dev_err(wm831x->dev, "Failed to unlock registers: %d\n", ret);
		return;
	}

	if((value & WM831X_CHG_ENA_MASK)){
		ret = wm831x_set_bits(wm831x, WM831X_CHARGER_CONTROL_1,
				      WM831X_CHG_ENA_MASK,
				      0);
		if (ret != 0)
			dev_err(wm831x->dev, "Failed to set charger control 1: %d\n",
				ret);
	}
	wm831x_reg_lock(wm831x);
}

static int usbcable_without_docking(void)
{
	int ret =0;
//	printk(KERN_DEBUG "usb cable without docking %d\n",amb_udc_status);
	if((amb_udc_status == AMBARELLA_UDC_STATUS_CONFIGURED)
		& (ambarella_gpio_get(GPIO(7)) == GPIO_LOW)){
//		printk(KERN_DEBUG "usb cable without docking \n");
		ret = 1;
	}
	return ret;
}

static int wm831x_power_check_online(struct wm831x *wm831x, int supply,
				     union power_supply_propval *val)
{
	int ret;

	ret = wm831x_reg_read(wm831x, WM831X_SYSTEM_STATUS);
	if (ret < 0)
		return ret;

	if (ret & supply)
		val->intval = 1;
	else
		val->intval = 0;

	return 0;
}

static int wm831x_power_read_voltage(struct wm831x *wm831x,
				     enum wm831x_auxadc src,
				     union power_supply_propval *val)
{
	int ret;

	ret = wm831x_auxadc_read_uv(wm831x, src);
	if (ret >= 0)
		val->intval = ret;

	return ret;
}

/*********************************************************************
 *		WALL Power
 *********************************************************************/
static int wm831x_wall_get_prop(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct wm831x_power *wm831x_power = dev_get_drvdata(psy->dev->parent);
	struct wm831x *wm831x = wm831x_power->wm831x;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = wm831x_power_check_online(wm831x, WM831X_PWR_WALL, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = wm831x_power_read_voltage(wm831x, WM831X_AUX_WALL, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property wm831x_wall_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

/*********************************************************************
 *		USB Power
 *********************************************************************/
static int wm831x_usb_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct wm831x_power *wm831x_power = dev_get_drvdata(psy->dev->parent);
	struct wm831x *wm831x = wm831x_power->wm831x;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = wm831x_power_check_online(wm831x, WM831X_PWR_USB, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = wm831x_power_read_voltage(wm831x, WM831X_AUX_USB, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property wm831x_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

/*********************************************************************
 *		Battery properties
 *********************************************************************/

struct chg_map {
	int val;
	int reg_val;
};

static struct chg_map trickle_ilims[] = {
	{  50, 0 << WM831X_CHG_TRKL_ILIM_SHIFT },
	{ 100, 1 << WM831X_CHG_TRKL_ILIM_SHIFT },
	{ 150, 2 << WM831X_CHG_TRKL_ILIM_SHIFT },
	{ 200, 3 << WM831X_CHG_TRKL_ILIM_SHIFT },
};

static struct chg_map vsels[] = {
	{ 4050, 0 << WM831X_CHG_VSEL_SHIFT },
	{ 4100, 1 << WM831X_CHG_VSEL_SHIFT },
	{ 4150, 2 << WM831X_CHG_VSEL_SHIFT },
	{ 4200, 3 << WM831X_CHG_VSEL_SHIFT },
};

static struct chg_map fast_ilims[] = {
	{    0,  0 << WM831X_CHG_FAST_ILIM_SHIFT },
	{   50,  1 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  100,  2 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  150,  3 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  200,  4 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  250,  5 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  300,  6 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  350,  7 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  400,  8 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  450,  9 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  500, 10 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  600, 11 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  700, 12 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  800, 13 << WM831X_CHG_FAST_ILIM_SHIFT },
	{  900, 14 << WM831X_CHG_FAST_ILIM_SHIFT },
	{ 1000, 15 << WM831X_CHG_FAST_ILIM_SHIFT },
};

static struct chg_map eoc_iterms[] = {
	{ 20, 0 << WM831X_CHG_ITERM_SHIFT },
	{ 30, 1 << WM831X_CHG_ITERM_SHIFT },
	{ 40, 2 << WM831X_CHG_ITERM_SHIFT },
	{ 50, 3 << WM831X_CHG_ITERM_SHIFT },
	{ 60, 4 << WM831X_CHG_ITERM_SHIFT },
	{ 70, 5 << WM831X_CHG_ITERM_SHIFT },
	{ 80, 6 << WM831X_CHG_ITERM_SHIFT },
	{ 90, 7 << WM831X_CHG_ITERM_SHIFT },
};

static struct chg_map chg_times[] = {
	{  60,  0 << WM831X_CHG_TIME_SHIFT },
	{  90,  1 << WM831X_CHG_TIME_SHIFT },
	{ 120,  2 << WM831X_CHG_TIME_SHIFT },
	{ 150,  3 << WM831X_CHG_TIME_SHIFT },
	{ 180,  4 << WM831X_CHG_TIME_SHIFT },
	{ 210,  5 << WM831X_CHG_TIME_SHIFT },
	{ 240,  6 << WM831X_CHG_TIME_SHIFT },
	{ 270,  7 << WM831X_CHG_TIME_SHIFT },
	{ 300,  8 << WM831X_CHG_TIME_SHIFT },
	{ 330,  9 << WM831X_CHG_TIME_SHIFT },
	{ 360, 10 << WM831X_CHG_TIME_SHIFT },
	{ 390, 11 << WM831X_CHG_TIME_SHIFT },
	{ 420, 12 << WM831X_CHG_TIME_SHIFT },
	{ 450, 13 << WM831X_CHG_TIME_SHIFT },
	{ 480, 14 << WM831X_CHG_TIME_SHIFT },
	{ 510, 15 << WM831X_CHG_TIME_SHIFT },
};

static void wm831x_battey_apply_config(struct wm831x *wm831x,
				       struct chg_map *map, int count, int val,
				       int *reg, const char *name,
				       const char *units)
{
	int i;

	for (i = 0; i < count; i++)
		if (val == map[i].val)
			break;
	if (i == count) {
		dev_err(wm831x->dev, "Invalid %s %d%s\n",
			name, val, units);
	} else {
		*reg |= map[i].reg_val;
		dev_dbg(wm831x->dev, "Set %s of %d%s\n", name, val, units);
	}
}

static void wm831x_config_battery(struct wm831x *wm831x)
{
	struct wm831x_pdata *wm831x_pdata = wm831x->dev->platform_data;
	struct wm831x_battery_pdata *pdata;
	int ret, reg1, reg2;

	if (!wm831x_pdata || !wm831x_pdata->battery) {
		dev_warn(wm831x->dev,
			 "No battery charger configuration\n");
		return;
	}

	pdata = wm831x_pdata->battery;

	reg1 = 0;
	reg2 = 0;

	if (!pdata->enable) {
		dev_info(wm831x->dev, "Battery charger disabled\n");
		return;
	}

	reg1 |= WM831X_CHG_ENA;
	if (pdata->off_mask)
		reg2 |= WM831X_CHG_OFF_MSK;
	if (pdata->fast_enable)
		reg1 |= WM831X_CHG_FAST;

	wm831x_battey_apply_config(wm831x, trickle_ilims,
				   ARRAY_SIZE(trickle_ilims),
				   pdata->trickle_ilim, &reg2,
				   "trickle charge current limit", "mA");

	wm831x_battey_apply_config(wm831x, vsels, ARRAY_SIZE(vsels),
				   pdata->vsel, &reg2,
				   "target voltage", "mV");

	wm831x_battey_apply_config(wm831x, fast_ilims, ARRAY_SIZE(fast_ilims),
				   pdata->fast_ilim, &reg2,
				   "fast charge current limit", "mA");

	wm831x_battey_apply_config(wm831x, eoc_iterms, ARRAY_SIZE(eoc_iterms),
				   pdata->eoc_iterm, &reg1,
				   "end of charge current threshold", "mA");

	wm831x_battey_apply_config(wm831x, chg_times, ARRAY_SIZE(chg_times),
				   pdata->timeout, &reg2,
				   "charger timeout", "min");

	ret = wm831x_reg_unlock(wm831x);
	if (ret != 0) {
		dev_err(wm831x->dev, "Failed to unlock registers: %d\n", ret);
		return;
	}

	ret = wm831x_set_bits(wm831x, WM831X_CHARGER_CONTROL_1,
			      WM831X_CHG_ENA_MASK |
			      WM831X_CHG_FAST_MASK |
			      WM831X_CHG_ITERM_MASK,
			      reg1);
	if (ret != 0)
		dev_err(wm831x->dev, "Failed to set charger control 1: %d\n",
			ret);

	ret = wm831x_set_bits(wm831x, WM831X_CHARGER_CONTROL_2,
			      WM831X_CHG_OFF_MSK |
			      WM831X_CHG_TIME_MASK |
			      WM831X_CHG_FAST_ILIM_MASK |
			      WM831X_CHG_TRKL_ILIM_MASK |
			      WM831X_CHG_VSEL_MASK,
			      reg2);
	if (ret != 0)
		dev_err(wm831x->dev, "Failed to set charger control 2: %d\n",
			ret);

	wm831x_reg_lock(wm831x);
}

static int wm831x_bat_check_status(struct wm831x *wm831x, int *status)
{
	int ret;

	ret = wm831x_reg_read(wm831x, WM831X_SYSTEM_STATUS);
	if (ret < 0)
		return ret;

	if (ret & WM831X_PWR_SRC_BATT) {
		*status = POWER_SUPPLY_STATUS_DISCHARGING;
		ret = wm831x_reg_read(wm831x, WM831X_RESET_CONTROL);
		if (ret < 0)
			return ret;
		if(!(ret & WM831X_BATT_FET_ENA)){
			ret = wm831x_set_bits(wm831x, WM831X_RESET_CONTROL,
				WM831X_BATT_FET_ENA_MASK,
				WM831X_BATT_FET_ENA);
			}
		return 0;
	}

	ret = wm831x_reg_read(wm831x, WM831X_CHARGER_STATUS);
	if (ret < 0)
		return ret;

	switch (ret & WM831X_CHG_STATE_MASK) {
	case WM831X_CHG_STATE_OFF:
		*status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case WM831X_CHG_STATE_TRICKLE:
	case WM831X_CHG_STATE_FAST:
		*status = POWER_SUPPLY_STATUS_CHARGING;
		break;

	default:
		*status = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return 0;
}

static int wm831x_bat_check_type(struct wm831x *wm831x, int *type)
{
	int ret;

	ret = wm831x_reg_read(wm831x, WM831X_CHARGER_STATUS);
	if (ret < 0)
		return ret;

	switch (ret & WM831X_CHG_STATE_MASK) {
	case WM831X_CHG_STATE_TRICKLE:
	case WM831X_CHG_STATE_TRICKLE_OT:
		*type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case WM831X_CHG_STATE_FAST:
	case WM831X_CHG_STATE_FAST_OT:
		*type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		*type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	return 0;
}

static int wm831x_bat_check_health(struct wm831x *wm831x, int *health)
{
	int ret;

	ret = wm831x_reg_read(wm831x, WM831X_CHARGER_STATUS);
	if (ret < 0)
		return ret;

	if (ret & WM831X_BATT_HOT_STS) {
		*health = POWER_SUPPLY_HEALTH_OVERHEAT;
		return 0;
	}

	if (ret & WM831X_BATT_COLD_STS) {
		*health = POWER_SUPPLY_HEALTH_COLD;
		return 0;
	}

	if (ret & WM831X_BATT_OV_STS) {
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		return 0;
	}

	switch (ret & WM831X_CHG_STATE_MASK) {
	case WM831X_CHG_STATE_TRICKLE_OT:
	case WM831X_CHG_STATE_FAST_OT:
		*health = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	case WM831X_CHG_STATE_DEFECTIVE:
		*health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	default:
		*health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	}

	return 0;
}


static int cap_by_adc(int sys_status, int charge_status, int uV)
{
	int capc=0;
	static int first_time=0,timecounter=0;
	static int pre_adc_voltage_capacity=-1;

	if(uV < BAT_ADC_NOAC_MIN){
		return 0;
	}
	if(!(sys_status & WM831X_PWR_WALL)){
		if(uV <= BAT_ADC_NOAC_MID){
			capc = 30 * (uV-BAT_ADC_NOAC_MIN) /
				(BAT_ADC_NOAC_MID - BAT_ADC_NOAC_MIN);
		}
		else{
			capc = 30 + 70 * (uV - BAT_ADC_NOAC_MID) /
				(BAT_ADC_NOAC_MAX - BAT_ADC_NOAC_MID);
		}
		if(capc >= 100)
			capc = 100;
	}else{
		if(charge_status & WM831X_CHG_TOPOFF){//constant voltage mode
			if(pre_adc_voltage_capacity < 30)
				pre_adc_voltage_capacity = 30;
			if(timecounter== ((2*3*60) / (BAT_UPDATE_DELAY_MSEC / 1000))){//every 3 min to increase 1
				capc = pre_adc_voltage_capacity + 1;
				timecounter = 0;
			}else
				timecounter++;
			if(capc >= 100)
			capc = 99;
			if((charge_status & WM831X_CHG_STATE_MASK) == 0)
				capc = 100;
		}else{
			capc = 30 * (uV - BAT_ADC_MIN) /
				(BAT_ADC_MAX - BAT_ADC_MIN);
		}
	}

//printk("uV=%d, 400d=0x%x,404a=0x%x,cap=%d,pre=%d\n",uV, sys_status, charge_status, capc, pre_adc_voltage_capacity);
	if (first_time == 0) {
		first_time = 1;
		if (!(sys_status & WM831X_PWR_WALL))
			pre_adc_voltage_capacity = capc+20;//20:for first time ADC value adjust number
		else
			pre_adc_voltage_capacity = capc;
	}

	if(capc == pre_adc_voltage_capacity){
	}
	else if(capc > pre_adc_voltage_capacity)
	{
		if(!(sys_status & WM831X_PWR_WALL)){
			if(pre_adc_voltage_capacity == 0){
				capc = 1;
			pre_adc_voltage_capacity = capc;
		}
			capc = pre_adc_voltage_capacity;
		}
		else{
			capc = pre_adc_voltage_capacity + 1;
				pre_adc_voltage_capacity = capc;
			}
	}
		else
	{
		if(!(sys_status & WM831X_PWR_WALL)){
			if((pre_adc_voltage_capacity - capc) <= 20){
				capc = pre_adc_voltage_capacity-1;
				pre_adc_voltage_capacity=capc;
			}
			else{
				capc = pre_adc_voltage_capacity - 2;
			pre_adc_voltage_capacity = capc;
			}
		}else{
			capc = pre_adc_voltage_capacity;
		}
	}
	if(capc <= 0){
		capc = 0;
		pre_adc_voltage_capacity = capc;
	}

	if(capc >= 100){
		capc = 100;
		pre_adc_voltage_capacity = capc;
	}

	if(sys_status & WM831X_PWR_WALL){
		if((charge_status & WM831X_CHG_STATE_MASK) == 0){
				capc = 100;
		}
	}
	return capc;
}


static int cap_by_current(int chargecurrent, int sys_status, int charge_status, int uV)
{
	static int pre_capc,first_time=2;
	int capc=0;

	if(charge_status & WM831X_CHG_ACTIVE){
		if(sys_status & WM831X_PWR_WALL){
			if((charge_status & WM831X_CHG_STATE_MASK) == WM831X_CHG_STATE_FAST)
				capc = (fast_percent * (uV - BAT_ADC_MIN)) / (BAT_ADC_MAX - BAT_ADC_MIN);
			if(charge_status & WM831X_CHG_TOPOFF)
				capc = fast_percent + (cv_percent * (BAT_CC_MAX - chargecurrent)) / (BAT_CC_MAX - BAT_CC_MIN);
		}
	}else if(!(sys_status & WM831X_PWR_WALL)){
		if(uV < BAT_ADC_NOAC_MID){
			capc = 30 * (uV-BAT_ADC_NOAC_MIN) /
				(BAT_ADC_NOAC_MID - BAT_ADC_NOAC_MIN);
		}
		else{
			capc = 30 + 70 * (uV - BAT_ADC_NOAC_MID) /
				(BAT_ADC_NOAC_MAX - BAT_ADC_NOAC_MID);
		}
		if(capc >= 100)
			capc = 100;
	}

	if(capc < 0)
		capc = 0;

	if(capc >= 100){
		capc = 99;
	}
	//printk("uV=%d, current=%d,400d=0x%x,404a=0x%x,cap=%d,pre=%d\n",uV, chargecurrent, sys_status, charge_status, capc, pre_capc);
	if(sys_status & WM831X_PWR_WALL){
		if(((chargecurrent == 0) || (uV == 0)) && (uV <= 4190000) && (pre_capc != 99)){
			return pre_capc;
		}
	}

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

	if(capc == pre_capc){
	}
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
	return capc;
}

static int wm831x_bat_read_capacity(struct wm831x *wm831x, int *capacity)
{
	int uV, ret,status,capc=0,stopcharge=0,chargecurrent=0;

	/* calculate the capacity from voltage */
	uV = wm831x_auxadc_read_uv(wm831x, WM831X_AUX_BATT);
	if(uV < 0){
		ret = -EINVAL;
		return ret;
	}

	status = wm831x_reg_read(wm831x, WM831X_SYSTEM_STATUS);
	if (status < 0)
		return status;

	if(usbcable_without_docking()){
		disable_battery_charge(wm831x);
		stopcharge = 1;
	}else{
		enable_battery_charge(wm831x);
	}

	ret = wm831x_reg_read(wm831x, WM831X_CHARGER_STATUS);
	if (ret < 0)
		return ret;

	chargecurrent = wm831x_auxadc_read_uv(wm831x, WM831X_AUX_AUX1);
	chargecurrent = ((chargecurrent / 1000) * 125) / 100;
	if((chargecurrent == 0)
		&& (status & WM831X_CHG_ACTIVE)){
		capc = cap_by_adc(status,ret,uV);
	}else{
		capc = cap_by_current(chargecurrent,status,ret,uV);
	}

	*capacity = capc;
	return ret;
 }

static int wm831x_bat_read_capacity_level(struct wm831x *wm831x, int *cl)
{
	int ret;
	int c;

	ret = wm831x_bat_read_capacity(wm831x, &c);
	if (ret >= 0) {
		if (c >= 100) {
			*cl = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		} else if ((c < 100) && (c >= 60)) {
			*cl = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
		} else if ((c < 60) && ( c >= 20)) {
			*cl = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		} else if ((c < 20) && ( c >= 10)) {
			*cl = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		} else {
			*cl = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		}
	}

	return ret;
}

static int wm831x_bat_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct wm831x_power *wm831x_power = dev_get_drvdata(psy->dev->parent);
	struct wm831x *wm831x = wm831x_power->wm831x;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = wm831x_bat_check_status(wm831x, &val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = wm831x_power_check_online(wm831x, WM831X_PWR_SRC_BATT,
						val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = wm831x_power_read_voltage(wm831x, WM831X_AUX_BATT, val);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = wm831x_bat_check_health(wm831x, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = wm831x_bat_check_type(wm831x, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = wm831x_bat_read_capacity(wm831x, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = wm831x_bat_read_capacity_level(wm831x, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property wm831x_bat_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
//	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_PRESENT,
};

static const char *wm831x_bat_irqs[] = {
	"BATT HOT",
	"BATT COLD",
	"BATT FAIL",
	"OV",
	"END",
	"TO",
	"MODE",
	"START",
};

static irqreturn_t wm831x_bat_irq(int irq, void *data)
{
	struct wm831x_power *wm831x_power = data;
	struct wm831x *wm831x = wm831x_power->wm831x;

	dev_dbg(wm831x->dev, "Battery status changed: %d\n", irq);

	/* The battery charger is autonomous so we don't need to do
	 * anything except kick user space */
	power_supply_changed(&wm831x_power->battery);

	return IRQ_HANDLED;
}


/*********************************************************************
 *		Initialisation
 *********************************************************************/

static irqreturn_t wm831x_syslo_irq(int irq, void *data)
{
	struct wm831x_power *wm831x_power = data;
	struct wm831x *wm831x = wm831x_power->wm831x;

	/* Not much we can actually *do* but tell people for
	 * posterity, we're probably about to run out of power. */
	dev_crit(wm831x->dev, "SYSVDD under voltage\n");

	return IRQ_HANDLED;
}

static irqreturn_t wm831x_pwr_src_irq(int irq, void *data)
{
	struct wm831x_power *wm831x_power = data;
	struct wm831x *wm831x = wm831x_power->wm831x;

	dev_dbg(wm831x->dev, "Power source changed\n");
	/* Just notify for everything - little harm in overnotifying. */
	power_supply_changed(&wm831x_power->battery);
	power_supply_changed(&wm831x_power->usb);
	power_supply_changed(&wm831x_power->wall);

	return IRQ_HANDLED;
}

static void battery_update_timer_func(unsigned long data)
{
	struct wm831x_power *power = (struct wm831x_power *)data;
	power_supply_changed(&power->battery);
	mod_timer(&power->battery_update_timer,
		jiffies + msecs_to_jiffies(BAT_UPDATE_DELAY_MSEC));
}
static __devinit int wm831x_power_probe(struct platform_device *pdev)
{
	struct wm831x *wm831x = dev_get_drvdata(pdev->dev.parent);
	struct wm831x_pdata *wm831x_pdata = wm831x->dev->platform_data;
	struct wm831x_power *power;
	struct power_supply *usb;
	struct power_supply *battery;
	struct power_supply *wall;
	int ret, irq, i;

	power = kzalloc(sizeof(struct wm831x_power), GFP_KERNEL);
	if (power == NULL)
		return -ENOMEM;

	power->wm831x = wm831x;
	platform_set_drvdata(pdev, power);

	usb = &power->usb;
	battery = &power->battery;
	wall = &power->wall;

	if (wm831x_pdata && wm831x_pdata->wm831x_num) {
		snprintf(power->wall_name, sizeof(power->wall_name),
			 "wm831x-wall.%d", wm831x_pdata->wm831x_num);
		snprintf(power->battery_name, sizeof(power->wall_name),
			 "wm831x-battery.%d", wm831x_pdata->wm831x_num);
		snprintf(power->usb_name, sizeof(power->wall_name),
			 "wm831x-usb.%d", wm831x_pdata->wm831x_num);
	} else {
		snprintf(power->wall_name, sizeof(power->wall_name),
			 "wm831x-wall");
		snprintf(power->battery_name, sizeof(power->wall_name),
			 "wm831x-battery");
		snprintf(power->usb_name, sizeof(power->wall_name),
			 "wm831x-usb");
	}

	/* We ignore configuration failures since we can still read back
	 * the status without enabling the charger.
	 */
	wm831x_config_battery(wm831x);

	wall->name = power->wall_name;
	wall->type = POWER_SUPPLY_TYPE_MAINS;
	wall->properties = wm831x_wall_props;
	wall->num_properties = ARRAY_SIZE(wm831x_wall_props);
	wall->get_property = wm831x_wall_get_prop;
	ret = power_supply_register(&pdev->dev, wall);
	if (ret)
		goto err_kmalloc;

	battery->name = power->battery_name;
	battery->properties = wm831x_bat_props;
	battery->num_properties = ARRAY_SIZE(wm831x_bat_props);
	battery->get_property = wm831x_bat_get_prop;
	battery->use_for_apm = 1;
	ret = power_supply_register(&pdev->dev, battery);
	if (ret)
		goto err_wall;

	usb->name = power->usb_name,
	usb->type = POWER_SUPPLY_TYPE_USB;
	usb->properties = wm831x_usb_props;
	usb->num_properties = ARRAY_SIZE(wm831x_usb_props);
	usb->get_property = wm831x_usb_get_prop;
	ret = power_supply_register(&pdev->dev, usb);
	if (ret)
		goto err_battery;

	irq = platform_get_irq_byname(pdev, "SYSLO");
	ret = request_threaded_irq(irq, NULL, wm831x_syslo_irq,
				   IRQF_TRIGGER_RISING, "System power low",
				   power);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to request SYSLO IRQ %d: %d\n",
			irq, ret);
		goto err_usb;
	}

	irq = platform_get_irq_byname(pdev, "PWR SRC");
	ret = request_threaded_irq(irq, NULL, wm831x_pwr_src_irq,
				   IRQF_TRIGGER_RISING, "Power source",
				   power);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to request PWR SRC IRQ %d: %d\n",
			irq, ret);
		goto err_syslo;
	}

	for (i = 0; i < ARRAY_SIZE(wm831x_bat_irqs); i++) {
		irq = platform_get_irq_byname(pdev, wm831x_bat_irqs[i]);
		ret = request_threaded_irq(irq, NULL, wm831x_bat_irq,
					   IRQF_TRIGGER_RISING,
					   wm831x_bat_irqs[i],
					   power);
		if (ret != 0) {
			dev_err(&pdev->dev,
				"Failed to request %s IRQ %d: %d\n",
				wm831x_bat_irqs[i], irq, ret);
			goto err_bat_irq;
		}
	}

	setup_timer(&power->battery_update_timer, battery_update_timer_func,
		(unsigned long)power);
	mod_timer(&power->battery_update_timer,
		jiffies + msecs_to_jiffies(3*BAT_UPDATE_DELAY_MSEC));

	ret = wm831x_reg_unlock(wm831x);
	if (ret != 0) {
		dev_err(wm831x->dev, "Failed to unlock registers: %d\n", ret);
	return ret;
	}

	ret = wm831x_set_bits(wm831x, WM831X_CHARGER_CONTROL_1,
			      WM831X_CHG_IMON_ENA_MASK,
			      WM831X_CHG_IMON_ENA);
	if (ret != 0)
		dev_err(wm831x->dev, "Failed to set charger control 1: %d\n",
			ret);

	wm831x_reg_lock(wm831x);

	return ret;

err_bat_irq:
	for (; i >= 0; i--) {
		irq = platform_get_irq_byname(pdev, wm831x_bat_irqs[i]);
		free_irq(irq, power);
	}
	irq = platform_get_irq_byname(pdev, "PWR SRC");
	free_irq(irq, power);
err_syslo:
	irq = platform_get_irq_byname(pdev, "SYSLO");
	free_irq(irq, power);
err_usb:
	power_supply_unregister(usb);
err_battery:
	power_supply_unregister(battery);
err_wall:
	power_supply_unregister(wall);
err_kmalloc:
	kfree(power);
	return ret;
}

static __devexit int wm831x_power_remove(struct platform_device *pdev)
{
	struct wm831x_power *wm831x_power = platform_get_drvdata(pdev);
	int irq, i;

	del_timer_sync(&wm831x_power->battery_update_timer);

	for (i = 0; i < ARRAY_SIZE(wm831x_bat_irqs); i++) {
		irq = platform_get_irq_byname(pdev, wm831x_bat_irqs[i]);
		free_irq(irq, wm831x_power);
	}

	irq = platform_get_irq_byname(pdev, "PWR SRC");
	free_irq(irq, wm831x_power);

	irq = platform_get_irq_byname(pdev, "SYSLO");
	free_irq(irq, wm831x_power);

	power_supply_unregister(&wm831x_power->battery);
	power_supply_unregister(&wm831x_power->wall);
	power_supply_unregister(&wm831x_power->usb);
	kfree(wm831x_power);

	return 0;
}

static struct platform_driver wm831x_power_driver = {
	.probe = wm831x_power_probe,
	.remove = __devexit_p(wm831x_power_remove),
	.driver = {
		.name = "wm831x-power",
	},
};

static int __init wm831x_power_init(void)
{
	return platform_driver_register(&wm831x_power_driver);
}
module_init(wm831x_power_init);

static void __exit wm831x_power_exit(void)
{
	platform_driver_unregister(&wm831x_power_driver);
}
module_exit(wm831x_power_exit);

MODULE_DESCRIPTION("Power supply driver for WM831x PMICs");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wm831x-power");
