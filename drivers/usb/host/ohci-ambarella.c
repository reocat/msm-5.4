/*
* linux/drivers/usb/host/ohci-ambarella.c
* driver for Full speed (USB1.1) USB host controller on Ambarella processors
*
* Note:
* At present, only iONE can support USB host controller
*
* History:
*	2010/08/11 - [Cao Rongrong] created file
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

#include <linux/platform_device.h>
#include <linux/signal.h>

#include <mach/hardware.h>

#if 0
#define USB_HOST_CONFIG    (USB_MSR_BASE + USB_MSR_MCFG)
#define USB_MCFG_PFEN     (1<<31)
#define USB_MCFG_RDCOMB   (1<<30)
#define USB_MCFG_SSDEN    (1<<23)
#define USB_MCFG_OHCCLKEN (1<<16)
#ifdef CONFIG_DMA_COHERENT
#define USB_MCFG_UCAM     (1<<7)
#else
#define USB_MCFG_UCAM     (0)
#endif
#define USB_MCFG_OBMEN    (1<<1)
#define USB_MCFG_OMEMEN   (1<<0)

#define USBH_ENABLE_CE    USB_MCFG_OHCCLKEN

#define USBH_ENABLE_INIT  (USB_MCFG_PFEN  | USB_MCFG_RDCOMB 	|	\
			   USBH_ENABLE_CE | USB_MCFG_SSDEN	|	\
			   USB_MCFG_UCAM  |				\
			   USB_MCFG_OBMEN | USB_MCFG_OMEMEN)

#define USBH_DISABLE      (USB_MCFG_OBMEN | USB_MCFG_OMEMEN)
#endif

extern int usb_disabled(void);

static void ambarella_start_ohc(void)
{
#if 0
	/* enable host controller */
	au_writel(au_readl(USB_HOST_CONFIG) | USBH_ENABLE_CE, USB_HOST_CONFIG);
	au_sync();
	udelay(1000);

	au_writel(au_readl(USB_HOST_CONFIG) | USBH_ENABLE_INIT, USB_HOST_CONFIG);
	au_sync();
	udelay(2000);
#endif
}

static void ambarella_stop_ohc(void)
{
#if 0
	/* Disable mem */
	au_writel(au_readl(USB_HOST_CONFIG) & ~USBH_DISABLE, USB_HOST_CONFIG);
	au_sync();
	udelay(1000);

	/* Disable clock */
	au_writel(au_readl(USB_HOST_CONFIG) & ~USBH_ENABLE_CE, USB_HOST_CONFIG);
	au_sync();
#endif
}

static int __devinit ohci_ambarella_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	int ret;

	ohci_dbg(ohci, "ohci_ambarella_start, ohci:%p", ohci);

	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	if ((ret = ohci_run(ohci)) < 0) {
		err ("can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

static const struct hc_driver ohci_ambarella_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"Ambarella OHCI",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.start =		ohci_ambarella_start,
	.stop =			ohci_stop,
	.shutdown =		ohci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend =		ohci_bus_suspend,
	.bus_resume =		ohci_bus_resume,
#endif
	.start_port_reset =	ohci_start_port_reset,
};

static int ohci_hcd_ambarella_drv_probe(struct platform_device *pdev)
{
	int ret;
	struct usb_hcd *hcd;

	if (usb_disabled())
		return -ENODEV;

	if (pdev->resource[1].flags != IORESOURCE_IRQ) {
		pr_debug("resource[1] is not IORESOURCE_IRQ\n");
		return -ENOMEM;
	}

	hcd = usb_create_hcd(&ohci_ambarella_hc_driver, &pdev->dev, "Ambi1");
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = pdev->resource[0].start;
	hcd->rsrc_len = pdev->resource[0].end - pdev->resource[0].start + 1;
	hcd->regs = (void __iomem *)pdev->resource[0].start;

	ambarella_start_ohc();
	ohci_hcd_init(hcd_to_ohci(hcd));

	ret = usb_add_hcd(hcd, pdev->resource[1].start, IRQF_DISABLED | IRQF_TRIGGER_LOW);
	if (ret == 0) {
		platform_set_drvdata(pdev, hcd);
		return ret;
	}

	ambarella_stop_ohc();
	usb_put_hcd(hcd);
	return ret;
}

static int ohci_hcd_ambarella_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	ambarella_stop_ohc();
	usb_put_hcd(hcd);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int ohci_hcd_ambarella_drv_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	unsigned long flags;
	int rc;

	rc = 0;

	/* Root hub was already suspended. Disable irq emission and
	 * mark HW unaccessible, bail out if RH has been resumed. Use
	 * the spinlock to properly synchronize with possible pending
	 * RH suspend or resume activity.
	 *
	 * This is still racy as hcd->state is manipulated outside of
	 * any locks =P But that will be a different fix.
	 */
	spin_lock_irqsave(&ohci->lock, flags);
	if (hcd->state != HC_STATE_SUSPENDED) {
		rc = -EINVAL;
		goto bail;
	}
	ohci_writel(ohci, OHCI_INTR_MIE, &ohci->regs->intrdisable);
	(void)ohci_readl(ohci, &ohci->regs->intrdisable);

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	ambarella_stop_ohc();
bail:
	spin_unlock_irqrestore(&ohci->lock, flags);

	return rc;
}

static int ohci_hcd_ambarella_drv_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);

	ambarella_start_ohc();

	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
	ohci_finish_controller_resume(hcd);

	return 0;
}

static struct dev_pm_ops ambarella_ohci_pmops = {
	.suspend	= ohci_hcd_ambarella_drv_suspend,
	.resume		= ohci_hcd_ambarella_drv_resume,
};

#define AMBARELLA_OHCI_PMOPS &ambarella_ohci_pmops

#else
#define AMBARELLA_OHCI_PMOPS NULL
#endif

static struct platform_driver ohci_hcd_ambarella_driver = {
	.probe		= ohci_hcd_ambarella_drv_probe,
	.remove		= ohci_hcd_ambarella_drv_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver		= {
		.name	= "ambarella-ohci",
		.owner	= THIS_MODULE,
		.pm	= AMBARELLA_OHCI_PMOPS,
	},
};

MODULE_ALIAS("platform:ambarella-ohci");
