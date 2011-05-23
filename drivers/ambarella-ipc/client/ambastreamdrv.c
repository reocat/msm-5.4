
/*
 * ambastreamdrv.c
 *
 * History:
 *	2011/4/7 - [Keny Huang] created file
 *
 * Copyright (C) 2007-2011, Ambarella, Inc.
 *
 * All rights reserved. No Part of this file may be reproduced, stored
 * in a retrieval system, or transmitted, in any form, or by any means,
 * electronic, mechanical, photocopying, recording, or otherwise,
 * without the prior consent of Ambarella, Inc.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>

extern int ipc_i_streamer_get_iavpool_info(unsigned char **base_addr, unsigned int *size);
extern int ipc_i_streamer_init(void);
extern void ipc_i_streamer_cleanup(void);


static const char *ambastrdrv_name = "ambastreamdrv";
//static int iav_major = 248;
//static int iav_minor = 0;
static struct cdev ambastrdrv_cdev;
static dev_t dev_id;
static unsigned char *iavpool_baseaddr = NULL;
static unsigned int iavpool_size = 0;

//DEFINE_MUTEX(iav_mutex);

static long ambastrdrv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
/*	int rval;
//	mutex_lock(&iav_mutex);

	switch (cmd) {
	case IAV_IOC_GET_UCODE_INFO:
		rval = copy_to_user((ucode_load_info_t __user*)arg,
			dsp_get_ucode_info(), sizeof(ucode_load_info_t)) ? -EFAULT : 0;
		break;

	case IAV_IOC_GET_UCODE_VERSION:
		rval = copy_to_user((ucode_version_t __user*)arg,
			dsp_get_ucode_version(), sizeof(ucode_version_t)) ? -EFAULT : 0;
		break;

	case IAV_IOC_UPDATE_UCODE:
		if (ucode_user_start == 0)
			rval = -EPERM;
		else {
			clean_d_cache((void*)ucode_user_start, dsp_get_ucode_size());
			rval = 0;
		}
		break;

	default:
		rval = -ENOIOCTLCMD;
		break;
	}

//	mutex_unlock(&iav_mutex);
	return rval;
	*/
	printk("%s\n",__func__);
	return 0;
}

#ifdef CONFIG_ARM_DMA_MEM_BUFFERABLE
extern pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
			      unsigned long size, pgprot_t vma_prot);
#endif

static int ambastrdrv_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int rval;
	unsigned long size;

	//iavpool must map as read-only area!
	//if((vma->vm_flags & VM_WRITE)!=0){
	//	printk("ERR(%s): the memory area must be mapped as READ-ONLY (PROT_READ)!!\n",__func__);
	//	return -EACCES;
	//}
	
//	mutex_lock(&iav_mutex);
//printk("%s\n",__func__);
	if(ipc_i_streamer_get_iavpool_info(&iavpool_baseaddr, &iavpool_size)<0){
		printk("ipc_i_streamer_get_iavpool_info() fail\n");
		rval = -EINVAL;
		goto Done;
	}
	
	printk("%s: iavpool_baseaddr=%p, iavpool_size=%x\n",__func__, iavpool_baseaddr, iavpool_size);
	
	size = vma->vm_end - vma->vm_start;
	if (size != iavpool_size) {
		rval = -EINVAL;
		goto Done;
	}

	//if(filp->f_flags & O_SYNC){
	//	vma->vm_flags |= VM_IO;
	//}
	//vma->vm_flags |= VM_RESERVED;

#ifdef CONFIG_ARM_DMA_MEM_BUFFERABLE
	printk("before phys_mem_access_prot\n");
	vma->vm_page_prot = phys_mem_access_prot(filp, vma->vm_pgoff,
						 size,
						 vma->vm_page_prot);
#endif

	vma->vm_pgoff = ((int)iavpool_baseaddr) >> PAGE_SHIFT;
	if ((rval = remap_pfn_range(vma,
			vma->vm_start,
			vma->vm_pgoff,
			size,
			vma->vm_page_prot)) < 0) {
		goto Done;
	}

	//ucode_user_start = vma->vm_start;
	rval = 0;

Done:
//	mutex_unlock(&iav_mutex);
	return rval;
}

static int ambastrdrv_open(struct inode *inode, struct file *filp)
{
printk("%s\n",__func__);
	return 0;
}

static int ambastrdrv_release(struct inode *inode, struct file *filp)
{
printk("%s\n",__func__);
	return 0;
}

static struct file_operations ambastrdrv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ambastrdrv_ioctl,
	.mmap = ambastrdrv_mmap,
	.open = ambastrdrv_open,
	.release = ambastrdrv_release,
};

static int __init __ambastrdrv_init(void)
{
	
	int rval;
	
	ipc_i_streamer_init();
	//if (*major) {
	//	dev_id = MKDEV(*major, minor);
	//	rval = register_chrdev_region(dev_id, numdev, name);
	//} else {
		rval = alloc_chrdev_region(&dev_id, 0, 1, ambastrdrv_name);
		//major = MAJOR(dev_id);
	//}

	if (rval) {
		//LOG_ERROR("failed to get dev region for %s\n", name);
		return rval;
	}

	cdev_init(&ambastrdrv_cdev, &ambastrdrv_fops);
	//ambastrdrv_cdev.owner = THIS_MODULE;
	rval = cdev_add(&ambastrdrv_cdev, dev_id, 1);
	if (rval) {
		//LOG_ERROR("cdev_add failed for %s, error = %d\n", name, rval);
		return rval;
	}

	printk("%s dev init done, dev_id = %d:%d\n", ambastrdrv_name, MAJOR(dev_id), MINOR(dev_id));
	return 0;
}

static void __exit __ambastrdrv_exit(void)
{
	ipc_i_streamer_cleanup();
}

module_init(__ambastrdrv_init);
module_exit(__ambastrdrv_exit);

MODULE_AUTHOR("Keny Huang <skhuang@ambarella.com>");
MODULE_DESCRIPTION("Ambarella streaming driver");
MODULE_LICENSE("GPL");

