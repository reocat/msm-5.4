/*
 * Intersil ISL12022M rtc class driver
 *
 * Copyright 2005,2006 Hebert Valerio Riedel <hvr@gnu.org>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>

#define DRV_VERSION "0.1"

/* Register map */
/* rtc section */
#define ISL12022M_REG_SC  0x00
#define ISL12022M_REG_MN  0x01
#define ISL12022M_REG_HR  0x02
#define ISL12022M_REG_HR_MIL     (1<<7)	/* 24h/12h mode */
#define ISL12022M_REG_HR_PM      (1<<5)	/* PM/AM bit in 12h mode */
#define ISL12022M_REG_DT  0x03
#define ISL12022M_REG_MO  0x04
#define ISL12022M_REG_YR  0x05
#define ISL12022M_REG_DW  0x06
#define ISL12022M_RTC_SECTION_LEN 7

/* control/status section */
#define ISL12022M_REG_SR  0x07
#define ISL12022M_REG_SR_RTCF    (1<<0)	/* rtc fail */
#define ISL12022M_REG_INT 0x08
#define ISL12022M_REG_INT_ARST  (1<<7)
#define ISL12022M_REG_INT_WRTC (1<<6)

#define ISL12022M_REG_PWR_VDD  0x09	/* reserved */
#define ISL12022M_REG_PWR_VBAT 0x0a
#define ISL12022M_REG_ITRO 0x0b
#define ISL12022M_REG_ALPHA 0x0c
#define ISL12022M_REG_BETA 0x0d
#define ISL12022M_REG_FATR 0x0e
#define ISL12022M_REG_FDTR 0x0f
#define ISL12022M_CSR_SECTION_LEN 9


/* alarm section */
#define ISL12022M_REG_SCA0 0x10
#define ISL12022M_REG_MNA0 0x11
#define ISL12022M_REG_HRA0 0x12
#define ISL12022M_REG_DTA0 0x13
#define ISL12022M_REG_MOA0 0x14
#define ISL12022M_REG_DWA0 0x15
#define ISL12022M_ALARM_SECTION_LEN 6

/* tsv2b section */
#define ISL12022M_REG_VSC 0x16
#define ISL12022M_REG_VMN 0x17
#define ISL12022M_REG_VHR 0x18
#define ISL12022M_REG_VDT 0x19
#define ISL12022M_REG_VMO 0x1a
#define ISL12022M_TSV2B_SECTION_LEN 5

/* tsb2v section */
#define ISL12022M_REG_BSC 0x1b
#define ISL12022M_REG_BMN 0x1c
#define ISL12022M_REG_BHR 0x1d
#define ISL12022M_REG_BDT 0x1e
#define ISL12022M_REG_BMO 0x1f
#define ISL12022M_TSB2V_SECTION_LEN 5

/* dstcr section */
#define ISL12022M_REG_DstMoFd 0x20
#define ISL12022M_REG_DstDwFd 0x21
#define ISL12022M_REG_DstDtFd 0x22
#define ISL12022M_REG_DstHrFd 0x23
#define ISL12022M_REG_DstMoRv 0x24
#define ISL12022M_REG_DstDwRv 0x25
#define ISL12022M_REG_DstDtRv 0x26
#define ISL12022M_REG_DstHrRv 0x27
#define ISL12022M_DSTCR_SECTION_LEN 8

/* temp section */
#define ISL12022M_REG_TK0L 0x28
#define ISL12022M_REG_TK0M 0x29
#define ISL12022M_TEMP_SECTION_LEN 2

/* nppm section */
#define ISL12022M_REG_NPPML 0x2a
#define ISL12022M_REG_NPPMH 0x2b
#define ISL12022M_NPPM_SECTION_LEN 2

/* xt0 section */
#define ISL12022M_REG_XT0 0x2c
#define ISL12022M_XT0_SECTION_LEN 1

/* alphah section */
#define ISL12022M_REG_ALPHAH 0x2d
#define ISL12022M_ALPHAH_SECTION_LEN 1

/* gpm section */
#define ISL12022M_REG_GPM1 0x2e
#define ISL12022M_REG_GPM2 0x2f
#define ISL12022M_GPM_SECTION_LEN 2


static struct i2c_driver isl12022m_driver;

