/*
* linux/drivers/usb/host/ehci-ambarella.c
* driver for High speed (USB2.0) USB host controller on Ambarella processors
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
#include <mach/hardware.h>
#include <plat/uhc.h>

extern int usb_disabled(void);

static void ambarella_start_ehc(struct platform_device *pdev)
{
	struct ambarella_uhc_controller *plat_ehci;
	printk("%s: %d\n", __func__, __LINE__);

	if (pdev == NULL)
		return;

	/* enable clock to EHCI block and HS PHY PLL*/
	plat_ehci = (struct ambarella_uhc_controller *)pdev->dev.platform_data;
	if (plat_ehci && plat_ehci->dedicated_io)
		plat_ehci->dedicated_io();
	if (plat_ehci && plat_ehci->enable_host)
		plat_ehci->enable_host();
}

static void ambarella_stop_ehc(struct platform_device *pdev)
{
	struct ambarella_uhc_controller *plat_ehci;
	printk("%s: %d\n", __func__, __LINE__);

	if (pdev == NULL)
		return;

	/* enable clock to EHCI block and HS PHY PLL*/
	plat_ehci = (struct ambarella_uhc_controller *)pdev->dev.platform_data;
	if (plat_ehci && plat_ehci->disable_host)
		plat_ehci->disable_host();

}

static int ambarella_ehci_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int retval = 0;

	/* registers start at offset 0x0 */
	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs +
		HC_LENGTH(ehci_readl(ehci, &ehci->caps->hc_capbase));
	dbg_hcs_params(ehci, "reset");
	dbg_hcc_params(ehci, "reset");

	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);
	printk("%s: hcs_params(0x%08x) = 0x%08x, hcc_params(0x%08x) = 0x%08x\n", __func__,
		(u32)&ehci->caps->hcs_params, ehci_readl(ehci, &ehci->caps->hcs_params),
		(u32)&ehci->caps->hcc_params, ehci_readl(ehci, &ehci->caps->hcc_params));

	retval = ehci_halt(ehci);
	if (retval)
		return retval;

	/* data structure init */
	retval = ehci_init(hcd);
	if (retval)
		return retval;

	ehci->sbrn = 0x20;

	ehci_reset(ehci);
	ehci_port_power(ehci, 0);

	printk("%s: CF = 0x%08x, EPP0 = 0x%08x, EPP1 = 0x%08x\n", __func__,
		ehci_readl(ehci, &ehci->regs->configured_flag),
		ehci_readl(ehci, &ehci->regs->port_status[0]),
		ehci_readl(ehci, &ehci->regs->port_status[1]));

	printk("%s: GPIO0_AFSEL = 0x%08x, USBP0_CTRL_REG = 0x%08x,"
		"USBP1_CTRL_REG = 0x%08x, ANA_PWR_REG = 0x%08x\n", __func__,
		amba_readl(GPIO0_AFSEL_REG), amba_readl(USBP0_CTRL_REG),
		amba_readl(USBP1_CTRL_REG), amba_readl(ANA_PWR_REG));

	return retval;
}

static const struct hc_driver ehci_ambarella_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "Ambarella EHCI",
	.hcd_priv_size		= sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq			= ehci_irq,
	.flags			= HCD_MEMORY | HCD_USB2,

	/*
	 * basic lifecycle operations
	 *
	 * FIXME -- ehci_init() doesn't do enough here.
	 * See ehci-ppc-soc for a complete implementation.
	 */
	.reset			= ambarella_ehci_setup,
	.start			= ehci_run,
	.stop			= ehci_stop,
	.shutdown		= ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ehci_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,
	.endpoint_reset		= ehci_endpoint_reset,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ehci_hub_status_data,
	.hub_control		= ehci_hub_control,
	.bus_suspend		= ehci_bus_suspend,
	.bus_resume		= ehci_bus_resume,
	.relinquish_port	= ehci_relinquish_port,
	.port_handed_over	= ehci_port_handed_over,

	.clear_tt_buffer_complete	= ehci_clear_tt_buffer_complete,
};

