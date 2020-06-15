
/*
 * SNVS Tamper Detection
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc., All Rights Reserved
 */

#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "compat.h"
#include "intern.h"
#include "tamper.h"
#include "regs.h"

static struct snvs_secvio_drv_private *svpriv;
static struct device *svdev;
static struct snvs_expanded __iomem *snvsexregs;
static int tester_lock;
static int tamper_pin;

static int tamper_major;
static struct class *tamper_class;

void tamper_detect_handler(void)
{
	u32 val;
	
	val = rd_reg32(&svpriv->svregs->hp.secvio_intcfg);
	wr_reg32(&svpriv->svregs->hp.secvio_intcfg, val & ~(0x80000000));
	
	if (rd_reg32(&svpriv->svregs->lp.status) & 0x600)
		printk("tamper detected!\n");
}
EXPORT_SYMBOL(tamper_detect_handler);

static long tamper_ioctl(struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	int errval = 0;
	u32 val;
	
	switch (cmd) {
		case TAMPER_IOCTL_PASSIVE_EN:
		{
			tamper_passive passive;
			
			if (copy_from_user(&passive, (tamper_passive *) arg, sizeof(tamper_passive)))
				return -EFAULT;
			
			if((tamper_pin != 0) && tester_lock) {
				printk("tamper fuse is programmed, tamper shadow register is locked, can't verify tamper function\n");
				return -EINVAL;
			}
			
			if (passive.rx > 1){
				printk("tamper pin should be 0 or 1 \n");
				return -EINVAL;
			}
			
			val = rd_reg32(&svpriv->svregs->lp.tamper_filt_cfg);
			if (passive.rx == 0)
				val |= 0x800000;
			else if (passive.rx == 1)
				val |= 0x80000000;
			wr_reg32(&svpriv->svregs->lp.tamper_filt_cfg, val);
			
	
			if (passive.polarity)
			{
				val = rd_reg32(&svpriv->svregs->lp.tamper_det_cfg);
				if (passive.rx == 0)
					val |= 0x800;
				else if (passive.rx == 1)
					val |= 0x1000;
				wr_reg32(&svpriv->svregs->lp.tamper_det_cfg, val);
				
			}
			
			
			val = rd_reg32(&svpriv->svregs->lp.tamper_det_cfg);
			if (passive.rx == 0)
				val |= 0x200;
			else if (passive.rx == 1)
				val |= 0x400;
			wr_reg32(&svpriv->svregs->lp.tamper_det_cfg, val);
			
			val = rd_reg32(&svpriv->svregs->hp.secvio_intcfg);
			wr_reg32(&svpriv->svregs->hp.secvio_intcfg, val | 0x80000000);
			break;
		}
		case TAMPER_IOCTL_GET_STATUS:
		{
			tamper_status status;
			
			status.lpsr = rd_reg32(&svpriv->svregs->lp.status) & 0x7f0;
			status.lptdsr = rd_reg32(&svpriv->svregs->lp.tamper_det_status) & 0xff;
			
			if (copy_to_user((tamper_status *) arg, &status, sizeof(tamper_status)))
				return -EFAULT;
			break;
		}
		case TAMPER_IOCTL_CLEAR_STATUS:
		{
			tamper_status status;
			
			if (copy_from_user(&status, (tamper_status *) arg, sizeof(tamper_status)))
				return -EFAULT;
			
			wr_reg32(&svpriv->svregs->lp.status, status.lpsr & 0x7f0);
			wr_reg32(&svpriv->svregs->lp.tamper_det_status, status.lptdsr & 0xff);
			break;
		}
		case TAMPER_IOCTL_CLEAR_GLITCH_STATUS:
		{
			tamper_status status;
			
			if (copy_from_user(&status, (tamper_status *) arg, sizeof(tamper_status)))
				return -EFAULT;
			
			wr_reg32(&svpriv->svregs->lp.pwr_glitch_det, SNVS_LPPGDR_INIT);
			wr_reg32(&svpriv->svregs->lp.status, 0xffffffff);
			break;
		}
		default:
			break;
	}
	
	return errval;
}

