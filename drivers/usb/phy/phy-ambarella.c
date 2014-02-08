/*
* linux/drivers/usb/phy/phy-ambarella.c
*
* History:
*	2014/01/28 - [Cao Rongrong] created file
*
* Copyright (C) 2012 by Ambarella, Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/usb/otg.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>
#include <plat/uhc.h>

#define DRIVER_NAME "ambarella_phy"

struct ambarella_phy {
	struct usb_phy phy;
	void __iomem *pol_reg;
	void __iomem *ana_reg;
	u32 use_ocp;
	struct pinctrl *pinctrl;
};

#define to_ambarella_phy(p) container_of((p), struct ambarella_phy, phy)

static int ambarella_phy_init(struct usb_phy *phy)
{
	struct ambarella_phy *amb_phy = to_ambarella_phy(phy);

	/* If there are 2 PHYs, no matter which PHY need to be initialized,
	 * we initialize all of them at the same time */
	if (!(amba_readl(amb_phy->ana_reg) & ANA_PWR_USB_PHY_ENABLE)) {
		amba_setbitsl(amb_phy->ana_reg, ANA_PWR_USB_PHY_ENABLE);
		mdelay(1);
	}

	return 0;
}

static int ambarella_otg_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct ambarella_phy *amb_phy;

	if (!otg)
		return -ENODEV;

	amb_phy = to_ambarella_phy(otg->phy);
	amb_phy->phy.otg->host = host;

	return 0;
}

static int ambarella_otg_set_peripheral(struct usb_otg *otg,
					struct usb_gadget *gadget)
{
	struct ambarella_phy *amb_phy;

	if (!otg)
		return -ENODEV;

	amb_phy = to_ambarella_phy(otg->phy);
	amb_phy->phy.otg->gadget = gadget;

	return 0;
}

static int ambarella_phy_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ambarella_phy *amb_phy;
	struct resource *mem;
	int ocp, rval;

	amb_phy = devm_kzalloc(&pdev->dev, sizeof(*amb_phy), GFP_KERNEL);
	if (!amb_phy) {
		dev_err(&pdev->dev, "Failed to allocate memory!\n");
		return -ENOMEM;
	}

	amb_phy->phy.otg = devm_kzalloc(&pdev->dev, sizeof(struct usb_otg),
					GFP_KERNEL);
	if (!amb_phy->phy.otg) {
		dev_err(&pdev->dev, "Failed to allocate memory!\n");
		return -ENOMEM;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No mem resource for pol_reg!\n");
		return -ENXIO;
	}

	amb_phy->pol_reg = devm_ioremap(&pdev->dev,
					mem->start, resource_size(mem));
	if (!amb_phy->pol_reg) {
		dev_err(&pdev->dev, "devm_ioremap() failed\n");
		return -ENOMEM;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!mem) {
		dev_err(&pdev->dev, "No mem resource for ana_reg!\n");
		return -ENXIO;
	}

	amb_phy->ana_reg = devm_ioremap(&pdev->dev,
					mem->start, resource_size(mem));
	if (!amb_phy->ana_reg) {
		dev_err(&pdev->dev, "devm_ioremap() failed\n");
		return -ENOMEM;
	}

	rval = of_property_read_u32(np, "amb,ocp-polarity", &ocp);
	if (rval < 0)
		amb_phy->use_ocp = 0;
	else
		amb_phy->use_ocp = 1;

	if (amb_phy->use_ocp) {
		amba_clrbitsl(amb_phy->pol_reg, 0x1 << 13);
		amba_setbitsl(amb_phy->pol_reg, ocp << 13);
	}

	amb_phy->pinctrl = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(amb_phy->pinctrl)) {
		dev_err(&pdev->dev, "Failed to request pinctrl\n");
		return PTR_ERR(amb_phy->pinctrl);
	}

	amb_phy->phy.dev = &pdev->dev;
	amb_phy->phy.label = DRIVER_NAME;
	amb_phy->phy.init = ambarella_phy_init;

	amb_phy->phy.otg->phy = &amb_phy->phy;
	amb_phy->phy.otg->set_host = ambarella_otg_set_host;
	amb_phy->phy.otg->set_peripheral = ambarella_otg_set_peripheral;

	platform_set_drvdata(pdev, &amb_phy->phy);

	return usb_add_phy_dev(&amb_phy->phy);
}

static int ambarella_phy_remove(struct platform_device *pdev)
{
	struct ambarella_phy *amb_phy = platform_get_drvdata(pdev);

	usb_remove_phy(&amb_phy->phy);

	return 0;
}

static const struct of_device_id ambarella_phy_dt_ids[] = {
	{ .compatible = "ambarella,usbphy", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ambarella_phy_dt_ids);

static struct platform_driver ambarella_phy_driver = {
	.probe = ambarella_phy_probe,
	.remove = ambarella_phy_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = ambarella_phy_dt_ids,
	 },
};

static int __init ambarella_phy_module_init(void)
{
	return platform_driver_register(&ambarella_phy_driver);
}
postcore_initcall(ambarella_phy_module_init);

static void __exit ambarella_phy_module_exit(void)
{
	platform_driver_unregister(&ambarella_phy_driver);
}
module_exit(ambarella_phy_module_exit);


MODULE_AUTHOR("Cao Rongrong <rrcao@ambarella.com>");
MODULE_DESCRIPTION("Ambarella USB PHY driver");
MODULE_LICENSE("GPL");

