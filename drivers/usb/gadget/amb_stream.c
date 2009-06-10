/*
* amb_stream.c -- Ambarella Data Streaming Gadget
*
* History:
*	2009/01/01 - [Cao Rongrong] created file
*
* Copyright (C) 2008 by Ambarella, Inc.
* http://www.ambarella.com
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
* along with this program; if not, write to the
* Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA  02111-1307, USA.
*/


/*
 * Ambarella Data Streaming Gadget only needs two bulk endpoints, and it
 * supports single configurations. 
 *
 * Module options include:
 *   buflen=N	default N=4096, buffer size used
 *
 */

//#define DEBUG 1
// #define VERBOSE

#include <linux/utsname.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"

#include <mach/hardware.h>

/*-------------------------------------------------------------------------*/

#define DRIVER_VERSION		"1 Jan 2009"

static const char shortname [] = "g_amb_stream";
static const char longname [] = "Ambarella Data Streaming Gadget";

static const char source_sink [] = "bulk in and out data";


#define xprintk(d,level,fmt,args...) \
	dev_printk(level , &(d)->gadget->dev , fmt , ## args)

#ifdef DEBUG
#define DBG(dev,fmt,args...) \
	xprintk(dev , KERN_DEBUG , fmt , ## args)
#else
#define DBG(dev,fmt,args...) \
	do { } while (0)
#endif /* DEBUG */

#ifdef VERBOSE
#define VDBG	DBG
#else
#define VDBG(dev,fmt,args...) \
	do { } while (0)
#endif /* VERBOSE */

#define ERROR(dev,fmt,args...) \
	xprintk(dev , KERN_ERR , fmt , ## args)
#define _WARN(dev,fmt,args...) \
	xprintk(dev , KERN_WARNING , fmt , ## args)
#define INFO(dev,fmt,args...) \
	xprintk(dev , KERN_INFO , fmt , ## args)


/*-------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*/

#define AMB_GADGET_MAJOR		AMBA_DEV_MAJOR
#define AMB_GADGET_MINOR_START		(AMBA_DEV_MINOR_PUBLIC_START + 0)

/* TOKEN */
#define AMB_COMMAND_TOKEN		0x55434D44
#define AMB_STATUS_TOKEN		0x55525350
#define AMB_ACK_TOKEN		0x5541434b

#define AMB_RSP_SUCCESS		0
#define AMB_RSP_FAILED		1

#define	AMB_RSP_NO_CONN		0
#define AMB_RSP_CONNECT		1


#define AMB_ACK_SUCCESS		0x00000000
#define AMB_ACK_FAILURE		0x00000001

#define USB_CMD_RECV_REQUEST	0x00000006

#define SIMPLE_CMD_SIZE		32

#define AG_NOTIFY_INTERVAL		5	/* 1 << 5 == 32 msec */
#define AG_NOTIFY_MAXPACKET		8

static struct cdev ag_cdev;

struct amb_cmd {
	u32 signature;
	u32 command;
	u32 parameter[(SIMPLE_CMD_SIZE / sizeof(u32)) - 2];
};

struct amb_rsp {
	u32 signature;
	u32 response;
	u32 parameter0;
	u32 parameter1;
};

struct amb_ack {
	u32 signature;
	u32 acknowledge;
	u32 parameter0;
	u32 parameter1;
};

/* for bNotifyType */
#define PORT_STATUS_CHANGE		0x55
#define PORT_NOTIFY_IDLE		0xff
/* for value */
#define PORT_FREE_ALL			2
#define PORT_CONNECT			1
#define PORT_NO_CONNECT		0
/* for status */
#define DEVICE_OPENED			1
#define DEVICE_NOT_OPEN		0

struct amb_notify {
	u16	bNotifyType;
	u16	port_id;
	u16	value;
	u16	status;
};

struct amb_notify g_port = {
	.bNotifyType = PORT_NOTIFY_IDLE,
	.port_id = 0xffff,
	.value = 0,
};


/*-------------------------------------------------------------------------*/

#define AMB_DATA_STREAM_MAGIC	'u'
#define AMB_DATA_STREAM_WR_RSP	_IOW(AMB_DATA_STREAM_MAGIC, 1, struct amb_rsp *)
#define AMB_DATA_STREAM_RD_CMD	_IOR(AMB_DATA_STREAM_MAGIC, 1, struct amb_cmd *)
#define AMB_DATA_STREAM_STATUS_CHANGE	_IOW(AMB_DATA_STREAM_MAGIC, 2, struct amb_notify *)

/*-------------------------------------------------------------------------*/

/* big enough to hold our biggest descriptor */
#define USB_BUFSIZ	256
#define Q_DEPTH		5

struct amb_dev {
	spinlock_t		lock;
	struct usb_gadget	*gadget;
	struct usb_request	*ctrl_req;	/* for control responses */
	struct usb_request	*notify_req;

	/* when configured, we have one of two configs:
	 * - source data (in to host) and sink it (out from host)
	 * - or loop it back (out from host back in to host)
	 */
	u8			config;
	struct usb_ep		*in_ep, *out_ep;
	struct usb_endpoint_descriptor		/* ep descriptor */
				*in_ep_desc, *out_ep_desc;

	struct usb_ep		*notify_ep;	/* interrupt endpoint */
	struct usb_endpoint_descriptor	*notify_ep_desc;

	struct list_head		in_req_list;	/* list of write requests */
	struct list_head		out_req_list;	/* list of bulk out requests */

	wait_queue_head_t	wq;
	struct mutex		mtx;

	int			open_count;
};

struct amb_dev *ag_device;

static void notify_worker(struct work_struct *work);
static DECLARE_WORK(notify_work, notify_worker);

/*-------------------------------------------------------------------------*/

static unsigned int buflen = (64*1024);
module_param (buflen, uint, S_IRUGO);
MODULE_PARM_DESC(buflen, "buffer length, default=32K");

static unsigned int use_notify = 1;
module_param(use_notify, uint, S_IRUGO);
MODULE_PARM_DESC(use_notify, "Use intr_in Endpoint, 0=no, 1=yes, default=yes");


/*-------------------------------------------------------------------------*/

#define DRIVER_VENDOR_ID	0x4255		/* Ambarella */
#define DRIVER_PRODUCT_ID	0x0001

/*-------------------------------------------------------------------------*/

/*
 * DESCRIPTORS ... most are static, but strings and (full)
 * configuration descriptors are built on demand.
 */

#define STRING_MANUFACTURER		25
#define STRING_PRODUCT			42
#define STRING_SERIAL			101
#define STRING_SOURCE_SINK		250

/*
 * This device advertises two configurations; these numbers work
 * on a pxa250 as well as more flexible hardware.
 */
#define	AMB_BULK_CONFIG_VALUE	1

static struct usb_device_descriptor
ag_device_desc = {
	.bLength =		sizeof ag_device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		__constant_cpu_to_le16 (0x0200),
	.bDeviceClass =		USB_CLASS_VENDOR_SPEC,

	.idVendor =		__constant_cpu_to_le16 (DRIVER_VENDOR_ID),
	.idProduct =		__constant_cpu_to_le16 (DRIVER_PRODUCT_ID),
	.iManufacturer =	STRING_MANUFACTURER,
	.iProduct =		STRING_PRODUCT,
	.iSerialNumber =	STRING_SERIAL,
	.bNumConfigurations =	1,
};

static struct usb_config_descriptor
amb_bulk_config = {
	.bLength =		sizeof amb_bulk_config,
	.bDescriptorType =	USB_DT_CONFIG,

	/* compute wTotalLength on the fly */
	.bNumInterfaces =	1,
	.bConfigurationValue =	AMB_BULK_CONFIG_VALUE,
	.iConfiguration =	STRING_SOURCE_SINK,
	.bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower =		1,	/* self-powered */
};


static struct usb_otg_descriptor
otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,

	.bmAttributes =		USB_OTG_SRP,
};

/* one interface in each configuration */

static struct usb_interface_descriptor
amb_data_stream_intf = {
	.bLength =		sizeof amb_data_stream_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.iInterface =		STRING_SOURCE_SINK,
};


/* two full speed bulk endpoints; their use is config-dependent */

static struct usb_endpoint_descriptor
fs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor
fs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor
fs_intr_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(AG_NOTIFY_MAXPACKET),
	.bInterval =		1 << AG_NOTIFY_INTERVAL,
};

static const struct usb_descriptor_header *fs_amb_data_stream_function [] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	(struct usb_descriptor_header *) &amb_data_stream_intf,
	(struct usb_descriptor_header *) &fs_bulk_out_desc,
	(struct usb_descriptor_header *) &fs_bulk_in_desc,
	(struct usb_descriptor_header *) &fs_intr_in_desc,
	NULL,
};


#ifdef	CONFIG_USB_GADGET_DUALSPEED

/*
 * usb 2.0 devices need to expose both high speed and full speed
 * descriptors, unless they only run at full speed.
 *
 * that means alternate endpoint descriptors (bigger packets)
 * and a "device qualifier" ... plus more construction options
 * for the config descriptor.
 */

static struct usb_endpoint_descriptor
hs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress will be copied from fs_bulk_in_desc during amb_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16 (512),
};

static struct usb_endpoint_descriptor
hs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16 (512),
};

