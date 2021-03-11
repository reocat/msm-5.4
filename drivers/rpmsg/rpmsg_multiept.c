/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 *
 * Remote processor messaging transport
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#define ASSERT(x) \
do { \
	if (x) \
		break; \
		pr_emerg("### ASSERTION FAILED %s: %s: %d: %s\n", \
		__FILE__, __func__, __LINE__, #x); dump_stack(); BUG(); \
} while (0)

static ssize_t dev_attr_local_default_addr_show(struct device *dev,
			struct device_attribute *attr, char *buffer);
static ssize_t dev_attr_remote_default_addr_show(struct device *dev,
			struct device_attribute *attr, char *buffer);
static ssize_t dev_attr_name_show(struct device *dev,
			struct device_attribute *attr, char *buffer);

static ssize_t dev_attr_ept_delete_store(struct device *dev,
			struct device_attribute *attr,
			const char *buffer, size_t size);
static ssize_t dev_attr_ept_new_store(struct device *dev,
			struct device_attribute *attr,
			const char *buffer, size_t size);

static DEVICE_ATTR(local_addr, S_IRUGO,
		dev_attr_local_default_addr_show, NULL);
static DEVICE_ATTR(remote_addr, S_IRUGO,
		dev_attr_remote_default_addr_show, NULL);
static DEVICE_ATTR(name, S_IRUGO,
		dev_attr_name_show, NULL);
static DEVICE_ATTR(ept_new, S_IWUSR | S_IWGRP, NULL,
		dev_attr_ept_new_store);
static DEVICE_ATTR(ept_delete, S_IWUSR | S_IWGRP, NULL,
		dev_attr_ept_delete_store);



static ssize_t dev_attr_address_show(struct device *dev,
			struct device_attribute *attr, char *buffer);
static ssize_t dev_attr_channel_show(struct device *dev,
			struct device_attribute *attr, char *buffer);
static ssize_t dev_attr_dst_show(struct device *dev,
			struct device_attribute *attr, char *buffer);
static ssize_t dev_attr_src_show(struct device *dev,
			struct device_attribute *attr, char *buffer);
static ssize_t dev_attr_type_show(struct device *dev,
			struct device_attribute *attr, char *buffer);

static ssize_t dev_attr_dst_store(struct device *dev,
			struct device_attribute *attr,
			const char *buffer, size_t size);
static ssize_t dev_attr_src_store(struct device *dev,
			struct device_attribute *attr,
			const char *buffer, size_t size);

static DEVICE_ATTR(channel, S_IRUGO, dev_attr_channel_show, NULL);
static DEVICE_ATTR(address, S_IRUGO, dev_attr_address_show, NULL);
static DEVICE_ATTR(type, S_IRUGO, dev_attr_type_show, NULL);
static DEVICE_ATTR(dst, S_IRUGO | S_IWUSR | S_IWGRP,
	dev_attr_dst_show, dev_attr_dst_store);
static DEVICE_ATTR(src, S_IRUGO | S_IWUSR | S_IWGRP,
	dev_attr_src_show, dev_attr_src_store);


struct channel_device {
	struct device *chnl_dev;
	int id;
	struct rpmsg_device *chnl;
	struct channel_device *next;
};

enum rpmsg_ept_type {
	ENDPOINT_STREAM,
	ENDPOINT_DGRAM,
};

struct endpoint_device {
	#define FIFO_SIZE (8192)
	#define SIG_FIFO_SIZE (512)
	#define MAX_RPMSG_BUFF_SIZE (8192)
	struct device *dev;
	struct cdev cdev;
	struct rpmsg_endpoint *ept;
	enum rpmsg_ept_type type;
	struct channel_device *chnl_dev;
	pid_t pid;
	struct kfifo fifo; /* RX FIFO for synchronous read-out*/
	struct kfifo sig_fifo; /* signal FIFO for DGRAM mode */
	int src; /* source of the received messages */
	int dst; /* destination of all TXed messages */
	struct mutex lock;
	int major; /* device major */
	int minor; /* device minor */
	char txbuff[MAX_RPMSG_BUFF_SIZE];
	struct endpoint_device *next;
};

static int rpmsg_rx_cb(struct rpmsg_device *chnl, void *data,
			int len, void *priv, u32 src);
static int endpoint_list_add(struct endpoint_device **base,
				struct channel_device *chnl_dev,
				struct rpmsg_endpoint *new_ept,
				unsigned int addr,
				enum rpmsg_ept_type type, pid_t pid);
static void channel_list_add(struct channel_device **base,
				struct rpmsg_device *rpdev);
static int channel_get_free_id(struct channel_device **base);

struct channel_device *channel_list;
struct endpoint_device *endpoint_list;


static int device_open(struct inode *inode, struct file *filp);
static int device_close(struct inode *inode, struct file *filp);
static ssize_t device_read(struct file *filp, char __user *buffer,
				size_t length, loff_t *offset);
static ssize_t device_write(struct file *filp, const char __user *buffer,
				size_t length, loff_t *offset);

static const struct file_operations device_ops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_close
};

