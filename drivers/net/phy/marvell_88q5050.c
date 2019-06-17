/*
 * drivers/net/phy/marvell_88q5050.c
 *
 * Driver for Marvell 88Q5050 PHYs
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/marvell_phy.h>
#include <linux/of.h>

#include <linux/io.h>
#include <asm/irq.h>
#include <linux/uaccess.h>

#define	RGMII_RX_TIMING		(1 << 15)
#define	RGMII_TX_TIMING		(1 << 14)

#define MARVELL_PHY_CTRL_REG	0x1

static int m88q5050_probe(struct phy_device *phydev)
{
	return 0;
}

static int m88q5050_config_init(struct phy_device *phydev)
{
	unsigned int value;

	value = phy_read(phydev, MARVELL_PHY_CTRL_REG);
	value |= RGMII_RX_TIMING;
	value |= RGMII_TX_TIMING;

	phy_write(phydev, MARVELL_PHY_CTRL_REG, value);

	return 0;
}

static int m88q5050_config_aneg(struct phy_device *phydev)
{
	return 0;
}

static int m88q5050_read_status(struct phy_device *phydev)
{
	phydev->duplex = DUPLEX_FULL;
	phydev->speed = SPEED_1000;
	phydev->pause = 0;
	phydev->asym_pause = 0;
	phydev->link = 1;

	return 0;
}
static int m88q5050_ack_interrupt(struct phy_device *phydev)
{
	return 0;
}
static int m88q5050_config_intr(struct phy_device *phydev)
{
	return 0;
}

static int m88q5050_did_interrupt(struct phy_device *phydev)
{
	return 0;
}

static int m88q5050_aneg_done(struct phy_device *phydev)
{
	return 1;
}
static struct phy_driver marvell_88q5050_drivers[] = {
	{
		.phy_id = 0xff00a510,
		.phy_id_mask = MARVELL_PHY_ID_MASK,
		.name = "Marvell 88Q5050",
		.features = PHY_GBIT_FEATURES,
		.flags = PHY_HAS_INTERRUPT,
		.probe = m88q5050_probe,
		.config_init = &m88q5050_config_init,
		.config_aneg = &m88q5050_config_aneg,
		.read_status = &m88q5050_read_status,
		.ack_interrupt = &m88q5050_ack_interrupt,
		.config_intr = &m88q5050_config_intr,
		.did_interrupt = &m88q5050_did_interrupt,
		.aneg_done = &m88q5050_aneg_done,
	},
};

module_phy_driver(marvell_88q5050_drivers);

static struct mdio_device_id __maybe_unused marvell_88q5050_tbl[] = {
	{ 0xff00a510, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, marvell_88q5050_tbl);