static int tamper_mmap(struct file *file, struct vm_area_struct *vma)
{
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot)) {
		printk(KERN_ERR
				"mmap failed!\n");
		return -ENOBUFS;
	}
	return 0;
}

static int tamper_open(struct inode *inode, struct file *file)
{
	int errval = 0;
	
	return errval;
}

static int tamper_release(struct inode *inode, struct file *file)
{
	int errval = 0;
	
	return errval;
}

static const struct file_operations tamper_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tamper_ioctl,
	.mmap = tamper_mmap,
	.open = tamper_open,
	.release = tamper_release,
};

static void __exit snvs_tamper_exit(void)
{
	iounmap(snvsexregs);
	
	device_destroy(tamper_class, MKDEV(tamper_major, 0));
	class_destroy(tamper_class);
	unregister_chrdev(tamper_major, "tamper");
}

static int __init snvs_tamper_init(void)
{
	int err = 0;
	struct device_node *dev_node;
	struct platform_device *pdev;
	void __iomem *ocotpaddr = NULL;
	u32 val, trim = 0;
	
	dev_node = of_find_compatible_node(NULL, NULL, "fsl,imx6q-caam-snvs");
	if (!dev_node)
		return -ENODEV;
	
	pdev = of_find_device_by_node(dev_node);
	if (!pdev) {
		of_node_put(dev_node);
		return -ENODEV;
	}
	
	svdev = &pdev->dev;
	svpriv = dev_get_drvdata(svdev);
	if (!svpriv) {
		of_node_put(dev_node);
		return -ENODEV;
	}
	
	dev_node = of_find_compatible_node(NULL, NULL, "fsl,imx6sx-snvs");
	if (!dev_node)
		return -ENODEV;
	
	snvsexregs = of_iomap(dev_node, 0);
	if (!snvsexregs)
		return -ENOMEM;
	
	dev_node = of_find_compatible_node(NULL, NULL, "fsl,imx6sx-ocotp");
	if (!dev_node)
		return -ENODEV;
	
	ocotpaddr = of_iomap(dev_node, 0);
	if (!ocotpaddr)
		return -ENOMEM;
	
	tester_lock = rd_reg32(ocotpaddr + 0x400) & 0x2;
	tamper_pin = (rd_reg32(ocotpaddr + 0x430) & 0x300000) >> 20;
	trim = rd_reg32(ocotpaddr + 0x490);
	
	if((tamper_pin != 0) && tester_lock)
		printk("tamper fuse is programmed, tamper shadow register is locked, can't verify tamper function\n");
	
	if((tamper_pin != 0) && (tester_lock == 0)) {
		val = rd_reg32(ocotpaddr + 0x430) & ~(0x300000);
		wr_reg32(ocotpaddr + 0x430, val);
	}
	
	iounmap(ocotpaddr);
	
	if (!trim)
		trim = 0x020081a1;
	
	val = rd_reg32(&svpriv->svregs->hp.secvio_intcfg);
	wr_reg32(&svpriv->svregs->hp.secvio_intcfg, val & ~(0x80000000));
	val = rd_reg32(&svpriv->svregs->hp.secvio_ctl);
	wr_reg32(&svpriv->svregs->hp.secvio_ctl, val | 0x40000000);
	
	tamper_major = register_chrdev(0, "tamper", &tamper_fops);
	if (tamper_major < 0) {
		printk("TAMPER: Unable to register driver\n");
		return -ENODEV;
	}
	tamper_class = class_create(THIS_MODULE, "tamper");
	if (IS_ERR(tamper_class)) {
		printk("TAMPER: Unable to create class\n");
		err = PTR_ERR(tamper_class);
		goto out_chrdev;
	}
	device_create(tamper_class, NULL, MKDEV(tamper_major, 0), NULL, "tamper");
	
	goto out;
	
out_chrdev:
	unregister_chrdev(tamper_major, "tamper");

out:
		return err;
}

module_init(snvs_tamper_init);
module_exit(snvs_tamper_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("FSL SNVS Tamper Detection");
MODULE_AUTHOR("Freescale Semiconductor - MCU");
	