#define CLASS_NAME "rpmsg"
#define RPMSG_MAX_NUM_EPTS (16384)
#define RPMSG_MINOR_CHANNEL_BITS (3)
#define RPMSG_MINOR_CHANNEL_MASK ((1<<RPMSG_MINOR_CHANNEL_BITS)-1)
#define RPMSG_MINOR_EPT_BITS (11)
#define RPMSG_MINOR_EPT_MASK ((1<<RPMSG_MINOR_EPT_BITS)-1)

dev_t rpmsg_major;
static struct class *rpmsg_class;

static int channel_get_free_id(struct channel_device **base)
{
	int found_match = 0;
	int retval = -1;
	struct channel_device *iterator = NULL;

	pr_debug("rpmsg: called %s\n", __func__);

	for (retval = 1; retval <= RPMSG_MINOR_CHANNEL_MASK;
		retval++) {
		found_match = 0;
		for (iterator = *base;
			iterator != NULL;
			iterator = iterator->next) {
			if (iterator->id == retval) {
				found_match = 1;
				break;
			}
		}
		if (found_match == 0)
			return retval;
	}
	return -1; /* out of channel IDs*/
}

static void channel_list_add(struct channel_device **base,
			struct rpmsg_device *rpdev)
{
	struct channel_device *iterator = NULL;
	struct channel_device *prev = NULL;
	int id = -1;
	int retval = -1;

	ASSERT(base);
	ASSERT(rpdev);

	pr_debug("rpmsg: called %s\n", __func__);
	id = channel_get_free_id(base);
	if (id == -1) {
		pr_err("rpmsg: out of channel IDs\n");
		return;
	}

	if (*base == NULL) {
		*base = (struct channel_device *)
		kzalloc(sizeof(struct channel_device), GFP_KERNEL);
		iterator = *base;
	} else {
		iterator = *base;
		while (iterator->next != NULL)
			iterator = iterator->next;
		prev = iterator;
		iterator->next = (struct channel_device *)
		kzalloc(sizeof(struct channel_device), GFP_KERNEL);
		iterator = iterator->next;
	}

	if (iterator == NULL) {
		pr_err("rpmsg: out of memory\n");
		return;
	}

	iterator->next = NULL;
	iterator->chnl = rpdev;
	iterator->id = id;

	/* Create /channel_xx entry in /sys/rpmsg_interface
	 *  directory with specified attributes
	 */
	ASSERT(rpmsg_class);

	iterator->chnl_dev  = device_create(rpmsg_class,
		NULL,
		MKDEV(0, 0),
		iterator,
		"channel_%d",
		iterator->id);

	/* success */
	if (iterator->chnl_dev == NULL) {
		pr_err("rpmsg: cannot create device\n");
		goto error_chnl_dev;
	}

	retval =  device_create_file(iterator->chnl_dev,
				&dev_attr_local_addr);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto error_chnl_dev;
	}

	retval =  device_create_file(iterator->chnl_dev,
				&dev_attr_remote_addr);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto error_chnl_dev;
	}

	retval =  device_create_file(iterator->chnl_dev,
				&dev_attr_name);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto error_chnl_dev;
	}

	retval =  device_create_file(iterator->chnl_dev,
				&dev_attr_ept_new);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto error_chnl_dev;
	}

	retval =  device_create_file(iterator->chnl_dev,
				&dev_attr_ept_delete);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto error_chnl_dev;
	}

	return;