/* block read */
static int
isl12022m_i2c_read_regs(struct i2c_client *client, u8 reg, u8 buf[],
		      unsigned len)
{
	u8 reg_addr[1] = { reg };
	struct i2c_msg msgs[2] = {
		{client->addr, I2C_M_IGNORE_NAK | I2C_M_PIN_MUXING, sizeof(reg_addr), reg_addr}
		,
		{client->addr, I2C_M_RD | I2C_M_PIN_MUXING, len, buf}
	};
	int ret;

	BUG_ON(reg > ISL12022M_REG_GPM2);
	BUG_ON(reg + len > ISL12022M_REG_GPM2 + 1);

	ret = i2c_transfer(client->adapter, msgs, 2);

	if (ret > 0)
		ret = 0;
	return ret;
}


/* block write */
static int
isl12022m_i2c_set_regs(struct i2c_client *client, u8 reg, u8 const buf[],
		     unsigned len)
{
	u8 i2c_buf[ISL12022M_REG_GPM2 + 2];
	struct i2c_msg msgs[1] = {
		{client->addr, I2C_M_IGNORE_NAK | I2C_M_PIN_MUXING, len + 1, i2c_buf}
	};
	int ret;

	BUG_ON(reg > ISL12022M_REG_GPM2);
	BUG_ON(reg + len > ISL12022M_REG_GPM2 + 1);

	i2c_buf[0] = reg;
	memcpy(&i2c_buf[1], &buf[0], len);
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret > 0)
		ret = 0;
	return ret;
}


/* simple check to see wether we have a isl12022m */
static int
isl12022m_i2c_validate_client(struct i2c_client *client)
{
	u8 regs[ISL12022M_RTC_SECTION_LEN] = { 0, };
	u8 zero_mask[ISL12022M_RTC_SECTION_LEN] = {
		0x80, 0x80, 0x40, 0xc0, 0xe0, 0x00, 0xf8
	};
	int i;
	int ret;

	ret = isl12022m_i2c_read_regs(client, 0, regs, ISL12022M_RTC_SECTION_LEN);

	if (ret < 0)
		return ret;

	for (i = 0; i < ISL12022M_RTC_SECTION_LEN; ++i) {
		if (regs[i] & zero_mask[i])	/* check if bits are cleared */
			return -ENODEV;
	}

	return 0;
}


static int
isl12022m_i2c_get_sr(struct i2c_client *client)
{
	int sr = -1;
	  int buf[1]={0};
	isl12022m_i2c_read_regs(client, ISL12022M_REG_SR, &buf, 1);
	sr=buf[0];
	if (sr < 0)
		return -EIO;
	return sr;
}


static int
isl12022m_i2c_get_int(struct i2c_client *client)
{
	int r_int = -1;

	isl12022m_i2c_read_regs(client, ISL12022M_REG_INT, &r_int, 1);

	if (r_int < 0)
		return -EIO;

	return r_int;
}


static int
isl12022m_i2c_get_atr(struct i2c_client *client)
{
	int atr = -1;

	isl12022m_i2c_read_regs(client, ISL12022M_REG_FATR, &atr, 1);

	if (atr < 0)
		return atr;

	/* The 6bit value in the ATR register controls the load
	 * capacitance C_load * in steps of 0.25pF
	 *
	 * bit (1<<5) of the ATR register is inverted
	 *
	 * C_load(ATR=0x20) =  4.50pF
	 * C_load(ATR=0x00) = 12.50pF
	 * C_load(ATR=0x1f) = 20.25pF
	 *
	 */

	atr &= 0x3f;		/* mask out lsb */
	atr ^= 1 << 5;		/* invert 6th bit */
	atr += 2 * 9;		/* add offset of 4.5pF; unit[atr] = 0.25pF */

	return atr;
}

static int
isl12022m_i2c_get_dtr(struct i2c_client *client)
{
	int dtr =-1;

	isl12022m_i2c_read_regs(client, ISL12022M_REG_FDTR, &dtr, 1);
	if (dtr < 0)
		return -EIO;

	/* dtr encodes adjustments of {-60,-40,-20,0,20,40,60} ppm */
	dtr = ((dtr & 0x3) * 20) * (dtr & (1 << 2) ? -1 : 1);

	return dtr;
}


