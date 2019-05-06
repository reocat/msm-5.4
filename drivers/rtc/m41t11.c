/*
 * rtc class driver for the ST M41T11 chip
 *
 * based on previously existing rtc class drivers
 *
 * 2007 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/rtc.h>
#include <linux/delay.h>

#define DRV_VERSION "0.2"
int debug = 0;
module_param(debug, int, 0);
#define DBG_PRINTF(fmt,args...)   \
	do			                          \
	{                                             \
		if(debug)                        \
		printk(fmt,##args);  \
	}while(0);
/*
 * register indices
 */
#define M41T11_REG_SC			0	/* seconds      00-59 */
#define M41T11_REG_MN			1	/* minutes      00-59 */
#define M41T11_REG_HR			2	/* hours        00-23 */
#define M41T11_REG_DT			4	/* day of month 00-31 */
#define M41T11_REG_MO			5	/* month        01-12 */
#define M41T11_REG_DW			3	/* day of week   1-7  */
#define M41T11_REG_YR			6	/* year         00-99 */
#define M41T11_REG_CT			7	/* control */

#define M41T11_REG_LEN			8

#define M41T11_BURST_LEN		8	/* can burst r/w first 8 regs */

#define M41T11_REG_CT_WP		(1 << 7)	/* Write Protect */



#define M41T11_IDLE_TIME_AFTER_WRITE	3	/* specification says 2.5 mS */

static struct i2c_driver m41t11_driver;

static int m41t11_i2c_read_regs(struct i2c_client *client, u8 *buf)
{
	u8 reg_burst_read[1] = { 0x0 };
	struct i2c_msg msgs[2] = {
		{
		 .addr = client->addr,
		 .flags = 0,	/* write */
		 .len = sizeof(reg_burst_read),
		 .buf = reg_burst_read}
		,
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = M41T11_BURST_LEN,
		 .buf = buf}
		
	};
	int rc;

	rc = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (rc != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "%s: register read failed\n", __func__);
		return -EIO;
	}
	return 0;
}

static int m41t11_i2c_write_regs(struct i2c_client *client, u8 const *buf)
{
	
	u8 i2c_burst_buf[M41T11_BURST_LEN] = { 0x0 };
	struct i2c_msg burst_msgs[1] = {
		{
		 .addr = client->addr,
		 .flags = 0,	/* write */
		 .len = sizeof(i2c_burst_buf),
		 .buf = i2c_burst_buf}
	};
	int rc;

	memcpy(&i2c_burst_buf[1], buf, M41T11_BURST_LEN-1);

	rc = i2c_transfer(client->adapter, burst_msgs, ARRAY_SIZE(burst_msgs));
	if (rc != ARRAY_SIZE(burst_msgs))
		goto write_failed;
	msleep(M41T11_IDLE_TIME_AFTER_WRITE);

	return 0;

 write_failed:
	dev_err(&client->dev, "%s: register write failed\n", __func__);
	return -EIO;
}

static int m41t11_i2c_read_time(struct i2c_client *client, struct rtc_time *tm)
{
	int rc;
	u8 regs[M41T11_REG_LEN];
	DBG_PRINTF(KERN_INFO "read_time \n");
	rc = m41t11_i2c_read_regs(client, regs);
	if (rc < 0)
		return rc;

	tm->tm_sec = bcd2bin(regs[M41T11_REG_SC] & 0x7f);
	tm->tm_min = bcd2bin(regs[M41T11_REG_MN] & 0x7f);
	tm->tm_hour = bcd2bin(regs[M41T11_REG_HR] & 0x3f);
	tm->tm_mday = bcd2bin(regs[M41T11_REG_DT] & 0x3f);
	tm->tm_mon = bcd2bin(regs[M41T11_REG_MO] & 0x1f) - 1;
	tm->tm_year = bcd2bin(regs[M41T11_REG_YR])+100;
	tm->tm_wday = bcd2bin(regs[M41T11_REG_DW] & 0x7);

	
	DBG_PRINTF(KERN_INFO "currrent time : \n");
	
	DBG_PRINTF(KERN_INFO "\t sec  %d \n",tm->tm_sec);
	DBG_PRINTF(KERN_INFO "\t min  %d \n",tm->tm_min);
	DBG_PRINTF(KERN_INFO "\t hour %d \n",tm->tm_hour);
	DBG_PRINTF(KERN_INFO "\t mday %d \n",tm->tm_mday);
	DBG_PRINTF(KERN_INFO "\t mon  %d \n",tm->tm_mon);
	DBG_PRINTF(KERN_INFO "\t year %d \n",tm->tm_year);
	DBG_PRINTF(KERN_INFO "\t wday %d \n",tm->tm_wday);
	
	return rtc_valid_tm(tm);
}

