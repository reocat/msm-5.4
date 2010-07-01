/*
 *  mma7455l.c - driver for 3-Axis digital low-g accelerometer
 *
 *  Copyright (C) 2008-2009 Rodolfo Giometti <giometti@xxxxxxxx>
 *  Copyright (C) 2008-2009 Eurotech S.p.A. <info@xxxxxxxxxxx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <linux/i2c/mma7455l.h>

#define DRIVER_VERSION		"0.40.0"

/*
 * Defines
 */

#define ACCEL_DATA		0x00
#define MCTL			0x16
#define    GSEL_SHIFT		   2
#define    GSEL_MASK		   (3 << 2)
#define    GSEL_NOP		   3
#define    MOD_SHIFT		   0
#define    MOD_MASK		   (3 << 0)

static const char * mode_lut[] = {
	"standby", "measurement", "level", "pulse",
};

/*
 * Structs
 */

struct maa7455l_data {
	struct mutex update_lock;
};

/*
 * Management functions
 */


static int
mma7455l_i2c_read_reg(struct i2c_client *client, u8 reg, u8 buf[])
{
	u8 reg_addr[1] = { reg };
	struct i2c_msg msgs[2] = {
		{client->addr, I2C_M_IGNORE_NAK | client->flags, sizeof(reg_addr), reg_addr}
		,
		{client->addr, I2C_M_RD | client->flags, 1, buf}
	};
	int ret;
	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret > 0)
		ret = 0;
	return ret;
}

static int
mma7455l_i2c_set_reg(struct i2c_client *client, u8 reg, u8 const buf[])
{
	u8 i2c_buf[2];
	struct i2c_msg msgs[1] = {
		{client->addr, I2C_M_IGNORE_NAK | client->flags, 1 + 1, i2c_buf}
	};
	int ret;
	i2c_buf[0] = reg;
	memcpy(&i2c_buf[1], &buf[0], 1);
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret > 0)
		ret = 0;
	return ret;
}

static int get_angle(struct i2c_client *client, u8 type, s16 *a)
{
	struct maa7455l_data *data = i2c_get_clientdata(client);
	struct i2c_msg msg1[] = {
		{ client->addr, 0| client->flags, 1, &type },
	};
	u8 d[9];
	struct i2c_msg msg2[] = {
		{ client->addr, I2C_M_RD| client->flags, 9, d },
	};
	int ret;

	mutex_lock(&data->update_lock);

	/* FIXME: should check DRDY flag */

	ret = i2c_transfer(client->adapter, msg1, 1);
	if (ret < 0)
		goto exit;

	mdelay(1);

	ret = i2c_transfer(client->adapter, msg2, 1);
	if (ret < 0)
		goto exit;
	ret = 0;
exit:
	mutex_unlock(&data->update_lock);

	a[0] = (s16) ((d[1] << 8) | d[0]);
	a[1] = (s16) ((d[3] << 8) | d[2]);
	a[2] = (s16) ((d[5] << 8) | d[4]);

	return ret;
}

static int write_g_select(struct i2c_client *client, int val)
{
	struct maa7455l_data *data = i2c_get_clientdata(client);
	int ret;
	int s_ret;
	u8 buf[1];

	switch (val) {
	case 2:
		val = GSEL_2;
		break;
	case 4:
		val = GSEL_4;
		break;
	case 8:
		val = GSEL_8;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&data->update_lock);

	//ret = i2c_smbus_read_byte_data(client, MCTL);
	s_ret = mma7455l_i2c_read_reg(client,MCTL,buf);
	if (s_ret < 0)
		goto exit;

    ret=buf[0];
	ret &= ~GSEL_MASK;
	ret |= val << GSEL_SHIFT;
	buf[0]=ret;

	//ret = i2c_smbus_write_byte_data(client, MCTL, buf);
	s_ret = mma7455l_i2c_set_reg(client,MCTL,buf);

	if (s_ret < 0)
		goto exit;
	s_ret = 0;
exit:
	mutex_unlock(&data->update_lock);

	return s_ret;
}

static int write_mode(struct i2c_client *client, int val)
{
	struct maa7455l_data *data = i2c_get_clientdata(client);
	int ret;
	int s_ret;
	u8 buf[1];

	if (val < 0 || val > 3)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	//ret = i2c_smbus_read_byte_data(client, MCTL);
	s_ret = mma7455l_i2c_read_reg(client,MCTL,buf);
	if (s_ret < 0)
		goto exit;

    ret=buf[0];
	ret &= ~MOD_MASK;
	ret |= val << MOD_SHIFT;
	buf[0]=ret;

	//ret = i2c_smbus_write_byte_data(client, MCTL, ret);
	s_ret = mma7455l_i2c_set_reg(client,MCTL,buf);
	if (s_ret < 0)
		goto exit;
	s_ret = 0;
exit:
	mutex_unlock(&data->update_lock);

	return s_ret;
}

/*
 * SysFS support
 */

static ssize_t show_accel(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	s16 data[3];
	int ret = get_angle(to_i2c_client(dev), ACCEL_DATA, data);
	if (ret < 0)
		return ret;

	return sprintf(buf, "Ax:%d,Ay:%d,Az:%d\n", data[0], data[1], data[2]);
}

static DEVICE_ATTR(accel, S_IRUGO, show_accel, NULL);

