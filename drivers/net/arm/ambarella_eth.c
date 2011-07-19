/*
 * drivers/net/ambarella_eth.c
 *
 * History:
 *	2007/10/30 - [Grady Chen] created file
 *	2007/12/29 - [Jay Zhang] fixed a few bugs
 *	2008/04/14 - [Louis Sun] changed it to transmit multi packets
 *	    in one irq also removed memcpy in transmit
 *	2008/05/13 - [Louis Sun] port NAPI for receive
 *	2008/06/11 - [Louis Sun] add multicast receive with DA filtering,
 *	    add ethernet stop implementation
 *	2008/10/23 - [Louis Sun] added Realtek 8201 PHY driver to support
 *	    detection link mode and speed enable MAC config in linux.
 *	2009/01/04 - [Anthony Ginger] Port to 2.6.28
 *
 * Copyright (C) 2004-2009, Ambarella, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/time.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/ethtool.h>

#include <mach/hardware.h>
#include <plat/eth.h>

#define AMBETH_TX_TIMEOUT	(2 * HZ)
#define AMBETH_TX_RING_SIZE	(32)
#define AMBETH_RX_RING_SIZE	(64)
#define AMBETH_PACKET_MAXFRAME	(1536)
#define AMBETH_RX_COPYBREAK	(1518)
#define AMBETH_MULTICAST_LIMIT	(1000)
#define AMBETH_MULTICAST_PF	(15)

#define AMBETH_MAC_FLOW_CTR_FDX	(ETH_MAC_FLOW_CTR_RFE | ETH_MAC_FLOW_CTR_TFE)
#define AMBETH_DMA_BUS_MODE	(ETH_DMA_BUS_MODE_FB | \
				ETH_DMA_BUS_MODE_PBL_32 | \
				ETH_DMA_BUS_MODE_DA_RX)
#define AMBETH_DMA_INT_RXPOLL	(ETH_DMA_STATUS_RI | ETH_DMA_STATUS_RU | \
				ETH_DMA_STATUS_RPS | ETH_DMA_STATUS_RWT | \
				ETH_DMA_STATUS_OVF | ETH_DMA_STATUS_ERI)
#define AMBETH_DMA_INT_ENABLE	(ETH_DMA_STATUS_NIS | ETH_DMA_STATUS_AIS | \
				ETH_DMA_STATUS_FBI | ETH_DMA_STATUS_RWT | \
				ETH_DMA_STATUS_RU | ETH_DMA_STATUS_RI | \
				ETH_DMA_STATUS_UNF | ETH_DMA_STATUS_OVF | \
				ETH_DMA_STATUS_TJT | ETH_DMA_STATUS_TI | \
				ETH_DMA_STATUS_ERI)

#define AMBETH_MII_RETRY_LIMIT	(200)
#define AMBETH_MII_RETRY_TMO	(10)

struct ambeth_desc {
	u32					status;
	u32					length;
	u32					buffer1;
	u32					buffer2;
} __attribute((packed));

struct ambeth_buffmng_info {
	struct sk_buff				*skb;
	dma_addr_t				mapping;
};

struct ambeth_tx_rngmng {
	unsigned int				cur_tx;
	unsigned int				dirty_tx;
	struct ambeth_buffmng_info		rng[AMBETH_TX_RING_SIZE];
	struct ambeth_desc			*desc;
};

struct ambeth_rx_rngmng {
	unsigned int				cur_rx;
	unsigned int				dirty_rx;
	struct ambeth_buffmng_info		rng[AMBETH_RX_RING_SIZE];
	struct ambeth_desc			*desc;
};

struct ambeth_info {
	struct ambeth_rx_rngmng			rx;
	struct ambeth_tx_rngmng			tx;
	dma_addr_t				rx_dma_desc;
	dma_addr_t				tx_dma_desc;
	spinlock_t				lock;
	int					oldspeed;
	int					oldduplex;
	int					oldlink;

	struct net_device_stats			stats;
	struct timer_list			oom_timer;
	struct napi_struct			napi;
	struct net_device			*ndev;
	struct mii_bus				new_bus;
	struct phy_device			*phydev;
	uint32_t				msg_enable;

	unsigned char __iomem			*regbase;
	struct ambarella_eth_platform_info	*platform_info;
	unsigned int				mc_filter[2];
};

static int msg_level = -1;
module_param (msg_level, int, 0);
MODULE_PARM_DESC (msg_level, "Override default message level");

/* ==========================================================================*/
static inline int ambhw_dma_reset(struct ambeth_info *lp)
{
	int					errorCode = 0;
	u32					counter = 0;

	amba_setbitsl(lp->regbase + ETH_DMA_BUS_MODE_OFFSET,
		ETH_DMA_BUS_MODE_SWR);
	do {
		if (counter++ > 100) {
			errorCode = -EIO;
			break;
		}
		msleep(1);
	} while (amba_tstbitsl(lp->regbase + ETH_DMA_BUS_MODE_OFFSET,
		ETH_DMA_BUS_MODE_SWR));

	return errorCode;
}

static inline void ambhw_dma_int_enable(struct ambeth_info *lp)
{
	amba_writel(lp->regbase + ETH_DMA_INTEN_OFFSET, AMBETH_DMA_INT_ENABLE);
}

static inline void ambhw_dma_int_disable(struct ambeth_info *lp)
{
	amba_writel(lp->regbase + ETH_DMA_INTEN_OFFSET, 0);
}

static inline void ambhw_dma_rx_start(struct ambeth_info *lp)
{
	amba_test_and_set_mask(lp->regbase + ETH_DMA_OPMODE_OFFSET,
		ETH_DMA_OPMODE_SR);
}

static inline void ambhw_dma_tx_start(struct ambeth_info *lp)
{
	amba_test_and_set_mask(lp->regbase + ETH_DMA_OPMODE_OFFSET,
		ETH_DMA_OPMODE_ST);
}

static inline void ambhw_dma_stop_rxtx(struct ambeth_info *lp)
{
	unsigned int				irq_status;
	/* wait until in-flight frame completes.
	* Max time @ 10BT: 1500*8b/10Mbps == 1200us (+ 100us margin)
	* Typically expect this loop to end in < 50 us on 100BT.
	*/
	int					i = 1300 / 10;

	amba_clrbitsl(lp->regbase + ETH_DMA_OPMODE_OFFSET,
		(ETH_DMA_OPMODE_ST | ETH_DMA_OPMODE_SR));
	/* is Transmit or Receive is still On */
	/* TS bit 22:20, RS bit 19:17, so TS | RS is 22:17 */
	do {
		udelay(10);
		irq_status = amba_readl(lp->regbase + ETH_DMA_STATUS_OFFSET);
	} while (((irq_status >> 17) & 0x3F) && --i);

	if ((i <= 0) && netif_msg_hw(lp)) {
		dev_err(&lp->ndev->dev, "%s: status=0x%x, opmode=0x%x fail.\n",
			__func__,
			amba_readl(lp->regbase + ETH_DMA_STATUS_OFFSET),
			amba_readl(lp->regbase + ETH_DMA_OPMODE_OFFSET));
	}
}

static inline void ambhw_set_dma_desc(struct ambeth_info *lp)
{
	amba_writel(lp->regbase + ETH_DMA_RX_DESC_LIST_OFFSET,
		lp->rx_dma_desc);
	amba_writel(lp->regbase + ETH_DMA_TX_DESC_LIST_OFFSET,
		lp->tx_dma_desc);
}

static inline phy_interface_t ambhw_get_interface(struct ambeth_info *lp)
{
	return amba_tstbitsl(lp->regbase + ETH_MAC_CFG_OFFSET,
		ETH_MAC_CFG_PS) ? PHY_INTERFACE_MODE_MII :
		PHY_INTERFACE_MODE_GMII;
}

static inline void ambhw_set_hwaddr(struct ambeth_info *lp, u8 *hwaddr)
{
	u32					val;

	val = (hwaddr[5] << 8) | hwaddr[4];
	amba_writel(lp->regbase + ETH_MAC_MAC0_HI_OFFSET, val);
	udelay(4);

	val = (hwaddr[3] << 24) | (hwaddr[2] << 16) |
		(hwaddr[1] <<  8) |  hwaddr[0];
	amba_writel(lp->regbase + ETH_MAC_MAC0_LO_OFFSET, val);
}

static inline void ambhw_get_hwaddr(struct ambeth_info *lp, u8 *hwaddr)
{
	u32					hval;
	u32					lval;

	hval = amba_readl(lp->regbase + ETH_MAC_MAC0_HI_OFFSET);
	lval = amba_readl(lp->regbase + ETH_MAC_MAC0_LO_OFFSET);

	hwaddr[5] = ((hval >> 8) & 0xff);
	hwaddr[4] = ((hval >> 0) & 0xff);
	hwaddr[3] = ((lval >> 24) & 0xff);
	hwaddr[2] = ((lval >> 16) & 0xff);
	hwaddr[1] = ((lval >> 8) & 0xff);
	hwaddr[0] = ((lval >> 0) & 0xff);
}

