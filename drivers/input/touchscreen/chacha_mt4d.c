/*
 * drivers/input/touchscreen/chacha_mt4d.c
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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
#include <linux/i2c/chacha_mt4d.h>

//#define DEBUG_CHACHA_MT4D

#ifdef	DEBUG_CHACHA_MT4D
#define CHACHA_MT4D_DEBUG(format, arg...)	printk(format , ## arg)
#else
#define CHACHA_MT4D_DEBUG(format, arg...)
#endif

#define MAX_X	4096
#define MAX_Y	6000
#define MAX_Z	16

typedef enum {
	CM_STATUS = 0,
	CM_X1_LOW,
	CM_X1_HIGH,
	CM_Y1_LOW,
	CM_Y1_HIGH,
	CM_X2_LOW,
	CM_X2_HIGH,
	CM_Y2_LOW,
	CM_Y2_HIGH,
	CM_PRESSURE_LOW,
	CM_PRESSURE_HIGH,
} chacha_mt4d_sub_addr_t;

#define NUM_DATA			11

struct chacha_mt4d {
	char				phys[32];
	struct input_dev		*input;
	struct i2c_client		*client;
	struct workqueue_struct 	*workqueue;
	struct work_struct		report_worker;
	u8				reg_data[NUM_DATA];
	int				irq;
	struct chacha_mt4d_fix_data	fix;
	int				(*get_pendown_state)(void);
	void				(*clear_penirq)(void);
};

static inline int chacha_mt4d_calibrate(struct chacha_mt4d *cm)
{
	struct i2c_msg		msg;
	int			errorCode;
	u8			buf[4];

	msg.addr = cm->client->addr;
	msg.flags = cm->client->flags;
	msg.len = 4;
	buf[0] = 0x13;
	buf[1] = 0x01;
	buf[2] = 0x40;
	buf[3] = 0xf3;
	msg.buf = buf;

	errorCode = i2c_transfer(cm->client->adapter, &msg, 1);
	if (errorCode != 1) {
		printk("Chacha-mt4d: Can't calibrate touch pannel.\n");
		return -EIO;
	}

	return 0;
}

static inline int chacha_mt4d_read_all(struct chacha_mt4d *cm)
{
	struct i2c_msg		msg;
	int			errorCode;

	msg.addr = cm->client->addr;
	msg.flags = I2C_M_RD | cm->client->flags;
	msg.len = NUM_DATA;
	msg.buf = cm->reg_data;

	errorCode = i2c_transfer(cm->client->adapter, &msg, 1);
	if (errorCode != 1) {
		printk("Chacha-mt4d: Can't read reg data.\n");
		return -EIO;
	}

	return 0;
}

static void chacha_mt4d_send_event(struct chacha_mt4d *cm)
{
	struct input_dev	*input = cm->input;
	static int		prev_touch = 0;
	static int		curr_touch = 0;
	int			event = 0;

	curr_touch = cm->reg_data[CM_STATUS] & 0x03;

	/* Button Pressed */
	if (!prev_touch && curr_touch) {
		input_report_key(input, BTN_TOUCH, 1);
		CHACHA_MT4D_DEBUG("Finger Pressed\n");
	}

	/* Button Released */
	if (prev_touch && !curr_touch) {
		event = 1;
		input_report_key(input, BTN_TOUCH, 0);
		CHACHA_MT4D_DEBUG("Finger Released\n\n\n");
	}

	if (curr_touch) {
		u8 x1h, x1l, y1h, y1l;
		u32 x1, y1;

		x1h = cm->reg_data[CM_X1_HIGH];
		x1l = cm->reg_data[CM_X1_LOW];
		y1h = cm->reg_data[CM_Y1_HIGH];
		y1l = cm->reg_data[CM_Y1_LOW];
		x1 = (x1h << 8) | x1l;
		y1 = (y1h << 8) | y1l;

		if (x1 > MAX_X)
			x1 = MAX_X;

		if (y1 > MAX_Y)
			y1 = MAX_Y;

		if (cm->fix.x_rescale) {
			x1 = (x1 > cm->fix.x_min) ? (x1 - cm->fix.x_min) : 0;
			x1 *= MAX_X;
			x1 /= (cm->fix.x_max - cm->fix.x_min);
			if (x1 >= MAX_X)
				x1 = MAX_X;
		}

		if (cm->fix.y_rescale) {
			y1 = (y1 > cm->fix.y_min) ? (y1 - cm->fix.y_min) : 0;
			y1 *= MAX_Y;
			y1 /= (cm->fix.y_max - cm->fix.y_min);
			if (y1 >= MAX_Y)
				y1 = MAX_Y;
		}

		if (cm->fix.x_invert)
			x1 = MAX_X - x1;

		if (cm->fix.y_invert)
			y1 = MAX_Y - y1;

		event = 1;
		input_report_abs(input, ABS_PRESSURE, MAX_Z);
		input_report_abs(input, ABS_X, x1);
		input_report_abs(input, ABS_Y, y1);
		CHACHA_MT4D_DEBUG("Finger1: (%d, %d)\n", x1, y1);
	}

	if (event)
		input_sync(input);
	prev_touch = curr_touch;
}