static struct usb_endpoint_descriptor
hs_intr_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(AG_NOTIFY_MAXPACKET),
	.bInterval =		AG_NOTIFY_INTERVAL + 4,
};

static struct usb_qualifier_descriptor
dev_qualifier = {
	.bLength =		sizeof dev_qualifier,
	.bDescriptorType =	USB_DT_DEVICE_QUALIFIER,

	.bcdUSB =		__constant_cpu_to_le16 (0x0200),
	.bDeviceClass =		USB_CLASS_VENDOR_SPEC,

	.bNumConfigurations =	1,
};

static const struct usb_descriptor_header *hs_amb_data_stream_function [] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	(struct usb_descriptor_header *) &amb_data_stream_intf,
	(struct usb_descriptor_header *) &hs_bulk_in_desc,
	(struct usb_descriptor_header *) &hs_bulk_out_desc,
	(struct usb_descriptor_header *) &hs_intr_in_desc,
	NULL,
};


/* maxpacket and other transfer characteristics vary by speed. */
#define ep_desc(g,hs,fs) (((g)->speed==USB_SPEED_HIGH)?(hs):(fs))

#else

/* if there's no high speed support, maxpacket doesn't change. */
#define ep_desc(g,hs,fs) fs

#endif	/* !CONFIG_USB_GADGET_DUALSPEED */

static char				manufacturer [50];
static char				serial [40];

/* static strings, in UTF-8 */
static struct usb_string		strings [] = {
	{ STRING_MANUFACTURER, manufacturer, },
	{ STRING_PRODUCT, longname, },
	{ STRING_SERIAL, serial, },
	{ STRING_SOURCE_SINK, source_sink, },
	{  }			/* end of list */
};

static struct usb_gadget_strings	stringtab = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings,
};

/*-------------------------------------------------------------------------*/
static int amb_send(u8 *buf, u32 len)
{
	struct amb_dev *dev = ag_device;
	struct usb_request *req = NULL;
	int rval = 0;

	if(wait_event_interruptible(dev->wq, !list_empty(&dev->in_req_list))){
		return -ERESTARTSYS;
	}

	spin_lock_irq(&dev->lock);
	req = list_entry(dev->in_req_list.next,	struct usb_request, list);
	list_del_init(&req->list);
	spin_unlock_irq(&dev->lock);

	if(buf)
		memcpy(req->buf, buf, len);

	req->length = len;
	rval = usb_ep_queue(dev->in_ep, req, GFP_ATOMIC);
	if (rval != 0) {
		ERROR(dev, "%s: cannot queue bulk in request, "
			"rval=%d\n", __func__, rval);
	}

	return rval;
}