error_chnl_dev:
	put_device(iterator->chnl_dev);
	pr_err("rpmsg: cannot create channel device\n");

	if (*base == iterator) {
		*base = NULL;
	} else {
		ASSERT(prev);
		prev->next = NULL;
	}
	kfree(iterator);
}

static ssize_t dev_attr_address_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct endpoint_device *ept_dev = NULL;

	pr_debug("rpmsg: called %s\n", __func__);
	ASSERT(dev);
	ept_dev = (struct endpoint_device *)dev_get_drvdata(dev);
	ASSERT(ept_dev);
	ASSERT(ept_dev->ept);

	return snprintf(buffer, PAGE_SIZE, "%d\n",
				ept_dev->ept->addr);
}

static ssize_t dev_attr_channel_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct endpoint_device *ept_dev = NULL;
	struct channel_device *chnl_dev = NULL;

	pr_debug("rpmsg: called %s\n", __func__);
	ASSERT(dev);
	ept_dev = (struct endpoint_device *)dev_get_drvdata(dev);
	ASSERT(ept_dev);
	chnl_dev = ept_dev->chnl_dev;
	ASSERT(chnl_dev);
	ASSERT(chnl_dev->chnl);

	return snprintf(buffer, PAGE_SIZE, "%s\n",
				chnl_dev->chnl->id.name);
}

static ssize_t dev_attr_type_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct endpoint_device *ept_dev = NULL;

	pr_debug("rpmsg: called %s\n", __func__);
	ASSERT(dev);
	ept_dev = (struct endpoint_device *)dev_get_drvdata(dev);
	ASSERT(ept_dev);

	return snprintf(buffer, PAGE_SIZE, "%s\n",
				(ept_dev->type == ENDPOINT_STREAM) ?
				"stream":"dgram");
}


static int endpoint_list_add(struct endpoint_device **base,
			struct channel_device *chnl_dev,
			struct rpmsg_endpoint *new_ept,
			unsigned int addr,
			enum rpmsg_ept_type type, pid_t pid)
{
	struct endpoint_device *iterator = NULL;
	struct rpmsg_endpoint *ept = NULL;
	struct endpoint_device *prev = NULL;
	int retval = -1;

	pr_debug("rpmsg: called %s\n", __func__);

	if (addr > RPMSG_MINOR_EPT_MASK) {
		pr_err("rpmsg: too high ept address\n");
		return -1;
	}

	ASSERT(base);
	if (*base == NULL) {
		*base = (struct endpoint_device *)
		kzalloc(sizeof(struct endpoint_device), GFP_KERNEL);
		iterator = *base;
	} else {
		iterator = *base;
		while (iterator->next != NULL)
			iterator = iterator->next;

		prev = iterator;
		iterator->next = (struct endpoint_device *)
		kzalloc(sizeof(struct endpoint_device), GFP_KERNEL);
		iterator = iterator->next;
	}

	if (iterator == NULL) {
		pr_err("rpmsg: out of memory\n");
		return -1;
	}

	iterator->next = NULL;
	iterator->type = type;
	iterator->pid = pid;

	if (kfifo_alloc(&iterator->fifo, FIFO_SIZE, GFP_KERNEL)) {
		pr_err("rpmsg: cannot allocate kfifo\n");
		goto ept_add_err_fifo;
	}

	if (type == ENDPOINT_DGRAM) {
		if (kfifo_alloc(&iterator->sig_fifo,
				SIG_FIFO_SIZE,
				GFP_KERNEL)) {
			pr_err("rpmsg: cannot allocate signal kfifo\n");
			goto ept_add_err_sig_fifo;
		}
	}

	if (new_ept == NULL) {

		struct rpmsg_channel_info chinfo;
		//FIXME
		strncpy(chinfo.name, "fixme", sizeof("fixme"));
		chinfo.src = addr;
		chinfo.dst = RPMSG_ADDR_ANY;

		ept = rpmsg_create_ept(chnl_dev->chnl,
					rpmsg_rx_cb,
					iterator, chinfo);
		if (NULL == ept) {
			pr_err("rpmsg: cannot create ept\n");
			goto ept_add_err_early;
		}
	} else {
		new_ept->priv = iterator;
		ept = new_ept;
	}
	/* by default bind to remote default endpoint */
	iterator->src = chnl_dev->chnl->dst;
	/* by default send to remote default endpoint */
	iterator->dst = chnl_dev->chnl->dst;
	/* keep reference to channel dev */
	iterator->chnl_dev = chnl_dev;

