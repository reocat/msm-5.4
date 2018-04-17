/**
 * History:
 *    2017/04/27 - [RamKumar] created file
 *
 * Copyright (C) 2012-2012, Ambarella, Inc.
 *
 * All rights reserved. No Part of this file may be reproduced, stored
 * in a retrieval system, or transmitted, in any form, or by any means,
 * electronic, mechanical, photocopying, recording, or otherwise,
 * without the prior consent of Ambarella, Inc.
 */

#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <plat/rpdev.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static struct  rpdev *relay_device_node;

static char *msg_to_relay = "Hello From Linux-2";

static int rpmsg_relay_set_example(const char *val, const struct kernel_param *kp)
{
       char *data = strstrip((char *)val);

       rpdev_send(relay_device_node, data, strlen(data) + 1);

       return param_set_charp(data, kp);
}
static int rpmsg_relay_get_example (char *val, const struct kernel_param *kp) {
	printk("Echo some text to file relay ");
	return 0;
}
static struct kernel_param_ops param_ops_example_printk = {
       .set = rpmsg_relay_set_example,
       .get = rpmsg_relay_get_example,
       .free = param_free_charp,
};

module_param_cb(msg_to_relay, &param_ops_example_printk,
       &(msg_to_relay), 0644);
MODULE_PARM_DESC(msg_to_relay, "rpmsg_relay_example");

static void rpdev_relay_cb(struct rpdev *rpdev, void *data, int len, void *priv, u32 src)
{
	printk("----->[ %20s ] message From RTOS: [%s]\n", __func__, (char *)data);
}

static void rpdev_relay_init_work(struct work_struct *work)
{
	struct rpdev *rpdev;
	char *str = "guest2 IPC ready!";
	char buf[64];

	rpdev = rpdev_alloc("relay_to_rtos", 0, rpdev_relay_cb, NULL);

	rpdev_register(rpdev, "ca9_a_and_b");

	relay_device_node = rpdev;

	printk("[ %20s ] send message: [%s]\n", __func__, str);

	sprintf(buf, "%s", str);
	printk("try sending: %s   OK\n", buf);
	/*Send some data to update rpmsg channel address on relay Server @ Linux1(Host)*/
	while (rpdev_trysend(rpdev, buf, strlen(buf) + 1) != 0) {
		printk("retry sending: %s\n", buf);
		msleep(10);
	}
}

static struct work_struct work;

static int rpdev_relay_init(void)
{
	INIT_WORK(&work, rpdev_relay_init_work);
	schedule_work(&work);
	return 0;
}

static void rpdev_relay_fini(void)
{
	printk(" Terminate ==> Rpdev Relay Demo ");
}

module_init(rpdev_relay_init);
module_exit(rpdev_relay_fini);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("rpmsg clnt example to relay via rpmsg_host_relay program to a remote client");