static irqreturn_t chacha_mt4d_irq(int irq, void *handle)
{
	struct chacha_mt4d *cm = handle;

	if (cm->get_pendown_state && !cm->get_pendown_state())
		goto chacha_mt4d_irq_exit;

	disable_irq(irq);
	queue_work(cm->workqueue, &cm->report_worker);

chacha_mt4d_irq_exit:
	return IRQ_HANDLED;
}

static void chacha_mt4d_report_worker(struct work_struct *work)
{
	struct chacha_mt4d	*cm;

	cm = container_of(work, struct chacha_mt4d, report_worker);

	chacha_mt4d_read_all(cm);
	chacha_mt4d_send_event(cm);

	if (cm->clear_penirq) {
		cm->clear_penirq();
	}
	enable_irq(cm->irq);
}

static int chacha_mt4d_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct input_dev 			*input_dev;
	struct chacha_mt4d 			*cm;
	struct chacha_mt4d_platform_data	*pdata;
	int					err;

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

	cm = kzalloc(sizeof(struct chacha_mt4d), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!cm || !input_dev) {
		err = -ENOMEM;
		goto err_free_mem;
	}

	cm->client = client;
	i2c_set_clientdata(client, cm);
	cm->input = input_dev;
	cm->fix	= pdata->fix;
	cm->get_pendown_state = pdata->get_pendown_state;
	cm->clear_penirq = pdata->clear_penirq;
	snprintf(cm->phys, sizeof(cm->phys),
		 "%s/input0", dev_name(&client->dev));

	input_dev->name = "Chacha mt4d Touchscreen";
	input_dev->phys = cm->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(input_dev, ABS_PRESSURE, 0, MAX_Z, 0, 0);
	input_set_abs_params(input_dev, ABS_X, 0, MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, MAX_Y, 0, 0);

	cm->workqueue = create_singlethread_workqueue("chacha_mt4d");
	INIT_WORK(&cm->report_worker, chacha_mt4d_report_worker);

	cm->irq = client->irq;
	err = request_irq(cm->irq, chacha_mt4d_irq, IRQF_TRIGGER_FALLING,
			client->dev.driver->name, cm);
	if (err < 0) {
		dev_err(&client->dev, "irq %d busy?\n", cm->irq);
		goto err_free_mem;
	}

	err = input_register_device(input_dev);
	if (err)
		goto err_free_irq;

	dev_info(&client->dev, "registered with irq (%d)\n", cm->irq);

	return 0;

 err_free_irq:
	free_irq(cm->irq, cm);
 err_free_mem:
	input_free_device(input_dev);
	kfree(cm);
	return err;
}

static int chacha_mt4d_remove(struct i2c_client *client)
{
	struct chacha_mt4d			*cm = i2c_get_clientdata(client);
	struct chacha_mt4d_platform_data	*pdata = client->dev.platform_data;

	pdata->exit_platform_hw();
	destroy_workqueue(cm->workqueue);
	free_irq(cm->irq, cm);
	input_unregister_device(cm->input);
	kfree(cm);

	return 0;
}

static struct i2c_device_id chacha_mt4d_idtable[] = {
	{ "chacha_mt4d", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, chacha_mt4d_idtable);

static struct i2c_driver chacha_mt4d_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "chacha_mt4d"
	},
	.id_table	= chacha_mt4d_idtable,
	.probe		= chacha_mt4d_probe,
	.remove		= chacha_mt4d_remove,
};

static int __init chacha_mt4d_init(void)
{
	return i2c_add_driver(&chacha_mt4d_driver);
}

static void __exit chacha_mt4d_exit(void)
{
	i2c_del_driver(&chacha_mt4d_driver);
}

module_init(chacha_mt4d_init);
module_exit(chacha_mt4d_exit);

MODULE_AUTHOR("Zhenwu Xue <zwxue@ambarella.com>");
MODULE_DESCRIPTION("Chacha mt4d TouchScreen Driver");
MODULE_LICENSE("GPL");