static int amb_recv(u8 *buf, u32 len)
{
	struct amb_dev *dev = ag_device;
	struct usb_request *req = NULL;
	int rval = 0;

	if(wait_event_interruptible(dev->wq, !list_empty(&dev->out_req_list))){
		return -ERESTARTSYS;
	}

	spin_lock_irq(&dev->lock);
	req = list_entry(dev->out_req_list.next, struct usb_request, list);
	list_del_init(&req->list);
	spin_unlock_irq(&dev->lock);

	if(buf)
		memcpy(buf, req->buf, req->actual);

	req->length = buflen;
	if ((rval = usb_ep_queue(dev->out_ep, req, GFP_ATOMIC))) {
		ERROR(dev, "%s: can't queue bulk out request, "
			"rval = %d\n", __func__, rval);
	}

	return rval;
}


/*-------------------------------------------------------------------------*/
static int ag_ioctl(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int rval = 0;
	struct amb_dev *dev = ag_device;
	struct amb_cmd _cmd;
//	struct amb_ack ack;
	struct amb_rsp rsp;
	struct amb_notify notify;

	DBG(dev, "%s: Enter\n", __func__);

	mutex_lock(&dev->mtx);

	switch (cmd) {
	case AMB_DATA_STREAM_WR_RSP:

		if(copy_from_user(&rsp, (struct amb_rsp __user *)arg, sizeof(rsp))){
			mutex_unlock(&dev->mtx);
			return -EFAULT;
		}

		rval = amb_send((u8 *)&rsp, sizeof(rsp));
		break;

	case AMB_DATA_STREAM_RD_CMD:

		rval = amb_recv((u8 *)&_cmd, sizeof(_cmd));
		if(rval < 0)
			break;

		if(copy_to_user((struct amb_cmd __user *)arg, &_cmd, sizeof(_cmd))){
			mutex_unlock(&dev->mtx);
			return -EFAULT;
		}

		break;

	case AMB_DATA_STREAM_STATUS_CHANGE:
		if(copy_from_user(&notify, (struct amb_notify __user *)arg, sizeof(notify))){
			mutex_unlock(&dev->mtx);
			return -EFAULT;
		}

		spin_lock_irq(&dev->lock);
		g_port.bNotifyType = notify.bNotifyType;
		g_port.port_id = notify.port_id;
		g_port.value = notify.value;
		spin_unlock_irq(&dev->lock);
#if 0
		/* wait for ack from host */
		amb_recv((u8 *)&ack, sizeof(ack));

		/* send response to host */
		if((ack.acknowledge == AMB_ACK_SUCCESS) &&
				(ack.parameter0 == USB_CMD_RECV_REQUEST)){
			rsp.signature = AMB_STATUS_TOKEN;
			rsp.response = AMB_RSP_SUCCESS;
			rsp.parameter0 = AMB_RSP_CONNECT;
			rsp.parameter1 = 0;
		}else {
			rsp.signature = AMB_STATUS_TOKEN;
			rsp.response = AMB_RSP_SUCCESS;
			rsp.parameter0 = AMB_RSP_NO_CONN;
			rsp.parameter1 = 0;
		}
		amb_send((u8 *)&rsp, sizeof(rsp));
#endif
		break;

	default:
		rval = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&dev->mtx);

	DBG(dev, "%s: Exit\n", __func__);

	return rval;
}
static int ag_open(struct inode *inode, struct file *filp)
{
	struct amb_dev *dev = ag_device;
	int rval = 0;

	mutex_lock(&dev->mtx);

	/* gadget have not been configured */
	if(dev->config == 0){
		rval = -ENODEV;
		goto exit;
	}

	if(dev->open_count > 0){
		rval = -EBUSY;
		goto exit;
	}

	dev->open_count++;
exit:
	mutex_unlock(&dev->mtx);
	return rval;
}

static int ag_release(struct inode *inode, struct file *filp)
{
	struct amb_dev *dev = ag_device;

	mutex_lock(&dev->mtx);

	dev->open_count--;

	spin_lock_irq(&dev->lock);
	g_port.bNotifyType = PORT_STATUS_CHANGE;
	g_port.port_id = 0xffff;
	g_port.value = PORT_FREE_ALL;
	spin_unlock_irq(&dev->lock);

	mutex_unlock(&dev->mtx);

	return 0;
}