static inline int ambhw_set_hwaddr_perfect_filtering(struct ambeth_info *lp,
	u8 *hwaddr, u8 index, int enable)
{
	int					errorCode = 0;
	u32 val;

	if (unlikely((index == 0) || (index > AMBETH_MULTICAST_PF))) {
		dev_err(&lp->ndev->dev,
			"%s: wrong index[%d]!\n", __func__, index);
		errorCode = -EPERM;
		goto ambhw_set_hwaddr_perfect_filtering_exit;
	}

	val = (hwaddr[5] << 8) | hwaddr[4] | (enable << 31);
	amba_writel(lp->regbase + ETH_MAC_MAC0_HI_OFFSET + (index << 3), val);
	udelay(4);
	val = (hwaddr[3] << 24) | (hwaddr[2] << 16) |
		(hwaddr[1] <<  8) |  hwaddr[0];
	amba_writel(lp->regbase + ETH_MAC_MAC0_LO_OFFSET + (index << 3), val);

ambhw_set_hwaddr_perfect_filtering_exit:
	return errorCode;
}

static inline void ambhw_set_link_mode_speed(struct ambeth_info *lp)
{
	u32					val;

	val = amba_readl(lp->regbase + ETH_MAC_CFG_OFFSET);

	switch (lp->oldspeed) {
	case SPEED_1000:
		val &= ~(ETH_MAC_CFG_PS);
		if (lp->platform_info->is_supportclk()) {
			lp->platform_info->setclk(
				lp->platform_info->default_1000_clk);
			if (netif_msg_hw(lp) &&
				(lp->platform_info->default_1000_clk !=
				lp->platform_info->getclk())) {
				dev_info(&lp->ndev->dev,
				"%s: want [%d], but get [%d]!\n", __func__,
				lp->platform_info->default_1000_clk,
				lp->platform_info->getclk());
			}
		}
		break;
	case SPEED_100:
		val |= ETH_MAC_CFG_PS;
		if (lp->platform_info->is_supportclk()) {
			lp->platform_info->setclk(
				lp->platform_info->default_100_clk);
			if (netif_msg_hw(lp) &&
				(lp->platform_info->default_100_clk !=
				lp->platform_info->getclk())) {
				dev_info(&lp->ndev->dev,
				"%s: want [%d], but get [%d]!\n", __func__,
				lp->platform_info->default_100_clk,
				lp->platform_info->getclk());
			}
		}
		break;
	case SPEED_10:
	default:
		val |= ETH_MAC_CFG_PS;
		if (lp->platform_info->is_supportclk()) {
			lp->platform_info->setclk(
				lp->platform_info->default_10_clk);
			if (netif_msg_hw(lp) &&
				(lp->platform_info->default_10_clk !=
				lp->platform_info->getclk())) {
				dev_info(&lp->ndev->dev,
				"%s: want [%d], but get [%d]!\n", __func__,
				lp->platform_info->default_10_clk,
				lp->platform_info->getclk());
			}
		}
		break;
	}

	val |= (ETH_MAC_CFG_TE | ETH_MAC_CFG_RE);

	if (lp->oldduplex) {
		val &= ~(ETH_MAC_CFG_DO);
		val |= ETH_MAC_CFG_DM;
	} else {
		val &= ~(ETH_MAC_CFG_DM);
		val |= ETH_MAC_CFG_DO;
	}

	amba_writel(lp->regbase + ETH_MAC_CFG_OFFSET, val);
}

static inline void ambhw_init(struct ambeth_info *lp)
{
	u32					val;

	val = AMBETH_DMA_BUS_MODE;
	amba_writel(lp->regbase + ETH_DMA_BUS_MODE_OFFSET, val);

	val = 0;
	amba_writel(lp->regbase + ETH_MAC_FRAME_FILTER_OFFSET, val);

	/* @@ why   RTC 64 , can we try RTC 96 or 128 ?  CHECKPOINT,
	why ETH_DMA_OPMODE_FUF?  may try to disable FUF
	we may also try ETH_DMA_OPMODE_SF, since it starts
	transfer when there is a full frame,
	*/
	val = (ETH_DMA_OPMODE_TTC_256 |
		ETH_DMA_OPMODE_RTC_96 |
		ETH_DMA_OPMODE_FUF);
	amba_writel(lp->regbase + ETH_DMA_OPMODE_OFFSET, val);

	val = AMBETH_MAC_FLOW_CTR_FDX;
	amba_writel(lp->regbase + ETH_MAC_FLOW_CTR_OFFSET, val);

	val = amba_readl(lp->regbase + ETH_DMA_STATUS_OFFSET);
	amba_writel(lp->regbase + ETH_DMA_STATUS_OFFSET, val);

	/* Set RX poll demand register */
	val = 0x1;
	amba_writel(lp->regbase + ETH_DMA_RX_POLL_DMD_OFFSET, val);
}

/* ==========================================================================*/
static int ambhw_mdio_read(struct mii_bus *bus,
	int mii_id, int regnum)
{
	struct ambeth_info			*lp;
	int					val;
	int					limit;

	lp = (struct ambeth_info *)bus->priv;

	for (limit = AMBETH_MII_RETRY_LIMIT; limit > 0; limit--) {
		if (!amba_tstbitsl(lp->regbase + ETH_MAC_GMII_ADDR_OFFSET,
			ETH_MAC_GMII_ADDR_GB))
			break;
		udelay(AMBETH_MII_RETRY_TMO);
	}
	if ((limit <= 0) && netif_msg_hw(lp)) {
		dev_err(&lp->ndev->dev, "%s: preread time out!\n", __func__);
		val = 0xFFFFFFFF;
		goto ambhw_mdio_read_exit;
	}

	val = ETH_MAC_GMII_ADDR_PA(mii_id) | ETH_MAC_GMII_ADDR_GR(regnum);
	val |= ETH_MAC_GMII_ADDR_CR_250_300MHZ | ETH_MAC_GMII_ADDR_GB;
	amba_writel(lp->regbase + ETH_MAC_GMII_ADDR_OFFSET, val);

	for (limit = AMBETH_MII_RETRY_LIMIT; limit > 0; limit--) {
		if (!amba_tstbitsl(lp->regbase + ETH_MAC_GMII_ADDR_OFFSET,
			ETH_MAC_GMII_ADDR_GB))
			break;
		udelay(AMBETH_MII_RETRY_TMO);
	}
	if ((limit <= 0) && netif_msg_hw(lp)) {
		dev_err(&lp->ndev->dev, "%s: postread time out!\n", __func__);
		val = 0xFFFFFFFF;
		goto ambhw_mdio_read_exit;
	}

	val = amba_readl(lp->regbase + ETH_MAC_GMII_DATA_OFFSET);

ambhw_mdio_read_exit:
	if (netif_msg_hw(lp))
		dev_info(&lp->ndev->dev,
			"%s: mii_id[0x%02x], regnum[0x%02x], val[0x%04x]!\n",
			__func__, mii_id, regnum, val);

	return val;
}

static int ambhw_mdio_write(struct mii_bus *bus,
	int mii_id, int regnum, u16 value)
{
	int					errorCode = 0;
	struct ambeth_info			*lp;
	int					val;
	int					limit = 0;

	lp = (struct ambeth_info *)bus->priv;

	if (netif_msg_hw(lp))
		dev_info(&lp->ndev->dev,
			"%s: mii_id[0x%02x], regnum[0x%02x], value[0x%04x]!\n",
			__func__, mii_id, regnum, value);

	for (limit = AMBETH_MII_RETRY_LIMIT; limit > 0; limit--) {
		if (!amba_tstbitsl(lp->regbase + ETH_MAC_GMII_ADDR_OFFSET,
			ETH_MAC_GMII_ADDR_GB))
			break;
		udelay(AMBETH_MII_RETRY_TMO);
	}
	if ((limit <= 0) && netif_msg_hw(lp)) {
		dev_err(&lp->ndev->dev, "%s: prewrite time out!\n", __func__);
		errorCode = -EIO;
		goto ambhw_mdio_write_exit;
	}

	val = value;
	amba_writel(lp->regbase + ETH_MAC_GMII_DATA_OFFSET, val);
	val = ETH_MAC_GMII_ADDR_PA(mii_id) | ETH_MAC_GMII_ADDR_GR(regnum);
	val |= ETH_MAC_GMII_ADDR_CR_250_300MHZ | ETH_MAC_GMII_ADDR_GW |
		ETH_MAC_GMII_ADDR_GB;
	amba_writel(lp->regbase + ETH_MAC_GMII_ADDR_OFFSET, val);

	for (limit = AMBETH_MII_RETRY_LIMIT; limit > 0; limit--) {
		if (!amba_tstbitsl(lp->regbase + ETH_MAC_GMII_ADDR_OFFSET,
			ETH_MAC_GMII_ADDR_GB))
			break;
		udelay(AMBETH_MII_RETRY_TMO);
	}
	if ((limit <= 0) && netif_msg_hw(lp)) {
		dev_err(&lp->ndev->dev, "%s: postwrite time out!\n", __func__);
		errorCode = -EIO;
		goto ambhw_mdio_write_exit;
	}

ambhw_mdio_write_exit:
	return errorCode;
}

static int ambhw_mdio_reset(struct mii_bus *bus)
{
	int					errorCode = 0;
	struct ambeth_info			*lp;

	lp = (struct ambeth_info *)bus->priv;

	if (netif_msg_hw(lp))
		dev_info(&lp->ndev->dev, "%s: power gpio = %d, "
		"reset gpio = %d, !\n", __func__,
		lp->platform_info->mii_power.gpio_id,
		lp->platform_info->mii_reset.gpio_id);

	ambarella_set_gpio_output(&lp->platform_info->mii_power, 0);
	ambarella_set_gpio_output(&lp->platform_info->mii_reset, 1);
	ambarella_set_gpio_output(&lp->platform_info->mii_power, 1);
	ambarella_set_gpio_output(&lp->platform_info->mii_reset, 0);

	return errorCode;
}

