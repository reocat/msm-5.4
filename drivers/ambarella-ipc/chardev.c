/*
 * drivers/ambarella-ipc/chardev.c
 *
 * Ambarella IPC in Linux kernel-space.
 *
 * Authors:
 *	Charles Chiou <cchiou@ambarella.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * Copyright (C) 2009-2010, Ambarella Inc.
 */

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <ipc/ipc.h>


//#define AIPCDEV_MAJOR	250	/* Uncomment to use dynamic allocation */
#define AIPCDEV_MINOR	240

static unsigned int aipcdev_poll(struct file *, struct poll_table_struct *);
static int aipcdev_ioctl(struct inode *, struct file *,
			 unsigned int, unsigned long);
static int aipcdev_open(struct inode *, struct file *);
static int aipcdev_fasync(int fd, struct file *filp, int on);
static int aipcdev_release(struct inode *, struct file *);

/*
 * Ambarella IPC kernel-space driver as char device.
 */
struct aipcdev_s
{
	dev_t dev;
	struct cdev *cdev;		/* Char dev structure */
	struct fasync_struct *fasync;
	struct list_head xprt_head;
};

/*
 * File ops.
 */
static struct file_operations aipcdev_fops =
{
	.owner =	THIS_MODULE,
	.poll =		aipcdev_poll,
	.ioctl =	aipcdev_ioctl,
	.fasync =	aipcdev_fasync,
	.open =		aipcdev_open,
	.release =	aipcdev_release,
};

static struct aipcdev_s G_aipcdev;
static struct aipcdev_s *aipcdev = &G_aipcdev;

/*
 * fasync()
 */
static int aipcdev_fasync(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &aipcdev->fasync);

	return retval < 0 ? retval : 0;
}

/*
 * poll()
 */
static unsigned int aipcdev_poll(struct file *filp,
				 struct poll_table_struct *wait)
{
	return 0;
}

/*
 * ioctl()
 */
static int aipcdev_ioctl(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
	case AIPC_IOCRESET:
	case AIPC_IOCCONNCLIENT:
	case AIPC_IOCCONNSERVER:
	case AIPC_IOCDISCCLIENT:
	case AIPC_IOCDISCSERVER:
	default:
		break;
	}

	return 0;
}

/*
 * open()
 */
static int aipcdev_open(struct inode *inode, struct file *filp)
{
	filp->private_data = aipcdev;

	return 0;
}

/*
 * release()/close()
 */
static int aipcdev_release(struct inode *inode, struct file *filp)
{
	aipcdev_fasync(-1, filp, 0);

	filp->private_data = NULL;

	return 0;
}

/*
 * Device initialization.
 */
int ipc_chardev_init(void)
{
	int rval;

#if defined(AIPCDEV_MAJOR)
	aipcdev->dev = MKDEV(AIPCDEV_MAJOR, AIPCDEV_MINOR);
	rval = register_chrdev_region(aipcdev->dev, 1, "aipcdev");
	if (rval != 0) {
		printk(KERN_NOTICE
		       "error: register_chrdev_region() failed %d\n",
		       rval);
		return rval;
	}
#else
	rval = alloc_chrdev_region(&aipcdev->dev, AIPCDEV_MINOR, 1, "aipcdev");
	if (rval != 0) {
		printk(KERN_NOTICE
		       "error: alloc_chrdev_region() failed %d\n",
		       rval);
		return rval;
	}
#endif

	aipcdev->cdev = cdev_alloc();
	cdev_init(aipcdev->cdev, &aipcdev_fops);
	aipcdev->cdev->owner = THIS_MODULE;
	aipcdev->cdev->ops = &aipcdev_fops;

	rval = cdev_add(aipcdev->cdev, aipcdev->dev, 1);
	if (rval) {
		printk(KERN_NOTICE "error: cdev_add() failed %d\n", rval);
#if defined(AIPCDEV_MAJOR)
		unregister_chrdev(AIPCDEV_MAJOR, "aipcdev");
#else
		unregister_chrdev_region(aipcdev->dev, 1);
#endif
		return rval;
	}

	INIT_LIST_HEAD(&aipcdev->xprt_head);

	return 0;
}

/*
 * Cleanup.
 */
void ipc_chardev_cleanup(void)
{
	if (aipcdev->cdev) {
		cdev_del(aipcdev->cdev);
#if defined(AIPCDEV_MAJOR)
		unregister_chrdev(AIPCDEV_MAJOR, "aipcdev");
#else
		unregister_chrdev_region(aipcdev->dev, 1);
#endif
	}
}

void aipc_notify_user(SVCXPRT *svcxprt)
{
	if (aipcdev) {
		INIT_LIST_HEAD(&svcxprt->u.flag.node);
		list_add_tail(&svcxprt->u.flag.node, &aipcdev->xprt_head);
		if (aipcdev->fasync)
			kill_fasync(&aipcdev->fasync, SIGIO, POLL_IN);
	}
}

EXPORT_SYMBOL(aipc_notify_user);