static int ag_read(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	struct amb_dev *dev = ag_device;
	struct usb_request *req = NULL;
	int len = 0, rval = 0;

	DBG(dev, "%s: Enter\n", __func__);

	mutex_lock(&dev->mtx);

	while(count > 0){
		if(wait_event_interruptible(dev->wq, !list_empty(&dev->out_req_list))){
			mutex_unlock(&dev->mtx);
			return -ERESTARTSYS;
		}

		spin_lock_irq(&dev->lock);
		req = list_entry(dev->out_req_list.next, struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irq(&dev->lock);

		if(copy_to_user(buf + len, req->buf, req->actual)){
			printk("len = %d, actual = %d\n", len, req->actual);
			mutex_unlock(&dev->mtx);
			return -EFAULT;
		}
		len += req->actual;
		count -= req->actual;

		req->length = buflen;
		if ((rval = usb_ep_queue(dev->out_ep, req, GFP_ATOMIC))) {
			ERROR(dev, "%s: can't queue bulk out request, "
				"rval = %d\n", __func__, rval);
			len = rval;
			break;
		}

		if (len % buflen != 0)
			break;
	}

	mutex_unlock(&dev->mtx);

	DBG(dev, "%s: Exit\n", __func__);

	return len;
}

static int ag_write(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	int rval = 0;
	int size, len = 0;
	struct amb_dev *dev = ag_device;
	struct usb_request *req = NULL;

	DBG(dev, "%s: Enter\n", __func__);

	mutex_lock(&dev->mtx);

	while(count > 0) {
		if(wait_event_interruptible(dev->wq, !list_empty(&dev->in_req_list))){
			mutex_unlock(&dev->mtx);
			return -ERESTARTSYS;
		}

		spin_lock_irq(&dev->lock);
		req = list_entry(dev->in_req_list.next, struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irq(&dev->lock);

		size = count < buflen ? count : buflen;
		if(copy_from_user(req->buf, buf + len, size)){
			mutex_unlock(&dev->mtx);
			return -EFAULT;
		}

		req->length = size;
		rval = usb_ep_queue(dev->in_ep, req, GFP_ATOMIC);
		if (rval != 0) {
			ERROR(dev, "%s: cannot queue bulk in request, "
				"rval=%d\n", __func__, rval);
			mutex_unlock(&dev->mtx);
			return rval;
		}

		count -= size;
		len += size;
	}

	mutex_unlock(&dev->mtx);

	DBG(dev, "%s: Exit\n", __func__);

	return len;
}

/*-------------------------------------------------------------------------*/
static void amb_notify_complete (struct usb_ep *ep, struct usb_request *req);

static void notify_worker(struct work_struct *work)
{
	struct amb_dev		*dev = ag_device;
	struct usb_request	*req = dev->notify_req;
	struct amb_notify	*event = req->buf;
	int rval = 0;

	spin_lock_irq(&dev->lock);
	memcpy(event, &g_port, sizeof(struct amb_notify));
	g_port.bNotifyType = PORT_NOTIFY_IDLE;
	g_port.port_id = 0xffff;
	g_port.value = 0;
	if(dev->open_count > 0)
		g_port.status = DEVICE_OPENED;
	else
		g_port.status = DEVICE_NOT_OPEN;

	req->length = AG_NOTIFY_MAXPACKET;
	req->complete = amb_notify_complete;
	req->context = dev;
	spin_unlock_irq(&dev->lock);

	rval = usb_ep_queue (dev->notify_ep, req, GFP_ATOMIC);
	if (rval < 0)
		DBG (dev, "status buf queue --> %d\n", rval);
}

/*-------------------------------------------------------------------------*/


/*
 * config descriptors are also handcrafted.  these must agree with code
 * that sets configurations, and with code managing interfaces and their
 * altsettings.  other complexity may come from:
 *
 *  - high speed support, including "other speed config" rules
 *  - multiple configurations
 *  - interfaces with alternate settings
 *  - embedded class or vendor-specific descriptors
 *
 * this handles high speed, and has a second config that could as easily
 * have been an alternate interface setting (on most hardware).
 *
 * NOTE:  to demonstrate (and test) more USB capabilities, this driver
 * should include an altsetting to test interrupt transfers, including
 * high bandwidth modes at high speed.  (Maybe work like Intel's test
 * device?)
 */
static int
amb_config_buf (struct usb_gadget *gadget,
		u8 *buf, u8 type, unsigned index)
{
	int len;
	const struct usb_descriptor_header **function;

#ifdef CONFIG_USB_GADGET_DUALSPEED
	int hs = (gadget->speed == USB_SPEED_HIGH);
#endif

	/* one configuration will always be index 0 */
	if (index > 0)
		return -EINVAL;

#ifdef CONFIG_USB_GADGET_DUALSPEED
	if (type == USB_DT_OTHER_SPEED_CONFIG)
		hs = !hs;
	if (hs)
		function = hs_amb_data_stream_function;
	else
#endif
		function = fs_amb_data_stream_function;

	/* for now, don't advertise srp-only devices */
	if (!gadget->is_otg)
		function++;

	len = usb_gadget_config_buf (&amb_bulk_config,
			buf, USB_BUFSIZ, function);
	if (len < 0)
		return len;

	((struct usb_config_descriptor *) buf)->bDescriptorType = type;

	return len;
}

/*-------------------------------------------------------------------------*/

static struct usb_request *
amb_alloc_buf_req (struct usb_ep *ep, unsigned length, gfp_t kmalloc_flags)
{
	struct usb_request	*req;

	req = usb_ep_alloc_request (ep, kmalloc_flags);
	if (req) {
		req->length = length;
		req->buf = kmalloc(length, kmalloc_flags);
		if (!req->buf) {
			usb_ep_free_request (ep, req);
			req = NULL;
		}
	}
	return req;
}

static void amb_free_buf_req (struct usb_ep *ep, struct usb_request *req)
{
	if (req->buf){
		kfree(req->buf);
		req->buf = NULL;
	}
	usb_ep_free_request (ep, req);
}

/*-------------------------------------------------------------------------*/

/* if there is only one request in the queue, there'll always be an
 * irq delay between end of one request and start of the next.
 * that prevents using hardware dma queues.
 */
static void amb_bulk_in_complete (struct usb_ep *ep, struct usb_request *req)
{
	struct amb_dev	*dev = ep->driver_data;
	int		status = req->status;
	int 		rval = 0;

	switch (status) {

	case 0: 			/* normal completion? */
		spin_lock_irq(&dev->lock);
		list_add_tail(&req->list, &dev->in_req_list);
		wake_up_interruptible(&dev->wq);
		spin_unlock_irq(&dev->lock);
		break;

	/* this endpoint is normally active while we're configured */
	case -ECONNRESET:		/* request dequeued */
		usb_ep_fifo_flush(ep);
	case -ESHUTDOWN:		/* disconnect from host */
		VDBG (dev, "%s gone (%d), %d/%d\n", ep->name, status,
				req->actual, req->length);
		amb_free_buf_req (ep, req);
		return;
	default:
		DBG (dev, "%s complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		/* queue request again */
		rval = usb_ep_queue(dev->in_ep, req, GFP_ATOMIC);
		if (rval != 0) {
			ERROR(dev, "%s: cannot queue bulk in request, "
				"rval=%d\n", __func__, rval);
		}
		break;
	}
}


/* if there is only one request in the queue, there'll always be an
 * irq delay between end of one request and start of the next.
 * that prevents using hardware dma queues.
 */
static void amb_bulk_out_complete (struct usb_ep *ep, struct usb_request *req)
{
	struct amb_dev	*dev = ep->driver_data;
	int		status = req->status;
	int 		rval = 0;

	switch (status) {
	case 0: 			/* normal completion */
		spin_lock_irq(&dev->lock);
		list_add_tail(&req->list, &dev->out_req_list);
		wake_up_interruptible(&dev->wq);
		spin_unlock_irq(&dev->lock);

		break;

	/* this endpoint is normally active while we're configured */
	case -ECONNRESET:		/* request dequeued */
		usb_ep_fifo_flush(ep);
	case -ESHUTDOWN:		/* disconnect from host */
		VDBG (dev, "%s gone (%d), %d/%d\n", ep->name, status,
				req->actual, req->length);
		amb_free_buf_req (ep, req);
		break;
	default:
		DBG (dev, "%s complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		/* queue request again */
		rval = usb_ep_queue(dev->out_ep, req, GFP_ATOMIC);
		if (rval != 0) {
			ERROR(dev, "%s: cannot queue bulk in request, "
				"rval=%d\n", __func__, rval);
		}
		break;
	}
}


/*-------------------------------------------------------------------------*/

static void amb_notify_complete (struct usb_ep *ep, struct usb_request *req)
{
	int				status = req->status;
	struct amb_dev			*dev = ep->driver_data;

	req->context = NULL;

	switch (status) {
	case 0: 			/* normal completion */
		/* issue the second notification if host reads the previous one */
		spin_lock_irq(&dev->lock);
		schedule_work(&notify_work);
		spin_unlock_irq(&dev->lock);
		break;

	/* this endpoint is normally active while we're configured */
	case -ECONNRESET:		/* request dequeued */
		usb_ep_fifo_flush(ep);
	case -ESHUTDOWN:		/* disconnect from host */
		VDBG (dev, "%s gone (%d), %d/%d\n", ep->name, status,
				req->actual, req->length);
		amb_free_buf_req (ep, req);
		break;
	default:
		DBG (dev, "%s complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
		break;
	}
}

static void amb_start_notify (struct amb_dev *dev)
{
	struct usb_request *req = dev->notify_req;
	struct amb_notify *event;
	int value;

	DBG (dev, "%s, flush old status first\n", __func__);

	/* flush old status
	 *
	 * FIXME ugly idiom, maybe we'd be better with just
	 * a "cancel the whole queue" primitive since any
	 * unlink-one primitive has way too many error modes.
	 * here, we "know" toggle is already clear...
	 *
	 * FIXME iff req->context != null just dequeue it
	 */
	usb_ep_disable (dev->notify_ep);
	usb_ep_enable (dev->notify_ep, dev->notify_ep_desc);

	event = req->buf;
	event->bNotifyType = __constant_cpu_to_le16(PORT_NOTIFY_IDLE);
	event->port_id = __constant_cpu_to_le16 (0);
	event->value = __constant_cpu_to_le32 (0);

	req->length = AG_NOTIFY_MAXPACKET;
	req->complete = amb_notify_complete;
	req->context = dev;

	value = usb_ep_queue (dev->notify_ep, req, GFP_ATOMIC);
	if (value < 0)
		DBG (dev, "status buf queue --> %d\n", value);
}



/*-------------------------------------------------------------------------*/

static void amb_reset_config (struct amb_dev *dev)
{
	struct usb_request *req;

	if (dev == NULL) {
		printk(KERN_ERR "amb_reset_config: NULL device pointer\n");
		return;
	}

	if (dev->config == 0)
		return;
	dev->config = 0;

	/* free write requests on the free list */
	while(!list_empty(&dev->in_req_list)) {
		req = list_entry(dev->in_req_list.next,
				struct usb_request, list);
		list_del(&req->list);
		req->length = buflen;
		amb_free_buf_req(dev->in_ep, req);
	}

	/* just disable endpoints, forcing completion of pending i/o.
	 * all our completion handlers free their requests in this case.
	 */
	/* Disable the endpoints */
	if (dev->notify_ep)
		usb_ep_disable(dev->notify_ep);
	usb_ep_disable(dev->in_ep);
	usb_ep_disable(dev->out_ep);
}

static int amb_set_stream_config (struct amb_dev *dev, gfp_t gfp_flags)
{
	int			result = 0, i;
	struct usb_gadget	*gadget = dev->gadget;
	struct usb_endpoint_descriptor	*d;
	struct usb_ep *ep;
	struct usb_request *req;

	/* one endpoint writes data in (to the host) */
	d = ep_desc (gadget, &hs_bulk_in_desc, &fs_bulk_in_desc);
	result = usb_ep_enable(dev->in_ep, d);
	if (result == 0) {
		dev->in_ep_desc = d;
	} else {
		ERROR(dev, "%s: can't enable %s, result=%d\n",
			__func__, dev->in_ep->name, result);
		return result;
	}

	/* one endpoint reads data out (from the host) */
	d = ep_desc (gadget, &hs_bulk_out_desc, &fs_bulk_out_desc);
	result = usb_ep_enable(dev->out_ep, d);
	if (result == 0) {
		dev->out_ep_desc = d;
	} else {
		ERROR(dev, "%s: can't enable %s, result=%d\n",
			__func__, dev->out_ep->name, result);
		usb_ep_disable(dev->in_ep);
		return result;
	}

	if(use_notify) {
		d = ep_desc (gadget, &hs_intr_in_desc, &fs_intr_in_desc);
		result = usb_ep_enable(dev->notify_ep, d);
		if (result == 0) {
			dev->notify_ep_desc = d;
		} else {
			ERROR(dev, "%s: can't enable %s, result=%d\n",
				__func__, dev->notify_ep->name, result);
			usb_ep_disable(dev->out_ep);
			usb_ep_disable(dev->in_ep);
			return result;
		}
	}

	/* allocate and queue read requests */
	ep = dev->out_ep;
	for (i = 0; i < Q_DEPTH && result == 0; i++) {
		req = amb_alloc_buf_req(ep, buflen, GFP_ATOMIC);
		if (req) {
			req->complete = amb_bulk_out_complete;
			result = usb_ep_queue(ep, req, GFP_ATOMIC);
		} else {
			ERROR(dev, "%s: can't allocate bulk in requests\n", __func__);
			result = -ENOMEM;
			goto exit;
		}
	}

	/* allocate write requests, and put on free list */
	ep = dev->in_ep;
	for (i = 0; i < Q_DEPTH; i++) {
		req = amb_alloc_buf_req(ep, buflen, GFP_ATOMIC);
		if (req) {
			req->complete = amb_bulk_in_complete;
			list_add(&req->list, &dev->in_req_list);
		} else {
			ERROR(dev, "%s: can't allocate bulk in requests\n", __func__);
			result = -ENOMEM;
			goto exit;
		}
	}

	if (dev->notify_ep)
		dev->notify_req = amb_alloc_buf_req(dev->notify_ep,
			AG_NOTIFY_MAXPACKET, GFP_ATOMIC);

	if(dev->notify_ep){
		amb_start_notify(dev);
	}

exit:
	/* caller is responsible for cleanup on error */
	return result;
}


/*-------------------------------------------------------------------------*/


/* change our operational config.  this code must agree with the code
 * that returns config descriptors, and altsetting code.
 *
 * it's also responsible for power management interactions. some
 * configurations might not work with our current power sources.
 *
 * note that some device controller hardware will constrain what this
 * code can do, perhaps by disallowing more than one configuration or
 * by limiting configuration choices (like the pxa2xx).
 */
static int
amb_set_config (struct amb_dev *dev, unsigned number, gfp_t gfp_flags)
{
	int			result = 0;
	struct usb_gadget	*gadget = dev->gadget;

	if (number == dev->config)
		return 0;

	amb_reset_config (dev);

	switch (number) {
	case AMB_BULK_CONFIG_VALUE:
		result = amb_set_stream_config(dev, gfp_flags);
		break;
	default:
		result = -EINVAL;
		/* FALL THROUGH */
	case 0:
		return result;
	}

	if (!result && (!dev->in_ep || !dev->out_ep))
		result = -ENODEV;
	if (result)
		amb_reset_config (dev);
	else {
		char *speed;
		char notify_info[64];

		switch (gadget->speed) {
		case USB_SPEED_LOW:	speed = "low"; break;
		case USB_SPEED_FULL:	speed = "full"; break;
		case USB_SPEED_HIGH:	speed = "high"; break;
		default: 		speed = "?"; break;
		}

		dev->config = number;
		INFO (dev, "%s speed config #%d: \n", speed, number);
		snprintf(notify_info, sizeof(notify_info), "intr_in address = 0x%02x",
			dev->notify_ep ? dev->notify_ep_desc->bEndpointAddress : 0);

		INFO(dev, "bulk_out address = 0x%02x, bulk_in address = 0x%02x, %s\n",
			dev->out_ep_desc->bEndpointAddress,
			dev->in_ep_desc->bEndpointAddress,
			dev->notify_ep ? notify_info : "");
	}
	return result;
}

/*-------------------------------------------------------------------------*/

static void amb_setup_complete (struct usb_ep *ep, struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		DBG ((struct amb_dev *) ep->driver_data,
				"setup complete --> %d, %d/%d\n",
				req->status, req->actual, req->length);
}

/*
 * The setup() callback implements all the ep0 functionality that's
 * not handled lower down, in hardware or the hardware driver (like
 * device and endpoint feature flags, and their status).  It's all
 * housekeeping for the gadget function we're implementing.  Most of
 * the work is in config-specific setup.
 */
static int
amb_setup (struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct amb_dev		*dev = get_gadget_data (gadget);
	struct usb_request	*req = dev->ctrl_req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* usually this stores reply data in the pre-allocated ep0 buffer,
	 * but config change events will reconfigure hardware.
	 */
	req->zero = 0;
	switch (ctrl->bRequest) {

	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != USB_DIR_IN)
			goto unknown;

		switch (w_value >> 8) {
		case USB_DT_DEVICE:
			value = min (w_length, (u16) sizeof ag_device_desc);
			memcpy (req->buf, &ag_device_desc, value);
			break;
#ifdef CONFIG_USB_GADGET_DUALSPEED
		case USB_DT_DEVICE_QUALIFIER:
			if (!gadget->is_dualspeed)
				break;
			value = min (w_length, (u16) sizeof dev_qualifier);
			memcpy (req->buf, &dev_qualifier, value);
			break;

		case USB_DT_OTHER_SPEED_CONFIG:
			if (!gadget->is_dualspeed)
				break;
			/* fall through */
#endif /* CONFIG_USB_GADGET_DUALSPEED */
		case USB_DT_CONFIG:
			value = amb_config_buf (gadget, req->buf,
					w_value >> 8,
					w_value & 0xff);
			if (value >= 0)
				value = min (w_length, (u16) value);
			break;

		case USB_DT_STRING:
			/* wIndex == language code.
			 * this driver only handles one language, you can
			 * add string tables for other languages, using
			 * any UTF-8 characters
			 */
			value = usb_gadget_get_string (&stringtab,
					w_value & 0xff, req->buf);
			if (value >= 0)
				value = min (w_length, (u16) value);
			break;
		}
		break;

	/* currently two configs, two speeds */
	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != 0)
			goto unknown;
		if (gadget->a_hnp_support)
			DBG (dev, "HNP available\n");
		else if (gadget->a_alt_hnp_support)
			DBG (dev, "HNP needs a different root port\n");
		else
			VDBG (dev, "HNP inactive\n");
		spin_lock (&dev->lock);
		value = amb_set_config (dev, w_value, GFP_ATOMIC);
		spin_unlock (&dev->lock);
		break;
	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != USB_DIR_IN)
			goto unknown;
		*(u8 *)req->buf = dev->config;
		value = min (w_length, (u16) 1);
		break;

	/* until we add altsetting support, or other interfaces,
	 * only 0/0 are possible.  pxa2xx only supports 0/0 (poorly)
	 * and already killed pending endpoint I/O.
	 */
	case USB_REQ_SET_INTERFACE:
		if (ctrl->bRequestType != USB_RECIP_INTERFACE)
			goto unknown;
		spin_lock (&dev->lock);
		if (dev->config && w_index == 0 && w_value == 0) {
			u8		config = dev->config;

			/* resets interface configuration, forgets about
			 * previous transaction state (queued bufs, etc)
			 * and re-inits endpoint state (toggle etc)
			 * no response queued, just zero status == success.
			 * if we had more than one interface we couldn't
			 * use this "reset the config" shortcut.
			 */
			amb_set_config (dev, config, GFP_ATOMIC);
			value = 0;
		}
		spin_unlock (&dev->lock);
		break;
	case USB_REQ_GET_INTERFACE:
		if (ctrl->bRequestType != (USB_DIR_IN|USB_RECIP_INTERFACE))
			goto unknown;
		if (!dev->config)
			break;
		if (w_index != 0) {
			value = -EDOM;
			break;
		}
		*(u8 *)req->buf = 0;
		value = min (w_length, (u16) 1);
		break;

	/*
	 * These are the same vendor-specific requests supported by
	 * Intel's USB 2.0 compliance test devices.  We exceed that
	 * device spec by allowing multiple-packet requests.
	 */
	case 0x5b:	/* control WRITE test -- fill the buffer */
		if (ctrl->bRequestType != (USB_DIR_OUT|USB_TYPE_VENDOR))
			goto unknown;
		if (w_value || w_index)
			break;
		/* just read that many bytes into the buffer */
		if (w_length > USB_BUFSIZ)
			break;
		value = w_length;
		break;
	case 0x5c:	/* control READ test -- return the buffer */
		if (ctrl->bRequestType != (USB_DIR_IN|USB_TYPE_VENDOR))
			goto unknown;
		if (w_value || w_index)
			break;
		/* expect those bytes are still in the buffer; send back */
		if (w_length > USB_BUFSIZ
				|| w_length != req->length)
			break;
		value = w_length;
		break;

	default:
unknown:
		VDBG (dev,
			"unknown control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer before status phase? */
	if (value >= 0) {
		req->length = value;
		req->zero = value < w_length;
		value = usb_ep_queue (gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			DBG (dev, "ep_queue --> %d\n", value);
			req->status = 0;
			amb_setup_complete (gadget->ep0, req);
		}
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static void amb_disconnect (struct usb_gadget *gadget)
{
	struct amb_dev		*dev = get_gadget_data (gadget);

	spin_lock (&dev->lock);
	amb_reset_config (dev);

	/* a more significant application might have some non-usb
	 * activities to quiesce here, saving resources like power
	 * or pushing the notification up a network stack.
	 */
	spin_unlock (&dev->lock);

	/* next we may get setup() calls to enumerate new connections;
	 * or an unbind() during shutdown (including removing module).
	 */
}


/*-------------------------------------------------------------------------*/

static void /* __init_or_exit */ amb_unbind (struct usb_gadget *gadget)
{
	struct amb_dev		*dev = get_gadget_data (gadget);

	DBG (dev, "unbind\n");

	/* we've already been disconnected ... no i/o is active */
	if (dev->ctrl_req) {
		dev->ctrl_req->length = USB_BUFSIZ;
		amb_free_buf_req (gadget->ep0, dev->ctrl_req);
		dev->ctrl_req = NULL;
	}

	kfree (dev);
	set_gadget_data (gadget, NULL);
	ag_device = NULL;
}

static int __init amb_bind (struct usb_gadget *gadget)
{
	struct amb_dev		*dev;
	struct usb_ep		*ep;
	int			gcnum;

	gcnum = usb_gadget_controller_number (gadget);
	if (gcnum >= 0)
		ag_device_desc.bcdDevice = cpu_to_le16 (0x0200 + gcnum);
	else {
		printk (KERN_WARNING "%s: controller '%s' not recognized\n",
			shortname, gadget->name);
		ag_device_desc.bcdDevice = __constant_cpu_to_le16 (0x9999);
	}

	/* ok, we made sense of the hardware ... */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	spin_lock_init (&dev->lock);
	mutex_init(&dev->mtx);
	init_waitqueue_head(&dev->wq);
	INIT_LIST_HEAD(&dev->in_req_list);
	INIT_LIST_HEAD(&dev->out_req_list);
	dev->gadget = gadget;
	ag_device = dev;
	set_gadget_data (gadget, dev);

	/* configure the endpoints */
	usb_ep_autoconfig_reset (gadget);

	ep = usb_ep_autoconfig (gadget, &fs_bulk_in_desc);
	if (!ep)
		goto autoconf_fail;
	ep->driver_data = dev;	/* claim the endpoint */
	dev->in_ep = ep;

	ep = usb_ep_autoconfig (gadget, &fs_bulk_out_desc);
	if (!ep)
		goto autoconf_fail;
	ep->driver_data = dev;	/* claim the endpoint */
	dev->out_ep = ep;

	if (use_notify) {
		ep = usb_ep_autoconfig(gadget, &fs_intr_in_desc);
		if (!ep) {
			printk(KERN_ERR "amb_bind: cannot use notify endpoint\n");
			goto autoconf_fail;
		}
		dev->notify_ep = ep;
		ep->driver_data = dev;	/* claim the endpoint */

		amb_data_stream_intf.bNumEndpoints = 3;
	}

	/* preallocate control response and buffer */
	dev->ctrl_req = amb_alloc_buf_req (gadget->ep0, USB_BUFSIZ, GFP_KERNEL);
	if (dev->ctrl_req == NULL) {
		goto enomem;
	}
	dev->ctrl_req->complete = amb_setup_complete;
	gadget->ep0->driver_data = dev;

	ag_device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;

#ifdef CONFIG_USB_GADGET_DUALSPEED
	/* assume ep0 uses the same value for both speeds ... */
	dev_qualifier.bMaxPacketSize0 = ag_device_desc.bMaxPacketSize0;

	/* and that all endpoints are dual-speed */
	hs_bulk_in_desc.bEndpointAddress = fs_bulk_in_desc.bEndpointAddress;
	hs_bulk_out_desc.bEndpointAddress = fs_bulk_out_desc.bEndpointAddress;
	hs_intr_in_desc.bEndpointAddress = fs_intr_in_desc.bEndpointAddress;
#endif

	if (gadget->is_otg) {
		otg_descriptor.bmAttributes |= USB_OTG_HNP,
		amb_bulk_config.bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	usb_gadget_set_selfpowered (gadget);

	gadget->ep0->driver_data = dev;

	INFO (dev, "%s, version: " DRIVER_VERSION "\n", longname);

	snprintf (manufacturer, sizeof manufacturer, "%s %s with %s",
		init_utsname()->sysname, init_utsname()->release,
		gadget->name);

	return 0;

enomem:
	printk(KERN_ERR "%s: No memory\n", __func__);
	amb_unbind (gadget);
	return -ENOMEM;
autoconf_fail:
	printk (KERN_ERR "%s: can't autoconfigure on %s\n",
		shortname, gadget->name);
	return -ENODEV;
}

/*-------------------------------------------------------------------------*/

static void amb_suspend (struct usb_gadget *gadget)
{
	struct amb_dev		*dev = NULL;

	if (gadget->speed == USB_SPEED_UNKNOWN)
		return;

	dev = get_gadget_data (gadget);
	DBG (dev, "suspend\n");
}

static void amb_resume (struct usb_gadget *gadget)
{
	struct amb_dev		*dev = NULL;

	dev = get_gadget_data (gadget);
	DBG (dev, "resume\n");
}


/*-------------------------------------------------------------------------*/

static struct file_operations ag_fops = {
	.owner = THIS_MODULE,
	.ioctl = ag_ioctl,
	.open = ag_open,
	.read = ag_read,
	.write = ag_write,
	.release = ag_release
};


static struct usb_gadget_driver amb_gadget_driver = {
#ifdef CONFIG_USB_GADGET_DUALSPEED
	.speed		= USB_SPEED_HIGH,
#else
	.speed		= USB_SPEED_FULL,
#endif
	.function	= (char *) longname,
	.bind		= amb_bind,
	.unbind		= __exit_p(amb_unbind),

	.setup		= amb_setup,
	.disconnect	= amb_disconnect,

	.suspend	= amb_suspend,
	.resume		= amb_resume,

	.driver 	= {
		.name		= (char *) shortname,
		.owner		= THIS_MODULE,
	},
};

MODULE_AUTHOR ("Cao Rongrong <rrcao@ambarella.com>");
MODULE_LICENSE ("GPL");


static int __init amb_gadget_init (void)
{
	int rval = 0;
	dev_t dev_id;

	/* a real value would likely come through some id prom
	 * or module option.  this one takes at least two packets.
	 */
	strlcpy (serial, "123456789ABC", sizeof serial);

	rval = usb_gadget_register_driver (&amb_gadget_driver);
	if (rval) {
		printk(KERN_ERR "amb_gadget_init: cannot register gadget driver, "
			"rval=%d\n", rval);
		goto out;
	}

	dev_id = MKDEV(AMB_GADGET_MAJOR, AMB_GADGET_MINOR_START);
	rval = register_chrdev_region(dev_id, 1, "amb_gadget");
	if(rval < 0){
		printk(KERN_ERR "amb_gadget_init: register devcie number error!\n");
		goto out;
	}

	cdev_init(&ag_cdev, &ag_fops);
	ag_cdev.owner = THIS_MODULE;
	rval = cdev_add(&ag_cdev, dev_id, 1);
	if (rval) {
		printk(KERN_ERR "amb_gadget_init: cdev_add failed\n");
		unregister_chrdev_region(dev_id, 1);
		goto out;
	}

out:
	if(rval)
		usb_gadget_unregister_driver(&amb_gadget_driver);

	return rval;
}
module_init (amb_gadget_init);

static void __exit amb_gadget_exit (void)
{
	dev_t dev_id;

	usb_gadget_unregister_driver (&amb_gadget_driver);

	dev_id = MKDEV(AMB_GADGET_MAJOR, AMB_GADGET_MINOR_START);
	unregister_chrdev_region(dev_id, 1);
	cdev_del(&ag_cdev);
}
module_exit (amb_gadget_exit);

