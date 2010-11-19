/*
 * drivers/input/touchscreen/tm1510.c
 *
 * Copyright (C) 2004-2010, Ambarella, Inc.
 *	Zhenwu Xue <zwxue@ambarella.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c/tm1510.h>

#ifdef	CONFIG_DEBUG_TOUCHSCREEN
#define TM1510_DEBUG(format, arg...)	printk(format , ## arg)
#else
#define TM1510_DEBUG(format, arg...)
#endif

#define MAX_X	3930
#define MAX_Y	2384
#define MAX_Z	16

typedef enum {
	TM_FINGER_STATE		= 0x0,
	TM_X1_HIGH,
	TM_Y1_HIGH,
	TM_XY1_LOW,
	TM_W1,
	TM_Z1,
	TM_X2_HIGH,
	TM_Y2_HIGH,
	TM_XY2_LOW,
	TM_W2,
	TM_Z2,

	TM_IRQ_STATUS_ABS	= 0x14,
	TM_FINGER_STATE_ABS	= 0x15,
	TM_IRQ_ENABLE_ABS	= 0x26,
} tm1510_sub_addr_t;

#define NUM_DATA			32

struct tm1510 {
	char				phys[32];
	struct input_dev		*input;
	struct i2c_client		*client;
	struct workqueue_struct 	*workqueue;
	struct work_struct		report_worker;
	u8				reg_data[NUM_DATA];
	int				irq;
	struct tm1510_fix_data	fix;
	int				(*get_pendown_state)(void);
	void				(*clear_penirq)(void);
};

static int tm1510_config_irq(struct tm1510 *tm)
{
	struct i2c_msg		msg;
	int			errorCode;
	u8			buf[] = {TM_IRQ_ENABLE_ABS,
						0x04, 0x01, 0x0b, 0x06, 0x06};

	msg.addr	= tm->client->addr;
	msg.flags	= tm->client->flags;
	msg.buf		= buf;
	msg.len		= sizeof(buf);
	errorCode = i2c_transfer(tm->client->adapter, &msg, 1);
	if (errorCode != 1) {
		printk("TM1510: Can't write reg data.\n");
		return -EIO;
	}

	return 0;
}

static int tm1510_clear_irq(struct tm1510 *tm)
{
	struct i2c_msg		msg;
	int			errorCode;
	u8			buf[] = {TM_IRQ_STATUS_ABS};

	msg.addr	= tm->client->addr;
	msg.flags	= tm->client->flags;
	msg.buf		= buf;
	msg.len		= sizeof(buf);
	errorCode = i2c_transfer(tm->client->adapter, &msg, 1);
	if (errorCode != 1) {
		printk("TM1510: Can't write reg data.\n");
		return -EIO;
	}

	msg.addr	= tm->client->addr;
	msg.flags	= tm->client->flags | I2C_M_RD;
	msg.buf		= buf;
	msg.len		= sizeof(buf);
	errorCode = i2c_transfer(tm->client->adapter, &msg, 1);
	if (errorCode != 1) {
		printk("TM1510: Can't read reg data.\n");
		return -EIO;
	}

	return 0;
}

static inline int tm1510_read_all(struct tm1510 *tm)
{
	struct i2c_msg		msg;
	int			errorCode;
	u8			buf[] = {TM_FINGER_STATE_ABS};

	msg.addr	= tm->client->addr;
	msg.flags	= tm->client->flags;
	msg.buf		= buf;
	msg.len		= 1;
	errorCode = i2c_transfer(tm->client->adapter, &msg, 1);
	if (errorCode != 1) {
		printk("TM1510: Can't write reg data.\n");
		return -EIO;
	}

	msg.addr	= tm->client->addr;
	msg.flags	= tm->client->flags | I2C_M_RD;
	msg.buf		= tm->reg_data;
	msg.len		= NUM_DATA;
	errorCode = i2c_transfer(tm->client->adapter, &msg, 1);
	if (errorCode != 1) {
		printk("TM1510: Can't read reg data.\n");
		return -EIO;
	}

	return 0;
}

static void tm1510_send_event(struct tm1510 *tm)
{
	struct input_dev	*input = tm->input;
	u8			finger_state[2];
	static int		prev_touch = 0;
	static int		curr_touch = 0;
	int			event = 0;

	finger_state[0] = tm->reg_data[TM_FINGER_STATE] & 0x03;
	finger_state[1] = (tm->reg_data[TM_FINGER_STATE] & 0x0c) >> 2;
	curr_touch = 0;
	if (finger_state[0] == 1)
		curr_touch++;
	if (finger_state[1] == 1)
		curr_touch++;

	/* Button Pressed */
	if (!prev_touch && curr_touch) {
		TM1510_DEBUG("Finger Pressed\n");
	}

	/* Button Released */
	if (prev_touch && !curr_touch) {
		event = 1;
		input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0);
		input_mt_sync(input);
		TM1510_DEBUG("Finger Released\n\n\n");
	}

	if (curr_touch) {
		u8 x1h, x1l, y1h, y1l;
		u32 x1, y1;

		x1h = tm->reg_data[TM_X1_HIGH];
		x1l = tm->reg_data[TM_XY1_LOW] & 0x0f;
		y1h = tm->reg_data[TM_Y1_HIGH];
		y1l = (tm->reg_data[TM_XY1_LOW] & 0xf0) >> 4;
		x1 = (x1h << 4) | x1l;
		y1 = (y1h << 4) | y1l;

		if (x1 > MAX_X)
			x1 = MAX_X;

		if (y1 > MAX_Y)
			y1 = MAX_Y;

		if (tm->fix.x_rescale) {
			x1 = (x1 > tm->fix.x_min) ? (x1 - tm->fix.x_min) : 0;
			x1 *= MAX_X;
			x1 /= (tm->fix.x_max - tm->fix.x_min);
			if (x1 >= MAX_X)
				x1 = MAX_X;
		}

		if (tm->fix.y_rescale) {
			y1 = (y1 > tm->fix.y_min) ? (y1 - tm->fix.y_min) : 0;
			y1 *= MAX_Y;
			y1 /= (tm->fix.y_max - tm->fix.y_min);
			if (y1 >= MAX_Y)
				y1 = MAX_Y;
		}

		if (tm->fix.x_invert)
			x1 = MAX_X - x1;

		if (tm->fix.y_invert)
			y1 = MAX_Y - y1;

		event = 1;
		input_report_abs(input, ABS_MT_TOUCH_MAJOR, MAX_Z);
		input_report_abs(input, ABS_MT_POSITION_X, x1);
		input_report_abs(input, ABS_MT_POSITION_Y, y1);
		input_mt_sync(input);
		TM1510_DEBUG("Finger1: (%d, %d)\n", x1, y1);
	}

	if (curr_touch >= 2) {
		u8 x2h, x2l, y2h, y2l;
		u32 x2, y2;

		x2h = tm->reg_data[TM_X2_HIGH];
		x2l = tm->reg_data[TM_XY2_LOW] & 0x0f;
		y2h = tm->reg_data[TM_Y2_HIGH];
		y2l = (tm->reg_data[TM_XY2_LOW] & 0xf0) >> 4;
		x2 = (x2h << 4) | x2l;
		y2 = (y2h << 4) | y2l;

		if (x2 > MAX_X)
			x2 = MAX_X;

		if (y2 > MAX_Y)
			y2 = MAX_Y;

		if (tm->fix.x_rescale) {
			x2 = (x2 > tm->fix.x_min) ? (x2 - tm->fix.x_min) : 0;
			x2 *= MAX_X;
			x2 /= (tm->fix.x_max - tm->fix.x_min);
			if (x2 >= MAX_X)
				x2 = MAX_X;
		}

		if (tm->fix.y_rescale) {
			y2 = (y2 > tm->fix.y_min) ? (y2 - tm->fix.y_min) : 0;
			y2 *= MAX_Y;
			y2 /= (tm->fix.y_max - tm->fix.y_min);
			if (y2 >= MAX_Y)
				y2 = MAX_Y;
		}

		if (tm->fix.x_invert)
			x2 = MAX_X - x2;

		if (tm->fix.y_invert)
			y2 = MAX_Y - y2;

		event = 1;
		input_report_abs(input, ABS_MT_TOUCH_MAJOR, MAX_Z);
		input_report_abs(input, ABS_MT_POSITION_X, x2);
		input_report_abs(input, ABS_MT_POSITION_Y, y2);
		input_mt_sync(input);
		TM1510_DEBUG("Finger2: (%d, %d)\n", x2, y2);
	}

	if (event)
		input_sync(input);
	prev_touch = curr_touch;
}