/* ==========================================================================*/
static void ambeth_adjust_link(struct net_device *ndev)
{
	struct ambeth_info			*lp;
	unsigned long				flags;
	struct phy_device			*phydev;
	int					need_update = 0;

	lp = (struct ambeth_info *)netdev_priv(ndev);

	spin_lock_irqsave(&lp->lock, flags);

	phydev = lp->phydev;
	if (phydev->link) {
		if (phydev->duplex != lp->oldduplex) {
			need_update = 1;
			lp->oldduplex = phydev->duplex;
		}

		if (phydev->speed != lp->oldspeed) {
			switch (phydev->speed) {
			case SPEED_1000:
			case SPEED_100:
			case SPEED_10:
				need_update = 1;
				lp->oldspeed = phydev->speed;
				break;
			default:
				if (netif_msg_link(lp))
					dev_warn(&lp->ndev->dev, "%s: "
						"Speed(%d) is not 10/100!\n",
						__func__, phydev->speed);
				break;
			}
		}

		if (!lp->oldlink) {
			need_update = 1;
			lp->oldlink = 1;
		}
	} else if (lp->oldlink) {
			need_update = 1;
			lp->oldlink = PHY_DOWN;
			lp->oldspeed = 0;
			lp->oldduplex = -1;
	}

	if (need_update && netif_msg_link(lp))
		phy_print_status(phydev);

	if (need_update)
		ambhw_set_link_mode_speed(lp);

	spin_unlock_irqrestore(&lp->lock, flags);
}

static int ambeth_init_phy(struct ambeth_info *lp)
{
	int					errorCode = 0;
	struct phy_device			*phydev = NULL;
	phy_interface_t				interface;
	struct net_device			*ndev;
	int					phy_addr;

	ndev = lp->ndev;
	lp->oldlink = PHY_DOWN;
	lp->oldspeed = 0;
	lp->oldduplex = -1;

	phy_addr = lp->platform_info->mii_id;
	if ((phy_addr >= 0) && (phy_addr < PHY_MAX_ADDR)) {
		if (lp->new_bus.phy_map[phy_addr]) {
			phydev = lp->new_bus.phy_map[phy_addr];
			if (phydev->phy_id == lp->platform_info->phy_id) {
				goto ambeth_init_phy_default;
			}
		}
		dev_notice(&lp->ndev->dev,
			"Could not find default PHY in %d.\n", phy_addr);
	}
	goto ambeth_init_phy_scan;

ambeth_init_phy_default:
	dev_dbg(&lp->ndev->dev, "Find default PHY in %d!\n", phy_addr);
	if (ambarella_is_valid_gpio_irq(&lp->platform_info->phy_irq)) {
		phydev->irq = lp->platform_info->phy_irq.irq_line;
		phydev->irq_flags = lp->platform_info->phy_irq.irq_type;
	}
	goto ambeth_init_phy_connect;

ambeth_init_phy_scan:
	for (phy_addr = 0; phy_addr < PHY_MAX_ADDR; phy_addr++) {
		if (lp->new_bus.phy_map[phy_addr]) {
			phydev = lp->new_bus.phy_map[phy_addr];
			if (phydev->phy_id == lp->platform_info->phy_id)
				goto ambeth_init_phy_connect;
		}
	}
	if (!phydev) {
		dev_err(&lp->ndev->dev, "No PHY device.\n");
		errorCode = -ENODEV;
		goto ambeth_init_phy_exit;
	} else {
		dev_notice(&lp->ndev->dev, "Try PHY[%d] whose id is 0x%08x!\n",
			phydev->addr, phydev->phy_id);
	}

ambeth_init_phy_connect:
	interface = ambhw_get_interface(lp);
	phydev = phy_connect(ndev, dev_name(&phydev->dev),
		&ambeth_adjust_link, 0, interface);
	if (IS_ERR(phydev)) {
		dev_err(&lp->ndev->dev, "Could not attach to PHY!\n");
		errorCode = PTR_ERR(phydev);
		goto ambeth_init_phy_exit;
	}

	phydev->supported &= lp->platform_info->get_supported();
	phydev->advertising = phydev->supported;

	lp->phydev = phydev;

	errorCode = phy_start_aneg(phydev);

ambeth_init_phy_exit:
	return errorCode;
}

static inline int ambeth_rx_rngmng_init(struct ambeth_info *lp)
{
	int					errorCode = 0;
	struct ambeth_rx_rngmng			*rx;
	int					i;
	dma_addr_t				mapping;
	struct sk_buff				*skb;

	rx = &lp->rx;
	rx->cur_rx = 0;
	rx->dirty_rx = 0;

	memset(rx->rng, 0,
		(sizeof(struct ambeth_buffmng_info) * AMBETH_RX_RING_SIZE));
	memset(rx->desc, 0,
		(sizeof(struct ambeth_desc) * AMBETH_RX_RING_SIZE));

	for (i = 0; i < AMBETH_RX_RING_SIZE; i++) {
		skb = dev_alloc_skb(AMBETH_PACKET_MAXFRAME);
		if (skb == NULL) {
			dev_err(&lp->ndev->dev,
				"%s: dev_alloc_skb fail!\n", __func__);
			errorCode = -ENOMEM;
			goto ambeth_rx_rngmng_init_exit;
		}
		skb->dev = lp->ndev;

		rx->rng[i].skb = skb;
		mapping = dma_map_single(&lp->ndev->dev, skb->data,
			AMBETH_PACKET_MAXFRAME, DMA_FROM_DEVICE);
		rx->rng[i].mapping = mapping;
		rx->desc[i].status = ETH_RDES0_OWN;
		rx->desc[i].length = ETH_RDES1_RCH | AMBETH_PACKET_MAXFRAME;
		rx->desc[i].buffer1 = mapping;
		rx->desc[i].buffer2 = (u32)lp->rx_dma_desc +
			((i + 1) * sizeof(struct ambeth_desc));
	}

	/* Mark the last entry as wrapping the ring. */
	rx->desc[AMBETH_RX_RING_SIZE - 1].length |= ETH_RDES1_RER;
	rx->desc[AMBETH_RX_RING_SIZE - 1].buffer2 = (u32)lp->rx_dma_desc;

ambeth_rx_rngmng_init_exit:
	return errorCode;
}

static inline int ambeth_rx_refill(struct ambeth_info *lp)
{
	int					refilled = 0;
	int					entry;
	struct ambeth_rx_rngmng			*rx;
	struct sk_buff				*skb;
	dma_addr_t				mapping;

	rx = &lp->rx;

	dev_dbg(&lp->ndev->dev, "%s: cur_rx %d, dirty_rx %d.\n",
		__func__, rx->cur_rx, rx->dirty_rx);

	for (; (rx->cur_rx - rx->dirty_rx) > 0; rx->dirty_rx++) {
		entry = rx->dirty_rx % AMBETH_RX_RING_SIZE;

		if (rx->rng[entry].skb == NULL) {
			skb = dev_alloc_skb(AMBETH_PACKET_MAXFRAME);
			if (skb == NULL) {
				dev_err(&lp->ndev->dev,
					"%s: dev_alloc_skb fail!\n", __func__);
				refilled = -1;
				goto ambeth_rx_refill_exit;
			}
			skb->dev = lp->ndev;

			rx->rng[entry].skb = skb;
			mapping = dma_map_single(&lp->ndev->dev, skb->data,
				AMBETH_PACKET_MAXFRAME, DMA_FROM_DEVICE);
			rx->rng[entry].mapping = mapping;
			rx->desc[entry].buffer1 = mapping;
			refilled++;
		}
		rx->desc[entry].status = ETH_RDES0_OWN;
	}

ambeth_rx_refill_exit:
	return refilled;
}

static inline void ambeth_rx_rngmng_del(struct ambeth_info *lp)
{
	int					i;
	dma_addr_t				mapping;
	struct sk_buff				*skb;
	struct ambeth_rx_rngmng			*rx;

	rx = &lp->rx;
	for (i = 0; i < AMBETH_RX_RING_SIZE; i++) {
		skb =rx->rng[i].skb;
		mapping = rx->rng[i].mapping;
		rx->rng[i].skb = NULL;
		rx->rng[i].mapping = 0;
		if (skb != NULL) {
			dma_unmap_single(&lp->ndev->dev, mapping,
				skb->len, DMA_FROM_DEVICE);
			dev_kfree_skb(skb);
		}
		if (rx->desc) {
			rx->desc[i].status = 0;
			rx->desc[i].length = 0;
			rx->desc[i].buffer1 = 0xBADF00D0;
			rx->desc[i].buffer2 = 0xBADF00D0;
		}
	}
}

