/*
 * Driver for Supercam PHY
 *
 * Author: Jon <tdlin@ambarella.com>
 *
 * Copyright (c) 2016 Ambarella
 *
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/phy.h>

#define PHY_ID_VSC8531		0x00070570

#define SUPERCAM_REG_SEL		31      /* Register Set Select Register */
#define SUPERCAM_EXTEND_2		0x0002  /* Extend 2 Registers */
#define SUPERCAM_RGMII_CTRL		20	    /* RGMII Control Register */

MODULE_DESCRIPTION("Supercam PHY driver");
MODULE_AUTHOR("Jon <tdlin@ambarella.com>");
MODULE_LICENSE("GPL");

static int supercam_config_init(struct phy_device *phydev)
{   
    unsigned short phy_reg = 0;

    printk("[PHY]Supercam PHY patch\n");
	phy_write(phydev, SUPERCAM_REG_SEL, SUPERCAM_EXTEND_2);
	phy_reg = phy_read(phydev, 0x14);
    printk("[PHY]RGMII control:0x%08x\n", phy_reg);
	phy_write(phydev, SUPERCAM_RGMII_CTRL, 0x0040);
	phy_reg = phy_read(phydev, 0x14);
    printk("[PHY]RGMII control:0x%08x\n", phy_reg);
	phy_write(phydev, SUPERCAM_REG_SEL, 0x0);

	return 0;
}

static struct phy_driver supercam_driver[] = { {
	.phy_id		= PHY_ID_VSC8531,
	.name		= "VSC8531",
	.phy_id_mask	= 0xfffffff0,
	.features	= PHY_BASIC_FEATURES,
	.flags		= 0,
	.config_init	= supercam_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.driver		= { .owner = THIS_MODULE,},
} };

module_phy_driver(supercam_driver);

static struct mdio_device_id __maybe_unused supercam_tbl[] = {
	{ PHY_ID_VSC8531, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, supercam_tbl);
