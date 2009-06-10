/*
 * linux/drivers/mmc/host/ambarella_sd.c
 *
 * Copyright (C) 2006-2007, Ambarella, Inc.
 *  Anthony Ginger, <hfjiang@ambarella.com>
 *
 * Ambarella Media Processor Watch Dog Timer
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#define CONFIG_WDT_AMBARELLA_TIMEOUT		(15)

static int init_tmo = CONFIG_WDT_AMBARELLA_TIMEOUT;
module_param(init_tmo, int, 0);
MODULE_PARM_DESC(init_tmo, "Watchdog timeout in seconds. default=" \
__MODULE_STRING(CONFIG_WDT_AMBARELLA_TIMEOUT) ")");

enum ambarella_wdt_state {
	AMBA_WDT_CLOSE_STATE_DISABLE,
	AMBA_WDT_CLOSE_STATE_ALLOW,
};
struct ambarella_wdt_info {
	unsigned char __iomem 		*regbase;

	struct device			*dev;
	struct resource			*mem;
	unsigned int			irq;

	struct semaphore		wdt_mutex;

	enum ambarella_wdt_state	state;

	u32				tmo;
	u32				boot_tmo;

	struct miscdevice		wdt_dev;
};
static struct ambarella_wdt_info wdt_info;

static void ambarella_wdt_keepalive(void)
{
	__raw_writel(wdt_info.tmo, wdt_info.regbase + WDOG_RELOAD_OFFSET);
	__raw_writel(0x4755, wdt_info.regbase + WDOG_RESTART_OFFSET);
}

static void ambarella_wdt_stop(void)
{
	__raw_writel(0, wdt_info.regbase + WDOG_CONTROL_OFFSET);
	while (__raw_readl(wdt_info.regbase + WDOG_CONTROL_OFFSET) != 0);
}

static void ambarella_wdt_start(void)
{
	u32				cnt_reg;

	ambarella_wdt_stop();
	ambarella_wdt_keepalive();

	cnt_reg = WDOG_CTR_INT_EN | WDOG_CTR_EN | WDOG_CTR_RST_EN;
	__raw_writel(cnt_reg, wdt_info.regbase + WDOG_CONTROL_OFFSET);
	while ((__raw_readl(wdt_info.regbase + WDOG_CONTROL_OFFSET) & cnt_reg) != cnt_reg);
}

static int ambarella_wdt_set_heartbeat(u32 timeout)
{
	u32				freq = get_apb_bus_freq_hz();
	u32				max_tmo;

	max_tmo = 0xFFFFFFFF / freq;

	if (timeout > max_tmo) {
		dev_err(wdt_info.dev, "max_tmo is %d.\n", max_tmo);
		return -EINVAL;
	}

	wdt_info.tmo = timeout * freq;
	ambarella_wdt_keepalive();

	return 0;
}

static int ambarella_wdt_open(struct inode *inode, struct file *file)
{
	if(down_trylock(&wdt_info.wdt_mutex))
		return -EBUSY;

	wdt_info.state = AMBA_WDT_CLOSE_STATE_DISABLE;

	ambarella_wdt_start();

	return nonseekable_open(inode, file);
}

static int ambarella_wdt_release(struct inode *inode, struct file *file)
{
	if (wdt_info.state == AMBA_WDT_CLOSE_STATE_ALLOW) {
		ambarella_wdt_stop();
	} else {
		printk(KERN_CRIT "Not stopping watchdog, V first!\n");
		ambarella_wdt_keepalive();
	}

	wdt_info.state = AMBA_WDT_CLOSE_STATE_DISABLE;
	up(&wdt_info.wdt_mutex);

	return 0;
}

static ssize_t ambarella_wdt_write(struct file *file, const char __user *data,
				size_t len, loff_t *ppos)
{
	if(len) {
		size_t i;

		for (i = 0; i < len; i++) {
			char c;

			if (get_user(c, data + i))
				return -EFAULT;

			if (c == 'V')
				wdt_info.state = AMBA_WDT_CLOSE_STATE_ALLOW;
		}

		ambarella_wdt_keepalive();
	}

	return len;
}

static struct watchdog_info ambarella_wdt_ident = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.firmware_version = 0,
	.identity = "ambarella-wdt",
};

static int ambarella_wdt_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int				errorCode = 0;
	void __user			*argp = (void __user *)arg;
	u32 __user			*p = argp;
	u32				new_tmo;

	switch (cmd) {
		case WDIOC_GETSUPPORT:
			errorCode = copy_to_user(argp, &ambarella_wdt_ident,
				sizeof(ambarella_wdt_ident)) ? -EFAULT : 0;
			break;

		case WDIOC_GETSTATUS:
			errorCode = put_user(0, p);
			break;

		case WDIOC_GETBOOTSTATUS:
			errorCode = put_user(wdt_info.boot_tmo, p);
			break;

		case WDIOC_KEEPALIVE:
			ambarella_wdt_keepalive();
			break;

		case WDIOC_SETTIMEOUT:
			if (get_user(new_tmo, p)) {
				errorCode = -EFAULT;
				break;
			}

			if (ambarella_wdt_set_heartbeat(new_tmo)) {
				errorCode = -EFAULT;
				break;
			}

			ambarella_wdt_keepalive();
			errorCode = put_user(wdt_info.tmo, p);
			break;

		case WDIOC_GETTIMEOUT:
			errorCode = put_user(wdt_info.tmo, p);
			break;

		default:
			return -ENOTTY;
	}

	return errorCode;
}

/* kernel interface */