static int
isl12022m_i2c_read_time(struct i2c_client *client, struct rtc_time *tm)
{
	int sr;
	u8 regs[ISL12022M_RTC_SECTION_LEN] = { 0, };

	sr = isl12022m_i2c_get_sr(client);
	if (sr < 0) {
		dev_err(&client->dev, "%s: reading SR failed\n", __func__);
		return -EIO;
	}

	sr = isl12022m_i2c_read_regs(client, 0, regs, ISL12022M_RTC_SECTION_LEN);
	if (sr < 0) {
		dev_err(&client->dev, "%s: reading RTC section failed\n",
			__func__);
		return sr;
	}

	tm->tm_sec = bcd2bin(regs[ISL12022M_REG_SC]);
	tm->tm_min = bcd2bin(regs[ISL12022M_REG_MN]);

	/* HR field has a more complex interpretation */
	{
		const u8 _hr = regs[ISL12022M_REG_HR];
		if (_hr & ISL12022M_REG_HR_MIL)	/* 24h format */
			tm->tm_hour = bcd2bin(_hr & 0x3f);
		else {
			/* 12h format */
			tm->tm_hour = bcd2bin(_hr & 0x1f);
			if (_hr & ISL12022M_REG_HR_PM)	/* PM flag set */
				tm->tm_hour += 12;
		}
	}

	tm->tm_mday = bcd2bin(regs[ISL12022M_REG_DT]);
	tm->tm_mon = bcd2bin(regs[ISL12022M_REG_MO]) - 1;	/* rtc starts at 1 */
	tm->tm_year = bcd2bin(regs[ISL12022M_REG_YR]) + 100;
	tm->tm_wday = bcd2bin(regs[ISL12022M_REG_DW]);

	return 0;
}


static int
isl12022m_i2c_read_alarm(struct i2c_client *client, struct rtc_wkalrm *alarm)
{
	struct rtc_time *const tm = &alarm->time;
	u8 regs[ISL12022M_ALARM_SECTION_LEN] = { 0, };
	int sr;

	sr = isl12022m_i2c_get_sr(client);
	if (sr < 0) {
		dev_err(&client->dev, "%s: reading SR failed\n", __func__);
		return sr;
	}

	sr = isl12022m_i2c_read_regs(client, ISL12022M_REG_SCA0, regs,
				   ISL12022M_ALARM_SECTION_LEN);
	if (sr < 0) {
		dev_err(&client->dev, "%s: reading alarm section failed\n",
			__func__);
		return sr;
	}

	/* MSB of each alarm register is an enable bit */
	tm->tm_sec = bcd2bin(regs[ISL12022M_REG_SCA0 -ISL12022M_REG_SCA0] & 0x7f);
	tm->tm_min = bcd2bin(regs[ISL12022M_REG_MNA0 - ISL12022M_REG_SCA0] & 0x7f);
	tm->tm_hour = bcd2bin(regs[ISL12022M_REG_HRA0 - ISL12022M_REG_SCA0] & 0x3f);
	tm->tm_mday = bcd2bin(regs[ISL12022M_REG_DTA0 - ISL12022M_REG_SCA0] & 0x3f);
	tm->tm_mon =
		bcd2bin(regs[ISL12022M_REG_MOA0 - ISL12022M_REG_SCA0] & 0x1f) - 1;
	tm->tm_wday = bcd2bin(regs[ISL12022M_REG_DWA0 - ISL12022M_REG_SCA0] & 0x03);

	return 0;
}


static int
isl12022m_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	return isl12022m_i2c_read_time(to_i2c_client(dev), tm);
}

static int
isl12022m_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	return isl12022m_i2c_read_alarm(to_i2c_client(dev), alarm);
}

