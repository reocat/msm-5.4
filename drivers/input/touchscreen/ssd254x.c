/*
 *  "ssd254x"  touchscreen driver
 *	
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>


#include "ssd254x.h"

#define DRV_NAME		"ssd254x"

static unsigned short normal_i2c[] = { ssd254x_I2C_SLAVE_ADDR, I2C_CLIENT_END };

void deviceReset(struct i2c_client *client);
#if 0//TBD later
void deviceResume(struct i2c_client *client);
void deviceSuspend(struct i2c_client *client);
#endif
void SSD254xdeviceInit(struct i2c_client *client); 

static struct ssd254x_data ssd254x;
static bool sensitivity_flag = true;

int ReadRegister(struct i2c_client *client,uint8_t reg,int ByteNo)
{
	unsigned char buf[4];
	struct i2c_msg msg[2];
	int ret;

	memset(buf, 0xFF, sizeof(buf));
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;
	//msg[0].scl_rate = 400 * 1000;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = ByteNo;
	msg[1].buf = buf;
	//msg[1].scl_rate = 400 * 1000;

	ret = i2c_transfer(client->adapter, msg, 2);

	#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
	if(ret<0)	printk("		ReadRegister: i2c_transfer Error !\n");
	else		printk("		ReadRegister: i2c_transfer OK !\n");
	#endif
//	printk("buf[0]=0x%02x,buf[1]=0x%02x,buf[2]=0x%02x,buf[3]=0x%02x\n",buf[0],buf[1],buf[2],buf[3]);
	if(ByteNo==1) return (int)((unsigned int)buf[0]<<0);
	if(ByteNo==2) return (int)((unsigned int)buf[1]<<0)|((unsigned int)buf[0]<<8);
	if(ByteNo==3) return (int)((unsigned int)buf[2]<<0)|((unsigned int)buf[1]<<8)|((unsigned int)buf[0]<<16);
	if(ByteNo==4) return (int)((unsigned int)buf[3]<<0)|((unsigned int)buf[2]<<8)|((unsigned int)buf[1]<<16)|(buf[0]<<24);
	
	return 0;
}

void WriteRegister(struct i2c_client *client,uint8_t Reg,unsigned char Data1,unsigned char Data2,int ByteNo)
{	
	struct i2c_msg msg;
	unsigned char buf[4];
	int ret;

	buf[0]=Reg;
	buf[1]=Data1;
	buf[2]=Data2;
	buf[3]=0;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = ByteNo+1;
	msg.buf = (char *)buf;
	//msg.scl_rate = 400 * 1000;
	ret = i2c_transfer(client->adapter, &msg, 1);

	#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
	if(ret<0)	printk("		WriteRegister: i2c_master_send Error !\n");
	else		printk("		WriteRegister: i2c_master_send OK !\n");
	#endif
}

void SSD254xdeviceInit(struct i2c_client *client)
{	
	int i;
	for(i=0;i<sizeof(ssd254x_cfgTable)/sizeof(ssd254x_cfgTable[0]);i++)
                {
                       if(ssd254x_cfgTable[i].Reg == 0x00)
                	 {
                	 	msleep(ssd254x_cfgTable[i].Data1 * 256 + ssd254x_cfgTable[i].Data2);
                	 }
			 else
			 {
                        WriteRegister(  client,ssd254x_cfgTable[i].Reg,
                                ssd254x_cfgTable[i].Data1,ssd254x_cfgTable[i].Data2,
                                ssd254x_cfgTable[i].No);
			 }
                }
}

void SSD254xdevicePatch(struct i2c_client *client)
{	
	int i;
	for(i=0;i<sizeof(ssd60xxcfgPatch)/sizeof(ssd60xxcfgPatch[0]);i++)
                {
                	 if(ssd60xxcfgPatch[i].Reg == 0x00)
                	 {
                	 	msleep(ssd60xxcfgPatch[i].Data1 * 256 + ssd60xxcfgPatch[i].Data2);
                	 }
			 else
			 {
                        WriteRegister(  client,ssd60xxcfgPatch[i].Reg,
                                ssd60xxcfgPatch[i].Data1,ssd60xxcfgPatch[i].Data2,
                                ssd60xxcfgPatch[i].No);
			 }
                }
}


void deviceReset(struct i2c_client *client)
{
	
	int i;
	for(i=0;i<sizeof(Reset)/sizeof(Reset[0]);i++)
	{
		WriteRegister(	client,Reset[i].Reg,
				Reset[i].Data1,Reset[i].Data2,
				Reset[i].No);
	}
	msleep(100);   
}
#if 0//TBD later
void deviceResume(struct i2c_client *client)
{
	if (client == NULL) {
		return;
	}
	gpio_direction_output(ssd254x.gpio_reset, 1);
	msleep(5);
	gpio_direction_output(ssd254x.gpio_reset, 0);
	msleep(10);
	gpio_direction_output(ssd254x.gpio_reset, 1);
	msleep(100);
	SSD254xdeviceInit(client);
}

void deviceSuspend(struct i2c_client *client)
{	
	if(client == NULL)
        return;
	WriteRegister(	client,Suspend[0].Reg,
				Suspend[0].Data1,Suspend[0].Data2,
				Suspend[0].No);
	msleep(100);
	
	WriteRegister(	client,Suspend[1].Reg,
				Suspend[1].Data1,Suspend[1].Data2,
				Suspend[1].No);	
}
#endif
static int ssd254x_register_input(void)
{
	int ret;
	struct input_dev *dev;

	dev = ssd254x.input = input_allocate_device();
	if (dev == NULL)
		return -ENOMEM;

	dev->name = "ssd254x";

	set_bit(EV_KEY, dev->evbit);
	set_bit(EV_ABS, dev->evbit);
	set_bit(EV_SYN, dev->evbit);
	set_bit(BTN_TOUCH, dev->keybit);

	input_mt_init_slots(dev, FINGERNO, 0);
	input_set_abs_params(dev, ABS_MT_POSITION_X, 0,SCREEN_MAX_X, 0, 0);
	input_set_abs_params(dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	input_set_abs_params(dev, ABS_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(dev, ABS_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(dev, ABS_PRESSURE, 0, 255, 0, 0);

	ret = input_register_device(dev);
	if (ret < 0)
		goto bail1;

	return 0;

bail1:
	input_free_device(dev);
	return ret;
}

static int x1[FINGERNO] = {4095,4095,4095,4095,4095,4095,4095,4095,4095,4095};
static int y1[FINGERNO] = {4095,4095,4095,4095,4095,4095,4095,4095,4095,4095};

static void ssd254x_wq(struct work_struct *work)
{
	//struct input_dev *dev = ssd254x.input;

	int i;
	unsigned short xpos=0, ypos=0, width=0;
	int FingerInfo;
	int EventStatus;
	int FingerX[FINGERNO];
	int FingerY[FINGERNO];
	int FingerP[FINGERNO];

	#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
	printk("+-----------------------------------------+\n");
	printk("|	ssd254x_wq!                  |\n");
	printk("+-----------------------------------------+\n");
	#endif

	EventStatus = ReadRegister(ssd254x.client,ssd254x_EVENT_STATUS,2)>>4;

	ssd254x.FingerDetect=0;
	
	for(i=0;i<ssd254x.FingerNo;i++)
	{
		if((EventStatus>>i)&0x1)
		{
			 FingerInfo=ReadRegister(ssd254x.client,ssd254x_FINGER01_REG+i,4);
			 ypos = ((FingerInfo>>0)&0xF00)|((FingerInfo>>16)&0xFF);
			 xpos = ((FingerInfo>>4)&0xF00)|((FingerInfo>>24)&0xFF);
			 width= FingerInfo & 0x0FF;	
			
			 if(xpos!=0xFFF)
			 {
					ssd254x.FingerDetect++;
			 }
			 else 
			 {
					EventStatus=EventStatus&~(1<<i);
			 }
		}
		else
		{
			 xpos=ypos=0xFFF;
			 width=0;
		}
		FingerX[i]=xpos;
		FingerY[i]=ypos;
		FingerP[i]=width;
	}

	enable_irq(ssd254x.client->irq);
				
	for(i=0;i<ssd254x.FingerNo;i++)
	{
		xpos=FingerX[i];
		ypos=FingerY[i];
		width=FingerP[i];
		
		if(xpos!=0xFFF)
		{
			if (sensitivity_flag)
			{
				WriteRegister(ssd254x.client,0x34,0xC6,0x80,2);
			}
			sensitivity_flag = false;
                   
                   #ifdef ProtocolB
                        input_mt_slot(ssd254x.input, i);
                        input_report_abs(ssd254x.input, ABS_MT_TRACKING_ID, i);
                        input_report_abs(ssd254x.input, ABS_MT_POSITION_X, xpos);
                        input_report_abs(ssd254x.input, ABS_MT_POSITION_Y, ypos);
                        input_report_abs(ssd254x.input, ABS_MT_WIDTH_MAJOR, width); 
                        input_report_abs(ssd254x.input, ABS_MT_PRESSURE, width);
                        input_report_abs(ssd254x.input, ABS_MT_TOUCH_MAJOR, width);                                               
                   #else
                        input_report_abs(ssd254x.input, ABS_MT_TOUCH_MAJOR, 1);
                        input_report_abs(ssd254x.input, ABS_MT_POSITION_X, xpos);
                        input_report_abs(ssd254x.input, ABS_MT_POSITION_Y, ypos);
                        input_report_abs(ssd254x.input, ABS_MT_WIDTH_MAJOR, width);
                        input_report_abs(ssd254x.input, ABS_MT_PRESSURE, 1);
                        input_report_abs(ssd254x.input, BTN_TOUCH,1);
                        input_report_abs(ssd254x.input, ABS_MT_TRACKING_ID, i);
                        input_mt_sync(ssd254x.input);
                   #endif
			x1[i] = xpos;
			y1[i] = ypos;
		}
		else if(ssd254x.FingerX[i]!=0xFFF)
		{			
				
			WriteRegister(ssd254x.client,0x34,0xC6,0x80,2);
			sensitivity_flag = true;

                      #ifdef ProtocolB
                        input_mt_slot(ssd254x.input, i);
                        input_report_abs(ssd254x.input, ABS_MT_TRACKING_ID, -1);
                        input_mt_report_slot_state(ssd254x.input, MT_TOOL_FINGER, false);
                      #else
                        input_report_abs(ssd254x.input, ABS_MT_POSITION_X, x1[i]);
                        input_report_abs(ssd254x.input, ABS_MT_POSITION_Y, y1[i]);
                        input_report_abs(ssd254x.input, ABS_MT_TRACKING_ID, i);
                        input_report_abs(ssd254x.input, ABS_MT_WIDTH_MAJOR, width);
                        input_report_abs(ssd254x.input, ABS_MT_TOUCH_MAJOR, 0);
                        input_report_abs(ssd254x.input, ABS_MT_PRESSURE, 0);
                        input_report_abs(ssd254x.input, BTN_TOUCH,0);
                        input_mt_sync(ssd254x.input);
                      #endif
		}
		ssd254x.FingerX[i]=FingerX[i];
		ssd254x.FingerY[i]=FingerY[i];
		ssd254x.FingerP[i]=width;

	}		
	ssd254x.EventStatus=EventStatus;
		
	input_sync(ssd254x.input);
}
static DECLARE_WORK(ssd254x_work, ssd254x_wq);

static irqreturn_t ssd254x_interrupt(int irq, void *dev_id)
{
	disable_irq_nosync(ssd254x.client->irq);

	queue_work(ssd254x.workq, &ssd254x_work);

	return IRQ_HANDLED;
}

static int of_ssd254x_get_pins(struct device_node *np,
				unsigned int *int_pin, unsigned int *reset_pin)
{
	if (of_gpio_count(np) < 2)
		return -ENODEV;

	*int_pin = of_get_gpio(np, 0);
	*reset_pin = of_get_gpio(np, 1);
    printk(KERN_INFO "int: %i, reset: %i\n", *int_pin, *reset_pin);
	if (!gpio_is_valid(*int_pin) || !gpio_is_valid(*reset_pin)) {
		pr_err("%s: invalid GPIO pins, int=%d/reset=%d\n",
		       np->full_name, *int_pin, *reset_pin);
		return -ENODEV;
	}

	return 0;
}

static int ssd254x_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct device_node *np = i2c->dev.of_node;
	struct ssd254x_data *pdata = i2c->dev.platform_data;
	int ret;

	#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
	printk("+-----------------------------------------+\n");
	printk("|	ssd254x_probe!                 |\n");
	printk("+-----------------------------------------+\n");
	#endif

	/* Check functionality */
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&i2c->dev, "%s adapter not supported\n",
			dev_driver_string(&i2c->adapter->dev));
		return -ENODEV;
	}

	if (np != NULL) {
		pdata = i2c->dev.platform_data =
			devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			dev_err(&i2c->dev, "No platform data for Fusion driver\n");
			return -ENODEV;
		}
		ret = of_ssd254x_get_pins(i2c->dev.of_node,
				&pdata->gpio_int, &pdata->gpio_reset);
		if (ret)
			return ret;
	}
	else if (pdata == NULL) {
		dev_err(&i2c->dev, "No platform data for SSL SSD254x driver\n");
		return -ENODEV;
	}

	if ((gpio_request(pdata->gpio_int, "SSL interrupt") == 0) &&
	    (gpio_direction_input(pdata->gpio_int) == 0)) {
		gpio_export(pdata->gpio_int, 0);
	} else {
		dev_warn(&i2c->dev, "Could not obtain GPIO for interrupt\n");
		return -ENODEV;
	}

	if ((gpio_request(pdata->gpio_reset, "SSL reset") == 0) &&
	    (gpio_direction_output(pdata->gpio_reset, 0) == 0)) {
		//gpio_set_value(pdata->gpio_reset, 1);
		gpio_set_value_cansleep(pdata->gpio_reset, 1);
		msleep(5);
		//gpio_set_value(pdata->gpio_reset, 0);
		gpio_set_value_cansleep(pdata->gpio_reset, 0);
		msleep(10);		
		//gpio_set_value(pdata->gpio_reset, 1);
		gpio_set_value_cansleep(pdata->gpio_reset, 1);
		msleep(100);

		//gpio_export(pdata->gpio_reset, 0);
	} else {
        printk(KERN_INFO "Could not obtain GPIO for Fusion reset\n");
		dev_warn(&i2c->dev, "Could not obtain GPIO for Fusion reset\n");
		ret = -ENODEV;
		goto bail0;
	}

	ssd254x.FingerNo = FINGERNO;
	
	/* Use INT GPIO as sampling interrupt */
	i2c->irq = gpio_to_irq(pdata->gpio_int);
	irq_set_irq_type(i2c->irq, IRQ_TYPE_LEVEL_LOW);
	if(!i2c->irq)
	{
		dev_err(&i2c->dev, "ssd254x irq < 0 \n");
		ret = -ENOMEM;
		goto bail1;
	}

	/* Attach the I2C client */
	ssd254x.client =  i2c;
	i2c_set_clientdata(i2c, &ssd254x);
	dev_info(&i2c->dev, "Touchscreen registered with bus id (%d) with slave address 0x%x\n",
			i2c_adapter_id(ssd254x.client->adapter),	ssd254x.client->addr);

	/* Register the input device. */
	ret = ssd254x_register_input();
	if (ret < 0) {
		dev_err(&i2c->dev, "can't register input: %d\n", ret);
		goto bail1;
	}
	deviceReset(ssd254x.client);
	ssd254x.input->id.product = ReadRegister(ssd254x.client, ssd254x_DEVICE_ID_REG,2);
	ssd254x.input->id.version = ReadRegister(ssd254x.client, ssd254x_VERSION_ID_REG,2);

	printk(KERN_INFO "SSL Touchscreen Device ID  : 0x%04X\n",ssd254x.input->id.product);
	printk(KERN_INFO "SSL Touchscreen Version ID : 0x%04X\n",ssd254x.input->id.version);

	//SSD254xdevicePatch(ssd254x.client);
	SSD254xdeviceInit(ssd254x.client);

	/* Create a worker thread */
	ssd254x.workq = create_singlethread_workqueue(DRV_NAME);
	if (ssd254x.workq == NULL) {
		dev_err(&i2c->dev, "can't create work queue\n");
		ret = -ENOMEM;
		goto bail2;
	}

	/* Register for the interrupt and enable it. Our handler will
	*  start getting invoked after this call. */
	ret = request_irq(i2c->irq, ssd254x_interrupt, IRQF_TRIGGER_LOW,
	i2c->name, &ssd254x);
	if (ret < 0) {
		dev_err(&i2c->dev, "can't get irq %d: %d\n", i2c->irq, ret);
		goto bail3;
	}

	return 0;

	free_irq(i2c->irq, &ssd254x);