static const struct file_operations ambarella_wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= ambarella_wdt_write,
	.ioctl		= ambarella_wdt_ioctl,
	.open		= ambarella_wdt_open,
	.release	= ambarella_wdt_release,
};

static irqreturn_t ambarella_wdt_irq(int irqno, void *param)
{
	printk(KERN_INFO "Watchdog timer expired!\n");

	__raw_writel(0x01, wdt_info.regbase + WDOG_CLR_TMO_OFFSET);

	return IRQ_HANDLED;
}

static int __devinit ambarella_wdt_probe(struct platform_device *pdev)
{
	int				errorCode;
	struct resource 		*irq;
	struct resource 		*mem;
	struct resource 		*ioarea;
	
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "Get WDT mem resource failed!\n");
		errorCode = -ENXIO;
		goto ambarella_wdt_na;
	}

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq == NULL) {
		dev_err(&pdev->dev, "Get WDT irq resource failed!\n");
		errorCode = -ENXIO;
		goto ambarella_wdt_na;
	}

	ioarea = request_mem_region(mem->start,
			(mem->end - mem->start) + 1, pdev->name);
	if (ioarea == NULL) {
		dev_err(&pdev->dev, "Request WDT ioarea failed!\n");
		errorCode = -EBUSY;
		goto ambarella_wdt_na;
	}

	wdt_info.regbase = (unsigned char __iomem *)mem->start;
	wdt_info.mem = mem;
	wdt_info.dev = &pdev->dev;
	wdt_info.irq = irq->start;
	init_MUTEX(&wdt_info.wdt_mutex);
	wdt_info.state = AMBA_WDT_CLOSE_STATE_DISABLE;
	wdt_info.wdt_dev.minor = WATCHDOG_MINOR,
	wdt_info.wdt_dev.name = "watchdog",
	wdt_info.wdt_dev.fops = &ambarella_wdt_fops,
	wdt_info.boot_tmo = __raw_readl(wdt_info.regbase + WDOG_TIMEOUT_OFFSET);
	platform_set_drvdata(pdev, &wdt_info);

	errorCode = ambarella_wdt_set_heartbeat(init_tmo);
	if (errorCode)
		ambarella_wdt_set_heartbeat(CONFIG_WDT_AMBARELLA_TIMEOUT);

	errorCode = misc_register(&wdt_info.wdt_dev);
	if (errorCode) {
		dev_err(&pdev->dev, "cannot register miscdev minor=%d (%d)\n",
			WATCHDOG_MINOR, errorCode);
		goto ambarella_wdt_ioarea;
	}

	ambarella_wdt_stop();

	errorCode = request_irq(wdt_info.irq, ambarella_wdt_irq,
		IRQF_TRIGGER_RISING, pdev->name, &wdt_info);
	if (errorCode) {
		dev_err(&pdev->dev, "Request IRQ failed!\n");
		goto ambarella_wdt_deregister;
	}

	dev_notice(&pdev->dev,
		"Probe Ambarella Media Processor Watch Dog Timer[%s] [%d].\n",
		wdt_info.dev->bus_id, errorCode);

	goto ambarella_wdt_na;

ambarella_wdt_deregister:
	errorCode = misc_deregister(&wdt_info.wdt_dev);

ambarella_wdt_ioarea:
	release_mem_region(mem->start, (mem->end - mem->start) + 1);

ambarella_wdt_na:
	return errorCode;
}

static int __devexit ambarella_wdt_remove(struct platform_device *pdev)
{
	struct ambarella_i2c_dev_info	*pinfo;
	int				errorCode = 0;

	pinfo = platform_get_drvdata(pdev);

	if (pinfo) {
		ambarella_wdt_stop();

		errorCode = misc_deregister(&wdt_info.wdt_dev);

		free_irq(wdt_info.irq, pinfo);

		platform_set_drvdata(pdev, NULL);

		release_mem_region(wdt_info.mem->start,
			(wdt_info.mem->end - wdt_info.mem->start) + 1);
	}

	dev_notice(&pdev->dev,
		"Remove Ambarella Media Processor Watch Dog Timer[%s] [%d].\n",
		wdt_info.dev->bus_id, errorCode);

	return errorCode;
}

static void ambarella_wdt_shutdown(struct platform_device *dev)
{
	ambarella_wdt_stop();	
}

#ifdef CONFIG_PM
static int ambarella_wdt_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int ambarella_wdt_resume(struct platform_device *dev)
{
	return 0;
}
#else
#define ambarella_wdt_suspend NULL
#define ambarella_wdt_resume  NULL
#endif /* CONFIG_PM */


static struct platform_driver ambarella_wdt_driver = {
	.probe		= ambarella_wdt_probe,
	.remove		= ambarella_wdt_remove,
	.shutdown	= ambarella_wdt_shutdown,
	.suspend	= ambarella_wdt_suspend,
	.resume		= ambarella_wdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ambarella-wdt",
	},
};

static int __init ambarella_wdt_init(void)
{
	return platform_driver_register(&ambarella_wdt_driver);
}

static void __exit ambarella_wdt_exit(void)
{
	platform_driver_unregister(&ambarella_wdt_driver);
}

module_init(ambarella_wdt_init);
module_exit(ambarella_wdt_exit);

MODULE_DESCRIPTION("Ambarella Media Processor Watch Dog Timer");
MODULE_AUTHOR("Anthony Ginger, <hfjiang@ambarella.com>");
MODULE_LICENSE("GPL");