static inline int ambeth_tx_rngmng_init(struct ambeth_info *lp)
{
	int					errorCode = 0;
	struct ambeth_tx_rngmng			*tx;
	int					i;

	tx = &lp->tx;
	tx->cur_tx = 0;
	tx->dirty_tx = 0;

	memset(tx->rng, 0,
		(sizeof(struct ambeth_buffmng_info) * AMBETH_TX_RING_SIZE));
	memset(tx->desc, 0,
		(sizeof(struct ambeth_desc) * AMBETH_TX_RING_SIZE));

	for (i = 0; i < AMBETH_TX_RING_SIZE; i++) {
		tx->rng[i].mapping = 0 ;
		tx->desc[i].length = (ETH_TDES1_LS |
			ETH_TDES1_FS | ETH_TDES1_TCH);
		tx->desc[i].buffer1 = 0;
		tx->desc[i].buffer2 = (u32)lp->tx_dma_desc +
			((i + 1) * sizeof(struct ambeth_desc));
	}

	/* Mark the last entry as wrapping the ring. */
	tx->desc[AMBETH_TX_RING_SIZE - 1].length |= ETH_TDES1_TER;
	tx->desc[AMBETH_TX_RING_SIZE - 1].buffer2 = (u32)lp->tx_dma_desc;

	return errorCode;
}

static inline void ambeth_tx_rngmng_del(struct ambeth_info *lp)
{
	int					i;
	dma_addr_t				mapping;
	struct sk_buff				*skb;
	struct ambeth_tx_rngmng			*tx;
	unsigned int				dirty_tx;
	int					entry;
	int					status;

	tx = &lp->tx;
	for (dirty_tx = tx->dirty_tx; (tx->cur_tx - dirty_tx) > 0; dirty_tx++) {
		entry = dirty_tx % AMBETH_TX_RING_SIZE;
		if (tx->desc) {
			status = tx->desc[entry].status;
			if (status & ETH_TDES0_OWN)
				lp->stats.tx_errors++;
		}
	}
	for (i = 0; i < AMBETH_TX_RING_SIZE; i++) {
		skb = tx->rng[i].skb;
		mapping = tx->rng[i].mapping;
		tx->rng[i].skb = NULL;
		tx->rng[i].mapping = 0;
		if (skb != NULL) {
			dma_unmap_single(&lp->ndev->dev, mapping,
				skb->len, DMA_TO_DEVICE);
			dev_kfree_skb(skb);
		}
		if (tx->desc) {
			tx->desc[i].status = 0;
			tx->desc[i].length = 0;
			tx->desc[i].buffer1 = 0xBADF00D0;
			tx->desc[i].buffer2 = 0xBADF00D0;
		}
	}
}

static inline void ambeth_restart_rxtx(struct ambeth_info *lp)
{
	ambhw_dma_stop_rxtx(lp);
	udelay(5);

	ambhw_dma_rx_start(lp);
	ambhw_dma_tx_start(lp);
}

static irqreturn_t ambeth_interrupt(int irq, void *dev_id)
{
	struct net_device			*ndev;
	struct ambeth_info			*lp;
	u32					irq_status;
	int					tx_count = 0;
	int					work_count;
	int					rxd = 0;
	struct ambeth_tx_rngmng			*tx;
	int					entry;
	int					status;
	unsigned int				dirty_tx;
	unsigned int				dirty_to_tx;
	u32					miss_ov_reg;

	ndev = (struct net_device *)dev_id;
	lp = netdev_priv(ndev);
	irq_status = amba_readl(lp->regbase + ETH_DMA_STATUS_OFFSET);
	work_count = lp->platform_info->max_work_count;

	if (unlikely((irq_status &
		(ETH_DMA_STATUS_NIS | ETH_DMA_STATUS_AIS)) == 0))
		goto ambeth_interrupt_exit;

	do {
		if (!rxd && (irq_status & (ETH_DMA_STATUS_RI |
			ETH_DMA_STATUS_RU | ETH_DMA_STATUS_ERI))) {
			dev_dbg(&lp->ndev->dev,	"%s: RX IRQ[0x%x]!\n",
				__func__, irq_status);
			rxd++;

			amba_writel(lp->regbase + ETH_DMA_INTEN_OFFSET,
				AMBETH_DMA_INT_ENABLE & (~AMBETH_DMA_INT_RXPOLL));
			amba_writel(lp->regbase + ETH_DMA_STATUS_OFFSET,
				ETH_DMA_STATUS_RI | ETH_DMA_STATUS_ERI |
				ETH_DMA_STATUS_RU);

			napi_schedule(&lp->napi);

			if (!(irq_status & (~(ETH_DMA_STATUS_NIS |
				ETH_DMA_STATUS_AIS | AMBETH_DMA_INT_RXPOLL |
				ETH_DMA_STATUS_OVF)))) {
				dev_dbg(&lp->ndev->dev,
					"%s: No Intr except Rx 0x%x!\n",
					__func__, irq_status);
				break;
			}
		}

		if (likely(irq_status & (ETH_DMA_STATUS_TI |
			ETH_DMA_STATUS_TU | ETH_DMA_STATUS_TPS))) {
			tx = &lp->tx;

			dev_dbg(&lp->ndev->dev, "%s: gap %d, irq 0x%x.\n",
				__func__, tx->cur_tx - tx->dirty_tx,
				irq_status);

			for (dirty_tx = tx->dirty_tx;
				tx->cur_tx - dirty_tx > 0; dirty_tx++) {
				entry = dirty_tx % AMBETH_TX_RING_SIZE;
				status = tx->desc[entry].status;

				if (status & ETH_TDES0_OWN) {
					dev_dbg(&lp->ndev->dev,
						"%s: frame not transmitted yet"
						", entry %d, irq 0x%x,"
						" Tx status 0x%x\n",
						__func__, entry,
						irq_status, status);
					break;
				}

				if (unlikely(tx->rng[entry].skb == NULL)) {
					dev_err(&lp->ndev->dev,
						"%s: NULL skb in ring!\n",
						__func__);
					continue;
				}

				/* if there was an major error, log it. */
				if (unlikely(status & ETH_TDES0_ES)) {
					dev_dbg(&lp->ndev->dev,
						"%s: Transmit error, entry %d,"
						" irq 0x%x, Tx status 0x%x\n",
						__func__, entry,
						irq_status, status);
					lp->stats.tx_errors++;
					if (status & (ETH_TDES0_JT |
						ETH_TDES0_EC | ETH_TDES0_ED))
						lp->stats.tx_aborted_errors++;
					if (status & (ETH_TDES0_LCA |
						ETH_TDES0_NC))
						lp->stats.tx_carrier_errors++;
					if (status & ETH_TDES0_LCO)
						lp->stats.tx_window_errors++;
					if (status & ETH_TDES0_UF)
						lp->stats.tx_fifo_errors++;
					if ((status & ETH_TDES0_VF) &&
						lp->oldduplex == 0)
						lp->stats.tx_heartbeat_errors++;
				} else {
					lp->stats.tx_bytes +=
						tx->rng[entry].skb->len;
					lp->stats.collisions +=
						ETH_TDES0_CC(status);
					lp->stats.tx_packets++;
				}

				if (unlikely(tx->rng[entry].mapping == 0)) {
					dev_err(&lp->ndev->dev,
						"%s: null mapping!\n",
						__func__);
				} else {
					dma_unmap_single(&lp->ndev->dev,
						tx->rng[entry].mapping,
						tx->rng[entry].skb->len,
						DMA_TO_DEVICE);
				}

				dev_kfree_skb_irq(tx->rng[entry].skb);
				tx->rng[entry].skb = NULL;
				tx->rng[entry].mapping = 0;
				tx_count++;
			}

			dirty_to_tx = tx->cur_tx - dirty_tx;
			/* if dirty_tx (to be transferred by DMA and
			* then freed) cannot catch cur_tx,
			* there must be some error, so try to catch up
			*/
			if (unlikely(dirty_to_tx > AMBETH_TX_RING_SIZE)) {
				dev_err(&lp->ndev->dev,
					"%s: Out-of-sync 0x%x vs. 0x%x.\n",
					__func__, dirty_tx, tx->cur_tx);
				dirty_tx += AMBETH_TX_RING_SIZE;
			}

			/* if dirty_tx can catch cur_tx within a AMBETH_TX_RING_SIZE,
			* then wake up netif
			*/
			if (likely(dirty_to_tx < (AMBETH_TX_RING_SIZE - 2))) {
				dev_dbg(&lp->ndev->dev,
					"%s: Now gap %d, wakeup.\n",
					__func__, dirty_to_tx);
				netif_wake_queue(ndev);
			}
			tx->dirty_tx = dirty_tx;

			if (unlikely(irq_status & ETH_DMA_STATUS_TPS)) {
				dev_err(&lp->ndev->dev,
					"%s: transmission stopped and may "
					"need restart, irq status 0x%x.\n",
					__func__, irq_status);
				ambeth_restart_rxtx(lp);
			}

			dev_dbg(&lp->ndev->dev,	"%s: tx count %d.\n",
				__func__, tx_count);
		}

		if (unlikely(irq_status & ETH_DMA_STATUS_AIS)) {
			dev_dbg(&lp->ndev->dev, "%s: Abnormal: 0x%x\n",
				__func__, irq_status);

			if (irq_status & ETH_DMA_STATUS_TJT) {
				dev_err(&lp->ndev->dev,
					"%s: ETH_DMA_STATUS_TJT 0x%x\n",
					__func__, irq_status);
				lp->stats.tx_errors++;
			}
			if (irq_status & ETH_DMA_STATUS_UNF) {
				dev_err(&lp->ndev->dev,
					"%s: TxUnderFlow 0x%x\n",
					__func__, irq_status);
				ambeth_restart_rxtx(lp);
			}
			if (irq_status & ETH_DMA_STATUS_RU) {
				dev_err(&lp->ndev->dev,
					"%s: ETH_DMA_STATUS_RU 0x%x\n",
					__func__, irq_status);
			}
			if (irq_status & (ETH_DMA_STATUS_RPS |
				ETH_DMA_STATUS_OVF)) {
				dev_dbg(&lp->ndev->dev,
					"%s: ETH_DMA_STATUS_RPS or "
					"ETH_DMA_STATUS_OVF 0x%x\n",
					__func__, irq_status);
				miss_ov_reg = amba_readl(lp->regbase +
					ETH_DMA_MISS_FRAME_BOCNT_OFFSET);
				if (miss_ov_reg &
					ETH_DMA_MISS_FRAME_BOCNT_FRAME) {
					lp->stats.rx_missed_errors +=
						ETH_DMA_MISS_FRAME_BOCNT_HOST(
						miss_ov_reg);
				}
				if (miss_ov_reg &
					ETH_DMA_MISS_FRAME_BOCNT_FIFO) {
					lp->stats.rx_fifo_errors +=
						ETH_DMA_MISS_FRAME_BOCNT_APP(
						miss_ov_reg);
				}
				lp->stats.rx_errors++;

				amba_writel(lp->regbase +
					ETH_DMA_MISS_FRAME_BOCNT_OFFSET, 0);
				if (irq_status & ETH_DMA_STATUS_RPS) {
					dev_err(&lp->ndev->dev, "%s: Recieve "
						"Process Stopped 0x%x\n",
						__func__, irq_status);
					ambeth_restart_rxtx(lp);
				}
			}
			if (irq_status & ETH_DMA_STATUS_FBI) {
				dev_err(&lp->ndev->dev,
					"%s: ETH_DMA_STATUS_FBI 0x%x\n",
					__func__, irq_status);
			}

			amba_writel(lp->regbase + ETH_DMA_STATUS_OFFSET,
				(ETH_DMA_STATUS_AIS | ETH_DMA_STATUS_TPS |
				ETH_DMA_STATUS_TJT | ETH_DMA_STATUS_OVF |
				ETH_DMA_STATUS_UNF | ETH_DMA_STATUS_RU |
				ETH_DMA_STATUS_RPS | ETH_DMA_STATUS_RWT |
				ETH_DMA_STATUS_ETI | ETH_DMA_STATUS_FBI));
		}

		work_count--;
		if (work_count == 0)
			break;

		irq_status = amba_readl(lp->regbase + ETH_DMA_STATUS_OFFSET);
		if (rxd) {
			irq_status &= ~AMBETH_DMA_INT_RXPOLL;
		}
	} while ((irq_status & (ETH_DMA_STATUS_TU | ETH_DMA_STATUS_TPS |
		ETH_DMA_STATUS_TI | ETH_DMA_STATUS_RPS | ETH_DMA_STATUS_UNF |
		ETH_DMA_STATUS_TJT | ETH_DMA_STATUS_FBI)) != 0);

	amba_writel(lp->regbase + ETH_DMA_STATUS_OFFSET, irq_status);

ambeth_interrupt_exit:
	return IRQ_HANDLED;
}

