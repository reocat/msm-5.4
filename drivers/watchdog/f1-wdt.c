/*
 * Watchdog driver for F1
 *
 *  This version by Dean Matsen
 *  Copyright 2013-2018 Honeywell
 *
 *  Based substantially on imx2_wdt.c
 *  Copyright (C) 2010 Wolfram Sang, Pengutronix e.K. <w.sang@pengutronix.de>
 *
 * some parts adapted by similar drivers from Darius Augulis and Vladimir
 * Zapolskiy, additional improvements by Wim Van Sebroeck.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * NOTE: MX1 has a slightly different Watchdog from MX2 and later:
 *
 *			MX1:		MX2+:
 *			----		-----
 * Registers:		32-bit		16-bit
 * Stopable timer:	Yes		No
 * Need to enable clk:	No		Yes
 * Halt on suspend:	Manual		Can be automatic
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/f1-watchdog.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/reboot.h>

// Note that it's easiest to just replace the imx2-wdt because the clock
// is already reserved under the name "imx2-wdt.0"
#define DRIVER_NAME "imx2-wdt"

#define IMX6_WDT_WCR		0x00		/* Control Register */
#define IMX6_WDT_WCR_WT		(0xFF << 8)	/* -> Watchdog Timeout Field */
#define IMX6_WDT_WCR_WDT	(1 << 3)	/* -> WDOG timeout assert */
#define IMX6_WDT_WCR_WDE	(1 << 2)	/* -> Watchdog Enable */
#define IMX6_WDT_WCR_WDBG       (1 << 1)        /* -> 1=suspent WD during debug */
#define IMX6_WDT_WCR_WDZST	(1 << 0)	/* -> Watchdog timer Suspend */


#define IMX6_WDT_WSR		0x02		/* Service Register */
#define IMX6_WDT_SEQ1		0x5555		/* -> service sequence 1 */
#define IMX6_WDT_SEQ2		0xAAAA		/* -> service sequence 2 */

#define IMX6_WDT_WICR		0x06		/*Interrupt Control Register*/
#define IMX6_WDT_WICR_WIE	(1 << 15)	/* -> Interrupt Enable */
#define IMX6_WDT_WICR_WTIS	(1 << 14)	/* -> Interrupt Status */
#define IMX6_WDT_WICR_WICT	(0xFF << 0)	/* -> Watchdog Interrupt Timeout Field */

#define IMX6_WDT_MAX_TIME	128
#define IMX6_WDT_DEFAULT_TIME	60		/* in seconds */

#define WDOG_SEC_TO_COUNTS(s)	(((s)*2 - 1) << 8)

static struct
  {
  int clock_open;
  struct clk *clk;

  int io_mapped;
  void __iomem *base;

  unsigned hw_timeout_sec;
  unsigned sw_timeout_sec;

  unsigned sw_counter;
  int enable_timer_pet;

  int isopen;

      /* One second timer
       */
  int timer_running;
  struct timer_list timer;

  struct notifier_block restart_handler;

  } f1_wdt;


static void pet_watchdog ( void );

static struct miscdevice f1_wdt_miscdev;

DEFINE_SPINLOCK ( f1_wdt_lock );

/*--------------------------------------------------------------------------*/
/** Pet watchdog (loads WT value into counter)
 */
/*--------------------------------------------------------------------------*/

static void pet_watchdog ()
{
  __raw_writew(IMX6_WDT_SEQ1, f1_wdt.base + IMX6_WDT_WSR);
  __raw_writew(IMX6_WDT_SEQ2, f1_wdt.base + IMX6_WDT_WSR);
}

/*--------------------------------------------------------------------------*/
/* Timer routine for petting the watchdog when there is no userspace
 * process doing it
 */
/*--------------------------------------------------------------------------*/