	iterator->ept = ept;

	ASSERT(chnl_dev);
	ASSERT(!(chnl_dev->id & (~RPMSG_MINOR_CHANNEL_MASK)));
	ASSERT(!(ept->addr & (~RPMSG_MINOR_EPT_MASK)));
	iterator->major = MAJOR(rpmsg_major);
	iterator->minor = (ept->addr << RPMSG_MINOR_EPT_BITS) |
					(chnl_dev->id);

	cdev_init(&iterator->cdev, &device_ops);
	iterator->cdev.owner = THIS_MODULE;

	if (cdev_add(&iterator->cdev,
		MKDEV(iterator->major, iterator->minor), 1)) {
		pr_err("rpmsg: chardev registration failed.\n");
		goto ept_add_err;
	}

	mutex_init(&iterator->lock);

	ASSERT(rpmsg_class);
	ASSERT(ept);
	ASSERT(iterator);
	iterator->dev = device_create(rpmsg_class,
		chnl_dev->chnl_dev,
		MKDEV(iterator->major, iterator->minor),
		iterator,
		"rpmsg_ept%d.%d",
		ept->addr, chnl_dev->id);

	if (iterator->dev == NULL) {
		pr_err("rpmsg: cannot create device\n");
		goto ept_dev_err;
	}

	retval = device_create_file(iterator->dev,
				&dev_attr_channel);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto ept_add_err;
	}

	retval = device_create_file(iterator->dev,
				&dev_attr_address);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto ept_add_err;
	}

	retval = device_create_file(iterator->dev,
				&dev_attr_dst);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto ept_add_err;
	}

	retval = device_create_file(iterator->dev,
				&dev_attr_src);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto ept_add_err;
	}

	retval = device_create_file(iterator->dev,
				&dev_attr_type);
	if (retval < 0) {
		pr_err("rpmsg: could not add devattr.\n");
		goto ept_add_err;
	}


	return ept->addr;

ept_add_err:
	/* we dynamically created ept, clean up */
	if (new_ept == NULL)
		rpmsg_destroy_ept(ept);
ept_dev_err:
	put_device(iterator->dev);
ept_add_err_early:
	kfifo_free(&iterator->sig_fifo);
ept_add_err_sig_fifo:
	kfifo_free(&iterator->fifo);
ept_add_err_fifo:
	if (*base == iterator) {
		*base = NULL;
	} else {
		ASSERT(prev);
		prev->next = NULL;
	}
	kfree(iterator);

	return -1;
}

void endpoint_device_free(struct endpoint_device *rm)
{
	pr_debug("rpmsg: called %s\n", __func__);

	ASSERT(rm);
	ASSERT(rm->ept);
	ASSERT(rm->ept->rpdev);

	device_unregister(rm->dev);

	cdev_del(&rm->cdev);

	/* This is not the default endpoint, free it */
	if (rm->ept->rpdev->ept != rm->ept)
		rpmsg_destroy_ept(rm->ept);


	kfifo_free(&rm->fifo);

	if (rm->type == ENDPOINT_DGRAM)
		kfifo_free(&rm->sig_fifo);

	kfree(rm);
}

static int endpoint_list_rm(struct endpoint_device **base, int rm_addr,
			pid_t pid)
{
	struct endpoint_device *iterator = NULL;
	struct endpoint_device *prev = NULL;

	pr_debug("rpmsg: called %s\n", __func__);

	for (iterator = *base;
		iterator != NULL;
		prev = iterator, iterator = iterator->next) {
		if (iterator->ept->addr == rm_addr) {
			if (pid != iterator->pid) {
				pr_err("rpmsg:");
				pr_err("PID err, have %d, should have %d\n",
					pid,
					iterator->pid);
				return -1;
			}

			if ((iterator == *base) &&
				(iterator->next == NULL)) {
				*base = NULL;
				endpoint_device_free(iterator);
			} else {
				prev->next = iterator->next;
				endpoint_device_free(iterator);
			}
			return 0;
		}
	}
	return -1;
}