static void ambeth_oom_timer(unsigned long data)
{
	struct net_device			*ndev;
	struct ambeth_info			*lp;

	ndev = (struct net_device *)data;
	lp = (struct ambeth_info *)netdev_priv(ndev);

	dev_dbg(&lp->ndev->dev, "%s: napi_schedule.\n", __func__);
	napi_schedule(&lp->napi);
}

static int ambeth_start_hw(struct net_device *ndev)
{
	int					errorCode = 0;
	struct ambeth_info			*lp;

	lp = (struct ambeth_info *)netdev_priv(ndev);

	ambarella_set_gpio_output(&lp->platform_info->mii_power, 1);
	ambarella_set_gpio_reset(&lp->platform_info->mii_reset);

	if (lp->platform_info->is_supportclk())
		lp->platform_info->setclk(lp->platform_info->default_clk);

	if (lp->phydev == NULL) {
		errorCode = ambeth_init_phy(lp);
		if (errorCode)
			goto ambeth_start_hw_exit;
	}

	lp->rx.desc = dma_alloc_coherent(&lp->ndev->dev,
		(sizeof(struct ambeth_desc) * AMBETH_RX_RING_SIZE),
		&lp->rx_dma_desc, GFP_KERNEL);
	if (lp->rx.desc == NULL) {
		dev_err(&lp->ndev->dev,
			"%s: dma_alloc_coherent rx.desc fail.\n", __func__);
		errorCode = -ENOMEM;
		goto ambeth_start_hw_exit;
	}
	errorCode = ambeth_rx_rngmng_init(lp);
	if (errorCode)
		goto ambeth_start_hw_exit;

	lp->tx.desc = dma_alloc_coherent(&lp->ndev->dev,
		(sizeof(struct ambeth_desc) * AMBETH_TX_RING_SIZE),
		&lp->tx_dma_desc, GFP_KERNEL);
	if (lp->tx.desc == NULL) {
		dev_err(&lp->ndev->dev,
			"%s: dma_alloc_coherent tx.desc fail.\n", __func__);
		errorCode = -ENOMEM;
		goto ambeth_start_hw_exit;
	}
	errorCode = ambeth_tx_rngmng_init(lp);
	if (errorCode)
		goto ambeth_start_hw_exit;

	ambhw_set_dma_desc(lp);
	ambhw_init(lp);
	ambhw_set_hwaddr(lp, ndev->dev_addr);
	ambhw_dma_rx_start(lp);
	ambhw_dma_tx_start(lp);

ambeth_start_hw_exit:
	return errorCode;
}

static int ambeth_stop_hw(struct net_device *ndev)
{
	int					errorCode = 0;
	struct ambeth_info			*lp;

	lp = (struct ambeth_info *)netdev_priv(ndev);

	if (lp->phydev) {
		phy_disconnect(lp->phydev);
		lp->phydev = NULL;
	}

	ambhw_dma_stop_rxtx(lp);

	if (lp->tx.desc) {
		ambeth_tx_rngmng_del(lp);
		dma_free_coherent(&lp->ndev->dev,
			(sizeof(struct ambeth_desc) * AMBETH_TX_RING_SIZE),
			lp->tx.desc, lp->tx_dma_desc);
	}
	if (lp->rx.desc) {
		ambeth_rx_rngmng_del(lp);
		dma_free_coherent(&lp->ndev->dev,
			(sizeof(struct ambeth_desc) * AMBETH_RX_RING_SIZE),
			lp->rx.desc, lp->rx_dma_desc);
	}

	ambarella_set_gpio_output(&lp->platform_info->mii_power, 0);

	return errorCode;
}

static int ambeth_open(struct net_device *ndev)
{
	int					errorCode = 0;
	struct ambeth_info			*lp;

	lp = (struct ambeth_info *)netdev_priv(ndev);

	errorCode = ambeth_start_hw(ndev);
	if (errorCode)
		goto ambeth_open_exit;

	errorCode = request_irq(ndev->irq, ambeth_interrupt,
		IRQF_SHARED | IRQF_TRIGGER_HIGH,
		ndev->name, ndev);
	if (errorCode) {
		dev_err(&lp->ndev->dev, "%s: request_irq[%d] fail.\n",
			__func__, ndev->irq);
		goto ambeth_open_exit;
	}

	init_timer(&lp->oom_timer);
	lp->oom_timer.data = (unsigned long)ndev;
	lp->oom_timer.function = ambeth_oom_timer;
	napi_enable(&lp->napi);
	netif_start_queue(ndev);
	ambhw_dma_int_enable(lp);

ambeth_open_exit:
	if (errorCode)
		ambeth_stop_hw(ndev);
	return errorCode;
}

static int ambeth_stop(struct net_device *ndev)
{
	struct ambeth_info			*lp;

	lp = (struct ambeth_info *)netdev_priv(ndev);

	ambhw_dma_int_disable(lp);
	free_irq(ndev->irq, ndev);

	netif_stop_queue(ndev);
	napi_disable(&lp->napi);
	flush_scheduled_work();
	del_timer_sync(&lp->oom_timer);

	return ambeth_stop_hw(ndev);
}