static void f1_wdt_timer_func(unsigned long arg)
{
  unsigned long flags;

  spin_lock_irqsave ( &f1_wdt_lock, flags );

      /* Come back here every second
       */
  mod_timer ( &f1_wdt.timer, jiffies + HZ );

      /* Maybe pet the hardware watchdog
       */
  if ( f1_wdt.enable_timer_pet &&
       f1_wdt.sw_counter )
    {
//    printk ( KERN_INFO "[%u]", f1_wdt.sw_counter );
    pet_watchdog ();
    f1_wdt.sw_counter--;
    }

  spin_unlock_irqrestore ( &f1_wdt_lock, flags );
}

/*--------------------------------------------------------------------------*/
/** Can be used by early-init functions to initialize the watchdog
 * before it becomes a real device
 *
 * This function does not require the checksum to be set
 */
/*--------------------------------------------------------------------------*/

long f1_wdt_control ( struct f1wd_control *cmddata )
{
  unsigned long flags;
  u16 wcrval;
  int started = 0;

  spin_lock_irqsave ( &f1_wdt_lock, flags );

  if ( cmddata->flags & F1WDF_FORCE_RESET )
    {
    __raw_writew ( 0, f1_wdt.base + IMX6_WDT_WCR );
    while ( 1 );
    }

  if ( cmddata->flags & F1WDF_SET_HW_TIMEOUT )
    {
    f1_wdt.hw_timeout_sec = cmddata->hw_timeout_sec;
//    printk ( KERN_DEBUG "HW timeout set to %u sec\n", f1_wdt.hw_timeout_sec );
    }

  if ( cmddata->flags & F1WDF_SET_SW_TIMEOUT )
    {
    f1_wdt.sw_timeout_sec = cmddata->sw_timeout_sec;
    f1_wdt.sw_counter = f1_wdt.sw_timeout_sec;
//    printk ( KERN_DEBUG "SW timer reset to to %u sec\n", f1_wdt.sw_timeout_sec );
    }
  else if ( cmddata->flags & F1WDF_PET_SW_WDOG )
    {
//    printk ( KERN_DEBUG "SW timer reset to to %u sec\n", f1_wdt.sw_timeout_sec );
    f1_wdt.sw_counter = f1_wdt.sw_timeout_sec;
    }

  if ( cmddata->flags & F1WDF_SW_TIMER_ON )
    {
    f1_wdt.enable_timer_pet = 1;

    if ( ! f1_wdt.timer_running )
      {
          /* Start petting timer
           */
      setup_timer ( &f1_wdt.timer, f1_wdt_timer_func, 0 );
      mod_timer ( &f1_wdt.timer, jiffies );
      f1_wdt.timer_running = 1;
      }
    }

  if ( cmddata->flags & F1WDF_SW_TIMER_OFF )
    {
    f1_wdt.enable_timer_pet = 0;
    }

      /* Set the WT bits.  This value is loaded when
       * the WDT is originally run, or when it is pet
       */
  wcrval = __raw_readw(f1_wdt.base + IMX6_WDT_WCR);

  if ( ! (wcrval & IMX6_WDT_WCR_WDE) )
    {
        /* Watchdog not running yet
         */

        /* Load in the WT value before enabling it
         */
    wcrval &= ~IMX6_WDT_WCR_WT;
    wcrval |= WDOG_SEC_TO_COUNTS(f1_wdt.hw_timeout_sec);
    __raw_writew ( wcrval, f1_wdt.base + IMX6_WDT_WCR);

        /* Suspend watch dog timer in low power mode and during debug
         */
    wcrval |= (IMX6_WDT_WCR_WDZST | IMX6_WDT_WCR_WDBG | IMX6_WDT_WCR_WDE);

        /* Don't bother with WDOG line
         */
    wcrval &= ~IMX6_WDT_WCR_WDT;

        /* Start it off for the first time
         */
    __raw_writew ( wcrval, f1_wdt.base + IMX6_WDT_WCR );

    started = 1;
    }
  else
    {
    u16 oldwcrval = wcrval;

        /* The watchdog is already running -- all we can do now is
         * update the WT value
         */
    wcrval &= ~IMX6_WDT_WCR_WT;
    wcrval |= WDOG_SEC_TO_COUNTS(f1_wdt.hw_timeout_sec);

    if ( wcrval != oldwcrval )
      {
          /* Update WT value
           */
      __raw_writew ( wcrval, f1_wdt.base + IMX6_WDT_WCR);

          /* Pet the watchdog to load the new value into the counter
           */
      pet_watchdog ();
      }
    else
      {
//      printk ( KERN_DEBUG "No change to hardware\n" );
      }
    }


  spin_unlock_irqrestore ( &f1_wdt_lock, flags );

  if ( started )
    {
        /* This module is here to stay now
         */
    try_module_get ( THIS_MODULE );
    }

  return 0;
}