void channel_device_free(struct endpoint_device **base,
			struct channel_device *rm)
{
	struct endpoint_device *iterator = NULL;
	struct endpoint_device *prev = NULL;

	pr_debug("rpmsg: called %s\n", __func__);

	for (iterator = *base;
		iterator != NULL;
		prev = iterator, iterator = iterator->next) {
		if (iterator->ept->rpdev == rm->chnl) {
			if ((iterator == *base) && (iterator->next == NULL)) {
				*base = NULL;
				endpoint_device_free(iterator);
				break;
			}

			prev->next = iterator->next;
			endpoint_device_free(iterator);
		}
	}

	device_unregister(rm->chnl_dev);
	kfree(rm);
}

static int channel_list_rm(struct channel_device **base,
			struct endpoint_device **ept_base,
			struct rpmsg_device *chnl)
{
	struct channel_device *iterator = NULL;
	struct channel_device *prev = NULL;

	pr_debug("rpmsg: called %s\n", __func__);

	for (iterator = *base;
		iterator != NULL;
		prev = iterator, iterator = iterator->next) {
		if (iterator->chnl == chnl) {
			if ((iterator == *base) && (iterator->next == NULL)) {
				*base = NULL;
				channel_device_free(ept_base, iterator);
			} else {
				prev->next = iterator->next;
				channel_device_free(ept_base, iterator);
			}
			return 0;
		}
	}

	return -1;
}


static int device_open(struct inode *inode, struct file *filp)
{
	struct endpoint_device *ept_dev = NULL;

	ASSERT(inode);
	ASSERT(filp);

	pr_debug("rpmsg: called %s\n", __func__);

	ept_dev = container_of(inode->i_cdev,
				struct endpoint_device,
				cdev);

	/* Ensure that only one process
	 * has access to our device at any one time
	 */
	if (!mutex_trylock(&ept_dev->lock)) {
		pr_err(
		"rpmsg: another process is accessing the device\n"
		);
		return -EBUSY;
	}

	filp->private_data = ept_dev;
	return 0;
}

static int device_close(struct inode *inode, struct file *filp)
{
	struct endpoint_device *ept_dev = NULL;

	ASSERT(inode);
	ASSERT(filp);

	pr_debug("rpmsg: called %s\n", __func__);

	ept_dev = (struct endpoint_device *)filp->private_data;

	mutex_unlock(&ept_dev->lock);
	return 0;
}

static ssize_t device_read(struct file *filp,
			char __user *buffer,
			size_t length,
			loff_t *offset)
{
	int copied = 0;
	int ret = -1;
	u32 src_addr = -1;
	int message_length = -1;
	struct endpoint_device *ept_dev = NULL;

	ASSERT(buffer);
	ASSERT(filp);

	pr_debug("rpmsg: called %s\n", __func__);

	ept_dev = (struct endpoint_device *)filp->private_data;

	if (kfifo_is_empty(&ept_dev->fifo)) {
		pr_debug("rpmsg: FIFO for this ept empty\n");
		return 0;
	}

	pr_debug("rpmsg: data available in FIFO\n");
	if (ept_dev->type == ENDPOINT_DGRAM) {
		ret = kfifo_out(&ept_dev->sig_fifo, &src_addr, sizeof(u32));
		if (ret != sizeof(u32)) {
			pr_err("rpmsg: signal fifo error addr, %d\n",
				ret);
			return -1;
		}

		ret = kfifo_out(&ept_dev->sig_fifo,
				&message_length,
				sizeof(int));

		if (ret != sizeof(int)) {
			pr_err("rpmsg: signal fifo error len, %d\n",
				ret);
			return -1;
		}

		length = (length < message_length) ? length : message_length;
		ept_dev->src = (int)src_addr;

		/* Read from FIFO */
		ret = kfifo_to_user(&ept_dev->fifo, buffer, length, &copied);
		pr_debug(
		"rpmsg: DGRAM kfifo_to_user returns %d, copied %d\n",
		ret, copied);

	} else {
		/* Read from FIFO */
		ret = kfifo_to_user(&ept_dev->fifo, buffer, length, &copied);
		pr_debug(
		"rpmsg: STREAM kfifo_to_user returns %d, copied %d\n",
		ret, copied);
	}

	return ret ? ret : copied;
}