static int
isl12022m_i2c_set_time(struct i2c_client *client, struct rtc_time const *tm)
{
	int sr;
	u8 regs[ISL12022M_RTC_SECTION_LEN] = { 0, };

	/* The clock has an 8 bit wide bcd-coded register (they never learn)
	 * for the year. tm_year is an offset from 1900 and we are interested
	 * in the 2000-2099 range, so any value less than 100 is invalid.
	 */

	if (tm->tm_year < 100)
		{
		return -EINVAL;
		}

	regs[ISL12022M_REG_SC] = bin2bcd(tm->tm_sec);
	regs[ISL12022M_REG_MN] = bin2bcd(tm->tm_min);
	regs[ISL12022M_REG_HR] = bin2bcd(tm->tm_hour) | ISL12022M_REG_HR_MIL;

	regs[ISL12022M_REG_DT] = bin2bcd(tm->tm_mday);
	regs[ISL12022M_REG_MO] = bin2bcd(tm->tm_mon + 1);
	regs[ISL12022M_REG_YR] = bin2bcd(tm->tm_year - 100);

	regs[ISL12022M_REG_DW] = bin2bcd(tm->tm_wday & 7);

	sr = isl12022m_i2c_get_sr(client);
	if (sr < 0) {
		dev_err(&client->dev, "%s: reading SR failed\n", __func__);
		return sr;
	}

	/* set WRTC */

	int r_int=isl12022m_i2c_get_int(client);

	r_int |= ISL12022M_REG_INT_WRTC;

	sr=isl12022m_i2c_set_regs(client,ISL12022M_REG_INT,&r_int,1);

		if (sr< 0) {
		dev_err(&client->dev, "%s: writing INT failed\n", __func__);
		return sr;
	}

	/* write RTC registers */
	sr = isl12022m_i2c_set_regs(client, 0, regs, ISL12022M_RTC_SECTION_LEN);

	if (sr < 0) {
		dev_err(&client->dev, "%s: writing RTC section failed\n",
			__func__);
		return sr;
	}

	/* clear WRTC again */

	r_int &= ~ISL12022M_REG_INT_WRTC;
	r_int=isl12022m_i2c_set_regs(client,ISL12022M_REG_INT,&r_int,1);

	if (sr < 0) {
		dev_err(&client->dev, "%s: writing INT failed\n", __func__);
		return sr;
	}

	return 0;
}

static int
isl12022m_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return isl12022m_i2c_set_time(to_i2c_client(dev), tm);
}

static const struct rtc_class_ops isl12022m_rtc_ops = {
	.read_time = isl12022m_rtc_read_time,
	.set_time = isl12022m_rtc_set_time,
};

/* sysfs interface */




static int
isl12022m_sysfs_register(struct device *dev)
{

	return 0;
}

static int
isl12022m_sysfs_unregister(struct device *dev)
{


	return 0;
}

static int
isl12022m_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;
	struct rtc_device *rtc;


	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	if (isl12022m_i2c_validate_client(client) < 0)
		return -ENODEV;

	dev_info(&client->dev,
		 "chip found, driver version " DRV_VERSION "\n");


	rtc = rtc_device_register(isl12022m_driver.driver.name,
				  &client->dev, &isl12022m_rtc_ops,
				  THIS_MODULE);

	if (IS_ERR(rtc))
		return PTR_ERR(rtc);

	i2c_set_clientdata(client, rtc);


	rc = isl12022m_i2c_get_sr(client);
	if (rc < 0) {
		dev_err(&client->dev, "reading status failed\n");
		goto exit_unregister;
	}

	if (rc & ISL12022M_REG_SR_RTCF)
		dev_warn(&client->dev, "rtc power failure detected, "
			 "please set clock.\n");
	rc = isl12022m_sysfs_register(&client->dev);
	if (rc)
		goto exit_unregister;

	return 0;

exit_unregister:
	rtc_device_unregister(rtc);

	return rc;
}

static int
isl12022m_remove(struct i2c_client *client)
{
	struct rtc_device *rtc = i2c_get_clientdata(client);

	isl12022m_sysfs_unregister(&client->dev);
	rtc_device_unregister(rtc);

	return 0;
}

static const struct i2c_device_id isl12022m_id[] = {
	{ "isl12022m", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, isl12022m_id);

static struct i2c_driver isl12022m_driver = {
	.driver = {
		   .name = "rtc-isl12022m",
		   },
	.probe = isl12022m_probe,
	.remove = isl12022m_remove,
	.id_table = isl12022m_id,
};

static int __init
isl12022m_init(void)
{
	printk("%s: %d\n", __func__, __LINE__);
	return i2c_add_driver(&isl12022m_driver);
}//done

static void __exit
isl12022m_exit(void)
{
	i2c_del_driver(&isl12022m_driver);
}//done

MODULE_AUTHOR("Wang Zhangkai <zkwang@ambarella.com>");
MODULE_DESCRIPTION("Intersil ISL12022M RTC driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

module_init(isl12022m_init);
module_exit(isl12022m_exit);