/*--------------------------------------------------------------------------*/
/** Induce hardware reset
 */
/*--------------------------------------------------------------------------*/

void f1_wdt_induce_hard_reset ()
{
  __raw_writew ( 0, f1_wdt.base + IMX6_WDT_WCR );
  while ( 1 );
}
EXPORT_SYMBOL_GPL ( f1_wdt_induce_hard_reset );

/*--------------------------------------------------------------------------*/
/* Filesystem open
 *
 * Only one process can open it.
 */
/*--------------------------------------------------------------------------*/

static int f1_wdt_open ( struct inode *inode, struct file *file )
{
  int rval = 0;
  unsigned long flags;

  spin_lock_irqsave ( &f1_wdt_lock, flags );

  if ( f1_wdt.isopen )
    rval = -EBUSY;
  else
    f1_wdt.isopen = 1;

  spin_unlock_irqrestore ( &f1_wdt_lock, flags );

  if ( rval )
    return rval;
  else
    return nonseekable_open(inode, file);
}

/*--------------------------------------------------------------------------*/
/* Standardized watchdog operations
 */
/*--------------------------------------------------------------------------*/

static long f1_wdt_ioctl(struct file *file, unsigned int cmd,
                          unsigned long arg)
{
  void __user *argp = (void __user *)arg;
  int __user *p = argp;

  switch (cmd)
    {
    case F1WDIOC_CONTROL :
      {
      struct f1wd_control cmddata;

      if ( copy_from_user ( &cmddata, p, sizeof(cmddata) ) )
        return -EFAULT;

          /* TODO: validate structure
           */

      return f1_wdt_control ( &cmddata );
      }
      break;

    default:
      return -ENOTTY;
    }
}

/*--------------------------------------------------------------------------*/
/* System file write to watchdog device
 */
/*--------------------------------------------------------------------------*/

static ssize_t f1_wdt_write(struct file *file, const char __user *data,
						size_t len, loff_t *ppos)
{
  return len;
}

/*--------------------------------------------------------------------------*/
/** Restart handler -- actually used by the "reboot" mechanism to reboot
 * the CPU
 */
/*--------------------------------------------------------------------------*/

static int f1_restart_handler(struct notifier_block *this,
                               unsigned long mode,
                               void *cmd)
{
      /* Just set the reset line
       */
  __raw_writew ( 0, f1_wdt.base + IMX6_WDT_WCR );

      /* wait for reset to assert... */
  mdelay(500);

      /* We shouldn't get here
       */
  return NOTIFY_DONE;
}

/*--------------------------------------------------------------------------*/
/* File close the watchdog device.
 *
 * If a capital "V" was written while open, and if the "no way out"
 * feature is not enabled, then the watchdog will be pet in the background
 * by the petting timer.  Otherwise, the watchdog will bite.
 */
/*--------------------------------------------------------------------------*/

static int f1_wdt_close(struct inode *inode, struct file *file)
{
  unsigned long flags;

  spin_lock_irqsave ( &f1_wdt_lock, flags );

  f1_wdt.isopen = 0;

  spin_unlock_irqrestore ( &f1_wdt_lock, flags );

  return 0;
}


static const struct file_operations f1_wdt_fops =
{
  .owner = THIS_MODULE,
  .llseek = no_llseek,
  .open = f1_wdt_open,
  .unlocked_ioctl = f1_wdt_ioctl,
  .write = f1_wdt_write,
  .release = f1_wdt_close,
};