static ssize_t device_write(struct file *filp, const char __user *buffer,
			size_t length, loff_t *offset)
{
	int retval = -1;
	size_t size = 0;
	struct endpoint_device *ept_dev = NULL;

	ASSERT(buffer);
	ASSERT(filp);

	pr_debug("rpmsg: called %s\n", __func__);

	ept_dev = (struct endpoint_device *)filp->private_data;

	ASSERT(ept_dev);

	if (length < MAX_RPMSG_BUFF_SIZE)
		size = length;
	else
		size = MAX_RPMSG_BUFF_SIZE;


	if (copy_from_user(ept_dev->txbuff, buffer, size)) {
		pr_err("rpmsg: user to kernel buff copy error.\n");
		return -1;
	}

	pr_debug("rpmsg: rpmsg_send_offchannel with size %d, dst %d\n",
		size, ept_dev->dst);

	/* Send RPMSG message */
	//FIXME
	retval = rpmsg_send_offchannel(ept_dev->ept,
                ept_dev->ept->addr,
                ept_dev->dst,
				(void *)ept_dev->txbuff,
				size);

	pr_debug("rpmsg: rpmsg_send_offchannel returns %d\n", retval);

	return (retval == 0) ? size : -1;
}


static ssize_t dev_attr_dst_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct endpoint_device *ept_dev = NULL;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	ept_dev =  (struct endpoint_device *)dev_get_drvdata(dev);
	return snprintf(buffer, PAGE_SIZE, "%d\n", ept_dev->dst);
}

static ssize_t dev_attr_src_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct endpoint_device *ept_dev = NULL;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	ept_dev =  (struct endpoint_device *)dev_get_drvdata(dev);
	return snprintf(buffer, PAGE_SIZE, "%d\n", ept_dev->src);
}

static ssize_t dev_attr_local_default_addr_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct channel_device *chnl_dev = NULL;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	chnl_dev =  (struct channel_device *)dev_get_drvdata(dev);

	ASSERT(chnl_dev);
	ASSERT(chnl_dev->chnl);
	ASSERT(chnl_dev->chnl->ept);

	pr_debug("rpmsg: local default channel addr %d\n",
		chnl_dev->chnl->ept->addr);

	return snprintf(buffer, PAGE_SIZE, "%d\n",
			chnl_dev->chnl->ept->addr);
}


static ssize_t dev_attr_remote_default_addr_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct channel_device *chnl_dev = NULL;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	chnl_dev =  (struct channel_device *)dev_get_drvdata(dev);

	ASSERT(chnl_dev);
	ASSERT(chnl_dev->chnl);
	ASSERT(chnl_dev->chnl->ept);

	pr_debug("rpmsg: remote default channel addr %d\n",
		chnl_dev->chnl->dst);

	return snprintf(buffer, PAGE_SIZE, "%d\n",
			chnl_dev->chnl->dst);
}

static ssize_t dev_attr_name_show(struct device *dev,
			struct device_attribute *attr, char *buffer)
{
	struct channel_device *chnl_dev = NULL;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	chnl_dev =  (struct channel_device *)dev_get_drvdata(dev);

	ASSERT(chnl_dev);
	ASSERT(chnl_dev->chnl);


	return snprintf(buffer, PAGE_SIZE, "%s\n",
			chnl_dev->chnl->id.name);
}

static ssize_t dev_attr_ept_delete_store(struct device *dev,
			struct device_attribute *attr,
			const char *buffer, size_t size)
{
	int ret = -1;
	int rm_addr = -1;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	ret = kstrtoint(buffer, 10, &rm_addr);
	if (ret != 0) {
		pr_err("rpmsg: scanf error\n");
		return -1;
	}

	/* delete endpoint here and remove it from the list */
	ret = endpoint_list_rm(&endpoint_list, rm_addr, current->pid);
	if (ret == -1) {
		pr_err("rpmsg: likely bad endpoint number\n");
		return -1;
	}

	return size;
}