bail3:
	destroy_workqueue(ssd254x.workq);
	ssd254x.workq = NULL;

bail2:
	input_unregister_device(ssd254x.input);
bail1:
	gpio_free(pdata->gpio_reset);
bail0:
	gpio_free(pdata->gpio_int);

	return ret;
}


#ifdef CONFIG_PM_SLEEP
static int ssd254x_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	disable_irq(i2c->irq);
	flush_workqueue(ssd254x.workq);

	return 0;
}

static int ssd254x_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	enable_irq(i2c->irq);

	return 0;
}
#endif

static int ssd254x_remove(struct i2c_client *i2c)
{
	struct ssd254x_data *pdata = i2c->dev.platform_data;

	gpio_free(pdata->gpio_int);
	gpio_free(pdata->gpio_reset);
	destroy_workqueue(ssd254x.workq);
	free_irq(i2c->irq, &ssd254x);
	input_unregister_device(ssd254x.input);
	i2c_set_clientdata(i2c, NULL);

	dev_info(&i2c->dev, "driver removed\n");
	
	return 0;
}

static struct i2c_device_id ssd254x_id[] = {
	{"ssd254x", 0},
	{},
};

static const struct of_device_id ssd254x_dt_ids[] = {
	{
		.compatible = "solomon,ssd254x",
	}, 
	{	}
};
MODULE_DEVICE_TABLE(of, ssd254x_dt_ids);

static const struct dev_pm_ops ssd254x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ssd254x_suspend, ssd254x_resume)
};

static struct i2c_driver ssd254x_i2c_drv = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= DRV_NAME,
		.pm		= &ssd254x_pm_ops,
		.of_match_table = ssd254x_dt_ids,
	},
	.probe          = ssd254x_probe,
	.remove         = ssd254x_remove,
	.id_table       = ssd254x_id,
	.address_list   = normal_i2c,
};

//module_i2c_driver(ssd254x_i2c_drv);

static int __init ssd254x_init( void )
{
	int ret;

	printk(" ssd254x init\n");
	memset(&ssd254x, 0, sizeof(ssd254x));

	/* Probe for ssd254x on I2C. */
	ret = i2c_add_driver(&ssd254x_i2c_drv);
	if (ret < 0) {
		printk(KERN_WARNING DRV_NAME " can't add i2c driver: %d\n", ret);
	}

	return ret;
}

static void __exit ssd254x_exit( void )
{
	i2c_del_driver(&ssd254x_i2c_drv);
}
late_initcall(ssd254x_init);
module_exit(ssd254x_exit);

MODULE_DESCRIPTION("ssd254x Touchscreen Driver");
MODULE_LICENSE("GPL");