static ssize_t show_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
    int ret;
    int s_ret;
	u8 t_buf[1];
	//int ret = i2c_smbus_read_byte_data(to_i2c_client(dev), MCTL);
	s_ret = mma7455l_i2c_read_reg(to_i2c_client(dev),MCTL,t_buf);
	if (s_ret < 0)
		return s_ret;

    ret=t_buf[0];
	ret = (ret & MOD_MASK) >> MOD_SHIFT;
	return sprintf(buf, "%s", mode_lut[ret]);
}

static ssize_t store_mode(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *p;
	int len, i, ret;

	/* Remove '\n' from buf */
	p = memchr(buf, '\n', count);
	len = p ? p - buf : count;

	for (i = 0; i < 4; i++)
		if (!strncmp(buf, mode_lut[i], len))
			break;
	if (i >= 4)
		return -EINVAL;

	ret = write_mode(to_i2c_client(dev), i);
	return ret ? ret : count;
}

static DEVICE_ATTR(mode, S_IWUSR | S_IRUGO, show_mode, store_mode);

static ssize_t show_g_select(struct device *dev,
				struct device_attribute *attr, char *buf)
{
    int ret;
	int s_ret;
	u8 t_buf[1];
	//int ret = i2c_smbus_read_byte_data(to_i2c_client(dev), MCTL);
	s_ret = mma7455l_i2c_read_reg(to_i2c_client(dev),MCTL,t_buf);
	if (s_ret < 0)
		return s_ret;

    ret=t_buf[0];
	ret = (ret & GSEL_MASK) >> GSEL_SHIFT;
	if (ret == GSEL_NOP)
		return -EINVAL;
	return sprintf(buf, "%d", ret == GSEL_8 ? 8 : (ret == GSEL_2 ? 2 : 4));
}

static ssize_t store_g_select(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	long val;
	int ret = strict_strtol(buf, 10, &val);
	if (ret)
		return -EINVAL;

	ret = write_g_select(to_i2c_client(dev), val);
	return ret ? ret : count;
}

static DEVICE_ATTR(g_select, S_IWUSR | S_IRUGO, show_g_select, store_g_select);

static struct attribute *mma7455l_attributes[] = {
	&dev_attr_accel.attr,
	&dev_attr_g_select.attr,
	&dev_attr_mode.attr,
	NULL,
};

static const struct attribute_group mma7455l_attr_group = {
	.attrs = mma7455l_attributes,
};

/*
 * Initialization function
 */

static int mma7455l_init_client(struct i2c_client *client,
				struct mma7455l_platform_data *pdata)
{
	int ret;

	/* Set the platform defaults */

	ret = write_g_select(client, pdata->g_select);
	if (ret)
		dev_warn(&client->dev, "unable to init g selct to %d\n",
				pdata->g_select);
	ret = write_mode(client, pdata->mode);
	if (ret)
		dev_warn(&client->dev, "unable to init mode to %d\n",
				pdata->mode);

	return 0;
}

/*
 * I2C init/probing/exit functions
 */

static struct mma7455l_platform_data defs;	/* all values are set to '0' */

static struct i2c_driver mma7455l_driver;
static int __devinit mma7455l_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{

	struct i2c_adapter *adapter;
	struct maa7455l_data *data;
	struct mma7455l_platform_data *pdata;
	int ret;

	adapter = to_i2c_adapter(client->dev.parent);
	ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE
				     | I2C_FUNC_SMBUS_WRITE_BYTE_DATA)) {
		ret = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct maa7455l_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto exit;
	}
	i2c_set_clientdata(client, data);


	/* Check platform data */
	pdata = client->dev.platform_data;
	if (!pdata)
		{
		pdata = &defs;
	pdata->g_select=8;
	pdata->mode=2;
		}
	dev_info(&client->dev, "g select %d\n", pdata->g_select);
	dev_info(&client->dev, "mode %d\n", pdata->mode);


	mutex_init(&data->update_lock);
	/* Initialize the MMA7455L chip */
	ret = mma7455l_init_client(client, pdata);
	if (ret)
		goto exit_kfree;
	/* Register sysfs hooks */
	ret = sysfs_create_group(&client->dev.kobj, &mma7455l_attr_group);
	if (ret)
		goto exit_kfree;
		goto exit;

	dev_info(&client->dev, "support ver. %s enabled\n", DRIVER_VERSION);
	return 0;

exit_kfree:
	kfree(data);
exit:
	return ret;
}

static int __devexit mma7455l_remove(struct i2c_client *client)
{
	struct maa7455l_data *data = i2c_get_clientdata(client);
	sysfs_remove_group(&client->dev.kobj, &mma7455l_attr_group);
	i2c_set_clientdata(client, NULL);

	if(data)
	   kfree(data);

	return 0;
}

static const struct i2c_device_id mma7455l_id[] = {
	{ "mma7455l", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mma7455l_id);

static struct i2c_driver mma7455l_driver = {
	.driver = {
		.name	= "mma7455l",
		.owner	= THIS_MODULE,
	},
	.probe	= mma7455l_probe,
	.remove	= __devexit_p(mma7455l_remove),
	.id_table = mma7455l_id,
};

static int __init mma7455l_init(void)
{
	return i2c_add_driver(&mma7455l_driver);
}

static void __exit mma7455l_exit(void)
{
	i2c_del_driver(&mma7455l_driver);
}

MODULE_AUTHOR("Rodolfo Giometti <giometti@xxxxxxxx>");
MODULE_DESCRIPTION("MMA7455L 3-Axis digital low-g accelerometer");
MODULE_LICENSE("GPL");

module_init(mma7455l_init);
module_exit(mma7455l_exit);