static struct miscdevice f1_wdt_miscdev =
{
  .minor = MISC_DYNAMIC_MINOR,
  .name = "f1-wdt",
  .fops = &f1_wdt_fops,
};

/*--------------------------------------------------------------------------*/
/* Called when TBD
 */
/*--------------------------------------------------------------------------*/

static int __init f1_wdt_probe(struct platform_device *pdev)
{
  int ret;
  int res_size;
  struct resource *res;

  res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!res)
    {
    dev_err(&pdev->dev, "can't get device resources\n");
    ret = -ENODEV;
    goto errexit;
    }

  res_size = resource_size(res);
  if ( ! devm_request_mem_region(&pdev->dev,
                                 res->start,
                                 res_size,
                                 res->name))
    {
    dev_err(&pdev->dev, "can't allocate %d bytes at %d address\n",
            res_size, res->start);
    ret = -ENOMEM;
    goto errexit;
    }

  f1_wdt.base = devm_ioremap_nocache ( &pdev->dev, res->start, res_size);
  if (!f1_wdt.base)
    {
    dev_err(&pdev->dev, "ioremap failed\n");
    ret = -ENOMEM;
    }

  f1_wdt.clk = clk_get(&pdev->dev, NULL);
  if (IS_ERR(f1_wdt.clk))
    {
    dev_err(&pdev->dev, "can't get Watchdog clock\n");
    ret = PTR_ERR(f1_wdt.clk);
    }
  f1_wdt.clock_open = 1;
  clk_enable ( f1_wdt.clk );

  f1_wdt_miscdev.parent = &pdev->dev;
  ret = misc_register(&f1_wdt_miscdev);
  if (ret)
    goto errexit;

      /* Register restart handler
       */
  f1_wdt.restart_handler.notifier_call = f1_restart_handler;
  f1_wdt.restart_handler.priority = 128;
  ret = register_restart_handler(&f1_wdt.restart_handler);
  if (ret)
    {
    dev_err(&pdev->dev, "cannot register restart handler\n");
    goto errexit;
    }

  dev_info(&pdev->dev, "F1 Watchdog Timer enabled\n");

  return 0;

errexit :
  f1_wdt_miscdev.parent = NULL;

  if ( f1_wdt.clock_open )
    {
    clk_put ( f1_wdt.clk );
    }

  return ret;
}

/*--------------------------------------------------------------------------*/
/* Called when TBD
 */
/*--------------------------------------------------------------------------*/

static int __exit f1_wdt_remove(struct platform_device *pdev)
{
  return -EBUSY;
}

/*--------------------------------------------------------------------------*/
/* Called when TBD
 */
/*--------------------------------------------------------------------------*/

static void f1_wdt_shutdown(struct platform_device *pdev)
{
}

static const struct of_device_id f1_wdt_dt_ids[] = {
	{ .compatible = "f1-mx6sx-wdt", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, f1_wdt_dt_ids);

static struct platform_driver f1_wdt_driver =
{
  .remove	= __exit_p(f1_wdt_remove),
  .shutdown	= f1_wdt_shutdown,
  .driver	=
      {
        .name	= DRIVER_NAME,
	.of_match_table = f1_wdt_dt_ids,
      },
};

/*--------------------------------------------------------------------------*/
/* Called during init or module load
 */
/*--------------------------------------------------------------------------*/

static int __init f1_wdt_init(void)
{
  printk ( KERN_INFO "F1 Watchdog driver\n");

  f1_wdt.hw_timeout_sec = 60;
  f1_wdt.sw_timeout_sec = 60;
  return platform_driver_probe ( &f1_wdt_driver, f1_wdt_probe );
}
module_init(f1_wdt_init);

/*--------------------------------------------------------------------------*/
/* Called during shutdown or module unload
 */
/*--------------------------------------------------------------------------*/

static void __exit f1_wdt_exit(void)
{
  platform_driver_unregister ( &f1_wdt_driver );
}
module_exit(f1_wdt_exit);


MODULE_AUTHOR("Dean Matsen,Wolfram Sang");
MODULE_DESCRIPTION("Watchdog driver for F1");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