static int ehci_hcd_ambarella_drv_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	if (pdev->resource[1].flags != IORESOURCE_IRQ) {
		pr_debug("resource[1] is not IORESOURCE_IRQ");
		return -ENOMEM;
	}
	hcd = usb_create_hcd(&ehci_ambarella_hc_driver, &pdev->dev, "Ambi1");
	if (!hcd)
		return -ENOMEM;

	hcd->rsrc_start = pdev->resource[0].start;
	hcd->rsrc_len = pdev->resource[0].end - pdev->resource[0].start + 1;
	hcd->regs = (void __iomem *)pdev->resource[0].start;

	ambarella_start_ehc(pdev);

	ret = usb_add_hcd(hcd, pdev->resource[1].start, IRQF_DISABLED | IRQF_TRIGGER_RISING);
	if (ret == 0) {
		platform_set_drvdata(pdev, hcd);
		return ret;
	}

	ambarella_stop_ehc(pdev);
	usb_put_hcd(hcd);

	return ret;
}

static int ehci_hcd_ambarella_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
	ambarella_stop_ehc(pdev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int ehci_hcd_ambarella_drv_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	unsigned long flags;
	int rc;

	printk("%s: %d\n", __func__, __LINE__);

	return 0;
	rc = 0;

	if (time_before(jiffies, ehci->next_statechange))
		msleep(10);

	/* Root hub was already suspended. Disable irq emission and
	 * mark HW unaccessible, bail out if RH has been resumed. Use
	 * the spinlock to properly synchronize with possible pending
	 * RH suspend or resume activity.
	 *
	 * This is still racy as hcd->state is manipulated outside of
	 * any locks =P But that will be a different fix.
	 */
	spin_lock_irqsave(&ehci->lock, flags);
	if (hcd->state != HC_STATE_SUSPENDED) {
		rc = -EINVAL;
		goto bail;
	}
	ehci_writel(ehci, 0, &ehci->regs->intr_enable);
	(void)ehci_readl(ehci, &ehci->regs->intr_enable);

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	ambarella_stop_ehc(NULL);

bail:
	spin_unlock_irqrestore(&ehci->lock, flags);

	// could save FLADJ in case of Vaux power loss
	// ... we'd only use it to handle clock skew

	return rc;
}

static int ehci_hcd_ambarella_drv_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);

	printk("%s: %d\n", __func__, __LINE__);

	ambarella_start_ehc(NULL);

	// maybe restore FLADJ

	if (time_before(jiffies, ehci->next_statechange))
		msleep(100);

	/* Mark hardware accessible again as we are out of D3 state by now */
	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	/* If CF is still set, we maintained PCI Vaux power.
	 * Just undo the effect of ehci_pci_suspend().
	 */
	if (ehci_readl(ehci, &ehci->regs->configured_flag) == FLAG_CF) {
		int	mask = INTR_MASK;

		if (!hcd->self.root_hub->do_remote_wakeup)
			mask &= ~STS_PCD;
		ehci_writel(ehci, mask, &ehci->regs->intr_enable);
		ehci_readl(ehci, &ehci->regs->intr_enable);
		return 0;
	}

	ehci_dbg(ehci, "lost power, restarting\n");
	usb_root_hub_lost_power(hcd->self.root_hub);

	/* Else reset, to cope with power loss or flush-to-storage
	 * style "resume" having let BIOS kick in during reboot.
	 */
	(void) ehci_halt(ehci);
	(void) ehci_reset(ehci);

	/* emptying the schedule aborts any urbs */
	spin_lock_irq(&ehci->lock);
	if (ehci->reclaim)
		end_unlink_async(ehci);
	ehci_work(ehci);
	spin_unlock_irq(&ehci->lock);

	ehci_writel(ehci, ehci->command, &ehci->regs->command);
	ehci_writel(ehci, FLAG_CF, &ehci->regs->configured_flag);
	ehci_readl(ehci, &ehci->regs->command);	/* unblock posted writes */

	/* here we "know" root ports should always stay powered */
	ehci_port_power(ehci, 1);

	hcd->state = HC_STATE_SUSPENDED;

	return 0;
}

static struct dev_pm_ops ambarella_ehci_pmops = {
	.suspend	= ehci_hcd_ambarella_drv_suspend,
	.resume		= ehci_hcd_ambarella_drv_resume,
};

#define AMBARELLA_EHCI_PMOPS &ambarella_ehci_pmops

#else
#define AMBARELLA_EHCI_PMOPS NULL
#endif

static struct platform_driver ehci_hcd_ambarella_driver = {
	.probe		= ehci_hcd_ambarella_drv_probe,
	.remove		= ehci_hcd_ambarella_drv_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver = {
		.name	= "ambarella-ehci",
		.owner	= THIS_MODULE,
		.pm	= AMBARELLA_EHCI_PMOPS,
	}
};

MODULE_ALIAS("platform:ambarella-ehci");