static ssize_t dev_attr_ept_new_store(struct device *dev,
			struct device_attribute *attr,
			const char *buffer, size_t size)
{
	struct channel_device *chnl_dev = NULL;
	int ept_addr = -1;
	int retval = -1;
	char type;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	chnl_dev =  (struct channel_device *)dev_get_drvdata(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	retval = sscanf(buffer, "%d,%c", &ept_addr, &type);
	if (retval == -1) {
		pr_err("rpmsg: scanf error\n");
		return -1;
	}

	if (ept_addr < 0) {
		pr_err("rpmsg: negative addr\n");
		return -1;
	}

	ASSERT(chnl_dev);
	ASSERT(chnl_dev->chnl);
	ASSERT(chnl_dev->chnl->ept);

	pr_debug("rpmsg: adding endpoint %d, default %d\n",
		ept_addr, chnl_dev->chnl->ept->addr);

	if (ept_addr == chnl_dev->chnl->ept->addr) {
		pr_debug("rpmsg: adding default endpoint\n");

		retval = endpoint_list_add(&endpoint_list, chnl_dev,
					chnl_dev->chnl->ept,
					ept_addr,
					(type == 'S') ?
					ENDPOINT_STREAM : ENDPOINT_DGRAM,
					current->pid);
	} else {
		pr_debug("rpmsg: adding regular endpoint\n");
		retval = endpoint_list_add(&endpoint_list, chnl_dev, NULL,
					ept_addr,
					(type == 'S') ?
					ENDPOINT_STREAM : ENDPOINT_DGRAM,
					current->pid);
	}

	if (retval != -1)
		return size;
	else
		return -1;
}

static ssize_t dev_attr_dst_store(struct device *dev,
			struct device_attribute *attr,
			const char *buffer, size_t size)
{
	struct endpoint_device *ept_dev = NULL;
	int ret = -1;
	int dst = 0;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	ept_dev = (struct endpoint_device *)dev_get_drvdata(dev);
	ret = kstrtoint(buffer, 10, &dst);
	if (ret != 0)
		return -1;

	/* TODO validation of dst here? */
	ept_dev->dst = dst;
	return size;
}

static ssize_t dev_attr_src_store(struct device *dev,
			struct device_attribute *attr,
			const char *buffer, size_t size)
{
	struct endpoint_device *ept_dev = NULL;
	int ret = -1;
	int src = 0;

	ASSERT(buffer);
	ASSERT(attr);
	ASSERT(dev);

	pr_debug("rpmsg: called %s\n", __func__);

	ept_dev = (struct endpoint_device *)dev_get_drvdata(dev);
	ret = kstrtoint(buffer, 10, &src);
	if (ret != 0)
		return -1;

	/* TODO validation of src here? */
	ept_dev->src = src;
	return size;
}

static int rpmsg_rx_cb(struct rpmsg_device *chnl, void *data,
			int len, void *priv, u32 src)
{
	int retlen = -1;
	struct endpoint_device *ept_dev = NULL;

	pr_debug("rpmsg: called %s\n", __func__);

	ASSERT(data);
	ASSERT(chnl);

	if (priv == NULL) {
		pr_info("rpmsg: data at unexported default endpoint");
		pr_info(", dropping...\n");
		//return;
		return -EINVAL;
	}

	if (!len) {
		pr_info("rpmsg: zero-length msg from src %d\n", src);
		//return;
		return -EINVAL;
	}

	ept_dev = (struct endpoint_device *)priv;

	pr_debug("rpmsg: msg from src %d, length %d\n", src, len);

	if (ept_dev->type == ENDPOINT_STREAM) {
		if (ept_dev->src != src) {
			pr_info("rpmsg:");
			pr_info("msg from src %d, expected from %d, drop.\n",
				src, ept_dev->src);
			//return;
			return -EINVAL;
		}

		/* ept_dev contains reference to a FIFO
		 * associated with this endpoint, write data there
		 */
		retlen = kfifo_in(&ept_dev->fifo, data, len);
		pr_debug("rpmsg: kfifo_in returns %d, requested %d\n",
			retlen, len);

		if (retlen < len) /* Too fast, dropping...  */
		{
			pr_info("rpmsg: dropping...\n");
			return -EINVAL;
		}
	} else { /* ENDPOINT_DGRAM */
		if (kfifo_avail(&ept_dev->fifo) <
			 (sizeof(u32) + sizeof(int))) {
			pr_info("rpmsg: signal fifo dropping...\n");
			//return;
			return -EINVAL;
		}

		if (kfifo_avail(&ept_dev->fifo) < len) {
			pr_info("rpmsg: dropping...\n");
			//return;
			return -EINVAL;
		}

		retlen = kfifo_in(&ept_dev->sig_fifo, &src, sizeof(u32));
		if (retlen != sizeof(u32)) {
			pr_err("rpmsg: signal fifo error1\n");
			//return;
			return -EINVAL;
		}

		retlen = kfifo_in(&ept_dev->sig_fifo, &len, sizeof(int));
		if (retlen != sizeof(int)) {
			pr_err("rpmsg: signal fifo error2\n");
			//return;
			return -EINVAL;
		}

		pr_debug("rpmsg: msg from src %d, len %d.\n",
			src, len);
		retlen = kfifo_in(&ept_dev->fifo, data, len);
		pr_debug("rpmsg: kfifo_in returns %d, requested %d\n",
			retlen, len);

		if (retlen < len) /* Too fast, dropping...  */
		{
			pr_info("rpmsg: dropping...\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int rpmsg_multiept_probe(struct rpmsg_device *rpdev)
{
	pr_info("rpmsg: new channel 0x%x -> 0x%x\n",
		rpdev->src, rpdev->dst);

	/* add channel to the sysfs, rpchnl */
	/* export enpoint create and endpoint destroy interfaces
	 * and create validation functions here
	 */
	channel_list_add(&channel_list, rpdev);

	return 0;
}

static void rpmsg_multiept_remove(struct rpmsg_device *chnl)
{
	int ret = -1;

	ASSERT(chnl);
	pr_debug("rpmsg: called %s\n", __func__);

	/* remove channel from the sysfs */
	ret = channel_list_rm(&channel_list, &endpoint_list, chnl);
	ASSERT(ret == 0); /* should always pass */
}

static struct rpmsg_device_id rpmsg_driver_sysfs_id_table[] = {
	{ .name	= "iotgw-rpmsg-openamp-channel" },
	{},
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_sysfs_id_table);

static struct rpmsg_driver rpmsg_multiept_client = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_driver_sysfs_id_table,
	.probe		= rpmsg_multiept_probe,

	/* callback for the default endpoint,
	 * set it to our common rpmsg_rx_cb()
	 */
	.callback	= rpmsg_rx_cb,

	.remove		= rpmsg_multiept_remove,
};

static int __init rpmsg_multiept_init(void)
{
	int retval = 0;

	channel_list = NULL;
	endpoint_list = NULL;
	rpmsg_major = 0;
	rpmsg_class = NULL;

	pr_debug("rpmsg: called %s\n", __func__);

	/* Allocate char device for this rpmsg driver */
	if (alloc_chrdev_region(&rpmsg_major, 0,
				RPMSG_MAX_NUM_EPTS,
				KBUILD_MODNAME) < 0) {
		pr_err("rpmsg: error allocating char device\n");
		goto rpmsg_init_err;
	}

	rpmsg_class = class_create(THIS_MODULE, "rpmsg");
	if (rpmsg_class == NULL) {
		pr_err("rpmsg driver cannot create class\n");
		goto rpmsg_init_err;
	}

	retval = register_rpmsg_driver(&rpmsg_multiept_client);
	if (retval) {
		pr_err("rpmsg driver could not be registered\n");
		goto rpmsg_init_err;
	}

rpmsg_init_err:

	return retval;
}
module_init(rpmsg_multiept_init);

static void __exit rpmsg_multiept_fini(void)
{
	ASSERT(rpmsg_class);
	class_destroy(rpmsg_class);
	unregister_rpmsg_driver(&rpmsg_multiept_client);
	pr_info("rpmsg: cleanup, goodbye.\n");
}
module_exit(rpmsg_multiept_fini);

MODULE_DESCRIPTION("Remote processor messaging sysfs client driver");
MODULE_LICENSE("GPL v2");
