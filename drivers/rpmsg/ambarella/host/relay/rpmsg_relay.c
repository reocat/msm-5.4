/*
 * Copyright (C) 2012-2016, Ambarella, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg.h>


static struct rpmsg_device  *c0rtos;
static struct rpmsg_device  *c2linux;

static int rpmsg_relay_cb(struct rpmsg_device *rpdev, void *data, int len, void *priv, u32 src) {


	/*
	 * Message we recv from a given rpmsg device is passed to other rpmsg deivice
	 * complex relay mechanism such as one-to-one or one-to-many can also be achived by this way.
	 * */

	/*printk("====>[ %20s ] recv msg: [%s] from 0x%x and len %d\n", __func__, (const char*)data, src, len);*/
	if (strcmp(rpdev->id.name, "relay_to_linux2")) {
		if (c2linux) 
			return rpmsg_send(c2linux->ept, data, len);
		else
			printk("rpmsg relay address table incomplete\n");
	
	}
	if (strcmp(rpdev->id.name, "relay_to_rtos")) {
		if (c0rtos) 
			return rpmsg_send(c0rtos->ept, data, len);
		else
			printk("rpmsg relay address table incomplete\n");
	}
	return 0;
}

static int rpmsg_relay_probe(struct rpmsg_device *rpdev) {
	int ret = 0;
	struct rpmsg_channel_info chinfo;

	strncpy(chinfo.name, rpdev->id.name, sizeof(chinfo.name));
	chinfo.src = RPMSG_ADDR_ANY;
	chinfo.dst = rpdev->dst;

	/*printk("====>probe:::[ %20s ] from 0x%x \n", __func__,  rpdev->src);*/
	rpmsg_send(rpdev->ept, &chinfo, sizeof(chinfo));
	if (strcmp(rpdev->id.name, "relay_to_linux2")) {
		printk("rpmsg_relay::::relay_to_linux2\n");
		c0rtos = rpdev;
	}
	if (strcmp(rpdev->id.name, "relay_to_rtos")) {
		printk("rpmsg_relay::::relay_to_rtos\n");
		c2linux = rpdev;
	}
	return ret;
}

static void rpmsg_relay_remove(struct rpmsg_device *rpdev) {
	printk("RPMSG relay driver: GoodBye.. ");
}

static struct rpmsg_device_id rpmsg_relay_device_id_table[] = {
	{ .name	= "relay_to_rtos", },
	{ .name	= "relay_to_linux2", },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_relay_device_id_table);

static struct rpmsg_driver rpmsg_relay_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_relay_device_id_table,
	.probe		= rpmsg_relay_probe,
	.remove		= rpmsg_relay_remove,
	.callback	= rpmsg_relay_cb,
};

static int __init rpmsg_relay_init(void) {
	return register_rpmsg_driver(&rpmsg_relay_driver);
}

static void __exit rpmsg_relay_terminate(void) {
	unregister_rpmsg_driver(&rpmsg_relay_driver);
}

module_init(rpmsg_relay_init);
module_exit(rpmsg_relay_terminate);
MODULE_DESCRIPTION("RPMSG Relay Agent");
MODULE_LICENSE("GPL v2");