static int ambeth_hard_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ambeth_info			*lp;
	dma_addr_t				mapping;
	struct ambeth_tx_rngmng			*tx;
	int					entry;
	int					tx_flag;
	unsigned long				flags;

	lp = (struct ambeth_info *)netdev_priv(ndev);
	tx = &lp->tx;

	spin_lock_irqsave(&lp->lock, flags);

	entry = tx->cur_tx % AMBETH_TX_RING_SIZE;

	dev_dbg(&lp->ndev->dev,
		"%s: cur_tx[%d], dirty_tx[%d], entry[%d], skb.len[%d].\n",
		__func__, tx->cur_tx, tx->dirty_tx, entry, skb->len);

	tx->rng[entry].skb = skb;
	mapping = dma_map_single(&lp->ndev->dev, skb->data,
		skb->len, DMA_TO_DEVICE);
	tx->rng[entry].mapping = mapping;
	tx->desc[entry].buffer1 = mapping;

	if ((tx->cur_tx - tx->dirty_tx) < (AMBETH_TX_RING_SIZE / 2)) {
		tx_flag = ETH_TDES1_LS | ETH_TDES1_FS;
	} else if ((tx->cur_tx - tx->dirty_tx) == (AMBETH_TX_RING_SIZE / 2)) {
		tx_flag = ETH_TDES1_IC | ETH_TDES1_LS | ETH_TDES1_FS;
	} else if ((tx->cur_tx - tx->dirty_tx) < (AMBETH_TX_RING_SIZE - 2)) {
		tx_flag = ETH_TDES1_LS | ETH_TDES1_FS;
	} else {
		tx_flag = ETH_TDES1_IC | ETH_TDES1_LS | ETH_TDES1_FS;
		netif_stop_queue(ndev);
	}

	if (entry == (AMBETH_TX_RING_SIZE - 1))
		tx_flag = ETH_TDES1_IC | ETH_TDES1_LS |
			ETH_TDES1_FS | ETH_TDES1_TER;

	tx->desc[entry].length = ETH_TDES1_TBS1x(skb->len) | tx_flag;
	tx->desc[entry].status = ETH_TDES0_OWN;
	tx->cur_tx++;

	amba_writel(lp->regbase + ETH_DMA_TX_POLL_DMD_OFFSET, 0x01);

	spin_unlock_irqrestore(&lp->lock, flags);

	ndev->trans_start = jiffies;

	return 0;
}

static void ambeth_timeout(struct net_device *ndev)
{
	struct ambeth_info			*lp;

	lp = (struct ambeth_info *)netdev_priv(ndev);

	if (netif_msg_timer(lp))
		dev_info(&lp->ndev->dev, "%s: ...\n", __func__);

	netif_wake_queue(ndev);
}

static struct net_device_stats *ambeth_get_stats(struct net_device *ndev)
{
	struct ambeth_info *lp = netdev_priv(ndev);

	return &lp->stats;
}

static inline void ambeth_poll_error_status(struct ambeth_info *lp,
	unsigned int status)
{
	if ((status & 0x38000300) != 0x0300) {
		if (!(status & 0xffff)) {
			dev_dbg(&lp->ndev->dev, "%s: 802.3 frame size %d "
			"spanned multi buffer, status %8.8x.\n",
			__func__, ETH_RDES0_FL(status),	status);
		} else if ((status & 0xffff) !=	0x7fff) {
			dev_dbg(&lp->ndev->dev, "%s: Ethernet frame size %d"
			" spanned multi buffer, status %8.8x.\n",
			__func__, ETH_RDES0_FL(status), status);
			//lp->stats.rx_length_errors++;
		}
	} else if (status & ETH_RDES0_ES) {
		dev_dbg(&lp->ndev->dev,
			"%s: Receive error, Rx status 0x%8.8x.\n",
			__func__, status);
		lp->stats.rx_errors++;

		if (status & ETH_RDES0_DE) {
			dev_err(&lp->ndev->dev, "%s: rx descriptor error.\n",
				__func__);
		}
		if (status & ETH_RDES0_SAF) {
			dev_err(&lp->ndev->dev, "%s: rx SA filter fail.\n",
				__func__);
		}
		if (status & ETH_RDES0_LC) {
			dev_err(&lp->ndev->dev, "%s: rx collison error.\n",
				__func__);
			lp->stats.collisions++;
		}
		if(status & ETH_RDES0_CE ) {
			lp->stats.rx_crc_errors++;
			dev_err(&lp->ndev->dev, "%s: rx crc error.\n",
				__func__);
		}
		if(status & ETH_RDES0_DBE ) {
			lp->stats.rx_frame_errors++;
			dev_err(&lp->ndev->dev, "%s: rx dribbling error.\n",
				__func__);
		}
		if(status & ETH_RDES0_LE) {
			lp->stats.rx_length_errors++;
			dev_err(&lp->ndev->dev, "%s: rx length error.\n",
				__func__);
		}
		if (status & ETH_RDES0_OE) {
			dev_dbg(&lp->ndev->dev,
				"%s: rx damage frame because of overflow.\n",
				__func__);
		}
		if (status & ETH_RDES0_IPC) {
			dev_err(&lp->ndev->dev,
				"%s: rx Long frame error.\n",
				__func__);
		}
		if (status & ETH_RDES0_RWT) {
			dev_err(&lp->ndev->dev,
				"%s: rx watchdog timeout.\n",
				__func__);
		}
		if (status & ETH_RDES0_RE) {
			dev_err(&lp->ndev->dev,
				"%s: rx receive error by mii.\n",
				__func__);
		}
	}
}

static inline void ambeth_poll_status(struct ambeth_info *lp,
	unsigned int status, int entry)
{
	short					pkt_len;
	struct sk_buff				*skb;
	char					*temp;
	struct net_device			*ndev;

	ndev = lp->ndev;
	pkt_len = ETH_RDES0_FL(status) - 4;

	if (unlikely(pkt_len > AMBETH_RX_COPYBREAK)) {
		dev_warn(&lp->ndev->dev,
			"%s: Bogus packet size of %d.\n",
			__func__, pkt_len);
		pkt_len = AMBETH_RX_COPYBREAK;
		lp->stats.rx_length_errors++;
	}

	temp = skb_put(skb = lp->rx.rng[entry].skb, pkt_len);
	if (unlikely(lp->rx.rng[entry].mapping != lp->rx.desc[entry].buffer1)) {
		dev_warn(&lp->ndev->dev,
			"%s: The skbuff addresses do not match "
			"0x%08x vs 0x%08x %p %p.\n",
			__func__, lp->rx.desc[entry].buffer1,
			(u32)lp->rx.rng[entry].mapping,
			skb->head, temp);
	}
	dma_unmap_single(&lp->ndev->dev, lp->rx.rng[entry].mapping,
		lp->rx.rng[entry].skb->len, DMA_FROM_DEVICE);
	lp->rx.rng[entry].skb = NULL;
	lp->rx.rng[entry].mapping = 0;

	skb->dev = ndev;
	skb->protocol = eth_type_trans(skb, ndev);

	netif_receive_skb(skb);

	ndev->last_rx = jiffies;
	lp->stats.rx_packets++;
	lp->stats.rx_bytes += pkt_len;
}

int ambeth_poll(struct napi_struct *napi, int budget)
{
	struct ambeth_info			*lp;
	struct net_device			*ndev;
	int					entry;
	int					rx_work_limit;
	int					received = 0;
	int					rx_intr = 0;
	int					loop_count = 0;
	unsigned int				status;

	lp = container_of(napi, struct ambeth_info, napi);
	ndev = lp->ndev;
	entry = lp->rx.cur_rx % AMBETH_RX_RING_SIZE;
	rx_work_limit = budget;

	if (unlikely(!netif_carrier_ok(ndev)))
		goto ambeth_poll_complete;

	do {
		while (1) {
			status = lp->rx.desc[entry].status;
			if (status & ETH_RDES0_OWN)
				break;

			if (unlikely((status & 0x38008300) != 0x0300)) {
				ambeth_poll_error_status(lp, status);
			} else {
				ambeth_poll_status(lp, status, entry);
			}

			if (unlikely((lp->rx.cur_rx - lp->rx.dirty_rx) >
				(AMBETH_RX_RING_SIZE / 4))) {
				dev_vdbg(&lp->ndev->dev,
					"%s: 1/4 RX RING USED, refill.\n",
					__func__);
				if (ambeth_rx_refill(lp) < 0)
					goto ambeth_poll_out_of_memory;
			}

			received++;
			rx_work_limit--;
			lp->rx.cur_rx++;
			entry = lp->rx.cur_rx % AMBETH_RX_RING_SIZE;
			if (rx_work_limit <= 0)
				goto ambeth_poll_exit;
		}

		rx_intr = amba_test_and_set_mask(
			lp->regbase + ETH_DMA_STATUS_OFFSET,
			ETH_DMA_STATUS_RI);

		loop_count++;
		if (loop_count > 1)
			dev_vdbg(&lp->ndev->dev, "%s: rx_intr = %d.\n",
				__func__, loop_count);
	} while (rx_intr);

ambeth_poll_complete:
	if (ambeth_rx_refill(lp) < 0)
		goto ambeth_poll_out_of_memory;

	napi_complete(&lp->napi);
	ambhw_dma_int_enable(lp);
	goto ambeth_poll_exit;

ambeth_poll_out_of_memory:
	dev_info(&lp->ndev->dev, "%s: ambeth_poll_out_of_memory.\n", __func__);
	mod_timer(&lp->oom_timer, jiffies + 1);
	napi_complete(&lp->napi);

ambeth_poll_exit:
	return received;
}