static int m41t11_i2c_clear_write_protect(struct i2c_client *client)
{
	int rc;
	#if 0
	rc = i2c_smbus_write_byte_data(client, M41T11_REG_CONTROL_WRITE, 0);
	if (rc < 0) {
		dev_err(&client->dev, "%s: control register write failed\n",
			__func__);
		return -EIO;
	}
	#endif
	return 0;
}

static int
m41t11_i2c_set_time(struct i2c_client *client, struct rtc_time const *tm)
{
	u8 regs[M41T11_REG_LEN];
	int rc;
	DBG_PRINTF(KERN_INFO "set_time");
	rc = m41t11_i2c_clear_write_protect(client);
	if (rc < 0)
		return rc;

	regs[M41T11_REG_SC] = bin2bcd(tm->tm_sec);
	regs[M41T11_REG_MN] = bin2bcd(tm->tm_min);
	regs[M41T11_REG_HR] = bin2bcd(tm->tm_hour);
	regs[M41T11_REG_DT] = bin2bcd(tm->tm_mday);
	regs[M41T11_REG_MO] = bin2bcd(tm->tm_mon + 1 );
	regs[M41T11_REG_DW] = bin2bcd(tm->tm_wday);
	regs[M41T11_REG_YR] = bin2bcd(tm->tm_year % 100);
	/* set write protect */
	//regs[M41T11_REG_CT] = M41T11_REG_CT_WP;

	rc = m41t11_i2c_write_regs(client, regs);
	if (rc < 0)
		//return rc;

	return 0;
}

static int m41t11_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	return m41t11_i2c_read_time(to_i2c_client(dev), tm);
}

static int m41t11_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	return m41t11_i2c_set_time(to_i2c_client(dev), tm);
}

static int m41t11_remove(struct i2c_client *client)
{
	struct rtc_device *rtc = i2c_get_clientdata(client);

	if (rtc)
		rtc_device_unregister(rtc);

	return 0;
}

static const struct rtc_class_ops m41t11_rtc_ops = {
	.read_time = m41t11_rtc_read_time,
	.set_time = m41t11_rtc_set_time,
};

static int
m41t11_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct rtc_device *rtc;
	struct rtc_time rtc_tm;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	dev_info(&client->dev, "chip found, driver version " DRV_VERSION "\n");

	rtc = rtc_device_register(m41t11_driver.driver.name,
				  &client->dev, &m41t11_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc))
		return PTR_ERR(rtc);

	i2c_set_clientdata(client, rtc);
	
		
	return 0;
}

static struct i2c_device_id m41t11_id[] = {
	{ "m41t11", 0},
};
static struct i2c_driver m41t11_driver = {
	.driver = {
		   .name = "m41t11",
		   },
	.probe = m41t11_probe,
	.remove = m41t11_remove,
	.id_table = m41t11_id,
};

static int  m41t11_init(void)
{
	return i2c_add_driver(&m41t11_driver);
}

static void  m41t11_exit(void)
{
	i2c_del_driver(&m41t11_driver);
}
/*
 * Module entry point
 */
module_init(m41t11_init);
module_exit(m41t11_exit);
MODULE_DESCRIPTION("ST M41T11 RTC driver");
MODULE_AUTHOR("Issac liu");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
