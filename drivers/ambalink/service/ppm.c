#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/ppm.h>
#include <linux/uaccess.h>
#include <linux/of.h>


MODULE_AUTHOR("Joey Li");
MODULE_LICENSE("GPL");

#define DEVICE_NAME "ppm"

static unsigned int     ppm_major;
static struct class*    ppm_class;
static struct device*   ppm_device;
static struct PPM_MEM_INFO_s mem_info;

static DEFINE_MUTEX(amba_ppm_mutex);

static int ppm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int rval = 0;

	mutex_lock(&amba_ppm_mutex);

	if (filp->f_flags & O_DSYNC) {
		pr_debug("PPM: pgprot_noncached\n");
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	}
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	rval = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
                        vma->vm_end - vma->vm_start, vma->vm_page_prot);
	if (rval) {
		printk("PPM: mmap failed with error %d.\n", rval);
		goto Done;
	}

        pr_debug("PPM: remap [%x---%x]\n", (unsigned int)vma->vm_start, (unsigned int)vma->vm_end);
Done:
        mutex_unlock(&amba_ppm_mutex);
	return rval;
}

static long ppm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    __be32 *reg;
    int len;
    struct device_node *node;

    mutex_lock(&amba_ppm_mutex);
    switch (cmd) {
        case PPM_GET_MEMIO_INFO:
            /* Get memio mem range */
            node = of_find_node_by_name(NULL, "app_shm");
            if(!node) {
                printk("err: memio_shm is NULL\n");
                goto Done;
            }

            reg = (__be32 *) of_get_property(node, "reg", &len);
            if (WARN_ON(!reg || (len != 2 * sizeof(u32)))) {
                printk("err: memio_shm wrong format\n");
                goto Done;
            }

            mem_info.phys = be32_to_cpu(reg[0]);
            mem_info.size = be32_to_cpu(reg[1]);

            pr_debug("PPM: MEMIO MEM RANGE:   [0x%08lx--0x%08lx]\n",
                   mem_info.phys, mem_info.phys + mem_info.size);
            pr_debug("PPM: ioctl: virt_addr= 0x%lx, phys_addr= 0x%lx, size= 0x%lx\n",
                mem_info.virt, mem_info.phys, mem_info.size);
            if(copy_to_user((void **)arg, &mem_info, sizeof(struct PPM_MEM_INFO_s))) {
                ret = -EFAULT;
            }
            break;
        default:
            printk("%s: unknown command 0x%08x", __func__, cmd);
            ret = -EINVAL;
            break;
    }

Done:
    mutex_unlock(&amba_ppm_mutex);

    return ret;
}


static const struct file_operations ppm_fops = {
        .owner = THIS_MODULE,
        .mmap = ppm_mmap,
        .unlocked_ioctl = ppm_ioctl,
};

static int  __init ppm_init(void)
{
        ppm_major = register_chrdev(0, DEVICE_NAME, &ppm_fops);
        if (ppm_major < 0) {
                printk("PPM: failed to register device %d.\n", ppm_major);
                return ppm_major;
        }

        ppm_class = class_create(THIS_MODULE, DEVICE_NAME);
        if (IS_ERR(ppm_class)) {
                unregister_chrdev(ppm_major, DEVICE_NAME);
                printk("PPM: failed to create class.\n");
                return PTR_ERR(ppm_class);
        }

        ppm_device = device_create(ppm_class, NULL, MKDEV(ppm_major, 0),
                                NULL, DEVICE_NAME);
        if (IS_ERR(ppm_device)) {
                class_destroy(ppm_class);
                unregister_chrdev(ppm_major, DEVICE_NAME);
                printk("PPM: falied to create device.\n");
                return PTR_ERR(ppm_device);
        }

        printk("PPM: init with major = %d.\n", ppm_major);
        return 0;
}

static void __exit ppm_exit(void)
{
        device_destroy(ppm_class, MKDEV(ppm_major, 0));
        class_destroy(ppm_class);
        unregister_chrdev(ppm_major, DEVICE_NAME);
        printk("PPM exit\n");
        return;
}

module_init(ppm_init);
module_exit(ppm_exit);