static void ambeth_set_multicast_list(struct net_device *ndev)
{
	struct ambeth_info			*lp;
	unsigned int				mac_filter_reg;

	lp = (struct ambeth_info *)netdev_priv(ndev);

	mac_filter_reg = amba_readl(lp->regbase + ETH_MAC_FRAME_FILTER_OFFSET);
	dev_dbg(&lp->ndev->dev, "%s: mac_filter 0x%x.\n",
		__func__, mac_filter_reg);
	mac_filter_reg &= ~(ETH_MAC_FRAME_FILTER_PM | ETH_MAC_FRAME_FILTER_PR |
		ETH_MAC_FRAME_FILTER_HMC | ETH_MAC_FRAME_FILTER_RA);
	dev_dbg(&lp->ndev->dev, "%s: flags 0x%x.\n",
		__func__, ndev->flags);
	dev_dbg(&lp->ndev->dev, "%s: mc_count 0x%x.\n",
		__func__, netdev_mc_count(ndev));

	if (ndev->flags & IFF_PROMISC) {
		dev_dbg(&lp->ndev->dev, "%s: Promiscuous mode.\n", __func__);
		amba_writel(lp->regbase + ETH_MAC_FRAME_FILTER_OFFSET,
			mac_filter_reg | ETH_MAC_FRAME_FILTER_PR);
	} else if ((netdev_mc_count(ndev) > AMBETH_MULTICAST_LIMIT) ||
		(ndev->flags & IFF_ALLMULTI)) {
		if (netdev_mc_count(ndev) > AMBETH_MULTICAST_LIMIT) {
			dev_dbg(&lp->ndev->dev,
				"%s: too many in multicast, accept all.\n",
				__func__);
		}

		if (ndev->flags & IFF_ALLMULTI) {
			dev_dbg(&lp->ndev->dev,
				"%s: IFF_ALLLMULTI flag set, accept all.\n",
				__func__);
		}
		dev_dbg(&lp->ndev->dev,
			"%s: Accept All Multi %d.\n",
			__func__, netdev_mc_count(ndev));
		amba_writel(lp->regbase + ETH_MAC_FRAME_FILTER_OFFSET,
			mac_filter_reg | ETH_MAC_FRAME_FILTER_PM);
	} else if ((netdev_mc_count(ndev) <= AMBETH_MULTICAST_PF)) {
		int				i;
		int				filtered;
		int				count = netdev_mc_count(ndev);
		unsigned char			zeromacbuf[6];
		struct netdev_hw_addr		*ha;

		dev_dbg(&lp->ndev->dev,
			"%s: use perfect filtering %d.\n",
			__func__, netdev_mc_count(ndev));
		mac_filter_reg &= ~(ETH_MAC_FRAME_FILTER_HMC |
			ETH_MAC_FRAME_FILTER_PM);

		i = 0;
		netdev_for_each_mc_addr(ha, ndev) {
			if (!(ha->addr[0] & 1)) {
				dev_err(&lp->ndev->dev,
					"%s: Not a multicast addr 0x%x.\n",
					__func__, ha->addr[0]);
				continue;
			}
			ambhw_set_hwaddr_perfect_filtering(
				lp, ha->addr, (count - 1 - i) + 1, 1);
			dev_dbg(&lp->ndev->dev,
				"%s: Added perfect filtering[%pM].\n",
				__func__, ha->addr);
			i++;
		}

		filtered = i;
		dev_dbg(&lp->ndev->dev,
			"%s: Disable the rest %d MAC for perfect filtering.\n",
			__func__, AMBETH_MULTICAST_PF - filtered);
		memset(zeromacbuf, 0, sizeof(zeromacbuf));
		for (i = 0; i < AMBETH_MULTICAST_PF - filtered; i++) {
			ambhw_set_hwaddr_perfect_filtering(lp, zeromacbuf,
				filtered + i + 1, 0);
		}
		amba_writel(lp->regbase + ETH_MAC_FRAME_FILTER_OFFSET,
			mac_filter_reg);
	} else {
#ifdef ENABLE_HASH_MULTI
		struct netdev_hw_addr		*ha;
		u32				mc_filter[2] = {0, 0};
		int				filterbit;

		dev_dbg(&lp->ndev->dev, "%s: handling HASH MULTI.\n", __func__);

		netdev_for_each_mc_addr(ha, ndev) {
			if (!(ha->addr[0] & 1)) {
				dev_err(&lp->ndev->dev,
					"%s: Not a multicast addr 0x%x.\n",
					__func__, ha->addr[0]);
				continue;
			}

			filterbit = ether_crc(ETH_ALEN, ha->addr);
			filterbit >>= 26;
			filterbit &= 0x3F;

			mc_filter[filterbit >> 5] |= 1 << (filterbit & 31);

			dev_dbg(&lp->ndev->dev,
				"%s: Added filter for[%pM], hash:0x%8x.\n",
				__func__, ha->addr, filterbit);
		}

		if ((mc_filter[0] == lp->mc_filter[0]) &&
			(mc_filter[1] == lp->mc_filter[1])) {
			/* No change. */
		} else {
			dev_dbg(&lp->ndev->dev,
				"%s: Update HASH, HI[0x%x], LO[0x%x].\n",
				__func__, mc_filter[1], mc_filter[0]);

			amba_writel(lp->regbase + ETH_MAC_FRAME_FILTER_OFFSET,
				mac_filter_reg | ETH_MAC_FRAME_FILTER_HMC);
			amba_writel(lp->regbase + ETH_MAC_HASH_LO_OFFSET,
				mc_filter[0]);
			amba_writel(lp->regbase + ETH_MAC_HASH_HI_OFFSET,
				mc_filter[1]);
		}
		lp->mc_filter[0] = mc_filter[0];
		lp->mc_filter[1] = mc_filter[1];
#else
		dev_dbg(&lp->ndev->dev, "%s: Accept All Multi %d.\n",
			__func__, netdev_mc_count(ndev));
		amba_writel(lp->regbase + ETH_MAC_FRAME_FILTER_OFFSET,
			mac_filter_reg | ETH_MAC_FRAME_FILTER_PM);
#endif
	}
}