static irqreturn_t tm1510_irq(int irq, void *handle)
{
	struct tm1510 *tm = handle;

	if (tm->get_pendown_state && !tm->get_pendown_state())
		goto tm1510_irq_exit;

	if (tm->clear_penirq) {
		tm->clear_penirq();
	}

	queue_work(tm->workqueue, &tm->report_worker);

tm1510_irq_exit:
	return IRQ_HANDLED;
}

static void tm1510_report_worker(struct work_struct *work)
{
	struct tm1510	*tm;

	tm = container_of(work, struct tm1510, report_worker);

	tm1510_clear_irq(tm);
	tm1510_read_all(tm);
	tm1510_send_event(tm);
}

static int tm1510_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct input_dev 		*input_dev;
	struct tm1510 			*tm;
	struct tm1510_platform_data	*pdata;
	int				err;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "platform data is required!\n");
		return -EINVAL;
	}
	pdata->init_platform_hw();

	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_WRITE_BYTE_DATA | I2C_FUNC_SMBUS_WRITE_BYTE |
		I2C_FUNC_SMBUS_READ_BYTE))
		return -EIO;

	tm = kzalloc(sizeof(struct tm1510), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!tm || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	tm->client = client;
	i2c_set_clientdata(client, tm);
	tm->input = input_dev;
	tm->fix	= pdata->fix;
	tm->get_pendown_state = pdata->get_pendown_state;
	tm->clear_penirq = pdata->clear_penirq;
	snprintf(tm->phys, sizeof(tm->phys),
		 "%s/input0", dev_name(&client->dev));

	input_dev->name = "Synaptics TM1510 Touchscreen";
	input_dev->phys = tm->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, MAX_Z, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, MAX_Y, 0, 0);

	tm->workqueue = create_singlethread_workqueue("tm1510");
	INIT_WORK(&tm->report_worker, tm1510_report_worker);

	tm->irq = client->irq;
	err = request_irq(tm->irq, tm1510_irq, IRQF_TRIGGER_FALLING,
			client->dev.driver->name, tm);
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", tm->irq);
		goto err_free_mem;
	}

	err = input_register_device(input_dev);
	if (err)
		goto err_free_irq;

	err = tm1510_config_irq(tm);
	if (err)
		goto err_free_irq;

	err = tm1510_clear_irq(tm);
	if (err)
		goto err_free_irq;

	dev_info(&client->dev, "registered with irq (%d)\n", tm->irq);

	return 0;

 err_free_irq:
	free_irq(tm->irq, tm);
 err_free_mem:
	input_free_device(input_dev);
	kfree(tm);
	return err;
}

static int tm1510_remove(struct i2c_client *client)
{
	struct tm1510			*tm = i2c_get_clientdata(client);
	struct tm1510_platform_data	*pdata = client->dev.platform_data;

	pdata->exit_platform_hw();
	destroy_workqueue(tm->workqueue);
	free_irq(tm->irq, tm);
	input_unregister_device(tm->input);
	kfree(tm);

	return 0;
}

static struct i2c_device_id tm1510_idtable[] = {
	{ "tm1510", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tm1510_idtable);

static struct i2c_driver tm1510_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tm1510"
	},
	.id_table	= tm1510_idtable,
	.probe		= tm1510_probe,
	.remove		= tm1510_remove,
};

static int __init tm1510_init(void)
{
	return i2c_add_driver(&tm1510_driver);
}

static void __exit tm1510_exit(void)
{
	i2c_del_driver(&tm1510_driver);
}

module_init(tm1510_init);
module_exit(tm1510_exit);

MODULE_AUTHOR("Zhenwu Xue <zwxue@ambarella.com>");
MODULE_DESCRIPTION("Synaptics TM1510 TouchScreen Driver");
MODULE_LICENSE("GPL");