static int ambeth_set_mac_address(struct net_device *ndev, void *addr)
{
	struct sockaddr				*saddr;
	struct ambeth_info			*lp;

	saddr = (struct sockaddr *)(addr);
	lp = (struct ambeth_info *)netdev_priv(ndev);

	if (netif_running(ndev))
		return -EBUSY;

	if (!is_valid_ether_addr(saddr->sa_data))
		return -EADDRNOTAVAIL;

	dev_dbg(&lp->ndev->dev, "MAC address[%pM].\n", saddr->sa_data);

	memcpy(ndev->dev_addr, saddr->sa_data, ndev->addr_len);
	ambhw_set_hwaddr(lp, ndev->dev_addr);
	ambhw_get_hwaddr(lp, ndev->dev_addr);

	return 0;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void ambeth_poll_controller(struct net_device *ndev)
{
	unsigned long				flags;

	local_irq_save(flags);
	ambeth_interrupt(ndev->irq, ndev);
	local_irq_restore(flags);
}
#endif

static int ambeth_get_settings(struct net_device *ndev, struct ethtool_cmd *ecmd)
{
	struct ambeth_info *lp;

	lp = netdev_priv(ndev);

	return phy_ethtool_gset(lp->phydev, ecmd);
}

static int ambeth_set_settings(struct net_device *ndev, struct ethtool_cmd *ecmd)
{
	struct ambeth_info *lp;

	lp = netdev_priv(ndev);

	return phy_ethtool_sset(lp->phydev, ecmd);
}

/****************************************************************************/
static const struct net_device_ops ambeth_netdev_ops = {
	.ndo_open		= ambeth_open,
	.ndo_stop		= ambeth_stop,
	.ndo_start_xmit		= ambeth_hard_start_xmit,
	.ndo_set_multicast_list	= ambeth_set_multicast_list,
	.ndo_set_mac_address 	= ambeth_set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_tx_timeout		= ambeth_timeout,
	.ndo_get_stats		= ambeth_get_stats,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= ambeth_poll_controller,
#endif
};

static const struct ethtool_ops ambeth_ethtool_ops = {
	.get_settings	= ambeth_get_settings,
	.set_settings	= ambeth_set_settings,
	.get_link	= ethtool_op_get_link,
};

static int __devinit ambeth_drv_probe(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct net_device			*ndev;
	struct ambeth_info			*lp;
	struct ambarella_eth_platform_info	*platform_info;
	struct resource				*reg_res;
	struct resource				*irq_res;
	int					i;

	platform_info =
		(struct ambarella_eth_platform_info *)pdev->dev.platform_data;
	if (platform_info == NULL) {
		dev_err(&pdev->dev, "%s: Can't get platform_data!\n", __func__);
		errorCode = - EPERM;
		goto ambeth_drv_probe_exit;
	}
	if (platform_info->is_enabled) {
		if (!platform_info->is_enabled()) {
			dev_err(&pdev->dev,
				"%s: Not enabled, check HW config!\n",
				__func__);
			errorCode = -EPERM;
			goto ambeth_drv_probe_exit;
		}
	}

	if (platform_info->is_supportclk())
		platform_info->setclk(platform_info->default_clk);

	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (reg_res == NULL) {
		dev_err(&pdev->dev, "%s: Get reg_res failed!\n", __func__);
		errorCode = -ENXIO;
		goto ambeth_drv_probe_exit;
	}

	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq_res == NULL) {
		dev_err(&pdev->dev, "%s: Get irq_res failed!\n", __func__);
		errorCode = -ENXIO;
		goto ambeth_drv_probe_exit;
	}

	ndev = alloc_etherdev(sizeof(struct ambeth_info));
	if (ndev == NULL) {
		dev_err(&pdev->dev, "%s: alloc_etherdev fail.\n", __func__);
		errorCode = -ENOMEM;
		goto ambeth_drv_probe_exit;
	}
	SET_NETDEV_DEV(ndev, &pdev->dev);
	ndev->dev.dma_mask = pdev->dev.dma_mask;
	ndev->dev.coherent_dma_mask = pdev->dev.coherent_dma_mask;
	ndev->irq = irq_res->start;

	lp = netdev_priv(ndev);
	spin_lock_init(&lp->lock);
	lp->ndev = ndev;
	lp->regbase = (unsigned char __iomem *)reg_res->start;
	lp->platform_info = platform_info;
	lp->msg_enable = netif_msg_init(msg_level, NETIF_MSG_DRV |
		NETIF_MSG_PROBE | NETIF_MSG_RX_ERR | NETIF_MSG_TX_ERR);

	if (lp->platform_info->mii_power.gpio_id != -1) {
		errorCode = gpio_request(lp->platform_info->mii_power.gpio_id,
			pdev->name);
		if (errorCode) {
			dev_err(&pdev->dev, "gpio_request %d fail %d.\n",
				lp->platform_info->mii_power.gpio_id,
				errorCode);
			goto ambeth_drv_probe_free_netdev;
		}
	}

	if (lp->platform_info->mii_reset.gpio_id != -1) {
		errorCode = gpio_request(lp->platform_info->mii_reset.gpio_id,
			pdev->name);
		if (errorCode) {
			dev_err(&pdev->dev, "gpio_request %d fail %d.\n",
				lp->platform_info->mii_reset.gpio_id,
				errorCode);
			goto ambeth_drv_probe_free_mii_power;
		}
	}

	if (ambarella_is_valid_gpio_irq(&lp->platform_info->phy_irq)) {
		errorCode = gpio_request(lp->platform_info->phy_irq.irq_gpio,
			pdev->name);
		if (errorCode) {
			dev_err(&pdev->dev,
				"gpio_request %d for IRQ %d fail %d.\n",
				lp->platform_info->phy_irq.irq_gpio,
				lp->platform_info->phy_irq.irq_line,
				errorCode);
			goto ambeth_drv_probe_free_mii_reset;
		}
	}

	lp->new_bus.name = "Ambarella MII Bus",
	lp->new_bus.read = &ambhw_mdio_read,
	lp->new_bus.write = &ambhw_mdio_write,
	lp->new_bus.reset = &ambhw_mdio_reset,
	snprintf(lp->new_bus.id, MII_BUS_ID_SIZE, "%x", pdev->id);
	lp->new_bus.priv = lp;
	lp->new_bus.irq = kmalloc(sizeof(int)*PHY_MAX_ADDR, GFP_KERNEL);
	if (lp->new_bus.irq == NULL) {
		dev_err(&pdev->dev, "%s: alloc new_bus.irq fail.\n", __func__);
		errorCode = -ENOMEM;
		goto ambeth_drv_probe_free_mii_gpio_irq;
	}
	for(i = 0; i < PHY_MAX_ADDR; ++i)
		lp->new_bus.irq[i] = PHY_POLL;
	lp->new_bus.parent = &pdev->dev;
	lp->new_bus.state = MDIOBUS_ALLOCATED;
	lp->mc_filter[0] = 0;
	lp->mc_filter[1] = 0;

	errorCode = mdiobus_register(&lp->new_bus);
	if (errorCode) {
		dev_err(&pdev->dev,
			"%s: mdiobus_register fail%d.\n",
			__func__, errorCode);
		goto ambeth_drv_probe_kfree_mdiobus;
	}

	errorCode = ambhw_dma_reset(lp);
	if (errorCode) {
		dev_err(&pdev->dev,
			"%s: ambhw_dma_reset fail%d.\n",
			__func__, errorCode);
		goto ambeth_drv_probe_unregister_mdiobus;
	}

	ether_setup(ndev);
	ndev->netdev_ops = &ambeth_netdev_ops;
	ndev->watchdog_timeo = AMBETH_TX_TIMEOUT;
	netif_napi_add(ndev, &lp->napi, ambeth_poll,
		lp->platform_info->napi_weight);
	if (!is_valid_ether_addr(lp->platform_info->mac_addr))
		random_ether_addr(lp->platform_info->mac_addr);
	memcpy(ndev->dev_addr, lp->platform_info->mac_addr, AMBETH_MAC_SIZE);
	ambhw_set_hwaddr(lp, ndev->dev_addr);
	ambhw_get_hwaddr(lp, ndev->dev_addr);
	dev_info(&pdev->dev, "MAC Address[%pM].\n", ndev->dev_addr);

	SET_ETHTOOL_OPS(ndev, &ambeth_ethtool_ops);

	errorCode = register_netdev(ndev);
	if (errorCode) {
		dev_err(&pdev->dev,
			"%s: register_netdev fail%d.\n",
			__func__, errorCode);
		goto ambeth_drv_probe_netif_napi_del;
	}

	platform_set_drvdata(pdev, ndev);

	goto ambeth_drv_probe_exit;

ambeth_drv_probe_netif_napi_del:
	netif_napi_del(&lp->napi);

ambeth_drv_probe_unregister_mdiobus:
	mdiobus_unregister(&lp->new_bus);

ambeth_drv_probe_kfree_mdiobus:
	kfree(lp->new_bus.irq);

ambeth_drv_probe_free_mii_gpio_irq:
	if (ambarella_is_valid_gpio_irq(&lp->platform_info->phy_irq))
		gpio_free(lp->platform_info->phy_irq.irq_gpio);

ambeth_drv_probe_free_mii_reset:
	if (lp->platform_info->mii_reset.gpio_id != -1)
		gpio_free(lp->platform_info->mii_reset.gpio_id);

ambeth_drv_probe_free_mii_power:
	if (lp->platform_info->mii_power.gpio_id != -1)
		gpio_free(lp->platform_info->mii_power.gpio_id);

ambeth_drv_probe_free_netdev:
	free_netdev(ndev);

ambeth_drv_probe_exit:
	return errorCode;
}

static int __devexit ambeth_drv_remove(struct platform_device *pdev)
{
	struct net_device			*ndev;
	struct ambeth_info			*lp;

	ndev = platform_get_drvdata(pdev);

	if (ndev) {
		lp = (struct ambeth_info *)netdev_priv(ndev);

		unregister_netdev(ndev);
		netif_napi_del(&lp->napi);
		if (lp->platform_info->mii_power.gpio_id != -1)
			gpio_free(lp->platform_info->mii_power.gpio_id);
		if (lp->platform_info->mii_reset.gpio_id != -1)
			gpio_free(lp->platform_info->mii_reset.gpio_id);
		if (ambarella_is_valid_gpio_irq(&lp->platform_info->phy_irq))
			gpio_free(lp->platform_info->phy_irq.irq_gpio);
		mdiobus_unregister(&lp->new_bus);
		kfree(lp->new_bus.irq);
		free_netdev(ndev);

		platform_set_drvdata(pdev, NULL);
	}

	dev_info(&pdev->dev, "%s: exit.\n", __func__);

	return 0;
}

#ifdef CONFIG_PM
static int ambeth_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	int					errorCode = 0;
	struct net_device			*ndev;
	struct ambeth_info			*lp;

	ndev = platform_get_drvdata(pdev);
	if (ndev) {
		if (!netif_running(ndev))
			goto ambeth_drv_suspend_exit;

		lp = (struct ambeth_info *)netdev_priv(ndev);

		netif_stop_queue(ndev);
		disable_irq(ndev->irq);
		napi_disable(&lp->napi);
		netif_device_detach(ndev);
		ambhw_dma_int_disable(lp);
		ambeth_stop_hw(ndev);
	}

ambeth_drv_suspend_exit:
	dev_dbg(&pdev->dev, "%s exit with %d @ %d\n",
		__func__, errorCode, state.event);
	return errorCode;
}

static int ambeth_drv_resume(struct platform_device *pdev)
{
	int					errorCode = 0;
	struct net_device			*ndev;
	struct ambeth_info			*lp;

	ndev = platform_get_drvdata(pdev);
	if (ndev) {
		if (!netif_running(ndev))
			goto ambeth_drv_resume_exit;

		lp = (struct ambeth_info *)netdev_priv(ndev);
		ambhw_dma_reset(lp);
		ambeth_start_hw(ndev);
		ambhw_dma_int_enable(lp);
		netif_device_attach(ndev);
		napi_enable(&lp->napi);
		enable_irq(ndev->irq);
		netif_start_queue(ndev);
	}

ambeth_drv_resume_exit:
	dev_dbg(&pdev->dev, "%s exit with %d\n", __func__, errorCode);
	return errorCode;
}
#endif

static struct platform_driver ambeth_driver = {
	.probe		= ambeth_drv_probe,
	.remove		= __devexit_p(ambeth_drv_remove),
#ifdef CONFIG_PM
	.suspend        = ambeth_drv_suspend,
	.resume		= ambeth_drv_resume,
#endif
	.driver = {
		.name	= "ambarella-eth",
		.owner	= THIS_MODULE,
	},
};

static int __init ambeth_init(void)
{
	return platform_driver_register(&ambeth_driver);
}

static void __exit ambeth_exit(void)
{
	platform_driver_unregister(&ambeth_driver);
}

module_init(ambeth_init);
module_exit(ambeth_exit);

MODULE_AUTHOR("Grady Chen & Shanghai Team");
MODULE_DESCRIPTION("Ambarella Media Processor Ethernet Driver");
MODULE_LICENSE("GPL");

